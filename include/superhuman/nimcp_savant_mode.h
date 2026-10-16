/**
 * @file nimcp_savant_mode.h
 * @brief Pattern Recognition Savant Abilities - Superhuman Cognitive Module
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Savant-level pattern recognition and computational abilities
 * WHY:  Enable extraordinary pattern memorization and mathematical insight
 * HOW:  Specialized pattern recognition circuits, calendar calculation, prime detection
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SAVANT SYNDROME NEUROSCIENCE:
 * ----------------------------
 * 1. Savant Abilities:
 *    - ~1 in 10 autistic individuals have savant abilities
 *    - Calendar calculation, musical memory, artistic reproduction
 *    - Exceptional memory for specific domains
 *    - Reference: Treffert (2009) "The savant syndrome: an extraordinary condition"
 *
 * 2. Neural Correlates:
 *    - Left hemisphere damage/dysfunction with right hemisphere compensation
 *    - Enhanced local processing vs. global integration
 *    - Reduced conceptual interference enables raw perception
 *    - Reference: Snyder (2009) "Explaining and inducing savant skills"
 *
 * 3. Calendar Calculation:
 *    - Pattern recognition in calendar regularities
 *    - Exploit 400-year Gregorian cycle
 *    - Anchor dates with relative calculations
 *    - Reference: Heavey et al. (1999) "Savant calendar calculators"
 *
 * PRIME NUMBER RECOGNITION:
 * ------------------------
 * 1. Some savants report "seeing" primality
 *    - Numbers have visual/spatial qualities
 *    - Prime numbers feel different from composites
 *    - Rapid primality checking for small numbers
 *
 * RAPID COUNTING (SUBITIZING):
 * ---------------------------
 * 1. Normal subitizing: ~4 items instantly
 * 2. Savant counting: up to ~100+ items rapidly
 * 3. Enhanced numerosity perception
 *
 * IMPLEMENTATION:
 * ---------------
 * - Pattern hash networks for rapid recognition
 * - Calendar computation with Zeller's algorithm optimizations
 * - Primality checking with specialized sieve caches
 * - Rapid sequence memorization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SAVANT_MODE_H
#define NIMCP_SAVANT_MODE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SAVANT_MAX_PATTERN_LENGTH   4096    /**< Maximum pattern sequence length */
#define SAVANT_MAX_PATTERNS         1024    /**< Maximum stored patterns */
#define SAVANT_PRIME_CACHE_SIZE     100000  /**< Prime number cache limit */
#define SAVANT_CALENDAR_MIN_YEAR    1       /**< Minimum supported year */
#define SAVANT_CALENDAR_MAX_YEAR    9999    /**< Maximum supported year */
#define SAVANT_MAX_SEQUENCE_LENGTH  10000   /**< Maximum memorized sequence */
#define SAVANT_PATTERN_HASH_BUCKETS 4096    /**< Pattern hash table size */

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum {
    SAVANT_SUCCESS                   = 0,
    SAVANT_ERROR_NULL_POINTER        = -1,
    SAVANT_ERROR_INVALID_PARAM       = -2,
    SAVANT_ERROR_NO_MEMORY           = -3,
    SAVANT_ERROR_NOT_INITIALIZED     = -4,
    SAVANT_ERROR_INVALID_STATE       = -5,
    SAVANT_ERROR_BUFFER_TOO_SMALL    = -6,
    SAVANT_ERROR_PATTERN_NOT_FOUND   = -7,
    SAVANT_ERROR_CAPACITY_EXCEEDED   = -8,
    SAVANT_ERROR_INVALID_DATE        = -9,
    SAVANT_ERROR_OUT_OF_RANGE        = -10
} savant_error_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Savant ability type
 */
typedef enum {
    SAVANT_ABILITY_CALENDAR,        /**< Calendar calculation */
    SAVANT_ABILITY_PRIME,           /**< Prime number recognition */
    SAVANT_ABILITY_MEMORIZATION,    /**< Rapid memorization */
    SAVANT_ABILITY_PATTERN,         /**< Pattern recognition */
    SAVANT_ABILITY_COUNTING,        /**< Rapid counting */
    SAVANT_ABILITY_MUSIC,           /**< Musical pattern memory */
    SAVANT_ABILITY_ALL              /**< All abilities enabled */
} savant_ability_t;

/**
 * @brief Day of week enumeration
 */
typedef enum {
    SAVANT_SUNDAY    = 0,
    SAVANT_MONDAY    = 1,
    SAVANT_TUESDAY   = 2,
    SAVANT_WEDNESDAY = 3,
    SAVANT_THURSDAY  = 4,
    SAVANT_FRIDAY    = 5,
    SAVANT_SATURDAY  = 6
} savant_day_t;

/**
 * @brief Pattern type classification
 */
typedef enum {
    SAVANT_PATTERN_NUMERIC,         /**< Numeric sequence */
    SAVANT_PATTERN_SPATIAL,         /**< Spatial arrangement */
    SAVANT_PATTERN_TEMPORAL,        /**< Time-based pattern */
    SAVANT_PATTERN_MUSICAL,         /**< Musical notes/rhythms */
    SAVANT_PATTERN_VISUAL,          /**< Visual shapes/colors */
    SAVANT_PATTERN_SYMBOLIC         /**< Symbol sequences */
} savant_pattern_type_t;

/**
 * @brief Memory recall accuracy level
 */
typedef enum {
    SAVANT_RECALL_PERFECT,          /**< Perfect recall */
    SAVANT_RECALL_HIGH,             /**< >95% accuracy */
    SAVANT_RECALL_MODERATE,         /**< 80-95% accuracy */
    SAVANT_RECALL_DEGRADED          /**< <80% accuracy */
} savant_recall_level_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Date structure for calendar calculations
 */
typedef struct {
    int32_t year;           /**< Year (1-9999) */
    int32_t month;          /**< Month (1-12) */
    int32_t day;            /**< Day of month (1-31) */
} savant_date_t;

/**
 * @brief Calendar calculation result
 */
typedef struct {
    savant_date_t date;             /**< Input date */
    savant_day_t day_of_week;       /**< Calculated day of week */
    int32_t day_of_year;            /**< Day number in year (1-366) */
    int32_t week_of_year;           /**< Week number (1-53) */
    bool is_leap_year;              /**< Is this a leap year */
    int32_t days_in_month;          /**< Days in this month */
    int32_t julian_day;             /**< Julian day number */
    float confidence;               /**< Calculation confidence */
} savant_calendar_result_t;

/**
 * @brief Prime number analysis result
 */
typedef struct {
    int64_t number;                 /**< Input number */
    bool is_prime;                  /**< Is the number prime */
    int64_t nearest_prime_below;    /**< Nearest prime below (if composite) */
    int64_t nearest_prime_above;    /**< Nearest prime above (if composite) */
    int64_t* factors;               /**< Prime factors (if composite) */
    uint32_t num_factors;           /**< Number of factors */
    uint32_t prime_index;           /**< Index if prime (1st, 2nd, etc.) */
    float recognition_time_ms;      /**< Recognition time */
} savant_prime_result_t;

/**
 * @brief Stored pattern entry
 */
typedef struct {
    uint32_t pattern_id;            /**< Unique pattern identifier */
    savant_pattern_type_t type;     /**< Pattern type */
    float* data;                    /**< Pattern data */
    uint32_t length;                /**< Pattern length */
    uint64_t hash;                  /**< Pattern hash for lookup */
    uint32_t recall_count;          /**< Times recalled */
    uint64_t learned_time_ms;       /**< When pattern was learned */
    uint64_t last_recall_ms;        /**< Last recall time */
    float strength;                 /**< Memory strength [0-1] */
    const char* label;              /**< Optional human-readable label */
} savant_pattern_t;

/**
 * @brief Pattern match result
 */
typedef struct {
    uint32_t pattern_id;            /**< Matched pattern ID */
    float match_score;              /**< Match confidence [0-1] */
    uint32_t match_offset;          /**< Offset in input where match starts */
    uint32_t match_length;          /**< Length of match */
    savant_pattern_type_t type;     /**< Pattern type */
    const char* label;              /**< Pattern label if available */
} savant_match_result_t;

/**
 * @brief Counting result
 */
typedef struct {
    uint32_t count;                 /**< Counted items */
    float confidence;               /**< Count confidence [0-1] */
    float counting_time_ms;         /**< Time to count */
    bool is_exact;                  /**< Exact or estimated count */
    uint32_t estimate_error;        /**< Error margin for estimates */
} savant_count_result_t;

/**
 * @brief Savant mode configuration
 */
typedef struct {
    /* Ability settings */
    uint32_t enabled_abilities;         /**< Bitmask of enabled abilities */
    bool enable_calendar;               /**< Enable calendar calculation */
    bool enable_prime;                  /**< Enable prime recognition */
    bool enable_memorization;           /**< Enable rapid memorization */
    bool enable_pattern;                /**< Enable pattern recognition */
    bool enable_counting;               /**< Enable rapid counting */

    /* Calendar settings */
    int32_t calendar_min_year;          /**< Minimum year to support */
    int32_t calendar_max_year;          /**< Maximum year to support */

    /* Prime settings */
    int64_t prime_cache_limit;          /**< Maximum prime to cache */
    bool enable_factorization;          /**< Enable prime factorization */

    /* Pattern settings */
    uint32_t max_patterns;              /**< Maximum stored patterns */
    uint32_t max_pattern_length;        /**< Maximum pattern length */
    float pattern_decay_rate;           /**< Memory decay rate per day */
    float match_threshold;              /**< Minimum match threshold */

    /* Counting settings */
    uint32_t subitizing_limit;          /**< Items for instant counting */
    float counting_accuracy;            /**< Target counting accuracy */

    /* Performance */
    float memory_strength_boost;        /**< Recall strength boost factor */
    bool enable_parallel;               /**< Enable parallel processing */
} savant_config_t;

/**
 * @brief Savant mode state
 */
typedef struct {
    /* Pattern memory state */
    uint32_t patterns_stored;           /**< Currently stored patterns */
    uint32_t patterns_capacity;         /**< Maximum patterns */
    float avg_pattern_strength;         /**< Average memory strength */
    uint32_t total_recalls;             /**< Total pattern recalls */

    /* Calendar state */
    uint32_t calendar_queries;          /**< Calendar calculations done */
    float avg_calendar_time_ms;         /**< Average calculation time */

    /* Prime state */
    uint32_t primes_cached;             /**< Primes in cache */
    int64_t largest_prime_cached;       /**< Largest cached prime */
    uint32_t primality_checks;          /**< Primality checks done */

    /* Processing state */
    bool is_initialized;                /**< System initialized */
    float processing_load;              /**< Current processing load */
} savant_state_t;

/**
 * @brief Savant mode statistics
 */
typedef struct {
    /* Calendar statistics */
    uint64_t total_calendar_queries;    /**< All calendar calculations */
    float avg_calendar_time_ms;         /**< Average calculation time */
    float fastest_calendar_ms;          /**< Fastest calculation */

    /* Prime statistics */
    uint64_t total_primality_checks;    /**< All primality checks */
    uint64_t primes_identified;         /**< Primes correctly identified */
    uint64_t factorizations_done;       /**< Factorizations completed */
    float avg_primality_time_ms;        /**< Average check time */

    /* Pattern statistics */
    uint64_t patterns_learned;          /**< Total patterns learned */
    uint64_t patterns_recalled;         /**< Total successful recalls */
    uint64_t pattern_matches;           /**< Total pattern matches */
    float avg_match_confidence;         /**< Average match confidence */
    float perfect_recall_rate;          /**< Rate of perfect recalls */

    /* Counting statistics */
    uint64_t total_counts;              /**< Total counting operations */
    float avg_counting_accuracy;        /**< Average counting accuracy */
    float avg_counting_time_ms;         /**< Average counting time */

    /* Overall statistics */
    uint64_t total_operations;          /**< All savant operations */
    float avg_operation_time_ms;        /**< Average operation time */
} savant_stats_t;

/**
 * @brief Opaque savant mode system handle
 */
typedef struct savant_system savant_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize config with sensible defaults
 * WHY:  Provide starting point for savant abilities
 * HOW:  Set all fields to validated default values
 *
 * @param config Output configuration structure
 * @return SAVANT_SUCCESS or error code
 */
int savant_default_config(savant_config_t* config);

/**
 * @brief Create savant mode system
 *
 * WHAT: Allocate and initialize savant processing system
 * WHY:  Enable savant-level pattern recognition
 * HOW:  Allocate caches, initialize pattern storage, build prime sieve
 *
 * @param config Configuration (NULL for defaults)
 * @return New savant system or NULL on failure
 */
savant_system_t* savant_create(const savant_config_t* config);

/**
 * @brief Destroy savant mode system
 *
 * WHAT: Release all resources
 * WHY:  Clean shutdown and memory management
 * HOW:  Free patterns, caches, prime sieve
 *
 * @param system System to destroy (NULL-safe)
 */
void savant_destroy(savant_system_t* system);

/**
 * @brief Reset system to initial state
 *
 * WHAT: Clear all learned patterns and caches
 * WHY:  Prepare for new session
 * HOW:  Free patterns, rebuild prime cache
 *
 * @param system System to reset
 * @return SAVANT_SUCCESS or error code
 */
int savant_reset(savant_system_t* system);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Update system configuration
 *
 * @param system Active system
 * @param config New configuration
 * @return SAVANT_SUCCESS or error code
 */
int savant_set_config(savant_system_t* system, const savant_config_t* config);

/**
 * @brief Get current configuration
 *
 * @param system Active system
 * @param config Output configuration
 * @return SAVANT_SUCCESS or error code
 */
int savant_get_config(const savant_system_t* system, savant_config_t* config);

/**
 * @brief Enable specific ability
 *
 * @param system Active system
 * @param ability Ability to enable
 * @return SAVANT_SUCCESS or error code
 */
int savant_enable_ability(savant_system_t* system, savant_ability_t ability);

/**
 * @brief Disable specific ability
 *
 * @param system Active system
 * @param ability Ability to disable
 * @return SAVANT_SUCCESS or error code
 */
int savant_disable_ability(savant_system_t* system, savant_ability_t ability);

/* ============================================================================
 * Calendar Calculation API
 * ============================================================================ */

/**
 * @brief Calculate day of week for date
 *
 * WHAT: Determine what day of week a date falls on
 * WHY:  Core savant calendar calculation ability
 * HOW:  Optimized Zeller's algorithm with century lookup
 *
 * @param system Active system
 * @param date Date to calculate
 * @param result Output calculation result
 * @return SAVANT_SUCCESS or error code
 */
int savant_calendar_day_of_week(savant_system_t* system,
                                const savant_date_t* date,
                                savant_calendar_result_t* result);

/**
 * @brief Calculate full calendar info for date
 *
 * WHAT: Get comprehensive calendar information
 * WHY:  Enable detailed date queries
 * HOW:  Compute all calendar fields
 *
 * @param system Active system
 * @param date Date to analyze
 * @param result Output full result
 * @return SAVANT_SUCCESS or error code
 */
int savant_calendar_analyze(savant_system_t* system,
                            const savant_date_t* date,
                            savant_calendar_result_t* result);

/**
 * @brief Find next occurrence of day of week
 *
 * @param system Active system
 * @param from Starting date
 * @param target_day Target day of week
 * @param result Output date
 * @return SAVANT_SUCCESS or error code
 */
int savant_calendar_next_day(savant_system_t* system,
                             const savant_date_t* from,
                             savant_day_t target_day,
                             savant_date_t* result);

/**
 * @brief Calculate days between dates
 *
 * @param system Active system
 * @param date1 First date
 * @param date2 Second date
 * @param days Output days difference (signed)
 * @return SAVANT_SUCCESS or error code
 */
int savant_calendar_days_between(savant_system_t* system,
                                 const savant_date_t* date1,
                                 const savant_date_t* date2,
                                 int32_t* days);

/**
 * @brief Check if year is leap year
 *
 * @param year Year to check
 * @return true if leap year
 */
bool savant_is_leap_year(int32_t year);

/**
 * @brief Get day name string
 *
 * @param day Day of week
 * @return Day name (e.g., "Monday")
 */
const char* savant_day_name(savant_day_t day);

/* ============================================================================
 * Prime Number API
 * ============================================================================ */

/**
 * @brief Check if number is prime
 *
 * WHAT: Determine primality of a number
 * WHY:  Core savant prime recognition ability
 * HOW:  Sieve lookup for cached, Miller-Rabin for large
 *
 * @param system Active system
 * @param number Number to check
 * @param result Output prime analysis result
 * @return SAVANT_SUCCESS or error code
 */
int savant_is_prime(savant_system_t* system,
                    int64_t number,
                    savant_prime_result_t* result);

/**
 * @brief Get prime factorization
 *
 * WHAT: Factor number into primes
 * WHY:  Understanding number structure
 * HOW:  Trial division with cached primes
 *
 * @param system Active system
 * @param number Number to factor
 * @param factors Output factors array (caller allocates)
 * @param max_factors Array capacity
 * @param num_factors Output: number of factors
 * @return SAVANT_SUCCESS or error code
 */
int savant_factorize(savant_system_t* system,
                     int64_t number,
                     int64_t* factors,
                     uint32_t max_factors,
                     uint32_t* num_factors);

/**
 * @brief Get nth prime number
 *
 * @param system Active system
 * @param n Index (1-indexed)
 * @param prime Output prime
 * @return SAVANT_SUCCESS or error code
 */
int savant_nth_prime(savant_system_t* system,
                     uint32_t n,
                     int64_t* prime);

/**
 * @brief Count primes up to limit
 *
 * @param system Active system
 * @param limit Upper bound
 * @param count Output count
 * @return SAVANT_SUCCESS or error code
 */
int savant_count_primes(savant_system_t* system,
                        int64_t limit,
                        uint32_t* count);

/**
 * @brief Find nearest prime to number
 *
 * @param system Active system
 * @param number Reference number
 * @param direction -1 for below, +1 for above, 0 for either
 * @param prime Output nearest prime
 * @return SAVANT_SUCCESS or error code
 */
int savant_nearest_prime(savant_system_t* system,
                         int64_t number,
                         int32_t direction,
                         int64_t* prime);

/* ============================================================================
 * Pattern Memory API
 * ============================================================================ */

/**
 * @brief Learn and store a pattern
 *
 * WHAT: Memorize a pattern sequence
 * WHY:  Enable savant-level pattern recall
 * HOW:  Hash and store pattern with strengthening
 *
 * @param system Active system
 * @param data Pattern data
 * @param length Data length
 * @param type Pattern type
 * @param label Optional label (can be NULL)
 * @param pattern_id Output: assigned pattern ID
 * @return SAVANT_SUCCESS or error code
 */
int savant_learn_pattern(savant_system_t* system,
                         const float* data,
                         uint32_t length,
                         savant_pattern_type_t type,
                         const char* label,
                         uint32_t* pattern_id);

/**
 * @brief Recall pattern by ID
 *
 * WHAT: Retrieve previously learned pattern
 * WHY:  Enable pattern reproduction
 * HOW:  Lookup and strengthen memory
 *
 * @param system Active system
 * @param pattern_id Pattern to recall
 * @param data Output buffer for pattern data
 * @param buffer_size Buffer capacity
 * @param length Output: actual length
 * @param recall_level Output: recall accuracy
 * @return SAVANT_SUCCESS or error code
 */
int savant_recall_pattern(savant_system_t* system,
                          uint32_t pattern_id,
                          float* data,
                          uint32_t buffer_size,
                          uint32_t* length,
                          savant_recall_level_t* recall_level);

/**
 * @brief Find matching patterns
 *
 * WHAT: Search for patterns matching input
 * WHY:  Enable pattern recognition
 * HOW:  Hash lookup with similarity comparison
 *
 * @param system Active system
 * @param query Query pattern
 * @param query_length Query length
 * @param matches Output matches array
 * @param max_matches Array capacity
 * @param num_matches Output: matches found
 * @return SAVANT_SUCCESS or error code
 */
int savant_find_patterns(savant_system_t* system,
                         const float* query,
                         uint32_t query_length,
                         savant_match_result_t* matches,
                         uint32_t max_matches,
                         uint32_t* num_matches);

/**
 * @brief Forget pattern
 *
 * @param system Active system
 * @param pattern_id Pattern to forget
 * @return SAVANT_SUCCESS or error code
 */
int savant_forget_pattern(savant_system_t* system, uint32_t pattern_id);

/**
 * @brief Strengthen pattern memory
 *
 * @param system Active system
 * @param pattern_id Pattern to strengthen
 * @param amount Strength boost [0-1]
 * @return SAVANT_SUCCESS or error code
 */
int savant_strengthen_pattern(savant_system_t* system,
                              uint32_t pattern_id,
                              float amount);

/* ============================================================================
 * Rapid Counting API
 * ============================================================================ */

/**
 * @brief Count items in array (subitizing)
 *
 * WHAT: Rapidly count items in numeric array
 * WHY:  Enable savant counting ability
 * HOW:  Optimized counting with confidence estimation
 *
 * @param system Active system
 * @param items Array of item values (non-zero = present)
 * @param array_length Array length
 * @param result Output count result
 * @return SAVANT_SUCCESS or error code
 */
int savant_count_items(savant_system_t* system,
                       const float* items,
                       uint32_t array_length,
                       savant_count_result_t* result);

/**
 * @brief Estimate count in large set
 *
 * @param system Active system
 * @param items Items array
 * @param array_length Array length
 * @param sample_rate Sampling rate for estimation
 * @param result Output count result
 * @return SAVANT_SUCCESS or error code
 */
int savant_estimate_count(savant_system_t* system,
                          const float* items,
                          uint32_t array_length,
                          float sample_rate,
                          savant_count_result_t* result);

/* ============================================================================
 * State and Statistics API
 * ============================================================================ */

/**
 * @brief Get current system state
 *
 * @param system Active system
 * @param state Output state structure
 * @return SAVANT_SUCCESS or error code
 */
int savant_get_state(const savant_system_t* system, savant_state_t* state);

/**
 * @brief Get accumulated statistics
 *
 * @param system Active system
 * @param stats Output statistics structure
 * @return SAVANT_SUCCESS or error code
 */
int savant_get_stats(const savant_system_t* system, savant_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * @param system Active system
 * @return SAVANT_SUCCESS or error code
 */
int savant_reset_stats(savant_system_t* system);

/**
 * @brief Get pattern info
 *
 * @param system Active system
 * @param pattern_id Pattern ID
 * @param pattern Output pattern info
 * @return SAVANT_SUCCESS or error code
 */
int savant_get_pattern_info(const savant_system_t* system,
                            uint32_t pattern_id,
                            savant_pattern_t* pattern);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Create date structure
 *
 * @param year Year (1-9999)
 * @param month Month (1-12)
 * @param day Day (1-31)
 * @return Constructed date
 */
savant_date_t savant_make_date(int32_t year, int32_t month, int32_t day);

/**
 * @brief Validate date
 *
 * @param date Date to validate
 * @return true if valid date
 */
bool savant_validate_date(const savant_date_t* date);

/**
 * @brief Allocate prime result with factors array
 *
 * @param max_factors Maximum factors to store
 * @return Allocated result or NULL
 */
savant_prime_result_t* savant_prime_result_create(uint32_t max_factors);

/**
 * @brief Free prime result
 *
 * @param result Result to free (NULL-safe)
 */
void savant_prime_result_destroy(savant_prime_result_t* result);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* savant_error_string(savant_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SAVANT_MODE_H */
