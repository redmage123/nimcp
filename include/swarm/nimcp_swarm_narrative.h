/**
 * @file nimcp_swarm_narrative.h
 * @brief Swarm Narrative Memory System for NIMCP
 *
 * WHAT: Store and share narratives (sequences of events) across swarm agents
 * WHY:  Enable agents to share experiences, build collective memory, and learn from others' stories
 * HOW:  Neural-encoded event sequences with emotional valence, coherence scoring, and gossip-based sharing
 *
 * BIOLOGICAL INSPIRATION: Social learning and narrative memory in humans and primates
 *
 * Narratives are sequences of events that tell stories. In biological systems, narrative memory
 * enables social learning, culture transmission, and collective intelligence. This module implements
 * a distributed narrative memory system where agents can:
 * - Create narratives from event sequences
 * - Share narratives with other agents (unicast/broadcast)
 * - Query narratives by topic/similarity
 * - Track narrative popularity and diffusion
 *
 * KEY FEATURES:
 * - Neural encoding of events (floating-point vectors)
 * - Emotional valence tracking (-1.0 to 1.0)
 * - Importance weighting (0.0 to 1.0)
 * - Coherence scoring for narrative quality
 * - Share counting for popularity tracking
 * - Topic-based retrieval with similarity matching
 * - Optional compression for efficient transmission
 * - Full bio-async integration for swarm communication
 *
 * @version 1.0
 * @date 2025
 */

#ifndef NIMCP_SWARM_NARRATIVE_H
#define NIMCP_SWARM_NARRATIVE_H

#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/time/nimcp_time.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/thread/nimcp_thread.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct swarm_narrative swarm_narrative_t;

/* ============================================================================
 * Event and Narrative Structures
 * ============================================================================ */

/**
 * @brief Individual event in a narrative
 *
 * WHAT: Represents a single experience/event with neural encoding and emotional context
 * WHY:  Events are the building blocks of narratives, capturing what happened, when, and how it felt
 * HOW:  Neural vector encoding with emotional valence and importance weights
 */
typedef struct {
    uint32_t event_id;              /**< Unique event identifier */
    uint32_t agent_id;              /**< Agent who experienced the event */
    uint64_t timestamp_ms;          /**< When the event occurred */
    float* event_encoding;          /**< Neural representation of event */
    uint32_t encoding_size;         /**< Size of encoding vector */
    float emotional_valence;        /**< Emotional tone: -1.0 (negative) to 1.0 (positive) */
    float importance;               /**< Event importance: 0.0 (trivial) to 1.0 (critical) */
} narrative_event_t;

/**
 * @brief Complete narrative (story) composed of event sequence
 *
 * WHAT: A coherent sequence of events that tells a story
 * WHY:  Narratives enable social learning and cultural transmission across the swarm
 * HOW:  Ordered event list with coherence scoring and sharing metrics
 */
typedef struct {
    uint32_t narrative_id;          /**< Unique narrative identifier */
    narrative_event_t* events;      /**< Array of events in the narrative */
    uint32_t num_events;            /**< Number of events in narrative */
    uint32_t teller_agent_id;       /**< Agent telling/sharing the story */
    float coherence_score;          /**< How coherent the narrative is (0.0-1.0) */
    uint32_t share_count;           /**< Number of times shared (popularity metric) */
} narrative_t;

/**
 * @brief Configuration for narrative memory system
 *
 * WHAT: Tunable parameters for narrative storage and sharing behavior
 * WHY:  Allow customization of memory capacity, quality thresholds, and features
 * HOW:  Configuration structure passed at creation time
 */
typedef struct {
    uint32_t max_narratives;            /**< Maximum narratives to store */
    uint32_t max_events_per_narrative;  /**< Maximum events per narrative */
    float coherence_threshold;          /**< Minimum coherence to store (0.0-1.0) */
    bool enable_compression;            /**< Compress narratives for transmission */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} swarm_narrative_config_t;

/* ============================================================================
 * Core API Functions
 * ============================================================================ */

/**
 * @brief Create a new swarm narrative memory system
 *
 * WHAT: Allocates and initializes the narrative memory system
 * WHY:  Required before any narrative operations can be performed
 * HOW:  Allocates structures, creates hash tables, initializes state
 *
 * @param config System configuration (required)
 * @return Pointer to created system, or NULL on failure
 */
swarm_narrative_t* swarm_narrative_create(const swarm_narrative_config_t* config);

/**
 * @brief Destroy narrative memory system and free all resources
 *
 * WHAT: Cleanly deallocates all memory and resources
 * WHY:  Prevent memory leaks and ensure proper cleanup
 * HOW:  Frees narratives, events, hash tables, and main structure
 *
 * @param sn Narrative system to destroy (NULL-safe)
 */
void swarm_narrative_destroy(swarm_narrative_t* sn);

/**
 * @brief Initialize narrative system with optional bio-async context
 *
 * WHAT: Sets up bio-async messaging and registers message handlers
 * WHY:  Enable swarm communication for narrative sharing
 * HOW:  Registers with bio-router, sets up inbox processing
 *
 * @param sn Narrative system
 * @param bio_ctx Bio-async context (optional, can be NULL)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_narrative_init(swarm_narrative_t* sn, void* bio_ctx);

/* ============================================================================
 * Narrative Creation API
 * ============================================================================ */

/**
 * @brief Begin creating a new narrative
 *
 * WHAT: Starts a new narrative recording session
 * WHY:  Provides a transaction-like interface for building narratives
 * HOW:  Allocates narrative structure, returns ID for adding events
 *
 * @param sn Narrative system
 * @param teller_id Agent ID of the storyteller
 * @param narrative_id Output: assigned narrative ID
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int swarm_narrative_begin(swarm_narrative_t* sn, uint32_t teller_id, uint32_t* narrative_id);

/**
 * @brief Add an event to a narrative being constructed
 *
 * WHAT: Appends an event to the narrative event sequence
 * WHY:  Build up the narrative story incrementally
 * HOW:  Copies event data into narrative's event array
 *
 * @param sn Narrative system
 * @param narrative_id Narrative being constructed
 * @param event Event to add (copied into narrative)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int swarm_narrative_add_event(swarm_narrative_t* sn, uint32_t narrative_id,
                               const narrative_event_t* event);

/**
 * @brief Finalize narrative construction
 *
 * WHAT: Completes the narrative, calculates coherence, and stores it
 * WHY:  Signal that the narrative is complete and ready to share
 * HOW:  Calculates coherence score, validates threshold, stores in hash table
 *
 * @param sn Narrative system
 * @param narrative_id Narrative to finalize
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int swarm_narrative_end(swarm_narrative_t* sn, uint32_t narrative_id);

/* ============================================================================
 * Narrative Sharing API
 * ============================================================================ */

/**
 * @brief Share a narrative with specific target agents
 *
 * WHAT: Send narrative to selected agents via bio-async
 * WHY:  Enable targeted narrative sharing based on relevance
 * HOW:  Compresses (if enabled) and sends via bio-router to each target
 *
 * @param sn Narrative system
 * @param narrative_id Narrative to share
 * @param target_agents Array of target agent IDs
 * @param num_targets Number of target agents
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int swarm_narrative_share(swarm_narrative_t* sn, uint32_t narrative_id,
                           const uint32_t* target_agents, uint32_t num_targets);

/**
 * @brief Broadcast a narrative to all agents in the swarm
 *
 * WHAT: Share narrative with entire swarm
 * WHY:  Distribute important narratives widely for collective learning
 * HOW:  Sends via bio-router broadcast mechanism
 *
 * @param sn Narrative system
 * @param narrative_id Narrative to broadcast
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int swarm_narrative_broadcast(swarm_narrative_t* sn, uint32_t narrative_id);

/* ============================================================================
 * Narrative Retrieval API
 * ============================================================================ */

/**
 * @brief Query narratives by topic similarity
 *
 * WHAT: Find narratives similar to a topic vector
 * WHY:  Enable content-based narrative retrieval
 * HOW:  Computes cosine similarity between topic and event encodings
 *
 * @param sn Narrative system
 * @param topic_vector Topic query vector
 * @param vec_size Size of topic vector
 * @param results Output: array of matching narratives (allocated by caller)
 * @param count Output: number of results found
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int swarm_narrative_query_by_topic(swarm_narrative_t* sn, const float* topic_vector,
                                    uint32_t vec_size, narrative_t** results, uint32_t* count);

/**
 * @brief Get most popular narratives (by share count)
 *
 * WHAT: Retrieve narratives sorted by sharing frequency
 * WHY:  Identify culturally important narratives in the swarm
 * HOW:  Sorts by share_count descending, returns top N
 *
 * @param sn Narrative system
 * @param results Output: array of popular narratives (allocated by caller)
 * @param count Output: number of results found
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int swarm_narrative_get_popular(swarm_narrative_t* sn, narrative_t** results, uint32_t* count);

/**
 * @brief Get a specific narrative by ID
 *
 * WHAT: Retrieve narrative by identifier
 * WHY:  Direct access to known narratives
 * HOW:  Hash table lookup by ID
 *
 * @param sn Narrative system
 * @param narrative_id Narrative identifier
 * @param result Output: pointer to narrative (do not free)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int swarm_narrative_get(swarm_narrative_t* sn, uint32_t narrative_id, narrative_t** result);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Process incoming bio-async message
 *
 * WHAT: Handle received narrative sharing messages
 * WHY:  Enable narrative reception from other agents
 * HOW:  Parses message, deserializes narrative, stores locally
 *
 * @param sn Narrative system
 * @param msg Incoming message
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t swarm_narrative_process_message(swarm_narrative_t* sn, const void* msg);

/**
 * @brief Process inbox messages (call periodically)
 *
 * WHAT: Process pending narrative messages from bio-async inbox
 * WHY:  Receive and integrate shared narratives from swarm
 * HOW:  Polls inbox, processes each message, updates local narrative store
 *
 * @param sn Narrative system
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t swarm_narrative_process_inbox(swarm_narrative_t* sn, uint32_t max_messages);

/* ============================================================================
 * Statistics and Monitoring API
 * ============================================================================ */

/**
 * @brief Get narrative system statistics
 *
 * WHAT: Retrieve metrics about narrative storage and sharing
 * WHY:  Monitor system health and usage patterns
 * HOW:  Returns counts, averages, and state information
 *
 * @param sn Narrative system
 * @param total_narratives Output: total narratives stored
 * @param total_events Output: total events across all narratives
 * @param avg_coherence Output: average coherence score
 * @param total_shares Output: total sharing operations
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int swarm_narrative_get_stats(swarm_narrative_t* sn, uint32_t* total_narratives,
                               uint32_t* total_events, float* avg_coherence,
                               uint32_t* total_shares);

/**
 * @brief Print narrative system status to log
 *
 * WHAT: Log current system state and statistics
 * WHY:  Debug and monitor narrative memory system
 * HOW:  Logs all key metrics and configuration
 *
 * @param sn Narrative system
 * @param verbose Include detailed information
 */
void swarm_narrative_print_status(const swarm_narrative_t* sn, bool verbose);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Create a narrative event
 *
 * WHAT: Helper to construct a narrative event
 * WHY:  Simplify event creation with proper initialization
 * HOW:  Allocates and initializes event structure
 *
 * @param agent_id Agent who experienced the event
 * @param encoding Neural encoding of event
 * @param encoding_size Size of encoding
 * @param emotional_valence Emotional tone (-1.0 to 1.0)
 * @param importance Event importance (0.0 to 1.0)
 * @return Allocated event (caller must free encoding and struct)
 */
narrative_event_t* narrative_event_create(uint32_t agent_id, const float* encoding,
                                           uint32_t encoding_size, float emotional_valence,
                                           float importance);

/**
 * @brief Destroy a narrative event
 *
 * WHAT: Free event and its encoding
 * WHY:  Proper cleanup of event resources
 * HOW:  Frees encoding array and event struct
 *
 * @param event Event to destroy (NULL-safe)
 */
void narrative_event_destroy(narrative_event_t* event);

/**
 * @brief Calculate narrative coherence score
 *
 * WHAT: Compute how coherent/connected a narrative is
 * WHY:  Quality metric for narrative filtering
 * HOW:  Analyzes temporal consistency, emotional flow, encoding similarity
 *
 * @param narrative Narrative to analyze
 * @return Coherence score (0.0 = incoherent, 1.0 = highly coherent)
 */
float narrative_calculate_coherence(const narrative_t* narrative);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_NARRATIVE_H */
