//=============================================================================
// nimcp_izhikevich.h - Izhikevich Neuron Model
//=============================================================================
/**
 * @file nimcp_izhikevich.h
 * @brief Izhikevich simple model of spiking neurons
 *
 * BIOLOGICAL BACKGROUND:
 * The Izhikevich model (2003) reproduces 20+ firing patterns with just 2 ODEs:
 *   v' = 0.04v² + 5v + 140 - u + I
 *   u' = a(bv - u)
 *   if v ≥ 30mV: v ← c, u ← u + d
 *
 * PARAMETERS (a, b, c, d):
 * - a: recovery time scale (0.001-0.1)
 * - b: sensitivity of u to v (0.2-0.25)
 * - c: after-spike reset value of v (-65 to -50 mV)
 * - d: after-spike reset of u (2-8)
 *
 * FIRING PATTERNS SUPPORTED:
 * - Regular Spiking (RS): Cortical excitatory neurons
 * - Intrinsically Bursting (IB): Chattering cells
 * - Chattering (CH): Fast rhythmic bursting
 * - Fast Spiking (FS): Cortical inhibitory neurons
 * - Low-Threshold Spiking (LTS): Interneurons
 * - Resonator (RZ): Subthreshold oscillations
 * - And 14+ others...
 *
 * ADVANTAGES OVER LIF:
 * - Richer dynamics (adaptation, bursting, resonance)
 * - Similar computational cost to LIF
 * - Based on bifurcation theory (mathematically grounded)
 * - Parameters map to biological ion channels
 *
 * REFERENCE:
 * Izhikevich, E. M. (2003). Simple model of spiking neurons.
 * IEEE Transactions on Neural Networks, 14(6), 1569-1572.
 *
 * @author NIMCP Development Team
 * @date 2025-11-06
 */

#ifndef NIMCP_IZHIKEVICH_H
#define NIMCP_IZHIKEVICH_H

#include "core/neuron_models/nimcp_neuron_model.h"
#include "utils/numerical/nimcp_integration.h"  // For integration_method_t

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

/**
 * @brief Spike detection threshold
 *
 * WHAT: Voltage threshold for spike detection in millivolts
 * WHY: Standard value from Izhikevich 2003 paper
 * HOW: When v >= SPIKE_THRESHOLD, spike is detected
 */
#define IZHIKEVICH_SPIKE_THRESHOLD 30.0f

/**
 * @brief Default resting potential
 *
 * WHAT: Typical resting membrane potential in millivolts
 * WHY: Biologically realistic starting point
 */
#define IZHIKEVICH_RESTING_POTENTIAL -65.0f

//=============================================================================
// Parameter Structures
//=============================================================================

/**
 * @brief Izhikevich model parameters
 *
 * WHAT: Four-parameter specification for Izhikevich dynamics
 * WHY: Complete characterization of neuron behavior
 * HOW: Maps to biological properties (adaptation, threshold, reset)
 *
 * INVARIANTS:
 * - 0.001 <= a <= 0.1 (typical range)
 * - 0.1 <= b <= 0.3 (typical range)
 * - -65.0 <= c <= -50.0 (typical range for reset voltage)
 * - 0.0 <= d <= 10.0 (typical range for reset recovery)
 */
typedef struct {
    float a;  /**< Recovery time scale (smaller = slower recovery) */
    float b;  /**< Sensitivity of recovery variable to subthreshold fluctuations */
    float c;  /**< After-spike reset value of voltage (mV) */
    float d;  /**< After-spike reset of recovery variable */
} izhikevich_params_t;

//=============================================================================
// Preset Parameter Sets - From Izhikevich 2003 Paper
//=============================================================================

/**
 * @brief Parameter preset enumeration
 *
 * WHAT: Named presets for common neuron types
 * WHY: Easy access to validated parameter combinations
 * HOW: Each enum maps to specific (a,b,c,d) values
 */
typedef enum {
    IZHI_PRESET_REGULAR_SPIKING,        /**< RS: Typical cortical pyramidal cell */
    IZHI_PRESET_INTRINSICALLY_BURSTING, /**< IB: Bursting cortical neuron */
    IZHI_PRESET_CHATTERING,              /**< CH: Fast rhythmic bursting */
    IZHI_PRESET_FAST_SPIKING,            /**< FS: Cortical inhibitory interneuron */
    IZHI_PRESET_LOW_THRESHOLD_SPIKING,  /**< LTS: Another interneuron type */
    IZHI_PRESET_THALAMO_CORTICAL,       /**< TC: Thalamic relay neuron */
    IZHI_PRESET_RESONATOR,               /**< RZ: Subthreshold oscillator */
    IZHI_PRESET_CUSTOM                   /**< Custom: User-defined parameters */
} izhikevich_preset_t;

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Get Izhikevich model vtable
 *
 * WHAT: Returns virtual function table for Izhikevich model
 * WHY: Factory method for creating Izhikevich neurons
 * HOW: Returns pointer to static const vtable structure
 *
 * COMPLEXITY: O(1)
 *
 * @return Pointer to Izhikevich vtable (never NULL)
 */
const neuron_model_vtable_t* izhikevich_get_vtable(void);

/**
 * @brief Get parameters for a preset neuron type
 *
 * WHAT: Returns pre-configured (a,b,c,d) parameters
 * WHY: Convenient access to validated parameter sets
 * HOW: Looks up preset in static table
 *
 * COMPLEXITY: O(1)
 *
 * @param preset Desired neuron type preset
 * @return Parameter structure with (a,b,c,d) values
 */
izhikevich_params_t izhikevich_get_preset_params(izhikevich_preset_t preset);

/**
 * @brief Create custom parameter set
 *
 * WHAT: Constructs parameter structure from individual values
 * WHY: Allows exploration of parameter space
 * HOW: Validates and packages parameters
 *
 * COMPLEXITY: O(1)
 *
 * @param a Recovery time scale (0.001-0.1)
 * @param b Sensitivity parameter (0.1-0.3)
 * @param c After-spike voltage reset (-65 to -50 mV)
 * @param d After-spike recovery reset (0-10)
 * @return Parameter structure
 */
izhikevich_params_t izhikevich_create_params(float a, float b, float c, float d);

/**
 * @brief Get preset name as string
 *
 * WHAT: Returns human-readable name for preset type
 * WHY: Debugging, logging, and UI display
 * HOW: String lookup table
 *
 * COMPLEXITY: O(1)
 *
 * @param preset Preset type
 * @return Preset name string (never NULL)
 */
const char* izhikevich_get_preset_name(izhikevich_preset_t preset);

/**
 * @brief Get preset description
 *
 * WHAT: Returns detailed description of neuron type
 * WHY: Educational and documentation purposes
 * HOW: String lookup table
 *
 * COMPLEXITY: O(1)
 *
 * @param preset Preset type
 * @return Description string (never NULL)
 */
const char* izhikevich_get_preset_description(izhikevich_preset_t preset);

/**
 * @brief Set ODE integration method for Izhikevich neuron
 *
 * WHAT: Configures numerical integration algorithm (Euler, RK4, etc.)
 * WHY: Enables accuracy/speed tradeoff control from brain configuration
 * HOW: Writes integration_method field in neuron model state
 *
 * PART A1.1: RK4 Integration
 * - INTEGRATION_EULER: Fastest, least accurate (default)
 * - INTEGRATION_RK4: 4x slower, 10-1000x more accurate
 *
 * USAGE:
 * @code
 *   neuron_model_state_t neuron = neuron_model_create(izhikevich_get_vtable(), &params);
 *   izhikevich_set_integration_method(neuron, INTEGRATION_RK4);
 * @endcode
 *
 * COMPLEXITY: O(1)
 *
 * @param state Neuron model state
 * @param method Integration method to use
 */
void izhikevich_set_integration_method(neuron_model_state_t state, integration_method_t method);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_IZHIKEVICH_H
