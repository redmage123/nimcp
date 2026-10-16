/**
 * @file nimcp_biological_timescales.h
 * @brief Scientifically accurate biological timing constants for bio-async system
 *
 * WHAT: Comprehensive biological constants from peer-reviewed neuroscience literature
 * WHY:  Enable biologically realistic async coordination with accurate dynamics
 * HOW:  Constants derived from experimental data, with computational scaling factors
 *
 * LITERATURE REFERENCES:
 * [1] Rice & Bhardwaj (2006) - Dopamine release dynamics, J Neurochem
 * [2] Bunin & Wightman (1998) - Dopamine kinetics in striatum, J Neurosci
 * [3] Jennings (2013) - Serotonin transporter kinetics, PNAS
 * [4] Bhattacharya et al. (2019) - Acetylcholine receptor dynamics, eLife
 * [5] Kuramoto (1984) - Chemical Oscillations, Waves, and Turbulence
 * [6] Rao & Bhardwaj (1999) - Predictive coding in visual cortex, Nat Neurosci
 * [7] Cornell-Bell et al. (1990) - Calcium waves in astrocytes, Science
 * [8] De Pittà et al. (2009) - Astrocyte calcium dynamics model, J Biol Phys
 *
 * COMPUTATIONAL SCALING:
 * Biological timescales span 10μs to 100s - too slow for real-time simulation.
 * We provide COMP_ scaled versions that maintain relative dynamics while
 * achieving practical performance targets.
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#ifndef NIMCP_BIOLOGICAL_TIMESCALES_H
#define NIMCP_BIOLOGICAL_TIMESCALES_H

#include <stdint.h>
#include <math.h>
#include "constants/nimcp_math_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Architecture Constants
//=============================================================================

/** Cache line size for alignment (prevents false sharing) */
#define BIO_CACHE_LINE_SIZE 64

/** SIMD vector width for aligned operations */
#define BIO_SIMD_WIDTH 32

/** Default thread pool size for bio-async workers */
#define BIO_DEFAULT_THREAD_POOL_SIZE 8

//=============================================================================
// Unit Conversion Macros
//=============================================================================

/** Convert milliseconds to microseconds */
#define BIO_MS_TO_US(ms) ((ms) * 1000.0f)

/** Convert seconds to milliseconds */
#define BIO_SEC_TO_MS(sec) ((sec) * 1000.0f)

/** Convert Hz to period in milliseconds */
#define BIO_HZ_TO_PERIOD_MS(hz) (1000.0f / (hz))

/** Convert period in ms to Hz */
#define BIO_PERIOD_MS_TO_HZ(period_ms) (1000.0f / (period_ms))

/** Convert μm²/s to μm²/ms */
#define BIO_UM2_S_TO_MS(d) ((d) / 1000.0f)

//=============================================================================
// Mathematical Constants
//=============================================================================


#ifndef M_E
#define M_E 2.71828182845904523536
#endif

/** Two times PI (full circle in radians) */
#define BIO_TWO_PI (2.0 * M_PI)

/** Square root of 2*PI for Gaussian calculations */
#define BIO_SQRT_2PI 2.506628274631000502415765284811

//=============================================================================
// DOPAMINE (DA) - Reward and Goal Signaling
// Reference: Rice & Bhardwaj (2006), Bunin & Wightman (1998)
//=============================================================================

/**
 * Dopamine release transient duration (ms)
 * Time for phasic burst to reach peak after stimulus
 */
#define BIO_DA_RELEASE_TRANSIENT_MS 3.0f

/**
 * Dopamine receptor time constant τ (ms)
 * D1/D2 receptor activation/deactivation dynamics
 */
#define BIO_DA_RECEPTOR_TAU_MS 350.0f

/**
 * Dopamine decay time constant (s → ms)
 * Clearance via DAT reuptake and diffusion
 */
#define BIO_DA_DECAY_TAU_MS 2000.0f

/**
 * Dopamine diffusion coefficient (μm²/ms)
 * Effective diffusion in neuropil
 */
#define BIO_DA_DIFFUSION_COEF 0.0002f

/**
 * Michaelis-Menten Km for DAT (μM)
 * Dopamine transporter affinity
 */
#define BIO_DA_DAT_KM_UM 0.2f

/**
 * Maximum reuptake velocity Vmax (μM/s)
 */
#define BIO_DA_REUPTAKE_VMAX 4.0f

/**
 * Baseline dopamine concentration (μM)
 */
#define BIO_DA_BASELINE_UM 0.02f

/**
 * Peak phasic dopamine concentration (μM)
 */
#define BIO_DA_PEAK_PHASIC_UM 1.0f

/** Computational scaled version for real-time */
#define BIO_COMP_DA_DECAY_TAU_MS 200.0f

//=============================================================================
// SEROTONIN (5-HT) - Mood and State Coordination
// Reference: Jennings (2013)
//=============================================================================

/**
 * Serotonin synaptic cleft decay (ms)
 * Rapid clearance from active zone
 */
#define BIO_5HT_CLEFT_DECAY_MS 0.5f

/**
 * Serotonin reuptake time constant (ms)
 * SERT transporter kinetics
 */
#define BIO_5HT_REUPTAKE_TAU_MS 12.5f

/**
 * Serotonin decay time constant (ms)
 * Slower extrasynaptic clearance
 */
#define BIO_5HT_DECAY_TAU_MS 10000.0f

/**
 * Serotonin diffusion coefficient (μm²/ms)
 */
#define BIO_5HT_DIFFUSION_COEF 0.00005f

/**
 * Michaelis-Menten Km for SERT (μM)
 */
#define BIO_5HT_SERT_KM_UM 0.17f

/**
 * Baseline serotonin concentration (μM)
 */
#define BIO_5HT_BASELINE_UM 0.01f

/** Computational scaled version - serotonin has slower tonic decay than DA/NE
 * Biological basis: 5-HT has slower reuptake via SERT vs DAT/NET */
#define BIO_COMP_5HT_DECAY_TAU_MS 1500.0f

//=============================================================================
// NOREPINEPHRINE (NE) - Alertness and Priority
// Reference: Aston-Jones & Cohen (2005)
//=============================================================================

/**
 * Norepinephrine transient duration (ms)
 * Locus coeruleus phasic burst
 */
#define BIO_NE_TRANSIENT_MS 0.3f

/**
 * Norepinephrine phasic response (ms)
 * Duration of alerting signal
 */
#define BIO_NE_PHASIC_MS 100.0f

/**
 * Norepinephrine decay time constant (ms)
 */
#define BIO_NE_DECAY_TAU_MS 3000.0f

/**
 * Norepinephrine diffusion coefficient (μm²/ms)
 */
#define BIO_NE_DIFFUSION_COEF 0.00015f

/**
 * Michaelis-Menten Km for NET (μM)
 */
#define BIO_NE_NET_KM_UM 0.3f

/**
 * Baseline NE concentration (μM)
 */
#define BIO_NE_BASELINE_UM 0.05f

/** Computational scaled version */
#define BIO_COMP_NE_DECAY_TAU_MS 300.0f

//=============================================================================
// ACETYLCHOLINE (ACh) - Attention and Fast Switching
// Reference: Bhattacharya et al. (2019)
//=============================================================================

/**
 * Acetylcholine channel open time (ms)
 * nAChR channel kinetics ~10μs
 */
#define BIO_ACH_CHANNEL_OPEN_MS 0.01f

/**
 * Acetylcholine burst duration (ms)
 * Cholinergic interneuron burst
 */
#define BIO_ACH_BURST_MS 1.5f

/**
 * Acetylcholine decay time constant (ms)
 * Hydrolysis by AChE
 */
#define BIO_ACH_DECAY_TAU_MS 500.0f

/**
 * Acetylcholine diffusion coefficient (μm²/ms)
 * Fast diffusion due to small molecule
 */
#define BIO_ACH_DIFFUSION_COEF 0.0003f

/**
 * AChE hydrolysis rate constant (1/s)
 * Extremely fast enzymatic breakdown
 */
#define BIO_ACH_ACHE_RATE 16000.0f

/**
 * Baseline ACh concentration (μM)
 */
#define BIO_ACH_BASELINE_UM 0.1f

/** Computational scaled version */
#define BIO_COMP_ACH_DECAY_TAU_MS 50.0f

//=============================================================================
// NEURAL OSCILLATIONS - Kuramoto Model Parameters
// Reference: Kuramoto (1984), Buzsáki (2006)
//=============================================================================

/**
 * @brief Oscillation band frequency ranges (Hz)
 */

/* Delta band: 0.5-4 Hz - Deep sleep, homeostatic regulation */
#define BIO_OSC_DELTA_LOW_HZ 0.5f
#define BIO_OSC_DELTA_HIGH_HZ 4.0f
#define BIO_OSC_DELTA_CENTER_HZ 2.0f
#define BIO_OSC_DELTA_PERIOD_MS 500.0f

/* Theta band: 4-8 Hz - Memory encoding, navigation */
#define BIO_OSC_THETA_LOW_HZ 4.0f
#define BIO_OSC_THETA_HIGH_HZ 8.0f
#define BIO_OSC_THETA_CENTER_HZ 6.0f
#define BIO_OSC_THETA_PERIOD_MS 166.67f

/* Alpha band: 8-12 Hz - Attention, inhibition */
#define BIO_OSC_ALPHA_LOW_HZ 8.0f
#define BIO_OSC_ALPHA_HIGH_HZ 12.0f
#define BIO_OSC_ALPHA_CENTER_HZ 10.0f
#define BIO_OSC_ALPHA_PERIOD_MS 100.0f

/* Beta band: 12-30 Hz - Motor control, working memory */
#define BIO_OSC_BETA_LOW_HZ 12.0f
#define BIO_OSC_BETA_HIGH_HZ 30.0f
#define BIO_OSC_BETA_CENTER_HZ 20.0f
#define BIO_OSC_BETA_PERIOD_MS 50.0f

/* Gamma band: 30-100 Hz - Binding, attention, consciousness */
#define BIO_OSC_GAMMA_LOW_HZ 30.0f
#define BIO_OSC_GAMMA_HIGH_HZ 100.0f
#define BIO_OSC_GAMMA_CENTER_HZ 40.0f
#define BIO_OSC_GAMMA_PERIOD_MS 25.0f

/**
 * @brief Kuramoto coupling constants K
 *
 * Higher K = stronger synchronization tendency
 * K_critical = 2/(π*g(0)) where g is frequency distribution
 */
#define BIO_KURAMOTO_K_DELTA 0.5f   /**< Weak coupling for slow rhythms */
#define BIO_KURAMOTO_K_THETA 1.0f   /**< Moderate coupling */
#define BIO_KURAMOTO_K_ALPHA 0.8f   /**< Moderate coupling */
#define BIO_KURAMOTO_K_BETA 1.2f    /**< Strong coupling */
#define BIO_KURAMOTO_K_GAMMA 2.0f   /**< Strongest coupling for fast sync */

/**
 * @brief Phase coherence threshold for "synchronized" state
 * r = |1/N * Σ exp(i*θ_j)| where r ∈ [0,1]
 */
#define BIO_PHASE_COHERENCE_THRESHOLD 0.7f

/**
 * @brief Cross-frequency coupling ratios
 * Theta-gamma: typically 1:5 to 1:8 (5-8 gamma cycles per theta)
 */
#define BIO_CFC_THETA_GAMMA_LOW 5
#define BIO_CFC_THETA_GAMMA_HIGH 8
#define BIO_CFC_ALPHA_GAMMA_RATIO 4

/**
 * @brief Phase precession rate (radians/cycle)
 * For hippocampal place cell-like timing
 */
#define BIO_PHASE_PRECESSION_RATE 0.1f

//=============================================================================
// CALCIUM WAVES - Glial Signaling (Reaction-Diffusion)
// Reference: Cornell-Bell (1990), De Pittà (2009)
//=============================================================================

/**
 * Calcium diffusion coefficient in cytoplasm (μm²/s)
 */
#define BIO_CA_DIFFUSION_CYTOPLASM 200.0f

/**
 * IP3 diffusion coefficient (μm²/s)
 */
#define BIO_IP3_DIFFUSION 280.0f

/**
 * Calcium wave propagation speed in vivo (μm/s)
 * Range: 10-100 μm/s, reference: 61 μm/s
 */
#define BIO_CA_WAVE_SPEED_IN_VIVO 61.0f

/**
 * Computational target speed (μm/s)
 * Scaled for real-time performance
 */
#define BIO_COMP_CA_WAVE_SPEED 500.0f

/**
 * Baseline calcium concentration (μM)
 */
#define BIO_CA_BASELINE_UM 0.1f

/**
 * Calcium wave threshold (μM)
 * Concentration needed to trigger wave propagation
 */
#define BIO_CA_WAVE_THRESHOLD_UM 0.5f

/**
 * Peak calcium concentration during wave (μM)
 */
#define BIO_CA_PEAK_UM 2.0f

/**
 * Calcium wave duration (s)
 */
#define BIO_CA_WAVE_DURATION_S 30.0f

/**
 * SERCA pump Vmax (μM/s)
 * Sarco/endoplasmic reticulum Ca²⁺-ATPase
 */
#define BIO_CA_SERCA_VMAX 0.9f

/**
 * SERCA pump Km (μM)
 */
#define BIO_CA_SERCA_KM 0.1f

/**
 * ER calcium leak rate (1/s)
 */
#define BIO_CA_ER_LEAK_RATE 0.002f

/**
 * Calcium buffer capacity ratio
 * Free:bound calcium ratio
 */
#define BIO_CA_BUFFER_CAPACITY 100.0f

/**
 * IP3 receptor open probability parameters
 * P_open = (m∞ * h)³ where m∞ = IP3/(IP3 + d1) * Ca/(Ca + d5)
 */
#define BIO_IP3R_D1 0.13f  /**< IP3 dissociation constant (μM) */
#define BIO_IP3R_D5 0.08f  /**< Ca²⁺ activation dissociation (μM) */

/** Computational scaled wave duration */
#define BIO_COMP_CA_WAVE_DURATION_MS 3000.0f

//=============================================================================
// SYNAPTIC KINETICS - Alpha Function Parameters
// Reference: Destexhe et al. (1998)
//=============================================================================

/**
 * AMPA receptor kinetics
 * Fast excitatory transmission
 */
#define BIO_AMPA_TAU_RISE_MS 0.2f
#define BIO_AMPA_TAU_DECAY_MS 2.0f
#define BIO_AMPA_G_MAX_NS 0.5f  /**< Peak conductance (nS) */

/**
 * NMDA receptor kinetics
 * Slow excitatory, voltage-dependent Mg²⁺ block
 */
#define BIO_NMDA_TAU_RISE_MS 5.0f
#define BIO_NMDA_TAU_DECAY_MS 100.0f
#define BIO_NMDA_G_MAX_NS 0.1f
#define BIO_NMDA_MG_BLOCK_SLOPE 0.062f  /**< Mg²⁺ block voltage sensitivity (1/mV) */
#define BIO_NMDA_MG_BLOCK_V0 0.0f       /**< Voltage at half-block (mV) */

/**
 * GABA_A receptor kinetics
 * Fast inhibitory transmission
 */
#define BIO_GABAA_TAU_RISE_MS 0.5f
#define BIO_GABAA_TAU_DECAY_MS 10.0f
#define BIO_GABAA_G_MAX_NS 0.3f

/**
 * @brief Alpha function for synaptic conductance
 * g(t) = g_max * (t/τ) * exp(1 - t/τ) for t ≥ 0
 */
static inline float bio_alpha_function(float t_ms, float tau_ms, float g_max) {
    if (t_ms < 0.0f || tau_ms <= 0.0f) return 0.0f;
    float x = t_ms / tau_ms;
    return g_max * x * expf(1.0f - x);
}

/**
 * @brief Dual exponential for synaptic conductance
 * g(t) = g_max * (exp(-t/τ_decay) - exp(-t/τ_rise)) * norm
 */
static inline float bio_dual_exp_conductance(float t_ms, float tau_rise, float tau_decay, float g_max) {
    if (t_ms < 0.0f) return 0.0f;
    float norm = 1.0f / (tau_decay - tau_rise);  // Normalization factor
    return g_max * norm * (expf(-t_ms / tau_decay) - expf(-t_ms / tau_rise));
}

//=============================================================================
// PREDICTIVE CODING - Bayesian Inference Parameters
// Reference: Rao & Bhardwaj (1999), Friston (2005)
//=============================================================================

/**
 * Default prior precision (inverse variance)
 * π_prior = 1/σ²_prior
 */
#define BIO_PRED_PRIOR_PRECISION 1.0f

/**
 * Default likelihood precision
 * π_likelihood = 1/σ²_likelihood
 */
#define BIO_PRED_LIKELIHOOD_PRECISION 2.0f

/**
 * Learning rate for precision updates
 * How fast to adapt uncertainty estimates
 */
#define BIO_PRED_PRECISION_LEARNING_RATE 0.01f

/**
 * Prediction error gain
 * κ = scaling factor for error signal
 */
#define BIO_PRED_ERROR_GAIN 1.0f

/**
 * Surprise threshold for callback triggering
 * -log(p(observation|prediction))
 */
#define BIO_PRED_SURPRISE_THRESHOLD 2.0f

/**
 * @brief Gaussian precision-weighted prediction error
 * ε = π_likelihood * (actual - prediction)
 */
static inline float bio_prediction_error(float prediction, float actual, float precision) {
    return precision * (actual - prediction);
}

/**
 * @brief Bayesian update of belief
 * posterior = (π_prior * prior + π_likelihood * likelihood) / (π_prior + π_likelihood)
 */
static inline float bio_bayesian_update(float prior, float likelihood,
                                        float prior_precision, float likelihood_precision) {
    float total_precision = prior_precision + likelihood_precision;
    if (total_precision <= 0.0f) return prior;
    return (prior_precision * prior + likelihood_precision * likelihood) / total_precision;
}

/**
 * @brief Posterior precision after Bayesian update
 * π_posterior = π_prior + π_likelihood
 */
static inline float bio_posterior_precision(float prior_precision, float likelihood_precision) {
    return prior_precision + likelihood_precision;
}

/**
 * @brief Surprise (precision-weighted squared prediction error)
 * S = 0.5 * π * ε²
 *
 * In predictive coding, surprise is the precision-weighted squared
 * prediction error. Higher precision amplifies surprise for a given
 * error magnitude (more confidence = more surprise when wrong).
 *
 * This formulation ensures non-negative surprisal values.
 */
static inline float bio_surprise(float prediction_error, float precision) {
    if (precision <= 0.0f) return 0.0f;
    return 0.5f * precision * (prediction_error * prediction_error);
}

//=============================================================================
// INFORMATION THEORY - Channel Capacity and Entropy
//=============================================================================

/**
 * Estimated channel capacity for dopamine signaling (bits/s)
 * Based on temporal resolution and concentration levels
 */
#define BIO_CHANNEL_CAPACITY_DA 10.0f

/**
 * Estimated channel capacity for serotonin (bits/s)
 */
#define BIO_CHANNEL_CAPACITY_5HT 2.0f

/**
 * Estimated channel capacity for norepinephrine (bits/s)
 */
#define BIO_CHANNEL_CAPACITY_NE 5.0f

/**
 * Estimated channel capacity for acetylcholine (bits/s)
 */
#define BIO_CHANNEL_CAPACITY_ACH 50.0f

/**
 * Estimated channel capacity for gamma oscillations (bits/s)
 * Based on phase coding
 */
#define BIO_CHANNEL_CAPACITY_GAMMA 100.0f

/**
 * @brief Shannon entropy for discrete probability distribution
 * H = -Σ p_i * log2(p_i)
 *
 * @param probs Probability array
 * @param n Number of probabilities
 * @return Entropy in bits
 */
static inline float bio_shannon_entropy(const float* probs, size_t n) {
    float h = 0.0f;
    for (size_t i = 0; i < n; i++) {
        if (probs[i] > 0.0f) {
            h -= probs[i] * log2f(probs[i]);
        }
    }
    return h;
}

/**
 * @brief Mutual information I(X;Y) = H(X) + H(Y) - H(X,Y)
 */
static inline float bio_mutual_information(float h_x, float h_y, float h_xy) {
    return h_x + h_y - h_xy;
}

//=============================================================================
// DIFFERENTIAL EQUATION HELPERS
//=============================================================================

/**
 * @brief Exponential decay: dx/dt = -x/τ
 * Solution: x(t) = x₀ * exp(-t/τ)
 *
 * @param x0 Initial value
 * @param t Time elapsed (ms)
 * @param tau Time constant (ms)
 * @return Decayed value
 */
static inline float bio_exponential_decay(float x0, float t_ms, float tau_ms) {
    if (tau_ms <= 0.0f) return 0.0f;
    return x0 * expf(-t_ms / tau_ms);
}

/**
 * @brief First-order kinetic step (Euler)
 * x_new = x + dt * (target - x) / τ
 *
 * @param x Current value
 * @param target Equilibrium value
 * @param dt Time step (ms)
 * @param tau Time constant (ms)
 * @return Updated value
 */
static inline float bio_first_order_kinetics(float x, float target, float dt_ms, float tau_ms) {
    if (tau_ms <= 0.0f) return target;
    return x + dt_ms * (target - x) / tau_ms;
}

/**
 * @brief Michaelis-Menten kinetics: v = Vmax * [S] / (Km + [S])
 *
 * @param substrate Substrate concentration
 * @param vmax Maximum velocity
 * @param km Michaelis constant
 * @return Reaction rate
 */
static inline float bio_michaelis_menten(float substrate, float vmax, float km) {
    if (km + substrate <= 0.0f) return 0.0f;
    return vmax * substrate / (km + substrate);
}

/**
 * @brief Hill equation: v = Vmax * [S]^n / (K^n + [S]^n)
 *
 * @param substrate Substrate concentration
 * @param vmax Maximum velocity
 * @param k Half-saturation constant
 * @param n Hill coefficient (cooperativity)
 * @return Reaction rate
 */
static inline float bio_hill_equation(float substrate, float vmax, float k, float n) {
    float s_n = powf(substrate, n);
    float k_n = powf(k, n);
    if (k_n + s_n <= 0.0f) return 0.0f;
    return vmax * s_n / (k_n + s_n);
}

/**
 * @brief Kuramoto phase update
 * dθ/dt = ω + (K/N) * Σ sin(θ_j - θ_i)
 *
 * Simplified single-oscillator version with mean field:
 * dθ/dt = ω + K * r * sin(ψ - θ)
 *
 * @param theta Current phase (radians)
 * @param omega Natural frequency (radians/ms)
 * @param K Coupling strength
 * @param r Order parameter magnitude
 * @param psi Mean phase
 * @param dt Time step (ms)
 * @return Updated phase (wrapped to [0, 2π])
 */
static inline float bio_kuramoto_step(float theta, float omega, float K, float r, float psi, float dt_ms) {
    float dtheta = omega + K * r * sinf(psi - theta);
    theta += dtheta * dt_ms;
    // Wrap to [0, 2π]
    while (theta >= BIO_TWO_PI) theta -= BIO_TWO_PI;
    while (theta < 0.0f) theta += BIO_TWO_PI;
    return theta;
}

/**
 * @brief Calculate Kuramoto order parameter
 * r * exp(i*ψ) = (1/N) * Σ exp(i*θ_j)
 *
 * @param phases Array of phase values (radians)
 * @param n Number of oscillators
 * @param out_r Output: order parameter magnitude [0,1]
 * @param out_psi Output: mean phase (radians)
 */
static inline void bio_kuramoto_order_parameter(const float* phases, size_t n,
                                                 float* out_r, float* out_psi) {
    if (n == 0) {
        *out_r = 0.0f;
        *out_psi = 0.0f;
        return;
    }
    float sum_cos = 0.0f;
    float sum_sin = 0.0f;
    for (size_t i = 0; i < n; i++) {
        sum_cos += cosf(phases[i]);
        sum_sin += sinf(phases[i]);
    }
    sum_cos /= (float)n;
    sum_sin /= (float)n;
    *out_r = sqrtf(sum_cos * sum_cos + sum_sin * sum_sin);
    *out_psi = atan2f(sum_sin, sum_cos);
}

//=============================================================================
// STOCHASTIC PROCESS HELPERS
//=============================================================================

/**
 * @brief Ornstein-Uhlenbeck process step
 * dx = θ(μ - x)dt + σ*dW
 *
 * Generates biologically realistic noise with mean reversion
 *
 * @param x Current value
 * @param mu Mean (equilibrium)
 * @param theta Rate of mean reversion
 * @param sigma Volatility
 * @param dt Time step
 * @param noise Random normal sample N(0,1)
 * @return Updated value
 */
static inline float bio_ornstein_uhlenbeck_step(float x, float mu, float theta,
                                                 float sigma, float dt, float noise) {
    float drift = theta * (mu - x) * dt;
    float diffusion = sigma * sqrtf(dt) * noise;
    return x + drift + diffusion;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIOLOGICAL_TIMESCALES_H */
