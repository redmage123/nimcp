//=============================================================================
// nimcp_cognitive_training_bridge.h - Cognitive-Training Bridge Integration
//=============================================================================
//
// WHAT: Bidirectional bridge integrating 5 cognitive modules (Executive,
//       Introspection, Attention, Curiosity, Emotion) with training pipeline.
//
// WHY: Models prefrontal-limbic-sensory cognitive control over learning.
//      Cognitive states modulate training (high cognitive load → reduce batch),
//      while training events trigger cognitive responses (divergence → alarm).
//
// HOW: Cognitive → Training: Modulates LR, batch size, gradient scaling, checkpointing
//      Training → Cognitive: Signals satisfaction, frustration, novelty, stagnation
//      Integrates with training-logic, training-plasticity, training-immune bridges
//
// BIOLOGICAL BASIS:
// - Executive: Prefrontal cortex regulates learning effort based on task difficulty
// - Introspection: Metacognitive uncertainty modulates confidence in learning
// - Attention: Feature salience guides gradient scaling and sample prioritization
// - Curiosity: Exploration drive balances exploitation vs exploration
// - Emotion: Emotional state affects learning rate (frustration → reduce LR)
//
//=============================================================================

#ifndef NIMCP_COGNITIVE_TRAINING_BRIDGE_H
#define NIMCP_COGNITIVE_TRAINING_BRIDGE_H

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

typedef struct cognitive_training_bridge cognitive_training_bridge_t;
typedef struct nimcp_brain_training_ctx nimcp_brain_training_ctx_t;
typedef struct executive_controller executive_controller_t;
typedef struct introspection_context_struct* introspection_context_t;
typedef struct multihead_attention_struct* multihead_attention_t;
typedef struct curiosity_engine_struct* curiosity_engine_t;
typedef struct emotion_recognition_system emotion_recognition_system_t;
typedef struct training_logic_bridge training_logic_bridge_t;
typedef struct training_plasticity_bridge training_plasticity_bridge_t;
typedef struct training_immune_system training_immune_system_t;
typedef struct perception_training_bridge perception_training_bridge_t;
typedef struct cortical_training_bridge cortical_training_bridge_t;

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define COGNITIVE_TRAINING_MODULE_NAME      "cognitive_training_bridge"
#define COGNITIVE_TRAINING_MODULE_VERSION   "1.0.0"

/* Bio-async module ID defined in nimcp_bio_messages.h as BIO_MODULE_COGNITIVE_TRAINING (0x0522) */

/** Default configuration values */
#define COGNITIVE_TRAINING_DEFAULT_UPDATE_INTERVAL_MS    100
#define COGNITIVE_TRAINING_DEFAULT_LR_MIN_FACTOR         0.1f
#define COGNITIVE_TRAINING_DEFAULT_LR_MAX_FACTOR         2.0f
#define COGNITIVE_TRAINING_DEFAULT_BATCH_MIN_FACTOR      0.5f
#define COGNITIVE_TRAINING_DEFAULT_BATCH_MAX_FACTOR      2.0f
#define COGNITIVE_TRAINING_DEFAULT_GRADIENT_MIN_SCALE    0.1f
#define COGNITIVE_TRAINING_DEFAULT_GRADIENT_MAX_SCALE    1.5f

/** Limits */
#define COGNITIVE_TRAINING_MAX_FEATURES              1024
#define COGNITIVE_TRAINING_MAX_REASON_LENGTH         512
#define COGNITIVE_TRAINING_MAX_PATTERN_NAME_LENGTH   128

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Types of cognitive modulation on training
 *
 * WHAT: Different ways cognitive state affects training parameters
 * WHY: Each modulation type targets specific training aspect
 * HOW: Computed from cognitive effects, applied to training
 */
typedef enum {
    COGNITIVE_TRAINING_MODULATION_LR = 0,          /**< Learning rate modulation */
    COGNITIVE_TRAINING_MODULATION_BATCH_SIZE,      /**< Batch size adjustment */
    COGNITIVE_TRAINING_MODULATION_GRADIENT_SCALE,  /**< Per-feature gradient scaling */
    COGNITIVE_TRAINING_MODULATION_CHECKPOINT,      /**< Checkpoint triggering */
    COGNITIVE_TRAINING_MODULATION_EXPLORATION,     /**< Exploration intensity */
    COGNITIVE_TRAINING_MODULATION_SAMPLE_PRIORITY, /**< Sample prioritization */
    COGNITIVE_TRAINING_MODULATION_EARLY_STOP,      /**< Early stopping decision */
    COGNITIVE_TRAINING_MODULATION_COUNT
} cognitive_training_modulation_t;

/**
 * @brief Feedback events from training to cognitive modules
 *
 * WHAT: Training events that trigger cognitive responses
 * WHY: Training progress affects cognitive state (satisfaction vs frustration)
 * HOW: Each event updates cognitive modules via API calls
 */
typedef enum {
    COGNITIVE_TRAINING_FEEDBACK_SATISFACTION = 0, /**< Loss improved significantly */
    COGNITIVE_TRAINING_FEEDBACK_FRUSTRATION,      /**< Loss plateaued or worsened */
    COGNITIVE_TRAINING_FEEDBACK_ALARM,            /**< Training divergence detected */
    COGNITIVE_TRAINING_FEEDBACK_NOVELTY,          /**< Novel pattern encountered */
    COGNITIVE_TRAINING_FEEDBACK_MASTERY,          /**< Pattern fully learned */
    COGNITIVE_TRAINING_FEEDBACK_STAGNATION,       /**< No progress for N steps */
    COGNITIVE_TRAINING_FEEDBACK_BREAKTHROUGH,     /**< Sudden improvement */
    COGNITIVE_TRAINING_FEEDBACK_CHECKPOINT_OK,    /**< Checkpoint completed successfully */
    COGNITIVE_TRAINING_FEEDBACK_COUNT
} cognitive_training_feedback_t;

/**
 * @brief Operation mode for cognitive-training bridge
 *
 * WHAT: How the bridge processes and applies modulations
 * WHY: Different use cases need different automation levels
 * HOW: Modes control whether modulations are advisory or automatic
 */
typedef enum {
    COGNITIVE_TRAINING_MODE_DISABLED = 0,     /**< Bridge disabled */
    COGNITIVE_TRAINING_MODE_MONITOR_ONLY,     /**< Monitor but don't modulate */
    COGNITIVE_TRAINING_MODE_ADVISORY,         /**< Provide recommendations */
    COGNITIVE_TRAINING_MODE_AUTOMATIC,        /**< Automatically apply modulations */
    COGNITIVE_TRAINING_MODE_COORDINATED       /**< Coordinate with other bridges */
} cognitive_training_mode_t;

/**
 * @brief Cognitive priority for training decisions
 *
 * WHAT: Which cognitive module takes priority in conflicts
 * WHY: Executive may override curiosity in high-load situations
 * HOW: Priority order determines modulation resolution
 */
typedef enum {
    COGNITIVE_PRIORITY_EXECUTIVE = 0,   /**< Executive function priority */
    COGNITIVE_PRIORITY_INTROSPECTION,   /**< Metacognitive priority */
    COGNITIVE_PRIORITY_EMOTION,         /**< Emotional state priority */
    COGNITIVE_PRIORITY_ATTENTION,       /**< Attention focus priority */
    COGNITIVE_PRIORITY_CURIOSITY,       /**< Exploration priority */
    COGNITIVE_PRIORITY_BALANCED         /**< Weighted combination */
} cognitive_priority_t;

//=============================================================================
// Cognitive Effects (Cognitive → Training)
//=============================================================================

/**
 * @brief Cognitive effects on training parameters
 *
 * WHAT: Aggregated cognitive state that modulates training
 * WHY: Captures multi-module cognitive influences on learning
 * HOW: Updated from connected cognitive modules each cycle
 *
 * BIOLOGICAL ANALOGY: Prefrontal cortex (executive) monitors task difficulty
 * and adjusts effort, while limbic system (emotion) and metacognition
 * (introspection) provide additional regulatory signals.
 */
typedef struct {
    /* === Executive Function Effects === */
    float cognitive_load;            /**< Task difficulty [0-1], high → reduce batch */
    float task_relevance;            /**< Goal alignment [0-1], high → boost LR */
    uint32_t active_goals;           /**< Number of active goals */
    float executive_priority;        /**< Executive priority [0-1] */

    /* === Introspection Effects === */
    float epistemic_uncertainty;     /**< Model uncertainty [0-1], high → conservative LR */
    float consciousness_phi;         /**< IIT consciousness [0-1], low → reduce intensity */
    float metacognitive_confidence;  /**< Confidence in learning [0-1] */
    float calibration_error;         /**< Prediction calibration error [0-1] */

    /* === Attention Effects === */
    float attention_focus;           /**< Overall focus [0-1] */
    float* feature_attention;        /**< Per-feature attention weights [0-1] */
    uint32_t num_features;           /**< Number of features */
    float attention_entropy;         /**< Attention distribution entropy */

    /* === Curiosity Effects === */
    float exploration_drive;         /**< Exploration vs exploitation [0-1] */
    float knowledge_gap_size;        /**< Size of knowledge gaps [0-1] */
    float learning_potential;        /**< Expected learning value [0-1] */
    float intrinsic_motivation;      /**< Intrinsic curiosity [0-1] */

    /* === Emotion Effects === */
    float emotional_valence;         /**< Valence [-1,1], negative → reduce LR */
    float emotional_arousal;         /**< Arousal [0-1], high → increase LR */
    float emotional_salience;        /**< Emotional importance [0-1] */
    float stress_level;              /**< Stress/distress [0-1] */

    /* === Computed Modulations === */
    float lr_factor;                 /**< Computed LR multiplier [0.1-2.0] */
    float batch_size_factor;         /**< Computed batch size multiplier [0.5-2.0] */
    float gradient_scale_factor;     /**< Global gradient scale [0.1-1.5] */
    bool should_checkpoint;          /**< Checkpoint recommended */
    bool should_explore;             /**< Exploration phase recommended */
    bool should_consolidate;         /**< Consolidation phase recommended */

    /* === Metadata === */
    uint64_t last_update_ms;         /**< When effects were last updated */
    bool valid;                      /**< Whether effects are current */
} cognitive_training_effects_t;

//=============================================================================
// Training Effects (Training → Cognitive)
//=============================================================================

/**
 * @brief Training feedback to cognitive modules
 *
 * WHAT: Training state that triggers cognitive responses
 * WHY: Learning progress affects cognitive states (satisfaction, curiosity)
 * HOW: Signals sent to cognitive modules after metric updates
 *
 * BIOLOGICAL ANALOGY: Reward prediction error (dopamine) signals learning
 * progress, updating motivational and emotional states.
 */
typedef struct {
    /* === Loss Metrics === */
    float loss_current;              /**< Current loss value */
    float loss_delta;                /**< Change from previous step */
    float loss_trend;                /**< Smoothed trend [-1,1] */
    bool loss_improved;              /**< Loss decreased this step */
    bool divergence_detected;        /**< Training divergence */

    /* === Gradient Metrics === */
    float gradient_norm;             /**< Current gradient norm */
    float gradient_stability;        /**< Gradient stability [0-1] */
    bool gradient_explosion;         /**< Gradient explosion detected */
    bool gradient_vanishing;         /**< Gradient vanishing detected */

    /* === Learning Progress === */
    float novelty_score;             /**< Novelty of current batch [0-1] */
    bool stagnation_detected;        /**< No progress for N steps */
    bool breakthrough_detected;      /**< Sudden improvement */
    uint32_t steps_since_improvement;/**< Steps without improvement */

    /* === Checkpoint State === */
    uint64_t current_step;           /**< Current training step */
    uint64_t last_checkpoint_step;   /**< Step of last checkpoint */
    bool checkpoint_complete;        /**< Recent checkpoint succeeded */

    /* === Uncertainty Metrics === */
    float prediction_variance;       /**< Prediction variance [0-1] */
    float calibration_error;         /**< Calibration error [0-1] */
    float aleatoric_uncertainty;     /**< Data uncertainty [0-1] */

    /* === Metadata === */
    uint64_t timestamp_ms;           /**< When metrics were captured */
    bool valid;                      /**< Whether metrics are current */
} training_cognitive_effects_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for cognitive-training bridge
 *
 * WHAT: All configurable parameters for the bridge
 * WHY: Allow customization for different training scenarios
 * HOW: Passed to create function, copied internally
 */
typedef struct {
    cognitive_training_mode_t mode;         /**< Operation mode */
    cognitive_priority_t priority;          /**< Priority resolution strategy */

    /* === Cognitive Module Enables === */
    bool enable_executive;                  /**< Enable executive function integration */
    bool enable_introspection;              /**< Enable introspection integration */
    bool enable_attention;                  /**< Enable attention integration */
    bool enable_curiosity;                  /**< Enable curiosity integration */
    bool enable_emotion;                    /**< Enable emotion integration */

    /* === Modulation Strengths === */
    float executive_strength;               /**< Executive modulation weight [0-1] */
    float introspection_strength;           /**< Introspection modulation weight [0-1] */
    float attention_strength;               /**< Attention modulation weight [0-1] */
    float curiosity_strength;               /**< Curiosity modulation weight [0-1] */
    float emotion_strength;                 /**< Emotion modulation weight [0-1] */

    /* === LR Modulation Limits === */
    float lr_min_factor;                    /**< Minimum LR multiplier (default: 0.1) */
    float lr_max_factor;                    /**< Maximum LR multiplier (default: 2.0) */
    float lr_uncertainty_scale;             /**< How much uncertainty reduces LR */
    float lr_emotion_scale;                 /**< How much emotion affects LR */

    /* === Batch Size Modulation === */
    float batch_min_factor;                 /**< Minimum batch size multiplier */
    float batch_max_factor;                 /**< Maximum batch size multiplier */
    float batch_cognitive_load_threshold;   /**< Cognitive load to reduce batch */

    /* === Gradient Scaling === */
    float gradient_min_scale;               /**< Minimum gradient scale */
    float gradient_max_scale;               /**< Maximum gradient scale */
    bool enable_feature_attention_scaling;  /**< Use attention for per-feature scaling */

    /* === Checkpointing === */
    float checkpoint_uncertainty_threshold; /**< Uncertainty to trigger checkpoint */
    float checkpoint_phi_threshold;         /**< Min consciousness for checkpoint */
    uint32_t checkpoint_min_interval_steps; /**< Min steps between checkpoints */

    /* === Exploration === */
    float exploration_curiosity_threshold;  /**< Curiosity to trigger exploration */
    float exploration_plateau_threshold;    /**< Plateau to trigger exploration */

    /* === Feedback Thresholds === */
    float satisfaction_improvement_threshold;  /**< Loss improvement for satisfaction */
    float frustration_plateau_steps;        /**< Steps without improvement → frustration */
    float novelty_threshold;                /**< Novelty score threshold */

    /* === Integration Flags === */
    bool enable_training_logic;             /**< Integrate with training-logic bridge */
    bool enable_training_plasticity;        /**< Integrate with training-plasticity bridge */
    bool enable_training_immune;            /**< Integrate with training-immune system */
    bool enable_bio_async;                  /**< Enable bio-async messaging */

    /* === Update Settings === */
    uint32_t update_interval_ms;            /**< Update interval (default: 100ms) */
    bool disable_auto_update;               /**< Disable automatic updates (testing) */

    /* === Safety Limits === */
    float max_modulation_change_per_step;   /**< Max modulation change per step */
    bool enable_emergency_override;         /**< Allow emergency LR reduction */
} cognitive_training_config_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Statistics for cognitive-training bridge
 *
 * WHAT: Tracking of bridge activity and performance
 * WHY: Monitoring and debugging
 * HOW: Accumulated during operation, queryable via API
 */
typedef struct {
    /* === Modulation Counts === */
    uint64_t total_modulations;
    uint64_t modulations_by_type[COGNITIVE_TRAINING_MODULATION_COUNT];
    uint64_t lr_increases;
    uint64_t lr_decreases;
    uint64_t batch_increases;
    uint64_t batch_decreases;
    uint64_t checkpoints_triggered;
    uint64_t exploration_phases;

    /* === Feedback Counts === */
    uint64_t total_feedback_events;
    uint64_t feedback_by_type[COGNITIVE_TRAINING_FEEDBACK_COUNT];

    /* === Average Effects === */
    float avg_cognitive_load;
    float avg_epistemic_uncertainty;
    float avg_attention_focus;
    float avg_exploration_drive;
    float avg_emotional_valence;

    /* === Modulation Factors === */
    float avg_lr_factor;
    float avg_batch_size_factor;
    float avg_gradient_scale;
    float min_lr_factor;
    float max_lr_factor;

    /* === Integration Status === */
    bool executive_connected;
    bool introspection_connected;
    bool attention_connected;
    bool curiosity_connected;
    bool emotion_connected;
    bool training_logic_connected;
    bool training_plasticity_connected;
    bool training_immune_connected;
    bool perception_training_connected;
    bool cortical_training_connected;
    bool bio_async_connected;

    /* === Timing === */
    uint64_t total_update_calls;
    float avg_update_time_us;
    float max_update_time_us;
    uint64_t last_update_ms;

    /* === Current State === */
    cognitive_training_mode_t current_mode;
    uint64_t current_training_step;
} cognitive_training_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize configuration with default values
 *
 * WHAT: Populates config struct with sensible defaults
 * WHY: Ensure all fields have valid initial values
 * HOW: Sets mode to AUTOMATIC, enables all modules, balanced weights
 *
 * @param config Configuration to initialize (must not be NULL)
 */
void cognitive_training_default_config(cognitive_training_config_t* config);

/**
 * @brief Create a new cognitive-training bridge
 *
 * WHAT: Allocates and initializes bridge structure
 * WHY: Entry point for using the bridge
 * HOW: Allocates effects structures, initializes state
 *
 * @param config Configuration (NULL uses defaults)
 * @return Bridge handle or NULL on failure
 */
cognitive_training_bridge_t* cognitive_training_create(
    const cognitive_training_config_t* config
);

/**
 * @brief Destroy a cognitive-training bridge
 *
 * WHAT: Frees all resources associated with bridge
 * WHY: Proper cleanup
 * HOW: Disconnects integrations, frees memory, destroys mutex
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void cognitive_training_destroy(cognitive_training_bridge_t* bridge);

/**
 * @brief Start the cognitive-training bridge
 *
 * WHAT: Activates the bridge for operation
 * WHY: Allows deferred start after configuration
 * HOW: Connects bio-async if enabled, initializes update timer
 *
 * @param bridge Bridge to start
 * @return 0 on success, negative on error
 */
int cognitive_training_start(cognitive_training_bridge_t* bridge);

/**
 * @brief Stop the cognitive-training bridge
 *
 * WHAT: Deactivates the bridge
 * WHY: Pause operation without destroying
 * HOW: Disconnects bio-async, preserves state
 *
 * @param bridge Bridge to stop
 * @return 0 on success, negative on error
 */
int cognitive_training_stop(cognitive_training_bridge_t* bridge);

//=============================================================================
// Cognitive Module Connection API
//=============================================================================

/**
 * @brief Connect to executive function controller
 *
 * WHAT: Links bridge to executive control module
 * WHY: Executive regulates learning effort based on task difficulty
 * HOW: Queries cognitive load, task relevance, active goals
 *
 * BIOLOGICAL BASIS: DLPFC regulates learning intensity based on
 * working memory load and goal priorities.
 *
 * @param bridge Bridge to connect
 * @param executive Executive controller (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cognitive_training_connect_executive(
    cognitive_training_bridge_t* bridge,
    executive_controller_t* executive
);

/**
 * @brief Connect to introspection module
 *
 * WHAT: Links bridge to metacognitive awareness
 * WHY: Uncertainty modulates learning conservativeness
 * HOW: Queries epistemic uncertainty, consciousness phi, confidence
 *
 * BIOLOGICAL BASIS: Metacognitive monitoring adjusts learning rate
 * based on confidence - low confidence → cautious learning.
 *
 * @param bridge Bridge to connect
 * @param introspection Introspection context (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cognitive_training_connect_introspection(
    cognitive_training_bridge_t* bridge,
    introspection_context_t introspection
);

/**
 * @brief Connect to multihead attention module
 *
 * WHAT: Links bridge to attention mechanism
 * WHY: Feature attention guides gradient scaling
 * HOW: Queries attention weights for per-feature gradient modulation
 *
 * BIOLOGICAL BASIS: Attention selectively amplifies learning for
 * salient features (top-down modulation of synaptic plasticity).
 *
 * @param bridge Bridge to connect
 * @param attention Multihead attention (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cognitive_training_connect_attention(
    cognitive_training_bridge_t* bridge,
    multihead_attention_t attention
);

/**
 * @brief Connect to curiosity engine
 *
 * WHAT: Links bridge to curiosity-driven exploration
 * WHY: Exploration drive balances exploration vs exploitation
 * HOW: Queries knowledge gaps, learning potential, intrinsic motivation
 *
 * BIOLOGICAL BASIS: Dopaminergic circuits encode novelty and
 * knowledge gaps, promoting exploration of unfamiliar patterns.
 *
 * @param bridge Bridge to connect
 * @param curiosity Curiosity engine (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cognitive_training_connect_curiosity(
    cognitive_training_bridge_t* bridge,
    curiosity_engine_t curiosity
);

/**
 * @brief Connect to emotion recognition system
 *
 * WHAT: Links bridge to emotional state
 * WHY: Emotions modulate learning (frustration → reduce LR)
 * HOW: Queries valence, arousal, salience, stress level
 *
 * BIOLOGICAL BASIS: Amygdala and insula modulate learning rate
 * based on emotional state (stress → impaired learning).
 *
 * @param bridge Bridge to connect
 * @param emotion Emotion recognition system (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cognitive_training_connect_emotion(
    cognitive_training_bridge_t* bridge,
    emotion_recognition_system_t* emotion
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
int cognitive_training_connect_training_context(
    cognitive_training_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training_ctx
);

/**
 * @brief Connect to training-logic bridge
 *
 * WHAT: Links bridge to logic-based training control
 * WHY: Cognitive conditions feed into logic gates
 * HOW: Provides cognitive state as logic conditions
 *
 * @param bridge Bridge to connect
 * @param training_logic Training-logic bridge (may be NULL)
 * @return 0 on success, negative on error
 */
int cognitive_training_connect_training_logic(
    cognitive_training_bridge_t* bridge,
    training_logic_bridge_t* training_logic
);

/**
 * @brief Connect to training-plasticity bridge
 *
 * WHAT: Links bridge to plasticity-based training control
 * WHY: Cognitive state affects plasticity mechanisms
 * HOW: Modulates STDP, BCM based on cognitive effects
 *
 * @param bridge Bridge to connect
 * @param training_plasticity Training-plasticity bridge (may be NULL)
 * @return 0 on success, negative on error
 */
int cognitive_training_connect_training_plasticity(
    cognitive_training_bridge_t* bridge,
    training_plasticity_bridge_t* training_plasticity
);

/**
 * @brief Connect to training-immune system
 *
 * WHAT: Links bridge to immune-based training control
 * WHY: Inflammation and stress affect cognitive-training
 * HOW: Immune state modulates cognitive effects
 *
 * @param bridge Bridge to connect
 * @param training_immune Training-immune system (may be NULL)
 * @return 0 on success, negative on error
 */
int cognitive_training_connect_training_immune(
    cognitive_training_bridge_t* bridge,
    training_immune_system_t* training_immune
);

/**
 * @brief Connect to perception-training bridge
 *
 * WHAT: Links cognitive bridge to perception-training for cross-integration
 * WHY: Perception state affects cognitive state (novelty → curiosity, etc.)
 * HOW: Propagates perception effects to cognitive modulations:
 *      - visual_confidence → attention_focus
 *      - speech_salience → task_relevance
 *      - visual_novelty → exploration_drive
 *
 * BIOLOGICAL BASIS: Sensory cortices (V1, A1, STG) provide perceptual quality
 * and salience signals that modulate prefrontal cognitive control. Clear,
 * salient perception enhances attention and focus on learning.
 *
 * @param bridge Bridge to connect
 * @param perception_training Perception-training bridge (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cognitive_training_connect_perception_training(
    cognitive_training_bridge_t* bridge,
    perception_training_bridge_t* perception_training
);

/**
 * @brief Connect to cortical-training bridge
 *
 * WHAT: Links cognitive bridge to cortical-training for cross-integration
 * WHY: Cortical dynamics affect cognitive uncertainty and confidence
 * HOW: Propagates cortical effects to cognitive modulations:
 *      - (1 - burst_rate) → epistemic_uncertainty
 *      - predictions_stable → metacognitive_confidence
 *      - free_energy → cognitive_load (high FE = high load)
 *
 * BIOLOGICAL BASIS: Predictive coding and dendritic bursting in cortical
 * hierarchies provide signals about model uncertainty and prediction stability
 * that inform metacognitive awareness in prefrontal cortex.
 *
 * @param bridge Bridge to connect
 * @param cortical_training Cortical-training bridge (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cognitive_training_connect_cortical_training(
    cognitive_training_bridge_t* bridge,
    cortical_training_bridge_t* cortical_training
);

//=============================================================================
// Cognitive → Training: Modulation API
//=============================================================================

/**
 * @brief Get current cognitive effects
 *
 * WHAT: Retrieve aggregated cognitive state
 * WHY: Query cognitive modulations for training
 * HOW: Copies internal effects to output
 *
 * @param bridge Bridge to query
 * @param effects Output effects structure
 * @return 0 on success, negative on error
 */
int cognitive_training_get_effects(
    const cognitive_training_bridge_t* bridge,
    cognitive_training_effects_t* effects
);

/**
 * @brief Get modulated learning rate
 *
 * WHAT: Apply cognitive modulation to base LR
 * WHY: Automatic LR adjustment based on cognitive state
 * HOW: base_lr × cognitive_effects.lr_factor
 *
 * BIOLOGICAL BASIS: High cognitive load or uncertainty → reduce LR.
 * Positive emotion or high attention → increase LR.
 *
 * @param bridge Bridge to query
 * @param base_lr Base learning rate
 * @return Modulated learning rate
 */
float cognitive_training_get_modulated_lr(
    const cognitive_training_bridge_t* bridge,
    float base_lr
);

/**
 * @brief Get modulated batch size
 *
 * WHAT: Apply cognitive modulation to batch size
 * WHY: Automatic batch size adjustment based on cognitive load
 * HOW: base_batch_size × cognitive_effects.batch_size_factor
 *
 * BIOLOGICAL BASIS: High cognitive load → reduce batch (working memory limit).
 * Low load → increase batch (parallel processing).
 *
 * @param bridge Bridge to query
 * @param base_batch_size Base batch size
 * @return Modulated batch size
 */
uint32_t cognitive_training_get_modulated_batch_size(
    const cognitive_training_bridge_t* bridge,
    uint32_t base_batch_size
);

/**
 * @brief Get per-feature gradient scaling factors
 *
 * WHAT: Attention-based per-feature gradient modulation
 * WHY: Amplify gradients for salient features, suppress for irrelevant
 * HOW: factors[i] = cognitive_effects.feature_attention[i]
 *
 * BIOLOGICAL BASIS: Attention selectively amplifies plasticity for
 * attended features (top-down modulation of LTP/LTD).
 *
 * @param bridge Bridge to query
 * @param factors Output scaling factors [num_features]
 * @param num_features Number of features
 * @return 0 on success, negative on error
 */
int cognitive_training_get_gradient_scaling(
    const cognitive_training_bridge_t* bridge,
    float* factors,
    uint32_t num_features
);

/**
 * @brief Check if checkpoint should be created
 *
 * WHAT: Determine if cognitive state warrants checkpoint
 * WHY: Checkpoint at high uncertainty or low consciousness
 * HOW: Returns cognitive_effects.should_checkpoint
 *
 * BIOLOGICAL BASIS: Metacognitive uncertainty signals need to
 * consolidate current knowledge before continuing.
 *
 * @param bridge Bridge to query
 * @return true if checkpoint recommended, false otherwise
 */
bool cognitive_training_should_checkpoint(
    const cognitive_training_bridge_t* bridge
);

/**
 * @brief Get exploration intensity
 *
 * WHAT: Determine exploration vs exploitation balance
 * WHY: Curiosity drives exploration of novel patterns
 * HOW: Returns exploration drive [0-1]
 *
 * BIOLOGICAL BASIS: High novelty/knowledge gaps → explore.
 * High mastery → exploit (consolidate).
 *
 * @param bridge Bridge to query
 * @return Exploration intensity [0-1]
 */
float cognitive_training_get_exploration_intensity(
    const cognitive_training_bridge_t* bridge
);

//=============================================================================
// Training → Cognitive: Feedback API
//=============================================================================

/**
 * @brief Update training metrics
 *
 * WHAT: Reports current training state to cognitive modules
 * WHY: Training progress triggers cognitive responses
 * HOW: Updates internal state, may trigger feedback events
 *
 * @param bridge Bridge to update
 * @param loss Current loss value
 * @param grad_norm Current gradient norm
 * @param lr Current learning rate
 * @param step Current training step
 * @return 0 on success, negative on error
 */
int cognitive_training_update_metrics(
    cognitive_training_bridge_t* bridge,
    float loss,
    float grad_norm,
    float lr,
    uint64_t step
);

/**
 * @brief Signal a training feedback event
 *
 * WHAT: Notify cognitive modules of training event
 * WHY: Events trigger emotional/cognitive responses
 * HOW: Calls appropriate cognitive module APIs
 *
 * EXAMPLES:
 * - SATISFACTION: Loss improved → positive emotion, reduce uncertainty
 * - FRUSTRATION: Plateau → negative emotion, increase exploration
 * - ALARM: Divergence → stress, emergency checkpoint
 *
 * @param bridge Bridge to signal
 * @param event Feedback event type
 * @param magnitude Event magnitude [0-1]
 * @return 0 on success, negative on error
 */
int cognitive_training_signal_event(
    cognitive_training_bridge_t* bridge,
    cognitive_training_feedback_t event,
    float magnitude
);

/**
 * @brief Report pattern learned
 *
 * WHAT: Notify curiosity module of learning progress
 * WHY: Reduces knowledge gap, updates motivation
 * HOW: Calls curiosity API with pattern info
 *
 * @param bridge Bridge to update
 * @param pattern_name Name of learned pattern
 * @param novelty Novelty score [0-1]
 * @return 0 on success, negative on error
 */
int cognitive_training_pattern_learned(
    cognitive_training_bridge_t* bridge,
    const char* pattern_name,
    float novelty
);

/**
 * @brief Report checkpoint completion
 *
 * WHAT: Notify cognitive modules of successful checkpoint
 * WHY: Reduces uncertainty, increases confidence
 * HOW: Updates introspection and executive state
 *
 * @param bridge Bridge to update
 * @param step Step number of checkpoint
 * @return 0 on success, negative on error
 */
int cognitive_training_checkpoint_complete(
    cognitive_training_bridge_t* bridge,
    uint64_t step
);

//=============================================================================
// Update Cycle API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Main update cycle for bridge
 * WHY: Refresh cognitive state and compute modulations
 * HOW: Queries cognitive modules, computes effects, applies modulations
 *
 * CALL FREQUENCY: Every training step or every N milliseconds
 *
 * @param bridge Bridge to update
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success, negative on error
 */
int cognitive_training_update(
    cognitive_training_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Update cognitive state from connected modules
 *
 * WHAT: Query all connected cognitive modules
 * WHY: Refresh effects structure
 * HOW: Calls APIs for executive, introspection, attention, curiosity, emotion
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative on error
 */
int cognitive_training_update_cognitive_state(
    cognitive_training_bridge_t* bridge
);

/**
 * @brief Apply training feedback to cognitive modules
 *
 * WHAT: Send training state updates to cognitive modules
 * WHY: Training progress affects cognitive state
 * HOW: Calls cognitive module update APIs with training metrics
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative on error
 */
int cognitive_training_apply_feedback(
    cognitive_training_bridge_t* bridge
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
int cognitive_training_connect_bio_async(
    cognitive_training_bridge_t* bridge
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
int cognitive_training_disconnect_bio_async(
    cognitive_training_bridge_t* bridge
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
bool cognitive_training_is_bio_async_connected(
    const cognitive_training_bridge_t* bridge
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
int cognitive_training_process_inbox(
    cognitive_training_bridge_t* bridge
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
int cognitive_training_get_stats(
    const cognitive_training_bridge_t* bridge,
    cognitive_training_stats_t* stats
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
int cognitive_training_reset_stats(
    cognitive_training_bridge_t* bridge
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
const char* cognitive_training_modulation_to_string(
    cognitive_training_modulation_t modulation
);

/**
 * @brief Convert feedback event enum to string
 *
 * @param event Feedback event to convert
 * @return String name of event
 */
const char* cognitive_training_feedback_to_string(
    cognitive_training_feedback_t event
);

/**
 * @brief Convert mode enum to string
 *
 * @param mode Mode to convert
 * @return String name of mode
 */
const char* cognitive_training_mode_to_string(
    cognitive_training_mode_t mode
);

/**
 * @brief Dump bridge state for debugging
 *
 * @param bridge Bridge to dump
 */
void cognitive_training_dump_state(
    const cognitive_training_bridge_t* bridge
);

//=============================================================================
// Test API (for unit/integration testing without real cognitive modules)
//=============================================================================

/**
 * @brief Set cognitive effects directly for testing
 *
 * WHAT: Injects cognitive effects without connected modules
 * WHY: Enables testing of modulation behavior without full system
 * HOW: Copies provided effects struct into bridge
 *
 * NOTE: This function is intended for testing only. In production,
 * use the proper cognitive module connection APIs.
 *
 * @param bridge Bridge to update
 * @param effects Effects to set
 * @return 0 on success, negative on error
 */
int cognitive_training_set_effects_for_testing(
    cognitive_training_bridge_t* bridge,
    const cognitive_training_effects_t* effects
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COGNITIVE_TRAINING_BRIDGE_H */
