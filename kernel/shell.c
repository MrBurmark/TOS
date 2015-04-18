#include <kernel.h>


static WINDOW shell_window_def = {0, 10, 80 - MAZE_WIDTH, 14, 0, 0, '_'};
WINDOW* shell_wnd = &shell_window_def;

void win_printc(WINDOW *wnd, char c);

command shell_cmd[MAX_COMMANDS + 1];

typedef struct _input_buffer
{
	BOOL used;
	int length;
	char buffer[INPUT_BUFFER_MAX_LENGTH + 1];
} input_buffer;

input_buffer history[SHELL_HISTORY_SIZE];
input_buffer *history_head; // always points to a new buffer
input_buffer *history_current; // always points to the buffer displayed on screen

void clear_in_buf(input_buffer *in_buf)
{
	k_memset(in_buf->buffer, 0, INPUT_BUFFER_MAX_LENGTH + 1);
	in_buf->length = 0;
	in_buf->used = FALSE;
}

int clean_in_buf(input_buffer *in_buf)
{
	int i = 0;
	int j = 0;
	int in_arg = FALSE;

	while (j < in_buf->length)
	{
		if (in_buf->buffer[j] == ' ' || 
				in_buf->buffer[j] == '\t' || 
				in_buf->buffer[j] == '\n' || 
				in_buf->buffer[j] == '\r' ||
				in_buf->buffer[i] == '\0')
		{
			// skip white space
			if (in_arg == TRUE)
			{
				in_buf->buffer[i++] = '\0';
			}
			in_arg = FALSE;
			j++;
		}
		else
		{
			in_buf->buffer[i++] = in_buf->buffer[j++];
			in_arg = TRUE;
		}
	}
	in_buf->buffer[i] = '\0';
	in_buf->length = i;
	return i;
}

void history_up()
{
	int i;
	input_buffer *next = history + ((history_current - history - 1) % SHELL_HISTORY_SIZE);
	if (next->used && next != history_head)
	{
		// if next buffer used and not back to head

		// clear screen of current data
		for(i = 0; i < history_current->length; i++)
		{
			win_printc(shell_wnd, '\b');
		}

		// add new data
		history_current = next;
		for(i = 0; i < history_current->length; i++)
		{
			win_printc(shell_wnd, history_current->buffer[i]);
		}
	}
}

void history_down()
{
	int i;
	input_buffer *previous = history + ((history_current - history + 1) % SHELL_HISTORY_SIZE);
	if (history_current != history_head)
	{
		// if not back to head

		// clear screen of current data
		for(i = 0; i < history_current->length; i++)
		{
			win_printc(shell_wnd, '\b');
		}

		// add new data
		history_current = previous;
		for(i = 0; i < history_current->length; i++)
		{
			win_printc(shell_wnd, history_current->buffer[i]);
		}
	}
}


void print_commands(WINDOW *wnd, const command *cmds)
{
	int i;

	wprintf(wnd, "Available commands\n");

	for (i = 0; cmds[i].func != NULL; i++)
	{
		wprintf(wnd, " - %s:\t%s\n", cmds[i].name, cmds[i].description);
	}
}

command* find_command(const command *cmds, const char *in_name)
{
	for (; cmds->func != NULL; cmds++)
	{
		if (k_strcmp(cmds->name, in_name) == 0)
		{
			break;
		}
	}
	return (command *)cmds;
}

void init_command(char *name, int (*func) (int argc, char **argv), char *description, command *cmd) 
{
	cmd->name = name;
	cmd->func = func;
	cmd->description = description;
}


// finds null char seperated args in in_buf
// saves a pointer to each arg in argv
// returns the number of args found
int setup_args(const input_buffer *in_buf, char **argv)
{
	int i, j;
	int in_arg = FALSE;
	for (i = j = 0; i < in_buf->length; i++)
	{
		if (in_buf->buffer[i] != '\0')
		{
			// reading through part of an argument, add to argv if this is the first char
			if (in_arg == FALSE)
			{
				argv[j++] = (char *)&in_buf->buffer[i];
				in_arg = TRUE;
			}
		}
		else
		{
			// found a null char, no longer in an argument
			in_arg = FALSE;
		}
	}
	return j;
}

void clear_args(char **argv)
{
	k_memset(argv, 0, INPUT_BUFFER_MAX_LENGTH*sizeof(char *));
}


// prints characters or removes characters if backspace was pressed
void win_printc(WINDOW *wnd, char c)
{
	if (c == '\b')
	{
		remove_char(wnd);
	}
	else
	{
		if (c == '\0') c = ' ';
		wprintf(wnd, "%c", c);
	}
}

// returns the number of caracters input
// formats the input buffer to contain non-white space characters separated by the null char
// prints the entered characters and returns when \n or \r is received
int get_input()
{
	char c;
	Keyb_Message msg;
	msg.key_buffer = &c;

	history_current->used = TRUE;

	do 
	{
		// get new character
		send(keyb_port, (void *)&msg);

		if (c == '\b' && history_current->length > 0)
		{
			// backspace received and can backspace
			history_current->buffer[--history_current->length] = '\0';
			win_printc(shell_wnd, c);
		}
		else if (((c >= 32 && c < 127) || c == '\t')
				 && history_current->length < INPUT_BUFFER_MAX_LENGTH)
		{
			// writable caracter received and can print
			history_current->buffer[history_current->length++] = c;
			win_printc(shell_wnd, c);
		}
		else
		{
			// non-writable
			switch (c)
			{
				case TOS_UP:
					history_up();
					break;
				case TOS_DOWN:
					history_down();
					break;
			}
		}
	} 
	while(c != '\n' && c != '\r');

	wprintf(shell_wnd, "\n");

	history_current->buffer[history_current->length] = '\0';
	return history_current->length;
}

void shell_process(PROCESS proc, PARAM param)
{
	int i;
	command *cmd;
	char *argv[INPUT_BUFFER_MAX_LENGTH];

	wprintf(shell_wnd, "Welcome to the TOS shell\n");

	while(1)
	{
		wprintf(shell_wnd, "tos> ");

		clear_in_buf(history_current);

		get_input();

		clean_in_buf(history_current);

		cmd = find_command(shell_cmd, history_current->buffer);

		if (history_current->buffer[0] == '\0')
		{
			// empty line, don't print
			continue;
		}
		else if (cmd->func == NULL)
		{
			wprintf(shell_wnd, "Unknown Command: %s\n", history_current->buffer);
		}
		else
		{
			clear_args(argv);

			i = setup_args(history_current, argv);

			// run command
			i = (cmd->func)(i, argv);

			// check for error code returned
			if (i != 0)
				wprintf(shell_wnd, "%s exited with code %d\n", cmd->name, i);
		}

		// copy current to head if necessary
		if (history_head != history_current)
		{
			*history_head = *history_current;
		}
		// move head and current along
		history_head = history + ((history_head - history + 1) % SHELL_HISTORY_SIZE);
		history_current = history_head;
	}
}

int print_shell_commands_func(int argc, char **argv)
{
	print_commands(shell_wnd, shell_cmd);
	return 0;
}

int print_process_func(int argc, char **argv)
{
	print_all_processes(shell_wnd);
	return 0;
}

int sleep_func(int argc, char **argv)
{
	if (argc > 1)
	{
		wprintf(shell_wnd, "Sleeping: ");
		sleep(atoi(argv[1]));
		wprintf(shell_wnd, "woke after %d\n", atoi(argv[1]));
		return 0;
	}
	else
	{
		wprintf(shell_wnd, "Usage: sleep num_of_ticks\n");
		return 1;
	}
}

int clear_func(int argc, char **argv)
{
	clear_window(shell_wnd);
	return 0;
}

int echo_func(int argc, char **argv)
{
	int i = 1;
	while (i < argc)
		wprintf(shell_wnd, "%s ", argv[i++]);
	if (argc > 1)
		wprintf(shell_wnd, "\n");
	return 0;
}

int pacman_func(int argc, char **argv)
{
	int i;
	pacman_wnd->x = WINDOW_TOTAL_WIDTH - MAZE_WIDTH;
	pacman_wnd->y = WINDOW_TOTAL_HEIGHT - MAZE_HEIGHT - 1;
	pacman_wnd->width = MAZE_WIDTH;
	pacman_wnd->height = MAZE_HEIGHT + 1;
	move_cursor(pacman_wnd, 0, 0);
	pacman_wnd->cursor_char = '_';

	if (argc > 1)
	{
		i = atoi(argv[1]);

		if (i <= 0 || i > 5)
		{
			wprintf(shell_wnd, "Invalid number of ghosts %d\n", i);
			return 2;
		}

		wprintf(shell_wnd, "Starting pacman with %d ghosts\n", i);
		init_pacman(pacman_wnd, i);
		return 0;
	}
	else
	{
		wprintf(shell_wnd, "Usage: pacman num_ghosts\n");
		return 1;
	}
}

int prime_func(int argc, char **argv)
{
	if (argc == 1)
	{
		wprintf(shell_wnd, "%d\n", null_prime);
	}
	else if (argc > 1)
	{
		if (is_num(argv[1]))
		{
			prime_reset = TRUE;
			new_start = atoi(argv[1]);
		}
	}	
	return 0;
}

int train_func(int argc, char **argv)
{
	Train_Message msg;

	if (argc < 2)
	{
		wprintf(shell_wnd, "Usage: train cmd [args]\n");
		return 1;
	}
	else
	{
		msg.argc = argc - 1;
		msg.argv = &argv[1];
		send(train_port, (void *)&msg);
		return 0;
	}
}

int kill_func(int argc, char **argv)
{
	BOOL force = FALSE;
	int proc_num;

	if (argc < 2)
	{
		wprintf(shell_wnd, "Usage: kill proc_num [-f]\n");
		return 1;
	}
	else if (is_num(argv[1]))
	{
		proc_num = atoi(argv[1]);

		if (argc >= 3)
			if (k_strcmp("-f", argv[2]) == 0)
				force = TRUE;

		if (kill_process(pcb + proc_num, force))
		{
			wprintf(shell_wnd, "Kill Success\n");
		} 
		else
		{
			wprintf(shell_wnd, "Kill Failure\n");
		}
		return 0;
	}
	else
	{
		wprintf(shell_wnd, "Usage: kill proc_num [-f]\n");
		return 1;
	}
}

void init_shell()
{
	int i = 0;

	// init used commands
	init_command("help", print_shell_commands_func, "Prints all commands", &shell_cmd[i++]);
	init_command("prtprc", print_process_func, "Prints all processes", &shell_cmd[i++]);
	init_command("sleep", sleep_func, "Sleeps for the given duration", &shell_cmd[i++]);
	init_command("clear", clear_func, "Clears the screen", &shell_cmd[i++]);
	init_command("echo", echo_func, "Prints its arguments", &shell_cmd[i++]);
	init_command("pacman", pacman_func, "Starts pacman with given number of ghosts", &shell_cmd[i++]);
	init_command("prime", prime_func, "Prints a prime computed by the null process", &shell_cmd[i++]);
	init_command("train", train_func, "Train related commands, see train help", &shell_cmd[i++]);
	init_command("kill", kill_func, "Kill a process", &shell_cmd[i++]);

	// init unused commands
	while (i < MAX_COMMANDS)
	{
		init_command("----", NULL, "unused", &shell_cmd[i++]);
	}
	shell_cmd[MAX_COMMANDS].name = "NULL";
	shell_cmd[MAX_COMMANDS].func = NULL;
	shell_cmd[MAX_COMMANDS].description = "NULL";

	// init shell command history
	for (i = 0; i < SHELL_HISTORY_SIZE; i++)
	{
		clear_in_buf(history + i);
	}
	history_head = history;
	history_current = history;

	create_process (shell_process, 3, 0, "Shell process");

	init_train(train_wnd);
}
