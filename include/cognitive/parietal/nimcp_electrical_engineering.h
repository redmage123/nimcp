/**
 * @file nimcp_electrical_engineering.h
 * @brief Electrical engineering reasoning module for parietal lobe
 *
 * WHAT: Electrical engineering analysis and reasoning capabilities
 * WHY:  Enable intelligent reasoning about circuits, signals, and power systems
 * HOW:  Domain-specific analysis with intuitive extrapolation
 *
 * BIOLOGICAL BASIS:
 * The parietal cortex processes spatial-numerical relationships used in
 * circuit topology understanding and signal analysis.
 *
 * CAPABILITIES:
 * - Circuit analysis (DC, AC, transient)
 * - Signal processing (frequency domain, filtering)
 * - Power systems analysis
 * - Control systems (transfer functions, stability)
 * - Electromagnetic field reasoning
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_ELECTRICAL_ENGINEERING_H
#define NIMCP_ELECTRICAL_ENGINEERING_H

#include "utils/validation/nimcp_common.h"
#include "utils/math/nimcp_complex_math.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Use neural_phasor_t for C/C++ compatible complex numbers */
typedef neural_phasor_t ee_complex_t;

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum circuit nodes */
#define EE_MAX_NODES                    256

/** Maximum circuit elements */
#define EE_MAX_ELEMENTS                 512

/** Maximum transfer function order */
#define EE_MAX_TF_ORDER                 16

/** Bio-async module ID */
#define BIO_MODULE_ELECTRICAL_ENG       0x0390

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for electrical engineering processor */
typedef struct electrical_eng electrical_eng_t;

/**
 * @brief Circuit element types
 */
typedef enum {
    EE_ELEMENT_RESISTOR,
    EE_ELEMENT_CAPACITOR,
    EE_ELEMENT_INDUCTOR,
    EE_ELEMENT_VOLTAGE_SOURCE,
    EE_ELEMENT_CURRENT_SOURCE,
    EE_ELEMENT_DIODE,
    EE_ELEMENT_TRANSISTOR_NPN,
    EE_ELEMENT_TRANSISTOR_PNP,
    EE_ELEMENT_MOSFET_N,
    EE_ELEMENT_MOSFET_P,
    EE_ELEMENT_OP_AMP,
    EE_ELEMENT_TRANSFORMER,
    EE_ELEMENT_TRANSMISSION_LINE
} ee_element_type_t;

/**
 * @brief Analysis types
 */
typedef enum {
    EE_ANALYSIS_DC,                     /**< DC operating point */
    EE_ANALYSIS_AC,                     /**< AC small signal */
    EE_ANALYSIS_TRANSIENT,              /**< Time domain */
    EE_ANALYSIS_FREQUENCY,              /**< Frequency sweep */
    EE_ANALYSIS_NOISE,                  /**< Noise analysis */
    EE_ANALYSIS_SENSITIVITY             /**< Sensitivity analysis */
} ee_analysis_type_t;

/**
 * @brief Signal types
 */
typedef enum {
    EE_SIGNAL_DC,
    EE_SIGNAL_SINE,
    EE_SIGNAL_SQUARE,
    EE_SIGNAL_TRIANGLE,
    EE_SIGNAL_SAWTOOTH,
    EE_SIGNAL_PULSE,
    EE_SIGNAL_STEP,
    EE_SIGNAL_RAMP,
    EE_SIGNAL_NOISE,
    EE_SIGNAL_ARBITRARY
} ee_signal_type_t;

/**
 * @brief Filter types
 */
typedef enum {
    EE_FILTER_LOWPASS,
    EE_FILTER_HIGHPASS,
    EE_FILTER_BANDPASS,
    EE_FILTER_BANDSTOP,
    EE_FILTER_ALLPASS,
    EE_FILTER_NOTCH
} ee_filter_type_t;

/**
 * @brief Filter design methods
 */
typedef enum {
    EE_FILTER_BUTTERWORTH,
    EE_FILTER_CHEBYSHEV_I,
    EE_FILTER_CHEBYSHEV_II,
    EE_FILTER_ELLIPTIC,
    EE_FILTER_BESSEL
} ee_filter_design_t;

/**
 * @brief Circuit element
 */
typedef struct {
    uint32_t id;
    ee_element_type_t type;
    uint32_t node_positive;             /**< Positive terminal node */
    uint32_t node_negative;             /**< Negative terminal node */
    float value;                        /**< Primary value (R, C, L, V, I) */
    float tolerance;                    /**< Component tolerance [0,1] */
    float temperature_coeff;            /**< Temperature coefficient */

    /* Additional parameters for complex elements */
    union {
        struct {                        /* For transformers */
            float turns_ratio;
            float coupling_coeff;
        } transformer;
        struct {                        /* For transmission lines */
            float impedance;
            float length;
            float velocity_factor;
        } tline;
        struct {                        /* For transistors */
            float beta;                 /**< Current gain */
            float vth;                  /**< Threshold voltage */
        } transistor;
    } params;
} ee_element_t;

/**
 * @brief Circuit definition
 */
typedef struct {
    ee_element_t* elements;
    uint32_t num_elements;
    uint32_t num_nodes;
    uint32_t ground_node;               /**< Reference node */
    char name[64];
} ee_circuit_t;

/**
 * @brief DC analysis result
 */
typedef struct {
    float* node_voltages;               /**< Voltage at each node */
    float* branch_currents;             /**< Current through each element */
    uint32_t num_nodes;
    uint32_t num_branches;
    float total_power_dissipation;
    bool converged;
} ee_dc_result_t;

/**
 * @brief AC analysis result
 */
typedef struct {
    ee_complex_t* node_voltages;        /**< Complex voltage phasors */
    ee_complex_t* branch_currents;      /**< Complex current phasors */
    ee_complex_t* impedances;           /**< Element impedances */
    float frequency;
    float* magnitude_db;                /**< Magnitude in dB */
    float* phase_deg;                   /**< Phase in degrees */
    uint32_t num_nodes;
} ee_ac_result_t;

/**
 * @brief Transient analysis result
 */
typedef struct {
    float** node_voltages;              /**< [time_steps][num_nodes] */
    float** branch_currents;            /**< [time_steps][num_elements] */
    float* time_points;                 /**< Time values */
    uint32_t num_steps;
    uint32_t num_nodes;
    float dt;
    float t_start;
    float t_end;
} ee_transient_result_t;

/**
 * @brief Frequency response
 */
typedef struct {
    float* frequencies;                 /**< Frequency points (Hz) */
    float* magnitude_db;                /**< Magnitude response (dB) */
    float* phase_deg;                   /**< Phase response (degrees) */
    float* group_delay;                 /**< Group delay (seconds) */
    uint32_t num_points;
    float f_3db;                        /**< -3dB cutoff frequency */
    float bandwidth;                    /**< Bandwidth */
    float phase_margin;                 /**< Phase margin (degrees) */
    float gain_margin;                  /**< Gain margin (dB) */
} ee_frequency_response_t;

/**
 * @brief Transfer function representation
 */
typedef struct {
    ee_complex_t* zeros;                /**< Zeros of transfer function */
    ee_complex_t* poles;                /**< Poles of transfer function */
    uint32_t num_zeros;
    uint32_t num_poles;
    float gain;                         /**< DC gain */
    float* numerator;                   /**< Numerator coefficients */
    float* denominator;                 /**< Denominator coefficients */
    uint32_t num_order;                 /**< Numerator order */
    uint32_t den_order;                 /**< Denominator order */
} ee_transfer_function_t;

/**
 * @brief Stability analysis result
 */
typedef struct {
    bool is_stable;                     /**< System is stable */
    bool is_marginally_stable;          /**< On stability boundary */
    float phase_margin_deg;             /**< Phase margin */
    float gain_margin_db;               /**< Gain margin */
    float crossover_frequency;          /**< Unity gain frequency */
    float* pole_real;                   /**< Real parts of poles */
    float* pole_imag;                   /**< Imaginary parts of poles */
    uint32_t num_poles;
    float settling_time;                /**< 2% settling time */
    float rise_time;                    /**< 10-90% rise time */
    float overshoot_percent;            /**< Peak overshoot % */
} ee_stability_result_t;

/**
 * @brief Power system analysis result
 */
typedef struct {
    float real_power;                   /**< Real power (W) */
    float reactive_power;               /**< Reactive power (VAR) */
    float apparent_power;               /**< Apparent power (VA) */
    float power_factor;                 /**< Power factor */
    float thd;                          /**< Total harmonic distortion */
    float* harmonic_magnitudes;         /**< Harmonic magnitudes */
    uint32_t num_harmonics;
    float efficiency;                   /**< System efficiency */
} ee_power_result_t;

/**
 * @brief Electrical engineering configuration
 */
typedef struct {
    float default_frequency;            /**< Default AC frequency (Hz) */
    float temperature;                  /**< Temperature (Celsius) */
    float convergence_tolerance;        /**< Solver convergence tolerance */
    uint32_t max_iterations;            /**< Max solver iterations */
    bool include_parasitics;            /**< Include parasitic elements */
    float min_frequency;                /**< Min frequency for sweeps */
    float max_frequency;                /**< Max frequency for sweeps */
    uint32_t frequency_points;          /**< Points per decade */
    bool enable_intuition;              /**< Enable intuitive analysis */
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} ee_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t dc_analyses;
    uint64_t ac_analyses;
    uint64_t transient_analyses;
    uint64_t filter_designs;
    uint64_t stability_analyses;
    float avg_processing_time_us;
} ee_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

electrical_eng_t* electrical_eng_create(void);
electrical_eng_t* electrical_eng_create_custom(const ee_config_t* config);
void electrical_eng_destroy(electrical_eng_t* ee);
ee_config_t electrical_eng_default_config(void);

/* ============================================================================
 * CIRCUIT ANALYSIS API
 * ============================================================================ */

/**
 * @brief Create circuit
 */
ee_circuit_t* electrical_eng_create_circuit(const char* name);

/**
 * @brief Destroy circuit
 */
void electrical_eng_destroy_circuit(ee_circuit_t* circuit);

/**
 * @brief Add element to circuit
 */
int electrical_eng_add_element(
    ee_circuit_t* circuit,
    ee_element_type_t type,
    uint32_t node_pos,
    uint32_t node_neg,
    float value
);

/**
 * @brief Perform DC analysis
 */
int electrical_eng_dc_analysis(
    electrical_eng_t* ee,
    const ee_circuit_t* circuit,
    ee_dc_result_t* result
);

/**
 * @brief Perform AC analysis at frequency
 */
int electrical_eng_ac_analysis(
    electrical_eng_t* ee,
    const ee_circuit_t* circuit,
    float frequency,
    ee_ac_result_t* result
);

/**
 * @brief Perform transient analysis
 */
int electrical_eng_transient_analysis(
    electrical_eng_t* ee,
    const ee_circuit_t* circuit,
    float t_start,
    float t_end,
    float dt,
    ee_transient_result_t* result
);

/**
 * @brief Free DC result
 */
void electrical_eng_free_dc_result(ee_dc_result_t* result);

/**
 * @brief Free AC result
 */
void electrical_eng_free_ac_result(ee_ac_result_t* result);

/**
 * @brief Free transient result
 */
void electrical_eng_free_transient_result(ee_transient_result_t* result);

/* ============================================================================
 * SIGNAL PROCESSING API
 * ============================================================================ */

/**
 * @brief Compute frequency response
 */
int electrical_eng_frequency_response(
    electrical_eng_t* ee,
    const ee_transfer_function_t* tf,
    float f_start,
    float f_end,
    uint32_t num_points,
    ee_frequency_response_t* response
);

/**
 * @brief Design filter
 */
int electrical_eng_design_filter(
    electrical_eng_t* ee,
    ee_filter_type_t type,
    ee_filter_design_t design,
    float cutoff_freq,
    float stopband_freq,
    float passband_ripple_db,
    float stopband_atten_db,
    uint32_t order,
    ee_transfer_function_t* tf
);

/**
 * @brief Apply filter to signal
 */
int electrical_eng_apply_filter(
    electrical_eng_t* ee,
    const ee_transfer_function_t* filter,
    const float* input,
    float* output,
    uint32_t num_samples,
    float sample_rate
);

/**
 * @brief Compute FFT
 */
int electrical_eng_fft(
    electrical_eng_t* ee,
    const float* signal,
    uint32_t num_samples,
    ee_complex_t* spectrum
);

/**
 * @brief Compute inverse FFT
 */
int electrical_eng_ifft(
    electrical_eng_t* ee,
    const ee_complex_t* spectrum,
    uint32_t num_samples,
    float* signal
);

/**
 * @brief Free frequency response
 */
void electrical_eng_free_frequency_response(ee_frequency_response_t* response);

/**
 * @brief Free transfer function
 */
void electrical_eng_free_transfer_function(ee_transfer_function_t* tf);

/* ============================================================================
 * CONTROL SYSTEMS API
 * ============================================================================ */

/**
 * @brief Create transfer function from coefficients
 */
int electrical_eng_create_transfer_function(
    const float* numerator,
    uint32_t num_order,
    const float* denominator,
    uint32_t den_order,
    ee_transfer_function_t* tf
);

/**
 * @brief Analyze system stability
 */
int electrical_eng_stability_analysis(
    electrical_eng_t* ee,
    const ee_transfer_function_t* tf,
    ee_stability_result_t* result
);

/**
 * @brief Compute step response
 */
int electrical_eng_step_response(
    electrical_eng_t* ee,
    const ee_transfer_function_t* tf,
    float t_end,
    float dt,
    float** response,
    uint32_t* num_points
);

/**
 * @brief Design PID controller
 */
int electrical_eng_design_pid(
    electrical_eng_t* ee,
    const ee_transfer_function_t* plant,
    float desired_bandwidth,
    float desired_phase_margin,
    float* kp,
    float* ki,
    float* kd
);

/**
 * @brief Free stability result
 */
void electrical_eng_free_stability_result(ee_stability_result_t* result);

/* ============================================================================
 * POWER SYSTEMS API
 * ============================================================================ */

/**
 * @brief Analyze power in AC circuit
 */
int electrical_eng_power_analysis(
    electrical_eng_t* ee,
    ee_complex_t voltage,
    ee_complex_t current,
    ee_power_result_t* result
);

/**
 * @brief Compute harmonic analysis
 */
int electrical_eng_harmonic_analysis(
    electrical_eng_t* ee,
    const float* waveform,
    uint32_t num_samples,
    float fundamental_freq,
    float sample_rate,
    ee_power_result_t* result
);

/**
 * @brief Calculate power factor correction
 */
int electrical_eng_power_factor_correction(
    electrical_eng_t* ee,
    float real_power,
    float current_pf,
    float target_pf,
    float frequency,
    float* capacitor_value
);

/**
 * @brief Free power result
 */
void electrical_eng_free_power_result(ee_power_result_t* result);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int electrical_eng_set_inflammation(electrical_eng_t* ee, float level);
int electrical_eng_set_fatigue(electrical_eng_t* ee, float level);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int electrical_eng_get_stats(const electrical_eng_t* ee, ee_stats_t* stats);
void electrical_eng_reset_stats(electrical_eng_t* ee);
const char* electrical_eng_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ELECTRICAL_ENGINEERING_H */
