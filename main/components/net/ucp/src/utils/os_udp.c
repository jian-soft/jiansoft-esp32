//UDP连接管理
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "os_socket.h"
#include "os_udp.h"
#include "os_interfaces.h"

typedef struct
{
    int sfd;
    int connstate;
    UDP_RX_CB_FUN rx_cb;
    UDP_CONNSTATE_CB connstate_cb;
} udp_obj_t;

udp_obj_t g_udp;

int udp_get_connstate()
{
    return g_udp.connstate;
}
void udp_set_connstate(int state)
{
    if (state == g_udp.connstate) {
        return;
    }

    g_udp.connstate = state;
    os_log("udp_set_connstate: %d\n", state);

    if (g_udp.connstate_cb) {
        g_udp.connstate_cb(state);
    }
}
void udp_register_rx_cb(UDP_RX_CB_FUN cb)
{
    g_udp.rx_cb = cb;
}
void udp_register_connstate_cb(UDP_CONNSTATE_CB cb)
{
    g_udp.connstate_cb = cb;
}
int os_udp_send(uint8_t *buffer, uint32_t len)
{
    return send(g_udp.sfd, buffer, len, 0);
}

/* 整个程序生命周期只创建一次，不会销毁，server角色创建此线程 */
void* udpserver_accept_and_recv_thread(void *arg)
{
    int ret, sfd;
    static uint8_t ll_rcv_buf[1500];

    while (1) {
        udp_set_connstate(0);
        g_udp.sfd = -1;

        g_udp.sfd = socket_server(0, UDP_SERVER_PORT);
        if (g_udp.sfd < 0) {
            os_log("FATAL: socket_server error\n");
            os_sleep(1);
            continue;
        }
        sfd = g_udp.sfd;
        os_log("socket_server: sfd:%d\n", sfd);

        ret = udp_accept(sfd);
        if (ret < 0) {
            os_log("FATAL: udp_accept return fail.\n");
            close(sfd);
            continue;
        }

        udp_set_connstate(1);
        socket_set_rcvtimeo(sfd, 2);

        while (1) {
            ret = recv(sfd, ll_rcv_buf, sizeof(ll_rcv_buf), 0);
            if (ret < 0) {
                if (udp_get_connstate() && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                    continue;
                }
                os_log("os_udp_recv error:%s\n", strerror(errno));
                break;
            }

            //处理客户端发来的握手和挥手消息
            if (8 == ret) {
                if (strncmp((char *)ll_rcv_buf, "ucp:sync", 8) == 0) {
                    os_log("server recv ucp:sync\n");
                    send(sfd, "ucp:ack", 7, 0);
                    os_log("server send ucp:ack\n");
                    continue;
                } else if (strncmp((char *)ll_rcv_buf, "ucp:leav", 8) == 0) {
                    os_log("server recv ucp:leav\n");
                    break;
                }
            }

            if (g_udp.rx_cb) {
                g_udp.rx_cb(ll_rcv_buf, ret);
            }
        }

        //异常处理: 关掉socket, 重新建立连接
        close(sfd);
    }

    return NULL;
}


/* 客户端握手 */
int udpclient_handshake(int sfd)
{
    int ret;
    char rcv_buf[16];

    for (int i = 0; i < 3; i++) {
        os_log("udpclient send ucp:sync\n");
        ret = send(sfd, "ucp:sync", 8, 0);
        if (ret < 0) {
            os_log("send ucp:sync error:%s\n", strerror(errno));
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ENOBUFS) {
                // 资源暂时不可用，稍后重试
                os_usleep(10000);
                continue;
            }
            return -1;
        }
        os_usleep(10000);
        ret = recv(sfd, rcv_buf, sizeof(rcv_buf), MSG_DONTWAIT);
        if (ret < 0) {
            os_log("recv ucp:ack error:%s\n", strerror(errno));
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                // 资源暂时不可用，稍后重试
                os_usleep(300000);
                continue;
            }
            return -1;
        }

        if (strncmp(rcv_buf, "ucp:ack", 7) == 0) {
            os_log("server respond ucp:ack\n");
            return 0; // 握手成功
        }
    }

    return -1; // 握手失败
}

int udpclient_leave(int sfd)
{
    send(sfd, "ucp:leav", 8, 0);
    os_log("udpclient send ucp:leav\n");
    os_usleep(10000);
    return 0;
}


/* 整个程序生命周期只创建一次，不会销毁, client角色创建此线程 */
void* udpclient_connect_and_recv_thread(void *arg)
{
    int sfd, ret;
    static uint8_t ll_rcv_buf[1500];
    udp_client_conn_thread_info_t *info = (udp_client_conn_thread_info_t *)arg;

    while (1) {
        udp_set_connstate(0);
        g_udp.sfd = -1;

        while (!(info->conn_thread_start)) {
            os_usleep(200*1000);  //连接线程被暂停
        }

        g_udp.sfd = socket_client(0, info->server_ip, UDP_SERVER_PORT);
        if (g_udp.sfd < 0) {
            os_log("Error: socket_client error\n");
            os_sleep(1);
            continue;
        }
        sfd = g_udp.sfd;
        os_log("socket_client: sfd:%d\n", sfd);

        ret = udpclient_handshake(sfd);
        if (ret < 0) {
            os_log("udpclient_handshake fail, retry later\n");
            close(sfd);
            os_sleep(1);
            continue;
        }

        udp_set_connstate(1);
        socket_set_rcvtimeo(sfd, 2);

        while (1) {
            ret = recv(sfd, ll_rcv_buf, sizeof(ll_rcv_buf), 0);
            if (!udp_get_connstate()) {
                udpclient_leave(sfd);
                break;
            }

            if (ret < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    continue;
                }

                os_log("udpclient recv error:%s\n", strerror(errno));
                break;
            }

            //recv call back
            if (g_udp.rx_cb) {
                g_udp.rx_cb(ll_rcv_buf, ret);
            }
        }

        //异常处理: 关掉socket, 重新建立连接
        close(sfd);
    }
}

