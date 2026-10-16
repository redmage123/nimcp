/**
 * @file nimcp_brain_oscillations.h
 * @brief Brain oscillation analysis using spectral methods
 *
 * WHAT: Analyze neural oscillations and brain waves in NIMCP brains
 * WHY:  Brain oscillations encode cognitive states, attention, sleep, learning
 * HOW:  FFT-based spectral analysis of neural activity patterns
 *
 * NEUROSCIENCE BACKGROUND:
 * Brain oscillations are rhythmic patterns of neural activity that:
 * - Coordinate communication between brain regions
 * - Encode cognitive states (attention, memory consolidation, sleep)
 * - Gate information flow through brain networks
 * - Support temporal binding of distributed representations
 *
 * OSCILLATION TYPES:
 * - Delta (1-4 Hz): Deep sleep, unconscious processes
 * - Theta (4-8 Hz): Memory consolidation, drowsiness, meditation
 * - Alpha (8-13 Hz): Relaxed wakefulness, eyes closed, default mode
 * - Beta (13-30 Hz): Active thinking, focus, motor control
 * - Gamma (30-100 Hz): Feature binding, consciousness, attention
 *
 * USE CASES:
 * - Detect attention states from gamma power
 * - Monitor memory consolidation via theta activity
 * - Identify sleep-like states from delta dominance
 * - Measure cross-frequency coupling for binding
 * - Analyze network synchrony and coherence
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6 (GPU P2P + Spectral Analysis)
 */

#ifndef NIMCP_BRAIN_OSCILLATIONS_H
#define NIMCP_BRAIN_OSCILLATIONS_H

#include "core/brain/nimcp_brain.h"
#include "utils/spectral/nimcp_fft.h"
#include "cognitive/nimcp_sleep_wake.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Oscillation analyzer for brain activity
 *
 * WHAT: Tracks neural activity over time and performs spectral analysis
 * WHY:  Detect oscillations, measure power in frequency bands
 * HOW:  Ring buffer for activity history + FFT for frequency analysis
 */
typedef struct brain_oscillation_analyzer_struct brain_oscillation_analyzer_t;

/**
 * @brief Brain wave power measurements
 */
typedef struct {
    float delta_power;    /**< Delta band power (1-4 Hz) */
    float theta_power;    /**< Theta band power (4-8 Hz) */
    float alpha_power;    /**< Alpha band power (8-13 Hz) */
    float beta_power;     /**< Beta band power (13-30 Hz) */
    float gamma_power;    /**< Gamma band power (30-100 Hz) */

    float total_power;    /**< Total power across all frequencies */
    float dominant_freq;  /**< Dominant frequency (Hz) */
    brain_wave_band_t dominant_band;  /**< Dominant brain wave band */
} brain_wave_power_t;

/**
 * @brief Cognitive state inferred from oscillations
 */
typedef enum {
    COGNITIVE_STATE_UNKNOWN,        /**< Not enough data to determine */
    COGNITIVE_STATE_DEEP_SLEEP,     /**< Delta dominance */
    COGNITIVE_STATE_LIGHT_SLEEP,    /**< Theta + delta */
    COGNITIVE_STATE_RELAXED,        /**< Alpha dominance */
    COGNITIVE_STATE_FOCUSED,        /**< Beta dominance */
    COGNITIVE_STATE_ATTENTIVE,      /**< Gamma + beta */
    COGNITIVE_STATE_CONSOLIDATING   /**< Theta dominance (memory) */
} cognitive_state_t;

/**
 * @brief Oscillation analysis results
 */
typedef struct {
    brain_wave_power_t wave_power;   /**< Power in each brain wave band */
    cognitive_state_t state;          /**< Inferred cognitive state */
    float state_confidence;           /**< Confidence in state (0-1) */

    // Spectral metrics
    float spectral_entropy;           /**< Entropy of power distribution */
    float peak_frequency;             /**< Frequency of peak power */
    float bandwidth;                  /**< Bandwidth of dominant peak */

    // Cross-frequency coupling
    float theta_gamma_coupling;       /**< Theta-gamma PAC (phase-amplitude) */
    float alpha_beta_coupling;        /**< Alpha-beta coupling */

    // Synchrony
    float synchrony;                  /**< Network synchrony (0-1) */
    float coherence;                  /**< Inter-regional coherence */
} oscillation_analysis_t;

//=============================================================================
// Analyzer Creation and Management
//=============================================================================

/**
 * WHAT: Create brain oscillation analyzer
 * WHY:  Track neural activity for spectral analysis
 * HOW:  Allocate ring buffer, FFT plan, and tracking structures
 *
 * @param brain Brain to analyze
 * @param window_size_ms Analysis window size in milliseconds (100-1000ms typical)
 * @param sampling_rate_hz Sampling rate in Hz (100-1000 Hz typical)
 * @return Analyzer handle or NULL on failure
 *
 * COMPLEXITY: O(N) where N = window_size_ms * sampling_rate_hz / 1000
 * MEMORY: O(N) for activity buffer + FFT workspace
 *
 * RECOMMENDED SETTINGS:
 * - Short-term (attention): 250ms window, 500Hz sampling
 * - Medium-term (task): 500ms window, 250Hz sampling
 * - Long-term (sleep): 1000ms window, 100Hz sampling
 *
 * NOTE: Window size must allow capturing lowest frequency of interest
 * - Delta (1 Hz) requires >= 1000ms window
 * - Theta (4 Hz) requires >= 250ms window
 */
brain_oscillation_analyzer_t* brain_oscillation_create(
    brain_t brain,
    uint32_t window_size_ms,
    uint32_t sampling_rate_hz
);

/**
 * WHAT: Destroy oscillation analyzer
 * WHY:  Free resources
 * HOW:  Free buffer, FFT plan, and analyzer structure
 *
 * @param analyzer Analyzer to destroy
 */
void brain_oscillation_destroy(brain_oscillation_analyzer_t* analyzer);

//=============================================================================
// Activity Recording
//=============================================================================

/**
 * WHAT: Record current brain activity snapshot
 * WHY:  Build up temporal history for spectral analysis
 * HOW:  Sample average neuron activation, add to ring buffer
 *
 * @param analyzer Oscillation analyzer
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(N) where N = number of neurons
 *
 * USAGE:
 * Call this regularly at sampling_rate_hz to build activity history.
 * For example, if sampling_rate = 250Hz, call every 4ms.
 */
bool brain_oscillation_record_activity(brain_oscillation_analyzer_t* analyzer);

/**
 * WHAT: Record custom activity value
 * WHY:  Inject pre-computed activity metric
 * HOW:  Add value to ring buffer
 *
 * @param analyzer Oscillation analyzer
 * @param activity Activity value (typically 0-1)
 * @return true on success, false on failure
 *
 * USE CASES:
 * - External sensors (EEG, MEG)
 * - Custom activity metrics
 * - Replay recorded data
 */
bool brain_oscillation_record_value(
    brain_oscillation_analyzer_t* analyzer,
    float activity
);

//=============================================================================
// Spectral Analysis
//=============================================================================

/**
 * WHAT: Analyze current oscillation patterns
 * WHY:  Extract brain wave power, cognitive state, and spectral metrics
 * HOW:  FFT on activity buffer, compute power in each band, infer state
 *
 * @param analyzer Oscillation analyzer
 * @param results Output analysis results
 * @return true on success, false on failure (e.g., insufficient data)
 *
 * COMPLEXITY: O(N log N) where N = buffer size (for FFT)
 *
 * REQUIRES:
 * - Activity buffer must be full (window_size samples recorded)
 * - At least one dominant oscillation present
 *
 * OUTPUT:
 * - Power in each brain wave band
 * - Inferred cognitive state
 * - Spectral metrics (entropy, peak frequency, etc.)
 */
bool brain_oscillation_analyze(
    brain_oscillation_analyzer_t* analyzer,
    oscillation_analysis_t* results
);

/**
 * WHAT: Get brain wave power in real-time
 * WHY:  Quick check of oscillatory state
 * HOW:  FFT + band power computation
 *
 * @param analyzer Oscillation analyzer
 * @param wave_power Output brain wave power
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(N log N)
 *
 * NOTE: Lighter-weight than full analyze() - just returns power
 */
bool brain_oscillation_get_wave_power(
    brain_oscillation_analyzer_t* analyzer,
    brain_wave_power_t* wave_power
);

/**
 * WHAT: Get inferred cognitive state
 * WHY:  Quick state classification from oscillations
 * HOW:  Analyze dominant frequency band, compare power ratios
 *
 * @param analyzer Oscillation analyzer
 * @param state Output cognitive state
 * @param confidence Output confidence (0-1)
 * @return true on success, false on failure
 *
 * HEURISTICS:
 * - Delta > 60% total → DEEP_SLEEP
 * - Theta > 40% total → CONSOLIDATING or LIGHT_SLEEP
 * - Alpha > 40% total → RELAXED
 * - Beta > 40% total → FOCUSED
 * - Gamma > 30% total → ATTENTIVE
 */
bool brain_oscillation_get_state(
    brain_oscillation_analyzer_t* analyzer,
    cognitive_state_t* state,
    float* confidence
);

//=============================================================================
// Cross-Frequency Coupling
//=============================================================================

/**
 * WHAT: Compute phase-amplitude coupling (PAC)
 * WHY:  Measure how low-freq phase modulates high-freq amplitude
 * HOW:  Extract phase of low freq, amplitude of high freq, correlate
 *
 * @param analyzer Oscillation analyzer
 * @param phase_band Low-frequency phase band (e.g., theta)
 * @param amplitude_band High-frequency amplitude band (e.g., gamma)
 * @return PAC strength (0-1), or -1.0 on error
 *
 * COMPLEXITY: O(N log N) for FFT + O(N) for correlation
 *
 * NEUROSCIENCE SIGNIFICANCE:
 * - Theta-gamma PAC: Memory encoding/retrieval
 * - Alpha-beta PAC: Attention gating
 * - Delta-gamma PAC: Sleep spindles
 *
 * PAC VALUES:
 * - 0.0-0.2: Weak/no coupling
 * - 0.2-0.4: Moderate coupling
 * - 0.4-1.0: Strong coupling
 */
float brain_oscillation_compute_pac(
    brain_oscillation_analyzer_t* analyzer,
    brain_wave_band_t phase_band,
    brain_wave_band_t amplitude_band
);

//=============================================================================
// Network Synchrony
//=============================================================================

/**
 * WHAT: Compute network synchrony using Kuramoto order parameter
 * WHY:  Measure phase synchronization of neural oscillations
 * HOW:  Kuramoto order parameter R = |⟨e^(iθ)⟩| from instantaneous phases
 *
 * @param analyzer Oscillation analyzer
 * @return Synchrony (0-1), or -1.0 on error
 *
 * COMPLEXITY: O(N log N) where N = buffer size
 *
 * SYNCHRONY VALUES:
 * - 0.0-0.3: Asynchronous (independent oscillations)
 * - 0.3-0.6: Partially synchronized
 * - 0.6-1.0: Highly synchronized (coherent oscillations)
 *
 * INTERPRETATION:
 * - High synchrony: Strong phase locking, focused processing
 * - Low synchrony: Independent phases, exploration
 */
float brain_oscillation_compute_synchrony(brain_oscillation_analyzer_t* analyzer);

/**
 * WHAT: Compute spectral coherence
 * WHY:  Measure consistency of oscillations in frequency domain
 * HOW:  Spectral concentration index (inverse of spectral entropy)
 *
 * @param analyzer Oscillation analyzer
 * @return Coherence (0-1), or -1.0 on error
 *
 * COMPLEXITY: O(N) where N = spectrum size
 *
 * COHERENCE VALUES:
 * - 0.0-0.3: Broadband/noisy activity
 * - 0.3-0.7: Moderate spectral concentration
 * - 0.7-1.0: Highly coherent oscillations
 *
 * INTERPRETATION:
 * - High coherence: Stable, precise rhythms
 * - Low coherence: Transient or irregular activity
 */
float brain_oscillation_compute_coherence(brain_oscillation_analyzer_t* analyzer);

/**
 * WHAT: Compute 3dB bandwidth around peak frequency
 * WHY:  Quantify sharpness of dominant oscillation
 * HOW:  Find frequency range where power drops to half of peak
 *
 * @param analyzer Oscillation analyzer
 * @param peak_freq Dominant frequency in Hz
 * @return Bandwidth in Hz, or -1.0 on error
 *
 * COMPLEXITY: O(N) where N = spectrum size
 *
 * INTERPRETATION:
 * - Narrow bandwidth (< 2 Hz): Sharp, stable oscillation
 * - Moderate bandwidth (2-5 Hz): Normal variability
 * - Wide bandwidth (> 5 Hz): Transient or unstable rhythm
 */
float brain_oscillation_compute_bandwidth(
    brain_oscillation_analyzer_t* analyzer,
    float peak_freq);

//=============================================================================
// Visualization and Export
//=============================================================================

/**
 * WHAT: Get power spectrum for plotting
 * WHY:  Visualize frequency content
 * HOW:  Return FFT power spectrum
 *
 * @param analyzer Oscillation analyzer
 * @param spectrum Output power spectrum [num_bins]
 * @param num_bins Number of frequency bins (output)
 * @return true on success, false on failure
 *
 * FREQUENCY BINS:
 * - bin[k] corresponds to frequency k * (sampling_rate / window_size)
 * - Use fft_bin_to_frequency() to convert bin index to Hz
 */
bool brain_oscillation_get_spectrum(
    brain_oscillation_analyzer_t* analyzer,
    float** spectrum,
    uint32_t* num_bins
);

/**
 * WHAT: Get raw activity buffer for export
 * WHY:  Export raw data for external analysis or plotting
 * HOW:  Return pointer to ring buffer
 *
 * @param analyzer Oscillation analyzer
 * @param buffer Output buffer pointer [window_size samples]
 * @param size Number of samples (output)
 * @return true on success, false on failure
 *
 * WARNING: Buffer is read-only. Do not modify.
 */
bool brain_oscillation_get_activity_buffer(
    brain_oscillation_analyzer_t* analyzer,
    const float** buffer,
    uint32_t* size
);

//=============================================================================
// Immune System Integration
//=============================================================================

/* Forward declaration for brain immune system */
typedef struct brain_immune_system brain_immune_system_t;

/**
 * @brief Immune-induced oscillation disruption parameters
 *
 * BIOLOGICAL BASIS:
 * Pro-inflammatory cytokines (IL-1β, IL-6, TNF-α) alter neural oscillations:
 * - Increase slow-wave (delta) activity during infection
 * - Reduce gamma oscillations (impaired cognition during illness)
 * - Disrupt theta-gamma coupling (memory deficits)
 * - Systemic inflammation → increased delta, decreased gamma/beta
 */
typedef struct {
    float delta_amplification;     /**< Delta band amplification (1.0-3.0) */
    float theta_suppression;       /**< Theta band suppression (0.5-1.0) */
    float gamma_suppression;       /**< Gamma band suppression (0.3-1.0) */
    float beta_suppression;        /**< Beta band suppression (0.5-1.0) */
    float coherence_disruption;    /**< Network coherence reduction (0.0-1.0) */
    float synchrony_disruption;    /**< Synchrony reduction (0.0-1.0) */
} immune_oscillation_effects_t;

/**
 * @brief Oscillation abnormality detection for immune surveillance
 *
 * BIOLOGICAL BASIS:
 * Abnormal EEG patterns can indicate infection, inflammation, or pathology:
 * - Excessive slow-wave activity → possible infection/inflammation
 * - Loss of gamma → cognitive impairment
 * - Reduced coherence → network dysfunction
 */
typedef struct {
    bool excessive_delta;          /**< Delta > 50% total power (infection marker) */
    bool suppressed_gamma;         /**< Gamma < 5% total power (cognitive impairment) */
    bool low_coherence;            /**< Coherence < 0.3 (network dysfunction) */
    bool low_synchrony;            /**< Synchrony < 0.2 (desynchronization) */
    float abnormality_score;       /**< Overall abnormality (0-1) */
    uint32_t consecutive_abnormal; /**< Consecutive abnormal readings */
} oscillation_abnormality_t;

/**
 * WHAT: Connect oscillation analyzer to immune system
 * WHY:  Enable bidirectional immune-oscillation modulation
 * HOW:  Store immune handle, register callbacks for cytokine effects
 *
 * @param analyzer Oscillation analyzer
 * @param immune_system Brain immune system
 * @return true on success, false on failure
 *
 * INTEGRATION:
 * - Immune → Oscillations: Cytokine release disrupts oscillations
 * - Oscillations → Immune: Abnormal patterns trigger immune surveillance
 *
 * BIOLOGICAL BASIS:
 * Neuroimmune interactions are bidirectional:
 * 1. Cytokines alter neural excitability and synchronization
 * 2. Abnormal neural activity signals potential pathology to immune system
 *
 * USAGE:
 * brain_oscillation_analyzer_t* osc = brain_oscillation_create(brain, 500, 250);
 * brain_immune_system_t* immune = brain_immune_create(NULL);
 * brain_oscillation_connect_immune(osc, immune);
 */
bool brain_oscillation_connect_immune(
    brain_oscillation_analyzer_t* analyzer,
    brain_immune_system_t* immune_system
);

/**
 * WHAT: Apply immune-induced oscillation effects
 * WHY:  Model cytokine-induced EEG changes
 * HOW:  Modulate band powers based on inflammation level
 *
 * @param analyzer Oscillation analyzer
 * @param inflammation_level Inflammation severity (0=none, 4=storm)
 * @param cytokine_concentration Pro-inflammatory cytokine level (0-1)
 * @return Computed oscillation effects
 *
 * COMPLEXITY: O(1)
 *
 * BIOLOGICAL MODEL:
 * - LOCAL inflammation: Minor gamma suppression, slight delta increase
 * - REGIONAL inflammation: Moderate effects, theta disruption
 * - SYSTEMIC inflammation: Strong delta increase, major gamma/beta suppression
 * - CYTOKINE STORM: Severe disruption, loss of coherent oscillations
 *
 * EFFECTS SCALE:
 * - Delta amplification: 1.0x (baseline) → 3.0x (storm)
 * - Gamma suppression: 1.0x (baseline) → 0.3x (storm)
 * - Beta suppression: 1.0x (baseline) → 0.5x (storm)
 * - Coherence/synchrony disruption: 0.0 (none) → 0.8 (severe)
 */
immune_oscillation_effects_t brain_oscillation_compute_immune_effects(
    brain_oscillation_analyzer_t* analyzer,
    uint32_t inflammation_level,
    float cytokine_concentration
);

/**
 * WHAT: Apply immune effects to current oscillation analysis
 * WHY:  Modulate oscillations based on immune state
 * HOW:  Scale band powers, reduce coherence/synchrony
 *
 * @param analyzer Oscillation analyzer
 * @param effects Immune-induced effects to apply
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 *
 * SIDE EFFECTS:
 * - Modifies cached wave power (delta amplified, gamma/beta suppressed)
 * - Reduces synchrony and coherence
 * - May change inferred cognitive state (e.g., focused → drowsy)
 *
 * BIOLOGICAL REALISM:
 * Models observed EEG changes during infection/inflammation:
 * - Increased slow-wave activity (delta) during sickness behavior
 * - Reduced gamma power during cognitive impairment
 * - Loss of network coherence during inflammatory states
 */
bool brain_oscillation_apply_immune_effects(
    brain_oscillation_analyzer_t* analyzer,
    const immune_oscillation_effects_t* effects
);

/**
 * WHAT: Detect abnormal oscillation patterns for immune surveillance
 * WHY:  Trigger immune response to neural dysfunction
 * HOW:  Check for excessive delta, suppressed gamma, low coherence
 *
 * @param analyzer Oscillation analyzer
 * @param abnormality Output abnormality detection results
 * @return true if abnormal patterns detected, false if normal
 *
 * COMPLEXITY: O(1)
 *
 * DETECTION CRITERIA:
 * - Excessive delta: > 50% of total power (infection marker)
 * - Suppressed gamma: < 5% of total power (cognitive impairment)
 * - Low coherence: < 0.3 (network dysfunction)
 * - Low synchrony: < 0.2 (severe desynchronization)
 *
 * ABNORMALITY SCORE:
 * Weighted sum of detected abnormalities:
 * - Each criterion adds 0.25 to score
 * - Score > 0.5 triggers immune surveillance
 *
 * BIOLOGICAL BASIS:
 * Abnormal EEG patterns can indicate:
 * - CNS infection (excessive slow waves)
 * - Neurodegenerative disease (loss of gamma)
 * - Autoimmune encephalitis (coherence loss)
 * - Metabolic dysfunction (synchrony loss)
 */
bool brain_oscillation_detect_abnormality(
    brain_oscillation_analyzer_t* analyzer,
    oscillation_abnormality_t* abnormality
);

/**
 * WHAT: Notify immune system of oscillation abnormality
 * WHY:  Trigger immune surveillance and response
 * HOW:  Present abnormal pattern as antigen to immune system
 *
 * @param analyzer Oscillation analyzer
 * @param abnormality Detected abnormality
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 *
 * IMMUNE RESPONSE:
 * - Creates antigen from oscillation pattern signature
 * - Severity scaled by abnormality score
 * - Triggers immune surveillance phase
 * - May activate B/T cells if pattern persists
 *
 * BIOLOGICAL ANALOGY:
 * Neural dysfunction (detected via abnormal oscillations) triggers
 * immune surveillance, similar to how damaged cells release danger
 * signals (DAMPs) that activate immune responses.
 */
bool brain_oscillation_notify_immune_abnormality(
    brain_oscillation_analyzer_t* analyzer,
    const oscillation_abnormality_t* abnormality
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Convert cognitive state to string
 * WHY:  Human-readable output
 * HOW:  Enum to string mapping
 *
 * @param state Cognitive state
 * @return String name
 */
const char* brain_oscillation_state_to_string(cognitive_state_t state);

/**
 * WHAT: Get recommended window size for frequency band
 * WHY:  Ensure window captures at least 3 cycles of lowest frequency
 * HOW:  window_ms >= 3000 / freq_low_hz
 *
 * @param band Brain wave band
 * @return Recommended window size in milliseconds
 *
 * EXAMPLES:
 * - Delta (1 Hz): 3000ms minimum
 * - Theta (4 Hz): 750ms minimum
 * - Alpha (8 Hz): 375ms minimum
 * - Beta (13 Hz): 230ms minimum
 * - Gamma (30 Hz): 100ms minimum
 */
uint32_t brain_oscillation_recommended_window(brain_wave_band_t band);

//=============================================================================
// Sleep Integration
//=============================================================================

/**
 * WHAT: Set current sleep state for oscillation modulation
 * WHY:  Sleep state directly determines brain oscillation patterns
 * HOW:  Store state, apply frequency/band modulation
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Beta/Gamma (13-100 Hz)
 * - DROWSY: Alpha (8-13 Hz)
 * - LIGHT_NREM: Theta (4-8 Hz) + sleep spindles (12-14 Hz)
 * - DEEP_NREM: Delta (0.5-4 Hz) - slow wave sleep
 * - REM: Theta + desynchronized
 *
 * @param analyzer Oscillation analyzer
 * @param state Current sleep state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void brain_oscillation_set_sleep_state(
    brain_oscillation_analyzer_t* analyzer,
    sleep_state_t state
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_OSCILLATIONS_H */
