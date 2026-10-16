//=============================================================================
// nimcp_hodgkin_huxley.h - Biophysically Realistic Neuron Model
//=============================================================================
// Implements the classic Hodgkin-Huxley model with extensions for:
// - Multiple ion channel types (Na+, K+, Ca2+, leak)
// - Temperature dependence (Q10 factors)
// - Bio-async messaging integration
// - Immune system monitoring for channelopathies
//=============================================================================

#ifndef NIMCP_HODGKIN_HUXLEY_H
#define NIMCP_HODGKIN_HUXLEY_H

#include <stdbool.h>
#include <stdint.h>
#include "utils/error/nimcp_error_codes.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

#ifndef NIMCP_BRAIN_T_DEFINED
#define NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#endif

typedef struct nimcp_bio_router_s nimcp_bio_router_t;

//=============================================================================
// Constants
//=============================================================================

/** Default Hodgkin-Huxley parameters (squid giant axon at 6.3C) */
#define NIMCP_HH_DEFAULT_G_NA      120.0f   /**< Sodium conductance (mS/cm^2) */
#define NIMCP_HH_DEFAULT_G_K       36.0f    /**< Potassium conductance (mS/cm^2) */
#define NIMCP_HH_DEFAULT_G_L       0.3f     /**< Leak conductance (mS/cm^2) */
#define NIMCP_HH_DEFAULT_E_NA      55.0f    /**< Sodium reversal potential (mV) */
#define NIMCP_HH_DEFAULT_E_K       (-77.0f) /**< Potassium reversal potential (mV) */
#define NIMCP_HH_DEFAULT_E_L       (-54.3f) /**< Leak reversal potential (mV) */
#define NIMCP_HH_DEFAULT_C_M       1.0f     /**< Membrane capacitance (uF/cm^2) */
#define NIMCP_HH_DEFAULT_V_REST    (-65.0f) /**< Resting potential (mV) */
#define NIMCP_HH_DEFAULT_V_THRESH  (-55.0f) /**< Spike threshold (mV) */
#define NIMCP_HH_TEMPERATURE_REF   6.3f     /**< Reference temperature (C) */
#define NIMCP_HH_Q10_RATE          3.0f     /**< Temperature coefficient */

/** Spike detection */
#define NIMCP_HH_SPIKE_THRESHOLD   0.0f     /**< Voltage threshold for spike (mV) */
#define NIMCP_HH_SPIKE_REFRACTORY  2.0f     /**< Refractory period (ms) */

/** Numerical limits */
#define NIMCP_HH_MAX_DT            0.1f     /**< Maximum timestep (ms) */
#define NIMCP_HH_MIN_DT            0.001f   /**< Minimum timestep (ms) */

//=============================================================================
// Ion Channel Types
//=============================================================================

/**
 * @brief Ion channel types for HH model
 */
typedef enum {
    NIMCP_ION_CHANNEL_NA,        /**< Sodium (fast inactivating) */
    NIMCP_ION_CHANNEL_K,         /**< Potassium (delayed rectifier) */
    NIMCP_ION_CHANNEL_LEAK,      /**< Leak (passive) */
    NIMCP_ION_CHANNEL_CA_L,      /**< L-type calcium (high threshold) */
    NIMCP_ION_CHANNEL_CA_T,      /**< T-type calcium (low threshold) */
    NIMCP_ION_CHANNEL_K_CA,      /**< Calcium-activated potassium */
    NIMCP_ION_CHANNEL_K_A,       /**< A-type potassium (transient) */
    NIMCP_ION_CHANNEL_H,         /**< Hyperpolarization-activated (Ih) */
    NIMCP_ION_CHANNEL_COUNT
} nimcp_ion_channel_type_t;

//=============================================================================
// Gating Variable Structure
//=============================================================================

/**
 * @brief Gating variable state
 *
 * BIOLOGICAL: Represents the fraction of ion channel gates in open state
 * Models voltage-dependent conformational changes in channel proteins
 */
typedef struct {
    float value;          /**< Current gate state [0, 1] */
    float alpha;          /**< Opening rate (1/ms) */
    float beta;           /**< Closing rate (1/ms) */
    float tau;            /**< Time constant (ms) */
    float inf;            /**< Steady-state value */
} nimcp_gate_t;

//=============================================================================
// Ion Channel Structure
//=============================================================================

/**
 * @brief Ion channel state
 *
 * BIOLOGICAL: Models voltage-gated ion channels with multiple gates
 */
typedef struct {
    nimcp_ion_channel_type_t type;  /**< Channel type */
    float g_max;                     /**< Maximum conductance (mS/cm^2) */
    float E_rev;                     /**< Reversal potential (mV) */
    float g_current;                 /**< Current conductance (mS/cm^2) */
    float I_current;                 /**< Current through channel (uA/cm^2) */

    /* Gating variables */
    nimcp_gate_t activation;         /**< Activation gate (m for Na, n for K) */
    nimcp_gate_t inactivation;       /**< Inactivation gate (h for Na) */

    /* Power coefficients */
    uint8_t activation_power;        /**< e.g., m^3 for Na */
    uint8_t inactivation_power;      /**< e.g., h^1 for Na */

    /* Channel modulation */
    float modulation_factor;         /**< Neuromodulatory scaling [0, 2] */
    bool enabled;                    /**< Channel active flag */
} nimcp_ion_channel_t;

//=============================================================================
// HH Neuron Configuration
//=============================================================================

/**
 * @brief Configuration for HH neuron
 */
typedef struct {
    /* Conductances (mS/cm^2) */
    float g_Na;          /**< Sodium conductance */
    float g_K;           /**< Potassium conductance */
    float g_L;           /**< Leak conductance */
    float g_Ca_L;        /**< L-type calcium conductance */
    float g_Ca_T;        /**< T-type calcium conductance */
    float g_K_Ca;        /**< Ca-activated K conductance */
    float g_K_A;         /**< A-type K conductance */
    float g_H;           /**< H-current conductance */

    /* Reversal potentials (mV) */
    float E_Na;          /**< Sodium reversal */
    float E_K;           /**< Potassium reversal */
    float E_L;           /**< Leak reversal */
    float E_Ca;          /**< Calcium reversal */
    float E_H;           /**< H-current reversal */

    /* Membrane properties */
    float C_m;           /**< Membrane capacitance (uF/cm^2) */
    float V_rest;        /**< Resting potential (mV) */

    /* Temperature */
    float temperature;   /**< Temperature (Celsius) */

    /* Morphology (for multi-compartment) */
    float surface_area;  /**< Membrane surface area (cm^2) */
    float length;        /**< Compartment length (um) */
    float diameter;      /**< Compartment diameter (um) */

    /* Extended channels */
    bool enable_calcium;     /**< Enable Ca channels */
    bool enable_adaptation;  /**< Enable adaptation currents */
    bool enable_h_current;   /**< Enable Ih current */

    /* Integration */
    float dt_max;        /**< Maximum timestep (ms) */
    bool adaptive_dt;    /**< Use adaptive timestep */
} nimcp_hh_config_t;

//=============================================================================
// HH Neuron State
//=============================================================================

/**
 * @brief Hodgkin-Huxley neuron state
 *
 * BIOLOGICAL: Complete biophysical neuron model with:
 * - Voltage-gated ion channels
 * - Calcium dynamics
 * - Temperature dependence
 * - Synaptic integration
 */
typedef struct {
    /* Membrane potential */
    float V;             /**< Membrane voltage (mV) */
    float V_prev;        /**< Previous voltage for spike detection */

    /* Ion channels */
    nimcp_ion_channel_t channels[NIMCP_ION_CHANNEL_COUNT];

    /* Calcium dynamics */
    float Ca_i;          /**< Intracellular calcium (uM) */
    float Ca_o;          /**< Extracellular calcium (mM) */
    float Ca_decay_tau;  /**< Calcium decay time constant (ms) */

    /* Synaptic currents */
    float I_syn_exc;     /**< Excitatory synaptic current (uA/cm^2) */
    float I_syn_inh;     /**< Inhibitory synaptic current (uA/cm^2) */
    float I_ext;         /**< External current injection (uA/cm^2) */

    /* Spike state */
    bool spiked;             /**< Spike occurred this step */
    float last_spike_time;   /**< Time of last spike (ms) */
    float refractory_remaining; /**< Refractory time remaining (ms) */
    uint32_t spike_count;    /**< Total spikes since init */

    /* Time tracking */
    float time;          /**< Current simulation time (ms) */
    float dt;            /**< Current timestep (ms) */

    /* Temperature */
    float temperature;   /**< Current temperature (C) */
    float phi;           /**< Temperature factor */

    /* Morphology */
    float surface_area;  /**< Membrane area (cm^2) */

    /* Configuration */
    nimcp_hh_config_t config;

    /* Statistics */
    float avg_firing_rate;   /**< Running average firing rate (Hz) */
    float membrane_time_constant; /**< Effective tau_m (ms) */
    float input_resistance;  /**< Effective R_in (MOhm) */

    /* Module ID for bio-async */
    uint32_t module_id;

    /* Flags */
    bool initialized;
} nimcp_hh_neuron_t;

//=============================================================================
// HH Population (for efficient simulation of many neurons)
//=============================================================================

/* Forward declaration for thread pool */
struct nimcp_thread_pool;

/**
 * @brief Population of HH neurons for efficient simulation
 */
typedef struct {
    nimcp_hh_neuron_t* neurons;  /**< Array of neurons */
    uint32_t count;              /**< Number of neurons */
    uint32_t capacity;           /**< Allocated capacity */

    /* Population statistics */
    float mean_voltage;          /**< Mean membrane voltage */
    float mean_firing_rate;      /**< Population firing rate (Hz) */
    float synchrony;             /**< Population synchrony [0, 1] */

    /* Shared configuration */
    nimcp_hh_config_t default_config;

    /* Thread pool for parallel updates (optional) */
    struct nimcp_thread_pool* thread_pool;  /**< Persistent thread pool */
    uint32_t pool_threads;                   /**< Number of threads in pool */

    /* Module ID for bio-async */
    uint32_t module_id;

    bool initialized;
} nimcp_hh_population_t;

//=============================================================================
// Core API - Configuration
//=============================================================================

/**
 * @brief Initialize HH configuration with defaults
 *
 * @param config Configuration to initialize
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_config_default(nimcp_hh_config_t* config);

/**
 * @brief Initialize HH configuration for specific neuron type
 *
 * @param config Configuration to initialize
 * @param type Neuron type (e.g., "pyramidal", "interneuron", "purkinje")
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_config_for_type(
    nimcp_hh_config_t* config,
    const char* type
);

//=============================================================================
// Core API - Neuron Lifecycle
//=============================================================================

/**
 * @brief Create and initialize HH neuron
 *
 * @param neuron Neuron to initialize
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_neuron_init(
    nimcp_hh_neuron_t* neuron,
    const nimcp_hh_config_t* config
);

/**
 * @brief Create HH neuron with brain integration
 *
 * @param neuron Neuron to initialize
 * @param brain Parent brain for bio-async/immune integration
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_neuron_create(
    nimcp_hh_neuron_t* neuron,
    brain_t brain,
    const nimcp_hh_config_t* config
);

/**
 * @brief Destroy HH neuron
 *
 * @param neuron Neuron to destroy
 */
NIMCP_EXPORT void nimcp_hh_neuron_destroy(nimcp_hh_neuron_t* neuron);

/**
 * @brief Reset neuron to initial state
 *
 * @param neuron Neuron to reset
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_neuron_reset(nimcp_hh_neuron_t* neuron);

//=============================================================================
// Core API - Simulation
//=============================================================================

/**
 * @brief Update neuron state for one timestep
 *
 * WHAT: Integrates HH equations using 4th-order Runge-Kutta
 * WHY:  Accurate simulation of action potential dynamics
 * HOW:  Computes gating variable updates and membrane voltage
 *
 * @param neuron Neuron to update
 * @param I_ext External current injection (uA/cm^2)
 * @param dt Timestep (ms), 0 for adaptive
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_neuron_update(
    nimcp_hh_neuron_t* neuron,
    float I_ext,
    float dt
);

/**
 * @brief Check if neuron spiked
 *
 * @param neuron Neuron to check
 * @param spiked Output: true if spike occurred
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_neuron_get_spike(
    const nimcp_hh_neuron_t* neuron,
    bool* spiked
);

/**
 * @brief Get membrane voltage
 *
 * @param neuron Neuron to query
 * @return Membrane voltage (mV)
 */
NIMCP_EXPORT float nimcp_hh_neuron_get_voltage(const nimcp_hh_neuron_t* neuron);

/**
 * @brief Inject synaptic current
 *
 * @param neuron Target neuron
 * @param I_exc Excitatory current (uA/cm^2)
 * @param I_inh Inhibitory current (uA/cm^2)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_neuron_inject_synaptic(
    nimcp_hh_neuron_t* neuron,
    float I_exc,
    float I_inh
);

//=============================================================================
// Core API - Ion Channels
//=============================================================================

/**
 * @brief Get ion channel state
 *
 * @param neuron Neuron to query
 * @param type Channel type
 * @param channel Output: channel state
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_get_channel(
    const nimcp_hh_neuron_t* neuron,
    nimcp_ion_channel_type_t type,
    nimcp_ion_channel_t* channel
);

/**
 * @brief Modulate ion channel conductance
 *
 * BIOLOGICAL: Models neuromodulatory effects on channel density/function
 *
 * @param neuron Target neuron
 * @param type Channel type to modulate
 * @param factor Modulation factor [0, 2] (1 = normal)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_modulate_channel(
    nimcp_hh_neuron_t* neuron,
    nimcp_ion_channel_type_t type,
    float factor
);

/**
 * @brief Enable/disable ion channel
 *
 * @param neuron Target neuron
 * @param type Channel type
 * @param enable Enable flag
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_set_channel_enabled(
    nimcp_hh_neuron_t* neuron,
    nimcp_ion_channel_type_t type,
    bool enable
);

//=============================================================================
// Core API - Temperature
//=============================================================================

/**
 * @brief Set neuron temperature
 *
 * BIOLOGICAL: Temperature affects all rate constants via Q10 factors
 *
 * @param neuron Target neuron
 * @param temperature Temperature in Celsius
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_set_temperature(
    nimcp_hh_neuron_t* neuron,
    float temperature
);

/**
 * @brief Get temperature scaling factor (phi)
 *
 * @param neuron Neuron to query
 * @return Temperature factor
 */
NIMCP_EXPORT float nimcp_hh_get_phi(const nimcp_hh_neuron_t* neuron);

//=============================================================================
// Core API - Population
//=============================================================================

/**
 * @brief Create HH population
 *
 * @param population Population to initialize
 * @param count Number of neurons
 * @param config Shared configuration (NULL for defaults)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_population_create(
    nimcp_hh_population_t* population,
    uint32_t count,
    const nimcp_hh_config_t* config
);

/**
 * @brief Destroy HH population
 *
 * @param population Population to destroy
 */
NIMCP_EXPORT void nimcp_hh_population_destroy(nimcp_hh_population_t* population);

/**
 * @brief Update entire population
 *
 * @param population Population to update
 * @param I_ext Array of external currents (one per neuron)
 * @param dt Timestep (ms)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_population_update(
    nimcp_hh_population_t* population,
    const float* I_ext,
    float dt
);

/**
 * @brief Get population firing rate
 *
 * @param population Population to query
 * @param rate Output: firing rate (Hz)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_population_get_rate(
    const nimcp_hh_population_t* population,
    float* rate
);

/**
 * @brief Get population synchrony
 *
 * @param population Population to query
 * @param synchrony Output: synchrony measure [0, 1]
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_population_get_synchrony(
    const nimcp_hh_population_t* population,
    float* synchrony
);

//=============================================================================
// Core API - Parallel Population (Thread Pool)
//=============================================================================

/**
 * @brief Update population in parallel using thread pool
 *
 * WHAT: Updates all neurons in parallel using multiple threads
 * WHY:  Significant speedup for large populations (2-4x on multi-core)
 * HOW:  Divides neurons into chunks, submits to thread pool, waits
 *
 * @param population Population to update
 * @param I_ext Array of external currents (one per neuron)
 * @param dt Timestep (ms)
 * @param num_threads Number of worker threads (0 for auto-detect)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_population_update_parallel(
    nimcp_hh_population_t* population,
    const float* I_ext,
    float dt,
    uint32_t num_threads
);

/**
 * @brief Create population with persistent thread pool
 *
 * WHAT: Creates population with its own thread pool for repeated parallel updates
 * WHY:  Avoids thread pool creation/destruction overhead for simulation loops
 * HOW:  Allocates thread pool during create, reuses across updates
 *
 * @param population Population to initialize
 * @param count Number of neurons
 * @param config Shared configuration (NULL for defaults)
 * @param num_threads Number of threads for pool (0 for auto-detect)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_population_create_parallel(
    nimcp_hh_population_t* population,
    uint32_t count,
    const nimcp_hh_config_t* config,
    uint32_t num_threads
);

//=============================================================================
// Gating Variable Utilities
//=============================================================================

/**
 * @brief Compute sodium activation (m) steady-state
 */
NIMCP_EXPORT float nimcp_hh_m_inf(float V);

/**
 * @brief Compute sodium activation (m) time constant
 */
NIMCP_EXPORT float nimcp_hh_m_tau(float V);

/**
 * @brief Compute sodium inactivation (h) steady-state
 */
NIMCP_EXPORT float nimcp_hh_h_inf(float V);

/**
 * @brief Compute sodium inactivation (h) time constant
 */
NIMCP_EXPORT float nimcp_hh_h_tau(float V);

/**
 * @brief Compute potassium activation (n) steady-state
 */
NIMCP_EXPORT float nimcp_hh_n_inf(float V);

/**
 * @brief Compute potassium activation (n) time constant
 */
NIMCP_EXPORT float nimcp_hh_n_tau(float V);

//=============================================================================
// Analysis Utilities
//=============================================================================

/**
 * @brief Compute f-I curve (firing rate vs input current)
 *
 * @param neuron Neuron to analyze
 * @param I_min Minimum current (uA/cm^2)
 * @param I_max Maximum current (uA/cm^2)
 * @param num_points Number of points
 * @param currents Output: current values
 * @param rates Output: firing rates (Hz)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_compute_fi_curve(
    nimcp_hh_neuron_t* neuron,
    float I_min,
    float I_max,
    uint32_t num_points,
    float* currents,
    float* rates
);

/**
 * @brief Compute rheobase (minimum current for spiking)
 *
 * @param neuron Neuron to analyze
 * @param rheobase Output: rheobase current (uA/cm^2)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_compute_rheobase(
    nimcp_hh_neuron_t* neuron,
    float* rheobase
);

/**
 * @brief Get neuron statistics
 *
 * @param neuron Neuron to query
 * @param firing_rate Output: average firing rate (Hz)
 * @param tau_m Output: membrane time constant (ms)
 * @param R_in Output: input resistance (MOhm)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_hh_get_stats(
    const nimcp_hh_neuron_t* neuron,
    float* firing_rate,
    float* tau_m,
    float* R_in
);

//=============================================================================
// Wave W15: Runtime KG emission + read-path (aggregated, never per-neuron)
//
// HH action-potential dynamics run per-tick per-neuron — emitting a KG node
// every spike would trash the graph. Instead, the emit is aggregated by the
// caller (e.g. every N spikes, or on firing-mode transitions between
// quiescent / tonic / bursting).
//=============================================================================

struct brain_kg;

/**
 * @brief Register brain handle for HH runtime KG emission.
 */
NIMCP_EXPORT void nimcp_hh_kg_register_brain(brain_t brain);

/**
 * @brief Emit an aggregated HH firing-mode event.
 *
 * Rate-limit: emit only on mode-transition (quiescent↔tonic↔bursting) or
 * every N>=100 spikes. Never call per-spike.
 *
 * @param brain       Brain handle (falls back to the registered one if NULL)
 * @param kind        "mode_tonic", "mode_bursting", "mode_quiescent",
 *                    "rate_spike", "rheobase_shift"
 * @param firing_rate Current aggregate firing rate (Hz)
 * @param spike_count Number of spikes since last emit (rate-limiter check)
 * @param ts_us       Timestamp in microseconds
 */
NIMCP_EXPORT void nimcp_hh_kg_trigger_mode_event(
    brain_t brain,
    const char* kind,
    float firing_rate,
    uint32_t spike_count,
    uint64_t ts_us
);

/**
 * @brief Read-path: count HH mode events matching a kind substring.
 */
NIMCP_EXPORT uint32_t nimcp_hh_kg_count_events(
    struct brain_kg* kg,
    const char* kind_substr
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HODGKIN_HUXLEY_H */
