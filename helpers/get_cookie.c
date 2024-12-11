char *GetCookieFromRequest(const char *request, const char *cookie_name)
{
  char *cookie_header = strstr(request, "Cookie: ");
  if (!cookie_header)
  {
    return NULL;
  }

  cookie_header += 8;

  char *value = NULL;

  char *start = strstr(cookie_header, cookie_name);
  if (start)
  {
    start += strlen(cookie_name) + 1;

    char *end = strchr(start, ';');
    if (!end)
    {
      end = start + strlen(start);
    }

    size_t length = end - start;
    value = (char *)malloc(length + 1);
    if (value)
    {
      strncpy(value, start, length);
      value[length] = '\0';
    }
  }

  return value;
}

char *UpdatedGetCookie(const char *request)
{
  char *dup_req = strdup(request);
  char *cookie = strstr(request, "token=");
  if (!cookie)
  {
    return NULL;
  }

  char *safe_cookie = cookie + 6;
  safe_cookie[strcspn(safe_cookie, "\r\n")] = '\0';
  return safe_cookie;
}