/**
 * @file nimcp_savant_mode.c
 * @brief Pattern Recognition Savant Abilities - Implementation
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Savant-level pattern recognition and computational abilities
 * WHY:  Enable extraordinary pattern memorization and mathematical insight
 * HOW:  Specialized pattern recognition circuits, calendar calculation, prime detection
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 * Savant abilities emerge from unusual neural organization, particularly enhanced
 * local processing with reduced global integration. Calendar calculators exploit
 * calendar regularities and anchor dates. Prime recognition involves pattern-based
 * number representation. This implementation uses optimized algorithms inspired
 * by these cognitive strategies.
 *
 * REFERENCES:
 * - Treffert (2009) "The savant syndrome: an extraordinary condition"
 * - Snyder (2009) "Explaining and inducing savant skills"
 * - Heavey et al. (1999) "Savant calendar calculators"
 */

#include "superhuman/nimcp_savant_mode.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(savant_mode)

/* ============================================================================
 * Constants
 * ============================================================================ */

static const int32_t DAYS_IN_MONTH[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const int32_t DAYS_BEFORE_MONTH[] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

static const char* DAY_NAMES[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Pattern hash bucket
 */
typedef struct pattern_bucket {
    savant_pattern_t pattern;       /**< Pattern data */
    float* data_copy;               /**< Owned data copy */
    char* label_copy;               /**< Owned label copy */
    bool active;                    /**< Bucket in use */
    struct pattern_bucket* next;    /**< Hash chain next */
} pattern_bucket_t;

/**
 * @brief Prime sieve cache
 */
typedef struct {
    uint8_t* sieve;                 /**< Bit sieve (1 = composite) */
    int64_t limit;                  /**< Sieve upper limit */
    int64_t* prime_list;            /**< List of primes */
    uint32_t prime_count;           /**< Number of primes in list */
    uint32_t prime_capacity;        /**< List capacity */
} prime_cache_t;

/**
 * @brief Internal savant system
 */
struct savant_system {
    /* Configuration */
    savant_config_t config;

    /* State */
    savant_state_t state;
    savant_stats_t stats;

    /* Pattern storage */
    pattern_bucket_t** pattern_table;   /**< Hash table for patterns */
    uint32_t table_size;                /**< Hash table size */
    uint32_t pattern_count;             /**< Total patterns stored */
    uint32_t next_pattern_id;           /**< Next pattern ID */

    /* Prime cache */
    prime_cache_t prime_cache;          /**< Prime number cache */

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get current time in milliseconds
 * WHY:  Performance timing
 * HOW:  Use CLOCK_MONOTONIC
 */
static float get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (float)ts.tv_sec * 1000.0f + (float)ts.tv_nsec / 1000000.0f;
}

/**
 * WHAT: Compute hash for pattern data
 * WHY:  Fast pattern lookup
 * HOW:  FNV-1a style hash
 */
static uint64_t compute_pattern_hash(const float* data, uint32_t length) {
    uint64_t hash = 14695981039346656037ULL;  /* FNV offset basis */
    const uint8_t* bytes = (const uint8_t*)data;
    size_t byte_len = length * sizeof(float);

    for (size_t i = 0; i < byte_len; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;  /* FNV prime */
    }

    return hash;
}

/**
 * WHAT: Check if year is leap year
 * WHY:  Calendar calculations need leap year info
 * HOW:  Gregorian calendar rules
 */
bool savant_is_leap_year(int32_t year) {
    if (year % 4 != 0) {
        return false;
    }
    if (year % 100 != 0) return true;
    if (year % 400 != 0) {
        return false;
    }
    return true;
}

/**
 * WHAT: Get days in month
 * WHY:  Calendar validation and calculation
 * HOW:  Look up with leap year adjustment
 */
static int32_t days_in_month(int32_t year, int32_t month) {
    if (month < 1 || month > 12) return 0;
    if (month == 2 && savant_is_leap_year(year)) return 29;
    return DAYS_IN_MONTH[month];
}

/**
 * WHAT: Validate date components
 * WHY:  Prevent invalid date calculations
 * HOW:  Check ranges and day validity
 */
bool savant_validate_date(const savant_date_t* date) {
    if (!date) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "savant_validate_date: date is NULL");
        return false;
    }
    if (date->year < SAVANT_CALENDAR_MIN_YEAR || date->year > SAVANT_CALENDAR_MAX_YEAR) {
        return false;
    }
    if (date->month < 1 || date->month > 12) {
        return false;
    }
    if (date->day < 1 || date->day > days_in_month(date->year, date->month)) {
        return false;
    }
    return true;
}

/**
 * WHAT: Convert date to Julian day number
 * WHY:  Uniform date arithmetic
 * HOW:  Algorithm from "Astronomical Algorithms"
 */
static int32_t date_to_julian(const savant_date_t* date) {
    int32_t a = (14 - date->month) / 12;
    int32_t y = date->year + 4800 - a;
    int32_t m = date->month + 12 * a - 3;

    /* Gregorian calendar */
    return date->day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
}

/**
 * WHAT: Convert Julian day to date
 * WHY:  Date arithmetic result
 * HOW:  Inverse of date_to_julian
 */
static void julian_to_date(int32_t jd, savant_date_t* date) {
    int32_t a = jd + 32044;
    int32_t b = (4 * a + 3) / 146097;
    int32_t c = a - (146097 * b) / 4;
    int32_t d = (4 * c + 3) / 1461;
    int32_t e = c - (1461 * d) / 4;
    int32_t m = (5 * e + 2) / 153;

    date->day = e - (153 * m + 2) / 5 + 1;
    date->month = m + 3 - 12 * (m / 10);
    date->year = 100 * b + d - 4800 + m / 10;
}

/**
 * WHAT: Initialize prime sieve
 * WHY:  Fast primality testing for cached range
 * HOW:  Sieve of Eratosthenes
 */
static int init_prime_cache(prime_cache_t* cache, int64_t limit) {
    if (limit < 2) limit = 2;

    /* Allocate sieve */
    size_t sieve_size = (size_t)((limit / 8) + 1);
    cache->sieve = nimcp_calloc(sieve_size, 1);
    if (!cache->sieve) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sieve_size,
                           "init_prime_cache: Failed to allocate sieve");
        return SAVANT_ERROR_NO_MEMORY;
    }

    cache->limit = limit;

    /* Sieve of Eratosthenes */
    /* Mark 0 and 1 as composite */
    cache->sieve[0] |= 0x03;

    for (int64_t i = 2; i * i <= limit; i++) {
        if ((cache->sieve[i / 8] & (1 << (i % 8))) == 0) {
            /* i is prime, mark multiples as composite */
            for (int64_t j = i * i; j <= limit; j += i) {
                cache->sieve[j / 8] |= (1 << (j % 8));
            }
        }
    }

    /* Build prime list */
    uint32_t estimated_count = (uint32_t)(limit / (log((double)limit) - 1.0) * 1.3);
    cache->prime_list = nimcp_malloc(estimated_count * sizeof(int64_t));
    if (!cache->prime_list) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, estimated_count * sizeof(int64_t),
                           "init_prime_cache: Failed to allocate prime list");
        nimcp_free(cache->sieve);
        cache->sieve = NULL;
        return SAVANT_ERROR_NO_MEMORY;
    }
    cache->prime_capacity = estimated_count;
    cache->prime_count = 0;

    for (int64_t i = 2; i <= limit && cache->prime_count < cache->prime_capacity; i++) {
        if ((cache->sieve[i / 8] & (1 << (i % 8))) == 0) {
            cache->prime_list[cache->prime_count++] = i;
        }
    }

    return SAVANT_SUCCESS;
}

/**
 * WHAT: Check primality using cache or Miller-Rabin
 * WHY:  Fast primality testing
 * HOW:  Cache lookup for small, Miller-Rabin for large
 */
static bool is_prime_internal(prime_cache_t* cache, int64_t n) {
    if (n < 2) {
        return false;
    }
    if (n == 2) return true;
    if (n % 2 == 0) {
        return false;
    }

    /* Use sieve for cached range */
    if (n <= cache->limit) {
        return (cache->sieve[n / 8] & (1 << (n % 8))) == 0;
    }

    /* Miller-Rabin for larger numbers */
    /* Deterministic for n < 3,317,044,064,679,887,385,961,981 with these witnesses */
    static const int64_t witnesses[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    int num_witnesses = 12;

    /* Write n-1 as 2^r * d */
    int64_t d = n - 1;
    int32_t r = 0;
    while ((d & 1) == 0) {
        d >>= 1;
        r++;
    }

    /* Test each witness */
    for (int i = 0; i < num_witnesses && witnesses[i] < n; i++) {
        int64_t a = witnesses[i];

        /* Compute a^d mod n */
        int64_t x = 1;
        int64_t base = a % n;
        int64_t exp = d;

        while (exp > 0) {
            if (exp & 1) {
                x = (__int128_t)x * base % n;
            }
            base = (__int128_t)base * base % n;
            exp >>= 1;
        }

        if (x == 1 || x == n - 1) continue;

        bool composite = true;
        for (int32_t j = 0; j < r - 1; j++) {
            x = (__int128_t)x * x % n;
            if (x == n - 1) {
                composite = false;
                break;
            }
        }

        if (composite) {
            return false;
        }
    }

    return true;
}

/**
 * WHAT: Free prime cache resources
 * WHY:  Clean resource release
 * HOW:  Free sieve and prime list
 */
static void free_prime_cache(prime_cache_t* cache) {
    if (cache->sieve) {
        nimcp_free(cache->sieve);
        cache->sieve = NULL;
    }
    if (cache->prime_list) {
        nimcp_free(cache->prime_list);
        cache->prime_list = NULL;
    }
    cache->limit = 0;
    cache->prime_count = 0;
}

/**
 * WHAT: Free pattern bucket
 * WHY:  Clean resource release
 * HOW:  Free data copy and label copy
 */
static void free_pattern_bucket(pattern_bucket_t* bucket) {
    if (!bucket) return;

    if (bucket->data_copy) {
        nimcp_free(bucket->data_copy);
        bucket->data_copy = NULL;
    }
    if (bucket->label_copy) {
        nimcp_free(bucket->label_copy);
        bucket->label_copy = NULL;
    }
    bucket->active = false;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int savant_default_config(savant_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_default_config: config is NULL");
        return SAVANT_ERROR_NULL_POINTER;
    }

    /* Ability settings */
    config->enabled_abilities = 0xFFFFFFFF;  /* All enabled */
    config->enable_calendar = true;
    config->enable_prime = true;
    config->enable_memorization = true;
    config->enable_pattern = true;
    config->enable_counting = true;

    /* Calendar settings */
    config->calendar_min_year = SAVANT_CALENDAR_MIN_YEAR;
    config->calendar_max_year = SAVANT_CALENDAR_MAX_YEAR;

    /* Prime settings */
    config->prime_cache_limit = SAVANT_PRIME_CACHE_SIZE;
    config->enable_factorization = true;

    /* Pattern settings */
    config->max_patterns = SAVANT_MAX_PATTERNS;
    config->max_pattern_length = SAVANT_MAX_PATTERN_LENGTH;
    config->pattern_decay_rate = 0.01f;     /* 1% per day */
    config->match_threshold = 0.8f;

    /* Counting settings */
    config->subitizing_limit = 10;          /* Instant count limit */
    config->counting_accuracy = 0.99f;

    /* Performance */
    config->memory_strength_boost = 0.1f;
    config->enable_parallel = false;

    return SAVANT_SUCCESS;
}

savant_system_t* savant_create(const savant_config_t* config) {
    savant_system_t* sys = nimcp_calloc(1, sizeof(savant_system_t));
    if (!sys) {
        NIMCP_LOGGING_ERROR("Failed to allocate savant system");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(savant_system_t),
                           "savant_create: Failed to allocate system");
        return NULL;
    }

    /* Apply configuration */
    savant_config_t default_cfg;
    if (!config) {
        savant_default_config(&default_cfg);
        config = &default_cfg;
    }
    sys->config = *config;

    /* Initialize pattern hash table */
    sys->table_size = SAVANT_PATTERN_HASH_BUCKETS;
    sys->pattern_table = nimcp_calloc(sys->table_size, sizeof(pattern_bucket_t*));
    if (!sys->pattern_table) {
        NIMCP_LOGGING_ERROR("Failed to allocate pattern table");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           sys->table_size * sizeof(pattern_bucket_t*),
                           "savant_create: Failed to allocate pattern table");
        savant_destroy(sys);
        return NULL;
    }
    sys->next_pattern_id = 1;

    /* Initialize prime cache */
    if (config->enable_prime) {
        int result = init_prime_cache(&sys->prime_cache, config->prime_cache_limit);
        if (result != SAVANT_SUCCESS) {
            NIMCP_LOGGING_ERROR("Failed to initialize prime cache");
            /* Exception already thrown in init_prime_cache */
            savant_destroy(sys);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "savant_create: validation failed");
            return NULL;
        }
    }

    /* Create mutex */
    sys->mutex = nimcp_platform_mutex_create();
    if (!sys->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_THREADING(NIMCP_ERROR_THREAD_CREATE, 0,
                              "savant_create: Failed to create mutex%s", "");
        savant_destroy(sys);
        return NULL;
    }

    /* Initialize state */
    sys->state.patterns_capacity = config->max_patterns;
    sys->state.primes_cached = sys->prime_cache.prime_count;
    sys->state.largest_prime_cached = sys->prime_cache.limit;
    sys->state.is_initialized = true;

    NIMCP_LOGGING_INFO("Savant system created: %u primes cached, %u pattern capacity",
                       sys->prime_cache.prime_count, config->max_patterns);

    return sys;
}

void savant_destroy(savant_system_t* system) {
    if (!system) return;

    /* Free pattern table */
    if (system->pattern_table) {
        for (uint32_t i = 0; i < system->table_size; i++) {
            pattern_bucket_t* bucket = system->pattern_table[i];
            while (bucket) {
                pattern_bucket_t* next = bucket->next;
                free_pattern_bucket(bucket);
                nimcp_free(bucket);
                bucket = next;
            }
        }
        nimcp_free(system->pattern_table);
    }

    /* Free prime cache */
    free_prime_cache(&system->prime_cache);

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    nimcp_free(system);
    NIMCP_LOGGING_INFO("Savant system destroyed");
}

int savant_reset(savant_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_reset: system is NULL");
        return SAVANT_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Clear patterns */
    for (uint32_t i = 0; i < system->table_size; i++) {
        pattern_bucket_t* bucket = system->pattern_table[i];
        while (bucket) {
            pattern_bucket_t* next = bucket->next;
            free_pattern_bucket(bucket);
            nimcp_free(bucket);
            bucket = next;
        }
        system->pattern_table[i] = NULL;
    }
    system->pattern_count = 0;

    /* Reset state */
    system->state.patterns_stored = 0;
    system->state.avg_pattern_strength = 0.0f;
    system->state.total_recalls = 0;
    system->state.calendar_queries = 0;
    system->state.avg_calendar_time_ms = 0.0f;
    system->state.primality_checks = 0;
    system->state.processing_load = 0.0f;

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_DEBUG("Savant system reset");
    return SAVANT_SUCCESS;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int savant_set_config(savant_system_t* system, const savant_config_t* config) {
    if (!system || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_set_config: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);
    system->config = *config;
    nimcp_platform_mutex_unlock(system->mutex);

    return SAVANT_SUCCESS;
}

int savant_get_config(const savant_system_t* system, savant_config_t* config) {
    if (!system || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_get_config: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }

    *config = system->config;
    return SAVANT_SUCCESS;
}

int savant_enable_ability(savant_system_t* system, savant_ability_t ability) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_enable_ability: system is NULL");
        return SAVANT_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    switch (ability) {
        case SAVANT_ABILITY_CALENDAR:
            system->config.enable_calendar = true;
            break;
        case SAVANT_ABILITY_PRIME:
            system->config.enable_prime = true;
            break;
        case SAVANT_ABILITY_MEMORIZATION:
            system->config.enable_memorization = true;
            break;
        case SAVANT_ABILITY_PATTERN:
            system->config.enable_pattern = true;
            break;
        case SAVANT_ABILITY_COUNTING:
            system->config.enable_counting = true;
            break;
        case SAVANT_ABILITY_ALL:
            system->config.enable_calendar = true;
            system->config.enable_prime = true;
            system->config.enable_memorization = true;
            system->config.enable_pattern = true;
            system->config.enable_counting = true;
            break;
        default:
            break;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return SAVANT_SUCCESS;
}

int savant_disable_ability(savant_system_t* system, savant_ability_t ability) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_disable_ability: system is NULL");
        return SAVANT_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    switch (ability) {
        case SAVANT_ABILITY_CALENDAR:
            system->config.enable_calendar = false;
            break;
        case SAVANT_ABILITY_PRIME:
            system->config.enable_prime = false;
            break;
        case SAVANT_ABILITY_MEMORIZATION:
            system->config.enable_memorization = false;
            break;
        case SAVANT_ABILITY_PATTERN:
            system->config.enable_pattern = false;
            break;
        case SAVANT_ABILITY_COUNTING:
            system->config.enable_counting = false;
            break;
        case SAVANT_ABILITY_ALL:
            system->config.enable_calendar = false;
            system->config.enable_prime = false;
            system->config.enable_memorization = false;
            system->config.enable_pattern = false;
            system->config.enable_counting = false;
            break;
        default:
            break;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return SAVANT_SUCCESS;
}

/* ============================================================================
 * Calendar Calculation Implementation
 * ============================================================================ */

int savant_calendar_day_of_week(savant_system_t* system,
                                const savant_date_t* date,
                                savant_calendar_result_t* result) {
    if (!system || !date || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_calendar_day_of_week: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }
    if (!system->config.enable_calendar) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                              "savant_calendar_day_of_week: calendar not enabled");
        return SAVANT_ERROR_INVALID_STATE;
    }
    if (!savant_validate_date(date)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "savant_calendar_day_of_week: invalid date");
        return SAVANT_ERROR_INVALID_DATE;
    }

    float start_time = get_time_ms();

    /* Use Zeller's congruence for Gregorian calendar */
    int32_t q = date->day;
    int32_t m = date->month;
    int32_t year = date->year;

    /* Adjust for Jan/Feb (treat as months 13/14 of previous year) */
    if (m < 3) {
        m += 12;
        year--;
    }

    int32_t k = year % 100;
    int32_t j = year / 100;

    int32_t h = (q + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;

    /* Convert from Zeller (0=Saturday) to our enum (0=Sunday) */
    savant_day_t dow = (savant_day_t)((h + 6) % 7);

    /* Fill result */
    memset(result, 0, sizeof(savant_calendar_result_t));
    result->date = *date;
    result->day_of_week = dow;
    result->is_leap_year = savant_is_leap_year(date->year);
    result->days_in_month = days_in_month(date->year, date->month);
    result->julian_day = date_to_julian(date);
    result->confidence = 1.0f;

    /* Day of year */
    result->day_of_year = DAYS_BEFORE_MONTH[date->month] + date->day;
    if (date->month > 2 && result->is_leap_year) {
        result->day_of_year++;
    }

    /* Week of year (ISO 8601 approximation) */
    result->week_of_year = (result->day_of_year - dow + 10) / 7;

    float elapsed = get_time_ms() - start_time;

    /* Update statistics */
    nimcp_platform_mutex_lock(system->mutex);
    system->state.calendar_queries++;
    system->state.avg_calendar_time_ms =
        system->state.avg_calendar_time_ms * 0.99f + elapsed * 0.01f;
    system->stats.total_calendar_queries++;
    system->stats.avg_calendar_time_ms =
        system->stats.avg_calendar_time_ms * 0.99f + elapsed * 0.01f;
    if (elapsed < system->stats.fastest_calendar_ms ||
        system->stats.fastest_calendar_ms == 0.0f) {
        system->stats.fastest_calendar_ms = elapsed;
    }
    nimcp_platform_mutex_unlock(system->mutex);

    return SAVANT_SUCCESS;
}

int savant_calendar_analyze(savant_system_t* system,
                            const savant_date_t* date,
                            savant_calendar_result_t* result) {
    /* Just call day_of_week - it fills all fields */
    return savant_calendar_day_of_week(system, date, result);
}

int savant_calendar_next_day(savant_system_t* system,
                             const savant_date_t* from,
                             savant_day_t target_day,
                             savant_date_t* result) {
    if (!system || !from || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_calendar_next_day: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }
    if (!savant_validate_date(from)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "savant_calendar_next_day: invalid date");
        return SAVANT_ERROR_INVALID_DATE;
    }
    if (target_day > SAVANT_SATURDAY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "savant_calendar_next_day: invalid target day");
        return SAVANT_ERROR_INVALID_PARAM;
    }

    savant_calendar_result_t cal;
    savant_calendar_day_of_week(system, from, &cal);

    /* Calculate days to add */
    int32_t current_dow = (int32_t)cal.day_of_week;
    int32_t target_dow = (int32_t)target_day;
    int32_t days_to_add = (target_dow - current_dow + 7) % 7;
    if (days_to_add == 0) days_to_add = 7;  /* Next occurrence, not same day */

    /* Add days using Julian arithmetic */
    int32_t jd = cal.julian_day + days_to_add;
    julian_to_date(jd, result);

    return SAVANT_SUCCESS;
}

int savant_calendar_days_between(savant_system_t* system,
                                 const savant_date_t* date1,
                                 const savant_date_t* date2,
                                 int32_t* days) {
    if (!system || !date1 || !date2 || !days) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_calendar_days_between: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }
    if (!savant_validate_date(date1) || !savant_validate_date(date2)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "savant_calendar_days_between: invalid date");
        return SAVANT_ERROR_INVALID_DATE;
    }

    int32_t jd1 = date_to_julian(date1);
    int32_t jd2 = date_to_julian(date2);

    *days = jd2 - jd1;
    return SAVANT_SUCCESS;
}

const char* savant_day_name(savant_day_t day) {
    if (day > SAVANT_SATURDAY) return "Unknown";
    return DAY_NAMES[day];
}

savant_date_t savant_make_date(int32_t year, int32_t month, int32_t day) {
    savant_date_t date = {year, month, day};
    return date;
}

/* ============================================================================
 * Prime Number Implementation
 * ============================================================================ */

int savant_is_prime(savant_system_t* system,
                    int64_t number,
                    savant_prime_result_t* result) {
    if (!system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_is_prime: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }
    if (!system->config.enable_prime) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                              "savant_is_prime: prime detection not enabled");
        return SAVANT_ERROR_INVALID_STATE;
    }

    float start_time = get_time_ms();

    memset(result, 0, sizeof(savant_prime_result_t));
    result->number = number;

    if (number < 2) {
        result->is_prime = false;
        result->recognition_time_ms = get_time_ms() - start_time;
        return SAVANT_SUCCESS;
    }

    result->is_prime = is_prime_internal(&system->prime_cache, number);

    /* Find prime index if prime */
    if (result->is_prime && number <= system->prime_cache.limit) {
        for (uint32_t i = 0; i < system->prime_cache.prime_count; i++) {
            if (system->prime_cache.prime_list[i] == number) {
                result->prime_index = i + 1;
                break;
            }
        }
    }

    /* Find nearest primes if composite */
    if (!result->is_prime) {
        /* Search below */
        for (int64_t p = number - 1; p >= 2; p--) {
            if (is_prime_internal(&system->prime_cache, p)) {
                result->nearest_prime_below = p;
                break;
            }
        }

        /* Search above */
        for (int64_t p = number + 1; p <= number + 1000; p++) {
            if (is_prime_internal(&system->prime_cache, p)) {
                result->nearest_prime_above = p;
                break;
            }
        }
    }

    result->recognition_time_ms = get_time_ms() - start_time;

    /* Update statistics */
    nimcp_platform_mutex_lock(system->mutex);
    system->state.primality_checks++;
    system->stats.total_primality_checks++;
    if (result->is_prime) {
        system->stats.primes_identified++;
    }
    system->stats.avg_primality_time_ms =
        system->stats.avg_primality_time_ms * 0.99f + result->recognition_time_ms * 0.01f;
    nimcp_platform_mutex_unlock(system->mutex);

    return SAVANT_SUCCESS;
}

int savant_factorize(savant_system_t* system,
                     int64_t number,
                     int64_t* factors,
                     uint32_t max_factors,
                     uint32_t* num_factors) {
    if (!system || !factors || !num_factors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_factorize: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }
    if (!system->config.enable_factorization) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                              "savant_factorize: factorization not enabled");
        return SAVANT_ERROR_INVALID_STATE;
    }

    *num_factors = 0;

    if (number < 2) return SAVANT_SUCCESS;

    int64_t n = number;

    /* Trial division with cached primes */
    for (uint32_t i = 0; i < system->prime_cache.prime_count && n > 1; i++) {
        int64_t p = system->prime_cache.prime_list[i];
        if (p * p > n) break;

        while (n % p == 0) {
            if (*num_factors >= max_factors) return SAVANT_ERROR_BUFFER_TOO_SMALL;
            factors[(*num_factors)++] = p;
            n /= p;
        }
    }

    /* Remaining factor (if any) is prime */
    if (n > 1) {
        if (*num_factors >= max_factors) return SAVANT_ERROR_BUFFER_TOO_SMALL;
        factors[(*num_factors)++] = n;
    }

    nimcp_platform_mutex_lock(system->mutex);
    system->stats.factorizations_done++;
    nimcp_platform_mutex_unlock(system->mutex);

    return SAVANT_SUCCESS;
}

int savant_nth_prime(savant_system_t* system,
                     uint32_t n,
                     int64_t* prime) {
    if (!system || !prime) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_nth_prime: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }
    if (n < 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "savant_nth_prime: n must be >= 1");
        return SAVANT_ERROR_INVALID_PARAM;
    }

    if (n <= system->prime_cache.prime_count) {
        *prime = system->prime_cache.prime_list[n - 1];
        return SAVANT_SUCCESS;
    }

    return SAVANT_ERROR_OUT_OF_RANGE;
}

int savant_count_primes(savant_system_t* system,
                        int64_t limit,
                        uint32_t* count) {
    if (!system || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_count_primes: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }

    if (limit <= system->prime_cache.limit) {
        /* Count from cache */
        *count = 0;
        for (uint32_t i = 0; i < system->prime_cache.prime_count; i++) {
            if (system->prime_cache.prime_list[i] <= limit) {
                (*count)++;
            } else {
                break;
            }
        }
        return SAVANT_SUCCESS;
    }

    return SAVANT_ERROR_OUT_OF_RANGE;
}

int savant_nearest_prime(savant_system_t* system,
                         int64_t number,
                         int32_t direction,
                         int64_t* prime) {
    if (!system || !prime) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_nearest_prime: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }

    savant_prime_result_t result;
    savant_is_prime(system, number, &result);

    if (result.is_prime) {
        *prime = number;
        return SAVANT_SUCCESS;
    }

    if (direction <= 0 && result.nearest_prime_below > 0) {
        *prime = result.nearest_prime_below;
        return SAVANT_SUCCESS;
    }

    if (direction >= 0 && result.nearest_prime_above > 0) {
        *prime = result.nearest_prime_above;
        return SAVANT_SUCCESS;
    }

    return SAVANT_ERROR_OUT_OF_RANGE;
}

/* ============================================================================
 * Pattern Memory Implementation
 * ============================================================================ */

int savant_learn_pattern(savant_system_t* system,
                         const float* data,
                         uint32_t length,
                         savant_pattern_type_t type,
                         const char* label,
                         uint32_t* pattern_id) {
    if (!system || !data || !pattern_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_learn_pattern: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }
    if (!system->config.enable_memorization) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                              "savant_learn_pattern: memorization not enabled");
        return SAVANT_ERROR_INVALID_STATE;
    }
    if (length == 0 || length > system->config.max_pattern_length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "savant_learn_pattern: invalid pattern length");
        return SAVANT_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Check capacity */
    if (system->pattern_count >= system->config.max_patterns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE,
                              "savant_learn_pattern: pattern capacity exceeded");
        nimcp_platform_mutex_unlock(system->mutex);
        return SAVANT_ERROR_CAPACITY_EXCEEDED;
    }

    /* Compute hash */
    uint64_t hash = compute_pattern_hash(data, length);
    uint32_t bucket_idx = (uint32_t)(hash % system->table_size);

    /* Allocate bucket */
    pattern_bucket_t* bucket = nimcp_calloc(1, sizeof(pattern_bucket_t));
    if (!bucket) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(pattern_bucket_t),
                           "savant_learn_pattern: Failed to allocate bucket");
        nimcp_platform_mutex_unlock(system->mutex);
        return SAVANT_ERROR_NO_MEMORY;
    }

    /* Copy data */
    bucket->data_copy = nimcp_malloc(length * sizeof(float));
    if (!bucket->data_copy) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, length * sizeof(float),
                           "savant_learn_pattern: Failed to copy pattern data");
        nimcp_free(bucket);
        nimcp_platform_mutex_unlock(system->mutex);
        return SAVANT_ERROR_NO_MEMORY;
    }
    memcpy(bucket->data_copy, data, length * sizeof(float));

    /* Copy label if provided */
    if (label) {
        size_t label_len = strlen(label) + 1;
        bucket->label_copy = nimcp_malloc(label_len);
        if (bucket->label_copy) {
            memcpy(bucket->label_copy, label, label_len);
        }
    }

    /* Fill pattern info */
    bucket->pattern.pattern_id = system->next_pattern_id++;
    bucket->pattern.type = type;
    bucket->pattern.data = bucket->data_copy;
    bucket->pattern.length = length;
    bucket->pattern.hash = hash;
    bucket->pattern.recall_count = 0;
    bucket->pattern.learned_time_ms = (uint64_t)get_time_ms();
    bucket->pattern.last_recall_ms = 0;
    bucket->pattern.strength = 1.0f;
    bucket->pattern.label = bucket->label_copy;
    bucket->active = true;

    /* Insert into hash table */
    bucket->next = system->pattern_table[bucket_idx];
    system->pattern_table[bucket_idx] = bucket;

    system->pattern_count++;
    system->state.patterns_stored = system->pattern_count;

    *pattern_id = bucket->pattern.pattern_id;

    /* Update statistics */
    system->stats.patterns_learned++;

    nimcp_platform_mutex_unlock(system->mutex);
    return SAVANT_SUCCESS;
}

int savant_recall_pattern(savant_system_t* system,
                          uint32_t pattern_id,
                          float* data,
                          uint32_t buffer_size,
                          uint32_t* length,
                          savant_recall_level_t* recall_level) {
    if (!system || !data || !length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_recall_pattern: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Search for pattern */
    pattern_bucket_t* found = NULL;
    for (uint32_t i = 0; i < system->table_size && !found; i++) {
        pattern_bucket_t* bucket = system->pattern_table[i];
        while (bucket) {
            if (bucket->active && bucket->pattern.pattern_id == pattern_id) {
                found = bucket;
                break;
            }
            bucket = bucket->next;
        }
    }

    if (!found) {
        nimcp_platform_mutex_unlock(system->mutex);
        return SAVANT_ERROR_PATTERN_NOT_FOUND;
    }

    /* Check buffer size */
    if (found->pattern.length > buffer_size) {
        *length = found->pattern.length;
        nimcp_platform_mutex_unlock(system->mutex);
        return SAVANT_ERROR_BUFFER_TOO_SMALL;
    }

    /* Copy pattern data */
    memcpy(data, found->pattern.data, found->pattern.length * sizeof(float));
    *length = found->pattern.length;

    /* Update recall info */
    found->pattern.recall_count++;
    found->pattern.last_recall_ms = (uint64_t)get_time_ms();
    found->pattern.strength += system->config.memory_strength_boost;
    if (found->pattern.strength > 1.0f) {
        found->pattern.strength = 1.0f;
    }

    /* Determine recall level based on strength */
    if (recall_level) {
        if (found->pattern.strength > 0.95f) {
            *recall_level = SAVANT_RECALL_PERFECT;
        } else if (found->pattern.strength > 0.8f) {
            *recall_level = SAVANT_RECALL_HIGH;
        } else if (found->pattern.strength > 0.5f) {
            *recall_level = SAVANT_RECALL_MODERATE;
        } else {
            *recall_level = SAVANT_RECALL_DEGRADED;
        }
    }

    /* Update statistics */
    system->state.total_recalls++;
    system->stats.patterns_recalled++;
    if (recall_level && *recall_level == SAVANT_RECALL_PERFECT) {
        system->stats.perfect_recall_rate =
            (system->stats.perfect_recall_rate * (system->stats.patterns_recalled - 1) + 1.0f) /
            system->stats.patterns_recalled;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return SAVANT_SUCCESS;
}

int savant_find_patterns(savant_system_t* system,
                         const float* query,
                         uint32_t query_length,
                         savant_match_result_t* matches,
                         uint32_t max_matches,
                         uint32_t* num_matches) {
    if (!system || !query || !matches || !num_matches) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_find_patterns: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }
    if (!system->config.enable_pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                              "savant_find_patterns: pattern finding not enabled");
        return SAVANT_ERROR_INVALID_STATE;
    }

    *num_matches = 0;

    nimcp_platform_mutex_lock(system->mutex);

    /* Compute query hash for exact matching */
    uint64_t query_hash = compute_pattern_hash(query, query_length);

    /* Search all patterns for matches */
    for (uint32_t i = 0; i < system->table_size && *num_matches < max_matches; i++) {
        pattern_bucket_t* bucket = system->pattern_table[i];
        while (bucket && *num_matches < max_matches) {
            if (!bucket->active) {
                bucket = bucket->next;
                continue;
            }

            /* Check for exact hash match */
            if (bucket->pattern.hash == query_hash &&
                bucket->pattern.length == query_length) {
                /* Verify data match */
                bool exact = true;
                for (uint32_t j = 0; j < query_length; j++) {
                    if (fabsf(bucket->pattern.data[j] - query[j]) > 0.001f) {
                        exact = false;
                        break;
                    }
                }

                if (exact) {
                    savant_match_result_t* m = &matches[*num_matches];
                    m->pattern_id = bucket->pattern.pattern_id;
                    m->match_score = 1.0f;
                    m->match_offset = 0;
                    m->match_length = query_length;
                    m->type = bucket->pattern.type;
                    m->label = bucket->pattern.label;
                    (*num_matches)++;

                    system->stats.pattern_matches++;
                    bucket = bucket->next;
                    continue;
                }
            }

            /* Check for partial match (substring) */
            if (bucket->pattern.length >= query_length) {
                for (uint32_t offset = 0;
                     offset <= bucket->pattern.length - query_length &&
                     *num_matches < max_matches;
                     offset++) {

                    float match_sum = 0.0f;
                    for (uint32_t j = 0; j < query_length; j++) {
                        float diff = fabsf(bucket->pattern.data[offset + j] - query[j]);
                        match_sum += 1.0f - fminf(diff, 1.0f);
                    }
                    float match_score = match_sum / query_length;

                    if (match_score >= system->config.match_threshold) {
                        savant_match_result_t* m = &matches[*num_matches];
                        m->pattern_id = bucket->pattern.pattern_id;
                        m->match_score = match_score;
                        m->match_offset = offset;
                        m->match_length = query_length;
                        m->type = bucket->pattern.type;
                        m->label = bucket->pattern.label;
                        (*num_matches)++;

                        system->stats.pattern_matches++;
                        break;  /* One match per pattern */
                    }
                }
            }

            bucket = bucket->next;
        }
    }

    /* Update average match confidence */
    if (*num_matches > 0) {
        float total_conf = 0.0f;
        for (uint32_t i = 0; i < *num_matches; i++) {
            total_conf += matches[i].match_score;
        }
        system->stats.avg_match_confidence =
            (system->stats.avg_match_confidence * 0.9f) +
            (total_conf / *num_matches * 0.1f);
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return SAVANT_SUCCESS;
}

int savant_forget_pattern(savant_system_t* system, uint32_t pattern_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_forget_pattern: system is NULL");
        return SAVANT_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    for (uint32_t i = 0; i < system->table_size; i++) {
        pattern_bucket_t** pp = &system->pattern_table[i];
        while (*pp) {
            if ((*pp)->active && (*pp)->pattern.pattern_id == pattern_id) {
                pattern_bucket_t* to_remove = *pp;
                *pp = to_remove->next;

                free_pattern_bucket(to_remove);
                nimcp_free(to_remove);

                system->pattern_count--;
                system->state.patterns_stored = system->pattern_count;

                nimcp_platform_mutex_unlock(system->mutex);
                return SAVANT_SUCCESS;
            }
            pp = &(*pp)->next;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return SAVANT_ERROR_PATTERN_NOT_FOUND;
}

int savant_strengthen_pattern(savant_system_t* system,
                              uint32_t pattern_id,
                              float amount) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_strengthen_pattern: system is NULL");
        return SAVANT_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    for (uint32_t i = 0; i < system->table_size; i++) {
        pattern_bucket_t* bucket = system->pattern_table[i];
        while (bucket) {
            if (bucket->active && bucket->pattern.pattern_id == pattern_id) {
                bucket->pattern.strength += amount;
                if (bucket->pattern.strength > 1.0f) {
                    bucket->pattern.strength = 1.0f;
                }
                if (bucket->pattern.strength < 0.0f) {
                    bucket->pattern.strength = 0.0f;
                }

                nimcp_platform_mutex_unlock(system->mutex);
                return SAVANT_SUCCESS;
            }
            bucket = bucket->next;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return SAVANT_ERROR_PATTERN_NOT_FOUND;
}

/* ============================================================================
 * Rapid Counting Implementation
 * ============================================================================ */

int savant_count_items(savant_system_t* system,
                       const float* items,
                       uint32_t array_length,
                       savant_count_result_t* result) {
    if (!system || !items || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_count_items: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }
    if (!system->config.enable_counting) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                              "savant_count_items: counting not enabled");
        return SAVANT_ERROR_INVALID_STATE;
    }

    float start_time = get_time_ms();

    memset(result, 0, sizeof(savant_count_result_t));

    /* Count non-zero items */
    uint32_t count = 0;
    for (uint32_t i = 0; i < array_length; i++) {
        if (fabsf(items[i]) > 0.001f) {
            count++;
        }
    }

    result->count = count;
    result->counting_time_ms = get_time_ms() - start_time;
    result->is_exact = true;
    result->estimate_error = 0;

    /* Confidence based on count vs subitizing limit */
    if (count <= system->config.subitizing_limit) {
        result->confidence = 1.0f;
    } else {
        result->confidence = system->config.counting_accuracy;
    }

    /* Update statistics */
    nimcp_platform_mutex_lock(system->mutex);
    system->stats.total_counts++;
    system->stats.avg_counting_accuracy =
        system->stats.avg_counting_accuracy * 0.99f + result->confidence * 0.01f;
    system->stats.avg_counting_time_ms =
        system->stats.avg_counting_time_ms * 0.99f + result->counting_time_ms * 0.01f;
    nimcp_platform_mutex_unlock(system->mutex);

    return SAVANT_SUCCESS;
}

int savant_estimate_count(savant_system_t* system,
                          const float* items,
                          uint32_t array_length,
                          float sample_rate,
                          savant_count_result_t* result) {
    if (!system || !items || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_estimate_count: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }
    if (sample_rate <= 0.0f || sample_rate > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "savant_estimate_count: invalid sample rate");
        return SAVANT_ERROR_INVALID_PARAM;
    }

    float start_time = get_time_ms();

    memset(result, 0, sizeof(savant_count_result_t));

    /* Sample and count */
    uint32_t sample_size = (uint32_t)(array_length * sample_rate);
    if (sample_size < 1) sample_size = 1;

    uint32_t step = array_length / sample_size;
    if (step < 1) step = 1;

    uint32_t sample_count = 0;
    uint32_t samples_taken = 0;

    for (uint32_t i = 0; i < array_length && samples_taken < sample_size; i += step) {
        if (fabsf(items[i]) > 0.001f) {
            sample_count++;
        }
        samples_taken++;
    }

    /* Estimate total from sample */
    float density = samples_taken > 0 ? (float)sample_count / samples_taken : 0.0f;
    result->count = (uint32_t)(density * array_length);
    result->is_exact = false;
    result->estimate_error = (uint32_t)(result->count * (1.0f - sample_rate) * 0.5f);
    result->confidence = sample_rate * 0.9f + 0.1f;
    result->counting_time_ms = get_time_ms() - start_time;

    /* Update statistics */
    nimcp_platform_mutex_lock(system->mutex);
    system->stats.total_counts++;
    system->stats.avg_counting_time_ms =
        system->stats.avg_counting_time_ms * 0.99f + result->counting_time_ms * 0.01f;
    nimcp_platform_mutex_unlock(system->mutex);

    return SAVANT_SUCCESS;
}

/* ============================================================================
 * State and Statistics Implementation
 * ============================================================================ */

int savant_get_state(const savant_system_t* system, savant_state_t* state) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_get_state: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }

    *state = system->state;
    return SAVANT_SUCCESS;
}

int savant_get_stats(const savant_system_t* system, savant_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_get_stats: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }

    *stats = system->stats;
    return SAVANT_SUCCESS;
}

int savant_reset_stats(savant_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_reset_stats: system is NULL");
        return SAVANT_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);
    memset(&system->stats, 0, sizeof(savant_stats_t));
    nimcp_platform_mutex_unlock(system->mutex);

    return SAVANT_SUCCESS;
}

int savant_get_pattern_info(const savant_system_t* system,
                            uint32_t pattern_id,
                            savant_pattern_t* pattern) {
    if (!system || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "savant_get_pattern_info: NULL parameter");
        return SAVANT_ERROR_NULL_POINTER;
    }

    for (uint32_t i = 0; i < system->table_size; i++) {
        pattern_bucket_t* bucket = system->pattern_table[i];
        while (bucket) {
            if (bucket->active && bucket->pattern.pattern_id == pattern_id) {
                *pattern = bucket->pattern;
                return SAVANT_SUCCESS;
            }
            bucket = bucket->next;
        }
    }

    return SAVANT_ERROR_PATTERN_NOT_FOUND;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

savant_prime_result_t* savant_prime_result_create(uint32_t max_factors) {
    savant_prime_result_t* result = nimcp_calloc(1, sizeof(savant_prime_result_t));
    if (!result) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(savant_prime_result_t),
                           "savant_prime_result_create: Failed to allocate result");
        return NULL;
    }

    if (max_factors > 0) {
        result->factors = nimcp_calloc(max_factors, sizeof(int64_t));
        if (!result->factors) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, max_factors * sizeof(int64_t),
                               "savant_prime_result_create: Failed to allocate factors");
            nimcp_free(result);
            return NULL;
        }
    }

    return result;
}

void savant_prime_result_destroy(savant_prime_result_t* result) {
    if (!result) return;

    if (result->factors) {
        nimcp_free(result->factors);
    }
    nimcp_free(result);
}

const char* savant_error_string(savant_error_t error) {
    switch (error) {
        case SAVANT_SUCCESS:                 return "Success";
        case SAVANT_ERROR_NULL_POINTER:      return "Null pointer";
        case SAVANT_ERROR_INVALID_PARAM:     return "Invalid parameter";
        case SAVANT_ERROR_NO_MEMORY:         return "Memory allocation failed";
        case SAVANT_ERROR_NOT_INITIALIZED:   return "System not initialized";
        case SAVANT_ERROR_INVALID_STATE:     return "Invalid state";
        case SAVANT_ERROR_BUFFER_TOO_SMALL:  return "Buffer too small";
        case SAVANT_ERROR_PATTERN_NOT_FOUND: return "Pattern not found";
        case SAVANT_ERROR_CAPACITY_EXCEEDED: return "Capacity exceeded";
        case SAVANT_ERROR_INVALID_DATE:      return "Invalid date";
        case SAVANT_ERROR_OUT_OF_RANGE:      return "Out of range";
        default:                              return "Unknown error";
    }
}
