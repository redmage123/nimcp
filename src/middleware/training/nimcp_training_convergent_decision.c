//=============================================================================
// nimcp_training_convergent_decision.c - Convergent Training Decision
//=============================================================================
//
// WHAT: Evidence accumulator for multi-module training control decisions
// WHY:  Replace flat gate chains with convergent evidence from arousal,
//       inflammation, FEP, instability, Portia, emotion, and BG
// HOW:  Each module submits evidence; EMA convergence detection resolves
//       consensus action and weighted modulation factors
//
//=============================================================================

#include <stddef.h>
#include "middleware/training/nimcp_training_convergent_decision.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include <string.h>
#include <math.h>

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_convergent_decision)

/*=============================================================================
 * Internal Structures
 *===========================================================================*/

/** Stored copy of submitted evidence */
typedef struct {
    training_evidence_t evidence;
    bool occupied;
} evidence_slot_t;

/** Session internal state */
struct training_convergent_session {
    training_convergent_config_t config;

    /* Evidence storage */
    evidence_slot_t slots[TRAINING_CONVERGENT_MAX_EVIDENCE];
    uint32_t num_evidence;

    /* EMA convergence tracking */
    float current_confidence;    /**< Running weighted-average confidence */
    float ema_delta;             /**< EMA of confidence deltas */
    uint32_t submission_count;   /**< Total submissions for EMA tracking */
    bool converged;
};

/*=============================================================================
 * Default Config
 *===========================================================================*/

training_convergent_config_t training_convergent_default_config(void)
{
    training_convergent_config_t config;
    memset(&config, 0, sizeof(config));
    config.enabled = true;
    config.convergence_threshold = TRAINING_CONVERGENT_DEFAULT_CONVERGENCE_THRESHOLD;
    config.ema_alpha = TRAINING_CONVERGENT_DEFAULT_EMA_ALPHA;
    config.min_submissions = TRAINING_CONVERGENT_DEFAULT_MIN_SUBMISSIONS;
    config.pause_urgency_threshold = TRAINING_CONVERGENT_DEFAULT_PAUSE_URGENCY_THRESHOLD;
    return config;
}

/*=============================================================================
 * Lifecycle
 *===========================================================================*/

training_convergent_session_t* training_convergent_session_create(
    const training_convergent_config_t* config)
{
    training_convergent_config_t default_config;
    if (!config) {
        default_config = training_convergent_default_config();
        config = &default_config;
    }

    training_convergent_session_t* session =
        (training_convergent_session_t*)nimcp_calloc(1, sizeof(*session));
    if (!session) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate training convergent session");
        return NULL;
    }

    memcpy(&session->config, config, sizeof(*config));
    session->num_evidence = 0;
    session->current_confidence = 0.0f;
    session->ema_delta = 1.0f;  /* Start high so convergence doesn't trigger immediately */
    session->submission_count = 0;
    session->converged = false;

    NIMCP_LOGGING_DEBUG("Training convergent session created (threshold=%.3f, alpha=%.2f, min_sub=%u)",
        config->convergence_threshold, config->ema_alpha, config->min_submissions);

    return session;
}

void training_convergent_session_destroy(training_convergent_session_t* session)
{
    if (!session) return;
    nimcp_free(session);
}

/*=============================================================================
 * Evidence Submission
 *===========================================================================*/

int training_convergent_submit_evidence(
    training_convergent_session_t* session,
    const training_evidence_t* evidence)
{
    if (!session || !evidence) {
        return -1;
    }

    if (session->num_evidence >= TRAINING_CONVERGENT_MAX_EVIDENCE) {
        NIMCP_LOGGING_WARN("Training convergent session full (%u/%u), evidence from '%s' dropped",
            session->num_evidence, TRAINING_CONVERGENT_MAX_EVIDENCE,
            evidence->source_name ? evidence->source_name : "unknown");
        return -1;
    }

    /* Validate evidence type */
    if (evidence->type >= TRAINING_EVIDENCE_TYPE_COUNT) {
        NIMCP_LOGGING_WARN("Invalid evidence type %d from '%s'",
            evidence->type, evidence->source_name ? evidence->source_name : "unknown");
        return -1;
    }

    /* Store the evidence */
    evidence_slot_t* slot = &session->slots[session->num_evidence];
    memcpy(&slot->evidence, evidence, sizeof(*evidence));
    slot->occupied = true;
    session->num_evidence++;

    /* Update EMA convergence tracking */
    float conf = evidence->confidence;
    if (conf < 0.0f) conf = 0.0f;
    if (conf > 1.0f) conf = 1.0f;

    float prev_confidence = session->current_confidence;
    session->submission_count++;
    float alpha = session->config.ema_alpha;

    /* True EMA of confidence (not running mean)
     * WHY: Running mean weight = 1/count converges too slowly for real-time
     *      convergence detection. EMA with fixed alpha gives recent evidence
     *      exponentially more influence, matching the "EMA" naming intent.
     */
    session->current_confidence = alpha * conf + (1.0f - alpha) * prev_confidence;

    /* EMA of deltas */
    float delta = fabsf(session->current_confidence - prev_confidence);
    session->ema_delta = alpha * delta + (1.0f - alpha) * session->ema_delta;

    /* Check convergence */
    if (session->submission_count >= session->config.min_submissions &&
        session->ema_delta < session->config.convergence_threshold) {
        session->converged = true;
    }

    NIMCP_LOGGING_DEBUG("Evidence from '%s': type=%d conf=%.2f urgency=%.2f lr=%.3f (ema_delta=%.4f, %s)",
        evidence->source_name ? evidence->source_name : "?",
        evidence->type, evidence->confidence, evidence->urgency, evidence->lr_factor,
        session->ema_delta, session->converged ? "CONVERGED" : "accumulating");

    return 0;
}

/*=============================================================================
 * Decision Computation
 *===========================================================================*/

int training_convergent_compute_decision(
    training_convergent_session_t* session,
    training_convergent_decision_t* decision)
{
    if (!session || !decision) {
        return -1;
    }

    memset(decision, 0, sizeof(*decision));

    if (session->num_evidence == 0) {
        /* No evidence: default to CONTINUE */
        decision->consensus_action = TRAINING_EVIDENCE_CONTINUE;
        decision->consensus_confidence = 0.0f;
        decision->lr_factor = 1.0f;
        decision->batch_factor = 1.0f;
        decision->grad_clip_factor = 1.0f;
        decision->urgency = 0.0f;
        decision->converged = false;
        decision->ema_delta = session->ema_delta;
        return 0;
    }

    /* --- Vote counting and factor accumulation --- */
    float vote_weights[TRAINING_EVIDENCE_TYPE_COUNT];
    memset(vote_weights, 0, sizeof(vote_weights));

    float total_confidence = 0.0f;
    float weighted_urgency = 0.0f;
    float max_urgency = 0.0f;

    /* For geometric mean: accumulate sum of conf_i * ln(factor_i) */
    float lr_log_sum = 0.0f;
    float batch_log_sum = 0.0f;
    float grad_clip_log_sum = 0.0f;
    float factor_weight_sum = 0.0f;

    uint32_t pause_count = 0;
    uint32_t continue_count = 0;
    uint32_t rollback_count = 0;

    for (uint32_t i = 0; i < session->num_evidence; i++) {
        if (!session->slots[i].occupied) continue;

        const training_evidence_t* e = &session->slots[i].evidence;
        float conf = e->confidence;
        if (conf < 0.0f) conf = 0.0f;
        if (conf > 1.0f) conf = 1.0f;

        /* Vote counting weighted by confidence */
        if (e->type < TRAINING_EVIDENCE_TYPE_COUNT) {
            vote_weights[e->type] += conf;
        }

        /* Track specific action counts */
        if (e->type == TRAINING_EVIDENCE_PAUSE) pause_count++;
        if (e->type == TRAINING_EVIDENCE_CONTINUE) continue_count++;
        if (e->type == TRAINING_EVIDENCE_ROLLBACK) rollback_count++;

        /* Urgency accumulation */
        float urg = e->urgency;
        if (urg < 0.0f) urg = 0.0f;
        if (urg > 1.0f) urg = 1.0f;
        weighted_urgency += conf * urg;
        if (urg > max_urgency) max_urgency = urg;

        total_confidence += conf;

        /* Factor accumulation for geometric mean */
        /* Clamp factors to valid ranges */
        float lr = e->lr_factor;
        if (lr < 0.01f) lr = 0.01f;
        if (lr > 2.0f) lr = 2.0f;

        float batch = e->batch_factor;
        if (batch < 0.5f) batch = 0.5f;
        if (batch > 2.0f) batch = 2.0f;

        float gc = e->grad_clip_factor;
        if (gc < 0.1f) gc = 0.1f;
        if (gc > 2.0f) gc = 2.0f;

        lr_log_sum += conf * logf(lr);
        batch_log_sum += conf * logf(batch);
        grad_clip_log_sum += conf * logf(gc);
        factor_weight_sum += conf;
    }

    /* --- Consensus action: most-voted weighted by confidence --- */
    training_evidence_type_t best_action = TRAINING_EVIDENCE_CONTINUE;
    float best_weight = -1.0f;
    for (int t = 0; t < TRAINING_EVIDENCE_TYPE_COUNT; t++) {
        if (vote_weights[t] > best_weight) {
            best_weight = vote_weights[t];
            best_action = (training_evidence_type_t)t;
        }
    }

    /* --- Compute weighted factors via geometric mean --- */
    float lr_factor = 1.0f;
    float batch_factor = 1.0f;
    float grad_clip_factor = 1.0f;

    if (factor_weight_sum > 0.0f) {
        lr_factor = expf(lr_log_sum / factor_weight_sum);
        batch_factor = expf(batch_log_sum / factor_weight_sum);
        grad_clip_factor = expf(grad_clip_log_sum / factor_weight_sum);
    }

    /* --- Urgency: confidence-weighted mean --- */
    float urgency = (total_confidence > 0.0f)
        ? (weighted_urgency / total_confidence)
        : 0.0f;

    /* --- Forced pause on high urgency --- */
    if (urgency > session->config.pause_urgency_threshold) {
        best_action = TRAINING_EVIDENCE_PAUSE;
        NIMCP_LOGGING_WARN("Training convergent: urgency %.3f > threshold %.3f, forcing PAUSE",
            urgency, session->config.pause_urgency_threshold);
    }

    /* --- Consensus confidence --- */
    float consensus_confidence = (total_confidence > 0.0f)
        ? (best_weight / total_confidence)
        : 0.0f;

    /* --- Fill output --- */
    decision->consensus_action = best_action;
    decision->consensus_confidence = consensus_confidence;
    decision->lr_factor = lr_factor;
    decision->batch_factor = batch_factor;
    decision->grad_clip_factor = grad_clip_factor;
    decision->urgency = urgency;
    decision->num_contributors = session->num_evidence;
    decision->num_for_pause = pause_count;
    decision->num_for_continue = continue_count;
    decision->num_for_rollback = rollback_count;
    decision->converged = session->converged;
    decision->ema_delta = session->ema_delta;

    NIMCP_LOGGING_DEBUG("Convergent decision: action=%d conf=%.2f lr=%.3f batch=%.3f urgency=%.2f "
        "contributors=%u converged=%d",
        best_action, consensus_confidence, lr_factor, batch_factor, urgency,
        session->num_evidence, session->converged);

    return 0;
}

/*=============================================================================
 * Reset
 *===========================================================================*/

int training_convergent_session_reset(training_convergent_session_t* session)
{
    if (!session) {
        return -1;
    }

    memset(session->slots, 0, sizeof(session->slots));
    session->num_evidence = 0;
    session->current_confidence = 0.0f;
    session->ema_delta = 1.0f;
    session->submission_count = 0;
    session->converged = false;

    return 0;
}
