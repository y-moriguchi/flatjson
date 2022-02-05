/*
 * flat JSON
 *
 * Copyright (c) 2022 Yuichiro MORIGUCHI
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 **/
#define EXIT_ERROR 8
#define EXIT_EXCEPTION 4
#define EXIT_USAGE 2

extern void *xalloc(int size);
extern void init_buffer();
extern void append_buffer(char ch);
extern int equals_buffer(const char *str);
extern char *to_string_buffer();
extern int append_codepoint_buffer(int codepoint);
extern char *get_delimiter_arg(int argc, char *argv[], char *arg_string, void (*usage)(), int *argindex);
extern int get_ascii_arg(int argc, char *argv[], char *arg_string, int escape, void (*usage)(), int *argindex);
extern int get_ascii_optional_arg(int argc, char *argv[], char *arg_string, void (*usage)(), int *argindex);
extern FILE *openfile(char *filename, char *mode);

