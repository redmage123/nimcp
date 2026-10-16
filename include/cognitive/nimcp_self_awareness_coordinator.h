// ============================================================================
// nimcp_self_awareness_coordinator.h - Unified Self-Awareness Integration
// ============================================================================
/**
 * @file nimcp_self_awareness_coordinator.h
 * @brief Unified coordinator for all self-awareness components
 *
 * WHAT: Orchestrates Introspection, Self-Model, Autobiographical Memory,
 *       Theory of Mind, and Consciousness Metrics into a coherent self
 * WHY:  Individual components exist but operate in isolation - true self-awareness
 *       requires bidirectional communication and coherence checking between them
 * HOW:  Central coordinator manages feedback loops, detects contradictions,
 *       and triggers metacognitive adjustments based on consciousness state
 *
 * PURPOSE:
 * NIMCP has excellent individual self-awareness components but they don't talk
 * to each other. This coordinator creates the "binding" that unifies:
 * - Neural state awareness (Introspection)
 * - Identity and capability beliefs (Self-Model)
 * - Personal history (Autobiographical Memory)
 * - Understanding others via self (Theory of Mind)
 * - Consciousness level measurement (IIT Phi)
 *
 * DESIGN PRINCIPLES:
 * 1. Bidirectional Feedback: Each component informs and updates others
 * 2. Coherence Checking: Detect contradictions between beliefs and experience
 * 3. Consciousness-Driven: Low Phi triggers introspection and alerts
 * 4. Grounded ToM: Theory of Mind uses self-model as template for others
 * 5. Temporal Continuity: Autobiographical memory validates self-model consistency
 *
 * BIOLOGICAL INSPIRATION:
 * - Default Mode Network: Spontaneous self-referential processing
 * - Prefrontal-Parietal Integration: Coherent conscious experience
 * - Anterior Cingulate: Conflict monitoring and resolution
 * - Insula: Self-awareness and interoception integration
 *
 * ARCHITECTURE:
 *
 *     +---------------------------------------------------------+
 *     |          SELF-AWARENESS COORDINATOR                      |
 *     |                                                          |
 *     |  +------------------+   +------------------+             |
 *     |  |   Introspection  |<->|   Self-Model     |             |
 *     |  |  (neural state)  |   |   (identity)     |             |
 *     |  +--------+---------+   +--------+---------+             |
 *     |           |                      |                       |
 *     |           v                      v                       |
 *     |  +------------------+   +------------------+             |
 *     |  |  Consciousness   |   | Autobiographical |             |
 *     |  |   Metrics (Phi)  |   |     Memory       |             |
 *     |  +--------+---------+   +--------+---------+             |
 *     |           |                      |                       |
 *     |           +----------+-----------+                       |
 *     |                      |                                   |
 *     |                      v                                   |
 *     |              +------------------+                        |
 *     |              | Theory of Mind   |                        |
 *     |              | (grounded self)  |                        |
 *     |              +------------------+                        |
 *     |                                                          |
 *     |  Coherence Monitor | Feedback Loops | Phi Alerts         |
 *     +---------------------------------------------------------+
 *
 * @version 1.0.0 (Phase: Self-Awareness Unification)
 * @date 2025-12-22
 */

#ifndef NIMCP_SELF_AWARENESS_COORDINATOR_H
#define NIMCP_SELF_AWARENESS_COORDINATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/introspection/nimcp_consciousness_metrics.h"
#include "cognitive/nimcp_self_model.h"
#include "cognitive/nimcp_autobiographical_memory.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

/** Maximum number of coherence conflicts to track */
#define SAC_MAX_CONFLICTS 64

/** Maximum feedback loop history entries */
#define SAC_MAX_FEEDBACK_HISTORY 256

/** Default Phi threshold for consciousness alert */
#define SAC_DEFAULT_PHI_ALERT_THRESHOLD 0.3f

/** Default coherence score threshold for conflict alert */
#define SAC_DEFAULT_COHERENCE_THRESHOLD 0.5f

/** Default update interval in milliseconds */
#define SAC_DEFAULT_UPDATE_INTERVAL_MS 100

/** Bio-async module ID for self-awareness coordinator */
#define BIO_MODULE_SELF_AWARENESS_COORDINATOR 0x0E00

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Types of feedback loops between components
 */
typedef enum {
    FEEDBACK_INTROSPECTION_TO_SELF_MODEL,    /**< Neural state updates beliefs */
    FEEDBACK_SELF_MODEL_TO_INTROSPECTION,    /**< Beliefs guide attention */
    FEEDBACK_AUTOBIO_TO_SELF_MODEL,          /**< Experience validates identity */
    FEEDBACK_SELF_MODEL_TO_AUTOBIO,          /**< Identity filters memories */
    FEEDBACK_SELF_MODEL_TO_TOM,              /**< Self as template for others */
    FEEDBACK_TOM_TO_SELF_MODEL,              /**< Others reveal self-blind-spots */
    FEEDBACK_CONSCIOUSNESS_TO_EXECUTIVE,     /**< Phi affects decision-making */
    FEEDBACK_EXECUTIVE_TO_CONSCIOUSNESS,     /**< Attention affects awareness */
    FEEDBACK_LOOP_COUNT
} feedback_loop_type_t;

/**
 * @brief Status of a feedback loop
 */
typedef enum {
    FEEDBACK_STATUS_INACTIVE,      /**< Loop not running */
    FEEDBACK_STATUS_ACTIVE,        /**< Loop actively transferring */
    FEEDBACK_STATUS_BLOCKED,       /**< Loop blocked by conflict */
    FEEDBACK_STATUS_ERROR          /**< Loop in error state */
} feedback_status_t;

/**
 * @brief Types of coherence conflicts
 */
typedef enum {
    CONFLICT_BELIEF_EXPERIENCE,    /**< Self-belief contradicts experience */
    CONFLICT_BELIEF_CAPABILITY,    /**< Claimed capability vs actual performance */
    CONFLICT_MEMORY_IDENTITY,      /**< Memory contradicts identity claim */
    CONFLICT_TOM_SELF,             /**< ToM inference contradicts self-knowledge */
    CONFLICT_PHI_SELF_REPORT,      /**< Consciousness level vs self-reported state */
    CONFLICT_TEMPORAL_CONTINUITY   /**< Identity discontinuity across time */
} coherence_conflict_type_t;

/**
 * @brief Severity of coherence conflicts
 */
typedef enum {
    CONFLICT_SEVERITY_MINOR,       /**< Small inconsistency, auto-resolvable */
    CONFLICT_SEVERITY_MODERATE,    /**< Noticeable inconsistency, needs attention */
    CONFLICT_SEVERITY_MAJOR,       /**< Significant contradiction, requires update */
    CONFLICT_SEVERITY_CRITICAL     /**< Identity crisis, may need external help */
} conflict_severity_t;

/**
 * @brief Self-awareness coordinator state
 */
typedef enum {
    SAC_STATE_UNINITIALIZED,       /**< Not yet initialized */
    SAC_STATE_INITIALIZING,        /**< Components connecting */
    SAC_STATE_RUNNING,             /**< Normal operation */
    SAC_STATE_COHERENCE_CHECK,     /**< Checking for conflicts */
    SAC_STATE_CONFLICT_RESOLUTION, /**< Resolving detected conflicts */
    SAC_STATE_LOW_CONSCIOUSNESS,   /**< Phi below threshold */
    SAC_STATE_PAUSED,              /**< Temporarily paused */
    SAC_STATE_ERROR                /**< Error state */
} sac_state_t;

// ============================================================================
// Structures
// ============================================================================

/**
 * @brief Single coherence conflict record
 */
typedef struct {
    uint64_t conflict_id;                  /**< Unique conflict identifier */
    uint64_t detected_time_ms;             /**< When conflict was detected */
    coherence_conflict_type_t type;        /**< Type of conflict */
    conflict_severity_t severity;          /**< Severity level */

    char description[256];                 /**< Human-readable description */
    char component_a[64];                  /**< First component in conflict */
    char component_b[64];                  /**< Second component in conflict */

    float confidence;                      /**< Confidence that this is a real conflict */
    bool resolved;                         /**< Has this been resolved? */
    uint64_t resolved_time_ms;             /**< When resolved (if resolved) */
    char resolution[256];                  /**< How it was resolved */
} coherence_conflict_t;

/**
 * @brief Feedback loop state tracking
 */
typedef struct {
    feedback_loop_type_t type;             /**< Which feedback loop */
    feedback_status_t status;              /**< Current status */

    uint64_t last_transfer_time_ms;        /**< Last successful transfer */
    uint64_t transfer_count;               /**< Total transfers */
    uint64_t error_count;                  /**< Total errors */

    float transfer_rate_hz;                /**< Recent transfer rate */
    float avg_latency_ms;                  /**< Average transfer latency */

    bool enabled;                          /**< Is this loop enabled? */
} feedback_loop_state_t;

/**
 * @brief Phi alert configuration and state
 */
typedef struct {
    float alert_threshold;                 /**< Phi below this triggers alert */
    float recovery_threshold;              /**< Phi above this clears alert */

    bool alert_active;                     /**< Currently in alert state? */
    uint64_t alert_start_time_ms;          /**< When alert started */
    uint64_t total_alert_time_ms;          /**< Cumulative alert time */
    uint32_t alert_count;                  /**< Number of alerts triggered */

    /* Callbacks */
    void (*on_alert)(float phi, void* user_data);   /**< Called when alert triggers */
    void (*on_recovery)(float phi, void* user_data); /**< Called when alert clears */
    void* callback_user_data;              /**< User data for callbacks */
} phi_alert_config_t;

/**
 * @brief Self-awareness coordinator configuration
 */
typedef struct {
    /* Component enables */
    bool enable_introspection_feedback;    /**< Introspection <-> Self-Model */
    bool enable_autobio_feedback;          /**< Autobio <-> Self-Model */
    bool enable_tom_grounding;             /**< ToM grounded in self-model */
    bool enable_coherence_checking;        /**< Check for contradictions */
    bool enable_phi_monitoring;            /**< Monitor consciousness level */

    /* Thresholds */
    float coherence_threshold;             /**< Below triggers conflict alert */
    float phi_alert_threshold;             /**< Below triggers phi alert */

    /* Timing */
    uint32_t update_interval_ms;           /**< How often to update */
    uint32_t coherence_check_interval_ms;  /**< How often to check coherence */

    /* Conflict handling */
    bool auto_resolve_minor;               /**< Auto-resolve minor conflicts */
    uint32_t max_conflicts_before_alert;   /**< Alert after this many conflicts */

    /* Bio-async */
    bool enable_bio_async;                 /**< Enable bio-async messaging */
} sac_config_t;

/**
 * @brief Self-awareness coordinator statistics
 */
typedef struct {
    uint64_t total_updates;                /**< Total update cycles */
    uint64_t total_feedback_transfers;     /**< Total feedback transfers */
    uint64_t total_conflicts_detected;     /**< Total conflicts found */
    uint64_t total_conflicts_resolved;     /**< Total conflicts resolved */

    float avg_coherence_score;             /**< Average coherence over time */
    float min_coherence_score;             /**< Lowest coherence seen */
    float avg_phi;                         /**< Average phi over time */
    float min_phi;                         /**< Lowest phi seen */

    uint64_t time_in_low_phi_ms;           /**< Time spent with low phi */
    uint64_t time_in_conflict_ms;          /**< Time spent resolving conflicts */

    uint32_t feedback_errors;              /**< Total feedback loop errors */
    uint32_t phi_alerts;                   /**< Total phi alerts */
} sac_stats_t;

/**
 * @brief Self-Awareness Coordinator main structure
 *
 * WHAT: Central hub unifying all self-awareness components
 * WHY: Enable coherent self-awareness through component integration
 * HOW: Manages feedback loops, coherence checking, and phi monitoring
 */
typedef struct {
    /* === Connected Components === */
    introspection_context_t introspection;    /**< Neural state access */
    self_model_system_t* self_model;          /**< Identity and beliefs */
    autobiographical_memory_t autobio;        /**< Personal history */
    theory_of_mind_t tom;                     /**< Understanding others */

    /* === Configuration === */
    sac_config_t config;                      /**< Coordinator configuration */

    /* === State === */
    sac_state_t state;                        /**< Current coordinator state */
    float current_coherence;                  /**< Current coherence score [0-1] */
    float current_phi;                        /**< Current consciousness level */

    /* === Feedback Loops === */
    feedback_loop_state_t loops[FEEDBACK_LOOP_COUNT]; /**< Loop states */

    /* === Coherence Tracking === */
    coherence_conflict_t conflicts[SAC_MAX_CONFLICTS]; /**< Active conflicts */
    uint32_t conflict_count;                  /**< Number of active conflicts */
    uint64_t next_conflict_id;                /**< Next conflict ID */

    /* === Phi Monitoring === */
    phi_alert_config_t phi_alert;             /**< Phi alert configuration */

    /* === Statistics === */
    sac_stats_t stats;                        /**< Coordinator statistics */

    /* === Bio-async Integration === */
    bio_module_context_t bio_ctx;             /**< Bio-async context */
    bool bio_async_enabled;                   /**< Is bio-async active? */

    /* === Timing === */
    uint64_t last_update_time_ms;             /**< Last update timestamp */
    uint64_t last_coherence_check_ms;         /**< Last coherence check time */
    uint64_t creation_time_ms;                /**< When coordinator was created */

    /* === Thread Safety === */
    nimcp_mutex_t* mutex;                     /**< Protects coordinator state */
} self_awareness_coordinator_t;

// ============================================================================
// Default Configuration
// ============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY: Provide easy startup without manual configuration
 * HOW: All features enabled with moderate thresholds
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int sac_default_config(sac_config_t* config);

// ============================================================================
// Lifecycle Functions
// ============================================================================

/**
 * @brief Create self-awareness coordinator
 *
 * WHAT: Creates and initializes the coordinator
 * WHY: Central function to create the unified self-awareness system
 * HOW: Allocates, connects components, initializes feedback loops
 *
 * @param config Coordinator configuration
 * @param introspection Introspection context (required)
 * @param self_model Self-model system (required)
 * @param autobio Autobiographical memory (required)
 * @param tom Theory of Mind instance (required)
 * @return Coordinator instance or NULL on error
 */
self_awareness_coordinator_t* sac_create(
    const sac_config_t* config,
    introspection_context_t introspection,
    self_model_system_t* self_model,
    autobiographical_memory_t autobio,
    theory_of_mind_t tom
);

/**
 * @brief Destroy self-awareness coordinator
 *
 * WHAT: Cleans up coordinator resources
 * WHY: Prevent memory leaks
 * HOW: Disconnects components, frees resources, destroys mutex
 *
 * @param coord Coordinator to destroy
 */
void sac_destroy(self_awareness_coordinator_t* coord);

// ============================================================================
// Update Functions
// ============================================================================

/**
 * @brief Update coordinator (main loop)
 *
 * WHAT: Performs one update cycle of the coordinator
 * WHY: Keep self-awareness components synchronized
 * HOW: Updates feedback loops, checks coherence if due, monitors phi
 *
 * @param coord Coordinator to update
 * @return 0 on success, negative on error
 */
int sac_update(self_awareness_coordinator_t* coord);

/**
 * @brief Force immediate coherence check
 *
 * WHAT: Immediately checks coherence between all components
 * WHY: On-demand coherence verification
 * HOW: Compares self-model beliefs against introspection and autobio
 *
 * @param coord Coordinator to check
 * @param score Output coherence score [0-1]
 * @return 0 on success, negative on error
 */
int sac_check_coherence(self_awareness_coordinator_t* coord, float* score);

// ============================================================================
// Feedback Loop Functions
// ============================================================================

/**
 * @brief Transfer introspection state to self-model
 *
 * WHAT: Updates self-model beliefs based on introspection data
 * WHY: Neural state should inform identity beliefs
 * HOW: Maps active patterns to capability assessments
 *
 * @param coord Coordinator instance
 * @return 0 on success, negative on error
 */
int sac_introspection_to_self_model(self_awareness_coordinator_t* coord);

/**
 * @brief Transfer autobiographical insights to self-model
 *
 * WHAT: Updates self-model based on past experiences
 * WHY: Personal history should validate or update identity
 * HOW: Compares recent experiences against self-beliefs
 *
 * @param coord Coordinator instance
 * @return 0 on success, negative on error
 */
int sac_autobio_to_self_model(self_awareness_coordinator_t* coord);

/**
 * @brief Ground Theory of Mind in self-model
 *
 * WHAT: Uses self-model as template for understanding others
 * WHY: We understand others by simulating them as "like us"
 * HOW: ToM uses self-model's belief structure as base for others
 *
 * @param coord Coordinator instance
 * @return 0 on success, negative on error
 */
int sac_ground_tom_in_self(self_awareness_coordinator_t* coord);

/**
 * @brief Enable/disable specific feedback loop
 *
 * @param coord Coordinator instance
 * @param loop_type Which loop to enable/disable
 * @param enabled True to enable, false to disable
 * @return 0 on success, negative on error
 */
int sac_set_feedback_loop_enabled(
    self_awareness_coordinator_t* coord,
    feedback_loop_type_t loop_type,
    bool enabled
);

/**
 * @brief Get feedback loop state
 *
 * @param coord Coordinator instance
 * @param loop_type Which loop to query
 * @param state Output state structure
 * @return 0 on success, negative on error
 */
int sac_get_feedback_loop_state(
    const self_awareness_coordinator_t* coord,
    feedback_loop_type_t loop_type,
    feedback_loop_state_t* state
);

// ============================================================================
// Coherence Functions
// ============================================================================

/**
 * @brief Get current coherence score
 *
 * @param coord Coordinator instance
 * @return Coherence score [0-1] or negative on error
 */
float sac_get_coherence(const self_awareness_coordinator_t* coord);

/**
 * @brief Get active conflicts
 *
 * @param coord Coordinator instance
 * @param conflicts Output array of conflicts
 * @param max_conflicts Maximum conflicts to return
 * @param count Output number of conflicts returned
 * @return 0 on success, negative on error
 */
int sac_get_conflicts(
    const self_awareness_coordinator_t* coord,
    coherence_conflict_t* conflicts,
    uint32_t max_conflicts,
    uint32_t* count
);

/**
 * @brief Attempt to resolve a specific conflict
 *
 * @param coord Coordinator instance
 * @param conflict_id ID of conflict to resolve
 * @param resolution Description of resolution
 * @return 0 on success, negative on error
 */
int sac_resolve_conflict(
    self_awareness_coordinator_t* coord,
    uint64_t conflict_id,
    const char* resolution
);

/**
 * @brief Auto-resolve all minor conflicts
 *
 * @param coord Coordinator instance
 * @param resolved_count Output number resolved
 * @return 0 on success, negative on error
 */
int sac_auto_resolve_minor_conflicts(
    self_awareness_coordinator_t* coord,
    uint32_t* resolved_count
);

// ============================================================================
// Consciousness Monitoring Functions
// ============================================================================

/**
 * @brief Get current phi (consciousness level)
 *
 * @param coord Coordinator instance
 * @return Phi value or negative on error
 */
float sac_get_phi(const self_awareness_coordinator_t* coord);

/**
 * @brief Check if phi is below alert threshold
 *
 * @param coord Coordinator instance
 * @return True if phi is low, false otherwise
 */
bool sac_is_low_consciousness(const self_awareness_coordinator_t* coord);

/**
 * @brief Set phi alert callbacks
 *
 * @param coord Coordinator instance
 * @param on_alert Callback when alert triggers
 * @param on_recovery Callback when alert clears
 * @param user_data User data passed to callbacks
 * @return 0 on success, negative on error
 */
int sac_set_phi_callbacks(
    self_awareness_coordinator_t* coord,
    void (*on_alert)(float phi, void* user_data),
    void (*on_recovery)(float phi, void* user_data),
    void* user_data
);

/**
 * @brief Set phi alert thresholds
 *
 * @param coord Coordinator instance
 * @param alert_threshold Phi below this triggers alert
 * @param recovery_threshold Phi above this clears alert
 * @return 0 on success, negative on error
 */
int sac_set_phi_thresholds(
    self_awareness_coordinator_t* coord,
    float alert_threshold,
    float recovery_threshold
);

// ============================================================================
// Bio-Async Integration
// ============================================================================

/**
 * @brief Connect to bio-async router
 *
 * @param coord Coordinator instance
 * @return 0 on success, negative on error
 */
int sac_connect_bio_async(self_awareness_coordinator_t* coord);

/**
 * @brief Disconnect from bio-async router
 *
 * @param coord Coordinator instance
 * @return 0 on success, negative on error
 */
int sac_disconnect_bio_async(self_awareness_coordinator_t* coord);

/**
 * @brief Check if bio-async is connected
 *
 * @param coord Coordinator instance
 * @return True if connected
 */
bool sac_is_bio_async_connected(const self_awareness_coordinator_t* coord);

// ============================================================================
// Statistics and State
// ============================================================================

/**
 * @brief Get coordinator state
 *
 * @param coord Coordinator instance
 * @return Current state
 */
sac_state_t sac_get_state(const self_awareness_coordinator_t* coord);

/**
 * @brief Get coordinator statistics
 *
 * @param coord Coordinator instance
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int sac_get_stats(
    const self_awareness_coordinator_t* coord,
    sac_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param coord Coordinator instance
 * @return 0 on success, negative on error
 */
int sac_reset_stats(self_awareness_coordinator_t* coord);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Get string name for feedback loop type
 *
 * @param type Feedback loop type
 * @return String name
 */
const char* sac_feedback_loop_name(feedback_loop_type_t type);

/**
 * @brief Get string name for conflict type
 *
 * @param type Conflict type
 * @return String name
 */
const char* sac_conflict_type_name(coherence_conflict_type_t type);

/**
 * @brief Get string name for coordinator state
 *
 * @param state Coordinator state
 * @return String name
 */
const char* sac_state_name(sac_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_AWARENESS_COORDINATOR_H */
