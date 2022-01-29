#
# dflatj
#
# Copyright (c) 2022 Yuichiro MORIGUCHI
#
# This software is released under the MIT License.
# http://opensource.org/licenses/mit-license.php
#

NAME  = flatj
SRCS  = flatj.c ../common.c
OBJS  = $(SRCS:.c=.o)
CC    = gcc
CFLAG = 

$(NAME) : $(OBJS)
	$(CC) -o $(NAME) $(OBJS)

.c.o:
	$(CC) -c $< -o $@

