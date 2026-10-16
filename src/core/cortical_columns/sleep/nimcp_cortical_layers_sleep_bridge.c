/**
 * @file nimcp_cortical_layers_sleep_bridge.c
 * @brief Sleep-Cortical Layers Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "core/cortical_columns/sleep/nimcp_cortical_layers_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_layers_sleep_bridge)


/* ========================================================================
 * INTERNAL STRUCTURES
 * ======================================================================== */

struct cortical_layers_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    cortical_layers_sleep_config_t config;
    laminar_structure_t* layers;
    sleep_system_t sleep_system;
    cortical_layers_sleep_effects_t effects;
    bool callback_registered;
    uint64_t last_oscillation_update_us;
};

/* ========================================================================
 * FORWARD DECLARATIONS
 * ======================================================================== */

static void cortical_layers_on_sleep_state_change(sleep_state_t new_state, void* user_data);
static void update_layer_specific_effects(cortical_layers_sleep_bridge_t bridge);
static void update_slow_oscillation(cortical_layers_sleep_bridge_t bridge);

/* ========================================================================
 * CALLBACK IMPLEMENTATION
 * ======================================================================== */

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update cortical layer parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state transitions cause immediate neuromodulator changes
 * - Cortical layers respond within seconds to state changes
 * - Layer-specific effects reflect differential neuromodulator sensitivity
 */
static void cortical_layers_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    cortical_layers_sleep_bridge_t bridge = (cortical_layers_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Cortical layers bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update global activity factor */
    if (bridge->config.enable_activity_modulation) {
        float activity_base = cortical_layers_sleep_activity_for_state(new_state);
        bridge->effects.global_activity_factor =
            activity_base * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);
    }

    /* Update feedforward/feedback balance */
    if (bridge->config.enable_connectivity_modulation) {
        bridge->effects.feedforward_balance = cortical_layers_sleep_feedforward_for_state(new_state);
        bridge->effects.feedback_balance = cortical_layers_sleep_feedback_for_state(new_state);

        /* Lateral connectivity reduced in NREM, active in REM */
        if (new_state == SLEEP_STATE_LIGHT_NREM || new_state == SLEEP_STATE_DEEP_NREM) {
            bridge->effects.lateral_connectivity = 0.3f;
        } else if (new_state == SLEEP_STATE_REM) {
            bridge->effects.lateral_connectivity = 0.9f;
        } else {
            bridge->effects.lateral_connectivity = 1.0f;
        }
    }

    /* Update layer-specific effects */
    if (bridge->config.enable_layer_specific) {
        update_layer_specific_effects(bridge);
    }

    /* Update offline status */
    bridge->effects.layers_offline = (new_state == SLEEP_STATE_DEEP_NREM);

    /* Initialize slow oscillation for deep NREM */
    if (bridge->config.enable_slow_oscillations && new_state == SLEEP_STATE_DEEP_NREM) {
        bridge->effects.slow_oscillation_phase = 0.0f;
        bridge->effects.in_up_state = true;
        bridge->last_oscillation_update_us = 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Cortical layers modulated: activity=%.2f, FF=%.2f, FB=%.2f, offline=%d",
                        bridge->effects.global_activity_factor,
                        bridge->effects.feedforward_balance,
                        bridge->effects.feedback_balance,
                        bridge->effects.layers_offline);
}

/* ========================================================================
 * INTERNAL HELPER FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Update layer-specific sleep effects
 * WHY:  Different cortical layers respond differently to sleep states
 * HOW:  Apply layer-specific modulation factors based on current state
 *
 * BIOLOGICAL BASIS:
 * - Layer IV: Most affected by sleep (thalamic input gated)
 * - Layer II/III: Spindles in light NREM, active in REM
 * - Layer V: Burst firing during UP states in deep NREM
 * - Layer VI: Deep hyperpolarization in NREM DOWN states
 */
static void update_layer_specific_effects(cortical_layers_sleep_bridge_t bridge)
{
    sleep_state_t state = bridge->effects.current_state;
    layer_sleep_effects_t* effects = &bridge->effects.layer_effects;

    /* Default: all layers equally affected */
    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        effects->layer_activity[i] = bridge->effects.global_activity_factor;
        effects->layer_excitability[i] = 1.0f;
        effects->layer_connectivity[i] = 1.0f;
    }

    /* State-specific layer modulation */
    switch (state) {
        case SLEEP_STATE_AWAKE:
            /* All layers at full capacity */
            break;

        case SLEEP_STATE_DROWSY:
            /* Layer IV slightly reduced (thalamic input weakening) */
            effects->layer_activity[CC_LAYER_IV] *= 0.9f;
            /* Layer II/III beginning to show reduced lateral integration */
            effects->layer_connectivity[CC_LAYER_II_III] *= 0.8f;
            break;

        case SLEEP_STATE_LIGHT_NREM:
            /* Layer IV: Minimal thalamic processing */
            effects->layer_activity[CC_LAYER_IV] *= 0.2f;
            effects->layer_excitability[CC_LAYER_IV] = 0.3f;

            /* Layer II/III: Sleep spindles (maintained some activity) */
            effects->layer_activity[CC_LAYER_II_III] *= 0.4f;
            effects->layer_excitability[CC_LAYER_II_III] = 0.6f;

            /* Layer V: K-complex generation capability */
            effects->layer_activity[CC_LAYER_V] *= 0.3f;
            effects->layer_excitability[CC_LAYER_V] = 0.5f;

            /* Layer VI: Maintained corticothalamic loops */
            effects->layer_activity[CC_LAYER_VI] *= 0.5f;
            break;

        case SLEEP_STATE_DEEP_NREM:
            /* Layer IV: Nearly silent except during UP states */
            effects->layer_activity[CC_LAYER_IV] = LAYERS_SLEEP_L4_NREM_FACTOR;
            effects->layer_excitability[CC_LAYER_IV] = 0.1f;

            /* Layer II/III: Reduced but participates in slow oscillations */
            effects->layer_activity[CC_LAYER_II_III] = LAYERS_SLEEP_L23_NREM_FACTOR;
            effects->layer_excitability[CC_LAYER_II_III] = 0.2f;

            /* Layer V: Strong bursts during UP states */
            effects->layer_activity[CC_LAYER_V] = LAYERS_SLEEP_L5_NREM_FACTOR;
            effects->layer_excitability[CC_LAYER_V] = 0.3f; /* Bursting potential */

            /* Layer VI: Deep hyperpolarization in DOWN states */
            effects->layer_activity[CC_LAYER_VI] = LAYERS_SLEEP_L6_NREM_FACTOR;
            effects->layer_excitability[CC_LAYER_VI] = 0.1f;

            /* All connectivity reduced */
            for (int i = 0; i < CC_LAYER_COUNT; i++) {
                effects->layer_connectivity[i] = 0.2f;
            }
            break;

        case SLEEP_STATE_REM:
            /* Layer II/III: High activity (dream content) */
            effects->layer_activity[CC_LAYER_II_III] = 0.95f;
            effects->layer_excitability[CC_LAYER_II_III] = 0.9f;

            /* Layer V: Phasic activation */
            effects->layer_activity[CC_LAYER_V] = 0.8f;
            effects->layer_excitability[CC_LAYER_V] = 0.85f;

            /* Layer VI: Active but altered predictions */
            effects->layer_activity[CC_LAYER_VI] = 0.7f;
            effects->layer_excitability[CC_LAYER_VI] = 0.75f;

            /* Layer IV: Some activation but gated */
            effects->layer_activity[CC_LAYER_IV] = 0.4f;
            effects->layer_excitability[CC_LAYER_IV] = 0.5f;

            /* Lateral connectivity mostly restored */
            effects->layer_connectivity[CC_LAYER_II_III] = 0.9f;
            break;

        default:
            NIMCP_LOGGING_WARN("Unknown sleep state: %d", state);
            break;
    }
}

/**
 * WHAT: Update slow oscillation phase for deep NREM
 * WHY:  Model UP/DOWN state transitions in slow wave sleep
 * HOW:  Advance phase based on configured frequency, toggle UP/DOWN states
 *
 * BIOLOGICAL BASIS:
 * - Slow oscillations (0.5-1 Hz) are hallmark of deep NREM
 * - UP states: Synchronized depolarization, neurons fire
 * - DOWN states: Synchronized hyperpolarization, neurons silent
 * - Critical for memory consolidation and synaptic homeostasis
 */
static void update_slow_oscillation(cortical_layers_sleep_bridge_t bridge)
{
    if (bridge->effects.current_state != SLEEP_STATE_DEEP_NREM) {
        return;
    }

    /* Get current time (would come from system in real implementation) */
    /* For now, we'll use a simple phase advance */
    float dt = 0.001f; /* 1ms time step */
    float frequency = bridge->config.slow_oscillation_frequency;

    /* Advance phase: Δφ = 2π * f * Δt */
    bridge->effects.slow_oscillation_phase += 2.0f * M_PI * frequency * dt;

    /* Wrap phase to [0, 2π] */
    if (bridge->effects.slow_oscillation_phase >= 2.0f * M_PI) {
        bridge->effects.slow_oscillation_phase -= 2.0f * M_PI;
    }

    /* Determine UP/DOWN state based on phase */
    /* UP state: phase ∈ [0, π], DOWN state: phase ∈ [π, 2π] */
    bridge->effects.in_up_state = (bridge->effects.slow_oscillation_phase < M_PI);

    /* Modulate layer activity based on UP/DOWN state */
    if (bridge->effects.in_up_state) {
        /* UP state: Increase activity */
        for (int i = 0; i < CC_LAYER_COUNT; i++) {
            bridge->effects.layer_effects.layer_activity[i] *= 2.0f;
        }
    } else {
        /* DOWN state: Decrease activity */
        for (int i = 0; i < CC_LAYER_COUNT; i++) {
            bridge->effects.layer_effects.layer_activity[i] *= 0.5f;
        }
    }
}

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

int cortical_layers_sleep_default_config(cortical_layers_sleep_config_t* config)
{
    /* Guard clause: Validate config */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    config->enable_activity_modulation = true;
    config->enable_connectivity_modulation = true;
    config->enable_layer_specific = true;
    config->enable_slow_oscillations = true;
    config->modulation_strength = 1.0f;
    config->slow_oscillation_frequency = 0.75f; /* 0.75 Hz typical */

    return 0;
}

cortical_layers_sleep_bridge_t cortical_layers_sleep_bridge_create(
    const cortical_layers_sleep_config_t* config,
    laminar_structure_t* layers,
    sleep_system_t sleep)
{
    /* Guard clauses: Validate required parameters */
    if (!layers || !sleep) {
        NIMCP_LOGGING_ERROR("NULL layers or sleep system in bridge create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_layers_sleep_bridge_create: required parameter is NULL (layers, sleep)");
        return NULL;
    }

    /* Allocate bridge */
    struct cortical_layers_sleep_bridge_struct* bridge =
        (struct cortical_layers_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct cortical_layers_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate cortical layers sleep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_layers_sleep_bridge_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct cortical_layers_sleep_bridge_struct));

    /* Initialize configuration */
    if (config) {
        bridge->config = *config;
    } else {
        cortical_layers_sleep_default_config(&bridge->config);
    }

    /* Store references */
    bridge->layers = layers;
    bridge->sleep_system = sleep;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "cortical_layers_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for cortical layers sleep bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_layers_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize effects */
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.global_activity_factor = 1.0f;
    bridge->effects.feedforward_balance = 1.0f;
    bridge->effects.feedback_balance = 1.0f;
    bridge->effects.lateral_connectivity = 1.0f;
    bridge->effects.layers_offline = false;
    bridge->effects.in_up_state = true;
    bridge->effects.slow_oscillation_phase = 0.0f;

    /* Initialize layer-specific effects */
    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        bridge->effects.layer_effects.layer_activity[i] = 1.0f;
        bridge->effects.layer_effects.layer_excitability[i] = 1.0f;
        bridge->effects.layer_effects.layer_connectivity[i] = 1.0f;
    }

    /* Register callback with sleep system */
    bool registered = sleep_register_state_callback(
        sleep,
        cortical_layers_on_sleep_state_change,
        bridge);

    if (!registered) {
        NIMCP_LOGGING_WARN("Failed to register cortical layers sleep callback");
        /* Continue anyway - bridge still functional, just won't get automatic updates */
    } else {
        bridge->callback_registered = true;
        NIMCP_LOGGING_INFO("Cortical layers sleep bridge created and callback registered");
    }

    return bridge;
}

void cortical_layers_sleep_bridge_destroy(cortical_layers_sleep_bridge_t bridge)
{
    /* Guard clause: NULL safe */
    if (!bridge) {
        return;
    }

    /* Unregister callback */
    if (bridge->callback_registered) {
        sleep_unregister_state_callback(
            bridge->sleep_system,
            cortical_layers_on_sleep_state_change,
            bridge);
        NIMCP_LOGGING_DEBUG("Cortical layers sleep callback unregistered");
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_DEBUG("Cortical layers sleep bridge destroyed");
}

/* ========================================================================
 * STATE UPDATE FUNCTIONS
 * ======================================================================== */

int cortical_layers_sleep_update(cortical_layers_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update layer-specific effects if enabled */
    if (bridge->config.enable_layer_specific) {
        update_layer_specific_effects(bridge);
    }

    /* Update slow oscillations if in deep NREM */
    if (bridge->config.enable_slow_oscillations) {
        update_slow_oscillation(bridge);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int cortical_layers_sleep_apply_modulation(cortical_layers_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Copy state under lock, then call external functions outside lock
     * to avoid potential deadlock if external code acquires other locks */
    nimcp_mutex_lock(bridge->base.mutex);

    laminar_structure_t* layers = bridge->layers;
    bool do_connectivity = bridge->config.enable_connectivity_modulation;
    float ff_balance = bridge->effects.feedforward_balance;
    float fb_balance = bridge->effects.feedback_balance;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Apply feedforward modulation (outside lock) */
    if (do_connectivity) {
        /* Layer IV -> Layer II/III */
        laminar_connect_feedforward(layers, CC_LAYER_IV, CC_LAYER_II_III,
                                    1.0f * ff_balance);

        /* Layer II/III -> Layer V */
        laminar_connect_feedforward(layers, CC_LAYER_II_III, CC_LAYER_V,
                                    0.8f * ff_balance);

        /* Layer V -> Layer VI */
        laminar_connect_feedforward(layers, CC_LAYER_V, CC_LAYER_VI,
                                    0.7f * ff_balance);
    }

    /* Apply feedback modulation (outside lock) */
    if (do_connectivity) {
        /* Layer VI -> Layer IV */
        laminar_connect_feedback(layers, CC_LAYER_VI, CC_LAYER_IV,
                                0.5f * fb_balance);

        /* Layer I -> Layer II/III */
        laminar_connect_feedback(layers, CC_LAYER_I, CC_LAYER_II_III,
                                0.4f * fb_balance);
    }

    return 0;
}

/* ========================================================================
 * STATE ACCESS FUNCTIONS
 * ======================================================================== */

int cortical_layers_sleep_get_effects(
    const cortical_layers_sleep_bridge_t bridge,
    cortical_layers_sleep_effects_t* effects)
{
    /* Guard clauses: Validate parameters */
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_layers_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float cortical_layers_sleep_get_activity_factor(const cortical_layers_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        return -1.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->effects.global_activity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

bool cortical_layers_sleep_is_offline(const cortical_layers_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool offline = bridge->effects.layers_offline;
    nimcp_mutex_unlock(bridge->base.mutex);

    return offline;
}

bool cortical_layers_sleep_is_up_state(const cortical_layers_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool up_state = bridge->effects.in_up_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return up_state;
}

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

float cortical_layers_sleep_activity_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return LAYERS_SLEEP_ACTIVITY_AWAKE;
        case SLEEP_STATE_DROWSY:
            return LAYERS_SLEEP_ACTIVITY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return LAYERS_SLEEP_ACTIVITY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return LAYERS_SLEEP_ACTIVITY_DEEP_NREM;
        case SLEEP_STATE_REM:
            return LAYERS_SLEEP_ACTIVITY_REM;
        default:
            return LAYERS_SLEEP_ACTIVITY_AWAKE;
    }
}

float cortical_layers_sleep_feedforward_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:
        case SLEEP_STATE_DROWSY:
            return LAYERS_SLEEP_FF_BALANCE_AWAKE;
        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:
            return LAYERS_SLEEP_FF_BALANCE_NREM;
        case SLEEP_STATE_REM:
            return LAYERS_SLEEP_FF_BALANCE_REM;
        default:
            return LAYERS_SLEEP_FF_BALANCE_AWAKE;
    }
}

float cortical_layers_sleep_feedback_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:
        case SLEEP_STATE_DROWSY:
            return LAYERS_SLEEP_FB_BALANCE_AWAKE;
        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:
            return LAYERS_SLEEP_FB_BALANCE_NREM;
        case SLEEP_STATE_REM:
            return LAYERS_SLEEP_FB_BALANCE_REM;
        default:
            return LAYERS_SLEEP_FB_BALANCE_AWAKE;
    }
}
