
#include <sys/random.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "core/net.h"
#include "core/ethernet.h"
#include "core/ip.h"
#include "core/tcp.h"
#include "http/http_server.h"
#include "http/http_server_misc.h"

#include "debug.h"

#include "cloud_request.h"

#define APP_HTTP_MAX_CONNECTIONS 32
HttpConnection httpConnections[APP_HTTP_MAX_CONNECTIONS];

char_t *ipAddrToString(const IpAddr *ipAddr, char_t *str)
{
    return "(not implemented)";
}

error_t resGetData(const char_t *path, const uint8_t **data, size_t *length)
{
    TRACE_INFO("resGetData: %s (static response)\n", path);

    *data = (uint8_t *)"CONTENT\r\n";
    *length = (size_t)osStrlen((char *)*data);

    return NO_ERROR;
}

typedef struct
{
    uint32_t status;
    HttpConnection *connection;
} cbr_ctx_t;

void httpServerResponseCbr(void *ctx_in)
{
    cbr_ctx_t *ctx = (cbr_ctx_t *)ctx_in;
    char line[128];

    osSprintf(line, "HTTP/%u.%u %u ", MSB(ctx->connection->response.version), LSB(ctx->connection->response.version), ctx->connection->response.statusCode);

    osStrcat(line, "Some response");

    httpSend(ctx->connection, line, osStrlen(line), HTTP_FLAG_DELAY);

    ctx->status = 1;
}

void httpServerHeaderCbr(void *ctx_in, const char *header, const char *value)
{
    cbr_ctx_t *ctx = (cbr_ctx_t *)ctx_in;
    char line[128];

    if (header)
    {
        TRACE_INFO(">> httpServerHeaderCbr: %s = %s\r\n", header, value);
        osSprintf(line, "%s: %s\r\n", header, value);
    }
    else
    {
        TRACE_INFO(">> httpServerHeaderCbr: NULL\r\n");
        osStrcpy(line, "\r\n");
    }

    httpSend(ctx->connection, line, osStrlen(line), HTTP_FLAG_DELAY);

    ctx->status = 1;
}

void httpServerBodyCbr(void *ctx_in, const char *payload, size_t length)
{
    cbr_ctx_t *ctx = (cbr_ctx_t *)ctx_in;

    TRACE_INFO(">> httpServerBodyCbr: %lu received\r\n", length);
    httpSend(ctx->connection, payload, length, HTTP_FLAG_DELAY);

    ctx->status = 2;
}

void httpServerDiscCbr(void *ctx_in)
{
    cbr_ctx_t *ctx = (cbr_ctx_t *)ctx_in;

    TRACE_INFO(">> httpServerDiscCbr\r\n");
    ctx->status = 3;
}

error_t httpServerRequestCallback(HttpConnection *connection,
                                  const char_t *uri)
{
    TRACE_INFO("httpServerRequestCallback: '%s'\n", uri);

    if (connection->request.auth.found && connection->request.auth.mode == HTTP_AUTH_MODE_DIGEST)
    {
        char uid[18];
        uint8_t *token = connection->private.authentication_token;

        TRACE_INFO("httpServerRequestCallback: '%s' auth\n", uri);

        if (!strncmp("/v2/content/", uri, 12))
        {
            osStrncpy(uid, &uri[12], sizeof(uid));
            uid[17] = 0;

            if (osStrlen(uid) != 16)
            {
                TRACE_WARNING("  invalid URI\n");
            }
            TRACE_INFO("  client requested %s\n", uid);
            TRACE_INFO("  client authenticated with %02X%02X%02X%02X...\n", token[0], token[1], token[2], token[3]);

            httpInitResponseHeader(connection);

/* sending response */
#if 0
            char *header_data = "Congratulations, here could have been the content for UID %s and the hash ";
            char *footer_data = " - if there was any...\r\n";

            char *build_string = malloc(strlen(header_data) + osStrlen(uid) + 2 * AUTH_TOKEN_LENGTH + osStrlen(footer_data));

            osSprintf(build_string, header_data, uid);
            for (int pos = 0; pos < AUTH_TOKEN_LENGTH; pos++)
            {
                char buf[3];
                osSprintf(buf, "%02X", token[pos]);
                osStrcat(build_string, buf);
            }
            osStrcat(build_string, footer_data);
            connection->response.contentType = "application/binary";
            connection->response.contentLength = osStrlen(build_string);

            error_t error = httpWriteHeader(connection);

            if (error != NO_ERROR)
            {
                free(build_string);
                TRACE_ERROR("Failed to send header");
                return error;
            }
            error = httpWriteStream(connection, build_string, connection->response.contentLength);
            free(build_string);
            if (error != NO_ERROR)
            {
                TRACE_ERROR("Failed to send header");
                return error;
            }

            // Properly close output stream
            error = httpCloseStream(connection);
#endif

            cbr_ctx_t ctx = {
                .status = 0,
                .connection = connection};
            req_cbr_t cbr = {
                .ctx = &ctx,
                .response = &httpServerResponseCbr,
                .header = &httpServerHeaderCbr,
                .body = &httpServerBodyCbr,
                .disconnect = &httpServerDiscCbr};

            /* here call cloud request, which has to get extended for cbr for header fields and content packets */
            error_t error = cloud_request_get(NULL, 0, uri, token, &cbr);

            TRACE_INFO("httpServerRequestCallback: %s (waiting)\n", uri);
            while (ctx.status != 3)
            {
                sleep(100);
            }
            error = httpCloseStream(connection);
            TRACE_INFO("httpServerRequestCallback: %s (done)\n", uri);
            return NO_ERROR;
        }
    }
    TRACE_INFO("httpServerRequestCallback: %s (ignoring)\n", uri);

    return ERROR_NOT_FOUND;
}

error_t httpServerUriNotFoundCallback(HttpConnection *connection,
                                      const char_t *uri)
{
    TRACE_INFO("httpServerUriNotFoundCallback: %s (ignoring)\n", uri);
    return ERROR_NOT_FOUND;
}

void httpParseAuthorizationField(HttpConnection *connection, char_t *value)
{
    if (!strncmp(value, "BD ", 3))
    {
        if (strlen(value) != 3 + 2 * AUTH_TOKEN_LENGTH)
        {
            TRACE_WARNING("Authentication: Failed to parse auth token '%s'\n", value);
            return;
        }
        for (int pos = 0; pos < AUTH_TOKEN_LENGTH; pos++)
        {
            char hex_digits[3];
            char *end_ptr = NULL;

            /* get a hex byte into a buffer for parsing it */
            osStrncpy(hex_digits, &value[3 + 2 * pos], 2);
            hex_digits[2] = 0;

            /* will still fail for minus sign and possibly other things, but then the token is just incorrect */
            connection->private.authentication_token[pos] = (uint8_t)osStrtoul(hex_digits, &end_ptr, 16);

            if (end_ptr != &hex_digits[2])
            {
                TRACE_WARNING("Authentication: Failed to parse auth token '%s'\n", value);
                return;
            }
        }
        /* if we come across this part, this means the token was most likely correctly *parsed* */
        connection->request.auth.found = 1;
        connection->request.auth.mode = HTTP_AUTH_MODE_DIGEST;
        connection->status = HTTP_ACCESS_ALLOWED;
    }
}

size_t httpAddAuthenticateField(HttpConnection *connection, char_t *output)
{
    TRACE_INFO("httpAddAuthenticateField\n");
    return 0;
}

error_t httpServerCgiCallback(HttpConnection *connection,
                              const char_t *param)
{
    // Not implemented
    TRACE_INFO("httpServerCgiCallback: %s\n", param);
    return ERROR_NOT_FOUND;
}

void server_init()
{
    HttpServerSettings settings;
    HttpServerContext context;

    httpServerGetDefaultSettings(&settings);
    settings.interface = NULL;
    settings.port = 80;
    settings.maxConnections = APP_HTTP_MAX_CONNECTIONS;
    settings.connections = httpConnections;

    strcpy(settings.rootDirectory, "www/");
    strcpy(settings.defaultDocument, "index.shtm");

    settings.cgiCallback = httpServerCgiCallback;
    settings.requestCallback = httpServerRequestCallback;
    settings.uriNotFoundCallback = httpServerUriNotFoundCallback;

    httpServerInit(&context, &settings);
    httpServerStart(&context);

    while (1)
    {
        sleep(100);
    }
}