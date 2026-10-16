//=============================================================================
// nimcp_physics_immune_bridge.h - Physics Layer to Immune System Integration
//=============================================================================
/**
 * @file nimcp_physics_immune_bridge.h
 * @brief Bridge connecting Phase 1 Physics modules with Brain Immune System
 *
 * WHAT: Provides bidirectional integration between physics layer modules
 *       (Hodgkin-Huxley, Thermodynamics, Ephaptic) and the brain immune system.
 *
 * WHY:  Physics-immune interactions are biologically significant:
 *       - Temperature affects immune response kinetics
 *       - ATP depletion impairs immune cell function
 *       - Cytokines modulate ion channel conductances
 *       - Inflammation alters membrane properties
 *
 * HOW:  - Monitors physics state and reports to immune system
 *       - Receives immune signals and modulates physics parameters
 *       - Uses bio-async messaging for decoupled communication
 *
 * BIOLOGICAL BASIS:
 * ```
 * PHYSIOLOGICAL MECHANISM              BRIDGE IMPLEMENTATION
 * ──────────────────────────────────────────────────────────────
 * Fever (elevated temp)              → Accelerated immune kinetics
 * Hypothermia                        → Suppressed immune function
 * ATP depletion (ischemia)           → Immune cell dysfunction
 * Inflammatory cytokines (IL-1, TNF) → Ion channel modulation
 * Oxidative stress                   → Membrane permeability changes
 * Local field potentials             → Glial activation patterns
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PHYSICS_IMMUNE_BRIDGE_H
#define NIMCP_PHYSICS_IMMUNE_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "physics/ephaptic/nimcp_ephaptic.h"
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PHYSICS_IMMUNE_MODULE_NAME    "physics_immune_bridge"

/** Normal body temperature (Celsius) */
#define PHYSICS_IMMUNE_NORMAL_TEMP    37.0f

/** Fever threshold (Celsius) */
#define PHYSICS_IMMUNE_FEVER_THRESH   38.0f

/** Hypothermia threshold (Celsius) */
#define PHYSICS_IMMUNE_HYPO_THRESH    35.0f

/** Critical ATP level (fraction of max) */
#define PHYSICS_IMMUNE_ATP_CRITICAL   0.2f

/** Low ATP level triggering immune alert */
#define PHYSICS_IMMUNE_ATP_LOW        0.4f

/** Maximum cytokine modulation factor */
#define PHYSICS_IMMUNE_MAX_CYTOKINE_MOD  2.0f

/** Minimum cytokine modulation factor */
#define PHYSICS_IMMUNE_MIN_CYTOKINE_MOD  0.5f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Physics-immune interaction types
 */
typedef enum {
    PHYSICS_IMMUNE_INTERACTION_NONE = 0,

    /** Temperature effects on immune system */
    PHYSICS_IMMUNE_INTERACTION_TEMP_FEVER,      /**< Elevated temp → faster immune */
    PHYSICS_IMMUNE_INTERACTION_TEMP_HYPO,       /**< Low temp → slower immune */

    /** ATP effects on immune system */
    PHYSICS_IMMUNE_INTERACTION_ATP_LOW,         /**< Low ATP → weakened immune */
    PHYSICS_IMMUNE_INTERACTION_ATP_CRITICAL,    /**< Critical ATP → immune failure */

    /** Immune effects on physics */
    PHYSICS_IMMUNE_INTERACTION_CYTOKINE_NA,     /**< Na channel modulation */
    PHYSICS_IMMUNE_INTERACTION_CYTOKINE_K,      /**< K channel modulation */
    PHYSICS_IMMUNE_INTERACTION_INFLAMMATION,    /**< Membrane permeability change */

    /** Ephaptic-immune interactions */
    PHYSICS_IMMUNE_INTERACTION_LFP_GLIAL,       /**< LFP → glial activation */

    PHYSICS_IMMUNE_INTERACTION_COUNT
} physics_immune_interaction_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Enable temperature monitoring */
    bool monitor_temperature;

    /** Enable ATP monitoring */
    bool monitor_atp;

    /** Enable cytokine modulation */
    bool enable_cytokine_mod;

    /** Enable inflammation effects */
    bool enable_inflammation;

    /** Temperature sampling interval (ms) */
    float temp_sample_interval_ms;

    /** ATP sampling interval (ms) */
    float atp_sample_interval_ms;

    /** Fever response scale factor */
    float fever_response_scale;

    /** Hypothermia response scale factor */
    float hypo_response_scale;

    /** Cytokine modulation strength */
    float cytokine_mod_strength;
} physics_immune_config_t;

/**
 * @brief Physics state reported to immune system
 */
typedef struct {
    /** Current temperature (Celsius) */
    float temperature;

    /** ATP level (fraction of max, 0.0-1.0) */
    float atp_level;

    /** Entropy production rate */
    float entropy_rate;

    /** Average membrane potential (mV) */
    float avg_membrane_potential;

    /** LFP amplitude (mV) */
    float lfp_amplitude;

    /** Dominant LFP frequency (Hz) */
    float lfp_frequency;

    /** Phase coherence */
    float phase_coherence;

    /** Timestamp (ms) */
    float timestamp_ms;
} physics_immune_state_t;

/**
 * @brief Immune modulation applied to physics
 */
typedef struct {
    /** Na+ channel conductance modifier */
    float g_na_modifier;

    /** K+ channel conductance modifier */
    float g_k_modifier;

    /** Leak conductance modifier */
    float g_leak_modifier;

    /** Membrane capacitance modifier */
    float cm_modifier;

    /** Temperature offset (Celsius) */
    float temp_offset;

    /** Q10 modifier */
    float q10_modifier;

    /** Active inflammation level (0.0-1.0) */
    float inflammation_level;

    /** Source cytokine type */
    uint32_t cytokine_source;
} physics_immune_modulation_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /** Total physics→immune reports */
    uint64_t physics_to_immune_count;

    /** Total immune→physics modulations */
    uint64_t immune_to_physics_count;

    /** Temperature alerts sent */
    uint64_t temp_alerts;

    /** ATP alerts sent */
    uint64_t atp_alerts;

    /** Cytokine modulations applied */
    uint64_t cytokine_mods_applied;

    /** Inflammation events */
    uint64_t inflammation_events;

    /** Last update timestamp */
    float last_update_ms;
} physics_immune_stats_t;

/**
 * @brief Opaque bridge structure
 */
typedef struct physics_immune_bridge_struct physics_immune_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_immune_default_config(physics_immune_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create physics-immune bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
NIMCP_EXPORT physics_immune_bridge_t* physics_immune_bridge_create(
    const physics_immune_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void physics_immune_bridge_destroy(physics_immune_bridge_t* bridge);

/**
 * @brief Connect bridge to physics modules
 *
 * @param bridge Bridge instance
 * @param thermo Thermodynamics system (may be NULL)
 * @param hh_pop HH population (may be NULL)
 * @param ephaptic Ephaptic system (may be NULL)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_immune_connect_physics(
    physics_immune_bridge_t* bridge,
    nimcp_thermodynamic_state_t* thermo,
    nimcp_hh_population_t* hh_pop,
    nimcp_ephaptic_system_t* ephaptic
);

/**
 * @brief Connect bridge to immune system
 *
 * @param bridge Bridge instance
 * @param immune Brain immune system
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_immune_connect_immune(
    physics_immune_bridge_t* bridge,
    brain_immune_system_t* immune
);

//=============================================================================
// Physics → Immune API
//=============================================================================

/**
 * @brief Report physics state to immune system
 *
 * WHAT: Collects current physics state and reports to immune
 * WHY:  Immune system needs physics context for response modulation
 * HOW:  Samples thermodynamics, HH, ephaptic and sends report
 *
 * @param bridge Bridge instance
 * @param state Output physics state (optional, may be NULL)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_immune_report_state(
    physics_immune_bridge_t* bridge,
    physics_immune_state_t* state
);

/**
 * @brief Check and report temperature-related alerts
 *
 * @param bridge Bridge instance
 * @return Interaction type detected (or NONE)
 */
NIMCP_EXPORT physics_immune_interaction_t physics_immune_check_temperature(
    physics_immune_bridge_t* bridge
);

/**
 * @brief Check and report ATP-related alerts
 *
 * @param bridge Bridge instance
 * @return Interaction type detected (or NONE)
 */
NIMCP_EXPORT physics_immune_interaction_t physics_immune_check_atp(
    physics_immune_bridge_t* bridge
);

//=============================================================================
// Immune → Physics API
//=============================================================================

/**
 * @brief Apply immune modulation to physics parameters
 *
 * WHAT: Modifies physics parameters based on immune signals
 * WHY:  Cytokines and inflammation affect ion channel function
 * HOW:  Adjusts conductances, Q10, membrane properties
 *
 * @param bridge Bridge instance
 * @param modulation Modulation to apply
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_immune_apply_modulation(
    physics_immune_bridge_t* bridge,
    const physics_immune_modulation_t* modulation
);

/**
 * @brief Process incoming cytokine signal
 *
 * @param bridge Bridge instance
 * @param cytokine_type Cytokine type (from nimcp_microglia.h)
 * @param concentration Cytokine concentration (arbitrary units)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_immune_receive_cytokine(
    physics_immune_bridge_t* bridge,
    uint32_t cytokine_type,
    float concentration
);

/**
 * @brief Process inflammation event
 *
 * @param bridge Bridge instance
 * @param inflammation_level Inflammation severity (0.0-1.0)
 * @param region_x X coordinate of affected region
 * @param region_y Y coordinate
 * @param region_z Z coordinate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_immune_receive_inflammation(
    physics_immune_bridge_t* bridge,
    float inflammation_level,
    float region_x,
    float region_y,
    float region_z
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge (call each simulation step)
 *
 * WHAT: Performs periodic physics-immune synchronization
 * WHY:  Maintains bidirectional state coherence
 * HOW:  Samples physics, applies pending modulations
 *
 * @param bridge Bridge instance
 * @param dt Time step (ms)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_immune_update(
    physics_immune_bridge_t* bridge,
    float dt
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current physics state
 *
 * @param bridge Bridge instance
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_immune_get_state(
    const physics_immune_bridge_t* bridge,
    physics_immune_state_t* state
);

/**
 * @brief Get current modulation being applied
 *
 * @param bridge Bridge instance
 * @param modulation Output modulation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_immune_get_modulation(
    const physics_immune_bridge_t* bridge,
    physics_immune_modulation_t* modulation
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_immune_get_stats(
    const physics_immune_bridge_t* bridge,
    physics_immune_stats_t* stats
);

/**
 * @brief Check if bridge is connected to all systems
 *
 * @param bridge Bridge instance
 * @return true if fully connected
 */
NIMCP_EXPORT bool physics_immune_is_connected(
    const physics_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_IMMUNE_BRIDGE_H */
