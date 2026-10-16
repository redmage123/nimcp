/**
 * @file nimcp_microglia_immune_bridge.c
 * @brief Microglia-Brain Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and microglia
 * WHY:  Microglia ARE the brain's resident immune cells - phagocytosis,
 *       cytokine production, antigen presentation, M1/M2 polarization
 * HOW:  Brain immune uses microglia for local response; microglia report
 *       threats as antigens and coordinate with systemic immunity
 */

#include "glial/immune/nimcp_microglia_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(microglia_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current inflammation level from immune system
 *
 * WHAT: Extract inflammation level from brain immune stats
 * WHY:  Inflammation drives microglial activation and polarization
 * HOW:  Query immune stats, map inflammation_sites to level
 */
static brain_inflammation_level_t get_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_immune_stats_t stats;
    if (brain_immune_get_stats((brain_immune_system_t*)immune, &stats) != 0) {
        return INFLAMMATION_NONE;
    }

    /* Map inflammation sites to level */
    if (stats.inflammation_sites == 0) return INFLAMMATION_NONE;
    if (stats.inflammation_sites <= 2) return INFLAMMATION_LOCAL;
    if (stats.inflammation_sites <= 5) return INFLAMMATION_REGIONAL;
    if (stats.inflammation_sites <= 10) return INFLAMMATION_SYSTEMIC;
    return INFLAMMATION_STORM;
}

/**
 * @brief Get cytokine concentration from immune system
 *
 * WHAT: Query specific cytokine level
 * WHY:  Cytokine milieu determines M1/M2 polarization
 * HOW:  Sum concentrations by type from active cytokines
 *
 * Thread safety: Acquires immune system mutex during iteration
 */
static float get_cytokine_concentration(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune || !immune->cytokines) return 0.0f;

    /* Acquire immune system mutex to safely iterate cytokines */
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)immune->mutex);

    float total = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        if (immune->cytokines[i].type == type) {
            total += immune->cytokines[i].concentration;
        }
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)immune->mutex);

    return total;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int microglia_immune_default_config(microglia_immune_config_t* config) {
    /* Guard: validate parameters */
    if (!config) {
        NIMCP_LOGGING_ERROR("microglia_immune_default_config: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "microglia_immune_default_config: config is NULL");
        return -1;
    }

    /* Enable all features by default */
    config->enable_cytokine_polarization = true;
    config->enable_inflammation_activation = true;
    config->enable_m1_cytokine_production = true;
    config->enable_m2_cytokine_production = true;
    config->enable_antigen_presentation = true;
    config->enable_complement_reporting = true;

    /* Default sensitivity (1.0 = biological baseline) */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->polarization_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Default thresholds from biological literature */
    config->m1_threshold = M1_POLARIZATION_THRESHOLD;
    config->m2_threshold = M2_POLARIZATION_THRESHOLD;
    config->phagocytic_threshold = PHAGOCYTIC_ANTIGEN_THRESHOLD;

    return 0;
}

microglia_immune_bridge_t* microglia_immune_bridge_create(
    const microglia_immune_config_t* config,
    brain_immune_system_t* immune_system,
    microglia_network_t* microglia_network
) {
    /* Guard: validate critical parameters */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("microglia_immune_bridge_create: NULL immune_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }
    if (!microglia_network) {
        NIMCP_LOGGING_ERROR("microglia_immune_bridge_create: NULL microglia_network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "microglia_network is NULL");

        return NULL;
    }

    /* Allocate bridge */
    microglia_immune_bridge_t* bridge = (microglia_immune_bridge_t*)
        nimcp_malloc(sizeof(microglia_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("microglia_immune_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(microglia_immune_bridge_t));

    /* Use default config if not provided */
    microglia_immune_config_t default_config;
    if (!config) {
        microglia_immune_default_config(&default_config);
        config = &default_config;
    }

    /* Initialize handles */
    bridge->immune_system = immune_system;
    bridge->microglia_network = microglia_network;

    /* Copy configuration */
    bridge->enable_cytokine_polarization = config->enable_cytokine_polarization;
    bridge->enable_inflammation_activation = config->enable_inflammation_activation;
    bridge->enable_m1_cytokine_production = config->enable_m1_cytokine_production;
    bridge->enable_m2_cytokine_production = config->enable_m2_cytokine_production;
    bridge->enable_antigen_presentation = config->enable_antigen_presentation;
    bridge->enable_complement_reporting = config->enable_complement_reporting;

    /* Initialize state structures */
    memset(&bridge->cytokine_effects, 0, sizeof(cytokine_microglia_effects_t));
    memset(&bridge->inflammation_state, 0, sizeof(inflammation_microglia_state_t));
    memset(&bridge->microglia_modulation, 0, sizeof(microglia_immune_modulation_t));

    /* Initialize timing */
    bridge->last_update_time = nimcp_time_get_us();

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "microglia_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("microglia_immune_bridge_create: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "microglia_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("microglia_immune_bridge_create: bridge created successfully");
    return bridge;
}

void microglia_immune_bridge_destroy(microglia_immune_bridge_t* bridge) {
    /* Guard: NULL-safe */
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        microglia_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex (created with nimcp_platform_mutex_create) */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge structure */
    nimcp_free(bridge);

    NIMCP_LOGGING_DEBUG("microglia_immune_bridge_destroy: bridge destroyed");
}

/* ============================================================================
 * Immune -> Microglia Implementation
 * ============================================================================ */

int microglia_immune_apply_cytokine_effects(microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->microglia_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->enable_cytokine_polarization) {
        return NIMCP_SUCCESS; /* Feature disabled */
    }

    /* Query cytokine levels BEFORE acquiring bridge mutex to avoid deadlock.
     * get_cytokine_concentration() acquires immune->mutex internally.
     * Acquiring bridge->base.mutex first then immune->mutex creates a
     * lock ordering violation if any code path does the reverse. */
    float il1 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float il10 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL10);
    float ifn_gamma = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute M1 polarization drivers (pro-inflammatory) */
    bridge->cytokine_effects.il1_m1_drive = il1 * CYTOKINE_IL1_ACTIVATION_FACTOR;
    bridge->cytokine_effects.il6_m1_drive = il6 * CYTOKINE_IL6_ACTIVATION_FACTOR;
    bridge->cytokine_effects.tnf_m1_drive = tnf * CYTOKINE_TNF_ACTIVATION_FACTOR;
    bridge->cytokine_effects.ifn_gamma_m1_drive = ifn_gamma * CYTOKINE_IFN_GAMMA_ACTIVATION_FACTOR;

    /* Compute M2 polarization driver (anti-inflammatory) */
    bridge->cytokine_effects.il10_m2_drive = il10 * CYTOKINE_IL10_M2_FACTOR;

    /* Calculate aggregate M1 and M2 polarization strengths */
    float m1_total = bridge->cytokine_effects.il1_m1_drive +
                     bridge->cytokine_effects.il6_m1_drive +
                     bridge->cytokine_effects.tnf_m1_drive +
                     bridge->cytokine_effects.ifn_gamma_m1_drive;

    float m2_total = bridge->cytokine_effects.il10_m2_drive;

    bridge->cytokine_effects.m1_polarization_strength = nimcp_clamp01(m1_total);
    bridge->cytokine_effects.m2_polarization_strength = nimcp_clamp01(m2_total);

    /* Total activation is combined pro-inflammatory drive */
    bridge->cytokine_effects.total_activation = nimcp_clamp01(m1_total);

    /* Determine polarization state */
    if (m1_total >= M1_POLARIZATION_THRESHOLD && m1_total > m2_total * M1_M2_DOMINANCE_RATIO) {
        bridge->cytokine_effects.polarization = MICROGLIA_POLARIZATION_M1;
        bridge->m1_polarization_events++;
    } else if (m2_total >= M2_POLARIZATION_THRESHOLD && m2_total > m1_total * M2_M1_DOMINANCE_RATIO) {
        bridge->cytokine_effects.polarization = MICROGLIA_POLARIZATION_M2;
        bridge->m2_polarization_events++;
    } else if (m1_total > MIN_POLARIZATION_SIGNAL && m2_total > MIN_POLARIZATION_SIGNAL) {
        bridge->cytokine_effects.polarization = MICROGLIA_POLARIZATION_MIXED;
    } else {
        bridge->cytokine_effects.polarization = MICROGLIA_POLARIZATION_NONE;
    }

    /* Process retraction scales with activation */
    bridge->cytokine_effects.process_retraction = nimcp_clamp01(
        m1_total * PROCESS_RETRACTION_ACTIVATION
    );

    /* Phagocytic capacity increases with M1, decreases with M2 */
    bridge->cytokine_effects.phagocytic_capacity = nimcp_clamp01(
        0.3f + m1_total * 0.5f - m2_total * 0.2f
    );

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int microglia_immune_apply_inflammation_effects(microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->microglia_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->enable_inflammation_activation) {
        return NIMCP_SUCCESS; /* Feature disabled */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current inflammation level */
    brain_inflammation_level_t level = get_inflammation_level(bridge->immune_system);
    bridge->inflammation_state.current_level = level;

    /* Map inflammation level to microglial population activation */
    float activated_fraction = 0.0f;
    float m1_fraction = 0.0f;

    switch (level) {
        case INFLAMMATION_NONE:
            activated_fraction = INFLAMMATION_NONE_ACTIVATION;
            m1_fraction = 0.0f;
            break;
        case INFLAMMATION_LOCAL:
            activated_fraction = INFLAMMATION_LOCAL_ACTIVATION;
            m1_fraction = 0.2f;
            break;
        case INFLAMMATION_REGIONAL:
            activated_fraction = INFLAMMATION_REGIONAL_ACTIVATION;
            m1_fraction = 0.5f;
            break;
        case INFLAMMATION_SYSTEMIC:
            activated_fraction = INFLAMMATION_SYSTEMIC_ACTIVATION;
            m1_fraction = 0.8f;
            break;
        case INFLAMMATION_STORM:
            activated_fraction = INFLAMMATION_STORM_TOXICITY;
            m1_fraction = 0.95f;
            bridge->inflammation_state.cytokine_storm_toxicity = true;
            break;
    }

    bridge->inflammation_state.activated_microglia_fraction = activated_fraction;
    bridge->inflammation_state.m1_fraction = m1_fraction;
    bridge->inflammation_state.m2_fraction = nimcp_clamp01(1.0f - m1_fraction - 0.1f);

    /* Calculate average cytokine production and phagocytosis rates */
    bridge->inflammation_state.avg_cytokine_production = m1_fraction * 0.8f;
    bridge->inflammation_state.avg_phagocytosis_rate = activated_fraction * 0.6f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float microglia_immune_compute_activation(const microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) return 0.0f;

    /* Combine cytokine-driven and inflammation-driven activation */
    float cytokine_activation = bridge->cytokine_effects.total_activation;
    float inflammation_activation = bridge->inflammation_state.activated_microglia_fraction;

    /* Take maximum of the two (not sum, to avoid > 1.0) */
    return fmaxf(cytokine_activation, inflammation_activation);
}

microglia_polarization_t microglia_immune_compute_polarization(
    const microglia_immune_bridge_t* bridge
) {
    /* Guard: validate parameters */
    if (!bridge) return MICROGLIA_POLARIZATION_NONE;

    /* Use cytokine-computed polarization as primary signal */
    return bridge->cytokine_effects.polarization;
}

/* ============================================================================
 * Microglia -> Immune Implementation
 * ============================================================================ */

int microglia_immune_release_m1_cytokines(microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->microglia_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->enable_m1_cytokine_production) {
        return NIMCP_SUCCESS; /* Feature disabled */
    }

    /* Check if M1 polarization threshold met */
    float m1_strength = bridge->cytokine_effects.m1_polarization_strength;
    if (m1_strength < M1_CYTOKINE_PRODUCTION_THRESHOLD) {
        return NIMCP_SUCCESS; /* Not enough M1 activation */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float release_strength = m1_strength - M1_CYTOKINE_PRODUCTION_THRESHOLD;
    uint32_t cytokine_id;

    /* Release IL-1beta (primary M1 cytokine) */
    brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL1,
        0, /* source cell not tracked */
        release_strength * 0.5f,
        0, /* broadcast */
        &cytokine_id
    );

    /* Release IL-6 */
    brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL6,
        0,
        release_strength * 0.3f,
        0,
        &cytokine_id
    );

    /* Release TNF-alpha (strongest M1 marker) */
    brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_TNF,
        0,
        release_strength * 0.4f,
        0,
        &cytokine_id
    );

    /* Update M1 cytokine production metric */
    bridge->microglia_modulation.m1_cytokine_production += release_strength;
    bridge->cytokine_releases++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int microglia_immune_release_m2_cytokines(microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->microglia_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->enable_m2_cytokine_production) {
        return NIMCP_SUCCESS; /* Feature disabled */
    }

    /* Check if M2 polarization threshold met */
    float m2_strength = bridge->cytokine_effects.m2_polarization_strength;
    if (m2_strength < M2_CYTOKINE_PRODUCTION_THRESHOLD) {
        return NIMCP_SUCCESS; /* Not enough M2 activation */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float release_strength = m2_strength - M2_CYTOKINE_PRODUCTION_THRESHOLD;
    uint32_t cytokine_id;

    /* Release IL-10 (primary anti-inflammatory cytokine) */
    brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL10,
        0,
        release_strength * 0.6f,
        0, /* broadcast */
        &cytokine_id
    );

    /* Update M2 cytokine production metric */
    bridge->microglia_modulation.m2_cytokine_production += release_strength;
    bridge->cytokine_releases++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int microglia_immune_present_damp_antigens(microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->microglia_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->enable_antigen_presentation) {
        return NIMCP_SUCCESS; /* Feature disabled */
    }

    /* Only present antigens if phagocytically active */
    float phagocytic = bridge->cytokine_effects.phagocytic_capacity;
    if (phagocytic < PHAGOCYTIC_ANTIGEN_THRESHOLD) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Present DAMP antigens based on phagocytic activity */
    uint8_t damp_epitope[] = {0xDA, 0x4D, 0x50}; /* DAMP marker */
    uint32_t antigen_id;

    /* Severity scales with phagocytic activity */
    uint8_t severity = (uint8_t)(3 + (phagocytic - 0.7f) * 10);

    brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL, /* From microglia detection */
        damp_epitope,
        sizeof(damp_epitope),
        severity,
        0, /* node ID - network-wide */
        &antigen_id
    );

    bridge->microglia_modulation.antigens_presented++;
    bridge->antigens_presented++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int microglia_immune_report_complement_pruning(microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->microglia_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->enable_complement_reporting) {
        return NIMCP_SUCCESS; /* Feature disabled */
    }

    /* Only report if microglia are active enough for pruning */
    float activation = microglia_immune_compute_activation(bridge);
    if (activation < 0.3f) {
        return NIMCP_SUCCESS; /* Not active enough */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Sample pruning events (not every one) */
    static uint32_t pruning_counter = 0;
    pruning_counter++;

    if ((float)(pruning_counter % 10) / 10.0f < PRUNING_ANTIGEN_REPORT_RATE) {
        /* Report C3-tagged synapse as low-severity antigen */
        uint8_t c3_epitope[] = {0xC3, 0x53, 0x59}; /* C3-SYN marker */
        uint32_t antigen_id;

        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_MANUAL,
            c3_epitope,
            sizeof(c3_epitope),
            COMPLEMENT_C3_ANTIGEN_SEVERITY,
            0,
            &antigen_id
        );

        bridge->microglia_modulation.complement_tag_reports++;
        bridge->complement_reports++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int microglia_immune_report_phagocytosis(microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->microglia_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update phagocytosis activity metric */
    float phagocytic = bridge->cytokine_effects.phagocytic_capacity;
    bridge->microglia_modulation.phagocytosis_activity = phagocytic;
    bridge->microglia_modulation.phagocytosis_reports++;

    /* Track if triggering local inflammation */
    if (phagocytic >= 0.8f) {
        bridge->microglia_modulation.local_inflammation_trigger = true;
        bridge->microglia_modulation.immune_cell_recruitment = phagocytic * 0.5f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int microglia_immune_bridge_update(
    microglia_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard: validate parameters */
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    (void)delta_ms; /* Currently unused but available for time-dependent dynamics */

    /* Apply immune -> microglia effects */
    microglia_immune_apply_cytokine_effects(bridge);
    microglia_immune_apply_inflammation_effects(bridge);

    /* Apply microglia -> immune effects */
    microglia_immune_release_m1_cytokines(bridge);
    microglia_immune_release_m2_cytokines(bridge);
    microglia_immune_present_damp_antigens(bridge);
    microglia_immune_report_complement_pruning(bridge);
    microglia_immune_report_phagocytosis(bridge);

    /* Update timing and statistics */
    bridge->last_update_time = nimcp_time_get_us();
    bridge->total_updates++;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int microglia_immune_get_cytokine_effects(
    const microglia_immune_bridge_t* bridge,
    cytokine_microglia_effects_t* effects
) {
    /* Guard: validate parameters */
    if (!bridge || !effects) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_microglia_effects_t));
    return NIMCP_SUCCESS;
}

int microglia_immune_get_inflammation_state(
    const microglia_immune_bridge_t* bridge,
    inflammation_microglia_state_t* state
) {
    /* Guard: validate parameters */
    if (!bridge || !state) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_microglia_state_t));
    return NIMCP_SUCCESS;
}

bool microglia_immune_has_storm_toxicity(const microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) {
        return false;
    }

    return bridge->inflammation_state.cytokine_storm_toxicity;
}

float microglia_immune_get_activation_factor(const microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) return 0.0f;

    return microglia_immune_compute_activation(bridge);
}

float microglia_immune_get_m1_fraction(const microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) return 0.0f;

    return bridge->inflammation_state.m1_fraction;
}

float microglia_immune_get_m2_fraction(const microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) return 0.0f;

    return bridge->inflammation_state.m2_fraction;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int microglia_immune_connect_bio_async(microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS; /* Already connected */
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_MICROGLIA,
        .module_name = "microglia_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("microglia_immune_connect_bio_async: connected to bio-async router");
    } else {
        NIMCP_LOGGING_DEBUG("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

int microglia_immune_disconnect_bio_async(microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS; /* Already disconnected */
    }

    /* Unregister from bio-async router */
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("microglia_immune_disconnect_bio_async: disconnected");
    return NIMCP_SUCCESS;
}

bool microglia_immune_is_bio_async_connected(const microglia_immune_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}
