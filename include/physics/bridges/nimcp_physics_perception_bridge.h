//=============================================================================
// nimcp_physics_perception_bridge.h - Physics Layer to Perception Bridge
//=============================================================================
/**
 * @file nimcp_physics_perception_bridge.h
 * @brief Bidirectional bridge between biophysics and perception systems
 *
 * WHAT: Connects Hodgkin-Huxley, Thermodynamics, and Ephaptic modules with
 *       visual cortex, auditory processing, and multimodal perception.
 *
 * WHY:  Biophysics fundamentally shapes perception:
 *       - Retinal ganglion cells are HH neurons (spike encoding of light)
 *       - Auditory nerve fibers follow HH dynamics (sound transduction)
 *       - LFP oscillations (ephaptic) affect perceptual binding (gamma)
 *       - Temperature affects phototransduction kinetics
 *       - ATP depletion causes visual/auditory fatigue
 *
 * HOW:  Two-way integration:
 *       1. Physics → Perception: Spike trains drive sensory processing
 *       2. Perception → Physics: Attention modulates sensory biophysics
 *       3. Ephaptic → Binding: Gamma oscillations enable feature binding
 *       4. Thermodynamics → Gain: Temperature/ATP affect processing speed
 *
 * BIOLOGICAL BASIS:
 * ```
 * SENSORY BIOPHYSICS                    PERCEPTION
 * ─────────────────────────────────────────────────────────────────
 * Retinal ganglion cells (HH)        → Visual cortex input
 * Auditory nerve fibers (HH)         → Auditory processing input
 * LFP gamma (30-100Hz)               → Feature binding coherence
 * LFP alpha (8-12Hz)                 → Inhibitory gating
 * Temperature (Q10)                  → Phototransduction speed
 * ATP level                          → Signal amplification gain
 * ```
 *
 * PERCEPTUAL MODULATION PATHWAYS:
 * - Visual attention → Retinal gain modulation
 * - Auditory attention → Hair cell sensitivity
 * - Cross-modal binding → Synchronized gamma
 * - Perceptual load → ATP consumption
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PHYSICS_PERCEPTION_BRIDGE_H
#define NIMCP_PHYSICS_PERCEPTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PHYSICS_PERCEPT_MODULE_NAME   "physics_perception"

/** Maximum sensory channels */
#define PHYSICS_PERCEPT_MAX_CHANNELS  128

/** Gamma band center frequency (Hz) */
#define PHYSICS_PERCEPT_GAMMA_CENTER  40.0f

/** Alpha band center frequency (Hz) */
#define PHYSICS_PERCEPT_ALPHA_CENTER  10.0f

/** Default phototransduction time constant (ms) */
#define PHYSICS_PERCEPT_PHOTO_TAU     30.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Sensory modality
 */
typedef enum {
    PHYSICS_PERCEPT_VISUAL = 0,      /**< Visual processing */
    PHYSICS_PERCEPT_AUDITORY,        /**< Auditory processing */
    PHYSICS_PERCEPT_SOMATOSENSORY,   /**< Touch/proprioception */
    PHYSICS_PERCEPT_MULTIMODAL       /**< Cross-modal binding */
} physics_percept_modality_t;

/**
 * @brief Oscillation band for perceptual binding
 */
typedef enum {
    PHYSICS_PERCEPT_BAND_DELTA = 0,  /**< 1-4 Hz: drowsiness */
    PHYSICS_PERCEPT_BAND_THETA,      /**< 4-8 Hz: memory encoding */
    PHYSICS_PERCEPT_BAND_ALPHA,      /**< 8-12 Hz: inhibitory gating */
    PHYSICS_PERCEPT_BAND_BETA,       /**< 12-30 Hz: motor/attention */
    PHYSICS_PERCEPT_BAND_GAMMA       /**< 30-100 Hz: feature binding */
} physics_percept_band_t;

/**
 * @brief Perceptual state
 */
typedef enum {
    PHYSICS_PERCEPT_STATE_NORMAL = 0,
    PHYSICS_PERCEPT_STATE_ENHANCED,  /**< Attention-enhanced */
    PHYSICS_PERCEPT_STATE_DEGRADED,  /**< Fatigue/low ATP */
    PHYSICS_PERCEPT_STATE_MASKED     /**< Cross-modal masking */
} physics_percept_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for physics-perception bridge
 */
typedef struct {
    /** Modality settings */
    bool enable_visual;              /**< Enable visual pathway */
    bool enable_auditory;            /**< Enable auditory pathway */
    bool enable_binding;             /**< Enable cross-modal binding */

    /** Biophysical parameters */
    float photo_tau_ms;              /**< Phototransduction time constant */
    float audio_tau_ms;              /**< Hair cell time constant */
    float temp_q10;                  /**< Q10 for sensory transduction */

    /** Oscillation parameters */
    float gamma_binding_threshold;   /**< Gamma coherence for binding */
    float alpha_inhibition_gain;     /**< Alpha inhibition strength */

    /** Metabolic parameters */
    float atp_gain_min;              /**< Min gain at ATP=0 */
    float atp_gain_max;              /**< Max gain at ATP=1 */
    float atp_fatigue_threshold;     /**< ATP below which fatigue occurs */

    /** Update rate */
    float update_interval_ms;        /**< Bridge update interval */
} physics_percept_config_t;

/**
 * @brief Sensory input from physics layer
 */
typedef struct {
    physics_percept_modality_t modality;
    uint32_t channel_id;             /**< Sensory channel (e.g., pixel, frequency bin) */
    float spike_rate_hz;             /**< Firing rate from HH neurons */
    float intensity;                 /**< Raw stimulus intensity */
    float temperature;               /**< Local temperature (affects kinetics) */
    float atp_level;                 /**< Local ATP (affects gain) */
} physics_percept_input_t;

/**
 * @brief Processed sensory signal
 */
typedef struct {
    physics_percept_modality_t modality;
    uint32_t channel_id;
    float processed_value;           /**< Physics-modulated signal */
    float gain;                      /**< Applied gain (ATP/temp) */
    float binding_coherence;         /**< Gamma coherence with other channels */
    physics_percept_state_t state;   /**< Current perceptual state */
} physics_percept_output_t;

/**
 * @brief Binding state for cross-modal perception
 */
typedef struct {
    float gamma_coherence;           /**< Current gamma coherence */
    float alpha_power;               /**< Current alpha power (inhibition) */
    float binding_strength;          /**< Effective binding strength */
    uint32_t bound_channels;         /**< Number of bound channels */
    bool binding_active;             /**< Whether binding is active */
} physics_percept_binding_t;

/**
 * @brief Attention modulation signal
 */
typedef struct {
    physics_percept_modality_t target_modality;
    float attention_gain;            /**< Attention-based gain (0.5-2.0) */
    float spatial_focus_x;           /**< Spatial attention center X */
    float spatial_focus_y;           /**< Spatial attention center Y */
    float focus_radius;              /**< Attention spotlight radius */
} physics_percept_attention_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t visual_inputs;          /**< Visual signals processed */
    uint64_t auditory_inputs;        /**< Auditory signals processed */
    uint64_t binding_events;         /**< Cross-modal binding events */
    uint64_t fatigue_events;         /**< ATP-induced fatigue events */
    float avg_visual_gain;           /**< Average visual gain */
    float avg_auditory_gain;         /**< Average auditory gain */
    float avg_binding_coherence;     /**< Average binding coherence */
    float last_update_ms;            /**< Last update timestamp */
} physics_percept_stats_t;

/** Opaque bridge handle */
typedef struct physics_percept_bridge_struct physics_percept_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_percept_default_config(physics_percept_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create physics-perception bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT physics_percept_bridge_t* physics_percept_bridge_create(
    const physics_percept_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void physics_percept_bridge_destroy(physics_percept_bridge_t* bridge);

//=============================================================================
// Sensory Input API (Physics → Perception)
//=============================================================================

/**
 * @brief Process sensory input through physics layer
 *
 * WHAT: Applies biophysical transformations to sensory signals
 * WHY:  Models how physics affects perception
 * HOW:  Temperature/ATP modulation of gain and timing
 *
 * @param bridge Bridge handle
 * @param input Raw sensory input
 * @param output Processed output (optional)
 * @return Processed signal value
 */
NIMCP_EXPORT float physics_percept_process_input(
    physics_percept_bridge_t* bridge,
    const physics_percept_input_t* input,
    physics_percept_output_t* output
);

/**
 * @brief Batch process multiple sensory channels
 *
 * @param bridge Bridge handle
 * @param inputs Array of inputs
 * @param outputs Array of outputs
 * @param count Number of inputs
 * @return Number processed successfully
 */
NIMCP_EXPORT int physics_percept_process_batch(
    physics_percept_bridge_t* bridge,
    const physics_percept_input_t* inputs,
    physics_percept_output_t* outputs,
    uint32_t count
);

//=============================================================================
// Oscillation/Binding API
//=============================================================================

/**
 * @brief Set ephaptic oscillation state for binding
 *
 * WHAT: Updates gamma/alpha power from ephaptic module
 * WHY:  Oscillations control perceptual binding and gating
 * HOW:  Gamma enables binding, alpha gates input
 *
 * @param bridge Bridge handle
 * @param gamma_power Gamma band power (30-100 Hz)
 * @param alpha_power Alpha band power (8-12 Hz)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_percept_set_oscillations(
    physics_percept_bridge_t* bridge,
    float gamma_power,
    float alpha_power
);

/**
 * @brief Get current binding state
 *
 * @param bridge Bridge handle
 * @param binding Output binding state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_percept_get_binding(
    const physics_percept_bridge_t* bridge,
    physics_percept_binding_t* binding
);

/**
 * @brief Check if cross-modal binding is active
 *
 * @param bridge Bridge handle
 * @return true if binding active (gamma above threshold)
 */
NIMCP_EXPORT bool physics_percept_is_binding_active(
    const physics_percept_bridge_t* bridge
);

//=============================================================================
// Attention Modulation API (Perception → Physics)
//=============================================================================

/**
 * @brief Apply attention modulation to sensory biophysics
 *
 * WHAT: Modulates HH gain based on attention
 * WHY:  Attention affects sensory neuron responsiveness
 * HOW:  Gain factor applied to sensory transduction
 *
 * @param bridge Bridge handle
 * @param attention Attention signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_percept_apply_attention(
    physics_percept_bridge_t* bridge,
    const physics_percept_attention_t* attention
);

/**
 * @brief Get current attention state for modality
 *
 * @param bridge Bridge handle
 * @param modality Target modality
 * @param attention Output attention state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_percept_get_attention(
    const physics_percept_bridge_t* bridge,
    physics_percept_modality_t modality,
    physics_percept_attention_t* attention
);

//=============================================================================
// Metabolic API
//=============================================================================

/**
 * @brief Set current temperature for kinetics scaling
 *
 * @param bridge Bridge handle
 * @param temperature_k Temperature in Kelvin
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_percept_set_temperature(
    physics_percept_bridge_t* bridge,
    float temperature_k
);

/**
 * @brief Set current ATP level for gain modulation
 *
 * @param bridge Bridge handle
 * @param atp_level ATP level (0.0-1.0)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_percept_set_atp(
    physics_percept_bridge_t* bridge,
    float atp_level
);

/**
 * @brief Check if sensory fatigue is occurring
 *
 * @param bridge Bridge handle
 * @return true if ATP below fatigue threshold
 */
NIMCP_EXPORT bool physics_percept_is_fatigued(
    const physics_percept_bridge_t* bridge
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_percept_update(
    physics_percept_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_percept_reset(physics_percept_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_percept_get_stats(
    const physics_percept_bridge_t* bridge,
    physics_percept_stats_t* stats
);

/**
 * @brief Get current gain for modality (temperature/ATP adjusted)
 *
 * @param bridge Bridge handle
 * @param modality Target modality
 * @return Current gain factor
 */
NIMCP_EXPORT float physics_percept_get_gain(
    const physics_percept_bridge_t* bridge,
    physics_percept_modality_t modality
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_PERCEPTION_BRIDGE_H */
