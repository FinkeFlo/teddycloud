
#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "handler_rtnl.h"
#include "settings.h"
#include "stats.h"
#include "cloud_request.h"

#include "proto/toniebox.pb.rtnl.pb-c.h"

error_t handleRtnl(HttpConnection *connection, const char_t *uri, const char_t *queryString)
{
    char_t *data = connection->buffer;
    size_t size = connection->response.contentLength;

    if (Settings.rtnl.logRaw)
    {
        FsFile *file = fsOpenFile(Settings.rtnl.logRawFile, FS_FILE_MODE_WRITE | FS_FILE_MODE_CREATE);
        fsWriteFile(file, &data[connection->response.byteCount], size);
        fsCloseFile(file);
    }

    size_t pos = 0;
    while (size > 4 && pos < (size - 4))
    {
        data = &connection->buffer[pos];
        uint32_t protoLength = (uint32_t)((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
        char_t *protoData = &data[4];
        if (protoLength > (size - pos))
        {
            break;
        }
        if (protoLength == 0 || protoLength + 4 > HTTP_SERVER_BUFFER_SIZE - 1) // find apropiate size
        {
            pos++;
            continue;
        }
        TonieRtnlRPC *rpc = tonie_rtnl_rpc__unpack(NULL, protoLength, (const uint8_t *)protoData);
        pos += protoLength + 4;
        if (rpc && (rpc->log2 || rpc->log3))
        {
            TRACE_INFO("RTNL: \r\n");
            if (rpc->log2)
            {
                TRACE_INFO(" LOG2:\r\n");
                TRACE_INFO("  uptime=%" PRIu64 "\r\n", rpc->log2->uptime);
                TRACE_INFO("  sequence=%" PRIu32 "\r\n", rpc->log2->sequence);
                TRACE_INFO("  3=%" PRIu32 "\r\n", rpc->log2->field3);
                TRACE_INFO("  group=%" PRIu32 "\r\n", rpc->log2->function_group);
                TRACE_INFO("  function=%" PRIu32 "\r\n", rpc->log2->function);
                TRACE_INFO("  6=len(data)=%" PRIuSIZE ", data=", rpc->log2->field6.len);
                for (size_t i = 0; i < rpc->log2->field6.len; i++)
                {
                    TRACE_INFO_RESUME("%02X", rpc->log2->field6.data[i]);
                }
                TRACE_INFO_RESUME(", txt=%s\r\n", rpc->log2->field6.data);
                if (rpc->log2->has_field8)
                    TRACE_INFO("  8=%u\r\n", rpc->log2->field8);
                if (rpc->log2->has_field9)
                {
                    TRACE_INFO("  9=len(data)=%" PRIuSIZE ", data=", rpc->log2->field9.len);
                    for (size_t i = 0; i < rpc->log2->field9.len; i++)
                    {
                        TRACE_INFO_RESUME("%02X", rpc->log2->field9.data[i]);
                    }
                    TRACE_INFO_RESUME(", txt=%s\r\n", rpc->log2->field9.data);
                }
            }
            if (rpc->log3)
            {
                TRACE_INFO(" LOG3:\r\n");
                TRACE_INFO("  datetime=%" PRIu32 "\r\n", rpc->log3->datetime);
                TRACE_INFO("  2=%" PRIu32 "\r\n", rpc->log3->field2);
            }
        }
        tonie_rtnl_rpc__free_unpacked(rpc, NULL);
    }

    osMemcpy(connection->buffer, data, size - pos); // 1023-1024 may occur
    connection->response.byteCount = size - pos;

    return NO_ERROR;
}