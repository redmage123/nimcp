//=============================================================================
// nimcp_language_orchestrator.h - Language Layer Central Orchestrator
//=============================================================================
/**
 * @file nimcp_language_orchestrator.h
 * @brief Central orchestrator coordinating all language processing modules
 *
 * WHAT: Unified control point for comprehension, production, and NLP processing
 * WHY:  Coordinate Wernicke (comprehension), Broca (production), NLP, and bridges
 * HOW:  State machine managing pipeline from perceptual input to motor output
 *
 * BIOLOGICAL BASIS:
 * - Models language network coordination
 * - Integrates Wernicke's area (BA22) for comprehension
 * - Integrates Broca's area (BA44/45) for production
 * - Coordinates with prefrontal cortex (executive control)
 * - Implements arcuate fasciculus bidirectional flow
 *
 * ARCHITECTURE:
 * ```
 *   Perception Layer          Cognitive Layer           Training Layer
 *         │                         │                         │
 *         ▼                         ▼                         ▼
 *   ┌─────────────────────────────────────────────────────────────────┐
 *   │                  LANGUAGE ORCHESTRATOR                          │
 *   │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐│
 *   │  │ Wernicke │◄─►│ Broca    │  │ NLP Core │  │ Multimodal/Spike││
 *   │  │ (Compre- │  │ (Produc- │  │ (Attn,   │  │ (Cross-modal,   ││
 *   │  │  hension)│  │  tion)   │  │ Embed)   │  │  Spiking NLP)   ││
 *   │  └──────────┘  └──────────┘  └──────────┘  └──────────────────┘│
 *   │         │              │            │               │          │
 *   │         └──────────────┴────────────┴───────────────┘          │
 *   │                              │                                  │
 *   │                    State Machine                                │
 *   │     IDLE → LISTENING → COMPREHENDING → INTEGRATING → OUTPUT    │
 *   └─────────────────────────────────────────────────────────────────┘
 *         │                         │                         │
 *         ▼                         ▼                         ▼
 *   Perception Bridge      Cognitive Bridge          Training Bridge
 * ```
 *
 * @version 1.0.0 - Phase L1: Language Layer Core Infrastructure
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_ORCHESTRATOR_H
#define NIMCP_LANGUAGE_ORCHESTRATOR_H

#include "language/nimcp_language_types.h"
#include "language/nimcp_language_config.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/* Orchestrator */
typedef struct language_orchestrator language_orchestrator_t;

/* Language Subsystems */
typedef struct wernicke_adapter wernicke_adapter_t;
typedef struct broca_adapter broca_adapter_t;
struct nlp_network_struct;
typedef struct nlp_network_struct* nlp_network_t;
typedef struct speech_cortex speech_cortex_t;
typedef struct multimodal_nlp_bridge multimodal_nlp_bridge_t;

/* Language Layer Bridges */
typedef struct language_perception_bridge language_perception_bridge_t;
typedef struct language_cognitive_bridge language_cognitive_bridge_t;
typedef struct language_training_bridge language_training_bridge_t;
typedef struct language_omni_bridge language_omni_bridge_t;
typedef struct language_immune_bridge language_immune_bridge_t;
typedef struct language_gpu_bridge language_gpu_bridge_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Language orchestrator statistics
 */
typedef struct {
    /* Comprehension stats */
    uint64_t utterances_comprehended;     /**< Total utterances comprehended */
    uint64_t words_recognized;            /**< Total words recognized */
    uint64_t phonemes_processed;          /**< Total phonemes processed */
    uint64_t comprehension_errors;        /**< Comprehension error count */
    float avg_comprehension_time_ms;      /**< Average comprehension time */
    float avg_lexical_confidence;         /**< Average lexical confidence */
    float avg_semantic_coherence;         /**< Average semantic coherence */

    /* Production stats */
    uint64_t utterances_produced;         /**< Total utterances produced */
    uint64_t words_generated;             /**< Total words generated */
    uint64_t motor_commands_issued;       /**< Total motor commands */
    uint64_t production_errors;           /**< Production error count */
    float avg_production_time_ms;         /**< Average production time */
    float avg_fluency_score;              /**< Average fluency score */

    /* Integration stats */
    uint64_t semantic_activations;        /**< Concept activation count */
    uint64_t cross_modal_fusions;         /**< Cross-modal fusion events */
    uint64_t training_updates;            /**< Training updates received */
    uint64_t state_transitions;           /**< State machine transitions */
    uint64_t bio_async_messages;          /**< Bio-async messages processed */

    /* Anomaly stats */
    uint64_t semantic_anomalies;          /**< Semantic anomalies detected */
    uint64_t syntactic_anomalies;         /**< Syntactic anomalies detected */
    uint64_t ambiguities_detected;        /**< Ambiguous inputs */

    /* Connection status */
    bool wernicke_connected;              /**< Wernicke adapter connected */
    bool broca_connected;                 /**< Broca adapter connected */
    bool nlp_connected;                   /**< NLP network connected */
    bool perception_bridge_connected;     /**< Perception bridge connected */
    bool cognitive_bridge_connected;      /**< Cognitive bridge connected */
    bool training_bridge_connected;       /**< Training bridge connected */
    bool omni_bridge_connected;           /**< Omni bridge connected */
    bool immune_bridge_connected;         /**< Immune bridge connected */
    bool gpu_bridge_connected;            /**< GPU bridge connected */
    bool bio_async_connected;             /**< Bio-async registered */

    /* Current state */
    language_state_t current_state;       /**< Current processing state */
    language_mode_t current_mode;         /**< Current processing mode */
    uint64_t state_entry_time_ms;         /**< When current state was entered */
    uint64_t last_update_ms;              /**< Last update timestamp */
} language_orchestrator_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default orchestrator configuration
 *
 * @param config Output configuration structure
 */
void language_orchestrator_default_config(language_orchestrator_config_t* config);

/**
 * @brief Create language orchestrator
 *
 * WHAT: Create central language layer coordinator
 * WHY:  Single control point for all language processing
 * HOW:  Allocate orchestrator, initialize state machine, prepare connections
 *
 * @param config Configuration (NULL for defaults)
 * @return Orchestrator handle or NULL on failure
 */
language_orchestrator_t* language_orchestrator_create(
    const language_orchestrator_config_t* config
);

/**
 * @brief Destroy language orchestrator
 *
 * @param orchestrator Orchestrator to destroy
 */
void language_orchestrator_destroy(language_orchestrator_t* orchestrator);

/**
 * @brief Start language orchestrator
 *
 * WHAT: Activate the language orchestrator
 * WHY:  Begin accepting input and coordinating subsystems
 * HOW:  Start state machine, enable bridges, register bio-async
 *
 * @param orchestrator Orchestrator instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_start(language_orchestrator_t* orchestrator);

/**
 * @brief Stop language orchestrator
 *
 * @param orchestrator Orchestrator instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_stop(language_orchestrator_t* orchestrator);

/**
 * @brief Check if orchestrator is running
 *
 * @param orchestrator Orchestrator instance
 * @return true if running, false otherwise
 */
bool language_orchestrator_is_running(const language_orchestrator_t* orchestrator);

//=============================================================================
// Subsystem Connection API
//=============================================================================

/**
 * @brief Connect Wernicke's area adapter (comprehension)
 *
 * BIOLOGICAL BASIS:
 * - Wernicke's area (BA22) handles speech comprehension
 * - Phonological → Lexical → Semantic → Syntactic pipeline
 *
 * @param orchestrator Orchestrator instance
 * @param wernicke Wernicke adapter instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_connect_wernicke(
    language_orchestrator_t* orchestrator,
    wernicke_adapter_t* wernicke
);

/**
 * @brief Connect Broca's area adapter (production)
 *
 * BIOLOGICAL BASIS:
 * - Broca's area (BA44/45) handles speech production
 * - Semantic → Syntactic → Phonological → Motor pipeline
 *
 * @param orchestrator Orchestrator instance
 * @param broca Broca adapter instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_connect_broca(
    language_orchestrator_t* orchestrator,
    broca_adapter_t* broca
);

/**
 * @brief Connect NLP network
 *
 * @param orchestrator Orchestrator instance
 * @param nlp NLP network instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_connect_nlp(
    language_orchestrator_t* orchestrator,
    nlp_network_t nlp
);

/**
 * @brief Connect speech cortex
 *
 * @param orchestrator Orchestrator instance
 * @param speech Speech cortex instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_connect_speech_cortex(
    language_orchestrator_t* orchestrator,
    speech_cortex_t* speech
);

/**
 * @brief Connect multimodal NLP bridge
 *
 * @param orchestrator Orchestrator instance
 * @param multimodal Multimodal NLP bridge instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_connect_multimodal(
    language_orchestrator_t* orchestrator,
    multimodal_nlp_bridge_t* multimodal
);

//=============================================================================
// Bridge Connection API
//=============================================================================

/**
 * @brief Connect perception integration bridge
 *
 * WHAT: Connect language layer to perception layer
 * WHY:  Receive phonemes, speech features, visual text
 * HOW:  Bidirectional: perception→language (input), language→perception (attention)
 *
 * @param orchestrator Orchestrator instance
 * @param bridge Perception bridge instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_connect_perception_bridge(
    language_orchestrator_t* orchestrator,
    language_perception_bridge_t* bridge
);

/**
 * @brief Connect cognitive integration bridge
 *
 * WHAT: Connect language layer to cognitive layer
 * WHY:  Access working memory, attention, semantic memory, reasoning
 * HOW:  Bidirectional: cognitive→language (state), language→cognitive (content)
 *
 * @param orchestrator Orchestrator instance
 * @param bridge Cognitive bridge instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_connect_cognitive_bridge(
    language_orchestrator_t* orchestrator,
    language_cognitive_bridge_t* bridge
);

/**
 * @brief Connect training integration bridge
 *
 * WHAT: Connect language layer to training layer
 * WHY:  Language learning (vocabulary, grammar, phonemes)
 * HOW:  Bidirectional: training→language (rates), language→training (errors)
 *
 * @param orchestrator Orchestrator instance
 * @param bridge Training bridge instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_connect_training_bridge(
    language_orchestrator_t* orchestrator,
    language_training_bridge_t* bridge
);

/**
 * @brief Connect omnidirectional inference bridge
 *
 * WHAT: Connect to predictive processing system
 * WHY:  Predictive language processing (phoneme/word prediction)
 * HOW:  Integrate with JEPA, predictive hierarchy, Hopfield memory
 *
 * @param orchestrator Orchestrator instance
 * @param bridge Omni bridge instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_connect_omni_bridge(
    language_orchestrator_t* orchestrator,
    language_omni_bridge_t* bridge
);

/**
 * @brief Connect immune integration bridge
 *
 * WHAT: Connect to brain immune system
 * WHY:  Model inflammation effects on language (aphasia)
 * HOW:  Cytokine effects on comprehension/production
 *
 * @param orchestrator Orchestrator instance
 * @param bridge Immune bridge instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_connect_immune_bridge(
    language_orchestrator_t* orchestrator,
    language_immune_bridge_t* bridge
);

/**
 * @brief Connect GPU acceleration bridge
 *
 * @param orchestrator Orchestrator instance
 * @param bridge GPU bridge instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_connect_gpu_bridge(
    language_orchestrator_t* orchestrator,
    language_gpu_bridge_t* bridge
);

//=============================================================================
// Processing API
//=============================================================================

/**
 * @brief Process language input
 *
 * WHAT: Main entry point for language input processing
 * WHY:  Accept audio, text, phonemes, or semantic vectors
 * HOW:  Route to appropriate pipeline based on input type
 *
 * @param orchestrator Orchestrator instance
 * @param input Input data
 * @param input_size Size of input data (bytes or count depending on type)
 * @param input_type Type of input
 * @return 0 on success, -1 on error
 */
int language_orchestrator_process_input(
    language_orchestrator_t* orchestrator,
    const void* input,
    uint32_t input_size,
    language_input_type_t input_type
);

/**
 * @brief Process phoneme sequence
 *
 * @param orchestrator Orchestrator instance
 * @param phonemes Phoneme array
 * @param count Number of phonemes
 * @return 0 on success, -1 on error
 */
int language_orchestrator_process_phonemes(
    language_orchestrator_t* orchestrator,
    const language_phoneme_t* phonemes,
    uint32_t count
);

/**
 * @brief Process text input
 *
 * @param orchestrator Orchestrator instance
 * @param text Text string (null-terminated)
 * @return 0 on success, -1 on error
 */
int language_orchestrator_process_text(
    language_orchestrator_t* orchestrator,
    const char* text
);

/**
 * @brief Generate language output
 *
 * WHAT: Generate speech/text from semantic representation
 * WHY:  Convert concepts/meaning to producible form
 * HOW:  Route through Broca's area production pipeline
 *
 * @param orchestrator Orchestrator instance
 * @param semantic_input Semantic representation to verbalize
 * @param semantic_dim Dimension of semantic vector
 * @param output Output buffer for generated content
 * @param max_output Maximum output buffer size
 * @param output_size Actual output size written
 * @param output_type Desired output type
 * @return 0 on success, -1 on error
 */
int language_orchestrator_generate_output(
    language_orchestrator_t* orchestrator,
    const float* semantic_input,
    uint32_t semantic_dim,
    void* output,
    uint32_t max_output,
    uint32_t* output_size,
    language_output_type_t output_type
);

/**
 * @brief Get current comprehension result
 *
 * @param orchestrator Orchestrator instance
 * @param result Output: comprehension result (caller allocates)
 * @return 0 on success, -1 on error or no result available
 */
int language_orchestrator_get_comprehension(
    const language_orchestrator_t* orchestrator,
    language_comprehension_result_t* result
);

/**
 * @brief Get current production plan
 *
 * @param orchestrator Orchestrator instance
 * @param plan Output: production plan (caller allocates)
 * @return 0 on success, -1 on error or no plan available
 */
int language_orchestrator_get_production_plan(
    const language_orchestrator_t* orchestrator,
    language_production_plan_t* plan
);

//=============================================================================
// Update Cycle API
//=============================================================================

/**
 * @brief Main update cycle
 *
 * WHAT: Periodic update of language layer state
 * WHY:  Process pending input, update bridges, advance state machine
 * HOW:  Called from brain update loop
 *
 * @param orchestrator Orchestrator instance
 * @param current_time_ms Current timestamp in milliseconds
 * @return 0 on success, -1 on error
 */
int language_orchestrator_update(
    language_orchestrator_t* orchestrator,
    uint64_t current_time_ms
);

/**
 * @brief Process pending bio-async messages
 *
 * @param orchestrator Orchestrator instance
 * @return Number of messages processed, or -1 on error
 */
int language_orchestrator_process_messages(
    language_orchestrator_t* orchestrator
);

//=============================================================================
// State API
//=============================================================================

/**
 * @brief Get current state
 *
 * @param orchestrator Orchestrator instance
 * @return Current state
 */
language_state_t language_orchestrator_get_state(
    const language_orchestrator_t* orchestrator
);

/**
 * @brief Get current mode
 *
 * @param orchestrator Orchestrator instance
 * @return Current mode
 */
language_mode_t language_orchestrator_get_mode(
    const language_orchestrator_t* orchestrator
);

/**
 * @brief Set processing mode
 *
 * @param orchestrator Orchestrator instance
 * @param mode Desired mode
 * @return 0 on success, -1 on error
 */
int language_orchestrator_set_mode(
    language_orchestrator_t* orchestrator,
    language_mode_t mode
);

/**
 * @brief Reset to idle state
 *
 * @param orchestrator Orchestrator instance
 * @return 0 on success, -1 on error
 */
int language_orchestrator_reset(language_orchestrator_t* orchestrator);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get statistics
 *
 * @param orchestrator Orchestrator instance
 * @param stats Output: statistics structure
 * @return 0 on success, -1 on error
 */
int language_orchestrator_get_stats(
    const language_orchestrator_t* orchestrator,
    language_orchestrator_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param orchestrator Orchestrator instance
 */
void language_orchestrator_reset_stats(language_orchestrator_t* orchestrator);

//=============================================================================
// Event API
//=============================================================================

/**
 * @brief Language event callback type
 */
typedef void (*language_event_callback_t)(
    const language_event_t* event,
    void* user_data
);

/**
 * @brief Register event callback
 *
 * @param orchestrator Orchestrator instance
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return 0 on success, -1 on error
 */
int language_orchestrator_register_callback(
    language_orchestrator_t* orchestrator,
    language_event_callback_t callback,
    void* user_data
);

/**
 * @brief Unregister event callback
 *
 * @param orchestrator Orchestrator instance
 * @param callback Callback to unregister
 * @return 0 on success, -1 on error
 */
int language_orchestrator_unregister_callback(
    language_orchestrator_t* orchestrator,
    language_event_callback_t callback
);

//=============================================================================
// Memory Management
//=============================================================================

/**
 * @brief Free comprehension result resources
 *
 * @param result Result to free (does not free the struct itself)
 */
void language_comprehension_result_free(language_comprehension_result_t* result);

/**
 * @brief Free production plan resources
 *
 * @param plan Plan to free (does not free the struct itself)
 */
void language_production_plan_free(language_production_plan_t* plan);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_ORCHESTRATOR_H */
