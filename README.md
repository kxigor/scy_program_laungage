# Modern C++ Project Template

This repository serves as a robust foundation for C++ projects, configured for the modern **C++23** standard. It features a pre-configured CMake build system, integration with code sanitizers, static analysis, code formatting, and comprehensive testing tools.

## Features at a Glance

* **Standard:** C++23 required.
* **Build System:** Uses `CMakePresets.json` for streamlined, repeatable configuration across different environments.
* **Code Safety (Sanitizers):** Seamless integration with Clang/GCC Sanitizers (Address, Thread, Memory, Undefined).
* **Code Quality:**
  * Strict compiler warnings enforced via `cmake/ProjectOptions.cmake`.
  * Automatic formatting check (`clang-format`).
  * Static analysis (`clang-tidy`).
* **Testing:** Integrated unit testing using Google Test (GTest).
* **Code Coverage:** Integrated LCOV/GCOV generation.

## Prerequisites
To fully utilize this project, you will need the following tools installed:
* **CMake** (version 3.25 or higher)
* **C++ Compiler** with C++23 support (GCC 13+, Clang 16+, or MSVC)
* **Ninja** (Recommended generator, specified in the presets)
* **LLVM Utilities:** `clang-format`, `clang-tidy`
* **Coverage Tools:** `gcov`, `lcov`, and `genhtml` (required for coverage reports).

## Building and Running

The project heavily relies on **CMake Presets** defined in `CMakePresets.json`.

### 1. View Available Presets
To see all available configurations:
```shell
cmake --list-presets
```

### 2. Standard Development and Debugging
The most common scenario is a Debug build with the **Address Sanitizer (ASan)** enabled to catch memory leaks and access violations.

| Configuration | Command | Description |
|:---|:---|:---|
| **Configure** | `cmake --preset dev-debug-asan` | Configures a Debug build with ASan enabled. |
| **Build** | `cmake --build --preset dev-debug-asan` | Builds all targets (app, tests) in the ASan configuration. |
| **Run Executable** | `./build/dev-debug-asan/app/app` | Executes the main application. |

### 3. Using Sanitizer Profiles
The template includes specialized profiles for debugging various issues.

| Preset Name | Sanitizer | Compiler Flags Added | Purpose |
|:---|:---|:---|:---|
| `dev-debug-asan` | Address Sanitizer | `-fsanitize=address` | Memory errors, leaks, use-after-free. |
| `dev-debug-tsan` | Thread Sanitizer | `-fsanitize=thread` | Data races and deadlocks in concurrent code. |
| `dev-debug-msan` | Memory Sanitizer | `-fsanitize=memory` | Checks for uninitialized reads (special setup required). |
| `dev-debug-ubsan` | Undefined Behavior Sanitizer | `-fsanitize=undefined` | Integer overflows, unaligned loads, null pointer references. |

### 4. Release Build (CI / Production)
For maximum performance (no sanitizers, optimized compilation):

| Configuration | Command | Description |
|:---|:---|:---|
| **Configure** | `cmake --preset ci-release` | Configures a Release build (`-O3 -DNDEBUG`). |
| **Build** | `cmake --build --preset ci-release` | Builds all targets with optimizations. |

## Testing and Quality Checks
Tests and quality checks are managed using CTest, allowing them to be run across any configuration profile.

### 1. Running Unit Tests (GTest)
Unit tests are linked against the `project_coverage` target, so they generate coverage data when compiled with the `ci-coverage` preset, and they use the selected sanitizer in `dev-debug-*` presets.

**Run tests in the ASan configuration:**

```shell
# Configure and build first
cmake --preset dev-debug-asan
cmake --build --preset dev-debug-asan

# Run all tests linked to ASan
ctest --preset dev-debug-asan
```

**Running specific tests:**

You can use CTest's regular expression filtering:

```shell
# Run all tests
ctest --preset dev-debug-asan

# Run tests only from the 'Basic' suite
ctest --preset dev-debug-asan -R Basic
```

### 2. Static Analysis and Formatting

Static analysis tools are registered as custom targets or CTest tests within the project.

| QA Target | CTest Command | Custom Build Command | Purpose |
|:---|:---|:---|:---|
| **Format Check** | `ctest --preset <preset> -R FormatCheck` | `cmake --build --preset <preset> --target format` | Runs `clang-format` in check mode. |
| **Tidy Check** | `ctest --preset <preset> -R TidyCheck` | N/A | Runs `clang-tidy` for static analysis. |

### 3. Code Coverage (LCOV/GCOV)

To generate a coverage report, you must use the special **`ci-coverage`** preset, which enables the necessary `--coverage` flags (handled by `cmake/ProjectCoverage.cmake`).

**Step 1: Configure and Build (using coverage flags)**

```shell
cmake --preset ci-coverage
cmake --build --preset ci-coverage
```

**Step 2: Run the dedicated `coverage` target**

This target automatically performs zero-counting, runs CTest to generate `.gcda` files, captures the data into LCOV format, filters system headers, and generates the final HTML report.

```shell
cmake --build --preset ci-coverage --target coverage
```

The final HTML report will be located at: **`build/ci-coverage/coverage_report/index.html`**

## Configuration Details

### CMake Modules and Targets

| Module File | Interface Target | Description |
|:---|:---|:---|
| `ProjectOptions.cmake` | `project_options` | Sets required C++ standard (C++23) and optimization level (`-O3`). |
| `ProjectSanitizers.cmake` | `project_sanitizers` | Applies ASan, TSan, MSan, or UBSan flags conditionally. |
| `ProjectCoverage.cmake` | `project_coverage` | Applies GCC/Clang `--coverage` flags conditionally. |

To apply flags to a target (e.g., `app`), you link against the corresponding interface target:

```cmake
target_link_libraries(app
  PRIVATE
    project_options
    project_sanitizers
    project_coverage
)
```

# GTest Usage Examples: add_gtest function

The examples below illustrate how to use the `add_gtest` function to register Google Test suites within the CMake build system, from the simplest case to the most complex one.

## 1. Simplest Example (Minimal Configuration)

This example is suitable for basic tests that link against a core project library and do not require special options or sanitizers.

### Scenario: Testing a basic math utility.

```cmake
add_gtest(BasicMathTest
  SOURCES
    math_test.cpp
  LIBRARIES
    MyCoreLib::Math
    GTest::gtest_main
)
````

| Argument | Description |
|:---|:---|
| `BasicMathTest` | The name of the test executable and the test name in CTest. |
| `SOURCES` | The single source file for the test executable. |
| `LIBRARIES` | Links against the library under test and the main GTest library. |

-----

## 2\. Standard Example (Including Sanitizers and Coverage)

This is the most common scenario, involving linking against the interface targets `project_options`, `project_sanitizers`, and `project_coverage` to ensure automatic support for all build modes (Debug, ASan, Coverage).

### Scenario: Testing a network connection API component.

```cmake
add_gtest(NetworkAPITests
  SOURCES
    api_test.cpp
    connection_mock.cpp
  LIBRARIES
    MyCoreLib::Networking
    project_options
    project_sanitizers
    project_coverage
  PROPERTIES
    TIMEOUT 90
)
```

| Argument | Description |
|:---|:---|
| `SOURCES` | Multiple files: the test itself and possibly mocks/stubs. |
| `LIBRARIES` | Includes all interface targets for full integration. |
| `PROPERTIES` | Sets a CTest property: execution timeout of 90 seconds. |

-----

## 3\. Advanced Example (Using All Flags)

This example demonstrates the use of all available arguments, useful for tests that require a specific environment, custom compilation flags, or execution in a single thread.

### Scenario: Testing a critical, thread-sensitive code section with extra debugging definitions.

```cmake
add_gtest(CriticalSectionTests
  SOURCES
    lock_free_test.cpp
  LIBRARIES
    MyCoreLib::Concurrency
    project_options
    project_sanitizers
  
  # Options
  EXCLUSIVE
  
  # Values
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/data
  
  # Additional compiler and linker settings
  INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/third_party/atomic_queue/include
  COMPILE_OPTIONS
    -Wno-conversion
  COMPILE_DEFINITIONS
    USE_DEBUG_LOGGING
    LOCK_FREE_ENABLED
  LINK_OPTIONS
    -Wl,--export-dynamic
  
  # Additional CTest properties
  PROPERTIES
    TIMEOUT 180
    FIXTURES_SETUP "Setup_Heavy_System"
    ENVIRONMENT "DEBUG_LEVEL=5;CONFIG_FILE=test.conf"
)

```
| Argument | Description |
|:---|:---|
| `EXCLUSIVE` | Ensures this test runs sequentially (not in parallel), which is critical for TSan tests. |
| `WORKING_DIRECTORY` | Sets the working directory, e.g., to access test data (config files). |
| `INCLUDE_DIRECTORIES` | Adds an external header path (e.g., for a third-party library). |
| `COMPILE_OPTIONS` | Suppresses a specific warning only for this test file. |
| `COMPILE_DEFINITIONS` | Defines macros required for conditional compilation in test mode. |
| `LINK_OPTIONS` | Passes specific flags to the linker. |
| `PROPERTIES` | Additional CTest properties, including setting environment variables (`ENVIRONMENT`) or using test fixtures. |
```
