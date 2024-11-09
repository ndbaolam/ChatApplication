void handleUser(const char *request, const int client_fd) {
  char *cookie = GetCookieFromRequest(request, "token");

  if(cookie)
    ServeStaticFile("ui/user.html", client_fd);
  else
    RedirectRespose("/", NULL, client_fd);
}