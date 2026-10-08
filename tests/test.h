/* Minimal test harness: no dependencies, works on every target. */
#ifndef TNFS_TEST_H
#define TNFS_TEST_H

#include <stdio.h>
#include <string.h>

static int test_failures;
static int test_checks;

#define CHECK(cond) do { \
    test_checks++; \
    if (!(cond)) { \
        test_failures++; \
        fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_EQ_INT(actual, expected) do { \
    long long a_ = (long long)(actual), e_ = (long long)(expected); \
    test_checks++; \
    if (a_ != e_) { \
        test_failures++; \
        fprintf(stderr, "%s:%d: %s == %lld, expected %lld\n", __FILE__, __LINE__, #actual, a_, e_); \
    } \
} while (0)

#define CHECK_MEM(actual, expected, len) CHECK(memcmp((actual), (expected), (len)) == 0)

#define TEST_REPORT() ( \
    fprintf(stderr, "%d checks, %d failures\n", test_checks, test_failures), \
    test_failures == 0 ? 0 : 1)

#endif
