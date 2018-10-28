#pragma once

#define ERROR_BAD_CONFIG -401
#define ERROR_SPRINTF -402
#define PATH "/etc/httpd.conf"

#define MAX_PATH_LENGHT 256

struct configf {
    unsigned port;
    unsigned cpu;
    unsigned thread;
    char path[MAX_PATH_LENGHT];
};

int parse_config(struct configf* configf);
