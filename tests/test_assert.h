#pragma once

/**
 * @file test_assert.h
 * @brief Minimal, dependency-free test scaffolding.
 *
 * The repository does not vendor a test framework, and pulling one in would
 * change the dependency surface of every build. These macros are deliberately
 * tiny: register cases with CVEDIX_TEST_CASE, run them with
 * cvedix_test::run_all(argc, argv), and let CTest treat a non-zero exit as
 * failure.
 *
 * Usage:
 * @code
 * CVEDIX_TEST_CASE(queue_delivers_in_order) {
 *     CVEDIX_ASSERT_EQ(sink->count(), 10);
 * }
 *
 * int main(int argc, char** argv) { return cvedix_test::run_all(argc, argv); }
 * @endcode
 */

#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace cvedix_test {

/// @brief Thrown by the CVEDIX_ASSERT_* macros; carries the failure location.
class assertion_error : public std::exception {
public:
    explicit assertion_error(std::string message) : message_(std::move(message)) {}
    const char* what() const noexcept override { return message_.c_str(); }

private:
    std::string message_;
};

struct test_case {
    std::string name;
    std::function<void()> body;
};

/// @brief Global registry; populated at static-init time by CVEDIX_TEST_CASE.
inline std::vector<test_case>& registry() {
    static std::vector<test_case> cases;
    return cases;
}

struct registrar {
    registrar(const char* name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

/**
 * @brief Run every registered case, or only those whose name contains argv[1].
 * @return 0 when all selected cases pass, 1 otherwise.
 */
inline int run_all(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";
    int passed = 0;
    std::vector<std::string> failures;

    for (const auto& tc : registry()) {
        if (!filter.empty() && tc.name.find(filter) == std::string::npos) {
            continue;
        }
        std::cout << "[ RUN      ] " << tc.name << std::endl;
        try {
            tc.body();
            ++passed;
            std::cout << "[       OK ] " << tc.name << std::endl;
        }
        catch (const std::exception& e) {
            failures.push_back(tc.name + ": " + e.what());
            std::cout << "[  FAILED  ] " << tc.name << ": " << e.what() << std::endl;
        }
        catch (...) {
            failures.push_back(tc.name + ": unknown exception");
            std::cout << "[  FAILED  ] " << tc.name << ": unknown exception" << std::endl;
        }
    }

    std::cout << "\n" << passed << " passed, " << failures.size() << " failed" << std::endl;
    for (const auto& f : failures) {
        std::cout << "  FAILED: " << f << std::endl;
    }
    return failures.empty() ? 0 : 1;
}

/**
 * @brief Poll @p predicate until it holds or @p timeout elapses.
 *
 * Node processing is asynchronous, so tests must never assert on a counter
 * immediately after pushing work. Polling keeps the tests fast when things work
 * and bounded when they do not.
 */
template <typename Predicate>
bool wait_for(Predicate predicate, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return predicate();
}

}  // namespace cvedix_test

#define CVEDIX_TEST_CASE(name)                                                        \
    static void cvedix_test_body_##name();                                            \
    static ::cvedix_test::registrar cvedix_test_reg_##name(#name, cvedix_test_body_##name); \
    static void cvedix_test_body_##name()

#define CVEDIX_FAIL(msg)                                                              \
    do {                                                                              \
        std::ostringstream cvedix_test_oss;                                           \
        cvedix_test_oss << __FILE__ << ":" << __LINE__ << ": " << msg;                 \
        throw ::cvedix_test::assertion_error(cvedix_test_oss.str());                   \
    } while (false)

#define CVEDIX_ASSERT_TRUE(cond)                                                      \
    do {                                                                              \
        if (!(cond)) {                                                                \
            CVEDIX_FAIL("expected true: " #cond);                                     \
        }                                                                             \
    } while (false)

#define CVEDIX_ASSERT_FALSE(cond)                                                     \
    do {                                                                              \
        if ((cond)) {                                                                 \
            CVEDIX_FAIL("expected false: " #cond);                                    \
        }                                                                             \
    } while (false)

#define CVEDIX_ASSERT_EQ(actual, expected)                                            \
    do {                                                                              \
        const auto cvedix_test_a = (actual);                                          \
        const auto cvedix_test_e = (expected);                                        \
        if (!(cvedix_test_a == cvedix_test_e)) {                                       \
            CVEDIX_FAIL(#actual " == " #expected " -> got " << cvedix_test_a          \
                        << ", want " << cvedix_test_e);                                \
        }                                                                             \
    } while (false)

#define CVEDIX_ASSERT_LE(actual, bound)                                               \
    do {                                                                              \
        const auto cvedix_test_a = (actual);                                          \
        const auto cvedix_test_b = (bound);                                           \
        if (!(cvedix_test_a <= cvedix_test_b)) {                                       \
            CVEDIX_FAIL(#actual " <= " #bound " -> got " << cvedix_test_a             \
                        << ", bound " << cvedix_test_b);                               \
        }                                                                             \
    } while (false)
