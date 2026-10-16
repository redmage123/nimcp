/**
 * @file nimcp_mammillary.h
 * @brief Mammillary Bodies - Memory Consolidation Relay and Papez Circuit Node
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * The mammillary bodies are paired structures at the posterior end of the
 * hypothalamus that serve as critical relay stations in the Papez circuit.
 * They receive input from the hippocampus via the fornix and project to the
 * anterior thalamus via the mammillothalamic tract.
 *
 * Key Functions:
 * - Memory consolidation relay (hippocampus → thalamus)
 * - Head direction signal processing and relay
 * - Spatial memory encoding support
 * - Episodic memory consolidation
 * - Papez circuit integration
 *
 * Bidirectional Integration:
 * - Full integration with all NIMCP subsystems
 * - Real-time synchronization with hippocampus and thalamus
 * - Head direction cell coordination with vestibular system
 */

#ifndef NIMCP_MAMMILLARY_H
#define NIMCP_MAMMILLARY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define MAMMILLARY_DEFAULT_HD_CELLS         64   /* Head direction cells */
#define MAMMILLARY_DEFAULT_RELAY_CELLS      128  /* Memory relay cells */
#define MAMMILLARY_DEFAULT_SPATIAL_CELLS    64   /* Spatial processing cells */
#define MAMMILLARY_MAX_MEMORY_TRACES        512  /* Maximum memory traces */
#define MAMMILLARY_MAX_HD_TUNING_WIDTH      60.0f /* Degrees */
#define MAMMILLARY_MIN_HD_TUNING_WIDTH      15.0f /* Degrees */
#define MAMMILLARY_CONSOLIDATION_THRESHOLD  0.7f
#define MAMMILLARY_VERSION_MAJOR            1
#define MAMMILLARY_VERSION_MINOR            0
#define MAMMILLARY_VERSION_PATCH            0

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

/* Brain region adapters */
typedef struct hippocampus_adapter hippocampus_adapter_t;
typedef struct thalamus_adapter thalamus_adapter_t;
typedef struct nimcp_entorhinal nimcp_entorhinal_t;
typedef struct hypothalamus_adapter hypothalamus_adapter_t;
typedef struct cerebellum_adapter cerebellum_adapter_t;
typedef struct medulla_adapter medulla_adapter_t;
typedef struct cingulate_adapter cingulate_adapter_t;

/* Specialized systems */
typedef struct nimcp_prime_resonance nimcp_prime_resonance_t;
typedef struct nimcp_swarm_coordinator nimcp_swarm_coordinator_t;
typedef struct nimcp_dragonfly_system nimcp_dragonfly_system_t;

/* Substrate and layers */
typedef struct nimcp_neural_substrate nimcp_neural_substrate_t;
typedef struct nimcp_perception_layer nimcp_perception_layer_t;

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Mammillary bodies operational status
 */
typedef enum {
    MAMMILLARY_STATUS_IDLE = 0,
    MAMMILLARY_STATUS_READY,
    MAMMILLARY_STATUS_RELAYING,
    MAMMILLARY_STATUS_CONSOLIDATING,
    MAMMILLARY_STATUS_HD_PROCESSING,
    MAMMILLARY_STATUS_SPATIAL_ENCODING,
    MAMMILLARY_STATUS_ERROR
} mammillary_status_t;

/**
 * @brief Mammillary bodies error codes
 */
typedef enum {
    MAMMILLARY_ERROR_NONE = 0,
    MAMMILLARY_ERROR_INVALID_INPUT,
    MAMMILLARY_ERROR_MEMORY_FULL,
    MAMMILLARY_ERROR_RELAY_FAILED,
    MAMMILLARY_ERROR_HD_DRIFT,
    MAMMILLARY_ERROR_CONSOLIDATION_FAILED,
    MAMMILLARY_ERROR_CIRCUIT_BROKEN,
    MAMMILLARY_ERROR_INTERNAL
} mammillary_error_t;

/**
 * @brief Memory trace types for consolidation
 */
typedef enum {
    MEMORY_TRACE_EPISODIC = 0,
    MEMORY_TRACE_SPATIAL,
    MEMORY_TRACE_CONTEXTUAL,
    MEMORY_TRACE_TEMPORAL,
    MEMORY_TRACE_EMOTIONAL,
    MEMORY_TRACE_PROCEDURAL
} memory_trace_type_t;

/**
 * @brief Papez circuit phase
 */
typedef enum {
    PAPEZ_PHASE_IDLE = 0,
    PAPEZ_PHASE_HIPPOCAMPAL_INPUT,
    PAPEZ_PHASE_MAMMILLARY_RELAY,
    PAPEZ_PHASE_THALAMIC_OUTPUT,
    PAPEZ_PHASE_CINGULATE_FEEDBACK,
    PAPEZ_PHASE_COMPLETE
} papez_phase_t;

/**
 * @brief Head direction cell state
 */
typedef enum {
    HD_STATE_INACTIVE = 0,
    HD_STATE_TUNING,
    HD_STATE_ACTIVE,
    HD_STATE_DRIFTING,
    HD_STATE_CORRECTING
} hd_cell_state_t;

/**
 * @brief Memory consolidation state
 */
typedef enum {
    CONSOLIDATION_IDLE = 0,
    CONSOLIDATION_ENCODING,
    CONSOLIDATION_RELAYING,
    CONSOLIDATION_STRENGTHENING,
    CONSOLIDATION_COMPLETE
} consolidation_state_t;

/*=============================================================================
 * STRUCTURES - CELLULAR COMPONENTS
 *===========================================================================*/

/**
 * @brief Head direction cell
 * Encodes the animal's current heading direction
 */
typedef struct {
    uint32_t cell_id;
    float preferred_direction;      /* Peak firing direction (radians) */
    float tuning_width;             /* Tuning curve width (degrees) */
    float current_firing_rate;      /* Current firing rate (Hz) */
    float max_firing_rate;          /* Maximum firing rate */
    float baseline_rate;            /* Baseline firing rate */
    float drift_rate;               /* Angular drift rate */
    hd_cell_state_t state;

    /* Vestibular integration */
    float vestibular_weight;
    float visual_weight;
    float landmark_weight;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
    float spike_threshold;

    /* Plasticity */
    float learning_rate;
    float last_spike_time;
    float eligibility_trace;
} nimcp_hd_cell_t;

/**
 * @brief Memory relay cell
 * Relays memory information from hippocampus to thalamus
 */
typedef struct {
    uint32_t cell_id;
    float activation;
    float* input_weights;           /* From hippocampus */
    float* output_weights;          /* To thalamus */
    uint32_t num_inputs;
    uint32_t num_outputs;

    /* Temporal dynamics */
    float persistence;              /* How long activation persists */
    float decay_rate;
    float last_activation_time;

    /* Memory specificity */
    float* memory_tuning;           /* Selectivity for memory types */
    uint32_t memory_dim;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
    bool refractory;
} nimcp_relay_cell_t;

/**
 * @brief Spatial processing cell
 * Processes spatial information for memory consolidation
 */
typedef struct {
    uint32_t cell_id;
    float* spatial_tuning;          /* Spatial selectivity profile */
    float activation;
    float position_encoding[3];     /* Associated position */
    float orientation;              /* Associated heading */

    /* Integration */
    float grid_cell_input;          /* From entorhinal */
    float place_cell_input;         /* From hippocampus */
    float hd_cell_input;            /* From head direction cells */
} nimcp_spatial_cell_t;

/**
 * @brief Memory trace for consolidation
 */
typedef struct {
    uint32_t trace_id;
    memory_trace_type_t type;
    float* content;                 /* Memory content vector */
    uint32_t content_dim;
    float strength;                 /* Consolidation strength */
    float age;                      /* Time since encoding */
    float last_retrieval;           /* Time of last retrieval */
    uint32_t retrieval_count;
    consolidation_state_t state;

    /* Spatial context */
    float position[3];
    float heading;
    bool has_spatial_context;

    /* Temporal context */
    uint64_t encoding_timestamp;
    float temporal_tag;

    /* Emotional valence */
    float emotional_intensity;
    float valence;                  /* Positive/negative */

    /* Hippocampal binding */
    uint32_t hippocampal_trace_id;
    float hippocampal_binding_strength;

    /* Thalamic projection */
    uint32_t thalamic_target_id;
    float thalamic_projection_strength;
} nimcp_memory_trace_t;

/*=============================================================================
 * STRUCTURES - BRIDGE INTEGRATIONS
 *===========================================================================*/

/**
 * @brief Hippocampus bridge (primary input)
 */
typedef struct {
    bool initialized;
    void* hippocampus_ref;
    float fornix_strength;          /* Fornix connection strength */
    float ca3_input_weight;
    float ca1_input_weight;
    float subiculum_input_weight;
    uint32_t active_projections;
    float last_sync_time;
    float input_buffer[256];
    uint32_t input_buffer_size;
} mammillary_hippocampus_bridge_t;

/**
 * @brief Anterior thalamus bridge (primary output)
 */
typedef struct {
    bool initialized;
    void* thalamus_ref;
    float tract_strength;           /* Mammillothalamic tract strength */
    float relay_efficiency;
    uint32_t active_channels;
    float output_buffer[256];
    uint32_t output_buffer_size;
    float last_output_time;
} mammillary_thalamus_bridge_t;

/**
 * @brief Cingulate cortex bridge (feedback)
 */
typedef struct {
    bool initialized;
    void* cingulate_ref;
    float feedback_strength;
    float error_signal;
    float attention_modulation;
    float last_feedback_time;
} mammillary_cingulate_bridge_t;

/**
 * @brief Hypothalamus bridge (local connections)
 */
typedef struct {
    bool initialized;
    void* hypothalamus_ref;
    float stress_level;
    float arousal_level;
    float circadian_phase;
    float metabolic_state;
    float modulation_strength;
} mammillary_hypothalamus_bridge_t;

/**
 * @brief Vestibular system bridge (head direction)
 */
typedef struct {
    bool initialized;
    void* vestibular_ref;
    float angular_velocity[3];      /* Roll, pitch, yaw */
    float linear_acceleration[3];
    float head_orientation[4];      /* Quaternion */
    float update_rate;
    float calibration_offset;
} mammillary_vestibular_bridge_t;

/**
 * @brief Entorhinal cortex bridge (grid cell input)
 */
typedef struct {
    bool initialized;
    void* entorhinal_ref;
    float grid_cell_input_weight;
    float* grid_phase;
    uint32_t grid_dim;
    float path_integration_gain;
} mammillary_entorhinal_bridge_t;

/**
 * @brief Security bridge
 */
typedef struct {
    bool initialized;
    void* security_ctx;
    void* security_ops;
    uint32_t access_level;
    bool memory_encryption_enabled;
    uint64_t validation_token;
} mammillary_security_bridge_t;

/**
 * @brief Immune system bridge
 */
typedef struct {
    bool initialized;
    void* immune_system;
    float health_score;
    float inflammation_level;
    float cytokine_influence;
    bool under_attack;
    uint32_t anomalies_detected;
} mammillary_immune_bridge_t;

/**
 * @brief Bio-async bridge
 */
typedef struct {
    bool initialized;
    void* runtime;
    uint32_t pending_tasks;
    float async_efficiency;
    bool background_consolidation;
} mammillary_bio_async_bridge_t;

/**
 * @brief Logging bridge
 */
typedef struct {
    bool initialized;
    void* logger;
    uint32_t log_level;
    bool trace_enabled;
    char log_prefix[32];
} mammillary_logging_bridge_t;

/**
 * @brief Prime resonance bridge
 * Integrates mammillary bodies with theta/gamma oscillation system for
 * memory consolidation timing and Papez circuit synchronization.
 */
typedef struct {
    nimcp_prime_resonance_t* resonance;
    float theta_phase;          /* Current theta phase (4-8 Hz) for memory timing */
    float gamma_phase;          /* Current gamma phase for local processing */
    float resonance_amplitude;  /* Oscillation amplitude */
    float frequency;            /* Current resonance frequency */
    float papez_coupling;       /* Coupling strength to Papez circuit */
    float hd_theta_sync;        /* Head direction-theta synchronization */
    bool initialized;
    bool synchronized;
    uint64_t phase_updates;
} mammillary_resonance_bridge_t;

/**
 * @brief Cognitive/training bridge
 */
typedef struct {
    bool initialized;
    void* cognitive_ctx;
    void* training_ctx;
    float learning_rate;
    float cognitive_load;
    bool training_active;
} mammillary_cognitive_bridge_t;

/**
 * @brief Logic system bridge
 */
typedef struct {
    bool initialized;
    void* logic_ctx;
    float inference_confidence;
    uint32_t active_rules;
    bool reasoning_active;
} mammillary_logic_bridge_t;

/**
 * @brief Neural substrate bridge
 */
typedef struct {
    bool initialized;
    void* substrate;
    float atp_level;
    float oxygen_level;
    float glucose_level;
    float temperature;
    bool homeostasis_ok;
} mammillary_substrate_bridge_t;

/**
 * @brief Thalamic layers bridge
 */
typedef struct {
    bool initialized;
    void* thalamic_layers;
    float relay_gain;
    float gating_level;
    uint32_t active_pathways;
} mammillary_thalamic_bridge_t;

/**
 * @brief Perception bridge
 */
typedef struct {
    bool initialized;
    void* perception_ctx;
    float salience_threshold;
    float attention_level;
    uint32_t attended_features;
} mammillary_perception_bridge_t;

/**
 * @brief SNN/Plasticity bridge
 */
typedef struct {
    bool initialized;
    void* snn_network;
    void* plasticity_ctx;
    void* stdp_ctx;
    float global_learning_rate;
    float spike_rate;
    bool plasticity_enabled;
} mammillary_snn_bridge_t;

/**
 * @brief Swarm/Dragonfly bridge
 */
typedef struct {
    bool initialized;
    void* swarm_ctx;
    void* dragonfly_ctx;
    float swarm_coherence;
    uint32_t active_agents;
} mammillary_swarm_bridge_t;

/**
 * @brief Cerebellum bridge
 */
typedef struct {
    bool initialized;
    void* cerebellum;
    float timing_precision;
    float motor_learning_rate;
    float prediction_error;
} mammillary_cerebellum_bridge_t;

/**
 * @brief Medulla bridge
 */
typedef struct {
    bool initialized;
    void* medulla;
    float autonomic_state;
    float vital_signs[4];           /* HR, BP, RR, temp */
    bool emergency_mode;
} mammillary_medulla_bridge_t;

/**
 * @brief Omnidirectional system bridge
 */
typedef struct {
    bool initialized;
    void* omni_ctx;
    float integration_weight;
    float synchronization_quality;
} mammillary_omni_bridge_t;

/*=============================================================================
 * MAIN STRUCTURE
 *===========================================================================*/

/**
 * @brief Configuration for mammillary bodies
 */
typedef struct {
    uint32_t num_hd_cells;
    uint32_t num_relay_cells;
    uint32_t num_spatial_cells;
    uint32_t max_memory_traces;
    float default_hd_tuning_width;
    float consolidation_threshold;
    float relay_decay_rate;
    float hd_drift_correction_rate;
    bool enable_papez_circuit;
    bool enable_spatial_processing;
    bool enable_head_direction;
    bool enable_background_consolidation;
    float hippocampal_input_gain;
    float thalamic_output_gain;
} mammillary_config_t;

/**
 * @brief Statistics for mammillary bodies
 */
typedef struct {
    uint32_t total_memory_traces;
    uint32_t traces_consolidated;
    uint32_t traces_decayed;
    uint32_t relay_operations;
    uint32_t hd_updates;
    uint32_t papez_cycles;
    float avg_consolidation_strength;
    float avg_relay_efficiency;
    float hd_accuracy;
    float current_heading;
    uint64_t last_update_time;
    uint32_t updates_processed;
} mammillary_stats_t;

/**
 * @brief Main mammillary bodies structure
 */
typedef struct nimcp_mammillary {
    /* Configuration */
    mammillary_config_t config;

    /* Status */
    mammillary_status_t status;
    mammillary_error_t last_error;

    /* Cellular components */
    nimcp_hd_cell_t* hd_cells;
    uint32_t num_hd_cells;
    nimcp_relay_cell_t* relay_cells;
    uint32_t num_relay_cells;
    nimcp_spatial_cell_t* spatial_cells;
    uint32_t num_spatial_cells;

    /* Memory traces */
    nimcp_memory_trace_t* memory_traces;
    uint32_t num_memory_traces;
    uint32_t max_memory_traces;

    /* Head direction state */
    float current_heading;
    float heading_confidence;
    float angular_velocity;
    float hd_population_vector[2];  /* cos, sin components */

    /* Papez circuit state */
    papez_phase_t papez_phase;
    float papez_activity;
    float circuit_integrity;

    /* Consolidation state */
    consolidation_state_t consolidation_state;
    float consolidation_rate;
    uint32_t traces_pending_consolidation;

    /* Bridge integrations */
    mammillary_hippocampus_bridge_t hippocampus_bridge;
    mammillary_thalamus_bridge_t thalamus_bridge;
    mammillary_cingulate_bridge_t cingulate_bridge;
    mammillary_hypothalamus_bridge_t hypothalamus_bridge;
    mammillary_vestibular_bridge_t vestibular_bridge;
    mammillary_entorhinal_bridge_t entorhinal_bridge;
    mammillary_security_bridge_t security_bridge;
    mammillary_immune_bridge_t immune_bridge;
    mammillary_bio_async_bridge_t bio_async_bridge;
    mammillary_logging_bridge_t logging_bridge;
    mammillary_resonance_bridge_t resonance_bridge;
    mammillary_cognitive_bridge_t cognitive_bridge;
    mammillary_logic_bridge_t logic_bridge;
    mammillary_substrate_bridge_t substrate_bridge;
    mammillary_thalamic_bridge_t thalamic_bridge;
    mammillary_perception_bridge_t perception_bridge;
    mammillary_snn_bridge_t snn_bridge;
    mammillary_swarm_bridge_t swarm_bridge;
    mammillary_cerebellum_bridge_t cerebellum_bridge;
    mammillary_medulla_bridge_t medulla_bridge;
    mammillary_omni_bridge_t omni_bridge;

    /* Statistics */
    uint32_t updates_processed;
    uint32_t relay_operations;
    uint32_t consolidations_completed;
    uint32_t hd_corrections;
    uint64_t creation_time;
    uint64_t last_update_time;
} nimcp_mammillary_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
mammillary_config_t mammillary_default_config(void);

/**
 * @brief Create mammillary bodies instance
 */
nimcp_mammillary_t* mammillary_create(const mammillary_config_t* config);

/**
 * @brief Destroy mammillary bodies instance
 */
void mammillary_destroy(nimcp_mammillary_t* mb);

/**
 * @brief Reset mammillary bodies to initial state
 */
int mammillary_reset(nimcp_mammillary_t* mb);

/**
 * @brief Update mammillary bodies state
 */
int mammillary_update(nimcp_mammillary_t* mb, float dt);

/*=============================================================================
 * HEAD DIRECTION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update head direction from vestibular input
 */
int mammillary_update_head_direction(nimcp_mammillary_t* mb,
                                      float angular_velocity,
                                      float dt);

/**
 * @brief Set head direction from visual landmark
 */
int mammillary_set_hd_from_landmark(nimcp_mammillary_t* mb,
                                     float landmark_bearing,
                                     float confidence);

/**
 * @brief Get current head direction estimate
 */
float mammillary_get_head_direction(nimcp_mammillary_t* mb);

/**
 * @brief Get head direction confidence
 */
float mammillary_get_hd_confidence(nimcp_mammillary_t* mb);

/**
 * @brief Get head direction population vector
 */
int mammillary_get_hd_population_vector(nimcp_mammillary_t* mb,
                                         float* vector,
                                         uint32_t* dim);

/**
 * @brief Correct head direction drift
 */
int mammillary_correct_hd_drift(nimcp_mammillary_t* mb,
                                 float true_heading);

/**
 * @brief Get active head direction cells
 */
int mammillary_get_active_hd_cells(nimcp_mammillary_t* mb,
                                    uint32_t* cell_ids,
                                    float* firing_rates,
                                    uint32_t max_cells,
                                    uint32_t* num_active);

/*=============================================================================
 * MEMORY RELAY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Receive memory trace from hippocampus
 */
int mammillary_receive_hippocampal_input(nimcp_mammillary_t* mb,
                                          const float* trace,
                                          uint32_t trace_dim,
                                          memory_trace_type_t type,
                                          float emotional_valence,
                                          uint32_t* trace_id_out);

/**
 * @brief Relay memory trace to anterior thalamus
 */
int mammillary_relay_to_thalamus(nimcp_mammillary_t* mb,
                                  uint32_t trace_id);

/**
 * @brief Process Papez circuit cycle
 */
int mammillary_process_papez_cycle(nimcp_mammillary_t* mb);

/**
 * @brief Get current Papez circuit phase
 */
papez_phase_t mammillary_get_papez_phase(nimcp_mammillary_t* mb);

/**
 * @brief Get Papez circuit activity level
 */
float mammillary_get_papez_activity(nimcp_mammillary_t* mb);

/**
 * @brief Advance Papez circuit to next phase
 */
int mammillary_advance_papez_phase(nimcp_mammillary_t* mb);

/*=============================================================================
 * MEMORY CONSOLIDATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Start memory consolidation process
 */
int mammillary_start_consolidation(nimcp_mammillary_t* mb,
                                    uint32_t trace_id);

/**
 * @brief Update consolidation state
 */
int mammillary_update_consolidation(nimcp_mammillary_t* mb, float dt);

/**
 * @brief Get consolidation state
 */
consolidation_state_t mammillary_get_consolidation_state(nimcp_mammillary_t* mb);

/**
 * @brief Get memory trace by ID
 */
const nimcp_memory_trace_t* mammillary_get_trace(nimcp_mammillary_t* mb,
                                                  uint32_t trace_id);

/**
 * @brief Strengthen memory trace
 */
int mammillary_strengthen_trace(nimcp_mammillary_t* mb,
                                 uint32_t trace_id,
                                 float amount);

/**
 * @brief Get traces by type
 */
int mammillary_get_traces_by_type(nimcp_mammillary_t* mb,
                                   memory_trace_type_t type,
                                   uint32_t* trace_ids,
                                   uint32_t max_traces,
                                   uint32_t* num_found);

/**
 * @brief Get strongest memory traces
 */
int mammillary_get_strongest_traces(nimcp_mammillary_t* mb,
                                     uint32_t* trace_ids,
                                     float* strengths,
                                     uint32_t max_traces,
                                     uint32_t* num_returned);

/**
 * @brief Decay old memory traces
 */
int mammillary_decay_traces(nimcp_mammillary_t* mb, float decay_factor);

/**
 * @brief Remove memory trace
 */
int mammillary_remove_trace(nimcp_mammillary_t* mb, uint32_t trace_id);

/*=============================================================================
 * SPATIAL PROCESSING FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update spatial cells with current position
 */
int mammillary_update_spatial_cells(nimcp_mammillary_t* mb,
                                     const float* position,
                                     uint32_t dim);

/**
 * @brief Encode spatial memory
 */
int mammillary_encode_spatial_memory(nimcp_mammillary_t* mb,
                                      const float* position,
                                      float heading,
                                      const float* context,
                                      uint32_t context_dim,
                                      uint32_t* trace_id_out);

/**
 * @brief Retrieve spatial context for position
 */
int mammillary_retrieve_spatial_context(nimcp_mammillary_t* mb,
                                         const float* position,
                                         uint32_t dim,
                                         float* context,
                                         uint32_t* context_dim);

/**
 * @brief Get spatial cell activity
 */
int mammillary_get_spatial_activity(nimcp_mammillary_t* mb,
                                     float* activity,
                                     uint32_t max_cells,
                                     uint32_t* num_cells);

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW
 *===========================================================================*/

/**
 * @brief Process incoming data from all bridges
 */
int mammillary_process_incoming(nimcp_mammillary_t* mb);

/**
 * @brief Send outgoing data to all bridges
 */
int mammillary_send_outgoing(nimcp_mammillary_t* mb);

/**
 * @brief Full bidirectional update cycle
 */
int mammillary_bidirectional_update(nimcp_mammillary_t* mb, float dt);

/**
 * @brief Synchronize with hippocampus
 */
int mammillary_sync_hippocampus(nimcp_mammillary_t* mb);

/**
 * @brief Synchronize with anterior thalamus
 */
int mammillary_sync_thalamus(nimcp_mammillary_t* mb);

/**
 * @brief Send output to thalamus
 */
int mammillary_send_to_thalamus(nimcp_mammillary_t* mb);

/**
 * @brief Receive feedback from cingulate
 */
int mammillary_receive_cingulate_feedback(nimcp_mammillary_t* mb);

/*=============================================================================
 * BRIDGE INITIALIZATION FUNCTIONS
 *===========================================================================*/

int mammillary_init_hippocampus_bridge(nimcp_mammillary_t* mb, void* hippocampus);
int mammillary_init_thalamus_bridge(nimcp_mammillary_t* mb, void* thalamus);
int mammillary_init_cingulate_bridge(nimcp_mammillary_t* mb, void* cingulate);
int mammillary_init_hypothalamus_bridge(nimcp_mammillary_t* mb, void* hypothalamus);
int mammillary_init_vestibular_bridge(nimcp_mammillary_t* mb, void* vestibular);
int mammillary_init_entorhinal_bridge(nimcp_mammillary_t* mb, void* entorhinal);
int mammillary_init_security_bridge(nimcp_mammillary_t* mb, void* security_ctx, void* security_ops);
int mammillary_init_immune_bridge(nimcp_mammillary_t* mb, void* immune);
int mammillary_init_bio_async_bridge(nimcp_mammillary_t* mb, void* runtime);
int mammillary_init_logging_bridge(nimcp_mammillary_t* mb, void* logger);
int mammillary_init_resonance_bridge(nimcp_mammillary_t* mb, nimcp_prime_resonance_t* resonance);

/**
 * @brief Update resonance phase from prime resonance system
 */
int mammillary_update_resonance_phase(nimcp_mammillary_t* mb, float theta_phase, float gamma_phase);

/**
 * @brief Get current theta phase for memory timing
 */
float mammillary_get_theta_phase(const nimcp_mammillary_t* mb);

/**
 * @brief Get head direction-theta synchronization level
 */
float mammillary_get_hd_theta_sync(const nimcp_mammillary_t* mb);

/**
 * @brief Synchronize Papez circuit with theta rhythm
 */
int mammillary_sync_papez_to_theta(nimcp_mammillary_t* mb);
int mammillary_init_cognitive_bridge(nimcp_mammillary_t* mb, void* cognitive, void* training);
int mammillary_init_logic_bridge(nimcp_mammillary_t* mb, void* logic);
int mammillary_init_substrate_bridge(nimcp_mammillary_t* mb, void* substrate);
int mammillary_init_thalamic_bridge(nimcp_mammillary_t* mb, void* thalamic_layers);
int mammillary_init_perception_bridge(nimcp_mammillary_t* mb, void* perception);
int mammillary_init_snn_bridge(nimcp_mammillary_t* mb, void* snn);
int mammillary_init_swarm_bridge(nimcp_mammillary_t* mb, void* swarm, void* dragonfly);
int mammillary_init_cerebellum_bridge(nimcp_mammillary_t* mb, void* cerebellum);
int mammillary_init_medulla_bridge(nimcp_mammillary_t* mb, void* medulla);
int mammillary_init_omni_bridge(nimcp_mammillary_t* mb, void* omni);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current status
 */
mammillary_status_t mammillary_get_status(nimcp_mammillary_t* mb);

/**
 * @brief Get last error code
 */
mammillary_error_t mammillary_get_last_error(nimcp_mammillary_t* mb);

/**
 * @brief Get error string
 */
const char* mammillary_error_string(mammillary_error_t error);

/**
 * @brief Get status string
 */
const char* mammillary_status_string(mammillary_status_t status);

/**
 * @brief Get statistics
 */
int mammillary_get_stats(nimcp_mammillary_t* mb, mammillary_stats_t* stats);

/**
 * @brief Get current configuration
 */
int mammillary_get_config(nimcp_mammillary_t* mb, mammillary_config_t* config);

/**
 * @brief Get health status
 */
float mammillary_get_health_status(nimcp_mammillary_t* mb);

/**
 * @brief Get circuit integrity
 */
float mammillary_get_circuit_integrity(nimcp_mammillary_t* mb);

/**
 * @brief Log diagnostics
 */
int mammillary_log_diagnostics(nimcp_mammillary_t* mb);

/*=============================================================================
 * CELL ACTIVITY QUERIES
 *===========================================================================*/

/**
 * @brief Get head direction cell activity
 */
size_t mammillary_get_hd_cell_activity(nimcp_mammillary_t* mb,
                                        float* activity,
                                        size_t max_cells);

/**
 * @brief Get relay cell activity
 */
size_t mammillary_get_relay_cell_activity(nimcp_mammillary_t* mb,
                                           float* activity,
                                           size_t max_cells);

/**
 * @brief Get spatial cell activity
 */
size_t mammillary_get_spatial_cell_activity(nimcp_mammillary_t* mb,
                                             float* activity,
                                             size_t max_cells);

/*=============================================================================
 * SERIALIZATION
 *===========================================================================*/

/**
 * @brief Get serialization size
 */
size_t mammillary_get_serialization_size(nimcp_mammillary_t* mb);

/**
 * @brief Serialize mammillary bodies state
 */
int mammillary_serialize(nimcp_mammillary_t* mb,
                          uint8_t* buffer,
                          size_t buffer_size,
                          size_t* bytes_written);

/**
 * @brief Deserialize mammillary bodies state
 */
nimcp_mammillary_t* mammillary_deserialize(const uint8_t* buffer,
                                            size_t buffer_size,
                                            size_t* bytes_read);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MAMMILLARY_H */
