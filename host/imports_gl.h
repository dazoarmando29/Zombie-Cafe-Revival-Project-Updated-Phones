/* OpenGL ES bridge.
 *
 * The engine mixes GLES1 fixed-function (glMatrixMode/glVertexPointer/...)
 * with GLES2 shader calls (glCreateShader/...). We request a GLES2 context
 * from the Java side, pass shader calls straight through to the driver, and
 * translate the fixed-function subset through a built-in shader program. */
#pragma once

#include <cstdint>

struct uc_struct;
typedef struct uc_struct uc_engine;

void register_gl_imports();
