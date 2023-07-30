#ifndef _HANDLER_CLOUD_H
#define _HANDLER_CLOUD_H

#include "debug.h"

#include "core/net.h"
#include "core/ethernet.h"
#include "core/ip.h"
#include "core/tcp.h"
#include "http/http_server.h"
#include "http/http_server_misc.h"

#include "handler.h"

#define BODY_BUFFER_SIZE 4096
#define TAF_HEADER_SIZE 4092

void getContentPathFromCharRUID(char ruid[17], char **pcontentPath, settings_t *settings);
void getContentPathFromUID(uint64_t uid, char **pcontentPath, settings_t *settings);
tonie_info_t getTonieInfo(const char *contentPath);
void freeTonieInfo(tonie_info_t *tonieInfo);

void httpPrepareHeader(HttpConnection *connection, const void *contentType, size_t contentLength);

error_t handleCloudTime(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudOTA(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudLog(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudClaim(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudContent(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx, bool_t noPassword);
error_t handleCloudContentV1(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudContentV2(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudFreshnessCheck(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudReset(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);

#endif