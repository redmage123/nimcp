/**
 * @file nimcp_cerebellum_adapter.h
 * @brief Brain adapter for Cerebellum integration
 *
 * WHAT: Unified adapter connecting cerebellum sub-modules to the brain system
 * WHY:  Enable motor coordination, timing, and error-based learning
 * HOW:  Orchestrates Purkinje cells, granule cells, parallel fibers, and climbing fibers
 *
 * ARCHITECTURE:
 * - Wraps all cerebellar sub-modules (granule, Purkinje, deep nuclei)
 * - Provides high-level API for motor coordination pipeline
 * - Integrates with motor cortex for movement planning
 * - Connects to basal ganglia for action selection coordination
 * - Supports error-based learning through climbing fiber signals
 *
 * BIOLOGICAL BASIS:
 * - Cerebellar cortex: Purkinje cells, granule cells, parallel fibers
 * - Deep cerebellar nuclei: Dentate, interposed, fastigial
 * - Climbing fibers from inferior olive: Error signals
 * - Mossy fibers from pontine nuclei: Motor commands
 *
 * COMPUTATIONAL MODEL:
 * - Marr-Albus-Ito model: Long-term depression at parallel fiber synapses
 * - Timing: Precise temporal control through Purkinje cell firing patterns
 * - Forward models: Predictive motor control
 *
 * @version Phase B4: Cerebellum Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_CEREBELLUM_ADAPTER_H
#define NIMCP_CEREBELLUM_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Logging system */
#include "utils/logging/nimcp_logging.h"

/* Unified memory system */
#include "utils/memory/nimcp_unified_memory.h"

/* Forward declarations for sub-modules */
typedef struct granule_layer granule_layer_t;
typedef struct purkinje_layer purkinje_layer_t;
typedef struct deep_nuclei deep_nuclei_t;
typedef struct climbing_fiber_system climbing_fiber_system_t;

/* Forward declarations for new sub-modules (Phase 1-2) */
typedef struct glomerular_layer glomerular_layer_t;
typedef struct molecular_layer_interneurons molecular_layer_interneurons_t;
typedef struct golgi_cell_network golgi_cell_network_t;

/* Forward declaration for opaque adapter type */
typedef struct cerebellum_adapter cerebellum_adapter_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define CEREBELLUM_DEFAULT_GRANULE_CELLS        10000
#define CEREBELLUM_DEFAULT_PURKINJE_CELLS       100
#define CEREBELLUM_DEFAULT_PARALLEL_FIBERS      1000
#define CEREBELLUM_DEFAULT_CLIMBING_FIBERS      100
#define CEREBELLUM_DEFAULT_TIMING_RESOLUTION_MS 1.0f
#define CEREBELLUM_DEFAULT_LTD_RATE             0.001f
#define CEREBELLUM_DEFAULT_LTP_RATE             0.0001f

/* Interneuron defaults (Phase 2) */
#define CEREBELLUM_DEFAULT_BASKET_CELLS         10    /**< num_purkinje/10 */
#define CEREBELLUM_DEFAULT_PURKINJE_PER_BASKET  5     /**< Purkinje cells per basket */
#define CEREBELLUM_DEFAULT_STELLATE_CELLS       20    /**< num_purkinje/5 */
#define CEREBELLUM_DEFAULT_PURKINJE_PER_STELLATE 3    /**< Purkinje cells per stellate */
#define CEREBELLUM_DEFAULT_GOLGI_CELLS          20    /**< num_granule/500 */
#define CEREBELLUM_DEFAULT_GRANULES_PER_GOLGI   75    /**< Granule cells per Golgi */

/* Glomerular divergence defaults (Phase 1) */
#define CEREBELLUM_DEFAULT_GRANULES_PER_MOSSY   75    /**< 50-100 divergence */

/* Synaptic dynamics defaults (Phase 1) */
#define CEREBELLUM_SYNAPSE_DEFAULT_RRP_CAPACITY     10
#define CEREBELLUM_SYNAPSE_DEFAULT_RECYCLING_CAP    100
#define CEREBELLUM_SYNAPSE_DEFAULT_RELEASE_PROB     0.3f
#define CEREBELLUM_SYNAPSE_DEFAULT_REFILL_RATE      2.0f
#define CEREBELLUM_SYNAPSE_DEFAULT_STP_U            0.5f
#define CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_D        200.0f
#define CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_F        50.0f
#define CEREBELLUM_SYNAPSE_DEFAULT_CA_BASELINE      0.1f
#define CEREBELLUM_SYNAPSE_DEFAULT_CA_DECAY_TAU     100.0f
#define CEREBELLUM_SYNAPSE_DEFAULT_CA_INFLUX        1.0f

/*=============================================================================
 * SYNAPTIC DYNAMICS (Phase 1)
 *===========================================================================*/

/**
 * @brief Configuration for cerebellar synaptic dynamics
 *
 * WHAT: Parameters for vesicle pool, short-term plasticity, and calcium dynamics
 * WHY:  Enable realistic synaptic transmission and plasticity
 * HOW:  Integrates vesicle depletion, facilitation/depression, and Ca2+ signaling
 */
typedef struct {
    /* Vesicle pool configuration */
    uint32_t rrp_capacity;           /**< Readily releasable pool capacity */
    uint32_t recycling_capacity;     /**< Recycling pool capacity */
    float release_probability;       /**< Base release probability [0, 1] */
    float refill_rate;               /**< Vesicles/sec refill rate */

    /* Short-term plasticity (Tsodyks-Markram model) */
    float stp_U;                     /**< Baseline release probability */
    float stp_tau_D;                 /**< Depression time constant (ms) */
    float stp_tau_F;                 /**< Facilitation time constant (ms) */

    /* Calcium dynamics for plasticity */
    float ca_baseline;               /**< Baseline [Ca2+] (uM) */
    float ca_decay_tau;              /**< Calcium decay tau (ms) */
    float ca_influx_per_spike;       /**< Calcium per spike (uM) */

    /* Enable flags */
    bool enable_vesicle_dynamics;    /**< Enable vesicle pool simulation */
    bool enable_stp;                 /**< Enable short-term plasticity */
    bool enable_calcium_dynamics;    /**< Enable calcium-based learning */
} cerebellar_synapse_config_t;

/**
 * @brief State for cerebellar synaptic dynamics
 *
 * WHAT: Runtime state for one synapse's dynamics
 * WHY:  Tracks vesicle availability, STP state, and calcium
 * HOW:  Updated on each presynaptic spike and continuous decay
 */
typedef struct {
    /* Vesicle pool state (readily releasable) */
    uint32_t rrp_available;          /**< Available vesicles in RRP */
    uint32_t rrp_capacity;           /**< Maximum RRP capacity */
    float vesicle_depletion;         /**< Depletion factor [0, 1] */

    /* Short-term plasticity state */
    float stp_x;                     /**< Available resources [0, 1] */
    float stp_u;                     /**< Utilization probability [0, 1] */
    uint64_t last_spike_time;        /**< Last spike timestamp */

    /* Calcium state */
    float calcium_concentration;     /**< Current [Ca2+] (uM) */
    float calcium_decay_tau;         /**< Decay time constant */

    /* Facilitation/Depression factors */
    float facilitation_factor;       /**< Current facilitation [0, 2] */
    float depression_factor;         /**< Current depression [0, 1] */
    float effective_weight;          /**< Weight after STP modulation */
} cerebellar_synapse_state_t;

/*=============================================================================
 * GLOMERULUS STRUCTURE (Phase 1)
 *===========================================================================*/

/**
 * @brief Glomerulus for mossy fiber divergence
 *
 * BIOLOGICAL: Mossy fiber rosettes contact 50-100 granule cell dendrites
 * Each glomerulus is a synaptic complex where one mossy fiber
 * diverges to many granule cells with potential Golgi cell inhibition.
 */
typedef struct {
    uint32_t mossy_fiber_id;         /**< Source mossy fiber ID */
    uint32_t* granule_targets;       /**< Array of target granule cell IDs */
    uint32_t num_targets;            /**< Number of targets (50-100) */
    float* synapse_weights;          /**< Weight to each granule cell */
    float activity;                  /**< Current glomerulus activity */
    cerebellar_synapse_state_t synapse; /**< Synaptic dynamics state */
} glomerulus_t;

/*=============================================================================
 * INTERNEURON STRUCTURES (Phase 2)
 *===========================================================================*/

/**
 * @brief Basket cell state - inhibits Purkinje cell soma
 *
 * BIOLOGICAL: Fast-spiking interneurons in molecular layer
 * Receive parallel fiber input, provide strong perisomatic inhibition
 * to ~5 nearby Purkinje cells
 */
typedef struct {
    float activation;                /**< Current activation [0, 1] */
    float* purkinje_weights;         /**< Inhibitory weights to Purkinje cells */
    uint32_t num_purkinje_targets;   /**< Number of Purkinje cells targeted (~5) */
    uint32_t* target_purkinje_ids;   /**< IDs of target Purkinje cells */
    float firing_rate;               /**< Current firing rate (Hz) */
    float somatic_inhibition;        /**< Total inhibition output */
    cerebellar_synapse_state_t synapse; /**< Synaptic dynamics */
} basket_cell_t;

/**
 * @brief Stellate cell state - inhibits Purkinje dendrites
 *
 * BIOLOGICAL: Stellate cells in outer molecular layer
 * Provide dendritic inhibition to ~2-3 Purkinje cells
 * Receive parallel fiber input from granule cells
 */
typedef struct {
    float activation;                /**< Current activation [0, 1] */
    float* purkinje_weights;         /**< Inhibitory weights to Purkinje dendrites */
    uint32_t num_purkinje_targets;   /**< Number of Purkinje cells targeted (~2-3) */
    uint32_t* target_purkinje_ids;   /**< IDs of target Purkinje cells */
    float dendritic_inhibition;      /**< Inhibition delivered to dendrites */
    cerebellar_synapse_state_t synapse; /**< Synaptic dynamics */
} stellate_cell_t;

/**
 * @brief Golgi cell state - feedback inhibition to granule cells
 *
 * BIOLOGICAL: Granule layer interneurons forming feedback loop
 * Receive parallel fiber and mossy fiber input
 * Provide feedback inhibition to ~50-100 granule cells
 */
typedef struct {
    float activation;                /**< Current activation [0, 1] */
    float* granule_weights;          /**< Inhibitory weights to granule cells */
    uint32_t num_granule_targets;    /**< Number of granule cells targeted (~50-100) */
    uint32_t* target_granule_ids;    /**< IDs of target granule cells */
    float feedback_inhibition;       /**< Total inhibition output */
    float parallel_fiber_input;      /**< Input from parallel fibers */
    float mossy_fiber_input;         /**< Direct mossy fiber input */
    cerebellar_synapse_state_t synapse; /**< Synaptic dynamics */
} golgi_cell_t;

/**
 * @brief Cerebellum adapter configuration
 */
typedef struct {
    /* Cell counts */
    uint32_t num_granule_cells;       /**< Number of granule cells */
    uint32_t num_purkinje_cells;      /**< Number of Purkinje cells */
    uint32_t num_parallel_fibers;     /**< Number of parallel fibers */
    uint32_t num_climbing_fibers;     /**< Number of climbing fibers */
    uint32_t num_mossy_fibers;        /**< Number of mossy fibers */

    /* Deep nuclei configuration */
    uint32_t num_dentate_neurons;     /**< Dentate nucleus neurons */
    uint32_t num_interposed_neurons;  /**< Interposed nucleus neurons */
    uint32_t num_fastigial_neurons;   /**< Fastigial nucleus neurons */

    /* Processing options */
    bool enable_timing;               /**< Enable precise timing computations */
    bool enable_error_learning;       /**< Enable climbing fiber error learning */
    bool enable_forward_models;       /**< Enable predictive forward models */
    bool enable_motor_adaptation;     /**< Enable motor adaptation */

    /* Plasticity parameters */
    float ltd_rate;                   /**< Long-term depression rate (Purkinje) */
    float ltp_rate;                   /**< Long-term potentiation rate (granule) */
    float learning_rate;              /**< Base learning rate */

    /* Timing parameters */
    float timing_resolution_ms;       /**< Temporal resolution */
    float max_prediction_horizon_ms;  /**< Max forward prediction time */

    /* Event system */
    bool enable_events;               /**< Enable event bus integration */

    /* Bio-async communication */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */

    /*=== Phase 1: Synaptic Dynamics (disabled by default for backward compat) ===*/

    /* Synaptic dynamics configuration */
    cerebellar_synapse_config_t synapse_config; /**< Synaptic dynamics params */
    bool enable_synaptic_dynamics;    /**< Master switch for dynamics */

    /* Glomerular divergence */
    uint32_t granules_per_mossy_fiber;/**< Target divergence (50-100) */
    bool enable_glomerular_divergence;/**< Enable glomerulus model */

    /*=== Phase 2: Inhibitory Interneurons (disabled by default) ===*/

    /* Basket cells - inhibit Purkinje soma */
    uint32_t num_basket_cells;        /**< Number of basket cells */
    uint32_t purkinje_per_basket;     /**< Purkinje targets per basket (~5) */
    bool enable_basket_cells;         /**< Enable basket cell inhibition */

    /* Stellate cells - inhibit Purkinje dendrites */
    uint32_t num_stellate_cells;      /**< Number of stellate cells */
    uint32_t purkinje_per_stellate;   /**< Purkinje targets per stellate (~2-3) */
    bool enable_stellate_cells;       /**< Enable stellate cell inhibition */

    /* Golgi cells - feedback inhibition to granules */
    uint32_t num_golgi_cells;         /**< Number of Golgi cells */
    uint32_t granules_per_golgi;      /**< Granule targets per Golgi (~50-100) */
    bool enable_golgi_cells;          /**< Enable Golgi feedback inhibition */

    /*=== Phase 3: Vestibulocerebellum (disabled by default) ===*/

    /* Vestibulocerebellum configuration */
    uint32_t num_flocculus_purkinje;  /**< Flocculus Purkinje cell count */
    uint32_t num_nodulus_purkinje;    /**< Nodulus Purkinje cell count */
    bool enable_vestibulocerebellum;  /**< Enable vestibular integration */
    bool enable_vor_adaptation;       /**< Enable VOR gain adaptation */
    float vor_ltd_rate;               /**< VOR-specific LTD rate */
} cerebellum_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Processing status of the adapter
 */
typedef enum {
    CEREBELLUM_STATUS_IDLE = 0,       /**< Ready for input */
    CEREBELLUM_STATUS_RECEIVING,      /**< Receiving mossy fiber input */
    CEREBELLUM_STATUS_PROCESSING,     /**< Granule-Purkinje processing */
    CEREBELLUM_STATUS_TIMING,         /**< Timing computation */
    CEREBELLUM_STATUS_LEARNING,       /**< Error-based learning active */
    CEREBELLUM_STATUS_OUTPUT_READY,   /**< Motor commands ready */
    CEREBELLUM_STATUS_ERROR           /**< Error state */
} cerebellum_status_t;

/**
 * @brief Error codes for cerebellum operations
 */
typedef enum {
    CEREBELLUM_ERROR_NONE = 0,
    CEREBELLUM_ERROR_INVALID_INPUT,
    CEREBELLUM_ERROR_GRANULE_FAILURE,
    CEREBELLUM_ERROR_PURKINJE_FAILURE,
    CEREBELLUM_ERROR_TIMING_FAILURE,
    CEREBELLUM_ERROR_LEARNING_FAILURE,
    CEREBELLUM_ERROR_NUCLEI_FAILURE,
    CEREBELLUM_ERROR_BUFFER_OVERFLOW,
    CEREBELLUM_ERROR_INTERNAL
} cerebellum_error_t;

/*=============================================================================
 * INPUT/OUTPUT STRUCTURES
 *===========================================================================*/

/**
 * @brief Mossy fiber input (from pontine nuclei/spinal cord)
 */
typedef struct {
    uint32_t fiber_id;               /**< Fiber identifier */
    float activity;                  /**< Activity level [0, 1] */
    float timestamp_ms;              /**< Timing information */
    uint8_t modality;                /**< Input modality (motor, sensory, etc.) */
} mossy_fiber_input_t;

/**
 * @brief Climbing fiber signal (error from inferior olive)
 */
typedef struct {
    uint32_t fiber_id;               /**< Fiber identifier */
    float error_signal;              /**< Error magnitude [-1, 1] */
    float timestamp_ms;              /**< When error occurred */
    uint32_t target_purkinje_id;     /**< Target Purkinje cell */
    uint8_t error_type;              /**< Type of error (timing, force, etc.) */
} climbing_fiber_signal_t;

/**
 * @brief Purkinje cell output
 */
typedef struct {
    uint32_t purkinje_id;            /**< Purkinje cell ID */
    float firing_rate;               /**< Output firing rate */
    float simple_spike_rate;         /**< Simple spike frequency */
    float complex_spike_flag;        /**< Complex spike occurrence (0 or 1) */
    float inhibition_to_nuclei;      /**< Inhibitory output to deep nuclei */
} purkinje_output_t;

/**
 * @brief Deep nuclei output (motor command)
 */
typedef struct {
    uint32_t nucleus_id;             /**< Nucleus ID (dentate/interposed/fastigial) */
    float activity;                  /**< Output activity level */
    float motor_command[8];          /**< Motor command vector (up to 8 DOF) */
    uint32_t num_dimensions;         /**< Actual DOF used */
    float timestamp_ms;              /**< Command timestamp */
    float timing_adjustment;         /**< Timing correction factor */
} nuclei_output_t;

/**
 * @brief Motor coordination result
 */
typedef struct {
    /* Timing outputs */
    float predicted_timing_ms;       /**< Predicted event timing */
    float timing_error_ms;           /**< Current timing error */
    float timing_correction;         /**< Correction to apply */

    /* Motor outputs */
    float motor_gain[8];             /**< Gain adjustments per DOF */
    uint32_t num_motor_dims;         /**< Number of motor dimensions */
    bool motor_ready;                /**< Motor command is ready */

    /* Learning state */
    float current_error;             /**< Current prediction error */
    float learning_progress;         /**< Learning convergence [0, 1] */
    bool adaptation_active;          /**< Adaptation is occurring */

    /* Forward model */
    float predicted_outcome[8];      /**< Forward model prediction */
    float confidence;                /**< Prediction confidence */
} motor_coordination_result_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t mossy_inputs_processed; /**< Total mossy fiber inputs */
    uint64_t climbing_signals;       /**< Total climbing fiber signals */
    uint64_t motor_commands_output;  /**< Total motor commands generated */

    /* Cell activity */
    uint64_t granule_activations;    /**< Total granule cell activations */
    uint64_t purkinje_simple_spikes; /**< Total simple spikes */
    uint64_t purkinje_complex_spikes;/**< Total complex spikes */

    /* Learning statistics */
    uint64_t ltd_events;             /**< Long-term depression events */
    uint64_t ltp_events;             /**< Long-term potentiation events */
    float avg_error_before_learning; /**< Average error before learning */
    float avg_error_after_learning;  /**< Average error after learning */

    /* Timing */
    float avg_timing_error_ms;       /**< Average timing error */
    float max_timing_error_ms;       /**< Maximum timing error */
    float avg_latency_ms;            /**< Average processing latency */

    /*=== Phase 1-2: New statistics for interneurons and synaptic dynamics ===*/

    /* Interneuron activity (Phase 2) */
    uint64_t basket_cell_spikes;     /**< Total basket cell spikes */
    uint64_t stellate_cell_spikes;   /**< Total stellate cell spikes */
    uint64_t golgi_cell_spikes;      /**< Total Golgi cell spikes */
    float basket_inhibition_total;   /**< Cumulative basket inhibition */
    float stellate_inhibition_total; /**< Cumulative stellate inhibition */
    float golgi_feedback_total;      /**< Cumulative Golgi feedback */
    float avg_basket_firing_rate;    /**< Average basket firing rate (Hz) */
    float avg_stellate_firing_rate;  /**< Average stellate firing rate (Hz) */
    float avg_golgi_firing_rate;     /**< Average Golgi firing rate (Hz) */

    /* Synaptic dynamics (Phase 1) */
    uint64_t vesicle_releases;       /**< Total vesicle release events */
    uint64_t vesicle_depletion_events;/**< Times RRP depleted */
    float avg_release_probability;   /**< Average Pr across synapses */
    float avg_facilitation;          /**< Average facilitation factor */
    float avg_depression;            /**< Average depression factor */
    float avg_calcium_concentration; /**< Average [Ca2+] */

    /* Glomerular stats (Phase 1) */
    uint64_t glomerular_activations; /**< Total glomerulus activations */
    float effective_divergence;      /**< Actual mossy->granule ratio */

    /* Vestibulocerebellum stats (Phase 3) */
    uint64_t vestibular_inputs;      /**< Total vestibular inputs processed */
    uint64_t vor_corrections;        /**< Total VOR corrections made */
    float avg_vor_gain;              /**< Average VOR gain */
    float vor_error_total;           /**< Cumulative VOR error */
} cerebellum_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for motor output (integration with motor cortex)
 */
typedef void (*cerebellum_motor_callback_t)(
    const nuclei_output_t* output,
    void* user_data
);

/**
 * @brief Callback for error feedback (from sensory system)
 */
typedef void (*cerebellum_error_callback_t)(
    const climbing_fiber_signal_t* error,
    void* user_data
);

/**
 * @brief Callback for event notification
 */
typedef void (*cerebellum_event_callback_t)(
    uint32_t event_type,
    const void* event_data,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration for cerebellum adapter
 * WHY:  Provide sensible defaults for motor coordination
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
cerebellum_config_t cerebellum_default_config(void);

/**
 * @brief Create cerebellum adapter
 *
 * WHAT: Allocate and initialize the adapter with all sub-modules
 * WHY:  Central point for motor coordination initialization
 * HOW:  Create granule, Purkinje, nuclei layers; initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New adapter instance, or NULL on failure
 */
cerebellum_adapter_t* cerebellum_create(const cerebellum_config_t* config);

/**
 * @brief Destroy cerebellum adapter
 *
 * WHAT: Free all resources associated with the adapter
 * WHY:  Prevent memory leaks
 * HOW:  Destroy sub-modules, free buffers
 *
 * @param adapter Adapter to destroy
 */
void cerebellum_destroy(cerebellum_adapter_t* adapter);

/**
 * @brief Reset adapter state
 *
 * WHAT: Clear buffers and reset to idle state
 * WHY:  Prepare for new motor sequence
 * HOW:  Reset all sub-modules, clear activity
 *
 * @param adapter Adapter instance
 * @return true on success, false on failure
 */
bool cerebellum_reset(cerebellum_adapter_t* adapter);

/*=============================================================================
 * MOSSY FIBER INPUT PROCESSING
 *===========================================================================*/

/**
 * @brief Process mossy fiber input
 *
 * WHAT: Feed motor command or sensory input through mossy fibers
 * WHY:  First stage of cerebellar processing
 * HOW:  Distribute to granule cells via mossy fiber rosettes
 *
 * @param adapter Adapter instance
 * @param input Mossy fiber input
 * @return true on success
 */
bool cerebellum_process_mossy_input(cerebellum_adapter_t* adapter,
                                     const mossy_fiber_input_t* input);

/**
 * @brief Process batch of mossy fiber inputs
 *
 * WHAT: Feed multiple inputs simultaneously
 * WHY:  Efficient batch processing for motor patterns
 * HOW:  Process all inputs in parallel through granule layer
 *
 * @param adapter Adapter instance
 * @param inputs Array of mossy fiber inputs
 * @param count Number of inputs
 * @return true on success
 */
bool cerebellum_process_mossy_batch(cerebellum_adapter_t* adapter,
                                     const mossy_fiber_input_t* inputs,
                                     uint32_t count);

/*=============================================================================
 * CLIMBING FIBER ERROR PROCESSING
 *===========================================================================*/

/**
 * @brief Process climbing fiber error signal
 *
 * WHAT: Feed error signal from inferior olive
 * WHY:  Trigger error-based learning (LTD)
 * HOW:  Target specific Purkinje cells, trigger complex spikes
 *
 * @param adapter Adapter instance
 * @param signal Climbing fiber signal
 * @return true on success
 */
bool cerebellum_process_climbing_signal(cerebellum_adapter_t* adapter,
                                         const climbing_fiber_signal_t* signal);

/**
 * @brief Broadcast error to all Purkinje cells
 *
 * WHAT: Send global error signal
 * WHY:  General performance error affects all cells
 * HOW:  Scale by error magnitude, trigger LTD
 *
 * @param adapter Adapter instance
 * @param error_magnitude Error magnitude [-1, 1]
 * @param error_type Type of error
 * @return true on success
 */
bool cerebellum_broadcast_error(cerebellum_adapter_t* adapter,
                                 float error_magnitude,
                                 uint8_t error_type);

/*=============================================================================
 * MOTOR COORDINATION PIPELINE
 *===========================================================================*/

/**
 * @brief Begin motor coordination sequence
 *
 * WHAT: Initialize pipeline for new motor sequence
 * WHY:  Clear previous state, prepare for new movement
 * HOW:  Reset timing, clear predictions
 *
 * @param adapter Adapter instance
 * @return true on success
 */
bool cerebellum_begin_coordination(cerebellum_adapter_t* adapter);

/**
 * @brief Process current state through cerebellum
 *
 * WHAT: Run full cerebellar processing pipeline
 * WHY:  Generate motor coordination output
 * HOW:  Granule → Purkinje → Deep nuclei
 *
 * @param adapter Adapter instance
 * @param result Output coordination result
 * @return true on success
 */
bool cerebellum_process(cerebellum_adapter_t* adapter,
                         motor_coordination_result_t* result);

/**
 * @brief Get deep nuclei output
 *
 * WHAT: Retrieve motor command from deep nuclei
 * WHY:  Interface with motor cortex
 * HOW:  Read from dentate/interposed/fastigial outputs
 *
 * @param adapter Adapter instance
 * @param output Output structure
 * @return true if output available
 */
bool cerebellum_get_nuclei_output(cerebellum_adapter_t* adapter,
                                   nuclei_output_t* output);

/*=============================================================================
 * TIMING CONTROL
 *===========================================================================*/

/**
 * @brief Set target timing for movement
 *
 * WHAT: Specify desired movement timing
 * WHY:  Cerebellum provides precise timing control
 * HOW:  Store target, compare with actual timing
 *
 * @param adapter Adapter instance
 * @param target_ms Target time in milliseconds
 * @return true on success
 */
bool cerebellum_set_target_timing(cerebellum_adapter_t* adapter,
                                   float target_ms);

/**
 * @brief Get timing prediction
 *
 * WHAT: Predict when movement will occur
 * WHY:  Forward model for anticipatory control
 * HOW:  Use internal model to estimate timing
 *
 * @param adapter Adapter instance
 * @param predicted_ms Output: predicted time
 * @param confidence Output: prediction confidence
 * @return true on success
 */
bool cerebellum_predict_timing(cerebellum_adapter_t* adapter,
                                float* predicted_ms,
                                float* confidence);

/**
 * @brief Report actual timing
 *
 * WHAT: Feed back actual movement timing
 * WHY:  Update timing error for learning
 * HOW:  Compare with prediction, trigger learning if error
 *
 * @param adapter Adapter instance
 * @param actual_ms Actual time in milliseconds
 * @return true on success
 */
bool cerebellum_report_timing(cerebellum_adapter_t* adapter,
                               float actual_ms);

/*=============================================================================
 * FORWARD MODEL OPERATIONS
 *===========================================================================*/

/**
 * @brief Update forward model with new observation
 *
 * WHAT: Train forward model on motor-outcome pair
 * WHY:  Improve predictive control
 * HOW:  Supervised learning on (command, outcome) pairs
 *
 * @param adapter Adapter instance
 * @param motor_command Motor command vector
 * @param outcome Observed outcome vector
 * @param num_dims Number of dimensions
 * @return true on success
 */
bool cerebellum_update_forward_model(cerebellum_adapter_t* adapter,
                                      const float* motor_command,
                                      const float* outcome,
                                      uint32_t num_dims);

/**
 * @brief Predict outcome from motor command
 *
 * WHAT: Use forward model to predict outcome
 * WHY:  Anticipatory motor control
 * HOW:  Feed command through internal model
 *
 * @param adapter Adapter instance
 * @param motor_command Motor command vector
 * @param num_dims Number of dimensions
 * @param predicted_outcome Output: predicted outcome
 * @param confidence Output: prediction confidence
 * @return true on success
 */
bool cerebellum_predict_outcome(cerebellum_adapter_t* adapter,
                                 const float* motor_command,
                                 uint32_t num_dims,
                                 float* predicted_outcome,
                                 float* confidence);

/*=============================================================================
 * MOTOR ADAPTATION
 *===========================================================================*/

/**
 * @brief Apply motor gain adaptation
 *
 * WHAT: Adjust motor gains based on error
 * WHY:  Motor adaptation (VOR, reaching, etc.)
 * HOW:  Scale gains using cerebellar output
 *
 * @param adapter Adapter instance
 * @param gains Current gains (in/out)
 * @param num_dims Number of dimensions
 * @param adaptation_rate Rate of adaptation [0, 1]
 * @return true on success
 */
bool cerebellum_adapt_gains(cerebellum_adapter_t* adapter,
                             float* gains,
                             uint32_t num_dims,
                             float adaptation_rate);

/**
 * @brief Get current adaptation state
 *
 * WHAT: Retrieve current adaptation level
 * WHY:  Monitor adaptation progress
 * HOW:  Read internal adaptation state
 *
 * @param adapter Adapter instance
 * @param adaptation_level Output: adaptation level [0, 1]
 * @return true on success
 */
bool cerebellum_get_adaptation_state(cerebellum_adapter_t* adapter,
                                      float* adaptation_level);

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

/**
 * @brief Set motor output callback
 *
 * @param adapter Adapter instance
 * @param callback Motor output handler
 * @param user_data User context
 * @return true on success
 */
bool cerebellum_set_motor_callback(cerebellum_adapter_t* adapter,
                                    cerebellum_motor_callback_t callback,
                                    void* user_data);

/**
 * @brief Set error input callback
 *
 * @param adapter Adapter instance
 * @param callback Error input handler
 * @param user_data User context
 * @return true on success
 */
bool cerebellum_set_error_callback(cerebellum_adapter_t* adapter,
                                    cerebellum_error_callback_t callback,
                                    void* user_data);

/**
 * @brief Set event callback
 *
 * @param adapter Adapter instance
 * @param callback Event handler
 * @param user_data User context
 * @return true on success
 */
bool cerebellum_set_event_callback(cerebellum_adapter_t* adapter,
                                    cerebellum_event_callback_t callback,
                                    void* user_data);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current processing status
 *
 * @param adapter Adapter instance
 * @return Current status
 */
cerebellum_status_t cerebellum_get_status(const cerebellum_adapter_t* adapter);

/**
 * @brief Get last error code
 *
 * @param adapter Adapter instance
 * @return Last error, or CEREBELLUM_ERROR_NONE
 */
cerebellum_error_t cerebellum_get_last_error(const cerebellum_adapter_t* adapter);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* cerebellum_error_string(cerebellum_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable status description
 */
const char* cerebellum_status_string(cerebellum_status_t status);

/**
 * @brief Get adapter statistics
 *
 * @param adapter Adapter instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool cerebellum_get_stats(const cerebellum_adapter_t* adapter, cerebellum_stats_t* stats);

/**
 * @brief Get adapter configuration
 *
 * @param adapter Adapter instance
 * @param config Output configuration structure
 * @return true on success
 */
bool cerebellum_get_config(const cerebellum_adapter_t* adapter, cerebellum_config_t* config);

/*=============================================================================
 * SUB-MODULE ACCESS (Advanced)
 *===========================================================================*/

/**
 * @brief Get granule layer handle
 *
 * @param adapter Adapter instance
 * @return Granule layer, or NULL
 */
granule_layer_t* cerebellum_get_granule_layer(cerebellum_adapter_t* adapter);

/**
 * @brief Get Purkinje layer handle
 *
 * @param adapter Adapter instance
 * @return Purkinje layer, or NULL
 */
purkinje_layer_t* cerebellum_get_purkinje_layer(cerebellum_adapter_t* adapter);

/**
 * @brief Get deep nuclei handle
 *
 * @param adapter Adapter instance
 * @return Deep nuclei, or NULL
 */
deep_nuclei_t* cerebellum_get_deep_nuclei(cerebellum_adapter_t* adapter);

/*=============================================================================
 * NEW SUB-MODULE ACCESS (Phase 1-2)
 *===========================================================================*/

/**
 * @brief Get glomerular layer handle
 *
 * @param adapter Adapter instance
 * @return Glomerular layer, or NULL if not enabled
 */
glomerular_layer_t* cerebellum_get_glomerular_layer(cerebellum_adapter_t* adapter);

/**
 * @brief Get molecular layer interneurons handle
 *
 * @param adapter Adapter instance
 * @return Molecular interneurons, or NULL if not enabled
 */
molecular_layer_interneurons_t* cerebellum_get_molecular_interneurons(
    cerebellum_adapter_t* adapter);

/**
 * @brief Get Golgi cell network handle
 *
 * @param adapter Adapter instance
 * @return Golgi network, or NULL if not enabled
 */
golgi_cell_network_t* cerebellum_get_golgi_network(cerebellum_adapter_t* adapter);

/*=============================================================================
 * SYNAPTIC DYNAMICS API (Phase 1)
 *===========================================================================*/

/**
 * @brief Get default synaptic dynamics configuration
 *
 * @return Default configuration with biologically-motivated values
 */
cerebellar_synapse_config_t cerebellar_synapse_default_config(void);

/**
 * @brief Initialize synaptic dynamics state
 *
 * @param state State to initialize
 * @param config Configuration (NULL for defaults)
 */
void cerebellar_synapse_init(cerebellar_synapse_state_t* state,
                              const cerebellar_synapse_config_t* config);

/**
 * @brief Update synaptic dynamics for time step
 *
 * @param state Synapse state
 * @param dt_ms Time step in milliseconds
 * @param presynaptic_activity Presynaptic firing rate [0, 1]
 */
void cerebellar_synapse_update(cerebellar_synapse_state_t* state,
                                float dt_ms,
                                float presynaptic_activity);

/**
 * @brief Get effective weight after STP modulation
 *
 * @param state Synapse state
 * @param base_weight Base synaptic weight
 * @return Modulated weight
 */
float cerebellar_synapse_get_effective_weight(
    const cerebellar_synapse_state_t* state,
    float base_weight);

/*=============================================================================
 * INTERNEURON QUERIES (Phase 2)
 *===========================================================================*/

/**
 * @brief Get basket cell activity for specific cell
 *
 * @param adapter Adapter instance
 * @param basket_id Basket cell ID
 * @param activation Output: activation level [0, 1]
 * @param firing_rate Output: firing rate (Hz)
 * @return true on success
 */
bool cerebellum_get_basket_activity(const cerebellum_adapter_t* adapter,
                                     uint32_t basket_id,
                                     float* activation,
                                     float* firing_rate);

/**
 * @brief Get total inhibition received by specific Purkinje cell
 *
 * @param adapter Adapter instance
 * @param purkinje_id Purkinje cell ID
 * @param somatic_inhibition Output: inhibition from basket cells
 * @param dendritic_inhibition Output: inhibition from stellate cells
 * @return true on success
 */
bool cerebellum_get_purkinje_inhibition(const cerebellum_adapter_t* adapter,
                                         uint32_t purkinje_id,
                                         float* somatic_inhibition,
                                         float* dendritic_inhibition);

/**
 * @brief Get Golgi feedback inhibition statistics
 *
 * @param adapter Adapter instance
 * @param total_feedback Output: total feedback inhibition
 * @param avg_granule_inhibition Output: average per-granule inhibition
 * @return true on success
 */
bool cerebellum_get_golgi_feedback(const cerebellum_adapter_t* adapter,
                                    float* total_feedback,
                                    float* avg_granule_inhibition);

/*=============================================================================
 * VESTIBULOCEREBELLUM API (Phase 3)
 *===========================================================================*/

/* Forward declaration for vestibular signal type */
typedef struct vestibular_mossy_signal vestibular_mossy_signal_t;

/**
 * @brief Process vestibular mossy fiber input
 *
 * Routes vestibular signals to flocculus and nodulus.
 *
 * @param adapter Cerebellum adapter
 * @param input Vestibular mossy fiber signal
 * @return true on success
 */
bool cerebellum_process_vestibular_input(cerebellum_adapter_t* adapter,
                                          const vestibular_mossy_signal_t* input);

/**
 * @brief Get VOR output from flocculus
 *
 * @param adapter Cerebellum adapter
 * @param vor_output Output: VOR command (yaw, pitch, roll)
 * @return true on success
 */
bool cerebellum_get_vor_output(cerebellum_adapter_t* adapter,
                                float vor_output[3]);

/**
 * @brief Set metabolic modulation from substrate bridge
 *
 * @param adapter Adapter instance
 * @param motor_coordination Motor coordination scaling [0.3, 1.0]
 * @param timing_precision Timing precision scaling [0.3, 1.0]
 * @param learning_capacity Learning capacity scaling [0.3, 1.0]
 * @return true on success
 */
bool cerebellum_set_metabolic_modulation(cerebellum_adapter_t* adapter,
                                          float motor_coordination,
                                          float timing_precision,
                                          float learning_capacity);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 *
 * @param adapter Adapter instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t cerebellum_get_bio_context(cerebellum_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 *
 * @param adapter Adapter instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t cerebellum_process_bio_messages(cerebellum_adapter_t* adapter, uint32_t max_messages);

/**
 * @brief Request motor coordination asynchronously
 *
 * @param adapter Adapter instance
 * @param motor_command Motor command vector
 * @param num_dims Number of dimensions
 * @return Future for coordination result, or NULL on failure
 */
nimcp_bio_future_t cerebellum_request_coordination_async(
    cerebellum_adapter_t* adapter,
    const float* motor_command,
    uint32_t num_dims
);

/**
 * @brief Broadcast motor adaptation complete
 *
 * @param adapter Adapter instance
 * @param result Motor coordination result to broadcast
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cerebellum_broadcast_adaptation_complete(
    cerebellum_adapter_t* adapter,
    const motor_coordination_result_t* result
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CEREBELLUM_ADAPTER_H */
