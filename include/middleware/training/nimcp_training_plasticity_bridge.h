//=============================================================================
// nimcp_training_plasticity_bridge.h - Training-Plasticity Integration Bridge
//=============================================================================
/**
 * @file nimcp_training_plasticity_bridge.h
 * @brief Bridge connecting training pipeline to biological plasticity systems
 *
 * Phase TPB-1: Training-Plasticity Bridge
 *
 * WHAT: Bidirectional bridge between computational training and biological plasticity
 * WHY:  Enable biologically-inspired learning by connecting loss-based training to
 *       neuromodulator-gated plasticity mechanisms
 * HOW:  Event-driven architecture with reward prediction error, region-specific routing
 *
 * ARCHITECTURE:
 * +===========================================================================+
 * |                    Training-Plasticity Bridge                              |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |   Training Pipeline   |       | Biological Plasticity |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | Loss Functions        |       | STDP (Hebbian)        |                |
 * |  | Optimizers            |       | BCM (Homeostatic)     |                |
 * |  | LR Schedulers         |       | Neuromodulators       |                |
 * |  | Gradient Management   |       | Three-Factor Learning |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |              Loss-Dopamine Connector (RPE)               |            |
 * |  |  loss_delta → reward_prediction_error → DA release       |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |           Region-Specific Plasticity Router              |            |
 * |  |  weight_targets → region → (STDP|BCM|Homeostatic)        |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |          Neuromodulator-LR Modulator                     |            |
 * |  |  DA/ACh/5-HT levels → learning_rate_multiplier           |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 *
 * BIOLOGICAL BASIS:
 * - Reward Prediction Error (RPE): Loss decrease → dopamine burst (positive RPE)
 *                                  Loss increase → dopamine dip (negative RPE)
 * - Three-Factor Learning: Hebbian timing × Reward signal × Dopamine modulation
 * - Regional Specialization: Different brain regions use different learning rules
 *   - Cortical: STDP with ACh attention modulation
 *   - Striatal: Strong DA modulation, reinforcement learning
 *   - Hippocampal: BCM for pattern separation
 *   - Cerebellar: Error-driven, supervised
 *
 * PERFORMANCE:
 * - RPE computation: O(1) per loss sample
 * - Region routing: O(log R) where R = number of regions
 * - Parallel plasticity updates via thread pool
 * - CoW for weight snapshots during updates
 *
 * THREAD SAFETY:
 * - Thread pool for parallel plasticity application
 * - Atomic neuromodulator level updates
 * - CoW for weight matrix snapshots
 * - RW locks for region routing table
 *
 * @author NIMCP Development Team
 * @version 1.0.0
 * @date 2025-11-27
 */

#ifndef NIMCP_TRAINING_PLASTICITY_BRIDGE_H
#define NIMCP_TRAINING_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>
#include "utils/validation/nimcp_common.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "middleware/training/nimcp_brain_training_integration.h"
#include "utils/memory/nimcp_cow_manager.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/thread/nimcp_thread.h"
#include "core/events/nimcp_event_bus.h"
#include "common/nimcp_export.h"
#include "security/nimcp_security_integration.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Cross-Bridge Integration
//=============================================================================

typedef struct perception_training_bridge perception_training_bridge_t;
typedef struct cortical_training_bridge cortical_training_bridge_t;

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of brain regions for plasticity routing */
#define TPB_MAX_REGIONS 32

/** Maximum number of plasticity rules per region */
#define TPB_MAX_RULES_PER_REGION 8

/** Maximum loss history for RPE computation */
#define TPB_LOSS_HISTORY_SIZE 64

/** Default thread pool size for parallel updates */
#define TPB_DEFAULT_THREAD_POOL_SIZE 4

/** Default RPE smoothing window */
#define TPB_DEFAULT_RPE_WINDOW 10

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Brain region types for plasticity routing
 *
 * WHAT: Identifiers for different brain regions with distinct plasticity rules
 * WHY:  Different regions have different learning dynamics and neuromodulator sensitivity
 */
typedef enum {
    TPB_REGION_CORTICAL = 0,      /**< Cortical: STDP + ACh attention modulation */
    TPB_REGION_STRIATAL,          /**< Striatal: Strong DA, reinforcement learning */
    TPB_REGION_HIPPOCAMPAL,       /**< Hippocampal: BCM, pattern separation */
    TPB_REGION_CEREBELLAR,        /**< Cerebellar: Error-driven supervised */
    TPB_REGION_THALAMIC,          /**< Thalamic: Gating and routing */
    TPB_REGION_AMYGDALA,          /**< Amygdala: Emotional salience, fear learning */
    TPB_REGION_PREFRONTAL,        /**< Prefrontal: Working memory, executive */
    TPB_REGION_CUSTOM,            /**< User-defined region */
    TPB_REGION_COUNT
} tpb_region_type_t;

/**
 * @brief Plasticity rule types available for routing
 */
typedef enum {
    TPB_RULE_STDP = 0,            /**< Spike-Timing Dependent Plasticity */
    TPB_RULE_BCM,                 /**< Bienenstock-Cooper-Munro */
    TPB_RULE_HOMEOSTATIC,         /**< Homeostatic scaling */
    TPB_RULE_ELIGIBILITY,         /**< Eligibility traces */
    TPB_RULE_HEBBIAN,             /**< Classic Hebbian (Oja's rule) */
    TPB_RULE_ANTI_HEBBIAN,        /**< Anti-Hebbian (decorrelation) */
    TPB_RULE_PREDICTIVE,          /**< Predictive coding */
    TPB_RULE_COUNT
} tpb_plasticity_rule_t;

/**
 * @brief RPE computation modes
 */
typedef enum {
    TPB_RPE_TEMPORAL_DIFF = 0,    /**< Temporal difference (TD) style RPE */
    TPB_RPE_EXPONENTIAL_AVG,      /**< Exponential moving average baseline */
    TPB_RPE_SLIDING_WINDOW,       /**< Sliding window average baseline */
    TPB_RPE_ADAPTIVE              /**< Adaptive baseline with variance tracking */
} tpb_rpe_mode_t;

/**
 * @brief Neuromodulator influence on learning rate
 */
typedef enum {
    TPB_NEUROMOD_DA_PRIMARY = 0,  /**< DA dominant (reward learning) */
    TPB_NEUROMOD_ACH_PRIMARY,     /**< ACh dominant (attention learning) */
    TPB_NEUROMOD_5HT_PRIMARY,     /**< 5-HT dominant (patience/inhibition) */
    TPB_NEUROMOD_NE_PRIMARY,      /**< NE dominant (arousal/vigilance) */
    TPB_NEUROMOD_BALANCED,        /**< Balanced contribution from all */
    TPB_NEUROMOD_CUSTOM           /**< Custom weighting */
} tpb_neuromod_mode_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Reward Prediction Error (RPE) state
 *
 * WHAT: Tracks loss history for computing dopamine-like reward signals
 * WHY:  Dopamine neurons encode prediction error, not absolute reward
 *
 * BIOLOGICAL BASIS:
 * - Schultz et al. (1997): DA neurons fire for unexpected rewards
 * - Positive RPE (reward > expected) → DA burst → LTP
 * - Negative RPE (reward < expected) → DA dip → LTD
 * - Zero RPE (reward == expected) → Baseline DA → no learning
 */
typedef struct {
    float loss_history[TPB_LOSS_HISTORY_SIZE];  /**< Circular buffer of losses */
    uint32_t history_index;                     /**< Current write index */
    uint32_t history_count;                     /**< Valid history entries */
    float baseline_loss;                        /**< Predicted/expected loss */
    float baseline_variance;                    /**< Loss variance for adaptive scaling */
    float last_rpe;                             /**< Most recent RPE value */
    float smoothed_rpe;                         /**< EMA-smoothed RPE */
    float rpe_alpha;                            /**< EMA smoothing factor */
    tpb_rpe_mode_t mode;                        /**< RPE computation mode */
} tpb_rpe_state_t;

/**
 * @brief Per-region plasticity configuration
 */
typedef struct {
    tpb_region_type_t type;                     /**< Region type */
    const char* name;                           /**< Human-readable name */

    /* Plasticity rules enabled for this region */
    tpb_plasticity_rule_t primary_rule;         /**< Primary plasticity rule */
    tpb_plasticity_rule_t secondary_rule;       /**< Secondary/modulating rule */
    bool enable_three_factor;                   /**< Enable three-factor learning */

    /* Neuromodulator sensitivity */
    float da_sensitivity;                       /**< Dopamine sensitivity [0,2] */
    float ach_sensitivity;                      /**< Acetylcholine sensitivity [0,2] */
    float ht5_sensitivity;                      /**< Serotonin sensitivity [0,2] */
    float ne_sensitivity;                       /**< Norepinephrine sensitivity [0,2] */

    /* Learning parameters */
    float base_learning_rate;                   /**< Base LR for this region */
    float lr_modulation_strength;               /**< How much neuromod affects LR */
    float plasticity_window_ms;                 /**< Time window for plasticity */

    /* Neuron range for this region */
    uint32_t neuron_start_idx;                  /**< First neuron index */
    uint32_t neuron_end_idx;                    /**< Last neuron index (exclusive) */

    /* Custom parameters */
    void* custom_params;                        /**< Region-specific parameters */
} tpb_region_config_t;

/**
 * @brief Neuromodulator-Learning Rate coupling configuration
 */
typedef struct {
    tpb_neuromod_mode_t mode;                   /**< Neuromodulator influence mode */

    /* Weight coefficients for balanced/custom mode */
    float da_weight;                            /**< DA contribution weight [0,1] */
    float ach_weight;                           /**< ACh contribution weight [0,1] */
    float ht5_weight;                           /**< 5-HT contribution weight [0,1] */
    float ne_weight;                            /**< NE contribution weight [0,1] */

    /* Modulation bounds */
    float min_lr_multiplier;                    /**< Minimum LR multiplier (e.g., 0.1) */
    float max_lr_multiplier;                    /**< Maximum LR multiplier (e.g., 5.0) */

    /* Nonlinearity */
    bool use_sigmoid_scaling;                   /**< Apply sigmoid to prevent extremes */
    float sigmoid_steepness;                    /**< Sigmoid curve steepness */
} tpb_lr_modulation_config_t;

/**
 * @brief Training-Plasticity Bridge configuration
 */
typedef struct {
    /* RPE configuration */
    tpb_rpe_mode_t rpe_mode;                    /**< RPE computation mode */
    uint32_t rpe_window_size;                   /**< Window size for baseline */
    float rpe_smoothing_alpha;                  /**< EMA alpha for RPE smoothing */
    float rpe_to_da_gain;                       /**< RPE → DA concentration gain */

    /* Region configuration */
    uint32_t num_regions;                       /**< Number of active regions */
    tpb_region_config_t regions[TPB_MAX_REGIONS]; /**< Per-region configs */

    /* LR modulation */
    tpb_lr_modulation_config_t lr_modulation;   /**< LR modulation config */

    /* Thread pool */
    uint32_t thread_pool_size;                  /**< Worker threads for updates */

    /* Memory management */
    bool enable_cow;                            /**< Enable CoW for weight snapshots */
    cow_manager_t cow_manager;                  /**< Shared CoW manager (NULL = create) */

    /* Neuromodulator system */
    neuromodulator_system_t neuromod_system;    /**< Shared neuromod (NULL = create) */

    /* Event bus integration */
    event_bus_t event_bus;                      /**< Shared event bus (NULL = no events) */
    bool publish_events;                        /**< Publish bridge events */

    /* Callbacks */
    void (*on_rpe_computed)(float rpe, void* user_data);
    void (*on_lr_modulated)(uint32_t region_id, float new_lr, void* user_data);
    void (*on_plasticity_applied)(uint32_t region_id, uint32_t updates, void* user_data);
    void* callback_user_data;

    /* Security integration */
    nimcp_sec_integration_t* security_ctx;        /**< Security context (optional) */

} tpb_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* RPE statistics */
    uint64_t rpe_computations;                  /**< Total RPE computations */
    float total_positive_rpe;                   /**< Cumulative positive RPE */
    float total_negative_rpe;                   /**< Cumulative negative RPE */
    float avg_rpe;                              /**< Average RPE */
    float rpe_variance;                         /**< RPE variance */

    /* Plasticity statistics */
    uint64_t total_plasticity_updates;          /**< Total weight updates */
    uint64_t stdp_updates;                      /**< STDP-driven updates */
    uint64_t bcm_updates;                       /**< BCM-driven updates */
    uint64_t homeostatic_updates;               /**< Homeostatic updates */

    /* Per-region statistics */
    uint64_t region_updates[TPB_MAX_REGIONS];   /**< Updates per region */
    float region_avg_delta[TPB_MAX_REGIONS];    /**< Avg weight change per region */

    /* Neuromodulator statistics */
    float da_bursts;                            /**< DA burst events (RPE > 0.5) */
    float da_dips;                              /**< DA dip events (RPE < -0.5) */
    float avg_lr_multiplier;                    /**< Average LR multiplier applied */

    /* Performance */
    uint64_t total_time_ns;                     /**< Total bridge processing time */
    uint64_t cow_copy_count;                    /**< CoW copy operations */
    uint64_t cow_saved_bytes;                   /**< Memory saved by CoW */

} tpb_stats_t;

/**
 * @brief Training-Plasticity Bridge context (opaque)
 */
#ifndef TPB_CONTEXT_TYPEDEF
#define TPB_CONTEXT_TYPEDEF
typedef struct tpb_context tpb_context_t;
#endif

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Minimize boilerplate, provide good starting point
 *
 * @return Default configuration structure
 */
NIMCP_EXPORT tpb_config_t tpb_config_default(void);

/**
 * @brief Get preset configuration for specific use case
 *
 * WHAT: Returns optimized configuration for common scenarios
 * WHY:  Different applications need different tuning
 *
 * @param preset_name Preset name ("reinforcement", "supervised", "unsupervised", "biological")
 * @return Configuration for preset, or default if not found
 */
NIMCP_EXPORT tpb_config_t tpb_config_preset(const char* preset_name);

/**
 * @brief Create Training-Plasticity Bridge
 *
 * WHAT: Allocates and initializes bridge context
 * WHY:  Central coordination of training-plasticity integration
 * HOW:  Creates thread pool, initializes RPE state, sets up routing
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge context or NULL on failure
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(R) where R = number of regions
 */
NIMCP_EXPORT tpb_context_t* tpb_create(const tpb_config_t* config);

/**
 * @brief Initialize bridge with brain training context
 *
 * WHAT: Connects bridge to existing brain training integration
 * WHY:  Enable loss events to trigger RPE computation
 * HOW:  Registers event handlers, connects neuromodulator system
 *
 * @param ctx Bridge context
 * @param training_ctx Brain training context
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT nimcp_result_t tpb_connect_training(
    tpb_context_t* ctx,
    nimcp_brain_training_ctx_t* training_ctx
);

/**
 * @brief Destroy Training-Plasticity Bridge
 *
 * WHAT: Releases all resources
 * WHY:  Clean shutdown, prevent memory leaks
 * HOW:  Shuts down thread pool, releases CoW handles
 *
 * @param ctx Bridge context
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT void tpb_destroy(tpb_context_t* ctx);

//=============================================================================
// Loss-Dopamine Connector (RPE)
//=============================================================================

/**
 * @brief Report loss value and compute RPE
 *
 * WHAT: Core function connecting training loss to dopamine release
 * WHY:  Implements reward prediction error for biological learning
 * HOW:  Updates loss history, computes RPE, triggers DA release
 *
 * @param ctx Bridge context
 * @param loss Current loss value
 * @param rpe_out Output: computed RPE (NULL to skip)
 * @return NIMCP_SUCCESS or error code
 *
 * BIOLOGICAL EFFECT:
 * - loss_decreased (RPE > 0) → DA burst → enhance recent activity
 * - loss_increased (RPE < 0) → DA dip → weaken recent activity
 * - loss_stable (RPE ≈ 0) → baseline → no change
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT nimcp_result_t tpb_report_loss(
    tpb_context_t* ctx,
    float loss,
    float* rpe_out
);

/**
 * @brief Manually set dopamine level (for external reward signals)
 *
 * WHAT: Allows external systems to inject reward signals
 * WHY:  Support for RL environments, human feedback
 * HOW:  Directly modulates DA, bypasses RPE computation
 *
 * @param ctx Bridge context
 * @param da_delta Change in dopamine level [-1, 1]
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT nimcp_result_t tpb_inject_reward(
    tpb_context_t* ctx,
    float da_delta
);

/**
 * @brief Get current RPE state
 *
 * @param ctx Bridge context
 * @param state_out Output: RPE state (NULL to skip)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_get_rpe_state(
    tpb_context_t* ctx,
    tpb_rpe_state_t* state_out
);

//=============================================================================
// Region-Specific Plasticity Router
//=============================================================================

/**
 * @brief Configure plasticity for a brain region
 *
 * WHAT: Sets up plasticity rules for a specific region
 * WHY:  Different regions have different learning dynamics
 * HOW:  Registers region config in routing table
 *
 * @param ctx Bridge context
 * @param region_config Region configuration
 * @param region_id_out Output: assigned region ID
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Thread-safe (write-locks routing table)
 */
NIMCP_EXPORT nimcp_result_t tpb_configure_region(
    tpb_context_t* ctx,
    const tpb_region_config_t* region_config,
    uint32_t* region_id_out
);

/**
 * @brief Route weight update to appropriate plasticity rule
 *
 * WHAT: Applies region-specific plasticity to weight updates
 * WHY:  Centralizes plasticity logic, ensures correct rule application
 * HOW:  Looks up region by neuron ID, applies configured rule
 *
 * @param ctx Bridge context
 * @param neuron_id Postsynaptic neuron ID
 * @param pre_activity Presynaptic activity
 * @param post_activity Postsynaptic activity
 * @param spike_time_delta Pre-post spike timing (ms, negative = post-before-pre)
 * @param weight_delta_out Output: weight change to apply
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Thread-safe (read-locks routing table)
 * COMPLEXITY: O(log R) where R = number of regions
 */
NIMCP_EXPORT nimcp_result_t tpb_route_weight_update(
    tpb_context_t* ctx,
    uint32_t neuron_id,
    float pre_activity,
    float post_activity,
    float spike_time_delta,
    float* weight_delta_out
);

/**
 * @brief Apply plasticity to batch of synapses (parallel)
 *
 * WHAT: High-performance batch plasticity update
 * WHY:  Leverage thread pool for parallel updates
 * HOW:  Partitions work across threads, applies region-specific rules
 *
 * @param ctx Bridge context
 * @param num_synapses Number of synapses to update
 * @param pre_neuron_ids Array of presynaptic neuron IDs
 * @param post_neuron_ids Array of postsynaptic neuron IDs
 * @param pre_activities Array of presynaptic activities
 * @param post_activities Array of postsynaptic activities
 * @param spike_deltas Array of spike timing deltas
 * @param weights Input/Output: weight array to update
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Thread-safe (uses thread pool)
 * COMPLEXITY: O(N/T) where N = synapses, T = threads
 */
NIMCP_EXPORT nimcp_result_t tpb_apply_plasticity_batch(
    tpb_context_t* ctx,
    uint32_t num_synapses,
    const uint32_t* pre_neuron_ids,
    const uint32_t* post_neuron_ids,
    const float* pre_activities,
    const float* post_activities,
    const float* spike_deltas,
    float* weights
);

//=============================================================================
// Neuromodulator-Learning Rate Modulator
//=============================================================================

/**
 * @brief Get modulated learning rate for a region
 *
 * WHAT: Computes effective learning rate based on neuromodulator levels
 * WHY:  Implements neuromodulator-gated learning
 * HOW:  Applies configured weights to current neuromod levels
 *
 * @param ctx Bridge context
 * @param region_id Region ID (from tpb_configure_region)
 * @param base_lr Base learning rate
 * @param modulated_lr_out Output: modulated learning rate
 * @return NIMCP_SUCCESS or error code
 *
 * MODULATION FORMULA (balanced mode):
 *   lr_mult = w_da * DA + w_ach * ACh + w_5ht * (1-5HT) + w_ne * NE
 *   modulated_lr = base_lr * clamp(lr_mult, min, max)
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT nimcp_result_t tpb_get_modulated_lr(
    tpb_context_t* ctx,
    uint32_t region_id,
    float base_lr,
    float* modulated_lr_out
);

/**
 * @brief Get current neuromodulator levels
 *
 * @param ctx Bridge context
 * @param da_out Output: dopamine level [0,1]
 * @param ach_out Output: acetylcholine level [0,1]
 * @param ht5_out Output: serotonin level [0,1]
 * @param ne_out Output: norepinephrine level [0,1]
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_get_neuromod_levels(
    tpb_context_t* ctx,
    float* da_out,
    float* ach_out,
    float* ht5_out,
    float* ne_out
);

/**
 * @brief Set neuromodulator levels (for testing/external control)
 *
 * @param ctx Bridge context
 * @param da Dopamine level [0,1] (negative to keep current)
 * @param ach Acetylcholine level [0,1] (negative to keep current)
 * @param ht5 Serotonin level [0,1] (negative to keep current)
 * @param ne Norepinephrine level [0,1] (negative to keep current)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_set_neuromod_levels(
    tpb_context_t* ctx,
    float da,
    float ach,
    float ht5,
    float ne
);

//=============================================================================
// STDP and BCM Integration
//=============================================================================

/**
 * @brief Create STDP synapse with bridge-managed neuromodulation
 *
 * WHAT: Factory for STDP synapses integrated with bridge
 * WHY:  Ensure consistent neuromodulation coupling
 *
 * @param ctx Bridge context
 * @param region_id Region ID for sensitivity parameters
 * @param synapse_out Output: initialized STDP synapse
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_create_stdp_synapse(
    tpb_context_t* ctx,
    uint32_t region_id,
    stdp_synapse_t* synapse_out
);

/**
 * @brief Create BCM synapse with bridge-managed parameters
 *
 * @param ctx Bridge context
 * @param region_id Region ID for parameters
 * @param synapse_out Output: initialized BCM synapse
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_create_bcm_synapse(
    tpb_context_t* ctx,
    uint32_t region_id,
    bcm_synapse_t* synapse_out
);

/**
 * @brief Update STDP synapse with current neuromodulation
 *
 * WHAT: Applies bridge's neuromod levels to STDP weight change
 * WHY:  Three-factor learning: Hebbian × Timing × Reward
 *
 * @param ctx Bridge context
 * @param synapse STDP synapse to update
 * @param pre_spike True if presynaptic spike occurred
 * @param post_spike True if postsynaptic spike occurred
 * @param current_time_ms Current simulation time
 * @param weight_delta_out Output: weight change applied
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_update_stdp(
    tpb_context_t* ctx,
    stdp_synapse_t* synapse,
    bool pre_spike,
    bool post_spike,
    float current_time_ms,
    float* weight_delta_out
);

/**
 * @brief Update BCM synapse with current neuromodulation
 *
 * @param ctx Bridge context
 * @param synapse BCM synapse to update
 * @param pre_activity Presynaptic activity
 * @param post_activity Postsynaptic activity
 * @param dt Time step (seconds)
 * @param weight_delta_out Output: weight change applied
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_update_bcm(
    tpb_context_t* ctx,
    bcm_synapse_t* synapse,
    float pre_activity,
    float post_activity,
    float dt,
    float* weight_delta_out
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param ctx Bridge context
 * @param stats_out Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_get_stats(
    tpb_context_t* ctx,
    tpb_stats_t* stats_out
);

/**
 * @brief Reset bridge statistics
 *
 * @param ctx Bridge context
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_reset_stats(tpb_context_t* ctx);

/**
 * @brief Print bridge status to stdout
 *
 * @param ctx Bridge context
 */
NIMCP_EXPORT void tpb_print_status(tpb_context_t* ctx);

//=============================================================================
// CoW Integration for Weight Snapshots
//=============================================================================

/**
 * @brief Create CoW snapshot of weights before update
 *
 * WHAT: Takes lazy copy of weights for rollback capability
 * WHY:  Enable undo on divergence, checkpoint creation
 *
 * @param ctx Bridge context
 * @param weights Weight array
 * @param num_weights Number of weights
 * @param snapshot_out Output: CoW handle
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_snapshot_weights(
    tpb_context_t* ctx,
    const float* weights,
    uint32_t num_weights,
    cow_handle_t* snapshot_out
);

/**
 * @brief Restore weights from CoW snapshot
 *
 * @param ctx Bridge context
 * @param snapshot CoW handle from tpb_snapshot_weights
 * @param weights Output: weight array to restore into
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_restore_weights(
    tpb_context_t* ctx,
    cow_handle_t snapshot,
    float* weights
);

/**
 * @brief Release CoW snapshot
 *
 * @param ctx Bridge context
 * @param snapshot CoW handle to release
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_release_snapshot(
    tpb_context_t* ctx,
    cow_handle_t snapshot
);

//=============================================================================
// Preset Region Configurations
//=============================================================================

/**
 * @brief Get preset cortical region configuration
 */
NIMCP_EXPORT tpb_region_config_t tpb_region_cortical_default(void);

/**
 * @brief Get preset striatal region configuration
 */
NIMCP_EXPORT tpb_region_config_t tpb_region_striatal_default(void);

/**
 * @brief Get preset hippocampal region configuration
 */
NIMCP_EXPORT tpb_region_config_t tpb_region_hippocampal_default(void);

/**
 * @brief Get preset cerebellar region configuration
 */
NIMCP_EXPORT tpb_region_config_t tpb_region_cerebellar_default(void);

/**
 * @brief Get preset amygdala region configuration
 */
NIMCP_EXPORT tpb_region_config_t tpb_region_amygdala_default(void);

/**
 * @brief Get preset prefrontal region configuration
 */
NIMCP_EXPORT tpb_region_config_t tpb_region_prefrontal_default(void);

//=============================================================================
// Training Callbacks Integration (Phase TCB-1)
//=============================================================================

/**
 * @brief Connect callback context to plasticity bridge
 *
 * WHAT: Links callback system to plasticity bridge for bidirectional communication
 * WHY:  Enable biological plasticity to respond to training callbacks
 * HOW:  Registers plasticity-specific callbacks, enables action handling
 *
 * @param ctx Bridge context
 * @param callback_ctx Training callback context
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_connect_callbacks(
    tpb_context_t* ctx,
    tcb_context_t* callback_ctx
);

/**
 * @brief Get connected callback context
 *
 * @param ctx Bridge context
 * @return Callback context or NULL if not connected
 */
NIMCP_EXPORT tcb_context_t* tpb_get_callback_context(const tpb_context_t* ctx);

/**
 * @brief Handle callback action for plasticity modulation
 *
 * WHAT: Responds to callback actions with biological plasticity adjustments
 * WHY:  Actions like REDUCE_LR should affect biological learning rates too
 * HOW:  Maps actions to neuromodulator adjustments and LR multipliers
 *
 * Actions handled:
 * - TCB_ACTION_REDUCE_LR: Reduce DA, increase 5-HT (slow learning)
 * - TCB_ACTION_INCREASE_LR: Increase DA, reduce 5-HT (fast learning)
 * - TCB_ACTION_ROLLBACK: Restore from CoW snapshot if available
 * - TCB_ACTION_STOP_TRAINING: Set shutdown flag
 *
 * @param ctx Bridge context
 * @param action Action to handle
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_handle_callback_action(
    tpb_context_t* ctx,
    tcb_action_t action
);

/**
 * @brief Register built-in plasticity callbacks
 *
 * WHAT: Registers callbacks that trigger plasticity updates at training events
 * WHY:  Automate biological learning rule application during training
 * HOW:  Registers callbacks for loss, weights, epoch events
 *
 * Callbacks registered:
 * - on_loss_computed: Computes RPE, updates dopamine
 * - on_weights_updated: Triggers STDP/BCM based on gradients
 * - on_epoch_complete: Consolidation, homeostatic scaling
 * - on_divergence: Emergency neuromodulator reset
 *
 * @param ctx Bridge context
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_register_plasticity_callbacks(tpb_context_t* ctx);

/**
 * @brief Callback: Triggered when loss is computed
 *
 * Computes Reward Prediction Error from loss and updates dopamine levels.
 * Loss decrease → positive RPE → DA burst → enhanced plasticity.
 *
 * @param event Callback event with loss metrics
 * @return Action (usually CONTINUE, STOP_TRAINING on divergence)
 */
NIMCP_EXPORT tcb_action_t tpb_callback_on_loss(const tcb_event_t* event);

/**
 * @brief Callback: Triggered when weights are updated
 *
 * Applies three-factor learning: modulates weight change by current
 * neuromodulator levels (DA × eligibility × Hebbian).
 *
 * @param event Callback event with gradient info
 * @return Action (usually CONTINUE)
 */
NIMCP_EXPORT tcb_action_t tpb_callback_on_weights(const tcb_event_t* event);

/**
 * @brief Callback: Triggered at epoch completion
 *
 * Triggers:
 * - Memory consolidation (transfer from short-term to long-term)
 * - Homeostatic scaling (normalize firing rates)
 * - BCM threshold updates
 *
 * @param event Callback event with epoch metrics
 * @return Action (usually CONTINUE)
 */
NIMCP_EXPORT tcb_action_t tpb_callback_on_epoch(const tcb_event_t* event);

/**
 * @brief Callback: Triggered on training divergence
 *
 * Emergency response:
 * - Reset neuromodulators to baseline
 * - Reduce all learning rates
 * - Optionally rollback weights
 *
 * @param event Callback event with divergence info
 * @return Action (ROLLBACK or STOP_TRAINING)
 */
NIMCP_EXPORT tcb_action_t tpb_callback_on_divergence(const tcb_event_t* event);

/**
 * @brief Set plasticity modulation based on callback frequency
 *
 * WHAT: Adjusts biological learning intensity based on callback patterns
 * WHY:  Frequent LR reduction callbacks indicate training instability
 * HOW:  Tracks callback history, modulates neuromodulator sensitivity
 *
 * @param ctx Bridge context
 * @param event_type Event type to track
 * @param modulation_factor Factor to apply (0.0-2.0, 1.0 = normal)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_set_callback_modulation(
    tpb_context_t* ctx,
    tcb_event_type_t event_type,
    float modulation_factor
);

/**
 * @brief Get plasticity callback statistics
 *
 * @param ctx Bridge context
 * @param loss_callbacks_fired Output: loss callback count
 * @param weight_callbacks_fired Output: weight callback count
 * @param epoch_callbacks_fired Output: epoch callback count
 * @param divergence_callbacks_fired Output: divergence callback count
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_get_callback_stats(
    const tpb_context_t* ctx,
    uint64_t* loss_callbacks_fired,
    uint64_t* weight_callbacks_fired,
    uint64_t* epoch_callbacks_fired,
    uint64_t* divergence_callbacks_fired
);

//=============================================================================
// Perception-Cortical Training Integration (Phase XBI)
//=============================================================================

/**
 * @brief Connect to perception training bridge
 *
 * WHAT: Links perception training to plasticity modulation
 * WHY:  Visual attention weights scale per-feature plasticity
 * HOW:  Stores pointer, queries effects during updates
 *
 * BIOLOGICAL BASIS: Visual attention gates plasticity in visual cortex.
 * Attended features get enhanced LTP (attention × Hebbian × reward).
 *
 * @param ctx Bridge context
 * @param perception_bridge Perception training bridge (NULL to disconnect)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_connect_perception_training(
    tpb_context_t* ctx,
    perception_training_bridge_t* perception_bridge
);

/**
 * @brief Connect to cortical training bridge
 *
 * WHAT: Links cortical training to plasticity modulation
 * WHY:  Burst rate and prediction error modulate plasticity
 * HOW:  Stores pointer, queries effects during updates
 *
 * BIOLOGICAL BASIS: Dendritic bursts signal successful prediction → enhance LTP.
 * High prediction error → focus plasticity on error regions.
 *
 * @param ctx Bridge context
 * @param cortical_bridge Cortical training bridge (NULL to disconnect)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t tpb_connect_cortical_training(
    tpb_context_t* ctx,
    cortical_training_bridge_t* cortical_bridge
);

/**
 * @brief Get perception-based plasticity modulation factor
 *
 * WHAT: Compute plasticity factor from perception state
 * WHY:  Visual confidence and attention modulate learning
 * HOW:  Uses lr_factor and visual_confidence from perception effects
 *
 * MODULATION FORMULA:
 *   factor = perception.lr_factor × (0.5 + 0.5 × perception.visual_confidence)
 *
 * @param ctx Bridge context
 * @return Perception plasticity factor [0.25-1.5]
 */
NIMCP_EXPORT float tpb_get_perception_plasticity_factor(
    const tpb_context_t* ctx
);

/**
 * @brief Get cortical-based plasticity modulation factor
 *
 * WHAT: Compute plasticity factor from cortical state
 * WHY:  Burst rate and predictions stable modulate learning
 * HOW:  Uses burst_rate and predictions_stable from cortical effects
 *
 * MODULATION FORMULA:
 *   base = 0.5 + 0.5 × burst_rate
 *   if predictions_stable: factor = base × 1.2  (consolidation boost)
 *   else: factor = base × (1.0 + 0.3 × prediction_error_mag)  (error-driven)
 *
 * @param ctx Bridge context
 * @return Cortical plasticity factor [0.25-1.5]
 */
NIMCP_EXPORT float tpb_get_cortical_plasticity_factor(
    const tpb_context_t* ctx
);

/**
 * @brief Get combined perception-cortical plasticity factor
 *
 * WHAT: Combined modulation from both perception and cortical state
 * WHY:  Unified factor for training-plasticity coordination
 * HOW:  Weighted average of perception and cortical factors
 *
 * @param ctx Bridge context
 * @return Combined plasticity factor [0.25-2.0]
 */
NIMCP_EXPORT float tpb_get_combined_plasticity_factor(
    const tpb_context_t* ctx
);

/**
 * @brief Check if perception training is connected
 *
 * @param ctx Bridge context
 * @return true if connected, false otherwise
 */
NIMCP_EXPORT bool tpb_is_perception_training_connected(
    const tpb_context_t* ctx
);

/**
 * @brief Check if cortical training is connected
 *
 * @param ctx Bridge context
 * @return true if connected, false otherwise
 */
NIMCP_EXPORT bool tpb_is_cortical_training_connected(
    const tpb_context_t* ctx
);

//=============================================================================
// Phase 5: Backprop Gating
//=============================================================================

/**
 * @brief Set backprop-active flag to suppress biological plasticity during backprop
 *
 * WHAT: Thread-safe flag to gate plasticity during gradient computation
 * WHY:  Biological plasticity (STDP, BCM, etc.) interferes with gradient-based training
 * HOW:  Uses GCC __atomic_store_n with release semantics
 *
 * @param ctx    Bridge context
 * @param active true to suppress plasticity, false to re-enable
 */
NIMCP_EXPORT void nimcp_tpb_set_backprop_active(tpb_context_t* ctx, bool active);

/**
 * @brief Check if backprop is currently active (plasticity should be suppressed)
 *
 * @param ctx Bridge context
 * @return true if backprop is active, false otherwise
 */
NIMCP_EXPORT bool nimcp_tpb_is_backprop_active(tpb_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TRAINING_PLASTICITY_BRIDGE_H
