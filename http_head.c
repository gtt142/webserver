#include "config.h"
#include <string.h>
#include "time.h"
#include <stdio.h>

void create_response (char *status,
                       char *status_msg, char* response) {
    sprintf(response, "HTTP/1.1 %s %s\r\n", status, status_msg);
    sprintf(response + strlen(response), "Server: HLserver\r\n");
    sprintf(response + strlen(response), "Connection: close\r\n");
    char buf[TIME_BUFSIZE];
    time_t now = time(NULL);
    struct tm tm = *gmtime(&now);
    strftime(buf, sizeof(buf), "%a, %d, %b, %Y %H:%M:%S %Z", &tm);
    sprintf(response + strlen(response), "Date: %s\r\n", buf);
}