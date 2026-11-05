/* Entry point of the Linux simulation build.
 *
 * usage: logger_sim [options]
 *   --duration <s>                stop after s seconds and print SUMMARY (default: run forever)
 *   --out <file>                  binary log file (default log.bin)
 *   --periodic-stall <every>:<ms> every <every> ms the next storage write stalls <ms>
 *   --at <ms>:"<command>"         type a console command at time <ms> (can repeat)
 *   --no-stdin                    do not read commands from the keyboard
 *
 * example: ./logger_sim --duration 60 --at 20000:"stall 1500"
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app.h"
#include "bsp.h"
#include "bsp_sim.h"

static void usage(void)
{
    printf("usage: logger_sim [--duration s] [--out file] [--periodic-stall every_ms:stall_ms]\n"
           "                  [--at ms:\"command\"]... [--no-stdin]\n");
    exit(1);
}

int main(int argc, char **argv)
{
    sim_options_t o;

    memset(&o, 0, sizeof(o));
    o.out_path = "log.bin";
    o.use_stdin = isatty(STDIN_FILENO);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            o.duration_s = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            o.out_path = argv[++i];
        } else if (strcmp(argv[i], "--periodic-stall") == 0 && i + 1 < argc) {
            unsigned every, ms;
            if (sscanf(argv[++i], "%u:%u", &every, &ms) != 2 || every == 0) {
                usage();
            }
            o.periodic_stall_period_ms = every;
            o.periodic_stall_ms = ms;
        } else if (strcmp(argv[i], "--at") == 0 && i + 1 < argc) {
            const char *arg = argv[++i];
            const char *colon = strchr(arg, ':');
            if (colon == NULL || o.script_count >= SIM_MAX_SCRIPT) {
                usage();
            }
            sim_script_entry_t *e = &o.script[o.script_count++];
            e->at_ms = (uint32_t)strtoul(arg, NULL, 10);
            snprintf(e->cmd, sizeof(e->cmd), "%s", colon + 1);
        } else if (strcmp(argv[i], "--no-stdin") == 0) {
            o.use_stdin = 0;
        } else if (strcmp(argv[i], "--stdin") == 0) {
            o.use_stdin = 1;
        } else {
            usage();
        }
    }

    bsp_sim_set_options(&o);
    bsp_init();
    app_start();   /* does not return */
    return 0;
}
