/**
 * @file nimcp_source_cache.h
 * @brief Source file cache for self-healing fault tolerance system
 *
 * WHAT: Memory-mapped source file cache with line indexing
 * WHY: Fast access to source code for diagnostics, debugging, and self-healing
 * HOW: mmap source files, build line offset index, reader-writer lock protection
 *
 * ARCHITECTURE:
 *
 *   Application              Source Cache                   Filesystem
 *   ┌─────────────┐         ┌─────────────────┐            ┌─────────────┐
 *   │ Get Lines   │────────>│ Hash Table      │            │ Source File │
 *   │ Get Source  │         │ ┌─────────────┐ │   mmap     │  .c/.h      │
 *   └─────────────┘         │ │ file.c      │─┼───────────>│             │
 *                           │ │  mmap_addr  │ │            └─────────────┘
 *                           │ │  line_offs  │ │
 *                           │ └─────────────┘ │
 *                           │ ┌─────────────┐ │
 *                           │ │ other.h     │ │
 *                           │ └─────────────┘ │
 *                           └─────────────────┘
 *
 * FEATURES:
 * - Memory-mapped files for fast, zero-copy access
 * - Line offset indexing for O(1) line retrieval
 * - File modification tracking (mtime-based)
 * - Thread-safe with reader-writer locks (many readers, one writer)
 * - Automatic cache invalidation on file changes
 * - Configurable source root directory
 *
 * PERFORMANCE:
 * - Initial load: O(n) where n = file size (mmap + line scanning)
 * - Get line: O(1) after indexing
 * - Get range: O(lines) copy
 * - Memory: file_size + (4 bytes * line_count) per file
 *
 * USAGE:
 *   source_cache_t cache = source_cache_create("/path/to/nimcp");
 *
 *   // Get specific lines (1-indexed)
 *   char buffer[4096];
 *   if (source_cache_get_lines(cache, "src/core/brain.c", 100, 110, buffer, sizeof(buffer))) {
 *       printf("Lines 100-110:\n%s", buffer);
 *   }
 *
 *   // Get function source
 *   char* source = source_cache_get_function_source(cache, "file.c", 50, 75);
 *   if (source) {
 *       printf("Function:\n%s", source);
 *       nimcp_free(source);
 *   }
 *
 *   // Invalidate if file changed externally
 *   source_cache_invalidate(cache, "file.c");
 *
 *   source_cache_destroy(cache);
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SOURCE_CACHE_H
#define NIMCP_SOURCE_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "utils/platform/nimcp_platform_rwlock.h"

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

/** Maximum filename path length */
#define NIMCP_SOURCE_CACHE_MAX_PATH 512

/** Maximum source root path length */
#define NIMCP_SOURCE_CACHE_MAX_ROOT 256

/** Default hash table size for file entries */
#define NIMCP_SOURCE_CACHE_DEFAULT_BUCKETS 64

/** Maximum files to cache (0 = unlimited) */
#define NIMCP_SOURCE_CACHE_DEFAULT_MAX_FILES 0

/** Maximum total memory for cached files (0 = unlimited) */
#define NIMCP_SOURCE_CACHE_DEFAULT_MAX_MEMORY 0

/** Magic value for cache validation */
#define NIMCP_SOURCE_CACHE_MAGIC 0x53524343  /* 'SRCC' */

/* ============================================================================
 * TYPE DEFINITIONS
 * ============================================================================ */

/**
 * @brief Source cache opaque handle
 */
typedef struct nimcp_source_cache* source_cache_t;

/**
 * @brief Source file entry
 *
 * WHAT: Cached source file with line indexing
 * WHY: Fast line-based access to source files
 * HOW: mmap file content, build line offset table
 */
typedef struct {
    char filename[NIMCP_SOURCE_CACHE_MAX_PATH];  /**< Relative filename */
    void* mmap_addr;                              /**< Memory-mapped file address */
    size_t file_size;                             /**< File size in bytes */
    time_t mtime;                                 /**< Last modification time */
    uint32_t line_count;                          /**< Total number of lines */
    uint32_t* line_offsets;                       /**< Offset to each line start */
    bool modified;                                /**< File modified since cache */
    nimcp_platform_rwlock_t lock;                 /**< Per-entry reader-writer lock */
} source_file_entry_t;

/**
 * @brief Source cache configuration
 */
typedef struct {
    size_t max_files;           /**< Maximum files to cache (0 = unlimited) */
    size_t max_memory;          /**< Maximum memory usage (0 = unlimited) */
    bool auto_refresh;          /**< Auto-refresh on access if file changed */
    bool preload_extensions;    /**< Preload files with specified extensions */
    const char* extensions;     /**< Comma-separated extensions to preload (e.g., ".c,.h") */
} source_cache_config_t;

/**
 * @brief Source cache statistics
 */
typedef struct {
    uint64_t files_cached;          /**< Number of files currently cached */
    uint64_t total_file_size;       /**< Total size of cached files */
    uint64_t total_lines;           /**< Total lines across all files */
    uint64_t cache_hits;            /**< Successful cache lookups */
    uint64_t cache_misses;          /**< Cache misses (file not cached) */
    uint64_t refresh_count;         /**< Times files were refreshed */
    uint64_t invalidation_count;    /**< Times files were invalidated */
    uint64_t memory_used;           /**< Total memory used by cache */
} source_cache_stats_t;

/* ============================================================================
 * CACHE LIFECYCLE
 * ============================================================================ */

/**
 * @brief Create source cache
 *
 * WHAT: Initialize source cache with root directory
 * WHY: Set up caching infrastructure for source files
 * HOW: Allocate hash table, set root, initialize locks
 *
 * @param source_root Root directory for source files (NULL for cwd)
 * @return Cache handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (after creation)
 *
 * @note Call source_cache_destroy() to clean up
 */
NIMCP_EXPORT source_cache_t source_cache_create(const char* source_root);

/**
 * @brief Create source cache with configuration
 *
 * WHAT: Initialize source cache with custom configuration
 * WHY: Allow tuning cache behavior
 * HOW: Create cache and apply config
 *
 * @param source_root Root directory for source files
 * @param config Configuration options (NULL for defaults)
 * @return Cache handle or NULL on failure
 */
NIMCP_EXPORT source_cache_t source_cache_create_with_config(
    const char* source_root,
    const source_cache_config_t* config
);

/**
 * @brief Destroy source cache
 *
 * WHAT: Free all cache resources
 * WHY: Clean shutdown
 * HOW: Unmap all files, free entries, destroy locks
 *
 * @param cache Cache handle
 *
 * COMPLEXITY: O(n) where n = cached files
 */
NIMCP_EXPORT void source_cache_destroy(source_cache_t cache);

/**
 * @brief Get default configuration
 *
 * @return Default configuration values
 */
NIMCP_EXPORT source_cache_config_t source_cache_default_config(void);

/* ============================================================================
 * FILE ACCESS
 * ============================================================================ */

/**
 * @brief Get cached file entry
 *
 * WHAT: Get mmap'd file content
 * WHY: Access file data without reading from disk
 * HOW: Lookup in hash table, mmap if not cached
 *
 * @param cache Cache handle
 * @param filename Relative or absolute filename
 * @return File entry or NULL if not found/failed
 *
 * COMPLEXITY: O(1) lookup + O(n) mmap on cache miss
 * THREAD-SAFE: Yes (reader lock)
 *
 * @note Returns pointer to internal entry - do not modify or free
 * @note Entry valid until cache destroyed or file invalidated
 */
NIMCP_EXPORT const source_file_entry_t* source_cache_get_file(
    source_cache_t cache,
    const char* filename
);

/**
 * @brief Get specific lines from file
 *
 * WHAT: Extract lines from cached source file
 * WHY: Get context around error location
 * HOW: Use line offset table for O(1) line start lookup
 *
 * @param cache Cache handle
 * @param filename Relative or absolute filename
 * @param start_line First line to get (1-indexed)
 * @param end_line Last line to get (inclusive, 0 = to end of file)
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or 0 on failure
 *
 * COMPLEXITY: O(lines) copy
 * THREAD-SAFE: Yes (reader lock)
 *
 * @note Lines are 1-indexed (first line is 1)
 * @note Output is null-terminated
 * @note Truncates if buffer too small
 */
NIMCP_EXPORT size_t source_cache_get_lines(
    source_cache_t cache,
    const char* filename,
    uint32_t start_line,
    uint32_t end_line,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Get single line from file
 *
 * WHAT: Extract single line from cached source file
 * WHY: Get specific line for error display
 * HOW: Use line offset table
 *
 * @param cache Cache handle
 * @param filename Relative or absolute filename
 * @param line_number Line to get (1-indexed)
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Length of line (may exceed buffer_size if truncated), 0 on failure
 *
 * COMPLEXITY: O(1) lookup + O(line_length) copy
 * THREAD-SAFE: Yes (reader lock)
 */
NIMCP_EXPORT size_t source_cache_get_line(
    source_cache_t cache,
    const char* filename,
    uint32_t line_number,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Get function source code
 *
 * WHAT: Extract function source between lines
 * WHY: Get complete function for analysis/display
 * HOW: Get lines and return newly allocated string
 *
 * @param cache Cache handle
 * @param filename Relative or absolute filename
 * @param start_line Function start line (1-indexed)
 * @param end_line Function end line (inclusive)
 * @return Newly allocated string with function source, or NULL on failure
 *
 * COMPLEXITY: O(lines) allocation and copy
 * THREAD-SAFE: Yes (reader lock)
 *
 * @note Caller must free returned string with nimcp_free()
 */
NIMCP_EXPORT char* source_cache_get_function_source(
    source_cache_t cache,
    const char* filename,
    uint32_t start_line,
    uint32_t end_line
);

/**
 * @brief Get line count for file
 *
 * WHAT: Get total number of lines in cached file
 * WHY: Validate line ranges, display progress
 * HOW: Return cached line_count
 *
 * @param cache Cache handle
 * @param filename Relative or absolute filename
 * @return Line count, or 0 if file not cached
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (reader lock)
 */
NIMCP_EXPORT uint32_t source_cache_get_line_count(
    source_cache_t cache,
    const char* filename
);

/**
 * @brief Get file size
 *
 * WHAT: Get size of cached file in bytes
 * WHY: Memory estimation, progress display
 * HOW: Return cached file_size
 *
 * @param cache Cache handle
 * @param filename Relative or absolute filename
 * @return File size in bytes, or 0 if not cached
 */
NIMCP_EXPORT size_t source_cache_get_file_size(
    source_cache_t cache,
    const char* filename
);

/* ============================================================================
 * CACHE MANAGEMENT
 * ============================================================================ */

/**
 * @brief Mark file as modified
 *
 * WHAT: Flag file as externally modified
 * WHY: Track files that may need refresh
 * HOW: Set modified flag
 *
 * @param cache Cache handle
 * @param filename Relative or absolute filename
 * @return true if file was cached and marked
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (writer lock)
 */
NIMCP_EXPORT bool source_cache_mark_modified(
    source_cache_t cache,
    const char* filename
);

/**
 * @brief Invalidate cache entry
 *
 * WHAT: Remove file from cache
 * WHY: Force re-read on next access
 * HOW: Unmap file, free entry, remove from hash table
 *
 * @param cache Cache handle
 * @param filename Relative or absolute filename
 * @return true if file was cached and removed
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (writer lock)
 */
NIMCP_EXPORT bool source_cache_invalidate(
    source_cache_t cache,
    const char* filename
);

/**
 * @brief Refresh cached file
 *
 * WHAT: Re-mmap file if modified
 * WHY: Get updated content without full invalidation
 * HOW: Check mtime, re-mmap if changed
 *
 * @param cache Cache handle
 * @param filename Relative or absolute filename
 * @return true if file was refreshed or unchanged
 *
 * COMPLEXITY: O(1) stat + O(n) mmap if changed
 * THREAD-SAFE: Yes (writer lock)
 *
 * @note Returns true if file is current (whether refreshed or not)
 * @note Returns false on error (file not found, mmap failed)
 */
NIMCP_EXPORT bool source_cache_refresh(
    source_cache_t cache,
    const char* filename
);

/**
 * @brief Refresh all modified files
 *
 * WHAT: Refresh all files marked as modified
 * WHY: Bulk refresh after external changes
 * HOW: Iterate entries, refresh if modified flag set
 *
 * @param cache Cache handle
 * @return Number of files refreshed
 *
 * COMPLEXITY: O(n) where n = cached files
 * THREAD-SAFE: Yes (writer lock per file)
 */
NIMCP_EXPORT uint32_t source_cache_refresh_all_modified(source_cache_t cache);

/**
 * @brief Check if file is cached
 *
 * WHAT: Check if file exists in cache
 * WHY: Determine if access will require disk read
 * HOW: Hash table lookup
 *
 * @param cache Cache handle
 * @param filename Relative or absolute filename
 * @return true if file is cached
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (reader lock)
 */
NIMCP_EXPORT bool source_cache_is_cached(
    source_cache_t cache,
    const char* filename
);

/**
 * @brief Check if file needs refresh
 *
 * WHAT: Check if cached file is outdated
 * WHY: Determine if refresh is needed
 * HOW: Compare mtime with filesystem
 *
 * @param cache Cache handle
 * @param filename Relative or absolute filename
 * @return true if file modified since cached
 *
 * COMPLEXITY: O(1) stat
 * THREAD-SAFE: Yes (reader lock)
 */
NIMCP_EXPORT bool source_cache_needs_refresh(
    source_cache_t cache,
    const char* filename
);

/**
 * @brief Clear entire cache
 *
 * WHAT: Remove all cached files
 * WHY: Memory cleanup, reset state
 * HOW: Iterate all entries, unmap and free
 *
 * @param cache Cache handle
 *
 * COMPLEXITY: O(n) where n = cached files
 * THREAD-SAFE: Yes (writer lock)
 */
NIMCP_EXPORT void source_cache_clear(source_cache_t cache);

/* ============================================================================
 * STATISTICS AND DIAGNOSTICS
 * ============================================================================ */

/**
 * @brief Get cache statistics
 *
 * WHAT: Retrieve cache performance metrics
 * WHY: Monitor cache effectiveness
 * HOW: Copy statistics structure
 *
 * @param cache Cache handle
 * @param stats Output statistics structure
 * @return true on success
 *
 * THREAD-SAFE: Yes (reader lock)
 */
NIMCP_EXPORT bool source_cache_get_stats(
    source_cache_t cache,
    source_cache_stats_t* stats
);

/**
 * @brief Reset cache statistics
 *
 * WHAT: Clear all counters
 * WHY: Start fresh measurement period
 * HOW: Zero statistics (preserve current state counts)
 *
 * @param cache Cache handle
 */
NIMCP_EXPORT void source_cache_reset_stats(source_cache_t cache);

/**
 * @brief Print cache statistics
 *
 * WHAT: Display cache info to stdout
 * WHY: Debugging and monitoring
 * HOW: Format and print stats
 *
 * @param cache Cache handle
 */
NIMCP_EXPORT void source_cache_print_stats(source_cache_t cache);

/**
 * @brief Get source root path
 *
 * WHAT: Get configured source root directory
 * WHY: Resolve relative paths
 * HOW: Return stored root path
 *
 * @param cache Cache handle
 * @return Source root path or NULL
 */
NIMCP_EXPORT const char* source_cache_get_root(source_cache_t cache);

/**
 * @brief Validate cache integrity
 *
 * WHAT: Check internal data structure consistency
 * WHY: Debugging and testing
 * HOW: Verify hash table, entries, locks
 *
 * @param cache Cache handle
 * @return true if cache is consistent
 */
NIMCP_EXPORT bool source_cache_validate(source_cache_t cache);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOURCE_CACHE_H */
