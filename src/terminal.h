#ifndef HF_TERMINAL_H
#define HF_TERMINAL_H

#include "hf_state.h"

/* Terminal identity from environment variables and the Windows Terminal
   settings file, console size from the screen buffer, font from conhost.
   Colour capability mirrors what TERM / COLORTERM / ANSICON report on a
   POSIX system: which colour depth the emulator advertises, not its palette. */
void terminal_collect(hf_state* s);

#endif