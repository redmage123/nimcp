/**
 * @file nimcp_lnn_immune.c
 * @brief LNN immune system integration implementation
 * @version 1.0.0
 * @date 2025-12-20
 */

#include "lnn/nimcp_lnn_immune.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_immune)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute L2 norm of state vector
 *
 * WHAT: Calculate magnitude of network state
 * WHY:  Detect state explosion/collapse
 * HOW:  Sum of squares, take sqrt
 */
static float compute_state_norm(const lnn_network_t* network)
{
    if (!network || !network->layers || network->n_layers == 0) {
        return 0.0f;
    }

    float sum_sq = 0.0f;
    uint32_t total_neurons = 0;

    for (uint32_t i = 0; i < network->n_layers; i++) {
        lnn_layer_t* layer = network->layers[i];
        if (!layer || !layer->neurons) continue;

        for (uint32_t j = 0; j < layer->n_neurons; j++) {
            float x = layer->neurons[j].x;
            sum_sq += x * x;
            total_neurons++;
        }
    }

    return (total_neurons > 0) ? sqrtf(sum_sq / total_neurons) : 0.0f;
}

/**
 * @brief Check for NaN/Inf in state
 *
 * WHAT: Scan network for invalid floating point values
 * WHY:  NaN/Inf indicate critical failure
 * HOW:  Use isnan()/isinf() on all neuron states
 */
static bool check_state_validity(const lnn_network_t* network, bool* has_nan, bool* has_inf)
{
    if (!network || !network->layers || !has_nan || !has_inf) {
        return false;
    }

    *has_nan = false;
    *has_inf = false;

    for (uint32_t i = 0; i < network->n_layers; i++) {
        lnn_layer_t* layer = network->layers[i];
        if (!layer || !layer->neurons) continue;

        for (uint32_t j = 0; j < layer->n_neurons; j++) {
            float x = layer->neurons[j].x;
            float tau = layer->neurons[j].tau_current;

            if (isnan(x) || isnan(tau)) {
                *has_nan = true;
                return false;
            }
            if (isinf(x) || isinf(tau)) {
                *has_inf = true;
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Check tau values across network
 *
 * WHAT: Verify τ values within bounds
 * WHY:  Detect τ explosion/collapse
 * HOW:  Check each neuron's τ against thresholds
 */
static lnn_instability_type_t check_tau_health(
    const lnn_network_t* network,
    float tau_min,
    float tau_max)
{
    if (!network || !network->layers) {
        return LNN_INSTABILITY_NONE;
    }

    for (uint32_t i = 0; i < network->n_layers; i++) {
        lnn_layer_t* layer = network->layers[i];
        if (!layer || !layer->neurons) continue;

        for (uint32_t j = 0; j < layer->n_neurons; j++) {
            float tau = layer->neurons[j].tau_current;

            if (tau > tau_max) {
                return LNN_INSTABILITY_TAU_EXPLOSION;
            }
            if (tau < tau_min) {
                return LNN_INSTABILITY_TAU_COLLAPSE;
            }
        }
    }

    return LNN_INSTABILITY_NONE;
}

/**
 * @brief Map inflammation level to tau scale
 *
 * WHAT: Convert inflammation to time constant scaling factor
 * WHY:  Higher inflammation → slower dynamics (higher τ)
 * HOW:  Lookup table or custom mapping
 */
static float inflammation_to_tau_scale(
    brain_inflammation_level_t inflammation,
    const lnn_immune_config_t* config)
{
    if (!config) {
        /* Use defaults */
        switch (inflammation) {
            case INFLAMMATION_NONE:     return LNN_IMMUNE_TAU_SCALE_NONE;
            case INFLAMMATION_LOCAL:    return LNN_IMMUNE_TAU_SCALE_LOCAL;
            case INFLAMMATION_REGIONAL: return LNN_IMMUNE_TAU_SCALE_REGIONAL;
            case INFLAMMATION_SYSTEMIC: return LNN_IMMUNE_TAU_SCALE_SYSTEMIC;
            case INFLAMMATION_STORM:    return LNN_IMMUNE_TAU_SCALE_STORM;
            default:                    return 1.0f;
        }
    }

    /* Use custom mapping */
    uint32_t idx = (uint32_t)inflammation;
    if (idx >= 5) idx = 0;
    return config->tau_inflammation_scales[idx];
}

/**
 * @brief Map inflammation level to LR factor
 *
 * WHAT: Convert inflammation to learning rate reduction
 * WHY:  Suppress learning during illness
 * HOW:  Same as training immune LR factors
 */
static float inflammation_to_lr_factor(
    brain_inflammation_level_t inflammation,
    const lnn_immune_config_t* config)
{
    if (!config) {
        /* Match training immune defaults */
        switch (inflammation) {
            case INFLAMMATION_NONE:     return 1.00f;
            case INFLAMMATION_LOCAL:    return 0.95f;
            case INFLAMMATION_REGIONAL: return 0.80f;
            case INFLAMMATION_SYSTEMIC: return 0.50f;
            case INFLAMMATION_STORM:    return 0.10f;
            default:                    return 1.0f;
        }
    }

    /* Use custom mapping */
    uint32_t idx = (uint32_t)inflammation;
    if (idx >= 5) idx = 0;
    return config->lr_inflammation_factors[idx];
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int lnn_immune_config_default(lnn_immune_config_t* config)
{
    /* Guard clause */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null config pointer in lnn_immune_config_default");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Instability thresholds */
    config->state_explosion_threshold = LNN_IMMUNE_STATE_EXPLOSION_DEFAULT;
    config->state_collapse_threshold = LNN_IMMUNE_STATE_COLLAPSE_DEFAULT;
    config->tau_max = LNN_IMMUNE_TAU_MAX_DEFAULT;
    config->tau_min = LNN_IMMUNE_TAU_MIN_DEFAULT;
    config->gradient_explosion_threshold = LNN_IMMUNE_GRAD_EXPLOSION_DEFAULT;
    config->gradient_vanishing_threshold = LNN_IMMUNE_GRAD_VANISHING_DEFAULT;

    /* Immune response */
    config->auto_report_instabilities = true;
    config->instability_severity[LNN_INSTABILITY_NONE] = 0;
    config->instability_severity[LNN_INSTABILITY_NAN_STATE] = LNN_IMMUNE_SEVERITY_NAN_STATE;
    config->instability_severity[LNN_INSTABILITY_INF_STATE] = LNN_IMMUNE_SEVERITY_INF_STATE;
    config->instability_severity[LNN_INSTABILITY_STATE_EXPLOSION] = LNN_IMMUNE_SEVERITY_STATE_EXPLOSION;
    config->instability_severity[LNN_INSTABILITY_STATE_COLLAPSE] = LNN_IMMUNE_SEVERITY_STATE_COLLAPSE;
    config->instability_severity[LNN_INSTABILITY_TAU_EXPLOSION] = LNN_IMMUNE_SEVERITY_TAU_EXPLOSION;
    config->instability_severity[LNN_INSTABILITY_TAU_COLLAPSE] = LNN_IMMUNE_SEVERITY_TAU_COLLAPSE;
    config->instability_severity[LNN_INSTABILITY_GRADIENT_EXPLOSION] = LNN_IMMUNE_SEVERITY_GRAD_EXPLOSION;
    config->instability_severity[LNN_INSTABILITY_GRADIENT_VANISHING] = LNN_IMMUNE_SEVERITY_GRAD_VANISHING;
    config->instability_severity[LNN_INSTABILITY_ODE_DIVERGENCE] = LNN_IMMUNE_SEVERITY_ODE_DIVERGENCE;

    /* Cytokine modulation */
    config->enable_tau_modulation = true;
    config->enable_lr_modulation = true;
    config->enable_state_damping = true;

    /* Inflammation -> tau scale (slow down during inflammation) */
    config->tau_inflammation_scales[INFLAMMATION_NONE] = LNN_IMMUNE_TAU_SCALE_NONE;
    config->tau_inflammation_scales[INFLAMMATION_LOCAL] = LNN_IMMUNE_TAU_SCALE_LOCAL;
    config->tau_inflammation_scales[INFLAMMATION_REGIONAL] = LNN_IMMUNE_TAU_SCALE_REGIONAL;
    config->tau_inflammation_scales[INFLAMMATION_SYSTEMIC] = LNN_IMMUNE_TAU_SCALE_SYSTEMIC;
    config->tau_inflammation_scales[INFLAMMATION_STORM] = LNN_IMMUNE_TAU_SCALE_STORM;

    /* Inflammation -> LR factor (reduce learning during inflammation) */
    config->lr_inflammation_factors[INFLAMMATION_NONE] = 1.00f;
    config->lr_inflammation_factors[INFLAMMATION_LOCAL] = 0.95f;
    config->lr_inflammation_factors[INFLAMMATION_REGIONAL] = 0.80f;
    config->lr_inflammation_factors[INFLAMMATION_SYSTEMIC] = 0.50f;
    config->lr_inflammation_factors[INFLAMMATION_STORM] = 0.10f;

    /* Bio-async */
    config->enable_bio_async = false;

    return NIMCP_SUCCESS;
}

lnn_immune_bridge_t* lnn_immune_bridge_create(
    lnn_network_t* network,
    const lnn_immune_config_t* config)
{
    /* Guard clause */
    if (!network) {
        NIMCP_LOGGING_ERROR("NULL network pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_immune_bridge_create");
        return NULL;
    }

    /* Allocate bridge */
    lnn_immune_bridge_t* bridge = (lnn_immune_bridge_t*)nimcp_malloc(sizeof(lnn_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate LNN immune bridge");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(lnn_immune_bridge_t),
                          "Failed to allocate LNN immune bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(lnn_immune_bridge_t));

    /* Store network */
    bridge->network = network;

    /* Copy or default config */
    if (config) {
        memcpy(&bridge->config, config, sizeof(lnn_immune_config_t));
    } else {
        lnn_immune_config_default(&bridge->config);
    }

    /* Initialize cytokine effects */
    bridge->cytokine_effects.tau_scale = 1.0f;
    bridge->cytokine_effects.tau_offset = 0.0f;
    bridge->cytokine_effects.lr_factor = 1.0f;
    bridge->cytokine_effects.gradient_scale = 1.0f;
    bridge->cytokine_effects.state_damping = 1.0f;
    bridge->cytokine_effects.noise_injection = 0.0f;
    bridge->cytokine_effects.inflammation = INFLAMMATION_NONE;
    bridge->cytokine_effects.valid = true;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(lnn_immune_stats_t));
    bridge->stats.current_inflammation = INFLAMMATION_NONE;
    bridge->stats.current_tau_scale = 1.0f;
    bridge->stats.current_lr_factor = 1.0f;

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "lnn_immune") != 0) {
        NIMCP_LOGGING_WARN("Failed to init bridge base for LNN immune bridge");
    }

    NIMCP_LOGGING_INFO("Created LNN immune bridge");
    return bridge;
}

void lnn_immune_bridge_destroy(lnn_immune_bridge_t* bridge)
{
    /* Guard clause */
    if (!bridge) {
        return;
    }

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed LNN immune bridge");
}

/* ============================================================================
 * Integration API Implementation
 * ============================================================================ */

int lnn_immune_connect_brain_immune(
    lnn_immune_bridge_t* bridge,
    brain_immune_system_t* brain_immune)
{
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null bridge pointer in lnn_immune_connect_brain_immune");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!brain_immune) {
        NIMCP_LOGGING_ERROR("NULL brain_immune pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null brain_immune pointer in lnn_immune_connect_brain_immune");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Thread-safe update */
    if (bridge->base.mutex) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    bridge->brain_immune = brain_immune;

    if (bridge->base.mutex) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    NIMCP_LOGGING_INFO("Connected LNN immune bridge to brain immune system");
    return NIMCP_SUCCESS;
}

int lnn_immune_connect_training_immune(
    lnn_immune_bridge_t* bridge,
    training_immune_system_t* training_immune)
{
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null bridge pointer in lnn_immune_connect_training_immune");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!training_immune) {
        NIMCP_LOGGING_ERROR("NULL training_immune pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null training_immune pointer in lnn_immune_connect_training_immune");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Thread-safe update */
    if (bridge->base.mutex) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    bridge->training_immune = training_immune;

    if (bridge->base.mutex) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    NIMCP_LOGGING_INFO("Connected LNN immune bridge to training immune system");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Stability Monitoring API Implementation
 * ============================================================================ */

lnn_instability_type_t lnn_immune_check_stability(lnn_immune_bridge_t* bridge)
{
    /* Guard clause */
    if (!bridge || !bridge->network) {
        return LNN_INSTABILITY_NONE;
    }

    lnn_network_t* network = bridge->network;
    const lnn_immune_config_t* config = &bridge->config;

    /* Check for NaN/Inf */
    bool has_nan = false;
    bool has_inf = false;
    if (!check_state_validity(network, &has_nan, &has_inf)) {
        if (has_nan) {
            NIMCP_LOGGING_ERROR("Detected NaN in LNN state");
            return LNN_INSTABILITY_NAN_STATE;
        }
        if (has_inf) {
            NIMCP_LOGGING_ERROR("Detected Inf in LNN state");
            return LNN_INSTABILITY_INF_STATE;
        }
    }

    /* Check state norm for explosion/collapse */
    float state_norm = compute_state_norm(network);
    if (state_norm > config->state_explosion_threshold) {
        NIMCP_LOGGING_ERROR("State explosion detected: norm=%.2e", state_norm);
        return LNN_INSTABILITY_STATE_EXPLOSION;
    }
    if (state_norm < config->state_collapse_threshold && state_norm > 0.0f) {
        NIMCP_LOGGING_WARN("State collapse detected: norm=%.2e", state_norm);
        return LNN_INSTABILITY_STATE_COLLAPSE;
    }

    /* Check tau health */
    lnn_instability_type_t tau_issue = check_tau_health(network, config->tau_min, config->tau_max);
    if (tau_issue != LNN_INSTABILITY_NONE) {
        NIMCP_LOGGING_ERROR("Tau instability detected: type=%d", tau_issue);
        return tau_issue;
    }

    /* Check gradient health (if training) */
    if (network->is_training && network->grad_ctx) {
        /* Placeholder: would check gradient norms here */
        /* For now, return NONE as gradients not implemented yet */
    }

    return LNN_INSTABILITY_NONE;
}

int lnn_immune_report_instability(
    lnn_immune_bridge_t* bridge,
    lnn_instability_type_t type,
    uint32_t layer_id,
    uint32_t neuron_id)
{
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null bridge pointer in lnn_immune_report_instability");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (type <= LNN_INSTABILITY_NONE || type >= LNN_INSTABILITY_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid instability type: %d", type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid instability type: %d", type);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Thread-safe update */
    if (bridge->base.mutex) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    /* Update statistics */
    bridge->stats.total_instabilities++;
    bridge->stats.instabilities_by_type[type]++;

    /* Create epitope (threat signature) from instability */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);

    /* Encode: [type(1) | layer_id(4) | neuron_id(4)] */
    epitope[0] = (uint8_t)type;
    memcpy(&epitope[1], &layer_id, sizeof(uint32_t));
    memcpy(&epitope[5], &neuron_id, sizeof(uint32_t));

    size_t epitope_len = 9;

    /* Get severity for this type */
    uint32_t severity = bridge->config.instability_severity[type];

    if (bridge->base.mutex) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    /* Present to brain immune if connected */
    if (bridge->brain_immune) {
        uint32_t antigen_id = 0;
        int result = brain_immune_present_antigen(
            bridge->brain_immune,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            epitope_len,
            severity,
            layer_id,
            &antigen_id
        );

        if (result == 0) {
            NIMCP_LOGGING_INFO("Reported LNN instability as antigen %u (type=%s, severity=%u)",
                               antigen_id, lnn_instability_type_to_string(type), severity);

            if (bridge->base.mutex) {
                nimcp_mutex_lock(bridge->base.mutex);
            }
            bridge->stats.antigens_presented++;
            if (bridge->base.mutex) {
                nimcp_mutex_unlock(bridge->base.mutex);
            }
        } else {
            NIMCP_LOGGING_ERROR("Failed to present instability as antigen");
            return NIMCP_ERROR_OPERATION_FAILED;
        }
    }

    /* Report to training immune if connected */
    if (bridge->training_immune) {
        /* Training immune has its own instability types, so we'd need mapping */
        /* For now, just log */
        NIMCP_LOGGING_DEBUG("LNN instability also available to training immune");
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Cytokine Effect Management API Implementation
 * ============================================================================ */

int lnn_immune_update_effects(lnn_immune_bridge_t* bridge)
{
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null bridge pointer in lnn_immune_update_effects");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->brain_immune) {
        /* No immune system connected, use default effects */
        bridge->cytokine_effects.inflammation = INFLAMMATION_NONE;
        bridge->cytokine_effects.tau_scale = 1.0f;
        bridge->cytokine_effects.lr_factor = 1.0f;
        bridge->cytokine_effects.valid = true;
        return NIMCP_SUCCESS;
    }

    /* Get current inflammation level */
    brain_inflammation_level_t inflammation = brain_immune_get_inflammation_level(bridge->brain_immune);

    /* Compute effects */
    float tau_scale = inflammation_to_tau_scale(inflammation, &bridge->config);
    float lr_factor = inflammation_to_lr_factor(inflammation, &bridge->config);

    /* Thread-safe update */
    if (bridge->base.mutex) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    bridge->cytokine_effects.inflammation = inflammation;
    bridge->cytokine_effects.tau_scale = tau_scale;
    bridge->cytokine_effects.tau_offset = 0.0f;  /* No offset for now */
    bridge->cytokine_effects.lr_factor = lr_factor;
    bridge->cytokine_effects.gradient_scale = lr_factor;  /* Same as LR */
    bridge->cytokine_effects.state_damping = (inflammation >= INFLAMMATION_SYSTEMIC) ? 0.9f : 1.0f;
    bridge->cytokine_effects.noise_injection = 0.0f;  /* No noise injection */
    bridge->cytokine_effects.valid = true;

    /* Update statistics */
    bridge->stats.current_inflammation = inflammation;
    bridge->stats.current_tau_scale = tau_scale;
    bridge->stats.current_lr_factor = lr_factor;

    if (bridge->base.mutex) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return NIMCP_SUCCESS;
}

int lnn_immune_apply_effects(lnn_immune_bridge_t* bridge)
{
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null bridge pointer in lnn_immune_apply_effects");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->network) {
        NIMCP_LOGGING_ERROR("NULL network pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "Null network in lnn_immune_apply_effects");
        return NIMCP_ERROR_INVALID_STATE;
    }
    if (!bridge->cytokine_effects.valid) {
        NIMCP_LOGGING_WARN("Cytokine effects not valid, skipping apply");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "Cytokine effects not valid in lnn_immune_apply_effects");
        return NIMCP_ERROR_INVALID_STATE;
    }

    lnn_network_t* network = bridge->network;
    const lnn_cytokine_effects_t* effects = &bridge->cytokine_effects;

    /* Apply tau modulation */
    if (bridge->config.enable_tau_modulation && effects->tau_scale != 1.0f) {
        for (uint32_t i = 0; i < network->n_layers; i++) {
            lnn_layer_t* layer = network->layers[i];
            if (!layer || !layer->neurons) continue;

            for (uint32_t j = 0; j < layer->n_neurons; j++) {
                lnn_neuron_t* neuron = &layer->neurons[j];

                /* Scale tau (but respect min/max bounds) */
                float new_tau = neuron->tau_base * effects->tau_scale + effects->tau_offset;
                if (new_tau < neuron->tau_min) new_tau = neuron->tau_min;
                if (new_tau > neuron->tau_max) new_tau = neuron->tau_max;

                neuron->tau_current = new_tau;
            }
        }

        if (bridge->base.mutex) {
            nimcp_mutex_lock(bridge->base.mutex);
        }
        bridge->stats.tau_modulations++;
        if (bridge->base.mutex) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
    }

    /* Apply learning rate modulation */
    if (bridge->config.enable_lr_modulation && effects->lr_factor != 1.0f) {
        /* LR modulation would be applied via optimizer integration */
        /* This requires optimizer handle, which we don't have here */
        /* Mark that modulation is needed */

        if (bridge->base.mutex) {
            nimcp_mutex_lock(bridge->base.mutex);
        }
        bridge->stats.lr_modulations++;
        if (bridge->base.mutex) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
    }

    /* Apply state damping */
    if (bridge->config.enable_state_damping && effects->state_damping != 1.0f) {
        for (uint32_t i = 0; i < network->n_layers; i++) {
            lnn_layer_t* layer = network->layers[i];
            if (!layer || !layer->neurons) continue;

            for (uint32_t j = 0; j < layer->n_neurons; j++) {
                lnn_neuron_t* neuron = &layer->neurons[j];
                neuron->x *= effects->state_damping;
            }
        }

        if (bridge->base.mutex) {
            nimcp_mutex_lock(bridge->base.mutex);
        }
        bridge->stats.state_dampings++;
        if (bridge->base.mutex) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
    }

    return NIMCP_SUCCESS;
}

int lnn_immune_get_effects(
    const lnn_immune_bridge_t* bridge,
    lnn_cytokine_effects_t* effects)
{
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null bridge pointer in lnn_immune_get_effects");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!effects) {
        NIMCP_LOGGING_ERROR("NULL effects pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null effects pointer in lnn_immune_get_effects");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Thread-safe copy */
    if (bridge->base.mutex) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    memcpy(effects, &bridge->cytokine_effects, sizeof(lnn_cytokine_effects_t));

    if (bridge->base.mutex) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query and Statistics API Implementation
 * ============================================================================ */

int lnn_immune_get_stats(
    const lnn_immune_bridge_t* bridge,
    lnn_immune_stats_t* stats)
{
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null bridge pointer in lnn_immune_get_stats");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!stats) {
        NIMCP_LOGGING_ERROR("NULL stats pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null stats pointer in lnn_immune_get_stats");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Thread-safe copy */
    if (bridge->base.mutex) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    memcpy(stats, &bridge->stats, sizeof(lnn_immune_stats_t));

    if (bridge->base.mutex) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * String Conversion Utilities Implementation
 * ============================================================================ */

const char* lnn_instability_type_to_string(lnn_instability_type_t type)
{
    switch (type) {
        case LNN_INSTABILITY_NONE:                return "NONE";
        case LNN_INSTABILITY_NAN_STATE:           return "NAN_STATE";
        case LNN_INSTABILITY_INF_STATE:           return "INF_STATE";
        case LNN_INSTABILITY_STATE_EXPLOSION:     return "STATE_EXPLOSION";
        case LNN_INSTABILITY_STATE_COLLAPSE:      return "STATE_COLLAPSE";
        case LNN_INSTABILITY_TAU_EXPLOSION:       return "TAU_EXPLOSION";
        case LNN_INSTABILITY_TAU_COLLAPSE:        return "TAU_COLLAPSE";
        case LNN_INSTABILITY_GRADIENT_EXPLOSION:  return "GRADIENT_EXPLOSION";
        case LNN_INSTABILITY_GRADIENT_VANISHING:  return "GRADIENT_VANISHING";
        case LNN_INSTABILITY_ODE_DIVERGENCE:      return "ODE_DIVERGENCE";
        default:                                   return "UNKNOWN";
    }
}

/**
 * @brief Connect network to immune bridge for modulation
 *
 * WHAT: Links LNN to immune system for learning modulation
 * WHY:  Allow immune-based suppression during inflammation
 * HOW:  Register network with immune bridge for callbacks
 */
int lnn_immune_connect(lnn_network_t* network, void* immune_bridge)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_immune_connect");
        return -1;
    }

    /* Store immune bridge reference for modulation callbacks */
    network->immune_bridge = immune_bridge;
    NIMCP_LOGGING_INFO("LNN network connected to immune bridge");
    return 0;
}
