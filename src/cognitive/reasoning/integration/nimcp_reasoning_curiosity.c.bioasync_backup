/**
 * @file nimcp_reasoning_curiosity.c
 * @brief MODULE 2: Reasoning-Curiosity Integration Implementation
 */

#include "cognitive/reasoning/integration/nimcp_reasoning_curiosity.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

struct reasoning_curiosity {
    reasoning_curiosity_config_t config;
    event_bus_t event_bus;
    curiosity_engine_t curiosity;
    event_subscription_handle_t subscription_handle;
    reasoning_curiosity_stats_t stats;
};

static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000000 + (uint64_t)(tv.tv_usec);
}

reasoning_curiosity_config_t reasoning_curiosity_default_config(void) {
    reasoning_curiosity_config_t config = {
        .enable_proof_failure_exploration = true,
        .enable_novel_fact_exploration = true,
        .proof_failed_curiosity_boost = REASONING_CURIOSITY_PROOF_FAILED_BOOST,
        .novel_fact_curiosity_boost = REASONING_CURIOSITY_NOVEL_FACT_BOOST,
        .min_curiosity_threshold = 0.1f
    };
    return config;
}

bool reasoning_curiosity_validate_config(const reasoning_curiosity_config_t* config) {
    if (!config) return false;
    if (config->proof_failed_curiosity_boost < 0.0f || config->proof_failed_curiosity_boost > 1.0f) return false;
    if (config->novel_fact_curiosity_boost < 0.0f || config->novel_fact_curiosity_boost > 1.0f) return false;
    return true;
}

reasoning_curiosity_t* reasoning_curiosity_create(event_bus_t bus, curiosity_engine_t curiosity) {
    reasoning_curiosity_config_t config = reasoning_curiosity_default_config();
    return reasoning_curiosity_create_custom(bus, curiosity, &config);
}

reasoning_curiosity_t* reasoning_curiosity_create_custom(
    event_bus_t bus, curiosity_engine_t curiosity, const reasoning_curiosity_config_t* config
) {
    if (!bus || !curiosity) return NULL;
    
    reasoning_curiosity_t* integration = nimcp_calloc(1, sizeof(reasoning_curiosity_t));
    if (!integration) return NULL;
    
    reasoning_curiosity_config_t default_cfg = reasoning_curiosity_default_config();
    integration->config = config ? *config : default_cfg;
    integration->event_bus = bus;
    integration->curiosity = curiosity;
    
    integration->subscription_handle = event_bus_subscribe(
        bus, EVENT_ALL, reasoning_curiosity_callback, integration
    );
    
    if (integration->subscription_handle == INVALID_SUBSCRIPTION_HANDLE) {
        nimcp_free(integration);
        return NULL;
    }
    
    return integration;
}

void reasoning_curiosity_destroy(reasoning_curiosity_t* integration) {
    if (!integration) return;
    if (integration->subscription_handle != INVALID_SUBSCRIPTION_HANDLE) {
        event_bus_unsubscribe(integration->event_bus, integration->subscription_handle);
    }
    nimcp_free(integration);
}

void reasoning_curiosity_callback(const brain_event_t* event, void* context) {
    if (!event || !context) return;
    
    reasoning_curiosity_t* integration = (reasoning_curiosity_t*)context;
    uint64_t start = get_time_us();
    
    if (event->type < EVENT_LOGIC_GATE_EVALUATED || event->type > EVENT_NOVEL_FACT_DERIVED) {
        return;
    }
    
    integration->stats.total_events_processed++;
    
    float curiosity_boost = 0.0f;
    
    if (event->type == EVENT_PROOF_FAILED && integration->config.enable_proof_failure_exploration) {
        curiosity_boost = integration->config.proof_failed_curiosity_boost;
        integration->stats.proof_failure_triggers++;
        integration->stats.exploration_triggers++;
    } else if (event->type == EVENT_NOVEL_FACT_DERIVED && integration->config.enable_novel_fact_exploration) {
        curiosity_boost = integration->config.novel_fact_curiosity_boost;
        integration->stats.novel_fact_triggers++;
    }
    
    if (curiosity_boost >= integration->config.min_curiosity_threshold) {
        float total = integration->stats.avg_curiosity_boost * (integration->stats.exploration_triggers - 1);
        integration->stats.avg_curiosity_boost = (total + curiosity_boost) / integration->stats.exploration_triggers;
    }
    
    uint64_t elapsed = get_time_us() - start;
    uint64_t total_time = integration->stats.avg_callback_time_us * (integration->stats.total_events_processed - 1);
    integration->stats.avg_callback_time_us = (total_time + elapsed) / integration->stats.total_events_processed;
}

bool reasoning_curiosity_explore_unexplained_fact(reasoning_curiosity_t* integration, const char* fact) {
    if (!integration || !fact) return false;
    
    knowledge_gap_t gap = curiosity_detect_knowledge_gap(integration->curiosity, fact);
    if (gap.gap_size > 0.3f) {
        integration->stats.exploration_triggers++;
        return true;
    }
    return false;
}

bool reasoning_curiosity_get_stats(const reasoning_curiosity_t* integration, reasoning_curiosity_stats_t* stats) {
    if (!integration || !stats) return false;
    *stats = integration->stats;
    return true;
}

bool reasoning_curiosity_reset_stats(reasoning_curiosity_t* integration) {
    if (!integration) return false;
    memset(&integration->stats, 0, sizeof(reasoning_curiosity_stats_t));
    return true;
}

bool reasoning_curiosity_get_config(const reasoning_curiosity_t* integration, reasoning_curiosity_config_t* config) {
    if (!integration || !config) return false;
    *config = integration->config;
    return true;
}

bool reasoning_curiosity_set_config(reasoning_curiosity_t* integration, const reasoning_curiosity_config_t* config) {
    if (!integration || !config || !reasoning_curiosity_validate_config(config)) return false;
    integration->config = *config;
    return true;
}
