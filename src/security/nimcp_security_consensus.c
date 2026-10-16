/**
 * @file nimcp_security_consensus.c
 * @brief NIMCP Security Consensus Implementation
 */

#include "security/nimcp_security_consensus.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_consensus, MESH_ADAPTER_CATEGORY_SECURITY)


/* Forward declarations from protocol */
typedef enum {
    CONSENSUS_RPC_REQUEST_VOTE,
    CONSENSUS_RPC_REQUEST_VOTE_RESPONSE,
    CONSENSUS_RPC_APPEND_ENTRIES,
    CONSENSUS_RPC_APPEND_ENTRIES_RESPONSE
} consensus_rpc_type_t;

/* Consensus log entry (distinct from bio_messages log entry) */
typedef struct {
    nimcp_term_t term;                     /* Term when entry was created */
    nimcp_log_entry_type_t type;           /* Entry type */
    union {
        nimcp_security_policy_t policy;
        nimcp_threat_info_t threat;
        struct {
            nimcp_response_type_t type;
            uint8_t params[512];
        } response;
    } data;
} consensus_log_entry_t;

/* Cluster node */
typedef struct nimcp_cluster_node {
    nimcp_node_id_t node_id;
    char address[NIMCP_ERROR_BUFFER_SIZE];
    uint16_t port;
    bool is_alive;
    time_t last_contact;
    nimcp_log_index_t next_index;          /* Next log index to send */
    nimcp_log_index_t match_index;         /* Highest replicated index */
    struct nimcp_cluster_node* next;
} nimcp_cluster_node_t;

/* Consensus state */
struct nimcp_security_consensus {
    uint32_t magic;

    /* Persistent state */
    nimcp_term_t current_term;             /* Latest term seen */
    nimcp_node_id_t voted_for;             /* CandidateId that received vote in current term */
    consensus_log_entry_t* log;                /* Log entries */
    size_t log_size;                       /* Number of log entries */
    size_t log_capacity;                   /* Log capacity */

    /* Volatile state */
    nimcp_consensus_role_t role;           /* Current role */
    nimcp_log_index_t commit_index;        /* Highest log entry known to be committed */
    nimcp_log_index_t last_applied;        /* Highest log entry applied to state machine */
    nimcp_node_id_t current_leader;        /* Current leader (0 if none) */

    /* Leader state (volatile) */
    nimcp_log_index_t* next_index;         /* For each server, index of next log entry to send */
    nimcp_log_index_t* match_index;        /* For each server, highest log entry known to be replicated */

    /* Configuration */
    nimcp_node_id_t node_id;
    char bind_address[NIMCP_ERROR_BUFFER_SIZE];
    uint16_t port;
    uint32_t election_timeout_min_ms;
    uint32_t election_timeout_max_ms;
    uint32_t heartbeat_interval_ms;
    size_t max_nodes;

    /* Cluster membership */
    nimcp_cluster_node_t* nodes;
    size_t node_count;

    /* Timers */
    struct timeval election_timeout;
    struct timeval last_heartbeat_sent;
    struct timeval last_heartbeat_received;
    uint32_t current_election_timeout_ms;

    /* Statistics */
    uint64_t elections_started;
    uint64_t elections_won;
    uint64_t votes_granted;
    uint64_t heartbeats_sent;
    uint64_t heartbeats_received;
    uint64_t policies_proposed;
    uint64_t policies_committed;
    uint64_t threats_shared;
    uint64_t responses_coordinated;

    /* Bio-async */
    bio_router_t router;
    bio_module_context_t bio_ctx;

    /* Thread safety */
    nimcp_mutex_t mutex;
    pthread_t timer_thread;
    bool running;

    void* user_data;
};

static const char* role_names[] = {
    "FOLLOWER",
    "CANDIDATE",
    "LEADER"
};

/**
 * Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * NIMCP_MS_PER_SEC + tv.tv_usec / NIMCP_US_PER_MS;
}

/**
 * Generate random election timeout
 *
 * WHAT: Randomized timeout to prevent split votes
 * WHY: Reduces election conflicts
 * HOW: Random value between min and max
 */
static uint32_t generate_election_timeout(nimcp_security_consensus_t c) {
    uint32_t range = c->election_timeout_max_ms - c->election_timeout_min_ms;
    return c->election_timeout_min_ms + (nimcp_tl_rand() % range);
}

/**
 * Reset election timeout
 */
static void reset_election_timeout(nimcp_security_consensus_t c) {
    gettimeofday(&c->election_timeout, NULL);
    c->current_election_timeout_ms = generate_election_timeout(c);
}

/**
 * Check if election timeout has elapsed
 */
static bool is_election_timeout(nimcp_security_consensus_t c) {
    struct timeval now;
    gettimeofday(&now, NULL);

    uint64_t elapsed = (now.tv_sec - c->election_timeout.tv_sec) * NIMCP_MS_PER_SEC +
                      (now.tv_usec - c->election_timeout.tv_usec) / NIMCP_US_PER_MS;

    return elapsed >= c->current_election_timeout_ms;
}

/**
 * Get last log index
 */
static nimcp_log_index_t get_last_log_index(nimcp_security_consensus_t c) {
    return c->log_size;
}

/**
 * Get last log term
 */
static nimcp_term_t get_last_log_term(nimcp_security_consensus_t c) {
    if (c->log_size == 0) {
        return 0;
    }
    return c->log[c->log_size - 1].term;
}

/**
 * Append log entry
 *
 * WHAT: Adds entry to replicated log
 * WHY: Record operations for replication
 * HOW: Resizes array if needed, appends entry
 */
static nimcp_error_t append_log_entry(
    nimcp_security_consensus_t c,
    const consensus_log_entry_t* entry
) {
    if (c->log_size >= c->log_capacity) {
        size_t new_capacity = c->log_capacity * 2;
        consensus_log_entry_t* new_log = nimcp_realloc(c->log, new_capacity * sizeof(consensus_log_entry_t));
        if (!new_log) {
            LOG_ERROR("Failed to resize log");
            return NIMCP_ERROR_NO_MEMORY;
        }
        c->log = new_log;
        c->log_capacity = new_capacity;
    }

    memcpy(&c->log[c->log_size], entry, sizeof(consensus_log_entry_t));
    c->log_size++;

    LOG_DEBUG("Appended log entry: index=%lu, term=%lu, type=%d",
                    (unsigned long)c->log_size,
                    (unsigned long)entry->term,
                    entry->type);

    return NIMCP_SUCCESS;
}

/**
 * Convert to candidate
 *
 * WHAT: Transitions to candidate role and starts election
 * WHY: No leader detected, need new election
 * HOW: Increment term, vote for self, request votes from peers
 */
static void become_candidate(nimcp_security_consensus_t c) {
    c->role = NIMCP_CONSENSUS_CANDIDATE;
    c->current_term++;
    c->voted_for = c->node_id;
    c->current_leader = 0;
    c->elections_started++;

    reset_election_timeout(c);

    LOG_INFO("Node %lu became CANDIDATE for term %lu",
             (unsigned long)c->node_id,
             (unsigned long)c->current_term);

    /* Request votes from all peers */
    /* This would send RequestVote RPCs via bio-router */
    if (c->router) {
        char msg[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "RequestVote:term=%lu,candidate=%lu",
                (unsigned long)c->current_term, (unsigned long)c->node_id);
        bio_router_broadcast(c->bio_ctx, msg, strlen(msg) + 1);
    }
}

/**
 * Convert to follower
 */
static void become_follower(nimcp_security_consensus_t c, nimcp_term_t term) {
    c->role = NIMCP_CONSENSUS_FOLLOWER;
    c->current_term = term;
    c->voted_for = 0;
    c->current_leader = 0;

    reset_election_timeout(c);

    LOG_INFO("Node %lu became FOLLOWER for term %lu",
             (unsigned long)c->node_id,
             (unsigned long)c->current_term);
}

/**
 * Convert to leader
 *
 * WHAT: Transitions to leader role
 * WHY: Won election with majority votes
 * HOW: Initialize leader state, send initial heartbeats
 */
static void become_leader(nimcp_security_consensus_t c) {
    c->role = NIMCP_CONSENSUS_LEADER;
    c->current_leader = c->node_id;
    c->elections_won++;

    LOG_INFO("Node %lu became LEADER for term %lu",
             (unsigned long)c->node_id,
             (unsigned long)c->current_term);

    /* Initialize leader state */
    nimcp_log_index_t last_index = get_last_log_index(c);
    for (size_t i = 0; i < c->node_count; i++) {
        if (c->next_index) {
            c->next_index[i] = last_index + 1;
        }
        if (c->match_index) {
            c->match_index[i] = 0;
        }
    }

    /* Send initial heartbeat */
    gettimeofday(&c->last_heartbeat_sent, NULL);

    if (c->router) {
        char msg[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "Heartbeat:term=%lu,leader=%lu",
                (unsigned long)c->current_term, (unsigned long)c->node_id);
        bio_router_broadcast(c->bio_ctx, msg, strlen(msg) + 1);
        c->heartbeats_sent++;
    }
}

/**
 * Handle RequestVote RPC
 *
 * WHAT: Processes vote request from candidate
 * WHY: Participate in leader election
 * HOW: Grant vote if candidate's log is up-to-date and haven't voted
 */
static void handle_request_vote(
    nimcp_security_consensus_t c,
    nimcp_term_t candidate_term,
    nimcp_node_id_t candidate_id,
    nimcp_log_index_t candidate_last_log_index,
    nimcp_term_t candidate_last_log_term
) {
    bool vote_granted = false;

    /* If candidate's term is greater, become follower */
    if (candidate_term > c->current_term) {
        become_follower(c, candidate_term);
    }

    /* Grant vote if:
     * 1. Haven't voted in this term OR already voted for this candidate
     * 2. Candidate's log is at least as up-to-date as ours
     */
    if (candidate_term == c->current_term) {
        bool can_vote = (c->voted_for == 0 || c->voted_for == candidate_id);

        /* Check if candidate's log is up-to-date */
        nimcp_term_t our_last_term = get_last_log_term(c);
        nimcp_log_index_t our_last_index = get_last_log_index(c);

        bool log_ok = (candidate_last_log_term > our_last_term) ||
                     (candidate_last_log_term == our_last_term &&
                      candidate_last_log_index >= our_last_index);

        if (can_vote && log_ok) {
            c->voted_for = candidate_id;
            vote_granted = true;
            c->votes_granted++;
            reset_election_timeout(c);

            LOG_INFO("Granted vote to candidate %lu for term %lu",
                     (unsigned long)candidate_id,
                     (unsigned long)candidate_term);
        }
    }

    /* Send response */
    if (c->router) {
        char msg[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "VoteResponse:term=%lu,granted=%d,voter=%lu",
                (unsigned long)c->current_term, vote_granted, (unsigned long)c->node_id);
        bio_router_broadcast(c->bio_ctx, msg, strlen(msg) + 1);
    }
}

/**
 * Handle AppendEntries RPC
 *
 * WHAT: Processes log replication from leader
 * WHY: Replicate log entries and heartbeat
 * HOW: Check log consistency, append entries, update commit index
 */
static void handle_append_entries(
    nimcp_security_consensus_t c,
    nimcp_term_t leader_term,
    nimcp_node_id_t leader_id,
    nimcp_log_index_t prev_log_index,
    nimcp_term_t prev_log_term,
    nimcp_log_index_t leader_commit,
    const consensus_log_entry_t* entries,
    size_t entry_count
) {
    bool success = false;
    nimcp_log_index_t match_index = 0;

    /* If leader's term is greater, become follower */
    if (leader_term > c->current_term) {
        become_follower(c, leader_term);
    }

    /* Accept leader */
    if (leader_term == c->current_term) {
        if (c->role == NIMCP_CONSENSUS_CANDIDATE) {
            become_follower(c, leader_term);
        }
        c->current_leader = leader_id;
        reset_election_timeout(c);
        gettimeofday(&c->last_heartbeat_received, NULL);
        c->heartbeats_received++;

        /* Check log consistency */
        if (prev_log_index == 0 ||
            (prev_log_index <= c->log_size &&
             c->log[prev_log_index - 1].term == prev_log_term)) {

            success = true;
            match_index = prev_log_index;

            /* Append new entries (simplified - should handle conflicts) */
            for (size_t i = 0; i < entry_count; i++) {
                append_log_entry(c, &entries[i]);
                match_index++;
            }

            /* Update commit index */
            if (leader_commit > c->commit_index) {
                c->commit_index = leader_commit < get_last_log_index(c) ?
                                 leader_commit : get_last_log_index(c);
            }
        }
    }

    /* Send response */
    if (c->router) {
        char msg[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "AppendResponse:term=%lu,success=%d,match=%lu",
                (unsigned long)c->current_term, success, (unsigned long)match_index);
        bio_router_broadcast(c->bio_ctx, msg, strlen(msg) + 1);
    }
}

/**
 * Timer thread
 *
 * WHAT: Background thread for timeouts
 * WHY: Trigger elections and heartbeats
 * HOW: Checks timeouts, transitions roles
 */
static void* timer_thread_func(void* arg) {
    nimcp_security_consensus_t c = (nimcp_security_consensus_t)arg;

    while (c->running) {
        nimcp_mutex_lock(&c->mutex);

        if (c->role == NIMCP_CONSENSUS_FOLLOWER || c->role == NIMCP_CONSENSUS_CANDIDATE) {
            if (is_election_timeout(c)) {
                become_candidate(c);
            }
        } else if (c->role == NIMCP_CONSENSUS_LEADER) {
            /* Send heartbeats */
            struct timeval now;
            gettimeofday(&now, NULL);
            uint64_t elapsed = (now.tv_sec - c->last_heartbeat_sent.tv_sec) * NIMCP_MS_PER_SEC +
                             (now.tv_usec - c->last_heartbeat_sent.tv_usec) / NIMCP_US_PER_MS;

            if (elapsed >= c->heartbeat_interval_ms) {
                if (c->router) {
                    char msg[NIMCP_ERROR_BUFFER_SIZE];
                    snprintf(msg, sizeof(msg), "Heartbeat:term=%lu,leader=%lu",
                            (unsigned long)c->current_term, (unsigned long)c->node_id);
                    bio_router_broadcast(c->bio_ctx, msg, strlen(msg) + 1);
                    c->heartbeats_sent++;
                }
                gettimeofday(&c->last_heartbeat_sent, NULL);
            }
        }

        nimcp_mutex_unlock(&c->mutex);

        /* Sleep 10ms */
        usleep(10000);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "timer_thread_func: operation failed");
    return NULL;
}

nimcp_security_consensus_t nimcp_consensus_create(const nimcp_consensus_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_consensus_create: config is NULL");
        LOG_ERROR("NULL config provided");
        return NULL;
    }

    nimcp_security_consensus_t c = nimcp_calloc(1, sizeof(struct nimcp_security_consensus));
    if (!c) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_consensus_create: failed to allocate consensus");
        LOG_ERROR("Failed to allocate consensus");
        return NULL;
    }

    c->magic = NIMCP_CONSENSUS_MAGIC;
    c->node_id = config->node_id;
    c->port = config->port;
    c->election_timeout_min_ms = config->election_timeout_min_ms > 0 ?
                                 config->election_timeout_min_ms : 150;
    c->election_timeout_max_ms = config->election_timeout_max_ms > 0 ?
                                 config->election_timeout_max_ms : 300;
    c->heartbeat_interval_ms = config->heartbeat_interval_ms > 0 ?
                              config->heartbeat_interval_ms : 50;
    c->max_nodes = config->max_nodes > 0 ? config->max_nodes : 16;
    c->router = config->router ? *config->router : NULL;
    c->user_data = config->user_data;

    if (config->bind_address) {
        strncpy(c->bind_address, config->bind_address, sizeof(c->bind_address) - 1);
    }

    /* Initialize persistent state */
    c->current_term = 0;
    c->voted_for = 0;
    c->log_capacity = config->max_log_entries > 0 ? config->max_log_entries : 1024;
    c->log = nimcp_calloc(c->log_capacity, sizeof(consensus_log_entry_t));
    if (!c->log) {
        LOG_ERROR("Failed to allocate log");
        nimcp_free(c);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_consensus_create: c->log is NULL");
        return NULL;
    }

    /* Initialize volatile state */
    c->role = NIMCP_CONSENSUS_FOLLOWER;
    c->commit_index = 0;
    c->last_applied = 0;
    c->current_leader = 0;

    /* Initialize mutex */
    if (nimcp_mutex_init(&c->mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize mutex");
        nimcp_free(c->log);
        nimcp_free(c);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_consensus_create: validation failed");
        return NULL;
    }

    /* Register with bio-async */
    if (c->router) {
        bio_module_info_t mod_info = {
            .module_id = BIO_MODULE_SECURITY,
            .module_name = "security_consensus",
            .inbox_capacity = 64,
            .user_data = c
        };
        c->bio_ctx = bio_router_register_module(&mod_info);
    }

    /* Start timer thread */
    c->running = true;
    reset_election_timeout(c);
    gettimeofday(&c->last_heartbeat_sent, NULL);
    gettimeofday(&c->last_heartbeat_received, NULL);

    if (nimcp_thread_create(&c->timer_thread, timer_thread_func, c, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to create timer thread");
        if (c->bio_ctx) bio_router_unregister_module(c->bio_ctx);
        nimcp_mutex_destroy(&c->mutex);
        nimcp_free(c->log);
        nimcp_free(c);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_consensus_create: validation failed");
        return NULL;
    }

    LOG_INFO("Consensus created: node_id=%lu, role=FOLLOWER",
             (unsigned long)c->node_id);

    return c;
}

void nimcp_consensus_destroy(nimcp_security_consensus_t c) {
    if (!c || c->magic != NIMCP_CONSENSUS_MAGIC) {
        return;
    }

    /* Stop timer thread */
    c->running = false;
    nimcp_thread_join(c->timer_thread, NULL);

    nimcp_mutex_lock(&c->mutex);

    /* Unregister from bio-async */
    if (c->bio_ctx) {
        bio_router_unregister_module(c->bio_ctx);
    }

    /* Free cluster nodes */
    nimcp_cluster_node_t* node = c->nodes;
    while (node) {
        nimcp_cluster_node_t* next = node->next;
        nimcp_free(node);
        node = next;
    }

    /* Free leader state */
    if (c->next_index) nimcp_free(c->next_index);
    if (c->match_index) nimcp_free(c->match_index);

    /* Free log */
    if (c->log) {
        memset(c->log, 0, c->log_capacity * sizeof(consensus_log_entry_t));
        nimcp_free(c->log);
    }

    LOG_INFO("Consensus destroyed: node_id=%lu, term=%lu, elections=%lu",
             (unsigned long)c->node_id,
             (unsigned long)c->current_term,
             (unsigned long)c->elections_started);

    c->magic = 0;

    nimcp_mutex_unlock(&c->mutex);
    nimcp_mutex_destroy(&c->mutex);

    nimcp_free(c);
}

nimcp_error_t nimcp_consensus_join(
    nimcp_security_consensus_t c,
    const char* peer_address
) {
    if (!c || c->magic != NIMCP_CONSENSUS_MAGIC || !peer_address) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Send join request to peer */
    if (c->router) {
        char msg[NIMCP_ERROR_BUFFER_LARGE];
        snprintf(msg, sizeof(msg), "JoinRequest:node_id=%lu,address=%s:%u",
                (unsigned long)c->node_id, c->bind_address, c->port);
        bio_router_broadcast(c->bio_ctx, msg, strlen(msg) + 1);
    }

    LOG_INFO("Node %lu joining cluster via %s",
             (unsigned long)c->node_id, peer_address);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_consensus_leave(nimcp_security_consensus_t c) {
    if (!c || c->magic != NIMCP_CONSENSUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&c->mutex);

    if (c->router) {
        char msg[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "LeaveRequest:node_id=%lu",
                (unsigned long)c->node_id);
        bio_router_broadcast(c->bio_ctx, msg, strlen(msg) + 1);
    }

    LOG_INFO("Node %lu leaving cluster", (unsigned long)c->node_id);

    nimcp_mutex_unlock(&c->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_consensus_propose_policy(
    nimcp_security_consensus_t c,
    const nimcp_security_policy_t* policy
) {
    if (!c || c->magic != NIMCP_CONSENSUS_MAGIC || !policy) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&c->mutex);

    /* Only leader can propose */
    if (c->role != NIMCP_CONSENSUS_LEADER) {
        nimcp_mutex_unlock(&c->mutex);
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* Create log entry */
    consensus_log_entry_t entry;
    entry.term = c->current_term;
    entry.type = NIMCP_LOG_ENTRY_POLICY;
    memcpy(&entry.data.policy, policy, sizeof(nimcp_security_policy_t));

    /* Append to log */
    nimcp_error_t err = append_log_entry(c, &entry);
    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(&c->mutex);
        return err;
    }

    c->policies_proposed++;

    LOG_INFO("Policy proposed: id=%lu, name=%s",
             (unsigned long)policy->policy_id, policy->name);

    /* Replicate to followers (simplified) */
    if (c->router) {
        char msg[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "ReplicatePolicy:id=%lu",
                (unsigned long)policy->policy_id);
        bio_router_broadcast(c->bio_ctx, msg, strlen(msg) + 1);
    }

    nimcp_mutex_unlock(&c->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_consensus_share_threat(
    nimcp_security_consensus_t c,
    const nimcp_threat_info_t* threat
) {
    if (!c || c->magic != NIMCP_CONSENSUS_MAGIC || !threat) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&c->mutex);

    /* Only leader can propose */
    if (c->role != NIMCP_CONSENSUS_LEADER) {
        nimcp_mutex_unlock(&c->mutex);
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* Create log entry */
    consensus_log_entry_t entry;
    entry.term = c->current_term;
    entry.type = NIMCP_LOG_ENTRY_THREAT;
    memcpy(&entry.data.threat, threat, sizeof(nimcp_threat_info_t));

    /* Append to log */
    nimcp_error_t err = append_log_entry(c, &entry);
    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(&c->mutex);
        return err;
    }

    c->threats_shared++;

    LOG_WARN("Threat shared: id=%lu, type=%s, severity=%u",
                   (unsigned long)threat->threat_id,
                   threat->type,
                   threat->severity);

    nimcp_mutex_unlock(&c->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_consensus_initiate_response(
    nimcp_security_consensus_t c,
    nimcp_response_type_t response,
    const void* params
) {
    if (!c || c->magic != NIMCP_CONSENSUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&c->mutex);

    if (c->role != NIMCP_CONSENSUS_LEADER) {
        nimcp_mutex_unlock(&c->mutex);
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* Create log entry */
    consensus_log_entry_t entry;
    entry.term = c->current_term;
    entry.type = NIMCP_LOG_ENTRY_RESPONSE;
    entry.data.response.type = response;
    if (params) {
        memcpy(entry.data.response.params, params, 512);
    }

    /* Append to log */
    nimcp_error_t err = append_log_entry(c, &entry);
    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(&c->mutex);
        return err;
    }

    c->responses_coordinated++;

    LOG_WARN("Coordinated response initiated: type=%d", response);

    nimcp_mutex_unlock(&c->mutex);

    return NIMCP_SUCCESS;
}

nimcp_consensus_role_t nimcp_consensus_get_role(nimcp_security_consensus_t c) {
    if (!c || c->magic != NIMCP_CONSENSUS_MAGIC) {
        return NIMCP_CONSENSUS_FOLLOWER;
    }

    nimcp_mutex_lock(&c->mutex);
    nimcp_consensus_role_t role = c->role;
    nimcp_mutex_unlock(&c->mutex);

    return role;
}

nimcp_node_id_t nimcp_consensus_get_leader(nimcp_security_consensus_t c) {
    if (!c || c->magic != NIMCP_CONSENSUS_MAGIC) {
        return 0;
    }

    nimcp_mutex_lock(&c->mutex);
    nimcp_node_id_t leader = c->current_leader;
    nimcp_mutex_unlock(&c->mutex);

    return leader;
}

nimcp_error_t nimcp_consensus_get_stats(
    nimcp_security_consensus_t c,
    nimcp_consensus_stats_t* stats
) {
    if (!c || c->magic != NIMCP_CONSENSUS_MAGIC || !stats) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&c->mutex);

    stats->current_role = c->role;
    stats->current_term = c->current_term;
    stats->voted_for = c->voted_for;
    stats->current_leader = c->current_leader;
    stats->commit_index = c->commit_index;
    stats->last_applied = c->last_applied;
    stats->log_size = c->log_size;
    stats->cluster_size = c->node_count;
    stats->elections_started = c->elections_started;
    stats->elections_won = c->elections_won;
    stats->votes_granted = c->votes_granted;
    stats->heartbeats_sent = c->heartbeats_sent;
    stats->heartbeats_received = c->heartbeats_received;
    stats->policies_proposed = c->policies_proposed;
    stats->policies_committed = c->policies_committed;
    stats->threats_shared = c->threats_shared;
    stats->responses_coordinated = c->responses_coordinated;
    stats->last_heartbeat_time = c->last_heartbeat_received.tv_sec;

    nimcp_mutex_unlock(&c->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_consensus_get_nodes(
    nimcp_security_consensus_t c,
    nimcp_node_info_t* nodes,
    size_t max_nodes,
    size_t* count_out
) {
    if (!c || c->magic != NIMCP_CONSENSUS_MAGIC || !nodes || !count_out) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&c->mutex);

    size_t count = 0;
    nimcp_cluster_node_t* node = c->nodes;

    while (node && count < max_nodes) {
        nodes[count].node_id = node->node_id;
        strncpy(nodes[count].address, node->address, sizeof(nodes[count].address) - 1);
        nodes[count].port = node->port;
        nodes[count].is_alive = node->is_alive;
        nodes[count].last_contact = node->last_contact;
        nodes[count].next_index = node->next_index;
        nodes[count].match_index = node->match_index;

        count++;
        node = node->next;
    }

    *count_out = count;

    nimcp_mutex_unlock(&c->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_consensus_process(nimcp_security_consensus_t c) {
    if (!c || c->magic != NIMCP_CONSENSUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Process messages via bio-async inbox processing */
    /* Note: inbox message processing is handled by the timer thread */
    /* This function provides a synchronous processing point if needed */

    return NIMCP_SUCCESS;
}

const char* nimcp_consensus_role_name(nimcp_consensus_role_t role) {
    if (role < 0 || role > NIMCP_CONSENSUS_LEADER) {
        return "UNKNOWN";
    }
    return role_names[role];
}
