/* Logger task: takes samples out of the queue, encodes them into 24 byte
 * records with CRC and writes them to the sink in batches of 32.
 *
 * Batching is because a real SD card write has a large fixed cost, so
 * writing 768 bytes at once is much cheaper than 32 small writes.
 * This task is the ONLY one that touches the sink, so the sink needs no mutex.
 */
#include "app.h"
#include "app_config.h"
#include "bsp.h"
#include "record.h"

static uint8_t batch_buf[LOG_BATCH_RECORDS * RECORD_SIZE];

static void flush_batch(uint32_t count)
{
    if (count == 0) {
        return;
    }
    uint32_t t0 = bsp_time_us();
    int rc = bsp_sink_write(batch_buf, count * RECORD_SIZE);
    uint32_t dt = bsp_time_us() - t0;

    if (rc != 0) {
        g_stats.sink_errors++;
    } else {
        g_stats.written += count;
    }
    if (dt > g_stats.write_max_us) {
        g_stats.write_max_us = dt;
    }
    g_stats.batches++;
}

void logger_task(void *arg)
{
    (void)arg;
    uint32_t count = 0;
    TickType_t last_flush = xTaskGetTickCount();
    sample_t s;

    for (;;) {
        heartbeat(HB_LOGGER);

        /* ---- fault injection, used to test the health monitor + watchdog ---- */
        if (g_logger_fault == 1) {
            /* simulates a driver that waits for an interrupt that never comes */
            vTaskSuspend(NULL);
        } else if (g_logger_fault == 2) {
            /* simulates an endless loop bug. This task is above health in
             * priority, so health is starved and only the watchdog can save us */
            for (;;) {
            }
        }

        /* wait max 100 ms so the heartbeat keeps going even with no data */
        if (xQueueReceive(g_sample_queue, &s, pdMS_TO_TICKS(100)) == pdTRUE) {
            record_encode(&s, &batch_buf[count * RECORD_SIZE]);
            count++;
            if (count == LOG_BATCH_RECORDS) {
                flush_batch(count);
                count = 0;
                last_flush = xTaskGetTickCount();
            }
        }

        /* write a partial batch if data is waiting too long (or we are stopping) */
        if (count > 0 &&
            (xTaskGetTickCount() - last_flush) >= pdMS_TO_TICKS(LOGGER_FLUSH_TIMEOUT_MS)) {
            flush_batch(count);
            count = 0;
            last_flush = xTaskGetTickCount();
        }

        if (g_shutdown_request && uxQueueMessagesWaiting(g_sample_queue) == 0) {
            flush_batch(count);
            count = 0;
            g_logger_done = 1;
            vTaskSuspend(NULL);
        }
    }
}
