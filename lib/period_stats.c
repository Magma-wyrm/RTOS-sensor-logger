#include "period_stats.h"

static const uint32_t bucket_limits[PSTATS_BUCKETS - 1] = { 50, 100, 250, 500, 1000 };

void pstats_init(period_stats_t *ps, uint32_t expected_us)
{
    ps->expected_us = expected_us;
    ps->count = 0;
    ps->min_us = 0xFFFFFFFFu;
    ps->max_us = 0;
    ps->sum_us = 0;
    ps->max_abs_err_us = 0;
    for (int i = 0; i < PSTATS_BUCKETS; i++) {
        ps->hist[i] = 0;
    }
}

void pstats_add(period_stats_t *ps, uint32_t period_us)
{
    uint32_t err;
    int b;

    if (period_us >= ps->expected_us) {
        err = period_us - ps->expected_us;
    } else {
        err = ps->expected_us - period_us;
    }

    ps->count++;
    ps->sum_us += period_us;
    if (period_us < ps->min_us) {
        ps->min_us = period_us;
    }
    if (period_us > ps->max_us) {
        ps->max_us = period_us;
    }
    if (err > ps->max_abs_err_us) {
        ps->max_abs_err_us = err;
    }

    for (b = 0; b < PSTATS_BUCKETS - 1; b++) {
        if (err < bucket_limits[b]) {
            break;
        }
    }
    ps->hist[b]++;
}

uint32_t pstats_mean_us(const period_stats_t *ps)
{
    if (ps->count == 0) {
        return 0;
    }
    return (uint32_t)(ps->sum_us / ps->count);
}

uint32_t pstats_bucket_limit(int bucket)
{
    if (bucket < 0 || bucket >= PSTATS_BUCKETS - 1) {
        return 0;
    }
    return bucket_limits[bucket];
}
