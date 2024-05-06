CC = gcc
CARGS = -std=c11 -Wall -pedantic -g -D__DEBUG __test.c -D__TEST_SUITE='"$@"'

# put the name of the tests and their dependencies here

all: mem_test
.PHONY: mem_test

mem_test: mem_test.c ../src/mem.c
	$(CC) $(CARGS) -o $@ $^
	./$@
