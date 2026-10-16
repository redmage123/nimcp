//=============================================================================
// nimcp_physics_training_bridge.h - Physics Layer to Training Layer Bridge
//=============================================================================
/**
 * @file nimcp_physics_training_bridge.h
 * @brief Bridge connecting Phase 1 Physics modules with Training Layer
 *
 * WHAT: Enables physics-aware training with biophysically-grounded learning.
 *
 * WHY:  Training should respect biophysical constraints:
 *       - Learning rate modulation by metabolic state
 *       - Plasticity windows constrained by membrane dynamics
 *       - Parameter bounds respecting biological ranges
 *
 * HOW:  - Reports physics constraints to training system
 *       - Receives learning signals that respect biophysics
 *       - Implements metabolically-gated plasticity
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PHYSICS_TRAINING_BRIDGE_H
#define NIMCP_PHYSICS_TRAINING_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/biophysics/nimcp_hodgkin_huxley.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PHYSICS_TRAIN_MODULE_NAME    "physics_training_bridge"

/** Default ATP threshold for plasticity */
#define PHYSICS_TRAIN_ATP_THRESHOLD  0.5f

/** Default metabolic cost of learning */
#define PHYSICS_TRAIN_LEARNING_COST  0.01f

/** Default plasticity window (ms) */
#define PHYSICS_TRAIN_PLASTICITY_WINDOW 20.0f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Training modulation factors from physics
 */
typedef struct {
    /** Learning rate scale (0.0-2.0) */
    float learning_rate_scale;

    /** Plasticity enabled */
    bool plasticity_enabled;

    /** ATP available for learning */
    float available_atp;

    /** Metabolic cost per update */
    float metabolic_cost;

    /** Current STDP window width (ms) */
    float stdp_window_ms;

    /** Timestamp */
    float timestamp_ms;
} physics_train_modulation_t;

/**
 * @brief Training feedback to physics
 */
typedef struct {
    /** Weight update magnitude (for metabolic cost) */
    float update_magnitude;

    /** Number of synapses updated */
    uint32_t synapses_updated;

    /** Learning signal strength */
    float learning_signal;

    /** Timestamp */
    float timestamp_ms;
} physics_train_feedback_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Enable metabolic gating of plasticity */
    bool enable_metabolic_gating;

    /** ATP threshold for plasticity */
    float atp_threshold;

    /** Metabolic cost per learning event */
    float learning_cost;

    /** Base STDP window (ms) */
    float base_stdp_window_ms;

    /** Enable temperature effects on plasticity */
    bool enable_temp_effects;
} physics_train_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /** Total modulations computed */
    uint64_t modulations_computed;

    /** Plasticity blocks due to low ATP */
    uint64_t plasticity_blocks;

    /** Total metabolic cost of learning */
    float total_learning_cost;

    /** Average learning rate scale */
    float avg_lr_scale;

    /** Last update timestamp */
    float last_update_ms;
} physics_train_stats_t;

/**
 * @brief Opaque bridge structure
 */
typedef struct physics_train_bridge_struct physics_train_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
NIMCP_EXPORT int physics_train_default_config(physics_train_config_t* config);

/**
 * @brief Create training bridge
 */
NIMCP_EXPORT physics_train_bridge_t* physics_train_bridge_create(
    const physics_train_config_t* config
);

/**
 * @brief Destroy bridge
 */
NIMCP_EXPORT void physics_train_bridge_destroy(physics_train_bridge_t* bridge);

/**
 * @brief Connect physics modules
 */
NIMCP_EXPORT int physics_train_connect_physics(
    physics_train_bridge_t* bridge,
    nimcp_thermodynamic_state_t* thermo,
    nimcp_hh_population_t* hh_pop
);

/**
 * @brief Get training modulation from physics state
 */
NIMCP_EXPORT int physics_train_get_modulation(
    physics_train_bridge_t* bridge,
    physics_train_modulation_t* modulation
);

/**
 * @brief Report training feedback
 */
NIMCP_EXPORT int physics_train_report_feedback(
    physics_train_bridge_t* bridge,
    const physics_train_feedback_t* feedback
);

/**
 * @brief Check if plasticity is enabled
 */
NIMCP_EXPORT bool physics_train_is_plasticity_enabled(
    const physics_train_bridge_t* bridge
);

/**
 * @brief Update bridge
 */
NIMCP_EXPORT int physics_train_update(
    physics_train_bridge_t* bridge,
    float dt
);

/**
 * @brief Get statistics
 */
NIMCP_EXPORT int physics_train_get_stats(
    const physics_train_bridge_t* bridge,
    physics_train_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_TRAINING_BRIDGE_H */
