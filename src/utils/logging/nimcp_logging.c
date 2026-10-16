#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_logging.c - Enhanced Logging System Implementation
//=============================================================================
/**
 * @file nimcp_logging.c
 * @brief High-performance async logging with rotation, filtering, and metrics
 *
 * WHAT: Complete logging implementation with all enhanced features
 * WHY:  Production-ready logging for NIMCP brain modules
 * HOW:  Lock-free ring buffer, background writer, pluggable destinations
 *
 * ARCHITECTURE:
 * - Lock-free SPMC ring buffer for async logging
 * - Background writer thread for file/console/syslog output
 * - Pluggable formatters (text, JSON, syslog)
 * - Automatic rotation with compression
 * - Rate limiting with token bucket algorithm
 * - Thread-local context for correlation IDs
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 2.0.0
 */

#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_cond.h"
#include "utils/platform/nimcp_platform_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  // For strcasecmp
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* P1-42: nimcp_memory.h MUST be at file scope, NOT inside #ifdef blocks.
 * Including inside #ifdef causes implicit declaration -> pointer truncation -> SEGFAULT
 * on non-Linux platforms. */
#include "utils/memory/nimcp_memory.h"

#ifdef __linux__
#include <sys/syscall.h>
#include <syslog.h>
#endif

//=============================================================================
// Early stdout/stderr unbuffering (runs before main)
//=============================================================================

/**
 * WHAT: Force line-buffered stdout/stderr at library load time
 * WHY:  Under nohup / pipe / file redirection, C library switches to full
 *       buffering. fprintf(stderr, ...) output in brain init won't appear
 *       until the 4KB buffer fills. This constructor runs before main().
 */
__attribute__((constructor))
static void _nimcp_unbuffer_stdio(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
}

//=============================================================================
// Forward Declarations for Security and Memory Integration
//=============================================================================

/**
 * WHAT: Forward declarations for security integration
 * WHY:  Avoid header conflicts, use void* for opaque types
 */
typedef struct nimcp_sec_integration nimcp_sec_integration_t;
typedef struct unified_mem_manager_struct* unified_mem_manager_t;
typedef struct unified_mem_handle_struct* unified_mem_handle_t;

#define NIMCP_SEC_CAT_UTILITY 7

extern int nimcp_sec_register_module(
    nimcp_sec_integration_t* ctx,
    const char* name,
    int category,
    uint32_t* module_id
);

extern int nimcp_sec_unregister_module(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id
);

extern int nimcp_sec_record_interaction(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    bool success,
    double weight
);

// Unified memory forward declarations
typedef struct {
    size_t size;
    const void* initial_data;
    int strategy;
    bool enable_cow;
    size_t alignment;
} unified_mem_request_t;

extern unified_mem_handle_t unified_mem_alloc(
    unified_mem_manager_t manager,
    const unified_mem_request_t* request
);

extern void unified_mem_free(unified_mem_handle_t handle);
extern void* unified_mem_write(unified_mem_handle_t handle);

//=============================================================================
// Constants
//=============================================================================

#define LOG_MODULE_NAME "logging"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(logging)

#define DEFAULT_LOG_PATH "/var/log/nimcp/nimcp.log"
#define MAX_MODULE_FILTERS 64
#define CONTEXT_KEY_SIZE 32

/** @brief ANSI color codes for console output */
static const char* ANSI_COLORS[] = {
    "\033[37m",      // TRACE - white
    "\033[36m",      // DEBUG - cyan
    "\033[32m",      // INFO - green
    "\033[33m",      // WARN - yellow
    "\033[31m",      // ERROR - red
    "\033[35;1m",    // FATAL - bold magenta
    ""               // OFF
};
static const char* ANSI_RESET = "\033[0m";

/** @brief Level name strings */
static const char* LEVEL_NAMES[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"
};

/** @brief Short level names for compact format */
static const char* LEVEL_NAMES_SHORT[] = {
    "T", "D", "I", "W", "E", "F", "-"
};

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Module filter entry
 */
typedef struct {
    char module[NIMCP_LOG_MAX_MODULE_LEN];
    log_level_t level;
    bool enabled;
} log_module_filter_t;

/**
 * @brief Rate limiter state (token bucket)
 */
typedef struct {
    nimcp_atomic_uint64_t tokens;
    nimcp_atomic_uint64_t last_refill_time_ns;
    uint32_t max_tokens;
    uint32_t refill_rate;  // tokens per second
    bool enabled;
} log_rate_limiter_t;

/**
 * @brief Ring buffer for async logging
 */
typedef struct {
    nimcp_log_entry_t* entries;
    size_t capacity;
    nimcp_atomic_uint64_t write_pos;
    nimcp_atomic_uint64_t read_pos;
    nimcp_atomic_uint64_t drop_count;
} log_ring_buffer_t;

/**
 * @brief Allocation header for unified memory tracking
 */
typedef struct {
    unified_mem_handle_t handle;
    size_t size;
} log_alloc_header_t;

/**
 * @brief Logger instance structure
 */
struct nimcp_logger_struct {
    uint32_t magic;                     /**< Magic for validation */

    // Configuration
    log_level_t level;
    uint32_t destinations;
    nimcp_log_format_t format;
    nimcp_log_async_mode_t async_mode;
    nimcp_log_color_mode_t color_mode;
    bool include_source_location;

    // File output
    FILE* file;
    char file_path[PATH_MAX];
    nimcp_platform_mutex_t file_mutex;

    // Rotation
    nimcp_log_rotation_config_t rotation;
    size_t current_file_size;
    time_t last_rotation_time;

    // Rate limiting
    log_rate_limiter_t rate_limiter;

    // Module filtering
    log_module_filter_t module_filters[MAX_MODULE_FILTERS];
    size_t module_filter_count;
    nimcp_platform_mutex_t filter_mutex;

    // Callbacks
    nimcp_log_output_callback_t callback;
    void* callback_user_data;
    nimcp_log_filter_t filter;
    void* filter_user_data;

    // Async components
    log_ring_buffer_t ring_buffer;
    nimcp_platform_thread_t writer_thread;
    nimcp_platform_mutex_t async_mutex;
    nimcp_platform_cond_t async_cond;
    nimcp_atomic_bool_t async_running;
    uint32_t flush_interval_ms;

    // Statistics
    nimcp_atomic_uint64_t messages_logged;
    nimcp_atomic_uint64_t messages_dropped;
    nimcp_atomic_uint64_t messages_filtered;
    nimcp_atomic_uint64_t level_counts[LOG_LEVEL_COUNT];
    nimcp_atomic_uint64_t async_writes;
    nimcp_atomic_uint64_t sync_writes;
    nimcp_atomic_uint64_t buffer_overflows;
    nimcp_atomic_uint64_t flush_operations;
    nimcp_atomic_uint64_t rotations_performed;
    nimcp_atomic_uint64_t bytes_written;
    nimcp_atomic_uint64_t rate_limit_hits;
    nimcp_atomic_uint64_t total_log_time_ns;
    nimcp_atomic_uint64_t max_log_time_ns;
    nimcp_atomic_uint32_t sequence;
    uint64_t start_time_ns;

    // Integration
    unified_mem_manager_t memory_mgr;
    nimcp_sec_integration_t* security_ctx;
    uint32_t security_module_id;

    // Context (thread-local simulation via mutex)
    nimcp_platform_mutex_t context_mutex;
    char current_context[NIMCP_LOG_MAX_CONTEXT_LEN];

    // Syslog
    bool syslog_opened;
};

/**
 * @brief Log context structure
 */
struct nimcp_log_context_struct {
    nimcp_logger_t logger;
    char context_id[NIMCP_LOG_MAX_CONTEXT_LEN];
    char previous_context[NIMCP_LOG_MAX_CONTEXT_LEN];
};

//=============================================================================
// Global State
//=============================================================================

static nimcp_logger_t g_global_logger = NULL;
static nimcp_platform_mutex_t g_global_mutex;
static nimcp_platform_once_t g_global_once = NIMCP_PLATFORM_ONCE_INIT;
static bool g_global_initialized = false;


// Forward declarations for static functions (SRP split)
static void init_global_mutex(void);
static void* log_alloc(nimcp_logger_t logger, size_t size);
static void log_free(void* ptr);
static uint32_t get_thread_id(void);
static uint64_t get_time_ns(void);
static uint64_t get_wall_time_ns(void);
static int mkdir_p(const char* path);
static void get_directory(const char* path, char* dir, size_t dir_size);
static const char* get_basename(const char* path);
static bool ring_buffer_init(log_ring_buffer_t* rb, size_t capacity, nimcp_logger_t logger);
static void ring_buffer_destroy(log_ring_buffer_t* rb);
static bool ring_buffer_push(log_ring_buffer_t* rb, const nimcp_log_entry_t* entry);
static bool ring_buffer_pop(log_ring_buffer_t* rb, nimcp_log_entry_t* entry);
static size_t ring_buffer_size(log_ring_buffer_t* rb);
static void rate_limiter_init(log_rate_limiter_t* rl, const nimcp_log_rate_limit_config_t* config);
static bool rate_limiter_try_acquire(log_rate_limiter_t* rl);
static void format_timestamp(uint64_t timestamp_ns, char* buf, size_t buf_size, bool include_ms);
static size_t format_text(const nimcp_log_entry_t* entry, char* buf, size_t buf_size, bool include_source, bool color, bool compact);
static size_t format_json(const nimcp_log_entry_t* entry, char* buf, size_t buf_size);
static size_t format_syslog(const nimcp_log_entry_t* entry, char* buf, size_t buf_size);
static void write_to_file(nimcp_logger_t logger, const nimcp_log_entry_t* entry, const char* formatted, size_t len);
static void write_to_console(nimcp_logger_t logger, const nimcp_log_entry_t* entry, bool use_color);
static void write_to_syslog(nimcp_logger_t logger, const nimcp_log_entry_t* entry);
static void write_to_callback(nimcp_logger_t logger, const nimcp_log_entry_t* entry);
static void write_entry(nimcp_logger_t logger, const nimcp_log_entry_t* entry);
static void* async_writer_thread(void* arg);
static bool should_log(nimcp_logger_t logger, log_level_t level, const char* module);
static void process_entry(nimcp_logger_t logger, nimcp_log_entry_t* entry);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_logging_part_helpers.c"  // 23 functions: helpers
#include "nimcp_logging_part_lifecycle.c"  // 15 functions: lifecycle
#include "nimcp_logging_part_io.c"  // 4 functions: io
#include "nimcp_logging_part_accessors.c"  // 20 functions: accessors
#include "nimcp_logging_part_stats.c"  // 8 functions: stats
#include "nimcp_logging_part_core.c"  // 2 functions: core
