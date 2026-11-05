/* Board support for the Linux simulation.
 *
 * The FreeRTOS POSIX port runs every task as a pthread, but only one of
 * them runs at a time (like a single core MCU). The tick comes from a
 * timer thread every 1 ms.
 *
 * Things the real board has in hardware are faked here with normal
 * Linux threads (they are NOT FreeRTOS tasks and never call FreeRTOS):
 *   - "UART RX interrupt": a thread that reads stdin and the --at script
 *     and puts the bytes into the same ring buffer the STM32 ISR uses
 *   - "IWDG": a thread that kills the process if not kicked in time
 * The IMU is the imu_model (fake MPU6500 on the I2C level).
 * The storage sink is a binary file, stalls can be injected.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"
#include "bsp.h"
#include "bsp_sim.h"
#include "imu_model.h"
#include "ringbuf.h"

static sim_options_t opt;
static uint64_t start_ns;
static int sink_fd = -1;
static ringbuf_t console_rx;

static volatile uint32_t pending_stall_ms;
static uint32_t next_periodic_stall_ms;

/* The fake watchdog counts RTOS ticks, not host time. On this PC the
 * POSIX port tick is slower than 1 ms when the machine is busy, so a
 * host-time watchdog fired even when the firmware was healthy. A real
 * IWDG and the real SysTick both run from hardware clocks, so counting
 * ticks here is the closer model. */
static atomic_uint sim_ticks;          /* incremented by the tick hook */
static atomic_int wdg_running;
static atomic_uint wdg_last_kick_tick;
static uint32_t wdg_timeout_ms;

static uint64_t now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

/* host threads must not get the FreeRTOS tick signal */
static void block_signals(void)
{
    sigset_t all;
    sigfillset(&all);
    sigdelset(&all, SIGINT);
    pthread_sigmask(SIG_SETMASK, &all, NULL);
}

/* write directly with write(2) so a host thread never waits on a stdio lock */
static void raw_print(const char *s)
{
    size_t len = strlen(s);
    while (len > 0) {
        ssize_t n = write(STDOUT_FILENO, s, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        s += n;
        len -= (size_t)n;
    }
}

/* ---------------- fake watchdog ---------------- */

static void *watchdog_thread(void *arg)
{
    (void)arg;
    block_signals();
    for (;;) {
        usleep(2000);
        if (!atomic_load(&wdg_running)) {
            continue;
        }
        uint32_t now = atomic_load(&sim_ticks);
        uint32_t age = now - atomic_load(&wdg_last_kick_tick);   /* 1 tick = 1 ms */
        if (age > wdg_timeout_ms) {
            char msg[160];
            snprintf(msg, sizeof(msg),
                     "WATCHDOG RESET at t=%u ms (not kicked for %u ms, timeout %u ms)\n",
                     now, age, wdg_timeout_ms);
            raw_print(msg);
            _exit(3);   /* exit code 3 = watchdog reset, checked by the test script */
        }
    }
    return NULL;
}

/* ---------------- fake UART RX ---------------- */

static void push_string(const char *s)
{
    while (*s) {
        ringbuf_put(&console_rx, (uint8_t)*s);
        s++;
    }
    ringbuf_put(&console_rx, '\n');
}

static void *console_rx_thread(void *arg)
{
    (void)arg;
    int next_script = 0;
    block_signals();

    for (;;) {
        uint32_t t_ms = (uint32_t)((now_ns() - start_ns) / 1000000ull);

        while (next_script < opt.script_count && t_ms >= opt.script[next_script].at_ms) {
            push_string(opt.script[next_script].cmd);
            next_script++;
        }

        if (opt.use_stdin) {
            struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
            if (poll(&pfd, 1, 5) > 0) {
                char c;
                ssize_t n = read(STDIN_FILENO, &c, 1);
                if (n == 1) {
                    ringbuf_put(&console_rx, (uint8_t)c);
                } else if (n == 0) {
                    opt.use_stdin = 0;   /* EOF */
                }
            }
        } else {
            usleep(5000);
        }
    }
    return NULL;
}

/* ---------------- bsp interface ---------------- */

void bsp_sim_set_options(const sim_options_t *o)
{
    opt = *o;
}

void bsp_init(void)
{
    pthread_t th;

    start_ns = now_ns();
    setvbuf(stdout, NULL, _IOLBF, 0);
    ringbuf_init(&console_rx);
    imu_model_reset();

    sink_fd = open(opt.out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (sink_fd < 0) {
        perror("open output file");
        exit(1);
    }
    next_periodic_stall_ms = opt.periodic_stall_period_ms;

    pthread_create(&th, NULL, watchdog_thread, NULL);
    pthread_create(&th, NULL, console_rx_thread, NULL);
}

const char *bsp_name(void)
{
    return "linux-sim (FreeRTOS POSIX port)";
}

uint32_t bsp_time_us(void)
{
    return (uint32_t)((now_ns() - start_ns) / 1000ull);
}

void bsp_log(const char *fmt, ...)
{
    va_list ap;

    /* In the POSIX port a task can be switched out inside printf while it
     * holds the stdio lock, and the next task that prints would deadlock.
     * Suspending the scheduler around printf avoids that (same trick as
     * the official FreeRTOS POSIX demo). */
    vTaskSuspendAll();
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    (void)xTaskResumeAll();
}

int bsp_console_getc(uint8_t *c)
{
    return ringbuf_get(&console_rx, c);
}

int bsp_i2c_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    return imu_model_i2c_write(dev_addr, reg, data, len);
}

int bsp_i2c_read(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    return imu_model_i2c_read(dev_addr, reg, data, len);
}

int bsp_sink_write(const uint8_t *data, uint32_t len)
{
    uint32_t stall = pending_stall_ms;
    uint32_t t_ms = bsp_time_us() / 1000u;

    /* periodic stalls from the command line (like an SD card doing wear levelling) */
    if (opt.periodic_stall_period_ms > 0 && t_ms >= next_periodic_stall_ms) {
        stall = opt.periodic_stall_ms;
        next_periodic_stall_ms = t_ms + opt.periodic_stall_period_ms;
    }

    if (stall > 0) {
        pending_stall_ms = 0;
        /* the card is "busy": the logger task is blocked, other tasks keep running */
        vTaskDelay(pdMS_TO_TICKS(stall));
    }

    while (len > 0) {
        ssize_t n = write(sink_fd, data, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        data += n;
        len -= (uint32_t)n;
    }
    return 0;
}

void bsp_sink_inject_stall(uint32_t ms)
{
    pending_stall_ms = ms;
}

void bsp_watchdog_start(uint32_t timeout_ms)
{
    wdg_timeout_ms = timeout_ms;
    atomic_store(&wdg_last_kick_tick, atomic_load(&sim_ticks));
    atomic_store(&wdg_running, 1);
}

void bsp_watchdog_kick(void)
{
    atomic_store(&wdg_last_kick_tick, atomic_load(&sim_ticks));
}

int bsp_last_reset_was_watchdog(void)
{
    return 0;
}

void bsp_led_toggle(void)
{
    /* no LED on a PC */
}

int bsp_run_finished(void)
{
    if (opt.duration_s == 0) {
        return 0;
    }
    return (now_ns() - start_ns) >= (uint64_t)opt.duration_s * 1000000000ull;
}

void bsp_shutdown(int exit_code)
{
    /* no more task switches and no more watchdog while closing the file */
    vTaskSuspendAll();
    atomic_store(&wdg_running, 0);
    if (sink_fd >= 0) {
        fsync(sink_fd);
        close(sink_fd);
    }
    fflush(stdout);
    _exit(exit_code);
}

void vAssertCalled(const char *file, int line)
{
    char msg[200];
    snprintf(msg, sizeof(msg), "ASSERT failed %s:%d\n", file, line);
    raw_print(msg);
    _exit(2);
}

/* runs in the tick interrupt (signal handler), only counts ticks for the fake watchdog */
void vApplicationTickHook(void)
{
    atomic_fetch_add(&sim_ticks, 1);
}
