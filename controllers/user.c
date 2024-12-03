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
  char *dup_req = strdup(request);
  char *cookie = strstr(request, "token=");  
  if (!cookie)
  {
    RedirectResponse("/", NULL, client_fd);
    return;
  }

  char *safe_cookie = cookie + 6;
  safe_cookie[strcspn(safe_cookie, "\r\n")] = '\0';  

  // Query user information
  const char *current_username = safe_cookie;
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
  snprintf(json + strlen(json), sizeof(json) - strlen(json), "\"total_online\": %d, \"online_users\": \"", online_user_count);

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
  snprintf(json + strlen(json), sizeof(json) - strlen(json), "\"total_request\": %d, \"senders\": \"", total_requests);

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
  const char *friends_query =   "SELECT u2.username FROM friends "
                                "JOIN users u1 ON u1.user_id = friends.friend_user_id "
                                "JOIN users u2 ON u2.user_id = friends.user_id "
                                "WHERE u1.username = $1;";
  
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
  strcat(json, "\",\n");

  PQclear(friends_res);

  //Get all groupchat
  const char *groups_query =  "SELECT chat_groups.group_name FROM chat_groups "
                              "JOIN group_members ON group_members.group_id = chat_groups.group_id "                              
                              "JOIN users ON users.user_id = group_members.user_id "
                              "WHERE users.username = $1;";
  PGresult *groups_res = PQexecParams(psql, groups_query, 1, NULL, params, NULL, NULL, 0);

  if (PQresultStatus(groups_res) != PGRES_TUPLES_OK)
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

  int total_groups = PQntuples(groups_res);
  snprintf(json + strlen(json), sizeof(json) - strlen(json), "\"total_group\": %d, \"groups\": \"", total_groups);

  for (int i = 0; i < total_groups; i++)
  {
    strcat(json, PQgetvalue(groups_res, i, 0));
    if (i < total_groups - 1)
    {
      strcat(json, ", ");
    }
  }
  strcat(json, "\",\n");

  PQclear(groups_res);

  // Get username
  snprintf(json + strlen(json), sizeof(json) - strlen(json), "\"username\": \"%s\"\n", safe_cookie);

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

// POST /send-request
void handleSendAddFriendRequest(const char *request, const int client_fd)
{  
  char *dup_req = strdup(request);
  char *cookie = strstr(request, "token=");  
  if (!cookie)
  {
    RedirectResponse("/", NULL, client_fd);
    return;
  }

  char *safe_cookie = cookie + 6;
  safe_cookie[strcspn(safe_cookie, "\r\n")] = '\0';

  char query[512] = {0};
  snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", safe_cookie);
  PGresult *res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  int sender_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);
    
  char *body = strstr(dup_req, "username=");  
  if (!body)
  {
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }
    
  char user_received[32] = {0};
  if (strncmp(body, "username=", 9) == 0)
  {
    strncpy(user_received, body + 9, sizeof(user_received) - 1);
    user_received[sizeof(user_received) - 1] = '\0';
  }

  if (!user_received)
  {
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  memset(query, 0, sizeof(query));
  snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", user_received);  
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  int receiver_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  snprintf(query, sizeof(query),
           "INSERT INTO friend_requests (sender_id, receiver_id, status) "
           "VALUES (%d, %d, 'pending');",
           sender_id, receiver_id);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {    
    fprintf(stderr, "INSERT failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  PQclear(res);

  // Send success response
  char response[4096] = {0};
  char mess[512] = {0};
  snprintf(mess, sizeof(mess) - 1, "{\"message\": \"Friend request sent successfully.\"}");
  snprintf(response, sizeof(response) - 1,
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: %lu\r\n"
           "Connection: close\r\n\r\n"
           "%s",
           strlen(mess), mess);
  send(client_fd, response, strlen(response), 0);
}

// POST /accept-request
void handleAcceptAddFriendRequest(const char *request, const int client_fd)
{  
  char *dup_req = strdup(request);
  char *cookie = strstr(request, "token=");  
  if (!cookie)
  {
    RedirectResponse("/", NULL, client_fd);
    return;
  }

  char *safe_cookie = cookie + 6;
  safe_cookie[strcspn(safe_cookie, "\r\n")] = '\0';

  //Get current user_id
  char query[512] = {0};
  snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", safe_cookie);
  PGresult *res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  int receiver_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);
  
  //Get user_id sent request
  char *body = strstr(dup_req, "username=");  
  if (!body)
  {
    printf("Error 2\n");
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }
    
  char user_sent[32] = {0};
  if (strncmp(body, "username=", 9) == 0)
  {
    strncpy(user_sent, body + 9, sizeof(user_sent) - 1);
    user_sent[sizeof(user_sent) - 1] = '\0';
  }

  if (!user_sent)
  {
    printf("Error 3\n");
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  memset(query, 0, sizeof(query));
  snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", user_sent);  
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT failed: %s", PQerrorMessage(psql)); 
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  int sender_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Update friend_requests table
  memset(query, 0, sizeof(query));
  snprintf(query, sizeof(query),
            "UPDATE friend_requests SET status='accepted'"
            "WHERE sender_id='%d' AND receiver_id='%d';",
           sender_id, receiver_id);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "UPDATE failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  PQclear(res);

  // Insert into friends table
  memset(query, 0, sizeof(query));
  snprintf(query, sizeof(query),
            "INSERT INTO friends(user_id, friend_user_id)"
            "VALUES ('%d', '%d'), ('%d', '%d');",
           receiver_id, sender_id, sender_id, receiver_id);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "INSERT failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  PQclear(res);

  // Send success response
  char response[4096] = {0};
  char mess[512] = {0};
  snprintf(mess, sizeof(mess) - 1, "{\"message\": \"Friend request has been accepted successfully.\"}");
  snprintf(response, sizeof(response) - 1,
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: %lu\r\n"
           "Connection: close\r\n\r\n"
           "%s",
           strlen(mess), mess);
  send(client_fd, response, strlen(response), 0);
}

// POST /decline-request
void handleDeclineAddFriendRequest(const char *request, const int client_fd)
{  
  char *dup_req = strdup(request);
  char *cookie = strstr(request, "token=");  
  if (!cookie)
  {
    RedirectResponse("/", NULL, client_fd);
    return;
  }

  char *safe_cookie = cookie + 6;
  safe_cookie[strcspn(safe_cookie, "\r\n")] = '\0';

  //Get current user_id
  char query[512] = {0};
  snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", safe_cookie);
  PGresult *res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  int receiver_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);
  
  //Get user_id sent request
  char *body = strstr(dup_req, "username=");  
  if (!body)
  {
    printf("Error 2\n");
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }
    
  char user_sent[32] = {0};
  if (strncmp(body, "username=", 9) == 0)
  {
    strncpy(user_sent, body + 9, sizeof(user_sent) - 1);
    user_sent[sizeof(user_sent) - 1] = '\0';
  }

  if (!user_sent)
  {
    printf("Error 3\n");
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  memset(query, 0, sizeof(query));
  snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", user_sent);  
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  int sender_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Update friend_requests table
  memset(query, 0, sizeof(query));
  snprintf(query, sizeof(query),
            "UPDATE friend_requests SET status='declined'"
            "WHERE sender_id='%d' AND receiver_id='%d';",
           sender_id, receiver_id);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "UPDATE failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  PQclear(res);

  // Send success response
  char response[4096] = {0};
  char mess[512] = {0};
  snprintf(mess, sizeof(mess) - 1, "{\"message\": \"Friend request has been declined successfully.\"}");
  snprintf(response, sizeof(response) - 1,
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: %lu\r\n"
           "Connection: close\r\n\r\n"
           "%s",
           strlen(mess), mess);
  send(client_fd, response, strlen(response), 0);
}

// POST /remove-friend
void handleRemoveFriend(const char *request, const int client_fd)
{  
  char *dup_req = strdup(request);
  char *cookie = strstr(request, "token=");  
  if (!cookie)
  {
    RedirectResponse("/", NULL, client_fd);
    return;
  }

  char *safe_cookie = cookie + 6;
  safe_cookie[strcspn(safe_cookie, "\r\n")] = '\0';

  //Get current user_id
  char query[512] = {0};
  snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", safe_cookie);
  PGresult *res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "Select failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  int user_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);
  
  //Get user_id need to be removed
  char *body = strstr(dup_req, "username=");  
  if (!body)
  {
    printf("Error 2\n");
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }
    
  char user_sent[32] = {0};
  if (strncmp(body, "username=", 9) == 0)
  {
    strncpy(user_sent, body + 9, sizeof(user_sent) - 1);
    user_sent[sizeof(user_sent) - 1] = '\0';
  }

  if (!user_sent)
  {
    printf("Error 3\n");
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  memset(query, 0, sizeof(query));
  snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", user_sent);  
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "Select failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  int friend_user_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Update friends table
  memset(query, 0, sizeof(query));
  snprintf(query, sizeof(query),
          "DELETE FROM friends "
          "WHERE (user_id='%d' AND friend_user_id='%d') "
          "OR (user_id='%d' AND friend_user_id='%d');",
          user_id, friend_user_id, friend_user_id, user_id);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Delete failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  PQclear(res);

  memset(query, 0, sizeof(query));
  snprintf(query, sizeof(query),
          "DELETE FROM friend_requests "
          "WHERE (sender_id='%d' AND receiver_id='%d') "
          "OR (sender_id='%d' AND receiver_id='%d');",
          user_id, friend_user_id, friend_user_id, user_id);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "DELETE failed: %s", PQerrorMessage(psql));
    PQclear(res);
    char response[4096] = {0};
    char mess[512] = {0};
    snprintf(mess, sizeof(mess) - 1, "{\"error\": \"Invalid friend request data.\"}");
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(mess), mess);
    send(client_fd, response, strlen(response), 0);
    return;
  }

  PQclear(res);  

  // Send success response
  char response[4096] = {0};
  char mess[512] = {0};
  snprintf(mess, sizeof(mess) - 1, "{\"message\": \"Friend has been removed successfully.\"}");
  snprintf(response, sizeof(response) - 1,
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: %lu\r\n"
           "Connection: close\r\n\r\n"
           "%s",
           strlen(mess), mess);
  send(client_fd, response, strlen(response), 0);
}