/* Command task: reads characters from the console (UART RX interrupt puts
 * them in a ring buffer), builds a line and runs the command.
 * Polling every 20 ms is simple and fast enough for a human typing. */
#include <string.h>
#include "app.h"
#include "app_config.h"
#include "bsp.h"
#include "cli.h"

#define CMD_LINE_LEN 48

/* time in RTOS ticks (1 tick = 1 ms), used to time the fault tests */
static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void print_help(void)
{
    bsp_log("commands:\r\n"
            "  stats        print counters\r\n"
            "  start / stop start or stop logging\r\n"
            "  stall <ms>   next storage write takes <ms> (fault test)\r\n"
            "  hang         logger task blocks forever (watchdog test)\r\n"
            "  spin         logger task loops forever (watchdog test)\r\n");
}

static void run_command(const char *line)
{
    cli_cmd_t cmd;

    cli_parse(line, &cmd);
    switch (cmd.id) {
    case CMD_NONE:
        break;
    case CMD_HELP:
        print_help();
        break;
    case CMD_STATS:
        app_print_stats("STATS");
        break;
    case CMD_START:
        g_logging_enabled = 1;
        bsp_log("logging started\r\n");
        break;
    case CMD_STOP:
        g_logging_enabled = 0;
        bsp_log("logging stopped\r\n");
        break;
    case CMD_STALL:
        bsp_log("FAULT: next storage write will stall %lu ms, at t=%lu ms\r\n",
                (unsigned long)cmd.arg, (unsigned long)now_ms());
        bsp_sink_inject_stall(cmd.arg);
        break;
    case CMD_HANG:
        bsp_log("FAULT: logger hang at t=%lu ms\r\n", (unsigned long)now_ms());
        g_logger_fault = 1;
        break;
    case CMD_SPIN:
        bsp_log("FAULT: logger spin at t=%lu ms\r\n", (unsigned long)now_ms());
        g_logger_fault = 2;
        break;
    case CMD_BAD_ARG:
        bsp_log("bad argument, type help\r\n");
        break;
    default:
        bsp_log("unknown command '%s', type help\r\n", line);
        break;
    }
}

void command_task(void *arg)
{
    (void)arg;
    char line[CMD_LINE_LEN];
    uint32_t len = 0;
    uint8_t c;

    for (;;) {
        heartbeat(HB_COMMAND);

        while (bsp_console_getc(&c) == 0) {
            if (c == '\n' || c == '\r') {
                line[len] = '\0';
                if (len > 0) {
                    run_command(line);
                }
                len = 0;
            } else if (len < CMD_LINE_LEN - 1) {
                line[len++] = (char)c;
            }
            /* characters after CMD_LINE_LEN-1 are ignored until end of line */
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
