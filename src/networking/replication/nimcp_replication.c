#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_replication.c - Brain Replication System (Production-Ready)
//=============================================================================
/**
 * @file nimcp_replication.c
 * @brief Distributed brain replication for high availability
 *
 * ARCHITECTURE:
 * - Strategy Pattern: Different backends (Redis, filesystem, PostgreSQL)
 * - Repository Pattern: Abstracted storage operations
 * - Observer Pattern: Nodes notify each other of updates
 * - Factory Pattern: Backend-specific cluster creation
 *
 * WHAT: Synchronizes brain state across multiple Artemis instances
 * WHY: High availability, load balancing, disaster recovery
 * HOW: Backend-agnostic replication with conflict resolution
 *
 * DESIGN PATTERNS:
 * - Strategy Pattern: replication_backend_strategy_t
 * - Repository Pattern: Abstracted get/set operations
 * - Factory Pattern: Backend-specific creation functions
 *
 * COMPLEXITY:
 * - Sync push: O(s) where s = serialized brain size
 * - Sync pull: O(s)
 * - Get status: O(n) where n = number of nodes
 */

#include "networking/replication/nimcp_replication.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_path_traversal.h"
#include "constants/nimcp_timing_constants.h"
#include "utils/platform/nimcp_platform_once.h"

// Global BBB security system — protected by once-init to avoid TOCTOU race
static bbb_system_t g_bbb_system = NULL;
static nimcp_platform_once_t g_bbb_once = NIMCP_PLATFORM_ONCE_INIT;

//=============================================================================
// Security Initialization
//=============================================================================

/* Forward declaration for atexit registration */
static void replication_security_cleanup(void);

/**
 * @brief Internal initialization routine (called exactly once)
 *
 * WHAT: Create and configure BBB system for input validation
 * WHY: Protect against malicious external input and TOCTOU race on g_bbb_system
 * HOW: Called exactly once via nimcp_platform_once — thread-safe initialization
 */
static void replication_security_init_impl(void) {
    bbb_config_t config = bbb_default_config();
    config.strict_mode = false;  // Don't block, just log
    config.default_action = BBB_ACTION_LOG;
    config.input.validate_strings = true;
    config.input.validate_integers = true;
    config.input.max_string_length = 4096;  // Reasonable limit

    g_bbb_system = bbb_system_create(&config);
    if (!g_bbb_system) {
        LOG_ERROR("replication: Failed to initialize security subsystem");
    } else {
        if (atexit(replication_security_cleanup) != 0) {
            LOG_WARN("replication: Failed to register atexit cleanup handler");
        }
        LOG_INFO("replication: Security subsystem initialized");
    }
}

/**
 * @brief Thread-safe security init entry point
 * WHY: nimcp_platform_once guarantees exactly-once execution, no TOCTOU race
 */
static void replication_security_init(void) {
    nimcp_platform_once(&g_bbb_once, replication_security_init_impl);
}

/**
 * @brief Cleanup security subsystem
 * WHY: __attribute__((destructor)) ensures cleanup on library unload
 */
__attribute__((destructor))
static void replication_security_cleanup(void) {
    if (g_bbb_system) {
        bbb_system_destroy(g_bbb_system);
        g_bbb_system = NULL;
    }
}

#define LOG_MODULE "REPLICATION"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(replication)

// Ensure clock_gettime is available
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

//=============================================================================
// Thread-Local Error Handling (Pattern: Thread-Safe Singleton)
//=============================================================================

/**
 * @brief Thread-local error storage
 * WHY: Thread-safe error messages without locks
 * PATTERN: Thread-local storage for error handling
 */
static __thread char g_replication_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

/**
 * @brief Set error message (thread-safe)
 * COMPLEXITY: O(1)
 */
static void set_replication_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_replication_error, sizeof(g_replication_error), format, args);
    va_end(args);
}

/**
 * @brief Get last error message
 * COMPLEXITY: O(1)
 */
const char* replication_get_last_error(void)
{
    return g_replication_error[0] ? g_replication_error : NULL;
}

//=============================================================================
// Forward Declarations - Strategy Pattern
//=============================================================================

typedef struct replication_backend_strategy replication_backend_strategy_t;

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Registered brain entry
 *
 * WHY: Track brains registered with cluster
 * WHAT: Links brain handle to cluster name
 *
 * COMPLEXITY: O(1) access to all fields
 */
typedef struct registered_brain {
    brain_t brain;                  // Brain handle
    char brain_name[NIMCP_ID_BUFFER_SIZE];            // Unique name in cluster
    bool autosync_enabled;          // Auto-sync on updates
    uint64_t version;               // Local version counter
    uint64_t last_sync_version;     // Last synced version
    struct registered_brain* next;  // Linked list next
} registered_brain_t;

/**
 * @brief Cluster internal structure
 *
 * WHY: Encapsulates cluster state and strategy
 * PATTERN: Strategy pattern for backend operations
 *
 * COMPLEXITY: O(1) access to all members
 */
struct replication_cluster_struct {
    replication_config_t config;               // Configuration
    replication_backend_strategy_t* strategy;  // Backend strategy
    void* backend_context;                     // Backend-specific state

    // Registered brains (linked list)
    registered_brain_t* brains;   // Linked list head
    nimcp_mutex_t brains_lock;  // Thread-safe brain list

    // Node tracking
    replication_node_status_t node_status;  // This node's status
    cluster_node_t* known_nodes;            // Array of known nodes
    uint32_t num_known_nodes;               // Number of nodes
    nimcp_mutex_t nodes_lock;             // Thread-safe node list

    // Heartbeat thread
    nimcp_thread_t heartbeat_thread;  // Background heartbeat
    bool heartbeat_running;      // Heartbeat active flag
    nimcp_mutex_t heartbeat_lock;    // Lock for heartbeat condition
    nimcp_cond_t heartbeat_cond;     // Condition to interrupt heartbeat sleep

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

//=============================================================================
// Strategy Pattern - Backend Operations
//=============================================================================

/**
 * @brief Backend strategy interface
 *
 * WHY: Different backends (Redis, filesystem, PostgreSQL) need different ops
 * PATTERN: Strategy pattern - encapsulates algorithm families
 * HOW: Function pointers for backend-specific operations
 *
 * COMPLEXITY: All operations depend on backend implementation
 */
struct replication_backend_strategy {
    replication_backend_t backend_type;

    /**
     * @brief Initialize backend connection
     * WHAT: Connect to Redis/filesystem/database
     * WHY: Establish communication channel
     * COMPLEXITY: O(1) typically
     */
    bool (*initialize)(void** context, const replication_config_t* config);

    /**
     * @brief Shutdown backend connection
     * WHAT: Close connections, cleanup resources
     * WHY: Graceful cleanup
     * COMPLEXITY: O(1)
     */
    void (*shutdown)(void* context);

    /**
     * @brief Store brain state
     * WHAT: Serialize and store brain to backend
     * WHY: Make brain available to other nodes
     * COMPLEXITY: O(s) where s = serialized size
     */
    bool (*store_brain)(void* context, const char* brain_name, const void* data, size_t data_size);

    /**
     * @brief Retrieve brain state
     * WHAT: Load brain state from backend
     * WHY: Get latest state from cluster
     * COMPLEXITY: O(s) where s = serialized size
     */
    bool (*retrieve_brain)(void* context, const char* brain_name, void** data, size_t* data_size);

    /**
     * @brief Get cluster nodes
     * WHAT: Query backend for active nodes
     * WHY: Monitor cluster health
     * COMPLEXITY: O(n) where n = number of nodes
     */
    uint32_t (*get_nodes)(void* context, cluster_node_t* nodes, uint32_t max_nodes);

    /**
     * @brief Send heartbeat
     * WHAT: Announce this node is alive
     * WHY: Cluster health monitoring
     * COMPLEXITY: O(1)
     */
    bool (*heartbeat)(void* context, const char* node_id);
};

//=============================================================================
// Filesystem Backend Strategy (Simple, No Dependencies)
//=============================================================================

/**
 * @brief Filesystem backend context
 *
 * WHAT: Shared directory-based replication
 * WHY: Simple replication without external dependencies
 * HOW: NFS/GlusterFS shared filesystem
 */
typedef struct {
    char shared_dir[NIMCP_METRICS_PATH_SIZE];  // Shared directory path
    char node_id[NIMCP_ID_BUFFER_SIZE];      // This node's ID
} filesystem_context_t;

/**
 * @brief Initialize filesystem backend
 *
 * WHAT: Verify shared directory exists and is writable
 * WHY: Ensure filesystem is ready for replication
 * HOW: Create directories if needed
 *
 * COMPLEXITY: O(1) - just directory operations
 */
static bool filesystem_initialize(void** context, const replication_config_t* config)
{
    // Guard: Validate parameters
    if (!context || !config) {
        set_replication_error("Invalid parameters to filesystem_initialize");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filesystem_initialize: required parameter is NULL (context, config)");
        return false;
    }

    // Create filesystem context
    filesystem_context_t* fs_ctx = nimcp_calloc(1, sizeof(filesystem_context_t));
    if (!fs_ctx) {
        set_replication_error("Failed to allocate filesystem context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "filesystem_initialize: fs_ctx is NULL");
        return false;
    }

    // Parse connection string (format: "file:///path/to/shared/dir")
    const char* path = config->connection_string;
    if (strncmp(path, "file://", 7) == 0) {
        path += 7;  // Skip "file://" prefix
    }

    strncpy(fs_ctx->shared_dir, path, sizeof(fs_ctx->shared_dir) - 1);
    strncpy(fs_ctx->node_id, config->node_id, sizeof(fs_ctx->node_id) - 1);

    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(fs_ctx->shared_dir, &st) == -1) {
        if (mkdir(fs_ctx->shared_dir, 0755) != 0) {
            set_replication_error("Failed to create directory %s: %s", fs_ctx->shared_dir,
                                  strerror(errno));
            nimcp_free(fs_ctx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "filesystem_initialize: validation failed");
            return false;
        }
    }

    /* Create brains/nodes subdirectories. shared_dir and the *_dir
     * destination are both NIMCP_METRICS_PATH_SIZE=512 bytes, so
     * appending a suffix could overflow without capping the source.
     * Longest fixed suffix here is "/brains" (7 bytes + null). Keep
     * a 500-byte cap so any suffix up to 11 bytes fits cleanly. */
    char brains_dir[NIMCP_METRICS_PATH_SIZE];
    snprintf(brains_dir, sizeof(brains_dir), "%.500s/brains", fs_ctx->shared_dir);
    if (stat(brains_dir, &st) == -1) {
        mkdir(brains_dir, 0755);
    }

    // Create nodes subdirectory for heartbeats
    char nodes_dir[NIMCP_METRICS_PATH_SIZE];
    snprintf(nodes_dir, sizeof(nodes_dir), "%.500s/nodes", fs_ctx->shared_dir);
    if (stat(nodes_dir, &st) == -1) {
        mkdir(nodes_dir, 0755);
    }

    *context = fs_ctx;
    return true;
}

/**
 * @brief Shutdown filesystem backend
 *
 * WHAT: Cleanup filesystem context
 * WHY: Release resources
 * HOW: Remove heartbeat file, free context
 *
 * COMPLEXITY: O(1)
 */
static void filesystem_shutdown(void* context)
{
    if (!context)
        return;

    filesystem_context_t* fs_ctx = (filesystem_context_t*) context;

    /* Remove heartbeat file. node_id is up to 64 bytes, so we need
     * shared_dir capped at 512 - 7 (/nodes/) - 64 (id) - 10 (.heartbeat) - 1
     * ≈ 430 to safely fit. Use 420 for margin. */
    char heartbeat_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(heartbeat_path, sizeof(heartbeat_path),
             "%.420s/nodes/%.64s.heartbeat",
             fs_ctx->shared_dir, fs_ctx->node_id);
    remove(heartbeat_path);

    nimcp_free(fs_ctx);
}

/**
 * @brief Store brain to filesystem
 *
 * WHAT: Write brain data to shared directory
 * WHY: Make brain available to other nodes
 * HOW: Atomic write (tmp file + rename)
 *
 * COMPLEXITY: O(s) where s = data size
 */
static bool filesystem_store_brain(void* context, const char* brain_name, const void* data,
                                   size_t data_size)
{
    // Guard: Validate parameters
    if (!context || !brain_name || !data || data_size == 0) {
        set_replication_error("Invalid parameters to filesystem_store_brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filesystem_store_brain: required parameter is NULL (context, brain_name, data)");
        return false;
    }

    filesystem_context_t* fs_ctx = (filesystem_context_t*) context;

    /* Create brain file path. shared_dir[512] + "/brains/" (8) +
     * brain_name (up to ~128) + ".nimcp" (6) must fit in 512. Cap
     * shared_dir to 350, brain_name to 128, leaving safe headroom.
     * tmp_path appends ".tmp" (4) — also cap brain_path in that copy. */
    char brain_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(brain_path, sizeof(brain_path), "%.350s/brains/%.128s.nimcp",
             fs_ctx->shared_dir, brain_name);

    // P1-3 fix: Path traversal validation
    if (!nimcp_path_is_safe(brain_path)) {
        set_replication_error("Path validation failed: %s", brain_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "filesystem_store_brain: nimcp_path_is_safe is NULL");
        return false;
    }

    // Write to temporary file first (atomic operation)
    char tmp_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(tmp_path, sizeof(tmp_path), "%.500s.tmp", brain_path);

    // P1-3 fix: Path traversal validation for temp path
    if (!nimcp_path_is_safe(tmp_path)) {
        set_replication_error("Temp path validation failed: %s", tmp_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "filesystem_store_brain: nimcp_path_is_safe is NULL");
        return false;
    }

    FILE* f = fopen(tmp_path, "wb");
    if (!f) {
        set_replication_error("Failed to open %s: %s", tmp_path, strerror(errno));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filesystem_store_brain: f is NULL");
        return false;
    }

    // Write data
    size_t written = fwrite(data, 1, data_size, f);
    fclose(f);

    if (written != data_size) {
        set_replication_error("Failed to write brain data: only %zu/%zu bytes written", written,
                              data_size);
        remove(tmp_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "filesystem_store_brain: validation failed");
        return false;
    }

    // Atomic rename
    if (rename(tmp_path, brain_path) != 0) {
        set_replication_error("Failed to rename %s to %s: %s", tmp_path, brain_path,
                              strerror(errno));
        remove(tmp_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "filesystem_store_brain: validation failed");
        return false;
    }

    return true;
}

/**
 * @brief Retrieve brain from filesystem
 *
 * WHAT: Read brain data from shared directory
 * WHY: Load latest brain state
 * HOW: Read file, allocate buffer, return data
 *
 * COMPLEXITY: O(s) where s = file size
 */
static bool filesystem_retrieve_brain(void* context, const char* brain_name, void** data,
                                      size_t* data_size)
{
    // Guard: Validate parameters
    if (!context || !brain_name || !data || !data_size) {
        set_replication_error("Invalid parameters to filesystem_retrieve_brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filesystem_retrieve_brain: required parameter is NULL (context, brain_name, data, data_size)");
        return false;
    }

    filesystem_context_t* fs_ctx = (filesystem_context_t*) context;

    /* Create brain file path (retrieve). Same cap as filesystem_store_brain
     * above: shared_dir ≤ 350, brain_name ≤ 128, fits in 512 with suffix. */
    char brain_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(brain_path, sizeof(brain_path), "%.350s/brains/%.128s.nimcp",
             fs_ctx->shared_dir, brain_name);

    // P1-3 fix: Path traversal validation
    if (!nimcp_path_is_safe(brain_path)) {
        set_replication_error("Path validation failed: %s", brain_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "filesystem_retrieve_brain: nimcp_path_is_safe is NULL");
        return false;
    }

    // Open file
    FILE* f = fopen(brain_path, "rb");
    if (!f) {
        set_replication_error("Failed to open %s: %s", brain_path, strerror(errno));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filesystem_retrieve_brain: f is NULL");
        return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        set_replication_error("Invalid file size: %ld", file_size);
        fclose(f);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "filesystem_retrieve_brain: validation failed");
        return false;
    }

    // Allocate buffer
    void* buffer = nimcp_malloc(file_size);
    if (!buffer) {
        set_replication_error("Failed to allocate %ld bytes", file_size);
        fclose(f);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "filesystem_retrieve_brain: buffer is NULL");
        return false;
    }

    // Read data
    size_t bytes_read = fread(buffer, 1, file_size, f);
    fclose(f);

    if (bytes_read != (size_t) file_size) {
        set_replication_error("Failed to read brain: only %zu/%ld bytes read", bytes_read,
                              file_size);
        nimcp_free(buffer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "filesystem_retrieve_brain: validation failed");
        return false;
    }

    *data = buffer;
    *data_size = file_size;
    return true;
}

/**
 * @brief Get nodes from filesystem
 *
 * WHAT: Scan nodes directory for heartbeat files
 * WHY: Discover active nodes in cluster
 * HOW: Read directory, check file timestamps
 *
 * COMPLEXITY: O(n) where n = number of nodes
 */
static uint32_t filesystem_get_nodes(void* context, cluster_node_t* nodes, uint32_t max_nodes)
{
    if (!context || !nodes || max_nodes == 0)
        return 0;

    filesystem_context_t* fs_ctx = (filesystem_context_t*) context;

    char nodes_dir[NIMCP_METRICS_PATH_SIZE];
    snprintf(nodes_dir, sizeof(nodes_dir), "%.500s/nodes", fs_ctx->shared_dir);

    // Open directory
    DIR* dir = opendir(nodes_dir);
    if (!dir)
        return 0;

    uint32_t count = 0;
    struct dirent* entry;
    time_t now = nimcp_time_get_sec();

    // Scan for .heartbeat files
    while ((entry = readdir(dir)) != NULL && count < max_nodes) {

    // BBB: Validate external input
    // WHAT: Check input for security threats before processing
    // WHY: Prevent injection attacks and buffer overflows
    // TODO: Customize validation for specific input parameters
    /*
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_input(system, data, size, &val_result)) {
        LOG_ERROR("Input validation failed in while: %s", val_result.reason);
        return NIMCP_ERROR_INVALID_INPUT;
    }
    */
        // Skip non-heartbeat files
        if (!strstr(entry->d_name, ".heartbeat"))
            continue;

        /* Get file stats. dirent.d_name is up to 256 bytes (NAME_MAX),
         * so cap nodes_dir to leave room. */
        char heartbeat_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(heartbeat_path, sizeof(heartbeat_path), "%.250s/%.256s",
                 nodes_dir, entry->d_name);

        struct stat st;
        if (stat(heartbeat_path, &st) != 0)
            continue;

        // Extract node ID from filename (remove .heartbeat suffix)
        char node_id[NIMCP_ID_BUFFER_SIZE];
        strncpy(node_id, entry->d_name, sizeof(node_id) - 1);
        char* dot = strstr(node_id, ".heartbeat");
        if (dot)
            *dot = '\0';

        // Check if node is alive (heartbeat within timeout)
        time_t age = now - st.st_mtime;
        bool is_alive = (age < 60);  // 60 second timeout

        // Fill node info
        strncpy(nodes[count].node_id, node_id, sizeof(nodes[count].node_id) - 1);
        nodes[count].status = is_alive ? NODE_STATUS_FOLLOWER : NODE_STATUS_DEAD;
        nodes[count].last_heartbeat = st.st_mtime;
        nodes[count].lag_ms = age * NIMCP_MS_PER_SEC;  // Convert seconds to ms
        nodes[count].version = 0;          // Filesystem doesn't track versions

        count++;
    }

    closedir(dir);
    return count;
}

/**
 * @brief Send heartbeat to filesystem
 *
 * WHAT: Touch heartbeat file to update timestamp
 * WHY: Signal this node is alive
 * HOW: Create/update file in nodes directory
 *
 * COMPLEXITY: O(1)
 */
static bool filesystem_heartbeat(void* context, const char* node_id)
{
    if (!context || !node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filesystem_heartbeat: required parameter is NULL (context, node_id)");
        return false;
    }

    filesystem_context_t* fs_ctx = (filesystem_context_t*) context;

    /* Heartbeat path: shared_dir ≤ 420, node_id ≤ 64 (NIMCP_ID_BUFFER_SIZE),
     * "/nodes/" = 7, ".heartbeat" = 10. Total ≤ 502, fits in 512. */
    char heartbeat_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(heartbeat_path, sizeof(heartbeat_path),
             "%.420s/nodes/%.64s.heartbeat",
             fs_ctx->shared_dir, node_id);

    // P1-3 fix: Path traversal validation
    if (!nimcp_path_is_safe(heartbeat_path)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "filesystem_heartbeat: nimcp_path_is_safe is NULL");
        return false;
    }

    // Touch file (create or update timestamp)
    FILE* f = fopen(heartbeat_path, "w");
    if (!f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filesystem_heartbeat: f is NULL");
        return false;
    }

    fprintf(f, "%ld\n", (long) nimcp_time_get_sec());
    fclose(f);

    return true;
}

/**
 * @brief Filesystem backend strategy
 *
 * WHAT: Strategy implementation for filesystem backend
 * WHY: Encapsulates filesystem-specific operations
 * PATTERN: Strategy pattern
 */
static replication_backend_strategy_t g_filesystem_strategy = {
    .backend_type = REPLICATION_BACKEND_FILESYSTEM,
    .initialize = filesystem_initialize,
    .shutdown = filesystem_shutdown,
    .store_brain = filesystem_store_brain,
    .retrieve_brain = filesystem_retrieve_brain,
    .get_nodes = filesystem_get_nodes,
    .heartbeat = filesystem_heartbeat};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Find registered brain by name
 *
 * WHAT: Search linked list for brain by name
 * WHY: Lookup brain for sync operations
 * HOW: Linear search through linked list
 *
 * COMPLEXITY: O(n) where n = number of registered brains
 */
static registered_brain_t* find_brain(replication_cluster_t cluster, const char* brain_name)
{
    if (!cluster || !brain_name) {
        return NULL;
    }

    nimcp_mutex_lock(&cluster->brains_lock);

    registered_brain_t* current = cluster->brains;
    while (current) {
        if (strcmp(current->brain_name, brain_name) == 0) {
            nimcp_mutex_unlock(&cluster->brains_lock);
            return current;
        }
        current = current->next;
    }

    nimcp_mutex_unlock(&cluster->brains_lock);
    return NULL;  /* Not found - normal search behavior */
}

/**
 * @brief Heartbeat thread function
 *
 * WHAT: Background thread that sends periodic heartbeats
 * WHY: Keep cluster informed this node is alive
 * HOW: Sleep, send heartbeat, repeat
 *
 * COMPLEXITY: O(1) per iteration
 */
static void* heartbeat_thread_fn(void* arg)
{
    replication_cluster_t cluster = (replication_cluster_t) arg;

    while (cluster->heartbeat_running) {
        // Send heartbeat
        cluster->strategy->heartbeat(cluster->backend_context, cluster->config.node_id);

        // Sleep for heartbeat interval using condition variable (can be interrupted)
        nimcp_mutex_lock(&cluster->heartbeat_lock);
        if (cluster->heartbeat_running) {
            // nimcp_cond_timedwait expects timeout in milliseconds, not absolute timespec
            nimcp_cond_timedwait(&cluster->heartbeat_cond, &cluster->heartbeat_lock,
                                 cluster->config.heartbeat_interval_ms);
        }
        nimcp_mutex_unlock(&cluster->heartbeat_lock);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "heartbeat_thread_fn: validation failed");
    return NULL;
}

//=============================================================================
// Public API Implementation
//=============================================================================

/**
 * @brief Create replication cluster
 *
 * WHAT: Initialize cluster with specified backend
 * WHY: Entry point for replication setup
 * HOW: Factory pattern - create appropriate backend
 *
 * PATTERN: Factory pattern for backend creation
 * COMPLEXITY: O(1) plus backend initialization
 */
replication_cluster_t replication_create_cluster(const replication_config_t* config)
{
    // Guard: Validate config
    if (!config) {
        set_replication_error("Null configuration provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    // Allocate cluster structure
    replication_cluster_t cluster = nimcp_calloc(1, sizeof(struct replication_cluster_struct));
    if (!cluster) {
        set_replication_error("Failed to allocate cluster structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cluster is NULL");

        return NULL;
    }

    // Copy configuration
    memcpy(&cluster->config, config, sizeof(replication_config_t));

    // Initialize mutexes and condition variables
    nimcp_mutex_init(&cluster->brains_lock, NULL);
    nimcp_mutex_init(&cluster->nodes_lock, NULL);
    nimcp_mutex_init(&cluster->heartbeat_lock, NULL);
    nimcp_cond_init(&cluster->heartbeat_cond);

    // Select strategy based on backend type
    switch (config->backend) {
        case REPLICATION_BACKEND_FILESYSTEM:
            cluster->strategy = &g_filesystem_strategy;
            break;

        case REPLICATION_BACKEND_REDIS:
            // TODO: Implement Redis strategy
            set_replication_error("Redis backend not yet implemented");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_IMPLEMENTED,
                "replication_create_cluster: Redis backend not yet implemented");
            nimcp_cond_destroy(&cluster->heartbeat_cond);
            nimcp_mutex_destroy(&cluster->heartbeat_lock);
            nimcp_mutex_destroy(&cluster->brains_lock);
            nimcp_mutex_destroy(&cluster->nodes_lock);
            nimcp_free(cluster);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_create_cluster: operation failed");
            return NULL;

        case REPLICATION_BACKEND_POSTGRES:
            // TODO: Implement PostgreSQL strategy
            set_replication_error("PostgreSQL backend not yet implemented");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_IMPLEMENTED,
                "replication_create_cluster: PostgreSQL backend not yet implemented");
            nimcp_cond_destroy(&cluster->heartbeat_cond);
            nimcp_mutex_destroy(&cluster->heartbeat_lock);
            nimcp_mutex_destroy(&cluster->brains_lock);
            nimcp_mutex_destroy(&cluster->nodes_lock);
            nimcp_free(cluster);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_create_cluster: operation failed");
            return NULL;

        default:
            set_replication_error("Unknown backend type: %d", config->backend);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "replication_create_cluster: unknown backend type %d", config->backend);
            nimcp_cond_destroy(&cluster->heartbeat_cond);
            nimcp_mutex_destroy(&cluster->heartbeat_lock);
            nimcp_mutex_destroy(&cluster->brains_lock);
            nimcp_mutex_destroy(&cluster->nodes_lock);
            nimcp_free(cluster);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_create_cluster: operation failed");
            return NULL;
    }

    // Initialize backend
    if (!cluster->strategy->initialize(&cluster->backend_context, config)) {
        nimcp_cond_destroy(&cluster->heartbeat_cond);
        nimcp_mutex_destroy(&cluster->heartbeat_lock);
        nimcp_mutex_destroy(&cluster->brains_lock);
        nimcp_mutex_destroy(&cluster->nodes_lock);
        nimcp_free(cluster);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_create_cluster: cluster->strategy->initialize is NULL");
        return NULL;
    }

    // Set initial node status
    cluster->node_status = NODE_STATUS_FOLLOWER;

    // Send initial heartbeat synchronously to register this node immediately
    // This ensures the node is discoverable right after cluster creation
    cluster->strategy->heartbeat(cluster->backend_context, cluster->config.node_id);

    // Start heartbeat thread
    cluster->heartbeat_running = true;
    int thread_result =
        nimcp_thread_create(&cluster->heartbeat_thread, heartbeat_thread_fn, cluster, NULL);
    if (thread_result != 0) {
        set_replication_error("Failed to create heartbeat thread: %d", thread_result);
        cluster->heartbeat_running = false;
        if (cluster->strategy && cluster->strategy->shutdown) {
            cluster->strategy->shutdown(cluster->backend_context);
        }
        nimcp_cond_destroy(&cluster->heartbeat_cond);
        nimcp_mutex_destroy(&cluster->heartbeat_lock);
        nimcp_mutex_destroy(&cluster->brains_lock);
        nimcp_mutex_destroy(&cluster->nodes_lock);
        nimcp_free(cluster);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_create_cluster: validation failed");
        return NULL;
    }

    // Bio-async registration
    cluster->bio_ctx = NULL;
    cluster->bio_async_enabled = false;
    if (config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_REPLICATION,
            .module_name = "replication",
            .inbox_capacity = 64,
            .user_data = cluster
        };
        cluster->bio_ctx = bio_router_register_module(&bio_info);
        if (cluster->bio_ctx) {
            cluster->bio_async_enabled = true;
            LOG_INFO(LOG_MODULE, "Bio-async registered for replication cluster");
        }
    }

    return cluster;
}

/**
 * @brief Destroy replication cluster
 *
 * WHAT: Cleanup cluster resources
 * WHY: Graceful shutdown
 * HOW: Stop heartbeat, unregister brains, cleanup backend
 *
 * COMPLEXITY: O(n) where n = number of registered brains
 */
void replication_destroy_cluster(replication_cluster_t cluster)
{
    if (!cluster)
        return;

    // Stop heartbeat thread (only join if it was actually created)
    bool was_running = cluster->heartbeat_running;
    cluster->heartbeat_running = false;
    if (was_running) {
        // Wake up heartbeat thread immediately
        nimcp_mutex_lock(&cluster->heartbeat_lock);
        nimcp_cond_signal(&cluster->heartbeat_cond);
        nimcp_mutex_unlock(&cluster->heartbeat_lock);
        nimcp_thread_join(cluster->heartbeat_thread, NULL);
    }

    // Bio-async unregistration
    if (cluster->bio_async_enabled && cluster->bio_ctx) {
        bio_router_unregister_module(cluster->bio_ctx);
        cluster->bio_ctx = NULL;
        cluster->bio_async_enabled = false;
    }

    // Free registered brains list
    nimcp_mutex_lock(&cluster->brains_lock);
    registered_brain_t* current = cluster->brains;
    while (current) {
        registered_brain_t* next = current->next;
        // Destroy the brain object itself (not just the registration entry)
        if (current->brain) {
            brain_destroy(current->brain);
        }
        nimcp_free(current);
        current = next;
    }
    nimcp_mutex_unlock(&cluster->brains_lock);

    // Shutdown backend
    if (cluster->strategy && cluster->strategy->shutdown) {
        cluster->strategy->shutdown(cluster->backend_context);
        cluster->backend_context = NULL;
    }

    // Cleanup mutexes and condition variables
    nimcp_cond_destroy(&cluster->heartbeat_cond);
    nimcp_mutex_destroy(&cluster->heartbeat_lock);
    nimcp_mutex_destroy(&cluster->brains_lock);
    nimcp_mutex_destroy(&cluster->nodes_lock);

    // Free cluster
    nimcp_free(cluster);
}

/**
 * @brief Register brain with cluster
 *
 * WHAT: Add brain to replication cluster
 * WHY: Make brain available for sync operations
 * HOW: Add to linked list of registered brains
 *
 * COMPLEXITY: O(1)
 */
bool replication_register_brain(replication_cluster_t cluster, brain_t brain,
                                const char* brain_name)
{
    // Process pending bio-async messages
    if (cluster && cluster->bio_ctx) {
        bio_router_process_inbox(cluster->bio_ctx, 5);
    }

    // Guard: Validate parameters
    if (!cluster || !brain || !brain_name) {
        set_replication_error("Invalid parameters to replication_register_brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_register_brain: required parameter is NULL (cluster, brain, brain_name)");
        return false;
    }

    // Check if brain already registered
    if (find_brain(cluster, brain_name)) {
        set_replication_error("Brain '%s' already registered", brain_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "replication_register_brain: validation failed");
        return false;
    }

    // Create brain entry
    registered_brain_t* brain_entry = nimcp_calloc(1, sizeof(registered_brain_t));
    if (!brain_entry) {
        set_replication_error("Failed to allocate brain entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "replication_register_brain: brain_entry is NULL");
        return false;
    }

    brain_entry->brain = brain;
    strncpy(brain_entry->brain_name, brain_name, sizeof(brain_entry->brain_name) - 1);
    brain_entry->autosync_enabled = false;
    brain_entry->version = 0;
    brain_entry->last_sync_version = 0;

    // Add to linked list (prepend)
    nimcp_mutex_lock(&cluster->brains_lock);
    brain_entry->next = cluster->brains;
    cluster->brains = brain_entry;
    nimcp_mutex_unlock(&cluster->brains_lock);

    return true;
}

/**
 * @brief Unregister brain from cluster
 *
 * WHAT: Remove brain from replication
 * WHY: Brain no longer needs replication
 * HOW: Remove from linked list
 *
 * COMPLEXITY: O(n) where n = number of registered brains
 */
bool replication_unregister_brain(replication_cluster_t cluster, const char* brain_name)
{
    if (!cluster || !brain_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_unregister_brain: required parameter is NULL (cluster, brain_name)");
        return false;
    }

    nimcp_mutex_lock(&cluster->brains_lock);

    registered_brain_t* current = cluster->brains;
    registered_brain_t* prev = NULL;

    while (current) {
        if (strcmp(current->brain_name, brain_name) == 0) {
            // Remove from linked list
            if (prev) {
                prev->next = current->next;
            } else {
                cluster->brains = current->next;
            }

            // Destroy the brain object (cluster owns registered brains)
            if (current->brain) {
                brain_destroy(current->brain);
            }

            nimcp_free(current);
            nimcp_mutex_unlock(&cluster->brains_lock);
            return true;
        }

        prev = current;
        current = current->next;
    }

    nimcp_mutex_unlock(&cluster->brains_lock);
    set_replication_error("Brain '%s' not found", brain_name);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "replication_unregister_brain: operation failed");
    return false;
}

/**
 * @brief Sync brain to cluster (push)
 *
 * WHAT: Save brain state to cluster backend
 * WHY: Make local changes available to other nodes
 * HOW: Serialize brain, store via backend strategy
 *
 * COMPLEXITY: O(s) where s = serialized brain size
 */
bool replication_sync_push(replication_cluster_t cluster, const char* brain_name)
{
    // Guard: Validate parameters
    if (!cluster || !brain_name) {
        set_replication_error("Invalid parameters to replication_sync_push");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_sync_push: required parameter is NULL (cluster, brain_name)");
        return false;
    }

    // Find brain
    registered_brain_t* brain_entry = find_brain(cluster, brain_name);
    if (!brain_entry) {
        set_replication_error("Brain '%s' not registered", brain_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_sync_push: brain_entry is NULL");
        return false;
    }

    // Save brain to temporary file
    char tmp_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/nimcp_sync_%s_%ld.tmp", brain_name,
             (long) nimcp_time_get_sec());

    if (!brain_save(brain_entry->brain, tmp_path)) {
        set_replication_error("Failed to serialize brain '%s'", brain_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "replication_sync_push: brain_save is NULL");
        return false;
    }

    // Read serialized data
    FILE* f = fopen(tmp_path, "rb");
    if (!f) {
        set_replication_error("Failed to open temp file: %s", strerror(errno));
        remove(tmp_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_sync_push: f is NULL");
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void* data = nimcp_malloc(size);
    if (!data) {
        fclose(f);
        remove(tmp_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "replication_sync_push: data is NULL");
        return false;
    }

    /* Read the full serialized brain data. A short read indicates the
     * temp file got truncated between ftell and now — treat as failure. */
    size_t got = fread(data, 1, size, f);
    fclose(f);
    remove(tmp_path);
    if (got != (size_t)size) {
        nimcp_free(data);
        set_replication_error("Truncated temp file: %zu/%ld bytes", got, size);
        return false;
    }

    // Store via backend
    bool success = cluster->strategy->store_brain(cluster->backend_context, brain_name, data, size);
    nimcp_free(data);

    if (success) {
        brain_entry->version++;
        brain_entry->last_sync_version = brain_entry->version;
    }

    return success;
}

/**
 * @brief Sync brain from cluster (pull)
 *
 * WHAT: Load brain state from cluster backend
 * WHY: Get latest changes from other nodes
 * HOW: Retrieve via backend, deserialize, update brain
 *
 * COMPLEXITY: O(s) where s = serialized brain size
 */
bool replication_sync_pull(replication_cluster_t cluster, const char* brain_name)
{
    // Guard: Validate parameters
    if (!cluster || !brain_name) {
        set_replication_error("Invalid parameters to replication_sync_pull");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_sync_pull: required parameter is NULL (cluster, brain_name)");
        return false;
    }

    // Retrieve from backend
    void* data = NULL;
    size_t data_size = 0;

    if (!cluster->strategy->retrieve_brain(cluster->backend_context, brain_name, &data,
                                           &data_size)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "replication_sync_pull: operation failed");
        return false;
    }

    // Write to temporary file
    char tmp_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/nimcp_pull_%s_%ld.tmp", brain_name,
             (long) nimcp_time_get_sec());

    FILE* f = fopen(tmp_path, "wb");
    if (!f) {
        nimcp_free(data);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_sync_pull: f is NULL");
        return false;
    }

    fwrite(data, 1, data_size, f);
    fclose(f);
    nimcp_free(data);

    // Load brain from file
    brain_t loaded_brain = brain_load(tmp_path);
    remove(tmp_path);

    if (!loaded_brain) {
        set_replication_error("Failed to deserialize brain '%s'", brain_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_sync_pull: loaded_brain is NULL");
        return false;
    }

    // Find and update registered brain
    registered_brain_t* brain_entry = find_brain(cluster, brain_name);
    if (brain_entry) {
        // Destroy old brain and replace with loaded brain
        // NOTE: This changes the brain pointer! Caller should not hold references to the old brain.
        brain_destroy(brain_entry->brain);

        // Replace with loaded brain
        brain_entry->brain = loaded_brain;
        brain_entry->last_sync_version = brain_entry->version;
    } else {
        // Not registered yet, register it
        replication_register_brain(cluster, loaded_brain, brain_name);
    }

    return true;
}

/**
 * @brief Get brain from cluster
 *
 * WHAT: Load brain from any available replica
 * WHY: Access shared brain state
 * HOW: Pull from backend, return loaded brain
 *
 * COMPLEXITY: O(s) where s = serialized size
 */
brain_t replication_get_brain(replication_cluster_t cluster, const char* brain_name)
{
    if (!cluster || !brain_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_get_brain: required parameter is NULL (cluster, brain_name)");
        return NULL;
    }

    // Try pulling from cluster
    if (!replication_sync_pull(cluster, brain_name)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_get_brain: replication_sync_pull is NULL");
        return NULL;
    }

    // Find registered brain
    registered_brain_t* brain_entry = find_brain(cluster, brain_name);
    return brain_entry ? brain_entry->brain : NULL;
}

/**
 * @brief Enable/disable auto-sync
 *
 * WHAT: Automatically push changes after learning
 * WHY: Keep cluster in sync without manual pushes
 * HOW: Set flag on registered brain
 *
 * COMPLEXITY: O(n) to find brain
 */
bool replication_set_autosync(replication_cluster_t cluster, const char* brain_name, bool enabled)
{
    registered_brain_t* brain_entry = find_brain(cluster, brain_name);
    if (!brain_entry) {
        set_replication_error("Brain '%s' not registered", brain_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_set_autosync: brain_entry is NULL");
        return false;
    }

    brain_entry->autosync_enabled = enabled;
    return true;
}

/**
 * @brief Get cluster status
 *
 * WHAT: Query backend for active nodes
 * WHY: Monitor cluster health
 * HOW: Delegate to backend strategy
 *
 * COMPLEXITY: O(n) where n = number of nodes
 */
uint32_t replication_get_cluster_status(replication_cluster_t cluster, cluster_node_t* nodes,
                                        uint32_t max_nodes)
{
    if (!cluster || !nodes || max_nodes == 0)
        return 0;

    return cluster->strategy->get_nodes(cluster->backend_context, nodes, max_nodes);
}

/**
 * @brief Get this node's status
 *
 * COMPLEXITY: O(1)
 */
replication_node_status_t replication_get_node_status(replication_cluster_t cluster)
{
    return cluster ? cluster->node_status : NODE_STATUS_DEAD;
}

/**
 * @brief Get replication lag
 *
 * WHAT: Measure how out-of-sync this node is
 * WHY: Monitor replication health
 * HOW: Compare local version to cluster version
 *
 * COMPLEXITY: O(1)
 */
uint32_t replication_get_lag(replication_cluster_t cluster, const char* brain_name)
{
    registered_brain_t* brain_entry = find_brain(cluster, brain_name);
    if (!brain_entry)
        return 0;

    uint64_t lag = brain_entry->version - brain_entry->last_sync_version;
    return (uint32_t) (lag * 100);  // Rough estimate in ms
}

/**
 * @brief Check if cluster is healthy
 *
 * WHAT: Verify majority of nodes are alive
 * WHY: Ensure cluster can serve requests
 * HOW: Query nodes, count alive vs dead
 *
 * COMPLEXITY: O(n) where n = number of nodes
 */
bool replication_is_healthy(replication_cluster_t cluster)
{
    if (!cluster) {
        return false;
    }

    cluster_node_t nodes[32];
    uint32_t num_nodes = replication_get_cluster_status(cluster, nodes, 32);

    if (num_nodes == 0) {
        return false;
    }

    uint32_t alive_count = 0;
    for (uint32_t i = 0; i < num_nodes; i++) {
        if (nodes[i].status != NODE_STATUS_DEAD) {
            alive_count++;
        }
    }

    // Healthy if majority alive
    return alive_count > (num_nodes / 2);
}

//=============================================================================
// Convenience Functions
//=============================================================================

/**
 * @brief Create Redis replication cluster
 *
 * PATTERN: Factory method for Redis backend
 * COMPLEXITY: O(1) plus backend initialization
 */
replication_cluster_t replication_create_redis_cluster(const char* redis_url,
                                                       const char* cluster_name,
                                                       const char* node_id)
{
    replication_config_t config = {.backend = REPLICATION_BACKEND_REDIS,
                                   .strategy = REPLICATION_STRATEGY_LEADER_FOLLOWER,
                                   .sync_interval_ms = NIMCP_TIMEOUT_LONG_MS,
                                   .heartbeat_interval_ms = 5000,
                                   .node_timeout_ms = 15000,
                                   .enable_vector_clock = false,
                                   .enable_crdt = false};

    strncpy(config.connection_string, redis_url, sizeof(config.connection_string) - 1);
    strncpy(config.cluster_name, cluster_name, sizeof(config.cluster_name) - 1);
    strncpy(config.node_id, node_id, sizeof(config.node_id) - 1);

    return replication_create_cluster(&config);
}

/**
 * @brief Create filesystem replication cluster
 *
 * PATTERN: Factory method for filesystem backend
 * COMPLEXITY: O(1) plus backend initialization
 */
replication_cluster_t replication_create_filesystem_cluster(const char* shared_dir,
                                                            const char* node_id)
{
    // Guard: Validate parameters
    if (!shared_dir || !node_id) {
        set_replication_error("Null shared_dir or node_id provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "replication_create_filesystem_cluster: required parameter is NULL (shared_dir, node_id)");
        return NULL;
    }

    replication_config_t config = {.backend = REPLICATION_BACKEND_FILESYSTEM,
                                   .strategy = REPLICATION_STRATEGY_EVENTUAL,
                                   .sync_interval_ms = 5000,
                                   .heartbeat_interval_ms = 10000,
                                   .node_timeout_ms = NIMCP_LONG_TIMEOUT_MS,
                                   .enable_vector_clock = false,
                                   .enable_crdt = false};

    snprintf(config.connection_string, sizeof(config.connection_string), "file://%s", shared_dir);
    strncpy(config.cluster_name, "default", sizeof(config.cluster_name) - 1);
    strncpy(config.node_id, node_id, sizeof(config.node_id) - 1);

    return replication_create_cluster(&config);
}
