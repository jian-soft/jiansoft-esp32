#ifndef _OS_UDP_H_
#define _OS_UDP_H_
#include "os_interfaces.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define UDP_SERVER_PORT  9007

typedef void (*UDP_RX_CB_FUN)(uint8_t *buffer, uint32_t len);
typedef void (*UDP_CONNSTATE_CB)(int state);

typedef struct {
    char server_ip[16];
    char conn_thread_start;
} udp_client_conn_thread_info_t;

int udp_get_connstate();
void udp_set_connstate(int state);
void udp_register_rx_cb(UDP_RX_CB_FUN cb);
void udp_register_connstate_cb(UDP_CONNSTATE_CB cb);
int os_udp_send(uint8_t *buffer, uint32_t len);

void* udpclient_connect_and_recv_thread(void *arg);
void* udpserver_accept_and_recv_thread(void *arg);


#endif /* _OS_UDP_H_ */


