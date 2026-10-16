/**
 * @file nimcp_collective_cognition.h
 * @brief Unified collective cognition system for distributed consciousness
 *
 * WHAT: Integrates hyperscanning, extended mind, collective phi, and shared intentionality
 * WHY: Enable distributed consciousness across multiple NIMCP brain instances
 * HOW: Unified API coordinating all collective cognition subsystems
 *
 * THEORETICAL BASIS:
 * - Distributed Consciousness: Consciousness as shared/networked phenomenon
 * - Integrated Information Theory (IIT): Phi as measure of integrated information
 * - Extended Mind (Clark & Chalmers): External tools as cognitive extensions
 * - Shared Intentionality (Tomasello): Joint goals and collective intentions
 * - Hyperscanning: Real-time neural synchronization between brains
 *
 * ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                 Collective Cognition System                      │
 * ├─────────────────────────────────────────────────────────────────┤
 * │ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ │
 * │ │Hyperscanning│ │Extended Mind│ │Collective   │ │   Shared    │ │
 * │ │   System    │ │   System    │ │    Phi      │ │Intentionality│ │
 * │ └──────┬──────┘ └──────┬──────┘ └──────┬──────┘ └──────┬──────┘ │
 * │        └───────────────┴───────────────┴───────────────┘        │
 * │                          │                                       │
 * │              ┌───────────┴───────────┐                          │
 * │              │    Bio-Async Router   │                          │
 * │              └───────────┬───────────┘                          │
 * │                          │                                       │
 * │              ┌───────────┴───────────┐                          │
 * │              │   Message Protocol    │                          │
 * │              │ (Hybrid: Fast + Proto)│                          │
 * │              └───────────────────────┘                          │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#ifndef NIMCP_COLLECTIVE_COGNITION_H
#define NIMCP_COLLECTIVE_COGNITION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations - use same definitions as nimcp_brain.h and nimcp_bio_router.h */
struct brain_struct;
typedef struct brain_struct* brain_t;
struct bio_router_struct;
typedef struct bio_router_struct* bio_router_t;
struct neural_substrate;
typedef struct neural_substrate neural_substrate_t;

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Bio-async module IDs for collective cognition */
#define BIO_MODULE_COLLECTIVE_COGNITION         0x1220
#define BIO_MODULE_HYPERSCANNING                0x1221
#define BIO_MODULE_EXTENDED_MIND                0x1222
#define BIO_MODULE_COLLECTIVE_PHI               0x1223
#define BIO_MODULE_SHARED_INTENTIONALITY        0x1224
#define BIO_MODULE_SUBSTRATE_COLLECTIVE         0x1225
#define BIO_MODULE_THALAMIC_COLLECTIVE          0x1226
#define BIO_MODULE_FEP_COLLECTIVE               0x1227

/** Maximum instances in a collective */
#define COLLECTIVE_MAX_INSTANCES                16

/** Maximum shared goals */
#define COLLECTIVE_MAX_SHARED_GOALS             32

/** Maximum joint attention targets */
#define COLLECTIVE_MAX_JOINT_ATTENTIONS         16

/** Maximum cognitive extensions */
#define COLLECTIVE_MAX_EXTENSIONS               32

/*=============================================================================
 * Consciousness Level Classification
 *===========================================================================*/

/**
 * @brief Collective consciousness level based on phi
 */
typedef enum {
    COLLECTIVE_CONSCIOUSNESS_NONE = 0,        /**< phi < 0.1 */
    COLLECTIVE_CONSCIOUSNESS_MINIMAL,         /**< 0.1 <= phi < 0.3 */
    COLLECTIVE_CONSCIOUSNESS_EMERGING,        /**< 0.3 <= phi < 0.5 */
    COLLECTIVE_CONSCIOUSNESS_PARTIAL,         /**< 0.5 <= phi < 0.7 */
    COLLECTIVE_CONSCIOUSNESS_UNIFIED,         /**< 0.7 <= phi < 0.9 */
    COLLECTIVE_CONSCIOUSNESS_TRANSCENDENT     /**< phi >= 0.9 */
} collective_consciousness_level_t;

/*=============================================================================
 * Hyperscanning Types
 *===========================================================================*/

/**
 * @brief EEG-like frequency bands for synchronization
 */
typedef enum {
    SYNC_BAND_DELTA = 0,      /**< 0.5-4 Hz: Deep coordination, empathy */
    SYNC_BAND_THETA,          /**< 4-8 Hz: Memory, emotional bonding */
    SYNC_BAND_ALPHA,          /**< 8-13 Hz: Relaxed attention, inhibition */
    SYNC_BAND_BETA,           /**< 13-30 Hz: Active thinking, motor planning */
    SYNC_BAND_GAMMA,          /**< 30-100 Hz: Binding, consciousness */
    SYNC_BAND_COUNT
} sync_band_t;

/**
 * @brief Hyperscanning state between instances
 */
typedef struct {
    float global_sync;              /**< Overall synchronization [0-1] */
    float gamma_binding;            /**< Gamma-band binding strength [0-1] */
    float theta_emotional;          /**< Theta-band emotional sync [0-1] */
    float beta_coordination;        /**< Beta-band motor coordination [0-1] */
    bool is_entrained;              /**< True if strongly synchronized */
    uint32_t leader_instance_id;    /**< Currently leading instance */
    float leader_influence;         /**< Leader's influence [0-1] */
} hyperscan_state_t;

/*=============================================================================
 * Extended Mind Types
 *===========================================================================*/

/**
 * @brief Types of cognitive extensions
 */
typedef enum {
    EXT_TYPE_MEMORY = 0,        /**< External memory (databases, files) */
    EXT_TYPE_PERCEPTION,        /**< External sensors (cameras, APIs) */
    EXT_TYPE_REASONING,         /**< External reasoning (LLMs, calculators) */
    EXT_TYPE_ACTION,            /**< External effectors (robots, APIs) */
    EXT_TYPE_COMMUNICATION      /**< External communication (messaging) */
} extension_type_t;

/**
 * @brief Extended mind state
 */
typedef struct {
    float total_cognitive_capacity;  /**< Local + extended capacity [0-2] */
    float extended_ratio;            /**< Fraction from extensions [0-1] */
    float integration_quality;       /**< How well integrated [0-1] */
    uint32_t active_extensions;      /**< Currently available count */
    uint32_t degraded_extensions;    /**< Available but slow/unreliable */
} extended_mind_state_t;

/*=============================================================================
 * Collective Phi Types
 *===========================================================================*/

/**
 * @brief Integrated Information Theory metrics
 */
typedef struct {
    float phi_local;            /**< Sum of individual brain phis */
    float phi_network;          /**< Inter-brain integration phi */
    float phi_total;            /**< Combined collective phi */

    /* IIT decomposition */
    float information;          /**< Shannon information content */
    float integration;          /**< Irreducibility of the whole */
    float exclusion;            /**< Definiteness of boundaries */

    /* Network topology effects */
    float connectivity;         /**< Graph connectivity measure */
    float modularity;           /**< Clustering coefficient */
    float small_world_index;    /**< Small-world network measure */
} collective_phi_t;

/*=============================================================================
 * Shared Intentionality Types
 *===========================================================================*/

/**
 * @brief State of a shared goal
 */
typedef enum {
    GOAL_STATE_PROPOSED = 0,
    GOAL_STATE_NEGOTIATING,
    GOAL_STATE_ACCEPTED,
    GOAL_STATE_ACTIVE,
    GOAL_STATE_COMPLETED,
    GOAL_STATE_ABANDONED,
    GOAL_STATE_FAILED
} shared_goal_state_t;

/**
 * @brief We-mode state (Tomasello's shared intentionality)
 */
typedef struct {
    float we_mode_strength;         /**< Strength of "we" identification [0-1] */
    float joint_commitment;         /**< Collective commitment level [0-1] */
    float mutual_responsiveness;    /**< Responsiveness to each other [0-1] */
    float role_understanding;       /**< Understanding of roles [0-1] */
    uint32_t active_shared_goals;   /**< Number of active shared goals */
    uint32_t active_joint_attentions; /**< Number of active joint attentions */
} we_mode_state_t;

/*=============================================================================
 * Aggregate State Types
 *===========================================================================*/

/**
 * @brief Complete collective cognition state
 */
typedef struct {
    /* Component states */
    hyperscan_state_t hyperscanning;
    extended_mind_state_t extended_mind;
    collective_phi_t phi;
    we_mode_state_t we_mode;

    /* Aggregate metrics */
    float collective_capacity;       /**< Total cognitive capacity [0-2] */
    float integration_quality;       /**< How well integrated [0-1] */
    float consciousness_level;       /**< Overall consciousness [0-1] */

    /* Flow dynamics */
    float information_flow_rate;     /**< Bits/second across network */
    float attention_coherence;       /**< Shared attention alignment [0-1] */
    float goal_alignment;            /**< Shared goal agreement [0-1] */

    /* Health indicators */
    bool is_fragmented;              /**< Network fragmentation detected */
    bool is_overloaded;              /**< Cognitive overload detected */
    uint32_t active_instances;       /**< Number of connected instances */

    /* Timestamp */
    uint64_t last_update_us;         /**< Last update timestamp */
} collective_cognition_state_t;

/*=============================================================================
 * Configuration Types
 *===========================================================================*/

/**
 * @brief Hyperscanning configuration
 */
typedef struct {
    uint32_t max_instances;         /**< Max tracked instances */
    float sync_threshold;           /**< Entrainment threshold [0-1] */
    uint32_t sample_rate_hz;        /**< Neural state sampling rate */
    bool enable_leader_detection;   /**< Detect leader-follower dynamics */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} hyperscanning_config_t;

/**
 * @brief Extended mind configuration
 */
typedef struct {
    uint32_t max_extensions;         /**< Max cognitive extensions */
    float trust_decay_rate;          /**< Trust decay per failure */
    float integration_threshold;     /**< Min reliability for integration */
    bool enable_automatic_offload;   /**< Auto-offload to extensions */
    bool enable_bio_async;
} extended_mind_config_t;

/**
 * @brief Collective phi configuration
 */
typedef struct {
    uint32_t aggregation_method;     /**< SUM, AVG, GEOMETRIC, SYNERGISTIC */
    float synergy_coefficient;       /**< For synergistic aggregation */
    uint32_t min_instances_for_phi;  /**< Min instances for meaningful phi */
    float coherence_weight;          /**< Weight of coherence in phi */
    bool enable_network_topology;    /**< Include topology in calculation */
} collective_phi_config_t;

/**
 * @brief Shared intentionality configuration
 */
typedef struct {
    uint32_t max_shared_goals;
    uint32_t max_joint_attentions;
    float commitment_threshold;      /**< Min commitment for acceptance */
    float we_mode_threshold;         /**< Threshold for we-mode activation */
    bool enable_role_negotiation;
    bool enable_bio_async;
} shared_intentionality_config_t;

/**
 * @brief Main collective cognition configuration
 */
typedef struct {
    hyperscanning_config_t hyperscanning;
    extended_mind_config_t extended_mind;
    collective_phi_config_t phi;
    shared_intentionality_config_t intentionality;

    /* Global settings */
    uint32_t max_instances;          /**< Max brain instances */
    float fragmentation_threshold;   /**< Threshold for fragmentation alert */
    float overload_threshold;        /**< Threshold for overload alert */
    bool enable_auto_balancing;      /**< Auto-distribute cognitive load */
    bool enable_bio_async;           /**< Enable bio-async integration */
    uint32_t update_interval_ms;     /**< Update interval in ms */
} collective_cognition_config_t;

/*=============================================================================
 * Statistics Types
 *===========================================================================*/

/**
 * @brief Collective cognition statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t instances_joined;
    uint64_t instances_left;
    uint64_t goals_proposed;
    uint64_t goals_completed;
    uint64_t goals_failed;
    uint64_t entrainment_events;
    uint64_t fragmentation_events;
    uint64_t overload_events;
    float avg_phi;
    float max_phi;
    float avg_sync;
    uint64_t bytes_transferred;
} collective_cognition_stats_t;

/*=============================================================================
 * Opaque Handles
 *===========================================================================*/

typedef struct collective_cognition collective_cognition_t;
typedef struct hyperscanning hyperscanning_t;
typedef struct extended_mind extended_mind_t;
typedef struct collective_phi_system collective_phi_system_t;
typedef struct shared_intentionality shared_intentionality_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * @brief Get default collective cognition configuration
 *
 * @return Default configuration with biologically-motivated defaults
 */
collective_cognition_config_t collective_cognition_default_config(void);

/**
 * @brief Get default hyperscanning configuration
 */
hyperscanning_config_t hyperscanning_default_config(void);

/**
 * @brief Get default extended mind configuration
 */
extended_mind_config_t extended_mind_default_config(void);

/**
 * @brief Get default collective phi configuration
 */
collective_phi_config_t collective_phi_default_config(void);

/**
 * @brief Get default shared intentionality configuration
 */
shared_intentionality_config_t shared_intentionality_default_config(void);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create collective cognition system
 *
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on failure
 */
collective_cognition_t* collective_cognition_create(
    const collective_cognition_config_t* config
);

/**
 * @brief Destroy collective cognition system
 *
 * @param cc System to destroy
 */
void collective_cognition_destroy(collective_cognition_t* cc);

/**
 * @brief Reset collective cognition system
 *
 * Clears all instances, goals, and statistics.
 *
 * @param cc System to reset
 * @return 0 on success, -1 on error
 */
int collective_cognition_reset(collective_cognition_t* cc);

/*=============================================================================
 * Instance Management API
 *===========================================================================*/

/**
 * @brief Register a brain instance with the collective
 *
 * @param cc Collective cognition system
 * @param instance_id Unique instance identifier
 * @param brain Brain handle (optional, for direct integration)
 * @return 0 on success, -1 on error
 */
int collective_cognition_register_instance(
    collective_cognition_t* cc,
    uint32_t instance_id,
    brain_t* brain
);

/**
 * @brief Unregister a brain instance from the collective
 *
 * @param cc Collective cognition system
 * @param instance_id Instance to unregister
 * @return 0 on success, -1 on error
 */
int collective_cognition_unregister_instance(
    collective_cognition_t* cc,
    uint32_t instance_id
);

/**
 * @brief Get number of registered instances
 *
 * @param cc Collective cognition system
 * @return Number of instances
 */
uint32_t collective_cognition_instance_count(const collective_cognition_t* cc);

/**
 * @brief Check if instance is registered
 *
 * @param cc Collective cognition system
 * @param instance_id Instance to check
 * @return true if registered
 */
bool collective_cognition_has_instance(
    const collective_cognition_t* cc,
    uint32_t instance_id
);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Update collective cognition state
 *
 * Processes incoming messages, updates all subsystems, computes new state.
 * Call this periodically (e.g., every 10-100ms).
 *
 * @param cc Collective cognition system
 * @return 0 on success, -1 on error
 */
int collective_cognition_update(collective_cognition_t* cc);

/*=============================================================================
 * State Query API
 *===========================================================================*/

/**
 * @brief Get complete collective cognition state
 *
 * @param cc Collective cognition system
 * @param state Output state structure
 * @return 0 on success, -1 on error
 */
int collective_cognition_get_state(
    const collective_cognition_t* cc,
    collective_cognition_state_t* state
);

/**
 * @brief Get current consciousness level
 *
 * @param cc Collective cognition system
 * @return Consciousness level classification
 */
collective_consciousness_level_t collective_cognition_get_consciousness_level(
    const collective_cognition_t* cc
);

/**
 * @brief Get hyperscanning state
 *
 * @param cc Collective cognition system
 * @param state Output state structure
 * @return 0 on success, -1 on error
 */
int collective_cognition_get_hyperscan_state(
    const collective_cognition_t* cc,
    hyperscan_state_t* state
);

/**
 * @brief Get extended mind state
 *
 * @param cc Collective cognition system
 * @param state Output state structure
 * @return 0 on success, -1 on error
 */
int collective_cognition_get_extended_mind_state(
    const collective_cognition_t* cc,
    extended_mind_state_t* state
);

/**
 * @brief Get collective phi metrics
 *
 * @param cc Collective cognition system
 * @param phi Output phi structure
 * @return 0 on success, -1 on error
 */
int collective_cognition_get_phi(
    const collective_cognition_t* cc,
    collective_phi_t* phi
);

/**
 * @brief Get we-mode state
 *
 * @param cc Collective cognition system
 * @param state Output state structure
 * @return 0 on success, -1 on error
 */
int collective_cognition_get_we_mode(
    const collective_cognition_t* cc,
    we_mode_state_t* state
);

/*=============================================================================
 * Component Access API
 *===========================================================================*/

/**
 * @brief Get hyperscanning subsystem
 *
 * @param cc Collective cognition system
 * @return Hyperscanning handle
 */
hyperscanning_t* collective_cognition_get_hyperscanning(collective_cognition_t* cc);

/**
 * @brief Get extended mind subsystem
 *
 * @param cc Collective cognition system
 * @return Extended mind handle
 */
extended_mind_t* collective_cognition_get_extended_mind(collective_cognition_t* cc);

/**
 * @brief Get collective phi subsystem
 *
 * @param cc Collective cognition system
 * @return Collective phi handle
 */
collective_phi_system_t* collective_cognition_get_phi_system(collective_cognition_t* cc);

/**
 * @brief Get shared intentionality subsystem
 *
 * @param cc Collective cognition system
 * @return Shared intentionality handle
 */
shared_intentionality_t* collective_cognition_get_intentionality(collective_cognition_t* cc);

/*=============================================================================
 * Bio-Async Integration API
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param cc Collective cognition system
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int collective_cognition_connect_bio_async(
    collective_cognition_t* cc,
    bio_router_t* router
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param cc Collective cognition system
 * @return 0 on success, -1 on error
 */
int collective_cognition_disconnect_bio_async(collective_cognition_t* cc);

/**
 * @brief Check if bio-async is connected
 *
 * @param cc Collective cognition system
 * @return true if connected
 */
bool collective_cognition_is_bio_async_connected(const collective_cognition_t* cc);

/*=============================================================================
 * Load Balancing API
 *===========================================================================*/

/**
 * @brief Balance cognitive load across instances
 *
 * Redistributes tasks from overloaded to underloaded instances.
 *
 * @param cc Collective cognition system
 * @return Number of tasks redistributed, or -1 on error
 */
int collective_cognition_balance_load(collective_cognition_t* cc);

/**
 * @brief Offload a task to another instance
 *
 * @param cc Collective cognition system
 * @param from_instance Source instance
 * @param to_instance Target instance
 * @param task_data Task data
 * @param task_size Task data size
 * @return 0 on success, -1 on error
 */
int collective_cognition_offload_task(
    collective_cognition_t* cc,
    uint32_t from_instance,
    uint32_t to_instance,
    const void* task_data,
    size_t task_size
);

/*=============================================================================
 * Statistics API
 *===========================================================================*/

/**
 * @brief Get collective cognition statistics
 *
 * @param cc Collective cognition system
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int collective_cognition_get_stats(
    const collective_cognition_t* cc,
    collective_cognition_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param cc Collective cognition system
 */
void collective_cognition_reset_stats(collective_cognition_t* cc);

/*=============================================================================
 * Utility API
 *===========================================================================*/

/**
 * @brief Get consciousness level name
 *
 * @param level Consciousness level
 * @return Human-readable name
 */
const char* collective_consciousness_level_name(collective_consciousness_level_t level);

/**
 * @brief Get sync band name
 *
 * @param band Sync band
 * @return Human-readable name (e.g., "GAMMA")
 */
const char* sync_band_name(sync_band_t band);

/**
 * @brief Get extension type name
 *
 * @param type Extension type
 * @return Human-readable name (e.g., "MEMORY")
 */
const char* extension_type_name(extension_type_t type);

/**
 * @brief Dump collective cognition state for debugging
 *
 * @param cc Collective cognition system
 */
void collective_cognition_dump(const collective_cognition_t* cc);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COLLECTIVE_COGNITION_H */
