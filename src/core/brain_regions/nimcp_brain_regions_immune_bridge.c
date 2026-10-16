/**
 * @file nimcp_brain_regions_immune_bridge.c
 * @brief Brain Regions-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional coupling between brain immune and brain region systems
 * WHY:  Biological realism - cytokines affect regions differently, abnormal activity triggers immune
 * HOW:  Monitor cytokines to modulate regions, monitor regions to trigger immune response
 */

#include "core/brain_regions/nimcp_brain_regions_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(brain_regions_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get default sensitivity for region type
 *
 * WHAT: Return biologically-based sensitivity for region
 * WHY:  Different regions have different immune sensitivity
 * HOW:  Switch on region type, return appropriate constants
 */
static void get_default_sensitivity(
    brain_region_type_t type,
    region_cytokine_sensitivity_t* sens
) {
    if (!sens) return;

    sens->region_type = type;
    sens->il10_responsiveness = 0.8f;  /* Default IL-10 responsiveness */

    switch (type) {
        case REGION_HIPPOCAMPUS:
            sens->il1_sensitivity = REGION_HIPPOCAMPUS_IL1_SENSITIVITY;
            sens->il6_sensitivity = REGION_HIPPOCAMPUS_IL6_SENSITIVITY;
            sens->tnf_sensitivity = REGION_HIPPOCAMPUS_TNF_SENSITIVITY;
            sens->ifn_sensitivity = REGION_HIPPOCAMPUS_IFN_SENSITIVITY;
            break;

        case REGION_PREFRONTAL:
            sens->il1_sensitivity = REGION_PREFRONTAL_IL1_SENSITIVITY;
            sens->il6_sensitivity = REGION_PREFRONTAL_IL6_SENSITIVITY;
            sens->tnf_sensitivity = REGION_PREFRONTAL_TNF_SENSITIVITY;
            sens->ifn_sensitivity = REGION_PREFRONTAL_IFN_SENSITIVITY;
            break;

        case REGION_MOTOR_M1:
        case REGION_MOTOR_PREMOTOR:
        case REGION_MOTOR_SMA:
            sens->il1_sensitivity = REGION_MOTOR_IL1_SENSITIVITY;
            sens->il6_sensitivity = REGION_MOTOR_IL6_SENSITIVITY;
            sens->tnf_sensitivity = REGION_MOTOR_TNF_SENSITIVITY;
            sens->ifn_sensitivity = REGION_MOTOR_IFN_SENSITIVITY;
            break;

        case REGION_VISUAL_V1:
        case REGION_VISUAL_V2:
        case REGION_VISUAL_V4:
        case REGION_VISUAL_MT:
        case REGION_VISUAL_IT:
        case REGION_AUDITORY_A1:
        case REGION_AUDITORY_A2:
        case REGION_AUDITORY_BELT:
        case REGION_AUDITORY_PARABELT:
        case REGION_SOMATOSENSORY_S1:
        case REGION_SOMATOSENSORY_S2:
            sens->il1_sensitivity = REGION_SENSORY_IL1_SENSITIVITY;
            sens->il6_sensitivity = REGION_SENSORY_IL6_SENSITIVITY;
            sens->tnf_sensitivity = REGION_SENSORY_TNF_SENSITIVITY;
            sens->ifn_sensitivity = REGION_SENSORY_IFN_SENSITIVITY;
            break;

        case REGION_THALAMUS:
            sens->il1_sensitivity = REGION_THALAMUS_IL1_SENSITIVITY;
            sens->il6_sensitivity = REGION_THALAMUS_IL6_SENSITIVITY;
            sens->tnf_sensitivity = REGION_THALAMUS_TNF_SENSITIVITY;
            sens->ifn_sensitivity = REGION_THALAMUS_IFN_SENSITIVITY;
            break;

        case REGION_BASAL_GANGLIA:
            sens->il1_sensitivity = REGION_BASAL_GANGLIA_IL1_SENSITIVITY;
            sens->il6_sensitivity = REGION_BASAL_GANGLIA_IL6_SENSITIVITY;
            sens->tnf_sensitivity = REGION_BASAL_GANGLIA_TNF_SENSITIVITY;
            sens->ifn_sensitivity = REGION_BASAL_GANGLIA_IFN_SENSITIVITY;
            break;

        case REGION_CEREBELLUM:
            sens->il1_sensitivity = REGION_CEREBELLUM_IL1_SENSITIVITY;
            sens->il6_sensitivity = REGION_CEREBELLUM_IL6_SENSITIVITY;
            sens->tnf_sensitivity = REGION_CEREBELLUM_TNF_SENSITIVITY;
            sens->ifn_sensitivity = REGION_CEREBELLUM_IFN_SENSITIVITY;
            break;

        default:
            /* Default moderate sensitivity */
            sens->il1_sensitivity = 0.6f;
            sens->il6_sensitivity = 0.6f;
            sens->tnf_sensitivity = 0.6f;
            sens->ifn_sensitivity = 0.5f;
            break;
    }
}

/**
 * @brief Get cytokine concentration from immune system
 *
 * WHAT: Query cytokine level from brain immune system
 * WHY:  Need current cytokine state to modulate regions
 * HOW:  Iterate through cytokines array, find matching type
 */
static float get_cytokine_concentration(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune) return 0.0f;

    float total = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        if (immune->cytokines[i].type == type && !immune->cytokines[i].delivered) {
            total += immune->cytokines[i].concentration;
        }
    }

    return nimcp_clampf(total, 0.0f, 1.0f);
}

/**
 * @brief Find or create inflammation state for region
 *
 * WHAT: Get inflammation state for a region, creating if needed
 * WHY:  Track per-region inflammation
 * HOW:  Search existing states, create new if not found
 */
static region_inflammation_state_t* get_or_create_inflammation_state(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id,
    brain_region_type_t region_type
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    /* Search existing */
    for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
        if (bridge->inflammation_states[i].region_id == region_id) {
            return &bridge->inflammation_states[i];
        }
    }

    /* Create new if space available */
    if (bridge->inflammation_state_count >= bridge->max_inflammation_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_or_create_inflammation_state: capacity exceeded");
        return NULL;
    }

    region_inflammation_state_t* state =
        &bridge->inflammation_states[bridge->inflammation_state_count++];
    memset(state, 0, sizeof(region_inflammation_state_t));
    state->region_id = region_id;
    state->region_type = region_type;
    state->level = REGION_INFLAMMATION_NONE;
    state->activity_modulation = 1.0f;
    state->connectivity_modulation = 1.0f;

    return state;
}

/**
 * @brief Find or create abnormality state for region
 *
 * WHAT: Get abnormality state for a region, creating if needed
 * WHY:  Track per-region abnormalities
 * HOW:  Search existing states, create new if not found
 */
static region_abnormality_state_t* get_or_create_abnormality_state(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id,
    brain_region_type_t region_type
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    /* Search existing */
    for (uint32_t i = 0; i < bridge->abnormality_state_count; i++) {
        if (bridge->abnormality_states[i].region_id == region_id) {
            return &bridge->abnormality_states[i];
        }
    }

    /* Create new if space available */
    if (bridge->abnormality_state_count >= bridge->max_abnormality_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_or_create_abnormality_state: capacity exceeded");
        return NULL;
    }

    region_abnormality_state_t* state =
        &bridge->abnormality_states[bridge->abnormality_state_count++];
    memset(state, 0, sizeof(region_abnormality_state_t));
    state->region_id = region_id;
    state->region_type = region_type;
    state->abnormality_type = REGION_ABNORMALITY_NONE;
    state->baseline_activity = 1.0f;
    state->connectivity_baseline = 1.0f;

    return state;
}

/**
 * @brief Find sensitivity profile for region type
 *
 * WHAT: Look up sensitivity profile
 * WHY:  Need to apply correct sensitivity
 * HOW:  Search custom sensitivities first, then use defaults
 */
static const region_cytokine_sensitivity_t* find_sensitivity(
    const brain_regions_immune_bridge_t* bridge,
    brain_region_type_t region_type
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    /* Search custom sensitivities */
    for (uint32_t i = 0; i < bridge->sensitivity_count; i++) {
        if (bridge->sensitivities[i].region_type == region_type) {
            return &bridge->sensitivities[i];
        }
    }

    return NULL;
}

/**
 * @brief Create epitope from region abnormality
 *
 * WHAT: Generate antigen signature from abnormal region
 * WHY:  Need threat signature for immune system
 * HOW:  Pack region ID, type, activity ratio into byte array
 */
static size_t create_region_epitope(
    uint32_t region_id,
    brain_region_type_t region_type,
    region_abnormality_type_t abnormality,
    float activity_ratio,
    uint8_t* epitope_out,
    size_t max_len
) {
    if (!epitope_out || max_len < 16) return 0;

    uint32_t* data = (uint32_t*)epitope_out;
    size_t idx = 0;

    data[idx++] = region_id;
    data[idx++] = (uint32_t)region_type;
    data[idx++] = (uint32_t)abnormality;
    data[idx++] = (uint32_t)(activity_ratio * 1000.0f);

    return idx * sizeof(uint32_t);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int brain_regions_immune_default_config(brain_regions_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_default_config: config is NULL");
        return -1;
    }

    /* All features enabled by default */
    config->enable_region_specific_sensitivity = true;
    config->enable_inflammation_propagation = true;
    config->enable_layer_effects = true;
    config->enable_abnormality_detection = true;
    config->enable_il10_recovery = true;
    config->enable_bio_async = true;

    /* Biologically-based defaults */
    config->cytokine_sensitivity_multiplier = 1.0f;
    config->propagation_rate_multiplier = 1.0f;
    config->abnormality_sensitivity = 1.0f;

    /* Thresholds */
    config->hyperactivity_threshold = REGION_HYPERACTIVITY_THRESHOLD;
    config->hypoactivity_threshold = REGION_HYPOACTIVITY_THRESHOLD;
    config->persistence_threshold = REGION_ABNORMALITY_PERSISTENCE;

    /* Capacity */
    config->max_regions = 64;
    config->max_propagations = 32;

    return 0;
}

brain_regions_immune_bridge_t* brain_regions_immune_bridge_create(
    const brain_regions_immune_config_t* config,
    brain_module_t* brain_module,
    brain_immune_system_t* immune_system
) {
    /* Guard: require brain module */
    if (!brain_module) {
        NIMCP_LOGGING_ERROR("Cannot create bridge without brain module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_regions_immune_bridge_create: brain_module is NULL");
        return NULL;
    }

    /* Allocate bridge */
    brain_regions_immune_bridge_t* bridge = (brain_regions_immune_bridge_t*)
        nimcp_calloc(1, sizeof(brain_regions_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Bridge allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_regions_immune_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Apply configuration */
    brain_regions_immune_config_t default_cfg;
    if (!config) {
        brain_regions_immune_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    /* Link systems */
    bridge->brain_module = brain_module;
    bridge->immune_system = immune_system;

    /* Allocate sensitivity array */
    bridge->sensitivities = (region_cytokine_sensitivity_t*)
        nimcp_calloc(REGION_TYPE_COUNT, sizeof(region_cytokine_sensitivity_t));
    if (!bridge->sensitivities) {
        NIMCP_LOGGING_ERROR("Sensitivity allocation failed");
        brain_regions_immune_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_regions_immune_bridge_create: bridge->sensitivities is NULL");
        return NULL;
    }
    bridge->sensitivity_count = 0;

    /* Allocate inflammation states */
    bridge->max_inflammation_states = config->max_regions;
    bridge->inflammation_states = (region_inflammation_state_t*)
        nimcp_calloc(bridge->max_inflammation_states, sizeof(region_inflammation_state_t));
    if (!bridge->inflammation_states) {
        NIMCP_LOGGING_ERROR("Inflammation state allocation failed");
        brain_regions_immune_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_regions_immune_bridge_create: bridge->inflammation_states is NULL");
        return NULL;
    }
    bridge->inflammation_state_count = 0;

    /* Allocate abnormality states */
    bridge->max_abnormality_states = config->max_regions;
    bridge->abnormality_states = (region_abnormality_state_t*)
        nimcp_calloc(bridge->max_abnormality_states, sizeof(region_abnormality_state_t));
    if (!bridge->abnormality_states) {
        NIMCP_LOGGING_ERROR("Abnormality state allocation failed");
        brain_regions_immune_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_regions_immune_bridge_create: bridge->abnormality_states is NULL");
        return NULL;
    }
    bridge->abnormality_state_count = 0;

    /* Allocate propagation tracking */
    bridge->max_propagations = config->max_propagations;
    bridge->propagations = (inflammation_propagation_t*)
        nimcp_calloc(bridge->max_propagations, sizeof(inflammation_propagation_t));
    if (!bridge->propagations) {
        NIMCP_LOGGING_ERROR("Propagation allocation failed");
        brain_regions_immune_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_regions_immune_bridge_create: bridge->propagations is NULL");
        return NULL;
    }
    bridge->propagation_count = 0;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "brain_regions_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Mutex allocation failed");
        brain_regions_immune_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_regions_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Try bio-async connection if enabled */
    bridge->base.bio_async_enabled = false;
    if (config->enable_bio_async) {
        brain_regions_immune_connect_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Brain regions immune bridge created");
    return bridge;
}

void brain_regions_immune_bridge_destroy(brain_regions_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        brain_regions_immune_disconnect_bio_async(bridge);
    }

    /* Free arrays */
    if (bridge->sensitivities) {
        nimcp_free(bridge->sensitivities);
    }
    if (bridge->inflammation_states) {
        nimcp_free(bridge->inflammation_states);
    }
    if (bridge->abnormality_states) {
        nimcp_free(bridge->abnormality_states);
    }
    if (bridge->propagations) {
        nimcp_free(bridge->propagations);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Brain regions immune bridge destroyed");
}

/* ============================================================================
 * Bio-async Integration Implementation
 * ============================================================================ */

int brain_regions_immune_connect_bio_async(brain_regions_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_BRAIN_REGIONS,
        .module_name = "brain_regions_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_regions_immune_connect_bio_async: validation failed");
    return -1;
}

int brain_regions_immune_disconnect_bio_async(brain_regions_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool brain_regions_immune_is_bio_async_connected(const brain_regions_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Region Sensitivity Implementation
 * ============================================================================ */

int brain_regions_immune_set_sensitivity(
    brain_regions_immune_bridge_t* bridge,
    const region_cytokine_sensitivity_t* sensitivity
) {
    if (!bridge || !sensitivity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_set_sensitivity: required parameter is NULL (bridge, sensitivity)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Check if already exists */
    for (uint32_t i = 0; i < bridge->sensitivity_count; i++) {
        if (bridge->sensitivities[i].region_type == sensitivity->region_type) {
            bridge->sensitivities[i] = *sensitivity;
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    /* Add new */
    if (bridge->sensitivity_count < REGION_TYPE_COUNT) {
        bridge->sensitivities[bridge->sensitivity_count++] = *sensitivity;
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_regions_immune_set_sensitivity: validation failed");
    return -1;
}

int brain_regions_immune_get_sensitivity(
    const brain_regions_immune_bridge_t* bridge,
    brain_region_type_t region_type,
    region_cytokine_sensitivity_t* sensitivity
) {
    if (!bridge || !sensitivity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_get_sensitivity: required parameter is NULL (bridge, sensitivity)");
        return -1;
    }

    const region_cytokine_sensitivity_t* found = find_sensitivity(bridge, region_type);
    if (found) {
        *sensitivity = *found;
        return 0;
    }

    /* Return defaults */
    get_default_sensitivity(region_type, sensitivity);
    return 0;
}

/* ============================================================================
 * Immune → Brain Regions Implementation
 * ============================================================================ */

int brain_regions_immune_apply_effects(brain_regions_immune_bridge_t* bridge) {
    if (!bridge || !bridge->brain_module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_apply_effects: required parameter is NULL (bridge, bridge->brain_module)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Apply to each region in brain module */
    for (uint32_t i = 0; i < bridge->brain_module->num_regions; i++) {
        brain_region_t* region = bridge->brain_module->regions[i];
        if (!region) continue;

        brain_regions_immune_apply_to_region(bridge, region->id);
    }

    bridge->stats.total_updates++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int brain_regions_immune_apply_to_region(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
) {
    if (!bridge || !bridge->brain_module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_apply_to_region: required parameter is NULL (bridge, bridge->brain_module)");
        return -1;
    }

    /* Find region */
    brain_region_t* region = brain_module_get_region(bridge->brain_module, region_id);
    if (!region) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_apply_to_region: region is NULL");
        return -1;
    }

    /* Get or create inflammation state */
    region_inflammation_state_t* state =
        get_or_create_inflammation_state(bridge, region_id, region->type);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_apply_to_region: state is NULL");
        return -1;
    }

    /* Skip if no immune system */
    if (!bridge->immune_system) {
        state->level = REGION_INFLAMMATION_NONE;
        state->activity_modulation = 1.0f;
        state->connectivity_modulation = 1.0f;
        return 0;
    }

    /* Get sensitivity */
    region_cytokine_sensitivity_t sens;
    const region_cytokine_sensitivity_t* custom = find_sensitivity(bridge, region->type);
    if (custom) {
        sens = *custom;
    } else {
        get_default_sensitivity(region->type, &sens);
    }

    /* Get cytokine concentrations */
    float il1 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float ifn = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);

    /* Apply sensitivity */
    float mult = bridge->config.cytokine_sensitivity_multiplier;
    state->il1_impact = il1 * sens.il1_sensitivity * mult;
    state->il6_impact = il6 * sens.il6_sensitivity * mult;
    state->tnf_impact = tnf * sens.tnf_sensitivity * mult;
    state->ifn_impact = ifn * sens.ifn_sensitivity * mult;

    /* Composite impact */
    state->composite_impact = nimcp_clampf(
        (state->il1_impact + state->il6_impact + state->tnf_impact + state->ifn_impact) / 4.0f,
        0.0f, 1.0f
    );

    /* Determine inflammation level */
    if (state->composite_impact < 0.1f) {
        state->level = REGION_INFLAMMATION_NONE;
    } else if (state->composite_impact < 0.3f) {
        state->level = REGION_INFLAMMATION_MILD;
    } else if (state->composite_impact < 0.5f) {
        state->level = REGION_INFLAMMATION_MODERATE;
    } else if (state->composite_impact < 0.7f) {
        state->level = REGION_INFLAMMATION_SEVERE;
    } else {
        state->level = REGION_INFLAMMATION_CRITICAL;
    }

    state->intensity = state->composite_impact;

    /* Calculate modulation factors */
    state->activity_modulation = nimcp_clampf(1.0f - state->composite_impact * 0.5f, 0.3f, 1.0f);
    state->connectivity_modulation = nimcp_clampf(1.0f - state->composite_impact * 0.4f, 0.4f, 1.0f);

    bridge->stats.inflammations_applied++;
    return 0;
}

int brain_regions_immune_apply_layer_effects(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
) {
    if (!bridge || !bridge->config.enable_layer_effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_apply_layer_effects: required parameter is NULL (bridge, bridge->config)");
        return -1;
    }

    /* Find inflammation state */
    region_inflammation_state_t* state = NULL;
    for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
        if (bridge->inflammation_states[i].region_id == region_id) {
            state = &bridge->inflammation_states[i];
            break;
        }
    }

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_apply_layer_effects: state is NULL");
        return -1;
    }

    /* Apply layer-specific effects */
    float intensity = state->intensity;
    state->layer_disruption[LAYER_1] = intensity * 0.3f;  /* Molecular layer */
    state->layer_disruption[LAYER_2] = intensity * LAYER_23_INHIBITION_FACTOR;
    state->layer_disruption[LAYER_3] = intensity * LAYER_23_INHIBITION_FACTOR;
    state->layer_disruption[LAYER_4] = intensity * LAYER_4_INPUT_DISRUPTION_FACTOR;
    state->layer_disruption[LAYER_5] = intensity * LAYER_5_OUTPUT_REDUCTION_FACTOR;
    state->layer_disruption[LAYER_6] = intensity * LAYER_6_FEEDBACK_DISRUPTION_FACTOR;

    return 0;
}

int brain_regions_immune_propagate_inflammation(brain_regions_immune_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_inflammation_propagation) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    int propagations = 0;

    /* Check each region for inflammation to propagate */
    for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
        region_inflammation_state_t* source = &bridge->inflammation_states[i];

        /* Only propagate if above threshold */
        if (source->intensity < INFLAMMATION_PROPAGATION_THRESHOLD) continue;

        /* Find source region */
        brain_region_t* src_region =
            brain_module_get_region(bridge->brain_module, source->region_id);
        if (!src_region) continue;

        /* Propagate to output regions */
        for (uint32_t j = 0; j < src_region->num_output_regions; j++) {
            uint32_t target_id = src_region->output_regions[j];

            /* Get or create target state */
            brain_region_t* tgt_region =
                brain_module_get_region(bridge->brain_module, target_id);
            if (!tgt_region) continue;

            region_inflammation_state_t* target =
                get_or_create_inflammation_state(bridge, target_id, tgt_region->type);
            if (!target) continue;

            /* Calculate propagation */
            float rate = INFLAMMATION_PROPAGATION_RATE * bridge->config.propagation_rate_multiplier;
            if (source->region_type == REGION_THALAMUS) {
                rate *= INFLAMMATION_THALAMIC_AMPLIFICATION;
            }

            float propagated = source->intensity * rate;
            target->intensity = nimcp_clampf(target->intensity + propagated, 0.0f, 1.0f);

            propagations++;
        }
    }

    bridge->stats.propagations_triggered += propagations;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return propagations;
}

int brain_regions_immune_restore_region(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id,
    float il10_concentration
) {
    if (!bridge || !bridge->config.enable_il10_recovery) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_restore_region: required parameter is NULL (bridge, bridge->config)");
        return -1;
    }

    /* Find inflammation state */
    region_inflammation_state_t* state = NULL;
    for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
        if (bridge->inflammation_states[i].region_id == region_id) {
            state = &bridge->inflammation_states[i];
            break;
        }
    }

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_restore_region: state is NULL");
        return -1;
    }

    /* Get responsiveness */
    region_cytokine_sensitivity_t sens;
    brain_regions_immune_get_sensitivity(bridge, state->region_type, &sens);

    /* Apply IL-10 recovery */
    float recovery = il10_concentration * sens.il10_responsiveness;
    state->intensity = nimcp_clampf(state->intensity * (1.0f - recovery * 0.2f), 0.0f, 1.0f);

    /* Update modulation */
    state->activity_modulation = nimcp_clampf(1.0f - state->intensity * 0.5f, 0.3f, 1.0f);
    state->connectivity_modulation = nimcp_clampf(1.0f - state->intensity * 0.4f, 0.4f, 1.0f);

    bridge->stats.recoveries++;
    return 0;
}

/* ============================================================================
 * Brain Regions → Immune Implementation
 * ============================================================================ */

int brain_regions_immune_detect_abnormalities(brain_regions_immune_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_abnormality_detection) return 0;
    if (!bridge->brain_module) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    int abnormalities = 0;

    for (uint32_t i = 0; i < bridge->brain_module->num_regions; i++) {
        brain_region_t* region = bridge->brain_module->regions[i];
        if (!region) continue;

        region_abnormality_type_t type =
            brain_regions_immune_detect_region_abnormality(bridge, region->id);
        if (type != REGION_ABNORMALITY_NONE) {
            abnormalities++;
        }
    }

    bridge->stats.abnormalities_detected += abnormalities;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return abnormalities;
}

region_abnormality_type_t brain_regions_immune_detect_region_abnormality(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
) {
    if (!bridge || !bridge->brain_module) return REGION_ABNORMALITY_NONE;

    /* Find region */
    brain_region_t* region = brain_module_get_region(bridge->brain_module, region_id);
    if (!region) return REGION_ABNORMALITY_NONE;

    /* Get or create abnormality state */
    region_abnormality_state_t* state =
        get_or_create_abnormality_state(bridge, region_id, region->type);
    if (!state) return REGION_ABNORMALITY_NONE;

    /* Update current activity from region */
    state->current_activity = region->activity_level;
    if (state->baseline_activity < 0.001f) {
        state->baseline_activity = 1.0f;  /* Default baseline */
    }

    state->activity_ratio = state->current_activity / state->baseline_activity;

    /* Check for abnormalities */
    region_abnormality_type_t detected = REGION_ABNORMALITY_NONE;

    if (state->activity_ratio > bridge->config.hyperactivity_threshold) {
        detected = REGION_ABNORMALITY_HYPERACTIVE;
    } else if (state->activity_ratio < bridge->config.hypoactivity_threshold) {
        detected = REGION_ABNORMALITY_HYPOACTIVE;
    }

    /* Check connectivity */
    if (region->num_input_regions > 0) {
        state->input_connectivity = (float)region->num_input_regions /
            (float)(region->num_input_regions + 5);  /* Normalized */
    }
    if (region->num_output_regions > 0) {
        state->output_connectivity = (float)region->num_output_regions /
            (float)(region->num_output_regions + 5);
    }

    if (state->input_connectivity < REGION_DESYNC_THRESHOLD ||
        state->output_connectivity < REGION_DESYNC_THRESHOLD) {
        if (detected != REGION_ABNORMALITY_NONE) {
            detected = REGION_ABNORMALITY_MIXED;
        } else {
            detected = REGION_ABNORMALITY_DESYNC;
        }
    }

    /* Update state */
    if (detected != REGION_ABNORMALITY_NONE) {
        state->consecutive_abnormal++;
        state->abnormality_type = detected;
        state->abnormality_score = nimcp_clampf(
            (float)state->consecutive_abnormal / (float)bridge->config.persistence_threshold,
            0.0f, 1.0f
        );
    } else {
        state->consecutive_abnormal = 0;
        state->abnormality_type = REGION_ABNORMALITY_NONE;
        state->abnormality_score = 0.0f;
    }

    return detected;
}

int brain_regions_immune_trigger_response(
    brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_trigger_response: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Find abnormality state */
    region_abnormality_state_t* state = NULL;
    for (uint32_t i = 0; i < bridge->abnormality_state_count; i++) {
        if (bridge->abnormality_states[i].region_id == region_id) {
            state = &bridge->abnormality_states[i];
            break;
        }
    }

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_trigger_response: state is NULL");
        return -1;
    }

    /* Check persistence threshold */
    if (state->consecutive_abnormal < bridge->config.persistence_threshold) {
        return 0;  /* Not persistent enough */
    }

    /* Create epitope */
    uint8_t epitope[32];
    size_t epitope_len = create_region_epitope(
        region_id,
        state->region_type,
        state->abnormality_type,
        state->activity_ratio,
        epitope,
        sizeof(epitope)
    );

    if (epitope_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_regions_immune_trigger_response: epitope_len is zero");
        return -1;
    }

    /* Calculate severity */
    state->immune_severity = brain_regions_immune_compute_severity(bridge, region_id);

    /* Present antigen to immune system */
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        epitope_len,
        (uint8_t)state->immune_severity,
        region_id,
        &state->antigen_id
    );

    if (result == 0) {
        state->immune_triggered = true;
        bridge->stats.antigens_presented++;
        NIMCP_LOGGING_INFO("Immune response triggered for region %u, severity %u",
                          region_id, state->immune_severity);
    }

    return result;
}

uint32_t brain_regions_immune_compute_severity(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
) {
    if (!bridge) return 1;

    /* Find abnormality state */
    const region_abnormality_state_t* state = NULL;
    for (uint32_t i = 0; i < bridge->abnormality_state_count; i++) {
        if (bridge->abnormality_states[i].region_id == region_id) {
            state = &bridge->abnormality_states[i];
            break;
        }
    }

    if (!state) return 1;

    /* Base severity on abnormality type and magnitude */
    uint32_t severity = 1;

    switch (state->abnormality_type) {
        case REGION_ABNORMALITY_HYPERACTIVE:
            severity = (uint32_t)(state->activity_ratio * 2.0f);
            break;
        case REGION_ABNORMALITY_HYPOACTIVE:
            severity = (uint32_t)((1.0f - state->activity_ratio) * 6.0f + 2.0f);
            break;
        case REGION_ABNORMALITY_DESYNC:
            severity = 4;
            break;
        case REGION_ABNORMALITY_LAYER_FAILURE:
            severity = 6;
            break;
        case REGION_ABNORMALITY_MIXED:
            severity = 8;
            break;
        default:
            severity = 1;
            break;
    }

    /* Clamp to valid range */
    if (severity < 1) severity = 1;
    if (severity > 10) severity = 10;

    return severity;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int brain_regions_immune_bridge_update(
    brain_regions_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_bridge_update: bridge is NULL");
        return -1;
    }

    /* Update duration tracking (inline - needs its own lock) */
    nimcp_platform_mutex_lock(bridge->base.mutex);
    for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
        if (bridge->inflammation_states[i].level > REGION_INFLAMMATION_NONE) {
            bridge->inflammation_states[i].duration_sec += delta_ms / 1000.0f;
        }
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Apply immune effects to regions (self-locking) */
    brain_regions_immune_apply_effects(bridge);

    /* Propagate inflammation (self-locking) */
    brain_regions_immune_propagate_inflammation(bridge);

    /* Apply layer effects (inline - needs its own lock) */
    if (bridge->config.enable_layer_effects) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
        for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
            brain_regions_immune_apply_layer_effects(
                bridge, bridge->inflammation_states[i].region_id
            );
        }
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    /* Detect abnormalities (self-locking) */
    brain_regions_immune_detect_abnormalities(bridge);

    /* Trigger immune responses for persistent abnormalities (inline) */
    nimcp_platform_mutex_lock(bridge->base.mutex);
    for (uint32_t i = 0; i < bridge->abnormality_state_count; i++) {
        region_abnormality_state_t* state = &bridge->abnormality_states[i];
        if (state->abnormality_type != REGION_ABNORMALITY_NONE &&
            state->consecutive_abnormal >= bridge->config.persistence_threshold &&
            !state->immune_triggered) {
            /* Unlock before calling trigger_response to avoid potential deadlock
             * with immune system mutex */
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            brain_regions_immune_trigger_response(bridge, state->region_id);
            nimcp_platform_mutex_lock(bridge->base.mutex);
        }
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Apply IL-10 recovery if available */
    if (bridge->config.enable_il10_recovery && bridge->immune_system) {
        float il10 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL10);
        if (il10 > 0.1f) {
            nimcp_platform_mutex_lock(bridge->base.mutex);
            for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
                brain_regions_immune_restore_region(
                    bridge, bridge->inflammation_states[i].region_id, il10
                );
            }
            nimcp_platform_mutex_unlock(bridge->base.mutex);
        }
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.total_updates++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int brain_regions_immune_get_inflammation_state(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id,
    region_inflammation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
        if (bridge->inflammation_states[i].region_id == region_id) {
            *state = bridge->inflammation_states[i];
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_regions_immune_get_inflammation_state: validation failed");
    return -1;
}

int brain_regions_immune_get_abnormality_state(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id,
    region_abnormality_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_get_abnormality_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->abnormality_state_count; i++) {
        if (bridge->abnormality_states[i].region_id == region_id) {
            *state = bridge->abnormality_states[i];
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_regions_immune_get_abnormality_state: validation failed");
    return -1;
}

float brain_regions_immune_get_activity_modulation(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
) {
    if (!bridge) return 1.0f;

    for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
        if (bridge->inflammation_states[i].region_id == region_id) {
            return bridge->inflammation_states[i].activity_modulation;
        }
    }

    return 1.0f;  /* No modulation */
}

float brain_regions_immune_get_connectivity_modulation(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
) {
    if (!bridge) return 1.0f;

    for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
        if (bridge->inflammation_states[i].region_id == region_id) {
            return bridge->inflammation_states[i].connectivity_modulation;
        }
    }

    return 1.0f;  /* No modulation */
}

bool brain_regions_immune_is_region_modulated(
    const brain_regions_immune_bridge_t* bridge,
    uint32_t region_id
) {
    if (!bridge) {
        return false;
    }

    for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
        if (bridge->inflammation_states[i].region_id == region_id) {
            return bridge->inflammation_states[i].level > REGION_INFLAMMATION_NONE;
        }
    }

    return false;
}

int brain_regions_immune_get_stats(
    const brain_regions_immune_bridge_t* bridge,
    brain_regions_immune_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_regions_immune_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    stats->regions_monitored = bridge->inflammation_state_count;

    /* Calculate average activity */
    if (bridge->brain_module && bridge->brain_module->num_regions > 0) {
        float total = 0.0f;
        for (uint32_t i = 0; i < bridge->brain_module->num_regions; i++) {
            if (bridge->brain_module->regions[i]) {
                total += bridge->brain_module->regions[i]->activity_level;
            }
        }
        stats->avg_region_activity = total / bridge->brain_module->num_regions;
    }

    /* Find max inflammation */
    stats->max_inflammation_intensity = 0.0f;
    for (uint32_t i = 0; i < bridge->inflammation_state_count; i++) {
        if (bridge->inflammation_states[i].intensity > stats->max_inflammation_intensity) {
            stats->max_inflammation_intensity = bridge->inflammation_states[i].intensity;
        }
    }

    return 0;
}

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

const char* region_inflammation_level_to_string(region_inflammation_level_t level) {
    switch (level) {
        case REGION_INFLAMMATION_NONE:     return "NONE";
        case REGION_INFLAMMATION_MILD:     return "MILD";
        case REGION_INFLAMMATION_MODERATE: return "MODERATE";
        case REGION_INFLAMMATION_SEVERE:   return "SEVERE";
        case REGION_INFLAMMATION_CRITICAL: return "CRITICAL";
        default:                           return "UNKNOWN";
    }
}

const char* region_abnormality_type_to_string(region_abnormality_type_t type) {
    switch (type) {
        case REGION_ABNORMALITY_NONE:          return "NONE";
        case REGION_ABNORMALITY_HYPERACTIVE:   return "HYPERACTIVE";
        case REGION_ABNORMALITY_HYPOACTIVE:    return "HYPOACTIVE";
        case REGION_ABNORMALITY_DESYNC:        return "DESYNC";
        case REGION_ABNORMALITY_LAYER_FAILURE: return "LAYER_FAILURE";
        case REGION_ABNORMALITY_MIXED:         return "MIXED";
        default:                               return "UNKNOWN";
    }
}
