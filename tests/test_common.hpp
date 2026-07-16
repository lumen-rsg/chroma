#pragma once

/// Minimal single-header test harness for Chroma WM domain tests.
/// Provides basic assertion macros, test registration, and a runner.
/// No external dependencies — just <cstdio>, <cstdlib>, and <vector>.

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <vector>
#include <string>
#include <functional>

// ============================================================================
// Test runner (global singleton for simplicity)
// ============================================================================

struct TestRunner {
    struct TestCase {
        std::string name;
        std::function<void()> func;
    };

    std::vector<TestCase> tests;
    int passed{0};
    int failed{0};
    const char* current_test{""};

    void add(const std::string& name, std::function<void()> func) {
        tests.push_back({name, std::move(func)});
    }

    int run() {
        std::printf("\n══════════════════════════════════════════\n");
        std::printf("  Chroma WM — Domain Unit Tests\n");
        std::printf("══════════════════════════════════════════\n\n");

        for (auto& t : tests) {
            current_test = t.name.c_str();
            std::printf("  %-50s", t.name.c_str());
            std::fflush(stdout);
            int before_pass = passed;
            int before_fail = failed;
            t.func();
            if (passed > before_pass) {
                // Pass was recorded inside the test
            }
            if (failed == before_fail && passed == before_pass) {
                // No explicit pass/fail — consider it passed
                passed++;
                std::printf(" PASS\n");
            }
        }

        int total = passed + failed;
        std::printf("\n──────────────────────────────────────────\n");
        std::printf("  %d / %d passed", passed, total);
        if (failed > 0) {
            std::printf("  (%d FAILED)", failed);
        }
        std::printf("\n══════════════════════════════════════════\n\n");
        return failed > 0 ? 1 : 0;
    }
};

inline TestRunner& test_runner() {
    static TestRunner runner;
    return runner;
}

// ============================================================================
// Assertion macros
// ============================================================================

#define TEST(name) \
    static void test_##name(); \
    static struct Register_##name { \
        Register_##name() { test_runner().add(#name, test_##name); } \
    } register_##name; \
    static void test_##name()

#define EXPECT_TRUE(expr) do { \
    if (!(expr)) { \
        std::printf(" FAIL\n    %s:%d: EXPECT_TRUE(%s) failed\n", \
                    __FILE__, __LINE__, #expr); \
        test_runner().failed++; \
        return; \
    } \
} while(0)

#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))

#define EXPECT_EQ(a, b) do { \
    if (!((a) == (b))) { \
        std::printf(" FAIL\n    %s:%d: EXPECT_EQ failed\n      left:  ", \
                    __FILE__, __LINE__); \
        print_val(a); std::printf("\n      right: "); print_val(b); \
        std::printf("\n"); \
        test_runner().failed++; \
        return; \
    } \
} while(0)

#define EXPECT_FLOAT_EQ(a, b, eps) do { \
    if (std::fabs((a) - (b)) > (eps)) { \
        std::printf(" FAIL\n    %s:%d: EXPECT_FLOAT_EQ failed (%f vs %f, eps=%f)\n", \
                    __FILE__, __LINE__, (float)(a), (float)(b), (float)(eps)); \
        test_runner().failed++; \
        return; \
    } \
} while(0)

#define EXPECT_NE(a, b) do { \
    if ((a) == (b)) { \
        std::printf(" FAIL\n    %s:%d: EXPECT_NE failed (both == ", \
                    __FILE__, __LINE__); \
        print_val(a); std::printf(")\n"); \
        test_runner().failed++; \
        return; \
    } \
} while(0)

#define PASS() do { \
    test_runner().passed++; \
    std::printf(" PASS\n"); \
} while(0)

// ============================================================================
// Helper: print values in error messages
// ============================================================================

inline void print_val(int v)           { std::printf("%d", v); }
inline void print_val(unsigned int v)  { std::printf("%u", v); }
inline void print_val(long v)          { std::printf("%ld", v); }
inline void print_val(unsigned long v) { std::printf("%lu", v); }
inline void print_val(float v)         { std::printf("%f", v); }
inline void print_val(double v)        { std::printf("%f", v); }
inline void print_val(bool v)          { std::printf("%s", v ? "true" : "false"); }
inline void print_val(const char* v)   { std::printf("\"%s\"", v); }
inline void print_val(const std::string& v) { std::printf("\"%s\"", v.c_str()); }
// uint64_t and size_t are unsigned long on LP64 — skip to avoid redefinition
