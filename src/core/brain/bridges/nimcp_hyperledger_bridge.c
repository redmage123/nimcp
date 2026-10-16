//=============================================================================
// nimcp_hyperledger_bridge.c - Hyperledger-Inspired Training/Inference Integration
//=============================================================================

#include "core/brain/bridges/nimcp_hyperledger_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "HYPERLEDGER"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hyperledger_bridge)

//=============================================================================
// Simple hash for audit chain (FNV-1a based, not cryptographic)
//=============================================================================

static void compute_entry_hash(const hyperledger_audit_entry_t* entry,
                                uint8_t hash_out[HYPERLEDGER_HASH_SIZE])
{
    /* FNV-1a hash over entry content (excluding entry_hash itself) */
    uint64_t h = 14695981039346656037ULL;
    const uint8_t* data = (const uint8_t*)entry;
    /* Hash everything up to entry_hash field (offset = seq + timestamp + type + prev_hash) */
    size_t prefix_size = sizeof(uint64_t) + sizeof(uint64_t) + sizeof(audit_entry_type_t)
                        + HYPERLEDGER_HASH_SIZE;
    for (size_t i = 0; i < prefix_size; i++) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    /* Also hash the data union */
    const uint8_t* union_data = (const uint8_t*)&entry->data;
    size_t union_size = sizeof(entry->data);
    for (size_t i = 0; i < union_size; i++) {
        h ^= union_data[i];
        h *= 1099511628211ULL;
    }

    /* Spread 64-bit hash into 32 bytes via repeated mixing */
    memset(hash_out, 0, HYPERLEDGER_HASH_SIZE);
    for (int round = 0; round < 4; round++) {
        uint64_t r = h + (uint64_t)round * 2654435761ULL;
        for (int i = 0; i < 8; i++) {
            hash_out[round * 8 + i] = (uint8_t)(r >> (i * 8));
        }
        h ^= h >> 17;
        h *= 0xbf58476d1ce4e5b9ULL;
    }
}

//=============================================================================
// Bridge structure
//=============================================================================

struct hyperledger_bridge {
    uint32_t magic;
    hyperledger_bridge_config_t config;

    /* Connected subsystems */
    struct security_distributed_training_bridge* sec_bridge;
    struct nimcp_gradient_manager_ctx* grad_mgr;
    struct swarm_consensus_context* consensus;
    struct collective_cognition* collective;

    /* EOV transaction state */
    eov_transaction_t current_tx;
    uint64_t next_tx_id;
    uint32_t steps_since_validation;

    /* Audit ring buffer */
    hyperledger_audit_entry_t* audit_ring;
    uint64_t audit_write_pos;       /* Next write position */
    uint64_t audit_count;           /* Total entries written */
    uint8_t last_hash[HYPERLEDGER_HASH_SIZE]; /* Hash of last entry */

    /* COW snapshots (indices into external snapshot array) */
    uint32_t snapshot_count;

    /* Loss tracking for auto-rollback */
    float last_committed_loss;
    float best_loss;

    /* Statistics */
    hyperledger_bridge_stats_t stats;
};

//=============================================================================
// Configuration
//=============================================================================

hyperledger_bridge_config_t hyperledger_bridge_default_config(void)
{
    hyperledger_bridge_config_t config = {
        .enable_eov_pipeline = true,
        .anomaly_reject_threshold = 0.8f,
        .validate_every_step = false,
        .validation_interval = 10,

        .enable_consensus_gate = false,  /* Disabled until collective connected */
        .consensus_threshold = 0.666667f,  /* 2/3 majority */
        .consensus_quorum = 0,
        .consensus_timeout_ms = 1000,

        .enable_audit_log = true,
        .enable_hash_chain = true,
        .audit_weight_threshold = 0.001f,
        .enable_snapshots = true,
        .snapshot_interval = 100,
        .max_snapshots = HYPERLEDGER_MAX_ROLLBACK_DEPTH,

        .enable_auto_rollback = false,
        .rollback_loss_threshold = 2.0f
    };
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

hyperledger_bridge_t* hyperledger_bridge_create(
    const hyperledger_bridge_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_bridge_create: NULL config");
        return NULL;
    }

    hyperledger_bridge_t* bridge = nimcp_calloc(1, sizeof(hyperledger_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "hyperledger_bridge_create: failed to allocate bridge");
        return NULL;
    }

    bridge->magic = HYPERLEDGER_BRIDGE_MAGIC;
    bridge->config = *config;
    bridge->next_tx_id = 1;
    bridge->last_committed_loss = 1.0f;
    bridge->best_loss = 1.0f;

    /* Allocate audit ring buffer */
    if (config->enable_audit_log) {
        bridge->audit_ring = nimcp_calloc(HYPERLEDGER_AUDIT_RING_SIZE,
                                           sizeof(hyperledger_audit_entry_t));
        if (!bridge->audit_ring) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "hyperledger_bridge_create: failed to allocate audit ring buffer");
            nimcp_free(bridge);
            return NULL;
        }
    }

    /* Register with BBB and log creation */
    bbb_register_module("hyperledger_bridge", BBB_MODULE_TYPE_COGNITIVE);
    bbb_audit_log(BBB_AUDIT_INFO, "hyperledger_bridge", "created",
                  "EOV training + consensus audit");

    LOG_INFO(LOG_MODULE, "Hyperledger bridge created (EOV=%s, consensus=%s, audit=%s)",
             config->enable_eov_pipeline ? "on" : "off",
             config->enable_consensus_gate ? "on" : "off",
             config->enable_audit_log ? "on" : "off");

    return bridge;
}

void hyperledger_bridge_destroy(hyperledger_bridge_t* bridge)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) return;

    if (bridge->audit_ring) {
        nimcp_free(bridge->audit_ring);
    }

    bridge->magic = 0;
    nimcp_free(bridge);
}

//=============================================================================
// Connection
//=============================================================================

int hyperledger_bridge_connect_security(
    hyperledger_bridge_t* bridge,
    struct security_distributed_training_bridge* sec_bridge)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_bridge_connect_security: NULL or invalid bridge");
        return -1;
    }
    bridge->sec_bridge = sec_bridge;
    return 0;
}

int hyperledger_bridge_connect_gradient_manager(
    hyperledger_bridge_t* bridge,
    struct nimcp_gradient_manager_ctx* grad_mgr)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_bridge_connect_gradient_manager: NULL or invalid bridge");
        return -1;
    }
    bridge->grad_mgr = grad_mgr;
    return 0;
}

int hyperledger_bridge_connect_consensus(
    hyperledger_bridge_t* bridge,
    struct swarm_consensus_context* consensus)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_bridge_connect_consensus: NULL or invalid bridge");
        return -1;
    }
    bridge->consensus = consensus;
    if (consensus) {
        bridge->config.enable_consensus_gate = true;
    }
    return 0;
}

int hyperledger_bridge_connect_collective(
    hyperledger_bridge_t* bridge,
    struct collective_cognition* collective)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_bridge_connect_collective: NULL or invalid bridge");
        return -1;
    }
    bridge->collective = collective;
    return 0;
}

//=============================================================================
// Internal: Audit logging
//=============================================================================

static uint64_t audit_log_internal(hyperledger_bridge_t* bridge,
                                    hyperledger_audit_entry_t* entry)
{
    if (!bridge->audit_ring) return 0;

    /* Assign sequence ID and timestamp */
    entry->sequence_id = ++bridge->audit_count;
    entry->timestamp_ms = nimcp_time_get_ms();

    /* Chain: copy previous hash */
    memcpy(entry->prev_hash, bridge->last_hash, HYPERLEDGER_HASH_SIZE);

    /* Compute this entry's hash */
    if (bridge->config.enable_hash_chain) {
        compute_entry_hash(entry, entry->entry_hash);
        memcpy(bridge->last_hash, entry->entry_hash, HYPERLEDGER_HASH_SIZE);
    }

    /* Write to ring buffer */
    uint64_t ring_idx = bridge->audit_write_pos % HYPERLEDGER_AUDIT_RING_SIZE;
    bridge->audit_ring[ring_idx] = *entry;
    bridge->audit_write_pos++;

    bridge->stats.audit_entries = bridge->audit_count;
    return entry->sequence_id;
}

//=============================================================================
// EOV Training Pipeline (Integration Point #1)
//=============================================================================

uint64_t hyperledger_eov_begin(hyperledger_bridge_t* bridge, float loss)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_eov_begin: NULL or invalid bridge");
        return 0;
    }
    if (!bridge->config.enable_eov_pipeline) return bridge->next_tx_id++;

    eov_transaction_t* tx = &bridge->current_tx;
    tx->tx_id = bridge->next_tx_id++;
    tx->phase = EOV_PHASE_EXECUTE;
    tx->start_time_ms = nimcp_time_get_ms();
    tx->loss = loss;
    tx->validated = false;
    tx->anomaly_score = 0.0f;

    bridge->stats.total_transactions++;
    return tx->tx_id;
}

int hyperledger_eov_order(hyperledger_bridge_t* bridge,
                           uint64_t tx_id,
                           float gradient_norm,
                           uint32_t num_gradients)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_eov_order: NULL or invalid bridge");
        return -1;
    }
    if (!bridge->config.enable_eov_pipeline) return 0;

    eov_transaction_t* tx = &bridge->current_tx;
    if (tx->tx_id != tx_id) return -1;

    tx->phase = EOV_PHASE_ORDER;
    tx->gradient_norm = gradient_norm;

    /* Basic ordering: check gradient is finite */
    if (isnan(gradient_norm) || isinf(gradient_norm)) {
        tx->phase = EOV_PHASE_REJECTED;
        tx->anomaly_score = 1.0f;
        bridge->stats.rejected_transactions++;

        /* Audit log: rejected gradient */
        if (bridge->config.enable_audit_log) {
            hyperledger_audit_entry_t entry = {0};
            entry.type = AUDIT_GRADIENT_REJECTED;
            entry.data.gradient_rejected.anomaly_score = 1.0f;
            entry.data.gradient_rejected.gradient_norm = gradient_norm;
            audit_log_internal(bridge, &entry);
        }
        return -1;
    }

    return 0;
}

bool hyperledger_eov_validate(hyperledger_bridge_t* bridge,
                               uint64_t tx_id,
                               const float* gradients,
                               uint32_t num_gradients)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_eov_validate: NULL or invalid bridge");
        return false;
    }
    if (!bridge->config.enable_eov_pipeline) return true;

    eov_transaction_t* tx = &bridge->current_tx;
    if (tx->tx_id != tx_id || tx->phase == EOV_PHASE_REJECTED) return false;

    tx->phase = EOV_PHASE_VALIDATE;
    bridge->steps_since_validation++;

    /* Check if we should validate this step */
    if (!bridge->config.validate_every_step &&
        bridge->steps_since_validation < bridge->config.validation_interval) {
        /* Skip validation on this step — pass through */
        tx->validated = true;
        return true;
    }

    bridge->steps_since_validation = 0;
    uint64_t start_us = nimcp_time_get_us();

    /* Validation Step 1: Basic health check on gradients */
    float anomaly_score = 0.0f;
    if (gradients && num_gradients > 0) {
        /* Check for NaN/Inf */
        uint32_t bad_count = 0;
        float sum_sq = 0.0f;
        for (uint32_t i = 0; i < num_gradients && i < 10000; i++) {
            if (isnan(gradients[i]) || isinf(gradients[i])) {
                bad_count++;
            } else {
                sum_sq += gradients[i] * gradients[i];
            }
        }
        if (bad_count > 0) {
            anomaly_score += 0.5f * ((float)bad_count / fminf(num_gradients, 10000));
        }

        /* Check gradient norm against threshold */
        float norm = sqrtf(sum_sq);
        if (norm > 100.0f) {
            anomaly_score += 0.3f * fminf(norm / 1000.0f, 1.0f);
        }

        /* Check gradient variance (detect flat/dead gradients) */
        if (num_gradients > 1 && norm < 1e-10f) {
            anomaly_score += 0.2f;  /* Suspiciously zero gradients */
        }
    }

    tx->anomaly_score = anomaly_score;
    tx->validation_time_us = (uint32_t)(nimcp_time_get_us() - start_us);
    bridge->stats.total_validation_time_us += tx->validation_time_us;

    /* Update average */
    uint64_t validated = bridge->stats.committed_transactions +
                         bridge->stats.rejected_transactions + 1;
    bridge->stats.avg_validation_time_us =
        (float)bridge->stats.total_validation_time_us / (float)validated;

    /* Decision: accept or reject */
    if (anomaly_score >= bridge->config.anomaly_reject_threshold) {
        tx->phase = EOV_PHASE_REJECTED;
        tx->validated = false;
        bridge->stats.rejected_transactions++;
        bridge->stats.rejection_rate =
            (float)bridge->stats.rejected_transactions /
            (float)bridge->stats.total_transactions;

        /* Audit log */
        if (bridge->config.enable_audit_log) {
            hyperledger_audit_entry_t entry = {0};
            entry.type = AUDIT_GRADIENT_REJECTED;
            entry.data.gradient_rejected.anomaly_score = anomaly_score;
            entry.data.gradient_rejected.gradient_norm = tx->gradient_norm;
            audit_log_internal(bridge, &entry);
        }

        LOG_WARN(LOG_MODULE, "EOV REJECT tx=%lu: anomaly_score=%.3f > threshold=%.3f",
                 (unsigned long)tx_id, anomaly_score,
                 bridge->config.anomaly_reject_threshold);
        return false;
    }

    tx->validated = true;
    return true;
}

int hyperledger_eov_commit(hyperledger_bridge_t* bridge,
                            uint64_t tx_id,
                            float weight_delta_norm,
                            float loss_after)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_eov_commit: NULL or invalid bridge");
        return -1;
    }
    if (!bridge->config.enable_eov_pipeline) return 0;

    eov_transaction_t* tx = &bridge->current_tx;
    if (tx->tx_id != tx_id) return -1;
    if (!tx->validated && tx->phase != EOV_PHASE_VALIDATE) return -1;

    tx->phase = EOV_PHASE_COMMITTED;
    bridge->stats.committed_transactions++;
    bridge->stats.rejection_rate =
        (float)bridge->stats.rejected_transactions /
        (float)bridge->stats.total_transactions;

    /* Audit log: weight update */
    if (bridge->config.enable_audit_log &&
        weight_delta_norm >= bridge->config.audit_weight_threshold) {
        hyperledger_audit_entry_t entry = {0};
        entry.type = AUDIT_WEIGHT_UPDATE;
        entry.data.weight_update.weight_delta_norm = weight_delta_norm;
        entry.data.weight_update.loss_before = tx->loss;
        entry.data.weight_update.loss_after = loss_after;
        audit_log_internal(bridge, &entry);
    }

    /* Check for auto-rollback condition */
    if (bridge->config.enable_auto_rollback && loss_after > 0.0f) {
        float loss_increase = loss_after - bridge->best_loss;
        if (loss_increase > bridge->config.rollback_loss_threshold &&
            bridge->best_loss > 0.0f) {
            LOG_WARN(LOG_MODULE, "Loss spike detected: %.4f → %.4f (delta=%.4f > threshold=%.4f)",
                     bridge->best_loss, loss_after, loss_increase,
                     bridge->config.rollback_loss_threshold);
            /* Log but don't auto-rollback — caller should check and decide */
        }
    }

    /* Track losses */
    bridge->last_committed_loss = loss_after;
    if (loss_after < bridge->best_loss && loss_after > 0.0f) {
        bridge->best_loss = loss_after;
    }

    return 0;
}

int hyperledger_eov_get_state(const hyperledger_bridge_t* bridge,
                               eov_transaction_t* state)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_eov_get_state: NULL bridge or state");
        return -1;
    }
    *state = bridge->current_tx;
    return 0;
}

//=============================================================================
// Consensus-Gated Inference (Integration Point #2)
//=============================================================================

int hyperledger_consensus_gate(hyperledger_bridge_t* bridge,
                                const float* decision_vector,
                                uint32_t decision_dim,
                                float local_confidence,
                                consensus_gate_result_t* result)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_consensus_gate: NULL bridge or result");
        return -1;
    }

    /* If consensus not enabled or no collective, pass through */
    if (!bridge->config.enable_consensus_gate || !bridge->collective) {
        *result = CONSENSUS_GATE_SKIP;
        return 0;
    }

    bridge->stats.consensus_proposals++;

    /* Compute decision summary for voting: use first 4 floats as proposal values */
    float proposal_values[4] = {0};
    uint32_t copy_dim = (decision_dim < 4) ? decision_dim : 4;
    if (decision_vector) {
        for (uint32_t i = 0; i < copy_dim; i++) {
            proposal_values[i] = decision_vector[i];
        }
    }

    /* Local gating: check if decision confidence meets the threshold.
     * In multi-brain mode with swarm consensus connected, would also
     * require external votes via the BFT voting API. */
    if (local_confidence >= bridge->config.consensus_threshold) {
        *result = CONSENSUS_GATE_PASS;
        bridge->stats.consensus_passed++;
        bridge->stats.avg_agreement =
            (bridge->stats.avg_agreement * (bridge->stats.consensus_passed - 1) +
             local_confidence) / bridge->stats.consensus_passed;

        /* Audit log */
        if (bridge->config.enable_audit_log) {
            hyperledger_audit_entry_t entry = {0};
            entry.type = AUDIT_CONSENSUS_REACHED;
            entry.data.consensus.agreement = local_confidence;
            entry.data.consensus.agree_count = 1;
            entry.data.consensus.disagree_count = 0;
            entry.data.consensus.decision_confidence = local_confidence;
            audit_log_internal(bridge, &entry);
        }
    } else {
        *result = CONSENSUS_GATE_REJECT;
        bridge->stats.consensus_failed++;

        if (bridge->config.enable_audit_log) {
            hyperledger_audit_entry_t entry = {0};
            entry.type = AUDIT_CONSENSUS_FAILED;
            entry.data.consensus.agreement = local_confidence;
            entry.data.consensus.agree_count = 0;
            entry.data.consensus.disagree_count = 1;
            entry.data.consensus.decision_confidence = local_confidence;
            audit_log_internal(bridge, &entry);
        }
    }

    return 0;
}

int hyperledger_consensus_vote(hyperledger_bridge_t* bridge,
                                uint32_t proposal_id,
                                float agreement,
                                float confidence)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_consensus_vote: NULL or invalid bridge");
        return -1;
    }
    if (!bridge->consensus) return -1;

    /* In a full multi-brain deployment, this would call
     * swarm_consensus_vote(bridge->consensus, proposal_id, choice, confidence).
     * For now we track the intent for when collective is wired. */
    (void)proposal_id;
    (void)agreement;
    (void)confidence;

    return 0;
}

//=============================================================================
// Auditable Weight Ledger (Integration Point #3)
//=============================================================================

uint64_t hyperledger_audit_log(hyperledger_bridge_t* bridge,
                                hyperledger_audit_entry_t* entry)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_audit_log: NULL bridge or entry");
        return 0;
    }
    if (!bridge->config.enable_audit_log) return 0;
    return audit_log_internal(bridge, entry);
}

bool hyperledger_audit_verify_chain(const hyperledger_bridge_t* bridge,
                                     uint64_t* num_verified)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC || !num_verified) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_audit_verify_chain: NULL bridge or num_verified");
        if (num_verified) *num_verified = 0;
        return false;
    }

    if (!bridge->audit_ring || bridge->audit_count == 0) {
        *num_verified = 0;
        return true;  /* Empty chain is valid */
    }

    /* Determine range to verify */
    uint64_t start = 0;
    uint64_t count = bridge->audit_count;
    if (count > HYPERLEDGER_AUDIT_RING_SIZE) {
        start = count - HYPERLEDGER_AUDIT_RING_SIZE;
        count = HYPERLEDGER_AUDIT_RING_SIZE;
    }

    *num_verified = 0;
    uint8_t prev_hash[HYPERLEDGER_HASH_SIZE] = {0};

    for (uint64_t i = 0; i < count; i++) {
        uint64_t idx = (start + i) % HYPERLEDGER_AUDIT_RING_SIZE;
        const hyperledger_audit_entry_t* entry = &bridge->audit_ring[idx];

        /* Verify prev_hash matches (skip first entry — its prev is the initial state) */
        if (i > 0) {
            if (memcmp(entry->prev_hash, prev_hash, HYPERLEDGER_HASH_SIZE) != 0) {
                /* Hash chain broken! */
                return false;
            }
        }

        /* Recompute this entry's hash and verify */
        uint8_t computed_hash[HYPERLEDGER_HASH_SIZE];
        compute_entry_hash(entry, computed_hash);
        if (memcmp(entry->entry_hash, computed_hash, HYPERLEDGER_HASH_SIZE) != 0) {
            /* Entry hash doesn't match — tampered */
            return false;
        }

        memcpy(prev_hash, entry->entry_hash, HYPERLEDGER_HASH_SIZE);
        (*num_verified)++;
    }

    return true;
}

int hyperledger_audit_get_entry(const hyperledger_bridge_t* bridge,
                                 uint64_t sequence_id,
                                 hyperledger_audit_entry_t* entry)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_audit_get_entry: NULL bridge or entry");
        return -1;
    }
    if (!bridge->audit_ring || sequence_id == 0) return -1;

    /* Check if entry is still in ring buffer */
    uint64_t oldest = 1;
    if (bridge->audit_count > HYPERLEDGER_AUDIT_RING_SIZE) {
        oldest = bridge->audit_count - HYPERLEDGER_AUDIT_RING_SIZE + 1;
    }
    if (sequence_id < oldest || sequence_id > bridge->audit_count) return -1;

    /* Find in ring */
    uint64_t ring_idx = (sequence_id - 1) % HYPERLEDGER_AUDIT_RING_SIZE;
    const hyperledger_audit_entry_t* stored = &bridge->audit_ring[ring_idx];
    if (stored->sequence_id != sequence_id) return -1;  /* Sanity check */

    *entry = *stored;
    return 0;
}

uint64_t hyperledger_audit_count(const hyperledger_bridge_t* bridge)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_audit_count: NULL or invalid bridge");
        return 0;
    }
    return bridge->audit_count;
}

uint32_t hyperledger_snapshot_create(hyperledger_bridge_t* bridge)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_snapshot_create: NULL or invalid bridge");
        return 0;
    }
    if (!bridge->config.enable_snapshots) return 0;
    if (bridge->snapshot_count >= bridge->config.max_snapshots) return 0;

    bridge->snapshot_count++;

    /* Audit log */
    if (bridge->config.enable_audit_log) {
        hyperledger_audit_entry_t entry = {0};
        entry.type = AUDIT_CHECKPOINT;
        audit_log_internal(bridge, &entry);
    }

    bridge->stats.snapshots_created++;
    return bridge->snapshot_count;
}

int hyperledger_snapshot_rollback(hyperledger_bridge_t* bridge,
                                   uint32_t snapshot_idx)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_snapshot_rollback: NULL or invalid bridge");
        return -1;
    }
    if (snapshot_idx == 0 || snapshot_idx > bridge->snapshot_count) return -1;

    /* Audit log */
    if (bridge->config.enable_audit_log) {
        hyperledger_audit_entry_t entry = {0};
        entry.type = AUDIT_ROLLBACK;
        entry.data.rollback.snapshot_idx = snapshot_idx;
        entry.data.rollback.loss_at_rollback = bridge->last_committed_loss;
        audit_log_internal(bridge, &entry);
    }

    bridge->stats.rollbacks_performed++;

    LOG_INFO(LOG_MODULE, "Rollback to snapshot %u (loss was %.4f)",
             snapshot_idx, bridge->last_committed_loss);
    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int hyperledger_bridge_get_stats(const hyperledger_bridge_t* bridge,
                                  hyperledger_bridge_stats_t* stats)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_bridge_get_stats: NULL bridge or stats");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void hyperledger_bridge_reset_stats(hyperledger_bridge_t* bridge)
{
    if (!bridge || bridge->magic != HYPERLEDGER_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hyperledger_bridge_reset_stats: NULL or invalid bridge");
        return;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}
