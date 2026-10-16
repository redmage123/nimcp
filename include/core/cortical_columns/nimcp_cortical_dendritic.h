/**
 * @file nimcp_cortical_dendritic.h
 * @brief Dendritic computation in Layer V pyramidal cells
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Apical/basal dendritic integration with calcium spikes and burst coding
 * WHY:  Layer V pyramidal neurons perform coincidence detection between feedforward
 *       (basal) and feedback (apical) inputs, implementing predictive coding via
 *       Burst-Assisted Coding (BAC) firing when predictions match bottom-up input
 * HOW:  Multi-compartment model with separate basal, apical oblique, and apical tuft
 *       dendrites; NMDA-dependent calcium spikes in apical tuft; burst vs single
 *       spike output mode based on coincidence timing
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * LAYER V PYRAMIDAL CELL ARCHITECTURE:
 * -------------------------------------
 * Layer V thick-tufted pyramidal cells are the primary output neurons of cortex:
 * - Large soma with extensive dendritic tree
 * - Basal dendrites receive feedforward input from Layer IV (sensory/thalamic)
 * - Apical oblique dendrites receive local feedback from Layer II/III
 * - Apical tuft dendrites (in Layer I) receive long-range feedback from higher areas
 * - Axon projects to subcortical targets (brainstem, spinal cord, thalamus)
 *
 * DENDRITIC COMPARTMENTALIZATION:
 * --------------------------------
 * Three electrically semi-independent dendritic zones:
 *
 * 1. BASAL DENDRITES:
 *    - Located near soma in Layer V
 *    - Receive feedforward input from Layer IV
 *    - Drive single-spike firing when activated alone
 *    - ~150-250 μm from soma
 *
 * 2. APICAL OBLIQUE DENDRITES:
 *    - Proximal apical branches in Layer IV-V border
 *    - Receive local recurrent input from Layer II/III
 *    - Provide intermediate-level integration
 *    - ~200-400 μm from soma
 *
 * 3. APICAL TUFT DENDRITES:
 *    - Distal apical branches in Layer I
 *    - Receive long-range feedback from higher cortical areas
 *    - Generate NMDA-dependent calcium spikes
 *    - ~600-1000 μm from soma
 *
 * CALCIUM SPIKES (Ca²⁺ SPIKES):
 * ------------------------------
 * Apical tuft dendrites can generate calcium spikes independently of soma:
 * - Mediated by voltage-gated calcium channels (VGCC) and NMDA receptors
 * - Threshold: ~-35 mV (more depolarized than Na⁺ spike threshold)
 * - Duration: 10-50 ms (longer than Na⁺ spikes ~1-2 ms)
 * - Amplitude: 20-40 mV local dendritic depolarization
 * - Propagate toward soma, but attenuate with distance
 * - Can be triggered by coincident apical input + backpropagating action potential
 *
 * BURST-ASSISTED CODING (BAC) FIRING:
 * ------------------------------------
 * When basal and apical inputs coincide within ~20 ms window:
 * - Basal input triggers initial action potential (AP) at soma
 * - AP backpropagates into apical dendrite
 * - Backpropagating AP + apical input triggers Ca²⁺ spike in tuft
 * - Ca²⁺ spike propagates to soma
 * - Soma fires burst of 2-4 action potentials (burst mode)
 * - Interburst interval: 3-5 ms between spikes in burst
 *
 * FUNCTIONAL SIGNIFICANCE:
 * ------------------------
 * Single spike mode: Bottom-up sensory input alone
 * - Only basal dendrites active
 * - Regular single-spike firing
 * - Represents "unexpected" or "unpredicted" input
 *
 * Burst mode: Bottom-up input matches top-down prediction
 * - Basal AND apical inputs coincide
 * - Burst of 2-4 spikes
 * - Represents "confirmed prediction" or "salient event"
 * - Higher synaptic efficacy (bursts cause more EPSP in targets)
 * - Triggers plasticity in targets (burst = "teaching signal")
 *
 * Silent mode: Top-down input without bottom-up
 * - Only apical dendrites active
 * - Subthreshold modulation
 * - Primes neuron for subsequent input
 * - Does not drive output
 *
 * PREDICTIVE CODING IMPLEMENTATION:
 * ----------------------------------
 * This dendritic mechanism implements predictive coding:
 * - Apical input = prediction from higher areas
 * - Basal input = sensory evidence from lower areas
 * - Burst = prediction confirmation (prediction + evidence)
 * - Single spike = prediction error (evidence without prediction)
 * - Silent = prediction without evidence (preparatory state)
 *
 * KEY REFERENCES:
 * ---------------
 * - Larkum et al. (1999) "Dendritic mechanisms underlying the coupling of the
 *   dendritic with the axonal action potential initiation zone"
 * - Larkum (2013) "A cellular mechanism for cortical associations: an organizing
 *   principle for the cerebral cortex"
 * - Major et al. (2013) "Active properties of neocortical pyramidal neuron dendrites"
 * - Takahashi et al. (2016) "Active dendritic currents gate descending cortical
 *   outputs gating in perception"
 * - Sacramento et al. (2018) "Dendritic cortical microcircuits approximate the
 *   backpropagation algorithm"
 *
 * IMPLEMENTATION NOTES:
 * ---------------------
 * - Simplified compartmental model (3 dendrites + soma)
 * - Voltage dynamics use leaky integrator model
 * - Ca²⁺ spike uses threshold + duration model (not full Hodgkin-Huxley)
 * - NMDA nonlinearity implemented as voltage-dependent gain
 * - Burst detection uses spike timing analysis
 * - Compatible with cortical column layer structure
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_DENDRITIC_H
#define NIMCP_CORTICAL_DENDRITIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations and Handles
//=============================================================================

/**
 * WHAT: Opaque handle to dendritic computation system
 * WHY:  Encapsulation - hide internal implementation
 * HOW:  Forward declaration, defined in .c file
 */
typedef struct cortical_dendritic cortical_dendritic_t;

/**
 * WHAT: Opaque handle to bio-async context
 * WHY:  Enable inter-module messaging via bio-async router
 * HOW:  Context returned by bio_router_register_module()
 */
typedef void* bio_module_context_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * WHAT: Dendritic compartment types in Layer V pyramidal cell
 * WHY:  Distinguish functional zones receiving different input types
 * HOW:  Enum for three main dendritic compartments
 *
 * BIOLOGICAL MAPPING:
 * - BASAL: Feedforward input from Layer IV (thalamic/sensory)
 * - APICAL_OBLIQUE: Local recurrent from Layer II/III
 * - APICAL_TUFT: Long-range feedback from higher cortical areas (Layer I)
 */
typedef enum {
    DENDRITE_BASAL = 0,           /**< Basal dendrites (feedforward input) */
    DENDRITE_APICAL_OBLIQUE = 1,  /**< Proximal apical dendrites (local feedback) */
    DENDRITE_APICAL_TUFT = 2,     /**< Distal apical tuft (long-range feedback) */
    DENDRITE_COMPARTMENT_COUNT = 3 /**< Total number of compartments */
} dendrite_compartment_t;

/**
 * WHAT: Output firing modes of Layer V pyramidal cell
 * WHY:  Distinguish burst vs single-spike coding (information-rich output)
 * HOW:  Enum for three output states
 *
 * BIOLOGICAL SIGNIFICANCE:
 * - SINGLE_SPIKE: Bottom-up input only (prediction error)
 * - BURST: Bottom-up + top-down coincidence (prediction confirmation)
 * - SILENT: Subthreshold or top-down only (preparatory/modulatory)
 */
typedef enum {
    OUTPUT_SILENT = 0,       /**< Below threshold (no output spike) */
    OUTPUT_SINGLE_SPIKE = 1, /**< Regular firing (1 spike) */
    OUTPUT_BURST = 2         /**< Burst firing (2-4 spikes) */
} output_mode_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * WHAT: Configuration for dendritic computation
 * WHY:  Control dendritic integration parameters and thresholds
 * HOW:  Structure with biologically-inspired defaults
 *
 * PARAMETER GUIDANCE:
 * - basal_weight: Typically 0.6-0.8 (feedforward dominant)
 * - apical_weight: Typically 0.2-0.4 (modulatory role)
 * - calcium_threshold: -35 mV to -30 mV (typical Ca²⁺ spike threshold)
 * - calcium_duration_ms: 10-50 ms (longer than Na⁺ spikes)
 * - coincidence_window_ms: 10-30 ms (BAC firing window)
 * - burst_threshold: Determines when Ca²⁺ spike triggers burst
 * - nmda_voltage_threshold: -40 mV (NMDA Mg²⁺ block removal)
 *
 * INVARIANTS:
 * - 0.0 < basal_weight <= 1.0
 * - 0.0 <= apical_weight <= 1.0
 * - basal_weight + apical_weight should be ~1.0
 * - calcium_threshold > -70 mV (must be above resting)
 * - calcium_duration_ms > 0
 * - coincidence_window_ms > 0
 */
typedef struct {
    float basal_weight;              /**< Weight of basal input (feedforward) */
    float apical_weight;             /**< Weight of apical input (feedback) */
    float calcium_threshold;         /**< Threshold for Ca²⁺ spike (mV) */
    float calcium_duration_ms;       /**< Duration of Ca²⁺ spike (ms) */
    float coincidence_window_ms;     /**< Time window for BAC firing (ms) */
    float burst_threshold;           /**< Threshold for burst vs single spike */
    bool enable_nmda_nonlinearity;   /**< Enable NMDA voltage-dependent gain */
    float nmda_voltage_threshold;    /**< Voltage threshold for NMDA (mV) */
    float soma_threshold;            /**< Somatic spike threshold (mV) */
    float resting_potential;         /**< Resting membrane potential (mV) */
    uint32_t max_burst_spikes;       /**< Maximum spikes in burst (2-4) */
    float interburst_interval_ms;    /**< Interval between spikes in burst (ms) */
} dendritic_config_t;

/**
 * WHAT: Statistics from dendritic computation system
 * WHY:  Monitor dendritic integration patterns and output modes
 * HOW:  Counters and averages updated during simulation
 */
typedef struct {
    uint64_t total_updates;            /**< Total update cycles */
    uint64_t calcium_spikes_generated; /**< Total Ca²⁺ spikes */
    uint64_t burst_outputs;            /**< Total burst firings */
    uint64_t single_spike_outputs;     /**< Total single spikes */
    uint64_t silent_outputs;           /**< Total subthreshold events */
    uint64_t bac_coincidences;         /**< BAC firing events (burst from coincidence) */
    float mean_basal_activation;      /**< Average basal dendrite activation */
    float mean_apical_activation;      /**< Average apical dendrite activation */
    float mean_soma_voltage;           /**< Average somatic voltage */
    float burst_rate;                  /**< Fraction of outputs that are bursts */
    float bac_success_rate;            /**< Fraction of Ca²⁺ spikes triggering bursts */
} dendritic_stats_t;

//=============================================================================
// Core API - Lifecycle
//=============================================================================

/**
 * WHAT: Create dendritic computation system
 * WHY:  Initialize multi-compartment dendritic model for Layer V pyramidal cells
 * HOW:  Allocate structures, initialize compartments, apply configuration
 *
 * @param config Configuration (NULL for defaults)
 * @param num_cells Number of Layer V pyramidal cells to simulate
 * @return Handle to dendritic system or NULL on failure
 *
 * POSTCONDITIONS:
 * - Returns valid handle if num_cells > 0
 * - All compartments initialized to resting state
 * - Statistics zeroed
 * - Bio-async not connected (call cortical_dendritic_connect_bio_async)
 *
 * COMPLEXITY: O(num_cells)
 * THREAD SAFETY: Not thread-safe (call from single thread during init)
 *
 * EXAMPLE:
 * ```c
 * dendritic_config_t config;
 * cortical_dendritic_default_config(&config);
 * cortical_dendritic_t* dend = cortical_dendritic_create(&config, 1000);
 * if (!dend) {
 *     // Handle error
 * }
 * ```
 */
cortical_dendritic_t* cortical_dendritic_create(
    const dendritic_config_t* config,
    uint32_t num_cells
);

/**
 * WHAT: Destroy dendritic computation system
 * WHY:  Free all allocated resources
 * HOW:  Disconnect bio-async, free compartments, free structure
 *
 * @param dend Dendritic system handle (NULL-safe)
 *
 * PRECONDITIONS: None (NULL-safe)
 * POSTCONDITIONS: All memory freed, handle invalid
 *
 * COMPLEXITY: O(num_cells)
 * THREAD SAFETY: Not thread-safe (ensure no concurrent access)
 */
void cortical_dendritic_destroy(cortical_dendritic_t* dend);

/**
 * WHAT: Get default dendritic configuration
 * WHY:  Provide biologically realistic starting parameters
 * HOW:  Return preset configuration based on Larkum (2013) data
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 if config is NULL
 *
 * DEFAULT VALUES:
 * - basal_weight: 0.7 (feedforward dominant)
 * - apical_weight: 0.3 (modulatory)
 * - calcium_threshold: -35.0 mV
 * - calcium_duration_ms: 30.0 ms
 * - coincidence_window_ms: 20.0 ms
 * - burst_threshold: 0.8
 * - enable_nmda_nonlinearity: true
 * - nmda_voltage_threshold: -40.0 mV
 * - soma_threshold: -55.0 mV
 * - resting_potential: -70.0 mV
 * - max_burst_spikes: 3
 * - interburst_interval_ms: 4.0 ms
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (read-only)
 */
int cortical_dendritic_default_config(dendritic_config_t* config);

//=============================================================================
// Core API - Input Setting
//=============================================================================

/**
 * WHAT: Set input to basal dendrites (feedforward from Layer IV)
 * WHY:  Basal dendrites receive bottom-up sensory/thalamic input
 * HOW:  Store input values for each cell's basal compartment
 *
 * @param dend Dendritic system handle
 * @param cell_idx Index of pyramidal cell (0 to num_cells-1)
 * @param input Input current or conductance value
 * @return 0 on success, -1 on error
 *
 * PRECONDITIONS:
 * - dend != NULL
 * - cell_idx < num_cells
 *
 * BIOLOGICAL CONTEXT:
 * - Typically driven by Layer IV stellate cells
 * - Represents sensory evidence or thalamic input
 * - Strong input can drive single-spike output
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe with internal locking
 */
int cortical_dendritic_set_basal_input(
    cortical_dendritic_t* dend,
    uint32_t cell_idx,
    float input
);

/**
 * WHAT: Set input to apical tuft dendrites (feedback from Layer I)
 * WHY:  Apical tuft receives top-down predictions from higher cortical areas
 * HOW:  Store input values for apical tuft compartment
 *
 * @param dend Dendritic system handle
 * @param cell_idx Index of pyramidal cell
 * @param input Input current or conductance value
 * @return 0 on success, -1 on error
 *
 * PRECONDITIONS:
 * - dend != NULL
 * - cell_idx < num_cells
 *
 * BIOLOGICAL CONTEXT:
 * - Receives long-range projections from higher cortical areas
 * - Represents top-down prediction or attentional modulation
 * - Can trigger Ca²⁺ spikes when strong + coincident with basal
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe with internal locking
 */
int cortical_dendritic_set_apical_input(
    cortical_dendritic_t* dend,
    uint32_t cell_idx,
    float input
);

/**
 * WHAT: Set input to apical oblique dendrites (local feedback from L2/3)
 * WHY:  Apical oblique receives recurrent input from same cortical area
 * HOW:  Store input values for apical oblique compartment
 *
 * @param dend Dendritic system handle
 * @param cell_idx Index of pyramidal cell
 * @param input Input current or conductance value
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe with internal locking
 */
int cortical_dendritic_set_oblique_input(
    cortical_dendritic_t* dend,
    uint32_t cell_idx,
    float input
);

//=============================================================================
// Core API - Computation
//=============================================================================

/**
 * WHAT: Update all dendritic compartments and soma for one time step
 * WHY:  Compute dendritic integration, check for Ca²⁺ spikes and bursts
 * HOW:  For each cell: update voltages, check coincidence, determine output mode
 *
 * @param dend Dendritic system handle
 * @param dt Time step in milliseconds
 * @return 0 on success, -1 on error
 *
 * ALGORITHM:
 * 1. For each cell:
 *    a. Update basal compartment voltage (leaky integration)
 *    b. Update apical oblique voltage
 *    c. Update apical tuft voltage (with NMDA nonlinearity if enabled)
 *    d. Check if apical tuft crossed Ca²⁺ threshold → generate Ca²⁺ spike
 *    e. Propagate inputs to soma (weighted sum)
 *    f. Check coincidence timing (basal + apical within window)
 *    g. Determine output mode (BURST if coincidence, SINGLE if basal only, SILENT)
 *    h. Update soma voltage
 * 2. Update statistics
 *
 * PRECONDITIONS:
 * - dend != NULL
 * - dt > 0
 *
 * POSTCONDITIONS:
 * - All compartment voltages updated
 * - Ca²⁺ spikes tracked
 * - Output modes determined
 * - Statistics incremented
 *
 * COMPLEXITY: O(num_cells)
 * THREAD SAFETY: Thread-safe with internal locking
 */
int cortical_dendritic_update(cortical_dendritic_t* dend, float dt);

/**
 * WHAT: Get output mode for a specific cell
 * WHY:  Query whether cell is bursting, single-spiking, or silent
 * HOW:  Return last computed output mode
 *
 * @param dend Dendritic system handle
 * @param cell_idx Index of pyramidal cell
 * @param mode Output parameter for mode
 * @return 0 on success, -1 on error
 *
 * PRECONDITIONS:
 * - dend != NULL
 * - cell_idx < num_cells
 * - mode != NULL
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe with internal locking
 */
int cortical_dendritic_get_output_mode(
    const cortical_dendritic_t* dend,
    uint32_t cell_idx,
    output_mode_t* mode
);

/**
 * WHAT: Get current somatic voltage for a cell
 * WHY:  Query membrane potential for analysis or visualization
 * HOW:  Return current soma voltage
 *
 * @param dend Dendritic system handle
 * @param cell_idx Index of pyramidal cell
 * @param voltage Output parameter for voltage (mV)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe with internal locking
 */
int cortical_dendritic_get_soma_voltage(
    const cortical_dendritic_t* dend,
    uint32_t cell_idx,
    float* voltage
);

/**
 * WHAT: Check if cell is currently in calcium spike state
 * WHY:  Detect active Ca²⁺ spike for analysis or modulation
 * HOW:  Check if Ca²⁺ spike was triggered and still active
 *
 * @param dend Dendritic system handle
 * @param cell_idx Index of pyramidal cell
 * @param active Output parameter (true if Ca²⁺ spike active)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe with internal locking
 */
int cortical_dendritic_is_calcium_spike_active(
    const cortical_dendritic_t* dend,
    uint32_t cell_idx,
    bool* active
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * WHAT: Get statistics from dendritic computation
 * WHY:  Monitor system behavior and burst coding patterns
 * HOW:  Return copy of statistics structure
 *
 * @param dend Dendritic system handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe with internal locking
 */
int cortical_dendritic_get_stats(
    const cortical_dendritic_t* dend,
    dendritic_stats_t* stats
);

/**
 * WHAT: Reset statistics counters to zero
 * WHY:  Clear stats for new measurement epoch
 * HOW:  Zero all counters and averages
 *
 * @param dend Dendritic system handle
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe with internal locking
 */
int cortical_dendritic_reset_stats(cortical_dendritic_t* dend);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect dendritic system to bio-async router
 * WHY:  Enable inter-module messaging for dendritic signals
 * HOW:  Register with bio-async router as BIO_MODULE_CORTICAL_DENDRITIC
 *
 * @param dend Dendritic system handle
 * @return 0 on success, -1 on error, 1 if bio-async not available
 *
 * BIOLOGICAL ANALOGY:
 * - Dendritic signals influence other modules (e.g., plasticity, attention)
 * - Burst outputs are "teaching signals" for downstream plasticity
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe with internal locking
 */
int cortical_dendritic_connect_bio_async(cortical_dendritic_t* dend);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean up bio-async resources
 * HOW:  Unregister module
 *
 * @param dend Dendritic system handle
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe with internal locking
 */
int cortical_dendritic_disconnect_bio_async(cortical_dendritic_t* dend);

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query connection status
 * HOW:  Return bio_async_enabled flag
 *
 * @param dend Dendritic system handle
 * @return true if connected, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe with internal locking
 */
bool cortical_dendritic_is_bio_async_connected(
    const cortical_dendritic_t* dend
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CORTICAL_DENDRITIC_H
