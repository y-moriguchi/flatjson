/*
 * flatj
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
#include <math.h>
#include <setjmp.h>

#define INIT_STRING_LENGTH 20
#define EXIT_EXCEPTION 4
#define EXIT_USAGE 2

static FILE *fpout;

int parse_json(FILE *fp);

static jmp_buf top;

void throw() {
    longjmp(top, EXIT_EXCEPTION);
}

int number_prefix = '#';

char *int_to_string(int value) {
    char buf[50];
    char *result;

    sprintf(buf, "%c%d", number_prefix, value);
    result = (char *)malloc(strlen(buf) + 1);
    strcpy(result, buf);
    return result;
}

char *literal_to_string(char *literal) {
    char *result;

    result = (char *)malloc(strlen(literal) + 1);
    strcpy(result, literal);
    return result;
}

static int buffer_len;
static char *buffer = NULL;
static char *buffer_ptr;

void init_buffer() {
    if(buffer != NULL) {
        free(buffer);
    }
    buffer = buffer_ptr = (char *)malloc(INIT_STRING_LENGTH);
    buffer_len = INIT_STRING_LENGTH;
}

void append_buffer(char ch) {
    char *tmp;

    if(buffer_ptr - buffer >= buffer_len - 1) {
        tmp = buffer;
        buffer = (char *)malloc(buffer_len * 2);
        memcpy(buffer, tmp, (buffer_ptr - tmp) * sizeof(char));
        buffer_ptr = buffer + buffer_len - 1;
        buffer_len *= 2;
        free(tmp);
    }
    *buffer_ptr++ = ch;
}

void append_codepoint_buffer(int codepoint) {
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
        fprintf(stderr, "invalid codepoint\n");
        throw();
    }
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
    result = (char *)malloc(buffer_ptr - buffer);
    strcpy(result, buffer);
    return result;
}

typedef struct list {
    char *value;
    struct list *next;
    struct list *prev;
} stack_list;

static stack_list *stack = NULL;
static stack_list *stack_ptr;
static char separator = ':';

void print_stack(FILE *fpout) {
    stack_list *p;

    for(p = stack; p != NULL; p = p->next) {
        if(p != stack) {
            fprintf(fpout, "%c", separator);
        }
        fprintf(fpout, "%s", p->value);
    }
    fprintf(fpout, "\n");
}

void push_stack(char *str) {
    stack_list *element = malloc(sizeof(stack_list));

    element->value = str;
    element->next = NULL;
    if(stack == NULL) {
        element->prev = NULL;
        stack = stack_ptr = element;
    } else {
        stack_ptr->next = element;
        element->prev = stack_ptr;
        stack_ptr = element;
    }
}

void pop_stack() {
    stack_list *ptr = stack_ptr;

    if(ptr == NULL) {
        fprintf(stderr, "stack empty\n");
        throw();
    }
    stack_ptr = stack_ptr->prev;
    if(stack_ptr == NULL) {
        stack = NULL;
    } else {
        stack_ptr->next = NULL;
        free(ptr->value);
        ptr->value = NULL;
    }
    free(ptr);
}

char nextchar(FILE *fp) {
    char ch;

    while((ch = getc(fp)) == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
    return ch;
}

enum state_parse_string {
    PARSE_STRING_INIT,
    PARSE_STRING_STRING,
    PARSE_STRING_BACKSLASH,
    PARSE_STRING_CODEPOINT
};

char *parse_string(FILE *fp, int quote) {
    enum state_parse_string state = PARSE_STRING_INIT;
    char ch;
    int codepoint, codepoint_count;

    ch = nextchar(fp);
    ungetc(ch, fp);
    init_buffer();
    while(1) {
        if((ch = getc(fp)) == EOF) {
            fprintf(stderr, "unexpected EOF\n");
            throw();
        }

        switch(state) {
        case PARSE_STRING_INIT:
            if(ch == '\"') {
                if(quote > 0) {
                    append_buffer(quote);
                }
                state = PARSE_STRING_STRING;
            } else {
                ungetc(ch, fp);
                return NULL;
            }
            break;

        case PARSE_STRING_STRING:
            if(ch == '\"') {
                if(quote > 0) {
                    append_buffer(quote);
                }
                return to_string_buffer();
            } else if(ch == '\\') {
                state = PARSE_STRING_BACKSLASH;
            } else if(ch != '\n') {
                if(ch == number_prefix) {
                    append_buffer('\\');
                }
                append_buffer(ch);
            }
            break;

        case PARSE_STRING_BACKSLASH:
            switch(ch) {
            case '\"':  case '/':
                append_buffer(ch);
                state = PARSE_STRING_STRING;
                break;
            case '\\':
                append_buffer('\\');
                append_buffer(ch);
                state = PARSE_STRING_STRING;
                break;
            case 'b':
                append_buffer('\\');
                append_buffer('b');
                state = PARSE_STRING_STRING;
                break;
            case 'f':
                append_buffer('\\');
                append_buffer('f');
                state = PARSE_STRING_STRING;
                break;
            case 'n':
                append_buffer('\\');
                append_buffer('n');
                state = PARSE_STRING_STRING;
                break;
            case 'r':
                append_buffer('\\');
                append_buffer('r');
                state = PARSE_STRING_STRING;
                break;
            case 't':
                append_buffer('\\');
                append_buffer('t');
                state = PARSE_STRING_STRING;
                break;
            case 'u':
                codepoint = 0;
                codepoint_count = 0;
                state = PARSE_STRING_CODEPOINT;
                break;
            default:
                fprintf(stderr, "invalid escape sequence\n");
                throw();
                break;
            }
            break;

        case PARSE_STRING_CODEPOINT:
            if(codepoint_count < 4) {
                if(isdigit(ch)) {
                    codepoint = (codepoint << 4) + (ch - '0');
                } else if(ch >= 'A' && ch <= 'F') {
                    codepoint = (codepoint << 4) + ((ch - 'A') + 10);
                } else if(ch >= 'a' && ch <= 'f') {
                    codepoint = (codepoint << 4) + ((ch - 'a') + 10);
                } else {
                    fprintf(stderr, "invalid escape sequence\n");
                    throw();
                }
                codepoint_count++;
            } else {
                ungetc(ch, fp);
                append_codepoint_buffer(codepoint);
                state = PARSE_STRING_STRING;
            }
        }
    }
    fprintf(stderr, "internal error\n");
    throw();
    return NULL;
}

enum state_parse_number {
    PARSE_NUMBER_INIT,
    PARSE_NUMBER_NUMBER_START,
    PARSE_NUMBER_NUMBER,
    PARSE_NUMBER_AFTER_ZERO,
    PARSE_NUMBER_POINT_START,
    PARSE_NUMBER_POINT,
    PARSE_NUMBER_EXPONENT,
    PARSE_NUMBER_EXPONENT_NUMBER_START,
    PARSE_NUMBER_EXPONENT_NUMBER
};

char *parse_number(FILE *fp) {
    enum state_parse_number state = PARSE_NUMBER_INIT;
    char ch, *result, *restring, buf[256];
    double parsed;

    ch = nextchar(fp);
    ungetc(ch, fp);
    init_buffer();
    while(1) {
        ch = getc(fp);
        switch(state) {
        case PARSE_NUMBER_INIT:
            if(ch == '0') {
                append_buffer(ch);
                state = PARSE_NUMBER_AFTER_ZERO;
            } else if(isdigit(ch)) {
                append_buffer(ch);
                state = PARSE_NUMBER_NUMBER;
            } else if(ch == '-') {
                append_buffer(ch);
                state = PARSE_NUMBER_NUMBER_START;
            } else {
                ungetc(ch, fp);
                return NULL;
            }
            break;

        case PARSE_NUMBER_NUMBER_START:
            if(ch == '0') {
                append_buffer(ch);
                state = PARSE_NUMBER_AFTER_ZERO;
            } else if(isdigit(ch)) {
                append_buffer(ch);
                state = PARSE_NUMBER_NUMBER;
            } else {
                fprintf(stderr, "invalid number\n");
                throw();
            }
            break;

        case PARSE_NUMBER_NUMBER:
            if(isdigit(ch)) {
                append_buffer(ch);
            } else if(ch == '.') {
                append_buffer(ch);
                state = PARSE_NUMBER_POINT_START;
            } else if(ch == 'e' || ch == 'E') {
                append_buffer(ch);
                state = PARSE_NUMBER_EXPONENT;
            } else {
                goto matched;
            }
            break;

        case PARSE_NUMBER_AFTER_ZERO:
            if(ch == '.') {
                append_buffer(ch);
                state = PARSE_NUMBER_POINT_START;
            } else if(ch == 'e' || ch == 'E') {
                append_buffer(ch);
                state = PARSE_NUMBER_EXPONENT;
            } else {
                goto matched;
            }
            break;

        case PARSE_NUMBER_POINT_START:
            if(isdigit(ch)) {
                append_buffer(ch);
                state = PARSE_NUMBER_POINT;
            } else {
                fprintf(stderr, "invalid number\n");
                throw();
            }
            break;

        case PARSE_NUMBER_POINT:
            if(isdigit(ch)) {
                append_buffer(ch);
            } else if(ch == 'e' || ch == 'E') {
                append_buffer(ch);
                state = PARSE_NUMBER_EXPONENT;
            } else {
                goto matched;
            }
            break;

        case PARSE_NUMBER_EXPONENT:
            if(ch == '+') {
                state = PARSE_NUMBER_EXPONENT_NUMBER_START;
            } else if(ch == '-') {
                append_buffer(ch);
                state = PARSE_NUMBER_EXPONENT_NUMBER_START;
            } else if(isdigit(ch)) {
                append_buffer(ch);
                state = PARSE_NUMBER_EXPONENT_NUMBER;
            } else {
                fprintf(stderr, "invalid number\n");
                throw();
            }
            break;

        case PARSE_NUMBER_EXPONENT_NUMBER_START:
            if(isdigit(ch)) {
                append_buffer(ch);
                state = PARSE_NUMBER_EXPONENT_NUMBER;
            } else {
                fprintf(stderr, "invalid number\n");
                throw();
            }
            break;

        case PARSE_NUMBER_EXPONENT_NUMBER:
            if(isdigit(ch)) {
                append_buffer(ch);
            } else {
                goto matched;
            }
            break;

        default:
            fprintf(stderr, "internal error\n");
            throw();
            break;
        }
    }
    fprintf(stderr, "internal error\n");
    throw();
    return NULL;

    matched:
    ungetc(ch, fp);
    result = to_string_buffer();
    sscanf(result, "%lf", &parsed);
    sprintf(buf, "%.15g", parsed);
    restring = (char *)malloc(strlen(buf) + 1);
    strcpy(restring, buf);
    free(result);
    return restring;
}

char *parse_literal(FILE *fp) {
    char ch;

    ch = nextchar(fp);
    ungetc(ch, fp);
    init_buffer();
    while(1) {
        if(isalpha(ch = getc(fp))) {
            append_buffer(ch);
        } else {
            ungetc(ch, fp);
            break;
        }
    }

    if(equals_buffer("null") || equals_buffer("true") || equals_buffer("false")) {
        return to_string_buffer();
    } else {
        fprintf(stderr, "invalid literal\n");
        throw();
        return NULL;
    }
}

enum state_parse_object {
    PARSE_OBJECT_INIT,
    PARSE_OBJECT_KEY,
    PARSE_OBJECT_NEXT,
    PARSE_OBJECT_RESULT
};

int parse_object(FILE *fp) {
    enum state_parse_object state = PARSE_OBJECT_INIT;
    char ch;
    char *str;

    while(1) {
        if((ch = nextchar(fp)) == EOF) {
            fprintf(stderr, "unexpected EOF\n");
            throw();
        }

        switch(state) {
        case PARSE_OBJECT_INIT:
            if(ch == '{') {
                state = PARSE_OBJECT_KEY;
            } else {
                ungetc(ch, fp);
                return 0;
            }
            break;

        case PARSE_OBJECT_KEY:
            if(ch == '}') {
                push_stack(literal_to_string("{}"));
                print_stack(fpout);
                pop_stack();
                return 1;
            } else {
                ungetc(ch, fp);
                if((str = parse_string(fp, -1)) != NULL) {
                    push_stack(str);
                    state = PARSE_OBJECT_NEXT;
                } else {
                    fprintf(stderr, "string needed\n");
                    throw();
                }
            }
            break;

        case PARSE_OBJECT_NEXT:
            if(ch == ':') {
                parse_json(fp);
                state = PARSE_OBJECT_RESULT;
            } else {
                fprintf(stderr, "comma needed\n");
                throw();
            }
            break;

        case PARSE_OBJECT_RESULT:
            if(ch == ',') {
                pop_stack();
                state = PARSE_OBJECT_KEY;
            } else if(ch == '}') {
                pop_stack();
                return 1;
            }
            break;

        default:
            fprintf(stderr, "internal error\n");
            throw();
            break;
        }
    }
    fprintf(stderr, "internal error\n");
    throw();
    return 0;
}

enum state_parse_array {
    PARSE_ARRAY_INIT,
    PARSE_ARRAY_LIST,
    PARSE_ARRAY_RESULT
};

int parse_array(FILE *fp) {
    enum state_parse_array state = PARSE_ARRAY_INIT;
    char ch;
    int index = 0;

    while(1) {
        if((ch = nextchar(fp)) == EOF) {
            fprintf(stderr, "unexpected EOF\n");
            throw();
        }

        switch(state) {
        case PARSE_ARRAY_INIT:
            if(ch == '[') {
                state = PARSE_ARRAY_LIST;
            } else {
                ungetc(ch, fp);
                return 0;
            }
            break;

        case PARSE_ARRAY_LIST:
            if(ch == ']') {
                push_stack(literal_to_string("[]"));
                print_stack(fpout);
                pop_stack();
                return 1;
            } else {
                ungetc(ch, fp);
                push_stack(int_to_string(index++));
                parse_json(fp);
                pop_stack();
                state = PARSE_ARRAY_RESULT;
            }
            break;

        case PARSE_ARRAY_RESULT:
            if(ch == ',') {
                state = PARSE_ARRAY_LIST;
            } else if(ch == ']') {
                return 1;
            } else {
                fprintf(stderr, "invalid array\n");
                throw();
            }
            break;

        default:
            fprintf(stderr, "internal error\n");
            throw();
            break;
        }
    }
    fprintf(stderr, "internal error\n");
    throw();
    return 0;
}

int parse_json(FILE *fp) {
    char *result;

    if(parse_object(fp)) {
        /* ok */
    } else if(parse_array(fp)) {
        /* ok */
    } else if((result = parse_string(fp, '`')) != NULL || (result = parse_number(fp)) != NULL || (result = parse_literal(fp)) != NULL) {
        push_stack(result);
        print_stack(fpout);
        pop_stack();
    } else {
        fprintf(stderr, "invalid JSON\n");
        throw();
    }
    return 1;
}

void parse_json_root(FILE *fp) {
    char ch;

    parse_json(fp);
    if((ch = nextchar(fp)) != EOF) {
        fprintf(stderr, "unexpected EOF\n");
        throw();
    }
}

void usage() {
    fprintf(stderr, "usage: flatj [-o output] [input]\n");
    exit(EXIT_USAGE);
}

FILE *openfile(char *filename, char *mode) {
    FILE *result;

    if((result = fopen(filename, mode)) == NULL) {
        fprintf(stderr, "cannot open file %s\n", filename);
        exit(EXIT_EXCEPTION);
    }
    return result;
}

int main(int argc, char *argv[]) {
    FILE *input = NULL;
    int argindex = 1, errcode = 0, outputflg = 0;

    fpout = stdout;
    while(argindex < argc) {
        if(strcmp(argv[argindex], "-o") == 0) {
            if(argindex + 1 >= argc) {
                usage();
            }
            fpout = openfile(argv[argindex + 1], "w");
            outputflg = 1;
            argindex += 2;
        } else if(strcmp(argv[argindex], "-F") == 0) {
            if(argindex + 1 >= argc) {
                usage();
            } else if(strlen(argv[argindex + 1]) == 0) {
                usage();
            }
            separator = argv[argindex + 1][0];
            argindex += 2;
        } else if(strcmp(argv[argindex], "-n") == 0) {
            if(argindex + 1 >= argc) {
                usage();
            } else if(strlen(argv[argindex + 1]) == 0) {
                usage();
            }
            number_prefix = argv[argindex + 1][0];
            argindex += 2;
        } else {
            break;
        }
    }

    if(argindex == argc) {
        if((errcode = setjmp(top)) == 0) {
            parse_json_root(stdin);
        }
    } else {
        input = openfile(argv[argindex], "r");
        if((errcode = setjmp(top)) == 0) {
            parse_json_root(input);
        }
        fclose(input);
        return errcode;
    }
    if(outputflg) {
        fclose(fpout);
    }
    return errcode;
}

