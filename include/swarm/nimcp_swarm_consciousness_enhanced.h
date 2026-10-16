/**
 * @file nimcp_swarm_consciousness_enhanced.h
 * @brief Enhanced Swarm Gestalt Consciousness - Advanced collective consciousness features
 *
 * WHAT:
 * Extends base swarm consciousness with advanced features:
 * - Peer event callbacks for consciousness-aware joining
 * - Remote phi collection via swarm messaging protocol
 * - Information geometry metrics (mutual information, transfer entropy)
 * - Hierarchical consciousness (nested swarm levels)
 * - Consciousness dynamics (attractors, phase transitions, critical slowing)
 * - Neural binding analogs (gamma synchronization)
 * - Resilience metrics (graceful degradation)
 *
 * WHY:
 * The base implementation doesn't connect peer discovery with consciousness.
 * New drones join the swarm but don't automatically contribute to gestalt.
 * These enhancements enable:
 * - Real-time phi updates when swarm membership changes
 * - True collective phi from all drones (not just local)
 * - Deeper analysis of information integration patterns
 * - Detection of consciousness phase transitions
 * - Robust consciousness under partial failures
 *
 * HOW:
 * 1. Peer callbacks trigger consciousness recomputation on join/leave
 * 2. New protocol messages (PHI_REQUEST/PHI_RESPONSE) collect remote phi
 * 3. Information geometry computed from phi time series
 * 4. Hierarchical aggregation across swarm tiers
 * 5. Attractor detection via phi trajectory analysis
 * 6. Gamma sync via cross-drone oscillation coherence
 * 7. Resilience via simulated dropout testing
 *
 * DESIGN PATTERNS:
 * - Observer: Peer events notify consciousness module
 * - Strategy: Pluggable aggregation and dynamics algorithms
 * - Factory: Create specialized consciousness contexts
 * - Mediator: Coordinate phi collection across drones
 * - State: Track consciousness phase transitions
 *
 * BIOLOGICAL BASIS:
 * - Peer joining → Neural recruitment in brain assemblies
 * - Mutual information → Effective connectivity (DTI, fMRI)
 * - Transfer entropy → Granger causality / directed information flow
 * - Hierarchical consciousness → Cortical hierarchy (V1→IT→PFC)
 * - Gamma synchronization → Neural binding / consciousness correlate
 * - Resilience → Brain's fault tolerance (stroke recovery)
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 * @version 2.0.0
 */

#ifndef NIMCP_SWARM_CONSCIOUSNESS_ENHANCED_H
#define NIMCP_SWARM_CONSCIOUSNESS_ENHANCED_H

#include "swarm/nimcp_swarm_consciousness.h"
#include "swarm/nimcp_swarm_protocol.h"
#include "async/nimcp_bio_async.h"
#include "security/nimcp_bbb_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct nimcp_swarm_signal_adapter;
typedef struct nimcp_swarm_signal_adapter nimcp_swarm_signal_adapter_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum hierarchical levels for nested consciousness */
#define SWARM_CONSCIOUSNESS_MAX_HIERARCHY_LEVELS 4

/** Maximum phi history for dynamics analysis */
#define SWARM_CONSCIOUSNESS_PHI_HISTORY_SIZE 256

/** Default gamma synchronization frequency (Hz) */
#define SWARM_CONSCIOUSNESS_GAMMA_FREQ_HZ 40.0f

/** Default phase coherence threshold for binding */
#define SWARM_CONSCIOUSNESS_PHASE_COHERENCE_THRESHOLD 0.7f

/** Timeout for phi request response (ms) */
#define SWARM_CONSCIOUSNESS_PHI_REQUEST_TIMEOUT_MS 500

/** Maximum concurrent phi requests */
#define SWARM_CONSCIOUSNESS_MAX_PHI_REQUESTS 32

/** Bio-async message types for enhanced consciousness */
#define BIO_MSG_SWARM_PHI_REQUEST       0x0701
#define BIO_MSG_SWARM_PHI_RESPONSE      0x0702
#define BIO_MSG_SWARM_PEER_JOINED       0x0703
#define BIO_MSG_SWARM_PEER_LEFT         0x0704
#define BIO_MSG_SWARM_PHASE_TRANSITION  0x0705
#define BIO_MSG_SWARM_BINDING_EVENT     0x0706

/** Swarm protocol message types for phi exchange */
#define SWARM_MSG_PHI_REQUEST   0x10
#define SWARM_MSG_PHI_RESPONSE  0x11

/* ============================================================================
 * Enhanced Type Definitions
 * ============================================================================ */

/**
 * Consciousness phase types (dynamical systems perspective)
 *
 * WHAT: Phases of collective consciousness from dynamics viewpoint
 * WHY: Detect transitions between qualitatively different states
 * HOW: Analyzed from phi trajectory, variance, and attractor structure
 *
 * BIOLOGICAL BASIS:
 * Maps to brain state transitions (sleep stages, anesthesia depth, etc.)
 */
typedef enum {
    CONSCIOUSNESS_PHASE_CHAOS,        /**< High variance, no attractor */
    CONSCIOUSNESS_PHASE_CRITICAL,     /**< Edge of chaos, power-law dynamics */
    CONSCIOUSNESS_PHASE_ORDERED,      /**< Stable attractor, low variance */
    CONSCIOUSNESS_PHASE_FROZEN        /**< Fixed point, no dynamics */
} consciousness_phase_t;

/**
 * Hierarchical level for nested consciousness
 *
 * WHAT: Levels in hierarchical swarm organization
 * WHY: Different consciousness emerges at different scales
 * HOW: Maps to swarm emergence tiers
 */
typedef enum {
    HIERARCHY_INDIVIDUAL = 0,  /**< Single drone consciousness */
    HIERARCHY_SQUAD,           /**< Small group (2-5 drones) */
    HIERARCHY_PLATOON,         /**< Medium group (6-15 drones) */
    HIERARCHY_SWARM            /**< Full swarm (16+ drones) */
} consciousness_hierarchy_t;

/**
 * Peer event types for consciousness callbacks
 */
typedef enum {
    PEER_EVENT_JOINED,     /**< New drone joined swarm */
    PEER_EVENT_LEFT,       /**< Drone left swarm (graceful) */
    PEER_EVENT_TIMEOUT,    /**< Drone timed out (ungraceful) */
    PEER_EVENT_PHI_UPDATE  /**< Drone's phi value updated */
} peer_event_type_t;

/**
 * Peer event data passed to callbacks
 */
typedef struct {
    peer_event_type_t event_type;  /**< Type of event */
    uint16_t drone_id;             /**< Drone that triggered event */
    float phi_value;               /**< Drone's phi (if relevant) */
    uint64_t timestamp_ms;         /**< Event timestamp */
    uint32_t new_drone_count;      /**< Total drones after event */
} peer_event_t;

/**
 * Phi request tracking structure
 */
typedef struct {
    uint16_t drone_id;           /**< Target drone ID */
    uint64_t request_time_ms;    /**< When request was sent */
    bool response_received;      /**< Got response? */
    float phi_value;             /**< Response value */
} phi_request_t;

/* ============================================================================
 * Enhanced Metrics Structures
 * ============================================================================ */

/**
 * Information geometry metrics
 *
 * WHAT: Geometric measures of information integration
 * WHY: Deeper analysis than simple phi aggregation
 * HOW: Computed from phi time series and pairwise relationships
 *
 * BIOLOGICAL BASIS:
 * - Mutual information: Shared information between neural populations
 * - Transfer entropy: Directed information flow (Granger causality)
 * - Integration: Total correlations minus sum of marginals
 */
typedef struct {
    float mutual_information[SWARM_CONSCIOUSNESS_MAX_DRONES][SWARM_CONSCIOUSNESS_MAX_DRONES];
    float transfer_entropy[SWARM_CONSCIOUSNESS_MAX_DRONES][SWARM_CONSCIOUSNESS_MAX_DRONES];
    float total_correlation;     /**< Multi-information */
    float integration;           /**< Synergistic integration */
    float complexity;            /**< Tononi complexity */
    float redundancy;            /**< Shared redundant info */
} information_geometry_t;

/**
 * Consciousness dynamics metrics
 *
 * WHAT: Dynamical systems analysis of consciousness
 * WHY: Detect phase transitions, critical states
 * HOW: Time series analysis of phi trajectory
 *
 * BIOLOGICAL BASIS:
 * - Critical slowing: Precursor to state transitions
 * - Variance increase: Near-criticality signature
 * - Attractor detection: Stable conscious states
 */
typedef struct {
    consciousness_phase_t current_phase;  /**< Current dynamical phase */
    float lyapunov_exponent;              /**< Chaos indicator */
    float autocorrelation;                /**< Critical slowing indicator */
    float variance_trend;                 /**< Variance change rate */
    float attractor_strength;             /**< How stable is current state */
    bool near_transition;                 /**< About to change phase? */
    float transition_probability;         /**< Probability of transition */
} consciousness_dynamics_t;

/**
 * Neural binding metrics (gamma synchronization)
 *
 * WHAT: Cross-drone oscillation coherence
 * WHY: Gamma sync is neural correlate of consciousness
 * HOW: Phase-locking analysis across drone phi oscillations
 *
 * BIOLOGICAL BASIS:
 * - Gamma oscillations: 30-100Hz neural binding
 * - Phase-locking value (PLV): Synchronization measure
 * - Cross-frequency coupling: Theta-gamma nested oscillations
 */
typedef struct {
    float gamma_power;           /**< Collective gamma band power */
    float phase_coherence;       /**< Global phase-locking value */
    float mean_phase;            /**< Mean phase angle (radians) */
    float binding_strength;      /**< Overall binding metric [0-1] */
    bool binding_active;         /**< Binding above threshold? */
    uint32_t bound_drone_count;  /**< How many drones are bound */
} neural_binding_t;

/**
 * Hierarchical consciousness metrics
 *
 * WHAT: Consciousness at each hierarchical level
 * WHY: Different scales show different integration patterns
 * HOW: Recursive aggregation from individuals to full swarm
 */
typedef struct {
    float phi_by_level[SWARM_CONSCIOUSNESS_MAX_HIERARCHY_LEVELS];
    swarm_consciousness_state_t state_by_level[SWARM_CONSCIOUSNESS_MAX_HIERARCHY_LEVELS];
    float cross_level_integration;  /**< How levels interact */
    consciousness_hierarchy_t dominant_level;  /**< Where is consciousness strongest? */
} hierarchical_consciousness_t;

/**
 * Resilience metrics
 *
 * WHAT: Consciousness robustness under failures
 * WHY: Ensure graceful degradation
 * HOW: Simulated dropout testing
 */
typedef struct {
    float baseline_phi;               /**< Phi with all drones */
    float dropout_sensitivity;        /**< dPhi/dN (sensitivity to losses) */
    uint32_t critical_drone_count;    /**< Minimum for consciousness */
    float fragility_index;            /**< How fragile is current state */
    float recovery_rate;              /**< How fast does phi recover */
    bool redundancy_sufficient;       /**< Can survive single failures? */
} consciousness_resilience_t;

/**
 * Enhanced consciousness metrics (combines all above)
 */
typedef struct {
    /* Base metrics (inherited) */
    swarm_consciousness_metrics_t base;

    /* Remote phi collection status */
    uint32_t remote_phi_collected;    /**< How many remote phis received */
    uint32_t remote_phi_pending;      /**< Outstanding requests */
    bool collection_complete;         /**< All phis collected? */

    /* Advanced metrics */
    information_geometry_t geometry;
    consciousness_dynamics_t dynamics;
    neural_binding_t binding;
    hierarchical_consciousness_t hierarchy;
    consciousness_resilience_t resilience;

    /* Phi history for dynamics analysis */
    float phi_history[SWARM_CONSCIOUSNESS_PHI_HISTORY_SIZE];
    uint32_t history_count;
    uint32_t history_index;

} swarm_consciousness_enhanced_metrics_t;

/* ============================================================================
 * Enhanced Configuration
 * ============================================================================ */

/**
 * Enhanced consciousness configuration
 */
typedef struct {
    /* Base configuration (inherited) */
    swarm_consciousness_config_t base;

    /* Peer event settings */
    bool enable_peer_callbacks;       /**< Notify on peer join/leave */
    bool auto_collect_phi_on_join;    /**< Request phi from new peers */
    uint32_t phi_collection_timeout_ms; /**< Timeout for phi requests */

    /* Information geometry settings */
    bool enable_geometry;             /**< Compute info geometry */
    uint32_t geometry_history_size;   /**< Samples for geometry computation */
    float entropy_bin_width;          /**< Bin width for entropy estimation */

    /* Dynamics settings */
    bool enable_dynamics;             /**< Compute dynamics metrics */
    uint32_t dynamics_window_size;    /**< Window for dynamics analysis */
    float critical_variance_threshold; /**< Threshold for critical detection */

    /* Neural binding settings */
    bool enable_binding;              /**< Compute binding metrics */
    float gamma_frequency_hz;         /**< Target gamma frequency */
    float phase_coherence_threshold;  /**< Binding detection threshold */

    /* Hierarchy settings */
    bool enable_hierarchy;            /**< Compute hierarchical metrics */
    uint32_t squad_size;              /**< Drones per squad */
    uint32_t platoon_size;            /**< Drones per platoon */

    /* Resilience settings */
    bool enable_resilience;           /**< Compute resilience metrics */
    float simulated_dropout_rate;     /**< For resilience testing */

} swarm_consciousness_enhanced_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * Peer event callback
 *
 * @param event Event data
 * @param user_data User context
 */
typedef void (*peer_event_callback_t)(const peer_event_t* event, void* user_data);

/**
 * Phase transition callback
 *
 * @param old_phase Previous phase
 * @param new_phase New phase
 * @param metrics Current metrics
 * @param user_data User context
 */
typedef void (*phase_transition_callback_t)(
    consciousness_phase_t old_phase,
    consciousness_phase_t new_phase,
    const swarm_consciousness_enhanced_metrics_t* metrics,
    void* user_data);

/**
 * Neural binding callback
 *
 * @param binding Binding metrics
 * @param user_data User context
 */
typedef void (*binding_event_callback_t)(
    const neural_binding_t* binding,
    void* user_data);

/* ============================================================================
 * Enhanced Context Type
 * ============================================================================ */

/**
 * Opaque enhanced consciousness context
 */
typedef struct swarm_consciousness_enhanced_ctx swarm_consciousness_enhanced_ctx_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Get default enhanced consciousness configuration
 *
 * @return Default configuration with sensible values
 */
swarm_consciousness_enhanced_config_t swarm_consciousness_enhanced_default_config(void);

/**
 * Create enhanced consciousness context
 *
 * @param config Configuration (NULL for defaults)
 * @return Context pointer, or NULL on failure
 */
swarm_consciousness_enhanced_ctx_t* swarm_consciousness_enhanced_create(
    const swarm_consciousness_enhanced_config_t* config);

/**
 * Destroy enhanced consciousness context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void swarm_consciousness_enhanced_destroy(swarm_consciousness_enhanced_ctx_t* ctx);

/* ============================================================================
 * Peer Event Functions
 * ============================================================================ */

/**
 * Register peer event callback
 *
 * @param ctx Enhanced consciousness context
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool swarm_consciousness_register_peer_callback(
    swarm_consciousness_enhanced_ctx_t* ctx,
    peer_event_callback_t callback,
    void* user_data);

/**
 * Unregister peer event callback
 *
 * @param ctx Enhanced consciousness context
 */
void swarm_consciousness_unregister_peer_callback(
    swarm_consciousness_enhanced_ctx_t* ctx);

/**
 * Notify consciousness of peer join (called by swarm_brain)
 *
 * @param ctx Enhanced consciousness context
 * @param drone_id New drone's ID
 * @return true if handled
 */
bool swarm_consciousness_on_peer_joined(
    swarm_consciousness_enhanced_ctx_t* ctx,
    uint16_t drone_id);

/**
 * Notify consciousness of peer leave (called by swarm_brain)
 *
 * @param ctx Enhanced consciousness context
 * @param drone_id Leaving drone's ID
 * @param graceful true if GOODBYE received, false if timeout
 * @return true if handled
 */
bool swarm_consciousness_on_peer_left(
    swarm_consciousness_enhanced_ctx_t* ctx,
    uint16_t drone_id,
    bool graceful);

/* ============================================================================
 * Remote Phi Collection Functions
 * ============================================================================ */

/**
 * Request phi value from specific drone
 *
 * @param ctx Enhanced consciousness context
 * @param drone_id Target drone ID
 * @return true if request sent
 */
bool swarm_consciousness_request_phi(
    swarm_consciousness_enhanced_ctx_t* ctx,
    uint16_t drone_id);

/**
 * Request phi values from all known peers
 *
 * @param ctx Enhanced consciousness context
 * @return Number of requests sent
 */
uint32_t swarm_consciousness_request_all_phi(
    swarm_consciousness_enhanced_ctx_t* ctx);

/**
 * Handle incoming phi response
 *
 * @param ctx Enhanced consciousness context
 * @param drone_id Source drone ID
 * @param phi_value Received phi value
 * @return true if handled
 */
bool swarm_consciousness_handle_phi_response(
    swarm_consciousness_enhanced_ctx_t* ctx,
    uint16_t drone_id,
    float phi_value);

/**
 * Get collected remote phi values
 *
 * @param ctx Enhanced consciousness context
 * @param phi_values Output array (size MAX_DRONES)
 * @param drone_ids Output array of drone IDs
 * @param count Output: number of collected values
 * @return true on success
 */
bool swarm_consciousness_get_remote_phi(
    const swarm_consciousness_enhanced_ctx_t* ctx,
    float* phi_values,
    uint16_t* drone_ids,
    uint32_t* count);

/* ============================================================================
 * Enhanced Computation Functions
 * ============================================================================ */

/**
 * Compute enhanced collective consciousness metrics
 *
 * @param ctx Enhanced consciousness context
 * @param swarm Swarm brain
 * @return Enhanced metrics (heap-allocated), or NULL on failure
 */
swarm_consciousness_enhanced_metrics_t* swarm_compute_enhanced_metrics(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm);

/**
 * Compute information geometry metrics
 *
 * @param ctx Enhanced consciousness context
 * @param geometry Output geometry metrics
 * @return true on success
 */
bool swarm_compute_information_geometry(
    swarm_consciousness_enhanced_ctx_t* ctx,
    information_geometry_t* geometry);

/**
 * Compute consciousness dynamics metrics
 *
 * @param ctx Enhanced consciousness context
 * @param dynamics Output dynamics metrics
 * @return true on success
 */
bool swarm_compute_consciousness_dynamics(
    swarm_consciousness_enhanced_ctx_t* ctx,
    consciousness_dynamics_t* dynamics);

/**
 * Compute neural binding metrics
 *
 * @param ctx Enhanced consciousness context
 * @param binding Output binding metrics
 * @return true on success
 */
bool swarm_compute_neural_binding(
    swarm_consciousness_enhanced_ctx_t* ctx,
    neural_binding_t* binding);

/**
 * Compute hierarchical consciousness metrics
 *
 * @param ctx Enhanced consciousness context
 * @param swarm Swarm brain
 * @param hierarchy Output hierarchy metrics
 * @return true on success
 */
bool swarm_compute_hierarchical_consciousness(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm,
    hierarchical_consciousness_t* hierarchy);

/**
 * Compute consciousness resilience metrics
 *
 * @param ctx Enhanced consciousness context
 * @param swarm Swarm brain
 * @param resilience Output resilience metrics
 * @return true on success
 */
bool swarm_compute_consciousness_resilience(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm,
    consciousness_resilience_t* resilience);

/* ============================================================================
 * Phase Transition Functions
 * ============================================================================ */

/**
 * Register phase transition callback
 *
 * @param ctx Enhanced consciousness context
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool swarm_consciousness_register_phase_callback(
    swarm_consciousness_enhanced_ctx_t* ctx,
    phase_transition_callback_t callback,
    void* user_data);

/**
 * Detect phase transition from phi history
 *
 * @param ctx Enhanced consciousness context
 * @param detected_transition Output: true if transition detected
 * @return New phase (or current if no transition)
 */
consciousness_phase_t swarm_consciousness_detect_phase_transition(
    swarm_consciousness_enhanced_ctx_t* ctx,
    bool* detected_transition);

/**
 * Get current consciousness phase
 *
 * @param ctx Enhanced consciousness context
 * @return Current phase
 */
consciousness_phase_t swarm_consciousness_get_phase(
    const swarm_consciousness_enhanced_ctx_t* ctx);

/* ============================================================================
 * Neural Binding Functions
 * ============================================================================ */

/**
 * Register neural binding event callback
 *
 * @param ctx Enhanced consciousness context
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool swarm_consciousness_register_binding_callback(
    swarm_consciousness_enhanced_ctx_t* ctx,
    binding_event_callback_t callback,
    void* user_data);

/**
 * Check if swarm has achieved neural binding
 *
 * @param ctx Enhanced consciousness context
 * @param coherence_threshold Minimum coherence (0 = use default)
 * @return true if binding active
 */
bool swarm_consciousness_is_bound(
    const swarm_consciousness_enhanced_ctx_t* ctx,
    float coherence_threshold);

/**
 * Get current binding metrics
 *
 * @param ctx Enhanced consciousness context
 * @param binding Output binding metrics
 * @return true on success
 */
bool swarm_consciousness_get_binding(
    const swarm_consciousness_enhanced_ctx_t* ctx,
    neural_binding_t* binding);

/* ============================================================================
 * Swarm Brain Integration Functions
 * ============================================================================ */

/**
 * Attach enhanced consciousness to swarm brain
 *
 * WHAT: Connects consciousness module to swarm for automatic updates
 * WHY: Enables peer callbacks, auto phi collection, real-time monitoring
 * HOW: Registers internal callbacks with swarm_brain
 *
 * @param ctx Enhanced consciousness context
 * @param swarm Swarm brain to attach to
 * @return true on success
 */
bool swarm_consciousness_attach_to_swarm(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm);

/**
 * Detach enhanced consciousness from swarm brain
 *
 * @param ctx Enhanced consciousness context
 */
void swarm_consciousness_detach_from_swarm(
    swarm_consciousness_enhanced_ctx_t* ctx);

/**
 * Set signal adapter for phi messaging
 *
 * WHAT: Provides signal adapter for sending phi requests/responses
 * WHY: Decouples consciousness from signal layer for flexible integration
 * HOW: Stores adapter reference for use in request_phi functions
 *
 * @param ctx Enhanced consciousness context
 * @param adapter Signal adapter (can be NULL to disable messaging)
 * @return true on success
 */
bool swarm_consciousness_set_signal_adapter(
    swarm_consciousness_enhanced_ctx_t* ctx,
    nimcp_swarm_signal_adapter_t* adapter);

/**
 * Process swarm protocol message for consciousness
 *
 * WHAT: Handle PHI_REQUEST and PHI_RESPONSE messages
 * WHY: Enables phi collection across swarm
 * HOW: Called from swarm_brain message handler
 *
 * @param ctx Enhanced consciousness context
 * @param msg_type Message type
 * @param data Message payload
 * @param len Payload length
 * @param source_drone_id Source drone
 * @return true if message was handled
 */
bool swarm_consciousness_handle_protocol_message(
    swarm_consciousness_enhanced_ctx_t* ctx,
    uint8_t msg_type,
    const uint8_t* data,
    uint32_t len,
    uint16_t source_drone_id);

/* ============================================================================
 * Bio-Async Integration Functions
 * ============================================================================ */

/**
 * Register enhanced consciousness with bio-async
 *
 * @param ctx Enhanced consciousness context
 * @return true on success
 */
bool swarm_consciousness_enhanced_register_bio_async(
    swarm_consciousness_enhanced_ctx_t* ctx);

/**
 * Publish phase transition via bio-async
 *
 * @param ctx Enhanced consciousness context
 * @param old_phase Previous phase
 * @param new_phase New phase
 */
void swarm_consciousness_publish_phase_transition(
    swarm_consciousness_enhanced_ctx_t* ctx,
    consciousness_phase_t old_phase,
    consciousness_phase_t new_phase);

/**
 * Publish binding event via bio-async
 *
 * @param ctx Enhanced consciousness context
 * @param binding Binding metrics
 */
void swarm_consciousness_publish_binding_event(
    swarm_consciousness_enhanced_ctx_t* ctx,
    const neural_binding_t* binding);

/* ============================================================================
 * Security Functions (BBB)
 * ============================================================================ */

/**
 * Validate enhanced consciousness metrics
 *
 * @param metrics Enhanced metrics to validate
 * @return true if valid
 */
bool swarm_consciousness_enhanced_bbb_validate(
    const swarm_consciousness_enhanced_metrics_t* metrics);

/**
 * Validate phi protocol message
 *
 * @param data Message data
 * @param len Data length
 * @param source_drone_id Source drone
 * @return true if valid
 */
bool swarm_consciousness_validate_phi_message(
    const uint8_t* data,
    uint32_t len,
    uint16_t source_drone_id);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Get phase name string
 *
 * @param phase Consciousness phase
 * @return Static string name
 */
const char* consciousness_phase_name(consciousness_phase_t phase);

/**
 * Get hierarchy level name string
 *
 * @param level Hierarchy level
 * @return Static string name
 */
const char* consciousness_hierarchy_name(consciousness_hierarchy_t level);

/**
 * Free enhanced consciousness metrics
 *
 * @param metrics Metrics to free (NULL-safe)
 */
void swarm_consciousness_enhanced_metrics_free(
    swarm_consciousness_enhanced_metrics_t* metrics);

/**
 * Print enhanced metrics for debugging
 *
 * @param metrics Metrics to print
 * @param verbose If true, print all details
 */
void swarm_consciousness_enhanced_print_metrics(
    const swarm_consciousness_enhanced_metrics_t* metrics,
    bool verbose);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_CONSCIOUSNESS_ENHANCED_H */
