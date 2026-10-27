/* OpenGL ES bridge.
 *
 * The game's GLSurfaceView creates a GLES 1.x context (n.smali passes a null
 * attrib array to eglCreateContext). The engine's fixed-function calls
 * (glMatrixMode / glVertexPointer / glDrawArrays / ...) are therefore
 * forwarded straight to the real libGLESv1_CM. Its shader calls
 * (glCreateShader / ...) fail against a GLES1 context exactly like on the
 * original devices ("shader compile error" in the startup log) and the engine
 * falls back to fixed-function rendering.
 *
 * Pointers passed to GL live in emulated memory. The emulated heap/stack are
 * zero-copy host aliases, so most client-array pointers resolve directly;
 * anything else is copied into scratch buffers at draw time. */
#pragma once

#include <cstdint>

struct uc_struct;
typedef struct uc_struct uc_engine;

void register_gl_imports();
