//=============================================================================
// nimcp_myelin_math.h - Advanced Myelin Mathematical Models
//=============================================================================
/**
 * @file nimcp_myelin_math.h
 * @brief Advanced mathematical models for myelin biophysics
 *
 * WHAT: Comprehensive mathematical models for myelin sheath properties
 * WHY:  Enable biologically accurate myelination dynamics and conduction
 * HOW:  Implements cable theory, saltatory conduction, and plasticity kinetics
 *
 * MATHEMATICAL MODELS:
 * 1. Rushton G-ratio optimization with diameter dependence
 * 2. Cable theory (space constant λ, time constant τ)
 * 3. Saltatory conduction velocity model
 * 4. Hill-kinetics activity-dependent myelination
 * 5. Conduction block probability (temperature-dependent)
 * 6. Internode length optimization (power law)
 * 7. Metabolic efficiency calculation
 * 8. Stochastic variability model
 *
 * BIOLOGICAL BASIS:
 * - Rushton (1951): Optimal g-ratio theory
 * - Hursh (1939): Velocity-diameter relationship
 * - Hodgkin-Huxley (1952): Action potential dynamics
 * - Waxman & Bennett (1972): Saltatory conduction
 * - Fields (2015): Activity-dependent myelination
 *
 * @version 2.0.0
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#ifndef NIMCP_MYELIN_MATH_H
#define NIMCP_MYELIN_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "constants/nimcp_math_constants.h"

//=============================================================================
// Mathematical Constants
//=============================================================================

/** @name Biophysical Constants */
///@{
#define NIMCP_MYELIN_MATH_PI            NIMCP_PI_F
#define NIMCP_MYELIN_MATH_E             NIMCP_EULER_F
#define NIMCP_MYELIN_MATH_EPSILON       1e-9f
///@}

/** @name Cable Theory Constants */
///@{
#define NIMCP_CABLE_R_M_BASE            1000.0f     /**< Base membrane resistance (Ω·cm²) */
#define NIMCP_CABLE_R_A_CYTOPLASM       100.0f      /**< Axoplasmic resistivity (Ω·cm) */
#define NIMCP_CABLE_C_M_BASE            1.0f        /**< Base membrane capacitance (μF/cm²) */
#define NIMCP_CABLE_R_M_PER_LAMELLA     100.0f      /**< Resistance increase per lamella (Ω·cm²) */
#define NIMCP_CABLE_C_M_REDUCTION       0.9f        /**< Capacitance reduction factor per lamella */
///@}

/** @name Saltatory Conduction Constants */
///@{
#define NIMCP_SALTATORY_TAU_NODE_MS     0.03f       /**< Node delay (ms) - AP regeneration */
#define NIMCP_SALTATORY_V_PASSIVE       1000.0f     /**< Passive propagation (μm/ms) */
#define NIMCP_SALTATORY_NODE_LENGTH_UM  1.0f        /**< Node of Ranvier length (μm) */
#define NIMCP_SALTATORY_V_MIN_MS        0.5f        /**< Minimum velocity (m/s) */
#define NIMCP_SALTATORY_V_MAX_MS        150.0f      /**< Maximum velocity (m/s) */
///@}

/** @name G-ratio Optimization Constants */
///@{
#define NIMCP_G_RATIO_BASE              0.77f       /**< Large axon optimal g-ratio */
#define NIMCP_G_RATIO_ALPHA             0.08f       /**< Small axon correction factor */
#define NIMCP_G_RATIO_D_CRITICAL        0.5f        /**< Critical diameter (μm) */
#define NIMCP_G_RATIO_MIN               0.4f        /**< Minimum physiological g-ratio */
#define NIMCP_G_RATIO_MAX               0.95f       /**< Maximum physiological g-ratio */
///@}

/** @name Myelination Kinetics Constants */
///@{
#define NIMCP_MYELIN_K_MAX              2.0f        /**< Max myelination rate (lamellae/s) */
#define NIMCP_MYELIN_K_HALF             0.3f        /**< Half-max activity level */
#define NIMCP_MYELIN_HILL_N             2.5f        /**< Hill coefficient */
#define NIMCP_MYELIN_K_DEMYELIN         0.1f        /**< Demyelination rate constant */
#define NIMCP_MYELIN_K_DEMYELIN_HALF    0.1f        /**< Half-max for demyelination */
///@}

/** @name Conduction Block Constants */
///@{
#define NIMCP_BLOCK_I_CRITICAL          0.4f        /**< 50% block integrity threshold */
#define NIMCP_BLOCK_SIGMA               0.1f        /**< Transition steepness */
#define NIMCP_BLOCK_T_REF               37.0f       /**< Reference temperature (°C) */
#define NIMCP_BLOCK_T_SENSITIVITY       0.05f       /**< Temperature sensitivity */
///@}

/** @name Internode Optimization Constants */
///@{
#define NIMCP_INTERNODE_ALPHA           150.0f      /**< Scaling coefficient */
#define NIMCP_INTERNODE_BETA            0.9f        /**< Power exponent */
#define NIMCP_INTERNODE_MIN_UM          100.0f      /**< Minimum internode (μm) */
#define NIMCP_INTERNODE_MAX_UM          2000.0f     /**< Maximum internode (μm) */
///@}

/** @name Metabolic Constants */
///@{
#define NIMCP_METAB_C_M                 1e-2f       /**< Membrane capacitance (F/m²) */
#define NIMCP_METAB_V_AP                0.1f        /**< Action potential amplitude (V) */
#define NIMCP_METAB_ATP_ENERGY_J        5e-20f      /**< Energy per ATP (J) */
#define NIMCP_METAB_PARANODE_LEAK       0.9f        /**< Paranode efficiency factor */
///@}

/** @name Stochastic Model Constants */
///@{
#define NIMCP_STOCH_CV_LAMELLAE         0.12f       /**< CV for lamellae count */
#define NIMCP_STOCH_CV_G_RATIO          0.06f       /**< CV for g-ratio */
#define NIMCP_STOCH_CV_INTERNODE        0.25f       /**< CV for internode length */
#define NIMCP_STOCH_CV_VELOCITY         0.08f       /**< CV for conduction velocity */
///@}

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Cable theory parameters
 *
 * WHAT: Electrical properties of myelinated axon segment
 * WHY:  Enable accurate passive signal propagation modeling
 * HOW:  Derived from membrane and axoplasmic properties
 */
typedef struct nimcp_cable_params {
    float lambda_um;            /**< Space constant (μm) */
    float tau_ms;               /**< Time constant (ms) */
    float r_m;                  /**< Membrane resistance (Ω·cm²) */
    float r_a;                  /**< Axial resistance (Ω/cm) */
    float c_m;                  /**< Membrane capacitance (μF/cm²) */
    float attenuation_factor;   /**< Signal attenuation per λ */
} nimcp_cable_params_t;

/**
 * @brief Myelination kinetics parameters
 *
 * WHAT: Parameters controlling activity-dependent myelination
 * WHY:  Enable realistic plasticity dynamics
 * HOW:  Hill-function kinetics with saturation
 */
typedef struct nimcp_myelination_kinetics {
    float k_max;                /**< Maximum myelination rate (lamellae/s) */
    float k_half;               /**< Half-max activity level (0-1) */
    float hill_n;               /**< Hill coefficient (cooperativity) */
    float k_demyelin;           /**< Demyelination rate constant */
    float k_demyelin_half;      /**< Half-max for demyelination */
    float saturation_lamellae;  /**< Max lamellae for saturation term */
    float min_activity;         /**< Minimum activity for any myelination */
    float history_weight;       /**< Weight for activity history (EMA) */
} nimcp_myelination_kinetics_t;

/**
 * @brief Conduction block parameters
 *
 * WHAT: Parameters for conduction failure probability
 * WHY:  Model pathological conditions and temperature sensitivity
 * HOW:  Sigmoid probability with temperature modulation
 */
typedef struct nimcp_conduction_block_params {
    float i_critical;           /**< Critical integrity for 50% block */
    float sigma;                /**< Sigmoid steepness */
    float t_ref;                /**< Reference temperature (°C) */
    float t_sensitivity;        /**< Temperature sensitivity factor */
    float frequency_factor;     /**< High-frequency block sensitivity */
    float refractory_ms;        /**< Refractory period after block (ms) */
} nimcp_conduction_block_params_t;

/**
 * @brief Saltatory conduction result
 *
 * WHAT: Complete conduction velocity calculation result
 * WHY:  Provide all components for debugging and analysis
 * HOW:  Breakdown of velocity into constituent factors
 */
typedef struct nimcp_saltatory_result {
    float velocity_ms;          /**< Final conduction velocity (m/s) */
    float tau_node_ms;          /**< Node delay component (ms) */
    float tau_internode_ms;     /**< Internode delay component (ms) */
    float g_efficiency;         /**< G-ratio efficiency factor (0-1) */
    float compaction_factor;    /**< Compaction contribution (0-1) */
    float integrity_factor;     /**< Integrity contribution (0-1) */
    float lambda_um;            /**< Space constant used (μm) */
    bool is_blocked;            /**< True if conduction is blocked */
    float block_probability;    /**< Probability of block (0-1) */
} nimcp_saltatory_result_t;

/**
 * @brief Metabolic efficiency result
 *
 * WHAT: Energy efficiency metrics for myelinated conduction
 * WHY:  Quantify metabolic benefit of myelination
 * HOW:  Compare myelinated vs unmyelinated energy costs
 */
typedef struct nimcp_metabolic_efficiency {
    float energy_per_ap_pj;     /**< Energy per action potential (pJ) */
    float energy_unmyelin_pj;   /**< Unmyelinated energy cost (pJ) */
    float efficiency_ratio;     /**< Efficiency vs unmyelinated */
    float atp_per_ap;           /**< ATP molecules per AP */
    float atp_per_meter;        /**< ATP molecules per meter */
    float power_uw;             /**< Power consumption (μW) at 100 Hz */
} nimcp_metabolic_efficiency_t;

/**
 * @brief Stochastic random state
 *
 * WHAT: State for deterministic pseudo-random generation
 * WHY:  Reproducible stochastic simulations
 * HOW:  LCG with state preservation
 */
typedef struct nimcp_myelin_rng {
    uint64_t state;             /**< Current RNG state */
    uint64_t seed;              /**< Original seed for reset */
    uint32_t samples_generated; /**< Count for diagnostics */
} nimcp_myelin_rng_t;

/**
 * @brief Complete myelin biophysics state
 *
 * WHAT: Aggregated state for all mathematical models
 * WHY:  Single structure for comprehensive myelin modeling
 * HOW:  Contains all parameters and cached calculations
 */
typedef struct nimcp_myelin_biophysics {
    // === Configuration ===
    nimcp_myelination_kinetics_t kinetics;
    nimcp_conduction_block_params_t block_params;

    // === Cached calculations ===
    nimcp_cable_params_t cable;
    nimcp_saltatory_result_t conduction;
    nimcp_metabolic_efficiency_t metabolism;

    // === State ===
    float activity_ema;         /**< Exponential moving average of activity */
    float temperature_c;        /**< Current temperature (°C) */
    uint64_t last_update_us;    /**< Last update timestamp (μs) */

    // === Stochastic ===
    nimcp_myelin_rng_t rng;
    bool use_stochastic;        /**< Enable variability */

    // === Flags ===
    bool cable_valid;           /**< Cable params are current */
    bool conduction_valid;      /**< Conduction result is current */
    bool metabolism_valid;      /**< Metabolism result is current */
} nimcp_myelin_biophysics_t;

//=============================================================================
// Initialization Functions
//=============================================================================

/**
 * @brief Get default myelination kinetics
 *
 * WHAT: Returns biologically reasonable default kinetics
 * WHY:  Simplify initialization with validated defaults
 * HOW:  Pre-filled structure with literature values
 *
 * @return Default kinetics parameters
 */
nimcp_myelination_kinetics_t nimcp_myelin_kinetics_default(void);

/**
 * @brief Get default conduction block parameters
 *
 * WHAT: Returns default block probability parameters
 * WHY:  Simplify initialization
 * HOW:  Pre-filled with physiological values
 *
 * @return Default block parameters
 */
nimcp_conduction_block_params_t nimcp_myelin_block_params_default(void);

/**
 * @brief Initialize myelin biophysics state
 *
 * WHAT: Create and initialize biophysics state
 * WHY:  Central management of all math models
 * HOW:  Allocate and set defaults
 *
 * @param use_stochastic Enable stochastic variability
 * @param seed Random seed (0 for time-based)
 * @return Initialized biophysics state or NULL
 */
nimcp_myelin_biophysics_t* nimcp_myelin_biophysics_create(bool use_stochastic,
                                                           uint64_t seed);

/**
 * @brief Destroy biophysics state
 * @param bio State to destroy
 */
void nimcp_myelin_biophysics_destroy(nimcp_myelin_biophysics_t* bio);

/**
 * @brief Reset biophysics state
 *
 * Resets cached values and RNG to initial state
 *
 * @param bio State to reset
 */
void nimcp_myelin_biophysics_reset(nimcp_myelin_biophysics_t* bio);

//=============================================================================
// G-ratio Optimization Functions
//=============================================================================

/**
 * @brief Compute optimal g-ratio for axon diameter
 *
 * WHAT: Calculate diameter-dependent optimal g-ratio
 * WHY:  Smaller axons have different optimal myelination
 * HOW:  Rushton model with exponential diameter correction
 *
 * FORMULA:
 *   g_opt(d) = g_base + α × exp(-d / d_critical)
 *
 * BIOLOGICAL BASIS:
 * - Rushton (1951): Optimal g-ratio theory
 * - Chomiak & Bhm (2009): Diameter dependence
 *
 * COMPLEXITY: O(1)
 *
 * @param axon_diameter_um Axon diameter in micrometers
 * @return Optimal g-ratio (typically 0.6-0.85)
 */
float nimcp_myelin_optimal_g_ratio(float axon_diameter_um);

/**
 * @brief Compute g-ratio efficiency factor
 *
 * WHAT: Calculate efficiency based on deviation from optimal
 * WHY:  Suboptimal g-ratio reduces conduction efficiency
 * HOW:  Parabolic efficiency centered on optimal
 *
 * FORMULA:
 *   η = 1 - k × (g - g_opt)²
 *
 * @param g_ratio Current g-ratio
 * @param axon_diameter_um Axon diameter (for optimal calculation)
 * @return Efficiency factor (0.5-1.0)
 */
float nimcp_myelin_g_ratio_efficiency(float g_ratio, float axon_diameter_um);

/**
 * @brief Compute lamellae needed for target g-ratio
 *
 * WHAT: Inverse calculation from g-ratio to lamellae
 * WHY:  Determine myelination level for target efficiency
 * HOW:  Algebraic inversion of g-ratio formula
 *
 * @param axon_diameter_um Axon diameter (μm)
 * @param target_g_ratio Desired g-ratio
 * @param lamella_thickness_nm Single lamella thickness (nm)
 * @return Required number of lamellae
 */
uint32_t nimcp_myelin_lamellae_for_g_ratio(float axon_diameter_um,
                                            float target_g_ratio,
                                            float lamella_thickness_nm);

//=============================================================================
// Cable Theory Functions
//=============================================================================

/**
 * @brief Compute cable theory parameters
 *
 * WHAT: Calculate space and time constants for myelinated segment
 * WHY:  Required for accurate passive signal propagation
 * HOW:  Classic cable equation with myelin modifications
 *
 * FORMULAS:
 *   r_m = r_m_base + r_m_per_lamella × n_lamellae
 *   c_m = c_m_base × (c_m_reduction ^ n_lamellae)
 *   r_a = ρ_a / (π × (d/2)²)
 *   λ = sqrt(r_m × d / (4 × r_a))
 *   τ = r_m × c_m
 *
 * BIOLOGICAL BASIS:
 * - Hodgkin & Rushton (1946): Cable theory
 * - Jack, Noble & Tsien (1983): Myelin modifications
 *
 * COMPLEXITY: O(1)
 *
 * @param axon_diameter_um Axon inner diameter (μm)
 * @param num_lamellae Number of myelin wraps
 * @param params Output cable parameters
 */
void nimcp_myelin_compute_cable_params(float axon_diameter_um,
                                        uint32_t num_lamellae,
                                        nimcp_cable_params_t* params);

/**
 * @brief Compute space constant for segment
 *
 * WHAT: Calculate electrotonic length constant
 * WHY:  Determines signal attenuation along segment
 * HOW:  From membrane and axial resistances
 *
 * @param axon_diameter_um Axon diameter (μm)
 * @param num_lamellae Number of lamellae
 * @return Space constant λ (μm)
 */
float nimcp_myelin_space_constant(float axon_diameter_um, uint32_t num_lamellae);

/**
 * @brief Compute time constant for segment
 *
 * WHAT: Calculate membrane time constant
 * WHY:  Determines temporal filtering of signals
 * HOW:  Product of membrane resistance and capacitance
 *
 * @param num_lamellae Number of lamellae
 * @return Time constant τ (ms)
 */
float nimcp_myelin_time_constant(uint32_t num_lamellae);

/**
 * @brief Compute signal attenuation
 *
 * WHAT: Calculate voltage decay along segment
 * WHY:  Passive signals attenuate exponentially
 * HOW:  exp(-L/λ) decay formula
 *
 * @param length_um Segment length (μm)
 * @param lambda_um Space constant (μm)
 * @return Attenuation factor (0-1)
 */
float nimcp_myelin_attenuation(float length_um, float lambda_um);

//=============================================================================
// Saltatory Conduction Functions
//=============================================================================

/**
 * @brief Compute saltatory conduction velocity
 *
 * WHAT: Calculate action potential propagation velocity
 * WHY:  Primary metric for myelination effectiveness
 * HOW:  Combined node and internode delay model
 *
 * FORMULA:
 *   v = L_internode / (τ_node + τ_internode)
 *   τ_internode = L² / (λ² × v_passive)
 *   v_final = v × g_efficiency × compaction × integrity
 *
 * BIOLOGICAL BASIS:
 * - Waxman & Bennett (1972): Saltatory conduction theory
 * - Ritchie (1995): Node delay measurements
 * - Court et al. (2004): G-ratio effects
 *
 * COMPLEXITY: O(1)
 *
 * @param axon_diameter_um Axon diameter (μm)
 * @param internode_length_um Internode length (μm)
 * @param num_lamellae Number of myelin wraps
 * @param g_ratio Current g-ratio
 * @param compaction_score Compaction factor (0-1)
 * @param integrity Structural integrity (0-1)
 * @param result Output detailed result (can be NULL for just velocity)
 * @return Conduction velocity (m/s)
 */
float nimcp_myelin_saltatory_velocity(float axon_diameter_um,
                                       float internode_length_um,
                                       uint32_t num_lamellae,
                                       float g_ratio,
                                       float compaction_score,
                                       float integrity,
                                       nimcp_saltatory_result_t* result);

/**
 * @brief Compute propagation delay through segment
 *
 * WHAT: Calculate time for AP to traverse segment
 * WHY:  Required for timing-sensitive neural circuits
 * HOW:  Length divided by velocity plus node delay
 *
 * @param length_um Segment length (μm)
 * @param velocity_ms Conduction velocity (m/s)
 * @return Propagation delay (ms)
 */
float nimcp_myelin_propagation_delay(float length_um, float velocity_ms);

/**
 * @brief Compute velocity with full biophysics state
 *
 * WHAT: Comprehensive velocity calculation using all parameters
 * WHY:  Single function for complete segment analysis
 * HOW:  Updates biophysics state with cached results
 *
 * @param bio Biophysics state
 * @param axon_diameter_um Axon diameter (μm)
 * @param internode_length_um Internode length (μm)
 * @param num_lamellae Number of lamellae
 * @param g_ratio Current g-ratio
 * @param compaction_score Compaction factor (0-1)
 * @param integrity Structural integrity (0-1)
 * @return Conduction velocity (m/s)
 */
float nimcp_myelin_compute_velocity_full(nimcp_myelin_biophysics_t* bio,
                                          float axon_diameter_um,
                                          float internode_length_um,
                                          uint32_t num_lamellae,
                                          float g_ratio,
                                          float compaction_score,
                                          float integrity);

//=============================================================================
// Activity-Dependent Myelination Functions
//=============================================================================

/**
 * @brief Compute myelination rate from activity
 *
 * WHAT: Calculate rate of lamellae addition/removal
 * WHY:  Activity-dependent plasticity is biological reality
 * HOW:  Hill-function kinetics with saturation
 *
 * FORMULA:
 *   rate_myelin = k_max × (A^n / (K^n + A^n)) × (1 - N/N_max)
 *   rate_demyelin = k_demyelin × (K_d / (K_d + A))
 *   net_rate = rate_myelin - rate_demyelin
 *
 * BIOLOGICAL BASIS:
 * - Fields (2015): Activity-dependent myelination
 * - Gibson et al. (2014): Neural activity regulation
 * - Mitew et al. (2018): OPC activity response
 *
 * COMPLEXITY: O(1)
 *
 * @param activity Neural activity level (0-1 normalized)
 * @param current_lamellae Current lamellae count
 * @param kinetics Kinetics parameters
 * @return Net myelination rate (lamellae/second)
 */
float nimcp_myelin_compute_myelination_rate(float activity,
                                             float current_lamellae,
                                             const nimcp_myelination_kinetics_t* kinetics);

/**
 * @brief Update lamellae count based on activity
 *
 * WHAT: Apply myelination/demyelination for time step
 * WHY:  Integrate activity over time for plasticity
 * HOW:  Rate × dt with bounds checking
 *
 * @param current_lamellae Current lamellae count
 * @param activity Neural activity level (0-1)
 * @param dt Time step (seconds)
 * @param kinetics Kinetics parameters
 * @return New lamellae count (may be fractional for accumulation)
 */
float nimcp_myelin_update_lamellae(float current_lamellae,
                                    float activity,
                                    float dt,
                                    const nimcp_myelination_kinetics_t* kinetics);

/**
 * @brief Update activity EMA in biophysics state
 *
 * WHAT: Update exponential moving average of activity
 * WHY:  Smooth activity signal for plasticity decisions
 * HOW:  EMA with configurable time constant
 *
 * @param bio Biophysics state
 * @param activity Current activity level
 * @param dt Time step (seconds)
 */
void nimcp_myelin_update_activity_ema(nimcp_myelin_biophysics_t* bio,
                                       float activity,
                                       float dt);

/**
 * @brief Get myelination threshold for activity level
 *
 * WHAT: Calculate activity needed for net myelination
 * WHY:  Determine if current activity promotes growth
 * HOW:  Solve rate equation for zero crossing
 *
 * @param kinetics Kinetics parameters
 * @return Activity threshold for positive myelination
 */
float nimcp_myelin_activity_threshold(const nimcp_myelination_kinetics_t* kinetics);

//=============================================================================
// Conduction Block Functions
//=============================================================================

/**
 * @brief Compute conduction block probability
 *
 * WHAT: Calculate probability of signal failure
 * WHY:  Model pathological conditions (MS, temperature effects)
 * HOW:  Sigmoid function with temperature modulation
 *
 * FORMULA:
 *   P_base = 1 / (1 + exp((I - I_crit) / σ))
 *   T_factor = 1 + k_T × max(0, T - T_ref)
 *   P_block = P_base × T_factor
 *
 * BIOLOGICAL BASIS:
 * - Uhthoff phenomenon: Temperature-dependent block
 * - Smith & McDonald (1999): Demyelination block
 * - Waxman (2006): Conduction failure mechanisms
 *
 * COMPLEXITY: O(1)
 *
 * @param integrity Structural integrity (0-1)
 * @param temperature_c Temperature (°C)
 * @param params Block parameters
 * @return Block probability (0-1)
 */
float nimcp_myelin_block_probability(float integrity,
                                      float temperature_c,
                                      const nimcp_conduction_block_params_t* params);

/**
 * @brief Check if conduction is blocked
 *
 * WHAT: Stochastic determination of block event
 * WHY:  Apply probability to individual conduction attempts
 * HOW:  Compare random number to block probability
 *
 * @param integrity Structural integrity (0-1)
 * @param temperature_c Temperature (°C)
 * @param params Block parameters
 * @param rng Random number generator state
 * @return true if blocked, false if conducting
 */
bool nimcp_myelin_is_blocked(float integrity,
                              float temperature_c,
                              const nimcp_conduction_block_params_t* params,
                              nimcp_myelin_rng_t* rng);

/**
 * @brief Compute frequency-dependent block threshold
 *
 * WHAT: Calculate integrity needed for given frequency
 * WHY:  High-frequency conduction fails first
 * HOW:  Inverse of block probability with frequency factor
 *
 * @param frequency_hz Stimulation frequency (Hz)
 * @param temperature_c Temperature (°C)
 * @param params Block parameters
 * @return Minimum integrity for reliable conduction
 */
float nimcp_myelin_frequency_threshold(float frequency_hz,
                                        float temperature_c,
                                        const nimcp_conduction_block_params_t* params);

//=============================================================================
// Internode Optimization Functions
//=============================================================================

/**
 * @brief Compute optimal internode length
 *
 * WHAT: Calculate ideal segment length for diameter
 * WHY:  Internode length affects velocity and efficiency
 * HOW:  Power-law relationship from biology
 *
 * FORMULA:
 *   L_opt = α × d^β
 *   α ≈ 150, β ≈ 0.9
 *
 * BIOLOGICAL BASIS:
 * - Rushton (1951): Optimal internode theory
 * - Brill et al. (1977): Internode-diameter relationship
 * - Ibrahim et al. (1995): Power-law scaling
 *
 * COMPLEXITY: O(1)
 *
 * @param axon_diameter_um Axon diameter (μm)
 * @return Optimal internode length (μm)
 */
float nimcp_myelin_optimal_internode(float axon_diameter_um);

/**
 * @brief Compute internode efficiency
 *
 * WHAT: Calculate efficiency of current vs optimal length
 * WHY:  Suboptimal length reduces velocity
 * HOW:  Gaussian efficiency centered on optimal
 *
 * @param current_length_um Current internode length (μm)
 * @param axon_diameter_um Axon diameter (μm)
 * @return Efficiency factor (0.5-1.0)
 */
float nimcp_myelin_internode_efficiency(float current_length_um,
                                         float axon_diameter_um);

/**
 * @brief Compute optimal node count for axon
 *
 * WHAT: Calculate ideal number of nodes of Ranvier
 * WHY:  Planning optimal myelination strategy
 * HOW:  Total length / optimal internode
 *
 * @param axon_length_um Total axon length (μm)
 * @param axon_diameter_um Axon diameter (μm)
 * @return Optimal number of nodes
 */
uint32_t nimcp_myelin_optimal_node_count(float axon_length_um,
                                          float axon_diameter_um);

//=============================================================================
// Metabolic Efficiency Functions
//=============================================================================

/**
 * @brief Compute metabolic efficiency metrics
 *
 * WHAT: Calculate energy costs and efficiency
 * WHY:  Quantify metabolic benefit of myelination
 * HOW:  Compare charging costs of myelinated vs unmyelinated
 *
 * FORMULA:
 *   E_unmyelin = C_m × V_AP² × A_membrane
 *   E_myelin = C_m × V_AP² × A_nodes × N_nodes / efficiency_factors
 *   ratio = E_unmyelin / E_myelin
 *
 * BIOLOGICAL BASIS:
 * - Hartline & Colman (2007): Energy efficiency
 * - Harris & Attwell (2012): Neural energy budgets
 * - Nave (2010): Metabolic support
 *
 * COMPLEXITY: O(1)
 *
 * @param axon_length_um Total axon length (μm)
 * @param axon_diameter_um Axon diameter (μm)
 * @param num_nodes Number of nodes of Ranvier
 * @param mean_compaction Mean compaction score (0-1)
 * @param mean_integrity Mean integrity (0-1)
 * @param result Output efficiency metrics
 */
void nimcp_myelin_compute_metabolic_efficiency(float axon_length_um,
                                                float axon_diameter_um,
                                                uint32_t num_nodes,
                                                float mean_compaction,
                                                float mean_integrity,
                                                nimcp_metabolic_efficiency_t* result);

/**
 * @brief Compute ATP cost per action potential
 *
 * WHAT: Calculate ATP molecules needed for one AP
 * WHY:  Link to metabolic simulation
 * HOW:  Energy cost / energy per ATP
 *
 * @param efficiency Efficiency result from compute function
 * @return ATP molecules per action potential
 */
float nimcp_myelin_atp_per_ap(const nimcp_metabolic_efficiency_t* efficiency);

/**
 * @brief Compute power consumption at given firing rate
 *
 * WHAT: Calculate metabolic power for sustained activity
 * WHY:  Link to brain energy budgets
 * HOW:  Energy per AP × firing rate
 *
 * @param efficiency Efficiency metrics
 * @param firing_rate_hz Firing rate (Hz)
 * @return Power consumption (μW)
 */
float nimcp_myelin_power_consumption(const nimcp_metabolic_efficiency_t* efficiency,
                                      float firing_rate_hz);

//=============================================================================
// Stochastic Variability Functions
//=============================================================================

/**
 * @brief Initialize random number generator
 *
 * WHAT: Set up RNG for stochastic simulations
 * WHY:  Reproducible biological variability
 * HOW:  Seed-based initialization
 *
 * @param rng RNG state to initialize
 * @param seed Seed value (0 for time-based)
 */
void nimcp_myelin_rng_init(nimcp_myelin_rng_t* rng, uint64_t seed);

/**
 * @brief Reset RNG to initial state
 * @param rng RNG state to reset
 */
void nimcp_myelin_rng_reset(nimcp_myelin_rng_t* rng);

/**
 * @brief Generate uniform random value
 *
 * @param rng RNG state
 * @return Uniform random value [0, 1)
 */
float nimcp_myelin_rng_uniform(nimcp_myelin_rng_t* rng);

/**
 * @brief Generate normal random value
 *
 * WHAT: Generate Gaussian distributed random value
 * WHY:  Normal distribution for biological variability
 * HOW:  Box-Muller transform
 *
 * @param rng RNG state
 * @param mean Distribution mean
 * @param stddev Standard deviation
 * @return Normal random value
 */
float nimcp_myelin_rng_normal(nimcp_myelin_rng_t* rng, float mean, float stddev);

/**
 * @brief Generate log-normal random value
 *
 * WHAT: Generate log-normal distributed value
 * WHY:  Positive-valued biological quantities
 * HOW:  Exp of normal distribution
 *
 * @param rng RNG state
 * @param mean Target mean (not log-mean)
 * @param cv Coefficient of variation
 * @return Log-normal random value
 */
float nimcp_myelin_rng_lognormal(nimcp_myelin_rng_t* rng, float mean, float cv);

/**
 * @brief Apply variability to lamellae count
 *
 * @param rng RNG state
 * @param target_lamellae Target lamellae count
 * @return Varied lamellae count
 */
uint32_t nimcp_myelin_vary_lamellae(nimcp_myelin_rng_t* rng, uint32_t target_lamellae);

/**
 * @brief Apply variability to g-ratio
 *
 * @param rng RNG state
 * @param target_g_ratio Target g-ratio
 * @return Varied g-ratio
 */
float nimcp_myelin_vary_g_ratio(nimcp_myelin_rng_t* rng, float target_g_ratio);

/**
 * @brief Apply variability to internode length
 *
 * @param rng RNG state
 * @param target_length_um Target length (μm)
 * @return Varied internode length (μm)
 */
float nimcp_myelin_vary_internode(nimcp_myelin_rng_t* rng, float target_length_um);

/**
 * @brief Apply variability to conduction velocity
 *
 * @param rng RNG state
 * @param target_velocity Target velocity (m/s)
 * @return Varied velocity (m/s)
 */
float nimcp_myelin_vary_velocity(nimcp_myelin_rng_t* rng, float target_velocity);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Clamp value to range
 */
static inline float nimcp_myelin_clamp(float val, float min, float max) {
    return val < min ? min : (val > max ? max : val);
}

/**
 * @brief Safe division with epsilon
 */
static inline float nimcp_myelin_safe_div(float num, float denom) {
    return num / (denom + NIMCP_MYELIN_MATH_EPSILON);
}

/**
 * @brief Fast approximate exponential
 *
 * WHAT: Pade approximation for exp()
 * WHY:  Performance-critical path optimization
 * HOW:  Rational approximation valid for |x| < 4
 *
 * @param x Input value
 * @return Approximate exp(x)
 */
float nimcp_myelin_fast_exp(float x);

/**
 * @brief Fast approximate square root
 *
 * @param x Input value (must be positive)
 * @return Approximate sqrt(x)
 */
float nimcp_myelin_fast_sqrt(float x);

/**
 * @brief Fast approximate power
 *
 * @param base Base value
 * @param exp Exponent
 * @return Approximate base^exp
 */
float nimcp_myelin_fast_pow(float base, float exp);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MYELIN_MATH_H
