/* Sampling task: reads the IMU every 10 ms and puts the sample in the queue.
 *
 * Rules for this task (it has the hard deadline):
 *  - uses xTaskDelayUntil, not vTaskDelay, so the period does not drift
 *  - never blocks on the queue (timeout 0). If the queue is full the
 *    sample is dropped and counted, the task does not wait for the logger
 *  - no printing from here, printing is slow and takes a lock
 */
#include "app.h"
#include "app_config.h"
#include "bsp.h"
#include "mpu6500.h"
#include "record.h"

static mpu6500_t imu = {
    .addr = MPU6500_ADDR,
    .write = bsp_i2c_write,
    .read = bsp_i2c_read,
};

void sampling_task(void *arg)
{
    (void)arg;
    uint32_t seq = 0;
    uint32_t prev_us = 0;
    int have_prev = 0;
    int imu_ok;

    int rc = mpu6500_init(&imu);
    imu_ok = (rc == MPU6500_OK);
    /* logging from here is ok only once, before the periodic loop starts */
    bsp_log("sampling: mpu6500 init %s (who_am_i=0x%02X)\r\n",
            imu_ok ? "ok" : "FAILED", (unsigned)imu.who_am_i);

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        BaseType_t on_time = xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
        uint32_t now_us = bsp_time_us();   /* first thing after waking up */

        heartbeat(HB_SAMPLING);

        if (have_prev) {
            pstats_add(&g_stats.period, now_us - prev_us);   /* unsigned math handles wrap */
        }
        prev_us = now_us;
        have_prev = 1;

        sample_t s;
        s.seq = seq;
        s.t_us = now_us;
        s.flags = 0;
        seq++;   /* always counts up, so a dropped sample leaves a gap in the log */

        if (on_time == pdFALSE) {
            s.flags |= SAMPLE_FLAG_LATE;
            g_stats.late++;
        }

        mpu6500_raw_t raw;
        if (imu_ok && mpu6500_read_raw(&imu, &raw) == MPU6500_OK) {
            s.ax = raw.ax; s.ay = raw.ay; s.az = raw.az;
            s.gx = raw.gx; s.gy = raw.gy; s.gz = raw.gz;
        } else {
            s.ax = s.ay = s.az = 0;
            s.gx = s.gy = s.gz = 0;
            s.flags |= SAMPLE_FLAG_IMU_ERROR;
            g_stats.imu_errors++;
        }

        if (!g_logging_enabled) {
            continue;   /* "stop" command: keep timing, do not log */
        }

        g_stats.samples++;
        if (xQueueSend(g_sample_queue, &s, 0) != pdTRUE) {
            g_stats.dropped++;
        }

        uint32_t depth = (uint32_t)uxQueueMessagesWaiting(g_sample_queue);
        if (depth > g_stats.queue_max) {
            g_stats.queue_max = depth;
        }
    }
}
