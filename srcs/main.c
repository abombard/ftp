#include "sys_types.h"
#include "log.h"
#include "list.h"

#include "msg.h"
#include "fd_set.h"
#include "sock.h"
#include "user.h"

#include <string.h>			/* memset(), memcpy() */
#include <stdlib.h>			/* atoi() */
#include <stdio.h>


/* -- types -- */
#include <unistd.h>		/* close() */

/**/

#include <sys/select.h>		/* select() */
#include <signal.h>				/* signal() */

#include <sys/utsname.h>	/* uname() */

#include <sys/time.h>

#include <signal.h>				/* signal(), SIGINT */

/* read / write */
static bool		read_from_client (const SOCKET sock, t_msg *msg, bool *ready_for_treatment)
{
	ssize_t		size;
	char	    buf[ MSG_SIZE_MAX ];

	*ready_for_treatment = false;
	size = read (sock, buf, sizeof (buf));
	if (size < 0)
	{
		perror ("read");
		return ( false );
	}
	if (size == 0)
	{
		msg_init (msg);
		ASSERT (msg_update (sizeof ("QUIT") - 1, "QUIT", msg));
		*ready_for_treatment = true;
	}
	else
 	{
		if (!msg_update ((size_t)size, buf, msg))
		{
			LOG_ERROR ("msg_update failed socket %d msg {%.*s} buf {%.*s}", sock, (int)msg->size, msg->bytes, (int)size, buf);
			return ( false );
		}
		if (buf[size - 1] == '\n')
		{
			msg->size -= 1;
			*ready_for_treatment = true;
		}
	}
	LOG_DEBUG ("Updated nessage on sock %d {%.*s}", sock, (int)msg->size, msg->bytes);
	return ( true );
}

static bool		write_to_client (const SOCKET sock, t_msg *msg, bool *ready_for_treatment)
{
	ssize_t		size;

	*ready_for_treatment = false;
	size = write (sock, msg->bytes, msg->size);
	if (size < 0)
	{
		perror ("write");
		return ( false );
	}
	LOG_DEBUG ("write %zu bytes on %zu bytes from {%.*s} on sock %d", size, msg->size, (int)size, msg->bytes, sock);
	memmove (msg->bytes + size,
					 msg->bytes,
					 msg->size - size);
	msg->size -= size;
	if (msg->size == 0)
	{
		*ready_for_treatment = true;
	}
	return ( true );
}

static void		send_msg (t_user *user, t_sets *sets)
{
	user->msg.bytes[user->msg.size] = '\n';
	user->msg.size += sizeof ("\n") - 1;
	list_move (&user->list, &sets->wfds_head);
}

static void		recv_msg (t_user *user, t_sets *sets)
{
	msg_init (&user->msg);
	list_move (&user->list, &sets->rfds_head);
}

/* accept connection */
static bool		accept_new_connection (t_users *users,
																		t_user **new_user)
{
	t_user							*new;
	SOCKET							connect_socket;
	socklen_t						client_address_size;
	struct sockaddr_in	client_address;

	*new_user = NULL;
	client_address_size = sizeof (client_address);
	connect_socket = accept (users->listen->sock, (struct sockaddr *)&client_address, &client_address_size);
	if (connect_socket == INVALID_SOCKET)
	{
		perror ("accept");
		return ( false );
	}
	if (client_address_size > sizeof (client_address))
	{
		LOG_ERROR ("client_address_size %zu sizeof (client_address) %zu", (size_t)client_address_size, (size_t)sizeof (client_address));
		close (connect_socket);
		return ( false );
	}
	if (!user__new (users,
									connect_socket,
									client_address_size,
									(struct sockaddr *)&client_address,
									&new))
	{
		LOG_DEBUG ("Too many connections");
		if (send (connect_socket, "Too many connections\n", sizeof ("Too many connections\n") - 1, MSG_DONTWAIT) == -1)
			perror ("send");
		if (close (connect_socket))
			perror ("close");
		return ( false );
	}
	if (!msg_update (sizeof ("220 Welcome") - 1, "220 Welcome", &new->msg))
	{
		LOG_ERROR ("msg_update failed sock %d msg {220 Welcome}", connect_socket);
		user__del (users, new);
		return ( false );
	}
	*new_user = new;
	return ( true );
}

static bool		ftp_command_user (t_user *user, const t_msg in, t_msg *out)
{
	if (in.size > USER_NAME_SIZE_MAX)
	{
		ASSERT (msg_update (sizeof ("501 User name too long"), "501 User name too long", out));
		return ( true );
	}
	memcpy (user->name.bytes, in.bytes, in.size);
	user->name.bytes[in.size] = '\0';
	user->name.size = in.size;
	ASSERT (msg_update (sizeof ("220 ") - 1, "220 ", out));
	ASSERT (msg_update (sizeof ("Logged in as ") - 1, "Logged in as ", out));
	ASSERT (msg_update (in.size, in.bytes, out));
	return ( true );
}

static bool		ftp_command_quit (t_user *user, const t_msg in, t_msg *out)
{
  (void) in;
	ASSERT (msg_update (sizeof ("221 Goodbye") - 1, "221 Goodbye", out));
	user->actif = false;
	return ( true );
}

static bool		ftp_command_syst (t_user *user, const t_msg in, t_msg *out)
{
	struct utsname	buf;

  (void)user;
  (void)in;
	if (uname (&buf))
	{
		perror ("uname");
		return ( false );
	}
	ASSERT (msg_update (sizeof ("212 ") - 1, "212 ", out));
	ASSERT (msg_update (strlen (buf.sysname), buf.sysname, out));
	ASSERT (msg_update (sizeof (" ") - 1, " ", out));
	ASSERT (msg_update (strlen (buf.release), buf.release, out));
	return ( true );
}

static bool		ftp_command_pwd (t_user *user, const t_msg in, t_msg *out)
{
  (void)in;
	ASSERT (msg_update (sizeof ("257 ") - 1, "257 ", out));
	ASSERT (msg_update (user->dir_cur.size, user->dir_cur.bytes, out));
	return ( true );
}

#if 0
static bool		ftp_command_pasv (t_user *user, const t_msg in, t_msg *out)
{
	SOCKET	sock;
	char		*sock_string;

	sock = socket_open ();
	ASSERT (sock != INVALID_SOCK);
	sock_string = itoa (sock);
	ASSERT (msg_update (strlen (sock_string), sock_string, out));
	return ( true );
}
#endif

#include <dirent.h>
static bool		ftp_command_pasv (t_user *user, const t_msg in, t_msg *out)
{
	DIR						*dp;
	struct dirent	*ep;

  (void)in;
	ASSERT (dp = opendir (user->dir_cur.bytes));
	while ((ep = readdir (dp)) != NULL)
	{
		if (ep->d_name[0] == '.')
			continue ;
		if (!msg_update (strlen (ep->d_name), ep->d_name, out))
		{
			LOG_ERROR ("msg_update failed");
			break ;
		}
		if (!msg_update (sizeof ("\n") - 1, "\n", out))
		{
			LOG_ERROR ("msg_update failed");
			break ;
		}
	}
	ASSERT (!closedir (dp));
	return ( true );
}

static bool		ftp_command_not_implemented (t_user *user, const t_msg in, t_msg *out)
{
  (void)user;
  (void)in;
	ASSERT (msg_update (sizeof ("502 Comand not implemented") - 1, "502 Command not implemented", out));
	return ( true );
}

#define ftp_command_port	ftp_command_not_implemented
#define ftp_command_type	ftp_command_not_implemented
#define ftp_command_mode	ftp_command_not_implemented
#define ftp_command_stru	ftp_command_not_implemented
#define ftp_command_retr	ftp_command_not_implemented
#define ftp_command_stor	ftp_command_not_implemented
#define ftp_command_noop	ftp_command_not_implemented

typedef struct		s_ftp_command
{
	t_buffer	cmd;
	bool			(*func)(t_user *, const t_msg, t_msg *);
}									t_ftp_command;

#define FTP_COMMAND_SIZE_MAX		4
static const t_ftp_command	ftp_command_array[] = {
	{ STR_TO_BUFFER ("USER"), ftp_command_user },
	{ STR_TO_BUFFER ("SYST"),	ftp_command_syst },
	{ STR_TO_BUFFER ("QUIT"), ftp_command_quit },
	{ STR_TO_BUFFER ("PORT"), ftp_command_port },
	{ STR_TO_BUFFER ("TYPE"), ftp_command_type },
	{ STR_TO_BUFFER ("MODE"), ftp_command_mode },
	{ STR_TO_BUFFER ("STRU"), ftp_command_stru },
	{ STR_TO_BUFFER ("RETR"), ftp_command_retr },
	{ STR_TO_BUFFER ("STOR"), ftp_command_stor },
	{ STR_TO_BUFFER ("NOOP"), ftp_command_noop },
	{ STR_TO_BUFFER ("PWD"),	ftp_command_pwd  },
	{ STR_TO_BUFFER ("PASV"),	ftp_command_pasv }
};

#include <ctype.h>	/* toupper() */

static bool		ftp_command (t_user *user, t_sets *sets)
{
	t_buffer	cmd;
	char			bytes[FTP_COMMAND_SIZE_MAX];
	t_msg			request;
	unsigned int				i, j;

	cmd.bytes = bytes;
	i = 0;
	while (i < sizeof (ftp_command_array) / sizeof (ftp_command_array[0]))
	{
		cmd.size = ftp_command_array[i].cmd.size;
		for (j = 0; j < ftp_command_array[i].cmd.size; j ++)
		{
			cmd.bytes[j] = toupper (user->msg.bytes[j]);
		}
		if (!memcmp (cmd.bytes, ftp_command_array[i].cmd.bytes, ftp_command_array[i].cmd.size))
		{
      for (j = cmd.size; j < user->msg.size; j ++)
      {
        if (!isspace(user->msg.bytes[j]))
          break;
      }
			msg_init (&request);
			ASSERT (msg_update (user->msg.size - j,
							            user->msg.bytes + j,
								          &request));
			msg_init (&user->msg);
			if (!ftp_command_array[i].func (user, request, &user->msg))
			{
				LOG_ERROR ("ftp_command_func failed cmd {%.*s} request {%.*s}", (int)cmd.size, cmd.bytes, (int)request.size, request.bytes);
				return ( false );
			}
			LOG_DEBUG ("ftp command out {%.*s}", (int)user->msg.size, user->msg.bytes);
			return ( true );
		}
		i++;
	}
	msg_init (&user->msg);
	ASSERT (msg_update (sizeof ("502 Command not recognized") - 1, "502 Command not recognized", &user->msg));
	list_move (&user->list, &sets->wfds_head);
	return ( false );
}

static bool		loop (t_users *users, t_sets *sets, int sig_fd)
{
	t_user		*user;

	while ( 1 )
	{
		int				ready;
		bool			ready_for_treatment;

		/* //TEMP use sigmask for SIGPIPE */
		struct timeval	tv;

		ASSERT (sets__prepare (sets));
		ASSERT (fd_set__update (sig_fd, &sets->rfds, &sets->nfds));

		tv.tv_sec = 5;
		tv.tv_usec = 0;

		ready = select (sets->nfds,
										&sets->rfds,
										list_is_empty (&sets->wfds_head) ? NULL : &sets->wfds,
										list_is_empty (&sets->efds_head) ? NULL : &sets->efds,
										&tv);
		if (ready == -1)
		{
			if (errno == EINTR)
				continue ;
			perror ("select");
			return ( false );
		}
		LOG_DEBUG ("select ready %d", ready);
		if (ready == 0)
			continue ;

		/* check wether thread_sig catched SIGINT */
		if (FD_ISSET (sig_fd, &sets->rfds))
			break ;

		/* Check for new connection */
		if (FD_ISSET (users->listen->sock, &sets->rfds))
		{
			FD_CLR (users->listen->sock, &sets->rfds);
			if (accept_new_connection (users, &user))
				send_msg (user, sets);
			ready --;
		}

		/* Check rfds */
		t_list	*pos, *safe;

		safe = sets->rfds_head.next;
		while ((pos = safe) && pos != &sets->rfds_head && (safe = safe->next))
		{
			user = CONTAINER_OF (pos, t_user, list);
			if (FD_ISSET (user->sock, &sets->rfds))
			{
				LOG_DEBUG ("socket %d ready for reading", user->sock);
				if (!read_from_client (user->sock, &user->msg, &ready_for_treatment))
				{
					LOG_ERROR ("read_from_client failed sock %d", user->sock);
					user__del (users, user);
				}
				if (ready_for_treatment)
				{
					(void) ftp_command (user, sets);
					send_msg (user, sets);
				}
				ready --;
			}
		}

		/* check wfds */
		safe = sets->wfds_head.next;
		while ((pos = safe) && pos != &sets->wfds_head && (safe = safe->next))
		{
			user = CONTAINER_OF (pos, t_user, list);
			if (FD_ISSET (user->sock, &sets->wfds))
			{
				LOG_DEBUG ("socket %d ready for writing", user->sock);
				if (!write_to_client (user->sock, &user->msg, &ready_for_treatment))
				{
					LOG_ERROR ("write_to_client failed sock %d msg {%.*s}", user->sock, (int)user->msg.size, user->msg.bytes);
					user__del (users, user);
				}
				if (ready_for_treatment)
				{
					// Case MESSAGE NOT FINISHED
					if (user->actif)
						recv_msg (user, sets);
					else
						user__del (users, user);
				}
				ready --;
			}
		}

		if (ready != 0)
		{
			LOG_ERROR ("ready %d", ready);
		}

	}

	while (!list_is_empty (&sets->rfds_head))
	{
		LOG_DEBUG ("clearing rfds");

		user = CONTAINER_OF (sets->rfds_head.next, t_user, list);
		user__del (users, user);
	}
	while (!list_is_empty (&sets->wfds_head))
	{
		LOG_DEBUG ("clearing wfds");

		user = CONTAINER_OF (sets->wfds_head.next, t_user, list);
		user__del (users, user);
	}

	return ( true );
}

void		*thread_sig (void *arg)
{
	t_sig_warn		*sig_warn;
	int						sig;
	int						size;

	sig_warn = arg;
	while ( 1 )
	{
		SYS_ERR (sigwait (&sig_warn->sigset, &sig));
		if (sig == SIGINT)
		{
			while ((size = write (sig_warn->fd, "dummy", sizeof ("dummy") - 1)) == 0)
				;
			if (size == -1)
			{
				perror ("write");
				LOG_ERROR ("Failed to interrupt select. Consider using 'kill -9 <server>'");
			}
			else
				break ;
		}
	}
	LOG_DEBUG ("Terminating thread_sig");
	return ( NULL );
}

int main (int argc, char **argv)
{
	int				exit_status;

	if (argc != 3)
	{
		fprintf (stderr, "Usage: %s <host> <listen-port>\n", argv[0]);
		return ( 1 );
	}

	/* sig_warn */
	t_sig_warn		sig_warn;
	int						pipefd[2];

	if (pipe (pipefd))
	{
		perror ("pipe");
		return ( 1 );
	}
	sig_warn.fd = pipefd[1];

	/* .sigset */
	if (sigemptyset (&sig_warn.sigset))
	{
		LOG_ERROR ("sigemptyset failed");
		return ( 1 );
	}
	if (sigaddset (&sig_warn.sigset, SIGINT))
	{
		LOG_ERROR ("sigaddset failed");
		return ( 1 );
	}
	errno = pthread_sigmask (SIG_BLOCK, &sig_warn.sigset, NULL);
	if (errno)
	{
		perror ("pthread_sigmask");
		return ( 1 );
	}

	pthread_t		thread_sig_handle;

	SYS_ERR (pthread_create (&thread_sig_handle, NULL, &thread_sig, &sig_warn));

	/* program */
	t_users		users;
	t_sets		sets;

	char			*host;
	int				port;

	host = argv[1];
	port = atoi (argv[2]);
	if (!users__initialize (&users, host, port))
	{
		LOG_ERROR ("users__initialize failed");
		return ( 1 );
	}
	sets__initialize (&sets);

	list_push_back (&users.listen->list, &sets.rfds_head);

	exit_status = loop (&users, &sets, pipefd[0]) ? 0 : 1;

	sets__finalize (&sets);
	users__finalize (&users);

	if (exit_status)
		kill (getpid (), SIGINT);

	/* thread */
	SYS_ERR (pthread_join (thread_sig_handle, NULL));

	if (close (pipefd[0]))
		perror ("close");
	if (close (pipefd[1]))
		perror ("close");

	LOG_DEBUG ("exit normally");

  return ( true );
}
