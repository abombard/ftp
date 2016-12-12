#ifndef FD_SET_H
# define FD_SET_H

#include "sys_types.h"
#include "log.h"

#include "sock.h"
#include "user.h"

typedef struct	s_sets
{
	int				nfds;

	fd_set		rfds;
	fd_set		wfds;
	fd_set		efds;

	t_list		rfds_head;
	t_list		wfds_head;
	t_list		efds_head;
}								t_sets;

void		sets__initialize (t_sets *sets, t_user *listen_user);
void		sets__finalize (t_sets *sets);

bool		fd_set__update (const SOCKET sock, fd_set *set, int *nfds);
bool		fd_set__init (t_list *head, fd_set *set, int *nfds);
bool		sets__prepare (t_sets *sets);

#endif
