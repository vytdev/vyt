#ifndef _VYT_TEST_H
#define _VYT_TEST_H

/**
 * this function should be defined by the test suite
 */
int test(const char *suite_name);

/**
 * creates a new test. it should return the number 1 at the end to indicate it
 * succeded.
 */
#define TEST(name) int __TEST_CASE_##name(void)

/**
 * starts a test, returns 1 if the test passed, 0 otherwise
 */
#define TEST_RUN(name) __TEST_RUN(__TEST_CASE_##name, #name);
int __TEST_RUN(int (*test_func)(void), const char *test_name); /* internal */

/**
 * our custom assertion function, returns 1 if succed, 0 otherwise
 */
int TEST_ASSERT(int cond, const char *fail_msg);

#endif // _VYT_TEST_H
