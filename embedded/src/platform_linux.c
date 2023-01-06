
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

#include "tls.h"
#include "core/net.h"
#include "core/ethernet.h"
#include "core/ip.h"
#include "core/tcp.h"
#include "debug.h"

void platform_init()
{
}

void platform_deinit()
{
}

Socket *socketOpen(uint_t type, uint_t protocol)
{
    // printf("socketOpen %d %d\n", type, protocol);
    int ret = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    // printf("socketOpen: %d\n", ret);
    return (Socket *)(intptr_t)ret;
}

error_t socketSetTimeout(Socket *socket, systime_t timeout)
{
    // printf("socketSetTimeout %d\n", timeout);
    intptr_t sock = (intptr_t)socket;
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv,
                   sizeof(tv)) < 0)
        perror("setsockopt failed\n");

    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv,
                   sizeof(tv)) < 0)
        perror("setsockopt failed\n");

    return NO_ERROR;
}

void socketClose(Socket *socket)
{
    // printf("socketClose\n");
    intptr_t sock = (intptr_t)socket;
    close(sock);
}

error_t socketShutdown(Socket *socket, uint_t how)
{
    intptr_t sock = (intptr_t)socket;
    shutdown(sock, how);
    return NO_ERROR;
}

error_t socketSetInterface(Socket *socket, NetInterface *interface)
{
    // printf("socketSetInterface\n");
    return NO_ERROR;
}

error_t socketConnect(Socket *socket, const IpAddr *remoteIpAddr,
                      uint16_t remotePort)
{
    intptr_t sock = (intptr_t)socket;
    struct sockaddr addr;
    memset(&addr, 0, sizeof(addr));

    struct sockaddr_in *sa = (struct sockaddr_in *)&addr;
    sa->sin_family = AF_INET;
    sa->sin_port = htons(remotePort);
    sa->sin_addr.s_addr = remoteIpAddr->ipv4Addr;
    TRACE_INFO("socketConnect %s:%d\n", inet_ntoa(sa->sin_addr), remotePort);

    int ret = connect(sock, &addr, sizeof(addr));

    printf("socketConnect done %d %s\n", ret, strerror(errno));

    return ret != -1 ? NO_ERROR : ERROR_ACCESS_DENIED;
}

error_t socketSend(Socket *socket, const void *data, size_t length,
                   size_t *written, uint_t flags)
{
    // printf("socketSend\n");
    int_t n;
    error_t error;

    // Send data
    n = send((intptr_t)socket, data, length, 0);

    // Check return value
    if (n > 0)
    {
        // Total number of data that have been written
        *written = n;
        // Successful write operation
        error = NO_ERROR;
    }
    else
    {
        // Timeout error?
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            error = ERROR_TIMEOUT;
        else
            error = ERROR_WRITE_FAILED;
    }

    // printf("socketSend done\n");
    //  Return status code
    return error;
}

error_t socketReceive(Socket *socket, void *data,
                      size_t size, size_t *received, uint_t flags)
{
    // printf("socketReceive\n");
    int_t n;
    error_t error;

    // Receive data
    n = recv((intptr_t)socket, data, size, 0);

    // Check return value
    if (n > 0)
    {
        // Total number of data that have been received
        *received = n;
        // Successful write operation
        error = NO_ERROR;
    }
    else
    {
        // Timeout error?
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            error = ERROR_TIMEOUT;
        else
            error = ERROR_READ_FAILED;
    }

    // printf("socketReceive done\n");
    //  Return status code
    return error;
}
