#ifndef APP_H
#define APP_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "period_stats.h"

/* task ids for the heartbeat table */
enum {
    HB_SAMPLING = 0,
    HB_LOGGER,
    HB_COMMAND,
    HB_COUNT
};

/* Counters shared between tasks.
 * Each counter is written by only ONE task (see comment next to it), and
 * a 32-bit aligned write is atomic on Cortex-M4, so readers never see a
 * half written value. Readers that need several values to match each
 * other (stats printing) copy them inside a critical section. */
typedef struct {
    uint32_t samples;        /* sampling: samples produced */
    uint32_t dropped;        /* sampling: queue full, sample lost */
    uint32_t late;           /* sampling: xTaskDelayUntil returned pdFALSE */
    uint32_t imu_errors;     /* sampling: I2C read failed */
    uint32_t queue_max;      /* sampling: highest queue fill seen */
    period_stats_t period;   /* sampling: measured period between wakeups */

    uint32_t written;        /* logger: records written to the sink */
    uint32_t sink_errors;    /* logger */
    uint32_t write_max_us;   /* logger: slowest sink write */
    uint32_t batches;        /* logger */
} app_stats_t;

extern app_stats_t g_stats;
extern QueueHandle_t g_sample_queue;
extern volatile uint32_t g_heartbeat[HB_COUNT];   /* tick count of last sign of life */
extern volatile int g_logging_enabled;
extern volatile int g_logger_fault;               /* 0 none, 1 hang, 2 spin */
extern volatile int g_shutdown_request;
extern volatile int g_logger_done;
extern TaskHandle_t g_task_handles[4];

void app_start(void);   /* creates queue + tasks, then starts the scheduler */

void sampling_task(void *arg);
void logger_task(void *arg);
void command_task(void *arg);
void health_task(void *arg);

void app_print_stats(const char *tag);

static inline void heartbeat(int id)
{
    g_heartbeat[id] = (uint32_t)xTaskGetTickCount();
}

#endif
