#include <string.h>
#include <arpa/inet.h>
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
        if(host_entry == NULL) { return -1; }
        memcpy(&addr_host.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
    }

    // Create socket for TCP stream IPv4 domain and connect host.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0) {
        return sock_fd;
    }
    if(connect(sock_fd, (struct sockaddr *)&addr_host, sizeof(addr_host)) < 0) {
        return -1;
    }
    return sock_fd;
}