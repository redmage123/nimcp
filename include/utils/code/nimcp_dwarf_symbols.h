/**
 * @file nimcp_dwarf_symbols.h
 * @brief DWARF debug symbols parser for self-healing fault tolerance
 *
 * WHAT: Address-to-source mapping using DWARF debug information
 * WHY: Enable precise error location for diagnostics, debugging, and self-healing
 * HOW: Parse DWARF info (via libdw), fallback to addr2line, dladdr for basic symbols
 *
 * ARCHITECTURE:
 *
 *   Stack Trace                Symbol Parser                    Sources
 *   ┌─────────────┐           ┌────────────────────┐           ┌─────────────┐
 *   │ 0x7f...1234 │──────────>│ dwarf_symbols_t    │           │ libdw       │
 *   │ 0x7f...5678 │           │ ┌────────────────┐ │   #1      │ (elfutils)  │
 *   └─────────────┘           │ │ DWARF Parser   │<┼───────────┤             │
 *          │                  │ └────────────────┘ │           └─────────────┘
 *          │                  │ ┌────────────────┐ │   #2      ┌─────────────┐
 *          └─────────────────>│ │ addr2line     │<┼───────────┤ /usr/bin/   │
 *                             │ │ (fallback)    │ │           │ addr2line   │
 *                             │ └────────────────┘ │           └─────────────┘
 *                             │ ┌────────────────┐ │   #3      ┌─────────────┐
 *                             │ │ dladdr        │<┼───────────┤ ELF symtab  │
 *                             │ │ (fallback)    │ │           │ (dlopen)    │
 *                             │ └────────────────┘ │           └─────────────┘
 *                             │ ┌────────────────┐ │
 *                             │ │ LRU Cache     │ │
 *                             │ │ (results)     │ │
 *                             │ └────────────────┘ │
 *                             └────────────────────┘
 *
 * FALLBACK STRATEGY:
 * 1. libdw (elfutils) - Full DWARF parsing with inline detection
 * 2. addr2line - Fork/exec external tool, parse output
 * 3. dladdr - Basic symbol info from ELF symbol table
 * 4. Address-only - Return hex address if all else fails
 *
 * FEATURES:
 * - DWARF parsing for complete debug info (file, line, column, function)
 * - Inline function detection and unwinding
 * - PIE (Position Independent Executable) support
 * - Result caching for repeated lookups (common in stack traces)
 * - Local variable extraction (if DWARF info available)
 * - Function range queries for disassembly context
 *
 * PERFORMANCE:
 * - Initial load: O(n) where n = DWARF section size
 * - Cached lookup: O(1) hash table lookup
 * - Uncached lookup: O(log n) binary search in debug_line
 *
 * USAGE:
 *   dwarf_symbols_t syms = dwarf_symbols_create("/path/to/binary");
 *   if (!syms) {
 *       LOG_ERROR("Failed to load symbols");
 *       return;
 *   }
 *
 *   symbol_info_t info;
 *   if (dwarf_symbols_lookup(syms, fault_address, &info)) {
 *       printf("Fault at %s:%u in %s()\n",
 *              info.source_file, info.line_number, info.function_name);
 *   }
 *
 *   dwarf_symbols_destroy(syms);
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_DWARF_SYMBOLS_H
#define NIMCP_DWARF_SYMBOLS_H

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
#define DWARF_SYMBOLS_MAX_PATH 256

/** Maximum function name length */
#define DWARF_SYMBOLS_MAX_FUNC 128

/** Maximum local variable buffer size */
#define DWARF_SYMBOLS_MAX_LOCALS 1024

/** Default cache size (number of entries) */
#define DWARF_SYMBOLS_DEFAULT_CACHE_SIZE 256

/** Magic value for validation */
#define DWARF_SYMBOLS_MAGIC 0x44574152  /* 'DWAR' */

/* ============================================================================
 * TYPE DEFINITIONS
 * ============================================================================ */

/**
 * @brief DWARF symbols handle (opaque)
 */
typedef struct dwarf_symbols* dwarf_symbols_t;

/**
 * @brief Symbol lookup result
 *
 * WHAT: Complete symbol information for an address
 * WHY: Provide all available debug info for diagnostics
 * HOW: Populated from DWARF, addr2line, or dladdr
 */
typedef struct {
    char source_file[DWARF_SYMBOLS_MAX_PATH];   /**< Source file path */
    char function_name[DWARF_SYMBOLS_MAX_FUNC]; /**< Function name */
    uint32_t line_number;                        /**< Line number (1-indexed, 0 = unknown) */
    uint32_t column;                             /**< Column number (1-indexed, 0 = unknown) */
    bool is_inline;                              /**< True if address is in inlined function */
    char inline_caller[DWARF_SYMBOLS_MAX_FUNC]; /**< If inlined, the caller function */
    uint32_t inline_caller_line;                 /**< Line in caller where inlined */
} symbol_info_t;

/**
 * @brief Symbol lookup source (how the info was obtained)
 */
typedef enum {
    SYMBOL_SOURCE_NONE = 0,     /**< No symbol info available */
    SYMBOL_SOURCE_DWARF,        /**< Full DWARF debug info */
    SYMBOL_SOURCE_ADDR2LINE,    /**< From addr2line external tool */
    SYMBOL_SOURCE_DLADDR,       /**< From dladdr (ELF symbol table only) */
    SYMBOL_SOURCE_CACHED        /**< From internal cache */
} symbol_source_t;

/**
 * @brief Extended symbol info with metadata
 */
typedef struct {
    symbol_info_t info;         /**< Core symbol information */
    symbol_source_t source;     /**< How the info was obtained */
    void* raw_address;          /**< Original runtime address */
    void* file_address;         /**< Address relative to file (for PIE) */
    const char* library_path;   /**< Library/binary containing the symbol */
} symbol_info_extended_t;

/**
 * @brief DWARF symbols configuration
 */
typedef struct {
    size_t cache_size;          /**< Maximum cached lookups (0 = default) */
    bool enable_inline_unwind;  /**< Detect and unwind inline functions */
    bool enable_locals;         /**< Enable local variable extraction */
    bool auto_detect_pie;       /**< Auto-detect PIE and adjust addresses */
    const char* addr2line_path; /**< Custom addr2line path (NULL = auto) */
} dwarf_symbols_config_t;

/**
 * @brief DWARF symbols statistics
 */
typedef struct {
    uint64_t total_lookups;     /**< Total lookup requests */
    uint64_t cache_hits;        /**< Cache hits */
    uint64_t dwarf_lookups;     /**< Successful DWARF lookups */
    uint64_t addr2line_lookups; /**< Fallback to addr2line */
    uint64_t dladdr_lookups;    /**< Fallback to dladdr */
    uint64_t failed_lookups;    /**< Lookups that returned no info */
    uint64_t inline_detected;   /**< Inline functions detected */
    size_t   memory_used;       /**< Approximate memory usage */
} dwarf_symbols_stats_t;

/* ============================================================================
 * LIFECYCLE FUNCTIONS
 * ============================================================================ */

/**
 * @brief Create DWARF symbols parser for a binary
 *
 * WHAT: Initialize DWARF parser for specified binary
 * WHY: Enable address-to-source mapping for diagnostics
 * HOW: Load DWARF info (if available), detect PIE, set up fallbacks
 *
 * @param binary_path Path to binary (NULL for current executable)
 * @return Symbol parser handle or NULL on failure
 *
 * COMPLEXITY: O(n) where n = DWARF section size
 * THREAD-SAFE: Yes (after creation)
 *
 * @note For self, use NULL or "/proc/self/exe"
 * @note Call dwarf_symbols_destroy() to clean up
 */
NIMCP_EXPORT dwarf_symbols_t dwarf_symbols_create(const char* binary_path);

/**
 * @brief Create DWARF symbols parser with configuration
 *
 * WHAT: Initialize with custom configuration
 * WHY: Allow tuning behavior (cache size, fallback preferences)
 * HOW: Create parser and apply config
 *
 * @param binary_path Path to binary (NULL for current executable)
 * @param config Configuration options (NULL for defaults)
 * @return Symbol parser handle or NULL on failure
 */
NIMCP_EXPORT dwarf_symbols_t dwarf_symbols_create_with_config(
    const char* binary_path,
    const dwarf_symbols_config_t* config
);

/**
 * @brief Destroy DWARF symbols parser
 *
 * WHAT: Free all resources
 * WHY: Clean shutdown
 * HOW: Close DWARF handles, free cache, release memory
 *
 * @param syms Symbol parser handle (NULL is safe)
 *
 * COMPLEXITY: O(n) where n = cached entries
 */
NIMCP_EXPORT void dwarf_symbols_destroy(dwarf_symbols_t syms);

/**
 * @brief Get default configuration
 *
 * @return Default configuration values
 */
NIMCP_EXPORT dwarf_symbols_config_t dwarf_symbols_default_config(void);

/* ============================================================================
 * CORE LOOKUP FUNCTIONS
 * ============================================================================ */

/**
 * @brief Look up symbol information for an address
 *
 * WHAT: Map runtime address to source file:line
 * WHY: Enable precise error location in diagnostics
 * HOW: Check cache, try DWARF, fallback to addr2line/dladdr
 *
 * @param syms Symbol parser handle
 * @param addr Runtime address to look up
 * @param info Output symbol information
 * @return true if symbol found, false if not found
 *
 * COMPLEXITY: O(1) cached, O(log n) uncached
 * THREAD-SAFE: Yes (internal locking)
 *
 * @note Address should be a valid instruction pointer
 * @note Empty strings in info indicate unavailable data
 *
 * EXAMPLE:
 *   void* frames[32];
 *   int depth = backtrace(frames, 32);
 *   for (int i = 0; i < depth; i++) {
 *       symbol_info_t info;
 *       if (dwarf_symbols_lookup(syms, frames[i], &info)) {
 *           printf("#%d %s at %s:%u\n",
 *                  i, info.function_name, info.source_file, info.line_number);
 *       }
 *   }
 */
NIMCP_EXPORT bool dwarf_symbols_lookup(
    dwarf_symbols_t syms,
    void* addr,
    symbol_info_t* info
);

/**
 * @brief Look up symbol with extended metadata
 *
 * WHAT: Map address to source with lookup source info
 * WHY: Know how reliable the info is (DWARF vs dladdr)
 * HOW: Same as dwarf_symbols_lookup but returns source type
 *
 * @param syms Symbol parser handle
 * @param addr Runtime address to look up
 * @param info_ext Output extended symbol information
 * @return true if any symbol found, false if not found
 */
NIMCP_EXPORT bool dwarf_symbols_lookup_extended(
    dwarf_symbols_t syms,
    void* addr,
    symbol_info_extended_t* info_ext
);

/**
 * @brief Batch lookup for multiple addresses
 *
 * WHAT: Look up symbols for array of addresses
 * WHY: Efficient stack trace symbolication
 * HOW: Batch process with shared cache
 *
 * @param syms Symbol parser handle
 * @param addrs Array of addresses
 * @param count Number of addresses
 * @param infos Output array of symbol info (must be pre-allocated)
 * @return Number of successfully resolved addresses
 *
 * COMPLEXITY: O(n) where n = count
 * THREAD-SAFE: Yes
 */
NIMCP_EXPORT size_t dwarf_symbols_lookup_batch(
    dwarf_symbols_t syms,
    void** addrs,
    size_t count,
    symbol_info_t* infos
);

/* ============================================================================
 * FUNCTION RANGE QUERIES
 * ============================================================================ */

/**
 * @brief Get address range for a function
 *
 * WHAT: Find start and end addresses of a function
 * WHY: Get function boundaries for disassembly or patching
 * HOW: Search DWARF DIEs for function entries
 *
 * @param syms Symbol parser handle
 * @param func_name Function name to find
 * @param start_addr Output start address
 * @param end_addr Output end address (exclusive)
 * @return true if function found, false otherwise
 *
 * COMPLEXITY: O(n) where n = number of functions
 * THREAD-SAFE: Yes
 *
 * @note Returns file addresses (not runtime for PIE)
 */
NIMCP_EXPORT bool dwarf_symbols_get_function_range(
    dwarf_symbols_t syms,
    const char* func_name,
    void** start_addr,
    void** end_addr
);

/**
 * @brief Get function containing an address
 *
 * WHAT: Find function name that contains given address
 * WHY: Identify current function for diagnostics
 * HOW: Binary search function ranges
 *
 * @param syms Symbol parser handle
 * @param addr Runtime address
 * @param func_name Output buffer for function name
 * @param func_name_size Size of output buffer
 * @return true if function found, false otherwise
 */
NIMCP_EXPORT bool dwarf_symbols_get_function_at(
    dwarf_symbols_t syms,
    void* addr,
    char* func_name,
    size_t func_name_size
);

/* ============================================================================
 * LOCAL VARIABLE ACCESS
 * ============================================================================ */

/**
 * @brief Get local variables at an address
 *
 * WHAT: Extract local variable info from DWARF
 * WHY: Provide context for crash analysis
 * HOW: Parse DW_TAG_variable entries in current scope
 *
 * @param syms Symbol parser handle
 * @param addr Runtime address (instruction pointer)
 * @param buffer Output buffer for formatted variable info
 * @param buffer_size Size of output buffer
 * @return true if any locals found, false otherwise
 *
 * COMPLEXITY: O(n) where n = number of local variables
 * THREAD-SAFE: Yes
 *
 * @note Requires DWARF debug info with variable locations
 * @note Output format: "name: type @ location\n..."
 *
 * EXAMPLE OUTPUT:
 *   buffer = 42
 *   count = 100
 *   ptr = 0x7fff5fbff8d0
 */
NIMCP_EXPORT bool dwarf_symbols_get_locals(
    dwarf_symbols_t syms,
    void* addr,
    char* buffer,
    size_t buffer_size
);

/* ============================================================================
 * PIE/ASLR SUPPORT
 * ============================================================================ */

/**
 * @brief Get load offset for PIE binary
 *
 * WHAT: Get offset between file addresses and runtime addresses
 * WHY: Adjust addresses for PIE/ASLR binaries
 * HOW: Parse /proc/self/maps for load address
 *
 * @param syms Symbol parser handle
 * @return Load offset (add to file address to get runtime)
 *
 * @note Returns 0 for non-PIE binaries
 */
NIMCP_EXPORT ptrdiff_t dwarf_symbols_get_load_offset(dwarf_symbols_t syms);

/**
 * @brief Convert runtime address to file address
 *
 * WHAT: Adjust runtime address for DWARF lookup
 * WHY: DWARF contains file addresses, not runtime
 * HOW: Subtract load offset
 *
 * @param syms Symbol parser handle
 * @param runtime_addr Runtime address (from stack trace)
 * @return File address (for DWARF lookup)
 */
NIMCP_EXPORT void* dwarf_symbols_to_file_addr(
    dwarf_symbols_t syms,
    void* runtime_addr
);

/**
 * @brief Convert file address to runtime address
 *
 * WHAT: Adjust file address to runtime
 * WHY: Convert DWARF addresses to runtime for patching
 * HOW: Add load offset
 *
 * @param syms Symbol parser handle
 * @param file_addr File address (from DWARF)
 * @return Runtime address
 */
NIMCP_EXPORT void* dwarf_symbols_to_runtime_addr(
    dwarf_symbols_t syms,
    void* file_addr
);

/* ============================================================================
 * CACHE MANAGEMENT
 * ============================================================================ */

/**
 * @brief Clear symbol lookup cache
 *
 * WHAT: Remove all cached lookups
 * WHY: Force fresh lookups after binary changes
 * HOW: Clear internal hash table
 *
 * @param syms Symbol parser handle
 */
NIMCP_EXPORT void dwarf_symbols_cache_clear(dwarf_symbols_t syms);

/**
 * @brief Preload cache with common addresses
 *
 * WHAT: Pre-populate cache with given addresses
 * WHY: Optimize subsequent batch lookups
 * HOW: Perform lookups and cache results
 *
 * @param syms Symbol parser handle
 * @param addrs Array of addresses to preload
 * @param count Number of addresses
 * @return Number of addresses successfully cached
 */
NIMCP_EXPORT size_t dwarf_symbols_cache_preload(
    dwarf_symbols_t syms,
    void** addrs,
    size_t count
);

/* ============================================================================
 * STATISTICS AND DIAGNOSTICS
 * ============================================================================ */

/**
 * @brief Get parser statistics
 *
 * WHAT: Retrieve lookup performance metrics
 * WHY: Monitor cache effectiveness and fallback usage
 * HOW: Copy statistics structure
 *
 * @param syms Symbol parser handle
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool dwarf_symbols_get_stats(
    dwarf_symbols_t syms,
    dwarf_symbols_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear all counters
 * WHY: Start fresh measurement period
 *
 * @param syms Symbol parser handle
 */
NIMCP_EXPORT void dwarf_symbols_reset_stats(dwarf_symbols_t syms);

/**
 * @brief Print statistics to stdout
 *
 * @param syms Symbol parser handle
 */
NIMCP_EXPORT void dwarf_symbols_print_stats(dwarf_symbols_t syms);

/**
 * @brief Check if DWARF info is available
 *
 * WHAT: Check if binary has DWARF debug info
 * WHY: Know if we'll get full symbols or just function names
 * HOW: Check for presence of .debug_info section
 *
 * @param syms Symbol parser handle
 * @return true if DWARF info is available
 */
NIMCP_EXPORT bool dwarf_symbols_has_dwarf(dwarf_symbols_t syms);

/**
 * @brief Get binary path
 *
 * @param syms Symbol parser handle
 * @return Path to binary being analyzed
 */
NIMCP_EXPORT const char* dwarf_symbols_get_binary_path(dwarf_symbols_t syms);

/**
 * @brief Validate parser state
 *
 * WHAT: Check internal data structure consistency
 * WHY: Debugging and testing
 *
 * @param syms Symbol parser handle
 * @return true if parser is consistent
 */
NIMCP_EXPORT bool dwarf_symbols_validate(dwarf_symbols_t syms);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Format symbol info as string
 *
 * WHAT: Create human-readable string from symbol info
 * WHY: Easy display in logs and diagnostics
 * HOW: Format as "function at file:line"
 *
 * @param info Symbol info to format
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of bytes written (excluding null)
 *
 * EXAMPLE OUTPUT:
 *   "brain_update at src/core/brain.c:142"
 *   "handle_signal (inlined from main) at src/main.c:55"
 */
NIMCP_EXPORT size_t dwarf_symbols_format_info(
    const symbol_info_t* info,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Get source name for lookup type
 *
 * @param source Symbol source type
 * @return String name ("DWARF", "addr2line", etc.)
 */
NIMCP_EXPORT const char* dwarf_symbols_source_name(symbol_source_t source);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DWARF_SYMBOLS_H */
