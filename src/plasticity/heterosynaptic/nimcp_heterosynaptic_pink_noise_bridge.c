/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * heterosynaptic_pink_noise_bridge_create has zero callers in
 * production code. Wrapper around pink_noise_create that is unused.
 * Either wire a consumer or delete before the next major version.
 * Do not extend.
 */

/**
 * @file nimcp_heterosynaptic_pink_noise_bridge.c
 * @brief Heterosynaptic Plasticity-Pink Noise Integration Implementation
 * @version 1.0.0
 * @date 2025-12-21
 */

#include "plasticity/heterosynaptic/nimcp_heterosynaptic_pink_noise_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(heterosynaptic_pink_noise_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(hetero_pink_noise_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Compute competition rate from heterosynaptic statistics
 * WHY:  Assess whether competition is excessive
 * HOW:  Total competitions / time
 */
static float compute_competition_rate(const hetero_system_t* system) {
    if (!system) return 0.0f;

    uint64_t total_comps;
    uint64_t total_deps;
    float avg_neighbors;
    hetero_get_statistics(system, &total_comps, &total_deps, &avg_neighbors);

    /* Simple rate metric: total competitions (placeholder) */
    return (float)total_comps;
}

/**
 * WHAT: Compute saturation fraction
 * WHY:  Detect when many synapses approaching w_max
 * HOW:  Count synapses with weight > threshold
 */
static float compute_saturation_fraction(const hetero_system_t* system,
                                         float threshold) {
    if (!system || system->num_synapses == 0) return 0.0f;

    size_t saturated = 0;
    for (size_t i = 0; i < system->num_synapses; i++) {
        float norm_weight = system->synapses[i].weight / system->synapses[i].w_max;
        if (norm_weight > threshold) {
            saturated++;
        }
    }

    return (float)saturated / (float)system->num_synapses;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

/**
 * WHAT: Get default heterosynaptic-pink noise bridge configuration
 * WHY:  Provide biologically plausible starting parameters
 * HOW:  Fill struct with empirical defaults
 */
int hetero_pink_noise_default_config(hetero_pink_noise_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Modulation parameters */
    config->modulation.competition_alpha = HETERO_PINK_DEFAULT_COMPETITION_ALPHA;
    config->modulation.radius_alpha = HETERO_PINK_DEFAULT_RADIUS_ALPHA;
    config->modulation.wta_threshold_alpha = HETERO_PINK_DEFAULT_WTA_ALPHA;
    config->modulation.delay_alpha = HETERO_PINK_DEFAULT_DELAY_ALPHA;

    /* Enable all modulation types by default */
    config->modulation.enable_competition_noise = true;
    config->modulation.enable_radius_noise = true;
    config->modulation.enable_wta_noise = true;
    config->modulation.enable_delay_noise = true;
    config->modulation.enable_adaptive_amplitude = true;

    /* Base parameters (typical heterosynaptic defaults) */
    config->base_competition = HETERO_DEFAULT_DEPRESSION_FACTOR;
    config->base_radius = HETERO_DEFAULT_NEIGHBOR_RADIUS;
    config->base_wta_threshold = HETERO_WTA_THRESHOLD;
    config->base_delay = HETERO_DEFAULT_DELAY_MS;

    /* Feedback configuration */
    config->enable_feedback = true;
    config->feedback_gain = HETERO_PINK_ADAPTIVE_GAIN;
    config->high_competition_threshold = HETERO_PINK_HIGH_COMPETITION_THRESHOLD;
    config->saturation_threshold = HETERO_PINK_SATURATION_THRESHOLD;

    /* Pink noise configuration */
    config->pink_config = pink_noise_default_config();
    config->pink_config.alpha = 1.0f;           /* True pink noise */
    config->pink_config.amplitude = 0.05f;      /* 5% baseline modulation */
    config->pink_config.method = PINK_NOISE_VOSS;

    return 0;
}

/**
 * WHAT: Create heterosynaptic-pink noise bridge
 * WHY:  Initialize bidirectional integration
 * HOW:  Allocate bridge, connect systems, initialize state
 */
hetero_pink_noise_bridge_t* hetero_pink_noise_bridge_create(
    const hetero_pink_noise_config_t* config,
    hetero_system_t* hetero_system,
    pink_noise_generator_t pink_generator)
{
    if (!hetero_system) {
        NIMCP_LOGGING_ERROR("Heterosynaptic system required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_system is NULL");

        return NULL;
    }

    /* Allocate bridge */
    hetero_pink_noise_bridge_t* bridge = (hetero_pink_noise_bridge_t*)nimcp_malloc(
        sizeof(hetero_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(hetero_pink_noise_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hetero_pink_noise_default_config(&bridge->config);
    }

    /* Connect heterosynaptic system */
    bridge->hetero_system = hetero_system;

    /* Connect or create pink noise generator */
    if (pink_generator) {
        bridge->pink_generator = pink_generator;
        bridge->pink_generator_owned = false;
    } else {
        bridge->pink_generator = pink_noise_create(&bridge->config.pink_config);
        if (!bridge->pink_generator) {
            NIMCP_LOGGING_ERROR("Failed to create pink noise generator");
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hetero_pink_noise_bridge_create: bridge->pink_generator is NULL");
            return NULL;
        }
        bridge->pink_generator_owned = true;
    }

    /* Initialize noise state with base parameters */
    bridge->noise_state.base_competition = bridge->config.base_competition;
    bridge->noise_state.base_radius = bridge->config.base_radius;
    bridge->noise_state.base_wta_threshold = bridge->config.base_wta_threshold;
    bridge->noise_state.base_delay = bridge->config.base_delay;

    /* Set effective parameters to base initially */
    bridge->noise_state.effective_competition = bridge->config.base_competition;
    bridge->noise_state.effective_radius = bridge->config.base_radius;
    bridge->noise_state.effective_wta_threshold = bridge->config.base_wta_threshold;
    bridge->noise_state.effective_delay = bridge->config.base_delay;

    /* Initialize adaptive amplitude */
    bridge->adaptive_amplitude = bridge->config.pink_config.amplitude;

    /* Initialize mutex */
    nimcp_platform_mutex_init(bridge->base.mutex, false);

    /* Not enabled by default */
    bridge->enabled = false;

    NIMCP_LOGGING_INFO("Created heterosynaptic-pink noise bridge");
    return bridge;
}

/**
 * WHAT: Destroy heterosynaptic-pink noise bridge
 * WHY:  Clean up resources
 * HOW:  Free pink generator if owned, destroy mutex, free bridge
 */
void hetero_pink_noise_bridge_destroy(hetero_pink_noise_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy owned pink generator */
    if (bridge->pink_generator_owned && bridge->pink_generator) {
        pink_noise_destroy(bridge->pink_generator);
    }

    /* Destroy mutex */
    nimcp_platform_mutex_destroy(bridge->base.mutex);
    nimcp_free(bridge->base.mutex);
    bridge->base.mutex = NULL;

    /* Free bridge */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed heterosynaptic-pink noise bridge");
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

/**
 * WHAT: Connect to heterosynaptic system
 * WHY:  Attach to existing heterosynaptic plasticity
 * HOW:  Store handle, extract base parameters from config
 */
int hetero_pink_noise_connect_hetero(
    hetero_pink_noise_bridge_t* bridge,
    hetero_system_t* hetero_system)
{
    if (!bridge || !hetero_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_pink_noise_connect_hetero: required parameter is NULL (bridge, hetero_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->hetero_system = hetero_system;

    /* Extract base parameters from heterosynaptic config */
    bridge->noise_state.base_competition = hetero_system->config.depression_factor;
    bridge->noise_state.base_radius = hetero_system->config.neighbor_radius;
    bridge->noise_state.base_wta_threshold = hetero_system->config.wta_threshold;
    bridge->noise_state.base_delay = hetero_system->config.delay_ms;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Connect to pink noise generator
 * WHY:  Attach to existing noise source
 * HOW:  Store handle, mark as not owned
 */
int hetero_pink_noise_connect_generator(
    hetero_pink_noise_bridge_t* bridge,
    pink_noise_generator_t pink_generator)
{
    if (!bridge || !pink_generator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_pink_noise_connect_generator: required parameter is NULL (bridge, pink_generator)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Destroy old generator if we owned it */
    if (bridge->pink_generator_owned && bridge->pink_generator) {
        pink_noise_destroy(bridge->pink_generator);
    }

    bridge->pink_generator = pink_generator;
    bridge->pink_generator_owned = false;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Disconnect from all systems
 * WHY:  Clean shutdown without destroying bridge
 * HOW:  Clear handles, restore base parameters
 */
int hetero_pink_noise_disconnect(hetero_pink_noise_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Disable modulation */
    bridge->enabled = false;

    /* Restore base parameters */
    bridge->noise_state.effective_competition = bridge->noise_state.base_competition;
    bridge->noise_state.effective_radius = bridge->noise_state.base_radius;
    bridge->noise_state.effective_wta_threshold = bridge->noise_state.base_wta_threshold;
    bridge->noise_state.effective_delay = bridge->noise_state.base_delay;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Control API Implementation
 * ============================================================================ */

/**
 * WHAT: Enable pink noise modulation
 * WHY:  Start applying stochastic fluctuations to heterosynaptic parameters
 * HOW:  Set enabled flag
 */
int hetero_pink_noise_enable(hetero_pink_noise_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->enabled = true;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Enabled heterosynaptic pink noise modulation");
    return 0;
}

/**
 * WHAT: Disable pink noise modulation
 * WHY:  Stop stochastic modulation, restore deterministic behavior
 * HOW:  Clear enabled flag, restore base parameters
 */
int hetero_pink_noise_disable(hetero_pink_noise_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->enabled = false;

    /* Restore base parameters */
    bridge->noise_state.effective_competition = bridge->noise_state.base_competition;
    bridge->noise_state.effective_radius = bridge->noise_state.base_radius;
    bridge->noise_state.effective_wta_threshold = bridge->noise_state.base_wta_threshold;
    bridge->noise_state.effective_delay = bridge->noise_state.base_delay;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disabled heterosynaptic pink noise modulation");
    return 0;
}

/**
 * WHAT: Check if bridge is enabled
 */
bool hetero_pink_noise_is_enabled(const hetero_pink_noise_bridge_t* bridge) {
    return bridge ? bridge->enabled : false;
}

/* ============================================================================
 * Pink Noise → Heterosynaptic API Implementation
 * ============================================================================ */

/**
 * WHAT: Apply pink noise modulation to heterosynaptic parameters
 * WHY:  Update effective competition parameters with stochastic fluctuations
 * HOW:  Sample pink noise, compute modulated values
 */
int hetero_pink_noise_apply_modulation(hetero_pink_noise_bridge_t* bridge) {
    if (!bridge || !bridge->enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_pink_noise_apply_modulation: required parameter is NULL (bridge, bridge->enabled)");
        return -1;
    }
    if (!bridge->pink_generator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_pink_noise_apply_modulation: bridge->pink_generator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Generate pink noise sample */
    float noise;
    if (!pink_noise_generate_sample(bridge->pink_generator, &noise)) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hetero_pink_noise_apply_modulation: pink_noise_generate_sample is NULL");
        return -1;
    }

    bridge->noise_state.current_noise = noise;
    bridge->noise_samples_generated++;

    /* Apply modulation to each parameter */
    const hetero_pink_noise_modulation_t* mod = &bridge->config.modulation;

    /* Competition strength: η_eff = η_base × (1 + α × noise) */
    if (mod->enable_competition_noise) {
        float factor = 1.0f + mod->competition_alpha * noise;
        bridge->noise_state.effective_competition =
            bridge->noise_state.base_competition * factor;
        bridge->noise_state.effective_competition = nimcp_clampf(
            bridge->noise_state.effective_competition, 0.0f, 1.0f);
    } else {
        bridge->noise_state.effective_competition = bridge->noise_state.base_competition;
    }

    /* Spatial radius: r_eff = r_base × (1 + α × noise) */
    if (mod->enable_radius_noise) {
        float factor = 1.0f + mod->radius_alpha * noise;
        bridge->noise_state.effective_radius =
            bridge->noise_state.base_radius * factor;
        bridge->noise_state.effective_radius = nimcp_clampf(
            bridge->noise_state.effective_radius, 1.0f, 100.0f);
    } else {
        bridge->noise_state.effective_radius = bridge->noise_state.base_radius;
    }

    /* WTA threshold: θ_eff = θ_base + α × noise */
    if (mod->enable_wta_noise) {
        bridge->noise_state.effective_wta_threshold =
            bridge->noise_state.base_wta_threshold +
            mod->wta_threshold_alpha * noise;
        bridge->noise_state.effective_wta_threshold = nimcp_clampf(
            bridge->noise_state.effective_wta_threshold, 0.0f, 1.0f);
    } else {
        bridge->noise_state.effective_wta_threshold =
            bridge->noise_state.base_wta_threshold;
    }

    /* Delay: d_eff = d_base × (1 + α × noise) */
    if (mod->enable_delay_noise) {
        float factor = 1.0f + mod->delay_alpha * noise;
        bridge->noise_state.effective_delay =
            bridge->noise_state.base_delay * factor;
        bridge->noise_state.effective_delay = nimcp_clampf(
            bridge->noise_state.effective_delay, 10.0f, 5000.0f);
    } else {
        bridge->noise_state.effective_delay = bridge->noise_state.base_delay;
    }

    /* Update running averages */
    bridge->avg_effective_competition =
        (bridge->avg_effective_competition * 0.99f) +
        (bridge->noise_state.effective_competition * 0.01f);
    bridge->avg_effective_radius =
        (bridge->avg_effective_radius * 0.99f) +
        (bridge->noise_state.effective_radius * 0.01f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get effective competition strength with noise modulation
 */
float hetero_pink_noise_get_effective_competition(
    const hetero_pink_noise_bridge_t* bridge)
{
    return bridge ? bridge->noise_state.effective_competition : 0.0f;
}

/**
 * WHAT: Get effective competition radius with noise modulation
 */
float hetero_pink_noise_get_effective_radius(
    const hetero_pink_noise_bridge_t* bridge)
{
    return bridge ? bridge->noise_state.effective_radius : 0.0f;
}

/**
 * WHAT: Get effective WTA threshold with noise modulation
 */
float hetero_pink_noise_get_effective_wta_threshold(
    const hetero_pink_noise_bridge_t* bridge)
{
    return bridge ? bridge->noise_state.effective_wta_threshold : 0.0f;
}

/**
 * WHAT: Get effective depression delay with noise modulation
 */
float hetero_pink_noise_get_effective_delay(
    const hetero_pink_noise_bridge_t* bridge)
{
    return bridge ? bridge->noise_state.effective_delay : 0.0f;
}

/* ============================================================================
 * Heterosynaptic → Pink Noise API Implementation (Feedback)
 * ============================================================================ */

/**
 * WHAT: Update feedback from heterosynaptic competition state
 * WHY:  Adapt noise amplitude based on competition dynamics
 * HOW:  Compute competition metrics, set feedback flags
 */
int hetero_pink_noise_update_feedback(hetero_pink_noise_bridge_t* bridge) {
    if (!bridge || !bridge->hetero_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_pink_noise_update_feedback: required parameter is NULL (bridge, bridge->hetero_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    hetero_system_t* hetero = bridge->hetero_system;
    hetero_pink_noise_feedback_t* fb = &bridge->feedback;

    /* Compute competition rate */
    fb->competition_rate = compute_competition_rate(hetero);

    /* Compute saturation fraction */
    fb->saturation_fraction = compute_saturation_fraction(
        hetero, bridge->config.saturation_threshold);

    /* Get heterosynaptic statistics */
    uint64_t total_comps, total_deps;
    float avg_neighbors;
    hetero_get_statistics(hetero, &total_comps, &total_deps, &avg_neighbors);
    fb->avg_competitors = avg_neighbors;

    /* Compute average winner strength (placeholder - would need real data) */
    fb->avg_winner_strength = 0.7f;

    /* Set detection flags */
    fb->high_competition_detected =
        (fb->competition_rate > bridge->config.high_competition_threshold);
    fb->saturation_detected =
        (fb->saturation_fraction > bridge->config.saturation_threshold);
    fb->balanced_competition =
        (fb->competition_rate >= HETERO_PINK_BALANCED_COMPETITION_MIN) &&
        (fb->competition_rate <= HETERO_PINK_BALANCED_COMPETITION_MAX);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get competition feedback state
 */
int hetero_pink_noise_get_feedback(
    const hetero_pink_noise_bridge_t* bridge,
    hetero_pink_noise_feedback_t* feedback)
{
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_pink_noise_get_feedback: required parameter is NULL (bridge, feedback)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *feedback = bridge->feedback;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Adapt noise amplitude based on competition state
 * WHY:  Increase noise when competition pathological, decrease when healthy
 * HOW:  Adjust amplitude within bounds
 */
float hetero_pink_noise_adapt_amplitude(
    hetero_pink_noise_bridge_t* bridge,
    float adaptation_factor)
{
    if (!bridge) return 0.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Adjust amplitude */
    float delta = bridge->config.feedback_gain * adaptation_factor;
    bridge->adaptive_amplitude += delta;

    /* Clamp to valid range */
    bridge->adaptive_amplitude = nimcp_clampf(
        bridge->adaptive_amplitude,
        HETERO_PINK_MIN_AMPLITUDE,
        HETERO_PINK_MAX_AMPLITUDE
    );

    float result = bridge->adaptive_amplitude;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

/**
 * WHAT: Update heterosynaptic-pink noise bridge (both directions)
 * WHY:  Advance coupled state machine
 * HOW:  Sample noise, apply modulation, update feedback, adapt if enabled
 */
int hetero_pink_noise_bridge_update(
    hetero_pink_noise_bridge_t* bridge,
    uint64_t delta_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms;  /* Currently unused, reserved for future time-based logic */

    bridge->total_updates++;

    /* Apply pink noise modulation to heterosynaptic parameters */
    if (bridge->enabled) {
        if (hetero_pink_noise_apply_modulation(bridge) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hetero_pink_noise_bridge_update: validation failed");
            return -1;
        }
    }

    /* Update feedback from heterosynaptic state */
    if (bridge->config.enable_feedback) {
        if (hetero_pink_noise_update_feedback(bridge) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hetero_pink_noise_bridge_update: validation failed");
            return -1;
        }

        /* Adaptive amplitude adjustment */
        if (bridge->config.modulation.enable_adaptive_amplitude) {
            float adaptation_factor = 0.0f;

            if (bridge->feedback.high_competition_detected) {
                adaptation_factor = 1.0f;  /* Increase noise */
            } else if (bridge->feedback.saturation_detected) {
                adaptation_factor = 0.5f;  /* Moderate increase */
            } else if (bridge->feedback.balanced_competition) {
                adaptation_factor = -0.5f; /* Decrease noise */
            }

            hetero_pink_noise_adapt_amplitude(bridge, adaptation_factor);
        }
    }

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

/**
 * WHAT: Get current noise state
 */
int hetero_pink_noise_get_state(
    const hetero_pink_noise_bridge_t* bridge,
    hetero_pink_noise_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hetero_pink_noise_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *state = bridge->noise_state;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Get current adaptive amplitude
 */
float hetero_pink_noise_get_adaptive_amplitude(
    const hetero_pink_noise_bridge_t* bridge)
{
    return bridge ? bridge->adaptive_amplitude : 0.0f;
}

/**
 * WHAT: Get bridge statistics
 */
int hetero_pink_noise_get_statistics(
    const hetero_pink_noise_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* noise_samples,
    float* avg_competition,
    float* avg_radius)
{
    BRIDGE_BBB_VALIDATE(bridge, total_updates, sizeof(*total_updates));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    if (total_updates) *total_updates = bridge->total_updates;
    if (noise_samples) *noise_samples = bridge->noise_samples_generated;
    if (avg_competition) *avg_competition = bridge->avg_effective_competition;
    if (avg_radius) *avg_radius = bridge->avg_effective_radius;

    return 0;
}

/**
 * WHAT: Reset bridge state
 * WHY:  Clear statistics, restart noise sequence
 * HOW:  Reset counters, reseed pink generator
 */
int hetero_pink_noise_reset(hetero_pink_noise_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset statistics */
    bridge->total_updates = 0;
    bridge->noise_samples_generated = 0;
    bridge->avg_effective_competition = 0.0f;
    bridge->avg_effective_radius = 0.0f;

    /* Reset feedback */
    memset(&bridge->feedback, 0, sizeof(hetero_pink_noise_feedback_t));

    /* Reset adaptive amplitude to configured value */
    bridge->adaptive_amplitude = bridge->config.pink_config.amplitude;

    /* Reset pink noise generator */
    if (bridge->pink_generator) {
        pink_noise_reset(bridge->pink_generator, 0);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable inter-module messaging for noise modulation events
 * HOW:  Register with bio_router
 */
int hetero_pink_noise_connect_bio_async(hetero_pink_noise_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->base.bio_async_enabled) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HETEROSYNAPTIC_PINK_NOISE,
        .module_name = "heterosynaptic_pink_noise_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 */
int hetero_pink_noise_disconnect_bio_async(hetero_pink_noise_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->base.bio_async_enabled && bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
        bridge->base.bio_async_enabled = false;
        NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 */
bool hetero_pink_noise_is_bio_async_connected(
    const hetero_pink_noise_bridge_t* bridge)
{
    return bridge ? bridge->base.bio_async_enabled : false;
}
