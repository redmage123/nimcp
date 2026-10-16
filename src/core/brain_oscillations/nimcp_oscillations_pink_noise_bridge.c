/**
 * @file nimcp_oscillations_pink_noise_bridge.c
 * @brief Brain Oscillations-Pink Noise Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 */

#include "core/brain_oscillations/nimcp_oscillations_pink_noise_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_frequency_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(oscillations_pink_noise_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Validate configuration parameters
 *
 * WHAT: Check if configuration values are in valid ranges
 * WHY:  Prevent invalid noise generation
 * HOW:  Range checks on all parameters
 */
static bool validate_config(const oscillations_pink_noise_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_config: config is NULL");
        return false;
    }

    /* Check alpha exponent */
    if (config->alpha_exponent < 0.5f || config->alpha_exponent > 2.0f) {
        NIMCP_LOGGING_ERROR("Invalid alpha exponent (must be 0.5-2.0)");
        return false;
    }

    /* Check global amplitude */
    if (config->global_amplitude < 0.0f || config->global_amplitude > 0.5f) {
        NIMCP_LOGGING_ERROR("Invalid global amplitude (must be 0-0.5)");
        return false;
    }

    /* Check band amplitudes */
    if (config->delta_amplitude < 0.0f || config->delta_amplitude > 0.2f ||
        config->theta_amplitude < 0.0f || config->theta_amplitude > 0.2f ||
        config->alpha_amplitude < 0.0f || config->alpha_amplitude > 0.2f ||
        config->beta_amplitude < 0.0f || config->beta_amplitude > 0.2f ||
        config->gamma_amplitude < 0.0f || config->gamma_amplitude > 0.2f) {
        NIMCP_LOGGING_ERROR("Invalid band amplitude (must be 0-0.2)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_config: operation failed");
        return false;
    }

    /* Check coherence/synchrony reductions */
    if (config->coherence_reduction < 0.0f || config->coherence_reduction > 0.5f ||
        config->synchrony_reduction < 0.0f || config->synchrony_reduction > 0.5f) {
        NIMCP_LOGGING_ERROR("Invalid coherence/synchrony reduction (must be 0-0.5)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_config: operation failed");
        return false;
    }

    return true;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int oscillations_pink_noise_default_config(oscillations_pink_noise_config_t* config)
{
    /* WHAT: Fill config with biologically realistic defaults
     * WHY:  Easy initialization without parameter tuning
     * HOW:  Set evidence-based values from neuroscience literature
     */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_default_config: config is NULL");
        return -1;
    }

    /* Noise characteristics */
    config->alpha_exponent = PINK_NOISE_DEFAULT_ALPHA;
    config->global_amplitude = PINK_NOISE_DEFAULT_GLOBAL_AMP;

    /* Band-specific amplitudes */
    config->delta_amplitude = PINK_NOISE_DELTA_AMPLITUDE;
    config->theta_amplitude = PINK_NOISE_THETA_AMPLITUDE;
    config->alpha_amplitude = PINK_NOISE_ALPHA_AMPLITUDE;
    config->beta_amplitude = PINK_NOISE_BETA_AMPLITUDE;
    config->gamma_amplitude = PINK_NOISE_GAMMA_AMPLITUDE;

    /* Coherence/synchrony effects */
    config->coherence_reduction = PINK_NOISE_COHERENCE_REDUCTION;
    config->synchrony_reduction = PINK_NOISE_SYNCHRONY_REDUCTION;

    /* Multi-scale options */
    config->use_multiscale = false;
    config->num_scales = 5;

    /* Generation options */
    config->method = PINK_NOISE_VOSS;
    config->seed = 0;

    return 0;
}

oscillations_pink_noise_bridge_t* oscillations_pink_noise_bridge_create(
    const oscillations_pink_noise_config_t* config,
    brain_oscillation_analyzer_t* oscillation_analyzer)
{
    /* WHAT: Create pink noise bridge with oscillation analyzer
     * WHY:  Initialize 1/f noise injection into oscillations
     * HOW:  Allocate structure, create noise generator, link analyzer
     */

    /* Guard: NULL analyzer */
    if (!oscillation_analyzer) {
        NIMCP_LOGGING_ERROR("NULL oscillation analyzer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillation_analyzer is NULL");

        return NULL;
    }

    /* Use defaults if no config provided */
    oscillations_pink_noise_config_t default_cfg;
    if (!config) {
        oscillations_pink_noise_default_config(&default_cfg);
        config = &default_cfg;
    }

    /* Guard: invalid config */
    if (!validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_bridge_create: validate_config is NULL");
        return NULL;
    }

    /* Allocate bridge structure */
    oscillations_pink_noise_bridge_t* bridge =
        (oscillations_pink_noise_bridge_t*)nimcp_malloc(sizeof(oscillations_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, sizeof(oscillations_pink_noise_bridge_t));

    /* Link oscillation analyzer */
    bridge->oscillation_analyzer = oscillation_analyzer;

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "oscillations_pink_noise") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    /* Configure pink noise generator */
    bridge->noise_config = pink_noise_default_config();
    bridge->noise_config.alpha = config->alpha_exponent;
    bridge->noise_config.amplitude = config->global_amplitude;
    bridge->noise_config.min_frequency = PINK_NOISE_MIN_FREQUENCY;
    bridge->noise_config.max_frequency = PINK_NOISE_MAX_FREQUENCY;
    bridge->noise_config.sample_rate = NIMCP_OSC_SAMPLE_RATE_HZ; /* Default 1kHz, updated on first use */
    bridge->noise_config.method = config->method;
    bridge->noise_config.seed = config->seed;

    /* Create pink noise generator */
    bridge->pink_noise_gen = pink_noise_create(&bridge->noise_config);
    if (!bridge->pink_noise_gen) {
        NIMCP_LOGGING_ERROR("Failed to create pink noise generator");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oscillations_pink_noise_bridge_create: bridge->pink_noise_gen is NULL");
        return NULL;
    }

    /* Initialize injection parameters */
    bridge->injection_params.delta_amplitude = config->delta_amplitude;
    bridge->injection_params.theta_amplitude = config->theta_amplitude;
    bridge->injection_params.alpha_amplitude = config->alpha_amplitude;
    bridge->injection_params.beta_amplitude = config->beta_amplitude;
    bridge->injection_params.gamma_amplitude = config->gamma_amplitude;
    bridge->injection_params.global_amplitude = config->global_amplitude;
    bridge->injection_params.alpha_exponent = config->alpha_exponent;
    bridge->injection_params.coherence_reduction = config->coherence_reduction;
    bridge->injection_params.synchrony_reduction = config->synchrony_reduction;
    bridge->injection_params.current_noise_sample = 0.0f;

    /* Initialize state */
    bridge->enabled = true;
    bridge->use_multiscale = config->use_multiscale;
    bridge->sample_rate_hz = 1000.0f; /* Default, updated on first use */
    bridge->last_update_ms = 0;
    bridge->total_samples_injected = 0;

    /* Initialize statistics */
    bridge->total_updates = 0;
    bridge->noise_injections = 0;
    bridge->avg_noise_amplitude = 0.0f;
    bridge->peak_noise_amplitude = 0.0f;

    NIMCP_LOGGING_INFO("Created oscillations-pink noise bridge");
    return bridge;
}

void oscillations_pink_noise_bridge_destroy(oscillations_pink_noise_bridge_t* bridge)
{
    /* WHAT: Clean up bridge resources
     * WHY:  Proper deallocation
     * HOW:  Destroy noise generators, free structure
     */
    if (!bridge) return;

    /* Destroy pink noise generator */
    if (bridge->pink_noise_gen) {
        pink_noise_destroy(bridge->pink_noise_gen);
    }

    /* Destroy multiscale generator if used */
    if (bridge->multiscale_gen) {
        pink_noise_multiscale_destroy(bridge->multiscale_gen);
    }

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free structure */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed oscillations-pink noise bridge");
}

/* ============================================================================
 * Enable/Disable API Implementation
 * ============================================================================ */

int oscillations_pink_noise_enable(oscillations_pink_noise_bridge_t* bridge)
{
    /* WHAT: Enable pink noise injection
     * WHY:  Turn on biologically realistic noise
     * HOW:  Set enabled flag, reset generator
     */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_enable: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->enabled = true;

    /* Reset noise generator for fresh start */
    pink_noise_reset(bridge->pink_noise_gen, 0);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Enabled pink noise injection");
    return 0;
}

int oscillations_pink_noise_disable(oscillations_pink_noise_bridge_t* bridge)
{
    /* WHAT: Disable pink noise injection
     * WHY:  Compare noisy vs clean oscillations
     * HOW:  Clear enabled flag
     */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_disable: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->enabled = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disabled pink noise injection");
    return 0;
}

bool oscillations_pink_noise_is_enabled(const oscillations_pink_noise_bridge_t* bridge)
{
    /* WHAT: Check if pink noise is enabled
     * WHY:  Query current state
     * HOW:  Return enabled flag
     */
    if (!bridge) {
        return false;
    }

    return bridge->enabled;
}

/* ============================================================================
 * Pink Noise → Oscillations API Implementation
 * ============================================================================ */

int oscillations_pink_noise_inject(oscillations_pink_noise_bridge_t* bridge)
{
    /* WHAT: Inject pink noise into oscillation activity
     * WHY:  Add biologically realistic 1/f variability
     * HOW:  Generate noise sample, add to activity buffer
     */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_inject: bridge is NULL");
        return -1;
    }

    /* Guard: not enabled */
    if (!bridge->enabled) {
        return 0; /* Not an error, just skip */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Generate pink noise sample */
    float noise_sample = 0.0f;
    if (!pink_noise_generate_sample(bridge->pink_noise_gen, &noise_sample)) {
        NIMCP_LOGGING_ERROR("Failed to generate pink noise sample");
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "oscillations_pink_noise_inject: pink_noise_generate_sample is NULL");
        return -1;
    }

    /* Scale by global amplitude */
    noise_sample *= bridge->injection_params.global_amplitude;

    /* Store current sample */
    bridge->injection_params.current_noise_sample = noise_sample;

    /* Inject into oscillation analyzer activity buffer */
    if (!brain_oscillation_record_value(bridge->oscillation_analyzer, noise_sample)) {
        NIMCP_LOGGING_WARN("Failed to record noise value (analyzer may be full)");
        /* Not a fatal error, continue */
    }

    /* Update statistics */
    bridge->total_samples_injected++;
    bridge->noise_injections++;

    /* Update running average */
    float abs_noise = fabsf(noise_sample);
    bridge->avg_noise_amplitude =
        (bridge->avg_noise_amplitude * (bridge->noise_injections - 1) + abs_noise) /
        bridge->noise_injections;

    /* Update peak */
    if (abs_noise > bridge->peak_noise_amplitude) {
        bridge->peak_noise_amplitude = abs_noise;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oscillations_pink_noise_apply_effects(
    oscillations_pink_noise_bridge_t* bridge,
    oscillation_analysis_t* analysis)
{
    /* WHAT: Apply pink noise effects to oscillation metrics
     * WHY:  Noise naturally reduces coherence and synchrony
     * HOW:  Reduce coherence/synchrony proportional to noise level
     */
    if (!bridge || !analysis) {
        NIMCP_LOGGING_ERROR("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_apply_effects: required parameter is NULL (bridge, analysis)");
        return -1;
    }

    /* Guard: not enabled */
    if (!bridge->enabled) {
        return 0; /* No effects if disabled */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reduce coherence */
    float coherence_factor = 1.0f - bridge->injection_params.coherence_reduction;
    analysis->coherence *= coherence_factor;
    if (analysis->coherence < 0.0f) analysis->coherence = 0.0f;

    /* Reduce synchrony */
    float synchrony_factor = 1.0f - bridge->injection_params.synchrony_reduction;
    analysis->synchrony *= synchrony_factor;
    if (analysis->synchrony < 0.0f) analysis->synchrony = 0.0f;

    /* Note: Pink noise naturally creates these effects through variability
     * This explicit reduction models the statistical consequence of noise
     * on phase-locking and coherence measures
     */

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oscillations_pink_noise_bridge_update(
    oscillations_pink_noise_bridge_t* bridge,
    uint64_t delta_ms)
{
    /* WHAT: Update pink noise state
     * WHY:  Maintain continuous noise injection
     * HOW:  Check interval, inject if enabled
     */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_bridge_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update timestamp */
    bridge->last_update_ms += delta_ms;
    bridge->total_updates++;

    /* Check if we should inject noise */
    if (bridge->last_update_ms >= PINK_NOISE_UPDATE_INTERVAL_MS) {
        bridge->last_update_ms = 0;

        nimcp_mutex_unlock(bridge->base.mutex);

        /* Inject noise (this will lock/unlock internally) */
        return oscillations_pink_noise_inject(bridge);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Configuration API Implementation
 * ============================================================================ */

int oscillations_pink_noise_set_amplitude(
    oscillations_pink_noise_bridge_t* bridge,
    float amplitude)
{
    /* WHAT: Set global noise amplitude
     * WHY:  Adjust strength of 1/f background
     * HOW:  Update parameter, clamp to valid range
     */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_set_amplitude: bridge is NULL");
        return -1;
    }

    /* Clamp to valid range */
    if (amplitude < 0.0f) amplitude = 0.0f;
    if (amplitude > 0.5f) amplitude = 0.5f;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->injection_params.global_amplitude = amplitude;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int oscillations_pink_noise_set_band_amplitude(
    oscillations_pink_noise_bridge_t* bridge,
    brain_wave_band_t band,
    float amplitude)
{
    /* WHAT: Set band-specific noise amplitude
     * WHY:  Different bands may need different noise levels
     * HOW:  Update appropriate band parameter
     */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_set_band_amplitude: bridge is NULL");
        return -1;
    }

    /* Clamp to valid range */
    if (amplitude < 0.0f) amplitude = 0.0f;
    if (amplitude > 0.2f) amplitude = 0.2f;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update appropriate band */
    switch (band) {
        case BRAIN_WAVE_DELTA:
            bridge->injection_params.delta_amplitude = amplitude;
            break;
        case BRAIN_WAVE_THETA:
            bridge->injection_params.theta_amplitude = amplitude;
            break;
        case BRAIN_WAVE_ALPHA:
            bridge->injection_params.alpha_amplitude = amplitude;
            break;
        case BRAIN_WAVE_BETA:
            bridge->injection_params.beta_amplitude = amplitude;
            break;
        case BRAIN_WAVE_GAMMA:
            bridge->injection_params.gamma_amplitude = amplitude;
            break;
        default:
            NIMCP_LOGGING_ERROR("Invalid brain wave band");
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "oscillations_pink_noise_set_band_amplitude: operation failed");
            return -1;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oscillations_pink_noise_set_alpha(
    oscillations_pink_noise_bridge_t* bridge,
    float alpha)
{
    /* WHAT: Set spectral exponent (alpha)
     * WHY:  Change noise color (white/pink/red)
     * HOW:  Update config, recreate generator
     */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_set_alpha: bridge is NULL");
        return -1;
    }

    /* Clamp to valid range */
    if (alpha < 0.5f) alpha = 0.5f;
    if (alpha > 2.0f) alpha = 2.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update config */
    bridge->noise_config.alpha = alpha;
    bridge->injection_params.alpha_exponent = alpha;

    /* Recreate generator with new alpha */
    pink_noise_destroy(bridge->pink_noise_gen);
    bridge->pink_noise_gen = pink_noise_create(&bridge->noise_config);

    if (!bridge->pink_noise_gen) {
        NIMCP_LOGGING_ERROR("Failed to recreate pink noise generator");
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_set_alpha: bridge->pink_noise_gen is NULL");
        return -1;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int oscillations_pink_noise_get_params(
    const oscillations_pink_noise_bridge_t* bridge,
    pink_noise_injection_params_t* params)
{
    /* WHAT: Get current injection parameters
     * WHY:  Query configuration
     * HOW:  Copy parameters to output
     */
    if (!bridge || !params) {
        NIMCP_LOGGING_ERROR("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_get_params: required parameter is NULL (bridge, params)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *params = bridge->injection_params;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float oscillations_pink_noise_get_current_sample(
    const oscillations_pink_noise_bridge_t* bridge)
{
    /* WHAT: Get latest noise sample
     * WHY:  Monitor current noise value
     * HOW:  Return cached sample
     */
    if (!bridge) return 0.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float sample = bridge->injection_params.current_noise_sample;
    nimcp_mutex_unlock(bridge->base.mutex);

    return sample;
}

int oscillations_pink_noise_get_stats(
    const oscillations_pink_noise_bridge_t* bridge,
    uint32_t* total_injections,
    float* avg_amplitude,
    float* peak_amplitude)
{
    /* WHAT: Get statistics
     * WHY:  Monitor noise characteristics
     * HOW:  Copy statistics to outputs
     */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_pink_noise_get_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (total_injections) {
        *total_injections = bridge->noise_injections;
    }

    if (avg_amplitude) {
        *avg_amplitude = bridge->avg_noise_amplitude;
    }

    if (peak_amplitude) {
        *peak_amplitude = bridge->peak_noise_amplitude;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}
