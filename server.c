#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct Request {
    char method[16];
    char uri[256];
    char version[16];
    char mimeType[32];
    char status[32];
};

int main(int argc, char const *argv[])
{
    int sock, listener;
    struct sockaddr_in addr;

    listener = socket(AF_INET, SOCK_STREAM, 0);

    if (setsockopt(listener,SOL_SOCKET,SO_REUSEADDR, &addr, sizeof(addr)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    if (listener < 0) {
        perror("socket");
        exit (1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listener, (struct sockaddr *) &addr, sizeof(addr)) < 0) 
    {
        perror("bind");
        exit(2);
    }

    listen(listener, 1);

    while(1) {
        FILE *fs;
        char buf[1024];
        struct Request request;

        sock = accept(listener, NULL, NULL);
        if (sock < 0) {
            perror("accept");
            exit(3);
        }

        recv(sock, buf, sizeof(buf), 0);
        // printf("\n====\n%s\n======\n", buf);

        sscanf(buf, "%s %s %s\n", request.method, request.uri, request.version);

        if(strlen(request.uri) < 2) {
            strcpy(request.uri, "/index.html");
        }

        if(strstr(request.uri, ".html")) {
            strcpy(request.mimeType, "text/html");
        } else
        if(strstr(request.uri, ".css")) {
            strcpy(request.mimeType, "text/css");
        } else
        if(strstr(request.uri, ".js")) {
            strcpy(request.mimeType, "application/javascript");
        } else
        if(strstr(request.uri, ".jpg") || strstr(request.uri, ".jpeg")) {
            strcpy(request.mimeType, "image/jpeg");
        } else
        if(strstr(request.uri, ".png")) {
            strcpy(request.mimeType, "image/png");
        } else
        if(strstr(request.uri, ".gif")) {
            strcpy(request.mimeType, "image/gif");
        } else
        if(strstr(request.uri, ".swf")) {
            strcpy(request.mimeType, "application/x-shockwave-flash");
        }

        printf("\n%s\n%s\n%s\n", request.method, request.uri, request.version);

        if (!strstr(request.uri, "..")) {
            fs = fopen(request.uri+1, "r");
            if (fs == NULL) {
                strcpy(buf, "HTTP/1.1 404 NOT FOUND\n\n\0");
                strcpy(buf + strlen(buf), "<h1 align='center'>404 NOT FOUND<h1>\0");
                send(sock, buf, strlen(buf), 0);
            } else {
                strcpy(buf, "HTTP/1.1 200 OK\nContent-Type: ");
                strcpy(buf+strlen(buf), request.mimeType);
                strcpy(buf+strlen(buf), "\n\n");
                send(sock, buf, strlen(buf), 0);

                int len;
                while (len = fread(buf, sizeof(char), 1024, fs)) {
                    send(sock, buf, len, 0);
                }
                fclose(fs);
            }
        } else {
            strcpy(buf, "HTTP/1.1 403 FORBIDDEN\n\n\0");
            strcpy(buf + strlen(buf), "<h1 align='center'>403 FORBIDDEN<h1>\0");
            send(sock, buf, strlen(buf), 0);            
        }
        close(sock);
    }
    
    return 0;
}
