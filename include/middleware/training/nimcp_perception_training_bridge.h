//=============================================================================
// nimcp_perception_training_bridge.h - Perception-Training Bridge Integration
//=============================================================================
//
// WHAT: Bidirectional bridge integrating 3 perception cortices (Visual,
//       Audio, Speech) with training pipeline for perception-aware learning.
//
// WHY: Models how perceptual quality and salience affect learning. High visual
//      confidence boosts learning, poor audio quality reduces it, speech
//      comprehension guides sample prioritization.
//
// HOW: Perception → Training: Modulates LR, sample weights, attention, skip decisions
//      Training → Cognitive: Signals sensitivity boost, load reduction, consolidation
//      Integrates with cognitive-training, training-logic, training-immune bridges
//
// BIOLOGICAL BASIS:
// - Visual confidence: Clear visual input → enhanced plasticity (attention-gated LTP)
// - Audio quality: Poor signal-to-noise → reduced learning (sensory gating)
// - Speech salience: Phonological salience → prioritized memory encoding
// - Novelty detection: Novel perceptual patterns → curiosity-driven learning
// - Multi-modal integration: Cross-modal binding enhances memory consolidation
//
//=============================================================================

#ifndef NIMCP_PERCEPTION_TRAINING_BRIDGE_H
#define NIMCP_PERCEPTION_TRAINING_BRIDGE_H

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

typedef struct perception_training_bridge perception_training_bridge_t;
typedef struct nimcp_brain_training_ctx nimcp_brain_training_ctx_t;
typedef struct visual_cortex_struct* visual_cortex_t;
typedef struct audio_cortex_struct* audio_cortex_t;
typedef struct speech_cortex_struct* speech_cortex_t;
typedef struct cognitive_training_bridge cognitive_training_bridge_t;
typedef struct training_logic_bridge training_logic_bridge_t;
typedef struct training_plasticity_bridge training_plasticity_bridge_t;
typedef struct training_immune_system training_immune_system_t;

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define PERCEPTION_TRAINING_MODULE_NAME      "perception_training_bridge"
#define PERCEPTION_TRAINING_MODULE_VERSION   "1.0.0"

/* Bio-async module ID defined in nimcp_bio_messages.h as BIO_MODULE_PERCEPTION_TRAINING (0x0523) */

/** Default configuration values */
#define PERCEPTION_TRAINING_DEFAULT_UPDATE_INTERVAL_MS    100
#define PERCEPTION_TRAINING_DEFAULT_LR_MIN_FACTOR         0.5f
#define PERCEPTION_TRAINING_DEFAULT_LR_MAX_FACTOR         1.5f
#define PERCEPTION_TRAINING_DEFAULT_SAMPLE_MIN_WEIGHT     0.1f
#define PERCEPTION_TRAINING_DEFAULT_SAMPLE_MAX_WEIGHT     2.0f

/** Limits */
#define PERCEPTION_TRAINING_MAX_ATTENTION_FEATURES   1024

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Types of perception modulation on training
 *
 * WHAT: Different ways perception state affects training parameters
 * WHY: Each modulation type targets specific training aspect
 * HOW: Computed from perception effects, applied to training
 */
typedef enum {
    PERCEPTION_TRAINING_MODULATION_LR = 0,        /**< Learning rate modulation */
    PERCEPTION_TRAINING_MODULATION_SAMPLE_WEIGHT, /**< Sample importance weighting */
    PERCEPTION_TRAINING_MODULATION_ATTENTION,     /**< Attention-based feature scaling */
    PERCEPTION_TRAINING_MODULATION_SKIP_SAMPLE,   /**< Skip low-quality samples */
    PERCEPTION_TRAINING_MODULATION_COUNT
} perception_training_modulation_t;

/**
 * @brief Feedback events from training to perception cortices
 *
 * WHAT: Training events that trigger perception adjustments
 * WHY: Training progress affects perceptual sensitivity and processing
 * HOW: Each event updates perception cortex parameters
 */
typedef enum {
    PERCEPTION_TRAINING_FEEDBACK_SENSITIVITY_BOOST = 0, /**< Increase perceptual sensitivity */
    PERCEPTION_TRAINING_FEEDBACK_LOAD_REDUCE,           /**< Reduce processing load */
    PERCEPTION_TRAINING_FEEDBACK_NOVELTY_SEEK,          /**< Seek novel perceptual patterns */
    PERCEPTION_TRAINING_FEEDBACK_CONSOLIDATE,           /**< Consolidate perception memories */
    PERCEPTION_TRAINING_FEEDBACK_COUNT
} perception_training_feedback_t;

/**
 * @brief Operation mode for perception-training bridge
 *
 * WHAT: How the bridge processes and applies modulations
 * WHY: Different use cases need different automation levels
 * HOW: Modes control whether modulations are advisory or automatic
 */
typedef enum {
    PERCEPTION_TRAINING_MODE_DISABLED = 0,     /**< Bridge disabled */
    PERCEPTION_TRAINING_MODE_MONITOR_ONLY,     /**< Monitor but don't modulate */
    PERCEPTION_TRAINING_MODE_ADVISORY,         /**< Provide recommendations */
    PERCEPTION_TRAINING_MODE_AUTOMATIC,        /**< Automatically apply modulations */
    PERCEPTION_TRAINING_MODE_COORDINATED       /**< Coordinate with other bridges */
} perception_training_mode_t;

/**
 * @brief Perception priority for training decisions
 *
 * WHAT: Which perception cortex takes priority in conflicts
 * WHY: Visual may override audio in visual-dominant tasks
 * HOW: Priority order determines modulation resolution
 */
typedef enum {
    PERCEPTION_PRIORITY_VISUAL = 0,   /**< Visual cortex priority */
    PERCEPTION_PRIORITY_AUDIO,        /**< Audio cortex priority */
    PERCEPTION_PRIORITY_SPEECH,       /**< Speech cortex priority */
    PERCEPTION_PRIORITY_BALANCED      /**< Weighted combination */
} perception_priority_t;

//=============================================================================
// Perception Effects (Perception → Training)
//=============================================================================

/**
 * @brief Perception effects on training parameters
 *
 * WHAT: Aggregated perception state that modulates training
 * WHY: Captures multi-cortex perceptual influences on learning
 * HOW: Updated from connected perception cortices each cycle
 *
 * BIOLOGICAL ANALOGY: Sensory cortices (V1, A1, STG) provide perceptual
 * quality signals that gate synaptic plasticity in downstream regions.
 * Clear, salient input → enhanced LTP. Noisy, ambiguous input → reduced learning.
 */
typedef struct {
    /* === Visual Cortex Effects === */
    float visual_confidence;         /**< Visual pattern confidence [0-1], high → boost LR */
    float visual_novelty;            /**< Visual novelty score [0-1], high → exploration */
    float* visual_attention_weights; /**< Per-feature attention weights [0-1] */
    uint32_t num_visual_features;    /**< Number of visual features */

    /* === Audio Cortex Effects === */
    float audio_quality;             /**< Audio signal quality [0-1], high → stable LR */
    float speech_salience;           /**< Speech salience [0-1], high → prioritize sample */
    float temporal_coherence;        /**< Temporal coherence [0-1], high → boost learning */

    /* === Speech Cortex Effects === */
    float comprehension;             /**< Speech comprehension [0-1], high → boost LR */
    float phoneme_accuracy;          /**< Phoneme accuracy [0-1], high → confidence */
    float prosody_confidence;        /**< Prosody confidence [0-1], intonation clarity */

    /* === Computed Modulations === */
    float lr_factor;                 /**< Computed LR multiplier [0.5-1.5] */
    float sample_weight;             /**< Sample importance weight [0.1-2.0] */
    bool skip_sample;                /**< Skip this sample (too low quality) */

    /* === Metadata === */
    uint64_t last_update_ms;         /**< When effects were last updated */
    bool valid;                      /**< Whether effects are current */
} perception_training_effects_t;

//=============================================================================
// Training Effects (Training → Perception)
//=============================================================================

/**
 * @brief Training feedback to perception cortices
 *
 * WHAT: Training state that triggers perception adjustments
 * WHY: Learning progress affects perceptual sensitivity and processing
 * HOW: Signals sent to perception cortices after training events
 *
 * BIOLOGICAL ANALOGY: Reward prediction error (dopamine) and attention
 * feedback modulates sensory cortex gain. During learning, sensory cortices
 * adapt their sensitivity to task-relevant features.
 */
typedef struct {
    /* === Loss Metrics === */
    float loss_current;              /**< Current loss value */
    float loss_delta;                /**< Change from previous step */
    float loss_trend;                /**< Smoothed trend [-1,1] */
    bool loss_improved;              /**< Loss decreased this step */

    /* === Gradient Metrics === */
    float gradient_norm;             /**< Current gradient norm */
    bool gradient_stable;            /**< Gradients are stable */

    /* === Perception Feedback === */
    bool boost_sensitivity;          /**< Increase perceptual sensitivity */
    bool reduce_load;                /**< Reduce processing intensity */
    bool seek_novelty;               /**< Search for novel patterns */
    bool consolidate;                /**< Consolidate perception memories */

    /* === Metadata === */
    uint64_t timestamp_ms;           /**< When metrics were captured */
    bool valid;                      /**< Whether metrics are current */
} training_perception_effects_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for perception-training bridge
 *
 * WHAT: All configurable parameters for the bridge
 * WHY: Allow customization for different training scenarios
 * HOW: Passed to create function, copied internally
 */
typedef struct {
    perception_training_mode_t mode;        /**< Operation mode */
    perception_priority_t priority;         /**< Priority resolution strategy */

    /* === Perception Cortex Enables === */
    bool enable_visual;                     /**< Enable visual cortex integration */
    bool enable_audio;                      /**< Enable audio cortex integration */
    bool enable_speech;                     /**< Enable speech cortex integration */

    /* === Modulation Strengths === */
    float visual_strength;                  /**< Visual modulation weight [0-1] */
    float audio_strength;                   /**< Audio modulation weight [0-1] */
    float speech_strength;                  /**< Speech modulation weight [0-1] */

    /* === LR Modulation Limits === */
    float lr_min_factor;                    /**< Minimum LR multiplier (default: 0.5) */
    float lr_max_factor;                    /**< Maximum LR multiplier (default: 1.5) */
    float lr_confidence_scale;              /**< How much confidence affects LR */
    float lr_quality_scale;                 /**< How much quality affects LR */

    /* === Sample Weight Modulation === */
    float sample_min_weight;                /**< Minimum sample weight */
    float sample_max_weight;                /**< Maximum sample weight */
    float sample_salience_threshold;        /**< Salience threshold for weighting */

    /* === Sample Skip Thresholds === */
    float skip_visual_confidence_threshold; /**< Skip if visual_confidence < threshold */
    float skip_audio_quality_threshold;     /**< Skip if audio_quality < threshold */
    float skip_speech_comprehension_threshold; /**< Skip if comprehension < threshold */

    /* === Attention Scaling === */
    bool enable_attention_scaling;          /**< Use attention for per-feature scaling */
    float attention_boost_factor;           /**< Attention weight boost [1.0-2.0] */

    /* === Integration Flags === */
    bool enable_cognitive_training;         /**< Integrate with cognitive-training bridge */
    bool enable_training_logic;             /**< Integrate with training-logic bridge */
    bool enable_training_plasticity;        /**< Integrate with training-plasticity bridge */
    bool enable_training_immune;            /**< Integrate with training-immune system */
    bool enable_bio_async;                  /**< Enable bio-async messaging */

    /* === Update Settings === */
    uint32_t update_interval_ms;            /**< Update interval (default: 100ms) */
    bool disable_auto_update;               /**< Disable automatic updates (testing) */

    /* === Safety Limits === */
    float max_modulation_change_per_step;   /**< Max modulation change per step */
    bool enable_emergency_skip;             /**< Allow emergency sample skip */
} perception_training_config_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Statistics for perception-training bridge
 *
 * WHAT: Tracking of bridge activity and performance
 * WHY: Monitoring and debugging
 * HOW: Accumulated during operation, queryable via API
 */
typedef struct {
    /* === Modulation Counts === */
    uint64_t total_modulations;
    uint64_t modulations_by_type[PERCEPTION_TRAINING_MODULATION_COUNT];
    uint64_t lr_increases;
    uint64_t lr_decreases;
    uint64_t samples_skipped;
    uint64_t samples_prioritized;

    /* === Feedback Counts === */
    uint64_t total_feedback_events;
    uint64_t feedback_by_type[PERCEPTION_TRAINING_FEEDBACK_COUNT];

    /* === Average Effects === */
    float avg_visual_confidence;
    float avg_audio_quality;
    float avg_speech_comprehension;
    float avg_visual_novelty;

    /* === Modulation Factors === */
    float avg_lr_factor;
    float avg_sample_weight;
    float min_lr_factor;
    float max_lr_factor;

    /* === Integration Status === */
    bool visual_connected;
    bool audio_connected;
    bool speech_connected;
    bool cognitive_training_connected;
    bool training_logic_connected;
    bool training_plasticity_connected;
    bool training_immune_connected;
    bool bio_async_connected;

    /* === Timing === */
    uint64_t total_update_calls;
    float avg_update_time_us;
    float max_update_time_us;
    uint64_t last_update_ms;

    /* === Current State === */
    perception_training_mode_t current_mode;
    uint64_t current_training_step;
} perception_training_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize configuration with default values
 *
 * WHAT: Populates config struct with sensible defaults
 * WHY: Ensure all fields have valid initial values
 * HOW: Sets mode to AUTOMATIC, enables all cortices, balanced weights
 *
 * @param config Configuration to initialize (must not be NULL)
 */
void perception_training_default_config(perception_training_config_t* config);

/**
 * @brief Create a new perception-training bridge
 *
 * WHAT: Allocates and initializes bridge structure
 * WHY: Entry point for using the bridge
 * HOW: Allocates effects structures, initializes state
 *
 * @param config Configuration (NULL uses defaults)
 * @return Bridge handle or NULL on failure
 */
perception_training_bridge_t* perception_training_create(
    const perception_training_config_t* config
);

/**
 * @brief Destroy a perception-training bridge
 *
 * WHAT: Frees all resources associated with bridge
 * WHY: Proper cleanup
 * HOW: Disconnects integrations, frees memory, destroys mutex
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void perception_training_destroy(perception_training_bridge_t* bridge);

/**
 * @brief Start the perception-training bridge
 *
 * WHAT: Activates the bridge for operation
 * WHY: Allows deferred start after configuration
 * HOW: Connects bio-async if enabled, initializes update timer
 *
 * @param bridge Bridge to start
 * @return 0 on success, negative on error
 */
int perception_training_start(perception_training_bridge_t* bridge);

/**
 * @brief Stop the perception-training bridge
 *
 * WHAT: Deactivates the bridge
 * WHY: Pause operation without destroying
 * HOW: Disconnects bio-async, preserves state
 *
 * @param bridge Bridge to stop
 * @return 0 on success, negative on error
 */
int perception_training_stop(perception_training_bridge_t* bridge);

//=============================================================================
// Perception Cortex Connection API
//=============================================================================

/**
 * @brief Connect to visual cortex
 *
 * WHAT: Links bridge to visual perception
 * WHY: Visual confidence and novelty modulate learning
 * HOW: Queries visual features, attention, confidence
 *
 * BIOLOGICAL BASIS: V1/V2 provide visual quality signals that
 * gate plasticity in downstream areas (IT, PFC).
 *
 * @param bridge Bridge to connect
 * @param visual_cortex Visual cortex (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int perception_training_connect_visual_cortex(
    perception_training_bridge_t* bridge,
    visual_cortex_t visual_cortex
);

/**
 * @brief Connect to audio cortex
 *
 * WHAT: Links bridge to auditory perception
 * WHY: Audio quality and temporal coherence affect learning
 * HOW: Queries audio features, quality, coherence
 *
 * BIOLOGICAL BASIS: A1 provides auditory signal quality that
 * gates downstream plasticity (STG, frontal areas).
 *
 * @param bridge Bridge to connect
 * @param audio_cortex Audio cortex (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int perception_training_connect_audio_cortex(
    perception_training_bridge_t* bridge,
    audio_cortex_t audio_cortex
);

/**
 * @brief Connect to speech cortex
 *
 * WHAT: Links bridge to speech/language perception
 * WHY: Speech comprehension and phoneme accuracy guide learning
 * HOW: Queries speech features, comprehension, prosody
 *
 * BIOLOGICAL BASIS: STG/Wernicke's area provide linguistic
 * quality signals for language learning tasks.
 *
 * @param bridge Bridge to connect
 * @param speech_cortex Speech cortex (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int perception_training_connect_speech_cortex(
    perception_training_bridge_t* bridge,
    speech_cortex_t speech_cortex
);

//=============================================================================
// Training Integration API
//=============================================================================

/**
 * @brief Connect to brain training context
 *
 * WHAT: Links bridge to training pipeline
 * WHY: Enables automatic metric updates and modulation application
 * HOW: Stores reference, may register callbacks
 *
 * @param bridge Bridge to connect
 * @param training_ctx Training context (may be NULL)
 * @return 0 on success, negative on error
 */
int perception_training_connect_training_context(
    perception_training_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training_ctx
);

/**
 * @brief Connect to cognitive-training bridge
 *
 * WHAT: Links bridge to cognitive training control
 * WHY: Perception state affects cognitive state (novelty → curiosity)
 * HOW: Provides perception state to cognitive bridge
 *
 * @param bridge Bridge to connect
 * @param cognitive_training Cognitive-training bridge (may be NULL)
 * @return 0 on success, negative on error
 */
int perception_training_connect_cognitive_training(
    perception_training_bridge_t* bridge,
    cognitive_training_bridge_t* cognitive_training
);

/**
 * @brief Connect to training-logic bridge
 *
 * WHAT: Links bridge to logic-based training control
 * WHY: Perception conditions feed into logic gates
 * HOW: Provides perception state as logic conditions
 *
 * @param bridge Bridge to connect
 * @param training_logic Training-logic bridge (may be NULL)
 * @return 0 on success, negative on error
 */
int perception_training_connect_training_logic(
    perception_training_bridge_t* bridge,
    training_logic_bridge_t* training_logic
);

/**
 * @brief Connect to training-plasticity bridge
 *
 * WHAT: Links bridge to plasticity-based training control
 * WHY: Perception state affects plasticity mechanisms
 * HOW: Modulates STDP, BCM based on perception effects
 *
 * @param bridge Bridge to connect
 * @param training_plasticity Training-plasticity bridge (may be NULL)
 * @return 0 on success, negative on error
 */
int perception_training_connect_training_plasticity(
    perception_training_bridge_t* bridge,
    training_plasticity_bridge_t* training_plasticity
);

/**
 * @brief Connect to training-immune system
 *
 * WHAT: Links bridge to immune-based training control
 * WHY: Perception failures signal potential threats
 * HOW: Reports low-quality samples as instabilities
 *
 * @param bridge Bridge to connect
 * @param training_immune Training-immune system (may be NULL)
 * @return 0 on success, negative on error
 */
int perception_training_connect_training_immune(
    perception_training_bridge_t* bridge,
    training_immune_system_t* training_immune
);

//=============================================================================
// Perception → Training: Modulation API
//=============================================================================

/**
 * @brief Get current perception effects
 *
 * WHAT: Retrieve aggregated perception state
 * WHY: Query perception modulations for training
 * HOW: Copies internal effects to output
 *
 * @param bridge Bridge to query
 * @param effects Output effects structure
 * @return 0 on success, negative on error
 */
int perception_training_get_effects(
    const perception_training_bridge_t* bridge,
    perception_training_effects_t* effects
);

/**
 * @brief Get modulated learning rate
 *
 * WHAT: Apply perception modulation to base LR
 * WHY: Automatic LR adjustment based on perception state
 * HOW: base_lr × perception_effects.lr_factor
 *
 * BIOLOGICAL BASIS: High visual confidence or audio quality → boost LR.
 * Poor perception quality → reduce LR (conservative learning).
 *
 * @param bridge Bridge to query
 * @param base_lr Base learning rate
 * @return Modulated learning rate
 */
float perception_training_get_modulated_lr(
    const perception_training_bridge_t* bridge,
    float base_lr
);

/**
 * @brief Get sample weight
 *
 * WHAT: Apply perception-based sample weighting
 * WHY: Prioritize high-salience, high-quality samples
 * HOW: Returns sample_weight based on perception effects
 *
 * BIOLOGICAL BASIS: Salient stimuli get prioritized encoding
 * (attentional boost of memory consolidation).
 *
 * @param bridge Bridge to query
 * @return Sample weight [0.1-2.0]
 */
float perception_training_get_sample_weight(
    const perception_training_bridge_t* bridge
);

/**
 * @brief Check if sample should be skipped
 *
 * WHAT: Determine if perception quality is too low
 * WHY: Avoid training on corrupted/ambiguous samples
 * HOW: Returns true if any cortex quality is below threshold
 *
 * BIOLOGICAL BASIS: Sensory gating filters out low-quality input
 * before it reaches higher cognitive areas.
 *
 * @param bridge Bridge to query
 * @return true if sample should be skipped, false otherwise
 */
bool perception_training_should_skip_sample(
    const perception_training_bridge_t* bridge
);

/**
 * @brief Get per-feature attention scaling factors
 *
 * WHAT: Attention-based per-feature gradient modulation
 * WHY: Amplify gradients for visually/auditorily salient features
 * HOW: factors[i] = perception_effects.visual_attention_weights[i]
 *
 * BIOLOGICAL BASIS: Attention selectively amplifies plasticity for
 * attended features (top-down modulation of sensory cortex).
 *
 * @param bridge Bridge to query
 * @param factors Output scaling factors [num_features]
 * @param num_features Number of features
 * @return 0 on success, negative on error
 */
int perception_training_get_attention_scaling(
    const perception_training_bridge_t* bridge,
    float* factors,
    uint32_t num_features
);

//=============================================================================
// Training → Perception: Feedback API
//=============================================================================

/**
 * @brief Update training metrics
 *
 * WHAT: Reports current training state to perception cortices
 * WHY: Training progress triggers perception adjustments
 * HOW: Updates internal state, may trigger feedback events
 *
 * @param bridge Bridge to update
 * @param loss Current loss value
 * @param grad_norm Current gradient norm
 * @return 0 on success, negative on error
 */
int perception_training_update_metrics(
    perception_training_bridge_t* bridge,
    float loss,
    float grad_norm
);

/**
 * @brief Signal a training feedback event
 *
 * WHAT: Notify perception cortices of training event
 * WHY: Events trigger perceptual adjustments
 * HOW: Calls appropriate perception cortex APIs
 *
 * EXAMPLES:
 * - SENSITIVITY_BOOST: Learning improving → increase sensory gain
 * - LOAD_REDUCE: Training struggling → reduce processing intensity
 * - NOVELTY_SEEK: Plateau → search for novel perceptual patterns
 *
 * @param bridge Bridge to signal
 * @param event Feedback event type
 * @param magnitude Event magnitude [0-1]
 * @return 0 on success, negative on error
 */
int perception_training_signal_event(
    perception_training_bridge_t* bridge,
    perception_training_feedback_t event,
    float magnitude
);

//=============================================================================
// Update Cycle API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Main update cycle for bridge
 * WHY: Refresh perception state and compute modulations
 * HOW: Queries perception cortices, computes effects, applies modulations
 *
 * CALL FREQUENCY: Every training step or every N milliseconds
 *
 * @param bridge Bridge to update
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success, negative on error
 */
int perception_training_update(
    perception_training_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Update perception state from connected cortices
 *
 * WHAT: Query all connected perception cortices
 * WHY: Refresh effects structure
 * HOW: Calls APIs for visual, audio, speech cortices
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative on error
 */
int perception_training_update_perception_state(
    perception_training_bridge_t* bridge
);

/**
 * @brief Apply training feedback to perception cortices
 *
 * WHAT: Send training state updates to perception cortices
 * WHY: Training progress affects perception state
 * HOW: Calls perception cortex update APIs with training metrics
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative on error
 */
int perception_training_apply_feedback(
    perception_training_bridge_t* bridge
);

//=============================================================================
// Bio-Async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY: Enable inter-module communication
 * HOW: Registers module, creates inbox
 *
 * @param bridge Bridge to connect
 * @return 0 on success, negative on error
 */
int perception_training_connect_bio_async(
    perception_training_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async system
 * WHY: Clean shutdown
 * HOW: Unregisters module
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, negative on error
 */
int perception_training_disconnect_bio_async(
    perception_training_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY: Determine if messaging is available
 * HOW: Returns internal flag
 *
 * @param bridge Bridge to query
 * @return true if connected, false otherwise
 */
bool perception_training_is_bio_async_connected(
    const perception_training_bridge_t* bridge
);

/**
 * @brief Process incoming bio-async messages
 *
 * WHAT: Handle messages in inbox
 * WHY: Respond to external events
 * HOW: Dequeues and processes messages
 *
 * @param bridge Bridge to process
 * @return Number of messages processed, negative on error
 */
int perception_training_process_inbox(
    perception_training_bridge_t* bridge
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve accumulated statistics
 * WHY: Monitoring and debugging
 * HOW: Copies internal stats to output
 *
 * @param bridge Bridge to query
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int perception_training_get_stats(
    const perception_training_bridge_t* bridge,
    perception_training_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY: Start fresh measurement period
 * HOW: Zeros all stat counters
 *
 * @param bridge Bridge to reset
 * @return 0 on success, negative on error
 */
int perception_training_reset_stats(
    perception_training_bridge_t* bridge
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Convert modulation type enum to string
 *
 * @param modulation Modulation type to convert
 * @return String name of modulation type
 */
const char* perception_training_modulation_to_string(
    perception_training_modulation_t modulation
);

/**
 * @brief Convert feedback event enum to string
 *
 * @param event Feedback event to convert
 * @return String name of event
 */
const char* perception_training_feedback_to_string(
    perception_training_feedback_t event
);

/**
 * @brief Convert mode enum to string
 *
 * @param mode Mode to convert
 * @return String name of mode
 */
const char* perception_training_mode_to_string(
    perception_training_mode_t mode
);

/**
 * @brief Dump bridge state for debugging
 *
 * @param bridge Bridge to dump
 */
void perception_training_dump_state(
    const perception_training_bridge_t* bridge
);

//=============================================================================
// Test API (for unit/integration testing without real perception cortices)
//=============================================================================

/**
 * @brief Set perception effects directly for testing
 *
 * WHAT: Injects perception effects without connected cortices
 * WHY: Enables testing of modulation behavior without full system
 * HOW: Copies provided effects struct into bridge
 *
 * NOTE: This function is intended for testing only. In production,
 * use the proper perception cortex connection APIs.
 *
 * @param bridge Bridge to update
 * @param effects Effects to set
 * @return 0 on success, negative on error
 */
int perception_training_set_effects_for_testing(
    perception_training_bridge_t* bridge,
    const perception_training_effects_t* effects
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PERCEPTION_TRAINING_BRIDGE_H */
