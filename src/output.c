#include "output.h"

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* U+00B7 MIDDLE DOT, UTF-8 C2 B7 */
static const char SEP[] = "\xC2\xB7";
static const char INDENT[] = "             "; /* OUTPUT_COLUMN spaces */

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

void output_clear_screen(void)
{
    fputs("\x1b[H\x1b[2J", stdout);
    fflush(stdout);
}

void output_hide_cursor(void)
{
    fputs("\x1b[?25l", stdout);
    fflush(stdout);
}

void output_show_cursor(void)
{
    fputs("\x1b[?25h", stdout);
    fflush(stdout);
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