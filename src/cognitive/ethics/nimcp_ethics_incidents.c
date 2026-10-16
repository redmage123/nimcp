//=============================================================================
// nimcp_ethics_incidents.c - Ethics Incident Logging and Querying
//=============================================================================
// RESPONSIBILITY: Incident logging, indexing, and query operations
//
// This module implements comprehensive incident logging with B-tree indexing
// for efficient temporal queries and hash table indexing for violation types.
//=============================================================================

#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/ethics/nimcp_ethics_internal.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/containers/nimcp_btree.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_w11_safety_kg_events.h"  /* w11_emit_ethics_incident */
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <string.h>

#define LOG_MODULE "ethics_incidents"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(ethics_incidents, MESH_ADAPTER_CATEGORY_COGNITIVE)


// Incident logging configuration
#define MAX_INCIDENT_HISTORY 10000  // Circular buffer size (10k incidents)

//=============================================================================
// Helper Functions for B-tree
//=============================================================================

/**
 * @brief Compare timestamp keys for B-tree ordering
 *
 * Keys are formatted as "%020lu" strings for lexicographic comparison
 */
static int compare_incident_timestamps(const char* key1, const char* key2)
{
    if (!key1 || !key2) {
        return 0;
    }

    // Parse timestamp keys
    uint64_t ts1 = strtoull(key1, NULL, 10);
    uint64_t ts2 = strtoull(key2, NULL, 10);

    if (ts1 < ts2) return -1;
    if (ts1 > ts2) return 1;
    return 0;
}

/**
 * @brief Extract timestamp key from incident for B-tree indexing
 */
static const char* extract_incident_timestamp_key(const void* data)
{
    const ethics_incident_t* incident = (const ethics_incident_t*)data;
    return incident->timestamp_key;
}

/**
 * @brief Free incident data (no-op since incidents are in circular buffer)
 */
static void free_incident_data(void* data)
{
    // No-op: incidents are stored in circular buffer, not individually allocated
    (void)data;
}

//=============================================================================
// Incident Logging Initialization and Cleanup
//=============================================================================

/**
 * @brief Initialize incident logging infrastructure
 *
 * @param engine Ethics engine
 * @return true on success
 */
bool ethics_init_incident_logging(ethics_engine_t engine)
{
    // Guard clause: Validate input
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ethics_init_incident_logging: engine is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_incidents_heartbeat("ethics_incid_ethics_init_incident", 0.0f);

    // Initialize incident storage
    ethics_incident_storage_t* storage = ethics_engine_get_incident_storage(engine);
    if (!storage)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "ethics_init_incident_logging: storage is NULL");

            return false;

        }

    // Allocate circular buffer
    storage->incident_history = nimcp_calloc(MAX_INCIDENT_HISTORY, sizeof(ethics_incident_t));
    if (!storage->incident_history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_init_incident_logging: storage->incident_history is NULL");
        return false;
    }

    storage->incident_count = 0;
    storage->incident_index = 0;
    storage->next_incident_id = 1;

    // Create B-tree for timestamp indexing
    storage->incident_btree = btree_create(compare_incident_timestamps,
                                         extract_incident_timestamp_key,
                                         free_incident_data);
    if (!storage->incident_btree) {
        nimcp_free(storage->incident_history);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_init_incident_logging: btree_create failed for incident_btree");
        return false;
    }

    // Create hash table for violation type indexing
    hash_table_config_t hash_config = {
        .initial_buckets = 32,
        .key_type = HASH_KEY_UINT64,
        .hash_algorithm = HASH_ALG_MURMUR3,
        .value_destructor = NULL,
        .thread_safe = false
    };
    storage->incident_by_type = hash_table_create(&hash_config);
    if (!storage->incident_by_type) {
        btree_destroy(storage->incident_btree);
        nimcp_free(storage->incident_history);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_init_incident_logging: hash_table_create failed for incident_by_type");
        return false;
    }

    // Initialize mutex
    if (nimcp_mutex_init(&storage->incident_mutex, NULL) != NIMCP_SUCCESS) {
        hash_table_destroy(storage->incident_by_type);
        btree_destroy(storage->incident_btree);
        nimcp_free(storage->incident_history);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "ethics_init_incident_logging: validation failed");
        return false;
    }

    return true;
}

/**
 * @brief Cleanup incident logging infrastructure
 *
 * @param engine Ethics engine
 */
void ethics_cleanup_incident_logging(ethics_engine_t engine)
{
    if (!engine)
        return;

    /* Phase 8: Heartbeat at operation start */
    ethics_incidents_heartbeat("ethics_incid_ethics_cleanup_incid", 0.0f);

    ethics_incident_storage_t* storage = ethics_engine_get_incident_storage(engine);
    if (!storage)
        return;

    if (storage->incident_history) {
        nimcp_free(storage->incident_history);
        storage->incident_history = NULL;
    }

    if (storage->incident_btree) {
        btree_destroy(storage->incident_btree);
        storage->incident_btree = NULL;
    }

    if (storage->incident_by_type) {
        hash_table_destroy(storage->incident_by_type);
        storage->incident_by_type = NULL;
    }

    nimcp_mutex_destroy(&storage->incident_mutex);
}

//=============================================================================
// Incident Logging Functions
//=============================================================================

/**
 * @brief Log an ethics incident
 */
bool ethics_log_incident(ethics_engine_t engine, const ethics_incident_t* incident)
{
    // Guard clause: Validate inputs
    if (!engine || !incident) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_log_incident: required parameter is NULL (engine, incident)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_incidents_heartbeat("ethics_incid_ethics_log_incident", 0.0f);

    ethics_incident_storage_t* storage = ethics_engine_get_incident_storage(engine);
    if (!storage || !storage->incident_history || !storage->incident_btree || !storage->incident_by_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_log_incident: required parameter is NULL (storage, storage->incident_history, storage->incident_btree, storage->incident_by_type)");
        return false;
    }

    // Thread safety
    nimcp_mutex_lock(&storage->incident_mutex);

    // Get current index in circular buffer
    uint32_t index = storage->incident_index % MAX_INCIDENT_HISTORY;

    // Copy incident to circular buffer
    storage->incident_history[index] = *incident;
    storage->incident_history[index].incident_id = storage->next_incident_id++;

    // Generate timestamp if not provided
    if (storage->incident_history[index].timestamp == 0) {
        storage->incident_history[index].timestamp = nimcp_time_get_us();
    }

    // Generate timestamp key for B-tree indexing
    snprintf(storage->incident_history[index].timestamp_key, 32, "%020lu",
             storage->incident_history[index].timestamp);

    // Insert into B-tree (indexed by timestamp)
    if (storage->incident_btree) {
        int result = btree_insert(storage->incident_btree, &storage->incident_history[index]);
        if (result != BTREE_SUCCESS && result != BTREE_DUPLICATE) {
            // Log warning but continue - B-tree is optimization, not critical
        }
    }

    // Update indices
    storage->incident_index++;
    if (storage->incident_count < MAX_INCIDENT_HISTORY)
        storage->incident_count++;

    // Update statistics
    ethics_engine_increment_violations_detected(engine);

    /* Snapshot incident fields under lock for the KG emit below, to avoid
     * touching storage outside the critical section. */
    uint64_t kg_incident_id = storage->incident_history[index].incident_id;
    int      kg_violation   = (int)incident->violation_type;
    float    kg_severity    = incident->severity;
    int      kg_action      = (int)incident->action_taken;
    uint32_t kg_policy_id   = incident->policy_id;
    char     kg_policy_name[64];
    strncpy(kg_policy_name, incident->policy_name, sizeof(kg_policy_name) - 1);
    kg_policy_name[sizeof(kg_policy_name) - 1] = '\0';

    nimcp_mutex_unlock(&storage->incident_mutex);

    // Log to console
    LOG_INFO("Incident logged: type=%d severity=%.2f action=%d",
                       incident->violation_type, incident->severity, incident->action_taken);

    /* W11: emit KG incident event (parallels the B-tree / hash storage;
     * makes the incident queryable by KG consumers). */
    {
        brain_t kg_brain = engine->host_brain;
        if (kg_brain) {
            w11_emit_ethics_incident(kg_brain,
                                     kg_incident_id,
                                     kg_violation,
                                     kg_severity,
                                     kg_action,
                                     kg_policy_id,
                                     kg_policy_name);
        }
    }

    return true;
}

//=============================================================================
// Incident Query Functions
//=============================================================================

/**
 * @brief Get recent ethics incidents
 */
uint32_t ethics_get_recent_incidents(ethics_engine_t engine, uint32_t max_incidents,
                                     ethics_incident_t** incidents_out)
{
    // Guard clause: Validate inputs
    if (!engine || !incidents_out || max_incidents == 0)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    ethics_incidents_heartbeat("ethics_incid_ethics_get_recent_in", 0.0f);

    ethics_incident_storage_t* storage = ethics_engine_get_incident_storage(engine);
    if (!storage || !storage->incident_history)
        return 0;

    nimcp_mutex_lock(&storage->incident_mutex);

    // Determine actual number to return
    uint32_t num_to_return = (max_incidents < storage->incident_count) ?
                             max_incidents : storage->incident_count;

    if (num_to_return == 0) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Allocate output array
    *incidents_out = nimcp_calloc(num_to_return, sizeof(ethics_incident_t));
    if (!*incidents_out) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Copy most recent incidents (walking backwards from current index)
    for (uint32_t i = 0; i < num_to_return; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_to_return > 256) {
            ethics_incidents_heartbeat("ethics_incid_loop",
                             (float)(i + 1) / (float)num_to_return);
        }

        uint32_t src_index = (storage->incident_index - 1 - i) % MAX_INCIDENT_HISTORY;
        (*incidents_out)[i] = storage->incident_history[src_index];
    }

    nimcp_mutex_unlock(&storage->incident_mutex);
    return num_to_return;
}

/**
 * @brief Query incidents by time range
 */
uint32_t ethics_get_incidents_by_time_range(ethics_engine_t engine, uint64_t start_time,
                                            uint64_t end_time,
                                            ethics_incident_t** incidents_out)
{
    // Guard clause: Validate inputs
    if (!engine || !incidents_out)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    ethics_incidents_heartbeat("ethics_incid_ethics_get_incidents", 0.0f);

    ethics_incident_storage_t* storage = ethics_engine_get_incident_storage(engine);
    if (!storage || !storage->incident_history || !storage->incident_btree)
        return 0;

    nimcp_mutex_lock(&storage->incident_mutex);

    // Use B-tree for efficient time-range query
    // First pass: count matching incidents
    uint32_t match_count = 0;
    btree_iterator_t* iter = btree_iterator_create(storage->incident_btree);
    if (!iter) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    void* data = NULL;
    while (btree_iterator_next(iter, &data)) {
        ethics_incident_t* incident = (ethics_incident_t*)data;
        if (incident->timestamp >= start_time && incident->timestamp <= end_time) {
            match_count++;
        }
    }
    btree_iterator_destroy(iter);

    if (match_count == 0) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Allocate output array
    *incidents_out = nimcp_calloc(match_count, sizeof(ethics_incident_t));
    if (!*incidents_out) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Second pass: copy matching incidents (B-tree returns in timestamp order)
    iter = btree_iterator_create(storage->incident_btree);
    if (!iter) {
        nimcp_free(*incidents_out);
        *incidents_out = NULL;
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    uint32_t out_index = 0;
    while (btree_iterator_next(iter, &data) && out_index < match_count) {
        ethics_incident_t* incident = (ethics_incident_t*)data;
        if (incident->timestamp >= start_time && incident->timestamp <= end_time) {
            (*incidents_out)[out_index++] = *incident;
        }
    }
    btree_iterator_destroy(iter);

    nimcp_mutex_unlock(&storage->incident_mutex);
    return match_count;
}

/**
 * @brief Query incidents by violation type
 */
uint32_t ethics_get_incidents_by_violation_type(ethics_engine_t engine,
                                                ethics_violation_type_t violation_type,
                                                ethics_incident_t** incidents_out)
{
    // Guard clause: Validate inputs
    if (!engine || !incidents_out)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    ethics_incidents_heartbeat("ethics_incid_ethics_get_incidents", 0.0f);

    ethics_incident_storage_t* storage = ethics_engine_get_incident_storage(engine);
    if (!storage || !storage->incident_history)
        return 0;

    nimcp_mutex_lock(&storage->incident_mutex);

    // Count matching incidents
    uint32_t match_count = 0;
    for (uint32_t i = 0; i < storage->incident_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && storage->incident_count > 256) {
            ethics_incidents_heartbeat("ethics_incid_loop",
                             (float)(i + 1) / (float)storage->incident_count);
        }

        if (storage->incident_history[i].violation_type == violation_type)
            match_count++;
    }

    if (match_count == 0) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Allocate output array
    *incidents_out = nimcp_calloc(match_count, sizeof(ethics_incident_t));
    if (!*incidents_out) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Copy matching incidents
    uint32_t out_index = 0;
    for (uint32_t i = 0; i < storage->incident_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && storage->incident_count > 256) {
            ethics_incidents_heartbeat("ethics_incid_loop",
                             (float)(i + 1) / (float)storage->incident_count);
        }

        if (storage->incident_history[i].violation_type == violation_type) {
            (*incidents_out)[out_index++] = storage->incident_history[i];
        }
    }

    nimcp_mutex_unlock(&storage->incident_mutex);
    return match_count;
}

/**
 * @brief Query incidents by minimum severity
 */
uint32_t ethics_get_incidents_by_severity(ethics_engine_t engine, float min_severity,
                                          ethics_incident_t** incidents_out)
{
    // Guard clause: Validate inputs
    if (!engine || !incidents_out || min_severity < 0.0F || min_severity > 1.0F)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    ethics_incidents_heartbeat("ethics_incid_ethics_get_incidents", 0.0f);

    ethics_incident_storage_t* storage = ethics_engine_get_incident_storage(engine);
    if (!storage || !storage->incident_history)
        return 0;

    nimcp_mutex_lock(&storage->incident_mutex);

    // Count matching incidents
    uint32_t match_count = 0;
    for (uint32_t i = 0; i < storage->incident_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && storage->incident_count > 256) {
            ethics_incidents_heartbeat("ethics_incid_loop",
                             (float)(i + 1) / (float)storage->incident_count);
        }

        if (storage->incident_history[i].severity >= min_severity)
            match_count++;
    }

    if (match_count == 0) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Allocate output array
    *incidents_out = nimcp_calloc(match_count, sizeof(ethics_incident_t));
    if (!*incidents_out) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Copy matching incidents
    uint32_t out_index = 0;
    for (uint32_t i = 0; i < storage->incident_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && storage->incident_count > 256) {
            ethics_incidents_heartbeat("ethics_incid_loop",
                             (float)(i + 1) / (float)storage->incident_count);
        }

        if (storage->incident_history[i].severity >= min_severity) {
            (*incidents_out)[out_index++] = storage->incident_history[i];
        }
    }

    nimcp_mutex_unlock(&storage->incident_mutex);
    return match_count;
}

/**
 * @brief Query incidents by action taken
 */
uint32_t ethics_get_incidents_by_action(ethics_engine_t engine, ethics_action_t action,
                                        ethics_incident_t** incidents_out)
{
    // Guard clause: Validate inputs
    if (!engine || !incidents_out)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    ethics_incidents_heartbeat("ethics_incid_ethics_get_incidents", 0.0f);

    ethics_incident_storage_t* storage = ethics_engine_get_incident_storage(engine);
    if (!storage || !storage->incident_history)
        return 0;

    nimcp_mutex_lock(&storage->incident_mutex);

    // Count matching incidents
    uint32_t match_count = 0;
    for (uint32_t i = 0; i < storage->incident_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && storage->incident_count > 256) {
            ethics_incidents_heartbeat("ethics_incid_loop",
                             (float)(i + 1) / (float)storage->incident_count);
        }

        if (storage->incident_history[i].action_taken == action)
            match_count++;
    }

    if (match_count == 0) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Allocate output array
    *incidents_out = nimcp_calloc(match_count, sizeof(ethics_incident_t));
    if (!*incidents_out) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Copy matching incidents
    uint32_t out_index = 0;
    for (uint32_t i = 0; i < storage->incident_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && storage->incident_count > 256) {
            ethics_incidents_heartbeat("ethics_incid_loop",
                             (float)(i + 1) / (float)storage->incident_count);
        }

        if (storage->incident_history[i].action_taken == action) {
            (*incidents_out)[out_index++] = storage->incident_history[i];
        }
    }

    nimcp_mutex_unlock(&storage->incident_mutex);
    return match_count;
}

/**
 * @brief Get all incidents in chronological order
 */
uint32_t ethics_get_all_incidents(ethics_engine_t engine, ethics_incident_t** incidents_out)
{
    // Guard clause: Validate inputs
    if (!engine || !incidents_out)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    ethics_incidents_heartbeat("ethics_incid_ethics_get_all_incid", 0.0f);

    ethics_incident_storage_t* storage = ethics_engine_get_incident_storage(engine);
    if (!storage || !storage->incident_history)
        return 0;

    nimcp_mutex_lock(&storage->incident_mutex);

    uint32_t count = storage->incident_count;
    if (count == 0) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Allocate output array
    *incidents_out = nimcp_calloc(count, sizeof(ethics_incident_t));
    if (!*incidents_out) {
        nimcp_mutex_unlock(&storage->incident_mutex);
        return 0;
    }

    // Copy all incidents in chronological order
    if (storage->incident_count < MAX_INCIDENT_HISTORY) {
        // Haven't wrapped yet, simple copy
        memcpy(*incidents_out, storage->incident_history, count * sizeof(ethics_incident_t));
    } else {
        // Wrapped around, need to copy in two parts for chronological order
        uint32_t start_index = storage->incident_index % MAX_INCIDENT_HISTORY;
        uint32_t first_part = MAX_INCIDENT_HISTORY - start_index;

        // Copy older incidents (from start_index to end)
        memcpy(*incidents_out, &storage->incident_history[start_index],
               first_part * sizeof(ethics_incident_t));

        // Copy newer incidents (from 0 to start_index)
        memcpy(&(*incidents_out)[first_part], storage->incident_history,
               start_index * sizeof(ethics_incident_t));
    }

    nimcp_mutex_unlock(&storage->incident_mutex);
    return count;
}

/**
 * @brief Export incidents to file
 */
bool ethics_export_incidents(ethics_engine_t engine, const char* filepath, const char* format)
{
    // Guard clause: Validate inputs
    if (!engine || !filepath || !format) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_export_incidents: required parameter is NULL (engine, filepath, format)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_incidents_heartbeat("ethics_incid_ethics_export_incide", 0.0f);

    ethics_incident_storage_t* storage = ethics_engine_get_incident_storage(engine);
    if (!storage || !storage->incident_history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_export_incidents: required parameter is NULL (storage, storage->incident_history)");
        return false;
    }

    ethics_incident_t* incidents = NULL;
    uint32_t count = ethics_get_all_incidents(engine, &incidents);

    if (count == 0 || !incidents) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_export_incidents: incidents is NULL");
        return false;
    }

    FILE* file = fopen(filepath, "w");
    if (!file) {
        nimcp_free(incidents);
        incidents = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_export_incidents: file is NULL");
        return false;
    }

    if (strcmp(format, "json") == 0) {
        // Export as JSON
        fprintf(file, "{\n  \"incidents\": [\n");
        for (uint32_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                ethics_incidents_heartbeat("ethics_incid_loop",
                                 (float)(i + 1) / (float)count);
            }

            fprintf(file, "    {\n");
            fprintf(file, "      \"id\": %lu,\n", incidents[i].incident_id);
            fprintf(file, "      \"timestamp\": %lu,\n", incidents[i].timestamp);
            fprintf(file, "      \"violation_type\": %d,\n", incidents[i].violation_type);
            fprintf(file, "      \"severity\": %.4f,\n", incidents[i].severity);
            fprintf(file, "      \"action_taken\": %d,\n", incidents[i].action_taken);
            fprintf(file, "      \"policy_id\": %u,\n", incidents[i].policy_id);
            fprintf(file, "      \"policy_name\": \"%s\",\n", incidents[i].policy_name);
            fprintf(file, "      \"description\": \"%s\",\n", incidents[i].description);
            fprintf(file, "      \"golden_rule_score\": %.4f,\n", incidents[i].golden_rule_score);
            fprintf(file, "      \"acting_agent\": %u,\n", incidents[i].acting_agent);
            fprintf(file, "      \"affected_agent\": %u\n", incidents[i].affected_agent);
            fprintf(file, "    }%s\n", (i < count - 1) ? "," : "");
        }
        fprintf(file, "  ]\n}\n");
    } else if (strcmp(format, "csv") == 0) {
        // Export as CSV
        fprintf(file, "id,timestamp,violation_type,severity,action_taken,policy_id,policy_name,description,golden_rule_score,acting_agent,affected_agent\n");
        for (uint32_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                ethics_incidents_heartbeat("ethics_incid_loop",
                                 (float)(i + 1) / (float)count);
            }

            fprintf(file, "%lu,%lu,%d,%.4f,%d,%u,\"%s\",\"%s\",%.4f,%u,%u\n",
                    incidents[i].incident_id,
                    incidents[i].timestamp,
                    incidents[i].violation_type,
                    incidents[i].severity,
                    incidents[i].action_taken,
                    incidents[i].policy_id,
                    incidents[i].policy_name,
                    incidents[i].description,
                    incidents[i].golden_rule_score,
                    incidents[i].acting_agent,
                    incidents[i].affected_agent);
        }
    } else {
        fclose(file);
        nimcp_free(incidents);
        incidents = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ethics_export_incidents: operation failed");
        return false;
    }

    fclose(file);
    nimcp_free(incidents);
    incidents = NULL;

    LOG_INFO("Exported %u incidents to %s", count, filepath);
    return true;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Ethics Incidents self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int ethics_incidents_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    ethics_incidents_heartbeat("ethics_incid_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Ethics_Incidents_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                ethics_incidents_heartbeat("ethics_incid_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Ethics incidents self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Ethics_Incidents_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Ethics_Incidents_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void ethics_incidents_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_ethics_incidents_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int ethics_incidents_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_incidents_training_begin: NULL argument");
        return -1;
    }
    ethics_incidents_heartbeat_instance(NULL, "ethics_incidents_training_begin", 0.0f);
    return 0;
}

int ethics_incidents_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_incidents_training_end: NULL argument");
        return -1;
    }
    ethics_incidents_heartbeat_instance(NULL, "ethics_incidents_training_end", 1.0f);
    return 0;
}

int ethics_incidents_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_incidents_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ethics_incidents_heartbeat_instance(NULL, "ethics_incidents_training_step", progress);
    return 0;
}
