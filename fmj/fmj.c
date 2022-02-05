/*
 * fmj
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

static FILE *fpout;

int parse_json(FILE *fp);

static jmp_buf top;

void throw() {
    longjmp(top, EXIT_EXCEPTION);
}

void writech(int ch) {
    fprintf(fpout, "%c", ch);
}

static int indent = 0;
static int indent_size = 2;
static int pretty = 1;

void print_indent() {
    int i;

    if(pretty) {
        writech('\n');
        for(i = 0; i < indent; i++) {
            writech(' ');
        }
    }
}

void indent_right() {
    indent += indent_size;
}

void indent_left() {
    if(indent <= 0) {
        fprintf(stderr, "internal error\n");
        throw();
    }
    indent -= indent_size;
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

enum state_parse_string {
    PARSE_STRING_INIT,
    PARSE_STRING_STRING,
    PARSE_STRING_BACKSLASH,
    PARSE_STRING_CODEPOINT
};

int parse_string(FILE *fp) {
    enum state_parse_string state = PARSE_STRING_INIT;
    int ch, codepoint, codepoint_count, surrogate = 0;

    ch = nextchar(fp);
    ungetc(ch, fp);
    while(1) {
        if((ch = getc(fp)) == EOF) {
            fprintf(stderr, "unexpected EOF\n");
            throw();
        }

        switch(state) {
        case PARSE_STRING_INIT:
            if(ch == '\"') {
                writech(ch);
                state = PARSE_STRING_STRING;
            } else {
                ungetc(ch, fp);
                return 0;
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
                }
                writech(ch);
                return 1;
            } else if(ch == '\\') {
                state = PARSE_STRING_BACKSLASH;
            } else if(ch >= 0x20) {
                writech(ch);
            }
            break;

        case PARSE_STRING_BACKSLASH:
            if(ch != 'u' && surrogate) {
                fprintf(stderr, "invalid surrogate pair\n");
                throw();
            }
            switch(ch) {
            case '\"':  case '/':
                writech(ch);
                state = PARSE_STRING_STRING;
                break;
            case '\\': case 'b': case 'f': case 'n': case 'r': case 't':
                writech('\\');
                writech(ch);
                state = PARSE_STRING_STRING;
                break;
            case 'u':
                codepoint = 0;
                codepoint_count = 0;
                writech('\\');
                writech(ch);
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
                writech(ch);
                codepoint_count++;
            } else {
                if(surrogate) {
                    if(codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                        surrogate = 0;
                    } else {
                        fprintf(stderr, "invalid surrogate pair\n");
                        throw();
                    }
                } else {
                    if(codepoint >= 0xD800 && codepoint <= 0xDCFF) {
                        surrogate = codepoint;
                    }
                }
                ungetc(ch, fp);
                state = PARSE_STRING_STRING;
            }
        }
    }
    fprintf(stderr, "internal error\n");
    throw();
    return 0;
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

int parse_number(FILE *fp) {
    enum state_parse_number state = PARSE_NUMBER_INIT;
    int ch;

    ch = nextchar(fp);
    ungetc(ch, fp);
    while(1) {
        ch = getc(fp);
        switch(state) {
        case PARSE_NUMBER_INIT:
            if(ch == '0') {
                writech(ch);
                state = PARSE_NUMBER_AFTER_ZERO;
            } else if(isdigit(ch)) {
                writech(ch);
                state = PARSE_NUMBER_NUMBER;
            } else if(ch == '-') {
                writech(ch);
                state = PARSE_NUMBER_NUMBER_START;
            } else {
                ungetc(ch, fp);
                return 0;
            }
            break;

        case PARSE_NUMBER_NUMBER_START:
            if(ch == '0') {
                writech(ch);
                state = PARSE_NUMBER_AFTER_ZERO;
            } else if(isdigit(ch)) {
                writech(ch);
                state = PARSE_NUMBER_NUMBER;
            } else {
                fprintf(stderr, "invalid number\n");
                throw();
            }
            break;

        case PARSE_NUMBER_NUMBER:
            if(isdigit(ch)) {
                writech(ch);
            } else if(ch == '.') {
                writech(ch);
                state = PARSE_NUMBER_POINT_START;
            } else if(ch == 'e' || ch == 'E') {
                writech(ch);
                state = PARSE_NUMBER_EXPONENT;
            } else {
                goto matched;
            }
            break;

        case PARSE_NUMBER_AFTER_ZERO:
            if(ch == '.') {
                writech(ch);
                state = PARSE_NUMBER_POINT_START;
            } else if(ch == 'e' || ch == 'E') {
                writech(ch);
                state = PARSE_NUMBER_EXPONENT;
            } else {
                goto matched;
            }
            break;

        case PARSE_NUMBER_POINT_START:
            if(isdigit(ch)) {
                writech(ch);
                state = PARSE_NUMBER_POINT;
            } else {
                fprintf(stderr, "invalid number\n");
                throw();
            }
            break;

        case PARSE_NUMBER_POINT:
            if(isdigit(ch)) {
                writech(ch);
            } else if(ch == 'e' || ch == 'E') {
                writech(ch);
                state = PARSE_NUMBER_EXPONENT;
            } else {
                goto matched;
            }
            break;

        case PARSE_NUMBER_EXPONENT:
            if(ch == '+') {
                state = PARSE_NUMBER_EXPONENT_NUMBER_START;
            } else if(ch == '-') {
                writech(ch);
                state = PARSE_NUMBER_EXPONENT_NUMBER_START;
            } else if(isdigit(ch)) {
                writech(ch);
                state = PARSE_NUMBER_EXPONENT_NUMBER;
            } else {
                fprintf(stderr, "invalid number\n");
                throw();
            }
            break;

        case PARSE_NUMBER_EXPONENT_NUMBER_START:
            if(isdigit(ch)) {
                writech(ch);
                state = PARSE_NUMBER_EXPONENT_NUMBER;
            } else {
                fprintf(stderr, "invalid number\n");
                throw();
            }
            break;

        case PARSE_NUMBER_EXPONENT_NUMBER:
            if(isdigit(ch)) {
                writech(ch);
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
    return 0;

    matched:
    ungetc(ch, fp);
    return 1;
}

int parse_literal(FILE *fp) {
    int ch;
    char buf[10], *ptr = buf;

    ch = nextchar(fp);
    ungetc(ch, fp);
    while(1) {
        if(isalpha(ch = getc(fp))) {
            writech(ch);
            *ptr++ = ch;
            if(ptr - buf > 5) {
                fprintf(stderr, "invalid literal\n");
                throw();
                return 0;
            }
        } else {
            ungetc(ch, fp);
            break;
        }
    }

    *ptr = '\0';
    if(strcmp(buf, "null") == 0 || strcmp(buf, "true") == 0 || strcmp(buf, "false") == 0) {
        return 1;
    } else {
        fprintf(stderr, "invalid literal\n");
        throw();
        return 0;
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
                writech(ch);
                state = PARSE_OBJECT_KEY_INIT;
            } else {
                ungetc(ch, fp);
                return 0;
            }
            break;

        case PARSE_OBJECT_KEY_INIT:
            if(ch == '}') {
                writech(ch);
                return 1;
            } else {
                indent_right();
                print_indent();
                ungetc(ch, fp);
                if(parse_string(fp)) {
                    state = PARSE_OBJECT_NEXT;
                } else {
                    fprintf(stderr, "string needed\n");
                    throw();
                }
            }
            break;

        case PARSE_OBJECT_KEY:
            ungetc(ch, fp);
            if(parse_string(fp)) {
                state = PARSE_OBJECT_NEXT;
            } else {
                fprintf(stderr, "string needed\n");
                throw();
            }
            break;

        case PARSE_OBJECT_NEXT:
            if(ch == ':') {
                writech(ch);
                writech(' ');
                parse_json(fp);
                state = PARSE_OBJECT_RESULT;
            } else {
                fprintf(stderr, "comma needed\n");
                throw();
            }
            break;

        case PARSE_OBJECT_RESULT:
            if(ch == ',') {
                writech(ch);
                print_indent();
                state = PARSE_OBJECT_KEY;
            } else if(ch == '}') {
                indent_left();
                print_indent();
                writech(ch);
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
                writech(ch);
                state = PARSE_ARRAY_LIST_INIT;
            } else {
                ungetc(ch, fp);
                return 0;
            }
            break;

        case PARSE_ARRAY_LIST_INIT:
            if(ch == ']') {
                writech(ch);
                return 1;
            } else {
                indent_right();
                print_indent();
                ungetc(ch, fp);
                parse_json(fp);
                state = PARSE_ARRAY_RESULT;
            }
            break;

        case PARSE_ARRAY_LIST:
            ungetc(ch, fp);
            parse_json(fp);
            state = PARSE_ARRAY_RESULT;
            break;

        case PARSE_ARRAY_RESULT:
            if(ch == ',') {
                writech(ch);
                print_indent();
                state = PARSE_ARRAY_LIST;
            } else if(ch == ']') {
                indent_left();
                print_indent();
                writech(ch);
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
    } else if(parse_string(fp) || parse_number(fp) || parse_literal(fp)) {
        /* ok */
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
    fprintf(stderr, "usage: fmj [-m] [-o output] [input]\n");
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
        } else if(strcmp(argv[argindex], "-m") == 0) {
            pretty = 0;
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
    writech('\n');
    if(outfile != NULL) {
        fclose(fpout);
    }
    return errcode;
}

