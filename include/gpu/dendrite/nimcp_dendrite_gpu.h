/**
 * @file nimcp_dendrite_gpu.h
 * @brief GPU-Accelerated Dendrite Module using Tensor Operations
 *
 * WHAT: C API for GPU-accelerated dendritic computation
 * WHY:  GPU acceleration for massively parallel dendrite simulation
 * HOW:  Wraps CUDA kernels using nimcp_gpu_tensor_t for all data
 *
 * BIOLOGICAL BASIS:
 * =================
 * Dendrites perform sophisticated computations requiring parallel processing:
 * - Cable equation integration across thousands of segments
 * - Calcium dynamics in spines (2000-10000 per neuron)
 * - NMDA spike detection and propagation
 * - STDP weight updates based on spike timing
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Dendritic computation is embarrassingly parallel:
 * - Each segment integrates independently (cable equation)
 * - Each spine's calcium evolves independently
 * - NMDA detection is a parallel threshold operation
 * - STDP updates are independent per spine
 *
 * TENSOR LAYOUT:
 * ==============
 * - segment_voltages:  [N_dendrites, N_segments] - Membrane potential per segment
 * - calcium_levels:    [N_dendrites, N_spines]   - Spine calcium concentration
 * - spine_weights:     [N_dendrites, N_spines]   - Synaptic weight per spine
 * - nmda_states:       [N_dendrites, N_segments] - NMDA activation state
 * - bap_signals:       [N_dendrites, N_segments] - Backpropagating AP amplitude
 * - cable_params:      [N_dendrites, 3]          - Rm, Cm, Ra per dendrite
 *
 * USAGE:
 * ======
 * @code
 * nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(0);
 * dendrite_gpu_config_t config = dendrite_gpu_default_config();
 * dendrite_gpu_context_t* dend_gpu = dendrite_gpu_create(ctx, &config);
 *
 * // Upload dendrite data to GPU
 * dendrite_gpu_upload_segments(dend_gpu, segments, num_dendrites, num_segments);
 * dendrite_gpu_upload_spines(dend_gpu, spines, num_dendrites, num_spines);
 *
 * // Run simulation step
 * dendrite_gpu_integrate(dend_gpu, dt_ms);
 * dendrite_gpu_update_calcium(dend_gpu, dt_ms);
 * dendrite_gpu_detect_nmda_spikes(dend_gpu);
 * dendrite_gpu_apply_stdp(dend_gpu, pre_times, post_times, num_events);
 *
 * // Download results
 * dendrite_gpu_download_voltages(dend_gpu, host_voltages);
 *
 * dendrite_gpu_destroy(dend_gpu);
 * nimcp_gpu_context_destroy(ctx);
 * @endcode
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_DENDRITE_GPU_H
#define NIMCP_DENDRITE_GPU_H

// Include GPU context BEFORE extern "C" block - it brings in CUDA headers
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Forward Declarations
//=============================================================================

/** @brief Opaque GPU Dendrite context */
typedef struct dendrite_gpu_context dendrite_gpu_context_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief GPU Dendrite configuration
 */
typedef struct {
    uint32_t max_dendrites;           /**< Maximum number of dendrites */
    uint32_t max_segments_per_dendrite; /**< Max segments per dendrite */
    uint32_t max_spines_per_dendrite; /**< Max spines per dendrite */
    float default_rm;                 /**< Default membrane resistance (Ohm*cm^2) */
    float default_cm;                 /**< Default membrane capacitance (uF/cm^2) */
    float default_ra;                 /**< Default axial resistance (Ohm*cm) */
    float calcium_decay_tau_ms;       /**< Calcium decay time constant (ms) */
    float nmda_threshold_mv;          /**< NMDA spike threshold (mV) */
    float bap_velocity_um_ms;         /**< bAP propagation velocity (um/ms) */
    float bap_attenuation_per_um;     /**< bAP attenuation per micrometer */
    float stdp_tau_plus_ms;           /**< STDP LTP time constant (ms) */
    float stdp_tau_minus_ms;          /**< STDP LTD time constant (ms) */
    float stdp_a_plus;                /**< STDP LTP amplitude */
    float stdp_a_minus;               /**< STDP LTD amplitude */
    bool enable_async_transfer;       /**< Enable async CPU-GPU transfers */
} dendrite_gpu_config_t;

/**
 * @brief Get default GPU Dendrite configuration
 */
NIMCP_EXPORT dendrite_gpu_config_t dendrite_gpu_default_config(void);

//=============================================================================
// GPU Segment Data Structure (for upload)
//=============================================================================

/**
 * @brief GPU-optimized segment data (padded for coalesced access)
 */
typedef struct {
    uint32_t segment_id;              /**< Segment identifier */
    uint32_t dendrite_id;             /**< Parent dendrite ID */
    uint32_t parent_segment;          /**< Parent segment index */
    float length;                     /**< Segment length (um) */
    float diameter;                   /**< Segment diameter (um) */
    float path_distance;              /**< Distance from soma (um) */
    float rm;                         /**< Membrane resistance */
    float cm;                         /**< Membrane capacitance */
    float ra;                         /**< Axial resistance */
    float voltage;                    /**< Current voltage (mV) */
    float calcium;                    /**< Current calcium (uM) */
    uint32_t has_active_properties;   /**< Has active conductances */
    float _padding;                   /**< Padding for alignment */
} dendrite_gpu_segment_t;

/**
 * @brief GPU-optimized spine data
 */
typedef struct {
    uint32_t spine_id;                /**< Spine identifier */
    uint32_t dendrite_id;             /**< Parent dendrite ID */
    uint32_t segment_id;              /**< Attached segment */
    float neck_resistance;            /**< Neck electrical resistance (MOhm) */
    float head_capacitance;           /**< Head capacitance (fF) */
    float voltage;                    /**< Spine head voltage (mV) */
    float calcium;                    /**< Spine calcium (uM) */
    float synaptic_weight;            /**< Synaptic weight (0-1) */
    float ampa_conductance;           /**< AMPA conductance (nS) */
    float nmda_conductance;           /**< NMDA conductance (nS) */
    float pre_trace;                  /**< STDP presynaptic trace */
    float post_trace;                 /**< STDP postsynaptic trace */
    uint64_t last_pre_spike;          /**< Last presynaptic spike time */
    uint64_t last_post_spike;         /**< Last postsynaptic spike time */
} dendrite_gpu_spine_t;

/**
 * @brief STDP event for batch processing
 */
typedef struct {
    uint32_t spine_id;                /**< Target spine */
    uint32_t dendrite_id;             /**< Parent dendrite */
    uint64_t pre_time;                /**< Presynaptic spike time (us) */
    uint64_t post_time;               /**< Postsynaptic spike time (us) */
    float current;                    /**< Input current (pA) */
} dendrite_gpu_stdp_event_t;

//=============================================================================
// Context Structure (Tensor-based)
//=============================================================================

/**
 * @brief GPU Dendrite context with tensor-based storage
 *
 * All data stored as GPU tensors for efficient parallel operations
 */
struct dendrite_gpu_context {
    nimcp_gpu_context_t* gpu_ctx;     /**< GPU context reference */
    dendrite_gpu_config_t config;     /**< Configuration */

    // Tensor storage - main state
    nimcp_gpu_tensor_t* segment_voltages;  /**< [N_dendrites, N_segments] */
    nimcp_gpu_tensor_t* calcium_levels;    /**< [N_dendrites, N_spines] */
    nimcp_gpu_tensor_t* spine_weights;     /**< [N_dendrites, N_spines] */
    nimcp_gpu_tensor_t* nmda_states;       /**< [N_dendrites, N_segments] */
    nimcp_gpu_tensor_t* bap_signals;       /**< [N_dendrites, N_segments] */
    nimcp_gpu_tensor_t* cable_params;      /**< [N_dendrites, 3] - Rm, Cm, Ra */

    // Tensor storage - segment properties
    nimcp_gpu_tensor_t* segment_lengths;   /**< [N_dendrites, N_segments] */
    nimcp_gpu_tensor_t* segment_diameters; /**< [N_dendrites, N_segments] */
    nimcp_gpu_tensor_t* segment_distances; /**< [N_dendrites, N_segments] - path distance */
    nimcp_gpu_tensor_t* segment_parents;   /**< [N_dendrites, N_segments] - parent indices */
    nimcp_gpu_tensor_t* segment_active;    /**< [N_dendrites, N_segments] - has active props */

    // Tensor storage - spine properties
    nimcp_gpu_tensor_t* spine_segments;    /**< [N_dendrites, N_spines] - attached segment */
    nimcp_gpu_tensor_t* spine_resistances; /**< [N_dendrites, N_spines] - neck resistance */
    nimcp_gpu_tensor_t* spine_capacitances;/**< [N_dendrites, N_spines] - head capacitance */

    // STDP traces
    nimcp_gpu_tensor_t* pre_traces;        /**< [N_dendrites, N_spines] */
    nimcp_gpu_tensor_t* post_traces;       /**< [N_dendrites, N_spines] */

    // Axial currents for cable equation
    nimcp_gpu_tensor_t* axial_currents;    /**< [N_dendrites, N_segments] */

    // Temporary buffers
    nimcp_gpu_tensor_t* temp_buffer_1;     /**< General purpose temp buffer */
    nimcp_gpu_tensor_t* temp_buffer_2;     /**< General purpose temp buffer */

    // Dimensions
    uint32_t num_dendrites;               /**< Current number of dendrites */
    uint32_t num_segments;                /**< Segments per dendrite */
    uint32_t num_spines;                  /**< Spines per dendrite */

    // CUDA stream
#ifdef NIMCP_ENABLE_CUDA
    void* stream;                         /**< CUDA stream (cudaStream_t) */
#else
    void* stream;
#endif

    // Statistics
    uint64_t integrate_calls;             /**< Number of integrate calls */
    uint64_t calcium_updates;             /**< Number of calcium updates */
    uint64_t nmda_detections;             /**< Number of NMDA spike detections */
    uint64_t stdp_updates;                /**< Number of STDP updates */
    float avg_integrate_time_us;          /**< Average integrate time */
    float avg_calcium_time_us;            /**< Average calcium update time */
    size_t gpu_memory_used;               /**< GPU memory in use (bytes) */
};

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create GPU Dendrite context
 *
 * WHAT: Initialize GPU resources for dendrite acceleration
 * WHY:  Allocate GPU tensors for parallel dendritic computation
 * HOW:  Create CUDA streams, allocate device tensors
 *
 * @param gpu_ctx GPU context (required)
 * @param config Configuration (NULL for defaults)
 * @return New GPU Dendrite context, or NULL on failure
 */
NIMCP_EXPORT dendrite_gpu_context_t* dendrite_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const dendrite_gpu_config_t* config
);

/**
 * @brief Destroy GPU Dendrite context
 *
 * @param ctx GPU Dendrite context to destroy
 */
NIMCP_EXPORT void dendrite_gpu_destroy(dendrite_gpu_context_t* ctx);

/**
 * @brief Synchronize GPU operations
 *
 * @param ctx GPU Dendrite context
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_synchronize(dendrite_gpu_context_t* ctx);

//=============================================================================
// Data Upload Functions
//=============================================================================

/**
 * @brief Upload segment data to GPU tensors
 *
 * WHAT: Transfer segment properties from CPU to GPU
 * WHY:  Enable GPU-accelerated cable equation integration
 * HOW:  Populate voltage, cable params, and connectivity tensors
 *
 * @param ctx GPU Dendrite context
 * @param segments Array of segment data
 * @param num_dendrites Number of dendrites
 * @param segments_per_dendrite Segments per dendrite
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_upload_segments(
    dendrite_gpu_context_t* ctx,
    const dendrite_gpu_segment_t* segments,
    uint32_t num_dendrites,
    uint32_t segments_per_dendrite
);

/**
 * @brief Upload spine data to GPU tensors
 *
 * WHAT: Transfer spine properties from CPU to GPU
 * WHY:  Enable GPU-accelerated calcium dynamics and STDP
 * HOW:  Populate calcium, weight, and STDP trace tensors
 *
 * @param ctx GPU Dendrite context
 * @param spines Array of spine data
 * @param num_dendrites Number of dendrites
 * @param spines_per_dendrite Spines per dendrite
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_upload_spines(
    dendrite_gpu_context_t* ctx,
    const dendrite_gpu_spine_t* spines,
    uint32_t num_dendrites,
    uint32_t spines_per_dendrite
);

/**
 * @brief Upload cable parameters
 *
 * @param ctx GPU Dendrite context
 * @param rm Membrane resistance array [num_dendrites]
 * @param cm Membrane capacitance array [num_dendrites]
 * @param ra Axial resistance array [num_dendrites]
 * @param num_dendrites Number of dendrites
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_upload_cable_params(
    dendrite_gpu_context_t* ctx,
    const float* rm,
    const float* cm,
    const float* ra,
    uint32_t num_dendrites
);

//=============================================================================
// Download Functions
//=============================================================================

/**
 * @brief Download segment voltages to host
 *
 * @param ctx GPU Dendrite context
 * @param voltages Output array [num_dendrites * num_segments]
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_download_voltages(
    dendrite_gpu_context_t* ctx,
    float* voltages
);

/**
 * @brief Download calcium levels to host
 *
 * @param ctx GPU Dendrite context
 * @param calcium Output array [num_dendrites * num_spines]
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_download_calcium(
    dendrite_gpu_context_t* ctx,
    float* calcium
);

/**
 * @brief Download spine weights to host
 *
 * @param ctx GPU Dendrite context
 * @param weights Output array [num_dendrites * num_spines]
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_download_weights(
    dendrite_gpu_context_t* ctx,
    float* weights
);

/**
 * @brief Download NMDA states to host
 *
 * @param ctx GPU Dendrite context
 * @param nmda_states Output array [num_dendrites * num_segments]
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_download_nmda_states(
    dendrite_gpu_context_t* ctx,
    float* nmda_states
);

//=============================================================================
// Core Computation Functions
//=============================================================================

/**
 * @brief Integrate cable equation for all segments (GPU kernel)
 *
 * WHAT: Solve cable equation for all dendrite segments in parallel
 * WHY:  Main update of membrane potential based on axial currents
 * HOW:  Each thread computes one segment: dV/dt = (I_syn - V/R_m + I_axial) / C_m
 *
 * @param ctx GPU Dendrite context
 * @param dt_ms Time step in milliseconds
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_integrate(
    dendrite_gpu_context_t* ctx,
    float dt_ms
);

/**
 * @brief Update calcium dynamics for all spines (GPU kernel)
 *
 * WHAT: Update calcium concentration in all spines in parallel
 * WHY:  Calcium drives plasticity and active properties
 * HOW:  Each thread: dCa/dt = -Ca/tau + influx from NMDA/bAP
 *
 * @param ctx GPU Dendrite context
 * @param dt_ms Time step in milliseconds
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_update_calcium(
    dendrite_gpu_context_t* ctx,
    float dt_ms
);

/**
 * @brief Detect NMDA spikes in all segments (GPU kernel)
 *
 * WHAT: Check for NMDA spike threshold crossing in parallel
 * WHY:  NMDA spikes enable nonlinear dendritic computation
 * HOW:  Each thread checks voltage > threshold with Mg2+ block relief
 *
 * @param ctx GPU Dendrite context
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_detect_nmda_spikes(
    dendrite_gpu_context_t* ctx
);

/**
 * @brief Apply STDP weight updates (GPU kernel)
 *
 * WHAT: Update synaptic weights based on spike timing
 * WHY:  STDP is primary mechanism for Hebbian learning
 * HOW:  Each thread computes dW from pre/post traces using tensor ops
 *
 * @param ctx GPU Dendrite context
 * @param events Array of STDP events
 * @param num_events Number of events
 * @param timestamp Current simulation time (us)
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_apply_stdp(
    dendrite_gpu_context_t* ctx,
    const dendrite_gpu_stdp_event_t* events,
    uint32_t num_events,
    uint64_t timestamp
);

/**
 * @brief Propagate backpropagating action potential (GPU kernel)
 *
 * WHAT: Propagate bAP signal through dendrite tree
 * WHY:  bAP provides coincidence signal for STDP
 * HOW:  Each thread updates bAP amplitude based on distance and attenuation
 *
 * @param ctx GPU Dendrite context
 * @param amplitude_mv Initial AP amplitude at soma
 * @param dt_ms Time step in milliseconds
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_propagate_bap(
    dendrite_gpu_context_t* ctx,
    float amplitude_mv,
    float dt_ms
);

/**
 * @brief Compute axial currents between segments (GPU kernel)
 *
 * WHAT: Calculate inter-segment currents using cable theory
 * WHY:  Required for realistic voltage propagation
 * HOW:  I_axial = g_axial * (V_neighbor - V_segment)
 *
 * @param ctx GPU Dendrite context
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_compute_axial_currents(
    dendrite_gpu_context_t* ctx
);

//=============================================================================
// Batch Input Processing
//=============================================================================

/**
 * @brief Inject synaptic currents into spines (GPU kernel)
 *
 * WHAT: Apply input currents to multiple spines in parallel
 * WHY:  Efficient batch processing of synaptic inputs
 * HOW:  Each thread updates one spine's conductance
 *
 * @param ctx GPU Dendrite context
 * @param spine_indices Array of target spine indices [num_inputs]
 * @param dendrite_indices Array of dendrite indices [num_inputs]
 * @param currents Array of input currents [num_inputs]
 * @param num_inputs Number of inputs
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_inject_currents(
    dendrite_gpu_context_t* ctx,
    const uint32_t* spine_indices,
    const uint32_t* dendrite_indices,
    const float* currents,
    uint32_t num_inputs
);

/**
 * @brief Decay STDP traces (GPU kernel)
 *
 * WHAT: Apply exponential decay to all STDP traces
 * WHY:  Traces decay between spikes
 * HOW:  trace *= exp(-dt/tau)
 *
 * @param ctx GPU Dendrite context
 * @param dt_ms Time step in milliseconds
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_decay_traces(
    dendrite_gpu_context_t* ctx,
    float dt_ms
);

//=============================================================================
// Full Simulation Step
//=============================================================================

/**
 * @brief Run complete dendrite simulation step (fused GPU operations)
 *
 * WHAT: Execute full dendrite update in one call
 * WHY:  Minimize kernel launch overhead
 * HOW:  Fused kernels: axial -> integrate -> calcium -> NMDA -> traces
 *
 * @param ctx GPU Dendrite context
 * @param dt_ms Time step in milliseconds
 * @param timestamp Current simulation time (us)
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_step(
    dendrite_gpu_context_t* ctx,
    float dt_ms,
    uint64_t timestamp
);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief GPU Dendrite statistics
 */
typedef struct {
    uint64_t integrate_calls;         /**< Total integrate calls */
    uint64_t calcium_updates;         /**< Total calcium updates */
    uint64_t nmda_detections;         /**< Total NMDA detections */
    uint64_t stdp_updates;            /**< Total STDP updates */
    float avg_integrate_time_us;      /**< Average integrate time */
    float avg_calcium_time_us;        /**< Average calcium update time */
    float avg_nmda_time_us;           /**< Average NMDA detection time */
    float avg_stdp_time_us;           /**< Average STDP update time */
    size_t gpu_memory_used;           /**< GPU memory in use (bytes) */
    uint32_t active_dendrites;        /**< Number of active dendrites */
    uint32_t active_segments;         /**< Number of active segments */
    uint32_t active_spines;           /**< Number of active spines */
    float mean_voltage;               /**< Mean segment voltage */
    float mean_calcium;               /**< Mean spine calcium */
} dendrite_gpu_stats_t;

/**
 * @brief Get GPU Dendrite statistics
 *
 * @param ctx GPU Dendrite context
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool dendrite_gpu_get_stats(
    const dendrite_gpu_context_t* ctx,
    dendrite_gpu_stats_t* stats
);

/**
 * @brief Reset GPU Dendrite statistics
 *
 * @param ctx GPU Dendrite context
 */
NIMCP_EXPORT void dendrite_gpu_reset_stats(dendrite_gpu_context_t* ctx);

//=============================================================================
// CPU Reference Implementations (for testing)
//=============================================================================

/**
 * @brief CPU reference implementation of cable equation integration
 */
NIMCP_EXPORT bool dendrite_cpu_integrate(
    float* voltages,
    const float* cable_params,
    const float* axial_currents,
    uint32_t num_dendrites,
    uint32_t num_segments,
    float dt_ms
);

/**
 * @brief CPU reference implementation of calcium dynamics
 */
NIMCP_EXPORT bool dendrite_cpu_update_calcium(
    float* calcium,
    const float* nmda_states,
    const float* bap_signals,
    uint32_t num_dendrites,
    uint32_t num_spines,
    float dt_ms,
    float decay_tau_ms
);

/**
 * @brief CPU reference implementation of NMDA spike detection
 */
NIMCP_EXPORT bool dendrite_cpu_detect_nmda_spikes(
    float* nmda_states,
    const float* voltages,
    uint32_t num_dendrites,
    uint32_t num_segments,
    float threshold_mv
);

/**
 * @brief CPU reference implementation of STDP
 */
NIMCP_EXPORT bool dendrite_cpu_apply_stdp(
    float* weights,
    float* pre_traces,
    float* post_traces,
    const dendrite_gpu_stdp_event_t* events,
    uint32_t num_events,
    float a_plus,
    float a_minus,
    float tau_plus_ms,
    float tau_minus_ms
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DENDRITE_GPU_H */
