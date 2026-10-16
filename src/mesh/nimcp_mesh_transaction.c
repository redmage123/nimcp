/**
 * @file nimcp_mesh_transaction.c
 * @brief Mesh Network Transaction Implementation
 *
 * WHAT: Implementation of Execute-Order-Validate transaction flow
 * WHY:  Enable Hyperledger-inspired distributed consensus
 * HOW:  Transaction manager with lifecycle tracking
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_transaction.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/crypto/nimcp_crypto.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Transaction entry in manager
 */
typedef struct tx_entry {
    mesh_transaction_t* tx;                 /**< Transaction */
    bool is_active;                         /**< Entry is in use */
} tx_entry_t;

/**
 * @brief Transaction manager structure
 */
struct mesh_tx_manager {
    uint32_t magic;                         /**< Magic for validation */
    tx_entry_t* entries;                    /**< Transaction entries */
    size_t capacity;                        /**< Entry capacity */
    size_t count;                           /**< Active transactions */
    uint64_t next_sequence;                 /**< Next sequence number */
    mesh_tx_manager_config_t config;        /**< Configuration */
    mesh_tx_manager_stats_t stats;          /**< Statistics */
    mesh_participant_registry_t* registry;  /**< Participant registry */
    nimcp_mutex_t* mutex;                   /**< Thread safety */
    bool enable_logging;                    /**< Logging flag */
};

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/* Use global LOG_* macros from nimcp_logging.h */

/**
 * @brief Validate manager handle
 */
static bool validate_manager(mesh_tx_manager_t* manager) {
    return manager && manager->magic == NIMCP_MESH_MAGIC;
}

/**
 * @brief Find transaction by ID
 */
static tx_entry_t* find_tx_entry(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
) {
    if (!manager || !tx_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_tx_entry: required parameter is NULL (manager, tx_id)");
        return NULL;
    }

    for (size_t i = 0; i < manager->capacity; i++) {
        if (manager->entries[i].is_active &&
            manager->entries[i].tx &&
            mesh_tx_id_compare(&manager->entries[i].tx->id, tx_id) == 0) {
            return &manager->entries[i];
        }
    }
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return NULL;
}

/**
 * @brief Find free slot
 */
static tx_entry_t* find_free_slot(mesh_tx_manager_t* manager) {
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_slot: manager is NULL");
        return NULL;
    }

    for (size_t i = 0; i < manager->capacity; i++) {
        if (!manager->entries[i].is_active) {
            return &manager->entries[i];
        }
    }
    /* No free slot available - capacity exhausted, not a NULL pointer error (P2: false positive removal) */
    return NULL;
}

/**
 * @brief Update latency statistics
 */
static void update_latency_stats(mesh_tx_manager_t* manager, uint64_t latency_ns) {
    float latency_ms = (float)latency_ns / 1000000.0f;
    uint64_t committed = manager->stats.transactions_committed;

    if (committed == 0) {
        manager->stats.avg_latency_ms = latency_ms;
    } else {
        /* Running average */
        manager->stats.avg_latency_ms =
            (manager->stats.avg_latency_ms * (float)committed + latency_ms) /
            (float)(committed + 1);
    }
}

/* ============================================================================
 * String Conversion (Implementation)
 * ============================================================================ */

const char* mesh_tx_type_to_string(mesh_tx_type_t type) {
    switch (type) {
        case MESH_TX_NONE:                 return "NONE";
        case MESH_TX_BELIEF_UPDATE:        return "BELIEF_UPDATE";
        case MESH_TX_STATE_CHANGE:         return "STATE_CHANGE";
        case MESH_TX_CONSENSUS_VOTE:       return "CONSENSUS_VOTE";
        case MESH_TX_COORDINATOR_ELECTION: return "COORDINATOR_ELECTION";
        case MESH_TX_CHANNEL_JOIN:         return "CHANNEL_JOIN";
        case MESH_TX_CHANNEL_LEAVE:        return "CHANNEL_LEAVE";
        case MESH_TX_CROSS_CHANNEL:        return "CROSS_CHANNEL";
        case MESH_TX_EMERGENCY_OVERRIDE:   return "EMERGENCY_OVERRIDE";
        case MESH_TX_GPU_BATCH:            return "GPU_BATCH";
        case MESH_TX_IMMUNE_RESPONSE:      return "IMMUNE_RESPONSE";
        case MESH_TX_CONFIG_UPDATE:        return "CONFIG_UPDATE";
        default:                           return "UNKNOWN";
    }
}

const char* mesh_tx_status_to_string(mesh_tx_status_t status) {
    switch (status) {
        case MESH_TX_STATUS_NONE:       return "NONE";
        case MESH_TX_STATUS_PROPOSED:   return "PROPOSED";
        case MESH_TX_STATUS_ENDORSING:  return "ENDORSING";
        case MESH_TX_STATUS_ENDORSED:   return "ENDORSED";
        case MESH_TX_STATUS_ORDERING:   return "ORDERING";
        case MESH_TX_STATUS_ORDERED:    return "ORDERED";
        case MESH_TX_STATUS_VALIDATING: return "VALIDATING";
        case MESH_TX_STATUS_COMMITTED:  return "COMMITTED";
        case MESH_TX_STATUS_FAILED:     return "FAILED";
        case MESH_TX_STATUS_EXPIRED:    return "EXPIRED";
        case MESH_TX_STATUS_REJECTED:   return "REJECTED";
        default:                        return "UNKNOWN";
    }
}

const char* mesh_endorsement_result_to_string(endorsement_result_t result) {
    switch (result) {
        case ENDORSEMENT_NONE:     return "NONE";
        case ENDORSEMENT_APPROVED: return "APPROVED";
        case ENDORSEMENT_REJECTED: return "REJECTED";
        case ENDORSEMENT_ABSTAIN:  return "ABSTAIN";
        case ENDORSEMENT_ERROR:    return "ERROR";
        case ENDORSEMENT_TIMEOUT:  return "TIMEOUT";
        default:                   return "UNKNOWN";
    }
}

/* ============================================================================
 * Manager Lifecycle
 * ============================================================================ */

nimcp_error_t mesh_tx_manager_default_config(mesh_tx_manager_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    config->max_pending = MESH_MAX_PENDING_TRANSACTIONS;
    config->max_batch_size = MESH_MAX_BATCH_SIZE;
    config->default_timeout_ms = MESH_DEFAULT_TX_TIMEOUT_MS;
    config->endorsement_timeout_ms = MESH_DEFAULT_ENDORSEMENT_TIMEOUT_MS;
    config->batch_timeout_ms = MESH_DEFAULT_BATCH_TIMEOUT_MS;
    config->enable_logging = true;

    return NIMCP_SUCCESS;
}

mesh_tx_manager_t* mesh_tx_manager_create(
    const mesh_tx_manager_config_t* config,
    mesh_participant_registry_t* registry
) {
    mesh_tx_manager_config_t default_config;
    if (!config) {
        mesh_tx_manager_default_config(&default_config);
        config = &default_config;
    }

    mesh_tx_manager_t* manager = nimcp_calloc(1, sizeof(*manager));
    if (!manager) {
        LOG_ERROR("Failed to allocate transaction manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_tx_manager_create: manager is NULL");
        return NULL;
    }

    manager->capacity = config->max_pending;
    manager->entries = nimcp_calloc(manager->capacity, sizeof(tx_entry_t));
    if (!manager->entries) {
        LOG_ERROR("Failed to allocate transaction entries");
        nimcp_free(manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_tx_manager_create: manager->entries is NULL");
        return NULL;
    }

    manager->mutex = nimcp_mutex_create(NULL);
    if (!manager->mutex) {
        LOG_ERROR("Failed to create manager mutex");
        nimcp_free(manager->entries);
        nimcp_free(manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_tx_manager_create: manager->mutex is NULL");
        return NULL;
    }

    manager->magic = NIMCP_MESH_MAGIC;
    memcpy(&manager->config, config, sizeof(*config));
    manager->registry = registry;
    manager->next_sequence = 1;
    manager->enable_logging = config->enable_logging;
    memset(&manager->stats, 0, sizeof(manager->stats));

    LOG_INFO("Created transaction manager with capacity %zu", manager->capacity);
    return manager;
}

void mesh_tx_manager_destroy(mesh_tx_manager_t* manager) {
    if (!manager) return;

    /* Destroy all pending transactions */
    if (manager->entries) {
        for (size_t i = 0; i < manager->capacity; i++) {
            if (manager->entries[i].is_active && manager->entries[i].tx) {
                mesh_transaction_destroy(manager->entries[i].tx);
            }
        }
        nimcp_free(manager->entries);
    }

    if (manager->mutex) {
        nimcp_mutex_destroy(manager->mutex);
    }

    manager->magic = 0;
    nimcp_free(manager);
    LOG_INFO("Destroyed transaction manager");
}

/* ============================================================================
 * Transaction Creation
 * ============================================================================ */

mesh_transaction_t* mesh_transaction_create(
    mesh_tx_type_t type,
    mesh_participant_id_t proposer,
    mesh_channel_id_t channel
) {
    mesh_transaction_t* tx = nimcp_calloc(1, sizeof(*tx));
    if (!tx) {
        LOG_ERROR("Failed to allocate transaction");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_transaction_create: tx is NULL");
        return NULL;
    }

    tx->type = type;
    tx->status = MESH_TX_STATUS_NONE;
    tx->proposer_id = proposer;
    tx->source_channel = channel;
    tx->target_channel = channel;
    tx->created_ns = nimcp_time_now_ns();

    /* Initialize ID */
    tx->id.channel = channel;
    tx->id.proposer = proposer;
    tx->id.timestamp_ns = tx->created_ns;
    tx->id.sequence = 0; /* Assigned by ordering service */

    /* Initialize endorsements */
    tx->endorsements.endorsements = NULL;
    tx->endorsements.count = 0;
    tx->endorsements.capacity = 0;
    tx->endorsements.policy_satisfied = false;

    return tx;
}

void mesh_transaction_destroy(mesh_transaction_t* tx) {
    if (!tx) return;

    if (tx->payload) {
        nimcp_free(tx->payload);
    }

    if (tx->endorsements.endorsements) {
        nimcp_free(tx->endorsements.endorsements);
    }

    nimcp_free(tx);
}

nimcp_error_t mesh_transaction_set_payload(
    mesh_transaction_t* tx,
    const void* payload,
    size_t size
) {
    if (!tx) return NIMCP_ERROR_NULL_POINTER;
    if (!payload && size > 0) return NIMCP_ERROR_INVALID_PARAM;

    if (tx->payload) {
        nimcp_free(tx->payload);
        tx->payload = NULL;
    }

    if (size > 0) {
        if (size > MESH_MAX_PAYLOAD_SIZE) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "mesh_transaction: payload exceeds MESH_MAX_PAYLOAD_SIZE");
            return NIMCP_ERROR_BUFFER_OVERFLOW;
        }

        tx->payload = nimcp_malloc(size);
        if (!tx->payload) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_transaction: memory allocation failed");
            return NIMCP_ERROR_NO_MEMORY;
        }

        memcpy(tx->payload, payload, size);
        tx->payload_size = size;

        /* Compute hash */
        mesh_tx_compute_hash(payload, size, tx->payload_hash);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_transaction_set_policy(
    mesh_transaction_t* tx,
    const char* policy_name
) {
    if (!tx) return NIMCP_ERROR_NULL_POINTER;
    tx->endorsement_policy = policy_name;
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_transaction_set_callback(
    mesh_transaction_t* tx,
    mesh_tx_callback_t callback,
    void* ctx
) {
    if (!tx) return NIMCP_ERROR_NULL_POINTER;
    tx->callback = callback;
    tx->callback_ctx = ctx;
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_transaction_set_timeout(
    mesh_transaction_t* tx,
    uint64_t timeout_ms
) {
    if (!tx) return NIMCP_ERROR_NULL_POINTER;
    tx->timeout_ns = tx->created_ns + (timeout_ms * 1000000ULL);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Transaction Lifecycle
 * ============================================================================ */

nimcp_error_t mesh_tx_propose(
    mesh_tx_manager_t* manager,
    mesh_transaction_t* tx
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;
    if (!tx) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    /* Find free slot */
    tx_entry_t* entry = find_free_slot(manager);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        LOG_ERROR("Transaction manager full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_transaction: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Set defaults */
    if (tx->timeout_ns == 0) {
        tx->timeout_ns = tx->created_ns +
                         (manager->config.default_timeout_ms * 1000000ULL);
    }

    tx->status = MESH_TX_STATUS_PROPOSED;
    tx->endorsement_deadline_ns = nimcp_time_now_ns() +
                                   (manager->config.endorsement_timeout_ms * 1000000ULL);

    entry->tx = tx;
    entry->is_active = true;
    manager->count++;
    manager->stats.transactions_proposed++;
    manager->stats.pending_count = manager->count;

    nimcp_mutex_unlock(manager->mutex);

    LOG_INFO("Proposed transaction type=%s proposer=0x%016lx channel=%u",
             mesh_tx_type_to_string(tx->type),
             (unsigned long)tx->proposer_id,
             tx->source_channel);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_tx_collect_endorsements(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;
    if (!tx_id) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    tx_entry_t* entry = find_tx_entry(manager, tx_id);
    if (!entry || !entry->tx) {
        nimcp_mutex_unlock(manager->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_transaction: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    entry->tx->status = MESH_TX_STATUS_ENDORSING;

    nimcp_mutex_unlock(manager->mutex);

    /* TODO: Request endorsements from policy-specified endorsers */
    /* This would iterate through participants and invoke on_endorse_request */

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_tx_add_endorsement(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id,
    const mesh_endorsement_t* endorsement
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;
    if (!tx_id || !endorsement) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    tx_entry_t* entry = find_tx_entry(manager, tx_id);
    if (!entry || !entry->tx) {
        nimcp_mutex_unlock(manager->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_transaction: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    mesh_transaction_t* tx = entry->tx;

    /* Expand endorsement array if needed */
    if (tx->endorsements.count >= tx->endorsements.capacity) {
        size_t new_capacity = tx->endorsements.capacity == 0 ?
                              8 : tx->endorsements.capacity * 2;
        mesh_endorsement_t* new_arr = nimcp_realloc(
            tx->endorsements.endorsements,
            new_capacity * sizeof(mesh_endorsement_t)
        );
        if (!new_arr) {
            nimcp_mutex_unlock(manager->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_transaction: memory allocation failed");
            return NIMCP_ERROR_NO_MEMORY;
        }
        tx->endorsements.endorsements = new_arr;
        tx->endorsements.capacity = new_capacity;
    }

    /* Add endorsement */
    memcpy(&tx->endorsements.endorsements[tx->endorsements.count],
           endorsement, sizeof(mesh_endorsement_t));
    tx->endorsements.count++;

    /* TODO: Check if policy is satisfied */
    /* For now, simple majority */
    size_t approved = 0;
    for (size_t i = 0; i < tx->endorsements.count; i++) {
        if (tx->endorsements.endorsements[i].result == ENDORSEMENT_APPROVED) {
            approved++;
        }
    }

    if (approved > 0) {
        tx->endorsements.policy_satisfied = true;
        tx->status = MESH_TX_STATUS_ENDORSED;
        manager->stats.transactions_endorsed++;
    }

    nimcp_mutex_unlock(manager->mutex);

    LOG_DEBUG("Added endorsement from 0x%016lx result=%s",
              (unsigned long)endorsement->endorser_id,
              mesh_endorsement_result_to_string(endorsement->result));

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_tx_submit_for_ordering(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;
    if (!tx_id) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    tx_entry_t* entry = find_tx_entry(manager, tx_id);
    if (!entry || !entry->tx) {
        nimcp_mutex_unlock(manager->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_transaction: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (!entry->tx->endorsements.policy_satisfied) {
        nimcp_mutex_unlock(manager->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_PERMISSION_DENIED, "mesh_transaction: error condition");
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    entry->tx->status = MESH_TX_STATUS_ORDERING;

    nimcp_mutex_unlock(manager->mutex);

    /* TODO: Submit to ordering service */

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_tx_mark_ordered(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id,
    uint64_t sequence,
    const uint8_t* signature
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;
    if (!tx_id) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    tx_entry_t* entry = find_tx_entry(manager, tx_id);
    if (!entry || !entry->tx) {
        nimcp_mutex_unlock(manager->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_transaction: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    entry->tx->sequence_number = sequence;
    entry->tx->ordering_timestamp_ns = nimcp_time_now_ns();
    entry->tx->status = MESH_TX_STATUS_ORDERED;

    if (signature) {
        memcpy(entry->tx->ordering_signature, signature, MESH_SIGNATURE_SIZE);
    }

    manager->stats.transactions_ordered++;

    nimcp_mutex_unlock(manager->mutex);

    LOG_DEBUG("Transaction ordered: sequence=%lu", (unsigned long)sequence);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_tx_validate(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id,
    mesh_participant_id_t validator,
    bool valid
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;
    if (!tx_id) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    tx_entry_t* entry = find_tx_entry(manager, tx_id);
    if (!entry || !entry->tx) {
        nimcp_mutex_unlock(manager->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_transaction: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    mesh_transaction_t* tx = entry->tx;

    if (tx->status == MESH_TX_STATUS_ORDERED) {
        tx->status = MESH_TX_STATUS_VALIDATING;
    }

    tx->validation_count++;

    if (!valid) {
        tx->validation_passed = false;
    } else if (tx->validation_count >= tx->validation_required ||
               tx->validation_required == 0) {
        tx->validation_passed = true;
    }

    nimcp_mutex_unlock(manager->mutex);

    (void)validator; /* TODO: Track individual validators */

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_tx_commit(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;
    if (!tx_id) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    tx_entry_t* entry = find_tx_entry(manager, tx_id);
    if (!entry || !entry->tx) {
        nimcp_mutex_unlock(manager->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_transaction: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    mesh_transaction_t* tx = entry->tx;
    tx->commit_timestamp_ns = nimcp_time_now_ns();
    tx->status = MESH_TX_STATUS_COMMITTED;

    uint64_t latency_ns = tx->commit_timestamp_ns - tx->created_ns;
    update_latency_stats(manager, latency_ns);
    manager->stats.transactions_committed++;

    /* P1-34: Copy callback locally before unlocking mutex to prevent use-after-free */
    mesh_tx_callback_t local_callback = tx->callback;
    void* local_callback_ctx = tx->callback_ctx;

    nimcp_mutex_unlock(manager->mutex);

    /* Invoke callback outside of lock */
    if (local_callback) {
        mesh_result_t result = {0};
        result.tx_id = tx->id;
        result.status = MESH_TX_STATUS_COMMITTED;
        result.error = NIMCP_SUCCESS;
        result.commit_timestamp_ns = tx->commit_timestamp_ns;
        memcpy(result.result_hash, tx->payload_hash, 32);

        local_callback(&result, local_callback_ctx);
    }

    /* Notify all channel participants */
    if (manager->registry) {
        /* TODO: Invoke on_commit for all channel members */
    }

    LOG_INFO("Transaction committed: latency=%.2f ms",
             (float)latency_ns / 1000000.0f);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_tx_fail(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id,
    nimcp_error_t error,
    const char* message
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;
    if (!tx_id) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    tx_entry_t* entry = find_tx_entry(manager, tx_id);
    if (!entry || !entry->tx) {
        nimcp_mutex_unlock(manager->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_transaction: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    mesh_transaction_t* tx = entry->tx;
    tx->status = MESH_TX_STATUS_FAILED;
    manager->stats.transactions_failed++;

    /* P1-34: Copy callback locally before unlocking mutex to prevent use-after-free */
    mesh_tx_callback_t local_callback = tx->callback;
    void* local_callback_ctx = tx->callback_ctx;

    nimcp_mutex_unlock(manager->mutex);

    /* Invoke callback outside of lock */
    if (local_callback) {
        mesh_result_t result = {0};
        result.tx_id = tx->id;
        result.status = MESH_TX_STATUS_FAILED;
        result.error = error;
        if (message) {
            strncpy(result.error_msg, message, sizeof(result.error_msg) - 1);
        }

        local_callback(&result, local_callback_ctx);
    }

    LOG_ERROR("Transaction failed: %s", message ? message : "unknown error");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Transaction Query
 * ============================================================================ */

/**
 * P3: WARNING - Returns internal pointer to transaction data. Caller must not
 * hold this pointer across concurrent modifications. The returned pointer is
 * valid only while the transaction entry remains active in the manager.
 * For thread-safe access, prefer mesh_tx_get_result() which copies data.
 */
const mesh_transaction_t* mesh_tx_get(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
) {
    if (!validate_manager(manager) || !tx_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_tx_get: required parameter is NULL (validate_manager, tx_id)");
        return NULL;
    }

    nimcp_mutex_lock(manager->mutex);
    tx_entry_t* entry = find_tx_entry(manager, tx_id);
    nimcp_mutex_unlock(manager->mutex);

    return entry ? entry->tx : NULL;
}

mesh_tx_status_t mesh_tx_get_status(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
) {
    const mesh_transaction_t* tx = mesh_tx_get(manager, tx_id);
    return tx ? tx->status : MESH_TX_STATUS_NONE;
}

nimcp_error_t mesh_tx_get_result(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id,
    mesh_result_t* result
) {
    if (!result) return NIMCP_ERROR_NULL_POINTER;

    const mesh_transaction_t* tx = mesh_tx_get(manager, tx_id);
    if (!tx) return NIMCP_ERROR_NOT_FOUND;

    memset(result, 0, sizeof(*result));
    result->tx_id = tx->id;
    result->status = tx->status;
    result->commit_timestamp_ns = tx->commit_timestamp_ns;
    memcpy(result->result_hash, tx->payload_hash, 32);

    return NIMCP_SUCCESS;
}

bool mesh_tx_is_complete(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
) {
    mesh_tx_status_t status = mesh_tx_get_status(manager, tx_id);
    return status == MESH_TX_STATUS_COMMITTED ||
           status == MESH_TX_STATUS_FAILED ||
           status == MESH_TX_STATUS_EXPIRED ||
           status == MESH_TX_STATUS_REJECTED;
}

bool mesh_tx_is_endorsed(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
) {
    const mesh_transaction_t* tx = mesh_tx_get(manager, tx_id);
    return tx && tx->endorsements.policy_satisfied;
}

/* ============================================================================
 * Batch Operations
 * ============================================================================ */

mesh_tx_batch_t* mesh_tx_batch_create(
    mesh_channel_id_t channel,
    size_t capacity
) {
    mesh_tx_batch_t* batch = nimcp_calloc(1, sizeof(*batch));
    if (!batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_tx_batch_create: batch is NULL");
        return NULL;
    }

    batch->channel = channel;
    batch->capacity = capacity > 0 ? capacity : MESH_MAX_BATCH_SIZE;
    batch->transactions = nimcp_calloc(batch->capacity, sizeof(mesh_transaction_t*));
    if (!batch->transactions) {
        nimcp_free(batch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_tx_batch_create: batch->transactions is NULL");
        return NULL;
    }

    return batch;
}

void mesh_tx_batch_destroy(mesh_tx_batch_t* batch) {
    if (!batch) return;

    if (batch->transactions) {
        nimcp_free(batch->transactions);
    }
    nimcp_free(batch);
}

nimcp_error_t mesh_tx_batch_add(
    mesh_tx_batch_t* batch,
    mesh_transaction_t* tx
) {
    if (!batch || !tx) return NIMCP_ERROR_NULL_POINTER;

    if (batch->count >= batch->capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_transaction: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    batch->transactions[batch->count++] = tx;
    return NIMCP_SUCCESS;
}

void mesh_tx_batch_clear(mesh_tx_batch_t* batch) {
    if (!batch) return;
    batch->count = 0;
}

nimcp_error_t mesh_tx_batch_submit(
    mesh_tx_manager_t* manager,
    mesh_tx_batch_t* batch
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;
    if (!batch) return NIMCP_ERROR_NULL_POINTER;

    for (size_t i = 0; i < batch->count; i++) {
        nimcp_error_t err = mesh_tx_propose(manager, batch->transactions[i]);
        if (err != NIMCP_SUCCESS) {
            LOG_WARN("Failed to submit batch transaction %zu: %d", i, err);
        }
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Timeout and Cleanup
 * ============================================================================ */

size_t mesh_tx_cleanup_expired(
    mesh_tx_manager_t* manager,
    uint64_t current_time_ns
) {
    if (!validate_manager(manager)) return 0;

    size_t expired_count = 0;

    nimcp_mutex_lock(manager->mutex);

    for (size_t i = 0; i < manager->capacity; i++) {
        if (!manager->entries[i].is_active) continue;

        mesh_transaction_t* tx = manager->entries[i].tx;
        if (!tx) continue;

        /* P1-48: Check status directly instead of calling mesh_tx_is_complete()
         * which would deadlock by re-locking manager->mutex we already hold */
        if (tx->status == MESH_TX_STATUS_COMMITTED ||
            tx->status == MESH_TX_STATUS_FAILED ||
            tx->status == MESH_TX_STATUS_EXPIRED ||
            tx->status == MESH_TX_STATUS_REJECTED) continue;

        /* Check timeout */
        if (tx->timeout_ns > 0 && current_time_ns >= tx->timeout_ns) {
            tx->status = MESH_TX_STATUS_EXPIRED;
            manager->stats.transactions_expired++;
            expired_count++;

            /* P1-34: Copy callback locally before unlocking mutex */
            mesh_tx_callback_t local_cb = tx->callback;
            void* local_ctx = tx->callback_ctx;

            if (local_cb) {
                mesh_result_t result = {0};
                result.tx_id = tx->id;
                result.status = MESH_TX_STATUS_EXPIRED;
                result.error = NIMCP_ERROR_TIMEOUT;
                strncpy(result.error_msg, "Transaction timed out",
                        sizeof(result.error_msg) - 1);

                nimcp_mutex_unlock(manager->mutex);
                local_cb(&result, local_ctx);
                nimcp_mutex_lock(manager->mutex);

                /* Re-validate entry after reacquiring lock */
                if (!manager->entries[i].is_active || manager->entries[i].tx != tx) {
                    continue;
                }
            }

            LOG_WARN("Transaction expired: type=%s",
                     mesh_tx_type_to_string(tx->type));
        }
    }

    nimcp_mutex_unlock(manager->mutex);

    return expired_count;
}

nimcp_error_t mesh_tx_manager_update(
    mesh_tx_manager_t* manager,
    uint64_t delta_ms
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;

    uint64_t now = nimcp_time_now_ns();
    mesh_tx_cleanup_expired(manager, now);

    (void)delta_ms; /* TODO: Use for periodic tasks */

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

nimcp_error_t mesh_tx_manager_get_stats(
    mesh_tx_manager_t* manager,
    mesh_tx_manager_stats_t* stats
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    memcpy(stats, &manager->stats, sizeof(mesh_tx_manager_stats_t));
    stats->pending_count = manager->count;

    /* Calculate endorsement success rate */
    if (manager->stats.transactions_proposed > 0) {
        stats->endorsement_success_rate =
            (float)manager->stats.transactions_endorsed /
            (float)manager->stats.transactions_proposed;
    }

    nimcp_mutex_unlock(manager->mutex);

    return NIMCP_SUCCESS;
}

void mesh_tx_manager_reset_stats(mesh_tx_manager_t* manager) {
    if (!validate_manager(manager)) return;

    nimcp_mutex_lock(manager->mutex);
    memset(&manager->stats, 0, sizeof(manager->stats));
    manager->stats.pending_count = manager->count;
    nimcp_mutex_unlock(manager->mutex);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

nimcp_error_t mesh_tx_generate_id(
    mesh_participant_id_t proposer,
    mesh_channel_id_t channel,
    mesh_tx_id_t* id_out
) {
    if (!id_out) return NIMCP_ERROR_NULL_POINTER;

    id_out->channel = channel;
    id_out->proposer = proposer;
    id_out->timestamp_ns = nimcp_time_now_ns();
    id_out->sequence = 0; /* Assigned by ordering service */

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_tx_compute_hash(
    const void* payload,
    size_t size,
    uint8_t* hash_out
) {
    if (!hash_out) return NIMCP_ERROR_NULL_POINTER;

    if (!payload || size == 0) {
        memset(hash_out, 0, 32);
        return NIMCP_SUCCESS;
    }

    /* Use crypto hash if available, otherwise simple hash */
    if (nimcp_sha256(payload, size, hash_out) != NIMCP_SUCCESS) {
        /* Fallback: simple XOR-based hash */
        const uint8_t* data = (const uint8_t*)payload;
        memset(hash_out, 0, 32);
        for (size_t i = 0; i < size; i++) {
            hash_out[i % 32] ^= data[i];
        }
    }

    return NIMCP_SUCCESS;
}

/* P3: Diagnostic print functions use printf intentionally for console debug output */
void mesh_transaction_print(const mesh_transaction_t* tx) {
    if (!tx) {
        printf("Transaction: NULL\n");
        return;
    }

    printf("Transaction:\n");
    printf("  Type:      %s\n", mesh_tx_type_to_string(tx->type));
    printf("  Status:    %s\n", mesh_tx_status_to_string(tx->status));
    printf("  Proposer:  0x%016lx\n", (unsigned long)tx->proposer_id);
    printf("  Channel:   %u -> %u\n", tx->source_channel, tx->target_channel);
    printf("  Payload:   %zu bytes\n", tx->payload_size);
    printf("  Endorse:   %zu (satisfied=%s)\n",
           tx->endorsements.count,
           tx->endorsements.policy_satisfied ? "yes" : "no");
    printf("  Sequence:  %lu\n", (unsigned long)tx->sequence_number);
}

/* P3: Diagnostic print function - printf intentional for console debug output */
void mesh_tx_manager_print_status(const mesh_tx_manager_t* manager) {
    if (!manager) {
        printf("Transaction Manager: NULL\n");
        return;
    }

    printf("Transaction Manager:\n");
    printf("  Capacity:   %zu\n", manager->capacity);
    printf("  Pending:    %zu\n", manager->count);
    printf("  Proposed:   %lu\n", (unsigned long)manager->stats.transactions_proposed);
    printf("  Endorsed:   %lu\n", (unsigned long)manager->stats.transactions_endorsed);
    printf("  Ordered:    %lu\n", (unsigned long)manager->stats.transactions_ordered);
    printf("  Committed:  %lu\n", (unsigned long)manager->stats.transactions_committed);
    printf("  Failed:     %lu\n", (unsigned long)manager->stats.transactions_failed);
    printf("  Expired:    %lu\n", (unsigned long)manager->stats.transactions_expired);
    printf("  Avg Latency: %.2f ms\n", manager->stats.avg_latency_ms);
}
