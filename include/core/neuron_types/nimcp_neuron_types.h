/**
 * @file nimcp_neuron_types.h
 * @brief Phase 8.7: Specialized neuron type system with biological diversity
 *
 * DESIGN PHILOSOPHY:
 * This system models the rich diversity of neuron types found in biological neural
 * circuits. Each type has specialized computation matched to its biological role.
 *
 * BIOLOGICAL MOTIVATION:
 * Real brains aren't uniform - they contain dozens of distinct neuron types:
 * - V1 simple/complex cells for visual processing (Hubel & Wiesel, 1962)
 * - A1 frequency-tuned neurons for auditory processing (Schreiner et al., 2000)
 * - Metacognitive neurons for uncertainty estimation (Fleming & Dolan, 2012)
 * - Executive control neurons for goal-directed behavior (Miller & Cohen, 2001)
 *
 * REFERENCES:
 * [1] Hubel & Wiesel (1962) "Receptive fields in cat striate cortex"
 * [2] Izhikevich (2003) "Simple model of spiking neurons"
 * [3] Schreiner et al. (2000) "Functional architecture of auditory cortex"
 * [4] Fleming & Dolan (2012) "The neural basis of metacognitive ability"
 *
 * @author NIMCP Phase 8.7
 * @date 2025-11-08
 */

#ifndef NIMCP_NEURON_TYPES_H
#define NIMCP_NEURON_TYPES_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// Forward declarations to avoid circular dependencies
struct izhikevich_params_t_forward;  // Will use existing definition from nimcp_izhikevich.h

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// NEURON TYPE ENUMERATION - Phase 8.7
// ============================================================================

/**
 * @brief Specialized neuron types for task-specific processing
 *
 * DESIGN: Each type implements biologically-motivated computation:
 * - Generic types: Standard neuron models (LIF, Izhikevich)
 * - Sensory types: Specialized receptive fields and tuning
 * - Cognitive types: High-level processing (metacognition, executive control)
 */
typedef enum {
    // ========== BACKWARD COMPATIBILITY (0-1) ==========
    NEURON_EXCITATORY = 0,           /**< Generic excitatory neuron (backward compat) */
    NEURON_INHIBITORY = 1,           /**< Generic inhibitory neuron (backward compat) */

    // ========== GENERIC NEURON MODELS (2-99) ==========
    NEURON_GENERIC_LIF = 2,          /**< Leaky Integrate-and-Fire neuron */
    NEURON_GENERIC_IZHIKEVICH = 3,   /**< Izhikevich neuron model */

    // ========== VISUAL CORTEX (V1) NEURONS (100-199) ==========
    /**
     * V1 Simple Cell: Oriented edge detector with Gabor-like receptive field
     *
     * BIOLOGICAL MOTIVATION:
     * Simple cells in primary visual cortex (V1) respond to edges at specific
     * orientations and positions. They have phase-dependent responses.
     *
     * COMPUTATION:
     * - Gabor filter convolution over input
     * - Orientation selectivity via cosine tuning
     * - Spatial frequency selectivity
     *
     * REFERENCE: Hubel & Wiesel (1962)
     */
    NEURON_V1_SIMPLE_CELL = 100,
    NEURON_VISUAL_EDGE = 100,  /**< Alias for V1_SIMPLE_CELL (backward compat) */

    /**
     * V1 Complex Cell: Phase-invariant edge detector
     *
     * BIOLOGICAL MOTIVATION:
     * Complex cells pool over multiple simple cells with different phases,
     * giving position-invariant edge detection.
     *
     * COMPUTATION:
     * - Pool over simple cell responses
     * - Energy model: sqrt(even^2 + odd^2)
     * - Directional selectivity
     *
     * REFERENCE: Hubel & Wiesel (1962)
     */
    NEURON_V1_COMPLEX_CELL = 101,
    NEURON_VISUAL_ORIENTATION = 102,  /**< Orientation-selective neuron */
    NEURON_VISUAL_DIRECTION = 103,    /**< Direction-selective neuron (MT/V5) */

    // Pyramidal neuron types
    NEURON_PYRAMIDAL_L23 = 150,       /**< Layer 2/3 pyramidal neuron */
    NEURON_PYRAMIDAL_L5_THICK = 151,  /**< Layer 5 thick-tufted pyramidal */
    NEURON_PYRAMIDAL_L6 = 152,        /**< Layer 6 pyramidal neuron */

    // ========== AUDITORY CORTEX (A1) NEURONS (200-299) ==========
    /**
     * A1 Frequency-Tuned Neuron: Tonotopically organized spectral filter
     *
     * BIOLOGICAL MOTIVATION:
     * A1 neurons are organized tonotopically - each responds to a specific
     * frequency band, creating a spatial frequency map.
     *
     * COMPUTATION:
     * - Bandpass filtering centered at preferred frequency
     * - Quality factor (Q) determines tuning sharpness
     * - Temporal integration window
     *
     * REFERENCE: Schreiner et al. (2000)
     */
    NEURON_A1_FREQUENCY_TUNED = 200,
    NEURON_AUDITORY_FREQUENCY = 200,  /**< Alias (backward compat) */

    /**
     * A1 Coincidence Detector: Temporal integration for binaural processing
     *
     * BIOLOGICAL MOTIVATION:
     * Coincidence detection neurons in auditory brainstem and cortex compute
     * interaural time differences for sound localization.
     *
     * COMPUTATION:
     * - Short integration window (~1ms)
     * - High temporal precision
     * - Cross-correlation of binaural inputs
     *
     * REFERENCE: Jeffress (1948) place theory
     */
    NEURON_A1_COINCIDENCE_DETECTOR = 201,
    NEURON_AUDITORY_ONSET = 202,      /**< Onset detector neuron */

    // ========== MOTOR NEURONS (250-299) ==========
    NEURON_MOTOR_ALPHA = 250,         /**< Alpha motoneuron */
    NEURON_MOTOR_PATTERN_GEN = 251,   /**< Central pattern generator */

    // ========== COGNITIVE/METACOGNITIVE NEURONS (300-399) ==========
    /**
     * Metacognitive Neuron: Uncertainty estimation and confidence monitoring
     *
     * BIOLOGICAL MOTIVATION:
     * Prefrontal cortex contains neurons that track decision confidence and
     * uncertainty, enabling metacognitive awareness.
     *
     * COMPUTATION:
     * - Estimate uncertainty via input variance
     * - Confidence = 1 / (1 + uncertainty)
     * - Second-order monitoring of first-order processing
     *
     * REFERENCE: Fleming & Dolan (2012)
     * COMPLEXITY: O(n) where n = number of inputs
     */
    NEURON_METACOGNITIVE = 300,

    /**
     * Executive Control Neuron: Goal-directed activity with top-down modulation
     *
     * BIOLOGICAL MOTIVATION:
     * Dorsolateral prefrontal cortex contains neurons that maintain goal
     * representations and modulate processing in other areas.
     *
     * COMPUTATION:
     * - Maintain activity during delay periods
     * - Modulate gain of target neurons
     * - Working memory via sustained firing
     *
     * REFERENCE: Miller & Cohen (2001)
     * COMPLEXITY: O(1) for self-maintenance, O(n) for modulation
     */
    NEURON_EXECUTIVE_CONTROL = 301,

    // ========== NEURAL LOGIC GATES (650-699) ==========
    /**
     * Logic AND Gate Neuron: Conjunction operation (∧)
     *
     * BIOLOGICAL MOTIVATION:
     * Coincidence detector neurons in auditory brainstem and cortex fire only
     * when multiple inputs arrive simultaneously. This implements AND logic.
     *
     * COMPUTATION:
     * - Fires only if ALL inputs exceed threshold
     * - Threshold = sum of expected input weights
     * - Integration window: ~1-5ms
     *
     * EXAMPLE:
     * Input A=1, B=1 → Output=1
     * Input A=1, B=0 → Output=0
     *
     * REFERENCE: Coincidence detection (Jeffress, 1948)
     * COMPLEXITY: O(n) where n = number of inputs
     */
    NEURON_LOGIC_AND = 650,

    /**
     * Logic OR Gate Neuron: Disjunction operation (∨)
     *
     * BIOLOGICAL MOTIVATION:
     * Neurons that respond to any of multiple stimuli implement OR logic.
     * Common in sensory integration pathways.
     *
     * COMPUTATION:
     * - Fires if ANY input exceeds threshold
     * - Low threshold relative to input weights
     * - Fast response (~1ms)
     *
     * EXAMPLE:
     * Input A=1, B=0 → Output=1
     * Input A=0, B=0 → Output=0
     *
     * COMPLEXITY: O(n) where n = number of inputs
     */
    NEURON_LOGIC_OR = 651,

    /**
     * Logic NOT Gate Neuron: Negation operation (¬)
     *
     * BIOLOGICAL MOTIVATION:
     * Inhibitory neurons implement NOT logic by suppressing their targets.
     * Fundamental for contrast enhancement and normalization.
     *
     * COMPUTATION:
     * - Baseline firing rate when no input
     * - Suppressed (output=0) when input present
     * - Fast inhibition (~1ms)
     *
     * EXAMPLE:
     * Input A=1 → Output=0
     * Input A=0 → Output=1
     *
     * COMPLEXITY: O(1)
     */
    NEURON_LOGIC_NOT = 652,

    /**
     * Logic XOR Gate Neuron: Exclusive OR operation (⊕)
     *
     * BIOLOGICAL MOTIVATION:
     * Neurons with balanced excitation/inhibition can detect pattern differences.
     * Used in change detection and novelty circuits.
     *
     * COMPUTATION:
     * - Fires if inputs differ (one high, one low)
     * - Inhibited if both inputs same
     * - Requires excitatory + inhibitory integration
     *
     * EXAMPLE:
     * Input A=1, B=0 → Output=1
     * Input A=1, B=1 → Output=0
     *
     * COMPLEXITY: O(n) where n = number of inputs
     */
    NEURON_LOGIC_XOR = 653,

    /**
     * Variable Binding Neuron: Symbolic variable pointer
     *
     * BIOLOGICAL MOTIVATION:
     * Pointer neurons (Eliasmith, 2013) bind symbolic variables to neural
     * activation patterns, enabling compositional reasoning.
     *
     * COMPUTATION:
     * - Associates variable ID with activation pattern
     * - Binding strength = confidence [0,1]
     * - Supports unification (pattern matching)
     *
     * EXAMPLE:
     * Bind "X" to pattern [0.8, 0.2, 0.5, ...]
     * Query "X" → Returns bound pattern
     *
     * REFERENCE: Eliasmith (2013) "How to build a brain"
     * COMPLEXITY: O(d) where d = pattern dimensionality
     */
    NEURON_LOGIC_VARIABLE = 654,

    /**
     * Logic IMPLIES Gate Neuron: Implication operation (→)
     *
     * BIOLOGICAL MOTIVATION:
     * Conditional responses: neuron fires if consequent follows antecedent.
     * Models if-then reasoning in prefrontal circuits.
     *
     * COMPUTATION:
     * - Fires if: A=0 OR (A=1 AND B=1)
     * - Implements ¬A ∨ B (material implication)
     * - Forward inference: A → B
     *
     * EXAMPLE:
     * A=1, B=1 → Output=1 (true)
     * A=1, B=0 → Output=0 (false)
     * A=0, B=* → Output=1 (vacuously true)
     *
     * COMPLEXITY: O(1)
     */
    NEURON_LOGIC_IMPLIES = 655,

    NEURON_TYPE_COUNT  /**< Total number of neuron types */
} neuron_type_t;

// Backward compatibility alias
typedef neuron_type_t neuron_type_extended_t;

// ============================================================================
// TYPE-SPECIFIC PARAMETER STRUCTURES
// ============================================================================

/**
 * @brief Parameters for V1 simple cell (Gabor filter)
 */
typedef struct {
    float orientation;          /**< Preferred orientation in degrees [0, 180] */
    float spatial_frequency;    /**< Spatial frequency in cycles/degree */
    float phase;                /**< Phase offset in radians [0, 2π] */
    float aspect_ratio;         /**< Aspect ratio of Gabor envelope (typically 0.5) */
    float sigma;                /**< Gaussian envelope width */
    bool on_center;             /**< ON-center vs OFF-center cell */
} v1_simple_cell_params_t;

/**
 * @brief Parameters for V1 complex cell
 */
typedef struct {
    float orientation;          /**< Preferred orientation in degrees [0, 180] */
    float direction_selectivity;/**< Direction selectivity index [0, 1] */
    float surround_suppression; /**< Strength of surround suppression [0, 1] */
    float pooling_size;         /**< Spatial pooling size in degrees */
} v1_complex_cell_params_t;

/**
 * @brief Parameters for A1 frequency-tuned neuron
 */
typedef struct {
    float center_frequency;     /**< Center frequency in Hz */
    float q_factor;             /**< Quality factor (center_freq / bandwidth) */
    float bandwidth;            /**< Bandwidth in Hz (derived from Q) */
    float integration_window;   /**< Temporal integration window in ms */
    float adaptation_rate;      /**< Rate of adaptation to sustained input */
} a1_frequency_params_t;

/**
 * @brief Parameters for A1 coincidence detector
 */
typedef struct {
    float integration_window;   /**< Integration time constant in ms (~1ms) */
    float temporal_precision;   /**< Temporal precision in µs (~100µs) */
    float threshold;            /**< Coincidence threshold */
    float decay_rate;           /**< Decay rate of integration */
} a1_coincidence_params_t;

/**
 * @brief Parameters for metacognitive neuron
 */
typedef struct {
    float confidence_threshold; /**< Minimum confidence for high-confidence output */
    float uncertainty_window;   /**< Time window for uncertainty estimation (ms) */
    float uncertainty_beta;     /**< Beta parameter for uncertainty calculation */
    uint32_t history_size;      /**< Number of inputs to track for variance */
} metacognitive_params_t;

/**
 * @brief Parameters for executive control neuron
 */
typedef struct {
    float goal_maintenance;     /**< Strength of goal maintenance [0, 1] */
    float modulation_strength;  /**< Strength of top-down modulation */
    float decay_rate;           /**< Decay rate during maintenance */
    float threshold_boost;      /**< Threshold boost during goal-directed state */
    bool delay_activity;        /**< Maintain activity during delays */
} executive_control_params_t;

// ============================================================================
// NEURAL LOGIC GATE PARAMETERS
// ============================================================================

/**
 * @brief Parameters for logic AND gate neuron
 *
 * AND gate fires only if ALL inputs are active (coincidence detection).
 */
typedef struct {
    uint32_t num_inputs;        /**< Number of input connections (typically 2-4) */
    float threshold;            /**< Firing threshold (typically num_inputs * 0.9) */
    float integration_window;   /**< Temporal integration window in ms (1-5ms) */
    float input_weight;         /**< Expected weight per input (default 1.0) */
    bool require_simultaneous;  /**< Require inputs within integration window */
} logic_and_params_t;

/**
 * @brief Parameters for logic OR gate neuron
 *
 * OR gate fires if ANY input is active (low threshold).
 */
typedef struct {
    uint32_t num_inputs;        /**< Number of input connections (typically 2-4) */
    float threshold;            /**< Firing threshold (typically 0.5 * input_weight) */
    float input_weight;         /**< Expected weight per input (default 1.0) */
    float refractory_period;    /**< Refractory period in ms (0.5-2ms) */
} logic_or_params_t;

/**
 * @brief Parameters for logic NOT gate neuron
 *
 * NOT gate fires when input is ABSENT (inhibitory logic).
 */
typedef struct {
    float baseline_rate;        /**< Baseline firing rate when no input (Hz, default 10) */
    float inhibition_strength;  /**< Strength of inhibition [0,1] (default 1.0) */
    float recovery_time;        /**< Time to recover from inhibition (ms, default 5) */
    float threshold;            /**< Threshold for baseline firing (default 0.1) */
} logic_not_params_t;

/**
 * @brief Parameters for logic XOR gate neuron
 *
 * XOR gate fires if inputs DIFFER (one high, one low).
 */
typedef struct {
    uint32_t num_inputs;        /**< Number of inputs (must be 2 for XOR) */
    float excitatory_weight;    /**< Weight for excitatory inputs (default 1.0) */
    float inhibitory_weight;    /**< Weight for inhibitory inputs (default -1.0) */
    float threshold;            /**< Firing threshold (default 0.5) */
    float balance_tolerance;    /**< Tolerance for balanced inputs (default 0.1) */
} logic_xor_params_t;

/**
 * @brief Parameters for variable binding neuron
 *
 * Binds symbolic variables to neural activation patterns.
 */
typedef struct {
    uint32_t variable_id;       /**< Unique variable identifier (e.g., hash of "X") */
    uint32_t pattern_dim;       /**< Dimensionality of bound pattern (default 64) */
    float binding_strength;     /**< Binding confidence [0,1] (default 1.0) */
    float decay_rate;           /**< Binding decay rate (default 0.001/ms) */
    float* bound_pattern;       /**< Current bound activation pattern (heap allocated) */
    bool is_bound;              /**< Whether variable is currently bound */
} logic_variable_params_t;

/**
 * @brief Parameters for logic IMPLIES gate neuron
 *
 * IMPLIES gate fires if antecedent implies consequent (A → B).
 */
typedef struct {
    float antecedent_threshold; /**< Threshold for antecedent activation (default 0.8) */
    float consequent_threshold; /**< Threshold for consequent activation (default 0.8) */
    float vacuous_truth_rate;   /**< Firing rate when antecedent false (default 0.1) */
    float implication_window;   /**< Temporal window for A→B (ms, default 10) */
} logic_implies_params_t;

/**
 * @brief Generic LIF parameters
 */
typedef struct {
    float tau_membrane;         /**< Membrane time constant (ms) */
    float rest_potential;       /**< Resting potential (mV) */
    float threshold;            /**< Spike threshold (mV) */
    float reset_potential;      /**< Reset potential after spike (mV) */
    float refractory_period;    /**< Refractory period (ms) */
} lif_params_t;

/**
 * @brief Izhikevich neuron parameters (Phase 8.7)
 *
 * NOTE: This is a local definition for the neuron_type_params_t union.
 * The neuron_models system has its own izhikevich_params_t which is identical.
 * We define it here too to avoid circular dependencies.
 */
typedef struct {
    float a;  /**< Recovery time scale parameter */
    float b;  /**< Sensitivity of recovery to subthreshold fluctuations */
    float c;  /**< After-spike reset value of membrane potential (mV) */
    float d;  /**< After-spike reset of recovery variable */
} izh_params_t;

// ============================================================================
// UNIFIED PARAMETER UNION
// ============================================================================

/**
 * @brief Union of all type-specific parameters
 *
 * DESIGN: Store in neuron structure's type_params field
 * SIZE: Largest member determines size (~64 bytes)
 */
typedef union {
    lif_params_t lif;
    izh_params_t izhikevich;  // Local definition, compatible with neuron_models/izhikevich_params_t
    v1_simple_cell_params_t v1_simple;
    v1_complex_cell_params_t v1_complex;
    a1_frequency_params_t a1_frequency;
    a1_coincidence_params_t a1_coincidence;
    metacognitive_params_t metacognitive;
    executive_control_params_t executive;
} neuron_type_params_t;

// ============================================================================
// TYPE-SPECIFIC COMPUTE FUNCTIONS
// ============================================================================

/**
 * @brief Compute output for generic LIF neuron
 *
 * WHAT: Implements leaky integrate-and-fire dynamics
 * WHY: Most common computational neuron model
 * HOW: dV/dt = (E_L - V + I)/tau_m
 *
 * @param params LIF parameters
 * @param input Input current
 * @param state Current membrane potential
 * @param dt Time step in ms
 * @return New membrane potential
 *
 * COMPLEXITY: O(1)
 */
float compute_lif_neuron(const lif_params_t* params, float input,
                         float state, float dt);

/**
 * @brief Compute output for Izhikevich neuron
 *
 * WHAT: Implements Izhikevich simple neuron model
 * WHY: Reproduces 20+ cortical neuron firing patterns
 * HOW: dv/dt = 0.04v^2 + 5v + 140 - u + I; du/dt = a(bv - u)
 *
 * @param params Izhikevich parameters
 * @param input Input current
 * @param v Membrane potential
 * @param u Recovery variable
 * @param dt Time step in ms
 * @param out_v Output membrane potential
 * @param out_u Output recovery variable
 *
 * REFERENCE: Izhikevich (2003)
 * COMPLEXITY: O(1)
 */
void compute_izhikevich_neuron(const izh_params_t* params, float input,
                               float v, float u, float dt,
                               float* out_v, float* out_u);

/**
 * @brief Compute V1 simple cell response (Gabor filter)
 *
 * WHAT: Oriented edge detection with spatial frequency selectivity
 * WHY: Models V1 simple cells (Hubel & Wiesel, 1962)
 * HOW: Gabor filter = Gaussian envelope × sinusoidal carrier
 *      G(x,y) = exp(-(x'^2 + γ^2*y'^2)/(2σ^2)) * cos(2πf*x' + φ)
 *      where (x',y') are rotated coordinates
 *
 * @param params Simple cell parameters
 * @param inputs 2D input array (flattened)
 * @param input_width Width of input
 * @param input_height Height of input
 * @param center_x Receptive field center x
 * @param center_y Receptive field center y
 * @return Gabor filter response
 *
 * COMPLEXITY: O(w*h) where w,h are receptive field size
 */
float compute_v1_simple_cell(const v1_simple_cell_params_t* params,
                             const float* inputs,
                             uint32_t input_width, uint32_t input_height,
                             float center_x, float center_y);

/**
 * @brief Compute V1 complex cell response (energy model)
 *
 * WHAT: Phase-invariant edge detection via pooling over simple cells
 * WHY: Models V1 complex cells (Hubel & Wiesel, 1962)
 * HOW: Energy model: sqrt(even^2 + odd^2) where even/odd are 90° phase shifted
 *
 * @param params Complex cell parameters
 * @param simple_cell_responses Array of simple cell responses (different phases)
 * @param num_simple_cells Number of simple cells to pool
 * @return Complex cell response
 *
 * COMPLEXITY: O(n) where n = num_simple_cells
 */
float compute_v1_complex_cell(const v1_complex_cell_params_t* params,
                              const float* simple_cell_responses,
                              uint32_t num_simple_cells);

/**
 * @brief Compute A1 frequency-tuned neuron response
 *
 * WHAT: Bandpass filtering for tonotopic organization
 * WHY: Models A1 frequency-selective neurons
 * HOW: Apply bandpass filter centered at preferred frequency
 *      Q = center_freq / bandwidth (quality factor)
 *      Higher Q = sharper tuning
 *
 * @param params Frequency tuning parameters
 * @param audio_input Audio input signal
 * @param signal_length Length of audio signal
 * @param sample_rate Sample rate in Hz
 * @param timestamp Current time in µs
 * @return Filtered audio response
 *
 * COMPLEXITY: O(n) where n = signal_length
 */
float compute_a1_frequency_tuned(const a1_frequency_params_t* params,
                                 const float* audio_input,
                                 uint32_t signal_length,
                                 float sample_rate,
                                 uint64_t timestamp);

/**
 * @brief Compute A1 coincidence detector response
 *
 * WHAT: Temporal integration for binaural processing
 * WHY: Sound localization via interaural time difference
 * HOW: Cross-correlate left/right inputs with high temporal precision
 *
 * @param params Coincidence detector parameters
 * @param left_input Left ear input
 * @param right_input Right ear input
 * @param num_samples Number of samples
 * @param timestamp Current time in µs
 * @return Coincidence detection strength
 *
 * COMPLEXITY: O(n) where n = num_samples in integration window
 */
float compute_a1_coincidence_detector(const a1_coincidence_params_t* params,
                                      const float* left_input,
                                      const float* right_input,
                                      uint32_t num_samples,
                                      uint64_t timestamp);

/**
 * @brief Compute metacognitive neuron output (uncertainty estimation)
 *
 * WHAT: Estimate confidence/uncertainty of network state
 * WHY: Enable metacognitive awareness and adaptive behavior
 * HOW: Compute variance of inputs over time window
 *      Uncertainty = variance, Confidence = 1 / (1 + uncertainty)
 *
 * @param params Metacognitive parameters
 * @param inputs Array of input values
 * @param num_inputs Number of inputs
 * @param input_history Historical inputs for variance
 * @param history_size Size of history buffer
 * @param out_confidence Output confidence value [0, 1]
 * @param out_uncertainty Output uncertainty value
 * @return Metacognitive activation
 *
 * COMPLEXITY: O(n) where n = history_size
 */
float compute_metacognitive(const metacognitive_params_t* params,
                            const float* inputs, uint32_t num_inputs,
                            const float* input_history, uint32_t history_size,
                            float* out_confidence, float* out_uncertainty);

/**
 * @brief Compute executive control neuron output
 *
 * WHAT: Goal-directed activity with sustained firing and top-down modulation
 * WHY: Models prefrontal executive control
 * HOW: Maintain activity during delays, modulate gain of targets
 *
 * @param params Executive control parameters
 * @param goal_input Goal/context input
 * @param current_state Current neuron state
 * @param dt Time step in ms
 * @param is_delay_period Whether in delay period (maintain activity)
 * @return New activation state
 *
 * COMPLEXITY: O(1)
 */
float compute_executive_control(const executive_control_params_t* params,
                                float goal_input,
                                float current_state,
                                float dt,
                                bool is_delay_period);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Get default parameters for a neuron type
 *
 * @param type Neuron type
 * @param out_params Output parameter structure (must be pre-allocated)
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t neuron_type_get_default_params(neuron_type_t type,
                                               neuron_type_params_t* out_params);

/**
 * @brief Validate type-specific parameters
 *
 * @param type Neuron type
 * @param params Parameters to validate
 * @return NIMCP_SUCCESS if valid, error code otherwise
 */
nimcp_result_t neuron_type_validate_params(neuron_type_t type,
                                            const neuron_type_params_t* params);

/**
 * @brief Get human-readable name for neuron type
 *
 * @param type Neuron type
 * @return String name (e.g., "V1 Simple Cell")
 */
const char* neuron_type_get_name(neuron_type_t type);

/**
 * @brief Check if neuron type is excitatory
 *
 * @param type Neuron type
 * @return true if excitatory, false if inhibitory
 *
 * NOTE: Most specialized types are excitatory by default.
 * Type-specific E/I balance can be configured per-neuron.
 */
bool neuron_type_is_excitatory(neuron_type_t type);

/**
 * Process input through neuron type-specific processing
 *
 * WHAT: Stub function for type-specific input processing
 * WHY:  Needed by tests, full implementation pending
 * HOW:  Returns weighted input based on type parameters
 *
 * @param type Neuron type
 * @param params Type parameters (can be NULL for defaults)
 * @param input Input value
 * @param timestamp Current timestamp in microseconds
 * @return Processed output value
 */
float neuron_type_process_input(neuron_type_t type, const neuron_type_params_t* params,
                                 float input, uint64_t timestamp);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURON_TYPES_H
