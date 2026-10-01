// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef BP32_TEST_CHECK_H
#define BP32_TEST_CHECK_H

/**
 * @file test_check.h
 * @brief Minimal unit testing framework for Bluepad32.
 *
 * This testing framework provides a set of assertion and expectation macros
 * similar to Google Test. It is designed to be completely immune to the `NDEBUG`
 * macro, ensuring that tests always evaluate their arguments and perform checks
 * regardless of whether the build is in debug or release mode. Furthermore,
 * where applicable, macros safely evaluate their arguments exactly once by
 * capturing them in local variables, preventing side-effect related bugs.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_test_failures = 0;

/**
 * @brief Defines a test function.
 * @param name The name of the test.
 */
#define TEST(name) static void test_##name(void)

/**
 * @brief Runs a test function and logs its result.
 * @param name The name of the test to run.
 */
#define RUN_TEST(name)                        \
    do {                                      \
        int prev_failures = g_test_failures;  \
        g_tests_run++;                        \
        test_##name();                        \
        if (g_test_failures == prev_failures) \
            printf("  [PASS] %s\n", #name);   \
        else                                  \
            printf("  [FAIL] %s\n", #name);   \
    } while (0)

#define ASSERT_TRUE(cond)                                                                    \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            fprintf(stderr, "ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_test_failures++;                                                               \
            abort();                                                                         \
        }                                                                                    \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(actual, expected)                                                                                 \
    do {                                                                                                            \
        long long _a = (long long)(actual);                                                                         \
        long long _e = (long long)(expected);                                                                       \
        if (_a != _e) {                                                                                             \
            fprintf(stderr, "ASSERT_EQ failed at %s:%d: %s (%lld) != %s (%lld)\n", __FILE__, __LINE__, #actual, _a, \
                    #expected, _e);                                                                                 \
            g_test_failures++;                                                                                      \
            abort();                                                                                                \
        }                                                                                                           \
    } while (0)

#define ASSERT_NE(actual, expected)                                                                                 \
    do {                                                                                                            \
        long long _a = (long long)(actual);                                                                         \
        long long _e = (long long)(expected);                                                                       \
        if (_a == _e) {                                                                                             \
            fprintf(stderr, "ASSERT_NE failed at %s:%d: %s (%lld) == %s (%lld)\n", __FILE__, __LINE__, #actual, _a, \
                    #expected, _e);                                                                                 \
            g_test_failures++;                                                                                      \
            abort();                                                                                                \
        }                                                                                                           \
    } while (0)

#define EXPECT_TRUE(cond)                                                                    \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            fprintf(stderr, "EXPECT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_test_failures++;                                                               \
        }                                                                                    \
    } while (0)

#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))

#define EXPECT_EQ(actual, expected)                                                                                 \
    do {                                                                                                            \
        long long _a = (long long)(actual);                                                                         \
        long long _e = (long long)(expected);                                                                       \
        if (_a != _e) {                                                                                             \
            fprintf(stderr, "EXPECT_EQ failed at %s:%d: %s (%lld) != %s (%lld)\n", __FILE__, __LINE__, #actual, _a, \
                    #expected, _e);                                                                                 \
            g_test_failures++;                                                                                      \
        }                                                                                                           \
    } while (0)

#define EXPECT_NE(actual, expected)                                                                                 \
    do {                                                                                                            \
        long long _a = (long long)(actual);                                                                         \
        long long _e = (long long)(expected);                                                                       \
        if (_a == _e) {                                                                                             \
            fprintf(stderr, "EXPECT_NE failed at %s:%d: %s (%lld) == %s (%lld)\n", __FILE__, __LINE__, #actual, _a, \
                    #expected, _e);                                                                                 \
            g_test_failures++;                                                                                      \
        }                                                                                                           \
    } while (0)

#define EXPECT_GT(actual, expected)                                                                                 \
    do {                                                                                                            \
        long long _a = (long long)(actual);                                                                         \
        long long _e = (long long)(expected);                                                                       \
        if (!(_a > _e)) {                                                                                           \
            fprintf(stderr, "EXPECT_GT failed at %s:%d: %s (%lld) <= %s (%lld)\n", __FILE__, __LINE__, #actual, _a, \
                    #expected, _e);                                                                                 \
            g_test_failures++;                                                                                      \
        }                                                                                                           \
    } while (0)

#define EXPECT_FLOAT_NEAR(actual, expected, eps)                                                                 \
    do {                                                                                                         \
        double _a = (double)(actual);                                                                            \
        double _e = (double)(expected);                                                                          \
        double _eps = (double)(eps);                                                                             \
        if (fabs(_a - _e) > _eps) {                                                                              \
            fprintf(stderr, "EXPECT_FLOAT_NEAR failed at %s:%d: |%s (%f) - %s (%f)| > %f\n", __FILE__, __LINE__, \
                    #actual, _a, #expected, _e, _eps);                                                           \
            g_test_failures++;                                                                                   \
        }                                                                                                        \
    } while (0)

/**
 * @brief Evaluates a condition and logs a failure if false.
 * @param cond The condition to evaluate.
 */
#define TEST_CHECK(cond) EXPECT_TRUE(cond)

/**
 * @brief Checks if two integer values are equal.
 * Evaluates its arguments exactly once to avoid side-effect bugs.
 * @param actual The actual value produced by the test.
 * @param expected The expected value.
 */
#define TEST_CHECK_EQ_INT(actual, expected) EXPECT_EQ(actual, expected)

#define TEST_CHECK_EQ_PTR(actual, expected)                                                                         \
    do {                                                                                                            \
        const void* _a = (const void*)(actual);                                                                     \
        const void* _e = (const void*)(expected);                                                                   \
        if (_a != _e) {                                                                                             \
            fprintf(stderr, "TEST_CHECK_EQ_PTR failed at %s:%d: %s (%p) != %s (%p)\n", __FILE__, __LINE__, #actual, \
                    _a, #expected, _e);                                                                             \
            g_test_failures++;                                                                                      \
        }                                                                                                           \
    } while (0)

/**
 * @brief Checks if two floating-point values are within a given epsilon.
 * Evaluates its arguments exactly once.
 * @param actual The actual float value.
 * @param expected The expected float value.
 * @param eps The acceptable error margin (epsilon).
 */
#define TEST_CHECK_NEAR_FLOAT(actual, expected, eps) EXPECT_FLOAT_NEAR(actual, expected, eps)

/**
 * @brief Prints a summary of all executed tests.
 * @return 0 if all tests passed, 1 otherwise.
 */
static inline int test_summary(void) {
    printf("%d test(s) run, %d failure(s).\n", g_tests_run, g_test_failures);
    return g_test_failures == 0 ? 0 : 1;
}

#endif  // BP32_TEST_CHECK_H
