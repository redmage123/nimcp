/**
 * @file nimcp_raphe_thalamic_bridge.h
 * @brief Raphe Nuclei - Thalamic Relay Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between Raphe (serotonin) and thalamic systems
 * WHY:  Enable 5-HT-mediated thalamic modulation and sensory gating
 * HOW:  5-HT affects thalamic oscillations; thalamic state influences Raphe
 *
 * THEORETICAL FOUNDATIONS:
 * - Monti (2011): 5-HT and thalamocortical oscillations
 * - Steriade (1993): Thalamic modulation by brainstem
 * - Portas et al. (1998): 5-HT and sensory processing
 *
 * BIOLOGICAL BASIS:
 * - 5-HT modulates thalamic reticular nucleus
 * - Sensory gating via 5-HT at thalamic level
 * - Sleep-wake transitions involve 5-HT-thalamus
 * - Pain modulation through 5-HT thalamic effects
 *
 * INTEGRATION FLOWS:
 *
 * Raphe --> Thalamus:
 *   1. 5-HT level modulates sensory gating
 *   2. Mood state affects thalamic processing
 *   3. Sleep pressure influences thalamic mode
 *   4. Pain modulation via 5-HT thalamic action
 *
 * Thalamus --> Raphe:
 *   1. Sensory load affects 5-HT release
 *   2. Thalamic synchrony indicates arousal state
 *   3. Pain signals drive 5-HT response
 *   4. Thalamic oscillations phase 5-HT release
 *
 * @see nimcp_raphe.h
 * @see nimcp_thalamic_relay.h
 */

#ifndef NIMCP_RAPHE_THALAMIC_BRIDGE_H
#define NIMCP_RAPHE_THALAMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_raphe_adapter_struct;
typedef struct nimcp_raphe_adapter_struct* nimcp_raphe_adapter_t;
struct nimcp_thalamic_relay;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default sensory gating strength */
#define RAPHE_THAL_GATING_BASELINE      0.5f

/** @brief Pain modulation factor */
#define RAPHE_THAL_PAIN_GAIN            0.8f

/** @brief Bio-async module ID */
#define BIO_MODULE_RAPHE_THALAMIC       0x0E20

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Sensory gating mode
 */
typedef enum {
    RAPHE_THAL_GATE_OPEN = 0,        /**< Minimal gating */
    RAPHE_THAL_GATE_NORMAL,          /**< Normal gating */
    RAPHE_THAL_GATE_TIGHT,           /**< Strong gating */
    RAPHE_THAL_GATE_SELECTIVE        /**< Selective gating */
} nimcp_raphe_thal_gate_t;

/**
 * @brief Thalamic oscillation state
 */
typedef enum {
    RAPHE_THAL_OSC_DELTA = 0,        /**< Delta oscillation (sleep) */
    RAPHE_THAL_OSC_THETA,            /**< Theta oscillation */
    RAPHE_THAL_OSC_ALPHA,            /**< Alpha oscillation (relaxed) */
    RAPHE_THAL_OSC_GAMMA             /**< Gamma oscillation (active) */
} nimcp_raphe_thal_osc_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Raphe-Thalamic bridge configuration
 */
typedef struct {
    /* Sensory gating */
    nimcp_raphe_thal_gate_t default_gate;
    float gating_baseline;           /**< Baseline gating strength */
    float ht5_gating_gain;           /**< 5-HT effect on gating */

    /* Pain modulation */
    bool enable_pain_modulation;     /**< Enable pain gating */
    float pain_gain;                 /**< Pain modulation strength */
    float pain_threshold;            /**< Pain signal threshold */

    /* Oscillation coupling */
    bool enable_osc_coupling;        /**< Couple to oscillations */
    float osc_coupling_strength;     /**< Oscillation coupling */

    /* Feedback */
    float sensory_load_gain;         /**< Sensory load effect */
    float arousal_feedback_gain;     /**< Arousal state feedback */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_raphe_thal_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Thalamic modulation output
 */
typedef struct {
    nimcp_raphe_thal_gate_t gate_mode;  /**< Current gating mode */
    float gating_strength;           /**< Gating strength [0-1] */
    float pain_modulation;           /**< Pain signal modulation */
    float oscillation_influence;     /**< Effect on oscillations */
} nimcp_raphe_thal_modulation_t;

/**
 * @brief Thalamic feedback to Raphe
 */
typedef struct {
    nimcp_raphe_thal_osc_t oscillation; /**< Dominant oscillation */
    float sensory_load;              /**< Current sensory load [0-1] */
    float pain_signal;               /**< Pain signal strength [0-1] */
    float arousal_level;             /**< Thalamic arousal indicator */
    float synchrony;                 /**< Thalamic synchrony */
} nimcp_raphe_thal_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_raphe_thal_gate_t current_gate;
    float current_gating;
    float current_pain_mod;
    float accumulated_sensory;
    nimcp_raphe_thal_osc_t dominant_osc;
} nimcp_raphe_thal_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t gate_changes;
    uint64_t pain_modulations;
    float avg_gating;
    float avg_sensory_load;
    float time_in_tight_gate;
} nimcp_raphe_thal_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_raphe_thal_bridge nimcp_raphe_thal_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_raphe_thal_config_t nimcp_raphe_thal_config_default(void);

nimcp_raphe_thal_bridge_t* nimcp_raphe_thal_create(
    const nimcp_raphe_thal_config_t* config
);

void nimcp_raphe_thal_destroy(nimcp_raphe_thal_bridge_t* bridge);

int nimcp_raphe_thal_reset(nimcp_raphe_thal_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_raphe_thal_connect_raphe(
    nimcp_raphe_thal_bridge_t* bridge,
    nimcp_raphe_adapter_t raphe_adapter
);

int nimcp_raphe_thal_connect_thalamus(
    nimcp_raphe_thal_bridge_t* bridge,
    struct nimcp_thalamic_relay* thalamus
);

/*=============================================================================
 * Raphe --> Thalamus API
 *===========================================================================*/

/**
 * @brief Compute thalamic modulation from 5-HT state
 */
int nimcp_raphe_thal_compute_modulation(
    nimcp_raphe_thal_bridge_t* bridge,
    nimcp_raphe_thal_modulation_t* modulation
);

/**
 * @brief Set gating mode
 */
int nimcp_raphe_thal_set_gate(
    nimcp_raphe_thal_bridge_t* bridge,
    nimcp_raphe_thal_gate_t gate
);

/**
 * @brief Apply pain modulation
 */
int nimcp_raphe_thal_modulate_pain(
    nimcp_raphe_thal_bridge_t* bridge,
    float ht5_level
);

/*=============================================================================
 * Thalamus --> Raphe API
 *===========================================================================*/

/**
 * @brief Receive thalamic feedback
 */
int nimcp_raphe_thal_receive_feedback(
    nimcp_raphe_thal_bridge_t* bridge,
    const nimcp_raphe_thal_feedback_t* feedback
);

/**
 * @brief Process pain signal
 */
int nimcp_raphe_thal_process_pain(
    nimcp_raphe_thal_bridge_t* bridge,
    float pain_level
);

/**
 * @brief Get 5-HT response to thalamic state
 */
float nimcp_raphe_thal_get_ht5_response(nimcp_raphe_thal_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_raphe_thal_update(nimcp_raphe_thal_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_raphe_thal_get_state(
    const nimcp_raphe_thal_bridge_t* bridge,
    nimcp_raphe_thal_bridge_state_t* state
);

int nimcp_raphe_thal_get_stats(
    const nimcp_raphe_thal_bridge_t* bridge,
    nimcp_raphe_thal_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RAPHE_THALAMIC_BRIDGE_H */
