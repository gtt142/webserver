#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <event2/buffer.h>

#include "responce_parse.h"

void get_uri_without_parameters(char* src, char* dest, int max) {
    char* ptr = strchr(src, '?');
    if (ptr) {
        max = ptr - src;
    }
    strncpy(dest, src, max);
    dest[max] = '\0';
}

void decode_uri(char* src, char* dest, int max) {
    char *p = src;
    char code[3] = {0};
    while (*p != '\0' && --max) {
        if (*p == '%') {
            memcpy(code, ++p, 2);
            *dest++ = (char) strtoul(code, NULL, 16);
            p += 2;
        } else {
            *dest++ = *p++;
        }
    }
    *dest = '\0';
}

int http_parse(http_t *http_request, struct evbuffer *client_buffer, char *root_dir) {

    struct evbuffer *event_buffer = client_buffer;
    struct stat sbuf;     /* file status */
    size_t line_size = LINE_BUFSIZE;

    printf("HTTP PARSE START\n");

    /* get the HTTP request line */
    if(strcpy(http_request->buf, evbuffer_readln(event_buffer, &line_size, EVBUFFER_EOL_CRLF)) == NULL) {
        return PARSE_ERROR;
    }

    if ((sscanf(http_request->buf, "%s %s %s\n", http_request->method, http_request->uri, http_request->version)) < 3) {
        return PARSE_ERROR;
    }

    printf("GETTING HTTP REQUEST\n");


    /* server only supports the GET and HEAD methods */
    if (strcmp(http_request->method, "GET") != 0 && strcmp(http_request->method, "HEAD") != 0) {
      return NOT_ALLOWED_HTTP_METHOD;
    }

    printf("ALLOWED METHODS\n");

    /* parse the uri */
    if (!strstr(http_request->uri, "cgi-bin")) {
        strcpy(http_request->cgiargs, "");
        strcpy(http_request->filename, root_dir);
        if(strstr(http_request->uri, "%")) {
            printf("uri decoding\n");
            decode_uri(http_request->uri, http_request->uri, strlen(http_request->uri));
            printf("URI = %s\n", http_request->uri);
        }
        if(strstr(http_request->uri, "?")) {
            printf("uri need to be reed out of parametres\n");
            char temp[strlen(http_request->uri)];
            get_uri_without_parameters(http_request->uri, temp, strlen(http_request->uri));
            strcpy(http_request->uri, temp);
            printf("URI = %s\n", http_request->uri);

        }

        strcat(http_request->filename, http_request->uri);
        if (http_request->uri[strlen(http_request->uri)-1] == '/') {
            strcat(http_request->filename, "index.html");
            if (stat(http_request->filename, &sbuf) < 0) {
                return INDEX_FILE_NOT_EXIST;
            }
        }
    }

    printf("%s\n", http_request->filename);

    if ((strstr(http_request->filename, "/..")) != NULL) {
        return ESCAPING_ROOT;
    }

    /* make sure the file exists */
    if (stat(http_request->filename, &sbuf) < 0) {
        return FILE_NOT_EXIST;
    }

    /* serve static content */
    if (strstr(http_request->filename, ".html"))
        strcpy(http_request->filetype, "text/html");
    else if (strstr(http_request->filename, ".css"))
        strcpy(http_request->filetype, "text/css");
    else if (strstr(http_request->filename, ".js"))
        strcpy(http_request->filetype, "application/javascript");
    else if (strstr(http_request->filename, ".gif"))
        strcpy(http_request->filetype, "image/gif");
    else if (strstr(http_request->filename, ".swf"))
        strcpy(http_request->filetype, "application/x-shockwave-flash");
    else if (strstr(http_request->filename, ".jpeg"))
        strcpy(http_request->filetype, "image/jpeg");
    else if (strstr(http_request->filename, ".jpg"))
        strcpy(http_request->filetype, "image/jpeg");
    else if (strstr(http_request->filename, ".png"))
        strcpy(http_request->filetype, "image/png");

    http_request->filesize = (size_t) sbuf.st_size;
    if (!(S_ISREG(sbuf.st_mode))) {
        return FILE_IS_EXECUTABLE;
    }

    return ALL_OK;
  }
