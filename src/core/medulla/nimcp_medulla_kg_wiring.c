/**
 * @file nimcp_medulla_kg_wiring.c
 * @brief Knowledge Graph wiring for Medulla module - Implementation
 *
 * WHAT: Implements KG wiring between medulla and brain knowledge graph
 * WHY:  Enable semantic queries and state tracking for medulla
 * HOW:  Register nodes/edges, log state changes, maintain history
 *
 * @author NIMCP Development Team
 * @date 2026-02-01
 * @version 1.0.0
 */

#include "core/medulla/nimcp_medulla_kg_wiring.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/time/nimcp_time.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

//=============================================================================
// Module Constants
//=============================================================================

#define MEDULLA_KG_MODULE_NAME "medulla_kg_wiring"
#define MEDULLA_KG_ROOT_NAME "medulla_oblongata"
#define MEDULLA_KG_AROUSAL_NAME "medulla_arousal"
#define MEDULLA_KG_PROTECTION_NAME "medulla_protection"
#define MEDULLA_KG_CIRCADIAN_NAME "medulla_circadian"
#define MEDULLA_KG_EMERGENCY_NAME "medulla_emergency"
#define MEDULLA_KG_MAX_HISTORY 1000
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(medulla_kg_wiring)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief History entry for arousal tracking
 */
typedef struct {
    uint64_t timestamp_us;
    float arousal_value;
    arousal_level_t arousal_level;
} arousal_history_entry_t;

/**
 * @brief History entry for protection tracking
 */
typedef struct {
    uint64_t timestamp_us;
    protection_level_t level;
} protection_history_entry_t;

/**
 * @brief History entry for circadian tracking
 */
typedef struct {
    uint64_t timestamp_us;
    circadian_phase_t phase;
    float circadian_time;
} circadian_history_entry_t;

/**
 * @brief History entry for emergency events
 */
typedef struct {
    uint64_t timestamp_us;
    char reason[NIMCP_ERROR_BUFFER_SIZE];
} emergency_history_entry_t;

/**
 * @brief Internal KG wiring structure
 */
struct medulla_kg_wiring {
    /* Configuration */
    medulla_kg_config_t config;

    /* References */
    medulla_t medulla;
    brain_kg_t* kg;

    /* KG node IDs */
    brain_kg_node_id_t root_id;
    brain_kg_node_id_t arousal_id;
    brain_kg_node_id_t protection_id;
    brain_kg_node_id_t circadian_id;
    brain_kg_node_id_t emergency_id;

    /* History buffers */
    arousal_history_entry_t* arousal_history;
    size_t arousal_history_count;
    size_t arousal_history_head;

    protection_history_entry_t* protection_history;
    size_t protection_history_count;
    size_t protection_history_head;

    circadian_history_entry_t* circadian_history;
    size_t circadian_history_count;
    size_t circadian_history_head;

    emergency_history_entry_t* emergency_history;
    size_t emergency_history_count;
    size_t emergency_history_head;

    /* State tracking */
    float last_logged_arousal;
    protection_level_t last_logged_protection;
    circadian_phase_t last_logged_phase;

    /* Statistics */
    uint64_t total_state_logs;
    uint64_t total_arousal_logs;
    uint64_t total_protection_logs;
    uint64_t total_emergency_logs;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_timestamp_us(void) {
    return nimcp_time_get_us();
}

/**
 * @brief Create a medulla KG node with description
 */
static brain_kg_node_id_t create_medulla_node(
    brain_kg_t* kg,
    const char* name,
    brain_kg_node_type_t type,
    const char* description
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;

    brain_kg_node_id_t id = brain_kg_add_node(kg, name, type, description);
    if (id != BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_DEBUG(MEDULLA_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

/**
 * @brief Create an edge between medulla nodes
 */
static brain_kg_edge_id_t create_medulla_edge(
    brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    brain_kg_edge_type_t type,
    const char* description,
    float weight
) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    if (from == BRAIN_KG_INVALID_NODE || to == BRAIN_KG_INVALID_NODE) {
        return BRAIN_KG_INVALID_NODE;
    }

    return brain_kg_add_edge(kg, from, to, type, description, weight);
}

//=============================================================================
// Configuration API
//=============================================================================

medulla_kg_config_t medulla_kg_default_config(void) {
    medulla_kg_config_t config;
    memset(&config, 0, sizeof(config));

    config.enable_state_logging = true;
    config.enable_arousal_history = true;
    config.enable_protection_history = true;
    config.enable_circadian_history = true;
    config.history_depth = 100;
    config.log_threshold = 0.01f;

    return config;
}

//=============================================================================
// Lifecycle API
//=============================================================================

medulla_kg_wiring_t medulla_kg_create(
    const medulla_kg_config_t* config,
    medulla_t medulla,
    brain_kg_t* kg
) {
    if (!medulla || !kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "medulla_kg_create: medulla or kg is NULL");
        return NULL;
    }

    /* Allocate structure */
    medulla_kg_wiring_t wiring = nimcp_calloc(1, sizeof(struct medulla_kg_wiring));
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "medulla_kg_create: failed to allocate wiring structure");
        return NULL;
    }

    /* Copy config */
    if (config) {
        wiring->config = *config;
    } else {
        wiring->config = medulla_kg_default_config();
    }

    /* Validate history depth */
    if (wiring->config.history_depth > MEDULLA_KG_MAX_HISTORY) {
        wiring->config.history_depth = MEDULLA_KG_MAX_HISTORY;
    }
    if (wiring->config.history_depth == 0) {
        wiring->config.history_depth = 100;
    }

    /* Store references */
    wiring->medulla = medulla;
    wiring->kg = kg;

    /* Allocate history buffers */
    if (wiring->config.enable_arousal_history) {
        wiring->arousal_history = nimcp_calloc(
            wiring->config.history_depth,
            sizeof(arousal_history_entry_t)
        );
        if (!wiring->arousal_history) {
            NIMCP_LOG_WARN(MEDULLA_KG_MODULE_NAME,
                "Failed to allocate arousal history buffer");
        }
    }

    if (wiring->config.enable_protection_history) {
        wiring->protection_history = nimcp_calloc(
            wiring->config.history_depth,
            sizeof(protection_history_entry_t)
        );
        if (!wiring->protection_history) {
            NIMCP_LOG_WARN(MEDULLA_KG_MODULE_NAME,
                "Failed to allocate protection history buffer");
        }
    }

    if (wiring->config.enable_circadian_history) {
        wiring->circadian_history = nimcp_calloc(
            wiring->config.history_depth,
            sizeof(circadian_history_entry_t)
        );
        if (!wiring->circadian_history) {
            NIMCP_LOG_WARN(MEDULLA_KG_MODULE_NAME,
                "Failed to allocate circadian history buffer");
        }
    }

    /* Always allocate emergency history */
    wiring->emergency_history = nimcp_calloc(
        wiring->config.history_depth,
        sizeof(emergency_history_entry_t)
    );

    /* Create root node in KG */
    wiring->root_id = create_medulla_node(
        kg, MEDULLA_KG_ROOT_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Medulla Oblongata - Vital functions, arousal, protection, circadian rhythm"
    );

    if (wiring->root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(MEDULLA_KG_MODULE_NAME, "Failed to create root node");
        medulla_kg_destroy(wiring);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "medulla_kg_create: validation failed");
        return NULL;
    }

    /* Create arousal subsystem node */
    wiring->arousal_id = create_medulla_node(
        kg, MEDULLA_KG_AROUSAL_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Arousal state management - consciousness level regulation"
    );

    if (wiring->arousal_id != BRAIN_KG_INVALID_NODE) {
        create_medulla_edge(kg, wiring->root_id, wiring->arousal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "manages arousal", 1.0f);
    }

    /* Create protection subsystem node */
    wiring->protection_id = create_medulla_node(
        kg, MEDULLA_KG_PROTECTION_NAME,
        BRAIN_KG_NODE_SECURITY,
        "Protection level management - emergency response escalation"
    );

    if (wiring->protection_id != BRAIN_KG_INVALID_NODE) {
        create_medulla_edge(kg, wiring->root_id, wiring->protection_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "manages protection", 1.0f);
    }

    /* Create circadian subsystem node */
    wiring->circadian_id = create_medulla_node(
        kg, MEDULLA_KG_CIRCADIAN_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Circadian rhythm coordination - 24-hour cycle management"
    );

    if (wiring->circadian_id != BRAIN_KG_INVALID_NODE) {
        create_medulla_edge(kg, wiring->root_id, wiring->circadian_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "manages circadian", 1.0f);
    }

    /* Create emergency events node */
    wiring->emergency_id = create_medulla_node(
        kg, MEDULLA_KG_EMERGENCY_NAME,
        BRAIN_KG_NODE_SECURITY,
        "Emergency shutdown events - critical event log"
    );

    if (wiring->emergency_id != BRAIN_KG_INVALID_NODE) {
        create_medulla_edge(kg, wiring->root_id, wiring->emergency_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "logs emergencies", 1.0f);
    }

    /* Initialize state tracking */
    wiring->last_logged_arousal = -1.0f;  /* Invalid to force first log */
    wiring->last_logged_protection = PROTECTION_LEVEL_NORMAL;
    wiring->last_logged_phase = CIRCADIAN_PHASE_MORNING;

    NIMCP_LOG_INFO(MEDULLA_KG_MODULE_NAME,
        "Medulla KG wiring created (root_id=%u)", wiring->root_id);

    return wiring;
}

void medulla_kg_destroy(medulla_kg_wiring_t wiring) {
    if (!wiring) {
        return;
    }

    /* Free history buffers */
    if (wiring->arousal_history) {
        nimcp_free(wiring->arousal_history);
    }
    if (wiring->protection_history) {
        nimcp_free(wiring->protection_history);
    }
    if (wiring->circadian_history) {
        nimcp_free(wiring->circadian_history);
    }
    if (wiring->emergency_history) {
        nimcp_free(wiring->emergency_history);
    }

    /* Note: We don't remove nodes from KG here - they persist */

    nimcp_free(wiring);

    NIMCP_LOG_INFO(MEDULLA_KG_MODULE_NAME, "Medulla KG wiring destroyed");
}

//=============================================================================
// State Logging API
//=============================================================================

int medulla_kg_log_state(medulla_kg_wiring_t wiring) {
    if (!wiring || !wiring->medulla || !wiring->kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "medulla_kg_log_state: invalid wiring");
        return -1;
    }

    if (!wiring->config.enable_state_logging) {
        return 0;
    }

    medulla_kg_wiring_heartbeat("medulla_kg_log_state", 0.5f);

    /* Get current medulla stats */
    medulla_stats_t stats;
    if (medulla_get_stats(wiring->medulla, &stats) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "medulla_kg_log_state: validation failed");
        return -1;
    }

    char value_str[NIMCP_ID_BUFFER_SIZE];

    /* Update root node metadata */
    if (wiring->root_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%.3f", stats.current_arousal);
        brain_kg_add_metadata(wiring->kg, wiring->root_id, "current_arousal", value_str);

        snprintf(value_str, sizeof(value_str), "%s",
            medulla_arousal_level_to_string(stats.arousal_level));
        brain_kg_add_metadata(wiring->kg, wiring->root_id, "arousal_level", value_str);

        snprintf(value_str, sizeof(value_str), "%s",
            medulla_protection_level_to_string(stats.protection_level));
        brain_kg_add_metadata(wiring->kg, wiring->root_id, "protection_level", value_str);

        snprintf(value_str, sizeof(value_str), "%s",
            medulla_circadian_phase_to_string(stats.circadian_phase));
        brain_kg_add_metadata(wiring->kg, wiring->root_id, "circadian_phase", value_str);

        snprintf(value_str, sizeof(value_str), "%s",
            medulla_state_to_string(stats.state));
        brain_kg_add_metadata(wiring->kg, wiring->root_id, "state", value_str);

        snprintf(value_str, sizeof(value_str), "%" PRIu64, stats.total_updates);
        brain_kg_add_metadata(wiring->kg, wiring->root_id, "total_updates", value_str);

        snprintf(value_str, sizeof(value_str), "%" PRIu64, stats.uptime_ms);
        brain_kg_add_metadata(wiring->kg, wiring->root_id, "uptime_ms", value_str);
    }

    /* Update arousal node */
    if (wiring->arousal_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%.3f", stats.current_arousal);
        brain_kg_add_metadata(wiring->kg, wiring->arousal_id, "level", value_str);

        snprintf(value_str, sizeof(value_str), "%.3f", stats.avg_arousal);
        brain_kg_add_metadata(wiring->kg, wiring->arousal_id, "average", value_str);

        snprintf(value_str, sizeof(value_str), "%" PRIu64, stats.arousal_updates);
        brain_kg_add_metadata(wiring->kg, wiring->arousal_id, "updates", value_str);
    }

    /* Update protection node */
    if (wiring->protection_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%s",
            medulla_protection_level_to_string(stats.protection_level));
        brain_kg_add_metadata(wiring->kg, wiring->protection_id, "level", value_str);

        snprintf(value_str, sizeof(value_str), "%u", stats.protection_activations);
        brain_kg_add_metadata(wiring->kg, wiring->protection_id, "activations", value_str);

        snprintf(value_str, sizeof(value_str), "%u", stats.emergency_shutdowns);
        brain_kg_add_metadata(wiring->kg, wiring->protection_id, "emergencies", value_str);
    }

    /* Update circadian node */
    if (wiring->circadian_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%s",
            medulla_circadian_phase_to_string(stats.circadian_phase));
        brain_kg_add_metadata(wiring->kg, wiring->circadian_id, "phase", value_str);

        snprintf(value_str, sizeof(value_str), "%.2f", stats.circadian_time_hours);
        brain_kg_add_metadata(wiring->kg, wiring->circadian_id, "time_hours", value_str);

        snprintf(value_str, sizeof(value_str), "%u", stats.circadian_cycles);
        brain_kg_add_metadata(wiring->kg, wiring->circadian_id, "cycles", value_str);
    }

    wiring->total_state_logs++;

    return 0;
}

int medulla_kg_log_arousal_change(
    medulla_kg_wiring_t wiring,
    float old_arousal,
    float new_arousal
) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "medulla_kg_log_arousal_change: invalid wiring");
        return -1;
    }

    if (!wiring->config.enable_arousal_history) {
        return 0;
    }

    /* Check threshold */
    float delta = new_arousal - old_arousal;
    if (delta < 0) delta = -delta;
    if (delta < wiring->config.log_threshold) {
        return 0;  /* Change below threshold */
    }

    /* Add to history buffer (circular) */
    if (wiring->arousal_history) {
        size_t idx = wiring->arousal_history_head;
        wiring->arousal_history[idx].timestamp_us = get_timestamp_us();
        wiring->arousal_history[idx].arousal_value = new_arousal;

        /* Classify arousal level */
        if (new_arousal < 0.1f) {
            wiring->arousal_history[idx].arousal_level = AROUSAL_LEVEL_COMA;
        } else if (new_arousal < 0.3f) {
            wiring->arousal_history[idx].arousal_level = AROUSAL_LEVEL_DEEP_SLEEP;
        } else if (new_arousal < 0.5f) {
            wiring->arousal_history[idx].arousal_level = AROUSAL_LEVEL_DROWSY;
        } else if (new_arousal < 0.7f) {
            wiring->arousal_history[idx].arousal_level = AROUSAL_LEVEL_AWAKE;
        } else if (new_arousal < 0.9f) {
            wiring->arousal_history[idx].arousal_level = AROUSAL_LEVEL_ALERT;
        } else {
            wiring->arousal_history[idx].arousal_level = AROUSAL_LEVEL_HYPERAROUSAL;
        }

        wiring->arousal_history_head =
            (wiring->arousal_history_head + 1) % wiring->config.history_depth;
        if (wiring->arousal_history_count < wiring->config.history_depth) {
            wiring->arousal_history_count++;
        }
    }

    wiring->last_logged_arousal = new_arousal;
    wiring->total_arousal_logs++;

    NIMCP_LOG_DEBUG(MEDULLA_KG_MODULE_NAME,
        "Logged arousal change: %.3f -> %.3f", old_arousal, new_arousal);

    return 0;
}

int medulla_kg_log_protection_change(
    medulla_kg_wiring_t wiring,
    protection_level_t old_level,
    protection_level_t new_level
) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "medulla_kg_log_protection_change: invalid wiring");
        return -1;
    }

    if (!wiring->config.enable_protection_history) {
        return 0;
    }

    if (old_level == new_level) {
        return 0;  /* No change */
    }

    /* Add to history buffer (circular) */
    if (wiring->protection_history) {
        size_t idx = wiring->protection_history_head;
        wiring->protection_history[idx].timestamp_us = get_timestamp_us();
        wiring->protection_history[idx].level = new_level;

        wiring->protection_history_head =
            (wiring->protection_history_head + 1) % wiring->config.history_depth;
        if (wiring->protection_history_count < wiring->config.history_depth) {
            wiring->protection_history_count++;
        }
    }

    /* Update KG node */
    if (wiring->protection_id != BRAIN_KG_INVALID_NODE) {
        char value_str[NIMCP_ID_BUFFER_SIZE];
        snprintf(value_str, sizeof(value_str), "%s -> %s",
            medulla_protection_level_to_string(old_level),
            medulla_protection_level_to_string(new_level));
        brain_kg_add_metadata(wiring->kg, wiring->protection_id,
            "last_transition", value_str);
    }

    wiring->last_logged_protection = new_level;
    wiring->total_protection_logs++;

    NIMCP_LOG_INFO(MEDULLA_KG_MODULE_NAME,
        "Logged protection change: %s -> %s",
        medulla_protection_level_to_string(old_level),
        medulla_protection_level_to_string(new_level));

    return 0;
}

int medulla_kg_log_emergency(
    medulla_kg_wiring_t wiring,
    const char* reason
) {
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "medulla_kg_log_emergency: invalid wiring");
        return -1;
    }

    medulla_kg_wiring_heartbeat("medulla_kg_log_emergency", 1.0f);

    /* Add to history buffer (circular) */
    if (wiring->emergency_history) {
        size_t idx = wiring->emergency_history_head;
        wiring->emergency_history[idx].timestamp_us = get_timestamp_us();
        if (reason) {
            strncpy(wiring->emergency_history[idx].reason, reason,
                sizeof(wiring->emergency_history[idx].reason) - 1);
            wiring->emergency_history[idx].reason[
                sizeof(wiring->emergency_history[idx].reason) - 1] = '\0';
        } else {
            strncpy(wiring->emergency_history[idx].reason, "unknown",
                sizeof(wiring->emergency_history[idx].reason) - 1);
            wiring->emergency_history[idx].reason[
                sizeof(wiring->emergency_history[idx].reason) - 1] = '\0';
        }

        wiring->emergency_history_head =
            (wiring->emergency_history_head + 1) % wiring->config.history_depth;
        if (wiring->emergency_history_count < wiring->config.history_depth) {
            wiring->emergency_history_count++;
        }
    }

    /* Update KG node */
    if (wiring->emergency_id != BRAIN_KG_INVALID_NODE) {
        char value_str[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(value_str, sizeof(value_str), "%s", reason ? reason : "unknown");
        brain_kg_add_metadata(wiring->kg, wiring->emergency_id,
            "last_reason", value_str);

        snprintf(value_str, sizeof(value_str), "%" PRIu64,
            wiring->total_emergency_logs + 1);
        brain_kg_add_metadata(wiring->kg, wiring->emergency_id,
            "total_emergencies", value_str);
    }

    wiring->total_emergency_logs++;

    NIMCP_LOG_ERROR(MEDULLA_KG_MODULE_NAME,
        "Logged emergency: %s", reason ? reason : "unknown");

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

int medulla_kg_get_arousal_history(
    medulla_kg_wiring_t wiring,
    float* history,
    size_t max_entries,
    size_t* count
) {
    if (!wiring || !history || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "medulla_kg_get_arousal_history: invalid parameters");
        return -1;
    }

    *count = 0;

    if (!wiring->arousal_history || wiring->arousal_history_count == 0) {
        return 0;  /* No history available */
    }

    size_t to_copy = wiring->arousal_history_count;
    if (to_copy > max_entries) {
        to_copy = max_entries;
    }

    /* Copy from circular buffer in chronological order */
    size_t start_idx;
    if (wiring->arousal_history_count < wiring->config.history_depth) {
        start_idx = 0;
    } else {
        start_idx = wiring->arousal_history_head;
    }

    for (size_t i = 0; i < to_copy; i++) {
        size_t idx = (start_idx + i) % wiring->config.history_depth;
        history[i] = wiring->arousal_history[idx].arousal_value;
    }

    *count = to_copy;
    return 0;
}
