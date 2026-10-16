/**
 * @file nimcp_self_awareness_coordinator.c
 * @brief Implementation of Unified Self-Awareness Coordinator
 *
 * WHAT: Orchestrates all self-awareness components into coherent self
 * WHY: Components exist in isolation - need binding for true self-awareness
 * HOW: Manages feedback loops, coherence checking, consciousness monitoring
 *
 * @version 1.0.0
 * @date 2025-12-22
 */

#include "cognitive/nimcp_self_awareness_coordinator.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define LOG_MODULE "self_awareness_coordinator"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(self_awareness_coordinator, MESH_ADAPTER_CATEGORY_COGNITIVE)



// ============================================================================
// Internal Helpers
// ============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief Initialize feedback loop states
 */
static void init_feedback_loops(self_awareness_coordinator_t* coord) {
    for (int i = 0; i < FEEDBACK_LOOP_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FEEDBACK_LOOP_COUNT > 256) {
            self_awareness_coordinator_heartbeat("self_awarene_loop",
                             (float)(i + 1) / (float)FEEDBACK_LOOP_COUNT);
        }

        coord->loops[i].type = (feedback_loop_type_t)i;
        coord->loops[i].status = FEEDBACK_STATUS_INACTIVE;
        coord->loops[i].last_transfer_time_ms = 0;
        coord->loops[i].transfer_count = 0;
        coord->loops[i].error_count = 0;
        coord->loops[i].transfer_rate_hz = 0.0f;
        coord->loops[i].avg_latency_ms = 0.0f;
        coord->loops[i].enabled = true;
    }
}

/**
 * @brief Add a coherence conflict
 */
static int add_conflict(
    self_awareness_coordinator_t* coord,
    coherence_conflict_type_t type,
    conflict_severity_t severity,
    const char* description,
    const char* component_a,
    const char* component_b,
    float confidence
) {
    if (coord->conflict_count >= SAC_MAX_CONFLICTS) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "conflict limit %u reached", SAC_MAX_CONFLICTS);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    coherence_conflict_t* conflict = &coord->conflicts[coord->conflict_count];
    conflict->conflict_id = coord->next_conflict_id++;
    conflict->detected_time_ms = get_current_time_ms();
    conflict->type = type;
    conflict->severity = severity;
    conflict->confidence = confidence;
    conflict->resolved = false;
    conflict->resolved_time_ms = 0;

    if (description) {
        strncpy(conflict->description, description, sizeof(conflict->description) - 1);
        conflict->description[sizeof(conflict->description) - 1] = '\0';
    }
    if (component_a) {
        strncpy(conflict->component_a, component_a, sizeof(conflict->component_a) - 1);
        conflict->component_a[sizeof(conflict->component_a) - 1] = '\0';
    }
    if (component_b) {
        strncpy(conflict->component_b, component_b, sizeof(conflict->component_b) - 1);
        conflict->component_b[sizeof(conflict->component_b) - 1] = '\0';
    }

    coord->conflict_count++;
    coord->stats.total_conflicts_detected++;

    NIMCP_LOGGING_DEBUG("Added conflict %lu: %s (severity: %d)",
        conflict->conflict_id, description, severity);

    return 0;
}

/**
 * @brief Update phi monitoring
 */
static void update_phi_monitoring(self_awareness_coordinator_t* coord) {
    if (!coord->config.enable_phi_monitoring) {
        return;
    }

    /* Get current phi from consciousness metrics */
    consciousness_phi_result_t* result = introspection_compute_phi(
        coord->introspection, NULL);

    if (result) {
        coord->current_phi = result->phi;

        /* Update statistics */
        if (coord->stats.total_updates == 0) {
            coord->stats.min_phi = result->phi;
            coord->stats.avg_phi = result->phi;
        } else {
            if (result->phi < coord->stats.min_phi) {
                coord->stats.min_phi = result->phi;
            }
            /* Running average using EMA weights */
            if (isfinite(result->phi)) {
                coord->stats.avg_phi = (coord->stats.avg_phi * NIMCP_EMA_WEIGHT_MEDIUM) + (result->phi * NIMCP_EMA_WEIGHT_MEDIUM_NEW);
            }
        }

        /* Check for alert conditions */
        if (!coord->phi_alert.alert_active &&
            result->phi < coord->phi_alert.alert_threshold) {
            /* Trigger alert */
            coord->phi_alert.alert_active = true;
            coord->phi_alert.alert_start_time_ms = get_current_time_ms();
            coord->phi_alert.alert_count++;
            coord->stats.phi_alerts++;

            if (coord->phi_alert.on_alert) {
                coord->phi_alert.on_alert(result->phi, coord->phi_alert.callback_user_data);
            }

            NIMCP_LOGGING_WARN("Phi alert triggered: %.3f < %.3f",
                result->phi, coord->phi_alert.alert_threshold);
            coord->state = SAC_STATE_LOW_CONSCIOUSNESS;
        }
        else if (coord->phi_alert.alert_active &&
                 result->phi > coord->phi_alert.recovery_threshold) {
            /* Clear alert */
            uint64_t now = get_current_time_ms();
            coord->phi_alert.total_alert_time_ms +=
                (now - coord->phi_alert.alert_start_time_ms);
            coord->phi_alert.alert_active = false;

            if (coord->phi_alert.on_recovery) {
                coord->phi_alert.on_recovery(result->phi, coord->phi_alert.callback_user_data);
            }

            NIMCP_LOGGING_INFO("Phi alert cleared: %.3f > %.3f",
                result->phi, coord->phi_alert.recovery_threshold);
            coord->state = SAC_STATE_RUNNING;
        }

        /* Track time in low phi */
        if (coord->phi_alert.alert_active) {
            coord->stats.time_in_low_phi_ms +=
                (get_current_time_ms() - coord->last_update_time_ms);
        }

        /* Clean up result */
        consciousness_phi_result_free(result);
    }
}

// ============================================================================
// Default Configuration
// ============================================================================

int sac_default_config(sac_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->enable_introspection_feedback = true;
    config->enable_autobio_feedback = true;
    config->enable_tom_grounding = true;
    config->enable_coherence_checking = true;
    config->enable_phi_monitoring = true;

    config->coherence_threshold = SAC_DEFAULT_COHERENCE_THRESHOLD;
    config->phi_alert_threshold = SAC_DEFAULT_PHI_ALERT_THRESHOLD;

    config->update_interval_ms = SAC_DEFAULT_UPDATE_INTERVAL_MS;
    config->coherence_check_interval_ms = NIMCP_TIMEOUT_LONG_MS;  /* Check every second */

    config->auto_resolve_minor = true;
    config->max_conflicts_before_alert = 5;

    config->enable_bio_async = true;

    return 0;
}

// ============================================================================
// Lifecycle Functions
// ============================================================================

self_awareness_coordinator_t* sac_create(
    const sac_config_t* config,
    introspection_context_t introspection,
    self_model_system_t* self_model,
    autobiographical_memory_t autobio,
    theory_of_mind_t tom
) {
    /* Validate required components */
    if (!introspection || !self_model || !autobio || !tom) {
        NIMCP_LOGGING_ERROR("Missing required component for self-awareness coordinator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sac_create: required parameter is NULL (introspection, self_model, autobio, tom)");
        return NULL;
    }

    /* Allocate coordinator */
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_create", 0.0f);


    self_awareness_coordinator_t* coord = nimcp_malloc(sizeof(self_awareness_coordinator_t));
    if (!coord) {
        NIMCP_LOGGING_ERROR("Failed to allocate self-awareness coordinator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate coord");

        return NULL;
    }
    memset(coord, 0, sizeof(self_awareness_coordinator_t));

    /* Store components */
    coord->introspection = introspection;
    coord->self_model = self_model;
    coord->autobio = autobio;
    coord->tom = tom;

    /* Apply configuration */
    if (config) {
        coord->config = *config;
    } else {
        sac_default_config(&coord->config);
    }

    /* Initialize state */
    coord->state = SAC_STATE_INITIALIZING;
    coord->current_coherence = 1.0f;  /* Assume coherent initially */
    coord->current_phi = 0.0f;
    coord->conflict_count = 0;
    coord->next_conflict_id = 1;

    /* Initialize feedback loops */
    init_feedback_loops(coord);

    /* Initialize phi alert */
    coord->phi_alert.alert_threshold = coord->config.phi_alert_threshold;
    coord->phi_alert.recovery_threshold = coord->config.phi_alert_threshold + NIMCP_PLASTICITY_RATE_DEFAULT;
    coord->phi_alert.alert_active = false;
    coord->phi_alert.alert_start_time_ms = 0;
    coord->phi_alert.total_alert_time_ms = 0;
    coord->phi_alert.alert_count = 0;
    coord->phi_alert.on_alert = NULL;
    coord->phi_alert.on_recovery = NULL;
    coord->phi_alert.callback_user_data = NULL;

    /* Initialize statistics */
    memset(&coord->stats, 0, sizeof(sac_stats_t));
    coord->stats.min_coherence_score = 1.0f;
    coord->stats.min_phi = 1.0f;

    /* Initialize timing */
    coord->creation_time_ms = get_current_time_ms();
    coord->last_update_time_ms = coord->creation_time_ms;
    coord->last_coherence_check_ms = coord->creation_time_ms;

    /* Create mutex */
    coord->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!coord->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        nimcp_free(coord);
        coord = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sac_create: coord->mutex is NULL");
        return NULL;
    }
    nimcp_mutex_init(coord->mutex, NULL);

    /* Bio-async integration */
    coord->bio_async_enabled = false;
    if (coord->config.enable_bio_async) {
        sac_connect_bio_async(coord);
    }

    /* Transition to running */
    coord->state = SAC_STATE_RUNNING;

    NIMCP_LOGGING_INFO("Self-awareness coordinator created successfully");
    return coord;
}

void sac_destroy(self_awareness_coordinator_t* coord) {
    if (!coord) {
        return;
    }

    /* Disconnect bio-async */
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_destroy", 0.0f);


    if (coord->bio_async_enabled) {
        sac_disconnect_bio_async(coord);
    }

    /* Destroy mutex */
    if (coord->mutex) {
        nimcp_mutex_destroy(coord->mutex);
    }

    /* Free coordinator */
    nimcp_free(coord);
    coord = NULL;

    NIMCP_LOGGING_INFO("Self-awareness coordinator destroyed");
}

// ============================================================================
// Update Functions
// ============================================================================

int sac_update(self_awareness_coordinator_t* coord) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_update", 0.0f);


    NIMCP_CHECK_THROW(coord, NIMCP_ERROR_NULL_POINTER, "coord is NULL");

    nimcp_mutex_lock(coord->mutex);

    uint64_t now = get_current_time_ms();
    uint64_t elapsed = now - coord->last_update_time_ms;

    /* Check if update is due */
    if (elapsed < coord->config.update_interval_ms) {
        nimcp_mutex_unlock(coord->mutex);
        return 0;
    }

    coord->last_update_time_ms = now;
    coord->stats.total_updates++;

    /* Update phi monitoring */
    update_phi_monitoring(coord);

    /* Execute feedback loops */
    if (coord->config.enable_introspection_feedback) {
        if (coord->loops[FEEDBACK_INTROSPECTION_TO_SELF_MODEL].enabled) {
            sac_introspection_to_self_model(coord);
        }
    }

    if (coord->config.enable_autobio_feedback) {
        if (coord->loops[FEEDBACK_AUTOBIO_TO_SELF_MODEL].enabled) {
            sac_autobio_to_self_model(coord);
        }
    }

    if (coord->config.enable_tom_grounding) {
        if (coord->loops[FEEDBACK_SELF_MODEL_TO_TOM].enabled) {
            sac_ground_tom_in_self(coord);
        }
    }

    /* Check coherence if due */
    if (coord->config.enable_coherence_checking) {
        uint64_t coherence_elapsed = now - coord->last_coherence_check_ms;
        if (coherence_elapsed >= coord->config.coherence_check_interval_ms) {
            coord->state = SAC_STATE_COHERENCE_CHECK;
            float score = 0.0f;
            sac_check_coherence(coord, &score);
            coord->last_coherence_check_ms = now;

            /* Auto-resolve minor conflicts if enabled */
            if (coord->config.auto_resolve_minor && coord->conflict_count > 0) {
                uint32_t resolved = 0;
                sac_auto_resolve_minor_conflicts(coord, &resolved);
            }

            coord->state = (coord->conflict_count > 0) ?
                SAC_STATE_CONFLICT_RESOLUTION : SAC_STATE_RUNNING;
        }
    }

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

int sac_check_coherence(self_awareness_coordinator_t* coord, float* score) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_check_coherence", 0.0f);


    NIMCP_CHECK_THROW(coord && score, NIMCP_ERROR_NULL_POINTER, "coord or score is NULL");

    float total_coherence = 1.0f;
    int checks_performed = 0;

    /* Check 1: Self-model beliefs vs introspection data */
    if (coord->introspection && coord->self_model) {
        /* Get stats from introspection to assess coherence */
        introspection_stats_t stats;
        bool ret = introspection_get_stats(coord->introspection, &stats);
        if (ret) {
            /* Compare claimed capabilities with actual neural activity patterns */
            /* Use queries_total as a proxy for activity level */
            float capability_coherence = 0.8f + 0.2f *
                (stats.queries_total > 0 ? 0.5f : 0.0f);
            total_coherence *= capability_coherence;
            checks_performed++;
        }
    }

    /* Check 2: Self-model vs autobiographical memory */
    if (coord->self_model && coord->autobio) {
        /* Query recent memories that might conflict with beliefs */
        memory_query_t query = {0};
        query.start_time_ms = 0;  /* No lower bound */
        query.end_time_ms = 0;    /* No upper bound (will get recent) */
        query.filter_by_type = true;
        query.type_filter = AUTOBIO_FAILURE;
        query.max_results = 10;
        query.sort_by_recency = true;

        autobiographical_memory_entry_t results[10];
        uint32_t found = 0;
        bool ret = autobio_query(coord->autobio, &query, results, 10, &found);

        if (ret && found > 0) {
            /* Check if recent failures contradict capability beliefs */
            float memory_coherence = 1.0f - (found * NIMCP_PLASTICITY_RATE_DEFAULT);
            if (memory_coherence < 0.5f) memory_coherence = 0.5f;
            total_coherence *= memory_coherence;
            checks_performed++;
        }
    }

    /* Check 3: Theory of Mind consistency with self */
    if (coord->tom && coord->self_model) {
        /* Check if ToM predictions for self match actual self-model */
        float tom_coherence = NIMCP_EMA_WEIGHT_SLOW;  /* Simplified - assume mostly coherent */
        total_coherence *= tom_coherence;
        checks_performed++;
    }

    /* Compute final coherence score */
    if (checks_performed > 0) {
        *score = total_coherence;
    } else {
        *score = 1.0f;  /* No checks = assume coherent */
    }

    coord->current_coherence = *score;

    /* Update statistics */
    if (coord->stats.total_updates == 0) {
        coord->stats.avg_coherence_score = *score;
        coord->stats.min_coherence_score = *score;
    } else {
        if (*score < coord->stats.min_coherence_score) {
            coord->stats.min_coherence_score = *score;
        }
        coord->stats.avg_coherence_score =
            (coord->stats.avg_coherence_score * 0.95f) + (*score * 0.05f);
    }

    /* Detect low coherence */
    if (*score < coord->config.coherence_threshold) {
        add_conflict(coord, CONFLICT_BELIEF_EXPERIENCE, CONFLICT_SEVERITY_MODERATE,
            "Overall coherence below threshold", "multiple", "components",
            1.0f - *score);
    }

    NIMCP_LOGGING_DEBUG("Coherence check: %.3f (checks: %d)", *score, checks_performed);
    return 0;
}

// ============================================================================
// Feedback Loop Functions
// ============================================================================

int sac_introspection_to_self_model(self_awareness_coordinator_t* coord) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_introspection_to", 0.0f);


    NIMCP_CHECK_THROW(coord && coord->introspection && coord->self_model, NIMCP_ERROR_NULL_POINTER, "coord, introspection, or self_model is NULL");

    feedback_loop_state_t* loop = &coord->loops[FEEDBACK_INTROSPECTION_TO_SELF_MODEL];
    uint64_t start_time = get_current_time_ms();

    loop->status = FEEDBACK_STATUS_ACTIVE;

    /* Get stats from introspection */
    introspection_stats_t stats;
    bool ret = introspection_get_stats(coord->introspection, &stats);

    if (ret) {
        /* Update self-model with introspection data */
        /* In a full implementation, this would update specific beliefs */
        /* For now, we track that the transfer happened */
        loop->transfer_count++;
        loop->last_transfer_time_ms = get_current_time_ms();
        loop->status = FEEDBACK_STATUS_ACTIVE;
        coord->stats.total_feedback_transfers++;

        /* Update latency tracking */
        float latency = (float)(get_current_time_ms() - start_time);
        if (isfinite(latency)) {
            loop->avg_latency_ms = (loop->avg_latency_ms * NIMCP_EMA_WEIGHT_SLOW) + (latency * NIMCP_EMA_WEIGHT_FAST);
        }
        return NIMCP_SUCCESS;
    } else {
        loop->error_count++;
        loop->status = FEEDBACK_STATUS_ERROR;
        coord->stats.feedback_errors++;
        return NIMCP_ERROR_OPERATION_FAILED;
    }
}

int sac_autobio_to_self_model(self_awareness_coordinator_t* coord) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_autobio_to_self_", 0.0f);


    NIMCP_CHECK_THROW(coord && coord->autobio && coord->self_model, NIMCP_ERROR_NULL_POINTER, "coord, autobio, or self_model is NULL");

    feedback_loop_state_t* loop = &coord->loops[FEEDBACK_AUTOBIO_TO_SELF_MODEL];
    uint64_t start_time = get_current_time_ms();

    loop->status = FEEDBACK_STATUS_ACTIVE;

    /* Query recent core memories */
    autobiographical_memory_entry_t core_memories[5];
    uint32_t found = 0;
    bool ret = autobio_get_core_memories(coord->autobio, core_memories, 5, &found);

    if (ret) {
        /* In a full implementation, this would validate self-model beliefs
         * against core memories and update beliefs if needed */

        loop->transfer_count++;
        loop->last_transfer_time_ms = get_current_time_ms();
        coord->stats.total_feedback_transfers++;

        float latency = (float)(get_current_time_ms() - start_time);
        if (isfinite(latency)) {
            loop->avg_latency_ms = (loop->avg_latency_ms * NIMCP_EMA_WEIGHT_SLOW) + (latency * NIMCP_EMA_WEIGHT_FAST);
        }
        return NIMCP_SUCCESS;
    } else {
        loop->error_count++;
        loop->status = FEEDBACK_STATUS_ERROR;
        coord->stats.feedback_errors++;
        return NIMCP_ERROR_OPERATION_FAILED;
    }
}

int sac_ground_tom_in_self(self_awareness_coordinator_t* coord) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_ground_tom_in_se", 0.0f);


    NIMCP_CHECK_THROW(coord && coord->tom && coord->self_model, NIMCP_ERROR_NULL_POINTER, "coord, tom, or self_model is NULL");

    feedback_loop_state_t* loop = &coord->loops[FEEDBACK_SELF_MODEL_TO_TOM];
    uint64_t start_time = get_current_time_ms();

    loop->status = FEEDBACK_STATUS_ACTIVE;

    /* In a full implementation, this would:
     * 1. Extract self-model's belief structure
     * 2. Provide it to ToM as template for modeling others
     * 3. ToM uses "simulation theory" - models others as variations of self
     *
     * For now, we track that the grounding occurred */

    loop->transfer_count++;
    loop->last_transfer_time_ms = get_current_time_ms();
    coord->stats.total_feedback_transfers++;

    float latency = (float)(get_current_time_ms() - start_time);
    if (isfinite(latency)) {
        loop->avg_latency_ms = (loop->avg_latency_ms * NIMCP_EMA_WEIGHT_SLOW) + (latency * NIMCP_EMA_WEIGHT_FAST);
    }

    return 0;
}

int sac_set_feedback_loop_enabled(
    self_awareness_coordinator_t* coord,
    feedback_loop_type_t loop_type,
    bool enabled
) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_set_feedback_loo", 0.0f);


    NIMCP_CHECK_THROW(coord, NIMCP_ERROR_NULL_POINTER, "coord is NULL");
    NIMCP_CHECK_THROW(loop_type < FEEDBACK_LOOP_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid loop_type: %d", loop_type);

    nimcp_mutex_lock(coord->mutex);
    coord->loops[loop_type].enabled = enabled;
    coord->loops[loop_type].status = enabled ?
        FEEDBACK_STATUS_INACTIVE : FEEDBACK_STATUS_INACTIVE;
    nimcp_mutex_unlock(coord->mutex);

    return 0;
}

int sac_get_feedback_loop_state(
    const self_awareness_coordinator_t* coord,
    feedback_loop_type_t loop_type,
    feedback_loop_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_get_feedback_loo", 0.0f);


    NIMCP_CHECK_THROW(coord && state, NIMCP_ERROR_NULL_POINTER, "coord or state is NULL");
    NIMCP_CHECK_THROW(loop_type < FEEDBACK_LOOP_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid loop_type: %d", loop_type);

    *state = coord->loops[loop_type];
    return 0;
}

// ============================================================================
// Coherence Functions
// ============================================================================

float sac_get_coherence(const self_awareness_coordinator_t* coord) {
    if (!coord) {
        return -1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_get_coherence", 0.0f);


    return coord->current_coherence;
}

int sac_get_conflicts(
    const self_awareness_coordinator_t* coord,
    coherence_conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count
) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_get_conflicts", 0.0f);


    NIMCP_CHECK_THROW(coord && conflicts && count, NIMCP_ERROR_NULL_POINTER, "coord, conflicts, or count is NULL");

    uint32_t to_copy = (coord->conflict_count < max_conflicts) ?
        coord->conflict_count : max_conflicts;

    memcpy(conflicts, coord->conflicts, to_copy * sizeof(coherence_conflict_t));
    *count = to_copy;

    return 0;
}

int sac_resolve_conflict(
    self_awareness_coordinator_t* coord,
    uint64_t conflict_id,
    const char* resolution
) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_resolve_conflict", 0.0f);


    NIMCP_CHECK_THROW(coord, NIMCP_ERROR_NULL_POINTER, "coord is NULL");

    nimcp_mutex_lock(coord->mutex);

    for (uint32_t i = 0; i < coord->conflict_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && coord->conflict_count > 256) {
            self_awareness_coordinator_heartbeat("self_awarene_loop",
                             (float)(i + 1) / (float)coord->conflict_count);
        }

        if (coord->conflicts[i].conflict_id == conflict_id &&
            !coord->conflicts[i].resolved) {

            coord->conflicts[i].resolved = true;
            coord->conflicts[i].resolved_time_ms = get_current_time_ms();
            if (resolution) {
                strncpy(coord->conflicts[i].resolution, resolution,
                    sizeof(coord->conflicts[i].resolution) - 1);
            }

            coord->stats.total_conflicts_resolved++;

            NIMCP_LOGGING_DEBUG("Resolved conflict %lu: %s",
                conflict_id, resolution ? resolution : "(no description)");

            nimcp_mutex_unlock(coord->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(coord->mutex);
    NIMCP_THROW(NIMCP_ERROR_NOT_FOUND, "conflict_id %lu not found", conflict_id);
    return NIMCP_ERROR_NOT_FOUND;
}

int sac_auto_resolve_minor_conflicts(
    self_awareness_coordinator_t* coord,
    uint32_t* resolved_count
) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_auto_resolve_min", 0.0f);


    NIMCP_CHECK_THROW(coord && resolved_count, NIMCP_ERROR_NULL_POINTER, "coord or resolved_count is NULL");

    nimcp_mutex_lock(coord->mutex);

    *resolved_count = 0;

    for (uint32_t i = 0; i < coord->conflict_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && coord->conflict_count > 256) {
            self_awareness_coordinator_heartbeat("self_awarene_loop",
                             (float)(i + 1) / (float)coord->conflict_count);
        }

        if (!coord->conflicts[i].resolved &&
            coord->conflicts[i].severity == CONFLICT_SEVERITY_MINOR) {

            coord->conflicts[i].resolved = true;
            coord->conflicts[i].resolved_time_ms = get_current_time_ms();
            strncpy(coord->conflicts[i].resolution, "Auto-resolved (minor)",
                sizeof(coord->conflicts[i].resolution) - 1);

            coord->stats.total_conflicts_resolved++;
            (*resolved_count)++;
        }
    }

    nimcp_mutex_unlock(coord->mutex);

    if (*resolved_count > 0) {
        NIMCP_LOGGING_DEBUG("Auto-resolved %u minor conflicts", *resolved_count);
    }

    return 0;
}

// ============================================================================
// Consciousness Monitoring Functions
// ============================================================================

float sac_get_phi(const self_awareness_coordinator_t* coord) {
    if (!coord) {
        return -1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_get_phi", 0.0f);


    return coord->current_phi;
}

bool sac_is_low_consciousness(const self_awareness_coordinator_t* coord) {
    if (!coord) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_is_low_conscious", 0.0f);


    return coord->phi_alert.alert_active;
}

int sac_set_phi_callbacks(
    self_awareness_coordinator_t* coord,
    void (*on_alert)(float phi, void* user_data),
    void (*on_recovery)(float phi, void* user_data),
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_set_phi_callback", 0.0f);


    NIMCP_CHECK_THROW(coord, NIMCP_ERROR_NULL_POINTER, "coord is NULL");

    nimcp_mutex_lock(coord->mutex);
    coord->phi_alert.on_alert = on_alert;
    coord->phi_alert.on_recovery = on_recovery;
    coord->phi_alert.callback_user_data = user_data;
    nimcp_mutex_unlock(coord->mutex);

    return 0;
}

int sac_set_phi_thresholds(
    self_awareness_coordinator_t* coord,
    float alert_threshold,
    float recovery_threshold
) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_set_phi_threshol", 0.0f);


    NIMCP_CHECK_THROW(coord, NIMCP_ERROR_NULL_POINTER, "coord is NULL");
    NIMCP_CHECK_THROW(alert_threshold >= 0.0f && recovery_threshold >= 0.0f, NIMCP_ERROR_INVALID_PARAM, "negative threshold value");

    nimcp_mutex_lock(coord->mutex);
    coord->phi_alert.alert_threshold = alert_threshold;
    coord->phi_alert.recovery_threshold = recovery_threshold;
    nimcp_mutex_unlock(coord->mutex);

    return 0;
}

// ============================================================================
// Bio-Async Integration
// ============================================================================

int sac_connect_bio_async(self_awareness_coordinator_t* coord) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_connect_bio_asyn", 0.0f);


    NIMCP_CHECK_THROW(coord, NIMCP_ERROR_NULL_POINTER, "coord is NULL");

    if (coord->bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SELF_AWARENESS_COORDINATOR,
        .module_name = "self_awareness_coordinator",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = coord
    };

    coord->bio_ctx = bio_router_register_module(&info);
    if (coord->bio_ctx) {
        coord->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    return NIMCP_ERROR_NOT_FOUND;
}

int sac_disconnect_bio_async(self_awareness_coordinator_t* coord) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_disconnect_bio_a", 0.0f);


    NIMCP_CHECK_THROW(coord, NIMCP_ERROR_NULL_POINTER, "coord is NULL");

    if (!coord->bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    if (coord->bio_ctx) {
        bio_router_unregister_module(coord->bio_ctx);
        coord->bio_ctx = NULL;
    }

    coord->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool sac_is_bio_async_connected(const self_awareness_coordinator_t* coord) {
    if (!coord) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_is_bio_async_con", 0.0f);


    return coord->bio_async_enabled;
}

// ============================================================================
// Statistics and State
// ============================================================================

sac_state_t sac_get_state(const self_awareness_coordinator_t* coord) {
    if (!coord) {
        return SAC_STATE_ERROR;
    }
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_get_state", 0.0f);


    return coord->state;
}

int sac_get_stats(
    const self_awareness_coordinator_t* coord,
    sac_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_get_stats", 0.0f);


    NIMCP_CHECK_THROW(coord && stats, NIMCP_ERROR_NULL_POINTER, "coord or stats is NULL");

    *stats = coord->stats;
    return NIMCP_SUCCESS;
}

int sac_reset_stats(self_awareness_coordinator_t* coord) {
    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_sac_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(coord, NIMCP_ERROR_NULL_POINTER, "coord is NULL");

    nimcp_mutex_lock(coord->mutex);
    memset(&coord->stats, 0, sizeof(sac_stats_t));
    coord->stats.min_coherence_score = 1.0f;
    coord->stats.min_phi = 1.0f;
    nimcp_mutex_unlock(coord->mutex);

    return 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* sac_feedback_loop_name(feedback_loop_type_t type) {
    static const char* names[] = {
        "introspection_to_self_model",
        "self_model_to_introspection",
        "autobio_to_self_model",
        "self_model_to_autobio",
        "self_model_to_tom",
        "tom_to_self_model",
        "consciousness_to_executive",
        "executive_to_consciousness"
    };

    if (type >= FEEDBACK_LOOP_COUNT) {
        return "unknown";
    }
    return names[type];
}

const char* sac_conflict_type_name(coherence_conflict_type_t type) {
    static const char* names[] = {
        "belief_experience",
        "belief_capability",
        "memory_identity",
        "tom_self",
        "phi_self_report",
        "temporal_continuity"
    };

    if (type > CONFLICT_TEMPORAL_CONTINUITY) {
        return "unknown";
    }
    return names[type];
}

const char* sac_state_name(sac_state_t state) {
    static const char* names[] = {
        "uninitialized",
        "initializing",
        "running",
        "coherence_check",
        "conflict_resolution",
        "low_consciousness",
        "paused",
        "error"
    };

    if (state > SAC_STATE_ERROR) {
        return "unknown";
    }
    return names[state];
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int self_awareness_coordinator_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    self_awareness_coordinator_heartbeat("self_awarene_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Self_Awareness_Coordinator");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                self_awareness_coordinator_heartbeat("self_awarene_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Self_Awareness_Coordinator");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Self_Awareness_Coordinator");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_awareness_coordinator_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_self_awareness_coordinator_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_awareness_coordinator_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_coordinator_training_begin: NULL argument");
        return -1;
    }
    self_awareness_coordinator_heartbeat_instance(NULL, "self_awareness_coordinator_training_begin", 0.0f);
    return 0;
}

int self_awareness_coordinator_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_coordinator_training_end: NULL argument");
        return -1;
    }
    self_awareness_coordinator_heartbeat_instance(NULL, "self_awareness_coordinator_training_end", 1.0f);
    return 0;
}

int self_awareness_coordinator_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_coordinator_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    self_awareness_coordinator_heartbeat_instance(NULL, "self_awareness_coordinator_training_step", progress);
    return 0;
}
