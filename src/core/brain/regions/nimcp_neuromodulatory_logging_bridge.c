/**
 * @file nimcp_neuromodulatory_logging_bridge.c
 * @brief Implementation of Unified Neuromodulatory-Logging Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/nimcp_neuromodulatory_logging_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(neuromodulatory_logging_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "NEUROMODULATORY_LOGGING_BRIDGE"


/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_PATTERNS_TRACKED    32

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct neuromod_logging_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    neuromod_logging_bridge_config_t config;

    /* Adapter connections */
    nimcp_lc_adapter_t lc_adapter;
    nimcp_vta_adapter_t vta_adapter;
    nimcp_raphe_adapter_t raphe_adapter;
    nimcp_habenula_adapter_t habenula_adapter;

    /* Logging system connection */
    nimcp_logging_context_t logging;
    bool connected;

    /* Current neuromodulator state (for context in logs) */
    float current_ne;
    float current_da;
    float current_ht;
    float current_hab;

    /* Log buffer */
    neuromod_log_entry_t* buffer;
    uint32_t buffer_head;
    uint32_t buffer_count;
    uint64_t sequence_counter;

    /* Pattern tracking */
    neuromod_log_pattern_t patterns[MAX_PATTERNS_TRACKED];
    uint32_t pattern_count;

    /* Timing */
    uint64_t last_flush_us;
    float time_since_flush_ms;

    /* Statistics */
    neuromod_logging_bridge_stats_t stats;
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

int neuromod_logging_bridge_default_config(neuromod_logging_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_logging_bridge_default_config: config is NULL");
        return -1;
    }

    config->enable_lc_logging = true;
    config->enable_vta_logging = true;
    config->enable_raphe_logging = true;
    config->enable_habenula_logging = true;
    config->enable_sync_logging = true;

    config->min_log_level = NEUROMOD_LOG_LEVEL_INFO;

    config->arousal_change_threshold = NEUROMOD_LOG_THRESHOLD_CHANGE;
    config->rpe_logging_threshold = NEUROMOD_LOG_THRESHOLD_CHANGE;
    config->mood_change_threshold = NEUROMOD_LOG_THRESHOLD_CHANGE;
    config->hab_change_threshold = NEUROMOD_LOG_THRESHOLD_CHANGE;

    config->log_buffer_size = NEUROMOD_LOG_MAX_EVENT_BUFFER;
    config->flush_interval_ms = NEUROMOD_LOG_DEFAULT_FLUSH_MS;

    config->enable_pattern_analysis = true;
    config->pattern_confidence_threshold = 0.7f;

    config->include_timestamps = true;
    config->include_neuromod_state = true;
    config->json_format = false;

    return 0;
}

neuromod_logging_bridge_t* neuromod_logging_bridge_create(const neuromod_logging_bridge_config_t* config) {
    neuromod_logging_bridge_t* bridge = nimcp_calloc(1, sizeof(neuromod_logging_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    bridge->magic = NEUROMOD_LOGGING_BRIDGE_MAGIC;

    if (config) {
        bridge->config = *config;
    } else {
        neuromod_logging_bridge_default_config(&bridge->config);
    }

    /* Allocate log buffer */
    bridge->buffer = nimcp_calloc(bridge->config.log_buffer_size, sizeof(neuromod_log_entry_t));
    if (!bridge->buffer) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromod_logging_bridge_create: bridge->buffer is NULL");
        return NULL;
    }

    bridge->last_flush_us = get_timestamp_us();
    NIMCP_LOGGING_INFO("Created %s bridge", "neuromodulatory_logging");
    return bridge;
}

void neuromod_logging_bridge_destroy(neuromod_logging_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromodulatory_logging");
    if (bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) return;

    /* Flush remaining logs */
    if (bridge->connected && bridge->buffer_count > 0) {
        neuromod_logging_flush(bridge);
    }

    if (bridge->connected) {
        neuromod_logging_bridge_disconnect(bridge);
    }

    nimcp_free(bridge->buffer);
    bridge->magic = 0;
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection
 * ============================================================================ */

int neuromod_logging_bridge_connect_logging(neuromod_logging_bridge_t* bridge, nimcp_logging_context_t logging) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_logging_bridge_connect_logging: bridge is NULL");
        return -1;
    }

    bridge->logging = logging;
    bridge->connected = true;

    return 0;
}

int neuromod_logging_bridge_disconnect(neuromod_logging_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_logging_bridge_disconnect: bridge is NULL");
        return -1;
    }

    bridge->logging = NULL;
    bridge->connected = false;

    return 0;
}

bool neuromod_logging_bridge_is_connected(const neuromod_logging_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        return false;
    }
    return bridge->connected;
}

/* ============================================================================
 * Adapter Registration
 * ============================================================================ */

int neuromod_logging_bridge_register_lc(neuromod_logging_bridge_t* bridge, nimcp_lc_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_logging_bridge_register_lc: bridge is NULL");
        return -1;
    }
    bridge->lc_adapter = adapter;
    return 0;
}

int neuromod_logging_bridge_register_vta(neuromod_logging_bridge_t* bridge, nimcp_vta_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_logging_bridge_register_vta: bridge is NULL");
        return -1;
    }
    bridge->vta_adapter = adapter;
    return 0;
}

int neuromod_logging_bridge_register_raphe(neuromod_logging_bridge_t* bridge, nimcp_raphe_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_logging_bridge_register_raphe: bridge is NULL");
        return -1;
    }
    bridge->raphe_adapter = adapter;
    return 0;
}

int neuromod_logging_bridge_register_habenula(neuromod_logging_bridge_t* bridge, nimcp_habenula_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_logging_bridge_register_habenula: bridge is NULL");
        return -1;
    }
    bridge->habenula_adapter = adapter;
    return 0;
}

/* ============================================================================
 * Internal Logging Helper
 * ============================================================================ */

/* Return values: 0 = added, 1 = filtered by level, -1 = error */
static int add_log_entry(neuromod_logging_bridge_t* bridge, const neuromod_log_entry_t* entry) {
    if (!bridge || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "add_log_entry: required parameter is NULL (bridge, entry)");
        return -1;
    }

    /* Check log level filter - return 1 to indicate filtered */
    if (entry->level < bridge->config.min_log_level) return 1;

    /* Add to buffer */
    if (bridge->buffer_count < bridge->config.log_buffer_size) {
        uint32_t index = (bridge->buffer_head + bridge->buffer_count) % bridge->config.log_buffer_size;
        bridge->buffer[index] = *entry;
        bridge->buffer[index].sequence_number = bridge->sequence_counter++;
        bridge->buffer[index].timestamp_us = get_timestamp_us();
        bridge->buffer_count++;
    } else {
        /* Buffer full - drop oldest or increment drop counter */
        bridge->stats.events_dropped++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "add_log_entry: validation failed");
        return -1;
    }

    return 0;
}

/* Helper to update level stats */
static void update_level_stats(neuromod_logging_bridge_t* bridge, neuromod_log_level_t level) {
    switch (level) {
        case NEUROMOD_LOG_LEVEL_TRACE: bridge->stats.trace_events++; break;
        case NEUROMOD_LOG_LEVEL_DEBUG: bridge->stats.debug_events++; break;
        case NEUROMOD_LOG_LEVEL_INFO: bridge->stats.info_events++; break;
        case NEUROMOD_LOG_LEVEL_WARN: bridge->stats.warn_events++; break;
        case NEUROMOD_LOG_LEVEL_ERROR: bridge->stats.error_events++; break;
        case NEUROMOD_LOG_LEVEL_CRITICAL: bridge->stats.critical_events++; break;
        default: break;
    }
}

/* ============================================================================
 * Logging Functions
 * ============================================================================ */

int neuromod_logging_log_lc_event(neuromod_logging_bridge_t* bridge, neuromod_log_event_t event,
                                  neuromod_log_level_t level, float ne_level, float value, const char* message) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "update_level_stats: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_lc_logging) return 0;

    neuromod_log_entry_t entry = {0};
    entry.category = NEUROMOD_LOG_CAT_LC;
    entry.level = level;
    entry.event_type = event;
    entry.ne_level = ne_level;
    entry.da_level = bridge->current_da;
    entry.ht_level = bridge->current_ht;
    entry.hab_level = bridge->current_hab;
    entry.primary_value = value;

    if (message) {
        strncpy(entry.message, message, NEUROMOD_LOG_MAX_MESSAGE_LEN - 1);
    }

    bridge->current_ne = ne_level;

    int result = add_log_entry(bridge, &entry);
    if (result == 0) {  /* Successfully added (not filtered) */
        bridge->stats.lc_events_logged++;
        bridge->stats.total_events_logged++;
        update_level_stats(bridge, level);
        bridge->stats.last_log_time_us = get_timestamp_us();
    }

    /* Return 0 for both success and filtered */
    return (result == -1) ? -1 : 0;
}

int neuromod_logging_log_vta_event(neuromod_logging_bridge_t* bridge, neuromod_log_event_t event,
                                   neuromod_log_level_t level, float da_level, float rpe, const char* message) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "update_level_stats: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_vta_logging) return 0;

    neuromod_log_entry_t entry = {0};
    entry.category = NEUROMOD_LOG_CAT_VTA;
    entry.level = level;
    entry.event_type = event;
    entry.ne_level = bridge->current_ne;
    entry.da_level = da_level;
    entry.ht_level = bridge->current_ht;
    entry.hab_level = bridge->current_hab;
    entry.primary_value = rpe;

    if (message) {
        strncpy(entry.message, message, NEUROMOD_LOG_MAX_MESSAGE_LEN - 1);
    }

    bridge->current_da = da_level;

    int result = add_log_entry(bridge, &entry);
    if (result == 0) {  /* Successfully added (not filtered) */
        bridge->stats.vta_events_logged++;
        bridge->stats.total_events_logged++;
        update_level_stats(bridge, level);
        bridge->stats.last_log_time_us = get_timestamp_us();
    }

    return (result == -1) ? -1 : 0;
}

int neuromod_logging_log_raphe_event(neuromod_logging_bridge_t* bridge, neuromod_log_event_t event,
                                     neuromod_log_level_t level, float ht_level, float mood, const char* message) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "update_level_stats: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_raphe_logging) return 0;

    neuromod_log_entry_t entry = {0};
    entry.category = NEUROMOD_LOG_CAT_RAPHE;
    entry.level = level;
    entry.event_type = event;
    entry.ne_level = bridge->current_ne;
    entry.da_level = bridge->current_da;
    entry.ht_level = ht_level;
    entry.hab_level = bridge->current_hab;
    entry.primary_value = mood;

    if (message) {
        strncpy(entry.message, message, NEUROMOD_LOG_MAX_MESSAGE_LEN - 1);
    }

    bridge->current_ht = ht_level;

    int result = add_log_entry(bridge, &entry);
    if (result == 0) {  /* Successfully added (not filtered) */
        bridge->stats.raphe_events_logged++;
        bridge->stats.total_events_logged++;
        update_level_stats(bridge, level);
        bridge->stats.last_log_time_us = get_timestamp_us();
    }

    return (result == -1) ? -1 : 0;
}

int neuromod_logging_log_habenula_event(neuromod_logging_bridge_t* bridge, neuromod_log_event_t event,
                                        neuromod_log_level_t level, float hab_level, float value, const char* message) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "update_level_stats: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_habenula_logging) return 0;

    neuromod_log_entry_t entry = {0};
    entry.category = NEUROMOD_LOG_CAT_HABENULA;
    entry.level = level;
    entry.event_type = event;
    entry.ne_level = bridge->current_ne;
    entry.da_level = bridge->current_da;
    entry.ht_level = bridge->current_ht;
    entry.hab_level = hab_level;
    entry.primary_value = value;

    if (message) {
        strncpy(entry.message, message, NEUROMOD_LOG_MAX_MESSAGE_LEN - 1);
    }

    bridge->current_hab = hab_level;

    int result = add_log_entry(bridge, &entry);
    if (result == 0) {  /* Successfully added (not filtered) */
        bridge->stats.habenula_events_logged++;
        bridge->stats.total_events_logged++;
        update_level_stats(bridge, level);
        bridge->stats.last_log_time_us = get_timestamp_us();
    }

    return (result == -1) ? -1 : 0;
}

int neuromod_logging_log_entry(neuromod_logging_bridge_t* bridge, const neuromod_log_entry_t* entry) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_logging_log_entry: required parameter is NULL (bridge, entry)");
        return -1;
    }
    return add_log_entry(bridge, entry);
}

/* ============================================================================
 * Flush and Buffer Management
 * ============================================================================ */

int neuromod_logging_flush(neuromod_logging_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_logging_flush: bridge is NULL");
        return -1;
    }

    /* In a real implementation, this would write to the logging system */
    /* For now, just clear the buffer */
    bridge->buffer_head = 0;
    bridge->buffer_count = 0;
    bridge->last_flush_us = get_timestamp_us();
    bridge->stats.flushes_performed++;

    return 0;
}

int neuromod_logging_update(neuromod_logging_bridge_t* bridge, float delta_ms) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_logging_update: bridge is NULL");
        return -1;
    }

    bridge->time_since_flush_ms += delta_ms;

    /* Auto-flush if interval elapsed */
    if (bridge->time_since_flush_ms >= bridge->config.flush_interval_ms) {
        neuromod_logging_flush(bridge);
        bridge->time_since_flush_ms = 0.0f;
    }

    return 0;
}

/* ============================================================================
 * Pattern Analysis
 * ============================================================================ */

int neuromod_logging_analyze_patterns(neuromod_logging_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_logging_analyze_patterns: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_pattern_analysis) return 0;

    /* Simple pattern detection - stub for demonstration */
    /* A real implementation would use time-series analysis */

    /* Count event frequencies in buffer */
    uint32_t event_counts[NEUROMOD_LOG_EVENT_COUNT] = {0};

    for (uint32_t i = 0; i < bridge->buffer_count; i++) {
        uint32_t index = (bridge->buffer_head + i) % bridge->config.log_buffer_size;
        neuromod_log_event_t evt = bridge->buffer[index].event_type;
        if (evt < NEUROMOD_LOG_EVENT_COUNT) {
            event_counts[evt]++;
        }
    }

    /* Simple pattern: frequent event detection */
    for (uint32_t evt = 0; evt < NEUROMOD_LOG_EVENT_COUNT && bridge->pattern_count < MAX_PATTERNS_TRACKED; evt++) {
        if (event_counts[evt] > bridge->buffer_count / 4) {  /* More than 25% of events */
            /* Check if pattern already tracked */
            bool found = false;
            for (uint32_t p = 0; p < bridge->pattern_count; p++) {
                if (bridge->patterns[p].pattern_type == (neuromod_log_event_t)evt) {
                    bridge->patterns[p].occurrence_count += event_counts[evt];
                    bridge->patterns[p].last_seen_us = get_timestamp_us();
                    found = true;
                    break;
                }
            }

            if (!found) {
                neuromod_log_pattern_t* pattern = &bridge->patterns[bridge->pattern_count];
                pattern->pattern_type = (neuromod_log_event_t)evt;
                pattern->confidence = (float)event_counts[evt] / (float)bridge->buffer_count;
                pattern->frequency = pattern->confidence;
                pattern->occurrence_count = event_counts[evt];
                snprintf(pattern->description, sizeof(pattern->description),
                         "Frequent %s events", neuromod_log_event_name((neuromod_log_event_t)evt));
                pattern->first_seen_us = get_timestamp_us();
                pattern->last_seen_us = pattern->first_seen_us;

                bridge->pattern_count++;
                bridge->stats.patterns_detected++;
            }
        }
    }

    return 0;
}

int neuromod_logging_get_pattern(const neuromod_logging_bridge_t* bridge, uint32_t index, neuromod_log_pattern_t* pattern) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_logging_get_pattern: required parameter is NULL (bridge, pattern)");
        return -1;
    }
    if (index >= bridge->pattern_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "neuromod_logging_get_pattern: capacity exceeded");
        return -1;
    }

    *pattern = bridge->patterns[index];
    return 0;
}

uint32_t neuromod_logging_get_pattern_count(const neuromod_logging_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) return 0;
    return bridge->pattern_count;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int neuromod_logging_bridge_get_stats(const neuromod_logging_bridge_t* bridge, neuromod_logging_bridge_stats_t* stats) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_logging_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int neuromod_logging_bridge_reset_stats(neuromod_logging_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_logging_bridge_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

static const char* category_names[] = {
    "LC", "VTA", "RAPHE", "HABENULA", "SYNC", "ANALYSIS"
};

static const char* level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"
};

static const char* event_names[] = {
    /* LC events */
    "AROUSAL_CHANGE", "PHASIC_BURST", "TONIC_SHIFT", "STRESS_ONSET", "STRESS_OFFSET", "GAIN_CHANGE",
    /* VTA events */
    "RPE_POSITIVE", "RPE_NEGATIVE", "MOTIVATION_CHANGE", "VALUE_UPDATE", "REWARD_RECEIVED", "REWARD_OMITTED",
    /* Raphe events */
    "MOOD_POSITIVE", "MOOD_NEGATIVE", "PATIENCE_CHANGE", "IMPULSE_BLOCKED", "SOCIAL_CONTEXT",
    /* Habenula events */
    "PUNISHMENT_DETECTED", "DISAPPOINTMENT", "AVOIDANCE_TRIGGERED", "NEGATIVE_RPE_HAB",
    /* Sync events */
    "NE_DA_SYNC", "DA_5HT_SYNC", "FULL_SYNC",
    /* Analysis events */
    "PATTERN_DETECTED", "ANOMALY_DETECTED", "TREND_IDENTIFIED"
};

const char* neuromod_log_category_name(neuromod_log_category_t category) {
    if (category >= NEUROMOD_LOG_CAT_COUNT) return "UNKNOWN";
    return category_names[category];
}

const char* neuromod_log_level_name(neuromod_log_level_t level) {
    if (level >= NEUROMOD_LOG_LEVEL_COUNT) return "UNKNOWN";
    return level_names[level];
}

const char* neuromod_log_event_name(neuromod_log_event_t event) {
    if (event >= NEUROMOD_LOG_EVENT_COUNT) return "UNKNOWN";
    return event_names[event];
}

void neuromod_logging_bridge_print_summary(const neuromod_logging_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_LOGGING_BRIDGE_MAGIC) {
        printf("Neuromodulatory Logging Bridge: NULL or invalid\n");
        return;
    }

    printf("Neuromodulatory Logging Bridge Summary:\n");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Adapters registered:\n");
    printf("    LC: %s, VTA: %s, Raphe: %s, Habenula: %s\n",
           bridge->lc_adapter ? "yes" : "no",
           bridge->vta_adapter ? "yes" : "no",
           bridge->raphe_adapter ? "yes" : "no",
           bridge->habenula_adapter ? "yes" : "no");
    printf("  Events logged:\n");
    printf("    LC: %u, VTA: %u, Raphe: %u, Habenula: %u, Sync: %u\n",
           bridge->stats.lc_events_logged,
           bridge->stats.vta_events_logged,
           bridge->stats.raphe_events_logged,
           bridge->stats.habenula_events_logged,
           bridge->stats.sync_events_logged);
    printf("  Log levels: Trace=%u, Debug=%u, Info=%u, Warn=%u, Error=%u, Critical=%u\n",
           bridge->stats.trace_events, bridge->stats.debug_events,
           bridge->stats.info_events, bridge->stats.warn_events,
           bridge->stats.error_events, bridge->stats.critical_events);
    printf("  Total: %u logged, %u dropped, %u flushes\n",
           bridge->stats.total_events_logged,
           bridge->stats.events_dropped,
           bridge->stats.flushes_performed);
    printf("  Patterns detected: %u, Anomalies: %u\n",
           bridge->stats.patterns_detected,
           bridge->stats.anomalies_detected);
    printf("  Buffer: %u/%u entries\n",
           bridge->buffer_count, bridge->config.log_buffer_size);
}
