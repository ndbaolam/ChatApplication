#ifndef CONTROLLERS_H
#define CONTROLLERS_H

#include "../helpers/main.h"
#include "./authenticate.c"
#include "./dashboard.c"
#include "./user.c"
// Add more as needed

void handleSignIn(const char *, const int);
void handleSignUp(const char *, const int);
void handleDashBoard(const char *, const int);
void handleUser(const char *, const int);
void handleSignOut(const char *, const int);

#endif
