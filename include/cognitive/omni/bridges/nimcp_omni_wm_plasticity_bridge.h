/**
 * @file nimcp_omni_wm_plasticity_bridge.h
 * @brief World Model Plasticity Bridge - SNN/STDP/Plasticity Integration with RSSM
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting Omnidirectional World Model (RSSM) with
 *       SNN/STDP/Plasticity systems for closed-loop learning
 * WHY:  Close the gap in existing STDP-Omni bridge that accumulates "world model deltas"
 *       but never applies them to actual RSSM parameters
 * HOW:  STDP weight changes -> RSSM encoder updates; RSSM prediction errors -> STDP modulation
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * PREDICTIVE CODING + SYNAPTIC PLASTICITY (Friston & Rao):
 * --------------------------------------------------------
 * The brain uses prediction errors at multiple levels to drive learning:
 *
 *   1. BOTTOM-UP PREDICTION ERRORS:
 *      - Sensory mismatches propagate upward
 *      - Drive STDP in feedforward connections
 *      - Update world model encoder weights
 *
 *   2. TOP-DOWN PREDICTION GENERATION:
 *      - World model predicts expected activity
 *      - Guides SNN spike pattern generation
 *      - Shapes expectation-driven plasticity
 *
 * CLOSED-LOOP LEARNING:
 * ---------------------
 * This bridge creates a complete learning loop:
 *
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │                 PLASTICITY-WM LEARNING LOOP                  │
 *   │                                                              │
 *   │   SNN Spikes → STDP Events → Weight Changes → RSSM Update   │
 *   │        ↑                                           ↓         │
 *   │   Predicted Patterns ←──── Prediction ←──── World Model     │
 *   │                                                              │
 *   │   Prediction Error ─────→ STDP Modulation ─────→ A+/A-/τ   │
 *   └─────────────────────────────────────────────────────────────┘
 *
 * DATA FLOW:
 * ----------
 *   STDP → WM: Weight change events → update RSSM encoder weights
 *   WM → STDP: Prediction error magnitude → modulate A+/A- STDP parameters
 *   WM → STDP: Prediction error direction → adjust STDP timing windows (τ+/τ-)
 *   SNN → WM:  Spike sequences → temporal training data for RSSM
 *   WM → SNN:  Predicted activation patterns → guide spike generation
 *   BCM → WM:  Threshold sliding → inform world model confidence calibration
 *   Eligibility → WM: Three-factor signals → targeted weight updates in RSSM
 *   STP → WM:  Facilitation/depression states → modulate prediction dynamics
 *
 * INTEGRATION POINTS:
 * -------------------
 *   - STDP (nimcp_stdp.h, nimcp_stdp_omni_bridge.h): Spike-timing plasticity
 *   - BCM (nimcp_bcm.h): Bienenstock-Cooper-Munro sliding threshold
 *   - Eligibility (nimcp_eligibility_trace.h): Three-factor learning
 *   - STP (nimcp_stp.h): Short-term facilitation/depression
 *   - SNN (nimcp_neuralnet.h): Spiking neural network layer
 *   - Plasticity Coordinator (nimcp_plasticity_coordinator.h): Unified management
 *   - World Model (nimcp_omni_world_model.h): RSSM predictions
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E6D
 *   Message Range: 0x6D00-0x6DFF
 */

#ifndef NIMCP_OMNI_WM_PLASTICITY_BRIDGE_H
#define NIMCP_OMNI_WM_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_messages.h"  /* BIO_MSG_WM_PLASTICITY_* message types */
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================
 * Types used by this bridge that may be defined elsewhere. We use struct
 * pointers where possible to avoid conflicts with existing type definitions.
 */

/* World Model (from nimcp_omni_world_model.h) */
typedef struct omni_world_model omni_world_model_t;

/* STDP Omni Bridge (from nimcp_stdp_omni_bridge.h) */
typedef struct stdp_omni_bridge_struct* stdp_omni_bridge_t;

/* Forward declare structs - actual types from nimcp_stdp.h, nimcp_bcm.h, nimcp_stp.h
 * will be used when those headers are included. These are for API signatures only. */
struct stdp_synapse;
struct bcm_synapse;
struct eligibility_trace_t;
struct stp_state;

/* SNN (from nimcp_neuralnet.h) */
typedef struct neural_network_struct* neural_network_t;

/* Plasticity Coordinator (from nimcp_plasticity_coordinator.h) */
typedef struct plasticity_coordinator plasticity_coordinator_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model Plasticity Bridge */
#define BIO_MODULE_WM_PLASTICITY_BRIDGE         0x0E6D

/** Maximum STDP events to buffer per update cycle */
#define WM_PLASTICITY_MAX_STDP_EVENTS           256

/** Maximum spike sequence length for training */
#define WM_PLASTICITY_MAX_SPIKE_SEQ_LENGTH      128

/** Maximum neurons in SNN prediction target */
#define WM_PLASTICITY_MAX_SNN_NEURONS           1024

/** Default PE threshold for STDP modulation */
#define WM_PLASTICITY_DEFAULT_PE_THRESHOLD      0.1f

/** Default learning rate for RSSM encoder updates */
#define WM_PLASTICITY_DEFAULT_ENCODER_LR        0.001f

/** Default A+/A- modulation strength */
#define WM_PLASTICITY_DEFAULT_STDP_MOD_STRENGTH 0.5f

/** Maximum eligibility traces to process per update */
#define WM_PLASTICITY_MAX_ELIGIBILITY_TRACES    512

/* ============================================================================
 * Bio-Async Message Types (0x6D00-0x6DFF)
 * ============================================================================
 * Message types are defined in nimcp_bio_messages.h to avoid duplication.
 * Key message types used by this bridge:
 *   - BIO_MSG_WM_PLASTICITY_STDP_EVENT (0x6D00): STDP event notification
 *   - BIO_MSG_WM_PLASTICITY_STDP_MOD (0x6D01): STDP modulation params
 *   - BIO_MSG_WM_PLASTICITY_SPIKE_SEQ (0x6D10): Spike sequence for training
 *   - BIO_MSG_WM_PLASTICITY_SNN_PRED (0x6D11): Predicted SNN activity
 *   - BIO_MSG_WM_PLASTICITY_BCM_THRESHOLD (0x6D20): BCM threshold update
 *   - BIO_MSG_WM_PLASTICITY_ELIGIBILITY: Eligibility trace signal
 *   - BIO_MSG_WM_PLASTICITY_STATE: Plasticity state query
 *   - BIO_MSG_WM_PLASTICITY_PE_FEEDBACK: PE -> plasticity feedback
 *   - BIO_MSG_WM_PLASTICITY_COORD_SYNC: Coordinator sync
 * ============================================================================ */

/** @brief Message type alias for Plasticity bridge (uses bio_message_type_t from nimcp_bio_messages.h) */
typedef bio_message_type_t omni_wm_plasticity_msg_type_t;

/* ============================================================================
 * Data Transfer Structures
 * ============================================================================ */

/**
 * @brief STDP event for world model training
 *
 * WHAT: Single STDP weight change event to be applied to RSSM encoder
 * WHY:  Synaptic plasticity should refine world model representations
 * HOW:  Capture pre/post timing, weight change, and synapse identity
 */
typedef struct {
    uint32_t pre_neuron_id;               /**< Presynaptic neuron ID */
    uint32_t post_neuron_id;              /**< Postsynaptic neuron ID */
    float weight_change;                  /**< Delta weight from STDP */
    float pre_post_dt_ms;                 /**< Timing difference (post - pre) in ms */
    bool is_ltp;                          /**< True if LTP, false if LTD */
    uint64_t timestamp_us;                /**< Event timestamp in microseconds */
} wm_stdp_event_t;

/**
 * @brief SNN spike sequence for WM training
 *
 * WHAT: Temporal sequence of spikes from SNN layer
 * WHY:  Train RSSM dynamics on actual neural firing patterns
 * HOW:  Record neuron IDs and spike times over a sequence window
 */
typedef struct {
    uint32_t neuron_count;                /**< Number of neurons in sequence */
    uint32_t* neuron_ids;                 /**< Array of neuron IDs */
    float* spike_times_ms;                /**< Spike times relative to sequence start */
    uint32_t spike_count;                 /**< Total number of spikes */
    float sequence_duration_ms;           /**< Total duration of sequence */
    uint64_t start_timestamp_us;          /**< Sequence start time */
    bool is_spontaneous;                  /**< Spontaneous vs evoked activity */
} wm_spike_sequence_t;

/**
 * @brief WM prediction error feedback to plasticity
 *
 * WHAT: Prediction error signals from RSSM to modulate STDP parameters
 * WHY:  High PE indicates model needs updating via enhanced plasticity
 * HOW:  PE magnitude/direction translate to A+/A-/tau adjustments
 *
 * BIOLOGICAL: Dopamine/noradrenaline modulate STDP based on surprise
 */
typedef struct {
    float forward_pe;                     /**< Forward prediction error magnitude */
    float backward_pe;                    /**< Backward inference error magnitude */
    float lateral_pe;                     /**< Cross-modal prediction error */
    float combined_pe;                    /**< Weighted combination of all PEs */
    float precision;                      /**< Confidence in PE estimate [0-1] */
    float a_plus_modulation;              /**< Suggested A+ scaling factor */
    float a_minus_modulation;             /**< Suggested A- scaling factor */
    float tau_plus_adjustment_ms;         /**< Timing window τ+ adjustment */
    float tau_minus_adjustment_ms;        /**< Timing window τ- adjustment */
    uint64_t timestamp_us;                /**< When modulation was computed */
} wm_to_plasticity_modulation_t;

/**
 * @brief Plasticity state for WM awareness
 *
 * WHAT: Current state of all plasticity mechanisms for WM integration
 * WHY:  WM needs plasticity state to calibrate predictions and training
 * HOW:  Aggregate state from STDP, BCM, eligibility, STP
 */
typedef struct {
    /* STDP state */
    float current_stdp_rate;              /**< Current effective learning rate */
    float avg_a_plus;                     /**< Average A+ across synapses */
    float avg_a_minus;                    /**< Average A- across synapses */
    float avg_tau_plus_ms;                /**< Average τ+ across synapses */
    float avg_tau_minus_ms;               /**< Average τ- across synapses */

    /* BCM state */
    float bcm_threshold;                  /**< BCM sliding threshold */
    float bcm_avg_activity;               /**< BCM average activity level */

    /* Eligibility state */
    float avg_eligibility;                /**< Average eligibility trace value */
    uint32_t eligible_synapse_count;      /**< Synapses with significant traces */

    /* STP state */
    float stp_facilitation;               /**< Short-term facilitation level */
    float stp_depression;                 /**< Short-term depression level */
    float stp_avg_utilization;            /**< Average STP utilization */

    /* Aggregate statistics */
    uint64_t recent_ltp_count;            /**< Recent LTP events */
    uint64_t recent_ltd_count;            /**< Recent LTD events */
    float synaptic_stability;             /**< Overall stability metric [0-1] */
    float learning_momentum;              /**< Current learning momentum */

    /* Timestamp */
    uint64_t timestamp_us;                /**< State snapshot time */
} plasticity_to_wm_state_t;

/**
 * @brief WM -> SNN prediction guidance
 *
 * WHAT: Predicted neural activity patterns for guiding SNN firing
 * WHY:  Top-down predictions shape expectation-driven activity
 * HOW:  Provide target firing rates for next time window
 */
typedef struct {
    uint32_t target_neuron_count;         /**< Number of neurons targeted */
    uint32_t* target_neuron_ids;          /**< Array of target neuron IDs */
    float* predicted_activations;         /**< Expected firing rates */
    float* prediction_uncertainty;        /**< Uncertainty per neuron */
    float prediction_confidence;          /**< Overall confidence [0-1] */
    uint32_t prediction_horizon_ms;       /**< Prediction time window */
    uint64_t timestamp_us;                /**< When prediction was made */
} wm_to_snn_prediction_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief World Model Plasticity Bridge configuration
 *
 * WHAT: Parameters controlling WM-Plasticity integration
 * WHY:  Tune bidirectional learning dynamics
 * HOW:  Configurable learning rates, modulation strengths, and thresholds
 */
typedef struct {
    /* General Settings */
    bool enable_modulation;               /**< Enable bidirectional modulation */
    float sensitivity;                    /**< General sensitivity [0.5-2.0] */

    /* STDP -> WM Settings (Weight changes to RSSM encoder) */
    bool enable_stdp_to_wm;               /**< Apply STDP changes to RSSM */
    float encoder_learning_rate;          /**< Learning rate for RSSM encoder updates */
    float weight_change_threshold;        /**< Min weight change to propagate */
    uint32_t stdp_event_batch_size;       /**< Batch size for STDP event processing */
    bool accumulate_before_apply;         /**< Accumulate weight changes vs immediate */

    /* WM -> STDP Settings (PE modulation of STDP parameters) */
    bool enable_wm_to_stdp;               /**< Modulate STDP based on PE */
    float pe_modulation_strength;         /**< Strength of PE -> STDP modulation */
    float pe_threshold_low;               /**< PE below this: reduce learning */
    float pe_threshold_high;              /**< PE above this: increase learning */
    float a_plus_modulation_range;        /**< Max A+ modulation [0.5-2.0] */
    float a_minus_modulation_range;       /**< Max A- modulation [0.5-2.0] */
    float tau_adjustment_max_ms;          /**< Max timing window adjustment */

    /* SNN -> WM Settings (Spike sequences for RSSM training) */
    bool enable_spike_training;           /**< Train WM from spike sequences */
    float spike_sequence_learning_rate;   /**< Learning rate for spike training */
    uint32_t min_sequence_length;         /**< Minimum spikes for valid sequence */
    float sequence_window_ms;             /**< Window for collecting sequences */

    /* WM -> SNN Settings (Prediction guidance for spike generation) */
    bool enable_prediction_guidance;      /**< Guide SNN with WM predictions */
    float guidance_strength;              /**< How strongly to bias SNN [0-1] */
    uint32_t prediction_horizon_ms;       /**< How far ahead to predict */

    /* BCM -> WM Settings */
    bool enable_bcm_integration;          /**< Integrate BCM threshold info */
    float bcm_confidence_weight;          /**< Weight for BCM -> WM confidence */

    /* Eligibility -> WM Settings */
    bool enable_eligibility_integration;  /**< Use eligibility for targeted updates */
    float eligibility_threshold;          /**< Min trace value for update */
    float reward_modulation_strength;     /**< Strength of reward signal */

    /* STP -> WM Settings */
    bool enable_stp_integration;          /**< Modulate predictions by STP state */
    float stp_dynamics_weight;            /**< Weight of STP on prediction dynamics */

    /* Coordinator Integration */
    bool enable_coordinator_sync;         /**< Sync with plasticity coordinator */

    /* Bio-async Settings */
    bool enable_bio_async;                /**< Enable bio-async messaging */
} omni_wm_plasticity_bridge_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief Effects from Plasticity Systems to World Model
 *
 * WHAT: Aggregated plasticity signals flowing to RSSM
 * WHY:  Inform WM training and predictions based on synaptic state
 * HOW:  Collect from STDP, BCM, eligibility, STP
 */
typedef struct {
    /* STDP effects */
    uint32_t pending_stdp_events;         /**< Number of buffered STDP events */
    float total_weight_delta;             /**< Net weight change to apply */
    float ltp_ltd_ratio;                  /**< Ratio of LTP to LTD events */

    /* BCM effects */
    float bcm_threshold_signal;           /**< Current BCM threshold */
    float bcm_confidence_contribution;    /**< BCM contribution to WM confidence */

    /* Eligibility effects */
    float eligibility_modulation;         /**< Eligibility-weighted update signal */
    uint32_t eligible_updates_pending;    /**< Pending eligibility-gated updates */

    /* STP effects */
    float stp_facilitation_factor;        /**< Current facilitation level */
    float stp_depression_factor;          /**< Current depression level */
    float stp_effective_gain;             /**< Combined STP gain */

    /* Spike sequence training data */
    bool spike_sequence_available;        /**< New sequence ready for training */
    uint32_t spike_sequence_count;        /**< Number of sequences buffered */

    /* Aggregate signals */
    float plasticity_activity_level;      /**< Overall plasticity activity [0-1] */
    float learning_signal_strength;       /**< Combined learning signal */
} plasticity_to_omni_wm_effects_t;

/**
 * @brief Effects from World Model to Plasticity Systems
 *
 * WHAT: WM signals flowing to plasticity mechanisms
 * WHY:  Modulate learning based on prediction accuracy
 * HOW:  PE-driven modulation of STDP, guidance for SNN
 */
typedef struct {
    /* Prediction error signals */
    float forward_pe;                     /**< Forward prediction error */
    float backward_pe;                    /**< Backward inference error */
    float lateral_pe;                     /**< Cross-modal error */
    float combined_pe;                    /**< Weighted combined PE */

    /* STDP modulation */
    wm_to_plasticity_modulation_t stdp_modulation; /**< STDP parameter adjustments */
    bool modulation_pending;              /**< New modulation ready */

    /* SNN prediction guidance */
    wm_to_snn_prediction_t* snn_prediction; /**< Predicted activity (NULL if none) */
    bool prediction_available;            /**< New prediction ready */

    /* Confidence calibration */
    float wm_confidence;                  /**< Current WM confidence level */
    float wm_uncertainty;                 /**< Current WM uncertainty */

    /* Training signals */
    float training_priority;              /**< Priority for plasticity updates */
    bool consolidation_mode;              /**< Currently in memory consolidation */
} omni_wm_to_plasticity_effects_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief World Model Plasticity Bridge statistics
 *
 * WHAT: Metrics for monitoring bridge operation and learning
 * WHY:  Performance tracking, debugging, and optimization
 * HOW:  Counters, averages, and timing metrics
 */
typedef struct {
    /* STDP -> WM Statistics */
    uint64_t stdp_events_received;        /**< Total STDP events received */
    uint64_t stdp_events_applied;         /**< STDP events applied to RSSM */
    float total_encoder_weight_delta;     /**< Cumulative RSSM weight change */
    float mean_weight_change;             /**< Average weight change per event */

    /* WM -> STDP Statistics */
    uint64_t pe_modulations_sent;         /**< PE modulation signals sent */
    float mean_pe_magnitude;              /**< Average PE magnitude */
    float mean_a_plus_modulation;         /**< Average A+ modulation applied */
    float mean_a_minus_modulation;        /**< Average A- modulation applied */

    /* Spike Training Statistics */
    uint64_t spike_sequences_received;    /**< Spike sequences for training */
    uint64_t spike_training_updates;      /**< Training updates from spikes */
    float mean_sequence_length;           /**< Average sequence length */
    float spike_training_loss;            /**< Current spike training loss */

    /* SNN Guidance Statistics */
    uint64_t predictions_generated;       /**< SNN predictions generated */
    uint64_t predictions_applied;         /**< Predictions used by SNN */
    float mean_prediction_accuracy;       /**< How accurate were predictions */

    /* BCM Integration Statistics */
    uint64_t bcm_threshold_updates;       /**< BCM threshold updates received */
    float bcm_threshold_range;            /**< Range of BCM thresholds seen */

    /* Eligibility Statistics */
    uint64_t eligibility_updates;         /**< Eligibility-gated updates */
    float mean_eligibility_strength;      /**< Average eligibility trace */

    /* STP Statistics */
    uint64_t stp_state_updates;           /**< STP state updates received */
    float mean_facilitation;              /**< Average facilitation level */
    float mean_depression;                /**< Average depression level */

    /* Timing Statistics */
    uint64_t total_updates;               /**< Total update cycles */
    double total_processing_time_ms;      /**< Total processing time */
    double mean_update_time_ms;           /**< Average update duration */
    uint64_t last_update_time_us;         /**< Last update timestamp */

    /* Error Statistics */
    uint64_t errors_total;                /**< Total errors encountered */
    uint64_t errors_stdp;                 /**< STDP-related errors */
    uint64_t errors_training;             /**< Training-related errors */
    uint64_t errors_prediction;           /**< Prediction-related errors */
} omni_wm_plasticity_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief World Model Plasticity Bridge
 *
 * WHAT: Main bridge structure connecting WM with plasticity systems
 * WHY:  Orchestrates bidirectional learning flow
 * HOW:  Maintains connections, buffers, effects, and state
 *
 * Memory Layout:
 *   bridge_base_t base MUST be first for pointer casting compatibility
 */
typedef struct omni_wm_plasticity_bridge {
    bridge_base_t base;                   /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    omni_wm_plasticity_bridge_config_t config; /**< Bridge configuration */

    /* Connected Systems */
    omni_world_model_t* world_model;      /**< World model (RSSM) */
    stdp_omni_bridge_t stdp_bridge;       /**< Existing STDP-Omni bridge to extend */
    plasticity_coordinator_t* coordinator; /**< Plasticity coordinator */
    neural_network_t snn;                 /**< Spiking neural network layer */

    /* Bidirectional Effects */
    plasticity_to_omni_wm_effects_t plasticity_to_wm; /**< Effects: Plasticity -> WM */
    omni_wm_to_plasticity_effects_t wm_to_plasticity; /**< Effects: WM -> Plasticity */

    /* Current State */
    plasticity_to_wm_state_t current_plasticity_state; /**< Snapshot of plasticity state */
    wm_to_plasticity_modulation_t current_modulation;  /**< Current STDP modulation */

    /* STDP Event Buffer */
    wm_stdp_event_t* stdp_event_buffer;   /**< Buffer for incoming STDP events */
    uint32_t stdp_event_count;            /**< Current buffer occupancy */
    uint32_t stdp_event_capacity;         /**< Buffer capacity */

    /* Spike Sequence Buffer */
    wm_spike_sequence_t* spike_seq_buffer; /**< Buffer for spike sequences */
    uint32_t spike_seq_count;             /**< Current sequence count */
    uint32_t spike_seq_capacity;          /**< Sequence buffer capacity */

    /* SNN Prediction Buffer */
    wm_to_snn_prediction_t* snn_prediction; /**< Current SNN prediction */
    bool snn_prediction_valid;            /**< Is current prediction valid */

    /* Accumulated Weight Deltas */
    float* accumulated_encoder_deltas;    /**< Accumulated RSSM encoder changes */
    uint32_t encoder_delta_dim;           /**< Dimension of encoder deltas */
    float accumulated_delta_norm;         /**< Norm of accumulated deltas */

    /* Statistics */
    omni_wm_plasticity_bridge_stats_t stats; /**< Bridge statistics */

    /* Thread safety mutex */
    nimcp_mutex_t* mutex;                 /**< Thread-safe operations */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;   /**< Per-instance health agent */
} omni_wm_plasticity_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Convenient initialization with biologically-plausible values
 * HOW:  Sets all config fields to defaults
 *
 * @param config Configuration structure to initialize
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_default_config(
    omni_wm_plasticity_bridge_config_t* config);

/**
 * @brief Create World Model Plasticity Bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Required before connecting systems
 * HOW:  Allocate structure, initialize base, set config, create buffers
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
omni_wm_plasticity_bridge_t* omni_wm_plasticity_bridge_create(
    const omni_wm_plasticity_bridge_config_t* config);

/**
 * @brief Destroy World Model Plasticity Bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource management
 * HOW:  Disconnect systems, free buffers, cleanup base
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void omni_wm_plasticity_bridge_destroy(omni_wm_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset effects, buffers, and statistics, keep connections
 * WHY:  Allow fresh start without reconnection
 * HOW:  Zero effects, clear buffers, reset stats, preserve config
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_reset(omni_wm_plasticity_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect all systems to bridge
 *
 * WHAT: Establish connections to WM, STDP bridge, coordinator, and SNN
 * WHY:  Single call to wire up all systems
 * HOW:  Store pointers, validate connections, activate bridge
 *
 * @param bridge Bridge instance
 * @param world_model World model (RSSM) - required
 * @param stdp_bridge Existing STDP-Omni bridge - optional (wraps/extends it)
 * @param coordinator Plasticity coordinator - optional
 * @param snn Spiking neural network - optional
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_connect(
    omni_wm_plasticity_bridge_t* bridge,
    omni_world_model_t* world_model,
    stdp_omni_bridge_t stdp_bridge,
    plasticity_coordinator_t* coordinator,
    neural_network_t snn);

/**
 * @brief Connect world model
 *
 * @param bridge Bridge instance
 * @param world_model World model to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_connect_world_model(
    omni_wm_plasticity_bridge_t* bridge,
    omni_world_model_t* world_model);

/**
 * @brief Connect STDP-Omni bridge (to extend it)
 *
 * @param bridge Bridge instance
 * @param stdp_bridge Existing STDP-Omni bridge
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_connect_stdp_bridge(
    omni_wm_plasticity_bridge_t* bridge,
    stdp_omni_bridge_t stdp_bridge);

/**
 * @brief Connect plasticity coordinator
 *
 * @param bridge Bridge instance
 * @param coordinator Plasticity coordinator
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_connect_coordinator(
    omni_wm_plasticity_bridge_t* bridge,
    plasticity_coordinator_t* coordinator);

/**
 * @brief Connect spiking neural network
 *
 * @param bridge Bridge instance
 * @param snn Spiking neural network
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_connect_snn(
    omni_wm_plasticity_bridge_t* bridge,
    neural_network_t snn);

/**
 * @brief Check if bridge is minimally connected
 *
 * @param bridge Bridge to check
 * @return true if world model connected (minimum requirement)
 */
bool omni_wm_plasticity_bridge_is_connected(const omni_wm_plasticity_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main update cycle
 *
 * WHAT: Process bidirectional information flow
 * WHY:  Called each timestep to sync WM and plasticity systems
 * HOW:  Process STDP events, update WM, compute PE modulation, guide SNN
 *
 * @param bridge Bridge instance
 * @param dt Time delta in seconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_update(
    omni_wm_plasticity_bridge_t* bridge,
    float dt);

/* ============================================================================
 * STDP -> WM API (Weight changes to RSSM encoder)
 * ============================================================================ */

/**
 * @brief Notify bridge of STDP weight change event
 *
 * WHAT: Report STDP plasticity event for RSSM encoder update
 * WHY:  Synaptic changes should refine world model representations
 * HOW:  Buffer event, optionally apply immediately or batch
 *
 * @param bridge Bridge instance
 * @param event STDP event details
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_on_stdp_event(
    omni_wm_plasticity_bridge_t* bridge,
    const wm_stdp_event_t* event);

/**
 * @brief Process buffered STDP events and apply to RSSM
 *
 * WHAT: Apply accumulated weight changes to RSSM encoder
 * WHY:  Batch processing is more efficient for large event counts
 * HOW:  Aggregate changes, apply gradient update to encoder weights
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_apply_stdp_to_rssm(
    omni_wm_plasticity_bridge_t* bridge);

/* ============================================================================
 * WM -> STDP API (PE modulation of STDP parameters)
 * ============================================================================ */

/**
 * @brief Get current STDP modulation based on prediction error
 *
 * WHAT: Compute STDP parameter adjustments from WM prediction error
 * WHY:  High PE indicates model needs more plasticity, low PE less
 * HOW:  Map PE magnitude/direction to A+/A-/tau modifications
 *
 * @param bridge Bridge instance
 * @param out_modulation Output: modulation parameters
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_get_stdp_modulation(
    omni_wm_plasticity_bridge_t* bridge,
    wm_to_plasticity_modulation_t* out_modulation);

/**
 * @brief Set prediction error values for STDP modulation
 *
 * WHAT: Update the PE values used for modulation computation
 * WHY:  Allow external PE sources (e.g., from actual inference)
 * HOW:  Store PE values, trigger modulation recomputation
 *
 * @param bridge Bridge instance
 * @param forward_pe Forward prediction error
 * @param backward_pe Backward inference error
 * @param lateral_pe Cross-modal error
 * @param precision Confidence in PE estimates
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_set_prediction_error(
    omni_wm_plasticity_bridge_t* bridge,
    float forward_pe,
    float backward_pe,
    float lateral_pe,
    float precision);

/* ============================================================================
 * SNN -> WM API (Spike sequences for training)
 * ============================================================================ */

/**
 * @brief Submit spike sequence for world model training
 *
 * WHAT: Provide temporal spike pattern from SNN for RSSM training
 * WHY:  Train world model on actual neural dynamics
 * HOW:  Buffer sequence, use for RSSM dynamics learning
 *
 * @param bridge Bridge instance
 * @param sequence Spike sequence data
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_train_from_spikes(
    omni_wm_plasticity_bridge_t* bridge,
    const wm_spike_sequence_t* sequence);

/* ============================================================================
 * WM -> SNN API (Prediction guidance for spike generation)
 * ============================================================================ */

/**
 * @brief Get predicted SNN activity pattern
 *
 * WHAT: Get WM prediction of expected SNN firing rates
 * WHY:  Guide SNN activity based on top-down expectations
 * HOW:  Use RSSM forward dynamics to predict neural activity
 *
 * @param bridge Bridge instance
 * @param horizon_ms Prediction horizon in milliseconds
 * @param out_prediction Output: predicted activity (caller manages memory)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_predict_snn_activity(
    omni_wm_plasticity_bridge_t* bridge,
    uint32_t horizon_ms,
    wm_to_snn_prediction_t* out_prediction);

/* ============================================================================
 * BCM Integration API
 * ============================================================================ */

/**
 * @brief Notify bridge of BCM threshold shift
 *
 * WHAT: Report BCM sliding threshold change
 * WHY:  BCM threshold informs WM confidence calibration
 * HOW:  Store threshold, update confidence contribution
 *
 * @param bridge Bridge instance
 * @param new_threshold New BCM threshold value
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_on_bcm_threshold_shift(
    omni_wm_plasticity_bridge_t* bridge,
    float new_threshold);

/* ============================================================================
 * Eligibility Trace Integration API
 * ============================================================================ */

/**
 * @brief Apply eligibility-gated learning to RSSM
 *
 * WHAT: Use eligibility traces for targeted RSSM weight updates
 * WHY:  Three-factor learning with reward enables credit assignment
 * HOW:  Weight RSSM updates by eligibility trace * reward signal
 *
 * @param bridge Bridge instance
 * @param eligibility_traces Array of eligibility trace values
 * @param trace_count Number of traces
 * @param reward_signal Reward signal for modulation [-1, 1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_apply_eligibility(
    omni_wm_plasticity_bridge_t* bridge,
    const float* eligibility_traces,
    uint32_t trace_count,
    float reward_signal);

/* ============================================================================
 * STP Integration API
 * ============================================================================ */

/**
 * @brief Update STP state for WM dynamics modulation
 *
 * WHAT: Report current STP facilitation/depression state
 * WHY:  STP affects effective synaptic strength, should modulate predictions
 * HOW:  Store STP state, factor into prediction dynamics
 *
 * @param bridge Bridge instance
 * @param facilitation Current facilitation level
 * @param depression Current depression level
 * @param utilization Average STP utilization
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_update_stp_state(
    omni_wm_plasticity_bridge_t* bridge,
    float facilitation,
    float depression,
    float utilization);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current plasticity state for WM
 *
 * @param bridge Bridge instance
 * @param out_state Output: plasticity state snapshot
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_get_plasticity_state(
    omni_wm_plasticity_bridge_t* bridge,
    plasticity_to_wm_state_t* out_state);

/**
 * @brief Get current effects from plasticity to WM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const plasticity_to_omni_wm_effects_t* omni_wm_plasticity_bridge_get_plasticity_effects(
    const omni_wm_plasticity_bridge_t* bridge);

/**
 * @brief Get current effects from WM to plasticity
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const omni_wm_to_plasticity_effects_t* omni_wm_plasticity_bridge_get_wm_effects(
    const omni_wm_plasticity_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_get_stats(
    const omni_wm_plasticity_bridge_t* bridge,
    omni_wm_plasticity_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_reset_stats(
    omni_wm_plasticity_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_connect_bio_async(
    omni_wm_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_plasticity_bridge_disconnect_bio_async(
    omni_wm_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool omni_wm_plasticity_bridge_is_bio_async_connected(
    const omni_wm_plasticity_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return Human-readable message name
 */
const char* omni_wm_plasticity_msg_type_to_string(omni_wm_plasticity_msg_type_t msg_type);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code describing issue
 */
nimcp_error_t omni_wm_plasticity_bridge_validate_config(
    const omni_wm_plasticity_bridge_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_PLASTICITY_BRIDGE_H */
