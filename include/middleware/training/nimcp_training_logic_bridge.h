//=============================================================================
// nimcp_training_logic_bridge.h - Training-Logic Bridge Integration
//=============================================================================
//
// WHAT: Bidirectional bridge integrating training pipeline with neural logic
//       gates for symbolic reasoning about training decisions.
//
// WHY: Models prefrontal cortex executive control over learning. Training
//      metrics provide evidence for logical decision-making, while logic
//      gates determine when to adjust LR, pause training, or checkpoint.
//
// HOW: Training -> Logic: Metrics converted to boolean conditions
//      Logic -> Training: Gate outputs modulate training parameters
//      Integrates with immune (inflammation), Portia (resources), swarm (consensus)
//
//=============================================================================

#ifndef NIMCP_TRAINING_LOGIC_BRIDGE_H
#define NIMCP_TRAINING_LOGIC_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct training_logic_bridge training_logic_bridge_t;
typedef struct nimcp_brain_training_ctx nimcp_brain_training_ctx_t;
typedef struct training_immune_system training_immune_system_t;
typedef struct portia_logic_bridge portia_logic_bridge_t;
typedef struct swarm_logic_bridge swarm_logic_bridge_t;
typedef struct portia_swarm_logic_bridge portia_swarm_logic_bridge_t;
typedef struct perception_training_bridge perception_training_bridge_t;
typedef struct cortical_training_bridge cortical_training_bridge_t;

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define TRAINING_LOGIC_MODULE_NAME      "training_logic_bridge"
#define TRAINING_LOGIC_MODULE_VERSION   "1.1.0"

/** Pre-built gate IDs */
#define TRAINING_LOGIC_GATE_STABILITY_CHECK      1
#define TRAINING_LOGIC_GATE_NEED_INTERVENTION    2
#define TRAINING_LOGIC_GATE_SAFE_TO_INCREASE_LR  3
#define TRAINING_LOGIC_GATE_BATCH_SIZE_DECISION  4
#define TRAINING_LOGIC_GATE_CHECKPOINT_DECISION  5
#define TRAINING_LOGIC_GATE_CUSTOM_START         100

/** Default thresholds */
#define TRAINING_LOGIC_DEFAULT_STABLE_STEPS         10
#define TRAINING_LOGIC_DEFAULT_LR_INCREASE_FACTOR   1.1f
#define TRAINING_LOGIC_DEFAULT_LR_DECREASE_FACTOR   0.5f
#define TRAINING_LOGIC_DEFAULT_BATCH_SCALE_FACTOR   0.75f
#define TRAINING_LOGIC_DEFAULT_CHECKPOINT_INTERVAL  100
#define TRAINING_LOGIC_DEFAULT_CONSENSUS_TIMEOUT_MS 1000
#define TRAINING_LOGIC_DEFAULT_CONFIDENCE_THRESHOLD 0.7f

/** Limits */
#define TRAINING_LOGIC_MAX_CUSTOM_GATES    50
#define TRAINING_LOGIC_MAX_HISTORY_SIZE    100
#define TRAINING_LOGIC_MAX_REASON_LENGTH   256

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    TRAINING_COND_LOSS_STABLE = 0,
    TRAINING_COND_GRAD_STABLE,
    TRAINING_COND_LR_REASONABLE,
    TRAINING_COND_MEMORY_OK,
    TRAINING_COND_THROUGHPUT_OK,
    TRAINING_COND_NOT_MID_BATCH,
    TRAINING_COND_SUFFICIENT_PROGRESS,
    TRAINING_COND_GRAD_EXPLODING,
    TRAINING_COND_LOSS_NAN,
    TRAINING_COND_DIVERGING,
    TRAINING_COND_STABLE_FOR_N_STEPS,
    TRAINING_COND_IMMUNE_OK,
    TRAINING_COND_RESOURCE_OK,
    TRAINING_COND_SWARM_CONSENSUS,
    TRAINING_COND_PERCEPTION_QUALITY,
    TRAINING_COND_CORTICAL_STABLE,
    TRAINING_COND_PREDICTIONS_OK,
    TRAINING_COND_COUNT
} training_logic_condition_t;

typedef enum {
    TRAINING_DECISION_CONTINUE = 0,
    TRAINING_DECISION_PAUSE,
    TRAINING_DECISION_RESUME,
    TRAINING_DECISION_INCREASE_LR,
    TRAINING_DECISION_DECREASE_LR,
    TRAINING_DECISION_INCREASE_BATCH,
    TRAINING_DECISION_DECREASE_BATCH,
    TRAINING_DECISION_CHECKPOINT,
    TRAINING_DECISION_ROLLBACK,
    TRAINING_DECISION_TERMINATE,
    TRAINING_DECISION_COUNT
} training_logic_decision_type_t;

typedef enum {
    TRAINING_LOGIC_MODE_DISABLED = 0,
    TRAINING_LOGIC_MODE_MONITOR_ONLY,
    TRAINING_LOGIC_MODE_ADVISORY,
    TRAINING_LOGIC_MODE_AUTOMATIC,
    TRAINING_LOGIC_MODE_CONSENSUS_REQUIRED
} training_logic_mode_t;

typedef enum {
    LOGIC_INSTABILITY_NONE = 0,
    LOGIC_INSTABILITY_LOSS_NAN,
    LOGIC_INSTABILITY_LOSS_INF,
    LOGIC_INSTABILITY_LOSS_EXPLOSION,
    LOGIC_INSTABILITY_GRAD_EXPLOSION,
    LOGIC_INSTABILITY_GRAD_VANISHING,
    LOGIC_INSTABILITY_LOSS_PLATEAU,
    LOGIC_INSTABILITY_OSCILLATION,
    LOGIC_INSTABILITY_COUNT
} training_logic_instability_t;

/** Continuous instability metrics (v1.1.0) */
typedef struct {
    float instability_score;
    float gradient_variance;
    float loss_volatility;
    float gradient_explosion;
    float gradient_vanishing;
    float loss_plateau;
    float nan_inf_severity;
    training_logic_instability_t derived_label;
} training_instability_metrics_t;

/** Continuous instability response (v1.1.0) */
typedef struct {
    float lr_scale;
    float clip_threshold;
    float batch_scale;
    float pause_urgency;
    float checkpoint_urgency;
    float rollback_urgency;
    training_logic_instability_t derived_label;
} training_instability_response_t;

//=============================================================================
// Structures
//=============================================================================

typedef struct {
    bool loss_stable;
    bool grad_stable;
    bool lr_reasonable;
    bool memory_ok;
    bool throughput_ok;
    bool not_mid_batch;
    bool sufficient_progress;
    bool grad_exploding;
    bool loss_nan;
    bool diverging;
    bool stable_for_n_steps;
    bool immune_ok;
    bool resource_ok;
    bool swarm_consensus;
    bool perception_quality;
    bool cortical_stable;
    bool predictions_ok;
    float loss_current;
    float loss_trend;
    float grad_norm;
    float learning_rate;
    float memory_usage;
    float throughput;
    uint32_t steps_since_checkpoint;
    uint32_t stable_step_count;
    uint64_t current_step;
    training_instability_metrics_t instability;
} training_logic_conditions_t;

typedef struct {
    training_logic_decision_type_t type;
    bool approved;
    float confidence;
    float modulation_factor;
    char reason[TRAINING_LOGIC_MAX_REASON_LENGTH];
    uint64_t evaluation_time_us;
    bool stability_check_passed;
    bool intervention_needed;
    bool safe_to_increase_lr;
    bool batch_size_ok;
    bool checkpoint_needed;
} training_logic_decision_t;

typedef struct {
    training_logic_mode_t mode;
    float stability_threshold;
    float intervention_threshold;
    float lr_increase_threshold;
    float confidence_threshold;
    float lr_increase_factor;
    float lr_decrease_factor;
    float batch_scale_factor;
    uint32_t stable_steps_required;
    uint32_t checkpoint_interval;
    bool enable_immune_integration;
    bool enable_portia_integration;
    bool enable_swarm_integration;
    bool enable_bio_async;
    float min_learning_rate;
    float max_learning_rate;
    uint32_t min_batch_size;
    uint32_t max_batch_size;
    uint32_t consensus_timeout_ms;
    float consensus_threshold;
    uint32_t history_size;
    bool disable_auto_update;
} training_logic_config_t;

typedef struct {
    uint64_t total_decisions;
    uint64_t decisions_by_type[TRAINING_DECISION_COUNT];
    uint64_t stability_checks;
    uint64_t stability_passed;
    uint64_t intervention_triggers;
    uint64_t lr_increase_allowed;
    uint64_t lr_decrease_triggered;
    uint64_t batch_adjustments;
    uint64_t checkpoints_triggered;
    uint64_t lr_increases;
    uint64_t lr_decreases;
    uint64_t batch_increases;
    uint64_t batch_decreases;
    float avg_decision_time_us;
    float max_decision_time_us;
    uint64_t total_decision_time_us;
    uint64_t consensus_requests;
    uint64_t consensus_achieved;
    uint64_t consensus_timeouts;
    training_logic_mode_t current_mode;
    bool currently_paused;
    uint64_t last_decision_time_ms;
    uint32_t custom_gate_count;
} training_logic_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

void training_logic_default_config(training_logic_config_t* config);
training_logic_bridge_t* training_logic_create(const training_logic_config_t* config);
void training_logic_destroy(training_logic_bridge_t* bridge);
int training_logic_start(training_logic_bridge_t* bridge);
int training_logic_stop(training_logic_bridge_t* bridge);

//=============================================================================
// Integration API
//=============================================================================

int training_logic_connect_brain_training(training_logic_bridge_t* bridge, nimcp_brain_training_ctx_t* training_ctx);
int training_logic_connect_training_immune(training_logic_bridge_t* bridge, training_immune_system_t* immune_system);
int training_logic_connect_portia_logic(training_logic_bridge_t* bridge, portia_logic_bridge_t* portia_logic);
int training_logic_connect_swarm_logic(training_logic_bridge_t* bridge, swarm_logic_bridge_t* swarm_logic);
int training_logic_connect_unified(training_logic_bridge_t* bridge, portia_swarm_logic_bridge_t* unified_bridge);
int training_logic_connect_perception_training(training_logic_bridge_t* bridge, perception_training_bridge_t* perception_training);
int training_logic_connect_cortical_training(training_logic_bridge_t* bridge, cortical_training_bridge_t* cortical_training);

//=============================================================================
// Training -> Logic: Metric Updates
//=============================================================================

int training_logic_update_metrics(training_logic_bridge_t* bridge, float loss, float grad_norm, float learning_rate, uint64_t step);
int training_logic_update_batch_metrics(training_logic_bridge_t* bridge, uint32_t batch_size, float throughput, float memory_usage);
int training_logic_signal_instability(training_logic_bridge_t* bridge, training_logic_instability_t instability_type, uint32_t severity);

//=============================================================================
// Continuous Instability API (v1.1.0)
//=============================================================================

int training_logic_compute_instability_response(const training_logic_bridge_t* bridge, training_instability_response_t* response);
training_logic_instability_t training_logic_classify_instability(const training_instability_metrics_t* metrics);
int training_logic_get_instability_metrics(const training_logic_bridge_t* bridge, training_instability_metrics_t* metrics);
const char* training_logic_instability_to_string(training_logic_instability_t type);

//=============================================================================
// Logic -> Training: Decision Evaluation
//=============================================================================

bool training_logic_check_stability(training_logic_bridge_t* bridge);
bool training_logic_needs_intervention(training_logic_bridge_t* bridge);
bool training_logic_can_increase_lr(training_logic_bridge_t* bridge);
bool training_logic_should_adjust_batch(training_logic_bridge_t* bridge, bool* increase_batch);
bool training_logic_should_checkpoint(training_logic_bridge_t* bridge);
int training_logic_get_decision(training_logic_bridge_t* bridge, training_logic_decision_t* decision);

//=============================================================================
// Modulation API
//=============================================================================

float training_logic_get_lr_modulation(const training_logic_bridge_t* bridge, float base_lr);
uint32_t training_logic_get_batch_size_modulation(const training_logic_bridge_t* bridge, uint32_t base_batch_size);
int training_logic_apply_decision(training_logic_bridge_t* bridge, const training_logic_decision_t* decision);

//=============================================================================
// Condition Management API
//=============================================================================

int training_logic_update_conditions(training_logic_bridge_t* bridge);
int training_logic_get_conditions(const training_logic_bridge_t* bridge, training_logic_conditions_t* conditions);
int training_logic_set_condition(training_logic_bridge_t* bridge, training_logic_condition_t condition, bool value);
int training_logic_set_numeric_condition(training_logic_bridge_t* bridge, const char* name, float value);

//=============================================================================
// Custom Gate API
//=============================================================================

int training_logic_add_custom_gate(training_logic_bridge_t* bridge, const char* expression, uint32_t* gate_id);
bool training_logic_evaluate_gate(training_logic_bridge_t* bridge, uint32_t gate_id);
int training_logic_get_gate_decision(training_logic_bridge_t* bridge, uint32_t gate_id, training_logic_decision_t* decision);

//=============================================================================
// Bio-Async API
//=============================================================================

int training_logic_connect_bio_async(training_logic_bridge_t* bridge);
int training_logic_disconnect_bio_async(training_logic_bridge_t* bridge);
bool training_logic_is_bio_async_connected(const training_logic_bridge_t* bridge);
int training_logic_process_inbox(training_logic_bridge_t* bridge);
int training_logic_broadcast_decision(training_logic_bridge_t* bridge, const training_logic_decision_t* decision);

//=============================================================================
// Statistics API
//=============================================================================

int training_logic_get_stats(const training_logic_bridge_t* bridge, training_logic_stats_t* stats);
int training_logic_reset_stats(training_logic_bridge_t* bridge);

//=============================================================================
// Utility API
//=============================================================================

const char* training_logic_condition_to_string(training_logic_condition_t condition);
const char* training_logic_decision_type_to_string(training_logic_decision_type_t type);
const char* training_logic_mode_to_string(training_logic_mode_t mode);
void training_logic_dump_state(const training_logic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_LOGIC_BRIDGE_H */
