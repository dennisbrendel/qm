/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2023 Polina Agranat, Red Hat
   Copyright 2023 Dennis Brendel, Red Hat */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#define PROJECT_ID 'P'
#define QUEUE_PERMISSIONS 0660

struct message_text {
	int qid;
	char buf [200];
};

struct message
{
	long message_type;
	struct message_text message_text;
};

enum {
	EXIT_SUCCESS_SERVER,
	EXIT_SUCCESS_FILE_CREATE,
	EXIT_SUCCESS_KEY_GEN,
	EXIT_FAILURE_FILE_CREATE,
	EXIT_FAILURE_FILE_REMOVE,
	EXIT_FAILURE_KEY_GEN,
	EXIT_FAILURE_GET_CLIENT_QID,
	EXIT_FAILURE_GET_SERVER_QID,
	EXIT_FAILURE_SEND_MSG,
	EXIT_FAILURE_SEND_RESPONSE,
	EXIT_FAILURE_RECEIVE_RESPONSE,
	EXIT_FAILURE_RECEIVE_CLIENT_MSG,
	EXIT_FAILURE_TIMED_OUT,
	EXIT_FAILURE_OPEN_QUEUE,
	EXIT_FAILURE_INVALID_ARGUMENTS
};

volatile sig_atomic_t timed_out = 0;

void handleTimeout()
{
	timed_out = 1;
}

void cleanup(int qid)
{
	if (msgctl(qid, IPC_RMID, NULL) == -1) {
		printf("cleanup function: qid= %d \n", qid);
		fprintf(stderr, "Error: Unable to remove qid %s\n",strerror(errno));
	}
}

int createEmptyFile(const char *file_name)
{
	FILE *file = fopen(file_name, "w");
	if (file == NULL) {
		fprintf(stderr, "Error: Unable to create a file %s\n", strerror(errno));
		return EXIT_FAILURE_FILE_CREATE;
	}
	fclose(file);
	return EXIT_SUCCESS_FILE_CREATE;
}

int clientPart(key_t msg_queue_key, char *message_to_send)
{
	int server_qid = -1;
	int client_qid = -1;
	struct message my_message;
	struct message return_message;

	printf ("I'm a client \n");

	// create the client queue for receiving messages from server
	if ((client_qid = msgget(IPC_PRIVATE, 0660)) == -1) {
		fprintf(stderr, "Error: Unable to create client queue"
				"%s\n", strerror(errno));
		return EXIT_FAILURE_GET_CLIENT_QID;
	}

	// get the server queue to address the messages
	if ((server_qid = msgget(msg_queue_key, 0)) == -1) {
		fprintf(stderr, "Error: Unable to get server queue ID %s\n",
				strerror(errno));
		return EXIT_FAILURE_GET_SERVER_QID;
	}

	// structure the message to send to the server
	strcpy(my_message.message_text.buf, message_to_send);
	my_message.message_type = 1;
	my_message.message_text.qid = client_qid;

	// send message to server
	if (msgsnd(server_qid, &my_message, sizeof(
					struct message_text), 0) == -1) {
		fprintf(stderr, "Error: Failed to send message to server %s\n",
				strerror(errno));
		return EXIT_FAILURE_SEND_MSG;
	}
	printf("Client: sent message to server: %s\n", my_message.message_text.buf);

	// read response from server
	if (msgrcv(client_qid, &return_message, sizeof(
					struct message_text), 0, 0) == -1) {
		fprintf(stderr, "Error: Failed to receive response from server "
				"%s\n", strerror(errno));
		return EXIT_FAILURE_RECEIVE_RESPONSE;
	}
	printf("Client: response received from server: %s \n",
			return_message.message_text.buf);

	return EXIT_SUCCESS;
}

int serverPart(int timeout_server, int server_qid)
{
	int received_client_qid = -1;
	int message_len;
	struct message message;
	char msg_length_buffer[20];

	printf("I'm a server \n");

	signal(SIGALRM, handleTimeout);
	alarm(timeout_server);

	while (!timed_out) {
		// read an incoming message
		if (msgrcv(server_qid, &message, sizeof(
				struct message_text), 0, 0) == -1) {
			fprintf(stderr, "Error:Timed out getting incoming messages after"
					" %d seconds. %s\n", timeout_server, strerror(errno));
			cleanup(server_qid);
			return EXIT_FAILURE_TIMED_OUT;
		}

		printf("Server: message received: %s\n", message.message_text.buf);

		// process message
		message_len = strlen(message.message_text.buf);
		sprintf (msg_length_buffer, " %d", message_len);
		// concatenating the message text and the length to send as a response
		strcat(message.message_text.buf, msg_length_buffer);
		received_client_qid = message.message_text.qid;

		// send response message to client
		if (msgsnd(received_client_qid, &message, sizeof(
				struct message_text),0) == -1) {
			fprintf(stderr, "Error: Failed sending response to client %s\n",
					strerror(errno));
			cleanup(received_client_qid);
			return EXIT_FAILURE_SEND_RESPONSE;
		}
		printf("Server: the response sent to client.\n");
		cleanup(received_client_qid);
		cleanup(server_qid);
		exit(EXIT_SUCCESS_SERVER);
	}
	return EXIT_SUCCESS;
}

int parseArguments(
		int argc, char *argv[], int *timeout_seconds, char **message_to_send)
{
	char *err = NULL;
	int provided_timeout;

	if (argc == 1) {
		*timeout_seconds = 5;
		*message_to_send = "This is a client-to-server message";
	}
	else if (argc == 3) {
		provided_timeout = strtol(argv[1], &err, 10);
		*message_to_send = argv[2];
		if (*err == '\0' && errno != ERANGE && provided_timeout > 0) {
			*timeout_seconds = provided_timeout;
		} else {
			fprintf(stderr, "Error: Invalid timeout value. Usage: %s "
					"[timeout-seconds] [message_to_send]\n", argv[0]);
			return EXIT_FAILURE_INVALID_ARGUMENTS;
		}
	}
	else {
		fprintf(stderr, "Error: Invalid number of arguments. Usage: %s "
				"[timeout-seconds] [message_to_send]\n", argv[0]);
		return EXIT_FAILURE_INVALID_ARGUMENTS;
	}

	return EXIT_SUCCESS;
}

int generateMsgQKey (const char *file_name, key_t *msg_queue_key)
{
	int return_value = EXIT_SUCCESS_KEY_GEN;

	// Create an empty file in the current directory
	if (createEmptyFile(file_name) != EXIT_SUCCESS_FILE_CREATE) {
		return EXIT_FAILURE_FILE_CREATE;
	}

	if ((*msg_queue_key = ftok (file_name, PROJECT_ID)) == -1) {
			fprintf(stderr, "Error: Unable to generate a key for queue %s\n",
					strerror(errno));
			remove(file_name);
			return_value = EXIT_FAILURE_KEY_GEN;
		}
	return return_value;
}

int main (int argc, char *argv[])
{
	key_t msg_queue_key;
	int qid = -1;
	int timeout_seconds;
	int communication_result = EXIT_SUCCESS;
	char *message_to_send;
	const char file_name[] = "msg_queue_server_key";

	if (parseArguments(
			argc, argv, &timeout_seconds, &message_to_send) != EXIT_SUCCESS) {
		return EXIT_FAILURE_INVALID_ARGUMENTS;
	}

	if (generateMsgQKey(file_name, &msg_queue_key) != EXIT_SUCCESS_KEY_GEN) {
		return EXIT_FAILURE_KEY_GEN;
	}

	qid = msgget(msg_queue_key, IPC_CREAT | IPC_EXCL | QUEUE_PERMISSIONS);

	if (qid == -1) {
		if (errno == EEXIST) {
			//client part
			communication_result = clientPart(msg_queue_key, message_to_send);
		} else {
			fprintf(stderr, "Error: Unable to open queue. %s\n",
					strerror(errno));
			communication_result = EXIT_FAILURE_OPEN_QUEUE;
		}
	} else {
		//server part
		communication_result = serverPart(timeout_seconds, qid);
	}

	remove(file_name);
	return communication_result;
}
