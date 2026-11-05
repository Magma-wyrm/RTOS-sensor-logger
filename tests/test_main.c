#include "test.h"

int g_checks;
int g_failures;
int g_tests;

int main(void)
{
    printf("crc16\n");
    run_crc16_tests();
    printf("record\n");
    run_record_tests();
    printf("ringbuf\n");
    run_ringbuf_tests();
    printf("cli\n");
    run_cli_tests();
    printf("period_stats\n");
    run_period_stats_tests();
    printf("mpu6500\n");
    run_mpu6500_tests();

    printf("\n%d tests, %d checks, %d failures\n", g_tests, g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
