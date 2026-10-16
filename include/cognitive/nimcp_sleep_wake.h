/**
 * @file nimcp_sleep_wake.h
 * @brief Sleep-wake cycle for memory consolidation and synaptic homeostasis
 *
 * WHAT: Biologically-inspired sleep-wake cycle system
 * WHY:  Prevent catastrophic forgetting, improve generalization, maintain synaptic health
 * HOW:  Multi-stage sleep states with memory replay and synaptic scaling
 *
 * DESIGN PATTERNS:
 * - State Machine: Sleep state transitions (awake → drowsy → light → deep → REM → awake)
 * - Strategy: Different consolidation strategies per sleep stage
 * - Observer: Notify on state changes
 *
 * PHASE 10.1: Core sleep/wake cycle
 * PHASE 10.3 INTEGRATION: Emotional working memory prioritization during replay
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SLEEP_WAKE_H
#define NIMCP_SLEEP_WAKE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Sleep-wake states
 * WHY:  Maps to brain oscillation frequencies and functional modes
 * HOW:  Each state has specific consolidation and homeostasis behaviors
 */
typedef enum {
    SLEEP_STATE_AWAKE,       /**< Beta/Gamma: 13-100Hz - active processing */
    SLEEP_STATE_DROWSY,      /**< Alpha: 8-13Hz - relaxed, transition */
    SLEEP_STATE_LIGHT_NREM,  /**< NREM 1-2: Theta 4-8Hz - sorting */
    SLEEP_STATE_DEEP_NREM,   /**< NREM 3: Delta 0.5-4Hz - consolidation */
    SLEEP_STATE_REM          /**< REM: Theta + atonia - creativity */
} sleep_state_t;

/**
 * WHAT: Sleep system configuration parameters
 * WHY:  Customize sleep behavior for different learning scenarios
 * HOW:  Control pressure accumulation, stage durations, replay strategy
 */
typedef struct {
    // Sleep pressure dynamics
    float adenosine_accumulation_rate;  /**< Per learning step [0.0001] */
    float sleep_pressure_threshold;     /**< When to sleep [0.8] */
    float adenosine_clearance_rate;     /**< During sleep [0.05/min] */

    // Stage durations (milliseconds)
    uint32_t drowsy_duration_ms;        /**< Default: 120,000 (2 min) */
    uint32_t light_sleep_duration_ms;   /**< Default: 900,000 (15 min) */
    uint32_t deep_sleep_duration_ms;    /**< Default: 1,800,000 (30 min) */
    uint32_t rem_duration_ms;           /**< Default: 600,000 (10 min) */

    // Memory replay parameters
    uint32_t replay_batch_size;         /**< Memories per replay [100] */
    float replay_speed_multiplier;      /**< 10-20x faster than awake [15.0] */
    float replay_noise;                 /**< Variability in replay [0.1] */
    bool prioritize_emotional;          /**< Replay emotional memories first */
    bool prioritize_novel;              /**< Replay novel memories first */

    // Synaptic homeostasis
    float synaptic_downscaling_factor;  /**< Multiply all weights [0.85] */
    float synaptic_pruning_threshold;   /**< Remove if w < threshold [0.01] */
    bool enable_homeostasis;            /**< Enable weight downscaling */

    // REM parameters
    float rem_creativity_noise;         /**< Random activation [0.3] */
    bool enable_rem;                    /**< Enable REM stage */

    // Oscillation control
    bool sync_to_oscillations;          /**< Match brain oscillation frequencies */
} sleep_config_t;

/**
 * WHAT: Synaptic homeostasis statistics
 * WHY:  Track effects of synaptic downscaling and pruning
 * HOW:  Record before/after weights and synapse counts
 */
typedef struct {
    float total_weight_before;  /**< Total synaptic weight before scaling */
    float total_weight_after;   /**< Total synaptic weight after scaling */
    uint32_t synapses_pruned;   /**< Number of synapses removed */
    float energy_saved_percent; /**< Estimated energy savings */
} homeostasis_stats_t;

/**
 * WHAT: Sleep system statistics
 * WHY:  Monitor sleep quality and efficiency
 * HOW:  Track durations, cycles, consolidation metrics
 */
typedef struct {
    uint64_t total_awake_time_ms;        /**< Total time awake */
    uint64_t total_sleep_time_ms;        /**< Total time asleep */
    uint32_t sleep_cycles_completed;     /**< Number of full cycles */
    uint32_t total_memories_replayed;    /**< Memories consolidated */
    uint32_t total_synapses_pruned;      /**< Synapses removed */
    float avg_consolidation_efficiency;  /**< Memory retention [0-1] */
    float energy_savings_percent;        /**< Energy saved from pruning */
    float current_sleep_pressure;        /**< Current pressure [0-1] */
} sleep_stats_t;

/**
 * WHAT: Opaque handle to sleep system
 * WHY:  Encapsulation - hide implementation details
 * HOW:  Pimpl idiom - pointer to internal structure
 */
typedef struct sleep_system_struct* sleep_system_t;

/**
 * WHAT: Callback function type for sleep state change notifications
 * WHY:  Allow modules to react immediately to state changes
 * HOW:  Callback receives new state and user data
 *
 * @param new_state The sleep state being entered
 * @param user_data User-provided context pointer
 */
typedef void (*sleep_state_callback_t)(sleep_state_t new_state, void* user_data);

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Create sleep-wake system
 * WHY:  Initialize sleep tracking and configuration
 * HOW:  Allocate structures, set defaults
 *
 * @param config Sleep configuration (NULL for defaults)
 * @return Sleep system handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
sleep_system_t sleep_system_create(const sleep_config_t* config);

/**
 * WHAT: Destroy sleep system
 * WHY:  Free resources and prevent memory leaks
 * HOW:  Free allocations, zero structure
 *
 * @param sleep Sleep system to destroy
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (caller ensures no concurrent access)
 */
void sleep_system_destroy(sleep_system_t sleep);

/**
 * WHAT: Get default sleep configuration
 * WHY:  Sensible defaults for typical use cases
 * HOW:  Return pre-configured struct
 *
 * DEFAULTS:
 * - Sleep threshold: 0.8 (80% pressure)
 * - Deep sleep: 30 minutes
 * - Replay speed: 15x
 * - Downscaling: 85% of original weights
 * - Pruning: Remove weights < 0.01
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
sleep_config_t sleep_default_config(void);

/* ========================================================================
 * SLEEP PRESSURE MANAGEMENT
 * ======================================================================== */

/**
 * WHAT: Update sleep pressure after learning
 * WHY:  Model metabolic cost of learning (adenosine accumulation)
 * HOW:  Increment pressure by learning_steps * accumulation_rate
 *
 * BIOLOGICAL BASIS:
 * - Learning increases synaptic activity
 * - Synaptic activity produces adenosine
 * - Adenosine accumulates → sleep pressure increases
 *
 * @param sleep Sleep system
 * @param learning_steps Number of learning steps performed
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void sleep_accumulate_pressure(sleep_system_t sleep, uint32_t learning_steps);

/**
 * WHAT: Get current sleep pressure
 * WHY:  Check how much the brain needs sleep
 * HOW:  Return current pressure level
 *
 * @param sleep Sleep system
 * @return Sleep pressure [0,1], where 1.0 = desperate for sleep
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float sleep_get_pressure(const sleep_system_t sleep);

/**
 * WHAT: Check if sleep is needed
 * WHY:  Determine when to initiate sleep cycle
 * HOW:  Compare pressure to threshold
 *
 * @param sleep Sleep system
 * @return true if pressure exceeds threshold
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool sleep_is_needed(const sleep_system_t sleep);

/* ========================================================================
 * SLEEP CYCLE CONTROL
 * ======================================================================== */

/**
 * WHAT: Get current sleep state
 * WHY:  Determine what the brain is doing
 * HOW:  Return current state enum
 *
 * @param sleep Sleep system
 * @return Current sleep state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
sleep_state_t sleep_get_current_state(const sleep_system_t sleep);

/**
 * WHAT: Enter specific sleep state
 * WHY:  Manual control of sleep stages (for testing)
 * HOW:  Set state, record timestamp, configure oscillations
 *
 * @param sleep Sleep system
 * @param state Desired sleep state
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool sleep_enter_state(sleep_system_t sleep, sleep_state_t state);

/**
 * WHAT: Run automatic full sleep cycle
 * WHY:  Automate entire consolidation process
 * HOW:  Progress through states: drowsy → light → deep → REM → awake
 *
 * PIPELINE:
 * 1. Drowsy (2 min): Reduce oscillation frequency
 * 2. Light NREM (15 min): Sort memories by importance
 * 3. Deep NREM (30 min): Replay memories, downscale weights
 * 4. REM (10 min): Creative recombination
 * 5. Wake: Reset pressure, return to awake state
 *
 * PHASE 10.3 INTEGRATION:
 * - Uses emotional working memory for replay prioritization
 * - Emotional memories get higher consolidation priority
 *
 * @param sleep Sleep system
 * @param num_cycles Number of cycles to perform (default: 1)
 * @return true on success
 *
 * COMPLEXITY: O(n*m) where n=memories, m=replay_batch_size
 * THREAD-SAFE: Yes
 */
bool sleep_run_cycle(sleep_system_t sleep, uint32_t num_cycles);

/**
 * WHAT: Wake brain from sleep
 * WHY:  Return to active processing mode
 * HOW:  Set state to awake, reset pressure
 *
 * @param sleep Sleep system
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool sleep_wake_up(sleep_system_t sleep);

/* ========================================================================
 * STATISTICS AND MONITORING
 * ======================================================================== */

/**
 * WHAT: Get sleep statistics
 * WHY:  Monitor sleep quality and efficiency
 * HOW:  Copy statistics structure
 *
 * @param sleep Sleep system
 * @param stats Output: statistics structure
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool sleep_get_statistics(const sleep_system_t sleep, sleep_stats_t* stats);

/**
 * WHAT: Reset sleep statistics
 * WHY:  Clear counters for new measurement period
 * HOW:  Zero all statistics except current state
 *
 * @param sleep Sleep system
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void sleep_reset_statistics(sleep_system_t sleep);

/**
 * WHAT: Set brain reference for working memory access
 * WHY:  Enable deep sleep to access emotional working memory for prioritization
 * HOW:  Store brain pointer (void* to avoid circular dependency)
 *
 * PHASE 10.3 INTEGRATION:
 * - Called by brain_create() during initialization
 * - Enables sleep system to access working memory
 * - Allows emotional prioritization during consolidation
 *
 * NOTE: This is an internal function used by the brain module
 *
 * @param sleep Sleep system
 * @param brain Brain handle (void* cast to avoid circular includes)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void sleep_set_brain_reference(sleep_system_t sleep, void* brain);

/* ========================================================================
 * STATE CHANGE NOTIFICATION
 * ======================================================================== */

/**
 * WHAT: Register callback for sleep state changes
 * WHY:  Allow modules to react immediately when sleep state changes
 * HOW:  Add callback to observer list, called on every state transition
 *
 * USAGE:
 * - Modules register their callback during initialization
 * - Callback receives new state and can update module parameters
 * - Multiple modules can register callbacks independently
 *
 * @param sleep Sleep system
 * @param callback Function to call on state changes
 * @param user_data Context pointer passed to callback
 * @return true on success, false if registration fails
 *
 * COMPLEXITY: O(1) amortized
 * THREAD-SAFE: Yes
 */
bool sleep_register_state_callback(sleep_system_t sleep,
                                    sleep_state_callback_t callback,
                                    void* user_data);

/**
 * WHAT: Unregister sleep state change callback
 * WHY:  Remove callback when module is destroyed
 * HOW:  Find and remove callback from observer list
 *
 * @param sleep Sleep system
 * @param callback Function to unregister
 * @param user_data Context pointer (must match registration)
 * @return true if callback was found and removed
 *
 * COMPLEXITY: O(n) where n = number of callbacks
 * THREAD-SAFE: Yes
 */
bool sleep_unregister_state_callback(sleep_system_t sleep,
                                      sleep_state_callback_t callback,
                                      void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SLEEP_WAKE_H */
