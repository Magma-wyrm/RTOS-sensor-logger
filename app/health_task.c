/* Health task: lowest priority task, runs every 500 ms.
 *  - checks that every other task gave a heartbeat recently
 *  - kicks the hardware watchdog ONLY if all of them are alive
 *  - prints stats every 10 s
 * So there are two ways a fault ends in a reset:
 *  1. a task blocks forever -> its heartbeat gets old -> health stops kicking
 *  2. a task hogs the CPU -> health itself cannot run -> nobody kicks */
#include "app.h"
#include "app_config.h"
#include "bsp.h"

static const char *hb_names[HB_COUNT] = { "sampling", "logger", "command" };

void health_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t ms_since_print = 0;
    int reported[HB_COUNT] = { 0 };

    /* give the other tasks one heartbeat period before the first check */
    for (int i = 0; i < HB_COUNT; i++) {
        g_heartbeat[i] = (uint32_t)xTaskGetTickCount();
    }
    bsp_watchdog_start(WATCHDOG_TIMEOUT_MS);

    for (;;) {
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HEALTH_PERIOD_MS));

        uint32_t now = (uint32_t)xTaskGetTickCount();
        int all_alive = 1;

        for (int i = 0; i < HB_COUNT; i++) {
            uint32_t age_ms = (now - g_heartbeat[i]) * portTICK_PERIOD_MS;
            if (age_ms > HEARTBEAT_TIMEOUT_MS) {
                all_alive = 0;
                if (!reported[i]) {
                    bsp_log("HEALTH: %s heartbeat lost (%lu ms old), stop kicking watchdog at t=%lu ms\r\n",
                            hb_names[i], (unsigned long)age_ms,
                            (unsigned long)(now * portTICK_PERIOD_MS));
                    reported[i] = 1;
                }
            }
        }

        if (all_alive) {
            bsp_watchdog_kick();
        }
        bsp_led_toggle();

        ms_since_print += HEALTH_PERIOD_MS;
        if (ms_since_print >= STATS_PRINT_EVERY_MS) {
            ms_since_print = 0;
            app_print_stats("STATS");
        }

        /* sim only: end of a timed test run */
        if (bsp_run_finished() && !g_shutdown_request) {
            g_logging_enabled = 0;     /* stop producing */
            g_shutdown_request = 1;    /* logger drains the queue and flushes */
        }
        if (g_shutdown_request && g_logger_done) {
            app_print_stats("SUMMARY");
            bsp_shutdown(0);
        }
    }
}
