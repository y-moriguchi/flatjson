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

#define INIT_STRING_LENGTH 20

void *xalloc(int size) {
    void *result;

    if((result = malloc(size)) == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_ERROR);
    }
    return result;
}

static int buffer_len;
static char *buffer = NULL;
static char *buffer_ptr;

void init_buffer() {
    if(buffer != NULL) {
        free(buffer);
    }
    buffer = buffer_ptr = (char *)xalloc(INIT_STRING_LENGTH);
    buffer_len = INIT_STRING_LENGTH;
}

void append_buffer(char ch) {
    char *tmp;

    if(buffer_ptr - buffer >= buffer_len - 1) {
        tmp = buffer;
        buffer = (char *)xalloc(buffer_len * 2);
        memcpy(buffer, tmp, (buffer_ptr - tmp) * sizeof(char));
        buffer_ptr = buffer + buffer_len - 1;
        buffer_len *= 2;
        free(tmp);
    }
    *buffer_ptr++ = ch;
}

int equals_buffer(const char *str) {
    int result;

    append_buffer('\0');
    result = strcmp(buffer, str);
    buffer_ptr--;
    return !result;
}

char *to_string_buffer() {
    char *result;

    append_buffer('\0');
    result = (char *)xalloc(buffer_ptr - buffer);
    strcpy(result, buffer);
    return result;
}

int append_codepoint_buffer(int codepoint) {
    if(codepoint < 0x80) {
        append_buffer((char)codepoint);
    } else if(codepoint < 0x800) {
        append_buffer(0xd0 | (codepoint >> 6));
        append_buffer(0x80 | (codepoint & 0x03f));
    } else if(codepoint < 0x10000) {
        append_buffer(0xe0 | (codepoint >> 12));
        append_buffer(0x80 | ((codepoint >> 6) & 0x3f));
        append_buffer(0x80 | (codepoint & 0x3f));
    } else if(codepoint < 0x110000) {
        append_buffer(0xf0 | (codepoint >> 18));
        append_buffer(0x80 | ((codepoint >> 12) & 0x3f));
        append_buffer(0x80 | ((codepoint >> 6) & 0x3f));
        append_buffer(0x80 | (codepoint & 0x3f));
    } else {
        return 0;
    }
    return 1;
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

