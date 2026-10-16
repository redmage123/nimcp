/**
 * @file nimcp_cognitive_meta_controller.h
 * @brief Cognitive Meta-Controller - Unified arbitrator for cognitive subsystems
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Central coordinator for working memory, attention, executive, curiosity, and emotional systems
 * WHY:  Cognitive modules compete for limited resources (working memory slots, attention focus, learning bandwidth).
 *       A meta-controller arbitrates resource allocation, resolves conflicts, and enables metacognitive control.
 * HOW:  Mediator pattern for subsystem coordination, priority queues for resource allocation, observer pattern
 *       for state change notifications, and strategy pattern for arbitration policies.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * ANTERIOR CINGULATE CORTEX (ACC):
 * ---------------------------------
 * - Conflict monitoring: Detects when multiple modules compete for attention
 * - Error detection: Identifies when predictions fail
 * - Performance monitoring: Tracks success/failure rates across tasks
 * - Resource allocation: Determines which subsystem gets priority
 *
 * DORSOLATERAL PREFRONTAL CORTEX (dlPFC):
 * ----------------------------------------
 * - Executive control: Maintains task goals and rules
 * - Working memory management: Allocates limited WM slots (7±2)
 * - Cognitive set-shifting: Switches between task modes
 * - Inhibitory control: Suppresses prepotent responses
 *
 * ROSTRAL PREFRONTAL CORTEX (rPFC):
 * ----------------------------------
 * - Metacognition: Monitors confidence, uncertainty, and performance
 * - Strategy selection: Chooses between different cognitive approaches
 * - Branching: Maintains multiple task goals simultaneously
 *
 * ORBITOFRONTAL CORTEX (OFC):
 * ----------------------------
 * - Affective value: Emotional significance modulates priorities
 * - Reward prediction: Expected outcomes influence resource allocation
 * - Somatic markers: Emotional states bias decision-making
 *
 * META-CONTROLLER ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    COGNITIVE META-CONTROLLER                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               RESOURCE ARBITRATION LAYER                            │  ║
 * ║   │                                                                     │  ║
 * ║   │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐   │  ║
 * ║   │  │ Working Memory  │  │ Attention Focus │  │ Learning Rate   │   │  ║
 * ║   │  │ Slot Allocation │  │ Conflict        │  │ Modulation      │   │  ║
 * ║   │  │ (7±2 items)     │  │ Resolution      │  │ (by state)      │   │  ║
 * ║   │  └─────────────────┘  └─────────────────┘  └─────────────────┘   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               METACOGNITIVE CONTROL LAYER                           │  ║
 * ║   │                                                                     │  ║
 * ║   │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐   │  ║
 * ║   │  │ Uncertainty     │  │ Confidence      │  │ Performance     │   │  ║
 * ║   │  │ Monitoring      │  │ Estimation      │  │ Tracking        │   │  ║
 * ║   │  │ (epistemic/     │  │ (per module)    │  │ (success rate)  │   │  ║
 * ║   │  │  aleatoric)     │  │                 │  │                 │   │  ║
 * ║   │  └─────────────────┘  └─────────────────┘  └─────────────────┘   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               AFFECTIVE METACONTROL LAYER                           │  ║
 * ║   │                                                                     │  ║
 * ║   │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐   │  ║
 * ║   │  │ Emotion →       │  │ Stress →        │  │ Arousal →       │   │  ║
 * ║   │  │ Prioritization  │  │ Capacity        │  │ Focus Width     │   │  ║
 * ║   │  │ (valence bias)  │  │ Reduction       │  │ (inverted-U)    │   │  ║
 * ║   │  └─────────────────┘  └─────────────────┘  └─────────────────┘   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               SUBSYSTEM COORDINATION                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   Working Memory    Executive        Attention       Curiosity     │  ║
 * ║   │   ───────────────   ─────────        ─────────       ─────────     │  ║
 * ║   │   • Slot requests   • Task queue     • Focus bids    • Explore     │  ║
 * ║   │   • Eviction        • Priorities     • Salience      • Exploit     │  ║
 * ║   │   • Refresh         • Switching      • Conflicts     • Novelty     │  ║
 * ║   │                                                                     │  ║
 * ║   │   Global Workspace  Emotion          Brain Immune                  │  ║
 * ║   │   ───────────────   ───────          ────────────                  │  ║
 * ║   │   • Broadcast       • Valence        • Inflammation                │  ║
 * ║   │   • Competition     • Arousal        • Cytokines                   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Mediator: Meta-controller mediates between cognitive subsystems
 * - Priority Queue: Resource allocation uses priority-based scheduling
 * - Observer: Modules notify meta-controller of state changes
 * - Strategy: Pluggable arbitration policies (winner-take-all, weighted fusion, etc.)
 *
 * KEY FEATURES:
 * 1. Working memory slot allocation (7±2 items, priority-based competition)
 * 2. Attention focus conflict resolution (multiple salience sources)
 * 3. Learning rate modulation by cognitive state (uncertainty, stress, fatigue)
 * 4. Executive priority weighting (task urgency influences resource allocation)
 * 5. Metacognitive control (uncertainty → exploration vs exploitation)
 * 6. Affective metacontrol (emotion → prioritization, stress → capacity reduction)
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * REFERENCES:
 * - Miller, E.K. & Cohen, J.D. (2001) "An integrative theory of prefrontal cortex function"
 * - Botvinick, M.M. et al. (2001) "Conflict monitoring and cognitive control"
 * - Fleming, S.M. & Dolan, R.J. (2012) "The neural basis of metacognitive ability"
 * - Pessoa, L. (2008) "On the relationship between emotion and cognition"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_COGNITIVE_META_CONTROLLER_H
#define NIMCP_COGNITIVE_META_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Core cognitive modules */
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_executive.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"

/* Integration modules */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* Utilities */
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define META_CONTROLLER_MAX_MODULES          32     /**< Max registered cognitive modules */
#define META_CONTROLLER_MAX_REQUESTS         64     /**< Max pending resource requests */
#define META_CONTROLLER_MAX_OBSERVERS        16     /**< Max observer callbacks */
#define META_CONTROLLER_DEFAULT_UPDATE_MS    50     /**< Default update interval */
#define META_CONTROLLER_MODULE_NAME          "cognitive_meta_controller"

/* Working memory allocation */
#define META_CONTROLLER_WM_DEFAULT_CAPACITY  7      /**< Miller's 7±2 */
#define META_CONTROLLER_WM_MIN_PRIORITY      0.0f   /**< Minimum priority for WM slot */
#define META_CONTROLLER_WM_EVICTION_THRESHOLD 0.1f  /**< Evict items below this priority */

/* Attention focus */
#define META_CONTROLLER_ATTENTION_THRESHOLD  0.5f   /**< Minimum salience for attention capture */
#define META_CONTROLLER_ATTENTION_DECAY      0.95f  /**< Decay factor for unattended salience */

/* Learning rate modulation */
#define META_CONTROLLER_LR_MIN               0.001f /**< Minimum learning rate */
#define META_CONTROLLER_LR_MAX               0.1f   /**< Maximum learning rate */
#define META_CONTROLLER_LR_DEFAULT           0.01f  /**< Default learning rate */

/* Metacognitive thresholds */
#define META_CONTROLLER_HIGH_UNCERTAINTY     0.7f   /**< High uncertainty threshold */
#define META_CONTROLLER_LOW_CONFIDENCE       0.3f   /**< Low confidence threshold */
#define META_CONTROLLER_POOR_PERFORMANCE     0.5f   /**< Poor performance threshold */

/* Bio-async module ID */
#define BIO_MODULE_COGNITIVE_META_CONTROLLER 0x0320 /**< Module ID for bio-async */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct cognitive_meta_controller cognitive_meta_controller_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Cognitive subsystem identifiers for meta-controller
 *
 * WHAT: Enum of cognitive modules that can request resources
 * WHY:  Type-safe module identification for resource arbitration
 * HOW:  Uses META_CTRL_ prefix to avoid conflict with platform_tier.h enums
 *
 * NOTE: platform_tier.h defines COGNITIVE_MODULE_* as bit flags (bitmask).
 *       This enum uses sequential values for array indexing within the
 *       meta-controller's internal tracking.
 */
typedef enum {
    META_CTRL_MODULE_NONE = 0,
    META_CTRL_MODULE_WORKING_MEMORY,
    META_CTRL_MODULE_EXECUTIVE,
    META_CTRL_MODULE_ATTENTION,
    META_CTRL_MODULE_CURIOSITY,
    META_CTRL_MODULE_EMOTION,
    META_CTRL_MODULE_INTROSPECTION,
    META_CTRL_MODULE_GLOBAL_WORKSPACE,
    META_CTRL_MODULE_THEORY_OF_MIND,
    META_CTRL_MODULE_ETHICS,
    META_CTRL_MODULE_WELLBEING,
    META_CTRL_MODULE_MENTAL_HEALTH,
    META_CTRL_MODULE_CONSOLIDATION,
    META_CTRL_MODULE_CUSTOM_START = 100
} cognitive_module_id_t;

/**
 * @brief Resource types that can be requested
 */
typedef enum {
    RESOURCE_WORKING_MEMORY_SLOT,      /**< Request WM slot allocation */
    RESOURCE_ATTENTION_FOCUS,          /**< Request attention focus */
    RESOURCE_LEARNING_BANDWIDTH,       /**< Request learning rate increase */
    RESOURCE_EXECUTIVE_PRIORITY,       /**< Request executive task priority */
    RESOURCE_GLOBAL_WORKSPACE_ACCESS   /**< Request global workspace broadcast */
} resource_type_t;

/**
 * @brief Arbitration strategies for resource conflicts
 */
typedef enum {
    ARBITRATION_WINNER_TAKE_ALL,       /**< Highest priority wins */
    ARBITRATION_WEIGHTED_FUSION,       /**< Weighted blend of requests */
    ARBITRATION_ROUND_ROBIN,           /**< Fair rotation */
    ARBITRATION_PRIORITY_WEIGHTED      /**< Priority + module importance */
} arbitration_strategy_t;

/**
 * @brief Meta-controller operational state
 */
typedef enum {
    META_CONTROLLER_STOPPED = 0,
    META_CONTROLLER_STARTING,
    META_CONTROLLER_RUNNING,
    META_CONTROLLER_PAUSED,
    META_CONTROLLER_STOPPING,
    META_CONTROLLER_ERROR
} meta_controller_state_t;

/* ============================================================================
 * Resource Request Structures
 * ============================================================================ */

/**
 * @brief Working memory slot request
 */
typedef struct {
    cognitive_module_id_t requester;   /**< Requesting module */
    float priority;                    /**< Request priority [0, 1] */
    uint32_t item_size;                /**< Size of item to store */
    float salience;                    /**< Item salience */
    void* item_data;                   /**< Pointer to item data */
    uint64_t timestamp_ms;             /**< Request timestamp */
} wm_slot_request_t;

/**
 * @brief Attention focus request
 */
typedef struct {
    cognitive_module_id_t requester;   /**< Requesting module */
    float salience;                    /**< Focus salience [0, 1] */
    float urgency;                     /**< Time urgency [0, 1] */
    void* focus_data;                  /**< Pointer to focus data */
    uint64_t timestamp_ms;             /**< Request timestamp */
} attention_request_t;

/**
 * @brief Learning rate modulation request
 */
typedef struct {
    cognitive_module_id_t requester;   /**< Requesting module */
    float uncertainty;                 /**< Current uncertainty [0, 1] */
    float confidence;                  /**< Current confidence [0, 1] */
    float desired_lr;                  /**< Desired learning rate */
    uint64_t timestamp_ms;             /**< Request timestamp */
} learning_rate_request_t;

/**
 * @brief Generic resource request
 */
typedef struct {
    uint32_t request_id;               /**< Unique request ID */
    resource_type_t type;              /**< Resource type */
    cognitive_module_id_t requester;   /**< Requesting module */
    float priority;                    /**< Request priority [0, 1] */
    bool granted;                      /**< Whether request was granted */
    uint64_t timestamp_ms;             /**< Request timestamp */

    union {
        wm_slot_request_t wm_slot;
        attention_request_t attention;
        learning_rate_request_t learning_rate;
    } data;
} resource_request_t;

/* ============================================================================
 * Module Performance Tracking
 * ============================================================================ */

/**
 * @brief Per-module performance metrics
 */
typedef struct {
    cognitive_module_id_t module;      /**< Module ID */
    uint64_t requests_made;            /**< Total resource requests */
    uint64_t requests_granted;         /**< Granted requests */
    uint64_t requests_denied;          /**< Denied requests */
    float avg_priority;                /**< Average request priority */
    float success_rate;                /**< Success rate [0, 1] */
    float current_confidence;          /**< Current confidence [0, 1] */
    float current_uncertainty;         /**< Current uncertainty [0, 1] */
    uint64_t last_request_time;        /**< Last request timestamp */
} module_performance_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Cognitive meta-controller configuration
 */
typedef struct {
    /* Resource limits */
    uint32_t max_wm_slots;             /**< Working memory capacity */
    uint32_t max_attention_foci;       /**< Max simultaneous attention foci */
    float base_learning_rate;          /**< Base learning rate */

    /* Arbitration */
    arbitration_strategy_t strategy;   /**< Arbitration strategy */
    float priority_threshold;          /**< Minimum priority for consideration */

    /* Metacognitive control */
    bool enable_uncertainty_modulation; /**< Modulate by uncertainty */
    bool enable_affective_metacontrol; /**< Emotion influences priorities */
    bool enable_performance_tracking;  /**< Track module performance */

    /* Integration enables */
    bool enable_working_memory;        /**< Connect to working memory */
    bool enable_executive;             /**< Connect to executive controller */
    bool enable_global_workspace;      /**< Connect to global workspace */
    bool enable_brain_immune;          /**< Connect to brain immune */
    bool enable_bio_async;             /**< Connect to bio-async router */

    /* Update timing */
    uint64_t update_interval_ms;       /**< Update interval */

    /* Module priorities (for ARBITRATION_PRIORITY_WEIGHTED) */
    float module_weights[META_CONTROLLER_MAX_MODULES]; /**< Per-module importance */
} meta_controller_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Meta-controller statistics
 */
typedef struct {
    /* Request statistics */
    uint64_t total_requests;           /**< Total resource requests */
    uint64_t granted_requests;         /**< Granted requests */
    uint64_t denied_requests;          /**< Denied requests */
    uint64_t conflicts_resolved;       /**< Resource conflicts resolved */

    /* Resource utilization */
    float wm_utilization;              /**< WM slot utilization [0, 1] */
    float attention_utilization;       /**< Attention utilization [0, 1] */
    float avg_learning_rate;           /**< Average learning rate */

    /* Metacognitive metrics */
    float system_uncertainty;          /**< System-wide uncertainty [0, 1] */
    float system_confidence;           /**< System-wide confidence [0, 1] */
    float system_performance;          /**< Overall performance [0, 1] */

    /* Timing */
    uint64_t total_updates;            /**< Total update cycles */
    float avg_update_time_us;          /**< Average update time (microseconds) */
    float max_update_time_us;          /**< Max update time */

    /* Per-module stats */
    module_performance_t modules[META_CONTROLLER_MAX_MODULES];
} meta_controller_stats_t;

/* ============================================================================
 * Observer Callbacks
 * ============================================================================ */

/**
 * @brief Resource allocation event callback
 *
 * @param request Resource request that was processed
 * @param user_data User-provided context
 */
typedef void (*resource_allocation_callback_t)(
    const resource_request_t* request,
    void* user_data
);

/**
 * @brief Metacognitive state change callback
 *
 * @param uncertainty System uncertainty [0, 1]
 * @param confidence System confidence [0, 1]
 * @param performance System performance [0, 1]
 * @param user_data User-provided context
 */
typedef void (*metacognitive_callback_t)(
    float uncertainty,
    float confidence,
    float performance,
    void* user_data
);

/* ============================================================================
 * Main Structure
 * ============================================================================ */

/**
 * @brief Cognitive meta-controller state
 */
struct cognitive_meta_controller {
    meta_controller_config_t config;   /**< Configuration */
    meta_controller_state_t state;     /**< Current state */

    /* Resource tracking */
    resource_request_t* requests;      /**< Request queue */
    uint32_t request_count;            /**< Current requests */
    uint32_t request_capacity;         /**< Array capacity */
    uint32_t next_request_id;          /**< Next request ID */

    /* Module tracking */
    module_performance_t modules[META_CONTROLLER_MAX_MODULES];
    uint32_t module_count;             /**< Active modules */

    /* Integration handles */
    working_memory_t* working_memory;
    executive_controller_t* executive;
    global_workspace_t* global_workspace;
    brain_immune_system_t* brain_immune;
    bio_module_context_t bio_context;

    /* Observer callbacks */
    resource_allocation_callback_t allocation_callbacks[META_CONTROLLER_MAX_OBSERVERS];
    void* allocation_callback_data[META_CONTROLLER_MAX_OBSERVERS];
    uint32_t allocation_callback_count;

    metacognitive_callback_t metacognitive_callbacks[META_CONTROLLER_MAX_OBSERVERS];
    void* metacognitive_callback_data[META_CONTROLLER_MAX_OBSERVERS];
    uint32_t metacognitive_callback_count;

    /* Statistics */
    meta_controller_stats_t stats;

    /* Timing */
    uint64_t start_time;
    uint64_t last_update_time;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Runtime state */
    bool bio_async_connected;
    bool immune_connected;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default meta-controller configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set all fields to standard values
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int meta_controller_default_config(meta_controller_config_t* config);

/**
 * @brief Create cognitive meta-controller
 *
 * WHAT: Initialize meta-controller infrastructure
 * WHY:  Central coordination point for all cognitive modules
 * HOW:  Allocate structures, initialize queues, register with bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return New meta-controller or NULL on failure
 */
cognitive_meta_controller_t* meta_controller_create(
    const meta_controller_config_t* config
);

/**
 * @brief Destroy cognitive meta-controller
 *
 * WHAT: Clean up meta-controller resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect integrations, free memory
 *
 * @param controller Meta-controller to destroy (NULL safe)
 */
void meta_controller_destroy(cognitive_meta_controller_t* controller);

/**
 * @brief Start meta-controller
 *
 * WHAT: Begin coordinated resource management
 * WHY:  Activate system-wide cognitive control
 * HOW:  Set state to RUNNING, initialize timing
 *
 * @param controller Meta-controller
 * @return 0 on success, -1 on error
 */
int meta_controller_start(cognitive_meta_controller_t* controller);

/**
 * @brief Stop meta-controller
 *
 * WHAT: Halt resource management
 * WHY:  Graceful shutdown
 * HOW:  Set state to STOPPED, complete pending requests
 *
 * @param controller Meta-controller
 * @return 0 on success, -1 on error
 */
int meta_controller_stop(cognitive_meta_controller_t* controller);

/**
 * @brief Pause meta-controller
 *
 * WHAT: Temporarily suspend resource management
 * WHY:  Debugging, checkpointing, or system maintenance
 * HOW:  Set state to PAUSED
 *
 * @param controller Meta-controller
 * @return 0 on success, -1 on error
 */
int meta_controller_pause(cognitive_meta_controller_t* controller);

/**
 * @brief Resume meta-controller
 *
 * WHAT: Resume resource management after pause
 * WHY:  Return to normal operation
 * HOW:  Set state to RUNNING
 *
 * @param controller Meta-controller
 * @return 0 on success, -1 on error
 */
int meta_controller_resume(cognitive_meta_controller_t* controller);

/* ============================================================================
 * Resource Request API
 * ============================================================================ */

/**
 * @brief Request working memory slot
 *
 * WHAT: Module requests allocation of working memory slot
 * WHY:  Limited WM capacity requires arbitration
 * HOW:  Add to request queue, resolve conflicts, allocate to winner
 *
 * @param controller Meta-controller
 * @param requester Requesting module
 * @param item_data Item to store
 * @param item_size Item size in floats
 * @param priority Request priority [0, 1]
 * @param salience Item salience [0, 1]
 * @return Request ID on success, 0 on error
 */
uint32_t meta_controller_request_wm_slot(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t requester,
    const float* item_data,
    uint32_t item_size,
    float priority,
    float salience
);

/**
 * @brief Request attention focus
 *
 * WHAT: Module requests attention focus
 * WHY:  Multiple salience sources compete for limited attention
 * HOW:  Add to request queue, resolve via arbitration strategy
 *
 * @param controller Meta-controller
 * @param requester Requesting module
 * @param salience Focus salience [0, 1]
 * @param urgency Time urgency [0, 1]
 * @param focus_data Pointer to focus data (module-specific)
 * @return Request ID on success, 0 on error
 */
uint32_t meta_controller_request_attention(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t requester,
    float salience,
    float urgency,
    void* focus_data
);

/**
 * @brief Request learning rate modulation
 *
 * WHAT: Module requests learning rate adjustment
 * WHY:  Cognitive state (uncertainty, stress) should modulate learning
 * HOW:  Compute modulated LR based on uncertainty, confidence, immune state
 *
 * @param controller Meta-controller
 * @param requester Requesting module
 * @param uncertainty Current uncertainty [0, 1]
 * @param confidence Current confidence [0, 1]
 * @param desired_lr Desired learning rate
 * @return Modulated learning rate, or -1.0 on error
 */
float meta_controller_request_learning_rate(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t requester,
    float uncertainty,
    float confidence,
    float desired_lr
);

/**
 * @brief Request executive priority boost
 *
 * WHAT: Module requests executive task priority increase
 * WHY:  Urgent tasks need immediate attention
 * HOW:  Forward to executive controller with priority boost
 *
 * @param controller Meta-controller
 * @param requester Requesting module
 * @param task_id Task ID in executive controller
 * @param priority Priority boost [0, 1]
 * @return 0 on success, -1 on error
 */
int meta_controller_request_executive_priority(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t requester,
    uint32_t task_id,
    float priority
);

/**
 * @brief Request global workspace access
 *
 * WHAT: Module requests broadcast to global workspace
 * WHY:  Limited workspace capacity requires competition
 * HOW:  Forward to global workspace with priority-weighted strength
 *
 * @param controller Meta-controller
 * @param requester Requesting module
 * @param content Content to broadcast
 * @param content_dim Content dimensionality
 * @param strength Signal strength [0, 1]
 * @return true if granted access, false otherwise
 */
bool meta_controller_request_workspace_access(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t requester,
    const float* content,
    uint32_t content_dim,
    float strength
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main meta-controller update
 *
 * WHAT: Process all pending requests and resolve conflicts
 * WHY:  Core of meta-controller - coordinates resource allocation
 * HOW:  Check requests, apply arbitration, allocate resources, update stats
 *
 * NOTE: Call this regularly from main loop (e.g., every 50ms)
 *
 * @param controller Meta-controller
 * @param current_time_ms Current time (milliseconds)
 * @return Number of requests processed, -1 on error
 */
int meta_controller_update(
    cognitive_meta_controller_t* controller,
    uint64_t current_time_ms
);

/**
 * @brief Update metacognitive state
 *
 * WHAT: Compute system-wide uncertainty, confidence, performance
 * WHY:  Metacognitive monitoring enables adaptive control
 * HOW:  Aggregate module metrics, detect anomalies, notify observers
 *
 * @param controller Meta-controller
 * @return 0 on success, -1 on error
 */
int meta_controller_update_metacognitive_state(
    cognitive_meta_controller_t* controller
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to working memory
 *
 * @param controller Meta-controller
 * @param working_memory Working memory instance
 * @return 0 on success, -1 on error
 */
int meta_controller_connect_working_memory(
    cognitive_meta_controller_t* controller,
    working_memory_t* working_memory
);

/**
 * @brief Connect to executive controller
 *
 * @param controller Meta-controller
 * @param executive Executive controller instance
 * @return 0 on success, -1 on error
 */
int meta_controller_connect_executive(
    cognitive_meta_controller_t* controller,
    executive_controller_t* executive
);

/**
 * @brief Connect to global workspace
 *
 * @param controller Meta-controller
 * @param workspace Global workspace instance
 * @return 0 on success, -1 on error
 */
int meta_controller_connect_global_workspace(
    cognitive_meta_controller_t* controller,
    global_workspace_t* workspace
);

/**
 * @brief Connect to brain immune system
 *
 * @param controller Meta-controller
 * @param immune Brain immune system
 * @return 0 on success, -1 on error
 */
int meta_controller_connect_brain_immune(
    cognitive_meta_controller_t* controller,
    brain_immune_system_t* immune
);

/**
 * @brief Connect to bio-async router
 *
 * @param controller Meta-controller
 * @return 0 on success, -1 on error
 */
int meta_controller_connect_bio_async(
    cognitive_meta_controller_t* controller
);

/**
 * @brief Disconnect from bio-async
 *
 * @param controller Meta-controller
 * @return 0 on success
 */
int meta_controller_disconnect_bio_async(
    cognitive_meta_controller_t* controller
);

/* ============================================================================
 * Observer API
 * ============================================================================ */

/**
 * @brief Register resource allocation observer
 *
 * @param controller Meta-controller
 * @param callback Callback function
 * @param user_data User-provided context
 * @return 0 on success, -1 on error
 */
int meta_controller_register_allocation_observer(
    cognitive_meta_controller_t* controller,
    resource_allocation_callback_t callback,
    void* user_data
);

/**
 * @brief Register metacognitive state observer
 *
 * @param controller Meta-controller
 * @param callback Callback function
 * @param user_data User-provided context
 * @return 0 on success, -1 on error
 */
int meta_controller_register_metacognitive_observer(
    cognitive_meta_controller_t* controller,
    metacognitive_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Statistics and Monitoring API
 * ============================================================================ */

/**
 * @brief Get meta-controller statistics
 *
 * @param controller Meta-controller
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int meta_controller_get_stats(
    const cognitive_meta_controller_t* controller,
    meta_controller_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param controller Meta-controller
 */
void meta_controller_reset_stats(cognitive_meta_controller_t* controller);

/**
 * @brief Get module performance
 *
 * @param controller Meta-controller
 * @param module Module ID
 * @param performance Output performance metrics
 * @return 0 on success, -1 on error
 */
int meta_controller_get_module_performance(
    const cognitive_meta_controller_t* controller,
    cognitive_module_id_t module,
    module_performance_t* performance
);

/**
 * @brief Get current state
 *
 * @param controller Meta-controller
 * @return Current state
 */
meta_controller_state_t meta_controller_get_state(
    const cognitive_meta_controller_t* controller
);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Set arbitration strategy
 *
 * @param controller Meta-controller
 * @param strategy New arbitration strategy
 * @return 0 on success, -1 on error
 */
int meta_controller_set_arbitration_strategy(
    cognitive_meta_controller_t* controller,
    arbitration_strategy_t strategy
);

/**
 * @brief Set module weight (for ARBITRATION_PRIORITY_WEIGHTED)
 *
 * @param controller Meta-controller
 * @param module Module ID
 * @param weight Module importance weight [0, 1]
 * @return 0 on success, -1 on error
 */
int meta_controller_set_module_weight(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t module,
    float weight
);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert module ID to string
 *
 * @param module Module ID
 * @return Human-readable string
 */
const char* cognitive_module_id_to_string(cognitive_module_id_t module);

/**
 * @brief Convert resource type to string
 *
 * @param type Resource type
 * @return Human-readable string
 */
const char* resource_type_to_string(resource_type_t type);

/**
 * @brief Convert arbitration strategy to string
 *
 * @param strategy Strategy
 * @return Human-readable string
 */
const char* arbitration_strategy_to_string(arbitration_strategy_t strategy);

/**
 * @brief Convert state to string
 *
 * @param state State
 * @return Human-readable string
 */
const char* meta_controller_state_to_string(meta_controller_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COGNITIVE_META_CONTROLLER_H */
