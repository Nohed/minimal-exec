// Included headerfiles.
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// Global constants.
#define MAX_LINE_LENGTH 1024

// Function declarations.
int read_input(char ***command_arr, FILE *fp);
void dealloc_arr(char **command_arr, int number_of_commands);
void exec_command(char *command, int number_of_commands, int pipe_array[number_of_commands - 1][2], int current_iteration, pid_t *processes);
void wait_for_processes(pid_t *processes, int number_of_processes);
void close_pipes(int number_of_processes, int pipe_array[number_of_processes - 1][2]);
void execute(char *command);
void create_pipes(int number_of_commands, int (*pipe_array)[number_of_commands - 1][2]);

/**!
*	\brief	 A program that executes multiple commands in parallel.
*	\arg	 0 arguments or 1 argument, a file containing commands.
*		 0 arguments will read commands from stdin.
*		 1 argument will read commands from the file.
*	\author  Noel Hedlund
*	\date    2023-09-14
*	\version 1.0
*		 2.0 Split main and execute_command into multiple functions.
*		     Changed read_input to allocate memory row by row instead of reading 1024.
*		     improved readability.
		     Changed error print for failure of waitpid().
*
*	\return  0 on success.
*/
int main(int argc, char *argv[])
{
	FILE *fp;
	int number_of_commands = 0;
	char **input_array;
	// Check arguments.
	switch (argc)
	{
	case 1:
		// No arguments, Read from stdin.
		number_of_commands = read_input(&input_array, stdin);
		break;
	case 2:
		// File argument given.
		if ((fp = fopen(argv[1], "r")) == NULL)
		{
			perror("File could not be opened");
			exit(EXIT_FAILURE);
		}

		number_of_commands = read_input(&input_array, fp);
		fclose(fp);
		break;
	default:
		// Wrong number of arguments.
		fprintf(stderr, "Wrong number of arguments\nExpected: zero or one\n"
				"Got: %d\n", argc);
		exit(EXIT_FAILURE);
	}

	int pipe_array[number_of_commands - 1][2];
	pid_t processes[number_of_commands];

	// Create all necessary pipes.
	create_pipes(number_of_commands, &pipe_array);

	// Execute all commands.
	for (int i = 0; i < number_of_commands; i++)
	{
		exec_command(input_array[i], number_of_commands, pipe_array, i, &processes[i]);
	}
	// Clean up, wait for processes.
	dealloc_arr(input_array, number_of_commands);
	close_pipes(number_of_commands, pipe_array);
	wait_for_processes(processes, number_of_commands);
	exit(EXIT_SUCCESS);
}

/**!
*	\brief	Reads input from a file or stdin.
*	\param	command_arr: A pointer to an array where commands will be stored as "strings".
*		fp: A pointer to a file(filepointer or stdin).
*	\version 1.0
*		2.0 Function now allocates memory row by row
*		instead of reading 1024.
*	\return The number of commands read.
*/
int read_input(char ***command_arr, FILE *fp)
{
	char line[MAX_LINE_LENGTH];
	int read_commands = 0;
	*command_arr = NULL;

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		// Remove trailing newline character.
		if (line[strlen(line) - 1] == '\n')
		{
			line[strlen(line) - 1] = '\0';
		}

		// Allocate memory for the current command
		char *current_command = strdup(line);
		if (current_command == NULL)
		{
			perror("Memory allocation failed");
			exit(EXIT_FAILURE);
		}

		// Reallocate memory for the array to accommodate the new command
		*command_arr = realloc(*command_arr, (read_commands + 1) * sizeof(char *));
		if (*command_arr == NULL)
		{
			perror("Memory reallocation failed");
			exit(EXIT_FAILURE);
		}

		// Store the current command in the array
		(*command_arr)[read_commands] = current_command;
		read_commands++;
	}

	return read_commands;
}

/**
 * \brief Executes a command with pipes if needed.
 * \param command: The command to execute as a string.
 * \param number_of_commands: The total number of commands that the program executes.
 * \param pipe_array: An array of pipes for the program.
 * \param current_iteration: The current iteration of the executing loop.
 * \param process: A pointer to the address where process IDs will be stored.
 *
 * \version 1.0
 * 	    2.0 Shortened function by simplifying if/else. removed unnecessary code.
 *  		Improved readability.
 * \return void
 */
void exec_command(char *command, int number_of_commands, int pipe_array[][2], int current_iteration, pid_t *process)
{
	pid_t id = fork();
	if (id == -1)
	{
		perror("Fork failed");
		exit(EXIT_FAILURE);
	}

	if (id == 0)
	{
		// Child process
		// Handle pipes if not the first command
		if (current_iteration > 0)
		{
			if (dup2(pipe_array[current_iteration - 1][0], STDIN_FILENO) == -1)
			{
				perror("dup2 for stdin failed");
				exit(EXIT_FAILURE);
			}
			close(pipe_array[current_iteration - 1][1]);
		}

		// Handle pipes if not the last command
		if (current_iteration < number_of_commands - 1)
		{
			if (dup2(pipe_array[current_iteration][1], STDOUT_FILENO) == -1)
			{
				perror("dup2 for stdout failed");
				exit(EXIT_FAILURE);
			}
			close(pipe_array[current_iteration][0]);
		}

		// Close all pipes
		close_pipes(number_of_commands, pipe_array);

		// Execute the command
		execute(command);
	}
	else
	{
		*process = id;
	}
}

/**!
*\brief	Deallocates memory for an array of commands.
*\param	command_arr: A pointer to an array of commands.
*\param number_of_commands: The number of commands in the array.
*\version 1.0
*\return void
*/
void dealloc_arr(char **command_arr, int number_of_commands)
{

	for (int i = 0; i < number_of_commands; i++)
	{
		free(command_arr[i]);
	}
	free(command_arr);
}

/**!
*\brief	Waits for every child process to exit, and checks their exit status.
*\param	processes: An array of process ID´s.
*\param	number_of_processes: The number of processes.
*\version 1.0
*\return void
*/
void wait_for_processes(pid_t *processes, int number_of_processes)
{
	int status;
	for (int i = 0; i < number_of_processes; i++)
	{
		waitpid(processes[i], &status, 0);
		if (status != 0)
		{
			perror("Child process failed");
			exit(EXIT_FAILURE);
		}
	}
}

/*!
 * \brief Creates all pipes for the program, exits with error message if pipe fails.
 * \param number_of_commands The number of commands that will be executed.
 * \param pipe_array A pointer to the array where the pipes will be stored.
 * \version 1.0
 * \return void
*/
void create_pipes(int number_of_commands, int (*pipe_array)[number_of_commands - 1][2])
{
	for (int i = 0; i < number_of_commands - 1; i++)
	{
		if (pipe((*pipe_array)[i]) == -1)
		{
			perror("Pipe failed");
			exit(EXIT_FAILURE);
		}
	}
}

/**!
 * \brief Closes all pipes for the program.
 * \param number_of_processes The number of processes that will be executed.
 * \param pipe_array A pointer to the array where the pipes are stored.
 * \version 1.0
 * \return void
*/
void close_pipes(int number_of_processes, int pipe_array[number_of_processes - 1][2])
{
	for (int i = 0; i < number_of_processes - 1; i++)
	{
		close(pipe_array[i][0]);
		close(pipe_array[i][1]);
	}
}

/**!
 * \brief Executes a command, and exits if it fails.
 * \param command The command to execute as a string.
 * \version 1.0
 * \return void
*/
void execute(char *command)
{
	execlp("sh", "sh", "-c", command, NULL);
	perror("Exec failed");
	exit(EXIT_FAILURE);
}
