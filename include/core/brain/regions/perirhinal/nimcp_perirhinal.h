/**
 * @file nimcp_perirhinal.h
 * @brief Perirhinal Cortex - Object Recognition and Familiarity Memory
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * WHAT: Perirhinal cortex implementation with object identity representation,
 *       familiarity-based recognition, and novelty detection.
 *
 * WHY:  The perirhinal cortex is critical for object recognition memory and
 *       provides the "what" pathway to the medial temporal lobe memory system.
 *
 * HOW:  Implements visual object identity encoding, familiarity signals,
 *       novelty detection, and bidirectional integration with entorhinal cortex.
 *
 * BIOLOGICAL BASIS:
 * - Object cells: View-invariant object representations
 * - Familiarity neurons: Respond based on prior exposure
 * - Novelty detectors: Signal new/unexpected objects
 * - Recency signals: Encode time since last encounter
 *
 * FULL BIDIRECTIONAL INTEGRATION:
 * - Security Module: Access control, threat detection, data validation
 * - Immune System: Anomaly detection, self-healing, pathogen response
 * - Bio-Async System: Neuromodulator channels (DA/5-HT/NE/ACh), async messaging
 * - Brain Factory/KG: Self-awareness, component registration, health monitoring
 * - Logging Module: Full audit trail, metrics, diagnostics
 * - Prime Resonance: Oscillatory coupling, phase synchronization
 * - Cognitive Layer: Working memory, attention, reasoning integration
 * - Training Layer: Supervised/unsupervised learning, backprop adapters
 * - Logic System: Symbolic reasoning, constraint satisfaction
 * - Neural Substrate: Metabolic state, ATP/O2/glucose effects
 * - Thalamic Layer: Relay routing, attention gating
 * - Perception Layer: Visual input processing, feature extraction
 * - Swarm System: Distributed coordination, consensus
 * - Dragonfly System: Fast visual processing, interception
 * - Portia System: Planning, deception detection
 * - Cerebellum: Motor timing, prediction error
 * - Medulla Oblongata: Vital function coordination
 * - Omnidirectional System: 360-degree spatial awareness
 * - Hypothalamus: Homeostatic regulation, motivation
 * - SNN Module: Spiking neural network integration
 * - Plasticity Module: Synaptic plasticity coordination
 * - STDP Module: Spike-timing dependent plasticity
 * - Entorhinal Cortex: Spatial context, memory gateway
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PERIRHINAL_H
#define NIMCP_PERIRHINAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * FORWARD DECLARATIONS - ALL INTEGRATED MODULES
 * Note: Only define if not already defined by actual headers
 *===========================================================================*/

/* Core brain systems - use actual header defines if available */
#ifndef NIMCP_H
typedef struct nimcp_brain_handle* nimcp_brain_t;
#endif
typedef struct nimcp_brain_kg nimcp_brain_kg_t;
typedef struct brain_immune_system brain_immune_system_t;

/* Bio-async communication */
#ifndef NIMCP_BIO_ASYNC_H
typedef struct nimcp_bio_future_struct* nimcp_bio_future_t;
#endif
#ifndef NIMCP_BIO_ROUTER_H
typedef struct bio_router_struct* bio_router_t;
typedef struct bio_module_context_struct* bio_module_context_t;
#endif
/* Handler - perirhinal-specific opaque type */
typedef struct nimcp_bio_async_handler nimcp_bio_async_handler_t;
typedef bio_router_t nimcp_bio_router_t;  /* Alias for consistency */

/* Security */
typedef struct nimcp_security_context nimcp_security_context_t;
typedef struct nimcp_access_control nimcp_access_control_t;

/* Logging and metrics - use actual header defines if available */
#ifndef NIMCP_LOGGING_H
typedef struct nimcp_logger_struct* nimcp_logger_t;
#endif
typedef struct nimcp_metrics nimcp_metrics_t;

/* Neural systems */
typedef struct nimcp_snn_network nimcp_snn_network_t;
typedef struct nimcp_plasticity_manager nimcp_plasticity_manager_t;
typedef struct nimcp_stdp_rule nimcp_stdp_rule_t;

/* Cognitive systems */
typedef struct working_memory working_memory_t;
typedef struct attention_system attention_system_t;
typedef struct cognitive_integration_hub cognitive_integration_hub_t;

/* Training */
typedef struct nimcp_training_context nimcp_training_context_t;
typedef struct nimcp_backprop_adapter nimcp_backprop_adapter_t;

/* Other brain regions */
typedef struct nimcp_entorhinal nimcp_entorhinal_t;
typedef struct hippocampus_adapter hippocampus_adapter_t;
typedef struct thalamus_adapter thalamus_adapter_t;
typedef struct hypothalamus_adapter hypothalamus_adapter_t;
typedef struct cerebellum_adapter cerebellum_adapter_t;
typedef struct medulla_adapter medulla_adapter_t;

/* Specialized systems */
typedef struct nimcp_prime_resonance nimcp_prime_resonance_t;
typedef struct nimcp_swarm_coordinator nimcp_swarm_coordinator_t;
typedef struct nimcp_dragonfly_system nimcp_dragonfly_system_t;
typedef struct nimcp_portia_system nimcp_portia_system_t;
typedef struct nimcp_omnidirectional_system nimcp_omnidirectional_system_t;

/* Substrate and layers */
typedef struct nimcp_neural_substrate nimcp_neural_substrate_t;
typedef struct nimcp_perception_layer nimcp_perception_layer_t;
typedef struct nimcp_logic_system nimcp_logic_system_t;

/*=============================================================================
 * CONFIGURATION CONSTANTS
 *===========================================================================*/

#define PERIRHINAL_DEFAULT_OBJECT_CELLS         512
#define PERIRHINAL_DEFAULT_FAMILIARITY_CELLS    256
#define PERIRHINAL_DEFAULT_NOVELTY_CELLS        128
#define PERIRHINAL_DEFAULT_RECENCY_CELLS        64
#define PERIRHINAL_DEFAULT_FEATURE_DIM          256
#define PERIRHINAL_DEFAULT_OBJECT_DIM           128
#define PERIRHINAL_DEFAULT_MAX_STORED_OBJECTS   1024
#define PERIRHINAL_DEFAULT_VIEW_INVARIANCE      0.8f
#define PERIRHINAL_FAMILIARITY_THRESHOLD        0.5f
#define PERIRHINAL_NOVELTY_THRESHOLD            0.3f
#define PERIRHINAL_RECENCY_DECAY_RATE           0.01f

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Perirhinal cortex processing status
 */
typedef enum {
    PERIRHINAL_STATUS_IDLE = 0,
    PERIRHINAL_STATUS_ENCODING,
    PERIRHINAL_STATUS_RECOGNIZING,
    PERIRHINAL_STATUS_NOVELTY_DETECTING,
    PERIRHINAL_STATUS_FAMILIARITY_COMPUTING,
    PERIRHINAL_STATUS_RECENCY_UPDATING,
    PERIRHINAL_STATUS_CONSOLIDATING,
    PERIRHINAL_STATUS_READY,
    PERIRHINAL_STATUS_ERROR
} perirhinal_status_t;

/**
 * @brief Error codes
 */
typedef enum {
    PERIRHINAL_ERROR_NONE = 0,
    PERIRHINAL_ERROR_INVALID_INPUT,
    PERIRHINAL_ERROR_OBJECT_NOT_FOUND,
    PERIRHINAL_ERROR_MEMORY_FULL,
    PERIRHINAL_ERROR_ENCODING_FAILED,
    PERIRHINAL_ERROR_RECOGNITION_FAILED,
    PERIRHINAL_ERROR_SECURITY_VIOLATION,
    PERIRHINAL_ERROR_IMMUNE_REJECTION,
    PERIRHINAL_ERROR_SUBSTRATE_DEPLETED,
    PERIRHINAL_ERROR_SYNC_FAILURE,
    PERIRHINAL_ERROR_BUFFER_OVERFLOW,
    PERIRHINAL_ERROR_INTERNAL
} perirhinal_error_t;

/**
 * @brief Object recognition confidence levels
 */
typedef enum {
    RECOGNITION_CONFIDENCE_NONE = 0,
    RECOGNITION_CONFIDENCE_LOW,
    RECOGNITION_CONFIDENCE_MEDIUM,
    RECOGNITION_CONFIDENCE_HIGH,
    RECOGNITION_CONFIDENCE_CERTAIN
} recognition_confidence_t;

/**
 * @brief Familiarity signal types
 */
typedef enum {
    FAMILIARITY_TYPE_NOVEL = 0,
    FAMILIARITY_TYPE_SEEN_BEFORE,
    FAMILIARITY_TYPE_FAMILIAR,
    FAMILIARITY_TYPE_VERY_FAMILIAR,
    FAMILIARITY_TYPE_KNOWN
} familiarity_type_t;

/**
 * @brief Neuromodulator channel types for bio-async
 */
typedef enum {
    PERIRHINAL_CHANNEL_DOPAMINE = 0,
    PERIRHINAL_CHANNEL_SEROTONIN,
    PERIRHINAL_CHANNEL_NOREPINEPHRINE,
    PERIRHINAL_CHANNEL_ACETYLCHOLINE,
    PERIRHINAL_CHANNEL_COUNT
} perirhinal_neuromod_channel_t;

/*=============================================================================
 * OBJECT CELL STRUCTURES
 *===========================================================================*/

/**
 * @brief Single object cell - responds to specific object identity
 */
typedef struct {
    uint32_t cell_id;
    uint32_t preferred_object_id;   /* Object this cell prefers */
    float selectivity;              /* How selective for this object */
    float view_invariance;          /* Degree of view invariance */
    float activation;               /* Current activation [0, 1] */
    float peak_rate;                /* Maximum firing rate */

    /* Feature tuning */
    float* feature_weights;         /* Weights to visual features */
    uint32_t feature_dim;

    /* Plasticity state */
    float eligibility_trace;
    float weight_sum;
    float learning_rate;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
    float last_spike_time;
} nimcp_object_cell_t;

/**
 * @brief Stored object representation
 */
typedef struct {
    uint32_t object_id;
    char name[64];                  /* Optional object name */

    /* Visual representation */
    float* visual_features;         /* Visual feature vector */
    uint32_t feature_dim;

    /* Identity representation */
    float* identity_vector;         /* Abstract identity vector */
    uint32_t identity_dim;

    /* Multi-view representations */
    float** view_vectors;           /* View-specific representations */
    uint32_t num_views;

    /* Familiarity and recency */
    float familiarity_strength;     /* Overall familiarity */
    float recency_signal;           /* Time since last seen */
    uint64_t last_seen_ms;          /* Timestamp of last encounter */
    uint32_t encounter_count;       /* Number of encounters */

    /* Semantic associations */
    uint32_t* associated_objects;   /* IDs of associated objects */
    float* association_strengths;
    uint32_t num_associations;

    /* Context binding */
    float* spatial_context;         /* Associated spatial context */
    uint32_t spatial_dim;
    float context_binding_strength;
} nimcp_stored_object_t;

/*=============================================================================
 * FAMILIARITY CELL STRUCTURES
 *===========================================================================*/

/**
 * @brief Familiarity neuron - responds based on prior exposure
 */
typedef struct {
    uint32_t cell_id;
    float familiarity_threshold;    /* Threshold for firing */
    float adaptation_rate;          /* How quickly it adapts */
    float activation;               /* Current activation [0, 1] */

    /* Familiarity computation */
    float baseline_response;        /* Response to novel stimuli */
    float repetition_suppression;   /* Suppression for repeated items */

    /* Memory trace */
    float* memory_trace;            /* Trace of seen objects */
    uint32_t trace_size;
    float trace_decay;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
} nimcp_familiarity_cell_t;

/*=============================================================================
 * NOVELTY DETECTOR STRUCTURES
 *===========================================================================*/

/**
 * @brief Novelty detector - signals new/unexpected objects
 */
typedef struct {
    uint32_t cell_id;
    float novelty_threshold;        /* Threshold for novelty signal */
    float activation;               /* Current activation [0, 1] */

    /* Novelty computation */
    float prediction_error;         /* Difference from expected */
    float surprise_signal;          /* Surprise magnitude */
    float curiosity_drive;          /* Drive to explore */

    /* Expected feature distribution */
    float* expected_features;
    float* feature_variance;
    uint32_t feature_dim;

    /* Habituation */
    float habituation_rate;
    float recovery_rate;
    float habituation_state;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
} nimcp_novelty_detector_t;

/*=============================================================================
 * RECENCY CELL STRUCTURES
 *===========================================================================*/

/**
 * @brief Recency cell - encodes time since last encounter
 */
typedef struct {
    uint32_t cell_id;
    float preferred_recency;        /* Time interval for peak firing */
    float tuning_width;             /* Temporal tuning width */
    float activation;               /* Current activation [0, 1] */

    /* Time encoding */
    float decay_rate;
    float time_constant;

    /* Object-specific recency */
    uint32_t tracked_object_id;
    float last_encounter_time;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
} nimcp_recency_cell_t;

/*=============================================================================
 * RECOGNITION RESULT STRUCTURE
 *===========================================================================*/

/**
 * @brief Result of object recognition
 */
typedef struct {
    uint32_t object_id;             /* Recognized object ID */
    float match_confidence;         /* Confidence in match [0, 1] */
    recognition_confidence_t confidence_level;

    /* Familiarity signals */
    float familiarity_strength;
    familiarity_type_t familiarity_type;

    /* Novelty signals */
    float novelty_signal;
    bool is_novel;

    /* Recency signals */
    float recency_signal;
    uint64_t last_seen_ms;
    uint32_t encounter_count;

    /* Alternative matches */
    uint32_t* alternative_ids;
    float* alternative_confidences;
    uint32_t num_alternatives;
} perirhinal_recognition_result_t;

/*=============================================================================
 * BRIDGE STATE STRUCTURES
 *===========================================================================*/

/**
 * @brief Entorhinal bridge state
 */
typedef struct {
    nimcp_entorhinal_t* entorhinal;
    float spatial_context_weight;
    float object_context_binding;
    uint64_t items_transferred;
} perirhinal_entorhinal_bridge_t;

/**
 * @brief Security bridge state
 */
typedef struct {
    nimcp_security_context_t* security_ctx;
    nimcp_access_control_t* access_control;
    uint32_t access_level;
    bool threat_detected;
    float threat_level;
    uint64_t last_validation_ms;
} perirhinal_security_bridge_t;

/**
 * @brief Immune bridge state
 */
typedef struct {
    brain_immune_system_t* immune;
    float health_score;
    bool anomaly_detected;
    float inflammation_level;
    uint32_t antibody_count;
    uint64_t last_scan_ms;
} perirhinal_immune_bridge_t;

/**
 * @brief Bio-async bridge state
 */
typedef struct {
    nimcp_bio_async_handler_t* bio_handler;
    nimcp_bio_router_t* router;
    float neuromodulator_levels[PERIRHINAL_CHANNEL_COUNT];
    uint32_t pending_messages;
    uint64_t messages_processed;
} perirhinal_bio_async_bridge_t;

/**
 * @brief SNN bridge state
 */
typedef struct {
    nimcp_snn_network_t* snn;
    uint32_t input_layer_id;
    uint32_t object_layer_id;
    uint32_t familiarity_layer_id;
    uint32_t novelty_layer_id;
    uint32_t output_layer_id;
    float spike_rate;
    float mean_membrane_potential;
} perirhinal_snn_bridge_t;

/**
 * @brief Plasticity bridge state
 */
typedef struct {
    nimcp_plasticity_manager_t* plasticity;
    nimcp_stdp_rule_t* stdp_rule;
    float learning_rate;
    float weight_decay;
    uint64_t weight_updates;
    float total_weight_change;
} perirhinal_plasticity_bridge_t;

/**
 * @brief Cognitive bridge state
 */
typedef struct {
    working_memory_t* working_memory;
    attention_system_t* attention;
    cognitive_integration_hub_t* hub;
    float attention_modulation;
    float working_memory_load;
    uint32_t cognitive_events_sent;
} perirhinal_cognitive_bridge_t;

/**
 * @brief Training bridge state
 */
typedef struct {
    nimcp_training_context_t* training_ctx;
    nimcp_backprop_adapter_t* backprop;
    bool training_enabled;
    float current_loss;
    uint64_t training_steps;
    float gradient_norm;
} perirhinal_training_bridge_t;

/**
 * @brief Neural substrate bridge state
 */
typedef struct {
    nimcp_neural_substrate_t* substrate;
    float atp_level;
    float oxygen_level;
    float glucose_level;
    float metabolic_rate;
    float firing_rate_modifier;
} perirhinal_substrate_bridge_t;

/**
 * @brief Prime resonance bridge state
 */
typedef struct {
    nimcp_prime_resonance_t* resonance;
    float theta_phase;
    float gamma_phase;
    float phase_lock_strength;
    float resonance_quality;
} perirhinal_resonance_bridge_t;

/**
 * @brief Thalamic bridge state
 */
typedef struct {
    thalamus_adapter_t* thalamus;
    float relay_gain;
    float attention_gate;
    uint32_t active_pathways;
} perirhinal_thalamic_bridge_t;

/**
 * @brief Hippocampus bridge state
 */
typedef struct {
    hippocampus_adapter_t* hippocampus;
    float hippocampal_theta_phase;
    float item_context_binding;
    float retrieval_cue_strength;
} perirhinal_hippocampus_bridge_t;

/**
 * @brief Perception bridge state
 */
typedef struct {
    nimcp_perception_layer_t* perception;
    float* visual_input;
    uint32_t visual_dim;
    float salience_signal;
    float attention_weight;
} perirhinal_perception_bridge_t;

/**
 * @brief Swarm bridge state
 */
typedef struct {
    nimcp_swarm_coordinator_t* swarm;
    float consensus_value;
    uint32_t active_agents;
    float coordination_strength;
} perirhinal_swarm_bridge_t;

/**
 * @brief Dragonfly bridge state
 */
typedef struct {
    nimcp_dragonfly_system_t* dragonfly;
    float* object_velocity;
    float tracking_confidence;
    uint32_t tracked_object_count;
} perirhinal_dragonfly_bridge_t;

/**
 * @brief Portia bridge state
 */
typedef struct {
    nimcp_portia_system_t* portia;
    float planning_depth;
    float object_value_estimate;
    float strategy_confidence;
} perirhinal_portia_bridge_t;

/**
 * @brief Cerebellum bridge state
 */
typedef struct {
    cerebellum_adapter_t* cerebellum;
    float timing_signal;
    float prediction_error;
    float motor_context;
} perirhinal_cerebellum_bridge_t;

/**
 * @brief Medulla bridge state
 */
typedef struct {
    medulla_adapter_t* medulla;
    float arousal_level;
    float orienting_response;
} perirhinal_medulla_bridge_t;

/**
 * @brief Omnidirectional bridge state
 */
typedef struct {
    nimcp_omnidirectional_system_t* omni;
    float* object_directions;
    uint32_t num_objects_in_view;
    float attention_direction;
} perirhinal_omni_bridge_t;

/**
 * @brief Hypothalamus bridge state
 */
typedef struct {
    hypothalamus_adapter_t* hypothalamus;
    float motivation_signal;
    float reward_association;
    float object_value;
} perirhinal_hypothalamus_bridge_t;

/**
 * @brief Logic system bridge state
 */
typedef struct {
    nimcp_logic_system_t* logic;
    bool constraint_satisfied;
    float inference_confidence;
    uint32_t active_rules;
} perirhinal_logic_bridge_t;

/**
 * @brief Brain KG bridge state
 */
typedef struct {
    nimcp_brain_kg_t* kg;
    uint32_t node_id;
    float health_status;
    uint32_t edge_count;
} perirhinal_kg_bridge_t;

/*=============================================================================
 * MAIN CONFIGURATION STRUCTURE
 *===========================================================================*/

/**
 * @brief Comprehensive perirhinal cortex configuration
 */
typedef struct {
    /* Object cell parameters */
    uint32_t num_object_cells;
    float object_selectivity;
    float view_invariance_target;

    /* Familiarity cell parameters */
    uint32_t num_familiarity_cells;
    float familiarity_threshold;
    float repetition_suppression_rate;

    /* Novelty detector parameters */
    uint32_t num_novelty_cells;
    float novelty_threshold;
    float habituation_rate;

    /* Recency cell parameters */
    uint32_t num_recency_cells;
    float recency_decay_rate;
    float max_recency_time_s;

    /* Memory parameters */
    uint32_t max_stored_objects;
    uint32_t feature_dim;
    uint32_t identity_dim;
    uint32_t max_views_per_object;

    /* Recognition parameters */
    float recognition_threshold;
    uint32_t max_alternatives;
    float view_matching_weight;
    float feature_matching_weight;

    /* Integration enables */
    bool enable_entorhinal;
    bool enable_security;
    bool enable_immune;
    bool enable_bio_async;
    bool enable_snn;
    bool enable_plasticity;
    bool enable_stdp;
    bool enable_cognitive;
    bool enable_training;
    bool enable_substrate;
    bool enable_resonance;
    bool enable_thalamic;
    bool enable_hippocampus;
    bool enable_perception;
    bool enable_swarm;
    bool enable_dragonfly;
    bool enable_portia;
    bool enable_cerebellum;
    bool enable_medulla;
    bool enable_omni;
    bool enable_hypothalamus;
    bool enable_logic;
    bool enable_kg;

    /* Processing options */
    bool enable_view_invariance;
    bool enable_semantic_associations;
    bool enable_context_binding;
    bool enable_recency_tracking;

    /* Learning parameters */
    float learning_rate;
    float weight_decay;
    float eligibility_decay;

    /* Oscillation parameters */
    float theta_frequency;
    float gamma_frequency;
    float phase_coupling_strength;
} perirhinal_config_t;

/*=============================================================================
 * MAIN PERIRHINAL CORTEX STRUCTURE
 *===========================================================================*/

/**
 * @brief Complete perirhinal cortex adapter
 */
typedef struct nimcp_perirhinal {
    /* Configuration */
    perirhinal_config_t config;

    /* Status */
    perirhinal_status_t status;
    perirhinal_error_t last_error;

    /* Object cell system */
    nimcp_object_cell_t* object_cells;
    uint32_t num_object_cells;

    /* Familiarity cells */
    nimcp_familiarity_cell_t* familiarity_cells;
    uint32_t num_familiarity_cells;

    /* Novelty detectors */
    nimcp_novelty_detector_t* novelty_cells;
    uint32_t num_novelty_cells;

    /* Recency cells */
    nimcp_recency_cell_t* recency_cells;
    uint32_t num_recency_cells;

    /* Stored object memory */
    nimcp_stored_object_t* stored_objects;
    uint32_t num_stored_objects;
    uint32_t max_stored_objects;

    /* Current processing state */
    float* current_visual_input;
    uint32_t current_input_dim;
    float current_familiarity;
    float current_novelty;
    float current_recency;

    /*=========================================================================
     * ALL INTEGRATION BRIDGES
     *=======================================================================*/

    perirhinal_entorhinal_bridge_t entorhinal_bridge;
    perirhinal_security_bridge_t security_bridge;
    perirhinal_immune_bridge_t immune_bridge;
    perirhinal_bio_async_bridge_t bio_async_bridge;
    perirhinal_snn_bridge_t snn_bridge;
    perirhinal_plasticity_bridge_t plasticity_bridge;
    perirhinal_cognitive_bridge_t cognitive_bridge;
    perirhinal_training_bridge_t training_bridge;
    perirhinal_substrate_bridge_t substrate_bridge;
    perirhinal_resonance_bridge_t resonance_bridge;
    perirhinal_thalamic_bridge_t thalamic_bridge;
    perirhinal_hippocampus_bridge_t hippocampus_bridge;
    perirhinal_perception_bridge_t perception_bridge;
    perirhinal_swarm_bridge_t swarm_bridge;
    perirhinal_dragonfly_bridge_t dragonfly_bridge;
    perirhinal_portia_bridge_t portia_bridge;
    perirhinal_cerebellum_bridge_t cerebellum_bridge;
    perirhinal_medulla_bridge_t medulla_bridge;
    perirhinal_omni_bridge_t omni_bridge;
    perirhinal_hypothalamus_bridge_t hypothalamus_bridge;
    perirhinal_logic_bridge_t logic_bridge;
    perirhinal_kg_bridge_t kg_bridge;

    /* Logging and metrics */
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;

    /* Statistics */
    uint64_t updates_processed;
    uint64_t objects_encoded;
    uint64_t objects_recognized;
    uint64_t novelty_detections;
    float mean_recognition_confidence;
    float mean_familiarity_signal;
    double total_processing_time_ms;

    /* Timing */
    uint64_t creation_time_ms;
    uint64_t last_update_ms;
    float simulation_dt_ms;
} nimcp_perirhinal_t;

/*=============================================================================
 * STATISTICS STRUCTURE
 *===========================================================================*/

/**
 * @brief Comprehensive statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t updates_processed;
    uint64_t objects_encoded;
    uint64_t objects_recognized;
    uint64_t objects_forgotten;
    uint64_t novelty_detections;
    uint64_t familiarity_computations;

    /* Object cell statistics */
    float mean_object_activation;
    float object_selectivity_mean;
    float view_invariance_achieved;

    /* Familiarity statistics */
    float mean_familiarity_signal;
    float familiarity_accuracy;
    uint64_t correct_recognitions;
    uint64_t false_alarms;
    uint64_t misses;

    /* Novelty statistics */
    float mean_novelty_signal;
    float novelty_detection_rate;
    float false_novelty_rate;

    /* Recency statistics */
    float mean_recency_signal;
    float recency_accuracy;

    /* Memory statistics */
    uint32_t total_stored_objects;
    float memory_utilization;
    float mean_encounter_count;

    /* Recognition statistics */
    float recognition_accuracy;
    float mean_recognition_latency_ms;
    float mean_recognition_confidence;

    /* Integration statistics */
    uint64_t bio_async_messages_sent;
    uint64_t bio_async_messages_received;
    uint64_t cognitive_events_published;
    uint64_t training_updates;
    uint32_t security_validations;
    uint32_t immune_scans;

    /* Timing statistics */
    float mean_update_latency_ms;
    float max_update_latency_ms;
    float mean_encoding_latency_ms;
    float mean_recognition_latency;
} perirhinal_stats_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
perirhinal_config_t perirhinal_default_config(void);

/**
 * @brief Create perirhinal cortex instance
 */
nimcp_perirhinal_t* perirhinal_create(const perirhinal_config_t* config);

/**
 * @brief Destroy perirhinal cortex instance
 */
void perirhinal_destroy(nimcp_perirhinal_t* pr);

/**
 * @brief Reset to initial state
 */
int perirhinal_reset(nimcp_perirhinal_t* pr);

/**
 * @brief Main update function
 */
int perirhinal_update(nimcp_perirhinal_t* pr, float dt);

/*=============================================================================
 * BRIDGE INITIALIZATION API
 *===========================================================================*/

/**
 * @brief Initialize entorhinal bridge
 */
int perirhinal_init_entorhinal_bridge(nimcp_perirhinal_t* pr,
    nimcp_entorhinal_t* entorhinal);

/**
 * @brief Initialize security bridge
 */
int perirhinal_init_security_bridge(nimcp_perirhinal_t* pr,
    nimcp_security_context_t* security_ctx,
    nimcp_access_control_t* access_control);

/**
 * @brief Initialize immune bridge
 */
int perirhinal_init_immune_bridge(nimcp_perirhinal_t* pr,
    brain_immune_system_t* immune);

/**
 * @brief Initialize bio-async bridge
 */
int perirhinal_init_bio_async_bridge(nimcp_perirhinal_t* pr,
    nimcp_bio_router_t* router);

/**
 * @brief Initialize SNN bridge
 */
int perirhinal_init_snn_bridge(nimcp_perirhinal_t* pr,
    nimcp_snn_network_t* snn);

/**
 * @brief Initialize plasticity bridge
 */
int perirhinal_init_plasticity_bridge(nimcp_perirhinal_t* pr,
    nimcp_plasticity_manager_t* plasticity,
    nimcp_stdp_rule_t* stdp_rule);

/**
 * @brief Initialize cognitive bridge
 */
int perirhinal_init_cognitive_bridge(nimcp_perirhinal_t* pr,
    working_memory_t* wm,
    attention_system_t* attention,
    cognitive_integration_hub_t* hub);

/**
 * @brief Initialize training bridge
 */
int perirhinal_init_training_bridge(nimcp_perirhinal_t* pr,
    nimcp_training_context_t* training_ctx);

/**
 * @brief Initialize neural substrate bridge
 */
int perirhinal_init_substrate_bridge(nimcp_perirhinal_t* pr,
    nimcp_neural_substrate_t* substrate);

/**
 * @brief Initialize prime resonance bridge
 */
int perirhinal_init_resonance_bridge(nimcp_perirhinal_t* pr,
    nimcp_prime_resonance_t* resonance);

/**
 * @brief Initialize thalamic bridge
 */
int perirhinal_init_thalamic_bridge(nimcp_perirhinal_t* pr,
    thalamus_adapter_t* thalamus);

/**
 * @brief Initialize hippocampus bridge
 */
int perirhinal_init_hippocampus_bridge(nimcp_perirhinal_t* pr,
    hippocampus_adapter_t* hippocampus);

/**
 * @brief Initialize perception bridge
 */
int perirhinal_init_perception_bridge(nimcp_perirhinal_t* pr,
    nimcp_perception_layer_t* perception);

/**
 * @brief Initialize all bridges at once
 */
int perirhinal_init_all_bridges(nimcp_perirhinal_t* pr,
    nimcp_brain_t* brain);

/*=============================================================================
 * OBJECT RECOGNITION API
 *===========================================================================*/

/**
 * @brief Encode a new object
 */
int perirhinal_encode_object(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim,
    const char* name, uint32_t* object_id_out);

/**
 * @brief Recognize an object from visual features
 */
int perirhinal_recognize_object(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim,
    perirhinal_recognition_result_t* result);

/**
 * @brief Add a new view to an existing object
 */
int perirhinal_add_object_view(nimcp_perirhinal_t* pr,
    uint32_t object_id, const float* view_features, uint32_t feature_dim);

/**
 * @brief Get object by ID
 */
const nimcp_stored_object_t* perirhinal_get_object(const nimcp_perirhinal_t* pr,
    uint32_t object_id);

/**
 * @brief Update object familiarity (after encounter)
 */
int perirhinal_update_familiarity(nimcp_perirhinal_t* pr,
    uint32_t object_id);

/**
 * @brief Forget an object (remove from memory)
 */
int perirhinal_forget_object(nimcp_perirhinal_t* pr,
    uint32_t object_id);

/*=============================================================================
 * FAMILIARITY API
 *===========================================================================*/

/**
 * @brief Compute familiarity signal for input
 */
float perirhinal_compute_familiarity(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim);

/**
 * @brief Get familiarity type classification
 */
familiarity_type_t perirhinal_classify_familiarity(const nimcp_perirhinal_t* pr,
    float familiarity_signal);

/**
 * @brief Check if input is familiar
 */
bool perirhinal_is_familiar(const nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim);

/**
 * @brief Get familiarity for known object
 */
float perirhinal_get_object_familiarity(const nimcp_perirhinal_t* pr,
    uint32_t object_id);

/*=============================================================================
 * NOVELTY API
 *===========================================================================*/

/**
 * @brief Compute novelty signal for input
 */
float perirhinal_compute_novelty(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim);

/**
 * @brief Check if input is novel
 */
bool perirhinal_is_novel(const nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim);

/**
 * @brief Get surprise/prediction error signal
 */
float perirhinal_get_surprise_signal(const nimcp_perirhinal_t* pr);

/**
 * @brief Habituate to repeated stimuli
 */
int perirhinal_habituate(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim);

/*=============================================================================
 * RECENCY API
 *===========================================================================*/

/**
 * @brief Update recency for encountered object
 */
int perirhinal_update_recency(nimcp_perirhinal_t* pr,
    uint32_t object_id);

/**
 * @brief Get recency signal for object
 */
float perirhinal_get_recency_signal(const nimcp_perirhinal_t* pr,
    uint32_t object_id);

/**
 * @brief Get time since last encounter
 */
uint64_t perirhinal_get_time_since_encounter(const nimcp_perirhinal_t* pr,
    uint32_t object_id);

/**
 * @brief Decay all recency signals
 */
int perirhinal_decay_recency(nimcp_perirhinal_t* pr, float dt);

/*=============================================================================
 * SEMANTIC ASSOCIATION API
 *===========================================================================*/

/**
 * @brief Create association between objects
 */
int perirhinal_create_association(nimcp_perirhinal_t* pr,
    uint32_t object_id_a, uint32_t object_id_b, float strength);

/**
 * @brief Get associated objects
 */
int perirhinal_get_associations(const nimcp_perirhinal_t* pr,
    uint32_t object_id, uint32_t* associated_ids, float* strengths,
    uint32_t max_associations, uint32_t* num_found);

/**
 * @brief Strengthen association
 */
int perirhinal_strengthen_association(nimcp_perirhinal_t* pr,
    uint32_t object_id_a, uint32_t object_id_b, float delta);

/*=============================================================================
 * CONTEXT BINDING API
 *===========================================================================*/

/**
 * @brief Bind object to spatial context
 */
int perirhinal_bind_spatial_context(nimcp_perirhinal_t* pr,
    uint32_t object_id, const float* spatial_context, uint32_t spatial_dim);

/**
 * @brief Get spatial context for object
 */
int perirhinal_get_spatial_context(const nimcp_perirhinal_t* pr,
    uint32_t object_id, float* context_out, uint32_t max_dim);

/**
 * @brief Find objects by spatial context
 */
int perirhinal_find_by_context(const nimcp_perirhinal_t* pr,
    const float* spatial_context, uint32_t spatial_dim,
    uint32_t* object_ids, float* match_strengths,
    uint32_t max_results, uint32_t* num_found);

/*=============================================================================
 * ENTORHINAL INTEGRATION API
 *===========================================================================*/

/**
 * @brief Send object representation to entorhinal
 */
int perirhinal_send_to_entorhinal(nimcp_perirhinal_t* pr,
    uint32_t object_id);

/**
 * @brief Receive spatial context from entorhinal
 */
int perirhinal_receive_from_entorhinal(nimcp_perirhinal_t* pr,
    const float* spatial_context, uint32_t spatial_dim);

/**
 * @brief Synchronize with entorhinal cortex
 */
int perirhinal_sync_entorhinal(nimcp_perirhinal_t* pr);

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW API
 *===========================================================================*/

/**
 * @brief Process incoming data from all bridges
 */
int perirhinal_process_incoming(nimcp_perirhinal_t* pr);

/**
 * @brief Send outgoing data to all bridges
 */
int perirhinal_send_outgoing(nimcp_perirhinal_t* pr);

/**
 * @brief Full bidirectional update cycle
 */
int perirhinal_bidirectional_update(nimcp_perirhinal_t* pr, float dt);

/**
 * @brief Process visual input from perception layer
 */
int perirhinal_process_visual_input(nimcp_perirhinal_t* pr,
    const float* visual_features, uint32_t feature_dim);

/*=============================================================================
 * STATUS AND DIAGNOSTICS API
 *===========================================================================*/

/**
 * @brief Get current status
 */
perirhinal_status_t perirhinal_get_status(const nimcp_perirhinal_t* pr);

/**
 * @brief Get last error
 */
perirhinal_error_t perirhinal_get_last_error(const nimcp_perirhinal_t* pr);

/**
 * @brief Get error string
 */
const char* perirhinal_error_string(perirhinal_error_t error);

/**
 * @brief Get status string
 */
const char* perirhinal_status_string(perirhinal_status_t status);

/**
 * @brief Get comprehensive statistics
 */
int perirhinal_get_stats(const nimcp_perirhinal_t* pr, perirhinal_stats_t* stats);

/**
 * @brief Get configuration
 */
int perirhinal_get_config(const nimcp_perirhinal_t* pr, perirhinal_config_t* config);

/**
 * @brief Get health status (for brain KG)
 */
float perirhinal_get_health_status(const nimcp_perirhinal_t* pr);

/**
 * @brief Log diagnostic information
 */
int perirhinal_log_diagnostics(const nimcp_perirhinal_t* pr);

/**
 * @brief Get object cell activity
 */
size_t perirhinal_get_object_cell_activity(const nimcp_perirhinal_t* pr,
    float* activity, size_t max_cells);

/**
 * @brief Get familiarity cell activity
 */
size_t perirhinal_get_familiarity_cell_activity(const nimcp_perirhinal_t* pr,
    float* activity, size_t max_cells);

/**
 * @brief Get current familiarity signal
 */
float perirhinal_get_current_familiarity(const nimcp_perirhinal_t* pr);

/**
 * @brief Get current novelty signal
 */
float perirhinal_get_current_novelty(const nimcp_perirhinal_t* pr);

/*=============================================================================
 * SERIALIZATION API
 *===========================================================================*/

/**
 * @brief Serialize perirhinal state
 */
int perirhinal_serialize(const nimcp_perirhinal_t* pr,
    uint8_t* buffer, size_t buffer_size, size_t* bytes_written);

/**
 * @brief Deserialize perirhinal state
 */
int perirhinal_deserialize(nimcp_perirhinal_t* pr,
    const uint8_t* buffer, size_t buffer_size);

/**
 * @brief Get serialization size
 */
size_t perirhinal_get_serialization_size(const nimcp_perirhinal_t* pr);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PERIRHINAL_H */
