/**
 * @file nimcp_snn_emotional_tagging_bridge.c
 * @brief SNN-Emotional Tagging integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_emotional_tagging_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_emotional_tagging_bridge)

#define BIO_MODULE_SNN_EMOTIONAL_TAGGING_BRIDGE 0x0621
#define DEFAULT_MAX_TAGS 1024

void snn_emotional_tagging_config_default(snn_emotional_tagging_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_config_default: null config pointer");
        return;
    }

    config->tagging_threshold = 50.0f;
    config->emotional_decay_rate = 0.1f;
    config->max_tag_intensity = 1.0f;
    config->synchrony_weight = 0.4f;
    config->burst_weight = 0.6f;
    config->ltp_enhancement_factor = 2.0f;
    config->tagging_pop_id = 0;
    config->memory_pop_id = 0;
    config->consolidation_window_ms = 1000.0f;
    config->enable_synaptic_tagging = true;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
}

snn_emotional_tagging_bridge_t* snn_emotional_tagging_bridge_create(
    const snn_emotional_tagging_config_t* config,
    snn_network_t* snn,
    emotional_tagging_system_t* tagging_system
) {
    if (!config || !snn) {
        NIMCP_LOGGING_ERROR("Null parameters to snn_emotional_tagging_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_bridge_create: config/snn is NULL");
        return NULL;
    }

    snn_emotional_tagging_bridge_t* bridge = nimcp_malloc(sizeof(snn_emotional_tagging_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-emotional-tagging bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_emotional_tagging_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_emotional_tagging_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_emotional_tagging") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->tagging_system = tagging_system;
    bridge->config = *config;
    bridge->max_tags = DEFAULT_MAX_TAGS;

    bridge->tags = nimcp_malloc(sizeof(snn_emotional_tag_t) * bridge->max_tags);
    if (!bridge->tags) {
        NIMCP_LOGGING_ERROR("Failed to allocate tag array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_emotional_tagging_bridge_create: failed to allocate tags");
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->tags, 0, sizeof(snn_emotional_tag_t) * bridge->max_tags);

    if (config->tagging_pop_id > 0) {
        bridge->tagging_pop = snn_network_get_population(snn, config->tagging_pop_id);
    }
    if (config->memory_pop_id > 0) {
        bridge->memory_pop = snn_network_get_population(snn, config->memory_pop_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-emotional-tagging bridge");
    return bridge;
}

void snn_emotional_tagging_bridge_destroy(snn_emotional_tagging_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_bridge_destroy: null bridge pointer");
        return;
    }

    if (bridge->base.bio_async_enabled) {
        snn_emotional_tagging_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);
    if (bridge->tags) nimcp_free(bridge->tags);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-emotional-tagging bridge");
}

int snn_emotional_tagging_bridge_connect_bio_async(snn_emotional_tagging_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_bridge_connect_bio_async: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_EMOTIONAL_TAGGING_BRIDGE,
        .module_name = "snn_emotional_tagging_bridge",
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
    return SNN_ERROR_OPERATION_FAILED;
}

int snn_emotional_tagging_bridge_disconnect_bio_async(snn_emotional_tagging_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_emotional_tagging_bridge_is_bio_async_connected(const snn_emotional_tagging_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_emotional_tagging_bridge_update(snn_emotional_tagging_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_bridge_update: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    /* Detect burst activity */
    float burst_rate = 0.0f, synchrony = 0.0f;
    snn_emotional_tagging_detect_burst(bridge, &burst_rate, &synchrony);

    bridge->state.burst_rate = burst_rate;
    bridge->state.spike_synchrony = synchrony;

    /* Compute tagging intensity */
    float tag_intensity = bridge->config.burst_weight * (burst_rate / bridge->config.tagging_threshold) +
                          bridge->config.synchrony_weight * synchrony;
    if (tag_intensity > bridge->config.max_tag_intensity) {
        tag_intensity = bridge->config.max_tag_intensity;
    }
    bridge->state.current_tag_intensity = tag_intensity;

    /* Decay existing tags */
    snn_emotional_tagging_decay_tags(bridge, dt);

    /* Update statistics */
    bridge->state.avg_tag_intensity = bridge->state.avg_tag_intensity * 0.95f + tag_intensity * 0.05f;
    if (!isfinite(bridge->state.avg_tag_intensity)) bridge->state.avg_tag_intensity = tag_intensity;

    /* Count active tags */
    uint32_t active = 0, consolidated = 0;
    for (uint32_t i = 0; i < bridge->tag_count; i++) {
        if (bridge->tags[i].intensity > 0.01f) active++;
        if (bridge->tags[i].consolidated) consolidated++;
    }
    bridge->state.active_tags = active;
    bridge->state.consolidated_tags = consolidated;

    return 0;
}

int snn_emotional_tagging_detect_burst(
    snn_emotional_tagging_bridge_t* bridge,
    float* burst_rate_out,
    float* synchrony_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_detect_burst: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    float burst_rate = 0.0f;
    float synchrony = 0.0f;

    if (bridge->tagging_pop) {
        burst_rate = snn_population_get_firing_rate(bridge->tagging_pop);
        /* Estimate synchrony from coefficient of variation (simplified) */
        synchrony = (burst_rate > 10.0f) ? 0.5f + 0.3f * (burst_rate / bridge->config.tagging_threshold) : 0.0f;
        if (synchrony > 1.0f) synchrony = 1.0f;
    }

    if (burst_rate_out) *burst_rate_out = burst_rate;
    if (synchrony_out) *synchrony_out = synchrony;

    return 0;
}

int snn_emotional_tagging_create_tag(
    snn_emotional_tagging_bridge_t* bridge,
    uint32_t event_id,
    float initial_intensity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_create_tag: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->tag_count >= bridge->max_tags) {
        NIMCP_LOGGING_WARN("Tag array full, cannot create new tag");
        return SNN_ERROR_OPERATION_FAILED;
    }

    /* Check if tag already exists */
    for (uint32_t i = 0; i < bridge->tag_count; i++) {
        if (bridge->tags[i].event_id == event_id) {
            /* Update existing tag */
            bridge->tags[i].intensity = initial_intensity;
            bridge->tags[i].last_update_time = 0.0f;
            return 0;
        }
    }

    /* Create new tag */
    snn_emotional_tag_t* tag = &bridge->tags[bridge->tag_count];
    tag->event_id = event_id;
    tag->intensity = initial_intensity;
    tag->creation_time = 0.0f;
    tag->last_update_time = 0.0f;
    tag->consolidated = false;
    tag->consolidation_strength = 0.0f;

    bridge->tag_count++;
    bridge->state.tagged_events_count++;

    return 0;
}

int snn_emotional_tagging_decay_tags(
    snn_emotional_tagging_bridge_t* bridge,
    float dt
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_decay_tags: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    float decay_factor = expf(-bridge->config.emotional_decay_rate * dt / 1000.0f);

    for (uint32_t i = 0; i < bridge->tag_count; i++) {
        if (!bridge->tags[i].consolidated) {
            bridge->tags[i].intensity *= decay_factor;
            bridge->tags[i].last_update_time += dt;
        }
    }

    return 0;
}

int snn_emotional_tagging_consolidate_memory(
    snn_emotional_tagging_bridge_t* bridge,
    uint32_t event_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_consolidate_memory: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    for (uint32_t i = 0; i < bridge->tag_count; i++) {
        if (bridge->tags[i].event_id == event_id) {
            bridge->tags[i].consolidated = true;
            bridge->tags[i].consolidation_strength = bridge->tags[i].intensity;
            NIMCP_LOGGING_DEBUG("Consolidated memory tag for event %u", event_id);
            return 0;
        }
    }

    NIMCP_LOGGING_WARN("Tag not found for event %u", event_id);
    return SNN_ERROR_OPERATION_FAILED;
}

int snn_emotional_tagging_enhance_ltp(
    snn_emotional_tagging_bridge_t* bridge,
    uint32_t event_id,
    float* enhancement_factor
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_enhance_ltp: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!enhancement_factor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_enhance_ltp: null enhancement_factor pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    for (uint32_t i = 0; i < bridge->tag_count; i++) {
        if (bridge->tags[i].event_id == event_id) {
            float factor = 1.0f + bridge->tags[i].intensity *
                          (bridge->config.ltp_enhancement_factor - 1.0f);
            *enhancement_factor = factor;
            return 0;
        }
    }

    *enhancement_factor = 1.0f;
    return 0;
}

int snn_emotional_tagging_bridge_get_state(
    const snn_emotional_tagging_bridge_t* bridge,
    snn_emotional_tagging_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_bridge_get_state: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_bridge_get_state: null state pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_emotional_tagging_get_current_intensity(const snn_emotional_tagging_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_get_current_intensity: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.current_tag_intensity;
}

uint32_t snn_emotional_tagging_get_tagged_count(const snn_emotional_tagging_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_get_tagged_count: null bridge pointer");
        return 0;
    }
    return bridge->state.tagged_events_count;
}

uint32_t snn_emotional_tagging_get_active_tags(const snn_emotional_tagging_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_get_active_tags: null bridge pointer");
        return 0;
    }
    return bridge->state.active_tags;
}

int snn_emotional_tagging_get_tag(
    const snn_emotional_tagging_bridge_t* bridge,
    uint32_t event_id,
    snn_emotional_tag_t* tag_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_get_tag: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!tag_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_get_tag: null tag_out pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    for (uint32_t i = 0; i < bridge->tag_count; i++) {
        if (bridge->tags[i].event_id == event_id) {
            *tag_out = bridge->tags[i];
            return 0;
        }
    }

    return SNN_ERROR_OPERATION_FAILED;
}

int snn_emotional_tagging_get_stats(
    const snn_emotional_tagging_bridge_t* bridge,
    uint32_t* tagged_count,
    float* avg_intensity,
    uint32_t* consolidated_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_get_stats: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (tagged_count) *tagged_count = bridge->state.tagged_events_count;
    if (avg_intensity) *avg_intensity = bridge->state.avg_tag_intensity;
    if (consolidated_count) *consolidated_count = bridge->state.consolidated_tags;
    return 0;
}

void snn_emotional_tagging_reset_stats(snn_emotional_tagging_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotional_tagging_reset_stats: null bridge pointer");
        return;
    }
    bridge->state.tagged_events_count = 0;
    bridge->state.avg_tag_intensity = 0.0f;
    bridge->state.active_tags = 0;
    bridge->state.consolidated_tags = 0;
    bridge->tag_count = 0;
}
