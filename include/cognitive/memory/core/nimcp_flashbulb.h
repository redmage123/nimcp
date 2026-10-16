//=============================================================================
// nimcp_flashbulb.h - Flashbulb Memory System for Enhanced Emotional Encoding
//=============================================================================
/**
 * @file nimcp_flashbulb.h
 * @brief Enhanced encoding of emotionally significant events via flashbulb memories
 *
 * WHAT: Flashbulb memory system for vivid, emotionally-charged autobiographical events
 * WHY:  Highly emotional events create exceptionally vivid memories with enhanced
 *       contextual details, requiring special encoding and trauma handling
 * HOW:  Arousal-modulated encoding strength, amygdala integration for emotional
 *       tagging, and specialized trauma processing pathways
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Flashbulb Memory Model (Brown & Kulik, 1977):
 *   +-----------------------------------------------------------------------+
 *   |  Flashbulb memories are vivid, highly detailed memories of the        |
 *   |  circumstances surrounding emotionally significant events:            |
 *   |                                                                       |
 *   |  Key Characteristics:                                                 |
 *   |  - Exceptional vividness and subjective certainty                     |
 *   |  - Rich contextual details (location, informant, ongoing activity)   |
 *   |  - Strong emotional content (arousal + valence)                       |
 *   |  - Surprising or personally significant events                        |
 *   |                                                                       |
 *   |  Examples: 9/11, JFK assassination, personal trauma/triumph           |
 *   +-----------------------------------------------------------------------+
 *
 *   Amygdala-Mediated Memory Enhancement:
 *   +-----------------------------------------------------------------------+
 *   |  Neural Mechanism:                                                    |
 *   |                                                                       |
 *   |  1. EMOTIONAL EVENT detected by sensory cortices                     |
 *   |  2. AMYGDALA activation (rapid, subcortical pathway)                  |
 *   |  3. NOREPINEPHRINE release from locus coeruleus                       |
 *   |  4. HIPPOCAMPAL potentiation via beta-adrenergic receptors           |
 *   |  5. ENHANCED CONSOLIDATION of emotional memory                        |
 *   |                                                                       |
 *   |  Formula: Encoding_Strength = Base * (1 + Arousal_Boost * Arousal)   |
 *   |           * (1 + Surprise_Boost * Surprise) * Significance            |
 *   +-----------------------------------------------------------------------+
 *
 *   Flashbulb Memory Attributes (Conway et al., 1994):
 *   +-----------------------------------------------------------------------+
 *   |  Canonical Attributes captured:                                       |
 *   |  - LOCATION: Where were you?                                          |
 *   |  - INFORMANT: Who told you? / How did you learn?                     |
 *   |  - ACTIVITY: What were you doing?                                     |
 *   |  - OTHERS: Who else was there?                                        |
 *   |  - AFTERMATH: What happened next?                                     |
 *   |  - EMOTIONAL: How did you feel?                                       |
 *   +-----------------------------------------------------------------------+
 *
 *   Reconsolidation and Accuracy:
 *   +-----------------------------------------------------------------------+
 *   |  CRITICAL: Flashbulb memories are VIVID but not necessarily ACCURATE |
 *   |                                                                       |
 *   |  - High subjective confidence != high accuracy                        |
 *   |  - Memories change during reconsolidation                             |
 *   |  - Confidence remains high even as accuracy degrades                  |
 *   |  - This system tracks both vividness AND verifiable accuracy         |
 *   +-----------------------------------------------------------------------+
 *
 *   Trauma Memory Handling:
 *   +-----------------------------------------------------------------------+
 *   |  Traumatic memories require special handling:                         |
 *   |                                                                       |
 *   |  - INTRUSIONS: Unwanted spontaneous recall                           |
 *   |  - AVOIDANCE: Active suppression attempts                            |
 *   |  - FRAGMENTATION: Sensory fragments without narrative coherence      |
 *   |  - OVERCONSOLIDATION: Excessively strong encoding                    |
 *   |                                                                       |
 *   |  Therapeutic Interventions Modeled:                                   |
 *   |  - Reconsolidation therapy (EMDR-like)                               |
 *   |  - Exposure therapy (controlled re-experiencing)                     |
 *   |  - Propranolol-like emotional dampening                              |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Event detection: ~50ns (threshold comparison)
 * - Flashbulb encoding: ~500ns (enhanced encoding + context capture)
 * - Retrieval: ~200ns (vivid memory lookup)
 * - Trauma processing: ~1us (specialized handling)
 *
 * MEMORY:
 * - flashbulb_memory_t: ~512 bytes per flashbulb memory
 * - flashbulb_system_t: ~1KB base + array of memories
 *
 * INTEGRATION:
 * - PR Memory Node: Each flashbulb memory wraps a pr_memory_node_t
 * - Entanglement Graph: Enhanced linking for contextual details
 * - Resonance Engine: Emotion-weighted retrieval
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_FLASHBULB_H
#define NIMCP_FLASHBULB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "nimcp_quaternion.h"
#include "nimcp_prime_signature.h"
#include "nimcp_pr_memory_node.h"
#include "nimcp_entanglement.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default arousal threshold for flashbulb detection */
#define FLASHBULB_DEFAULT_AROUSAL_THRESHOLD     0.7f

/** Default arousal boost factor for encoding strength */
#define FLASHBULB_DEFAULT_AROUSAL_BOOST         1.5f

/** Default surprise boost factor for encoding strength */
#define FLASHBULB_DEFAULT_SURPRISE_BOOST        1.2f

/** Maximum flashbulb memories per system */
#define FLASHBULB_MAX_MEMORIES                  4096

/** Maximum trauma memories tracked separately */
#define FLASHBULB_MAX_TRAUMA                    256

/** Intrusion frequency threshold for trauma classification */
#define FLASHBULB_TRAUMA_INTRUSION_THRESHOLD    0.5f

/** Minimum vividness for flashbulb classification */
#define FLASHBULB_MIN_VIVIDNESS                 0.6f

/** Reconsolidation window duration (simulated milliseconds) */
#define FLASHBULB_RECONSOLIDATION_WINDOW_MS     21600000  // 6 hours

/** Therapy effectiveness decay rate per session */
#define FLASHBULB_THERAPY_DECAY_RATE            0.85f

/** Epsilon for floating-point comparisons */
#define FLASHBULB_EPSILON                       1e-6f

/** Maximum context signatures per flashbulb memory */
#define FLASHBULB_MAX_CONTEXT_SIGNATURES        8

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Flashbulb memory type classification
 *
 * WHAT: Categorical classification of flashbulb memory emotional valence
 * WHY:  Different types require different processing (especially trauma)
 * HOW:  Based on emotional valence and intensity
 */
typedef enum {
    FLASHBULB_POSITIVE = 0,     /**< Highly positive event (wedding, birth, graduation) */
    FLASHBULB_NEGATIVE,         /**< Highly negative event (death, accident, loss) */
    FLASHBULB_SURPRISING,       /**< Unexpected shocking event (surprising news) */
    FLASHBULB_TRAUMATIC,        /**< Traumatic event requiring special handling */
    FLASHBULB_TYPE_COUNT        /**< Number of types (for array sizing) */
} flashbulb_type_t;

/**
 * @brief Flashbulb system error codes
 */
typedef enum {
    FLASHBULB_SUCCESS = 0,              /**< Operation succeeded */
    FLASHBULB_ERROR_NULL_POINTER = -1,  /**< NULL pointer argument */
    FLASHBULB_ERROR_INVALID_CONFIG = -2,/**< Invalid configuration */
    FLASHBULB_ERROR_NO_MEMORY = -3,     /**< Memory allocation failed */
    FLASHBULB_ERROR_NOT_FOUND = -4,     /**< Flashbulb memory not found */
    FLASHBULB_ERROR_CAPACITY = -5,      /**< Maximum capacity reached */
    FLASHBULB_ERROR_INVALID_STATE = -6, /**< Invalid state for operation */
    FLASHBULB_ERROR_BELOW_THRESHOLD = -7,/**< Event below arousal threshold */
    FLASHBULB_ERROR_RECONSOLIDATING = -8 /**< Memory is reconsolidating */
} flashbulb_error_t;

/**
 * @brief Context type for flashbulb memory attributes
 *
 * WHAT: The canonical flashbulb memory context attributes
 * WHY:  These specific details are characteristically remembered
 * HOW:  Each maps to a prime signature for content-addressable linking
 */
typedef enum {
    FLASHBULB_CONTEXT_LOCATION = 0,    /**< Where were you? */
    FLASHBULB_CONTEXT_INFORMANT,       /**< Who told you? / How did you learn? */
    FLASHBULB_CONTEXT_ACTIVITY,        /**< What were you doing? */
    FLASHBULB_CONTEXT_OTHERS,          /**< Who else was there? */
    FLASHBULB_CONTEXT_AFTERMATH,       /**< What happened next? */
    FLASHBULB_CONTEXT_EMOTIONAL,       /**< How did you feel? */
    FLASHBULB_CONTEXT_SENSORY,         /**< Salient sensory details */
    FLASHBULB_CONTEXT_TEMPORAL,        /**< When exactly? */
    FLASHBULB_CONTEXT_COUNT            /**< Number of context types */
} flashbulb_context_type_t;

/**
 * @brief Emotional intensity parameters
 *
 * WHAT: Multi-dimensional emotional state during flashbulb encoding
 * WHY:  Arousal modulates encoding strength, valence affects type classification
 * HOW:  Combines physiological arousal, emotional valence, surprise, and significance
 *
 * Memory layout: 20 bytes
 */
typedef struct {
    float arousal;               /**< Physiological arousal level [0-1] */
    float valence;               /**< Emotional valence [-1 to +1] (negative to positive) */
    float surprise;              /**< How unexpected the event was [0-1] */
    float personal_significance; /**< How personally relevant [0-1] */
    float rehearsal_count;       /**< How often thought about (cumulative) */
} emotional_intensity_t;

/**
 * @brief Context detail for flashbulb memory
 *
 * WHAT: A single contextual attribute with its signature
 * WHY:  Links specific context (location, informant, etc.) to flashbulb
 * HOW:  Stores signature and vividness for each context type
 */
typedef struct {
    flashbulb_context_type_t type;   /**< What kind of context this is */
    prime_signature_t signature;      /**< Content signature of context detail */
    float vividness;                  /**< How vividly this detail is remembered [0-1] */
    float confidence;                 /**< Confidence in this detail's accuracy [0-1] */
    bool is_verified;                 /**< Whether accuracy has been verified */
    float actual_accuracy;            /**< If verified, what was actual accuracy [0-1] */
} flashbulb_context_t;

/**
 * @brief Flashbulb memory structure
 *
 * WHAT: Complete flashbulb memory with event, context, and trauma state
 * WHY:  Encapsulates all aspects of a flashbulb memory for the system
 * HOW:  Wraps PR memory node with emotional intensity and contextual details
 *
 * Memory layout: ~512 bytes
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t flashbulb_id;           /**< Unique flashbulb memory identifier */
    flashbulb_type_t type;           /**< Flashbulb type classification */

    //-------------------------------------------------------------------------
    // Core Memory Reference
    //-------------------------------------------------------------------------
    pr_memory_node_t* memory;        /**< Underlying PR memory node */
    prime_signature_t event_signature;/**< Content signature of the event */
    nimcp_quaternion_t event_quaternion;/**< Semantic state during encoding */

    //-------------------------------------------------------------------------
    // Emotional State at Encoding
    //-------------------------------------------------------------------------
    emotional_intensity_t intensity; /**< Emotional parameters at encoding */

    //-------------------------------------------------------------------------
    // Contextual Details (canonical flashbulb attributes)
    //-------------------------------------------------------------------------
    flashbulb_context_t contexts[FLASHBULB_CONTEXT_COUNT];
    uint32_t num_contexts;           /**< Number of populated context slots */

    //-------------------------------------------------------------------------
    // Vividness and Accuracy
    //-------------------------------------------------------------------------
    float vividness;                 /**< Subjective vividness [0-1] */
    float confidence;                /**< Confidence in memory accuracy [0-1] */
    float actual_accuracy;           /**< If verifiable, actual accuracy [0-1] */
    bool accuracy_verified;          /**< Whether accuracy has been checked */

    //-------------------------------------------------------------------------
    // Consolidation State
    //-------------------------------------------------------------------------
    float consolidation_strength;    /**< Current consolidation level [0-1] */
    float encoding_boost;            /**< Arousal-based encoding boost applied */
    bool is_reconsolidating;         /**< Currently in reconsolidation window */
    uint64_t last_retrieval_ms;      /**< Timestamp of last retrieval */
    uint64_t reconsolidation_start_ms;/**< When reconsolidation window opened */

    //-------------------------------------------------------------------------
    // Trauma Markers (only relevant for FLASHBULB_TRAUMATIC)
    //-------------------------------------------------------------------------
    bool requires_trauma_handling;   /**< Flag for special trauma processing */
    float intrusion_frequency;       /**< Rate of unwanted spontaneous recall [0-1] */
    float avoidance_level;           /**< Active suppression/avoidance behavior [0-1] */
    float fragmentation;             /**< Degree of narrative fragmentation [0-1] */
    float hyperarousal;              /**< Trauma-related hyperarousal [0-1] */
    uint32_t therapy_sessions;       /**< Number of therapeutic interventions */

    //-------------------------------------------------------------------------
    // Temporal Information
    //-------------------------------------------------------------------------
    uint64_t event_time_ms;          /**< When the event occurred */
    uint64_t encoding_time_ms;       /**< When memory was encoded */
    uint32_t retrieval_count;        /**< Total times retrieved */

} flashbulb_memory_t;

/**
 * @brief Configuration for flashbulb system
 *
 * WHAT: Parameters controlling flashbulb detection and processing
 * WHY:  Different applications need different sensitivity thresholds
 * HOW:  Configurable thresholds, boosts, and capacity limits
 */
typedef struct {
    float arousal_threshold;         /**< Minimum arousal for flashbulb detection [0-1] */
    float arousal_boost;             /**< Encoding boost per unit arousal */
    float surprise_boost;            /**< Encoding boost per unit surprise */
    float significance_weight;       /**< Weight for personal significance */
    float trauma_arousal_threshold;  /**< Arousal threshold for trauma classification */
    float min_vividness_threshold;   /**< Minimum vividness for flashbulb status */
    size_t max_flashbulb_memories;   /**< Maximum flashbulb memories to store */
    size_t max_trauma_memories;      /**< Maximum trauma memories to track */
    bool enable_reconsolidation;     /**< Enable reconsolidation processing */
    bool enable_trauma_handling;     /**< Enable specialized trauma processing */
    uint64_t reconsolidation_window_ms;/**< Duration of reconsolidation window */
} flashbulb_config_t;

/**
 * @brief Flashbulb system handle
 *
 * WHAT: Main flashbulb memory system with amygdala state
 * WHY:  Manages detection, encoding, and retrieval of flashbulb memories
 * HOW:  Integrates with PR memory system and entanglement graph
 */
typedef struct {
    //-------------------------------------------------------------------------
    // PR Integration
    //-------------------------------------------------------------------------
    entangle_graph_t entanglement;   /**< Entanglement graph for context linking */
    pr_node_manager_t node_manager;  /**< Node manager for memory creation */

    //-------------------------------------------------------------------------
    // Amygdala State (emotional processing)
    //-------------------------------------------------------------------------
    float current_arousal;           /**< Current system arousal level [0-1] */
    float baseline_arousal;          /**< Baseline arousal for normalization */
    float arousal_decay_rate;        /**< Rate at which arousal decays */

    //-------------------------------------------------------------------------
    // Flashbulb Memory Storage
    //-------------------------------------------------------------------------
    flashbulb_memory_t* memories;    /**< Array of flashbulb memories */
    size_t num_memories;             /**< Current number of flashbulb memories */
    size_t capacity;                 /**< Allocated capacity */

    //-------------------------------------------------------------------------
    // Trauma Tracking
    //-------------------------------------------------------------------------
    flashbulb_memory_t** trauma_memories;/**< Pointers to trauma memories */
    size_t num_trauma;               /**< Number of trauma memories */
    size_t trauma_capacity;          /**< Allocated trauma capacity */

    //-------------------------------------------------------------------------
    // Configuration and State
    //-------------------------------------------------------------------------
    flashbulb_config_t config;       /**< System configuration */
    uint64_t next_flashbulb_id;      /**< Next available flashbulb ID */
    bool is_initialized;             /**< Initialization flag */

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------
    uint64_t total_detections;       /**< Total flashbulb-worthy events detected */
    uint64_t total_encodings;        /**< Total successful encodings */
    uint64_t total_retrievals;       /**< Total retrieval operations */
    uint64_t total_intrusions;       /**< Total trauma intrusions processed */

} flashbulb_system_t;

/**
 * @brief Event candidate for flashbulb detection
 *
 * WHAT: Input structure for checking if event is flashbulb-worthy
 * WHY:  Encapsulates all information needed for detection
 * HOW:  Contains emotional state, content, and context
 */
typedef struct {
    const void* event_data;          /**< Event content data */
    size_t event_size;               /**< Size of event data */
    emotional_intensity_t intensity; /**< Emotional parameters */
    nimcp_quaternion_t state;        /**< Semantic state during event */
    uint64_t timestamp_ms;           /**< When event occurred */
} flashbulb_event_t;

/**
 * @brief Context capture request
 *
 * WHAT: Request to capture contextual detail for flashbulb memory
 * WHY:  Provides all information needed to encode a context attribute
 */
typedef struct {
    flashbulb_context_type_t type;   /**< What context to capture */
    const void* context_data;        /**< Context content data */
    size_t context_size;             /**< Size of context data */
    float vividness;                 /**< How vivid this detail is */
    float confidence;                /**< Confidence in accuracy */
} flashbulb_context_request_t;

/**
 * @brief Flashbulb retrieval result
 *
 * WHAT: Result of flashbulb memory retrieval
 * WHY:  Contains memory plus retrieval metadata
 */
typedef struct {
    flashbulb_memory_t* memory;      /**< Retrieved flashbulb memory */
    float retrieval_vividness;       /**< Vividness during this retrieval */
    float resonance_score;           /**< Resonance match score */
    bool triggered_reconsolidation;  /**< Whether retrieval opened reconsolidation */
} flashbulb_retrieval_result_t;

/**
 * @brief Therapy intervention parameters
 *
 * WHAT: Parameters for simulating therapeutic interventions
 * WHY:  Model effects of EMDR, exposure therapy, pharmacological dampening
 */
typedef struct {
    float emotional_dampening;       /**< Reduce emotional intensity [0-1] */
    float intrusion_reduction;       /**< Target reduction in intrusions [0-1] */
    float avoidance_reduction;       /**< Target reduction in avoidance [0-1] */
    bool trigger_reconsolidation;    /**< Force reconsolidation window */
    uint32_t session_duration_ms;    /**< Duration of therapy session */
} flashbulb_therapy_params_t;

/**
 * @brief Statistics for flashbulb system
 */
typedef struct {
    size_t total_flashbulbs;         /**< Total flashbulb memories */
    size_t positive_count;           /**< Positive flashbulb count */
    size_t negative_count;           /**< Negative flashbulb count */
    size_t surprising_count;         /**< Surprising flashbulb count */
    size_t traumatic_count;          /**< Traumatic flashbulb count */
    float mean_vividness;            /**< Average vividness */
    float mean_confidence;           /**< Average confidence */
    float mean_accuracy;             /**< Average verified accuracy */
    size_t verified_count;           /**< Number with verified accuracy */
    float mean_intrusion_freq;       /**< Average intrusion frequency (trauma) */
    uint64_t total_therapy_sessions; /**< Total therapeutic interventions */
} flashbulb_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default flashbulb configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 * HOW:  Sets balanced thresholds and boost factors
 *
 * @return Default configuration with:
 *         - arousal_threshold: 0.7
 *         - arousal_boost: 1.5
 *         - surprise_boost: 1.2
 *         - significance_weight: 1.0
 *         - trauma_arousal_threshold: 0.9
 *         - enable_reconsolidation: true
 *         - enable_trauma_handling: true
 *
 * Performance: ~5ns
 *
 * Example:
 *   flashbulb_config_t config = flashbulb_config_default();
 *   config.arousal_threshold = 0.8f;  // More selective detection
 */
NIMCP_EXPORT flashbulb_config_t flashbulb_config_default(void);

/**
 * @brief Validate flashbulb configuration
 *
 * WHAT: Checks configuration values are valid
 * WHY:  Prevent invalid configs causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool flashbulb_config_validate(const flashbulb_config_t* config);

//=============================================================================
// System Lifecycle Functions
//=============================================================================

/**
 * @brief Create a new flashbulb memory system
 *
 * WHAT: Allocates and initializes flashbulb system
 * WHY:  Entry point for flashbulb memory processing
 * HOW:  Creates system with entanglement graph and node manager
 *
 * @param entanglement Entanglement graph for context linking (can be NULL)
 * @param node_manager PR node manager for memory creation
 * @param config System configuration (NULL for defaults)
 * @return New flashbulb system handle, or NULL on failure
 *
 * Performance: O(capacity) for array allocation
 * Memory: ~1KB base + capacity * sizeof(flashbulb_memory_t)
 *
 * Example:
 *   flashbulb_system_t* system = flashbulb_create(
 *       entanglement, node_manager, NULL);
 *   if (!system) {
 *       fprintf(stderr, "Failed to create flashbulb system\n");
 *   }
 */
NIMCP_EXPORT flashbulb_system_t* flashbulb_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const flashbulb_config_t* config
);

/**
 * @brief Destroy flashbulb system and free resources
 *
 * WHAT: Deallocates system and all flashbulb memories
 * WHY:  Resource cleanup
 * HOW:  Frees all memories, arrays, and system structure
 *
 * @param system System to destroy (NULL safe)
 *
 * Performance: O(N) where N = number of memories
 *
 * WARNING: Does NOT destroy the underlying PR memory nodes or
 *          entanglement graph - those must be destroyed separately
 */
NIMCP_EXPORT void flashbulb_destroy(flashbulb_system_t* system);

/**
 * @brief Reset flashbulb system to initial state
 *
 * WHAT: Clears all memories but keeps system allocated
 * WHY:  Reset without reallocation overhead
 *
 * @param system System to reset
 * @return FLASHBULB_SUCCESS or error code
 *
 * Performance: O(N)
 */
NIMCP_EXPORT flashbulb_error_t flashbulb_reset(flashbulb_system_t* system);

//=============================================================================
// Detection and Encoding Functions
//=============================================================================

/**
 * @brief Detect if event is flashbulb-worthy
 *
 * WHAT: Evaluates if event meets flashbulb criteria
 * WHY:  Gate for flashbulb encoding (avoid encoding mundane events)
 * HOW:  Checks arousal, surprise, and significance against thresholds
 *
 * CRITERIA (all must be met):
 * 1. Arousal >= arousal_threshold
 * 2. Arousal * Surprise * Significance >= detection_threshold
 * 3. Event has meaningful content (non-empty)
 *
 * @param system Flashbulb system
 * @param event Event to evaluate
 * @param detected_type Output: type if detected (NULL to skip)
 * @return true if event is flashbulb-worthy, false otherwise
 *
 * Performance: ~50ns
 *
 * Example:
 *   flashbulb_type_t type;
 *   if (flashbulb_detect(system, &event, &type)) {
 *       printf("Flashbulb event detected: %s\n", flashbulb_type_name(type));
 *       flashbulb_encode(system, &event, type);
 *   }
 */
NIMCP_EXPORT bool flashbulb_detect(
    flashbulb_system_t* system,
    const flashbulb_event_t* event,
    flashbulb_type_t* detected_type
);

/**
 * @brief Encode flashbulb memory
 *
 * WHAT: Create flashbulb memory with enhanced encoding
 * WHY:  Store emotionally significant event with arousal-boosted strength
 * HOW:  Creates PR node, computes encoding boost, initializes flashbulb
 *
 * ENCODING STRENGTH FORMULA:
 *   encoding_strength = base_strength
 *                     * (1 + arousal_boost * arousal)
 *                     * (1 + surprise_boost * surprise)
 *                     * personal_significance
 *
 * @param system Flashbulb system
 * @param event Event to encode
 * @param type Flashbulb type classification
 * @return Pointer to created flashbulb memory, or NULL on error
 *
 * Performance: ~500ns
 * Memory: Allocates flashbulb_memory_t in system array
 *
 * SIDE EFFECTS:
 * - Updates system arousal state
 * - If trauma, adds to trauma tracking array
 * - Creates PR memory node
 *
 * Example:
 *   flashbulb_memory_t* fb = flashbulb_encode(system, &event, FLASHBULB_POSITIVE);
 *   if (fb) {
 *       printf("Encoded with strength: %.3f\n", fb->consolidation_strength);
 *   }
 */
NIMCP_EXPORT flashbulb_memory_t* flashbulb_encode(
    flashbulb_system_t* system,
    const flashbulb_event_t* event,
    flashbulb_type_t type
);

/**
 * @brief Encode contextual detail for flashbulb memory
 *
 * WHAT: Captures specific contextual attribute (location, informant, etc.)
 * WHY:  Flashbulb memories include vivid contextual details
 * HOW:  Creates signature for context, links to flashbulb via entanglement
 *
 * @param system Flashbulb system
 * @param flashbulb Target flashbulb memory
 * @param request Context capture request
 * @return FLASHBULB_SUCCESS or error code
 *
 * Performance: ~200ns
 *
 * SIDE EFFECTS:
 * - Updates flashbulb's context array
 * - Creates entanglement link between flashbulb and context
 *
 * Example:
 *   flashbulb_context_request_t ctx = {
 *       .type = FLASHBULB_CONTEXT_LOCATION,
 *       .context_data = "Office building, 3rd floor",
 *       .context_size = strlen("Office building, 3rd floor"),
 *       .vividness = 0.9f,
 *       .confidence = 0.85f
 *   };
 *   flashbulb_encode_context(system, flashbulb, &ctx);
 */
NIMCP_EXPORT flashbulb_error_t flashbulb_encode_context(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    const flashbulb_context_request_t* request
);

//=============================================================================
// Arousal State Functions
//=============================================================================

/**
 * @brief Update current arousal state
 *
 * WHAT: Sets system's current arousal level
 * WHY:  Arousal modulates encoding strength and detection sensitivity
 * HOW:  Updates current_arousal with optional decay from previous
 *
 * @param system Flashbulb system
 * @param arousal New arousal level [0-1]
 * @param apply_decay If true, blend with decayed previous arousal
 * @return Previous arousal value
 *
 * Performance: ~10ns
 *
 * Example:
 *   // Sudden startle response
 *   flashbulb_update_arousal(system, 0.95f, false);
 *
 *   // Gradual arousal with decay
 *   flashbulb_update_arousal(system, 0.7f, true);
 */
NIMCP_EXPORT float flashbulb_update_arousal(
    flashbulb_system_t* system,
    float arousal,
    bool apply_decay
);

/**
 * @brief Compute encoding strength from arousal
 *
 * WHAT: Calculates encoding boost based on current emotional state
 * WHY:  Core formula for arousal-modulated memory enhancement
 * HOW:  Applies arousal, surprise, and significance weights
 *
 * FORMULA:
 *   strength = base * (1 + arousal_boost * arousal)
 *            * (1 + surprise_boost * surprise)
 *            * max(significance, 0.1)
 *
 * @param system Flashbulb system (for config)
 * @param intensity Emotional intensity parameters
 * @param base_strength Starting strength before modulation [0-1]
 * @return Modulated encoding strength [0-2+]
 *
 * Performance: ~15ns
 *
 * Example:
 *   emotional_intensity_t intensity = {
 *       .arousal = 0.9f,
 *       .valence = -0.8f,  // Negative event
 *       .surprise = 0.95f,
 *       .personal_significance = 0.9f
 *   };
 *   float strength = flashbulb_compute_encoding_strength(system, &intensity, 0.5f);
 *   // Result: ~1.5 (3x boost from high arousal + surprise)
 */
NIMCP_EXPORT float flashbulb_compute_encoding_strength(
    const flashbulb_system_t* system,
    const emotional_intensity_t* intensity,
    float base_strength
);

/**
 * @brief Decay system arousal over time
 *
 * WHAT: Applies time-based arousal decay
 * WHY:  Arousal naturally diminishes over time
 * HOW:  Exponential decay toward baseline
 *
 * @param system Flashbulb system
 * @param elapsed_ms Time elapsed in milliseconds
 * @return New arousal level after decay
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT float flashbulb_decay_arousal(
    flashbulb_system_t* system,
    uint64_t elapsed_ms
);

//=============================================================================
// Retrieval Functions
//=============================================================================

/**
 * @brief Retrieve flashbulb memory by ID
 *
 * WHAT: Direct lookup of flashbulb memory
 * WHY:  Fast access when ID is known
 *
 * @param system Flashbulb system
 * @param flashbulb_id Flashbulb memory ID
 * @param result Output retrieval result (NULL to just get memory)
 * @return Pointer to flashbulb memory, or NULL if not found
 *
 * Performance: O(N) linear search (could be optimized with hash)
 *
 * SIDE EFFECTS:
 * - Updates retrieval count and timestamp
 * - May trigger reconsolidation window
 *
 * Example:
 *   flashbulb_retrieval_result_t result;
 *   flashbulb_memory_t* fb = flashbulb_retrieve(system, fb_id, &result);
 *   if (fb && result.triggered_reconsolidation) {
 *       printf("Memory is now reconsolidating\n");
 *   }
 */
NIMCP_EXPORT flashbulb_memory_t* flashbulb_retrieve(
    flashbulb_system_t* system,
    uint64_t flashbulb_id,
    flashbulb_retrieval_result_t* result
);

/**
 * @brief Retrieve flashbulb memory by content similarity
 *
 * WHAT: Find flashbulb memory matching query signature
 * WHY:  Content-addressable retrieval
 * HOW:  Uses resonance scoring on event signatures
 *
 * @param system Flashbulb system
 * @param query Query signature
 * @param min_resonance Minimum resonance score [0-1]
 * @param result Output retrieval result
 * @return Best matching flashbulb, or NULL if none above threshold
 *
 * Performance: O(N) where N = number of flashbulbs
 */
NIMCP_EXPORT flashbulb_memory_t* flashbulb_retrieve_by_content(
    flashbulb_system_t* system,
    const prime_signature_t* query,
    float min_resonance,
    flashbulb_retrieval_result_t* result
);

/**
 * @brief Retrieve flashbulb memories by type
 *
 * WHAT: Get all flashbulb memories of specific type
 * WHY:  Filter by emotional category
 *
 * @param system Flashbulb system
 * @param type Flashbulb type to filter
 * @param memories Output array (caller-allocated)
 * @param max_memories Maximum memories to return
 * @param count Output: actual count returned
 * @return FLASHBULB_SUCCESS or error code
 *
 * Performance: O(N)
 */
NIMCP_EXPORT flashbulb_error_t flashbulb_retrieve_by_type(
    flashbulb_system_t* system,
    flashbulb_type_t type,
    flashbulb_memory_t** memories,
    size_t max_memories,
    size_t* count
);

/**
 * @brief Retrieve most vivid flashbulb memories
 *
 * WHAT: Get top-K flashbulb memories by vividness
 * WHY:  Most vivid memories are most salient
 *
 * @param system Flashbulb system
 * @param k Number of memories to return
 * @param memories Output array (caller-allocated, size >= k)
 * @param count Output: actual count returned (<= k)
 * @return FLASHBULB_SUCCESS or error code
 *
 * Performance: O(N log K) via partial sort
 */
NIMCP_EXPORT flashbulb_error_t flashbulb_retrieve_most_vivid(
    flashbulb_system_t* system,
    size_t k,
    flashbulb_memory_t** memories,
    size_t* count
);

//=============================================================================
// Vividness and Accuracy Functions
//=============================================================================

/**
 * @brief Assess subjective vividness of flashbulb memory
 *
 * WHAT: Compute current vividness based on memory state
 * WHY:  Vividness changes over time and with retrieval
 * HOW:  Factors in consolidation, recency, and retrieval count
 *
 * FORMULA:
 *   vividness = base_vividness
 *             * (1 - decay_factor * time_since_encoding)
 *             * (1 + rehearsal_boost * sqrt(retrieval_count))
 *             * consolidation_strength
 *
 * @param system Flashbulb system
 * @param flashbulb Flashbulb memory to assess
 * @param current_time_ms Current timestamp
 * @return Assessed vividness [0-1]
 *
 * Performance: ~20ns
 *
 * NOTE: This updates the memory's vividness field
 */
NIMCP_EXPORT float flashbulb_assess_vividness(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    uint64_t current_time_ms
);

/**
 * @brief Verify accuracy of flashbulb memory against ground truth
 *
 * WHAT: Compare flashbulb memory against verified facts
 * WHY:  Flashbulb memories are vivid but not always accurate
 * HOW:  Computes similarity between memory and ground truth signatures
 *
 * @param system Flashbulb system
 * @param flashbulb Flashbulb memory to verify
 * @param ground_truth_sig Signature of verified facts
 * @return Accuracy score [0-1]
 *
 * Performance: ~30ns
 *
 * SIDE EFFECTS:
 * - Sets accuracy_verified flag
 * - Updates actual_accuracy field
 *
 * Example:
 *   prime_signature_t* truth = prime_sig_from_content(verified_data, size);
 *   float accuracy = flashbulb_verify_accuracy(system, flashbulb, truth);
 *   printf("Memory is %.0f%% accurate\n", accuracy * 100);
 *   // Note: confidence may be 90% but accuracy only 60%
 */
NIMCP_EXPORT float flashbulb_verify_accuracy(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    const prime_signature_t* ground_truth_sig
);

/**
 * @brief Check for confidence-accuracy discrepancy
 *
 * WHAT: Compare subjective confidence with actual accuracy
 * WHY:  Classic flashbulb memory finding: high confidence != high accuracy
 * HOW:  Returns difference between confidence and verified accuracy
 *
 * @param flashbulb Flashbulb memory (must have verified accuracy)
 * @return Discrepancy (positive = overconfident, negative = underconfident)
 *
 * Performance: ~5ns
 *
 * Returns 0 if accuracy hasn't been verified
 */
NIMCP_EXPORT float flashbulb_confidence_accuracy_gap(
    const flashbulb_memory_t* flashbulb
);

//=============================================================================
// Reconsolidation Functions
//=============================================================================

/**
 * @brief Reconsolidate flashbulb memory
 *
 * WHAT: Update flashbulb memory during reconsolidation window
 * WHY:  Memories can be modified during retrieval-triggered reconsolidation
 * HOW:  Blends existing memory with new information
 *
 * @param system Flashbulb system
 * @param flashbulb Flashbulb memory to reconsolidate
 * @param new_information Optional new information to incorporate
 * @param new_info_size Size of new information
 * @param blend_factor How much to blend new info [0-1] (0=keep old, 1=replace)
 * @return FLASHBULB_SUCCESS or error code
 *
 * Performance: ~300ns
 *
 * PRECONDITION: Memory must be in reconsolidation window (is_reconsolidating)
 *
 * SIDE EFFECTS:
 * - Updates memory content and signature
 * - May modify vividness and confidence
 * - Closes reconsolidation window after processing
 *
 * Example:
 *   if (flashbulb->is_reconsolidating) {
 *       const char* correction = "Actually it was 9:45 AM, not 9:30";
 *       flashbulb_reconsolidate(system, flashbulb, correction, strlen(correction), 0.3f);
 *   }
 */
NIMCP_EXPORT flashbulb_error_t flashbulb_reconsolidate(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    const void* new_information,
    size_t new_info_size,
    float blend_factor
);

/**
 * @brief Check if memory is in reconsolidation window
 *
 * WHAT: Test if memory is currently susceptible to modification
 * WHY:  Reconsolidation has a time-limited window
 *
 * @param system Flashbulb system
 * @param flashbulb Flashbulb memory to check
 * @param current_time_ms Current timestamp
 * @return true if in reconsolidation window
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool flashbulb_is_reconsolidating(
    const flashbulb_system_t* system,
    const flashbulb_memory_t* flashbulb,
    uint64_t current_time_ms
);

/**
 * @brief Force open reconsolidation window
 *
 * WHAT: Manually trigger reconsolidation for memory
 * WHY:  Therapeutic interventions may deliberately open window
 *
 * @param system Flashbulb system
 * @param flashbulb Flashbulb memory
 * @param current_time_ms Current timestamp
 * @return FLASHBULB_SUCCESS or error code
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT flashbulb_error_t flashbulb_trigger_reconsolidation(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    uint64_t current_time_ms
);

//=============================================================================
// Trauma Handling Functions
//=============================================================================

/**
 * @brief Handle trauma memory specially
 *
 * WHAT: Apply special processing for traumatic flashbulb memories
 * WHY:  Traumatic memories need different handling (intrusions, avoidance)
 * HOW:  Updates trauma-specific markers and triggers appropriate responses
 *
 * @param system Flashbulb system
 * @param flashbulb Traumatic flashbulb memory
 * @return FLASHBULB_SUCCESS or error code
 *
 * Performance: ~100ns
 *
 * PRECONDITION: flashbulb->type == FLASHBULB_TRAUMATIC
 *
 * SIDE EFFECTS:
 * - Adds to trauma tracking array
 * - Initializes trauma markers
 */
NIMCP_EXPORT flashbulb_error_t flashbulb_handle_trauma(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb
);

/**
 * @brief Process intrusive memory event
 *
 * WHAT: Handle unwanted spontaneous recall of trauma memory
 * WHY:  Intrusions are key PTSD symptom requiring tracking
 * HOW:  Updates intrusion frequency, may trigger therapeutic response
 *
 * @param system Flashbulb system
 * @param flashbulb Intrusive memory
 * @param intrusion_strength How strong was the intrusion [0-1]
 * @param current_time_ms Current timestamp
 * @return Updated intrusion frequency
 *
 * Performance: ~50ns
 *
 * SIDE EFFECTS:
 * - Increments intrusion frequency
 * - Updates system intrusion statistics
 */
NIMCP_EXPORT float flashbulb_process_intrusion(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    float intrusion_strength,
    uint64_t current_time_ms
);

/**
 * @brief Apply therapeutic intervention
 *
 * WHAT: Simulate therapeutic treatment for trauma memory
 * WHY:  Model effects of EMDR, exposure therapy, pharmacological intervention
 * HOW:  Modifies emotional intensity, intrusion frequency, and avoidance
 *
 * THERAPY TYPES MODELED:
 * - Emotional dampening (propranolol-like): Reduces emotional intensity
 * - Intrusion reduction (EMDR-like): Reduces unwanted recall
 * - Avoidance reduction (exposure-like): Reduces avoidance behavior
 * - Reconsolidation therapy: Opens window for modification
 *
 * @param system Flashbulb system
 * @param flashbulb Trauma memory to treat
 * @param params Therapy parameters
 * @return FLASHBULB_SUCCESS or error code
 *
 * Performance: ~200ns
 *
 * SIDE EFFECTS:
 * - Updates emotional intensity
 * - Modifies trauma markers
 * - Increments therapy_sessions count
 * - May trigger reconsolidation
 *
 * Example:
 *   flashbulb_therapy_params_t therapy = {
 *       .emotional_dampening = 0.3f,  // Moderate dampening
 *       .intrusion_reduction = 0.2f,
 *       .avoidance_reduction = 0.1f,
 *       .trigger_reconsolidation = true,
 *       .session_duration_ms = 3600000  // 1 hour
 *   };
 *   flashbulb_apply_therapy(system, trauma_memory, &therapy);
 */
NIMCP_EXPORT flashbulb_error_t flashbulb_apply_therapy(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    const flashbulb_therapy_params_t* params
);

/**
 * @brief Get all trauma memories above intrusion threshold
 *
 * WHAT: Retrieve trauma memories with high intrusion frequency
 * WHY:  Identify memories needing therapeutic attention
 *
 * @param system Flashbulb system
 * @param min_intrusion_freq Minimum intrusion frequency threshold
 * @param memories Output array (caller-allocated)
 * @param max_memories Maximum memories to return
 * @param count Output: actual count
 * @return FLASHBULB_SUCCESS or error code
 *
 * Performance: O(trauma_count)
 */
NIMCP_EXPORT flashbulb_error_t flashbulb_get_high_intrusion_memories(
    flashbulb_system_t* system,
    float min_intrusion_freq,
    flashbulb_memory_t** memories,
    size_t max_memories,
    size_t* count
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get flashbulb type name as string
 *
 * @param type Flashbulb type
 * @return Static string name (e.g., "POSITIVE", "TRAUMATIC")
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT const char* flashbulb_type_name(flashbulb_type_t type);

/**
 * @brief Get context type name as string
 *
 * @param type Context type
 * @return Static string name (e.g., "LOCATION", "INFORMANT")
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT const char* flashbulb_context_type_name(flashbulb_context_type_t type);

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT const char* flashbulb_error_string(flashbulb_error_t error);

/**
 * @brief Get flashbulb system statistics
 *
 * @param system Flashbulb system
 * @param stats Output statistics structure
 * @return FLASHBULB_SUCCESS or error code
 *
 * Performance: O(N) for computing averages
 */
NIMCP_EXPORT flashbulb_error_t flashbulb_get_stats(
    const flashbulb_system_t* system,
    flashbulb_stats_t* stats
);

/**
 * @brief Print flashbulb memory summary to stdout
 *
 * @param flashbulb Memory to print
 *
 * Performance: ~1us (I/O bound)
 */
NIMCP_EXPORT void flashbulb_print(const flashbulb_memory_t* flashbulb);

/**
 * @brief Print flashbulb system summary to stdout
 *
 * @param system System to summarize
 */
NIMCP_EXPORT void flashbulb_system_print_summary(const flashbulb_system_t* system);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t flashbulb_current_time_ms(void);

/**
 * @brief Initialize emotional intensity with defaults
 *
 * @param intensity Structure to initialize
 */
NIMCP_EXPORT void flashbulb_intensity_init(emotional_intensity_t* intensity);

/**
 * @brief Create emotional intensity from components
 *
 * @param arousal Arousal level [0-1]
 * @param valence Emotional valence [-1, +1]
 * @param surprise Surprise level [0-1]
 * @param significance Personal significance [0-1]
 * @return Initialized emotional intensity
 */
NIMCP_EXPORT emotional_intensity_t flashbulb_intensity_create(
    float arousal,
    float valence,
    float surprise,
    float significance
);

/**
 * @brief Classify flashbulb type from emotional intensity
 *
 * WHAT: Determine flashbulb type based on emotional parameters
 * WHY:  Automatic classification during encoding
 * HOW:  Uses arousal, valence, and trauma threshold
 *
 * CLASSIFICATION RULES:
 * - Trauma: arousal > trauma_threshold AND valence < -0.5
 * - Positive: valence > 0.3 AND arousal > detection_threshold
 * - Negative: valence < -0.3 AND arousal > detection_threshold
 * - Surprising: surprise > 0.7 AND arousal > detection_threshold
 *
 * @param system Flashbulb system (for thresholds)
 * @param intensity Emotional intensity to classify
 * @return Classified flashbulb type
 */
NIMCP_EXPORT flashbulb_type_t flashbulb_classify_type(
    const flashbulb_system_t* system,
    const emotional_intensity_t* intensity
);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Check if flashbulb memory is traumatic
 *
 * @param fb Flashbulb memory
 * @return true if traumatic
 */
static inline bool flashbulb_is_traumatic(const flashbulb_memory_t* fb) {
    return fb && (fb->type == FLASHBULB_TRAUMATIC || fb->requires_trauma_handling);
}

/**
 * @brief Check if flashbulb memory has high confidence
 *
 * @param fb Flashbulb memory
 * @return true if confidence > 0.8
 */
static inline bool flashbulb_is_high_confidence(const flashbulb_memory_t* fb) {
    return fb && fb->confidence > 0.8f;
}

/**
 * @brief Check if flashbulb memory is highly vivid
 *
 * @param fb Flashbulb memory
 * @return true if vividness > 0.8
 */
static inline bool flashbulb_is_vivid(const flashbulb_memory_t* fb) {
    return fb && fb->vividness > 0.8f;
}

/**
 * @brief Get flashbulb memory age in milliseconds
 *
 * @param fb Flashbulb memory
 * @param current_time_ms Current timestamp
 * @return Age in milliseconds
 */
static inline uint64_t flashbulb_age_ms(
    const flashbulb_memory_t* fb,
    uint64_t current_time_ms
) {
    if (!fb) return 0;
    return (current_time_ms > fb->encoding_time_ms) ?
           (current_time_ms - fb->encoding_time_ms) : 0;
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FLASHBULB_H
