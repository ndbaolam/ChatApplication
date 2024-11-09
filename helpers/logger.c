void WriteLog(const char *mess) {
    FILE *log = fopen("log.txt", "a");
    if (!log) {
        perror("Log file is missing...");
        return; 
    }

    time_t now = time(NULL) + 7 * 3600;
    struct tm *tm_info = gmtime(&now);    
    char date_str[30];

    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT+07", tm_info);

    fprintf(log, "[%s] %s\n", date_str, mess);

    fclose(log);
}
