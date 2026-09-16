# Tests

First tests for the CVEDIX core runtime. They cover the framework layer that
every pipeline depends on — `cvedix_node`'s queues, its two internal threads, and
the `attach_to()` graph wiring — and deliberately nothing else.

## Why these tests and not others

The nodes that do real work (detectors, trackers, encoders) need models, video
files and often a GPU, so tests for them cannot run in CI. `cvedix_node` needs
none of that: it can be exercised with instrumented subclasses that count what
passes through them. That makes these tests the highest-value ones to have
first — a break here breaks every pipeline in the SDK.

## Running

```bash
cmake -S . -B build -DCVEDIX_BUILD_TESTS=ON
cmake --build build --target test_cvedix_node test_pipeline_smoke -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

`CVEDIX_BUILD_TESTS` defaults to `OFF`, so existing builds are unaffected.

Each binary can also be run directly, optionally filtering by a substring of the
test name:

```bash
./build/tests/test_cvedix_node                       # all cases
./build/tests/test_cvedix_node throwing              # only cases matching "throwing"
```

Exit status is 0 when every selected case passes, 1 otherwise.

## Layout

| File | Purpose |
|---|---|
| `test_assert.h` | Assertion macros, case registration, and `wait_for()`. No external dependency. |
| `test_nodes.h` | Instrumented `cvedix_node` subclasses (`counting_mid_node`, `counting_des_node`, `throwing_mid_node`, `tagging_mid_node`) plus `make_frame()`. |
| `test_cvedix_node.cpp` | Unit tests for the queue and threading contract of the base node. |
| `test_pipeline_smoke.cpp` | End-to-end dataflow through multi-stage, fan-out and fan-in graphs. |

## No test framework

The repository does not vendor GoogleTest or Catch2, and adding one would change
the dependency surface of every build (including the `.deb` package). The
assertions in `test_assert.h` are ~100 lines and cover what these tests need:
`CVEDIX_ASSERT_TRUE/FALSE/EQ/LE`, `CVEDIX_FAIL`, and `CVEDIX_TEST_CASE` for
registration. If the suite grows enough to want fixtures, parameterised cases or
death tests, that is the point to reconsider — behind `FetchContent` and the same
`CVEDIX_BUILD_TESTS` option, so a default build stays dependency-free.

## Writing a new test

```cpp
#include "test_assert.h"
#include "test_nodes.h"

CVEDIX_TEST_CASE(my_new_case) {
    auto mid = std::make_shared<cvedix_test::counting_mid_node>("mid");
    auto des = std::make_shared<cvedix_test::counting_des_node>("des");
    des->attach_to({mid});

    mid->meta_flow(cvedix_test::make_frame(0));

    // Node processing is asynchronous: never assert on a counter immediately
    // after pushing. wait_for() polls until the condition holds or it times out.
    CVEDIX_ASSERT_TRUE(cvedix_test::wait_for([&] { return des->received_count() == 1; }));

    des->stop();
    mid->stop();
}
```

Two rules worth repeating:

- **Always `wait_for()`.** A meta pushed with `meta_flow()` is handled on the
  node's own thread; asserting straight afterwards is a race that passes on a
  fast machine and fails in CI.
- **Always `stop()` in reverse pipeline order** (or let the destructors do it).
  `stop()` calls `deinitialized()`, which joins both threads and is idempotent.
