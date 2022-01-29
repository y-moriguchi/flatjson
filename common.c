/*
 * flat JSON
 *
 * Copyright (c) 2022 Yuichiro MORIGUCHI
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"

void *xalloc(int size) {
    void *result;

    if((result = malloc(size)) == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_ERROR);
    }
    return result;
}

char *get_delimiter_arg(int argc, char *argv[], char *arg_string, void (*usage)(), int *argindex) {
    int nowindex = *argindex;

    if(strcmp(argv[nowindex], arg_string) == 0) {
        if(nowindex + 1 >= argc) {
            usage();
        }
        *argindex += 2;
        return argv[nowindex + 1];
    } else if(strncmp(argv[nowindex], arg_string, strlen(arg_string)) == 0) {
        *argindex += 1;
        return argv[nowindex] + strlen(arg_string);
    } else {
        return NULL;
    }
}

int get_ascii_arg(int argc, char *argv[], char *arg_string, int escape, void (*usage)(), int *argindex) {
    char *getarg;

    if((getarg = get_delimiter_arg(argc, argv, arg_string, usage, argindex)) == NULL) {
        return -1;
    } else if(strlen(getarg) == 0) {
        usage();
        return -1;
    } else if(!isascii(getarg[0])) {
        usage();
        return -1;
    } else if(getarg[0] == escape && strlen(getarg) >= 2) {
        if(getarg[1] == 'n') {
            return '\n';
        } else if(getarg[1] == 't') {
            return '\t';
        } else {
            return getarg[0];
        }
    } else {
        return getarg[0];
    }
}

int get_ascii_optional_arg(int argc, char *argv[], char *arg_string, void (*usage)(), int *argindex) {
    char *getarg;

    if((getarg = get_delimiter_arg(argc, argv, arg_string, usage, argindex)) == NULL) {
        return -2;
    } else if(strlen(getarg) == 0) {
        return -1;
    } else if(!isascii(getarg[0])) {
        usage();
        return -2;
    } else {
        return getarg[0];
    }
}

FILE *openfile(char *filename, char *mode) {
    FILE *result;

    if((result = fopen(filename, mode)) == NULL) {
        fprintf(stderr, "cannot open file %s\n", filename);
        exit(EXIT_EXCEPTION);
    }
    return result;
}

