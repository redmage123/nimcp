/**
 * @file nimcp_source_cache.c
 * @brief Source file cache implementation
 *
 * IMPLEMENTATION NOTES:
 * - Uses mmap for zero-copy file access
 * - Builds line offset table on first access
 * - Reader-writer locks for thread safety
 * - Hash table for O(1) file lookup
 * - mtime tracking for modification detection
 */

#include "utils/code/nimcp_source_cache.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* POSIX headers for mmap and file operations */
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(source_cache)

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Source cache internal structure
 */
struct nimcp_source_cache {
    uint32_t magic;                                     /**< Magic for validation */
    char source_root[NIMCP_SOURCE_CACHE_MAX_ROOT];      /**< Source root directory */
    hash_table_t* file_table;                           /**< Hash table of file entries */
    source_cache_config_t config;                       /**< Cache configuration */
    source_cache_stats_t stats;                         /**< Cache statistics */
    nimcp_platform_mutex_t cache_lock;                  /**< Global cache lock */
    bool initialized;                                   /**< Initialization flag */
};

/* ============================================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Build full path from root and filename
 */
static bool build_full_path(
    const struct nimcp_source_cache* cache,
    const char* filename,
    char* path_out,
    size_t path_size
) {
    if (!filename || !path_out || path_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "build_full_path: required parameter is NULL (filename, path_out)");
        return false;
    }

    /* Check if already absolute path */
    if (filename[0] == '/') {
        size_t len = strlen(filename);
        if (len >= path_size) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_full_path: capacity exceeded");
            return false;
        }
        strncpy(path_out, filename, path_size - 1);
        path_out[path_size - 1] = '\0';
        return true;
    }

    /* Build relative path from root */
    int result = snprintf(path_out, path_size, "%s/%s",
                          cache->source_root[0] ? cache->source_root : ".",
                          filename);

    return (result > 0 && (size_t)result < path_size);
}

/**
 * @brief Get file modification time
 */
static time_t get_file_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return st.st_mtime;
}

/**
 * @brief Get file size
 */
static size_t get_file_size_stat(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return (size_t)st.st_size;
}

/**
 * @brief Build line offset table from mmap'd content
 *
 * Scans file content and records offset of each line start.
 * Line 1 starts at offset 0.
 */
static uint32_t* build_line_offsets(
    const char* content,
    size_t file_size,
    uint32_t* line_count_out
) {
    if (!content || file_size == 0) {
        *line_count_out = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_line_offsets: content is NULL");
        return NULL;
    }

    /* First pass: count lines */
    uint32_t line_count = 1;  /* At least one line */
    for (size_t i = 0; i < file_size; i++) {
        if (content[i] == '\n') {
            line_count++;
        }
    }

    /* Allocate offset table */
    uint32_t* offsets = nimcp_malloc((line_count + 1) * sizeof(uint32_t));
    if (!offsets) {
        *line_count_out = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_line_offsets: offsets is NULL");
        return NULL;
    }

    /* Second pass: record offsets */
    offsets[0] = 0;  /* Line 1 starts at offset 0 */
    uint32_t line_idx = 1;

    for (size_t i = 0; i < file_size && line_idx < line_count; i++) {
        if (content[i] == '\n') {
            offsets[line_idx] = (uint32_t)(i + 1);  /* Next line starts after newline */
            line_idx++;
        }
    }

    /* Sentinel: end of file */
    offsets[line_count] = (uint32_t)file_size;

    *line_count_out = line_count;
    return offsets;
}

/**
 * @brief Memory-map a file
 */
static void* mmap_file(const char* path, size_t* size_out) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        LOG_DEBUG("Failed to open file for mmap: %s (errno=%d)", path, errno);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mmap_file: validation failed");
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        LOG_DEBUG("Failed to stat file: %s (errno=%d)", path, errno);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mmap_file: validation failed");
        return NULL;
    }

    size_t file_size = (size_t)st.st_size;
    if (file_size == 0) {
        close(fd);
        *size_out = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mmap_file: file_size is zero");
        return NULL;
    }

    void* addr = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (addr == MAP_FAILED) {
        LOG_DEBUG("Failed to mmap file: %s (errno=%d)", path, errno);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mmap_file: validation failed");
        return NULL;
    }

    *size_out = file_size;
    return addr;
}

/**
 * @brief Unmap a file
 */
static void munmap_file(void* addr, size_t size) {
    if (addr && size > 0) {
        munmap(addr, size);
    }
}

/**
 * @brief Free file entry resources
 */
static void free_file_entry(source_file_entry_t* entry) {
    if (!entry) {
        return;
    }

    if (entry->mmap_addr && entry->file_size > 0) {
        munmap_file(entry->mmap_addr, entry->file_size);
    }

    if (entry->line_offsets) {
        nimcp_free(entry->line_offsets);
    }

    nimcp_platform_rwlock_destroy(&entry->lock);
}

/**
 * @brief Hash table value destructor for file entries
 */
static void file_entry_destructor(void* value, size_t value_size) {
    (void)value_size;
    source_file_entry_t* entry = (source_file_entry_t*)value;
    free_file_entry(entry);
}

/**
 * @brief Load file into cache
 */
static source_file_entry_t* load_file_entry(
    struct nimcp_source_cache* cache,
    const char* filename,
    const char* full_path
) {
    /* Get file info */
    size_t file_size = 0;
    time_t mtime = get_file_mtime(full_path);
    if (mtime == 0) {
        LOG_DEBUG("File not found: %s", full_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_file_entry: mtime is zero");
        return NULL;
    }

    /* mmap the file */
    void* mmap_addr = mmap_file(full_path, &file_size);
    if (!mmap_addr && file_size > 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_file_entry: mmap_addr is NULL");
        return NULL;
    }

    /* Build line offsets */
    uint32_t line_count = 0;
    uint32_t* line_offsets = NULL;

    if (mmap_addr && file_size > 0) {
        line_offsets = build_line_offsets((const char*)mmap_addr, file_size, &line_count);
    }

    /* Create entry */
    source_file_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    strncpy(entry.filename, filename, NIMCP_SOURCE_CACHE_MAX_PATH - 1);
    entry.filename[NIMCP_SOURCE_CACHE_MAX_PATH - 1] = '\0';
    entry.mmap_addr = mmap_addr;
    entry.file_size = file_size;
    entry.mtime = mtime;
    entry.line_count = line_count;
    entry.line_offsets = line_offsets;
    entry.modified = false;

    if (nimcp_platform_rwlock_init(&entry.lock) != 0) {
        LOG_ERROR("Failed to initialize rwlock for file entry: %s", filename);
        munmap_file(mmap_addr, file_size);
        if (line_offsets) {
            nimcp_free(line_offsets);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "load_file_entry: validation failed");
        return NULL;
    }

    /* Insert into hash table */
    if (!hash_table_insert_string(cache->file_table, filename, &entry, sizeof(entry))) {
        LOG_ERROR("Failed to insert file entry into hash table: %s", filename);
        free_file_entry(&entry);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_file_entry: hash_table_insert_string is NULL");
        return NULL;
    }

    /* Update stats */
    nimcp_platform_mutex_lock(&cache->cache_lock);
    cache->stats.files_cached++;
    cache->stats.total_file_size += file_size;
    cache->stats.total_lines += line_count;
    cache->stats.memory_used += file_size + (line_count * sizeof(uint32_t)) + sizeof(source_file_entry_t);
    nimcp_platform_mutex_unlock(&cache->cache_lock);

    LOG_DEBUG("Cached file: %s (%zu bytes, %u lines)", filename, file_size, line_count);

    /* Return pointer to stored entry */
    return (source_file_entry_t*)hash_table_lookup_string(cache->file_table, filename);
}

/**
 * @brief Refresh a file entry if modified
 */
static bool refresh_file_entry(
    struct nimcp_source_cache* cache,
    source_file_entry_t* entry,
    const char* full_path
) {
    time_t current_mtime = get_file_mtime(full_path);
    if (current_mtime == 0) {
        LOG_DEBUG("File no longer exists: %s", entry->filename);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "refresh_file_entry: current_mtime is zero");
        return false;
    }

    /* Check if file actually changed */
    if (current_mtime == entry->mtime && !entry->modified) {
        return true;  /* No change needed */
    }

    /* Unmap old content */
    if (entry->mmap_addr && entry->file_size > 0) {
        /* Update stats before unmapping */
        nimcp_platform_mutex_lock(&cache->cache_lock);
        cache->stats.total_file_size -= entry->file_size;
        cache->stats.total_lines -= entry->line_count;
        cache->stats.memory_used -= entry->file_size + (entry->line_count * sizeof(uint32_t));
        nimcp_platform_mutex_unlock(&cache->cache_lock);

        munmap_file(entry->mmap_addr, entry->file_size);
        entry->mmap_addr = NULL;
    }

    if (entry->line_offsets) {
        nimcp_free(entry->line_offsets);
        entry->line_offsets = NULL;
    }

    /* Re-mmap file */
    size_t file_size = 0;
    void* mmap_addr = mmap_file(full_path, &file_size);
    if (!mmap_addr && file_size > 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "refresh_file_entry: mmap_addr is NULL");
        return false;
    }

    /* Rebuild line offsets */
    uint32_t line_count = 0;
    uint32_t* line_offsets = NULL;

    if (mmap_addr && file_size > 0) {
        line_offsets = build_line_offsets((const char*)mmap_addr, file_size, &line_count);
    }

    /* Update entry */
    entry->mmap_addr = mmap_addr;
    entry->file_size = file_size;
    entry->mtime = current_mtime;
    entry->line_count = line_count;
    entry->line_offsets = line_offsets;
    entry->modified = false;

    /* Update stats */
    nimcp_platform_mutex_lock(&cache->cache_lock);
    cache->stats.total_file_size += file_size;
    cache->stats.total_lines += line_count;
    cache->stats.memory_used += file_size + (line_count * sizeof(uint32_t));
    cache->stats.refresh_count++;
    nimcp_platform_mutex_unlock(&cache->cache_lock);

    LOG_DEBUG("Refreshed file: %s (%zu bytes, %u lines)", entry->filename, file_size, line_count);

    return true;
}

/**
 * @brief Get or load file entry
 */
static source_file_entry_t* get_or_load_file(
    struct nimcp_source_cache* cache,
    const char* filename
) {
    if (!cache || !filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_or_load_file: required parameter is NULL (cache, filename)");
        return NULL;
    }

    /* Build full path */
    char full_path[NIMCP_SOURCE_CACHE_MAX_PATH];
    if (!build_full_path(cache, filename, full_path, sizeof(full_path))) {
        LOG_DEBUG("Failed to build path for: %s", filename);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "get_or_load_file: build_full_path is NULL");
        return NULL;
    }

    /* Look up in hash table */
    source_file_entry_t* entry = (source_file_entry_t*)hash_table_lookup_string(
        cache->file_table, filename);

    if (entry) {
        /* Update hit stats */
        nimcp_platform_mutex_lock(&cache->cache_lock);
        cache->stats.cache_hits++;
        nimcp_platform_mutex_unlock(&cache->cache_lock);

        /* Check if auto-refresh needed */
        if (cache->config.auto_refresh && entry->modified) {
            nimcp_platform_rwlock_wrlock(&entry->lock);
            refresh_file_entry(cache, entry, full_path);
            nimcp_platform_rwlock_wrunlock(&entry->lock);
        }

        return entry;
    }

    /* Cache miss - load file */
    nimcp_platform_mutex_lock(&cache->cache_lock);
    cache->stats.cache_misses++;
    nimcp_platform_mutex_unlock(&cache->cache_lock);

    return load_file_entry(cache, filename, full_path);
}

/* ============================================================================
 * PUBLIC API - LIFECYCLE
 * ============================================================================ */

source_cache_t source_cache_create(const char* source_root) {
    return source_cache_create_with_config(source_root, NULL);
}

source_cache_t source_cache_create_with_config(
    const char* source_root,
    const source_cache_config_t* config
) {
    struct nimcp_source_cache* cache = nimcp_calloc(1, sizeof(struct nimcp_source_cache));
    if (!cache) {
        LOG_ERROR("Failed to allocate source cache");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "source_cache_create_with_config: cache is NULL");
        return NULL;
    }

    cache->magic = NIMCP_SOURCE_CACHE_MAGIC;

    /* Set source root */
    if (source_root && source_root[0]) {
        strncpy(cache->source_root, source_root, NIMCP_SOURCE_CACHE_MAX_ROOT - 1);
        cache->source_root[NIMCP_SOURCE_CACHE_MAX_ROOT - 1] = '\0';
    } else {
        cache->source_root[0] = '\0';  /* Use cwd */
    }

    /* Apply configuration */
    if (config) {
        cache->config = *config;
    } else {
        cache->config = source_cache_default_config();
    }

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&cache->cache_lock, false) != 0) {
        LOG_ERROR("Failed to initialize cache mutex");
        nimcp_free(cache);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "source_cache_create_with_config: validation failed");
        return NULL;
    }

    /* Create hash table for file entries */
    hash_table_config_t table_config = {
        .initial_buckets = NIMCP_SOURCE_CACHE_DEFAULT_BUCKETS,
        .key_type = HASH_KEY_STRING,
        .hash_algorithm = HASH_ALG_FNV1A,
        .case_insensitive = false,
        .value_destructor = file_entry_destructor,
        .thread_safe = false  /* We manage our own locking */
    };

    cache->file_table = hash_table_create(&table_config);
    if (!cache->file_table) {
        LOG_ERROR("Failed to create file hash table");
        nimcp_platform_mutex_destroy(&cache->cache_lock);
        nimcp_free(cache);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "source_cache_create_with_config: cache->file_table is NULL");
        return NULL;
    }

    /* Clear stats */
    memset(&cache->stats, 0, sizeof(source_cache_stats_t));

    cache->initialized = true;

    LOG_INFO("Source cache created with root: %s",
             cache->source_root[0] ? cache->source_root : "(cwd)");

    return cache;
}

void source_cache_destroy(source_cache_t cache) {
    if (!cache) {
        return;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;

    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        LOG_ERROR("Invalid source cache (bad magic)");
        return;
    }

    LOG_INFO("Destroying source cache (%lu files, %lu bytes)",
             c->stats.files_cached, c->stats.total_file_size);

    /* Destroy hash table (calls entry destructors) */
    if (c->file_table) {
        hash_table_destroy(c->file_table);
    }

    /* Destroy mutex */
    nimcp_platform_mutex_destroy(&c->cache_lock);

    /* Clear magic to detect use-after-free */
    c->magic = 0;

    nimcp_free(c);
}

source_cache_config_t source_cache_default_config(void) {
    source_cache_config_t config = {
        .max_files = NIMCP_SOURCE_CACHE_DEFAULT_MAX_FILES,
        .max_memory = NIMCP_SOURCE_CACHE_DEFAULT_MAX_MEMORY,
        .auto_refresh = true,
        .preload_extensions = false,
        .extensions = NULL
    };
    return config;
}

/* ============================================================================
 * PUBLIC API - FILE ACCESS
 * ============================================================================ */

const source_file_entry_t* source_cache_get_file(
    source_cache_t cache,
    const char* filename
) {
    if (!cache || !filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_get_file: required parameter is NULL (cache, filename)");
        return NULL;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        LOG_ERROR("Invalid source cache");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_get_file: validation failed");
        return NULL;
    }

    return get_or_load_file(c, filename);
}

size_t source_cache_get_lines(
    source_cache_t cache,
    const char* filename,
    uint32_t start_line,
    uint32_t end_line,
    char* buffer,
    size_t buffer_size
) {
    if (!cache || !filename || !buffer || buffer_size == 0) {
        return 0;
    }

    if (start_line == 0) {
        start_line = 1;  /* Lines are 1-indexed */
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    source_file_entry_t* entry = get_or_load_file(c, filename);
    if (!entry) {
        return 0;
    }

    /* Acquire read lock */
    nimcp_platform_rwlock_rdlock(&entry->lock);

    /* Validate line range */
    if (start_line > entry->line_count) {
        nimcp_platform_rwlock_rdunlock(&entry->lock);
        return 0;
    }

    if (end_line == 0 || end_line > entry->line_count) {
        end_line = entry->line_count;
    }

    if (start_line > end_line) {
        nimcp_platform_rwlock_rdunlock(&entry->lock);
        return 0;
    }

    /* Calculate byte range (line numbers are 1-indexed, array is 0-indexed) */
    uint32_t start_offset = entry->line_offsets[start_line - 1];
    uint32_t end_offset = entry->line_offsets[end_line];  /* Includes trailing newline */

    if (start_offset >= entry->file_size) {
        nimcp_platform_rwlock_rdunlock(&entry->lock);
        return 0;
    }

    if (end_offset > entry->file_size) {
        end_offset = (uint32_t)entry->file_size;
    }

    /* Copy content */
    size_t copy_size = end_offset - start_offset;
    if (copy_size >= buffer_size) {
        copy_size = buffer_size - 1;  /* Leave room for null terminator */
    }

    const char* content = (const char*)entry->mmap_addr;
    memcpy(buffer, content + start_offset, copy_size);
    buffer[copy_size] = '\0';

    nimcp_platform_rwlock_rdunlock(&entry->lock);

    return copy_size;
}

size_t source_cache_get_line(
    source_cache_t cache,
    const char* filename,
    uint32_t line_number,
    char* buffer,
    size_t buffer_size
) {
    if (!cache || !filename || !buffer || buffer_size == 0 || line_number == 0) {
        return 0;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    source_file_entry_t* entry = get_or_load_file(c, filename);
    if (!entry) {
        return 0;
    }

    /* Acquire read lock */
    nimcp_platform_rwlock_rdlock(&entry->lock);

    /* Validate line number */
    if (line_number > entry->line_count) {
        nimcp_platform_rwlock_rdunlock(&entry->lock);
        return 0;
    }

    /* Get line boundaries (1-indexed to 0-indexed) */
    uint32_t start_offset = entry->line_offsets[line_number - 1];
    uint32_t end_offset = entry->line_offsets[line_number];

    if (start_offset >= entry->file_size) {
        nimcp_platform_rwlock_rdunlock(&entry->lock);
        return 0;
    }

    /* Calculate line length (excluding trailing newline) */
    size_t line_length = end_offset - start_offset;
    const char* content = (const char*)entry->mmap_addr;

    /* Remove trailing newline from length */
    while (line_length > 0 &&
           (content[start_offset + line_length - 1] == '\n' ||
            content[start_offset + line_length - 1] == '\r')) {
        line_length--;
    }

    /* Copy content */
    size_t copy_size = line_length;
    if (copy_size >= buffer_size) {
        copy_size = buffer_size - 1;
    }

    memcpy(buffer, content + start_offset, copy_size);
    buffer[copy_size] = '\0';

    nimcp_platform_rwlock_rdunlock(&entry->lock);

    return line_length;  /* Return actual line length, not truncated size */
}

char* source_cache_get_function_source(
    source_cache_t cache,
    const char* filename,
    uint32_t start_line,
    uint32_t end_line
) {
    if (!cache || !filename || start_line == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_get_function_source: required parameter is NULL (cache, filename)");
        return NULL;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    source_file_entry_t* entry = get_or_load_file(c, filename);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return NULL;
    }

    /* Acquire read lock */
    nimcp_platform_rwlock_rdlock(&entry->lock);

    /* Validate line range */
    if (start_line > entry->line_count) {
        nimcp_platform_rwlock_rdunlock(&entry->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_get_function_source: validation failed");
        return NULL;
    }

    if (end_line == 0 || end_line > entry->line_count) {
        end_line = entry->line_count;
    }

    if (start_line > end_line) {
        nimcp_platform_rwlock_rdunlock(&entry->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_get_function_source: validation failed");
        return NULL;
    }

    /* Calculate byte range */
    uint32_t start_offset = entry->line_offsets[start_line - 1];
    uint32_t end_offset = entry->line_offsets[end_line];

    if (start_offset >= entry->file_size) {
        nimcp_platform_rwlock_rdunlock(&entry->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "source_cache_get_function_source: capacity exceeded");
        return NULL;
    }

    if (end_offset > entry->file_size) {
        end_offset = (uint32_t)entry->file_size;
    }

    /* Allocate and copy */
    size_t copy_size = end_offset - start_offset;
    char* result = nimcp_malloc(copy_size + 1);
    if (!result) {
        nimcp_platform_rwlock_rdunlock(&entry->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "source_cache_get_function_source: result is NULL");
        return NULL;
    }

    const char* content = (const char*)entry->mmap_addr;
    memcpy(result, content + start_offset, copy_size);
    result[copy_size] = '\0';

    nimcp_platform_rwlock_rdunlock(&entry->lock);

    return result;
}

uint32_t source_cache_get_line_count(
    source_cache_t cache,
    const char* filename
) {
    if (!cache || !filename) {
        return 0;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    source_file_entry_t* entry = get_or_load_file(c, filename);
    if (!entry) {
        return 0;
    }

    nimcp_platform_rwlock_rdlock(&entry->lock);
    uint32_t count = entry->line_count;
    nimcp_platform_rwlock_rdunlock(&entry->lock);

    return count;
}

size_t source_cache_get_file_size(
    source_cache_t cache,
    const char* filename
) {
    if (!cache || !filename) {
        return 0;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    source_file_entry_t* entry = get_or_load_file(c, filename);
    if (!entry) {
        return 0;
    }

    nimcp_platform_rwlock_rdlock(&entry->lock);
    size_t size = entry->file_size;
    nimcp_platform_rwlock_rdunlock(&entry->lock);

    return size;
}

/* ============================================================================
 * PUBLIC API - CACHE MANAGEMENT
 * ============================================================================ */

bool source_cache_mark_modified(
    source_cache_t cache,
    const char* filename
) {
    if (!cache || !filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_mark_modified: required parameter is NULL (cache, filename)");
        return false;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "source_cache_mark_modified: validation failed");
        return false;
    }

    source_file_entry_t* entry = (source_file_entry_t*)hash_table_lookup_string(
        c->file_table, filename);

    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_mark_modified: entry is NULL");
        return false;
    }

    nimcp_platform_rwlock_wrlock(&entry->lock);
    entry->modified = true;
    nimcp_platform_rwlock_wrunlock(&entry->lock);

    return true;
}

bool source_cache_invalidate(
    source_cache_t cache,
    const char* filename
) {
    if (!cache || !filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_invalidate: required parameter is NULL (cache, filename)");
        return false;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        return false;
    }

    /* Check if entry exists */
    source_file_entry_t* entry = (source_file_entry_t*)hash_table_lookup_string(
        c->file_table, filename);

    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_invalidate: entry is NULL");
        return false;
    }

    /* Update stats before removal */
    nimcp_platform_mutex_lock(&c->cache_lock);
    if (c->stats.files_cached > 0) {
        c->stats.files_cached--;
    }
    c->stats.total_file_size -= entry->file_size;
    c->stats.total_lines -= entry->line_count;
    c->stats.memory_used -= entry->file_size + (entry->line_count * sizeof(uint32_t)) + sizeof(source_file_entry_t);
    c->stats.invalidation_count++;
    nimcp_platform_mutex_unlock(&c->cache_lock);

    /* Remove from hash table (destructor will free resources) */
    bool removed = hash_table_remove_string(c->file_table, filename);

    LOG_DEBUG("Invalidated file: %s", filename);

    return removed;
}

bool source_cache_refresh(
    source_cache_t cache,
    const char* filename
) {
    if (!cache || !filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_refresh: required parameter is NULL (cache, filename)");
        return false;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "source_cache_refresh: validation failed");
        return false;
    }

    /* Build full path */
    char full_path[NIMCP_SOURCE_CACHE_MAX_PATH];
    if (!build_full_path(c, filename, full_path, sizeof(full_path))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "source_cache_refresh: build_full_path is NULL");
        return false;
    }

    /* Get entry */
    source_file_entry_t* entry = (source_file_entry_t*)hash_table_lookup_string(
        c->file_table, filename);

    if (!entry) {
        /* Not cached - try to load */
        return (load_file_entry(c, filename, full_path) != NULL);
    }

    /* Refresh entry */
    nimcp_platform_rwlock_wrlock(&entry->lock);
    bool result = refresh_file_entry(c, entry, full_path);
    nimcp_platform_rwlock_wrunlock(&entry->lock);

    return result;
}

/**
 * @brief Iterator callback for refresh_all_modified
 */
typedef struct {
    struct nimcp_source_cache* cache;
    uint32_t refresh_count;
} refresh_context_t;

static bool refresh_modified_iterator(
    const void* key,
    size_t key_size,
    void* value,
    size_t value_size,
    void* user_data
) {
    (void)key_size;
    (void)value_size;

    const char* filename = (const char*)key;
    source_file_entry_t* entry = (source_file_entry_t*)value;
    refresh_context_t* ctx = (refresh_context_t*)user_data;

    if (entry->modified) {
        char full_path[NIMCP_SOURCE_CACHE_MAX_PATH];
        if (build_full_path(ctx->cache, filename, full_path, sizeof(full_path))) {
            nimcp_platform_rwlock_wrlock(&entry->lock);
            if (refresh_file_entry(ctx->cache, entry, full_path)) {
                ctx->refresh_count++;
            }
            nimcp_platform_rwlock_wrunlock(&entry->lock);
        }
    }

    return true;  /* Continue iteration */
}

uint32_t source_cache_refresh_all_modified(source_cache_t cache) {
    if (!cache) {
        return 0;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        return 0;
    }

    refresh_context_t ctx = {
        .cache = c,
        .refresh_count = 0
    };

    hash_table_iterate(c->file_table, refresh_modified_iterator, &ctx);

    return ctx.refresh_count;
}

bool source_cache_is_cached(
    source_cache_t cache,
    const char* filename
) {
    if (!cache || !filename) {
        return false;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        return false;
    }

    return (hash_table_lookup_string(c->file_table, filename) != NULL);
}

bool source_cache_needs_refresh(
    source_cache_t cache,
    const char* filename
) {
    if (!cache || !filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_needs_refresh: required parameter is NULL (cache, filename)");
        return false;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "source_cache_needs_refresh: validation failed");
        return false;
    }

    source_file_entry_t* entry = (source_file_entry_t*)hash_table_lookup_string(
        c->file_table, filename);

    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_needs_refresh: entry is NULL");
        return false;  /* Not cached */
    }

    /* Check if marked modified */
    nimcp_platform_rwlock_rdlock(&entry->lock);
    if (entry->modified) {
        nimcp_platform_rwlock_rdunlock(&entry->lock);
        return true;
    }
    time_t cached_mtime = entry->mtime;
    nimcp_platform_rwlock_rdunlock(&entry->lock);

    /* Check filesystem mtime */
    char full_path[NIMCP_SOURCE_CACHE_MAX_PATH];
    if (!build_full_path(c, filename, full_path, sizeof(full_path))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "source_cache_needs_refresh: build_full_path is NULL");
        return false;
    }

    time_t current_mtime = get_file_mtime(full_path);
    return (current_mtime != cached_mtime);
}

/**
 * @brief Iterator callback for clear
 */
static bool clear_iterator(
    const void* key,
    size_t key_size,
    void* value,
    size_t value_size,
    void* user_data
) {
    (void)key;
    (void)key_size;
    (void)value;
    (void)value_size;
    (void)user_data;
    /* Hash table destructor will handle cleanup */
    return true;
}

void source_cache_clear(source_cache_t cache) {
    if (!cache) {
        return;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        return;
    }

    LOG_INFO("Clearing source cache (%lu files)", c->stats.files_cached);

    /* Clear hash table */
    hash_table_clear(c->file_table);

    /* Reset stats */
    nimcp_platform_mutex_lock(&c->cache_lock);
    c->stats.files_cached = 0;
    c->stats.total_file_size = 0;
    c->stats.total_lines = 0;
    c->stats.memory_used = 0;
    nimcp_platform_mutex_unlock(&c->cache_lock);
}

/* ============================================================================
 * PUBLIC API - STATISTICS
 * ============================================================================ */

bool source_cache_get_stats(
    source_cache_t cache,
    source_cache_stats_t* stats
) {
    if (!cache || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_get_stats: required parameter is NULL (cache, stats)");
        return false;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "source_cache_get_stats: validation failed");
        return false;
    }

    nimcp_platform_mutex_lock(&c->cache_lock);
    *stats = c->stats;
    nimcp_platform_mutex_unlock(&c->cache_lock);

    return true;
}

void source_cache_reset_stats(source_cache_t cache) {
    if (!cache) {
        return;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        return;
    }

    nimcp_platform_mutex_lock(&c->cache_lock);

    /* Preserve current state counts, reset performance counters */
    uint64_t files = c->stats.files_cached;
    uint64_t file_size = c->stats.total_file_size;
    uint64_t lines = c->stats.total_lines;
    uint64_t memory = c->stats.memory_used;

    memset(&c->stats, 0, sizeof(source_cache_stats_t));

    c->stats.files_cached = files;
    c->stats.total_file_size = file_size;
    c->stats.total_lines = lines;
    c->stats.memory_used = memory;

    nimcp_platform_mutex_unlock(&c->cache_lock);
}

void source_cache_print_stats(source_cache_t cache) {
    if (!cache) {
        printf("Source cache: NULL\n");
        return;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        printf("Source cache: INVALID\n");
        return;
    }

    source_cache_stats_t stats;
    source_cache_get_stats(cache, &stats);

    printf("\n=== Source Cache Statistics ===\n");
    printf("Root: %s\n", c->source_root[0] ? c->source_root : "(cwd)");
    printf("Files cached: %lu\n", stats.files_cached);
    printf("Total file size: %.2f KB\n", stats.total_file_size / 1024.0);
    printf("Total lines: %lu\n", stats.total_lines);
    printf("Memory used: %.2f KB\n", stats.memory_used / 1024.0);
    printf("Cache hits: %lu\n", stats.cache_hits);
    printf("Cache misses: %lu\n", stats.cache_misses);

    if (stats.cache_hits + stats.cache_misses > 0) {
        double hit_rate = (double)stats.cache_hits /
                          (stats.cache_hits + stats.cache_misses) * 100.0;
        printf("Hit rate: %.1f%%\n", hit_rate);
    }

    printf("Refresh count: %lu\n", stats.refresh_count);
    printf("Invalidation count: %lu\n", stats.invalidation_count);
    printf("================================\n\n");
}

const char* source_cache_get_root(source_cache_t cache) {
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cache is NULL");

        return NULL;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_get_root: validation failed");
        return NULL;
    }

    return c->source_root[0] ? c->source_root : NULL;
}

bool source_cache_validate(source_cache_t cache) {
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_validate: cache is NULL");
        return false;
    }

    struct nimcp_source_cache* c = (struct nimcp_source_cache*)cache;

    /* Check magic */
    if (c->magic != NIMCP_SOURCE_CACHE_MAGIC) {
        LOG_ERROR("Source cache validation failed: bad magic");
        return false;
    }

    /* Check hash table */
    if (!c->file_table) {
        LOG_ERROR("Source cache validation failed: no file table");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_validate: c->file_table is NULL");
        return false;
    }

    /* Check initialized flag */
    if (!c->initialized) {
        LOG_ERROR("Source cache validation failed: not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "source_cache_validate: c->initialized is NULL");
        return false;
    }

    /* Verify file count matches hash table */
    size_t table_size = hash_table_size(c->file_table);
    if (table_size != c->stats.files_cached) {
        LOG_WARN("Source cache stats mismatch: table=%zu, stats=%lu",
                 table_size, c->stats.files_cached);
        /* This is a warning, not a failure - stats might lag */
    }

    return true;
}
