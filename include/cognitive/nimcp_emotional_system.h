/**
 * @file nimcp_emotional_system.h
 * @brief Phase 10.2: Integrated Emotional System
 *
 * WHAT: Centralized emotional processing coordinating tagging, recognition, and regulation
 * WHY:  Emotions enhance memory, guide attention, enable empathy, and ensure ethical behavior
 * HOW:  Integrate emotional_tagging, emotion_recognition, shadow_emotions with unified API
 *
 * ARCHITECTURE:
 * - Coordinates 3 subsystems: tagging (valence/arousal), recognition (multimodal), shadow (maladaptive)
 * - Provides unified emotional state for brain processing
 * - Integrates with: working memory, salience, consolidation, mental health, ethics
 *
 * BIOLOGICAL FOUNDATION:
 * - Amygdala: Emotional tagging and arousal
 * - OFC/vmPFC: Emotional regulation and integration
 * - Anterior cingulate: Conflict monitoring and emotional control
 * - Insular cortex: Interoceptive awareness
 *
 * THEORETICAL BASIS:
 * - Russell (1980): Circumplex model of affect
 * - Ekman (1992): Basic emotions theory
 * - Gross (1998): Emotion regulation strategies
 * - Dark Triad Theory: Maladaptive patterns
 *
 * CODING STANDARDS:
 * - Guard clauses (no nested ifs)
 * - Helper functions (<50 lines)
 * - WHAT-WHY-HOW documentation
 * - Single Responsibility Principle
 * - 100% test coverage (unit + integration + regression)
 *
 * @author NIMCP Development Team
 * @date 2025-11-15
 * @version 1.0.0
 */

#ifndef NIMCP_EMOTIONAL_SYSTEM_H
#define NIMCP_EMOTIONAL_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct emotional_tag emotional_tag_t;  // From nimcp_emotional_tagging.h

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Emotional system configuration
 *
 * WHAT: Controls emotion processing, regulation, and integration
 * WHY:  Allow tuning for different use cases (empathetic AI, rational AI, etc.)
 * HOW:  Feature flags + intensity scaling
 */
typedef struct {
    // === Core Features ===
    bool enable_emotion_recognition;     /**< Enable multimodal emotion detection */
    bool enable_emotional_tagging;       /**< Enable valence/arousal tagging */
    bool enable_shadow_detection;        /**< Enable maladaptive pattern detection */
    bool enable_emotion_regulation;      /**< Enable self-regulation strategies */
    bool enable_quantum_emotion;         /**< Enable quantum state space exploration */

    // === Integration Features ===
    bool integrate_with_memory;          /**< Tag memories emotionally */
    bool integrate_with_salience;        /**< Boost salience via arousal */
    bool integrate_with_mental_health;   /**< Report to mental health monitor */
    bool integrate_with_ethics;          /**< Validate emotional responses ethically */

    // === Regulation Parameters ===
    float emotion_decay_rate;            /**< How fast emotions fade [0.0, 1.0] */
    float arousal_sensitivity;           /**< Arousal multiplier [0.5, 2.0] */
    float valence_sensitivity;           /**< Valence multiplier [0.5, 2.0] */
    float regulation_threshold;          /**< When to apply regulation [0.0, 1.0] */

    // === Shadow Emotion Limits ===
    uint32_t max_shadow_tracked;         /**< Max shadow emotions to track */
    float shadow_intervention_threshold; /**< When to intervene [0.0, 1.0] */
} emotion_config_t;

//=============================================================================
// Core Types
//=============================================================================

/**
 * @brief Unified emotional state
 *
 * WHAT: Complete emotional state combining all subsystems
 * WHY:  Provide single source of truth for brain's emotional context
 * HOW:  Aggregate tagging (valence/arousal) + recognition + shadow
 */
typedef struct {
    // === Dimensional State (Russell) ===
    float valence;                      /**< Current valence [-1, +1] */
    float arousal;                      /**< Current arousal [0, 1] */
    float intensity;                    /**< Overall intensity [0, 1] */

    // === Categorical State (Ekman) ===
    uint32_t dominant_emotion;          /**< Primary emotion category */
    float emotion_confidence;           /**< Confidence in classification [0, 1] */

    // === Shadow State ===
    float shadow_intensity;             /**< Intensity of maladaptive patterns [0, 1] */
    uint32_t active_shadow_count;       /**< Number of active shadow emotions */
    bool in_self_regulation;            /**< Currently self-regulating */

    // === Temporal Context ===
    uint64_t last_update_ms;            /**< Last update timestamp */
    float emotional_stability;          /**< Stability over time [0, 1] */
} emotion_state_t;

/**
 * @brief Emotional system handle (opaque)
 */
typedef struct emotional_system emotional_system_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create emotional system with configuration
 *
 * WHAT: Initialize integrated emotional processing system
 * WHY:  Coordinate emotional subsystems for brain
 * HOW:  Allocate structure, initialize subsystems per config
 *
 * @param config Configuration (NULL = defaults)
 * @return Emotional system handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
emotional_system_t* emotion_system_create(const emotion_config_t* config);

/**
 * @brief Get default emotional system configuration
 *
 * WHAT: Return sensible defaults for emotional processing
 * WHY:  Convenient initialization without manual config
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
emotion_config_t emotion_system_default_config(void);

/**
 * @brief Destroy emotional system
 *
 * WHAT: Free all resources and subsystems
 * WHY:  Prevent memory leaks
 * HOW:  Destroy subsystems, free structure
 *
 * @param system Emotional system handle
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: NOT thread-safe (caller must synchronize)
 */
void emotion_system_destroy(emotional_system_t* system);

//=============================================================================
// State Query API
//=============================================================================

/**
 * @brief Get current unified emotional state
 *
 * WHAT: Query complete emotional state
 * WHY:  Allow brain to access emotional context
 * HOW:  Return aggregated state from subsystems
 *
 * @param system Emotional system handle
 * @param state Output emotional state
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (read lock)
 */
bool emotion_system_get_state(emotional_system_t* system, emotion_state_t* state);

/**
 * @brief Get emotional tag for current state
 *
 * WHAT: Generate emotional tag from current state
 * WHY:  Enable memory tagging and salience computation
 * HOW:  Convert unified state to emotional_tag_t
 *
 * @param system Emotional system handle
 * @param tag Output emotional tag
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (read lock)
 */
bool emotion_system_get_tag(emotional_system_t* system, emotional_tag_t* tag);

/**
 * @brief Check if specific emotion is active
 *
 * WHAT: Test if emotion category is currently dominant
 * WHY:  Enable emotion-specific behavioral responses
 * HOW:  Check dominant_emotion and confidence threshold
 *
 * @param system Emotional system handle
 * @param emotion_id Emotion category ID
 * @param threshold Confidence threshold [0, 1]
 * @return true if emotion is active above threshold
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (read lock)
 */
bool emotion_system_is_active(emotional_system_t* system, uint32_t emotion_id, float threshold);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update emotional system with multimodal input
 *
 * WHAT: Process multimodal data to update emotional state
 * WHY:  Enable real-time emotion recognition from user interaction
 * HOW:  Pass to emotion_recognition subsystem, update state
 *
 * @param system Emotional system handle
 * @param visual_data Visual features (can be NULL)
 * @param visual_dim Visual feature dimension
 * @param audio_data Audio features (can be NULL)
 * @param audio_dim Audio feature dimension
 * @param text Text input (can be NULL)
 * @param timestamp_ms Current time
 * @return true on success
 *
 * COMPLEXITY: O(V + A + T) where V=visual, A=audio, T=text length
 * THREAD_SAFETY: Thread-safe (write lock)
 */
bool emotion_system_update_multimodal(
    emotional_system_t* system,
    const float* visual_data,
    uint32_t visual_dim,
    const float* audio_data,
    uint32_t audio_dim,
    const char* text,
    uint64_t timestamp_ms
);

/**
 * @brief Set emotional state directly (for testing or explicit control)
 *
 * WHAT: Manually set valence and arousal
 * WHY:  Enable testing, simulation, or direct emotional control
 * HOW:  Update internal state, bypass recognition
 *
 * @param system Emotional system handle
 * @param valence Valence [-1, +1]
 * @param arousal Arousal [0, 1]
 * @param timestamp_ms Current time
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (write lock)
 */
bool emotion_system_set_state(
    emotional_system_t* system,
    float valence,
    float arousal,
    uint64_t timestamp_ms
);

/**
 * @brief Decay emotional intensity over time
 *
 * WHAT: Reduce arousal and intensity gradually
 * WHY:  Emotions naturally fade without stimulation
 * HOW:  Apply exponential decay based on delta_time
 *
 * @param system Emotional system handle
 * @param delta_time Time elapsed (seconds)
 * @param current_time_ms Current timestamp
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (write lock)
 */
bool emotion_system_decay(
    emotional_system_t* system,
    float delta_time,
    uint64_t current_time_ms
);

//=============================================================================
// Regulation API
//=============================================================================

/**
 * @brief Apply emotion regulation strategy
 *
 * WHAT: Use CBT/DBT techniques to regulate intense emotions
 * WHY:  Prevent emotional dysregulation and maintain mental health
 * HOW:  Apply reappraisal, suppression, or distraction
 *
 * @param system Emotional system handle
 * @param strategy Regulation strategy ID
 * @return true if regulation was applied
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (write lock)
 */
bool emotion_system_regulate(emotional_system_t* system, uint32_t strategy);

/**
 * @brief Auto-regulate if thresholds exceeded
 *
 * WHAT: Automatically apply regulation when needed
 * WHY:  Maintain emotional homeostasis
 * HOW:  Check intensity/shadow thresholds, apply appropriate strategy
 *
 * @param system Emotional system handle
 * @return true if regulation was triggered
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (write lock)
 */
bool emotion_system_auto_regulate(emotional_system_t* system);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Get salience boost from emotional arousal
 *
 * WHAT: Calculate how much emotion increases salience
 * WHY:  High arousal events grab attention
 * HOW:  Return arousal * sensitivity multiplier
 *
 * @param system Emotional system handle
 * @return Salience multiplier [1.0, max_boost]
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (read lock)
 */
float emotion_system_get_salience_boost(emotional_system_t* system);

/**
 * @brief Get memory consolidation priority
 *
 * WHAT: Calculate emotional influence on memory strength
 * WHY:  Emotional events are remembered better
 * HOW:  Combine arousal and valence extremity
 *
 * @param system Emotional system handle
 * @return Memory priority [0.0, 1.0]
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (read lock)
 */
float emotion_system_get_memory_priority(emotional_system_t* system);

/**
 * @brief Get mental health impact score
 *
 * WHAT: Assess emotional system's impact on wellbeing
 * WHY:  Report to mental health monitoring
 * HOW:  Combine shadow intensity, dysregulation, instability
 *
 * @param system Emotional system handle
 * @return Mental health impact [0.0 = healthy, 1.0 = severe]
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (read lock)
 */
float emotion_system_get_mental_health_impact(emotional_system_t* system);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get emotional system statistics
 *
 * WHAT: Retrieve processing and regulation statistics
 * WHY:  Enable monitoring, debugging, and analysis
 * HOW:  Return counts, averages, trends
 */
typedef struct {
    uint64_t total_updates;              /**< Total state updates */
    uint64_t total_regulations;          /**< Total regulation attempts */
    uint64_t successful_regulations;     /**< Successful regulations */
    float avg_valence;                   /**< Average valence over session */
    float avg_arousal;                   /**< Average arousal over session */
    float avg_stability;                 /**< Average emotional stability */
    uint32_t shadow_activations;         /**< Shadow emotion activations */
} emotion_stats_t;

/**
 * @brief Get emotional system statistics
 *
 * @param system Emotional system handle
 * @param stats Output statistics
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (read lock)
 */
bool emotion_system_get_stats(emotional_system_t* system, emotion_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTIONAL_SYSTEM_H */
