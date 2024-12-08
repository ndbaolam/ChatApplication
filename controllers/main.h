#ifndef CONTROLLERS_H
#define CONTROLLERS_H

#include "../helpers/main.h"
#include "./authenticate.c"
#include "./dashboard.c"
#include "./chatuser.c"
#include "./user.c"
// Add more as needed

void handleSignIn(const char *, const int);
void handleSignUp(const char *, const int);
void handleDashBoard(const char *, const int);
void handleUser(const char *, const int);
void handleGetUserInfo(const char *, const int);
void handleSignOut(const char *, const int);
void handleSendAddFriendRequest(const char *, const int);
void handleAcceptAddFriendRequest(const char *, const int);
void handleDeclineAddFriendRequest(const char *, const int);
void handleRemoveFriend(const char *, const int);
void handleChatUser(const char *, const int);
void sendMessageToUser(const char *, const int);
void sendMessageToUser(const char *, const int);
void getAllMessages(const char *request, const int, const char *);

#endif
