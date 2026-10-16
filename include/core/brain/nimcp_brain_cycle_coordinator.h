/**
 * @file nimcp_brain_cycle_coordinator.h
 * @brief Brain Cycle Coordinator - Unified observability across all brain cycle types
 *
 * WHAT: Lightweight coordinator for monitoring and coordinating all brain cycle types
 * WHY:  Provides unified timing reference, centralized health tracking, cross-cycle
 *       dependency management, and a single diagnostic entry point
 * HOW:  Registry of cycle entries with health monitoring, dependency graph, callback
 *       system, and integration points for immune, bio-async, KG, and other subsystems
 *
 * BIOLOGICAL BASIS:
 * Models the suprachiasmatic nucleus (SCN) and brainstem reticular formation which
 * coordinate multiple biological rhythms at different timescales:
 * - Fast (ms): Neural oscillations, immune signaling, synaptic updates
 * - Medium (sec): Health monitoring, metabolic regulation
 * - Slow (hours): Circadian rhythm, sleep-wake cycling, arousal modulation
 * - Background (adaptive): Garbage collection, I/O dispatching
 *
 * DESIGN PATTERNS:
 * - Registry: Fixed-size array indexed by cycle type
 * - Observer: Callback system for health change notifications
 * - Coordinator: Dependency graph for cross-cycle validation
 * - Strategy: Per-cycle health functions for custom health assessment
 *
 * THREAD SAFETY:
 * All public functions are thread-safe via internal mutex.
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_BRAIN_CYCLE_COORDINATOR_H
#define NIMCP_BRAIN_CYCLE_COORDINATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations (avoid circular includes)
//=============================================================================

#ifndef NIMCP_BRAIN_IMMUNE_SYSTEM_FWD
#define NIMCP_BRAIN_IMMUNE_SYSTEM_FWD
typedef struct brain_immune_system brain_immune_system_t;
#endif

#ifndef NIMCP_BIO_MODULE_CONTEXT_FWD
#define NIMCP_BIO_MODULE_CONTEXT_FWD
typedef struct bio_module_context_struct* bio_module_context_t;
#endif

typedef struct kg_io_dispatcher kg_io_dispatcher_t;

#ifndef NIMCP_INTROSPECTION_CONTEXT_FWD
#define NIMCP_INTROSPECTION_CONTEXT_FWD
typedef struct introspection_context_struct* introspection_context_t;
#endif
typedef struct hemispheric_brain hemispheric_brain_t;
typedef struct oscillations_fep_bridge oscillations_fep_bridge_t;
typedef struct meta_learning_substrate_bridge meta_learning_substrate_bridge_t;
typedef struct sfa_pink_noise_bridge sfa_pink_noise_bridge_t;
typedef struct snn_global_workspace_bridge snn_global_workspace_bridge_t;
typedef struct snn_attention_bridge snn_attention_bridge_t;
typedef struct nimcp_world_model world_model_multimodal_t;

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of callback sets that can be registered */
#define BRAIN_CYCLE_MAX_CALLBACKS       16

/** Maximum dependencies per cycle */
#define BRAIN_CYCLE_MAX_DEPENDENCIES    4

/** Maximum health patterns tracked */
#define BRAIN_CYCLE_MAX_HEALTH_PATTERNS 64

/** Default stall threshold multiplier (N * expected_interval = stall) */
#define BRAIN_CYCLE_DEFAULT_STALL_MULTIPLIER 3

/** Default health check interval in milliseconds */
#define BRAIN_CYCLE_DEFAULT_HEALTH_CHECK_MS  1000

/** Default KG write interval in milliseconds */
#define BRAIN_CYCLE_DEFAULT_KG_WRITE_MS      5000

/*
 * Bio-async message types for coordinator events.
 * See bio_message_type_t enum in nimcp_bio_messages.h (range 0x6F00 - 0x6F0F):
 *   BIO_MSG_CYCLE_HEALTH_CHANGED, BIO_MSG_CYCLE_STALL_DETECTED,
 *   BIO_MSG_CYCLE_DEPENDENCY_VIOLATED, BIO_MSG_CYCLE_COORDINATOR_STATS
 */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Brain cycle type enumeration
 *
 * WHAT: Identifies each distinct brain cycle
 * WHY:  Fixed-size registry indexed by type for O(1) access
 */
typedef enum {
    BRAIN_CYCLE_IMMUNE_TICK = 0,    /**< Immune system tick (50ms) */
    BRAIN_CYCLE_HEALTH_AGENT,       /**< Health agent monitoring (100ms) */
    BRAIN_CYCLE_SLEEP_WAKE,         /**< Sleep-wake state machine */
    BRAIN_CYCLE_CIRCADIAN,          /**< Circadian rhythm (continuous) */
    BRAIN_CYCLE_AROUSAL,            /**< Arousal state (event-driven) */
    BRAIN_CYCLE_OSCILLATIONS,       /**< Neural oscillations (10ms) */
    BRAIN_CYCLE_GC_AGENT,           /**< Garbage collection agent (60s) */
    BRAIN_CYCLE_IO_DISPATCHER,      /**< KG I/O dispatcher (queue-driven) */
    BRAIN_CYCLE_BRAIN_UPDATE,       /**< Main brain update loop (16ms) */
    BRAIN_CYCLE_LONG_TERM_MEMORY,   /**< Phase E6: Z-Ladder consolidation + landmark hygiene (100ms) */
    BRAIN_CYCLE_NEUROGENESIS,       /**< Adult neurogenesis tick — neuron birth/maturation/pruning (1s) */
    BRAIN_CYCLE_EPIGENETICS,        /**< Epigenetic state tick — methylation/histone dynamics (100ms) */
    BRAIN_CYCLE_NEUROVASCULAR,      /**< Neurovascular coupling tick — HRF, CBF, BOLD response (100ms) */
    BRAIN_CYCLE_PREDICTIVE_IMMUNE,  /**< Predictive-immune coupling tick (100ms) */
    BRAIN_CYCLE_CHEMISTRY,          /**< Chemistry: protons/buffers/pH/NO in one tick (10ms) */
    BRAIN_CYCLE_COCHLEA_BRIDGES,    /**< Cochlea + 15 consumer bridges in one tick (10ms) */
    BRAIN_CYCLE_NEUROMOD,           /**< Wave 8B-a: medulla/LC/raphe/VTA/habenula (16ms) */
    BRAIN_CYCLE_SENSORIMOTOR,       /**< Wave 8B-b: motor/olf/gust/soma/amyg/cing (16ms) */
    BRAIN_CYCLE_LANGUAGE,           /**< Wave 8B-d: broca + wernicke bio-msg drain (16ms) */
    BRAIN_CYCLE_PHYSICS_BRIDGES,    /**< Wave 8C: ephaptic/hh/thermo bio_async + ephaptic_fft (16ms) */
    BRAIN_CYCLE_META_LEARNING,      /**< Wave 4: meta_adapt_learning_rate tick (100ms) */
    BRAIN_CYCLE_INTUITIVE_PHYSICS,  /**< Wave 5: intuitive_physics_step tick (16ms) */
    BRAIN_CYCLE_HYPOTHALAMUS,       /**< Wave 8B-c: drives/homeostasis/HPA/autonomic (100ms) */
    BRAIN_CYCLE_ENTORHINAL,         /**< Wave 8B-c: spatial/grid cells / MTL (16ms) — PARTIAL */
    BRAIN_CYCLE_CEREBELLUM,         /**< Wave 8B-c: motor-error forward model + bio-msg drain (16ms) */
    BRAIN_CYCLE_BASAL_GANGLIA,      /**< Wave 8B-c: action selection / reward gating (16ms) */
    BRAIN_CYCLE_TOXICITY,           /**< 2026-05-21: pattern + ML toxicity classifier driver (1s) */
    BRAIN_CYCLE_COUNT               /**< Total number of cycle types */
} brain_cycle_type_t;

/**
 * @brief Cycle frequency category
 *
 * WHAT: Groups cycles by timescale
 * WHY:  Enables category-level health aggregation and timing thresholds
 */
typedef enum {
    BRAIN_CYCLE_CATEGORY_FAST = 0,      /**< Millisecond timescale */
    BRAIN_CYCLE_CATEGORY_MEDIUM,         /**< Second timescale */
    BRAIN_CYCLE_CATEGORY_SLOW,           /**< Minute/hour timescale */
    BRAIN_CYCLE_CATEGORY_BACKGROUND,     /**< Adaptive/queue-driven */
    BRAIN_CYCLE_CATEGORY_COUNT
} brain_cycle_category_t;

/**
 * @brief Cycle health status
 *
 * WHAT: Health state for an individual cycle
 * WHY:  Drives callbacks, immune reporting, and diagnostics
 */
typedef enum {
    BRAIN_CYCLE_HEALTH_UNKNOWN = 0,     /**< Not yet assessed */
    BRAIN_CYCLE_HEALTH_HEALTHY,         /**< Operating normally */
    BRAIN_CYCLE_HEALTH_DEGRADED,        /**< Slower than expected */
    BRAIN_CYCLE_HEALTH_STALLED,         /**< No tick for N * interval */
    BRAIN_CYCLE_HEALTH_ERROR,           /**< Error reported */
    BRAIN_CYCLE_HEALTH_DISABLED         /**< Intentionally disabled */
} brain_cycle_health_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Health query function for a registered cycle
 *
 * @param cycle_handle Opaque handle to the cycle instance
 * @return Current health status of the cycle
 */
typedef brain_cycle_health_t (*brain_cycle_health_fn_t)(void* cycle_handle);

/**
 * @brief Callback invoked when a cycle's health status changes
 */
typedef void (*cycle_health_changed_cb_t)(
    brain_cycle_type_t type,
    brain_cycle_health_t old_health,
    brain_cycle_health_t new_health,
    void* user_data);

/**
 * @brief Callback invoked when a cycle stall is detected
 */
typedef void (*cycle_stall_detected_cb_t)(
    brain_cycle_type_t type,
    uint64_t stall_duration_ms,
    void* user_data);

/**
 * @brief Callback invoked when a dependency ordering is violated
 */
typedef void (*dependency_violated_cb_t)(
    brain_cycle_type_t dependent,
    brain_cycle_type_t dependency,
    void* user_data);

/**
 * @brief Callback invoked when overall coordinator health changes
 */
typedef void (*overall_health_changed_cb_t)(
    float old_health,
    float new_health,
    void* user_data);

/**
 * @brief Aggregated callback registration structure
 *
 * Set non-NULL function pointers for events of interest.
 */
typedef struct {
    cycle_health_changed_cb_t    on_health_changed;
    cycle_stall_detected_cb_t    on_stall_detected;
    dependency_violated_cb_t     on_dependency_violated;
    overall_health_changed_cb_t  on_overall_health_changed;
    void*                        user_data;
} brain_cycle_coordinator_callbacks_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Per-cycle status snapshot
 */
typedef struct {
    brain_cycle_type_t      type;
    const char*             name;
    brain_cycle_category_t  category;
    brain_cycle_health_t    health;
    bool                    enabled;
    bool                    running;
    uint64_t                last_tick_us;
    uint64_t                expected_interval_us;
    uint64_t                ticks_executed;
    uint64_t                errors_encountered;
    double                  avg_duration_us;
    uint64_t                max_duration_us;
    float                   monitoring_weight;
} brain_cycle_status_t;

/**
 * @brief Per-category aggregate statistics
 */
typedef struct {
    uint32_t total_cycles;
    uint32_t healthy_cycles;
    uint32_t degraded_cycles;
    uint32_t stalled_cycles;
    uint64_t total_ticks;
    double   avg_tick_time_us;
} brain_cycle_category_stats_t;

/**
 * @brief Global coordinator statistics
 */
typedef struct {
    uint64_t                       coordinator_uptime_ms;
    uint64_t                       last_health_check_us;
    brain_cycle_category_stats_t   categories[BRAIN_CYCLE_CATEGORY_COUNT];
    uint32_t                       total_cycles_registered;
    uint32_t                       total_cycles_healthy;
    uint32_t                       total_cycles_degraded;
    uint32_t                       total_cycles_stalled;
    uint64_t                       timing_anomalies_detected;
    uint64_t                       dependency_violations;
    float                          overall_health;
} brain_cycle_coordinator_stats_t;

/**
 * @brief Coordinator configuration with all integration points
 *
 * Use brain_cycle_coordinator_default_config() for safe defaults.
 */
typedef struct {
    /* Timing */
    bool        enable_timing_checks;
    uint32_t    stall_threshold_multiplier;
    uint32_t    health_check_interval_ms;
    bool        enable_auto_health_check;

    /* Dependencies */
    bool        enable_dependency_tracking;

    /* Bio-Async Integration */
    bool                    enable_bio_async;
    bio_module_context_t*   bio_context;

    /* Immune System Integration */
    bool                    enable_immune_reporting;
    brain_immune_system_t*  immune_system;

    /* Knowledge Graph Persistence */
    bool                    enable_kg_persistence;
    kg_io_dispatcher_t*     kg_dispatcher;
    const char*             kg_table_name;
    uint32_t                kg_write_interval_ms;

    /* Introspection Integration */
    bool                    enable_introspection;
    introspection_context_t* introspection_ctx;

    /* Hemispheric Brain Integration */
    bool                    enable_hemispheric_tracking;
    hemispheric_brain_t*    hemispheric_brain;

    /* FEP Monitoring Integration */
    bool                        enable_fep_monitoring;
    oscillations_fep_bridge_t*  fep_bridge;

    /* Meta-Learning Integration */
    bool                              enable_meta_learning;
    meta_learning_substrate_bridge_t* meta_bridge;

    /* Pink Noise Modulation */
    bool                    enable_pink_noise_modulation;
    sfa_pink_noise_bridge_t* pink_noise_bridge;
    float                   noise_health_sensitivity;

    /* Global Workspace Integration */
    bool                          enable_global_workspace;
    snn_global_workspace_bridge_t* gw_bridge;

    /* Attention Modulation */
    bool                       enable_attention_modulation;
    snn_attention_bridge_t*    attention_bridge;

    /* World Model Integration */
    bool                       enable_world_model;
    world_model_multimodal_t*  world_model;

    /* Logging */
    bool        enable_logging;
    bool        enable_debug_logging;
} brain_cycle_coordinator_config_t;

/** @brief Opaque coordinator handle */
typedef struct brain_cycle_coordinator brain_cycle_coordinator_t;

//=============================================================================
// API Functions - Lifecycle
//=============================================================================

/**
 * @brief Initialize config with safe default values
 *
 * @param config Configuration to initialize (must not be NULL)
 */
void brain_cycle_coordinator_default_config(brain_cycle_coordinator_config_t* config);

/**
 * @brief Create a new brain cycle coordinator
 *
 * @param config Configuration (NULL for defaults)
 * @return New coordinator handle, or NULL on failure
 */
brain_cycle_coordinator_t* brain_cycle_coordinator_create(
    const brain_cycle_coordinator_config_t* config);

/**
 * @brief Destroy a brain cycle coordinator and free all resources
 *
 * @param coord Coordinator to destroy (NULL is safe)
 */
void brain_cycle_coordinator_destroy(brain_cycle_coordinator_t* coord);

//=============================================================================
// API Functions - Registration
//=============================================================================

/**
 * @brief Register a brain cycle with the coordinator
 *
 * @param coord        Coordinator handle
 * @param type         Cycle type to register
 * @param cycle_handle Opaque handle passed to health_fn
 * @param health_fn    Health query callback (may be NULL for timing-inferred health)
 * @return 0 on success, -1 on error
 */
int brain_cycle_coordinator_register(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    void* cycle_handle,
    brain_cycle_health_fn_t health_fn);

//=============================================================================
// API Functions - Driven Cycles (coordinator owns the driver thread)
//=============================================================================

/**
 * @brief Tick callback invoked periodically by a coordinator-owned driver thread.
 *
 * Called from a dedicated per-cycle thread (one per driven cycle) at the configured
 * interval. MUST be thread-safe with respect to any state it touches. MUST NOT call
 * back into the coordinator for its own cycle type (would deadlock on the entry's
 * mutex). May return quickly or do long work — slow ticks only delay their own cycle,
 * not others (each driven cycle has its own thread).
 *
 * @param ctx Opaque context passed at register time. Lifetime must exceed
 *            the time between register and unregister.
 */
typedef void (*brain_cycle_tick_fn_t)(void* ctx);

/**
 * @brief Register a cycle AND have the coordinator drive it.
 *
 * Unlike the observation-only `brain_cycle_coordinator_register()`, this variant
 * owns a driver thread that calls `tick_fn(tick_ctx)` every `interval_us`
 * microseconds and records the duration automatically via notify_tick. Use this
 * when a cycle has a natural periodic cadence (e.g., chemistry at 10ms,
 * neurogenesis at 1s). For hot-path cycles driven by brain_decide/learn_vector,
 * use `register()` + manual `notify_tick()` instead.
 *
 * Only one registration (driven or observation) is permitted per cycle type.
 * Unregister clears either kind; see `brain_cycle_coordinator_unregister()`.
 *
 * @param coord        Coordinator handle (required)
 * @param type         Cycle type to drive
 * @param interval_us  Desired tick period in microseconds (min 1000 = 1ms)
 * @param tick_fn      Function invoked each period (required, must be thread-safe)
 * @param tick_ctx     Opaque context passed to tick_fn (may be NULL)
 * @param health_fn    Optional health query (NULL → timing-inferred health)
 * @return 0 on success; -1 on invalid args or thread-create failure
 */
int brain_cycle_coordinator_register_driven(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    uint64_t interval_us,
    brain_cycle_tick_fn_t tick_fn,
    void* tick_ctx,
    brain_cycle_health_fn_t health_fn);

/**
 * @brief Unregister a brain cycle from the coordinator
 *
 * For driven cycles (registered via register_driven), this stops the driver
 * thread, waits for any in-flight tick to finish, and joins. Safe to call
 * on either observation-only or driven registrations.
 *
 * @param coord Coordinator handle
 * @param type  Cycle type to unregister
 * @return 0 on success, -1 on error
 */
int brain_cycle_coordinator_unregister(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type);

//=============================================================================
// API Functions - Tick Notification
//=============================================================================

/**
 * @brief Notify the coordinator that a cycle has completed a tick
 *
 * @param coord       Coordinator handle
 * @param type        Cycle that ticked
 * @param duration_us Duration of the tick in microseconds
 * @return 0 on success, -1 on error
 */
int brain_cycle_coordinator_notify_tick(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    uint64_t duration_us);

//=============================================================================
// API Functions - Health Check
//=============================================================================

/**
 * @brief Run a health check across all registered cycles
 *
 * @param coord Coordinator handle
 * @return Number of issues detected, or -1 on error
 */
int brain_cycle_coordinator_check_health(brain_cycle_coordinator_t* coord);

//=============================================================================
// API Functions - Query
//=============================================================================

/**
 * @brief Get status of a specific cycle
 *
 * @param coord  Coordinator handle
 * @param type   Cycle type to query
 * @param status Output status struct
 * @return 0 on success, -1 on error
 */
int brain_cycle_coordinator_get_status(
    const brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    brain_cycle_status_t* status);

/**
 * @brief Get status of all registered cycles
 *
 * @param coord    Coordinator handle
 * @param statuses Output array (must hold BRAIN_CYCLE_COUNT entries)
 * @param count    Output: number of filled entries
 * @return 0 on success, -1 on error
 */
int brain_cycle_coordinator_get_all_status(
    const brain_cycle_coordinator_t* coord,
    brain_cycle_status_t* statuses,
    uint32_t* count);

/**
 * @brief Get global coordinator statistics
 *
 * @param coord Coordinator handle
 * @param stats Output stats struct
 * @return 0 on success, -1 on error
 */
int brain_cycle_coordinator_get_stats(
    const brain_cycle_coordinator_t* coord,
    brain_cycle_coordinator_stats_t* stats);

//=============================================================================
// API Functions - Diagnostics
//=============================================================================

/**
 * @brief Generate a human-readable diagnostic report
 *
 * @param coord       Coordinator handle
 * @param buffer      Output buffer for diagnostic text
 * @param buffer_size Size of the output buffer in bytes
 * @return Number of issues found, or -1 on error
 */
int brain_cycle_coordinator_diagnose(
    const brain_cycle_coordinator_t* coord,
    char* buffer,
    size_t buffer_size);

/**
 * @brief Log current coordinator state to the logging subsystem
 *
 * @param coord Coordinator handle (NULL is safe)
 */
void brain_cycle_coordinator_log_state(const brain_cycle_coordinator_t* coord);

//=============================================================================
// API Functions - Dependency Management
//=============================================================================

/**
 * @brief Add a dependency between two cycles
 *
 * @param coord      Coordinator handle
 * @param dependent  The cycle that depends on another
 * @param dependency The cycle that must be healthy
 * @return 0 on success, -1 on error
 */
int brain_cycle_coordinator_add_dependency(
    brain_cycle_coordinator_t* coord,
    brain_cycle_type_t dependent,
    brain_cycle_type_t dependency);

/**
 * @brief Check whether all dependencies for a cycle are satisfied
 *
 * @param coord     Coordinator handle
 * @param type      Cycle to check dependencies for
 * @param satisfied Output: true if all dependencies met
 * @return 0 on success, -1 on error
 */
int brain_cycle_coordinator_check_dependencies(
    const brain_cycle_coordinator_t* coord,
    brain_cycle_type_t type,
    bool* satisfied);

//=============================================================================
// API Functions - Callback Registration
//=============================================================================

/**
 * @brief Register a set of event callbacks
 *
 * @param coord     Coordinator handle
 * @param callbacks Callback struct
 * @return 0 on success, -1 on error
 */
int brain_cycle_coordinator_register_callbacks(
    brain_cycle_coordinator_t* coord,
    const brain_cycle_coordinator_callbacks_t* callbacks);

/**
 * @brief Unregister callbacks by user_data pointer
 *
 * @param coord     Coordinator handle
 * @param user_data The user_data pointer used during registration
 * @return 0 on success, -1 if not found
 */
int brain_cycle_coordinator_unregister_callbacks(
    brain_cycle_coordinator_t* coord,
    void* user_data);

//=============================================================================
// API Functions - Integration Connection
//=============================================================================

int brain_cycle_coordinator_connect_bio_async(
    brain_cycle_coordinator_t* coord,
    bio_module_context_t* bio_ctx);

int brain_cycle_coordinator_connect_immune(
    brain_cycle_coordinator_t* coord,
    brain_immune_system_t* immune);

int brain_cycle_coordinator_connect_kg(
    brain_cycle_coordinator_t* coord,
    kg_io_dispatcher_t* kg_dispatcher);

int brain_cycle_coordinator_connect_introspection(
    brain_cycle_coordinator_t* coord,
    introspection_context_t* intro_ctx);

int brain_cycle_coordinator_connect_hemispheric(
    brain_cycle_coordinator_t* coord,
    hemispheric_brain_t* hemi_brain);

int brain_cycle_coordinator_connect_fep(
    brain_cycle_coordinator_t* coord,
    oscillations_fep_bridge_t* fep_bridge);

int brain_cycle_coordinator_connect_meta_learning(
    brain_cycle_coordinator_t* coord,
    meta_learning_substrate_bridge_t* meta_bridge);

int brain_cycle_coordinator_connect_pink_noise(
    brain_cycle_coordinator_t* coord,
    sfa_pink_noise_bridge_t* pink_bridge);

int brain_cycle_coordinator_connect_global_workspace(
    brain_cycle_coordinator_t* coord,
    snn_global_workspace_bridge_t* gw_bridge);

int brain_cycle_coordinator_connect_attention(
    brain_cycle_coordinator_t* coord,
    snn_attention_bridge_t* attention_bridge);

int brain_cycle_coordinator_connect_world_model(
    brain_cycle_coordinator_t* coord,
    world_model_multimodal_t* world_model);

//=============================================================================
// API Functions - Persistence
//=============================================================================

/**
 * @brief Flush current coordinator state to the knowledge graph
 *
 * @param coord Coordinator handle
 * @return 0 on success, -1 on error
 */
int brain_cycle_coordinator_flush_to_kg(brain_cycle_coordinator_t* coord);

//=============================================================================
// API Functions - Utility
//=============================================================================

const char* brain_cycle_type_name(brain_cycle_type_t type);
const char* brain_cycle_health_name(brain_cycle_health_t health);
const char* brain_cycle_category_name(brain_cycle_category_t category);
brain_cycle_category_t brain_cycle_get_category(brain_cycle_type_t type);
uint64_t brain_cycle_get_default_interval_us(brain_cycle_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_CYCLE_COORDINATOR_H */
