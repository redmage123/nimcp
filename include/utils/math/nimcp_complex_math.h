//=============================================================================
// nimcp_complex_math.h - Complex Number Mathematics for Neural Phase Coding
//=============================================================================
/**
 * @file nimcp_complex_math.h
 * @brief Complex number support for neural oscillations and phase relationships
 *
 * WHAT: Complex phasor mathematics for neural phase-amplitude representations
 * WHY:  Brain uses phase relationships for binding, memory, and spatial coding
 * HOW:  C99 complex.h with neural-specific wrappers and optimizations
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Phase Coding in Neural Systems:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  Theta Oscillation (8 Hz)                                   │
 *   │    ╱╲      ╱╲      ╱╲      ╱╲                              │
 *   │   ╱  ╲    ╱  ╲    ╱  ╲    ╱  ╲                             │
 *   │  ╱    ╲  ╱    ╲  ╱    ╲  ╱    ╲                            │
 *   │        ╲╱      ╲╱      ╲╱      ╲╱                           │
 *   │         │       │       │       │                            │
 *   │      Phase:  0°    90°   180°   270°                        │
 *   │                                                              │
 *   │  Information encoded in PHASE, not just amplitude:          │
 *   │  - Hippocampal place cells: theta phase = position          │
 *   │  - Working memory: gamma phase = item order                 │
 *   │  - Grid cells: phase interference → spatial patterns        │
 *   └──────────────────────────────────────────────────────────────┘
 *
 *   Complex Representation (Phasor):
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │   Imaginary Axis                                             │
 *   │        ↑                                                     │
 *   │        │  z = A·e^(iφ) = A·(cos φ + i·sin φ)               │
 *   │        │                                                     │
 *   │        │ /|  ← Phasor (length = amplitude A)                │
 *   │        │/ | θ   (angle = phase φ)                           │
 *   │   ─────●──────→ Real Axis                                   │
 *   │                                                              │
 *   │  Operations:                                                 │
 *   │  - Multiplication: z1·z2 → Add phases, multiply amplitudes  │
 *   │  - Division: z1/z2 → Subtract phases, divide amplitudes     │
 *   │  - Conjugate: z* → Flip phase sign (for phase difference)   │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * PERFORMANCE:
 * - Phasor creation: ~5ns (2x float assignment)
 * - Phase difference: ~8ns (1 multiply + 1 arg)
 * - Array coherence (N=1000): <1µs (vectorized)
 * - FFT (N=1024): <50µs (FFTW comparable)
 *
 * MEMORY:
 * - neural_phasor_t: 8 bytes (2x float)
 * - 2x overhead vs amplitude-only, but encodes phase information
 * - Algorithmic savings compensate for memory cost
 *
 * INTEGRATION:
 * - Core: Brain oscillations, complex neurons/synapses
 * - Middleware: PAC detector, oscillation detector, pattern library
 * - API: Complex oscillation queries, phase synchrony metrics
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 * @version 1.0.0
 */

#ifndef NIMCP_COMPLEX_MATH_H
#define NIMCP_COMPLEX_MATH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Neural phasor representation (complex number)
 *
 * Represents neural oscillations as amplitude + phase:
 * - Amplitude (magnitude): Strength of oscillation
 * - Phase (angle): Timing within oscillation cycle
 *
 * Mathematical form: z = A·e^(iφ) where:
 * - A = amplitude = |z| = sqrt(real² + imag²)
 * - φ = phase = arg(z) = atan2(imag, real)
 *
 * NOTE: Uses explicit real/imag fields for C++ compatibility
 */
typedef struct {
    float real;  /**< Real part (in-phase component) */
    float imag;  /**< Imaginary part (quadrature component) */
} neural_phasor_t;

/**
 * @brief Configuration for complex math operations
 */
typedef struct {
    bool enable_simd;           /**< Use SIMD vectorization if available */
    bool enable_fft_cache;      /**< Cache FFT plans for reuse */
    uint32_t fft_cache_size;    /**< Maximum number of cached FFT plans */
} complex_math_config_t;

/**
 * @brief Statistics for complex signal analysis
 */
typedef struct {
    float mean_amplitude;       /**< Mean amplitude across phasors */
    float mean_phase;           /**< Circular mean phase */
    float phase_std;            /**< Phase standard deviation (circular) */
    float coherence;            /**< Inter-trial phase coherence (0-1) */
    float synchrony;            /**< Phase synchrony index (0-1) */
} complex_signal_stats_t;

//=============================================================================
// Core Phasor Operations
//=============================================================================

/**
 * @brief Create phasor from polar coordinates (amplitude, phase)
 *
 * @param amplitude Oscillation amplitude (≥ 0)
 * @param phase Phase angle in radians (-π to π)
 * @return Neural phasor z = amplitude·e^(i·phase)
 *
 * Performance: ~5ns
 *
 * Example:
 *   neural_phasor_t z = phasor_from_polar(1.0f, M_PI/4);  // 45° phase
 */
NIMCP_EXPORT neural_phasor_t phasor_from_polar(float amplitude, float phase);

/**
 * @brief Create phasor from Cartesian coordinates (real, imaginary)
 *
 * @param real Real part (in-phase component)
 * @param imag Imaginary part (quadrature component)
 * @return Neural phasor z = real + i·imag
 *
 * Performance: ~3ns
 */
NIMCP_EXPORT neural_phasor_t phasor_from_cartesian(float real, float imag);

/**
 * @brief Extract amplitude (magnitude) from phasor
 *
 * @param z Neural phasor
 * @return Amplitude |z| = sqrt(real² + imag²)
 *
 * Performance: ~8ns (sqrt)
 */
NIMCP_EXPORT float phasor_amplitude(neural_phasor_t z);

/**
 * @brief Extract phase (angle) from phasor
 *
 * @param z Neural phasor
 * @return Phase arg(z) = atan2(imag, real) in radians (-π to π)
 *
 * Performance: ~12ns (atan2)
 */
NIMCP_EXPORT float phasor_phase(neural_phasor_t z);

/**
 * @brief Compute phase difference between two phasors
 *
 * Returns the phase difference φ2 - φ1, wrapped to [-π, π].
 * Used for computing phase lags, leads, and synchronization.
 *
 * @param z1 First phasor
 * @param z2 Second phasor
 * @return Phase difference in radians (-π to π)
 *
 * Performance: ~8ns (multiply + arg)
 *
 * Mathematical form: arg(z2·z1*) where z1* is complex conjugate
 */
NIMCP_EXPORT float phasor_phase_difference(neural_phasor_t z1, neural_phasor_t z2);

/**
 * @brief Normalize phasor to unit amplitude
 *
 * Returns z/|z|, preserving phase but setting amplitude to 1.0.
 * Useful for phase-only analysis.
 *
 * @param z Neural phasor
 * @return Normalized phasor with amplitude = 1
 */
NIMCP_EXPORT neural_phasor_t phasor_normalize(neural_phasor_t z);

//=============================================================================
// Array Operations (Vectorized)
//=============================================================================

/**
 * @brief Compute inter-trial phase coherence (ITPC)
 *
 * ITPC measures phase consistency across trials/neurons:
 * ITPC = |mean(z_i/|z_i|)| where i indexes trials
 *
 * Range: [0, 1] where:
 * - 0 = random phases (no coherence)
 * - 1 = identical phases (perfect coherence)
 *
 * @param signals Array of neural phasors
 * @param n Number of phasors
 * @return Coherence value in [0, 1]
 *
 * Performance: ~0.8µs for n=1000 (vectorized)
 *
 * Neuroscience application:
 * - High ITPC → synchronized oscillations
 * - Low ITPC → desynchronized/independent oscillations
 */
NIMCP_EXPORT float phasor_array_coherence(const neural_phasor_t* signals, uint32_t n);

/**
 * @brief Compute phase synchrony index (PLV - Phase Locking Value)
 *
 * Measures phase synchronization between two signals:
 * PLV = |mean(exp(i·(φ1_i - φ2_i)))|
 *
 * @param signals1 First array of phasors
 * @param signals2 Second array of phasors
 * @param n Number of phasors (must match for both arrays)
 * @return Synchrony value in [0, 1]
 *
 * Performance: ~1.2µs for n=1000
 *
 * Neuroscience application:
 * - Measure cross-frequency coupling (theta-gamma)
 * - Assess inter-regional synchronization
 */
NIMCP_EXPORT float phasor_array_synchrony(const neural_phasor_t* signals1,
                                          const neural_phasor_t* signals2,
                                          uint32_t n);

/**
 * @brief Compute circular mean phase
 *
 * Computes mean phase accounting for circular statistics
 * (e.g., mean of 350° and 10° should be 0°, not 180°)
 *
 * @param signals Array of neural phasors
 * @param n Number of phasors
 * @return Mean phase in radians (-π to π)
 *
 * Performance: ~0.5µs for n=1000
 */
NIMCP_EXPORT float phasor_array_mean_phase(const neural_phasor_t* signals, uint32_t n);

/**
 * @brief Compute circular phase variance
 *
 * Returns 1 - R where R is the mean resultant length.
 * Lower values indicate tighter phase clustering.
 *
 * @param signals Array of neural phasors
 * @param n Number of phasors
 * @return Phase variance in [0, 1] (0 = all same phase, 1 = uniform)
 *
 * Performance: ~0.8µs for n=1000
 */
NIMCP_EXPORT float phasor_array_phase_variance(const neural_phasor_t* signals, uint32_t n);

/**
 * @brief Compute comprehensive signal statistics
 *
 * Efficiently computes multiple statistics in one pass over data.
 *
 * @param signals Array of neural phasors
 * @param n Number of phasors
 * @param stats Output statistics structure
 *
 * Performance: ~1.5µs for n=1000 (all stats combined)
 */
NIMCP_EXPORT void phasor_array_statistics(const neural_phasor_t* signals,
                                          uint32_t n,
                                          complex_signal_stats_t* stats);

//=============================================================================
// FFT Operations
//=============================================================================

/**
 * @brief Perform forward complex FFT
 *
 * Computes discrete Fourier transform of complex signal.
 * Input and output can be the same array (in-place).
 *
 * @param input Input signal (time domain)
 * @param output Output spectrum (frequency domain)
 * @param n Signal length (must be power of 2 for optimal performance)
 * @return true on success, false on error
 *
 * Performance: ~50µs for n=1024
 *
 * Note: Uses internal FFT implementation or FFTW if available
 */
NIMCP_EXPORT bool phasor_fft(const neural_phasor_t* input,
                             neural_phasor_t* output,
                             uint32_t n);

/**
 * @brief Perform inverse complex FFT
 *
 * Computes inverse Fourier transform (frequency → time).
 * Input and output can be the same array (in-place).
 *
 * @param input Input spectrum (frequency domain)
 * @param output Output signal (time domain)
 * @param n Signal length (must be power of 2 for optimal performance)
 * @return true on success, false on error
 *
 * Performance: ~50µs for n=1024
 */
NIMCP_EXPORT bool phasor_ifft(const neural_phasor_t* input,
                              neural_phasor_t* output,
                              uint32_t n);

/**
 * @brief Compute power spectral density (PSD)
 *
 * Returns |FFT(x)|² for each frequency bin.
 *
 * @param input Input signal (time domain)
 * @param output Output PSD (power at each frequency)
 * @param n Signal length
 * @return true on success, false on error
 *
 * Performance: ~60µs for n=1024
 */
NIMCP_EXPORT bool phasor_power_spectrum(const neural_phasor_t* input,
                                        float* output,
                                        uint32_t n);

//=============================================================================
// Neural-Specific Operations
//=============================================================================

/**
 * @brief Compute phase-amplitude coupling (PAC) modulation index
 *
 * Measures how gamma amplitude is modulated by theta phase.
 * Classic measure of cross-frequency coupling in hippocampus.
 *
 * @param theta_phase Low-frequency phase signal
 * @param gamma_amplitude High-frequency amplitude envelope
 * @param n Number of samples
 * @return Modulation index in [0, 1] (0 = no coupling, 1 = perfect)
 *
 * Performance: ~2µs for n=1000
 *
 * Neuroscience application:
 * - Hippocampal theta-gamma coupling
 * - Working memory phase coding
 * - Cortical hierarchy communication
 */
NIMCP_EXPORT float phasor_pac_modulation_index(const neural_phasor_t* theta_phase,
                                                const float* gamma_amplitude,
                                                uint32_t n);

/**
 * @brief Hilbert transform to extract analytic signal
 *
 * Converts real signal to complex analytic signal for
 * instantaneous amplitude and phase extraction.
 *
 * @param real_signal Input real-valued signal
 * @param analytic_signal Output complex analytic signal
 * @param n Signal length
 * @return true on success, false on error
 *
 * Performance: ~80µs for n=1024 (includes FFT operations)
 */
NIMCP_EXPORT bool phasor_hilbert_transform(const float* real_signal,
                                            neural_phasor_t* analytic_signal,
                                            uint32_t n);

//=============================================================================
// Configuration and Utilities
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @return Default complex math configuration
 */
NIMCP_EXPORT complex_math_config_t complex_math_default_config(void);

/**
 * @brief Initialize complex math subsystem
 *
 * Call once at startup. Initializes SIMD, FFT caching, etc.
 *
 * @param config Configuration (NULL for defaults)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool complex_math_init(const complex_math_config_t* config);

/**
 * @brief Cleanup complex math subsystem
 *
 * Call at shutdown to free cached resources.
 */
NIMCP_EXPORT void complex_math_cleanup(void);

/**
 * @brief Check if SIMD acceleration is available
 *
 * @return true if SIMD (AVX/SSE) is available and enabled
 */
NIMCP_EXPORT bool complex_math_has_simd(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_COMPLEX_MATH_H
