/**
 * @file nimcp_mesh_coordinator.c
 * @brief Mesh Network Coordinator Implementation
 *
 * WHAT: Implementation of single coordinator node
 * WHY:  Coordinators manage participants and transactions
 * HOW:  State machine with role-based behavior
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_coordinator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Vote record for elections
 */
typedef struct vote_record {
    uint64_t term;                      /**< Term voted in */
    mesh_participant_id_t voted_for;    /**< Candidate voted for */
} vote_record_t;

/**
 * @brief Coordinator structure
 */
struct mesh_coordinator {
    uint32_t magic;                         /**< Magic for validation */
    char name[MESH_MAX_NAME_LEN];           /**< Coordinator name */
    mesh_participant_id_t id;               /**< Coordinator participant ID */

    /* Role and state */
    coordinator_role_t role;                /**< Current role */
    coordinator_state_t state;              /**< Current state */
    coordinator_level_t level;              /**< Hierarchy level */
    mesh_pool_id_t pool_id;                 /**< Pool ID (0 if not in pool) */
    mesh_channel_id_t channel;              /**< Associated channel */

    /* Assigned participants */
    mesh_participant_id_t* participants;    /**< Assigned participants */
    size_t participant_count;               /**< Assigned count */
    size_t participant_capacity;            /**< Array capacity */

    /* Election state */
    uint64_t current_term;                  /**< Current election term */
    vote_record_t vote;                     /**< Vote record */
    size_t votes_received;                  /**< Votes received (as candidate) */

    /* Timing */
    float heartbeat_interval_ms;            /**< Heartbeat interval */
    float election_timeout_ms;              /**< Election timeout */
    uint64_t last_heartbeat_ns;             /**< Last heartbeat received */
    uint64_t last_heartbeat_sent_ns;        /**< Last heartbeat sent */

    /* Health tracking */
    uint64_t consecutive_failures;          /**< Consecutive failures */
    uint64_t total_failures;                /**< Total failures */
    uint64_t total_recoveries;              /**< Total recoveries */
    float health_score;                     /**< Health [0,1] */

    /* Statistics */
    mesh_coordinator_stats_t stats;         /**< Statistics */

    /* References */
    mesh_participant_registry_t* registry;  /**< Participant registry */
    mesh_channel_t* channel_ref;            /**< Channel reference */

    /* Configuration */
    mesh_coordinator_config_t config;       /**< Configuration */

    /* Thread safety */
    nimcp_mutex_t* mutex;                   /**< Coordinator mutex */

    /* Timestamps */
    uint64_t creation_time_ns;              /**< Creation timestamp */
    uint64_t last_update_ns;                /**< Last update timestamp */
};

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/**
 * @brief Validate coordinator handle
 */
static bool validate_coordinator(const mesh_coordinator_t* coord) {
    return coord && coord->magic == NIMCP_MESH_MAGIC;
}

/**
 * @brief Compute load from participant count
 */
static float compute_load(const mesh_coordinator_t* coord) {
    if (!coord || coord->participant_capacity == 0) return 0.0f;
    return (float)coord->participant_count / (float)coord->participant_capacity;
}

/**
 * @brief Update health score based on failures
 */
static void update_health(mesh_coordinator_t* coord) {
    if (!coord) return;

    /* Health decreases with consecutive failures */
    if (coord->consecutive_failures == 0) {
        coord->health_score = 1.0f;
    } else if (coord->consecutive_failures >= MESH_MAX_CONSECUTIVE_FAILURES) {
        coord->health_score = 0.0f;
    } else {
        coord->health_score = 1.0f - ((float)coord->consecutive_failures /
                                       (float)MESH_MAX_CONSECUTIVE_FAILURES);
    }
}

/* ============================================================================
 * Level Timing
 * ============================================================================ */

void mesh_coordinator_get_level_timing(
    coordinator_level_t level,
    mesh_timing_t* timing
) {
    if (!timing) return;

    switch (level) {
    case COORD_LEVEL_SYSTEM:
        timing->base_interval_ms = 100.0f;
        timing->jitter_amplitude_ms = 50.0f;
        timing->min_interval_ms = 50.0f;
        timing->max_interval_ms = 200.0f;
        break;
    case COORD_LEVEL_HEMISPHERE:
        timing->base_interval_ms = 50.0f;
        timing->jitter_amplitude_ms = 25.0f;
        timing->min_interval_ms = 25.0f;
        timing->max_interval_ms = 100.0f;
        break;
    case COORD_LEVEL_LAYER:
        timing->base_interval_ms = 10.0f;
        timing->jitter_amplitude_ms = 5.0f;
        timing->min_interval_ms = 5.0f;
        timing->max_interval_ms = 20.0f;
        break;
    case COORD_LEVEL_ORDERING:
        timing->base_interval_ms = 5.0f;
        timing->jitter_amplitude_ms = 1.0f;
        timing->min_interval_ms = 3.0f;
        timing->max_interval_ms = 10.0f;
        break;
    default:
        timing->base_interval_ms = 50.0f;
        timing->jitter_amplitude_ms = 25.0f;
        timing->min_interval_ms = 25.0f;
        timing->max_interval_ms = 100.0f;
    }
}

const char* mesh_coordinator_level_to_string(coordinator_level_t level) {
    switch (level) {
    case COORD_LEVEL_SYSTEM:     return "SYSTEM";
    case COORD_LEVEL_HEMISPHERE: return "HEMISPHERE";
    case COORD_LEVEL_LAYER:      return "LAYER";
    case COORD_LEVEL_ORDERING:   return "ORDERING";
    default:                     return "UNKNOWN";
    }
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_error_t mesh_coordinator_default_config(mesh_coordinator_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));
    config->name = "coordinator";
    config->level = COORD_LEVEL_LAYER;
    config->channel = MESH_CHANNEL_SYSTEM;
    config->pool_id = 0;
    config->heartbeat_interval_ms = MESH_DEFAULT_HEARTBEAT_INTERVAL_MS;
    config->election_timeout_ms = MESH_DEFAULT_ELECTION_TIMEOUT_MS;
    config->sync_timeout_ms = MESH_DEFAULT_SYNC_TIMEOUT_MS;
    config->max_participants = MESH_MAX_PARTICIPANTS_PER_COORDINATOR;
    config->enable_logging = true;

    return NIMCP_SUCCESS;
}

mesh_coordinator_t* mesh_coordinator_create(
    const mesh_coordinator_config_t* config,
    mesh_participant_registry_t* registry,
    mesh_channel_t* channel
) {
    mesh_coordinator_config_t default_config;
    if (!config) {
        mesh_coordinator_default_config(&default_config);
        config = &default_config;
    }

    mesh_coordinator_t* coord = nimcp_calloc(1, sizeof(*coord));
    if (!coord) {
        LOG_ERROR("Failed to allocate coordinator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_coordinator_create: coord is NULL");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&coord->config, config, sizeof(*config));
    if (config->name) {
        strncpy(coord->name, config->name, MESH_MAX_NAME_LEN - 1);
    }
    coord->level = config->level;
    coord->channel = config->channel;
    coord->pool_id = config->pool_id;
    coord->heartbeat_interval_ms = config->heartbeat_interval_ms;
    coord->election_timeout_ms = config->election_timeout_ms;

    /* Allocate participant array */
    coord->participant_capacity = config->max_participants;
    coord->participants = nimcp_calloc(coord->participant_capacity,
                                        sizeof(mesh_participant_id_t));
    if (!coord->participants) {
        LOG_ERROR("Failed to allocate participant array");
        nimcp_free(coord);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_coordinator_create: coord->participants is NULL");
        return NULL;
    }

    /* Create mutex */
    coord->mutex = nimcp_mutex_create(NULL);
    if (!coord->mutex) {
        LOG_ERROR("Failed to create coordinator mutex");
        nimcp_free(coord->participants);
        nimcp_free(coord);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_coordinator_create: coord->mutex is NULL");
        return NULL;
    }

    /* Initialize state */
    coord->magic = NIMCP_MESH_MAGIC;
    coord->role = COORD_ROLE_STANDBY;
    coord->state = COORD_STATE_INIT;
    coord->registry = registry;
    coord->channel_ref = channel;
    coord->current_term = 0;
    coord->health_score = 1.0f;
    coord->creation_time_ns = nimcp_time_now_ns();
    coord->last_update_ns = coord->creation_time_ns;
    coord->last_heartbeat_ns = coord->creation_time_ns;
    coord->last_heartbeat_sent_ns = coord->creation_time_ns;

    /* Generate coordinator ID (P1-32: atomic to prevent data race) */
    static _Atomic uint32_t coord_id_counter = 1000;
    uint32_t my_id = atomic_fetch_add(&coord_id_counter, 1);
    coord->id = mesh_make_participant_id(
        config->channel,
        MESH_PARTICIPANT_COORDINATOR,
        my_id
    );

    /* Register as participant */
    if (registry) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);
        strncpy(iface.module_name, coord->name, MESH_MAX_NAME_LEN);
        iface.type = MESH_PARTICIPANT_COORDINATOR;
        iface.home_channel = config->channel;

        mesh_participant_config_t p_config;
        mesh_participant_config_init(&p_config);
        p_config.module_name = coord->name;
        p_config.type = MESH_PARTICIPANT_COORDINATOR;
        p_config.home_channel = config->channel;

        mesh_participant_id_t reg_id;
        if (mesh_participant_register(registry, &iface, &p_config, &reg_id) == NIMCP_SUCCESS) {
            coord->id = reg_id;
        }
    }

    /* Initialize statistics */
    memset(&coord->stats, 0, sizeof(coord->stats));
    coord->stats.id = coord->id;
    coord->stats.role = coord->role;
    coord->stats.state = coord->state;

    LOG_INFO("Created coordinator '%s' (id=0x%016lx, level=%s)",
             coord->name, (unsigned long)coord->id,
             mesh_coordinator_level_to_string(coord->level));

    return coord;
}

void mesh_coordinator_destroy(mesh_coordinator_t* coord) {
    if (!coord) return;

    if (coord->registry) {
        mesh_participant_unregister(coord->registry, coord->id);
    }

    if (coord->participants) {
        nimcp_free(coord->participants);
    }

    if (coord->mutex) {
        nimcp_mutex_destroy(coord->mutex);
    }

    LOG_INFO("Destroyed coordinator '%s'", coord->name);

    coord->magic = 0;
    nimcp_free(coord);
}

mesh_participant_id_t mesh_coordinator_get_id(const mesh_coordinator_t* coord) {
    return coord ? coord->id : 0;
}

const char* mesh_coordinator_get_name(const mesh_coordinator_t* coord) {
    return coord ? coord->name : NULL;
}

/* ============================================================================
 * Role and State
 * ============================================================================ */

coordinator_role_t mesh_coordinator_get_role(const mesh_coordinator_t* coord) {
    return coord ? coord->role : COORD_ROLE_NONE;
}

nimcp_error_t mesh_coordinator_set_role(
    mesh_coordinator_t* coord,
    coordinator_role_t role
) {
    if (!validate_coordinator(coord)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    coordinator_role_t old_role = coord->role;
    coord->role = role;
    coord->stats.role = role;

    nimcp_mutex_unlock(coord->mutex);

    if (old_role != role) {
        LOG_INFO("Coordinator '%s' role changed: %s -> %s",
                 coord->name,
                 mesh_coordinator_role_to_string(old_role),
                 mesh_coordinator_role_to_string(role));
    }

    return NIMCP_SUCCESS;
}

coordinator_state_t mesh_coordinator_get_state(const mesh_coordinator_t* coord) {
    return coord ? coord->state : COORD_STATE_INIT;
}

nimcp_error_t mesh_coordinator_set_state(
    mesh_coordinator_t* coord,
    coordinator_state_t state
) {
    if (!validate_coordinator(coord)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    coordinator_state_t old_state = coord->state;
    coord->state = state;
    coord->stats.state = state;

    nimcp_mutex_unlock(coord->mutex);

    if (old_state != state) {
        LOG_DEBUG("Coordinator '%s' state changed: %s -> %s",
                  coord->name,
                  mesh_coordinator_state_to_string(old_state),
                  mesh_coordinator_state_to_string(state));
    }

    return NIMCP_SUCCESS;
}

coordinator_level_t mesh_coordinator_get_level(const mesh_coordinator_t* coord) {
    return coord ? coord->level : COORD_LEVEL_LAYER;
}

/* ============================================================================
 * Participant Assignment
 * ============================================================================ */

nimcp_error_t mesh_coordinator_assign_participant(
    mesh_coordinator_t* coord,
    mesh_participant_id_t participant_id
) {
    if (!validate_coordinator(coord)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    /* Check if already assigned */
    for (size_t i = 0; i < coord->participant_count; i++) {
        if (coord->participants[i] == participant_id) {
            nimcp_mutex_unlock(coord->mutex);
            return NIMCP_SUCCESS;
        }
    }

    /* Check capacity */
    if (coord->participant_count >= coord->participant_capacity) {
        nimcp_mutex_unlock(coord->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_coordinator: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    coord->participants[coord->participant_count++] = participant_id;
    coord->stats.assigned_participants = coord->participant_count;
    coord->stats.current_load = compute_load(coord);

    nimcp_mutex_unlock(coord->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_coordinator_unassign_participant(
    mesh_coordinator_t* coord,
    mesh_participant_id_t participant_id
) {
    if (!validate_coordinator(coord)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    bool found = false;
    for (size_t i = 0; i < coord->participant_count; i++) {
        if (coord->participants[i] == participant_id) {
            /* Shift remaining */
            for (size_t j = i; j < coord->participant_count - 1; j++) {
                coord->participants[j] = coord->participants[j + 1];
            }
            coord->participant_count--;
            found = true;
            break;
        }
    }

    coord->stats.assigned_participants = coord->participant_count;
    coord->stats.current_load = compute_load(coord);

    nimcp_mutex_unlock(coord->mutex);

    return found ? NIMCP_SUCCESS : NIMCP_ERROR_NOT_FOUND;
}

bool mesh_coordinator_has_participant(
    const mesh_coordinator_t* coord,
    mesh_participant_id_t participant_id
) {
    if (!validate_coordinator(coord)) {
        return false;
    }

    /* P2-150: Mutex protection for participant array access */
    nimcp_mutex_lock(((mesh_coordinator_t*)coord)->mutex);

    bool found = false;
    for (size_t i = 0; i < coord->participant_count; i++) {
        if (coord->participants[i] == participant_id) {
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(((mesh_coordinator_t*)coord)->mutex);
    return found;
}

size_t mesh_coordinator_get_participant_count(const mesh_coordinator_t* coord) {
    return coord ? coord->participant_count : 0;
}

nimcp_error_t mesh_coordinator_get_participants(
    const mesh_coordinator_t* coord,
    mesh_participant_id_t* ids_out,
    size_t max_ids,
    size_t* count_out
) {
    if (!validate_coordinator(coord)) return NIMCP_ERROR_INVALID_PARAM;
    if (!ids_out || !count_out) return NIMCP_ERROR_NULL_POINTER;

    /* P2-150: Mutex protection for participant array access */
    nimcp_mutex_lock(((mesh_coordinator_t*)coord)->mutex);

    size_t count = coord->participant_count < max_ids ?
                   coord->participant_count : max_ids;
    memcpy(ids_out, coord->participants, count * sizeof(mesh_participant_id_t));
    *count_out = count;

    nimcp_mutex_unlock(((mesh_coordinator_t*)coord)->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Load and Health
 * ============================================================================ */

float mesh_coordinator_get_load(const mesh_coordinator_t* coord) {
    return coord ? compute_load(coord) : 0.0f;
}

float mesh_coordinator_get_health(const mesh_coordinator_t* coord) {
    return coord ? coord->health_score : 0.0f;
}

bool mesh_coordinator_is_overloaded(const mesh_coordinator_t* coord) {
    return coord ? (compute_load(coord) > MESH_COORDINATOR_LOAD_THRESHOLD) : false;
}

void mesh_coordinator_report_failure(
    mesh_coordinator_t* coord,
    nimcp_error_t error
) {
    if (!validate_coordinator(coord)) return;

    nimcp_mutex_lock(coord->mutex);

    coord->consecutive_failures++;
    coord->total_failures++;
    coord->stats.consecutive_failures = coord->consecutive_failures;
    update_health(coord);

    if (coord->health_score == 0.0f && coord->state != COORD_STATE_FAILED) {
        coord->state = COORD_STATE_FAILED;
        coord->stats.state = COORD_STATE_FAILED;
        LOG_ERROR("Coordinator '%s' marked as FAILED after %lu consecutive failures",
                  coord->name, (unsigned long)coord->consecutive_failures);
    }

    nimcp_mutex_unlock(coord->mutex);

    (void)error; /* May be used for categorization later */
}

void mesh_coordinator_report_recovery(mesh_coordinator_t* coord) {
    if (!validate_coordinator(coord)) return;

    nimcp_mutex_lock(coord->mutex);

    coord->consecutive_failures = 0;
    coord->total_recoveries++;
    coord->stats.consecutive_failures = 0;
    coord->stats.failures_recovered++;
    update_health(coord);

    if (coord->state == COORD_STATE_FAILED) {
        coord->state = COORD_STATE_ACTIVE;
        coord->stats.state = COORD_STATE_ACTIVE;
        LOG_INFO("Coordinator '%s' recovered from FAILED state", coord->name);
    }

    nimcp_mutex_unlock(coord->mutex);
}

/* ============================================================================
 * Heartbeat and Election
 * ============================================================================ */

nimcp_error_t mesh_coordinator_send_heartbeat(mesh_coordinator_t* coord) {
    if (!validate_coordinator(coord)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    coord->last_heartbeat_sent_ns = nimcp_time_now_ns();
    coord->stats.heartbeats_sent++;

    nimcp_mutex_unlock(coord->mutex);

    /* TODO: Actually send heartbeat to pool peers */

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_coordinator_receive_heartbeat(
    mesh_coordinator_t* coord,
    mesh_participant_id_t from_id,
    uint64_t term
) {
    if (!validate_coordinator(coord)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    coord->last_heartbeat_ns = nimcp_time_now_ns();

    /* Update term if newer */
    if (term > coord->current_term) {
        coord->current_term = term;
        /* Step down if we were candidate/leader and see higher term */
        if (coord->role == COORD_ROLE_LEADER && coord->state == COORD_STATE_ELECTION) {
            coord->role = COORD_ROLE_FOLLOWER;
            coord->state = COORD_STATE_ACTIVE;
        }
    }

    nimcp_mutex_unlock(coord->mutex);

    (void)from_id; /* May be used for leader tracking */

    return NIMCP_SUCCESS;
}

uint64_t mesh_coordinator_get_last_heartbeat(const mesh_coordinator_t* coord) {
    return coord ? coord->last_heartbeat_ns : 0;
}

bool mesh_coordinator_heartbeat_timed_out(const mesh_coordinator_t* coord) {
    if (!validate_coordinator(coord)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_coordinator_heartbeat_timed_out: validate_coordinator is NULL");
        return false;
    }

    uint64_t now = nimcp_time_now_ns();
    uint64_t timeout_ns = (uint64_t)(coord->election_timeout_ms * 1000000.0f);
    return (now - coord->last_heartbeat_ns) > timeout_ns;
}

nimcp_error_t mesh_coordinator_request_vote(
    mesh_coordinator_t* coord,
    uint64_t term
) {
    if (!validate_coordinator(coord)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    coord->state = COORD_STATE_ELECTION;
    coord->current_term = term;
    coord->votes_received = 1; /* Vote for self */
    coord->vote.term = term;
    coord->vote.voted_for = coord->id;
    coord->stats.elections_participated++;

    nimcp_mutex_unlock(coord->mutex);

    LOG_DEBUG("Coordinator '%s' requesting votes for term %lu",
              coord->name, (unsigned long)term);

    /* TODO: Actually send vote requests to pool peers */

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_coordinator_cast_vote(
    mesh_coordinator_t* coord,
    mesh_participant_id_t candidate_id,
    uint64_t term
) {
    if (!validate_coordinator(coord)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    /* Only vote if haven't voted in this term */
    if (coord->vote.term < term) {
        coord->vote.term = term;
        coord->vote.voted_for = candidate_id;
        coord->current_term = term;
        LOG_DEBUG("Coordinator '%s' voted for 0x%016lx in term %lu",
                  coord->name, (unsigned long)candidate_id, (unsigned long)term);
    }

    nimcp_mutex_unlock(coord->mutex);

    return NIMCP_SUCCESS;
}

uint64_t mesh_coordinator_get_term(const mesh_coordinator_t* coord) {
    return coord ? coord->current_term : 0;
}

void mesh_coordinator_set_term(mesh_coordinator_t* coord, uint64_t term) {
    if (!validate_coordinator(coord)) return;

    nimcp_mutex_lock(coord->mutex);
    if (term > coord->current_term) {
        coord->current_term = term;
    }
    nimcp_mutex_unlock(coord->mutex);
}

/* ============================================================================
 * Update
 * ============================================================================ */

nimcp_error_t mesh_coordinator_update(
    mesh_coordinator_t* coord,
    uint64_t delta_ms
) {
    if (!validate_coordinator(coord)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    uint64_t now = nimcp_time_now_ns();

    /* Send heartbeat if leader */
    if (coord->role == COORD_ROLE_LEADER) {
        uint64_t hb_interval_ns = (uint64_t)(coord->heartbeat_interval_ms * 1000000.0f);
        if ((now - coord->last_heartbeat_sent_ns) >= hb_interval_ns) {
            nimcp_mutex_unlock(coord->mutex);
            mesh_coordinator_send_heartbeat(coord);
            nimcp_mutex_lock(coord->mutex);
        }
    }

    /* Update stats */
    coord->stats.current_load = compute_load(coord);
    coord->stats.health_score = coord->health_score;
    coord->stats.uptime_ms = (now - coord->creation_time_ns) / 1000000;

    coord->last_update_ns = now;

    nimcp_mutex_unlock(coord->mutex);

    (void)delta_ms;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

nimcp_error_t mesh_coordinator_get_stats(
    const mesh_coordinator_t* coord,
    mesh_coordinator_stats_t* stats
) {
    if (!validate_coordinator(coord)) return NIMCP_ERROR_INVALID_PARAM;
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    memcpy(stats, &coord->stats, sizeof(mesh_coordinator_stats_t));
    return NIMCP_SUCCESS;
}

void mesh_coordinator_reset_stats(mesh_coordinator_t* coord) {
    if (!validate_coordinator(coord)) return;

    nimcp_mutex_lock(coord->mutex);

    memset(&coord->stats, 0, sizeof(coord->stats));
    coord->stats.id = coord->id;
    coord->stats.role = coord->role;
    coord->stats.state = coord->state;
    coord->stats.assigned_participants = coord->participant_count;
    coord->stats.current_load = compute_load(coord);
    coord->stats.health_score = coord->health_score;

    nimcp_mutex_unlock(coord->mutex);
}

/* ============================================================================
 * Utility
 * ============================================================================ */

void mesh_coordinator_print_info(const mesh_coordinator_t* coord) {
    if (!coord) {
        printf("Coordinator: NULL\n");
        return;
    }

    printf("Coordinator: %s\n", coord->name);
    printf("  ID:          0x%016lx\n", (unsigned long)coord->id);
    printf("  Role:        %s\n", mesh_coordinator_role_to_string(coord->role));
    printf("  State:       %s\n", mesh_coordinator_state_to_string(coord->state));
    printf("  Level:       %s\n", mesh_coordinator_level_to_string(coord->level));
    printf("  Channel:     %s\n", mesh_channel_name(coord->channel));
    printf("  Participants: %zu / %zu\n", coord->participant_count, coord->participant_capacity);
    printf("  Load:        %.2f\n", compute_load(coord));
    printf("  Health:      %.2f\n", coord->health_score);
    printf("  Term:        %lu\n", (unsigned long)coord->current_term);
}
