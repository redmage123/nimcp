/**
 * @file nimcp_recompiler.h
 * @brief Dynamic code recompilation pipeline for self-healing modules
 *
 * WHAT: Compile source code into loadable shared objects at runtime
 * WHY: Enable hot-patching and self-healing of faulty code
 * HOW: Invoke GCC, capture output, verify symbols, sandbox test execution
 *
 * ARCHITECTURE:
 *
 *   Source Code                 Recompiler                   Runtime
 *   ┌─────────────┐            ┌──────────────────┐         ┌─────────────┐
 *   │ Patch       │────────────>│ 1. Write temp.c  │         │ dlopen()    │
 *   │ Source      │            │ 2. gcc compile   │────────>│ Load .so    │
 *   └─────────────┘            │ 3. Verify output │         │             │
 *                              │ 4. nm symbols    │         └─────────────┘
 *   ┌─────────────┐            │ 5. dlopen test   │
 *   │ Source      │────────────>│ 6. Sandbox test  │
 *   │ File        │            └──────────────────┘
 *   └─────────────┘                    │
 *                                      ▼
 *                              ┌──────────────────┐
 *                              │ Compiler Output  │
 *                              │ - Errors         │
 *                              │ - Warnings       │
 *                              │ - Exit code      │
 *                              └──────────────────┘
 *
 * FEATURES:
 * - Compile source files to shared objects
 * - Compile inline patch code strings
 * - Capture compiler errors and warnings
 * - Verify output .so exists and is valid
 * - Symbol verification via nm
 * - Test loading via dlopen
 * - Sandboxed test execution
 * - Timeout protection for compilation
 * - Incremental compilation support
 * - Auto-detect NIMCP include paths and CFLAGS
 *
 * SECURITY CONSIDERATIONS:
 * - Sandbox all test executions
 * - Validate source before compilation
 * - Timeout protection prevents hangs
 * - Temporary files in controlled directory
 *
 * USAGE:
 *   recompiler_config_t config = recompiler_default_config();
 *   config.temp_dir = "/tmp/nimcp_patches";
 *   recompiler_t recompiler = recompiler_create(&config);
 *
 *   // Compile a source file
 *   recompile_request_t req = {0};
 *   strncpy(req.source_file, "patched_function.c", sizeof(req.source_file));
 *   strncpy(req.output_so, "patch.so", sizeof(req.output_so));
 *   req.debug_symbols = true;
 *   req.position_independent = true;
 *
 *   recompile_result_t result = {0};
 *   if (recompiler_compile(recompiler, &req, &result)) {
 *       printf("Compiled successfully: %s\n", result.output_path);
 *       if (recompiler_verify_symbol(result.output_path, "patched_func")) {
 *           printf("Symbol verified\n");
 *       }
 *   } else {
 *       printf("Compile failed: %s\n", result.error_msg);
 *   }
 *
 *   recompiler_destroy(recompiler);
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_RECOMPILER_H
#define NIMCP_RECOMPILER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * EXPORT MACRO
 * ============================================================================ */

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum source file path length */
#define NIMCP_RECOMPILER_MAX_PATH 512

/** Maximum temp directory path length */
#define NIMCP_RECOMPILER_MAX_TEMP_PATH 256

/** Maximum compiler output buffer size */
#define NIMCP_RECOMPILER_MAX_OUTPUT 4096

/** Maximum error message length */
#define NIMCP_RECOMPILER_MAX_ERROR 1024

/** Maximum number of include paths */
#define NIMCP_RECOMPILER_MAX_INCLUDES 32

/** Maximum number of library paths */
#define NIMCP_RECOMPILER_MAX_LIB_PATHS 16

/** Maximum number of libraries to link */
#define NIMCP_RECOMPILER_MAX_LIBS 32

/** Maximum extra CFLAGS length */
#define NIMCP_RECOMPILER_MAX_CFLAGS 512

/** Maximum symbol name length */
#define NIMCP_RECOMPILER_MAX_SYMBOL 256

/** Default compilation timeout (ms) */
#define NIMCP_RECOMPILER_DEFAULT_TIMEOUT_MS 30000

/** Magic value for recompiler validation */
#define NIMCP_RECOMPILER_MAGIC 0x52434D50  /* 'RCMP' */

/** Maximum sandbox output buffer size */
#define NIMCP_SANDBOX_MAX_OUTPUT 8192

/** Maximum test cases per validation */
#define NIMCP_SANDBOX_MAX_TEST_CASES 32

/** Default sandbox timeout (ms) */
#define NIMCP_SANDBOX_DEFAULT_TIMEOUT_MS 5000

/** Default CPU time limit (seconds) */
#define NIMCP_SANDBOX_DEFAULT_CPU_LIMIT_SEC 10

/** Default memory limit (bytes) - 256MB */
#define NIMCP_SANDBOX_DEFAULT_MEMORY_LIMIT (256 * 1024 * 1024)

/** Default file descriptor limit */
#define NIMCP_SANDBOX_DEFAULT_FD_LIMIT 32

/** Default process limit (prevent fork bombs) */
#define NIMCP_SANDBOX_DEFAULT_PROC_LIMIT 4

/* ============================================================================
 * TYPE DEFINITIONS
 * ============================================================================ */

/**
 * @brief Compile request parameters
 *
 * WHAT: Specification for a single compilation job
 * WHY: Encapsulate all compilation options in one structure
 * HOW: Pass to recompiler_compile() or recompiler_compile_patch()
 */
typedef struct {
    char source_file[NIMCP_RECOMPILER_MAX_PATH];    /**< Source file to compile */
    char output_so[NIMCP_RECOMPILER_MAX_PATH];      /**< Output .so path */
    char temp_dir[NIMCP_RECOMPILER_MAX_TEMP_PATH];  /**< Temp directory for intermediates */

    const char** include_paths;     /**< Array of include directories */
    uint32_t include_count;         /**< Number of include paths */

    const char** library_paths;     /**< Array of library directories */
    uint32_t library_count;         /**< Number of library paths */

    const char** libraries;         /**< Array of libraries to link */
    uint32_t lib_count;             /**< Number of libraries */

    const char* extra_cflags;       /**< Additional CFLAGS string */

    bool debug_symbols;             /**< Include debug symbols (-g) */
    bool position_independent;      /**< Position independent code (-fPIC) */
    bool optimize;                  /**< Enable optimization (-O2) */
    bool warnings_as_errors;        /**< Treat warnings as errors (-Werror) */
    bool verbose;                   /**< Verbose output (-v) */

    uint32_t timeout_ms;            /**< Compilation timeout (0 = default) */
} recompile_request_t;

/**
 * @brief Compile result
 *
 * WHAT: Result of a compilation attempt
 * WHY: Provide feedback on success/failure and diagnostics
 */
typedef struct {
    bool success;                   /**< Compilation succeeded */
    int exit_code;                  /**< Compiler exit code */
    char output[NIMCP_RECOMPILER_MAX_OUTPUT];  /**< stdout/stderr combined */
    char error_msg[NIMCP_RECOMPILER_MAX_ERROR]; /**< Human-readable error */
    char output_path[NIMCP_RECOMPILER_MAX_PATH]; /**< Path to output .so */
    uint64_t compile_time_ms;       /**< Time spent compiling */
    uint32_t warning_count;         /**< Number of warnings detected */
    uint32_t error_count;           /**< Number of errors detected */
} recompile_result_t;

/**
 * @brief Recompiler configuration
 *
 * WHAT: Global configuration for recompiler instance
 * WHY: Set up paths, default options
 */
typedef struct {
    char temp_dir[NIMCP_RECOMPILER_MAX_TEMP_PATH]; /**< Default temp directory */
    char compiler_path[NIMCP_RECOMPILER_MAX_PATH]; /**< Path to gcc (NULL = auto) */
    char nm_path[NIMCP_RECOMPILER_MAX_PATH];       /**< Path to nm (NULL = auto) */
    uint32_t default_timeout_ms;    /**< Default compilation timeout */
    bool auto_cleanup;              /**< Auto-cleanup temp files on destroy */
    bool preserve_intermediates;    /**< Keep intermediate files for debugging */
    bool use_ccache;                /**< Use ccache if available */
} recompiler_config_t;

/**
 * @brief Symbol verification result
 */
typedef struct {
    bool found;                     /**< Symbol was found */
    char symbol_type;               /**< Symbol type (T, U, D, etc.) */
    uint64_t address;               /**< Symbol address (if available) */
    bool is_defined;                /**< Symbol is defined (not undefined) */
    bool is_global;                 /**< Symbol has global binding */
} symbol_info_t;

/**
 * @brief Sandbox test function signature
 *
 * @param so_path Path to loaded shared object
 * @param user_data User-provided context
 * @return 0 on success, non-zero on failure
 */
typedef int (*sandbox_test_fn_t)(const char* so_path, void* user_data);

/**
 * @brief Recompiler opaque handle
 */
typedef struct nimcp_recompiler* recompiler_t;

/* ============================================================================
 * ENHANCED SANDBOX TYPES
 * ============================================================================ */

/**
 * @brief Sandbox test outcome
 *
 * WHAT: Result classification for sandbox test execution
 * WHY: Distinguish between pass, fail, crash, timeout
 */
typedef enum {
    SANDBOX_RESULT_PASS = 0,        /**< Test passed successfully */
    SANDBOX_RESULT_FAIL,            /**< Test failed (returned non-zero) */
    SANDBOX_RESULT_CRASH,           /**< Test crashed (signal) */
    SANDBOX_RESULT_TIMEOUT,         /**< Test timed out */
    SANDBOX_RESULT_RESOURCE_LIMIT,  /**< Hit resource limit */
    SANDBOX_RESULT_COMPILE_ERROR,   /**< Compilation failed */
    SANDBOX_RESULT_LOAD_ERROR,      /**< dlopen failed */
    SANDBOX_RESULT_SETUP_ERROR      /**< Sandbox setup failed */
} sandbox_result_t;

/**
 * @brief Test case type for validation
 *
 * WHAT: Classification of generated test cases
 * WHY: Ensure diverse test coverage
 */
typedef enum {
    TEST_CASE_NORMAL = 0,           /**< Normal input values */
    TEST_CASE_BOUNDARY,             /**< Boundary value testing */
    TEST_CASE_NULL_PTR,             /**< NULL pointer testing */
    TEST_CASE_ZERO,                 /**< Zero/empty input */
    TEST_CASE_MAX_VALUE,            /**< Maximum value testing */
    TEST_CASE_NEGATIVE,             /**< Negative value testing */
    TEST_CASE_OVERFLOW,             /**< Potential overflow values */
    TEST_CASE_REGRESSION,           /**< Regression from original crash */
    TEST_CASE_CUSTOM                /**< User-defined test case */
} test_case_type_t;

/**
 * @brief Sandbox resource limits configuration
 *
 * WHAT: Resource constraints for sandboxed execution
 * WHY: Prevent runaway processes, fork bombs, resource exhaustion
 * HOW: Applied via setrlimit() in child process
 */
typedef struct {
    uint32_t cpu_time_sec;          /**< CPU time limit (seconds) */
    size_t memory_bytes;            /**< Address space limit (bytes) */
    uint32_t file_descriptors;      /**< Max open file descriptors */
    uint32_t processes;             /**< Max child processes (fork bomb protection) */
    size_t file_size_bytes;         /**< Max file size that can be created */
    size_t stack_size_bytes;        /**< Stack size limit */
    bool restrict_network;          /**< Disable network access (seccomp) */
    bool restrict_filesystem;       /**< Restrict filesystem access */
    bool use_seccomp;               /**< Enable seccomp filtering */
} sandbox_limits_t;

/**
 * @brief Enhanced sandbox test result
 *
 * WHAT: Detailed result from sandbox test execution
 * WHY: Provide comprehensive feedback for fix validation
 */
typedef struct {
    sandbox_result_t result;        /**< Test outcome classification */
    int exit_code;                  /**< Process exit code (if exited) */
    int signal_number;              /**< Signal number (if crashed) */
    char signal_name[32];           /**< Signal name (e.g., "SIGSEGV") */

    char stdout_output[NIMCP_SANDBOX_MAX_OUTPUT];  /**< Captured stdout */
    char stderr_output[NIMCP_SANDBOX_MAX_OUTPUT];  /**< Captured stderr */
    size_t stdout_len;              /**< Length of stdout */
    size_t stderr_len;              /**< Length of stderr */
    bool output_truncated;          /**< Output was truncated */

    uint64_t execution_time_us;     /**< Execution time in microseconds */
    uint64_t cpu_time_us;           /**< CPU time consumed */
    size_t peak_memory_bytes;       /**< Peak memory usage */

    bool resource_limit_hit;        /**< Hit a resource limit */
    char limit_hit_name[32];        /**< Name of limit that was hit */
} sandbox_test_result_t;

/**
 * @brief Single test case for validation
 *
 * WHAT: Definition of a single test case
 * WHY: Enable systematic testing of fixes
 */
typedef struct {
    test_case_type_t type;          /**< Type of test case */
    char description[256];          /**< Human-readable description */

    void* input_data;               /**< Input data for test */
    size_t input_size;              /**< Size of input data */

    void* expected_output;          /**< Expected output (NULL if any accepted) */
    size_t expected_size;           /**< Size of expected output */

    bool expect_success;            /**< Test should pass/fail */
    int expected_exit_code;         /**< Expected exit code (if specific) */

    uint32_t priority;              /**< Execution priority (lower = higher) */
} test_case_t;

/**
 * @brief Test case collection for validation
 *
 * WHAT: Collection of test cases to run
 * WHY: Validate fix with multiple scenarios
 */
typedef struct {
    test_case_t cases[NIMCP_SANDBOX_MAX_TEST_CASES];  /**< Test cases */
    uint32_t count;                 /**< Number of test cases */
    uint32_t passed;                /**< Tests passed */
    uint32_t failed;                /**< Tests failed */
    uint32_t crashed;               /**< Tests crashed */
    uint32_t timed_out;             /**< Tests timed out */
} test_case_collection_t;

/**
 * @brief Crash context for test generation
 *
 * WHAT: Context from original crash for generating tests
 * WHY: Generate targeted test cases based on crash
 */
typedef struct {
    int crash_signal;               /**< Signal that caused crash */
    void* crash_address;            /**< Address that caused crash */
    char function_name[256];        /**< Function where crash occurred */
    char source_file[NIMCP_RECOMPILER_MAX_PATH];  /**< Source file */
    uint32_t line_number;           /**< Line number of crash */

    char* faulty_code;              /**< Original faulty code snippet */
    size_t faulty_code_len;         /**< Length of faulty code */

    void** parameter_values;        /**< Parameter values at crash */
    uint32_t parameter_count;       /**< Number of parameters */

    uint32_t crash_frequency;       /**< How often this crash occurs */
} crash_context_t;

/**
 * @brief Fix validation result
 *
 * WHAT: Complete result of fix validation pipeline
 * WHY: Determine if fix is safe to deploy
 */
typedef struct {
    bool validation_passed;         /**< Overall validation passed */
    float validation_score;         /**< Score 0.0-1.0 */

    /* Compilation status */
    bool compile_success;           /**< Fix compiled successfully */
    recompile_result_t compile_result;  /**< Compilation details */

    /* Test results */
    test_case_collection_t test_results;  /**< Individual test results */
    uint32_t regression_tests_passed;     /**< Regression tests passed */
    uint32_t regression_tests_total;      /**< Total regression tests */

    /* Original crash reproduction */
    bool original_crash_fixed;      /**< Original crash no longer occurs */
    sandbox_test_result_t crash_test;  /**< Result of crash reproduction test */

    /* Performance impact */
    float performance_ratio;        /**< Ratio vs original (1.0 = same) */

    /* Recommendations */
    bool safe_to_deploy;            /**< Recommended for deployment */
    char notes[1024];               /**< Validation notes/warnings */
} fix_validation_result_t;

/* ============================================================================
 * LIFECYCLE FUNCTIONS
 * ============================================================================ */

/**
 * @brief Create recompiler instance
 *
 * WHAT: Initialize recompiler with configuration
 * WHY: Set up compilation infrastructure
 * HOW: Validate paths, create temp directories
 *
 * @param config Configuration (NULL for defaults)
 * @return Recompiler handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (after creation)
 */
NIMCP_EXPORT recompiler_t recompiler_create(const recompiler_config_t* config);

/**
 * @brief Destroy recompiler instance
 *
 * WHAT: Free all recompiler resources
 * WHY: Clean shutdown
 * HOW: Cleanup temp files (if configured), free memory
 *
 * @param recompiler Recompiler handle
 *
 * COMPLEXITY: O(n) where n = temp files
 * THREAD-SAFE: No (must not have active compilations)
 */
NIMCP_EXPORT void recompiler_destroy(recompiler_t recompiler);

/**
 * @brief Get default configuration
 *
 * @return Default configuration with sensible values
 */
NIMCP_EXPORT recompiler_config_t recompiler_default_config(void);

/* ============================================================================
 * COMPILATION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Compile source file to shared object
 *
 * WHAT: Compile a source file to loadable .so
 * WHY: Create hot-patchable code
 * HOW: Invoke gcc with specified options
 *
 * @param recompiler Recompiler handle
 * @param request Compilation request
 * @param result Output result (must not be NULL)
 * @return true on success
 *
 * COMPLEXITY: O(source_size) + compile time
 * THREAD-SAFE: Yes (can run concurrent compilations)
 *
 * EXAMPLE:
 * ```c
 * recompile_request_t req = {0};
 * strncpy(req.source_file, "patch.c", sizeof(req.source_file));
 * strncpy(req.output_so, "/tmp/patch.so", sizeof(req.output_so));
 * req.position_independent = true;
 * req.debug_symbols = true;
 *
 * recompile_result_t result = {0};
 * if (recompiler_compile(recompiler, &req, &result)) {
 *     printf("Success: %s\n", result.output_path);
 * }
 * ```
 */
NIMCP_EXPORT bool recompiler_compile(
    recompiler_t recompiler,
    const recompile_request_t* request,
    recompile_result_t* result
);

/**
 * @brief Compile patch source code string to shared object
 *
 * WHAT: Compile inline source code to loadable .so
 * WHY: Allow patching without separate source file
 * HOW: Write to temp file, compile, return result
 *
 * @param recompiler Recompiler handle
 * @param source_code Source code string
 * @param fn_name Primary function name (for output naming)
 * @param result Output result
 * @return true on success
 *
 * COMPLEXITY: O(source_len) + compile time
 * THREAD-SAFE: Yes
 *
 * EXAMPLE:
 * ```c
 * const char* patch = "int patched_func(int x) { return x * 2; }";
 * recompile_result_t result = {0};
 * if (recompiler_compile_patch(recompiler, patch, "patched_func", &result)) {
 *     // Load result.output_path via dlopen
 * }
 * ```
 */
NIMCP_EXPORT bool recompiler_compile_patch(
    recompiler_t recompiler,
    const char* source_code,
    const char* fn_name,
    recompile_result_t* result
);

/**
 * @brief Compile with incremental compilation
 *
 * WHAT: Compile only if source newer than output
 * WHY: Skip unnecessary recompilation
 * HOW: Check mtime, skip if output is newer
 *
 * @param recompiler Recompiler handle
 * @param request Compilation request
 * @param result Output result
 * @param was_compiled Set to true if compilation was performed
 * @return true on success (or if skipped)
 *
 * COMPLEXITY: O(1) stat + optional compile
 * THREAD-SAFE: Yes
 */
NIMCP_EXPORT bool recompiler_compile_incremental(
    recompiler_t recompiler,
    const recompile_request_t* request,
    recompile_result_t* result,
    bool* was_compiled
);

/* ============================================================================
 * VERIFICATION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Verify symbol exists in shared object
 *
 * WHAT: Check if symbol is defined in .so file
 * WHY: Ensure patched function is present
 * HOW: Run nm -D on .so file, parse output
 *
 * @param so_path Path to shared object
 * @param symbol_name Symbol to look for
 * @return true if symbol exists
 *
 * COMPLEXITY: O(1) + nm execution
 * THREAD-SAFE: Yes
 */
NIMCP_EXPORT bool recompiler_verify_symbol(
    const char* so_path,
    const char* symbol_name
);

/**
 * @brief Get detailed symbol information
 *
 * WHAT: Get full details about a symbol
 * WHY: Determine symbol type, binding, address
 * HOW: Run nm with verbose output, parse
 *
 * @param so_path Path to shared object
 * @param symbol_name Symbol to look for
 * @param info Output symbol information
 * @return true if symbol found
 */
NIMCP_EXPORT bool recompiler_get_symbol_info(
    const char* so_path,
    const char* symbol_name,
    symbol_info_t* info
);

/**
 * @brief Test load shared object with dlopen
 *
 * WHAT: Verify .so can be loaded
 * WHY: Catch link errors before production use
 * HOW: dlopen with RTLD_NOW, close immediately
 *
 * @param so_path Path to shared object
 * @return true if loadable
 *
 * COMPLEXITY: O(1) + dlopen time
 * THREAD-SAFE: Yes
 */
NIMCP_EXPORT bool recompiler_test_load(const char* so_path);

/**
 * @brief Get load error message
 *
 * WHAT: Get last dlopen/dlerror message
 * WHY: Diagnose load failures
 * HOW: Return dlerror() result
 *
 * @return Error message or NULL
 *
 * @note Thread-local, not safe across threads
 */
NIMCP_EXPORT const char* recompiler_get_load_error(void);

/* ============================================================================
 * SANDBOX TEST FUNCTIONS
 * ============================================================================ */

/**
 * @brief Run test in sandbox before production use
 *
 * WHAT: Execute test function in isolated context
 * WHY: Validate patch works correctly before activation
 * HOW: Fork child process, run test, collect result
 *
 * @param so_path Path to shared object
 * @param test_fn Test function to run
 * @param user_data User context for test function
 * @param timeout_ms Test timeout (0 = default)
 * @return Test function return value, or -1 on sandbox failure
 *
 * COMPLEXITY: O(1) + test execution time
 * THREAD-SAFE: Yes
 *
 * SANDBOX FEATURES:
 * - Runs in child process (fork)
 * - Timeout protection
 * - Signal isolation
 * - Exit code collection
 *
 * EXAMPLE:
 * ```c
 * int my_test(const char* so_path, void* user_data) {
 *     void* handle = dlopen(so_path, RTLD_NOW);
 *     if (!handle) return -1;
 *     int (*func)(int) = dlsym(handle, "patched_func");
 *     int result = func(5);
 *     dlclose(handle);
 *     return (result == 10) ? 0 : -1;  // Expect 5*2 = 10
 * }
 *
 * int status = recompiler_sandbox_test(
 *     result.output_path,
 *     my_test,
 *     NULL,
 *     5000  // 5 second timeout
 * );
 * ```
 */
NIMCP_EXPORT int recompiler_sandbox_test(
    const char* so_path,
    sandbox_test_fn_t test_fn,
    void* user_data,
    uint32_t timeout_ms
);

/* ============================================================================
 * ENHANCED SANDBOX FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get default sandbox resource limits
 *
 * WHAT: Get sensible default limits for sandbox execution
 * WHY: Prevent resource exhaustion while allowing normal operation
 * HOW: Return struct with pre-configured limits
 *
 * @return Default sandbox limits configuration
 */
NIMCP_EXPORT sandbox_limits_t sandbox_default_limits(void);

/**
 * @brief Run enhanced sandbox test with detailed results
 *
 * WHAT: Execute test in isolated sandbox with full result capture
 * WHY: Get detailed feedback on test execution
 * HOW: Fork, apply limits, capture output, collect metrics
 *
 * @param so_path Path to shared object to test
 * @param test_fn Test function to execute
 * @param user_data User context for test function
 * @param limits Resource limits to apply (NULL for defaults)
 * @param timeout_ms Wall-clock timeout (0 = default)
 * @param result Output: detailed test result
 * @return 0 on successful execution (check result->result for test outcome)
 *
 * THREAD-SAFE: Yes
 *
 * EXAMPLE:
 * ```c
 * sandbox_test_result_t result = {0};
 * sandbox_limits_t limits = sandbox_default_limits();
 * limits.memory_bytes = 64 * 1024 * 1024;  // 64MB limit
 *
 * int ret = sandbox_test_enhanced(
 *     "patch.so", my_test, NULL, &limits, 5000, &result
 * );
 *
 * if (result.result == SANDBOX_RESULT_PASS) {
 *     printf("Test passed in %lu us\n", result.execution_time_us);
 * } else if (result.result == SANDBOX_RESULT_CRASH) {
 *     printf("Crashed with %s\n", result.signal_name);
 * }
 * ```
 */
NIMCP_EXPORT int sandbox_test_enhanced(
    const char* so_path,
    sandbox_test_fn_t test_fn,
    void* user_data,
    const sandbox_limits_t* limits,
    uint32_t timeout_ms,
    sandbox_test_result_t* result
);

/**
 * @brief Get signal name from signal number
 *
 * WHAT: Convert signal number to human-readable name
 * WHY: Better diagnostic messages
 * HOW: Static lookup table
 *
 * @param signum Signal number
 * @return Signal name (static string, do not free)
 */
NIMCP_EXPORT const char* sandbox_signal_name(int signum);

/**
 * @brief Get sandbox result name
 *
 * WHAT: Convert result enum to string
 * WHY: Human-readable output
 *
 * @param result Sandbox result enum
 * @return Result name (static string, do not free)
 */
NIMCP_EXPORT const char* sandbox_result_name(sandbox_result_t result);

/* ============================================================================
 * TEST CASE GENERATION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Generate test cases from crash context
 *
 * WHAT: Create test cases based on crash information
 * WHY: Targeted testing of likely failure modes
 * HOW: Analyze crash type, generate boundary/null/overflow tests
 *
 * @param crash_ctx Crash context information
 * @param collection Output: generated test cases
 * @return Number of test cases generated, or -1 on error
 *
 * THREAD-SAFE: Yes
 *
 * EXAMPLE:
 * ```c
 * crash_context_t ctx = {0};
 * ctx.crash_signal = SIGSEGV;
 * ctx.function_name = "process_buffer";
 *
 * test_case_collection_t cases = {0};
 * int count = recompiler_generate_test_cases(&ctx, &cases);
 * printf("Generated %d test cases\n", count);
 * ```
 */
NIMCP_EXPORT int recompiler_generate_test_cases(
    const crash_context_t* crash_ctx,
    test_case_collection_t* collection
);

/**
 * @brief Add custom test case to collection
 *
 * WHAT: Add user-defined test case
 * WHY: Allow domain-specific test cases
 * HOW: Append to collection if space available
 *
 * @param collection Test case collection
 * @param test_case Test case to add
 * @return 0 on success, -1 if collection full
 */
NIMCP_EXPORT int recompiler_add_test_case(
    test_case_collection_t* collection,
    const test_case_t* test_case
);

/**
 * @brief Clear test case collection
 *
 * WHAT: Reset test case collection
 * WHY: Prepare for new test run
 * HOW: Zero out counts, free input data
 *
 * @param collection Collection to clear
 */
NIMCP_EXPORT void recompiler_clear_test_cases(test_case_collection_t* collection);

/**
 * @brief Get test case type name
 *
 * WHAT: Convert test case type to string
 * WHY: Human-readable output
 *
 * @param type Test case type
 * @return Type name (static string)
 */
NIMCP_EXPORT const char* test_case_type_name(test_case_type_t type);

/* ============================================================================
 * FIX VALIDATION PIPELINE
 * ============================================================================ */

/**
 * @brief Validate fix in sandbox
 *
 * WHAT: Complete fix validation pipeline
 * WHY: Verify fix is safe to deploy
 * HOW: Compile, run tests, check regression, score fix
 *
 * @param recompiler Recompiler handle
 * @param source_code Fixed source code
 * @param fn_name Function being patched
 * @param crash_ctx Original crash context
 * @param extra_tests Additional tests to run (optional)
 * @param result Output: validation result
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 *
 * EXAMPLE:
 * ```c
 * fix_validation_result_t result = {0};
 * int ret = recompiler_validate_fix(
 *     recompiler,
 *     fixed_code,
 *     "process_buffer",
 *     &crash_context,
 *     NULL,  // No extra tests
 *     &result
 * );
 *
 * if (result.validation_passed && result.safe_to_deploy) {
 *     printf("Fix validated with score %.2f\n", result.validation_score);
 * }
 * ```
 */
NIMCP_EXPORT int recompiler_validate_fix(
    recompiler_t recompiler,
    const char* source_code,
    const char* fn_name,
    const crash_context_t* crash_ctx,
    const test_case_collection_t* extra_tests,
    fix_validation_result_t* result
);

/**
 * @brief Run test collection in sandbox
 *
 * WHAT: Execute all tests in collection
 * WHY: Batch test execution with aggregated results
 * HOW: Run each test in sandbox, update collection stats
 *
 * @param so_path Path to shared object
 * @param test_fn Test function to execute per case
 * @param collection Test case collection (updated with results)
 * @param limits Resource limits (NULL for defaults)
 * @param timeout_per_test_ms Timeout per test (0 = default)
 * @return Number of tests passed, or -1 on error
 */
NIMCP_EXPORT int recompiler_run_test_collection(
    const char* so_path,
    sandbox_test_fn_t test_fn,
    test_case_collection_t* collection,
    const sandbox_limits_t* limits,
    uint32_t timeout_per_test_ms
);

/**
 * @brief Check if fix addresses original crash
 *
 * WHAT: Verify that the original crash no longer occurs
 * WHY: Primary validation criterion
 * HOW: Reproduce crash conditions, check for crash
 *
 * @param so_path Path to compiled fix
 * @param crash_ctx Original crash context
 * @param result Output: test result
 * @return true if crash is fixed, false if still crashes
 */
NIMCP_EXPORT bool recompiler_verify_crash_fixed(
    const char* so_path,
    const crash_context_t* crash_ctx,
    sandbox_test_result_t* result
);

/**
 * @brief Calculate validation score
 *
 * WHAT: Compute overall validation score
 * WHY: Quantify fix quality
 * HOW: Weight test results, compile warnings, performance
 *
 * @param validation Validation result to score
 * @return Score from 0.0 (poor) to 1.0 (excellent)
 */
NIMCP_EXPORT float recompiler_calculate_validation_score(
    const fix_validation_result_t* validation
);

/* ============================================================================
 * SANDBOX ISOLATION (SECCOMP)
 * ============================================================================ */

/**
 * @brief Check if seccomp is available
 *
 * WHAT: Check if kernel supports seccomp
 * WHY: Seccomp provides stronger isolation
 * HOW: Test prctl(PR_GET_SECCOMP)
 *
 * @return true if seccomp is available
 */
NIMCP_EXPORT bool sandbox_seccomp_available(void);

/**
 * @brief Apply seccomp filter for sandbox
 *
 * WHAT: Install seccomp filter to restrict syscalls
 * WHY: Prevent malicious code from escaping sandbox
 * HOW: Use seccomp-bpf to whitelist safe syscalls
 *
 * @param allow_network Allow network syscalls
 * @param allow_filesystem Allow filesystem beyond temp dir
 * @return 0 on success, -1 on error
 *
 * @note Call this AFTER fork() but BEFORE executing untrusted code
 */
NIMCP_EXPORT int sandbox_apply_seccomp(
    bool allow_network,
    bool allow_filesystem
);

/* ============================================================================
 * NIMCP INTEGRATION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get CFLAGS for NIMCP compilation
 *
 * WHAT: Get compiler flags used to build NIMCP
 * WHY: Ensure patches use compatible flags
 * HOW: Return compiled-in flags or detect from CMake
 *
 * @return CFLAGS string (static, do not free)
 */
NIMCP_EXPORT const char* recompiler_get_nimcp_cflags(void);

/**
 * @brief Get NIMCP include paths
 *
 * WHAT: Get include directories for NIMCP headers
 * WHY: Patches need access to NIMCP types/functions
 * HOW: Return array of include paths
 *
 * @param count Output number of paths
 * @return Array of path strings (static, do not free)
 */
NIMCP_EXPORT const char** recompiler_get_nimcp_includes(uint32_t* count);

/**
 * @brief Get NIMCP library paths
 *
 * WHAT: Get library directories for linking
 * WHY: Patches may need to link against NIMCP
 * HOW: Return array of library paths
 *
 * @param count Output number of paths
 * @return Array of path strings (static, do not free)
 */
NIMCP_EXPORT const char** recompiler_get_nimcp_lib_paths(uint32_t* count);

/**
 * @brief Populate request with NIMCP defaults
 *
 * WHAT: Fill request with NIMCP-compatible options
 * WHY: Convenience for common use case
 * HOW: Set include paths, cflags, etc.
 *
 * @param request Request to populate
 *
 * @note Does not overwrite source_file, output_so
 */
NIMCP_EXPORT void recompiler_set_nimcp_defaults(recompile_request_t* request);

/* ============================================================================
 * CLEANUP FUNCTIONS
 * ============================================================================ */

/**
 * @brief Clean up temporary files
 *
 * WHAT: Remove temp files created during compilation
 * WHY: Free disk space, security
 * HOW: Delete files in temp directory
 *
 * @param recompiler Recompiler handle
 * @return Number of files removed
 *
 * COMPLEXITY: O(n) where n = temp files
 * THREAD-SAFE: No (should not have active compilations)
 */
NIMCP_EXPORT uint32_t recompiler_cleanup_temp(recompiler_t recompiler);

/**
 * @brief Remove specific output file
 *
 * WHAT: Delete a compiled .so file
 * WHY: Cleanup after patch is no longer needed
 * HOW: unlink file
 *
 * @param so_path Path to shared object
 * @return true if removed (or didn't exist)
 */
NIMCP_EXPORT bool recompiler_remove_output(const char* so_path);

/* ============================================================================
 * STATISTICS
 * ============================================================================ */

/**
 * @brief Recompiler statistics
 */
typedef struct {
    uint64_t compilations_total;        /**< Total compilation attempts */
    uint64_t compilations_success;      /**< Successful compilations */
    uint64_t compilations_failed;       /**< Failed compilations */
    uint64_t total_compile_time_ms;     /**< Total compile time */
    uint64_t avg_compile_time_ms;       /**< Average compile time */
    uint64_t symbols_verified;          /**< Symbol verifications */
    uint64_t load_tests;                /**< Load tests performed */
    uint64_t sandbox_tests;             /**< Sandbox tests performed */
    uint64_t temp_files_created;        /**< Temp files created */
    uint64_t temp_files_cleaned;        /**< Temp files cleaned up */
} recompiler_stats_t;

/**
 * @brief Get recompiler statistics
 *
 * @param recompiler Recompiler handle
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool recompiler_get_stats(
    recompiler_t recompiler,
    recompiler_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param recompiler Recompiler handle
 */
NIMCP_EXPORT void recompiler_reset_stats(recompiler_t recompiler);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RECOMPILER_H */
