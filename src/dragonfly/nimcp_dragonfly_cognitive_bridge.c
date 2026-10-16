/**
 * @file nimcp_dragonfly_cognitive_bridge.c
 * @brief Implementation of Dragonfly-to-Cognitive Systems Bridge
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "dragonfly/nimcp_dragonfly_cognitive_bridge.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_tsdn.h"
#include "dragonfly/nimcp_dragonfly_tracking.h"
#include "dragonfly/nimcp_dragonfly_prediction.h"
#include "dragonfly/nimcp_dragonfly_intercept.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_cognitive_bridge)

#define LOG_MODULE "DRAGONFLY_COGNITIVE_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_cognitive_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    bool initialized;

    /* Reference to dragonfly system */
    dragonfly_system_t* dragonfly;

    /* Configuration */
    dragonfly_cognitive_config_t config;

    /* Registered cognitive systems (may be NULL) */
    salience_evaluator_t salience;
    attention_system_t attention;
    working_memory_t wm;
    executive_control_t executive;

    /* Internal state - salience */
    target_salience_t target_saliences[COGNITIVE_MAX_WM_TARGETS];
    uint32_t target_ids[COGNITIVE_MAX_WM_TARGETS];
    uint32_t num_tracked_targets;
    uint32_t most_salient_target;
    float most_salient_value;

    /* Internal state - attention */
    dragonfly_attention_focus_t attention_foci[COGNITIVE_MAX_ATTENTION_FOCI];
    uint32_t num_attention_foci;
    dragonfly_attention_focus_t primary_focus;

    /* Internal state - working memory */
    wm_target_entry_t wm_entries[COGNITIVE_MAX_WM_TARGETS];
    uint32_t wm_num_entries;
    uint64_t wm_last_refresh_us;

    /* Internal state - executive */
    executive_state_t exec_state;
    uint64_t last_decision_us;

    /* Statistics */
    cognitive_bridge_stats_t stats;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static float vec3_magnitude(const float v[3]) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static float compute_motion_salience(float speed, float max_speed) {
    /* Motion salience increases with speed */
    if (max_speed <= 0) return 0;
    return nimcp_clampf(speed / max_speed, 0, 1);
}

static float compute_direction_salience(const float velocity[3], const float to_self[3]) {
    /* Direction salience is higher when target moves toward self */
    float v_mag = vec3_magnitude(velocity);
    float d_mag = vec3_magnitude(to_self);
    if (v_mag < 1e-6f || d_mag < 1e-6f) return 0;

    /* Dot product gives cosine of angle */
    float dot = velocity[0]*to_self[0] + velocity[1]*to_self[1] + velocity[2]*to_self[2];
    float cos_angle = dot / (v_mag * d_mag);

    /* Map [-1, 1] to [0, 1] where 1 = moving toward, 0 = moving away */
    return nimcp_clampf((cos_angle + 1.0f) / 2.0f, 0, 1);
}

static dragonfly_attention_priority_t compute_priority_from_salience(float salience) {
    if (salience < 0.1f) return ATTENTION_PRIORITY_NONE;
    if (salience < 0.3f) return ATTENTION_PRIORITY_LOW;
    if (salience < 0.6f) return ATTENTION_PRIORITY_MEDIUM;
    if (salience < 0.85f) return ATTENTION_PRIORITY_HIGH;
    return ATTENTION_PRIORITY_CRITICAL;
}

static executive_action_t determine_action(
    const dragonfly_cognitive_bridge_t* bridge,
    float confidence,
    float time_to_intercept,
    float success_prob,
    bool is_evading
) {
    if (confidence < 0.1f) return EXEC_ACTION_NONE;

    if (success_prob < bridge->config.abort_threshold && is_evading) {
        return EXEC_ACTION_ABORT;
    }

    if (confidence < bridge->config.pursuit_threshold) {
        return EXEC_ACTION_TRACK;
    }

    if (confidence >= bridge->config.intercept_threshold && time_to_intercept < 0.5f) {
        return EXEC_ACTION_INTERCEPT;
    }

    return EXEC_ACTION_PURSUE;
}

static bool is_tracking_mode(dragonfly_mode_t mode) {
    return (mode == DRAGONFLY_MODE_TRACKING ||
            mode == DRAGONFLY_MODE_PURSUING ||
            mode == DRAGONFLY_MODE_INTERCEPTING);
}

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_cognitive_bridge_default_config(dragonfly_cognitive_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->mode = COGNITIVE_BRIDGE_ACTIVE;

    /* Salience weights */
    config->motion_salience_weight = 0.3f;
    config->velocity_salience_weight = 0.2f;
    config->direction_salience_weight = 0.3f;
    config->evasion_salience_weight = 0.2f;
    config->salience_threshold = 0.2f;
    config->salience_decay_rate = COGNITIVE_SALIENCE_DECAY_RATE;

    /* Attention */
    config->attention_base_width = 0.5f;  /* radians */
    config->attention_narrowing_factor = 0.5f;
    config->max_attention_foci = COGNITIVE_MAX_ATTENTION_FOCI;

    /* Working memory */
    config->wm_max_targets = COGNITIVE_MAX_WM_TARGETS;
    config->wm_refresh_interval_ms = COGNITIVE_WM_REFRESH_MS;
    config->wm_decay_time_ms = 2000.0f;

    /* Executive */
    config->pursuit_threshold = 0.4f;
    config->intercept_threshold = 0.7f;
    config->abort_threshold = 0.2f;
    config->allow_target_switching = true;

    /* Integration */
    config->sync_on_update = true;
    config->enable_feedback = false;

    return 0;
}

int dragonfly_cognitive_bridge_validate_config(const dragonfly_cognitive_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_bridge_validate_config: config is NULL");
        return -1;
    }

    if (config->salience_threshold < 0 || config->salience_threshold > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cognitive_bridge_validate_config: salience_threshold out of range [0, 1]");
        return -1;
    }
    if (config->salience_decay_rate < 0 || config->salience_decay_rate > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cognitive_bridge_validate_config: salience_decay_rate out of range [0, 1]");
        return -1;
    }
    if (config->attention_base_width <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cognitive_bridge_validate_config: attention_base_width <= 0");
        return -1;
    }
    if (config->max_attention_foci == 0 || config->max_attention_foci > COGNITIVE_MAX_ATTENTION_FOCI) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cognitive_bridge_validate_config: max_attention_foci invalid");
        return -1;
    }
    if (config->wm_max_targets == 0 || config->wm_max_targets > COGNITIVE_MAX_WM_TARGETS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cognitive_bridge_validate_config: wm_max_targets invalid");
        return -1;
    }
    if (config->pursuit_threshold < 0 || config->pursuit_threshold > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cognitive_bridge_validate_config: pursuit_threshold out of range [0, 1]");
        return -1;
    }
    if (config->intercept_threshold < 0 || config->intercept_threshold > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cognitive_bridge_validate_config: intercept_threshold out of range [0, 1]");
        return -1;
    }
    if (config->abort_threshold < 0 || config->abort_threshold > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cognitive_bridge_validate_config: abort_threshold out of range [0, 1]");
        return -1;
    }

    return 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_cognitive_bridge_t* dragonfly_cognitive_bridge_create(
    dragonfly_system_t* dragonfly,
    const dragonfly_cognitive_config_t* config
) {
    dragonfly_cognitive_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_cognitive_bridge_create: failed to allocate bridge");
        return NULL;
    }

    if (config) {
        if (dragonfly_cognitive_bridge_validate_config(config) != 0) {
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cognitive_bridge_create: invalid config");
            return NULL;
        }
        bridge->config = *config;
    } else {
        dragonfly_cognitive_bridge_default_config(&bridge->config);
    }

    bridge->dragonfly = dragonfly;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_cognitive") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_cognitive_bridge_create: failed to init base");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize executive state */
    bridge->exec_state.current_action = EXEC_ACTION_NONE;
    bridge->exec_state.recommended_action = EXEC_ACTION_NONE;

    bridge->initialized = true;
    return bridge;
}

void dragonfly_cognitive_bridge_destroy(dragonfly_cognitive_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_cognitive");
    bridge->initialized = false;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int dragonfly_cognitive_bridge_reset(dragonfly_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_bridge_reset: bridge is NULL or not initialized");
        return -1;
    }

    bridge->num_tracked_targets = 0;
    bridge->most_salient_target = 0;
    bridge->most_salient_value = 0;

    bridge->num_attention_foci = 0;
    memset(&bridge->primary_focus, 0, sizeof(bridge->primary_focus));

    bridge->wm_num_entries = 0;
    bridge->wm_last_refresh_us = 0;

    bridge->exec_state.current_action = EXEC_ACTION_NONE;
    bridge->exec_state.recommended_action = EXEC_ACTION_NONE;
    bridge->exec_state.active_target_id = 0;
    bridge->last_decision_us = 0;

    return 0;
}

//=============================================================================
// Cognitive System Registration
//=============================================================================

int dragonfly_cognitive_register_salience(
    dragonfly_cognitive_bridge_t* bridge,
    salience_evaluator_t salience
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_register_salience: bridge is NULL or not initialized");
        return -1;
    }
    bridge->salience = salience;
    return 0;
}

int dragonfly_cognitive_register_attention(
    dragonfly_cognitive_bridge_t* bridge,
    attention_system_t attention
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_register_attention: bridge is NULL or not initialized");
        return -1;
    }
    bridge->attention = attention;
    return 0;
}

int dragonfly_cognitive_register_working_memory(
    dragonfly_cognitive_bridge_t* bridge,
    working_memory_t wm
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_register_working_memory: bridge is NULL or not initialized");
        return -1;
    }
    bridge->wm = wm;
    return 0;
}

int dragonfly_cognitive_register_executive(
    dragonfly_cognitive_bridge_t* bridge,
    executive_control_t executive
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_register_executive: bridge is NULL or not initialized");
        return -1;
    }
    bridge->executive = executive;
    return 0;
}

//=============================================================================
// Salience Functions
//=============================================================================

int dragonfly_cognitive_compute_salience(
    dragonfly_cognitive_bridge_t* bridge,
    uint32_t target_id,
    target_salience_t* salience
) {
    if (!bridge || !bridge->initialized || !salience) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_compute_salience: bridge, salience is NULL or bridge not initialized");
        return -1;
    }

    memset(salience, 0, sizeof(*salience));

    /* Get target info from dragonfly */
    if (bridge->dragonfly) {
        dragonfly_target_info_t target;
        if (dragonfly_get_primary_target(bridge->dragonfly, &target) == 0 &&
            target.id == target_id) {
            /* Compute individual salience components */
            float speed = vec3_magnitude(target.velocity);

            /* Motion salience: 1.0 if tracking, scaled by state */
            salience->motion_salience = (target.state >= TRACK_STATE_ACQUIRING) ? 1.0f : 0.0f;
            salience->velocity_salience = compute_motion_salience(speed, 10.0f);

            /* Direction toward origin (self) */
            float to_self[3] = {-target.position[0], -target.position[1], -target.position[2]};
            salience->direction_salience = compute_direction_salience(target.velocity, to_self);

            /* Evasion salience from evasion type */
            salience->evasion_salience = (target.evasion_type != EVASION_NONE) ? 0.8f : 0.0f;

            /* Surprise and novelty would need history - use threat level as proxy */
            salience->surprise = 0.0f;
            salience->novelty = 0.0f;
            salience->urgency = target.threat_level;

            /* Combined salience */
            salience->combined_salience =
                bridge->config.motion_salience_weight * salience->motion_salience +
                bridge->config.velocity_salience_weight * salience->velocity_salience +
                bridge->config.direction_salience_weight * salience->direction_salience +
                bridge->config.evasion_salience_weight * salience->evasion_salience;

            salience->combined_salience = nimcp_clampf(salience->combined_salience, 0, 1);
        }
    }

    return 0;
}

int dragonfly_cognitive_update_salience(dragonfly_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_update_salience: bridge is NULL or not initialized");
        return -1;
    }

    uint64_t start_time = get_time_us();

    /* Get active target from dragonfly */
    if (bridge->dragonfly) {
        dragonfly_mode_t mode = dragonfly_get_mode(bridge->dragonfly);
        if (is_tracking_mode(mode)) {
            dragonfly_target_info_t target;
            if (dragonfly_get_primary_target(bridge->dragonfly, &target) == 0) {
                /* Update salience for active target */
                target_salience_t salience;
                if (dragonfly_cognitive_compute_salience(bridge, target.id, &salience) == 0) {
                    /* Store in tracked targets */
                    bool found = false;
                    for (uint32_t i = 0; i < bridge->num_tracked_targets; i++) {
                        if (bridge->target_ids[i] == target.id) {
                            bridge->target_saliences[i] = salience;
                            found = true;
                            break;
                        }
                    }
                    if (!found && bridge->num_tracked_targets < COGNITIVE_MAX_WM_TARGETS) {
                        bridge->target_ids[bridge->num_tracked_targets] = target.id;
                        bridge->target_saliences[bridge->num_tracked_targets] = salience;
                        bridge->num_tracked_targets++;
                    }

                    /* Update most salient */
                    if (salience.combined_salience > bridge->most_salient_value) {
                        bridge->most_salient_target = target.id;
                        bridge->most_salient_value = salience.combined_salience;
                    }
                }
            }
        }
    }

    /* Apply decay to all saliences */
    for (uint32_t i = 0; i < bridge->num_tracked_targets; i++) {
        bridge->target_saliences[i].combined_salience *= bridge->config.salience_decay_rate;
    }

    /* Update stats */
    bridge->stats.salience_updates++;
    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.total_processing_time_us += elapsed;

    /* Update average salience */
    float total_salience = 0;
    for (uint32_t i = 0; i < bridge->num_tracked_targets; i++) {
        total_salience += bridge->target_saliences[i].combined_salience;
    }
    if (bridge->num_tracked_targets > 0) {
        bridge->stats.avg_salience = total_salience / bridge->num_tracked_targets;
    }

    return 0;
}

int dragonfly_cognitive_get_most_salient(
    const dragonfly_cognitive_bridge_t* bridge,
    uint32_t* target_id,
    float* salience
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_get_most_salient: bridge is NULL or not initialized");
        return -1;
    }

    if (target_id) *target_id = bridge->most_salient_target;
    if (salience) *salience = bridge->most_salient_value;
    return 0;
}

//=============================================================================
// Attention Functions
//=============================================================================

int dragonfly_cognitive_update_attention(dragonfly_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_update_attention: bridge is NULL or not initialized");
        return -1;
    }

    uint64_t start_time = get_time_us();

    if (bridge->dragonfly) {
        dragonfly_mode_t mode = dragonfly_get_mode(bridge->dragonfly);
        if (is_tracking_mode(mode)) {
            dragonfly_target_info_t target;
            if (dragonfly_get_primary_target(bridge->dragonfly, &target) == 0) {
                /* Create attention focus from tracking state */
                bridge->primary_focus.focus_position[0] = target.position[0];
                bridge->primary_focus.focus_position[1] = target.position[1];
                bridge->primary_focus.focus_position[2] = target.position[2];

                /* Direction from position (azimuth) */
                bridge->primary_focus.focus_direction = atan2f(target.position[1], target.position[0]);

                /* Attention narrows with confidence */
                bridge->primary_focus.focus_width = bridge->config.attention_base_width *
                    (1.0f - bridge->config.attention_narrowing_factor * target.confidence);

                /* Priority from salience */
                float salience = bridge->most_salient_value;
                bridge->primary_focus.priority = compute_priority_from_salience(salience);
                bridge->primary_focus.confidence = target.confidence;
                bridge->primary_focus.target_id = target.id;

                /* Store as first focus */
                bridge->attention_foci[0] = bridge->primary_focus;
                bridge->num_attention_foci = 1;
            } else {
                bridge->primary_focus.priority = ATTENTION_PRIORITY_NONE;
                bridge->primary_focus.confidence = 0;
                bridge->num_attention_foci = 0;
            }
        } else {
            bridge->primary_focus.priority = ATTENTION_PRIORITY_NONE;
            bridge->primary_focus.confidence = 0;
            bridge->num_attention_foci = 0;
        }
    }

    /* Update stats */
    bridge->stats.attention_updates++;
    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.total_processing_time_us += elapsed;

    if (bridge->num_attention_foci > 0) {
        bridge->stats.avg_attention_width = bridge->primary_focus.focus_width;
    }

    return 0;
}

int dragonfly_cognitive_get_attention_focus(
    const dragonfly_cognitive_bridge_t* bridge,
    dragonfly_attention_focus_t* focus
) {
    if (!bridge || !bridge->initialized || !focus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_get_attention_focus: bridge, focus is NULL or bridge not initialized");
        return -1;
    }
    *focus = bridge->primary_focus;
    return 0;
}

int dragonfly_cognitive_get_attention_foci(
    const dragonfly_cognitive_bridge_t* bridge,
    dragonfly_attention_focus_t* foci,
    uint32_t max_foci,
    uint32_t* num_foci
) {
    if (!bridge || !bridge->initialized || !foci || !num_foci) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_get_attention_foci: bridge, foci, num_foci is NULL or bridge not initialized");
        return -1;
    }

    uint32_t count = bridge->num_attention_foci;
    if (count > max_foci) count = max_foci;

    memcpy(foci, bridge->attention_foci, count * sizeof(dragonfly_attention_focus_t));
    *num_foci = count;
    return 0;
}

int dragonfly_cognitive_set_attention_priority(
    dragonfly_cognitive_bridge_t* bridge,
    uint32_t target_id,
    dragonfly_attention_priority_t priority
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_set_attention_priority: bridge is NULL or not initialized");
        return -1;
    }

    /* Update focus if it matches target */
    if (bridge->primary_focus.target_id == target_id) {
        bridge->primary_focus.priority = priority;
        if (bridge->num_attention_foci > 0) {
            bridge->attention_foci[0].priority = priority;
        }
    }

    return 0;
}

//=============================================================================
// Working Memory Functions
//=============================================================================

int dragonfly_cognitive_update_working_memory(dragonfly_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_update_working_memory: bridge is NULL or not initialized");
        return -1;
    }

    uint64_t now = get_time_us();
    uint64_t start_time = now;

    /* Check refresh interval */
    float elapsed_ms = (float)(now - bridge->wm_last_refresh_us) / 1000.0f;
    if (elapsed_ms < bridge->config.wm_refresh_interval_ms && bridge->wm_last_refresh_us > 0) {
        return 0;  /* Not time to refresh yet */
    }

    if (bridge->dragonfly) {
        dragonfly_mode_t mode = dragonfly_get_mode(bridge->dragonfly);
        if (is_tracking_mode(mode)) {
            dragonfly_target_info_t target;
            if (dragonfly_get_primary_target(bridge->dragonfly, &target) == 0) {
                /* Find or create WM entry */
                bool found = false;
                for (uint32_t i = 0; i < bridge->wm_num_entries; i++) {
                    if (bridge->wm_entries[i].target_id == target.id) {
                        /* Update existing entry */
                        memcpy(bridge->wm_entries[i].position, target.position, sizeof(target.position));
                        memcpy(bridge->wm_entries[i].velocity, target.velocity, sizeof(target.velocity));
                        memcpy(bridge->wm_entries[i].predicted_position, target.predicted_position, sizeof(target.predicted_position));
                        bridge->wm_entries[i].confidence = target.confidence;
                        bridge->wm_entries[i].last_update_us = now;
                        bridge->wm_entries[i].update_count++;
                        bridge->wm_entries[i].is_evading = (target.evasion_type != EVASION_NONE);
                        found = true;
                        break;
                    }
                }

                if (!found && bridge->wm_num_entries < bridge->config.wm_max_targets) {
                    /* Create new entry */
                    wm_target_entry_t* entry = &bridge->wm_entries[bridge->wm_num_entries];
                    entry->target_id = target.id;
                    memcpy(entry->position, target.position, sizeof(target.position));
                    memcpy(entry->velocity, target.velocity, sizeof(target.velocity));
                    memcpy(entry->predicted_position, target.predicted_position, sizeof(target.predicted_position));
                    entry->confidence = target.confidence;
                    entry->last_update_us = now;
                    entry->update_count = 1;
                    entry->is_evading = (target.evasion_type != EVASION_NONE);
                    entry->evasion_pattern = 0;
                    bridge->wm_num_entries++;
                }
            }
        }
    }

    /* Decay old entries */
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < bridge->wm_num_entries; i++) {
        float age_ms = (float)(now - bridge->wm_entries[i].last_update_us) / 1000.0f;
        if (age_ms < bridge->config.wm_decay_time_ms) {
            /* Keep entry */
            if (write_idx != i) {
                bridge->wm_entries[write_idx] = bridge->wm_entries[i];
            }
            write_idx++;
        }
    }
    bridge->wm_num_entries = write_idx;

    bridge->wm_last_refresh_us = now;

    /* Update stats */
    bridge->stats.wm_updates++;
    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.total_processing_time_us += elapsed;
    bridge->stats.avg_wm_occupancy = (float)bridge->wm_num_entries / (float)bridge->config.wm_max_targets;

    return 0;
}

int dragonfly_cognitive_wm_get_target(
    const dragonfly_cognitive_bridge_t* bridge,
    uint32_t target_id,
    wm_target_entry_t* entry
) {
    if (!bridge || !bridge->initialized || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_wm_get_target: bridge, entry is NULL or bridge not initialized");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->wm_num_entries; i++) {
        if (bridge->wm_entries[i].target_id == target_id) {
            *entry = bridge->wm_entries[i];
            return 0;
        }
    }
    return -1;  /* Target not found */
}

int dragonfly_cognitive_wm_get_all_targets(
    const dragonfly_cognitive_bridge_t* bridge,
    wm_target_entry_t* entries,
    uint32_t max_entries,
    uint32_t* num_entries
) {
    if (!bridge || !bridge->initialized || !entries || !num_entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_wm_get_all_targets: bridge, entries, num_entries is NULL or bridge not initialized");
        return -1;
    }

    uint32_t count = bridge->wm_num_entries;
    if (count > max_entries) count = max_entries;

    memcpy(entries, bridge->wm_entries, count * sizeof(wm_target_entry_t));
    *num_entries = count;
    return 0;
}

float dragonfly_cognitive_wm_get_occupancy(const dragonfly_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) return 0;
    return (float)bridge->wm_num_entries / (float)bridge->config.wm_max_targets;
}

int dragonfly_cognitive_wm_clear(dragonfly_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_wm_clear: bridge is NULL or not initialized");
        return -1;
    }
    bridge->wm_num_entries = 0;
    return 0;
}

//=============================================================================
// Executive Control Functions
//=============================================================================

int dragonfly_cognitive_update_executive(dragonfly_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_update_executive: bridge is NULL or not initialized");
        return -1;
    }

    uint64_t start_time = get_time_us();

    if (bridge->dragonfly) {
        dragonfly_mode_t mode = dragonfly_get_mode(bridge->dragonfly);
        if (is_tracking_mode(mode)) {
            dragonfly_target_info_t target;
            if (dragonfly_get_primary_target(bridge->dragonfly, &target) == 0) {
                bridge->exec_state.active_target_id = target.id;
                bridge->exec_state.action_confidence = target.confidence;
                bridge->exec_state.time_to_intercept_ms = target.time_to_intercept_s * 1000.0f;
                bridge->exec_state.success_probability = target.confidence * 0.8f;  /* Conservative estimate */

                bool is_evading = (target.evasion_type != EVASION_NONE);

                /* Determine recommended action */
                bridge->exec_state.recommended_action = determine_action(
                    bridge,
                    target.confidence,
                    target.time_to_intercept_s,
                    bridge->exec_state.success_probability,
                    is_evading
                );

                /* Check for abort condition */
                if (bridge->exec_state.recommended_action == EXEC_ACTION_ABORT) {
                    bridge->exec_state.abort_recommended = true;
                    bridge->exec_state.abort_reason = "Low success probability with evasion";
                    bridge->stats.abort_decisions++;
                } else {
                    bridge->exec_state.abort_recommended = false;
                    bridge->exec_state.abort_reason = NULL;
                }

                /* Track decisions */
                if (bridge->exec_state.recommended_action == EXEC_ACTION_PURSUE) {
                    bridge->stats.pursue_decisions++;
                } else if (bridge->exec_state.recommended_action == EXEC_ACTION_INTERCEPT) {
                    bridge->stats.intercept_decisions++;
                }
            } else {
                bridge->exec_state.recommended_action = EXEC_ACTION_NONE;
                bridge->exec_state.action_confidence = 0;
            }
        } else {
            bridge->exec_state.recommended_action = EXEC_ACTION_NONE;
            bridge->exec_state.action_confidence = 0;
        }
    }

    bridge->last_decision_us = start_time;

    /* Update stats */
    bridge->stats.executive_updates++;
    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.total_processing_time_us += elapsed;

    return 0;
}

int dragonfly_cognitive_get_executive_state(
    const dragonfly_cognitive_bridge_t* bridge,
    executive_state_t* state
) {
    if (!bridge || !bridge->initialized || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_get_executive_state: bridge, state is NULL or bridge not initialized");
        return -1;
    }
    *state = bridge->exec_state;
    return 0;
}

executive_action_t dragonfly_cognitive_get_recommended_action(
    const dragonfly_cognitive_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) return EXEC_ACTION_NONE;
    return bridge->exec_state.recommended_action;
}

int dragonfly_cognitive_execute_action(dragonfly_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_execute_action: bridge is NULL or not initialized");
        return -1;
    }

    /* Execute the recommended action */
    bridge->exec_state.current_action = bridge->exec_state.recommended_action;

    /* In a real implementation, this would send commands to the dragonfly system */

    return 0;
}

int dragonfly_cognitive_request_abort(
    dragonfly_cognitive_bridge_t* bridge,
    const char* reason
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_request_abort: bridge is NULL or not initialized");
        return -1;
    }

    bridge->exec_state.current_action = EXEC_ACTION_ABORT;
    bridge->exec_state.abort_recommended = true;
    bridge->exec_state.abort_reason = reason;
    bridge->stats.abort_decisions++;

    return 0;
}

//=============================================================================
// Unified Update Functions
//=============================================================================

int dragonfly_cognitive_update_all(dragonfly_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_update_all: bridge is NULL or not initialized");
        return -1;
    }

    int result = 0;

    if (dragonfly_cognitive_update_salience(bridge) != 0) result = -1;
    if (dragonfly_cognitive_update_attention(bridge) != 0) result = -1;
    if (dragonfly_cognitive_update_working_memory(bridge) != 0) result = -1;
    if (dragonfly_cognitive_update_executive(bridge) != 0) result = -1;

    return result;
}

int dragonfly_cognitive_step(
    dragonfly_cognitive_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_step: bridge is NULL or not initialized");
        return -1;
    }
    (void)dt_ms;  /* Not currently used, but available for time-based updates */

    return dragonfly_cognitive_update_all(bridge);
}

//=============================================================================
// Statistics and Configuration
//=============================================================================

int dragonfly_cognitive_bridge_get_stats(
    const dragonfly_cognitive_bridge_t* bridge,
    cognitive_bridge_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_bridge_get_stats: bridge, stats is NULL or bridge not initialized");
        return -1;
    }

    *stats = bridge->stats;

    /* Compute average processing time */
    uint64_t total_updates = stats->salience_updates + stats->attention_updates +
                            stats->wm_updates + stats->executive_updates;
    if (total_updates > 0) {
        stats->avg_processing_time_us = (float)stats->total_processing_time_us / (float)total_updates;
    }

    return 0;
}

int dragonfly_cognitive_bridge_reset_stats(dragonfly_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_bridge_reset_stats: bridge is NULL or not initialized");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

int dragonfly_cognitive_bridge_get_config(
    const dragonfly_cognitive_bridge_t* bridge,
    dragonfly_cognitive_config_t* config
) {
    if (!bridge || !bridge->initialized || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_bridge_get_config: bridge, config is NULL or bridge not initialized");
        return -1;
    }
    *config = bridge->config;
    return 0;
}

int dragonfly_cognitive_bridge_set_config(
    dragonfly_cognitive_bridge_t* bridge,
    const dragonfly_cognitive_config_t* config
) {
    if (!bridge || !bridge->initialized || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cognitive_bridge_set_config: bridge, config is NULL or bridge not initialized");
        return -1;
    }
    if (dragonfly_cognitive_bridge_validate_config(config) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cognitive_bridge_set_config: invalid config");
        return -1;
    }
    bridge->config = *config;
    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* dragonfly_cognitive_action_name(executive_action_t action) {
    switch (action) {
        case EXEC_ACTION_NONE: return "none";
        case EXEC_ACTION_TRACK: return "track";
        case EXEC_ACTION_PURSUE: return "pursue";
        case EXEC_ACTION_INTERCEPT: return "intercept";
        case EXEC_ACTION_ABORT: return "abort";
        case EXEC_ACTION_SWITCH_TARGET: return "switch_target";
        default: return "unknown";
    }
}

const char* dragonfly_cognitive_priority_name(dragonfly_attention_priority_t priority) {
    switch (priority) {
        case ATTENTION_PRIORITY_NONE: return "none";
        case ATTENTION_PRIORITY_LOW: return "low";
        case ATTENTION_PRIORITY_MEDIUM: return "medium";
        case ATTENTION_PRIORITY_HIGH: return "high";
        case ATTENTION_PRIORITY_CRITICAL: return "critical";
        default: return "unknown";
    }
}

const char* dragonfly_cognitive_mode_name(cognitive_bridge_mode_t mode) {
    switch (mode) {
        case COGNITIVE_BRIDGE_PASSIVE: return "passive";
        case COGNITIVE_BRIDGE_ACTIVE: return "active";
        case COGNITIVE_BRIDGE_OVERRIDE: return "override";
        default: return "unknown";
    }
}
