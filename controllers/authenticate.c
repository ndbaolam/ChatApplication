//POST /sign-in
void handleSignIn(const char *request, const int client_fd)
{
  char username[256], password[256];
  char *body = strstr(request, "\r\n\r\n");
  if (body)
  {
    char response[1024] = {0};
    char cmd[1024];

    body += 4;
    char *token = strtok(body, "&");

    while (token != NULL)
    {
      if (strncmp(token, "username=", 9) == 0)
      {
        strncpy(username, token + 9, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
      }
      else if (strncmp(token, "password=", 9) == 0)
      {
        strncpy(password, token + 9, sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';
      }
      token = strtok(NULL, "&");
    }

    sprintf(cmd, "SELECT * FROM users WHERE username='%s' AND password='%s';", username, password);
    int result = ExecDBCommand(psql, cmd);
    if (result < 0)
    {
      perror("Database error");

      snprintf(response, sizeof(response) - 1,
               "HTTP/1.1 500 Internal Server Error\r\n"
               "Content-Type: application/json\r\n\r\n"
               "{\"error\": \"Internal Server Error\", \"message\": \"An unexpected error occurred on the server.\"}\r\n");
      send(client_fd, response, strlen(response), 0);
    }
    else if (result == 0)
    {
     snprintf(response, sizeof(response) - 1,
              "HTTP/1.1 401 Unauthorized\r\n"
              "Content-Type: text/html\r\n"
              "Content-Length: 0\r\n"
              "Location: /\r\n"
              "Connection: close\r\n\r\n");

      send(client_fd, response, strlen(response), 0);
    }
    else if (result == 1)
    {
      memset(cmd, 0, sizeof(cmd));

      sprintf(cmd, "UPDATE users SET is_online='true' WHERE username='%s';", username);
      result = ExecDBCommand(psql, cmd);

      char log[1024] = {0};
      sprintf(log, "USER %s HAS SIGNED IN", username);
      WriteLog(log);

      RedirectRespose("user-info", username, client_fd);      
    }
  }
  else
  {
    printf("Invalid POST body.\n");
  }
}

//GET /sign-out
void handleSignOut(const char *request, const int client_fd) {  
  // Get the "token" cookie from the request
  char *cookie = GetCookieFromRequest(request, "token");    

  char response[1024] = {0};
  char cmd[1024];

  if (!cookie) {
    snprintf(response, sizeof(response) - 1,
              "HTTP/1.1 401 Unauthorized\r\n"
              "Content-Type: text/html\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");
    send(client_fd, response, strlen(response), 0);
    return;
  }  

  //remove s\r\n at the end  
  cookie[sizeof(cookie) - 2]   = '\0';

  snprintf(cmd, sizeof(cmd) - 1, "UPDATE users SET is_online='false' WHERE username='%s';", cookie);
  int result = ExecDBCommand(psql, cmd);

  if (result == -1) {
    snprintf(response, sizeof(response) - 1,
              "HTTP/1.1 500 Internal Server Error\r\n"
              "Content-Type: text/html\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");
    send(client_fd, response, strlen(response), 0);
    return;
  }

  char log[1024] = {0};
  
  snprintf(log, sizeof(log), "USER %s HAS SIGNED OUT", cookie);
  WriteLog(log);

  snprintf(response, sizeof(response) - 1,
              "HTTP/1.1 302 Found\r\n"
              "Content-Type: application/json\r\n"
              "Set-Cookie: token=; expires=Thu, 01 Jan 1970 00:00:00 GMT; path=/; HttpOnly; Secure\r\n"
              "Location: /\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");

  send(client_fd, response, strlen(response), 0);
}

//POST /sign-in
void handleSignUp(const char *request, const int client_fd)
{
  char username[256], password[256];
  char *body = strstr(request, "\r\n\r\n");
  if (body)
  {
    char response[1024] = {0};
    char cmd[1024];

    body += 4;  // Skip the headers to get to the body
    char *token = strtok(body, "&");

    while (token != NULL)
    {
      if (strncmp(token, "username=", 9) == 0)
      {
        strncpy(username, token + 9, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
      }
      else if (strncmp(token, "password=", 9) == 0)
      {
        strncpy(password, token + 9, sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';
      }
      token = strtok(NULL, "&");
    }

    // Check if the username already exists
    sprintf(cmd, "SELECT * FROM users WHERE username='%s';", username);
    int result = ExecDBCommand(psql, cmd);
    if (result < 0)
    {
      perror("Database error");

      snprintf(response, sizeof(response) - 1,
               "HTTP/1.1 500 Internal Server Error\r\n"
               "Content-Type: application/json\r\n\r\n"
               "{\"error\": \"Internal Server Error\", \"message\": \"An unexpected error occurred on the server.\"}\r\n");
      send(client_fd, response, strlen(response), 0);
    }    
    else if (result == 1)
    {
      // Username already exists, send error
      snprintf(response, sizeof(response) - 1,
               "HTTP/1.1 409 Conflict\r\n"
               "Content-Type: application/json\r\n\r\n"
               "{\"error\": \"User already exists\", \"message\": \"The username you provided is already in use.\"}\r\n");
      send(client_fd, response, strlen(response), 0);
    }
    else
    {
      // Username does not exist, proceed to create new user
      sprintf(cmd, "INSERT INTO users (username, password) VALUES ('%s', '%s');", username, password);
      int tmp = ExecDBCommand(psql, cmd);
      if (tmp < 0)
      {
        snprintf(response, sizeof(response) - 1,
                 "HTTP/1.1 500 Internal Server Error\r\n"
                 "Content-Type: application/json\r\n\r\n"
                 "{\"error\": \"Internal Server Error\", \"message\": \"Failed to create user.\"}\r\n");
        send(client_fd, response, strlen(response), 0);
      }
      else
      {
        // User created successfully
        const char *message = "{\"message\": \"User created successfully.\"}";
        int content_length = strlen(message);

        snprintf(response, sizeof(response) - 1,
                "HTTP/1.1 201 Created\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n\r\n"
                "%s\r\n", content_length, message);
        send(client_fd, response, strlen(response), 0);

        char log[1024] = {0};
        sprintf(log, "USER %s HAS SIGNED UP", username);
        WriteLog(log);
      }
    }
  }
  else
  {
    printf("Invalid POST body.\n");
  }
}
