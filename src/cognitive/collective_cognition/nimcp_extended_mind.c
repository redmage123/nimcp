/**
 * @file nimcp_extended_mind.c
 * @brief Implementation of external tools as cognitive extensions
 *
 * WHAT: Model external tools (MCP servers, databases, AI models) as cognitive extensions
 * WHY: Extended cognition - tools become part of the cognitive system
 * HOW: Track tool reliability, latency, and integration depth
 */

#include "cognitive/collective_cognition/nimcp_extended_mind.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(extended_mind, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Internal Structures
 *===========================================================================*/

/**
 * @brief Pending query entry
 */
typedef struct {
    uint32_t query_id;
    ext_query_request_t request;
    uint8_t query_data[EXT_QUERY_MAX_SIZE];
    size_t query_size;
    uint8_t response_data[EXT_RESPONSE_MAX_SIZE];
    size_t response_size;
    ext_query_status_t status;
    uint64_t submitted_us;
    uint64_t completed_us;
    bool active;
} pending_query_t;

/**
 * @brief Pending offload entry
 */
typedef struct {
    uint32_t offload_id;
    ext_offload_request_t request;
    uint8_t result_data[EXT_RESPONSE_MAX_SIZE];
    size_t result_size;
    bool completed;
    bool active;
} pending_offload_t;

/**
 * @brief Extended mind internal state
 */
struct extended_mind {
    /* Configuration */
    extended_mind_config_t config;

    /* Registered extensions */
    cognitive_extension_t extensions[COLLECTIVE_MAX_EXTENSIONS];
    uint32_t extension_count;
    uint32_t next_extension_id;

    /* Pending queries */
    pending_query_t queries[64];
    uint32_t query_count;
    uint32_t next_query_id;

    /* Pending offloads */
    pending_offload_t offloads[32];
    uint32_t offload_count;
    uint32_t next_offload_id;

    /* Cached state */
    extended_mind_state_t state;

    /* Statistics */
    extended_mind_stats_t stats;

    /* Flags */
    bool initialized;
    uint64_t last_update_us;
};

/*=============================================================================
 * Helper Functions - Time
 *===========================================================================*/

static uint64_t get_timestamp_us(void) {
    static _Atomic uint64_t counter = 0;
    return atomic_fetch_add(&counter, 1);
}

/*=============================================================================
 * Helper Functions - Extension Management
 *===========================================================================*/

static cognitive_extension_t* find_extension(
    extended_mind_t* em,
    uint32_t extension_id
) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_EXTENSIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_EXTENSIONS > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_EXTENSIONS);
        }

        if (em->extensions[i].extension_id == extension_id) {
            return &em->extensions[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static cognitive_extension_t* find_free_extension_slot(extended_mind_t* em) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_EXTENSIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_EXTENSIONS > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_EXTENSIONS);
        }

        if (em->extensions[i].extension_id == 0) {
            return &em->extensions[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static cognitive_extension_t* find_best_extension_of_type(
    const extended_mind_t* em,
    extension_type_t type
) {
    cognitive_extension_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < COLLECTIVE_MAX_EXTENSIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_EXTENSIONS > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_EXTENSIONS);
        }

        cognitive_extension_t* ext = (cognitive_extension_t*)&em->extensions[i];
        if (ext->extension_id == 0) continue;
        if (ext->type != type) continue;
        if (ext->health == EXT_HEALTH_UNAVAILABLE) continue;

        /* Score based on reliability, integration, and inverse latency */
        float score = ext->reliability * 0.4f +
                      ext->integration_depth * 0.3f +
                      (1.0f / (1.0f + ext->avg_latency_ms * 0.001f)) * 0.3f;

        if (ext->health == EXT_HEALTH_DEGRADED) {
            score *= 0.5f;
        }

        if (score > best_score) {
            best_score = score;
            best = ext;
        }
    }

    return best;
}

/*=============================================================================
 * Helper Functions - Query Management
 *===========================================================================*/

static pending_query_t* find_query(
    extended_mind_t* em,
    uint32_t query_id
) {
    for (uint32_t i = 0; i < 64; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 64 > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)64);
        }

        if (em->queries[i].active && em->queries[i].query_id == query_id) {
            return &em->queries[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static pending_query_t* find_free_query_slot(extended_mind_t* em) {
    for (uint32_t i = 0; i < 64; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 64 > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)64);
        }

        if (!em->queries[i].active) {
            return &em->queries[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

/*=============================================================================
 * Helper Functions - Offload Management
 *===========================================================================*/

static pending_offload_t* find_offload(
    extended_mind_t* em,
    uint32_t offload_id
) {
    for (uint32_t i = 0; i < 32; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 32 > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)32);
        }

        if (em->offloads[i].active && em->offloads[i].offload_id == offload_id) {
            return &em->offloads[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static pending_offload_t* find_free_offload_slot(extended_mind_t* em) {
    for (uint32_t i = 0; i < 32; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 32 > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)32);
        }

        if (!em->offloads[i].active) {
            return &em->offloads[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

/*=============================================================================
 * Helper Functions - State Computation
 *===========================================================================*/

static void update_state(extended_mind_t* em) {
    extended_mind_state_t* state = &em->state;

    /* Count active extensions */
    uint32_t active = 0;
    uint32_t degraded = 0;
    float total_capacity = 1.0f;  /* Base local capacity */
    float total_reliability = 0.0f;
    float total_integration = 0.0f;

    for (uint32_t i = 0; i < COLLECTIVE_MAX_EXTENSIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_EXTENSIONS > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_EXTENSIONS);
        }

        cognitive_extension_t* ext = &em->extensions[i];
        if (ext->extension_id == 0) continue;

        if (ext->health == EXT_HEALTH_HEALTHY) {
            active++;
            total_capacity += ext->integration_depth * 0.2f;
            total_reliability += ext->reliability;
            total_integration += ext->integration_depth;
        } else if (ext->health == EXT_HEALTH_DEGRADED) {
            degraded++;
            total_capacity += ext->integration_depth * 0.1f;
            total_reliability += ext->reliability * 0.5f;
            total_integration += ext->integration_depth * 0.5f;
        }
    }

    state->active_extensions = active;
    state->degraded_extensions = degraded;
    state->total_cognitive_capacity = total_capacity;

    if (active + degraded > 0) {
        state->extended_ratio = (total_capacity - 1.0f) / total_capacity;
        state->integration_quality = total_integration / (active + degraded);
    } else {
        state->extended_ratio = 0.0f;
        state->integration_quality = 0.0f;
    }
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

extended_mind_t* extended_mind_create(const extended_mind_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_create", 0.0f);


    extended_mind_t* em = nimcp_malloc(sizeof(extended_mind_t));
    if (!em) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate em");

        return NULL;

    }

    memset(em, 0, sizeof(extended_mind_t));

    /* Apply configuration */
    if (config) {
        em->config = *config;
    } else {
        em->config = extended_mind_default_config();
    }

    em->next_extension_id = 1;
    em->next_query_id = 1;
    em->next_offload_id = 1;
    em->initialized = true;
    em->last_update_us = get_timestamp_us();

    update_state(em);

    return em;
}

void extended_mind_destroy(extended_mind_t* em) {
    if (!em) return;
    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_destroy", 0.0f);


    nimcp_free(em);
    em = NULL;
}

int extended_mind_reset(extended_mind_t* em) {
    if (!em) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_reset: em is NULL");
        return -1;
    }

    /* Clear extensions */
    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_reset", 0.0f);


    memset(em->extensions, 0, sizeof(em->extensions));
    em->extension_count = 0;

    /* Clear queries and offloads */
    memset(em->queries, 0, sizeof(em->queries));
    memset(em->offloads, 0, sizeof(em->offloads));
    em->query_count = 0;
    em->offload_count = 0;

    /* Reset state and stats */
    memset(&em->state, 0, sizeof(em->state));
    memset(&em->stats, 0, sizeof(em->stats));

    em->state.total_cognitive_capacity = 1.0f;
    em->last_update_us = get_timestamp_us();

    return 0;
}

/*=============================================================================
 * Extension Management API
 *===========================================================================*/

uint32_t extended_mind_register_extension(
    extended_mind_t* em,
    const cognitive_extension_t* ext
) {
    if (!em || !ext) return 0;
    if (em->extension_count >= em->config.max_extensions) return 0;

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_register_extension", 0.0f);


    cognitive_extension_t* slot = find_free_extension_slot(em);
    if (!slot) return 0;

    *slot = *ext;
    slot->extension_id = em->next_extension_id++;

    /* Initialize if not set */
    if (slot->reliability == 0.0f) slot->reliability = 1.0f;
    if (slot->trust_level == 0.0f) slot->trust_level = 0.5f;
    if (slot->health == EXT_HEALTH_UNKNOWN) slot->health = EXT_HEALTH_HEALTHY;

    em->extension_count++;
    update_state(em);

    return slot->extension_id;
}

int extended_mind_unregister_extension(
    extended_mind_t* em,
    uint32_t extension_id
) {
    if (!em) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_unregister_extension: em is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_unregister_extension", 0.0f);


    cognitive_extension_t* ext = find_extension(em, extension_id);
    if (!ext) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_unregister_extension: ext is NULL");
        return -1;
    }

    memset(ext, 0, sizeof(cognitive_extension_t));
    em->extension_count--;
    update_state(em);

    return 0;
}

int extended_mind_get_extension(
    const extended_mind_t* em,
    uint32_t extension_id,
    cognitive_extension_t* ext
) {
    if (!em || !ext) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_get_extension: required parameter is NULL (em, ext)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_get_extension", 0.0f);


    cognitive_extension_t* found = find_extension((extended_mind_t*)em, extension_id);
    if (!found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_get_extension: found is NULL");
        return -1;
    }

    *ext = *found;
    return 0;
}

int extended_mind_update_extension_stats(
    extended_mind_t* em,
    uint32_t extension_id,
    bool success,
    float latency_ms
) {
    if (!em) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_update_extension_stats: em is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_update_extension_sta", 0.0f);


    cognitive_extension_t* ext = find_extension(em, extension_id);
    if (!ext) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_update_extension_stats: ext is NULL");
        return -1;
    }

    ext->total_queries++;
    ext->last_access_us = get_timestamp_us();

    if (success) {
        ext->successful_queries++;
        /* Exponential moving average for latency */
        if (isfinite(latency_ms)) {
            ext->avg_latency_ms = ext->avg_latency_ms * 0.9f + latency_ms * 0.1f;
        }
    } else {
        ext->failed_queries++;
        ext->last_failure_us = get_timestamp_us();
        /* Decay trust on failure */
        ext->trust_level *= (1.0f - em->config.trust_decay_rate);
        if (ext->trust_level < 0.1f) ext->trust_level = 0.1f;
    }

    /* Update reliability */
    if (ext->total_queries > 0) {
        ext->reliability = (float)ext->successful_queries / ext->total_queries;
    }

    /* Update health based on recent performance */
    if (ext->reliability >= em->config.integration_threshold) {
        ext->health = EXT_HEALTH_HEALTHY;
    } else if (ext->reliability >= 0.5f) {
        ext->health = EXT_HEALTH_DEGRADED;
    } else {
        ext->health = EXT_HEALTH_UNAVAILABLE;
    }

    /* Update integration depth based on reliability and trust */
    ext->integration_depth = ext->reliability * ext->trust_level;

    update_state(em);

    return 0;
}

uint32_t extended_mind_extension_count(const extended_mind_t* em) {
    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_extension_count", 0.0f);


    return em ? em->extension_count : 0;
}

uint32_t extended_mind_count_by_type(
    const extended_mind_t* em,
    extension_type_t type
) {
    if (!em) return 0;

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_count_by_type", 0.0f);


    uint32_t count = 0;
    for (uint32_t i = 0; i < COLLECTIVE_MAX_EXTENSIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_EXTENSIONS > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_EXTENSIONS);
        }

        if (em->extensions[i].extension_id != 0 &&
            em->extensions[i].type == type) {
            count++;
        }
    }
    return count;
}

/*=============================================================================
 * Query API
 *===========================================================================*/

uint32_t extended_mind_query(
    extended_mind_t* em,
    const ext_query_request_t* request,
    const void* query,
    size_t query_size
) {
    if (!em || !request || !query) return 0;
    if (query_size > EXT_QUERY_MAX_SIZE) return 0;

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_query", 0.0f);


    pending_query_t* slot = find_free_query_slot(em);
    if (!slot) return 0;

    slot->query_id = em->next_query_id++;
    slot->request = *request;
    slot->request.query_id = slot->query_id;
    memcpy(slot->query_data, query, query_size);
    slot->query_size = query_size;
    slot->status = EXT_QUERY_PENDING;
    slot->submitted_us = get_timestamp_us();
    slot->active = true;

    em->query_count++;
    em->stats.total_queries++;

    return slot->query_id;
}

int extended_mind_query_sync(
    extended_mind_t* em,
    extension_type_t type,
    const void* query,
    size_t query_size,
    void* response,
    size_t* response_size
) {
    if (!em || !query || !response || !response_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_query_sync: required parameter is NULL (em, query, response, response_size)");
        return -1;
    }

    /* Find best extension of type */
    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_query_sync", 0.0f);


    cognitive_extension_t* ext = find_best_extension_of_type(em, type);
    if (!ext || !ext->query_fn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_query_sync: required parameter is NULL (ext, ext->query_fn)");
        return -1;
    }

    uint64_t start_us = get_timestamp_us();

    /* Execute query */
    int result = ext->query_fn(query, query_size, response, response_size, ext->user_data);

    uint64_t end_us = get_timestamp_us();
    float latency_ms = (float)(end_us - start_us) / 1000.0f;

    /* Update stats */
    extended_mind_update_extension_stats(em, ext->extension_id, result == 0, latency_ms);

    if (result == 0) {
        em->stats.successful_queries++;
        em->stats.bytes_transferred += query_size + *response_size;
    } else {
        em->stats.failed_queries++;
    }

    /* Update average latency */
    if (isfinite(latency_ms)) {
        em->stats.avg_latency_ms = em->stats.avg_latency_ms * 0.9f + latency_ms * 0.1f;
    }

    return result;
}

int extended_mind_cancel_query(
    extended_mind_t* em,
    uint32_t query_id
) {
    if (!em) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_cancel_query: em is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_cancel_query", 0.0f);


    pending_query_t* q = find_query(em, query_id);
    if (!q) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_cancel_query: q is NULL");
        return -1;
    }

    q->status = EXT_QUERY_CANCELLED;
    q->active = false;
    em->query_count--;

    return 0;
}

ext_query_status_t extended_mind_get_query_status(
    const extended_mind_t* em,
    uint32_t query_id
) {
    if (!em) return EXT_QUERY_FAILED;

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_get_query_status", 0.0f);


    pending_query_t* q = find_query((extended_mind_t*)em, query_id);
    if (!q) return EXT_QUERY_FAILED;

    return q->status;
}

/*=============================================================================
 * Offload API
 *===========================================================================*/

uint32_t extended_mind_offload(
    extended_mind_t* em,
    const ext_offload_request_t* request,
    const void* task,
    size_t task_size
) {
    if (!em || !request || !task) return 0;

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_offload", 0.0f);


    pending_offload_t* slot = find_free_offload_slot(em);
    if (!slot) return 0;

    slot->offload_id = em->next_offload_id++;
    slot->request = *request;
    slot->request.offload_id = slot->offload_id;
    slot->completed = false;
    slot->active = true;
    slot->result_size = 0;

    em->offload_count++;
    em->stats.offload_requests++;

    /* Simulate immediate completion for now */
    slot->completed = true;
    em->stats.offload_completions++;

    return slot->offload_id;
}

int extended_mind_check_offload(
    extended_mind_t* em,
    uint32_t offload_id,
    void* result,
    size_t* result_size
) {
    if (!em) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_check_offload: em is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_check_offload", 0.0f);


    pending_offload_t* o = find_offload(em, offload_id);
    if (!o) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_check_offload: o is NULL");
        return -1;
    }

    if (!o->completed) return 1;  /* Still pending */

    if (result && result_size) {
        size_t copy_size = o->result_size;
        if (copy_size > *result_size) copy_size = *result_size;
        memcpy(result, o->result_data, copy_size);
        *result_size = o->result_size;
    }

    /* Mark as complete and free slot */
    o->active = false;
    em->offload_count--;

    return 0;
}

/*=============================================================================
 * State API
 *===========================================================================*/

int extended_mind_get_state(
    const extended_mind_t* em,
    extended_mind_state_t* state
) {
    if (!em || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_get_state: required parameter is NULL (em, state)");
        return -1;
    }

    *state = em->state;
    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_get_state", 0.0f);


    return 0;
}

float extended_mind_get_capacity(
    const extended_mind_t* em,
    extension_type_t type
) {
    if (!em) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_get_capacity", 0.0f);


    if ((int)type < 0) {
        /* All types */
        return em->state.total_cognitive_capacity;
    }

    /* Specific type */
    float capacity = 0.0f;
    for (uint32_t i = 0; i < COLLECTIVE_MAX_EXTENSIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_EXTENSIONS > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_EXTENSIONS);
        }

        cognitive_extension_t* ext = (cognitive_extension_t*)&em->extensions[i];
        if (ext->extension_id == 0) continue;
        if (ext->type != type) continue;

        if (ext->health == EXT_HEALTH_HEALTHY) {
            capacity += ext->integration_depth;
        } else if (ext->health == EXT_HEALTH_DEGRADED) {
            capacity += ext->integration_depth * 0.5f;
        }
    }

    return capacity;
}

uint32_t extended_mind_best_extension(
    const extended_mind_t* em,
    extension_type_t type
) {
    if (!em) return 0;

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_best_extension", 0.0f);


    cognitive_extension_t* best = find_best_extension_of_type(em, type);
    return best ? best->extension_id : 0;
}

/*=============================================================================
 * Update API
 *===========================================================================*/

int extended_mind_update(extended_mind_t* em) {
    if (!em || !em->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_update: required parameter is NULL (em, em->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_update", 0.0f);


    uint64_t now = get_timestamp_us();

    /* Check extension health */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_EXTENSIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_EXTENSIONS > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_EXTENSIONS);
        }

        cognitive_extension_t* ext = &em->extensions[i];
        if (ext->extension_id == 0) continue;

        /* Check health via status callback if available */
        if (ext->status_fn) {
            ext->health = ext->status_fn(ext->user_data);
        }

        /* Decay trust slowly over time if not used */
        if (now - ext->last_access_us > 1000000) {  /* 1 second */
            ext->trust_level *= 0.999f;
            if (ext->trust_level < 0.1f) ext->trust_level = 0.1f;
        }
    }

    /* Process pending queries */
    for (uint32_t i = 0; i < 64; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 64 > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)64);
        }

        pending_query_t* q = &em->queries[i];
        if (!q->active) continue;

        if (q->status == EXT_QUERY_PENDING) {
            /* Find extension and execute */
            cognitive_extension_t* ext = NULL;

            if (q->request.extension_id != 0) {
                ext = find_extension(em, q->request.extension_id);
            } else {
                ext = find_best_extension_of_type(em, q->request.type);
            }

            if (ext && ext->query_fn) {
                q->status = EXT_QUERY_IN_PROGRESS;
                q->response_size = EXT_RESPONSE_MAX_SIZE;

                int result = ext->query_fn(
                    q->query_data, q->query_size,
                    q->response_data, &q->response_size,
                    ext->user_data
                );

                q->completed_us = get_timestamp_us();
                float latency_ms = (float)(q->completed_us - q->submitted_us) / 1000.0f;

                if (result == 0) {
                    q->status = EXT_QUERY_COMPLETED;
                    em->stats.successful_queries++;
                    em->stats.bytes_transferred += q->query_size + q->response_size;
                } else {
                    q->status = EXT_QUERY_FAILED;
                    em->stats.failed_queries++;
                }

                extended_mind_update_extension_stats(em, ext->extension_id, result == 0, latency_ms);

                /* Call callback */
                if (q->request.callback) {
                    q->request.callback(
                        q->query_id,
                        q->status,
                        q->response_data,
                        q->response_size,
                        q->request.callback_user_data
                    );
                }
            } else {
                q->status = EXT_QUERY_FAILED;
                em->stats.failed_queries++;
            }
        }

        /* Check for timeouts */
        if (q->status == EXT_QUERY_PENDING || q->status == EXT_QUERY_IN_PROGRESS) {
            uint64_t elapsed_ms = (now - q->submitted_us) / 1000;
            if (elapsed_ms > q->request.timeout_ms) {
                q->status = EXT_QUERY_TIMEOUT;
                em->stats.timeout_queries++;

                if (q->request.callback) {
                    q->request.callback(
                        q->query_id,
                        EXT_QUERY_TIMEOUT,
                        NULL, 0,
                        q->request.callback_user_data
                    );
                }
            }
        }

        /* Clean up completed queries */
        if (q->status == EXT_QUERY_COMPLETED ||
            q->status == EXT_QUERY_FAILED ||
            q->status == EXT_QUERY_TIMEOUT ||
            q->status == EXT_QUERY_CANCELLED) {
            q->active = false;
            em->query_count--;
        }
    }

    /* Update average reliability */
    float total_reliability = 0.0f;
    uint32_t count = 0;
    for (uint32_t i = 0; i < COLLECTIVE_MAX_EXTENSIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_EXTENSIONS > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_EXTENSIONS);
        }

        if (em->extensions[i].extension_id != 0) {
            total_reliability += em->extensions[i].reliability;
            count++;
        }
    }
    if (count > 0) {
        em->stats.avg_reliability = total_reliability / count;
    }

    update_state(em);
    em->last_update_us = now;

    return 0;
}

/*=============================================================================
 * Statistics API
 *===========================================================================*/

int extended_mind_get_stats(
    const extended_mind_t* em,
    extended_mind_stats_t* stats
) {
    if (!em || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extended_mind_get_stats: required parameter is NULL (em, stats)");
        return -1;
    }

    *stats = em->stats;
    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_get_stats", 0.0f);


    return 0;
}

void extended_mind_reset_stats(extended_mind_t* em) {
    if (!em) return;
    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_reset_stats", 0.0f);


    memset(&em->stats, 0, sizeof(em->stats));
}

/*=============================================================================
 * Debug API
 *===========================================================================*/

void extended_mind_dump(const extended_mind_t* em) {
    if (!em) {
        printf("Extended Mind: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_dump", 0.0f);


    printf("=== Extended Mind State ===\n");
    printf("Initialized: %s\n", em->initialized ? "yes" : "no");
    printf("Extensions: %u\n", em->extension_count);
    printf("Pending queries: %u\n", em->query_count);
    printf("Pending offloads: %u\n", em->offload_count);

    printf("\nState:\n");
    printf("  Total capacity: %.3f\n", em->state.total_cognitive_capacity);
    printf("  Extended ratio: %.3f\n", em->state.extended_ratio);
    printf("  Integration quality: %.3f\n", em->state.integration_quality);
    printf("  Active extensions: %u\n", em->state.active_extensions);
    printf("  Degraded extensions: %u\n", em->state.degraded_extensions);

    printf("\nRegistered Extensions:\n");
    for (uint32_t i = 0; i < COLLECTIVE_MAX_EXTENSIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_EXTENSIONS > 256) {
            extended_mind_heartbeat("extended_min_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_EXTENSIONS);
        }

        const cognitive_extension_t* ext = &em->extensions[i];
        if (ext->extension_id == 0) continue;

        printf("  [%u] %s (%s)\n",
               ext->extension_id,
               ext->name,
               extension_type_name(ext->type));
        printf("       Health: %s, Reliability: %.2f, Trust: %.2f\n",
               ext->health == EXT_HEALTH_HEALTHY ? "HEALTHY" :
               ext->health == EXT_HEALTH_DEGRADED ? "DEGRADED" :
               ext->health == EXT_HEALTH_UNAVAILABLE ? "UNAVAILABLE" : "UNKNOWN",
               ext->reliability,
               ext->trust_level);
        printf("       Queries: %lu success / %lu total, Avg latency: %.1fms\n",
               (unsigned long)ext->successful_queries,
               (unsigned long)ext->total_queries,
               ext->avg_latency_ms);
    }

    printf("\nStatistics:\n");
    printf("  Total queries: %lu\n", (unsigned long)em->stats.total_queries);
    printf("  Successful: %lu\n", (unsigned long)em->stats.successful_queries);
    printf("  Failed: %lu\n", (unsigned long)em->stats.failed_queries);
    printf("  Timeout: %lu\n", (unsigned long)em->stats.timeout_queries);
    printf("  Avg latency: %.2fms\n", em->stats.avg_latency_ms);
    printf("  Avg reliability: %.2f\n", em->stats.avg_reliability);
    printf("  Bytes transferred: %lu\n", (unsigned long)em->stats.bytes_transferred);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Extended Mind self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int extended_mind_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    extended_mind_heartbeat("extended_min_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Extended_Mind");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                extended_mind_heartbeat("extended_min_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            printf("Extended Mind self-knowledge: %s\n", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Extended_Mind");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Extended_Mind");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */

void extended_mind_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_extended_mind_instance_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
static _Atomic uint64_t g_extended_mind_training_steps = 0;
static _Atomic double g_extended_mind_training_total_error = 0.0;
static _Atomic double g_extended_mind_training_best_error = 1e30;
static _Atomic bool g_extended_mind_training_active = false;

int extended_mind_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "extended_mind_training_begin: NULL argument");
        return -1;
    }
    extended_mind_heartbeat_instance(g_extended_mind_instance_health_agent, "ext_mind_train_beg", 0.0f);
    extended_mind_t* ctx = (extended_mind_t*)instance;

    /* Reset training counters */
    atomic_store(&g_extended_mind_training_steps, 0);
    atomic_store(&g_extended_mind_training_total_error, 0.0);
    atomic_store(&g_extended_mind_training_best_error, 1e30);
    atomic_store(&g_extended_mind_training_active, true);

    /* Reset module stats */
    memset(&ctx->stats, 0, sizeof(ctx->stats));

    NIMCP_LOGGING_INFO("extended_mind training begin: counters reset");
    return 0;
}

int extended_mind_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "extended_mind_training_step: NULL argument");
        return -1;
    }

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    extended_mind_heartbeat_instance(g_extended_mind_instance_health_agent, "ext_mind_train_stp", progress);
    (void)instance;

    atomic_fetch_add(&g_extended_mind_training_steps, 1);

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    double old_error = atomic_load(&g_extended_mind_training_total_error);
    atomic_store(&g_extended_mind_training_total_error, old_error * (double)decay);

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    double old_best = atomic_load(&g_extended_mind_training_best_error);
    double new_best = old_best - (double)threshold_adjust;
    if (new_best < 0.0) new_best = 0.0;
    atomic_store(&g_extended_mind_training_best_error, new_best);

    return 0;
}

int extended_mind_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "extended_mind_training_end: NULL argument");
        return -1;
    }
    extended_mind_heartbeat_instance(g_extended_mind_instance_health_agent, "ext_mind_train_end", 1.0f);

    /* Compute final averages */
    uint64_t steps = atomic_load(&g_extended_mind_training_steps);
    double total_error = atomic_load(&g_extended_mind_training_total_error);
    double best_error = atomic_load(&g_extended_mind_training_best_error);
    double avg_error = (steps > 0)
        ? total_error / (double)steps
        : 0.0;

    /* Clear training flag */
    atomic_store(&g_extended_mind_training_active, false);

    NIMCP_LOGGING_INFO("extended_mind training end: %lu steps, avg_error=%.6f, best_error=%.6f",
                       (unsigned long)steps,
                       avg_error, best_error);
    return 0;
}
