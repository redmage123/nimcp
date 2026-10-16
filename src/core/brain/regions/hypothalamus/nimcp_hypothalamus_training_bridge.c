/**
 * @file nimcp_hypothalamus_training_bridge.c
 * @brief Implementation of hypothalamus-training bridge for homeostatic learning
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_training_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "training/integration/nimcp_training_integration_hub.h"
#include "training/integration/nimcp_training_event_types.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_training_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_TRAINING_BRIDGE"


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Internal bridge structure
 */
struct hypo_training_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    hypo_training_bridge_config_t config;

    /* Connections */
    hypo_orchestrator_t orchestrator;
    training_integration_hub_t training_hub;
    uint32_t bridge_id;                     /**< ID from orchestrator registration */
    bool orchestrator_connected;
    bool hub_connected;

    /* Homeostatic state */
    hypo_training_homeostatic_state_t homeostatic;

    /* Drive state */
    hypo_training_drive_state_t drives;

    /* Current modulation */
    hypo_training_modulation_t modulation;

    /* Loss history for trend analysis */
    float loss_history[HYPO_TRAINING_MAX_LOSS_HISTORY];
    uint32_t loss_history_count;
    uint32_t loss_history_idx;

    /* Gradient history */
    float gradient_history[HYPO_TRAINING_MAX_GRADIENT_HISTORY];
    uint32_t gradient_history_count;
    uint32_t gradient_history_idx;

    /* Training progress tracking */
    uint32_t current_epoch;
    uint32_t total_batches;
    float cumulative_loss;
    uint64_t last_update_time;

    /* Statistics */
    hypo_training_bridge_stats_t stats;
    uint64_t creation_time;

    /* State flags */
    bool initialized;
    bool in_consolidation;
};

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static float compute_loss_trend(const hypo_training_bridge_t* bridge) {
    if (bridge->loss_history_count < 2) {
        return 0.0f;
    }

    /* Simple linear regression over recent losses */
    uint32_t count = bridge->loss_history_count;
    if (count > 10) count = 10; /* Use last 10 entries */

    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
    uint32_t start_idx = (bridge->loss_history_idx + HYPO_TRAINING_MAX_LOSS_HISTORY - count)
                         % HYPO_TRAINING_MAX_LOSS_HISTORY;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start_idx + i) % HYPO_TRAINING_MAX_LOSS_HISTORY;
        float x = (float)i;
        float y = bridge->loss_history[idx];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }

    float n = (float)count;
    float denominator = n * sum_x2 - sum_x * sum_x;
    if (fabsf(denominator) < 1e-6f) {
        return 0.0f;
    }

    return (n * sum_xy - sum_x * sum_y) / denominator;
}

static float compute_loss_variance(const hypo_training_bridge_t* bridge) {
    if (bridge->loss_history_count < 2) {
        return 0.0f;
    }

    uint32_t count = bridge->loss_history_count;
    if (count > 20) count = 20; /* Use last 20 entries */

    float mean = 0.0f;
    uint32_t start_idx = (bridge->loss_history_idx + HYPO_TRAINING_MAX_LOSS_HISTORY - count)
                         % HYPO_TRAINING_MAX_LOSS_HISTORY;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start_idx + i) % HYPO_TRAINING_MAX_LOSS_HISTORY;
        mean += bridge->loss_history[idx];
    }
    mean /= (float)count;

    float variance = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start_idx + i) % HYPO_TRAINING_MAX_LOSS_HISTORY;
        float diff = bridge->loss_history[idx] - mean;
        variance += diff * diff;
    }

    return variance / (float)count;
}

static hypo_training_state_t assess_training_state(hypo_training_bridge_t* bridge) {
    float deviation = fabsf(bridge->homeostatic.deviation);
    float tolerance = bridge->config.homeostatic_config.loss_tolerance;
    float trend = bridge->homeostatic.loss_trend;
    float variance = bridge->homeostatic.loss_variance;

    /* Check for critical state (NaN/Inf or extreme deviation) */
    if (!isfinite(bridge->homeostatic.current_loss) || deviation > tolerance * 5.0f) {
        return HYPO_TRAIN_STATE_CRITICAL;
    }

    /* Check for unstable state (high variance) */
    if (variance > tolerance * tolerance * 4.0f) {
        return HYPO_TRAIN_STATE_UNSTABLE;
    }

    /* Check for diverging state (loss increasing) */
    if (trend > 0.01f && deviation > tolerance) {
        return HYPO_TRAIN_STATE_DIVERGING;
    }

    /* Check for plateau (no improvement) */
    if (fabsf(trend) < 0.001f && bridge->homeostatic.epochs_since_improvement > 5) {
        return HYPO_TRAIN_STATE_PLATEAU;
    }

    /* Check for improving state */
    if (trend < -0.005f) {
        return HYPO_TRAIN_STATE_IMPROVING;
    }

    /* Default: healthy */
    return HYPO_TRAIN_STATE_HEALTHY;
}

static void update_drives_from_training(hypo_training_bridge_t* bridge) {
    hypo_training_state_t state = bridge->homeostatic.state;
    float deviation = bridge->homeostatic.deviation;
    float trend = bridge->homeostatic.loss_trend;

    /* Update safety drive based on training stability */
    switch (state) {
        case HYPO_TRAIN_STATE_CRITICAL:
            bridge->drives.safety_activation = 1.0f;
            break;
        case HYPO_TRAIN_STATE_DIVERGING:
        case HYPO_TRAIN_STATE_UNSTABLE:
            bridge->drives.safety_activation = nimcp_clampf(
                0.5f + fabsf(deviation) * 2.0f, 0.0f, 1.0f);
            break;
        case HYPO_TRAIN_STATE_PLATEAU:
            bridge->drives.safety_activation = 0.3f;
            break;
        default:
            bridge->drives.safety_activation = nimcp_clampf(
                0.1f + fabsf(deviation), 0.0f, 0.5f);
            break;
    }

    /* Update curiosity drive - inverse of safety when healthy */
    if (state == HYPO_TRAIN_STATE_HEALTHY || state == HYPO_TRAIN_STATE_IMPROVING) {
        bridge->drives.curiosity_activation = nimcp_clampf(
            0.5f + (1.0f - bridge->drives.safety_activation) * 0.5f, 0.0f, 1.0f);
    } else {
        bridge->drives.curiosity_activation = nimcp_clampf(
            0.3f - bridge->drives.safety_activation * 0.3f, 0.0f, 0.5f);
    }

    /* Update competence drive based on progress */
    if (trend < 0 && state != HYPO_TRAIN_STATE_PLATEAU) {
        /* Making progress - competence increasing */
        bridge->drives.competence_activation = nimcp_clampf(
            bridge->drives.competence_activation + 0.02f, 0.0f, 1.0f);
    } else if (state == HYPO_TRAIN_STATE_PLATEAU) {
        /* Plateau - competence decreasing */
        bridge->drives.competence_activation = nimcp_clampf(
            bridge->drives.competence_activation - 0.01f, 0.2f, 1.0f);
    }

    /* Compute derived states */
    bridge->drives.exploration_tendency =
        bridge->drives.curiosity_activation - bridge->drives.safety_activation;
    bridge->drives.learning_readiness =
        1.0f - bridge->drives.fatigue_level;
    bridge->drives.difficulty_readiness =
        bridge->drives.competence_activation * bridge->drives.learning_readiness;
}

static void compute_modulations(hypo_training_bridge_t* bridge) {
    hypo_training_modulation_t* mod = &bridge->modulation;
    hypo_training_drive_state_t* drives = &bridge->drives;
    hypo_training_drive_config_t* cfg = &bridge->config.drive_config;

    /* Learning rate modulation */
    float curiosity_effect = drives->curiosity_activation * (cfg->curiosity_lr_multiplier - 1.0f);
    float safety_effect = drives->safety_activation * (1.0f - cfg->safety_lr_reduction);
    float fatigue_effect = drives->fatigue_level * cfg->fatigue_lr_decay;

    mod->lr_multiplier = nimcp_clampf(
        1.0f + curiosity_effect - safety_effect - fatigue_effect,
        HYPO_TRAINING_MIN_PRECISION,
        HYPO_TRAINING_MAX_PRECISION
    );

    /* Batch size modulation (inverse of LR for stability) */
    mod->batch_size_multiplier = nimcp_clampf(
        2.0f - mod->lr_multiplier,
        0.5f, 2.0f
    );

    /* Gradient clipping modulation */
    mod->gradient_clip_multiplier = nimcp_clampf(
        1.0f - drives->safety_activation * (1.0f - cfg->safety_gradient_clip_mult),
        0.5f, 2.0f
    );

    /* Curriculum difficulty adjustment */
    mod->difficulty_adjustment = nimcp_clampf(
        drives->difficulty_readiness * cfg->competence_difficulty_weight - 0.5f,
        -1.0f, 1.0f
    );

    /* Sample priority boost (curiosity-driven) */
    mod->sample_priority_boost = nimcp_clampf(
        drives->curiosity_activation * 0.5f,
        0.0f, 1.0f
    );

    /* Checkpoint urgency */
    mod->checkpoint_urgency = nimcp_clampf(
        drives->safety_activation * 0.5f +
        (bridge->homeostatic.state == HYPO_TRAIN_STATE_IMPROVING ? 0.3f : 0.0f),
        0.0f, 1.0f
    );

    /* Multi-task weight shift (competence-driven) */
    mod->multi_task_weight_shift = nimcp_clampf(
        (drives->competence_activation - 0.5f) * 2.0f,
        -1.0f, 1.0f
    );

    /* Replay priority boost (safety-driven) */
    mod->replay_priority_boost = nimcp_clampf(
        drives->safety_activation * 0.5f,
        0.0f, 1.0f
    );

    /* Consolidation recommendation */
    if (drives->fatigue_level >= cfg->fatigue_consolidation_threshold) {
        mod->recommended_consolidation = HYPO_CONSOL_FULL_REST;
    } else if (drives->fatigue_level >= cfg->fatigue_consolidation_threshold * 0.75f) {
        mod->recommended_consolidation = HYPO_CONSOL_REPLAY;
    } else if (drives->fatigue_level >= cfg->fatigue_consolidation_threshold * 0.5f) {
        mod->recommended_consolidation = HYPO_CONSOL_CHECKPOINT;
    } else if (drives->fatigue_level >= cfg->fatigue_consolidation_threshold * 0.25f) {
        mod->recommended_consolidation = HYPO_CONSOL_MINI_REST;
    } else {
        mod->recommended_consolidation = HYPO_CONSOL_NONE;
    }

    /* Early stopping recommendation */
    mod->recommend_early_stopping =
        (bridge->homeostatic.state == HYPO_TRAIN_STATE_CRITICAL ||
         (bridge->homeostatic.state == HYPO_TRAIN_STATE_DIVERGING &&
          drives->safety_activation > 0.8f)) &&
        cfg->safety_enables_early_stopping;

    /* LR reduction recommendation */
    mod->recommend_lr_reduction =
        drives->safety_activation > 0.6f ||
        bridge->homeostatic.state == HYPO_TRAIN_STATE_UNSTABLE;

    /* Checkpoint recommendation */
    mod->recommend_checkpoint =
        mod->checkpoint_urgency > 0.5f ||
        bridge->homeostatic.state == HYPO_TRAIN_STATE_IMPROVING;
}

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

int hypo_training_bridge_default_config(hypo_training_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Connection configuration */
    config->auto_connect_orchestrator = true;
    config->auto_connect_training_hub = true;
    config->enable_bio_async = false;

    /* Drive configuration */
    config->drive_config.curiosity_lr_multiplier = HYPO_TRAINING_DEFAULT_CURIOSITY_LR_MULT;
    config->drive_config.curiosity_exploration_weight = 0.3f;
    config->drive_config.curiosity_enables_random_search = true;

    config->drive_config.safety_lr_reduction = HYPO_TRAINING_DEFAULT_SAFETY_LR_MULT;
    config->drive_config.safety_gradient_clip_mult = 0.5f;
    config->drive_config.safety_divergence_threshold = 0.2f;
    config->drive_config.safety_enables_early_stopping = true;

    config->drive_config.competence_difficulty_weight = 0.5f;
    config->drive_config.competence_mastery_threshold = 0.9f;
    config->drive_config.competence_auto_curriculum = true;

    config->drive_config.fatigue_lr_decay = 0.2f;
    config->drive_config.fatigue_consolidation_threshold = HYPO_TRAINING_DEFAULT_CONSOLIDATION_THRESHOLD;
    config->drive_config.fatigue_max_epochs_before_rest = 50;

    config->drive_config.autonomy_self_pacing_weight = 0.5f;
    config->drive_config.autonomy_override_teacher = false;

    /* Homeostatic configuration */
    config->homeostatic_config.loss_setpoint = HYPO_TRAINING_DEFAULT_LOSS_SETPOINT;
    config->homeostatic_config.loss_tolerance = HYPO_TRAINING_DEFAULT_LOSS_TOLERANCE;
    config->homeostatic_config.deviation_response_gain = 1.0f;
    config->homeostatic_config.adaptive_setpoint = true;
    config->homeostatic_config.setpoint_decay_rate = 0.01f;
    config->homeostatic_config.min_setpoint = 0.01f;

    /* Operational configuration */
    config->enable_consolidation = true;
    config->enable_stress_response = true;
    config->enable_reward_signals = true;
    config->update_interval_ms = 0;

    /* Logging */
    config->enable_logging = false;
    config->enable_metrics = true;

    return 0;
}

/* ============================================================================
 * LIFECYCLE MANAGEMENT
 * ============================================================================ */

hypo_training_bridge_t* hypo_training_bridge_create(
    const hypo_training_bridge_config_t* config,
    hypo_orchestrator_t orchestrator,
    training_integration_hub_t training_hub
) {
    hypo_training_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_training_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "hypothalamus_training") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_training_bridge_create: bridge_base_init failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_training_bridge_default_config(&bridge->config);
    }

    /* Initialize homeostatic state */
    bridge->homeostatic.loss_setpoint = bridge->config.homeostatic_config.loss_setpoint;
    bridge->homeostatic.current_loss = bridge->homeostatic.loss_setpoint;
    bridge->homeostatic.best_loss_seen = 1e9f;
    bridge->homeostatic.state = HYPO_TRAIN_STATE_HEALTHY;

    /* Initialize drive state */
    bridge->drives.curiosity_activation = 0.5f;
    bridge->drives.safety_activation = 0.2f;
    bridge->drives.competence_activation = 0.3f;
    bridge->drives.fatigue_level = 0.0f;
    bridge->drives.autonomy_activation = 0.5f;
    bridge->drives.exploration_tendency = 0.3f;
    bridge->drives.learning_readiness = 1.0f;
    bridge->drives.difficulty_readiness = 0.3f;

    /* Initialize modulation */
    bridge->modulation.lr_multiplier = 1.0f;
    bridge->modulation.batch_size_multiplier = 1.0f;
    bridge->modulation.gradient_clip_multiplier = 1.0f;

    /* Initialize timing */
    bridge->creation_time = get_time_us();
    bridge->last_update_time = bridge->creation_time;

    bridge->initialized = true;

    /* Auto-connect if configured */
    if (orchestrator && bridge->config.auto_connect_orchestrator) {
        hypo_training_bridge_connect_orchestrator(bridge, orchestrator);
    }

    if (training_hub && bridge->config.auto_connect_training_hub) {
        hypo_training_bridge_connect_training_hub(bridge, training_hub);
    }

    return bridge;
}

void hypo_training_bridge_destroy(hypo_training_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_training");

    /* Disconnect from systems */
    hypo_training_bridge_disconnect(bridge);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int hypo_training_bridge_reset(hypo_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Reset homeostatic state */
    bridge->homeostatic.current_loss = bridge->homeostatic.loss_setpoint;
    bridge->homeostatic.deviation = 0.0f;
    bridge->homeostatic.deviation_rate = 0.0f;
    bridge->homeostatic.loss_trend = 0.0f;
    bridge->homeostatic.loss_variance = 0.0f;
    bridge->homeostatic.state = HYPO_TRAIN_STATE_HEALTHY;
    bridge->homeostatic.epochs_since_improvement = 0;
    bridge->homeostatic.best_loss_seen = 1e9f;

    /* Reset drive state */
    bridge->drives.curiosity_activation = 0.5f;
    bridge->drives.safety_activation = 0.2f;
    bridge->drives.competence_activation = 0.3f;
    bridge->drives.fatigue_level = 0.0f;
    bridge->drives.autonomy_activation = 0.5f;
    bridge->drives.exploration_tendency = 0.3f;
    bridge->drives.learning_readiness = 1.0f;
    bridge->drives.difficulty_readiness = 0.3f;

    /* Reset modulation */
    bridge->modulation.lr_multiplier = 1.0f;
    bridge->modulation.batch_size_multiplier = 1.0f;
    bridge->modulation.gradient_clip_multiplier = 1.0f;
    bridge->modulation.difficulty_adjustment = 0.0f;
    bridge->modulation.recommended_consolidation = HYPO_CONSOL_NONE;
    bridge->modulation.recommend_early_stopping = false;

    /* Reset history */
    bridge->loss_history_count = 0;
    bridge->loss_history_idx = 0;
    bridge->gradient_history_count = 0;
    bridge->gradient_history_idx = 0;

    /* Reset training progress */
    bridge->current_epoch = 0;
    bridge->total_batches = 0;
    bridge->cumulative_loss = 0.0f;
    bridge->in_consolidation = false;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

/* ============================================================================
 * CONNECTION MANAGEMENT
 * ============================================================================ */

int hypo_training_bridge_connect_orchestrator(
    hypo_training_bridge_t* bridge,
    hypo_orchestrator_t orchestrator
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_connect_orchestrator: bridge is NULL");
        return -1;
    }

    bridge->orchestrator = orchestrator;
    bridge->orchestrator_connected = (orchestrator != NULL);

    /* Register with orchestrator if available */
    if (orchestrator) {
        uint32_t bridge_id = 0;
        int ret = hypo_orch_register_bridge(
            orchestrator,
            HYPO_BRIDGE_PREDICTIVE,  /* Use predictive bridge type for training */
            "HypothalamusTrainingBridge",
            bridge,
            NULL,
            &bridge_id
        );
        if (ret == 0) {
            bridge->bridge_id = bridge_id;
        }
    }

    return 0;
}

int hypo_training_bridge_connect_training_hub(
    hypo_training_bridge_t* bridge,
    training_integration_hub_t training_hub
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_connect_training_hub: bridge is NULL");
        return -1;
    }

    bridge->training_hub = training_hub;
    bridge->hub_connected = (training_hub != NULL);

    /* Register with training hub if available */
    if (training_hub) {
        training_hub_register_module(
            training_hub,
            HYPO_TRAINING_BRIDGE_MODULE_ID,
            TRAINING_CATEGORY_OPTIMIZATION,
            "HypothalamusTrainingBridge",
            bridge
        );
    }

    return 0;
}

int hypo_training_bridge_disconnect(hypo_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_disconnect: bridge is NULL");
        return -1;
    }

    /* Unregister from training hub */
    if (bridge->hub_connected && bridge->training_hub) {
        training_hub_unregister_module(bridge->training_hub, HYPO_TRAINING_BRIDGE_MODULE_ID);
    }

    /* Unregister from orchestrator */
    if (bridge->orchestrator_connected && bridge->orchestrator) {
        hypo_orch_unregister_bridge(bridge->orchestrator, bridge->bridge_id);
    }

    bridge->orchestrator = NULL;
    bridge->training_hub = NULL;
    bridge->orchestrator_connected = false;
    bridge->hub_connected = false;

    return 0;
}

int hypo_training_bridge_is_connected(
    const hypo_training_bridge_t* bridge,
    bool* orchestrator_connected,
    bool* hub_connected
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_is_connected: bridge is NULL");
        return -1;
    }

    if (orchestrator_connected) {
        *orchestrator_connected = bridge->orchestrator_connected;
    }
    if (hub_connected) {
        *hub_connected = bridge->hub_connected;
    }

    return 0;
}

/* ============================================================================
 * TRAINING EVENT PROCESSING
 * ============================================================================ */

int hypo_training_bridge_process_loss(
    hypo_training_bridge_t* bridge,
    uint32_t epoch,
    uint32_t batch,
    float loss
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_process_loss: bridge is NULL");
        return -1;
    }

    /* Update statistics */
    bridge->stats.training_events_received++;

    /* Check for invalid loss */
    if (!isfinite(loss)) {
        bridge->homeostatic.state = HYPO_TRAIN_STATE_CRITICAL;
        bridge->drives.safety_activation = 1.0f;
        update_drives_from_training(bridge);
        compute_modulations(bridge);
        return 0;
    }

    /* Add to loss history */
    bridge->loss_history[bridge->loss_history_idx] = loss;
    bridge->loss_history_idx = (bridge->loss_history_idx + 1) % HYPO_TRAINING_MAX_LOSS_HISTORY;
    if (bridge->loss_history_count < HYPO_TRAINING_MAX_LOSS_HISTORY) {
        bridge->loss_history_count++;
    }

    /* Update homeostatic state */
    float prev_loss = bridge->homeostatic.current_loss;
    bridge->homeostatic.current_loss = loss;
    bridge->homeostatic.deviation = loss - bridge->homeostatic.loss_setpoint;
    bridge->homeostatic.deviation_rate = loss - prev_loss;
    bridge->homeostatic.loss_trend = compute_loss_trend(bridge);
    bridge->homeostatic.loss_variance = compute_loss_variance(bridge);

    /* Track best loss */
    if (loss < bridge->homeostatic.best_loss_seen) {
        bridge->homeostatic.best_loss_seen = loss;
        bridge->homeostatic.epochs_since_improvement = 0;
    }

    /* Assess training state */
    bridge->homeostatic.state = assess_training_state(bridge);

    /* Update drives based on training state */
    update_drives_from_training(bridge);

    /* Compute modulations */
    compute_modulations(bridge);

    /* Update adaptive setpoint if enabled */
    if (bridge->config.homeostatic_config.adaptive_setpoint) {
        if (bridge->homeostatic.state == HYPO_TRAIN_STATE_IMPROVING) {
            float new_setpoint = bridge->homeostatic.loss_setpoint -
                                 bridge->config.homeostatic_config.setpoint_decay_rate;
            if (new_setpoint >= bridge->config.homeostatic_config.min_setpoint) {
                bridge->homeostatic.loss_setpoint = new_setpoint;
            }
        }
    }

    /* Update tracking */
    bridge->current_epoch = epoch;
    bridge->total_batches++;
    bridge->cumulative_loss += loss;
    bridge->last_update_time = get_time_us();

    return 0;
}

int hypo_training_bridge_process_gradient(
    hypo_training_bridge_t* bridge,
    float gradient_norm,
    bool was_clipped
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_process_gradient: bridge is NULL");
        return -1;
    }

    bridge->stats.training_events_received++;

    /* Add to gradient history */
    bridge->gradient_history[bridge->gradient_history_idx] = gradient_norm;
    bridge->gradient_history_idx = (bridge->gradient_history_idx + 1) % HYPO_TRAINING_MAX_GRADIENT_HISTORY;
    if (bridge->gradient_history_count < HYPO_TRAINING_MAX_GRADIENT_HISTORY) {
        bridge->gradient_history_count++;
    }

    /* Detect gradient instability */
    if (!isfinite(gradient_norm) || gradient_norm > 100.0f) {
        bridge->drives.safety_activation = nimcp_clampf(
            bridge->drives.safety_activation + 0.2f, 0.0f, 1.0f);
        bridge->stats.safety_interventions++;
    } else if (was_clipped) {
        bridge->drives.safety_activation = nimcp_clampf(
            bridge->drives.safety_activation + 0.05f, 0.0f, 1.0f);
    }

    /* Recompute modulations */
    compute_modulations(bridge);

    return 0;
}

int hypo_training_bridge_process_epoch(
    hypo_training_bridge_t* bridge,
    uint32_t epoch,
    float avg_loss
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_process_epoch: bridge is NULL");
        return -1;
    }

    bridge->stats.training_events_received++;

    /* Update fatigue */
    bridge->drives.fatigue_level = nimcp_clampf(
        bridge->drives.fatigue_level + HYPO_TRAINING_DEFAULT_FATIGUE_RATE,
        0.0f, 1.0f
    );

    /* Track improvement */
    if (avg_loss >= bridge->homeostatic.best_loss_seen) {
        bridge->homeostatic.epochs_since_improvement++;
    }

    /* Update epoch tracking */
    bridge->current_epoch = epoch;

    /* Check for consolidation need */
    if (bridge->drives.fatigue_level >= bridge->config.drive_config.fatigue_consolidation_threshold) {
        bridge->stats.consolidation_phases++;
    }

    /* Recompute modulations */
    update_drives_from_training(bridge);
    compute_modulations(bridge);

    return 0;
}

int hypo_training_bridge_process_lr_change(
    hypo_training_bridge_t* bridge,
    float old_lr,
    float new_lr
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_process_lr_change: bridge is NULL");
        return -1;
    }

    bridge->stats.training_events_received++;
    bridge->stats.lr_modulations++;

    /* LR increase suggests exploration phase */
    if (new_lr > old_lr) {
        bridge->drives.curiosity_activation = nimcp_clampf(
            bridge->drives.curiosity_activation + 0.1f, 0.0f, 1.0f);
    } else {
        /* LR decrease suggests convergence/exploitation */
        bridge->drives.curiosity_activation = nimcp_clampf(
            bridge->drives.curiosity_activation - 0.05f, 0.0f, 1.0f);
    }

    /* Recompute modulations */
    compute_modulations(bridge);

    return 0;
}

/* ============================================================================
 * MODULATION OUTPUT
 * ============================================================================ */

int hypo_training_bridge_compute_modulation(
    hypo_training_bridge_t* bridge,
    hypo_training_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_compute_modulation: bridge or modulation is NULL");
        return -1;
    }

    /* Ensure modulations are current */
    compute_modulations(bridge);

    *modulation = bridge->modulation;
    bridge->stats.modulations_published++;

    return 0;
}

int hypo_training_bridge_get_lr_multiplier(
    const hypo_training_bridge_t* bridge,
    float* lr_multiplier
) {
    if (!bridge || !lr_multiplier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_get_lr_multiplier: bridge or lr_multiplier is NULL");
        return -1;
    }

    *lr_multiplier = bridge->modulation.lr_multiplier;
    return 0;
}

int hypo_training_bridge_get_difficulty_adjustment(
    const hypo_training_bridge_t* bridge,
    float* difficulty_adj
) {
    if (!bridge || !difficulty_adj) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_get_difficulty_adjustment: bridge or difficulty_adj is NULL");
        return -1;
    }

    *difficulty_adj = bridge->modulation.difficulty_adjustment;
    return 0;
}

int hypo_training_bridge_check_consolidation(
    const hypo_training_bridge_t* bridge,
    hypo_consolidation_type_t* consolidation_type
) {
    if (!bridge || !consolidation_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_check_consolidation: bridge or consolidation_type is NULL");
        return -1;
    }

    *consolidation_type = bridge->modulation.recommended_consolidation;
    return 0;
}

/* ============================================================================
 * HOMEOSTATIC STATE
 * ============================================================================ */

int hypo_training_bridge_get_homeostatic_state(
    const hypo_training_bridge_t* bridge,
    hypo_training_homeostatic_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_get_homeostatic_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->homeostatic;
    return 0;
}

int hypo_training_bridge_get_training_state(
    const hypo_training_bridge_t* bridge,
    hypo_training_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_get_training_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->homeostatic.state;
    return 0;
}

int hypo_training_bridge_set_loss_setpoint(
    hypo_training_bridge_t* bridge,
    float new_setpoint
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_set_loss_setpoint: bridge is NULL");
        return -1;
    }

    if (new_setpoint < bridge->config.homeostatic_config.min_setpoint) {
        new_setpoint = bridge->config.homeostatic_config.min_setpoint;
    }

    bridge->homeostatic.loss_setpoint = new_setpoint;

    /* Recompute deviation */
    bridge->homeostatic.deviation =
        bridge->homeostatic.current_loss - bridge->homeostatic.loss_setpoint;

    return 0;
}

/* ============================================================================
 * DRIVE STATE
 * ============================================================================ */

int hypo_training_bridge_get_drive_state(
    const hypo_training_bridge_t* bridge,
    hypo_training_drive_state_t* drive_state
) {
    if (!bridge || !drive_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_get_drive_state: bridge or drive_state is NULL");
        return -1;
    }

    *drive_state = bridge->drives;
    return 0;
}

int hypo_training_bridge_set_drive(
    hypo_training_bridge_t* bridge,
    uint32_t drive_type,
    float activation
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_set_drive: bridge is NULL");
        return -1;
    }

    activation = nimcp_clampf(activation, 0.0f, 1.0f);

    switch (drive_type) {
        case 0:
            bridge->drives.curiosity_activation = activation;
            break;
        case 1:
            bridge->drives.safety_activation = activation;
            break;
        case 2:
            bridge->drives.competence_activation = activation;
            break;
        case 3:
            bridge->drives.fatigue_level = activation;
            break;
        case 4:
            bridge->drives.autonomy_activation = activation;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_training_bridge_set_drive: operation failed");
            return -1;
    }

    /* Recompute derived states and modulations */
    bridge->drives.exploration_tendency =
        bridge->drives.curiosity_activation - bridge->drives.safety_activation;
    bridge->drives.learning_readiness =
        1.0f - bridge->drives.fatigue_level;
    bridge->drives.difficulty_readiness =
        bridge->drives.competence_activation * bridge->drives.learning_readiness;

    compute_modulations(bridge);

    return 0;
}

int hypo_training_bridge_reset_fatigue(hypo_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_reset_fatigue: bridge is NULL");
        return -1;
    }

    bridge->drives.fatigue_level = 0.0f;
    bridge->drives.learning_readiness = 1.0f;
    bridge->drives.difficulty_readiness =
        bridge->drives.competence_activation * bridge->drives.learning_readiness;
    bridge->in_consolidation = false;

    compute_modulations(bridge);

    return 0;
}

/* ============================================================================
 * STATISTICS AND MONITORING
 * ============================================================================ */

int hypo_training_bridge_get_stats(
    const hypo_training_bridge_t* bridge,
    hypo_training_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    stats->uptime_us = get_time_us() - bridge->creation_time;

    return 0;
}

int hypo_training_bridge_reset_stats(hypo_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_training_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

const char* hypo_training_state_name(hypo_training_state_t state) {
    switch (state) {
        case HYPO_TRAIN_STATE_HEALTHY:    return "HEALTHY";
        case HYPO_TRAIN_STATE_IMPROVING:  return "IMPROVING";
        case HYPO_TRAIN_STATE_PLATEAU:    return "PLATEAU";
        case HYPO_TRAIN_STATE_DIVERGING:  return "DIVERGING";
        case HYPO_TRAIN_STATE_UNSTABLE:   return "UNSTABLE";
        case HYPO_TRAIN_STATE_CRITICAL:   return "CRITICAL";
        default:                          return "UNKNOWN";
    }
}

const char* hypo_consolidation_type_name(hypo_consolidation_type_t type) {
    switch (type) {
        case HYPO_CONSOL_NONE:       return "NONE";
        case HYPO_CONSOL_MINI_REST:  return "MINI_REST";
        case HYPO_CONSOL_CHECKPOINT: return "CHECKPOINT";
        case HYPO_CONSOL_REPLAY:     return "REPLAY";
        case HYPO_CONSOL_FULL_REST:  return "FULL_REST";
        default:                     return "UNKNOWN";
    }
}

const char* hypo_training_modulation_name(hypo_training_modulation_type_t type) {
    switch (type) {
        case HYPO_TRAIN_MOD_LEARNING_RATE:   return "LEARNING_RATE";
        case HYPO_TRAIN_MOD_BATCH_SIZE:      return "BATCH_SIZE";
        case HYPO_TRAIN_MOD_GRADIENT_CLIP:   return "GRADIENT_CLIP";
        case HYPO_TRAIN_MOD_CURRICULUM_DIFF: return "CURRICULUM_DIFFICULTY";
        case HYPO_TRAIN_MOD_SAMPLE_PRIORITY: return "SAMPLE_PRIORITY";
        case HYPO_TRAIN_MOD_CHECKPOINT_FREQ: return "CHECKPOINT_FREQUENCY";
        case HYPO_TRAIN_MOD_MULTI_TASK_WEIGHT: return "MULTI_TASK_WEIGHT";
        case HYPO_TRAIN_MOD_REPLAY_PRIORITY: return "REPLAY_PRIORITY";
        default:                             return "UNKNOWN";
    }
}

void hypo_training_bridge_print_summary(const hypo_training_bridge_t* bridge) {
    if (!bridge) {
        printf("HypothalamusTrainingBridge: NULL\n");
        return;
    }

    printf("=== Hypothalamus-Training Bridge Summary ===\n");
    printf("Connected: Orchestrator=%s, TrainingHub=%s\n",
           bridge->orchestrator_connected ? "Yes" : "No",
           bridge->hub_connected ? "Yes" : "No");
    printf("\n");

    printf("Homeostatic State:\n");
    printf("  Current Loss: %.4f (Setpoint: %.4f)\n",
           bridge->homeostatic.current_loss,
           bridge->homeostatic.loss_setpoint);
    printf("  Deviation: %.4f (Rate: %.4f)\n",
           bridge->homeostatic.deviation,
           bridge->homeostatic.deviation_rate);
    printf("  Trend: %.4f, Variance: %.6f\n",
           bridge->homeostatic.loss_trend,
           bridge->homeostatic.loss_variance);
    printf("  State: %s\n", hypo_training_state_name(bridge->homeostatic.state));
    printf("  Best Loss: %.4f, Epochs Since Improvement: %u\n",
           bridge->homeostatic.best_loss_seen,
           bridge->homeostatic.epochs_since_improvement);
    printf("\n");

    printf("Drive State:\n");
    printf("  Curiosity: %.2f, Safety: %.2f\n",
           bridge->drives.curiosity_activation,
           bridge->drives.safety_activation);
    printf("  Competence: %.2f, Fatigue: %.2f\n",
           bridge->drives.competence_activation,
           bridge->drives.fatigue_level);
    printf("  Exploration Tendency: %.2f\n", bridge->drives.exploration_tendency);
    printf("  Learning Readiness: %.2f\n", bridge->drives.learning_readiness);
    printf("\n");

    printf("Current Modulations:\n");
    printf("  LR Multiplier: %.2f\n", bridge->modulation.lr_multiplier);
    printf("  Batch Size Multiplier: %.2f\n", bridge->modulation.batch_size_multiplier);
    printf("  Gradient Clip Multiplier: %.2f\n", bridge->modulation.gradient_clip_multiplier);
    printf("  Difficulty Adjustment: %.2f\n", bridge->modulation.difficulty_adjustment);
    printf("  Consolidation: %s\n",
           hypo_consolidation_type_name(bridge->modulation.recommended_consolidation));
    printf("  Recommend Early Stop: %s, LR Reduction: %s\n",
           bridge->modulation.recommend_early_stopping ? "Yes" : "No",
           bridge->modulation.recommend_lr_reduction ? "Yes" : "No");
    printf("\n");

    printf("Training Progress:\n");
    printf("  Current Epoch: %u, Total Batches: %u\n",
           bridge->current_epoch, bridge->total_batches);
}

void hypo_training_bridge_print_stats(const hypo_training_bridge_stats_t* stats) {
    if (!stats) {
        printf("HypothalamusTrainingBridge Stats: NULL\n");
        return;
    }

    printf("=== Hypothalamus-Training Bridge Statistics ===\n");
    printf("Events:\n");
    printf("  Training Events Received: %lu\n", (unsigned long)stats->training_events_received);
    printf("  Modulations Published: %lu\n", (unsigned long)stats->modulations_published);
    printf("  Drive Updates: %lu\n", (unsigned long)stats->drive_updates);
    printf("\n");

    printf("Modulations:\n");
    printf("  LR Modulations: %lu\n", (unsigned long)stats->lr_modulations);
    printf("  Safety Interventions: %lu\n", (unsigned long)stats->safety_interventions);
    printf("  Consolidation Phases: %lu\n", (unsigned long)stats->consolidation_phases);
    printf("  Early Stop Recommendations: %lu\n", (unsigned long)stats->early_stopping_recommendations);
    printf("\n");

    printf("Homeostatic:\n");
    printf("  Avg Loss Deviation: %.4f\n", stats->avg_loss_deviation);
    printf("  Max Loss Deviation: %.4f\n", stats->max_loss_deviation);
    printf("  Divergence Detections: %u\n", stats->divergence_detections);
    printf("  Plateau Detections: %u\n", stats->plateau_detections);
    printf("\n");

    printf("Performance:\n");
    printf("  Avg Processing Time: %lu us\n", (unsigned long)stats->avg_processing_time_us);
    printf("  Uptime: %lu us\n", (unsigned long)stats->uptime_us);
}
