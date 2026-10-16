//=============================================================================
// nimcp_physics_cognitive_bridge.h - Physics Layer to Cognitive Layer Bridge
//=============================================================================
/**
 * @file nimcp_physics_cognitive_bridge.h
 * @brief Bridge connecting Phase 1 Physics modules with Cognitive Layer
 *
 * WHAT: Provides bidirectional integration between physics layer modules
 *       (HH biophysics, Thermodynamics, Ephaptic) and cognitive functions.
 *
 * WHY:  Physics-cognition coupling is essential for realistic brain modeling:
 *       - Neural excitability affects cognitive processing speed
 *       - Ephaptic coherence enables global workspace broadcasting
 *       - ATP levels gate cognitive effort/capacity
 *       - Temperature affects reaction times
 *
 * HOW:  - Reports physics state as cognitive context
 *       - Receives attention/arousal signals to modulate excitability
 *       - Implements bidirectional capacity-performance coupling
 *
 * BIOLOGICAL BASIS:
 * ```
 * PHYSICS STATE                      COGNITIVE EFFECT
 * ─────────────────────────────────────────────────────────────────────────
 * High firing rate (HH)          →   Increased processing speed
 * Low ATP (Thermo)               →   Reduced cognitive capacity
 * High phase coherence (Eph)     →   Enhanced binding/integration
 * Temperature deviation          →   Impaired attention
 * High entropy production        →   Fatigue/reduced vigilance
 *
 * COGNITIVE OUTPUT                   PHYSICS MODULATION
 * ─────────────────────────────────────────────────────────────────────────
 * High attention allocation      →   Enhanced excitability in region
 * Task engagement                →   Increased metabolic demand
 * Working memory load            →   Sustained activity patterns
 * Stress/arousal                 →   Noradrenergic conductance modulation
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PHYSICS_COGNITIVE_BRIDGE_H
#define NIMCP_PHYSICS_COGNITIVE_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "physics/ephaptic/nimcp_ephaptic.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PHYSICS_COG_MODULE_NAME    "physics_cognitive_bridge"

/** Default attention influence on excitability */
#define PHYSICS_COG_ATTENTION_GAIN  0.2f

/** Default arousal influence on conductance */
#define PHYSICS_COG_AROUSAL_GAIN    0.15f

/** ATP threshold for cognitive impairment */
#define PHYSICS_COG_ATP_IMPAIR      0.4f

/** Coherence threshold for binding enhancement */
#define PHYSICS_COG_COHERENCE_THRESH 0.6f

/** Maximum capacity scale factor */
#define PHYSICS_COG_MAX_CAPACITY    1.5f

/** Minimum capacity scale factor */
#define PHYSICS_COG_MIN_CAPACITY    0.3f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Cognitive capacity factors
 */
typedef enum {
    /** Processing speed (reaction time) */
    PHYSICS_COG_FACTOR_SPEED = 0,

    /** Working memory capacity */
    PHYSICS_COG_FACTOR_CAPACITY,

    /** Sustained attention */
    PHYSICS_COG_FACTOR_ATTENTION,

    /** Integration/binding */
    PHYSICS_COG_FACTOR_BINDING,

    /** Executive control */
    PHYSICS_COG_FACTOR_CONTROL,

    PHYSICS_COG_FACTOR_COUNT
} physics_cog_factor_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Enable biophysics influence on cognition */
    bool enable_physics_to_cog;

    /** Enable cognitive influence on physics */
    bool enable_cog_to_physics;

    /** Enable ATP-based capacity scaling */
    bool enable_atp_scaling;

    /** Enable coherence-based binding */
    bool enable_coherence_binding;

    /** Enable temperature effects */
    bool enable_temperature_effects;

    /** Attention gain factor (0.0-1.0) */
    float attention_gain;

    /** Arousal gain factor (0.0-1.0) */
    float arousal_gain;

    /** ATP impairment threshold */
    float atp_impairment_threshold;

    /** Coherence threshold for binding */
    float coherence_threshold;

    /** Update interval (ms) */
    float update_interval_ms;
} physics_cog_config_t;

/**
 * @brief Physics state for cognition
 */
typedef struct {
    /** Average firing rate (Hz) */
    float firing_rate;

    /** Population synchrony (0.0-1.0) */
    float synchrony;

    /** ATP level (0.0-1.0) */
    float atp_level;

    /** Temperature deviation from optimal (Celsius) */
    float temp_deviation;

    /** Entropy production rate */
    float entropy_rate;

    /** Ephaptic phase coherence (0.0-1.0) */
    float phase_coherence;

    /** LFP dominant frequency (Hz) */
    float lfp_frequency;

    /** Neural excitability (0.0-1.0) */
    float excitability;

    /** Timestamp (ms) */
    float timestamp_ms;
} physics_cog_state_t;

/**
 * @brief Cognitive capacity factors
 */
typedef struct {
    /** Processing speed factor (0.0-2.0, 1.0 = baseline) */
    float speed_factor;

    /** Working memory capacity factor */
    float capacity_factor;

    /** Attention factor */
    float attention_factor;

    /** Binding/integration factor */
    float binding_factor;

    /** Executive control factor */
    float control_factor;

    /** Overall cognitive efficiency (geometric mean) */
    float overall_efficiency;

    /** Is system impaired? */
    bool impaired;

    /** Impairment reason (if any) */
    uint32_t impairment_reason;

    /** Timestamp (ms) */
    float timestamp_ms;
} physics_cog_capacity_t;

/**
 * @brief Cognitive feedback to physics
 */
typedef struct {
    /** Attention allocation per region [0.0-1.0] */
    float attention_level;

    /** Arousal/vigilance state [0.0-1.0] */
    float arousal_level;

    /** Working memory load [0.0-1.0] */
    float wm_load;

    /** Task engagement [0.0-1.0] */
    float engagement;

    /** Stress level [0.0-1.0] */
    float stress_level;

    /** Required excitability modulation */
    float excitability_mod;

    /** Required metabolic demand */
    float metabolic_demand;

    /** Timestamp (ms) */
    float timestamp_ms;
} physics_cog_feedback_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /** Total physics → cognitive reports */
    uint64_t physics_to_cog_count;

    /** Total cognitive → physics feedbacks */
    uint64_t cog_to_physics_count;

    /** Impairment events */
    uint64_t impairment_events;

    /** Binding enhancement events */
    uint64_t binding_enhancements;

    /** Average capacity factor */
    float avg_capacity_factor;

    /** Average attention level */
    float avg_attention_level;

    /** Last update timestamp */
    float last_update_ms;
} physics_cog_stats_t;

/**
 * @brief Impairment reasons
 */
typedef enum {
    PHYSICS_COG_IMPAIR_NONE = 0,
    PHYSICS_COG_IMPAIR_LOW_ATP     = (1 << 0),
    PHYSICS_COG_IMPAIR_TEMP_HIGH   = (1 << 1),
    PHYSICS_COG_IMPAIR_TEMP_LOW    = (1 << 2),
    PHYSICS_COG_IMPAIR_LOW_SYNC    = (1 << 3),
    PHYSICS_COG_IMPAIR_HIGH_ENTROPY= (1 << 4)
} physics_cog_impairment_t;

/**
 * @brief Opaque bridge structure
 */
typedef struct physics_cog_bridge_struct physics_cog_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_default_config(physics_cog_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create physics-cognitive bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
NIMCP_EXPORT physics_cog_bridge_t* physics_cog_bridge_create(
    const physics_cog_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void physics_cog_bridge_destroy(physics_cog_bridge_t* bridge);

/**
 * @brief Connect bridge to physics modules
 *
 * @param bridge Bridge instance
 * @param thermo Thermodynamics system (may be NULL)
 * @param hh_pop HH population (may be NULL)
 * @param ephaptic Ephaptic system (may be NULL)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_connect_physics(
    physics_cog_bridge_t* bridge,
    nimcp_thermodynamic_state_t* thermo,
    nimcp_hh_population_t* hh_pop,
    nimcp_ephaptic_system_t* ephaptic
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_reset(physics_cog_bridge_t* bridge);

//=============================================================================
// Physics -> Cognitive API
//=============================================================================

/**
 * @brief Report physics state for cognitive processing
 *
 * WHAT: Collect physics state affecting cognitive performance
 * WHY:  Cognition depends on neural substrate state
 * HOW:  Sample biophysics, thermodynamics, ephaptic
 *
 * @param bridge Bridge instance
 * @param state Output physics state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_report_state(
    physics_cog_bridge_t* bridge,
    physics_cog_state_t* state
);

/**
 * @brief Compute cognitive capacity factors
 *
 * WHAT: Calculate how physics state affects cognitive capacity
 * WHY:  Enable physics-grounded cognitive performance
 * HOW:  Map physics variables to capacity factors
 *
 * @param bridge Bridge instance
 * @param capacity Output capacity factors
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_compute_capacity(
    physics_cog_bridge_t* bridge,
    physics_cog_capacity_t* capacity
);

/**
 * @brief Get single capacity factor
 *
 * @param bridge Bridge instance
 * @param factor Factor type
 * @return Factor value (1.0 = baseline)
 */
NIMCP_EXPORT float physics_cog_get_factor(
    const physics_cog_bridge_t* bridge,
    physics_cog_factor_t factor
);

//=============================================================================
// Cognitive -> Physics API
//=============================================================================

/**
 * @brief Receive cognitive feedback
 *
 * WHAT: Accept attention/arousal signals from cognitive layer
 * WHY:  Cognitive demands affect neural excitability
 * HOW:  Store feedback for physics modulation
 *
 * @param bridge Bridge instance
 * @param feedback Cognitive feedback
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_receive_feedback(
    physics_cog_bridge_t* bridge,
    const physics_cog_feedback_t* feedback
);

/**
 * @brief Apply cognitive modulation to physics
 *
 * WHAT: Modulate physics based on cognitive demands
 * WHY:  Attention enhances excitability in attended regions
 * HOW:  Scale conductances, thresholds based on feedback
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_apply_modulation(physics_cog_bridge_t* bridge);

/**
 * @brief Set attention level directly
 *
 * @param bridge Bridge instance
 * @param attention Attention level (0.0-1.0)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_set_attention(
    physics_cog_bridge_t* bridge,
    float attention
);

/**
 * @brief Set arousal level directly
 *
 * @param bridge Bridge instance
 * @param arousal Arousal level (0.0-1.0)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_set_arousal(
    physics_cog_bridge_t* bridge,
    float arousal
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Full bridge update cycle
 *
 * WHAT: Perform complete physics ↔ cognitive synchronization
 * WHY:  Single call for bidirectional integration
 * HOW:  Report state → Compute capacity → Apply modulation
 *
 * @param bridge Bridge instance
 * @param dt Time step (ms)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_update(
    physics_cog_bridge_t* bridge,
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
NIMCP_EXPORT int physics_cog_get_state(
    const physics_cog_bridge_t* bridge,
    physics_cog_state_t* state
);

/**
 * @brief Get current capacity
 *
 * @param bridge Bridge instance
 * @param capacity Output capacity
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_get_capacity(
    const physics_cog_bridge_t* bridge,
    physics_cog_capacity_t* capacity
);

/**
 * @brief Get current feedback
 *
 * @param bridge Bridge instance
 * @param feedback Output feedback
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_get_feedback(
    const physics_cog_bridge_t* bridge,
    physics_cog_feedback_t* feedback
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_cog_get_stats(
    const physics_cog_bridge_t* bridge,
    physics_cog_stats_t* stats
);

/**
 * @brief Check if system is impaired
 *
 * @param bridge Bridge instance
 * @return true if cognitive capacity is impaired
 */
NIMCP_EXPORT bool physics_cog_is_impaired(const physics_cog_bridge_t* bridge);

/**
 * @brief Get impairment reasons
 *
 * @param bridge Bridge instance
 * @return Bitmask of impairment reasons
 */
NIMCP_EXPORT uint32_t physics_cog_get_impairment(
    const physics_cog_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_COGNITIVE_BRIDGE_H */
