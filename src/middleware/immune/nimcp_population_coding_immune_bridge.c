/**
 * @file nimcp_population_coding_immune_bridge.c
 * @brief Population Coding-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and population coding
 * WHY:  Biological realism - cytokines increase neural noise, inflammation affects tuning
 * HOW:  Monitor cytokine levels to modulate population codes, detect anomalies to trigger immune
 */

#include "middleware/immune/nimcp_population_coding_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/rng/nimcp_rand.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(population_coding_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get cytokine concentration from immune system
 *
 * WHAT: Query current cytokine level
 * WHY:  Need cytokine levels for noise/gain modulation
 * HOW:  Search immune system cytokines for type
 */
static float get_cytokine_level(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune || !immune->cytokines) return 0.0f;

    float total_concentration = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        if (immune->cytokines[i].type == type) {
            total_concentration += immune->cytokines[i].concentration;
        }
    }

    return nimcp_clampf(total_concentration, 0.0f, 1.0f);
}

/**
 * @brief Get max inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines population coding impact
 * HOW:  Query immune system inflammation sites
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune || !immune->inflammation_sites) {
        return INFLAMMATION_NONE;
    }

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }

    return max_level;
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation has different effects
 * HOW:  Find oldest active inflammation site
 */
static float get_inflammation_duration_sec(
    const brain_immune_system_t* immune,
    uint64_t current_time
) {
    if (!immune || !immune->inflammation_sites) return 0.0f;

    uint64_t oldest_start = current_time;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        if (immune->inflammation_sites[i].start_time < oldest_start) {
            oldest_start = immune->inflammation_sites[i].start_time;
        }
    }

    if (oldest_start == current_time) return 0.0f;
    return (float)(current_time - oldest_start) / 1000.0f;
}

/**
 * @brief Add Gaussian noise
 *
 * WHAT: Add noise to value using simple approximation
 * WHY:  Model neural noise increase from cytokines
 * HOW:  Use sum of uniform randoms (CLT approximation)
 */
static float add_gaussian_noise(float value, float noise_std) {
    if (noise_std <= 0.0f) return value;

    /* Simple noise approximation */
    float noise = 0.0f;
    for (int i = 0; i < 12; i++) {
        noise += nimcp_rand_uniform() - 0.5f;
    }
    noise *= noise_std;

    return value + noise;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int population_immune_default_config(population_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    config->enable_cytokine_noise_modulation = true;
    config->enable_inflammation_tuning_modulation = true;
    config->enable_population_anomaly_detection = true;
    config->enable_gain_modulation = true;
    config->enable_precision_restoration = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->anomaly_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Evidence-based thresholds */
    config->noise_trigger_threshold = POPULATION_NOISE_THRESHOLD;
    config->synchrony_threshold = POPULATION_SYNCHRONY_THRESHOLD;
    config->gain_anomaly_threshold = POPULATION_GAIN_ANOMALY_THRESHOLD;

    /* Baseline values */
    config->baseline_noise = 0.1f;      /* 10% baseline noise */
    config->baseline_gain = 1.0f;       /* Full gain */
    config->baseline_precision = 0.95f; /* 95% precision */

    return 0;
}

population_immune_bridge_t* population_immune_bridge_create(
    const population_immune_config_t* config,
    brain_immune_system_t* immune_system,
    population_coding_encoder_t population_encoder
) {
    /* Guard: require both systems */
    if (!immune_system || !population_encoder) {
        LOG_MODULE_ERROR("population_immune_bridge",
                  "Cannot create bridge without immune and population systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "population_immune_bridge_create: required parameter is NULL (immune_system, population_encoder)");
        return NULL;
    }

    /* Allocate bridge */
    population_immune_bridge_t* bridge = (population_immune_bridge_t*)
        nimcp_malloc(sizeof(population_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("population_immune_bridge",
                  "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(population_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->population_encoder = population_encoder;

    /* Apply configuration */
    population_immune_config_t default_cfg;
    if (!config) {
        population_immune_default_config(&default_cfg);
        config = &default_cfg;
    }

    bridge->enable_cytokine_noise_modulation = config->enable_cytokine_noise_modulation;
    bridge->enable_inflammation_tuning_modulation = config->enable_inflammation_tuning_modulation;
    bridge->enable_population_anomaly_detection = config->enable_population_anomaly_detection;
    bridge->enable_gain_modulation = config->enable_gain_modulation;
    bridge->enable_precision_restoration = config->enable_precision_restoration;

    /* Set baselines */
    bridge->baseline_noise = config->baseline_noise;
    bridge->baseline_gain = config->baseline_gain;
    bridge->baseline_precision = config->baseline_precision;
    bridge->baseline_synchrony = 0.7f; /* Default baseline synchrony */

    /* Initialize health metrics to baseline */
    bridge->health_metrics.precision = config->baseline_precision;
    bridge->health_metrics.gain = config->baseline_gain;
    bridge->health_metrics.noise = config->baseline_noise;
    bridge->health_metrics.synchrony = 0.7f;
    bridge->health_metrics.reliability = 1.0f;
    bridge->health_metrics.overall_health = 1.0f;
    bridge->health_metrics.fully_recovered = true;

    /* Initialize cytokine and inflammation effects to baseline (1.0 = no effect) */
    bridge->cytokine_effects.tuning_width_broadening = 1.0f;
    bridge->inflammation_state.tuning_broadening = 1.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "population_coding_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("population_immune_bridge",
                  "Bridge created successfully");
    return bridge;
}

void population_immune_bridge_destroy(population_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("population_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Population Coding Implementation
 * ============================================================================ */

int population_immune_apply_cytokine_effects(
    population_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_noise_modulation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_immune_apply_cytokine_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Get cytokine levels */
    float il1_level = get_cytokine_level(bridge->immune_system,
                                         BRAIN_CYTOKINE_IL1);
    float il6_level = get_cytokine_level(bridge->immune_system,
                                         BRAIN_CYTOKINE_IL6);
    float tnf_level = get_cytokine_level(bridge->immune_system,
                                         BRAIN_CYTOKINE_TNF);
    float ifn_level = get_cytokine_level(bridge->immune_system,
                                         BRAIN_CYTOKINE_IFN_GAMMA);
    float il10_level = get_cytokine_level(bridge->immune_system,
                                          BRAIN_CYTOKINE_IL10);

    /* Compute effects */
    bridge->cytokine_effects.il1_noise_increase =
        il1_level * CYTOKINE_IL1_NOISE_INCREASE;
    bridge->cytokine_effects.il6_precision_loss =
        il6_level * CYTOKINE_IL6_PRECISION_REDUCTION;
    bridge->cytokine_effects.tnf_gain_reduction =
        tnf_level * CYTOKINE_TNF_GAIN_REDUCTION;
    bridge->cytokine_effects.ifn_gamma_noise_increase =
        ifn_level * CYTOKINE_IFN_NOISE_INCREASE;
    bridge->cytokine_effects.il10_precision_restoration =
        il10_level * CYTOKINE_IL10_RESTORATION;

    /* Aggregate effects */
    bridge->cytokine_effects.total_noise_increase =
        bridge->cytokine_effects.il1_noise_increase +
        bridge->cytokine_effects.ifn_gamma_noise_increase;

    bridge->cytokine_effects.total_precision_loss =
        bridge->cytokine_effects.il6_precision_loss -
        bridge->cytokine_effects.il10_precision_restoration;

    bridge->cytokine_effects.total_gain_reduction =
        bridge->cytokine_effects.tnf_gain_reduction;

    /* Tuning broadening from pro-inflammatory cytokines */
    float pro_inflammatory = (il1_level + il6_level + tnf_level) / 3.0f;
    bridge->cytokine_effects.tuning_width_broadening =
        1.0f + (pro_inflammatory * 0.5f); /* Up to 50% broadening */

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int population_immune_apply_inflammation_effects(
    population_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_tuning_modulation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Get inflammation state */
    brain_inflammation_level_t level =
        get_max_inflammation_level(bridge->immune_system);
    uint64_t current_time = 0; /* Would get from system */
    float duration_sec =
        get_inflammation_duration_sec(bridge->immune_system, current_time);

    /* Update state */
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.inflammation_duration_sec = duration_sec;
    bridge->inflammation_state.is_chronic =
        (duration_sec >= CHRONIC_INFLAMMATION_THRESHOLD);

    /* Compute effects based on inflammation level */
    float level_factor = (float)level / (float)INFLAMMATION_STORM;

    bridge->inflammation_state.noise_level =
        bridge->baseline_noise + (level_factor * 0.5f);
    bridge->inflammation_state.precision_degradation =
        level_factor * 0.6f;
    bridge->inflammation_state.gain_reduction =
        level_factor * INFLAMMATION_GAIN_REDUCTION;
    bridge->inflammation_state.tuning_broadening =
        1.0f + (level_factor * INFLAMMATION_TUNING_BROADENING);
    bridge->inflammation_state.synchrony_loss =
        level_factor * 0.4f;

    /* Chronic inflammation has additional effects */
    if (bridge->inflammation_state.is_chronic) {
        bridge->inflammation_state.noise_level += 0.2f;
        bridge->inflammation_state.precision_degradation += 0.2f;
        bridge->inflammation_state.variability_increase = 0.3f;
    }

    /* Sparse code reliability */
    bridge->inflammation_state.sparse_code_reliability =
        1.0f - bridge->inflammation_state.precision_degradation;

    /* Vector magnitude reduction */
    bridge->inflammation_state.vector_magnitude_reduction =
        bridge->inflammation_state.gain_reduction;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

float population_immune_compute_noise(
    const population_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;

    float total_noise = bridge->baseline_noise;

    /* Add cytokine noise */
    total_noise += bridge->cytokine_effects.total_noise_increase;

    /* Add inflammation noise */
    total_noise += bridge->inflammation_state.noise_level - bridge->baseline_noise;

    return nimcp_clampf(total_noise, 0.0f, 1.0f);
}

float population_immune_compute_gain(
    const population_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;

    float gain = bridge->baseline_gain;

    /* Reduce by cytokine effects */
    gain -= bridge->cytokine_effects.total_gain_reduction;

    /* Reduce by inflammation */
    gain -= bridge->inflammation_state.gain_reduction;

    return nimcp_clampf(gain, 0.1f, 1.0f);
}

float population_immune_compute_precision(
    const population_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;

    float precision = bridge->baseline_precision;

    /* Reduce by cytokine effects */
    precision -= bridge->cytokine_effects.total_precision_loss;

    /* Reduce by inflammation */
    precision -= bridge->inflammation_state.precision_degradation;

    return nimcp_clampf(precision, 0.0f, 1.0f);
}

float population_immune_compute_tuning_broadening(
    const population_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;

    float broadening = 1.0f;

    /* Broaden from cytokines */
    broadening *= bridge->cytokine_effects.tuning_width_broadening;

    /* Broaden from inflammation */
    broadening *= bridge->inflammation_state.tuning_broadening;

    return nimcp_clampf(broadening, 1.0f, 3.0f);
}

/* ============================================================================
 * Population Coding → Immune Implementation
 * ============================================================================ */

int population_immune_detect_anomalies(
    population_immune_bridge_t* bridge,
    float noise,
    float synchrony,
    float gain
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_population_anomaly_detection) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Update trigger state */
    bridge->immune_trigger.noise_level = noise;
    bridge->immune_trigger.synchrony_loss = bridge->baseline_synchrony - synchrony;
    bridge->immune_trigger.gain_anomaly =
        fabsf(gain - bridge->baseline_gain);

    /* Check thresholds */
    bridge->immune_trigger.noise_triggered =
        (noise > POPULATION_NOISE_THRESHOLD);
    bridge->immune_trigger.synchrony_triggered =
        (synchrony < POPULATION_SYNCHRONY_THRESHOLD);
    bridge->immune_trigger.gain_triggered =
        (bridge->immune_trigger.gain_anomaly > POPULATION_GAIN_ANOMALY_THRESHOLD);

    /* Compute overall threat severity */
    float threat = 0.0f;
    if (bridge->immune_trigger.noise_triggered) threat += 0.4f;
    if (bridge->immune_trigger.synchrony_triggered) threat += 0.3f;
    if (bridge->immune_trigger.gain_triggered) threat += 0.3f;

    bridge->immune_trigger.threat_severity = threat;

    if (threat > 0.0f) {
        bridge->anomaly_detections++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int population_immune_trigger_from_anomaly(
    population_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->immune_system) return -1;
    if (bridge->immune_trigger.threat_severity < 0.3f) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Create epitope from anomaly signature */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);

    /* Encode anomaly metrics into epitope */
    uint32_t* metrics = (uint32_t*)epitope;
    metrics[0] = (uint32_t)(bridge->immune_trigger.noise_level * 1000.0f);
    metrics[1] = (uint32_t)(bridge->immune_trigger.synchrony_loss * 1000.0f);
    metrics[2] = (uint32_t)(bridge->immune_trigger.gain_anomaly * 1000.0f);

    /* Present antigen */
    uint32_t antigen_id;
    uint32_t severity = (uint32_t)(bridge->immune_trigger.threat_severity * 10.0f);
    severity = (severity < 1) ? 1 : ((severity > 10) ? 10 : severity);

    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        sizeof(epitope),
        severity,
        0, /* source_node */
        &antigen_id
    );

    if (result == 0) {
        bridge->immune_triggers++;
        LOG_MODULE_INFO("population_immune_bridge",
                  "Triggered immune response from population anomaly (severity=%u)",
                  severity);
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return result;
}

int population_immune_restoration_signal(
    population_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_precision_restoration) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_immune_restoration_signal: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Check if recovered */
    bool recovered = (bridge->health_metrics.overall_health > 0.9f) &&
                     (bridge->health_metrics.degradation_from_baseline < 0.1f);

    if (recovered && !bridge->health_metrics.fully_recovered) {
        /* Release IL-10 to signal restoration */
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL10,
            0, /* source_cell */
            0.5f, /* concentration */
            0, /* broadcast */
            &cytokine_id
        );

        bridge->health_metrics.fully_recovered = true;
        bridge->restorations++;

        LOG_MODULE_INFO("population_immune_bridge",
                  "Population coding restored - releasing IL-10");
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int population_immune_bridge_update(
    population_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Apply immune effects to population coding */
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    population_immune_apply_cytokine_effects(bridge);
    population_immune_apply_inflammation_effects(bridge);
    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Update health metrics */
    bridge->health_metrics.noise = population_immune_compute_noise(bridge);
    bridge->health_metrics.gain = population_immune_compute_gain(bridge);
    bridge->health_metrics.precision = population_immune_compute_precision(bridge);

    /* Compute overall health */
    float health = 0.0f;
    health += bridge->health_metrics.precision * 0.3f;
    health += bridge->health_metrics.gain * 0.3f;
    health += (1.0f - bridge->health_metrics.noise) * 0.2f;
    health += bridge->health_metrics.synchrony * 0.2f;

    bridge->health_metrics.overall_health = health;

    /* Compute degradation from baseline */
    float degradation = 0.0f;
    degradation += fabsf(bridge->health_metrics.precision - bridge->baseline_precision);
    degradation += fabsf(bridge->health_metrics.gain - bridge->baseline_gain);
    degradation += fabsf(bridge->health_metrics.noise - bridge->baseline_noise);

    bridge->health_metrics.degradation_from_baseline = degradation / 3.0f;

    /* Update recovery progress if degraded */
    if (bridge->health_metrics.overall_health < 0.9f) {
        bridge->health_metrics.fully_recovered = false;
        bridge->health_metrics.recovery_progress = health;
    }

    bridge->total_updates++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    /* Check for restoration */
    population_immune_restoration_signal(bridge);

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int population_immune_get_cytokine_effects(
    const population_immune_bridge_t* bridge,
    cytokine_population_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(*effects));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int population_immune_get_inflammation_state(
    const population_immune_bridge_t* bridge,
    inflammation_population_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(*state));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int population_immune_get_health_metrics(
    const population_immune_bridge_t* bridge,
    population_health_metrics_t* metrics
) {
    if (!bridge || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_immune_get_health_metrics: required parameter is NULL (bridge, metrics)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(metrics, &bridge->health_metrics, sizeof(*metrics));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

bool population_immune_is_degraded(const population_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return (bridge->health_metrics.overall_health < 0.7f);
}

float population_immune_get_health_score(
    const population_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;
    return bridge->health_metrics.overall_health;
}

/* ============================================================================
 * Advanced Integration Implementation
 * ============================================================================ */

int population_immune_modulate_vector_decoding(
    population_immune_bridge_t* bridge,
    const float* rates,
    const tuning_curve_t* tuning_curves,
    uint32_t num_neurons,
    float* noisy_rates_out
) {
    /* Guard clauses */
    if (!bridge || !rates || !noisy_rates_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_immune_modulate_vector_decoding: required parameter is NULL (bridge, rates, noisy_rates_out)");
        return -1;
    }
    if (num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_immune_modulate_vector_decoding: num_neurons is zero");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Get noise and gain */
    float noise = population_immune_compute_noise(bridge);
    float gain = population_immune_compute_gain(bridge);

    /* Apply gain and noise to each rate */
    for (uint32_t i = 0; i < num_neurons; i++) {
        float modulated = rates[i] * gain;
        modulated = add_gaussian_noise(modulated, noise * 10.0f); /* Scale noise */
        noisy_rates_out[i] = nimcp_clampf(modulated, 0.0f, 100.0f); /* Max 100Hz */
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int population_immune_modulate_synchrony(
    population_immune_bridge_t* bridge,
    const synchrony_result_t* baseline_synchrony,
    synchrony_result_t* modulated_synchrony_out
) {
    /* Guard clauses */
    if (!bridge || !baseline_synchrony || !modulated_synchrony_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_immune_modulate_synchrony: required parameter is NULL (bridge, baseline_synchrony, modulated_synchrony_out)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Copy baseline */
    memcpy(modulated_synchrony_out, baseline_synchrony, sizeof(*baseline_synchrony));

    /* Reduce synchrony based on inflammation */
    float synchrony_reduction =
        bridge->inflammation_state.synchrony_loss;

    modulated_synchrony_out->synchrony_index =
        baseline_synchrony->synchrony_index * (1.0f - synchrony_reduction);
    modulated_synchrony_out->mean_correlation =
        baseline_synchrony->mean_correlation * (1.0f - synchrony_reduction);
    modulated_synchrony_out->coherence =
        baseline_synchrony->coherence * (1.0f - synchrony_reduction);

    /* Increase lag variability */
    modulated_synchrony_out->peak_lag_ms =
        baseline_synchrony->peak_lag_ms * (1.0f + synchrony_reduction);

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int population_immune_modulate_sparse_code(
    population_immune_bridge_t* bridge,
    const bool* baseline_code,
    uint32_t num_neurons,
    bool* noisy_code_out
) {
    /* Guard clauses */
    if (!bridge || !baseline_code || !noisy_code_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_immune_modulate_sparse_code: required parameter is NULL (bridge, baseline_code, noisy_code_out)");
        return -1;
    }
    if (num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_immune_modulate_sparse_code: num_neurons is zero");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Get reliability */
    float reliability = bridge->inflammation_state.sparse_code_reliability;
    float flip_probability = 1.0f - reliability;

    /* Copy baseline and add noise */
    for (uint32_t i = 0; i < num_neurons; i++) {
        noisy_code_out[i] = baseline_code[i];

        /* Random bit flips based on reliability */
        float rand_val = nimcp_rand_uniform();
        if (rand_val < flip_probability) {
            noisy_code_out[i] = !noisy_code_out[i];
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define POPULATION_CODING_IMMUNE_MODULE_NAME "population_coding_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int population_coding_immune_connect_bio_async(population_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_POPULATION_CODING,
        .module_name = POPULATION_CODING_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("population_coding_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int population_coding_immune_disconnect_bio_async(population_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("population_coding_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool population_coding_immune_is_bio_async_connected(const population_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
