/**
 * @file nimcp_emotional_contagion.h
 * @brief Emotional contagion protocol for swarm agents
 *
 * WHAT: Spreads emotions across swarm agents via social transmission
 * WHY:  Enable collective emotional states and social coordination
 * HOW:  Network propagation with decay, susceptibility, and resistance
 *
 * BIOLOGICAL INSPIRATION:
 * - Mirror neuron systems: automatic emotional resonance with others
 * - Social contagion: emotions spread through observation and interaction
 * - Emotional regulation: individual differences in susceptibility/resistance
 * - Crowd psychology: collective emotional states emerge from local interactions
 *
 * KEY CONCEPTS:
 * 1. CONTAGION RATE: Speed of emotional transmission between agents
 * 2. DECAY RATE: How fast emotions fade without reinforcement
 * 3. SUSCEPTIBILITY: Individual vulnerability to emotional influence
 * 4. RESISTANCE: Acquired immunity to specific emotions over time
 * 5. PROPAGATION DEPTH: Maximum hops for emotional spread
 *
 * PROPAGATION MODEL:
 * ┌────────────────────────────────────────────────────────────┐
 * │  Agent A (sad) ───contagion───> Agent B (neutral)          │
 * │       │                               │                     │
 * │       │                               ↓                     │
 * │       │                        Agent B (slightly sad)       │
 * │       ↓                               │                     │
 * │  Emotion decays              ┌────────┴────────┐            │
 * │  over time                   ↓                 ↓            │
 * │                         Agent C           Agent D           │
 * └────────────────────────────────────────────────────────────┘
 *
 * TRANSMISSION EQUATION:
 * new_intensity = current + contagion_rate * source_intensity *
 *                 susceptibility * (1 - resistance) * proximity
 *
 * DECAY EQUATION:
 * intensity(t+dt) = intensity(t) * exp(-decay_rate * dt)
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_EMOTIONAL_CONTAGION_H
#define NIMCP_EMOTIONAL_CONTAGION_H

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of agents in emotional contagion network */
#define EMOTIONAL_CONTAGION_MAX_AGENTS 4096

/** Maximum connections per agent */
#define EMOTIONAL_CONTAGION_MAX_CONNECTIONS 256

#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Emotion types (based on basic emotions + cognitive states)
 */
typedef enum {
    EMOTION_NEUTRAL      = 0,   /**< Baseline neutral state */
    EMOTION_JOY          = 1,   /**< Happiness, pleasure */
    EMOTION_SADNESS      = 2,   /**< Sorrow, loss */
    EMOTION_ANGER        = 3,   /**< Rage, frustration */
    EMOTION_FEAR         = 4,   /**< Anxiety, threat */
    EMOTION_SURPRISE     = 5,   /**< Unexpected event */
    EMOTION_DISGUST      = 6,   /**< Aversion, rejection */
    EMOTION_TRUST        = 7,   /**< Confidence in others */
    EMOTION_ANTICIPATION = 8,   /**< Expectation, preparation */
    EMOTION_CURIOSITY    = 9,   /**< Interest, exploration */
    EMOTION_CALM         = 10,  /**< Relaxation, peace */
    EMOTION_EXCITEMENT   = 11,  /**< High arousal positive */
    EMOTION_FRUSTRATION  = 12,  /**< Blocked goal */
    EMOTION_HOPE         = 13,  /**< Positive expectation */
    EMOTION_DESPAIR      = 14,  /**< Negative expectation */
    EMOTION_PRIDE        = 15,  /**< Achievement satisfaction */
    EMOTION_SHAME        = 16,  /**< Social failure */
    EMOTION_GUILT        = 17,  /**< Moral transgression */
    EMOTION_ENVY         = 18,  /**< Desire for others' state */
    EMOTION_GRATITUDE    = 19,  /**< Thankfulness */
    EMOTION_TYPE_COUNT   = 20   /**< Total emotion types */
} emotion_type_t;

/**
 * @brief Opaque emotional contagion system handle
 */
typedef struct emotional_contagion emotional_contagion_t;

/**
 * @brief Configuration for emotional contagion system
 */
typedef struct {
    /* Contagion parameters */
    float contagion_rate;               /**< Transmission speed (0.0-1.0) */
    float decay_rate;                   /**< Emotion fade rate (0.0-1.0) */
    float susceptibility_threshold;     /**< Min susceptibility to be affected */
    uint32_t max_propagation_depth;     /**< Max hops for spread (0 = unlimited) */
    bool enable_resistance;             /**< Build resistance to repeated emotions */

    /* Network parameters */
    uint32_t max_agents;                /**< Maximum number of agents */
    uint32_t max_connections_per_agent; /**< Max neighbors per agent */
    float proximity_decay;              /**< Distance-based attenuation */

    /* Dynamics parameters */
    bool enable_emotional_dampening;    /**< Prevent runaway emotions */
    float dampening_threshold;          /**< Intensity for dampening */
    bool enable_refractory_period;      /**< Cooldown after strong emotion */
    uint64_t refractory_duration_ms;    /**< Refractory period duration */

    /* Resistance parameters */
    float resistance_buildup_rate;      /**< How fast resistance builds */
    float resistance_decay_rate;        /**< How fast resistance fades */
    float max_resistance;               /**< Maximum resistance level */

    /* Bio-async parameters */
    bool enable_bio_async;              /**< Enable bio-async integration */
    uint32_t broadcast_interval_ms;     /**< State broadcast interval */
} emotional_contagion_config_t;

/**
 * @brief Agent emotional state
 */
typedef struct {
    uint32_t agent_id;                  /**< Agent identifier */
    emotion_type_t emotion;             /**< Current primary emotion */
    float intensity;                    /**< Emotion intensity (0.0-1.0) */
    float susceptibility;               /**< Base susceptibility (0.0-1.0) */
    float resistance[EMOTION_TYPE_COUNT]; /**< Resistance per emotion type */
    float emotional_inertia;            /**< Resistance to change */
    uint64_t last_update_ms;            /**< Last emotion update time */
    bool in_refractory;                 /**< In refractory period */
    uint64_t refractory_until_ms;       /**< When refractory ends */
} agent_emotional_state_t;

/**
 * @brief Emotional connection between agents
 */
typedef struct {
    uint32_t from_agent;                /**< Source agent */
    uint32_t to_agent;                  /**< Target agent */
    float connection_strength;          /**< Connection weight (0.0-1.0) */
    float proximity;                    /**< Spatial proximity (0.0-1.0) */
    uint32_t interaction_count;         /**< Number of interactions */
    uint64_t last_interaction_ms;       /**< Last interaction time */
} emotional_connection_t;

/**
 * @brief Emotional propagation event
 */
typedef struct {
    uint32_t source_agent;              /**< Agent originating emotion */
    uint32_t affected_agent;            /**< Agent receiving emotion */
    emotion_type_t emotion;             /**< Emotion being transmitted */
    float source_intensity;             /**< Source emotion intensity */
    float transmitted_intensity;        /**< Intensity after transmission */
    float susceptibility_factor;        /**< Susceptibility contribution */
    float resistance_factor;            /**< Resistance contribution */
    uint32_t propagation_depth;         /**< Current depth in propagation */
    uint64_t timestamp_ms;              /**< When propagation occurred */
} emotional_propagation_event_t;

/**
 * @brief System statistics
 */
typedef struct {
    uint64_t total_propagations;        /**< Total propagation events */
    uint64_t successful_transmissions;  /**< Transmissions above threshold */
    uint64_t blocked_by_resistance;     /**< Blocked by high resistance */
    uint64_t blocked_by_susceptibility; /**< Blocked by low susceptibility */
    uint64_t refractory_blocks;         /**< Blocked by refractory period */
    float avg_transmission_intensity;   /**< Average transmitted intensity */
    uint32_t active_agents;             /**< Agents with non-neutral emotions */
    uint64_t bio_broadcasts_sent;       /**< Bio-async broadcasts */
    uint64_t collective_emotion_changes; /**< Dominant emotion changes */
} emotional_contagion_stats_t;

/**
 * @brief Collective emotional state
 */
typedef struct {
    emotion_type_t dominant_emotion;    /**< Most prevalent emotion */
    float dominant_intensity;           /**< Average intensity of dominant */
    float emotional_diversity;          /**< Shannon entropy of distribution */
    float emotional_coherence;          /**< How aligned agents are */
    float avg_intensity;                /**< Average intensity across all */
    uint32_t emotion_counts[EMOTION_TYPE_COUNT]; /**< Count per emotion */
} collective_emotion_state_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create emotional contagion system
 *
 * WHAT: Initializes emotional contagion system for swarm
 * WHY:  Enable emotional coordination between agents
 * HOW:  Allocates state, initializes network, sets parameters
 *
 * @param config Configuration parameters
 * @return System handle or NULL on failure
 *
 * COMPLEXITY: O(max_agents)
 * THREAD SAFETY: Not thread-safe during creation
 */
emotional_contagion_t* emotional_contagion_create(
    const emotional_contagion_config_t* config
);

/**
 * @brief Destroy emotional contagion system
 *
 * WHAT: Frees all system resources
 * WHY:  Clean shutdown
 * HOW:  Frees agent states, connections, internal buffers
 *
 * @param ec System to destroy (NULL-safe)
 */
void emotional_contagion_destroy(emotional_contagion_t* ec);

/**
 * @brief Reset system state
 *
 * WHAT: Resets all agents to neutral state
 * WHY:  Clean slate for new simulation
 * HOW:  Clears emotions, resets resistance, clears stats
 *
 * @param ec System handle
 * @param clear_connections Whether to clear connection network
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_reset(
    emotional_contagion_t* ec,
    bool clear_connections
);

/* ============================================================================
 * Agent Management Functions
 * ============================================================================ */

/**
 * @brief Register agent in system
 *
 * WHAT: Adds agent to contagion network
 * WHY:  Agent can participate in emotional propagation
 * HOW:  Allocates state, initializes to neutral with given susceptibility
 *
 * @param ec System handle
 * @param agent_id Unique agent identifier
 * @param susceptibility Base susceptibility (0.0-1.0)
 * @return NIMCP_SUCCESS or error code
 *
 * ERRORS:
 * - NIMCP_INVALID_PARAM if susceptibility out of range
 * - NIMCP_ALREADY_EXISTS if agent already registered
 * - NIMCP_BUFFER_FULL if max agents reached
 */
nimcp_result_t emotional_contagion_register_agent(
    emotional_contagion_t* ec,
    uint32_t agent_id,
    float susceptibility
);

/**
 * @brief Unregister agent from system
 *
 * WHAT: Removes agent from contagion network
 * WHY:  Agent no longer participates
 * HOW:  Removes connections, frees state
 *
 * @param ec System handle
 * @param agent_id Agent to remove
 * @return NIMCP_SUCCESS or NIMCP_NOT_FOUND
 */
nimcp_result_t emotional_contagion_unregister_agent(
    emotional_contagion_t* ec,
    uint32_t agent_id
);

/**
 * @brief Set agent's emotional state
 *
 * WHAT: Manually set or inject emotion into agent
 * WHY:  External events trigger emotions
 * HOW:  Updates agent state, triggers propagation
 *
 * @param ec System handle
 * @param agent_id Agent to affect
 * @param emotion Emotion type to set
 * @param intensity Emotion intensity (0.0-1.0)
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: This bypasses susceptibility/resistance checks
 */
nimcp_result_t emotional_contagion_set_emotion(
    emotional_contagion_t* ec,
    uint32_t agent_id,
    emotion_type_t emotion,
    float intensity
);

/**
 * @brief Get agent's emotional state
 *
 * WHAT: Query current emotional state
 * WHY:  Monitor individual agent emotions
 * HOW:  Copies state to output structure
 *
 * @param ec System handle
 * @param agent_id Agent to query
 * @param state Output state structure
 * @return NIMCP_SUCCESS or NIMCP_NOT_FOUND
 */
nimcp_result_t emotional_contagion_get_emotional_state(
    emotional_contagion_t* ec,
    uint32_t agent_id,
    agent_emotional_state_t* state
);

/**
 * @brief Update agent susceptibility
 *
 * WHAT: Modify agent's base susceptibility
 * WHY:  Agents can become more/less susceptible over time
 * HOW:  Updates susceptibility parameter
 *
 * @param ec System handle
 * @param agent_id Agent to update
 * @param susceptibility New susceptibility (0.0-1.0)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_set_susceptibility(
    emotional_contagion_t* ec,
    uint32_t agent_id,
    float susceptibility
);

/* ============================================================================
 * Connection Management Functions
 * ============================================================================ */

/**
 * @brief Add emotional connection between agents
 *
 * WHAT: Creates directed connection for emotion transmission
 * WHY:  Define social network topology
 * HOW:  Adds connection with strength and proximity
 *
 * @param ec System handle
 * @param from_agent Source agent
 * @param to_agent Target agent
 * @param connection_strength Connection weight (0.0-1.0)
 * @param proximity Spatial proximity (0.0-1.0, 1.0=adjacent)
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: Connections are directed, add both directions for bidirectional
 */
nimcp_result_t emotional_contagion_add_connection(
    emotional_contagion_t* ec,
    uint32_t from_agent,
    uint32_t to_agent,
    float connection_strength,
    float proximity
);

/**
 * @brief Remove connection between agents
 *
 * WHAT: Removes emotional transmission link
 * WHY:  Network topology changes
 * HOW:  Removes connection from adjacency list
 *
 * @param ec System handle
 * @param from_agent Source agent
 * @param to_agent Target agent
 * @return NIMCP_SUCCESS or NIMCP_NOT_FOUND
 */
nimcp_result_t emotional_contagion_remove_connection(
    emotional_contagion_t* ec,
    uint32_t from_agent,
    uint32_t to_agent
);

/**
 * @brief Update connection strength
 *
 * WHAT: Modify existing connection parameters
 * WHY:  Relationship strength changes over time
 * HOW:  Updates strength and proximity factors
 *
 * @param ec System handle
 * @param from_agent Source agent
 * @param to_agent Target agent
 * @param new_strength New connection strength (0.0-1.0)
 * @param new_proximity New proximity (0.0-1.0)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_update_connection(
    emotional_contagion_t* ec,
    uint32_t from_agent,
    uint32_t to_agent,
    float new_strength,
    float new_proximity
);

/* ============================================================================
 * Propagation Functions
 * ============================================================================ */

/**
 * @brief Propagate emotions across network
 *
 * WHAT: Updates all agent emotions via contagion
 * WHY:  Core emotional spreading algorithm
 * HOW:  For each agent, transmit emotion to neighbors with decay
 *
 * @param ec System handle
 * @param delta_ms Time elapsed since last update (milliseconds)
 * @return NIMCP_SUCCESS or error code
 *
 * ALGORITHM:
 * 1. For each agent with non-neutral emotion:
 *    a. For each connected neighbor:
 *       - Check susceptibility threshold
 *       - Check resistance level
 *       - Check refractory period
 *       - Compute transmitted intensity
 *       - Update neighbor emotion if above threshold
 * 2. Apply decay to all agent emotions
 * 3. Update resistance levels
 * 4. Clear refractory periods if expired
 *
 * COMPLEXITY: O(agents * avg_connections)
 * THREAD SAFETY: Not thread-safe (use external locking)
 */
nimcp_result_t emotional_contagion_propagate(
    emotional_contagion_t* ec,
    uint64_t delta_ms
);

/**
 * @brief Trigger emotional outbreak from single agent
 *
 * WHAT: Breadth-first propagation from source
 * WHY:  Simulate strong emotional event
 * HOW:  BFS with intensity decay per hop
 *
 * @param ec System handle
 * @param source_agent Agent triggering outbreak
 * @param emotion Emotion to spread
 * @param initial_intensity Starting intensity
 * @param max_depth Maximum propagation depth (0=use config)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_trigger_outbreak(
    emotional_contagion_t* ec,
    uint32_t source_agent,
    emotion_type_t emotion,
    float initial_intensity,
    uint32_t max_depth
);

/**
 * @brief Apply emotion decay to all agents
 *
 * WHAT: Reduces emotion intensity over time
 * WHY:  Emotions fade without reinforcement
 * HOW:  Exponential decay based on time
 *
 * @param ec System handle
 * @param delta_ms Time elapsed
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_apply_decay(
    emotional_contagion_t* ec,
    uint64_t delta_ms
);

/* ============================================================================
 * Collective State Functions
 * ============================================================================ */

/**
 * @brief Get dominant emotion across swarm
 *
 * WHAT: Identifies most prevalent emotion
 * WHY:  Monitor collective emotional state
 * HOW:  Counts emotions, returns most frequent
 *
 * @param ec System handle
 * @param emotion Output dominant emotion
 * @param avg_intensity Output average intensity
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_get_dominant_emotion(
    emotional_contagion_t* ec,
    emotion_type_t* emotion,
    float* avg_intensity
);

/**
 * @brief Get emotional diversity metric
 *
 * WHAT: Measures variety of emotions in swarm
 * WHY:  Assess emotional heterogeneity
 * HOW:  Computes Shannon entropy of emotion distribution
 *
 * @param ec System handle
 * @param diversity Output diversity score (0.0-1.0)
 * @return NIMCP_SUCCESS or error code
 *
 * INTERPRETATION:
 * - 0.0: All agents have same emotion (homogeneous)
 * - 1.0: Emotions uniformly distributed (heterogeneous)
 */
nimcp_result_t emotional_contagion_get_emotional_diversity(
    emotional_contagion_t* ec,
    float* diversity
);

/**
 * @brief Get full collective emotional state
 *
 * WHAT: Comprehensive swarm emotional snapshot
 * WHY:  Detailed collective state monitoring
 * HOW:  Aggregates all metrics into single structure
 *
 * @param ec System handle
 * @param state Output collective state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_get_collective_state(
    emotional_contagion_t* ec,
    collective_emotion_state_t* state
);

/**
 * @brief Get emotional coherence
 *
 * WHAT: Measures alignment of agent emotions
 * WHY:  Assess emotional synchronization
 * HOW:  Correlation of emotional vectors
 *
 * @param ec System handle
 * @param coherence Output coherence (0.0-1.0)
 * @return NIMCP_SUCCESS or error code
 *
 * INTERPRETATION:
 * - 0.0: Completely uncorrelated emotions
 * - 1.0: Perfect emotional alignment
 */
nimcp_result_t emotional_contagion_get_coherence(
    emotional_contagion_t* ec,
    float* coherence
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Register with bio-async router
 *
 * WHAT: Integrates into bio-async messaging system
 * WHY:  Enable async emotion broadcasts
 * HOW:  Registers handlers for emotion messages
 *
 * @param ec System handle
 * @param router Bio-async router
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_register_bioasync(
    emotional_contagion_t* ec,
    bio_router_t* router
);

/**
 * @brief Broadcast emotional state change
 *
 * WHAT: Sends BIO_MSG_EMOTION_SPREAD message
 * WHY:  Notify system of emotion propagation
 * HOW:  Packages event into bio-async message (serotonin channel)
 *
 * @param ec System handle
 * @param event Propagation event to broadcast
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_broadcast_spread(
    emotional_contagion_t* ec,
    const emotional_propagation_event_t* event
);

/**
 * @brief Broadcast collective emotion change
 *
 * WHAT: Sends BIO_MSG_COLLECTIVE_EMOTION message
 * WHY:  Notify of swarm-level emotional shift
 * HOW:  Packages collective state (serotonin channel)
 *
 * @param ec System handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_broadcast_collective(
    emotional_contagion_t* ec
);

/**
 * @brief Handle incoming emotion message
 *
 * WHAT: Bio-async handler for emotion messages
 * WHY:  Process external emotion triggers
 * HOW:  Unpacks message, updates agent state
 *
 * @param ec System handle
 * @param message Bio-async message
 * @param msg_size Message size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_handle_message(
    emotional_contagion_t* ec,
    const void* message,
    size_t msg_size
);

/* ============================================================================
 * Statistics and Utilities
 * ============================================================================ */

/**
 * @brief Get system statistics
 *
 * WHAT: Retrieves accumulated statistics
 * WHY:  Monitor system performance
 * HOW:  Copies internal stats to output
 *
 * @param ec System handle
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_get_stats(
    emotional_contagion_t* ec,
    emotional_contagion_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Clears accumulated statistics
 * WHY:  Start fresh monitoring period
 * HOW:  Zeros statistics structure
 *
 * @param ec System handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t emotional_contagion_reset_stats(emotional_contagion_t* ec);

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Quick setup without manual tuning
 * HOW:  Fills config with biologically-inspired defaults
 *
 * @param out_config Output configuration structure
 */
void emotional_contagion_get_default_config(emotional_contagion_config_t* out_config);

/**
 * @brief Validate configuration
 *
 * WHAT: Checks configuration parameters for validity
 * WHY:  Catch errors before initialization
 * HOW:  Validates ranges, consistency
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid, error code otherwise
 */
nimcp_result_t emotional_contagion_validate_config(
    const emotional_contagion_config_t* config
);

/**
 * @brief Get emotion type name
 *
 * WHAT: Returns string name for emotion
 * WHY:  Logging and debugging
 * HOW:  Lookup table
 *
 * @param emotion Emotion type
 * @return String name
 */
const char* emotional_contagion_emotion_name(emotion_type_t emotion);

/**
 * @brief Get emotion from name
 *
 * WHAT: Parse emotion from string
 * WHY:  Configuration and scripting
 * HOW:  String comparison
 *
 * @param name Emotion name (case-insensitive)
 * @return Emotion type or EMOTION_NEUTRAL if not found
 */
emotion_type_t emotional_contagion_emotion_from_name(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTIONAL_CONTAGION_H */
