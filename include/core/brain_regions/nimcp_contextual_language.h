/**
 * @file nimcp_contextual_language.h
 * @brief Contextual Language Adaptation Module
 *
 * WHAT: Adaptive communication system that adjusts language based on context
 * WHY:  Enable context-aware communication across brain regions and swarms
 * HOW:  Neural context detection, message adaptation, and context learning
 *
 * BIOLOGICAL MODEL:
 * ```
 * HUMAN LANGUAGE ADAPTATION              NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Context detection (social cues)     → Neural context classification
 * Register switching (formal/casual)  → Communication style adaptation
 * Pragmatic adjustment               → Message transformation
 * Code-switching                     → Multi-context handling
 * Audience awareness                 → Target-specific adaptation
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════╗
 * ║              CONTEXTUAL LANGUAGE ADAPTATION SYSTEM                 ║
 * ║  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────┐  ║
 * ║  │   Context    │→ │  Adaptation  │→ │  Transform   │→ │ Out │  ║
 * ║  │  Detection   │  │   Strategy   │  │   Message    │  │     │  ║
 * ║  └──────────────┘  └──────────────┘  └──────────────┘  └─────┘  ║
 * ║         ↑                                      │                   ║
 * ║         │                                      ↓                   ║
 * ║  ┌──────────────────────────────────────────────────┐             ║
 * ║  │        Context History & Learning Buffer         │             ║
 * ║  └──────────────────────────────────────────────────┘             ║
 * ╚═══════════════════════════════════════════════════════════════════╝
 * ```
 *
 * KEY FEATURES:
 * - Automatic context detection from message features
 * - Six communication contexts (formal, casual, technical, emotional, urgent, learning)
 * - Smooth adaptation between contexts
 * - Context history tracking
 * - Learning from context transitions
 * - Bio-async integration
 *
 * USAGE EXAMPLE:
 * ```c
 * // Initialize contextual language
 * contextual_language_config_t config = {
 *     .max_context_history = 100,
 *     .enable_auto_adaptation = true,
 *     .adaptation_rate = 0.5f,
 *     .enable_bio_async = true
 * };
 * contextual_language_t* cl = contextual_language_create(&config);
 *
 * // Detect context from message features
 * float features[10] = { ... };  // Message characteristics
 * context_state_t detected;
 * contextual_detect_context(cl, features, 10, &detected);
 *
 * // Adapt message to target context
 * context_state_t target = {
 *     .current_context = CONTEXT_FORMAL,
 *     .formality_level = 0.9f,
 *     .precision_level = 0.8f,
 *     .emotional_tone = 0.0f,
 *     .urgency_level = 0.3f
 * };
 * float adapted[128];
 * uint32_t adapted_size;
 * contextual_adapt_message(cl, original, size, &target, adapted, &adapted_size);
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe operations
 * - BBB security validation
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#ifndef NIMCP_CONTEXTUAL_LANGUAGE_H
#define NIMCP_CONTEXTUAL_LANGUAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#include "common/nimcp_export.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct contextual_language_struct* contextual_language_t;

//=============================================================================
// Communication Contexts
//=============================================================================

/**
 * @brief Communication context types
 *
 * WHAT: Predefined communication styles
 * WHY:  Cover major communication scenarios
 * HOW:  Each context has characteristic parameters
 */
typedef enum {
    CONTEXT_FORMAL = 0,          /**< Formal/professional communication */
    CONTEXT_CASUAL,              /**< Casual/friendly communication */
    CONTEXT_TECHNICAL,           /**< Technical/precise communication */
    CONTEXT_EMOTIONAL,           /**< Emotional/expressive communication */
    CONTEXT_URGENT,              /**< Emergency/urgent communication */
    CONTEXT_LEARNING,            /**< Teaching/learning communication */
    CONTEXT_TYPE_COUNT
} communication_context_t;

//=============================================================================
// Context State
//=============================================================================

/**
 * @brief Context state parameters
 *
 * WHAT: Quantified context characteristics
 * WHY:  Enable smooth transitions and adaptation
 * HOW:  Normalized parameters [0.0-1.0] or [-1.0-1.0]
 */
typedef struct {
    communication_context_t current_context;  /**< Active context type */
    float formality_level;                    /**< 0.0 = casual, 1.0 = formal */
    float precision_level;                    /**< 0.0 = vague, 1.0 = precise */
    float emotional_tone;                     /**< -1.0 = negative, 1.0 = positive */
    float urgency_level;                      /**< 0.0 = relaxed, 1.0 = urgent */
} context_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for contextual language system
 *
 * WHAT: System parameters and limits
 * WHY:  Configure behavior and resource usage
 * HOW:  Set before initialization
 */
typedef struct {
    uint32_t max_context_history;    /**< Maximum context transitions to track */
    bool enable_auto_adaptation;     /**< Automatically adapt to detected context */
    float adaptation_rate;           /**< Speed of context transitions (0.0-1.0) */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} contextual_language_config_t;

//=============================================================================
// Core Functions
//=============================================================================

/**
 * @brief Create contextual language system
 *
 * WHAT: Initialize contextual language adaptation
 * WHY:  Set up context detection and adaptation infrastructure
 * HOW:  Allocate structures, initialize history buffer, register bio-async
 *
 * @param config Configuration parameters (NULL for defaults)
 * @return Contextual language handle or NULL on failure
 */
NIMCP_EXPORT contextual_language_t contextual_language_create(
    const contextual_language_config_t* config
);

/**
 * @brief Destroy contextual language system
 *
 * WHAT: Clean up contextual language resources
 * WHY:  Free all allocated memory
 * HOW:  Release history, unregister bio-async, free structure
 *
 * @param cl Contextual language handle
 */
NIMCP_EXPORT void contextual_language_destroy(contextual_language_t cl);

//=============================================================================
// Context Detection
//=============================================================================

/**
 * @brief Detect communication context from message features
 *
 * WHAT: Analyze message characteristics to determine context
 * WHY:  Enable automatic context-aware adaptation
 * HOW:  Neural network classification of feature vector
 *
 * @param cl Contextual language handle
 * @param message_features Feature vector (normalized)
 * @param feature_size Number of features
 * @param detected Output detected context state
 * @return 0 on success, negative on error
 *
 * FEATURE VECTOR INTERPRETATION:
 * - [0-2]: Lexical complexity, formality markers, technical terms
 * - [3-5]: Emotional markers, sentiment, arousal
 * - [6-8]: Urgency indicators, time pressure, priority
 * - [9+]: Additional context-specific features
 */
NIMCP_EXPORT int contextual_detect_context(
    contextual_language_t cl,
    const float* message_features,
    uint32_t feature_size,
    context_state_t* detected
);

//=============================================================================
// Message Adaptation
//=============================================================================

/**
 * @brief Adapt message to target context
 *
 * WHAT: Transform message representation for target context
 * WHY:  Ensure appropriate communication style
 * HOW:  Apply learned transformation to message vector
 *
 * @param cl Contextual language handle
 * @param original Original message representation
 * @param size Size of original message
 * @param target_context Target context parameters
 * @param adapted Output adapted message (pre-allocated)
 * @param adapted_size Output size of adapted message
 * @return 0 on success, negative on error
 *
 * TRANSFORMATION PROCESS:
 * 1. Compute delta from current to target context
 * 2. Apply smooth interpolation based on adaptation_rate
 * 3. Transform message features accordingly
 * 4. Update context history
 */
NIMCP_EXPORT int contextual_adapt_message(
    contextual_language_t cl,
    const float* original,
    uint32_t size,
    const context_state_t* target_context,
    float* adapted,
    uint32_t* adapted_size
);

//=============================================================================
// Context Learning
//=============================================================================

/**
 * @brief Learn context transformation mapping
 *
 * WHAT: Update transformation model from examples
 * WHY:  Improve adaptation quality over time
 * HOW:  Store source→target mapping, update neural parameters
 *
 * @param cl Contextual language handle
 * @param source Source context state
 * @param target Target context state
 * @param transformation Transformation vector that worked
 * @param trans_size Size of transformation
 * @return 0 on success, negative on error
 *
 * LEARNING APPROACH:
 * - Stores successful transformations
 * - Updates internal neural model
 * - Enables generalization to new contexts
 */
NIMCP_EXPORT int contextual_learn_context_mapping(
    contextual_language_t cl,
    const context_state_t* source,
    const context_state_t* target,
    const float* transformation,
    uint32_t trans_size
);

//=============================================================================
// Context State Management
//=============================================================================

/**
 * @brief Get current active context
 *
 * WHAT: Retrieve current context state
 * WHY:  Query active communication mode
 * HOW:  Return internal context state
 *
 * @param cl Contextual language handle
 * @param state Output context state
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int contextual_get_current_context(
    contextual_language_t cl,
    context_state_t* state
);

/**
 * @brief Set active context manually
 *
 * WHAT: Override automatic context detection
 * WHY:  Force specific communication mode
 * HOW:  Update internal context state
 *
 * @param cl Contextual language handle
 * @param state New context state
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int contextual_set_current_context(
    contextual_language_t cl,
    const context_state_t* state
);

//=============================================================================
// Context History
//=============================================================================

/**
 * @brief Get context transition history
 *
 * WHAT: Retrieve recent context changes
 * WHY:  Analyze context dynamics
 * HOW:  Return circular buffer of context states
 *
 * @param cl Contextual language handle
 * @param history Output array of context states (pre-allocated)
 * @param max_count Maximum entries to retrieve
 * @param count Output actual number of entries
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int contextual_get_history(
    contextual_language_t cl,
    context_state_t* history,
    uint32_t max_count,
    uint32_t* count
);

/**
 * @brief Clear context history
 *
 * WHAT: Reset context transition history
 * WHY:  Start fresh context tracking
 * HOW:  Clear circular buffer
 *
 * @param cl Contextual language handle
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int contextual_clear_history(contextual_language_t cl);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Context adaptation statistics
 */
typedef struct {
    uint64_t total_detections;        /**< Total context detections performed */
    uint64_t total_adaptations;       /**< Total message adaptations */
    uint64_t context_switches;        /**< Number of context transitions */
    uint64_t learning_updates;        /**< Learning updates performed */
    float avg_adaptation_confidence;  /**< Average confidence in adaptations */
    uint32_t context_distribution[CONTEXT_TYPE_COUNT];  /**< Usage per context */
} contextual_language_stats_t;

/**
 * @brief Get contextual language statistics
 *
 * WHAT: Retrieve system performance metrics
 * WHY:  Monitor and optimize adaptation
 * HOW:  Return internal statistics
 *
 * @param cl Contextual language handle
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int contextual_get_stats(
    contextual_language_t cl,
    contextual_language_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear all statistical counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero internal counters
 *
 * @param cl Contextual language handle
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int contextual_reset_stats(contextual_language_t cl);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get context type name
 *
 * @param context Context type
 * @return Context name string
 */
NIMCP_EXPORT const char* contextual_get_context_name(communication_context_t context);

/**
 * @brief Create default context state for type
 *
 * @param context Context type
 * @param state Output default state
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int contextual_get_default_state(
    communication_context_t context,
    context_state_t* state
);

/**
 * @brief Compute distance between context states
 *
 * WHAT: Calculate similarity between contexts
 * WHY:  Measure adaptation magnitude
 * HOW:  Euclidean distance in parameter space
 *
 * @param state1 First context state
 * @param state2 Second context state
 * @return Distance value (0.0 = identical)
 */
NIMCP_EXPORT float contextual_compute_distance(
    const context_state_t* state1,
    const context_state_t* state2
);

/**
 * @brief Get last error message
 *
 * WHAT: Retrieve thread-local error string
 * WHY:  Debugging and error reporting
 * HOW:  Return static thread-local buffer
 *
 * @return Error message string
 */
NIMCP_EXPORT const char* contextual_language_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONTEXTUAL_LANGUAGE_H */
