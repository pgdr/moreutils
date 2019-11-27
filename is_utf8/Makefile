##
## Copyright (c) 2016 Julien Palard.
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
## 1. Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
## 2. Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
## IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
## OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
## IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
## INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
## NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
## DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
## THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
## THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

NAME = isutf8
SRC = main.c is_utf8.c
OBJ = $(SRC:.c=.o)

VERSION = 0
MINOR = 0
RELEASE = 0

LIB_SRC = is_utf8.c

LINKERNAME = lib$(NAME).so
SONAME = $(LINKERNAME).$(VERSION)
REALNAME = $(SONAME).$(MINOR).$(RELEASE)

CC = gcc
CFLAGS ?= -O3 -Wextra -Wall -ansi -Wstrict-prototypes

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) -o $(NAME) $(OBJ) $(LDFLAGS)

IS_UTF8_LIB:
	$(CC) --shared -fPIC $(CFLAGS) $(LDFLAGS) $(LIB_SRC) -o $(LINKERNAME)

all:
		@make $(NAME)

clean:
		rm -f $(NAME) $(LINKERNAME) $(OBJ) $(LIB_OBJ)

re:		clean all
