#ifndef HELPERS_H
#define HELPERS_H

#include <time.h>
#include <libpq-fe.h>
#include "./simple_response.c"
#include "./datasource.c"
#include "./get_cookie.c"
#include "./logger.c"

PGconn *psql;

void Append(char **, const char *);
void ServeStaticFile(const char *, const int );
void Serve404(const int);
void RedirectResponse(const char *, const char *, const int);
char *GetCookieFromRequest(const char *, const char *);
void WriteLog(const char *);
char *LoadContentFile(const char *);

char *ExecDBQueryJSONFormat(PGconn *, const char *);
int ExecDBCommand(PGconn *, const char *);

#endif
