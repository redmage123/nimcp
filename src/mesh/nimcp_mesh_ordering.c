/**
 * @file nimcp_mesh_ordering.c
 * @brief Mesh Network Ordering Service - Raft-based Transaction Sequencing
 *
 * Implementation of the ordering service using Raft consensus for
 * transaction sequencing, batching, and block creation.
 */

#include "mesh/nimcp_mesh_ordering.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Pending transaction in queue
 */
typedef struct pending_tx {
    mesh_tx_id_t id;                    /**< Transaction ID */
    mesh_transaction_t* tx;             /**< Transaction pointer */
    uint64_t submit_time_ns;            /**< When submitted */
    struct pending_tx* next;            /**< Next in queue */
} pending_tx_t;

/**
 * @brief Batch being assembled
 */
typedef struct tx_batch_assembly {
    mesh_tx_id_t* tx_ids;               /**< Transaction IDs */
    mesh_transaction_t** transactions;   /**< Transaction pointers */
    size_t count;                        /**< Current count */
    size_t capacity;                     /**< Capacity */
    uint64_t start_time_ns;              /**< When batch started */
} tx_batch_assembly_t;

/**
 * @brief Ordering service structure
 */
struct mesh_ordering_service {
    /* Configuration */
    char* name;
    mesh_ordering_config_t config;

    /* Orderer pool */
    mesh_coordinator_pool_t* orderer_pool;

    /* Transaction queue */
    pending_tx_t* pending_head;
    pending_tx_t* pending_tail;
    size_t pending_count;

    /* Batch assembly */
    tx_batch_assembly_t* current_batch;

    /* Raft state */
    raft_state_t raft;

    /* Blocks */
    mesh_ordered_block_t** blocks;
    size_t block_count;
    size_t block_capacity;
    uint64_t current_block_number;
    uint64_t current_sequence;

    /* Channels */
    mesh_channel_id_t* channels;
    size_t channel_count;
    size_t channel_capacity;

    /* Statistics */
    mesh_ordering_stats_t stats;

    /* Timing */
    uint64_t last_batch_time_ns;
    uint64_t last_heartbeat_time_ns;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Logging */
    nimcp_logger_t logger;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_time_ns(void) {
    return nimcp_time_now_ns();
}

static _Atomic uint64_t s_ordering_prng_state = 0;

static uint32_t ordering_thread_safe_rand(void) {
    /* P2-143: Use CAS loop instead of atomic_load/atomic_store to prevent TOCTOU race */
    uint64_t old_state, new_state;
    do {
        old_state = atomic_load(&s_ordering_prng_state);
        if (old_state == 0) {
            /* Seed the PRNG - CAS 0 -> seed value, then loop will read the seed */
            uint64_t seed = (uint64_t)nimcp_time_now_ns();
            if (seed == 0) seed = 1;
            atomic_compare_exchange_strong(&s_ordering_prng_state, &old_state, seed);
            continue;  /* Re-read the (now-seeded) state */
        }
        /* xorshift64 */
        new_state = old_state;
        new_state ^= new_state << 13;
        new_state ^= new_state >> 7;
        new_state ^= new_state << 17;
    } while (!atomic_compare_exchange_weak(&s_ordering_prng_state, &old_state, new_state));
    return (uint32_t)(new_state & 0x7FFFFFFF);
}

static void generate_random_timeout(mesh_ordering_service_t* service) {
    /* Random timeout between 1x and 2x the configured timeout */
    float base = service->config.election_timeout_ms;
    float jitter = base * ((float)ordering_thread_safe_rand() / (float)0x7FFFFFFF);
    service->raft.election_timeout_ns = (uint64_t)((base + jitter) * 1000000.0f);
}

static bool is_log_up_to_date(const raft_state_t* raft,
                              uint64_t candidate_last_index,
                              uint64_t candidate_last_term) {
    if (raft->log_size == 0) {
        return true;
    }

    uint64_t our_last_term = raft->log[raft->log_size - 1].term;
    uint64_t our_last_index = raft->log[raft->log_size - 1].index;

    if (candidate_last_term != our_last_term) {
        return candidate_last_term > our_last_term;
    }
    return candidate_last_index >= our_last_index;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

nimcp_error_t mesh_ordering_default_config(mesh_ordering_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering_default_config: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(config, 0, sizeof(*config));

    config->service_name = "ordering_service";
    config->batch_size = MESH_DEFAULT_BATCH_SIZE;
    config->batch_timeout_ms = MESH_DEFAULT_ORDERING_BATCH_TIMEOUT;
    config->max_pending = MESH_MAX_ORDERING_QUEUE;
    config->heartbeat_interval_ms = MESH_RAFT_HEARTBEAT_INTERVAL_MS;
    config->election_timeout_ms = MESH_RAFT_ELECTION_TIMEOUT_MS;
    config->max_log_entries = MESH_MAX_LOG_ENTRIES;
    config->enable_logging = true;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

mesh_ordering_service_t* mesh_ordering_create(
    const mesh_ordering_config_t* config,
    mesh_coordinator_pool_t* orderer_pool
) {
    mesh_ordering_service_t* service = nimcp_calloc(1, sizeof(mesh_ordering_service_t));
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering_create: service is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        service->config = *config;
    } else {
        mesh_ordering_default_config(&service->config);
    }

    /* Copy name */
    if (service->config.service_name) {
        service->name = nimcp_strdup(service->config.service_name);
    } else {
        service->name = nimcp_strdup("ordering_service");
    }

    service->orderer_pool = orderer_pool;

    /* Initialize Raft state */
    service->raft.role = RAFT_ROLE_FOLLOWER;
    service->raft.current_term = 0;
    service->raft.voted_for = 0;
    service->raft.commit_index = 0;
    service->raft.last_applied = 0;

    /* Allocate Raft log */
    service->raft.log_capacity = 1024;
    service->raft.log = nimcp_calloc(service->raft.log_capacity, sizeof(raft_log_entry_t));
    if (!service->raft.log) {
        nimcp_free(service->name);
        nimcp_free(service);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering_create: service->raft is NULL");
        return NULL;
    }

    /* Allocate batch assembly */
    service->current_batch = nimcp_calloc(1, sizeof(tx_batch_assembly_t));
    if (!service->current_batch) {
        nimcp_free(service->raft.log);
        nimcp_free(service->name);
        nimcp_free(service);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering_create: service->current_batch is NULL");
        return NULL;
    }
    service->current_batch->capacity = service->config.batch_size;
    service->current_batch->tx_ids = nimcp_calloc(service->current_batch->capacity, sizeof(mesh_tx_id_t));
    if (!service->current_batch->tx_ids) {
        nimcp_free(service->current_batch);
        nimcp_free(service->raft.log);
        nimcp_free(service->name);
        nimcp_free(service);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering_create: batch tx_ids alloc failed");
        return NULL;
    }
    service->current_batch->transactions = nimcp_calloc(service->current_batch->capacity, sizeof(mesh_transaction_t*));
    if (!service->current_batch->transactions) {
        nimcp_free(service->current_batch->tx_ids);
        nimcp_free(service->current_batch);
        nimcp_free(service->raft.log);
        nimcp_free(service->name);
        nimcp_free(service);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering_create: batch transactions alloc failed");
        return NULL;
    }

    /* Allocate blocks array */
    service->block_capacity = 1024;
    service->blocks = nimcp_calloc(service->block_capacity, sizeof(mesh_ordered_block_t*));
    if (!service->blocks) {
        nimcp_free(service->current_batch->transactions);
        nimcp_free(service->current_batch->tx_ids);
        nimcp_free(service->current_batch);
        nimcp_free(service->raft.log);
        nimcp_free(service->name);
        nimcp_free(service);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering_create: blocks alloc failed");
        return NULL;
    }

    /* Allocate channels array */
    service->channel_capacity = MESH_MAX_CHANNELS;
    service->channels = nimcp_calloc(service->channel_capacity, sizeof(mesh_channel_id_t));
    if (!service->channels) {
        nimcp_free(service->blocks);
        nimcp_free(service->current_batch->transactions);
        nimcp_free(service->current_batch->tx_ids);
        nimcp_free(service->current_batch);
        nimcp_free(service->raft.log);
        nimcp_free(service->name);
        nimcp_free(service);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering_create: channels alloc failed");
        return NULL;
    }

    /* Create mutex for thread safety (P1-26) */
    service->mutex = nimcp_mutex_create(NULL);
    if (!service->mutex) {
        nimcp_free(service->channels);
        nimcp_free(service->blocks);
        nimcp_free(service->current_batch->transactions);
        nimcp_free(service->current_batch->tx_ids);
        nimcp_free(service->current_batch);
        nimcp_free(service->raft.log);
        nimcp_free(service->name);
        nimcp_free(service);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering_create: mutex creation failed");
        return NULL;
    }

    /* Copy channels from config - clamp to capacity */
    if (config && config->channels && config->channel_count > 0) {
        size_t count = config->channel_count;
        if (count > service->channel_capacity) {
            count = service->channel_capacity;
        }
        for (size_t i = 0; i < count; i++) {
            service->channels[i] = config->channels[i];
        }
        service->channel_count = count;
    }

    /* Initialize timing */
    uint64_t now = get_time_ns();
    service->last_batch_time_ns = now;
    service->last_heartbeat_time_ns = now;
    service->raft.last_heartbeat_ns = now;
    generate_random_timeout(service);

    /* Initialize logger */
    if (service->config.enable_logging) {
        service->logger = nimcp_logger_get("mesh.ordering");
    }

    return service;
}

void mesh_ordering_destroy(mesh_ordering_service_t* service) {
    if (!service) {
        return;
    }

    /* Free pending queue */
    pending_tx_t* pending = service->pending_head;
    while (pending) {
        pending_tx_t* next = pending->next;
        nimcp_free(pending);
        pending = next;
    }

    /* Free batch assembly */
    if (service->current_batch) {
        nimcp_free(service->current_batch->tx_ids);
        nimcp_free(service->current_batch->transactions);
        nimcp_free(service->current_batch);
    }

    /* Free Raft log */
    if (service->raft.log) {
        for (size_t i = 0; i < service->raft.log_size; i++) {
            nimcp_free(service->raft.log[i].tx_ids);
        }
        nimcp_free(service->raft.log);
    }
    nimcp_free(service->raft.next_index);
    nimcp_free(service->raft.match_index);

    /* Free blocks */
    if (service->blocks) {
        for (size_t i = 0; i < service->block_count; i++) {
            mesh_ordered_block_destroy(service->blocks[i]);
        }
        nimcp_free(service->blocks);
    }

    /* Free channels */
    nimcp_free(service->channels);

    /* Destroy mutex */
    if (service->mutex) {
        nimcp_mutex_destroy(service->mutex);
    }

    nimcp_free(service->name);
    nimcp_free(service);
}

const char* mesh_ordering_get_name(const mesh_ordering_service_t* service) {
    return service ? service->name : NULL;
}

/* ============================================================================
 * Transaction Submission
 * ============================================================================ */

nimcp_error_t mesh_ordering_submit(
    mesh_ordering_service_t* service,
    mesh_transaction_t* tx
) {
    if (!service || !tx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    if (service->pending_count >= service->config.max_pending) {
        service->stats.queue_full_rejections++;
        nimcp_mutex_unlock(service->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_QUEUE_FULL, "mesh_ordering: error condition");
        return NIMCP_ERROR_QUEUE_FULL;
    }

    /* Backpressure: warn when queue is > 80% full */
    float utilization = (float)service->pending_count / (float)service->config.max_pending;
    if (utilization > 0.8f) {
        service->stats.backpressure_events++;
        /* Allow submission but signal backpressure to caller for throttling */
        /* Caller can check mesh_ordering_get_utilization() to decide */
    }

    /* Create pending entry */
    pending_tx_t* pending = nimcp_calloc(1, sizeof(pending_tx_t));
    if (!pending) {
        nimcp_mutex_unlock(service->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    pending->id = tx->id;
    pending->tx = tx;
    pending->submit_time_ns = get_time_ns();
    pending->next = NULL;

    /* Add to queue */
    if (service->pending_tail) {
        service->pending_tail->next = pending;
    } else {
        service->pending_head = pending;
    }
    service->pending_tail = pending;
    service->pending_count++;

    service->stats.transactions_submitted++;

    nimcp_mutex_unlock(service->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_ordering_submit_batch(
    mesh_ordering_service_t* service,
    mesh_tx_batch_t* batch
) {
    if (!service || !batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Note: mesh_ordering_submit already locks the mutex per-transaction */
    for (size_t i = 0; i < batch->count; i++) {
        nimcp_error_t err = mesh_ordering_submit(service, batch->transactions[i]);
        if (err != NIMCP_SUCCESS) {
            return err;
        }
    }

    return NIMCP_SUCCESS;
}

bool mesh_ordering_is_pending(
    const mesh_ordering_service_t* service,
    const mesh_tx_id_t* tx_id
) {
    if (!service || !tx_id) {
        return false;
    }

    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);

    pending_tx_t* pending = service->pending_head;
    while (pending) {
        if (mesh_tx_id_compare(&pending->id, tx_id) == 0) {
            nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
            return true;
        }
        pending = pending->next;
    }

    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return false;
}

size_t mesh_ordering_get_pending_count(const mesh_ordering_service_t* service) {
    if (!service) return 0;
    /* P1: Lock mutex to prevent torn read while submit/batch modifies pending_count */
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    size_t count = service->pending_count;
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return count;
}

/* ============================================================================
 * Ordering Operations
 * ============================================================================ */

nimcp_error_t mesh_ordering_create_batch(mesh_ordering_service_t* service) {
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering_create_batch: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    if (!service->current_batch) {
        nimcp_mutex_unlock(service->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "mesh_ordering_create_batch: invalid state");
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Reset batch if starting fresh */
    if (service->current_batch->count == 0) {
        service->current_batch->start_time_ns = get_time_ns();
    }

    /* Move transactions from queue to batch */
    while (service->pending_head &&
           service->current_batch->count < service->current_batch->capacity) {
        pending_tx_t* pending = service->pending_head;
        service->pending_head = pending->next;
        if (!service->pending_head) {
            service->pending_tail = NULL;
        }
        service->pending_count--;

        /* Add to batch */
        size_t idx = service->current_batch->count;
        service->current_batch->tx_ids[idx] = pending->id;
        service->current_batch->transactions[idx] = pending->tx;
        service->current_batch->count++;

        service->stats.transactions_batched++;
        nimcp_free(pending);
    }

    nimcp_mutex_unlock(service->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_ordering_sequence_batch(mesh_ordering_service_t* service) {
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering_sequence_batch: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    /* Only leader can sequence - check role before batch state */
    if (service->raft.role != RAFT_ROLE_LEADER) {
        nimcp_mutex_unlock(service->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_LEADER, "mesh_ordering_sequence_batch: error condition");
        return NIMCP_ERROR_NOT_LEADER;
    }

    if (!service->current_batch || service->current_batch->count == 0) {
        nimcp_mutex_unlock(service->mutex);
        return NIMCP_SUCCESS;  /* Nothing to sequence */
    }

    /* Assign sequence numbers */
    for (size_t i = 0; i < service->current_batch->count; i++) {
        mesh_transaction_t* tx = service->current_batch->transactions[i];
        if (tx) {
            tx->sequence_number = service->current_sequence++;
            tx->ordering_timestamp_ns = get_time_ns();
            tx->status = MESH_TX_STATUS_ORDERED;
        }
    }

    /* Create Raft log entry */
    raft_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.term = service->raft.current_term;
    entry.index = service->raft.log_size;
    entry.type = RAFT_ENTRY_TX_BATCH;
    entry.timestamp_ns = get_time_ns();

    /* Copy transaction IDs */
    entry.tx_count = service->current_batch->count;
    entry.tx_ids = nimcp_calloc(entry.tx_count, sizeof(mesh_tx_id_t));
    if (entry.tx_ids) {
        memcpy(entry.tx_ids, service->current_batch->tx_ids,
               entry.tx_count * sizeof(mesh_tx_id_t));
    }

    /* Append to log (internal, already under mutex) */
    /* Inline append to avoid recursive lock */
    if (service->raft.log_size < service->raft.log_capacity) {
        raft_log_entry_t* new_entry = &service->raft.log[service->raft.log_size];
        *new_entry = entry;
        new_entry->index = service->raft.log_size;
        service->raft.log_size++;
        service->stats.log_entries = service->raft.log_size;
    } else {
        /* Log full - free allocated tx_ids to prevent leak */
        nimcp_free(entry.tx_ids);
    }

    service->stats.transactions_ordered += service->current_batch->count;

    nimcp_mutex_unlock(service->mutex);

    return NIMCP_SUCCESS;
}

mesh_ordered_block_t* mesh_ordering_create_block(
    mesh_ordering_service_t* service
) {
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering_create_block: service is NULL");
        return NULL;
    }

    nimcp_mutex_lock(service->mutex);

    if (!service->current_batch || service->current_batch->count == 0) {
        nimcp_mutex_unlock(service->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_ordering_create_block: service->current_batch is NULL");
        return NULL;
    }

    /* P1-30: NULL deref guard - ensure first transaction is not NULL */
    if (!service->current_batch->transactions[0]) {
        nimcp_mutex_unlock(service->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_ordering_create_block: first transaction in batch is NULL");
        return NULL;
    }

    mesh_ordered_block_t* block = nimcp_calloc(1, sizeof(mesh_ordered_block_t));
    if (!block) {
        nimcp_mutex_unlock(service->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering_create_block: block is NULL");
        return NULL;
    }

    block->block_number = service->current_block_number++;
    block->first_sequence = service->current_batch->transactions[0]->sequence_number;
    /* P2-144: NULL check on last transaction before dereferencing */
    mesh_transaction_t* last_tx = service->current_batch->transactions[service->current_batch->count - 1];
    block->last_sequence = last_tx ? last_tx->sequence_number : block->first_sequence;
    block->timestamp_ns = get_time_ns();

    /* Copy transaction IDs */
    block->tx_count = service->current_batch->count;
    block->tx_ids = nimcp_calloc(block->tx_count, sizeof(mesh_tx_id_t));
    if (block->tx_ids) {
        memcpy(block->tx_ids, service->current_batch->tx_ids,
               block->tx_count * sizeof(mesh_tx_id_t));
    }

    /* Set previous block hash */
    if (service->block_count > 0) {
        memcpy(block->prev_block_hash,
               service->blocks[service->block_count - 1]->block_hash,
               32);
    }

    /* Compute block hash */
    mesh_ordered_block_compute_hash(block);

    /* Store block */
    if (service->block_count >= service->block_capacity) {
        size_t new_capacity = service->block_capacity * 2;
        mesh_ordered_block_t** new_blocks = nimcp_realloc(service->blocks,
            new_capacity * sizeof(mesh_ordered_block_t*));
        if (new_blocks) {
            service->blocks = new_blocks;
            service->block_capacity = new_capacity;
        } else {
            /* Realloc failed - free block to prevent leak */
            NIMCP_LOGGING_ERROR("mesh_ordering_create_block: realloc failed");
            nimcp_free(block->tx_ids);
            nimcp_free(block);
            nimcp_mutex_unlock(service->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering_create_block: realloc failed");
            return NULL;
        }
    }
    if (service->block_count < service->block_capacity) {
        service->blocks[service->block_count++] = block;
    }

    /* Clear current batch */
    service->current_batch->count = 0;

    service->stats.blocks_created++;
    service->stats.current_block = block->block_number;
    service->stats.current_sequence = service->current_sequence;

    nimcp_mutex_unlock(service->mutex);

    return block;
}

const mesh_ordered_block_t* mesh_ordering_get_block(
    const mesh_ordering_service_t* service,
    uint64_t block_number
) {
    if (!service || !service->blocks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_ordering_get_block: required parameter is NULL (service, service->blocks)");
        return NULL;
    }

    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);

    for (size_t i = 0; i < service->block_count; i++) {
        if (service->blocks[i]->block_number == block_number) {
            nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
            return service->blocks[i];
        }
    }

    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return NULL;
}

uint64_t mesh_ordering_get_latest_block(const mesh_ordering_service_t* service) {
    if (!service) return 0;
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    uint64_t val = service->current_block_number;
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return val;
}

uint64_t mesh_ordering_get_sequence(const mesh_ordering_service_t* service) {
    if (!service) return 0;
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    uint64_t val = service->current_sequence;
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return val;
}

/* ============================================================================
 * Raft Consensus
 * ============================================================================ */

nimcp_error_t mesh_ordering_start_election(mesh_ordering_service_t* service) {
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering_start_election: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    /* Increment term */
    service->raft.current_term++;
    service->raft.role = RAFT_ROLE_CANDIDATE;

    /* Vote for self */
    mesh_participant_id_t self_id = 0;
    if (service->orderer_pool) {
        mesh_coordinator_t* leader = mesh_coordinator_pool_get_leader(service->orderer_pool);
        if (leader) {
            self_id = mesh_coordinator_get_id(leader);
        }
    }
    service->raft.voted_for = self_id;

    /* Reset election timeout */
    generate_random_timeout(service);
    service->raft.last_heartbeat_ns = get_time_ns();

    service->stats.elections_started++;

    /* In a real implementation, would request votes from other orderers */
    /* For now, assume we win if we're the only orderer */
    if (!service->orderer_pool || mesh_coordinator_pool_get_size(service->orderer_pool) <= 1) {
        service->raft.role = RAFT_ROLE_LEADER;
        service->raft.leader_id = self_id;
        service->stats.elections_won++;
    }

    nimcp_mutex_unlock(service->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_ordering_handle_vote_request(
    mesh_ordering_service_t* service,
    mesh_participant_id_t candidate_id,
    uint64_t term,
    uint64_t last_log_index,
    uint64_t last_log_term,
    bool* vote_granted
) {
    if (!service || !vote_granted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    *vote_granted = false;

    /* If candidate's term is less than ours, reject */
    if (term < service->raft.current_term) {
        nimcp_mutex_unlock(service->mutex);
        return NIMCP_SUCCESS;
    }

    /* If candidate's term is greater, update our term and become follower */
    if (term > service->raft.current_term) {
        service->raft.current_term = term;
        service->raft.role = RAFT_ROLE_FOLLOWER;
        service->raft.voted_for = 0;
    }

    /* Grant vote if we haven't voted yet and candidate's log is up-to-date */
    if ((service->raft.voted_for == 0 || service->raft.voted_for == candidate_id) &&
        is_log_up_to_date(&service->raft, last_log_index, last_log_term)) {
        service->raft.voted_for = candidate_id;
        *vote_granted = true;
        service->raft.last_heartbeat_ns = get_time_ns();
    }

    nimcp_mutex_unlock(service->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_ordering_handle_vote_response(
    mesh_ordering_service_t* service,
    mesh_participant_id_t voter_id,
    uint64_t term,
    bool vote_granted
) {
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    (void)voter_id;

    /* Ignore if not candidate */
    if (service->raft.role != RAFT_ROLE_CANDIDATE) {
        nimcp_mutex_unlock(service->mutex);
        return NIMCP_SUCCESS;
    }

    /* If term is greater, become follower */
    if (term > service->raft.current_term) {
        service->raft.current_term = term;
        service->raft.role = RAFT_ROLE_FOLLOWER;
        service->raft.voted_for = 0;
        nimcp_mutex_unlock(service->mutex);
        return NIMCP_SUCCESS;
    }

    /* Count votes and check for majority */
    /* In a real implementation, would track votes per term */
    if (vote_granted) {
        /* Assume winning with majority */
        mesh_participant_id_t self_id = 0;
        if (service->orderer_pool) {
            mesh_coordinator_t* coord = mesh_coordinator_pool_get_by_index(service->orderer_pool, 0);
            if (coord) {
                self_id = mesh_coordinator_get_id(coord);
            }
        }
        service->raft.role = RAFT_ROLE_LEADER;
        service->raft.leader_id = self_id;
        service->stats.elections_won++;
    }

    nimcp_mutex_unlock(service->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_ordering_handle_append_entries(
    mesh_ordering_service_t* service,
    mesh_participant_id_t leader_id,
    uint64_t term,
    uint64_t prev_log_index,
    uint64_t prev_log_term,
    const raft_log_entry_t* entries,
    size_t entry_count,
    uint64_t leader_commit,
    bool* success
) {
    if (!service || !success) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    *success = false;

    /* If term is less than ours, reject */
    if (term < service->raft.current_term) {
        nimcp_mutex_unlock(service->mutex);
        return NIMCP_SUCCESS;
    }

    /* If term is greater or equal, recognize leader */
    if (term >= service->raft.current_term) {
        service->raft.current_term = term;
        service->raft.role = RAFT_ROLE_FOLLOWER;
        service->raft.leader_id = leader_id;
    }

    /* Update heartbeat time */
    service->raft.last_heartbeat_ns = get_time_ns();

    /* Check if log matches at prev_log_index */
    if (prev_log_index > 0) {
        if (prev_log_index > service->raft.log_size) {
            /* P1-46: Must unlock mutex before early return */
            nimcp_mutex_unlock(service->mutex);
            return NIMCP_SUCCESS;  /* Missing entries */
        }
        if (prev_log_index <= service->raft.log_size &&
            service->raft.log[prev_log_index - 1].term != prev_log_term) {
            /* Conflict - truncate log */
            mesh_ordering_log_truncate(service, prev_log_index - 1);
        }
    }

    /* Append new entries */
    if (entries && entry_count > 0) {
        for (size_t i = 0; i < entry_count; i++) {
            mesh_ordering_log_append(service, &entries[i]);
        }
    }

    /* Update commit index */
    if (leader_commit > service->raft.commit_index) {
        uint64_t last_new_entry = service->raft.log_size > 0 ?
            service->raft.log[service->raft.log_size - 1].index : 0;
        service->raft.commit_index = leader_commit < last_new_entry ?
            leader_commit : last_new_entry;
    }

    *success = true;
    nimcp_mutex_unlock(service->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_ordering_send_heartbeat(mesh_ordering_service_t* service) {
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering_send_heartbeat: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    if (service->raft.role != RAFT_ROLE_LEADER) {
        nimcp_mutex_unlock(service->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_LEADER, "mesh_ordering_send_heartbeat: error condition");
        return NIMCP_ERROR_NOT_LEADER;
    }

    service->last_heartbeat_time_ns = get_time_ns();

    /* In a real implementation, would send AppendEntries to all followers */

    nimcp_mutex_unlock(service->mutex);

    return NIMCP_SUCCESS;
}

raft_role_t mesh_ordering_get_role(const mesh_ordering_service_t* service) {
    if (!service) return RAFT_ROLE_FOLLOWER;
    /* P1: Lock mutex for consistent Raft state reads */
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    raft_role_t role = service->raft.role;
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return role;
}

uint64_t mesh_ordering_get_term(const mesh_ordering_service_t* service) {
    if (!service) return 0;
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    uint64_t term = service->raft.current_term;
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return term;
}

mesh_participant_id_t mesh_ordering_get_leader(
    const mesh_ordering_service_t* service
) {
    if (!service) return 0;
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    mesh_participant_id_t leader = service->raft.leader_id;
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return leader;
}

bool mesh_ordering_is_leader(const mesh_ordering_service_t* service) {
    if (!service) return false;
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    bool is_leader = (service->raft.role == RAFT_ROLE_LEADER);
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return is_leader;
}

bool mesh_ordering_has_quorum(const mesh_ordering_service_t* service) {
    if (!service || !service->orderer_pool) {
        return false;
    }

    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);

    size_t pool_size = mesh_coordinator_pool_get_size(service->orderer_pool);
    size_t failed = mesh_coordinator_pool_get_failed_count(service->orderer_pool);
    size_t active = pool_size - failed;

    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);

    /* Quorum is majority */
    return active > pool_size / 2;
}

/* ============================================================================
 * Log Management
 * ============================================================================ */

nimcp_error_t mesh_ordering_log_append(
    mesh_ordering_service_t* service,
    const raft_log_entry_t* entry
) {
    if (!service || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    /* Grow log if needed */
    if (service->raft.log_size >= service->raft.log_capacity) {
        size_t new_capacity = service->raft.log_capacity * 2;
        raft_log_entry_t* new_log = nimcp_realloc(service->raft.log,
            new_capacity * sizeof(raft_log_entry_t));
        if (!new_log) {
            nimcp_mutex_unlock(service->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_ordering: memory allocation failed");
            return NIMCP_ERROR_NO_MEMORY;
        }
        service->raft.log = new_log;
        service->raft.log_capacity = new_capacity;
    }

    /* Copy entry */
    raft_log_entry_t* new_entry = &service->raft.log[service->raft.log_size];
    *new_entry = *entry;
    new_entry->index = service->raft.log_size;

    /* Copy tx_ids if present */
    if (entry->tx_ids && entry->tx_count > 0) {
        new_entry->tx_ids = nimcp_calloc(entry->tx_count, sizeof(mesh_tx_id_t));
        if (new_entry->tx_ids) {
            memcpy(new_entry->tx_ids, entry->tx_ids,
                   entry->tx_count * sizeof(mesh_tx_id_t));
        }
    }

    service->raft.log_size++;
    service->stats.log_entries = service->raft.log_size;

    nimcp_mutex_unlock(service->mutex);

    return NIMCP_SUCCESS;
}

const raft_log_entry_t* mesh_ordering_log_get(
    const mesh_ordering_service_t* service,
    uint64_t index
) {
    if (!service || index >= service->raft.log_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering_log_get: service is NULL");
        return NULL;
    }
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    const raft_log_entry_t* result = &service->raft.log[index];
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return result;
}

uint64_t mesh_ordering_log_last_index(const mesh_ordering_service_t* service) {
    if (!service) return 0;
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    uint64_t val = (service->raft.log_size == 0) ? 0 : service->raft.log_size - 1;
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return val;
}

uint64_t mesh_ordering_log_last_term(const mesh_ordering_service_t* service) {
    if (!service) return 0;
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    uint64_t val = (service->raft.log_size == 0) ? 0 : service->raft.log[service->raft.log_size - 1].term;
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return val;
}

uint64_t mesh_ordering_get_commit_index(const mesh_ordering_service_t* service) {
    if (!service) return 0;
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    uint64_t val = service->raft.commit_index;
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return val;
}

nimcp_error_t mesh_ordering_log_truncate(
    mesh_ordering_service_t* service,
    uint64_t index
) {
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    if (index >= service->raft.log_size) {
        nimcp_mutex_unlock(service->mutex);
        return NIMCP_SUCCESS;
    }

    /* Free truncated entries */
    for (size_t i = index + 1; i < service->raft.log_size; i++) {
        nimcp_free(service->raft.log[i].tx_ids);
    }

    service->raft.log_size = index + 1;
    nimcp_mutex_unlock(service->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_ordering_log_compact(
    mesh_ordering_service_t* service,
    uint64_t up_to_index
) {
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    if (up_to_index >= service->raft.log_size) {
        nimcp_mutex_unlock(service->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Free compacted entries */
    for (size_t i = 0; i <= up_to_index; i++) {
        nimcp_free(service->raft.log[i].tx_ids);
    }

    /* Shift remaining entries */
    size_t remaining = service->raft.log_size - up_to_index - 1;
    if (remaining > 0) {
        memmove(service->raft.log,
                &service->raft.log[up_to_index + 1],
                remaining * sizeof(raft_log_entry_t));
    }
    service->raft.log_size = remaining;

    nimcp_mutex_unlock(service->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Channel Management
 * ============================================================================ */

nimcp_error_t mesh_ordering_add_channel(
    mesh_ordering_service_t* service,
    mesh_channel_id_t channel
) {
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    /* Check if already added */
    for (size_t i = 0; i < service->channel_count; i++) {
        if (service->channels[i] == channel) {
            nimcp_mutex_unlock(service->mutex);
            return NIMCP_SUCCESS;
        }
    }

    if (service->channel_count >= service->channel_capacity) {
        nimcp_mutex_unlock(service->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_ordering: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    service->channels[service->channel_count++] = channel;
    nimcp_mutex_unlock(service->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_ordering_remove_channel(
    mesh_ordering_service_t* service,
    mesh_channel_id_t channel
) {
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    for (size_t i = 0; i < service->channel_count; i++) {
        if (service->channels[i] == channel) {
            /* Shift remaining */
            for (size_t j = i; j < service->channel_count - 1; j++) {
                service->channels[j] = service->channels[j + 1];
            }
            service->channel_count--;
            nimcp_mutex_unlock(service->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(service->mutex);
    /* Not found is a normal result, not an error (P2: false positive removal) */
    return NIMCP_ERROR_NOT_FOUND;
}

bool mesh_ordering_has_channel(
    const mesh_ordering_service_t* service,
    mesh_channel_id_t channel
) {
    if (!service) {
        return false;
    }

    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);

    for (size_t i = 0; i < service->channel_count; i++) {
        if (service->channels[i] == channel) {
            nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    /* Not found is a normal result, not an error (P2: false positive removal) */
    return false;
}

/* ============================================================================
 * Update
 * ============================================================================ */

nimcp_error_t mesh_ordering_update(
    mesh_ordering_service_t* service,
    uint64_t delta_ms
) {
    if (!service) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(service->mutex);

    uint64_t now = get_time_ns();
    (void)delta_ms;

    /* Check election timeout (follower/candidate) */
    if (service->raft.role != RAFT_ROLE_LEADER) {
        uint64_t elapsed = now - service->raft.last_heartbeat_ns;
        if (elapsed > service->raft.election_timeout_ns) {
            /* Unlock before calling public function to avoid recursive lock */
            nimcp_mutex_unlock(service->mutex);
            mesh_ordering_start_election(service);
            nimcp_mutex_lock(service->mutex);
        }
    }

    /* Leader duties */
    if (service->raft.role == RAFT_ROLE_LEADER) {
        /* Send heartbeat if needed */
        uint64_t heartbeat_interval_ns = (uint64_t)(service->config.heartbeat_interval_ms * 1000000.0f);
        if (now - service->last_heartbeat_time_ns > heartbeat_interval_ns) {
            nimcp_mutex_unlock(service->mutex);
            mesh_ordering_send_heartbeat(service);
            nimcp_mutex_lock(service->mutex);
        }

        /* Create batch if pending and timeout reached */
        uint64_t batch_timeout_ns = (uint64_t)(service->config.batch_timeout_ms * 1000000.0f);
        bool batch_full = service->current_batch &&
                          service->current_batch->count >= service->config.batch_size;
        bool batch_timeout = service->current_batch &&
                             service->current_batch->count > 0 &&
                             (now - service->current_batch->start_time_ns > batch_timeout_ns);

        if (service->pending_count > 0 || (service->current_batch && service->current_batch->count > 0)) {
            nimcp_mutex_unlock(service->mutex);
            mesh_ordering_create_batch(service);

            if (batch_full || batch_timeout) {
                mesh_ordering_sequence_batch(service);
                mesh_ordering_create_block(service);
            }
            nimcp_mutex_lock(service->mutex);
        }
    }

    /* Update stats */
    service->stats.role = service->raft.role;
    service->stats.current_term = service->raft.current_term;
    service->stats.leader_id = service->raft.leader_id;
    service->stats.commit_index = service->raft.commit_index;
    service->stats.pending_count = service->pending_count;

    nimcp_mutex_unlock(service->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

nimcp_error_t mesh_ordering_get_stats(
    const mesh_ordering_service_t* service,
    mesh_ordering_stats_t* stats
) {
    if (!service || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordering: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);

    *stats = service->stats;

    /* Update dynamic fields */
    stats->pending_count = service->pending_count;
    stats->max_pending = service->config.max_pending;
    stats->queue_utilization = service->config.max_pending > 0 ?
        (float)service->pending_count / (float)service->config.max_pending : 0.0f;

    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);

    return NIMCP_SUCCESS;
}

void mesh_ordering_reset_stats(mesh_ordering_service_t* service) {
    if (!service) {
        return;
    }

    nimcp_mutex_lock(service->mutex);

    memset(&service->stats, 0, sizeof(service->stats));
    service->stats.role = service->raft.role;
    service->stats.current_term = service->raft.current_term;
    service->stats.leader_id = service->raft.leader_id;
    service->stats.max_pending = service->config.max_pending;

    nimcp_mutex_unlock(service->mutex);
}

float mesh_ordering_get_utilization(const mesh_ordering_service_t* service) {
    if (!service || service->config.max_pending == 0) {
        return 0.0f;
    }
    nimcp_mutex_lock(((mesh_ordering_service_t*)service)->mutex);
    float result = (float)service->pending_count / (float)service->config.max_pending;
    nimcp_mutex_unlock(((mesh_ordering_service_t*)service)->mutex);
    return result;
}

bool mesh_ordering_is_backpressure_active(const mesh_ordering_service_t* service) {
    if (!service) {
        return false;
    }
    return mesh_ordering_get_utilization(service) > 0.8f;
}

/* ============================================================================
 * Block Management
 * ============================================================================ */

void mesh_ordered_block_destroy(mesh_ordered_block_t* block) {
    if (!block) {
        return;
    }

    nimcp_free(block->tx_ids);
    nimcp_free(block);
}

nimcp_error_t mesh_ordered_block_compute_hash(mesh_ordered_block_t* block) {
    if (!block) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_ordered_block_compute_hash: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Simple hash: combine all data */
    /* In production, use proper SHA256 */
    memset(block->block_hash, 0, 32);

    uint64_t hash = 0;
    hash ^= block->block_number;
    hash ^= block->first_sequence;
    hash ^= block->last_sequence;
    hash ^= block->timestamp_ns;
    hash ^= (uint64_t)block->tx_count;

    memcpy(block->block_hash, &hash, sizeof(hash));

    return NIMCP_SUCCESS;
}

bool mesh_ordered_block_verify_hash(const mesh_ordered_block_t* block) {
    if (!block) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_ordered_block_verify_hash: block is NULL");
        return false;
    }

    mesh_ordered_block_t temp = *block;
    mesh_ordered_block_compute_hash(&temp);

    return memcmp(block->block_hash, temp.block_hash, 32) == 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* mesh_raft_role_to_string(raft_role_t role) {
    switch (role) {
        case RAFT_ROLE_FOLLOWER:  return "FOLLOWER";
        case RAFT_ROLE_CANDIDATE: return "CANDIDATE";
        case RAFT_ROLE_LEADER:    return "LEADER";
        default:                  return "UNKNOWN";
    }
}

/* P3: Diagnostic print functions use printf intentionally for console output.
 * These are developer debugging aids, not production logging paths. */
void mesh_ordering_print_status(const mesh_ordering_service_t* service) {
    if (!service) {
        printf("Ordering Service: NULL\n");
        return;
    }

    printf("=== Ordering Service: %s ===\n", service->name);
    printf("  Role: %s\n", mesh_raft_role_to_string(service->raft.role));
    printf("  Term: %lu\n", (unsigned long)service->raft.current_term);
    printf("  Leader: %lu\n", (unsigned long)service->raft.leader_id);
    printf("  Pending: %zu\n", service->pending_count);
    printf("  Blocks: %zu\n", service->block_count);
    printf("  Sequence: %lu\n", (unsigned long)service->current_sequence);
    printf("  Log entries: %zu\n", service->raft.log_size);
    printf("  Commit index: %lu\n", (unsigned long)service->raft.commit_index);
}

/* P3: Diagnostic print function - printf intentional for console debug output */
void mesh_ordering_print_raft_state(const mesh_ordering_service_t* service) {
    if (!service) {
        return;
    }

    printf("=== Raft State ===\n");
    printf("  Role: %s\n", mesh_raft_role_to_string(service->raft.role));
    printf("  Term: %lu\n", (unsigned long)service->raft.current_term);
    printf("  Voted for: %lu\n", (unsigned long)service->raft.voted_for);
    printf("  Leader: %lu\n", (unsigned long)service->raft.leader_id);
    printf("  Log size: %zu\n", service->raft.log_size);
    printf("  Commit index: %lu\n", (unsigned long)service->raft.commit_index);
    printf("  Last applied: %lu\n", (unsigned long)service->raft.last_applied);
}

/* P3: Diagnostic print function - printf intentional for console debug output */
void mesh_ordered_block_print(const mesh_ordered_block_t* block) {
    if (!block) {
        printf("Block: NULL\n");
        return;
    }

    printf("=== Block %lu ===\n", (unsigned long)block->block_number);
    printf("  Sequences: %lu - %lu\n",
           (unsigned long)block->first_sequence,
           (unsigned long)block->last_sequence);
    printf("  Transactions: %zu\n", block->tx_count);
    printf("  Timestamp: %lu ns\n", (unsigned long)block->timestamp_ns);
}
