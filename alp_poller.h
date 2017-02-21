#ifndef ALP_POLLER_H
#define ALP_POLLER_H

void epoll_loop(int listen_fd);
int tcp_listen(int port);

#endif
