/**
 * @file nimcp_dwarf_symbols.c
 * @brief DWARF debug symbols parser implementation
 *
 * IMPLEMENTATION NOTES:
 * - Uses libdw (elfutils) when available for full DWARF parsing
 * - Falls back to addr2line via fork/exec
 * - Falls back to dladdr for basic ELF symbol table
 * - LRU cache for repeated lookups (common in stack traces)
 * - PIE support via /proc/self/maps parsing
 */

/* _GNU_SOURCE must be defined before ANY includes for dladdr */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "utils/code/nimcp_dwarf_symbols.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* Dynamic linker for dladdr */
#include <dlfcn.h>

/* Optional libdw support */
#ifdef HAVE_LIBDW
#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <dwarf.h>
#endif

/* ============================================================================
 * MODULE CONSTANTS
 * ============================================================================ */

#define LOG_MODULE "dwarf_symbols"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dwarf_symbols)

/** Maximum command line length for addr2line */
#define ADDR2LINE_CMD_MAX 512

/** Maximum output line from addr2line */
#define ADDR2LINE_OUTPUT_MAX 512

/** Maximum path for /proc/self/exe */
#define SELF_EXE_PATH_MAX 256

/** Default addr2line binary */
#define DEFAULT_ADDR2LINE_PATH "/usr/bin/addr2line"

/** Cache entry structure size */
#define CACHE_ENTRY_SIZE sizeof(cache_entry_t)

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Cache entry for symbol lookups
 */
typedef struct {
    void* address;                  /**< Looked up address */
    symbol_info_t info;             /**< Cached symbol info */
    symbol_source_t source;         /**< How info was obtained */
    uint64_t access_count;          /**< LRU counter */
} cache_entry_t;

/**
 * @brief Internal DWARF symbols structure
 */
struct dwarf_symbols {
    uint32_t magic;                                 /**< Magic for validation */
    char binary_path[DWARF_SYMBOLS_MAX_PATH];       /**< Path to binary */
    dwarf_symbols_config_t config;                  /**< Configuration */
    dwarf_symbols_stats_t stats;                    /**< Statistics */

    /* PIE/ASLR support */
    ptrdiff_t load_offset;                          /**< Runtime - file address offset */
    bool is_pie;                                    /**< Is position independent */
    void* base_address;                             /**< Base load address */

    /* Lookup cache */
    hash_table_t* cache;                            /**< Address -> cache_entry_t */
    uint64_t cache_access_counter;                  /**< LRU counter */
    nimcp_platform_mutex_t cache_lock;              /**< Cache lock */

    /* DWARF state */
#ifdef HAVE_LIBDW
    Dwfl* dwfl;                                     /**< DWARF frame library handle */
    Dwfl_Module* module;                            /**< DWARF module */
    bool dwarf_available;                           /**< DWARF info present */
#endif

    /* Fallback state */
    char addr2line_path[DWARF_SYMBOLS_MAX_PATH];    /**< Path to addr2line */
    bool addr2line_available;                       /**< addr2line found */

    bool initialized;                               /**< Initialization complete */
};

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

static bool lookup_via_dwarf(struct dwarf_symbols* syms, void* addr, symbol_info_t* info);
static bool lookup_via_addr2line(struct dwarf_symbols* syms, void* addr, symbol_info_t* info);
static bool lookup_via_dladdr(struct dwarf_symbols* syms, void* addr, symbol_info_t* info);
static bool parse_proc_maps(struct dwarf_symbols* syms);
static void cache_insert(struct dwarf_symbols* syms, void* addr,
                         const symbol_info_t* info, symbol_source_t source);
static bool cache_lookup(struct dwarf_symbols* syms, void* addr, symbol_info_t* info);
static void cache_entry_destructor(void* value, size_t value_size);

/* ============================================================================
 * LIBDW CALLBACKS (when available)
 * ============================================================================ */

#ifdef HAVE_LIBDW
/**
 * @brief Find ELF callback for libdwfl
 */
static int find_elf_cb(
    Dwfl_Module* mod,
    void** userdata,
    const char* modname,
    Dwarf_Addr base,
    char** file_name,
    Elf** elfp
) {
    (void)mod;
    (void)userdata;
    (void)modname;
    (void)base;
    (void)file_name;
    (void)elfp;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_elf_cb: operation failed");
    return -1;  /* Use default */
}

/**
 * @brief Find debug info callback for libdwfl
 */
static int find_debuginfo_cb(
    Dwfl_Module* mod,
    void** userdata,
    const char* modname,
    Dwarf_Addr base,
    const char* file_name,
    const char* debuglink_file,
    GElf_Word debuglink_crc,
    char** debuginfo_file_name
) {
    (void)mod;
    (void)userdata;
    (void)modname;
    (void)base;
    (void)file_name;
    (void)debuglink_file;
    (void)debuglink_crc;
    (void)debuginfo_file_name;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_debuginfo_cb: operation failed");
    return -1;  /* Use default */
}

static const Dwfl_Callbacks dwfl_callbacks = {
    .find_elf = dwfl_linux_proc_find_elf,
    .find_debuginfo = dwfl_standard_find_debuginfo,
    .debuginfo_path = NULL
};
#endif

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get path to current executable
 */
static bool get_self_exe_path(char* path, size_t path_size) {
    ssize_t len = readlink("/proc/self/exe", path, path_size - 1);
    if (len < 0) {
        LOG_MODULE_WARN(LOG_MODULE, "Failed to read /proc/self/exe: %s", strerror(errno));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "get_self_exe_path: validation failed");
        return false;
    }
    path[len] = '\0';
    return true;
}

/**
 * @brief Check if file exists and is readable
 */
static bool file_exists(const char* path) {
    return access(path, R_OK) == 0;
}

/**
 * @brief Parse /proc/self/maps to find load address
 *
 * For PIE binaries, the load address may differ from the file addresses
 * in DWARF debug info. We need to calculate the offset.
 */
static bool parse_proc_maps(struct dwarf_symbols* syms) {
    if (!syms || syms->binary_path[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parse_proc_maps: syms is NULL");
        return false;
    }

    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Cannot open /proc/self/maps: %s", strerror(errno));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_proc_maps: maps is NULL");
        return false;
    }

    char line[NIMCP_ERROR_BUFFER_LARGE];
    bool found = false;

    while (fgets(line, sizeof(line), maps)) {
        /* Parse line: "start-end perms offset dev inode pathname" */
        unsigned long start, end;
        char perms[5];
        unsigned long offset;
        unsigned int dev_major, dev_minor;
        unsigned long inode;
        char pathname[NIMCP_SHORT_PATH_SIZE];

        int matched = sscanf(line, "%lx-%lx %4s %lx %x:%x %lu %255s",
                             &start, &end, perms, &offset, &dev_major, &dev_minor,
                             &inode, pathname);

        if (matched >= 7 && offset == 0 && perms[2] == 'x') {
            /* Executable mapping at offset 0 - check if it's our binary */
            if (matched == 8 && strstr(pathname, syms->binary_path) != NULL) {
                syms->base_address = (void*)start;
                syms->is_pie = true;
                /* For PIE, the offset is base_address - 0 (file addresses start at 0) */
                syms->load_offset = (ptrdiff_t)start;
                found = true;
                LOG_MODULE_DEBUG(LOG_MODULE, "Found PIE binary at %p (offset=%ld)",
                                 syms->base_address, (long)syms->load_offset);
                break;
            }
        }
    }

    fclose(maps);

    if (!found) {
        /* Not PIE or couldn't find - assume no offset */
        syms->load_offset = 0;
        syms->is_pie = false;
        LOG_MODULE_DEBUG(LOG_MODULE, "Binary appears to be non-PIE or not found in maps");
    }

    return true;
}

/**
 * @brief Cache destructor for hash table
 */
static void cache_entry_destructor(void* value, size_t value_size) {
    (void)value;
    (void)value_size;
    /* cache_entry_t has no dynamic allocations */
}

/**
 * @brief Format address as string key for cache
 */
static void format_addr_key(void* addr, char* key_buf, size_t key_buf_size) {
    snprintf(key_buf, key_buf_size, "%p", addr);
}

/**
 * @brief Insert entry into cache
 */
static void cache_insert(struct dwarf_symbols* syms, void* addr,
                         const symbol_info_t* info, symbol_source_t source) {
    if (!syms || !info || !syms->cache) {
        return;
    }

    cache_entry_t entry;
    entry.address = addr;
    entry.info = *info;
    entry.source = source;
    entry.access_count = ++syms->cache_access_counter;

    /* Use address as string key */
    char key[32];
    format_addr_key(addr, key, sizeof(key));

    nimcp_platform_mutex_lock(&syms->cache_lock);
    hash_table_insert_string(syms->cache, key, &entry, sizeof(entry));
    nimcp_platform_mutex_unlock(&syms->cache_lock);
}

/**
 * @brief Look up entry in cache
 */
static bool cache_lookup(struct dwarf_symbols* syms, void* addr, symbol_info_t* info) {
    if (!syms || !info || !syms->cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cache_lookup: required parameter is NULL (syms, info, syms->cache)");
        return false;
    }

    char key[32];
    format_addr_key(addr, key, sizeof(key));

    nimcp_platform_mutex_lock(&syms->cache_lock);
    cache_entry_t* entry = (cache_entry_t*)hash_table_lookup_string(syms->cache, key);

    if (entry) {
        *info = entry->info;
        entry->access_count = ++syms->cache_access_counter;
        syms->stats.cache_hits++;
        nimcp_platform_mutex_unlock(&syms->cache_lock);
        return true;
    }

    nimcp_platform_mutex_unlock(&syms->cache_lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cache_lookup: validation failed");
    return false;
}

/* ============================================================================
 * DWARF LOOKUP (when libdw available)
 * ============================================================================ */

#ifdef HAVE_LIBDW
static bool lookup_via_dwarf(struct dwarf_symbols* syms, void* addr, symbol_info_t* info) {
    if (!syms || !info || !syms->dwfl || !syms->dwarf_available) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lookup_via_dwarf: required parameter is NULL (syms, info, syms->dwfl, syms->dwarf_available)");
        return false;
    }

    /* Convert runtime address to file address for PIE */
    Dwarf_Addr dwarf_addr = (Dwarf_Addr)addr;
    if (syms->is_pie) {
        dwarf_addr -= syms->load_offset;
    }

    /* Find module containing address */
    Dwfl_Module* mod = dwfl_addrmodule(syms->dwfl, dwarf_addr);
    if (!mod) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lookup_via_dwarf: mod is NULL");
        return false;
    }

    /* Get function name */
    const char* func_name = dwfl_module_addrname(mod, dwarf_addr);
    if (func_name) {
        strncpy(info->function_name, func_name, DWARF_SYMBOLS_MAX_FUNC - 1);
        info->function_name[DWARF_SYMBOLS_MAX_FUNC - 1] = '\0';
    } else {
        info->function_name[0] = '\0';
    }

    /* Get line information */
    Dwfl_Line* line = dwfl_module_getsrc(mod, dwarf_addr);
    if (line) {
        int lineno = 0;
        int col = 0;
        const char* src = dwfl_lineinfo(line, NULL, &lineno, &col, NULL, NULL);

        if (src) {
            strncpy(info->source_file, src, DWARF_SYMBOLS_MAX_PATH - 1);
            info->source_file[DWARF_SYMBOLS_MAX_PATH - 1] = '\0';
        } else {
            info->source_file[0] = '\0';
        }

        info->line_number = (uint32_t)lineno;
        info->column = (uint32_t)col;
    } else {
        info->source_file[0] = '\0';
        info->line_number = 0;
        info->column = 0;
    }

    /* Check for inline */
    info->is_inline = false;
    info->inline_caller[0] = '\0';
    info->inline_caller_line = 0;

    /* Try to get inline info */
    Dwarf_Addr bias;
    Dwarf* dwarf = dwfl_module_getdwarf(mod, &bias);
    if (dwarf) {
        Dwarf_Die* scopes = NULL;
        int nscopes = dwfl_module_addrscopes(mod, dwarf_addr, &scopes);

        if (nscopes > 1 && scopes) {
            /* More than one scope means we might be in an inline */
            Dwarf_Die* inline_die = NULL;
            for (int i = 0; i < nscopes; i++) {
                int tag = dwarf_tag(&scopes[i]);
                if (tag == DW_TAG_inlined_subroutine) {
                    info->is_inline = true;
                    inline_die = &scopes[i];

                    /* Get the inlined function name */
                    Dwarf_Die origin;
                    if (dwarf_attr_die(inline_die, DW_AT_abstract_origin, &origin)) {
                        const char* inline_name = dwarf_diename(&origin);
                        if (inline_name) {
                            strncpy(info->function_name, inline_name,
                                    DWARF_SYMBOLS_MAX_FUNC - 1);
                        }
                    }

                    /* Get caller info from outer scope */
                    if (i + 1 < nscopes) {
                        const char* caller_name = dwarf_diename(&scopes[i + 1]);
                        if (caller_name) {
                            strncpy(info->inline_caller, caller_name,
                                    DWARF_SYMBOLS_MAX_FUNC - 1);
                        }

                        /* Get call site line */
                        Dwarf_Attribute attr;
                        Dwarf_Word line_val;
                        if (dwarf_attr(inline_die, DW_AT_call_line, &attr) &&
                            dwarf_formudata(&attr, &line_val) == 0) {
                            info->inline_caller_line = (uint32_t)line_val;
                        }
                    }

                    syms->stats.inline_detected++;
                    break;
                }
            }
        }
        if (scopes) {
            nimcp_free(scopes);
        }
    }

    syms->stats.dwarf_lookups++;
    return true;
}
#else
static bool lookup_via_dwarf(struct dwarf_symbols* syms, void* addr, symbol_info_t* info) {
    (void)syms;
    (void)addr;
    (void)info;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lookup_via_dwarf: operation failed");
    return false;  /* No libdw support */
}
#endif

/* ============================================================================
 * ADDR2LINE FALLBACK
 * ============================================================================ */

/**
 * @brief Look up symbol via addr2line external process
 *
 * Fork and exec: addr2line -f -e binary -a addr
 * Parse output: "function_name\nfile:line"
 */
static bool lookup_via_addr2line(struct dwarf_symbols* syms, void* addr, symbol_info_t* info) {
    if (!syms || !info || !syms->addr2line_available) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lookup_via_addr2line: required parameter is NULL (syms, info, syms->addr2line_available)");
        return false;
    }

    /* Create pipe for output */
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        LOG_MODULE_DEBUG(LOG_MODULE, "pipe() failed: %s", strerror(errno));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lookup_via_addr2line: validation failed");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        LOG_MODULE_DEBUG(LOG_MODULE, "fork() failed: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lookup_via_addr2line: validation failed");
        return false;
    }

    if (pid == 0) {
        /* Child process */
        close(pipefd[0]);  /* Close read end */

        /* Redirect stdout to pipe */
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        /* Suppress stderr */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        /* Format address */
        char addr_str[32];
        snprintf(addr_str, sizeof(addr_str), "%p", addr);

        /* Execute addr2line */
        execlp(syms->addr2line_path, "addr2line",
               "-f", "-e", syms->binary_path, addr_str, (char*)NULL);

        /* If exec fails */
        _exit(127);
    }

    /* Parent process */
    close(pipefd[1]);  /* Close write end */

    /* Read output */
    char output[ADDR2LINE_OUTPUT_MAX];
    ssize_t bytes_read = read(pipefd[0], output, sizeof(output) - 1);
    close(pipefd[0]);

    /* Wait for child */
    int status;
    waitpid(pid, &status, 0);

    if (bytes_read <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        LOG_MODULE_DEBUG(LOG_MODULE, "addr2line failed for %p", addr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lookup_via_addr2line: WIFEXITED is NULL");
        return false;
    }

    output[bytes_read] = '\0';

    /* Parse output: "function_name\nfile:line\n" */
    char* newline = strchr(output, '\n');
    if (!newline) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lookup_via_addr2line: newline is NULL");
        return false;
    }

    /* Extract function name */
    *newline = '\0';
    if (output[0] == '?' && output[1] == '?') {
        info->function_name[0] = '\0';
    } else {
        strncpy(info->function_name, output, DWARF_SYMBOLS_MAX_FUNC - 1);
        info->function_name[DWARF_SYMBOLS_MAX_FUNC - 1] = '\0';
    }

    /* Parse file:line */
    char* file_line = newline + 1;
    char* colon = strrchr(file_line, ':');
    if (colon) {
        *colon = '\0';
        char* line_str = colon + 1;

        /* Check for ?? (unknown file) */
        if (file_line[0] == '?' && file_line[1] == '?') {
            info->source_file[0] = '\0';
            info->line_number = 0;
        } else {
            strncpy(info->source_file, file_line, DWARF_SYMBOLS_MAX_PATH - 1);
            info->source_file[DWARF_SYMBOLS_MAX_PATH - 1] = '\0';

            /* Remove trailing newline from line number */
            char* end = strchr(line_str, '\n');
            if (end) *end = '\0';

            /* Parse line number */
            char* discriminator = strchr(line_str, ' ');
            if (discriminator) *discriminator = '\0';

            info->line_number = (uint32_t)strtoul(line_str, NULL, 10);
        }
    } else {
        info->source_file[0] = '\0';
        info->line_number = 0;
    }

    info->column = 0;
    info->is_inline = false;
    info->inline_caller[0] = '\0';
    info->inline_caller_line = 0;

    syms->stats.addr2line_lookups++;
    return (info->function_name[0] != '\0' || info->source_file[0] != '\0');
}

/* ============================================================================
 * DLADDR FALLBACK
 * ============================================================================ */

/**
 * @brief Look up symbol via dladdr (ELF symbol table)
 */
static bool lookup_via_dladdr(struct dwarf_symbols* syms, void* addr, symbol_info_t* info) {
    if (!syms || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lookup_via_dladdr: required parameter is NULL (syms, info)");
        return false;
    }

    Dl_info dl_info;
    if (!dladdr(addr, &dl_info)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lookup_via_dladdr: dladdr is NULL");
        return false;
    }

    /* Extract function name */
    if (dl_info.dli_sname) {
        strncpy(info->function_name, dl_info.dli_sname, DWARF_SYMBOLS_MAX_FUNC - 1);
        info->function_name[DWARF_SYMBOLS_MAX_FUNC - 1] = '\0';
    } else {
        info->function_name[0] = '\0';
    }

    /* dladdr doesn't provide file:line info */
    info->source_file[0] = '\0';
    info->line_number = 0;
    info->column = 0;
    info->is_inline = false;
    info->inline_caller[0] = '\0';
    info->inline_caller_line = 0;

    syms->stats.dladdr_lookups++;
    return (info->function_name[0] != '\0');
}

/* ============================================================================
 * PUBLIC API - LIFECYCLE
 * ============================================================================ */

dwarf_symbols_t dwarf_symbols_create(const char* binary_path) {
    return dwarf_symbols_create_with_config(binary_path, NULL);
}

dwarf_symbols_t dwarf_symbols_create_with_config(
    const char* binary_path,
    const dwarf_symbols_config_t* config
) {
    struct dwarf_symbols* syms = nimcp_calloc(1, sizeof(struct dwarf_symbols));
    if (!syms) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate dwarf_symbols");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dwarf_symbols_create_with_config: syms is NULL");
        return NULL;
    }

    syms->magic = DWARF_SYMBOLS_MAGIC;

    /* Apply configuration */
    if (config) {
        syms->config = *config;
    } else {
        syms->config = dwarf_symbols_default_config();
    }

    /* Resolve binary path */
    if (binary_path && binary_path[0]) {
        strncpy(syms->binary_path, binary_path, DWARF_SYMBOLS_MAX_PATH - 1);
    } else {
        /* Use current executable */
        if (!get_self_exe_path(syms->binary_path, DWARF_SYMBOLS_MAX_PATH)) {
            LOG_MODULE_ERROR(LOG_MODULE, "Cannot determine binary path");
            nimcp_free(syms);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_create_with_config: get_self_exe_path is NULL");
            return NULL;
        }
    }

    /* Check binary exists */
    if (!file_exists(syms->binary_path)) {
        LOG_MODULE_ERROR(LOG_MODULE, "Binary not found: %s", syms->binary_path);
        nimcp_free(syms);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_create_with_config: file_exists is NULL");
        return NULL;
    }

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&syms->cache_lock, false) != 0) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to initialize mutex");
        nimcp_free(syms);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "dwarf_symbols_create_with_config: validation failed");
        return NULL;
    }

    /* Create cache */
    size_t cache_size = syms->config.cache_size;
    if (cache_size == 0) {
        cache_size = DWARF_SYMBOLS_DEFAULT_CACHE_SIZE;
    }

    hash_table_config_t table_config = {
        .initial_buckets = cache_size,
        .key_type = HASH_KEY_STRING,
        .hash_algorithm = HASH_ALG_FNV1A,
        .case_insensitive = false,
        .value_destructor = cache_entry_destructor,
        .thread_safe = false  /* We manage our own locking */
    };

    syms->cache = hash_table_create(&table_config);
    if (!syms->cache) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to create symbol cache");
        nimcp_platform_mutex_destroy(&syms->cache_lock);
        nimcp_free(syms);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dwarf_symbols_create_with_config: syms->cache is NULL");
        return NULL;
    }

    /* Parse /proc/self/maps for PIE support */
    if (syms->config.auto_detect_pie) {
        parse_proc_maps(syms);
    }

    /* Initialize DWARF (when available) */
#ifdef HAVE_LIBDW
    syms->dwfl = dwfl_begin(&dwfl_callbacks);
    if (syms->dwfl) {
        /* Report the process */
        if (dwfl_linux_proc_report(syms->dwfl, getpid()) == 0) {
            dwfl_report_end(syms->dwfl, NULL, NULL);
            syms->dwarf_available = true;
            LOG_MODULE_INFO(LOG_MODULE, "DWARF symbols available via libdw");
        } else {
            LOG_MODULE_WARN(LOG_MODULE, "dwfl_linux_proc_report failed");
            syms->dwarf_available = false;
        }
    }
#endif

    /* Set up addr2line path */
    if (syms->config.addr2line_path && syms->config.addr2line_path[0]) {
        strncpy(syms->addr2line_path, syms->config.addr2line_path,
                DWARF_SYMBOLS_MAX_PATH - 1);
    } else {
        strncpy(syms->addr2line_path, DEFAULT_ADDR2LINE_PATH,
                DWARF_SYMBOLS_MAX_PATH - 1);
    }

    syms->addr2line_available = file_exists(syms->addr2line_path);
    if (!syms->addr2line_available) {
        LOG_MODULE_DEBUG(LOG_MODULE, "addr2line not found at %s", syms->addr2line_path);
    }

    syms->initialized = true;

    LOG_MODULE_INFO(LOG_MODULE, "DWARF symbols initialized for: %s (PIE=%d, DWARF=%d, addr2line=%d)",
                    syms->binary_path, syms->is_pie,
#ifdef HAVE_LIBDW
                    syms->dwarf_available,
#else
                    0,
#endif
                    syms->addr2line_available);

    return syms;
}

void dwarf_symbols_destroy(dwarf_symbols_t syms) {
    if (!syms) {
        return;
    }

    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;

    if (s->magic != DWARF_SYMBOLS_MAGIC) {
        LOG_MODULE_ERROR(LOG_MODULE, "Invalid dwarf_symbols (bad magic)");
        return;
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Destroying DWARF symbols (%lu lookups, %lu cache hits)",
                     s->stats.total_lookups, s->stats.cache_hits);

#ifdef HAVE_LIBDW
    if (s->dwfl) {
        dwfl_end(s->dwfl);
        s->dwfl = NULL;
    }
#endif

    if (s->cache) {
        hash_table_destroy(s->cache);
        s->cache = NULL;
    }

    nimcp_platform_mutex_destroy(&s->cache_lock);

    s->magic = 0;  /* Prevent use-after-free detection */
    nimcp_free(s);
}

dwarf_symbols_config_t dwarf_symbols_default_config(void) {
    dwarf_symbols_config_t config = {
        .cache_size = DWARF_SYMBOLS_DEFAULT_CACHE_SIZE,
        .enable_inline_unwind = true,
        .enable_locals = false,
        .auto_detect_pie = true,
        .addr2line_path = NULL
    };
    return config;
}

/* ============================================================================
 * PUBLIC API - CORE LOOKUP
 * ============================================================================ */

bool dwarf_symbols_lookup(
    dwarf_symbols_t syms,
    void* addr,
    symbol_info_t* info
) {
    if (!syms || !addr || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_lookup: required parameter is NULL (syms, addr, info)");
        return false;
    }

    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;

    if (s->magic != DWARF_SYMBOLS_MAGIC || !s->initialized) {
        LOG_MODULE_ERROR(LOG_MODULE, "Invalid or uninitialized dwarf_symbols");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_lookup: s->initialized is NULL");
        return false;
    }

    s->stats.total_lookups++;

    /* Initialize output */
    memset(info, 0, sizeof(symbol_info_t));

    /* Check cache first */
    if (cache_lookup(s, addr, info)) {
        return true;
    }

    symbol_source_t source = SYMBOL_SOURCE_NONE;
    bool found = false;

    /* Try DWARF first */
    if (lookup_via_dwarf(s, addr, info)) {
        source = SYMBOL_SOURCE_DWARF;
        found = true;
    }
    /* Fallback to addr2line */
    else if (lookup_via_addr2line(s, addr, info)) {
        source = SYMBOL_SOURCE_ADDR2LINE;
        found = true;
    }
    /* Fallback to dladdr */
    else if (lookup_via_dladdr(s, addr, info)) {
        source = SYMBOL_SOURCE_DLADDR;
        found = true;
    }

    if (found) {
        cache_insert(s, addr, info, source);
    } else {
        s->stats.failed_lookups++;
    }

    return found;
}

bool dwarf_symbols_lookup_extended(
    dwarf_symbols_t syms,
    void* addr,
    symbol_info_extended_t* info_ext
) {
    if (!syms || !addr || !info_ext) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_lookup_extended: required parameter is NULL (syms, addr, info_ext)");
        return false;
    }

    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;

    memset(info_ext, 0, sizeof(symbol_info_extended_t));
    info_ext->raw_address = addr;

    /* Try cache first */
    char key[32];
    format_addr_key(addr, key, sizeof(key));

    nimcp_platform_mutex_lock(&s->cache_lock);
    cache_entry_t* cached = (cache_entry_t*)hash_table_lookup_string(s->cache, key);
    if (cached) {
        info_ext->info = cached->info;
        info_ext->source = SYMBOL_SOURCE_CACHED;
        s->stats.cache_hits++;
        nimcp_platform_mutex_unlock(&s->cache_lock);
        return true;
    }
    nimcp_platform_mutex_unlock(&s->cache_lock);

    /* Full lookup */
    bool found = false;

    if (lookup_via_dwarf(s, addr, &info_ext->info)) {
        info_ext->source = SYMBOL_SOURCE_DWARF;
        found = true;
    } else if (lookup_via_addr2line(s, addr, &info_ext->info)) {
        info_ext->source = SYMBOL_SOURCE_ADDR2LINE;
        found = true;
    } else if (lookup_via_dladdr(s, addr, &info_ext->info)) {
        info_ext->source = SYMBOL_SOURCE_DLADDR;
        found = true;
    }

    if (found) {
        info_ext->file_address = dwarf_symbols_to_file_addr(syms, addr);
        info_ext->library_path = s->binary_path;
        cache_insert(s, addr, &info_ext->info, info_ext->source);
    } else {
        info_ext->source = SYMBOL_SOURCE_NONE;
        s->stats.failed_lookups++;
    }

    s->stats.total_lookups++;
    return found;
}

size_t dwarf_symbols_lookup_batch(
    dwarf_symbols_t syms,
    void** addrs,
    size_t count,
    symbol_info_t* infos
) {
    if (!syms || !addrs || !infos || count == 0) {
        return 0;
    }

    size_t resolved = 0;
    for (size_t i = 0; i < count; i++) {
        if (dwarf_symbols_lookup(syms, addrs[i], &infos[i])) {
            resolved++;
        }
    }
    return resolved;
}

/* ============================================================================
 * PUBLIC API - FUNCTION RANGE
 * ============================================================================ */

bool dwarf_symbols_get_function_range(
    dwarf_symbols_t syms,
    const char* func_name,
    void** start_addr,
    void** end_addr
) {
    if (!syms || !func_name || !start_addr || !end_addr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_get_function_range: required parameter is NULL (syms, func_name, start_addr, end_addr)");
        return false;
    }

#ifdef HAVE_LIBDW
    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;

    if (!s->dwfl || !s->dwarf_available) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_get_function_range: required parameter is NULL (s->dwfl, s->dwarf_available)");
        return false;
    }

    /* Search for function in all modules */
    Dwfl_Module* mod = NULL;
    Dwarf_Addr addr = 0;

    while ((mod = dwfl_nextmodule(s->dwfl, mod)) != NULL) {
        int nsyms = dwfl_module_getsymtab(mod);
        for (int i = 0; i < nsyms; i++) {
            GElf_Sym sym;
            GElf_Word shndx;
            const char* name = dwfl_module_getsym_info(mod, i, &sym, &addr, &shndx, NULL, NULL);

            if (name && strcmp(name, func_name) == 0 &&
                GELF_ST_TYPE(sym.st_info) == STT_FUNC) {
                *start_addr = (void*)(uintptr_t)sym.st_value;
                *end_addr = (void*)(uintptr_t)(sym.st_value + sym.st_size);
                return true;
            }
        }
    }
#else
    (void)syms;
    (void)func_name;
    (void)start_addr;
    (void)end_addr;
#endif

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dwarf_symbols_get_function_range: operation failed");
    return false;
}

bool dwarf_symbols_get_function_at(
    dwarf_symbols_t syms,
    void* addr,
    char* func_name,
    size_t func_name_size
) {
    if (!syms || !addr || !func_name || func_name_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_get_function_at: required parameter is NULL (syms, addr, func_name)");
        return false;
    }

    symbol_info_t info;
    if (dwarf_symbols_lookup(syms, addr, &info) && info.function_name[0]) {
        strncpy(func_name, info.function_name, func_name_size - 1);
        func_name[func_name_size - 1] = '\0';
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dwarf_symbols_get_function_at: validation failed");
    return false;
}

/* ============================================================================
 * PUBLIC API - LOCAL VARIABLES
 * ============================================================================ */

bool dwarf_symbols_get_locals(
    dwarf_symbols_t syms,
    void* addr,
    char* buffer,
    size_t buffer_size
) {
    if (!syms || !addr || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_get_locals: required parameter is NULL (syms, addr, buffer)");
        return false;
    }

    buffer[0] = '\0';

#ifdef HAVE_LIBDW
    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;

    if (!s->dwfl || !s->dwarf_available || !s->config.enable_locals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_get_locals: required parameter is NULL (s->dwfl, s->dwarf_available, s->config)");
        return false;
    }

    Dwarf_Addr dwarf_addr = (Dwarf_Addr)addr;
    if (s->is_pie) {
        dwarf_addr -= s->load_offset;
    }

    Dwfl_Module* mod = dwfl_addrmodule(s->dwfl, dwarf_addr);
    if (!mod) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_get_locals: mod is NULL");
        return false;
    }

    Dwarf_Addr bias;
    Dwarf* dwarf = dwfl_module_getdwarf(mod, &bias);
    if (!dwarf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_get_locals: dwarf is NULL");
        return false;
    }

    /* Get scopes at this address */
    Dwarf_Die* scopes = NULL;
    int nscopes = dwfl_module_addrscopes(mod, dwarf_addr, &scopes);
    if (nscopes <= 0 || !scopes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_get_locals: scopes is NULL");
        return false;
    }

    size_t offset = 0;
    bool found_any = false;

    /* Iterate through scopes looking for local variables */
    for (int i = 0; i < nscopes && offset < buffer_size - 1; i++) {
        Dwarf_Die child;
        if (dwarf_child(&scopes[i], &child) == 0) {
            do {
                int tag = dwarf_tag(&child);
                if (tag == DW_TAG_variable || tag == DW_TAG_formal_parameter) {
                    const char* var_name = dwarf_diename(&child);
                    if (var_name) {
                        /* Get type name */
                        Dwarf_Die type_die;
                        const char* type_name = "unknown";
                        if (dwarf_attr_die(&child, DW_AT_type, &type_die)) {
                            const char* tn = dwarf_diename(&type_die);
                            if (tn) type_name = tn;
                        }

                        int written = snprintf(buffer + offset, buffer_size - offset,
                                               "%s: %s\n", var_name, type_name);
                        if (written > 0 && (size_t)written < buffer_size - offset) {
                            offset += written;
                            found_any = true;
                        }
                    }
                }
            } while (dwarf_siblingof(&child, &child) == 0 && offset < buffer_size - 1);
        }
    }

    nimcp_free(scopes);
    return found_any;
#else
    (void)syms;
    (void)addr;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dwarf_symbols_get_locals: operation failed");
    return false;
#endif
}

/* ============================================================================
 * PUBLIC API - PIE/ASLR SUPPORT
 * ============================================================================ */

ptrdiff_t dwarf_symbols_get_load_offset(dwarf_symbols_t syms) {
    if (!syms) {
        return 0;
    }
    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;
    return s->load_offset;
}

void* dwarf_symbols_to_file_addr(dwarf_symbols_t syms, void* runtime_addr) {
    if (!syms || !runtime_addr) {
        return runtime_addr;
    }
    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;
    return (void*)((uintptr_t)runtime_addr - s->load_offset);
}

void* dwarf_symbols_to_runtime_addr(dwarf_symbols_t syms, void* file_addr) {
    if (!syms || !file_addr) {
        return file_addr;
    }
    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;
    return (void*)((uintptr_t)file_addr + s->load_offset);
}

/* ============================================================================
 * PUBLIC API - CACHE MANAGEMENT
 * ============================================================================ */

void dwarf_symbols_cache_clear(dwarf_symbols_t syms) {
    if (!syms) {
        return;
    }
    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;

    nimcp_platform_mutex_lock(&s->cache_lock);
    if (s->cache) {
        hash_table_clear(s->cache);
    }
    s->cache_access_counter = 0;
    nimcp_platform_mutex_unlock(&s->cache_lock);

    LOG_MODULE_DEBUG(LOG_MODULE, "Symbol cache cleared");
}

size_t dwarf_symbols_cache_preload(
    dwarf_symbols_t syms,
    void** addrs,
    size_t count
) {
    if (!syms || !addrs || count == 0) {
        return 0;
    }

    symbol_info_t info;
    size_t loaded = 0;

    for (size_t i = 0; i < count; i++) {
        if (dwarf_symbols_lookup(syms, addrs[i], &info)) {
            loaded++;
        }
    }

    return loaded;
}

/* ============================================================================
 * PUBLIC API - STATISTICS
 * ============================================================================ */

bool dwarf_symbols_get_stats(
    dwarf_symbols_t syms,
    dwarf_symbols_stats_t* stats
) {
    if (!syms || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_get_stats: required parameter is NULL (syms, stats)");
        return false;
    }

    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;
    if (s->magic != DWARF_SYMBOLS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dwarf_symbols_get_stats: validation failed");
        return false;
    }

    nimcp_platform_mutex_lock(&s->cache_lock);
    *stats = s->stats;

    /* Calculate memory usage */
    stats->memory_used = sizeof(struct dwarf_symbols);
    if (s->cache) {
        stats->memory_used += hash_table_size(s->cache) * CACHE_ENTRY_SIZE;
    }

    nimcp_platform_mutex_unlock(&s->cache_lock);
    return true;
}

void dwarf_symbols_reset_stats(dwarf_symbols_t syms) {
    if (!syms) {
        return;
    }

    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;
    if (s->magic != DWARF_SYMBOLS_MAGIC) {
        return;
    }

    nimcp_platform_mutex_lock(&s->cache_lock);
    memset(&s->stats, 0, sizeof(dwarf_symbols_stats_t));
    nimcp_platform_mutex_unlock(&s->cache_lock);
}

void dwarf_symbols_print_stats(dwarf_symbols_t syms) {
    dwarf_symbols_stats_t stats;
    if (!dwarf_symbols_get_stats(syms, &stats)) {
        printf("DWARF Symbols: invalid\n");
        return;
    }

    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;

    printf("\n=== DWARF Symbol Statistics ===\n");
    printf("Binary: %s\n", s->binary_path);
    printf("PIE: %s (offset=%ld)\n", s->is_pie ? "yes" : "no", (long)s->load_offset);
    printf("DWARF available: %s\n",
#ifdef HAVE_LIBDW
           s->dwarf_available ? "yes" : "no"
#else
           "no (libdw not compiled)"
#endif
    );
    printf("addr2line available: %s\n", s->addr2line_available ? "yes" : "no");
    printf("\n");
    printf("Total lookups: %lu\n", stats.total_lookups);
    printf("Cache hits: %lu (%.1f%%)\n", stats.cache_hits,
           stats.total_lookups > 0 ? (double)stats.cache_hits / stats.total_lookups * 100 : 0);
    printf("DWARF lookups: %lu\n", stats.dwarf_lookups);
    printf("addr2line lookups: %lu\n", stats.addr2line_lookups);
    printf("dladdr lookups: %lu\n", stats.dladdr_lookups);
    printf("Failed lookups: %lu\n", stats.failed_lookups);
    printf("Inline functions detected: %lu\n", stats.inline_detected);
    printf("Memory used: ~%lu bytes\n", stats.memory_used);
    printf("=================================\n\n");
}

bool dwarf_symbols_has_dwarf(dwarf_symbols_t syms) {
    if (!syms) {
        return false;
    }
    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;
#ifdef HAVE_LIBDW
    return s->dwarf_available;
#else
    (void)s;
    return false;
#endif
}

const char* dwarf_symbols_get_binary_path(dwarf_symbols_t syms) {
    if (!syms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "syms is NULL");

        return NULL;
    }
    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;
    return s->binary_path;
}

bool dwarf_symbols_validate(dwarf_symbols_t syms) {
    if (!syms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_validate: syms is NULL");
        return false;
    }

    struct dwarf_symbols* s = (struct dwarf_symbols*)syms;

    if (s->magic != DWARF_SYMBOLS_MAGIC) {
        LOG_MODULE_ERROR(LOG_MODULE, "Validation failed: bad magic");
        return false;
    }

    if (!s->initialized) {
        LOG_MODULE_ERROR(LOG_MODULE, "Validation failed: not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_validate: s->initialized is NULL");
        return false;
    }

    if (!s->cache) {
        LOG_MODULE_ERROR(LOG_MODULE, "Validation failed: no cache");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dwarf_symbols_validate: s->cache is NULL");
        return false;
    }

    if (s->binary_path[0] == '\0') {
        LOG_MODULE_ERROR(LOG_MODULE, "Validation failed: no binary path");
        return false;
    }

    return true;
}

/* ============================================================================
 * PUBLIC API - UTILITY
 * ============================================================================ */

size_t dwarf_symbols_format_info(
    const symbol_info_t* info,
    char* buffer,
    size_t buffer_size
) {
    if (!info || !buffer || buffer_size == 0) {
        return 0;
    }

    int written;

    if (info->function_name[0] && info->source_file[0]) {
        if (info->is_inline && info->inline_caller[0]) {
            written = snprintf(buffer, buffer_size,
                               "%s (inlined from %s:%u) at %s:%u",
                               info->function_name,
                               info->inline_caller,
                               info->inline_caller_line,
                               info->source_file,
                               info->line_number);
        } else {
            written = snprintf(buffer, buffer_size,
                               "%s at %s:%u",
                               info->function_name,
                               info->source_file,
                               info->line_number);
        }
    } else if (info->function_name[0]) {
        written = snprintf(buffer, buffer_size, "%s", info->function_name);
    } else if (info->source_file[0]) {
        written = snprintf(buffer, buffer_size, "%s:%u",
                           info->source_file, info->line_number);
    } else {
        written = snprintf(buffer, buffer_size, "(unknown)");
    }

    return (written > 0) ? (size_t)written : 0;
}

const char* dwarf_symbols_source_name(symbol_source_t source) {
    switch (source) {
        case SYMBOL_SOURCE_NONE:      return "none";
        case SYMBOL_SOURCE_DWARF:     return "DWARF";
        case SYMBOL_SOURCE_ADDR2LINE: return "addr2line";
        case SYMBOL_SOURCE_DLADDR:    return "dladdr";
        case SYMBOL_SOURCE_CACHED:    return "cached";
        default:                      return "unknown";
    }
}
