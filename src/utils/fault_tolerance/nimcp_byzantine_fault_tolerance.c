/**
 * @file nimcp_byzantine_fault_tolerance.c
 * @brief Implementation of Byzantine Fault Tolerance
 * @version 1.0.0
 * @date 2025-12-11
 */

#include "utils/fault_tolerance/nimcp_byzantine_fault_tolerance.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_timing_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(byzantine_fault_tolerance)

//=============================================================================
// Internal Structures
//=============================================================================

typedef struct {
    bft_msg_header_t header;
    void* payload;
    size_t payload_size;
} bft_pending_msg_t;

struct bft_context {
    bft_config_t config;
    bft_identity_t identity;
    bft_identity_t peer_keys[BFT_MAX_NODES];
    uint32_t peer_count;
    bft_trust_info_t trust[BFT_MAX_NODES];
    uint32_t trust_count;
    bft_pending_msg_t pending[BFT_MAX_MESSAGES];
    uint32_t pending_count;
    bft_checkpoint_t checkpoints[16];
    uint32_t checkpoint_count;
    uint64_t view_number;
    uint64_t sequence_number;
    bft_accusation_t accusations[32];
    uint32_t accusation_count;
    bft_consensus_callback_t consensus_callback;
    void* consensus_user_data;
    bft_byzantine_callback_t byzantine_callback;
    void* byzantine_user_data;
    bft_accusation_callback_t accusation_callback;
    void* accusation_user_data;
    bft_quarantine_callback_t quarantine_callback;
    void* quarantine_user_data;
    bft_trust_recovery_callback_t trust_recovery_callback;
    void* trust_recovery_user_data;
    bft_stats_t stats;
    nimcp_mutex_t lock;
    bool running;
    bool initialized;
};

//=============================================================================
// Private Functions
//=============================================================================

/**
 * @brief Simple hash function for signing simulation
 */
static void bft_simple_hash(const void* data, size_t size, uint8_t* out) {
    if (!data || !out) return;

    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t hash = 0x5A5A5A5A5A5A5A5A;

    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash = (hash << 7) | (hash >> 57);
        hash *= 0x9E3779B97F4A7C15ULL;
    }

    memcpy(out, &hash, 8);
    memcpy(out + 8, &hash, 8);
    memcpy(out + 16, &hash, 8);
    memcpy(out + 24, &hash, 8);
}

/**
 * @brief Find trust info for node
 */
static bft_trust_info_t* bft_find_trust(bft_context_t* ctx, uint32_t node_id) {
    for (uint32_t i = 0; i < ctx->trust_count; i++) {
        if (ctx->trust[i].node_id == node_id) {
            return &ctx->trust[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_find_trust: validation failed");
    return NULL;
}

/**
 * @brief Get or create trust info
 */
static bft_trust_info_t* bft_get_or_create_trust(bft_context_t* ctx, uint32_t node_id) {
    bft_trust_info_t* trust = bft_find_trust(ctx, node_id);
    if (trust) return trust;

    if (ctx->trust_count >= BFT_MAX_NODES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bft_get_or_create_trust: capacity exceeded");
        return NULL;
    }

    trust = &ctx->trust[ctx->trust_count++];
    trust->node_id = node_id;
    trust->status = BFT_STATUS_TRUSTED;
    trust->trust_score = ctx->config.initial_trust;
    trust->total_votes = 0;
    trust->correct_votes = 0;
    trust->accusations_received = 0;
    trust->accusations_made = 0;
    trust->false_accusations = 0;
    trust->last_activity_ms = nimcp_time_get_ms();
    trust->quarantine_until_ms = 0;

    return trust;
}

/**
 * @brief Notify security module of Byzantine behavior
 */
static void bft_notify_security(bft_context_t* ctx, uint32_t node_id, bft_behavior_t behavior) {
    // Send to security module via BBB audit
    bbb_audit_log(BBB_AUDIT_ERROR, "BFT", "BYZANTINE",
                 "Detected Byzantine behavior: node=%u, type=%s",
                 node_id, bft_behavior_to_string(behavior));

    // In real implementation, would send message to security recovery bridge
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

bft_config_t bft_default_config(void) {
    bft_config_t config = {
        .node_id = 0,
        .total_nodes = 4,
        .max_byzantine = 1,
        .view_timeout_ms = NIMCP_DEFAULT_TIMEOUT_MS,
        .message_timeout_ms = NIMCP_MEDIUM_TIMEOUT_MS,
        .checkpoint_interval = 100,
        .initial_trust = 80.0f,
        .trust_decay = 10.0f,
        .trust_recovery = 2.0f,
        .quarantine_threshold = 30.0f,
        .quarantine_duration_ms = 60000,
        .enable_signatures = true,
        .enable_trust_scoring = true
    };
    return config;
}

bft_context_t* bft_create(const bft_config_t* config) {
    if (!config) {
        LOG_ERROR("BFT", "NULL config provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    // Validate PBFT requirement: n >= 3f + 1
    if (config->total_nodes < 3 * config->max_byzantine + 1) {
        LOG_ERROR("BFT", "Insufficient nodes for PBFT: need %u, have %u",
                       3 * config->max_byzantine + 1, config->total_nodes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_create: validation failed");
        return NULL;
    }

    bft_context_t* ctx = nimcp_malloc(sizeof(bft_context_t));
    if (!ctx) {
        LOG_ERROR("BFT", "Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    memset(ctx, 0, sizeof(bft_context_t));
    ctx->config = *config;
    ctx->view_number = 0;
    ctx->sequence_number = 0;

    // Initialize own identity
    ctx->identity.node_id = config->node_id;
    ctx->identity.has_private_key = true;
    bft_generate_keys(&ctx->identity);

    if (nimcp_mutex_init(&ctx->lock, NULL) != 0) {
        LOG_ERROR("BFT", "Failed to initialize mutex");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "bft_create: validation failed");
        return NULL;
    }

    ctx->initialized = true;

    // Register with security module
    bbb_register_module("byzantine_fault_tolerance", BBB_MODULE_TYPE_CORE);
    bbb_audit_log(BBB_AUDIT_INFO, "BFT", "CREATE",
                 "Created BFT context: node=%u, total=%u, max_byzantine=%u",
                 config->node_id, config->total_nodes, config->max_byzantine);

    LOG_INFO("BFT", "Created BFT context for node %u", config->node_id);
    return ctx;
}

void bft_destroy(bft_context_t* ctx) {
    if (!ctx) return;

    if (ctx->running) {
        bft_stop(ctx);
    }

    // Clear sensitive data
    memset(&ctx->identity, 0, sizeof(bft_identity_t));
    memset(ctx->peer_keys, 0, sizeof(ctx->peer_keys));

    nimcp_mutex_destroy(&ctx->lock);
    nimcp_free(ctx);
}

bool bft_start(bft_context_t* ctx) {
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_start: required parameter is NULL (ctx, ctx->initialized)");
        return false;
    }
    if (ctx->running) return true;

    ctx->running = true;
    LOG_INFO("BFT", "Started BFT protocol");
    return true;
}

bool bft_stop(bft_context_t* ctx) {
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_stop: required parameter is NULL (ctx, ctx->initialized)");
        return false;
    }
    if (!ctx->running) return true;

    ctx->running = false;
    LOG_INFO("BFT", "Stopped BFT protocol");
    return true;
}

//=============================================================================
// Key Management
//=============================================================================

bool bft_generate_keys(bft_identity_t* identity) {
    if (!identity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_generate_keys: identity is NULL");
        return false;
    }

    // Simple key generation (in real implementation, use Ed25519)
    for (int i = 0; i < BFT_PUBLIC_KEY_SIZE; i++) {
        identity->public_key[i] = (uint8_t)(nimcp_tl_rand() & 0xFF);
    }
    for (int i = 0; i < BFT_PRIVATE_KEY_SIZE; i++) {
        identity->private_key[i] = (uint8_t)(nimcp_tl_rand() & 0xFF);
    }
    identity->has_private_key = true;

    return true;
}

bool bft_set_identity(bft_context_t* ctx, const bft_identity_t* identity) {
    if (!ctx || !identity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_set_identity: required parameter is NULL (ctx, identity)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->identity = *identity;
    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

bool bft_register_peer_key(bft_context_t* ctx, uint32_t node_id, const uint8_t* public_key) {
    if (!ctx || !public_key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_register_peer_key: required parameter is NULL (ctx, public_key)");
        return false;
    }
    if (ctx->peer_count >= BFT_MAX_NODES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "bft_register_peer_key: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    ctx->peer_keys[ctx->peer_count].node_id = node_id;
    memcpy(ctx->peer_keys[ctx->peer_count].public_key, public_key, BFT_PUBLIC_KEY_SIZE);
    ctx->peer_keys[ctx->peer_count].has_private_key = false;
    ctx->peer_count++;

    // Initialize trust
    bft_get_or_create_trust(ctx, node_id);

    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("BFT", "Registered peer key for node %u", node_id);
    return true;
}

//=============================================================================
// Consensus Protocol
//=============================================================================

bool bft_submit_request(bft_context_t* ctx, const void* data, size_t data_size, uint64_t* sequence) {
    if (!ctx || !data || data_size == 0 || !sequence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_submit_request: required parameter is NULL (ctx, data, sequence)");
        return false;
    }
    if (!ctx->running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_submit_request: ctx->running is NULL");
        return false;
    }

    // Validate with BBB
    if (!bbb_check_pointer((void*)data, "consensus_request")) {
        LOG_ERROR("BFT", "BBB rejected consensus request");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bft_submit_request: bbb_check_pointer is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    *sequence = ++ctx->sequence_number;
    ctx->stats.total_messages++;

    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("BFT", "Submitted request, sequence: %lu", (unsigned long)*sequence);
    return true;
}

bool bft_process_message(bft_context_t* ctx, const bft_msg_header_t* header, const void* payload, size_t payload_size) {
    if (!ctx || !header) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_process_message: required parameter is NULL (ctx, header)");
        return false;
    }
    if (!ctx->running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_process_message: ctx->running is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Verify signature
    if (ctx->config.enable_signatures) {
        if (!bft_verify_signature(ctx, header, payload, payload_size)) {
            ctx->stats.total_messages++;
            // Report to security
            bft_notify_security(ctx, header->sender_id, BFT_BEHAV_INVALID_SIG);
            nimcp_mutex_unlock(&ctx->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bft_process_message: bft_verify_signature is NULL");
            return false;
        }
    }

    // Update trust - node is active
    bft_trust_info_t* trust = bft_get_or_create_trust(ctx, header->sender_id);
    if (trust) {
        trust->last_activity_ms = nimcp_time_get_ms();
    }

    ctx->stats.total_messages++;

    switch (header->type) {
        case BFT_MSG_REQUEST:
        case BFT_MSG_PRE_PREPARE:
        case BFT_MSG_PREPARE:
        case BFT_MSG_COMMIT:
            ctx->stats.total_consensus_rounds++;
            break;

        case BFT_MSG_VIEW_CHANGE:
            ctx->stats.view_changes++;
            break;

        case BFT_MSG_ACCUSATION:
            // Process accusation
            break;

        default:
            break;
    }

    nimcp_mutex_unlock(&ctx->lock);
    return true;
}

bool bft_is_consensus_reached(bft_context_t* ctx, uint64_t sequence) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_is_consensus_reached: ctx is NULL");
        return false;
    }

    // Simple implementation: consensus reached if sequence is past
    return (sequence <= ctx->sequence_number);
}

bool bft_get_consensus_result(bft_context_t* ctx, uint64_t sequence, void* data_buffer, size_t buffer_size, size_t* actual_size) {
    if (!ctx || !data_buffer || !actual_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_get_consensus_result: required parameter is NULL (ctx, data_buffer, actual_size)");
        return false;
    }

    // In real implementation, would retrieve committed value
    *actual_size = 0;
    return true;
}

//=============================================================================
// Byzantine Detection
//=============================================================================

bool bft_verify_signature(bft_context_t* ctx, const bft_msg_header_t* header, const void* payload, size_t payload_size) {
    if (!ctx || !header) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_verify_signature: required parameter is NULL (ctx, header)");
        return false;
    }

    // Simple signature check (in real implementation, use Ed25519)
    // The first BFT_HASH_SIZE bytes of the signature contain the payload hash
    uint8_t expected_hash[BFT_HASH_SIZE];
    bft_simple_hash(payload, payload_size, expected_hash);

    return (memcmp(header->signature, expected_hash, BFT_HASH_SIZE) == 0);
}

bool bft_report_byzantine(bft_context_t* ctx, uint32_t accused_id, bft_behavior_t behavior, const bft_evidence_t* evidence, uint32_t evidence_count) {
    if (!ctx || accused_id == ctx->config.node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bft_report_byzantine: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    if (ctx->accusation_count >= 32) {
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "bft_report_byzantine: capacity exceeded");
        return false;
    }

    bft_accusation_t* acc = &ctx->accusations[ctx->accusation_count++];
    acc->accuser_id = ctx->config.node_id;
    acc->accused_id = accused_id;
    acc->behavior = behavior;
    acc->accusation_time_ms = nimcp_time_get_ms();
    acc->support_count = 1;
    acc->supporting_nodes[0] = ctx->config.node_id;

    if (evidence && evidence_count > 0) {
        uint32_t copy_count = (evidence_count < BFT_MAX_EVIDENCE) ? evidence_count : BFT_MAX_EVIDENCE;
        memcpy(acc->evidence, evidence, copy_count * sizeof(bft_evidence_t));
        acc->evidence_count = copy_count;
    }

    // Update accuser's count
    bft_trust_info_t* my_trust = bft_get_or_create_trust(ctx, ctx->config.node_id);
    if (my_trust) {
        my_trust->accusations_made++;
    }

    // Update accused's trust
    bft_trust_info_t* accused_trust = bft_get_or_create_trust(ctx, accused_id);
    if (accused_trust) {
        accused_trust->accusations_received++;
        accused_trust->trust_score -= ctx->config.trust_decay;
        if (accused_trust->trust_score < 0) accused_trust->trust_score = 0;

        if (accused_trust->trust_score < ctx->config.quarantine_threshold) {
            accused_trust->status = BFT_STATUS_QUARANTINED;
            accused_trust->quarantine_until_ms = nimcp_time_get_ms() + ctx->config.quarantine_duration_ms;
            ctx->stats.nodes_quarantined++;
        }
    }

    ctx->stats.byzantine_detected++;
    ctx->stats.accusations_processed++;

    nimcp_mutex_unlock(&ctx->lock);

    // Notify security module
    bft_notify_security(ctx, accused_id, behavior);

    // Invoke callback
    if (ctx->byzantine_callback) {
        ctx->byzantine_callback(accused_id, behavior, evidence, ctx->byzantine_user_data);
    }

    LOG_WARN("BFT", "Reported Byzantine behavior: node %u, type %s",
                  accused_id, bft_behavior_to_string(behavior));

    return true;
}

bool bft_process_accusation(bft_context_t* ctx, const bft_accusation_t* accusation) {
    if (!ctx || !accusation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_process_accusation: required parameter is NULL (ctx, accusation)");
        return false;
    }

    // Invoke accusation callback BEFORE processing (immune antigen presentation)
    if (ctx->accusation_callback) {
        ctx->accusation_callback(
            accusation->accuser_id,
            accusation->accused_id,
            accusation->behavior,
            accusation->evidence,
            accusation->evidence_count,
            ctx->accusation_user_data
        );
    }

    nimcp_mutex_lock(&ctx->lock);

    ctx->stats.accusations_processed++;

    // Verify evidence
    bool valid = true;
    for (uint32_t i = 0; i < accusation->evidence_count; i++) {
        // Basic validation
        if (accusation->evidence[i].accused_node_id != accusation->accused_id) {
            valid = false;
            break;
        }
    }

    if (!valid) {
        // False accusation
        bft_trust_info_t* accuser_trust = bft_get_or_create_trust(ctx, accusation->accuser_id);
        if (accuser_trust) {
            accuser_trust->false_accusations++;
            accuser_trust->trust_score -= ctx->config.trust_decay * 2;
        }
        ctx->stats.false_accusations++;
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bft_process_accusation: validation failed");
        return false;
    }

    nimcp_mutex_unlock(&ctx->lock);
    return true;
}

bool bft_vote_accusation(bft_context_t* ctx, const bft_accusation_t* accusation, bool support) {
    if (!ctx || !accusation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_vote_accusation: required parameter is NULL (ctx, accusation)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Find matching accusation
    for (uint32_t i = 0; i < ctx->accusation_count; i++) {
        if (ctx->accusations[i].accused_id == accusation->accused_id &&
            ctx->accusations[i].behavior == accusation->behavior) {

            if (support && ctx->accusations[i].support_count < BFT_MAX_NODES) {
                ctx->accusations[i].supporting_nodes[ctx->accusations[i].support_count++] = ctx->config.node_id;
            }

            nimcp_mutex_unlock(&ctx->lock);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bft_vote_accusation: operation failed");
    return false;
}

bool bft_check_equivocation(bft_context_t* ctx, const bft_msg_header_t* msg1, const bft_msg_header_t* msg2) {
    if (!ctx || !msg1 || !msg2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_check_equivocation: required parameter is NULL (ctx, msg1, msg2)");
        return false;
    }

    // Equivocation: same sender, same sequence, different content
    if (msg1->sender_id == msg2->sender_id &&
        msg1->sequence_number == msg2->sequence_number &&
        msg1->view_number == msg2->view_number &&
        memcmp(msg1->digest, msg2->digest, BFT_HASH_SIZE) != 0) {

        LOG_WARN("BFT", "Equivocation detected from node %u", msg1->sender_id);
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bft_check_equivocation: operation failed");
    return false;
}

//=============================================================================
// Trust Management
//=============================================================================

bool bft_get_trust_info(bft_context_t* ctx, uint32_t node_id, bft_trust_info_t* trust) {
    if (!ctx || !trust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_get_trust_info: required parameter is NULL (ctx, trust)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    bft_trust_info_t* found = bft_get_or_create_trust(ctx, node_id);
    if (found) {
        *trust = *found;
        nimcp_mutex_unlock(&ctx->lock);
        return true;
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bft_get_trust_info: validation failed");
    return false;
}

float bft_update_trust(bft_context_t* ctx, uint32_t node_id, bool correct_behavior) {
    if (!ctx) return 0.0f;

    nimcp_mutex_lock(&ctx->lock);

    bft_trust_info_t* trust = bft_get_or_create_trust(ctx, node_id);
    if (!trust) {
        nimcp_mutex_unlock(&ctx->lock);
        return 0.0f;
    }

    trust->total_votes++;
    float old_trust = trust->trust_score;
    bool was_probation = (trust->status == BFT_STATUS_PROBATION);

    if (correct_behavior) {
        trust->correct_votes++;
        trust->trust_score += ctx->config.trust_recovery;
        if (trust->trust_score > 100.0f) trust->trust_score = 100.0f;

        // Consider removing from probation
        if (trust->status == BFT_STATUS_PROBATION && trust->trust_score > 60.0f) {
            trust->status = BFT_STATUS_TRUSTED;
        }
    } else {
        trust->trust_score -= ctx->config.trust_decay;
        if (trust->trust_score < 0.0f) trust->trust_score = 0.0f;

        // Consider quarantine
        if (trust->trust_score < ctx->config.quarantine_threshold) {
            trust->status = BFT_STATUS_QUARANTINED;
            trust->quarantine_until_ms = nimcp_time_get_ms() + ctx->config.quarantine_duration_ms;
        }
    }

    float new_trust = trust->trust_score;
    bool is_trusted = (trust->status == BFT_STATUS_TRUSTED);
    nimcp_mutex_unlock(&ctx->lock);

    // Invoke trust recovery callback (immune memory formation)
    if (was_probation && is_trusted && ctx->trust_recovery_callback) {
        ctx->trust_recovery_callback(node_id, old_trust, new_trust, ctx->trust_recovery_user_data);
    }

    return new_trust;
}

bool bft_quarantine_node(bft_context_t* ctx, uint32_t node_id, uint64_t duration_ms) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_quarantine_node: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    bft_trust_info_t* trust = bft_get_or_create_trust(ctx, node_id);
    if (trust) {
        trust->status = BFT_STATUS_QUARANTINED;
        trust->quarantine_until_ms = nimcp_time_get_ms() + duration_ms;
        ctx->stats.nodes_quarantined++;

        float trust_score = trust->trust_score;

        nimcp_mutex_unlock(&ctx->lock);

        // Invoke quarantine callback (immune killer T cell coordination)
        if (ctx->quarantine_callback) {
            ctx->quarantine_callback(node_id, duration_ms, trust_score, ctx->quarantine_user_data);
        }

        bbb_audit_log(BBB_AUDIT_WARNING, "BFT", "QUARANTINE",
                     "Node %u quarantined for %lu ms", node_id, (unsigned long)duration_ms);
        return true;
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bft_quarantine_node: operation failed");
    return false;
}

bool bft_is_quarantined(bft_context_t* ctx, uint32_t node_id) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_is_quarantined: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    bft_trust_info_t* trust = bft_find_trust(ctx, node_id);
    if (!trust) {
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_is_quarantined: trust is NULL");
        return false;
    }

    bool quarantined = (trust->status == BFT_STATUS_QUARANTINED);

    // Check if quarantine expired
    if (quarantined && nimcp_time_get_ms() > trust->quarantine_until_ms) {
        trust->status = BFT_STATUS_PROBATION;
        quarantined = false;
    }

    nimcp_mutex_unlock(&ctx->lock);
    return quarantined;
}

bool bft_release_quarantine(bft_context_t* ctx, uint32_t node_id) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_release_quarantine: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    bft_trust_info_t* trust = bft_find_trust(ctx, node_id);
    if (trust && trust->status == BFT_STATUS_QUARANTINED) {
        trust->status = BFT_STATUS_PROBATION;
        trust->quarantine_until_ms = 0;

        nimcp_mutex_unlock(&ctx->lock);

        bbb_audit_log(BBB_AUDIT_INFO, "BFT", "RELEASE",
                     "Node %u released from quarantine", node_id);
        return true;
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bft_release_quarantine: operation failed");
    return false;
}

//=============================================================================
// View Change
//=============================================================================

bool bft_request_view_change(bft_context_t* ctx, bft_view_reason_t reason) {
    if (!ctx || !ctx->running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_request_view_change: required parameter is NULL (ctx, ctx->running)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    ctx->view_number++;
    ctx->stats.view_changes++;

    nimcp_mutex_unlock(&ctx->lock);

    bbb_audit_log(BBB_AUDIT_WARNING, "BFT", "VIEW_CHANGE",
                 "View change requested: reason=%s, new_view=%lu",
                 bft_view_reason_to_string(reason), (unsigned long)ctx->view_number);

    return true;
}

uint64_t bft_get_view(bft_context_t* ctx) {
    if (!ctx) return 0;
    return ctx->view_number;
}

uint32_t bft_get_leader(bft_context_t* ctx) {
    if (!ctx) return 0;
    // Leader = view_number mod total_nodes
    return (uint32_t)(ctx->view_number % ctx->config.total_nodes);
}

bool bft_is_leader(bft_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_is_leader: ctx is NULL");
        return false;
    }
    return (bft_get_leader(ctx) == ctx->config.node_id);
}

//=============================================================================
// Checkpoints
//=============================================================================

bool bft_create_checkpoint(bft_context_t* ctx, const uint8_t* state_hash) {
    if (!ctx || !state_hash) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_create_checkpoint: required parameter is NULL (ctx, state_hash)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    if (ctx->checkpoint_count >= 16) {
        // Remove oldest
        for (int i = 0; i < 15; i++) {
            ctx->checkpoints[i] = ctx->checkpoints[i + 1];
        }
        ctx->checkpoint_count = 15;
    }

    bft_checkpoint_t* cp = &ctx->checkpoints[ctx->checkpoint_count++];
    cp->sequence_number = ctx->sequence_number;
    cp->view_number = ctx->view_number;
    memcpy(cp->state_hash, state_hash, BFT_HASH_SIZE);
    cp->signatures_count = 1;
    cp->signers[0] = ctx->config.node_id;

    // Initialize immune state (will be populated by immune system if connected)
    memset(&cp->immune_state, 0, sizeof(bft_immune_state_t));

    // Sign checkpoint
    bft_sign(ctx, state_hash, BFT_HASH_SIZE, cp->signatures[0]);

    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("BFT", "Created checkpoint at sequence %lu", (unsigned long)cp->sequence_number);
    return true;
}

bool bft_get_stable_checkpoint(bft_context_t* ctx, bft_checkpoint_t* checkpoint) {
    if (!ctx || !checkpoint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_get_stable_checkpoint: required parameter is NULL (ctx, checkpoint)");
        return false;
    }
    if (ctx->checkpoint_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bft_get_stable_checkpoint: ctx->checkpoint_count is zero");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Find checkpoint with enough signatures (2f + 1)
    uint32_t required = 2 * ctx->config.max_byzantine + 1;

    for (int i = ctx->checkpoint_count - 1; i >= 0; i--) {
        if (ctx->checkpoints[i].signatures_count >= required) {
            *checkpoint = ctx->checkpoints[i];
            nimcp_mutex_unlock(&ctx->lock);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "bft_get_stable_checkpoint: capacity exceeded");
    return false;
}

bool bft_verify_checkpoint(bft_context_t* ctx, const bft_checkpoint_t* checkpoint) {
    if (!ctx || !checkpoint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_verify_checkpoint: required parameter is NULL (ctx, checkpoint)");
        return false;
    }

    // Verify required signatures
    uint32_t required = 2 * ctx->config.max_byzantine + 1;
    return (checkpoint->signatures_count >= required);
}

//=============================================================================
// Callbacks
//=============================================================================

bool bft_register_consensus_callback(bft_context_t* ctx, bft_consensus_callback_t callback, void* user_data) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_register_consensus_callback: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->consensus_callback = callback;
    ctx->consensus_user_data = user_data;
    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

bool bft_register_byzantine_callback(bft_context_t* ctx, bft_byzantine_callback_t callback, void* user_data) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_register_byzantine_callback: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->byzantine_callback = callback;
    ctx->byzantine_user_data = user_data;
    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

/**
 * @brief Register accusation callback
 *
 * WHAT: Store callback for accusation events
 * WHY:  Enable immune system to present antigens on accusations
 * HOW:  Store in context, invoke before processing
 */
bool bft_register_accusation_callback(bft_context_t* ctx, bft_accusation_callback_t callback, void* user_data) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_register_accusation_callback: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->accusation_callback = callback;
    ctx->accusation_user_data = user_data;
    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

/**
 * @brief Register quarantine callback
 *
 * WHAT: Store callback for quarantine actions
 * WHY:  Enable immune killer T cell coordination
 * HOW:  Store in context, invoke on quarantine
 */
bool bft_register_quarantine_callback(bft_context_t* ctx, bft_quarantine_callback_t callback, void* user_data) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_register_quarantine_callback: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->quarantine_callback = callback;
    ctx->quarantine_user_data = user_data;
    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

/**
 * @brief Register trust recovery callback
 *
 * WHAT: Store callback for trust restoration
 * WHY:  Enable immune memory formation on recovery
 * HOW:  Store in context, invoke on trust recovery
 */
bool bft_register_trust_recovery_callback(bft_context_t* ctx, bft_trust_recovery_callback_t callback, void* user_data) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_register_trust_recovery_callback: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->trust_recovery_callback = callback;
    ctx->trust_recovery_user_data = user_data;
    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

//=============================================================================
// Statistics
//=============================================================================

bool bft_get_stats(bft_context_t* ctx, bft_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_get_stats: required parameter is NULL (ctx, stats)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    *stats = ctx->stats;

    // Calculate cluster trust
    float total_trust = 0.0f;
    for (uint32_t i = 0; i < ctx->trust_count; i++) {
        total_trust += ctx->trust[i].trust_score;
    }
    stats->cluster_trust_score = (ctx->trust_count > 0) ? (total_trust / ctx->trust_count) : 100.0f;

    nimcp_mutex_unlock(&ctx->lock);
    return true;
}

void bft_reset_stats(bft_context_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(&ctx->lock);
    memset(&ctx->stats, 0, sizeof(bft_stats_t));
    nimcp_mutex_unlock(&ctx->lock);
}

bool bft_is_cluster_healthy(bft_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_is_cluster_healthy: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Count unhealthy (byzantine/quarantined) nodes
    uint32_t unhealthy = 0;
    for (uint32_t i = 0; i < ctx->trust_count; i++) {
        if (ctx->trust[i].status == BFT_STATUS_BYZANTINE ||
            ctx->trust[i].status == BFT_STATUS_QUARANTINED) {
            unhealthy++;
        }
    }

    // Cluster is healthy if unhealthy nodes don't exceed max_byzantine
    bool result = (unhealthy <= ctx->config.max_byzantine);

    nimcp_mutex_unlock(&ctx->lock);
    return result;
}

//=============================================================================
// Cryptographic Utilities
//=============================================================================

bool bft_sign(bft_context_t* ctx, const void* data, size_t data_size, uint8_t* signature) {
    if (!ctx || !data || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_sign: required parameter is NULL (ctx, data, signature)");
        return false;
    }

    // Simple signature (in real implementation, use Ed25519)
    bft_simple_hash(data, data_size, signature);
    memcpy(signature + 32, ctx->identity.private_key, 32);

    return true;
}

bool bft_verify(const uint8_t* public_key, const void* data, size_t data_size, const uint8_t* signature) {
    if (!public_key || !data || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bft_verify: required parameter is NULL (public_key, data, signature)");
        return false;
    }

    uint8_t expected[BFT_HASH_SIZE];
    bft_simple_hash(data, data_size, expected);

    return (memcmp(signature, expected, BFT_HASH_SIZE) == 0);
}

void bft_hash(const void* data, size_t data_size, uint8_t* hash) {
    bft_simple_hash(data, data_size, hash);
}

//=============================================================================
// String Conversion
//=============================================================================

const char* bft_behavior_to_string(bft_behavior_t behavior) {
    switch (behavior) {
        case BFT_BEHAV_NONE: return "None";
        case BFT_BEHAV_SILENT: return "Silent";
        case BFT_BEHAV_EQUIVOCATION: return "Equivocation";
        case BFT_BEHAV_INVALID_SIG: return "InvalidSignature";
        case BFT_BEHAV_REPLAY: return "Replay";
        case BFT_BEHAV_FABRICATION: return "Fabrication";
        case BFT_BEHAV_TIMING: return "Timing";
        case BFT_BEHAV_COLLUSION: return "Collusion";
        default: return "Unknown";
    }
}

const char* bft_msg_type_to_string(bft_msg_type_t type) {
    switch (type) {
        case BFT_MSG_REQUEST: return "Request";
        case BFT_MSG_PRE_PREPARE: return "PrePrepare";
        case BFT_MSG_PREPARE: return "Prepare";
        case BFT_MSG_COMMIT: return "Commit";
        case BFT_MSG_REPLY: return "Reply";
        case BFT_MSG_CHECKPOINT: return "Checkpoint";
        case BFT_MSG_VIEW_CHANGE: return "ViewChange";
        case BFT_MSG_NEW_VIEW: return "NewView";
        case BFT_MSG_ACCUSATION: return "Accusation";
        case BFT_MSG_DEFENSE: return "Defense";
        default: return "Unknown";
    }
}

const char* bft_status_to_string(bft_node_status_t status) {
    switch (status) {
        case BFT_STATUS_TRUSTED: return "Trusted";
        case BFT_STATUS_SUSPECTED: return "Suspected";
        case BFT_STATUS_BYZANTINE: return "Byzantine";
        case BFT_STATUS_QUARANTINED: return "Quarantined";
        case BFT_STATUS_PROBATION: return "Probation";
        default: return "Unknown";
    }
}

const char* bft_evidence_type_to_string(bft_evidence_type_t type) {
    switch (type) {
        case BFT_EVIDENCE_CONFLICTING_MSG: return "ConflictingMessage";
        case BFT_EVIDENCE_INVALID_SIG: return "InvalidSignature";
        case BFT_EVIDENCE_INVALID_DATA: return "InvalidData";
        case BFT_EVIDENCE_TIMING_VIOLATION: return "TimingViolation";
        case BFT_EVIDENCE_PROTOCOL_VIOLATION: return "ProtocolViolation";
        case BFT_EVIDENCE_WITNESS: return "Witness";
        default: return "Unknown";
    }
}

const char* bft_view_reason_to_string(bft_view_reason_t reason) {
    switch (reason) {
        case BFT_VIEW_LEADER_TIMEOUT: return "LeaderTimeout";
        case BFT_VIEW_LEADER_BYZANTINE: return "LeaderByzantine";
        case BFT_VIEW_STUCK_CONSENSUS: return "StuckConsensus";
        case BFT_VIEW_MANUAL: return "Manual";
        default: return "Unknown";
    }
}
