#include <stdio.h>
#include "__test.h"

// some variables
static unsigned int __TEST_ran    = 0;
static unsigned int __TEST_passed = 0;
static unsigned int __TEST_failed = 0;

int main(void) {
  fprintf(stderr, "\n======================================================\n");
  fprintf(stderr, "TEST: test suite '%s'\n", __TEST_SUITE);

  // start the test suite
  int extc = test(__TEST_SUITE);

  fprintf(stderr, "TEST: test suite completed\n");
  fprintf(stderr, "TEST:       ran: %d test(s)\n", __TEST_ran);
  fprintf(stderr, "TEST:    passed: %d test(s)\n", __TEST_passed);
  fprintf(stderr, "TEST:    failed: %d test(s)\n", __TEST_failed);
  fprintf(stderr, "======================================================\n\n");
  return (0 != __TEST_failed && 0 == extc) ? 1 : extc;
}

int __TEST_RUN(int (*test_func)(void), const char *test_name) {
  fprintf(stderr, "TEST: running '%s' ...\n", test_name);
  int result = test_func();

  // report and increment counters
  if (result) {
    fprintf(stderr, "TEST: '%s' passed\n", test_name);
    __TEST_passed++;
  }
  else {
    fprintf(stderr, "TEST: '%s' failed\n", test_name);
    __TEST_failed++;
  }
  __TEST_ran++;

  return result ? 1 : 0;
}

int TEST_ASSERT(int cond, const char *fail_msg, ...) {
  if (!cond) {
    va_list args;
    va_start(args, fail_msg);
    fprintf(stderr, "TEST: [assertion failed] ");
    vfprintf(stderr, fail_msg, args);
    fputc('\n', stderr);
    va_end(args);
  }
  return cond;
}
