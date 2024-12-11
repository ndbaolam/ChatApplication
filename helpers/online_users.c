#include <pthread.h>

#define MAX_USERS 100
typedef struct
{
    int user_id;
    int client_fd;
} OnlineUser;

OnlineUser online_users[MAX_USERS];
pthread_mutex_t online_users_mutex = PTHREAD_MUTEX_INITIALIZER;

int get_client_fd_by_user_id(int user_id)
{
    pthread_mutex_lock(&online_users_mutex);
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (online_users[i].user_id == user_id)
        {
            pthread_mutex_unlock(&online_users_mutex);
            return online_users[i].client_fd;
        }
    }
    pthread_mutex_unlock(&online_users_mutex);
    return -1; // Not found
}

void add_online_user(int user_id, int client_fd)
{
    pthread_mutex_lock(&online_users_mutex);
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (online_users[i].user_id == 0)
        { // Empty slot
            online_users[i].user_id = user_id;
            online_users[i].client_fd = client_fd;
            break;
        }
    }
    pthread_mutex_unlock(&online_users_mutex);
}

void remove_online_user(int client_fd)
{
    pthread_mutex_lock(&online_users_mutex);
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (online_users[i].client_fd == client_fd)
        {
            online_users[i].user_id = 0;
            online_users[i].client_fd = 0;
            break;
        }
    }
    pthread_mutex_unlock(&online_users_mutex);
}

/**
 * Retrieves the user ID for a given username from the PostgreSQL database.
 *
 * @param psql      Pointer to the PostgreSQL connection object.
 * @param username  The username to look up.
 * @return          The user ID as an integer, or -1 if not found or an error occurs.
 */
int GetUserIDByUsername(PGconn *psql, const char *username)
{
    if (!psql || !username)
    {
        fprintf(stderr, "Invalid parameters to GetUserIDByUsername\n");
        return -1;
    }

    char query[256];
    snprintf(query, sizeof(query), "SELECT user_id FROM users WHERE username = '%s';", username);

    PGresult *res = PQexec(psql, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Database query failed: %s\n", PQerrorMessage(psql));
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) == 0)
    {
        fprintf(stderr, "No user found for username: %s\n", username);
        PQclear(res);
        return -1;
    }

    int user_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    return user_id;
}
