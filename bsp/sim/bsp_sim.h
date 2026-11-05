#ifndef BSP_SIM_H
#define BSP_SIM_H

#include <stdint.h>

#define SIM_MAX_SCRIPT 16

typedef struct {
    uint32_t at_ms;
    char cmd[40];
} sim_script_entry_t;

typedef struct {
    uint32_t duration_s;                /* 0 = run forever */
    const char *out_path;
    int use_stdin;
    uint32_t periodic_stall_period_ms;  /* 0 = off */
    uint32_t periodic_stall_ms;
    int script_count;
    sim_script_entry_t script[SIM_MAX_SCRIPT];
} sim_options_t;

void bsp_sim_set_options(const sim_options_t *o);

#endif
