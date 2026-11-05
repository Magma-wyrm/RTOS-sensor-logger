#ifndef PERIOD_STATS_H
#define PERIOD_STATS_H

#include <stdint.h>

/* Keeps min / max / mean of the measured sampling period and a small
 * histogram of |period - expected|. No floats and no big arrays, so the
 * sampling task can update it every cycle in a few microseconds. */

#define PSTATS_BUCKETS 6
/* bucket upper limits in us: <50, <100, <250, <500, <1000, >=1000 */

typedef struct {
    uint32_t expected_us;
    uint32_t count;
    uint32_t min_us;
    uint32_t max_us;
    uint64_t sum_us;
    uint32_t max_abs_err_us;
    uint32_t hist[PSTATS_BUCKETS];
} period_stats_t;

void pstats_init(period_stats_t *ps, uint32_t expected_us);
void pstats_add(period_stats_t *ps, uint32_t period_us);
uint32_t pstats_mean_us(const period_stats_t *ps);
uint32_t pstats_bucket_limit(int bucket);  /* 0 for the last (open) bucket */

#endif
