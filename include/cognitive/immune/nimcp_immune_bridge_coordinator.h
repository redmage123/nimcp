/**
 * @file nimcp_immune_bridge_coordinator.h
 * @brief Immune Bridge Coordinator - Central Registry and Manager for All Immune Bridges
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Central coordinator managing 27+ immune bridge modules across NIMCP
 * WHY:  Immune bridges span cognitive, plasticity, middleware, perception, and core modules.
 *       A central coordinator ensures health monitoring, coordinated updates, and cross-bridge
 *       messaging for system-wide immune coherence.
 * HOW:  Registry pattern stores all bridges with category tracking; observer pattern for
 *       health monitoring; strategy pattern for update policies; bio-async integration
 *       for inter-bridge coordination.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SYSTEMIC IMMUNE COORDINATION:
 * -----------------------------
 * The biological immune system coordinates responses across multiple organs and systems:
 * - Local responses: Tissue-specific immune activation
 * - Regional coordination: Lymph node integration of signals
 * - Systemic coordination: Cytokine cascades coordinate whole-body response
 * - Cross-talk: Immune-nervous-endocrine integration
 *
 * This coordinator implements systemic immune coordination for NIMCP, ensuring that
 * immune responses in one module (e.g., attention) are appropriately propagated and
 * coordinated with other modules (e.g., memory, plasticity).
 *
 * HIERARCHICAL IMMUNE ORGANIZATION:
 * ---------------------------------
 * Immune system operates at multiple scales:
 * - Cellular level: Individual immune cells (B/T cells)
 * - Tissue level: Local inflammation and response
 * - Organ level: Organ-specific immune coordination
 * - Systemic level: Whole-organism immune state
 *
 * The coordinator manages bridge-level (module) coordination, analogous to organ-level
 * immune integration in biology.
 *
 * IMMUNE SURVEILLANCE AND MONITORING:
 * -----------------------------------
 * Biological immune system continuously monitors for:
 * - Pathogen presence (threat detection)
 * - Tissue damage (health monitoring)
 * - Immune cell status (readiness)
 * - Inflammatory balance (homeostasis)
 *
 * This coordinator implements surveillance through:
 * - Bridge health monitoring
 * - Cross-bridge message tracking
 * - Update latency monitoring
 * - System-wide immune state assessment
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║            IMMUNE BRIDGE COORDINATOR (Central Registry)                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   BRIDGE REGISTRY BY CATEGORY                       │  ║
 * ║   │                                                                     │  ║
 * ║   │  Cognitive Bridges       Plasticity Bridges      Middleware        │  ║
 * ║   │  ──────────────────      ──────────────────      ──────────        │  ║
 * ║   │  • Attention             • STDP                  • Training         │  ║
 * ║   │  • Memory                • BCM                   • Routing          │  ║
 * ║   │  • Emotion               • Homeostatic           • Buffering        │  ║
 * ║   │  • Reasoning             • Synaptic Scaling      • Population       │  ║
 * ║   │  • Executive             • Eligibility           • Feature Extract  │  ║
 * ║   │  • Introspection         • Dendritic             • Thalamic         │  ║
 * ║   │  • Curiosity             • Neuromodulator        • Sequence         │  ║
 * ║   │  • Wellbeing                                                        │  ║
 * ║   │  • Mental Health         Perception Bridges      Core Bridges      │  ║
 * ║   │  • Self Model            ──────────────────      ────────────      │  ║
 * ║   │  • Theory of Mind        • Visual Cortex         • Oscillations     │  ║
 * ║   │  • Sleep                 • Audio Cortex          • Cortical Columns │  ║
 * ║   │  • Autobiographical      • Speech Cortex         • Broca's Area     │  ║
 * ║   │  • Knowledge                                                        │  ║
 * ║   └────────────────────────────┬───────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   HEALTH MONITORING                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   • Bridge connectivity checks                                     │  ║
 * ║   │   • Update latency tracking                                        │  ║
 * ║   │   • Message delivery monitoring                                    │  ║
 * ║   │   • System-wide immune state                                       │  ║
 * ║   └────────────────────────────┬───────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   CROSS-BRIDGE COORDINATION                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   Bio-Async Messaging            Brain Immune Integration          │  ║
 * ║   │   ─────────────────────          ──────────────────────            │  ║
 * ║   │   Inter-bridge cytokine signals  Global immune state queries       │  ║
 * ║   │   Coordinated responses          Inflammation propagation          │  ║
 * ║   │   Update synchronization         Threat alerts                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Registry: Dynamic bridge registration/unregistration with category tracking
 * - Observer: Health monitoring and event notification across bridges
 * - Strategy: Configurable update policies (immediate vs batched)
 * - Mediator: Coordinates cross-bridge communication via bio-async
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * REFERENCES:
 * - Medzhitov, R. (2007) "Recognition of microorganisms and activation of the immune response"
 * - Murphy, K., Weaver, C. (2016) "Janeway's Immunobiology" (9th ed)
 * - Dantzer, R. et al. (2008) "From inflammation to sickness and depression"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_IMMUNE_BRIDGE_COORDINATOR_H
#define NIMCP_IMMUNE_BRIDGE_COORDINATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Core immune system */
#include "cognitive/immune/nimcp_brain_immune.h"

/* Integration modules */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

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

#define IMMUNE_COORDINATOR_MAX_BRIDGES        128    /**< Max registered bridges */
#define IMMUNE_COORDINATOR_MAX_CATEGORIES     8      /**< Max bridge categories */
#define IMMUNE_COORDINATOR_MODULE_NAME        "immune_bridge_coordinator"
#define IMMUNE_COORDINATOR_HEALTH_CHECK_MS    1000   /**< Health check interval */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct immune_bridge_coordinator immune_bridge_coordinator_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Immune bridge category types
 *
 * WHAT: Categorization of immune bridges by functional domain
 * WHY:  Different categories may have different update priorities
 * HOW:  Hierarchical grouping based on module type
 */
typedef enum {
    IMMUNE_BRIDGE_CATEGORY_COGNITIVE = 0,    /**< Cognitive modules (attention, memory, etc.) */
    IMMUNE_BRIDGE_CATEGORY_PLASTICITY,       /**< Plasticity mechanisms (STDP, BCM, etc.) */
    IMMUNE_BRIDGE_CATEGORY_MIDDLEWARE,       /**< Middleware (training, routing, etc.) */
    IMMUNE_BRIDGE_CATEGORY_PERCEPTION,       /**< Perception (visual, audio, speech) */
    IMMUNE_BRIDGE_CATEGORY_CORE,             /**< Core structures (oscillations, cortical) */
    IMMUNE_BRIDGE_CATEGORY_GLIAL,            /**< Glial support (microglia, astrocytes) */
    IMMUNE_BRIDGE_CATEGORY_SECURITY,         /**< Security modules (anomaly, rate limiter) */
    IMMUNE_BRIDGE_CATEGORY_OTHER,            /**< Other/uncategorized bridges */
    IMMUNE_BRIDGE_CATEGORY_COUNT             /**< Total category count */
} immune_bridge_category_t;

/**
 * @brief Bridge health status
 */
typedef enum {
    IMMUNE_BRIDGE_HEALTHY = 0,               /**< Bridge functioning normally */
    IMMUNE_BRIDGE_DEGRADED,                  /**< Performance degraded */
    IMMUNE_BRIDGE_DISCONNECTED,              /**< Not responding */
    IMMUNE_BRIDGE_ERROR                      /**< Error state */
} immune_bridge_health_t;

/**
 * @brief Coordinator operational state
 */
typedef enum {
    IMMUNE_COORDINATOR_STOPPED = 0,          /**< Not running */
    IMMUNE_COORDINATOR_STARTING,             /**< Initialization in progress */
    IMMUNE_COORDINATOR_RUNNING,              /**< Actively coordinating */
    IMMUNE_COORDINATOR_PAUSED,               /**< Paused */
    IMMUNE_COORDINATOR_STOPPING,             /**< Shutdown in progress */
    IMMUNE_COORDINATOR_ERROR                 /**< Error state */
} immune_coordinator_state_t;

/* ============================================================================
 * Bridge Registration Structures
 * ============================================================================ */

/**
 * @brief Generic bridge handle (opaque pointer to any immune bridge type)
 */
typedef void* immune_bridge_handle_t;

/**
 * @brief Bridge update callback
 *
 * WHAT: Function pointer for updating a bridge
 * WHY:  Bridges have different update signatures; callback abstracts this
 * HOW:  Each bridge provides update function during registration
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
typedef int (*immune_bridge_update_fn_t)(immune_bridge_handle_t bridge);

/**
 * @brief Bridge destroy callback
 *
 * @param bridge Bridge handle
 */
typedef void (*immune_bridge_destroy_fn_t)(immune_bridge_handle_t bridge);

/**
 * @brief Bridge health check callback
 *
 * WHAT: Function pointer for checking bridge health
 * WHY:  Enable custom health checks per bridge type
 * HOW:  Bridge returns health status when called
 *
 * @param bridge Bridge handle
 * @return Health status
 */
typedef immune_bridge_health_t (*immune_bridge_health_fn_t)(immune_bridge_handle_t bridge);

/**
 * @brief Registered bridge entry
 */
typedef struct {
    uint32_t bridge_id;                      /**< Unique bridge ID */
    char bridge_name[64];                    /**< Human-readable name (owned copy) */
    immune_bridge_category_t category;       /**< Bridge category */
    immune_bridge_handle_t handle;           /**< Opaque bridge pointer */

    /* Callbacks */
    immune_bridge_update_fn_t update_fn;     /**< Update callback */
    immune_bridge_destroy_fn_t destroy_fn;   /**< Destroy callback (optional) */
    immune_bridge_health_fn_t health_fn;     /**< Health check callback (optional) */

    /* Health tracking */
    immune_bridge_health_t health_status;    /**< Current health status */
    uint64_t last_update_time;               /**< Last update timestamp (ms) */
    uint64_t last_health_check_time;         /**< Last health check (ms) */
    uint64_t update_count;                   /**< Total updates */
    uint64_t total_update_time_us;           /**< Cumulative update time (microseconds) */
    uint32_t consecutive_failures;           /**< Failed updates in a row */

    /* State */
    bool enabled;                            /**< Whether updates are enabled */
    bool bio_async_connected;                /**< Has bio-async integration */
} immune_bridge_entry_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Per-category configuration
 */
typedef struct {
    bool enabled;                            /**< Enable this category */
    uint32_t update_priority;                /**< Update priority (0=highest) */
} immune_category_config_t;

/**
 * @brief Immune bridge coordinator configuration
 */
typedef struct {
    /* Category settings */
    immune_category_config_t categories[IMMUNE_BRIDGE_CATEGORY_COUNT];

    /* Global settings */
    uint32_t max_bridges;                    /**< Maximum registered bridges */
    bool enable_auto_update;                 /**< Auto-update on timer (vs manual) */
    uint64_t health_check_interval_ms;       /**< Health check interval */

    /* Integration enables */
    bool enable_bio_async;                   /**< Enable bio-async integration */
    bool enable_brain_immune;                /**< Enable brain immune integration */
    bool enable_statistics;                  /**< Track detailed statistics */
    bool enable_logging;                     /**< Enable logging */

    /* Performance tuning */
    uint32_t max_updates_per_cycle;          /**< Max bridges to update per cycle (0=all) */
    float update_time_budget_ms;             /**< Max time per update cycle (0=unlimited) */
    uint32_t max_consecutive_failures;       /**< Max failures before marking degraded */
} immune_bridge_coordinator_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Per-category statistics
 */
typedef struct {
    uint32_t bridge_count;                   /**< Bridges in this category */
    uint32_t healthy_bridges;                /**< Healthy bridges */
    uint32_t degraded_bridges;               /**< Degraded bridges */
    uint32_t disconnected_bridges;           /**< Disconnected bridges */
    uint64_t total_updates;                  /**< Total updates for category */
    float avg_update_time_us;                /**< Average update time (microseconds) */
    float max_update_time_us;                /**< Max update time */
} immune_category_stats_t;

/**
 * @brief Coordinator-wide statistics
 */
typedef struct {
    /* Bridge counts */
    uint32_t total_bridges;                  /**< Total registered bridges */
    uint32_t active_bridges;                 /**< Currently enabled bridges */
    uint32_t healthy_bridges;                /**< Healthy bridges */
    immune_category_stats_t categories[IMMUNE_BRIDGE_CATEGORY_COUNT];

    /* Update performance */
    uint64_t total_update_cycles;            /**< Total coordinator update calls */
    uint64_t total_bridge_updates;           /**< Total individual bridge updates */
    float avg_cycle_time_us;                 /**< Average cycle time (microseconds) */
    float max_cycle_time_us;                 /**< Max cycle time */

    /* Integration stats */
    uint64_t bio_async_messages_sent;        /**< Bio-async messages sent */
    uint64_t bio_async_messages_received;    /**< Bio-async messages received */
    uint64_t cross_bridge_coordinations;     /**< Cross-bridge coordinations */

    /* Health metrics */
    uint32_t update_errors;                  /**< Failed updates */
    uint32_t health_check_failures;          /**< Failed health checks */
    uint64_t last_health_check_time;         /**< Last global health check */
    float system_health;                     /**< Overall health (0-1) */
} immune_coordinator_stats_t;

/* ============================================================================
 * Main Coordinator Structure
 * ============================================================================ */

/**
 * @brief Immune bridge coordinator state
 */
struct immune_bridge_coordinator {
    immune_bridge_coordinator_config_t config;  /**< Configuration */
    immune_coordinator_state_t state;           /**< Current state */

    /* Bridge registry */
    immune_bridge_entry_t* bridges;          /**< Registered bridges array */
    uint32_t bridge_count;                   /**< Current bridge count */
    uint32_t bridge_capacity;                /**< Array capacity */
    uint32_t next_bridge_id;                 /**< Next bridge ID */

    /* Integration handles */
    bio_module_context_t bio_context;        /**< Bio-async context */
    brain_immune_system_t* brain_immune;     /**< Brain immune system */

    /* Statistics */
    immune_coordinator_stats_t stats;

    /* Timing */
    uint64_t start_time;                     /**< Coordinator start time (ms) */
    uint64_t last_update_time;               /**< Last global update (ms) */
    uint64_t last_health_check_time;         /**< Last health check (ms) */

    /* Thread safety */
    nimcp_mutex_t* mutex;                    /**< Thread-safe operations */

    /* Runtime state */
    bool bio_async_connected;                /**< Bio-async is connected */
    bool immune_connected;                   /**< Brain immune is connected */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default coordinator configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with reasonable defaults
 * HOW:  Set default values for all configuration fields
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int immune_bridge_coordinator_default_config(immune_bridge_coordinator_config_t* config);

/**
 * @brief Create immune bridge coordinator
 *
 * WHAT: Initialize coordinator infrastructure
 * WHY:  Central coordination point for all immune bridges
 * HOW:  Allocate bridge registry, initialize statistics, register with bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return New coordinator or NULL on failure
 */
immune_bridge_coordinator_t* immune_bridge_coordinator_create(
    const immune_bridge_coordinator_config_t* config
);

/**
 * @brief Destroy immune bridge coordinator
 *
 * WHAT: Clean up coordinator resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister all bridges, disconnect integrations, free memory
 *
 * NOTE: Does NOT destroy registered bridges (caller responsibility)
 *
 * @param coordinator Coordinator to destroy (NULL safe)
 */
void immune_bridge_coordinator_destroy(immune_bridge_coordinator_t* coordinator);

/**
 * @brief Start coordinator
 *
 * WHAT: Begin coordinated bridge monitoring
 * WHY:  Activate system-wide immune bridge coordination
 * HOW:  Set state to RUNNING, initialize timing, start health checks
 *
 * @param coordinator Coordinator
 * @return 0 on success, -1 on error
 */
int immune_bridge_coordinator_start(immune_bridge_coordinator_t* coordinator);

/**
 * @brief Stop coordinator
 *
 * WHAT: Halt coordinated bridge operations
 * WHY:  Graceful shutdown
 * HOW:  Set state to STOPPED, complete pending updates
 *
 * @param coordinator Coordinator
 * @return 0 on success, -1 on error
 */
int immune_bridge_coordinator_stop(immune_bridge_coordinator_t* coordinator);

/**
 * @brief Pause coordinator
 *
 * WHAT: Temporarily suspend operations
 * WHY:  Debugging, checkpointing, or system maintenance
 * HOW:  Set state to PAUSED
 *
 * @param coordinator Coordinator
 * @return 0 on success, -1 on error
 */
int immune_bridge_coordinator_pause(immune_bridge_coordinator_t* coordinator);

/**
 * @brief Resume coordinator
 *
 * WHAT: Resume operations after pause
 * WHY:  Return to normal operation
 * HOW:  Set state to RUNNING
 *
 * @param coordinator Coordinator
 * @return 0 on success, -1 on error
 */
int immune_bridge_coordinator_resume(immune_bridge_coordinator_t* coordinator);

/* ============================================================================
 * Bridge Registration API
 * ============================================================================ */

/**
 * @brief Register immune bridge
 *
 * WHAT: Add bridge to coordinator registry
 * WHY:  Enable coordinated monitoring and health tracking
 * HOW:  Store bridge handle, callbacks, category
 *
 * @param coordinator Coordinator
 * @param name Bridge name (e.g., "attention_immune_bridge")
 * @param category Bridge category
 * @param handle Bridge handle (opaque pointer)
 * @param update_fn Update callback (optional, can be NULL)
 * @param destroy_fn Destroy callback (optional, can be NULL)
 * @param health_fn Health check callback (optional, can be NULL)
 * @param bridge_id_out Output: assigned bridge ID
 * @return 0 on success, -1 on error
 */
int immune_bridge_coordinator_register_bridge(
    immune_bridge_coordinator_t* coordinator,
    const char* name,
    immune_bridge_category_t category,
    immune_bridge_handle_t handle,
    immune_bridge_update_fn_t update_fn,
    immune_bridge_destroy_fn_t destroy_fn,
    immune_bridge_health_fn_t health_fn,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister immune bridge
 *
 * WHAT: Remove bridge from coordinator
 * WHY:  Dynamic system reconfiguration
 * HOW:  Find and remove bridge entry
 *
 * NOTE: Does NOT call destroy_fn (caller responsibility)
 *
 * @param coordinator Coordinator
 * @param bridge_id Bridge ID to unregister
 * @return 0 on success, -1 if not found
 */
int immune_bridge_coordinator_unregister_bridge(
    immune_bridge_coordinator_t* coordinator,
    uint32_t bridge_id
);

/**
 * @brief Enable/disable specific bridge
 *
 * WHAT: Toggle bridge participation
 * WHY:  Selective activation without unregistering
 * HOW:  Set bridge enabled flag
 *
 * @param coordinator Coordinator
 * @param bridge_id Bridge ID
 * @param enabled Enable (true) or disable (false)
 * @return 0 on success, -1 if not found
 */
int immune_bridge_coordinator_set_bridge_enabled(
    immune_bridge_coordinator_t* coordinator,
    uint32_t bridge_id,
    bool enabled
);

/**
 * @brief Get bridge by ID
 *
 * @param coordinator Coordinator
 * @param bridge_id Bridge ID
 * @return Bridge entry or NULL if not found
 */
const immune_bridge_entry_t* immune_bridge_coordinator_get_bridge(
    const immune_bridge_coordinator_t* coordinator,
    uint32_t bridge_id
);

/**
 * @brief Get all bridges in category
 *
 * @param coordinator Coordinator
 * @param category Category
 * @param bridges Output array (caller allocated)
 * @param max_bridges Array capacity
 * @return Number of bridges written
 */
uint32_t immune_bridge_coordinator_get_bridges_by_category(
    const immune_bridge_coordinator_t* coordinator,
    immune_bridge_category_t category,
    const immune_bridge_entry_t** bridges,
    uint32_t max_bridges
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main coordinator update
 *
 * WHAT: Update all enabled bridges
 * WHY:  Core of coordinator - ensures all bridges advance state
 * HOW:  Iterate enabled bridges, call update_fn, track statistics
 *
 * NOTE: Call this regularly from main loop
 *
 * @param coordinator Coordinator
 * @param current_time_ms Current time (milliseconds)
 * @return Number of bridges updated, -1 on error
 */
int immune_bridge_coordinator_update(
    immune_bridge_coordinator_t* coordinator,
    uint64_t current_time_ms
);

/**
 * @brief Update specific category
 *
 * WHAT: Update all bridges in one category
 * WHY:  Fine-grained control over update timing
 * HOW:  Iterate enabled bridges in category, call update_fn
 *
 * @param coordinator Coordinator
 * @param category Category to update
 * @return Number of bridges updated, -1 on error
 */
int immune_bridge_coordinator_update_category(
    immune_bridge_coordinator_t* coordinator,
    immune_bridge_category_t category
);

/**
 * @brief Update specific bridge
 *
 * WHAT: Update single bridge by ID
 * WHY:  Manual override for specific bridge
 * HOW:  Find bridge, call update_fn
 *
 * @param coordinator Coordinator
 * @param bridge_id Bridge ID
 * @return 0 on success, -1 on error
 */
int immune_bridge_coordinator_update_bridge(
    immune_bridge_coordinator_t* coordinator,
    uint32_t bridge_id
);

/* ============================================================================
 * Health Monitoring API
 * ============================================================================ */

/**
 * @brief Perform health check on all bridges
 *
 * WHAT: Check health status of all registered bridges
 * WHY:  Detect disconnected or degraded bridges
 * HOW:  Call health_fn for each bridge, update health status
 *
 * @param coordinator Coordinator
 * @return Number of healthy bridges, -1 on error
 */
int immune_bridge_coordinator_health_check(immune_bridge_coordinator_t* coordinator);

/**
 * @brief Check health of specific bridge
 *
 * WHAT: Query health status of one bridge
 * WHY:  Targeted health monitoring
 * HOW:  Call health_fn if available, else infer from update failures
 *
 * @param coordinator Coordinator
 * @param bridge_id Bridge ID
 * @return Health status
 */
immune_bridge_health_t immune_bridge_coordinator_check_bridge_health(
    immune_bridge_coordinator_t* coordinator,
    uint32_t bridge_id
);

/**
 * @brief Get overall system health
 *
 * WHAT: Compute aggregate health metric
 * WHY:  High-level system status indicator
 * HOW:  Ratio of healthy bridges to total bridges
 *
 * @param coordinator Coordinator
 * @return Health score (0-1, 1.0 = all healthy)
 */
float immune_bridge_coordinator_get_system_health(
    const immune_bridge_coordinator_t* coordinator
);

/* ============================================================================
 * Cross-Bridge Coordination API
 * ============================================================================ */

/**
 * @brief Broadcast message to all bridges
 *
 * WHAT: Send coordination message to all registered bridges via bio-async
 * WHY:  System-wide immune alerts or state changes
 * HOW:  Send bio-async message to all bridge module IDs
 *
 * @param coordinator Coordinator
 * @param message_type Bio-async message type
 * @param data Message data
 * @param data_len Data length
 * @return Number of messages sent, -1 on error
 */
int immune_bridge_coordinator_broadcast_message(
    immune_bridge_coordinator_t* coordinator,
    bio_message_type_t message_type,
    const void* data,
    size_t data_len
);

/**
 * @brief Send message to specific category
 *
 * WHAT: Send coordination message to all bridges in category
 * WHY:  Category-specific coordination
 * HOW:  Filter bridges by category, send bio-async messages
 *
 * @param coordinator Coordinator
 * @param category Target category
 * @param message_type Bio-async message type
 * @param data Message data
 * @param data_len Data length
 * @return Number of messages sent, -1 on error
 */
int immune_bridge_coordinator_send_category_message(
    immune_bridge_coordinator_t* coordinator,
    immune_bridge_category_t category,
    bio_message_type_t message_type,
    const void* data,
    size_t data_len
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Integrate coordinator with brain immune
 * WHY:  Enable global immune state queries
 * HOW:  Store handle, register for cytokine callbacks
 *
 * @param coordinator Coordinator
 * @param immune Brain immune system
 * @return 0 on success, -1 on error
 */
int immune_bridge_coordinator_connect_brain_immune(
    immune_bridge_coordinator_t* coordinator,
    brain_immune_system_t* immune
);

/**
 * @brief Disconnect from brain immune
 *
 * @param coordinator Coordinator
 * @return 0 on success
 */
int immune_bridge_coordinator_disconnect_brain_immune(
    immune_bridge_coordinator_t* coordinator
);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register coordinator with bio-async
 * WHY:  Enable inter-bridge messaging
 * HOW:  Register module, set up handlers
 *
 * @param coordinator Coordinator
 * @return 0 on success, -1 on error
 */
int immune_bridge_coordinator_connect_bio_async(
    immune_bridge_coordinator_t* coordinator
);

/**
 * @brief Disconnect from bio-async
 *
 * @param coordinator Coordinator
 * @return 0 on success
 */
int immune_bridge_coordinator_disconnect_bio_async(
    immune_bridge_coordinator_t* coordinator
);

/* ============================================================================
 * Statistics and Monitoring API
 * ============================================================================ */

/**
 * @brief Get coordinator statistics
 *
 * @param coordinator Coordinator
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int immune_bridge_coordinator_get_stats(
    const immune_bridge_coordinator_t* coordinator,
    immune_coordinator_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param coordinator Coordinator
 */
void immune_bridge_coordinator_reset_stats(immune_bridge_coordinator_t* coordinator);

/**
 * @brief Get current state
 *
 * @param coordinator Coordinator
 * @return Current state
 */
immune_coordinator_state_t immune_bridge_coordinator_get_state(
    const immune_bridge_coordinator_t* coordinator
);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert category to string
 *
 * @param category Category
 * @return Human-readable string
 */
const char* immune_bridge_category_to_string(immune_bridge_category_t category);

/**
 * @brief Convert health status to string
 *
 * @param health Health status
 * @return Human-readable string
 */
const char* immune_bridge_health_to_string(immune_bridge_health_t health);

/**
 * @brief Convert state to string
 *
 * @param state State
 * @return Human-readable string
 */
const char* immune_coordinator_state_to_string(immune_coordinator_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMMUNE_BRIDGE_COORDINATOR_H */
