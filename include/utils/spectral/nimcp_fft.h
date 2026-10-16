/**
 * @file nimcp_fft.h
 * @brief Fast Fourier Transform for neural signal processing
 *
 * WHAT: Spectral analysis tools for neural oscillations and brain waves
 * WHY:  Analyze frequency content of neural signals, detect rhythms, measure coherence
 * HOW:  Cooley-Tukey FFT algorithm with real/complex transforms and power spectral density
 *
 * NEUROSCIENCE APPLICATIONS:
 * - Brain oscillations: Delta (1-4Hz), Theta (4-8Hz), Alpha (8-13Hz),
 *   Beta (13-30Hz), Gamma (30-100Hz)
 * - Spike train spectral analysis
 * - Local field potential (LFP) frequency decomposition
 * - Cross-frequency coupling detection
 * - Power spectral density for network activity patterns
 *
 * ALGORITHM:
 * - Cooley-Tukey radix-2 FFT: O(N log N) complexity
 * - Power-of-2 sizes for optimal performance
 * - In-place computation for memory efficiency
 * - Windowing functions to reduce spectral leakage
 *
 * USAGE:
 *   // Create FFT plan for 1024 samples
 *   fft_plan_t* plan = fft_plan_create(1024, FFT_REAL);
 *
 *   // Compute FFT of neural signal
 *   float signal[1024];
 *   fft_complex_t spectrum[512];  // N/2 + 1 for real FFT
 *   fft_execute_real(plan, signal, spectrum);
 *
 *   // Compute power spectral density
 *   float psd[512];
 *   fft_power_spectrum(spectrum, psd, 512);
 *
 *   // Find dominant frequency
 *   float freq = fft_dominant_frequency(psd, 512, sampling_rate);
 *
 *   fft_plan_destroy(plan);
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6 (GPU P2P + Spectral Analysis)
 */

#ifndef NIMCP_FFT_H
#define NIMCP_FFT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Complex number for FFT computation
 */
typedef struct {
    float real;      /**< Real part */
    float imag;      /**< Imaginary part */
} fft_complex_t;

/**
 * @brief FFT type
 */
typedef enum {
    FFT_REAL,        /**< Real-to-complex FFT (most common for signals) */
    FFT_COMPLEX,     /**< Complex-to-complex FFT */
    FFT_INVERSE      /**< Inverse FFT (complex-to-real or complex-to-complex) */
} fft_type_t;

/**
 * @brief Window function for reducing spectral leakage
 */
typedef enum {
    FFT_WINDOW_NONE,        /**< Rectangular (no windowing) */
    FFT_WINDOW_HANN,        /**< Hann window (good for general use) */
    FFT_WINDOW_HAMMING,     /**< Hamming window (similar to Hann) */
    FFT_WINDOW_BLACKMAN,    /**< Blackman window (better sidelobe suppression) */
    FFT_WINDOW_KAISER       /**< Kaiser window (adjustable) */
} fft_window_t;

/**
 * @brief Opaque FFT plan structure
 *
 * WHAT: Pre-computed FFT coefficients and metadata
 * WHY:  Avoid recomputing twiddle factors for each FFT
 * HOW:  Store size, type, window, and twiddle factors
 */
typedef struct fft_plan_struct fft_plan_t;

//=============================================================================
// FFT Plan Management
//=============================================================================

/**
 * WHAT: Create FFT plan for given size and type
 * WHY:  Pre-compute twiddle factors to optimize repeated FFTs
 * HOW:  Allocate plan, compute twiddle factors, generate bit-reversal table
 *
 * @param size FFT size (must be power of 2, range: 2-65536)
 * @param type FFT type (real, complex, or inverse)
 * @return FFT plan on success, NULL on failure
 *
 * COMPLEXITY: O(N log N) for plan creation (one-time cost)
 * MEMORY: O(N) for twiddle factors and bit-reversal table
 *
 * NOTE: Size must be power of 2. Use fft_next_power_of_2() if needed.
 */
fft_plan_t* fft_plan_create(uint32_t size, fft_type_t type);

/**
 * WHAT: Destroy FFT plan and free resources
 * WHY:  Prevent memory leaks
 * HOW:  Free twiddle factors, bit-reversal table, and plan structure
 *
 * @param plan FFT plan to destroy
 */
void fft_plan_destroy(fft_plan_t* plan);

/**
 * WHAT: Set window function for FFT plan
 * WHY:  Reduce spectral leakage (artifacts at non-integer frequencies)
 * HOW:  Pre-compute window coefficients and apply during FFT execution
 *
 * @param plan FFT plan
 * @param window Window function type
 * @return true on success, false on failure
 *
 * NOTE: Window is applied to input signal before FFT
 * Recommended: Hann window for general signal processing
 */
bool fft_plan_set_window(fft_plan_t* plan, fft_window_t window);

/**
 * WHAT: Get FFT size from plan
 * WHY:  Query plan properties for allocation and validation
 * HOW:  Return stored size field
 *
 * @param plan FFT plan
 * @return FFT size, or 0 if plan is NULL
 */
uint32_t fft_plan_get_size(const fft_plan_t* plan);

//=============================================================================
// FFT Execution
//=============================================================================

/**
 * WHAT: Execute real-to-complex FFT
 * WHY:  Most common case for neural signals (real-valued input)
 * HOW:  Apply window, bit-reverse, Cooley-Tukey butterfly operations
 *
 * @param plan FFT plan (must be FFT_REAL type)
 * @param input Real input signal [size]
 * @param output Complex output spectrum [size/2 + 1]
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(N log N) where N = FFT size
 * MEMORY: Input not modified, output must be pre-allocated
 *
 * OUTPUT FORMAT:
 * - DC component: output[0].real (output[0].imag = 0)
 * - Nyquist freq: output[N/2].real (output[N/2].imag = 0)
 * - Frequencies k: output[k] for k = 1..(N/2-1)
 *
 * FREQUENCY BINS:
 * - Bin k corresponds to frequency k * (sampling_rate / N)
 * - Example: 1000 Hz sampling, N=1024 → bin 10 = 9.77 Hz
 */
bool fft_execute_real(fft_plan_t* plan, const float* input, fft_complex_t* output);

/**
 * WHAT: Execute complex-to-complex FFT
 * WHY:  Handle complex signals or use for inverse FFT
 * HOW:  Cooley-Tukey FFT with complex butterfly operations
 *
 * @param plan FFT plan (FFT_COMPLEX or FFT_INVERSE type)
 * @param input Complex input [size]
 * @param output Complex output [size]
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(N log N)
 *
 * NOTE: Can be in-place (input == output) for memory efficiency
 */
bool fft_execute_complex(fft_plan_t* plan, const fft_complex_t* input,
                         fft_complex_t* output);

/**
 * WHAT: Execute inverse FFT (frequency → time domain)
 * WHY:  Reconstruct signal from spectrum
 * HOW:  Conjugate, FFT, conjugate, scale by 1/N
 *
 * @param plan FFT plan (must be FFT_INVERSE type)
 * @param input Complex frequency spectrum [size/2 + 1] for real output
 * @param output Real time-domain signal [size]
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(N log N)
 *
 * USE CASES:
 * - Signal reconstruction after filtering
 * - Frequency-domain convolution
 */
bool fft_execute_inverse_real(fft_plan_t* plan, const fft_complex_t* input,
                               float* output);

/**
 * WHAT: Execute inverse complex-to-complex FFT
 * WHY:  Reconstruct complex time-domain signal from frequency spectrum
 * HOW:  Conjugate input, FFT, conjugate and scale output by 1/N
 *
 * @param plan FFT plan (must be FFT_INVERSE type)
 * @param input Complex frequency spectrum [size]
 * @param output Complex time-domain signal [size]
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(N log N)
 *
 * USE CASES:
 * - Hilbert transform (analytic signal reconstruction)
 * - Complex filtering in frequency domain
 * - Phase-amplitude coupling analysis
 */
bool fft_execute_inverse_complex(fft_plan_t* plan, const fft_complex_t* input,
                                  fft_complex_t* output);

//=============================================================================
// Power Spectral Density (PSD)
//=============================================================================

/**
 * WHAT: Compute power spectrum from FFT output
 * WHY:  Measure signal power at each frequency
 * HOW:  |X[k]|² = real² + imag²
 *
 * @param spectrum Complex FFT output [size]
 * @param power Power values [size]
 * @param size Number of frequency bins
 * @return true on success, false on failure
 *
 * OUTPUT: Power[k] = Real[k]² + Imag[k]²
 *
 * UNITS: Power is in squared units of input signal
 * For voltage: V² (proportional to power)
 * For normalized signals: unitless
 */
bool fft_power_spectrum(const fft_complex_t* spectrum, float* power, uint32_t size);

/**
 * WHAT: Compute power spectral density in dB
 * WHY:  Logarithmic scale shows wide dynamic range
 * HOW:  10 * log10(power)
 *
 * @param spectrum Complex FFT output [size]
 * @param psd_db Power spectral density in dB [size]
 * @param size Number of frequency bins
 * @return true on success, false on failure
 *
 * OUTPUT: PSD[k] = 10 * log10(Real[k]² + Imag[k]²)
 *
 * UNITS: Decibels (dB)
 * Reference: 0 dB = power of 1.0
 */
bool fft_power_spectrum_db(const fft_complex_t* spectrum, float* psd_db,
                           uint32_t size);

/**
 * WHAT: Compute magnitude spectrum
 * WHY:  Amplitude at each frequency (not squared)
 * HOW:  |X[k]| = sqrt(real² + imag²)
 *
 * @param spectrum Complex FFT output [size]
 * @param magnitude Magnitude values [size]
 * @param size Number of frequency bins
 * @return true on success, false on failure
 *
 * OUTPUT: Magnitude[k] = sqrt(Real[k]² + Imag[k]²)
 */
bool fft_magnitude_spectrum(const fft_complex_t* spectrum, float* magnitude,
                            uint32_t size);

/**
 * WHAT: Compute phase spectrum
 * WHY:  Phase angle at each frequency
 * HOW:  atan2(imag, real)
 *
 * @param spectrum Complex FFT output [size]
 * @param phase Phase in radians [size]
 * @param size Number of frequency bins
 * @return true on success, false on failure
 *
 * OUTPUT: Phase[k] = atan2(Imag[k], Real[k])
 * RANGE: -π to +π radians
 */
bool fft_phase_spectrum(const fft_complex_t* spectrum, float* phase, uint32_t size);

//=============================================================================
// Spectral Analysis Utilities
//=============================================================================

/**
 * WHAT: Find dominant frequency in power spectrum
 * WHY:  Identify peak oscillation frequency
 * HOW:  Argmax of power spectrum, convert bin to Hz
 *
 * @param power Power spectrum [size]
 * @param size Number of frequency bins
 * @param sampling_rate Sampling rate in Hz
 * @return Dominant frequency in Hz, or 0.0 if invalid
 *
 * EXAMPLE:
 * - Sampling rate: 1000 Hz
 * - FFT size: 1024
 * - Peak at bin 20 → frequency = 20 * (1000/1024) = 19.53 Hz (beta wave)
 */
float fft_dominant_frequency(const float* power, uint32_t size, float sampling_rate);

/**
 * WHAT: Compute total power in frequency band
 * WHY:  Measure oscillatory power in specific brain rhythm (e.g., alpha, beta)
 * HOW:  Sum power[k] for bins in [freq_low, freq_high]
 *
 * @param power Power spectrum [size]
 * @param size Number of frequency bins
 * @param sampling_rate Sampling rate in Hz
 * @param freq_low Lower frequency bound (Hz)
 * @param freq_high Upper frequency bound (Hz)
 * @return Total power in band
 *
 * EXAMPLE: Measure alpha power (8-13 Hz)
 *   float alpha_power = fft_band_power(power, 512, 1000.0, 8.0, 13.0);
 */
float fft_band_power(const float* power, uint32_t size, float sampling_rate,
                     float freq_low, float freq_high);

/**
 * WHAT: Find frequency bin index for given frequency
 * WHY:  Convert Hz to bin index for array indexing
 * HOW:  bin = round(frequency * N / sampling_rate)
 *
 * @param frequency Target frequency in Hz
 * @param fft_size FFT size (N)
 * @param sampling_rate Sampling rate in Hz
 * @return Bin index, or -1 if out of range
 *
 * NOTE: Nyquist frequency = sampling_rate / 2
 */
int32_t fft_frequency_to_bin(float frequency, uint32_t fft_size, float sampling_rate);

/**
 * WHAT: Convert bin index to frequency
 * WHY:  Label frequency axis in plots
 * HOW:  frequency = bin * sampling_rate / N
 *
 * @param bin Bin index
 * @param fft_size FFT size (N)
 * @param sampling_rate Sampling rate in Hz
 * @return Frequency in Hz
 */
float fft_bin_to_frequency(uint32_t bin, uint32_t fft_size, float sampling_rate);

//=============================================================================
// Brain Wave Analysis (Neuroscience-Specific)
//=============================================================================

/**
 * @brief Standard brain wave frequency bands
 */
typedef enum {
    BRAIN_WAVE_DELTA,   /**< Delta: 1-4 Hz (deep sleep) */
    BRAIN_WAVE_THETA,   /**< Theta: 4-8 Hz (drowsiness, meditation) */
    BRAIN_WAVE_ALPHA,   /**< Alpha: 8-13 Hz (relaxed, eyes closed) */
    BRAIN_WAVE_BETA,    /**< Beta: 13-30 Hz (active thinking, focus) */
    BRAIN_WAVE_GAMMA    /**< Gamma: 30-100 Hz (higher cognitive functions) */
} brain_wave_band_t;

/**
 * WHAT: Compute power in standard brain wave band
 * WHY:  Analyze neural oscillations using established neuroscience bands
 * HOW:  Pre-defined frequency ranges, sum power in band
 *
 * @param power Power spectrum
 * @param size Number of frequency bins
 * @param sampling_rate Sampling rate in Hz
 * @param band Brain wave band (delta, theta, alpha, beta, gamma)
 * @return Total power in brain wave band
 *
 * FREQUENCY RANGES:
 * - Delta: 1-4 Hz (deep sleep, unconsciousness)
 * - Theta: 4-8 Hz (drowsy, light sleep, meditation)
 * - Alpha: 8-13 Hz (relaxed, eyes closed, wakeful rest)
 * - Beta: 13-30 Hz (alert, focused, active thinking)
 * - Gamma: 30-100 Hz (perception, consciousness, learning)
 */
float fft_brain_wave_power(const float* power, uint32_t size, float sampling_rate,
                           brain_wave_band_t band);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Round up to next power of 2
 * WHY:  FFT requires power-of-2 sizes for radix-2 algorithm
 * HOW:  Bit manipulation to find next power of 2
 *
 * @param n Input size
 * @return Next power of 2 >= n
 *
 * EXAMPLES:
 * - fft_next_power_of_2(1000) = 1024
 * - fft_next_power_of_2(2048) = 2048
 */
uint32_t fft_next_power_of_2(uint32_t n);

/**
 * WHAT: Check if number is power of 2
 * WHY:  Validate FFT size requirements
 * HOW:  n & (n-1) == 0 for powers of 2
 *
 * @param n Number to check
 * @return true if n is power of 2, false otherwise
 */
bool fft_is_power_of_2(uint32_t n);

/**
 * WHAT: Apply window function to signal (in-place)
 * WHY:  Reduce spectral leakage before FFT
 * HOW:  Multiply signal[k] by window[k]
 *
 * @param signal Signal to window [size]
 * @param size Signal size
 * @param window Window function type
 * @return true on success, false on failure
 *
 * NOTE: Modifies signal in-place
 * Windowing is automatically applied by fft_execute_real if window is set
 */
bool fft_apply_window(float* signal, uint32_t size, fft_window_t window);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FFT_H */
