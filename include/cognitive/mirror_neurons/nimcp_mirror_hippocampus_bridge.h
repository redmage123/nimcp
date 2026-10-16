/**
 * @file nimcp_mirror_hippocampus_bridge.h
 * @brief Mirror Neuron - Hippocampus Integration Bridge
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Bidirectional integration between mirror neurons and hippocampus
 * WHY:  Enable episodic storage of observed actions and context-based retrieval
 * HOW:  Store observed action sequences as episodes, retrieve for context/replay
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * MIRROR-HIPPOCAMPUS INTERACTION:
 * -------------------------------
 * 1. Observational Learning Memory:
 *    - Observed actions encoded as episodic memories
 *    - Action sequences stored with temporal structure
 *    - Social context (who performed) preserved
 *    - Reference: Iacoboni (2005) "Understanding others: Imitation, language,
 *      empathy"
 *
 * 2. Memory-Guided Action Understanding:
 *    - Past observations provide context for current
 *    - Similar actions retrieve related episodes
 *    - Enables prediction based on experience
 *    - Reference: Rizzolatti & Sinigaglia (2010) "The functional role of
 *      the parieto-frontal mirror circuit"
 *
 * 3. Sleep Consolidation of Observed Actions:
 *    - Observed action sequences replayed during sleep
 *    - Strengthens observation-execution mappings
 *    - Integrates into motor repertoire
 *    - Reference: Walker & Stickgold (2004) "Sleep-dependent motor memory plasticity"
 *
 * EPISODIC ACTION MEMORY:
 * -----------------------
 * 1. Encoding Pathway:
 *    - Mirror neuron activation -> hippocampal encoding
 *    - DG pattern separation for similar actions
 *    - CA3 stores action sequence associations
 *    - CA1 output to neocortex for consolidation
 *
 * 2. Retrieval Pathway:
 *    - Current observation -> pattern completion in CA3
 *    - Retrieve similar past observations
 *    - Provide context for action understanding
 *
 * 3. Replay Pathway:
 *    - Sleep/rest triggers replay of stored sequences
 *    - Forward replay for skill practice
 *    - Reverse replay for credit assignment
 *
 * ARCHITECTURE:
 * ```
 * +------------------+         +------------------+
 * |  MIRROR NEURONS  |         |   HIPPOCAMPUS    |
 * |                  |         |                  |
 * | [Observation] ---|-------->| [Encode Episode] |
 * | [Activation]  <--|---------|-- [Retrieve]     |
 * +--------+---------+         +--------+---------+
 *          |                            |
 *          v                            v
 * +--------------------------------------------------+
 * |         MIRROR-HIPPOCAMPUS BRIDGE                 |
 * |                                                   |
 * |  [Store Sequence] [Retrieve Context] [Replay]     |
 * +--------------------------------------------------+
 * ```
 *
 * Bio-async Module ID: 0x027D
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MIRROR_HIPPOCAMPUS_BRIDGE_H
#define NIMCP_MIRROR_HIPPOCAMPUS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Forward declaration for health agent */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Note: BIO_MODULE_MIRROR_HIPPOCAMPUS_BRIDGE is defined in nimcp_bio_messages.h */

/** @brief Maximum action sequence length for episodes */
#define MIRROR_HIPPO_MAX_SEQUENCE_LENGTH      32

/** @brief Maximum episodes that can be stored */
#define MIRROR_HIPPO_MAX_EPISODES             256

/** @brief Maximum retrievals per query */
#define MIRROR_HIPPO_MAX_RETRIEVALS           8

/** @brief Default encoding threshold */
#define MIRROR_HIPPO_ENCODE_THRESHOLD         0.5f

/** @brief Default retrieval similarity threshold */
#define MIRROR_HIPPO_RETRIEVAL_THRESHOLD      0.6f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mirror_hippocampus_bridge mirror_hippocampus_bridge_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Action observation record for episodic storage
 *
 * WHAT: Single observed action with context
 * WHY:  Atomic unit for episode construction
 */
typedef struct {
    uint32_t action_id;               /**< Action identifier */
    uint32_t agent_id;                /**< Who performed it */
    float activation;                 /**< Mirror neuron activation */
    float features[32];               /**< Action feature vector */
    uint32_t num_features;            /**< Number of features */
    uint64_t timestamp_ms;            /**< When observed */
} action_observation_t;

/**
 * @brief Observed action episode (sequence of actions)
 *
 * WHAT: Episodic memory of observed action sequence
 * WHY:  Store complete action sequences for replay and context
 */
typedef struct {
    uint32_t episode_id;              /**< Unique episode identifier */
    uint32_t hippocampus_memory_id;   /**< ID in hippocampus memory store */

    /* Action sequence */
    action_observation_t* actions;    /**< Sequence of actions */
    uint32_t num_actions;             /**< Number of actions in sequence */
    uint32_t max_actions;             /**< Maximum sequence capacity */

    /* Context information */
    uint32_t demonstrator_id;         /**< Agent who demonstrated */
    float social_salience;            /**< Social importance [0, 1] */
    float emotional_valence;          /**< Emotional significance [-1, 1] */

    /* Episode metadata */
    uint64_t start_time_ms;           /**< Episode start time */
    uint64_t end_time_ms;             /**< Episode end time */
    float episode_strength;           /**< Memory strength [0, 1] */
    bool is_consolidated;             /**< Transferred to cortex? */
    uint32_t replay_count;            /**< Times replayed */

    /* Retrieval metadata */
    uint64_t last_retrieval_ms;       /**< Last retrieval time */
    uint32_t retrieval_count;         /**< Total retrievals */
} action_episode_t;

/**
 * @brief Episode retrieval result
 *
 * WHAT: Result of searching for similar episodes
 * WHY:  Provide context from past observations
 */
typedef struct {
    action_episode_t* episodes[MIRROR_HIPPO_MAX_RETRIEVALS]; /**< Retrieved episodes */
    float similarities[MIRROR_HIPPO_MAX_RETRIEVALS];         /**< Similarity scores */
    uint32_t num_retrieved;           /**< Number retrieved */
    float best_similarity;            /**< Best match similarity */
} episode_retrieval_result_t;

/**
 * @brief Replay request for consolidation
 *
 * WHAT: Request to replay stored episodes
 * WHY:  Enable sleep-time consolidation
 */
typedef struct {
    uint32_t episode_id;              /**< Episode to replay (0 = auto-select) */
    bool reverse;                     /**< Replay in reverse order */
    float speed_factor;               /**< Replay speed multiplier (1.0 = real time) */
    float replay_strength;            /**< Strength of replay activation [0, 1] */
} replay_request_t;

/**
 * @brief Configuration for mirror-hippocampus bridge
 */
typedef struct {
    /* Encoding parameters */
    float encoding_threshold;         /**< Min activation for encoding (default: 0.5) */
    float sequence_gap_ms;            /**< Max gap between actions in sequence (default: 2000) */
    uint32_t min_sequence_length;     /**< Min actions for episode (default: 2) */
    bool encode_single_actions;       /**< Encode single actions too (default: false) */

    /* Retrieval parameters */
    float retrieval_threshold;        /**< Min similarity for retrieval (default: 0.6) */
    uint32_t max_retrievals;          /**< Max episodes to retrieve (default: 5) */
    bool enable_auto_retrieval;       /**< Auto-retrieve on observation (default: true) */

    /* Consolidation parameters */
    bool enable_replay;               /**< Enable replay during consolidation (default: true) */
    float consolidation_threshold;    /**< Min strength for consolidation (default: 0.7) */
    float replay_strengthening;       /**< Strength increase per replay (default: 0.1) */

    /* Memory parameters */
    bool enable_decay;                /**< Enable episode strength decay (default: true) */
    float decay_rate;                 /**< Decay rate per second (default: 0.001) */
    float emotional_boost;            /**< Emotional valence boost factor (default: 1.5) */

    /* Integration enables */
    bool enable_social_tagging;       /**< Tag episodes with social context (default: true) */
    bool enable_goal_association;     /**< Associate goals with episodes (default: true) */
} mirror_hippocampus_config_t;

/**
 * @brief Effects of mirror-hippocampus interaction
 */
typedef struct {
    /* Encoding effects */
    uint32_t episodes_encoded;        /**< Episodes encoded this cycle */
    uint32_t actions_stored;          /**< Actions stored this cycle */
    float avg_encoding_strength;      /**< Average encoding strength */

    /* Retrieval effects */
    uint32_t retrievals_performed;    /**< Retrievals this cycle */
    float avg_retrieval_similarity;   /**< Average retrieval similarity */
    bool context_retrieved;           /**< Context retrieved for current observation */

    /* Replay effects */
    uint32_t replays_performed;       /**< Replays this cycle */
    float replay_activation;          /**< Current replay activation level */
} mirror_hippocampus_effects_t;

/**
 * @brief Current state of mirror-hippocampus interaction
 */
typedef struct {
    /* Mirror neuron state */
    uint32_t active_observations;     /**< Currently active observations */
    float current_activation;         /**< Current max activation */
    bool sequence_in_progress;        /**< Building action sequence */

    /* Hippocampus state */
    uint32_t total_episodes;          /**< Total stored episodes */
    uint32_t consolidated_episodes;   /**< Episodes consolidated to cortex */
    float avg_episode_strength;       /**< Average episode strength */

    /* Bridge state */
    uint32_t current_sequence_length; /**< Length of current sequence */
    uint32_t pending_encoding;        /**< Episodes pending encoding */
    bool replay_active;               /**< Replay currently active */

    /* Performance */
    float encoding_rate;              /**< Encodings per second */
    float retrieval_rate;             /**< Retrievals per second */
} mirror_hippocampus_state_t;

/**
 * @brief Statistics for mirror-hippocampus bridge
 */
typedef struct {
    /* Encoding stats */
    uint64_t total_episodes_encoded;  /**< Total episodes encoded */
    uint64_t total_actions_stored;    /**< Total actions stored */
    float avg_sequence_length;        /**< Average sequence length */
    float avg_encoding_strength;      /**< Average encoding strength */

    /* Retrieval stats */
    uint64_t total_retrievals;        /**< Total retrieval operations */
    uint64_t successful_retrievals;   /**< Retrievals finding matches */
    float avg_retrieval_similarity;   /**< Average similarity score */

    /* Replay stats */
    uint64_t total_replays;           /**< Total replay operations */
    float avg_replay_strengthening;   /**< Average strength increase from replay */

    /* Consolidation stats */
    uint64_t episodes_consolidated;   /**< Episodes transferred to cortex */
    uint64_t episodes_forgotten;      /**< Episodes decayed below threshold */

    /* Timing stats */
    float avg_encoding_latency_ms;    /**< Average encoding time */
    float avg_retrieval_latency_ms;   /**< Average retrieval time */
} mirror_hippocampus_stats_t;

/* Forward declaration for health agent (B22 Upgrade) */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/**
 * @brief Mirror-hippocampus bridge state
 */
struct mirror_hippocampus_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    mirror_hippocampus_config_t config;

    /* Connected systems */
    mirror_neurons_t mirror;          /**< Mirror neuron system */
    hippocampus_adapter_t* hippocampus; /**< Hippocampus adapter */

    /* Episode storage */
    action_episode_t* episodes;       /**< Array of stored episodes */
    uint32_t num_episodes;            /**< Current episode count */
    uint32_t max_episodes;            /**< Maximum episodes */
    uint32_t next_episode_id;         /**< Next episode ID to assign */

    /* Current sequence being built */
    action_observation_t* current_sequence;
    uint32_t current_sequence_length;
    uint32_t current_sequence_max;
    uint64_t sequence_start_ms;
    uint64_t last_action_ms;
    uint32_t current_demonstrator;

    /* Replay state */
    bool replay_active;
    uint32_t replay_episode_id;
    uint32_t replay_position;
    float replay_speed;

    /* Current effects */
    mirror_hippocampus_effects_t effects;
    mirror_hippocampus_state_t state;

    /* Statistics */
    mirror_hippocampus_stats_t stats;

    /* Internal state */
    uint64_t last_update_ms;

    /* Health agent (instance-level) */
    nimcp_health_agent_t* health_agent;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default mirror-hippocampus configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds based on episodic memory properties
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int mirror_hippocampus_bridge_default_config(mirror_hippocampus_config_t* config);

/**
 * @brief Create mirror-hippocampus bridge
 *
 * WHAT: Initialize mirror neuron to hippocampus integration bridge
 * WHY:  Enable episodic storage and retrieval of observed actions
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
mirror_hippocampus_bridge_t* mirror_hippocampus_bridge_create(
    const mirror_hippocampus_config_t* config
);

/**
 * @brief Destroy mirror-hippocampus bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free episodes, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void mirror_hippocampus_bridge_destroy(mirror_hippocampus_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect mirror neuron system
 *
 * WHAT: Link bridge to mirror neuron system
 * WHY:  Source of observed action data
 * HOW:  Store mirror system handle for observation monitoring
 *
 * @param bridge Mirror-hippocampus bridge
 * @param mirror Mirror neuron system
 * @return 0 on success
 */
int mirror_hippocampus_bridge_connect_mirror(
    mirror_hippocampus_bridge_t* bridge,
    mirror_neurons_t mirror
);

/**
 * @brief Connect hippocampus adapter
 *
 * WHAT: Link bridge to hippocampus memory system
 * WHY:  Target for episode storage and retrieval
 * HOW:  Store hippocampus handle for memory operations
 *
 * @param bridge Mirror-hippocampus bridge
 * @param hippocampus Hippocampus adapter
 * @return 0 on success
 */
int mirror_hippocampus_bridge_connect_hippocampus(
    mirror_hippocampus_bridge_t* bridge,
    hippocampus_adapter_t* hippocampus
);

/* ============================================================================
 * Episode Encoding API
 * ============================================================================ */

/**
 * @brief Store observed action in current sequence
 *
 * WHAT: Add observed action to current episode sequence
 * WHY:  Build up action sequences for episodic encoding
 * HOW:  Append to current sequence, check for sequence completion
 *
 * @param bridge Mirror-hippocampus bridge
 * @param action Observed action
 * @return 0 on success, -1 on error
 */
int mirror_hippocampus_store_action(
    mirror_hippocampus_bridge_t* bridge,
    const action_t* action
);

/**
 * @brief Complete current action sequence as episode
 *
 * WHAT: Finalize current sequence and store as episode
 * WHY:  Commit action sequence to episodic memory
 * HOW:  Create episode, encode in hippocampus, reset sequence
 *
 * @param bridge Mirror-hippocampus bridge
 * @return Episode ID on success, UINT32_MAX on failure
 */
uint32_t mirror_hippocampus_complete_sequence(
    mirror_hippocampus_bridge_t* bridge
);

/**
 * @brief Store action demonstration as episode
 *
 * WHAT: Store complete demonstration directly as episode
 * WHY:  Efficient storage of pre-defined action sequences
 * HOW:  Create episode from provided actions
 *
 * @param bridge Mirror-hippocampus bridge
 * @param actions Array of actions
 * @param num_actions Number of actions
 * @param demonstrator_id Agent who demonstrated
 * @return Episode ID on success, UINT32_MAX on failure
 */
uint32_t mirror_hippocampus_store_demonstration(
    mirror_hippocampus_bridge_t* bridge,
    const action_t* actions,
    uint32_t num_actions,
    uint32_t demonstrator_id
);

/* ============================================================================
 * Episode Retrieval API
 * ============================================================================ */

/**
 * @brief Retrieve similar past observations
 *
 * WHAT: Find episodes similar to current observation
 * WHY:  Provide context from past observations
 * HOW:  Query hippocampus for similar patterns
 *
 * @param bridge Mirror-hippocampus bridge
 * @param cue Current observation as retrieval cue
 * @param result Output retrieval result
 * @return 0 on success
 */
int mirror_hippocampus_retrieve_by_action(
    mirror_hippocampus_bridge_t* bridge,
    const action_t* cue,
    episode_retrieval_result_t* result
);

/**
 * @brief Retrieve episodes by demonstrator
 *
 * WHAT: Find episodes demonstrated by specific agent
 * WHY:  Access agent-specific observation history
 * HOW:  Filter episodes by demonstrator ID
 *
 * @param bridge Mirror-hippocampus bridge
 * @param demonstrator_id Agent identifier
 * @param result Output retrieval result
 * @return 0 on success
 */
int mirror_hippocampus_retrieve_by_demonstrator(
    mirror_hippocampus_bridge_t* bridge,
    uint32_t demonstrator_id,
    episode_retrieval_result_t* result
);

/**
 * @brief Retrieve episodes with similar sequence
 *
 * WHAT: Find episodes with matching action sequence
 * WHY:  Identify recurring action patterns
 * HOW:  Sequence comparison using pattern completion
 *
 * @param bridge Mirror-hippocampus bridge
 * @param actions Action sequence to match
 * @param num_actions Sequence length
 * @param result Output retrieval result
 * @return 0 on success
 */
int mirror_hippocampus_retrieve_by_sequence(
    mirror_hippocampus_bridge_t* bridge,
    const action_t* actions,
    uint32_t num_actions,
    episode_retrieval_result_t* result
);

/**
 * @brief Get episode by ID
 *
 * WHAT: Direct access to stored episode
 * WHY:  Inspect specific episode details
 * HOW:  Look up by episode ID
 *
 * @param bridge Mirror-hippocampus bridge
 * @param episode_id Episode identifier
 * @param episode Output episode (pointer to internal storage)
 * @return 0 on success, -1 if not found
 */
int mirror_hippocampus_get_episode(
    const mirror_hippocampus_bridge_t* bridge,
    uint32_t episode_id,
    action_episode_t** episode
);

/* ============================================================================
 * Replay API (Consolidation)
 * ============================================================================ */

/**
 * @brief Request episode replay for consolidation
 *
 * WHAT: Initiate replay of stored episodes
 * WHY:  Strengthen memories during consolidation
 * HOW:  Re-activate mirror neurons with stored patterns
 *
 * @param bridge Mirror-hippocampus bridge
 * @param request Replay request parameters
 * @return 0 on success, -1 on error
 */
int mirror_hippocampus_request_replay(
    mirror_hippocampus_bridge_t* bridge,
    const replay_request_t* request
);

/**
 * @brief Step replay forward
 *
 * WHAT: Advance replay by one action
 * WHY:  Incremental replay for controlled consolidation
 * HOW:  Activate mirror neurons with next action in sequence
 *
 * @param bridge Mirror-hippocampus bridge
 * @param out_action Output: action being replayed (can be NULL)
 * @return 0 if replay continues, 1 if complete, -1 on error
 */
int mirror_hippocampus_step_replay(
    mirror_hippocampus_bridge_t* bridge,
    action_t* out_action
);

/**
 * @brief Stop current replay
 *
 * WHAT: Cancel ongoing replay
 * WHY:  Interrupt replay if needed
 * HOW:  Clear replay state
 *
 * @param bridge Mirror-hippocampus bridge
 * @return 0 on success
 */
int mirror_hippocampus_stop_replay(
    mirror_hippocampus_bridge_t* bridge
);

/**
 * @brief Check if replay is active
 *
 * @param bridge Mirror-hippocampus bridge
 * @return true if replay in progress
 */
bool mirror_hippocampus_is_replaying(
    const mirror_hippocampus_bridge_t* bridge
);

/**
 * @brief Trigger consolidation of strong episodes
 *
 * WHAT: Transfer strong episodes to hippocampus for long-term storage
 * WHY:  Systems consolidation for durable memory
 * HOW:  Replay and strengthen episodes above threshold
 *
 * @param bridge Mirror-hippocampus bridge
 * @param strength_threshold Minimum strength for consolidation
 * @return Number of episodes consolidated
 */
uint32_t mirror_hippocampus_consolidate(
    mirror_hippocampus_bridge_t* bridge,
    float strength_threshold
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update mirror-hippocampus bridge state
 *
 * WHAT: Main update loop for mirror-hippocampus integration
 * WHY:  Keep mirror neurons and hippocampus synchronized
 * HOW:  Process observations, manage sequences, decay strengths
 *
 * @param bridge Mirror-hippocampus bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int mirror_hippocampus_bridge_update(
    mirror_hippocampus_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Mirror-hippocampus bridge
 * @param state Output state
 * @return 0 on success
 */
int mirror_hippocampus_bridge_get_state(
    const mirror_hippocampus_bridge_t* bridge,
    mirror_hippocampus_state_t* state
);

/**
 * @brief Get bridge effects
 *
 * @param bridge Mirror-hippocampus bridge
 * @param effects Output effects
 * @return 0 on success
 */
int mirror_hippocampus_bridge_get_effects(
    const mirror_hippocampus_bridge_t* bridge,
    mirror_hippocampus_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Mirror-hippocampus bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int mirror_hippocampus_bridge_get_stats(
    const mirror_hippocampus_bridge_t* bridge,
    mirror_hippocampus_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Mirror-hippocampus bridge
 * @return 0 on success
 */
int mirror_hippocampus_bridge_reset_stats(
    mirror_hippocampus_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for mirror-hippocampus coordination
 * WHY:  Distributed episodic memory signaling
 * HOW:  Register module with ID 0x027D, set up handlers
 *
 * @param bridge Mirror-hippocampus bridge
 * @return 0 on success
 */
int mirror_hippocampus_bridge_connect_bio_async(
    mirror_hippocampus_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Mirror-hippocampus bridge
 * @return 0 on success
 */
int mirror_hippocampus_bridge_disconnect_bio_async(
    mirror_hippocampus_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Mirror-hippocampus bridge
 * @return true if bio-async enabled
 */
bool mirror_hippocampus_bridge_is_bio_async_connected(
    const mirror_hippocampus_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_HIPPOCAMPUS_BRIDGE_H */
