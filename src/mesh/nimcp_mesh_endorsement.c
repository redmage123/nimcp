/**
 * @file nimcp_mesh_endorsement.c
 * @brief Brain-Inspired Endorsement Collection Implementation
 *
 * Uses pattern routing for endorser selection instead of static policies.
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 2.0.0
 */

#include "mesh/nimcp_mesh_endorsement.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_CONCURRENT_COLLECTIONS      64
#define DEFAULT_TIMEOUT_MS              100.0f
#define DEFAULT_REQUIRED_RATIO          1.0f    /* All REQUIRED must endorse */
#define DEFAULT_PREFERRED_RATIO         0.5f    /* At least half of PREFERRED */
#define DEFAULT_MIN_ENDORSERS           1

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Endorsement collector
 */
struct mesh_endorsement_collector {
    mesh_endorsement_collector_config_t config;
    mesh_pattern_router_t* router;      /**< Pattern router for selection */

    endorsement_collection_t* collections;
    size_t collection_count;
    size_t collection_capacity;

    nimcp_mutex_t* mutex;
    bool enable_logging;
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static endorsement_collection_t* find_collection(
    mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
) {
    if (!collector || !tx_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_collection: required parameter is NULL (collector, tx_id)");
        return NULL;
    }

    for (size_t i = 0; i < collector->collection_count; i++) {
        if (collector->collections[i].tx_id.sequence == tx_id->sequence &&
            collector->collections[i].tx_id.channel == tx_id->channel) {
            return &collector->collections[i];
        }
    }
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return NULL;
}

static endorsement_collection_t* allocate_collection(
    mesh_endorsement_collector_t* collector
) {
    if (!collector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_collection: collector is NULL");
        return NULL;
    }
    if (collector->collection_count >= collector->collection_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "allocate_collection: capacity exceeded");
        return NULL;
    }
    return &collector->collections[collector->collection_count++];
}

static void check_quorum(
    endorsement_collection_t* coll,
    const endorsement_quorum_t* quorum
) {
    if (!coll) return;

    /* Count endorsements by role */
    size_t required_received = 0;
    size_t preferred_received = 0;
    size_t total_received = 0;

    for (size_t i = 0; i < coll->received.count; i++) {
        mesh_participant_id_t endorser_id = coll->received.endorsements[i].endorser_id;
        endorsement_result_t result = coll->received.endorsements[i].result;

        /* Check if this is a veto */
        if (result == ENDORSEMENT_REJECTED) {
            /* Find endorser role */
            for (size_t j = 0; j < coll->selected.count; j++) {
                if (coll->selected.endorsers[j].id == endorser_id) {
                    if (coll->selected.endorsers[j].role == ENDORSER_ROLE_VETO &&
                        quorum->allow_veto) {
                        coll->vetoed = true;
                        coll->collection_complete = true;
                        return;
                    }
                    break;
                }
            }
            continue;
        }

        if (result != ENDORSEMENT_APPROVED) continue;

        total_received++;

        /* Find endorser role */
        for (size_t j = 0; j < coll->selected.count; j++) {
            if (coll->selected.endorsers[j].id == endorser_id) {
                switch (coll->selected.endorsers[j].role) {
                case ENDORSER_ROLE_REQUIRED:
                    required_received++;
                    break;
                case ENDORSER_ROLE_PREFERRED:
                    preferred_received++;
                    break;
                default:
                    break;
                }
                break;
            }
        }
    }

    /* Check quorum requirements */
    size_t required_needed = (size_t)(coll->selected.required_count * quorum->required_ratio);
    size_t preferred_needed = (size_t)(coll->selected.preferred_count * quorum->preferred_ratio);

    bool required_met = (required_received >= required_needed);
    bool preferred_met = (preferred_received >= preferred_needed);
    bool min_met = (total_received >= quorum->min_endorsers);

    coll->quorum_met = required_met && preferred_met && min_met;
    if (coll->quorum_met) {
        coll->collection_complete = true;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

nimcp_error_t mesh_endorsement_collector_default_config(
    mesh_endorsement_collector_config_t* config
) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));
    config->max_concurrent = MAX_CONCURRENT_COLLECTIONS;
    config->default_timeout_ms = DEFAULT_TIMEOUT_MS;
    config->default_quorum.required_ratio = DEFAULT_REQUIRED_RATIO;
    config->default_quorum.preferred_ratio = DEFAULT_PREFERRED_RATIO;
    config->default_quorum.min_endorsers = DEFAULT_MIN_ENDORSERS;
    config->default_quorum.allow_veto = true;
    config->enable_logging = true;

    return NIMCP_SUCCESS;
}

mesh_endorsement_collector_t* mesh_endorsement_collector_create(
    const mesh_endorsement_collector_config_t* config,
    mesh_pattern_router_t* router
) {
    mesh_endorsement_collector_config_t default_config;
    if (!config) {
        mesh_endorsement_collector_default_config(&default_config);
        config = &default_config;
    }

    mesh_endorsement_collector_t* collector = nimcp_calloc(1, sizeof(*collector));
    if (!collector) {
        LOG_ERROR("Failed to allocate endorsement collector");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_endorsement_collector_create: collector is NULL");
        return NULL;
    }

    memcpy(&collector->config, config, sizeof(*config));
    collector->router = router;
    collector->enable_logging = config->enable_logging;

    collector->collection_capacity = config->max_concurrent;
    collector->collections = nimcp_calloc(
        collector->collection_capacity, sizeof(endorsement_collection_t)
    );
    if (!collector->collections) {
        nimcp_free(collector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_endorsement_collector_create: collector->collections is NULL");
        return NULL;
    }

    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    collector->mutex = nimcp_mutex_create(&attr);
    if (!collector->mutex) {
        nimcp_free(collector->collections);
        nimcp_free(collector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_endorsement_collector_create: collector->mutex is NULL");
        return NULL;
    }

    if (collector->enable_logging) {
        LOG_INFO("Created endorsement collector with pattern router");
    }

    return collector;
}

void mesh_endorsement_collector_destroy(mesh_endorsement_collector_t* collector) {
    if (!collector) return;

    if (collector->mutex) {
        nimcp_mutex_destroy(collector->mutex);
    }
    if (collector->collections) {
        /* Free all received endorsements arrays */
        for (size_t i = 0; i < collector->collection_count; i++) {
            if (collector->collections[i].received.endorsements) {
                nimcp_free(collector->collections[i].received.endorsements);
            }
        }
        nimcp_free(collector->collections);
    }
    nimcp_free(collector);
}

/* ============================================================================
 * Endorser Selection API (Pattern-Based)
 * ============================================================================ */

endorser_role_t mesh_endorsement_role_from_activation(float activation) {
    if (activation > MESH_REQUIRED_THRESHOLD) {
        return ENDORSER_ROLE_REQUIRED;
    } else if (activation > MESH_PREFERRED_THRESHOLD) {
        return ENDORSER_ROLE_PREFERRED;
    } else if (activation > MESH_OPTIONAL_THRESHOLD) {
        return ENDORSER_ROLE_OPTIONAL;
    } else if (activation < MESH_VETO_THRESHOLD) {
        return ENDORSER_ROLE_VETO;
    }
    return ENDORSER_ROLE_OPTIONAL;
}

nimcp_error_t mesh_endorsement_select_endorsers(
    mesh_endorsement_collector_t* collector,
    const mesh_pattern_t* pattern,
    endorser_set_t* endorsers_out
) {
    if (!collector || !pattern || !endorsers_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!collector->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_endorsement: not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    endorser_set_init(endorsers_out);

    /* Get activations from pattern router */
    mesh_activation_t activations[MESH_MAX_ENDORSERS];
    size_t activation_count = 0;

    mesh_pattern_transaction_t tx = {0};
    tx.content_pattern = *pattern;
    tx.urgency = pattern->magnitude;

    nimcp_error_t err = mesh_pattern_router_compute_activations(
        collector->router,
        &tx,
        activations,
        MESH_MAX_ENDORSERS,
        &activation_count
    );

    if (err != NIMCP_SUCCESS) {
        return err;
    }

    /* Convert activations to endorser set */
    for (size_t i = 0; i < activation_count && endorsers_out->count < MESH_MAX_ENDORSERS; i++) {
        if (!activations[i].should_endorse) continue;

        selected_endorser_t* endorser = &endorsers_out->endorsers[endorsers_out->count];
        endorser->id = activations[i].module_id;
        endorser->activation = activations[i].activation_level;
        endorser->similarity = activations[i].pattern_similarity;
        endorser->role = mesh_endorsement_role_from_activation(endorser->activation);

        switch (endorser->role) {
        case ENDORSER_ROLE_REQUIRED:
            endorsers_out->required_count++;
            break;
        case ENDORSER_ROLE_PREFERRED:
            endorsers_out->preferred_count++;
            break;
        case ENDORSER_ROLE_OPTIONAL:
            endorsers_out->optional_count++;
            break;
        case ENDORSER_ROLE_VETO:
            endorsers_out->veto_count++;
            break;
        }

        endorsers_out->count++;
    }

    if (collector->enable_logging) {
        LOG_DEBUG("Selected %zu endorsers: %zu required, %zu preferred, %zu optional, %zu veto",
            endorsers_out->count,
            endorsers_out->required_count,
            endorsers_out->preferred_count,
            endorsers_out->optional_count,
            endorsers_out->veto_count);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Collection API
 * ============================================================================ */

nimcp_error_t mesh_endorsement_start_collection(
    mesh_endorsement_collector_t* collector,
    mesh_transaction_t* tx,
    const mesh_pattern_t* pattern,
    const endorsement_quorum_t* quorum
) {
    if (!collector || !tx || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(collector->mutex);

    /* Check if already collecting */
    if (find_collection(collector, &tx->id)) {
        nimcp_mutex_unlock(collector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_ALREADY_EXISTS, "mesh_endorsement: error condition");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    /* Allocate collection */
    endorsement_collection_t* coll = allocate_collection(collector);
    if (!coll) {
        nimcp_mutex_unlock(collector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_endorsement: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    memset(coll, 0, sizeof(*coll));
    coll->tx_id = tx->id;
    coll->pattern = *pattern;
    coll->start_time_ns = nimcp_time_now_ns();

    /* Use collector's default timeout (endorsement_quorum_t has no timeout_ms field) */
    float timeout_ms = collector->config.default_timeout_ms;
    coll->deadline_ns = coll->start_time_ns + (uint64_t)(timeout_ms * 1000000.0);

    /* Allocate received endorsements array */
    coll->received.endorsements = nimcp_calloc(MESH_MAX_ENDORSERS, sizeof(mesh_endorsement_t));
    if (!coll->received.endorsements) {
        collector->collection_count--;
        nimcp_mutex_unlock(collector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_endorsement: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }
    coll->received.capacity = MESH_MAX_ENDORSERS;
    coll->received.count = 0;

    /* Select endorsers using pattern routing */
    nimcp_error_t err = mesh_endorsement_select_endorsers(
        collector, pattern, &coll->selected
    );

    if (err != NIMCP_SUCCESS) {
        /* P2-148: Free allocated endorsements array on failure to prevent leak */
        nimcp_free(coll->received.endorsements);
        coll->received.endorsements = NULL;
        coll->received.capacity = 0;
        collector->collection_count--;
        nimcp_mutex_unlock(collector->mutex);
        return err;
    }

    nimcp_mutex_unlock(collector->mutex);

    if (collector->enable_logging) {
        LOG_INFO("Started collection for tx %lu:%u with %zu endorsers",
            (unsigned long)tx->id.sequence, tx->id.channel, coll->selected.count);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_endorsement_add(
    mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id,
    const mesh_endorsement_t* endorsement
) {
    if (!collector || !tx_id || !endorsement) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(collector->mutex);

    endorsement_collection_t* coll = find_collection(collector, tx_id);
    if (!coll) {
        nimcp_mutex_unlock(collector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_endorsement: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (coll->collection_complete) {
        nimcp_mutex_unlock(collector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "mesh_endorsement: invalid state");
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Add to received endorsements */
    if (coll->received.count >= MESH_MAX_ENDORSERS) {
        nimcp_mutex_unlock(collector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_endorsement: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    coll->received.endorsements[coll->received.count++] = *endorsement;

    /* Check quorum */
    const endorsement_quorum_t* quorum = &collector->config.default_quorum;
    check_quorum(coll, quorum);

    nimcp_mutex_unlock(collector->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_endorsement_request(
    mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id,
    mesh_participant_id_t endorser_id
) {
    if (!collector || !tx_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(collector->mutex);

    endorsement_collection_t* coll = find_collection(collector, tx_id);
    if (!coll) {
        nimcp_mutex_unlock(collector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_endorsement: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    nimcp_mutex_unlock(collector->mutex);

    /* In a real implementation, this would send a request to the endorser */
    if (collector->enable_logging) {
        LOG_DEBUG("Requested endorsement from 0x%016lx for tx %lu",
            (unsigned long)endorser_id, (unsigned long)tx_id->sequence);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_endorsement_request_all(
    mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
) {
    if (!collector || !tx_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(collector->mutex);

    endorsement_collection_t* coll = find_collection(collector, tx_id);
    if (!coll) {
        nimcp_mutex_unlock(collector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_endorsement: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Request from all selected endorsers */
    for (size_t i = 0; i < coll->selected.count; i++) {
        /* In a real implementation, this would send requests */
        if (collector->enable_logging) {
            LOG_DEBUG("Requesting endorsement from 0x%016lx",
                (unsigned long)coll->selected.endorsers[i].id);
        }
    }

    nimcp_mutex_unlock(collector->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Collection Status API
 * ============================================================================ */

bool mesh_endorsement_is_complete(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
) {
    if (!collector || !tx_id) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)((mesh_endorsement_collector_t*)collector)->mutex);

    const endorsement_collection_t* coll = find_collection(
        (mesh_endorsement_collector_t*)collector, tx_id
    );
    bool result = coll ? coll->collection_complete : false;

    nimcp_mutex_unlock((nimcp_mutex_t*)((mesh_endorsement_collector_t*)collector)->mutex);

    return result;
}

bool mesh_endorsement_quorum_met(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
) {
    if (!collector || !tx_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement_quorum_met: required parameter is NULL (collector, tx_id)");
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)((mesh_endorsement_collector_t*)collector)->mutex);

    const endorsement_collection_t* coll = find_collection(
        (mesh_endorsement_collector_t*)collector, tx_id
    );
    bool result = coll ? coll->quorum_met : false;

    nimcp_mutex_unlock((nimcp_mutex_t*)((mesh_endorsement_collector_t*)collector)->mutex);

    return result;
}

bool mesh_endorsement_is_vetoed(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
) {
    if (!collector || !tx_id) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)((mesh_endorsement_collector_t*)collector)->mutex);

    const endorsement_collection_t* coll = find_collection(
        (mesh_endorsement_collector_t*)collector, tx_id
    );
    bool result = coll ? coll->vetoed : false;

    nimcp_mutex_unlock((nimcp_mutex_t*)((mesh_endorsement_collector_t*)collector)->mutex);

    return result;
}

const endorsement_set_t* mesh_endorsement_get_collected(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
) {
    if (!collector || !tx_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement_get_collected: required parameter is NULL (collector, tx_id)");
        return NULL;
    }

    /* P2-149: Mutex protection for collection access */
    nimcp_mutex_lock(((mesh_endorsement_collector_t*)collector)->mutex);

    const endorsement_collection_t* coll = find_collection(
        (mesh_endorsement_collector_t*)collector, tx_id
    );

    if (!coll) {
        nimcp_mutex_unlock(((mesh_endorsement_collector_t*)collector)->mutex);
        return NULL;
    }

    /* P1 fix: Copy the data before releasing mutex to prevent dangling pointer.
     * Caller receives a heap-allocated snapshot; caller must free when done. */
    endorsement_set_t* copy = nimcp_malloc(sizeof(endorsement_set_t));
    if (!copy) {
        nimcp_mutex_unlock(((mesh_endorsement_collector_t*)collector)->mutex);
        return NULL;
    }
    *copy = coll->received;
    /* Deep-copy the endorsements array */
    if (coll->received.endorsements && coll->received.count > 0) {
        copy->endorsements = nimcp_malloc(coll->received.count * sizeof(mesh_endorsement_t));
        if (copy->endorsements) {
            memcpy(copy->endorsements, coll->received.endorsements,
                   coll->received.count * sizeof(mesh_endorsement_t));
        }
        copy->capacity = coll->received.count;
    } else {
        copy->endorsements = NULL;
    }

    nimcp_mutex_unlock(((mesh_endorsement_collector_t*)collector)->mutex);
    return copy;
}

const endorser_set_t* mesh_endorsement_get_selected(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
) {
    if (!collector || !tx_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement_get_selected: required parameter is NULL (collector, tx_id)");
        return NULL;
    }

    /* P2-149: Mutex protection for collection access */
    nimcp_mutex_lock(((mesh_endorsement_collector_t*)collector)->mutex);

    const endorsement_collection_t* coll = find_collection(
        (mesh_endorsement_collector_t*)collector, tx_id
    );

    if (!coll) {
        nimcp_mutex_unlock(((mesh_endorsement_collector_t*)collector)->mutex);
        return NULL;
    }

    /* P1 fix: Copy the data before releasing mutex to prevent dangling pointer.
     * endorser_set_t uses fixed-size arrays so a shallow copy is sufficient.
     * Caller receives a heap-allocated snapshot; caller must free when done. */
    endorser_set_t* copy = nimcp_malloc(sizeof(endorser_set_t));
    if (!copy) {
        nimcp_mutex_unlock(((mesh_endorsement_collector_t*)collector)->mutex);
        return NULL;
    }
    *copy = coll->selected;

    nimcp_mutex_unlock(((mesh_endorsement_collector_t*)collector)->mutex);
    return copy;
}

nimcp_error_t mesh_endorsement_cancel_collection(
    mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
) {
    if (!collector || !tx_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(collector->mutex);

    for (size_t i = 0; i < collector->collection_count; i++) {
        if (collector->collections[i].tx_id.sequence == tx_id->sequence &&
            collector->collections[i].tx_id.channel == tx_id->channel) {
            /* Free received endorsements array */
            if (collector->collections[i].received.endorsements) {
                nimcp_free(collector->collections[i].received.endorsements);
            }
            /* Remove by swapping with last */
            if (i < collector->collection_count - 1) {
                collector->collections[i] =
                    collector->collections[collector->collection_count - 1];
            }
            collector->collection_count--;
            nimcp_mutex_unlock(collector->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(collector->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_endorsement: error condition");
    return NIMCP_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

nimcp_error_t mesh_endorsement_collector_update(
    mesh_endorsement_collector_t* collector,
    uint64_t delta_ms
) {
    if (!collector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    (void)delta_ms;
    uint64_t now_ns = nimcp_time_now_ns();

    nimcp_mutex_lock(collector->mutex);

    for (size_t i = 0; i < collector->collection_count; i++) {
        endorsement_collection_t* coll = &collector->collections[i];

        if (coll->collection_complete) continue;

        /* Check timeout */
        if (now_ns >= coll->deadline_ns) {
            coll->timed_out = true;
            coll->collection_complete = true;

            if (collector->enable_logging) {
                LOG_WARN("Collection for tx %lu timed out",
                    (unsigned long)coll->tx_id.sequence);
            }
        }
    }

    nimcp_mutex_unlock(collector->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

nimcp_error_t mesh_endorsement_create(
    mesh_participant_id_t endorser_id,
    endorsement_result_t result,
    mesh_endorsement_t* endorsement_out
) {
    if (!endorsement_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(endorsement_out, 0, sizeof(*endorsement_out));
    endorsement_out->endorser_id = endorser_id;
    endorsement_out->result = result;
    endorsement_out->timestamp_ns = nimcp_time_now_ns();

    return NIMCP_SUCCESS;
}

bool mesh_endorsement_verify_signature(
    const mesh_endorsement_t* endorsement,
    const mesh_transaction_t* tx,
    mesh_participant_registry_t* registry
) {
    /* Simple verification - in production would verify crypto signature */
    if (!endorsement || !tx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_endorsement_verify_signature: required parameter is NULL (endorsement, tx)");
        return false;
    }
    (void)registry;
    return endorsement->endorser_id != 0;
}

void endorser_set_init(endorser_set_t* set) {
    if (!set) return;
    memset(set, 0, sizeof(*set));
}

const char* endorser_role_to_string(endorser_role_t role) {
    switch (role) {
    case ENDORSER_ROLE_REQUIRED:  return "REQUIRED";
    case ENDORSER_ROLE_PREFERRED: return "PREFERRED";
    case ENDORSER_ROLE_OPTIONAL:  return "OPTIONAL";
    case ENDORSER_ROLE_VETO:      return "VETO";
    default:                      return "UNKNOWN";
    }
}

void mesh_endorsement_print_selected(const endorser_set_t* selected) {
    if (!selected) {
        printf("Endorser Set: NULL\n");
        return;
    }

    printf("Endorser Set: %zu endorsers\n", selected->count);
    printf("  Required:  %zu\n", selected->required_count);
    printf("  Preferred: %zu\n", selected->preferred_count);
    printf("  Optional:  %zu\n", selected->optional_count);
    printf("  Veto:      %zu\n", selected->veto_count);

    for (size_t i = 0; i < selected->count; i++) {
        const selected_endorser_t* e = &selected->endorsers[i];
        printf("  [%zu] ID=0x%016lx Role=%s Activation=%.2f Similarity=%.2f\n",
            i, (unsigned long)e->id, endorser_role_to_string(e->role),
            e->activation, e->similarity);
    }
}

void mesh_endorsement_print_collection(
    const mesh_endorsement_collector_t* collector,
    const mesh_tx_id_t* tx_id
) {
    if (!collector || !tx_id) {
        printf("Collection: NULL\n");
        return;
    }

    /* P1: Mutex protection for collection access */
    nimcp_mutex_lock(((mesh_endorsement_collector_t*)collector)->mutex);

    const endorsement_collection_t* coll = find_collection(
        (mesh_endorsement_collector_t*)collector, tx_id
    );

    if (!coll) {
        nimcp_mutex_unlock(((mesh_endorsement_collector_t*)collector)->mutex);
        printf("Collection: Not found for tx %lu\n",
            (unsigned long)tx_id->sequence);
        return;
    }

    printf("Collection for tx %lu:%u\n",
        (unsigned long)coll->tx_id.sequence, coll->tx_id.channel);
    printf("  Selected: %zu endorsers\n", coll->selected.count);
    printf("  Received: %zu endorsements\n", coll->received.count);
    printf("  Quorum:   %s\n", coll->quorum_met ? "MET" : "NOT MET");
    printf("  Complete: %s\n", coll->collection_complete ? "YES" : "NO");
    printf("  Vetoed:   %s\n", coll->vetoed ? "YES" : "NO");
    printf("  Timed out: %s\n", coll->timed_out ? "YES" : "NO");

    nimcp_mutex_unlock(((mesh_endorsement_collector_t*)collector)->mutex);
}
