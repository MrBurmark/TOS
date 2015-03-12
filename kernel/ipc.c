
#include <kernel.h>

PORT_DEF port[MAX_PORTS];



PORT create_port()
{
	return create_new_port(active_proc);
}


PORT create_new_port (PROCESS owner)
{
	PORT p, end;

	/* allocate unused port */
	end = port + MAX_PORTS;
	for (p = port; p < end; p++)
	{
		if (p->used == FALSE)
		{
			break;
		}
	}
	assert(p != end);

	/* Initialize PORT */
	p->magic 				= MAGIC_PORT;
	p->used 				= TRUE;
	p->open 				= TRUE;
	p->owner 				= owner;
	p->blocked_list_head 	= NULL;
    p->blocked_list_tail 	= NULL;
    p->next 				= NULL;

    /* Add new port to head of owner's port list */
    p->next 			= owner->first_port;
    owner->first_port 	= p;

	return p;
}


void open_port (PORT port)
{
	assert(port->magic == MAGIC_PORT);
	port->open = TRUE;
}


void close_port (PORT port)
{
	assert(port->magic == MAGIC_PORT);
	port->open = FALSE;
}

void add_to_blocked_list(PORT dest_port)
{
	if(dest_port->blocked_list_tail == NULL)
	{
		/* Empty list */
		dest_port->blocked_list_head = active_proc;
	}
	else
	{
		/* Add to tail of existing list */
		dest_port->blocked_list_tail->next_blocked = active_proc;
	}
	dest_port->blocked_list_tail = active_proc;
	active_proc->next_blocked = NULL;
}


void send (PORT dest_port, void* data)
{
	assert(dest_port->magic == MAGIC_PORT);

	if(dest_port->open && dest_port->owner->state == STATE_RECEIVE_BLOCKED)
	{
		/* Destination can recieve immediately */
		/* store data in dest PCB */
		dest_port->owner->param_proc = active_proc;
		dest_port->owner->param_data = data;
		dest_port->owner->state = STATE_READY;
		active_proc->state = STATE_REPLY_BLOCKED;
		add_ready_queue(dest_port->owner);
	}
	else
	{
		/* Must wait for receive */
		/* Store data to own PCB */
		active_proc->param_proc = active_proc;
		active_proc->param_data = data;

		add_to_blocked_list(dest_port);
		
		active_proc->state = STATE_SEND_BLOCKED;
	}
	remove_ready_queue(active_proc);
	resign();
}


void message (PORT dest_port, void* data)
{
	assert(dest_port->magic == MAGIC_PORT);

	if(dest_port->open && dest_port->owner->state == STATE_RECEIVE_BLOCKED)
	{
		/* Destination can recieve immediately */
		/* store data in dest PCB */
		dest_port->owner->param_proc = active_proc;
		dest_port->owner->param_data = data;
		dest_port->owner->state = STATE_READY;
		add_ready_queue(dest_port->owner);
	}
	else
	{
		/* Must wait for receive */
		/* Store data to own PCB */
		active_proc->param_proc = active_proc;
		active_proc->param_data = data;

		add_to_blocked_list(dest_port);

		active_proc->state = STATE_MESSAGE_BLOCKED;
		remove_ready_queue(active_proc);
	}
	resign();
}


PROCESS pop_message( void )
{
	PROCESS sender = NULL;

	/* Find first message waiting */
	PORT p = active_proc->first_port;
	while(p != NULL)
	{
		if (p->open && p->blocked_list_head != NULL)
		{
			/* Found first message */
			/* remove that process from blocked list */
			sender = p->blocked_list_head;
			p->blocked_list_head = sender->next_blocked;
			sender->next_blocked = NULL;
			if(p->blocked_list_head == NULL)
			{
				p->blocked_list_tail = NULL;
			}
			break;
		}
		p = p->next;
	}
	return sender;
}


void* receive (PROCESS* sender)
{
	void* mess = NULL;

	/* take a message if one is waiting */
	*sender = pop_message();

	if(*sender != NULL)
	{
		/* Message found */
		/* data in sender PCB */
		mess = (*sender)->param_data;

		if((*sender)->state == STATE_MESSAGE_BLOCKED)
		{
			(*sender)->state = STATE_READY;
			add_ready_queue(*sender);
		}
		else if((*sender)->state == STATE_SEND_BLOCKED)
		{
			(*sender)->state = STATE_REPLY_BLOCKED;
		}
	}
	else
	{
		/* Wait for a message */
		remove_ready_queue(active_proc);
		active_proc->state = STATE_RECEIVE_BLOCKED;
		resign();
		/* Message received */
		/* data in own PCB */
		mess = active_proc->param_data;
		*sender = active_proc->param_proc;
	}

	return mess;
}


void reply (PROCESS sender)
{
	/* put sender back on ready queue */
	sender->state = STATE_READY;
	add_ready_queue(sender);
	resign();
}


void init_ipc()
{
	PORT p, end;

	/* initialize all ports as used = FALSE */
	end = port + MAX_PORTS;
	for (p = port; p < end; p++)
	{
		p->magic = ~MAGIC_PORT;
		p->used = FALSE;
	}

	/* Create port for null process */
	create_port();
}
