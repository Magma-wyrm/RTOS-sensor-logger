/* Priority inversion demo (runs on the Linux simulation).
 *
 * Three tasks share one lock:
 *   LOW  (prio 1) takes the lock and does 4 ms of work while holding it
 *   HIGH (prio 3) wakes 1 ms later and needs the same lock
 *   MED  (prio 2) wakes 2 ms later and does 20 ms of work, no lock at all
 *
 * With a binary semaphore MED preempts LOW while LOW holds the lock, so
 * HIGH waits for MED too (priority inversion).
 * With a mutex FreeRTOS raises LOW to HIGH's priority while it holds the
 * lock (priority inheritance), so MED cannot get in between.
 *
 * Note: I first gave the tasks a start offset by setting the initial
 * "last wake" value of xTaskDelayUntil to 1 and 2. That does not work:
 * a last-wake time in the future is treated as a tick overflow and the
 * task does not block at all. Now all three wake together and the
 * offsets come from a short vTaskDelay.
 *
 * "work" is counted in ticks where the task was REALLY running (tick hook),
 * otherwise time spent preempted would count as work done.
 *
 * usage: inversion_demo semaphore|mutex [rounds]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define PRIO_LOW   1
#define PRIO_MED   2
#define PRIO_HIGH  3

#define ROUND_PERIOD_MS  100
#define LOW_WORK_TICKS   4
#define MED_WORK_TICKS   20

static SemaphoreHandle_t lock;
static TaskHandle_t low_handle, med_handle, high_handle;
static volatile uint32_t run_ticks[3];   /* 0 low, 1 med, 2 high */
static int rounds = 20;
static int use_mutex;

static volatile uint32_t wait_min = 0xFFFFFFFF, wait_max, wait_sum, wait_count;
static volatile UBaseType_t low_max_prio_while_holding;

void vApplicationTickHook(void)
{
    TaskHandle_t cur = xTaskGetCurrentTaskHandle();
    if (cur == low_handle) {
        run_ticks[0]++;
    } else if (cur == med_handle) {
        run_ticks[1]++;
    } else if (cur == high_handle) {
        run_ticks[2]++;
    }
}

static void do_work(int id, uint32_t ticks)
{
    uint32_t start = run_ticks[id];
    while (run_ticks[id] - start < ticks) {
        if (id == 0) {
            UBaseType_t p = uxTaskPriorityGet(NULL);
            if (p > low_max_prio_while_holding) {
                low_max_prio_while_holding = p;
            }
        }
    }
}

static void low_task(void *arg)
{
    (void)arg;
    TickType_t last = 0;
    for (;;) {
        xTaskDelayUntil(&last, pdMS_TO_TICKS(ROUND_PERIOD_MS));
        xSemaphoreTake(lock, portMAX_DELAY);
        do_work(0, LOW_WORK_TICKS);
        xSemaphoreGive(lock);
    }
}

static void med_task(void *arg)
{
    (void)arg;
    TickType_t last = 0;
    for (;;) {
        xTaskDelayUntil(&last, pdMS_TO_TICKS(ROUND_PERIOD_MS));
        vTaskDelay(2);   /* start 2 ms after low */
        do_work(1, MED_WORK_TICKS);
    }
}

static void high_task(void *arg)
{
    (void)arg;
    TickType_t last = 0;
    for (int r = 1; r <= rounds; r++) {
        xTaskDelayUntil(&last, pdMS_TO_TICKS(ROUND_PERIOD_MS));
        vTaskDelay(1);   /* start 1 ms after low */
        TickType_t t0 = xTaskGetTickCount();
        xSemaphoreTake(lock, portMAX_DELAY);
        uint32_t waited = (uint32_t)(xTaskGetTickCount() - t0);
        xSemaphoreGive(lock);

        if (waited < wait_min) wait_min = waited;
        if (waited > wait_max) wait_max = waited;
        wait_sum += waited;
        wait_count++;
    }

    vTaskSuspendAll();
    printf("RESULT lock=%s rounds=%lu high_wait_ms(min/avg/max)=%lu/%lu.%02lu/%lu "
           "low_prio_while_holding_max=%lu (low=%d med=%d high=%d)\n",
           use_mutex ? "mutex" : "binary_semaphore",
           (unsigned long)wait_count, (unsigned long)wait_min,
           (unsigned long)(wait_sum / wait_count),
           (unsigned long)((wait_sum * 100 / wait_count) % 100),
           (unsigned long)wait_max,
           (unsigned long)low_max_prio_while_holding, PRIO_LOW, PRIO_MED, PRIO_HIGH);
    fflush(stdout);
    _exit(0);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: inversion_demo semaphore|mutex [rounds]\n");
        return 1;
    }
    use_mutex = (strcmp(argv[1], "mutex") == 0);
    if (argc > 2) {
        rounds = atoi(argv[2]);
    }

    if (use_mutex) {
        lock = xSemaphoreCreateMutex();
    } else {
        lock = xSemaphoreCreateBinary();
        xSemaphoreGive(lock);   /* binary semaphore starts empty */
    }

    xTaskCreate(low_task, "low", 256, NULL, PRIO_LOW, &low_handle);
    xTaskCreate(med_task, "med", 256, NULL, PRIO_MED, &med_handle);
    xTaskCreate(high_task, "high", 256, NULL, PRIO_HIGH, &high_handle);
    vTaskStartScheduler();
    return 0;
}

void vApplicationMallocFailedHook(void)
{
    printf("malloc failed\n");
    _exit(1);
}

void vAssertCalled(const char *file, int line)
{
    printf("ASSERT %s:%d\n", file, line);
    _exit(2);
}
