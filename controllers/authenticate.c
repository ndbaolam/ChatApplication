// POST /sign-in
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

    sprintf(cmd, "SELECT user_id FROM users WHERE username='%s' AND password='%s';", username, password);
    PGresult *res = PQexec(psql, cmd);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
    {
      // Invalid login
      char *htmlServe = LoadContentFile("ui/401.html");
      if (htmlServe != NULL)
      {
        // Calculate content length
        int content_length = strlen(htmlServe);

        // Allocate a buffer for the entire HTTP response
        char response[4096];

        // Build the HTTP response headers
        int header_length = snprintf(response, sizeof(response),
                                     "HTTP/1.1 401 Unauthorized\r\n"
                                     "Content-Type: text/html\r\n"
                                     "Content-Length: %d\r\n"
                                     "Connection: close\r\n\r\n",
                                     content_length);

        // Check if the response buffer can hold the headers and HTML content
        if (header_length + content_length < sizeof(response))
        {
          // Append the HTML content to the response
          strcpy(response + header_length, htmlServe);

          // Send the response over the socket
          send(client_fd, response, header_length + content_length, 0);
        }
        else
        {
          fprintf(stderr, "Response buffer too small to fit headers and content.\n");
        }

        free(htmlServe); // Free the HTML content buffer
      }
      PQclear(res);
      return;
    }

    // Get user_id
    int user_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    // Update user's online status
    snprintf(cmd, sizeof(cmd), "UPDATE users SET is_online='true' WHERE username='%s';", username);
    int result = ExecDBCommand(psql, cmd);

    if (result < 0)
    {
      perror("Failed to update user status");
      snprintf(response, sizeof(response),
               "HTTP/1.1 500 Internal Server Error\r\n"
               "Content-Type: application/json\r\n\r\n"
               "{\"error\": \"Internal Server Error\", \"message\": \"An unexpected error occurred on the server.\"}\r\n");
      send(client_fd, response, strlen(response), 0);
      return;
    }

    // Add user to online users
    add_online_user(user_id, client_fd);

    // Log the sign-in event
    char log[1024] = {0};
    snprintf(log, sizeof(log), "USER %s HAS SIGNED IN", username);
    WriteLog(log);

    // Redirect to user-info
    RedirectResponse("user-info", username, client_fd);
  }
  else
  {
    printf("Invalid POST body.\n");
  }
}

// GET /sign-out
void handleSignOut(const char *request, const int client_fd)
{
  char response[1024] = {0};
  char cmd[1024] = {0};

  // Duplicate request for safe parsing
  char *dup_req = strdup(request);
  if (!dup_req)
  {
    // Memory allocation failure
    snprintf(response, sizeof(response),
             "HTTP/1.1 500 Internal Server Error\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: 0\r\n"
             "Connection: close\r\n\r\n");
    send(client_fd, response, strlen(response), 0);
    return;
  }

  // Find the token
  char *cookie = strstr(dup_req, "token=");
  if (!cookie)
  {
    free(dup_req);
    RedirectResponse("/", NULL, client_fd);
    return;
  }

  // Extract and sanitize token
  char *safe_cookie = cookie + 6; // Skip "token="
  safe_cookie[strcspn(safe_cookie, "\r\n")] = '\0';

  printf("DEBUG: Token received: %s\n", safe_cookie);

  // Update user status in the database
  snprintf(cmd, sizeof(cmd), "UPDATE users SET is_online='false' WHERE username='%s';", safe_cookie);
  int result = ExecDBCommand(psql, cmd);

  if (result == -1)
  {
    perror("Database update failed");
    snprintf(response, sizeof(response),
             "HTTP/1.1 500 Internal Server Error\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: 0\r\n"
             "Connection: close\r\n\r\n");
    send(client_fd, response, strlen(response), 0);
    free(dup_req);
    return;
  }

  // Remove user from online users array
  int user_id = GetUserIDByUsername(psql, safe_cookie);
  if (user_id != -1)
  {
    remove_online_user(user_id);
  }

  // Log the sign-out event
  char log[1024];
  snprintf(log, sizeof(log), "USER %s HAS SIGNED OUT", safe_cookie);
  WriteLog(log);

  // Respond to client with a redirect and cookie clearance
  snprintf(response, sizeof(response),
           "HTTP/1.1 302 Found\r\n"
           "Content-Type: application/json\r\n"
           "Set-Cookie: token=; expires=Thu, 01 Jan 1970 00:00:00 GMT; path=/; HttpOnly; Secure\r\n"
           "Location: /\r\n"
           "Content-Length: 0\r\n"
           "Connection: close\r\n\r\n");

  send(client_fd, response, strlen(response), 0);

  // Free allocated memory
  free(dup_req);
}

// POST /sign-up
void handleSignUp(const char *request, const int client_fd)
{
  char username[256], password[256];
  char *body = strstr(request, "\r\n\r\n");
  if (body)
  {
    char response[1024] = {0};
    char cmd[1024];

    body += 4; // Skip the headers to get to the body
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
                 "%s\r\n",
                 content_length, message);
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
