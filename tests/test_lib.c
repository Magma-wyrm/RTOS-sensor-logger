#include <string.h>
#include "test.h"
#include "crc16.h"
#include "record.h"
#include "ringbuf.h"
#include "cli.h"
#include "period_stats.h"

/* ---------------- crc16 ---------------- */

static void crc_check_value(void)
{
    const char *s = "123456789";
    /* standard check value for CRC-16/CCITT-FALSE */
    CHECK_EQ(crc16_ccitt((const uint8_t *)s, 9), 0x29B1);
}

static void crc_empty_is_init(void)
{
    CHECK_EQ(crc16_ccitt(NULL, 0), 0xFFFF);
}

static void crc_detects_single_bit_flip(void)
{
    uint8_t a[16];
    for (int i = 0; i < 16; i++) a[i] = (uint8_t)(i * 7);
    uint16_t c1 = crc16_ccitt(a, 16);
    a[5] ^= 0x10;
    CHECK(crc16_ccitt(a, 16) != c1);
}

void run_crc16_tests(void)
{
    RUN_TEST(crc_check_value);
    RUN_TEST(crc_empty_is_init);
    RUN_TEST(crc_detects_single_bit_flip);
}

/* ---------------- record ---------------- */

static sample_t make_sample(void)
{
    sample_t s;
    memset(&s, 0, sizeof(s));
    s.seq = 0x12345678;
    s.t_us = 4000000000u;
    s.ax = -1; s.ay = 32767; s.az = -32768;
    s.gx = 100; s.gy = -100; s.gz = 0;
    s.flags = SAMPLE_FLAG_LATE;
    return s;
}

static void record_roundtrip(void)
{
    sample_t in = make_sample(), out;
    uint8_t buf[RECORD_SIZE];

    record_encode(&in, buf);
    CHECK_EQ(record_decode(buf, &out), 0);
    CHECK_EQ(out.seq, in.seq);
    CHECK_EQ(out.t_us, in.t_us);
    CHECK_EQ(out.ax, -1);
    CHECK_EQ(out.ay, 32767);
    CHECK_EQ(out.az, -32768);
    CHECK_EQ(out.gx, 100);
    CHECK_EQ(out.gy, -100);
    CHECK_EQ(out.flags, SAMPLE_FLAG_LATE);
}

static void record_layout_is_little_endian(void)
{
    sample_t in = make_sample();
    uint8_t buf[RECORD_SIZE];

    record_encode(&in, buf);
    CHECK_EQ(buf[0], RECORD_SYNC);
    CHECK_EQ(buf[2], 0x78);   /* seq low byte first */
    CHECK_EQ(buf[5], 0x12);
    CHECK_EQ(buf[10], 0xFF);  /* ax = -1 */
    CHECK_EQ(buf[11], 0xFF);
}

static void record_bad_crc_rejected(void)
{
    sample_t in = make_sample(), out;
    uint8_t buf[RECORD_SIZE];

    record_encode(&in, buf);
    buf[12] ^= 0x01;
    CHECK_EQ(record_decode(buf, &out), -2);
}

static void record_bad_sync_rejected(void)
{
    sample_t in = make_sample(), out;
    uint8_t buf[RECORD_SIZE];

    record_encode(&in, buf);
    buf[0] = 0x00;
    CHECK_EQ(record_decode(buf, &out), -1);
}

void run_record_tests(void)
{
    RUN_TEST(record_roundtrip);
    RUN_TEST(record_layout_is_little_endian);
    RUN_TEST(record_bad_crc_rejected);
    RUN_TEST(record_bad_sync_rejected);
}

/* ---------------- ringbuf ---------------- */

static void ringbuf_empty_get_fails(void)
{
    ringbuf_t rb;
    uint8_t c;
    ringbuf_init(&rb);
    CHECK_EQ(ringbuf_get(&rb, &c), -1);
    CHECK_EQ(ringbuf_count(&rb), 0);
}

static void ringbuf_keeps_order(void)
{
    ringbuf_t rb;
    uint8_t c;
    ringbuf_init(&rb);
    ringbuf_put(&rb, 'a');
    ringbuf_put(&rb, 'b');
    ringbuf_put(&rb, 'c');
    CHECK_EQ(ringbuf_count(&rb), 3);
    ringbuf_get(&rb, &c); CHECK_EQ(c, 'a');
    ringbuf_get(&rb, &c); CHECK_EQ(c, 'b');
    ringbuf_get(&rb, &c); CHECK_EQ(c, 'c');
    CHECK_EQ(ringbuf_get(&rb, &c), -1);
}

static void ringbuf_full_drops_and_counts(void)
{
    ringbuf_t rb;
    int ok = 0;
    ringbuf_init(&rb);
    for (int i = 0; i < RINGBUF_SIZE + 10; i++) {
        if (ringbuf_put(&rb, (uint8_t)i) == 0) ok++;
    }
    CHECK_EQ(ok, RINGBUF_SIZE - 1);   /* one slot is always kept empty */
    CHECK_EQ(rb.overflows, 11);
}

static void ringbuf_wraps_around(void)
{
    ringbuf_t rb;
    uint8_t c;
    int errors = 0;
    ringbuf_init(&rb);
    for (int i = 0; i < 1000; i++) {
        ringbuf_put(&rb, (uint8_t)i);
        ringbuf_put(&rb, (uint8_t)(i + 1));
        if (ringbuf_get(&rb, &c) != 0 || c != (uint8_t)i) errors++;
        if (ringbuf_get(&rb, &c) != 0 || c != (uint8_t)(i + 1)) errors++;
    }
    CHECK_EQ(errors, 0);
}

void run_ringbuf_tests(void)
{
    RUN_TEST(ringbuf_empty_get_fails);
    RUN_TEST(ringbuf_keeps_order);
    RUN_TEST(ringbuf_full_drops_and_counts);
    RUN_TEST(ringbuf_wraps_around);
}

/* ---------------- cli ---------------- */

static cli_cmd_t parse(const char *s)
{
    cli_cmd_t c;
    cli_parse(s, &c);
    return c;
}

static void cli_simple_commands(void)
{
    CHECK_EQ(parse("help").id, CMD_HELP);
    CHECK_EQ(parse("stats").id, CMD_STATS);
    CHECK_EQ(parse("start").id, CMD_START);
    CHECK_EQ(parse("stop").id, CMD_STOP);
    CHECK_EQ(parse("hang").id, CMD_HANG);
    CHECK_EQ(parse("spin").id, CMD_SPIN);
    CHECK_EQ(parse("  stats  ").id, CMD_STATS);
    CHECK_EQ(parse("stats\r").id, CMD_STATS);   /* windows terminals send \r\n */
}

static void cli_empty_and_unknown(void)
{
    CHECK_EQ(parse("").id, CMD_NONE);
    CHECK_EQ(parse("   ").id, CMD_NONE);
    CHECK_EQ(parse("reboot").id, CMD_UNKNOWN);
    CHECK_EQ(parse("statsx").id, CMD_UNKNOWN);   /* prefix must not match */
    CHECK_EQ(parse("stats now").id, CMD_BAD_ARG);
}

static void cli_stall_argument(void)
{
    cli_cmd_t c = parse("stall 1500");
    CHECK_EQ(c.id, CMD_STALL);
    CHECK_EQ(c.arg, 1500);
    CHECK_EQ(parse("stall 10000").id, CMD_STALL);
    CHECK_EQ(parse("stall 10001").id, CMD_BAD_ARG);
    CHECK_EQ(parse("stall 0").id, CMD_BAD_ARG);
    CHECK_EQ(parse("stall").id, CMD_BAD_ARG);
    CHECK_EQ(parse("stall abc").id, CMD_BAD_ARG);
    CHECK_EQ(parse("stall 12x").id, CMD_BAD_ARG);
    CHECK_EQ(parse("stall -5").id, CMD_BAD_ARG);
    CHECK_EQ(parse("stall 99999999999").id, CMD_BAD_ARG);   /* overflow */
}

void run_cli_tests(void)
{
    RUN_TEST(cli_simple_commands);
    RUN_TEST(cli_empty_and_unknown);
    RUN_TEST(cli_stall_argument);
}

/* ---------------- period_stats ---------------- */

static void pstats_min_max_mean(void)
{
    period_stats_t ps;
    pstats_init(&ps, 10000);
    pstats_add(&ps, 9900);
    pstats_add(&ps, 10000);
    pstats_add(&ps, 10200);
    CHECK_EQ(ps.count, 3);
    CHECK_EQ(ps.min_us, 9900);
    CHECK_EQ(ps.max_us, 10200);
    CHECK_EQ(pstats_mean_us(&ps), 10033);
    CHECK_EQ(ps.max_abs_err_us, 200);
}

static void pstats_histogram_edges(void)
{
    period_stats_t ps;
    pstats_init(&ps, 10000);
    pstats_add(&ps, 10049);   /* err 49  -> bucket 0 */
    pstats_add(&ps, 10050);   /* err 50  -> bucket 1 */
    pstats_add(&ps, 9750);    /* err 250 -> bucket 3 (early is also error) */
    pstats_add(&ps, 10999);   /* err 999 -> bucket 4 */
    pstats_add(&ps, 11000);   /* err 1000 -> bucket 5 */
    CHECK_EQ(ps.hist[0], 1);
    CHECK_EQ(ps.hist[1], 1);
    CHECK_EQ(ps.hist[2], 0);
    CHECK_EQ(ps.hist[3], 1);
    CHECK_EQ(ps.hist[4], 1);
    CHECK_EQ(ps.hist[5], 1);
}

static void pstats_empty_mean_is_zero(void)
{
    period_stats_t ps;
    pstats_init(&ps, 10000);
    CHECK_EQ(pstats_mean_us(&ps), 0);
}

void run_period_stats_tests(void)
{
    RUN_TEST(pstats_min_max_mean);
    RUN_TEST(pstats_histogram_edges);
    RUN_TEST(pstats_empty_mean_is_zero);
}
