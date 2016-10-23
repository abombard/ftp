#include "user.h"
#include "listen_socket.h"

#include <string.h>			/* strcpy() */
#include <stdio.h>			/* perror() */

extern bool	users__initialize (t_users *users, char *host, int port)
{
	struct addrinfo	addr_info;
	SOCKET					sock;
	unsigned int		i;

	INIT_LIST_HEAD (&users->user_free);
	i = 0;
	while (i < sizeof (users->user) / sizeof (users->user[0]))
	{
		list_push_back (&users->user[i].list, &users->user_free);
		i++;
	}
	ASSERT (listen_socket_init (host, port, &addr_info, &sock));
	if (!user__new (users, sock, addr_info.ai_addrlen, addr_info.ai_addr, &users->listen))
	{
		LOG_ERROR ("user__new failed");
		socket_close (sock);
		return ( false );
	}
	return ( true );
}

extern void	users__finalize (t_users *users)
{
	if (list_size (&users->user_free) != sizeof (users->user) / sizeof (users->user[0]))
		LOG_WARNING ("Some users may still be actif");
}

static bool	getsocketinfo (const struct sockaddr *ai_addr,
													 const int ai_addrlen,
													 char hbuf[ IP_ADDRESS_SIZE_MAX ],
													 char sbuf[ SERVICE_ID_SIZE_MAX ])
{
	int		error_number;

	error_number = getnameinfo (ai_addr, ai_addrlen,
														  hbuf, IP_ADDRESS_SIZE_MAX,
														  sbuf, SERVICE_ID_SIZE_MAX,
														  NI_NUMERICHOST | NI_NUMERICSERV);
	if (error_number)
	{
		LOG_ERROR ("getnameinfo:%s", gai_strerror (error_number));
		return ( false );
	}
	return ( true );
}

static bool	internal_user__new (const SOCKET sock,
															 const char *hbuf,
															 const char *sbuf,
															 t_user *user)
{
	user->sock = sock;
	user->sock_cmd = sock;
	user->sock_data = -1;
	strcpy (user->ip_address, hbuf);
	strcpy (user->service_id, sbuf);
	user->name.size = 0;
	if (!getcwd (user->dir_home.bytes, sizeof (user->dir_home.bytes)))
	{
		perror ("getcwd");
		return ( false );
	}
	user->dir_home.size = strlen (user->dir_home.bytes);
	strcpy (user->dir_cur.bytes, user->dir_home.bytes);
	user->dir_cur.size = user->dir_home.size;
	msg_init (&user->msg);
	user->actif = true;
	return ( true );
}

extern bool	user__new (t_users *users,
											const SOCKET sock,
											const int ai_addrlen,
											const struct sockaddr *ai_addr,
											t_user **out_user)
{
	t_user		*user;
	t_list		*node;
	char			hbuf[ IP_ADDRESS_SIZE_MAX ];
	char			sbuf[ SERVICE_ID_SIZE_MAX ];

	*out_user = NULL;
	if (sock < 0 || sock > FD_SETSIZE)
	{
		LOG_ERROR ("socket %d FD_SETSIZE %d", sock, FD_SETSIZE);
		return ( false );
	}
	if (list_is_empty (&users->user_free))
	{
		LOG_ERROR ("CONNECTION_COUNT_MAX %d", CONNECTION_COUNT_MAX);
		return ( false );
	}
	ASSERT (getsocketinfo (ai_addr, ai_addrlen, hbuf, sbuf));
	node = list_nth (&users->user_free, 1);
	list_del (node);
	user = CONTAINER_OF (node, t_user, list);
	ASSERT (internal_user__new (sock, hbuf, sbuf, user));
	*out_user = user;
	LOG_DEBUG ("socket %d ip_address {%s} service_id {%s}", user->sock, user->ip_address, user->service_id);
	return ( true );
}

extern void		user__del (t_users *users, t_user *user)
{
	if (user->sock_cmd != -1)
		socket_close (user->sock_cmd);
	if (user->sock_data != -1)
		socket_close (user->sock_data);
	list_move (&user->list, &users->user_free);
}
