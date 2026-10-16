//=============================================================================
// nimcp_brain_init_dragonfly.c - Dragonfly System Initialization & Integration
//=============================================================================
/**
 * @file nimcp_brain_init_dragonfly.c
 * @brief Dragonfly target tracking subsystem initialization with brain integration
 *
 * BIOLOGICAL BASIS:
 * Dragonflies achieve 95% hunting success through:
 * - TSDN neurons encoding target direction as population vector
 * - CSTMD1 winner-take-all attention for single target lock
 * - Internal models predicting prey trajectory (Mischiati et al. 2015)
 * - Predictive gain modulation along expected target path
 *
 * INTEGRATION POINTS:
 * - Visual Cortex: Target detection from visual processing
 * - Audio Cortex: Directional cueing from sound localization
 * - Parietal Lobe: Spatial reasoning for interception planning
 * - Cognitive Systems: Attention allocation and salience detection
 * - Thalamic Layer: Signal routing and gating
 * - Substrate Layer: Metabolic costs and fatigue modeling
 * - FEP Orchestrator: Free energy minimization for prediction
 * - Bio-Async: Asynchronous neural processing
 * - Global Workspace: Conscious target awareness
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-29
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_lnn.h"
#include "dragonfly/nimcp_dragonfly_medulla_bridge.h"
#include "core/medulla/nimcp_medulla.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_dragonfly, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize dragonfly subsystem for brain with full integration
 *
 * Creates and configures the dragonfly target tracking system based on
 * brain configuration, then connects it to available brain subsystems.
 *
 * @param brain Brain instance to initialize dragonfly for
 * @return true on success, false on failure
 *
 * PROCESS:
 * 1. Check if dragonfly is enabled in brain config
 * 2. Create dragonfly configuration from brain config
 * 3. Create dragonfly system
 * 4. Store in brain->dragonfly
 * 5. Connect to available subsystems (visual, parietal, etc.)
 */
bool nimcp_brain_factory_init_dragonfly_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_dragonfly_subsystem: brain is NULL");

            return false;
    }

    /* Initialize dragonfly fields to defaults */
    brain->dragonfly = NULL;
    brain->dragonfly_enabled = false;
    brain->last_dragonfly_update_us = 0;
    brain->dragonfly_medulla_bridge = NULL;
    brain->dragonfly_lnn = NULL;
    brain->dragonfly_lnn_enabled = false;

    /* Check if dragonfly is disabled in config */
    if (!brain->config.enable_dragonfly) {
        return true;  /* Success - dragonfly is disabled by config */
    }

    fprintf(stderr, "[DRAGONFLY] Initializing dragonfly target tracking subsystem...\n");

    /* Create dragonfly configuration from brain config */
    dragonfly_config_t config = dragonfly_default_config();

    /* TSDN configuration */
    if (brain->config.dragonfly_tsdn_tuning_width > 0.0f) {
        config.tsdn_config.tuning_width = brain->config.dragonfly_tsdn_tuning_width;
    }

    /* Tracking configuration */
    if (brain->config.dragonfly_attention_threshold > 0.0f) {
        config.tracker_config.lock_threshold = brain->config.dragonfly_attention_threshold;
    }
    if (brain->config.dragonfly_size_selectivity_min > 0.0f) {
        config.tracker_config.min_target_size = brain->config.dragonfly_size_selectivity_min;
    }
    if (brain->config.dragonfly_size_selectivity_max > 0.0f) {
        config.tracker_config.max_target_size = brain->config.dragonfly_size_selectivity_max;
    }

    /* Prediction configuration */
    config.prediction_config.enable_imm = brain->config.dragonfly_enable_imm;
    if (brain->config.dragonfly_prediction_horizon_ms > 0.0f) {
        config.prediction_config.max_prediction_ms = brain->config.dragonfly_prediction_horizon_ms;
    }

    /* Interception configuration */
    if (brain->config.dragonfly_nav_gain > 0.0f) {
        config.intercept_config.pn_gain = brain->config.dragonfly_nav_gain;
    }

    /* Create dragonfly system */
    dragonfly_system_t* dragonfly = dragonfly_system_create(&config);
    if (!dragonfly) {
        fprintf(stderr, "[DRAGONFLY] ERROR: Failed to create dragonfly system\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_dragonfly_subsystem: dragonfly is NULL");
        return false;
    }

    /* Store in brain */
    brain->dragonfly = dragonfly;
    brain->dragonfly_enabled = true;
    brain->last_dragonfly_update_us = 0;

    fprintf(stderr, "[DRAGONFLY] Dragonfly system created successfully\n");
    fprintf(stderr, "[DRAGONFLY]   Prediction horizon: %.1f ms\n", config.prediction_config.max_prediction_ms);
    fprintf(stderr, "[DRAGONFLY]   Navigation gain: %.1f\n", config.intercept_config.pn_gain);

    /* Phase 4i: sidecar LNN reservoir for continuous-time temporal smoothing
     * of dragonfly state. Non-fatal on failure — dragonfly runs without it. */
    brain->dragonfly_lnn = (void*)dragonfly_lnn_create(dragonfly);
    if (brain->dragonfly_lnn) {
        brain->dragonfly_lnn_enabled = true;
        fprintf(stderr, "[DRAGONFLY]   LNN reservoir: live (16-dim NCP)\n");
    } else {
        fprintf(stderr, "[DRAGONFLY]   LNN reservoir: unavailable (continuing without)\n");
    }

    /* =========================================================================
     * DRAGONFLY-MEDULLA INTEGRATION BRIDGE
     * =========================================================================
     * Create and connect the bridge to integrate dragonfly with medulla states.
     * The bridge modulates hunting behavior based on:
     * - Arousal level: Alert dragonflies hunt better than drowsy ones
     * - Protection level: Can block or abort hunting (prioritize survival)
     * - Circadian phase: Diurnal hunters are inactive at night
     */
    if (brain->medulla) {
        fprintf(stderr, "[DRAGONFLY] Creating dragonfly-medulla integration bridge...\n");

        /* Create bridge with default configuration */
        dragonfly_medulla_config_t bridge_config = dragonfly_medulla_default_config();
        bridge_config.enable_logging = false;  /* Reduce verbosity */

        brain->dragonfly_medulla_bridge = dragonfly_medulla_bridge_create(&bridge_config);
        if (brain->dragonfly_medulla_bridge) {
            /* Connect bridge to both systems */
            int result = dragonfly_medulla_bridge_connect(
                brain->dragonfly_medulla_bridge,
                brain->dragonfly,
                brain->medulla
            );
            if (result == 0) {
                fprintf(stderr, "[DRAGONFLY] Dragonfly-medulla bridge connected\n");
                fprintf(stderr, "[DRAGONFLY]   Arousal modulation: enabled\n");
                fprintf(stderr, "[DRAGONFLY]   Protection modulation: enabled\n");
                fprintf(stderr, "[DRAGONFLY]   Circadian modulation: enabled\n");
            } else {
                fprintf(stderr, "[DRAGONFLY] WARNING: Failed to connect dragonfly-medulla bridge\n");
                dragonfly_medulla_bridge_destroy(brain->dragonfly_medulla_bridge);
                brain->dragonfly_medulla_bridge = NULL;
            }
        } else {
            fprintf(stderr, "[DRAGONFLY] WARNING: Failed to create dragonfly-medulla bridge\n");
        }
    } else {
        fprintf(stderr, "[DRAGONFLY] Note: Medulla not available, skipping dragonfly-medulla integration\n");
    }

    return true;
}

//=============================================================================
// Accessor Function
//=============================================================================

/**
 * @brief Get dragonfly system from brain
 *
 * @param brain Brain instance
 * @return Dragonfly system handle or NULL if not enabled
 */
dragonfly_system_t* brain_get_dragonfly(brain_t brain) {
    if (!brain || !brain->dragonfly_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_get_dragonfly: invalid parameters");

            return NULL;
    }
    return brain->dragonfly;
}

//=============================================================================
// Runtime Integration Functions
//=============================================================================

/**
 * @brief Update dragonfly with visual detection
 *
 * @param brain Brain instance
 * @param position Target position [x, y, z]
 * @param size Target angular size in radians
 * @param contrast Target contrast [0-1]
 * @return 0 on success, -1 on error
 */
int brain_dragonfly_detect(brain_t brain, const float position[3],
                           float size, float contrast) {
    if (!brain || !brain->dragonfly_enabled || !brain->dragonfly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_dragonfly_detect: invalid parameters");

            return -1;
    }

    /* Create detection structure */
    dragonfly_detection_t detection = {
        .position = {position[0], position[1], position[2]},
        .size = size,
        .contrast = contrast,
        .motion_direction_rad = 0.0f,  /* Will be computed from tracking */
        .motion_speed = 0.0f,
        .timestamp_us = brain->current_time_us,
        .id = 0  /* Auto-assigned */
    };

    return dragonfly_process_detection(brain->dragonfly, &detection);
}

/**
 * @brief Get current dragonfly motor command
 *
 * @param brain Brain instance
 * @param heading_rad Output: desired heading angle
 * @param pitch_rad Output: desired pitch angle
 * @param velocity Output: desired velocity [3]
 * @param urgency Output: command urgency [0-1]
 * @return 0 on success, -1 on error
 */
int brain_dragonfly_get_command(brain_t brain, float* heading_rad,
                                float* pitch_rad, float velocity[3],
                                float* urgency) {
    if (!brain || !brain->dragonfly_enabled || !brain->dragonfly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_dragonfly_get_command: invalid parameters");

            return -1;
    }

    dragonfly_motor_cmd_t cmd;
    int result = dragonfly_get_motor_command(brain->dragonfly, &cmd);
    if (result != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_dragonfly_get_command: motor command failed");
        return -1;
    }

    if (heading_rad) *heading_rad = cmd.heading_rad;
    if (pitch_rad) *pitch_rad = cmd.pitch_rad;
    if (velocity) {
        velocity[0] = cmd.velocity[0];
        velocity[1] = cmd.velocity[1];
        velocity[2] = cmd.velocity[2];
    }
    if (urgency) *urgency = cmd.urgency;

    return 0;
}

/**
 * @brief Step dragonfly system forward in time
 *
 * Also updates the dragonfly-medulla bridge if connected.
 *
 * @param brain Brain instance
 * @param delta_t Time step in microseconds
 * @return 0 on success, -1 on error
 */
int brain_step_dragonfly(brain_t brain, uint64_t delta_t) {
    if (!brain || !brain->dragonfly_enabled || !brain->dragonfly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_step_dragonfly: invalid parameters");

            return -1;
    }

    float dt_seconds = (float)delta_t / 1000000.0f;

    /* Update dragonfly-medulla bridge first to apply modulations */
    if (brain->dragonfly_medulla_bridge) {
        dragonfly_medulla_bridge_update(brain->dragonfly_medulla_bridge, dt_seconds);

        /* Check if hunting is blocked by protection level */
        if (!dragonfly_medulla_bridge_hunting_allowed(brain->dragonfly_medulla_bridge)) {
            /* Don't process dragonfly updates when hunting is blocked */
            return 0;
        }
    }

    int result = dragonfly_update(brain->dragonfly, dt_seconds);
    if (result == 0) {
        brain->last_dragonfly_update_us = brain->current_time_us;
        /* Phase 4i: drive the sidecar LNN forward with the same dt so its
         * continuous-time state stays aligned with the dragonfly tick. */
        if (brain->dragonfly_lnn && brain->dragonfly_lnn_enabled) {
            (void)dragonfly_lnn_step(
                (dragonfly_lnn_t*)brain->dragonfly_lnn,
                dt_seconds * 1000.0f);
        }
    }
    return result;
}

/**
 * @brief Get dragonfly operating mode
 *
 * @param brain Brain instance
 * @return Current mode or -1 on error
 */
int brain_dragonfly_get_mode(brain_t brain) {
    if (!brain || !brain->dragonfly_enabled || !brain->dragonfly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_dragonfly_get_mode: invalid parameters");

            return -1;
    }

    return (int)dragonfly_get_mode(brain->dragonfly);
}

/**
 * @brief Abort current dragonfly hunt
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_dragonfly_abort(brain_t brain) {
    if (!brain || !brain->dragonfly_enabled || !brain->dragonfly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_dragonfly_abort: invalid parameters");

            return -1;
    }

    return dragonfly_abort_pursuit(brain->dragonfly);
}

//=============================================================================
// Dragonfly-Medulla Bridge Functions
//=============================================================================

/**
 * @brief Get dragonfly-medulla bridge from brain
 *
 * @param brain Brain instance
 * @return Bridge handle or NULL if not connected
 */
dragonfly_medulla_bridge_t brain_get_dragonfly_medulla_bridge(brain_t brain) {
    if (!brain || !brain->dragonfly_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_get_dragonfly_medulla_bridge: invalid parameters");

            return NULL;
    }
    return brain->dragonfly_medulla_bridge;
}

/**
 * @brief Get current dragonfly modulation state from medulla
 *
 * Returns the current modulation factors applied to dragonfly
 * based on arousal, protection, and circadian states.
 *
 * @param brain Brain instance
 * @param modulation Output modulation state
 * @return 0 on success, -1 on error
 */
int brain_dragonfly_get_modulation(brain_t brain,
                                   dragonfly_medulla_modulation_t* modulation) {
    if (!brain || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_dragonfly_get_modulation: invalid parameters");

            return -1;
    }

    if (!brain->dragonfly_medulla_bridge) {
        /* No bridge - return default (unmodulated) values */
        modulation->nav_gain_scale = 1.0f;
        modulation->urgency_scale = 1.0f;
        modulation->reaction_scale = 1.0f;
        modulation->accuracy_scale = 1.0f;
        modulation->max_duration_scale = 1.0f;
        modulation->hunting_allowed = true;
        modulation->should_abort = false;
        modulation->arousal_level = 4;  /* AWAKE */
        modulation->protection_level = 0;  /* NORMAL */
        modulation->circadian_phase = 1;  /* MORNING */
        return 0;
    }

    return dragonfly_medulla_bridge_get_modulation(
        brain->dragonfly_medulla_bridge, modulation);
}

/**
 * @brief Check if dragonfly hunting is currently allowed
 *
 * @param brain Brain instance
 * @return true if hunting is allowed based on medulla state
 */
bool brain_dragonfly_hunting_allowed(brain_t brain) {
    if (!brain || !brain->dragonfly_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_dragonfly_hunting_allowed: invalid parameters");

            return false;
    }

    if (!brain->dragonfly_medulla_bridge) {
        return true;  /* No bridge - always allowed */
    }

    return dragonfly_medulla_bridge_hunting_allowed(brain->dragonfly_medulla_bridge);
}
