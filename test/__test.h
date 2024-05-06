#ifndef _VYT_TEST_H
#define _VYT_TEST_H
#include <stdint.h>

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
int TEST_ASSERT(int cond, const char *fail_msg, ...);

/**
 * some expectation macros
 */
#define TEST_EXPECT_EQ(a, b) TEST_ASSERT((a) == (b),                          \
    "expected '%s' to be equal-to '%s' (%lld == %lld)",                       \
    #a, #b, (int64_t)(a), (int64_t)(b)                                        \
  )
#define TEST_EXPECT_NE(a, b) TEST_ASSERT((a) != (b),                          \
    "expected '%s' to be not-equal-to '%s' (%lld != %lld)",                   \
    #a, #b, (int64_t)(a), (int64_t)(b)                                        \
  )
#define TEST_EXPECT_GT(a, b) TEST_ASSERT((a) > (b),                           \
    "expected '%s' to be greater-than '%s' (%lld > %lld)",                    \
    #a, #b, (int64_t)(a), (int64_t)(b)                                        \
  )
#define TEST_EXPECT_GE(a, b) TEST_ASSERT((a) >= (b),                          \
    "expected '%s' to be greater-than-or-equal-to '%s' (%lld >= %lld)",       \
    #a, #b, (int64_t)(a), (int64_t)(b)                                        \
  )
#define TEST_EXPECT_LT(a, b) TEST_ASSERT((a) < (b),                           \
    "expected '%s' to be less-than '%s' (%lld < %lld)",                       \
    #a, #b, (int64_t)(a), (int64_t)(b)                                        \
  )
#define TEST_EXPECT_LE(a, b) TEST_ASSERT((a) <= (b),                          \
    "expected '%s' to be less-than-or-equal-to '%s' (%lld <= %lld)",          \
    #a, #b, (int64_t)(a), (int64_t)(b)                                        \
  )
#define TEST_EXPECT_TRUE(a) TEST_ASSERT((a),                                  \
    "expected '%s' to be true, but got false",                                \
    #a                                                                        \
  )
#define TEST_EXPECT_FALSE(a) TEST_ASSERT(!(a),                                \
    "expected '%s' to be false, but got true",                                \
    #a                                                                        \
  )

#endif // _VYT_TEST_H
