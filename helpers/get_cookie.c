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
