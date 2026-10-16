/**
 * @file nimcp_astrocytes.h
 * @brief Biological astrocyte glial cells for synaptic modulation and homeostasis
 *
 * BIOLOGICAL CONTEXT:
 * Astrocytes are star-shaped glial cells that perform critical functions:
 * - Cover ~100,000 synapses in mammalian cortex
 * - Calcium waves propagate via IP3/gap junctions at 10-30 µm/s
 * - Release glutamate/D-serine to modulate synaptic transmission
 * - Enforce homeostatic plasticity (synaptic scaling)
 * - Modulate BCM plasticity thresholds
 * - Provide metabolic support (ATP/lactate to neurons)
 *
 * DESIGN PRINCIPLES:
 * - Single Responsibility: Each function has one clear purpose
 * - Open/Closed: Extends neural network without modifying core
 * - Performance: O(log N) spatial indexing, SIMD calcium dynamics
 * - Thread-safe: Spinlocks for concurrent updates
 *
 * INTEGRATION POINTS:
 * - nimcp_neuralnet.c: Synaptic transmission modulation
 * - nimcp_bcm.c: BCM threshold modulation
 * - nimcp_neuromodulators.c: Glutamate release
 *
 * @version 2.6.0
 */

#ifndef NIMCP_ASTROCYTES_H
#define NIMCP_ASTROCYTES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "utils/validation/nimcp_common.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/thread/nimcp_thread.h"
#include "glial/astrocyte_types/nimcp_astrocyte_types.h"

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of synapses one astrocyte can cover */
#define ASTROCYTE_MAX_SYNAPSES 1000

/** Maximum number of gap junction coupled neighbors */
#define ASTROCYTE_MAX_COUPLED 6

/** Biological baseline calcium concentration (µM) */
#define ASTROCYTE_BASELINE_CALCIUM_UM 0.1f

/** Calcium threshold for wave propagation (µM) */
#define ASTROCYTE_CALCIUM_WAVE_THRESHOLD_UM 2.0f

/** Gap junction coupling radius (µm) */
#define ASTROCYTE_COUPLING_RADIUS_UM 100.0f

//=============================================================================
// Enhancement A4.1: Reaction-Diffusion Calcium Dynamics Constants
//=============================================================================

/** Calcium diffusion coefficient (µm²/s) - TUNED for computational wave propagation */
#define CALCIUM_DIFFUSION_COEFF 100.0f

/** IP3 diffusion coefficient (µm²/s) - IP3 diffuses faster than Ca */
#define IP3_DIFFUSION_COEFF 200.0f

/** Calcium wave propagation speed (µm/s) - computational range 100-2000 */
#define CALCIUM_WAVE_SPEED_TARGET 500.0f

/** IP3 production rate constant - stimulus-driven */
#define IP3_PRODUCTION_RATE 1.0f

/** IP3 degradation rate constant (1/s) - TUNED for fast decay */
#define IP3_DEGRADATION_RATE 3.0f

/** Calcium release flux coefficient - TUNED for strong wave propagation */
#define CALCIUM_RELEASE_FLUX 0.5f

/** Calcium uptake/pump rate constant (1/s) - TUNED for fast decay to baseline */
#define CALCIUM_UPTAKE_RATE 3.0f

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Forward declarations
 */
typedef struct astrocyte_network_t astrocyte_network_t;
typedef struct astrocyte_calcium_system_t astrocyte_calcium_system_t;

/**
 * @brief Individual astrocyte cell state
 *
 * SINGLE RESPONSIBILITY: Represents one astrocyte with all its physiological states
 *
 * Memory layout optimized for cache efficiency:
 * - Hot path: calcium_concentration, ip3_concentration (frequently accessed)
 * - Warm path: glutamate_pool, d_serine_pool, atp_level
 * - Cold path: position, coupling data (infrequently accessed)
 */
typedef struct astrocyte_t {
    // === IDENTITY ===
    uint32_t id;                        /**< Unique astrocyte identifier */
    astrocyte_type_t type;              /**< Astrocyte type (Phase 8.7) */

    // === CALCIUM DYNAMICS (HOT PATH) ===
    float calcium_concentration;        /**< [Ca2+]i in µM (0.1-10.0) */
    float ip3_concentration;            /**< IP3 in µM (0-5.0) */
    float calcium_baseline;             /**< Resting [Ca2+] (~0.1 µM) */
    uint64_t last_calcium_spike;        /**< Timestamp of last Ca2+ spike */

    // === NEUROTRANSMITTER POOLS (WARM PATH) ===
    float glutamate_pool;               /**< Releasable glutamate (0-1 normalized) */
    float d_serine_pool;                /**< Releasable D-serine (0-1 normalized) */
    float atp_level;                    /**< Metabolic energy state (0-1) */

    // === SYNAPTIC COVERAGE DOMAIN ===
    uint32_t num_covered_synapses;      /**< Number of synapses in domain */
    uint32_t* covered_synapse_ids;      /**< Array[num_covered_synapses] of synapse IDs */
    float* synapse_calcium_levels;      /**< Array[num_covered_synapses] of local [Ca2+] microdomains */

    // === SPATIAL LOCATION ===
    float position[3];                  /**< x, y, z coordinates (µm) */
    float coverage_radius;              /**< Spatial coverage radius (µm) */

    // === GAP JUNCTION COUPLING (COLD PATH) ===
    uint32_t num_coupled_astrocytes;    /**< Number of gap junction neighbors */
    uint32_t* coupled_astrocyte_ids;    /**< Array[num_coupled] of neighbor IDs */
    float* coupling_strengths;          /**< Array[num_coupled] of conductance (0-1) */

    // === HOMEOSTATIC REGULATION ===
    float target_activity_level;        /**< Desired average network activity */
    float scaling_factor;               /**< Current synaptic scaling multiplier */
    float integral_error;               /**< Accumulated integral error for PID scaling (anti-windup bounded) */

    // === THREAD SAFETY ===
    nimcp_spinlock_t lock;              /**< Spinlock for concurrent calcium updates */

} astrocyte_t;

/**
 * @brief Enhancement A4.1: Reaction-Diffusion Calcium System
 *
 * SINGLE RESPONSIBILITY: Manages coupled Ca²⁺ and IP3 reaction-diffusion dynamics
 *
 * BIOLOGICAL MODEL:
 * - ∂Ca²⁺/∂t = D_Ca∇²Ca²⁺ + J_release - J_uptake
 * - ∂IP3/∂t = D_IP3∇²IP3 + production - degradation
 * - Graph-based discretization on astrocyte network topology
 *
 * INTEGRATION:
 * - Integrated with glial_integration module for calcium wave feedback
 * - Triggers gliotransmitter release when Ca²⁺ exceeds threshold
 * - Performance overhead target: < 15%
 */
struct astrocyte_calcium_system_t {
    uint32_t num_astrocytes;            /**< Number of astrocytes in system */

    // === STATE ARRAYS (HOT PATH) ===
    float* calcium;                     /**< [num_astrocytes] Ca²⁺ concentration (µM) */
    float* ip3;                         /**< [num_astrocytes] IP3 concentration (µM) */
    float* calcium_er;                  /**< [num_astrocytes] ER Ca²⁺ stores (µM) */

    // === WORKSPACE ARRAYS (pre-allocated for performance) ===
    float* workspace_dCa;               /**< [num_astrocytes] temp array for Ca derivatives */
    float* workspace_dIP3;              /**< [num_astrocytes] temp array for IP3 derivatives */
    float* workspace_dCaER;             /**< [num_astrocytes] temp array for ER derivatives */

    // === DIFFUSION COEFFICIENTS ===
    float D_ca;                         /**< Calcium diffusion coefficient (µm²/s) */
    float D_ip3;                        /**< IP3 diffusion coefficient (µm²/s) */

    // === REACTION RATE CONSTANTS ===
    float ip3_production_rate;          /**< IP3 synthesis rate */
    float ip3_degradation_rate;         /**< IP3 breakdown rate (1/s) */
    float ca_release_flux;              /**< Ca²⁺ release from ER */
    float ca_uptake_rate;               /**< Ca²⁺ pump rate (1/s) */

    // === NETWORK TOPOLOGY (for graph-based diffusion) ===
    astrocyte_network_t* network;       /**< Parent network for topology */

    // === WAVE TRACKING ===
    uint64_t* last_wave_time;           /**< [num_astrocytes] Last wave timestamp (µs) */
    float wave_speed_measured;          /**< Measured wave speed (µm/s) */

    // === PERFORMANCE METRICS ===
    uint64_t total_update_time_us;      /**< Total time spent in updates (µs) */
    uint64_t num_updates;               /**< Number of update calls */

    // === THREAD SAFETY ===
    nimcp_spinlock_t lock;              /**< Spinlock for concurrent updates */
};

/**
 * @brief Astrocyte network manager
 *
 * SINGLE RESPONSIBILITY: Manages astrocyte population and spatial indexing
 *
 * PERFORMANCE:
 * - Spatial index enables O(log N) nearest-neighbor queries
 * - Gap junction coupling graph for calcium wave propagation
 * - Thread-safe for parallel partition updates
 */
struct astrocyte_network_t {
    uint32_t num_astrocytes;            /**< Current number of astrocytes */
    uint32_t capacity;                  /**< Maximum capacity */
    astrocyte_t** astrocytes;           /**< Array of astrocyte pointers */

    // === SPATIAL INDEXING ===
    void* spatial_index;                /**< KD-tree for O(log N) nearest-neighbor */

    // === GLOBAL PARAMETERS ===
    float calcium_threshold_um;         /**< Threshold for Ca2+ wave propagation */
    float coupling_decay_rate;          /**< Gap junction diffusion rate (1/ms) */
    float coupling_radius_um;           /**< Max distance for gap junction coupling */

    // === ENHANCEMENT A4.1: Calcium Dynamics ===
    astrocyte_calcium_system_t* calcium_system; /**< Reaction-diffusion calcium system */

    // === THREAD SAFETY ===
    nimcp_mutex_t lock;                 /**< Mutex for network-level operations */
};

//=============================================================================
// Astrocyte Creation and Destruction
//=============================================================================

/**
 * @brief Create an astrocyte
 *
 * WHAT: Allocates and initializes an astrocyte with given spatial location and type
 * WHY:  Astrocytes need spatial positioning for coverage and coupling
 * HOW:  Allocates memory, sets position, type, initializes calcium to baseline
 *
 * @param id Unique astrocyte identifier
 * @param type Astrocyte type (Phase 8.7: regional specialization)
 * @param x X position in µm
 * @param y Y position in µm
 * @param z Z position in µm
 * @param coverage_radius Coverage radius in µm (typically 20-100 µm)
 * @return Pointer to astrocyte, or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (allocates independent memory)
 */
astrocyte_t* astrocyte_create(uint32_t id, astrocyte_type_t type, float x, float y, float z, float coverage_radius);

/**
 * @brief Destroy an astrocyte
 *
 * WHAT: Frees all memory associated with an astrocyte
 * WHY:  Prevent memory leaks
 * HOW:  Frees synapse arrays, coupling arrays, then astrocyte itself
 *
 * @param astro Astrocyte to destroy (can be NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must ensure astrocyte not in use)
 */
void astrocyte_destroy(astrocyte_t* astro);

//=============================================================================
// Calcium Dynamics (CORE FUNCTION)
//=============================================================================

/**
 * @brief Update astrocyte calcium concentration
 *
 * WHAT: Integrates calcium ODEs using Euler or RK4 method
 * WHY:  Calcium is the primary signaling mechanism in astrocytes
 * HOW:  Implements Li-Rinzel model or simplified Hill dynamics
 *
 * BIOLOGICAL MODEL:
 * d[Ca2+]/dt = J_channel + J_leak - J_pump - J_ER
 * where:
 * - J_channel = IP3-dependent calcium release from ER
 * - J_leak = passive calcium leak
 * - J_pump = active calcium reuptake (ATP-dependent)
 * - J_ER = calcium buffering by ER
 *
 * @param astro Astrocyte to update
 * @param dt Timestep in seconds (typically 0.0001-0.001)
 * @param external_stimulus External calcium stimulus (e.g., from synaptic activity)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (uses astrocyte->lock)
 */
void astrocyte_update_calcium(astrocyte_t* astro, float dt, float external_stimulus);

/**
 * @brief Propagate calcium wave to coupled neighbors
 *
 * WHAT: Diffuses calcium through gap junctions to neighboring astrocytes
 * WHY:  Calcium waves coordinate plasticity across spatial domains
 * HOW:  Implements diffusion equation with coupling conductances
 *
 * BIOLOGICAL BASIS:
 * - Calcium waves propagate at ~10-30 µm/s in vivo
 * - Gap junctions allow IP3 and Ca2+ diffusion
 * - Wave amplitude decays with distance
 *
 * @param astro Source astrocyte
 * @param network Astrocyte network containing neighbors
 * @param dt Timestep in seconds
 *
 * COMPLEXITY: O(K) where K = num_coupled_astrocytes (typically 3-6)
 * THREAD-SAFE: Yes (locks both source and neighbor astrocytes)
 */
void astrocyte_propagate_calcium_wave(astrocyte_t* astro,
                                      astrocyte_network_t* network,
                                      float dt);

//=============================================================================
// Neurotransmitter Release
//=============================================================================

/**
 * @brief Compute glutamate release for a specific synapse
 *
 * WHAT: Calculates glutamate release based on local calcium
 * WHY:  Glutamate modulates synaptic transmission efficacy
 * HOW:  Sigmoid function of synapse_calcium_levels[synapse_idx]
 *
 * BIOLOGICAL BASIS:
 * - Calcium-dependent exocytosis (threshold ~1 µM)
 * - Glutamate enhances AMPA/NMDA receptor sensitivity
 * - Time scale: 10-100ms release kinetics
 *
 * @param astro Astrocyte
 * @param synapse_idx Index into covered_synapse_ids array
 * @return Glutamate concentration (0-1 normalized)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
float astrocyte_compute_glutamate_release(astrocyte_t* astro, uint32_t synapse_idx);

/**
 * @brief Compute D-serine release for a specific synapse
 *
 * WHAT: Calculates D-serine release (NMDA co-agonist)
 * WHY:  D-serine required for NMDA-dependent plasticity
 * HOW:  Calcium-dependent release similar to glutamate
 *
 * BIOLOGICAL BASIS:
 * - D-serine is obligatory NMDA co-agonist
 * - Released by astrocytes, not neurons
 * - Critical for LTP induction
 *
 * @param astro Astrocyte
 * @param synapse_idx Index into covered_synapse_ids array
 * @return D-serine concentration (0-1 normalized)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float astrocyte_compute_d_serine_release(astrocyte_t* astro, uint32_t synapse_idx);

//=============================================================================
// Synaptic Modulation
//=============================================================================

/**
 * @brief Modulate synapse strength based on astrocyte state
 *
 * WHAT: Adjusts synapse->strength based on glutamate/D-serine levels
 * WHY:  Astrocyte-synapse interaction regulates transmission
 * HOW:  Multiplicative modulation: strength *= (1 + glu_factor)
 *
 * INTEGRATION POINT: Called from nimcp_neuralnet.c sum_synaptic_inputs()
 *
 * @param astro Astrocyte
 * @param synapse Synapse to modulate
 * @param synapse_idx Index in astrocyte's covered_synapses array
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must hold synapse lock)
 */
void astrocyte_modulate_synapse_strength(astrocyte_t* astro,
                                         synapse_t* synapse,
                                         uint32_t synapse_idx);

/**
 * @brief Assign synapse to astrocyte's coverage domain
 *
 * WHAT: Adds synapse ID to astrocyte's covered_synapses array
 * WHY:  Establishes which synapses this astrocyte monitors
 * HOW:  Reallocates array if needed, initializes calcium microdomain
 *
 * @param astro Astrocyte
 * @param synapse_id Synapse ID to cover
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1) amortized
 * THREAD-SAFE: No (use during network setup only)
 */
nimcp_result_t astrocyte_assign_synapse(astrocyte_t* astro, uint32_t synapse_id);

//=============================================================================
// Homeostatic Plasticity
//=============================================================================

/**
 * @brief Compute synaptic scaling factor for homeostasis
 *
 * WHAT: Calculates multiplicative scaling based on activity deviation
 * WHY:  Maintains target activity level (prevents runaway excitation/silence)
 * HOW:  Scaling = f(target_activity - current_activity)
 *
 * BIOLOGICAL BASIS:
 * - Homeostatic plasticity operates on hours-to-days timescale
 * - Astrocytes sense average activity via calcium integral
 * - Scale all synapses multiplicatively to restore target
 *
 * ALGORITHM:
 * 1. Estimate current activity from calcium concentration
 * 2. Compute error = target - current
 * 3. scaling = 1.0 + gain * error
 *
 * @param astro Astrocyte
 * @param network Neural network (for activity estimation)
 * @return Scaling factor (typically 0.5 - 2.0)
 *
 * COMPLEXITY: O(S) where S = num_covered_synapses
 * THREAD-SAFE: Yes
 */
float astrocyte_compute_synaptic_scaling(astrocyte_t* astro, neural_network_t network);

//=============================================================================
// BCM Plasticity Modulation
//=============================================================================

/**
 * @brief Compute shift to BCM threshold based on astrocyte state
 *
 * WHAT: Calculates additive shift to BCM sliding threshold θ
 * WHY:  Astrocytes modulate plasticity rules via threshold control
 * HOW:  shift = f(calcium_concentration - baseline)
 *
 * BIOLOGICAL BASIS:
 * - Elevated astrocyte calcium raises BCM threshold
 * - Shifts balance between LTP and LTD
 * - Implements metaplasticity (plasticity of plasticity)
 *
 * INTEGRATION POINT: Called from nimcp_bcm.c bcm_apply_rule()
 *
 * @param astro Astrocyte
 * @param default_threshold Current BCM threshold θ
 * @return Threshold shift (additive, typically -0.2 to +0.2)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float astrocyte_compute_bcm_threshold_shift(astrocyte_t* astro, float default_threshold);

//=============================================================================
// Metabolic Support
//=============================================================================

/**
 * @brief Update astrocyte ATP level based on neural activity
 *
 * WHAT: Integrates ATP production/consumption dynamics
 * WHY:  Astrocytes provide metabolic support to neurons
 * HOW:  d[ATP]/dt = production - consumption(activity)
 *
 * BIOLOGICAL BASIS:
 * - Astrocytes convert glucose to lactate
 * - Transfer lactate to neurons for ATP production
 * - High neural activity depletes astrocyte ATP
 *
 * @param astro Astrocyte
 * @param neural_activity Average activity of covered neurons (0-10)
 * @param dt Timestep in seconds
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void astrocyte_update_atp_level(astrocyte_t* astro, float neural_activity, float dt);

//=============================================================================
// Astrocyte Network Management
//=============================================================================

/**
 * @brief Create astrocyte network
 *
 * WHAT: Allocates astrocyte network manager
 * WHY:  Manages population-level operations (coupling, spatial indexing)
 * HOW:  Allocates arrays, initializes spatial index structure
 *
 * @param capacity Initial capacity for number of astrocytes
 * @return Pointer to network, or NULL on failure
 *
 * COMPLEXITY: O(capacity)
 * THREAD-SAFE: Yes
 */
astrocyte_network_t* astrocyte_network_create(uint32_t capacity);

/**
 * @brief Destroy astrocyte network
 *
 * WHAT: Frees all astrocytes and network structures
 * WHY:  Prevent memory leaks
 * HOW:  Destroys all astrocytes, frees spatial index, frees network
 *
 * @param network Network to destroy
 *
 * COMPLEXITY: O(N) where N = num_astrocytes
 * THREAD-SAFE: No (caller must ensure no concurrent access)
 */
void astrocyte_network_destroy(astrocyte_network_t* network);

/**
 * @brief Add astrocyte to network
 *
 * WHAT: Adds astrocyte to network's array (network takes ownership)
 * WHY:  Centralized management of astrocyte population
 * HOW:  Resizes array if needed, adds to end
 *
 * @param network Astrocyte network
 * @param astro Astrocyte to add (network takes ownership)
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1) amortized
 * THREAD-SAFE: No (use during setup only)
 */
nimcp_result_t astrocyte_network_add(astrocyte_network_t* network, astrocyte_t* astro);

/**
 * @brief Build spatial index for fast nearest-neighbor queries
 *
 * WHAT: Constructs KD-tree from astrocyte positions
 * WHY:  Enables O(log N) spatial queries instead of O(N)
 * HOW:  Builds 3D KD-tree partitioning on x, y, z alternating
 *
 * @param network Astrocyte network
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(N log N) where N = num_astrocytes
 * THREAD-SAFE: No (call once after all astrocytes added)
 */
nimcp_result_t astrocyte_network_build_spatial_index(astrocyte_network_t* network);

/**
 * @brief Find nearest astrocyte to a 3D point
 *
 * WHAT: Queries spatial index for nearest astrocyte
 * WHY:  Assign synapses/neurons to nearest astrocyte
 * HOW:  KD-tree nearest-neighbor search
 *
 * @param network Astrocyte network
 * @param point Query point [x, y, z] in µm
 * @return Nearest astrocyte, or NULL if network empty
 *
 * COMPLEXITY: O(log N) average case
 * THREAD-SAFE: Yes (read-only query)
 */
astrocyte_t* astrocyte_network_find_nearest(astrocyte_network_t* network, const float point[3]);

/**
 * @brief Establish gap junction coupling between astrocytes
 *
 * WHAT: Connects astrocytes within coupling_radius via gap junctions
 * WHY:  Required for calcium wave propagation
 * HOW:  For each astrocyte, find neighbors within radius, add to coupled_astrocyte_ids
 *
 * BIOLOGICAL BASIS:
 * - Gap junctions connect astrocytes within ~100 µm
 * - Connexin channels allow Ca2+ and IP3 diffusion
 * - Forms syncytium for coordinated calcium signaling
 *
 * @param network Astrocyte network
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(N log N) using spatial index
 * THREAD-SAFE: No (call once after spatial index built)
 */
nimcp_result_t astrocyte_network_establish_coupling(astrocyte_network_t* network);

/**
 * @brief Assign synapses to astrocytes based on spatial proximity
 *
 * WHAT: For each synapse, finds nearest astrocyte and assigns coverage
 * WHY:  Establishes astrocyte-synapse topology
 * HOW:  Queries spatial index for each synapse position
 *
 * @param network Astrocyte network
 * @param nn Neural network containing synapses
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(S log N) where S = total synapses, N = num_astrocytes
 * THREAD-SAFE: No (call once during network setup)
 */
nimcp_result_t astrocyte_network_assign_synapses(astrocyte_network_t* network,
                                                 neural_network_t* nn);

/**
 * @brief Step astrocyte network forward in time
 *
 * WHAT: Updates all astrocytes (calcium, ATP, glutamate pools)
 * WHY:  Integrate dynamics for entire population
 * HOW:  For each astrocyte: update_calcium, propagate_wave, update_atp
 *
 * @param network Astrocyte network
 * @param dt Timestep in seconds
 *
 * COMPLEXITY: O(N × K) where N = num_astrocytes, K = avg coupled neighbors
 * THREAD-SAFE: Can parallelize across astrocytes with partition assignment
 */
void astrocyte_network_step(astrocyte_network_t* network, float dt);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get astrocyte statistics for monitoring
 *
 * @param network Astrocyte network
 * @param avg_calcium Output: average calcium across population
 * @param max_calcium Output: maximum calcium
 * @param avg_glutamate Output: average glutamate pool
 */
void astrocyte_network_get_stats(astrocyte_network_t* network,
                                 float* avg_calcium,
                                 float* max_calcium,
                                 float* avg_glutamate);

//=============================================================================
// Enhancement A4.1: Reaction-Diffusion Calcium System API
//=============================================================================

/**
 * @brief Create calcium system for astrocyte network
 *
 * WHAT: Allocates and initializes reaction-diffusion system
 * WHY:  Enables realistic calcium wave propagation
 * HOW:  Allocates state arrays, sets parameters from biological data
 *
 * @param network Astrocyte network to attach to
 * @return Calcium system pointer, or NULL on failure
 *
 * COMPLEXITY: O(N) where N = num_astrocytes
 * THREAD-SAFE: Yes
 */
astrocyte_calcium_system_t* astrocyte_calcium_system_create(astrocyte_network_t* network);

/**
 * @brief Destroy calcium system
 *
 * WHAT: Frees all memory associated with calcium system
 * WHY:  Prevent memory leaks
 * HOW:  Frees state arrays, then system itself
 *
 * @param system Calcium system to destroy (can be NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must ensure not in use)
 */
void astrocyte_calcium_system_destroy(astrocyte_calcium_system_t* system);

/**
 * @brief Update calcium dynamics using reaction-diffusion equations
 *
 * WHAT: Integrates coupled Ca²⁺ and IP3 dynamics on astrocyte network graph
 * WHY:  Simulates calcium waves with realistic propagation speed
 * HOW:  Graph-based discretization of diffusion + reaction terms
 *
 * BIOLOGICAL MODEL:
 * ∂Ca²⁺/∂t = D_Ca * (graph Laplacian Ca²⁺) + J_release(IP3, Ca_ER) - J_uptake(Ca²⁺)
 * ∂IP3/∂t = D_IP3 * (graph Laplacian IP3) + production - degradation * IP3
 *
 * where:
 * - Graph Laplacian: Σ_neighbors (C_neighbor - C_self) / distance²
 * - J_release: IP3-dependent Ca²⁺ release from ER
 * - J_uptake: ATP-dependent Ca²⁺ pumps
 *
 * @param system Calcium system
 * @param dt Timestep in seconds (typically 0.0001-0.001s)
 * @param external_stimulus Per-astrocyte external stimulus array (can be NULL)
 *
 * COMPLEXITY: O(N × K) where N = num_astrocytes, K = avg coupled neighbors (~3-6)
 * THREAD-SAFE: Yes (uses system->lock)
 *
 * PERFORMANCE TARGET: < 15% overhead vs simple calcium decay
 */
void astrocyte_calcium_system_update(astrocyte_calcium_system_t* system,
                                     float dt,
                                     float* external_stimulus);

/**
 * @brief Stimulate specific astrocyte to initiate calcium wave
 *
 * WHAT: Increases Ca²⁺ and IP3 at source astrocyte
 * WHY:  Initiates calcium wave for testing or biological events
 * HOW:  Sets concentrations above threshold, triggers wave propagation
 *
 * @param system Calcium system
 * @param astrocyte_id ID of astrocyte to stimulate
 * @param intensity Stimulus intensity (0-10, where 5+ triggers waves)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void astrocyte_calcium_system_stimulate(astrocyte_calcium_system_t* system,
                                        uint32_t astrocyte_id,
                                        float intensity);

/**
 * @brief Measure calcium wave propagation speed
 *
 * WHAT: Estimates wave speed from recent wave events
 * WHY:  Validation against biological data (10-20 µm/s)
 * HOW:  Tracks wavefront position vs time
 *
 * @param system Calcium system
 * @return Measured wave speed in µm/s, or 0.0 if no recent waves
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float astrocyte_calcium_system_get_wave_speed(astrocyte_calcium_system_t* system);

/**
 * @brief Get performance overhead as percentage
 *
 * WHAT: Computes time spent in calcium updates vs total simulation time
 * WHY:  Monitor performance impact (target < 15%)
 * HOW:  Returns (total_update_time / total_sim_time) × 100
 *
 * @param system Calcium system
 * @param total_sim_time_us Total simulation time in microseconds
 * @return Overhead percentage (0-100), or 0.0 if no updates yet
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float astrocyte_calcium_system_get_overhead_percent(astrocyte_calcium_system_t* system,
                                                    uint64_t total_sim_time_us);

/**
 * @brief Check if gliotransmitter release should occur
 *
 * WHAT: Determines if Ca²⁺ level exceeds release threshold
 * WHY:  Triggers glutamate/D-serine release to modulate synapses
 * HOW:  Compares calcium[astrocyte_id] to threshold
 *
 * INTEGRATION POINT: Called by glial_integration module
 *
 * @param system Calcium system
 * @param astrocyte_id ID of astrocyte to check
 * @return true if gliotransmitter should be released
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool astrocyte_calcium_system_should_release_gliotransmitter(
    astrocyte_calcium_system_t* system,
    uint32_t astrocyte_id);

/**
 * @brief Get calcium concentration for specific astrocyte
 *
 * @param system Calcium system
 * @param astrocyte_id ID of astrocyte
 * @return Ca²⁺ concentration in µM, or 0.0 if invalid ID
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float astrocyte_calcium_system_get_calcium(astrocyte_calcium_system_t* system,
                                           uint32_t astrocyte_id);

/**
 * @brief Get IP3 concentration for specific astrocyte
 *
 * @param system Calcium system
 * @param astrocyte_id ID of astrocyte
 * @return IP3 concentration in µM, or 0.0 if invalid ID
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float astrocyte_calcium_system_get_ip3(astrocyte_calcium_system_t* system,
                                       uint32_t astrocyte_id);

//=============================================================================
// Bio-Async Integration API
//=============================================================================

/**
 * @brief Register astrocyte bio-async message handlers
 *
 * WHAT: Registers handlers for calcium wave, glutamate uptake, metabolic, and sync messages
 * WHY:  Enables event-driven astrocyte coordination via bio-async messaging
 * HOW:  Calls internal bio_init which registers with bio-router
 *
 * This function should be called during glial system initialization.
 * It registers handlers for:
 * - BIO_MSG_ASTROCYTE_CALCIUM_WAVE
 * - BIO_MSG_ASTROCYTE_GLUTAMATE_UPTAKE
 * - BIO_MSG_METABOLIC_DEMAND
 * - BIO_MSG_GLIAL_SYNC_REQUEST
 *
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (uses internal mutex)
 */
nimcp_result_t astrocyte_register_bio_handlers(void);

/**
 * @brief Unregister astrocyte bio-async handlers and cleanup
 *
 * WHAT: Unregisters from bio-router and frees bio-async resources
 * WHY:  Clean shutdown of bio-async integration
 * HOW:  Calls internal bio_shutdown
 *
 * Should be called during glial system shutdown.
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void astrocyte_unregister_bio_handlers(void);

//=============================================================================
// Phase 8: State Manager Integration for Fault Tolerance
//=============================================================================

/* Forward declaration for state ops (avoid circular include) */
struct nimcp_module_state_ops;
typedef struct nimcp_module_state_ops nimcp_module_state_ops_t;

/**
 * @brief Get astrocyte network state operations for state manager registration
 *
 * Returns a pointer to a static nimcp_module_state_ops_t structure that
 * provides serialize/deserialize/validate/reset/get_size operations for
 * astrocyte network state checkpointing and recovery.
 *
 * Usage:
 *   nimcp_state_manager_register(manager, "astrocyte_network",
 *                                 astrocyte_network_get_state_ops(), network_ptr);
 *
 * NOTE: Serialization includes network parameters and per-astrocyte core state.
 * Spatial index and coupling topology are NOT serialized and must be rebuilt
 * after restore using astrocyte_network_build_spatial_index() and
 * astrocyte_network_establish_coupling().
 *
 * @return Pointer to static state ops structure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
const nimcp_module_state_ops_t* astrocyte_network_get_state_ops(void);

/**
 * @brief Set health agent for astrocyte operations (Phase 8: Heartbeat)
 *
 * @param agent Health agent for heartbeat monitoring
 */
struct nimcp_health_agent;
void astrocyte_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ASTROCYTES_H
