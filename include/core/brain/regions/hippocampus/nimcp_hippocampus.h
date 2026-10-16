/**
 * @file nimcp_hippocampus.h
 * @brief Hippocampus - Central Hub for Memory Formation and Spatial Navigation
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * The hippocampus is the central hub of the memory system, integrating inputs
 * from entorhinal, perirhinal, and parahippocampal cortices to form episodic
 * memories and support spatial navigation.
 *
 * Subregions:
 * - Dentate Gyrus (DG): Pattern separation, neurogenesis
 * - CA3: Pattern completion, autoassociative memory, recurrent connections
 * - CA1: Temporal coding, output to cortex, memory retrieval
 * - Subiculum: Output relay to mammillary bodies and other targets
 *
 * Key Functions:
 * - Episodic memory encoding and retrieval
 * - Spatial navigation with place cells
 * - Pattern separation (DG) and completion (CA3)
 * - Memory consolidation coordination
 * - Theta/gamma rhythm generation
 * - Sharp-wave ripple memory replay
 *
 * Full Bidirectional Integration:
 * - Prime Resonance Memory System
 * - Immune System & Bio-Async
 * - Brain Initialization
 * - Security Module
 * - Logging System
 * - Cognitive & Training Layers
 * - Omnidirectional Module
 * - Hypothalamus
 * - Neural Substrate
 * - Thalamus Layers
 * - Portia & Dragonfly Modules
 * - Perception Layer
 * - SNN & Plasticity/STDP
 * - All Memory Circuit Regions
 */

#ifndef NIMCP_HIPPOCAMPUS_H
#define NIMCP_HIPPOCAMPUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define HIPPO_DEFAULT_DG_CELLS          1024    /* Dentate gyrus granule cells */
#define HIPPO_DEFAULT_CA3_CELLS         512     /* CA3 pyramidal cells */
#define HIPPO_DEFAULT_CA1_CELLS         512     /* CA1 pyramidal cells */
#define HIPPO_DEFAULT_SUBICULUM_CELLS   256     /* Subiculum cells */
#define HIPPO_DEFAULT_PLACE_CELLS       256     /* Place cells */
#define HIPPO_MAX_EPISODES              1024    /* Maximum stored episodes */
#define HIPPO_MAX_REPLAY_BUFFER         64      /* Sharp-wave ripple buffer */
#define HIPPO_THETA_FREQUENCY           8.0f    /* Hz - theta rhythm */
#define HIPPO_GAMMA_FREQUENCY           40.0f   /* Hz - gamma rhythm */
#define HIPPO_PATTERN_SEPARATION_RATIO  5.0f    /* DG expansion ratio */
#define HIPPO_CA3_RECURRENCE_PROB       0.02f   /* CA3 recurrent connection probability */
#define HIPPO_VERSION_MAJOR             1
#define HIPPO_VERSION_MINOR             0
#define HIPPO_VERSION_PATCH             0

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Hippocampus operational status
 */
typedef enum {
    HIPPO_STATUS_IDLE = 0,
    HIPPO_STATUS_READY,
    HIPPO_STATUS_ENCODING,
    HIPPO_STATUS_RETRIEVING,
    HIPPO_STATUS_CONSOLIDATING,
    HIPPO_STATUS_REPLAYING,
    HIPPO_STATUS_NAVIGATING,
    HIPPO_STATUS_PATTERN_SEPARATING,
    HIPPO_STATUS_PATTERN_COMPLETING,
    HIPPO_STATUS_ERROR
} hippo_status_t;

/**
 * @brief Hippocampus error codes
 */
typedef enum {
    HIPPO_ERROR_NONE = 0,
    HIPPO_ERROR_INVALID_INPUT,
    HIPPO_ERROR_MEMORY_FULL,
    HIPPO_ERROR_EPISODE_NOT_FOUND,
    HIPPO_ERROR_ENCODING_FAILED,
    HIPPO_ERROR_RETRIEVAL_FAILED,
    HIPPO_ERROR_PATTERN_MISMATCH,
    HIPPO_ERROR_RHYTHM_DISRUPTED,
    HIPPO_ERROR_CONSOLIDATION_FAILED,
    HIPPO_ERROR_BRIDGE_ERROR,
    HIPPO_ERROR_INTERNAL
} hippo_error_t;

/**
 * @brief Hippocampal subregion identifiers
 */
typedef enum {
    HIPPO_REGION_DG = 0,        /* Dentate Gyrus */
    HIPPO_REGION_CA3,           /* Cornu Ammonis 3 */
    HIPPO_REGION_CA1,           /* Cornu Ammonis 1 */
    HIPPO_REGION_SUBICULUM,     /* Subiculum */
    HIPPO_REGION_COUNT
} hippo_region_t;

/**
 * @brief Memory encoding strength
 */
typedef enum {
    ENCODING_WEAK = 0,
    ENCODING_MODERATE,
    ENCODING_STRONG,
    ENCODING_FLASHBULB          /* Highly emotional, vivid */
} encoding_strength_t;

/**
 * @brief Retrieval mode
 */
typedef enum {
    RETRIEVAL_FREE_RECALL = 0,
    RETRIEVAL_CUED_RECALL,
    RETRIEVAL_RECOGNITION,
    RETRIEVAL_PATTERN_COMPLETION
} retrieval_mode_t;

/**
 * @brief Oscillation state for theta/gamma rhythms
 */
typedef enum {
    OSCILLATION_NONE = 0,
    OSCILLATION_THETA,
    OSCILLATION_GAMMA,
    OSCILLATION_THETA_GAMMA_COUPLED,
    OSCILLATION_SHARP_WAVE_RIPPLE
} oscillation_state_t;

/**
 * @brief Replay state for memory consolidation
 */
typedef enum {
    REPLAY_IDLE = 0,
    REPLAY_FORWARD,
    REPLAY_REVERSE,
    REPLAY_COMPRESSED
} replay_state_t;

/**
 * @brief Episode type classification
 */
typedef enum {
    EPISODE_TYPE_SPATIAL = 0,
    EPISODE_TYPE_SEMANTIC,
    EPISODE_TYPE_EMOTIONAL,
    EPISODE_TYPE_PROCEDURAL,
    EPISODE_TYPE_AUTOBIOGRAPHICAL
} episode_type_t;

/*=============================================================================
 * STRUCTURES - CELLULAR COMPONENTS
 *===========================================================================*/

/**
 * @brief Dentate Gyrus granule cell
 * Performs pattern separation via sparse coding
 */
typedef struct {
    uint32_t cell_id;
    float activation;
    float* input_weights;           /* From entorhinal perforant path */
    uint32_t num_inputs;
    float sparsity;                 /* Firing sparsity (typically ~2%) */
    float threshold;                /* Activation threshold */

    /* Neurogenesis support */
    bool is_newborn;
    float maturity;                 /* 0.0 = just born, 1.0 = mature */
    float integration_progress;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
    float spike_threshold;
    float last_spike_time;

    /* Plasticity */
    float* eligibility_traces;
    float learning_rate;
} nimcp_dg_cell_t;

/**
 * @brief CA3 pyramidal cell
 * Performs pattern completion via recurrent connections
 */
typedef struct {
    uint32_t cell_id;
    float activation;
    float* mossy_fiber_weights;     /* From DG - strong, sparse */
    float* perforant_weights;       /* From entorhinal - weak, dense */
    float* recurrent_weights;       /* From other CA3 cells */
    uint32_t num_mossy_inputs;
    uint32_t num_perforant_inputs;
    uint32_t num_recurrent;

    /* Attractor dynamics */
    float attractor_strength;
    float completion_threshold;
    uint32_t associated_pattern_id;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
    float adaptation;               /* Spike frequency adaptation */

    /* Plasticity */
    float hebbian_trace;
    float stdp_trace;
} nimcp_ca3_cell_t;

/**
 * @brief CA1 pyramidal cell
 * Temporal coding and output to cortex
 */
typedef struct {
    uint32_t cell_id;
    float activation;
    float* schaffer_weights;        /* From CA3 Schaffer collaterals */
    float* perforant_weights;       /* Direct from entorhinal */
    uint32_t num_schaffer_inputs;
    uint32_t num_perforant_inputs;

    /* Temporal coding */
    float phase_precession;         /* Theta phase precession */
    float temporal_context;
    float sequence_position;

    /* Place field */
    bool is_place_cell;
    float place_field_center[3];
    float place_field_radius;
    float place_field_peak_rate;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
    float theta_modulation;

    /* Output projections */
    float cortical_output_weight;
    float subiculum_output_weight;
} nimcp_ca1_cell_t;

/**
 * @brief Subiculum cell
 * Output relay and spatial processing
 */
typedef struct {
    uint32_t cell_id;
    float activation;
    float* ca1_weights;             /* From CA1 */
    uint32_t num_ca1_inputs;

    /* Output targets */
    float mammillary_output;
    float entorhinal_output;
    float prefrontal_output;

    /* Spatial coding */
    bool is_boundary_cell;
    bool is_grid_cell;
    float spatial_tuning[3];

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
} nimcp_subiculum_cell_t;

/**
 * @brief Place cell for spatial navigation
 */
typedef struct {
    uint32_t cell_id;
    float place_field_center[3];
    float place_field_radius;
    float peak_firing_rate;
    float current_rate;
    float field_stability;

    /* Remapping */
    uint32_t context_id;
    bool has_remapped;
    float remap_threshold;

    /* Theta phase */
    float preferred_theta_phase;
    float phase_precession_slope;

    /* Associated memories */
    uint32_t* associated_episodes;
    uint32_t num_associated;
} nimcp_place_cell_t;

/**
 * @brief Episodic memory structure
 */
typedef struct {
    uint32_t episode_id;
    episode_type_t type;

    /* Content vectors */
    float* what_content;            /* Object/item information */
    uint32_t what_dim;
    float* where_content;           /* Spatial context */
    uint32_t where_dim;
    float* when_content;            /* Temporal context */
    uint32_t when_dim;

    /* Binding */
    float* bound_representation;    /* Unified episode representation */
    uint32_t bound_dim;

    /* Metadata */
    float encoding_strength;
    float emotional_valence;
    float emotional_arousal;
    uint64_t encoding_timestamp;
    float recency;
    uint32_t retrieval_count;
    float consolidation_level;      /* 0.0 = hippocampal, 1.0 = cortical */

    /* Neural patterns */
    uint32_t* dg_pattern;
    uint32_t dg_pattern_size;
    uint32_t* ca3_pattern;
    uint32_t ca3_pattern_size;
    uint32_t* ca1_pattern;
    uint32_t ca1_pattern_size;

    /* Associated place */
    float associated_position[3];
    bool has_spatial_context;

    /* Prime resonance tag */
    uint64_t resonance_signature;
    float resonance_strength;
} nimcp_episode_t;

/**
 * @brief Sharp-wave ripple event for memory replay
 */
typedef struct {
    uint32_t ripple_id;
    uint64_t timestamp;
    float amplitude;
    float frequency;                /* ~150-250 Hz */
    float duration_ms;

    /* Replayed content */
    uint32_t* episode_sequence;
    uint32_t sequence_length;
    replay_state_t replay_direction;
    float compression_factor;       /* Time compression during replay */

    /* Coordination */
    float cortical_spindle_coupling;
    float slow_oscillation_phase;
} nimcp_ripple_event_t;

/*=============================================================================
 * STRUCTURES - BRIDGE INTEGRATIONS
 *===========================================================================*/

/**
 * @brief Prime Resonance Memory bridge
 */
typedef struct {
    bool initialized;
    void* pr_memory_ctx;
    float resonance_frequency;
    float resonance_amplitude;
    float phase_alignment;
    float coherence_level;
    uint64_t last_resonance_tag;
    bool resonance_active;
    float memory_enhancement_factor;
} hippo_prime_resonance_bridge_t;

/**
 * @brief Immune System bridge
 */
typedef struct {
    bool initialized;
    void* immune_system;
    float health_score;
    float inflammation_level;
    float cytokine_il6;             /* Affects memory consolidation */
    float cytokine_tnf;             /* Affects synaptic plasticity */
    float microglial_activity;      /* Synaptic pruning */
    bool neuroinflammation;
    uint32_t anomalies_detected;
} hippo_immune_bridge_t;

/**
 * @brief Bio-Async bridge
 */
typedef struct {
    bool initialized;
    void* runtime;
    uint32_t pending_consolidations;
    uint32_t pending_replays;
    float async_efficiency;
    bool background_processing;
    uint32_t worker_threads;
} hippo_bio_async_bridge_t;

/**
 * @brief Brain Initialization bridge
 */
typedef struct {
    bool initialized;
    void* brain_init_ctx;
    uint32_t init_sequence_id;
    float initialization_progress;
    bool full_integration_complete;
    uint64_t last_init_timestamp;
} hippo_brain_init_bridge_t;

/**
 * @brief Security bridge
 */
typedef struct {
    bool initialized;
    void* security_ctx;
    void* security_ops;
    uint32_t access_level;
    bool memory_encryption_enabled;
    bool integrity_checking;
    uint64_t validation_token;
    uint32_t security_violations;
} hippo_security_bridge_t;

/**
 * @brief Logging bridge
 */
typedef struct {
    bool initialized;
    void* logger;
    uint32_t log_level;
    bool trace_enabled;
    bool memory_access_logging;
    char log_prefix[32];
    uint64_t events_logged;
} hippo_logging_bridge_t;

/**
 * @brief Cognitive Layer bridge
 */
typedef struct {
    bool initialized;
    void* cognitive_ctx;
    float attention_level;
    float working_memory_load;
    float cognitive_control;
    float executive_modulation;
    uint32_t active_goals;
    float goal_relevance_threshold;
} hippo_cognitive_bridge_t;

/**
 * @brief Training Layer bridge
 */
typedef struct {
    bool initialized;
    void* training_ctx;
    float learning_rate;
    float error_signal;
    bool training_mode;
    uint32_t training_iterations;
    float loss_value;
    float gradient_norm;
} hippo_training_bridge_t;

/**
 * @brief Omnidirectional Module bridge
 */
typedef struct {
    bool initialized;
    void* omni_ctx;
    float integration_weight;
    float synchronization_quality;
    float global_coherence;
    uint32_t connected_modules;
    bool bidirectional_active;
} hippo_omni_bridge_t;

/**
 * @brief Hypothalamus bridge
 */
typedef struct {
    bool initialized;
    void* hypothalamus;
    float stress_level;             /* Cortisol affects memory */
    float arousal_level;
    float circadian_phase;
    float metabolic_state;
    float hunger_signal;
    float reward_signal;
    float threat_signal;
} hippo_hypothalamus_bridge_t;

/**
 * @brief Neural Substrate bridge
 */
typedef struct {
    bool initialized;
    void* substrate;
    float atp_level;
    float oxygen_saturation;
    float glucose_level;
    float lactate_level;
    float neurotransmitter_levels[8];  /* Glu, GABA, ACh, DA, 5HT, NE, etc. */
    float ltp_threshold;
    float ltd_threshold;
    bool metabolic_stress;
} hippo_substrate_bridge_t;

/**
 * @brief Thalamus Layer bridge
 */
typedef struct {
    bool initialized;
    void* thalamus;
    float relay_gain;
    float gating_level;
    float spindle_coupling;         /* Sleep spindles for consolidation */
    uint32_t active_pathways;
    float anterior_nucleus_activity; /* Memory pathway */
} hippo_thalamus_bridge_t;

/**
 * @brief Portia Module bridge (strategic planning)
 */
typedef struct {
    bool initialized;
    void* portia_ctx;
    float planning_depth;
    float strategy_confidence;
    uint32_t active_plans;
    float memory_utilization;       /* How much Portia uses hippocampal memory */
    bool prospective_memory_active;
} hippo_portia_bridge_t;

/**
 * @brief Dragonfly Module bridge (fast reactions)
 */
typedef struct {
    bool initialized;
    void* dragonfly_ctx;
    float reaction_threshold;
    float threat_assessment;
    float escape_route_memory;
    bool rapid_retrieval_mode;
    float motor_preparation;
} hippo_dragonfly_bridge_t;

/**
 * @brief Perception Layer bridge
 */
typedef struct {
    bool initialized;
    void* perception_ctx;
    float* current_percept;
    uint32_t percept_dim;
    float salience_level;
    float novelty_signal;
    float attention_weight;
    uint32_t attended_features;
} hippo_perception_bridge_t;

/**
 * @brief SNN bridge
 */
typedef struct {
    bool initialized;
    void* snn_network;
    float global_firing_rate;
    float network_synchrony;
    uint32_t total_neurons;
    uint32_t active_neurons;
    float theta_power;
    float gamma_power;
    float ripple_power;
} hippo_snn_bridge_t;

/**
 * @brief Plasticity/STDP bridge
 */
typedef struct {
    bool initialized;
    void* plasticity_ctx;
    void* stdp_ctx;
    float ltp_magnitude;
    float ltd_magnitude;
    float metaplasticity_state;
    float bcm_threshold;            /* BCM sliding threshold */
    bool hebbian_learning;
    bool stdp_enabled;
    float synaptic_tagging;
} hippo_plasticity_bridge_t;

/**
 * @brief Entorhinal Cortex bridge
 */
typedef struct {
    bool initialized;
    void* entorhinal;
    float* grid_cell_input;
    uint32_t grid_input_dim;
    float perforant_path_strength;
    float mec_input_weight;         /* Medial EC - spatial */
    float lec_input_weight;         /* Lateral EC - object */
    float temporal_context_signal;
} hippo_entorhinal_bridge_t;

/**
 * @brief Perirhinal Cortex bridge
 */
typedef struct {
    bool initialized;
    void* perirhinal;
    float* object_representation;
    uint32_t object_dim;
    float familiarity_signal;
    float novelty_signal;
    float recognition_confidence;
} hippo_perirhinal_bridge_t;

/**
 * @brief Parahippocampal Cortex bridge
 */
typedef struct {
    bool initialized;
    void* parahippocampal;
    float* scene_representation;
    uint32_t scene_dim;
    float* spatial_context;
    uint32_t context_dim;
    float place_recognition;
    float context_stability;
} hippo_parahippocampal_bridge_t;

/**
 * @brief Mammillary Bodies bridge
 */
typedef struct {
    bool initialized;
    void* mammillary;
    float fornix_output_strength;
    float head_direction_input;
    float papez_circuit_activity;
    float consolidation_signal;
} hippo_mammillary_bridge_t;

/**
 * @brief Cerebellum bridge
 */
typedef struct {
    bool initialized;
    void* cerebellum;
    float timing_signal;
    float sequence_prediction;
    float motor_memory_link;
} hippo_cerebellum_bridge_t;

/**
 * @brief Medulla bridge
 */
typedef struct {
    bool initialized;
    void* medulla;
    float autonomic_state;
    float arousal_from_brainstem;
    bool emergency_mode;
} hippo_medulla_bridge_t;

/**
 * @brief Swarm Intelligence bridge
 */
typedef struct {
    bool initialized;
    void* swarm_ctx;
    float collective_memory;
    float swarm_coherence;
    uint32_t active_agents;
} hippo_swarm_bridge_t;

/*=============================================================================
 * MAIN STRUCTURES
 *===========================================================================*/

/**
 * @brief Configuration for hippocampus
 */
typedef struct {
    uint32_t num_dg_cells;
    uint32_t num_ca3_cells;
    uint32_t num_ca1_cells;
    uint32_t num_subiculum_cells;
    uint32_t num_place_cells;
    uint32_t max_episodes;

    /* Pattern separation/completion */
    float dg_sparsity;              /* Target DG activation sparsity */
    float ca3_recurrence_density;
    float pattern_completion_threshold;

    /* Oscillations */
    float theta_frequency;
    float gamma_frequency;
    bool enable_theta_gamma_coupling;
    bool enable_sharp_wave_ripples;

    /* Plasticity */
    float default_learning_rate;
    float ltp_threshold;
    float ltd_threshold;
    bool enable_neurogenesis;

    /* Consolidation */
    float consolidation_rate;
    bool enable_sleep_replay;
    bool enable_awake_replay;

    /* Integration */
    bool enable_prime_resonance;
    bool enable_all_bridges;
} hippo_config_t;

/**
 * @brief Statistics for hippocampus
 */
typedef struct {
    uint32_t total_episodes;
    uint32_t episodes_encoded;
    uint32_t episodes_retrieved;
    uint32_t pattern_separations;
    uint32_t pattern_completions;
    uint32_t replay_events;
    uint32_t place_cells_active;
    float avg_encoding_strength;
    float avg_retrieval_accuracy;
    float theta_power;
    float gamma_power;
    float consolidation_progress;
    uint64_t last_update_time;
    uint32_t updates_processed;
} hippo_stats_t;

/**
 * @brief Main hippocampus structure
 */
typedef struct {
    /* Configuration */
    hippo_config_t config;

    /* Status */
    hippo_status_t status;
    hippo_error_t last_error;

    /* Subregion cells */
    nimcp_dg_cell_t* dg_cells;
    uint32_t num_dg_cells;
    nimcp_ca3_cell_t* ca3_cells;
    uint32_t num_ca3_cells;
    nimcp_ca1_cell_t* ca1_cells;
    uint32_t num_ca1_cells;
    nimcp_subiculum_cell_t* subiculum_cells;
    uint32_t num_subiculum_cells;

    /* Place cells */
    nimcp_place_cell_t* place_cells;
    uint32_t num_place_cells;
    uint32_t active_place_cells;

    /* Episodes */
    nimcp_episode_t* episodes;
    uint32_t num_episodes;
    uint32_t max_episodes;

    /* Replay buffer */
    nimcp_ripple_event_t* replay_buffer;
    uint32_t replay_buffer_size;
    uint32_t replay_head;

    /* Oscillation state */
    oscillation_state_t oscillation_state;
    float theta_phase;
    float gamma_phase;
    float theta_power;
    float gamma_power;

    /* Current position for navigation */
    float current_position[3];
    float current_heading;
    void* spatial_metric;  /**< Optional riemannian_metric_t* for non-Euclidean environments */

    /* Pattern buffers */
    float* dg_activation_pattern;
    float* ca3_activation_pattern;
    float* ca1_activation_pattern;
    float* subiculum_pattern;

    /* Bridge integrations */
    hippo_prime_resonance_bridge_t prime_resonance_bridge;
    hippo_immune_bridge_t immune_bridge;
    hippo_bio_async_bridge_t bio_async_bridge;
    hippo_brain_init_bridge_t brain_init_bridge;
    hippo_security_bridge_t security_bridge;
    hippo_logging_bridge_t logging_bridge;
    hippo_cognitive_bridge_t cognitive_bridge;
    hippo_training_bridge_t training_bridge;
    hippo_omni_bridge_t omni_bridge;
    hippo_hypothalamus_bridge_t hypothalamus_bridge;
    hippo_substrate_bridge_t substrate_bridge;
    hippo_thalamus_bridge_t thalamus_bridge;
    hippo_portia_bridge_t portia_bridge;
    hippo_dragonfly_bridge_t dragonfly_bridge;
    hippo_perception_bridge_t perception_bridge;
    hippo_snn_bridge_t snn_bridge;
    hippo_plasticity_bridge_t plasticity_bridge;
    hippo_entorhinal_bridge_t entorhinal_bridge;
    hippo_perirhinal_bridge_t perirhinal_bridge;
    hippo_parahippocampal_bridge_t parahippocampal_bridge;
    hippo_mammillary_bridge_t mammillary_bridge;
    hippo_cerebellum_bridge_t cerebellum_bridge;
    hippo_medulla_bridge_t medulla_bridge;
    hippo_swarm_bridge_t swarm_bridge;

    /* Statistics */
    uint32_t updates_processed;
    uint32_t encodings_performed;
    uint32_t retrievals_performed;
    uint32_t replays_performed;
    uint64_t creation_time;
    uint64_t last_update_time;
} nimcp_hippocampus_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hippo_config_t hippo_default_config(void);
nimcp_hippocampus_t* hippo_create(const hippo_config_t* config);
void hippo_destroy(nimcp_hippocampus_t* hippo);
int hippo_reset(nimcp_hippocampus_t* hippo);
int hippo_update(nimcp_hippocampus_t* hippo, float dt);

/*=============================================================================
 * EPISODIC MEMORY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Encode a new episodic memory
 */
int hippo_encode_episode(nimcp_hippocampus_t* hippo,
                          const float* what, uint32_t what_dim,
                          const float* where, uint32_t where_dim,
                          const float* when, uint32_t when_dim,
                          float emotional_valence,
                          float emotional_arousal,
                          uint32_t* episode_id_out);

/**
 * @brief Retrieve episode by cue
 */
int hippo_retrieve_episode(nimcp_hippocampus_t* hippo,
                            const float* cue, uint32_t cue_dim,
                            retrieval_mode_t mode,
                            uint32_t* episode_id_out,
                            float* match_confidence);

/**
 * @brief Get episode by ID
 */
const nimcp_episode_t* hippo_get_episode(nimcp_hippocampus_t* hippo,
                                          uint32_t episode_id);

/**
 * @brief Strengthen episode through retrieval
 */
int hippo_strengthen_episode(nimcp_hippocampus_t* hippo,
                              uint32_t episode_id,
                              float amount);

/**
 * @brief Forget episode (decay or explicit removal)
 */
int hippo_forget_episode(nimcp_hippocampus_t* hippo, uint32_t episode_id);

/**
 * @brief Find similar episodes
 */
int hippo_find_similar_episodes(nimcp_hippocampus_t* hippo,
                                 const float* query, uint32_t query_dim,
                                 uint32_t* episode_ids,
                                 float* similarities,
                                 uint32_t max_results,
                                 uint32_t* num_found);

/**
 * @brief Get episodes by type
 */
int hippo_get_episodes_by_type(nimcp_hippocampus_t* hippo,
                                episode_type_t type,
                                uint32_t* episode_ids,
                                uint32_t max_episodes,
                                uint32_t* num_found);

/**
 * @brief Get recent episodes
 */
int hippo_get_recent_episodes(nimcp_hippocampus_t* hippo,
                               uint32_t* episode_ids,
                               uint32_t max_episodes,
                               uint32_t* num_found);

/*=============================================================================
 * PATTERN SEPARATION/COMPLETION
 *===========================================================================*/

/**
 * @brief Perform pattern separation in DG
 */
int hippo_pattern_separate(nimcp_hippocampus_t* hippo,
                            const float* input, uint32_t input_dim,
                            float* separated_output, uint32_t* output_dim);

/**
 * @brief Perform pattern completion in CA3
 */
int hippo_pattern_complete(nimcp_hippocampus_t* hippo,
                            const float* partial_cue, uint32_t cue_dim,
                            float* completed_pattern, uint32_t* pattern_dim,
                            float* completion_confidence);

/**
 * @brief Check if input is novel (requires separation) or familiar (allows completion)
 */
int hippo_assess_novelty(nimcp_hippocampus_t* hippo,
                          const float* input, uint32_t input_dim,
                          float* novelty_score,
                          bool* requires_separation);

/*=============================================================================
 * SPATIAL NAVIGATION
 *===========================================================================*/

/**
 * @brief Update current position and place cells
 */
int hippo_update_position(nimcp_hippocampus_t* hippo,
                           const float* position, uint32_t dim);

/**
 * @brief Get active place cells
 */
int hippo_get_active_place_cells(nimcp_hippocampus_t* hippo,
                                  uint32_t* cell_ids,
                                  float* firing_rates,
                                  uint32_t max_cells,
                                  uint32_t* num_active);

/**
 * @brief Decode position from place cell activity
 */
int hippo_decode_position(nimcp_hippocampus_t* hippo,
                           float* decoded_position,
                           float* confidence);

/**
 * @brief Create new place field
 */
int hippo_create_place_field(nimcp_hippocampus_t* hippo,
                              const float* center, float radius,
                              uint32_t* cell_id_out);

/**
 * @brief Link episode to current location
 */
int hippo_link_episode_to_place(nimcp_hippocampus_t* hippo,
                                 uint32_t episode_id,
                                 const float* position);

/**
 * @brief Get episodes at location
 */
int hippo_get_episodes_at_location(nimcp_hippocampus_t* hippo,
                                    const float* position,
                                    float radius,
                                    uint32_t* episode_ids,
                                    uint32_t max_episodes,
                                    uint32_t* num_found);

/*=============================================================================
 * OSCILLATIONS AND RHYTHM
 *===========================================================================*/

/**
 * @brief Update theta rhythm
 */
int hippo_update_theta(nimcp_hippocampus_t* hippo, float dt);

/**
 * @brief Update gamma rhythm
 */
int hippo_update_gamma(nimcp_hippocampus_t* hippo, float dt);

/**
 * @brief Get current theta phase
 */
float hippo_get_theta_phase(nimcp_hippocampus_t* hippo);

/**
 * @brief Get current gamma phase
 */
float hippo_get_gamma_phase(nimcp_hippocampus_t* hippo);

/**
 * @brief Get oscillation power spectrum
 */
int hippo_get_oscillation_power(nimcp_hippocampus_t* hippo,
                                 float* theta_power,
                                 float* gamma_power,
                                 float* ripple_power);

/**
 * @brief Set oscillation state
 */
int hippo_set_oscillation_state(nimcp_hippocampus_t* hippo,
                                 oscillation_state_t state);

/*=============================================================================
 * MEMORY REPLAY AND CONSOLIDATION
 *===========================================================================*/

/**
 * @brief Trigger sharp-wave ripple replay
 */
int hippo_trigger_replay(nimcp_hippocampus_t* hippo,
                          replay_state_t direction);

/**
 * @brief Process replay event
 */
int hippo_process_replay(nimcp_hippocampus_t* hippo);

/**
 * @brief Get last replay event
 */
const nimcp_ripple_event_t* hippo_get_last_ripple(nimcp_hippocampus_t* hippo);

/**
 * @brief Consolidate memories (transfer to cortex)
 */
int hippo_consolidate_memories(nimcp_hippocampus_t* hippo, float dt);

/**
 * @brief Get consolidation progress for episode
 */
float hippo_get_consolidation_level(nimcp_hippocampus_t* hippo,
                                     uint32_t episode_id);

/*=============================================================================
 * SUBREGION ACCESS
 *===========================================================================*/

/**
 * @brief Get DG activation pattern
 */
int hippo_get_dg_pattern(nimcp_hippocampus_t* hippo,
                          float* pattern, uint32_t max_size,
                          uint32_t* actual_size);

/**
 * @brief Get CA3 activation pattern
 */
int hippo_get_ca3_pattern(nimcp_hippocampus_t* hippo,
                           float* pattern, uint32_t max_size,
                           uint32_t* actual_size);

/**
 * @brief Get CA1 activation pattern
 */
int hippo_get_ca1_pattern(nimcp_hippocampus_t* hippo,
                           float* pattern, uint32_t max_size,
                           uint32_t* actual_size);

/**
 * @brief Get subiculum output
 */
int hippo_get_subiculum_output(nimcp_hippocampus_t* hippo,
                                float* output, uint32_t max_size,
                                uint32_t* actual_size);

/**
 * @brief Activate DG with input pattern
 */
int hippo_activate_dg(nimcp_hippocampus_t* hippo,
                       const float* input, uint32_t input_dim);

/**
 * @brief Propagate activity through trisynaptic loop
 */
int hippo_propagate_trisynaptic(nimcp_hippocampus_t* hippo);

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW
 *===========================================================================*/

int hippo_process_incoming(nimcp_hippocampus_t* hippo);
int hippo_send_outgoing(nimcp_hippocampus_t* hippo);
int hippo_bidirectional_update(nimcp_hippocampus_t* hippo, float dt);

/* Specific synchronization */
int hippo_sync_entorhinal(nimcp_hippocampus_t* hippo);
int hippo_sync_perirhinal(nimcp_hippocampus_t* hippo);
int hippo_sync_parahippocampal(nimcp_hippocampus_t* hippo);
int hippo_sync_mammillary(nimcp_hippocampus_t* hippo);
int hippo_sync_thalamus(nimcp_hippocampus_t* hippo);
int hippo_sync_cortical(nimcp_hippocampus_t* hippo);

/*=============================================================================
 * BRIDGE INITIALIZATION FUNCTIONS
 *===========================================================================*/

int hippo_init_prime_resonance_bridge(nimcp_hippocampus_t* hippo, void* pr_memory);
int hippo_init_immune_bridge(nimcp_hippocampus_t* hippo, void* immune);
int hippo_init_bio_async_bridge(nimcp_hippocampus_t* hippo, void* runtime);
int hippo_init_brain_init_bridge(nimcp_hippocampus_t* hippo, void* brain_init);
int hippo_init_security_bridge(nimcp_hippocampus_t* hippo, void* security_ctx, void* security_ops);
int hippo_init_logging_bridge(nimcp_hippocampus_t* hippo, void* logger);
int hippo_init_cognitive_bridge(nimcp_hippocampus_t* hippo, void* cognitive);
int hippo_init_training_bridge(nimcp_hippocampus_t* hippo, void* training);
int hippo_init_omni_bridge(nimcp_hippocampus_t* hippo, void* omni);
int hippo_init_hypothalamus_bridge(nimcp_hippocampus_t* hippo, void* hypothalamus);
int hippo_init_substrate_bridge(nimcp_hippocampus_t* hippo, void* substrate);
int hippo_init_thalamus_bridge(nimcp_hippocampus_t* hippo, void* thalamus);
int hippo_init_portia_bridge(nimcp_hippocampus_t* hippo, void* portia);
int hippo_init_dragonfly_bridge(nimcp_hippocampus_t* hippo, void* dragonfly);
int hippo_init_perception_bridge(nimcp_hippocampus_t* hippo, void* perception);
int hippo_init_snn_bridge(nimcp_hippocampus_t* hippo, void* snn);
int hippo_init_plasticity_bridge(nimcp_hippocampus_t* hippo, void* plasticity, void* stdp);
int hippo_init_entorhinal_bridge(nimcp_hippocampus_t* hippo, void* entorhinal);
int hippo_init_perirhinal_bridge(nimcp_hippocampus_t* hippo, void* perirhinal);
int hippo_init_parahippocampal_bridge(nimcp_hippocampus_t* hippo, void* parahippocampal);
int hippo_init_mammillary_bridge(nimcp_hippocampus_t* hippo, void* mammillary);
int hippo_init_cerebellum_bridge(nimcp_hippocampus_t* hippo, void* cerebellum);
int hippo_init_medulla_bridge(nimcp_hippocampus_t* hippo, void* medulla);
int hippo_init_swarm_bridge(nimcp_hippocampus_t* hippo, void* swarm);

/**
 * @brief Initialize all bridges at once
 */
int hippo_init_all_bridges(nimcp_hippocampus_t* hippo, void** bridge_contexts);

/*=============================================================================
 * PRIME RESONANCE INTEGRATION
 *===========================================================================*/

/**
 * @brief Tag episode with prime resonance signature
 */
int hippo_tag_with_resonance(nimcp_hippocampus_t* hippo,
                              uint32_t episode_id,
                              uint64_t resonance_signature);

/**
 * @brief Find episodes by resonance signature
 */
int hippo_find_by_resonance(nimcp_hippocampus_t* hippo,
                             uint64_t resonance_signature,
                             uint32_t* episode_ids,
                             uint32_t max_episodes,
                             uint32_t* num_found);

/**
 * @brief Enhance encoding with prime resonance
 */
int hippo_resonance_enhanced_encode(nimcp_hippocampus_t* hippo,
                                     const float* content, uint32_t dim,
                                     uint32_t* episode_id_out);

/**
 * @brief Resonance-guided retrieval
 */
int hippo_resonance_guided_retrieve(nimcp_hippocampus_t* hippo,
                                     const float* cue, uint32_t cue_dim,
                                     uint32_t* episode_id_out,
                                     float* confidence);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

hippo_status_t hippo_get_status(nimcp_hippocampus_t* hippo);
hippo_error_t hippo_get_last_error(nimcp_hippocampus_t* hippo);
const char* hippo_error_string(hippo_error_t error);
const char* hippo_status_string(hippo_status_t status);
int hippo_get_stats(nimcp_hippocampus_t* hippo, hippo_stats_t* stats);
int hippo_get_config(nimcp_hippocampus_t* hippo, hippo_config_t* config);
float hippo_get_health_status(nimcp_hippocampus_t* hippo);
int hippo_log_diagnostics(nimcp_hippocampus_t* hippo);

/*=============================================================================
 * CELL ACTIVITY QUERIES
 *===========================================================================*/

size_t hippo_get_dg_cell_activity(nimcp_hippocampus_t* hippo, float* activity, size_t max);
size_t hippo_get_ca3_cell_activity(nimcp_hippocampus_t* hippo, float* activity, size_t max);
size_t hippo_get_ca1_cell_activity(nimcp_hippocampus_t* hippo, float* activity, size_t max);
size_t hippo_get_subiculum_activity(nimcp_hippocampus_t* hippo, float* activity, size_t max);

/*=============================================================================
 * SERIALIZATION
 *===========================================================================*/

size_t hippo_get_serialization_size(nimcp_hippocampus_t* hippo);
int hippo_serialize(nimcp_hippocampus_t* hippo, uint8_t* buffer, size_t size, size_t* written);
nimcp_hippocampus_t* hippo_deserialize(const uint8_t* buffer, size_t size, size_t* bytes_read);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HIPPOCAMPUS_H */
