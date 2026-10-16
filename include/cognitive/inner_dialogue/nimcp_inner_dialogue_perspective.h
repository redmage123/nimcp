/**
 * @file nimcp_inner_dialogue_perspective.h
 * @brief Perspective Interface and Registry for Inner Dialogue Engine
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Defines cognitive perspective abstraction and a typed registry
 * WHY:  Each perspective wraps an existing cognitive module (executive, emotion, ethics, …)
 *        providing a uniform formulate() interface for structured turn generation
 * HOW:  Callback-based design with priority scheduling and relevance filtering
 *
 * BIOLOGICAL BASIS:
 * Different cortical networks contribute distinct "voices" during deliberation:
 *  - Dorsolateral PFC (analytical)   → executive planning perspective
 *  - Ventromedial PFC (emotional)    → emotion-based intuition
 *  - Anterior cingulate (critical)   → error monitoring / metacognition
 *  - Default-mode network (creative) → imagination / counterfactual
 *  - Hippocampus (memory)            → experiential recall
 *  - Right TPJ (ethical)             → moral reasoning
 *  - Medial PFC (metacognitive)      → self-awareness / narrative
 *
 * ERROR CODE RANGE: 29100-29199 (Inner Dialogue Perspective module)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INNER_DIALOGUE_PERSPECTIVE_H
#define NIMCP_INNER_DIALOGUE_PERSPECTIVE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/inner_dialogue/nimcp_inner_dialogue_turn.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct nimcp_health_agent;

/* ============================================================================
 * Error Codes (Range: 29100-29199)
 * ============================================================================ */

#define NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_BASE       29100
#define NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_NULL        (NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_BASE + 1)
#define NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_DUPLICATE   (NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_BASE + 2)
#define NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_FULL        (NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_BASE + 3)
#define NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_NOT_FOUND   (NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_BASE + 4)
#define NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_NO_MEMORY   (NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_BASE + 5)
#define NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_TIMEOUT     (NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_BASE + 6)
#define NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_INVALID     (NIMCP_INNER_DIALOGUE_PERSPECTIVE_ERROR_BASE + 7)

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of perspectives that can be registered */
#define INNER_DIALOGUE_MAX_PERSPECTIVES     16

/** Maximum perspective name length */
#define INNER_DIALOGUE_PERSPECTIVE_NAME_LEN 64

/** First ID for user-defined custom perspectives */
#define PERSPECTIVE_CUSTOM_START            32

/* ============================================================================
 * Perspective Type Enumeration
 * ============================================================================ */

/**
 * @brief Built-in perspective types mapped to cognitive modules
 *
 * WHAT: Enumeration of all built-in cognitive voices
 * WHY:  Type-safe identification and module-to-perspective mapping
 * HOW:  Each value corresponds to a specific backing cognitive module
 */
typedef enum {
    PERSPECTIVE_ANALYTICAL = 0,     /**< Backed by Executive Controller */
    PERSPECTIVE_EMOTIONAL,          /**< Backed by Emotion system */
    PERSPECTIVE_CRITICAL,           /**< Backed by Metacognition / error monitoring */
    PERSPECTIVE_CREATIVE,           /**< Backed by Imagination Engine / Recursive Cognition */
    PERSPECTIVE_MEMORY,             /**< Backed by Introspection + Autobiographical Memory */
    PERSPECTIVE_ETHICAL,            /**< Backed by Ethics Engine */
    PERSPECTIVE_METACOGNITIVE,      /**< Backed by Self-Awareness Extended */
    PERSPECTIVE_BUILTIN_COUNT,      /**< Number of built-in types */
    PERSPECTIVE_CUSTOM_START_VAL = PERSPECTIVE_CUSTOM_START /**< Start of user-defined range */
} perspective_type_t;

/* ============================================================================
 * Callback Signatures
 * ============================================================================ */

/**
 * @brief Context provided to a perspective when asked to formulate a turn
 *
 * WHAT: Read-only snapshot of dialogue state passed to formulate()
 * WHY:  Perspectives need context to produce relevant contributions
 */
typedef struct {
    uint32_t conversation_id;                   /**< Current conversation ID */
    uint32_t turn_number;                       /**< Which turn this will be */
    const char* topic;                          /**< Conversation topic string */
    const inner_dialogue_turn_t* last_turn;     /**< Most recent prior turn (NULL if first) */
    const inner_dialogue_turn_history_t* history; /**< Full history (read-only) */
    float urgency;                              /**< Time-pressure factor [0-1] */
    float emotional_temperature;                /**< Current emotional state [0-1] */
    void* brain;                                /**< Opaque brain_t pointer for module access */
} perspective_turn_context_t;

/**
 * @brief Formulate callback — REQUIRED
 *
 * WHAT: Produce a dialogue turn given current context
 * WHY:  Core function of every perspective
 * HOW:  Populate output turn struct; return true if turn was produced
 *
 * @param context Read-only conversation context
 * @param output  Output turn (caller pre-allocated, perspective fills content/act/scores)
 * @return true if turn was successfully formulated, false to skip this perspective
 */
typedef bool (*perspective_formulate_fn)(
    const perspective_turn_context_t* context,
    inner_dialogue_turn_t* output
);

/**
 * @brief Relevance check callback — OPTIONAL
 *
 * WHAT: Quick pre-check whether perspective has something to contribute
 * WHY:  Avoid expensive formulate() calls for irrelevant perspectives
 *
 * @param context Read-only conversation context
 * @return Relevance score [0-1]; 0 = irrelevant, 1 = highly relevant
 */
typedef float (*perspective_relevance_fn)(
    const perspective_turn_context_t* context
);

/**
 * @brief Observer callback — OPTIONAL
 *
 * WHAT: Notification that another perspective has produced a turn
 * WHY:  Allows perspectives to update internal state based on dialogue progress
 *
 * @param turn      The turn that was produced by another perspective
 * @param user_data Perspective's user_data pointer
 */
typedef void (*perspective_observe_fn)(
    const inner_dialogue_turn_t* turn,
    void* user_data
);

/* ============================================================================
 * Perspective Descriptor
 * ============================================================================ */

/**
 * @brief Descriptor for registering a perspective with the engine
 *
 * WHAT: Complete specification of a cognitive perspective
 * WHY:  Engine needs callbacks, priority, and metadata to schedule perspectives
 * HOW:  Caller fills this and passes to inner_dialogue_perspective_register()
 */
typedef struct {
    perspective_type_t type;                    /**< Unique perspective type ID */
    char name[INNER_DIALOGUE_PERSPECTIVE_NAME_LEN]; /**< Human-readable name */
    float base_priority;                        /**< Base scheduling priority [0-1] */

    /* Callbacks */
    perspective_formulate_fn formulate;          /**< REQUIRED: produce a turn */
    perspective_relevance_fn check_relevance;    /**< OPTIONAL: quick relevance test */
    perspective_observe_fn observe;              /**< OPTIONAL: observe other turns */

    /* Backing module */
    void* user_data;                            /**< Pointer to backing cognitive module */
} inner_dialogue_perspective_desc_t;

/* ============================================================================
 * Registry Structure
 * ============================================================================ */

/**
 * @brief Internal perspective entry in the registry
 */
typedef struct {
    inner_dialogue_perspective_desc_t desc;     /**< Full descriptor */
    bool registered;                            /**< Slot occupied flag */
    uint32_t turns_produced;                    /**< How many turns this perspective created */
    uint32_t turns_skipped;                     /**< How many times formulate returned false */
    float cumulative_confidence;                /**< Sum of confidence across produced turns */
    float cumulative_relevance;                 /**< Sum of relevance scores */
    uint64_t last_turn_timestamp_us;            /**< Timestamp of last turn */
} inner_dialogue_perspective_entry_t;

/**
 * @brief Perspective registry
 *
 * WHAT: Array-based registry of all registered perspectives
 * WHY:  Engine iterates registry for scheduling and notification
 * HOW:  Fixed-capacity array indexed by slot (not by type)
 */
typedef struct {
    inner_dialogue_perspective_entry_t entries[INNER_DIALOGUE_MAX_PERSPECTIVES];
    uint32_t count;                             /**< Number of registered perspectives */
} inner_dialogue_perspective_registry_t;

/* ============================================================================
 * Registry Lifecycle API
 * ============================================================================ */

/**
 * @brief Initialise perspective registry
 *
 * @param registry Registry to initialise (caller owns memory)
 * @return 0 on success, error code on failure
 */
int inner_dialogue_perspective_registry_init(
    inner_dialogue_perspective_registry_t* registry);

/**
 * @brief Clear all perspectives from registry
 *
 * @param registry Registry to clear
 */
void inner_dialogue_perspective_registry_clear(
    inner_dialogue_perspective_registry_t* registry);

/* ============================================================================
 * Registration API
 * ============================================================================ */

/**
 * @brief Register a perspective with the engine
 *
 * WHAT: Add a perspective to the registry
 * WHY:  Perspectives must be registered before they can participate in dialogue
 * HOW:  Validates descriptor, checks for duplicates, copies into next free slot
 *
 * @param registry Registry
 * @param desc     Perspective descriptor (must have non-NULL formulate)
 * @return 0 on success, error code on failure
 */
int inner_dialogue_perspective_register(
    inner_dialogue_perspective_registry_t* registry,
    const inner_dialogue_perspective_desc_t* desc);

/**
 * @brief Unregister a perspective by type
 *
 * @param registry Registry
 * @param type     Perspective type to remove
 * @return 0 on success, error code if not found
 */
int inner_dialogue_perspective_unregister(
    inner_dialogue_perspective_registry_t* registry,
    perspective_type_t type);

/**
 * @brief Find a perspective entry by type
 *
 * @param registry Registry
 * @param type     Perspective type to find
 * @return Pointer to entry, or NULL if not registered
 */
const inner_dialogue_perspective_entry_t* inner_dialogue_perspective_find(
    const inner_dialogue_perspective_registry_t* registry,
    perspective_type_t type);

/**
 * @brief Get number of registered perspectives
 *
 * @param registry Registry
 * @return Count, or 0 if registry is NULL
 */
uint32_t inner_dialogue_perspective_count(
    const inner_dialogue_perspective_registry_t* registry);

/* ============================================================================
 * Scheduling API
 * ============================================================================ */

/**
 * @brief Compute effective priority for a perspective
 *
 * WHAT: Combine base_priority, relevance, urgency, recency penalty, and fairness
 * WHY:  Determines which perspective speaks next
 * HOW:  effective = base * relevance * (1 + urgency) * (1 - recency_penalty) * fairness
 *
 * @param entry      Perspective entry
 * @param context    Current conversation context
 * @param current_turn_number  Current turn index (for recency/fairness)
 * @return Effective priority [0-∞)
 */
float inner_dialogue_perspective_compute_priority(
    const inner_dialogue_perspective_entry_t* entry,
    const perspective_turn_context_t* context,
    uint32_t current_turn_number);

/**
 * @brief Select the highest-priority perspective for the next turn
 *
 * WHAT: Iterate registry, compute priorities, select winner
 * WHY:  Core scheduling decision each turn
 * HOW:  Linear scan with argmax; O(MAX_PERSPECTIVES)
 *
 * @param registry    Registry
 * @param context     Current context
 * @param turn_number Current turn number
 * @return Index of winning perspective, or -1 if none available
 */
int inner_dialogue_perspective_select_next(
    const inner_dialogue_perspective_registry_t* registry,
    const perspective_turn_context_t* context,
    uint32_t turn_number);

/* ============================================================================
 * Built-in Perspective Registration
 * ============================================================================ */

/**
 * @brief Register all 7 built-in perspectives with stub formulate callbacks
 *
 * WHAT: Populate registry with analytical/emotional/critical/creative/memory/ethical/metacognitive
 * WHY:  Provides sensible defaults; stubs return basic template turns
 * HOW:  Calls inner_dialogue_perspective_register() seven times
 *
 * @param registry Registry to populate
 * @return 0 on success, error code on failure
 */
int inner_dialogue_register_builtin_perspectives(
    inner_dialogue_perspective_registry_t* registry);

/* ============================================================================
 * Utility
 * ============================================================================ */

/**
 * @brief Convert perspective type to string
 *
 * @param type Perspective type
 * @return Static string name, or "UNKNOWN"
 */
const char* perspective_type_to_string(perspective_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INNER_DIALOGUE_PERSPECTIVE_H */
