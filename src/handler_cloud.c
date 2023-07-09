#include <time.h>
#include <stdbool.h>

#include "handler_cloud.h"
#include "proto/toniebox.pb.freshness-check.fc-request.pb-c.h"
#include "proto/toniebox.pb.freshness-check.fc-response.pb-c.h"

void getContentPathFromCharRUID(char ruid[17], char contentPath[30])
{
    osSprintf(contentPath, "www/CONTENT/%.8s/%.8s", ruid, &ruid[8]);
    strupr(&contentPath[4]);
}
void getContentPathFromUID(uint64_t uid, char contentPath[30])
{
    uint16_t cuid[9];
    osSprintf(cuid, "%016lX", uid);
    uint16_t cruid[9];
    for (uint8_t i = 0; i < 8; i++)
    {
        cruid[i] = cuid[7 - i];
    }
    cruid[8] = 0;
    getContentPathFromCharRUID(cruid, contentPath);
}

error_t httpWriteResponse(HttpConnection *connection, const void *data, bool_t freeMemory)
{

    error_t error = httpWriteHeader(connection);
    if (error != NO_ERROR)
    {
        if (freeMemory)
            osFreeMem(data);
        TRACE_ERROR("Failed to send header");
        return error;
    }

    error = httpWriteStream(connection, data, connection->response.contentLength);
    if (freeMemory)
        if (freeMemory)
            osFreeMem(data);
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
void httpPrepareHeader(HttpConnection *connection, const void *contentType, size_t contentLength)
{
    httpInitResponseHeader(connection);
    connection->response.keepAlive = true;
    connection->response.chunkedEncoding = false;
    connection->response.contentType = contentType;
    connection->response.contentLength = contentLength;
}

error_t handleCloudTime(HttpConnection *connection, const char_t *uri)
{
    TRACE_INFO(" >> respond with current time\n");

    char response[32];

    sprintf(response, "%ld", time(NULL));

    httpPrepareHeader(connection, "text/plain; charset=utf-8", osStrlen(response));
    return httpWriteResponse(connection, response, false);
}

error_t handleCloudOTA(HttpConnection *connection, const char_t *uri)
{
    return NO_ERROR;
}

error_t handleCloudLog(HttpConnection *connection, const char_t *uri)
{
    return NO_ERROR;
}

error_t handleCloudClaim(HttpConnection *connection, const char_t *uri)
{
    char uid[18];
    uint8_t *token = connection->private.authentication_token;

    osStrncpy(uid, &uri[10], sizeof(uid));
    uid[17] = 0;

    if (osStrlen(uid) != 16)
    {
        TRACE_WARNING(" >>  invalid URI\n");
    }
    TRACE_INFO(" >> client requested UID %s\n", uid);
    TRACE_INFO(" >> client authenticated with %02X%02X%02X%02X...\n", token[0], token[1], token[2], token[3]);

    return NO_ERROR;
}

void strupr(char input[])
{
    for (uint16_t i = 0; input[i]; i++)
    {
        input[i] = toupper(input[i]);
    }
}
error_t handleCloudContent(HttpConnection *connection, const char_t *uri)
{
    char ruid[18];
    char contentPath[31];
    uint8_t *token = connection->private.authentication_token;

    if (connection->request.auth.found && connection->request.auth.mode == HTTP_AUTH_MODE_DIGEST)
    {
        osStrncpy(ruid, &uri[12], sizeof(ruid));
        ruid[17] = 0;

        if (osStrlen(ruid) != 16)
        {
            TRACE_WARNING(" >>  invalid URI\n");
        }
        TRACE_INFO(" >> client requested UID %s\n", ruid);
        TRACE_INFO(" >> client authenticated with %02X%02X%02X%02X...\n", token[0], token[1], token[2], token[3]);

        getContentPathFromCharRUID(ruid, contentPath);
        connection->response.keepAlive = true;
        error_t error = httpSendResponse(connection, &contentPath[4]);
        if (error)
        {
            TRACE_ERROR(" >> file %s not available or not send, error=%u...\n", contentPath, error);
            if (error == ERROR_NOT_FOUND)
            {
                httpPrepareHeader(connection, NULL, 0);
                connection->response.statusCode = 404;
                return httpWriteResponse(connection, NULL, false);
            }
        }
        return error;
    }
    return NO_ERROR;
}

error_t handleCloudFreshnessCheck(HttpConnection *connection, const char_t *uri)
{
    char_t data[BODY_BUFFER_SIZE];
    size_t size;
    if (BODY_BUFFER_SIZE <= connection->request.byteCount)
    {
        TRACE_ERROR("Body size %li bigger than buffer size %i bytes", connection->request.byteCount, BODY_BUFFER_SIZE);
    }
    else
    {
        error_t error = httpReceive(connection, &data, BODY_BUFFER_SIZE, &size, 0x00);
        if (error != NO_ERROR)
        {
            TRACE_ERROR("httpReceive failed!");
            return error;
        }
        TRACE_INFO("Content (%li of %li)\n", size, connection->request.byteCount);
        TonieFreshnessCheckRequest *freshReq = tonie_freshness_check_request__unpack(NULL, size, (const uint8_t *)data);
        if (freshReq == NULL)
        {
            TRACE_ERROR("Unpacking freshness request failed!\n");
        }
        else
        {
            uint64_t uidTest;
            TRACE_INFO("Found %li tonies:\n", freshReq->n_tonie_infos);
            TonieFreshnessCheckResponse freshResp = TONIE_FRESHNESS_CHECK_RESPONSE__INIT;
            freshResp.n_tonie_marked = 0;
            freshResp.tonie_marked = malloc(sizeof(uint64_t *) * freshReq->n_tonie_infos);
            for (uint16_t i = 0; i < freshReq->n_tonie_infos; i++)
            {
                struct tm tm_info;
                char date_buffer[32];
                bool custom = false;
                bool nocloud = false;
                bool live = false;
                time_t unix_time = freshReq->tonie_infos[i]->audio_id;

                if (unix_time < 0x0e000000)
                {
                    sprintf(date_buffer, "special");
                }
                else
                {
                    /* custom tonies from TeddyBench have the audio id reduced by a constant */
                    if (unix_time < 0x50000000)
                    {
                        unix_time += 0x50000000;
                        custom = true;
                    }
                    if (localtime_r(&unix_time, &tm_info) == NULL)
                    {
                        sprintf(date_buffer, "(localtime failed)");
                    }
                    else
                    {
                        strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d %H:%M:%S", &tm_info);
                    }
                }
                char contentPath[30 + 8]; //".nocloud" / ".live"
                getContentPathFromUID(freshReq->tonie_infos[i]->uid, contentPath);
                osStrcat(contentPath, ".nocloud");
                nocloud = fsFileExists(contentPath);
                contentPath[29] = 0;
                osStrcat(contentPath, ".live");
                live = fsFileExists(contentPath);
                TRACE_INFO("  uid: %016lX, nocloud: %d, live: %d, audioid: %08X (%s%s)\n",
                           freshReq->tonie_infos[i]->uid,
                           nocloud,
                           live,
                           freshReq->tonie_infos[i]->audio_id,
                           date_buffer,
                           custom ? ", custom" : "");
                if (live)
                {
                    freshResp.tonie_marked[freshResp.n_tonie_marked++] = freshReq->tonie_infos[i]->uid;
                }
            }
            tonie_freshness_check_request__free_unpacked(freshReq, NULL);
            // Upstream
            // TODO push to Boxine

            freshResp.max_vol_spk = 3;
            freshResp.slap_en = 0;
            freshResp.slap_dir = 0;
            freshResp.max_vol_hdp = 3;
            freshResp.led = 2;
            size_t dataLen = tonie_freshness_check_response__get_packed_size(&freshResp);
            tonie_freshness_check_response__pack(&freshResp, (uint8_t *)data);
            free(freshResp.tonie_marked);
            TRACE_INFO("Freshness check response: size=%li, content=%s\n", dataLen, data);

            httpPrepareHeader(connection, "application/octet-stream; charset=utf-8", dataLen);
            return httpWriteResponse(connection, data, false);
            // tonie_freshness_check_response__free_unpacked(&freshResp, NULL);
        }
        return NO_ERROR;
    }
    return NO_ERROR;
}