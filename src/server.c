
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

#include "mqtt/mqtt_client.h"

#include "debug.h"

#include "cloud_request.h"
#include "handler_cloud.h"
#include "proto/toniebox.pb.rtnl.pb-c.h"

#define APP_HTTP_MAX_CONNECTIONS 32
HttpConnection httpConnections[APP_HTTP_MAX_CONNECTIONS];
HttpConnection httpsConnections[APP_HTTP_MAX_CONNECTIONS];

enum eRequestMethod
{
    REQ_ANY,
    REQ_GET,
    REQ_POST
};

typedef struct
{
    enum eRequestMethod method;
    char *path;
    error_t (*handler)(HttpConnection *connection, const char_t *uri);
} request_type_t;

error_t handleWww(HttpConnection *connection, const char_t *uri)
{
    return httpSendResponse(connection, &uri[4]);
}

/* const for now. later maybe dynamic? */
request_type_t request_paths[] = {
    //    {REQ_ANY, "/reverse", &handle_reverse},
    {REQ_GET, "/www", &handleWww},
    {REQ_GET, "/v1/time", &handleCloudTime},
    {REQ_GET, "/v1/ota", &handleCloudOTA},
    {REQ_GET, "/v1/claim", &handleCloudClaim},
    {REQ_GET, "/v2/content", &handleCloudContent},
    {REQ_POST, "/v1/freshness-check", &handleCloudFreshnessCheck},
    {REQ_POST, "/v1/log", &handleCloudLog}};

char_t *ipv4AddrToString(Ipv4Addr ipAddr, char_t *str)
{
    uint8_t *p;
    static char_t buffer[16];

    // If the NULL pointer is given as parameter, then the internal buffer is used
    if (str == NULL)
        str = buffer;

    // Cast the address to byte array
    p = (uint8_t *)&ipAddr;
    // Format IPv4 address
    osSprintf(str, "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 "", p[0], p[1], p[2], p[3]);

    // Return a pointer to the formatted string
    return str;
}
char_t *ipv6AddrToString(const Ipv6Addr *ipAddr, char_t *str)
{
    static char_t buffer[40];
    uint_t i;
    uint_t j;
    char_t *p;

    // Best run of zeroes
    uint_t zeroRunStart = 0;
    uint_t zeroRunEnd = 0;

    // If the NULL pointer is given as parameter, then the internal buffer is used
    if (str == NULL)
        str = buffer;

    // Find the longest run of zeros for "::" short-handing
    for (i = 0; i < 8; i++)
    {
        // Compute the length of the current sequence of zeroes
        for (j = i; j < 8 && !ipAddr->w[j]; j++)
            ;

        // Keep track of the longest one
        if ((j - i) > 1 && (j - i) > (zeroRunEnd - zeroRunStart))
        {
            // The symbol "::" should not be used to shorten just one zero field
            zeroRunStart = i;
            zeroRunEnd = j;
        }
    }

    // Format IPv6 address
    for (p = str, i = 0; i < 8; i++)
    {
        // Are we inside the best run of zeroes?
        if (i >= zeroRunStart && i < zeroRunEnd)
        {
            // Append a separator
            *(p++) = ':';
            // Skip the sequence of zeroes
            i = zeroRunEnd - 1;
        }
        else
        {
            // Add a separator between each 16-bit word
            if (i > 0)
                *(p++) = ':';

            // Convert the current 16-bit word to string
            p += osSprintf(p, "%" PRIx16, ntohs(ipAddr->w[i]));
        }
    }

    // A trailing run of zeroes has been found?
    if (zeroRunEnd == 8)
        *(p++) = ':';

    // Properly terminate the string
    *p = '\0';

    // Return a pointer to the formatted string
    return str;
}
char_t *ipAddrToString(const IpAddr *ipAddr, char_t *str)
{
#if (IPV4_SUPPORT == ENABLED)
    // IPv4 address?
    if (ipAddr->length == sizeof(Ipv4Addr))
    {
        // Convert IPv4 address to string representation
        return ipv4AddrToString(ipAddr->ipv4Addr, str);
    }
    else
#endif
#if (IPV6_SUPPORT == ENABLED)
        // IPv6 address?
        if (ipAddr->length == sizeof(Ipv6Addr))
        {
            // Convert IPv6 address to string representation
            return ipv6AddrToString(&ipAddr->ipv6Addr, str);
        }
        else
#endif
        // Invalid IP address?
        {
            static char_t c;

            // The last parameter is optional
            if (str == NULL)
            {
                str = &c;
            }

            // Properly terminate the string
            str[0] = '\0';

            // Return an empty string
            return str;
        }
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

void cbrCloudResponsePassthrough(void *ctx_in)
{
    cbr_ctx_t *ctx = (cbr_ctx_t *)ctx_in;
    char line[128];

    osSprintf(line, "HTTP/%u.%u %u This is fine", MSB(ctx->connection->response.version), LSB(ctx->connection->response.version), ctx->connection->response.statusCode);
    httpSend(ctx->connection, line, osStrlen(line), HTTP_FLAG_DELAY);
    ctx->status = PROX_STATUS_CONN;
}

void cbrCloudHeaderPassthrough(void *ctx_in, const char *header, const char *value)
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
void cbrCloudBodyPassthrough(void *ctx_in, const char *payload, size_t length)
{
    cbr_ctx_t *ctx = (cbr_ctx_t *)ctx_in;

    TRACE_INFO(">> httpServerBodyCbr: %lu received\r\n", length);
    httpSend(ctx->connection, payload, length, HTTP_FLAG_DELAY);
    ctx->status = PROX_STATUS_BODY;
}
void cbrCloudServerDiskPasshtorugh(void *ctx_in)
{
    cbr_ctx_t *ctx = (cbr_ctx_t *)ctx_in;

    TRACE_INFO(">> httpServerDiscCbr\r\n");
    ctx->status = PROX_STATUS_DONE;
}

error_t
httpServerRequestCallback(HttpConnection *connection,
                          const char_t *uri)
{
    if (connection->tlsContext != NULL && connection->tlsContext->cert != NULL)
    {
        if (connection->tlsContext->clientCertRequested)
        {
            TRACE_INFO(" Client cert requested\n");
        }
        TRACE_INFO(" ID: -1 CertType=%i AuthMode=%i \n", connection->tlsContext->peerCertType, connection->tlsContext->clientAuthMode);
        for (size_t i = 0; i < connection->tlsContext->numCerts; i++)
        {
            TRACE_INFO(" ID: %li CertType=%i \n", i, connection->tlsContext->certs[i].type);
        }
    }
    else
    {
        TRACE_INFO(" No Cert or TLS \n");
    }

    TRACE_INFO(" >> client requested '%s' via %s \n", uri, connection->request.method);
    for (size_t i = 0; i < sizeof(request_paths) / sizeof(request_paths[0]); i++)
    {
        size_t pathLen = osStrlen(request_paths[i].path);
        if (!osStrncmp(request_paths[i].path, uri, pathLen) && ((request_paths[i].method == REQ_ANY) || (request_paths[i].method == REQ_GET && !osStrcasecmp(connection->request.method, "GET")) || (request_paths[i].method == REQ_POST && !osStrcasecmp(connection->request.method, "POST"))))
        {
            return (*request_paths[i].handler)(connection, uri);
        }
    }

    if (!osStrncmp("/reverse", uri, 8))
    {
        cbr_ctx_t ctx = {
            .status = PROX_STATUS_IDLE,
            .connection = connection};

        req_cbr_t cbr = {
            .ctx = &ctx,
            .response = &cbrCloudResponsePassthrough,
            .header = &cbrCloudHeaderPassthrough,
            .body = &cbrCloudBodyPassthrough,
            .disconnect = &cbrCloudServerDiskPasshtorugh};

        /* here call cloud request, which has to get extended for cbr for header fields and content packets */
        uint8_t *token = connection->private.authentication_token;
        error_t error = cloud_request_get(NULL, 0, &uri[8], token, &cbr);
        if (error != NO_ERROR)
        {
            TRACE_ERROR("cloud_request_get() failed");
            return error;
        }

        TRACE_INFO("httpServerRequestCallback: (waiting)\n");
        while (ctx.status != PROX_STATUS_DONE)
        {
            sleep(100);
        }
        error = httpCloseStream(connection);

        TRACE_INFO("httpServerRequestCallback: (done)\n");
        return NO_ERROR;
    }
    else
    {
        if (1 == 0)
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

            TRACE_INFO("httpServerRequestCallback: (done)\n");
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

void mqttTestPublishCallback(MqttClientContext *context,
                             const char_t *topic, const uint8_t *message, size_t length,
                             bool_t dup, MqttQosLevel qos, bool_t retain, uint16_t packetId)
{
    // Debug message
    TRACE_INFO("PUBLISH packet received...\r\n");
    TRACE_INFO("  Dup: %u\r\n", dup);
    TRACE_INFO("  QoS: %u\r\n", qos);
    TRACE_INFO("  Retain: %u\r\n", retain);
    TRACE_INFO("  Packet Identifier: %u\r\n", packetId);
    TRACE_INFO("  Topic: %s\r\n", topic);
    TRACE_INFO("  Message (%" PRIuSIZE " bytes):\r\n", length);
    TRACE_INFO_ARRAY("    ", message, length);

    // Check topic name
    if (!strcmp(topic, "teddybench/pong"))
    {
        if (length == 2 && !strncasecmp((char_t *)message, "on", 2))
        {
        }
    }
}
/*
error_t mqttConnect(MqttClientContext *mqtt_context)
{
    error_t error;
    IpAddr mqttIp;
    mqttClientInit(mqtt_context);
    mqttClientSetVersion(mqtt_context, MQTT_VERSION_3_1_1);
    mqttClientSetTransportProtocol(mqtt_context, MQTT_TRANSPORT_PROTOCOL_TCP);
    // Register publish callback function
    mqttClientRegisterPublishCallback(mqtt_context, mqttTestPublishCallback);

    mqttClientSetTimeout(mqtt_context, 20000);
    mqttClientSetKeepAlive(mqtt_context, 30);
    mqttClientSetIdentifier(mqtt_context, "teddycloud");
    mqttClientSetAuthInfo(mqtt_context, "username", "password");
    mqttClientSetWillMessage(mqtt_context, "teddycloud/status",
                             "offline", 7, MQTT_QOS_LEVEL_1, FALSE);

    do
    {
        // Establish connection with the MQTT server
        error = mqttClientConnect(mqtt_context,
                                  &mqttIp, 1883, TRUE);
        // Any error to report?
        if (error)
            break;

        // Subscribe to the desired topics
        error = mqttClientSubscribe(mqtt_context,
                                    "teddycloud/*", MQTT_QOS_LEVEL_1, NULL);
        // Any error to report?
        if (error)
            break;

        // Send PUBLISH packet
        error = mqttClientPublish(mqtt_context, "teddycloud/status",
                                  "online", 6, MQTT_QOS_LEVEL_1, TRUE, NULL);
        // Any error to report?
        if (error)
            break;

        // End of exception handling block
    } while (0);

    // Check status code
    if (error)
    {
        // Close connection
        mqttClientClose(mqtt_context);
    }

    // Return status code
    return error;
}
*/
void server_init()
{
    /*
    bool_t mqttConnected = FALSE;
    error_t error;
    MqttClientContext mqtt_context;
*/
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
    http_settings.port = 80;

    /* use them for HTTPS */
    https_settings = http_settings;
    https_settings.connections = httpsConnections;
    https_settings.port = 443;
    https_settings.tlsInitCallback = httpServerTlsInitCallback;

    if (httpServerInit(&http_context, &http_settings) != NO_ERROR)
    {
        TRACE_ERROR("httpServerInit() for HTTP failed\n");
        return;
    }
    if (httpServerInit(&https_context, &https_settings) != NO_ERROR)
    {
        TRACE_ERROR("httpServerInit() for HTTPS failed\n");
        return;
    }
    if (httpServerStart(&http_context) != NO_ERROR)
    {
        TRACE_ERROR("httpServerStart() for HTTP failed\n");
        return;
    }
    if (httpServerStart(&https_context) != NO_ERROR)
    {
        TRACE_ERROR("httpServerStart() for HTTPS failed\n");
        return;
    }

    while (1)
    {
        /*
        if (!mqttConnected)
        {
            error = mqttConnect(&mqtt_context);
            if (!error)
            {
                mqttConnected = TRUE;
            }
        }
        error = NO_ERROR;
        error = mqttClientPublish(&mqtt_context, "teddycloud/ping",
                                  "pong", 4, MQTT_QOS_LEVEL_1, TRUE, NULL);
        if (!error)
        {
            // Process events
            error = mqttClientTask(&mqtt_context, 100);
        }
        if (error)
        {
            // Close connection
            mqttClientClose(&mqtt_context);
            // Update connection state
            mqttConnected = FALSE;
            // Recovery delay
            osDelayTask(2000);
        }
        */
        sleep(100);
    }
}