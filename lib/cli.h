#ifndef CLI_H
#define CLI_H

#include <stdint.h>

typedef enum {
    CMD_NONE = 0,     /* empty line */
    CMD_UNKNOWN,
    CMD_BAD_ARG,
    CMD_HELP,
    CMD_STATS,
    CMD_START,
    CMD_STOP,
    CMD_STALL,        /* stall <ms>  : inject a storage stall (fault injection) */
    CMD_HANG,         /* hang        : logger task blocks forever */
    CMD_SPIN,         /* spin        : logger task busy-loops forever */
} cli_cmd_id_t;

typedef struct {
    cli_cmd_id_t id;
    uint32_t arg;
} cli_cmd_t;

#define CLI_MAX_STALL_MS 10000u

/* Parses one line (without the newline). Leading/trailing spaces are ok. */
void cli_parse(const char *line, cli_cmd_t *cmd);

#endif
