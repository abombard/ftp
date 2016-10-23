#include "fd_set.h"

extern void		sets__initialize (t_sets *sets)
{
	INIT_LIST_HEAD (&sets->rfds_head);
	INIT_LIST_HEAD (&sets->wfds_head);
	INIT_LIST_HEAD (&sets->efds_head);
}

extern void		sets__finalize (t_sets *sets)
{
	if (!list_is_empty (&sets->rfds_head))
		LOG_WARNING ("some sockets are still open on rfds");
	if (!list_is_empty (&sets->wfds_head))
		LOG_WARNING ("some sockets are still open in wfds");
	if (!list_is_empty (&sets->efds_head))
		LOG_WARNING ("some sockets are still open in efds");
}

extern bool		fd_set__update (const SOCKET sock, fd_set *set, int *nfds)
{
	if (sock < 0 || sock > FD_SETSIZE)
	{
		LOG_ERROR ("sock %d FD_SETSIZE %d", sock, FD_SETSIZE);
		return ( false );
	}
	FD_SET (sock, set);
	*nfds = sock > *nfds - 1 ? sock + 1 : *nfds;
	return ( true );
}

extern bool		fd_set__init (t_list *head, fd_set *set, int *nfds)
{
	t_user		*user;
	t_list		*pos;

	pos = head;
	while ((pos = pos->next) && pos != head)
	{
		user = CONTAINER_OF (pos, t_user, list);
		if (!fd_set__update (user->sock, set, nfds))
		{
			LOG_ERROR ("fd_set__update failed sock %d nfds %d", user->sock, *nfds);
			return ( false );
		}
	}
	return ( true );
}

extern bool		sets__prepare (t_sets *sets)
{
	int		sock_count;

	sets->nfds = 0;
	FD_ZERO (&sets->rfds);
	FD_ZERO (&sets->wfds);
	FD_ZERO (&sets->efds);
	ASSERT (fd_set__init (&sets->rfds_head, &sets->rfds, &sets->nfds));
	ASSERT (fd_set__init (&sets->wfds_head, &sets->wfds, &sets->nfds));
	ASSERT (fd_set__init (&sets->efds_head, &sets->efds, &sets->nfds));
	return ( true );
}

