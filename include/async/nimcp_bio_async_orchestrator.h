/**
 * @file nimcp_bio_async_orchestrator.h
 * @brief Bio-Async Orchestrator - Central coordinator for 200+ bio-async modules
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Central orchestrator managing all bio-async modules across NIMCP
 * WHY:  200+ modules need coordinated startup, health monitoring, and discovery
 * HOW:  Registry pattern for module storage, factory for lifecycle, observer for health
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * CENTRAL COORDINATION IN NEURAL SYSTEMS:
 * ----------------------------------------
 * The brain coordinates hundreds of specialized regions without a central "boss":
 * - Distributed modules maintain local autonomy
 * - Global coordination emerges from local interactions
 * - Health monitoring detects and isolates dysfunctional regions
 * - Discovery enables dynamic reconfiguration
 *
 * This orchestrator implements network-level coordination for bio-async modules:
 * - Startup sequencing respects dependencies (like cortical development)
 * - Health monitoring tracks module vitality (like homeostatic regulation)
 * - Discovery service enables dynamic module interaction
 * - Statistics track system-wide communication patterns
 *
 * DEPENDENCY ORDERING:
 * --------------------
 * Neural development follows strict sequences:
 * 1. Core infrastructure (neurons, synapses) develops first
 * 2. Plasticity mechanisms (STDP, homeostatic) come online
 * 3. Cognitive systems (attention, memory) build on lower levels
 * 4. High-level functions (reasoning, introspection) emerge last
 *
 * Orchestrator enforces similar ordering for bio-async modules.
 *
 * HEALTH MONITORING:
 * ------------------
 * Neural systems continuously monitor health:
 * - Glial cells detect neuronal dysfunction
 * - Immune system responds to threats
 * - Homeostatic mechanisms maintain stability
 *
 * Orchestrator provides similar monitoring:
 * - Periodic health checks of all modules
 * - Message throughput tracking
 * - Latency monitoring
 * - Connection status verification
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              BIO-ASYNC ORCHESTRATOR (Central Coordinator)                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                      MODULE REGISTRY                                │  ║
 * ║   │                                                                     │  ║
 * ║   │  Core Modules (100+)      Cognitive (50+)      Immune (40+)        │  ║
 * ║   │  ────────────────────      ─────────────       ─────────           │  ║
 * ║   │  • Brain                   • Attention          • Brain Immune      │  ║
 * ║   │  • Synapse                 • Memory             • Attention-Immune  │  ║
 * ║   │  • Cortical Columns        • Reasoning          • Memory-Immune     │  ║
 * ║   │  • Oscillations            • Executive          • Visual-Immune     │  ║
 * ║   │  • Logic Gates             • Introspection      • STDP-Immune       │  ║
 * ║   │                                                                     │  ║
 * ║   │  Plasticity (20+)          Swarm (12+)          Security (8+)      │  ║
 * ║   │  ────────────────           ──────────          ──────────         │  ║
 * ║   │  • STDP                    • Swarm Brain        • BBB              │  ║
 * ║   │  • BCM                     • Consensus          • Anomaly          │  ║
 * ║   │  • Predictive Coding       • Flocking           • Rate Limiter     │  ║
 * ║   │  • Neuromodulator          • Pheromone          • Pattern DB       │  ║
 * ║   │  • Second Messenger        • Quorum                                │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   STARTUP SEQUENCING                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   Phase 1: Core Infrastructure (Brain, Synapse, Topology)          │  ║
 * ║   │   Phase 2: Plasticity (STDP, BCM, Homeostatic)                     │  ║
 * ║   │   Phase 3: Perception (Visual, Audio, Speech)                      │  ║
 * ║   │   Phase 4: Cognitive (Attention, Memory, Reasoning)                │  ║
 * ║   │   Phase 5: High-level (Introspection, ToM, Self-Model)             │  ║
 * ║   │   Phase 6: Immune Bridges (All immune integration modules)         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   HEALTH MONITORING                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐          │  ║
 * ║   │   │ Module Check │   │Message Counts│   │   Latency    │          │  ║
 * ║   │   │  (Periodic)  │   │   Tracking   │   │  Monitoring  │          │  ║
 * ║   │   │              │   │              │   │              │          │  ║
 * ║   │   │ All modules  │   │ Throughput   │   │ Avg/Max/P99  │          │  ║
 * ║   │   │ respond to   │   │ per module   │   │ per module   │          │  ║
 * ║   │   │ health ping  │   │              │   │              │          │  ║
 * ║   │   └──────────────┘   └──────────────┘   └──────────────┘          │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   DISCOVERY SERVICE                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   • Enumerate all registered modules                               │  ║
 * ║   │   • Query module by ID or category                                 │  ║
 * ║   │   • Check module availability                                      │  ║
 * ║   │   • Get module capabilities                                        │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                           ║
 * ║                                ▼                                           ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   INTEGRATION LAYER                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   Bio-Async Router           Brain Immune System                   │  ║
 * ║   │   ──────────────────         ───────────────────                   │  ║
 * ║   │   Inter-module messaging     Immune-modulated routing              │  ║
 * ║   │   Health check broadcast     Inflammation → Priority boost         │  ║
 * ║   │   Discovery protocol         Cytokines → Module modulation         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Registry: Dynamic module registration/unregistration
 * - Factory: Module lifecycle management (create/destroy)
 * - Observer: Health notifications and event broadcasting
 * - Coordinator: Startup sequencing and dependency ordering
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * REFERENCES:
 * - Sporns, O. (2011) "Networks of the Brain"
 * - Bullmore & Sporns (2009) "Complex brain networks: graph theoretical analysis"
 * - Friston, K. (2010) "The free-energy principle: a unified brain theory?"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BIO_ASYNC_ORCHESTRATOR_H
#define NIMCP_BIO_ASYNC_ORCHESTRATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Integration modules */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_diagram.h"
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

#define BIO_ORCHESTRATOR_MAX_MODULES         256    /**< Max registered modules */
#define BIO_ORCHESTRATOR_MAX_CATEGORIES      16     /**< Max module categories */
#define BIO_ORCHESTRATOR_HEALTH_CHECK_MS     5000   /**< Health check interval */
#define BIO_ORCHESTRATOR_MODULE_NAME         "bio_async_orchestrator"

/* Startup phases */
#define BIO_STARTUP_PHASE_CORE               0      /**< Core infrastructure */
#define BIO_STARTUP_PHASE_PLASTICITY         1      /**< Plasticity mechanisms */
#define BIO_STARTUP_PHASE_PERCEPTION         2      /**< Perception modules */
#define BIO_STARTUP_PHASE_COGNITIVE          3      /**< Cognitive modules */
#define BIO_STARTUP_PHASE_HIGHLEVEL          4      /**< High-level cognition */
#define BIO_STARTUP_PHASE_IMMUNE             5      /**< Immune bridges */
#define BIO_STARTUP_PHASE_COUNT              6      /**< Total phases */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct bio_async_orchestrator bio_async_orchestrator_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Module category types
 *
 * WHAT: Categorization of bio-async modules by functional domain
 * WHY:  Different categories have different startup order and health requirements
 * HOW:  Hierarchical grouping based on biological organization
 */
typedef enum {
    BIO_MODULE_CATEGORY_CORE = 0,          /**< Core infrastructure (brain, synapse, etc.) */
    BIO_MODULE_CATEGORY_PLASTICITY,        /**< Plasticity mechanisms (STDP, BCM, etc.) */
    BIO_MODULE_CATEGORY_PERCEPTION,        /**< Perception (visual, audio, speech) */
    BIO_MODULE_CATEGORY_COGNITIVE,         /**< Cognitive (attention, memory, reasoning) */
    BIO_MODULE_CATEGORY_HIGHLEVEL,         /**< High-level (introspection, ToM, etc.) */
    BIO_MODULE_CATEGORY_IMMUNE,            /**< Immune bridges */
    BIO_MODULE_CATEGORY_SWARM,             /**< Swarm coordination */
    BIO_MODULE_CATEGORY_SECURITY,          /**< Security modules */
    BIO_MODULE_CATEGORY_MIDDLEWARE,        /**< Middleware (training, routing, etc.) */
    BIO_MODULE_CATEGORY_GLIAL,             /**< Glial support */
    BIO_MODULE_CATEGORY_COUNT              /**< Total category count */
} bio_module_category_t;

/**
 * @brief Orchestrator operational state
 */
typedef enum {
    BIO_ORCHESTRATOR_STOPPED = 0,          /**< Not running */
    BIO_ORCHESTRATOR_STARTING,             /**< Initialization in progress */
    BIO_ORCHESTRATOR_RUNNING,              /**< Actively coordinating */
    BIO_ORCHESTRATOR_PAUSED,               /**< Paused */
    BIO_ORCHESTRATOR_STOPPING,             /**< Shutdown in progress */
    BIO_ORCHESTRATOR_ERROR                 /**< Error state */
} bio_orchestrator_state_t;

/**
 * @brief Module health status
 */
typedef enum {
    BIO_MODULE_HEALTH_UNKNOWN = 0,         /**< Not yet checked */
    BIO_MODULE_HEALTH_HEALTHY,             /**< Responding normally */
    BIO_MODULE_HEALTH_DEGRADED,            /**< Slow responses */
    BIO_MODULE_HEALTH_UNHEALTHY,           /**< Not responding */
    BIO_MODULE_HEALTH_FAILED               /**< Failed completely */
} bio_module_health_t;

/* ============================================================================
 * Module Registration Structures
 * ============================================================================ */

/**
 * @brief Registered module entry
 */
typedef struct {
    bio_module_id_t module_id;             /**< Module ID (from nimcp_bio_messages.h) */
    const char* module_name;               /**< Human-readable name */
    bio_module_category_t category;        /**< Module category */
    bio_module_context_t bio_context;      /**< Bio-async context handle */

    /* Health tracking */
    bio_module_health_t health_status;     /**< Current health status */
    uint64_t last_health_check;            /**< Last health check time (ms) */
    uint64_t health_check_count;           /**< Total health checks */
    uint64_t health_failures;              /**< Failed health checks */

    /* Message statistics */
    uint64_t messages_sent;                /**< Messages sent */
    uint64_t messages_received;            /**< Messages received */
    float avg_latency_us;                  /**< Average message latency (microseconds) */
    float max_latency_us;                  /**< Max message latency */

    /* State */
    bool registered;                       /**< Is registered with bio-async */
    bool enabled;                          /**< Is enabled */
    uint64_t registration_time;            /**< When registered (ms) */

    /* Dependencies */
    uint32_t startup_phase;                /**< Startup phase (0-5) */
    bio_module_id_t* dependencies;         /**< Array of dependency module IDs */
    uint32_t dependency_count;             /**< Number of dependencies */

    /* Knowledge Graph integration */
    brain_kg_node_id_t kg_node_id;         /**< KG node ID for this module */

    /* Wiring diagram integration */
    wiring_module_config_t wiring;         /**< Discovered wiring configuration */
    wiring_handler_callback_t handler_callback;  /**< Handler registration callback */
    void* handler_callback_data;           /**< User data for callback */
    bool wiring_discovered;                /**< Wiring has been discovered from KG */
} bio_module_entry_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Per-category configuration
 */
typedef struct {
    bool enabled;                          /**< Enable this category */
    uint64_t health_check_interval_ms;     /**< Health check interval */
    uint32_t max_health_failures;          /**< Max failures before marking unhealthy */
} bio_category_config_t;

/**
 * @brief Bio-async orchestrator configuration
 */
typedef struct {
    /* Category settings */
    bio_category_config_t categories[BIO_MODULE_CATEGORY_COUNT];

    /* Global settings */
    uint32_t max_modules;                  /**< Maximum registered modules */
    bool enable_auto_health_check;         /**< Auto health checks */
    uint64_t global_health_check_ms;       /**< Global health check interval */

    /* Integration enables */
    bool enable_bio_async;                 /**< Enable bio-async integration */
    bool enable_brain_immune;              /**< Enable brain immune integration */
    bool enable_statistics;                /**< Track detailed statistics */
    bool enable_logging;                   /**< Enable logging */

    /* Startup sequencing */
    bool enforce_startup_order;            /**< Enforce dependency ordering */
    uint32_t startup_timeout_ms;           /**< Max time per phase startup */
} bio_orchestrator_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Per-category statistics
 */
typedef struct {
    uint32_t module_count;                 /**< Modules in this category */
    uint32_t healthy_count;                /**< Healthy modules */
    uint32_t degraded_count;               /**< Degraded modules */
    uint32_t unhealthy_count;              /**< Unhealthy modules */
    uint64_t total_messages;               /**< Total messages for category */
    float avg_latency_us;                  /**< Average latency (microseconds) */
} bio_category_stats_t;

/**
 * @brief Orchestrator-wide statistics
 */
typedef struct {
    /* Module counts */
    uint32_t total_modules;                /**< Total registered modules */
    uint32_t active_modules;               /**< Currently enabled modules */
    uint32_t healthy_modules;              /**< Healthy modules */
    bio_category_stats_t categories[BIO_MODULE_CATEGORY_COUNT];

    /* Health monitoring */
    uint64_t total_health_checks;          /**< Total health checks performed */
    uint64_t health_check_failures;        /**< Failed health checks */
    float health_check_success_rate;       /**< Success rate (0-1) */

    /* Message statistics */
    uint64_t total_messages_routed;        /**< Total messages routed */
    float avg_message_latency_us;          /**< Average message latency */
    float max_message_latency_us;          /**< Max message latency */

    /* Discovery statistics */
    uint64_t discovery_queries;            /**< Discovery queries performed */

    /* System health */
    float system_health_score;             /**< Overall health (0-1) */
    uint64_t uptime_ms;                    /**< Orchestrator uptime */
} bio_orchestrator_stats_t;

/* ============================================================================
 * Main Orchestrator Structure
 * ============================================================================ */

/**
 * @brief Bio-async orchestrator state
 */
struct bio_async_orchestrator {
    bio_orchestrator_config_t config;      /**< Configuration */
    bio_orchestrator_state_t state;        /**< Current state */

    /* Module registry */
    bio_module_entry_t* modules;           /**< Registered modules array */
    uint32_t module_count;                 /**< Current module count */
    uint32_t module_capacity;              /**< Array capacity */

    /* Integration handles */
    bio_module_context_t bio_context;      /**< Our bio-async context */
    void* brain_immune;                    /**< Brain immune system (opaque) */

    /* Statistics */
    bio_orchestrator_stats_t stats;

    /* Timing */
    uint64_t start_time;                   /**< Orchestrator start time (ms) */
    uint64_t last_health_check;            /**< Last health check (ms) */

    /* Thread safety */
    nimcp_mutex_t* mutex;                  /**< Thread-safe operations */
    uint32_t state_version;                /**< P2 fix: Version counter for TOCTOU detection */

    /* Runtime state */
    bool bio_async_connected;              /**< Bio-async is connected */
    bool immune_connected;                 /**< Brain immune is connected */
    uint32_t current_startup_phase;        /**< Current startup phase */

    /* Internal Knowledge Graph integration */
    kg_module_context_t kg_context;        /**< KG access context */
    bool kg_connected;                     /**< Internal KG is connected */

    /* Wiring diagram integration */
    wiring_diagram_t* wiring_diagram;      /**< Wiring diagram (not owned) */
    bool wiring_loaded;                    /**< Wiring has been loaded/discovered */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default orchestrator configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible settings
 * HOW:  Set category-specific intervals and enable flags
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int bio_orchestrator_default_config(bio_orchestrator_config_t* config);

/**
 * @brief Create bio-async orchestrator
 *
 * WHAT: Initialize orchestrator infrastructure
 * WHY:  Central coordination point for all bio-async modules
 * HOW:  Allocate module registry, initialize statistics, register with bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return New orchestrator or NULL on failure
 */
bio_async_orchestrator_t* bio_orchestrator_create(const bio_orchestrator_config_t* config);

/**
 * @brief Destroy bio-async orchestrator
 *
 * WHAT: Clean up orchestrator resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister all modules, disconnect integrations, free memory
 *
 * NOTE: Does NOT destroy registered modules (caller responsibility)
 *
 * @param orchestrator Orchestrator to destroy (NULL safe)
 */
void bio_orchestrator_destroy(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Start orchestrator
 *
 * WHAT: Begin coordinated module management
 * WHY:  Activate system-wide bio-async coordination
 * HOW:  Set state to RUNNING, initialize timing, start health monitoring
 *
 * @param orchestrator Orchestrator
 * @return 0 on success, -1 on error
 */
int bio_orchestrator_start(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Stop orchestrator
 *
 * WHAT: Halt coordinated module management
 * WHY:  Graceful shutdown
 * HOW:  Set state to STOPPED, complete pending operations
 *
 * @param orchestrator Orchestrator
 * @return 0 on success, -1 on error
 */
int bio_orchestrator_stop(bio_async_orchestrator_t* orchestrator);

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

/**
 * @brief Register bio-async module
 *
 * WHAT: Add module to orchestrator registry
 * WHY:  Enable coordinated management of all modules
 * HOW:  Store module info, assign category, track dependencies
 *
 * @param orchestrator Orchestrator
 * @param module_id Module ID (from nimcp_bio_messages.h)
 * @param name Module name
 * @param category Module category
 * @param bio_context Bio-async context
 * @param startup_phase Startup phase (0-5)
 * @return 0 on success, -1 on error
 */
int bio_orchestrator_register_module(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    const char* name,
    bio_module_category_t category,
    bio_module_context_t bio_context,
    uint32_t startup_phase
);

/**
 * @brief Unregister bio-async module
 *
 * WHAT: Remove module from orchestrator
 * WHY:  Dynamic system reconfiguration
 * HOW:  Find and remove module entry
 *
 * @param orchestrator Orchestrator
 * @param module_id Module ID to unregister
 * @return 0 on success, -1 if not found
 */
int bio_orchestrator_unregister_module(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
);

/**
 * @brief Add module dependency
 *
 * WHAT: Specify that module A depends on module B
 * WHY:  Enforce startup ordering
 * HOW:  Add dependency to module's dependency list
 *
 * @param orchestrator Orchestrator
 * @param module_id Module that has dependency
 * @param depends_on Module that is depended upon
 * @return 0 on success, -1 on error
 */
int bio_orchestrator_add_dependency(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    bio_module_id_t depends_on
);

/**
 * @brief Enable/disable specific module
 *
 * WHAT: Toggle module participation
 * WHY:  Selective activation without unregistering
 * HOW:  Set module enabled flag
 *
 * @param orchestrator Orchestrator
 * @param module_id Module ID
 * @param enabled Enable (true) or disable (false)
 * @return 0 on success, -1 if not found
 */
int bio_orchestrator_set_module_enabled(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    bool enabled
);

/* ============================================================================
 * Startup Sequencing API
 * ============================================================================ */

/**
 * @brief Execute startup sequence
 *
 * WHAT: Start all modules in dependency order
 * WHY:  Ensure proper initialization sequence
 * HOW:  Phase-by-phase startup respecting dependencies
 *
 * @param orchestrator Orchestrator
 * @return 0 on success, -1 on error
 */
int bio_orchestrator_execute_startup(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Get modules in startup phase
 *
 * WHAT: Query all modules assigned to a startup phase
 * WHY:  Inspect startup configuration
 * HOW:  Filter modules by startup_phase field
 *
 * @param orchestrator Orchestrator
 * @param phase Startup phase (0-5)
 * @param module_ids Output array (caller allocated)
 * @param max_modules Array capacity
 * @return Number of modules in phase
 */
uint32_t bio_orchestrator_get_phase_modules(
    const bio_async_orchestrator_t* orchestrator,
    uint32_t phase,
    bio_module_id_t* module_ids,
    uint32_t max_modules
);

/* ============================================================================
 * Health Monitoring API
 * ============================================================================ */

/**
 * @brief Execute health check on all modules
 *
 * WHAT: Check health of all registered modules
 * WHY:  Detect dysfunctional modules
 * HOW:  Send health ping, wait for response, update status
 *
 * @param orchestrator Orchestrator
 * @return Number of healthy modules, -1 on error
 */
int bio_orchestrator_health_check_all(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Check health of specific module
 *
 * WHAT: Send health ping to single module
 * WHY:  On-demand health verification
 * HOW:  Send BIO_MSG_HEALTH_CHECK, wait for BIO_MSG_HEALTH_RESPONSE
 *
 * @param orchestrator Orchestrator
 * @param module_id Module to check
 * @param health_out Output health status
 * @return 0 on success, -1 on error
 */
int bio_orchestrator_health_check_module(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    bio_module_health_t* health_out
);

/**
 * @brief Get module health status
 *
 * WHAT: Query cached health status
 * WHY:  Fast health lookup without ping
 * HOW:  Return cached health_status field
 *
 * @param orchestrator Orchestrator
 * @param module_id Module ID
 * @return Health status or BIO_MODULE_HEALTH_UNKNOWN if not found
 */
bio_module_health_t bio_orchestrator_get_module_health(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
);

/* ============================================================================
 * Discovery API
 * ============================================================================ */

/**
 * @brief Get all registered modules
 *
 * WHAT: Enumerate all modules
 * WHY:  System introspection
 * HOW:  Copy module IDs to output array
 *
 * @param orchestrator Orchestrator
 * @param module_ids Output array (caller allocated)
 * @param max_modules Array capacity
 * @return Number of modules written
 */
uint32_t bio_orchestrator_get_all_modules(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t* module_ids,
    uint32_t max_modules
);

/**
 * @brief Get modules by category
 *
 * WHAT: Query all modules in a category
 * WHY:  Category-specific operations
 * HOW:  Filter modules by category field
 *
 * @param orchestrator Orchestrator
 * @param category Category
 * @param module_ids Output array (caller allocated)
 * @param max_modules Array capacity
 * @return Number of modules written
 */
uint32_t bio_orchestrator_get_modules_by_category(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_category_t category,
    bio_module_id_t* module_ids,
    uint32_t max_modules
);

/**
 * @brief Get module info
 *
 * WHAT: Get detailed module information
 * WHY:  Inspect module state and statistics
 * HOW:  Return pointer to module entry
 *
 * @param orchestrator Orchestrator
 * @param module_id Module ID
 * @return Module entry or NULL if not found
 */
const bio_module_entry_t* bio_orchestrator_get_module_info(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
);

/**
 * @brief Check if module is registered
 *
 * @param orchestrator Orchestrator
 * @param module_id Module ID
 * @return true if registered, false otherwise
 */
bool bio_orchestrator_is_module_registered(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Integrate orchestrator with brain immune
 * WHY:  Enable immune modulation of bio-async routing
 * HOW:  Store handle, register for cytokine callbacks
 *
 * @param orchestrator Orchestrator
 * @param immune Brain immune system (opaque pointer)
 * @return 0 on success, -1 on error
 */
int bio_orchestrator_connect_brain_immune(
    bio_async_orchestrator_t* orchestrator,
    void* immune
);

/**
 * @brief Disconnect from brain immune
 *
 * @param orchestrator Orchestrator
 * @return 0 on success
 */
int bio_orchestrator_disconnect_brain_immune(bio_async_orchestrator_t* orchestrator);

/* ============================================================================
 * Internal Knowledge Graph Integration API
 * ============================================================================ */

/**
 * @brief Connect to internal brain Knowledge Graph
 *
 * WHAT: Initialize KG integration for topology awareness
 * WHY:  Enable KG queries for module dependencies and health sync
 * HOW:  Get KG from brain, find our node, create nodes for modules
 *
 * @param orchestrator Orchestrator
 * @param brain Brain instance containing internal KG
 * @return 0 on success, -1 on error
 *
 * @note Call after brain KG is populated
 * @note Safe to call if KG is disabled (no-op)
 */
int bio_orchestrator_connect_internal_kg(
    bio_async_orchestrator_t* orchestrator,
    brain_t brain
);

/**
 * @brief Disconnect from internal KG
 *
 * WHAT: Clean up KG integration
 * WHY:  Proper cleanup before shutdown
 * HOW:  Clear cached references
 *
 * @param orchestrator Orchestrator
 * @return 0 on success
 */
int bio_orchestrator_disconnect_internal_kg(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Synchronize module health status to KG
 *
 * WHAT: Update KG node states based on bio-async health
 * WHY:  Keep internal KG state consistent with runtime health
 * HOW:  Map health status to KG node state:
 *       - HEALTHY → BRAIN_KG_STATE_ACTIVE
 *       - DEGRADED → BRAIN_KG_STATE_ACTIVE (with degraded metadata)
 *       - UNHEALTHY/FAILED → BRAIN_KG_STATE_ERROR
 *
 * @param orchestrator Orchestrator
 * @return 0 on success, -1 on error
 *
 * @note Call periodically after health checks
 * @note No-op if KG is not connected
 */
int bio_orchestrator_sync_health_to_kg(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Validate startup ordering using KG topology
 *
 * WHAT: Check if startup sequence respects KG dependencies
 * WHY:  KG may have additional dependency info not in module entries
 * HOW:  Query KG for DEPENDS_ON edges and verify ordering
 *
 * @param orchestrator Orchestrator
 * @return 0 if valid, -1 if invalid dependencies found
 *
 * @note Returns 0 if KG is not connected (graceful degradation)
 */
int bio_orchestrator_validate_startup_ordering(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Get module's KG node ID
 *
 * @param orchestrator Orchestrator
 * @param module_id Module ID
 * @return KG node ID or BRAIN_KG_INVALID_NODE if not found
 */
brain_kg_node_id_t bio_orchestrator_get_module_kg_node(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
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
int bio_orchestrator_get_stats(
    const bio_async_orchestrator_t* orchestrator,
    bio_orchestrator_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param orchestrator Orchestrator
 */
void bio_orchestrator_reset_stats(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Get current system health score
 *
 * WHAT: Compute overall system health
 * WHY:  Single metric for system status
 * HOW:  Weighted average of module health statuses
 *
 * @param orchestrator Orchestrator
 * @return Health score (0-1, 1=perfect health)
 */
float bio_orchestrator_get_health_score(const bio_async_orchestrator_t* orchestrator);

/**
 * @brief Get current state
 *
 * @param orchestrator Orchestrator
 * @return Current state
 */
bio_orchestrator_state_t bio_orchestrator_get_state(
    const bio_async_orchestrator_t* orchestrator
);

/* ============================================================================
 * Wiring Diagram Integration API
 * ============================================================================ */

/**
 * @brief Set wiring diagram for orchestrator
 *
 * WHAT: Associate wiring diagram with orchestrator
 * WHY:  Enable KG-driven module discovery and handler registration
 * HOW:  Store reference, orchestrator uses it during startup
 *
 * @param orchestrator Orchestrator
 * @param wd Wiring diagram (ownership NOT transferred)
 * @return 0 on success, -1 on error
 */
int bio_orchestrator_set_wiring_diagram(
    bio_async_orchestrator_t* orchestrator,
    wiring_diagram_t* wd
);

/**
 * @brief Get wiring diagram from orchestrator
 *
 * @param orchestrator Orchestrator
 * @return Wiring diagram or NULL if not set
 */
wiring_diagram_t* bio_orchestrator_get_wiring_diagram(
    const bio_async_orchestrator_t* orchestrator
);

/**
 * @brief Discover wiring for all registered modules
 *
 * WHAT: Query wiring diagram for each module's configuration
 * WHY:  Populate dependencies and message handlers from KG
 * HOW:  Iterate modules, query wiring_diagram_get_module_config
 *
 * @param orchestrator Orchestrator
 * @return Number of modules with discovered wiring, -1 on error
 */
int bio_orchestrator_discover_all_wiring(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Discover wiring for specific module
 *
 * WHAT: Query wiring for single module
 * WHY:  On-demand discovery for dynamically registered modules
 * HOW:  Query wiring diagram, populate module entry
 *
 * @param orchestrator Orchestrator
 * @param module_id Module to discover wiring for
 * @return 0 on success, -1 if module or wiring not found
 */
int bio_orchestrator_discover_module_wiring(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
);

/**
 * @brief Invoke handler callbacks for all modules
 *
 * WHAT: Call each module's handler callback with discovered message types
 * WHY:  Enable modules to auto-register handlers based on KG wiring
 * HOW:  Iterate modules with callbacks, invoke with message type arrays
 *
 * @param orchestrator Orchestrator
 * @return Number of callbacks invoked, -1 on error
 */
int bio_orchestrator_invoke_handler_callbacks(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Register handler callback for module
 *
 * WHAT: Register callback to be invoked when wiring is discovered
 * WHY:  Enable modules to react to discovered message types
 * HOW:  Store callback in module entry, invoked by discover_all_wiring
 *
 * @param orchestrator Orchestrator
 * @param module_id Module ID
 * @param callback Handler callback function
 * @param user_data User data passed to callback
 * @return 0 on success, -1 on error
 */
int bio_orchestrator_register_handler_callback(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    wiring_handler_callback_t callback,
    void* user_data
);

/**
 * @brief Get module's discovered message handlers
 *
 * WHAT: Query message types a module should handle
 * WHY:  Introspection of discovered wiring
 * HOW:  Return array from module's wiring config
 *
 * @param orchestrator Orchestrator
 * @param module_id Module ID
 * @param message_types Output array (caller allocated)
 * @param max_types Array capacity
 * @return Number of message types, 0 if no wiring discovered
 */
uint32_t bio_orchestrator_get_module_handlers(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    bio_message_type_t* message_types,
    uint32_t max_types
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
const char* bio_module_category_to_string(bio_module_category_t category);

/**
 * @brief Convert state to string
 *
 * @param state State
 * @return Human-readable string
 */
const char* bio_orchestrator_state_to_string(bio_orchestrator_state_t state);

/**
 * @brief Convert health status to string
 *
 * @param health Health status
 * @return Human-readable string
 */
const char* bio_module_health_to_string(bio_module_health_t health);

/* ============================================================================
 * Phase 10: Automatic Self-Assembly API
 * ============================================================================
 *
 * These functions enable KG-driven automatic startup ordering, replacing
 * hardcoded startup_phase values with dynamic dependency resolution from
 * the wiring diagram's topological sort.
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * Neural development is self-organizing - neurons form connections based on
 * molecular signals rather than explicit blueprints. Similarly, these APIs
 * enable modules to self-assemble based on their declared dependencies.
 */

/**
 * @brief Compute startup order from wiring diagram dependencies
 *
 * WHAT: Use KG-driven topological sort to determine module startup sequence
 * WHY:  Replace hardcoded startup_phase values with automatic ordering
 * HOW:  Calls wiring_diagram_get_startup_order() using Kahn's algorithm
 *
 * @param orchestrator Orchestrator with wiring diagram set
 * @param order_out Output array for ordered module IDs (caller allocated)
 * @param max_modules Array capacity
 * @return Number of modules in order, -1 on error (e.g., circular deps)
 *
 * @note Requires wiring diagram to be set via bio_orchestrator_set_wiring_diagram()
 * @note Falls back to hardcoded phases if wiring diagram not available
 */
int bio_orchestrator_compute_startup_order(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t* order_out,
    uint32_t max_modules
);

/**
 * @brief Start modules in KG-computed dependency order
 *
 * WHAT: Execute startup using wiring diagram's topological sort
 * WHY:  Enable true self-assembly without hardcoded phase assignments
 * HOW:  Compute order via Kahn's algorithm, start each module sequentially
 *
 * @param orchestrator Orchestrator
 * @return Number of modules started, -1 on error
 *
 * @note Automatically discovers wiring and invokes handler callbacks
 * @note Falls back to phase-based startup if wiring diagram not set
 */
int bio_orchestrator_start_modules_ordered(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Stop modules in reverse dependency order
 *
 * WHAT: Shutdown modules in reverse topological order
 * WHY:  Ensure dependents stop before their dependencies
 * HOW:  Compute startup order, traverse in reverse
 *
 * @param orchestrator Orchestrator
 * @return Number of modules stopped, -1 on error
 */
int bio_orchestrator_stop_modules_ordered(bio_async_orchestrator_t* orchestrator);

/**
 * @brief Check if self-assembly is available
 *
 * WHAT: Verify wiring diagram is set and valid for self-assembly
 * WHY:  Allow callers to check before using self-assembly APIs
 * HOW:  Check wiring_diagram is set and has modules
 *
 * @param orchestrator Orchestrator
 * @return true if self-assembly is available, false otherwise
 */
bool bio_orchestrator_self_assembly_available(
    const bio_async_orchestrator_t* orchestrator
);

/**
 * @brief Get module's computed startup position
 *
 * WHAT: Query where a module falls in the computed startup order
 * WHY:  Introspection of self-assembly results
 * HOW:  Compute order, find module's position
 *
 * @param orchestrator Orchestrator
 * @param module_id Module to query
 * @return Position in startup order (0-based), -1 if not found
 */
int bio_orchestrator_get_module_startup_position(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
);

/**
 * @brief Validate self-assembly configuration
 *
 * WHAT: Check wiring diagram for circular dependencies and missing deps
 * WHY:  Prevent startup failures due to invalid configurations
 * HOW:  Calls wiring_diagram_validate()
 *
 * @param orchestrator Orchestrator
 * @param result Output validation result (optional)
 * @return 0 if valid, -1 if invalid
 */
int bio_orchestrator_validate_self_assembly(
    bio_async_orchestrator_t* orchestrator,
    wiring_validation_result_t* result
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIO_ASYNC_ORCHESTRATOR_H */
