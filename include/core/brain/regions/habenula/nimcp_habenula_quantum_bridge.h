/**
 * @file nimcp_habenula_quantum_bridge.h
 * @brief Habenula - Quantum Processing Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between Habenula and quantum layer
 * WHY:  Enable habenula-mediated quantum aversive processing
 * HOW:  Habenula affects quantum negative value computation
 *
 * THEORETICAL FOUNDATIONS:
 * - Speculative: Quantum effects in aversive processing
 * - Busemeyer (2015): Quantum models of decision aversion
 * - Hypothetical: Quantum superposition in avoidance
 *
 * BIOLOGICAL BASIS (SPECULATIVE):
 * - Habenula may influence quantum negative value states
 * - Aversive uncertainty could maintain superposition
 * - Avoidance decisions as quantum measurements
 * - Disappointment may collapse quantum expectations
 *
 * INTEGRATION FLOWS:
 *
 * Habenula --> Quantum:
 *   1. Aversive state affects quantum negative amplitudes
 *   2. Disappointment biases quantum value computation
 *   3. Avoidance mode shapes quantum search
 *   4. Negative PE may trigger quantum collapse
 *
 * Quantum --> Habenula:
 *   1. Quantum uncertainty in aversive outcomes
 *   2. Negative value computations feed habenula
 *   3. Avoidance quantum decisions affect habenula
 *   4. Quantum advantage in loss minimization
 *
 * @see nimcp_habenula.h
 * @see nimcp_quantum_layer.h
 */

#ifndef NIMCP_HABENULA_QUANTUM_BRIDGE_H
#define NIMCP_HABENULA_QUANTUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_habenula_adapter_struct;
typedef struct nimcp_habenula_adapter_struct* nimcp_habenula_adapter_t;
struct nimcp_quantum_processor;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default quantum aversive gain */
#define HAB_QUANTUM_AVERSIVE_GAIN       0.7f

/** @brief Maximum quantum aversive options */
#define HAB_QUANTUM_MAX_OPTIONS         16

/** @brief Bio-async module ID */
#define BIO_MODULE_HAB_QUANTUM_BRIDGE   0x0F50

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Quantum aversive mode
 */
typedef enum {
    HAB_QUANTUM_AVERSE_CLASSICAL = 0, /**< Classical aversive processing */
    HAB_QUANTUM_AVERSE_SUPERPOSED,    /**< Superposed aversive outcomes */
    HAB_QUANTUM_AVERSE_ENTANGLED,     /**< Entangled loss/gain */
    HAB_QUANTUM_AVERSE_COLLAPSED      /**< Collapsed (decided) */
} nimcp_hab_quantum_averse_t;

/**
 * @brief Loss computation mode
 */
typedef enum {
    HAB_QUANTUM_LOSS_EXPECTED = 0,   /**< Expected loss */
    HAB_QUANTUM_LOSS_COHERENT,       /**< Quantum coherent loss */
    HAB_QUANTUM_LOSS_INTERFERED      /**< Interference-computed loss */
} nimcp_hab_quantum_loss_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Habenula-Quantum bridge configuration
 */
typedef struct {
    /* Aversive parameters */
    nimcp_hab_quantum_averse_t default_mode;
    float aversive_gain;             /**< Habenula-quantum coupling */
    float superposition_threshold;   /**< Threshold for superposition */

    /* Loss computation */
    nimcp_hab_quantum_loss_t loss_mode;
    float loss_amplification;        /**< Loss signal amplification */
    float interference_strength;     /**< Loss interference */

    /* Collapse dynamics */
    bool enable_npe_collapse;        /**< NPE triggers collapse */
    float collapse_threshold;        /**< Threshold for collapse */

    /* Avoidance */
    float avoidance_quantum_gain;    /**< Quantum avoidance effect */
    float loss_aversion_factor;      /**< Quantum loss aversion */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_hab_quantum_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Quantum aversive modulation
 */
typedef struct {
    nimcp_hab_quantum_averse_t mode; /**< Current aversive mode */
    float option_amplitudes[HAB_QUANTUM_MAX_OPTIONS]; /**< Option weights */
    uint32_t num_options;            /**< Active options */
    float loss_superposition;        /**< Loss superposition degree */
    bool collapse_pending;           /**< Collapse imminent */
} nimcp_hab_quantum_modulation_t;

/**
 * @brief Quantum feedback to Habenula
 */
typedef struct {
    float quantum_uncertainty;       /**< Aversive uncertainty */
    float coherent_loss;             /**< Quantum-computed loss */
    float interference_contribution; /**< Interference effect */
    bool decision_collapsed;         /**< Decision made */
    uint32_t chosen_option;          /**< Collapsed option (if any) */
    float quantum_loss_advantage;    /**< Quantum loss minimization */
} nimcp_hab_quantum_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_hab_quantum_averse_t current_mode;
    float current_loss_coherence;
    float accumulated_loss;
    bool in_superposition;
    uint32_t active_options;
    float time_in_superposition;
} nimcp_hab_quantum_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t aversive_decisions;
    uint64_t quantum_avoidances;
    uint64_t classical_avoidances;
    float avg_superposition_time;
    float avg_quantum_advantage;
    float total_loss_avoided;
} nimcp_hab_quantum_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_hab_quantum_bridge nimcp_hab_quantum_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_hab_quantum_config_t nimcp_hab_quantum_config_default(void);

nimcp_hab_quantum_bridge_t* nimcp_hab_quantum_create(
    const nimcp_hab_quantum_config_t* config
);

void nimcp_hab_quantum_destroy(nimcp_hab_quantum_bridge_t* bridge);

int nimcp_hab_quantum_reset(nimcp_hab_quantum_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_hab_quantum_connect_habenula(
    nimcp_hab_quantum_bridge_t* bridge,
    nimcp_habenula_adapter_t hab_adapter
);

int nimcp_hab_quantum_connect_quantum(
    nimcp_hab_quantum_bridge_t* bridge,
    struct nimcp_quantum_processor* quantum
);

/*=============================================================================
 * Habenula --> Quantum API
 *===========================================================================*/

/**
 * @brief Compute quantum modulation from habenula state
 */
int nimcp_hab_quantum_compute_modulation(
    nimcp_hab_quantum_bridge_t* bridge,
    nimcp_hab_quantum_modulation_t* modulation
);

/**
 * @brief Initialize aversive superposition
 */
int nimcp_hab_quantum_init_superposition(
    nimcp_hab_quantum_bridge_t* bridge,
    const float* option_losses,
    uint32_t num_options
);

/**
 * @brief Trigger decision collapse
 */
int nimcp_hab_quantum_trigger_collapse(nimcp_hab_quantum_bridge_t* bridge);

/**
 * @brief Bias options with aversive state
 */
int nimcp_hab_quantum_bias_options(
    nimcp_hab_quantum_bridge_t* bridge,
    float aversive_level
);

/*=============================================================================
 * Quantum --> Habenula API
 *===========================================================================*/

/**
 * @brief Receive quantum feedback
 */
int nimcp_hab_quantum_receive_feedback(
    nimcp_hab_quantum_bridge_t* bridge,
    const nimcp_hab_quantum_feedback_t* feedback
);

/**
 * @brief Get habenula response to quantum outcome
 */
float nimcp_hab_quantum_get_hab_response(nimcp_hab_quantum_bridge_t* bridge);

/**
 * @brief Get avoidance drive from quantum state
 */
float nimcp_hab_quantum_get_avoidance(nimcp_hab_quantum_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_hab_quantum_update(nimcp_hab_quantum_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_hab_quantum_get_state(
    const nimcp_hab_quantum_bridge_t* bridge,
    nimcp_hab_quantum_bridge_state_t* state
);

int nimcp_hab_quantum_get_stats(
    const nimcp_hab_quantum_bridge_t* bridge,
    nimcp_hab_quantum_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HABENULA_QUANTUM_BRIDGE_H */
