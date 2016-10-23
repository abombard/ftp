#include "listen_socket.h"

#include <string.h>		/* memset() */
#include <stdio.h>		/* perror() */

static bool		addrinfo_alloc_init (const char *host,
														 			 struct addrinfo **out_result)
{
	struct addrinfo	*result;
	struct addrinfo	hints;
	int							error_number;

	*out_result = NULL;
	memset (&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;				/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;		/* Stream socket */
	hints.ai_flags = AI_PASSIVE;				/* For wildcard IP address */
	hints.ai_protocol = 0;							/* Any protocol */
	error_number = getaddrinfo (host, "ftp", &hints, &result);
	if (error_number)
		FATAL ("getaddrinfo: %s", gai_strerror (error_number));
	*out_result = result;
	return ( true );
}

#define SOCKET_BACKLOG_COUNT_MAX	1
static bool listen_socket_bind (const SOCKET listen_socket,
																const int listen_port)
{
	struct sockaddr_in	sockaddr;
	int									yes;

	yes = 1;
	if (setsockopt (listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (yes)) == -1)
	{
		perror ("setsockopt");
		return ( false );
	}
	memset (&sockaddr, 0, sizeof (sockaddr));
	sockaddr.sin_port = htons (listen_port);
	sockaddr.sin_family = AF_INET;
	if (bind (listen_socket, (struct sockaddr *)&sockaddr, sizeof (sockaddr)) == -1)
	{
		perror ("bind");
		return ( false );
	}
	LOG_DEBUG ("accepting connections on port %d\n", listen_port);
	if (listen (listen_socket, SOCKET_BACKLOG_COUNT_MAX))
	{
		perror ("listen");
		return ( false );
	}
	return ( true );
}

bool			listen_socket_init (const char *host, const int listen_port, struct addrinfo *out_info, SOCKET *out_sock)
{
	struct addrinfo 		*result, *rp;
	SOCKET							listen_socket;
	t_user							*new;
	int									error_number;

	memset (out_info, 0, sizeof (*out_info));
	*out_sock = -1;
	if (! addrinfo_alloc_init (host, &result))
		FATAL ("addrinfo_alloc_init failed");
	rp = result;
	while (rp != NULL)
	{
		if (socket_open (rp->ai_family,
										 rp->ai_socktype,
										 rp->ai_protocol,
										 &listen_socket))
		{
			if (listen_socket_bind (listen_socket, listen_port))
				break ;
			socket_close (listen_socket);
		}
		rp = rp->ai_next;
	}
	if (rp == NULL)
	{
		LOG_ERROR ("Could not set up listen_socket");
		freeaddrinfo (result);
		return ( false );
	}
	*out_info = *rp;
	*out_sock = listen_socket;
	freeaddrinfo (result);
	return ( true );
}

