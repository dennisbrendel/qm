/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2023 Dennis Brendel, Red Hat
   Copyright 2023 Polina Agranat, Red Hat */

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

enum {
    EXIT_FAILURE_SEM_OPEN,
    EXIT_FAILURE_SEM_TIMEDWAIT,
    EXIT_FAILURE_SEM_UNLINK,
    EXIT_FAILURE_INVALID_ARGUMENTS
};

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Error: Invalid number of arguments. Usage: %s <semaphore-name> <timeout-seconds>\n", argv[0]);
        return EXIT_FAILURE_INVALID_ARGUMENTS;
    }

    const int value = 0;
    const char *name = argv[1]; // Get the semaphore name from command-line argument
    char *err = NULL; // Used to check for conversion errors
    long timeout_seconds = strtol(argv[2], &err, 10); // Get the timeout value from command-line argument 
    sem_t *sem = NULL;

    // Check for conversion errors
    if (*err != '\0' || errno == ERANGE)
    {
        fprintf(stderr, "Error: Invalid timeout value. Usage: %s <semaphore-name> <timeout-seconds>\n", argv[0]);
        return EXIT_FAILURE_INVALID_ARGUMENTS;
    }

    sem = sem_open(name, O_CREAT | O_EXCL, 0600, value);
    if (sem == SEM_FAILED)
    {
        // semaphore already exists. Let's try to open it and then run as a client
        if (errno == EEXIST)
        {
            sem = sem_open(name, O_RDWR);
            if (sem == SEM_FAILED)
            {
                fprintf(stderr, "Error: Unable to open semaphore. %s\n", strerror(errno));
                return EXIT_FAILURE_SEM_OPEN;
            }
        }

        printf("--- Client mode ---\n");

        sem_post(sem);
    }
    else
    {
        printf("--- Server mode ---\n");

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_seconds; // Wait for up to provided timeout in sec

        int sem_wait_result = sem_timedwait(sem, &ts);

        if (sem_wait_result == 0)
        {
            // Semaphore was successfully acquired
            if (sem_unlink(name) != 0)
            {
                fprintf(stderr, "Error: Unable to unlink semaphore. %s\n", strerror(errno));
                return EXIT_FAILURE_SEM_UNLINK;
            }
        }
        else if (sem_wait_result == -1 && errno == ETIMEDOUT)
        {
            fprintf(stderr, "Error: Timed out waiting for semaphore. %s \n", strerror(errno));
            sem_unlink(name);
            return EXIT_FAILURE_SEM_TIMEDWAIT;
        }
        else
        {
            // Other error occurred
            fprintf(stderr, "Error: Semaphore wait failed. %s\n", strerror(errno));
            return EXIT_FAILURE_SEM_TIMEDWAIT;
        }
    }

    printf("Success!\n");

    return EXIT_SUCCESS;
}
