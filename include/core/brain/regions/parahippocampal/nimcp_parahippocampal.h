/**
 * @file nimcp_parahippocampal.h
 * @brief Parahippocampal Cortex - Scene and Context Processing
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * WHAT: Parahippocampal cortex implementation with scene recognition,
 *       spatial layout processing, and contextual memory.
 *
 * WHY:  The parahippocampal cortex is critical for scene/place recognition
 *       and provides the "where" pathway to the medial temporal lobe memory system.
 *
 * HOW:  Implements place cells, scene recognition, spatial layout encoding,
 *       and contextual associations with bidirectional entorhinal integration.
 *
 * BIOLOGICAL BASIS:
 * - Place cells: Respond to specific locations/scenes
 * - Scene-selective neurons: View-invariant scene representations
 * - Spatial layout cells: Encode geometric structure of environments
 * - Context neurons: Bind multiple elements into coherent contexts
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
 * - Perception Layer: Visual scene input, feature extraction
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
 * - Entorhinal Cortex: Grid cells, spatial context
 * - Perirhinal Cortex: Object identity, familiarity
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PARAHIPPOCAMPAL_H
#define NIMCP_PARAHIPPOCAMPAL_H

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
/* Handler - parahippocampal-specific opaque type */
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
typedef struct nimcp_perirhinal nimcp_perirhinal_t;
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

#define PARAHIPP_DEFAULT_PLACE_CELLS            256
#define PARAHIPP_DEFAULT_SCENE_CELLS            512
#define PARAHIPP_DEFAULT_LAYOUT_CELLS           128
#define PARAHIPP_DEFAULT_CONTEXT_CELLS          256
#define PARAHIPP_DEFAULT_LANDMARK_CELLS         64
#define PARAHIPP_DEFAULT_SCENE_DIM              512
#define PARAHIPP_DEFAULT_LAYOUT_DIM             128
#define PARAHIPP_DEFAULT_CONTEXT_DIM            256
#define PARAHIPP_DEFAULT_MAX_STORED_SCENES      512
#define PARAHIPP_DEFAULT_MAX_LANDMARKS          128
#define PARAHIPP_SCENE_MATCH_THRESHOLD          0.6f
#define PARAHIPP_CONTEXT_BINDING_THRESHOLD      0.5f

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Parahippocampal cortex processing status
 */
typedef enum {
    PARAHIPP_STATUS_IDLE = 0,
    PARAHIPP_STATUS_SCENE_ENCODING,
    PARAHIPP_STATUS_SCENE_RECOGNIZING,
    PARAHIPP_STATUS_LAYOUT_PROCESSING,
    PARAHIPP_STATUS_CONTEXT_BINDING,
    PARAHIPP_STATUS_LANDMARK_PROCESSING,
    PARAHIPP_STATUS_CONSOLIDATING,
    PARAHIPP_STATUS_READY,
    PARAHIPP_STATUS_ERROR
} parahipp_status_t;

/**
 * @brief Error codes
 */
typedef enum {
    PARAHIPP_ERROR_NONE = 0,
    PARAHIPP_ERROR_INVALID_INPUT,
    PARAHIPP_ERROR_SCENE_NOT_FOUND,
    PARAHIPP_ERROR_MEMORY_FULL,
    PARAHIPP_ERROR_ENCODING_FAILED,
    PARAHIPP_ERROR_RECOGNITION_FAILED,
    PARAHIPP_ERROR_SECURITY_VIOLATION,
    PARAHIPP_ERROR_IMMUNE_REJECTION,
    PARAHIPP_ERROR_SUBSTRATE_DEPLETED,
    PARAHIPP_ERROR_SYNC_FAILURE,
    PARAHIPP_ERROR_BUFFER_OVERFLOW,
    PARAHIPP_ERROR_INTERNAL
} parahipp_error_t;

/**
 * @brief Scene category types
 */
typedef enum {
    SCENE_CATEGORY_UNKNOWN = 0,
    SCENE_CATEGORY_INDOOR,
    SCENE_CATEGORY_OUTDOOR_NATURAL,
    SCENE_CATEGORY_OUTDOOR_URBAN,
    SCENE_CATEGORY_CORRIDOR,
    SCENE_CATEGORY_OPEN_SPACE,
    SCENE_CATEGORY_ENCLOSED,
    SCENE_CATEGORY_TRANSITIONAL,
    SCENE_CATEGORY_COUNT
} scene_category_t;

/**
 * @brief Spatial layout type
 */
typedef enum {
    LAYOUT_TYPE_UNKNOWN = 0,
    LAYOUT_TYPE_RECTANGULAR,
    LAYOUT_TYPE_CIRCULAR,
    LAYOUT_TYPE_IRREGULAR,
    LAYOUT_TYPE_CORRIDOR,
    LAYOUT_TYPE_OPEN,
    LAYOUT_TYPE_COMPLEX,
    LAYOUT_TYPE_COUNT
} layout_type_t;

/**
 * @brief Context state types
 */
typedef enum {
    CONTEXT_STATE_NOVEL = 0,
    CONTEXT_STATE_FAMILIAR,
    CONTEXT_STATE_CHANGED,
    CONTEXT_STATE_STABLE,
    CONTEXT_STATE_TRANSITIONING
} context_state_t;

/**
 * @brief Neuromodulator channel types for bio-async
 */
typedef enum {
    PARAHIPP_CHANNEL_DOPAMINE = 0,
    PARAHIPP_CHANNEL_SEROTONIN,
    PARAHIPP_CHANNEL_NOREPINEPHRINE,
    PARAHIPP_CHANNEL_ACETYLCHOLINE,
    PARAHIPP_CHANNEL_COUNT
} parahipp_neuromod_channel_t;

/*=============================================================================
 * PLACE CELL STRUCTURES
 *===========================================================================*/

/**
 * @brief Place cell - responds to specific locations
 */
typedef struct {
    uint32_t cell_id;
    float place_field_center[3];    /* Center of place field (x, y, z) */
    float place_field_radius;       /* Radius of place field */
    float activation;               /* Current activation [0, 1] */
    float peak_rate;                /* Maximum firing rate */

    /* Tuning properties */
    float directional_tuning;       /* Direction selectivity */
    float size_invariance;          /* Invariance to field size */

    /* Associated context */
    uint32_t associated_scene_id;
    float context_specificity;

    /* Plasticity state */
    float eligibility_trace;
    float learning_rate;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
    float last_spike_time;
} nimcp_place_cell_t;

/*=============================================================================
 * SCENE CELL STRUCTURES
 *===========================================================================*/

/**
 * @brief Scene-selective cell - responds to specific scenes/views
 */
typedef struct {
    uint32_t cell_id;
    uint32_t preferred_scene_id;    /* Scene this cell prefers */
    float selectivity;              /* How selective for this scene */
    float view_invariance;          /* Degree of view invariance */
    float activation;               /* Current activation [0, 1] */

    /* Feature tuning */
    float* scene_weights;           /* Weights to scene features */
    uint32_t scene_dim;

    /* Category selectivity */
    scene_category_t preferred_category;
    float category_selectivity;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
} nimcp_scene_cell_t;

/*=============================================================================
 * SPATIAL LAYOUT STRUCTURES
 *===========================================================================*/

/**
 * @brief Spatial layout cell - encodes geometric structure
 */
typedef struct {
    uint32_t cell_id;
    layout_type_t preferred_layout;
    float activation;

    /* Boundary encoding */
    float* boundary_distances;      /* Distance to boundaries at angles */
    uint32_t num_angles;
    float openness;                 /* 0 = enclosed, 1 = open */

    /* Geometric features */
    float aspect_ratio;
    float symmetry;
    float complexity;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
} nimcp_layout_cell_t;

/**
 * @brief Spatial layout representation
 */
typedef struct {
    layout_type_t type;
    float boundaries[360];          /* Distance to boundary at each degree */
    float openness;
    float navigability;
    float* geometric_features;
    uint32_t feature_dim;
    float center[3];
    float extent[3];                /* Bounding box extent */
} nimcp_spatial_layout_t;

/*=============================================================================
 * CONTEXT CELL STRUCTURES
 *===========================================================================*/

/**
 * @brief Context cell - binds multiple elements into coherent context
 */
typedef struct {
    uint32_t cell_id;
    float activation;

    /* Context binding */
    float* context_vector;          /* Current context representation */
    uint32_t context_dim;
    float binding_strength;

    /* Temporal context */
    float temporal_stability;
    float change_detection;
    uint64_t last_change_ms;

    /* Multi-modal binding */
    float spatial_weight;
    float object_weight;
    float temporal_weight;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
} nimcp_context_cell_t;

/*=============================================================================
 * LANDMARK STRUCTURES
 *===========================================================================*/

/**
 * @brief Landmark cell - responds to navigational landmarks
 */
typedef struct {
    uint32_t cell_id;
    uint32_t landmark_id;
    float activation;

    /* Landmark properties */
    float position[3];
    float salience;
    float permanence;               /* How permanent/stable the landmark is */
    float visibility_range;

    /* Directional cue */
    float bearing;                  /* Bearing from current position */
    float distance;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
} nimcp_landmark_cell_t;

/**
 * @brief Stored landmark representation
 */
typedef struct {
    uint32_t landmark_id;
    char name[64];
    float position[3];
    float* visual_features;
    uint32_t feature_dim;
    float salience;
    float reliability;
    float visibility_range;     /**< Max distance visible */
    uint64_t first_seen_ms;
    uint64_t last_seen_ms;
    uint32_t encounter_count;
} nimcp_stored_landmark_t;

/*=============================================================================
 * STORED SCENE STRUCTURES
 *===========================================================================*/

/**
 * @brief Stored scene representation
 */
typedef struct {
    uint32_t scene_id;
    char name[64];

    /* Visual representation */
    float* scene_features;
    uint32_t feature_dim;

    /* Category and layout */
    scene_category_t category;
    nimcp_spatial_layout_t layout;

    /* Context binding */
    float* context_vector;
    uint32_t context_dim;
    float context_stability;

    /* Associated elements */
    uint32_t* landmark_ids;
    uint32_t num_landmarks;
    uint32_t* object_ids;           /* From perirhinal */
    uint32_t num_objects;

    /* Spatial position */
    float position[3];              /* Position where scene was encoded */
    float heading;                  /* Heading when encoded */

    /* Memory properties */
    float familiarity;
    float recency;
    uint64_t first_encoded_ms;
    uint64_t last_visited_ms;
    uint32_t visit_count;

    /* View-specific data */
    float** view_features;
    float* view_headings;
    uint32_t num_views;
} nimcp_stored_scene_t;

/*=============================================================================
 * SCENE RECOGNITION RESULT
 *===========================================================================*/

/**
 * @brief Result of scene recognition
 */
typedef struct {
    uint32_t scene_id;
    float match_confidence;
    scene_category_t category;
    layout_type_t layout_type;

    /* Familiarity signals */
    float familiarity;
    bool is_novel;
    context_state_t context_state;

    /* Spatial information */
    float estimated_position[3];
    float position_confidence;

    /* Landmark matches */
    uint32_t* matched_landmarks;
    float* landmark_confidences;
    uint32_t num_matched_landmarks;

    /* Alternative matches */
    uint32_t* alternative_ids;
    float* alternative_confidences;
    uint32_t num_alternatives;
} parahipp_recognition_result_t;

/*=============================================================================
 * BRIDGE STATE STRUCTURES
 *===========================================================================*/

/**
 * @brief Entorhinal bridge state
 */
typedef struct {
    nimcp_entorhinal_t* entorhinal;
    float grid_cell_input_weight;
    float spatial_context_weight;
    uint64_t items_transferred;
} parahipp_entorhinal_bridge_t;

/**
 * @brief Perirhinal bridge state
 */
typedef struct {
    nimcp_perirhinal_t* perirhinal;
    float object_context_weight;
    float scene_object_binding;
    uint64_t items_transferred;
} parahipp_perirhinal_bridge_t;

/**
 * @brief Security bridge state
 */
typedef struct {
    nimcp_security_context_t* security_ctx;
    nimcp_access_control_t* access_control;
    uint32_t access_level;
    bool threat_detected;
    float threat_level;
} parahipp_security_bridge_t;

/**
 * @brief Immune bridge state
 */
typedef struct {
    brain_immune_system_t* immune;
    float health_score;
    bool anomaly_detected;
    float inflammation_level;
} parahipp_immune_bridge_t;

/**
 * @brief Bio-async bridge state
 */
typedef struct {
    nimcp_bio_async_handler_t* bio_handler;
    nimcp_bio_router_t* router;
    float neuromodulator_levels[PARAHIPP_CHANNEL_COUNT];
    uint32_t pending_messages;
    uint64_t messages_processed;
} parahipp_bio_async_bridge_t;

/**
 * @brief SNN bridge state
 */
typedef struct {
    nimcp_snn_network_t* snn;
    uint32_t place_layer_id;
    uint32_t scene_layer_id;
    uint32_t context_layer_id;
    uint32_t output_layer_id;
    float spike_rate;
} parahipp_snn_bridge_t;

/**
 * @brief Plasticity bridge state
 */
typedef struct {
    nimcp_plasticity_manager_t* plasticity;
    nimcp_stdp_rule_t* stdp_rule;
    float learning_rate;
    float weight_decay;
    uint64_t weight_updates;
} parahipp_plasticity_bridge_t;

/**
 * @brief Cognitive bridge state
 */
typedef struct {
    working_memory_t* working_memory;
    attention_system_t* attention;
    cognitive_integration_hub_t* hub;
    float attention_modulation;
    uint32_t cognitive_events_sent;
} parahipp_cognitive_bridge_t;

/**
 * @brief Training bridge state
 */
typedef struct {
    nimcp_training_context_t* training_ctx;
    nimcp_backprop_adapter_t* backprop;
    bool training_enabled;
    float current_loss;
    uint64_t training_steps;
} parahipp_training_bridge_t;

/**
 * @brief Neural substrate bridge state
 */
typedef struct {
    nimcp_neural_substrate_t* substrate;
    float atp_level;
    float oxygen_level;
    float glucose_level;
    float metabolic_rate;
} parahipp_substrate_bridge_t;

/**
 * @brief Prime resonance bridge state
 */
typedef struct {
    nimcp_prime_resonance_t* resonance;
    float theta_phase;
    float gamma_phase;
    float phase_lock_strength;
} parahipp_resonance_bridge_t;

/**
 * @brief Thalamic bridge state
 */
typedef struct {
    thalamus_adapter_t* thalamus;
    float relay_gain;
    float attention_gate;
} parahipp_thalamic_bridge_t;

/**
 * @brief Hippocampus bridge state
 */
typedef struct {
    hippocampus_adapter_t* hippocampus;
    float place_cell_coupling;
    float memory_encoding_strength;
    float retrieval_cue_strength;
} parahipp_hippocampus_bridge_t;

/**
 * @brief Perception bridge state
 */
typedef struct {
    nimcp_perception_layer_t* perception;
    float* visual_input;
    uint32_t visual_dim;
    float scene_salience;
} parahipp_perception_bridge_t;

/**
 * @brief Omnidirectional bridge state
 */
typedef struct {
    nimcp_omnidirectional_system_t* omni;
    float* spatial_panorama;
    uint32_t panorama_resolution;
    float current_heading;
} parahipp_omni_bridge_t;

/**
 * @brief Hypothalamus bridge state
 */
typedef struct {
    hypothalamus_adapter_t* hypothalamus;
    float motivation_signal;
    float contextual_reward;
    float safety_assessment;
} parahipp_hypothalamus_bridge_t;

/**
 * @brief Logic system bridge state
 */
typedef struct {
    nimcp_logic_system_t* logic;
    bool spatial_constraints_satisfied;
    float inference_confidence;
} parahipp_logic_bridge_t;

/**
 * @brief Brain KG bridge state
 */
typedef struct {
    nimcp_brain_kg_t* kg;
    uint32_t node_id;
    float health_status;
} parahipp_kg_bridge_t;

/*=============================================================================
 * MAIN CONFIGURATION STRUCTURE
 *===========================================================================*/

/**
 * @brief Comprehensive parahippocampal cortex configuration
 */
typedef struct {
    /* Place cell parameters */
    uint32_t num_place_cells;
    float place_field_radius_min;
    float place_field_radius_max;

    /* Scene cell parameters */
    uint32_t num_scene_cells;
    float scene_selectivity;
    float view_invariance_target;

    /* Layout cell parameters */
    uint32_t num_layout_cells;
    uint32_t boundary_angles;

    /* Context cell parameters */
    uint32_t num_context_cells;
    float context_binding_rate;
    float context_decay_rate;

    /* Landmark cell parameters */
    uint32_t num_landmark_cells;
    uint32_t max_landmarks;

    /* Memory parameters */
    uint32_t max_stored_scenes;
    uint32_t scene_dim;
    uint32_t layout_dim;
    uint32_t context_dim;
    uint32_t max_views_per_scene;

    /* Recognition parameters */
    float scene_match_threshold;
    float context_match_threshold;
    uint32_t max_alternatives;

    /* Integration enables */
    bool enable_entorhinal;
    bool enable_perirhinal;
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
    bool enable_omni;
    bool enable_hypothalamus;
    bool enable_logic;
    bool enable_kg;

    /* Processing options */
    bool enable_view_invariance;
    bool enable_layout_processing;
    bool enable_landmark_tracking;
    bool enable_context_binding;

    /* Learning parameters */
    float learning_rate;
    float weight_decay;
    float eligibility_decay;

    /* Oscillation parameters */
    float theta_frequency;
    float gamma_frequency;
} parahipp_config_t;

/*=============================================================================
 * MAIN PARAHIPPOCAMPAL CORTEX STRUCTURE
 *===========================================================================*/

/**
 * @brief Complete parahippocampal cortex adapter
 */
typedef struct nimcp_parahippocampal {
    /* Configuration */
    parahipp_config_t config;

    /* Status */
    parahipp_status_t status;
    parahipp_error_t last_error;

    /* Place cells */
    nimcp_place_cell_t* place_cells;
    uint32_t num_place_cells;

    /* Scene cells */
    nimcp_scene_cell_t* scene_cells;
    uint32_t num_scene_cells;

    /* Layout cells */
    nimcp_layout_cell_t* layout_cells;
    uint32_t num_layout_cells;

    /* Context cells */
    nimcp_context_cell_t* context_cells;
    uint32_t num_context_cells;

    /* Landmark cells */
    nimcp_landmark_cell_t* landmark_cells;
    uint32_t num_landmark_cells;

    /* Stored scenes */
    nimcp_stored_scene_t* stored_scenes;
    uint32_t num_stored_scenes;
    uint32_t max_stored_scenes;

    /* Stored landmarks */
    nimcp_stored_landmark_t* stored_landmarks;
    uint32_t num_stored_landmarks;
    uint32_t max_landmarks;

    /* Current processing state */
    float* current_scene_input;
    uint32_t current_input_dim;
    nimcp_spatial_layout_t current_layout;
    float* current_context;
    uint32_t current_context_dim;
    float current_position[3];
    float current_heading;

    /*=========================================================================
     * ALL INTEGRATION BRIDGES
     *=======================================================================*/

    parahipp_entorhinal_bridge_t entorhinal_bridge;
    parahipp_perirhinal_bridge_t perirhinal_bridge;
    parahipp_security_bridge_t security_bridge;
    parahipp_immune_bridge_t immune_bridge;
    parahipp_bio_async_bridge_t bio_async_bridge;
    parahipp_snn_bridge_t snn_bridge;
    parahipp_plasticity_bridge_t plasticity_bridge;
    parahipp_cognitive_bridge_t cognitive_bridge;
    parahipp_training_bridge_t training_bridge;
    parahipp_substrate_bridge_t substrate_bridge;
    parahipp_resonance_bridge_t resonance_bridge;
    parahipp_thalamic_bridge_t thalamic_bridge;
    parahipp_hippocampus_bridge_t hippocampus_bridge;
    parahipp_perception_bridge_t perception_bridge;
    parahipp_omni_bridge_t omni_bridge;
    parahipp_hypothalamus_bridge_t hypothalamus_bridge;
    parahipp_logic_bridge_t logic_bridge;
    parahipp_kg_bridge_t kg_bridge;

    /* Logging and metrics */
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;

    /* Statistics */
    uint64_t updates_processed;
    uint64_t scenes_encoded;
    uint64_t scenes_recognized;
    uint64_t context_switches;
    float mean_recognition_confidence;
    double total_processing_time_ms;

    /* Timing */
    uint64_t creation_time_ms;
    uint64_t last_update_ms;
    float simulation_dt_ms;
} nimcp_parahippocampal_t;

/*=============================================================================
 * STATISTICS STRUCTURE
 *===========================================================================*/

/**
 * @brief Comprehensive statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t updates_processed;
    uint64_t scenes_encoded;
    uint64_t scenes_recognized;
    uint64_t landmarks_tracked;
    uint64_t context_switches;
    uint64_t layout_computations;

    /* Place cell statistics */
    float mean_place_activation;
    uint32_t active_place_cells;
    float place_field_coverage;

    /* Scene cell statistics */
    float mean_scene_activation;
    float scene_recognition_accuracy;

    /* Context statistics */
    float mean_context_stability;
    float context_change_rate;
    uint64_t context_bindings;

    /* Memory statistics */
    uint32_t total_stored_scenes;
    uint32_t total_stored_landmarks;
    float memory_utilization;

    /* Recognition statistics */
    float recognition_accuracy;
    float mean_recognition_latency_ms;
    float mean_recognition_confidence;

    /* Integration statistics */
    uint64_t entorhinal_transfers;
    uint64_t perirhinal_transfers;
    uint64_t cognitive_events_published;
} parahipp_stats_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
parahipp_config_t parahipp_default_config(void);

/**
 * @brief Create parahippocampal cortex instance
 */
nimcp_parahippocampal_t* parahipp_create(const parahipp_config_t* config);

/**
 * @brief Destroy parahippocampal cortex instance
 */
void parahipp_destroy(nimcp_parahippocampal_t* ph);

/**
 * @brief Reset to initial state
 */
int parahipp_reset(nimcp_parahippocampal_t* ph);

/**
 * @brief Main update function
 */
int parahipp_update(nimcp_parahippocampal_t* ph, float dt);

/*=============================================================================
 * BRIDGE INITIALIZATION API
 *===========================================================================*/

int parahipp_init_entorhinal_bridge(nimcp_parahippocampal_t* ph, nimcp_entorhinal_t* ec);
int parahipp_init_perirhinal_bridge(nimcp_parahippocampal_t* ph, nimcp_perirhinal_t* pr);
int parahipp_init_security_bridge(nimcp_parahippocampal_t* ph, nimcp_security_context_t* ctx, nimcp_access_control_t* ac);
int parahipp_init_immune_bridge(nimcp_parahippocampal_t* ph, brain_immune_system_t* immune);
int parahipp_init_bio_async_bridge(nimcp_parahippocampal_t* ph, nimcp_bio_router_t* router);
int parahipp_init_snn_bridge(nimcp_parahippocampal_t* ph, nimcp_snn_network_t* snn);
int parahipp_init_plasticity_bridge(nimcp_parahippocampal_t* ph, nimcp_plasticity_manager_t* plasticity, nimcp_stdp_rule_t* stdp);
int parahipp_init_cognitive_bridge(nimcp_parahippocampal_t* ph, working_memory_t* wm, attention_system_t* attention, cognitive_integration_hub_t* hub);
int parahipp_init_training_bridge(nimcp_parahippocampal_t* ph, nimcp_training_context_t* ctx);
int parahipp_init_substrate_bridge(nimcp_parahippocampal_t* ph, nimcp_neural_substrate_t* substrate);
int parahipp_init_resonance_bridge(nimcp_parahippocampal_t* ph, nimcp_prime_resonance_t* resonance);
int parahipp_init_thalamic_bridge(nimcp_parahippocampal_t* ph, thalamus_adapter_t* thalamus);
int parahipp_init_hippocampus_bridge(nimcp_parahippocampal_t* ph, hippocampus_adapter_t* hippocampus);
int parahipp_init_perception_bridge(nimcp_parahippocampal_t* ph, nimcp_perception_layer_t* perception);
int parahipp_init_omni_bridge(nimcp_parahippocampal_t* ph, nimcp_omnidirectional_system_t* omni);
int parahipp_init_hypothalamus_bridge(nimcp_parahippocampal_t* ph, hypothalamus_adapter_t* hypothalamus);
int parahipp_init_all_bridges(nimcp_parahippocampal_t* ph, nimcp_brain_t* brain);

/*=============================================================================
 * SCENE ENCODING/RECOGNITION API
 *===========================================================================*/

/**
 * @brief Encode a new scene
 */
int parahipp_encode_scene(nimcp_parahippocampal_t* ph,
    const float* scene_features, uint32_t feature_dim,
    const float* position, float heading,
    const char* name, uint32_t* scene_id_out);

/**
 * @brief Recognize a scene from visual features
 */
int parahipp_recognize_scene(nimcp_parahippocampal_t* ph,
    const float* scene_features, uint32_t feature_dim,
    parahipp_recognition_result_t* result);

/**
 * @brief Add a new view to an existing scene
 */
int parahipp_add_scene_view(nimcp_parahippocampal_t* ph,
    uint32_t scene_id, const float* view_features, uint32_t feature_dim,
    float heading);

/**
 * @brief Get scene by ID
 */
const nimcp_stored_scene_t* parahipp_get_scene(const nimcp_parahippocampal_t* ph,
    uint32_t scene_id);

/**
 * @brief Update scene visit (after recognition)
 */
int parahipp_update_scene_visit(nimcp_parahippocampal_t* ph,
    uint32_t scene_id);

/**
 * @brief Forget a scene
 */
int parahipp_forget_scene(nimcp_parahippocampal_t* ph,
    uint32_t scene_id);

/*=============================================================================
 * PLACE CELL API
 *===========================================================================*/

/**
 * @brief Update place cell activations based on position
 */
int parahipp_update_place_cells(nimcp_parahippocampal_t* ph,
    const float* position, uint32_t dim);

/**
 * @brief Get place cell population vector
 */
int parahipp_get_place_population_vector(const nimcp_parahippocampal_t* ph,
    float* vector_out, uint32_t* dim);

/**
 * @brief Decode position from place cell activity
 */
int parahipp_decode_position(const nimcp_parahippocampal_t* ph,
    float* position_out, float* confidence_out);

/**
 * @brief Get active place cells
 */
int parahipp_get_active_place_cells(const nimcp_parahippocampal_t* ph,
    uint32_t* cell_ids, float* activations, uint32_t max_cells, uint32_t* num_active);

/*=============================================================================
 * SPATIAL LAYOUT API
 *===========================================================================*/

/**
 * @brief Process spatial layout from boundary information
 */
int parahipp_process_layout(nimcp_parahippocampal_t* ph,
    const float* boundary_distances, uint32_t num_angles);

/**
 * @brief Get current layout type
 */
layout_type_t parahipp_get_layout_type(const nimcp_parahippocampal_t* ph);

/**
 * @brief Get layout features
 */
int parahipp_get_layout_features(const nimcp_parahippocampal_t* ph,
    float* features_out, uint32_t max_dim);

/**
 * @brief Get openness estimate
 */
float parahipp_get_openness(const nimcp_parahippocampal_t* ph);

/**
 * @brief Get navigability estimate
 */
float parahipp_get_navigability(const nimcp_parahippocampal_t* ph);

/*=============================================================================
 * CONTEXT API
 *===========================================================================*/

/**
 * @brief Get current context vector
 */
int parahipp_get_current_context(const nimcp_parahippocampal_t* ph,
    float* context_out, uint32_t max_dim);

/**
 * @brief Set context from external source
 */
int parahipp_set_context(nimcp_parahippocampal_t* ph,
    const float* context, uint32_t dim);

/**
 * @brief Detect context change
 */
bool parahipp_detect_context_change(const nimcp_parahippocampal_t* ph);

/**
 * @brief Get context stability
 */
float parahipp_get_context_stability(const nimcp_parahippocampal_t* ph);

/**
 * @brief Get context state
 */
context_state_t parahipp_get_context_state(const nimcp_parahippocampal_t* ph);

/**
 * @brief Bind context to scene
 */
int parahipp_bind_context_to_scene(nimcp_parahippocampal_t* ph,
    uint32_t scene_id);

/*=============================================================================
 * LANDMARK API
 *===========================================================================*/

/**
 * @brief Add a landmark
 */
int parahipp_add_landmark(nimcp_parahippocampal_t* ph,
    const float* visual_features, uint32_t feature_dim,
    const float* position, const char* name, uint32_t* landmark_id_out);

/**
 * @brief Recognize landmarks in current view
 */
int parahipp_recognize_landmarks(nimcp_parahippocampal_t* ph,
    const float* visual_features, uint32_t feature_dim,
    uint32_t* landmark_ids, float* confidences,
    uint32_t max_landmarks, uint32_t* num_recognized);

/**
 * @brief Get landmark by ID
 */
const nimcp_stored_landmark_t* parahipp_get_landmark(const nimcp_parahippocampal_t* ph,
    uint32_t landmark_id);

/**
 * @brief Update landmark cells from current view
 */
int parahipp_update_landmark_cells(nimcp_parahippocampal_t* ph,
    const float* current_position);

/**
 * @brief Get bearing to landmark
 */
float parahipp_get_landmark_bearing(const nimcp_parahippocampal_t* ph,
    uint32_t landmark_id, const float* from_position);

/*=============================================================================
 * SCENE-OBJECT BINDING API (Perirhinal Integration)
 *===========================================================================*/

/**
 * @brief Bind objects to scene
 */
int parahipp_bind_objects_to_scene(nimcp_parahippocampal_t* ph,
    uint32_t scene_id, const uint32_t* object_ids, uint32_t num_objects);

/**
 * @brief Get objects associated with scene
 */
int parahipp_get_scene_objects(const nimcp_parahippocampal_t* ph,
    uint32_t scene_id, uint32_t* object_ids, uint32_t max_objects,
    uint32_t* num_objects);

/**
 * @brief Find scenes containing object
 */
int parahipp_find_scenes_with_object(const nimcp_parahippocampal_t* ph,
    uint32_t object_id, uint32_t* scene_ids, uint32_t max_scenes,
    uint32_t* num_scenes);

/*=============================================================================
 * ENTORHINAL/PERIRHINAL INTEGRATION API
 *===========================================================================*/

/**
 * @brief Send spatial context to entorhinal
 */
int parahipp_send_to_entorhinal(nimcp_parahippocampal_t* ph);

/**
 * @brief Receive grid cell input from entorhinal
 */
int parahipp_receive_from_entorhinal(nimcp_parahippocampal_t* ph,
    const float* grid_input, uint32_t dim);

/**
 * @brief Send context to perirhinal
 */
int parahipp_send_to_perirhinal(nimcp_parahippocampal_t* ph);

/**
 * @brief Receive object info from perirhinal
 */
int parahipp_receive_from_perirhinal(nimcp_parahippocampal_t* ph,
    const uint32_t* object_ids, uint32_t num_objects);

/**
 * @brief Synchronize with entorhinal
 */
int parahipp_sync_entorhinal(nimcp_parahippocampal_t* ph);

/**
 * @brief Synchronize with perirhinal
 */
int parahipp_sync_perirhinal(nimcp_parahippocampal_t* ph);

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW API
 *===========================================================================*/

int parahipp_process_incoming(nimcp_parahippocampal_t* ph);
int parahipp_send_outgoing(nimcp_parahippocampal_t* ph);
int parahipp_bidirectional_update(nimcp_parahippocampal_t* ph, float dt);
int parahipp_process_visual_input(nimcp_parahippocampal_t* ph,
    const float* visual_features, uint32_t feature_dim);

/*=============================================================================
 * STATUS AND DIAGNOSTICS API
 *===========================================================================*/

parahipp_status_t parahipp_get_status(const nimcp_parahippocampal_t* ph);
parahipp_error_t parahipp_get_last_error(const nimcp_parahippocampal_t* ph);
const char* parahipp_error_string(parahipp_error_t error);
const char* parahipp_status_string(parahipp_status_t status);
int parahipp_get_stats(const nimcp_parahippocampal_t* ph, parahipp_stats_t* stats);
int parahipp_get_config(const nimcp_parahippocampal_t* ph, parahipp_config_t* config);
float parahipp_get_health_status(const nimcp_parahippocampal_t* ph);
int parahipp_log_diagnostics(const nimcp_parahippocampal_t* ph);

/* Cell activity getters */
size_t parahipp_get_place_cell_activity(const nimcp_parahippocampal_t* ph, float* activity, size_t max_cells);
size_t parahipp_get_scene_cell_activity(const nimcp_parahippocampal_t* ph, float* activity, size_t max_cells);
size_t parahipp_get_context_cell_activity(const nimcp_parahippocampal_t* ph, float* activity, size_t max_cells);

/*=============================================================================
 * SERIALIZATION API
 *===========================================================================*/

int parahipp_serialize(const nimcp_parahippocampal_t* ph,
    uint8_t* buffer, size_t buffer_size, size_t* bytes_written);
int parahipp_deserialize(nimcp_parahippocampal_t* ph,
    const uint8_t* buffer, size_t buffer_size);
size_t parahipp_get_serialization_size(const nimcp_parahippocampal_t* ph);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARAHIPPOCAMPAL_H */
