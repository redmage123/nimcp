/**
 * @file nimcp_snn_training_integration_bridge.h
 * @brief SNN-Training Integration Bridge - Bidirectional SNN/Pipeline Connection
 *
 * WHAT: Bridge connecting SNN training subsystem to NIMCP training pipeline.
 *       Enables spike-based learning metrics to flow to training pipeline,
 *       and allows pipeline parameters to modulate SNN learning.
 *
 * WHY:  SNN training (STDP, R-STDP, eProp) operates at spike precision,
 *       while the training pipeline works at batch/step granularity.
 *       This bridge translates between these temporal scales.
 *
 * HOW:  SNN → Pipeline: Aggregates spike-based metrics (STDP updates,
 *                       eligibility traces, reward signals) into pipeline-
 *                       compatible statistics.
 *       Pipeline → SNN: Translates LR/batch modulations into SNN-specific
 *                       parameters (STDP amplitudes, reward scaling).
 *
 * BIOLOGICAL BASIS:
 * - STDP operates at millisecond spike timing
 * - Reward-modulated plasticity integrates over behavioral timescales
 * - Homeostatic mechanisms operate over hours/days
 * - This bridge models the multi-timescale integration of learning signals
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_TRAINING_INTEGRATION_BRIDGE_H
#define NIMCP_SNN_TRAINING_INTEGRATION_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct snn_training_integration_bridge snn_training_integration_bridge_t;
typedef struct snn_network_s snn_network_t;
typedef struct snn_training_ctx_s snn_training_ctx_t;
typedef struct snn_stdp_config_s snn_stdp_config_t;
typedef struct snn_rstdp_config_s snn_rstdp_config_t;
typedef struct snn_eprop_config_s snn_eprop_config_t;
typedef struct nimcp_brain_training_ctx nimcp_brain_training_ctx_t;
typedef struct training_immune_system training_immune_system_t;
typedef struct cognitive_training_bridge cognitive_training_bridge_t;
typedef struct training_plasticity_bridge training_plasticity_bridge_t;

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define SNN_TRAINING_INTEGRATION_MODULE_NAME     "snn_training_integration"
#define SNN_TRAINING_INTEGRATION_MODULE_VERSION  "1.0.0"

/** Bio-async module ID */
#define BIO_MODULE_SNN_TRAINING_INTEGRATION     0x0610

/** Default configuration values */
#define SNN_TRAINING_INTEGRATION_DEFAULT_UPDATE_INTERVAL_MS    10
#define SNN_TRAINING_INTEGRATION_DEFAULT_REWARD_SCALE          1.0f
#define SNN_TRAINING_INTEGRATION_DEFAULT_STDP_MODULATION_SCALE 1.0f
#define SNN_TRAINING_INTEGRATION_DEFAULT_ELIGIBILITY_DECAY     0.95f

/** Limits */
#define SNN_TRAINING_INTEGRATION_MAX_CONTEXTS          16
#define SNN_TRAINING_INTEGRATION_HISTORY_SIZE          1000
#define SNN_TRAINING_INTEGRATION_MAX_REWARD_SOURCES    8

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief SNN training mode for integration
 *
 * WHAT: Type of SNN learning algorithm in use
 * WHY:  Different algorithms require different integration approaches
 * HOW:  Determines how metrics are aggregated and parameters applied
 */
typedef enum {
    SNN_TRAINING_INTEGRATION_MODE_STDP = 0,    /**< Basic STDP */
    SNN_TRAINING_INTEGRATION_MODE_RSTDP,       /**< Reward-modulated STDP */
    SNN_TRAINING_INTEGRATION_MODE_EPROP,       /**< Eligibility propagation */
    SNN_TRAINING_INTEGRATION_MODE_SURROGATE,   /**< Surrogate gradient */
    SNN_TRAINING_INTEGRATION_MODE_HOMEOSTATIC, /**< Homeostatic plasticity */
    SNN_TRAINING_INTEGRATION_MODE_HYBRID,      /**< Multiple modes combined */
    SNN_TRAINING_INTEGRATION_MODE_COUNT
} snn_training_integration_mode_t;

/**
 * @brief Reward signal source type
 *
 * WHAT: Origin of reward signals for R-STDP
 * WHY:  Different sources may need different scaling/processing
 * HOW:  Each source contributes to final reward with configured weight
 */
typedef enum {
    SNN_REWARD_SOURCE_EXTERNAL = 0,  /**< External reward from environment */
    SNN_REWARD_SOURCE_LOSS,          /**< Derived from loss reduction */
    SNN_REWARD_SOURCE_CURIOSITY,     /**< Curiosity-driven intrinsic reward */
    SNN_REWARD_SOURCE_EMOTION,       /**< Emotional valence as reward */
    SNN_REWARD_SOURCE_COGNITIVE,     /**< Cognitive satisfaction */
    SNN_REWARD_SOURCE_NOVELTY,       /**< Novelty detection */
    SNN_REWARD_SOURCE_PREDICTION,    /**< Prediction error reduction */
    SNN_REWARD_SOURCE_COUNT
} snn_reward_source_t;

/**
 * @brief Spike-based learning event type
 *
 * WHAT: Categories of SNN learning events to report
 * WHY:  Different events may trigger different pipeline responses
 * HOW:  Events are aggregated and reported to connected bridges
 */
typedef enum {
    SNN_LEARNING_EVENT_LTP = 0,        /**< Long-term potentiation */
    SNN_LEARNING_EVENT_LTD,            /**< Long-term depression */
    SNN_LEARNING_EVENT_ELIGIBILITY,    /**< Eligibility trace update */
    SNN_LEARNING_EVENT_REWARD,         /**< Reward signal received */
    SNN_LEARNING_EVENT_HOMEOSTATIC,    /**< Homeostatic adjustment */
    SNN_LEARNING_EVENT_SATURATION,     /**< Weight saturation hit */
    SNN_LEARNING_EVENT_INSTABILITY,    /**< Learning instability detected */
    SNN_LEARNING_EVENT_COUNT
} snn_learning_event_t;

/**
 * @brief Integration operation mode
 *
 * WHAT: How the bridge processes and applies modulations
 * WHY:  Different use cases need different automation levels
 * HOW:  Controls whether modulations are advisory or automatic
 */
typedef enum {
    SNN_TRAINING_INTEGRATION_OP_DISABLED = 0,  /**< Bridge disabled */
    SNN_TRAINING_INTEGRATION_OP_MONITOR,       /**< Monitor only */
    SNN_TRAINING_INTEGRATION_OP_ADVISORY,      /**< Provide recommendations */
    SNN_TRAINING_INTEGRATION_OP_AUTOMATIC,     /**< Automatically apply */
    SNN_TRAINING_INTEGRATION_OP_COORDINATED    /**< Coordinate with other bridges */
} snn_training_integration_op_t;

//=============================================================================
// SNN Training Metrics (SNN → Pipeline)
//=============================================================================

/**
 * @brief Aggregated SNN training metrics
 *
 * WHAT: Spike-based learning statistics for pipeline consumption
 * WHY:  Summarizes SNN learning state at pipeline timescale
 * HOW:  Computed from STDP updates, eligibility traces, reward signals
 *
 * BIOLOGICAL ANALOGY: Like consolidating detailed synaptic changes
 * into higher-level measures of learning progress.
 */
typedef struct {
    /* === STDP Metrics === */
    uint64_t ltp_count;             /**< Number of LTP events this epoch */
    uint64_t ltd_count;             /**< Number of LTD events this epoch */
    float total_delta_w;            /**< Total weight change */
    float avg_ltp_magnitude;        /**< Average LTP magnitude */
    float avg_ltd_magnitude;        /**< Average LTD magnitude */
    float ltp_ltd_ratio;            /**< Ratio of LTP to LTD (balance indicator) */

    /* === Eligibility Metrics === */
    float eligibility_mean;         /**< Mean eligibility trace value */
    float eligibility_max;          /**< Max eligibility trace value */
    float eligibility_sparsity;     /**< Fraction of near-zero eligibility */
    float eligibility_entropy;      /**< Entropy of eligibility distribution */

    /* === Reward Metrics === */
    float reward_current;           /**< Current reward signal */
    float reward_cumulative;        /**< Cumulative reward this epoch */
    float reward_variance;          /**< Reward variance (instability indicator) */
    float td_error_mean;            /**< Mean TD error (if using R-STDP) */

    /* === Activity Metrics === */
    float firing_rate_mean;         /**< Mean firing rate across network */
    float firing_rate_variance;     /**< Variance in firing rates */
    float burst_ratio;              /**< Ratio of bursts to regular spikes */
    float synchrony;                /**< Population synchrony [0-1] */

    /* === Stability Metrics === */
    float weight_mean;              /**< Mean network weight */
    float weight_variance;          /**< Weight variance */
    float weight_saturation_ratio;  /**< Fraction of weights at bounds */
    float learning_stability;       /**< Overall stability [0-1] */

    /* === Homeostatic Metrics === */
    float rate_deviation;           /**< Deviation from target rate */
    uint32_t homeostatic_adjustments; /**< Homeostatic adjustment count */
    float intrinsic_excitability;   /**< Average excitability level */

    /* === Timing Metrics === */
    float update_time_us;           /**< Time spent on SNN training update */
    uint64_t spikes_processed;      /**< Total spikes processed this epoch */
    uint64_t synapses_updated;      /**< Synapses modified this epoch */

    /* === Metadata === */
    uint64_t epoch;                 /**< Current epoch number */
    uint64_t step;                  /**< Current step within epoch */
    uint64_t timestamp_ms;          /**< When metrics were computed */
    bool valid;                     /**< Whether metrics are current */
} snn_training_metrics_t;

//=============================================================================
// Pipeline Parameters (Pipeline → SNN)
//=============================================================================

/**
 * @brief Pipeline parameters for SNN training modulation
 *
 * WHAT: Parameters from training pipeline to modulate SNN learning
 * WHY:  Pipeline decisions should affect spike-based learning
 * HOW:  Translates LR/batch into STDP amplitudes, reward scaling
 *
 * BIOLOGICAL ANALOGY: Neuromodulatory signals (dopamine, acetylcholine)
 * that globally affect synaptic plasticity based on behavioral state.
 */
typedef struct {
    /* === Learning Rate Modulation === */
    float lr_factor;                /**< Learning rate multiplier [0.1-2.0] */
    float stdp_amplitude_scale;     /**< Scale for STDP A+/A- */
    float eligibility_decay_scale;  /**< Scale for eligibility decay */

    /* === Reward Modulation === */
    float reward_scale;             /**< External reward scaling */
    float reward_baseline;          /**< Reward baseline for variance reduction */
    float reward_source_weights[SNN_REWARD_SOURCE_COUNT]; /**< Per-source weights */

    /* === Timing Modulation === */
    float tau_plus_scale;           /**< Scale for LTP time constant */
    float tau_minus_scale;          /**< Scale for LTD time constant */
    float eligibility_tau_scale;    /**< Scale for eligibility trace decay */

    /* === Homeostatic Modulation === */
    float target_rate_scale;        /**< Scale for target firing rate */
    float homeostatic_rate_scale;   /**< Scale for homeostatic adaptation */

    /* === Stability Controls === */
    float weight_clip_scale;        /**< Scale for weight clipping bounds */
    float gradient_clip;            /**< Max gradient magnitude (surrogate) */
    bool enable_emergency_brake;    /**< Enable emergency learning stop */

    /* === Mode Flags === */
    bool pause_learning;            /**< Temporarily pause all learning */
    bool consolidation_mode;        /**< Enter consolidation (reduce new learning) */
    bool exploration_mode;          /**< Increase exploration (more stochastic) */

    /* === Metadata === */
    uint64_t last_update_ms;        /**< When parameters were updated */
    bool valid;                     /**< Whether parameters are current */
} snn_pipeline_params_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for SNN-Training Integration Bridge
 *
 * WHAT: All configurable parameters for the bridge
 * WHY:  Allow customization for different SNN/pipeline setups
 * HOW:  Passed to create function, copied internally
 */
typedef struct {
    snn_training_integration_op_t op_mode;   /**< Operation mode */
    snn_training_integration_mode_t learning_mode; /**< SNN learning mode */

    /* === Metric Aggregation === */
    uint32_t update_interval_ms;         /**< Metric aggregation interval */
    uint32_t history_length;             /**< Number of history samples */
    bool compute_entropy;                /**< Compute eligibility entropy */
    bool track_ltp_ltd_balance;          /**< Track LTP/LTD ratio */

    /* === Parameter Translation === */
    float stdp_modulation_scale;         /**< How much LR affects STDP amplitude */
    float reward_modulation_scale;       /**< How much loss affects reward */
    float tau_modulation_scale;          /**< How much batch affects time constants */

    /* === Reward Configuration === */
    bool enable_loss_reward;             /**< Derive reward from loss improvement */
    float loss_reward_sensitivity;       /**< Sensitivity of loss→reward */
    bool enable_curiosity_reward;        /**< Use curiosity as intrinsic reward */
    float curiosity_reward_weight;       /**< Weight for curiosity reward */
    bool enable_novelty_reward;          /**< Use novelty as intrinsic reward */
    float novelty_reward_weight;         /**< Weight for novelty reward */

    /* === Stability === */
    float stability_threshold;           /**< Threshold for stability detection */
    float saturation_warning_ratio;      /**< Ratio triggering saturation warning */
    bool enable_emergency_brake;         /**< Enable emergency learning stop */
    float emergency_instability_threshold; /**< Instability to trigger brake */

    /* === Integration Flags === */
    bool enable_cognitive_training;      /**< Integrate with cognitive-training bridge */
    bool enable_training_immune;         /**< Integrate with training-immune system */
    bool enable_training_plasticity;     /**< Integrate with training-plasticity bridge */
    bool enable_bio_async;               /**< Enable bio-async messaging */

    /* === Timing === */
    bool auto_update;                    /**< Automatically update on step */
    uint32_t report_interval_steps;      /**< Steps between reports */
} snn_training_integration_config_t;

//=============================================================================
// State
//=============================================================================

/**
 * @brief Bridge state snapshot
 *
 * WHAT: Current state of the SNN-Training Integration Bridge
 * WHY:  For state inspection and debugging
 * HOW:  Populated by get_state API
 */
typedef struct {
    snn_training_integration_op_t op_mode;
    snn_training_integration_mode_t learning_mode;
    uint32_t contexts_connected;
    bool learning_paused;
    bool consolidation_active;
    bool exploration_active;
    bool emergency_brake_active;
    float current_lr_factor;
    float current_reward_scale;
    float learning_stability;
    uint64_t epoch;
    uint64_t step;
    uint64_t timestamp_ms;
} snn_training_integration_state_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Statistics for SNN-Training Integration Bridge
 *
 * WHAT: Tracking of bridge activity and performance
 * WHY:  Monitoring and debugging
 * HOW:  Accumulated during operation, queryable via API
 */
typedef struct {
    /* === Learning Event Counts === */
    uint64_t total_ltp_events;
    uint64_t total_ltd_events;
    uint64_t total_eligibility_updates;
    uint64_t total_reward_events;
    uint64_t total_homeostatic_adjustments;
    uint64_t saturation_warnings;
    uint64_t instability_events;

    /* === Modulation Counts === */
    uint64_t lr_modulations;
    uint64_t reward_modulations;
    uint64_t tau_modulations;
    uint64_t emergency_brakes;
    uint64_t learning_pauses;

    /* === Aggregated Metrics === */
    float avg_ltp_ltd_ratio;
    float avg_eligibility;
    float avg_reward;
    float avg_firing_rate;
    float avg_learning_stability;

    /* === Integration Status === */
    bool brain_training_connected;
    bool cognitive_training_connected;
    bool training_immune_connected;
    bool training_plasticity_connected;
    bool bio_async_connected;
    uint32_t snn_contexts_connected;

    /* === Timing === */
    uint64_t total_update_calls;
    float avg_update_time_us;
    float max_update_time_us;
    uint64_t last_update_ms;
} snn_training_integration_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize configuration with default values
 *
 * WHAT: Populates config struct with sensible defaults
 * WHY:  Ensure all fields have valid initial values
 * HOW:  Sets conservative defaults appropriate for most SNN training
 *
 * @param config Configuration to initialize (must not be NULL)
 */
void snn_training_integration_config_default(
    snn_training_integration_config_t* config
);

/**
 * @brief Create a new SNN-Training Integration Bridge
 *
 * WHAT: Allocates and initializes bridge structure
 * WHY:  Entry point for using the bridge
 * HOW:  Allocates internal structures, initializes state
 *
 * @param config Configuration (NULL uses defaults)
 * @return Bridge handle or NULL on failure
 */
snn_training_integration_bridge_t* snn_training_integration_create(
    const snn_training_integration_config_t* config
);

/**
 * @brief Destroy an SNN-Training Integration Bridge
 *
 * WHAT: Frees all resources associated with bridge
 * WHY:  Proper cleanup
 * HOW:  Disconnects integrations, frees memory
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void snn_training_integration_destroy(
    snn_training_integration_bridge_t* bridge
);

/**
 * @brief Start the bridge
 *
 * WHAT: Activates the bridge for operation
 * WHY:  Allows deferred start after configuration
 * HOW:  Connects bio-async if enabled, initializes update timer
 *
 * @param bridge Bridge to start
 * @return 0 on success, negative on error
 */
int snn_training_integration_start(
    snn_training_integration_bridge_t* bridge
);

/**
 * @brief Stop the bridge
 *
 * WHAT: Deactivates the bridge
 * WHY:  Pause operation without destroying
 * HOW:  Disconnects bio-async, preserves state
 *
 * @param bridge Bridge to stop
 * @return 0 on success, negative on error
 */
int snn_training_integration_stop(
    snn_training_integration_bridge_t* bridge
);

//=============================================================================
// SNN Context Connection API
//=============================================================================

/**
 * @brief Connect an SNN training context
 *
 * WHAT: Links bridge to an SNN training context
 * WHY:  Enables metric collection and parameter modulation
 * HOW:  Stores reference, begins monitoring
 *
 * @param bridge Bridge to connect
 * @param ctx SNN training context
 * @param network Associated SNN network (for metric collection)
 * @param name Optional name for this context
 * @return Context ID (0+) on success, negative on error
 */
int snn_training_integration_connect_context(
    snn_training_integration_bridge_t* bridge,
    snn_training_ctx_t* ctx,
    snn_network_t* network,
    const char* name
);

/**
 * @brief Disconnect an SNN training context
 *
 * WHAT: Removes an SNN training context from bridge
 * WHY:  Dynamic context management
 * HOW:  Removes reference, stops monitoring
 *
 * @param bridge Bridge to disconnect from
 * @param context_id Context ID from connect call
 * @return 0 on success, negative on error
 */
int snn_training_integration_disconnect_context(
    snn_training_integration_bridge_t* bridge,
    int context_id
);

//=============================================================================
// Training Pipeline Connection API
//=============================================================================

/**
 * @brief Connect to brain training context
 *
 * WHAT: Links bridge to main training pipeline
 * WHY:  Enables bidirectional integration
 * HOW:  Stores reference, registers callbacks
 *
 * @param bridge Bridge to connect
 * @param training_ctx Training context (may be NULL)
 * @return 0 on success, negative on error
 */
int snn_training_integration_connect_brain_training(
    snn_training_integration_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training_ctx
);

/**
 * @brief Connect to cognitive-training bridge
 *
 * WHAT: Links bridge to cognitive-training integration
 * WHY:  Cognitive state affects SNN learning (attention→STDP)
 * HOW:  Stores reference for query
 *
 * BIOLOGICAL BASIS: Attention modulates synaptic plasticity
 * via cholinergic and noradrenergic systems.
 *
 * @param bridge Bridge to connect
 * @param cognitive_training Cognitive-training bridge (may be NULL)
 * @return 0 on success, negative on error
 */
int snn_training_integration_connect_cognitive_training(
    snn_training_integration_bridge_t* bridge,
    cognitive_training_bridge_t* cognitive_training
);

/**
 * @brief Connect to training-immune system
 *
 * WHAT: Links bridge to training-immune integration
 * WHY:  Inflammation affects SNN learning (fever→reduced plasticity)
 * HOW:  Stores reference for query
 *
 * BIOLOGICAL BASIS: Inflammatory cytokines suppress LTP
 * and impair learning during sickness.
 *
 * @param bridge Bridge to connect
 * @param training_immune Training-immune system (may be NULL)
 * @return 0 on success, negative on error
 */
int snn_training_integration_connect_training_immune(
    snn_training_integration_bridge_t* bridge,
    training_immune_system_t* training_immune
);

/**
 * @brief Connect to training-plasticity bridge
 *
 * WHAT: Links bridge to training-plasticity integration
 * WHY:  Coordinate SNN-level and pipeline-level plasticity
 * HOW:  Stores reference for bidirectional updates
 *
 * BIOLOGICAL BASIS: Multiple plasticity mechanisms (STDP, BCM,
 * homeostatic) coordinate across timescales.
 *
 * @param bridge Bridge to connect
 * @param training_plasticity Training-plasticity bridge (may be NULL)
 * @return 0 on success, negative on error
 */
int snn_training_integration_connect_training_plasticity(
    snn_training_integration_bridge_t* bridge,
    training_plasticity_bridge_t* training_plasticity
);

//=============================================================================
// SNN → Pipeline: Metrics API
//=============================================================================

/**
 * @brief Get current SNN training metrics
 *
 * WHAT: Retrieve aggregated SNN learning metrics
 * WHY:  Pipeline needs spike-level learning statistics
 * HOW:  Copies internal metrics to output
 *
 * @param bridge Bridge to query
 * @param metrics Output metrics structure
 * @return 0 on success, negative on error
 */
int snn_training_integration_get_metrics(
    const snn_training_integration_bridge_t* bridge,
    snn_training_metrics_t* metrics
);

/**
 * @brief Get LTP/LTD balance ratio
 *
 * WHAT: Ratio of potentiation to depression
 * WHY:  Indicator of net learning direction
 * HOW:  ltp_count / (ltp_count + ltd_count)
 *
 * BIOLOGICAL BASIS: Healthy learning maintains approximate
 * E/I balance; extreme ratios indicate pathology.
 *
 * @param bridge Bridge to query
 * @return LTP/LTD ratio [0-1], 0.5 = balanced
 */
float snn_training_integration_get_ltp_ltd_ratio(
    const snn_training_integration_bridge_t* bridge
);

/**
 * @brief Get learning stability indicator
 *
 * WHAT: Overall stability of SNN learning [0-1]
 * WHY:  Detects unstable learning (weight explosions, etc.)
 * HOW:  Computed from weight variance, saturation, etc.
 *
 * @param bridge Bridge to query
 * @return Stability [0-1], 1 = perfectly stable
 */
float snn_training_integration_get_learning_stability(
    const snn_training_integration_bridge_t* bridge
);

/**
 * @brief Get cumulative reward
 *
 * WHAT: Total accumulated reward this epoch
 * WHY:  Summary of learning signal strength
 * HOW:  Sum of reward signals received
 *
 * @param bridge Bridge to query
 * @return Cumulative reward
 */
float snn_training_integration_get_cumulative_reward(
    const snn_training_integration_bridge_t* bridge
);

/**
 * @brief Get mean eligibility trace value
 *
 * WHAT: Average eligibility across all traces
 * WHY:  Indicates pending plasticity potential
 * HOW:  Mean of eligibility trace tensor
 *
 * @param bridge Bridge to query
 * @return Mean eligibility [0-1]
 */
float snn_training_integration_get_eligibility_mean(
    const snn_training_integration_bridge_t* bridge
);

//=============================================================================
// Pipeline → SNN: Parameter API
//=============================================================================

/**
 * @brief Set pipeline parameters for SNN modulation
 *
 * WHAT: Apply pipeline parameters to SNN training
 * WHY:  Pipeline decisions should affect spike-level learning
 * HOW:  Stores parameters, applies on next update
 *
 * @param bridge Bridge to update
 * @param params Pipeline parameters
 * @return 0 on success, negative on error
 */
int snn_training_integration_set_params(
    snn_training_integration_bridge_t* bridge,
    const snn_pipeline_params_t* params
);

/**
 * @brief Apply learning rate modulation to SNN
 *
 * WHAT: Scale STDP amplitudes based on LR factor
 * WHY:  LR modulation should affect spike-level plasticity
 * HOW:  A+ and A- scaled by lr_factor * stdp_modulation_scale
 *
 * @param bridge Bridge to update
 * @param lr_factor Learning rate multiplier [0.1-2.0]
 * @return 0 on success, negative on error
 */
int snn_training_integration_apply_lr_modulation(
    snn_training_integration_bridge_t* bridge,
    float lr_factor
);

/**
 * @brief Set external reward signal
 *
 * WHAT: Provide reward signal for R-STDP
 * WHY:  External reward drives reinforcement learning
 * HOW:  Stored and applied to eligibility traces
 *
 * @param bridge Bridge to update
 * @param reward Reward value
 * @param source Reward source type
 * @return 0 on success, negative on error
 */
int snn_training_integration_set_reward(
    snn_training_integration_bridge_t* bridge,
    float reward,
    snn_reward_source_t source
);

/**
 * @brief Pause SNN learning
 *
 * WHAT: Temporarily stop all SNN weight updates
 * WHY:  Emergency or deliberate learning pause
 * HOW:  Sets flag that prevents STDP/eProp updates
 *
 * @param bridge Bridge to update
 * @param pause True to pause, false to resume
 * @return 0 on success, negative on error
 */
int snn_training_integration_pause_learning(
    snn_training_integration_bridge_t* bridge,
    bool pause
);

/**
 * @brief Enter consolidation mode
 *
 * WHAT: Switch to memory consolidation mode
 * WHY:  Reduce new learning, stabilize existing
 * HOW:  Reduces STDP amplitude, increases eligibility decay
 *
 * BIOLOGICAL BASIS: Sleep-like consolidation phase where
 * replay strengthens existing traces without new learning.
 *
 * @param bridge Bridge to update
 * @param enable True to enable, false to disable
 * @return 0 on success, negative on error
 */
int snn_training_integration_consolidation_mode(
    snn_training_integration_bridge_t* bridge,
    bool enable
);

/**
 * @brief Enter exploration mode
 *
 * WHAT: Increase learning stochasticity
 * WHY:  Escape local minima, explore new patterns
 * HOW:  Increases noise, widens STDP window
 *
 * @param bridge Bridge to update
 * @param enable True to enable, false to disable
 * @return 0 on success, negative on error
 */
int snn_training_integration_exploration_mode(
    snn_training_integration_bridge_t* bridge,
    bool enable
);

//=============================================================================
// Update Cycle API
//=============================================================================

/**
 * @brief Main update cycle
 *
 * WHAT: Update bridge state, metrics, and apply modulations
 * WHY:  Periodic synchronization between SNN and pipeline
 * HOW:  Collects metrics, applies parameters, reports events
 *
 * @param bridge Bridge to update
 * @param dt_ms Time since last update (milliseconds)
 * @return 0 on success, negative on error
 */
int snn_training_integration_update(
    snn_training_integration_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Report learning event
 *
 * WHAT: Report SNN learning event to pipeline
 * WHY:  Pipeline may respond to specific learning events
 * HOW:  Increments counters, may trigger callbacks
 *
 * @param bridge Bridge to update
 * @param event Learning event type
 * @param magnitude Event magnitude
 * @return 0 on success, negative on error
 */
int snn_training_integration_report_event(
    snn_training_integration_bridge_t* bridge,
    snn_learning_event_t event,
    float magnitude
);

/**
 * @brief Signal epoch boundary
 *
 * WHAT: Notify bridge of epoch completion
 * WHY:  Triggers metric aggregation and reporting
 * HOW:  Computes epoch-level statistics, resets counters
 *
 * @param bridge Bridge to update
 * @param epoch Completed epoch number
 * @return 0 on success, negative on error
 */
int snn_training_integration_epoch_complete(
    snn_training_integration_bridge_t* bridge,
    uint64_t epoch
);

//=============================================================================
// Bio-Async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY:  Enable inter-module communication
 * HOW:  Registers module, creates inbox
 *
 * @param bridge Bridge to connect
 * @return 0 on success, negative on error
 */
int snn_training_integration_connect_bio_async(
    snn_training_integration_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async system
 * WHY:  Clean shutdown
 * HOW:  Unregisters module
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, negative on error
 */
int snn_training_integration_disconnect_bio_async(
    snn_training_integration_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Determine if messaging is available
 * HOW:  Returns internal flag
 *
 * @param bridge Bridge to query
 * @return true if connected, false otherwise
 */
bool snn_training_integration_is_bio_async_connected(
    const snn_training_integration_bridge_t* bridge
);

//=============================================================================
// State and Statistics API
//=============================================================================

/**
 * @brief Get bridge state snapshot
 *
 * WHAT: Retrieve current bridge state
 * WHY:  State inspection and debugging
 * HOW:  Copies internal state to output
 *
 * @param bridge Bridge to query
 * @param state Output state structure
 * @return 0 on success, negative on error
 */
int snn_training_integration_get_state(
    const snn_training_integration_bridge_t* bridge,
    snn_training_integration_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve accumulated statistics
 * WHY:  Monitoring and debugging
 * HOW:  Copies internal stats to output
 *
 * @param bridge Bridge to query
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
int snn_training_integration_get_stats(
    const snn_training_integration_bridge_t* bridge,
    snn_training_integration_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zeros all stat counters
 *
 * @param bridge Bridge to reset
 */
void snn_training_integration_reset_stats(
    snn_training_integration_bridge_t* bridge
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Convert learning mode to string
 *
 * @param mode Learning mode to convert
 * @return String name of mode
 */
const char* snn_training_integration_mode_to_string(
    snn_training_integration_mode_t mode
);

/**
 * @brief Convert reward source to string
 *
 * @param source Reward source to convert
 * @return String name of source
 */
const char* snn_training_integration_reward_source_to_string(
    snn_reward_source_t source
);

/**
 * @brief Convert learning event to string
 *
 * @param event Learning event to convert
 * @return String name of event
 */
const char* snn_training_integration_event_to_string(
    snn_learning_event_t event
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_TRAINING_INTEGRATION_BRIDGE_H */
