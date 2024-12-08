#ifndef HELPERS_H
#define HELPERS_H

#include <time.h>
#include <libpq-fe.h>
#include "./simple_response.c"
#include "./datasource.c"
#include "./get_cookie.c"
#include "./logger.c"
#include "./online_users.c"

PGconn *psql;

void Append(char **, const char *);
void ServeStaticFile(const char *, const int );
void Serve404(const int);
void RedirectResponse(const char *, const char *, const int);
char *GetCookieFromRequest(const char *, const char *);

char *UpdatedGetCookie(const char *);

void WriteLog(const char *);
char *LoadContentFile(const char *);

char *ExecDBQueryJSONFormat(PGconn *, const char *);
int ExecDBCommand(PGconn *, const char *);

#define MAX_USERS 100

extern OnlineUser online_users[MAX_USERS];
extern pthread_mutex_t online_users_mutex;

int get_client_fd_by_user_id(int);
void add_online_user(int, int);
void remove_online_user(int);

#endif
