CC = gcc
CARGS = -std=c11 -Wall -pedantic -g -D__DEBUG __test.c -D__TEST_SUITE='"$@"'

# put the name of the tests here
TEST_SUITES = test_mem test_load

all: $(TEST_SUITES)
.PHONY: clean $(TEST_SUITES)

clean:
	rm -rf $(TEST_SUITES)

# define the test rules here

test_mem: test_mem.c ../src/mem.c
	$(CC) $(CARGS) -o $@ $^
	./$@

test_load: test_load.c ../src/exec.c ../src/mem.c ../src/vyt.c
	$(CC) $(CARGS) -o $@ $^
	./$@
