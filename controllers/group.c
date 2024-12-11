void createGroupChat(const char *request, const int client_fd)
{
  char *dup_req = strdup(request);
  char *cookie = UpdatedGetCookie(request);

  if (!cookie)
  {
    RedirectResponse("/", NULL, client_fd);
    free(dup_req);
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
    sendErrorResponse(client_fd, "{\"error\": \"Invalid user session.\"}");
    free(dup_req);
    return;
  }

  int sender_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Extract group name from request
  char *groupname_start = strstr(dup_req, "groupname=");
  if (!groupname_start)
  {
    sendErrorResponse(client_fd, "{\"error\": \"Group name is missing.\"}");
    free(dup_req);
    return;
  }
  groupname_start += 10; // Move past "groupname="

  char *groupname_end = strchr(groupname_start, '&');
  if (!groupname_end)
  {
    groupname_end = groupname_start + strlen(groupname_start);
  }

  size_t groupname_len = groupname_end - groupname_start;
  if (groupname_len == 0 || groupname_len > 100)
  {
    sendErrorResponse(client_fd, "{\"error\": \"Invalid group name length.\"}");
    free(dup_req);
    return;
  }

  char groupname[101] = {0};
  strncpy(groupname, groupname_start, groupname_len);

  // Insert the new group into the database
  snprintf(query, sizeof(query),
           "INSERT INTO chat_groups (group_name, created_by) VALUES ($1, $2) RETURNING group_id;");

  const char *paramValues[2] = {groupname, (char[]){sender_id + '0', '\0'}};
  int paramLengths[2] = {(int)strlen(groupname), (int)strlen(paramValues[1])};
  int paramFormats[2] = {0, 0};

  res = PQexecParams(psql, query, 2, NULL, paramValues, paramLengths, paramFormats, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "INSERT failed: %s", PQerrorMessage(psql));
    sendErrorResponse(client_fd, "{\"error\": \"Failed to create group.\"}");
    PQclear(res);
    free(dup_req);
    return;
  }

  int group_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Add the creator to the group_members table as an admin
  snprintf(query, sizeof(query),
           "INSERT INTO group_members (group_id, user_id, role) VALUES ($1, $2, 'admin');");

  const char *memberParams[2] = {NULL, NULL};
  char group_id_str[12] = {0};
  char sender_id_str[12] = {0};
  snprintf(group_id_str, sizeof(group_id_str), "%d", group_id);
  snprintf(sender_id_str, sizeof(sender_id_str), "%d", sender_id);
  memberParams[0] = group_id_str;
  memberParams[1] = sender_id_str;

  int memberLengths[2] = {(int)strlen(memberParams[0]), (int)strlen(memberParams[1])};
  int memberFormats[2] = {0, 0};

  res = PQexecParams(psql, query, 2, NULL, memberParams, memberLengths, memberFormats, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Failed to add creator to group_members: %s", PQerrorMessage(psql));
    sendErrorResponse(client_fd, "{\"error\": \"Failed to add creator to group.\"}");
    PQclear(res);
    free(dup_req);
    return;
  }
  PQclear(res);

  // Prepare success response
  char response[512];
  snprintf(response, sizeof(response), "{\"success\": true, \"group_id\": %d}", group_id);
  sendSuccessResponse(client_fd, response);

  free(dup_req);
}

void addUserToGroup(const char *request, const int client_fd)
{
  char *dup_req = strdup(request);
  char *cookie = UpdatedGetCookie(request);

  if (!cookie)
  {
    RedirectResponse("/", NULL, client_fd);
    free(dup_req);
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
    sendErrorResponse(client_fd, "{\"error\": \"Invalid user session.\"}");
    free(dup_req);
    return;
  }

  int sender_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Extract username and group name from request
  char *username_start = strstr(dup_req, "username=");
  if (!username_start)
  {
    sendErrorResponse(client_fd, "{\"error\": \"Username is missing.\"}");
    free(dup_req);
    return;
  }
  username_start += 9;

  char *username_end = strchr(username_start, '&');
  if (!username_end)
  {
    username_end = username_start + strlen(username_start);
  }

  size_t username_len = username_end - username_start;
  if (username_len == 0 || username_len >= 32)
  {
    sendErrorResponse(client_fd, "{\"error\": \"Invalid username length.\"}");
    free(dup_req);
    return;
  }

  char username[32] = {0};
  strncpy(username, username_start, username_len);

  char *groupname_start = strstr(dup_req, "groupname=");
  if (!groupname_start)
  {
    sendErrorResponse(client_fd, "{\"error\": \"Group name is missing.\"}");
    free(dup_req);
    return;
  }
  groupname_start += 10;

  char *groupname_end = strchr(groupname_start, '&');
  if (!groupname_end)
  {
    groupname_end = groupname_start + strlen(groupname_start);
  }

  size_t groupname_len = groupname_end - groupname_start;
  if (groupname_len == 0 || groupname_len > 100)
  {
    sendErrorResponse(client_fd, "{\"error\": \"Invalid group name length.\"}");
    free(dup_req);
    return;
  }

  char groupname[101] = {0};
  strncpy(groupname, groupname_start, groupname_len);

  // Get group_id from group name
  snprintf(query, sizeof(query), "SELECT group_id FROM chat_groups WHERE group_name='%s';", groupname);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT failed: %s %s", groupname, PQerrorMessage(psql));
    PQclear(res);
    sendErrorResponse(client_fd, "{\"error\": \"Group not found.\"}");
    free(dup_req);
    return;
  }

  int group_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Get user_id of the user to add
  snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", username);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT failed: %s %s", username, PQerrorMessage(psql));
    PQclear(res);
    sendErrorResponse(client_fd, "{\"error\": \"User not found.\"}");
    free(dup_req);
    return;
  }

  int user_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Check if the user is already a member of the group
  snprintf(query, sizeof(query),
           "SELECT COUNT(*) FROM group_members WHERE group_id = %d AND user_id = %d;",
           group_id, user_id);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT COUNT failed: %s", PQerrorMessage(psql));
    PQclear(res);
    sendErrorResponse(client_fd, "{\"error\": \"Database error.\"}");
    free(dup_req);
    return;
  }

  int count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  if (count > 0)
  {
    sendErrorResponse(client_fd, "{\"error\": \"User is already a member of the group.\"}");
    free(dup_req);
    return;
  }

  // Add user to the group
  snprintf(query, sizeof(query),
           "INSERT INTO group_members (group_id, user_id, role) VALUES ($1, $2, 'member');");

  const char *paramValues[2] = {(char[]){group_id + '0', '\0'}, (char[]){user_id + '0', '\0'}};
  int paramLengths[2] = {(int)strlen(paramValues[0]), (int)strlen(paramValues[1])};
  int paramFormats[2] = {0, 0};

  res = PQexecParams(psql, query, 2, NULL, paramValues, paramLengths, paramFormats, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "INSERT failed: %s", PQerrorMessage(psql));
    sendErrorResponse(client_fd, "{\"error\": \"Failed to add user to the group.\"}");
    PQclear(res);
    free(dup_req);
    return;
  }

  PQclear(res);

  // Prepare success response
  sendSuccessResponse(client_fd, "{\"success\": true}");

  free(dup_req);
}

void removeUserFromGroup(const char *request, const int client_fd)
{
  char *dup_req = strdup(request);
  char *cookie = UpdatedGetCookie(request);

  if (!cookie)
  {
    RedirectResponse("/", NULL, client_fd);
    free(dup_req);
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
    sendErrorResponse(client_fd, "{\"error\": \"Invalid user session.\"}");
    free(dup_req);
    return;
  }

  int sender_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Extract username and group name from request
  char *username_start = strstr(dup_req, "username=");
  if (!username_start)
  {
    sendErrorResponse(client_fd, "{\"error\": \"Username is missing.\"}");
    free(dup_req);
    return;
  }
  username_start += 9;

  char *username_end = strchr(username_start, '&');
  if (!username_end)
  {
    username_end = username_start + strlen(username_start);
  }

  size_t username_len = username_end - username_start;
  if (username_len == 0 || username_len >= 32)
  {
    sendErrorResponse(client_fd, "{\"error\": \"Invalid username length.\"}");
    free(dup_req);
    return;
  }

  char username[32] = {0};
  strncpy(username, username_start, username_len);

  char *groupname_start = strstr(dup_req, "groupname=");
  if (!groupname_start)
  {
    sendErrorResponse(client_fd, "{\"error\": \"Group name is missing.\"}");
    free(dup_req);
    return;
  }
  groupname_start += 10;

  char *groupname_end = strchr(groupname_start, '&');
  if (!groupname_end)
  {
    groupname_end = groupname_start + strlen(groupname_start);
  }

  size_t groupname_len = groupname_end - groupname_start;
  if (groupname_len == 0 || groupname_len > 100)
  {
    sendErrorResponse(client_fd, "{\"error\": \"Invalid group name length.\"}");
    free(dup_req);
    return;
  }

  char groupname[101] = {0};
  strncpy(groupname, groupname_start, groupname_len);

  // Get group_id from group name
  snprintf(query, sizeof(query), "SELECT group_id FROM chat_groups WHERE group_name='%s';", groupname);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT failed: %s %s", groupname, PQerrorMessage(psql));
    PQclear(res);
    sendErrorResponse(client_fd, "{\"error\": \"Group not found.\"}");
    free(dup_req);
    return;
  }

  int group_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Get user_id of the user to remove
  snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username='%s';", username);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT failed: %s %s", username, PQerrorMessage(psql));
    PQclear(res);
    sendErrorResponse(client_fd, "{\"error\": \"User not found.\"}");
    free(dup_req);
    return;
  }

  int user_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Verify sender's permission to remove users
  snprintf(query, sizeof(query),
           "SELECT role FROM group_members WHERE group_id = %d AND user_id = %d;",
           group_id, sender_id);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "SELECT failed: %s", PQerrorMessage(psql));
    PQclear(res);
    sendErrorResponse(client_fd, "{\"error\": \"You are not authorized to remove users.\"}");
    free(dup_req);
    return;
  }

  char *role = PQgetvalue(res, 0, 0);
  if (strcmp(role, "admin") != 0)
  {
    sendErrorResponse(client_fd, "{\"error\": \"Only admins can remove users from the group.\"}");
    PQclear(res);
    free(dup_req);
    return;
  }
  PQclear(res);

  // Remove user from the group
  snprintf(query, sizeof(query),
           "DELETE FROM group_members WHERE group_id = %d AND user_id = %d;",
           group_id, user_id);
  res = PQexec(psql, query);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "DELETE failed: %s", PQerrorMessage(psql));
    sendErrorResponse(client_fd, "{\"error\": \"Failed to remove user from group.\"}");
    PQclear(res);
    free(dup_req);
    return;
  }

  PQclear(res);

  // Send success response
  sendSuccessResponse(client_fd, "{\"success\": true, \"message\": \"User removed from group.\"}");

  free(dup_req);
}
