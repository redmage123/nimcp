/**
 * @file nimcp_training_convergent_decision.h
 * @brief Convergent Training Decision -- multi-module evidence accumulation for training control
 *
 * WHAT: Evidence accumulator where arousal, inflammation, FEP, instability, Portia, emotion,
 *       and BG each contribute evidence about what training should do next
 * WHY:  Replace flat gate chain (loss_nan OR grad_exploding -> PAUSE) with convergent agreement
 * HOW:  Each contributor produces a training_evidence_t; evidence accumulator detects convergence
 *
 * ARCHITECTURE:
 *   1. Each brain module submits training_evidence_t with source, type, factors, urgency, confidence
 *   2. Accumulator tracks vote counts, weighted factor aggregation, convergence via EMA
 *   3. training_convergent_compute_decision() resolves consensus action and modulation factors
 *   4. High urgency (>threshold) forces PAUSE regardless of vote counts
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#ifndef NIMCP_TRAINING_CONVERGENT_DECISION_H
#define NIMCP_TRAINING_CONVERGENT_DECISION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum evidence submissions per session */
#define TRAINING_CONVERGENT_MAX_EVIDENCE 32

/** Default convergence threshold (EMA delta below this = converged) */
#define TRAINING_CONVERGENT_DEFAULT_CONVERGENCE_THRESHOLD 0.01f

/** Default EMA smoothing factor */
#define TRAINING_CONVERGENT_DEFAULT_EMA_ALPHA 0.3f

/** Default minimum submissions before convergence can trigger */
#define TRAINING_CONVERGENT_DEFAULT_MIN_SUBMISSIONS 3

/** Default pause urgency threshold */
#define TRAINING_CONVERGENT_DEFAULT_PAUSE_URGENCY_THRESHOLD 0.9f

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief What kind of evidence a contributor provides
 *
 * WHAT: Categorizes the training action suggested by each contributor
 * WHY:  Consensus voting requires discrete action categories
 */
typedef enum {
    TRAINING_EVIDENCE_LR_MODULATION = 0,     /**< Suggests LR change */
    TRAINING_EVIDENCE_BATCH_MODULATION,       /**< Suggests batch size change */
    TRAINING_EVIDENCE_PAUSE,                  /**< Suggests pause */
    TRAINING_EVIDENCE_CHECKPOINT,             /**< Suggests checkpoint */
    TRAINING_EVIDENCE_CONTINUE,               /**< Training is fine */
    TRAINING_EVIDENCE_ROLLBACK,               /**< Rollback to checkpoint */
    TRAINING_EVIDENCE_TYPE_COUNT              /**< Sentinel */
} training_evidence_type_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Single evidence contribution from one brain module
 *
 * WHAT: One module's opinion about what training should do
 * WHY:  Decouple evidence production from decision aggregation
 * HOW:  Module fills this struct and submits to the session
 */
typedef struct {
    const char* source_name;              /**< e.g. "arousal", "instability", "portia" */
    training_evidence_type_t type;        /**< What action this contributor suggests */
    float lr_factor;                      /**< LR multiplier [0.01, 2.0] (1.0 = no change) */
    float batch_factor;                   /**< Batch multiplier [0.5, 2.0] (1.0 = no change) */
    float grad_clip_factor;               /**< Grad clip multiplier [0.1, 2.0] */
    float urgency;                        /**< How urgent (0=calm, 1=critical) */
    float confidence;                     /**< Confidence in this evidence [0,1] */
} training_evidence_t;

/**
 * @brief Accumulated convergent decision from all contributors
 *
 * WHAT: The resolved consensus output after evidence accumulation
 * WHY:  Single struct containing everything the training loop needs
 * HOW:  Computed by training_convergent_compute_decision()
 */
typedef struct {
    /* Consensus action */
    training_evidence_type_t consensus_action;
    float consensus_confidence;

    /* Weighted modulation factors */
    float lr_factor;                     /**< Geometric mean of all lr_factors, weighted by confidence */
    float batch_factor;                  /**< Geometric mean of all batch_factors, weighted by confidence */
    float grad_clip_factor;              /**< Geometric mean of all grad_clip_factors, weighted by confidence */

    /* Urgency consensus */
    float urgency;                       /**< Confidence-weighted mean urgency */

    /* Contributor counts */
    uint32_t num_contributors;
    uint32_t num_for_pause;
    uint32_t num_for_continue;
    uint32_t num_for_rollback;

    /* Convergence metadata */
    bool converged;
    float ema_delta;
} training_convergent_decision_t;

/**
 * @brief Configuration for convergent training decision sessions
 *
 * WHAT: Tuning knobs for convergence detection and forced-pause behavior
 * WHY:  Different training scenarios need different sensitivity
 */
typedef struct {
    bool enabled;                        /**< Master enable (default true) */
    float convergence_threshold;         /**< EMA delta threshold (default 0.01) */
    float ema_alpha;                     /**< Smoothing factor (default 0.3) */
    uint32_t min_submissions;            /**< Min evidence before convergence (default 3) */
    float pause_urgency_threshold;       /**< Urgency above this -> forced pause (default 0.9) */
} training_convergent_config_t;

/** Opaque session handle */
typedef struct training_convergent_session training_convergent_session_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * @brief Get default configuration
 * @return Config struct with default values
 */
training_convergent_config_t training_convergent_default_config(void);

/**
 * @brief Create a new convergent decision session
 * @param config Configuration (NULL for defaults)
 * @return Session handle, or NULL on failure
 */
training_convergent_session_t* training_convergent_session_create(
    const training_convergent_config_t* config);

/**
 * @brief Destroy a convergent decision session
 * @param session Session to destroy (NULL-safe)
 */
void training_convergent_session_destroy(training_convergent_session_t* session);

/**
 * @brief Submit evidence from one brain module
 * @param session Active session
 * @param evidence Evidence contribution
 * @return 0 on success, -1 on error
 */
int training_convergent_submit_evidence(
    training_convergent_session_t* session,
    const training_evidence_t* evidence);

/**
 * @brief Compute the convergent decision from accumulated evidence
 * @param session Active session with submitted evidence
 * @param decision Output decision struct
 * @return 0 on success, -1 on error
 */
int training_convergent_compute_decision(
    training_convergent_session_t* session,
    training_convergent_decision_t* decision);

/**
 * @brief Reset session state for reuse between training steps
 * @param session Session to reset
 * @return 0 on success, -1 on error
 */
int training_convergent_session_reset(training_convergent_session_t* session);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_CONVERGENT_DECISION_H */
