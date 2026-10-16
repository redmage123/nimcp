//=============================================================================
// nimcp_stp.h - Short-Term Synaptic Plasticity
//=============================================================================
/**
 * @file nimcp_stp.h
 * @brief Short-term plasticity: synaptic depression and facilitation
 *
 * BIOLOGICAL BACKGROUND:
 * Short-term plasticity (STP) operates on timescales of milliseconds to seconds:
 *
 * DEPRESSION (STD):
 * - Depletion of readily-releasable neurotransmitter vesicles
 * - Each spike reduces available resources
 * - Recovery time constant: τ_D ~100-1000ms
 * - Dominant in excitatory→excitatory synapses
 *
 * FACILITATION (STF):
 * - Residual calcium buildup increases release probability
 * - Each spike increases facilitation
 * - Decay time constant: τ_F ~10-100ms
 * - Dominant in excitatory→inhibitory synapses
 *
 * MATHEMATICAL MODEL (Tsodyks-Markram):
 *   dx/dt = (1 - x) / τ_D             (recovery of resources)
 *   du/dt = (U - u) / τ_F             (decay of facilitation)
 *   On spike: u ← u + U(1 - u)        (facilitation increases)
 *             x ← x - ux               (resource depletion)
 *   PSC amplitude: A = A_max · u · x  (effective strength)
 *
 * PARAMETERS:
 * - U: Baseline release probability (0.2-0.5)
 * - τ_D: Depression time constant (100-1000ms)
 * - τ_F: Facilitation time constant (10-100ms)
 * - A_max: Maximum PSC amplitude
 *
 * SYNAPSE TYPES:
 * - Depressing: High U, long τ_D, short τ_F (cortical L4→L2/3)
 * - Facilitating: Low U, short τ_D, long τ_F (cortical L2/3→L5)
 * - Mixed: Intermediate values
 *
 * REFERENCES:
 * - Tsodyks & Markram (1997). PNAS 94(2):719-723
 * - Abbott et al. (1997). J Neurophysiol 78:3320-3330
 *
 * @author NIMCP Development Team
 * @date 2025-11-06
 */

#ifndef NIMCP_STP_H
#define NIMCP_STP_H

#include <stdbool.h>
#include <stdint.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/**
 * @brief Default parameter values
 */
#define STP_DEFAULT_U 0.5f           /**< Default release probability */
#define STP_DEFAULT_TAU_D 200.0f     /**< Default depression time constant (ms) */
#define STP_DEFAULT_TAU_F 50.0f      /**< Default facilitation time constant (ms) */
#define STP_MIN_TIMESTEP 0.1f        /**< Minimum timestep for numerical stability (ms) */

//=============================================================================
// Parameter Structures
//=============================================================================

/**
 * @brief Short-term plasticity configuration
 *
 * WHAT: Configuration for STP module including bio-async
 * WHY: Centralized configuration with async communication
 * HOW: Combines plasticity parameters with bio-async settings
 */
typedef struct {
    float U;                /**< Baseline utilization/release probability */
    float tau_D;            /**< Depression recovery time constant (ms) */
    float tau_F;            /**< Facilitation decay time constant (ms) */
    bool enable_bio_async;  /**< Enable bio-async communication */
} stp_config_t;

/**
 * @brief Short-term plasticity parameters (legacy compatibility)
 *
 * WHAT: Parameters for Tsodyks-Markram STP model
 * WHY: Complete specification of depression/facilitation dynamics
 * HOW: Maps to biological vesicle release and calcium dynamics
 *
 * INVARIANTS:
 * - 0 < U ≤ 1 (release probability)
 * - τ_D > 0 (recovery time)
 * - τ_F > 0 (facilitation decay time)
 */
typedef struct {
    float U;      /**< Baseline utilization/release probability */
    float tau_D;  /**< Depression recovery time constant (ms) */
    float tau_F;  /**< Facilitation decay time constant (ms) */
} stp_params_t;

/**
 * @brief Short-term plasticity state
 *
 * WHAT: Dynamic state variables for one synapse
 * WHY: Tracks depletion and facilitation over time
 * HOW: Updated on each spike and continuously decayed
 *
 * INVARIANTS:
 * - 0 ≤ x ≤ 1 (fraction of available resources)
 * - 0 ≤ u ≤ 1 (utilization probability)
 */
typedef struct {
    float x;                /**< Available synaptic resources (0-1) */
    float u;                /**< Current utilization probability (0-1) */
    uint64_t last_update;   /**< Last update timestamp (ms) */
    stp_params_t params;    /**< STP parameters */
} stp_state_t;

/**
 * @brief STP preset types
 *
 * WHAT: Named presets for common synapse types
 * WHY: Easy access to biologically validated parameters
 * HOW: Based on experimental measurements
 */
typedef enum {
    STP_PRESET_DEPRESSING,     /**< Predominantly depressing synapse */
    STP_PRESET_FACILITATING,   /**< Predominantly facilitating synapse */
    STP_PRESET_MIXED,          /**< Balanced depression and facilitation */
    STP_PRESET_FAST_DEPRESSING,/**< Fast-recovering depressing synapse */
    STP_PRESET_SLOW_DEPRESSING,/**< Slow-recovering depressing synapse */
    STP_PRESET_NONE            /**< No STP (static synapse) */
} stp_preset_t;

//=============================================================================
// Core STP Functions
//=============================================================================

/**
 * @brief Initialize STP state
 *
 * WHAT: Sets initial state to resting values
 * WHY: Starts synapse at equilibrium (no recent activity)
 * HOW: x=1 (full resources), u=U (baseline release)
 *
 * COMPLEXITY: O(1)
 *
 * @param state STP state to initialize
 * @param params STP parameters (NULL = use defaults)
 * @param timestamp Initial timestamp (ms)
 */
void stp_init(stp_state_t* state, const stp_params_t* params, uint64_t timestamp);

/**
 * @brief Update STP state for continuous decay
 *
 * WHAT: Applies exponential decay of x and u between spikes
 * WHY: Resources recover, facilitation decays with time
 * HOW: Euler integration of dx/dt and du/dt
 *
 * COMPLEXITY: O(1)
 * STABILITY: Stable for dt < min(τ_D, τ_F)/10
 *
 * @param state STP state
 * @param timestamp Current time (ms)
 */
void stp_update(stp_state_t* state, uint64_t timestamp);

/**
 * @brief Process presynaptic spike
 *
 * WHAT: Updates u and x when presynaptic neuron fires
 * WHY: Implements facilitation increase and resource depletion
 * HOW: u ← u + U(1-u), then x ← x - ux
 *
 * COMPLEXITY: O(1)
 *
 * @param state STP state
 * @param timestamp Spike time (ms)
 */
void stp_process_spike(stp_state_t* state, uint64_t timestamp);

/**
 * @brief Compute effective synaptic strength
 *
 * WHAT: Returns modulation factor for synaptic weight
 * WHY: Determines actual postsynaptic current amplitude
 * HOW: Returns u × x (facilitation × available resources)
 *
 * COMPLEXITY: O(1)
 *
 * @param state STP state
 * @return Modulation factor (0-1), multiply with base weight
 */
float stp_get_modulation(const stp_state_t* state);

/**
 * @brief Reset STP state to resting values
 *
 * WHAT: Returns synapse to equilibrium
 * WHY: Network reset or testing
 * HOW: x=1, u=U
 *
 * COMPLEXITY: O(1)
 *
 * @param state STP state
 * @param timestamp Reset time (ms)
 */
void stp_reset(stp_state_t* state, uint64_t timestamp);

//=============================================================================
// Parameter Presets
//=============================================================================

/**
 * @brief Get parameters for preset synapse type
 *
 * WHAT: Returns pre-configured (U, τ_D, τ_F) parameters
 * WHY: Convenient access to validated parameter sets
 * HOW: Looks up preset in static table
 *
 * COMPLEXITY: O(1)
 *
 * @param preset Desired synapse type
 * @return Parameter structure
 */
stp_params_t stp_get_preset_params(stp_preset_t preset);

/**
 * @brief Create custom parameter set
 *
 * WHAT: Constructs parameters from individual values
 * WHY: Allows parameter space exploration
 * HOW: Validates and packages parameters
 *
 * COMPLEXITY: O(1)
 *
 * @param U Release probability (0-1)
 * @param tau_D Depression time constant (ms, > 0)
 * @param tau_F Facilitation time constant (ms, > 0)
 * @return Parameter structure
 */
stp_params_t stp_create_params(float U, float tau_D, float tau_F);

/**
 * @brief Get preset name as string
 *
 * WHAT: Returns human-readable name for preset
 * WHY: Debugging and logging
 * HOW: String lookup table
 *
 * COMPLEXITY: O(1)
 *
 * @param preset Preset type
 * @return Preset name (never NULL)
 */
const char* stp_get_preset_name(stp_preset_t preset);

/**
 * @brief Get preset description
 *
 * WHAT: Returns detailed description of synapse type
 * WHY: Educational purposes
 * HOW: String lookup table
 *
 * COMPLEXITY: O(1)
 *
 * @param preset Preset type
 * @return Description string (never NULL)
 */
const char* stp_get_preset_description(stp_preset_t preset);

//=============================================================================
// Analysis Functions
//=============================================================================

/**
 * @brief Compute steady-state values for periodic input
 *
 * WHAT: Calculates equilibrium x and u for given spike frequency
 * WHY: Predicts synapse behavior for repetitive stimulation
 * HOW: Solves fixed-point equations
 *
 * COMPLEXITY: O(1)
 *
 * @param params STP parameters
 * @param frequency Input spike frequency (Hz)
 * @param[out] x_ss Steady-state resource level
 * @param[out] u_ss Steady-state facilitation level
 */
void stp_compute_steady_state(const stp_params_t* params, float frequency, float* x_ss,
                               float* u_ss);

/**
 * @brief Determine synapse classification
 *
 * WHAT: Classifies synapse as depressing, facilitating, or mixed
 * WHY: Categorization for analysis and visualization
 * HOW: Compares relative strength of depression vs facilitation
 *
 * COMPLEXITY: O(1)
 *
 * @param params STP parameters
 * @return Classification (depressing=0, facilitating=1, mixed=2)
 */
int stp_classify_synapse(const stp_params_t* params);

//=============================================================================
// Module Management
//=============================================================================

/**
 * @brief Initialize STP module with bio-async support
 *
 * WHAT: Sets up bio-async communication for STP module
 * WHY: Enables async event-driven STP updates
 * HOW: Registers with bio-router and initializes module state
 *
 * @param config Module configuration (NULL = no bio-async)
 * @return true on success, false on failure
 */
bool stp_module_init(const stp_config_t* config);

/**
 * @brief Destroy STP module and cleanup resources
 *
 * WHAT: Cleans up bio-async resources and module state
 * WHY: Proper resource cleanup on shutdown
 * HOW: Unregisters from bio-router and resets state
 */
void stp_module_destroy(void);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_STP_H
