/* Unit tests for emu::Heap (the emulated-heap allocator core).
 *
 * Build (NDK clang, self-contained binary) and run on an Android device:
 *   see host/tests/run_heap_tests.ps1
 *
 * Covers: allocation/reuse, coalescing, fragmentation scenarios,
 * realloc grow/shrink, invalid pointers, double frees, exhaustion +
 * recovery, data integrity, and describe() output. */
#include "../emu_heap.h"

#include <cstdio>
#include <cstring>
#include <vector>

using emu::Heap;
using emu::HeapStats;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            g_failures++;                                                  \
            printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        g_checks++;                                                        \
        unsigned long long va = (unsigned long long)(a);                   \
        unsigned long long vb = (unsigned long long)(b);                   \
        if (va != vb) {                                                    \
            g_failures++;                                                  \
            printf("[FAIL] %s:%d: %s == %s (%llu vs %llu)\n", __FILE__,    \
                   __LINE__, #a, #b, va, vb);                              \
        }                                                                  \
    } while (0)

static std::vector<uint8_t> g_buf;
static const uint32_t kBase = 0x70000000u;

static void make_heap(Heap &h, size_t size) {
    g_buf.assign(size, 0);
    bool ok = h.init(g_buf.data(), kBase, size);
    CHECK(ok);
}

/* write a recognizable pattern into an allocation and verify it later */
static void pattern_write(uint32_t addr, size_t n, uint8_t seed) {
    uint8_t *p = g_buf.data() + (addr - kBase);
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)(seed ^ i);
}
static bool pattern_check(uint32_t addr, size_t n, uint8_t seed) {
    uint8_t *p = g_buf.data() + (addr - kBase);
    for (size_t i = 0; i < n; i++)
        if (p[i] != (uint8_t)(seed ^ i)) return false;
    return true;
}

static void test_basic() {
    printf("-- test_basic\n");
    Heap h;
    make_heap(h, 1 << 20);
    uint32_t a = h.alloc(100);
    uint32_t b = h.alloc(200);
    uint32_t c = h.alloc(300);
    CHECK(a && b && c);
    CHECK((a & 7) == 0 && (b & 7) == 0 && (c & 7) == 0);
    CHECK(a != b && b != c && a != c);
    pattern_write(a, 100, 1);
    pattern_write(b, 200, 2);
    pattern_write(c, 300, 3);
    CHECK(pattern_check(a, 100, 1));
    CHECK(pattern_check(b, 200, 2));
    CHECK(pattern_check(c, 300, 3));
    CHECK_EQ(h.free(b, 0), 0);
    CHECK(pattern_check(a, 100, 1));
    CHECK(pattern_check(c, 300, 3));
    uint32_t d = h.alloc(180);
    CHECK(d == b); /* LIFO free list reuses the freed block first */
    HeapStats st;
    h.stats(&st);
    CHECK_EQ(st.total_allocs, 4);
    CHECK_EQ(st.total_frees, 1);
    CHECK_EQ(st.bad_frees, 0);
    CHECK_EQ(st.double_frees, 0);
}

static void test_coalesce() {
    printf("-- test_coalesce\n");
    Heap h;
    make_heap(h, 1 << 20);
    /* three adjacent blocks */
    uint32_t a = h.alloc(64);
    uint32_t b = h.alloc(64);
    uint32_t c = h.alloc(64);
    CHECK(b == a + 72 && c == b + 72); /* 64 user + 8 header */
    CHECK_EQ(h.free(a, 0), 0);
    CHECK_EQ(h.free(c, 0), 0); /* c merges forward with the tail block */
    HeapStats st;
    h.stats(&st);
    CHECK_EQ(st.free_blocks, 2); /* a, and c+tail merged */
    CHECK_EQ(h.free(b, 0), 0);   /* merges a+b+c+tail in one call */
    h.stats(&st);
    CHECK(st.coalesces >= 3);
    CHECK_EQ(st.free_blocks, 1); /* everything back into one block */
    /* a block spanning a+b+c must now succeed */
    uint32_t big = h.alloc(176);
    CHECK(big == a);
}

static void test_coalesce_backward_only() {
    printf("-- test_coalesce_backward_only\n");
    Heap h;
    make_heap(h, 1 << 20);
    uint32_t a = h.alloc(48);
    uint32_t b = h.alloc(48);
    CHECK_EQ(h.free(a, 0), 0);
    CHECK_EQ(h.free(b, 0), 0); /* must merge into previous free block */
    HeapStats st;
    h.stats(&st);
    CHECK(st.coalesces >= 1);
    uint32_t big = h.alloc(48 * 2 + 16);
    CHECK(big == a);
}

static void test_fragmentation() {
    printf("-- test_fragmentation\n");
    Heap h;
    make_heap(h, 1 << 20);
    std::vector<uint32_t> ptrs;
    for (int i = 0; i < 200; i++) {
        uint32_t p = h.alloc(64 + (i % 7) * 8);
        CHECK(p != 0);
        ptrs.push_back(p);
    }
    /* free every other block: classic checkerboard */
    for (size_t i = 0; i < ptrs.size(); i += 2) CHECK_EQ(h.free(ptrs[i], 0), 0);
    /* a 2KB single alloc must still succeed (tail coalesces), while many
       64-byte allocs must fill the checkerboard holes */
    uint32_t big = h.alloc(2048);
    CHECK(big != 0);
    std::vector<uint32_t> fillers;
    for (int i = 0; i < 100; i++) {
        uint32_t p = h.alloc(64);
        CHECK(p != 0);
        fillers.push_back(p);
    }
    /* free everything and verify full coalescing recovery */
    for (size_t i = 1; i < ptrs.size(); i += 2) CHECK_EQ(h.free(ptrs[i], 0), 0);
    for (uint32_t p : fillers) CHECK_EQ(h.free(p, 0), 0);
    CHECK_EQ(h.free(big, 0), 0);
    HeapStats st;
    h.stats(&st);
    CHECK_EQ(st.free_blocks, 1); /* everything merged back */
    CHECK_EQ(st.cur_blocks, 0);
    CHECK_EQ(st.cur_bytes, 0);
}

static void test_realloc() {
    printf("-- test_realloc\n");
    Heap h;
    make_heap(h, 1 << 20);
    uint32_t a = h.alloc(64);
    pattern_write(a, 64, 9);
    uint32_t b = h.realloc(a, 32, 0); /* shrink in place */
    CHECK_EQ(b, a);
    CHECK(pattern_check(b, 32, 9));
    uint32_t c = h.realloc(b, 512, 0); /* grow: must move */
    CHECK(c != 0);
    CHECK(pattern_check(c, 32, 9));
    uint32_t d = h.realloc(0, 100, 0); /* realloc(null) == alloc */
    CHECK(d != 0);
    CHECK_EQ(h.realloc(d, 0, 0), 0); /* realloc(p, 0) == free */
    HeapStats st;
    h.stats(&st);
    CHECK_EQ(st.bad_frees, 0);
    CHECK_EQ(st.double_frees, 0);
}

static void test_invalid_frees() {
    printf("-- test_invalid_frees\n");
    Heap h;
    make_heap(h, 1 << 20);
    uint32_t a = h.alloc(64);
    CHECK(a != 0);
    /* wild pointers */
    CHECK_EQ(h.free(kBase + 4, 0), 1);          /* unaligned */
    CHECK_EQ(h.free(kBase - 8, 0), 1);          /* below heap */
    CHECK_EQ(h.free(kBase + (2u << 20), 0), 1); /* above heap */
    CHECK_EQ(h.free(0x12345678, 0), 1);         /* garbage */
    /* double free */
    CHECK_EQ(h.free(a, 0), 0);
    CHECK_EQ(h.free(a, 0), 2);
    CHECK_EQ(h.free(a, 0), 2);
    HeapStats st;
    h.stats(&st);
    CHECK_EQ(st.bad_frees, 4);
    CHECK_EQ(st.double_frees, 2);
    /* allocator still works after abuse */
    uint32_t b = h.alloc(64);
    CHECK(b != 0);
}

static void test_exhaustion_recovery() {
    printf("-- test_exhaustion_recovery\n");
    Heap h;
    make_heap(h, 4096);
    uint32_t a = h.alloc(1024);
    uint32_t b = h.alloc(1024);
    CHECK(a && b);
    uint32_t c = h.alloc(4096); /* way too big */
    CHECK_EQ(c, 0);
    HeapStats st;
    h.stats(&st);
    CHECK_EQ(st.alloc_failures, 1);
    CHECK_EQ(h.free(a, 0), 0);
    CHECK_EQ(h.free(b, 0), 0);
    uint32_t d = h.alloc(2048); /* recovery after frees */
    CHECK(d != 0);
}

static void test_describe() {
    printf("-- test_describe\n");
    Heap h;
    make_heap(h, 1 << 16);
    uint32_t a = h.alloc(128);
    char buf[256];
    CHECK(h.describe(a + 16, buf, sizeof(buf)));
    CHECK(strstr(buf, "LIVE") != nullptr);
    CHECK(strstr(buf, "off=16") != nullptr);
    CHECK_EQ(h.free(a, 0), 0);
    CHECK(h.describe(a + 16, buf, sizeof(buf)));
    CHECK(strstr(buf, "FREE") != nullptr);
    CHECK(!h.describe(0x1000, buf, sizeof(buf)));
}

static void test_churn() {
    printf("-- test_churn\n");
    Heap h;
    make_heap(h, 1 << 22);
    /* deterministic pseudo-random churn: alloc/free many sizes, verify all
     * surviving patterns after every wave */
    uint32_t s = 0x12345678;
    struct Live {
        uint32_t p;
        size_t n;
        uint8_t seed;
    };
    std::vector<Live> live;
    for (int wave = 0; wave < 200; wave++) {
        for (int i = 0; i < 32; i++) {
            s = s * 1664525u + 1013904223u;
            if ((s >> 16) % 3 == 0 && !live.empty()) {
                size_t idx = (s >> 8) % live.size();
                CHECK_EQ(h.free(live[idx].p, 0), 0);
                live[idx] = live.back();
                live.pop_back();
            } else {
                size_t n = 8 + (s % 512);
                uint32_t p = h.alloc(n);
                if (p) {
                    pattern_write(p, n, (uint8_t)(s & 0xFF));
                    live.push_back({p, n, (uint8_t)(s & 0xFF)});
                }
            }
        }
        for (const Live &l : live) CHECK(pattern_check(l.p, l.n, l.seed));
    }
    HeapStats st;
    h.stats(&st);
    CHECK_EQ(st.bad_frees, 0);
    CHECK_EQ(st.double_frees, 0);
    CHECK_EQ(st.alloc_failures, 0);
}

int main() {
    printf("emu::heap unit tests\n");
    test_basic();
    test_coalesce();
    test_coalesce_backward_only();
    test_fragmentation();
    test_realloc();
    test_invalid_frees();
    test_exhaustion_recovery();
    test_describe();
    test_churn();
    printf("checks=%d failures=%d\n", g_checks, g_failures);
    printf(g_failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return g_failures ? 1 : 0;
}
