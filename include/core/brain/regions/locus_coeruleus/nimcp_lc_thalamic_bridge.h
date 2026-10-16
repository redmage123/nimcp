/**
 * @file nimcp_lc_thalamic_bridge.h
 * @brief Locus Coeruleus - Thalamic Relay Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between LC and thalamic relay systems
 * WHY:  Enable NE-mediated thalamic gating for attention and sensory filtering
 * HOW:  LC modulates thalamic relay mode; thalamic state feeds back to LC
 *
 * THEORETICAL FOUNDATIONS:
 * - McCormick (1992): Thalamic neuromodulation and state transitions
 * - Steriade (2000): Thalamic oscillations and attention
 * - Aston-Jones & Cohen (2005): LC-NE and thalamic function
 *
 * BIOLOGICAL BASIS:
 * - NE shifts thalamic neurons from burst to tonic mode
 * - Tonic mode enables faithful sensory relay
 * - Burst mode gates sensory transmission
 * - LC arousal signals prepare thalamic relay for processing
 *
 * INTEGRATION FLOWS:
 *
 * LC --> Thalamus:
 *   1. NE concentration modulates relay mode
 *   2. Arousal state sets thalamic gain
 *   3. Phasic bursts trigger thalamic reset
 *   4. Vigilance level gates sensory transmission
 *
 * Thalamus --> LC:
 *   1. Thalamic synchrony indicates processing load
 *   2. Relay failures trigger LC attention boost
 *   3. Sensory onset signals drive phasic responses
 *   4. Thalamic oscillation phase modulates LC timing
 *
 * @see nimcp_locus_coeruleus.h
 * @see nimcp_thalamic_relay.h
 */

#ifndef NIMCP_LC_THALAMIC_BRIDGE_H
#define NIMCP_LC_THALAMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_lc_adapter_struct;
typedef struct nimcp_lc_adapter_struct* nimcp_lc_adapter_t;
struct nimcp_thalamic_relay;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default NE threshold for tonic mode */
#define LC_THAL_TONIC_THRESHOLD         0.3f

/** @brief Default relay gain range */
#define LC_THAL_GAIN_MIN                0.5f
#define LC_THAL_GAIN_MAX                2.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_LC_THALAMIC_BRIDGE   0x0C20

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Thalamic relay mode (NE-modulated)
 */
typedef enum {
    LC_THAL_MODE_BURST = 0,          /**< Burst mode (gated, sleep) */
    LC_THAL_MODE_TONIC,              /**< Tonic mode (relay, awake) */
    LC_THAL_MODE_TRANSITION          /**< Transitioning between modes */
} nimcp_lc_thal_mode_t;

/**
 * @brief Relay state for feedback to LC
 */
typedef enum {
    LC_THAL_RELAY_IDLE = 0,          /**< No active relay */
    LC_THAL_RELAY_ACTIVE,            /**< Active sensory relay */
    LC_THAL_RELAY_GATED,             /**< Relay gated/blocked */
    LC_THAL_RELAY_OVERLOAD           /**< Processing overload */
} nimcp_lc_thal_relay_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief LC-Thalamic bridge configuration
 */
typedef struct {
    /* Mode control */
    float tonic_threshold;           /**< NE threshold for tonic mode */
    float burst_threshold;           /**< NE threshold for burst mode */
    float mode_transition_tau_ms;    /**< Mode transition time constant */

    /* Gain modulation */
    float gain_min;                  /**< Minimum relay gain */
    float gain_max;                  /**< Maximum relay gain */
    float gain_sensitivity;          /**< NE-to-gain sensitivity */

    /* Gating */
    bool enable_gating;              /**< Enable sensory gating */
    float gating_threshold;          /**< Threshold for gating */
    float gating_decay_tau_ms;       /**< Gating decay constant */

    /* Feedback */
    float feedback_gain;             /**< Thalamic feedback gain */
    float overload_threshold;        /**< Threshold for overload signal */

    /* Update */
    float update_interval_ms;        /**< Bridge update interval */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} nimcp_lc_thal_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Thalamic modulation output
 */
typedef struct {
    nimcp_lc_thal_mode_t mode;       /**< Current relay mode */
    float relay_gain;                /**< Current relay gain */
    float sensory_gate;              /**< Sensory gating level [0-1] */
    float oscillation_coupling;      /**< Coupling to thalamic rhythms */
    bool attention_boost;            /**< Attention boost active */
} nimcp_lc_thal_modulation_t;

/**
 * @brief Thalamic feedback to LC
 */
typedef struct {
    nimcp_lc_thal_relay_t relay_state;   /**< Current relay state */
    float processing_load;           /**< Processing load [0-1] */
    float synchrony;                 /**< Thalamic synchrony [0-1] */
    float sensory_onset;             /**< Recent sensory onset strength */
    bool overload_signal;            /**< Processing overload flag */
} nimcp_lc_thal_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_lc_thal_mode_t current_mode;
    float current_gain;
    float mode_transition_progress;
    float accumulated_feedback;
    bool in_transition;
} nimcp_lc_thal_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t mode_transitions;
    uint64_t overload_events;
    float avg_relay_gain;
    float time_in_tonic;
    float time_in_burst;
    float avg_processing_load;
} nimcp_lc_thal_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_lc_thal_bridge nimcp_lc_thal_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_lc_thal_config_t nimcp_lc_thal_config_default(void);

nimcp_lc_thal_bridge_t* nimcp_lc_thal_create(
    const nimcp_lc_thal_config_t* config
);

void nimcp_lc_thal_destroy(nimcp_lc_thal_bridge_t* bridge);

int nimcp_lc_thal_reset(nimcp_lc_thal_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_lc_thal_connect_lc(
    nimcp_lc_thal_bridge_t* bridge,
    nimcp_lc_adapter_t lc_adapter
);

int nimcp_lc_thal_connect_thalamus(
    nimcp_lc_thal_bridge_t* bridge,
    struct nimcp_thalamic_relay* thalamus
);

/*=============================================================================
 * LC --> Thalamus API
 *===========================================================================*/

/**
 * @brief Compute thalamic modulation from LC state
 */
int nimcp_lc_thal_compute_modulation(
    nimcp_lc_thal_bridge_t* bridge,
    nimcp_lc_thal_modulation_t* modulation
);

/**
 * @brief Set thalamic relay mode based on NE level
 */
int nimcp_lc_thal_set_mode(
    nimcp_lc_thal_bridge_t* bridge,
    float ne_concentration
);

/**
 * @brief Trigger attention boost
 */
int nimcp_lc_thal_boost_attention(nimcp_lc_thal_bridge_t* bridge);

/*=============================================================================
 * Thalamus --> LC API
 *===========================================================================*/

/**
 * @brief Receive thalamic feedback
 */
int nimcp_lc_thal_receive_feedback(
    nimcp_lc_thal_bridge_t* bridge,
    const nimcp_lc_thal_feedback_t* feedback
);

/**
 * @brief Process sensory onset signal
 */
int nimcp_lc_thal_process_sensory_onset(
    nimcp_lc_thal_bridge_t* bridge,
    float onset_magnitude
);

/**
 * @brief Get recommended LC response to thalamic state
 */
float nimcp_lc_thal_get_lc_response(nimcp_lc_thal_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_lc_thal_update(nimcp_lc_thal_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_lc_thal_get_state(
    const nimcp_lc_thal_bridge_t* bridge,
    nimcp_lc_thal_bridge_state_t* state
);

int nimcp_lc_thal_get_stats(
    const nimcp_lc_thal_bridge_t* bridge,
    nimcp_lc_thal_stats_t* stats
);

nimcp_lc_thal_mode_t nimcp_lc_thal_get_mode(
    const nimcp_lc_thal_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LC_THALAMIC_BRIDGE_H */
