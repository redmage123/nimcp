/**
 * @file nimcp_graceful_degradation.c
 * @brief Graceful Degradation Profiles Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Service level tiers, feature prioritization, resource budgeting
 * WHY:  Maintain core functionality when resources are constrained
 * HOW:  Define service tiers, prioritize features, shed load progressively
 *
 * BIOLOGICAL BASIS:
 * - Autonomic regulation (reduce heart rate during rest)
 * - Energy conservation (hibernation, torpor states)
 * - Triage system (prioritize critical functions under stress)
 * - Neural plasticity (brain reassigns resources after damage)
 *
 * IMMUNE SYSTEM INTEGRATION:
 * - Security module can trigger degradation on threat detection
 * - Degradation events are reported to security for audit
 * - Resource stress may indicate attack vectors
 */

#include "utils/fault_tolerance/nimcp_graceful_degradation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(graceful_degradation)

/* P2-6 FIX: Helper function to check mutex lock return value.
 * All critical nimcp_mutex_lock() calls must check the return value.
 * Returns 0 on success, non-zero on failure (after logging and reporting). */
static inline int gd_lock_checked(nimcp_mutex_t* mutex, const char* caller) {
    int result = nimcp_mutex_lock(mutex);
    if (result != 0) {
        LOG_ERROR("GD", "%s: failed to acquire lock (error %d)", caller, result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_LOCK,
            "graceful_degradation: %s failed to acquire lock (error %d)", caller, result);
    }
    return result;
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Callback registration
 */
typedef struct {
    gd_tier_callback_t callback;
    void* user_data;
    bool active;
} gd_callback_entry_t;

/**
 * @brief Transition history entry
 */
typedef struct {
    gd_transition_event_t event;
    bool valid;
} gd_history_entry_t;

/**
 * @brief Internal context structure
 */
struct gd_context {
    gd_config_t config;

    /* Feature management */
    gd_feature_t features[GD_MAX_FEATURES];
    uint32_t feature_count;
    uint32_t next_feature_id;

    /* Resource management */
    gd_resource_budget_t resources[GD_RESOURCE_COUNT];
    float current_usage[GD_RESOURCE_COUNT];

    /* Profile management */
    gd_profile_t profiles[GD_MAX_PROFILES];
    uint32_t profile_count;
    uint32_t next_profile_id;
    uint32_t active_profile_id;

    /* Current state */
    gd_tier_t current_tier;
    uint64_t tier_start_time_ms;

    /* Load shedding */
    gd_load_shed_config_t load_shed;

    /* Callbacks */
    gd_callback_entry_t callbacks[8];
    uint32_t callback_count;

    /* Transition history */
    gd_history_entry_t history[64];
    uint32_t history_head;
    uint32_t history_count;

    /* Statistics */
    gd_stats_t stats;

    /* Threading */
    nimcp_mutex_t mutex;
    nimcp_thread_t monitor_thread;
    bool running;
    bool monitor_running;

    /* Security integration */
    bool security_registered;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t gd_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Notify security module of degradation event
 */
static void gd_notify_security(gd_context_t* ctx, const gd_transition_event_t* event) {
    if (!ctx || !event) return;

    const char* action = (event->to_tier > event->from_tier) ? "DOWNGRADE" : "UPGRADE";

    bbb_audit_log(BBB_AUDIT_WARNING, "GD", action,
                  "Tier change: %s -> %s, trigger=%s, resource=%s",
                  gd_tier_to_string(event->from_tier),
                  gd_tier_to_string(event->to_tier),
                  gd_trigger_to_string(event->trigger),
                  gd_resource_to_string(event->trigger_resource));

    /* Emergency tier triggers security alert */
    if (event->to_tier == GD_TIER_EMERGENCY) {
        bbb_audit_log(BBB_AUDIT_ERROR, "GD", "EMERGENCY",
                      "System entered emergency degradation tier");
    }
}

/**
 * @brief Invoke callbacks for tier change
 */
static void gd_invoke_callbacks(gd_context_t* ctx, const gd_transition_event_t* event) {
    if (!ctx || !event) return;

    for (uint32_t i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].active && ctx->callbacks[i].callback) {
            ctx->callbacks[i].callback(event, ctx->callbacks[i].user_data);
        }
    }
}

/**
 * @brief Add transition to history
 */
static void gd_record_transition(gd_context_t* ctx, const gd_transition_event_t* event) {
    if (!ctx || !event) return;

    ctx->history[ctx->history_head].event = *event;
    ctx->history[ctx->history_head].valid = true;
    ctx->history_head = (ctx->history_head + 1) % 64;
    if (ctx->history_count < 64) {
        ctx->history_count++;
    }
}

/**
 * @brief Apply tier to features
 */
static void gd_apply_tier_to_features(gd_context_t* ctx, gd_tier_t tier,
                                       gd_degradation_action_t* actions, uint32_t* action_count) {
    if (!ctx || !actions || !action_count) return;

    *action_count = 0;

    for (uint32_t i = 0; i < ctx->feature_count; i++) {
        gd_feature_t* feature = &ctx->features[i];

        /* Check if feature should be enabled at this tier */
        bool should_enable = (tier <= feature->minimum_tier);

        if (feature->is_enabled != should_enable) {
            feature->is_enabled = should_enable;

            if (*action_count < 16) {
                actions[*action_count].action = GD_ACTION_DISABLE_FEATURE;
                actions[*action_count].target_id = feature->feature_id;
                actions[*action_count].parameter = should_enable ? 1.0f : 0.0f;
                snprintf(actions[*action_count].description, 128,
                         "%s feature: %s", should_enable ? "Enable" : "Disable", feature->name);
                (*action_count)++;
            }

            if (!should_enable) {
                ctx->stats.features_disabled++;
            }
        }

        /* Apply quality reduction */
        if (feature->can_degrade && feature->is_enabled) {
            float quality_mult = 1.0f;

            /* Get quality multiplier from active profile */
            if (ctx->active_profile_id > 0) {
                for (uint32_t p = 0; p < ctx->profile_count; p++) {
                    if (ctx->profiles[p].profile_id == ctx->active_profile_id) {
                        quality_mult = ctx->profiles[p].quality_multipliers[tier];
                        break;
                    }
                }
            } else {
                /* Default quality multipliers */
                float defaults[] = {1.0f, 0.9f, 0.7f, 0.5f, 0.3f};
                quality_mult = defaults[tier];
            }

            float new_quality = feature->current_quality * quality_mult;
            if (new_quality < feature->min_quality) {
                new_quality = feature->min_quality;
            }

            if (new_quality != feature->current_quality && *action_count < 16) {
                feature->current_quality = new_quality;

                actions[*action_count].action = GD_ACTION_REDUCE_QUALITY;
                actions[*action_count].target_id = feature->feature_id;
                actions[*action_count].parameter = new_quality;
                snprintf(actions[*action_count].description, 128,
                         "Reduce quality for %s to %.1f%%", feature->name, new_quality);
                (*action_count)++;
            }
        }
    }
}

/**
 * @brief Evaluate if tier should change based on resources
 */
static gd_tier_t gd_evaluate_target_tier(gd_context_t* ctx) {
    if (!ctx) return GD_TIER_FULL;

    gd_tier_t target_tier = GD_TIER_FULL;

    /* Check each resource against thresholds */
    for (int r = 0; r < GD_RESOURCE_COUNT; r++) {
        float usage = ctx->current_usage[r];

        /* Apply hysteresis */
        float hysteresis = ctx->config.hysteresis_percent;

        /* Check critical threshold */
        if (usage >= ctx->resources[r].critical_threshold) {
            if (target_tier < GD_TIER_EMERGENCY) {
                target_tier = GD_TIER_EMERGENCY;
            }
        }
        /* Check warning threshold */
        else if (usage >= ctx->resources[r].warning_threshold) {
            if (target_tier < GD_TIER_REDUCED) {
                target_tier = GD_TIER_REDUCED;
            }
        }

        /* Check budget per tier with hysteresis */
        for (int t = GD_TIER_FULL; t <= GD_TIER_EMERGENCY; t++) {
            float budget = ctx->resources[r].budget_per_tier[t];

            if (budget > 0 && usage > budget + hysteresis) {
                if (target_tier < t + 1 && t < GD_TIER_EMERGENCY) {
                    target_tier = t + 1;
                }
            }
        }
    }

    /* Respect minimum tier config */
    if (target_tier > ctx->config.minimum_tier) {
        target_tier = ctx->config.minimum_tier;
    }

    return target_tier;
}

/**
 * @brief Monitoring thread function
 */
static void* gd_monitor_thread(void* arg) {
    gd_context_t* ctx = (gd_context_t*)arg;

    while (ctx->monitor_running) {
        /* P2-6 FIX: Check mutex lock return value */
        if (gd_lock_checked(&ctx->mutex, "gd_monitor_thread") != 0) {
            /* Lock failed - skip this iteration rather than operating on unprotected data */
            break;
        }

        if (ctx->config.enable_auto_degradation) {
            gd_evaluate_tier(ctx);
        }

        /* Update time spent at current tier */
        uint64_t now = gd_get_time_ms();
        uint64_t elapsed = now - ctx->tier_start_time_ms;
        ctx->stats.time_per_tier_ms[ctx->current_tier] += elapsed;
        ctx->tier_start_time_ms = now;

        /* Check load shedding expiration */
        if (ctx->load_shed.enabled && ctx->load_shed.shed_duration_ms > 0) {
            /* Duration tracking would go here */
        }

        nimcp_mutex_unlock(&ctx->mutex);

        /* Sleep for check interval */
        struct timespec ts;
        ts.tv_sec = ctx->config.check_interval_ms / 1000;
        ts.tv_nsec = (ctx->config.check_interval_ms % 1000) * 1000000;
        nanosleep(&ts, NULL);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_monitor_thread: operation failed");
    return NULL;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

gd_config_t gd_default_config(void) {
    gd_config_t config = {
        .enable_auto_degradation = true,
        .enable_load_shedding = true,
        .enable_quality_reduction = true,
        .hysteresis_percent = GD_HYSTERESIS_PERCENT,
        .check_interval_ms = 1000,
        .tier_cooldown_ms = 30000,
        .initial_tier = GD_TIER_FULL,
        .minimum_tier = GD_TIER_EMERGENCY
    };
    return config;
}

gd_context_t* gd_create(const gd_config_t* config) {
    /* Validate input */
    if (!config) {
        LOG_ERROR("GD", "NULL configuration provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    /* Allocate context */
    gd_context_t* ctx = (gd_context_t*)nimcp_malloc(sizeof(gd_context_t));
    if (!ctx) {
        LOG_ERROR("GD", "Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    memset(ctx, 0, sizeof(gd_context_t));
    ctx->config = *config;
    ctx->current_tier = config->initial_tier;
    ctx->tier_start_time_ms = gd_get_time_ms();
    ctx->next_feature_id = 1;
    ctx->next_profile_id = 1;

    /* Initialize mutex */
    if (nimcp_mutex_init(&ctx->mutex, NULL) != 0) {
        LOG_ERROR("GD", "Failed to initialize mutex");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "gd_create: validation failed");
        return NULL;
    }

    /* Initialize default resource budgets */
    for (int r = 0; r < GD_RESOURCE_COUNT; r++) {
        ctx->resources[r].type = r;
        ctx->resources[r].warning_threshold = 70.0f;
        ctx->resources[r].critical_threshold = 90.0f;

        /* Default budgets per tier */
        ctx->resources[r].budget_per_tier[GD_TIER_FULL] = 100.0f;
        ctx->resources[r].budget_per_tier[GD_TIER_STANDARD] = 80.0f;
        ctx->resources[r].budget_per_tier[GD_TIER_REDUCED] = 60.0f;
        ctx->resources[r].budget_per_tier[GD_TIER_MINIMAL] = 40.0f;
        ctx->resources[r].budget_per_tier[GD_TIER_EMERGENCY] = 20.0f;
    }

    /* Initialize statistics */
    ctx->stats.lowest_tier_reached = GD_TIER_FULL;
    ctx->stats.avg_quality = 100.0f;
    ctx->stats.min_quality_reached = 100.0f;

    /* Register with security module */
    ctx->security_registered = bbb_register_module("graceful_degradation", BBB_MODULE_TYPE_CORE);

    bbb_audit_log(BBB_AUDIT_INFO, "GD", "CREATE",
                  "Created graceful degradation context, initial_tier=%s",
                  gd_tier_to_string(config->initial_tier));

    LOG_INFO("GD", "Created graceful degradation context");

    return ctx;
}

void gd_destroy(gd_context_t* ctx) {
    if (!ctx) return;

    /* Stop monitoring if running */
    if (ctx->monitor_running) {
        gd_stop(ctx);
    }

    bbb_audit_log(BBB_AUDIT_INFO, "GD", "DESTROY",
                  "Destroying graceful degradation context, final_tier=%s",
                  gd_tier_to_string(ctx->current_tier));

    nimcp_mutex_destroy(&ctx->mutex);
    nimcp_free(ctx);

    LOG_INFO("GD", "Destroyed graceful degradation context");
}

bool gd_start(gd_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_start: ctx is NULL");
        return false;
    }

    /* P2-6 FIX: Check mutex lock return value */
    if (gd_lock_checked(&ctx->mutex, "gd_start") != 0) {
        return false;
    }

    if (ctx->running) {
        nimcp_mutex_unlock(&ctx->mutex);
        return true;
    }

    ctx->running = true;
    ctx->monitor_running = true;
    ctx->tier_start_time_ms = gd_get_time_ms();

    /* Start monitoring thread */
    if (nimcp_thread_create(&ctx->monitor_thread, gd_monitor_thread, ctx, NULL) != 0) {
        LOG_ERROR("GD", "Failed to start monitoring thread");
        ctx->running = false;
        ctx->monitor_running = false;
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_start: validation failed");
        return false;
    }

    nimcp_mutex_unlock(&ctx->mutex);

    bbb_audit_log(BBB_AUDIT_INFO, "GD", "START", "Started degradation monitoring");
    LOG_INFO("GD", "Started graceful degradation monitoring");

    return true;
}

bool gd_stop(gd_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_stop: ctx is NULL");
        return false;
    }

    /* P2-6 FIX: Check mutex lock return value */
    if (gd_lock_checked(&ctx->mutex, "gd_stop") != 0) {
        return false;
    }

    if (!ctx->running) {
        nimcp_mutex_unlock(&ctx->mutex);
        return true;
    }

    ctx->monitor_running = false;
    nimcp_mutex_unlock(&ctx->mutex);

    /* Wait for thread to finish */
    nimcp_thread_join(ctx->monitor_thread, NULL);

    /* P2-6 FIX: Check mutex lock return value */
    if (gd_lock_checked(&ctx->mutex, "gd_stop_final") != 0) {
        return false;
    }
    ctx->running = false;
    nimcp_mutex_unlock(&ctx->mutex);

    bbb_audit_log(BBB_AUDIT_INFO, "GD", "STOP", "Stopped degradation monitoring");
    LOG_INFO("GD", "Stopped graceful degradation monitoring");

    return true;
}

//=============================================================================
// Feature Management
//=============================================================================

uint32_t gd_register_feature(gd_context_t* ctx, const gd_feature_t* feature) {
    if (!ctx || !feature) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->feature_count >= GD_MAX_FEATURES) {
        LOG_WARNING("GD", "Maximum features reached");
        nimcp_mutex_unlock(&ctx->mutex);
        return 0;
    }

    /* Copy feature */
    gd_feature_t* f = &ctx->features[ctx->feature_count];
    *f = *feature;
    f->feature_id = ctx->next_feature_id++;
    f->is_enabled = (ctx->current_tier <= feature->minimum_tier);
    f->current_quality = 100.0f;

    ctx->feature_count++;

    uint32_t id = f->feature_id;

    nimcp_mutex_unlock(&ctx->mutex);

    LOG_DEBUG("GD", "Registered feature: %s (id=%u, priority=%s)",
                    feature->name, id, gd_priority_to_string(feature->priority));

    return id;
}

bool gd_unregister_feature(gd_context_t* ctx, uint32_t feature_id) {
    if (!ctx || feature_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_unregister_feature: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    for (uint32_t i = 0; i < ctx->feature_count; i++) {
        if (ctx->features[i].feature_id == feature_id) {
            /* Shift remaining features */
            for (uint32_t j = i; j < ctx->feature_count - 1; j++) {
                ctx->features[j] = ctx->features[j + 1];
            }
            ctx->feature_count--;
            nimcp_mutex_unlock(&ctx->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_unregister_feature: operation failed");
    return false;
}

bool gd_is_feature_enabled(gd_context_t* ctx, uint32_t feature_id) {
    if (!ctx || feature_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_is_feature_enabled: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    for (uint32_t i = 0; i < ctx->feature_count; i++) {
        if (ctx->features[i].feature_id == feature_id) {
            bool enabled = ctx->features[i].is_enabled;
            nimcp_mutex_unlock(&ctx->mutex);
            return enabled;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_is_feature_enabled: validation failed");
    return false;
}

float gd_get_feature_quality(gd_context_t* ctx, uint32_t feature_id) {
    if (!ctx || feature_id == 0) return 0.0f;

    nimcp_mutex_lock(&ctx->mutex);

    for (uint32_t i = 0; i < ctx->feature_count; i++) {
        if (ctx->features[i].feature_id == feature_id) {
            float quality = ctx->features[i].current_quality;
            nimcp_mutex_unlock(&ctx->mutex);
            return quality;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);
    return 0.0f;
}

bool gd_set_feature_enabled(gd_context_t* ctx, uint32_t feature_id, bool enabled) {
    if (!ctx || feature_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_set_feature_enabled: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    for (uint32_t i = 0; i < ctx->feature_count; i++) {
        if (ctx->features[i].feature_id == feature_id) {
            ctx->features[i].is_enabled = enabled;
            nimcp_mutex_unlock(&ctx->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_set_feature_enabled: validation failed");
    return false;
}

bool gd_set_feature_quality(gd_context_t* ctx, uint32_t feature_id, float quality) {
    if (!ctx || feature_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_set_feature_quality: ctx is NULL");
        return false;
    }
    if (quality < 0.0f || quality > 100.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_set_feature_quality: validation failed");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    for (uint32_t i = 0; i < ctx->feature_count; i++) {
        if (ctx->features[i].feature_id == feature_id) {
            ctx->features[i].current_quality = quality;

            /* Update stats */
            if (quality < ctx->stats.min_quality_reached) {
                ctx->stats.min_quality_reached = quality;
            }

            nimcp_mutex_unlock(&ctx->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_set_feature_quality: operation failed");
    return false;
}

//=============================================================================
// Resource Management
//=============================================================================

bool gd_update_resource(gd_context_t* ctx, gd_resource_t resource, float usage) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_update_resource: ctx is NULL");
        return false;
    }
    if (resource < 0 || resource >= GD_RESOURCE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "gd_update_resource: capacity exceeded");
        return false;
    }
    if (usage < 0.0f || usage > 100.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_update_resource: validation failed");
        return false;
    }

    /* P2-6 FIX: Check mutex lock return value */
    if (gd_lock_checked(&ctx->mutex, "gd_update_resource") != 0) {
        return false;
    }

    ctx->current_usage[resource] = usage;
    ctx->resources[resource].current_usage = usage;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

float gd_get_resource_usage(gd_context_t* ctx, gd_resource_t resource) {
    if (!ctx) return 0.0f;
    if (resource < 0 || resource >= GD_RESOURCE_COUNT) return 0.0f;

    nimcp_mutex_lock(&ctx->mutex);
    float usage = ctx->current_usage[resource];
    nimcp_mutex_unlock(&ctx->mutex);

    return usage;
}

bool gd_set_resource_budget(gd_context_t* ctx, const gd_resource_budget_t* budget) {
    if (!ctx || !budget) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_set_resource_budget: required parameter is NULL (ctx, budget)");
        return false;
    }
    if (budget->type < 0 || budget->type >= GD_RESOURCE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "gd_set_resource_budget: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    ctx->resources[budget->type] = *budget;
    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool gd_get_resource_budget(gd_context_t* ctx, gd_resource_t resource, gd_resource_budget_t* budget) {
    if (!ctx || !budget) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_get_resource_budget: required parameter is NULL (ctx, budget)");
        return false;
    }
    if (resource < 0 || resource >= GD_RESOURCE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "gd_get_resource_budget: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    *budget = ctx->resources[resource];
    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool gd_is_resource_critical(gd_context_t* ctx, gd_resource_t resource) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_is_resource_critical: ctx is NULL");
        return false;
    }
    if (resource < 0 || resource >= GD_RESOURCE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "gd_is_resource_critical: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    bool critical = ctx->current_usage[resource] >= ctx->resources[resource].critical_threshold;
    nimcp_mutex_unlock(&ctx->mutex);

    return critical;
}

//=============================================================================
// Tier Management
//=============================================================================

gd_tier_t gd_get_current_tier(gd_context_t* ctx) {
    if (!ctx) return GD_TIER_FULL;

    nimcp_mutex_lock(&ctx->mutex);
    gd_tier_t tier = ctx->current_tier;
    nimcp_mutex_unlock(&ctx->mutex);

    return tier;
}

bool gd_set_tier(gd_context_t* ctx, gd_tier_t tier, const char* reason) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_set_tier: ctx is NULL");
        return false;
    }
    if (tier < GD_TIER_FULL || tier > GD_TIER_EMERGENCY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_set_tier: validation failed");
        return false;
    }

    /* P2-6 FIX: Check mutex lock return value */
    if (gd_lock_checked(&ctx->mutex, "gd_set_tier") != 0) {
        return false;
    }

    gd_tier_t old_tier = ctx->current_tier;

    if (tier == old_tier) {
        nimcp_mutex_unlock(&ctx->mutex);
        return true;
    }

    /* Create transition event */
    gd_transition_event_t event = {0};
    event.from_tier = old_tier;
    event.to_tier = tier;
    event.trigger = GD_TRIGGER_MANUAL;
    event.timestamp_ms = gd_get_time_ms();

    /* Apply tier change */
    gd_apply_tier_to_features(ctx, tier, event.actions, &event.action_count);

    /* Update state */
    ctx->current_tier = tier;
    ctx->tier_start_time_ms = event.timestamp_ms;

    /* Update stats */
    ctx->stats.total_transitions++;
    if (tier > old_tier) {
        ctx->stats.downgrades++;
    } else {
        ctx->stats.upgrades++;
    }
    if (tier > ctx->stats.lowest_tier_reached) {
        ctx->stats.lowest_tier_reached = tier;
    }

    /* Record and notify */
    gd_record_transition(ctx, &event);
    gd_notify_security(ctx, &event);
    gd_invoke_callbacks(ctx, &event);

    nimcp_mutex_unlock(&ctx->mutex);

    LOG_INFO("GD", "Tier changed: %s -> %s (manual: %s)",
                   gd_tier_to_string(old_tier), gd_tier_to_string(tier),
                   reason ? reason : "unspecified");

    return true;
}

bool gd_evaluate_tier(gd_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_evaluate_tier: ctx is NULL");
        return false;
    }

    /* Already locked by caller or monitor thread */
    gd_tier_t target = gd_evaluate_target_tier(ctx);
    gd_tier_t current = ctx->current_tier;

    if (target == current) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_evaluate_tier: validation failed");
        return false;
    }

    /* Check cooldown */
    uint64_t now = gd_get_time_ms();
    uint64_t elapsed = now - ctx->tier_start_time_ms;

    if (elapsed < ctx->config.tier_cooldown_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_evaluate_tier: validation failed");
        return false;
    }

    /* Find triggering resource */
    gd_resource_t trigger_resource = GD_RESOURCE_CPU;
    float trigger_value = 0.0f;

    for (int r = 0; r < GD_RESOURCE_COUNT; r++) {
        if (ctx->current_usage[r] > trigger_value) {
            trigger_value = ctx->current_usage[r];
            trigger_resource = r;
        }
    }

    /* Create transition event */
    gd_transition_event_t event = {0};
    event.from_tier = current;
    event.to_tier = target;
    event.trigger = GD_TRIGGER_RESOURCE;
    event.trigger_resource = trigger_resource;
    event.trigger_value = trigger_value;
    event.timestamp_ms = now;

    /* Apply tier change */
    gd_apply_tier_to_features(ctx, target, event.actions, &event.action_count);

    /* Update state */
    ctx->current_tier = target;
    ctx->tier_start_time_ms = now;

    /* Update stats */
    ctx->stats.total_transitions++;
    if (target > current) {
        ctx->stats.downgrades++;
    } else {
        ctx->stats.upgrades++;
    }
    if (target > ctx->stats.lowest_tier_reached) {
        ctx->stats.lowest_tier_reached = target;
    }

    /* Record and notify */
    gd_record_transition(ctx, &event);
    gd_notify_security(ctx, &event);
    gd_invoke_callbacks(ctx, &event);

    LOG_INFO("GD", "Tier evaluated: %s -> %s (resource=%s, value=%.1f%%)",
                   gd_tier_to_string(current), gd_tier_to_string(target),
                   gd_resource_to_string(trigger_resource), trigger_value);

    return true;
}

uint32_t gd_get_transition_history(gd_context_t* ctx, gd_transition_event_t* events, uint32_t max_events) {
    if (!ctx || !events || max_events == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    uint32_t count = 0;
    uint32_t idx = (ctx->history_head + 64 - ctx->history_count) % 64;

    for (uint32_t i = 0; i < ctx->history_count && count < max_events; i++) {
        if (ctx->history[idx].valid) {
            events[count++] = ctx->history[idx].event;
        }
        idx = (idx + 1) % 64;
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return count;
}

//=============================================================================
// Profile Management
//=============================================================================

uint32_t gd_create_profile(gd_context_t* ctx, const gd_profile_t* profile) {
    if (!ctx || !profile) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->profile_count >= GD_MAX_PROFILES) {
        nimcp_mutex_unlock(&ctx->mutex);
        return 0;
    }

    gd_profile_t* p = &ctx->profiles[ctx->profile_count];
    *p = *profile;
    p->profile_id = ctx->next_profile_id++;

    ctx->profile_count++;

    uint32_t id = p->profile_id;

    nimcp_mutex_unlock(&ctx->mutex);

    LOG_DEBUG("GD", "Created profile: %s (id=%u)", profile->name, id);

    return id;
}

bool gd_delete_profile(gd_context_t* ctx, uint32_t profile_id) {
    if (!ctx || profile_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_delete_profile: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    for (uint32_t i = 0; i < ctx->profile_count; i++) {
        if (ctx->profiles[i].profile_id == profile_id) {
            /* Check if active */
            if (ctx->active_profile_id == profile_id) {
                ctx->active_profile_id = 0;
            }

            /* Shift remaining */
            for (uint32_t j = i; j < ctx->profile_count - 1; j++) {
                ctx->profiles[j] = ctx->profiles[j + 1];
            }
            ctx->profile_count--;

            nimcp_mutex_unlock(&ctx->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_delete_profile: operation failed");
    return false;
}

bool gd_activate_profile(gd_context_t* ctx, uint32_t profile_id) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_activate_profile: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    /* Find profile */
    for (uint32_t i = 0; i < ctx->profile_count; i++) {
        if (ctx->profiles[i].profile_id == profile_id) {
            ctx->active_profile_id = profile_id;

            /* Apply profile's current tier */
            gd_tier_t tier = ctx->profiles[i].current_tier;
            if (tier != ctx->current_tier) {
                nimcp_mutex_unlock(&ctx->mutex);
                gd_set_tier(ctx, tier, "profile activation");
                return true;
            }

            nimcp_mutex_unlock(&ctx->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_activate_profile: operation failed");
    return false;
}

bool gd_get_active_profile(gd_context_t* ctx, gd_profile_t* profile) {
    if (!ctx || !profile) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_get_active_profile: required parameter is NULL (ctx, profile)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->active_profile_id == 0) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_get_active_profile: ctx->active_profile_id is zero");
        return false;
    }

    for (uint32_t i = 0; i < ctx->profile_count; i++) {
        if (ctx->profiles[i].profile_id == ctx->active_profile_id) {
            *profile = ctx->profiles[i];
            nimcp_mutex_unlock(&ctx->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_get_active_profile: validation failed");
    return false;
}

//=============================================================================
// Load Shedding
//=============================================================================

bool gd_start_load_shedding(gd_context_t* ctx, float shed_rate, gd_priority_t min_priority, uint64_t duration_ms) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_start_load_shedding: ctx is NULL");
        return false;
    }
    if (shed_rate < 0.0f || shed_rate > 100.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_start_load_shedding: validation failed");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ctx->load_shed.enabled = true;
    ctx->load_shed.shed_rate = shed_rate;
    ctx->load_shed.min_priority = min_priority;
    ctx->load_shed.shed_duration_ms = duration_ms;
    ctx->load_shed.shed_count = 0;

    nimcp_mutex_unlock(&ctx->mutex);

    bbb_audit_log(BBB_AUDIT_WARNING, "GD", "LOAD_SHED",
                  "Started load shedding: rate=%.1f%%, min_priority=%s",
                  shed_rate, gd_priority_to_string(min_priority));

    LOG_WARNING("GD", "Started load shedding: %.1f%%, min_priority=%s",
                      shed_rate, gd_priority_to_string(min_priority));

    return true;
}

bool gd_stop_load_shedding(gd_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_stop_load_shedding: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    uint32_t shed_count = ctx->load_shed.shed_count;
    ctx->load_shed.enabled = false;
    ctx->stats.items_shed += shed_count;

    nimcp_mutex_unlock(&ctx->mutex);

    bbb_audit_log(BBB_AUDIT_INFO, "GD", "LOAD_SHED",
                  "Stopped load shedding, items_shed=%u", shed_count);

    LOG_INFO("GD", "Stopped load shedding, items shed: %u", shed_count);

    return true;
}

bool gd_should_accept_request(gd_context_t* ctx, gd_priority_t priority) {
    if (!ctx) return true;

    nimcp_mutex_lock(&ctx->mutex);

    /* If not shedding, accept all */
    if (!ctx->load_shed.enabled) {
        nimcp_mutex_unlock(&ctx->mutex);
        return true;
    }

    /* Check priority threshold */
    if (priority < ctx->load_shed.min_priority) {
        nimcp_mutex_unlock(&ctx->mutex);
        return true; /* High priority always accepted */
    }

    /* Random shedding based on rate */
    float r = (float)nimcp_tl_rand() / (float)RAND_MAX * 100.0f;
    bool accept = (r > ctx->load_shed.shed_rate);

    if (!accept) {
        ctx->load_shed.shed_count++;
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return accept;
}

bool gd_get_load_shed_status(gd_context_t* ctx, gd_load_shed_config_t* config) {
    if (!ctx || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_get_load_shed_status: required parameter is NULL (ctx, config)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    *config = ctx->load_shed;
    bool active = ctx->load_shed.enabled;
    nimcp_mutex_unlock(&ctx->mutex);

    return active;
}

//=============================================================================
// Callbacks
//=============================================================================

bool gd_register_callback(gd_context_t* ctx, gd_tier_callback_t callback, void* user_data) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_register_callback: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->callback_count >= 8) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "gd_register_callback: capacity exceeded");
        return false;
    }

    ctx->callbacks[ctx->callback_count].callback = callback;
    ctx->callbacks[ctx->callback_count].user_data = user_data;
    ctx->callbacks[ctx->callback_count].active = true;
    ctx->callback_count++;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool gd_unregister_callback(gd_context_t* ctx, gd_tier_callback_t callback) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_unregister_callback: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    for (uint32_t i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].callback == callback) {
            ctx->callbacks[i].active = false;
            nimcp_mutex_unlock(&ctx->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gd_unregister_callback: validation failed");
    return false;
}

//=============================================================================
// Statistics
//=============================================================================

bool gd_get_stats(gd_context_t* ctx, gd_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gd_get_stats: required parameter is NULL (ctx, stats)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    *stats = ctx->stats;
    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

void gd_reset_stats(gd_context_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(&ctx->mutex);
    memset(&ctx->stats, 0, sizeof(gd_stats_t));
    ctx->stats.lowest_tier_reached = ctx->current_tier;
    ctx->stats.avg_quality = 100.0f;
    ctx->stats.min_quality_reached = 100.0f;
    nimcp_mutex_unlock(&ctx->mutex);
}

uint64_t gd_get_time_at_tier(gd_context_t* ctx, gd_tier_t tier) {
    if (!ctx) return 0;
    if (tier < GD_TIER_FULL || tier > GD_TIER_EMERGENCY) return 0;

    nimcp_mutex_lock(&ctx->mutex);
    uint64_t time = ctx->stats.time_per_tier_ms[tier];
    nimcp_mutex_unlock(&ctx->mutex);

    return time;
}

//=============================================================================
// String Conversion
//=============================================================================

const char* gd_tier_to_string(gd_tier_t tier) {
    switch (tier) {
        case GD_TIER_FULL: return "Full";
        case GD_TIER_STANDARD: return "Standard";
        case GD_TIER_REDUCED: return "Reduced";
        case GD_TIER_MINIMAL: return "Minimal";
        case GD_TIER_EMERGENCY: return "Emergency";
        default: return "Unknown";
    }
}

const char* gd_priority_to_string(gd_priority_t priority) {
    switch (priority) {
        case GD_PRIORITY_CRITICAL: return "Critical";
        case GD_PRIORITY_HIGH: return "High";
        case GD_PRIORITY_MEDIUM: return "Medium";
        case GD_PRIORITY_LOW: return "Low";
        case GD_PRIORITY_OPTIONAL: return "Optional";
        default: return "Unknown";
    }
}

const char* gd_resource_to_string(gd_resource_t resource) {
    switch (resource) {
        case GD_RESOURCE_CPU: return "CPU";
        case GD_RESOURCE_MEMORY: return "Memory";
        case GD_RESOURCE_GPU: return "GPU";
        case GD_RESOURCE_NETWORK: return "Network";
        case GD_RESOURCE_POWER: return "Power";
        case GD_RESOURCE_LATENCY: return "Latency";
        case GD_RESOURCE_THROUGHPUT: return "Throughput";
        case GD_RESOURCE_STORAGE: return "Storage";
        default: return "Unknown";
    }
}

const char* gd_action_to_string(gd_action_t action) {
    switch (action) {
        case GD_ACTION_DISABLE_FEATURE: return "DisableFeature";
        case GD_ACTION_REDUCE_QUALITY: return "ReduceQuality";
        case GD_ACTION_REDUCE_BATCH: return "ReduceBatch";
        case GD_ACTION_INCREASE_INTERVAL: return "IncreaseInterval";
        case GD_ACTION_SHED_LOAD: return "ShedLoad";
        case GD_ACTION_CACHE_MORE: return "CacheMore";
        case GD_ACTION_CHECKPOINT: return "Checkpoint";
        case GD_ACTION_NOTIFY: return "Notify";
        default: return "Unknown";
    }
}

const char* gd_trigger_to_string(gd_trigger_t trigger) {
    switch (trigger) {
        case GD_TRIGGER_RESOURCE: return "Resource";
        case GD_TRIGGER_ERROR_RATE: return "ErrorRate";
        case GD_TRIGGER_LATENCY: return "Latency";
        case GD_TRIGGER_MANUAL: return "Manual";
        case GD_TRIGGER_CASCADING: return "Cascading";
        default: return "Unknown";
    }
}
