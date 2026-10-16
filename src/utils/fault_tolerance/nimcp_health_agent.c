/**
 * @file nimcp_health_agent.c
 * @brief Independent Health Monitor Agent Implementation
 * @version 1.0.0
 * @date 2025-01-17
 *
 * WHAT: Autonomous agent that monitors brain health independently
 * WHY:  Detect and respond to errors even when main system is compromised
 * HOW:  Separate thread with watchdog, lock-free communication to immune system
 */

/* Must be before any includes for pthread_timedjoin_np */
#define _GNU_SOURCE

#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/fault_tolerance/nimcp_lockfree_metrics.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_deadlock_detector.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "constants/nimcp_buffer_constants.h"

/* Module integrations */
#include "portia/nimcp_portia.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "swarm/nimcp_swarm_immune.h"
#include "swarm/nimcp_swarm_memory.h"

/* Phase 5.6: Behavioral module (Dragonfly/Portia) immune integration */
#include "dragonfly/nimcp_dragonfly_immune_bridge.h"
#include "portia/nimcp_portia_monitoring.h"

/* Phase 5.5: Neural module (SNN/LNN) immune integration */
#include "snn/nimcp_snn_immune.h"
#include "lnn/nimcp_lnn_immune.h"

/* Phase 4: Hypothalamus integration */
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_immune_bridge.h"

/* Phase 4: Checkpoint and recovery */
#include "utils/fault_tolerance/nimcp_checkpoint.h"

/* Phase 4: Brain oscillations and connectivity */
#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "cognitive/introspection/nimcp_connectivity_health.h"

/* Phase 4: Bio-async router */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Phase 4: Additional fault tolerance includes */
#include "core/brain/nimcp_kg_gc.h"
#include "utils/fault_tolerance/nimcp_runtime_adaptation.h"

/* Phase 7 (Section 26): GPU health monitoring */
#include "utils/gpu/nimcp_gpu_health.h"

/* Phase 5.8: Dynamic Capacity Management */
#include "utils/fault_tolerance/nimcp_capacity_manager.h"

/* Phase 5.9: Symbolic Logic Health Integration */
#include "cognitive/nimcp_symbolic_logic.h"

/* Phase 5.10: Neural Substrate Health Integration */
#include "core/neural_substrate/nimcp_neural_substrate.h"

/* Phase 5.11: Thalamic/Middleware Health Integration */
#include "cognitive/omni/bridges/nimcp_omni_wm_thalamic_bridge.h"
#include "middleware/training/nimcp_brain_training_integration.h"

/* Phase 5.12: Perception/Cortical Health Integration */
#include "perception/cortical/nimcp_visual_cortical_bridge.h"
#include "perception/cortical/nimcp_audio_cortical_bridge.h"
#include "core/cortical_columns/nimcp_cortical_immune.h"
#include "core/cortical_columns/nimcp_cortical_column.h"

/* Brain Immune Tick Integration */
#include "cognitive/immune/nimcp_brain_immune_tick.h"
#include "constants/nimcp_timing_constants.h"
#include "constants/nimcp_learning_constants.h"

/* Memory Store + OOD Detector health monitoring */
#include "memory/nimcp_memory_store.h"
#include "cognitive/nimcp_ood_detector.h"

/* Phase 5: Cognitive module integration for real API calls
 * Note: Cannot include full headers due to type conflicts (metric_type_t, cognitive_state_t)
 * Forward-declare the specific functions we need from the cognitive modules */

/* Forward declarations for failure_prediction.h functions */
extern uint32_t failure_predictor_get_prediction_count(failure_predictor_t* predictor);
extern bool failure_predictor_needs_prevention(failure_predictor_t* predictor);

/* Forward declarations for metacognition.h functions */
extern bool metacognition_is_degraded(metacognition_t* meta, float threshold);
extern float metacognition_get_self_confidence(const metacognition_t* meta);
extern float metacognition_get_uncertainty(const metacognition_t* meta);
extern bool metacognition_has_high_uncertainty(const metacognition_t* meta, float threshold);

/* Forward declarations for ethics.h functions (avoiding full header) */
typedef enum {
    ETHICS_EVAL_APPROVED = 0,
    ETHICS_EVAL_BLOCKED = 1,
    ETHICS_EVAL_CONDITIONAL = 2,
    ETHICS_EVAL_ERROR = -1
} ethics_eval_result_t;

/* Forward declarations for collective.h functions */
typedef enum {
    COLLECTIVE_CONSENSUS_NONE = 0,
    COLLECTIVE_CONSENSUS_ACHIEVED = 1,
    COLLECTIVE_CONSENSUS_TIMEOUT = 2,
    COLLECTIVE_CONSENSUS_ERROR = -1
} collective_consensus_result_t;

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

typedef enum {
    COLLECTIVE_CONSCIOUSNESS_NONE = 0,
    COLLECTIVE_CONSCIOUSNESS_MINIMAL,
    COLLECTIVE_CONSCIOUSNESS_EMERGING,
    COLLECTIVE_CONSCIOUSNESS_PARTIAL,
    COLLECTIVE_CONSCIOUSNESS_UNIFIED,
    COLLECTIVE_CONSCIOUSNESS_TRANSCENDENT
} health_agent_consciousness_level_t;

extern int collective_cognition_get_we_mode(const collective_cognition_t* cc, health_agent_we_mode_state_t* state);
extern int collective_cognition_get_phi(const collective_cognition_t* cc, health_agent_collective_phi_t* phi);
extern health_agent_consciousness_level_t collective_cognition_get_consciousness_level(const collective_cognition_t* cc);
extern uint32_t collective_cognition_instance_count(const collective_cognition_t* cc);
extern bool collective_cognition_is_bio_async_connected(const collective_cognition_t* cc);

/* RCOG engine forward declarations (avoiding header includes) */
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

extern health_agent_rcog_state_t rcog_engine_get_state(const rcog_engine_t* engine);
extern int rcog_engine_get_stats(const rcog_engine_t* engine, health_agent_rcog_stats_t* stats);
extern bool rcog_engine_is_ready(const rcog_engine_t* engine);
extern bool rcog_engine_has_capacity(const rcog_engine_t* engine);
extern int rcog_engine_enter_degraded_mode(rcog_engine_t* engine);
extern int rcog_engine_exit_degraded_mode(rcog_engine_t* engine);

/* Hypothalamus orchestrator type aliases
 * Use the actual types from the included hypothalamus headers */
typedef hypo_unified_drive_state_t health_agent_drive_state_t;
typedef hypo_orch_stats_t health_agent_hypo_stats_t;

#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>  /* Still needed for pthread_timedjoin_np */
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#include "constants/nimcp_buffer_constants.h"
#endif

#define LOG_MODULE "health_agent"

/* NaN/Inf detection macro (local definition to avoid circular deps) */
#ifndef NIMCP_IS_INVALID_FLOAT
#define NIMCP_IS_INVALID_FLOAT(x) (isnan(x) || isinf(x))
#endif

/* Helper macro to check if any consistency check is enabled */
#define CONSISTENCY_CHECKS_ENABLED(cfg) \
    ((cfg).check_reference_counts || (cfg).check_pointer_canaries || \
     (cfg).check_struct_magic || (cfg).check_mutex_state || \
     (cfg).check_circular_buffers || (cfg).check_kg_consistency || \
     (cfg).check_neuron_values)

/* ============================================================================
 * Internal Constants
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
 * @brief Internal health agent structure
 */
struct nimcp_health_agent {
    uint32_t magic;                      /**< Magic number for validation */
    uint64_t canary_front;               /**< Front canary for corruption detection */
    uint64_t expected_canary;            /**< Randomized expected canary value */

    /* Configuration */
    health_agent_config_t config;

    /* Core connections */
    brain_t brain;                       /**< Connected brain */
    brain_immune_system_t* immune;       /**< Connected immune system */
    health_monitor_t* monitor;           /**< Connected health monitor */

    /* Thread management - using NIMCP threading utilities */
    nimcp_thread_t agent_thread;
    _Atomic bool running;
    _Atomic bool stop_requested;
    nimcp_mutex_t* state_mutex;          /**< For state transitions (NIMCP mutex) */
    nimcp_cond_t* stop_cond;             /**< Condition for stop signaling */

    /* Message queue (agent -> immune) */
    health_msg_queue_t msg_queue;

    /* Heartbeat state */
    heartbeat_state_t heartbeat;

    /* Statistics */
    _Atomic uint64_t uptime_start_us;
    health_agent_stats_t stats;
    nimcp_mutex_t* stats_mutex;          /**< For stats update (NIMCP mutex) */

    /* Anomaly tracking */
    _Atomic uint64_t next_anomaly_id;
    _Atomic health_agent_severity_t current_severity;

    /* =========== COGNITIVE MODULE CONNECTIONS =========== */

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

    /* Cognitive stats mutex */
    nimcp_mutex_t* cognitive_mutex;      /**< For cognitive stats (NIMCP mutex) */

    /* =========== HYPOTHALAMUS & HOMEOSTASIS CONNECTIONS =========== */

    /* Hypothalamus orchestrator - central drive coordination */
    hypo_orchestrator_t hypothalamus;
    health_agent_hypothalamus_config_t hypothalamus_config;
    uint32_t hypo_bridge_id;              /**< Registered bridge ID */
    _Atomic bool in_stress_response;
    _Atomic bool in_sickness_mode;
    _Atomic uint64_t stress_responses;
    _Atomic uint64_t sickness_mode_entries;
    _Atomic uint64_t drive_events_published;

    /* Hypothalamus homeostasis system - PID-based regulation */
    hypo_homeostasis_handle_t* homeostasis;
    _Atomic float homeostatic_output;

    /* Hypothalamus immune bridge - bidirectional neuroimmune integration */
    hypo_immune_bridge_t* hypo_immune_bridge;

    /* Hypothalamus drive system */
    hypo_drive_system_handle_t* drives;

    /* =========== ADDITIONAL MODULE CONNECTIONS =========== */

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

    /* Deadlock detector (direct connection) */
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

    /* =========== NEURAL MODULE (SNN/LNN) CONNECTIONS =========== */

    /* SNN immune bridge - spiking neural network health monitoring */
    snn_immune_bridge_t* snn_bridge;
    health_agent_snn_config_t snn_config;
    _Atomic uint64_t snn_checks_run;
    _Atomic uint64_t snn_instabilities_detected;
    _Atomic uint64_t snn_recoveries_triggered;
    uint64_t last_snn_check_us;

    /* LNN immune bridge - liquid neural network health monitoring */
    lnn_immune_bridge_t* lnn_bridge;
    health_agent_lnn_config_t lnn_config;
    _Atomic uint64_t lnn_checks_run;
    _Atomic uint64_t lnn_instabilities_detected;
    _Atomic uint64_t lnn_recoveries_triggered;
    uint64_t last_lnn_check_us;

    /* Combined neural health metrics (cached for fast access) */
    neural_health_metrics_t neural_metrics;
    nimcp_mutex_t* neural_mutex;         /**< For neural metrics access */

    /* =========== PORTIA/DRAGONFLY/SWARM/MEMORY CONNECTIONS =========== */

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

    /* =========== BEHAVIORAL MODULE (DRAGONFLY/PORTIA) IMMUNE INTEGRATION (Phase 5.6) =========== */

    /* Dragonfly immune bridge - hunting behavior health monitoring */
    dragonfly_immune_bridge_t dragonfly_immune;
    health_agent_dragonfly_immune_config_t dragonfly_immune_config;
    _Atomic uint64_t dragonfly_immune_checks_run;
    _Atomic uint64_t dragonfly_stress_events;
    _Atomic uint64_t dragonfly_injury_events;
    _Atomic uint64_t dragonfly_rest_triggers;
    uint64_t last_dragonfly_immune_check_us;

    /* Portia monitor - platform resource health monitoring */
    portia_monitor_t portia_monitor;
    health_agent_portia_monitor_config_t portia_monitor_config;
    _Atomic uint64_t portia_monitor_checks_run;
    _Atomic uint64_t portia_thermal_warnings;
    _Atomic uint64_t portia_power_warnings;
    _Atomic uint64_t portia_coordination_actions;
    uint64_t last_portia_monitor_check_us;

    /* Combined behavioral health metrics (cached for fast access) */
    behavioral_health_metrics_t behavioral_metrics;
    nimcp_mutex_t* behavioral_mutex;     /**< For behavioral metrics access */

    /* Cross-module coordination state */
    _Atomic bool thermal_abort_active;
    _Atomic bool power_conservation_active;
    _Atomic bool rest_period_active;
    _Atomic uint64_t last_coordination_us;

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

    /* Engram memory system */
    engram_system_t* engram;
    health_agent_engram_config_t engram_config;
    _Atomic uint64_t engram_encodings;
    _Atomic uint64_t engram_recalls;

    /* Systems consolidation */
    systems_consolidation_system_t* memory_consolidation;
    health_agent_memory_consolidation_config_t consolidation_config;

    /* =========== MEMORY SYSTEM HEALTH (Phase 5.7) =========== */

    /* Hippocampus - episodic memory, spatial navigation, theta-gamma rhythms */
    nimcp_hippocampus_t* hippocampus;
    health_agent_hippocampus_config_t hippocampus_config;
    _Atomic bool hippocampus_connected;
    _Atomic uint64_t hippocampus_checks_run;
    _Atomic uint64_t hippocampus_anomalies;
    _Atomic uint64_t hippocampus_recoveries;
    uint64_t last_hippocampus_check_us;

    /* Mammillary bodies - memory consolidation relay, head direction cells */
    nimcp_mammillary_t* mammillary;
    health_agent_mammillary_config_t mammillary_config;
    _Atomic bool mammillary_connected;
    _Atomic uint64_t mammillary_checks_run;
    _Atomic uint64_t mammillary_anomalies;
    _Atomic uint64_t mammillary_recoveries;
    uint64_t last_mammillary_check_us;

    /* Memory system cross-tier tracking */
    _Atomic uint64_t memory_consistency_checks;
    _Atomic uint64_t memory_tier_mismatches;
    _Atomic uint64_t memory_recoveries_total;

    /* =========== CAPACITY MANAGEMENT (Phase 5.8) =========== */

    /* Registered capacity managers */
    capacity_manager_t* capacity_managers[HEALTH_AGENT_MAX_CAPACITY_MANAGERS];
    _Atomic uint32_t num_capacity_managers;
    nimcp_mutex_t* capacity_mutex;       /**< For capacity manager registration */

    /* Capacity tracking */
    _Atomic uint64_t capacity_checks_run;
    _Atomic uint64_t capacity_warnings_triggered;
    _Atomic uint64_t capacity_expansions_triggered;
    char most_critical_module[NIMCP_ID_BUFFER_SIZE];       /**< Name of module closest to capacity */

    /* =========== SYMBOLIC LOGIC HEALTH (Phase 5.9) =========== */

    /* Registered symbolic logic engines */
    symbolic_logic_t* logic_engines[HEALTH_AGENT_MAX_LOGIC_ENGINES];
    _Atomic uint32_t num_logic_engines;
    nimcp_mutex_t* logic_mutex;          /**< For logic engine registration */

    /* Symbolic logic health configuration */
    health_agent_symbolic_logic_config_t logic_config;

    /* Logic health tracking */
    _Atomic uint64_t logic_checks_run;
    _Atomic uint64_t logic_anomalies_detected;
    _Atomic uint64_t logic_recoveries_performed;
    _Atomic uint64_t logic_loop_detections;
    _Atomic uint64_t logic_kb_corruptions;
    _Atomic float logic_health_score;    /**< Current logic health score [0-100] */

    /* =========== NEURAL SUBSTRATE HEALTH (Phase 5.10) =========== */

    /* Registered neural substrates */
    neural_substrate_t* substrates[HEALTH_AGENT_MAX_NEURAL_SUBSTRATES];
    _Atomic uint32_t num_substrates;
    nimcp_mutex_t* substrate_mutex;      /**< For substrate registration */

    /* Neural substrate health configuration */
    health_agent_substrate_config_t substrate_config;

    /* Substrate health tracking */
    _Atomic uint64_t substrate_checks_run;
    _Atomic uint64_t substrate_anomalies_detected;
    _Atomic uint64_t substrate_recoveries_performed;
    _Atomic uint64_t substrate_critical_events;
    _Atomic float substrate_health_score; /**< Current substrate health score [0-100] */

    /* =========== THALAMIC/MIDDLEWARE HEALTH (Phase 5.11) =========== */

    /* Registered thalamic bridges */
    omni_wm_thalamic_bridge_t* thalamic_bridges[HEALTH_AGENT_MAX_THALAMIC_BRIDGES];
    _Atomic uint32_t num_thalamic_bridges;
    nimcp_mutex_t* thalamic_mutex;        /**< For thalamic bridge registration */

    /* Thalamic health configuration */
    health_agent_thalamic_config_t thalamic_config;

    /* Thalamic health tracking */
    _Atomic uint64_t thalamic_checks_run;
    _Atomic uint64_t thalamic_anomalies_detected;
    _Atomic uint64_t thalamic_recoveries_performed;
    _Atomic uint64_t thalamic_critical_events;
    _Atomic float thalamic_health_score;  /**< Current thalamic health score [0-100] */

    /* Registered middleware training contexts */
    nimcp_brain_training_ctx_t* training_contexts[HEALTH_AGENT_MAX_TRAINING_CONTEXTS];
    _Atomic uint32_t num_training_contexts;
    nimcp_mutex_t* middleware_mutex;      /**< For training context registration */

    /* Middleware health configuration */
    health_agent_middleware_config_t middleware_config;

    /* Middleware health tracking */
    _Atomic uint64_t middleware_checks_run;
    _Atomic uint64_t middleware_anomalies_detected;
    _Atomic uint64_t middleware_recoveries_performed;
    _Atomic uint64_t middleware_critical_events;
    _Atomic float middleware_health_score; /**< Current middleware health score [0-100] */

    /* =========== PERCEPTION/CORTICAL HEALTH (Phase 5.12) =========== */

    /* Registered visual cortical bridges */
    visual_cortical_bridge_t* visual_bridges[HEALTH_AGENT_MAX_PERCEPTION_BRIDGES];
    _Atomic uint32_t num_visual_bridges;

    /* Registered audio cortical bridges */
    audio_cortical_bridge_t* audio_bridges[HEALTH_AGENT_MAX_PERCEPTION_BRIDGES];
    _Atomic uint32_t num_audio_bridges;

    /* Perception health configuration and mutex */
    nimcp_mutex_t* perception_mutex;      /**< For perception bridge registration */
    health_agent_perception_config_t perception_config;

    /* Perception health tracking */
    _Atomic uint64_t perception_checks_run;
    _Atomic uint64_t perception_anomalies_detected;
    _Atomic uint64_t perception_recoveries_performed;
    _Atomic uint64_t perception_critical_events;
    _Atomic float perception_health_score; /**< Current perception health score [0-100] */

    /* Registered cortical immune systems */
    cortical_immune_system_t* cortical_immune_systems[HEALTH_AGENT_MAX_PERCEPTION_BRIDGES];
    _Atomic uint32_t num_cortical_immune_systems;

    /* Registered cortical columns (hypercolumns) */
    hypercolumn_t* cortical_columns[HEALTH_AGENT_MAX_CORTICAL_COLUMNS];
    _Atomic uint32_t num_cortical_columns;

    /* Cortical health configuration and mutex */
    nimcp_mutex_t* cortical_mutex;        /**< For cortical column/immune registration */
    health_agent_cortical_config_t cortical_config;

    /* Cortical health tracking */
    _Atomic uint64_t cortical_checks_run;
    _Atomic uint64_t cortical_anomalies_detected;
    _Atomic uint64_t cortical_recoveries_performed;
    _Atomic uint64_t cortical_critical_events;
    _Atomic float cortical_health_score;  /**< Current cortical health score [0-100] */

    /* =========== BRAIN PROBE HEALTH MONITORING (Phase 5.13) =========== */

    /* Registered brains for probe monitoring */
    brain_t monitored_brains[HEALTH_AGENT_MAX_BRAINS];
    brain_probe_health_metrics_t brain_metrics[HEALTH_AGENT_MAX_BRAINS];
    health_agent_brain_probe_config_t brain_probe_configs[HEALTH_AGENT_MAX_BRAINS];
    _Atomic uint32_t num_monitored_brains;
    nimcp_mutex_t* brain_probe_mutex;     /**< For brain registration */

    /* Default brain probe configuration */
    health_agent_brain_probe_config_t brain_probe_config;

    /* Brain probe health tracking */
    _Atomic uint64_t brain_probes_run;
    _Atomic uint64_t brain_warnings_triggered;
    _Atomic uint64_t brain_critical_events;
    _Atomic uint64_t brain_recoveries_performed;
    _Atomic float brain_probe_health_score; /**< Current brain probe health score [0-100] */
    uint64_t last_brain_probe_us;         /**< Timestamp of last brain probe */

    /* =========== STATE CONSISTENCY MANAGER (Phase 3) =========== */

    /* Consistency check state */
    health_agent_consistency_result_t last_consistency_result;
    _Atomic bool consistency_check_pending;
    _Atomic uint64_t last_consistency_check_us;
    _Atomic uint64_t consistency_checks_run;
    _Atomic uint64_t consistency_failures_total;
    nimcp_mutex_t* consistency_mutex;    /**< For consistency check state */

    /* Registered structures for magic validation */
    struct {
        void* ptr;                       /**< Pointer to structure */
        uint32_t expected_magic;         /**< Expected magic value */
        char name[NIMCP_ID_BUFFER_SIZE];                   /**< Structure name */
        bool active;                     /**< Entry is in use */
    } registered_structs[64];            /**< Registry of tracked structures */
    uint32_t registered_struct_count;    /**< Number of registered structs */

    /* Module connection mutex */
    nimcp_mutex_t* modules_mutex;        /**< For module connections (NIMCP mutex) */

    /* =========== WORLD MODEL & IMAGINATION HEALTH (Phase 5.14) =========== */

    /* Connected world model and imagination components */
    jepa_predictor_t* jepa_predictor;     /**< JEPA predictor for latent space prediction */
    omni_world_model_t* world_model;      /**< Omni world model for dynamics */
    imagination_engine_t* imagination;    /**< Imagination engine for mental simulation */
    nimcp_mutex_t* wm_imagination_mutex;  /**< For world model/imagination state */

    /* Configuration */
    health_agent_wm_imagination_config_t wm_imagination_config;

    /* Health metrics caches */
    jepa_health_metrics_t jepa_metrics;
    omni_wm_health_metrics_t wm_metrics;
    imagination_health_metrics_t imagination_metrics;
    world_imagination_health_t combined_wm_health;

    /* Health tracking */
    _Atomic uint64_t wm_checks_run;
    _Atomic uint64_t wm_anomalies_detected;
    _Atomic uint64_t wm_recoveries_performed;
    _Atomic uint64_t imagination_checks_run;
    _Atomic uint64_t imagination_anomalies_detected;
    _Atomic uint64_t imagination_recoveries_performed;
    _Atomic float wm_health_score;        /**< Current world model health score [0-1] */
    _Atomic float imagination_health_score; /**< Current imagination health score [0-1] */
    uint64_t last_wm_check_us;            /**< Timestamp of last world model check */
    uint64_t last_imagination_check_us;   /**< Timestamp of last imagination check */

    /* Trend tracking buffers (ring buffers for trend analysis) */
    float jepa_error_history[10];         /**< Prediction error history */
    uint32_t jepa_error_idx;
    float wm_accuracy_history[10];        /**< Forward accuracy history */
    uint32_t wm_accuracy_idx;
    float imagination_coherence_history[10]; /**< Scene coherence history */
    uint32_t imagination_coherence_idx;

    uint64_t canary_back;                /**< Back canary for corruption detection */
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void* agent_thread_main(void* arg);
static bool msg_queue_init(health_msg_queue_t* queue, uint32_t capacity);
static void msg_queue_destroy(health_msg_queue_t* queue);
static bool msg_queue_push(health_msg_queue_t* queue, const health_agent_message_t* msg);
static bool msg_queue_pop(health_msg_queue_t* queue, health_agent_message_t* msg);
static uint32_t msg_queue_size(const health_msg_queue_t* queue);
static uint64_t get_timestamp_us(void);

/* Cognitive integration helper functions */
static void agent_run_failure_prediction(nimcp_health_agent_t* agent);
static void agent_run_metacognition_check(nimcp_health_agent_t* agent);
static void agent_run_wellbeing_check(nimcp_health_agent_t* agent);
static void agent_apply_emotion_adjustments(nimcp_health_agent_t* agent);
static void agent_check_gpu_health(nimcp_health_agent_t* agent);
static void agent_run_neural_check(nimcp_health_agent_t* agent);
static void agent_check_snn_health(nimcp_health_agent_t* agent);
static void agent_check_lnn_health(nimcp_health_agent_t* agent);
static void agent_update_neural_metrics(nimcp_health_agent_t* agent);
static void agent_run_behavioral_check(nimcp_health_agent_t* agent);
static void agent_check_dragonfly_immune(nimcp_health_agent_t* agent);
static void agent_check_portia_monitor(nimcp_health_agent_t* agent);
static void agent_update_behavioral_metrics(nimcp_health_agent_t* agent);
static void agent_run_cross_module_coordination(nimcp_health_agent_t* agent);
static bool agent_check_ethics_permission(nimcp_health_agent_t* agent,
                                           const health_agent_message_t* msg,
                                           health_agent_recovery_t action);
static int agent_get_collective_consensus(nimcp_health_agent_t* agent,
                                           const health_agent_message_t* msg);
static int agent_run_rcog_diagnosis(nimcp_health_agent_t* agent,
                                     const health_agent_message_t* msg,
                                     health_agent_recovery_t* suggested_action);

/* Hypothalamus and module integration helper functions */
static void agent_run_hypothalamus_check(nimcp_health_agent_t* agent, float health_score);
static void agent_run_homeostatic_regulation(nimcp_health_agent_t* agent, float health_score);
static void agent_check_connectivity(nimcp_health_agent_t* agent);
static void agent_check_oscillations(nimcp_health_agent_t* agent);
static void agent_auto_gc_if_needed(nimcp_health_agent_t* agent);
static void agent_auto_checkpoint_if_needed(nimcp_health_agent_t* agent, float health_score);
static int hypo_drive_event_callback(const void* event, void* user_data);

/* State Consistency Manager helper functions (Phase 3) */
static void agent_run_consistency_checks(nimcp_health_agent_t* agent);
static bool agent_check_reference_counts(nimcp_health_agent_t* agent, health_agent_consistency_result_t* result);
static bool agent_check_pointer_canaries(nimcp_health_agent_t* agent, health_agent_consistency_result_t* result);
static bool agent_check_struct_magic(nimcp_health_agent_t* agent, health_agent_consistency_result_t* result);
static bool agent_check_mutex_state(nimcp_health_agent_t* agent, health_agent_consistency_result_t* result);
static bool agent_check_circular_buffers(nimcp_health_agent_t* agent, health_agent_consistency_result_t* result);
static bool agent_check_knowledge_graph(nimcp_health_agent_t* agent, health_agent_consistency_result_t* result);
static bool agent_check_neuron_values(nimcp_health_agent_t* agent, health_agent_consistency_result_t* result);

/* ============================================================================
 * Phase 5.7: Memory System Health Integration
 * ============================================================================ */

/* Forward declarations for hippocampus/mammillary health functions */
extern float hippo_get_health_status(nimcp_hippocampus_t* hippo);
extern float mammillary_get_health_status(nimcp_mammillary_t* mb);


/* -------------------------------------------------------------------------
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



// Forward declarations for static functions (SRP split)
static uint64_t generate_random_canary(void);
static bool validate_agent(const nimcp_health_agent_t* agent);
static uint32_t next_power_of_2(uint32_t value);
static void agent_probe_single_brain( nimcp_health_agent_t* agent, uint32_t index );

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_health_agent_part_helpers.c"  // 27 functions: helpers
#include "nimcp_health_agent_part_io.c"  // 6 functions: io
#include "nimcp_health_agent_part_lifecycle.c"  // 6 functions: lifecycle
#include "nimcp_health_agent_part_accessors.c"  // 59 functions: accessors
#include "nimcp_health_agent_part_core.c"  // 126 functions: core
#include "nimcp_health_agent_part_stats.c"  // 3 functions: stats
#include "nimcp_health_agent_part_processing.c"  // 24 functions: processing
