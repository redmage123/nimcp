/**
 * @file nimcp_lc_quantum_bridge.h
 * @brief Locus Coeruleus - Quantum Processing Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between LC (norepinephrine) and quantum layer
 * WHY:  Enable NE-mediated quantum coherence modulation and quantum feedback
 * HOW:  NE affects decoherence rates; quantum states influence LC dynamics
 *
 * THEORETICAL FOUNDATIONS:
 * - Penrose & Hameroff (2014): Orchestrated objective reduction
 * - Fisher (2015): Quantum cognition in neural phosphorus
 * - Adams & Petruccione (2020): Quantum effects in neuromodulation
 *
 * BIOLOGICAL BASIS (SPECULATIVE):
 * - NE may affect microtubule dynamics (coherence maintenance)
 * - Quantum superposition states could influence neural computations
 * - Arousal state may gate quantum-classical transitions
 * - Phasic bursts could trigger quantum measurement events
 *
 * INTEGRATION FLOWS:
 *
 * LC --> Quantum:
 *   1. NE concentration modulates decoherence rate
 *   2. Arousal state affects quantum resource allocation
 *   3. Phasic bursts trigger coherence collapse/measurement
 *   4. Exploration mode enables quantum superposition search
 *
 * Quantum --> LC:
 *   1. Coherence level feeds back to tonic NE
 *   2. Quantum uncertainty affects exploration drive
 *   3. Entanglement state influences global coordination
 *   4. Measurement events may trigger phasic responses
 *
 * @see nimcp_locus_coeruleus.h
 * @see nimcp_quantum_layer.h
 */

#ifndef NIMCP_LC_QUANTUM_BRIDGE_H
#define NIMCP_LC_QUANTUM_BRIDGE_H

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
struct nimcp_quantum_processor;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default coherence modulation gain */
#define LC_QUANTUM_COHERENCE_GAIN       0.5f

/** @brief Default decoherence baseline rate */
#define LC_QUANTUM_DECOHERENCE_BASE     0.1f

/** @brief Maximum qubits tracked */
#define LC_QUANTUM_MAX_QUBITS           64

/** @brief Bio-async module ID */
#define BIO_MODULE_LC_QUANTUM_BRIDGE    0x0C50

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Quantum processing mode
 */
typedef enum {
    LC_QUANTUM_MODE_CLASSICAL = 0,   /**< Classical processing only */
    LC_QUANTUM_MODE_COHERENT,        /**< Quantum coherent processing */
    LC_QUANTUM_MODE_HYBRID,          /**< Hybrid quantum-classical */
    LC_QUANTUM_MODE_MEASUREMENT      /**< Measurement/collapse phase */
} nimcp_lc_quantum_mode_t;

/**
 * @brief Quantum state for LC feedback
 */
typedef enum {
    LC_QUANTUM_STATE_GROUND = 0,     /**< Ground state (classical) */
    LC_QUANTUM_STATE_SUPERPOSITION,  /**< Superposition active */
    LC_QUANTUM_STATE_ENTANGLED,      /**< Entangled states present */
    LC_QUANTUM_STATE_DECOHERING      /**< Decoherence in progress */
} nimcp_lc_quantum_state_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief LC-Quantum bridge configuration
 */
typedef struct {
    /* Coherence modulation */
    float coherence_gain;            /**< NE effect on coherence */
    float decoherence_baseline;      /**< Baseline decoherence rate */
    float ne_coherence_threshold;    /**< NE threshold for coherence boost */

    /* Measurement */
    bool enable_phasic_measurement;  /**< Phasic bursts trigger measurement */
    float measurement_threshold;     /**< Threshold for measurement trigger */

    /* Quantum feedback */
    float coherence_feedback_gain;   /**< Coherence effect on NE */
    float uncertainty_exploration_gain; /**< Uncertainty drives exploration */

    /* Entanglement */
    bool enable_entanglement;        /**< Enable entanglement effects */
    float entanglement_threshold;    /**< Threshold for entanglement feedback */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_lc_quantum_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Quantum modulation output
 */
typedef struct {
    nimcp_lc_quantum_mode_t mode;    /**< Current quantum mode */
    float decoherence_rate;          /**< NE-modulated decoherence */
    float coherence_boost;           /**< Coherence maintenance boost */
    float resource_allocation;       /**< Quantum resource fraction */
    bool measurement_triggered;      /**< Measurement event triggered */
} nimcp_lc_quantum_modulation_t;

/**
 * @brief Quantum feedback to LC
 */
typedef struct {
    nimcp_lc_quantum_state_t state;  /**< Current quantum state */
    float coherence_level;           /**< Overall coherence [0-1] */
    float entanglement_strength;     /**< Entanglement measure [0-1] */
    float quantum_uncertainty;       /**< Measurement uncertainty [0-1] */
    bool collapse_event;             /**< Recent collapse event */
    float quantum_advantage;         /**< Estimated quantum advantage */
} nimcp_lc_quantum_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_lc_quantum_mode_t current_mode;
    float current_coherence;
    float current_decoherence_rate;
    float accumulated_uncertainty;
    bool measurement_pending;
    float time_since_measurement;
} nimcp_lc_quantum_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t measurement_events;
    uint64_t coherence_boosts;
    uint64_t mode_transitions;
    float avg_coherence;
    float avg_decoherence_rate;
    float total_quantum_time;
    float quantum_utilization;
} nimcp_lc_quantum_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_lc_quantum_bridge nimcp_lc_quantum_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_lc_quantum_config_t nimcp_lc_quantum_config_default(void);

nimcp_lc_quantum_bridge_t* nimcp_lc_quantum_create(
    const nimcp_lc_quantum_config_t* config
);

void nimcp_lc_quantum_destroy(nimcp_lc_quantum_bridge_t* bridge);

int nimcp_lc_quantum_reset(nimcp_lc_quantum_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_lc_quantum_connect_lc(
    nimcp_lc_quantum_bridge_t* bridge,
    nimcp_lc_adapter_t lc_adapter
);

int nimcp_lc_quantum_connect_quantum(
    nimcp_lc_quantum_bridge_t* bridge,
    struct nimcp_quantum_processor* quantum
);

/*=============================================================================
 * LC --> Quantum API
 *===========================================================================*/

/**
 * @brief Compute quantum modulation from LC state
 */
int nimcp_lc_quantum_compute_modulation(
    nimcp_lc_quantum_bridge_t* bridge,
    nimcp_lc_quantum_modulation_t* modulation
);

/**
 * @brief Trigger measurement/collapse event
 */
int nimcp_lc_quantum_trigger_measurement(nimcp_lc_quantum_bridge_t* bridge);

/**
 * @brief Set quantum mode based on NE level
 */
int nimcp_lc_quantum_set_mode(
    nimcp_lc_quantum_bridge_t* bridge,
    float ne_concentration
);

/**
 * @brief Get coherence boost factor
 */
float nimcp_lc_quantum_get_coherence_boost(nimcp_lc_quantum_bridge_t* bridge);

/*=============================================================================
 * Quantum --> LC API
 *===========================================================================*/

/**
 * @brief Receive quantum feedback
 */
int nimcp_lc_quantum_receive_feedback(
    nimcp_lc_quantum_bridge_t* bridge,
    const nimcp_lc_quantum_feedback_t* feedback
);

/**
 * @brief Get recommended LC response
 */
float nimcp_lc_quantum_get_lc_response(nimcp_lc_quantum_bridge_t* bridge);

/**
 * @brief Get exploration drive from quantum uncertainty
 */
float nimcp_lc_quantum_get_exploration_drive(nimcp_lc_quantum_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_lc_quantum_update(nimcp_lc_quantum_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_lc_quantum_get_state(
    const nimcp_lc_quantum_bridge_t* bridge,
    nimcp_lc_quantum_bridge_state_t* state
);

int nimcp_lc_quantum_get_stats(
    const nimcp_lc_quantum_bridge_t* bridge,
    nimcp_lc_quantum_stats_t* stats
);

nimcp_lc_quantum_mode_t nimcp_lc_quantum_get_mode(
    const nimcp_lc_quantum_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LC_QUANTUM_BRIDGE_H */
