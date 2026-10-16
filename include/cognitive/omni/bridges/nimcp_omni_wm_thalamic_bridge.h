/**
 * @file nimcp_omni_wm_thalamic_bridge.h
 * @brief World Model Thalamic Bridge - Integration with Thalamic Nuclei Systems
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with thalamic nuclei
 * WHY:  Enable attention-gated prediction and sensory filtering for world modeling
 * HOW:  Thalamus gates sensory inputs to WM; WM predictions bias thalamic attention
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * THALAMUS AS CORTICAL GATEWAY (Sherman & Guillery, 2013):
 * ---------------------------------------------------------
 * The thalamus is not a simple relay but an active filter that gates information
 * flow to cortex based on attention, arousal, and prediction:
 *
 *   Sensory Input -> Thalamus -> [Attention Gate] -> Cortex/World Model
 *   World Model Predictions -> Thalamus -> [Prediction-Based Biasing]
 *
 * PREDICTIVE PROCESSING IN THALAMUS (Kanai et al., 2015):
 * --------------------------------------------------------
 * The thalamus plays a key role in predictive processing:
 *   1. First-order nuclei (LGN, MGN): Gate sensory prediction errors
 *   2. Higher-order nuclei (Pulvinar, MD): Coordinate cortical predictions
 *   3. TRN: Selectively inhibit prediction pathways based on attention
 *
 * THALAMIC NUCLEI INTEGRATION:
 * ----------------------------
 *   - LGN/MGN: Visual/auditory prediction gating
 *   - Pulvinar: Attention-guided prediction selection
 *   - MD: Prefrontal-WM coordination for executive predictions
 *   - VA/VL: Motor prediction relay from BG/cerebellum
 *   - TRN: Inhibitory control of prediction pathways
 *
 * DATA FLOW:
 * ----------
 *   Thalamus -> WM: Gated sensory inputs (filtered by attention)
 *   WM -> Thalamus: Prediction-based attention biasing
 *   TRN -> WM: Selective inhibition of prediction streams
 *   WM -> TRN: Prediction confidence for inhibition modulation
 *   Pulvinar -> WM: Attention weights for prediction prioritization
 *   WM -> Pulvinar: Predicted salience for attention guidance
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E68
 *   Message Range: 0x6800-0x68FF
 */

#ifndef NIMCP_OMNI_WM_THALAMIC_BRIDGE_H
#define NIMCP_OMNI_WM_THALAMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_messages.h"  /* BIO_MSG_WM_THALAMIC_* message types */
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* World Model (from nimcp_omni_world_model.h) */
typedef struct omni_world_model omni_world_model_t;

/* Thalamus System (from nimcp_thalamus.h) */
typedef struct thalamus_t thalamus_t;
typedef struct thal_nucleus thal_nucleus_t;

/* Thalamic Router.  Authoritative typedef lives in
 * middleware/routing/nimcp_thalamic_router.h as
 *   typedef struct thalamic_router thalamic_router_t;
 * Match that here so this header can coexist in the same TU with the
 * authoritative one without a typedef conflict. */
#ifndef NIMCP_THALAMIC_ROUTER_T_DEFINED
#define NIMCP_THALAMIC_ROUTER_T_DEFINED
typedef struct thalamic_router thalamic_router_t;
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model Thalamic Bridge */
#define BIO_MODULE_WM_THALAMIC_BRIDGE         0x0E68

/** Maximum channels per nucleus for gating */
#define WM_THALAMIC_MAX_CHANNELS              256

/** Maximum attention weights tracked */
#define WM_THALAMIC_MAX_ATTENTION_WEIGHTS     64

/** Maximum gated input dimension */
#define WM_THALAMIC_MAX_GATED_DIM             256

/** Default attention baseline */
#define WM_THALAMIC_DEFAULT_ATTENTION         0.5f

/** Default TRN inhibition strength */
#define WM_THALAMIC_DEFAULT_TRN_INHIBITION    0.3f

/** Default prediction confidence threshold for biasing */
#define WM_THALAMIC_DEFAULT_PRED_THRESHOLD    0.6f

/* ============================================================================
 * Bio-Async Message Types (0x6800-0x68FF)
 * ============================================================================
 * Message types are defined in nimcp_bio_messages.h to avoid duplication.
 * Key message types used by this bridge:
 *   - BIO_MSG_WM_THALAMIC_GATE_INPUT (0x6800): Gated sensory input
 *   - BIO_MSG_WM_THALAMIC_ATTENTION_BIAS: Prediction-based attention bias
 *   - BIO_MSG_WM_THALAMIC_TRN_INHIBIT: TRN selective inhibition
 * ============================================================================ */

/** @brief Message type alias for Thalamic bridge (uses bio_message_type_t from nimcp_bio_messages.h) */
typedef bio_message_type_t omni_wm_thalamic_msg_type_t;

/* ============================================================================
 * Thalamic Nucleus Type (for targeting specific nuclei)
 * ============================================================================ */

typedef enum {
    WM_THAL_NUCLEUS_LGN = 0,       /**< Lateral Geniculate - visual */
    WM_THAL_NUCLEUS_MGN,           /**< Medial Geniculate - auditory */
    WM_THAL_NUCLEUS_VPL,           /**< Ventral Posterior Lateral - somatosensory */
    WM_THAL_NUCLEUS_VPM,           /**< Ventral Posterior Medial - face sensory */
    WM_THAL_NUCLEUS_VA,            /**< Ventral Anterior - motor (BG input) */
    WM_THAL_NUCLEUS_VL,            /**< Ventral Lateral - motor (cerebellar) */
    WM_THAL_NUCLEUS_PULVINAR,      /**< Pulvinar - attention coordination */
    WM_THAL_NUCLEUS_MD,            /**< Mediodorsal - prefrontal/executive */
    WM_THAL_NUCLEUS_TRN,           /**< Thalamic Reticular - inhibitory gating */
    WM_THAL_NUCLEUS_COUNT
} wm_thal_nucleus_type_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief World Model Thalamic Bridge configuration
 *
 * WHAT: Parameters controlling WM-Thalamus integration
 * WHY:  Tune attention gating, prediction biasing, and inhibition
 * HOW:  Configurable per-nucleus settings and global modulation
 */
typedef struct {
    /* General Settings */
    bool enable_modulation;                 /**< Enable bidirectional modulation */
    float sensitivity;                      /**< General sensitivity [0.5-2.0] */

    /* Sensory Gating Settings */
    bool enable_sensory_gating;             /**< Gate sensory inputs to WM */
    float attention_baseline;               /**< Baseline attention level [0-1] */
    float min_attention_threshold;          /**< Min attention for input pass [0-1] */
    bool gate_visual;                       /**< Gate visual (LGN) inputs */
    bool gate_auditory;                     /**< Gate auditory (MGN) inputs */
    bool gate_motor;                        /**< Gate motor (VA/VL) inputs */
    bool gate_executive;                    /**< Gate executive (MD) inputs */

    /* Prediction Biasing Settings */
    bool enable_prediction_biasing;         /**< WM predictions bias attention */
    float prediction_confidence_threshold;  /**< Min confidence for biasing */
    float prediction_bias_strength;         /**< Strength of prediction bias [0-1] */
    bool enable_salience_prediction;        /**< Predict salience for pulvinar */

    /* Pulvinar Settings */
    bool enable_pulvinar_coordination;      /**< Coordinate via pulvinar */
    float pulvinar_attention_gain;          /**< Pulvinar attention gain [0.5-2.0] */
    bool pulvinar_guides_prediction;        /**< Pulvinar guides WM predictions */

    /* TRN Inhibition Settings */
    bool enable_trn_inhibition;             /**< Enable TRN-based inhibition */
    float trn_inhibition_strength;          /**< TRN inhibition strength [0-1] */
    float trn_confidence_modulation;        /**< WM confidence modulates TRN */
    bool selective_inhibition;              /**< Selective vs global inhibition */

    /* Firing Mode Settings */
    bool track_firing_modes;                /**< Track tonic/burst modes */
    float burst_mode_threshold;             /**< Threshold for burst detection */
    bool arousal_affects_gating;            /**< Arousal modulates gating */

    /* Per-Nucleus Attention Weights (initial values) */
    float nucleus_attention[WM_THAL_NUCLEUS_COUNT]; /**< Per-nucleus attention */

    /* Bio-async Settings */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
} omni_wm_thalamic_bridge_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief Gated sensory input from thalamus to WM
 *
 * WHAT: Sensory data filtered by thalamic attention gating
 * WHY:  WM receives attention-weighted inputs for prediction
 * HOW:  Raw input multiplied by attention, plus nucleus metadata
 */
typedef struct {
    float* gated_input;                     /**< Attention-gated input values */
    uint32_t input_dim;                     /**< Input dimensionality */
    wm_thal_nucleus_type_t source_nucleus;  /**< Source thalamic nucleus */
    float attention_weight;                 /**< Applied attention weight */
    float arousal_level;                    /**< Current arousal [0-1] */
    bool is_burst_mode;                     /**< Nucleus in burst mode */
    uint64_t timestamp_us;                  /**< Gating timestamp */
} thalamic_gated_input_t;

/**
 * @brief Effects from Thalamus to World Model
 *
 * WHAT: Thalamus-derived information flowing to world model
 * WHY:  Provide gated sensory inputs and attention context
 * HOW:  Per-modality gated inputs, attention state, firing modes
 */
typedef struct {
    /* Gated Sensory Inputs */
    thalamic_gated_input_t visual_input;    /**< LGN gated visual */
    thalamic_gated_input_t auditory_input;  /**< MGN gated auditory */
    thalamic_gated_input_t somatosensory_input; /**< VPL/VPM gated somato */
    thalamic_gated_input_t motor_input;     /**< VA/VL gated motor */
    thalamic_gated_input_t executive_input; /**< MD gated executive */

    /* Attention State */
    float* attention_weights;               /**< Current attention per channel */
    uint32_t attention_dim;                 /**< Attention dimensionality */
    float global_attention;                 /**< Global attention level */

    /* Pulvinar State */
    float* pulvinar_attention;              /**< Pulvinar attention distribution */
    uint32_t pulvinar_dim;                  /**< Pulvinar attention dimension */
    float pulvinar_focus_strength;          /**< Focus intensity [0-1] */

    /* TRN Inhibition State */
    float* trn_inhibition_map;              /**< TRN inhibition per channel */
    uint32_t trn_inhibition_dim;            /**< Inhibition map dimension */
    float global_inhibition;                /**< Global TRN inhibition */

    /* Firing Mode State */
    float tonic_fraction;                   /**< Fraction in tonic mode [0-1] */
    bool dominant_burst_mode;               /**< Dominant mode is burst */
    float arousal_level;                    /**< Global arousal [0-1] */

    /* Per-Nucleus State */
    float nucleus_activity[WM_THAL_NUCLEUS_COUNT]; /**< Activity per nucleus */
    bool nucleus_burst[WM_THAL_NUCLEUS_COUNT];     /**< Burst mode per nucleus */
} thalamus_to_omni_wm_effects_t;

/**
 * @brief Effects from World Model to Thalamus
 *
 * WHAT: WM predictions and confidence flowing to thalamus
 * WHY:  Bias attention based on predictions, modulate TRN
 * HOW:  Prediction-based attention, salience, inhibition modulation
 */
typedef struct {
    /* Prediction-Based Attention Biasing */
    float* attention_bias;                  /**< Suggested attention weights */
    uint32_t attention_bias_dim;            /**< Bias dimensionality */
    float bias_confidence;                  /**< Confidence in bias */

    /* Predicted Salience (for Pulvinar) */
    float* predicted_salience;              /**< Predicted salience map */
    uint32_t salience_dim;                  /**< Salience map dimension */
    float salience_confidence;              /**< Salience prediction confidence */

    /* TRN Modulation */
    float trn_modulation_signal;            /**< WM-driven TRN modulation */
    float* selective_inhibition;            /**< Per-channel inhibition requests */
    uint32_t inhibition_dim;                /**< Inhibition request dimension */

    /* Prediction Error Feedback */
    float* prediction_errors;               /**< Current prediction errors */
    uint32_t pe_dim;                        /**< PE dimensionality */
    float mean_prediction_error;            /**< Average PE magnitude */

    /* Prediction Confidence */
    float forward_confidence;               /**< Forward prediction confidence */
    float backward_confidence;              /**< Backward inference confidence */
    float overall_confidence;               /**< Combined confidence */

    /* Requested Nucleus Attention */
    float requested_attention[WM_THAL_NUCLEUS_COUNT]; /**< Per-nucleus requests */
    bool attention_request_valid;           /**< Request validity flag */
} omni_wm_to_thalamus_effects_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief World Model Thalamic Bridge statistics
 *
 * WHAT: Metrics for monitoring bridge operation
 * WHY:  Performance tracking, debugging, and optimization
 * HOW:  Counters, averages, and per-nucleus metrics
 */
typedef struct {
    /* Sensory Gating Statistics */
    uint64_t inputs_gated;                  /**< Total inputs gated */
    uint64_t inputs_passed;                 /**< Inputs that passed gating */
    uint64_t inputs_blocked;                /**< Inputs blocked by gating */
    float mean_gating_attention;            /**< Average gating attention */

    /* Per-Nucleus Statistics */
    uint64_t nucleus_inputs[WM_THAL_NUCLEUS_COUNT];   /**< Inputs per nucleus */
    uint64_t nucleus_blocked[WM_THAL_NUCLEUS_COUNT];  /**< Blocked per nucleus */
    float nucleus_mean_attention[WM_THAL_NUCLEUS_COUNT]; /**< Avg attention */

    /* Prediction Biasing Statistics */
    uint64_t bias_updates;                  /**< Attention bias updates */
    uint64_t salience_predictions;          /**< Salience predictions made */
    float mean_bias_confidence;             /**< Average bias confidence */

    /* TRN Statistics */
    uint64_t trn_inhibitions;               /**< TRN inhibition events */
    uint64_t trn_releases;                  /**< TRN release events */
    float mean_trn_inhibition;              /**< Average inhibition strength */

    /* Pulvinar Statistics */
    uint64_t pulvinar_coordination_events;  /**< Pulvinar coordination count */
    float mean_pulvinar_focus;              /**< Average focus strength */

    /* Firing Mode Statistics */
    float time_in_tonic;                    /**< Fraction time in tonic */
    float time_in_burst;                    /**< Fraction time in burst */
    uint64_t mode_switches;                 /**< Tonic<->burst switches */

    /* Timing Statistics */
    uint64_t total_updates;                 /**< Total update cycles */
    double total_processing_time_ms;        /**< Total processing time */
    double mean_update_time_ms;             /**< Average update duration */
    uint64_t last_update_time_us;           /**< Last update timestamp */

    /* Error Statistics */
    uint64_t errors_total;                  /**< Total errors */
    uint64_t errors_gating;                 /**< Gating-related errors */
    uint64_t errors_biasing;                /**< Biasing-related errors */
    uint64_t errors_trn;                    /**< TRN-related errors */
} omni_wm_thalamic_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief World Model Thalamic Bridge
 *
 * WHAT: Main bridge structure connecting WM with thalamic systems
 * WHY:  Orchestrates attention-gated prediction flow
 * HOW:  Maintains connections, effects, and state
 *
 * Memory Layout:
 *   bridge_base_t base MUST be first for pointer casting compatibility
 */
typedef struct omni_wm_thalamic_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge */

    /* Configuration */
    omni_wm_thalamic_bridge_config_t config; /**< Bridge configuration */

    /* Connected Systems */
    omni_world_model_t* world_model;        /**< World model (RSSM) */
    thalamus_t* thalamus;                   /**< Thalamus system */
    thalamic_router_t* router;              /**< Thalamic router (optional) */

    /* Bidirectional Effects */
    thalamus_to_omni_wm_effects_t thal_to_wm; /**< Effects: Thalamus -> WM */
    omni_wm_to_thalamus_effects_t wm_to_thal; /**< Effects: WM -> Thalamus */

    /* Internal Buffers */
    float* input_buffer;                    /**< Input staging buffer */
    uint32_t input_buffer_size;             /**< Input buffer size */
    float* attention_buffer;                /**< Attention computation buffer */
    uint32_t attention_buffer_size;         /**< Attention buffer size */
    float* inhibition_buffer;               /**< TRN inhibition buffer */
    uint32_t inhibition_buffer_size;        /**< Inhibition buffer size */

    /* State Tracking */
    float current_arousal;                  /**< Current arousal level */
    bool is_burst_dominant;                 /**< Currently burst-dominant */
    float accumulated_pe;                   /**< Accumulated prediction error */

    /* Statistics */
    omni_wm_thalamic_bridge_stats_t stats;  /**< Bridge statistics */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;      /**< Per-instance health agent */
} omni_wm_thalamic_bridge_t;

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
nimcp_error_t omni_wm_thalamic_bridge_default_config(
    omni_wm_thalamic_bridge_config_t* config);

/**
 * @brief Create World Model Thalamic Bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Required before connecting systems
 * HOW:  Allocate structure, initialize base, set config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
omni_wm_thalamic_bridge_t* omni_wm_thalamic_bridge_create(
    const omni_wm_thalamic_bridge_config_t* config);

/**
 * @brief Destroy World Model Thalamic Bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource management
 * HOW:  Disconnect systems, free buffers, cleanup base
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void omni_wm_thalamic_bridge_destroy(omni_wm_thalamic_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset effects and statistics, keep connections
 * WHY:  Allow fresh start without reconnection
 * HOW:  Zero effects, reset stats, preserve config
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_reset(omni_wm_thalamic_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect all thalamic systems to bridge
 *
 * WHAT: Establish connections to WM, thalamus, and router
 * WHY:  Single call to wire up all systems
 * HOW:  Store pointers, validate connections, activate bridge
 *
 * @param bridge Bridge instance
 * @param world_model World model (RSSM) - required
 * @param thalamus Thalamus system - optional
 * @param router Thalamic router - optional
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_connect(
    omni_wm_thalamic_bridge_t* bridge,
    omni_world_model_t* world_model,
    thalamus_t* thalamus,
    thalamic_router_t* router);

/**
 * @brief Connect world model
 *
 * @param bridge Bridge instance
 * @param world_model World model to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_connect_world_model(
    omni_wm_thalamic_bridge_t* bridge,
    omni_world_model_t* world_model);

/**
 * @brief Connect thalamus
 *
 * @param bridge Bridge instance
 * @param thalamus Thalamus to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_connect_thalamus(
    omni_wm_thalamic_bridge_t* bridge,
    thalamus_t* thalamus);

/**
 * @brief Connect thalamic router
 *
 * @param bridge Bridge instance
 * @param router Router to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_connect_router(
    omni_wm_thalamic_bridge_t* bridge,
    thalamic_router_t* router);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge to check
 * @return true if world model connected (minimum requirement)
 */
bool omni_wm_thalamic_bridge_is_connected(const omni_wm_thalamic_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main update cycle
 *
 * WHAT: Process bidirectional information flow
 * WHY:  Called each timestep to sync WM and thalamus
 * HOW:  Gather thalamic state, compute effects, apply biasing
 *
 * @param bridge Bridge instance
 * @param dt Time delta in seconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_update(
    omni_wm_thalamic_bridge_t* bridge,
    float dt);

/**
 * @brief Set arousal level
 *
 * WHAT: Update global arousal affecting gating
 * WHY:  High arousal = tonic mode, faithful relay
 * HOW:  Update internal state, propagate to thalamus
 *
 * @param bridge Bridge instance
 * @param arousal Arousal level [0-1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_set_arousal(
    omni_wm_thalamic_bridge_t* bridge,
    float arousal);

/* ============================================================================
 * Sensory Gating API
 * ============================================================================ */

/**
 * @brief Gate sensory input through specified nucleus
 *
 * WHAT: Apply attention gating to sensory input
 * WHY:  Filter inputs to WM based on attention state
 * HOW:  Multiply by attention, check threshold, output gated signal
 *
 * @param bridge Bridge instance
 * @param nucleus Target nucleus type
 * @param input Raw sensory input
 * @param input_dim Input dimensionality
 * @param gated_output Output: gated input (pre-allocated)
 * @param output_dim Output buffer size
 * @param attention_applied Output: attention weight applied
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_gate_input(
    omni_wm_thalamic_bridge_t* bridge,
    wm_thal_nucleus_type_t nucleus,
    const float* input,
    uint32_t input_dim,
    float* gated_output,
    uint32_t output_dim,
    float* attention_applied);

/**
 * @brief Gate visual input through LGN
 *
 * @param bridge Bridge instance
 * @param visual_input Visual input
 * @param input_dim Input dimensionality
 * @param gated_output Output: gated visual
 * @param output_dim Output buffer size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_gate_visual(
    omni_wm_thalamic_bridge_t* bridge,
    const float* visual_input,
    uint32_t input_dim,
    float* gated_output,
    uint32_t output_dim);

/**
 * @brief Gate auditory input through MGN
 *
 * @param bridge Bridge instance
 * @param auditory_input Auditory input
 * @param input_dim Input dimensionality
 * @param gated_output Output: gated auditory
 * @param output_dim Output buffer size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_gate_auditory(
    omni_wm_thalamic_bridge_t* bridge,
    const float* auditory_input,
    uint32_t input_dim,
    float* gated_output,
    uint32_t output_dim);

/**
 * @brief Gate motor input through VA/VL
 *
 * @param bridge Bridge instance
 * @param motor_input Motor signal
 * @param input_dim Input dimensionality
 * @param gated_output Output: gated motor
 * @param output_dim Output buffer size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_gate_motor(
    omni_wm_thalamic_bridge_t* bridge,
    const float* motor_input,
    uint32_t input_dim,
    float* gated_output,
    uint32_t output_dim);

/**
 * @brief Gate executive input through MD
 *
 * @param bridge Bridge instance
 * @param executive_input Executive signal
 * @param input_dim Input dimensionality
 * @param gated_output Output: gated executive
 * @param output_dim Output buffer size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_gate_executive(
    omni_wm_thalamic_bridge_t* bridge,
    const float* executive_input,
    uint32_t input_dim,
    float* gated_output,
    uint32_t output_dim);

/* ============================================================================
 * Attention Biasing API
 * ============================================================================ */

/**
 * @brief Set attention bias from WM predictions
 *
 * WHAT: Update thalamic attention based on WM predictions
 * WHY:  Prediction-based attention allows anticipatory filtering
 * HOW:  Compute bias from predictions, apply to thalamic attention
 *
 * @param bridge Bridge instance
 * @param attention_bias Suggested attention weights
 * @param bias_dim Bias dimensionality
 * @param confidence Confidence in bias
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_set_attention_bias(
    omni_wm_thalamic_bridge_t* bridge,
    const float* attention_bias,
    uint32_t bias_dim,
    float confidence);

/**
 * @brief Set attention for specific nucleus
 *
 * @param bridge Bridge instance
 * @param nucleus Target nucleus
 * @param attention Attention level [0-1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_set_nucleus_attention(
    omni_wm_thalamic_bridge_t* bridge,
    wm_thal_nucleus_type_t nucleus,
    float attention);

/**
 * @brief Get current attention for nucleus
 *
 * @param bridge Bridge instance
 * @param nucleus Target nucleus
 * @return Attention level [0-1] or -1 on error
 */
float omni_wm_thalamic_bridge_get_nucleus_attention(
    const omni_wm_thalamic_bridge_t* bridge,
    wm_thal_nucleus_type_t nucleus);

/**
 * @brief Update pulvinar attention weights
 *
 * WHAT: Set pulvinar attention distribution
 * WHY:  Pulvinar coordinates visual attention
 * HOW:  Distribute attention weights across pulvinar channels
 *
 * @param bridge Bridge instance
 * @param attention_weights Pulvinar attention weights
 * @param weights_dim Weights dimensionality
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_set_pulvinar_attention(
    omni_wm_thalamic_bridge_t* bridge,
    const float* attention_weights,
    uint32_t weights_dim);

/**
 * @brief Predict salience for pulvinar guidance
 *
 * WHAT: Compute predicted salience from WM state
 * WHY:  Guide pulvinar attention based on predictions
 * HOW:  Extract salience from WM, send to pulvinar
 *
 * @param bridge Bridge instance
 * @param salience_out Output: predicted salience (pre-allocated)
 * @param salience_dim Salience buffer size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_predict_salience(
    omni_wm_thalamic_bridge_t* bridge,
    float* salience_out,
    uint32_t salience_dim);

/* ============================================================================
 * TRN Inhibition API
 * ============================================================================ */

/**
 * @brief Apply TRN inhibition to prediction pathway
 *
 * WHAT: Selectively inhibit prediction stream via TRN
 * WHY:  Suppress irrelevant predictions, focus resources
 * HOW:  Apply inhibition weights to specified channels
 *
 * @param bridge Bridge instance
 * @param nucleus Target nucleus to inhibit
 * @param inhibition_strength Inhibition strength [0-1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_apply_trn_inhibition(
    omni_wm_thalamic_bridge_t* bridge,
    wm_thal_nucleus_type_t nucleus,
    float inhibition_strength);

/**
 * @brief Apply selective TRN inhibition per channel
 *
 * @param bridge Bridge instance
 * @param inhibition_map Per-channel inhibition weights
 * @param map_dim Map dimensionality
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_apply_selective_inhibition(
    omni_wm_thalamic_bridge_t* bridge,
    const float* inhibition_map,
    uint32_t map_dim);

/**
 * @brief Release TRN inhibition
 *
 * WHAT: Remove TRN inhibition from specified nucleus
 * WHY:  Allow gated inputs to pass after attention shift
 * HOW:  Reset inhibition weights for nucleus
 *
 * @param bridge Bridge instance
 * @param nucleus Nucleus to release
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_release_trn_inhibition(
    omni_wm_thalamic_bridge_t* bridge,
    wm_thal_nucleus_type_t nucleus);

/**
 * @brief Modulate TRN based on WM prediction confidence
 *
 * WHAT: Adjust TRN inhibition based on WM confidence
 * WHY:  High confidence -> more selective inhibition
 * HOW:  Map confidence to inhibition modulation
 *
 * @param bridge Bridge instance
 * @param prediction_confidence WM prediction confidence [0-1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_modulate_trn_from_confidence(
    omni_wm_thalamic_bridge_t* bridge,
    float prediction_confidence);

/* ============================================================================
 * Prediction Integration API
 * ============================================================================ */

/**
 * @brief Provide prediction error feedback to thalamus
 *
 * WHAT: Send WM prediction errors to thalamus
 * WHY:  Prediction errors guide attention and gating
 * HOW:  High PE increases attention to error source
 *
 * @param bridge Bridge instance
 * @param prediction_errors Per-channel prediction errors
 * @param pe_dim PE dimensionality
 * @param mean_pe Average prediction error
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_prediction_error_feedback(
    omni_wm_thalamic_bridge_t* bridge,
    const float* prediction_errors,
    uint32_t pe_dim,
    float mean_pe);

/**
 * @brief Update WM predictions from gated input
 *
 * WHAT: Feed gated sensory input to WM for prediction update
 * WHY:  WM predictions based on attention-filtered inputs
 * HOW:  Encode gated input, update WM state
 *
 * @param bridge Bridge instance
 * @param gated_input Gated sensory input
 * @param input_dim Input dimensionality
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_update_from_gated_input(
    omni_wm_thalamic_bridge_t* bridge,
    const float* gated_input,
    uint32_t input_dim);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effects from thalamus to WM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const thalamus_to_omni_wm_effects_t* omni_wm_thalamic_bridge_get_thalamic_effects(
    const omni_wm_thalamic_bridge_t* bridge);

/**
 * @brief Get current effects from WM to thalamus
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const omni_wm_to_thalamus_effects_t* omni_wm_thalamic_bridge_get_wm_effects(
    const omni_wm_thalamic_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_get_stats(
    const omni_wm_thalamic_bridge_t* bridge,
    omni_wm_thalamic_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_reset_stats(
    omni_wm_thalamic_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_connect_bio_async(
    omni_wm_thalamic_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_thalamic_bridge_disconnect_bio_async(
    omni_wm_thalamic_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool omni_wm_thalamic_bridge_is_bio_async_connected(
    const omni_wm_thalamic_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get nucleus type name string
 *
 * @param nucleus Nucleus type
 * @return Human-readable nucleus name
 */
const char* wm_thal_nucleus_type_to_string(wm_thal_nucleus_type_t nucleus);

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return Human-readable message name
 */
const char* omni_wm_thalamic_msg_type_to_string(omni_wm_thalamic_msg_type_t msg_type);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code describing issue
 */
nimcp_error_t omni_wm_thalamic_bridge_validate_config(
    const omni_wm_thalamic_bridge_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_THALAMIC_BRIDGE_H */
