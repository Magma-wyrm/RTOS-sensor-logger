#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* FreeRTOS config for the Linux simulation (POSIX port).
 * Kept as close as possible to the STM32 config so the kernel behaves the
 * same way (same tick rate, priorities, mutex settings). */

#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    8
#define configMINIMAL_STACK_SIZE                256
#define configTOTAL_HEAP_SIZE                   (64 * 1024)
#define configMAX_TASK_NAME_LEN                 12
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_TASK_NOTIFICATIONS            1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0

#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     1   /* used by the priority inversion demo */
#define configCHECK_FOR_STACK_OVERFLOW          0   /* the POSIX port uses pthread stacks, check is done on the STM32 build */
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_TRACE_FACILITY                1
#define configGENERATE_RUN_TIME_STATS           0

#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1

#define configASSERT(x) do { if (!(x)) { vAssertCalled(__FILE__, __LINE__); } } while (0)
void vAssertCalled(const char *file, int line);

#endif
