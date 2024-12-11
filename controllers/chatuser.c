// GET /chat-user
void handleChatUser(const char *request, const int client_fd)
{
    char *cookie = GetCookieFromRequest(request, "token");

    if (!cookie)
    {
        ServeStaticFile("ui/index.html", client_fd);
        return;
    }

    ServeStaticFile("ui/chat-user.html", client_fd);
}

// POST /chat-user
void sendMessageToUser(const char *request, const int client_fd)
{
    char *dup_req = strdup(request);
    char *cookie = UpdatedGetCookie(request);

    if (!cookie)
    {
        RedirectResponse("/", NULL, client_fd);
        return;
    }

    char query[2048] = {0};
    snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", cookie);
    PGresult *res = PQexec(psql, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
    {
        fprintf(stderr, "SELECT failed: %s %s", cookie, PQerrorMessage(psql));
        PQclear(res);
        sendErrorResponse(client_fd, "{\"error\": \"Invalid message data.\"}");
        return;
    }

    int sender_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    // Get the recipient's username
    char *body = strstr(dup_req, "username=");
    if (!body)
    {
        sendErrorResponse(client_fd, "{\"error\": \"Invalid message data.\"}");
        return;
    }

    char user_received[32] = {0};
    if (strncmp(body, "username=", 9) == 0)
    {
        strncpy(user_received, body + 9, sizeof(user_received) - 1);
        user_received[strcspn(user_received, "&")] = '\0';
    }

    if (strlen(user_received) == 0)
    {
        sendErrorResponse(client_fd, "{\"error\": \"Invalid message data.\"}");
        return;
    }

    // Get message content
    body = strstr(dup_req, "content=");
    char content[1024] = {0};
    if (strncmp(body, "content=", 8) == 0)
    {
        strncpy(content, body + 8, sizeof(content) - 1);
        content[sizeof(content) - 1] = '\0';
    }

    if (strlen(content) == 0)
    {
        sendErrorResponse(client_fd, "{\"error\": \"Invalid message data.\"}");
        return;
    }

    // Get receiver's user_id
    snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", user_received);
    res = PQexec(psql, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
    {
        fprintf(stderr, "SELECT failed: %s %s", user_received, PQerrorMessage(psql));
        PQclear(res);
        sendErrorResponse(client_fd, "{\"error\": \"Invalid message data.\"}");
        return;
    }

    int receiver_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    // Insert message into the database
    snprintf(query, sizeof(query),
             "INSERT INTO messages(sender_id, receiver_id, content) "
             "VALUES (%d, %d, '%s');",
             sender_id, receiver_id, content);
    res = PQexec(psql, query);

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "INSERT failed: %s", PQerrorMessage(psql));
        PQclear(res);
        sendErrorResponse(client_fd, "{\"error\": \"Invalid message data.\"}");
        return;
    }

    PQclear(res);

    // Send success response to the sender
    sendSuccessResponse(client_fd, "{\"message\": \"Successfully sent.\"}");

    // Send message to the receiver
    int receiver_fd = get_client_fd_by_user_id(receiver_id);
    if (receiver_fd > 0)
    {
        char response[4096] = {0};
        char mess[2048] = {0};
        snprintf(mess, sizeof(mess) - 1, "{\"message\": \"%s.\"}", content);
        snprintf(response, sizeof(response) - 1,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %lu\r\n"
                 "Location: /chat-user\r\n" // Chua test
                 "Connection: close\r\n\r\n"
                 "%s",
                 strlen(mess), mess);
        send(receiver_fd, response, strlen(response), 0);
    }
}

// GET /message/:user
void getAllMessages(const char *request, const int client_fd, const char *username)
{
    char *dup_req = strdup(request);
    char *cookie = UpdatedGetCookie(request);

    if (!cookie)
    {
        RedirectResponse("/", NULL, client_fd);
        return;
    }

    // Get sender_id from the cookie (logged-in user)
    char query[2048] = {0};
    snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", cookie);
    PGresult *res = PQexec(psql, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
    {
        fprintf(stderr, "SELECT failed: %s %s", cookie, PQerrorMessage(psql));
        PQclear(res);
        sendErrorResponse(client_fd, "{\"error\": \"Invalid message data.\"}");
        return;
    }

    int sender_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    // Get receiver_id based on the provided username
    snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", username);
    res = PQexec(psql, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
    {
        fprintf(stderr, "SELECT failed: %s %s", username, PQerrorMessage(psql));
        PQclear(res);
        sendErrorResponse(client_fd, "{\"error\": \"Invalid message data.\"}");
        return;
    }

    int receiver_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    // Retrieve all messages between sender and receiver
    snprintf(query, sizeof(query),
             "SELECT u1.username AS sender, m.content "
             "FROM messages AS m "
             "JOIN users AS u1 ON m.sender_id = u1.user_id "
             "JOIN users AS u2 ON m.receiver_id = u2.user_id "
             "WHERE (m.receiver_id=%d AND m.sender_id=%d) OR (m.receiver_id=%d AND m.sender_id=%d) "
             "ORDER BY m.sent_at;",
             receiver_id, sender_id, sender_id, receiver_id);
    res = PQexec(psql, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
    {
        fprintf(stderr, "SELECT FAILED %s\n", query);
        PQclear(res);
        sendErrorResponse(client_fd, "{\"error\": \"No messages found.\"}");
        return;
    }

    // Build the response JSON with all the messages
    char mess[1024] = {0};
    char content[512] = {0};
    snprintf(mess, sizeof(mess), "{\"messages\": [");

    for (int i = 0; i < PQntuples(res); i++)
    {
        snprintf(content, sizeof(content), "{\"sender\": \"%s\", \"content\": \"%s\"},",
                 PQgetvalue(res, i, 0), PQgetvalue(res, i, 1));
        strncat(mess, content, sizeof(mess) - strlen(mess) - 1);
    }

    // Remove the trailing comma if messages exist
    if (PQntuples(res) > 0)
    {
        mess[strlen(mess) - 1] = '\0'; // Remove the last comma
    }

    strcat(mess, "]}");

    PQclear(res);
    sendSuccessResponse(client_fd, mess);
}