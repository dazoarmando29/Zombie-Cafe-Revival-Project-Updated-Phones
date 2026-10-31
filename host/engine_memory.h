/* Zombie Cafe Revival - 64-bit host: emulated address space map.
 *
 * The original 32-bit engine runs under an ARM32 CPU emulator (Unicorn).
 * Everything the engine can see lives in this flat 32-bit address space. */
#pragma once

#include <stdint.h>

/* Engine image load base (the ELF is vaddr-0 based; we relocate it here). */
#define ENG_BASE        0x10000000u

/* Import trampoline page: one 16-byte stub per imported symbol.
 * Executing a stub traps into the host dispatch loop. */
#define STUB_PAGE       0xF0000000u
#define STUB_SLOT_SIZE  16u

/* Fake JNIEnv / JavaVM function pointer pages (same trap trick).
 * JNIEnv vtable slots:  JNI_STUB_PAGE + i*16         (i < 512)
 * JavaVM  vtable slots: JNI_STUB_PAGE + (512+i)*16   (JNI_STUB_PAGE+0x8000 area) */
#define JNI_STUB_PAGE   0xF1000000u
#define JAVA_VM_VTABLE  0xF1008000u
#define JAVA_VM_SLOT_BASE 512u

/* Magic return address: finishing a host->engine call lands here. */
#define EMU_RETURN_ADDR 0xEFFFF000u

/* Emulated stack (grows down from top). */
#define STACK_BASE      0xE0000000u
#define STACK_SIZE      (8u * 1024u * 1024u)

/* Emulated heap used by hooked malloc/free. The cafe's texture loading peaks
 * well over 256MB with JNI buffers in play - give it real headroom. */
#define HEAP_BASE       0x70000000u
#define HEAP_SIZE       (1536u * 1024u * 1024u)

/* Where the fake JNIEnv / JavaVM objects live. */
#define JNI_ENV_OBJ     0xF2000000u
#define JAVA_VM_OBJ     0xF2000100u

/* Convenience: engine RVA -> emulated address. */
#define ENG_ADDR(rva)   ((uint32_t)(ENG_BASE + (rva)))
