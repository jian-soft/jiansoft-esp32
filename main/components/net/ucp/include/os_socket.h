#ifndef _OS_SOCKET_H_
#define _OS_SOCKET_H_

int socket_server(int istcp, int port);
int socket_client(int istcp, char *server, int port);
int socket_tcp_accept(int fd);
int udp_accept(int sfd);
int socket_set_rcvtimeo(int sfd, int seconds);

#endif /* _OS_SOCKET_H_ */


