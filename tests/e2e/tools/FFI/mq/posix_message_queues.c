/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2023 Polina Agranat, Red Hat */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <time.h>

#define SERVER_QUEUE_NAME "/server-queue"
#define CLIENT_QUEUE_NAME "/client-queue"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256
#define MSG_BUFFER_SIZE MAX_MSG_SIZE + 10

enum {
	EXIT_SUCCESS_CLEANUP,
	EXIT_FAILURE_SEND_MSG,
	EXIT_FAILURE_SEND_RESPONSE,
	EXIT_FAILURE_RECEIVE_RESPONSE,
	EXIT_FAILURE_RECEIVE_CLIENT_MSG,
	EXIT_FAILURE_TIMED_OUT,
	EXIT_FAILURE_OPEN_QUEUE,
	EXIT_FAILURE_QUEUE_CLOSE,
	EXIT_FAILURE_QUEUE_UNLINK,
	EXIT_FAILURE_CLEANUP
};

int cleanupQueue (mqd_t mq_descriptor, char *queue_name)
{
	if (mq_close(mq_descriptor) == -1) {
		fprintf(stderr, "Cleanup Error: failed closing queue descriptor"
						" %s\n", strerror(errno));
		return EXIT_FAILURE_QUEUE_CLOSE;
	}

	if (mq_unlink(queue_name) == -1) {
		fprintf(stderr, "Cleanup Error: failed to unlink queue %s %s\n",
				queue_name, strerror(errno));
		return EXIT_FAILURE_QUEUE_UNLINK;;
	}
	return EXIT_SUCCESS_CLEANUP;
}

int clientPart(struct mq_attr attr)
{
	mqd_t qd_server;
	mqd_t qd_client;
	char in_buffer[MSG_BUFFER_SIZE];
	char message_to_server[] = "Hello, Server!";

	printf ("I'm a client \n");

	// open client queue
	if ((qd_client = mq_open(
			CLIENT_QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)
			) == -1) {
		fprintf(stderr, "Client Error: failed to open client queue "
				" %s\n", strerror(errno));
		return EXIT_FAILURE_OPEN_QUEUE;
	}

	// open server queue
	if ((qd_server = mq_open(SERVER_QUEUE_NAME, O_WRONLY)) == -1) {
		fprintf(stderr, "Client Error: failed to open server queue "
						" %s\n", strerror(errno));
		return EXIT_FAILURE_OPEN_QUEUE;
	}

	// Send the message to the server
	if (mq_send(qd_server, message_to_server,
			strlen(message_to_server) + 1, 0) == -1) {
		fprintf(stderr, "Client Error: failed sending message to server"
							" %s\n", strerror(errno));
		return EXIT_FAILURE_SEND_MSG;
	}
	printf("Client: the message sent to the server: %s\n", message_to_server);

	// Receive response from the server
	if (mq_receive(qd_client, in_buffer, MSG_BUFFER_SIZE, NULL) == -1) {
		fprintf(stderr, "Client Error: failed to receive response from server "
				"%s\n", strerror(errno));
		return EXIT_FAILURE_RECEIVE_RESPONSE;
	}
	printf("Client: received response from server: %s\n", in_buffer);

	if (cleanupQueue(qd_client, CLIENT_QUEUE_NAME) != EXIT_SUCCESS_CLEANUP) {
		return EXIT_FAILURE_CLEANUP;
	}

	printf ("Client: bye\n");

	return EXIT_SUCCESS;
}

int serverPart(mqd_t queue_descr)
{
	int timeout_seconds = 5;
	mqd_t qd_client;
	char response_message[] = "Hello, Client!";
	char in_buffer [MSG_BUFFER_SIZE];
	struct timespec abs_timeout;

	printf ("I'm a server \n");

	clock_gettime(CLOCK_REALTIME, &abs_timeout);
	abs_timeout.tv_sec += timeout_seconds;// wait for up to timeout_seconds

	while (mq_timedreceive(
			queue_descr, in_buffer, MSG_BUFFER_SIZE, NULL, &abs_timeout) == -1) {
		if (errno == ETIMEDOUT) {
			fprintf(stderr, "Server Error: timed out getting incoming message "
					"after %d sec. %s\n", timeout_seconds, strerror(errno));
			cleanupQueue(queue_descr, SERVER_QUEUE_NAME);
			return EXIT_FAILURE_TIMED_OUT;
		} else if (errno != EINTR) {
			fprintf(stderr, "Server Error: failed receiving client message "
					"%s\n", strerror(errno));
			return EXIT_FAILURE_RECEIVE_CLIENT_MSG;
		}
	}

	// open client queue for sending response
	if ((qd_client = mq_open(CLIENT_QUEUE_NAME, O_RDWR)) == -1) {
		fprintf(stderr, "Server Error: failed to open client queue for "
				"sending reply %s\n", strerror(errno));
		return EXIT_FAILURE_OPEN_QUEUE;
	}

	// send reply message to client
	if (mq_send(qd_client, response_message, strlen(
			response_message) + 1, 0) == -1) {
		fprintf(stderr, "Server Error: failed sending response to client"
				" %s\n", strerror(errno));
		return EXIT_FAILURE_SEND_RESPONSE;
	}
	printf("Server: sent response to client %s \n", response_message);

	if (cleanupQueue(queue_descr, SERVER_QUEUE_NAME) != EXIT_SUCCESS_CLEANUP) {
		return EXIT_FAILURE_CLEANUP;
	}

	return EXIT_SUCCESS;
}

int main ()
{
	mqd_t qd_server = -1;
	int communication_result = EXIT_SUCCESS;
	struct mq_attr attr;

	attr.mq_flags = 0;
	attr.mq_maxmsg = MAX_MESSAGES;
	attr.mq_msgsize = MAX_MSG_SIZE;
	attr.mq_curmsgs = 0;

	qd_server = mq_open (SERVER_QUEUE_NAME, O_RDONLY | O_CREAT | O_EXCL,
			QUEUE_PERMISSIONS, &attr);

	if (qd_server == -1) {
		if (errno == EEXIST) {
			//client part
			communication_result = clientPart(attr);
		} else {
			fprintf(stderr, "Error: Unable to open queue. %s\n",
					strerror(errno));
			communication_result = EXIT_FAILURE_OPEN_QUEUE;
		}
	} else {
		//server part
		communication_result = serverPart(qd_server);
	}

	return communication_result;
}
