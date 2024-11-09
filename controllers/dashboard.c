//GET /
void handleDashBoard(const char *request, const int clien_fd) {
  char *cookie = GetCookieFromRequest(request, "token");

  if(!cookie) {
    ServeStaticFile("ui/index.html", clien_fd);
    return;
  }

  RedirectRespose("user-info", cookie, clien_fd);
}