#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>

PGconn *ConnectToDB(const char *dbname, const char *user, const char *password, const char *host, const int port)
{
  PGconn *conn = NULL;
  char conninfo[1024] = {0};

  snprintf(conninfo, sizeof(conninfo), "dbname=%s user=%s password=%s host=%s port=%d", dbname, user, password, host, port);

  conn = PQconnectdb(conninfo);

  if (PQstatus(conn) != CONNECTION_OK)
  {
    fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
    PQfinish(conn);
    exit(1);
  }

  printf("Connected to the database successfully.\n");
  return conn;
}

/**
 * @details: Executes a SQL command and returns the number of affected rows for
 * INSERT/UPDATE/DELETE, or the number of retrieved rows for SELECT.
 * Returns -1 if an error occurs.
 */
int ExecDBCommand(PGconn *conn, const char *cmd)
{
  PGresult *res = PQexec(conn, cmd);
  int result = 0;

  if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Error executing command: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return -1;
  }

  if (PQresultStatus(res) == PGRES_TUPLES_OK)
  {
    // SELECT command: Retrieve and print results
    int rows = PQntuples(res);
    int cols = PQnfields(res);
    result = rows;

    printf("Retrieved %d rows and %d columns:\n", rows, cols);
  }
  else
  {
    // INSERT, UPDATE, DELETE, etc.: Get affected rows
    int affected_rows = atoi(PQcmdTuples(res));
    result = affected_rows;
    printf("Command executed successfully, %d row(s) affected.\n", affected_rows);
  }

  PQclear(res);
  return result;
}

/**
 * @details: Query database and then return json format if success
 * {
  "column_1": ["value1", "value2", ...],
  "column_2": ["value1", "value2", ...],
  ...
  }
 * return NULL otherwise
 */
char *ExecDBQueryJSONFormat(PGconn *conn, const char *cmd)
{
  PGresult *res = PQexec(conn, cmd);
  char *args = NULL;

  if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Error executing command: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return NULL;
  }

  if (PQresultStatus(res) == PGRES_TUPLES_OK)
  {
    // SELECT command: Retrieve and print results
    int rows = PQntuples(res);
    int cols = PQnfields(res);

    if (rows == 0)
    {
      return NULL;
    }

    printf("Retrieved %d rows and %d columns:\n", rows, cols);

    Append(&args, "{");

    for (int j = 0; j < cols; j++)
    {
      char col[256] = {0};
      snprintf(col, sizeof(col) - 1, "\"%s\": [", PQfname(res, j));
      Append(&args, col);

      for (int i = 0; i < rows; i++)
      {
        char value[256] = {0};
        snprintf(value, sizeof(value) - 1, "\"%s\"", PQgetvalue(res, i, j));
        Append(&args, value);

        if (i < rows - 1)
        {
          Append(&args, ", ");
        }
      }

      Append(&args, "]");

      if (j < cols - 1)
      {
        Append(&args, ", ");
      }
    }

    Append(&args, "}");

    PQclear(res);
    return args;
  }
  else
  {
    fprintf(stderr, "Error executing command: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return NULL;
  }
}