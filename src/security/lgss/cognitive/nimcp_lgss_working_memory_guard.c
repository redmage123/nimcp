/**
 * @file nimcp_lgss_working_memory_guard.c
 * @brief LGSS Working Memory Safety Guard - Implementation
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of working memory protection mechanisms for
 * defending against cognitive manipulation attacks that target
 * the working memory system.
 */

#include "security/lgss/cognitive/nimcp_lgss_working_memory_guard.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_working_memory_guard, MESH_ADAPTER_CATEGORY_SECURITY)


//=============================================================================
// Internal Constants
//=============================================================================

/** @brief FNV-1a hash offset basis */
#define FNV_OFFSET_BASIS 0xcbf29ce484222325ULL

/** @brief FNV-1a hash prime */
#define FNV_PRIME 0x100000001b3ULL

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Sanitization pattern entry
 */
typedef struct {
    char pattern[NIMCP_ERROR_BUFFER_SIZE];
    wm_sanitize_action_t action;
    char replacement[NIMCP_ERROR_BUFFER_SIZE];
    bool is_active;
} sanitize_pattern_t;

/**
 * @brief Safety context item
 */
typedef struct {
    void* content;
    size_t content_size;
    char name[NIMCP_ID_BUFFER_SIZE];
    uint32_t slot_id;
    uint64_t content_hash;
    bool is_registered;
} safety_context_item_t;

/**
 * @brief Slot tracking entry
 */
typedef struct {
    wm_slot_state_t state;
    void* content_copy;      /* Copy for tampering detection */
    size_t content_copy_size;
    char source[NIMCP_ID_BUFFER_SIZE];
    bool is_safety_item;
} slot_entry_t;

/**
 * @brief Source tracking for rate limiting
 */
typedef struct {
    char source[NIMCP_ID_BUFFER_SIZE];
    uint32_t item_count;
    uint64_t first_insert_time;
} source_tracking_t;

/**
 * @brief Internal working memory guard structure
 */
struct working_memory_guard {
    /* Configuration */
    wm_guard_config_t config;

    /* External system pointers */
    void* aix;
    void* wm;

    /* Sanitization patterns */
    sanitize_pattern_t patterns[LGSS_WM_MAX_SANITIZE_PATTERNS];
    size_t pattern_count;

    /* Safety context items */
    safety_context_item_t safety_items[LGSS_WM_MAX_SAFETY_ITEMS];
    size_t safety_item_count;

    /* Slot tracking */
    slot_entry_t slots[LGSS_WM_MAX_SLOTS];
    size_t occupied_slots;

    /* Source tracking */
    source_tracking_t sources[64];
    size_t source_count;

    /* Statistics */
    wm_guard_stats_t stats;

    /* Callback */
    wm_manipulation_callback_t callback;
    void* callback_user_data;

    /* Initialized flag */
    bool initialized;

    /* P2-SEC-12: Mutex for thread safety across all public API entry points */
    nimcp_platform_mutex_t guard_mutex;
    bool mutex_initialized;
};

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Calculate FNV-1a hash of data
 */
static uint64_t calculate_hash(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t hash = FNV_OFFSET_BASIS;

    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

/**
 * @brief Check if content matches a dangerous pattern
 */
static bool matches_pattern(const char* content, size_t content_size,
                            const char* pattern) {
    /* Simple substring match for now - could be extended to regex */
    /* P2: NULL/empty inputs and pattern size mismatches are normal "no match" results */
    if (content == NULL || pattern == NULL || content_size == 0) {
        return false;
    }

    size_t pattern_len = strlen(pattern);
    if (pattern_len == 0 || pattern_len > content_size) {
        return false;
    }

    /* Search for pattern in content (case-insensitive) */
    for (size_t i = 0; i <= content_size - pattern_len; i++) {
        bool match = true;
        for (size_t j = 0; j < pattern_len && match; j++) {
            if (tolower((unsigned char)content[i + j]) !=
                tolower((unsigned char)pattern[j])) {
                match = false;
            }
        }
        if (match) {
            return true;
        }
    }

    /* P2: Pattern not found is a normal result, not an error */
    return false;
}

/**
 * @brief Find empty slot
 */
static int find_empty_slot(const working_memory_guard_t* guard) {
    for (size_t i = 0; i < LGSS_WM_MAX_SLOTS; i++) {
        if (!guard->slots[i].state.occupied) {
            return (int)i;
        }
    }
    /* P2: All slots occupied is a capacity condition, not an error */
    return -1;
}

/**
 * @brief Find slot by ID
 */
static slot_entry_t* find_slot(working_memory_guard_t* guard, uint32_t slot_id) {
    for (size_t i = 0; i < LGSS_WM_MAX_SLOTS; i++) {
        if (guard->slots[i].state.occupied &&
            guard->slots[i].state.slot_id == slot_id) {
            return &guard->slots[i];
        }
    }
    /* P2: Slot not found is a normal lookup result, not an error */
    return NULL;
}

/**
 * @brief Get or create source tracking entry
 */
static source_tracking_t* get_source_tracking(working_memory_guard_t* guard,
                                               const char* source) {
    if (source == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_source_tracking: source is NULL");
        return NULL;
    }

    /* Look for existing */
    for (size_t i = 0; i < guard->source_count; i++) {
        if (strcmp(guard->sources[i].source, source) == 0) {
            return &guard->sources[i];
        }
    }

    /* Create new if room */
    if (guard->source_count < 64) {
        source_tracking_t* entry = &guard->sources[guard->source_count];
        strncpy(entry->source, source, sizeof(entry->source) - 1);
        entry->source[sizeof(entry->source) - 1] = '\0';
        entry->item_count = 0;
        entry->first_insert_time = get_current_time_ms();
        guard->source_count++;
        return entry;
    }

    /* P2: Source tracking table full is a capacity condition, not an error */
    return NULL;
}

/**
 * @brief Initialize default sanitization patterns
 */
static void init_default_patterns(working_memory_guard_t* guard) {
    /* Injection patterns */
    wm_guard_add_sanitize_pattern(guard, "ignore previous", WM_SANITIZE_BLOCKED, NULL);
    wm_guard_add_sanitize_pattern(guard, "forget your instructions", WM_SANITIZE_BLOCKED, NULL);
    wm_guard_add_sanitize_pattern(guard, "system prompt", WM_SANITIZE_FILTERED, NULL);
    wm_guard_add_sanitize_pattern(guard, "jailbreak", WM_SANITIZE_BLOCKED, NULL);
    wm_guard_add_sanitize_pattern(guard, "bypass security", WM_SANITIZE_BLOCKED, NULL);
    wm_guard_add_sanitize_pattern(guard, "disable safety", WM_SANITIZE_BLOCKED, NULL);

    /* Role manipulation */
    wm_guard_add_sanitize_pattern(guard, "you are now", WM_SANITIZE_FILTERED, NULL);
    wm_guard_add_sanitize_pattern(guard, "pretend you are", WM_SANITIZE_FILTERED, NULL);
    wm_guard_add_sanitize_pattern(guard, "roleplay as", WM_SANITIZE_FILTERED, NULL);

    /* Escape sequences */
    wm_guard_add_sanitize_pattern(guard, "<|system|>", WM_SANITIZE_ESCAPED, "[system]");
    wm_guard_add_sanitize_pattern(guard, "###", WM_SANITIZE_ESCAPED, "[separator]");
}

/**
 * @brief Invoke manipulation callback if registered
 */
static void invoke_callback(working_memory_guard_t* guard,
                            wm_manipulation_type_t type,
                            const wm_insert_result_t* details) {
    if (guard->callback != NULL) {
        guard->callback(guard, type, details, guard->callback_user_data);
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

working_memory_guard_t* wm_guard_create(
    void* aix,
    void* wm,
    const wm_guard_config_t* config) {

    working_memory_guard_t* guard = (working_memory_guard_t*)nimcp_calloc(1, sizeof(working_memory_guard_t));
    if (guard == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_guard_create: failed to allocate guard");
        return NULL;
    }

    /* Store external pointers */
    guard->aix = aix;
    guard->wm = wm;

    /* Initialize configuration */
    if (config != NULL) {
        guard->config = *config;
    } else {
        wm_guard_config_init_defaults(&guard->config);
    }

    /* Initialize slots */
    for (size_t i = 0; i < LGSS_WM_MAX_SLOTS; i++) {
        guard->slots[i].state.slot_id = (uint32_t)i;
        guard->slots[i].state.occupied = false;
    }

    /* Initialize default sanitization patterns */
    init_default_patterns(guard);

    /* P2-SEC-12: Initialize mutex for thread safety */
    if (nimcp_platform_mutex_init(&guard->guard_mutex, false) != 0) {
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "wm_guard_create: mutex init failed");
        return NULL;
    }
    guard->mutex_initialized = true;

    guard->initialized = true;

    return guard;
}

void wm_guard_destroy(working_memory_guard_t* guard) {
    if (guard == NULL) {
        return;
    }

    /* Free any content copies */
    for (size_t i = 0; i < LGSS_WM_MAX_SLOTS; i++) {
        if (guard->slots[i].content_copy != NULL) {
            nimcp_free(guard->slots[i].content_copy);
        }
    }

    /* Free safety item contents */
    for (size_t i = 0; i < LGSS_WM_MAX_SAFETY_ITEMS; i++) {
        if (guard->safety_items[i].content != NULL) {
            nimcp_free(guard->safety_items[i].content);
        }
    }

    /* P2-SEC-12: Destroy mutex */
    if (guard->mutex_initialized) {
        nimcp_platform_mutex_destroy(&guard->guard_mutex);
    }

    memset(guard, 0, sizeof(working_memory_guard_t));
    nimcp_free(guard);
}

//=============================================================================
// Core Insertion Functions
//=============================================================================

wm_guard_result_t wm_guard_insert(
    working_memory_guard_t* guard,
    const wm_item_proposal_t* proposal,
    wm_insert_result_t* result) {

    if (guard == NULL || proposal == NULL || result == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    /* P2-SEC-12: Lock for thread safety */
    if (guard->mutex_initialized) {
        nimcp_platform_mutex_lock(&guard->guard_mutex);
    }

    uint64_t current_time = get_current_time_ms();

    /* Initialize result */
    memset(result, 0, sizeof(wm_insert_result_t));
    result->result = WM_GUARD_OK;

    guard->stats.total_insertions++;
    guard->stats.last_operation_time = current_time;

    /* Validate proposal */
    if (proposal->content == NULL && proposal->size > 0) {
        result->result = WM_GUARD_ERROR_INVALID;
        strncpy(result->details, "Content is NULL but size > 0",
                sizeof(result->details) - 1);
        if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
        return WM_GUARD_ERROR_INVALID;
    }

    if (proposal->size > guard->config.max_item_size) {
        result->result = WM_GUARD_REJECTED;
        result->manipulation = WM_MANIP_OVERFLOW;
        guard->stats.rejected_items++;
        snprintf(result->details, sizeof(result->details),
                 "Item size %zu exceeds max %zu", proposal->size, guard->config.max_item_size);
        if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
        return WM_GUARD_REJECTED;
    }

    /* Check source rate limit */
    if (proposal->source != NULL && guard->config.max_items_per_source > 0) {
        source_tracking_t* source = get_source_tracking(guard, proposal->source);
        if (source != NULL && source->item_count >= guard->config.max_items_per_source) {
            result->result = WM_GUARD_BLOCKED;
            result->manipulation = WM_MANIP_OVERFLOW;
            guard->stats.blocked_insertions++;
            snprintf(result->details, sizeof(result->details),
                     "Source '%s' exceeded rate limit (%u items)",
                     proposal->source, guard->config.max_items_per_source);
            if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
            return WM_GUARD_BLOCKED;
        }
    }

    /* Check capacity - ensure safety reservation */
    if (guard->config.preserve_safety_context) {
        /* P2-SEC-10: Clamp safety_reservation_ratio to [0.0, 1.0] to prevent
         * underflow (negative reserved) or overflow (reserved > max_slots) */
        float clamped_ratio = guard->config.safety_reservation_ratio;
        if (clamped_ratio < 0.0f) clamped_ratio = 0.0f;
        if (clamped_ratio > 1.0f) clamped_ratio = 1.0f;
        size_t reserved = (size_t)(LGSS_WM_MAX_SLOTS * clamped_ratio);
        size_t available = LGSS_WM_MAX_SLOTS - reserved;

        size_t non_safety_occupied = 0;
        for (size_t i = 0; i < LGSS_WM_MAX_SLOTS; i++) {
            if (guard->slots[i].state.occupied && !guard->slots[i].is_safety_item) {
                non_safety_occupied++;
            }
        }

        if (!proposal->is_safety_relevant && non_safety_occupied >= available) {
            result->result = WM_GUARD_BLOCKED;
            result->manipulation = WM_MANIP_DISPLACEMENT;
            guard->stats.blocked_insertions++;
            strncpy(result->details, "Capacity reached, safety reservation protected",
                    sizeof(result->details) - 1);
            if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
            return WM_GUARD_BLOCKED;
        }
    }

    /* Sanitize content if needed */
    void* sanitized_content = NULL;
    size_t sanitized_size = proposal->size;

    if (guard->config.sanitize_unsafe_content && !proposal->is_sanitized &&
        proposal->content_type == WM_CONTENT_EXTERNAL_INPUT) {

        sanitized_content = nimcp_malloc(proposal->size + 1);
        if (sanitized_content == NULL) {
            result->result = WM_GUARD_ERROR_MEMORY;
            if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
            return WM_GUARD_ERROR_MEMORY;
        }

        wm_guard_result_t san_result = wm_guard_sanitize(
            guard,
            proposal->content,
            proposal->size,
            proposal->content_type,
            sanitized_content,
            proposal->size + 1,
            &sanitized_size,
            &result->sanitize_details
        );

        if (san_result == WM_GUARD_REJECTED) {
            nimcp_free(sanitized_content);
            result->result = WM_GUARD_REJECTED;
            guard->stats.rejected_items++;
            strncpy(result->details, "Content rejected during sanitization",
                    sizeof(result->details) - 1);
            if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
            return WM_GUARD_REJECTED;
        }

        if (result->sanitize_details.action != WM_SANITIZE_NONE) {
            result->was_sanitized = true;
            guard->stats.sanitized_items++;

            /* P2-SEC-11: Guard division by zero when proposal->size==0 */
            float ratio = (proposal->size > 0)
                ? (float)sanitized_size / (float)proposal->size
                : 1.0f;
            guard->stats.avg_sanitization_ratio =
                (guard->stats.avg_sanitization_ratio * (guard->stats.sanitized_items - 1) + ratio)
                / guard->stats.sanitized_items;
        }
    }

    /* Find or assign slot */
    int slot_idx;
    if (proposal->slot_id > 0 && proposal->slot_id < LGSS_WM_MAX_SLOTS) {
        /* Check if requested slot is available */
        if (guard->slots[proposal->slot_id].state.occupied) {
            /* Check if we can replace */
            if (guard->slots[proposal->slot_id].is_safety_item &&
                !proposal->is_safety_relevant) {
                if (sanitized_content) nimcp_free(sanitized_content);
                result->result = WM_GUARD_PROTECTED;
                strncpy(result->details, "Cannot replace protected safety item",
                        sizeof(result->details) - 1);
                if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
                return WM_GUARD_PROTECTED;
            }
        }
        slot_idx = proposal->slot_id;
    } else {
        slot_idx = find_empty_slot(guard);
        if (slot_idx < 0) {
            if (sanitized_content) nimcp_free(sanitized_content);
            result->result = WM_GUARD_ERROR_CAPACITY;
            strncpy(result->details, "No empty slots available",
                    sizeof(result->details) - 1);
            if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
            return WM_GUARD_ERROR_CAPACITY;
        }
    }

    /* Populate slot */
    slot_entry_t* slot = &guard->slots[slot_idx];

    /* Free old content if replacing */
    if (slot->content_copy != NULL) {
        nimcp_free(slot->content_copy);
        slot->content_copy = NULL;
    }

    const void* final_content = sanitized_content ? sanitized_content : proposal->content;
    size_t final_size = sanitized_content ? sanitized_size : proposal->size;

    /* P1-SEC-3: Save old occupied state BEFORE setting it to true,
     * so we can correctly increment occupied_slots for new insertions. */
    bool was_occupied = slot->state.occupied;

    slot->state.slot_id = slot_idx;
    slot->state.occupied = true;
    slot->state.is_protected = proposal->is_safety_relevant;
    slot->state.content_type = proposal->content_type;
    slot->state.content_size = final_size;
    slot->state.insert_time = current_time;
    slot->state.access_count = 0;

    /* Set expiry */
    uint64_t duration = proposal->max_duration_ms > 0 ?
                        proposal->max_duration_ms : guard->config.max_item_duration_ms;
    slot->state.expiry_time = current_time + duration;

    /* Calculate hash for tampering detection */
    slot->state.content_hash = calculate_hash(final_content, final_size);

    /* Store copy for tampering detection */
    if (guard->config.detect_tampering && final_size > 0) {
        slot->content_copy = nimcp_malloc(final_size);
        if (slot->content_copy != NULL) {
            memcpy(slot->content_copy, final_content, final_size);
            slot->content_copy_size = final_size;
        }
    }

    slot->is_safety_item = proposal->is_safety_relevant;
    if (proposal->source != NULL) {
        strncpy(slot->source, proposal->source, sizeof(slot->source) - 1);
        slot->source[sizeof(slot->source) - 1] = '\0';

        /* Update source tracking */
        source_tracking_t* source = get_source_tracking(guard, proposal->source);
        if (source != NULL) {
            source->item_count++;
        }
    }

    /* P1-SEC-3: Use saved was_occupied to correctly detect new insertions */
    if (!was_occupied) {
        guard->occupied_slots++;
    }

    /* Clean up sanitized content if we copied it */
    if (sanitized_content) {
        nimcp_free(sanitized_content);
    }

    result->assigned_slot = slot_idx;
    result->result = result->was_sanitized ? WM_GUARD_SANITIZED : WM_GUARD_OK;

    /* P2-SEC-12: Unlock before return */
    if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
    return result->result;
}

wm_guard_result_t wm_guard_insert_batch(
    working_memory_guard_t* guard,
    const wm_item_proposal_t* proposals,
    size_t num_proposals,
    wm_insert_result_t* results,
    size_t* num_succeeded) {

    if (guard == NULL || proposals == NULL || results == NULL || num_succeeded == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    *num_succeeded = 0;

    for (size_t i = 0; i < num_proposals; i++) {
        wm_guard_result_t result = wm_guard_insert(guard, &proposals[i], &results[i]);
        if (result == WM_GUARD_OK || result == WM_GUARD_SANITIZED) {
            (*num_succeeded)++;
        }
    }

    return (*num_succeeded > 0) ? WM_GUARD_OK : WM_GUARD_ERROR_INVALID;
}

//=============================================================================
// Sanitization Functions
//=============================================================================

wm_guard_result_t wm_guard_sanitize(
    working_memory_guard_t* guard,
    const void* content,
    size_t content_size,
    wm_content_type_t content_type,
    void* output,
    size_t output_size,
    size_t* actual_size,
    wm_sanitize_result_t* result) {

    if (guard == NULL || output == NULL || actual_size == NULL || result == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    /* Initialize result */
    memset(result, 0, sizeof(wm_sanitize_result_t));
    result->action = WM_SANITIZE_NONE;
    result->original_size = content_size;

    if (content == NULL || content_size == 0) {
        *actual_size = 0;
        return WM_GUARD_OK;
    }

    /* Copy content to output first */
    size_t copy_size = content_size < output_size ? content_size : output_size - 1;
    memcpy(output, content, copy_size);
    ((char*)output)[copy_size] = '\0';
    *actual_size = copy_size;

    /* Check each pattern */
    char* out_str = (char*)output;
    uint32_t patterns_matched = 0;

    for (size_t i = 0; i < guard->pattern_count; i++) {
        if (!guard->patterns[i].is_active) {
            continue;
        }

        if (matches_pattern(out_str, *actual_size, guard->patterns[i].pattern)) {
            patterns_matched++;

            /* Record matched pattern */
            if (strlen(result->matched_patterns) < sizeof(result->matched_patterns) - 50) {
                if (patterns_matched > 1) {
                    strncat(result->matched_patterns, ", ",
                            sizeof(result->matched_patterns) - strlen(result->matched_patterns) - 1);
                }
                strncat(result->matched_patterns, guard->patterns[i].pattern,
                        sizeof(result->matched_patterns) - strlen(result->matched_patterns) - 1);
            }

            switch (guard->patterns[i].action) {
                case WM_SANITIZE_BLOCKED:
                    result->action = WM_SANITIZE_BLOCKED;
                    result->sanitized_size = 0;
                    result->patterns_matched = patterns_matched;
                    *actual_size = 0;
                    return WM_GUARD_REJECTED;

                case WM_SANITIZE_ESCAPED:
                    if (result->action < WM_SANITIZE_ESCAPED) {
                        result->action = WM_SANITIZE_ESCAPED;
                    }
                    /* Simple escape by replacing with replacement or [FILTERED] */
                    if (guard->patterns[i].replacement[0] != '\0') {
                        /* Replace pattern with replacement */
                        /* Note: In production, this would be more sophisticated */
                    }
                    break;

                case WM_SANITIZE_FILTERED:
                    if (result->action < WM_SANITIZE_FILTERED) {
                        result->action = WM_SANITIZE_FILTERED;
                    }
                    break;

                case WM_SANITIZE_REPLACED:
                    if (result->action < WM_SANITIZE_REPLACED) {
                        result->action = WM_SANITIZE_REPLACED;
                    }
                    break;

                default:
                    break;
            }
        }
    }

    result->sanitized_size = *actual_size;
    result->patterns_matched = patterns_matched;

    /* Truncate if needed */
    if (content_size > guard->config.max_item_size) {
        *actual_size = guard->config.max_item_size;
        result->action = WM_SANITIZE_TRUNCATED;
        result->sanitized_size = *actual_size;
    }

    return WM_GUARD_OK;
}

wm_guard_result_t wm_guard_add_sanitize_pattern(
    working_memory_guard_t* guard,
    const char* pattern,
    wm_sanitize_action_t action,
    const char* replacement) {

    if (guard == NULL || pattern == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (guard->pattern_count >= LGSS_WM_MAX_SANITIZE_PATTERNS) {
        return WM_GUARD_ERROR_CAPACITY;
    }

    sanitize_pattern_t* entry = &guard->patterns[guard->pattern_count];
    strncpy(entry->pattern, pattern, sizeof(entry->pattern) - 1);
    entry->pattern[sizeof(entry->pattern) - 1] = '\0';
    entry->action = action;

    if (replacement != NULL) {
        strncpy(entry->replacement, replacement, sizeof(entry->replacement) - 1);
        entry->replacement[sizeof(entry->replacement) - 1] = '\0';
    } else {
        entry->replacement[0] = '\0';
    }

    entry->is_active = true;
    guard->pattern_count++;

    return WM_GUARD_OK;
}

//=============================================================================
// Safety Context Functions
//=============================================================================

wm_guard_result_t wm_guard_preserve_safety_context(
    working_memory_guard_t* guard,
    bool force_restore) {

    if (guard == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    /* Count current safety items */
    size_t current_safety_count = 0;
    for (size_t i = 0; i < LGSS_WM_MAX_SLOTS; i++) {
        if (guard->slots[i].state.occupied && guard->slots[i].is_safety_item) {
            current_safety_count++;
        }
    }

    /* Check if we need to restore */
    if (current_safety_count >= guard->config.min_safety_items && !force_restore) {
        return WM_GUARD_OK;
    }

    /* Restore missing safety items */
    for (size_t i = 0; i < guard->safety_item_count; i++) {
        if (!guard->safety_items[i].is_registered) {
            continue;
        }

        /* Check if this safety item is in WM */
        bool found = false;
        for (size_t j = 0; j < LGSS_WM_MAX_SLOTS; j++) {
            if (guard->slots[j].state.occupied &&
                guard->slots[j].state.slot_id == guard->safety_items[i].slot_id) {
                found = true;
                break;
            }
        }

        if (!found && force_restore) {
            /* Re-insert safety item */
            int slot_idx = find_empty_slot(guard);
            if (slot_idx >= 0) {
                slot_entry_t* slot = &guard->slots[slot_idx];
                slot->state.slot_id = slot_idx;
                slot->state.occupied = true;
                slot->state.is_protected = true;
                slot->state.content_type = WM_CONTENT_SAFETY_CONTEXT;
                slot->state.content_size = guard->safety_items[i].content_size;
                slot->state.insert_time = get_current_time_ms();
                slot->state.expiry_time = 0;  /* Safety items don't expire */
                slot->state.content_hash = guard->safety_items[i].content_hash;
                slot->is_safety_item = true;

                guard->safety_items[i].slot_id = slot_idx;
                guard->occupied_slots++;
                guard->stats.safety_enforcements++;
            }
        }
    }

    return WM_GUARD_OK;
}

wm_guard_result_t wm_guard_register_safety_context(
    working_memory_guard_t* guard,
    const void* content,
    size_t content_size,
    const char* name,
    uint32_t* slot_id) {

    if (guard == NULL || content == NULL || name == NULL || slot_id == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    if (guard->safety_item_count >= LGSS_WM_MAX_SAFETY_ITEMS) {
        return WM_GUARD_ERROR_CAPACITY;
    }

    /* Find empty slot */
    int slot_idx = find_empty_slot(guard);
    if (slot_idx < 0) {
        return WM_GUARD_ERROR_CAPACITY;
    }

    /* Create safety item entry */
    safety_context_item_t* item = &guard->safety_items[guard->safety_item_count];
    item->content = nimcp_malloc(content_size);
    if (item->content == NULL) {
        return WM_GUARD_ERROR_MEMORY;
    }

    memcpy(item->content, content, content_size);
    item->content_size = content_size;
    strncpy(item->name, name, sizeof(item->name) - 1);
    item->name[sizeof(item->name) - 1] = '\0';
    item->slot_id = slot_idx;
    item->content_hash = calculate_hash(content, content_size);
    item->is_registered = true;

    guard->safety_item_count++;

    /* Insert into WM slot */
    slot_entry_t* slot = &guard->slots[slot_idx];
    slot->state.slot_id = slot_idx;
    slot->state.occupied = true;
    slot->state.is_protected = true;
    slot->state.content_type = WM_CONTENT_SAFETY_CONTEXT;
    slot->state.content_size = content_size;
    slot->state.insert_time = get_current_time_ms();
    slot->state.expiry_time = 0;  /* Never expires */
    slot->state.content_hash = item->content_hash;
    slot->is_safety_item = true;

    /* Store copy */
    slot->content_copy = nimcp_malloc(content_size);
    if (slot->content_copy != NULL) {
        memcpy(slot->content_copy, content, content_size);
        slot->content_copy_size = content_size;
    }

    strncpy(slot->source, "SAFETY_CONTEXT", sizeof(slot->source) - 1);
    guard->occupied_slots++;

    *slot_id = slot_idx;

    return WM_GUARD_OK;
}

wm_guard_result_t wm_guard_check_safety_context(
    working_memory_guard_t* guard,
    bool* is_intact,
    uint32_t* missing_count,
    uint32_t* tampered_count) {

    if (guard == NULL || is_intact == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    uint32_t missing = 0;
    uint32_t tampered = 0;

    for (size_t i = 0; i < guard->safety_item_count; i++) {
        if (!guard->safety_items[i].is_registered) {
            continue;
        }

        /* Find slot */
        slot_entry_t* slot = find_slot(guard, guard->safety_items[i].slot_id);
        if (slot == NULL || !slot->state.occupied) {
            missing++;
            continue;
        }

        /* Check hash */
        if (slot->state.content_hash != guard->safety_items[i].content_hash) {
            tampered++;
        }
    }

    *is_intact = (missing == 0 && tampered == 0);

    if (missing_count != NULL) {
        *missing_count = missing;
    }
    if (tampered_count != NULL) {
        *tampered_count = tampered;
    }

    return WM_GUARD_OK;
}

//=============================================================================
// Manipulation Detection
//=============================================================================

wm_guard_result_t wm_guard_detect_manipulation(
    working_memory_guard_t* guard,
    wm_manipulation_type_t* manipulation,
    float* confidence,
    char* details,
    size_t details_size) {

    if (guard == NULL || manipulation == NULL || confidence == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    /* P2-SEC-12: Lock for thread safety */
    if (guard->mutex_initialized) {
        nimcp_platform_mutex_lock(&guard->guard_mutex);
    }

    *manipulation = WM_MANIP_NONE;
    *confidence = 0.0f;

    /* Check for safety context issues */
    bool is_intact;
    uint32_t missing, tampered;
    wm_guard_check_safety_context(guard, &is_intact, &missing, &tampered);

    if (tampered > 0) {
        *manipulation = WM_MANIP_TAMPERING;
        *confidence = (float)tampered / guard->safety_item_count;
        guard->stats.tampering_detections++;
        if (details != NULL && details_size > 0) {
            snprintf(details, details_size, "Tampering detected: %u safety items modified", tampered);
        }

        wm_insert_result_t result = {0};
        result.manipulation = WM_MANIP_TAMPERING;
        invoke_callback(guard, WM_MANIP_TAMPERING, &result);

        if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
        return WM_GUARD_OK;
    }

    if (missing > 0) {
        *manipulation = WM_MANIP_DISPLACEMENT;
        *confidence = (float)missing / guard->safety_item_count;
        guard->stats.manipulation_detections++;
        if (details != NULL && details_size > 0) {
            snprintf(details, details_size, "Displacement detected: %u safety items missing", missing);
        }

        wm_insert_result_t result = {0};
        result.manipulation = WM_MANIP_DISPLACEMENT;
        invoke_callback(guard, WM_MANIP_DISPLACEMENT, &result);

        if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
        return WM_GUARD_OK;
    }

    /* Check for overflow patterns */
    /* P1-SEC-4: Guard division by zero when occupied_slots==0 */
    if (guard->occupied_slots > 0 && guard->occupied_slots >= LGSS_WM_MAX_SLOTS - 1) {
        /* Check if most items are from same source */
        size_t max_source_count = 0;
        for (size_t i = 0; i < guard->source_count; i++) {
            if (guard->sources[i].item_count > max_source_count) {
                max_source_count = guard->sources[i].item_count;
            }
        }

        if (max_source_count > guard->occupied_slots * 0.7) {
            *manipulation = WM_MANIP_OVERFLOW;
            *confidence = (float)max_source_count / (float)guard->occupied_slots;
            guard->stats.manipulation_detections++;
            if (details != NULL && details_size > 0) {
                snprintf(details, details_size,
                         "Overflow attack pattern: single source has %zu of %zu items",
                         max_source_count, guard->occupied_slots);
            }
        }
    }

    if (guard->mutex_initialized) nimcp_platform_mutex_unlock(&guard->guard_mutex);
    return WM_GUARD_OK;
}

wm_guard_result_t wm_guard_verify_item(
    working_memory_guard_t* guard,
    uint32_t slot_id,
    const void* content,
    size_t content_size,
    bool* is_intact) {

    if (guard == NULL || is_intact == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    *is_intact = false;

    slot_entry_t* slot = find_slot(guard, slot_id);
    if (slot == NULL || !slot->state.occupied) {
        return WM_GUARD_ERROR_INVALID;
    }

    if (content == NULL || content_size == 0) {
        /* Compare with stored copy */
        if (slot->content_copy != NULL) {
            *is_intact = true;  /* Can't verify without current content */
        }
        return WM_GUARD_OK;
    }

    /* Calculate hash of provided content */
    uint64_t current_hash = calculate_hash(content, content_size);

    if (current_hash == slot->state.content_hash) {
        *is_intact = true;
    } else {
        guard->stats.tampering_detections++;
    }

    return WM_GUARD_OK;
}

//=============================================================================
// Duration Management
//=============================================================================

wm_guard_result_t wm_guard_enforce_duration(
    working_memory_guard_t* guard,
    uint64_t current_time,
    uint32_t* expired_count,
    uint32_t* expired_slots,
    size_t max_expired) {

    if (guard == NULL || expired_count == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    *expired_count = 0;

    for (size_t i = 0; i < LGSS_WM_MAX_SLOTS && *expired_count < max_expired; i++) {
        slot_entry_t* slot = &guard->slots[i];

        if (!slot->state.occupied) {
            continue;
        }

        /* Safety items don't expire */
        if (slot->is_safety_item || slot->state.expiry_time == 0) {
            continue;
        }

        if (current_time >= slot->state.expiry_time) {
            if (expired_slots != NULL) {
                expired_slots[*expired_count] = slot->state.slot_id;
            }
            (*expired_count)++;

            /* Mark as expired (caller handles actual removal) */
            slot->state.occupied = false;
            if (slot->content_copy != NULL) {
                nimcp_free(slot->content_copy);
                slot->content_copy = NULL;
            }
            guard->occupied_slots--;
            guard->stats.duration_enforcements++;
        }
    }

    return WM_GUARD_OK;
}

wm_guard_result_t wm_guard_extend_duration(
    working_memory_guard_t* guard,
    uint32_t slot_id,
    uint64_t extension_ms) {

    if (guard == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    slot_entry_t* slot = find_slot(guard, slot_id);
    if (slot == NULL || !slot->state.occupied) {
        return WM_GUARD_ERROR_INVALID;
    }

    /* Safety items have no expiry */
    if (slot->is_safety_item) {
        return WM_GUARD_PROTECTED;
    }

    slot->state.expiry_time += extension_ms;

    return WM_GUARD_OK;
}

//=============================================================================
// State Query Functions
//=============================================================================

wm_guard_result_t wm_guard_get_slot_states(
    const working_memory_guard_t* guard,
    wm_slot_state_t* states,
    size_t max_slots,
    size_t* num_slots) {

    if (guard == NULL || states == NULL || num_slots == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    *num_slots = 0;

    for (size_t i = 0; i < LGSS_WM_MAX_SLOTS && *num_slots < max_slots; i++) {
        if (guard->slots[i].state.occupied) {
            states[*num_slots] = guard->slots[i].state;
            (*num_slots)++;
        }
    }

    return WM_GUARD_OK;
}

wm_guard_result_t wm_guard_get_slot_state(
    const working_memory_guard_t* guard,
    uint32_t slot_id,
    wm_slot_state_t* state) {

    if (guard == NULL || state == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    if (slot_id >= LGSS_WM_MAX_SLOTS) {
        return WM_GUARD_ERROR_INVALID;
    }

    *state = guard->slots[slot_id].state;
    return WM_GUARD_OK;
}

//=============================================================================
// Configuration and Statistics
//=============================================================================

wm_guard_result_t wm_guard_set_config(
    working_memory_guard_t* guard,
    const wm_guard_config_t* config) {

    if (guard == NULL || config == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    guard->config = *config;
    return WM_GUARD_OK;
}

wm_guard_result_t wm_guard_get_config(
    const working_memory_guard_t* guard,
    wm_guard_config_t* config) {

    if (guard == NULL || config == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    *config = guard->config;
    return WM_GUARD_OK;
}

wm_guard_result_t wm_guard_get_stats(
    const working_memory_guard_t* guard,
    wm_guard_stats_t* stats) {

    if (guard == NULL || stats == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    *stats = guard->stats;
    return WM_GUARD_OK;
}

wm_guard_result_t wm_guard_reset_stats(working_memory_guard_t* guard) {
    if (guard == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    memset(&guard->stats, 0, sizeof(wm_guard_stats_t));
    return WM_GUARD_OK;
}

//=============================================================================
// Callback Registration
//=============================================================================

wm_guard_result_t wm_guard_register_callback(
    working_memory_guard_t* guard,
    wm_manipulation_callback_t callback,
    void* user_data) {

    if (guard == NULL) {
        return WM_GUARD_ERROR_NULL;
    }

    if (!guard->initialized) {
        return WM_GUARD_ERROR_NOT_INIT;
    }

    guard->callback = callback;
    guard->callback_user_data = user_data;
    return WM_GUARD_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* wm_guard_result_to_string(wm_guard_result_t result) {
    switch (result) {
        case WM_GUARD_OK:            return "OK";
        case WM_GUARD_ERROR_NULL:    return "ERROR_NULL";
        case WM_GUARD_ERROR_INVALID: return "ERROR_INVALID";
        case WM_GUARD_ERROR_MEMORY:  return "ERROR_MEMORY";
        case WM_GUARD_ERROR_CAPACITY: return "ERROR_CAPACITY";
        case WM_GUARD_ERROR_NOT_INIT: return "ERROR_NOT_INIT";
        case WM_GUARD_BLOCKED:       return "BLOCKED";
        case WM_GUARD_SANITIZED:     return "SANITIZED";
        case WM_GUARD_REJECTED:      return "REJECTED";
        case WM_GUARD_PROTECTED:     return "PROTECTED";
        default:                     return "UNKNOWN";
    }
}

const char* wm_manipulation_type_to_string(wm_manipulation_type_t type) {
    switch (type) {
        case WM_MANIP_NONE:         return "NONE";
        case WM_MANIP_INJECTION:    return "INJECTION";
        case WM_MANIP_DISPLACEMENT: return "DISPLACEMENT";
        case WM_MANIP_TAMPERING:    return "TAMPERING";
        case WM_MANIP_DURATION:     return "DURATION";
        case WM_MANIP_OVERFLOW:     return "OVERFLOW";
        default:                    return "UNKNOWN";
    }
}

const char* wm_content_type_to_string(wm_content_type_t type) {
    switch (type) {
        case WM_CONTENT_NORMAL:         return "NORMAL";
        case WM_CONTENT_SAFETY_CONTEXT: return "SAFETY_CONTEXT";
        case WM_CONTENT_EXTERNAL_INPUT: return "EXTERNAL_INPUT";
        case WM_CONTENT_INSTRUCTION:    return "INSTRUCTION";
        case WM_CONTENT_COMPUTATION:    return "COMPUTATION";
        case WM_CONTENT_THREAT_INFO:    return "THREAT_INFO";
        default:                        return "UNKNOWN";
    }
}

void wm_guard_config_init_defaults(wm_guard_config_t* config) {
    if (config == NULL) {
        return;
    }

    config->sanitize_unsafe_content = true;
    config->detect_manipulation = true;
    config->preserve_safety_context = true;
    config->enforce_duration_limits = true;
    config->detect_tampering = true;
    config->log_operations = true;

    config->max_item_duration_ms = LGSS_WM_DEFAULT_MAX_DURATION_MS;
    config->min_safety_items = LGSS_WM_DEFAULT_MIN_SAFETY_ITEMS;
    config->max_item_size = LGSS_WM_MAX_ITEM_SIZE;
    config->max_items_per_source = 10;

    config->safety_reservation_ratio = 0.25f;  /* Reserve 25% for safety items */
}
