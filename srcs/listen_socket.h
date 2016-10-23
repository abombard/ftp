#ifndef LISTEN_SOCKET_H
# define LISTEN_SOCKET_H

#include "sys_types.h"
#include "log.h"
#include "sock.h"
#include "user.h"

bool	listen_socket_init (const char *host, const int listen_port, struct addrinfo *out_info, SOCKET *out_sock);

#endif
