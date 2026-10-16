/**
 * @file nimcp_amygdala_autobio_bridge.h
 * @brief Amygdala-Autobiographical Memory Integration Bridge
 * @version 1.0.0
 * @date 2025-12-22
 *
 * WHAT: Bidirectional integration between amygdala (emotional processing) and
 *       autobiographical memory (episodic life memories)
 * WHY:  Emotional events are better remembered (flashbulb memories). The amygdala
 *       tags memories with emotional significance. Recalling emotional memories
 *       reactivates amygdala responses.
 * HOW:  Amygdala emotional arousal enhances memory encoding salience. Memory recall
 *       of emotional events triggers amygdala reactivation. Fear conditioning creates
 *       autobiographical landmarks.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * AMYGDALA → AUTOBIOGRAPHICAL MEMORY PATHWAYS:
 * -------------------------------------------
 * 1. Emotional Tagging of Memories:
 *    - Amygdala activity during encoding enhances memory consolidation
 *    - Norepinephrine release strengthens hippocampal encoding
 *    - High-arousal events → stronger, more vivid autobiographical memories
 *    - Reference: McGaugh (2004) "The amygdala modulates the consolidation
 *      of memories of emotionally arousing experiences"
 *
 * 2. Flashbulb Memories:
 *    - Extreme emotional arousal creates highly detailed "snapshot" memories
 *    - High amygdala activation → enhanced encoding of contextual details
 *    - "Where were you when X happened?" - vivid recall of emotional events
 *    - Reference: Brown & Kulik (1977) "Flashbulb memories"
 *
 * 3. Fear Memory Consolidation:
 *    - Amygdala-driven consolidation enhances fear-related autobiographical memories
 *    - Fear conditioning creates distinct episodic memories
 *    - PTSD: Overconsolidation of traumatic autobiographical memories
 *    - Reference: Phelps (2004) "Human emotion and memory"
 *
 * 4. Negative Bias in Memory:
 *    - Amygdala preferentially enhances negative/threatening memories
 *    - Negative autobiographical events are more vivid and persistent
 *    - Evolutionary advantage: Remember dangers
 *    - Reference: Kensinger & Corkin (2003) "Memory enhancement for emotional words"
 *
 * AUTOBIOGRAPHICAL MEMORY → AMYGDALA PATHWAYS:
 * -------------------------------------------
 * 1. Emotional Re-experiencing on Recall:
 *    - Recalling emotional autobiographical memories reactivates amygdala
 *    - PTSD flashbacks: Full amygdala activation from memory retrieval
 *    - "Mental time travel" reinstates emotional states
 *    - Reference: Buchanan (2007) "Retrieval of emotional memories"
 *
 * 2. Trauma Memory Recall:
 *    - Autobiographical trauma memories trigger amygdala fear responses
 *    - Can induce full-blown fear/anxiety from memory alone
 *    - Avoidance behaviors emerge from autobiographical trauma recall
 *    - Reference: Lanius et al. (2006) "Functional connectivity of amygdala
 *      during traumatic imagery"
 *
 * 3. Positive Memory Regulation:
 *    - Recalling positive autobiographical memories can downregulate amygdala
 *    - "Think of a happy memory" - emotion regulation strategy
 *    - Positive memories reduce current fear/anxiety
 *    - Reference: Joormann & Siemer (2004) "Memory accessibility and mood regulation"
 *
 * 4. Autobiographical Self-Continuity:
 *    - Coherent autobiographical narrative stabilizes emotional processing
 *    - Identity-defining memories shape amygdala reactivity patterns
 *    - "Who I am" influences what triggers emotional responses
 *    - Reference: Conway (2005) "Memory and the self"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║            AMYGDALA-AUTOBIOGRAPHICAL MEMORY BRIDGE                         ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │          AMYGDALA → AUTOBIOGRAPHICAL MEMORY PATHWAYS                │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  AMYGDALA    │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ Fear: 0.8    │  ───────┐                                       │  ║
 * ║   │   │ Anxiety: 0.6 │         │ Emotional Tagging                     │  ║
 * ║   │   │ Arousal: 0.9 │         ├──→ Salience Boost (+0.4)              │  ║
 * ║   │   └──────────────┘         │    Encoding Enhancement               │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │  AUTOBIOGRAPHICAL MEMORY         │                             │  ║
 * ║   │   │  - Emotional salience enhanced   │                             │  ║
 * ║   │   │  - Flashbulb mode for high fear  │                             │  ║
 * ║   │   │  - Fear memories consolidated    │                             │  ║
 * ║   │   │  - Negative bias in encoding     │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │          AUTOBIOGRAPHICAL MEMORY → AMYGDALA PATHWAYS                │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ TRAUMA MEMORY    │ ──→ Fear Response Reactivation              │  ║
 * ║   │   │ CRISIS MEMORY    │ ──→ Anxiety Elevation                       │  ║
 * ║   │   │ FAILURE MEMORY   │ ──→ Negative Emotion                        │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ ACHIEVEMENT      │ ──→ Positive Emotion                        │  ║
 * ║   │   │ LEARNING         │ ──→ Reduced Anxiety                         │  ║
 * ║   │   │ MILESTONE        │ ──→ Amygdala Downregulation                 │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_AMYGDALA_AUTOBIO_BRIDGE_H
#define NIMCP_AMYGDALA_AUTOBIO_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "cognitive/nimcp_autobiographical_memory.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Utilities */
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Emotional tagging parameters */
#define AMYGDALA_SALIENCE_BOOST_BASE       0.3f   /**< Base salience boost from amygdala */
#define AMYGDALA_SALIENCE_BOOST_SCALE      0.5f   /**< Scaling factor for fear/anxiety */
#define AMYGDALA_NEGATIVE_BIAS             0.2f   /**< Extra boost for negative memories */

/* Flashbulb memory thresholds */
#define FLASHBULB_FEAR_THRESHOLD           0.7f   /**< Fear level for flashbulb mode */
#define FLASHBULB_AROUSAL_THRESHOLD        0.8f   /**< Arousal level for flashbulb mode */
#define FLASHBULB_SALIENCE_MULTIPLIER      2.0f   /**< Salience boost in flashbulb mode */

/* Fear memory consolidation */
#define FEAR_CONSOLIDATION_BASE            0.2f   /**< Base consolidation boost */
#define FEAR_CONSOLIDATION_MAX             0.8f   /**< Maximum consolidation boost */
#define FEAR_MEMORY_IMPORTANCE_MIN         0.6f   /**< Minimum importance for fear memories */

/* Recall reactivation thresholds */
#define TRAUMA_REACTIVATION_THRESHOLD      0.7f   /**< Memory importance for full reactivation */
#define NEGATIVE_MEMORY_IMPACT_THRESHOLD   0.5f   /**< Threshold for negative memory impact */
#define POSITIVE_MEMORY_REGULATION_THRESHOLD 0.6f /**< Threshold for positive memory regulation */

/* Recall reactivation intensity */
#define TRAUMA_FEAR_REACTIVATION           0.6f   /**< Fear level from trauma recall */
#define CRISIS_ANXIETY_REACTIVATION        0.4f   /**< Anxiety from crisis recall */
#define NEGATIVE_VALENCE_SCALING           0.3f   /**< Scaling for negative emotion reactivation */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Emotional tagging state
 *
 * How amygdala activity modulates memory encoding
 */
typedef struct {
    /* Amygdala state */
    float fear_level;                   /**< Current fear [0-1] */
    float anxiety_level;                /**< Current anxiety [0-1] */
    float arousal_level;                /**< Emotional arousal [0-1] */
    amyg_threat_level_t threat_level;   /**< Threat assessment */

    /* Memory encoding effects */
    float emotional_salience;           /**< Salience boost [0-1] */
    float consolidation_boost;          /**< Enhanced consolidation [0-1] */
    bool flashbulb_mode;                /**< High-arousal "snapshot" mode */
    float negative_bias;                /**< Extra encoding for negative events */

    /* Statistics */
    uint32_t tagged_memories;           /**< Total memories tagged */
    uint32_t flashbulb_memories;        /**< Count of flashbulb memories */
} emotional_tagging_state_t;

/**
 * @brief Recall reactivation state
 *
 * How memory recall triggers amygdala responses
 */
typedef struct {
    /* Recalled memory properties */
    uint64_t memory_id;                 /**< ID of recalled memory */
    autobio_memory_type_t memory_type;  /**< Type of memory */
    memory_valence_t valence;           /**< Emotional valence */
    float emotional_intensity;          /**< Intensity [0-1] */
    float importance;                   /**< Memory importance [0-1] */

    /* Amygdala reactivation */
    float fear_reactivation;            /**< Induced fear [0-1] */
    float anxiety_reactivation;         /**< Induced anxiety [0-1] */
    float emotional_reexperience;       /**< "Mental time travel" intensity [0-1] */
    bool full_reactivation;             /**< PTSD-like full re-experiencing */

    /* Regulation effects */
    float positive_regulation;          /**< Positive memory downregulation [0-1] */
    float anxiety_reduction;            /**< Anxiety reduction from positive recall */

    /* Statistics */
    uint32_t trauma_recalls;            /**< Count of trauma memory recalls */
    uint32_t positive_recalls;          /**< Count of positive memory recalls */
} recall_reactivation_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_emotional_tagging;          /**< Amygdala tags memory encoding */
    bool enable_flashbulb_memories;         /**< High-arousal snapshot mode */
    bool enable_fear_consolidation;         /**< Enhanced fear memory consolidation */
    bool enable_recall_reactivation;        /**< Memory recall triggers amygdala */
    bool enable_positive_regulation;        /**< Positive memories downregulate amygdala */
    bool enable_negative_bias;              /**< Preferential encoding of negative events */

    /* Sensitivity tuning */
    float salience_sensitivity;             /**< Salience boost multiplier [0.5-2.0] */
    float reactivation_sensitivity;         /**< Recall reactivation multiplier [0.5-2.0] */

    /* Thresholds */
    float flashbulb_fear_threshold;         /**< Fear level for flashbulb [0.5-0.9] */
    float trauma_reactivation_threshold;    /**< Importance for full reactivation [0.5-0.9] */
} amygdala_autobio_config_t;

/**
 * @brief Complete amygdala-autobiographical memory bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    amygdala_t* amygdala;
    autobiographical_memory_t autobio_memory;

    /* Current state */
    emotional_tagging_state_t tagging_state;
    recall_reactivation_state_t reactivation_state;

    /* Configuration */
    amygdala_autobio_config_t config;

    /* Statistics */
    uint64_t total_updates;
    uint32_t memories_tagged;
    uint32_t memories_recalled;
    uint32_t flashbulb_count;
    uint32_t trauma_reactivations;
    uint32_t positive_regulations;

    bool connected;                      /**< Whether systems are connected */
} amygdala_autobio_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, NIMCP_ERROR_NULL_POINTER on error
 */
int amygdala_autobio_default_config(amygdala_autobio_config_t* config);

/**
 * @brief Create amygdala-autobiographical memory bridge
 *
 * WHAT: Initialize bidirectional amygdala-autobio integration
 * WHY:  Enable realistic emotional memory coupling
 * HOW:  Allocate structure, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
amygdala_autobio_bridge_t* amygdala_autobio_create(
    const amygdala_autobio_config_t* config
);

/**
 * @brief Destroy amygdala-autobiographical memory bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void amygdala_autobio_destroy(amygdala_autobio_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect amygdala to bridge
 *
 * WHAT: Link amygdala system to bridge
 * WHY:  Enable amygdala → memory emotional tagging
 * HOW:  Store pointer, validate state
 *
 * @param bridge Bridge instance
 * @param amygdala Amygdala system
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_connect_amygdala(
    amygdala_autobio_bridge_t* bridge,
    amygdala_t* amygdala
);

/**
 * @brief Connect autobiographical memory to bridge
 *
 * WHAT: Link autobiographical memory system to bridge
 * WHY:  Enable memory → amygdala reactivation
 * HOW:  Store pointer, validate state
 *
 * @param bridge Bridge instance
 * @param autobio Autobiographical memory system
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_connect_memory(
    amygdala_autobio_bridge_t* bridge,
    autobiographical_memory_t autobio
);

/* ============================================================================
 * Amygdala → Memory API (Emotional Tagging)
 * ============================================================================ */

/**
 * @brief Update emotional tagging state from amygdala
 *
 * WHAT: Query amygdala state, compute memory encoding modulation
 * WHY:  Amygdala arousal enhances memory consolidation
 * HOW:  Read fear/anxiety/arousal, compute salience boost
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_update(amygdala_autobio_bridge_t* bridge);

/**
 * @brief Tag memory with emotional salience from amygdala
 *
 * WHAT: Apply amygdala-driven salience boost to memory encoding
 * WHY:  Emotional events are better remembered
 * HOW:  Increase importance, set identity_defining flag if high arousal
 *
 * @param bridge Bridge instance
 * @param memory_id Memory to tag
 * @param emotional_intensity Emotional intensity of event [0-1]
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_tag_memory(
    amygdala_autobio_bridge_t* bridge,
    uint64_t memory_id,
    float emotional_intensity
);

/**
 * @brief Get current emotional salience boost
 *
 * WHAT: Query current salience multiplier for new memories
 * WHY:  Allow external memory systems to apply tagging
 * HOW:  Return computed salience from amygdala state
 *
 * @param bridge Bridge instance
 * @return Salience boost [0-2+] (1.0 = neutral, >1.0 = enhanced)
 */
float amygdala_autobio_get_salience_boost(const amygdala_autobio_bridge_t* bridge);

/**
 * @brief Check if in flashbulb memory mode
 *
 * WHAT: Determine if current arousal warrants "snapshot" encoding
 * WHY:  Extreme emotional events create detailed flashbulb memories
 * HOW:  Check fear and arousal thresholds
 *
 * @param bridge Bridge instance
 * @return true if in flashbulb mode
 */
bool amygdala_autobio_is_flashbulb_mode(const amygdala_autobio_bridge_t* bridge);

/**
 * @brief Get fear memory consolidation boost
 *
 * WHAT: Query consolidation enhancement for fear-related memories
 * WHY:  Fear memories are preferentially consolidated
 * HOW:  Return boost factor based on amygdala fear level
 *
 * @param bridge Bridge instance
 * @return Consolidation boost [0-1]
 */
float amygdala_autobio_get_consolidation_boost(const amygdala_autobio_bridge_t* bridge);

/* ============================================================================
 * Memory → Amygdala API (Recall Reactivation)
 * ============================================================================ */

/**
 * @brief Trigger amygdala reactivation from memory recall
 *
 * WHAT: Activate amygdala emotional response from recalling autobiographical memory
 * WHY:  "Mental time travel" reinstates emotional states
 * HOW:  Check memory valence/intensity/type, trigger appropriate amygdala response
 *
 * @param bridge Bridge instance
 * @param memory_id Recalled memory ID
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_on_recall(
    amygdala_autobio_bridge_t* bridge,
    uint64_t memory_id
);

/**
 * @brief Reactivate fear from trauma memory
 *
 * WHAT: Trigger full fear response from trauma memory recall
 * WHY:  PTSD: Trauma memories fully reactivate amygdala
 * HOW:  Retrieve memory, check for trauma type, set amygdala fear/anxiety
 *
 * @param bridge Bridge instance
 * @param trauma_memory Trauma memory entry
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_reactivate_trauma(
    amygdala_autobio_bridge_t* bridge,
    const autobiographical_memory_entry_t* trauma_memory
);

/**
 * @brief Regulate amygdala from positive memory recall
 *
 * WHAT: Downregulate amygdala activity by recalling positive memories
 * WHY:  "Think of a happy memory" - emotion regulation strategy
 * HOW:  Check for achievement/learning/milestone, reduce amygdala fear/anxiety
 *
 * @param bridge Bridge instance
 * @param positive_memory Positive memory entry
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_regulate_from_positive(
    amygdala_autobio_bridge_t* bridge,
    const autobiographical_memory_entry_t* positive_memory
);

/**
 * @brief Get recall reactivation intensity
 *
 * WHAT: Query current amygdala reactivation from last memory recall
 * WHY:  Monitor emotional re-experiencing
 * HOW:  Return fear/anxiety reactivation levels
 *
 * @param bridge Bridge instance
 * @param fear_reactivation Output: fear level [0-1]
 * @param anxiety_reactivation Output: anxiety level [0-1]
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_get_reactivation(
    const amygdala_autobio_bridge_t* bridge,
    float* fear_reactivation,
    float* anxiety_reactivation
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get emotional tagging state
 *
 * @param bridge Bridge instance
 * @param state Output: tagging state
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_get_tagging_state(
    const amygdala_autobio_bridge_t* bridge,
    emotional_tagging_state_t* state
);

/**
 * @brief Get recall reactivation state
 *
 * @param bridge Bridge instance
 * @param state Output: reactivation state
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_get_reactivation_state(
    const amygdala_autobio_bridge_t* bridge,
    recall_reactivation_state_t* state
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge instance
 * @param total_updates Output: total update cycles
 * @param memories_tagged Output: memories emotionally tagged
 * @param trauma_reactivations Output: trauma memory recalls
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_get_statistics(
    const amygdala_autobio_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* memories_tagged,
    uint32_t* trauma_reactivations
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for emotional memory signals
 * HOW:  Register with bio_router using BIO_MODULE_AMYGDALA_AUTOBIO
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_autobio_connect_bio_async(amygdala_autobio_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int amygdala_autobio_disconnect_bio_async(amygdala_autobio_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool amygdala_autobio_is_bio_async_connected(const amygdala_autobio_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AMYGDALA_AUTOBIO_BRIDGE_H */
