#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>

PGconn *ConnectToDB(const char *dbname, const char *user, const char *password, const char *host, const int port) {
  PGconn *conn = NULL;
  char conninfo[1024] = {0};

  snprintf(conninfo, sizeof(conninfo), "dbname=%s user=%s password=%s host=%s port=%d", dbname, user, password, host, port);

  conn = PQconnectdb(conninfo);

  if (PQstatus(conn) != CONNECTION_OK) {
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
int ExecDBCommand(PGconn *conn, const char *cmd) {
  PGresult *res = PQexec(conn, cmd);
  int result = 0;

  if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "Error executing command: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return -1;
  }

  if (PQresultStatus(res) == PGRES_TUPLES_OK) {
    // SELECT command: Retrieve and print results
    int rows = PQntuples(res);
    int cols = PQnfields(res);
    result = rows;

    printf("Retrieved %d rows and %d columns:\n", rows, cols);

    // Print column names
    // for (int j = 0; j < cols; j++) {
    //   printf("%s\t", PQfname(res, j));
    // }
    // printf("\n");

    // // Print each row
    // for (int i = 0; i < rows; i++) {
    //   for (int j = 0; j < cols; j++) {
    //     printf("%s\t", PQgetvalue(res, i, j));
    //   }
    //   printf("\n");
    // }
  } else {
    // INSERT, UPDATE, DELETE, etc.: Get affected rows
    int affected_rows = atoi(PQcmdTuples(res));
    result = affected_rows;
    printf("Command executed successfully, %d row(s) affected.\n", affected_rows);
  }

  PQclear(res);
  return result;
}
