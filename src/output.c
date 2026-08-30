#include "output.h"

#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif

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
#endif
}

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
    printf("\n");
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

void print_header(const char* text)
{
    printf("\x1b[92m%s\x1b[0m\n", text);
}

void print_summary_line(const char* text)
{
    printf("\x1b[97m%s\x1b[0m\n", text);
}

void print_section_title(const char* title)
{
    printf("\x1b[1;90m%s\x1b[0m\n", title);
}

void print_item(const char* label, const char* value)
{
    printf("\x1b[90m- %-*s\x1b[0m  \x1b[97m%s\x1b[0m\n", 18, label, value);
}

void print_item_detail(const char* label, const char* value)
{
    printf("      \x1b[90m- %-*s\x1b[0m  \x1b[97m%s\x1b[0m\n", 18, label, value);
}

void print_storage_line(const char* used_total, const char* pct)
{
    printf("      \x1b[97m%s\x1b[0m  \x1b[90m%s\x1b[0m\n", used_total, pct);
}
