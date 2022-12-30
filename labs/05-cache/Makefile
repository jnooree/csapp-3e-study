#
# Student makefile for Cache Lab
# Note: requires a 64-bit x86-64 system
#
CC = gcc
CFLAGS = -g -Wall -Werror -std=gnu99 -m64

all: csim test-trans tracegen

csim: csim.c cache.c params.c cachelab.o
	$(CC) $(CFLAGS) -o $@ $^

test-trans: test-trans.c trans.o cachelab.o
	$(CC) $(CFLAGS) -o $@ $^

tracegen: tracegen.c trans.o cachelab.o
	$(CC) $(CFLAGS) -O0 -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -O0 -c $<

#
# Clean the src dirctory
#
clean:
	rm -rf *.o
	rm -f *.tar
	rm -f csim
	rm -f test-trans tracegen
	rm -f trace.*
	rm -f .csim_results .marker
