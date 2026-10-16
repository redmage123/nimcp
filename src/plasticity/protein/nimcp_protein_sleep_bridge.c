/**
 * @file nimcp_protein_sleep_bridge.c
 * @brief Implementation of Sleep-Protein Synthesis Integration Bridge
 *
 * WHAT: Bidirectional integration between sleep and protein synthesis
 * WHY:  Sleep upregulates protein synthesis for memory consolidation
 * HOW:  Sleep state modulates PRP synthesis rate
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include "plasticity/protein/nimcp_protein_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(protein_sleep_bridge)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal sleep-protein bridge state
 */
typedef struct protein_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    protein_sleep_config_t config;

    /* System handles */
    sleep_system_t sleep_system;
    protein_synthesis_system_t protein_system;

    /* Current effects */
    protein_sleep_effects_t effects;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;
} protein_sleep_bridge_struct;

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(protein_sleep_bridge, struct protein_sleep_bridge_struct)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float protein_sleep_get_synthesis_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return PROTEIN_SLEEP_SYNTH_AWAKE_FACTOR;
        case SLEEP_STATE_DROWSY:
            return PROTEIN_SLEEP_SYNTH_DROWSY_FACTOR;
        case SLEEP_STATE_LIGHT_NREM:
            return PROTEIN_SLEEP_SYNTH_LIGHT_NREM_FACTOR;
        case SLEEP_STATE_DEEP_NREM:
            return PROTEIN_SLEEP_SYNTH_DEEP_NREM_FACTOR;
        case SLEEP_STATE_REM:
            return PROTEIN_SLEEP_SYNTH_REM_FACTOR;
        default:
            return 1.0f;
    }
}

/**
 * WHAT: Get delivery enhancement for REM sleep
 * WHY:  REM enhances PRP delivery to synapses
 * HOW:  Return boost factor during REM
 */
static float get_delivery_enhancement(sleep_state_t state) {
    if (state == SLEEP_STATE_REM) {
        return 1.3f;  /* 30% enhanced delivery */
    }
    return 1.0f;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int protein_sleep_default_config(protein_sleep_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_sleep_default_config: config is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    config->enable_synthesis_modulation = true;
    config->enable_delivery_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

protein_sleep_bridge_t protein_sleep_bridge_create(
    const protein_sleep_config_t* config,
    sleep_system_t sleep_system,
    protein_synthesis_system_t protein_system
) {
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("NULL sleep_system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_sleep_bridge_create: sleep_system is NULL");
        return NULL;
    }
    if (!protein_system) {
        NIMCP_LOGGING_ERROR("NULL protein_system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_sleep_bridge_create: protein_system is NULL");
        return NULL;
    }

    /* Allocate bridge */
    protein_sleep_bridge_t bridge = (protein_sleep_bridge_t)
        nimcp_calloc(1, sizeof(protein_sleep_bridge_struct));

    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate protein sleep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "protein_sleep_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        protein_sleep_default_config(&bridge->config);
    }

    /* Link systems */
    bridge->sleep_system = sleep_system;
    bridge->protein_system = protein_system;

    /* Initialize effects */
    memset(&bridge->effects, 0, sizeof(protein_sleep_effects_t));
    bridge->effects.synthesis_rate_factor = 1.0f;
    bridge->effects.delivery_enhancement = 1.0f;
    bridge->effects.current_state = SLEEP_STATE_AWAKE;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "protein_sleep") != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "protein_sleep_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "protein_sleep_bridge_create: failed to create mutex");
        nimcp_free(bridge);
        return NULL;
    }

    /* Perform initial update */
    protein_sleep_update(bridge);

    NIMCP_LOGGING_INFO("Created protein-sleep bridge");
    return bridge;
}

void protein_sleep_bridge_destroy(protein_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed protein-sleep bridge");
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

int protein_sleep_update(protein_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_sleep_update: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Query sleep system */
    sleep_state_t current_state = sleep_get_current_state(bridge->sleep_system);
    float sleep_pressure = sleep_get_pressure(bridge->sleep_system);

    /* Compute synthesis modulation */
    float synthesis_factor = 1.0f;
    if (bridge->config.enable_synthesis_modulation) {
        synthesis_factor = protein_sleep_get_synthesis_factor(current_state);
        synthesis_factor = 1.0f + (synthesis_factor - 1.0f) *
                                   bridge->config.modulation_strength;
    }

    /* Compute delivery enhancement */
    float delivery_enhancement = 1.0f;
    if (bridge->config.enable_delivery_modulation) {
        delivery_enhancement = get_delivery_enhancement(current_state);
        delivery_enhancement = 1.0f + (delivery_enhancement - 1.0f) *
                                      bridge->config.modulation_strength;
    }

    /* Update effects */
    bridge->effects.synthesis_rate_factor = synthesis_factor;
    bridge->effects.delivery_enhancement = delivery_enhancement;
    bridge->effects.current_state = current_state;
    bridge->effects.sleep_pressure = sleep_pressure;
    bridge->effects.deep_sleep_consolidation =
        (current_state == SLEEP_STATE_DEEP_NREM);

    /* Apply modulation to protein system */
    prp_pool_state_t prp_state;
    if (protein_synthesis_get_prp_state(bridge->protein_system, &prp_state) == 0) {
        prp_state.sleep_modulation = synthesis_factor;
        /* Note: We update the internal state via the protein system's update function */
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Updated protein-sleep bridge: state=%d, synth_factor=%.2f",
                        current_state, synthesis_factor);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int protein_sleep_get_effects(
    const protein_sleep_bridge_t bridge,
    protein_sleep_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_sleep_get_effects: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_sleep_get_effects: effects is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

float protein_sleep_get_synthesis_rate(
    const protein_sleep_bridge_t bridge,
    float base_rate
) {
    if (!bridge) return base_rate;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float effective_rate = base_rate * bridge->effects.synthesis_rate_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return effective_rate;
}

bool protein_sleep_is_consolidation_window(
    const protein_sleep_bridge_t bridge
) {
    if (!bridge) {
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bool is_window = bridge->effects.deep_sleep_consolidation;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return is_window;
}
