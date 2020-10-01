/* $Id: socket.c 1.1 1995/01/01 07:11:14 cthuang Exp $
 *
 * This module has been modified by Radim Kolar for OS/2 emx.
 * This module has been modified by ucsk in Cygwin 3.1.2-1.
 */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
   module:         socket.c
   program:        popclient
   SCCS ID:        @(#)socket.c    1.5  4/1/94
   programmer:     Virginia Tech Computing Center
   compiler:       DEC RISC C compiler (Ultrix 4.1)
   environment:    DEC Ultrix 4.3
   description:    UNIX sockets code.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

static int Socket(const char *host, int client_port)
{
    // Ethernet address structure
    struct sockaddr_in addr_host;
    memset(&addr_host, 0 , sizeof(addr_host));
    addr_host.sin_family = AF_INET;
    addr_host.sin_port = htons(client_port);

    // Perform domain name resolution or IP address translation.
    unsigned long sin_addr = inet_addr(host);
    if(sin_addr != INADDR_NONE) {
        memcpy(&addr_host.sin_addr, &sin_addr, sizeof(sin_addr));
    } else {
        struct hostent *host_entry = gethostbyname(host);
        if(host_entry == NULL) {
            return -1;
        }
        memcpy(&addr_host.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
    }

    // Create socket for TCP stream IPv4 domain and connect host.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0) {
        return sock_fd;
    }
    if(connect(sock_fd, (struct sockaddr *)&addr_host, sizeof(addr_host)) < 0) {
        close(sock_fd);
        return -1;
    }
    return sock_fd;
}
