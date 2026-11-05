#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ---------- task priorities (bigger number = more important) ----------
 * sampling has the hard deadline (10 ms period) so it is the highest.
 * command is above logger so the console still answers when the logger
 * is busy writing. health is the lowest on purpose: if any task hogs the
 * CPU, health stops running, stops kicking the watchdog and the board resets. */
#define PRIO_SAMPLING   5
#define PRIO_COMMAND    4
#define PRIO_LOGGER     3
#define PRIO_HEALTH     2

/* stack sizes in words (FreeRTOS counts words, not bytes) */
#define STACK_SAMPLING  256
#define STACK_LOGGER    384
#define STACK_COMMAND   384
#define STACK_HEALTH    384

/* ---------- timing ---------- */
#define SAMPLE_PERIOD_MS        10      /* 100 Hz */
#define HEALTH_PERIOD_MS        500
#define STATS_PRINT_EVERY_MS    10000
#define HEARTBEAT_TIMEOUT_MS    2000    /* a task is "dead" if silent this long */
#define WATCHDOG_TIMEOUT_MS     1000    /* watchdog resets if not kicked this long */
#define LOGGER_FLUSH_TIMEOUT_MS 500     /* write a partial batch after this long */

/* ---------- queue and batching ----------
 * Queue length 100 = 1 second of samples at 100 Hz. So a storage stall
 * shorter than ~1 s should lose nothing; a longer stall drops samples,
 * and those drops are counted (never silent). See docs/RESULTS.md. */
#define SAMPLE_QUEUE_LEN    100
#define LOG_BATCH_RECORDS   32

#endif
