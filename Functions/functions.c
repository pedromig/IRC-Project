#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "functions.h"

int isnumber(char *string) {
    int status = 1;
    int i, len = (int) strlen(string);
    for (i = 0; i < len && status != 0; i++)
        if (!isdigit(string[i]))
            status = 0;
    return status;
}

void free_double_ptr(char **d_ptr, int size) {
    for (int i = 0; i < size; i++) {
        if (d_ptr[i] != NULL) {
            free(d_ptr[i]);
        }
    }
    free(d_ptr);
}

char **parseCommand(char *string) {
    int i = 0, error = 0;
    char **params = (char **) malloc(sizeof(char *) * 3);
    char *token, *save_ptr = string;
    char delimiter[1] = " ";

    while ((token = strtok_r(save_ptr, delimiter, &save_ptr)) && i != 4 && !error) {
        switch (i) {
            case 1:
                if (!strcmp(token, "TCP")) {
                    params[0] = strdup("TCP");
                } else if (!strcmp(token, "UDP")) {
                    params[0] = strdup("UDP");
                } else {
                    error = 1;
                }

                break;
            case 2:

                if (!strcmp(token, "ENC")) {
                    params[1] = strdup("ENC");
                } else if ((!strcmp(token, "NOR"))) {
                    params[1] = strdup("NOR");
                } else {
                    error = 1;
                }
                break;
            case 3:
                params[2] = strdup(token);

        }
        i++;
    }

    if (error || i <= 1) {
        free(params);
        error = 1;
    }
    return error ? 0 : params;
}
