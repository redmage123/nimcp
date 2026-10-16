/**
 * @file nimcp_chaos_engineering.h
 * @brief Chaos Engineering Integration for Fault Tolerance Testing
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Controlled fault injection for testing system resilience
 * WHY:  Verify fault tolerance works in practice, not just theory
 * HOW:  Inject failures, verify recovery, measure impact
 *
 * BIOLOGICAL BASIS:
 * - Immune system stress testing (vaccination = controlled infection)
 * - Neural plasticity through damage (stroke recovery reveals redundancy)
 * - Adaptation to stressors (hormesis - low-dose stress improves resilience)
 * - Antifragility (systems that benefit from disorder)
 *
 * CHAOS EXPERIMENTS:
 * - Fault injection (memory, network, process, disk)
 * - Latency injection (delays, jitter)
 * - Resource exhaustion (CPU, memory, handles)
 * - State corruption (bit flips, truncation)
 * - Dependency failures (service outages)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CHAOS_ENGINEERING_H
#define NIMCP_CHAOS_ENGINEERING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define CE_MAX_EXPERIMENTS 32               /**< Max concurrent experiments */
#define CE_MAX_TARGETS 64                   /**< Max injection targets */
#define CE_MAX_HYPOTHESIS 8                 /**< Max hypotheses per experiment */
#define CE_MAX_METRICS 16                   /**< Max metrics per experiment */
#define CE_DEFAULT_DURATION_MS 60000        /**< Default experiment duration */
#define CE_BLAST_RADIUS_MAX 100             /**< Maximum blast radius */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Chaos fault types
 */
typedef enum {
    CE_FAULT_NONE = 0,
    CE_FAULT_PROCESS_KILL,      /**< Kill process */
    CE_FAULT_PROCESS_PAUSE,     /**< Pause process (SIGSTOP) */
    CE_FAULT_MEMORY_LEAK,       /**< Induce memory leak */
    CE_FAULT_MEMORY_CORRUPT,    /**< Corrupt memory */
    CE_FAULT_CPU_STRESS,        /**< CPU saturation */
    CE_FAULT_DISK_FULL,         /**< Fill disk */
    CE_FAULT_DISK_SLOW,         /**< Slow disk I/O */
    CE_FAULT_NETWORK_LATENCY,   /**< Network delays */
    CE_FAULT_NETWORK_LOSS,      /**< Packet loss */
    CE_FAULT_NETWORK_PARTITION, /**< Network partition */
    CE_FAULT_NETWORK_CORRUPT,   /**< Corrupt packets */
    CE_FAULT_CLOCK_SKEW,        /**< Time synchronization issues */
    CE_FAULT_DNS_FAILURE,       /**< DNS resolution failure */
    CE_FAULT_SERVICE_OUTAGE,    /**< Service unavailability */
    CE_FAULT_BYZANTINE,         /**< Byzantine behavior */
    CE_FAULT_STATE_CORRUPT,     /**< Corrupt application state */
    CE_FAULT_CHECKPOINT_CORRUPT,/**< Corrupt checkpoints */
    CE_FAULT_CUSTOM             /**< Custom fault injection */
} ce_fault_type_t;

/**
 * @brief Experiment states
 */
typedef enum {
    CE_STATE_CREATED = 0,       /**< Experiment created */
    CE_STATE_READY,             /**< Ready to run */
    CE_STATE_RUNNING,           /**< Currently running */
    CE_STATE_PAUSED,            /**< Paused */
    CE_STATE_COMPLETING,        /**< Finishing up */
    CE_STATE_COMPLETED,         /**< Completed */
    CE_STATE_FAILED,            /**< Failed to run */
    CE_STATE_ABORTED            /**< Manually aborted */
} ce_state_t;

/**
 * @brief Hypothesis result
 */
typedef enum {
    CE_HYPOTHESIS_UNKNOWN = 0,  /**< Not yet evaluated */
    CE_HYPOTHESIS_CONFIRMED,    /**< Hypothesis confirmed */
    CE_HYPOTHESIS_REFUTED,      /**< Hypothesis refuted */
    CE_HYPOTHESIS_INCONCLUSIVE  /**< Insufficient data */
} ce_hypothesis_result_t;

/**
 * @brief Injection patterns
 */
typedef enum {
    CE_PATTERN_ONCE = 0,        /**< Single injection */
    CE_PATTERN_PERIODIC,        /**< Periodic injection */
    CE_PATTERN_RANDOM,          /**< Random timing */
    CE_PATTERN_BURST,           /**< Burst of faults */
    CE_PATTERN_PROGRESSIVE      /**< Gradually increasing */
} ce_pattern_t;

/**
 * @brief Target selection strategies
 */
typedef enum {
    CE_TARGET_SPECIFIC = 0,     /**< Specific node/component */
    CE_TARGET_RANDOM,           /**< Random selection */
    CE_TARGET_PERCENTAGE,       /**< Percentage of nodes */
    CE_TARGET_ROUND_ROBIN       /**< Round-robin through targets */
} ce_target_strategy_t;

/**
 * @brief Safety guardrails
 */
typedef enum {
    CE_GUARDRAIL_NONE = 0,
    CE_GUARDRAIL_BLAST_RADIUS,  /**< Limit affected nodes */
    CE_GUARDRAIL_TIME_LIMIT,    /**< Max experiment duration */
    CE_GUARDRAIL_ERROR_RATE,    /**< Max error rate increase */
    CE_GUARDRAIL_LATENCY,       /**< Max latency increase */
    CE_GUARDRAIL_AVAILABILITY   /**< Min availability */
} ce_guardrail_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Fault injection specification
 */
typedef struct {
    ce_fault_type_t type;       /**< Fault type */
    ce_pattern_t pattern;       /**< Injection pattern */
    float intensity;            /**< Fault intensity (0-100) */
    uint64_t duration_ms;       /**< How long to inject */
    uint64_t interval_ms;       /**< Interval for periodic */
    float probability;          /**< Injection probability (0-1) */
    void* parameters;           /**< Type-specific parameters */
    size_t param_size;          /**< Parameters size */
} ce_fault_spec_t;

/**
 * @brief Target specification
 */
typedef struct {
    char name[64];                  /**< Target name */
    uint32_t target_id;             /**< Target identifier */
    ce_target_strategy_t strategy;  /**< Selection strategy */
    uint32_t node_ids[CE_MAX_TARGETS]; /**< Specific node IDs */
    uint32_t node_count;            /**< Number of nodes */
    float percentage;               /**< For percentage strategy */
} ce_target_spec_t;

/**
 * @brief Experiment hypothesis
 */
typedef struct {
    char description[256];          /**< Hypothesis description */
    char metric_name[64];           /**< Metric to evaluate */
    float expected_min;             /**< Expected minimum value */
    float expected_max;             /**< Expected maximum value */
    ce_hypothesis_result_t result;  /**< Evaluation result */
    float actual_value;             /**< Actual observed value */
} ce_hypothesis_t;

/**
 * @brief Safety guardrail configuration
 */
typedef struct {
    ce_guardrail_t type;        /**< Guardrail type */
    float threshold;            /**< Threshold value */
    bool abort_on_violation;    /**< Abort experiment if violated */
} ce_guardrail_config_t;

/**
 * @brief Experiment metric
 */
typedef struct {
    char name[64];              /**< Metric name */
    double baseline;            /**< Baseline value */
    double during_experiment;   /**< Value during experiment */
    double after_experiment;    /**< Value after experiment */
    double delta;               /**< Change from baseline */
    double delta_percent;       /**< Percentage change */
} ce_metric_t;

/**
 * @brief Chaos experiment definition
 */
typedef struct {
    char name[64];                  /**< Experiment name */
    char description[256];          /**< Description */
    uint32_t experiment_id;         /**< Unique ID */
    ce_state_t state;               /**< Current state */
    ce_fault_spec_t fault;          /**< Fault specification */
    ce_target_spec_t target;        /**< Target specification */
    ce_hypothesis_t hypotheses[CE_MAX_HYPOTHESIS]; /**< Hypotheses */
    uint32_t hypothesis_count;      /**< Number of hypotheses */
    ce_guardrail_config_t guardrails[8]; /**< Safety guardrails */
    uint32_t guardrail_count;       /**< Number of guardrails */
    uint64_t scheduled_at_ms;       /**< Scheduled start time */
    uint64_t started_at_ms;         /**< Actual start time */
    uint64_t ended_at_ms;           /**< End time */
    uint64_t max_duration_ms;       /**< Maximum duration */
    ce_metric_t metrics[CE_MAX_METRICS]; /**< Collected metrics */
    uint32_t metric_count;          /**< Number of metrics */
    bool auto_rollback;             /**< Automatically rollback */
} ce_experiment_t;

/**
 * @brief Experiment result summary
 */
typedef struct {
    uint32_t experiment_id;         /**< Experiment ID */
    ce_state_t final_state;         /**< Final state */
    uint64_t duration_ms;           /**< Total duration */
    uint32_t faults_injected;       /**< Number of faults injected */
    uint32_t recoveries_observed;   /**< Recoveries that occurred */
    uint32_t hypotheses_confirmed;  /**< Confirmed hypotheses */
    uint32_t hypotheses_refuted;    /**< Refuted hypotheses */
    uint32_t guardrail_violations;  /**< Guardrail violations */
    float impact_score;             /**< Overall impact (0-100) */
    bool system_recovered;          /**< System fully recovered */
    char summary[512];              /**< Human-readable summary */
} ce_result_t;

/**
 * @brief Network fault parameters
 */
typedef struct {
    uint32_t latency_ms;        /**< Added latency */
    uint32_t jitter_ms;         /**< Latency variance */
    float loss_percent;         /**< Packet loss percentage */
    float corrupt_percent;      /**< Corruption percentage */
    uint32_t bandwidth_limit;   /**< Bandwidth limit (bytes/sec) */
} ce_network_params_t;

/**
 * @brief Memory fault parameters
 */
typedef struct {
    size_t leak_size;           /**< Bytes to leak */
    size_t leak_rate;           /**< Bytes per second */
    float corrupt_percent;      /**< Corruption percentage */
    bool target_heap;           /**< Target heap allocations */
    bool target_stack;          /**< Target stack */
} ce_memory_params_t;

/**
 * @brief CPU fault parameters
 */
typedef struct {
    uint32_t cpu_percent;       /**< Target CPU utilization */
    uint32_t thread_count;      /**< Worker threads */
    bool use_all_cores;         /**< Stress all cores */
} ce_cpu_params_t;

/**
 * @brief Disk fault parameters
 */
typedef struct {
    size_t fill_bytes;          /**< Bytes to fill */
    uint32_t slow_factor;       /**< I/O slowdown factor */
    float error_rate;           /**< I/O error rate */
    char target_path[256];      /**< Target path */
} ce_disk_params_t;

/**
 * @brief Configuration for chaos engineering
 */
typedef struct {
    bool enable_safety_checks;      /**< Enable safety guardrails */
    uint32_t max_blast_radius;      /**< Max affected nodes */
    uint64_t max_experiment_duration_ms; /**< Max experiment time */
    float max_error_rate_increase;  /**< Max allowed error increase */
    float min_availability;         /**< Minimum availability */
    bool require_confirmation;      /**< Require manual confirmation */
    bool auto_abort_on_critical;    /**< Abort on critical events */
    bool enable_dry_run;            /**< Dry run mode */
} ce_config_t;

/**
 * @brief Fault injection callback
 */
typedef bool (*ce_inject_callback_t)(
    const ce_fault_spec_t* fault,
    const ce_target_spec_t* target,
    void* user_data
);

/**
 * @brief Fault rollback callback
 */
typedef bool (*ce_rollback_callback_t)(
    const ce_fault_spec_t* fault,
    const ce_target_spec_t* target,
    void* user_data
);

/**
 * @brief Experiment event callback
 */
typedef void (*ce_event_callback_t)(
    uint32_t experiment_id,
    ce_state_t state,
    const char* message,
    void* user_data
);

/**
 * @brief Opaque chaos engineering handle
 */
typedef struct ce_context ce_context_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create chaos engineering context
 *
 * WHAT: Initialize chaos engineering system
 * WHY:  Required before running experiments
 * HOW:  Allocate context, register fault handlers
 *
 * @param config Configuration
 * @return CE context or NULL on failure
 */
ce_context_t* ce_create(const ce_config_t* config);

/**
 * @brief Destroy chaos engineering context
 *
 * @param ctx CE context
 */
void ce_destroy(ce_context_t* ctx);

/**
 * @brief Get default configuration
 *
 * @return Default configuration
 */
ce_config_t ce_default_config(void);

//=============================================================================
// Experiment Lifecycle
//=============================================================================

/**
 * @brief Create new experiment
 *
 * @param ctx CE context
 * @param name Experiment name
 * @param description Description
 * @return Experiment ID, 0 on failure
 */
uint32_t ce_create_experiment(ce_context_t* ctx, const char* name, const char* description);

/**
 * @brief Set experiment fault specification
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @param fault Fault specification
 * @return true on success
 */
bool ce_set_fault(ce_context_t* ctx, uint32_t experiment_id, const ce_fault_spec_t* fault);

/**
 * @brief Set experiment target
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @param target Target specification
 * @return true on success
 */
bool ce_set_target(ce_context_t* ctx, uint32_t experiment_id, const ce_target_spec_t* target);

/**
 * @brief Add hypothesis to experiment
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @param hypothesis Hypothesis to add
 * @return true on success
 */
bool ce_add_hypothesis(ce_context_t* ctx, uint32_t experiment_id, const ce_hypothesis_t* hypothesis);

/**
 * @brief Add guardrail to experiment
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @param guardrail Guardrail configuration
 * @return true on success
 */
bool ce_add_guardrail(ce_context_t* ctx, uint32_t experiment_id, const ce_guardrail_config_t* guardrail);

/**
 * @brief Add metric to track
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @param metric_name Metric name
 * @return true on success
 */
bool ce_add_metric(ce_context_t* ctx, uint32_t experiment_id, const char* metric_name);

//=============================================================================
// Experiment Execution
//=============================================================================

/**
 * @brief Start experiment
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @return true if started
 */
bool ce_start_experiment(ce_context_t* ctx, uint32_t experiment_id);

/**
 * @brief Pause experiment
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @return true on success
 */
bool ce_pause_experiment(ce_context_t* ctx, uint32_t experiment_id);

/**
 * @brief Resume experiment
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @return true on success
 */
bool ce_resume_experiment(ce_context_t* ctx, uint32_t experiment_id);

/**
 * @brief Abort experiment
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @param reason Abort reason
 * @return true on success
 */
bool ce_abort_experiment(ce_context_t* ctx, uint32_t experiment_id, const char* reason);

/**
 * @brief Get experiment state
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @return Current state
 */
ce_state_t ce_get_experiment_state(ce_context_t* ctx, uint32_t experiment_id);

/**
 * @brief Get experiment details
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @param experiment Output experiment
 * @return true on success
 */
bool ce_get_experiment(ce_context_t* ctx, uint32_t experiment_id, ce_experiment_t* experiment);

/**
 * @brief Get experiment result
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @param result Output result
 * @return true if experiment complete
 */
bool ce_get_result(ce_context_t* ctx, uint32_t experiment_id, ce_result_t* result);

//=============================================================================
// Direct Fault Injection
//=============================================================================

/**
 * @brief Inject fault directly
 *
 * @param ctx CE context
 * @param fault Fault to inject
 * @param target Target for injection
 * @return true on success
 */
bool ce_inject_fault(ce_context_t* ctx, const ce_fault_spec_t* fault, const ce_target_spec_t* target);

/**
 * @brief Rollback injected fault
 *
 * @param ctx CE context
 * @param fault Fault to rollback
 * @param target Target to rollback
 * @return true on success
 */
bool ce_rollback_fault(ce_context_t* ctx, const ce_fault_spec_t* fault, const ce_target_spec_t* target);

/**
 * @brief Inject network latency
 *
 * @param ctx CE context
 * @param target_id Target node
 * @param latency_ms Latency to add
 * @param duration_ms How long
 * @return true on success
 */
bool ce_inject_network_latency(ce_context_t* ctx, uint32_t target_id, uint32_t latency_ms, uint64_t duration_ms);

/**
 * @brief Inject packet loss
 *
 * @param ctx CE context
 * @param target_id Target node
 * @param loss_percent Loss percentage
 * @param duration_ms How long
 * @return true on success
 */
bool ce_inject_packet_loss(ce_context_t* ctx, uint32_t target_id, float loss_percent, uint64_t duration_ms);

/**
 * @brief Inject CPU stress
 *
 * @param ctx CE context
 * @param target_id Target node
 * @param cpu_percent Target CPU usage
 * @param duration_ms How long
 * @return true on success
 */
bool ce_inject_cpu_stress(ce_context_t* ctx, uint32_t target_id, uint32_t cpu_percent, uint64_t duration_ms);

/**
 * @brief Inject memory pressure
 *
 * @param ctx CE context
 * @param target_id Target node
 * @param bytes Bytes to consume
 * @param duration_ms How long
 * @return true on success
 */
bool ce_inject_memory_pressure(ce_context_t* ctx, uint32_t target_id, size_t bytes, uint64_t duration_ms);

/**
 * @brief Kill process
 *
 * @param ctx CE context
 * @param target_id Target node
 * @return true on success
 */
bool ce_kill_process(ce_context_t* ctx, uint32_t target_id);

/**
 * @brief Create network partition
 *
 * @param ctx CE context
 * @param group1 First group of nodes
 * @param count1 Size of group 1
 * @param group2 Second group of nodes
 * @param count2 Size of group 2
 * @param duration_ms Partition duration
 * @return true on success
 */
bool ce_create_partition(
    ce_context_t* ctx,
    const uint32_t* group1, uint32_t count1,
    const uint32_t* group2, uint32_t count2,
    uint64_t duration_ms
);

//=============================================================================
// Callbacks
//=============================================================================

/**
 * @brief Register fault injection callback
 *
 * @param ctx CE context
 * @param fault_type Fault type
 * @param callback Injection callback
 * @param user_data User data
 * @return true on success
 */
bool ce_register_inject_callback(
    ce_context_t* ctx,
    ce_fault_type_t fault_type,
    ce_inject_callback_t callback,
    void* user_data
);

/**
 * @brief Register rollback callback
 *
 * @param ctx CE context
 * @param fault_type Fault type
 * @param callback Rollback callback
 * @param user_data User data
 * @return true on success
 */
bool ce_register_rollback_callback(
    ce_context_t* ctx,
    ce_fault_type_t fault_type,
    ce_rollback_callback_t callback,
    void* user_data
);

/**
 * @brief Register experiment event callback
 *
 * @param ctx CE context
 * @param callback Event callback
 * @param user_data User data
 * @return true on success
 */
bool ce_register_event_callback(
    ce_context_t* ctx,
    ce_event_callback_t callback,
    void* user_data
);

//=============================================================================
// Safety Functions
//=============================================================================

/**
 * @brief Check if experiment is safe to run
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @return true if safe
 */
bool ce_is_safe_to_run(ce_context_t* ctx, uint32_t experiment_id);

/**
 * @brief Validate experiment configuration
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @param errors Output error messages
 * @param max_errors Array capacity
 * @return Number of validation errors
 */
uint32_t ce_validate_experiment(
    ce_context_t* ctx,
    uint32_t experiment_id,
    char errors[][256],
    uint32_t max_errors
);

/**
 * @brief Enable/disable dry run mode
 *
 * @param ctx CE context
 * @param enabled Enable dry run
 */
void ce_set_dry_run(ce_context_t* ctx, bool enabled);

/**
 * @brief Check if dry run mode is active
 *
 * @param ctx CE context
 * @return true if dry run mode
 */
bool ce_is_dry_run(ce_context_t* ctx);

//=============================================================================
// Game Days
//=============================================================================

/**
 * @brief Schedule game day (coordinated chaos session)
 *
 * @param ctx CE context
 * @param experiment_ids Array of experiments
 * @param count Number of experiments
 * @param start_time_ms Scheduled start
 * @return Game day ID
 */
uint32_t ce_schedule_game_day(
    ce_context_t* ctx,
    const uint32_t* experiment_ids,
    uint32_t count,
    uint64_t start_time_ms
);

/**
 * @brief Start game day
 *
 * @param ctx CE context
 * @param game_day_id Game day ID
 * @return true on success
 */
bool ce_start_game_day(ce_context_t* ctx, uint32_t game_day_id);

/**
 * @brief Abort game day
 *
 * @param ctx CE context
 * @param game_day_id Game day ID
 * @return true on success
 */
bool ce_abort_game_day(ce_context_t* ctx, uint32_t game_day_id);

//=============================================================================
// Reporting
//=============================================================================

/**
 * @brief Generate experiment report
 *
 * @param ctx CE context
 * @param experiment_id Experiment ID
 * @param buffer Output buffer
 * @param buffer_size Buffer capacity
 * @return Bytes written
 */
size_t ce_generate_report(ce_context_t* ctx, uint32_t experiment_id, char* buffer, size_t buffer_size);

/**
 * @brief List all experiments
 *
 * @param ctx CE context
 * @param experiments Output array
 * @param max_experiments Array capacity
 * @return Number of experiments
 */
uint32_t ce_list_experiments(ce_context_t* ctx, ce_experiment_t* experiments, uint32_t max_experiments);

//=============================================================================
// String Conversion
//=============================================================================

const char* ce_fault_type_to_string(ce_fault_type_t type);
const char* ce_state_to_string(ce_state_t state);
const char* ce_hypothesis_result_to_string(ce_hypothesis_result_t result);
const char* ce_pattern_to_string(ce_pattern_t pattern);
const char* ce_target_strategy_to_string(ce_target_strategy_t strategy);
const char* ce_guardrail_to_string(ce_guardrail_t guardrail);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CHAOS_ENGINEERING_H
