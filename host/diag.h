/* Diagnostics: log to logcat AND to a world-readable file so problems can be
 * reported without adb (file manager -> Download/zcr-log.txt). */
#pragma once

#include <cstdint>

void diag_log(const char *fmt, ...);
void diag_note_unimpl(const char *name);
void diag_note_emu_error(const char *where, const char *what);
void diag_note_draw();
void diag_summary();

/* Ring buffer of recent engine->Java calls, dumped at crash time so we can
 * see the exact call sequence before a fault (ChatGPT's "compare the last
 * successful frame with the first failed one"). */
void diag_trace(const char *fmt, ...);
void diag_dump_traces();

extern uint32_t g_diag_unimpl;
extern uint32_t g_diag_emu_errors;
extern uint32_t g_diag_draws;
