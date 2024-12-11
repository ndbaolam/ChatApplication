void Append(char **output, const char *str)
{
    char *tmp = *output;
    int oldlen = tmp ? strlen(tmp) : 0;
    int newlen = oldlen + strlen(str) + 1;

    tmp = realloc(tmp, newlen);
    if (tmp == NULL)
    {
        fprintf(stderr, "Memory allocation failed.\n");
        return;
    }

    strcpy(tmp + oldlen, str);

    *output = tmp;
}

void ServeStaticFile(const char *filename, const int client_fd)
{
    FILE *html = fopen(filename, "r");
    if (!html)
    {
        fprintf(stderr, "%s: Missing file!?...", filename);
        close(client_fd);
        pthread_exit(NULL);
    }

    char *response = NULL;
    Append(&response, "HTTP/1.1 200 OK\r\n");
    Append(&response, "Content-Type: text/html\r\n");

    fseek(html, 0, SEEK_END);
    long content_length = ftell(html);
    fseek(html, 0, SEEK_SET);

    char content_length_header[50];
    snprintf(content_length_header, sizeof(content_length_header), "Content-Length: %ld\r\n", content_length);
    Append(&response, content_length_header);
    Append(&response, "\r\n");

    char *html_content = malloc(content_length + 1);
    if (html_content)
    {
        fread(html_content, 1, content_length, html);
        html_content[content_length] = '\0';

        Append(&response, html_content);
        free(html_content);
    }
    else
    {
        perror("Memory allocation failed for HTML content");
    }

    send(client_fd, response, strlen(response), 0);

    fclose(html);
    free(response);
}

char *LoadContentFile(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(file_size + 1);
    if (!buffer)
    {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';

    fclose(file);
    return buffer;
}

void RedirectResponse(const char *redirect_url, const char *cookie, const int client_socket)
{
    char response[1024] = {0};
    if (!cookie)
    {
        snprintf(response, sizeof(response) - 1,
                 "HTTP/1.1 302 Found\r\n"
                 "Location: %s\r\n"
                 "Content-Length: 0\r\n"
                 "Connection: close\r\n\r\n",
                 redirect_url);
        send(client_socket, response, strlen(response), 0);
    }
    else
    {
        time_t now = time(NULL);
        time_t expire = now + 10 * 60;
        struct tm *tm_info = gmtime(&expire);

        char date_str[30] = {0};
        strftime(date_str, sizeof(date_str) - 1, "%a, %d %b %Y %H:%M:%S GMT", tm_info);

        snprintf(response, sizeof(response) - 1,
                 "HTTP/1.1 302 Found\r\n"
                 "Location: %s\r\n"
                 "Set-Cookie: token=%s; expires=%s; Path=/; HttpOnly\r\n"
                 "Content-Length: 0\r\n"
                 "Connection: close\r\n\r\n",
                 redirect_url, cookie, date_str);
        send(client_socket, response, strlen(response), 0);
    }
}

void Serve404(const int client_fd)
{
    FILE *html = fopen("ui/404.html", "r");
    if (!html)
    {
        perror("404.html: Missing file!?...");
        close(client_fd);
        pthread_exit(NULL);
    }

    char *response = NULL;
    Append(&response, "HTTP/1.1 404 Not Found\r\n");
    Append(&response, "Content-Type: text/html\r\n");

    fseek(html, 0, SEEK_END);
    long content_length = ftell(html);
    fseek(html, 0, SEEK_SET);

    char content_length_header[50];
    snprintf(content_length_header, sizeof(content_length_header), "Content-Length: %ld\r\n", content_length);
    Append(&response, content_length_header);
    Append(&response, "\r\n");

    char *html_content = malloc(content_length + 1);
    if (html_content)
    {
        fread(html_content, 1, content_length, html);
        html_content[content_length] = '\0';

        Append(&response, html_content);
        free(html_content);
    }
    else
    {
        perror("Memory allocation failed for HTML content");
    }

    send(client_fd, response, strlen(response), 0);

    fclose(html);
    free(response);
}

void sendErrorResponse(const int client_fd, const char *message)
{
    char response[4096] = {0};
    snprintf(response, sizeof(response),
             "HTTP/1.1 400 Bad Request\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(message), message);
    send(client_fd, response, strlen(response), 0);
}

void sendSuccessResponse(const int client_fd, const char *message)
{
    char response[4096] = {0};
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             strlen(message), message);
    send(client_fd, response, strlen(response), 0);
}