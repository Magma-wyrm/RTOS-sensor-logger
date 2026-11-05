#include "app.h"
#include "app_config.h"
#include "bsp.h"
#include "record.h"

app_stats_t g_stats;
QueueHandle_t g_sample_queue;
volatile uint32_t g_heartbeat[HB_COUNT];
volatile int g_logging_enabled = 1;
volatile int g_logger_fault = 0;
volatile int g_shutdown_request = 0;
volatile int g_logger_done = 0;
TaskHandle_t g_task_handles[4];

static const char *task_names[4] = { "sampling", "logger", "command", "health" };

static void create_task(TaskFunction_t fn, int idx, uint16_t stack_words, UBaseType_t prio)
{
    BaseType_t ok = xTaskCreate(fn, task_names[idx], stack_words, NULL, prio, &g_task_handles[idx]);
    if (ok != pdPASS) {
        bsp_log("ERROR: could not create task %s\r\n", task_names[idx]);
        for (;;) {
        }
    }
}

void app_start(void)
{
    bsp_log("\r\nrtos-sensor-logger on %s\r\n", bsp_name());
    if (bsp_last_reset_was_watchdog()) {
        bsp_log("NOTE: last reset was caused by the watchdog\r\n");
    }
    bsp_log("tick %u Hz, sample period %u ms, queue %u, batch %u\r\n",
            (unsigned)configTICK_RATE_HZ, (unsigned)SAMPLE_PERIOD_MS,
            (unsigned)SAMPLE_QUEUE_LEN, (unsigned)LOG_BATCH_RECORDS);

    pstats_init(&g_stats.period, SAMPLE_PERIOD_MS * 1000u);

    g_sample_queue = xQueueCreate(SAMPLE_QUEUE_LEN, sizeof(sample_t));
    if (g_sample_queue == NULL) {
        bsp_log("ERROR: queue create failed\r\n");
        for (;;) {
        }
    }

    create_task(sampling_task, 0, STACK_SAMPLING, PRIO_SAMPLING);
    create_task(logger_task, 1, STACK_LOGGER, PRIO_LOGGER);
    create_task(command_task, 2, STACK_COMMAND, PRIO_COMMAND);
    create_task(health_task, 3, STACK_HEALTH, PRIO_HEALTH);

    vTaskStartScheduler();

    /* only gets here if there was not enough heap for the idle task */
    bsp_log("ERROR: scheduler did not start\r\n");
    for (;;) {
    }
}

void app_print_stats(const char *tag)
{
    app_stats_t s;
    UBaseType_t min_stack = 0xFFFF;
    int min_stack_task = 0;

    /* copy everything at once so the numbers belong together */
    taskENTER_CRITICAL();
    s = g_stats;
    taskEXIT_CRITICAL();

    for (int i = 0; i < 4; i++) {
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(g_task_handles[i]);
        if (hwm < min_stack) {
            min_stack = hwm;
            min_stack_task = i;
        }
    }

    bsp_log("%s t=%lus samples=%lu dropped=%lu late=%lu imu_err=%lu qmax=%lu "
            "written=%lu period_us(min/avg/max)=%lu/%lu/%lu max_err_us=%lu "
            "write_max_us=%lu stack_free_min=%u(%s) heap_free=%u\r\n",
            tag,
            (unsigned long)(xTaskGetTickCount() / configTICK_RATE_HZ),
            (unsigned long)s.samples, (unsigned long)s.dropped,
            (unsigned long)s.late, (unsigned long)s.imu_errors,
            (unsigned long)s.queue_max, (unsigned long)s.written,
            (unsigned long)(s.period.count ? s.period.min_us : 0),
            (unsigned long)pstats_mean_us(&s.period),
            (unsigned long)s.period.max_us,
            (unsigned long)s.period.max_abs_err_us,
            (unsigned long)s.write_max_us,
            (unsigned)min_stack, task_names[min_stack_task],
            (unsigned)xPortGetFreeHeapSize());
}

/* ---- FreeRTOS hooks ---- */

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    bsp_log("FATAL: stack overflow in task %s\r\n", name);
    taskDISABLE_INTERRUPTS();
    for (;;) {
        /* wait here, the watchdog will reset us */
    }
}

void vApplicationMallocFailedHook(void)
{
    bsp_log("FATAL: malloc failed\r\n");
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}
