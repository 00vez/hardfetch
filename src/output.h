#ifndef HF_OUTPUT_H
#define HF_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Label column: %-11s + 2 spaces -> value starts at column 14.
   Detail rows are indented to that same column. */
#define OUTPUT_LABEL_WIDTH 11
#define OUTPUT_LABEL_GAP   2
#define OUTPUT_COLUMN      (OUTPUT_LABEL_WIDTH + OUTPUT_LABEL_GAP)   /* 13 */

/* stdout and stderr are in text mode, so the CRT already turns a bare LF
   into CRLF on Windows. Emitting an explicit CR here would produce CR CR LF. */
#define OUTPUT_NL "\n"

void output_init(void);

/* --- v0.3.0 layout (Windows) --- */
void print_row(const char* label, const char* value);       /* gray label, white value   */
void print_row_green(const char* label, const char* value);  /* gray label, green value   */
void print_row_detail(const char* value);                   /* indented gray continuation */

/* Same colour as a detail row but starting at column 0, for rows that sit
   outside the value column such as the extra network adapters. */
void print_line_flush(const char* value);
void print_header(const char* text);                        /* green, own line           */

void output_clear_screen(void);
void output_hide_cursor(void);
void output_show_cursor(void);

/* Non-zero once q, Esc or Ctrl+C has been seen on the console. Used by --live
   to leave the loop without relying on a signal handler. */
int  output_quit_requested(void);

const char* output_separator(void);                         /* U+00B7 MIDDLE DOT */

/* Legacy v0.2.4 layout, kept for the POSIX tree. */
void print_label(const char* label);
void print_value(const char* value);
void print_value_green(const char* value);
void print_section(const char* title);
void print_newline(void);
void reset_color(void);
void print_block(const char* label, const char* content);
void print_block_green(const char* label, const char* content);
void print_detail(const char* content);
void print_summary_line(const char* text);
void print_section_title(const char* title);
void print_item(const char* label, const char* value);
void print_item_detail(const char* label, const char* value);
void print_storage_line(const char* used_total, const char* pct);

#ifdef __cplusplus
}
#endif

#endif