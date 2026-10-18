#ifndef HF_RENDER_H
#define HF_RENDER_H

#include "hf_state.h"

/* Draws the whole report. options is the -a / --all state. */
void render(const hf_state* s, int options);

#define RENDER_OPT_ALL 1

#endif