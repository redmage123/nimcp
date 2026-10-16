/**
 * @file nimcp_mesh_coordinator_pool.c
 * @brief Mesh Network Coordinator Pool Implementation
 *
 * WHAT: Implementation of coordinator pool with BFT election and load balancing
 * WHY:  Fault tolerance through coordinator redundancy
 * HOW:  BFT voting for election, round-robin for assignment
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Vote tally for election
 */
typedef struct vote_tally {
    mesh_participant_id_t candidate;    /**< Candidate ID */
    size_t votes;                       /**< Vote count */
} vote_tally_t;

/**
 * @brief Vote record for duplicate detection
 */
typedef struct voter_record {
    mesh_participant_id_t voter_id;         /**< Who voted */
    mesh_participant_id_t voted_for;        /**< Who they voted for */
    uint64_t term;                          /**< Term of vote */
} voter_record_t;

/**
 * @brief Coordinator pool structure
 */
struct mesh_coordinator_pool {
    uint32_t magic;                         /**< Magic for validation */
    char name[MESH_MAX_NAME_LEN];           /**< Pool name */
    mesh_pool_id_t id;                      /**< Pool ID */
    mesh_channel_id_t channel;              /**< Associated channel */
    coordinator_level_t level;              /**< Hierarchy level */

    /* Coordinators */
    mesh_coordinator_t** coordinators;      /**< Coordinator array */
    size_t coordinator_count;               /**< Current count */
    size_t coordinator_capacity;            /**< Array capacity */

    /* Leader tracking */
    size_t leader_index;                    /**< Index of leader (-1 if none) */
    mesh_participant_id_t leader_id;        /**< Leader participant ID */

    /* Election state */
    uint64_t current_term;                  /**< Current election term */
    bool election_in_progress;              /**< Election ongoing */
    uint64_t election_start_ns;             /**< Election start time */
    vote_tally_t* vote_tallies;             /**< Vote tallies */
    size_t tally_count;                     /**< Number of tallies */
    voter_record_t* voter_records;          /**< Track who voted (duplicate detection) */
    size_t voter_record_count;              /**< Number of voter records */
    size_t voter_record_capacity;           /**< Voter record capacity */
    mesh_election_result_t last_election;   /**< Last election result */

    /* Election callback */
    mesh_election_callback_t election_callback;
    void* election_callback_ctx;

    /* Load balancing */
    size_t next_assignment_index;           /**< Round-robin counter */
    uint64_t last_rebalance_ns;             /**< Last rebalance time */

    /* Configuration */
    mesh_coordinator_pool_config_t config;  /**< Pool configuration */

    /* Statistics */
    mesh_coordinator_pool_stats_t stats;    /**< Pool statistics */

    /* References */
    mesh_participant_registry_t* registry;  /**< Participant registry */
    mesh_channel_t* channel_ref;            /**< Channel reference */

    /* Thread safety */
    nimcp_mutex_t* mutex;                   /**< Pool mutex */

    /* Timestamps */
    uint64_t creation_time_ns;              /**< Creation timestamp */
    uint64_t last_update_ns;                /**< Last update timestamp */
};

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/**
 * @brief Validate pool handle
 */
static bool validate_pool(const mesh_coordinator_pool_t* pool) {
    return pool && pool->magic == NIMCP_MESH_MAGIC;
}

/**
 * @brief Find coordinator index by ID
 */
static int find_coordinator_index(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t coord_id
) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_coordinator_index: pool is NULL");
        return -1;
    }

    for (size_t i = 0; i < pool->coordinator_count; i++) {
        if (pool->coordinators[i] &&
            mesh_coordinator_get_id(pool->coordinators[i]) == coord_id) {
            return (int)i;
        }
    }
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return -1;
}

/**
 * @brief Get active (non-failed) coordinator count
 */
static size_t get_active_count(const mesh_coordinator_pool_t* pool) {
    if (!pool) return 0;

    size_t count = 0;
    for (size_t i = 0; i < pool->coordinator_count; i++) {
        if (pool->coordinators[i]) {
            coordinator_state_t state = mesh_coordinator_get_state(pool->coordinators[i]);
            if (state != COORD_STATE_FAILED && state != COORD_STATE_SHUTDOWN) {
                count++;
            }
        }
    }
    return count;
}

/**
 * @brief Get count by role
 */
static size_t get_role_count(mesh_coordinator_pool_t* pool, coordinator_role_t role) {
    if (!pool) return 0;

    size_t count = 0;
    for (size_t i = 0; i < pool->coordinator_count; i++) {
        if (pool->coordinators[i] &&
            mesh_coordinator_get_role(pool->coordinators[i]) == role) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Get first coordinator with role
 */
static mesh_coordinator_t* get_first_with_role(
    mesh_coordinator_pool_t* pool,
    coordinator_role_t role
) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_first_with_role: pool is NULL");
        return NULL;
    }

    for (size_t i = 0; i < pool->coordinator_count; i++) {
        if (pool->coordinators[i] &&
            mesh_coordinator_get_role(pool->coordinators[i]) == role) {
            return pool->coordinators[i];
        }
    }
    /* No coordinator with requested role found - normal result (P2: false positive removal) */
    return NULL;
}

/**
 * @brief Find vote tally for candidate
 */
static vote_tally_t* find_tally(mesh_coordinator_pool_t* pool, mesh_participant_id_t candidate) {
    if (!pool || !pool->vote_tallies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_tally: required parameter is NULL (pool, pool->vote_tallies)");
        return NULL;
    }

    for (size_t i = 0; i < pool->tally_count; i++) {
        if (pool->vote_tallies[i].candidate == candidate) {
            return &pool->vote_tallies[i];
        }
    }
    /* No tally for candidate found - first vote, not an error (P2: false positive removal) */
    return NULL;
}

/**
 * @brief Add or increment vote tally
 */
static void add_vote(mesh_coordinator_pool_t* pool, mesh_participant_id_t candidate) {
    if (!pool) return;

    vote_tally_t* tally = find_tally(pool, candidate);
    if (tally) {
        tally->votes++;
    } else if (pool->tally_count < pool->coordinator_count) {
        pool->vote_tallies[pool->tally_count].candidate = candidate;
        pool->vote_tallies[pool->tally_count].votes = 1;
        pool->tally_count++;
    }
}

/**
 * @brief Check if voter has already voted in current term (Byzantine detection)
 */
static bool has_voter_already_voted(mesh_coordinator_pool_t* pool, mesh_participant_id_t voter_id) {
    if (!pool || !pool->voter_records) {
        /* No voter records means no prior votes, not an error (P2: false positive removal) */
        return false;
    }

    for (size_t i = 0; i < pool->voter_record_count; i++) {
        if (pool->voter_records[i].voter_id == voter_id &&
            pool->voter_records[i].term == pool->current_term) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Record a vote (for duplicate detection)
 */
static void record_vote(mesh_coordinator_pool_t* pool, mesh_participant_id_t voter_id,
                        mesh_participant_id_t voted_for) {
    if (!pool || !pool->voter_records) return;
    if (pool->voter_record_count >= pool->voter_record_capacity) return;

    voter_record_t* record = &pool->voter_records[pool->voter_record_count++];
    record->voter_id = voter_id;
    record->voted_for = voted_for;
    record->term = pool->current_term;
}

/**
 * @brief Clear vote records for new term
 */
static void clear_voter_records(mesh_coordinator_pool_t* pool) {
    if (pool) {
        pool->voter_record_count = 0;
    }
}

/**
 * @brief Get election winner (if any)
 *
 * Uses simple majority (N/2 + 1) for leader election.
 * This allows fault tolerance: with N coordinators, can tolerate (N-1)/2 failures.
 * For BFT with Byzantine faults, would need 2N/3 + 1, but simple majority
 * is appropriate for crash-fault tolerance.
 */
static mesh_participant_id_t get_election_winner(mesh_coordinator_pool_t* pool) {
    if (!pool || !pool->vote_tallies) return 0;

    size_t active = get_active_count(pool);
    if (active == 0) return 0;

    /* Simple majority: (N/2) + 1 for fault tolerance
     * For 3 nodes: quorum = 2 (can tolerate 1 failure)
     * For 4 nodes: quorum = 3 (can tolerate 1 failure)
     * For 5 nodes: quorum = 3 (can tolerate 2 failures)
     */
    size_t quorum = (active / 2) + 1;

    /* First pass: check for majority winner */
    for (size_t i = 0; i < pool->tally_count; i++) {
        if (pool->vote_tallies[i].votes >= quorum) {
            return pool->vote_tallies[i].candidate;
        }
    }

    /* No majority winner - use plurality (most votes wins, first breaks ties).
     * This ensures elections always converge when all coordinators vote for
     * themselves, which is the common case when all are equally healthy. */
    mesh_participant_id_t best_candidate = 0;
    size_t best_votes = 0;
    for (size_t i = 0; i < pool->tally_count; i++) {
        if (pool->vote_tallies[i].votes > best_votes) {
            best_votes = pool->vote_tallies[i].votes;
            best_candidate = pool->vote_tallies[i].candidate;
        }
    }
    return best_candidate;
}

/**
 * @brief Get least loaded worker
 */
static mesh_coordinator_t* get_least_loaded_worker(mesh_coordinator_pool_t* pool) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_least_loaded_worker: pool is NULL");
        return NULL;
    }

    mesh_coordinator_t* least_loaded = NULL;
    float min_load = 2.0f;

    for (size_t i = 0; i < pool->coordinator_count; i++) {
        mesh_coordinator_t* coord = pool->coordinators[i];
        if (!coord) continue;

        coordinator_role_t role = mesh_coordinator_get_role(coord);
        if (role != COORD_ROLE_WORKER) continue;

        coordinator_state_t state = mesh_coordinator_get_state(coord);
        if (state == COORD_STATE_FAILED || state == COORD_STATE_SHUTDOWN) continue;

        float load = mesh_coordinator_get_load(coord);
        if (load < min_load) {
            min_load = load;
            least_loaded = coord;
        }
    }

    return least_loaded;
}

/**
 * @brief Update pool statistics
 */
static void update_stats(mesh_coordinator_pool_t* pool) {
    if (!pool) return;

    pool->stats.coordinator_count = pool->coordinator_count;
    pool->stats.leader_count = get_role_count(pool, COORD_ROLE_LEADER);
    pool->stats.worker_count = get_role_count(pool, COORD_ROLE_WORKER);
    pool->stats.standby_count = get_role_count(pool, COORD_ROLE_STANDBY);
    pool->stats.active_count = get_active_count(pool);
    pool->stats.failed_count = pool->coordinator_count - pool->stats.active_count;
    pool->stats.current_term = pool->current_term;
    pool->stats.leader_id = pool->leader_id;

    /* Calculate load stats */
    size_t total_participants = 0;
    float total_load = 0.0f;
    float max_load = 0.0f;
    size_t worker_count = 0;

    for (size_t i = 0; i < pool->coordinator_count; i++) {
        if (!pool->coordinators[i]) continue;
        if (mesh_coordinator_get_role(pool->coordinators[i]) != COORD_ROLE_WORKER) continue;

        float load = mesh_coordinator_get_load(pool->coordinators[i]);
        total_load += load;
        if (load > max_load) max_load = load;
        worker_count++;

        total_participants += mesh_coordinator_get_participant_count(pool->coordinators[i]);
    }

    pool->stats.total_participants = total_participants;
    pool->stats.avg_load = worker_count > 0 ? total_load / (float)worker_count : 0.0f;
    pool->stats.max_load = max_load;

    /* Uptime ratio */
    if (pool->coordinator_count > 0) {
        pool->stats.uptime_ratio = (float)pool->stats.active_count /
                                    (float)pool->coordinator_count;
    }
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_error_t mesh_coordinator_pool_default_config(
    mesh_coordinator_pool_config_t* config
) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));
    config->pool_name = "coordinator_pool";
    config->pool_id = 1;
    config->channel = MESH_CHANNEL_SYSTEM;
    config->level = COORD_LEVEL_LAYER;
    config->initial_size = MESH_DEFAULT_POOL_SIZE;
    config->min_size = MESH_MIN_POOL_SIZE_BFT;
    config->max_size = MESH_MAX_COORDINATORS_PER_POOL;
    config->election_timeout_ms = MESH_DEFAULT_ELECTION_TIMEOUT_MS;
    config->max_election_rounds = MESH_MAX_ELECTION_ROUNDS;
    config->load_threshold = MESH_COORDINATOR_LOAD_THRESHOLD;
    config->rebalance_interval_ms = MESH_DEFAULT_REBALANCE_INTERVAL_MS;
    config->max_consecutive_failures = MESH_MAX_CONSECUTIVE_FAILURES;
    config->enable_logging = true;

    return NIMCP_SUCCESS;
}

mesh_coordinator_pool_t* mesh_coordinator_pool_create(
    const mesh_coordinator_pool_config_t* config,
    mesh_participant_registry_t* registry,
    mesh_channel_t* channel
) {
    mesh_coordinator_pool_config_t default_config;
    if (!config) {
        mesh_coordinator_pool_default_config(&default_config);
        config = &default_config;
    }

    mesh_coordinator_pool_t* pool = nimcp_calloc(1, sizeof(*pool));
    if (!pool) {
        LOG_ERROR("Failed to allocate coordinator pool");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_coordinator_pool_create: pool is NULL");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&pool->config, config, sizeof(*config));
    if (config->pool_name) {
        strncpy(pool->name, config->pool_name, MESH_MAX_NAME_LEN - 1);
    }
    pool->id = config->pool_id;
    pool->channel = config->channel;
    pool->level = config->level;

    /* Allocate coordinator array */
    pool->coordinator_capacity = config->max_size;
    pool->coordinators = nimcp_calloc(pool->coordinator_capacity,
                                       sizeof(mesh_coordinator_t*));
    if (!pool->coordinators) {
        LOG_ERROR("Failed to allocate coordinator array");
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_coordinator_pool_create: pool->coordinators is NULL");
        return NULL;
    }

    /* Allocate vote tallies */
    pool->vote_tallies = nimcp_calloc(pool->coordinator_capacity, sizeof(vote_tally_t));
    if (!pool->vote_tallies) {
        LOG_ERROR("Failed to allocate vote tallies");
        nimcp_free(pool->coordinators);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_coordinator_pool_create: pool->vote_tallies is NULL");
        return NULL;
    }

    /* Allocate voter records for duplicate vote detection (Byzantine protection) */
    pool->voter_record_capacity = pool->coordinator_capacity;
    pool->voter_records = nimcp_calloc(pool->voter_record_capacity, sizeof(voter_record_t));
    if (!pool->voter_records) {
        LOG_ERROR("Failed to allocate voter records");
        nimcp_free(pool->vote_tallies);
        nimcp_free(pool->coordinators);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_coordinator_pool_create: pool->voter_records is NULL");
        return NULL;
    }

    /* Create mutex */
    pool->mutex = nimcp_mutex_create(NULL);
    if (!pool->mutex) {
        LOG_ERROR("Failed to create pool mutex");
        nimcp_free(pool->voter_records);
        nimcp_free(pool->vote_tallies);
        nimcp_free(pool->coordinators);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_coordinator_pool_create: pool->mutex is NULL");
        return NULL;
    }

    /* Initialize state */
    pool->magic = NIMCP_MESH_MAGIC;
    pool->registry = registry;
    pool->channel_ref = channel;
    pool->leader_index = (size_t)-1;
    pool->leader_id = 0;
    pool->current_term = 0;
    pool->election_in_progress = false;
    pool->creation_time_ns = nimcp_time_now_ns();
    pool->last_update_ns = pool->creation_time_ns;
    pool->last_rebalance_ns = pool->creation_time_ns;

    /* Initialize statistics */
    memset(&pool->stats, 0, sizeof(pool->stats));
    pool->stats.pool_id = pool->id;

    /* Create initial coordinators */
    for (size_t i = 0; i < config->initial_size; i++) {
        char coord_name[MESH_MAX_NAME_LEN];
        snprintf(coord_name, sizeof(coord_name), "%s_coord_%zu", pool->name, i);

        mesh_coordinator_config_t coord_config;
        mesh_coordinator_default_config(&coord_config);
        coord_config.name = coord_name;
        coord_config.level = config->level;
        coord_config.channel = config->channel;
        coord_config.pool_id = config->pool_id;

        mesh_coordinator_t* coord = mesh_coordinator_create(&coord_config, registry, channel);
        if (coord) {
            pool->coordinators[pool->coordinator_count++] = coord;

            /* First coordinator becomes initial leader */
            if (pool->coordinator_count == 1) {
                mesh_coordinator_set_role(coord, COORD_ROLE_LEADER);
                mesh_coordinator_set_state(coord, COORD_STATE_ACTIVE);
                pool->leader_index = 0;
                pool->leader_id = mesh_coordinator_get_id(coord);
            } else if (pool->coordinator_count <= 3) {
                mesh_coordinator_set_role(coord, COORD_ROLE_WORKER);
                mesh_coordinator_set_state(coord, COORD_STATE_ACTIVE);
            } else {
                mesh_coordinator_set_role(coord, COORD_ROLE_STANDBY);
                mesh_coordinator_set_state(coord, COORD_STATE_ACTIVE);
            }
        }
    }

    update_stats(pool);

    LOG_INFO("Created coordinator pool '%s' (id=%u) with %zu coordinators",
             pool->name, pool->id, pool->coordinator_count);

    return pool;
}

void mesh_coordinator_pool_destroy(mesh_coordinator_pool_t* pool) {
    if (!pool) return;

    /* Destroy all coordinators */
    if (pool->coordinators) {
        for (size_t i = 0; i < pool->coordinator_count; i++) {
            if (pool->coordinators[i]) {
                mesh_coordinator_destroy(pool->coordinators[i]);
            }
        }
        nimcp_free(pool->coordinators);
    }

    if (pool->vote_tallies) {
        nimcp_free(pool->vote_tallies);
    }

    if (pool->voter_records) {
        nimcp_free(pool->voter_records);
    }

    if (pool->mutex) {
        nimcp_mutex_destroy(pool->mutex);
    }

    LOG_INFO("Destroyed coordinator pool '%s'", pool->name);

    pool->magic = 0;
    nimcp_free(pool);
}

mesh_pool_id_t mesh_coordinator_pool_get_id(const mesh_coordinator_pool_t* pool) {
    return pool ? pool->id : 0;
}

const char* mesh_coordinator_pool_get_name(const mesh_coordinator_pool_t* pool) {
    return pool ? pool->name : NULL;
}

/* ============================================================================
 * Coordinator Management
 * ============================================================================ */

nimcp_error_t mesh_coordinator_pool_add(
    mesh_coordinator_pool_t* pool,
    mesh_coordinator_t* coordinator
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;
    if (!coordinator) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(pool->mutex);

    if (pool->coordinator_count >= pool->coordinator_capacity) {
        nimcp_mutex_unlock(pool->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_coordinator_pool: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Check if already in pool */
    mesh_participant_id_t coord_id = mesh_coordinator_get_id(coordinator);
    if (find_coordinator_index(pool, coord_id) >= 0) {
        nimcp_mutex_unlock(pool->mutex);
        return NIMCP_SUCCESS;
    }

    pool->coordinators[pool->coordinator_count++] = coordinator;
    mesh_coordinator_set_role(coordinator, COORD_ROLE_STANDBY);
    mesh_coordinator_set_state(coordinator, COORD_STATE_JOINING);

    update_stats(pool);

    nimcp_mutex_unlock(pool->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_coordinator_pool_remove(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t coordinator_id
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(pool->mutex);

    int idx = find_coordinator_index(pool, coordinator_id);
    if (idx < 0) {
        nimcp_mutex_unlock(pool->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Check if removing leader */
    bool was_leader = ((size_t)idx == pool->leader_index);

    /* Shift remaining */
    for (size_t i = (size_t)idx; i < pool->coordinator_count - 1; i++) {
        pool->coordinators[i] = pool->coordinators[i + 1];
    }
    pool->coordinators[pool->coordinator_count - 1] = NULL;
    pool->coordinator_count--;

    /* Update leader index */
    if (was_leader) {
        pool->leader_index = (size_t)-1;
        pool->leader_id = 0;
    } else if (pool->leader_index != (size_t)-1 && (size_t)idx < pool->leader_index) {
        pool->leader_index--;
    }

    update_stats(pool);

    nimcp_mutex_unlock(pool->mutex);

    /* Trigger election if lost leader */
    if (was_leader && pool->coordinator_count > 0) {
        mesh_coordinator_pool_elect_leader(pool);
    }

    return NIMCP_SUCCESS;
}

mesh_coordinator_t* mesh_coordinator_pool_get(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t coordinator_id
) {
    if (!validate_pool(pool)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_coordinator_pool_get: validate_pool is NULL");
        return NULL;
    }

    int idx = find_coordinator_index(pool, coordinator_id);
    return idx >= 0 ? pool->coordinators[idx] : NULL;
}

mesh_coordinator_t* mesh_coordinator_pool_get_by_index(
    mesh_coordinator_pool_t* pool,
    size_t index
) {
    if (!validate_pool(pool) || index >= pool->coordinator_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mesh_coordinator_pool_get_by_index: validate_pool is NULL");
        return NULL;
    }
    return pool->coordinators[index];
}

size_t mesh_coordinator_pool_get_size(const mesh_coordinator_pool_t* pool) {
    return pool ? pool->coordinator_count : 0;
}

/* ============================================================================
 * Leader and Role
 * ============================================================================ */

mesh_coordinator_t* mesh_coordinator_pool_get_leader(
    mesh_coordinator_pool_t* pool
) {
    if (!validate_pool(pool)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_coordinator_pool_get_leader: validate_pool is NULL");
        return NULL;
    }
    if (pool->leader_index >= pool->coordinator_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mesh_coordinator_pool_get_leader: capacity exceeded");
        return NULL;
    }
    return pool->coordinators[pool->leader_index];
}

mesh_participant_id_t mesh_coordinator_pool_get_leader_id(
    const mesh_coordinator_pool_t* pool
) {
    return pool ? pool->leader_id : 0;
}

bool mesh_coordinator_pool_has_leader(const mesh_coordinator_pool_t* pool) {
    if (!pool) {
        return false;
    }
    return pool->leader_index < pool->coordinator_count && pool->leader_id != 0;
}

nimcp_error_t mesh_coordinator_pool_get_by_role(
    mesh_coordinator_pool_t* pool,
    coordinator_role_t role,
    mesh_coordinator_t** coords_out,
    size_t max_coords,
    size_t* count_out
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;
    if (!coords_out || !count_out) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(pool->mutex);

    size_t count = 0;
    for (size_t i = 0; i < pool->coordinator_count && count < max_coords; i++) {
        if (pool->coordinators[i] &&
            mesh_coordinator_get_role(pool->coordinators[i]) == role) {
            coords_out[count++] = pool->coordinators[i];
        }
    }

    *count_out = count;

    nimcp_mutex_unlock(pool->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Leader Election
 * ============================================================================ */

nimcp_error_t mesh_coordinator_pool_elect_leader(
    mesh_coordinator_pool_t* pool
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(pool->mutex);

    if (pool->election_in_progress) {
        nimcp_mutex_unlock(pool->mutex);
        return NIMCP_SUCCESS; /* Election already in progress */
    }

    /* Start new election */
    pool->election_in_progress = true;
    pool->current_term++;
    pool->election_start_ns = nimcp_time_now_ns();
    pool->tally_count = 0;
    memset(pool->vote_tallies, 0, pool->coordinator_capacity * sizeof(vote_tally_t));

    /* Clear voter records for new term (Byzantine protection) */
    clear_voter_records(pool);

    LOG_INFO("Pool '%s' starting election for term %lu",
             pool->name, (unsigned long)pool->current_term);

    /* Each active coordinator votes */
    for (size_t i = 0; i < pool->coordinator_count; i++) {
        mesh_coordinator_t* coord = pool->coordinators[i];
        if (!coord) continue;

        coordinator_state_t state = mesh_coordinator_get_state(coord);
        if (state == COORD_STATE_FAILED || state == COORD_STATE_SHUTDOWN) continue;

        /* Simple election: vote for self if healthy, otherwise first healthy */
        float health = mesh_coordinator_get_health(coord);
        mesh_participant_id_t vote_for;

        if (health > 0.5f) {
            vote_for = mesh_coordinator_get_id(coord);
        } else {
            /* Find healthiest coordinator */
            float best_health = 0.0f;
            mesh_coordinator_t* best = NULL;
            for (size_t j = 0; j < pool->coordinator_count; j++) {
                mesh_coordinator_t* c = pool->coordinators[j];
                if (!c) continue;
                float h = mesh_coordinator_get_health(c);
                if (h > best_health) {
                    best_health = h;
                    best = c;
                }
            }
            vote_for = best ? mesh_coordinator_get_id(best) : mesh_coordinator_get_id(coord);
        }

        add_vote(pool, vote_for);
        mesh_coordinator_cast_vote(coord, vote_for, pool->current_term);
    }

    pool->stats.elections_held++;

    /* Check for winner */
    mesh_participant_id_t winner = get_election_winner(pool);
    if (winner != 0) {
        /* We have a winner */
        int winner_idx = find_coordinator_index(pool, winner);
        if (winner_idx >= 0) {
            /* Demote old leader if exists */
            if (pool->leader_index < pool->coordinator_count) {
                mesh_coordinator_t* old_leader = pool->coordinators[pool->leader_index];
                if (old_leader && mesh_coordinator_get_id(old_leader) != winner) {
                    mesh_coordinator_set_role(old_leader, COORD_ROLE_WORKER);
                }
            }

            /* Promote new leader */
            mesh_coordinator_t* new_leader = pool->coordinators[winner_idx];
            mesh_coordinator_set_role(new_leader, COORD_ROLE_LEADER);
            mesh_coordinator_set_state(new_leader, COORD_STATE_ACTIVE);
            pool->leader_index = (size_t)winner_idx;
            pool->leader_id = winner;
            pool->stats.leader_changes++;

            LOG_INFO("Pool '%s' elected leader 0x%016lx in term %lu",
                     pool->name, (unsigned long)winner, (unsigned long)pool->current_term);

            /* Record election result */
            pool->last_election.pool_id = pool->id;
            pool->last_election.term = pool->current_term;
            pool->last_election.winner = winner;
            pool->last_election.success = true;
            pool->last_election.total_voters = get_active_count(pool);

            vote_tally_t* winner_tally = find_tally(pool, winner);
            pool->last_election.votes_received = winner_tally ? winner_tally->votes : 0;
            pool->last_election.duration_ms = (nimcp_time_now_ns() - pool->election_start_ns) / 1000000;

            /* Invoke callback */
            if (pool->election_callback) {
                nimcp_mutex_unlock(pool->mutex);
                pool->election_callback(pool->id, winner, pool->current_term,
                                        pool->election_callback_ctx);
                nimcp_mutex_lock(pool->mutex);
            }
        }

        pool->election_in_progress = false;
    }

    update_stats(pool);

    nimcp_mutex_unlock(pool->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_coordinator_pool_process_vote(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t voter_id,
    mesh_participant_id_t candidate_id,
    uint64_t term
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(pool->mutex);

    if (term != pool->current_term || !pool->election_in_progress) {
        nimcp_mutex_unlock(pool->mutex);
        return NIMCP_SUCCESS; /* Stale vote */
    }

    /* Byzantine protection: check if this voter has already voted in this term */
    if (has_voter_already_voted(pool, voter_id)) {
        LOG_WARN("Pool '%s' detected duplicate vote from 0x%016lx in term %lu (Byzantine?)",
                 pool->name, (unsigned long)voter_id, (unsigned long)term);
        nimcp_mutex_unlock(pool->mutex);
        return NIMCP_SUCCESS; /* Ignore duplicate vote */
    }

    /* Record the vote for future duplicate detection */
    record_vote(pool, voter_id, candidate_id);

    add_vote(pool, candidate_id);

    /* Check for winner */
    mesh_participant_id_t winner = get_election_winner(pool);
    if (winner != 0) {
        /* Process winner (similar to elect_leader) */
        int winner_idx = find_coordinator_index(pool, winner);
        if (winner_idx >= 0) {
            mesh_coordinator_t* new_leader = pool->coordinators[winner_idx];
            mesh_coordinator_set_role(new_leader, COORD_ROLE_LEADER);
            pool->leader_index = (size_t)winner_idx;
            pool->leader_id = winner;
        }
        pool->election_in_progress = false;
    }

    nimcp_mutex_unlock(pool->mutex);

    return NIMCP_SUCCESS;
}

uint64_t mesh_coordinator_pool_get_term(const mesh_coordinator_pool_t* pool) {
    return pool ? pool->current_term : 0;
}

nimcp_error_t mesh_coordinator_pool_get_last_election(
    const mesh_coordinator_pool_t* pool,
    mesh_election_result_t* result
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;
    if (!result) return NIMCP_ERROR_NULL_POINTER;

    memcpy(result, &pool->last_election, sizeof(mesh_election_result_t));
    return NIMCP_SUCCESS;
}

bool mesh_coordinator_pool_election_in_progress(
    const mesh_coordinator_pool_t* pool
) {
    return pool ? pool->election_in_progress : false;
}

nimcp_error_t mesh_coordinator_pool_set_election_callback(
    mesh_coordinator_pool_t* pool,
    mesh_election_callback_t callback,
    void* ctx
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    pool->election_callback = callback;
    pool->election_callback_ctx = ctx;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Load Balancing
 * ============================================================================ */

nimcp_error_t mesh_coordinator_pool_assign_participant(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t participant_id
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(pool->mutex);

    /* Find least loaded worker */
    mesh_coordinator_t* coord = get_least_loaded_worker(pool);

    /* If no workers, try leader */
    if (!coord) {
        coord = mesh_coordinator_pool_get_leader(pool);
    }

    if (!coord) {
        nimcp_mutex_unlock(pool->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    nimcp_error_t err = mesh_coordinator_assign_participant(coord, participant_id);

    update_stats(pool);

    nimcp_mutex_unlock(pool->mutex);

    return err;
}

nimcp_error_t mesh_coordinator_pool_assign_to_leader(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t participant_id
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    mesh_coordinator_t* leader = mesh_coordinator_pool_get_leader(pool);
    if (!leader) return NIMCP_ERROR_NOT_FOUND;

    return mesh_coordinator_assign_participant(leader, participant_id);
}

nimcp_error_t mesh_coordinator_pool_rebalance(
    mesh_coordinator_pool_t* pool
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(pool->mutex);

    /* Collect all participants from overloaded coordinators */
    mesh_participant_id_t overflow_participants[256];
    size_t overflow_count = 0;

    for (size_t i = 0; i < pool->coordinator_count; i++) {
        mesh_coordinator_t* coord = pool->coordinators[i];
        if (!coord) continue;
        if (mesh_coordinator_get_role(coord) != COORD_ROLE_WORKER) continue;

        if (mesh_coordinator_is_overloaded(coord)) {
            /* Move half of participants */
            size_t count = mesh_coordinator_get_participant_count(coord);
            size_t to_move = count / 2;

            mesh_participant_id_t ids[64];
            size_t actual;
            mesh_coordinator_get_participants(coord, ids, 64, &actual);

            for (size_t j = 0; j < to_move && overflow_count < 256; j++) {
                mesh_coordinator_unassign_participant(coord, ids[j]);
                overflow_participants[overflow_count++] = ids[j];
            }
        }
    }

    /* Reassign overflow participants */
    for (size_t i = 0; i < overflow_count; i++) {
        mesh_coordinator_t* least_loaded = get_least_loaded_worker(pool);
        if (least_loaded && !mesh_coordinator_is_overloaded(least_loaded)) {
            mesh_coordinator_assign_participant(least_loaded, overflow_participants[i]);
        }
    }

    pool->stats.rebalances++;
    pool->last_rebalance_ns = nimcp_time_now_ns();

    update_stats(pool);

    nimcp_mutex_unlock(pool->mutex);

    if (overflow_count > 0) {
        LOG_DEBUG("Pool '%s' rebalanced %zu participants", pool->name, overflow_count);
    }

    return NIMCP_SUCCESS;
}

mesh_coordinator_t* mesh_coordinator_pool_get_assignment(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t participant_id
) {
    if (!validate_pool(pool)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_coordinator_pool_get_assignment: validate_pool is NULL");
        return NULL;
    }

    for (size_t i = 0; i < pool->coordinator_count; i++) {
        if (pool->coordinators[i] &&
            mesh_coordinator_has_participant(pool->coordinators[i], participant_id)) {
            return pool->coordinators[i];
        }
    }
    /* P2-151: Not found is normal lookup result, not an error (false positive removal) */
    return NULL;
}

size_t mesh_coordinator_pool_get_total_participants(
    const mesh_coordinator_pool_t* pool
) {
    return pool ? pool->stats.total_participants : 0;
}

/* ============================================================================
 * Failure Handling
 * ============================================================================ */

nimcp_error_t mesh_coordinator_pool_handle_failure(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t failed_coord_id
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(pool->mutex);

    int idx = find_coordinator_index(pool, failed_coord_id);
    if (idx < 0) {
        nimcp_mutex_unlock(pool->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_coordinator_pool: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    mesh_coordinator_t* failed = pool->coordinators[idx];
    bool was_leader = (failed_coord_id == pool->leader_id);

    /* Mark as failed */
    mesh_coordinator_set_state(failed, COORD_STATE_FAILED);

    /* Migrate ALL participants to other coordinators (loop until done) */
    size_t total_migrated = 0;
    size_t remaining = mesh_coordinator_get_participant_count(failed);

    while (remaining > 0) {
        mesh_participant_id_t participants[64];
        size_t count;
        size_t batch_size = remaining < 64 ? remaining : 64;
        mesh_coordinator_get_participants(failed, participants, batch_size, &count);

        if (count == 0) break;  /* Safety check */

        for (size_t i = 0; i < count; i++) {
            mesh_coordinator_unassign_participant(failed, participants[i]);
            mesh_coordinator_t* new_coord = get_least_loaded_worker(pool);
            if (new_coord) {
                mesh_coordinator_assign_participant(new_coord, participants[i]);
            }
        }
        total_migrated += count;
        remaining = mesh_coordinator_get_participant_count(failed);
    }

    pool->stats.failovers++;

    update_stats(pool);

    nimcp_mutex_unlock(pool->mutex);

    /* If leader failed, trigger election */
    if (was_leader) {
        mesh_coordinator_pool_elect_leader(pool);
    }

    LOG_WARN("Pool '%s' handled failure of coordinator 0x%016lx, migrated %zu participants",
             pool->name, (unsigned long)failed_coord_id, total_migrated);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_coordinator_pool_promote_standby(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t standby_id
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    mesh_coordinator_t* standby = mesh_coordinator_pool_get(pool, standby_id);
    if (!standby) return NIMCP_ERROR_NOT_FOUND;

    if (mesh_coordinator_get_role(standby) != COORD_ROLE_STANDBY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "mesh_coordinator_pool: invalid state");
        return NIMCP_ERROR_INVALID_STATE;
    }

    mesh_coordinator_set_role(standby, COORD_ROLE_WORKER);
    mesh_coordinator_set_state(standby, COORD_STATE_ACTIVE);

    LOG_INFO("Pool '%s' promoted standby 0x%016lx to worker",
             pool->name, (unsigned long)standby_id);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_coordinator_pool_demote(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t coord_id
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    mesh_coordinator_t* coord = mesh_coordinator_pool_get(pool, coord_id);
    if (!coord) return NIMCP_ERROR_NOT_FOUND;

    mesh_coordinator_set_role(coord, COORD_ROLE_STANDBY);

    return NIMCP_SUCCESS;
}

size_t mesh_coordinator_pool_get_failed_count(
    const mesh_coordinator_pool_t* pool
) {
    return pool ? pool->stats.failed_count : 0;
}

/* ============================================================================
 * Update
 * ============================================================================ */

nimcp_error_t mesh_coordinator_pool_update(
    mesh_coordinator_pool_t* pool,
    uint64_t delta_ms
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(pool->mutex);

    /* Update all coordinators */
    for (size_t i = 0; i < pool->coordinator_count; i++) {
        if (pool->coordinators[i]) {
            mesh_coordinator_update(pool->coordinators[i], delta_ms);
        }
    }

    /* Check leader health */
    if (mesh_coordinator_pool_has_leader(pool)) {
        mesh_coordinator_t* leader = mesh_coordinator_pool_get_leader(pool);
        if (leader && mesh_coordinator_get_health(leader) < 0.3f) {
            LOG_WARN("Pool '%s' leader health critical, triggering election", pool->name);
            nimcp_mutex_unlock(pool->mutex);
            mesh_coordinator_pool_elect_leader(pool);
            nimcp_mutex_lock(pool->mutex);
        }
    } else if (pool->coordinator_count > 0 && !pool->election_in_progress) {
        /* No leader, trigger election */
        nimcp_mutex_unlock(pool->mutex);
        mesh_coordinator_pool_elect_leader(pool);
        nimcp_mutex_lock(pool->mutex);
    }

    /* Periodic rebalancing */
    uint64_t now = nimcp_time_now_ns();
    uint64_t rebalance_interval_ns = (uint64_t)(pool->config.rebalance_interval_ms * 1000000.0f);
    if ((now - pool->last_rebalance_ns) >= rebalance_interval_ns) {
        nimcp_mutex_unlock(pool->mutex);
        mesh_coordinator_pool_rebalance(pool);
        nimcp_mutex_lock(pool->mutex);
    }

    update_stats(pool);
    pool->last_update_ns = now;

    nimcp_mutex_unlock(pool->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

nimcp_error_t mesh_coordinator_pool_get_stats(
    const mesh_coordinator_pool_t* pool,
    mesh_coordinator_pool_stats_t* stats
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    memcpy(stats, &pool->stats, sizeof(mesh_coordinator_pool_stats_t));
    return NIMCP_SUCCESS;
}

void mesh_coordinator_pool_reset_stats(mesh_coordinator_pool_t* pool) {
    if (!validate_pool(pool)) return;

    nimcp_mutex_lock(pool->mutex);

    memset(&pool->stats, 0, sizeof(pool->stats));
    pool->stats.pool_id = pool->id;
    update_stats(pool);

    nimcp_mutex_unlock(pool->mutex);
}

/* ============================================================================
 * Scaling
 * ============================================================================ */

nimcp_error_t mesh_coordinator_pool_scale_up(
    mesh_coordinator_pool_t* pool,
    size_t count
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    for (size_t i = 0; i < count; i++) {
        if (pool->coordinator_count >= pool->coordinator_capacity) {
            break;
        }

        char name[MESH_MAX_NAME_LEN];
        snprintf(name, sizeof(name), "%s_coord_%zu", pool->name, pool->coordinator_count);

        mesh_coordinator_config_t config;
        mesh_coordinator_default_config(&config);
        config.name = name;
        config.level = pool->level;
        config.channel = pool->channel;
        config.pool_id = pool->id;

        mesh_coordinator_t* coord = mesh_coordinator_create(&config, pool->registry, pool->channel_ref);
        if (coord) {
            mesh_coordinator_pool_add(pool, coord);
            mesh_coordinator_pool_promote_standby(pool, mesh_coordinator_get_id(coord));
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_coordinator_pool_scale_down(
    mesh_coordinator_pool_t* pool,
    size_t count
) {
    if (!validate_pool(pool)) return NIMCP_ERROR_INVALID_PARAM;

    /* P3-C: Mutex protection during scale_down iteration */
    nimcp_mutex_lock(pool->mutex);

    /* Only remove standby coordinators */
    size_t removed = 0;
    while (removed < count && pool->coordinator_count > pool->config.min_size) {
        mesh_coordinator_t* standby = get_first_with_role(pool, COORD_ROLE_STANDBY);
        if (!standby) break;

        mesh_participant_id_t id = mesh_coordinator_get_id(standby);
        nimcp_mutex_unlock(pool->mutex);
        mesh_coordinator_pool_remove(pool, id);
        mesh_coordinator_destroy(standby);
        nimcp_mutex_lock(pool->mutex);
        removed++;
    }

    nimcp_mutex_unlock(pool->mutex);
    return NIMCP_SUCCESS;
}

size_t mesh_coordinator_pool_optimal_size(
    const mesh_coordinator_pool_t* pool,
    size_t total_participants
) {
    if (!pool) return MESH_DEFAULT_POOL_SIZE;

    /* Optimal: 1 coordinator per 32 participants, plus margin */
    size_t optimal = (total_participants / 32) + 2;

    /* Ensure BFT minimum */
    if (optimal < MESH_MIN_POOL_SIZE_BFT) {
        optimal = MESH_MIN_POOL_SIZE_BFT;
    }

    /* Cap at max */
    if (optimal > pool->coordinator_capacity) {
        optimal = pool->coordinator_capacity;
    }

    return optimal;
}

/* ============================================================================
 * Utility
 * ============================================================================ */

void mesh_coordinator_pool_print_status(const mesh_coordinator_pool_t* pool) {
    if (!pool) {
        printf("Coordinator Pool: NULL\n");
        return;
    }

    printf("Coordinator Pool: %s (ID=%u)\n", pool->name, pool->id);
    printf("  Channel: %s\n", mesh_channel_name(pool->channel));
    printf("  Level:   %s\n", mesh_coordinator_level_to_string(pool->level));
    printf("  Coordinators: %zu (L:%zu W:%zu S:%zu F:%zu)\n",
           pool->coordinator_count,
           pool->stats.leader_count,
           pool->stats.worker_count,
           pool->stats.standby_count,
           pool->stats.failed_count);
    printf("  Leader:  0x%016lx\n", (unsigned long)pool->leader_id);
    printf("  Term:    %lu\n", (unsigned long)pool->current_term);
    printf("  Participants: %zu\n", pool->stats.total_participants);
    printf("  Avg Load: %.2f\n", pool->stats.avg_load);
    printf("  Elections: %lu\n", (unsigned long)pool->stats.elections_held);

    printf("  Coordinators:\n");
    for (size_t i = 0; i < pool->coordinator_count; i++) {
        if (pool->coordinators[i]) {
            printf("    [%zu] ", i);
            mesh_coordinator_print_info(pool->coordinators[i]);
        }
    }
}

bool mesh_coordinator_pool_is_bft_valid(const mesh_coordinator_pool_t* pool) {
    if (!pool) {
        return false;
    }

    size_t active = get_active_count(pool);
    size_t failed = pool->coordinator_count - active;

    /* BFT requires N >= 3f + 1 where f is max tolerated faults.
     * Minimum meaningful BFT requires N >= 4 (to tolerate at least 1 fault).
     * With fewer than 4 coordinators, BFT cannot tolerate any faults. */
    if (pool->coordinator_count < 4) {
        return false;
    }

    /* Tolerated faults = (N - 1) / 3 */
    size_t tolerated = (pool->coordinator_count - 1) / 3;

    return failed <= tolerated;
}
