// GET /user-info
void handleUser(const char *request, const int client_fd)
{
  char *cookie = GetCookieFromRequest(request, "token");

  if (cookie)
    ServeStaticFile("ui/user.html", client_fd);
  else
    RedirectResponse("/", NULL, client_fd);
}

// POST /user-info
void handleGetUserInfo(const char *request, const int client_fd)
{
  char *cookie = GetCookieFromRequest(request, "token");

  if (!cookie)
  {
    RedirectResponse("/", NULL, client_fd);
    return;
  }

  cookie[sizeof(cookie) - 2] = '\0';

  // Query user information
  const char *current_username = cookie;
  if (!current_username)
  {
    RedirectResponse("/", NULL, client_fd);
    return;
  }

  char response[4096] = {0};
  char json[2048] = {0};

  strcat(json, "{\n");

  // Query online users
  const char *online_query = "SELECT username FROM users WHERE is_online = TRUE AND username != $1";
  const char *params[1] = {current_username};
  PGresult *online_res = PQexecParams(psql, online_query, 1, NULL, params, NULL, NULL, 0);

  if (PQresultStatus(online_res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Error executing online users query: %s\n", PQerrorMessage(psql));
    PQclear(online_res);
    snprintf(response, sizeof(response) - 1,
             "HTTP/1.1 500 Internal Server Error\r\n"
             "Content-Type: application/json\r\n\r\n"
             "{\"error\": \"Internal Server Error\", \"message\": \"An unexpected error occurred on the server.\"}\r\n");
    send(client_fd, response, strlen(response), 0);
    return;
  }

  // Process online users
  int online_user_count = PQntuples(online_res);
  snprintf(json + strlen(json), sizeof(json) - strlen(json), "\"total_online\": %d, \"online_user\": \"", online_user_count);

  for (int i = 0; i < online_user_count; i++)
  {
    strcat(json, PQgetvalue(online_res, i, 0));
    if (i < online_user_count - 1)
    {
      strcat(json, ", ");
    }
  }
  strcat(json, "\",\n");
  PQclear(online_res);

  // Query friend requests
  const char *requests_query = "SELECT users.username FROM friend_requests "
                               "JOIN users ON friend_requests.sender_id = users.user_id "
                               "WHERE friend_requests.receiver_id = (SELECT user_id FROM users WHERE username = $1) "
                               "AND friend_requests.status = 'pending'";
  PGresult *requests_res = PQexecParams(psql, requests_query, 1, NULL, params, NULL, NULL, 0);

  if (PQresultStatus(requests_res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Error executing friend requests query: %s\n", PQerrorMessage(psql));
    PQclear(requests_res);
    snprintf(response, sizeof(response) - 1,
             "HTTP/1.1 500 Internal Server Error\r\n"
             "Content-Type: application/json\r\n\r\n"
             "{\"error\": \"Internal Server Error\", \"message\": \"An unexpected error occurred on the server.\"}\r\n");
    send(client_fd, response, strlen(response), 0);
    return;
  }

  // Process friend requests
  int total_requests = PQntuples(requests_res);
  snprintf(json + strlen(json), sizeof(json) - strlen(json), "\"total_request\": %d, \"sender\": \"", total_requests);

  for (int i = 0; i < total_requests; i++)
  {
    strcat(json, PQgetvalue(requests_res, i, 0));
    if (i < total_requests - 1)
    {
      strcat(json, ", ");
    }
  }
  strcat(json, "\",\n");
  PQclear(requests_res);

  // Query user's friends
  const char *friends_query = "SELECT users.username FROM friends "
                              "JOIN users ON friends.friend_user_id = users.user_id "
                              "WHERE friends.user_id = (SELECT user_id FROM users WHERE username = $1)";
  PGresult *friends_res = PQexecParams(psql, friends_query, 1, NULL, params, NULL, NULL, 0);

  if (PQresultStatus(friends_res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Error executing friends query: %s\n", PQerrorMessage(psql));
    PQclear(friends_res);
    snprintf(response, sizeof(response) - 1,
             "HTTP/1.1 500 Internal Server Error\r\n"
             "Content-Type: application/json\r\n\r\n"
             "{\"error\": \"Internal Server Error\", \"message\": \"An unexpected error occurred on the server.\"}\r\n");
    send(client_fd, response, strlen(response), 0);
    return;
  }

  // Process friends list
  int total_friends = PQntuples(friends_res);
  snprintf(json + strlen(json), sizeof(json) - strlen(json), "\"total_friend\": %d, \"friends\": \"", total_friends);

  for (int i = 0; i < total_friends; i++)
  {
    strcat(json, PQgetvalue(friends_res, i, 0));
    if (i < total_friends - 1)
    {
      strcat(json, ", ");
    }
  }
  strcat(json, "\"\n");

  PQclear(friends_res);

  // Closing the JSON object
  strcat(json, "}");

  // Prepare and send HTTP response
  snprintf(response, sizeof(response) - 1,
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: %lu\r\n\r\n"
           "%s",
           strlen(json), json);

  send(client_fd, response, strlen(response), 0);
}
