#ifndef TEST_H
#define TEST_H

/* Very small test helper. I did not use a framework (Unity etc.) because
 * these tests only need "check and count", and this runs anywhere. */
#include <stdio.h>

extern int g_checks;
extern int g_failures;

#define CHECK(cond) do { \
        g_checks++; \
        if (!(cond)) { \
            g_failures++; \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define CHECK_EQ(a, b) do { \
        long long va_ = (long long)(a); \
        long long vb_ = (long long)(b); \
        g_checks++; \
        if (va_ != vb_) { \
            g_failures++; \
            printf("    FAIL %s:%d: %s == %s (%lld != %lld)\n", \
                   __FILE__, __LINE__, #a, #b, va_, vb_); \
        } \
    } while (0)

#define RUN_TEST(fn) do { printf("  %s\n", #fn); fn(); g_tests++; } while (0)

extern int g_tests;

void run_crc16_tests(void);
void run_record_tests(void);
void run_ringbuf_tests(void);
void run_cli_tests(void);
void run_period_stats_tests(void);
void run_mpu6500_tests(void);

#endif
