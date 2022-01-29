/*
 * dflatj
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
#include <setjmp.h>
#include "../common.h"

#define INIT_STRING_LENGTH 20

static jmp_buf top;

void throw() {
    longjmp(top, EXIT_EXCEPTION);
}

static FILE *fpout;
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

char *to_string_buffer() {
    char *result;

    append_buffer('\0');
    result = (char *)xalloc(buffer_ptr - buffer);
    strcpy(result, buffer);
    return result;
}

typedef struct list {
    char *value;
    struct list *next;
    struct list *prev;
} line_list;

static line_list *list = NULL;
static line_list *list_ptr;
static line_list *prev_list = NULL;
static line_list *prev_list_ptr;
static char separator = ':';
static char index_prefix = '#';

void push_list(char *str) {
    line_list *element = xalloc(sizeof(line_list));

    element->value = str;
    element->next = NULL;
    if(list == NULL) {
        element->prev = NULL;
        list = list_ptr = element;
    } else {
        list_ptr->next = element;
        element->prev = list_ptr;
        list_ptr = element;
    }
}

void free_list(line_list *free_ptr) {
    line_list *ptr = free_ptr, *tmp;

    while(ptr != NULL) {
        tmp = ptr;
        ptr = ptr->next;
        free(tmp->value);
        tmp->next = tmp->prev = NULL;
        free(tmp);
    }
}

char *get_array_index(char *value) {
    if(strlen(value) > 0 && *value == index_prefix) {
        return value + 1;
    } else {
        return NULL;
    }
}

int is_continue(char *current, char *prev) {
    char *current_index_ptr, *prev_index_ptr;
    int current_index, prev_index;

    if((current_index_ptr = get_array_index(current)) != NULL && (prev_index_ptr = get_array_index(prev)) != NULL) {
        sscanf(current_index_ptr, "%d", &current_index);
        sscanf(prev_index_ptr, "%d", &prev_index);
        return current_index == prev_index;
    } else {
        return strcmp(current, prev) == 0;
    }
}

int string_suffix = '!';
enum state_check_number {
    CHECK_NUMBER_INIT,
    CHECK_NUMBER_NUMBER_START,
    CHECK_NUMBER_NUMBER,
    CHECK_NUMBER_AFTER_ZERO,
    CHECK_NUMBER_POINT_START,
    CHECK_NUMBER_POINT,
    CHECK_NUMBER_EXPONENT,
    CHECK_NUMBER_EXPONENT_NUMBER_START,
    CHECK_NUMBER_EXPONENT_NUMBER
};

int check_number(char *str) {
    enum state_check_number state = CHECK_NUMBER_INIT;
    char ch, *ptr = str;

    while(1) {
        ch = *ptr++;
        switch(state) {
        case CHECK_NUMBER_INIT:
            if(ch == '0') {
                state = CHECK_NUMBER_AFTER_ZERO;
            } else if(isdigit(ch)) {
                state = CHECK_NUMBER_NUMBER;
            } else if(ch == '-') {
                state = CHECK_NUMBER_NUMBER_START;
            } else {
                return 0;
            }
            break;

        case CHECK_NUMBER_NUMBER_START:
            if(ch == '0') {
                state = CHECK_NUMBER_AFTER_ZERO;
            } else if(isdigit(ch)) {
                state = CHECK_NUMBER_NUMBER;
            } else {
                return 0;
            }
            break;

        case CHECK_NUMBER_NUMBER:
            if(isdigit(ch)) {
                /* ok */
            } else if(ch == '.') {
                state = CHECK_NUMBER_POINT_START;
            } else if(ch == 'e' || ch == 'E') {
                state = CHECK_NUMBER_EXPONENT;
            } else {
                return ch == '\0';
            }
            break;

        case CHECK_NUMBER_AFTER_ZERO:
            if(ch == '.') {
                state = CHECK_NUMBER_POINT_START;
            } else if(ch == 'e' || ch == 'E') {
                state = CHECK_NUMBER_EXPONENT;
            } else {
                return ch == '\0';
            }
            break;

        case CHECK_NUMBER_POINT_START:
            if(isdigit(ch)) {
                state = CHECK_NUMBER_POINT;
            } else {
                return 0;
            }
            break;

        case CHECK_NUMBER_POINT:
            if(isdigit(ch)) {
                /* ok */
            } else if(ch == 'e' || ch == 'E') {
                state = CHECK_NUMBER_EXPONENT;
            } else {
                return ch == '\0';
            }
            break;

        case CHECK_NUMBER_EXPONENT:
            if(ch == '+') {
                state = CHECK_NUMBER_EXPONENT_NUMBER_START;
            } else if(ch == '-') {
                state = CHECK_NUMBER_EXPONENT_NUMBER_START;
            } else if(isdigit(ch)) {
                state = CHECK_NUMBER_EXPONENT_NUMBER;
            } else {
                return 0;
            }
            break;

        case CHECK_NUMBER_EXPONENT_NUMBER_START:
            if(isdigit(ch)) {
                state = CHECK_NUMBER_EXPONENT_NUMBER;
            } else {
                return 0;
            }
            break;

        case CHECK_NUMBER_EXPONENT_NUMBER:
            if(isdigit(ch)) {
                /* ok */
            } else {
                return ch == '\0';
            }
            break;

        default:
            fprintf(stderr, "internal error\n");
            throw();
            return 0;
        }
    }
}

int check_keyword(char *value) {
    return strcmp(value, "null") == 0 ||
        strcmp(value, "true") == 0 ||
        strcmp(value, "false") == 0 ||
        strcmp(value, "[]") == 0 ||
        strcmp(value, "{}") == 0;
}

void print_value(char *value) {
    char *string_value;

    if(check_keyword(value) || check_number(value)) {
        fprintf(fpout, "%s", value);
    } else if(string_suffix < 0) {
        fprintf(fpout, "\"%s\"", value);
    } else if(value[strlen(value) - 1] == string_suffix) {
        string_value = (char *)xalloc(strlen(value));
        strncpy(string_value, value, strlen(value) - 1);
        string_value[strlen(value) - 1] = '\0';
        fprintf(fpout, "\"%s\"", string_value);
        free(string_value);
    } else {
        fprintf(stderr, "malformed string format\n");
        fprintf(fpout, "\n");
        throw();
    }
}

void print_line() {
    line_list *current_ptr = list, *prev_ptr = prev_list, *prev_tmp;
    int bracket;

    if(prev_list != NULL) {
        while(current_ptr != list_ptr && prev_ptr != prev_list_ptr) {
            if(!is_continue(current_ptr->value, prev_ptr->value)) {
                break;
            }
            current_ptr = current_ptr->next;
            prev_ptr = prev_ptr->next;
        }

        if(prev_ptr != prev_list_ptr) {
            prev_tmp = prev_list_ptr->prev;
            for(; prev_tmp != prev_ptr; prev_tmp = prev_tmp->prev) {
                fprintf(fpout, "%c", get_array_index(prev_tmp->value) == NULL ? '}' : ']');
            }
        }
        fprintf(fpout, ",");
        bracket = 0;

        if(((current_ptr->prev != NULL && get_array_index(current_ptr->prev->value) == NULL) ||
                (prev_ptr->prev != NULL && get_array_index(prev_ptr->prev->value) == NULL)) &&
                (current_ptr->next == NULL || prev_ptr->next == NULL)) {
            fprintf(stderr, "malformed flatj format\n");
            fprintf(fpout, "\n");
            throw();
        }
    } else {
        bracket = 1;
    }

    if(current_ptr != list_ptr) {
        for(; current_ptr != list_ptr; current_ptr = current_ptr->next) {
            if(get_array_index(current_ptr->value) == NULL) {
                if(bracket) {
                    fprintf(fpout, "{");
                }
                fprintf(fpout, "\"%s\":", current_ptr->value);
            } else {
                if(bracket) {
                    fprintf(fpout, "[");
                }
            }
            bracket = 1;
        }
    }

    print_value(current_ptr->value);

    if(prev_list != NULL) {
        free_list(prev_list);
    }
    prev_list = list;
    prev_list_ptr = list_ptr;
    list = list_ptr = NULL;
}

void print_eof() {
    line_list *ptr;

    if(prev_list != NULL) {
        if(prev_list_ptr->prev != NULL) {
            ptr = prev_list_ptr->prev;
            for(; ptr != NULL; ptr = ptr->prev) {
                fprintf(fpout, "%c", get_array_index(ptr->value) == NULL ? '}' : ']');
            }
        }
        free_list(prev_list);
    }
}

void dflatj_input(FILE *fp) {
    int ch, newline = 1;

    init_buffer();
    while(1) {
        if((ch = getc(fp)) == EOF) {
            if(!newline) {
                push_list(to_string_buffer());
                init_buffer();
            }
            if(list != NULL) {
                print_line();
            }
            print_eof();
            return;
        } else if(separator == '\n' && ch == '\n' && newline) {
            print_line();
        } else if(ch == separator) {
            push_list(to_string_buffer());
            init_buffer();
        } else if(ch == '\n') {
            push_list(to_string_buffer());
            init_buffer();
            print_line();
        } else {
            append_buffer(ch);
        }
        newline = ch == '\n';
    }
}

void usage() {
    fprintf(stderr, "usage: dflatj [option] [-o output] [input]\n");
    fprintf(stderr, "option:\n");
    fprintf(stderr, "-F delimiter\n");
    fprintf(stderr, "-i index-prefix\n");
    fprintf(stderr, "-s string-suffix\n");
    exit(EXIT_USAGE);
}

int main(int argc, char *argv[]) {
    FILE *input = NULL;
    int argindex = 1, errcode = 0, argch;
    char *outfile = NULL;

    fpout = stdout;
    while(argindex < argc) {
        if(strcmp(argv[argindex], "-o") == 0) {
            if(argindex + 1 >= argc) {
                usage();
            }
            outfile = argv[argindex + 1];
            argindex += 2;
        } else if((argch = get_ascii_arg(argc, argv, "-F", '\\', usage, &argindex)) >= 0) {
            separator = (char)argch;
        } else if((argch = get_ascii_arg(argc, argv, "-i", -1, usage, &argindex)) >= 0) {
            index_prefix = (char)argch;
        } else if((argch = get_ascii_optional_arg(argc, argv, "-s", usage, &argindex)) >= -1) {
            string_suffix = argch;
        } else if(argv[argindex][0] == '-') {
            usage();
        } else {
            break;
        }
    }

    if(outfile != NULL) {
        fpout = openfile(outfile, "w");
    }

    if(argindex == argc) {
        if((errcode = setjmp(top)) == 0) {
            dflatj_input(stdin);
        }
    } else {
        input = openfile(argv[argindex], "r");
        if((errcode = setjmp(top)) == 0) {
            dflatj_input(input);
        }
        fclose(input);
    }
    fprintf(fpout, "\n");
    if(outfile != NULL) {
        fclose(fpout);
    }
    return errcode;
}


