/**
 * @file nimcp_exception_metrics.c
 * @brief Exception metrics, telemetry, and adaptive recovery implementation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Implementation of exception metrics and adaptive recovery
 * WHY:  Enable observability and self-improving recovery
 * HOW:  Atomic counters, EMA calculations, pattern hash table
 *
 * THREAD SAFETY:
 * - All counters use atomic operations
 * - Pattern table uses mutex for structural modifications
 * - EMA updates are lock-free
 *
 * @author NIMCP Development Team
 */

#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <time.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(exception_metrics)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal category tracking with atomic counters
 */
typedef struct {
    nimcp_exception_category_t category;
    volatile uint64_t total_count;
    volatile uint64_t minute_count;
    volatile uint64_t hour_count;
    volatile uint64_t last_occurrence_us;
    float rate_ema;
    uint64_t last_rate_update_us;
} internal_category_t;

/**
 * @brief Internal recovery tracking with atomic counters
 */
typedef struct {
    volatile uint64_t attempts;
    volatile uint64_t successes;
    volatile uint64_t failures;
    volatile uint64_t total_time_us;
    volatile uint64_t min_time_us;
    volatile uint64_t max_time_us;
} internal_recovery_t;

/**
 * @brief Pattern hash table entry
 */
typedef struct pattern_entry {
    nimcp_adaptive_pattern_t pattern;
    uint32_t hash;
    bool occupied;
    struct pattern_entry* next;  /* For collision chaining */
} pattern_entry_t;

/* ============================================================================
 * Module State
 * ============================================================================ */

static bool g_metrics_initialized = false;
static bool g_adaptive_initialized = false;
static nimcp_metrics_config_t g_config;
static nimcp_platform_mutex_t* g_metrics_mutex = NULL;
static nimcp_platform_mutex_t* g_adaptive_mutex = NULL;

/* P2-U28: Global counters use GCC __atomic_* builtins for atomicity.
 * The volatile qualifier is retained for compatibility but the actual
 * thread-safety comes from __atomic_add_fetch, __atomic_load_n, etc.
 * We avoid C11 _Atomic here because the __atomic builtins are already
 * used consistently throughout the codebase and work on plain types. */
static volatile uint64_t g_total_exceptions = 0;
static volatile uint64_t g_total_recoveries_attempted = 0;
static volatile uint64_t g_total_recoveries_succeeded = 0;
static volatile uint64_t g_peak_rate = 0;
static uint64_t g_start_time_us = 0;
static float g_current_rate_ema = 0.0f;
static uint64_t g_last_rate_update_us = 0;

/* Per-category tracking */
static internal_category_t g_categories[NIMCP_METRICS_MAX_CATEGORIES];
static size_t g_category_count = 0;

/* Per-action recovery tracking */
static internal_recovery_t g_recovery[NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT];

/* Adaptive pattern storage */
static pattern_entry_t* g_patterns = NULL;
static size_t g_pattern_count = 0;

/* Adaptive stats */
static volatile uint64_t g_suggestions_made = 0;
static volatile uint64_t g_suggestions_followed = 0;
static volatile uint64_t g_patterns_reset = 0;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Compute hash for epitope
 */
static uint32_t hash_epitope(const uint8_t* epitope, size_t len) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + epitope[i];
    }
    return hash;
}

/**
 * @brief Compare epitopes
 */
static bool epitopes_equal(const uint8_t* a, size_t a_len, const uint8_t* b, size_t b_len) {
    if (a_len != b_len) return false;
    return memcmp(a, b, a_len) == 0;
}

/**
 * @brief Find or create category entry
 */
static internal_category_t* find_or_create_category(nimcp_exception_category_t category) {
    /* Check existing */
    for (size_t i = 0; i < g_category_count; i++) {
        if (g_categories[i].category == category) {
            return &g_categories[i];
        }
    }

    /* Create new if space available */
    if (g_category_count < NIMCP_METRICS_MAX_CATEGORIES) {
        internal_category_t* cat = &g_categories[g_category_count];
        cat->category = category;
        cat->total_count = 0;
        cat->minute_count = 0;
        cat->hour_count = 0;
        cat->last_occurrence_us = 0;
        cat->rate_ema = 0.0f;
        cat->last_rate_update_us = get_timestamp_us();
        g_category_count++;
        return cat;
    }

    return NULL;
}

/**
 * @brief Find pattern by epitope
 */
static pattern_entry_t* find_pattern(const uint8_t* epitope, size_t len) {
    if (!g_patterns || len == 0) return NULL;

    uint32_t hash = hash_epitope(epitope, len);
    size_t index = hash % NIMCP_METRICS_MAX_PATTERNS;

    pattern_entry_t* entry = &g_patterns[index];
    while (entry) {
        if (entry->occupied && entry->hash == hash &&
            epitopes_equal(entry->pattern.epitope, entry->pattern.epitope_len, epitope, len)) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

/**
 * @brief Find or create pattern entry
 */
static pattern_entry_t* find_or_create_pattern(const uint8_t* epitope, size_t len) {
    if (!g_patterns || len == 0 || len > 64) return NULL;

    uint32_t hash = hash_epitope(epitope, len);
    size_t index = hash % NIMCP_METRICS_MAX_PATTERNS;

    pattern_entry_t* entry = &g_patterns[index];

    /* Check if slot is free or matching */
    if (!entry->occupied) {
        /* Use this slot */
        entry->hash = hash;
        entry->occupied = true;
        memcpy(entry->pattern.epitope, epitope, len);
        entry->pattern.epitope_len = len;
        entry->pattern.preferred_action = EXCEPTION_RECOVERY_NONE;
        memset(entry->pattern.action_success_rates, 0, sizeof(entry->pattern.action_success_rates));
        memset(entry->pattern.action_attempts, 0, sizeof(entry->pattern.action_attempts));
        entry->pattern.last_success_us = 0;
        entry->pattern.consecutive_failures = 0;
        entry->pattern.learned = false;
        g_pattern_count++;
        return entry;
    }

    /* Check for match */
    if (entry->hash == hash &&
        epitopes_equal(entry->pattern.epitope, entry->pattern.epitope_len, epitope, len)) {
        return entry;
    }

    /* Collision - walk chain */
    while (entry->next) {
        entry = entry->next;
        if (entry->hash == hash &&
            epitopes_equal(entry->pattern.epitope, entry->pattern.epitope_len, epitope, len)) {
            return entry;
        }
    }

    /* Create new entry in chain */
    if (g_pattern_count < NIMCP_METRICS_MAX_PATTERNS) {
        pattern_entry_t* new_entry = nimcp_calloc(1, sizeof(pattern_entry_t));
        if (!new_entry) {
            return NULL;
        }

        new_entry->hash = hash;
        new_entry->occupied = true;
        memcpy(new_entry->pattern.epitope, epitope, len);
        new_entry->pattern.epitope_len = len;
        new_entry->pattern.preferred_action = EXCEPTION_RECOVERY_NONE;
        entry->next = new_entry;
        g_pattern_count++;
        return new_entry;
    }

    return NULL;
}

/**
 * @brief Update EMA rate
 */
static void update_rate_ema(internal_category_t* cat, uint64_t now_us) {
    if (!cat) return;

    uint64_t elapsed_us = now_us - cat->last_rate_update_us;
    if (elapsed_us < 100000) return;  /* Update at most every 100ms */

    float elapsed_sec = (float)elapsed_us / 1000000.0f;
    float instant_rate = 1.0f / elapsed_sec;

    /* EMA: rate = alpha * instant + (1 - alpha) * prev */
    cat->rate_ema = g_config.ema_alpha * instant_rate +
                    (1.0f - g_config.ema_alpha) * cat->rate_ema;

    cat->last_rate_update_us = now_us;
}

/**
 * @brief Determine best action for pattern based on success rates
 */
static nimcp_exception_recovery_action_t get_best_action(const nimcp_adaptive_pattern_t* pattern) {
    if (!pattern) return EXCEPTION_RECOVERY_NONE;

    nimcp_exception_recovery_action_t best_action = EXCEPTION_RECOVERY_NONE;
    float best_rate = -1.0f;

    for (int i = 0; i < NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT; i++) {
        if (pattern->action_attempts[i] >= g_config.min_samples_for_learning) {
            if (pattern->action_success_rates[i] > best_rate) {
                best_rate = pattern->action_success_rates[i];
                best_action = (nimcp_exception_recovery_action_t)i;
            }
        }
    }

    return best_action;
}

/* ============================================================================
 * Metrics API Implementation
 * ============================================================================ */

void nimcp_metrics_default_config(nimcp_metrics_config_t* config) {
    if (!config) return;

    config->enable_rate_tracking = true;
    config->enable_adaptive_recovery = true;
    config->ema_alpha = NIMCP_METRICS_EMA_ALPHA;
    config->min_samples_for_learning = NIMCP_METRICS_MIN_SAMPLES;
    config->rate_update_interval_ms = 100;
    config->persist_patterns = false;
}

int nimcp_metrics_init(void) {
    return nimcp_metrics_init_with_config(NULL);
}

int nimcp_metrics_init_with_config(const nimcp_metrics_config_t* config) {
    if (g_metrics_initialized) return 0;

    if (config) {
        g_config = *config;
    } else {
        nimcp_metrics_default_config(&g_config);
    }

    g_metrics_mutex = nimcp_platform_mutex_create();
    if (!g_metrics_mutex) return -1;

    /* Initialize state */
    g_total_exceptions = 0;
    g_total_recoveries_attempted = 0;
    g_total_recoveries_succeeded = 0;
    g_peak_rate = 0;
    g_start_time_us = get_timestamp_us();
    g_current_rate_ema = 0.0f;
    g_last_rate_update_us = g_start_time_us;

    memset(g_categories, 0, sizeof(g_categories));
    g_category_count = 0;

    memset(g_recovery, 0, sizeof(g_recovery));
    for (int i = 0; i < NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT; i++) {
        g_recovery[i].min_time_us = UINT64_MAX;
    }

    g_metrics_initialized = true;
    LOG_INFO("Exception metrics initialized");

    return 0;
}

void nimcp_metrics_shutdown(void) {
    if (!g_metrics_initialized) return;

    if (g_metrics_mutex) {
        nimcp_platform_mutex_destroy(g_metrics_mutex);
        nimcp_free(g_metrics_mutex);
        g_metrics_mutex = NULL;
    }

    g_metrics_initialized = false;
    LOG_INFO("Exception metrics shutdown");
}

void nimcp_metrics_record_exception(nimcp_exception_t* ex) {
    if (!g_metrics_initialized || !ex) return;

    uint64_t now_us = get_timestamp_us();

    /* Increment global counter atomically */
    __atomic_add_fetch(&g_total_exceptions, 1, __ATOMIC_SEQ_CST);

    /* Find or create category entry */
    if (g_metrics_mutex) nimcp_platform_mutex_lock(g_metrics_mutex);

    internal_category_t* cat = find_or_create_category(ex->category);
    if (cat) {
        __atomic_add_fetch(&cat->total_count, 1, __ATOMIC_SEQ_CST);
        __atomic_add_fetch(&cat->minute_count, 1, __ATOMIC_SEQ_CST);
        __atomic_add_fetch(&cat->hour_count, 1, __ATOMIC_SEQ_CST);
        __atomic_store_n(&cat->last_occurrence_us, now_us, __ATOMIC_SEQ_CST);

        /* Update rate EMA */
        if (g_config.enable_rate_tracking) {
            update_rate_ema(cat, now_us);
        }
    }

    /* Update global rate EMA */
    uint64_t elapsed_us = now_us - g_last_rate_update_us;
    if (elapsed_us >= 100000) {  /* 100ms */
        float elapsed_sec = (float)elapsed_us / 1000000.0f;
        float instant_rate = 1.0f / elapsed_sec;
        g_current_rate_ema = g_config.ema_alpha * instant_rate +
                             (1.0f - g_config.ema_alpha) * g_current_rate_ema;
        g_last_rate_update_us = now_us;

        /* Update peak */
        uint64_t rate_int = (uint64_t)g_current_rate_ema;
        uint64_t old_peak = __atomic_load_n(&g_peak_rate, __ATOMIC_SEQ_CST);
        while (rate_int > old_peak) {
            if (__atomic_compare_exchange_n(&g_peak_rate, &old_peak, rate_int,
                                            false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                break;
            }
        }
    }

    if (g_metrics_mutex) nimcp_platform_mutex_unlock(g_metrics_mutex);
}

void nimcp_metrics_record_recovery(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    bool success,
    uint64_t duration_us
) {
    if (!g_metrics_initialized) return;
    if (action < 0 || action >= NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT) return;

    internal_recovery_t* rec = &g_recovery[action];

    /* Update counters atomically */
    __atomic_add_fetch(&g_total_recoveries_attempted, 1, __ATOMIC_SEQ_CST);
    __atomic_add_fetch(&rec->attempts, 1, __ATOMIC_SEQ_CST);

    if (success) {
        __atomic_add_fetch(&g_total_recoveries_succeeded, 1, __ATOMIC_SEQ_CST);
        __atomic_add_fetch(&rec->successes, 1, __ATOMIC_SEQ_CST);
    } else {
        __atomic_add_fetch(&rec->failures, 1, __ATOMIC_SEQ_CST);
    }

    /* Update timing */
    __atomic_add_fetch(&rec->total_time_us, duration_us, __ATOMIC_SEQ_CST);

    /* Update min/max with CAS */
    uint64_t old_min = __atomic_load_n(&rec->min_time_us, __ATOMIC_SEQ_CST);
    while (duration_us < old_min) {
        if (__atomic_compare_exchange_n(&rec->min_time_us, &old_min, duration_us,
                                        false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            break;
        }
    }

    uint64_t old_max = __atomic_load_n(&rec->max_time_us, __ATOMIC_SEQ_CST);
    while (duration_us > old_max) {
        if (__atomic_compare_exchange_n(&rec->max_time_us, &old_max, duration_us,
                                        false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            break;
        }
    }

    /* Record for adaptive learning */
    if (ex && g_adaptive_initialized) {
        nimcp_adaptive_record_outcome(ex, action, success);
    }
}

void nimcp_metrics_get(nimcp_exception_metrics_t* metrics) {
    if (!metrics) return;

    memset(metrics, 0, sizeof(nimcp_exception_metrics_t));

    if (!g_metrics_initialized) return;

    metrics->total_exceptions = __atomic_load_n(&g_total_exceptions, __ATOMIC_SEQ_CST);
    metrics->total_recoveries_attempted = __atomic_load_n(&g_total_recoveries_attempted, __ATOMIC_SEQ_CST);
    metrics->total_recoveries_succeeded = __atomic_load_n(&g_total_recoveries_succeeded, __ATOMIC_SEQ_CST);

    if (metrics->total_recoveries_attempted > 0) {
        metrics->overall_recovery_rate = (float)metrics->total_recoveries_succeeded /
                                         (float)metrics->total_recoveries_attempted;
    }

    metrics->current_rate_per_second = g_current_rate_ema;
    metrics->peak_rate_per_second = __atomic_load_n(&g_peak_rate, __ATOMIC_SEQ_CST);
    metrics->uptime_us = get_timestamp_us() - g_start_time_us;

    /* Copy category metrics */
    if (g_metrics_mutex) nimcp_platform_mutex_lock(g_metrics_mutex);

    metrics->category_count = g_category_count;
    for (size_t i = 0; i < g_category_count && i < NIMCP_METRICS_MAX_CATEGORIES; i++) {
        metrics->categories[i].category = g_categories[i].category;
        metrics->categories[i].total_count = __atomic_load_n(&g_categories[i].total_count, __ATOMIC_SEQ_CST);
        metrics->categories[i].count_last_minute = __atomic_load_n(&g_categories[i].minute_count, __ATOMIC_SEQ_CST);
        metrics->categories[i].count_last_hour = __atomic_load_n(&g_categories[i].hour_count, __ATOMIC_SEQ_CST);
        metrics->categories[i].rate_per_second = g_categories[i].rate_ema;
        metrics->categories[i].last_occurrence_us = __atomic_load_n(&g_categories[i].last_occurrence_us, __ATOMIC_SEQ_CST);
    }

    if (g_metrics_mutex) nimcp_platform_mutex_unlock(g_metrics_mutex);

    /* Copy recovery metrics */
    for (int i = 0; i < NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT; i++) {
        metrics->recovery[i].action = (nimcp_exception_recovery_action_t)i;
        metrics->recovery[i].attempts = __atomic_load_n(&g_recovery[i].attempts, __ATOMIC_SEQ_CST);
        metrics->recovery[i].successes = __atomic_load_n(&g_recovery[i].successes, __ATOMIC_SEQ_CST);
        metrics->recovery[i].failures = __atomic_load_n(&g_recovery[i].failures, __ATOMIC_SEQ_CST);

        if (metrics->recovery[i].attempts > 0) {
            metrics->recovery[i].success_rate = (float)metrics->recovery[i].successes /
                                                (float)metrics->recovery[i].attempts;

            uint64_t total_time = __atomic_load_n(&g_recovery[i].total_time_us, __ATOMIC_SEQ_CST);
            metrics->recovery[i].total_time_us = total_time;
            metrics->recovery[i].avg_time_us = (float)total_time / (float)metrics->recovery[i].attempts;
        }

        uint64_t min_time = __atomic_load_n(&g_recovery[i].min_time_us, __ATOMIC_SEQ_CST);
        metrics->recovery[i].min_time_us = (min_time == UINT64_MAX) ? 0 : min_time;
        metrics->recovery[i].max_time_us = __atomic_load_n(&g_recovery[i].max_time_us, __ATOMIC_SEQ_CST);
    }
}

void nimcp_metrics_reset(void) {
    if (!g_metrics_initialized) return;

    __atomic_store_n(&g_total_exceptions, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&g_total_recoveries_attempted, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&g_total_recoveries_succeeded, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&g_peak_rate, 0, __ATOMIC_SEQ_CST);

    g_start_time_us = get_timestamp_us();
    g_current_rate_ema = 0.0f;
    g_last_rate_update_us = g_start_time_us;

    if (g_metrics_mutex) nimcp_platform_mutex_lock(g_metrics_mutex);

    for (size_t i = 0; i < g_category_count; i++) {
        __atomic_store_n(&g_categories[i].total_count, 0, __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_categories[i].minute_count, 0, __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_categories[i].hour_count, 0, __ATOMIC_SEQ_CST);
        g_categories[i].rate_ema = 0.0f;
    }

    if (g_metrics_mutex) nimcp_platform_mutex_unlock(g_metrics_mutex);

    for (int i = 0; i < NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT; i++) {
        __atomic_store_n(&g_recovery[i].attempts, 0, __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_recovery[i].successes, 0, __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_recovery[i].failures, 0, __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_recovery[i].total_time_us, 0, __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_recovery[i].min_time_us, UINT64_MAX, __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_recovery[i].max_time_us, 0, __ATOMIC_SEQ_CST);
    }

    LOG_INFO("Exception metrics reset");
}

/* ============================================================================
 * Per-Category Query Implementation
 * ============================================================================ */

float nimcp_metrics_get_rate(nimcp_exception_category_t category) {
    if (!g_metrics_initialized) return 0.0f;

    if (g_metrics_mutex) nimcp_platform_mutex_lock(g_metrics_mutex);

    float rate = 0.0f;
    for (size_t i = 0; i < g_category_count; i++) {
        if (g_categories[i].category == category) {
            rate = g_categories[i].rate_ema;
            break;
        }
    }

    if (g_metrics_mutex) nimcp_platform_mutex_unlock(g_metrics_mutex);

    return rate;
}

uint64_t nimcp_metrics_get_count(
    nimcp_exception_category_t category,
    uint32_t window_seconds
) {
    if (!g_metrics_initialized) return 0;

    if (g_metrics_mutex) nimcp_platform_mutex_lock(g_metrics_mutex);

    uint64_t count = 0;
    for (size_t i = 0; i < g_category_count; i++) {
        if (g_categories[i].category == category) {
            if (window_seconds <= 60) {
                count = __atomic_load_n(&g_categories[i].minute_count, __ATOMIC_SEQ_CST);
            } else if (window_seconds <= 3600) {
                count = __atomic_load_n(&g_categories[i].hour_count, __ATOMIC_SEQ_CST);
            } else {
                count = __atomic_load_n(&g_categories[i].total_count, __ATOMIC_SEQ_CST);
            }
            break;
        }
    }

    if (g_metrics_mutex) nimcp_platform_mutex_unlock(g_metrics_mutex);

    return count;
}

float nimcp_metrics_get_recovery_rate(nimcp_exception_recovery_action_t action) {
    if (!g_metrics_initialized) return 0.0f;
    if (action < 0 || action >= NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT) return 0.0f;

    uint64_t attempts = __atomic_load_n(&g_recovery[action].attempts, __ATOMIC_SEQ_CST);
    if (attempts == 0) return 0.0f;

    uint64_t successes = __atomic_load_n(&g_recovery[action].successes, __ATOMIC_SEQ_CST);
    return (float)successes / (float)attempts;
}

float nimcp_metrics_get_mttr(nimcp_exception_recovery_action_t action) {
    if (!g_metrics_initialized) return 0.0f;
    if (action < 0 || action >= NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT) return 0.0f;

    uint64_t attempts = __atomic_load_n(&g_recovery[action].attempts, __ATOMIC_SEQ_CST);
    if (attempts == 0) return 0.0f;

    uint64_t total_time = __atomic_load_n(&g_recovery[action].total_time_us, __ATOMIC_SEQ_CST);
    return (float)total_time / (float)attempts;
}

/* ============================================================================
 * Top-N Query Implementation
 * ============================================================================ */

size_t nimcp_metrics_top_categories(
    nimcp_category_metrics_t* out,
    size_t max_count
) {
    if (!out || max_count == 0 || !g_metrics_initialized) return 0;

    if (g_metrics_mutex) nimcp_platform_mutex_lock(g_metrics_mutex);

    /* Copy and sort by total count */
    nimcp_category_metrics_t temp[NIMCP_METRICS_MAX_CATEGORIES];
    size_t count = 0;

    for (size_t i = 0; i < g_category_count && i < NIMCP_METRICS_MAX_CATEGORIES; i++) {
        temp[count].category = g_categories[i].category;
        temp[count].total_count = __atomic_load_n(&g_categories[i].total_count, __ATOMIC_SEQ_CST);
        temp[count].count_last_minute = __atomic_load_n(&g_categories[i].minute_count, __ATOMIC_SEQ_CST);
        temp[count].count_last_hour = __atomic_load_n(&g_categories[i].hour_count, __ATOMIC_SEQ_CST);
        temp[count].rate_per_second = g_categories[i].rate_ema;
        temp[count].last_occurrence_us = __atomic_load_n(&g_categories[i].last_occurrence_us, __ATOMIC_SEQ_CST);
        count++;
    }

    if (g_metrics_mutex) nimcp_platform_mutex_unlock(g_metrics_mutex);

    /* P2-U29: Guard against underflow when count==0 (count-1 wraps to SIZE_MAX) */
    if (count == 0) return 0;

    /* Simple bubble sort (categories array is small) */
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = 0; j < count - i - 1; j++) {
            if (temp[j].total_count < temp[j + 1].total_count) {
                nimcp_category_metrics_t swap = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = swap;
            }
        }
    }

    /* Copy top N to output */
    size_t result_count = (count < max_count) ? count : max_count;
    memcpy(out, temp, result_count * sizeof(nimcp_category_metrics_t));

    return result_count;
}

size_t nimcp_metrics_top_patterns(
    nimcp_adaptive_pattern_t* out,
    size_t max_count
) {
    if (!out || max_count == 0 || !g_adaptive_initialized || !g_patterns) return 0;

    if (g_adaptive_mutex) nimcp_platform_mutex_lock(g_adaptive_mutex);

    /* Collect all patterns with their total attempts */
    nimcp_adaptive_pattern_t temp[NIMCP_METRICS_MAX_PATTERNS];
    uint32_t attempt_counts[NIMCP_METRICS_MAX_PATTERNS];
    size_t count = 0;

    for (size_t i = 0; i < NIMCP_METRICS_MAX_PATTERNS && count < NIMCP_METRICS_MAX_PATTERNS; i++) {
        pattern_entry_t* entry = &g_patterns[i];
        while (entry && count < NIMCP_METRICS_MAX_PATTERNS) {
            if (entry->occupied) {
                temp[count] = entry->pattern;
                uint32_t total = 0;
                for (int j = 0; j < NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT; j++) {
                    total += entry->pattern.action_attempts[j];
                }
                attempt_counts[count] = total;
                count++;
            }
            entry = entry->next;
        }
    }

    if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);

    /* P2-U30: Guard against underflow when count==0 (count-1 wraps to SIZE_MAX) */
    if (count == 0) return 0;

    /* Sort by total attempts (descending) */
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = 0; j < count - i - 1; j++) {
            if (attempt_counts[j] < attempt_counts[j + 1]) {
                nimcp_adaptive_pattern_t swap_pat = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = swap_pat;

                uint32_t swap_count = attempt_counts[j];
                attempt_counts[j] = attempt_counts[j + 1];
                attempt_counts[j + 1] = swap_count;
            }
        }
    }

    /* Copy top N */
    size_t result_count = (count < max_count) ? count : max_count;
    memcpy(out, temp, result_count * sizeof(nimcp_adaptive_pattern_t));

    return result_count;
}

/* ============================================================================
 * Adaptive Recovery Implementation
 * ============================================================================ */

int nimcp_adaptive_init(void) {
    if (g_adaptive_initialized) return 0;

    g_adaptive_mutex = nimcp_platform_mutex_create();
    if (!g_adaptive_mutex) return -1;

    g_patterns = nimcp_calloc(NIMCP_METRICS_MAX_PATTERNS, sizeof(pattern_entry_t));
    if (!g_patterns) {
        nimcp_platform_mutex_destroy(g_adaptive_mutex);
        nimcp_free(g_adaptive_mutex);
        g_adaptive_mutex = NULL;
        return -1;
    }

    g_pattern_count = 0;
    g_suggestions_made = 0;
    g_suggestions_followed = 0;
    g_patterns_reset = 0;

    g_adaptive_initialized = true;
    LOG_INFO("Adaptive recovery initialized");

    return 0;
}

void nimcp_adaptive_shutdown(void) {
    if (!g_adaptive_initialized) return;

    if (g_adaptive_mutex) nimcp_platform_mutex_lock(g_adaptive_mutex);

    /* Free chained entries */
    if (g_patterns) {
        for (size_t i = 0; i < NIMCP_METRICS_MAX_PATTERNS; i++) {
            pattern_entry_t* entry = g_patterns[i].next;
            while (entry) {
                pattern_entry_t* next = entry->next;
                nimcp_free(entry);
                entry = next;
            }
        }
        nimcp_free(g_patterns);
        g_patterns = NULL;
    }

    if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);

    if (g_adaptive_mutex) {
        nimcp_platform_mutex_destroy(g_adaptive_mutex);
        nimcp_free(g_adaptive_mutex);
        g_adaptive_mutex = NULL;
    }

    g_adaptive_initialized = false;
    LOG_INFO("Adaptive recovery shutdown");
}

nimcp_exception_recovery_action_t nimcp_adaptive_suggest_action(nimcp_exception_t* ex) {
    if (!g_adaptive_initialized || !ex) return EXCEPTION_RECOVERY_NONE;
    if (ex->epitope_len == 0) return EXCEPTION_RECOVERY_NONE;

    if (g_adaptive_mutex) nimcp_platform_mutex_lock(g_adaptive_mutex);

    pattern_entry_t* entry = find_pattern(ex->epitope, ex->epitope_len);

    nimcp_exception_recovery_action_t suggestion = EXCEPTION_RECOVERY_NONE;
    if (entry && entry->pattern.learned) {
        suggestion = entry->pattern.preferred_action;
        __atomic_add_fetch(&g_suggestions_made, 1, __ATOMIC_SEQ_CST);
    }

    if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);

    return suggestion;
}

int nimcp_adaptive_record_outcome(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    bool success
) {
    if (!g_adaptive_initialized || !ex) return -1;
    if (ex->epitope_len == 0) return -1;
    if (action < 0 || action >= NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT) return -1;

    if (g_adaptive_mutex) nimcp_platform_mutex_lock(g_adaptive_mutex);

    pattern_entry_t* entry = find_or_create_pattern(ex->epitope, ex->epitope_len);
    if (!entry) {
        if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);
        return -1;
    }

    nimcp_adaptive_pattern_t* pattern = &entry->pattern;

    /* Update attempt count */
    pattern->action_attempts[action]++;

    /* Update success rate with EMA */
    float old_rate = pattern->action_success_rates[action];
    float new_value = success ? 1.0f : 0.0f;
    pattern->action_success_rates[action] =
        g_config.ema_alpha * new_value + (1.0f - g_config.ema_alpha) * old_rate;

    /* Update consecutive failures */
    if (success) {
        pattern->consecutive_failures = 0;
        pattern->last_success_us = get_timestamp_us();
    } else {
        pattern->consecutive_failures++;

        /* Reset pattern if too many consecutive failures */
        if (pattern->consecutive_failures >= NIMCP_METRICS_MAX_CONSECUTIVE_FAILURES) {
            memset(pattern->action_success_rates, 0, sizeof(pattern->action_success_rates));
            memset(pattern->action_attempts, 0, sizeof(pattern->action_attempts));
            pattern->consecutive_failures = 0;
            pattern->learned = false;
            pattern->preferred_action = EXCEPTION_RECOVERY_NONE;
            __atomic_add_fetch(&g_patterns_reset, 1, __ATOMIC_SEQ_CST);
            LOG_WARNING("Adaptive pattern reset due to consecutive failures");
        }
    }

    /* Check if pattern is now learned */
    if (!pattern->learned) {
        for (int i = 0; i < NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT; i++) {
            if (pattern->action_attempts[i] >= g_config.min_samples_for_learning) {
                pattern->learned = true;
                break;
            }
        }
    }

    /* Update preferred action */
    if (pattern->learned) {
        pattern->preferred_action = get_best_action(pattern);
    }

    if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);

    return 0;
}

float nimcp_adaptive_get_confidence(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action
) {
    if (!g_adaptive_initialized || !ex) return 0.0f;
    if (ex->epitope_len == 0) return 0.0f;
    if (action < 0 || action >= NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT) return 0.0f;

    if (g_adaptive_mutex) nimcp_platform_mutex_lock(g_adaptive_mutex);

    float confidence = 0.0f;
    pattern_entry_t* entry = find_pattern(ex->epitope, ex->epitope_len);

    if (entry && entry->occupied) {
        uint32_t attempts = entry->pattern.action_attempts[action];
        if (attempts >= g_config.min_samples_for_learning) {
            /* Confidence based on sample size and success rate */
            float sample_confidence = (float)attempts / (float)(attempts + 10);
            float rate = entry->pattern.action_success_rates[action];
            confidence = sample_confidence * rate;
        } else if (attempts > 0) {
            /* Lower confidence for fewer samples */
            confidence = 0.1f * (float)attempts / (float)g_config.min_samples_for_learning;
        }
    }

    if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);

    return confidence;
}

int nimcp_adaptive_force_action(
    const uint8_t* epitope,
    size_t len,
    nimcp_exception_recovery_action_t action
) {
    if (!g_adaptive_initialized || !epitope || len == 0) return -1;
    if (action < 0 || action >= NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT) return -1;

    if (g_adaptive_mutex) nimcp_platform_mutex_lock(g_adaptive_mutex);

    pattern_entry_t* entry = find_pattern(epitope, len);
    if (!entry) {
        if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);
        return -1;
    }

    entry->pattern.preferred_action = action;
    entry->pattern.learned = true;  /* Force learned state */

    if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);

    LOG_INFO("Forced action %d for pattern", action);
    return 0;
}

void nimcp_adaptive_reset_pattern(const uint8_t* epitope, size_t len) {
    if (!g_adaptive_initialized || !epitope || len == 0) return;

    if (g_adaptive_mutex) nimcp_platform_mutex_lock(g_adaptive_mutex);

    pattern_entry_t* entry = find_pattern(epitope, len);
    if (entry && entry->occupied) {
        memset(entry->pattern.action_success_rates, 0, sizeof(entry->pattern.action_success_rates));
        memset(entry->pattern.action_attempts, 0, sizeof(entry->pattern.action_attempts));
        entry->pattern.consecutive_failures = 0;
        entry->pattern.learned = false;
        entry->pattern.preferred_action = EXCEPTION_RECOVERY_NONE;
        entry->pattern.last_success_us = 0;
        __atomic_add_fetch(&g_patterns_reset, 1, __ATOMIC_SEQ_CST);
    }

    if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);
}

/* Internal helper: reset patterns without acquiring mutex (caller must hold it) */
static void adaptive_reset_all_unlocked(void) {
    if (g_patterns) {
        for (size_t i = 0; i < NIMCP_METRICS_MAX_PATTERNS; i++) {
            pattern_entry_t* entry = g_patterns[i].next;
            while (entry) {
                pattern_entry_t* next = entry->next;
                nimcp_free(entry);
                entry = next;
            }
            memset(&g_patterns[i], 0, sizeof(pattern_entry_t));
        }
    }
    g_pattern_count = 0;
}

void nimcp_adaptive_reset_all(void) {
    if (!g_adaptive_initialized) return;

    if (g_adaptive_mutex) nimcp_platform_mutex_lock(g_adaptive_mutex);

    adaptive_reset_all_unlocked();

    g_pattern_count = 0;

    if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);

    LOG_INFO("All adaptive patterns reset");
}

/* ============================================================================
 * Persistence Implementation
 * ============================================================================ */

/**
 * @brief Serialization header
 */
typedef struct {
    uint32_t magic;           /* 'NIMM' */
    uint32_t version;
    uint32_t pattern_count;
    uint32_t reserved;
} persistence_header_t;

#define PERSISTENCE_MAGIC 0x4D4D494E  /* 'NIMM' */
#define PERSISTENCE_VERSION 1

size_t nimcp_adaptive_export(uint8_t* buffer, size_t size) {
    if (!g_adaptive_initialized || !g_patterns) return 0;

    if (g_adaptive_mutex) nimcp_platform_mutex_lock(g_adaptive_mutex);

    /* Calculate required size */
    size_t required = sizeof(persistence_header_t) +
                      g_pattern_count * sizeof(nimcp_adaptive_pattern_t);

    if (!buffer || size < required) {
        if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);
        return required;
    }

    /* Write header */
    persistence_header_t* header = (persistence_header_t*)buffer;
    header->magic = PERSISTENCE_MAGIC;
    header->version = PERSISTENCE_VERSION;
    header->pattern_count = (uint32_t)g_pattern_count;
    header->reserved = 0;

    /* Write patterns */
    size_t offset = sizeof(persistence_header_t);
    for (size_t i = 0; i < NIMCP_METRICS_MAX_PATTERNS; i++) {
        pattern_entry_t* entry = &g_patterns[i];
        while (entry) {
            if (entry->occupied) {
                memcpy(buffer + offset, &entry->pattern, sizeof(nimcp_adaptive_pattern_t));
                offset += sizeof(nimcp_adaptive_pattern_t);
            }
            entry = entry->next;
        }
    }

    if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);

    return offset;
}

int nimcp_adaptive_import(const uint8_t* buffer, size_t size) {
    if (!g_adaptive_initialized || !buffer) return -1;
    if (size < sizeof(persistence_header_t)) return -1;

    const persistence_header_t* header = (const persistence_header_t*)buffer;

    /* Validate header */
    if (header->magic != PERSISTENCE_MAGIC) {
        LOG_ERROR("Invalid persistence magic: 0x%08X", header->magic);
        return -1;
    }

    if (header->version != PERSISTENCE_VERSION) {
        LOG_ERROR("Unsupported persistence version: %u", header->version);
        return -1;
    }

    size_t expected_size = sizeof(persistence_header_t) +
                           header->pattern_count * sizeof(nimcp_adaptive_pattern_t);
    if (size < expected_size) {
        LOG_ERROR("Buffer too small: %zu < %zu", size, expected_size);
        return -1;
    }

    if (g_adaptive_mutex) nimcp_platform_mutex_lock(g_adaptive_mutex);

    /* Use unlocked helper since we already hold g_adaptive_mutex */
    adaptive_reset_all_unlocked();

    /* Import patterns */
    size_t offset = sizeof(persistence_header_t);
    for (uint32_t i = 0; i < header->pattern_count; i++) {
        const nimcp_adaptive_pattern_t* src =
            (const nimcp_adaptive_pattern_t*)(buffer + offset);

        pattern_entry_t* entry = find_or_create_pattern(src->epitope, src->epitope_len);
        if (entry) {
            entry->pattern = *src;
        }

        offset += sizeof(nimcp_adaptive_pattern_t);
    }

    if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);

    LOG_INFO("Imported %u adaptive patterns", header->pattern_count);
    return 0;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

void nimcp_adaptive_get_stats(nimcp_adaptive_stats_t* stats) {
    if (!stats) return;

    memset(stats, 0, sizeof(nimcp_adaptive_stats_t));

    if (!g_adaptive_initialized) return;

    stats->total_patterns = g_pattern_count;
    stats->suggestions_made = __atomic_load_n(&g_suggestions_made, __ATOMIC_SEQ_CST);
    stats->suggestions_followed = __atomic_load_n(&g_suggestions_followed, __ATOMIC_SEQ_CST);
    stats->patterns_reset = __atomic_load_n(&g_patterns_reset, __ATOMIC_SEQ_CST);

    if (stats->suggestions_made > 0) {
        stats->suggestion_accuracy = (float)stats->suggestions_followed /
                                     (float)stats->suggestions_made;
    }

    /* Count learned patterns */
    if (g_adaptive_mutex) nimcp_platform_mutex_lock(g_adaptive_mutex);

    if (g_patterns) {
        for (size_t i = 0; i < NIMCP_METRICS_MAX_PATTERNS; i++) {
            pattern_entry_t* entry = &g_patterns[i];
            while (entry) {
                if (entry->occupied && entry->pattern.learned) {
                    stats->learned_patterns++;
                }
                entry = entry->next;
            }
        }
    }

    if (g_adaptive_mutex) nimcp_platform_mutex_unlock(g_adaptive_mutex);
}

/* ============================================================================
 * Status Queries
 * ============================================================================ */

bool nimcp_metrics_is_initialized(void) {
    return g_metrics_initialized;
}

bool nimcp_adaptive_is_initialized(void) {
    return g_adaptive_initialized;
}
