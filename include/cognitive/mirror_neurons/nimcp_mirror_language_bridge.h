//=============================================================================
// nimcp_mirror_language_bridge.h - Mirror Neuron-Language Layer Bridge
//=============================================================================
/**
 * @file nimcp_mirror_language_bridge.h
 * @brief Bidirectional bridge integrating Mirror Neurons with Language Layer
 *
 * WHAT: Bridge connecting mirror neuron system with Broca's/Wernicke's areas
 * WHY:  Enable speech-based imitation learning, action-word binding, and
 *       articulatory simulation during speech observation
 * HOW:  Mirror neurons observe speech → activate Broca's articulatory simulation;
 *       Wernicke's provides semantic context for action understanding
 *
 * BIOLOGICAL BASIS:
 * =============================================================================
 * 1. Speech Mirror Neurons (F5/premotor cortex)
 *    - Neurons that fire both when producing and observing speech
 *    - Enable phoneme-level imitation learning
 *    - Involved in speech acquisition in infants (lip-reading, babbling)
 *    - Reference: Rizzolatti & Arbib (1998) "Language within our grasp"
 *
 * 2. Action-Semantic Binding (Wernicke-Mirror connection)
 *    - Action words ("grasp", "kick") activate motor representations
 *    - Observed actions prime associated word representations
 *    - Embodied cognition: understanding action words involves motor simulation
 *    - Reference: Pulvermuller (2005) "Brain mechanisms linking language and action"
 *
 * 3. Articulatory Rehearsal (Phonological loop)
 *    - Observing lip movements activates inner speech mechanisms
 *    - Broca's area simulates articulatory gestures during observation
 *    - Enables speech perception through motor theory
 *    - Reference: Liberman & Mattingly (1985) "Motor theory of speech perception"
 *
 * KEY PATHWAYS:
 * =============================================================================
 * Mirror -> Broca:
 *   - Speech observation activates articulatory simulation
 *   - Phoneme-level activation for covert rehearsal
 *   - Enables learning pronunciation by watching lip movements
 *
 * Mirror -> Wernicke:
 *   - Action observation triggers semantic association
 *   - Retrieves action-related words ("grasping" -> "grab", "hold")
 *   - Provides conceptual context for observed actions
 *
 * Broca -> Mirror:
 *   - Speech production activates speech mirror neurons
 *   - Enables self-monitoring through mirror system
 *   - Phoneme efference copy to mirror system
 *
 * Wernicke -> Mirror:
 *   - Word meaning provides action context
 *   - Hearing "grasp" activates grasping mirror neurons
 *   - Semantic priming of motor representations
 *
 * ARCHITECTURE:
 * =============================================================================
 * ┌────────────────────────────────────────────────────────────────────────┐
 * │                       MIRROR NEURON SYSTEM                              │
 * │  ┌─────────────────┐                    ┌─────────────────┐            │
 * │  │ Speech Mirror   │◄──────────────────►│ Action Mirror   │            │
 * │  │ Neurons (F5)    │   action-phoneme   │ Neurons (F5)    │            │
 * │  └────────┬────────┘     binding        └────────┬────────┘            │
 * │           │                                      │                      │
 * └───────────┼──────────────────────────────────────┼──────────────────────┘
 *             │                                      │
 *    ┌────────┴────────┐                    ┌────────┴────────┐
 *    │                 │                    │                 │
 *    ▼                 ▼                    ▼                 ▼
 * ┌─────────────────────────┐         ┌─────────────────────────┐
 * │      BROCA'S AREA       │         │    WERNICKE'S AREA      │
 * │ ┌───────────────────┐   │         │ ┌───────────────────┐   │
 * │ │ Phonological      │   │◄───────►│ │ Semantic          │   │
 * │ │ Processor         │   │ arcuate │ │ Integrator        │   │
 * │ ├───────────────────┤   │fasciculus├───────────────────┤   │
 * │ │ Speech Motor      │   │         │ │ Lexical           │   │
 * │ │ Planner           │   │         │ │ Access            │   │
 * │ └───────────────────┘   │         │ └───────────────────┘   │
 * └─────────────────────────┘         └─────────────────────────┘
 *
 * KEY FEATURES:
 * =============================================================================
 * 1. Phoneme-Level Mirroring
 *    - Map observed articulatory gestures to phoneme representations
 *    - Enable learning pronunciation through observation
 *    - Covert rehearsal during speech perception
 *
 * 2. Action-Word Binding
 *    - Bidirectional "grasp" <-> grasping action association
 *    - Hebbian learning strengthens action-word links
 *    - Grounding language in motor experience
 *
 * 3. Articulatory Simulation
 *    - Broca simulates observed speech gestures
 *    - Internal model of speaker's articulation
 *    - Prediction of upcoming phonemes
 *
 * 4. Semantic Context for Actions
 *    - Wernicke provides word meanings for action understanding
 *    - Goal inference enhanced by linguistic context
 *    - Narrative context shapes action interpretation
 *
 * @version 1.0.0 - Phase ML1: Mirror-Language Integration
 * @author NIMCP Development Team
 * @date 2026-01-05
 *
 * @see nimcp_mirror_neurons.h
 * @see nimcp_broca_adapter.h
 * @see nimcp_wernicke_adapter.h
 */

#ifndef NIMCP_MIRROR_LANGUAGE_BRIDGE_H
#define NIMCP_MIRROR_LANGUAGE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct mirror_language_bridge mirror_language_bridge_t;
typedef struct mirror_neurons_system* mirror_neurons_t;
typedef struct broca_adapter broca_adapter_t;
typedef struct wernicke_adapter wernicke_adapter_t;

#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

#define MIRROR_LANGUAGE_MODULE_NAME      "mirror_language_bridge"
#define MIRROR_LANGUAGE_MODULE_VERSION   "1.0.0"
#define MIRROR_LANGUAGE_BIO_MODULE_ID    0x027A  /**< Bio-async module ID */

/* Default configuration values */
#define ML_DEFAULT_UPDATE_INTERVAL_MS        10
#define ML_DEFAULT_MAX_PHONEME_MIRRORS       64
#define ML_DEFAULT_MAX_ACTION_WORDS          256
#define ML_DEFAULT_BINDING_CAPACITY          512
#define ML_DEFAULT_PHONEME_THRESHOLD         0.4f
#define ML_DEFAULT_SEMANTIC_THRESHOLD        0.5f
#define ML_DEFAULT_BINDING_STRENGTH_INIT     0.1f
#define ML_DEFAULT_BINDING_LEARNING_RATE     0.01f
#define ML_DEFAULT_SIMULATION_DECAY_RATE     0.05f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Speech observation type
 *
 * WHAT: Type of speech being observed
 * WHY:  Different speech types activate different mirror responses
 */
typedef enum {
    SPEECH_OBS_NONE = 0,              /**< No speech observed */
    SPEECH_OBS_LIP_MOVEMENT,          /**< Lip movement (visual) */
    SPEECH_OBS_AUDIO_PHONEME,         /**< Heard phoneme (auditory) */
    SPEECH_OBS_AUDIOVISUAL,           /**< Combined audiovisual */
    SPEECH_OBS_COVERT_REHEARSAL,      /**< Internal rehearsal */
    SPEECH_OBS_COUNT
} speech_observation_type_t;

/**
 * @brief Action-word binding type
 *
 * WHAT: Type of action-word association
 * WHY:  Different binding types have different strengths and uses
 */
typedef enum {
    BINDING_NONE = 0,                 /**< No binding */
    BINDING_VERB_ACTION,              /**< Action verb -> motor action ("grasp" -> grasping) */
    BINDING_NOUN_OBJECT,              /**< Object noun -> manipulation action ("cup" -> grasping) */
    BINDING_SPATIAL_MOVEMENT,         /**< Spatial word -> movement ("up" -> upward motion) */
    BINDING_MANNER_QUALITY,           /**< Manner adverb -> action quality ("quickly" -> fast) */
    BINDING_COUNT
} action_word_binding_type_t;

/**
 * @brief Mirror-language pathway direction
 *
 * WHAT: Direction of information flow
 * WHY:  Track bidirectional communication
 */
typedef enum {
    ML_PATH_MIRROR_TO_BROCA = 0,      /**< Observation -> Articulatory simulation */
    ML_PATH_MIRROR_TO_WERNICKE,       /**< Observation -> Semantic association */
    ML_PATH_BROCA_TO_MIRROR,          /**< Production -> Mirror activation */
    ML_PATH_WERNICKE_TO_MIRROR,       /**< Word meaning -> Action priming */
    ML_PATH_COUNT
} ml_pathway_t;

/**
 * @brief Bridge operating state
 *
 * WHAT: Current operational state
 * WHY:  Track bridge activity for monitoring
 */
typedef enum {
    ML_STATE_IDLE = 0,                /**< No active processing */
    ML_STATE_OBSERVING_SPEECH,        /**< Processing speech observation */
    ML_STATE_SIMULATING_ARTICULATION, /**< Covert articulatory simulation */
    ML_STATE_BINDING_ACTION_WORD,     /**< Learning action-word association */
    ML_STATE_SEMANTIC_RETRIEVAL,      /**< Retrieving semantic context */
    ML_STATE_ERROR,                   /**< Error state */
    ML_STATE_COUNT
} ml_bridge_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Configuration for Mirror-Language bridge
 */
typedef struct {
    /* Operating parameters */
    uint32_t update_interval_ms;          /**< Update cycle interval */
    uint32_t max_phoneme_mirrors;         /**< Max phoneme-level mirrors */
    uint32_t max_action_words;            /**< Max action words to track */
    uint32_t binding_capacity;            /**< Max action-word bindings */

    /* Thresholds */
    float phoneme_activation_threshold;   /**< Threshold for phoneme mirroring */
    float semantic_activation_threshold;  /**< Threshold for semantic priming */
    float binding_strength_init;          /**< Initial binding strength */

    /* Learning parameters */
    float binding_learning_rate;          /**< Hebbian binding learning rate */
    float simulation_decay_rate;          /**< Articulatory simulation decay */

    /* Features */
    bool enable_phoneme_mirroring;        /**< Enable phoneme-level mirroring */
    bool enable_action_word_binding;      /**< Enable action-word associations */
    bool enable_articulatory_simulation;  /**< Enable covert articulation */
    bool enable_semantic_priming;         /**< Enable semantic action priming */
    bool enable_covert_rehearsal;         /**< Enable covert rehearsal loop */

    /* Bio-async */
    bool enable_bio_async;                /**< Enable bio-async messaging */
} mirror_language_config_t;

/**
 * @brief Phoneme mirror activation
 *
 * WHAT: Activation of mirror neurons for observed phoneme
 * WHY:  Track phoneme-level mirroring during speech observation
 */
typedef struct {
    uint8_t phoneme_id;                   /**< Phoneme identifier */
    float observation_activation;         /**< Activation from observation */
    float simulation_activation;          /**< Activation from simulation */
    float articulatory_match;             /**< Match to articulatory gesture */
    uint64_t timestamp_ms;                /**< Activation timestamp */
    speech_observation_type_t obs_type;   /**< How phoneme was observed */
} phoneme_mirror_activation_t;

/**
 * @brief Action-word binding entry
 *
 * WHAT: Association between action and word
 * WHY:  Store learned action-word mappings
 */
typedef struct {
    uint32_t action_id;                   /**< Mirror neuron action ID */
    uint32_t word_id;                     /**< Lexicon word ID */
    char word_string[64];                 /**< Word string */
    action_word_binding_type_t type;      /**< Binding type */
    float binding_strength;               /**< Association strength [0-1] */
    uint32_t coactivation_count;          /**< Times co-activated */
    uint64_t last_activation_ms;          /**< Last activation time */
    bool is_bidirectional;                /**< Active in both directions */
} action_word_binding_t;

/**
 * @brief Speech observation event
 *
 * WHAT: Observed speech for mirror processing
 * WHY:  Input to mirror-language pathway
 */
typedef struct {
    speech_observation_type_t type;       /**< Observation type */
    uint8_t phoneme_id;                   /**< Phoneme observed */
    float articulatory_features[16];      /**< Articulatory gesture features */
    uint32_t num_features;                /**< Number of features */
    float confidence;                     /**< Observation confidence */
    uint32_t speaker_id;                  /**< Speaker/agent ID (0 = self) */
    uint64_t timestamp_ms;                /**< Observation timestamp */
} speech_observation_t;

/**
 * @brief Articulatory simulation request
 *
 * WHAT: Request for Broca to simulate articulation
 * WHY:  Enable covert rehearsal and prediction
 */
typedef struct {
    uint8_t phoneme_id;                   /**< Phoneme to simulate */
    float target_activation;              /**< Target activation level */
    bool covert_only;                     /**< Covert (no motor output) */
    uint64_t onset_time_ms;               /**< Simulation onset */
    float duration_ms;                    /**< Simulation duration */
} articulatory_simulation_t;

/**
 * @brief Semantic action context
 *
 * WHAT: Semantic context for action interpretation
 * WHY:  Wernicke provides meaning context for actions
 */
typedef struct {
    uint32_t concept_id;                  /**< Active concept ID */
    char concept_name[64];                /**< Concept label */
    float relevance;                      /**< Relevance to current action */
    uint32_t* related_actions;            /**< Related action IDs */
    uint32_t num_related;                 /**< Number of related actions */
    float semantic_activation;            /**< Semantic activation level */
} semantic_action_context_t;

/**
 * @brief Pathway activation record
 *
 * WHAT: Record of pathway activation
 * WHY:  Track bidirectional communication
 */
typedef struct {
    ml_pathway_t pathway;                 /**< Which pathway */
    float activation_strength;            /**< Activation level */
    uint64_t timestamp_ms;                /**< When activated */
    uint32_t source_id;                   /**< Source (action/word/phoneme ID) */
    uint32_t target_id;                   /**< Target ID */
} pathway_activation_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Counts */
    uint64_t speech_observations;         /**< Speech observations processed */
    uint64_t phoneme_activations;         /**< Phoneme mirror activations */
    uint64_t articulatory_simulations;    /**< Articulatory simulations triggered */
    uint64_t action_word_bindings;        /**< Action-word bindings created */
    uint64_t semantic_retrievals;         /**< Semantic retrievals performed */

    /* Pathway counts */
    uint64_t mirror_to_broca_count;       /**< Mirror->Broca activations */
    uint64_t mirror_to_wernicke_count;    /**< Mirror->Wernicke activations */
    uint64_t broca_to_mirror_count;       /**< Broca->Mirror activations */
    uint64_t wernicke_to_mirror_count;    /**< Wernicke->Mirror activations */

    /* Timing */
    float avg_observation_latency_ms;     /**< Average observation processing time */
    float avg_simulation_latency_ms;      /**< Average simulation latency */
    float avg_binding_latency_ms;         /**< Average binding update time */

    /* Quality */
    float avg_phoneme_match;              /**< Average phoneme match score */
    float avg_binding_strength;           /**< Average binding strength */
    float avg_semantic_relevance;         /**< Average semantic relevance */

    /* Current state */
    ml_bridge_state_t state;              /**< Current bridge state */
    uint32_t active_bindings;             /**< Currently active bindings */
    uint32_t active_simulations;          /**< Currently running simulations */
} mirror_language_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Callback for phoneme mirror activation
 */
typedef void (*ml_phoneme_callback_t)(
    const phoneme_mirror_activation_t* activation,
    void* user_data
);

/**
 * @brief Callback for action-word binding events
 */
typedef void (*ml_binding_callback_t)(
    const action_word_binding_t* binding,
    void* user_data
);

/**
 * @brief Callback for articulatory simulation events
 */
typedef void (*ml_simulation_callback_t)(
    const articulatory_simulation_t* simulation,
    void* user_data
);

/**
 * @brief Callback for semantic context events
 */
typedef void (*ml_semantic_callback_t)(
    const semantic_action_context_t* context,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default mirror-language bridge configuration
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
mirror_language_config_t mirror_language_default_config(void);

/**
 * @brief Create mirror-language bridge
 *
 * WHAT: Allocate and initialize the bridge
 * WHY:  Central point for mirror-language integration
 * HOW:  Connect mirror neurons to Broca's and Wernicke's areas
 *
 * @param mirror Mirror neuron system
 * @param broca Broca's area adapter (can be NULL initially)
 * @param wernicke Wernicke's area adapter (can be NULL initially)
 * @param config Configuration (NULL for defaults)
 * @return New bridge instance, or NULL on failure
 */
mirror_language_bridge_t* mirror_language_bridge_create(
    mirror_neurons_t mirror,
    broca_adapter_t* broca,
    wernicke_adapter_t* wernicke,
    const mirror_language_config_t* config
);

/**
 * @brief Destroy mirror-language bridge
 *
 * WHAT: Free all resources associated with the bridge
 * WHY:  Prevent memory leaks
 * HOW:  Release bindings, buffers, and bridge structure
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void mirror_language_bridge_destroy(mirror_language_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Clear state and return to idle
 * WHY:  Prepare for new processing without reallocation
 * HOW:  Reset buffers, clear activations, keep bindings
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int mirror_language_bridge_reset(mirror_language_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect to Broca's area
 *
 * WHAT: Establish connection to Broca's area adapter
 * WHY:  Enable articulatory simulation pathway
 * HOW:  Store adapter reference, setup communication
 *
 * @param bridge Bridge instance
 * @param broca Broca's area adapter
 * @return 0 on success, -1 on error
 */
int mirror_language_connect_broca(
    mirror_language_bridge_t* bridge,
    broca_adapter_t* broca
);

/**
 * @brief Connect to Wernicke's area
 *
 * WHAT: Establish connection to Wernicke's area adapter
 * WHY:  Enable semantic context pathway
 * HOW:  Store adapter reference, setup communication
 *
 * @param bridge Bridge instance
 * @param wernicke Wernicke's area adapter
 * @return 0 on success, -1 on error
 */
int mirror_language_connect_wernicke(
    mirror_language_bridge_t* bridge,
    wernicke_adapter_t* wernicke
);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging
 * WHY:  Asynchronous communication with language modules
 * HOW:  Register module with router
 *
 * @param bridge Bridge instance
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int mirror_language_connect_bio_async(
    mirror_language_bridge_t* bridge,
    bio_router_t router
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Process pending activities and update state
 * WHY:  Main processing loop for bridge
 * HOW:  Process observations, update simulations, decay activations
 *
 * @param bridge Bridge instance
 * @param timestamp_ms Current timestamp
 * @return 0 on success, -1 on error
 */
int mirror_language_bridge_update(
    mirror_language_bridge_t* bridge,
    uint64_t timestamp_ms
);

//=============================================================================
// Mirror -> Broca Pathway (Speech Observation -> Articulatory Simulation)
//=============================================================================

/**
 * @brief Process speech observation for mirror activation
 *
 * WHAT: Process observed speech gesture for mirroring
 * WHY:  Trigger articulatory simulation in Broca's area
 * HOW:  Match observation to phoneme, activate mirror neurons, trigger simulation
 *
 * @param bridge Bridge instance
 * @param observation Speech observation event
 * @return 0 on success, -1 on error
 */
int mirror_language_observe_speech(
    mirror_language_bridge_t* bridge,
    const speech_observation_t* observation
);

/**
 * @brief Request articulatory simulation
 *
 * WHAT: Request Broca to simulate phoneme articulation
 * WHY:  Enable covert rehearsal during speech perception
 * HOW:  Send simulation request to Broca's speech motor planner
 *
 * @param bridge Bridge instance
 * @param phoneme_id Phoneme to simulate
 * @param activation Activation level [0-1]
 * @return 0 on success, -1 on error
 */
int mirror_language_request_simulation(
    mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    float activation
);

/**
 * @brief Get phoneme mirror activation
 *
 * WHAT: Query activation level for phoneme mirror neurons
 * WHY:  Check current phoneme mirroring state
 * HOW:  Look up activation record by phoneme ID
 *
 * @param bridge Bridge instance
 * @param phoneme_id Phoneme to query
 * @param activation Output activation record
 * @return 0 on success, -1 if not found
 */
int mirror_language_get_phoneme_activation(
    const mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    phoneme_mirror_activation_t* activation
);

//=============================================================================
// Mirror -> Wernicke Pathway (Action Observation -> Semantic Association)
//=============================================================================

/**
 * @brief Process action for semantic association
 *
 * WHAT: Trigger semantic retrieval for observed action
 * WHY:  Get word meanings associated with action
 * HOW:  Query Wernicke for action-related concepts
 *
 * @param bridge Bridge instance
 * @param action_id Mirror neuron action ID
 * @param contexts Output semantic contexts (must be pre-allocated)
 * @param max_contexts Maximum contexts to retrieve
 * @param num_retrieved Output: actual number retrieved
 * @return 0 on success, -1 on error
 */
int mirror_language_get_action_semantics(
    mirror_language_bridge_t* bridge,
    uint32_t action_id,
    semantic_action_context_t* contexts,
    uint32_t max_contexts,
    uint32_t* num_retrieved
);

/**
 * @brief Get words associated with action
 *
 * WHAT: Retrieve words bound to action
 * WHY:  Linguistic description of observed action
 * HOW:  Look up action-word bindings
 *
 * @param bridge Bridge instance
 * @param action_id Mirror neuron action ID
 * @param bindings Output binding array
 * @param max_bindings Maximum bindings to retrieve
 * @param num_bindings Output: actual number retrieved
 * @return 0 on success, -1 on error
 */
int mirror_language_get_action_words(
    const mirror_language_bridge_t* bridge,
    uint32_t action_id,
    action_word_binding_t* bindings,
    uint32_t max_bindings,
    uint32_t* num_bindings
);

//=============================================================================
// Broca -> Mirror Pathway (Speech Production -> Mirror Activation)
//=============================================================================

/**
 * @brief Notify mirror system of speech production
 *
 * WHAT: Activate speech mirror neurons during production
 * WHY:  Self-monitoring through mirror system
 * HOW:  Send efference copy to mirror neurons
 *
 * @param bridge Bridge instance
 * @param phoneme_id Phoneme being produced
 * @param motor_activation Motor activation level
 * @return 0 on success, -1 on error
 */
int mirror_language_notify_production(
    mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    float motor_activation
);

/**
 * @brief Process speech efference copy
 *
 * WHAT: Process motor command copy for mirror prediction
 * WHY:  Enable prediction error computation
 * HOW:  Compare efference copy to observation prediction
 *
 * @param bridge Bridge instance
 * @param phoneme_id Phoneme in production
 * @param predicted_features Predicted articulatory features
 * @param num_features Number of features
 * @return Prediction error (0 = perfect match)
 */
float mirror_language_process_efference(
    mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    const float* predicted_features,
    uint32_t num_features
);

//=============================================================================
// Wernicke -> Mirror Pathway (Word Meaning -> Action Priming)
//=============================================================================

/**
 * @brief Prime action from word
 *
 * WHAT: Activate mirror neurons from word meaning
 * WHY:  Embodied semantics - words prime motor representations
 * HOW:  Look up word-action bindings, activate mirror neurons
 *
 * @param bridge Bridge instance
 * @param word_id Lexicon word ID
 * @param word_string Word string (if word_id is 0)
 * @param activation Priming activation level
 * @return 0 on success, -1 on error
 */
int mirror_language_prime_from_word(
    mirror_language_bridge_t* bridge,
    uint32_t word_id,
    const char* word_string,
    float activation
);

/**
 * @brief Get actions primed by word
 *
 * WHAT: Query which actions are primed by word
 * WHY:  Check motor simulation from language
 * HOW:  Look up word-action bindings
 *
 * @param bridge Bridge instance
 * @param word_id Lexicon word ID
 * @param action_ids Output action IDs
 * @param activations Output activation levels
 * @param max_actions Maximum actions to return
 * @param num_actions Output: actual number returned
 * @return 0 on success, -1 on error
 */
int mirror_language_get_primed_actions(
    const mirror_language_bridge_t* bridge,
    uint32_t word_id,
    uint32_t* action_ids,
    float* activations,
    uint32_t max_actions,
    uint32_t* num_actions
);

//=============================================================================
// Action-Word Binding Functions
//=============================================================================

/**
 * @brief Create action-word binding
 *
 * WHAT: Explicitly create action-word association
 * WHY:  Initialize vocabulary with action-word mappings
 * HOW:  Add binding to hash table
 *
 * @param bridge Bridge instance
 * @param action_id Mirror neuron action ID
 * @param word_id Lexicon word ID
 * @param word_string Word string
 * @param type Binding type
 * @param initial_strength Initial binding strength [0-1]
 * @return 0 on success, -1 on error
 */
int mirror_language_create_binding(
    mirror_language_bridge_t* bridge,
    uint32_t action_id,
    uint32_t word_id,
    const char* word_string,
    action_word_binding_type_t type,
    float initial_strength
);

/**
 * @brief Strengthen binding through co-activation
 *
 * WHAT: Hebbian update of action-word binding
 * WHY:  Learn associations through co-occurrence
 * HOW:  Apply learning rule when action and word co-active
 *
 * @param bridge Bridge instance
 * @param action_id Mirror neuron action ID
 * @param word_id Lexicon word ID
 * @param action_activation Action activation level
 * @param word_activation Word activation level
 * @return New binding strength, or -1.0 on error
 */
float mirror_language_strengthen_binding(
    mirror_language_bridge_t* bridge,
    uint32_t action_id,
    uint32_t word_id,
    float action_activation,
    float word_activation
);

/**
 * @brief Get binding between action and word
 *
 * WHAT: Query existing binding
 * WHY:  Check association strength
 * HOW:  Look up in binding table
 *
 * @param bridge Bridge instance
 * @param action_id Mirror neuron action ID
 * @param word_id Lexicon word ID
 * @param binding Output binding structure
 * @return 0 if found, -1 if not found
 */
int mirror_language_get_binding(
    const mirror_language_bridge_t* bridge,
    uint32_t action_id,
    uint32_t word_id,
    action_word_binding_t* binding
);

/**
 * @brief Remove action-word binding
 *
 * WHAT: Delete binding from table
 * WHY:  Prune unused associations
 * HOW:  Remove from hash table
 *
 * @param bridge Bridge instance
 * @param action_id Mirror neuron action ID
 * @param word_id Lexicon word ID
 * @return 0 on success, -1 if not found
 */
int mirror_language_remove_binding(
    mirror_language_bridge_t* bridge,
    uint32_t action_id,
    uint32_t word_id
);

//=============================================================================
// Phoneme Mirroring Functions
//=============================================================================

/**
 * @brief Register phoneme-articulation mapping
 *
 * WHAT: Define articulatory gesture for phoneme
 * WHY:  Enable phoneme-level mirroring
 * HOW:  Store articulatory feature template
 *
 * @param bridge Bridge instance
 * @param phoneme_id Phoneme identifier
 * @param features Articulatory feature vector
 * @param num_features Number of features
 * @return 0 on success, -1 on error
 */
int mirror_language_register_phoneme(
    mirror_language_bridge_t* bridge,
    uint8_t phoneme_id,
    const float* features,
    uint32_t num_features
);

/**
 * @brief Match observation to phoneme
 *
 * WHAT: Determine which phoneme matches observed gesture
 * WHY:  Phoneme recognition through motor matching
 * HOW:  Compare observation to registered templates
 *
 * @param bridge Bridge instance
 * @param features Observed articulatory features
 * @param num_features Number of features
 * @param best_phoneme Output: best matching phoneme ID
 * @param confidence Output: match confidence [0-1]
 * @return 0 on success, -1 on error
 */
int mirror_language_match_phoneme(
    const mirror_language_bridge_t* bridge,
    const float* features,
    uint32_t num_features,
    uint8_t* best_phoneme,
    float* confidence
);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Set phoneme activation callback
 *
 * @param bridge Bridge instance
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int mirror_language_set_phoneme_callback(
    mirror_language_bridge_t* bridge,
    ml_phoneme_callback_t callback,
    void* user_data
);

/**
 * @brief Set binding event callback
 *
 * @param bridge Bridge instance
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int mirror_language_set_binding_callback(
    mirror_language_bridge_t* bridge,
    ml_binding_callback_t callback,
    void* user_data
);

/**
 * @brief Set simulation event callback
 *
 * @param bridge Bridge instance
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int mirror_language_set_simulation_callback(
    mirror_language_bridge_t* bridge,
    ml_simulation_callback_t callback,
    void* user_data
);

/**
 * @brief Set semantic context callback
 *
 * @param bridge Bridge instance
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int mirror_language_set_semantic_callback(
    mirror_language_bridge_t* bridge,
    ml_semantic_callback_t callback,
    void* user_data
);

//=============================================================================
// Status and Statistics
//=============================================================================

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge instance
 * @return Current bridge state
 */
ml_bridge_state_t mirror_language_get_state(
    const mirror_language_bridge_t* bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int mirror_language_get_stats(
    const mirror_language_bridge_t* bridge,
    mirror_language_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge instance
 */
void mirror_language_reset_stats(mirror_language_bridge_t* bridge);

/**
 * @brief Get configuration
 *
 * @param bridge Bridge instance
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int mirror_language_get_config(
    const mirror_language_bridge_t* bridge,
    mirror_language_config_t* config
);

/**
 * @brief Set configuration
 *
 * WHAT: Update bridge configuration
 * WHY:  Allow runtime parameter adjustment
 * HOW:  Validate and apply new configuration
 *
 * @param bridge Bridge instance
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int mirror_language_set_config(
    mirror_language_bridge_t* bridge,
    const mirror_language_config_t* config
);

/**
 * @brief Check if Broca is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool mirror_language_has_broca(const mirror_language_bridge_t* bridge);

/**
 * @brief Check if Wernicke is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool mirror_language_has_wernicke(const mirror_language_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_LANGUAGE_BRIDGE_H */
