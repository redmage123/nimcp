/**
 * @file nimcp_consensus_protocol.c
 * @brief NIMCP Consensus Protocol Implementation (Raft-like)
 *
 * WHAT: Raft consensus protocol for distributed security
 * WHY: Provide strong consistency for security decisions
 * HOW: Leader election, log replication, safety guarantees
 */

#include "security/nimcp_security_consensus.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(consensus_protocol, MESH_ADAPTER_CATEGORY_SECURITY)


/* RPC message types */
typedef enum {
    NIMCP_RPC_REQUEST_VOTE,
    NIMCP_RPC_REQUEST_VOTE_RESPONSE,
    NIMCP_RPC_APPEND_ENTRIES,
    NIMCP_RPC_APPEND_ENTRIES_RESPONSE,
    NIMCP_RPC_INSTALL_SNAPSHOT,
    NIMCP_RPC_JOIN_CLUSTER,
    NIMCP_RPC_LEAVE_CLUSTER
} nimcp_rpc_type_t;

/* RequestVote RPC */
typedef struct {
    nimcp_term_t term;                     /* Candidate's term */
    nimcp_node_id_t candidate_id;          /* Candidate requesting vote */
    nimcp_log_index_t last_log_index;      /* Index of candidate's last log entry */
    nimcp_term_t last_log_term;            /* Term of candidate's last log entry */
} nimcp_request_vote_t;

typedef struct {
    nimcp_term_t term;                     /* Current term */
    bool vote_granted;                     /* True means candidate received vote */
} nimcp_request_vote_response_t;

/* AppendEntries RPC */
typedef struct {
    nimcp_term_t term;                     /* Leader's term */
    nimcp_node_id_t leader_id;             /* Leader ID */
    nimcp_log_index_t prev_log_index;      /* Index of log entry before new ones */
    nimcp_term_t prev_log_term;            /* Term of prev_log_index entry */
    nimcp_log_index_t leader_commit;       /* Leader's commit_index */
    size_t entry_count;                    /* Number of entries */
    /* Followed by log entries */
} nimcp_append_entries_t;

typedef struct {
    nimcp_term_t term;                     /* Current term */
    bool success;                          /* True if follower matched prev_log */
    nimcp_log_index_t match_index;         /* Highest index replicated */
} nimcp_append_entries_response_t;

/**
 * Serialize RequestVote RPC
 *
 * WHAT: Serializes vote request to buffer
 * WHY: Send over network
 * HOW: Simple binary format
 */
size_t serialize_request_vote(
    const nimcp_request_vote_t* req,
    uint8_t* buffer,
    size_t buffer_size
) {
    if (!req || !buffer || buffer_size < sizeof(nimcp_rpc_type_t) + sizeof(nimcp_request_vote_t)) {
        return 0;
    }

    size_t offset = 0;

    /* RPC type */
    nimcp_rpc_type_t type = NIMCP_RPC_REQUEST_VOTE;
    memcpy(buffer + offset, &type, sizeof(type));
    offset += sizeof(type);

    /* Request data */
    memcpy(buffer + offset, req, sizeof(nimcp_request_vote_t));
    offset += sizeof(nimcp_request_vote_t);

    return offset;
}

/**
 * Deserialize RequestVote RPC
 *
 * WHAT: Deserializes vote request from buffer
 * WHY: Receive from network
 * HOW: Parse binary format
 */
bool deserialize_request_vote(
    const uint8_t* buffer,
    size_t buffer_size,
    nimcp_request_vote_t* req
) {
    if (!buffer || !req || buffer_size < sizeof(nimcp_rpc_type_t) + sizeof(nimcp_request_vote_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_request_vote: required parameter is NULL (buffer, req)");
        return false;
    }

    size_t offset = sizeof(nimcp_rpc_type_t);  /* Skip type */
    memcpy(req, buffer + offset, sizeof(nimcp_request_vote_t));

    return true;
}

/**
 * Serialize RequestVote response
 *
 * WHAT: Serializes vote response to buffer
 * WHY: Send over network
 * HOW: Simple binary format
 */
size_t serialize_request_vote_response(
    const nimcp_request_vote_response_t* resp,
    uint8_t* buffer,
    size_t buffer_size
) {
    if (!resp || !buffer || buffer_size < sizeof(nimcp_rpc_type_t) + sizeof(nimcp_request_vote_response_t)) {
        return 0;
    }

    size_t offset = 0;

    nimcp_rpc_type_t type = NIMCP_RPC_REQUEST_VOTE_RESPONSE;
    memcpy(buffer + offset, &type, sizeof(type));
    offset += sizeof(type);

    memcpy(buffer + offset, resp, sizeof(nimcp_request_vote_response_t));
    offset += sizeof(nimcp_request_vote_response_t);

    return offset;
}

/**
 * Deserialize RequestVote response
 */
bool deserialize_request_vote_response(
    const uint8_t* buffer,
    size_t buffer_size,
    nimcp_request_vote_response_t* resp
) {
    if (!buffer || !resp || buffer_size < sizeof(nimcp_rpc_type_t) + sizeof(nimcp_request_vote_response_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_request_vote_response: required parameter is NULL (buffer, resp)");
        return false;
    }

    size_t offset = sizeof(nimcp_rpc_type_t);
    memcpy(resp, buffer + offset, sizeof(nimcp_request_vote_response_t));

    return true;
}

/**
 * Serialize AppendEntries RPC
 *
 * WHAT: Serializes append entries to buffer
 * WHY: Send over network
 * HOW: Header + variable-length entries
 */
size_t serialize_append_entries(
    const nimcp_append_entries_t* req,
    const void* entries,
    size_t entry_size,
    uint8_t* buffer,
    size_t buffer_size
) {
    if (!req || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "serialize_append_entries: invalid parameters");

            return 0;
    }

    size_t needed = sizeof(nimcp_rpc_type_t) + sizeof(nimcp_append_entries_t) +
                   (req->entry_count * entry_size);
    if (buffer_size < needed) {
        return 0;
    }

    size_t offset = 0;

    nimcp_rpc_type_t type = NIMCP_RPC_APPEND_ENTRIES;
    memcpy(buffer + offset, &type, sizeof(type));
    offset += sizeof(type);

    memcpy(buffer + offset, req, sizeof(nimcp_append_entries_t));
    offset += sizeof(nimcp_append_entries_t);

    if (req->entry_count > 0 && entries) {
        memcpy(buffer + offset, entries, req->entry_count * entry_size);
        offset += req->entry_count * entry_size;
    }

    return offset;
}

/**
 * Deserialize AppendEntries RPC
 */
bool deserialize_append_entries(
    const uint8_t* buffer,
    size_t buffer_size,
    nimcp_append_entries_t* req,
    void* entries,
    size_t max_entries,
    size_t entry_size
) {
    if (!buffer || !req) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "deserialize_append_entries: invalid parameters");

            return false;
    }

    if (buffer_size < sizeof(nimcp_rpc_type_t) + sizeof(nimcp_append_entries_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_append_entries: validation failed");
        return false;
    }

    size_t offset = sizeof(nimcp_rpc_type_t);
    memcpy(req, buffer + offset, sizeof(nimcp_append_entries_t));
    offset += sizeof(nimcp_append_entries_t);

    if (req->entry_count > 0) {
        if (!entries || req->entry_count > max_entries) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                    "if: invalid parameters");

                return false;
        }

        size_t entries_size = req->entry_count * entry_size;
        if (buffer_size < offset + entries_size) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_append_entries: validation failed");
            return false;
        }

        memcpy(entries, buffer + offset, entries_size);
    }

    return true;
}

/**
 * Serialize AppendEntries response
 */
size_t serialize_append_entries_response(
    const nimcp_append_entries_response_t* resp,
    uint8_t* buffer,
    size_t buffer_size
) {
    if (!resp || !buffer || buffer_size < sizeof(nimcp_rpc_type_t) + sizeof(nimcp_append_entries_response_t)) {
        return 0;
    }

    size_t offset = 0;

    nimcp_rpc_type_t type = NIMCP_RPC_APPEND_ENTRIES_RESPONSE;
    memcpy(buffer + offset, &type, sizeof(type));
    offset += sizeof(type);

    memcpy(buffer + offset, resp, sizeof(nimcp_append_entries_response_t));
    offset += sizeof(nimcp_append_entries_response_t);

    return offset;
}

/**
 * Deserialize AppendEntries response
 */
bool deserialize_append_entries_response(
    const uint8_t* buffer,
    size_t buffer_size,
    nimcp_append_entries_response_t* resp
) {
    if (!buffer || !resp || buffer_size < sizeof(nimcp_rpc_type_t) + sizeof(nimcp_append_entries_response_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_append_entries_response: required parameter is NULL (buffer, resp)");
        return false;
    }

    size_t offset = sizeof(nimcp_rpc_type_t);
    memcpy(resp, buffer + offset, sizeof(nimcp_append_entries_response_t));

    return true;
}

/**
 * Get RPC type from buffer
 *
 * WHAT: Extracts RPC type from message
 * WHY: Dispatch to correct handler
 * HOW: Read first bytes
 */
nimcp_rpc_type_t get_rpc_type(const uint8_t* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < sizeof(nimcp_rpc_type_t)) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    nimcp_rpc_type_t type;
    memcpy(&type, buffer, sizeof(type));
    return type;
}
