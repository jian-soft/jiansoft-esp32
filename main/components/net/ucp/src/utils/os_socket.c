//借鉴 https://swtch.com/libtask/net.c

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "os_interfaces.h"

int socket_server(int istcp, int port)
{
    int ret, proto, fd;
    struct sockaddr_in sa = {0};

    proto = istcp ? SOCK_STREAM : SOCK_DGRAM;
    fd = socket(AF_INET, proto, 0);
    if (fd < 0) {
        os_log("server socket failed:%s\n", strerror(errno));
        return -1;
    }

    /* set reuse flag */
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = htons(port);

    ret = bind(fd, (struct sockaddr*)&sa, sizeof(sa));
    if (ret < 0) {
        close(fd);
        os_log("bind failed:%s\n", strerror(errno));
        return -1;
    }

    if (istcp) {
        listen(fd, 3);
    }

    return fd;
}

/*
@server: server ip
@return: socket fd, -1 on error
*/
int socket_client(int istcp, char *server, int port)
{
    int proto, fd, ret;
    struct sockaddr_in sa;

    struct in_addr ipv4_addr;
    ret = inet_pton(AF_INET, server, &ipv4_addr);
    if (1 != ret) {
        os_log("server ip is invalid: %s\n", server);
        return -1;
    }

    proto = istcp ? SOCK_STREAM : SOCK_DGRAM;
    fd = socket(AF_INET, proto, 0);
    if (fd < 0) {
        os_log("client socket failed:%s\n", strerror(errno));
        return -1;
    }

    /* start connecting */
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = ipv4_addr.s_addr;
    sa.sin_port = htons(port);

    ret = connect(fd, (struct sockaddr*)&sa, sizeof(sa));
    if (ret < 0) {
        os_log("connect failed:%s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int socket_tcp_accept(int fd)
{
    int cfd;
    struct sockaddr_in sa;
    socklen_t len = sizeof(sa);

    cfd = accept(fd, (struct sockaddr*)&sa, &len);
    if (cfd < 0) {
        os_log("accept failed:%s\n", strerror(errno));
        return -1;
    }

    char server_ip[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, &sa.sin_addr, server_ip, INET_ADDRSTRLEN) == NULL) {
        os_log("inet_ntop error:%s\n", strerror(errno));
    }
    os_log("tcp client connected: %s:%d\n", server_ip, ntohs(sa.sin_port));

    return cfd;
}

int udp_accept(int sfd)
{
    int ret;
    char buf[16];
    struct sockaddr_in cliaddr;
    socklen_t addr_len = sizeof(cliaddr);

    ret = recvfrom(sfd, buf, sizeof(buf), MSG_PEEK, (struct sockaddr *)&cliaddr, &addr_len);
    if (ret <= 0) {
        os_log("recvfrom failed:%s\n", strerror(errno));
        return -1;
    }

    ret = connect(sfd, (const struct sockaddr *)&cliaddr, addr_len);
    if (ret < 0) {
        os_log("connect error:%s\n", strerror(errno));
        return -1;
    }

    char server_ip[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, &cliaddr.sin_addr, server_ip, INET_ADDRSTRLEN) == NULL) {
        os_log("inet_ntop error:%s\n", strerror(errno));
    }
    os_log("udp client connected: %s:%d\n", server_ip, ntohs(cliaddr.sin_port));

    return sfd;
}

int socket_set_rcvtimeo(int sfd, int seconds)
{
    int ret;
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;

    ret = setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    if (0 != ret) {
        os_log("setsockopt fail:%s\n", strerror(errno));
    }
    return ret;
}