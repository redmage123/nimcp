/**
 * @file nimcp_health_agent_internal.h
 * @brief Internal Health Agent Structure Definitions
 * @version 2.0.0
 * @date 2026-02-16
 *
 * Internal header for health agent implementation.
 * Decomposes massive struct into SRP-compliant sub-structures.
 */

#ifndef NIMCP_HEALTH_AGENT_INTERNAL_H
#define NIMCP_HEALTH_AGENT_INTERNAL_H

#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "constants/nimcp_buffer_constants.h"

/* Module integrations */
#include "portia/nimcp_portia.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "swarm/nimcp_swarm_immune.h"
#include "swarm/nimcp_swarm_memory.h"
#include "dragonfly/nimcp_dragonfly_immune_bridge.h"
#include "portia/nimcp_portia_monitoring.h"
#include "snn/nimcp_snn_immune.h"
#include "lnn/nimcp_lnn_immune.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_immune_bridge.h"
#include "utils/fault_tolerance/nimcp_checkpoint.h"
#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "cognitive/introspection/nimcp_connectivity_health.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_kg_gc.h"
#include "utils/fault_tolerance/nimcp_runtime_adaptation.h"
#include "utils/gpu/nimcp_gpu_health.h"
#include "utils/fault_tolerance/nimcp_capacity_manager.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_thalamic_bridge.h"
#include "middleware/training/nimcp_brain_training_integration.h"
#include "perception/cortical/nimcp_visual_cortical_bridge.h"
#include "perception/cortical/nimcp_audio_cortical_bridge.h"
#include "core/cortical_columns/nimcp_cortical_immune.h"
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "cognitive/immune/nimcp_brain_immune_tick.h"
#include "constants/nimcp_timing_constants.h"
#include "constants/nimcp_learning_constants.h"

#include <stdatomic.h>


/* Additional includes from original source */
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_lockfree_metrics.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/thread/nimcp_deadlock_detector.h"
#include <atomic>
#include <errno.h>
#include <math.h>
#include <pthread.h>  /* Still needed for pthread_timedjoin_np */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/* ============================================================================
 * Internal Struct Definitions (moved from .c for SRP split access)
 * ============================================================================ */

/* Collective cognition forward declarations (avoiding header includes) */
typedef struct {
    float we_mode_strength;         /**< Strength of "we" identification [0-1] */
    float joint_commitment;         /**< Collective commitment level [0-1] */
    float mutual_responsiveness;    /**< Responsiveness to each other [0-1] */
    float role_understanding;       /**< Understanding of roles [0-1] */
    uint32_t active_shared_goals;   /**< Number of active shared goals */
    uint32_t active_joint_attentions;
} health_agent_we_mode_state_t;

typedef struct {
    float phi_local;
    float phi_network;
    float phi_total;
    float information;
    float integration;
    float exclusion;
} health_agent_collective_phi_t;

typedef struct {
    uint64_t goals_submitted;
    uint64_t goals_completed;
    uint64_t goals_failed;
    uint64_t goals_timeout;
    float avg_confidence;
    uint32_t active_goals;
    uint32_t pending_goals;
    uint64_t immune_modulations;
} health_agent_rcog_stats_t;

     * The event structure contains drive type and urgency level.
     * We use this to adjust health agent behavior.
     */
    typedef struct {
        uint32_t drive_type;
        float urgency;
    } drive_event_t;

 * Helper: Perform single brain probe and update metrics
 * ------------------------------------------------------------------------- */

/* Local struct to match brain probe data (avoids nimcp.h dependency) */
typedef struct {
    uint32_t num_neurons;
    uint32_t num_synapses;
    uint32_t num_active_synapses;
    size_t memory_bytes;
    float avg_inference_time_us;
    float current_learning_rate;
    float avg_sparsity;
    float accuracy;
    bool is_cow_clone;
    uint32_t cow_ref_count;
    size_t cow_shared_bytes;
    size_t cow_private_bytes;
} local_brain_probe_data_t;

/* ============================================================================
 * Internal Enum Definitions (moved from .c for SRP split access)
 * ============================================================================ */

typedef enum {
    ETHICS_EVAL_APPROVED = 0,
    ETHICS_EVAL_BLOCKED = 1,
    ETHICS_EVAL_CONDITIONAL = 2,
    ETHICS_EVAL_ERROR = -1
} ethics_eval_result_t;

typedef enum {
    COLLECTIVE_CONSENSUS_NONE = 0,
    COLLECTIVE_CONSENSUS_ACHIEVED = 1,
    COLLECTIVE_CONSENSUS_TIMEOUT = 2,
    COLLECTIVE_CONSENSUS_ERROR = -1
} collective_consensus_result_t;

typedef enum {
    COLLECTIVE_CONSCIOUSNESS_NONE = 0,
    COLLECTIVE_CONSCIOUSNESS_MINIMAL,
    COLLECTIVE_CONSCIOUSNESS_EMERGING,
    COLLECTIVE_CONSCIOUSNESS_PARTIAL,
    COLLECTIVE_CONSCIOUSNESS_UNIFIED,
    COLLECTIVE_CONSCIOUSNESS_TRANSCENDENT
} health_agent_consciousness_level_t;

typedef enum {
    RCOG_ENGINE_UNINITIALIZED = 0,
    RCOG_ENGINE_INITIALIZING,
    RCOG_ENGINE_READY,
    RCOG_ENGINE_PROCESSING,
    RCOG_ENGINE_PAUSED,
    RCOG_ENGINE_DEGRADED,
    RCOG_ENGINE_SHUTTING_DOWN,
    RCOG_ENGINE_STOPPED
} health_agent_rcog_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Lock-free message queue capacity */
#define AGENT_MSG_QUEUE_CAPACITY  2048

/** Maximum time to wait for agent thread to stop (ms) */
#define AGENT_STOP_TIMEOUT_MS     NIMCP_DEFAULT_TIMEOUT_MS

/** Minimum check interval (ms) */
#define AGENT_MIN_CHECK_INTERVAL_MS  10

/** Maximum detector name length */
#define DETECTOR_NAME_MAX_LEN     32

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Cognitive module forward declarations */
typedef struct failure_predictor failure_predictor_t;
typedef struct metacognition metacognition_t;
typedef struct ethics_engine ethics_engine_t;
typedef struct emotional_system emotional_system_t;
typedef struct emotion_immune_bridge emotion_immune_bridge_t;
typedef struct wellbeing_monitor wellbeing_monitor_t;
typedef struct mental_health_monitor mental_health_monitor_t;
typedef struct collective_cognition collective_cognition_t;
typedef struct rcog_engine rcog_engine_t;

/* Memory system forward declarations */
typedef struct engram_system engram_system_t;
typedef struct systems_consolidation_system systems_consolidation_system_t;
typedef struct nimcp_hippocampus nimcp_hippocampus_t;
typedef struct nimcp_mammillary nimcp_mammillary_t;

/* World model forward declarations */
typedef struct jepa_predictor jepa_predictor_t;
typedef struct omni_world_model omni_world_model_t;
typedef struct imagination_engine imagination_engine_t;

/* Exception system */
typedef struct exception_immune exception_immune_t;

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Lock-free message node for agent queue
 */
typedef struct health_msg_node {
    health_agent_message_t msg;
    _Atomic uint64_t sequence;           /**< Sequence number for ordering */
} health_msg_node_t;

/**
 * @brief Lock-free message queue (MPSC - multiple producer, single consumer)
 */
typedef struct {
    health_msg_node_t* nodes;
    uint32_t capacity;
    uint32_t capacity_mask;
    _Atomic uint64_t head;               /**< Write position (producers) */
    _Atomic uint64_t tail;               /**< Read position (consumer) */
    _Atomic uint64_t dropped_count;
} health_msg_queue_t;

/**
 * @brief Heartbeat state (shared between main system and agent)
 */
typedef struct {
    _Atomic uint64_t last_heartbeat_us;  /**< Last heartbeat timestamp */
    _Atomic uint32_t missed_count;       /**< Consecutive missed heartbeats */
    char current_operation[NIMCP_ID_BUFFER_SIZE];          /**< Current operation name */
    _Atomic float current_progress;      /**< Current progress (0.0-1.0) */
} heartbeat_state_t;

/**
 * @brief Cognitive module state
 */
typedef struct {
    /* Failure prediction */
    failure_predictor_t* failure_predictor;
    health_agent_prediction_config_t prediction_config;
    _Atomic uint64_t predictions_made;
    _Atomic uint64_t predictions_correct;
    _Atomic uint64_t preventive_actions;

    /* Metacognition */
    metacognition_t* metacognition;
    health_agent_metacog_config_t metacog_config;
    _Atomic uint64_t self_diagnoses;
    _Atomic uint64_t degradation_alerts;
    _Atomic float current_confidence;

    /* Ethics engine */
    ethics_engine_t ethics;
    health_agent_ethics_config_t ethics_config;
    _Atomic uint64_t ethics_evaluations;
    _Atomic uint64_t ethics_blocks;
    _Atomic uint64_t mercy_applications;

    /* Emotional system */
    emotional_system_t* emotion;
    emotion_immune_bridge_t* emotion_immune;
    health_agent_emotion_config_t emotion_config;
    _Atomic float current_stress_level;
    _Atomic uint64_t emotion_adjustments;

    /* Wellbeing monitor */
    wellbeing_monitor_t* wellbeing;
    health_agent_wellbeing_config_t wellbeing_config;
    _Atomic uint64_t distress_detections;
    _Atomic uint64_t wellbeing_interventions;
    _Atomic float current_distress_level;

    /* Mental health monitor */
    mental_health_monitor_t* mental_health;

    /* Collective cognition */
    collective_cognition_t* collective;
    health_agent_collective_config_t collective_config;
    _Atomic uint64_t consensus_requests;
    _Atomic uint64_t consensus_achieved;
    _Atomic float avg_consensus_time_ms;

    /* Recursive cognition (RCOG) */
    rcog_engine_t* rcog;
    health_agent_rcog_config_t rcog_config;
    _Atomic uint64_t rcog_diagnoses;
    _Atomic uint64_t rcog_recovery_plans;
    _Atomic float avg_rcog_time_ms;

    /* GPU health */
    gpu_health_monitor_t* gpu_health;
    health_agent_gpu_config_t gpu_config;
    _Atomic uint64_t gpu_accelerated_checks;
    _Atomic float gpu_utilization;
    _Atomic bool gpu_healthy;
} health_agent_cognitive_state_t;

/**
 * @brief Neural module state (SNN/LNN)
 */
typedef struct {
    /* SNN immune bridge */
    snn_immune_bridge_t* snn_bridge;
    health_agent_snn_config_t snn_config;
    _Atomic uint64_t snn_checks_run;
    _Atomic uint64_t snn_instabilities_detected;
    _Atomic uint64_t snn_recoveries_triggered;
    uint64_t last_snn_check_us;

    /* LNN immune bridge */
    lnn_immune_bridge_t* lnn_bridge;
    health_agent_lnn_config_t lnn_config;
    _Atomic uint64_t lnn_checks_run;
    _Atomic uint64_t lnn_instabilities_detected;
    _Atomic uint64_t lnn_recoveries_triggered;
    uint64_t last_lnn_check_us;

    /* Combined neural health metrics */
    neural_health_metrics_t metrics;
} health_agent_neural_state_t;

/**
 * @brief Behavioral module state (Dragonfly/Portia)
 */
typedef struct {
    /* Portia adaptive resource management */
    portia_context_t* portia;
    health_agent_portia_config_t portia_config;
    _Atomic uint64_t portia_tier_changes;
    _Atomic uint64_t portia_degradations;

    /* Dragonfly tracking/interception */
    dragonfly_system_t* dragonfly;
    health_agent_dragonfly_config_t dragonfly_config;
    _Atomic uint64_t dragonfly_anomalies_tracked;
    _Atomic uint64_t dragonfly_pursuits;
    _Atomic uint64_t dragonfly_interceptions;
    _Atomic uint32_t dragonfly_current_target;

    /* Dragonfly immune bridge */
    dragonfly_immune_bridge_t dragonfly_immune;
    health_agent_dragonfly_immune_config_t dragonfly_immune_config;
    _Atomic uint64_t dragonfly_immune_checks_run;
    _Atomic uint64_t dragonfly_stress_events;
    _Atomic uint64_t dragonfly_injury_events;
    _Atomic uint64_t dragonfly_rest_triggers;
    uint64_t last_dragonfly_immune_check_us;

    /* Portia monitor */
    portia_monitor_t portia_monitor;
    health_agent_portia_monitor_config_t portia_monitor_config;
    _Atomic uint64_t portia_monitor_checks_run;
    _Atomic uint64_t portia_thermal_warnings;
    _Atomic uint64_t portia_power_warnings;
    _Atomic uint64_t portia_coordination_actions;
    uint64_t last_portia_monitor_check_us;

    /* Combined behavioral health metrics */
    behavioral_health_metrics_t metrics;

    /* Cross-module coordination state */
    _Atomic bool thermal_abort_active;
    _Atomic bool power_conservation_active;
    _Atomic bool rest_period_active;
    _Atomic uint64_t last_coordination_us;
} health_agent_behavioral_state_t;

/**
 * @brief Homeostasis module state (Hypothalamus)
 */
typedef struct {
    /* Hypothalamus orchestrator */
    hypo_orchestrator_t hypothalamus;
    health_agent_hypothalamus_config_t config;
    uint32_t hypo_bridge_id;
    _Atomic bool in_stress_response;
    _Atomic bool in_sickness_mode;
    _Atomic uint64_t stress_responses;
    _Atomic uint64_t sickness_mode_entries;
    _Atomic uint64_t drive_events_published;

    /* Hypothalamus homeostasis system */
    hypo_homeostasis_handle_t* homeostasis;
    _Atomic float homeostatic_output;

    /* Hypothalamus immune bridge */
    hypo_immune_bridge_t* hypo_immune_bridge;

    /* Hypothalamus drive system */
    hypo_drive_system_handle_t* drives;
} health_agent_homeostasis_state_t;

/**
 * @brief Memory system state (Hippocampus, Mammillary, Engram, Consolidation)
 */
typedef struct {
    /* Hippocampus */
    nimcp_hippocampus_t* hippocampus;
    health_agent_hippocampus_config_t hippocampus_config;
    _Atomic bool hippocampus_connected;
    _Atomic uint64_t hippocampus_checks_run;
    _Atomic uint64_t hippocampus_anomalies;
    _Atomic uint64_t hippocampus_recoveries;
    uint64_t last_hippocampus_check_us;

    /* Mammillary bodies */
    nimcp_mammillary_t* mammillary;
    health_agent_mammillary_config_t mammillary_config;
    _Atomic bool mammillary_connected;
    _Atomic uint64_t mammillary_checks_run;
    _Atomic uint64_t mammillary_anomalies;
    _Atomic uint64_t mammillary_recoveries;
    uint64_t last_mammillary_check_us;

    /* Engram memory system */
    engram_system_t* engram;
    health_agent_engram_config_t engram_config;
    _Atomic uint64_t engram_encodings;
    _Atomic uint64_t engram_recalls;

    /* Systems consolidation */
    systems_consolidation_system_t* memory_consolidation;
    health_agent_memory_consolidation_config_t consolidation_config;

    /* Memory system cross-tier tracking */
    _Atomic uint64_t memory_consistency_checks;
    _Atomic uint64_t memory_tier_mismatches;
    _Atomic uint64_t memory_recoveries_total;
} health_agent_memory_state_t;

/**
 * @brief Swarm module state
 */
typedef struct {
    /* Swarm immune system */
    NimcpSwarmImmuneSystem* swarm_immune;
    health_agent_swarm_immune_config_t swarm_immune_config;
    _Atomic uint64_t swarm_threats_detected;
    _Atomic uint64_t swarm_responses_generated;
    _Atomic uint64_t swarm_coordinated_responses;

    /* Swarm memory consolidation */
    NimcpSwarmMemory* swarm_memory;
    health_agent_swarm_memory_config_t swarm_memory_config;
    _Atomic uint64_t swarm_memories_stored;
    _Atomic uint64_t swarm_replays_performed;
} health_agent_swarm_state_t;

/**
 * @brief System module state (Connectivity, Oscillations, GC, Checkpoints, Bio-Async, etc.)
 */
typedef struct {
    /* Connectivity health monitor */
    connectivity_health_t* connectivity;
    health_agent_connectivity_config_t connectivity_config;

    /* Brain oscillations monitor */
    brain_oscillations_t* oscillations;
    health_agent_oscillations_config_t oscillations_config;

    /* Knowledge graph GC */
    kg_gc_context_t* gc_context;
    health_agent_gc_config_t gc_config;
    _Atomic uint64_t gc_triggers;
    uint64_t last_gc_time_us;

    /* Checkpoint manager */
    checkpoint_manager_t* checkpoint;
    health_agent_checkpoint_config_t checkpoint_config;
    _Atomic uint64_t checkpoints_created;
    _Atomic uint64_t rollbacks_performed;
    uint64_t last_checkpoint_time_us;

    /* Deadlock detector */
    deadlock_detector_t* deadlock_detector_ptr;

    /* Bio-async router */
    bio_router_t bio_async_router;
    health_agent_bio_async_config_t bio_async_config;
    uint32_t bio_async_module_id;
    _Atomic uint64_t bio_async_events_published;

    /* Runtime adaptation */
    runtime_adaptation_context_t runtime_adaptation;
    _Atomic uint64_t load_reductions;
    _Atomic bool load_reduced;

    /* Exception-immune bridge */
    exception_immune_t* exception_bridge;
    health_agent_exception_config_t exception_config;

    /* Capacity management */
    capacity_manager_t* capacity_managers[HEALTH_AGENT_MAX_CAPACITY_MANAGERS];
    _Atomic uint32_t num_capacity_managers;
    _Atomic uint64_t capacity_checks_run;
    _Atomic uint64_t capacity_warnings_triggered;
    _Atomic uint64_t capacity_expansions_triggered;
    char most_critical_module[NIMCP_ID_BUFFER_SIZE];

    /* Symbolic logic health */
    symbolic_logic_t* logic_engines[HEALTH_AGENT_MAX_LOGIC_ENGINES];
    _Atomic uint32_t num_logic_engines;
    health_agent_symbolic_logic_config_t logic_config;
    _Atomic uint64_t logic_checks_run;
    _Atomic uint64_t logic_anomalies_detected;
    _Atomic uint64_t logic_recoveries_performed;
    _Atomic uint64_t logic_loop_detections;
    _Atomic uint64_t logic_kb_corruptions;
    _Atomic float logic_health_score;

    /* Neural substrate health */
    neural_substrate_t* substrates[HEALTH_AGENT_MAX_NEURAL_SUBSTRATES];
    _Atomic uint32_t num_substrates;
    health_agent_substrate_config_t substrate_config;
    _Atomic uint64_t substrate_checks_run;
    _Atomic uint64_t substrate_anomalies_detected;
    _Atomic uint64_t substrate_recoveries_performed;
    _Atomic uint64_t substrate_critical_events;
    _Atomic float substrate_health_score;

    /* Thalamic/middleware health */
    omni_wm_thalamic_bridge_t* thalamic_bridges[HEALTH_AGENT_MAX_THALAMIC_BRIDGES];
    _Atomic uint32_t num_thalamic_bridges;
    health_agent_thalamic_config_t thalamic_config;
    _Atomic uint64_t thalamic_checks_run;
    _Atomic uint64_t thalamic_anomalies_detected;
    _Atomic uint64_t thalamic_recoveries_performed;
    _Atomic uint64_t thalamic_critical_events;
    _Atomic float thalamic_health_score;

    nimcp_brain_training_ctx_t* training_contexts[HEALTH_AGENT_MAX_TRAINING_CONTEXTS];
    _Atomic uint32_t num_training_contexts;
    health_agent_middleware_config_t middleware_config;
    _Atomic uint64_t middleware_checks_run;
    _Atomic uint64_t middleware_anomalies_detected;
    _Atomic uint64_t middleware_recoveries_performed;
    _Atomic uint64_t middleware_critical_events;
    _Atomic float middleware_health_score;

    /* Perception/cortical health */
    visual_cortical_bridge_t* visual_bridges[HEALTH_AGENT_MAX_PERCEPTION_BRIDGES];
    _Atomic uint32_t num_visual_bridges;
    audio_cortical_bridge_t* audio_bridges[HEALTH_AGENT_MAX_PERCEPTION_BRIDGES];
    _Atomic uint32_t num_audio_bridges;
    health_agent_perception_config_t perception_config;
    _Atomic uint64_t perception_checks_run;
    _Atomic uint64_t perception_anomalies_detected;
    _Atomic uint64_t perception_recoveries_performed;
    _Atomic uint64_t perception_critical_events;
    _Atomic float perception_health_score;

    cortical_immune_system_t* cortical_immune_systems[HEALTH_AGENT_MAX_PERCEPTION_BRIDGES];
    _Atomic uint32_t num_cortical_immune_systems;
    hypercolumn_t* cortical_columns[HEALTH_AGENT_MAX_CORTICAL_COLUMNS];
    _Atomic uint32_t num_cortical_columns;
    health_agent_cortical_config_t cortical_config;
    _Atomic uint64_t cortical_checks_run;
    _Atomic uint64_t cortical_anomalies_detected;
    _Atomic uint64_t cortical_recoveries_performed;
    _Atomic uint64_t cortical_critical_events;
    _Atomic float cortical_health_score;

    /* Brain probe health monitoring */
    brain_t monitored_brains[HEALTH_AGENT_MAX_BRAINS];
    brain_probe_health_metrics_t brain_metrics[HEALTH_AGENT_MAX_BRAINS];
    health_agent_brain_probe_config_t brain_probe_configs[HEALTH_AGENT_MAX_BRAINS];
    _Atomic uint32_t num_monitored_brains;
    health_agent_brain_probe_config_t brain_probe_config;
    _Atomic uint64_t brain_probes_run;
    _Atomic uint64_t brain_warnings_triggered;
    _Atomic uint64_t brain_critical_events;
    _Atomic uint64_t brain_recoveries_performed;
    _Atomic float brain_probe_health_score;
    uint64_t last_brain_probe_us;

    /* World model & imagination health */
    jepa_predictor_t* jepa_predictor;
    omni_world_model_t* world_model;
    imagination_engine_t* imagination;
    health_agent_wm_imagination_config_t wm_imagination_config;
    jepa_health_metrics_t jepa_metrics;
    omni_wm_health_metrics_t wm_metrics;
    imagination_health_metrics_t imagination_metrics;
    world_imagination_health_t combined_wm_health;
    _Atomic uint64_t wm_checks_run;
    _Atomic uint64_t wm_anomalies_detected;
    _Atomic uint64_t wm_recoveries_performed;
    _Atomic uint64_t imagination_checks_run;
    _Atomic uint64_t imagination_anomalies_detected;
    _Atomic uint64_t imagination_recoveries_performed;
    _Atomic float wm_health_score;
    _Atomic float imagination_health_score;
    uint64_t last_wm_check_us;
    uint64_t last_imagination_check_us;
    float jepa_error_history[10];
    uint32_t jepa_error_idx;
    float wm_accuracy_history[10];
    uint32_t wm_accuracy_idx;
    float imagination_coherence_history[10];
    uint32_t imagination_coherence_idx;
} health_agent_system_state_t;

/**
 * @brief Consistency checker state
 */
typedef struct {
    /* Consistency check state */
    health_agent_consistency_result_t last_consistency_result;
    _Atomic bool consistency_check_pending;
    _Atomic uint64_t last_consistency_check_us;
    _Atomic uint64_t consistency_checks_run;
    _Atomic uint64_t consistency_failures_total;

    /* Registered structures for magic validation */
    struct {
        void* ptr;
        uint32_t expected_magic;
        char name[NIMCP_ID_BUFFER_SIZE];
        bool active;
    } registered_structs[64];
    uint32_t registered_struct_count;
} health_agent_consistency_state_t;

/**
 * @brief Main health agent structure (refactored with sub-structures)
 */
struct nimcp_health_agent {
    uint32_t magic;
    uint64_t canary_front;
    uint64_t expected_canary;

    /* Configuration */
    health_agent_config_t config;

    /* Core connections */
    brain_t brain;
    brain_immune_system_t* immune;
    health_monitor_t* monitor;

    /* Thread management */
    nimcp_thread_t agent_thread;
    _Atomic bool running;
    _Atomic bool stop_requested;
    nimcp_mutex_t* state_mutex;
    nimcp_cond_t* stop_cond;

    /* Message queue */
    health_msg_queue_t msg_queue;

    /* Heartbeat state */
    heartbeat_state_t heartbeat;

    /* Statistics */
    _Atomic uint64_t uptime_start_us;
    health_agent_stats_t stats;
    nimcp_mutex_t* stats_mutex;

    /* Anomaly tracking */
    _Atomic uint64_t next_anomaly_id;
    _Atomic health_agent_severity_t current_severity;

    /* Sub-structures (SRP decomposition) */
    health_agent_cognitive_state_t cognitive;
    health_agent_neural_state_t neural;
    health_agent_behavioral_state_t behavioral;
    health_agent_homeostasis_state_t homeostasis;
    health_agent_memory_state_t memory;
    health_agent_swarm_state_t swarm;
    health_agent_system_state_t system;
    health_agent_consistency_state_t consistency;

    /* Sub-structure mutexes */
    nimcp_mutex_t* cognitive_mutex;
    nimcp_mutex_t* neural_mutex;
    nimcp_mutex_t* behavioral_mutex;
    nimcp_mutex_t* consistency_mutex;
    nimcp_mutex_t* capacity_mutex;
    nimcp_mutex_t* logic_mutex;
    nimcp_mutex_t* substrate_mutex;
    nimcp_mutex_t* thalamic_mutex;
    nimcp_mutex_t* middleware_mutex;
    nimcp_mutex_t* perception_mutex;
    nimcp_mutex_t* cortical_mutex;
    nimcp_mutex_t* brain_probe_mutex;
    nimcp_mutex_t* wm_imagination_mutex;
    nimcp_mutex_t* modules_mutex;

    uint64_t canary_back;
};

/* ============================================================================
 * Shared Helper Functions (Used Across Multiple Modules)
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
uint64_t health_agent_get_timestamp_us(void);

/**
 * @brief Generate a randomized canary value for memory protection
 */
uint64_t health_agent_generate_random_canary(void);

/**
 * @brief Validate agent structure integrity
 */
bool health_agent_validate(const nimcp_health_agent_t* agent);

/**
 * @brief Round up to next power of 2
 */
uint32_t health_agent_next_power_of_2(uint32_t value);

/* ============================================================================
 * Message Queue Functions (Used by Core)
 * ============================================================================ */

bool health_agent_msg_queue_init(health_msg_queue_t* queue, uint32_t capacity);
void health_agent_msg_queue_destroy(health_msg_queue_t* queue);
bool health_agent_msg_queue_push(health_msg_queue_t* queue, const health_agent_message_t* msg);
bool health_agent_msg_queue_pop(health_msg_queue_t* queue, health_agent_message_t* msg);
uint32_t health_agent_msg_queue_size(const health_msg_queue_t* queue);

/* ============================================================================
 * Cognitive Module Functions
 * ============================================================================ */

void health_agent_run_failure_prediction(nimcp_health_agent_t* agent);
void health_agent_run_metacognition_check(nimcp_health_agent_t* agent);
void health_agent_run_wellbeing_check(nimcp_health_agent_t* agent);
void health_agent_apply_emotion_adjustments(nimcp_health_agent_t* agent);
void health_agent_check_gpu_health(nimcp_health_agent_t* agent);
bool health_agent_check_ethics_permission(nimcp_health_agent_t* agent,
                                           const health_agent_message_t* msg,
                                           health_agent_recovery_t action);
int health_agent_get_collective_consensus(nimcp_health_agent_t* agent,
                                           const health_agent_message_t* msg);
int health_agent_run_rcog_diagnosis(nimcp_health_agent_t* agent,
                                     const health_agent_message_t* msg,
                                     health_agent_recovery_t* suggested_action);

/* ============================================================================
 * Neural Module Functions
 * ============================================================================ */

void health_agent_run_neural_check(nimcp_health_agent_t* agent);
void health_agent_check_snn_health(nimcp_health_agent_t* agent);
void health_agent_check_lnn_health(nimcp_health_agent_t* agent);
void health_agent_update_neural_metrics(nimcp_health_agent_t* agent);

/* ============================================================================
 * Behavioral Module Functions
 * ============================================================================ */

void health_agent_run_behavioral_check(nimcp_health_agent_t* agent);
void health_agent_check_dragonfly_immune(nimcp_health_agent_t* agent);
void health_agent_check_portia_monitor(nimcp_health_agent_t* agent);
void health_agent_update_behavioral_metrics(nimcp_health_agent_t* agent);
void health_agent_run_cross_module_coordination(nimcp_health_agent_t* agent);

/* ============================================================================
 * Homeostasis Module Functions
 * ============================================================================ */

void health_agent_run_hypothalamus_check(nimcp_health_agent_t* agent, float health_score);
void health_agent_run_homeostatic_regulation(nimcp_health_agent_t* agent, float health_score);
int health_agent_hypo_drive_event_callback(const void* event, void* user_data);

/* ============================================================================
 * Memory Module Functions
 * ============================================================================ */

void health_agent_check_hippocampus(nimcp_health_agent_t* agent);
void health_agent_check_mammillary(nimcp_health_agent_t* agent);
void health_agent_check_engram(nimcp_health_agent_t* agent);
void health_agent_check_memory_consolidation(nimcp_health_agent_t* agent);
void health_agent_check_memory_store(nimcp_health_agent_t* agent);
void health_agent_check_ood_detector(nimcp_health_agent_t* agent);

/* ============================================================================
 * Swarm Module Functions
 * ============================================================================ */

void health_agent_check_swarm_immune(nimcp_health_agent_t* agent);
void health_agent_check_swarm_memory(nimcp_health_agent_t* agent);

/* ============================================================================
 * System Module Functions
 * ============================================================================ */

void health_agent_check_connectivity(nimcp_health_agent_t* agent);
void health_agent_check_oscillations(nimcp_health_agent_t* agent);
void health_agent_auto_gc_if_needed(nimcp_health_agent_t* agent);
void health_agent_auto_checkpoint_if_needed(nimcp_health_agent_t* agent, float health_score);
void health_agent_check_capacity_managers(nimcp_health_agent_t* agent);
void health_agent_check_symbolic_logic(nimcp_health_agent_t* agent);
void health_agent_check_neural_substrates(nimcp_health_agent_t* agent);
void health_agent_check_thalamic_bridges(nimcp_health_agent_t* agent);
void health_agent_check_middleware(nimcp_health_agent_t* agent);
void health_agent_check_perception(nimcp_health_agent_t* agent);
void health_agent_check_cortical(nimcp_health_agent_t* agent);
void health_agent_check_brain_probes(nimcp_health_agent_t* agent);
void health_agent_check_world_model(nimcp_health_agent_t* agent);
void health_agent_check_imagination(nimcp_health_agent_t* agent);

/* ============================================================================
 * Consistency Module Functions
 * ============================================================================ */

void health_agent_run_consistency_checks(nimcp_health_agent_t* agent);
bool health_agent_check_reference_counts(nimcp_health_agent_t* agent,
                                          health_agent_consistency_result_t* result);
bool health_agent_check_pointer_canaries(nimcp_health_agent_t* agent,
                                          health_agent_consistency_result_t* result);
bool health_agent_check_struct_magic(nimcp_health_agent_t* agent,
                                      health_agent_consistency_result_t* result);
bool health_agent_check_mutex_state(nimcp_health_agent_t* agent,
                                     health_agent_consistency_result_t* result);
bool health_agent_check_circular_buffers(nimcp_health_agent_t* agent,
                                          health_agent_consistency_result_t* result);
bool health_agent_check_knowledge_graph(nimcp_health_agent_t* agent,
                                         health_agent_consistency_result_t* result);
bool health_agent_check_neuron_values(nimcp_health_agent_t* agent,
                                       health_agent_consistency_result_t* result);

/* ============================================================================
 * Core Module Functions (Thread Main, Heartbeat, etc.)
 * ============================================================================ */

void* health_agent_thread_main(void* arg);
void health_agent_update_heartbeat(nimcp_health_agent_t* agent);
void health_agent_check_heartbeat_timeout(nimcp_health_agent_t* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEALTH_AGENT_INTERNAL_H */
