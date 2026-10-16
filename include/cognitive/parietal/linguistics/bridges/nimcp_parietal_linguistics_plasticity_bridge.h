/**
 * @file nimcp_parietal_linguistics_plasticity_bridge.h
 * @brief Plasticity Integration Bridge for Parietal Linguistics
 * @version 1.0.0
 * @date 2026-01-31
 *
 * WHAT: Integrates STDP, BCM, structural plasticity, and homeostatic mechanisms
 *       for linguistics-specific learning
 * WHY:  Language learning requires multiple plasticity mechanisms:
 *       - Pairwise STDP: Word-meaning associations (timing-based binding)
 *       - Triplet STDP: Sequence learning ("twenty-one" compound numbers)
 *       - R-STDP: Reward-modulated vocabulary acquisition
 *       - BCM: Competitive word selection (winner-take-all)
 *       - Structural: Vocabulary lifecycle (new words, consolidation, forgetting)
 *       - Homeostatic: Maintain balance between frequent and rare words
 *
 * HOW:  Provides unified interface to all plasticity mechanisms for linguistics
 *       with mesh coordinator integration for distributed processing
 *
 * BIOLOGICAL BASIS:
 * - Angular Gyrus: Word-meaning associations via STDP binding
 * - Intraparietal Sulcus: Number word sequences via triplet STDP
 * - Supramarginal Gyrus: Phonological patterns via structural plasticity
 * - Basal ganglia feedback: Reward-modulated R-STDP for vocabulary acquisition
 * - Cortical competition: BCM for word selection and semantic disambiguation
 *
 * MESH INTEGRATION:
 * - Implements linguistics_mesh_handler_t interface
 * - Participates in FEP-based consensus for plasticity decisions
 * - Provides precision based on learning stability and convergence
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PARIETAL_LINGUISTICS_PLASTICITY_BRIDGE_H
#define NIMCP_PARIETAL_LINGUISTICS_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Linguistics types */
#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_types.h"

/* Plasticity systems */
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/stdp/nimcp_triplet_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"

/* Mesh coordinator */
#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_mesh.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Magic number for validation */
#define LING_PLASTICITY_MAGIC 0x4C504C41  /* "LPLA" */

/** Maximum number of synapses per word */
#define LING_PLASTICITY_MAX_SYNAPSES_PER_WORD 256

/** Maximum number of words in vocabulary for plasticity tracking */
#define LING_PLASTICITY_MAX_VOCABULARY 10000

/** Default learning rate for word associations */
#define LING_PLASTICITY_DEFAULT_LR 0.01f

/** Default eligibility trace decay (half-life ~14ms) */
#define LING_PLASTICITY_DEFAULT_TRACE_DECAY 0.95f

/** Dopamine burst threshold for 4-factor learning */
#define LING_PLASTICITY_BURST_THRESHOLD 6.0f

/** BCM threshold for competitive word selection */
#define LING_PLASTICITY_BCM_THRESHOLD 0.5f

/* ============================================================================
 * ERROR CODES
 * ============================================================================ */

typedef enum {
    LING_PLASTICITY_OK = 0,
    LING_PLASTICITY_ERR_NULL,
    LING_PLASTICITY_ERR_INVALID,
    LING_PLASTICITY_ERR_NO_MEMORY,
    LING_PLASTICITY_ERR_NOT_FOUND,
    LING_PLASTICITY_ERR_CAPACITY,
    LING_PLASTICITY_ERR_MESH,
    LING_PLASTICITY_ERR_STDP,
    LING_PLASTICITY_ERR_BCM,
    LING_PLASTICITY_ERR_STRUCTURAL,
    LING_PLASTICITY_ERR_HOMEOSTATIC,
    LING_PLASTICITY_ERR_ELIGIBILITY
} ling_plasticity_error_t;

/* ============================================================================
 * PLASTICITY RULE TYPES
 * ============================================================================ */

/**
 * @brief Plasticity rule type for linguistics
 */
typedef enum {
    LING_PLASTICITY_RULE_PAIRWISE_STDP,   /**< Classic STDP (Bi & Poo 1998) */
    LING_PLASTICITY_RULE_TRIPLET_STDP,    /**< Triplet STDP (Pfister & Gerstner 2006) */
    LING_PLASTICITY_RULE_R_STDP,          /**< Reward-modulated STDP */
    LING_PLASTICITY_RULE_BCM,             /**< BCM competitive learning */
    LING_PLASTICITY_RULE_COMBINED         /**< Multiple rules active */
} ling_plasticity_rule_t;

/**
 * @brief Spine state for vocabulary lifecycle
 *
 * Maps to structural plasticity spine states for word representations
 */
typedef enum {
    LING_SPINE_NASCENT,      /**< New word exposure - thin spine, unstable */
    LING_SPINE_STABLE,       /**< Consolidated vocabulary - mushroom spine */
    LING_SPINE_POTENTIATED,  /**< High-frequency word - enlarged spine */
    LING_SPINE_PRUNING,      /**< Disused word - marked for forgetting */
    LING_SPINE_ELIMINATED    /**< Forgotten - synapse removed */
} ling_spine_state_t;

/**
 * @brief Homeostatic mechanism type
 */
typedef enum {
    LING_HOMEOSTATIC_SCALING,    /**< Synaptic scaling for vocabulary balance */
    LING_HOMEOSTATIC_INTRINSIC,  /**< Intrinsic plasticity for word thresholds */
    LING_HOMEOSTATIC_BCM_THRESHOLD /**< BCM threshold sliding */
} ling_homeostatic_mechanism_t;

/* ============================================================================
 * SYNAPSE TYPES FOR LINGUISTICS
 * ============================================================================ */

/**
 * @brief Word-meaning synapse for pairwise/R-STDP
 *
 * Represents connection between word representation and semantic meaning
 */
typedef struct {
    uint32_t word_id;          /**< Source word ID */
    uint32_t meaning_id;       /**< Target meaning/concept ID */
    stdp_synapse_t stdp;       /**< STDP synapse state */
    float eligibility_trace;   /**< Eligibility trace for delayed reward */
    ling_spine_state_t spine_state; /**< Structural state */
    uint64_t last_activation;  /**< Last activation time (ms) */
    uint32_t activation_count; /**< Number of activations */
    bool dopamine_tagged;      /**< Tagged for consolidation during DA burst */
} ling_word_synapse_t;

/**
 * @brief Sequence synapse for triplet STDP
 *
 * Represents connection between consecutive words in sequences
 */
typedef struct {
    uint32_t word_a_id;        /**< First word in sequence */
    uint32_t word_b_id;        /**< Second word in sequence */
    triplet_stdp_synapse_t triplet; /**< Triplet STDP state */
    float sequence_strength;   /**< Overall sequence binding strength */
    ling_spine_state_t spine_state; /**< Structural state */
    uint64_t last_activation;  /**< Last activation time (ms) */
} ling_sequence_synapse_t;

/**
 * @brief BCM synapse for competitive word selection
 */
typedef struct {
    uint32_t word_id;          /**< Word ID */
    bcm_synapse_t bcm;         /**< BCM synapse state */
    float competition_score;   /**< Current competition score */
    uint64_t last_selection;   /**< Last time word was selected */
} ling_bcm_synapse_t;

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/**
 * @brief STDP configuration for linguistics
 */
typedef struct {
    /* Pairwise STDP */
    float pairwise_lr;         /**< Learning rate (default: 0.01) */
    float a_plus;              /**< LTP amplitude (default: 0.005) */
    float a_minus;             /**< LTD amplitude (default: 0.00525) */
    float tau_plus;            /**< LTP time constant [ms] (default: 20) */
    float tau_minus;           /**< LTD time constant [ms] (default: 20) */

    /* Dopamine modulation (R-STDP) */
    bool enable_da_modulation; /**< Enable reward modulation (default: true) */
    float da_gain;             /**< DA concentration scaling (default: 100.0) */
    float burst_amplification; /**< Burst multiplier (default: 3.0) */
    float burst_threshold;     /**< DA level for burst (default: 6.0) */

    /* Eligibility traces */
    float trace_decay;         /**< Trace decay rate (default: 0.95) */
    float trace_lr_mult;       /**< Trace learning rate multiplier (default: 1.0) */
} ling_stdp_config_t;

/**
 * @brief Triplet STDP configuration for sequence learning
 */
typedef struct {
    /* Pairwise components */
    float A2_plus;             /**< Pairwise LTP amplitude (default: 0.005) */
    float A2_minus;            /**< Pairwise LTD amplitude (default: 0.007) */

    /* Triplet components */
    float A3_plus;             /**< Triplet LTP amplitude (default: 0.0062) */
    float A3_minus;            /**< Triplet LTD amplitude (default: 0.00023) */

    /* Time constants */
    float tau_plus;            /**< Fast pre-trace (default: 16.8 ms) */
    float tau_minus;           /**< Fast post-trace (default: 33.7 ms) */
    float tau_x;               /**< Slow pre-trace (default: 101 ms) */
    float tau_y;               /**< Slow post-trace (default: 125 ms) */

    /* Frequency dependence */
    float low_freq_threshold;  /**< Below this, pairwise dominates (default: 10 Hz) */
    float high_freq_threshold; /**< Above this, triplet amplifies (default: 40 Hz) */
} ling_triplet_config_t;

/**
 * @brief BCM configuration for competitive learning
 */
typedef struct {
    float learning_rate;           /**< BCM learning rate (default: 0.01) */
    float threshold_tau;           /**< Threshold time constant (default: 10000 ms) */
    float initial_threshold;       /**< Initial BCM threshold (default: 0.5) */
    float competition_strength;    /**< Lateral inhibition strength (default: 0.8) */
    bool enable_winner_take_all;   /**< Enable WTA dynamics (default: true) */
} ling_bcm_config_t;

/**
 * @brief Structural plasticity configuration
 */
typedef struct {
    /* Formation thresholds */
    float formation_threshold;     /**< Activity threshold for spine formation (default: 20 Hz) */
    float stabilization_threshold; /**< Repeated activation for stabilization (default: 5) */

    /* Pruning thresholds */
    float pruning_threshold;       /**< Activity threshold for pruning (default: 0.5 Hz) */
    uint32_t pruning_timeout_ms;   /**< Time before pruning starts (default: 86400000 = 1 day) */

    /* Lifecycle timing */
    uint32_t nascent_duration_ms;  /**< Max time in nascent state (default: 3600000 = 1 hour) */
    uint32_t maturation_time_ms;   /**< Time for full stabilization (default: 604800000 = 7 days) */

    /* State transition probabilities */
    float stabilization_prob;      /**< Probability of nascent → stable (default: 0.8) */
    float potentiation_prob;       /**< Probability of stable → potentiated (default: 0.6) */
    float recovery_prob;           /**< Probability of pruning → stable (default: 0.3) */
} ling_structural_config_t;

/**
 * @brief Homeostatic plasticity configuration
 */
typedef struct {
    /* Synaptic scaling */
    float target_rate;             /**< Target firing rate (default: 5.0 Hz) */
    float scaling_tau;             /**< Scaling time constant (default: 3600000 ms = 1 hour) */
    float scaling_exponent;        /**< Scaling exponent (default: 1.0) */

    /* Intrinsic plasticity */
    float ip_tau;                  /**< Intrinsic plasticity time constant (default: 1000 ms) */
    float min_threshold;           /**< Minimum threshold (default: 0.1) */
    float max_threshold;           /**< Maximum threshold (default: 0.9) */

    /* BCM threshold */
    float bcm_tau;                 /**< BCM threshold time constant (default: 10000 ms) */
} ling_homeostatic_config_t;

/**
 * @brief Complete plasticity bridge configuration
 */
typedef struct {
    /* Sub-configurations */
    ling_stdp_config_t stdp;
    ling_triplet_config_t triplet;
    ling_bcm_config_t bcm;
    ling_structural_config_t structural;
    ling_homeostatic_config_t homeostatic;

    /* General settings */
    bool enable_mesh;              /**< Enable mesh participation (default: true) */
    uint32_t max_vocabulary_size;  /**< Maximum vocabulary (default: 10000) */
    uint32_t update_interval_ms;   /**< Plasticity update interval (default: 10) */

    /* Active mechanisms */
    bool enable_pairwise_stdp;     /**< Enable pairwise STDP (default: true) */
    bool enable_triplet_stdp;      /**< Enable triplet STDP (default: true) */
    bool enable_r_stdp;            /**< Enable reward-modulated STDP (default: true) */
    bool enable_bcm;               /**< Enable BCM competition (default: true) */
    bool enable_structural;        /**< Enable structural plasticity (default: true) */
    bool enable_homeostatic;       /**< Enable homeostatic mechanisms (default: true) */
} ling_plasticity_config_t;

/* ============================================================================
 * PLASTICITY EVENTS
 * ============================================================================ */

/**
 * @brief Plasticity event type
 */
typedef enum {
    LING_PLASTICITY_EVENT_LTP,          /**< Long-term potentiation */
    LING_PLASTICITY_EVENT_LTD,          /**< Long-term depression */
    LING_PLASTICITY_EVENT_FORMATION,    /**< New synapse formed */
    LING_PLASTICITY_EVENT_STABILIZATION,/**< Synapse stabilized */
    LING_PLASTICITY_EVENT_POTENTIATION, /**< Synapse potentiated */
    LING_PLASTICITY_EVENT_PRUNING,      /**< Synapse pruning started */
    LING_PLASTICITY_EVENT_ELIMINATION,  /**< Synapse eliminated */
    LING_PLASTICITY_EVENT_SCALING,      /**< Homeostatic scaling applied */
    LING_PLASTICITY_EVENT_BCM_WIN,      /**< Word won BCM competition */
    LING_PLASTICITY_EVENT_BCM_LOSE      /**< Word lost BCM competition */
} ling_plasticity_event_type_t;

/**
 * @brief Plasticity event data
 */
typedef struct {
    ling_plasticity_event_type_t type;
    uint32_t word_id;
    uint32_t target_id;        /**< Meaning ID or second word ID */
    float weight_change;
    float new_weight;
    ling_spine_state_t spine_state;
    uint64_t timestamp_ms;
} ling_plasticity_event_t;

/**
 * @brief Plasticity event callback
 */
typedef void (*ling_plasticity_callback_t)(
    const ling_plasticity_event_t* event,
    void* user_data
);

/* ============================================================================
 * BRIDGE STATISTICS
 * ============================================================================ */

/**
 * @brief Plasticity bridge statistics
 */
typedef struct {
    /* Synapse counts */
    uint32_t total_word_synapses;
    uint32_t total_sequence_synapses;
    uint32_t total_bcm_synapses;

    /* Spine state counts */
    uint32_t nascent_count;
    uint32_t stable_count;
    uint32_t potentiated_count;
    uint32_t pruning_count;
    uint32_t eliminated_count;

    /* Learning events */
    uint64_t total_ltp_events;
    uint64_t total_ltd_events;
    uint64_t total_da_burst_events;
    uint64_t total_formations;
    uint64_t total_eliminations;
    uint64_t total_bcm_competitions;

    /* Weight statistics */
    float mean_weight;
    float weight_variance;
    float max_weight;
    float min_weight;

    /* Learning rates */
    float current_effective_lr;
    float homeostatic_scaling_factor;
    float bcm_threshold;

    /* Mesh statistics */
    float mesh_precision;
    uint32_t mesh_updates;
} ling_plasticity_stats_t;

/* ============================================================================
 * BRIDGE HANDLE
 * ============================================================================ */

/**
 * @brief Opaque plasticity bridge handle
 */
typedef struct ling_plasticity_bridge_s ling_plasticity_bridge_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default plasticity configuration
 *
 * @return Default configuration with biologically plausible parameters
 */
ling_plasticity_config_t ling_plasticity_config_default(void);

/**
 * @brief Create plasticity bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
ling_plasticity_bridge_t* ling_plasticity_create(const ling_plasticity_config_t* config);

/**
 * @brief Destroy plasticity bridge
 *
 * @param bridge Bridge to destroy
 */
void ling_plasticity_destroy(ling_plasticity_bridge_t* bridge);

/**
 * @brief Reset plasticity bridge to initial state
 *
 * @param bridge Bridge to reset
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_reset(ling_plasticity_bridge_t* bridge);

/* ============================================================================
 * MESH INTEGRATION
 * ============================================================================ */

/**
 * @brief Register with mesh coordinator
 *
 * @param bridge Plasticity bridge
 * @param mesh Mesh coordinator
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_register_mesh(
    ling_plasticity_bridge_t* bridge,
    linguistics_mesh_t* mesh
);

/**
 * @brief Get mesh handler for external registration
 *
 * @param bridge Plasticity bridge
 * @param handler Output handler structure
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_get_mesh_handler(
    ling_plasticity_bridge_t* bridge,
    linguistics_mesh_handler_t* handler
);

/* ============================================================================
 * WORD SYNAPSE API (Pairwise/R-STDP)
 * ============================================================================ */

/**
 * @brief Create word-meaning synapse
 *
 * @param bridge Plasticity bridge
 * @param word_id Word ID
 * @param meaning_id Meaning/concept ID
 * @param initial_weight Initial weight (0.0-1.0)
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_create_word_synapse(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    float initial_weight
);

/**
 * @brief Process pre-synaptic spike for word synapse
 *
 * @param bridge Plasticity bridge
 * @param word_id Word ID
 * @param meaning_id Meaning ID
 * @param time_ms Current time (ms)
 * @return Weight change applied
 */
float ling_plasticity_word_pre_spike(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    float time_ms
);

/**
 * @brief Process post-synaptic spike for word synapse
 *
 * @param bridge Plasticity bridge
 * @param word_id Word ID
 * @param meaning_id Meaning ID
 * @param time_ms Current time (ms)
 * @return Weight change applied
 */
float ling_plasticity_word_post_spike(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    float time_ms
);

/**
 * @brief Process reward signal for word synapse (R-STDP)
 *
 * @param bridge Plasticity bridge
 * @param word_id Word ID
 * @param meaning_id Meaning ID
 * @param reward Reward value (-1.0 to 1.0)
 * @param dopamine_level Current dopamine level
 * @return Weight change applied
 */
float ling_plasticity_word_reward(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    float reward,
    float dopamine_level
);

/**
 * @brief Get word synapse weight
 *
 * @param bridge Plasticity bridge
 * @param word_id Word ID
 * @param meaning_id Meaning ID
 * @param weight Output weight
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_get_word_weight(
    const ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    float* weight
);

/* ============================================================================
 * SEQUENCE SYNAPSE API (Triplet STDP)
 * ============================================================================ */

/**
 * @brief Create sequence synapse between consecutive words
 *
 * @param bridge Plasticity bridge
 * @param word_a First word ID
 * @param word_b Second word ID
 * @param initial_weight Initial weight
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_create_sequence_synapse(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_a,
    uint32_t word_b,
    float initial_weight
);

/**
 * @brief Process spike for sequence learning (triplet STDP)
 *
 * @param bridge Plasticity bridge
 * @param word_a First word ID
 * @param word_b Second word ID
 * @param time_ms Current time (ms)
 * @param is_post True if post-synaptic spike
 * @return Weight change applied
 */
float ling_plasticity_sequence_spike(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_a,
    uint32_t word_b,
    float time_ms,
    bool is_post
);

/**
 * @brief Learn word sequence (e.g., "twenty" "one" for 21)
 *
 * @param bridge Plasticity bridge
 * @param word_ids Array of word IDs in sequence
 * @param num_words Number of words
 * @param frequency Presentation frequency (Hz)
 * @return Total weight change
 */
float ling_plasticity_learn_sequence(
    ling_plasticity_bridge_t* bridge,
    const uint32_t* word_ids,
    uint32_t num_words,
    float frequency
);

/* ============================================================================
 * BCM COMPETITIVE LEARNING API
 * ============================================================================ */

/**
 * @brief Create BCM synapse for word competition
 *
 * @param bridge Plasticity bridge
 * @param word_id Word ID
 * @param initial_weight Initial weight
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_create_bcm_synapse(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    float initial_weight
);

/**
 * @brief Update BCM synapse with activity
 *
 * @param bridge Plasticity bridge
 * @param word_id Word ID
 * @param pre_activity Pre-synaptic activity
 * @param post_activity Post-synaptic activity
 * @return Weight change applied
 */
float ling_plasticity_bcm_update(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    float pre_activity,
    float post_activity
);

/**
 * @brief Run BCM competition between candidate words
 *
 * @param bridge Plasticity bridge
 * @param candidate_ids Array of competing word IDs
 * @param num_candidates Number of candidates
 * @param winner_id Output winning word ID
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_bcm_compete(
    ling_plasticity_bridge_t* bridge,
    const uint32_t* candidate_ids,
    uint32_t num_candidates,
    uint32_t* winner_id
);

/* ============================================================================
 * STRUCTURAL PLASTICITY API
 * ============================================================================ */

/**
 * @brief Get spine state for word synapse
 *
 * @param bridge Plasticity bridge
 * @param word_id Word ID
 * @param meaning_id Meaning ID
 * @param state Output spine state
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_get_spine_state(
    const ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    ling_spine_state_t* state
);

/**
 * @brief Process structural update (call periodically)
 *
 * Handles:
 * - Nascent → Stable transitions (if stabilized)
 * - Stable → Potentiated transitions (if strengthened)
 * - Stable → Pruning transitions (if unused)
 * - Pruning → Eliminated transitions (if timeout)
 *
 * @param bridge Plasticity bridge
 * @param dt_ms Time since last update (ms)
 * @return Number of state transitions
 */
uint32_t ling_plasticity_structural_update(
    ling_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Mark word for consolidation (tag during dopamine burst)
 *
 * @param bridge Plasticity bridge
 * @param word_id Word ID
 * @param meaning_id Meaning ID
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_tag_for_consolidation(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id
);

/**
 * @brief Process sleep consolidation
 *
 * @param bridge Plasticity bridge
 * @param sleep_state Current sleep state
 * @param duration_ms Sleep duration (ms)
 * @return Number of synapses consolidated
 */
uint32_t ling_plasticity_sleep_consolidation(
    ling_plasticity_bridge_t* bridge,
    sleep_state_t sleep_state,
    float duration_ms
);

/* ============================================================================
 * HOMEOSTATIC API
 * ============================================================================ */

/**
 * @brief Apply synaptic scaling to maintain vocabulary balance
 *
 * @param bridge Plasticity bridge
 * @param target_rate Target mean firing rate
 * @return Scaling factor applied
 */
float ling_plasticity_apply_scaling(
    ling_plasticity_bridge_t* bridge,
    float target_rate
);

/**
 * @brief Update intrinsic plasticity thresholds
 *
 * @param bridge Plasticity bridge
 * @param dt_ms Time since last update (ms)
 */
void ling_plasticity_intrinsic_update(
    ling_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update BCM threshold (sliding threshold)
 *
 * @param bridge Plasticity bridge
 * @param mean_activity Mean network activity
 */
void ling_plasticity_bcm_threshold_update(
    ling_plasticity_bridge_t* bridge,
    float mean_activity
);

/* ============================================================================
 * ELIGIBILITY TRACE API
 * ============================================================================ */

/**
 * @brief Create eligibility trace for delayed reward
 *
 * @param bridge Plasticity bridge
 * @param word_id Word ID
 * @param meaning_id Meaning ID
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_create_eligibility_trace(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id
);

/**
 * @brief Decay all eligibility traces (call each timestep)
 *
 * @param bridge Plasticity bridge
 * @param dt_ms Time step (ms)
 */
void ling_plasticity_decay_traces(
    ling_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply reward to all active eligibility traces
 *
 * @param bridge Plasticity bridge
 * @param reward Reward value
 * @param dopamine_level Current dopamine level
 * @return Total weight change
 */
float ling_plasticity_apply_reward_to_traces(
    ling_plasticity_bridge_t* bridge,
    float reward,
    float dopamine_level
);

/* ============================================================================
 * BATCH OPERATIONS
 * ============================================================================ */

/**
 * @brief Update all plasticity traces (call each timestep)
 *
 * @param bridge Plasticity bridge
 * @param dt_ms Time step (ms)
 */
void ling_plasticity_update_traces(
    ling_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Run full plasticity update cycle
 *
 * Combines: trace update, structural update, homeostatic update
 *
 * @param bridge Plasticity bridge
 * @param dt_ms Time since last update
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_full_update(
    ling_plasticity_bridge_t* bridge,
    float dt_ms
);

/* ============================================================================
 * STATISTICS & MONITORING
 * ============================================================================ */

/**
 * @brief Get plasticity statistics
 *
 * @param bridge Plasticity bridge
 * @param stats Output statistics
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_get_stats(
    const ling_plasticity_bridge_t* bridge,
    ling_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Plasticity bridge
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_reset_stats(ling_plasticity_bridge_t* bridge);

/**
 * @brief Register event callback
 *
 * @param bridge Plasticity bridge
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return LING_PLASTICITY_OK on success
 */
int ling_plasticity_register_callback(
    ling_plasticity_bridge_t* bridge,
    ling_plasticity_callback_t callback,
    void* user_data
);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* ling_plasticity_get_last_error(void);

/**
 * @brief Get spine state name
 *
 * @param state Spine state
 * @return Human-readable state name
 */
const char* ling_plasticity_spine_state_name(ling_spine_state_t state);

/**
 * @brief Get plasticity rule name
 *
 * @param rule Plasticity rule
 * @return Human-readable rule name
 */
const char* ling_plasticity_rule_name(ling_plasticity_rule_t rule);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_LINGUISTICS_PLASTICITY_BRIDGE_H */
