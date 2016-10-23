#include "sock.h"
#include <stdio.h>	/* perror() */

extern bool socket_open (const int ai_family,
												 const int ai_socktype,
												 const int ai_protocol,
												 SOCKET *out_sock)
{
	SOCKET	sock;

	*out_sock = INVALID_SOCKET;
	sock = socket (ai_family, ai_socktype, ai_protocol);
	if (sock == INVALID_SOCKET)
	{
		perror ("socket");
		return ( false );
	}
	*out_sock = sock;
	return ( true );
}

extern void socket_close (SOCKET sock)
{
	if (sock >= 0)
	{
		if (shutdown (sock, SHUT_RDWR))
		{
			perror ("shutdown");
		}
		if (close (sock))
		{
			perror ("close");
		}
	}
}

