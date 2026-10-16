#include <stddef.h>  /* for NULL */
// Must define _GNU_SOURCE before any includes for dl_iterate_phdr
#ifdef __linux__
    #define _GNU_SOURCE
#endif

//=============================================================================
// nimcp_fn_dispatch.c - Function Dispatch Table Implementation
//=============================================================================
/**
 * @file nimcp_fn_dispatch.c
 * @brief Hot-swappable function dispatch with versioning and quarantine
 *
 * WHAT: Thread-safe function dispatch table with atomic swaps
 * WHY:  Enable live patching, A/B testing, and fault isolation
 * HOW:  Hash table with RW locks, version history, crash tracking
 *
 * DESIGN DECISIONS:
 * - Linear search for simplicity (O(n) but typically <1000 entries)
 * - RW locks per-entry for fine-grained concurrency
 * - Global mutex for table modifications (add/remove)
 * - Auto-quarantine based on crash count threshold
 *
 * PLATFORM NOTES:
 * - Auto-registration only works on Linux (ELF parsing via dl_iterate_phdr)
 * - On other platforms, use manual registration
 *
 * SRP: This module handles ONLY function dispatch table management
 *
 * @author NIMCP Development Team
 * @date 2025-12-27
 * @version 1.0.0
 */

#include "utils/dispatch/nimcp_fn_dispatch.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

// Platform-specific includes for auto-registration
#ifdef __linux__
    #include <dlfcn.h>
    #include <link.h>
    #include <elf.h>
#include "utils/thread/nimcp_thread.h"
#endif

//=============================================================================
// Module Constants
//=============================================================================

#define LOG_MODULE "fn_dispatch"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fn_dispatch)

//=============================================================================
// Internal Helper Declarations
//=============================================================================

static fn_dispatch_entry_t* find_entry(fn_dispatch_table_t* table, const char* name);
static int grow_table(fn_dispatch_table_t* table);
static int grow_history(fn_dispatch_entry_t* entry);
static int init_entry(fn_dispatch_entry_t* entry, const char* name, void* fn_ptr);
static void destroy_entry(fn_dispatch_entry_t* entry);

//=============================================================================
// Global Dispatch Table (Singleton)
//=============================================================================

static fn_dispatch_table_t* g_dispatch_table = NULL;
static pthread_once_t g_dispatch_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t g_dispatch_mutex = NIMCP_MUTEX_INITIALIZER;

static void init_global_dispatch(void)
{
    g_dispatch_table = fn_dispatch_create();
    if (g_dispatch_table) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Global dispatch table initialized");
    } else {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to create global dispatch table");
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT fn_dispatch_table_t* fn_dispatch_create(void)
{
    fn_dispatch_table_t* table = nimcp_calloc(1, sizeof(fn_dispatch_table_t));
    if (!table) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate dispatch table");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "table is NULL");

        return NULL;
    }

    // Initialize table mutex
    if (nimcp_mutex_init(&table->table_lock, NULL) != 0) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to initialize table mutex");
        nimcp_free(table);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "fn_dispatch_create: validation failed");
        return NULL;
    }

    // Allocate initial entries array
    table->capacity = FN_DISPATCH_DEFAULT_CAPACITY;
    table->entries = nimcp_calloc(table->capacity, sizeof(fn_dispatch_entry_t));
    if (!table->entries) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate entries array");
        nimcp_mutex_destroy(&table->table_lock);
        nimcp_free(table);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fn_dispatch_create: table->entries is NULL");
        return NULL;
    }

    table->count = 0;
    table->auto_registered = false;
    table->magic = FN_DISPATCH_MAGIC;

    LOG_MODULE_DEBUG(LOG_MODULE, "Created dispatch table with capacity %u", table->capacity);
    return table;
}

NIMCP_EXPORT void fn_dispatch_destroy(fn_dispatch_table_t* table)
{
    if (!table) {
        return;
    }

    if (table->magic != FN_DISPATCH_MAGIC) {
        LOG_MODULE_WARN(LOG_MODULE, "Destroying invalid dispatch table");
        return;
    }

    // Destroy all entries
    if (table->entries) {
        for (uint32_t i = 0; i < table->count; i++) {
            destroy_entry(&table->entries[i]);
        }
        nimcp_free(table->entries);
    }

    // Destroy table mutex
    nimcp_mutex_destroy(&table->table_lock);

    // Invalidate magic before freeing
    table->magic = 0;

    nimcp_free(table);
    LOG_MODULE_DEBUG(LOG_MODULE, "Destroyed dispatch table");
}

//=============================================================================
// Registration Functions
//=============================================================================

#ifdef __linux__

/**
 * @brief Callback for dl_iterate_phdr to find library symbols
 *
 * This is used during auto-registration to iterate over loaded
 * shared objects and find NIMCP_EXPORT symbols.
 */
typedef struct {
    fn_dispatch_table_t* table;
    int count;
    int errors;
} auto_register_context_t;

static int dl_iterate_callback(struct dl_phdr_info* info, size_t size, void* data)
{
    (void)size;
    auto_register_context_t* ctx = (auto_register_context_t*)data;

    // Skip if no name (usually the main executable has empty name)
    if (!info->dlpi_name || info->dlpi_name[0] == '\0') {
        return 0;  // Continue iteration
    }

    // Only process libnimcp libraries
    if (strstr(info->dlpi_name, "nimcp") == NULL) {
        return 0;  // Not our library
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Scanning library: %s", info->dlpi_name);

    // Open the library to get a handle for dlsym
    void* handle = dlopen(info->dlpi_name, RTLD_NOW | RTLD_NOLOAD);
    if (!handle) {
        LOG_MODULE_WARN(LOG_MODULE, "Could not open %s: %s", info->dlpi_name, dlerror());
        return 0;
    }

    // We would need to parse the ELF dynamic symbol table here
    // For now, this is a placeholder that demonstrates the approach
    // Full implementation would use libelf or manual ELF parsing

    dlclose(handle);
    return 0;  // Continue iteration
}

/**
 * @brief Parse ELF symbol table from /proc/self/maps
 *
 * WHAT: Find NIMCP_EXPORT symbols in the current process
 * WHY:  Auto-register all exported functions
 * HOW:  Parse /proc/self/maps to find library, then parse ELF
 */
static int parse_self_symbols(fn_dispatch_table_t* table)
{
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) {
        LOG_MODULE_WARN(LOG_MODULE, "Could not open /proc/self/maps: %s", strerror(errno));
        return FN_DISPATCH_ERR_PARSE;
    }

    char line[NIMCP_ERROR_BUFFER_LARGE];
    int count = 0;

    while (fgets(line, sizeof(line), maps)) {
        // Look for nimcp library mappings
        if (strstr(line, "libnimcp") == NULL) {
            continue;
        }

        // Extract library path from the line
        char* path = strchr(line, '/');
        if (!path) {
            continue;
        }

        // Remove newline
        char* newline = strchr(path, '\n');
        if (newline) {
            *newline = '\0';
        }

        LOG_MODULE_DEBUG(LOG_MODULE, "Found NIMCP library: %s", path);

        // Open the library
        void* handle = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
        if (!handle) {
            handle = dlopen(path, RTLD_NOW);
        }
        if (!handle) {
            LOG_MODULE_WARN(LOG_MODULE, "Could not dlopen %s: %s", path, dlerror());
            continue;
        }

        // Note: Full ELF parsing would go here to enumerate exported symbols
        // For demonstration, we'll register a few known functions

        // This is a simplified approach - in production, you would:
        // 1. Parse the ELF dynamic symbol table
        // 2. Filter for symbols with NIMCP_EXPORT visibility
        // 3. Use dlsym to get function pointers

        dlclose(handle);
    }

    fclose(maps);
    return count > 0 ? FN_DISPATCH_OK : FN_DISPATCH_ERR_PARSE;
}

#endif /* __linux__ */

NIMCP_EXPORT int fn_dispatch_auto_register(fn_dispatch_table_t* table)
{
    if (!table) {
        return FN_DISPATCH_ERR_NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        return FN_DISPATCH_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&table->table_lock);

    if (table->auto_registered) {
        nimcp_mutex_unlock(&table->table_lock);
        LOG_MODULE_DEBUG(LOG_MODULE, "Auto-registration already completed");
        return FN_DISPATCH_OK;
    }

#ifdef __linux__
    // Use dl_iterate_phdr to find all loaded libraries
    auto_register_context_t ctx = {
        .table = table,
        .count = 0,
        .errors = 0
    };

    dl_iterate_phdr(dl_iterate_callback, &ctx);

    // Also try parsing /proc/self/maps for additional coverage
    int result = parse_self_symbols(table);

    table->auto_registered = true;
    nimcp_mutex_unlock(&table->table_lock);

    LOG_MODULE_INFO(LOG_MODULE, "Auto-registration complete: %u functions registered",
                    table->count);

    return (ctx.errors > 0 || result != FN_DISPATCH_OK)
           ? FN_DISPATCH_ERR_PARSE : FN_DISPATCH_OK;
#else
    table->auto_registered = true;
    nimcp_mutex_unlock(&table->table_lock);

    LOG_MODULE_WARN(LOG_MODULE, "Auto-registration not supported on this platform");
    return FN_DISPATCH_ERR_DLOPEN;
#endif
}

NIMCP_EXPORT int fn_dispatch_register(fn_dispatch_table_t* table,
                                       const char* name,
                                       void* fn_ptr)
{
    if (!table || !name) {
        return FN_DISPATCH_ERR_NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        return FN_DISPATCH_ERR_INVALID_STATE;
    }

    if (strlen(name) >= FN_DISPATCH_NAME_MAX) {
        LOG_MODULE_ERROR(LOG_MODULE, "Function name too long: %s", name);
        return FN_DISPATCH_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&table->table_lock);

    // Check if already registered
    if (find_entry(table, name) != NULL) {
        nimcp_mutex_unlock(&table->table_lock);
        LOG_MODULE_WARN(LOG_MODULE, "Function already registered: %s", name);
        return FN_DISPATCH_ERR_ALREADY_EXISTS;
    }

    // Grow table if needed
    if (table->count >= table->capacity) {
        int result = grow_table(table);
        if (result != FN_DISPATCH_OK) {
            nimcp_mutex_unlock(&table->table_lock);
            return result;
        }
    }

    // Initialize new entry
    fn_dispatch_entry_t* entry = &table->entries[table->count];
    int result = init_entry(entry, name, fn_ptr);
    if (result != FN_DISPATCH_OK) {
        nimcp_mutex_unlock(&table->table_lock);
        return result;
    }

    table->count++;
    nimcp_mutex_unlock(&table->table_lock);

    LOG_MODULE_DEBUG(LOG_MODULE, "Registered function: %s", name);
    return FN_DISPATCH_OK;
}

//=============================================================================
// Lookup Functions
//=============================================================================

NIMCP_EXPORT void* fn_dispatch_get(fn_dispatch_table_t* table, const char* name)
{
    if (!table || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fn_dispatch_get: required parameter is NULL (table, name)");
        return NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fn_dispatch_get: fn_dispatch_is_valid is NULL");
        return NULL;
    }

    nimcp_mutex_lock(&table->table_lock);

    fn_dispatch_entry_t* entry = find_entry(table, name);
    if (!entry) {
        nimcp_mutex_unlock(&table->table_lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return NULL;
    }

    // Acquire read lock on entry
    pthread_rwlock_rdlock(&entry->lock);
    nimcp_mutex_unlock(&table->table_lock);

    // Check quarantine status
    if (entry->quarantined) {
        pthread_rwlock_unlock(&entry->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fn_dispatch_get: validation failed");
        return NULL;
    }

    void* ptr = entry->current_ptr;
    pthread_rwlock_unlock(&entry->lock);

    return ptr;
}

NIMCP_EXPORT const fn_dispatch_entry_t* fn_dispatch_get_entry(fn_dispatch_table_t* table,
                                                                const char* name)
{
    if (!table || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fn_dispatch_get_entry: required parameter is NULL (table, name)");
        return NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fn_dispatch_get_entry: fn_dispatch_is_valid is NULL");
        return NULL;
    }

    nimcp_mutex_lock(&table->table_lock);
    fn_dispatch_entry_t* entry = find_entry(table, name);
    nimcp_mutex_unlock(&table->table_lock);

    return entry;
}

//=============================================================================
// Hot-Swap Functions
//=============================================================================

NIMCP_EXPORT int fn_dispatch_swap(fn_dispatch_table_t* table,
                                   const char* name,
                                   void* new_ptr,
                                   void** old_ptr_out)
{
    if (!table || !name) {
        return FN_DISPATCH_ERR_NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        return FN_DISPATCH_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&table->table_lock);

    fn_dispatch_entry_t* entry = find_entry(table, name);
    if (!entry) {
        nimcp_mutex_unlock(&table->table_lock);
        return FN_DISPATCH_ERR_NOT_FOUND;
    }

    // Acquire write lock on entry
    pthread_rwlock_wrlock(&entry->lock);
    nimcp_mutex_unlock(&table->table_lock);

    // Store old pointer for caller
    void* old_ptr = entry->current_ptr;
    if (old_ptr_out) {
        *old_ptr_out = old_ptr;
    }

    // Grow history if needed
    if (entry->patch_count >= entry->patch_capacity) {
        int result = grow_history(entry);
        if (result != FN_DISPATCH_OK) {
            pthread_rwlock_unlock(&entry->lock);
            return result;
        }
    }

    // Save old pointer to history
    entry->patch_history[entry->patch_count++] = old_ptr;

    // Swap to new pointer
    entry->current_ptr = new_ptr;
    entry->version++;

    pthread_rwlock_unlock(&entry->lock);

    LOG_MODULE_INFO(LOG_MODULE, "Swapped %s to version %u", name, entry->version);
    return FN_DISPATCH_OK;
}

NIMCP_EXPORT int fn_dispatch_rollback(fn_dispatch_table_t* table,
                                       const char* name,
                                       uint32_t versions)
{
    if (!table || !name) {
        return FN_DISPATCH_ERR_NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        return FN_DISPATCH_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&table->table_lock);

    fn_dispatch_entry_t* entry = find_entry(table, name);
    if (!entry) {
        nimcp_mutex_unlock(&table->table_lock);
        return FN_DISPATCH_ERR_NOT_FOUND;
    }

    // Acquire write lock on entry
    pthread_rwlock_wrlock(&entry->lock);
    nimcp_mutex_unlock(&table->table_lock);

    // Special case: versions == 0 means rollback to original
    if (versions == 0) {
        entry->current_ptr = entry->original_ptr;
        entry->patch_count = 0;
        entry->version = 0;
        pthread_rwlock_unlock(&entry->lock);
        LOG_MODULE_INFO(LOG_MODULE, "Rolled back %s to original", name);
        return FN_DISPATCH_OK;
    }

    // Check if we have enough history
    if (versions > entry->patch_count) {
        pthread_rwlock_unlock(&entry->lock);
        return FN_DISPATCH_ERR_NO_HISTORY;
    }

    // Pop N versions from history
    for (uint32_t i = 0; i < versions && entry->patch_count > 0; i++) {
        entry->patch_count--;
        entry->current_ptr = entry->patch_history[entry->patch_count];
        entry->version--;
    }

    pthread_rwlock_unlock(&entry->lock);

    LOG_MODULE_INFO(LOG_MODULE, "Rolled back %s by %u versions to v%u",
                    name, versions, entry->version);
    return FN_DISPATCH_OK;
}

//=============================================================================
// Quarantine Functions
//=============================================================================

NIMCP_EXPORT int fn_dispatch_quarantine(fn_dispatch_table_t* table, const char* name)
{
    if (!table || !name) {
        return FN_DISPATCH_ERR_NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        return FN_DISPATCH_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&table->table_lock);

    fn_dispatch_entry_t* entry = find_entry(table, name);
    if (!entry) {
        nimcp_mutex_unlock(&table->table_lock);
        return FN_DISPATCH_ERR_NOT_FOUND;
    }

    pthread_rwlock_wrlock(&entry->lock);
    nimcp_mutex_unlock(&table->table_lock);

    entry->quarantined = true;

    pthread_rwlock_unlock(&entry->lock);

    LOG_MODULE_WARN(LOG_MODULE, "Quarantined function: %s", name);
    return FN_DISPATCH_OK;
}

NIMCP_EXPORT int fn_dispatch_unquarantine(fn_dispatch_table_t* table, const char* name)
{
    if (!table || !name) {
        return FN_DISPATCH_ERR_NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        return FN_DISPATCH_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&table->table_lock);

    fn_dispatch_entry_t* entry = find_entry(table, name);
    if (!entry) {
        nimcp_mutex_unlock(&table->table_lock);
        return FN_DISPATCH_ERR_NOT_FOUND;
    }

    pthread_rwlock_wrlock(&entry->lock);
    nimcp_mutex_unlock(&table->table_lock);

    entry->quarantined = false;
    entry->crash_count = 0;  // Reset crash count on unquarantine

    pthread_rwlock_unlock(&entry->lock);

    LOG_MODULE_INFO(LOG_MODULE, "Unquarantined function: %s", name);
    return FN_DISPATCH_OK;
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT int fn_dispatch_record_crash(fn_dispatch_table_t* table, const char* name)
{
    if (!table || !name) {
        return FN_DISPATCH_ERR_NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        return FN_DISPATCH_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&table->table_lock);

    fn_dispatch_entry_t* entry = find_entry(table, name);
    if (!entry) {
        nimcp_mutex_unlock(&table->table_lock);
        return FN_DISPATCH_ERR_NOT_FOUND;
    }

    pthread_rwlock_wrlock(&entry->lock);
    nimcp_mutex_unlock(&table->table_lock);

    entry->crash_count++;

    // Auto-quarantine if threshold exceeded
    if (entry->crash_count >= FN_DISPATCH_AUTO_QUARANTINE_THRESHOLD) {
        entry->quarantined = true;
        LOG_MODULE_WARN(LOG_MODULE, "Auto-quarantined %s after %lu crashes",
                        name, (unsigned long)entry->crash_count);
    }

    pthread_rwlock_unlock(&entry->lock);

    LOG_MODULE_WARN(LOG_MODULE, "Recorded crash for %s (count: %lu)",
                    name, (unsigned long)entry->crash_count);
    return FN_DISPATCH_OK;
}

NIMCP_EXPORT int fn_dispatch_record_call(fn_dispatch_table_t* table, const char* name)
{
    if (!table || !name) {
        return FN_DISPATCH_ERR_NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        return FN_DISPATCH_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&table->table_lock);

    fn_dispatch_entry_t* entry = find_entry(table, name);
    if (!entry) {
        nimcp_mutex_unlock(&table->table_lock);
        return FN_DISPATCH_ERR_NOT_FOUND;
    }

    // Use atomic increment for call count (lock-free path)
    __atomic_add_fetch(&entry->call_count, 1, __ATOMIC_RELAXED);

    nimcp_mutex_unlock(&table->table_lock);
    return FN_DISPATCH_OK;
}

NIMCP_EXPORT int fn_dispatch_clear_history(fn_dispatch_table_t* table,
                                            const char* name,
                                            uint32_t keep_versions)
{
    if (!table || !name) {
        return FN_DISPATCH_ERR_NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        return FN_DISPATCH_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&table->table_lock);

    fn_dispatch_entry_t* entry = find_entry(table, name);
    if (!entry) {
        nimcp_mutex_unlock(&table->table_lock);
        return FN_DISPATCH_ERR_NOT_FOUND;
    }

    pthread_rwlock_wrlock(&entry->lock);
    nimcp_mutex_unlock(&table->table_lock);

    if (keep_versions >= entry->patch_count) {
        // Nothing to clear
        pthread_rwlock_unlock(&entry->lock);
        return FN_DISPATCH_OK;
    }

    // Shift history to keep only the last N versions
    uint32_t to_remove = entry->patch_count - keep_versions;
    if (keep_versions > 0) {
        memmove(entry->patch_history,
                &entry->patch_history[to_remove],
                keep_versions * sizeof(void*));
    }
    entry->patch_count = keep_versions;

    pthread_rwlock_unlock(&entry->lock);

    LOG_MODULE_DEBUG(LOG_MODULE, "Cleared history for %s, kept %u versions",
                     name, keep_versions);
    return FN_DISPATCH_OK;
}

NIMCP_EXPORT int fn_dispatch_get_stats(fn_dispatch_table_t* table,
                                        fn_dispatch_stats_t* stats)
{
    if (!table || !stats) {
        return FN_DISPATCH_ERR_NULL;
    }

    if (!fn_dispatch_is_valid(table)) {
        return FN_DISPATCH_ERR_INVALID_STATE;
    }

    memset(stats, 0, sizeof(fn_dispatch_stats_t));

    nimcp_mutex_lock(&table->table_lock);

    stats->total_entries = table->count;

    for (uint32_t i = 0; i < table->count; i++) {
        fn_dispatch_entry_t* entry = &table->entries[i];

        pthread_rwlock_rdlock(&entry->lock);

        if (entry->quarantined) {
            stats->quarantined_count++;
        }
        stats->total_calls += entry->call_count;
        stats->total_crashes += entry->crash_count;
        stats->total_swaps += entry->patch_count;

        pthread_rwlock_unlock(&entry->lock);
    }

    nimcp_mutex_unlock(&table->table_lock);

    return FN_DISPATCH_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT bool fn_dispatch_is_valid(const fn_dispatch_table_t* table)
{
    if (!table) {
        return false;
    }
    return table->magic == FN_DISPATCH_MAGIC;
}

NIMCP_EXPORT const char* fn_dispatch_strerror(fn_dispatch_error_t error)
{
    switch (error) {
        case FN_DISPATCH_OK:
            return "Success";
        case FN_DISPATCH_ERR_NULL:
            return "NULL pointer argument";
        case FN_DISPATCH_ERR_NOT_FOUND:
            return "Function not found";
        case FN_DISPATCH_ERR_ALREADY_EXISTS:
            return "Function already registered";
        case FN_DISPATCH_ERR_NO_MEMORY:
            return "Memory allocation failed";
        case FN_DISPATCH_ERR_QUARANTINED:
            return "Function is quarantined";
        case FN_DISPATCH_ERR_NO_HISTORY:
            return "No history for rollback";
        case FN_DISPATCH_ERR_INVALID_STATE:
            return "Invalid table state";
        case FN_DISPATCH_ERR_LOCK_FAILED:
            return "Lock acquisition failed";
        case FN_DISPATCH_ERR_DLOPEN:
            return "dlopen/dlsym failed";
        case FN_DISPATCH_ERR_PARSE:
            return "ELF/symbol parsing failed";
        default:
            return "Unknown error";
    }
}

NIMCP_EXPORT uint32_t fn_dispatch_foreach(fn_dispatch_table_t* table,
                                           fn_dispatch_callback_t callback,
                                           void* user_data)
{
    if (!table || !callback) {
        return 0;
    }

    if (!fn_dispatch_is_valid(table)) {
        return 0;
    }

    nimcp_mutex_lock(&table->table_lock);

    uint32_t count = table->count;
    for (uint32_t i = 0; i < count; i++) {
        fn_dispatch_entry_t* entry = &table->entries[i];

        pthread_rwlock_rdlock(&entry->lock);
        callback(entry, user_data);
        pthread_rwlock_unlock(&entry->lock);
    }

    nimcp_mutex_unlock(&table->table_lock);

    return count;
}

//=============================================================================
// Global Dispatch Table Functions
//=============================================================================

NIMCP_EXPORT fn_dispatch_table_t* fn_dispatch_global(void)
{
    pthread_once(&g_dispatch_once, init_global_dispatch);
    return g_dispatch_table;
}

NIMCP_EXPORT void fn_dispatch_global_destroy(void)
{
    nimcp_mutex_lock(&g_dispatch_mutex);

    if (g_dispatch_table) {
        fn_dispatch_destroy(g_dispatch_table);
        g_dispatch_table = NULL;
    }

    // Reset pthread_once (platform-specific, may need reinit)
    g_dispatch_once = (pthread_once_t)PTHREAD_ONCE_INIT;

    nimcp_mutex_unlock(&g_dispatch_mutex);

    LOG_MODULE_DEBUG(LOG_MODULE, "Global dispatch table destroyed");
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Find entry by name (linear search)
 *
 * NOTE: Caller must hold table_lock
 */
static fn_dispatch_entry_t* find_entry(fn_dispatch_table_t* table, const char* name)
{
    for (uint32_t i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].name, name) == 0) {
            return &table->entries[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_entry: validation failed");
    return NULL;
}

/**
 * @brief Grow table capacity by 2x
 *
 * NOTE: Caller must hold table_lock
 */
static int grow_table(fn_dispatch_table_t* table)
{
    uint32_t new_capacity = table->capacity * 2;
    fn_dispatch_entry_t* new_entries = nimcp_realloc(
        table->entries,
        new_capacity * sizeof(fn_dispatch_entry_t)
    );

    if (!new_entries) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to grow table to %u entries", new_capacity);
        return FN_DISPATCH_ERR_NO_MEMORY;
    }

    // Zero new entries
    memset(&new_entries[table->capacity], 0,
           (new_capacity - table->capacity) * sizeof(fn_dispatch_entry_t));

    table->entries = new_entries;
    table->capacity = new_capacity;

    LOG_MODULE_DEBUG(LOG_MODULE, "Grew table to capacity %u", new_capacity);
    return FN_DISPATCH_OK;
}

/**
 * @brief Grow history array for an entry
 *
 * NOTE: Caller must hold entry write lock
 */
static int grow_history(fn_dispatch_entry_t* entry)
{
    uint32_t new_capacity = entry->patch_capacity == 0
                           ? FN_DISPATCH_HISTORY_INITIAL_CAPACITY
                           : entry->patch_capacity * 2;

    void** new_history = nimcp_realloc(
        entry->patch_history,
        new_capacity * sizeof(void*)
    );

    if (!new_history) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to grow history for %s", entry->name);
        return FN_DISPATCH_ERR_NO_MEMORY;
    }

    entry->patch_history = new_history;
    entry->patch_capacity = new_capacity;

    return FN_DISPATCH_OK;
}

/**
 * @brief Initialize a new entry
 */
static int init_entry(fn_dispatch_entry_t* entry, const char* name, void* fn_ptr)
{
    memset(entry, 0, sizeof(fn_dispatch_entry_t));

    strncpy(entry->name, name, FN_DISPATCH_NAME_MAX - 1);
    entry->name[FN_DISPATCH_NAME_MAX - 1] = '\0';

    entry->current_ptr = fn_ptr;
    entry->original_ptr = fn_ptr;
    entry->patch_history = NULL;
    entry->patch_count = 0;
    entry->patch_capacity = 0;
    entry->version = 0;
    entry->quarantined = false;
    entry->call_count = 0;
    entry->crash_count = 0;

    if (pthread_rwlock_init(&entry->lock, NULL) != 0) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to initialize rwlock for %s", name);
        return FN_DISPATCH_ERR_LOCK_FAILED;
    }

    return FN_DISPATCH_OK;
}

/**
 * @brief Destroy an entry and free its resources
 */
static void destroy_entry(fn_dispatch_entry_t* entry)
{
    if (!entry) {
        return;
    }

    pthread_rwlock_destroy(&entry->lock);

    if (entry->patch_history) {
        nimcp_free(entry->patch_history);
        entry->patch_history = NULL;
    }

    memset(entry, 0, sizeof(fn_dispatch_entry_t));
}

//=============================================================================
// Constructor/Destructor (Library Load/Unload)
//=============================================================================

/**
 * @brief Library constructor - called at library load time
 *
 * WHAT: Initialize global dispatch table automatically
 * WHY:  Enable auto-registration before main() runs
 * HOW:  GCC/Clang constructor attribute
 */
__attribute__((constructor))
static void fn_dispatch_library_init(void)
{
    // Early initialization can be done here if needed
    // Full initialization happens on first use via pthread_once
    LOG_MODULE_DEBUG(LOG_MODULE, "Function dispatch library loaded");
}

/**
 * @brief Library destructor - called at library unload time
 *
 * WHAT: Clean up global dispatch table
 * WHY:  Prevent memory leaks on library unload
 * HOW:  GCC/Clang destructor attribute
 */
__attribute__((destructor))
static void fn_dispatch_library_fini(void)
{
    fn_dispatch_global_destroy();
    LOG_MODULE_DEBUG(LOG_MODULE, "Function dispatch library unloaded");
}
