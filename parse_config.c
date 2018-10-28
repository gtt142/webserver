#include "parse_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FORMAT_LENGTH 32
#define MAX_OPTION_LENGTH 14

#define OPTION_PORT "listen"
#define OPTION_CPU "cpu_limit"
#define OPTION_THREAD_LIMIT "thread_limit"
#define OPTION_DOCUMENT_PATH "document_root"

int parse_config(struct configf* configf) {
    FILE* config = fopen(PATH, "r");
    if (config == NULL) {
        return ERROR_BAD_CONFIG;
    }

    char option[MAX_OPTION_LENGTH];
    char format[MAX_FORMAT_LENGTH];

    if (sprintf(format, "%%%ds", MAX_FORMAT_LENGTH) < 0) {
        return ERROR_SPRINTF;
    }

    printf("read config\n");

    while (fscanf(config, format, option) != EOF) {
        if (strcmp(option, OPTION_PORT) == 0) {
            if (fscanf(config, "%u", &(configf->port)) != 1) {
                return ERROR_BAD_CONFIG;
            }
        } else if (strcmp(option, OPTION_CPU) == 0) {
            if (fscanf(config, "%u", &(configf->cpu)) != 1) {
                return ERROR_BAD_CONFIG;
            }
        } else if (strcmp(option, OPTION_THREAD_LIMIT) == 0) {
            if (fscanf(config, "%u", &(configf->thread)) != 1) {
                return ERROR_BAD_CONFIG;
            }
        } else if (strcmp(option, OPTION_DOCUMENT_PATH) == 0) {
            char path_format[MAX_FORMAT_LENGTH];
            if (sprintf(path_format, "%%%ds", MAX_PATH_LENGHT) < 0) {
                return ERROR_SPRINTF;
            }
            if (fscanf(config, path_format, configf->path) != 1) {
                return ERROR_BAD_CONFIG;
            }
        }
    }
    printf("end read config\n");

    return 0;
}