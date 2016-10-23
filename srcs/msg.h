#ifndef MSG_H
# define MSG_H

#include "sys_types.h"

#define MSG_SIZE_MAX		512

typedef struct		s_msg
{
	char			bytes[MSG_SIZE_MAX];
	size_t		size;
} 								t_msg;

void		msg_init (t_msg *msg);
bool		msg_update (const size_t size,
										const char *str,
										t_msg *msg);


#endif
