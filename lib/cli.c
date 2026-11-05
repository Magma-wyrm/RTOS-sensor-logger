#include "cli.h"
#include <string.h>

static const char *skip_spaces(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r') {
        p++;
    }
    return p;
}

/* compares the word at p with name, word ends at space or end of string */
static int word_is(const char *p, const char *name, const char **after)
{
    size_t n = strlen(name);

    if (strncmp(p, name, n) != 0) {
        return 0;
    }
    if (p[n] != '\0' && p[n] != ' ' && p[n] != '\t' && p[n] != '\r') {
        return 0;
    }
    *after = p + n;
    return 1;
}

/* only plain decimal numbers, no sign, no hex. returns -1 if not a number */
static int parse_uint(const char *p, uint32_t *out)
{
    uint32_t value = 0;
    int digits = 0;

    p = skip_spaces(p);
    while (*p >= '0' && *p <= '9') {
        uint32_t d = (uint32_t)(*p - '0');
        if (value > (0xFFFFFFFFu - d) / 10u) {
            return -1;   /* would overflow */
        }
        value = value * 10u + d;
        digits++;
        p++;
    }
    p = skip_spaces(p);
    if (digits == 0 || *p != '\0') {
        return -1;
    }
    *out = value;
    return 0;
}

void cli_parse(const char *line, cli_cmd_t *cmd)
{
    const char *p = skip_spaces(line);
    const char *rest = p;

    cmd->id = CMD_UNKNOWN;
    cmd->arg = 0;

    /* strip trailing spaces / CR by checking what is left after the word */
    if (*p == '\0' || *p == '\r') {
        cmd->id = CMD_NONE;
        return;
    }

    if (word_is(p, "help", &rest)) {
        cmd->id = CMD_HELP;
    } else if (word_is(p, "stats", &rest)) {
        cmd->id = CMD_STATS;
    } else if (word_is(p, "start", &rest)) {
        cmd->id = CMD_START;
    } else if (word_is(p, "stop", &rest)) {
        cmd->id = CMD_STOP;
    } else if (word_is(p, "hang", &rest)) {
        cmd->id = CMD_HANG;
    } else if (word_is(p, "spin", &rest)) {
        cmd->id = CMD_SPIN;
    } else if (word_is(p, "stall", &rest)) {
        uint32_t ms;
        if (parse_uint(rest, &ms) != 0 || ms == 0 || ms > CLI_MAX_STALL_MS) {
            cmd->id = CMD_BAD_ARG;
        } else {
            cmd->id = CMD_STALL;
            cmd->arg = ms;
        }
        return;
    } else {
        return; /* CMD_UNKNOWN */
    }

    /* commands without arguments must not have extra text after them */
    rest = skip_spaces(rest);
    if (*rest != '\0' && *rest != '\r') {
        cmd->id = CMD_BAD_ARG;
    }
}
