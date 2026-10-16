/**
 * @file nimcp_entorhinal.h
 * @brief Entorhinal Cortex - Memory Gateway with Grid Cells
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * WHAT: Entorhinal cortex implementation with grid cells, border cells,
 *       head direction cells, and memory gateway functionality.
 *
 * WHY:  The entorhinal cortex is the primary interface between hippocampus
 *       and neocortex, essential for spatial navigation and memory encoding.
 *
 * HOW:  Implements grid cell firing patterns, path integration, and bidirectional
 *       memory transfer with full integration across all NIMCP modules.
 *
 * BIOLOGICAL BASIS:
 * - Grid cells: Hexagonal firing patterns for metric spatial representation
 * - Border cells: Fire near environmental boundaries
 * - Head direction cells: Encode current heading direction
 * - Memory gateway: Routes information between hippocampus and neocortex
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
 * - Perception Layer: Sensory input processing, feature extraction
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
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ENTORHINAL_H
#define NIMCP_ENTORHINAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * FORWARD DECLARATIONS - ALL INTEGRATED MODULES
 *===========================================================================*/

/* Core brain systems */
typedef struct nimcp_brain_handle* nimcp_brain_t;
typedef struct brain_kg brain_kg_t;
typedef struct brain_immune_system brain_immune_system_t;

/* Bio-async communication - forward declarations matching async/nimcp_bio_async.h */
typedef struct nimcp_bio_async_handler_struct* nimcp_bio_async_handler_t;
typedef struct nimcp_bio_router_struct* nimcp_bio_router_t;
typedef struct nimcp_bio_future_struct* nimcp_bio_future_t;

/* Security */
typedef struct nimcp_security_context nimcp_security_context_t;
typedef struct nimcp_access_control nimcp_access_control_t;

/* Logging and metrics */
typedef struct nimcp_logger_struct* nimcp_logger_t;
typedef struct nimcp_metrics nimcp_metrics_t;

/* Neural systems */
typedef struct snn_network_s snn_network_t;
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

#define ENTORHINAL_DEFAULT_GRID_CELLS           512
#define ENTORHINAL_DEFAULT_BORDER_CELLS         128
#define ENTORHINAL_DEFAULT_HD_CELLS             60
#define ENTORHINAL_DEFAULT_OBJECT_CELLS         256
#define ENTORHINAL_DEFAULT_SPEED_CELLS          64
#define ENTORHINAL_DEFAULT_TIME_CELLS           128
#define ENTORHINAL_DEFAULT_SPATIAL_DIM          3
#define ENTORHINAL_DEFAULT_FEATURE_DIM          256
#define ENTORHINAL_DEFAULT_NUM_GRID_SCALES      6
#define ENTORHINAL_DEFAULT_NUM_GRID_ORIENTATIONS 3
#define ENTORHINAL_MIN_GRID_SPACING             0.3f
#define ENTORHINAL_MAX_GRID_SPACING             3.0f
#define ENTORHINAL_GRID_SCALE_RATIO             1.42f  /* sqrt(2) */

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Entorhinal cortex processing status
 */
typedef enum {
    ENTORHINAL_STATUS_IDLE = 0,
    ENTORHINAL_STATUS_PATH_INTEGRATING,
    ENTORHINAL_STATUS_ENCODING,
    ENTORHINAL_STATUS_RETRIEVING,
    ENTORHINAL_STATUS_GATEWAY_TRANSFER,
    ENTORHINAL_STATUS_CONSOLIDATING,
    ENTORHINAL_STATUS_CALIBRATING,
    ENTORHINAL_STATUS_READY,
    ENTORHINAL_STATUS_ERROR
} entorhinal_status_t;

/**
 * @brief Error codes
 */
typedef enum {
    ENTORHINAL_ERROR_NONE = 0,
    ENTORHINAL_ERROR_INVALID_INPUT,
    ENTORHINAL_ERROR_GRID_DRIFT,
    ENTORHINAL_ERROR_PATH_INTEGRATION_FAILURE,
    ENTORHINAL_ERROR_MEMORY_GATEWAY_BLOCKED,
    ENTORHINAL_ERROR_SECURITY_VIOLATION,
    ENTORHINAL_ERROR_IMMUNE_REJECTION,
    ENTORHINAL_ERROR_SUBSTRATE_DEPLETED,
    ENTORHINAL_ERROR_SYNC_FAILURE,
    ENTORHINAL_ERROR_BUFFER_OVERFLOW,
    ENTORHINAL_ERROR_INTERNAL
} entorhinal_error_t;

/**
 * @brief Grid cell module types (different scales/orientations)
 */
typedef enum {
    GRID_MODULE_FINE = 0,      /* ~30cm spacing */
    GRID_MODULE_MEDIUM,        /* ~50cm spacing */
    GRID_MODULE_COARSE,        /* ~100cm spacing */
    GRID_MODULE_VERY_COARSE,   /* ~200cm spacing */
    GRID_MODULE_COUNT
} grid_module_t;

/**
 * @brief Memory gateway transfer direction
 */
typedef enum {
    GATEWAY_DIR_HIPPOCAMPUS_TO_NEOCORTEX = 0,
    GATEWAY_DIR_NEOCORTEX_TO_HIPPOCAMPUS,
    GATEWAY_DIR_BIDIRECTIONAL
} gateway_direction_t;

/**
 * @brief Neuromodulator channel types for bio-async
 */
typedef enum {
    ENTORHINAL_CHANNEL_DOPAMINE = 0,
    ENTORHINAL_CHANNEL_SEROTONIN,
    ENTORHINAL_CHANNEL_NOREPINEPHRINE,
    ENTORHINAL_CHANNEL_ACETYLCHOLINE,
    ENTORHINAL_CHANNEL_COUNT
} entorhinal_neuromod_channel_t;

/*=============================================================================
 * GRID CELL STRUCTURES
 *===========================================================================*/

/**
 * @brief Single grid cell representation
 */
typedef struct {
    uint32_t cell_id;
    grid_module_t module;
    float spacing;              /* Grid spacing in meters */
    float orientation;          /* Grid orientation in radians */
    float phase_x;              /* Phase offset X */
    float phase_y;              /* Phase offset Y */
    float phase_z;              /* Phase offset Z (for 3D) */
    float activation;           /* Current activation [0, 1] */
    float peak_rate;            /* Maximum firing rate */

    /* Plasticity state */
    float eligibility_trace;
    float weight_sum;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
    float last_spike_time;
} nimcp_grid_cell_t;

/**
 * @brief Grid cell module (group of cells at same scale)
 */
typedef struct {
    grid_module_t module_type;
    float base_spacing;
    float orientation_offset;
    nimcp_grid_cell_t* cells;
    uint32_t num_cells;

    /* Module-level statistics */
    float mean_activation;
    float population_vector_x;
    float population_vector_y;
    float coherence;
} nimcp_grid_module_t;

/*=============================================================================
 * BORDER CELL STRUCTURES
 *===========================================================================*/

/**
 * @brief Border cell - fires near environmental boundaries
 */
typedef struct {
    uint32_t cell_id;
    float preferred_distance;   /* Distance to boundary for peak firing */
    float preferred_direction;  /* Direction to boundary (radians) */
    float tuning_width;         /* Width of firing field */
    float activation;           /* Current activation [0, 1] */
    float boundary_confidence;  /* Confidence in boundary detection */

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
} nimcp_border_cell_t;

/*=============================================================================
 * HEAD DIRECTION CELL STRUCTURES
 *===========================================================================*/

/**
 * @brief Head direction cell - encodes current heading
 */
typedef struct {
    uint32_t cell_id;
    float preferred_direction;  /* Preferred heading (radians) */
    float tuning_width;         /* Tuning curve width */
    float activation;           /* Current activation [0, 1] */
    float anticipatory_offset;  /* Anticipatory firing offset */

    /* Vestibular integration */
    float angular_velocity_gain;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
} nimcp_hd_cell_t;

/*=============================================================================
 * OBJECT AND SPEED CELL STRUCTURES
 *===========================================================================*/

/**
 * @brief Object vector cell - encodes object positions relative to self
 */
typedef struct {
    uint32_t cell_id;
    float preferred_distance;
    float preferred_direction;
    float object_identity;      /* Object type encoding */
    float activation;
    uint32_t snn_neuron_id;
} nimcp_object_cell_t;

/**
 * @brief Speed cell - encodes running speed
 */
typedef struct {
    uint32_t cell_id;
    float preferred_speed;
    float tuning_width;
    float activation;
    float speed_gain;           /* For path integration */
    uint32_t snn_neuron_id;
} nimcp_speed_cell_t;

/**
 * @brief Time cell - encodes elapsed time
 */
typedef struct {
    uint32_t cell_id;
    float preferred_time;       /* Time since event */
    float tuning_width;
    float activation;
    float decay_rate;
    uint32_t snn_neuron_id;
} nimcp_time_cell_t;

/*=============================================================================
 * PATH INTEGRATION STATE
 *===========================================================================*/

/**
 * @brief Path integration accumulator
 */
typedef struct {
    /* Current estimated position */
    float position[3];          /* x, y, z */
    float heading;              /* Current heading (radians) */
    float speed;                /* Current speed */

    /* Velocity integration */
    float velocity[3];
    float angular_velocity;

    /* Error accumulation */
    float accumulated_error;
    float drift_rate;
    float last_reset_time;

    /* Correction signals */
    float visual_correction[3];
    float boundary_correction[3];
    float landmark_correction[3];

    /* Confidence */
    float position_confidence;
    float heading_confidence;
} nimcp_path_integration_t;

/*=============================================================================
 * MEMORY GATEWAY STRUCTURES
 *===========================================================================*/

/**
 * @brief Memory gateway state
 */
typedef struct {
    /* Gating signals */
    float encoding_gate;        /* Hippocampus input gate [0, 1] */
    float retrieval_gate;       /* Hippocampus output gate [0, 1] */
    float consolidation_gate;   /* Neocortex transfer gate [0, 1] */

    /* Binding strength */
    float memory_binding_strength;
    float context_binding_strength;
    float temporal_binding_strength;

    /* Transfer buffers */
    float* encoding_buffer;
    uint32_t encoding_buffer_size;
    float* retrieval_buffer;
    uint32_t retrieval_buffer_size;

    /* Statistics */
    uint64_t items_encoded;
    uint64_t items_retrieved;
    uint64_t items_consolidated;
    float transfer_latency_ms;
} nimcp_memory_gateway_t;

/*=============================================================================
 * BRIDGE STATE STRUCTURES
 *===========================================================================*/

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
} entorhinal_security_bridge_t;

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
} entorhinal_immune_bridge_t;

/**
 * @brief Bio-async bridge state
 */
typedef struct {
    nimcp_bio_async_handler_t* bio_handler;
    nimcp_bio_router_t* router;
    float neuromodulator_levels[ENTORHINAL_CHANNEL_COUNT];
    uint32_t pending_messages;
    uint64_t messages_processed;
} entorhinal_bio_async_bridge_t;

/**
 * @brief SNN bridge state
 */
typedef struct {
    snn_network_t* snn;
    uint32_t input_layer_id;
    uint32_t grid_layer_id;
    uint32_t border_layer_id;
    uint32_t hd_layer_id;
    uint32_t output_layer_id;
    float spike_rate;
    float mean_membrane_potential;
} entorhinal_snn_bridge_t;

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
} entorhinal_plasticity_bridge_t;

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
} entorhinal_cognitive_bridge_t;

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
} entorhinal_training_bridge_t;

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
} entorhinal_substrate_bridge_t;

/**
 * @brief Prime resonance bridge state
 */
typedef struct {
    nimcp_prime_resonance_t* resonance;
    float theta_phase;          /* 4-12 Hz hippocampal theta */
    float gamma_phase;          /* 30-100 Hz gamma */
    float phase_lock_strength;
    float resonance_quality;
} entorhinal_resonance_bridge_t;

/**
 * @brief Thalamic bridge state
 */
typedef struct {
    thalamus_adapter_t* thalamus;
    float relay_gain;
    float attention_gate;
    uint32_t active_pathways;
} entorhinal_thalamic_bridge_t;

/**
 * @brief Hippocampus bridge state
 */
typedef struct {
    hippocampus_adapter_t* hippocampus;
    float hippocampal_theta_phase;
    float ca3_input_strength;
    float ca1_output_strength;
    float dg_pattern_separation;
} entorhinal_hippocampus_bridge_t;

/**
 * @brief Perception bridge state
 */
typedef struct {
    nimcp_perception_layer_t* perception;
    float* visual_input;
    uint32_t visual_dim;
    float* auditory_input;
    uint32_t auditory_dim;
    float salience_signal;
} entorhinal_perception_bridge_t;

/**
 * @brief Swarm bridge state
 */
typedef struct {
    nimcp_swarm_coordinator_t* swarm;
    float consensus_value;
    uint32_t active_agents;
    float coordination_strength;
} entorhinal_swarm_bridge_t;

/**
 * @brief Dragonfly bridge state
 */
typedef struct {
    nimcp_dragonfly_system_t* dragonfly;
    float interception_vector[3];
    float target_velocity[3];
    float prediction_horizon;
} entorhinal_dragonfly_bridge_t;

/**
 * @brief Portia bridge state
 */
typedef struct {
    nimcp_portia_system_t* portia;
    float planning_depth;
    float deception_detection;
    float strategy_confidence;
} entorhinal_portia_bridge_t;

/**
 * @brief Cerebellum bridge state
 */
typedef struct {
    cerebellum_adapter_t* cerebellum;
    float timing_signal;
    float prediction_error;
    float motor_correction[3];
} entorhinal_cerebellum_bridge_t;

/**
 * @brief Medulla bridge state
 */
typedef struct {
    medulla_adapter_t* medulla;
    float arousal_level;
    float respiratory_phase;
    float cardiac_phase;
} entorhinal_medulla_bridge_t;

/**
 * @brief Omnidirectional bridge state
 */
typedef struct {
    nimcp_omnidirectional_system_t* omni;
    float spatial_attention[360];
    float threat_direction;
    float opportunity_direction;
} entorhinal_omni_bridge_t;

/**
 * @brief Hypothalamus bridge state
 */
typedef struct {
    hypothalamus_adapter_t* hypothalamus;
    float motivation_signal;
    float homeostatic_drive;
    float reward_prediction;
} entorhinal_hypothalamus_bridge_t;

/**
 * @brief Logic system bridge state
 */
typedef struct {
    nimcp_logic_system_t* logic;
    bool constraint_satisfied;
    float inference_confidence;
    uint32_t active_rules;
} entorhinal_logic_bridge_t;

/**
 * @brief Brain KG bridge state
 */
typedef struct {
    brain_kg_t* kg;
    uint32_t node_id;
    float health_status;
    uint32_t edge_count;
} entorhinal_kg_bridge_t;

/*=============================================================================
 * MAIN CONFIGURATION STRUCTURE
 *===========================================================================*/

/**
 * @brief Comprehensive entorhinal cortex configuration
 */
typedef struct {
    /* Grid cell parameters */
    uint32_t num_grid_cells;
    uint32_t num_grid_modules;
    float min_grid_spacing;
    float max_grid_spacing;
    float grid_scale_ratio;

    /* Border cell parameters */
    uint32_t num_border_cells;
    float border_detection_range;

    /* Head direction parameters */
    uint32_t num_hd_cells;
    float hd_tuning_width;
    float anticipatory_time_ms;

    /* Object/speed/time cells */
    uint32_t num_object_cells;
    uint32_t num_speed_cells;
    uint32_t num_time_cells;

    /* Path integration */
    float path_integration_gain;
    float drift_correction_rate;
    float visual_reset_threshold;

    /* Memory gateway */
    uint32_t encoding_buffer_size;
    uint32_t retrieval_buffer_size;
    float encoding_threshold;
    float retrieval_threshold;

    /* Spatial parameters */
    uint32_t spatial_dim;
    float feature_dim;
    float environment_size[3];

    /* Integration enables */
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
    bool enable_path_integration;
    bool enable_boundary_detection;
    bool enable_object_tracking;
    bool enable_speed_encoding;
    bool enable_time_encoding;

    /* Learning parameters */
    float learning_rate;
    float weight_decay;
    float eligibility_decay;

    /* Oscillation parameters */
    float theta_frequency;      /* ~8 Hz */
    float gamma_frequency;      /* ~40 Hz */
    float phase_precession_rate;
} entorhinal_config_t;

/*=============================================================================
 * MAIN ENTORHINAL CORTEX STRUCTURE
 *===========================================================================*/

/**
 * @brief Complete entorhinal cortex adapter
 */
typedef struct nimcp_entorhinal {
    /* Configuration */
    entorhinal_config_t config;

    /* Status */
    entorhinal_status_t status;
    entorhinal_error_t last_error;

    /* Grid cell system */
    nimcp_grid_module_t* grid_modules;
    uint32_t num_grid_modules;
    uint32_t total_grid_cells;

    /* Border cells */
    nimcp_border_cell_t* border_cells;
    uint32_t num_border_cells;

    /* Head direction cells */
    nimcp_hd_cell_t* hd_cells;
    uint32_t num_hd_cells;
    float current_heading;

    /* Object cells */
    nimcp_object_cell_t* object_cells;
    uint32_t num_object_cells;

    /* Speed cells */
    nimcp_speed_cell_t* speed_cells;
    uint32_t num_speed_cells;

    /* Time cells */
    nimcp_time_cell_t* time_cells;
    uint32_t num_time_cells;

    /* Path integration */
    nimcp_path_integration_t path_integration;

    /* Memory gateway */
    nimcp_memory_gateway_t memory_gateway;

    /*=========================================================================
     * ALL INTEGRATION BRIDGES
     *=======================================================================*/

    entorhinal_security_bridge_t security_bridge;
    entorhinal_immune_bridge_t immune_bridge;
    entorhinal_bio_async_bridge_t bio_async_bridge;
    entorhinal_snn_bridge_t snn_bridge;
    entorhinal_plasticity_bridge_t plasticity_bridge;
    entorhinal_cognitive_bridge_t cognitive_bridge;
    entorhinal_training_bridge_t training_bridge;
    entorhinal_substrate_bridge_t substrate_bridge;
    entorhinal_resonance_bridge_t resonance_bridge;
    entorhinal_thalamic_bridge_t thalamic_bridge;
    entorhinal_hippocampus_bridge_t hippocampus_bridge;
    entorhinal_perception_bridge_t perception_bridge;
    entorhinal_swarm_bridge_t swarm_bridge;
    entorhinal_dragonfly_bridge_t dragonfly_bridge;
    entorhinal_portia_bridge_t portia_bridge;
    entorhinal_cerebellum_bridge_t cerebellum_bridge;
    entorhinal_medulla_bridge_t medulla_bridge;
    entorhinal_omni_bridge_t omni_bridge;
    entorhinal_hypothalamus_bridge_t hypothalamus_bridge;
    entorhinal_logic_bridge_t logic_bridge;
    entorhinal_kg_bridge_t kg_bridge;

    /* Logging and metrics */
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;

    /* Statistics */
    uint64_t updates_processed;
    uint64_t position_updates;
    uint64_t memory_transfers;
    float mean_grid_coherence;
    float mean_position_error;
    double total_processing_time_ms;

    /* Timing */
    uint64_t creation_time_ms;
    uint64_t last_update_ms;
    float simulation_dt_ms;
} nimcp_entorhinal_t;

/*=============================================================================
 * STATISTICS STRUCTURE
 *===========================================================================*/

/**
 * @brief Comprehensive statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t updates_processed;
    uint64_t position_updates;
    uint64_t heading_updates;
    uint64_t memory_encodings;
    uint64_t memory_retrievals;
    uint64_t memory_consolidations;

    /* Grid cell statistics */
    float mean_grid_activation;
    float grid_population_coherence;
    float grid_drift_accumulated;
    uint32_t grid_resets_triggered;

    /* Border cell statistics */
    float mean_border_activation;
    uint32_t boundaries_detected;

    /* Head direction statistics */
    float mean_hd_activation;
    float heading_stability;

    /* Path integration statistics */
    float position_error_mean;
    float position_error_max;
    float heading_error_mean;
    uint32_t visual_corrections;
    uint32_t boundary_corrections;

    /* Memory gateway statistics */
    float encoding_success_rate;
    float retrieval_success_rate;
    float consolidation_rate;
    float mean_transfer_latency_ms;

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
    float mean_retrieval_latency_ms;
} entorhinal_stats_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
entorhinal_config_t entorhinal_default_config(void);

/**
 * @brief Create entorhinal cortex instance
 */
nimcp_entorhinal_t* entorhinal_create(const entorhinal_config_t* config);

/**
 * @brief Destroy entorhinal cortex instance
 */
void entorhinal_destroy(nimcp_entorhinal_t* ec);

/**
 * @brief Reset to initial state
 */
bool entorhinal_reset(nimcp_entorhinal_t* ec);

/*=============================================================================
 * BRIDGE INITIALIZATION API
 *===========================================================================*/

/**
 * @brief Initialize security bridge
 */
int entorhinal_init_security_bridge(nimcp_entorhinal_t* ec,
    nimcp_security_context_t* security_ctx,
    nimcp_access_control_t* access_control);

/**
 * @brief Initialize immune bridge
 */
int entorhinal_init_immune_bridge(nimcp_entorhinal_t* ec,
    brain_immune_system_t* immune);

/**
 * @brief Initialize bio-async bridge
 */
int entorhinal_init_bio_async_bridge(nimcp_entorhinal_t* ec,
    nimcp_bio_router_t* router);

/**
 * @brief Initialize SNN bridge
 */
int entorhinal_init_snn_bridge(nimcp_entorhinal_t* ec,
    snn_network_t* snn);

/**
 * @brief Initialize plasticity bridge
 */
int entorhinal_init_plasticity_bridge(nimcp_entorhinal_t* ec,
    nimcp_plasticity_manager_t* plasticity,
    nimcp_stdp_rule_t* stdp_rule);

/**
 * @brief Initialize cognitive bridge
 */
int entorhinal_init_cognitive_bridge(nimcp_entorhinal_t* ec,
    working_memory_t* wm,
    attention_system_t* attention,
    cognitive_integration_hub_t* hub);

/**
 * @brief Initialize training bridge
 */
int entorhinal_init_training_bridge(nimcp_entorhinal_t* ec,
    nimcp_training_context_t* training_ctx);

/**
 * @brief Initialize neural substrate bridge
 */
int entorhinal_init_substrate_bridge(nimcp_entorhinal_t* ec,
    nimcp_neural_substrate_t* substrate);

/**
 * @brief Initialize prime resonance bridge
 */
int entorhinal_init_resonance_bridge(nimcp_entorhinal_t* ec,
    nimcp_prime_resonance_t* resonance);

/**
 * @brief Initialize thalamic bridge
 */
int entorhinal_init_thalamic_bridge(nimcp_entorhinal_t* ec,
    thalamus_adapter_t* thalamus);

/**
 * @brief Initialize hippocampus bridge
 */
int entorhinal_init_hippocampus_bridge(nimcp_entorhinal_t* ec,
    hippocampus_adapter_t* hippocampus);

/**
 * @brief Initialize perception bridge
 */
int entorhinal_init_perception_bridge(nimcp_entorhinal_t* ec,
    nimcp_perception_layer_t* perception);

/**
 * @brief Initialize swarm bridge
 */
int entorhinal_init_swarm_bridge(nimcp_entorhinal_t* ec,
    nimcp_swarm_coordinator_t* swarm);

/**
 * @brief Initialize dragonfly bridge
 */
int entorhinal_init_dragonfly_bridge(nimcp_entorhinal_t* ec,
    nimcp_dragonfly_system_t* dragonfly);

/**
 * @brief Initialize portia bridge
 */
int entorhinal_init_portia_bridge(nimcp_entorhinal_t* ec,
    nimcp_portia_system_t* portia);

/**
 * @brief Initialize cerebellum bridge
 */
int entorhinal_init_cerebellum_bridge(nimcp_entorhinal_t* ec,
    cerebellum_adapter_t* cerebellum);

/**
 * @brief Initialize medulla bridge
 */
int entorhinal_init_medulla_bridge(nimcp_entorhinal_t* ec,
    medulla_adapter_t* medulla);

/**
 * @brief Initialize omnidirectional bridge
 */
int entorhinal_init_omni_bridge(nimcp_entorhinal_t* ec,
    nimcp_omnidirectional_system_t* omni);

/**
 * @brief Initialize hypothalamus bridge
 */
int entorhinal_init_hypothalamus_bridge(nimcp_entorhinal_t* ec,
    hypothalamus_adapter_t* hypothalamus);

/**
 * @brief Initialize logic bridge
 */
int entorhinal_init_logic_bridge(nimcp_entorhinal_t* ec,
    nimcp_logic_system_t* logic);

/**
 * @brief Initialize brain KG bridge
 */
int entorhinal_init_kg_bridge(nimcp_entorhinal_t* ec,
    brain_kg_t* kg);

/**
 * @brief Initialize all bridges at once
 */
int entorhinal_init_all_bridges(nimcp_entorhinal_t* ec,
    nimcp_brain_t* brain);

/*=============================================================================
 * GRID CELL API
 *===========================================================================*/

/**
 * @brief Update grid cell activations based on position
 */
int entorhinal_update_grid_cells(nimcp_entorhinal_t* ec,
    const float* position, uint32_t dim);

/**
 * @brief Get grid cell population vector
 */
int entorhinal_get_grid_population_vector(const nimcp_entorhinal_t* ec,
    float* vector_out, uint32_t* dim);

/**
 * @brief Decode position from grid cell activity
 */
int entorhinal_decode_position_from_grid(const nimcp_entorhinal_t* ec,
    float* position_out, float* confidence_out);

/**
 * @brief Reset grid cell phases (after disorientation)
 */
int entorhinal_reset_grid_phases(nimcp_entorhinal_t* ec,
    const float* known_position);

/**
 * @brief Get grid cell at index
 */
const nimcp_grid_cell_t* entorhinal_get_grid_cell(const nimcp_entorhinal_t* ec,
    uint32_t module_idx, uint32_t cell_idx);

/*=============================================================================
 * BORDER CELL API
 *===========================================================================*/

/**
 * @brief Update border cell activations
 */
int entorhinal_update_border_cells(nimcp_entorhinal_t* ec,
    const float* boundary_distances, uint32_t num_boundaries);

/**
 * @brief Detect boundaries from border cell activity
 */
int entorhinal_detect_boundaries(const nimcp_entorhinal_t* ec,
    float* boundary_directions, float* boundary_distances,
    uint32_t max_boundaries, uint32_t* num_detected);

/*=============================================================================
 * HEAD DIRECTION CELL API
 *===========================================================================*/

/**
 * @brief Update head direction cells
 */
int entorhinal_update_hd_cells(nimcp_entorhinal_t* ec,
    float heading, float angular_velocity);

/**
 * @brief Decode heading from HD cell activity
 */
int entorhinal_decode_heading(const nimcp_entorhinal_t* ec,
    float* heading_out, float* confidence_out);

/**
 * @brief Calibrate HD cells with visual landmark
 */
int entorhinal_calibrate_hd_cells(nimcp_entorhinal_t* ec,
    float known_heading);

/*=============================================================================
 * PATH INTEGRATION API
 *===========================================================================*/

/**
 * @brief Update path integration with velocity input
 */
int entorhinal_path_integrate(nimcp_entorhinal_t* ec,
    const float* velocity, float angular_velocity, float dt);

/**
 * @brief Get current path integration estimate
 */
int entorhinal_get_position_estimate(const nimcp_entorhinal_t* ec,
    float* position_out, float* heading_out,
    float* position_confidence, float* heading_confidence);

/**
 * @brief Apply visual correction to path integration
 */
int entorhinal_apply_visual_correction(nimcp_entorhinal_t* ec,
    const float* visual_position, float visual_heading,
    float confidence);

/**
 * @brief Apply boundary correction to path integration
 */
int entorhinal_apply_boundary_correction(nimcp_entorhinal_t* ec,
    const float* boundary_position, float boundary_direction);

/*=============================================================================
 * MEMORY GATEWAY API
 *===========================================================================*/

/**
 * @brief Set memory gateway encoding gate
 */
int entorhinal_set_encoding_gate(nimcp_entorhinal_t* ec, float gate_value);

/**
 * @brief Set memory gateway retrieval gate
 */
int entorhinal_set_retrieval_gate(nimcp_entorhinal_t* ec, float gate_value);

/**
 * @brief Transfer memory to hippocampus (encoding)
 */
int entorhinal_encode_to_hippocampus(nimcp_entorhinal_t* ec,
    const float* features, uint32_t feature_dim,
    const float* spatial_context, uint32_t spatial_dim);

/**
 * @brief Retrieve memory from hippocampus
 */
int entorhinal_retrieve_from_hippocampus(nimcp_entorhinal_t* ec,
    const float* cue, uint32_t cue_dim,
    float* retrieved_features, uint32_t max_features,
    uint32_t* actual_features);

/**
 * @brief Consolidate memory to neocortex
 */
int entorhinal_consolidate_to_neocortex(nimcp_entorhinal_t* ec,
    uint32_t memory_id, float consolidation_strength);

/**
 * @brief Get memory gateway statistics
 */
int entorhinal_get_gateway_stats(const nimcp_entorhinal_t* ec,
    uint64_t* encoded, uint64_t* retrieved, uint64_t* consolidated);

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW API
 *===========================================================================*/

/**
 * @brief Process incoming data from all bridges
 */
int entorhinal_process_incoming(nimcp_entorhinal_t* ec);

/**
 * @brief Send outgoing data to all bridges
 */
int entorhinal_send_outgoing(nimcp_entorhinal_t* ec);

/**
 * @brief Full bidirectional update cycle
 */
int entorhinal_bidirectional_update(nimcp_entorhinal_t* ec, float dt);

/**
 * @brief Synchronize with bio-async system
 */
int entorhinal_sync_bio_async(nimcp_entorhinal_t* ec);

/**
 * @brief Process neuromodulator effects
 */
int entorhinal_process_neuromodulation(nimcp_entorhinal_t* ec);

/**
 * @brief Update substrate effects
 */
int entorhinal_update_substrate_effects(nimcp_entorhinal_t* ec);

/**
 * @brief Publish cognitive events
 */
int entorhinal_publish_cognitive_events(nimcp_entorhinal_t* ec);

/**
 * @brief Apply plasticity updates
 */
int entorhinal_apply_plasticity(nimcp_entorhinal_t* ec, float dt);

/**
 * @brief Run security validation
 */
int entorhinal_validate_security(nimcp_entorhinal_t* ec);

/**
 * @brief Run immune scan
 */
int entorhinal_immune_scan(nimcp_entorhinal_t* ec);

/*=============================================================================
 * TRAINING API
 *===========================================================================*/

/**
 * @brief Enable/disable training mode
 */
int entorhinal_set_training_mode(nimcp_entorhinal_t* ec, bool enabled);

/**
 * @brief Forward pass for training
 */
int entorhinal_training_forward(nimcp_entorhinal_t* ec,
    const float* input, uint32_t input_dim,
    float* output, uint32_t output_dim);

/**
 * @brief Backward pass for training
 */
int entorhinal_training_backward(nimcp_entorhinal_t* ec,
    const float* grad_output, uint32_t grad_dim);

/**
 * @brief Apply weight updates
 */
int entorhinal_apply_weight_updates(nimcp_entorhinal_t* ec,
    float learning_rate);

/**
 * @brief Get training loss
 */
float entorhinal_get_training_loss(const nimcp_entorhinal_t* ec);

/*=============================================================================
 * STATUS AND DIAGNOSTICS API
 *===========================================================================*/

/**
 * @brief Get current status
 */
entorhinal_status_t entorhinal_get_status(const nimcp_entorhinal_t* ec);

/**
 * @brief Get last error
 */
entorhinal_error_t entorhinal_get_last_error(const nimcp_entorhinal_t* ec);

/**
 * @brief Get error string
 */
const char* entorhinal_error_string(entorhinal_error_t error);

/**
 * @brief Get status string
 */
const char* entorhinal_status_string(entorhinal_status_t status);

/**
 * @brief Get comprehensive statistics
 */
int entorhinal_get_stats(const nimcp_entorhinal_t* ec, entorhinal_stats_t* stats);

/**
 * @brief Get configuration
 */
int entorhinal_get_config(const nimcp_entorhinal_t* ec, entorhinal_config_t* config);

/**
 * @brief Get health status (for brain KG)
 */
float entorhinal_get_health_status(const nimcp_entorhinal_t* ec);

/**
 * @brief Log diagnostic information
 */
int entorhinal_log_diagnostics(const nimcp_entorhinal_t* ec);

/*=============================================================================
 * SERIALIZATION API
 *===========================================================================*/

/**
 * @brief Serialize entorhinal state
 */
int entorhinal_serialize(const nimcp_entorhinal_t* ec,
    uint8_t* buffer, size_t buffer_size, size_t* bytes_written);

/**
 * @brief Deserialize entorhinal state
 */
int entorhinal_deserialize(nimcp_entorhinal_t* ec,
    const uint8_t* buffer, size_t buffer_size);

/**
 * @brief Get serialization size
 */
size_t entorhinal_get_serialization_size(const nimcp_entorhinal_t* ec);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENTORHINAL_H */
