#include "user.h"
#include "listen_socket.h"

#include <string.h>			/* strcpy(), strncpy() */
#include <stdio.h>			/* perror() */

static t_users *users__context (void)
{
	static t_users  g_context = {
		.is_initialized = false
	};

	return ( &g_context );
}

extern bool	users__initialize (char *host, int port)
{
	t_users			*users;
	unsigned int	i;

	users = users__context ();
	INIT_LIST_HEAD (&users->user_free);
	i = 0;
	while (i < sizeof (users->user) / sizeof (users->user[0]))
	{
		list_push_back (&users->user[i].list, &users->user_free);
		i++;
	}
	return ( true );
}

extern t_user	*users__listen(char *host, int port)
{
	t_users			*users;
	struct addrinfo	addr_info;
	SOCKET			sock;

	users = users__context();
	ASSERT (listen_socket_init (host, port, &addr_info, &sock));
	if (!user__new (sock, addr_info.ai_addrlen, addr_info.ai_addr, &users->listen))
	{
		LOG_ERROR ("user__new failed");
		socket_close (sock);
		return ( NULL );
	}
	return ( users->listen );
}

extern void	users__finalize (void)
{
	t_users	*users;

	users = users__context();
	if (list_size (&users->user_free) != sizeof (users->user) / sizeof (users->user[0]))
		LOG_WARNING ("Some users may still be actif");
}

static bool	getsocketinfo (const struct sockaddr *ai_addr,
		const int ai_addrlen,
		char host[ NI_MAXHOST ],
		char serv[ NI_MAXSERV ])
{
	int		error_number;

	error_number = getnameinfo (ai_addr, ai_addrlen,
			host, NI_MAXHOST,
			serv, NI_MAXSERV,
			NI_NUMERICHOST | NI_NUMERICSERV);
	if (error_number)
	{
		LOG_ERROR ("getnameinfo:%s", gai_strerror (error_number));
		return ( false );
	}
	return ( true );
}

static bool	internal_user__new (const SOCKET sock,
		const char *host,
		const char *serv,
		t_user *user)
{
	user->sock = sock;
	user->sock_cmd = sock;
	user->sock_data = -1;
	strncpy (user->host, host, NI_MAXHOST);
	strncpy (user->serv, serv, NI_MAXSERV);
	user->name.size = 0;
	if (!getcwd (user->home.bytes, sizeof (user->home.bytes)))
	{
		perror ("getcwd");
		return ( false );
	}
	user->home.size = strlen (user->home.bytes);
	ASSERT (user->home.size < sizeof (user->home.bytes));
	strncpy (user->pwd.bytes, user->home.bytes, sizeof (user->home.bytes));
	user->pwd.size = user->home.size;
	msg_init (&user->msg);
	user->actif = true;
	return ( true );
}

extern bool	user__new (const SOCKET sock,
		const int ai_addrlen,
		const struct sockaddr *ai_addr,
		t_user **out_user)
{
	t_users			 *users;
	t_user		*user;
	t_list		*node;
	char		host[ NI_MAXHOST ];
	char		serv[ NI_MAXSERV ];

	*out_user = NULL;
	if (sock < 0 || sock > FD_SETSIZE)
	{
		LOG_ERROR ("socket %d FD_SETSIZE %d", sock, FD_SETSIZE);
		return ( false );
	}
	users = users__context();
	if (list_is_empty (&users->user_free))
	{
		LOG_ERROR ("CONNECTION_COUNT_MAX %d", CONNECTION_COUNT_MAX);
		return ( false );
	}
	ASSERT (getsocketinfo (ai_addr, ai_addrlen, host, serv));
	node = list_nth (&users->user_free, 1);
	list_del (node);
	user = CONTAINER_OF (node, t_user, list);
	ASSERT (internal_user__new (sock, host, serv, user));
	*out_user = user;
	LOG_DEBUG ("socket %d host {%s} serv {%s}", user->sock, user->host, user->serv);
	return ( true );
}

extern void		user__del (t_user *user)
{
	t_users	*users;

	users = users__context();
	fprintf (stderr, "%s\n", __func__);
	user__show(user);
	if (user->sock_cmd != -1)
		socket_close (user->sock_cmd);
	if (user->sock_data != -1)
		socket_close (user->sock_data);
	list_move (&user->list, &users->user_free);
}

void	user__show(t_user *user)
{
	fprintf (stderr, "User {%.*s}\nhost: %s\nserv: %s\nhome: %.*s\npwd: %.*s\n", (int)user->name.size, user->name.bytes, user->host, user->serv, (int)user->home.size, user->home.bytes, (int)user->pwd.size, user->pwd.bytes);
}
