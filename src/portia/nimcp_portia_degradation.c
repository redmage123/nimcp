/**
 * @file nimcp_portia_degradation.c
 * @brief Implementation of graceful degradation profiles
 */

#include "portia/nimcp_portia_degradation.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "portia_degradation"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_degradation)

#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "utils/platform/nimcp_platform_once.h"
#include "utils/thread/nimcp_thread.h"

/**
 * Internal context for degradation management
 */
typedef struct {
    degradation_internal_config_t config;
    bool initialized;
} degradation_context_t;

static degradation_context_t g_degradation_ctx = {0};
static nimcp_mutex_t* g_degradation_mutex = NULL;
static nimcp_platform_once_t g_degradation_once = NIMCP_PLATFORM_ONCE_INIT;

/**
 * @brief Initialize the degradation context mutex (called once)
 */
static void init_degradation_mutex(void) {
    g_degradation_mutex = nimcp_mutex_create(NULL);
}

/**
 * Default feature definitions
 */
static const degradation_feature_t DEFAULT_FEATURES[] = {
    {FEATURE_LOGGING_VERBOSE, "Verbose Logging", DEGRADATION_LEVEL_MINOR, 0.05F, false, true},
    {FEATURE_METRICS, "Metrics Collection", DEGRADATION_LEVEL_MINOR, 0.08F, false, true},
    {FEATURE_LEARNING, "Learning", DEGRADATION_LEVEL_MODERATE, 0.15F, false, true},
    {FEATURE_PLASTICITY, "Plasticity", DEGRADATION_LEVEL_MODERATE, 0.15F, false, true},
    {FEATURE_MEMORY_LONG, "Long-term Memory", DEGRADATION_LEVEL_MODERATE, 0.12F, false, true},
    {FEATURE_EMOTIONS, "Emotions", DEGRADATION_LEVEL_SEVERE, 0.10F, false, true},
    {FEATURE_PLANNING, "Planning", DEGRADATION_LEVEL_SEVERE, 0.18F, false, true},
    {FEATURE_SENSORS_FULL, "Full Sensors", DEGRADATION_LEVEL_SEVERE, 0.20F, false, true},
    {FEATURE_COMMUNICATION, "Communication", DEGRADATION_LEVEL_CRITICAL, 0.15F, false, true},
    {FEATURE_MEMORY_WORKING, "Working Memory", DEGRADATION_LEVEL_CRITICAL, 0.25F, true, true}
};

#define DEFAULT_FEATURE_COUNT (sizeof(DEFAULT_FEATURES) / sizeof(DEFAULT_FEATURES[0]))

/**
 * Broadcast degradation event (log only for now)
 *
 * NOTE: Bio-async event broadcasting will be implemented when the
 * bio-async API stabilizes with a clear context type.
 */
static void broadcast_degradation_event(
    void* bio_ctx,
    const degradation_event_t* event
) {
    // Suppress unused parameter warnings
    (void)bio_ctx;

    if (!event) {
        return;
    }

    // Log the event
    const char* event_types[] = {
        "LEVEL_CHANGE",
        "FEATURE_DISABLED",
        "FEATURE_ENABLED",
        "RESOURCE_WARNING"
    };

    const char* event_type_str = (event->type < 4) ? event_types[event->type] : "UNKNOWN";

    LOG_INFO("Degradation Event: %s (old=%d, new=%d, feature=0x%04x, usage=%.1f%%, reason=%s)",
             event_type_str,
             event->old_level,
             event->new_level,
             event->feature_id,
             event->resource_usage,
             event->reason ? event->reason : "none");

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "event",
                  "type=%s feat=0x%04x usage=%.1f%%",
                  event_type_str, event->feature_id, event->resource_usage);
}

/**
 * Compare features by disable_at level (for sorting)
 */
static int compare_features_by_level(const void* a, const void* b) {
    const degradation_feature_t* fa = (const degradation_feature_t*)a;
    const degradation_feature_t* fb = (const degradation_feature_t*)b;

    if (fa->disable_at != fb->disable_at) {
        return fa->disable_at - fb->disable_at;
    }

    // Secondary sort by resource cost
    if (fa->resource_cost > fb->resource_cost) return 1;
    if (fa->resource_cost < fb->resource_cost) return -1;
    return 0;
}

/**
 * Apply degradation level changes
 */
static nimcp_result_t apply_degradation_level(
    degradation_state_t* state,
    degradation_level_t new_level,
    void* bio_ctx
) {
    if (!state) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    degradation_level_t old_level = state->current_level;

    LOG_INFO("Applying degradation level change: %d -> %d", old_level, new_level);

    // Determine which features to enable/disable
    for (uint32_t i = 0; i < state->feature_count; i++) {
        degradation_feature_t* feature = &state->features[i];

        // Skip core features
        if (feature->is_core) {
            continue;
        }

        bool should_be_enabled = (new_level < feature->disable_at);

        if (feature->currently_enabled != should_be_enabled) {
            feature->currently_enabled = should_be_enabled;

            if (should_be_enabled) {
                state->active_features++;
                LOG_INFO("Enabled feature: %s (ID=0x%04x)",
                         feature->name, feature->feature_id);
            } else {
                state->active_features--;
                LOG_INFO("Disabled feature: %s (ID=0x%04x)",
                         feature->name, feature->feature_id);
            }

            // Broadcast feature change event
            degradation_event_t event = {
                .type = should_be_enabled ? DEGRADATION_EVENT_FEATURE_ENABLED
                                          : DEGRADATION_EVENT_FEATURE_DISABLED,
                .old_level = old_level,
                .new_level = new_level,
                .feature_id = feature->feature_id,
                .resource_usage = state->resource_usage,
                .reason = should_be_enabled ? "Restoration" : "Degradation"
            };
            broadcast_degradation_event(bio_ctx, &event);
        }
    }

    state->current_level = new_level;
    state->target_level = new_level;
    state->last_change_time_ms = nimcp_time_monotonic_ms();

    // Broadcast level change event
    degradation_event_t level_event = {
        .type = DEGRADATION_EVENT_LEVEL_CHANGE,
        .old_level = old_level,
        .new_level = new_level,
        .feature_id = 0,
        .resource_usage = state->resource_usage,
        .reason = (new_level > old_level) ? "Resource pressure" : "Resource recovery"
    };
    broadcast_degradation_event(bio_ctx, &level_event);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "DEGRADATION_LEVEL_CHANGE",
                  "Level changed from %d to %d", old_level, new_level);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

degradation_state_t* portia_degradation_init(
    const degradation_internal_config_t* config
) {
    // NULL config is allowed - defaults will be used (see below)

    // Thread-safe initialization of global mutex
    nimcp_platform_once(&g_degradation_once, init_degradation_mutex);

    LOG_INFO("Initializing portia degradation system");

    // Allocate state
    degradation_state_t* state = (degradation_state_t*)nimcp_calloc(
        1, sizeof(degradation_state_t)
    );
    if (!state) {
        LOG_ERROR("Failed to allocate degradation state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate degradation state");
        return NULL;
    }

    // Initialize mutex
    if (nimcp_mutex_init(&state->lock, NULL) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize mutex in portia_degradation_init");
        nimcp_free(state);
        return NULL;
    }

    // Allocate feature array with capacity for growth
    state->feature_capacity = DEFAULT_FEATURE_COUNT * 2;
    state->features = (degradation_feature_t*)nimcp_calloc(
        state->feature_capacity, sizeof(degradation_feature_t)
    );
    if (!state->features) {
        LOG_ERROR("Failed to allocate feature array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate feature array in portia_degradation_init");
        nimcp_mutex_destroy(&state->lock);
        nimcp_free(state);
        return NULL;
    }

    // Copy default features
    memcpy(state->features, DEFAULT_FEATURES,
           DEFAULT_FEATURE_COUNT * sizeof(degradation_feature_t));
    state->feature_count = DEFAULT_FEATURE_COUNT;
    state->active_features = DEFAULT_FEATURE_COUNT;

    // Initialize state
    state->current_level = DEGRADATION_LEVEL_NONE;
    state->target_level = DEGRADATION_LEVEL_NONE;
    state->resource_usage = 0.0F;
    state->last_change_time_ms = 0;  // Set to 0 so first evaluation is not blocked by hysteresis

    // Sort features by degradation level
    qsort(state->features, state->feature_count,
          sizeof(degradation_feature_t), compare_features_by_level);

    // Store context (protected by global mutex)
    nimcp_mutex_lock(g_degradation_mutex);
    if (config) {
        memcpy(&g_degradation_ctx.config, config,
               sizeof(degradation_internal_config_t));
    } else {
        // Default configuration
        g_degradation_ctx.config.level_thresholds[DEGRADATION_LEVEL_NONE] = 0.0F;
        g_degradation_ctx.config.level_thresholds[DEGRADATION_LEVEL_MINOR] = 70.0F;
        g_degradation_ctx.config.level_thresholds[DEGRADATION_LEVEL_MODERATE] = 80.0F;
        g_degradation_ctx.config.level_thresholds[DEGRADATION_LEVEL_SEVERE] = 90.0F;
        g_degradation_ctx.config.level_thresholds[DEGRADATION_LEVEL_CRITICAL] = 95.0F;
        g_degradation_ctx.config.hysteresis_ms = 5000;  // 5 seconds
        g_degradation_ctx.config.enable_auto_degrade = true;
        g_degradation_ctx.config.enable_auto_restore = true;
        g_degradation_ctx.config.restore_threshold = 10.0F;  // 10% below threshold
    }

    g_degradation_ctx.initialized = true;
    nimcp_mutex_unlock(g_degradation_mutex);

    LOG_INFO("Degradation system initialized with %u features", state->feature_count);
    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "DEGRADATION_INIT_SUCCESS",
                  "Initialized with %u features", state->feature_count);

    return state;
}

void portia_degradation_cleanup(degradation_state_t* state) {
    if (!state) {
        return;
    }

    LOG_INFO("Cleaning up degradation system");

    nimcp_mutex_lock(&state->lock);

    if (state->features) {
        nimcp_free(state->features);
        state->features = NULL;
    }

    state->feature_count = 0;
    state->feature_capacity = 0;

    nimcp_mutex_unlock(&state->lock);
    nimcp_mutex_destroy(&state->lock);

    nimcp_free(state);

    // Protected write to global context
    if (g_degradation_mutex) {
        nimcp_mutex_lock(g_degradation_mutex);
        g_degradation_ctx.initialized = false;
        nimcp_mutex_unlock(g_degradation_mutex);
    }

    LOG_INFO("Degradation system cleanup complete");
}

nimcp_result_t portia_degradation_evaluate(
    degradation_state_t* state,
    float resource_usage,
    void* bio_ctx
) {
    if (!state) {
        LOG_ERROR("Invalid state pointer");
        bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "DEGRADATION_EVALUATE_FAILED", "Invalid state pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Validate resource usage range
    if (resource_usage < 0.0F || resource_usage > 100.0F) {
        LOG_ERROR("Invalid resource usage: %.2f", resource_usage);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&state->lock);

    state->resource_usage = resource_usage;

    // Check hysteresis - prevent rapid oscillation
    uint64_t now = nimcp_time_monotonic_ms();
    uint64_t time_since_change = now - state->last_change_time_ms;

    if (time_since_change < g_degradation_ctx.config.hysteresis_ms) {
        nimcp_mutex_unlock(&state->lock);
        LOG_DEBUG("Hysteresis active, skipping evaluation (time=%llu ms)",
                  (unsigned long long)time_since_change);
        return NIMCP_SUCCESS;
    }

    degradation_level_t new_level = state->current_level;

    // Determine appropriate degradation level
    if (g_degradation_ctx.config.enable_auto_degrade) {
        // Check for increased degradation
        for (int level = DEGRADATION_LEVEL_CRITICAL; level > DEGRADATION_LEVEL_NONE; level--) {
            if (resource_usage >= g_degradation_ctx.config.level_thresholds[level]) {
                new_level = (degradation_level_t)level;
                break;
            }
        }
    }

    if (g_degradation_ctx.config.enable_auto_restore) {
        // Check for restoration opportunity
        if (state->current_level > DEGRADATION_LEVEL_NONE) {
            // Calculate restore threshold
            float current_threshold = g_degradation_ctx.config.level_thresholds[state->current_level];
            float restore_point = current_threshold - g_degradation_ctx.config.restore_threshold;

            if (resource_usage < restore_point) {
                // Find appropriate lower level
                for (int level = state->current_level - 1; level >= DEGRADATION_LEVEL_NONE; level--) {
                    float level_threshold = g_degradation_ctx.config.level_thresholds[level];
                    if (resource_usage >= level_threshold) {
                        new_level = (degradation_level_t)level;
                        break;
                    } else if (level == DEGRADATION_LEVEL_NONE) {
                        new_level = DEGRADATION_LEVEL_NONE;
                        break;
                    }
                }
            }
        }
    }

    // Apply level change if needed
    nimcp_result_t result = NIMCP_SUCCESS;
    if (new_level != state->current_level) {
        LOG_INFO("Resource usage %.2f%% triggered level change: %d -> %d",
                 resource_usage, state->current_level, new_level);

        result = apply_degradation_level(state, new_level, bio_ctx);
    } else {
        LOG_DEBUG("Evaluated degradation: level=%d, usage=%.2f%%, no change",
                  state->current_level, resource_usage);
    }

    // Check for warning thresholds
    for (int level = DEGRADATION_LEVEL_MINOR; level < DEGRADATION_LEVEL_COUNT; level++) {
        float threshold = g_degradation_ctx.config.level_thresholds[level];
        if (resource_usage >= threshold - 5.0F && resource_usage < threshold) {
            degradation_event_t warning_event = {
                .type = DEGRADATION_EVENT_RESOURCE_WARNING,
                .old_level = state->current_level,
                .new_level = (degradation_level_t)level,
                .feature_id = 0,
                .resource_usage = resource_usage,
                .reason = "Approaching threshold"
            };
            broadcast_degradation_event(bio_ctx, &warning_event);
            LOG_WARN("Resource usage %.2f%% approaching level %d threshold %.2f%%",
                     resource_usage, level, threshold);
            break;
        }
    }

    nimcp_mutex_unlock(&state->lock);

    return result;
}

nimcp_result_t portia_degradation_set_level(
    degradation_state_t* state,
    degradation_level_t level,
    void* bio_ctx
) {
    if (!state) {
        LOG_ERROR("Invalid state pointer");
        bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "DEGRADATION_SET_LEVEL_FAILED", "Invalid state pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Validate level
    if (level < DEGRADATION_LEVEL_NONE || level >= DEGRADATION_LEVEL_COUNT) {
        LOG_ERROR("Invalid degradation level: %d", level);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&state->lock);

    LOG_INFO("Forcing degradation level to %d (current=%d)", level, state->current_level);

    nimcp_result_t result = apply_degradation_level(state, level, bio_ctx);

    nimcp_mutex_unlock(&state->lock);

    return result;
}

nimcp_result_t portia_degradation_disable_feature(
    degradation_state_t* state,
    uint32_t feature_id,
    void* bio_ctx
) {
    if (!state) {
        LOG_ERROR("Invalid state pointer");
        bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "DEGRADATION_DISABLE_FEATURE_FAILED", "Invalid state pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&state->lock);

    nimcp_result_t result = NIMCP_ERROR_INVALID_PARAM;

    for (uint32_t i = 0; i < state->feature_count; i++) {
        if (state->features[i].feature_id == feature_id) {
            degradation_feature_t* feature = &state->features[i];

            // Check if core feature
            if (feature->is_core) {
                LOG_ERROR("Cannot disable core feature: %s", feature->name);
                result = NIMCP_ERROR_INVALID_PARAM;
                break;
            }

            if (feature->currently_enabled) {
                feature->currently_enabled = false;
                state->active_features--;

                LOG_INFO("Manually disabled feature: %s (ID=0x%04x)",
                         feature->name, feature->feature_id);

                // Broadcast event
                degradation_event_t event = {
                    .type = DEGRADATION_EVENT_FEATURE_DISABLED,
                    .old_level = state->current_level,
                    .new_level = state->current_level,
                    .feature_id = feature_id,
                    .resource_usage = state->resource_usage,
                    .reason = "Manual override"
                };
                broadcast_degradation_event(bio_ctx, &event);

                bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "DEGRADATION_FEATURE_DISABLED",
                              "Feature 0x%04x disabled manually", feature_id);
            }

            result = NIMCP_SUCCESS;
            break;
        }
    }

    nimcp_mutex_unlock(&state->lock);

    return result;
}

nimcp_result_t portia_degradation_enable_feature(
    degradation_state_t* state,
    uint32_t feature_id,
    void* bio_ctx
) {
    if (!state) {
        LOG_ERROR("Invalid state pointer");
        bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "DEGRADATION_ENABLE_FEATURE_FAILED", "Invalid state pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&state->lock);

    nimcp_result_t result = NIMCP_ERROR_INVALID_PARAM;

    for (uint32_t i = 0; i < state->feature_count; i++) {
        if (state->features[i].feature_id == feature_id) {
            degradation_feature_t* feature = &state->features[i];

            if (!feature->currently_enabled) {
                feature->currently_enabled = true;
                state->active_features++;

                LOG_INFO("Manually enabled feature: %s (ID=0x%04x)",
                         feature->name, feature->feature_id);

                // Broadcast event
                degradation_event_t event = {
                    .type = DEGRADATION_EVENT_FEATURE_ENABLED,
                    .old_level = state->current_level,
                    .new_level = state->current_level,
                    .feature_id = feature_id,
                    .resource_usage = state->resource_usage,
                    .reason = "Manual override"
                };
                broadcast_degradation_event(bio_ctx, &event);

                bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "DEGRADATION_FEATURE_ENABLED",
                              "Feature 0x%04x enabled manually", feature_id);
            }

            result = NIMCP_SUCCESS;
            break;
        }
    }

    nimcp_mutex_unlock(&state->lock);

    return result;
}

nimcp_result_t portia_degradation_get_state(
    const degradation_state_t* state,
    degradation_level_t* level,
    uint32_t* active_features,
    float* resource_usage
) {
    if (!state) {
        LOG_ERROR("Invalid state pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Note: Using const_cast pattern for mutex - safe as we only read
    degradation_state_t* mutable_state = (degradation_state_t*)state;
    nimcp_mutex_lock(&mutable_state->lock);

    if (level) {
        *level = state->current_level;
    }
    if (active_features) {
        *active_features = state->active_features;
    }
    if (resource_usage) {
        *resource_usage = state->resource_usage;
    }

    nimcp_mutex_unlock(&mutable_state->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t portia_degradation_register_feature(
    degradation_state_t* state,
    const degradation_feature_t* feature
) {
    if (!state) {
        LOG_ERROR("Invalid state pointer");
        bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "DEGRADATION_REGISTER_FAILED", "Invalid state pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!feature) {
        LOG_ERROR("Invalid feature pointer");
        bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "DEGRADATION_REGISTER_FAILED", "Invalid feature pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&state->lock);

    // Check for duplicate
    for (uint32_t i = 0; i < state->feature_count; i++) {
        if (state->features[i].feature_id == feature->feature_id) {
            LOG_WARN("Feature 0x%04x already registered", feature->feature_id);
            nimcp_mutex_unlock(&state->lock);
            return NIMCP_ALREADY_EXISTS;
        }
    }

    // Check capacity
    if (state->feature_count >= state->feature_capacity) {
        // Grow array
        uint32_t new_capacity = state->feature_capacity * 2;
        degradation_feature_t* new_array = (degradation_feature_t*)nimcp_calloc(
            new_capacity, sizeof(degradation_feature_t)
        );
        if (!new_array) {
            LOG_ERROR("Failed to grow feature array");
            nimcp_mutex_unlock(&state->lock);
            return NIMCP_ERROR_MEMORY;
        }

        memcpy(new_array, state->features,
               state->feature_count * sizeof(degradation_feature_t));
        nimcp_free(state->features);
        state->features = new_array;
        state->feature_capacity = new_capacity;

        LOG_DEBUG("Grew feature array to capacity %u", new_capacity);
    }

    // Add feature
    memcpy(&state->features[state->feature_count], feature,
           sizeof(degradation_feature_t));
    state->feature_count++;

    if (feature->currently_enabled) {
        state->active_features++;
    }

    // Re-sort features
    qsort(state->features, state->feature_count,
          sizeof(degradation_feature_t), compare_features_by_level);

    LOG_INFO("Registered feature: %s (ID=0x%04x, disable_at=%d)",
             feature->name, feature->feature_id, feature->disable_at);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "DEGRADATION_FEATURE_REGISTERED",
                  "Feature 0x%04x registered", feature->feature_id);

    nimcp_mutex_unlock(&state->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t portia_degradation_get_chain(
    const degradation_state_t* state,
    degradation_feature_t* chain,
    uint32_t chain_size,
    uint32_t* actual_count
) {
    if (!state) {
        LOG_ERROR("Invalid state pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!chain) {
        LOG_ERROR("Invalid chain buffer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!actual_count) {
        LOG_ERROR("Invalid actual_count pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Note: Using const_cast pattern for mutex - safe as we only read
    degradation_state_t* mutable_state = (degradation_state_t*)state;
    nimcp_mutex_lock(&mutable_state->lock);

    uint32_t count = (state->feature_count < chain_size) ?
                     state->feature_count : chain_size;

    memcpy(chain, state->features, count * sizeof(degradation_feature_t));
    *actual_count = count;

    nimcp_mutex_unlock(&mutable_state->lock);

    LOG_DEBUG("Retrieved degradation chain: %u features", count);

    return NIMCP_SUCCESS;
}

nimcp_result_t portia_degradation_is_feature_enabled(
    const degradation_state_t* state,
    uint32_t feature_id,
    bool* is_enabled
) {
    if (!state) {
        LOG_ERROR("Invalid state pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!is_enabled) {
        LOG_ERROR("Invalid is_enabled pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Note: Using const_cast pattern for mutex - safe as we only read
    degradation_state_t* mutable_state = (degradation_state_t*)state;
    nimcp_mutex_lock(&mutable_state->lock);

    nimcp_result_t result = NIMCP_NOT_FOUND;  /* Return NOT_FOUND if feature not found */

    for (uint32_t i = 0; i < state->feature_count; i++) {
        if (state->features[i].feature_id == feature_id) {
            *is_enabled = state->features[i].currently_enabled;
            result = NIMCP_SUCCESS;
            break;
        }
    }

    nimcp_mutex_unlock(&mutable_state->lock);

    return result;
}

nimcp_result_t portia_degradation_get_features_for_level(
    const degradation_state_t* state,
    degradation_level_t level,
    uint32_t* features,
    uint32_t max_features,
    uint32_t* actual_count
) {
    if (!state) {
        LOG_ERROR("Invalid state pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!features) {
        LOG_ERROR("Invalid features buffer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!actual_count) {
        LOG_ERROR("Invalid actual_count pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Validate level
    if (level < DEGRADATION_LEVEL_NONE || level >= DEGRADATION_LEVEL_COUNT) {
        LOG_ERROR("Invalid degradation level: %d", level);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Note: Using const_cast pattern for mutex - safe as we only read
    degradation_state_t* mutable_state = (degradation_state_t*)state;
    nimcp_mutex_lock(&mutable_state->lock);

    uint32_t count = 0;
    for (uint32_t i = 0; i < state->feature_count && count < max_features; i++) {
        const degradation_feature_t* feature = &state->features[i];

        // Feature should be disabled at this level
        if (feature->disable_at <= level && !feature->is_core) {
            features[count++] = feature->feature_id;
        }
    }

    *actual_count = count;

    nimcp_mutex_unlock(&mutable_state->lock);

    LOG_DEBUG("Found %u features to disable at level %d", count, level);

    return NIMCP_SUCCESS;
}
