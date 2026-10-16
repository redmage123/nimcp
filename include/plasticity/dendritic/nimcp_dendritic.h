/**
 * @file nimcp_dendritic.h
 * @brief Dendritic Nonlinearities - NMDA Dynamics and Dendritic Computation
 *
 * WHAT: Nonlinear signal processing in dendritic compartments
 * WHY:  Dendrites are computational units, not passive cables
 *
 * BIOLOGICAL BASIS:
 * - NMDA Receptors: Voltage-dependent Mg²⁺ block creates nonlinearity
 * - Dendritic Spikes: Local regenerative events (Ca²⁺, Na⁺, NMDA spikes)
 * - Compartmentalization: Different branches compute independently
 * - Coincidence Detection: NMDA requires pre + post activity
 *
 * MATHEMATICAL FORMULATION:
 *
 * 1. NMDA Voltage Dependence (Jahr & Stevens 1990):
 *    g_NMDA(V) = g_max × B(V) × s
 *    B(V) = 1 / (1 + [Mg²⁺]/3.57 × exp(-0.062 × V))
 *
 * 2. NMDA Kinetics:
 *    ds/dt = α × glutamate × (1-s) - β × s
 *    τ_rise ≈ 2ms, τ_decay ≈ 100ms
 *
 * 3. Dendritic Integration:
 *    V_dend = Σ w_i × g_i × (E_i - V_dend)
 *    With supralinear summation threshold
 *
 * 4. Dendritic Spike Generation:
 *    If V_dend > θ_dend: V_dend += spike_amplitude
 *
 * DESIGN PATTERNS:
 * - Strategy Pattern: Different receptor types
 * - Composite Pattern: Dendritic tree structure
 * - Observer Pattern: Notify on dendritic spikes
 *
 * PERFORMANCE:
 * - O(1) per synapse for NMDA update
 * - O(s) per compartment for integration (s = synapses)
 * - O(b) for tree propagation (b = branches)
 *
 * @author NIMCP Development Team
 * @date 2025-11-27
 */

#ifndef NIMCP_DENDRITIC_H
#define NIMCP_DENDRITIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define DENDRITIC_EPSILON 1e-8f           /**< Numerical stability */
#define DENDRITIC_MAX_BRANCHES 32         /**< Maximum branches per neuron */
#define DENDRITIC_MAX_COMPARTMENTS 128    /**< Maximum compartments per branch */

/* NMDA Constants (Jahr & Stevens 1990) */
#define NMDA_MG_CONCENTRATION 1.0f        /**< [Mg²⁺] in mM (extracellular) */
#define NMDA_MG_SENSITIVITY 3.57f         /**< Mg²⁺ binding sensitivity (mM) */
#define NMDA_VOLTAGE_SLOPE 0.062f         /**< Voltage slope factor (mV⁻¹) */

/* Reversal Potentials */
#define E_NMDA 0.0f                       /**< NMDA reversal potential (mV) */
#define E_AMPA 0.0f                       /**< AMPA reversal potential (mV) */
#define E_GABA_A -70.0f                   /**< GABA-A reversal potential (mV) */
#define E_LEAK -70.0f                     /**< Leak reversal potential (mV) */

//=============================================================================
// Receptor Types
//=============================================================================

/**
 * @brief Dendritic receptor type enumeration
 *
 * WHAT: Types of synaptic receptors with different kinetics
 * WHY:  Different computational properties
 *
 * NOTE: Prefixed with DENDRITIC_ to avoid conflicts with nimcp_neuromodulators.h
 */
typedef enum {
    DENDRITIC_RECEPTOR_AMPA,    /**< Fast excitatory (τ ≈ 2ms) */
    DENDRITIC_RECEPTOR_NMDA,    /**< Slow excitatory with voltage dependence (τ ≈ 100ms) */
    DENDRITIC_RECEPTOR_GABA_A,  /**< Fast inhibitory (τ ≈ 10ms) */
    DENDRITIC_RECEPTOR_GABA_B,  /**< Slow inhibitory (τ ≈ 200ms) */
    DENDRITIC_RECEPTOR_COUNT
} dendritic_receptor_type_t;

//=============================================================================
// NMDA Receptor Structures
//=============================================================================

/**
 * @brief NMDA receptor parameters
 *
 * WHAT: Configuration for NMDA receptor kinetics
 * WHY:  Different brain regions have different NMDA subunits
 *
 * BIOLOGICAL: Based on NR2A/NR2B subunit composition
 */
typedef struct {
    float g_max;                 /**< Maximum conductance (nS) */
    float tau_rise;              /**< Rise time constant (ms) */
    float tau_decay;             /**< Decay time constant (ms) */
    float mg_concentration;      /**< Extracellular [Mg²⁺] (mM) */
    float mg_sensitivity;        /**< Mg²⁺ binding sensitivity */
    float voltage_slope;         /**< Voltage slope factor */
    float ca_permeability;       /**< Relative Ca²⁺ permeability */
} nmda_params_t;

/**
 * @brief Dendritic NMDA receptor state
 *
 * WHAT: Per-synapse NMDA receptor state for dendritic computation
 * WHY:  Track gating variable and conductance
 *
 * NOTE: Prefixed with dendritic_ to avoid conflicts with synapse_types.h
 *
 * MEMORY: 20 bytes per synapse
 */
typedef struct {
    float s;                     /**< Gating variable [0,1] */
    float s_rise;                /**< Rising component */
    float conductance;           /**< Current conductance (nS) */
    float calcium_influx;        /**< Ca²⁺ influx rate */
    bool active;                 /**< True if glutamate bound */
} dendritic_nmda_state_t;

//=============================================================================
// Dendritic Compartment Structures
//=============================================================================

/**
 * @brief Dendritic compartment type
 *
 * WHAT: Type of dendritic compartment
 * WHY:  Different compartments have different properties
 */
typedef enum {
    COMPARTMENT_SOMA,            /**< Cell body */
    COMPARTMENT_PROXIMAL,        /**< Proximal dendrite (near soma) */
    COMPARTMENT_DISTAL,          /**< Distal dendrite (far from soma) */
    COMPARTMENT_APICAL_TRUNK,    /**< Main apical dendrite */
    COMPARTMENT_APICAL_TUFT,     /**< Apical tuft (distal apical) */
    COMPARTMENT_BASAL            /**< Basal dendrite */
} compartment_type_t;

/**
 * @brief Dendritic compartment parameters
 *
 * WHAT: Electrical and morphological properties
 * WHY:  Different compartments integrate differently
 */
typedef struct {
    compartment_type_t type;
    float length;                /**< Physical length (μm) */
    float diameter;              /**< Diameter (μm) */
    float membrane_capacitance;  /**< Cm (pF/μm²) */
    float axial_resistance;      /**< Ra (Ω·cm) */
    float leak_conductance;      /**< g_leak (nS) */
    float spike_threshold;       /**< θ for dendritic spike (mV) */
    float spike_amplitude;       /**< Amplitude of dendritic spike (mV) */
    float spike_duration;        /**< Duration of dendritic spike (ms) */
    float supralinearity_factor; /**< Factor for supralinear summation */
} compartment_params_t;

/**
 * @brief Dendritic compartment state
 *
 * WHAT: Dynamic state of a compartment
 * WHY:  Track voltage and currents
 *
 * MEMORY: 40 bytes per compartment
 */
typedef struct {
    float voltage;               /**< Membrane voltage (mV) */
    float voltage_prev;          /**< Previous voltage for spike detection */
    float total_excitatory;      /**< Total excitatory input */
    float total_inhibitory;      /**< Total inhibitory input */
    float calcium_concentration; /**< Local [Ca²⁺] (μM) */
    bool spike_active;           /**< True if dendritic spike in progress */
    float spike_time_remaining;  /**< Remaining spike duration (ms) */
    uint32_t spike_count;        /**< Total dendritic spikes generated */
} compartment_state_t;

//=============================================================================
// Dendritic Branch and Tree Structures
//=============================================================================

/**
 * @brief Dendritic branch (sequence of compartments)
 *
 * WHAT: Single dendritic branch with linear compartments
 * WHY:  Hierarchical structure of dendritic tree
 */
typedef struct dendritic_branch_struct {
    uint32_t id;
    uint32_t parent_id;          /**< ID of parent branch (0 = soma) */
    uint32_t num_compartments;
    compartment_params_t* params;
    compartment_state_t* states;
    float coupling_resistance;   /**< Axial coupling to parent */
} dendritic_branch_t;

/**
 * @brief Complete dendritic tree
 *
 * WHAT: Full dendritic tree with all branches
 * WHY:  Represent entire dendritic arbor
 */
typedef struct dendritic_tree_struct* dendritic_tree_t;

/**
 * @brief Dendritic tree configuration
 *
 * WHAT: Configuration for dendritic tree creation
 * WHY:  Specify morphology and properties
 */
typedef struct {
    uint32_t num_branches;
    uint32_t compartments_per_branch;
    compartment_type_t default_type;
    float default_spike_threshold;
    float default_supralinearity;
    bool enable_nmda;
    bool enable_dendritic_spikes;
    bool enable_calcium_dynamics;
} dendritic_tree_config_t;

/**
 * @brief Dendritic tree statistics
 *
 * WHAT: Monitoring metrics for dendritic computation
 * WHY:  Track activity and plasticity
 */
typedef struct {
    uint64_t total_updates;
    uint32_t dendritic_spikes;
    uint32_t nmda_activations;
    float mean_voltage;
    float max_calcium;
    float supralinear_events;
} dendritic_tree_stats_t;

//=============================================================================
// Factory Functions
//=============================================================================

/**
 * @brief Create default NMDA parameters
 *
 * WHAT: Factory method for NMDA receptor parameters
 * WHY:  Provides biologically plausible defaults
 *
 * BIOLOGICAL: Based on Jahr & Stevens 1990
 *
 * @return NMDA parameters
 */
nmda_params_t nmda_params_default(void);

/**
 * @brief Create NMDA parameters for NR2A subunit
 *
 * WHAT: Fast NMDA parameters (mature synapses)
 * WHY:  NR2A has faster kinetics than NR2B
 *
 * @return NR2A NMDA parameters
 */
nmda_params_t nmda_params_nr2a(void);

/**
 * @brief Create NMDA parameters for NR2B subunit
 *
 * WHAT: Slow NMDA parameters (developmental/plastic synapses)
 * WHY:  NR2B has slower kinetics, more Ca²⁺ influx
 *
 * @return NR2B NMDA parameters
 */
nmda_params_t nmda_params_nr2b(void);

/**
 * @brief Create default compartment parameters
 *
 * WHAT: Factory method for compartment parameters
 * WHY:  Standard compartment properties
 *
 * @param type Compartment type
 * @return Compartment parameters
 */
compartment_params_t compartment_params_default(compartment_type_t type);

/**
 * @brief Create default dendritic tree configuration
 *
 * WHAT: Factory method for tree configuration
 * WHY:  Standard tree with reasonable defaults
 *
 * @return Tree configuration
 */
dendritic_tree_config_t dendritic_tree_config_default(void);

//=============================================================================
// NMDA Receptor Functions
//=============================================================================

/**
 * @brief Initialize NMDA receptor state
 *
 * WHAT: Factory method for NMDA state
 * WHY:  Ensure valid initial state
 *
 * @return Initialized NMDA state
 */
dendritic_nmda_state_t nmda_state_init(void);

/**
 * @brief Compute NMDA voltage-dependent block
 *
 * WHAT: Calculate Mg²⁺ block factor B(V)
 * WHY:  NMDA conductance is voltage-dependent
 *
 * FORMULA: B(V) = 1 / (1 + [Mg²⁺]/3.57 × exp(-0.062 × V))
 *
 * COMPLEXITY: O(1)
 *
 * @param voltage Membrane voltage (mV)
 * @param params NMDA parameters
 * @return Block factor [0,1] (0 = fully blocked, 1 = unblocked)
 */
float nmda_compute_block(float voltage, const nmda_params_t* params);

/**
 * @brief Update NMDA receptor kinetics
 *
 * WHAT: Update gating variable based on glutamate
 * WHY:  NMDA has slow kinetics with dual-exponential profile
 *
 * FORMULA: ds/dt = (1-s)/τ_rise × glutamate - s/τ_decay
 *
 * COMPLEXITY: O(1)
 *
 * @param state NMDA state to update
 * @param glutamate Glutamate concentration [0,1]
 * @param dt Time step (ms)
 * @param params NMDA parameters
 */
void nmda_update_kinetics(dendritic_nmda_state_t* state,
                          float glutamate,
                          float dt,
                          const nmda_params_t* params);

/**
 * @brief Compute NMDA conductance and current
 *
 * WHAT: Calculate effective NMDA conductance
 * WHY:  Combine kinetics and voltage dependence
 *
 * FORMULA: g_NMDA = g_max × B(V) × s
 *          I_NMDA = g_NMDA × (V - E_NMDA)
 *
 * COMPLEXITY: O(1)
 *
 * @param state NMDA state
 * @param voltage Membrane voltage (mV)
 * @param params NMDA parameters
 * @return NMDA current (nA)
 */
float nmda_compute_current(const dendritic_nmda_state_t* state,
                           float voltage,
                           const nmda_params_t* params);

/**
 * @brief Compute NMDA calcium influx
 *
 * WHAT: Calculate Ca²⁺ entry through NMDA receptor
 * WHY:  Ca²⁺ is crucial for plasticity signaling
 *
 * FORMULA: Ca_influx ∝ g_NMDA × (V - E_Ca) × f_Ca
 *
 * @param state NMDA state
 * @param voltage Membrane voltage (mV)
 * @param params NMDA parameters
 * @return Calcium influx rate (μM/ms)
 */
float nmda_compute_calcium_influx(const dendritic_nmda_state_t* state,
                                  float voltage,
                                  const nmda_params_t* params);

//=============================================================================
// Dendritic Compartment Functions
//=============================================================================

/**
 * @brief Initialize compartment state
 *
 * WHAT: Factory method for compartment state
 * WHY:  Ensure valid initial state
 *
 * @param resting_voltage Resting membrane potential (mV)
 * @return Initialized compartment state
 */
compartment_state_t compartment_state_init(float resting_voltage);

/**
 * @brief Integrate synaptic inputs in compartment
 *
 * WHAT: Compute compartment voltage from synaptic inputs
 * WHY:  Core dendritic integration
 *
 * FORMULA: With NMDA: Check for supralinear summation
 *          V_dend = Σ g_i × (E_i - V) / g_leak
 *
 * COMPLEXITY: O(1) for single compartment
 *
 * @param state Compartment state
 * @param excitatory_input Total excitatory conductance
 * @param inhibitory_input Total inhibitory conductance
 * @param params Compartment parameters
 * @param dt Time step (ms)
 */
void compartment_integrate(compartment_state_t* state,
                           float excitatory_input,
                           float inhibitory_input,
                           const compartment_params_t* params,
                           float dt);

/**
 * @brief Check and generate dendritic spike
 *
 * WHAT: Detect threshold crossing and generate spike
 * WHY:  Dendritic spikes are local regenerative events
 *
 * @param state Compartment state
 * @param params Compartment parameters
 * @return true if dendritic spike was generated
 */
bool compartment_check_spike(compartment_state_t* state,
                             const compartment_params_t* params);

/**
 * @brief Compute supralinear summation factor
 *
 * WHAT: Calculate boost from clustered inputs
 * WHY:  NMDA creates supralinear summation for coincident inputs
 *
 * FORMULA: If inputs > threshold: factor = base + supralinearity × (inputs - threshold)
 *
 * @param total_input Sum of inputs
 * @param threshold Supralinearity threshold
 * @param supralinearity_factor Amount of supralinearity
 * @return Multiplicative factor (≥1.0)
 */
float compartment_supralinear_factor(float total_input,
                                     float threshold,
                                     float supralinearity_factor);

//=============================================================================
// Dendritic Tree Functions
//=============================================================================

/**
 * @brief Create dendritic tree
 *
 * WHAT: Factory method for dendritic tree
 * WHY:  Create complete dendritic arbor
 *
 * @param config Tree configuration
 * @return Tree handle or NULL on failure
 */
dendritic_tree_t dendritic_tree_create(const dendritic_tree_config_t* config);

/**
 * @brief Destroy dendritic tree
 *
 * WHAT: Free tree resources
 * WHY:  Prevent memory leaks
 *
 * @param tree Tree to destroy
 */
void dendritic_tree_destroy(dendritic_tree_t tree);

/**
 * @brief Inject synaptic input to compartment
 *
 * WHAT: Add synaptic input to specific compartment
 * WHY:  Synapses target specific dendritic locations
 *
 * @param tree Dendritic tree
 * @param branch_id Branch ID
 * @param compartment_id Compartment ID within branch
 * @param excitatory Excitatory conductance
 * @param inhibitory Inhibitory conductance
 * @param nmda_glutamate NMDA glutamate level [0,1]
 */
void dendritic_tree_inject_input(dendritic_tree_t tree,
                                 uint32_t branch_id,
                                 uint32_t compartment_id,
                                 float excitatory,
                                 float inhibitory,
                                 float nmda_glutamate);

/**
 * @brief Update dendritic tree for one timestep
 *
 * WHAT: Propagate voltages and update all compartments
 * WHY:  Compute dendritic integration
 *
 * @param tree Dendritic tree
 * @param dt Time step (ms)
 */
void dendritic_tree_update(dendritic_tree_t tree, float dt);

/**
 * @brief Get somatic voltage (output)
 *
 * WHAT: Read voltage at soma
 * WHY:  Soma voltage determines spike generation
 *
 * @param tree Dendritic tree
 * @return Somatic voltage (mV)
 */
float dendritic_tree_get_soma_voltage(dendritic_tree_t tree);

/**
 * @brief Get total calcium in tree
 *
 * WHAT: Sum calcium across all compartments
 * WHY:  Calcium level affects plasticity
 *
 * @param tree Dendritic tree
 * @return Total calcium concentration (μM)
 */
float dendritic_tree_get_total_calcium(dendritic_tree_t tree);

/**
 * @brief Get dendritic tree statistics
 *
 * WHAT: Retrieve monitoring metrics
 * WHY:  Track dendritic activity
 *
 * @param tree Dendritic tree
 * @param stats Output statistics
 * @return true on success
 */
bool dendritic_tree_get_stats(dendritic_tree_t tree,
                              dendritic_tree_stats_t* stats);

/**
 * @brief Reset dendritic tree to resting state
 *
 * WHAT: Reset all compartments to resting potential
 * WHY:  Clear activity for new stimulus
 *
 * @param tree Dendritic tree
 */
void dendritic_tree_reset(dendritic_tree_t tree);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_DENDRITIC_H
