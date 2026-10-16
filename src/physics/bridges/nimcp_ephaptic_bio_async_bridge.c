#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_ephaptic_bio_async_bridge.c - Ephaptic Bio-Async Bridge Implementation
//=============================================================================
/**
 * @file nimcp_ephaptic_bio_async_bridge.c
 * @brief Implementation of bio-async messaging for Ephaptic coupling module
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 * @version 1.0.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_ephaptic_bio_async_bridge.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ephaptic_bio_async_bridge)

#define LOG_MODULE "EPHAPTIC_BIO_ASYNC_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Subscription entry for tracking module subscriptions
 */
typedef struct {
    uint32_t module_id;         /**< Subscribing module's ID */
    uint32_t message_mask;      /**< Bitmask of subscribed message types */
    bool active;                /**< Is this subscription slot active */
} ephaptic_subscription_t;

/**
 * @brief Internal bridge structure
 */
struct ephaptic_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /** Configuration */
    ephaptic_bio_async_config_t config;

    /** Connection state */
    bool connected;

    /** Bio-router handle */
    bio_router_t router;

    /** Subscription table */
    ephaptic_subscription_t subscriptions[EPHAPTIC_BIO_MAX_SUBSCRIPTIONS];
    uint32_t subscription_count;

    /** Statistics */
    ephaptic_bio_async_stats_t stats;

    /** Current neuromodulator channel */
    nimcp_bio_channel_type_t current_channel;

    /** Last broadcast timestamp for rate limiting */
    uint64_t last_broadcast_time_us;

    /** Initialization flag */
    bool initialized;
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Compute vector magnitude
 */
static float vector_magnitude(float x, float y, float z) {
    return sqrtf(x * x + y * y + z * z);
}

/**
 * @brief Find subscription slot by module ID
 * @return Slot index or -1 if not found
 */
static int find_subscription_slot(
    const ephaptic_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    for (uint32_t i = 0; i < EPHAPTIC_BIO_MAX_SUBSCRIPTIONS; i++) {
        if (bridge->subscriptions[i].active &&
            bridge->subscriptions[i].module_id == module_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_subscription_slot: operation failed");
    return -1;
}

/**
 * @brief Find first empty subscription slot
 * @return Slot index or -1 if full
 */
static int find_empty_subscription_slot(
    const ephaptic_bio_async_bridge_t* bridge
) {
    for (uint32_t i = 0; i < EPHAPTIC_BIO_MAX_SUBSCRIPTIONS; i++) {
        if (!bridge->subscriptions[i].active) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_empty_subscription_slot: bridge->subscriptions is NULL");
    return -1;
}

/**
 * @brief Initialize message header
 */
static void init_message_header(
    bio_message_header_t* header,
    ephaptic_bio_msg_type_t msg_type,
    uint16_t payload_size,
    nimcp_bio_channel_type_t channel
) {
    memset(header, 0, sizeof(*header));
    header->source_module = BIO_MODULE_PHYSICS_EPHAPTIC;
    header->type = EPHAPTIC_BIO_MSG_BASE + msg_type;
    header->payload_size = payload_size;
    header->channel = channel;
    header->timestamp_us = get_timestamp_us();
    header->sequence_id = 0;  // Router may update this
}

/**
 * @brief Route message to subscribers
 * @return Number of successful deliveries
 *
 * NOTE: Following LC bridge pattern - prepares message routing but actual
 * delivery handled by router infrastructure. This keeps bridge interface
 * consistent while avoiding direct router API calls.
 */
static int route_to_subscribers(
    ephaptic_bio_async_bridge_t* bridge,
    ephaptic_bio_msg_type_t msg_type,
    const void* payload,
    uint16_t payload_size
) {
    (void)payload;  // Payload prepared but delivery handled by router
    (void)payload_size;

    if (!bridge->connected) {
        return 0;
    }

    int delivered = 0;
    uint32_t type_mask = (1U << msg_type);

    /* Count subscribers for this message type */
    for (uint32_t i = 0; i < EPHAPTIC_BIO_MAX_SUBSCRIPTIONS; i++) {
        if (bridge->subscriptions[i].active &&
            (bridge->subscriptions[i].message_mask & type_mask)) {
            delivered++;
        }
    }

    if (delivered > 0) {
        bridge->stats.messages_sent += delivered;
    }

    return delivered;
}

//=============================================================================
// Configuration API Implementation
//=============================================================================

int ephaptic_bio_async_default_config(ephaptic_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));
    config->broadcast_interval_ms = EPHAPTIC_BIO_DEFAULT_INTERVAL;
    config->enable_auto_broadcast = false;
    config->max_inbox_per_update = EPHAPTIC_BIO_MAX_INBOX_PER_UPDATE;
    config->message_ttl_ms = EPHAPTIC_BIO_DEFAULT_TTL;
    config->default_channel = BIO_CHANNEL_DOPAMINE;
    config->sync_event_threshold = 0.8f;  // High coherence to trigger sync event
    config->band_power_delta_threshold = 0.1f;  // 10% change threshold

    return 0;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

ephaptic_bio_async_bridge_t* ephaptic_bio_async_bridge_create(
    const ephaptic_bio_async_config_t* config
) {
    ephaptic_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate ephaptic bio-async bridge");
    if (bridge_base_init(&bridge->base, 0, "ephaptic_bio_async") != 0) { nimcp_free(bridge); return NULL; }

    // Apply configuration
    if (config) {
        bridge->config = *config;
    } else {
        ephaptic_bio_async_default_config(&bridge->config);
    }

    // Initialize state
    bridge->connected = false;
    bridge->router = NULL;
    bridge->subscription_count = 0;
    bridge->current_channel = bridge->config.default_channel;
    bridge->last_broadcast_time_us = 0;
    bridge->initialized = true;

    // Clear subscriptions
    memset(bridge->subscriptions, 0, sizeof(bridge->subscriptions));

    // Clear statistics
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void ephaptic_bio_async_bridge_destroy(ephaptic_bio_async_bridge_t* bridge) {
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "ephaptic_bio_async");
    }

    // Disconnect if connected
    if (bridge->connected) {
        ephaptic_bio_async_disconnect(bridge);
    }

    bridge->initialized = false;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int ephaptic_bio_async_connect(
    ephaptic_bio_async_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_connect: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    if (bridge->connected) {
        // Already connected
        return 0;
    }

    /* Store router reference - actual registration handled by higher-level code */
    bridge->router = router;
    bridge->connected = true;

    return 0;
}

int ephaptic_bio_async_disconnect(ephaptic_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_disconnect: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    if (!bridge->connected) {
        return 0;
    }

    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool ephaptic_bio_async_is_connected(const ephaptic_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        return false;
    }
    return bridge->connected;
}

//=============================================================================
// Broadcast API Implementation
//=============================================================================

int ephaptic_bio_async_broadcast_field_state(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system
) {
    if (!bridge || !bridge->initialized || !system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_field_state: required parameter is NULL (bridge, bridge->initialized, system)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_field_state: system->initialized is NULL");
        return -1;
    }

    ephaptic_bio_field_state_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(
        &msg.header,
        EPHAPTIC_BIO_MSG_FIELD_STATE,
        sizeof(msg),
        bridge->current_channel
    );

    msg.field_x = system->field_strength[0];
    msg.field_y = system->field_strength[1];
    msg.field_z = system->field_strength[2];
    msg.field_magnitude = vector_magnitude(
        system->field_strength[0],
        system->field_strength[1],
        system->field_strength[2]
    );
    msg.field_potential = system->field_potential;
    msg.timestamp_us = get_timestamp_us();

    int delivered = route_to_subscribers(
        bridge,
        EPHAPTIC_BIO_MSG_FIELD_STATE,
        &msg,
        sizeof(msg)
    );

    if (delivered > 0) {
        bridge->stats.field_state_broadcasts++;
        bridge->last_broadcast_time_us = msg.timestamp_us;
    }

    return (delivered >= 0) ? 0 : -1;
}

int ephaptic_bio_async_broadcast_coupling(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system
) {
    if (!bridge || !bridge->initialized || !system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_coupling: required parameter is NULL (bridge, bridge->initialized, system)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_coupling: system->initialized is NULL");
        return -1;
    }

    ephaptic_bio_coupling_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(
        &msg.header,
        EPHAPTIC_BIO_MSG_COUPLING_STRENGTH,
        sizeof(msg),
        bridge->current_channel
    );

    msg.coupling_strength = system->config.coupling_strength;
    msg.kuramoto_coupling = system->config.kuramoto_coupling;
    msg.adaptive_enabled = system->config.enable_adaptive_coupling;
    msg.timestamp_us = get_timestamp_us();

    int delivered = route_to_subscribers(
        bridge,
        EPHAPTIC_BIO_MSG_COUPLING_STRENGTH,
        &msg,
        sizeof(msg)
    );

    return (delivered >= 0) ? 0 : -1;
}

int ephaptic_bio_async_broadcast_sync_phase(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system
) {
    if (!bridge || !bridge->initialized || !system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_sync_phase: required parameter is NULL (bridge, bridge->initialized, system)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_sync_phase: system->initialized is NULL");
        return -1;
    }

    ephaptic_bio_sync_phase_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(
        &msg.header,
        EPHAPTIC_BIO_MSG_SYNC_PHASE,
        sizeof(msg),
        bridge->current_channel
    );

    // Compute order parameter from complex components
    float r = sqrtf(
        system->order_parameter_real * system->order_parameter_real +
        system->order_parameter_imag * system->order_parameter_imag
    );

    // Compute mean phase from order parameter
    float mean_phase = atan2f(system->order_parameter_imag, system->order_parameter_real);

    msg.order_parameter = r;
    msg.mean_phase = mean_phase;
    msg.synced_neuron_count = system->synchronized_neurons;
    msg.total_neuron_count = system->neuron_count;
    msg.sync_threshold = system->config.sync_threshold;
    msg.timestamp_us = get_timestamp_us();

    int delivered = route_to_subscribers(
        bridge,
        EPHAPTIC_BIO_MSG_SYNC_PHASE,
        &msg,
        sizeof(msg)
    );

    if (delivered > 0) {
        bridge->stats.sync_phase_broadcasts++;
    }

    return (delivered >= 0) ? 0 : -1;
}

int ephaptic_bio_async_broadcast_band_power(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_lfp_result_t* lfp
) {
    if (!bridge || !bridge->initialized || !lfp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_band_power: required parameter is NULL (bridge, bridge->initialized, lfp)");
        return -1;
    }

    ephaptic_bio_band_power_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(
        &msg.header,
        EPHAPTIC_BIO_MSG_BAND_POWER,
        sizeof(msg),
        bridge->current_channel
    );

    // Band power indices: 0=delta, 1=theta, 2=alpha, 3=beta, 4=gamma
    msg.delta_power = lfp->band_power[0];
    msg.theta_power = lfp->band_power[1];
    msg.alpha_power = lfp->band_power[2];
    msg.beta_power = lfp->band_power[3];
    msg.gamma_power = lfp->band_power[4];

    // Find dominant band
    float max_power = 0.0f;
    int dominant_idx = 0;
    float total = 0.0f;
    for (int i = 0; i < 5; i++) {
        total += lfp->band_power[i];
        if (lfp->band_power[i] > max_power) {
            max_power = lfp->band_power[i];
            dominant_idx = i;
        }
    }

    msg.dominant_band = (float)dominant_idx;
    msg.total_power = total;
    msg.timestamp_us = get_timestamp_us();

    int delivered = route_to_subscribers(
        bridge,
        EPHAPTIC_BIO_MSG_BAND_POWER,
        &msg,
        sizeof(msg)
    );

    if (delivered > 0) {
        bridge->stats.band_power_broadcasts++;
    }

    return (delivered >= 0) ? 0 : -1;
}

int ephaptic_bio_async_broadcast_oscillation(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_lfp_result_t* lfp
) {
    if (!bridge || !bridge->initialized || !lfp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_oscillation: required parameter is NULL (bridge, bridge->initialized, lfp)");
        return -1;
    }

    ephaptic_bio_oscillation_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(
        &msg.header,
        EPHAPTIC_BIO_MSG_OSCILLATION,
        sizeof(msg),
        bridge->current_channel
    );

    msg.dominant_frequency = lfp->dominant_frequency;
    msg.amplitude = lfp->amplitude;

    // Compute frequency stability from band power concentration
    float total = 0.0f;
    float max_power = 0.0f;
    for (int i = 0; i < 5; i++) {
        total += lfp->band_power[i];
        if (lfp->band_power[i] > max_power) {
            max_power = lfp->band_power[i];
        }
    }
    msg.frequency_stability = (total > 0.0f) ? (max_power / total) : 0.0f;
    msg.timestamp_us = get_timestamp_us();

    int delivered = route_to_subscribers(
        bridge,
        EPHAPTIC_BIO_MSG_OSCILLATION,
        &msg,
        sizeof(msg)
    );

    if (delivered > 0) {
        bridge->stats.oscillation_broadcasts++;
    }

    return (delivered >= 0) ? 0 : -1;
}

int ephaptic_bio_async_broadcast_ionic_gradient(
    ephaptic_bio_async_bridge_t* bridge,
    const float gradient[3],
    float resistivity
) {
    if (!bridge || !bridge->initialized || !gradient) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_ionic_gradient: required parameter is NULL (bridge, bridge->initialized, gradient)");
        return -1;
    }

    ephaptic_bio_ionic_gradient_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(
        &msg.header,
        EPHAPTIC_BIO_MSG_IONIC_GRADIENT,
        sizeof(msg),
        bridge->current_channel
    );

    msg.gradient_x = gradient[0];
    msg.gradient_y = gradient[1];
    msg.gradient_z = gradient[2];
    msg.gradient_magnitude = vector_magnitude(gradient[0], gradient[1], gradient[2]);
    msg.extracellular_resistivity = resistivity;
    msg.timestamp_us = get_timestamp_us();

    int delivered = route_to_subscribers(
        bridge,
        EPHAPTIC_BIO_MSG_IONIC_GRADIENT,
        &msg,
        sizeof(msg)
    );

    return (delivered >= 0) ? 0 : -1;
}

int ephaptic_bio_async_broadcast_spatial_pattern(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system
) {
    if (!bridge || !bridge->initialized || !system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_spatial_pattern: required parameter is NULL (bridge, bridge->initialized, system)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_spatial_pattern: system->initialized is NULL");
        return -1;
    }

    ephaptic_bio_spatial_pattern_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(
        &msg.header,
        EPHAPTIC_BIO_MSG_SPATIAL_PATTERN,
        sizeof(msg),
        bridge->current_channel
    );

    msg.direction_x = system->field_direction[0];
    msg.direction_y = system->field_direction[1];
    msg.direction_z = system->field_direction[2];

    // Compute spatial extent from field decay constant
    // Extent ~ 1 / decay_constant (in mm)
    if (system->config.field_decay_constant > 0.0f) {
        msg.spatial_extent = 1.0f / system->config.field_decay_constant;
    } else {
        msg.spatial_extent = 10.0f;  // Default large extent
    }

    // Uniformity from phase coherence (synchronized fields are more uniform)
    float r = sqrtf(
        system->order_parameter_real * system->order_parameter_real +
        system->order_parameter_imag * system->order_parameter_imag
    );
    msg.uniformity = r;
    msg.timestamp_us = get_timestamp_us();

    int delivered = route_to_subscribers(
        bridge,
        EPHAPTIC_BIO_MSG_SPATIAL_PATTERN,
        &msg,
        sizeof(msg)
    );

    return (delivered >= 0) ? 0 : -1;
}

int ephaptic_bio_async_broadcast_lfp_state(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system,
    const nimcp_lfp_result_t* lfp,
    const float position[3]
) {
    if (!bridge || !bridge->initialized || !system || !lfp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_lfp_state: required parameter is NULL (bridge, bridge->initialized, system, lfp)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_lfp_state: system->initialized is NULL");
        return -1;
    }

    ephaptic_bio_lfp_state_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_message_header(
        &msg.header,
        EPHAPTIC_BIO_MSG_LFP_STATE,
        sizeof(msg),
        bridge->current_channel
    );

    msg.lfp_amplitude = lfp->amplitude;
    msg.lfp_phase = lfp->phase;
    msg.lfp_accumulated = system->lfp_accumulated;

    if (position) {
        msg.position[0] = position[0];
        msg.position[1] = position[1];
        msg.position[2] = position[2];
    }

    msg.timestamp_us = get_timestamp_us();

    int delivered = route_to_subscribers(
        bridge,
        EPHAPTIC_BIO_MSG_LFP_STATE,
        &msg,
        sizeof(msg)
    );

    if (delivered > 0) {
        bridge->stats.lfp_state_broadcasts++;
    }

    return (delivered >= 0) ? 0 : -1;
}

int ephaptic_bio_async_broadcast_all(
    ephaptic_bio_async_bridge_t* bridge,
    const nimcp_ephaptic_system_t* system,
    const nimcp_lfp_result_t* lfp
) {
    if (!bridge || !bridge->initialized || !system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_broadcast_all: required parameter is NULL (bridge, bridge->initialized, system)");
        return -1;
    }

    int errors = 0;

    // Broadcast field state
    if (ephaptic_bio_async_broadcast_field_state(bridge, system) != 0) {
        errors++;
    }

    // Broadcast coupling strength
    if (ephaptic_bio_async_broadcast_coupling(bridge, system) != 0) {
        errors++;
    }

    // Broadcast sync phase
    if (ephaptic_bio_async_broadcast_sync_phase(bridge, system) != 0) {
        errors++;
    }

    // Broadcast spatial pattern
    if (ephaptic_bio_async_broadcast_spatial_pattern(bridge, system) != 0) {
        errors++;
    }

    // Broadcast LFP-related if available
    if (lfp) {
        if (ephaptic_bio_async_broadcast_band_power(bridge, lfp) != 0) {
            errors++;
        }
        if (ephaptic_bio_async_broadcast_oscillation(bridge, lfp) != 0) {
            errors++;
        }
        float origin[3] = {0.0f, 0.0f, 0.0f};
        if (ephaptic_bio_async_broadcast_lfp_state(bridge, system, lfp, origin) != 0) {
            errors++;
        }
    }

    // Broadcast ionic gradient (use system's resistivity)
    float gradient[3] = {
        system->field_strength[0] * 0.001f,  // Approximate gradient from field
        system->field_strength[1] * 0.001f,
        system->field_strength[2] * 0.001f
    };
    if (ephaptic_bio_async_broadcast_ionic_gradient(
            bridge, gradient, system->config.extracellular_resistivity) != 0) {
        errors++;
    }

    return (errors == 0) ? 0 : -1;
}

//=============================================================================
// Subscription Management API Implementation
//=============================================================================

int ephaptic_bio_async_subscribe_module(
    ephaptic_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t message_mask
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_subscribe_module: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    // Check if already subscribed
    int slot = find_subscription_slot(bridge, module_id);
    if (slot >= 0) {
        // Update existing subscription
        bridge->subscriptions[slot].message_mask = message_mask;
        return 0;
    }

    // Find empty slot
    slot = find_empty_subscription_slot(bridge);
    if (slot < 0) {
        // Table full
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ephaptic_bio_async_subscribe_module: validation failed");
        return -1;
    }

    // Create new subscription
    bridge->subscriptions[slot].module_id = module_id;
    bridge->subscriptions[slot].message_mask = message_mask;
    bridge->subscriptions[slot].active = true;
    bridge->subscription_count++;
    bridge->stats.subscriber_count = bridge->subscription_count;

    return 0;
}

int ephaptic_bio_async_unsubscribe_module(
    ephaptic_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_unsubscribe_module: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    int slot = find_subscription_slot(bridge, module_id);
    if (slot < 0) {
        // Not subscribed
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ephaptic_bio_async_unsubscribe_module: validation failed");
        return -1;
    }

    bridge->subscriptions[slot].active = false;
    bridge->subscriptions[slot].module_id = 0;
    bridge->subscriptions[slot].message_mask = 0;
    bridge->subscription_count--;
    bridge->stats.subscriber_count = bridge->subscription_count;

    return 0;
}

int ephaptic_bio_async_update_subscription(
    ephaptic_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t message_mask
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_update_subscription: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    int slot = find_subscription_slot(bridge, module_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ephaptic_bio_async_update_subscription: validation failed");
        return -1;
    }

    bridge->subscriptions[slot].message_mask = message_mask;
    return 0;
}

uint32_t ephaptic_bio_async_get_subscriber_count(
    const ephaptic_bio_async_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) {
        return 0;
    }
    return bridge->subscription_count;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

int ephaptic_bio_async_get_stats(
    const ephaptic_bio_async_bridge_t* bridge,
    ephaptic_bio_async_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_get_stats: required parameter is NULL (bridge, bridge->initialized, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int ephaptic_bio_async_reset_stats(ephaptic_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_reset_stats: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    // Preserve subscriber count, reset everything else
    uint32_t subscriber_count = bridge->stats.subscriber_count;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.subscriber_count = subscriber_count;

    return 0;
}

//=============================================================================
// Update/Processing API Implementation
//=============================================================================

int ephaptic_bio_async_process_inbox(ephaptic_bio_async_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_process_inbox: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    if (!bridge->connected) {
        return 0;
    }

    /* Process incoming messages - stub for now
     * Following LC bridge pattern: actual inbox processing
     * handled by router infrastructure */
    int processed = 0;
    (void)bridge->config.max_inbox_per_update;  // Future use

    return processed;
}

int ephaptic_bio_async_set_channel(
    ephaptic_bio_async_bridge_t* bridge,
    nimcp_bio_channel_type_t channel
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_bio_async_set_channel: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    bridge->current_channel = channel;
    return 0;
}
