CC = gcc
CARGS = -std=c11 -Wall -pedantic -g -D__DEBUG __test.c -D__TEST_SUITE='"$@"'

# put the name of the tests here
TEST_SUITES = mem_test

all: $(TEST_SUITES)
.PHONY: $(TEST_SUITES)

# define the test rules here

mem_test: mem_test.c ../src/mem.c
	$(CC) $(CARGS) -o $@ $^
	./$@
