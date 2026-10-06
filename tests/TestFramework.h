#pragma once

// Minimal dependency-free test harness.

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace gt {

struct TestCase
{
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failures()
{
    static int n = 0;
    return n;
}

struct Registrar
{
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({ name, std::move(fn) }); }
};

inline void fail(const char* file, int line, const std::string& msg)
{
    ++failures();
    std::cerr << "  FAIL " << file << ":" << line << ": " << msg << "\n";
}

} // namespace gt

#define GT_CAT2(a, b) a##b
#define GT_CAT(a, b) GT_CAT2(a, b)
#define TEST(name)                                                              \
    static void GT_CAT(test_fn_, __LINE__)();                                   \
    static gt::Registrar GT_CAT(test_reg_, __LINE__)(name, GT_CAT(test_fn_, __LINE__)); \
    static void GT_CAT(test_fn_, __LINE__)()

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) gt::fail(__FILE__, __LINE__, #cond);                       \
    } while (0)

#define CHECK_MSG(cond, msg)                                                    \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::ostringstream os_;                                             \
            os_ << #cond << " -- " << msg;                                      \
            gt::fail(__FILE__, __LINE__, os_.str());                            \
        }                                                                       \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                   \
    do {                                                                        \
        const double a_ = (a), b_ = (b);                                        \
        if (std::abs(a_ - b_) > (eps)) {                                        \
            std::ostringstream os_;                                             \
            os_ << #a << " = " << a_ << ", " << #b << " = " << b_;              \
            gt::fail(__FILE__, __LINE__, os_.str());                            \
        }                                                                       \
    } while (0)
