/**
 * @file nimcp_heal_patterns.h
 * @brief Self-Healing Fix Patterns - Pattern Library for Crash Recovery
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Library of code fix patterns for common crash types
 * WHY:  Enable pattern-based automated code repair without LNN for simple cases
 * HOW:  Define templates for NULL checks, bounds checks, div-zero, UAF, alignment
 *
 * BIOLOGICAL BASIS:
 * ```
 * BIOLOGICAL CONCEPT              NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Innate Immune Response       → Pattern-based fixes (fast, pre-defined)
 * Adaptive Immune Response     → LNN-learned fixes (slower, flexible)
 * Antibody Repertoire          → Fix pattern library
 * Pattern Recognition Receptor → Crash signature matching
 * Memory B Cells               → Cached successful fix patterns
 * ```
 *
 * PATTERN TYPES:
 * - NULL_CHECK: Add null pointer validation before dereference
 * - BOUNDS_CHECK: Add array bounds validation before access
 * - ZERO_CHECK: Add divisor validation before division
 * - UAF_CHECK: Add use-after-free protection
 * - ALIGN_FIX: Fix alignment issues with memcpy
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HEAL_PATTERNS_H
#define NIMCP_HEAL_PATTERNS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define HEAL_PATTERN_MAX_TEMPLATE_SIZE    2048   /**< Max template string size */
#define HEAL_PATTERN_MAX_NAME_SIZE        64     /**< Max pattern name length */
#define HEAL_PATTERN_MAX_DESCRIPTION_SIZE 256    /**< Max description length */
#define HEAL_PATTERN_MAX_CUSTOM_PATTERNS  64     /**< Max custom patterns */
#define HEAL_PATTERN_DEFAULT_CONFIDENCE   0.8f   /**< Default pattern confidence */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Fix pattern types for common crashes
 *
 * BIOLOGICAL BASIS:
 * Each pattern type represents a "pre-formed antibody" - a known fix
 * for a specific class of threats, like innate immunity.
 */
typedef enum {
    FIX_PATTERN_NULL_CHECK = 0,    /**< NULL pointer dereference fix */
    FIX_PATTERN_BOUNDS_CHECK,      /**< Array bounds check fix */
    FIX_PATTERN_ZERO_CHECK,        /**< Division by zero fix */
    FIX_PATTERN_UAF_CHECK,         /**< Use-after-free fix */
    FIX_PATTERN_ALIGN_FIX,         /**< Memory alignment fix */
    FIX_PATTERN_DOUBLE_FREE,       /**< Double-free protection */
    FIX_PATTERN_OVERFLOW_CHECK,    /**< Integer overflow check */
    FIX_PATTERN_INIT_CHECK,        /**< Uninitialized variable check */
    FIX_PATTERN_LOCK_ORDER,        /**< Deadlock prevention (lock ordering) */
    FIX_PATTERN_RACE_GUARD,        /**< Race condition guard */
    FIX_PATTERN_RESOURCE_LEAK,     /**< Resource leak - unclosed handles/memory */
    FIX_PATTERN_FORMAT_STRING,     /**< Format string vulnerability fix */
    FIX_PATTERN_BUFFER_UNDERFLOW,  /**< Buffer underflow - negative index fix */
    FIX_PATTERN_STACK_OVERFLOW,    /**< Stack overflow - recursion depth limit */
    FIX_PATTERN_LNN_GENERATED,     /**< Neural network generated fix */
    FIX_PATTERN_CUSTOM,            /**< User-defined custom pattern */
    FIX_PATTERN_UNKNOWN,           /**< Unknown/unclassified pattern */
    FIX_PATTERN_COUNT
} fix_pattern_type_t;

/**
 * @brief Pattern matching strategy
 *
 * WHAT: How to identify crash type from crash signature
 * WHY:  Different strategies for different accuracy/speed tradeoffs
 */
typedef enum {
    PATTERN_MATCH_EXACT = 0,       /**< Exact string match */
    PATTERN_MATCH_SUBSTRING,       /**< Substring containment */
    PATTERN_MATCH_REGEX,           /**< Regular expression match */
    PATTERN_MATCH_FUZZY,           /**< Fuzzy/approximate matching */
    PATTERN_MATCH_LNN              /**< LNN-based semantic matching */
} pattern_match_strategy_t;

/**
 * @brief Pattern applicability scope
 *
 * WHAT: Where the pattern can be applied
 * WHY:  Some patterns are language/context specific
 */
typedef enum {
    PATTERN_SCOPE_UNIVERSAL = 0,   /**< Applies to any C code */
    PATTERN_SCOPE_FUNCTION,        /**< Applies within function scope */
    PATTERN_SCOPE_BLOCK,           /**< Applies to code block only */
    PATTERN_SCOPE_EXPRESSION       /**< Applies to single expression */
} pattern_scope_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Fix pattern definition
 *
 * WHAT: Complete specification of a code fix pattern
 * WHY:  Templates enable generating fixes without full code analysis
 * HOW:  Match pattern, extract variables, apply template transformation
 */
typedef struct {
    fix_pattern_type_t type;                     /**< Pattern type identifier */
    char name[HEAL_PATTERN_MAX_NAME_SIZE];       /**< Human-readable name */
    char description[HEAL_PATTERN_MAX_DESCRIPTION_SIZE]; /**< Detailed description */

    /* Pattern templates */
    char template_before[HEAL_PATTERN_MAX_TEMPLATE_SIZE]; /**< Pattern to match */
    char template_after[HEAL_PATTERN_MAX_TEMPLATE_SIZE];  /**< Fixed code template */

    /* Matching configuration */
    pattern_match_strategy_t match_strategy;     /**< How to match patterns */
    pattern_scope_t scope;                       /**< Where pattern applies */

    /* Statistics */
    float confidence;                            /**< Confidence score (0-1) */
    uint32_t success_count;                      /**< Successful applications */
    uint32_t fail_count;                         /**< Failed applications */
    uint32_t total_applications;                 /**< Total applications */

    /* Metadata */
    uint32_t id;                                 /**< Unique pattern ID */
    uint64_t created_time;                       /**< Pattern creation time */
    uint64_t last_used_time;                     /**< Last successful use time */
    bool enabled;                                /**< Pattern is active */
    bool user_defined;                           /**< User-created pattern */
} fix_pattern_t;

/**
 * @brief Pattern match result
 *
 * WHAT: Result of pattern matching against code
 * WHY:  Provides extracted variables and match quality
 */
typedef struct {
    fix_pattern_type_t matched_type;             /**< Matched pattern type */
    float match_score;                           /**< Match quality (0-1) */
    size_t match_offset;                         /**< Offset in source code */
    size_t match_length;                         /**< Length of matched region */

    /* Extracted variables from pattern */
    char var_ptr[64];                            /**< Pointer variable name */
    char var_idx[64];                            /**< Index variable name */
    char var_size[64];                           /**< Size variable name */
    char var_divisor[64];                        /**< Divisor variable name */
    char var_type[64];                           /**< Type name */

    bool valid;                                  /**< Match is valid */
} pattern_match_result_t;

/**
 * @brief Pattern library
 *
 * WHAT: Collection of fix patterns with lookup capabilities
 * WHY:  Centralized pattern management and retrieval
 */
typedef struct {
    fix_pattern_t* builtin_patterns;             /**< Built-in patterns */
    size_t builtin_count;                        /**< Number of built-in patterns */

    fix_pattern_t* custom_patterns;              /**< User-defined patterns */
    size_t custom_count;                         /**< Number of custom patterns */
    size_t custom_capacity;                      /**< Custom pattern capacity */

    uint32_t next_pattern_id;                    /**< Next pattern ID to assign */
    void* mutex;                                 /**< Thread safety mutex */
    bool initialized;                            /**< Library initialized */
} pattern_library_t;

/* ============================================================================
 * Built-in Pattern Templates
 * ============================================================================ */

/**
 * @brief NULL pointer check template
 *
 * BEFORE: ptr->member
 * AFTER:  if (ptr == NULL) { return NIMCP_ERROR_NULL_POINTER; }
 *         ptr->member
 */
#define HEAL_PATTERN_NULL_BEFORE   "${ptr}->${member}"
#define HEAL_PATTERN_NULL_AFTER    \
    "if (${ptr} == NULL) { return NIMCP_ERROR_NULL_POINTER; }\n${ptr}->${member}"

/**
 * @brief Bounds check template
 *
 * BEFORE: array[idx]
 * AFTER:  if (idx >= size) { return NIMCP_ERROR_OUT_OF_RANGE; }
 *         array[idx]
 */
#define HEAL_PATTERN_BOUNDS_BEFORE "${array}[${idx}]"
#define HEAL_PATTERN_BOUNDS_AFTER  \
    "if (${idx} >= ${size}) { return NIMCP_ERROR_OUT_OF_RANGE; }\n${array}[${idx}]"

/**
 * @brief Division by zero check template
 *
 * BEFORE: a / b
 * AFTER:  if (b == 0) { return 0; }
 *         a / b
 */
#define HEAL_PATTERN_DIVZERO_BEFORE "${numerator} / ${divisor}"
#define HEAL_PATTERN_DIVZERO_AFTER  \
    "if (${divisor} == 0) { return 0; }\n${numerator} / ${divisor}"

/**
 * @brief Use-after-free check template
 *
 * BEFORE: nimcp_free(ptr); ... ptr->member;
 * AFTER:  nimcp_free(ptr); ptr = NULL; ... if (ptr != NULL) ptr->member;
 */
#define HEAL_PATTERN_UAF_FREE_AFTER   "nimcp_free(${ptr});\n${ptr} = NULL;"
#define HEAL_PATTERN_UAF_USE_BEFORE   "${ptr}->${member}"
#define HEAL_PATTERN_UAF_USE_AFTER    "if (${ptr} != NULL) ${ptr}->${member}"

/**
 * @brief Alignment fix template
 *
 * BEFORE: *(type*)ptr
 * AFTER:  type temp; memcpy(&temp, ptr, sizeof(type)); temp
 */
#define HEAL_PATTERN_ALIGN_BEFORE  "*(${type}*)${ptr}"
#define HEAL_PATTERN_ALIGN_AFTER   \
    "do { ${type} _aligned_tmp; memcpy(&_aligned_tmp, ${ptr}, sizeof(${type})); _aligned_tmp; } while(0)"

/**
 * @brief Double-free protection template
 *
 * BEFORE: nimcp_free(ptr);
 * AFTER:  if (ptr != NULL) { nimcp_free(ptr); ptr = NULL; }
 */
#define HEAL_PATTERN_DOUBLE_FREE_BEFORE "nimcp_free(${ptr})"
#define HEAL_PATTERN_DOUBLE_FREE_AFTER  \
    "if (${ptr} != NULL) { nimcp_free(${ptr}); ${ptr} = NULL; }"

/**
 * @brief Integer overflow check template
 *
 * BEFORE: a + b
 * AFTER:  if (a > TYPE_MAX - b) { return NIMCP_ERROR_OVERFLOW; }
 *         a + b
 */
#define HEAL_PATTERN_OVERFLOW_BEFORE "${a} + ${b}"
#define HEAL_PATTERN_OVERFLOW_AFTER  \
    "if (${a} > ${type_max} - ${b}) { return NIMCP_ERROR_OVERFLOW; }\n${a} + ${b}"

/**
 * @brief Resource leak fix template
 *
 * BEFORE: handle = open_resource(...);
 * AFTER:  handle = open_resource(...);
 *         if (handle != NULL) { ... use resource ... close_resource(handle); handle = NULL; }
 */
#define HEAL_PATTERN_RESOURCE_LEAK_BEFORE "${handle} = ${open_func}(${args})"
#define HEAL_PATTERN_RESOURCE_LEAK_AFTER  \
    "${handle} = ${open_func}(${args});\n" \
    "if (${handle} != ${invalid_value}) {\n" \
    "    /* resource acquired - ensure cleanup on all paths */\n" \
    "}\n" \
    "/* cleanup: ${close_func}(${handle}); ${handle} = ${invalid_value}; */"

/**
 * @brief Format string vulnerability fix template
 *
 * BEFORE: printf(user_input);
 * AFTER:  printf("%s", user_input);
 */
#define HEAL_PATTERN_FORMAT_STRING_BEFORE "${printf_func}(${user_str})"
#define HEAL_PATTERN_FORMAT_STRING_AFTER  "${printf_func}(\"%s\", ${user_str})"

/**
 * @brief Buffer underflow (negative index) fix template
 *
 * BEFORE: array[idx]
 * AFTER:  if (idx < 0) { return NIMCP_ERROR_OUT_OF_RANGE; }
 *         array[idx]
 */
#define HEAL_PATTERN_UNDERFLOW_BEFORE "${array}[${idx}]"
#define HEAL_PATTERN_UNDERFLOW_AFTER  \
    "if (${idx} < 0) { return NIMCP_ERROR_OUT_OF_RANGE; }\n${array}[${idx}]"

/**
 * @brief Uninitialized variable fix template
 *
 * BEFORE: type var; ... use(var);
 * AFTER:  type var = default_value; ... use(var);
 */
#define HEAL_PATTERN_UNINIT_BEFORE "${type} ${var};"
#define HEAL_PATTERN_UNINIT_AFTER  "${type} ${var} = ${default_value};"

/**
 * @brief Stack overflow (deep recursion) fix template
 *
 * BEFORE: void func() { ... func(); }
 * AFTER:  void func() { static int depth = 0;
 *                       if (++depth > MAX_DEPTH) { depth--; return; }
 *                       ... func();
 *                       depth--; }
 */
#define HEAL_PATTERN_STACK_OVERFLOW_BEFORE "void ${func}(${params})"
#define HEAL_PATTERN_STACK_OVERFLOW_AFTER  \
    "void ${func}(${params}) {\n" \
    "    static __thread int _recursion_depth = 0;\n" \
    "    if (++_recursion_depth > ${max_depth}) {\n" \
    "        _recursion_depth--;\n" \
    "        return ${default_return};\n" \
    "    }"

/**
 * @brief Race condition guard template
 *
 * BEFORE: shared_var = value;
 * AFTER:  mutex_lock(&mutex); shared_var = value; mutex_unlock(&mutex);
 */
#define HEAL_PATTERN_RACE_BEFORE "${shared_var} = ${value}"
#define HEAL_PATTERN_RACE_AFTER  \
    "nimcp_mutex_lock(${mutex});\n${shared_var} = ${value};\nnimcp_mutex_unlock(${mutex});"

/**
 * @brief Deadlock prevention (trylock with timeout) template
 *
 * BEFORE: mutex_lock(&mutex1); mutex_lock(&mutex2);
 * AFTER:  if (mutex_trylock_timed(&mutex1, timeout) == 0) {
 *             if (mutex_trylock_timed(&mutex2, timeout) != 0) {
 *                 mutex_unlock(&mutex1); return TIMEOUT;
 *             }
 *         }
 */
#define HEAL_PATTERN_DEADLOCK_BEFORE "nimcp_mutex_lock(${mutex1})"
#define HEAL_PATTERN_DEADLOCK_AFTER  \
    "if (nimcp_mutex_trylock_timed(${mutex1}, ${timeout}) != 0) {\n" \
    "    return NIMCP_ERROR_TIMEOUT;\n" \
    "}"

/* ============================================================================
 * Detection Heuristics
 * ============================================================================ */

/**
 * @brief Pattern detection heuristic
 *
 * WHAT: Defines code patterns to match for each fix type
 * WHY:  Enable pattern detection without full parsing
 */
typedef struct {
    const char* keywords[8];       /**< Keywords indicating this pattern */
    size_t keyword_count;          /**< Number of keywords */
    const char* code_patterns[4];  /**< Code pattern strings to match */
    size_t pattern_count;          /**< Number of code patterns */
    float base_confidence;         /**< Base confidence when matched */
} pattern_heuristic_t;

/**
 * @brief Pattern statistics for tracking effectiveness
 *
 * WHAT: Aggregate statistics across all patterns
 * WHY:  Monitor self-healing effectiveness over time
 */
typedef struct {
    uint64_t total_matches;              /**< Total pattern matches */
    uint64_t total_applications;         /**< Total fix applications */
    uint64_t successful_fixes;           /**< Verified successful fixes */
    uint64_t failed_fixes;               /**< Fixes that didn't work */
    uint64_t pattern_counts[FIX_PATTERN_COUNT]; /**< Per-pattern match count */
    float pattern_success_rates[FIX_PATTERN_COUNT]; /**< Per-pattern success rate */
    uint64_t last_update_time;           /**< Last statistics update */
} pattern_statistics_t;

/* ============================================================================
 * Pattern Library API
 * ============================================================================ */

/**
 * @brief Initialize pattern library
 *
 * WHAT: Create and initialize pattern library with built-in patterns
 * WHY:  Prepare pattern matching infrastructure
 * HOW:  Allocate storage, register built-in patterns
 *
 * @return Initialized pattern library or NULL on failure
 */
pattern_library_t* heal_pattern_library_create(void);

/**
 * @brief Destroy pattern library
 *
 * WHAT: Clean up pattern library resources
 * WHY:  Proper resource deallocation
 * HOW:  Free all patterns and library structure
 *
 * @param library Pattern library to destroy (NULL-safe)
 */
void heal_pattern_library_destroy(pattern_library_t* library);

/**
 * @brief Get pattern by type
 *
 * WHAT: Retrieve pattern definition for given type
 * WHY:  Look up pattern for code generation
 * HOW:  Search built-in then custom patterns
 *
 * @param library Pattern library
 * @param type Pattern type to find
 * @return Pattern definition or NULL if not found
 */
const fix_pattern_t* heal_pattern_get_by_type(
    pattern_library_t* library,
    fix_pattern_type_t type
);

/**
 * @brief Get pattern by ID
 *
 * WHAT: Retrieve pattern by unique ID
 * WHY:  Direct pattern lookup
 * HOW:  Search all patterns for matching ID
 *
 * @param library Pattern library
 * @param id Pattern ID
 * @return Pattern definition or NULL if not found
 */
const fix_pattern_t* heal_pattern_get_by_id(
    pattern_library_t* library,
    uint32_t id
);

/**
 * @brief Register custom pattern
 *
 * WHAT: Add user-defined pattern to library
 * WHY:  Extend pattern library with domain-specific fixes
 * HOW:  Copy pattern to custom array, assign ID
 *
 * @param library Pattern library
 * @param pattern Pattern definition to register
 * @param id_out Output: assigned pattern ID
 * @return 0 on success, negative on error
 */
int heal_pattern_register(
    pattern_library_t* library,
    const fix_pattern_t* pattern,
    uint32_t* id_out
);

/**
 * @brief Unregister custom pattern
 *
 * WHAT: Remove custom pattern from library
 * WHY:  Clean up unused patterns
 * HOW:  Find and remove from custom array
 *
 * @param library Pattern library
 * @param id Pattern ID to remove
 * @return 0 on success, -1 if not found
 */
int heal_pattern_unregister(
    pattern_library_t* library,
    uint32_t id
);

/**
 * @brief Match code against patterns
 *
 * WHAT: Find patterns that match given code snippet
 * WHY:  Identify applicable fixes for code
 * HOW:  Try each pattern's match strategy
 *
 * @param library Pattern library
 * @param code Source code to analyze
 * @param code_len Code length
 * @param result Output: match result
 * @return 0 on match found, -1 if no match
 */
int heal_pattern_match(
    pattern_library_t* library,
    const char* code,
    size_t code_len,
    pattern_match_result_t* result
);

/**
 * @brief Apply pattern to generate fix
 *
 * WHAT: Generate fixed code from pattern and match
 * WHY:  Produce concrete fix from pattern template
 * HOW:  Substitute extracted variables into template
 *
 * @param pattern Pattern to apply
 * @param match Match result with extracted variables
 * @param original_code Original source code
 * @param fixed_code Output buffer for fixed code
 * @param fixed_code_size Buffer size
 * @return 0 on success, negative on error
 */
int heal_pattern_apply(
    const fix_pattern_t* pattern,
    const pattern_match_result_t* match,
    const char* original_code,
    char* fixed_code,
    size_t fixed_code_size
);

/**
 * @brief Update pattern statistics
 *
 * WHAT: Record pattern application outcome
 * WHY:  Track pattern effectiveness over time
 * HOW:  Update success/fail counts, recalculate confidence
 *
 * @param library Pattern library
 * @param id Pattern ID
 * @param success Whether application was successful
 * @return 0 on success, -1 on error
 */
int heal_pattern_update_stats(
    pattern_library_t* library,
    uint32_t id,
    bool success
);

/**
 * @brief Get all patterns of a specific type
 *
 * WHAT: Retrieve all patterns matching a type
 * WHY:  Allow iteration over pattern variants
 * HOW:  Filter patterns by type
 *
 * @param library Pattern library
 * @param type Pattern type to filter
 * @param patterns_out Output array of pattern pointers
 * @param max_patterns Maximum patterns to return
 * @return Number of patterns found
 */
size_t heal_pattern_get_all_by_type(
    pattern_library_t* library,
    fix_pattern_type_t type,
    const fix_pattern_t** patterns_out,
    size_t max_patterns
);

/**
 * @brief Get best pattern for crash type
 *
 * WHAT: Find highest-confidence pattern for crash type
 * WHY:  Use most reliable fix for given crash
 * HOW:  Search patterns by type, sort by confidence
 *
 * @param library Pattern library
 * @param type Pattern type needed
 * @return Best pattern or NULL if none available
 */
const fix_pattern_t* heal_pattern_get_best(
    pattern_library_t* library,
    fix_pattern_type_t type
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get pattern type name
 *
 * @param type Pattern type
 * @return String name of pattern type
 */
const char* heal_pattern_type_to_string(fix_pattern_type_t type);

/**
 * @brief Parse pattern type from string
 *
 * @param name Pattern type name
 * @return Pattern type enum value
 */
fix_pattern_type_t heal_pattern_type_from_string(const char* name);

/**
 * @brief Initialize built-in pattern
 *
 * WHAT: Populate fix_pattern_t with built-in template
 * WHY:  Helper to create standard patterns
 * HOW:  Copy templates and set default values
 *
 * @param pattern Output pattern structure
 * @param type Pattern type to initialize
 * @return 0 on success, -1 if type not supported
 */
int heal_pattern_init_builtin(
    fix_pattern_t* pattern,
    fix_pattern_type_t type
);

/* ============================================================================
 * Pattern Statistics API
 * ============================================================================ */

/**
 * @brief Get aggregate pattern statistics
 *
 * WHAT: Retrieve overall pattern matching statistics
 * WHY:  Monitor self-healing effectiveness
 * HOW:  Aggregate data from all patterns in library
 *
 * @param library Pattern library
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int heal_pattern_get_statistics(
    pattern_library_t* library,
    pattern_statistics_t* stats
);

/**
 * @brief Reset pattern statistics
 *
 * WHAT: Clear all accumulated pattern statistics
 * WHY:  Allow fresh measurement period
 * HOW:  Zero all counters in library
 *
 * @param library Pattern library
 * @return 0 on success, -1 on error
 */
int heal_pattern_reset_statistics(pattern_library_t* library);

/**
 * @brief Get detection heuristic for pattern type
 *
 * WHAT: Retrieve heuristics for detecting specific pattern type
 * WHY:  Enable pattern detection in code analysis
 * HOW:  Return predefined heuristics for given type
 *
 * @param type Pattern type
 * @return Heuristic structure or NULL if not available
 */
const pattern_heuristic_t* heal_pattern_get_heuristic(fix_pattern_type_t type);

/**
 * @brief Calculate confidence score for pattern match
 *
 * WHAT: Compute confidence score for a code-pattern match
 * WHY:  Weight pattern matches by quality
 * HOW:  Combine heuristic score, historical success rate, context
 *
 * @param library Pattern library
 * @param type Matched pattern type
 * @param match_score Raw match score from detection
 * @return Adjusted confidence score (0.0-1.0)
 */
float heal_pattern_calculate_confidence(
    pattern_library_t* library,
    fix_pattern_type_t type,
    float match_score
);

/**
 * @brief Detect pattern type from code snippet
 *
 * WHAT: Analyze code to determine likely bug pattern
 * WHY:  Enable automatic pattern selection
 * HOW:  Apply heuristics, return best matching pattern type
 *
 * @param code Source code to analyze
 * @param code_len Length of code
 * @param confidence_out Output: confidence score
 * @return Detected pattern type, FIX_PATTERN_UNKNOWN if none
 */
fix_pattern_type_t heal_pattern_detect_type(
    const char* code,
    size_t code_len,
    float* confidence_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEAL_PATTERNS_H */
