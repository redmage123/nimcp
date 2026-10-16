/**
 * @file nimcp_swarm_module_registry.h
 * @brief Swarm Module Registry - Plugin Architecture for Swarm Behaviors
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Plugin registry for swarm behavioral modules (flocking, pheromone, memory,
 *       immune, consensus, etc.) with automatic wiring to swarm_brain and priority-
 *       based conflict resolution.
 * WHY:  Swarm intelligence emerges from multiple concurrent behaviors. Need unified
 *       framework to register, manage, and coordinate heterogeneous swarm modules.
 * HOW:  Registry Pattern + Plugin Pattern + Chain of Responsibility for arbitration.
 *       Modules register via standardized interface, registry manages lifecycle,
 *       priority system resolves behavioral conflicts.
 *
 * BIOLOGICAL INSPIRATION:
 * ==================================================================================
 *
 * HIERARCHICAL BEHAVIORAL SELECTION:
 * ----------------------------------
 * Animal behavior selection (Tinbergen's hierarchical model):
 * - Multiple concurrent motivations (foraging, mating, escape)
 * - Priority-based arbitration (predator > hunger > curiosity)
 * - Context-sensitive weighting (hunger increases over time)
 * - Modular behavioral circuits (central pattern generators)
 *
 * This registry implements behavioral module management similar to how
 * brain stem/hypothalamus coordinates competing drives.
 *
 * SWARM COORDINATION:
 * -------------------
 * Natural swarms coordinate multiple behaviors:
 * - Bees: Waggle dance (communication) + thermoregulation + foraging
 * - Ants: Pheromone trails + nest defense + brood care
 * - Birds: Flocking + predator evasion + migration
 *
 * Each behavior is a module with clear interfaces. Registry enables
 * dynamic composition of swarm intelligence from primitive behaviors.
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                 SWARM MODULE REGISTRY (Plugin Coordinator)                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                     MODULE CATEGORIES                               │  ║
 * ║   │                                                                     │  ║
 * ║   │  Movement         Communication       Memory          Defense      │  ║
 * ║   │  ────────         ─────────────       ──────          ───────      │  ║
 * ║   │  • Flocking       • Signal            • Memory        • Immune     │  ║
 * ║   │  • Formation      • Pheromone         • Pattern       • BFT        │  ║
 * ║   │                   • Emotional                                       │  ║
 * ║   │  Coordination     Emergence           Learning                     │  ║
 * ║   │  ─────────────    ─────────           ────────                     │  ║
 * ║   │  • Consensus      • Swarm Brain       • Consolidation              │  ║
 * ║   │  • Quorum         • Consciousness     • Knowledge                  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    REGISTRY CORE                                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────────┐   ┌─────────────────┐   ┌────────────────┐  │  ║
 * ║   │   │ Module Storage  │   │ Lifecycle Mgmt  │   │   Statistics   │  │  ║
 * ║   │   │ ─────────────   │   │ ─────────────   │   │   ──────────   │  │  ║
 * ║   │   │ Hash table by   │   │ Enable/Disable  │   │ Call counts    │  │  ║
 * ║   │   │ ID + Category   │   │ Priority config │   │ Perf metrics   │  │  ║
 * ║   │   │ lists           │   │ Init/Shutdown   │   │ Error tracking │  │  ║
 * ║   │   └─────────────────┘   └─────────────────┘   └────────────────┘  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 PRIORITY ARBITRATION ENGINE                         │  ║
 * ║   │                                                                     │  ║
 * ║   │  Behavioral Conflict Resolution (Chain of Responsibility)          │  ║
 * ║   │  ─────────────────────────────────────────────────────────────────  │  ║
 * ║   │  Example: Flocking vs Evasion                                      │  ║
 * ║   │    1. Immune detects threat → Priority = 10 (CRITICAL)             │  ║
 * ║   │    2. Flocking cohesion → Priority = 5 (NORMAL)                    │  ║
 * ║   │    3. Arbitrator selects evasion, suppresses flocking              │  ║
 * ║   │                                                                     │  ║
 * ║   │  Strategies: HIGHEST_PRIORITY, WEIGHTED_BLEND, SEQUENTIAL          │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 INTEGRATION LAYER                                   │  ║
 * ║   │                                                                     │  ║
 * ║   │   Swarm Brain Wiring         Bio-Async Router                      │  ║
 * ║   │   ──────────────────         ──────────────────                    │  ║
 * ║   │   Auto-connect modules       Inter-module messaging                │  ║
 * ║   │   to swarm_brain             Module discovery broadcasts           │  ║
 * ║   │   Lifecycle coordination     Event notifications                   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Registry: Central storage and lookup for modules
 * - Plugin: Standardized interface for heterogeneous modules
 * - Chain of Responsibility: Priority-based conflict resolution
 * - Observer: Event callbacks for module state changes
 * - Facade: Simplified interface over complex module management
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SWARM_MODULE_REGISTRY_H
#define NIMCP_SWARM_MODULE_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Swarm system dependencies */
#include "swarm/nimcp_swarm_brain.h"
#include "swarm/nimcp_swarm_flocking.h"
#include "swarm/nimcp_swarm_pheromone.h"
#include "swarm/nimcp_swarm_memory.h"
#include "swarm/nimcp_swarm_immune.h"
#include "swarm/nimcp_swarm_consensus.h"

/* Integration modules */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* Utilities */
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"

/* Internal Knowledge Graph integration */
#include "core/brain/nimcp_brain_kg_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SWARM_REGISTRY_MAX_MODULES           128    /**< Max registered modules */
#define SWARM_REGISTRY_MAX_CATEGORIES        16     /**< Max module categories */
#define SWARM_REGISTRY_MODULE_NAME_LEN       64     /**< Max module name length */
#define SWARM_REGISTRY_BIO_MODULE_ID         BIO_MODULE_SWARM_REGISTRY

/** Priority levels for conflict resolution */
#define SWARM_PRIORITY_IDLE                  0      /**< No urgency */
#define SWARM_PRIORITY_LOW                   3      /**< Background behaviors */
#define SWARM_PRIORITY_NORMAL                5      /**< Default behaviors */
#define SWARM_PRIORITY_HIGH                  7      /**< Important behaviors */
#define SWARM_PRIORITY_CRITICAL              10     /**< Emergency/survival */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct swarm_module_registry swarm_module_registry_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Module category types
 *
 * WHAT: Functional categorization of swarm behaviors
 * WHY:  Groups related modules for easier discovery and coordination
 * HOW:  Hierarchical grouping based on behavioral function
 */
typedef enum {
    SWARM_MODULE_CATEGORY_MOVEMENT = 0,    /**< Movement (flocking, formation) */
    SWARM_MODULE_CATEGORY_COMMUNICATION,   /**< Communication (signal, pheromone) */
    SWARM_MODULE_CATEGORY_MEMORY,          /**< Memory (consolidation, learning) */
    SWARM_MODULE_CATEGORY_DEFENSE,         /**< Defense (immune, BFT) */
    SWARM_MODULE_CATEGORY_COORDINATION,    /**< Coordination (consensus, quorum) */
    SWARM_MODULE_CATEGORY_EMERGENCE,       /**< Emergence (brain, consciousness) */
    SWARM_MODULE_CATEGORY_LEARNING,        /**< Learning (pattern, knowledge) */
    SWARM_MODULE_CATEGORY_CUSTOM,          /**< User-defined category */
    SWARM_MODULE_CATEGORY_COUNT
} swarm_module_category_t;

/**
 * @brief Module lifecycle state
 */
typedef enum {
    SWARM_MODULE_STATE_UNINITIALIZED = 0,  /**< Not initialized */
    SWARM_MODULE_STATE_INITIALIZED,        /**< Initialized, not running */
    SWARM_MODULE_STATE_ACTIVE,             /**< Running and enabled */
    SWARM_MODULE_STATE_DISABLED,           /**< Initialized but disabled */
    SWARM_MODULE_STATE_ERROR,              /**< Error state */
    SWARM_MODULE_STATE_SHUTDOWN            /**< Shutdown/destroyed */
} swarm_module_state_t;

/**
 * @brief Arbitration strategy for conflict resolution
 */
typedef enum {
    SWARM_ARBITRATION_HIGHEST_PRIORITY = 0, /**< Winner-take-all (highest priority) */
    SWARM_ARBITRATION_WEIGHTED_BLEND,       /**< Blend outputs weighted by priority */
    SWARM_ARBITRATION_SEQUENTIAL,           /**< Execute in priority order */
    SWARM_ARBITRATION_CUSTOM                /**< User-defined arbitration */
} swarm_arbitration_strategy_t;

/* ============================================================================
 * Module Interface Structures
 * ============================================================================ */

/**
 * @brief Generic module handle (opaque pointer to any swarm module type)
 */
typedef void* swarm_module_handle_t;

/**
 * @brief Module initialization callback
 *
 * WHAT: Initialize module with registry context
 * WHY:  Modules may need registry reference for inter-module communication
 * HOW:  Called once during registration
 *
 * @param module Module handle
 * @param registry Registry handle for inter-module access
 * @return 0 on success, -1 on error
 */
typedef int (*swarm_module_init_fn_t)(
    swarm_module_handle_t module,
    swarm_module_registry_t* registry
);

/**
 * @brief Module update callback
 *
 * WHAT: Update module state for one time step
 * WHY:  Core behavior execution
 * HOW:  Called regularly by registry or swarm_brain
 *
 * @param module Module handle
 * @param delta_time_ms Time since last update (milliseconds)
 * @return 0 on success, -1 on error
 */
typedef int (*swarm_module_update_fn_t)(
    swarm_module_handle_t module,
    uint64_t delta_time_ms
);

/**
 * @brief Module destroy callback
 *
 * @param module Module handle
 */
typedef void (*swarm_module_destroy_fn_t)(swarm_module_handle_t module);

/**
 * @brief Module enable callback (optional)
 *
 * @param module Module handle
 * @param enabled Enable (true) or disable (false)
 * @return 0 on success, -1 on error
 */
typedef int (*swarm_module_enable_fn_t)(
    swarm_module_handle_t module,
    bool enabled
);

/**
 * @brief Module interface descriptor
 *
 * WHAT: Standardized interface for all swarm behavioral modules
 * WHY:  Enables heterogeneous modules to be managed uniformly
 * HOW:  Function pointers for lifecycle and behavior
 */
typedef struct {
    swarm_module_init_fn_t init_fn;       /**< Initialization callback */
    swarm_module_update_fn_t update_fn;   /**< Update callback */
    swarm_module_destroy_fn_t destroy_fn; /**< Destroy callback */
    swarm_module_enable_fn_t enable_fn;   /**< Enable/disable callback (optional) */
} swarm_module_interface_t;

/* ============================================================================
 * Module Registration Structures
 * ============================================================================ */

/**
 * @brief Registered module entry
 */
typedef struct {
    uint32_t module_id;                         /**< Unique module ID */
    char module_name[SWARM_REGISTRY_MODULE_NAME_LEN]; /**< Human-readable name */
    swarm_module_category_t category;           /**< Module category */
    swarm_module_handle_t handle;               /**< Opaque module pointer */
    swarm_module_interface_t interface;         /**< Module callbacks */

    /* State and priority */
    swarm_module_state_t state;                 /**< Current state */
    uint32_t priority;                          /**< Priority (0-10) */
    bool enabled;                               /**< Whether module is active */

    /* Statistics */
    uint64_t last_update_time;                  /**< Last update timestamp (ms) */
    uint64_t update_count;                      /**< Total updates */
    uint64_t total_update_time_us;              /**< Cumulative update time (us) */
    uint32_t error_count;                       /**< Update errors */

    /* Integration */
    bool wired_to_swarm_brain;                  /**< Connected to swarm_brain */
    bio_module_id_t bio_module_id;              /**< Bio-async module ID (if registered) */

    /* User data */
    void* user_data;                            /**< Optional user data */

    /* Internal Knowledge Graph integration */
    brain_kg_node_id_t kg_node_id;              /**< KG node ID for this module */
} swarm_module_entry_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Per-category configuration
 */
typedef struct {
    bool enabled;                               /**< Enable this category */
    uint32_t default_priority;                  /**< Default priority for category */
    uint64_t update_interval_ms;                /**< Update interval (ms, 0=every cycle) */
} swarm_category_config_t;

/**
 * @brief Swarm module registry configuration
 */
typedef struct {
    /* Category settings */
    swarm_category_config_t categories[SWARM_MODULE_CATEGORY_COUNT];

    /* Global settings */
    uint32_t max_modules;                       /**< Maximum registered modules */
    swarm_arbitration_strategy_t arbitration;   /**< Conflict resolution strategy */
    bool enable_auto_wiring;                    /**< Auto-wire to swarm_brain */
    bool enable_bio_async;                      /**< Enable bio-async integration */
    bool enable_statistics;                     /**< Track detailed statistics */

    /* Performance tuning */
    uint32_t max_updates_per_cycle;             /**< Max modules per update cycle (0=all) */
    float update_time_budget_ms;                /**< Max time per cycle (0=unlimited) */
} swarm_registry_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Per-category statistics
 */
typedef struct {
    uint32_t module_count;                      /**< Modules in this category */
    uint64_t total_updates;                     /**< Total updates for category */
    float avg_update_time_us;                   /**< Average update time (us) */
    float max_update_time_us;                   /**< Max update time */
} swarm_category_stats_t;

/**
 * @brief Registry-wide statistics
 */
typedef struct {
    /* Module counts */
    uint32_t total_modules;                     /**< Total registered modules */
    uint32_t active_modules;                    /**< Currently enabled modules */
    swarm_category_stats_t categories[SWARM_MODULE_CATEGORY_COUNT];

    /* Update performance */
    uint64_t total_update_cycles;               /**< Total registry update calls */
    uint64_t total_module_updates;              /**< Total individual module updates */
    float avg_cycle_time_us;                    /**< Average cycle time (us) */
    float max_cycle_time_us;                    /**< Max cycle time */

    /* Arbitration */
    uint64_t conflicts_resolved;                /**< Behavioral conflicts resolved */
    uint64_t priority_overrides;                /**< High-priority overrides */

    /* Integration */
    uint32_t wired_to_brain;                    /**< Modules wired to swarm_brain */
    uint32_t bio_async_registered;              /**< Modules registered with bio-async */

    /* Health */
    uint32_t total_errors;                      /**< Cumulative errors */
    uint32_t overrun_count;                     /**< Times update exceeded budget */
} swarm_registry_stats_t;

/* ============================================================================
 * Main Registry Structure
 * ============================================================================ */

/**
 * @brief Swarm module registry state
 */
struct swarm_module_registry {
    swarm_registry_config_t config;             /**< Configuration */

    /* Module storage */
    swarm_module_entry_t* modules;              /**< Registered modules array */
    uint32_t module_count;                      /**< Current module count */
    uint32_t module_capacity;                   /**< Array capacity */
    uint32_t next_module_id;                    /**< Next module ID */

    /* Integration handles */
    swarm_brain_t* swarm_brain;                 /**< Swarm brain coordinator */
    bio_module_context_t bio_context;           /**< Bio-async context */
    brain_immune_system_t* brain_immune;        /**< Brain immune system */

    /* Statistics */
    swarm_registry_stats_t stats;

    /* Timing */
    uint64_t start_time;                        /**< Registry start time (ms) */
    uint64_t last_update_time;                  /**< Last global update (ms) */

    /* Thread safety */
    nimcp_mutex_t* mutex;                       /**< Thread-safe operations */

    /* Runtime state */
    bool bio_async_connected;                   /**< Bio-async is connected */
    bool brain_connected;                       /**< Swarm brain is connected */

    /* Internal Knowledge Graph integration */
    kg_module_context_t kg_context;             /**< KG access context */
    bool kg_connected;                          /**< Internal KG is connected */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default registry configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with standard settings
 * HOW:  Set category-specific defaults and enable flags
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int swarm_registry_default_config(swarm_registry_config_t* config);

/**
 * @brief Create swarm module registry
 *
 * WHAT: Initialize registry infrastructure
 * WHY:  Central coordination point for all swarm modules
 * HOW:  Allocate module storage, initialize statistics
 *
 * @param config Configuration (NULL for defaults)
 * @return New registry or NULL on failure
 */
swarm_module_registry_t* swarm_registry_create(
    const swarm_registry_config_t* config
);

/**
 * @brief Destroy swarm module registry
 *
 * WHAT: Clean up registry resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister all modules, disconnect integrations, free memory
 *
 * NOTE: Does NOT destroy registered modules (caller responsibility)
 *
 * @param registry Registry to destroy (NULL safe)
 */
void swarm_registry_destroy(swarm_module_registry_t* registry);

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

/**
 * @brief Register swarm module
 *
 * WHAT: Add module to registry
 * WHY:  Enable coordinated updates and conflict resolution
 * HOW:  Store module handle, interface, category
 *
 * @param registry Registry
 * @param name Module name (e.g., "flocking_engine")
 * @param category Module category
 * @param handle Module handle (opaque pointer)
 * @param interface Module callbacks
 * @param priority Initial priority (0-10)
 * @param module_id_out Output: assigned module ID
 * @return 0 on success, -1 on error
 */
int swarm_registry_register_module(
    swarm_module_registry_t* registry,
    const char* name,
    swarm_module_category_t category,
    swarm_module_handle_t handle,
    const swarm_module_interface_t* interface,
    uint32_t priority,
    uint32_t* module_id_out
);

/**
 * @brief Unregister swarm module
 *
 * WHAT: Remove module from registry
 * WHY:  Dynamic system reconfiguration
 * HOW:  Find and remove module entry
 *
 * NOTE: Does NOT call destroy_fn (caller responsibility)
 *
 * @param registry Registry
 * @param module_id Module ID to unregister
 * @return 0 on success, -1 if not found
 */
int swarm_registry_unregister_module(
    swarm_module_registry_t* registry,
    uint32_t module_id
);

/**
 * @brief Enable/disable specific module
 *
 * WHAT: Toggle module update participation
 * WHY:  Selective activation without unregistering
 * HOW:  Set module enabled flag, call enable_fn if provided
 *
 * @param registry Registry
 * @param module_id Module ID
 * @param enabled Enable (true) or disable (false)
 * @return 0 on success, -1 if not found
 */
int swarm_registry_set_module_enabled(
    swarm_module_registry_t* registry,
    uint32_t module_id,
    bool enabled
);

/**
 * @brief Set module priority
 *
 * WHAT: Change module priority for arbitration
 * WHY:  Dynamic priority adjustment based on context
 * HOW:  Update priority field in module entry
 *
 * @param registry Registry
 * @param module_id Module ID
 * @param priority New priority (0-10)
 * @return 0 on success, -1 if not found
 */
int swarm_registry_set_module_priority(
    swarm_module_registry_t* registry,
    uint32_t module_id,
    uint32_t priority
);

/**
 * @brief Get module by ID
 *
 * @param registry Registry
 * @param module_id Module ID
 * @return Module entry or NULL if not found
 */
const swarm_module_entry_t* swarm_registry_get_module(
    const swarm_module_registry_t* registry,
    uint32_t module_id
);

/**
 * @brief Find module by name
 *
 * @param registry Registry
 * @param name Module name
 * @return Module entry or NULL if not found
 */
const swarm_module_entry_t* swarm_registry_find_module_by_name(
    const swarm_module_registry_t* registry,
    const char* name
);

/**
 * @brief Get all modules in category
 *
 * @param registry Registry
 * @param category Category
 * @param modules Output array (caller allocated)
 * @param max_modules Array capacity
 * @return Number of modules written
 */
uint32_t swarm_registry_get_modules_by_category(
    const swarm_module_registry_t* registry,
    swarm_module_category_t category,
    const swarm_module_entry_t** modules,
    uint32_t max_modules
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main registry update
 *
 * WHAT: Update all enabled modules according to their category intervals
 * WHY:  Core of registry - coordinates distributed behaviors
 * HOW:  Iterate enabled modules, call update_fn, track statistics
 *
 * NOTE: Call this regularly from main loop
 *
 * @param registry Registry
 * @param current_time_ms Current time (milliseconds)
 * @return Number of modules updated, -1 on error
 */
int swarm_registry_update(
    swarm_module_registry_t* registry,
    uint64_t current_time_ms
);

/**
 * @brief Update specific category
 *
 * WHAT: Update all modules in one category
 * WHY:  Fine-grained control over update timing
 * HOW:  Iterate enabled modules in category, call update_fn
 *
 * @param registry Registry
 * @param category Category to update
 * @param current_time_ms Current time (milliseconds)
 * @return Number of modules updated, -1 on error
 */
int swarm_registry_update_category(
    swarm_module_registry_t* registry,
    swarm_module_category_t category,
    uint64_t current_time_ms
);

/**
 * @brief Update specific module
 *
 * WHAT: Update single module by ID
 * WHY:  Manual override for specific module
 * HOW:  Find module, call update_fn
 *
 * @param registry Registry
 * @param module_id Module ID
 * @param delta_time_ms Time since last update
 * @return 0 on success, -1 on error
 */
int swarm_registry_update_module(
    swarm_module_registry_t* registry,
    uint32_t module_id,
    uint64_t delta_time_ms
);

/* ============================================================================
 * Arbitration API
 * ============================================================================ */

/**
 * @brief Resolve behavioral conflict
 *
 * WHAT: Select winner from competing modules
 * WHY:  Prevent contradictory behaviors
 * HOW:  Apply arbitration strategy based on priorities
 *
 * @param registry Registry
 * @param module_ids Array of conflicting module IDs
 * @param module_count Number of conflicting modules
 * @param winner_id_out Output: winning module ID
 * @return 0 on success, -1 on error
 */
int swarm_registry_resolve_conflict(
    swarm_module_registry_t* registry,
    const uint32_t* module_ids,
    uint32_t module_count,
    uint32_t* winner_id_out
);

/**
 * @brief Set arbitration strategy
 *
 * @param registry Registry
 * @param strategy Arbitration strategy
 * @return 0 on success, -1 on error
 */
int swarm_registry_set_arbitration_strategy(
    swarm_module_registry_t* registry,
    swarm_arbitration_strategy_t strategy
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to swarm brain
 *
 * WHAT: Wire registry to swarm_brain coordinator
 * WHY:  Enable automatic module lifecycle coordination
 * HOW:  Store handle, optionally auto-wire registered modules
 *
 * @param registry Registry
 * @param swarm_brain Swarm brain coordinator
 * @return 0 on success, -1 on error
 */
int swarm_registry_connect_swarm_brain(
    swarm_module_registry_t* registry,
    swarm_brain_t* swarm_brain
);

/**
 * @brief Disconnect from swarm brain
 *
 * @param registry Registry
 * @return 0 on success
 */
int swarm_registry_disconnect_swarm_brain(
    swarm_module_registry_t* registry
);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register registry with bio-async
 * WHY:  Enable inter-module messaging and discovery
 * HOW:  Register module, set up handlers
 *
 * @param registry Registry
 * @return 0 on success, -1 on error
 */
int swarm_registry_connect_bio_async(swarm_module_registry_t* registry);

/**
 * @brief Disconnect from bio-async
 *
 * @param registry Registry
 * @return 0 on success
 */
int swarm_registry_disconnect_bio_async(swarm_module_registry_t* registry);

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Integrate registry with brain immune
 * WHY:  Enable immune modulation of swarm behaviors
 * HOW:  Store handle, register for cytokine callbacks
 *
 * @param registry Registry
 * @param immune Brain immune system
 * @return 0 on success, -1 on error
 */
int swarm_registry_connect_brain_immune(
    swarm_module_registry_t* registry,
    brain_immune_system_t* immune
);

/**
 * @brief Disconnect from brain immune
 *
 * @param registry Registry
 * @return 0 on success
 */
int swarm_registry_disconnect_brain_immune(
    swarm_module_registry_t* registry
);

/* ============================================================================
 * Internal Knowledge Graph Integration API
 * ============================================================================ */

/**
 * @brief Connect to internal brain Knowledge Graph
 *
 * WHAT: Initialize KG integration for swarm module topology awareness
 * WHY:  Enable KG-based discovery, conflict resolution, and dynamic node creation
 * HOW:  Get KG from brain, find our node, create nodes for registered modules
 *
 * @param registry Registry
 * @param brain Brain instance containing internal KG
 * @return 0 on success, -1 on error
 *
 * @note Call after brain KG is populated
 * @note Safe to call if KG is disabled (no-op)
 */
int swarm_registry_connect_internal_kg(
    swarm_module_registry_t* registry,
    brain_t brain
);

/**
 * @brief Disconnect from internal KG
 *
 * WHAT: Clean up KG integration
 * WHY:  Proper cleanup before shutdown
 * HOW:  Clear cached references
 *
 * @param registry Registry
 * @return 0 on success
 */
int swarm_registry_disconnect_internal_kg(swarm_module_registry_t* registry);

/**
 * @brief Create KG node for newly registered module
 *
 * WHAT: Create SWARM node in KG for dynamically registered module
 * WHY:  Modules registered after KG init need nodes too
 * HOW:  Create node with BRAIN_KG_NODE_SWARM type, update module's kg_node_id
 *
 * @param registry Registry
 * @param module_id Module ID
 * @return 0 on success, -1 on error
 *
 * @note No-op if KG is not connected or module already has node
 */
int swarm_registry_create_module_kg_node(
    swarm_module_registry_t* registry,
    uint32_t module_id
);

/**
 * @brief Resolve conflict using KG topology awareness
 *
 * WHAT: Enhanced conflict resolution using KG information
 * WHY:  KG may have additional context about module relationships
 * HOW:  Query KG for module priorities, dependencies, prefer central nodes
 *
 * @param registry Registry
 * @param module_ids Array of conflicting module IDs
 * @param module_count Number of conflicting modules
 * @param winner_id_out Output: winning module ID
 * @return 0 on success, -1 on error
 *
 * @note Falls back to standard arbitration if KG not connected
 */
int swarm_registry_resolve_conflict_topology_aware(
    swarm_module_registry_t* registry,
    const uint32_t* module_ids,
    uint32_t module_count,
    uint32_t* winner_id_out
);

/**
 * @brief Get module's KG node ID
 *
 * @param registry Registry
 * @param module_id Module ID
 * @return KG node ID or BRAIN_KG_INVALID_NODE if not found
 */
brain_kg_node_id_t swarm_registry_get_module_kg_node(
    const swarm_module_registry_t* registry,
    uint32_t module_id
);

/* ============================================================================
 * Discovery API
 * ============================================================================ */

/**
 * @brief Enumerate all registered modules
 *
 * WHAT: Get list of all registered modules
 * WHY:  Module discovery and introspection
 * HOW:  Copy module entries to output array
 *
 * @param registry Registry
 * @param modules Output array (caller allocated)
 * @param max_modules Array capacity
 * @return Number of modules written
 */
uint32_t swarm_registry_enumerate_modules(
    const swarm_module_registry_t* registry,
    const swarm_module_entry_t** modules,
    uint32_t max_modules
);

/**
 * @brief Get module count by category
 *
 * @param registry Registry
 * @param category Category
 * @return Number of modules in category
 */
uint32_t swarm_registry_get_category_count(
    const swarm_module_registry_t* registry,
    swarm_module_category_t category
);

/**
 * @brief Check if module is registered
 *
 * @param registry Registry
 * @param module_id Module ID
 * @return true if registered, false otherwise
 */
bool swarm_registry_is_module_registered(
    const swarm_module_registry_t* registry,
    uint32_t module_id
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get registry statistics
 *
 * @param registry Registry
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int swarm_registry_get_stats(
    const swarm_module_registry_t* registry,
    swarm_registry_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param registry Registry
 */
void swarm_registry_reset_stats(swarm_module_registry_t* registry);

/**
 * @brief Get module statistics
 *
 * @param registry Registry
 * @param module_id Module ID
 * @param update_count Output: update count
 * @param avg_time_us Output: average update time (us)
 * @param error_count Output: error count
 * @return 0 on success, -1 if not found
 */
int swarm_registry_get_module_stats(
    const swarm_module_registry_t* registry,
    uint32_t module_id,
    uint64_t* update_count,
    float* avg_time_us,
    uint32_t* error_count
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
const char* swarm_module_category_to_string(swarm_module_category_t category);

/**
 * @brief Convert state to string
 *
 * @param state State
 * @return Human-readable string
 */
const char* swarm_module_state_to_string(swarm_module_state_t state);

/**
 * @brief Convert arbitration strategy to string
 *
 * @param strategy Strategy
 * @return Human-readable string
 */
const char* swarm_arbitration_strategy_to_string(
    swarm_arbitration_strategy_t strategy
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_MODULE_REGISTRY_H */
