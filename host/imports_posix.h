/* Host-backed implementations of the engine's libc/zlib/stdio imports. */
#pragma once

#include <cstdint>

struct uc_struct;
typedef struct uc_struct uc_engine;

/* Register all POSIX-ish import handlers with the stub dispatcher. */
void register_posix_imports();
