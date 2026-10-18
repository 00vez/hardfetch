#define _GNU_SOURCE
#include "../terminal.h"
#include "../debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void env_str(const char* name, char* out, size_t outLen)
{
    const char* v = getenv(name);
    out[0] = 0;
    if (v && *v) snprintf(out, outLen, "%s", v);
}

static int env_present(const char* name)
{
    const char* v = getenv(name);
    return (v && *v) ? 1 : 0;
}

/* The colour capability a terminal advertises is the POSIX equivalent of the
   xterm-256color in tmux: TERM names the palette depth, COLORTERM names the
   true colour extension. */
static void collect_colour_capability(hf_state* s)
{
    char buf[64];

    s->term_color[0] = 0;

    env_str("COLORTERM", buf, sizeof(buf));
    if (buf[0]) {
        snprintf(s->term_color, sizeof(s->term_color), "%s", buf);
        debug_log("COLORTERM = '%s'", buf);
        return;
    }

    env_str("TERM", buf, sizeof(buf));
    if (buf[0]) {
        snprintf(s->term_color, sizeof(s->term_color), "%s", buf);
        debug_log("TERM = '%s'", buf);
        return;
    }

    debug_log("no colour capability environment variable set");
}

void terminal_collect(hf_state* s)
{
    char buf[64];
    struct winsize ws;

    debug_begin("terminal");

    s->term_name[0] = 0;
    s->term_font[0] = 0;
    s->term_scheme[0] = 0;
    s->term_cols = 0;
    s->term_rows = 0;

    env_str("TERM_PROGRAM", buf, sizeof(buf));
    if (env_present("WT_SESSION")) {
        snprintf(s->term_name, sizeof(s->term_name), "Windows Terminal");
    } else if (buf[0]) {
        char v[64];
        env_str("TERM_PROGRAM_VERSION", v, sizeof(v));
        snprintf(s->term_name, sizeof(s->term_name), "%s%s%s", buf, v[0] ? " " : "", v);
    } else if (env_present("KITTY_WINDOW_ID")) {
        snprintf(s->term_name, sizeof(s->term_name), "kitty");
    } else if (env_present("ALACRITTY_WINDOW_ID")) {
        snprintf(s->term_name, sizeof(s->term_name), "Alacritty");
    } else if (env_present("WEZTERM_PANE")) {
        snprintf(s->term_name, sizeof(s->term_name), "WezTerm");
    } else if (env_present("SSH_CONNECTION")) {
        snprintf(s->term_name, sizeof(s->term_name), "ssh");
    } else if (env_present("VTE_VERSION")) {
        snprintf(s->term_name, sizeof(s->term_name), "VTE");
    } else if (env_present("TERMINAL_EMULATOR")) {
        env_str("TERMINAL_EMULATOR", buf, sizeof(buf));
        snprintf(s->term_name, sizeof(s->term_name), "%s", buf);
    } else {
        env_str("TERM_PROGRAM", buf, sizeof(buf));
        snprintf(s->term_name, sizeof(s->term_name), "%s", buf[0] ? buf : "Terminal");
        debug_log("no terminal variable set, assuming a generic terminal");
    }

    collect_colour_capability(s);

    /* Console size. */
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        s->term_cols = ws.ws_col;
        s->term_rows = ws.ws_row;
        debug_log("console size %dx%d", s->term_cols, s->term_rows);
    } else {
        debug_log("TIOCGWINSZ unavailable");
    }

    /* Font size is not reachable without talking to the terminal emulator
       itself, so the row is left out rather than guessed. */
    debug_log("terminal = '%s' colour='%s' size=%dx%d",
              s->term_name, s->term_color, s->term_cols, s->term_rows);
    debug_end();
}
