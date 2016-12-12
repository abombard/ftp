#include "msg.h"

#include "log.h"
#include <string.h>

extern void		msg_init (t_msg *msg)
{
	msg->bytes[0] = '\0';
	msg->size = 0;
}

extern bool		msg_update (const size_t size,
									const char *str,
									t_msg *msg)
{
	if (size + msg->size > MSG_SIZE_MAX)
	{
		LOG_ERROR ("msg->size %zu size %zu MSG_SIZE_MAX %d", msg->size, size, MSG_SIZE_MAX);
		return ( false );
	}
	memcpy (msg->bytes + msg->size, str, size);
	msg->size += size;
	msg->bytes[msg->size] = '\0';
	return ( true );
}

