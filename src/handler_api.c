
#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "path.h"
#include "path_ext.h"
#include "server_helpers.h"
#include "fs_port.h"
#include "handler.h"
#include "handler_api.h"
#include "handler_cloud.h"
#include "settings.h"
#include "stats.h"
#include "returncodes.h"
#include "cJSON.h"

typedef enum
{
    PARSE_HEADER,
    PARSE_FILENAME,
    PARSE_BODY
} eMultipartState;

/* multipart receive buffer */
#define DATA_SIZE 1024
#define SAVE_SIZE 80
#define BUFFER_SIZE (DATA_SIZE + SAVE_SIZE)

error_t handleApiAssignUnknown(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    const char *rootPath;
    char *response = "OK";
    error_t ret = NO_ERROR;

    TRACE_INFO("Query: '%s'\r\n", queryString);

    char path[256];
    char overlay[16];
    char special[16];

    osStrcpy(path, "");
    osStrcpy(overlay, "");
    osStrcpy(special, "");

    if (queryGet(queryString, "overlay", overlay, sizeof(overlay)))
    {
        TRACE_INFO("got overlay '%s'\r\n", overlay);
    }
    rootPath = settings_get_string_ovl("internal.contentdirfull", overlay);

    if (queryGet(queryString, "path", path, sizeof(path)))
    {
        TRACE_INFO("got path '%s'\r\n", path);
    }
    if (queryGet(queryString, "special", special, sizeof(special)))
    {
        TRACE_INFO("requested index for special '%s'\r\n", special);
        if (!osStrcmp(special, "library"))
        {
            rootPath = settings_get_string_ovl("internal.librarydirfull", overlay);

            if (rootPath == NULL || !fsDirExists(rootPath))
            {
                TRACE_ERROR("internal.librarydirfull not set to a valid path: '%s'\r\n", rootPath);
                response = "FAIL";
                ret = ERROR_FAILURE;
            }
        }
    }

    if (ret == NO_ERROR)
    {
        pathSafeCanonicalize(path);
        char *pathAbsolute = osAllocMem(osStrlen(rootPath) + osStrlen(path) + 2);

        osSprintf(pathAbsolute, "%s/%s", rootPath, path);
        pathSafeCanonicalize(pathAbsolute);

        TRACE_INFO("Set '%s' for next unknown request\r\n", pathAbsolute);

        settings_set_string_ovl("internal.assign_unknown", pathAbsolute, overlay);
        osFreeMem(pathAbsolute);
    }

    httpInitResponseHeader(connection);
    connection->response.contentType = "text/plain";
    connection->response.contentLength = osStrlen(response);

    return httpWriteResponseString(connection, response, false);
}

error_t handleApiGetIndex(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *jsonArray = cJSON_AddArrayToObject(json, "options");
    int pos = 0;

    char overlay[16];
    osStrcpy(overlay, "");
    if (queryGet(queryString, "overlay", overlay, sizeof(overlay)))
    {
        TRACE_INFO("got overlay '%s'\r\n", overlay);
    }
    while (true)
    {
        setting_item_t *opt = settings_get_ovl(pos, overlay);

        if (!opt)
        {
            break;
        }
        if (opt->internal)
        {
            pos++;
            continue;
        }

        cJSON *jsonEntry = cJSON_CreateObject();
        cJSON_AddStringToObject(jsonEntry, "ID", opt->option_name);
        cJSON_AddStringToObject(jsonEntry, "shortname", opt->option_name);
        cJSON_AddStringToObject(jsonEntry, "description", opt->description);

        switch (opt->type)
        {
        case TYPE_BOOL:
            cJSON_AddStringToObject(jsonEntry, "type", "bool");
            cJSON_AddBoolToObject(jsonEntry, "value", settings_get_bool_ovl(opt->option_name, overlay));
            break;
        case TYPE_UNSIGNED:
            cJSON_AddStringToObject(jsonEntry, "type", "uint");
            cJSON_AddNumberToObject(jsonEntry, "value", settings_get_unsigned_ovl(opt->option_name, overlay));
            break;
        case TYPE_SIGNED:
            cJSON_AddStringToObject(jsonEntry, "type", "int");
            cJSON_AddNumberToObject(jsonEntry, "value", settings_get_signed_ovl(opt->option_name, overlay));
            break;
        case TYPE_HEX:
            cJSON_AddStringToObject(jsonEntry, "type", "hex");
            cJSON_AddNumberToObject(jsonEntry, "value", settings_get_unsigned_ovl(opt->option_name, overlay));
            break;
        case TYPE_STRING:
            cJSON_AddStringToObject(jsonEntry, "type", "string");
            cJSON_AddStringToObject(jsonEntry, "value", settings_get_string_ovl(opt->option_name, overlay));
            break;
        case TYPE_FLOAT:
            cJSON_AddStringToObject(jsonEntry, "type", "float");
            cJSON_AddNumberToObject(jsonEntry, "value", settings_get_float_ovl(opt->option_name, overlay));
            break;
        default:
            break;
        }

        cJSON_AddItemToArray(jsonArray, jsonEntry);

        pos++;
    }

    char *jsonString = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    httpInitResponseHeader(connection);
    connection->response.contentType = "text/json";
    connection->response.contentLength = osStrlen(jsonString);

    return httpWriteResponse(connection, jsonString, connection->response.contentLength, true);
}

error_t handleApiTrigger(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    const char *item = &uri[5];
    char response[256];

    osSprintf(response, "FAILED");

    if (!strcmp(item, "triggerExit"))
    {
        TRACE_INFO("Triggered Exit\r\n");
        settings_set_bool("internal.exit", TRUE);
        settings_set_signed("internal.returncode", RETURNCODE_USER_QUIT);
        osSprintf(response, "OK");
    }
    else if (!strcmp(item, "triggerRestart"))
    {
        TRACE_INFO("Triggered Restart\r\n");
        settings_set_bool("internal.exit", TRUE);
        settings_set_signed("internal.returncode", RETURNCODE_USER_RESTART);
        osSprintf(response, "OK");
    }
    else if (!strcmp(item, "triggerReloadConfig"))
    {
        TRACE_INFO("Triggered ReloadConfig\r\n");
        osSprintf(response, "OK");
        settings_load();
    }
    else if (!strcmp(item, "triggerWriteConfig"))
    {
        TRACE_INFO("Triggered WriteConfig\r\n");
        osSprintf(response, "OK");
        settings_save();
    }

    httpInitResponseHeader(connection);
    connection->response.contentType = "text/plain";
    connection->response.contentLength = osStrlen(response);

    return httpWriteResponse(connection, response, connection->response.contentLength, false);
}

error_t handleApiGet(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    const char *item = &uri[5 + 3 + 1];

    char response[32];
    osStrcpy(response, "ERROR");
    const char *response_ptr = response;

    char overlay[16];
    osStrcpy(overlay, "");
    if (queryGet(queryString, "overlay", overlay, sizeof(overlay)))
    {
        TRACE_INFO("got overlay '%s'\r\n", overlay);
    }
    setting_item_t *opt = settings_get_by_name_ovl(item, overlay);

    if (opt)
    {
        switch (opt->type)
        {
        case TYPE_BOOL:
            osSprintf(response, "%s", settings_get_bool_ovl(item, overlay) ? "true" : "false");
            break;
        case TYPE_HEX:
        case TYPE_UNSIGNED:
            osSprintf(response, "%d", settings_get_unsigned_ovl(item, overlay));
            break;
        case TYPE_SIGNED:
            osSprintf(response, "%d", settings_get_signed_ovl(item, overlay));
            break;
        case TYPE_STRING:
            response_ptr = settings_get_string_ovl(item, overlay);
            break;
        case TYPE_FLOAT:
            osSprintf(response, "%f", settings_get_float_ovl(item, overlay));
            break;
        default:
            break;
        }
    }

    httpInitResponseHeader(connection);
    connection->response.contentType = "text/plain";
    connection->response.contentLength = osStrlen(response_ptr);

    return httpWriteResponse(connection, (char_t *)response_ptr, connection->response.contentLength, false);
}

error_t handleApiSet(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    char response[256];
    osSprintf(response, "ERROR");
    const char *item = &uri[9];

    char_t data[BODY_BUFFER_SIZE];
    size_t size;
    if (BODY_BUFFER_SIZE <= connection->request.byteCount)
    {
        TRACE_ERROR("Body size for setting '%s' %zu bigger than buffer size %i bytes\r\n", item, connection->request.byteCount, BODY_BUFFER_SIZE);
    }
    else
    {
        error_t error = httpReceive(connection, &data, BODY_BUFFER_SIZE, &size, 0x00);
        if (error != NO_ERROR)
        {
            TRACE_ERROR("httpReceive failed!");
            return error;
        }
        data[size] = 0;

        TRACE_INFO("Setting: '%s' to '%s'\r\n", item, data);

        char overlay[16];
        osStrcpy(overlay, "");
        if (queryGet(queryString, "overlay", overlay, sizeof(overlay)))
        {
            TRACE_INFO("got overlay '%s'\r\n", overlay);
        }
        setting_item_t *opt = settings_get_by_name_ovl(item, overlay);
        bool success = false;

        if (opt)
        {
            switch (opt->type)
            {
            case TYPE_BOOL:
            {
                success = settings_set_bool_ovl(item, !strcasecmp(data, "true"), overlay);
                break;
            }
            case TYPE_STRING:
            {
                success = settings_set_string_ovl(item, data, overlay);
                break;
            }
            case TYPE_HEX:
            {
                uint32_t value = strtoul(data, NULL, 16);
                success = settings_set_unsigned_ovl(item, value, overlay);
                break;
            }

            case TYPE_UNSIGNED:
            {
                uint32_t value = strtoul(data, NULL, 10);
                success = settings_set_unsigned_ovl(item, value, overlay);
                break;
            }

            case TYPE_SIGNED:
            {
                int32_t value = strtol(data, NULL, 10);
                success = settings_set_signed_ovl(item, value, overlay);
                break;
            }

            case TYPE_FLOAT:
            {
                float value = strtof(data, NULL);
                success = settings_set_float_ovl(item, value, overlay);
                break;
            }

            default:
                break;
            }
        }
        else
        {
            TRACE_ERROR("Setting '%s' is unknown", item);
        }

        if (success)
        {
            osStrcpy(response, "OK");
        }
    }

    httpPrepareHeader(connection, "text/plain; charset=utf-8", 0);
    return httpWriteResponseString(connection, response, false);
}

error_t handleApiFileIndex(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    char *jsonString = strdup("{\"files\":[]}");

    do
    {

        char overlay[16];
        char special[16];
        osStrcpy(overlay, "");
        osStrcpy(special, "");

        if (queryGet(queryString, "overlay", overlay, sizeof(overlay)))
        {
            TRACE_INFO("requested index using overlay '%s'\r\n", overlay);
        }
        const char *rootPath = settings_get_string_ovl("internal.contentdirfull", overlay);
        if (rootPath == NULL || !fsDirExists(rootPath))
        {
            TRACE_ERROR("internal.contentdirfull not set to a valid path: '%s'\r\n", rootPath);
            break;
        }

        if (queryGet(queryString, "special", special, sizeof(special)))
        {
            TRACE_INFO("requested index for '%s'\r\n", special);
            if (!osStrcmp(special, "library"))
            {
                rootPath = settings_get_string_ovl("internal.librarydirfull", overlay);

                if (rootPath == NULL || !fsDirExists(rootPath))
                {
                    TRACE_ERROR("internal.librarydirfull not set to a valid path: '%s'\r\n", rootPath);
                    break;
                }
            }
        }

        char path[128];
        char pathAbsolute[256];

        if (!queryGet(queryString, "path", path, sizeof(path)))
        {
            osStrcpy(path, "/");
        }

        pathSafeCanonicalize(path);

        osSnprintf(pathAbsolute, sizeof(pathAbsolute), "%s/%s", rootPath, path);
        pathAbsolute[sizeof(pathAbsolute) - 1] = 0;

        pathSafeCanonicalize(pathAbsolute);

        int pos = 0;
        FsDir *dir = fsOpenDir(pathAbsolute);
        if (dir == NULL)
        {
            TRACE_ERROR("Failed to open dir '%s'\r\n", pathAbsolute);
            break;
        }

        cJSON *json = cJSON_CreateObject();
        cJSON *jsonArray = cJSON_AddArrayToObject(json, "files");

        while (true)
        {
            FsDirEntry entry;

            if (fsReadDir(dir, &entry) != NO_ERROR)
            {
                fsCloseDir(dir);
                break;
            }

            if (!osStrcmp(entry.name, ".") || !osStrcmp(entry.name, ".."))
            {
                continue;
            }

            char dateString[64];

            osSnprintf(dateString, sizeof(dateString), " %04" PRIu16 "-%02" PRIu8 "-%02" PRIu8 ",  %02" PRIu8 ":%02" PRIu8 ":%02" PRIu8,
                       entry.modified.year, entry.modified.month, entry.modified.day,
                       entry.modified.hours, entry.modified.minutes, entry.modified.seconds);

            char filePathAbsolute[384];
            osSnprintf(filePathAbsolute, sizeof(filePathAbsolute), "%s/%s", pathAbsolute, entry.name);

            char desc[64];
            desc[0] = 0;
            tonie_info_t tafInfo = getTonieInfo(filePathAbsolute);
            if (tafInfo.valid)
            {
                osSnprintf(desc, sizeof(desc), "TAF:%08X:", tafInfo.tafHeader->audio_id);
                for (int pos = 0; pos < tafInfo.tafHeader->sha1_hash.len; pos++)
                {
                    char tmp[3];
                    osSprintf(tmp, "%02X", tafInfo.tafHeader->sha1_hash.data[pos]);
                    osStrcat(desc, tmp);
                }
            }
            freeTonieInfo(&tafInfo);

            cJSON *jsonEntry = cJSON_CreateObject();
            cJSON_AddStringToObject(jsonEntry, "name", entry.name);
            cJSON_AddStringToObject(jsonEntry, "date", dateString);
            cJSON_AddNumberToObject(jsonEntry, "size", entry.size);
            cJSON_AddBoolToObject(jsonEntry, "isDirectory", (entry.attributes & FS_FILE_ATTR_DIRECTORY));
            cJSON_AddStringToObject(jsonEntry, "desc", desc);

            cJSON_AddItemToArray(jsonArray, jsonEntry);

            pos++;
        }

        jsonString = cJSON_PrintUnformatted(json);
        cJSON_Delete(json);
    } while (0);

    httpInitResponseHeader(connection);
    connection->response.contentType = "text/json";
    connection->response.contentLength = osStrlen(jsonString);

    return httpWriteResponse(connection, jsonString, connection->response.contentLength, true);
}

error_t handleApiStats(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *jsonArray = cJSON_AddArrayToObject(json, "stats");
    int pos = 0;

    while (true)
    {
        stat_t *stat = stats_get(pos);

        if (!stat)
        {
            break;
        }
        cJSON *jsonEntry = cJSON_CreateObject();
        cJSON_AddStringToObject(jsonEntry, "ID", stat->name);
        cJSON_AddStringToObject(jsonEntry, "description", stat->description);
        cJSON_AddNumberToObject(jsonEntry, "value", stat->value);
        cJSON_AddItemToArray(jsonArray, jsonEntry);

        pos++;
    }

    char *jsonString = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    httpInitResponseHeader(connection);
    connection->response.contentType = "text/json";
    connection->response.contentLength = osStrlen(jsonString);

    return httpWriteResponse(connection, jsonString, connection->response.contentLength, true);
}

FsFile *multipartStart(const char *rootPath, const char *filename, char *message, size_t message_max)
{
    // Ensure filename does not contain any directory separators
    if (strchr(filename, '\\') || strchr(filename, '/'))
    {
        osSnprintf(message, message_max, "Filename '%s' contains directory separators!", filename);
        TRACE_ERROR("Filename '%s' contains directory separators!\r\n", filename);
        return NULL;
    }

    char fullPath[1024]; // or a sufficiently large size for your paths
    osSnprintf(fullPath, sizeof(fullPath), "%s/%s", rootPath, filename);

    if (fsFileExists(fullPath))
    {
        TRACE_INFO("Filename '%s' already exists, overwriting\r\n", filename);
    }
    FsFile *file = fsOpenFile(fullPath, FS_FILE_MODE_WRITE | FS_FILE_MODE_CREATE | FS_FILE_MODE_TRUNC);

    return file;
}

error_t multipartAdd(FsFile *file, uint8_t *data, size_t length)
{
    if (fsWriteFile(file, data, length) != NO_ERROR)
    {
        return ERROR_FAILURE;
    }

    return NO_ERROR;
}

void multipartEnd(FsFile *file)
{
    if (!file)
    {
        return;
    }
    fsCloseFile(file);
}

int find_string(const void *haystack, size_t haystack_len, size_t haystack_start, const char *str)
{
    size_t str_len = osStrlen(str);

    if (str_len > haystack_len)
    {
        return -1;
    }

    for (size_t i = haystack_start; i <= haystack_len - str_len; i++)
    {
        if (osMemcmp((uint8_t *)haystack + i, str, str_len) == 0)
        {
            return i;
        }
    }

    return -1;
}

error_t parse_multipart_content(HttpConnection *connection, const char *rootPath, char *message, size_t message_max, const char *overlay, void (*fileCertUploaded)(const char *, const char *))
{
    char buffer[2 * BUFFER_SIZE];
    char filename[256];
    FsFile *file = NULL;
    eMultipartState state = PARSE_HEADER;
    char *boundary = connection->request.boundary;

    size_t leftover = 0;
    bool fetch = true;
    int save_start = 0;
    int save_end = 0;
    int payload_size = 0;

    do
    {
        if (!payload_size)
        {
            fetch = true;
        }

        payload_size = leftover;
        if (fetch)
        {
            size_t packet_size;
            error_t error = httpReceive(connection, &buffer[leftover], DATA_SIZE - leftover, &packet_size, SOCKET_FLAG_DONT_WAIT);
            if (error != NO_ERROR)
            {
                TRACE_ERROR("httpReceive failed\r\n");
                return error;
            }

            save_start = 0;
            payload_size = leftover + packet_size;
            save_end = payload_size;

            fetch = false;
        }

        switch (state)
        {
        case PARSE_HEADER:
        {
            /* when the payload starts with boundary, its a valid payload */
            int boundary_start = find_string(buffer, payload_size, 0, boundary);
            if (boundary_start < 0)
            {
                /* if not, check for newlines that preceed */
                int data_end = find_string(buffer, payload_size, 0, "\r\n");
                if (data_end >= 0)
                {
                    /* when found, start from there, after the newline */
                    save_start = data_end + 2;
                    save_end = payload_size;
                }
                else
                {
                    /* if not, drop most of the buffer, keeping the last few bytes */
                    save_start = (payload_size > SAVE_SIZE) ? payload_size - SAVE_SIZE : 0;
                    save_end = payload_size;
                }
            }
            else
            {
                save_start = boundary_start + osStrlen(boundary);
                save_end = payload_size;
                state = PARSE_FILENAME;
            }
            break;
        }

        case PARSE_FILENAME:
        {
            if (payload_size < 3)
            {
                save_start = 0;
                save_end = payload_size;
                fetch = true;
                break;
            }

            /* when the payload starts with --, then leave */
            if (!osMemcmp(buffer, "--", 2))
            {
                TRACE_DEBUG("Received multipart end\r\n");
                return NO_ERROR;
            }
            if (osMemcmp(buffer, "\r\n", 2))
            {
                TRACE_DEBUG("No valid multipart\r\n");
                return NO_ERROR;
            }

            int fn_start = find_string(buffer, payload_size, 0, "filename=\"");
            if (fn_start < 0)
            {
                TRACE_ERROR("No filename found\r\n");
                save_start = 0;
                save_end = payload_size;
                fetch = true;
                break;
            }

            fn_start += osStrlen("filename=\"");
            int fn_end = find_string(buffer, payload_size, fn_start, "\"\r\n");
            if (fn_end < 0)
            {
                TRACE_DEBUG("No filename end found\r\n");
                save_start = 0;
                save_end = payload_size;
                fetch = true;
                break;
            }

            save_start = find_string(buffer, payload_size, fn_end, "\r\n\r\n");
            if (save_start < 0)
            {
                TRACE_DEBUG("no newlines found, reading next block\r\n");
                save_start = 0;
                save_end = payload_size;
                fetch = true;
                break;
            }

            int inLen = fn_end - fn_start;
            int len = (inLen < sizeof(filename)) ? inLen : (sizeof(filename) - 1);
            TRACE_INFO("FN length %d %d %d %d\r\n", inLen, len, fn_start, fn_end);
            osStrncpy(filename, &buffer[fn_start], len);
            filename[len] = '\0';

            file = multipartStart(rootPath, filename, message, message_max);
            if (file == NULL)
            {
                TRACE_ERROR("Failed to open file\r\n");
                return ERROR_INVALID_PATH;
            }

            state = PARSE_BODY;
            save_start += 4;
            save_end = payload_size;
            break;
        }

        case PARSE_BODY:
        {
            if (!payload_size)
            {
                fetch = true;
                break;
            }

            int data_end = find_string(buffer, payload_size, 0, boundary);

            if (data_end >= 0)
            {
                /* We've found a boundary, let's finish up the current file, but skip the "--\r\n" */
                if (multipartAdd(file, (uint8_t *)buffer, data_end - 4) != NO_ERROR)
                {
                    TRACE_ERROR("Failed to open file\r\n");
                    return ERROR_FAILURE;
                }
                multipartEnd(file);
                file = NULL;

                TRACE_INFO("Received file '%s'\r\n", filename);
                if (fileCertUploaded)
                {
                    fileCertUploaded(filename, overlay);
                }

                /* Prepare for next file */
                state = PARSE_HEADER;
                save_start = data_end;
                save_end = payload_size;
            }
            else
            {
                /* there is no full bounday end in this block, save the end and fetch next */
                fetch = true;

                if (payload_size <= SAVE_SIZE)
                {
                    save_start = 0;
                    save_end = payload_size;
                }
                else
                {
                    save_start = payload_size - SAVE_SIZE;
                    save_end = payload_size;
                    multipartAdd(file, (uint8_t *)buffer, payload_size - SAVE_SIZE);
                }
            }
        }
        break;
        }

        leftover = save_end - save_start;
        if (leftover > 0)
        {
            memmove(buffer, &buffer[save_start], leftover);
        }
    } while (payload_size > 0);

    return NO_ERROR;
}

void fileCertUploaded(const char *filename, const char *overlay)
{
    /* rootpath must be valid, which is ensured by upload handler */
    const char *rootPath = settings_get_string_ovl("core.certdir", overlay);

    char *path = osAllocMem(osStrlen(rootPath) + osStrlen(filename) + 3);
    osSprintf(path, "%s/%s", rootPath, filename);

    if (!osStrcasecmp(filename, "ca.der"))
    {
        TRACE_INFO("Set ca.der to %s\r\n", path);
        settings_set_string_ovl("core.client_cert.file.ca", path, overlay);
    }
    else if (!osStrcasecmp(filename, "client.der"))
    {
        TRACE_INFO("Set client.der to %s\r\n", path);
        settings_set_string_ovl("core.client_cert.file.crt", path, overlay);
    }
    else if (!osStrcasecmp(filename, "private.der"))
    {
        TRACE_INFO("Set private.der to %s\r\n", path);
        settings_set_string_ovl("core.client_cert.file.key", path, overlay);
    }
    else
    {
        TRACE_INFO("Unknown file type %s\r\n", filename);
    }

    osFreeMem(path);
}

error_t handleApiUploadCert(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    uint_t statusCode = 500;
    char message[128];
    char overlay[16];
    osStrcpy(overlay, "");
    if (queryGet(queryString, "overlay", overlay, sizeof(overlay)))
    {
        TRACE_INFO("got overlay '%s'\r\n", overlay);
    }
    const char *rootPath = settings_get_string_ovl("core.certdir", overlay);

    if (rootPath == NULL || !fsDirExists(rootPath))
    {
        statusCode = 500;
        osSnprintf(message, sizeof(message), "core.certdir not set to a valid path");
        TRACE_ERROR("core.certdir not set to a valid path\r\n");
    }
    else
    {
        switch (parse_multipart_content(connection, rootPath, message, sizeof(message), (const char *)overlay, &fileCertUploaded))
        {
        case NO_ERROR:
            statusCode = 200;
            osSnprintf(message, sizeof(message), "OK");
            break;
        default:
            statusCode = 500;
            break;
        }
    }

    httpPrepareHeader(connection, "text/plain; charset=utf-8", osStrlen(message));
    connection->response.statusCode = statusCode;

    return httpWriteResponseString(connection, message, false);
}

void fileUploaded(const char *filename, const char *overlay)
{
    TRACE_INFO("Received new file '%s'\r\n", filename);
}

void sanitizePath(char *path, bool isDir)
{
    size_t i, j;
    bool slash = false;

    /* Merge all double (or more) slashes // */
    for (i = 0, j = 0; path[i]; ++i)
    {
        if (path[i] == '/')
        {
            if (slash)
                continue;
            slash = true;
        }
        else
        {
            slash = false;
        }
        path[j++] = path[i];
    }

    /* Make sure the path doesn't end with a '/' unless it's the root directory. */
    if (j > 1 && path[j - 1] == '/')
        j--;

    /* Null terminate the sanitized path */
    path[j] = '\0';

    /* If path doesn't start with '/', shift right and add '/' */
    if (path[0] != '/')
    {
        memmove(&path[1], &path[0], j + 1); // Shift right
        path[0] = '/';                      // Add '/' at the beginning
        j++;
    }

    /* If path doesn't end with '/', add '/' at the end */
    if (isDir)
    {
        if (path[j - 1] != '/')
        {
            path[j] = '/';      // Add '/' at the end
            path[j + 1] = '\0'; // Null terminate
        }
    }
}

error_t handleApiFileUpload(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    char overlay[16];
    osStrcpy(overlay, "");
    if (queryGet(queryString, "overlay", overlay, sizeof(overlay)))
    {
        TRACE_INFO("got overlay '%s'\r\n", overlay);
    }
    const char *rootPath = settings_get_string_ovl("internal.contentdirfull", overlay);

    if (rootPath == NULL || !fsDirExists(rootPath))
    {
        TRACE_ERROR("internal.contentdirfull not set to a valid path\r\n");
        return ERROR_FAILURE;
    }

    char path[128];

    if (!queryGet(queryString, "path", path, sizeof(path)))
    {
        osStrcpy(path, "/");
    }

    sanitizePath(path, true);

    char pathAbsolute[256];
    osSnprintf(pathAbsolute, sizeof(pathAbsolute), "%s/%s", rootPath, path);
    pathAbsolute[sizeof(pathAbsolute) - 1] = 0;

    uint_t statusCode = 500;
    char message[256];

    osSnprintf(message, sizeof(message), "OK");

    if (!fsDirExists(pathAbsolute))
    {
        statusCode = 500;
        osSnprintf(message, sizeof(message), "invalid path: '%s'", path);
        TRACE_ERROR("invalid path: '%s' -> '%s'\r\n", path, pathAbsolute);
    }
    else
    {
        switch (parse_multipart_content(connection, pathAbsolute, message, sizeof(message), (const char *)overlay, &fileUploaded))
        {
        case NO_ERROR:
            statusCode = 200;
            break;
        default:
            statusCode = 500;
            break;
        }
    }

    httpPrepareHeader(connection, "text/plain; charset=utf-8", osStrlen(message));
    connection->response.statusCode = statusCode;

    return httpWriteResponseString(connection, message, false);
}

error_t handleApiDirectoryCreate(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    char overlay[16];
    osStrcpy(overlay, "");
    if (queryGet(queryString, "overlay", overlay, sizeof(overlay)))
    {
        TRACE_INFO("got overlay '%s'\r\n", overlay);
    }
    const char *rootPath = settings_get_string_ovl("internal.contentdirfull", overlay);

    if (rootPath == NULL || !fsDirExists(rootPath))
    {
        TRACE_ERROR("internal.contentdirfull not set to a valid path\r\n");
        return ERROR_FAILURE;
    }
    char path[256];
    size_t size = 0;

    error_t error = httpReceive(connection, &path, sizeof(path), &size, 0x00);
    if (error != NO_ERROR)
    {
        TRACE_ERROR("httpReceive failed!");
        return error;
    }
    path[size] = 0;

    TRACE_INFO("Creating directory: '%s'\r\n", path);

    sanitizePath(path, true);

    char pathAbsolute[256 + 2];
    osSnprintf(pathAbsolute, sizeof(pathAbsolute), "%s/%s", rootPath, path);
    pathAbsolute[sizeof(pathAbsolute) - 1] = 0;

    uint_t statusCode = 200;
    char message[256 + 64];

    osSnprintf(message, sizeof(message), "OK");

    error_t err = fsCreateDir(pathAbsolute);

    if (err != NO_ERROR)
    {
        statusCode = 500;
        osSnprintf(message, sizeof(message), "Error creating directory '%s', error %d", path, err);
        TRACE_ERROR("Error creating directory '%s' -> '%s', error %d\r\n", path, pathAbsolute, err);
    }
    httpPrepareHeader(connection, "text/plain; charset=utf-8", osStrlen(message));
    connection->response.statusCode = statusCode;

    return httpWriteResponseString(connection, message, false);
}

error_t handleApiDirectoryDelete(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    char overlay[16];
    osStrcpy(overlay, "");
    if (queryGet(queryString, "overlay", overlay, sizeof(overlay)))
    {
        TRACE_INFO("got overlay '%s'\r\n", overlay);
    }
    const char *rootPath = settings_get_string_ovl("internal.contentdirfull", overlay);

    if (rootPath == NULL || !fsDirExists(rootPath))
    {
        TRACE_ERROR("internal.contentdirfull not set to a valid path\r\n");
        return ERROR_FAILURE;
    }
    char path[256];
    size_t size = 0;

    error_t error = httpReceive(connection, &path, sizeof(path), &size, 0x00);
    if (error != NO_ERROR)
    {
        TRACE_ERROR("httpReceive failed!");
        return error;
    }
    path[size] = 0;

    TRACE_INFO("Deleting directory: '%s'\r\n", path);

    sanitizePath(path, true);

    char pathAbsolute[256 + 2];
    osSnprintf(pathAbsolute, sizeof(pathAbsolute), "%s/%s", rootPath, path);
    pathAbsolute[sizeof(pathAbsolute) - 1] = 0;

    uint_t statusCode = 200;
    char message[256 + 64];

    osSnprintf(message, sizeof(message), "OK");

    error_t err = fsRemoveDir(pathAbsolute);

    if (err != NO_ERROR)
    {
        statusCode = 500;
        osSnprintf(message, sizeof(message), "Error deleting directory '%s', error %d", path, err);
        TRACE_ERROR("Error deleting directory '%s' -> '%s', error %d\r\n", path, pathAbsolute, err);
    }
    httpPrepareHeader(connection, "text/plain; charset=utf-8", osStrlen(message));
    connection->response.statusCode = statusCode;

    return httpWriteResponseString(connection, message, false);
}

error_t handleApiFileDelete(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx)
{
    char overlay[16];
    osStrcpy(overlay, "");
    if (queryGet(queryString, "overlay", overlay, sizeof(overlay)))
    {
        TRACE_INFO("got overlay '%s'\r\n", overlay);
    }
    const char *rootPath = settings_get_string_ovl("internal.contentdirfull", overlay);

    if (rootPath == NULL || !fsDirExists(rootPath))
    {
        TRACE_ERROR("internal.contentdirfull not set to a valid path\r\n");
        return ERROR_FAILURE;
    }
    char path[256];
    size_t size = 0;

    error_t error = httpReceive(connection, &path, sizeof(path), &size, 0x00);
    if (error != NO_ERROR)
    {
        TRACE_ERROR("httpReceive failed!");
        return error;
    }
    path[size] = 0;

    TRACE_INFO("Deleting file: '%s'\r\n", path);

    sanitizePath(path, false);

    char pathAbsolute[256 + 2];
    osSnprintf(pathAbsolute, sizeof(pathAbsolute), "%s/%s", rootPath, path);
    pathAbsolute[sizeof(pathAbsolute) - 1] = 0;

    uint_t statusCode = 200;
    char message[256 + 64];

    osSnprintf(message, sizeof(message), "OK");

    error_t err = fsDeleteFile(pathAbsolute);

    if (err != NO_ERROR)
    {
        statusCode = 500;
        osSnprintf(message, sizeof(message), "Error deleting file '%s', error %d", path, err);
        TRACE_ERROR("Error deleting file '%s' -> '%s', error %d\r\n", path, pathAbsolute, err);
    }
    httpPrepareHeader(connection, "text/plain; charset=utf-8", osStrlen(message));
    connection->response.statusCode = statusCode;

    return httpWriteResponseString(connection, message, false);
}
