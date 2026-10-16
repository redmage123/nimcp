/**
 * @file nimcp_salience.h
 * @brief Salience and attention evaluation for active consciousness
 *
 * WHAT: Fast evaluation of input "interestingness" without full decision
 * WHY: Active consciousness needs to know what to pay attention to
 * HOW: Partial network activation + novelty detection + surprise measurement
 *
 * DESIGN RATIONALE:
 * Full brain decisions are expensive (~1ms). Salience evaluation is ~10x faster
 * (~0.1ms) by using:
 * - Early network layers only (not full propagation)
 * - Recent input history for novelty detection
 * - Prediction error for surprise measurement
 * - Heuristic urgency scoring
 *
 * DESIGN PATTERNS:
 * - Strategy Pattern: Different salience computation strategies
 * - Memento Pattern: Recent input history for comparison
 * - Observer Pattern: Attention threshold callbacks
 * - Factory Pattern: Salience evaluator creation
 *
 * BIOLOGICAL INSPIRATION:
 * Human attention is selective - we notice:
 * 1. Novel stimuli (never seen before)
 * 2. Surprising stimuli (violated expectations)
 * 3. Salient stimuli (important/intense)
 * 4. Urgent stimuli (requires immediate response)
 *
 * This API models those four attention mechanisms.
 *
 * EXAMPLE:
 * @code
 *   // Create salience evaluator
 *   salience_evaluator_t eval = salience_evaluator_create(brain, &config);
 *
 *   // Fast salience check (~0.1ms vs 1ms for full decision)
 *   brain_salience_t salience = brain_evaluate_salience(eval, features, 13);
 *
 *   if (salience.urgency > 0.9) {
 *       // Immediate LLM reasoning needed
 *       response = llm_deliberate(features);
 *   } else if (salience.novelty > 0.8) {
 *       // Novel input - learn from it
 *       brain_learn_example(brain, features, label, 1.0);
 *   } else if (salience.surprise > 0.7) {
 *       // Unexpected - update predictions
 *       brain_update_predictions(brain, features);
 *   } else {
 *       // Normal - fast neural path
 *       decision = brain_decide(brain, features, num_features);
 *   }
 *
 *   salience_evaluator_destroy(eval);
 * @endcode
 */

#ifndef NIMCP_SALIENCE_H
#define NIMCP_SALIENCE_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations and Opaque Types
//=============================================================================

/**
 * WHAT: Opaque salience evaluator handle
 * WHY: Hides implementation details, maintains encapsulation
 * PATTERN: Opaque pointer (Pimpl idiom)
 */
typedef struct salience_evaluator_struct* salience_evaluator_t;

//=============================================================================
// Cross-Modal Salience Fusion (Multimodal Integration)
//=============================================================================

/**
 * WHAT: Sensory modality types for cross-modal salience
 * WHY: Different sensory channels have different salience contributions
 * HOW: Enumerate all sensory modalities that feed into attention
 *
 * BIOLOGICAL BASIS:
 * - Superior colliculus integrates visual, auditory, somatosensory salience
 * - Parietal cortex performs cross-modal attention enhancement
 * - Multimodal integration creates unified attention map
 */
typedef enum {
    SALIENCE_MODALITY_VISUAL = 0,       /**< Visual salience (V1, V4, IT) */
    SALIENCE_MODALITY_AUDIO = 1,        /**< Auditory salience (A1, auditory cortex) */
    SALIENCE_MODALITY_SOMATOSENSORY = 2,/**< Touch/proprioception (S1, S2) */
    SALIENCE_MODALITY_LINGUISTIC = 3,   /**< Language salience (Wernicke's, Broca's) */
    SALIENCE_MODALITY_COUNT = 4         /**< Total number of modalities */
} salience_modality_t;

/**
 * WHAT: Fusion strategy for combining modality saliences
 * WHY: Different situations require different integration methods
 * HOW: Strategy pattern - behavior changes based on strategy
 *
 * STRATEGIES:
 * - MAX: Winner-take-all (most salient modality dominates)
 * - WEIGHTED_AVG: Weighted average (balanced integration)
 * - LEARNED: Tensor-based learned fusion (adaptive weights)
 */
typedef enum {
    /**
     * WHAT: Maximum fusion (winner-take-all)
     * WHY: Dominant modality captures attention
     * WHEN: Strong unimodal signal (e.g., loud noise, bright flash)
     * BIOLOGICAL: Superior colliculus winner-take-all circuits
     */
    SALIENCE_FUSION_MAX,

    /**
     * WHAT: Weighted average fusion
     * WHY: Balanced integration of all modalities
     * WHEN: Multiple weak signals need combining
     * BIOLOGICAL: Parietal cortex weighted integration
     */
    SALIENCE_FUSION_WEIGHTED_AVG,

    /**
     * WHAT: Learned tensor-based fusion
     * WHY: Adaptive weights based on context
     * WHEN: Complex multimodal scenes
     * BIOLOGICAL: Prefrontal cortex context-dependent attention
     */
    SALIENCE_FUSION_LEARNED

} salience_fusion_strategy_t;

//=============================================================================
// Salience Metrics and Structures
//=============================================================================

/**
 * WHAT: Comprehensive salience scores for an input
 * WHY: Multiple dimensions of "interestingness" for different use cases
 * HOW: Each metric computed independently, combined for overall salience
 *
 * INTERPRETATION:
 * - All values are 0.0-1.0 (normalized)
 * - Higher values = more attention-worthy
 * - Use individual metrics or combined salience depending on needs
 */
typedef struct {
    /**
     * WHAT: Overall salience (combined metric)
     * WHY: Single score for general attention allocation
     * HOW: Weighted combination of novelty, surprise, urgency
     * RANGE: 0.0 (ignorable) to 1.0 (critical attention)
     */
    float salience;

    /**
     * WHAT: Novelty score (how different from recent inputs)
     * WHY: Novel stimuli deserve attention (learning opportunity)
     * HOW: Compare to recent input history, measure distance
     * RANGE: 0.0 (very familiar) to 1.0 (completely novel)
     *
     * BIOLOGICAL: Human attention drawn to novel stimuli
     */
    float novelty;

    /**
     * WHAT: Surprise score (prediction error magnitude)
     * WHY: Surprising events indicate model needs updating
     * HOW: |predicted - actual| for features
     * RANGE: 0.0 (expected) to 1.0 (totally unexpected)
     *
     * BIOLOGICAL: Prediction errors drive learning
     */
    float surprise;

    /**
     * WHAT: Urgency score (needs immediate attention)
     * WHY: Some situations require fast response
     * HOW: Confidence variance + temporal dynamics + learned urgency patterns
     * RANGE: 0.0 (can wait) to 1.0 (immediate action required)
     *
     * BIOLOGICAL: Threat detection, reflexive responses
     */
    float urgency;

    /**
     * WHAT: Confidence in salience evaluation
     * WHY: Sometimes salience itself is uncertain
     * HOW: Based on input quality, history depth, model certainty
     * RANGE: 0.0 (uncertain salience) to 1.0 (confident salience)
     */
    float confidence;

    /**
     * WHAT: Computational cost of full decision
     * WHY: Cost-benefit analysis for attention allocation
     * HOW: Estimated from network size, complexity
     * RANGE: 0.0 (cheap) to 1.0 (expensive)
     *
     * USE CASE: If salience is low but cost is high, skip full decision
     */
    float estimated_cost;

} brain_salience_t;

/**
 * WHAT: Salience computation strategy
 * WHY: Different applications need different salience metrics
 * HOW: Strategy pattern - behavior changes based on strategy
 */
typedef enum {
    /**
     * WHAT: Fast heuristic salience
     * WHY: Maximum speed, acceptable accuracy
     * WHEN: High-frequency input, latency critical
     * ACCURACY: ~80%
     * SPEED: ~0.05ms
     */
    SALIENCE_STRATEGY_FAST,

    /**
     * WHAT: Balanced salience evaluation
     * WHY: Good speed-accuracy tradeoff
     * WHEN: Normal operation
     * ACCURACY: ~90%
     * SPEED: ~0.1ms
     */
    SALIENCE_STRATEGY_BALANCED,

    /**
     * WHAT: Accurate deep salience
     * WHY: Maximum accuracy for critical decisions
     * WHEN: Low-frequency input, accuracy critical
     * ACCURACY: ~95%
     * SPEED: ~0.5ms
     */
    SALIENCE_STRATEGY_ACCURATE

} salience_strategy_t;

/**
 * WHAT: Salience evaluator configuration
 * WHY: Flexible configuration for different use cases
 * PATTERN: Builder pattern - step-by-step configuration
 */
typedef struct {
    // Strategy selection
    salience_strategy_t strategy; /**< Computation strategy */

    // History for novelty detection
    uint32_t history_size; /**< How many recent inputs to remember */
    bool enable_novelty;   /**< Compute novelty scores? */

    // Prediction for surprise detection
    bool enable_surprise;   /**< Compute surprise scores? */
    bool enable_prediction; /**< Maintain predictive model? */

    // Urgency detection
    bool enable_urgency;    /**< Compute urgency scores? */
    float urgency_baseline; /**< Baseline urgency (0-1) */

    // Weighting for combined salience
    float novelty_weight;  /**< Weight for novelty (default: 0.3) */
    float surprise_weight; /**< Weight for surprise (default: 0.4) */
    float urgency_weight;  /**< Weight for urgency (default: 0.3) */

    // Attention thresholds (for callbacks)
    float high_salience_threshold; /**< Trigger callback above this */
    float high_novelty_threshold;  /**< Trigger callback for novel */
    float high_surprise_threshold; /**< Trigger callback for surprising */
    float high_urgency_threshold;  /**< Trigger callback for urgent */

    // Performance tuning
    bool enable_caching; /**< Cache recent evaluations? */
    uint32_t cache_size; /**< Cache size if enabled */

    // Bio-async communication
    bool enable_bio_async; /**< Enable bio-async communication */

    // Cross-modal fusion configuration
    bool enable_multimodal;         /**< Enable multimodal fusion? */
    float default_modality_weight;  /**< Default weight for new modalities */
    salience_fusion_strategy_t fusion_strategy; /**< Fusion strategy */

} salience_config_t;

/**
 * WHAT: Salience event types for callbacks
 * WHY: Notify application when attention thresholds exceeded
 * PATTERN: Observer pattern
 */
typedef enum {
    SALIENCE_EVENT_HIGH_SALIENCE, /**< Overall salience high */
    SALIENCE_EVENT_HIGH_NOVELTY,  /**< Novel input detected */
    SALIENCE_EVENT_HIGH_SURPRISE, /**< Surprising input detected */
    SALIENCE_EVENT_HIGH_URGENCY   /**< Urgent input detected */
} salience_event_type_t;

/**
 * WHAT: Salience event data for callbacks
 * WHY: Provide context about attention event
 */
typedef struct {
    salience_event_type_t type; /**< Event type */
    brain_salience_t salience;  /**< Salience scores */
    const float* features;      /**< Input features (read-only) */
    uint32_t num_features;      /**< Feature count */
    uint64_t timestamp;         /**< When event occurred */
    const char* message;        /**< Human-readable description */
} salience_event_t;

/**
 * WHAT: Salience event callback function type
 * WHY: Allow application to react to attention events
 * PATTERN: Observer pattern - callback is the observer
 *
 * @param event Event that occurred
 * @param context User-provided context pointer
 */
typedef void (*salience_event_callback_fn)(const salience_event_t* event, void* context);

//=============================================================================
// Salience Evaluator Lifecycle (Factory Pattern)
//=============================================================================

/**
 * WHAT: Create salience evaluator
 * WHY: Factory function - single creation point with validation
 * HOW: Validates config, allocates resources, initializes history
 *
 * COMPLEXITY: O(h) where h = history_size
 * THREAD-SAFE: Yes
 * PATTERN: Factory pattern
 *
 * @param brain Brain to evaluate salience for
 * @param config Salience configuration (copied internally)
 * @return Evaluator handle or NULL on error
 *
 * ERROR CONDITIONS:
 * - NULL brain: Returns NULL, sets error "Invalid brain"
 * - NULL config: Returns NULL, sets error "Invalid config"
 * - Invalid history_size: Returns NULL, sets error "History size too large"
 */
salience_evaluator_t salience_evaluator_create(brain_t brain, const salience_config_t* config);

/**
 * WHAT: Destroy salience evaluator and free resources
 * WHY: Clean shutdown and memory cleanup
 * HOW: Frees history, cache, and structure
 *
 * COMPLEXITY: O(h + c) where h = history_size, c = cache_size
 * THREAD-SAFE: Yes (but evaluator should not be used after this call)
 *
 * @param evaluator Evaluator to destroy (NULL is safe)
 */
void salience_evaluator_destroy(salience_evaluator_t evaluator);

//=============================================================================
// Salience Evaluation Functions
//=============================================================================

/**
 * WHAT: Evaluate salience of single input (fast)
 * WHY: Quick check of "interestingness" before expensive full decision
 * HOW: Partial network activation + history comparison + prediction error
 *
 * COMPLEXITY: O(1) - constant time heuristics
 * THREAD-SAFE: Yes (uses internal mutex)
 * PERFORMANCE: ~0.1ms (10x faster than brain_decide)
 *
 * @param evaluator Salience evaluator handle
 * @param features Input feature vector
 * @param num_features Size of feature vector
 * @return Salience scores, or all zeros on error
 *
 * BEHAVIOR:
 * - Computes novelty by comparing to recent history
 * - Computes surprise by comparing to prediction
 * - Computes urgency using learned patterns
 * - Combines into overall salience score
 * - Updates history for next evaluation
 * - Triggers callbacks if thresholds exceeded
 *
 * USE CASE:
 * @code
 *   brain_salience_t s = brain_evaluate_salience(eval, features, 13);
 *   if (s.urgency > 0.9) {
 *       // Immediate attention needed
 *       handle_urgent(features);
 *   } else if (s.novelty > 0.8) {
 *       // Novel - worth learning from
 *       brain_learn_example(brain, features, label, 1.0);
 *   } else if (s.salience < 0.3) {
 *       // Not interesting - skip expensive decision
 *       return;
 *   } else {
 *       // Normal - proceed with decision
 *       decision = brain_decide(brain, features, num_features);
 *   }
 * @endcode
 */
brain_salience_t brain_evaluate_salience(salience_evaluator_t evaluator, const float* features,
                                         uint32_t num_features);

/**
 * WHAT: Evaluate salience for batch of inputs (efficient)
 * WHY: Batch processing is much faster than individual evaluations
 * HOW: Vectorized operations, shared history comparisons
 *
 * COMPLEXITY: O(n) where n = num_samples (but with better constant factor)
 * THREAD-SAFE: Yes
 * PERFORMANCE: ~10-100x faster than N individual calls
 *
 * @param evaluator Salience evaluator handle
 * @param features Array of feature vectors
 * @param num_samples Number of samples
 * @param num_features Features per sample
 * @param salience_scores Output array (pre-allocated, size = num_samples)
 * @return Number of samples successfully evaluated
 *
 * USE CASE:
 * @code
 *   // Evaluate 100 inputs
 *   brain_salience_t scores[100];
 *   brain_evaluate_salience_batch(eval, features, 100, 13, scores);
 *
 *   // Process only high-salience inputs
 *   for (int i = 0; i < 100; i++) {
 *       if (scores[i].salience > 0.7) {
 *           process_input(features[i]);
 *       }
 *   }
 * @endcode
 */
uint32_t brain_evaluate_salience_batch(salience_evaluator_t evaluator, const float** features,
                                       uint32_t num_samples, uint32_t num_features,
                                       brain_salience_t* salience_scores);

/**
 * WHAT: Evaluate salience with explicit timestamp
 * WHY: Temporal dynamics affect salience (recency, rate of change)
 * HOW: Uses timestamp for temporal weighting and decay
 *
 * @param evaluator Salience evaluator handle
 * @param features Input features
 * @param num_features Feature count
 * @param timestamp Input timestamp (milliseconds)
 * @return Salience scores
 */
brain_salience_t brain_evaluate_salience_temporal(salience_evaluator_t evaluator,
                                                  const float* features, uint32_t num_features,
                                                  uint64_t timestamp);

//=============================================================================
// Salience Configuration and Control
//=============================================================================

/**
 * WHAT: Update salience weights dynamically
 * WHY: Adapt attention mechanism to changing conditions
 * HOW: Updates internal weights without rebuilding evaluator
 *
 * @param evaluator Evaluator handle
 * @param novelty_weight Weight for novelty (0-1)
 * @param surprise_weight Weight for surprise (0-1)
 * @param urgency_weight Weight for urgency (0-1)
 * @return true on success
 *
 * NOTE: Weights will be normalized to sum to 1.0
 */
bool salience_set_weights(salience_evaluator_t evaluator, float novelty_weight,
                          float surprise_weight, float urgency_weight);

/**
 * WHAT: Set attention thresholds for callbacks
 * WHY: Dynamically adjust attention sensitivity
 * HOW: Updates thresholds in configuration
 *
 * @param evaluator Evaluator handle
 * @param high_salience_threshold Overall salience threshold (0-1)
 * @param high_novelty_threshold Novelty threshold (0-1)
 * @param high_surprise_threshold Surprise threshold (0-1)
 * @param high_urgency_threshold Urgency threshold (0-1)
 * @return true on success
 */
bool salience_set_thresholds(salience_evaluator_t evaluator, float high_salience_threshold,
                             float high_novelty_threshold, float high_surprise_threshold,
                             float high_urgency_threshold);

/**
 * WHAT: Register callback for attention events
 * WHY: Observer pattern - be notified when interesting inputs arrive
 * HOW: Stores callback function pointer and context
 *
 * @param evaluator Evaluator handle
 * @param callback Callback function
 * @param context User context passed to callback
 * @return true on success
 */
bool salience_register_callback(salience_evaluator_t evaluator, salience_event_callback_fn callback,
                                void* context);

//=============================================================================
// Salience History and State Management
//=============================================================================

/**
 * WHAT: Clear evaluation history
 * WHY: Reset novelty detection after context change
 * HOW: Empties recent input buffer
 *
 * @param evaluator Evaluator handle
 * @return true on success
 *
 * USE CASE: When switching to new conversation or domain
 */
bool salience_clear_history(salience_evaluator_t evaluator);

/**
 * WHAT: Get evaluation statistics
 * WHY: Monitor salience evaluator performance
 * HOW: Returns internal counters and metrics
 *
 * @param evaluator Evaluator handle
 * @param stats Output parameter for statistics
 * @return true on success
 */
typedef struct {
    uint64_t evaluations_performed; /**< Total evaluations */
    uint64_t high_salience_count;   /**< High salience detections */
    uint64_t high_novelty_count;    /**< High novelty detections */
    uint64_t high_surprise_count;   /**< High surprise detections */
    uint64_t high_urgency_count;    /**< High urgency detections */

    float avg_salience; /**< Average overall salience */
    float avg_novelty;  /**< Average novelty */
    float avg_surprise; /**< Average surprise */
    float avg_urgency;  /**< Average urgency */

    float avg_evaluation_time_us; /**< Average time per evaluation (microseconds) */

    uint32_t history_size;   /**< Current history depth */
    uint32_t cache_hit_rate; /**< Cache hits (if enabled) */
} salience_stats_t;

bool salience_get_stats(salience_evaluator_t evaluator, salience_stats_t* stats);

/**
 * WHAT: Reset salience statistics
 * WHY: Allow clearing counters for fresh measurements
 * HOW: Resets all statistics counters to zero
 *
 * @param evaluator Evaluator handle
 * @return true on success, false on error
 */
bool salience_reset_stats(salience_evaluator_t evaluator);

//=============================================================================
// Convenience Functions
//=============================================================================

/**
 * WHAT: Create default salience configuration
 * WHY: Sensible defaults for common use case
 * HOW: Returns pre-filled config struct
 *
 * @return Default configuration
 */
salience_config_t salience_default_config(void);

/**
 * WHAT: Quick salience check (convenience wrapper)
 * WHY: One-shot evaluation without creating evaluator
 * HOW: Creates temporary evaluator, evaluates, destroys
 *
 * PERFORMANCE: Slower than reusing evaluator (evaluator creation overhead)
 * USE CASE: One-off evaluations, prototyping
 *
 * @param brain Brain to evaluate with
 * @param features Input features
 * @param num_features Feature count
 * @return Salience scores
 */
brain_salience_t salience_quick_evaluate(brain_t brain, const float* features,
                                         uint32_t num_features);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * WHAT: Get last salience error message
 * WHY: Thread-safe error reporting
 * HOW: Thread-local storage for error strings
 *
 * @return Error message string (valid until next salience call)
 */
const char* salience_get_last_error(void);

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Boost salience for negative cues
 *
 * WHAT: Increase attention to negative stimuli
 * WHY:  Depression biases attention toward negative information
 * HOW:  Scale negative feature salience by boost factor
 *
 * BIOLOGY: Mood-congruent attentional bias
 *
 * @param evaluator Salience evaluator
 * @param boost_factor Salience boost [0, 1]
 */
void salience_boost_negative_cues(salience_evaluator_t evaluator, float boost_factor);

/**
 * @brief Boost threat detection sensitivity
 *
 * WHAT: Increase urgency for potential threats
 * WHY:  Anxiety increases threat vigilance
 * HOW:  Lower threshold for urgency detection
 *
 * BIOLOGY: Amygdala hyperactivity in anxiety
 *
 * @param evaluator Salience evaluator
 * @param boost_factor Threat sensitivity boost [0, 1]
 */
void salience_boost_threat_detection(salience_evaluator_t evaluator, float boost_factor);

/**
 * @brief Get surprise level from recent evaluation
 *
 * WHAT: Query most recent surprise score
 * WHY:  Emotional system modulates arousal based on surprise
 * HOW:  Return cached surprise value
 *
 * @param evaluator Salience evaluator
 * @return Surprise level [0, 1]
 */
float salience_get_surprise_level(salience_evaluator_t evaluator);

//=============================================================================
// Cross-Modal Salience Fusion Functions
//=============================================================================

/**
 * @brief Register a modality with initial weight
 *
 * WHAT: Enable a sensory modality for salience evaluation
 * WHY:  Multimodal integration requires registering each modality
 * HOW:  Set modality as active with initial fusion weight
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param evaluator Salience evaluator
 * @param modality Modality to register
 * @param weight Initial fusion weight (0-1, will be normalized)
 * @return true on success, false on error
 *
 * USE CASE:
 * @code
 *   // Register visual and audio modalities
 *   salience_register_modality(eval, SALIENCE_MODALITY_VISUAL, 0.6);
 *   salience_register_modality(eval, SALIENCE_MODALITY_AUDIO, 0.4);
 * @endcode
 */
bool salience_register_modality(salience_evaluator_t evaluator, salience_modality_t modality,
                                float weight);

/**
 * @brief Evaluate salience for specific modality
 *
 * WHAT: Compute salience from single sensory modality
 * WHY:  Separate evaluation before cross-modal fusion
 * HOW:  Run salience evaluation, store in modality-specific slot
 *
 * COMPLEXITY: O(1) - same as normal salience evaluation
 * THREAD-SAFE: Yes
 *
 * @param evaluator Salience evaluator
 * @param modality Which modality these features belong to
 * @param features Input feature vector from that modality
 * @param num_features Size of feature vector
 * @return Salience scores for this modality
 *
 * USE CASE:
 * @code
 *   // Evaluate visual salience
 *   brain_salience_t visual_sal = salience_evaluate_modality(
 *       eval, SALIENCE_MODALITY_VISUAL, visual_features, 512);
 *
 *   // Evaluate audio salience
 *   brain_salience_t audio_sal = salience_evaluate_modality(
 *       eval, SALIENCE_MODALITY_AUDIO, audio_features, 128);
 * @endcode
 */
brain_salience_t salience_evaluate_modality(salience_evaluator_t evaluator,
                                            salience_modality_t modality, const float* features,
                                            uint32_t num_features);

/**
 * @brief Combine modality scores into unified salience
 *
 * WHAT: Fuse per-modality saliences into single attention score
 * WHY:  Superior colliculus performs cross-modal integration
 * HOW:  Apply fusion strategy (MAX, WEIGHTED_AVG, LEARNED)
 *
 * BIOLOGICAL BASIS:
 * - Superior colliculus receives visual, auditory, somatosensory maps
 * - Fuses into unified spatial attention map
 * - Parietal cortex performs context-dependent weighting
 *
 * COMPLEXITY: O(m) where m = number of active modalities
 * THREAD-SAFE: Yes
 *
 * @param evaluator Salience evaluator
 * @return Fused salience score combining all active modalities
 *
 * FUSION STRATEGIES:
 * - SALIENCE_FUSION_MAX: salience = max(visual, audio, ...)
 * - SALIENCE_FUSION_WEIGHTED_AVG: salience = sum(weight_i * salience_i)
 * - SALIENCE_FUSION_LEARNED: salience = learned_fusion_tensor(saliences)
 *
 * USE CASE:
 * @code
 *   // Evaluate each modality
 *   salience_evaluate_modality(eval, SALIENCE_MODALITY_VISUAL, vis_feat, 512);
 *   salience_evaluate_modality(eval, SALIENCE_MODALITY_AUDIO, aud_feat, 128);
 *
 *   // Fuse into unified salience
 *   brain_salience_t fused = salience_fuse_modalities(eval);
 *
 *   if (fused.salience > 0.8) {
 *       // High multimodal salience - pay attention!
 *   }
 * @endcode
 */
brain_salience_t salience_fuse_modalities(salience_evaluator_t evaluator);

/**
 * @brief Adjust fusion weight for modality
 *
 * WHAT: Change how much a modality contributes to attention
 * WHY:  Context-dependent attention weighting (e.g., focus on audio in dark)
 * HOW:  Update modality weight, will be normalized with others
 *
 * BIOLOGICAL BASIS:
 * - Attention can bias toward specific modalities
 * - Top-down control from prefrontal cortex
 * - "Listen carefully" increases auditory weight
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param evaluator Salience evaluator
 * @param modality Modality to adjust
 * @param weight New weight (0-1, will be normalized)
 * @return true on success
 *
 * USE CASE:
 * @code
 *   // In dark environment, increase audio weight
 *   if (is_dark) {
 *       salience_set_modality_weight(eval, SALIENCE_MODALITY_AUDIO, 0.8);
 *       salience_set_modality_weight(eval, SALIENCE_MODALITY_VISUAL, 0.2);
 *   }
 * @endcode
 */
bool salience_set_modality_weight(salience_evaluator_t evaluator, salience_modality_t modality,
                                  float weight);

/**
 * @brief Get per-modality salience scores
 *
 * WHAT: Query salience for specific modality
 * WHY:  Inspect which modality is driving attention
 * HOW:  Return cached modality-specific salience
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param evaluator Salience evaluator
 * @param modality Which modality to query
 * @return Salience scores for that modality (zeros if not active)
 *
 * USE CASE:
 * @code
 *   // Check which modality is most salient
 *   brain_salience_t vis = salience_get_modality_salience(eval, SALIENCE_MODALITY_VISUAL);
 *   brain_salience_t aud = salience_get_modality_salience(eval, SALIENCE_MODALITY_AUDIO);
 *
 *   if (vis.salience > aud.salience) {
 *       printf("Visual dominates attention\n");
 *   } else {
 *       printf("Audio dominates attention\n");
 *   }
 * @endcode
 */
brain_salience_t salience_get_modality_salience(salience_evaluator_t evaluator,
                                                salience_modality_t modality);

/**
 * @brief Set fusion strategy
 *
 * WHAT: Change how modalities are combined
 * WHY:  Different strategies for different contexts
 * HOW:  Update strategy enum in evaluator
 *
 * @param evaluator Salience evaluator
 * @param strategy Fusion strategy (MAX, WEIGHTED_AVG, LEARNED)
 * @return true on success
 */
bool salience_set_fusion_strategy(salience_evaluator_t evaluator,
                                  salience_fusion_strategy_t strategy);

/**
 * @brief Get name string for modality
 *
 * WHAT: Convert modality enum to human-readable name
 * WHY:  Logging and debugging
 * HOW:  Return static string
 *
 * @param modality Modality enum
 * @return String name ("visual", "audio", etc.)
 */
const char* salience_modality_name(salience_modality_t modality);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_SALIENCE_H
