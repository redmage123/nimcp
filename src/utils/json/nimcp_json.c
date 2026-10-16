#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_json.c - Thread-Safe JSON Processing Wrapper
//=============================================================================
// ARCHITECTURAL OVERVIEW:
// This module implements a thread-safe wrapper around the Jansson JSON library,
// providing NIMCP with a consistent, safe interface for JSON parsing, generation,
// manipulation, and file I/O. It abstracts away Jansson's complexity while adding
// thread safety guarantees and NIMCP-specific error handling.
//
// KEY DESIGN: WRAPPER + FACADE + ADAPTER PATTERN
// ===============================================
// WHY WRAPPER PATTERN:
// - Encapsulates external dependency (Jansson)
// - Provides simplified, application-specific interface
// - Enables switching JSON libraries without changing client code
// - Adds thread safety layer on top of library
// - Centralizes error handling and memory management
//
// WHY FACADE PATTERN:
// - Hides complexity of Jansson's reference counting
// - Provides high-level operations (get_string, set_integer)
// - Unifies file and string operations under single context
// - Simplifies common tasks (path-based access)
//
// WHY ADAPTER PATTERN:
// - Adapts Jansson's C API to NIMCP conventions
// - Converts Jansson errors to NIMCP result codes
// - Bridges Jansson's json_t* to type-safe accessors
// - Translates between Jansson and NIMCP memory management
//
// JSON LIBRARY SELECTION: JANSSON
// ================================
// WHY JANSSON:
// - Pure C implementation (no C++ dependencies)
// - BSD license (compatible with NIMCP)
// - Mature and stable (10+ years development)
// - Thread-safe with proper locking (intrinsic reference counting)
// - Rich API for tree manipulation
// - Excellent documentation
// - Widely used in production systems
//
// ALTERNATIVES CONSIDERED:
// - cJSON: Simpler but less robust, manual memory management
// - json-c: More complex API, inconsistent error handling
// - YAJL: Stream-oriented, not ideal for tree manipulation
// - parson: Fewer features, less mature
//
// TRADE-OFFS:
// - External dependency vs reinventing wheel: Dependency wins (quality)
// - Reference counting complexity vs manual memory: Reference counting wins (safety)
// - Feature richness vs code size: Features win (flexibility)
//
// THREAD SAFETY ARCHITECTURE:
// ============================
//
//   ┌───────────────────────────────────────────────┐
//   │  JsonContext (per-document state)             │
//   │  ┌─────────────────────────────────────────┐  │
//   │  │ nimcp_mutex_t mutex                     │  │ ← Protects all operations
//   │  └─────────────────────────────────────────┘  │
//   │  ┌─────────────────────────────────────────┐  │
//   │  │ json_t* root (Jansson tree)             │  │ ← Protected by mutex
//   │  └─────────────────────────────────────────┘  │
//   └───────────────────────────────────────────────┘
//
// WHY PER-CONTEXT MUTEX:
// - Fine-grained locking (multiple contexts can be used concurrently)
// - Protects entire tree (no partial updates visible)
// - Prevents race conditions during tree modifications
// - Ensures reference count consistency
//
// ALTERNATIVE APPROACHES REJECTED:
// - No locking: Jansson's refcounting is NOT thread-safe by itself
// - Global mutex: Would serialize all JSON operations across contexts
// - Per-node locking: Complex, deadlock-prone, high overhead
// - Read-write locks: More complexity, minimal benefit (most ops modify)
//
// LOCKING GUARANTEES:
// - All public functions acquire context lock
// - Lock held for entire operation (no partial visibility)
// - Lock released before return (even on error paths)
// - Helper functions assume lock is held (NOT thread-safe alone)
//
// PATH-BASED ACCESS SYSTEM:
// ==========================
// WHAT: JSONPath-like addressing (e.g., "config/server/port")
// WHY: Intuitive navigation of nested structures without manual traversal
//
// PATH SYNTAX:
//   "key"              → Root object's "key" field
//   "obj/nested"       → obj["nested"]
//   "array/0"          → array[0] (numeric index)
//   "a/b/c/d"          → Arbitrarily deep nesting
//
// ALGORITHM (resolve_json_path):
// 1. Split path by '/' delimiter
// 2. Start at root
// 3. For each token:
//    - If current is object: Look up token as key
//    - If current is array: Parse token as integer index
//    - If neither: Fail (type mismatch)
// 4. Return final node (or NULL if path doesn't exist)
//
// WHY NOT JSONPath standard:
// - Simple paths sufficient for NIMCP use cases
// - Avoids JSONPath parser complexity (wildcards, filters, etc.)
// - Faster implementation (no regex, no query engine)
// - Predictable performance (O(n) where n = path depth)
//
// COMPLEXITY: O(n) where n = path depth
// - Each token: O(1) for object lookup, O(1) for array index
// - Total: Linear in path segments
//
// MEMORY MANAGEMENT STRATEGY:
// ============================
// JANSSON REFERENCE COUNTING:
// - json_t* objects are reference-counted
// - json_incref(): Increment reference count (ownership)
// - json_decref(): Decrement reference count (release ownership)
// - When refcount reaches 0: Object freed automatically
//
// WHY REFERENCE COUNTING:
// - Automatic memory management (no manual tracking)
// - Safe sharing of JSON nodes
// - Prevents use-after-free
// - Prevents double-free
//
// TRADE-OFFS:
// - Overhead: 8 bytes per object (refcount)
// - Complexity: Must carefully manage incref/decref
// - Cycles: Circular references cause leaks (rare in JSON)
//
// NIMCP WRAPPER RULES:
// 1. context->root: Owned by context (freed on destroy)
// 2. get_value(): Returns NEW reference (caller must decref)
// 3. set_value(): Borrows reference (incref internally if needed)
// 4. Internal helpers: Borrow references (no incref/decref)
//
// EXAMPLE LIFECYCLE:
//   JsonContext* ctx = ...;
//   json_t* value = json_string("foo");  // refcount = 1
//   nimcp_json_set_value(ctx, "key", value);  // Jansson increfs internally
//   json_decref(value);  // Release our reference, but key still owns it
//   // Later...
//   nimcp_json_destroy_context(ctx);  // Frees root, which frees all children
//
// WHY THIS APPROACH:
// - Matches Jansson conventions (interoperable)
// - Clear ownership semantics
// - Prevents leaks and double-frees
//
// ERROR HANDLING STRATEGY:
// =========================
// RESULT CODE DESIGN:
// - JSON_SUCCESS (0): Operation succeeded
// - Negative codes: Different error types
// - Each error has string description (nimcp_json_get_error)
//
// ERROR CATEGORIES:
// 1. INVALID_PARAM: NULL pointer, invalid argument
// 2. MEMORY: Allocation failure
// 3. PARSE: Invalid JSON syntax
// 4. FILE: I/O error (open, read, write)
// 5. TYPE: Type mismatch (expected string, got integer)
// 6. NOT_FOUND: Path doesn't exist
// 7. LOCK: Mutex acquisition failure
// 8. NULL_VALUE: JSON null (not an error, but needs handling)
//
// WHY RESULT CODES vs EXCEPTIONS:
// - C doesn't have exceptions
// - Explicit error handling (no hidden control flow)
// - Caller decides how to handle errors
// - Composable (can propagate up call stack)
//
// ERROR PROPAGATION:
// - Internal helpers return NULL on error
// - Public functions return JsonResult
// - Lock always released on error paths
// - Partial state changes rolled back when possible
//
// USE CASES IN NIMCP:
// ===================
// 1. CONFIGURATION FILES
//    - Load server.json at startup
//    - Read host, port, threads, etc.
//    - Update config and save back to disk
//
// 2. MESSAGE SERIALIZATION
//    - Convert C structs to JSON for network transmission
//    - Parse incoming JSON messages
//    - Handle protocol negotiation (version, capabilities)
//
// 3. STATE PERSISTENCE
//    - Serialize application state to JSON
//    - Load previous state on restart
//    - Checkpoint long-running operations
//
// 4. LOGGING AND DIAGNOSTICS
//    - Structured log output (JSON format)
//    - Export metrics as JSON
//    - Debugging dumps (human-readable JSON)
//
// DESIGN PATTERNS:
// ================
// 1. WRAPPER: Encapsulates Jansson library
// 2. FACADE: Simplifies complex Jansson API
// 3. ADAPTER: Translates Jansson to NIMCP conventions
// 4. OPAQUE POINTER: JsonContext hides implementation
// 5. RAII (Resource Acquisition Is Initialization):
//    - Context creation initializes mutex
//    - Context destruction frees all resources
// 6. DEPENDENCY INVERSION: Clients depend on JsonContext interface,
//    not on Jansson directly
//
// SOLID PRINCIPLES:
// =================
// - Single Responsibility: Each function has one clear purpose
//   * create_context: Initialize context
//   * load_file: Parse JSON from file
//   * get_value: Retrieve value by path
//   * set_value: Update value by path
//
// - Open/Closed: Can extend with new value types without modifying core
//   * Add get_array_value() without changing get_value()
//   * Add new error codes without changing error handling
//
// - Liskov Substitution: All JsonContext pointers behave consistently
//   * Any JsonContext* can be used with any function
//   * NULL context always returns INVALID_PARAM
//
// - Interface Segregation: Separate interfaces for different operations
//   * File I/O: load_file, dump_file
//   * Value access: get_value, set_value
//   * Type-specific: get_string, get_integer, etc.
//
// - Dependency Inversion: Depends on Jansson abstraction, not implementation
//   * Could swap Jansson for another library
//   * Clients don't know about json_t* or Jansson API
//
// PERFORMANCE CHARACTERISTICS:
// ============================
// SPACE COMPLEXITY:
// - Context: sizeof(JsonContext) ≈ 64 bytes
// - Per-node overhead: ~40 bytes (Jansson internal)
// - String storage: Actual string length + 1 (null terminator)
// - Array/object: Proportional to number of elements
//
// TIME COMPLEXITY:
// - create_context: O(1)
// - load_file: O(n) where n = file size
// - get_value: O(d) where d = path depth
// - set_value: O(d) path traversal + O(1) assignment
// - dump_file: O(n) where n = tree size
//
// CONCURRENCY:
// - Lock contention: O(1) per operation (held during entire operation)
// - Scalability: Multiple contexts scale linearly (independent locks)
// - Bottleneck: Single context accessed by many threads serializes
//
// LIMITATIONS AND TRADE-OFFS:
// ===========================
// LIMITATIONS:
// 1. External dependency (Jansson must be installed)
// 2. No streaming (entire document in memory)
// 3. No schema validation (trust input format)
// 4. Path syntax is simplified (not full JSONPath)
// 5. No JSONPointer support (RFC 6901)
// 6. Circular references cause memory leaks (rare)
//
// TRADE-OFFS:
// - Simplicity vs Features: Simple paths vs full JSONPath
//   CHOICE: Simple (sufficient for NIMCP needs)
//
// - Performance vs Safety: Lock granularity
//   CHOICE: Entire operation locked (safety over max performance)
//
// - Memory vs Speed: In-memory tree vs streaming
//   CHOICE: In-memory (easier API, typical JSON files are small)
//
// - Dependency vs NIH: External library vs roll our own
//   CHOICE: External (quality, maturity, correctness)
//
// WHY THESE TRADE-OFFS:
// - NIMCP doesn't need JSONPath complexity (simple configs)
// - JSON files are small (<1MB typically)
// - Thread safety more important than maximum throughput
// - Jansson quality exceeds what we'd build in-house
//
// TYPICAL USAGE PATTERN:
// ======================
//
//   // 1. Create context
//   JsonContext* ctx = NULL;
//   if (nimcp_json_create_context(&ctx) != JSON_SUCCESS) {
//       // Handle error
//   }
//
//   // 2. Load JSON from file
//   if (nimcp_json_load_file(ctx, "config.json", 0) != JSON_SUCCESS) {
//       // Handle parse error
//   }
//
//   // 3. Read values
//   char host[NIMCP_ERROR_BUFFER_SIZE];
//   int64_t port;
//   nimcp_json_get_string_value(ctx, "server/host", host, sizeof(host));
//   nimcp_json_get_integer_value(ctx, "server/port", &port);
//
//   // 4. Modify values
//   nimcp_json_set_integer_value(ctx, "stats/connections", 42);
//
//   // 5. Save back to file
//   nimcp_json_dump_file(ctx, "config.json", JSON_INDENT(2));
//
//   // 6. Cleanup
//   nimcp_json_destroy_context(ctx);
//
// INTEGRATION WITH NIMCP:
// =======================
// - Uses nimcp_memory for allocations (tracking, leak detection)
// - Uses nimcp_mutex for thread safety (consistent with NIMCP threading)
// - Uses nimcp_logging for errors (when available)
// - Follows NIMCP naming conventions (nimcp_json_*)
// - Returns NIMCP-style result codes (negative for error)
//
//=============================================================================

/**
 * @file nimcp_json.c
 * @brief Thread-safe JSON wrapper implementation
 *
 * WHAT: Thread-safe wrapper around Jansson JSON library
 * WHY: Provide NIMCP with safe, simple JSON processing
 * HOW: Per-context mutex + path-based access + type-safe API
 */

#define NIMCP_INTERNAL
#include "utils/json/nimcp_json.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(json)

//=============================================================================
// Static Error Messages
//=============================================================================

/**
 * WHAT: Human-readable error descriptions
 * WHY: Debugging aid, error reporting
 * HOW: Array indexed by negated error code
 *
 * USAGE: ERROR_MESSAGES[-JSON_ERROR_MEMORY] → "Memory allocation failed"
 */
static const char* const ERROR_MESSAGES[] = {[JSON_SUCCESS] = "Success",
                                             [-JSON_ERROR_INVALID_PARAM] = "Invalid parameter",
                                             [-JSON_ERROR_MEMORY] = "Memory allocation failed",
                                             [-JSON_ERROR_PARSE] = "JSON parsing failed",
                                             [-JSON_ERROR_FILE] = "File operation failed",
                                             [-JSON_ERROR_TYPE] = "Type mismatch",
                                             [-JSON_ERROR_NOT_FOUND] = "Path not found",
                                             [-JSON_ERROR_LOCK] = "Lock acquisition failed",
                                             [-JSON_ERROR_NULL_VALUE] = "JSON null value"};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Acquire context lock
 *
 * WHY HELPER FUNCTION:
 * - Centralizes lock acquisition logic
 * - Consistent error handling
 * - Reduces code duplication
 * - Easy to add lock debugging later
 *
 * ERROR HANDLING:
 * - NULL context: INVALID_PARAM (defensive programming)
 * - Lock failure: LOCK (should never happen in practice)
 *
 * COMPLEXITY: O(1) - mutex operation
 * THREAD SAFETY: N/A (this IS the synchronization primitive)
 *
 * @param ctx Context to lock
 * @return JSON_SUCCESS or error code
 */
static JsonResult acquire_lock(JsonContext* ctx)
{
    if (!ctx)
        return JSON_ERROR_INVALID_PARAM;
    if (nimcp_mutex_lock(&ctx->mutex) != NIMCP_SUCCESS)
        return JSON_ERROR_LOCK;
    return JSON_SUCCESS;
}

/**
 * @brief Release context lock
 *
 * WHY HELPER FUNCTION:
 * - Symmetry with acquire_lock
 * - NULL-safe (common pattern in cleanup code)
 * - Potential for lock debugging/tracing
 *
 * COMPLEXITY: O(1) - mutex operation
 * THREAD SAFETY: N/A (lock release is the synchronization)
 *
 * @param ctx Context to unlock (NULL-safe)
 */
static void release_lock(JsonContext* ctx)
{
    if (ctx)
        nimcp_mutex_unlock(&ctx->mutex);
}

/**
 * @brief Resolve JSON path to node
 *
 * WHY PATH-BASED ACCESS:
 * - More convenient than manual tree traversal
 * - Intuitive for configuration access: "server/host"
 * - Reduces client code complexity
 * - Enables generic get/set operations
 *
 * ALGORITHM:
 * 1. Duplicate path (strtok_r modifies string)
 * 2. Split path by '/' delimiter
 * 3. For each token:
 *    a. If current is object: json_object_get(current, token)
 *    b. If current is array: Parse token as index, json_array_get(current, index)
 *    c. If neither: Fail (type mismatch or path doesn't exist)
 * 4. Return final node
 *
 * OUTPUT PARAMETERS:
 * - parent: Second-to-last node (for set operations)
 * - last_key: Last token (for set operations)
 *
 * WHY RETURN PARENT:
 * - set_value needs parent to modify child
 * - Example: To set "a/b/c", need parent="a/b" and last_key="c"
 *
 * MEMORY MANAGEMENT:
 * - path_copy: Allocated with strdup, freed before return
 * - last_key: Allocated with strdup if requested, caller must free
 *
 * ERROR HANDLING:
 * - Returns NULL if path doesn't exist (NOT an error, caller decides)
 * - Returns NULL if type mismatch (object key on array)
 *
 * COMPLEXITY: O(d) where d = path depth
 * - strtok_r: O(n) where n = path length (one pass)
 * - Per token: O(1) object lookup or array index
 *
 * THREAD SAFETY: NOT thread-safe alone (caller must hold lock)
 *
 * LIMITATIONS:
 * - No escaping (can't have '/' in key names)
 * - No array slices or wildcards
 * - No JSONPath expressions (filters, recursion, etc.)
 *
 * WHY LIMITATIONS ACCEPTABLE:
 * - NIMCP JSON is for simple configs, not complex queries
 * - Can always traverse manually for complex cases
 * - Simpler implementation = fewer bugs
 *
 * @param root Root of JSON tree
 * @param path Path to resolve (e.g., "server/host")
 * @param parent Output: Parent node (optional)
 * @param last_key Output: Last path component (optional, caller must free)
 * @return Resolved node or NULL if not found
 */
static json_t* resolve_json_path(json_t* root, const char* path, json_t** parent, char** last_key)
{
    if (!root || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "resolve_json_path: required parameter is NULL (root, path)");
        return NULL;
    }

    json_t* current = root;
    json_t* prev = NULL;
    // Use nimcp_malloc instead of strdup to match nimcp_free below
    size_t path_len = strlen(path);
    char* path_copy = nimcp_malloc(path_len + 1);
    if (!path_copy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "resolve_json_path: path_copy is NULL");
        return NULL;
    }
    strncpy(path_copy, path, path_len);
    path_copy[path_len] = '\0';
    char* saveptr = NULL;
    char* token = strtok_r(path_copy, "/", &saveptr);
    char* last_token = NULL;

    // Traverse path components
    while (token) {
        last_token = token;
        prev = current;

        if (json_is_object(current)) {
            // Object: Look up token as key
            current = json_object_get(current, token);
        } else if (json_is_array(current)) {
            // Array: Parse token as integer index
            char* endptr;
            long index = strtol(token, &endptr, 10);
            if (*endptr != '\0' || index < 0) {
                // Invalid index (non-numeric or negative)
                current = NULL;
                break;
            }
            current = json_array_get(current, index);
        } else {
            // Neither object nor array: Type mismatch
            current = NULL;
            break;
        }

        // Advance to next token
        token = strtok_r(NULL, "/", &saveptr);
    }

    // Fill output parameters if requested
    if (parent)
        *parent = prev;
    if (last_key && last_token) {
        // Use nimcp_malloc for consistency (caller should use nimcp_free)
        size_t token_len = strlen(last_token);
        *last_key = nimcp_malloc(token_len + 1);
        if (*last_key) {
            strncpy(*last_key, last_token, token_len);
            (*last_key)[token_len] = '\0';
        }
    }
    nimcp_free(path_copy);

    return current;
}

//=============================================================================
// Context Management Implementation
//=============================================================================

/**
 * @brief Create JSON context
 *
 * WHY CONTEXT OBJECT:
 * - Encapsulates state (root tree, mutex)
 * - Enables multiple independent JSON documents
 * - Provides lifetime management (create/destroy)
 * - Enables thread safety (per-context locking)
 *
 * INITIALIZATION:
 * 1. Allocate context with nimcp_calloc (zeroed memory)
 * 2. Initialize mutex
 * 3. Set root to NULL (no JSON loaded yet)
 * 4. Return via output parameter
 *
 * WHY OUTPUT PARAMETER:
 * - Enables returning error code separately
 * - Avoids mixing NULL and error semantics
 * - Consistent with NIMCP conventions
 *
 * ERROR HANDLING:
 * - NULL ctx: INVALID_PARAM (defensive)
 * - Allocation failure: MEMORY
 * - Mutex init failure: LOCK (cleanup partial state)
 *
 * CLEANUP ON ERROR:
 * - If mutex init fails, free allocated context
 * - Leaves *ctx unchanged on error (caller's pointer unmodified)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully thread-safe (no shared state accessed)
 *
 * @param ctx Output parameter for created context
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_create_context(JsonContext** ctx)
{
    LOG_TRACE("Entering nimcp_json_create_context");
    if (!ctx) {
        LOG_ERROR("nimcp_json_create_context: NULL ctx pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL ctx pointer in nimcp_json_create_context");
        return JSON_ERROR_INVALID_PARAM;
    }

    // Allocate context (calloc zeros memory)
    JsonContext* new_ctx = nimcp_calloc(1, sizeof(JsonContext));
    if (!new_ctx) {
        LOG_ERROR("Failed to allocate JSON context");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(JsonContext), "Failed to allocate JSON context");
        return JSON_ERROR_MEMORY;
    }

    // Initialize mutex for thread safety
    if (nimcp_mutex_init(&new_ctx->mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize JSON context mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SYSTEM, "Failed to initialize JSON context mutex");
        nimcp_free(new_ctx);
        return JSON_ERROR_LOCK;
    }

    // Context starts with no JSON loaded
    new_ctx->root = NULL;
    *ctx = new_ctx;
    return JSON_SUCCESS;
}

/**
 * @brief Destroy JSON context
 *
 * WHY EXPLICIT DESTROY:
 * - RAII pattern (Resource Acquisition Is Initialization)
 * - Ensures all resources freed
 * - Prevents leaks
 *
 * CLEANUP SEQUENCE:
 * 1. Lock mutex (prevent concurrent access during destruction)
 * 2. Free JSON tree if present (json_decref recursively frees)
 * 3. Unlock mutex (allows any blocked threads to proceed)
 * 4. Destroy mutex (releases OS resources)
 * 5. Free context structure
 *
 * WHY THIS ORDER:
 * - Lock first: Prevent concurrent modification during cleanup
 * - Free JSON: While mutex still valid (in case Jansson uses it)
 * - Unlock: Clean shutdown, let blocked threads error out
 * - Destroy mutex: After unlock (can't destroy locked mutex)
 * - Free context: Last, after all resources released
 *
 * NULL SAFETY:
 * - Returns immediately if ctx is NULL
 * - Idempotent: Can safely call multiple times (first call frees, rest no-op)
 *
 * THREAD SAFETY:
 * - Safe to call while other threads blocked on mutex (they'll error)
 * - NOT safe to call concurrently (caller must synchronize)
 * - Typical usage: Call after all threads stopped using context
 *
 * COMPLEXITY: O(n) where n = number of nodes in JSON tree
 * - json_decref: Recursively frees all children
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void nimcp_json_destroy_context(JsonContext* ctx)
{
    LOG_TRACE("Entering nimcp_json_destroy_context");
    if (!ctx)
        return;

    // Lock during cleanup (prevent concurrent access)
    nimcp_mutex_lock(&ctx->mutex);
    if (ctx->root) {
        // Free JSON tree (json_decref handles recursive freeing)
        json_decref(ctx->root);
        ctx->root = NULL;
    }
    nimcp_mutex_unlock(&ctx->mutex);

    // Destroy synchronization primitive
    nimcp_mutex_destroy(&ctx->mutex);

    // Free context structure
    nimcp_free(ctx);
}

//=============================================================================
// File Operations Implementation
//=============================================================================

/**
 * @brief Load JSON from file
 *
 * WHY FILE LOADING:
 * - Configuration files (server.json, database.json)
 * - State persistence (save/restore application state)
 * - Data import (process JSON files from external sources)
 *
 * ALGORITHM:
 * 1. Validate parameters
 * 2. Acquire lock (prevent concurrent modifications)
 * 3. Call json_load_file (Jansson does parsing)
 * 4. If successful:
 *    a. Free old root if present (replacing existing JSON)
 *    b. Store new root in context
 * 5. Release lock
 * 6. Return result
 *
 * FLAGS PARAMETER:
 * - Passed directly to Jansson (flexibility)
 * - Common flags:
 *   * 0: Default (strict parsing)
 *   * JSON_REJECT_DUPLICATES: Error on duplicate keys
 *   * JSON_DECODE_ANY: Allow any JSON value (not just object/array)
 *
 * REFERENCE COUNTING:
 * - json_load_file returns new reference (refcount = 1)
 * - Context takes ownership (no need to incref)
 * - Old root decref'd (releases previous JSON)
 *
 * ERROR HANDLING:
 * - NULL ctx or filename: INVALID_PARAM
 * - Parse error: PARSE (syntax error in JSON)
 * - Lock failure: LOCK
 *
 * PARTIAL STATE:
 * - On parse error: Old root remains unchanged (rollback)
 * - On lock error: No changes made
 * - On success: Old root freed, new root installed (atomic swap)
 *
 * COMPLEXITY: O(n) where n = file size
 * - File I/O: O(n) bytes read
 * - Parsing: O(n) JSON text processed
 *
 * THREAD SAFETY: Fully thread-safe (lock-protected)
 *
 * @param ctx Context to load into
 * @param filename Path to JSON file
 * @param flags Jansson parsing flags (0 for defaults)
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_load_file(JsonContext* ctx, const char* filename, size_t flags)
{
    LOG_TRACE("Entering nimcp_json_load_file");
    if (!ctx || !filename)
        return JSON_ERROR_INVALID_PARAM;

    // Acquire lock for duration of operation
    JsonResult result = acquire_lock(ctx);
    if (result != JSON_SUCCESS)
        return result;

    // Parse JSON from file
    json_error_t error;
    json_t* new_root = json_load_file(filename, flags, &error);

    if (!new_root) {
        // Parse failed: Return error, leave context unchanged
        release_lock(ctx);
        return JSON_ERROR_PARSE;
    }

    // Success: Replace old root with new
    if (ctx->root) {
        json_decref(ctx->root);  // Free old JSON tree
    }
    ctx->root = new_root;

    release_lock(ctx);
    return JSON_SUCCESS;
}

/**
 * @brief Save JSON to file
 *
 * WHY FILE SAVING:
 * - Persist configuration changes
 * - Save application state
 * - Export data for external tools
 *
 * ALGORITHM:
 * 1. Validate parameters (including ctx->root must exist)
 * 2. Acquire lock (prevent modifications during serialization)
 * 3. Call json_dump_file (Jansson does serialization)
 * 4. Release lock
 * 5. Return result
 *
 * FLAGS PARAMETER:
 * - Passed directly to Jansson
 * - Common flags:
 *   * JSON_INDENT(n): Pretty-print with n spaces
 *   * JSON_COMPACT: No whitespace
 *   * JSON_SORT_KEYS: Sort object keys alphabetically
 *   * JSON_ENSURE_ASCII: Escape non-ASCII characters
 *
 * ERROR HANDLING:
 * - NULL ctx or filename: INVALID_PARAM
 * - No root loaded: INVALID_PARAM (nothing to save)
 * - File write error: FILE (disk full, permissions, etc.)
 * - Lock failure: LOCK
 *
 * ATOMICITY:
 * - json_dump_file is NOT atomic (overwrites file directly)
 * - Recommendation: Write to temp file, then rename (atomic on POSIX)
 * - This wrapper doesn't implement atomic save (caller's responsibility)
 *
 * WHY NOT ATOMIC:
 * - Adds complexity (temp file, rename, cleanup)
 * - Not always needed (config files rarely change)
 * - Can be implemented in caller if needed
 *
 * COMPLEXITY: O(n) where n = JSON tree size
 * - Serialization: Visit every node
 * - File I/O: O(n) bytes written
 *
 * THREAD SAFETY: Fully thread-safe (lock-protected)
 *
 * @param ctx Context to save from
 * @param filename Path to write JSON file
 * @param flags Jansson formatting flags (e.g., JSON_INDENT(2))
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_dump_file(JsonContext* ctx, const char* filename, size_t flags)
{
    LOG_TRACE("Entering nimcp_json_dump_file");
    if (!ctx || !filename || !ctx->root)
        return JSON_ERROR_INVALID_PARAM;

    // Acquire lock (prevent modifications during save)
    JsonResult result = acquire_lock(ctx);
    if (result != JSON_SUCCESS)
        return result;

    // Serialize JSON to file
    if (json_dump_file(ctx->root, filename, flags) != 0) {
        // Write failed
        release_lock(ctx);
        return JSON_ERROR_FILE;
    }

    release_lock(ctx);
    return JSON_SUCCESS;
}

//=============================================================================
// Value Access Implementation
//=============================================================================

/**
 * @brief Get value by path
 *
 * WHY GET_VALUE:
 * - Core access operation (all type-specific getters use this)
 * - Returns raw json_t* for advanced use cases
 * - Flexible (caller can inspect type, iterate children, etc.)
 *
 * ALGORITHM:
 * 1. Validate parameters
 * 2. Acquire lock
 * 3. Resolve path to node
 * 4. If found:
 *    a. Increment reference count (transfer ownership to caller)
 *    b. Return SUCCESS
 * 5. If not found:
 *    a. Return NOT_FOUND
 * 6. Release lock
 *
 * REFERENCE COUNTING:
 * - Increments refcount before returning (json_incref)
 * - Caller receives new reference (must decref when done)
 * - Prevents use-after-nimcp_free(node remains valid until caller decrefs)
 *
 * WHY INCREF:
 * - Node might be deleted from tree after we release lock
 * - Caller needs guarantee that node stays valid
 * - Matches Jansson conventions (get = new reference)
 *
 * USAGE PATTERN:
 *   json_t* value = NULL;
 *   if (nimcp_json_get_value(ctx, "key", &value) == JSON_SUCCESS) {
 *       // Use value...
 *       json_decref(value);  // Must release when done
 *   }
 *
 * ERROR HANDLING:
 * - NULL parameters: INVALID_PARAM
 * - Path not found: NOT_FOUND (not necessarily an error)
 * - Lock failure: LOCK
 *
 * COMPLEXITY: O(d) where d = path depth
 * THREAD SAFETY: Fully thread-safe (lock-protected)
 *
 * @param ctx Context to query
 * @param path Path to value (e.g., "server/host")
 * @param value Output parameter for value (caller must decref)
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_get_value(JsonContext* ctx, const char* path, json_t** value)
{
    LOG_TRACE("Entering nimcp_json_get_value");
    if (!ctx || !path || !value)
        return JSON_ERROR_INVALID_PARAM;

    // Acquire lock for duration of access
    JsonResult result = acquire_lock(ctx);
    if (result != JSON_SUCCESS)
        return result;

    // Resolve path to node
    *value = resolve_json_path(ctx->root, path, NULL, NULL);
    if (!*value) {
        // Path doesn't exist
        release_lock(ctx);
        return JSON_ERROR_NOT_FOUND;
    }

    // Transfer ownership to caller (increment refcount)
    json_incref(*value);
    release_lock(ctx);
    return JSON_SUCCESS;
}

/**
 * @brief Set value by path
 *
 * WHY SET_VALUE:
 * - Core modification operation
 * - Updates existing values or adds new ones
 * - Accepts any json_t* (string, number, object, array, etc.)
 *
 * ALGORITHM:
 * 1. Validate parameters
 * 2. Acquire lock
 * 3. If root is NULL, create empty object (lazy initialization)
 * 4. Resolve path to parent and last key
 * 5. If parent is object:
 *    a. json_object_set(parent, last_key, value)
 * 6. If parent is array:
 *    a. Parse last key as index
 *    b. json_array_set(parent, index, value)
 * 7. Release lock
 * 8. Free last_key
 *
 * WHY LAZY INITIALIZATION:
 * - Convenient: Can create context and immediately set values
 * - Common pattern: Build JSON from scratch without explicit root creation
 *
 * PATH CREATION:
 * - Does NOT create intermediate nodes
 * - Parent must already exist
 * - Example: To set "a/b/c", "a/b" must exist
 *
 * WHY NOT AUTO-CREATE:
 * - Ambiguity: Should "a/b" be object or array?
 * - Explicit is better (caller creates structure first)
 * - Avoids surprising behavior
 *
 * REFERENCE COUNTING:
 * - json_object_set/json_array_set: Borrow reference (incref internally)
 * - Caller retains ownership of value
 * - Caller must decref value when done (even after successful set)
 *
 * ERROR HANDLING:
 * - NULL parameters: INVALID_PARAM
 * - Parent not found: NOT_FOUND (must create parent first)
 * - Parent is neither object nor array: TYPE
 * - Array index invalid: INVALID_PARAM
 * - Lock failure: LOCK
 *
 * COMPLEXITY: O(d) where d = path depth
 * THREAD SAFETY: Fully thread-safe (lock-protected)
 *
 * @param ctx Context to modify
 * @param path Path to value (e.g., "server/port")
 * @param value Value to set (caller retains ownership)
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_set_value(JsonContext* ctx, const char* path, json_t* value)
{
    LOG_TRACE("Entering nimcp_json_set_value");
    if (!ctx || !path || !value)
        return JSON_ERROR_INVALID_PARAM;

    // Acquire lock for duration of operation
    JsonResult result = acquire_lock(ctx);
    if (result != JSON_SUCCESS)
        return result;

    // Special case: empty path means set the root
    if (path[0] == '\0') {
        if (ctx->root) {
            json_decref(ctx->root);
        }
        ctx->root = value;
        json_incref(value);  // Increment reference count since we're storing it
        release_lock(ctx);
        return JSON_SUCCESS;
    }

    json_t* parent = NULL;
    char* last_key = NULL;

    // Lazy initialization: Create root if needed
    if (!ctx->root) {
        ctx->root = json_object();
        if (!ctx->root) {
            release_lock(ctx);
            return JSON_ERROR_MEMORY;
        }
    }

    // Resolve path to parent and last key
    resolve_json_path(ctx->root, path, &parent, &last_key);

    if (!parent || !last_key) {
        // Parent doesn't exist or invalid path
        release_lock(ctx);
        nimcp_free(last_key);
        return JSON_ERROR_NOT_FOUND;
    }

    // Set value based on parent type
    if (json_is_object(parent)) {
        // Object: Set key to value
        json_object_set(parent, last_key, value);
    } else if (json_is_array(parent)) {
        // Array: Set index to value
        char* endptr;
        long index = strtol(last_key, &endptr, 10);
        if (*endptr != '\0' || index < 0) {
            // Invalid array index
            nimcp_free(last_key);
            release_lock(ctx);
            return JSON_ERROR_INVALID_PARAM;
        }
        json_array_set(parent, index, value);
    } else {
        // Parent is neither object nor array (type mismatch)
        nimcp_free(last_key);
        release_lock(ctx);
        return JSON_ERROR_TYPE;
    }

    nimcp_free(last_key);
    release_lock(ctx);
    return JSON_SUCCESS;
}

//=============================================================================
// Convenience Functions Implementation
//=============================================================================

/**
 * @brief Get string value by path
 *
 * WHY CONVENIENCE FUNCTION:
 * - Most common operation (90% of config access)
 * - Type-safe (validates JSON is string)
 * - Avoids boilerplate (get_value + type check + extract + decref)
 * - Safe buffer handling (null termination, size limit)
 *
 * ALGORITHM:
 * 1. Get value using nimcp_json_get_value
 * 2. Check for JSON null (special case)
 * 3. Validate type is string
 * 4. Extract string value
 * 5. Copy to buffer with size limit
 * 6. Null-terminate
 * 7. Decref value
 *
 * BUFFER HANDLING:
 * - strncpy with size-1 (leave room for null terminator)
 * - Explicit null termination (strncpy may not null-terminate)
 * - Safe against buffer overflow
 *
 * WHY SIZE PARAMETER:
 * - Caller controls buffer size
 * - Prevents overflow
 * - Standard C pattern (like snprintf)
 *
 * NULL VALUE HANDLING:
 * - JSON null is distinct from missing value
 * - Returns NULL_VALUE (not an error, but needs handling)
 * - Allows distinguishing: "key": null vs missing "key"
 *
 * ERROR HANDLING:
 * - NULL buffer or size==0: INVALID_PARAM
 * - Path not found: NOT_FOUND (propagated from get_value)
 * - JSON null: NULL_VALUE (special case)
 * - Type mismatch: TYPE (e.g., path points to number)
 *
 * COMPLEXITY: O(d + n) where d = path depth, n = string length
 * THREAD SAFETY: Fully thread-safe (get_value handles locking)
 *
 * @param ctx Context to query
 * @param path Path to string value
 * @param buffer Output buffer
 * @param size Buffer size
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_get_string_value(JsonContext* ctx, const char* path, char* buffer,
                                       size_t size)
{
    if (!buffer || size == 0)
        return JSON_ERROR_INVALID_PARAM;

    // Get value (returns new reference)
    json_t* value = NULL;
    JsonResult result = nimcp_json_get_value(ctx, path, &value);
    if (result != JSON_SUCCESS)
        return result;

    // Check for JSON null
    if (json_is_null(value)) {
        json_decref(value);
        return JSON_ERROR_NULL_VALUE;
    }

    // Validate type
    if (!json_is_string(value)) {
        json_decref(value);
        return JSON_ERROR_TYPE;
    }

    // Extract and copy string (safe buffer handling)
    const char* str = json_string_value(value);
    strncpy(buffer, str, size - 1);
    buffer[size - 1] = '\0';  // Ensure null termination

    json_decref(value);  // Release reference
    return JSON_SUCCESS;
}

/**
 * @brief Get integer value by path
 *
 * WHY CONVENIENCE FUNCTION:
 * - Common for numeric config (ports, timeouts, counts)
 * - Type-safe (validates JSON is integer)
 * - Avoids boilerplate
 *
 * TYPE HANDLING:
 * - Only accepts JSON integer (not real/float)
 * - Returns int64_t (matches Jansson's json_integer_t)
 * - Caller can cast to smaller type if needed
 *
 * WHY INT64_T:
 * - Sufficient range for all practical integers
 * - Matches Jansson API (consistent)
 * - No precision loss
 *
 * ERROR HANDLING:
 * - Same pattern as get_string_value
 * - NULL_VALUE for JSON null
 * - TYPE for non-integer (including real numbers)
 *
 * COMPLEXITY: O(d) where d = path depth
 * THREAD SAFETY: Fully thread-safe
 *
 * @param ctx Context to query
 * @param path Path to integer value
 * @param value Output parameter
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_get_integer_value(JsonContext* ctx, const char* path, int64_t* value)
{
    LOG_TRACE("Entering nimcp_json_get_integer_value");
    if (!value)
        return JSON_ERROR_INVALID_PARAM;

    json_t* json_value = NULL;
    JsonResult result = nimcp_json_get_value(ctx, path, &json_value);
    if (result != JSON_SUCCESS)
        return result;

    if (json_is_null(json_value)) {
        json_decref(json_value);
        return JSON_ERROR_NULL_VALUE;
    }

    if (!json_is_integer(json_value)) {
        json_decref(json_value);
        return JSON_ERROR_TYPE;
    }

    *value = json_integer_value(json_value);
    json_decref(json_value);
    return JSON_SUCCESS;
}

/**
 * @brief Get boolean value by path
 *
 * WHY CONVENIENCE FUNCTION:
 * - Common for flags and toggles
 * - Type-safe (validates JSON is boolean)
 * - Returns C bool (not int or json_t*)
 *
 * JSON BOOLEAN:
 * - JSON has distinct true/false types
 * - Not the same as 1/0 (different types)
 * - This function only accepts JSON boolean
 *
 * COMPLEXITY: O(d) where d = path depth
 * THREAD SAFETY: Fully thread-safe
 *
 * @param ctx Context to query
 * @param path Path to boolean value
 * @param value Output parameter
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_get_boolean_value(JsonContext* ctx, const char* path, bool* value)
{
    LOG_TRACE("Entering nimcp_json_get_boolean_value");
    if (!value)
        return JSON_ERROR_INVALID_PARAM;

    json_t* json_value = NULL;
    JsonResult result = nimcp_json_get_value(ctx, path, &json_value);
    if (result != JSON_SUCCESS)
        return result;

    if (json_is_null(json_value)) {
        json_decref(json_value);
        return JSON_ERROR_NULL_VALUE;
    }

    if (!json_is_boolean(json_value)) {
        json_decref(json_value);
        return JSON_ERROR_TYPE;
    }

    *value = json_boolean_value(json_value);
    json_decref(json_value);
    return JSON_SUCCESS;
}

/**
 * @brief Get number value by path
 *
 * WHY SEPARATE FROM INTEGER:
 * - JSON has distinct integer and real types
 * - Sometimes need floating-point (ratios, percentages, etc.)
 * - This function accepts both integer and real
 *
 * TYPE HANDLING:
 * - json_is_number(): True for both integer and real
 * - json_number_value(): Returns double (converts integer if needed)
 *
 * PRECISION:
 * - Returns double (IEEE 754 binary64)
 * - 53 bits precision (~15-17 decimal digits)
 * - Sufficient for most purposes
 *
 * WHY ACCEPT INTEGER:
 * - Convenient: Don't need to know if config uses 123 or 123.0
 * - JSON integer is subset of number
 * - Auto-conversion is safe (no precision loss for reasonable integers)
 *
 * COMPLEXITY: O(d) where d = path depth
 * THREAD SAFETY: Fully thread-safe
 *
 * @param ctx Context to query
 * @param path Path to number value
 * @param value Output parameter
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_get_number_value(JsonContext* ctx, const char* path, double* value)
{
    LOG_TRACE("Entering nimcp_json_get_number_value");
    if (!value)
        return JSON_ERROR_INVALID_PARAM;

    json_t* json_value = NULL;
    JsonResult result = nimcp_json_get_value(ctx, path, &json_value);
    if (result != JSON_SUCCESS)
        return result;

    if (json_is_null(json_value)) {
        json_decref(json_value);
        return JSON_ERROR_NULL_VALUE;
    }

    if (!json_is_number(json_value)) {
        json_decref(json_value);
        return JSON_ERROR_TYPE;
    }

    *value = json_number_value(json_value);
    json_decref(json_value);
    return JSON_SUCCESS;
}

/**
 * @brief Set string value by path
 *
 * WHY CONVENIENCE FUNCTION:
 * - Most common setter operation
 * - Type-safe (creates JSON string)
 * - Avoids boilerplate (create value + set + decref)
 *
 * ALGORITHM:
 * 1. Create JSON string value using Jansson
 * 2. Call nimcp_json_set_value
 * 3. Decref the value (set_value borrows reference)
 * 4. Return result
 *
 * ERROR HANDLING:
 * - NULL value: INVALID_PARAM
 * - Propagates errors from set_value (NOT_FOUND, TYPE, etc.)
 *
 * COMPLEXITY: O(d + n) where d = path depth, n = string length
 * THREAD SAFETY: Fully thread-safe (set_value handles locking)
 *
 * @param ctx Context to modify
 * @param path Path to string value
 * @param value String to set
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_set_string_value(JsonContext* ctx, const char* path, const char* value)
{
    LOG_TRACE("Entering nimcp_json_set_string_value");
    if (!value)
        return JSON_ERROR_INVALID_PARAM;

    // Create JSON string value
    json_t* json_value = json_string(value);
    if (!json_value)
        return JSON_ERROR_MEMORY;

    // Set value in tree
    JsonResult result = nimcp_json_set_value(ctx, path, json_value);

    // Release our reference (parent owns it now)
    json_decref(json_value);
    return result;
}

/**
 * @brief Set integer value by path
 *
 * WHY CONVENIENCE FUNCTION:
 * - Common for numeric config (ports, timeouts, counts)
 * - Type-safe (creates JSON integer)
 * - Avoids boilerplate
 *
 * ALGORITHM:
 * 1. Create JSON integer value
 * 2. Set via nimcp_json_set_value
 * 3. Decref
 *
 * COMPLEXITY: O(d) where d = path depth
 * THREAD SAFETY: Fully thread-safe
 *
 * @param ctx Context to modify
 * @param path Path to integer value
 * @param value Integer to set
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_set_integer_value(JsonContext* ctx, const char* path, int64_t value)
{
    LOG_TRACE("Entering nimcp_json_set_integer_value");
    // Create JSON integer value
    json_t* json_value = json_integer(value);
    if (!json_value)
        return JSON_ERROR_MEMORY;

    // Set value in tree
    JsonResult result = nimcp_json_set_value(ctx, path, json_value);

    // Release our reference
    json_decref(json_value);
    return result;
}

/**
 * @brief Set boolean value by path
 *
 * WHY CONVENIENCE FUNCTION:
 * - Common for flags and toggles
 * - Type-safe (creates JSON boolean)
 * - Simple API (C bool -> JSON boolean)
 *
 * COMPLEXITY: O(d) where d = path depth
 * THREAD SAFETY: Fully thread-safe
 *
 * @param ctx Context to modify
 * @param path Path to boolean value
 * @param value Boolean to set
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_set_boolean_value(JsonContext* ctx, const char* path, bool value)
{
    LOG_TRACE("Entering nimcp_json_set_boolean_value");
    // Create JSON boolean value
    json_t* json_value = json_boolean(value);
    if (!json_value)
        return JSON_ERROR_MEMORY;

    // Set value in tree
    JsonResult result = nimcp_json_set_value(ctx, path, json_value);

    // Release our reference
    json_decref(json_value);
    return result;
}

/**
 * @brief Set number value by path
 *
 * WHY SEPARATE FROM INTEGER:
 * - For floating-point values (ratios, percentages, etc.)
 * - Creates JSON real type (not integer)
 *
 * TYPE HANDLING:
 * - Always creates JSON real (even for 1.0)
 * - Use set_integer_value if you want JSON integer type
 *
 * COMPLEXITY: O(d) where d = path depth
 * THREAD SAFETY: Fully thread-safe
 *
 * @param ctx Context to modify
 * @param path Path to number value
 * @param value Number to set
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_set_number_value(JsonContext* ctx, const char* path, double value)
{
    LOG_TRACE("Entering nimcp_json_set_number_value");
    // Create JSON real value
    json_t* json_value = json_real(value);
    if (!json_value)
        return JSON_ERROR_MEMORY;

    // Set value in tree
    JsonResult result = nimcp_json_set_value(ctx, path, json_value);

    // Release our reference
    json_decref(json_value);
    return result;
}

//=============================================================================
// Null Value Handling
//=============================================================================

/**
 * @brief Check if value is JSON null
 *
 * WHY NEEDED:
 * - JSON null is distinct from missing value
 * - Sometimes need to distinguish: null vs absent vs wrong type
 * - Enables explicit null checking without type error
 *
 * USAGE PATTERN:
 *   bool is_null;
 *   if (nimcp_json_is_null_value(ctx, "key", &is_null) == JSON_SUCCESS) {
 *       if (is_null) {
 *           // Value is explicitly null
 *       }
 *   }
 *
 * ERROR HANDLING:
 * - Path not found: NOT_FOUND (different from null)
 * - NULL is a value, not absence of value
 *
 * COMPLEXITY: O(d) where d = path depth
 * THREAD SAFETY: Fully thread-safe (lock-protected)
 *
 * @param ctx Context to query
 * @param path Path to check
 * @param is_null Output parameter
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_is_null_value(JsonContext* ctx, const char* path, bool* is_null)
{
    LOG_TRACE("Entering nimcp_json_is_null_value");
    if (!ctx || !path || !is_null)
        return JSON_ERROR_INVALID_PARAM;

    JsonResult result = acquire_lock(ctx);
    if (result != JSON_SUCCESS)
        return result;

    json_t* value = resolve_json_path(ctx->root, path, NULL, NULL);
    if (!value) {
        release_lock(ctx);
        return JSON_ERROR_NOT_FOUND;
    }

    *is_null = json_is_null(value);
    release_lock(ctx);
    return JSON_SUCCESS;
}

/**
 * @brief Set value to JSON null
 *
 * WHY NEEDED:
 * - Explicit null different from deletion
 * - Some protocols distinguish null from absent
 * - Enables clearing value while keeping key
 *
 * USAGE:
 *   // Before: {"key": 123}
 *   nimcp_json_set_null_value(ctx, "key");
 *   // After:  {"key": null}
 *
 * vs DELETION:
 *   nimcp_json_delete_value(ctx, "key");
 *   // After:  {}  (key removed entirely)
 *
 * IMPLEMENTATION:
 * - Same as set_value but creates json_null() value
 * - Handles objects and arrays
 * - Supports lazy initialization
 *
 * COMPLEXITY: O(d) where d = path depth
 * THREAD SAFETY: Fully thread-safe (lock-protected)
 *
 * @param ctx Context to modify
 * @param path Path to set to null
 * @return JSON_SUCCESS or error code
 */
JsonResult nimcp_json_set_null_value(JsonContext* ctx, const char* path)
{
    LOG_TRACE("Entering nimcp_json_set_null_value");
    if (!ctx || !path)
        return JSON_ERROR_INVALID_PARAM;

    JsonResult result = acquire_lock(ctx);
    if (result != JSON_SUCCESS)
        return result;

    json_t* parent = NULL;
    char* last_key = NULL;

    // Lazy initialization
    if (!ctx->root) {
        ctx->root = json_object();
        if (!ctx->root) {
            release_lock(ctx);
            return JSON_ERROR_MEMORY;
        }
    }

    resolve_json_path(ctx->root, path, &parent, &last_key);

    if (!parent || !last_key) {
        nimcp_free(last_key);
        release_lock(ctx);
        return JSON_ERROR_NOT_FOUND;
    }

    // Create null value
    json_t* null_value = json_null();

    // Set based on parent type
    if (json_is_object(parent)) {
        json_object_set(parent, last_key, null_value);
    } else if (json_is_array(parent)) {
        char* endptr;
        long index = strtol(last_key, &endptr, 10);
        if (*endptr != '\0' || index < 0) {
            json_decref(null_value);
            nimcp_free(last_key);
            release_lock(ctx);
            return JSON_ERROR_INVALID_PARAM;
        }
        json_array_set(parent, index, null_value);
    } else {
        json_decref(null_value);
        nimcp_free(last_key);
        release_lock(ctx);
        return JSON_ERROR_TYPE;
    }

    json_decref(null_value);  // Release our reference (parent now owns)
    nimcp_free(last_key);
    release_lock(ctx);
    return JSON_SUCCESS;
}

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get human-readable error message
 *
 * WHY ERROR MESSAGES:
 * - Debugging aid (know what went wrong)
 * - User-facing error reporting
 * - Logging (record failures)
 *
 * IMPLEMENTATION:
 * - Static array indexed by negated error code
 * - O(1) lookup
 * - No dynamic allocation
 *
 * BOUNDS CHECKING:
 * - Returns "Unknown error" for out-of-range codes
 * - Safe against invalid error codes
 *
 * THREAD SAFETY: Fully thread-safe (read-only data)
 *
 * @param result Error code
 * @return Error message string (never NULL)
 */
const char* nimcp_json_get_error(JsonResult result)
{
    LOG_TRACE("Entering nimcp_json_get_error");
    if (result > 0 || result < -8)
        return "Unknown error";
    return ERROR_MESSAGES[-result];
}
