/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2023 Polina Agranat, Red Hat
   Copyright 2023 Dennis Brendel, Red Hat */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

enum {
	EXIT_FAILURE_SEM_OPEN,
	EXIT_FAILURE_SEM_WAIT,
	EXIT_FAILURE_SEM_REMOVE,
	EXIT_FAILURE_INVALID_ARGUMENTS,
	EXIT_FAILURE_KEY_GEN,
	EXIT_FAILURE_SEM_OPERATION,
	EXIT_FAILURE_FILE_CREATE,
	EXIT_FAILURE_FILE_REMOVE
};

volatile sig_atomic_t timed_out = 0;

void handle_timeout()
{
	timed_out = 1;
}

int main(int argc, char *argv[])
{
	int sem_id, provided_timeout, timeout_seconds;
	char *err = NULL, *file_name = "semaphore_key";
	struct sembuf sem_op;
	key_t sem_key;
	const int sem_value = 0;
	FILE *file = NULL;

	if (argc == 1)
	{
		timeout_seconds = 1;
	}
	else if (argc == 2)
	{
		provided_timeout = strtol(argv[1], &err, 10);
		if (*err == '\0' && errno != ERANGE && provided_timeout > 0)
		{
			timeout_seconds = provided_timeout;
		}
		else
		{
			fprintf(stderr, "Error: Invalid timeout value. "
					"Usage: %s <timeout-seconds>\n", argv[0]);
			return EXIT_FAILURE_INVALID_ARGUMENTS;
		}
	}
	else
	{
		fprintf(stderr, "Error: Invalid number of arguments. "
				"Usage: %s <timeout-seconds>\n", argv[0]);
		return EXIT_FAILURE_INVALID_ARGUMENTS;
	}


	// Create an empty file in the current directory
	file = fopen(file_name, "w");
	if (file == NULL)
	{
		fprintf(stderr, "Error: Unable to create a file %s\n", strerror(errno));
		return EXIT_FAILURE_FILE_CREATE;
	}
	fclose(file);

	signal(SIGALRM, handle_timeout);
	alarm(timeout_seconds);

	// Generate a key for the semaphore
	sem_key = ftok(file_name, 'A');
	if (sem_key == -1)
	{
		fprintf(stderr, "Error: "
				"Unable to generate a key for semaphore %s\n", strerror(errno));
		return EXIT_FAILURE_KEY_GEN;
	}

	// Create or open a System V semaphore
	sem_id = semget(sem_key, 1, IPC_CREAT | IPC_EXCL | 0666);
	if (sem_id == -1)
	{
		// Semaphore already exists. Try to open it and then run as a client.
		if (errno == EEXIST)
		{
			sem_id = semget(sem_key, 1, 0);
			if (sem_id == -1)
			{
				fprintf(stderr, "Error: "
						"Unable to open semaphore. %s\n", strerror(errno));
				return EXIT_FAILURE_SEM_OPEN;
			}
			printf("--- Client mode ---\n");
			// Signal the parent process by incrementing the semaphore value
			sem_op.sem_num = 0;
			sem_op.sem_op = 1;
			sem_op.sem_flg = 0;
			if (semop(sem_id, &sem_op, 1) == -1)
			{
				fprintf(stderr, "Error: "
						"Unable to increment semaphore. %s\n", strerror(errno));
				return EXIT_FAILURE_SEM_OPERATION;
			}
			printf("Client: Signaled the server.\n");
		}
		else
		{
			fprintf(stderr, "Error: "
					"Unable to open semaphore. %s\n", strerror(errno));
			return EXIT_FAILURE_SEM_OPEN;
		}
	}
	else
	{
		printf("--- Server mode ---\n");
		if (semctl(sem_id, 0, SETVAL, sem_value) == -1) // Set sem value to 0
		{
			fprintf(stderr, "Error: "
					"Unable to set the semaphore value. %s\n", strerror(errno));
			return EXIT_FAILURE_SEM_OPERATION;
		}
		// Wait for the client process to signal using the semaphore
		sem_op.sem_num = 0;
		sem_op.sem_op = -1;
		sem_op.sem_flg = 0;

		// Keep waiting until the semaphore is signaled or a timeout occurs
		while (semop(sem_id, &sem_op, 1) == -1 && !timed_out)
		{
			if (errno == EINTR)
			{
				// The semop was interrupted by alarm signal, continue waiting
				continue;
			}
			else
			{
				fprintf(stderr, "Error: "
						"Semaphore wait failed. %s\n", strerror(errno));
				return EXIT_FAILURE_SEM_WAIT;
			}
		}

		// Remove the semaphore
		if (semctl(sem_id, 0, IPC_RMID) == -1)
		{
			fprintf(stderr, "Error: "
					"Remove of the semaphore failed. %s\n", strerror(errno));
			return EXIT_FAILURE_SEM_REMOVE;
		}
		remove(file_name);
	}

	if (timed_out)
	{
		fprintf(stderr, "Error: Semaphore wait failed after %d seconds. "
				"%s\n", timeout_seconds, strerror(errno));
		return EXIT_FAILURE_SEM_WAIT;
	}
	else
	{

		remove(file_name);
		printf("Success!\n");
		return EXIT_SUCCESS;
	}
}
