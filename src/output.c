#include "output.h"

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* ASCII on purpose: the U+00B7 middle dot did not render in every console
   configuration. */
static const char SEP[] = "|";
/* Built from OUTPUT_COLUMN so the detail indent can never drift away from
   the value column. */
#define INDENT_STR "             "
static const char INDENT[] = INDENT_STR;

const char* output_separator(void)
{
    return SEP;
}

void output_init(void)
{
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hConsole, &mode))
            SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);
#endif
}

/* ---------------- v0.3.0 layout ---------------- */

static void print_label_col(const char* label)
{
    printf("\x1b[90m%-*s\x1b[0m%*s", OUTPUT_LABEL_WIDTH, label ? label : "",
           OUTPUT_LABEL_GAP, "");
}

void print_row(const char* label, const char* value)
{
    print_label_col(label);
    printf("\x1b[97m%s\x1b[0m" OUTPUT_NL, value ? value : "");
}

void print_row_green(const char* label, const char* value)
{
    print_label_col(label);
    printf("\x1b[92m%s\x1b[0m" OUTPUT_NL, value ? value : "");
}

/* Detail lines align with the value column of the row above. White rather
   than grey: grey detail text was unreadable against a dark background. */
void print_row_detail(const char* value)
{
    printf("%s\x1b[97m%s\x1b[0m" OUTPUT_NL, INDENT, value ? value : "");
}

void print_header(const char* text)
{
    printf("\x1b[1;92m%s\x1b[0m" OUTPUT_NL, text ? text : "");
}

/* The screen control below uses the Win32 console API rather than escape
   sequences. Escape sequences are not honoured on every console setup, which
   left stale text from the previous frame on screen. */
static HANDLE console_out(void)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    return h;
}

void output_clear_screen(void)
{
    HANDLE h = console_out();
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (h && GetConsoleScreenBufferInfo(h, &info)) {
        COORD topLeft = { 0, 0 };
        /* Only the visible window: filling the whole buffer would walk the
           entire scrollback on every tick. */
        SHORT width = (SHORT)(info.srWindow.Right - info.srWindow.Left + 1);
        SHORT rows = (SHORT)(info.dwCursorPosition.Y + 1);
        DWORD written;

        if (rows > 0) {
            FillConsoleOutputCharacterA(h, ' ', (DWORD)width * (DWORD)rows,
                                         topLeft, &written);
            FillConsoleOutputAttribute(h, info.wAttributes,
                                       (DWORD)width * (DWORD)rows, topLeft, &written);
        }
        SetConsoleCursorPosition(h, topLeft);
        fflush(stdout);
        return;
    }
    fputs("[H[2J", stdout);
    fflush(stdout);
}

static void set_cursor_visible(int visible)
{
    HANDLE h = console_out();
    CONSOLE_CURSOR_INFO info;

    if (!h || !GetConsoleCursorInfo(h, &info)) return;
    info.bVisible = visible ? TRUE : FALSE;
    SetConsoleCursorInfo(h, &info);
}

void output_hide_cursor(void) { set_cursor_visible(0); }
void output_show_cursor(void) { set_cursor_visible(1); }

int output_quit_requested(void)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD rec;
    DWORD read = 0;

    if (h == INVALID_HANDLE_VALUE) return 0;
    if (!PeekConsoleInput(h, &rec, 1, &read) || read == 0) return 0;

    if (!ReadConsoleInput(h, &rec, 1, &read) || read == 0) return 0;

    if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
        switch (rec.Event.KeyEvent.wVirtualKeyCode) {
            case 'Q': case 'q': case VK_ESCAPE:
                return 1;
            case 'C':   /* Ctrl+C arrives as a key event without a console host */
                if (rec.Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
                    return 1;
                break;
            default:
                break;
        }
    }
    return 0;
}

/* ---------------- legacy layout (frozen POSIX build) ---------------- */

void print_label(const char* label)
{
    printf("\x1b[90m%-10s\x1b[0m", label);
}

void print_value(const char* value)
{
    printf("\x1b[97m%s\x1b[0m", value);
}

void print_value_green(const char* value)
{
    printf("\x1b[92m%s\x1b[0m", value);
}

void print_section(const char* title)
{
    printf("\x1b[1;90m%s\x1b[0m", title);
}

void print_newline(void)
{
    printf(OUTPUT_NL);
}

void reset_color(void)
{
    printf("\x1b[0m");
}

void print_block(const char* label, const char* content)
{
    print_label(label);
    printf("  ");
    print_value(content);
    print_newline();
}

void print_block_green(const char* label, const char* content)
{
    print_label(label);
    printf("  ");
    print_value_green(content);
    print_newline();
}

void print_detail(const char* content)
{
    printf("            ");
    printf("\x1b[90m%s\x1b[0m", content);
    print_newline();
}

void print_summary_line(const char* text)
{
    printf("\x1b[97m%s\x1b[0m" OUTPUT_NL, text);
}

void print_section_title(const char* title)
{
    printf("\x1b[1;90m%s\x1b[0m" OUTPUT_NL, title);
}

void print_item(const char* label, const char* value)
{
    printf("\x1b[90m- %-*s\x1b[0m  \x1b[97m%s\x1b[0m" OUTPUT_NL, 18, label, value);
}

void print_item_detail(const char* label, const char* value)
{
    printf("      \x1b[90m- %-*s\x1b[0m  \x1b[97m%s\x1b[0m" OUTPUT_NL, 18, label, value);
}

void print_storage_line(const char* used_total, const char* pct)
{
    printf("      \x1b[97m%s\x1b[0m  \x1b[90m%s\x1b[0m" OUTPUT_NL, used_total, pct);
}