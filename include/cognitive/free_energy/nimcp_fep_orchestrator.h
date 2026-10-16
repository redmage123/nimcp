/**
 * @file nimcp_fep_orchestrator.h
 * @brief Free Energy Principle Orchestrator - System-wide FEP Bridge Coordinator
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Central orchestrator managing all FEP bridges across the NIMCP system
 * WHY:  FEP bridges span cognitive, swarm, security, plasticity, middleware, perception,
 *       async, glial, and core modules. A central orchestrator ensures coordinated
 *       free energy minimization and consistent belief updates across all subsystems.
 * HOW:  Holds references to all bridge types, provides unified lifecycle management,
 *       coordinates update cycles, integrates with bio-async for inter-bridge messaging,
 *       and integrates with brain immune system for immune modulation of FEP processing.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * UNIFIED FREE ENERGY MINIMIZATION:
 * ----------------------------------
 * The brain minimizes free energy at multiple scales simultaneously:
 * - Cellular level: Individual neurons minimize prediction error
 * - Regional level: Brain regions coordinate prediction hierarchies
 * - Network level: Entire brain maintains coherent generative model
 * - Swarm level: Multi-agent systems minimize collective free energy
 *
 * This orchestrator implements the network-level coordination, ensuring that
 * local free energy minimization (per module) contributes to global coherence.
 *
 * HIERARCHICAL COORDINATION:
 * --------------------------
 * Different brain systems operate at different timescales:
 * - Perception: Fast (milliseconds) - rapid belief updates
 * - Cognition: Medium (100ms-seconds) - deliberative inference
 * - Learning: Slow (seconds-minutes) - model parameter updates
 *
 * The orchestrator respects these timescales via configurable update intervals.
 *
 * IMMUNE-FEP INTERACTION:
 * -----------------------
 * Brain immune system modulates FEP processing:
 * - Inflammation reduces learning rates (conserve energy during threat response)
 * - Cytokines modulate precision (attention allocation during immune activation)
 * - Recovery enhances plasticity (post-threat model refinement)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FEP ORCHESTRATOR (Central Coordinator)                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   BRIDGE REGISTRY                                   │  ║
 * ║   │                                                                     │  ║
 * ║   │  Cognitive Bridges    Swarm Bridges      Security Bridges          │  ║
 * ║   │  ─────────────────    ─────────────      ────────────────          │  ║
 * ║   │  • Hierarchical       • Swarm Brain      • Blood-Brain Barrier     │  ║
 * ║   │  • Meta-Learning      • Consensus        • Anomaly Detector         │  ║
 * ║   │  • Personality        • Emergence        • Rate Limiter             │  ║
 * ║   │  • Consolidation      • Flocking         • Pattern DB               │  ║
 * ║   │  • Wellbeing          • Memory           • Security                 │  ║
 * ║   │                       • Signal                                      │  ║
 * ║   │                                                                     │  ║
 * ║   │  Plasticity Bridges   Middleware         Perception Bridges        │  ║
 * ║   │  ──────────────────   ──────────         ──────────────────        │  ║
 * ║   │  • Predictive Coding  • Training         • (Future expansion)       │  ║
 * ║   │  • STP                • Routing                                     │  ║
 * ║   │  • Pink Noise         • Bio-Router                                  │  ║
 * ║   │  • Neuromodulators                                                  │  ║
 * ║   │  • Second Messengers  Async Bridges      Glial Bridges             │  ║
 * ║   │                       ─────────────      ──────────────             │  ║
 * ║   │  Core Bridges         • Bio-Async        • Astrocytes               │  ║
 * ║   │  ─────────────        • Biological       • Microglia                │  ║
 * ║   │  • Cortical Columns     Timescales       • Oligodendrocytes         │  ║
 * ║   │                       • Predictive       • Myelin Sheath            │  ║
 * ║   │                         Protocol         • Glial Integration        │  ║
 * ║   │                       • Semantic                                    │  ║
 * ║   │                         Compression                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   UPDATE COORDINATION                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐ │  ║
 * ║   │   │  Fast Updates   │   │ Medium Updates  │   │  Slow Updates   │ │  ║
 * ║   │   │  (1-10ms)       │   │  (50-100ms)     │   │  (1s+)          │ │  ║
 * ║   │   │  ─────────────  │   │  ─────────────  │   │  ─────────────  │ │  ║
 * ║   │   │  • Perception   │   │  • Cognitive    │   │  • Learning     │ │  ║
 * ║   │   │  • Async        │   │  • Swarm        │   │  • Plasticity   │ │  ║
 * ║   │   │  • Security     │   │  • Middleware   │   │  • Consolidation│ │  ║
 * ║   │   └─────────────────┘   └─────────────────┘   └─────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   INTEGRATION LAYER                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   Bio-Async Router           Brain Immune System                   │  ║
 * ║   │   ──────────────────         ───────────────────                   │  ║
 * ║   │   Inter-bridge messaging     Immune modulation of FEP              │  ║
 * ║   │   Prediction propagation     Inflammation → LR scaling             │  ║
 * ║   │   Synchronization            Cytokines → Precision tuning          │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Facade: Unified interface over all FEP bridges
 * - Registry: Dynamic bridge registration/unregistration
 * - Coordinator: Manages update cycles across heterogeneous bridges
 * - Observer: Tracks statistics and events from all bridges
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * REFERENCES:
 * - Friston, K. (2010) "The free-energy principle: a unified brain theory?"
 * - Parr, Pezzulo, Friston (2022) "Active Inference: The Free Energy Principle"
 * - Ramstead et al. (2018) "Answering Schrödinger's question: A free-energy formulation"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FEP_ORCHESTRATOR_H
#define NIMCP_FEP_ORCHESTRATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Core FEP system */
#include "cognitive/free_energy/nimcp_free_energy.h"

/* Integration modules */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/nimcp_brain_kg_helpers.h"

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

#define FEP_ORCHESTRATOR_MAX_BRIDGES         256    /**< Max registered bridges */
#define FEP_ORCHESTRATOR_MAX_CATEGORIES      16     /**< Max bridge categories */
#define FEP_ORCHESTRATOR_DEFAULT_UPDATE_MS   50     /**< Default update interval */
#define FEP_ORCHESTRATOR_MODULE_NAME         "fep_orchestrator"

/* Default update intervals per category (milliseconds) — used as fallback baselines */
#define FEP_UPDATE_INTERVAL_PERCEPTION       10     /**< Fast: Perception bridges */
#define FEP_UPDATE_INTERVAL_ASYNC            10     /**< Fast: Async bridges */
#define FEP_UPDATE_INTERVAL_SECURITY         20     /**< Fast: Security bridges */
#define FEP_UPDATE_INTERVAL_COGNITIVE        50     /**< Medium: Cognitive bridges */
#define FEP_UPDATE_INTERVAL_SWARM            50     /**< Medium: Swarm bridges */
#define FEP_UPDATE_INTERVAL_MIDDLEWARE       50     /**< Medium: Middleware bridges */
#define FEP_UPDATE_INTERVAL_CORE             100    /**< Medium: Core bridges */
#define FEP_UPDATE_INTERVAL_GLIAL            100    /**< Medium: Glial bridges */
#define FEP_UPDATE_INTERVAL_PLASTICITY       1000   /**< Slow: Plasticity bridges */
#define FEP_UPDATE_INTERVAL_JEPA             25     /**< Medium-fast: JEPA bridges */

/* Continuous scheduling defaults */
#define FEP_CONTINUOUS_MIN_INTERVAL_MS       5.0f   /**< Absolute minimum interval */
#define FEP_CONTINUOUS_MAX_INTERVAL_MS       2000.0f /**< Absolute maximum interval */
#define FEP_CONTINUOUS_DECAY_RATE            3.0f   /**< Exponential decay rate for prediction error */
#define FEP_CONTINUOUS_FE_SCALE              2.0f   /**< Scaling factor for free energy sigmoid */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct fep_orchestrator fep_orchestrator_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Bridge category types
 *
 * WHAT: Categorization of FEP bridges by functional domain
 * WHY:  Different categories have different update timescales
 * HOW:  Hierarchical grouping based on biological timescales
 */
typedef enum {
    FEP_BRIDGE_CATEGORY_COGNITIVE = 0,     /**< Cognitive modules (hierarchical, meta-learning, etc.) */
    FEP_BRIDGE_CATEGORY_SWARM,             /**< Swarm coordination (brain, consensus, emergence, etc.) */
    FEP_BRIDGE_CATEGORY_SECURITY,          /**< Security modules (BBB, anomaly, rate limiter, etc.) */
    FEP_BRIDGE_CATEGORY_PLASTICITY,        /**< Plasticity mechanisms (predictive coding, STP, etc.) */
    FEP_BRIDGE_CATEGORY_MIDDLEWARE,        /**< Middleware (training, routing, bio-router, etc.) */
    FEP_BRIDGE_CATEGORY_PERCEPTION,        /**< Perception (visual, audio, speech - future expansion) */
    FEP_BRIDGE_CATEGORY_ASYNC,             /**< Async communication (bio-async, timescales, etc.) */
    FEP_BRIDGE_CATEGORY_GLIAL,             /**< Glial support (astrocytes, microglia, etc.) */
    FEP_BRIDGE_CATEGORY_CORE,              /**< Core structures (cortical columns, etc.) */
    FEP_BRIDGE_CATEGORY_JEPA,              /**< JEPA prediction (visual, speech, multimodal) */
    FEP_BRIDGE_CATEGORY_COUNT              /**< Total category count */
} fep_bridge_category_t;

/**
 * @brief Orchestrator operational state
 */
typedef enum {
    FEP_ORCHESTRATOR_STOPPED = 0,          /**< Not running */
    FEP_ORCHESTRATOR_STARTING,             /**< Initialization in progress */
    FEP_ORCHESTRATOR_RUNNING,              /**< Actively coordinating updates */
    FEP_ORCHESTRATOR_PAUSED,               /**< Paused (bridges registered but not updating) */
    FEP_ORCHESTRATOR_STOPPING,             /**< Shutdown in progress */
    FEP_ORCHESTRATOR_ERROR                 /**< Error state */
} fep_orchestrator_state_t;

/* ============================================================================
 * Bridge Registration Structures
 * ============================================================================ */

/**
 * @brief Generic bridge handle (opaque pointer to any FEP bridge type)
 */
typedef void* fep_bridge_handle_t;

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
typedef int (*fep_bridge_update_fn_t)(fep_bridge_handle_t bridge);

/**
 * @brief Bridge destroy callback
 *
 * @param bridge Bridge handle
 */
typedef void (*fep_bridge_destroy_fn_t)(fep_bridge_handle_t bridge);

/**
 * @brief Registered bridge entry
 */
typedef struct {
    uint32_t bridge_id;                    /**< Unique bridge ID */
    const char* bridge_name;               /**< Human-readable name */
    fep_bridge_category_t category;        /**< Bridge category */
    fep_bridge_handle_t handle;            /**< Opaque bridge pointer */
    fep_bridge_update_fn_t update_fn;      /**< Update callback */
    fep_bridge_destroy_fn_t destroy_fn;    /**< Destroy callback (optional) */
    uint64_t last_update_time;             /**< Last update timestamp (ms) */
    uint64_t update_count;                 /**< Total updates */
    uint64_t total_update_time_us;         /**< Cumulative update time (microseconds) */
    bool enabled;                          /**< Whether updates are enabled */
    brain_kg_node_id_t kg_node_id;         /**< KG node ID for this bridge */
} fep_bridge_entry_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Continuous scheduling parameters
 *
 * WHAT: Parameters for computing update intervals from FEP metrics
 * WHY:  Replace discrete fixed intervals with smooth curves driven by
 *       prediction error, free energy, and surprise — higher urgency
 *       (more prediction error / free energy) produces shorter intervals
 * HOW:  interval = min + (max - min) * exp(-decay_rate * prediction_error)
 *                  * 1 / (1 + fe_scale * free_energy)
 */
typedef struct {
    bool enabled;                          /**< Enable continuous scheduling (false = fixed) */
    float min_interval_ms;                 /**< Absolute minimum interval floor */
    float max_interval_ms;                 /**< Maximum interval when system is calm */
    float decay_rate;                      /**< Exponential decay rate for prediction error */
    float fe_scale;                        /**< Scaling factor for free energy sigmoid term */
} fep_continuous_schedule_config_t;

/**
 * @brief Current FEP metrics used to drive continuous scheduling
 *
 * WHAT: Snapshot of FEP state metrics for scheduling computation
 * WHY:  Decouples metric source from scheduling — callers provide metrics,
 *       orchestrator computes intervals
 * HOW:  Updated each cycle via fep_orchestrator_set_fep_metrics() or
 *       automatically from connected FEP system
 */
typedef struct {
    float prediction_error;                /**< Current prediction error magnitude (>= 0) */
    float free_energy;                     /**< Current variational free energy (>= 0) */
    float surprise;                        /**< Current surprise -ln p(o) (>= 0) */
} fep_scheduling_metrics_t;

/**
 * @brief Per-category configuration
 */
typedef struct {
    bool enabled;                          /**< Enable this category */
    uint64_t update_interval_ms;           /**< Update interval — fixed fallback (milliseconds) */
    uint64_t last_update_time;             /**< Last category update (milliseconds) */
    float continuous_interval_ms;          /**< Current continuously-computed interval */
} fep_category_config_t;

/**
 * @brief FEP orchestrator configuration
 */
typedef struct {
    /* Category settings */
    fep_category_config_t categories[FEP_BRIDGE_CATEGORY_COUNT];

    /* Global settings */
    uint32_t max_bridges;                  /**< Maximum registered bridges */
    bool enable_auto_update;               /**< Auto-update on timer (vs manual) */
    uint64_t global_update_interval_ms;    /**< Override all category intervals */

    /* Integration enables */
    bool enable_bio_async;                 /**< Enable bio-async integration */
    bool enable_brain_immune;              /**< Enable brain immune integration */
    bool enable_statistics;                /**< Track detailed statistics */
    bool enable_logging;                   /**< Enable logging */

    /* Performance tuning */
    uint32_t max_updates_per_cycle;        /**< Max bridges to update per cycle (0=all) */
    float update_time_budget_ms;           /**< Max time per update cycle (0=unlimited) */

    /* Continuous scheduling */
    fep_continuous_schedule_config_t continuous_schedule; /**< Continuous scheduling params */
} fep_orchestrator_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Per-category statistics
 */
typedef struct {
    uint32_t bridge_count;                 /**< Bridges in this category */
    uint64_t total_updates;                /**< Total updates for category */
    float avg_update_time_us;              /**< Average update time (microseconds) */
    float max_update_time_us;              /**< Max update time */
    uint64_t last_update_time;             /**< Last update timestamp */
} fep_category_stats_t;

/**
 * @brief Orchestrator-wide statistics
 */
typedef struct {
    /* Bridge counts */
    uint32_t total_bridges;                /**< Total registered bridges */
    uint32_t active_bridges;               /**< Currently enabled bridges */
    fep_category_stats_t categories[FEP_BRIDGE_CATEGORY_COUNT];

    /* Update performance */
    uint64_t total_update_cycles;          /**< Total orchestrator update calls */
    uint64_t total_bridge_updates;         /**< Total individual bridge updates */
    float avg_cycle_time_us;               /**< Average cycle time (microseconds) */
    float max_cycle_time_us;               /**< Max cycle time */

    /* Integration stats */
    uint64_t bio_async_messages_sent;      /**< Bio-async messages sent */
    uint64_t bio_async_messages_received;  /**< Bio-async messages received */
    uint64_t immune_modulations;           /**< Immune-driven modulations */

    /* Health metrics */
    uint32_t update_errors;                /**< Failed updates */
    uint32_t overrun_count;                /**< Times update exceeded budget */
    float system_load;                     /**< Current system load (0-1) */
} fep_orchestrator_stats_t;

/* ============================================================================
 * Main Orchestrator Structure
 * ============================================================================ */

/**
 * @brief FEP orchestrator state
 */
struct fep_orchestrator {
    fep_orchestrator_config_t config;      /**< Configuration */
    fep_orchestrator_state_t state;        /**< Current state */

    /* Bridge registry */
    fep_bridge_entry_t* bridges;           /**< Registered bridges array */
    uint32_t bridge_count;                 /**< Current bridge count */
    uint32_t bridge_capacity;              /**< Array capacity */
    uint32_t next_bridge_id;               /**< Next bridge ID */

    /* Integration handles */
    bio_module_context_t bio_context;      /**< Bio-async context */
    brain_immune_system_t* brain_immune;   /**< Brain immune system */

    /* Statistics */
    fep_orchestrator_stats_t stats;

    /* Continuous scheduling state */
    fep_scheduling_metrics_t fep_metrics;  /**< Current FEP metrics for scheduling */

    /* Timing */
    uint64_t start_time;                   /**< Orchestrator start time (ms) */
    uint64_t last_update_time;             /**< Last global update (ms) */

    /* Thread safety */
    nimcp_mutex_t* mutex;                  /**< Thread-safe operations */

    /* Runtime state */
    bool bio_async_connected;              /**< Bio-async is connected */
    bool immune_connected;                 /**< Brain immune is connected */

    /* Internal Knowledge Graph integration */
    kg_module_context_t kg_context;        /**< KG access context */
    bool kg_connected;                     /**< Internal KG is connected */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default orchestrator configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible timescales
 * HOW:  Set category-specific update intervals and enable flags
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_default_config(fep_orchestrator_config_t* config);

/**
 * @brief Create FEP orchestrator
 *
 * WHAT: Initialize orchestrator infrastructure
 * WHY:  Central coordination point for all FEP bridges
 * HOW:  Allocate bridge registry, initialize statistics, register with bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return New orchestrator or NULL on failure
 */
fep_orchestrator_t* fep_orchestrator_create(const fep_orchestrator_config_t* config);

/**
 * @brief Destroy FEP orchestrator
 *
 * WHAT: Clean up orchestrator resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister all bridges, disconnect integrations, free memory
 *
 * NOTE: Does NOT destroy registered bridges (caller responsibility)
 *
 * @param orchestrator Orchestrator to destroy (NULL safe)
 */
void fep_orchestrator_destroy(fep_orchestrator_t* orchestrator);

/**
 * @brief Start orchestrator
 *
 * WHAT: Begin coordinated bridge updates
 * WHY:  Activate system-wide FEP processing
 * HOW:  Set state to RUNNING, initialize timing
 *
 * @param orchestrator Orchestrator
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_start(fep_orchestrator_t* orchestrator);

/**
 * @brief Stop orchestrator
 *
 * WHAT: Halt coordinated bridge updates
 * WHY:  Graceful shutdown
 * HOW:  Set state to STOPPED, complete pending updates
 *
 * @param orchestrator Orchestrator
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_stop(fep_orchestrator_t* orchestrator);

/**
 * @brief Pause orchestrator
 *
 * WHAT: Temporarily suspend updates
 * WHY:  Debugging, checkpointing, or system maintenance
 * HOW:  Set state to PAUSED
 *
 * @param orchestrator Orchestrator
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_pause(fep_orchestrator_t* orchestrator);

/**
 * @brief Resume orchestrator
 *
 * WHAT: Resume updates after pause
 * WHY:  Return to normal operation
 * HOW:  Set state to RUNNING
 *
 * @param orchestrator Orchestrator
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_resume(fep_orchestrator_t* orchestrator);

/* ============================================================================
 * Bridge Registration API
 * ============================================================================ */

/**
 * @brief Register FEP bridge
 *
 * WHAT: Add bridge to orchestrator registry
 * WHY:  Enable coordinated updates across all bridges
 * HOW:  Store bridge handle, update function, category
 *
 * @param orchestrator Orchestrator
 * @param name Bridge name (e.g., "hierarchical_fep_bridge")
 * @param category Bridge category
 * @param handle Bridge handle (opaque pointer)
 * @param update_fn Update callback
 * @param destroy_fn Destroy callback (NULL if orchestrator shouldn't destroy)
 * @param bridge_id_out Output: assigned bridge ID
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_register_bridge(
    fep_orchestrator_t* orchestrator,
    const char* name,
    fep_bridge_category_t category,
    fep_bridge_handle_t handle,
    fep_bridge_update_fn_t update_fn,
    fep_bridge_destroy_fn_t destroy_fn,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister FEP bridge
 *
 * WHAT: Remove bridge from orchestrator
 * WHY:  Dynamic system reconfiguration
 * HOW:  Find and remove bridge entry
 *
 * NOTE: Does NOT call destroy_fn (caller responsibility)
 *
 * @param orchestrator Orchestrator
 * @param bridge_id Bridge ID to unregister
 * @return 0 on success, -1 if not found
 */
int fep_orchestrator_unregister_bridge(
    fep_orchestrator_t* orchestrator,
    uint32_t bridge_id
);

/**
 * @brief Enable/disable specific bridge
 *
 * WHAT: Toggle bridge update participation
 * WHY:  Selective activation without unregistering
 * HOW:  Set bridge enabled flag
 *
 * @param orchestrator Orchestrator
 * @param bridge_id Bridge ID
 * @param enabled Enable (true) or disable (false)
 * @return 0 on success, -1 if not found
 */
int fep_orchestrator_set_bridge_enabled(
    fep_orchestrator_t* orchestrator,
    uint32_t bridge_id,
    bool enabled
);

/**
 * @brief Get bridge by ID
 *
 * @param orchestrator Orchestrator
 * @param bridge_id Bridge ID
 * @return Bridge entry or NULL if not found
 */
const fep_bridge_entry_t* fep_orchestrator_get_bridge(
    const fep_orchestrator_t* orchestrator,
    uint32_t bridge_id
);

/**
 * @brief Get all bridges in category
 *
 * @param orchestrator Orchestrator
 * @param category Category
 * @param bridges Output array (caller allocated)
 * @param max_bridges Array capacity
 * @return Number of bridges written
 */
uint32_t fep_orchestrator_get_bridges_by_category(
    const fep_orchestrator_t* orchestrator,
    fep_bridge_category_t category,
    const fep_bridge_entry_t** bridges,
    uint32_t max_bridges
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main orchestrator update
 *
 * WHAT: Update all bridges according to their category intervals
 * WHY:  Core of orchestrator - coordinates distributed FEP minimization
 * HOW:  Check each category's interval, update due bridges, track statistics
 *
 * NOTE: Call this regularly from main loop (e.g., every 10ms)
 *
 * @param orchestrator Orchestrator
 * @param current_time_ms Current time (milliseconds)
 * @return Number of bridges updated, -1 on error
 */
int fep_orchestrator_update(
    fep_orchestrator_t* orchestrator,
    uint64_t current_time_ms
);

/**
 * @brief Update specific category
 *
 * WHAT: Update all bridges in one category
 * WHY:  Fine-grained control over update timing
 * HOW:  Iterate enabled bridges in category, call update_fn
 *
 * @param orchestrator Orchestrator
 * @param category Category to update
 * @param current_time_ms Current time (milliseconds)
 * @return Number of bridges updated, -1 on error
 */
int fep_orchestrator_update_category(
    fep_orchestrator_t* orchestrator,
    fep_bridge_category_t category,
    uint64_t current_time_ms
);

/**
 * @brief Update specific bridge
 *
 * WHAT: Update single bridge by ID
 * WHY:  Manual override for specific bridge
 * HOW:  Find bridge, call update_fn
 *
 * @param orchestrator Orchestrator
 * @param bridge_id Bridge ID
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_update_bridge(
    fep_orchestrator_t* orchestrator,
    uint32_t bridge_id
);

/**
 * @brief Force update all bridges immediately
 *
 * WHAT: Ignore intervals, update everything now
 * WHY:  Emergency synchronization or initialization
 * HOW:  Update all enabled bridges regardless of timing
 *
 * @param orchestrator Orchestrator
 * @return Number of bridges updated, -1 on error
 */
int fep_orchestrator_force_update_all(fep_orchestrator_t* orchestrator);

/* ============================================================================
 * Category Configuration API
 * ============================================================================ */

/**
 * @brief Set category update interval
 *
 * WHAT: Change update frequency for category
 * WHY:  Runtime tuning of timescales
 * HOW:  Update config struct
 *
 * @param orchestrator Orchestrator
 * @param category Category
 * @param interval_ms New interval (milliseconds)
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_set_update_interval(
    fep_orchestrator_t* orchestrator,
    fep_bridge_category_t category,
    uint64_t interval_ms
);

/**
 * @brief Enable/disable entire category
 *
 * @param orchestrator Orchestrator
 * @param category Category
 * @param enabled Enable/disable
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_set_category_enabled(
    fep_orchestrator_t* orchestrator,
    fep_bridge_category_t category,
    bool enabled
);

/**
 * @brief Get category configuration
 *
 * @param orchestrator Orchestrator
 * @param category Category
 * @param config Output config
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_get_category_config(
    const fep_orchestrator_t* orchestrator,
    fep_bridge_category_t category,
    fep_category_config_t* config
);

/* ============================================================================
 * Continuous Scheduling API
 * ============================================================================ */

/**
 * @brief Compute update interval from FEP metrics
 *
 * WHAT: Derive a continuous update interval from prediction error, free energy,
 *       and surprise using smooth mathematical functions
 * WHY:  Replaces discrete category-based intervals with adaptive scheduling
 *       that responds to the system's actual cognitive state
 * HOW:  Uses exponential decay for prediction error urgency and a sigmoid
 *       for free energy modulation:
 *
 *       base = max_interval * exp(-decay_rate * prediction_error)
 *       interval = base / (1 + fe_scale * free_energy)
 *       result = clamp(interval, min_interval, max_interval)
 *
 *       High prediction error → short interval (urgent updates)
 *       High free energy → shorter interval (model needs refinement)
 *       Low surprise + low error → long interval (system is calm)
 *
 * @param prediction_error  Current prediction error magnitude (>= 0)
 * @param free_energy       Current variational free energy (>= 0)
 * @param schedule_config   Continuous scheduling parameters
 * @param interval_ms       Output: computed interval in milliseconds
 * @return 0 on success, -1 on error
 */
int fep_compute_update_interval(
    float prediction_error,
    float free_energy,
    const fep_continuous_schedule_config_t* schedule_config,
    float* interval_ms
);

/**
 * @brief Set FEP metrics for continuous scheduling
 *
 * WHAT: Provide current FEP state metrics to the orchestrator
 * WHY:  Drives continuous scheduling — intervals adapt to cognitive state
 * HOW:  Stores metrics and recomputes all category intervals
 *
 * @param orchestrator Orchestrator
 * @param metrics      Current FEP metrics
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_set_fep_metrics(
    fep_orchestrator_t* orchestrator,
    const fep_scheduling_metrics_t* metrics
);

/**
 * @brief Get current FEP scheduling metrics
 *
 * @param orchestrator Orchestrator
 * @param metrics      Output metrics
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_get_fep_metrics(
    const fep_orchestrator_t* orchestrator,
    fep_scheduling_metrics_t* metrics
);

/**
 * @brief Derive discrete category label from continuous interval
 *
 * WHAT: Map a continuous interval to a discrete category name for logging
 * WHY:  Backward compatibility — existing log parsers expect category labels
 * HOW:  Threshold-based mapping:
 *       <= 15ms → "fast", <= 75ms → "medium", > 75ms → "slow"
 *
 * @param interval_ms  Continuous interval in milliseconds
 * @return Human-readable scheduling tier label
 */
const char* fep_scheduling_tier_label(float interval_ms);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Integrate orchestrator with brain immune
 * WHY:  Enable immune modulation of FEP processing
 * HOW:  Store handle, register for cytokine callbacks
 *
 * IMMUNE-FEP MODULATION:
 * - Inflammation → Reduce learning rates across all bridges
 * - IL-1β/IL-6 → Reduce precision (broaden attention)
 * - IL-10 → Restore normal FEP processing
 * - IFN-γ → Sharpen precision (focus on threat detection)
 *
 * @param orchestrator Orchestrator
 * @param immune Brain immune system
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_connect_brain_immune(
    fep_orchestrator_t* orchestrator,
    brain_immune_system_t* immune
);

/**
 * @brief Disconnect from brain immune
 *
 * @param orchestrator Orchestrator
 * @return 0 on success
 */
int fep_orchestrator_disconnect_brain_immune(fep_orchestrator_t* orchestrator);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register orchestrator with bio-async
 * WHY:  Enable inter-bridge messaging and coordination
 * HOW:  Register module, set up handlers
 *
 * @param orchestrator Orchestrator
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_connect_bio_async(fep_orchestrator_t* orchestrator);

/**
 * @brief Disconnect from bio-async
 *
 * @param orchestrator Orchestrator
 * @return 0 on success
 */
int fep_orchestrator_disconnect_bio_async(fep_orchestrator_t* orchestrator);

/**
 * @brief Connect to internal knowledge graph
 *
 * WHAT: Wire orchestrator to brain's internal KG
 * WHY:  Enable topology-aware bridge discovery and state tracking
 * HOW:  Initialize KG context, find orchestrator node, cache reference
 *
 * @param orchestrator Orchestrator
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_connect_internal_kg(
    fep_orchestrator_t* orchestrator,
    brain_t brain
);

/**
 * @brief Disconnect from internal KG
 *
 * @param orchestrator Orchestrator
 * @return 0 on success
 */
int fep_orchestrator_disconnect_internal_kg(fep_orchestrator_t* orchestrator);

/**
 * @brief Get bridges connected to a module via KG
 *
 * WHAT: Find bridges that coordinate with a specific module
 * WHY:  Enable topology-aware coordination queries
 * HOW:  Query KG for edges from orchestrator to bridges connected to module
 *
 * @param orchestrator Orchestrator
 * @param module_name Name of module to query
 * @param bridges Output array (caller allocated)
 * @param max_bridges Array capacity
 * @return Number of connected bridges
 */
uint32_t fep_orchestrator_get_bridges_for_module(
    const fep_orchestrator_t* orchestrator,
    const char* module_name,
    const fep_bridge_entry_t** bridges,
    uint32_t max_bridges
);

/**
 * @brief Get topology summary via KG
 *
 * @param orchestrator Orchestrator
 * @param summary Output buffer
 * @param summary_size Buffer size
 * @return Characters written, -1 on error
 */
int fep_orchestrator_get_topology_summary(
    const fep_orchestrator_t* orchestrator,
    char* summary,
    size_t summary_size
);

/* ============================================================================
 * Statistics and Monitoring API
 * ============================================================================ */

/**
 * @brief Get orchestrator statistics
 *
 * @param orchestrator Orchestrator
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int fep_orchestrator_get_stats(
    const fep_orchestrator_t* orchestrator,
    fep_orchestrator_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param orchestrator Orchestrator
 */
void fep_orchestrator_reset_stats(fep_orchestrator_t* orchestrator);

/**
 * @brief Get current system load
 *
 * WHAT: Estimate orchestrator computational load
 * WHY:  Adaptive scheduling based on system stress
 * HOW:  Ratio of actual update time to available time budget
 *
 * @param orchestrator Orchestrator
 * @return Load factor (0-1, >1 indicates overload)
 */
float fep_orchestrator_get_load(const fep_orchestrator_t* orchestrator);

/**
 * @brief Get current state
 *
 * @param orchestrator Orchestrator
 * @return Current state
 */
fep_orchestrator_state_t fep_orchestrator_get_state(
    const fep_orchestrator_t* orchestrator
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
const char* fep_bridge_category_to_string(fep_bridge_category_t category);

/**
 * @brief Convert state to string
 *
 * @param state State
 * @return Human-readable string
 */
const char* fep_orchestrator_state_to_string(fep_orchestrator_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_ORCHESTRATOR_H */
