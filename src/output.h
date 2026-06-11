#ifndef HF_OUTPUT_H
#define HF_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

void output_init(void);
void print_label(const char* label);
void print_value(const char* value);
void print_value_green(const char* value);
void print_section(const char* title);
void print_newline(void);
void reset_color(void);

// New block format: gray label (fixed 10-char) + white value
void print_block(const char* label, const char* content);
void print_block_green(const char* label, const char* content);
// Indented gray detail line
void print_detail(const char* content);

// New compact format
void print_header(const char* text);       // Tool name/version, user@host - green
void print_summary_line(const char* text); // OS · Shell · Uptime - white
void print_section_title(const char* title); // Section titles - bold gray
void print_item(const char* label, const char* value); // - Label      Value
void print_item_detail(const char* label, const char* value); // Indented detail line
void print_storage_line(const char* used_total, const char* pct); // Storage line

#ifdef __cplusplus
}
#endif

#endif