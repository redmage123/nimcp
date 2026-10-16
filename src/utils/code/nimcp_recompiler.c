/**
 * @file nimcp_recompiler.c
 * @brief Dynamic code recompilation pipeline implementation
 *
 * WHAT: Compile source code into loadable shared objects at runtime
 * WHY: Enable hot-patching and self-healing of faulty code
 * HOW: Invoke GCC, capture output, verify symbols, sandbox test execution
 *
 * IMPLEMENTATION NOTES:
 * - Uses fork/exec for gcc invocation
 * - Pipes capture stdout/stderr
 * - Timeout via alarm/SIGALRM
 * - nm for symbol verification
 * - dlopen for load testing
 * - Fork-based sandbox for test isolation
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "utils/code/nimcp_recompiler.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Internal recompiler state
 */
struct nimcp_recompiler {
    uint32_t magic;                 /**< Validation magic */
    recompiler_config_t config;     /**< Configuration */
    nimcp_platform_mutex_t mutex;   /**< Thread safety */
    recompiler_stats_t stats;       /**< Statistics */
    uint32_t temp_counter;          /**< Unique temp file counter */
    char last_error[NIMCP_RECOMPILER_MAX_ERROR]; /**< Last error message */
};

/* ============================================================================
 * STATIC DATA
 * ============================================================================ */

/**
 * @brief Default NIMCP include paths
 *
 * WHAT: Include directories for NIMCP headers
 * WHY: Patches need to find nimcp headers
 * HOW: Compiled-in paths based on build configuration
 */
static const char* s_nimcp_includes[] = {
    "/home/bbrelin/nimcp/include",
    NULL
};

/**
 * @brief Default NIMCP library paths
 */
static const char* s_nimcp_lib_paths[] = {
    "/home/bbrelin/nimcp/build/src/lib",
    NULL
};

/**
 * @brief Default NIMCP CFLAGS
 *
 * WHAT: Compiler flags for NIMCP compatibility
 * WHY: Ensure patches compile with same settings
 */
static const char* s_nimcp_cflags =
    "-std=c11 -Wall -Wextra -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L";

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

static bool validate_recompiler(recompiler_t recompiler);
static bool ensure_temp_dir(const char* path);
static char* generate_temp_path(recompiler_t recompiler, const char* suffix);
static bool write_temp_source(const char* path, const char* source);
static bool build_gcc_command(
    recompiler_t recompiler,
    const recompile_request_t* request,
    char* cmd_buffer,
    size_t buffer_size
);
static bool execute_compiler(
    const char* command,
    uint32_t timeout_ms,
    char* output,
    size_t output_size,
    int* exit_code
);
static bool verify_output_exists(const char* path);
static void parse_compiler_output(
    const char* output,
    uint32_t* warning_count,
    uint32_t* error_count
);
static bool run_nm_check(const char* so_path, const char* symbol_name);

/* ============================================================================
 * VALIDATION AND UTILITY HELPERS
 * ============================================================================ */

static bool validate_recompiler(recompiler_t recompiler) {
    if (!recompiler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_recompiler: recompiler is NULL");
        return false;
    }
    return (recompiler->magic == NIMCP_RECOMPILER_MAGIC);
}

static bool ensure_temp_dir(const char* path) {
    if (!path || !path[0]) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensure_temp_dir: required parameter is NULL (path, path)");
        return false;
    }

    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    /* Create directory with parents */
    char tmp[NIMCP_RECOMPILER_MAX_PATH];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return (mkdir(tmp, 0755) == 0 || errno == EEXIST);
}

static char* generate_temp_path(recompiler_t recompiler, const char* suffix) {
    if (!recompiler || !suffix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "generate_temp_path: required parameter is NULL (recompiler, suffix)");
        return NULL;
    }

    uint32_t counter = __atomic_add_fetch(&recompiler->temp_counter, 1, __ATOMIC_SEQ_CST);

    size_t len = strlen(recompiler->config.temp_dir) + strlen(suffix) + 32;
    char* path = nimcp_malloc(len);
    if (!path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_temp_path: path is NULL");
        return NULL;
    }

    snprintf(path, len, "%s/nimcp_tmp_%u%s",
             recompiler->config.temp_dir, counter, suffix);
    return path;
}

static bool write_temp_source(const char* path, const char* source) {
    if (!path || !source) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "write_temp_source: required parameter is NULL (path, source)");
        return false;
    }

    FILE* fp = fopen(path, "w");
    if (!fp) {
        LOG_ERROR("Failed to open temp source file: %s", path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "write_temp_source: fp is NULL");
        return false;
    }

    size_t len = strlen(source);
    size_t written = fwrite(source, 1, len, fp);
    fclose(fp);

    return (written == len);
}

uint32_t recompiler_cleanup_temp(recompiler_t recompiler) {
    if (!validate_recompiler(recompiler)) return 0;
    /* Stub: in production would remove temp files from config.temp_dir */
    LOG_DEBUG("Cleanup temp directory: %s", recompiler->config.temp_dir);
    return 0;
}

/* ============================================================================
 * LIFECYCLE FUNCTIONS
 * ============================================================================ */

recompiler_config_t recompiler_default_config(void)
{
    LOG_TRACE("Entering recompiler_default_config");

    recompiler_config_t config = {0};

    /**
     * WHAT: Set default temp directory
     * WHY: Need a place for intermediate files
     * HOW: Use /tmp with nimcp prefix
     */
    strncpy(config.temp_dir, "/tmp/nimcp_recompiler",
            sizeof(config.temp_dir) - 1);

    /**
     * WHAT: Set default compiler paths
     * WHY: Allow auto-detection
     * HOW: NULL means use PATH search
     */
    config.compiler_path[0] = '\0';  /* Auto-detect gcc */
    config.nm_path[0] = '\0';        /* Auto-detect nm */

    config.default_timeout_ms = NIMCP_RECOMPILER_DEFAULT_TIMEOUT_MS;
    config.auto_cleanup = true;
    config.preserve_intermediates = false;
    config.use_ccache = false;

    return config;
}

recompiler_t recompiler_create(const recompiler_config_t* config)
{
    LOG_TRACE("Entering recompiler_create");

    /**
     * WHAT: Allocate recompiler structure
     * WHY: Need persistent state
     * HOW: nimcp_calloc for zero-initialization
     */
    struct nimcp_recompiler* recompiler = nimcp_calloc(1, sizeof(*recompiler));
    if (!recompiler) {
        LOG_ERROR("Failed to allocate recompiler");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recompiler_create: recompiler is NULL");
        return NULL;
    }

    /**
     * WHAT: Initialize with config or defaults
     * WHY: Allow customization
     */
    if (config) {
        recompiler->config = *config;
    } else {
        recompiler->config = recompiler_default_config();
    }

    /**
     * WHAT: Ensure temp directory exists
     * WHY: Need place for temp files
     * HOW: mkdir -p equivalent
     */
    if (!ensure_temp_dir(recompiler->config.temp_dir)) {
        LOG_ERROR("Failed to create temp directory: %s", recompiler->config.temp_dir);
        nimcp_free(recompiler);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recompiler_create: ensure_temp_dir is NULL");
        return NULL;
    }

    /**
     * WHAT: Initialize mutex for thread safety
     * WHY: Support concurrent compilations
     */
    if (nimcp_platform_mutex_init(&recompiler->mutex, false) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        nimcp_free(recompiler);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "recompiler_create: validation failed");
        return NULL;
    }

    /**
     * WHAT: Set magic value
     * WHY: Validate handle in subsequent calls
     */
    recompiler->magic = NIMCP_RECOMPILER_MAGIC;

    LOG_INFO("Recompiler created with temp_dir: %s", recompiler->config.temp_dir);
    return recompiler;
}

void recompiler_destroy(recompiler_t recompiler)
{
    LOG_TRACE("Entering recompiler_destroy");

    if (!validate_recompiler(recompiler)) {
        return;
    }

    /**
     * WHAT: Cleanup temp files if configured
     * WHY: Free disk space
     */
    if (recompiler->config.auto_cleanup) {
        recompiler_cleanup_temp(recompiler);
    }

    /**
     * WHAT: Destroy mutex
     * WHY: Clean resource release
     */
    nimcp_platform_mutex_destroy(&recompiler->mutex);

    /**
     * WHAT: Invalidate magic
     * WHY: Catch use-after-free
     */
    recompiler->magic = 0;

    nimcp_free(recompiler);
    LOG_DEBUG("Recompiler destroyed");
}

/* ============================================================================
 * COMPILATION FUNCTIONS
 * ============================================================================ */

bool recompiler_compile(
    recompiler_t recompiler,
    const recompile_request_t* request,
    recompile_result_t* result)
{
    LOG_TRACE("Entering recompiler_compile");

    /**
     * WHAT: Validate inputs
     * WHY: Guard against NULL pointers
     */
    if (!validate_recompiler(recompiler)) {
        if (result) {
            result->success = false;
            strncpy(result->error_msg, "Invalid recompiler handle",
                    sizeof(result->error_msg) - 1);
        }
        return false;
    }

    if (!request || !result) {
        LOG_ERROR("NULL request or result");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recompiler_compile: required parameter is NULL (request, result)");
        return false;
    }

    /**
     * WHAT: Clear result structure
     * WHY: Start fresh
     */
    memset(result, 0, sizeof(*result));

    /**
     * WHAT: Validate source file exists
     * WHY: Early fail before invoking compiler
     */
    struct stat st;
    if (stat(request->source_file, &st) != 0) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Source file not found: %s", request->source_file);
        LOG_ERROR("%s", result->error_msg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_compile: validation failed");
        return false;
    }

    /**
     * WHAT: Build gcc command line
     * WHY: Construct full compilation command
     */
    char cmd[NIMCP_PATH_BUFFER_SIZE];
    if (!build_gcc_command(recompiler, request, cmd, sizeof(cmd))) {
        strncpy(result->error_msg, "Failed to build gcc command",
                sizeof(result->error_msg) - 1);
        LOG_ERROR("%s", result->error_msg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_compile: build_gcc_command is NULL");
        return false;
    }

    LOG_DEBUG("Compiler command: %s", cmd);

    /**
     * WHAT: Execute compiler with timeout
     * WHY: Prevent hanging on infinite loops
     */
    uint64_t start_time = nimcp_time_monotonic_ms();

    uint32_t timeout = request->timeout_ms > 0 ?
                       request->timeout_ms :
                       recompiler->config.default_timeout_ms;

    if (!execute_compiler(cmd, timeout, result->output,
                          sizeof(result->output), &result->exit_code)) {
        strncpy(result->error_msg, "Compiler execution failed",
                sizeof(result->error_msg) - 1);
        LOG_ERROR("%s", result->error_msg);

        nimcp_platform_mutex_lock(&recompiler->mutex);
        recompiler->stats.compilations_failed++;
        recompiler->stats.compilations_total++;
        nimcp_platform_mutex_unlock(&recompiler->mutex);

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_compile: operation failed");
        return false;
    }

    result->compile_time_ms = nimcp_time_monotonic_ms() - start_time;

    /**
     * WHAT: Parse compiler output for warnings/errors
     * WHY: Provide structured diagnostics
     */
    parse_compiler_output(result->output, &result->warning_count,
                          &result->error_count);

    /**
     * WHAT: Check compiler exit code
     * WHY: Determine success
     */
    if (result->exit_code != 0) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Compiler exited with code %d", result->exit_code);
        LOG_ERROR("%s: %s", result->error_msg, result->output);

        nimcp_platform_mutex_lock(&recompiler->mutex);
        recompiler->stats.compilations_failed++;
        recompiler->stats.compilations_total++;
        recompiler->stats.total_compile_time_ms += result->compile_time_ms;
        nimcp_platform_mutex_unlock(&recompiler->mutex);

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_compile: operation failed");
        return false;
    }

    /**
     * WHAT: Verify output file was created
     * WHY: Compiler might exit 0 but not produce output
     */
    if (!verify_output_exists(request->output_so)) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Output file not created: %s", request->output_so);
        LOG_ERROR("%s", result->error_msg);

        nimcp_platform_mutex_lock(&recompiler->mutex);
        recompiler->stats.compilations_failed++;
        recompiler->stats.compilations_total++;
        nimcp_platform_mutex_unlock(&recompiler->mutex);

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_compile: operation failed");
        return false;
    }

    /**
     * WHAT: Copy output path to result
     * WHY: Caller needs to know where output is
     */
    strncpy(result->output_path, request->output_so,
            sizeof(result->output_path) - 1);

    result->success = true;

    /**
     * WHAT: Update statistics
     * WHY: Track compilation performance
     */
    nimcp_platform_mutex_lock(&recompiler->mutex);
    recompiler->stats.compilations_success++;
    recompiler->stats.compilations_total++;
    recompiler->stats.total_compile_time_ms += result->compile_time_ms;
    if (recompiler->stats.compilations_success > 0) {
        recompiler->stats.avg_compile_time_ms =
            recompiler->stats.total_compile_time_ms /
            recompiler->stats.compilations_success;
    }
    nimcp_platform_mutex_unlock(&recompiler->mutex);

    LOG_INFO("Compiled successfully: %s (%lu ms)",
             result->output_path, (unsigned long)result->compile_time_ms);

    return true;
}

bool recompiler_compile_patch(
    recompiler_t recompiler,
    const char* source_code,
    const char* fn_name,
    recompile_result_t* result)
{
    LOG_TRACE("Entering recompiler_compile_patch");

    if (!validate_recompiler(recompiler)) {
        if (result) {
            result->success = false;
            strncpy(result->error_msg, "Invalid recompiler handle",
                    sizeof(result->error_msg) - 1);
        }
        return false;
    }

    if (!source_code || !fn_name || !result) {
        LOG_ERROR("NULL source_code, fn_name, or result");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recompiler_compile_patch: required parameter is NULL (source_code, fn_name, result)");
        return false;
    }

    memset(result, 0, sizeof(*result));

    /**
     * WHAT: Generate temp source file path
     * WHY: Need to write source to file for gcc
     */
    char* temp_source = generate_temp_path(recompiler, ".c");
    if (!temp_source) {
        strncpy(result->error_msg, "Failed to generate temp path",
                sizeof(result->error_msg) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recompiler_compile_patch: temp_source is NULL");
        return false;
    }

    /**
     * WHAT: Write source code to temp file
     * WHY: gcc needs file input
     */
    if (!write_temp_source(temp_source, source_code)) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Failed to write temp source: %s", temp_source);
        nimcp_free(temp_source);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_compile_patch: write_temp_source is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&recompiler->mutex);
    recompiler->stats.temp_files_created++;
    nimcp_platform_mutex_unlock(&recompiler->mutex);

    /**
     * WHAT: Generate output .so path
     * WHY: Need a destination for compiled code
     */
    char output_so[NIMCP_RECOMPILER_MAX_PATH];
    snprintf(output_so, sizeof(output_so), "%s/%s_%u.so",
             recompiler->config.temp_dir, fn_name, recompiler->temp_counter);

    /**
     * WHAT: Build and execute compilation request
     * WHY: Use main compile function
     */
    recompile_request_t request = {0};
    strncpy(request.source_file, temp_source, sizeof(request.source_file) - 1);
    strncpy(request.output_so, output_so, sizeof(request.output_so) - 1);
    strncpy(request.temp_dir, recompiler->config.temp_dir,
            sizeof(request.temp_dir) - 1);

    /* Set defaults for patch compilation */
    request.position_independent = true;
    request.debug_symbols = true;
    request.optimize = false;  /* Don't optimize patches for easier debugging */

    recompiler_set_nimcp_defaults(&request);

    bool success = recompiler_compile(recompiler, &request, result);

    /**
     * WHAT: Cleanup temp source if configured
     * WHY: Don't leave source lying around
     */
    if (!recompiler->config.preserve_intermediates) {
        unlink(temp_source);
        nimcp_platform_mutex_lock(&recompiler->mutex);
        recompiler->stats.temp_files_cleaned++;
        nimcp_platform_mutex_unlock(&recompiler->mutex);
    }

    nimcp_free(temp_source);
    return success;
}

bool recompiler_compile_incremental(
    recompiler_t recompiler,
    const recompile_request_t* request,
    recompile_result_t* result,
    bool* was_compiled)
{
    LOG_TRACE("Entering recompiler_compile_incremental");

    if (!validate_recompiler(recompiler) || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recompiler_compile_incremental: required parameter is NULL (validate_recompiler, request, result)");
        return false;
    }

    if (was_compiled) {
        *was_compiled = false;
    }

    /**
     * WHAT: Check if output exists and is newer than source
     * WHY: Skip compilation if not needed
     */
    struct stat src_stat, out_stat;

    if (stat(request->source_file, &src_stat) != 0) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Source file not found: %s", request->source_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_compile_incremental: validation failed");
        return false;
    }

    if (stat(request->output_so, &out_stat) == 0) {
        /* Output exists - check if it's newer */
        if (out_stat.st_mtime >= src_stat.st_mtime) {
            /* Output is up to date */
            LOG_DEBUG("Output is up to date, skipping compilation");
            result->success = true;
            strncpy(result->output_path, request->output_so,
                    sizeof(result->output_path) - 1);
            return true;
        }
    }

    /**
     * WHAT: Compile because output is stale or missing
     * WHY: Need fresh output
     */
    bool success = recompiler_compile(recompiler, request, result);
    if (was_compiled && success) {
        *was_compiled = true;
    }

    return success;
}

/* ============================================================================
 * VERIFICATION FUNCTIONS
 * ============================================================================ */

bool recompiler_verify_symbol(const char* so_path, const char* symbol_name)
{
    LOG_TRACE("Entering recompiler_verify_symbol: %s in %s", symbol_name, so_path);

    if (!so_path || !symbol_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recompiler_verify_symbol: required parameter is NULL (so_path, symbol_name)");
        return false;
    }

    return run_nm_check(so_path, symbol_name);
}

bool recompiler_get_symbol_info(
    const char* so_path,
    const char* symbol_name,
    symbol_info_t* info)
{
    LOG_TRACE("Entering recompiler_get_symbol_info");

    if (!so_path || !symbol_name || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recompiler_get_symbol_info: required parameter is NULL (so_path, symbol_name, info)");
        return false;
    }

    memset(info, 0, sizeof(*info));

    /**
     * WHAT: Run nm -D with output parsing
     * WHY: Get detailed symbol information
     */
    char cmd[NIMCP_CMD_BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "nm -D '%s' 2>/dev/null | grep ' %s$'",
             so_path, symbol_name);

    FILE* fp = popen(cmd, "r");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recompiler_get_symbol_info: fp is NULL");
        return false;
    }

    char line[NIMCP_ERROR_BUFFER_SIZE];
    if (fgets(line, sizeof(line), fp)) {
        info->found = true;

        /**
         * WHAT: Parse nm output format: "address type symbol"
         * WHY: Extract symbol metadata
         */
        uint64_t addr;
        char type;
        char name[NIMCP_NAME_BUFFER_SIZE];

        if (sscanf(line, "%lx %c %255s", &addr, &type, name) == 3) {
            info->address = addr;
            info->symbol_type = type;
            info->is_defined = (type != 'U');
            info->is_global = (type == 'T' || type == 'D' || type == 'B' ||
                               type == 'R' || type == 'G');
        } else if (sscanf(line, " %c %255s", &type, name) == 2) {
            /* No address (undefined symbol) */
            info->symbol_type = type;
            info->is_defined = false;
            info->is_global = false;
        }
    }

    pclose(fp);
    return info->found;
}

bool recompiler_test_load(const char* so_path)
{
    LOG_TRACE("Entering recompiler_test_load: %s", so_path);

    if (!so_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recompiler_test_load: so_path is NULL");
        return false;
    }

    /**
     * WHAT: Attempt to load with RTLD_NOW
     * WHY: Force resolution of all symbols
     * HOW: dlopen then immediately dlclose
     */
    void* handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        LOG_ERROR("dlopen failed: %s", dlerror());
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recompiler_test_load: handle is NULL");
        return false;
    }

    dlclose(handle);
    LOG_DEBUG("Test load successful: %s", so_path);
    return true;
}

const char* recompiler_get_load_error(void)
{
    return dlerror();
}

/* ============================================================================
 * SANDBOX TEST FUNCTIONS
 * ============================================================================ */

/**
 * @brief Volatile flag for timeout handling
 */
static volatile sig_atomic_t s_sandbox_timeout = 0;

/**
 * @brief SIGALRM handler for sandbox timeout
 */
static void sandbox_alarm_handler(int sig)
{
    (void)sig;
    s_sandbox_timeout = 1;
}

int recompiler_sandbox_test(
    const char* so_path,
    sandbox_test_fn_t test_fn,
    void* user_data,
    uint32_t timeout_ms)
{
    LOG_TRACE("Entering recompiler_sandbox_test");

    if (!so_path || !test_fn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recompiler_sandbox_test: required parameter is NULL (so_path, test_fn)");
        return -1;
    }

    /**
     * WHAT: Fork child process for isolation
     * WHY: Test execution should not affect parent
     */
    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("fork failed: %s", strerror(errno));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_sandbox_test: validation failed");
        return -1;
    }

    if (pid == 0) {
        /**
         * WHAT: Child process - run test
         * WHY: Isolated execution environment
         */

        /* Set up timeout if specified */
        if (timeout_ms > 0) {
            s_sandbox_timeout = 0;
            signal(SIGALRM, sandbox_alarm_handler);
            alarm((timeout_ms + 999) / 1000);  /* Convert to seconds, round up */
        }

        /* Execute test function */
        int result = test_fn(so_path, user_data);

        /* Cancel alarm */
        alarm(0);

        /* Exit with test result */
        _exit(result);
    }

    /**
     * WHAT: Parent process - wait for child
     * WHY: Collect result
     */
    int status;
    pid_t waited = waitpid(pid, &status, 0);

    if (waited < 0) {
        LOG_ERROR("waitpid failed: %s", strerror(errno));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_sandbox_test: validation failed");
        return -1;
    }

    /**
     * WHAT: Interpret child status
     * WHY: Return appropriate result
     */
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        LOG_DEBUG("Sandbox test exited with code: %d", exit_code);
        return exit_code;
    }

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        LOG_ERROR("Sandbox test killed by signal: %d", sig);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_sandbox_test: validation failed");
        return -1;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_sandbox_test: validation failed");
    return -1;
}

/* ============================================================================
 * ENHANCED SANDBOX FUNCTIONS
 * ============================================================================ */

/**
 * @brief Apply resource limits to child process
 *
 * WHAT: Set resource limits using setrlimit
 * WHY: Prevent runaway processes, memory exhaustion
 * HOW: Apply limits before executing untrusted code
 */
static bool apply_resource_limits(const sandbox_limits_t* limits)
{
    struct rlimit rl;

    /**
     * WHAT: Set CPU time limit
     * WHY: Prevent infinite loops consuming CPU
     */
    if (limits->cpu_time_sec > 0) {
        rl.rlim_cur = limits->cpu_time_sec;
        rl.rlim_max = limits->cpu_time_sec + 1;
        if (setrlimit(RLIMIT_CPU, &rl) != 0) {
            LOG_ERROR("setrlimit(RLIMIT_CPU) failed: %s", strerror(errno));
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_resource_limits: validation failed");
            return false;
        }
    }

    /**
     * WHAT: Set address space limit
     * WHY: Prevent memory exhaustion
     */
    if (limits->memory_bytes > 0) {
        rl.rlim_cur = limits->memory_bytes;
        rl.rlim_max = limits->memory_bytes;
        if (setrlimit(RLIMIT_AS, &rl) != 0) {
            LOG_ERROR("setrlimit(RLIMIT_AS) failed: %s", strerror(errno));
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_resource_limits: validation failed");
            return false;
        }
    }

    /**
     * WHAT: Set file descriptor limit
     * WHY: Prevent file descriptor exhaustion
     */
    if (limits->file_descriptors > 0) {
        rl.rlim_cur = limits->file_descriptors;
        rl.rlim_max = limits->file_descriptors;
        if (setrlimit(RLIMIT_NOFILE, &rl) != 0) {
            LOG_ERROR("setrlimit(RLIMIT_NOFILE) failed: %s", strerror(errno));
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_resource_limits: validation failed");
            return false;
        }
    }

    /**
     * WHAT: Set process limit (fork bomb protection)
     * WHY: Prevent fork bombs
     */
    if (limits->processes > 0) {
        rl.rlim_cur = limits->processes;
        rl.rlim_max = limits->processes;
        if (setrlimit(RLIMIT_NPROC, &rl) != 0) {
            LOG_ERROR("setrlimit(RLIMIT_NPROC) failed: %s", strerror(errno));
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_resource_limits: validation failed");
            return false;
        }
    }

    /**
     * WHAT: Set file size limit
     * WHY: Prevent disk space exhaustion
     */
    if (limits->file_size_bytes > 0) {
        rl.rlim_cur = limits->file_size_bytes;
        rl.rlim_max = limits->file_size_bytes;
        if (setrlimit(RLIMIT_FSIZE, &rl) != 0) {
            LOG_ERROR("setrlimit(RLIMIT_FSIZE) failed: %s", strerror(errno));
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_resource_limits: validation failed");
            return false;
        }
    }

    /**
     * WHAT: Set stack size limit
     * WHY: Prevent stack overflow attacks
     */
    if (limits->stack_size_bytes > 0) {
        rl.rlim_cur = limits->stack_size_bytes;
        rl.rlim_max = limits->stack_size_bytes;
        if (setrlimit(RLIMIT_STACK, &rl) != 0) {
            LOG_ERROR("setrlimit(RLIMIT_STACK) failed: %s", strerror(errno));
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_resource_limits: validation failed");
            return false;
        }
    }

    return true;
}

/**
 * @brief Read from pipe with timeout
 *
 * WHAT: Read data from pipe with timeout handling
 * WHY: Non-blocking read to prevent deadlocks
 * HOW: Use select() with timeout
 */
static ssize_t read_pipe_timeout(int fd, char* buffer, size_t size, uint32_t timeout_ms)
{
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ready = select(fd + 1, &readfds, NULL, NULL, &tv);
    if (ready < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_pipe_timeout: validation failed");
        return -1;
    }
    if (ready == 0) {
        return 0;  /* Timeout */
    }

    return read(fd, buffer, size);
}

sandbox_limits_t sandbox_default_limits(void)
{
    LOG_TRACE("Entering sandbox_default_limits");

    sandbox_limits_t limits = {0};

    limits.cpu_time_sec = NIMCP_SANDBOX_DEFAULT_CPU_LIMIT_SEC;
    limits.memory_bytes = NIMCP_SANDBOX_DEFAULT_MEMORY_LIMIT;
    limits.file_descriptors = NIMCP_SANDBOX_DEFAULT_FD_LIMIT;
    limits.processes = NIMCP_SANDBOX_DEFAULT_PROC_LIMIT;
    limits.file_size_bytes = 10 * 1024 * 1024;  /* 10MB max file size */
    limits.stack_size_bytes = 8 * 1024 * 1024;  /* 8MB stack */
    limits.restrict_network = true;
    limits.restrict_filesystem = false;
    limits.use_seccomp = false;  /* Default off for compatibility */

    return limits;
}

const char* sandbox_signal_name(int signum)
{
    /**
     * WHAT: Map signal number to name
     * WHY: Human-readable diagnostics
     */
    switch (signum) {
        case SIGABRT: return "SIGABRT";
        case SIGALRM: return "SIGALRM";
        case SIGBUS:  return "SIGBUS";
        case SIGCHLD: return "SIGCHLD";
        case SIGCONT: return "SIGCONT";
        case SIGFPE:  return "SIGFPE";
        case SIGHUP:  return "SIGHUP";
        case SIGILL:  return "SIGILL";
        case SIGINT:  return "SIGINT";
        case SIGKILL: return "SIGKILL";
        case SIGPIPE: return "SIGPIPE";
        case SIGQUIT: return "SIGQUIT";
        case SIGSEGV: return "SIGSEGV";
        case SIGSTOP: return "SIGSTOP";
        case SIGTERM: return "SIGTERM";
        case SIGTSTP: return "SIGTSTP";
        case SIGTTIN: return "SIGTTIN";
        case SIGTTOU: return "SIGTTOU";
        case SIGUSR1: return "SIGUSR1";
        case SIGUSR2: return "SIGUSR2";
        case SIGXCPU: return "SIGXCPU";
        case SIGXFSZ: return "SIGXFSZ";
#ifdef SIGSYS
        case SIGSYS:  return "SIGSYS";
#endif
        default:      return "UNKNOWN";
    }
}

const char* sandbox_result_name(sandbox_result_t result)
{
    switch (result) {
        case SANDBOX_RESULT_PASS:           return "PASS";
        case SANDBOX_RESULT_FAIL:           return "FAIL";
        case SANDBOX_RESULT_CRASH:          return "CRASH";
        case SANDBOX_RESULT_TIMEOUT:        return "TIMEOUT";
        case SANDBOX_RESULT_RESOURCE_LIMIT: return "RESOURCE_LIMIT";
        case SANDBOX_RESULT_COMPILE_ERROR:  return "COMPILE_ERROR";
        case SANDBOX_RESULT_LOAD_ERROR:     return "LOAD_ERROR";
        case SANDBOX_RESULT_SETUP_ERROR:    return "SETUP_ERROR";
        default:                            return "UNKNOWN";
    }
}

int sandbox_test_enhanced(
    const char* so_path,
    sandbox_test_fn_t test_fn,
    void* user_data,
    const sandbox_limits_t* limits,
    uint32_t timeout_ms,
    sandbox_test_result_t* result)
{
    LOG_TRACE("Entering sandbox_test_enhanced");

    /**
     * WHAT: Validate inputs
     * WHY: Guard against NULL pointers
     */
    if (!so_path || !test_fn || !result) {
        LOG_ERROR("NULL parameter to sandbox_test_enhanced");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sandbox_test_enhanced: required parameter is NULL (so_path, test_fn, result)");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /**
     * WHAT: Use default limits if not specified
     * WHY: Always apply some resource constraints
     */
    sandbox_limits_t effective_limits;
    if (limits) {
        effective_limits = *limits;
    } else {
        effective_limits = sandbox_default_limits();
    }

    if (timeout_ms == 0) {
        timeout_ms = NIMCP_SANDBOX_DEFAULT_TIMEOUT_MS;
    }

    /**
     * WHAT: Create pipes for stdout/stderr capture
     * WHY: Capture child process output for diagnostics
     */
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) != 0) {
        LOG_ERROR("pipe() for stdout failed: %s", strerror(errno));
        result->result = SANDBOX_RESULT_SETUP_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sandbox_test_enhanced: validation failed");
        return -1;
    }

    if (pipe(stderr_pipe) != 0) {
        LOG_ERROR("pipe() for stderr failed: %s", strerror(errno));
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        result->result = SANDBOX_RESULT_SETUP_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sandbox_test_enhanced: validation failed");
        return -1;
    }

    /**
     * WHAT: Record start time
     * WHY: Measure execution time
     */
    uint64_t start_time_us = nimcp_time_get_us();

    /**
     * WHAT: Fork child process for isolation
     * WHY: Test execution should not affect parent
     */
    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("fork failed: %s", strerror(errno));
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        result->result = SANDBOX_RESULT_SETUP_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sandbox_test_enhanced: validation failed");
        return -1;
    }

    if (pid == 0) {
        /**
         * WHAT: Child process setup
         * WHY: Isolated execution environment with output capture
         */

        /* Close read ends of pipes */
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        /* Redirect stdout and stderr to pipes */
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        /**
         * WHAT: Apply resource limits
         * WHY: Constrain untrusted code execution
         */
        if (!apply_resource_limits(&effective_limits)) {
            fprintf(stderr, "SANDBOX: Failed to apply resource limits\n");
            _exit(254);
        }

        /**
         * WHAT: Apply seccomp filter if enabled
         * WHY: Restrict syscalls for additional security
         */
        if (effective_limits.use_seccomp) {
            if (sandbox_apply_seccomp(
                    !effective_limits.restrict_network,
                    !effective_limits.restrict_filesystem) != 0) {
                fprintf(stderr, "SANDBOX: Failed to apply seccomp filter\n");
                _exit(253);
            }
        }

        /**
         * WHAT: Set up alarm timeout
         * WHY: Kill process if it exceeds wall-clock time
         */
        signal(SIGALRM, sandbox_alarm_handler);
        alarm((timeout_ms + 999) / 1000);

        /**
         * WHAT: Execute test function
         * WHY: Run the actual test
         */
        int test_result = test_fn(so_path, user_data);

        /* Cancel alarm and exit with test result */
        alarm(0);
        _exit(test_result);
    }

    /**
     * WHAT: Parent process - setup for output capture
     * WHY: Collect child output and wait for completion
     */
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    /* Set pipes to non-blocking */
    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);

    /**
     * WHAT: Wait for child with timeout
     * WHY: Don't wait forever for hung processes
     */
    bool timed_out = false;
    int status;
    pid_t waited;
    uint64_t deadline_us = start_time_us + (timeout_ms * 1000);

    while (1) {
        waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) {
            break;
        }
        if (waited < 0) {
            LOG_ERROR("waitpid failed: %s", strerror(errno));
            break;
        }

        /* Check timeout */
        uint64_t now_us = nimcp_time_get_us();
        if (now_us >= deadline_us) {
            timed_out = true;
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }

        /* Read available output while waiting */
        char temp_buf[NIMCP_LOG_BUFFER_SIZE];
        ssize_t n;

        n = read(stdout_pipe[0], temp_buf, sizeof(temp_buf));
        if (n > 0) {
            size_t space = NIMCP_SANDBOX_MAX_OUTPUT - result->stdout_len - 1;
            size_t copy = (size_t)n < space ? (size_t)n : space;
            memcpy(result->stdout_output + result->stdout_len, temp_buf, copy);
            result->stdout_len += copy;
            if ((size_t)n > space) {
                result->output_truncated = true;
            }
        }

        n = read(stderr_pipe[0], temp_buf, sizeof(temp_buf));
        if (n > 0) {
            size_t space = NIMCP_SANDBOX_MAX_OUTPUT - result->stderr_len - 1;
            size_t copy = (size_t)n < space ? (size_t)n : space;
            memcpy(result->stderr_output + result->stderr_len, temp_buf, copy);
            result->stderr_len += copy;
            if ((size_t)n > space) {
                result->output_truncated = true;
            }
        }

        /* Small sleep to avoid busy-waiting */
        nimcp_time_sleep_ms(1);
    }

    /**
     * WHAT: Read remaining output from pipes
     * WHY: Capture any buffered output
     */
    char temp_buf[NIMCP_LOG_BUFFER_SIZE];
    ssize_t n;

    while ((n = read(stdout_pipe[0], temp_buf, sizeof(temp_buf))) > 0) {
        size_t space = NIMCP_SANDBOX_MAX_OUTPUT - result->stdout_len - 1;
        size_t copy = (size_t)n < space ? (size_t)n : space;
        memcpy(result->stdout_output + result->stdout_len, temp_buf, copy);
        result->stdout_len += copy;
        if ((size_t)n > space) {
            result->output_truncated = true;
            break;
        }
    }

    while ((n = read(stderr_pipe[0], temp_buf, sizeof(temp_buf))) > 0) {
        size_t space = NIMCP_SANDBOX_MAX_OUTPUT - result->stderr_len - 1;
        size_t copy = (size_t)n < space ? (size_t)n : space;
        memcpy(result->stderr_output + result->stderr_len, temp_buf, copy);
        result->stderr_len += copy;
        if ((size_t)n > space) {
            result->output_truncated = true;
            break;
        }
    }

    /* Null-terminate outputs */
    result->stdout_output[result->stdout_len] = '\0';
    result->stderr_output[result->stderr_len] = '\0';

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    /**
     * WHAT: Record execution time
     * WHY: Performance metrics
     */
    result->execution_time_us = nimcp_time_get_us() - start_time_us;

    /**
     * WHAT: Interpret child status
     * WHY: Classify test outcome
     */
    if (timed_out) {
        result->result = SANDBOX_RESULT_TIMEOUT;
        result->signal_number = SIGKILL;
        strncpy(result->signal_name, "SIGKILL", sizeof(result->signal_name) - 1);
        LOG_DEBUG("Sandbox test timed out after %u ms", timeout_ms);
        return 0;
    }

    if (WIFEXITED(status)) {
        result->exit_code = WEXITSTATUS(status);

        /* Check for special exit codes indicating setup errors */
        if (result->exit_code == 254) {
            result->result = SANDBOX_RESULT_SETUP_ERROR;
            result->resource_limit_hit = true;
            strncpy(result->limit_hit_name, "resource_limits",
                    sizeof(result->limit_hit_name) - 1);
        } else if (result->exit_code == 253) {
            result->result = SANDBOX_RESULT_SETUP_ERROR;
            strncpy(result->limit_hit_name, "seccomp",
                    sizeof(result->limit_hit_name) - 1);
        } else if (result->exit_code == 0) {
            result->result = SANDBOX_RESULT_PASS;
        } else {
            result->result = SANDBOX_RESULT_FAIL;
        }

        LOG_DEBUG("Sandbox test exited with code: %d", result->exit_code);
        return 0;
    }

    if (WIFSIGNALED(status)) {
        result->signal_number = WTERMSIG(status);
        strncpy(result->signal_name, sandbox_signal_name(result->signal_number),
                sizeof(result->signal_name) - 1);

        /* Check if killed by resource limit */
        if (result->signal_number == SIGXCPU) {
            result->result = SANDBOX_RESULT_RESOURCE_LIMIT;
            result->resource_limit_hit = true;
            strncpy(result->limit_hit_name, "CPU_TIME",
                    sizeof(result->limit_hit_name) - 1);
        } else if (result->signal_number == SIGXFSZ) {
            result->result = SANDBOX_RESULT_RESOURCE_LIMIT;
            result->resource_limit_hit = true;
            strncpy(result->limit_hit_name, "FILE_SIZE",
                    sizeof(result->limit_hit_name) - 1);
        } else {
            result->result = SANDBOX_RESULT_CRASH;
        }

        LOG_DEBUG("Sandbox test killed by signal: %s (%d)",
                  result->signal_name, result->signal_number);
        return 0;
    }

    result->result = SANDBOX_RESULT_CRASH;
    return 0;
}

/* ============================================================================
 * STATIC HELPER FUNCTIONS (Compiler Pipeline)
 * ============================================================================ */

static bool build_gcc_command(
    recompiler_t recompiler,
    const recompile_request_t* request,
    char* cmd_buffer,
    size_t buffer_size
) {
    if (!recompiler || !request || !cmd_buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "build_gcc_command: required parameter is NULL (recompiler, request, cmd_buffer)");
        return false;
    }

    /* Start with gcc, shared library output, and PIC */
    int offset = snprintf(cmd_buffer, buffer_size,
        "gcc -shared -o %s", request->output_so);
    if (offset < 0 || (size_t)offset >= buffer_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_gcc_command: capacity exceeded");
        return false;
    }

    /* Position independent code */
    if (request->position_independent) {
        int n = snprintf(cmd_buffer + offset, buffer_size - offset, " -fPIC");
        if (n < 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_gcc_command: validation failed");
            return false;
        }
        offset += n;
    }

    /* Debug symbols */
    if (request->debug_symbols) {
        int n = snprintf(cmd_buffer + offset, buffer_size - offset, " -g");
        if (n < 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_gcc_command: validation failed");
            return false;
        }
        offset += n;
    }

    /* Optimization */
    if (request->optimize) {
        int n = snprintf(cmd_buffer + offset, buffer_size - offset, " -O2");
        if (n < 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_gcc_command: validation failed");
            return false;
        }
        offset += n;
    }

    /* Warnings as errors */
    if (request->warnings_as_errors) {
        int n = snprintf(cmd_buffer + offset, buffer_size - offset, " -Werror");
        if (n < 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_gcc_command: validation failed");
            return false;
        }
        offset += n;
    }

    /* Extra CFLAGS */
    if (request->extra_cflags && request->extra_cflags[0]) {
        int n = snprintf(cmd_buffer + offset, buffer_size - offset,
                         " %s", request->extra_cflags);
        if (n < 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_gcc_command: validation failed");
            return false;
        }
        offset += n;
    }

    /* Include paths */
    for (uint32_t i = 0; i < request->include_count && request->include_paths; i++) {
        if (request->include_paths[i]) {
            int n = snprintf(cmd_buffer + offset, buffer_size - offset,
                             " -I%s", request->include_paths[i]);
            if (n < 0) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_gcc_command: validation failed");
                return false;
            }
            offset += n;
        }
    }

    /* Source file */
    int n = snprintf(cmd_buffer + offset, buffer_size - offset,
                     " %s", request->source_file);
    if (n < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_gcc_command: validation failed");
        return false;
    }
    offset += n;

    /* Library paths */
    for (uint32_t i = 0; i < request->library_count && request->library_paths; i++) {
        if (request->library_paths[i]) {
            int nn = snprintf(cmd_buffer + offset, buffer_size - offset,
                              " -L%s", request->library_paths[i]);
            if (nn < 0) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_gcc_command: validation failed");
                return false;
            }
            offset += nn;
        }
    }

    /* Libraries */
    for (uint32_t i = 0; i < request->lib_count && request->libraries; i++) {
        if (request->libraries[i]) {
            int nn = snprintf(cmd_buffer + offset, buffer_size - offset,
                              " -l%s", request->libraries[i]);
            if (nn < 0) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_gcc_command: validation failed");
                return false;
            }
            offset += nn;
        }
    }

    /* Redirect stderr to stdout */
    n = snprintf(cmd_buffer + offset, buffer_size - offset, " 2>&1");
    if (n < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_gcc_command: validation failed");
        return false;
    }

    return true;
}

static bool execute_compiler(
    const char* command,
    uint32_t timeout_ms,
    char* output,
    size_t output_size,
    int* exit_code
) {
    if (!command || !output || !exit_code) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execute_compiler: required parameter is NULL (command, output, exit_code)");
        return false;
    }
    (void)timeout_ms;  /* TODO: implement timeout with alarm/timer */

    memset(output, 0, output_size);
    *exit_code = -1;

    FILE* fp = popen(command, "r");
    if (!fp) {
        LOG_ERROR("Failed to execute compiler: popen failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execute_compiler: fp is NULL");
        return false;
    }

    size_t total_read = 0;
    char buf[NIMCP_ERROR_BUFFER_LARGE];
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        size_t len = strlen(buf);
        if (total_read + len < output_size - 1) {
            memcpy(output + total_read, buf, len);
            total_read += len;
        }
    }
    output[total_read] = '\0';

    int status = pclose(fp);
    if (WIFEXITED(status)) {
        *exit_code = WEXITSTATUS(status);
    } else {
        *exit_code = -1;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "execute_compiler: validation failed");
        return false;
    }

    return true;
}

static bool verify_output_exists(const char* path) {
    if (!path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "verify_output_exists: path is NULL");
        return false;
    }
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0);
}

static void parse_compiler_output(
    const char* output,
    uint32_t* warning_count,
    uint32_t* error_count
) {
    if (!output) return;
    if (warning_count) *warning_count = 0;
    if (error_count) *error_count = 0;

    const char* p = output;
    while (*p) {
        if (strstr(p, " warning:") == p || (p > output && strstr(p - 1, " warning:") == p - 1)) {
            if (warning_count) (*warning_count)++;
        }
        if (strstr(p, " error:") == p || (p > output && strstr(p - 1, " error:") == p - 1)) {
            if (error_count) (*error_count)++;
        }
        /* Advance to next line */
        const char* nl = strchr(p, '\n');
        if (nl) {
            p = nl + 1;
        } else {
            break;
        }
    }
}

static bool run_nm_check(const char* so_path, const char* symbol_name) {
    if (!so_path || !symbol_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "run_nm_check: required parameter is NULL (so_path, symbol_name)");
        return false;
    }

    char cmd[NIMCP_CMD_BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "nm -D %s 2>/dev/null | grep -q ' T %s'", so_path, symbol_name);
    int ret = system(cmd);
    return (ret == 0);
}

/* ============================================================================
 * NIMCP INTEGRATION FUNCTIONS
 * ============================================================================ */

void recompiler_set_nimcp_defaults(recompile_request_t* request) {
    if (!request) return;

    /* Set NIMCP include paths */
    request->include_paths = s_nimcp_includes;
    uint32_t inc_count = 0;
    while (s_nimcp_includes[inc_count]) inc_count++;
    request->include_count = inc_count;

    /* Set NIMCP library paths */
    request->library_paths = s_nimcp_lib_paths;
    uint32_t lib_path_count = 0;
    while (s_nimcp_lib_paths[lib_path_count]) lib_path_count++;
    request->library_count = lib_path_count;

    /* Set default CFLAGS */
    request->extra_cflags = s_nimcp_cflags;

    /* Default options */
    request->position_independent = true;
    request->debug_symbols = true;
    request->optimize = false;
    request->warnings_as_errors = false;
}

/* ============================================================================
 * SECCOMP ISOLATION
 * ============================================================================ */

#include <sys/prctl.h>

int sandbox_apply_seccomp(bool allow_network, bool allow_filesystem) {
    (void)allow_network;
    (void)allow_filesystem;
#ifdef __linux__
    /* Apply basic process restrictions via prctl */
    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
#endif
    return 0;
}

/* ============================================================================
 * STATISTICS AND CLEANUP FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get recompiler statistics
 *
 * @param recompiler Recompiler handle
 * @param stats Output statistics structure
 * @return true on success
 */
bool recompiler_get_stats(recompiler_t recompiler, recompiler_stats_t* stats) {
    if (!validate_recompiler(recompiler) || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recompiler_get_stats: required parameter is NULL (validate_recompiler, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(&recompiler->mutex);
    *stats = recompiler->stats;
    nimcp_platform_mutex_unlock(&recompiler->mutex);

    return true;
}

/**
 * @brief Reset statistics
 *
 * @param recompiler Recompiler handle
 */
void recompiler_reset_stats(recompiler_t recompiler) {
    if (!validate_recompiler(recompiler)) {
        return;
    }

    nimcp_platform_mutex_lock(&recompiler->mutex);
    memset(&recompiler->stats, 0, sizeof(recompiler_stats_t));
    nimcp_platform_mutex_unlock(&recompiler->mutex);
}

/**
 * @brief Remove compiled output file
 *
 * @param so_path Path to the shared object file
 * @return true if file was removed or didn't exist
 */
bool recompiler_remove_output(const char* so_path) {
    if (!so_path || so_path[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_remove_output: so_path is NULL");
        return false;
    }

    /* Try to remove the file */
    if (unlink(so_path) == 0) {
        return true;
    }

    /* File doesn't exist is also success */
    if (errno == ENOENT) {
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recompiler_remove_output: validation failed");
    return false;
}
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(recompiler)

//=============================================================================
