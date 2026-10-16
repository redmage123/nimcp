/**
 * @file nimcp_raphe_quantum_bridge.h
 * @brief Raphe Nuclei - Quantum Processing Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between Raphe (serotonin) and quantum layer
 * WHY:  Enable 5-HT-mediated quantum temporal processing and uncertainty
 * HOW:  5-HT affects quantum temporal coherence; quantum timing affects 5-HT
 *
 * THEORETICAL FOUNDATIONS:
 * - Speculative: Quantum effects in temporal processing
 * - Busemeyer (2015): Quantum temporal decision making
 * - Hypothetical: 5-HT-quantum interactions in timing
 *
 * BIOLOGICAL BASIS (SPECULATIVE):
 * - 5-HT may influence quantum coherence in timing circuits
 * - Temporal uncertainty could be quantum in nature
 * - Patience/impulsivity may involve quantum superpositions
 * - Wait-or-act decisions as quantum measurements
 *
 * INTEGRATION FLOWS:
 *
 * Raphe --> Quantum:
 *   1. 5-HT level affects temporal quantum coherence
 *   2. Patience state maintains quantum superposition
 *   3. Impulse control gates quantum collapse
 *   4. Mood state modulates quantum temporal encoding
 *
 * Quantum --> Raphe:
 *   1. Temporal uncertainty drives 5-HT modulation
 *   2. Quantum timing outcomes affect patience
 *   3. Coherence maintenance feeds back to 5-HT
 *   4. Temporal decision collapse may trigger 5-HT
 *
 * @see nimcp_raphe.h
 * @see nimcp_quantum_layer.h
 */

#ifndef NIMCP_RAPHE_QUANTUM_BRIDGE_H
#define NIMCP_RAPHE_QUANTUM_BRIDGE_H

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
struct nimcp_quantum_processor;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default temporal coherence gain */
#define RAPHE_QUANTUM_TEMPORAL_GAIN     0.6f

/** @brief Maximum temporal options */
#define RAPHE_QUANTUM_MAX_TEMPORAL      16

/** @brief Bio-async module ID */
#define BIO_MODULE_RAPHE_QUANTUM        0x0E50

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Quantum temporal mode
 */
typedef enum {
    RAPHE_QUANTUM_TEMPORAL_CLASSICAL = 0, /**< Classical timing */
    RAPHE_QUANTUM_TEMPORAL_SUPERPOSED,    /**< Temporal superposition */
    RAPHE_QUANTUM_TEMPORAL_ENTANGLED,     /**< Entangled time points */
    RAPHE_QUANTUM_TEMPORAL_COLLAPSED      /**< Collapsed (decided) */
} nimcp_raphe_quantum_temporal_t;

/**
 * @brief Patience quantum state
 */
typedef enum {
    RAPHE_QUANTUM_PATIENCE_LOW = 0,  /**< Low patience (collapse) */
    RAPHE_QUANTUM_PATIENCE_MEDIUM,   /**< Medium patience */
    RAPHE_QUANTUM_PATIENCE_HIGH,     /**< High patience (maintain) */
    RAPHE_QUANTUM_PATIENCE_INFINITE  /**< Maximum patience */
} nimcp_raphe_quantum_patience_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Raphe-Quantum bridge configuration
 */
typedef struct {
    /* Temporal coherence */
    nimcp_raphe_quantum_temporal_t default_mode;
    float temporal_gain;             /**< 5-HT temporal coherence gain */
    float coherence_threshold;       /**< Threshold for coherence */

    /* Patience modulation */
    float patience_coherence_gain;   /**< Patience effect on coherence */
    float impulse_collapse_rate;     /**< Impulse causes collapse */

    /* Temporal decisions */
    bool enable_temporal_decisions;  /**< Model temporal choices */
    float decision_threshold;        /**< Temporal decision threshold */

    /* Feedback */
    float uncertainty_feedback;      /**< Temporal uncertainty effect */
    float collapse_ht5_gain;         /**< Collapse effect on 5-HT */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_raphe_quantum_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Quantum temporal modulation
 */
typedef struct {
    nimcp_raphe_quantum_temporal_t mode; /**< Current temporal mode */
    float temporal_coherence;        /**< Coherence level [0-1] */
    float time_superposition[RAPHE_QUANTUM_MAX_TEMPORAL]; /**< Time probs */
    uint32_t num_time_points;        /**< Active time points */
    nimcp_raphe_quantum_patience_t patience; /**< Patience state */
    bool collapse_imminent;          /**< Collapse approaching */
} nimcp_raphe_quantum_modulation_t;

/**
 * @brief Quantum feedback to Raphe
 */
typedef struct {
    float temporal_uncertainty;      /**< Timing uncertainty [0-1] */
    float coherence_level;           /**< Current coherence */
    bool decision_made;              /**< Temporal decision collapsed */
    uint32_t chosen_time;            /**< Chosen time point (if any) */
    float quantum_timing_advantage;  /**< Quantum timing benefit */
} nimcp_raphe_quantum_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_raphe_quantum_temporal_t current_mode;
    float current_coherence;
    nimcp_raphe_quantum_patience_t patience_state;
    float accumulated_uncertainty;
    bool in_superposition;
    float time_in_superposition;
} nimcp_raphe_quantum_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t temporal_decisions;
    uint64_t patience_maintained;
    uint64_t impulse_collapses;
    float avg_coherence;
    float avg_superposition_time;
    float total_timing_advantage;
} nimcp_raphe_quantum_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_raphe_quantum_bridge nimcp_raphe_quantum_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_raphe_quantum_config_t nimcp_raphe_quantum_config_default(void);

nimcp_raphe_quantum_bridge_t* nimcp_raphe_quantum_create(
    const nimcp_raphe_quantum_config_t* config
);

void nimcp_raphe_quantum_destroy(nimcp_raphe_quantum_bridge_t* bridge);

int nimcp_raphe_quantum_reset(nimcp_raphe_quantum_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_raphe_quantum_connect_raphe(
    nimcp_raphe_quantum_bridge_t* bridge,
    nimcp_raphe_adapter_t raphe_adapter
);

int nimcp_raphe_quantum_connect_quantum(
    nimcp_raphe_quantum_bridge_t* bridge,
    struct nimcp_quantum_processor* quantum
);

/*=============================================================================
 * Raphe --> Quantum API
 *===========================================================================*/

/**
 * @brief Compute quantum modulation from 5-HT state
 */
int nimcp_raphe_quantum_compute_modulation(
    nimcp_raphe_quantum_bridge_t* bridge,
    nimcp_raphe_quantum_modulation_t* modulation
);

/**
 * @brief Initialize temporal superposition
 */
int nimcp_raphe_quantum_init_temporal(
    nimcp_raphe_quantum_bridge_t* bridge,
    const float* time_values,
    uint32_t num_times
);

/**
 * @brief Trigger temporal collapse (impulse)
 */
int nimcp_raphe_quantum_trigger_collapse(nimcp_raphe_quantum_bridge_t* bridge);

/**
 * @brief Maintain temporal coherence (patience)
 */
int nimcp_raphe_quantum_maintain_coherence(
    nimcp_raphe_quantum_bridge_t* bridge,
    float ht5_level
);

/*=============================================================================
 * Quantum --> Raphe API
 *===========================================================================*/

/**
 * @brief Receive quantum feedback
 */
int nimcp_raphe_quantum_receive_feedback(
    nimcp_raphe_quantum_bridge_t* bridge,
    const nimcp_raphe_quantum_feedback_t* feedback
);

/**
 * @brief Get 5-HT response to quantum state
 */
float nimcp_raphe_quantum_get_ht5_response(nimcp_raphe_quantum_bridge_t* bridge);

/**
 * @brief Get patience level from quantum state
 */
float nimcp_raphe_quantum_get_patience(nimcp_raphe_quantum_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_raphe_quantum_update(nimcp_raphe_quantum_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_raphe_quantum_get_state(
    const nimcp_raphe_quantum_bridge_t* bridge,
    nimcp_raphe_quantum_bridge_state_t* state
);

int nimcp_raphe_quantum_get_stats(
    const nimcp_raphe_quantum_bridge_t* bridge,
    nimcp_raphe_quantum_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RAPHE_QUANTUM_BRIDGE_H */
