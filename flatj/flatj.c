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
#include "../common.h"

static FILE *fpout;

int parse_json(FILE *fp);

static jmp_buf top;

void throw() {
    longjmp(top, EXIT_EXCEPTION);
}

int surrogate_to_codepoint(int high, int low) {
    int h = high - 0xD800, l = low - 0xDC00;

    return (h << 10) + l + 0x10000;
}

static char index_prefix = '#';

char *int_to_string(int value) {
    char buf[50];
    char *result;

    sprintf(buf, "%c%d", index_prefix, value);
    result = (char *)xalloc(strlen(buf) + 1);
    strcpy(result, buf);
    return result;
}

char *literal_to_string(char *literal) {
    char *result;

    result = (char *)xalloc(strlen(literal) + 1);
    strcpy(result, literal);
    return result;
}

typedef struct list {
    char *value;
    struct list *next;
    struct list *prev;
} stack_list;

static stack_list *stack = NULL;
static stack_list *stack_ptr;
static char separator = '\t';

void print_stack(FILE *fpout) {
    stack_list *p;

    for(p = stack; p != NULL; p = p->next) {
        if(p != stack) {
            fprintf(fpout, "%c", separator);
        }
        fprintf(fpout, "%s", p->value);
    }
    if(separator == '\n') {
        fprintf(fpout, "\n");
    }
    fprintf(fpout, "\n");
}

void push_stack(char *str) {
    stack_list *element = xalloc(sizeof(stack_list));

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

int nextchar(FILE *fp) {
    int ch;

    while((ch = getc(fp)) == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
    return ch;
}

int nextcharline(FILE *fp) {
    int ch;

    while((ch = getc(fp)) == ' ' || ch == '\t' || ch == '\r');
    return ch;
}

static int suffix_char = -1;
static int expand_escape = 0;
enum state_parse_string {
    PARSE_STRING_INIT,
    PARSE_STRING_STRING,
    PARSE_STRING_BACKSLASH,
    PARSE_STRING_CODEPOINT
};

char *parse_string(FILE *fp, int suffix) {
    enum state_parse_string state = PARSE_STRING_INIT;
    int ch, codepoint, codepoint_count, surrogate = 0;

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
                state = PARSE_STRING_STRING;
            } else {
                ungetc(ch, fp);
                return NULL;
            }
            break;

        case PARSE_STRING_STRING:
            if(ch != '\\' && surrogate) {
                fprintf(stderr, "invalid surrogate pair\n");
                throw();
            }
            if(ch == '\"') {
                if(surrogate) {
                    fprintf(stderr, "invalid surrogate pair\n");
                    throw();
                } else if(suffix >= 0) {
                    append_buffer((char)suffix);
                }
                return to_string_buffer();
            } else if(ch == '\\') {
                state = PARSE_STRING_BACKSLASH;
            } else if(ch >= 0x20) {
                append_buffer(ch);
            }
            break;

        case PARSE_STRING_BACKSLASH:
            if(ch != 'u' && surrogate) {
                fprintf(stderr, "invalid surrogate pair\n");
                throw();
            }
            switch(ch) {
            case '\"':  case '/':
                append_buffer(ch);
                state = PARSE_STRING_STRING;
                break;
            case '\\': case 'b': case 'f': case 'n': case 'r': case 't':
                append_buffer('\\');
                append_buffer(ch);
                state = PARSE_STRING_STRING;
                break;
            case 'u':
                codepoint = 0;
                codepoint_count = 0;
                if(!expand_escape) {
                    append_buffer('\\');
                    append_buffer(ch);
                }
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
                if(!expand_escape) {
                    append_buffer(ch);
                }
                codepoint_count++;
            } else {
                if(surrogate) {
                    if(codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                        if(expand_escape) {
                            if(!append_codepoint_buffer(surrogate_to_codepoint(surrogate, codepoint))) {
                                fprintf(stderr, "invalid codepoint\n");
                                throw();
                            }
                        }
                        surrogate = 0;
                    } else {
                        fprintf(stderr, "invalid surrogate pair\n");
                        throw();
                    }
                } else {
                    if(codepoint >= 0xD800 && codepoint <= 0xDCFF) {
                        surrogate = codepoint;
                    } else if(expand_escape) {
                        if(!append_codepoint_buffer(codepoint)) {
                            fprintf(stderr, "invalid codepoint\n");
                            throw();
                        }
                    }
                }
                ungetc(ch, fp);
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
    int ch;
    char *result, *restring, buf[256];
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
    return to_string_buffer();
}

char *parse_literal(FILE *fp) {
    int ch;

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
    PARSE_OBJECT_KEY_INIT,
    PARSE_OBJECT_KEY,
    PARSE_OBJECT_NEXT,
    PARSE_OBJECT_RESULT
};

int parse_object(FILE *fp) {
    enum state_parse_object state = PARSE_OBJECT_INIT;
    int ch;
    char *str;

    while(1) {
        if((ch = nextchar(fp)) == EOF) {
            fprintf(stderr, "unexpected EOF\n");
            throw();
        }

        switch(state) {
        case PARSE_OBJECT_INIT:
            if(ch == '{') {
                state = PARSE_OBJECT_KEY_INIT;
            } else {
                ungetc(ch, fp);
                return 0;
            }
            break;

        case PARSE_OBJECT_KEY_INIT:
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

        case PARSE_OBJECT_KEY:
            ungetc(ch, fp);
            if((str = parse_string(fp, -1)) != NULL) {
                push_stack(str);
                state = PARSE_OBJECT_NEXT;
            } else {
                fprintf(stderr, "string needed\n");
                throw();
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
    PARSE_ARRAY_LIST_INIT,
    PARSE_ARRAY_LIST,
    PARSE_ARRAY_RESULT
};

int parse_array(FILE *fp) {
    enum state_parse_array state = PARSE_ARRAY_INIT;
    int ch, index = 0;

    while(1) {
        if((ch = nextchar(fp)) == EOF) {
            fprintf(stderr, "unexpected EOF\n");
            throw();
        }

        switch(state) {
        case PARSE_ARRAY_INIT:
            if(ch == '[') {
                state = PARSE_ARRAY_LIST_INIT;
            } else {
                ungetc(ch, fp);
                return 0;
            }
            break;

        case PARSE_ARRAY_LIST_INIT:
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

        case PARSE_ARRAY_LIST:
            ungetc(ch, fp);
            push_stack(int_to_string(index++));
            parse_json(fp);
            pop_stack();
            state = PARSE_ARRAY_RESULT;
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
    } else if((result = parse_string(fp, suffix_char)) != NULL || (result = parse_number(fp)) != NULL || (result = parse_literal(fp)) != NULL) {
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
    int ch;

    parse_json(fp);
    if((ch = nextchar(fp)) != EOF) {
        fprintf(stderr, "unexpected EOF\n");
        throw();
    }
}

void usage() {
    fprintf(stderr, "usage: flatj [option] [-E] [-o output] [input]\n");
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
            suffix_char = argch;
        } else if(strcmp(argv[argindex], "-E") == 0) {
            expand_escape = 1;
            argindex++;
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
            parse_json_root(stdin);
        }
    } else {
        input = openfile(argv[argindex], "r");
        if((errcode = setjmp(top)) == 0) {
            parse_json_root(input);
        }
        fclose(input);
    }
    if(outfile != NULL) {
        fclose(fpout);
    }
    return errcode;
}

