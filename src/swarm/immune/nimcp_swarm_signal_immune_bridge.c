/**
 * @file nimcp_swarm_signal_immune_bridge.c
 * @brief Swarm Signal-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Modulates swarm signal transmission based on immune state
 * WHY:  Inflammation affects signal quality; corruption triggers immune
 * HOW:  Cytokines increase packet loss; corrupted signals trigger cleanup
 *
 * @author NIMCP Development Team
 */

#include "swarm/immune/nimcp_swarm_signal_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_signal_immune_bridge)

struct swarm_signal_immune_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    swarm_signal_immune_config_t config;
    brain_immune_system_t* immune_system;
    void* swarm_signal;
    cytokine_signal_effects_t cytokine_effects;
    inflammation_signal_state_t inflammation_state;
    signal_immune_modulation_t modulation;
    void* bio_ctx;
    bool bio_async_connected;
};

static float get_signal_factor_for_level(brain_inflammation_level_t level)
{
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_NONE_SIGNAL_FACTOR,
        INFLAMMATION_STORM_SIGNAL_FACTOR);
}

int swarm_signal_immune_default_config(swarm_signal_immune_config_t* config)
{
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_cytokine_effects = true;
    config->enable_inflammation_effects = true;
    config->enable_corruption_detection = true;
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

swarm_signal_immune_bridge_t* swarm_signal_immune_bridge_create(
    const swarm_signal_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_signal)
{
    if (!config || !immune_system) {
        NIMCP_LOGGING_ERROR("Invalid parameters for swarm signal immune bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_signal_immune_bridge_create: required parameter is NULL (config, immune_system)");
        return NULL;
    }

    swarm_signal_immune_bridge_t* bridge =
        (swarm_signal_immune_bridge_t*)nimcp_malloc(sizeof(swarm_signal_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm signal immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_signal_immune_bridge_create: allocation failed");

        return NULL;
    }

    memset(bridge, 0, sizeof(swarm_signal_immune_bridge_t));
    memcpy(&bridge->config, config, sizeof(swarm_signal_immune_config_t));
    bridge->immune_system = immune_system;
    bridge->swarm_signal = swarm_signal;

    if (bridge_base_init(&bridge->base, 0, "swarm_signal_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for swarm signal immune bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_signal_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->cytokine_effects.packet_loss_increase = 0.0f;
    bridge->cytokine_effects.signal_strength_reduction = 0.0f;
    bridge->cytokine_effects.latency_increase = 0.0f;
    bridge->cytokine_effects.noise_floor_increase = 0.0f;

    bridge->inflammation_state.current_level = INFLAMMATION_NONE;
    bridge->inflammation_state.signal_quality_factor = 1.0f;
    bridge->inflammation_state.transmission_efficiency = 1.0f;
    bridge->inflammation_state.critical_signals_only = false;

    bridge->modulation.corrupted_packets = 0;
    bridge->modulation.corruption_rate = 0.0f;
    bridge->modulation.immune_activated = false;
    bridge->modulation.cleanup_signal = 0.0f;

    NIMCP_LOGGING_INFO("Swarm signal immune bridge created");
    return bridge;
}

void swarm_signal_immune_bridge_destroy(swarm_signal_immune_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Swarm signal immune bridge destroyed");
}

int swarm_signal_immune_apply_cytokine_effects(swarm_signal_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_signal_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_cytokine_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    float sensitivity = bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.packet_loss_increase =
        (CYTOKINE_IL1_PACKET_LOSS + CYTOKINE_IL6_PACKET_LOSS +
         CYTOKINE_TNF_PACKET_LOSS - CYTOKINE_IL10_SIGNAL_BOOST) * sensitivity;

    bridge->cytokine_effects.signal_strength_reduction =
        bridge->cytokine_effects.packet_loss_increase * 2.0f;
    bridge->cytokine_effects.latency_increase =
        bridge->cytokine_effects.packet_loss_increase * 1.5f;
    bridge->cytokine_effects.noise_floor_increase =
        bridge->cytokine_effects.packet_loss_increase;

    if (bridge->cytokine_effects.packet_loss_increase < 0.0f) {
        bridge->cytokine_effects.packet_loss_increase = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied cytokine signal effects: packet_loss=%.2f",
                        bridge->cytokine_effects.packet_loss_increase);
    return 0;
}

int swarm_signal_immune_apply_inflammation_effects(swarm_signal_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_signal_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_inflammation_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_signal_immune_apply_inflammation_effects: validation failed");
        return -1;
    }
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.signal_quality_factor = get_signal_factor_for_level(level);
    bridge->inflammation_state.transmission_efficiency =
        bridge->inflammation_state.signal_quality_factor;
    bridge->inflammation_state.critical_signals_only =
        (level >= INFLAMMATION_SYSTEMIC);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied inflammation signal effects: level=%d, quality=%.2f",
                        level, bridge->inflammation_state.signal_quality_factor);
    return 0;
}

int swarm_signal_immune_report_corruption(swarm_signal_immune_bridge_t* bridge, uint32_t count)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_corruption_detection) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->modulation.corrupted_packets += count;
    bridge->modulation.corruption_rate = (float)count / 100.0f;

    if (bridge->modulation.corruption_rate > 0.1f) {
        bridge->modulation.immune_activated = true;
        NIMCP_LOGGING_INFO("Signal corruption activated immune response: rate=%.2f",
                          bridge->modulation.corruption_rate);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_signal_immune_boost_from_clean_path(swarm_signal_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->modulation.cleanup_signal += 0.05f;
    bridge->modulation.corrupted_packets = 0;
    bridge->modulation.immune_activated = false;

    NIMCP_LOGGING_DEBUG("Clean signal path boosted cleanup: total=%.2f",
                       bridge->modulation.cleanup_signal);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_signal_immune_bridge_update(swarm_signal_immune_bridge_t* bridge, uint64_t delta_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms;

    swarm_signal_immune_apply_cytokine_effects(bridge);
    swarm_signal_immune_apply_inflammation_effects(bridge);

    return 0;
}

float swarm_signal_immune_get_quality_factor(const swarm_signal_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->inflammation_state.signal_quality_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

float swarm_signal_immune_get_packet_loss(const swarm_signal_immune_bridge_t* bridge)
{
    if (!bridge) return 0.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float loss = bridge->cytokine_effects.packet_loss_increase;
    nimcp_mutex_unlock(bridge->base.mutex);

    return loss;
}

bool swarm_signal_immune_is_critical_only(const swarm_signal_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool critical = bridge->inflammation_state.critical_signals_only;
    nimcp_mutex_unlock(bridge->base.mutex);

    return critical;
}

int swarm_signal_immune_connect_bio_async(swarm_signal_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect to bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_signal_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->bio_async_connected) {
        NIMCP_LOGGING_INFO("Swarm signal-immune bridge already connected to bio-async");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SWARM_SIGNAL,
        .module_name = "swarm_signal_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_INFO("Swarm signal-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

int swarm_signal_immune_disconnect_bio_async(swarm_signal_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect from bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_signal_immune_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    if (!bridge->bio_async_connected) {
        return 0;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->bio_async_connected = false;
    NIMCP_LOGGING_INFO("Swarm signal-immune bridge disconnected from bio-async router");

    return 0;
}

bool swarm_signal_immune_is_bio_async_connected(const swarm_signal_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}
