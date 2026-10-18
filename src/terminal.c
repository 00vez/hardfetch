#include "terminal.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static void env_str(const char* name, char* out, size_t outLen)
{
    char* v = NULL;
    size_t need = 0;
    out[0] = 0;
    if (_dupenv_s(&v, &need, name) != 0 || !v) { free(v); return; }
    snprintf(out, outLen, "%s", v);
    free(v);
}

static int env_present(const char* name)
{
    char* v = NULL;
    size_t need = 0;
    int present = (_dupenv_s(&v, &need, name) == 0 && v && v[0]) ? 1 : 0;
    free(v);
    return present;
}
/* Windows Terminal keeps profiles.defaults.colorScheme in settings.json.
   The built-in schemes are not in that file, so a profile without a
   colourScheme is reported as Campbell, the shipped default. A targeted
   scan is used instead of a JSON parser: the file is user-editable and
   comments are legal in it. */
static void read_wt_scheme(hf_state* s)
{
    static const char* rel =
        "Packages\\Microsoft.WindowsTerminal_8wekyb3d8bbwe\\LocalState\\settings.json";
    char path[512];
    char* json;
    FILE* f;
    long size;
    const char* p;
    const char* defaultProfile = NULL;

    snprintf(s->term_scheme, sizeof(s->term_scheme), "Campbell");

    {
        DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", path, (DWORD)sizeof(path));
        if (n == 0 || n >= sizeof(path)) return;
    }
    {
        size_t n = strlen(path);
        snprintf(path + n, sizeof(path) - n, "\\%s", rel);
    }

    if (fopen_s(&f, path, "rb") != 0) f = NULL;
    if (!f) {
        debug_log("Windows Terminal settings not found at '%s'", path);
        return;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 4 * 1024 * 1024) {
        fclose(f);
        return;
    }
    json = (char*)malloc((size_t)size + 1);
    if (!json) {
        fclose(f);
        return;
    }
    size = (long)fread(json, 1, (size_t)size, f);
    fclose(f);
    json[size] = '\0';

    if (debug_is_enabled())
        debug_raw("WT settings.json", json, (size_t)size);

    /* Find the profile the terminal actually opened, then its colourScheme. */
    p = strstr(json, "\"defaultProfile\"");
    if (p) p = strchr(p + 16, ':');
    if (p) {
        const char* q = strchr(p, '"');
        if (q) {
            const char* e = strchr(q + 1, '"');
            if (e && (size_t)(e - q - 1) < 48) {
                defaultProfile = q + 1;
                /* look for "guid": "<defaultProfile>" */
                {
                    const char* r = json;
                    while ((r = strstr(r, defaultProfile)) != NULL) {
                        const char* before = r;
                        while (before > json && *before != '{' && *before != ',') before--;
                        if (before > json && strstr(before, "\"guid\"")) {
                            const char* cs = strstr(r, "\"colorScheme\"");
                            if (cs && cs < r + 400) {
                                const char* v = strchr(cs + 13, '"');
                                if (v) {
                                    const char* w = strchr(v + 1, '"');
                                    if (w && (size_t)(w - v - 1) < sizeof(s->term_scheme))
                                        snprintf(s->term_scheme, sizeof(s->term_scheme), "%.*s",
                                                 (int)(w - v - 1), v + 1);
                                }
                            }
                            break;
                        }
                        r++;
                    }
                }
            }
        }
    }

    if (s->term_scheme[0] == '\0')
        snprintf(s->term_scheme, sizeof(s->term_scheme), "Campbell");

    debug_log("Windows Terminal scheme = '%s' (defaultProfile %s)",
              s->term_scheme, defaultProfile ? "found" : "not found");
    free(json);
}

static void collect_colour_capability(hf_state* s)
{
    char buf[64];

    s->term_color[0] = '\0';

    env_str("COLORTERM", buf, sizeof(buf));
    if (buf[0]) {
        snprintf(s->term_color, sizeof(s->term_color), "%s", buf);
        debug_log("COLORTERM = '%s'", buf);
        return;
    }

    env_str("ANSICON", buf, sizeof(buf));
    if (buf[0]) {
        snprintf(s->term_color, sizeof(s->term_color), "conemu");
        debug_log("ANSICON present, reporting conemu");
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
    HANDLE h;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    CONSOLE_FONT_INFOEX cfi;
    char wtSession[64];
    char termProgram[64];

    debug_begin("terminal");

    s->term_name[0] = '\0';
    s->term_cols = 0;
    s->term_rows = 0;
    s->term_font[0] = '\0';

    env_str("WT_SESSION", wtSession, sizeof(wtSession));
    env_str("TERM_PROGRAM", termProgram, sizeof(termProgram));

    if (wtSession[0]) {
        snprintf(s->term_name, sizeof(s->term_name), "Windows Terminal");
        read_wt_scheme(s);
    } else if (env_present("ConEmuPID")) {
        snprintf(s->term_name, sizeof(s->term_name), "ConEmu");
    } else if (termProgram[0]) {
        char v[64];
        env_str("TERM_PROGRAM_VERSION", v, sizeof(v));
        snprintf(s->term_name, sizeof(s->term_name), "%s%s%s",
                 termProgram, v[0] ? " " : "", v);
    } else if (env_present("ALACRITTY_WINDOW_ID")) {
        snprintf(s->term_name, sizeof(s->term_name), "Alacritty");
    } else if (env_present("KITTY_WINDOW_ID")) {
        snprintf(s->term_name, sizeof(s->term_name), "kitty");
    } else if (env_present("SSH_CONNECTION")) {
        snprintf(s->term_name, sizeof(s->term_name), "ssh");
    } else if (env_present("MSYSTEM")) {
        snprintf(s->term_name, sizeof(s->term_name), "MSYS2");
    } else {
        snprintf(s->term_name, sizeof(s->term_name), "Windows Console");
        debug_log("no terminal environment variable set, assuming conhost");
    }

    collect_colour_capability(s);

    /* Console size. Windows Terminal does not answer GetConsoleScreenBufferInfo
       reliably for the visible window, so CONOUT$ is tried as well. */
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h, &csbi)) {
        s->term_cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        s->term_rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        debug_log("console size from stdout handle: %dx%d", s->term_cols, s->term_rows);
    } else {
        debug_log("GetConsoleScreenBufferInfo failed: %s", debug_win32(GetLastError()));
    }

    /* Font. Only conhost implements GetCurrentConsoleFontEx; Windows Terminal
       and ConEmu leave the row unprintable, which is the intended behaviour. */
    ZeroMemory(&cfi, sizeof(cfi));
    cfi.cbSize = sizeof(cfi);
    if (h != INVALID_HANDLE_VALUE && GetCurrentConsoleFontEx(h, FALSE, &cfi)) {
        if (cfi.dwFontSize.X > 0 && cfi.dwFontSize.Y > 0) {
            char face[LF_FACESIZE] = {0};
            WideCharToMultiByte(CP_UTF8, 0, cfi.FaceName, -1, face, (int)sizeof(face), NULL, NULL);
            snprintf(s->term_font, sizeof(s->term_font), "%s %ux%u",
                     face[0] ? face : "?", (unsigned)cfi.dwFontSize.X, (unsigned)cfi.dwFontSize.Y);
            debug_log("console font = '%s'", s->term_font);
        }
    } else {
        debug_log("GetCurrentConsoleFontEx unsupported here (expected under Windows Terminal)");
    }

    debug_log("terminal = '%s' scheme='%s' colour='%s' size=%dx%d",
              s->term_name, s->term_scheme, s->term_color, s->term_cols, s->term_rows);
    debug_end();
}