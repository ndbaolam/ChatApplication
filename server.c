#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include "helpers/main.h"
#include "controllers/main.h"

#define PORT 3000

int fd, N = 0;
sem_t mutex;
PGconn *psql;
pthread_t *tid = NULL;

void signal_handler(int signum)
{
  if (signum == SIGINT)
  {
    printf("\nCaught signal %d (SIGINT). Exiting gracefully...\n", signum);
    close(fd);
    if (psql != NULL)
    {
      PQfinish(psql);
      printf("Disconnected from the database.\n");
    }

    sem_destroy(&mutex);
    exit(EXIT_SUCCESS);
  }
}

void *handle_client(void *args)
{
  int c = *((int *)args);
  free(args);

  char request[4096] = {0};
  int bytes_received;

  while ((bytes_received = recv(c, request, sizeof(request) - 1, 0)) > 0)
  {
    request[bytes_received] = '\0';

    char method[64], path[1024], version[64];

    if (sscanf(request, "%s %s %s", method, path, version) == 3)
    {
      printf("Method: %s, Path: %s, Version: %s\n", method, path, version);
    }
    else
    {
      perror("Error parsing HTTP request.");
      continue;
    }

    // GET /
    if (strcmp(path, "/") == 0 && strcmp(method, "GET") == 0)
    {
      handleDashBoard(request, c);
    }

    // POST /sign-in
    if (strcmp(path, "/sign-in") == 0 && strcmp(method, "POST") == 0)
    {
      handleSignIn(request, c);
    }

    // POST /sign-up
    if (strcmp(path, "/sign-up") == 0 && strcmp(method, "POST") == 0)
    {
      handleSignUp(request, c);
    }

    // POST /sign-out
    if (strcmp(path, "/sign-out?") == 0 && strcmp(method, "GET") == 0)
    {
      handleSignOut(request, c);
    }

    // GET /user-info
    if (strcmp(path, "/user-info") == 0 && strcmp(method, "GET") == 0)
    {
      handleUser(request, c);
    }

    // POST /user-info
    if (strcmp(path, "/user-info") == 0 && strcmp(method, "POST") == 0)
    {
      handleGetUserInfo(request, c);
    }

    // POST /send-request
    if (strcmp(path, "/send-request") == 0 && strcmp(method, "POST") == 0)
    {
      handleSendAddFriendRequest(request, c);
    }

    // POST /accept-request
    if (strcmp(path, "/accept-request") == 0 && strcmp(method, "POST") == 0)
    {
      handleAcceptAddFriendRequest(request, c);
    }

    // POST /decline-request
    if (strcmp(path, "/decline-request") == 0 && strcmp(method, "POST") == 0)
    {
      handleDeclineAddFriendRequest(request, c);
    }

    // POST /remove-friend
    if (strcmp(path, "/remove-friend") == 0 && strcmp(method, "POST") == 0)
    {
      handleRemoveFriend(request, c);
    }

    // GET /chat-user
    if (strncmp(path, "/chat-user/", 11) == 0 && strcmp(method, "GET") == 0)
    {
      handleChatUser(request, c);
    }

    // POST /chat-user
    if (strncmp(path, "/chat-user/", 11) == 0 && strcmp(method, "POST") == 0)
    {
      sendMessageToUser(request, c);
    }

    // GET /message/:user
    if (strncmp(path, "/message/", 9) == 0 && strcmp(method, "GET") == 0)
    {
      char *receiver = path + 9;
      receiver[sizeof(receiver)] = '\0';
      getAllMessages(request, c, receiver);
    }

    // POST /create-group
    if (strcmp(path, "/create-group") == 0 && strcmp(method, "POST") == 0)
    {
      createGroupChat(request, c);
    }

    // POST /add-user-to-group
    if (strcmp(path, "/add-user-to-group") == 0 && strcmp(method, "POST") == 0)
    {
      addUserToGroup(request, c);
    }

    // POST /remove-user-from-group
    if (strcmp(path, "/remove-user-from-group") == 0 && strcmp(method, "POST") == 0)
    {
      removeUserFromGroup(request, c);
    }

    Serve404(c);
    memset(request, 0, sizeof(request));
  }

  close(c);
  pthread_exit(NULL);
}

int main()
{
  struct sockaddr_in saddr, caddr;
  socklen_t clen = sizeof(caddr);

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
  {
    perror("socket()");
    exit(EXIT_FAILURE);
  }

  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = INADDR_ANY;
  saddr.sin_port = htons(PORT);

  if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
  {
    perror("bind()");
    close(fd);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d...\n", PORT);

  if (listen(fd, 10) < 0)
  {
    perror("listen()");
    close(fd);
    exit(EXIT_FAILURE);
  }

  // Connect to Database
  psql = ConnectToDB("chat_application", "postgres", "password", "localhost", 5432);
  signal(SIGINT, signal_handler);

  while (1)
  {
    int cd = accept(fd, (struct sockaddr *)&caddr, &clen);
    if (cd < 0)
    {
      perror("accept()");
      close(cd);
      continue;
    }

    int *arg = malloc(sizeof(int));
    if (!arg)
    {
      perror("malloc()");
      close(cd);
      continue;
    }
    *arg = cd;

    tid = realloc(tid, (N + 1) * sizeof(pthread_t));

    if (pthread_create(&tid[N], NULL, handle_client, arg) != 0)
    {
      perror("pthread_create()");
      free(arg);
      close(cd);
      continue;
    }

    N++;
  }

  return 0;
}
