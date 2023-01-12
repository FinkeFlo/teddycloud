
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
#include "rng/yarrow.h"
#include "tls_adapter.h"

#include "debug.h"

#include "cloud_request.h"

#define APP_HTTP_MAX_CONNECTIONS 32
HttpConnection httpConnections[APP_HTTP_MAX_CONNECTIONS];
HttpConnection httpsConnections[APP_HTTP_MAX_CONNECTIONS];

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

#define PROX_STATUS_IDLE 0
#define PROX_STATUS_CONN 1
#define PROX_STATUS_HEAD 2
#define PROX_STATUS_BODY 3
#define PROX_STATUS_DONE 4

typedef struct
{
    uint32_t status;
    HttpConnection *connection;
} cbr_ctx_t;

void httpServerResponseCbr(void *ctx_in)
{
    cbr_ctx_t *ctx = (cbr_ctx_t *)ctx_in;
    char line[128];

    osSprintf(line, "HTTP/%u.%u %u This is fine", MSB(ctx->connection->response.version), LSB(ctx->connection->response.version), ctx->connection->response.statusCode);

    httpSend(ctx->connection, line, osStrlen(line), HTTP_FLAG_DELAY);

    ctx->status = PROX_STATUS_CONN;
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

    ctx->status = PROX_STATUS_HEAD;
}

void httpServerBodyCbr(void *ctx_in, const char *payload, size_t length)
{
    cbr_ctx_t *ctx = (cbr_ctx_t *)ctx_in;

    TRACE_INFO(">> httpServerBodyCbr: %lu received\r\n", length);
    httpSend(ctx->connection, payload, length, HTTP_FLAG_DELAY);

    ctx->status = PROX_STATUS_BODY;
}

void httpServerDiscCbr(void *ctx_in)
{
    cbr_ctx_t *ctx = (cbr_ctx_t *)ctx_in;

    TRACE_INFO(">> httpServerDiscCbr\r\n");
    ctx->status = PROX_STATUS_DONE;
}

error_t httpServerRequestCallback(HttpConnection *connection,
                                  const char_t *uri)
{
    TRACE_INFO(" >> client requested '%s'\n", uri);

    char uid[18];
    uint8_t *token = connection->private.authentication_token;

    if (!osStrncmp("/reverse", uri, 8))
    {
        cbr_ctx_t ctx = {
            .status = PROX_STATUS_IDLE,
            .connection = connection};

        req_cbr_t cbr = {
            .ctx = &ctx,
            .response = &httpServerResponseCbr,
            .header = &httpServerHeaderCbr,
            .body = &httpServerBodyCbr,
            .disconnect = &httpServerDiscCbr};

        /* here call cloud request, which has to get extended for cbr for header fields and content packets */
        error_t error = cloud_request_get(NULL, 0, &uri[8], token, &cbr);
        if (error != NO_ERROR)
        {
            TRACE_ERROR("cloud_request_get() failed");
            return error;
        }

        TRACE_INFO("httpServerRequestCallback: %s (waiting)\n", uri);
        while (ctx.status != PROX_STATUS_DONE)
        {
            sleep(100);
        }
        error = httpCloseStream(connection);

        TRACE_INFO("httpServerRequestCallback: %s (done)\n", uri);
        return NO_ERROR;
    }
    else if (!osStrcmp("/v1/freshness-check", uri))
    {
    }
    else if (!osStrcmp("/v1/ota", uri))
    {
    }
    else if (!osStrcmp("/v1/time", uri))
    {
        TRACE_INFO(" >> respond with current time\n");

        char response[32];

        sprintf(response, "%ld", time(NULL));

        httpInitResponseHeader(connection);
        connection->response.contentType = "text/plain; charset=utf-8";
        connection->response.contentLength = osStrlen(response);

        error_t error = httpWriteHeader(connection);
        if (error != NO_ERROR)
        {
            TRACE_ERROR("Failed to send header");
            return error;
        }

        error = httpWriteStream(connection, response, connection->response.contentLength);
        if (error != NO_ERROR)
        {
            TRACE_ERROR("Failed to send payload");
            return error;
        }
        error = httpCloseStream(connection);
        if (error != NO_ERROR)
        {
            TRACE_ERROR("Failed to close");
            return error;
        }

        TRACE_INFO("httpServerRequestCallback: %s (done)\n", uri);
        return NO_ERROR;
    }
    else if (!osStrncmp("/v2/content/", uri, 12))
    {
        if (connection->request.auth.found && connection->request.auth.mode == HTTP_AUTH_MODE_DIGEST)
        {
            TRACE_INFO("httpServerRequestCallback: '%s' auth\n", uri);

            osStrncpy(uid, &uri[12], sizeof(uid));
            uid[17] = 0;

            if (osStrlen(uid) != 16)
            {
                TRACE_WARNING(" >>  invalid URI\n");
            }
            TRACE_INFO(" >> client requested UID %s\n", uid);
            TRACE_INFO(" >> client authenticated with %02X%02X%02X%02X...\n", token[0], token[1], token[2], token[3]);

            httpInitResponseHeader(connection);

            char *header_data = "Congratulations, here could have been the content for UID %s and the hash ";
            char *footer_data = " - if there was any...\r\n";

            char *build_string = osAllocMem(strlen(header_data) + osStrlen(uid) + 2 * AUTH_TOKEN_LENGTH + osStrlen(footer_data));

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
                osFreeMem(build_string);
                TRACE_ERROR("Failed to send header");
                return error;
            }

            error = httpWriteStream(connection, build_string, connection->response.contentLength);
            osFreeMem(build_string);
            if (error != NO_ERROR)
            {
                TRACE_ERROR("Failed to send payload");
                return error;
            }

            error = httpCloseStream(connection);
            if (error != NO_ERROR)
            {
                TRACE_ERROR("Failed to close");
                return error;
            }

            return NO_ERROR;
        }
        else
        {
            const char *response = "<html><head></head><body>No content for you</body></html>";

            httpInitResponseHeader(connection);
            connection->response.contentType = "text/html";
            connection->response.contentLength = osStrlen(response);

            error_t error = httpWriteHeader(connection);
            if (error != NO_ERROR)
            {
                TRACE_ERROR("Failed to send header");
                return error;
            }

            error = httpWriteStream(connection, response, connection->response.contentLength);
            if (error != NO_ERROR)
            {
                TRACE_ERROR("Failed to send payload");
                return error;
            }

            error = httpCloseStream(connection);
            if (error != NO_ERROR)
            {
                TRACE_ERROR("Failed to close");
                return error;
            }

            TRACE_INFO("httpServerRequestCallback: %s (done)\n", uri);
            return NO_ERROR;
        }
    }

    const char *response = "<html><head><title>Nothing found here</title></head><body>There is nothing to see</body></html>";
    httpInitResponseHeader(connection);
    connection->response.contentType = "text/html";
    connection->response.contentLength = osStrlen(response);

    error_t error = httpWriteHeader(connection);
    if (error != NO_ERROR)
    {
        TRACE_ERROR("Failed to send header");
        return error;
    }

    error = httpWriteStream(connection, response, connection->response.contentLength);
    if (error != NO_ERROR)
    {
        TRACE_ERROR("Failed to send header");
        return error;
    }

    TRACE_INFO(" >> ERROR_NOT_FOUND\n");
    return NO_ERROR;
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

HttpAccessStatus httpServerAuthCallback(HttpConnection *connection, const char_t *user, const char_t *uri)
{
    return HTTP_ACCESS_ALLOWED;
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
    return NO_ERROR;
}

error_t httpServerTlsInitCallback(HttpConnection *connection, TlsContext *tlsContext)
{
    error_t error;

    // Set TX and RX buffer size
    error = tlsSetBufferSize(tlsContext, 2048, 2048);
    // Any error to report?
    if (error)
        return error;

    // Set the PRNG algorithm to be used
    error = tlsSetPrng(tlsContext, YARROW_PRNG_ALGO, &yarrowContext);
    // Any error to report?
    if (error)
        return error;

    // Session cache that will be used to save/resume TLS sessions
    error = tlsSetCache(tlsContext, tlsCache);
    // Any error to report?
    if (error)
        return error;

    // Client authentication is not required
    error = tlsSetClientAuthMode(tlsContext, TLS_CLIENT_AUTH_OPTIONAL);
    // Any error to report?
    if (error)
        return error;

    // Import server's certificate

    error = tlsAddCertificate(tlsContext, serverCert, serverCertLen, serverKey, serverKeyLen);
    // Any error to report?
    if (error)
    {
        if (error == ERROR_BAD_CERTIFICATE)
        {
            TRACE_INFO("Adding certificate failed: ERROR_BAD_CERTIFICATE\n");
        }
        else
        {
            TRACE_INFO("Adding certificate failed: %d\n", error);
        }
        return error;
    }

    // Successful processing
    return NO_ERROR;
}

void server_init()
{
    HttpServerSettings http_settings;
    HttpServerSettings https_settings;
    HttpServerContext http_context;
    HttpServerContext https_context;

    /* setup settings for HTTP */
    httpServerGetDefaultSettings(&http_settings);

    http_settings.maxConnections = APP_HTTP_MAX_CONNECTIONS;
    http_settings.connections = httpConnections;
    strcpy(http_settings.rootDirectory, "www/");
    strcpy(http_settings.defaultDocument, "index.shtm");

    http_settings.cgiCallback = httpServerCgiCallback;
    http_settings.requestCallback = httpServerRequestCallback;
    http_settings.uriNotFoundCallback = httpServerUriNotFoundCallback;
    http_settings.authCallback = httpServerAuthCallback;

    /* use them for HTTPS */
    https_settings = http_settings;
    https_settings.connections = httpsConnections;
    https_settings.port = 443;
    https_settings.tlsInitCallback = httpServerTlsInitCallback;

    httpServerInit(&http_context, &http_settings);
    httpServerInit(&https_context, &https_settings);
    httpServerStart(&http_context);
    httpServerStart(&https_context);

    while (1)
    {
        sleep(100);
    }
}