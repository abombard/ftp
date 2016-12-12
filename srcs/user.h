#ifndef USER_H
# define USER_H

#include "sys_types.h"
#include "log.h"
#include "list.h"

#include "msg.h"
#include "sock.h"

#define USER_NAME_SIZE_MAX					31
typedef struct 	s_user_name
{
	size_t	size;
	char	bytes[ USER_NAME_SIZE_MAX + 1 ];
}		t_user_name;

# define PATH_SIZE_MAX						255
typedef struct	s_path
{
	size_t	size;
	char	bytes[ PATH_SIZE_MAX ];
}		t_path;

typedef struct	s_user
{
	SOCKET		sock;
	SOCKET		sock_cmd;
	SOCKET		sock_data;
	char		host[ NI_MAXHOST ];
	char		serv[ NI_MAXSERV ];
	t_user_name			name;
	t_path		home;
	t_path		pwd;
	bool		actif;
	t_msg		msg;
	t_list		list;
}		t_user;

#define CONNECTION_COUNT_MAX				1
typedef struct	s_users
{
	bool		is_initialized;

	t_path	  root_dir;

	t_user	user[ CONNECTION_COUNT_MAX + 1 ]; /* + 1 for listen_socket */
	t_list	user_free;

	t_user	*listen;
}		t_users;

bool	users__initialize (char *host, int port);
void	users__finalize (void);
t_user	*users__listen(char *host, int port);

bool	user__new (const SOCKET sock,
		const int ai_addrlen,
		const struct sockaddr *ai_addr,
		t_user **out_user);

void	user__del (t_user *user);

void	users__prepare (t_users *users);

void	user__show (t_user *user);

#endif
