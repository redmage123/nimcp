/**
 * @file nimcp_vta_quantum_bridge.h
 * @brief Ventral Tegmental Area - Quantum Processing Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between VTA (dopamine) and quantum layer
 * WHY:  Enable DA-mediated quantum decision making and uncertainty
 * HOW:  DA modulates quantum exploration; quantum outcomes affect DA
 *
 * THEORETICAL FOUNDATIONS:
 * - Busemeyer & Bruza (2012): Quantum cognition and decision
 * - Pothos & Busemeyer (2013): Quantum probability in choice
 * - Speculative: DA-quantum interactions in decision
 *
 * BIOLOGICAL BASIS (SPECULATIVE):
 * - DA may influence quantum decision superpositions
 * - Reward uncertainty could maintain quantum coherence
 * - Decision collapse may trigger phasic DA
 * - Exploration-exploitation via quantum superposition
 *
 * INTEGRATION FLOWS:
 *
 * VTA --> Quantum:
 *   1. DA level affects quantum decision superposition
 *   2. Reward salience biases quantum amplitudes
 *   3. Motivation state modulates quantum search
 *   4. RPE may trigger decision collapse
 *
 * Quantum --> VTA:
 *   1. Quantum uncertainty drives exploration via DA
 *   2. Decision outcomes affect phasic DA
 *   3. Coherent value computation feeds DA
 *   4. Quantum advantage in reward optimization
 *
 * @see nimcp_vta.h
 * @see nimcp_quantum_layer.h
 */

#ifndef NIMCP_VTA_QUANTUM_BRIDGE_H
#define NIMCP_VTA_QUANTUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_vta_adapter_struct;
typedef struct nimcp_vta_adapter_struct* nimcp_vta_adapter_t;
struct nimcp_quantum_processor;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default quantum decision gain */
#define VTA_QUANTUM_DECISION_GAIN       0.8f

/** @brief Maximum quantum options */
#define VTA_QUANTUM_MAX_OPTIONS         32

/** @brief Bio-async module ID */
#define BIO_MODULE_VTA_QUANTUM_BRIDGE   0x0D50

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Quantum decision mode
 */
typedef enum {
    VTA_QUANTUM_DECIDE_CLASSICAL = 0, /**< Classical decision */
    VTA_QUANTUM_DECIDE_SUPERPOSITION, /**< Quantum superposition */
    VTA_QUANTUM_DECIDE_ENTANGLED,     /**< Entangled options */
    VTA_QUANTUM_DECIDE_COLLAPSED      /**< Post-collapse state */
} nimcp_vta_quantum_decide_t;

/**
 * @brief Value computation mode
 */
typedef enum {
    VTA_QUANTUM_VALUE_EXPECTED = 0,  /**< Expected value */
    VTA_QUANTUM_VALUE_COHERENT,      /**< Quantum coherent value */
    VTA_QUANTUM_VALUE_INTERFERED     /**< Interference-computed value */
} nimcp_vta_quantum_value_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief VTA-Quantum bridge configuration
 */
typedef struct {
    /* Decision parameters */
    nimcp_vta_quantum_decide_t default_mode;
    float decision_gain;             /**< DA-decision coupling */
    float superposition_threshold;   /**< Threshold for superposition */

    /* Value computation */
    nimcp_vta_quantum_value_t value_mode;
    float interference_strength;     /**< Value interference */
    float coherence_reward_coupling; /**< Coherence-reward link */

    /* Collapse dynamics */
    bool enable_rpe_collapse;        /**< RPE triggers collapse */
    float collapse_threshold;        /**< Threshold for collapse */

    /* Exploration */
    float uncertainty_exploration;   /**< Uncertainty drives explore */
    float quantum_exploration_gain;  /**< Quantum exploration boost */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_vta_quantum_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Quantum decision modulation
 */
typedef struct {
    nimcp_vta_quantum_decide_t mode; /**< Current decision mode */
    float option_amplitudes[VTA_QUANTUM_MAX_OPTIONS]; /**< Option weights */
    uint32_t num_options;            /**< Active options */
    float superposition_degree;      /**< Superposition strength */
    bool collapse_pending;           /**< Collapse imminent */
} nimcp_vta_quantum_modulation_t;

/**
 * @brief Quantum feedback to VTA
 */
typedef struct {
    float quantum_uncertainty;       /**< Decision uncertainty */
    float coherent_value;            /**< Quantum-computed value */
    float interference_bonus;        /**< Interference contribution */
    bool decision_collapsed;         /**< Decision made */
    uint32_t chosen_option;          /**< Collapsed option (if any) */
    float quantum_advantage;         /**< Quantum speedup */
} nimcp_vta_quantum_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_vta_quantum_decide_t current_mode;
    float current_coherence;
    float accumulated_value;
    bool in_superposition;
    uint32_t active_options;
    float time_in_superposition;
} nimcp_vta_quantum_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t decisions_made;
    uint64_t quantum_decisions;
    uint64_t classical_decisions;
    float avg_superposition_time;
    float avg_quantum_advantage;
    float total_interference_bonus;
} nimcp_vta_quantum_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_vta_quantum_bridge nimcp_vta_quantum_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_vta_quantum_config_t nimcp_vta_quantum_config_default(void);

nimcp_vta_quantum_bridge_t* nimcp_vta_quantum_create(
    const nimcp_vta_quantum_config_t* config
);

void nimcp_vta_quantum_destroy(nimcp_vta_quantum_bridge_t* bridge);

int nimcp_vta_quantum_reset(nimcp_vta_quantum_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_vta_quantum_connect_vta(
    nimcp_vta_quantum_bridge_t* bridge,
    nimcp_vta_adapter_t vta_adapter
);

int nimcp_vta_quantum_connect_quantum(
    nimcp_vta_quantum_bridge_t* bridge,
    struct nimcp_quantum_processor* quantum
);

/*=============================================================================
 * VTA --> Quantum API
 *===========================================================================*/

/**
 * @brief Compute quantum modulation from DA state
 */
int nimcp_vta_quantum_compute_modulation(
    nimcp_vta_quantum_bridge_t* bridge,
    nimcp_vta_quantum_modulation_t* modulation
);

/**
 * @brief Initialize decision superposition
 */
int nimcp_vta_quantum_init_superposition(
    nimcp_vta_quantum_bridge_t* bridge,
    const float* option_values,
    uint32_t num_options
);

/**
 * @brief Trigger decision collapse
 */
int nimcp_vta_quantum_trigger_collapse(nimcp_vta_quantum_bridge_t* bridge);

/**
 * @brief Bias option amplitudes with DA
 */
int nimcp_vta_quantum_bias_options(
    nimcp_vta_quantum_bridge_t* bridge,
    float da_level
);

/*=============================================================================
 * Quantum --> VTA API
 *===========================================================================*/

/**
 * @brief Receive quantum feedback
 */
int nimcp_vta_quantum_receive_feedback(
    nimcp_vta_quantum_bridge_t* bridge,
    const nimcp_vta_quantum_feedback_t* feedback
);

/**
 * @brief Get DA response to quantum outcome
 */
float nimcp_vta_quantum_get_da_response(nimcp_vta_quantum_bridge_t* bridge);

/**
 * @brief Get exploration drive from quantum uncertainty
 */
float nimcp_vta_quantum_get_exploration(nimcp_vta_quantum_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_vta_quantum_update(nimcp_vta_quantum_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_vta_quantum_get_state(
    const nimcp_vta_quantum_bridge_t* bridge,
    nimcp_vta_quantum_bridge_state_t* state
);

int nimcp_vta_quantum_get_stats(
    const nimcp_vta_quantum_bridge_t* bridge,
    nimcp_vta_quantum_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VTA_QUANTUM_BRIDGE_H */
