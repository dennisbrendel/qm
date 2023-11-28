/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright 2022 Dennis Brendel, Red Hat */

#include <sys/socket.h>
#include <sys/stat.h>        /* For mode constants */
#include <sys/un.h>

#include <fcntl.h>           /* For O_* constants */
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* simple AF_UNIX access test which can be used to test access e.g. through
 * container boundaries */

int main(int argc, char* argv[]) {

  int s, len, timeout;
  unsigned int t;
  char sock_name[NAME_MAX];
  struct sockaddr_un local, remote;

  if (argc > 1) {
    strncpy(sock_name, argv[1], NAME_MAX-1);
  } else {
    strcpy(sock_name, "sock_test");
  }
  if (argc > 2) {
    timeout = atoi(argv[2]);
  } else {
    timeout = 2;
  }

  printf("Socket name: %s\n", sock_name);

  s = socket(AF_UNIX, SOCK_STREAM, 0);
  if (s < 0) {
    perror("socket");
    return 1;
  }

  local.sun_family = AF_UNIX;
  local.sun_path[0] = '\0';
  strcpy(local.sun_path + 1, sock_name);
  len = strlen(local.sun_path + 1) + 1 + sizeof(local.sun_family);

  if (bind(s, (struct sockaddr *)&local, len) == -1) {
    printf("-- Client mode --\n");
    if (connect(s, (struct sockaddr *)&local, len) == -1) {
      fprintf(stderr, "Failed to connect to already open socket %s!\n", sock_name);
      return 1;
    } else {
      printf("Connection successful!\n");
      close(s);
      return 0;
    }

  } else {
    printf("-- Server mode --\n");
    if (listen(s, 4) == -1) {
      perror("listen");
      return 1;
    }

    struct pollfd fds;
    fds.fd = s;
    fds.events = POLLIN;

    if (poll(&fds, 1, timeout * 1000) == 0) {
      fprintf(stderr, "Timeout!\n");
      close(s);
      return 1;
    }

    t = sizeof(remote);
    if (accept(s, (struct sockaddr *)&remote, &t) == -1) {
      perror("accept");
      return 1;
    }

    printf("Connection successful!\n");
    close(s);
    return 0;
  }

  return 0;
}

