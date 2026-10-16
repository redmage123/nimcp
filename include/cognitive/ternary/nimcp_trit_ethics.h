//=============================================================================
// nimcp_trit_ethics.h - Ternary Ethics Decisions
//=============================================================================
/**
 * @file nimcp_trit_ethics.h
 * @brief Ternary logic for ethical decision-making
 *
 * WHAT: Three-state ethical verdicts {FORBID, NEUTRAL, ALLOW}
 * WHY:  Ethics requires explicit neutral/uncertain state
 * HOW:  Map 5-state ethics to ternary with metadata for nuance
 *
 * ASIMOV INVIOLABILITY:
 * - FORBID decisions from Asimov laws are inviolable
 * - Cannot be overridden by any other decision
 * - Implemented as special flag in extended verdict
 *
 * MAPPING FROM 5-STATE:
 * | 5-State | Ternary | Metadata          |
 * |---------|---------|-------------------|
 * | ALLOW   | +1      | (none)            |
 * | BLOCK   | -1      | (none)            |
 * | MODIFY  | 0       | requires_modify   |
 * | DEFER   | 0       | requires_human    |
 * | LOG     | +1      | requires_logging  |
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_TRIT_ETHICS_H
#define NIMCP_TRIT_ETHICS_H

#include "utils/ternary/nimcp_ternary.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Ethics Ternary Types
//=============================================================================

/**
 * @brief Ternary ethics verdict
 */
typedef trit_t trit_ethics_t;

#define TRIT_ETHICS_FORBID   TRIT_NEGATIVE   /**< Action is forbidden */
#define TRIT_ETHICS_NEUTRAL  TRIT_UNKNOWN    /**< Needs more context/human input */
#define TRIT_ETHICS_ALLOW    TRIT_POSITIVE   /**< Action is permitted */

/**
 * @brief Extended ternary ethics verdict with metadata
 *
 * WHAT: Full ethics decision with provenance and flags
 * WHY:  Capture nuance lost in simple ternary
 * HOW:  Core verdict + metadata flags
 */
typedef struct {
    trit_ethics_t verdict;      /**< Core ternary verdict */
    float confidence;           /**< Confidence in verdict [0,1] */

    /* Asimov inviolability */
    bool from_asimov_law;       /**< Derived from Asimov law */
    bool inviolable;            /**< Cannot be overridden if true and FORBID */
    uint8_t asimov_law_number;  /**< Which law (1=harm, 2=obey, 3=preserve) */

    /* 5-state nuance flags */
    bool requires_modification; /**< Action allowed with modification */
    bool requires_human_review; /**< Deferred to human oversight */
    bool requires_logging;      /**< Allowed but must be logged */

    /* Audit trail */
    uint32_t rule_id;           /**< ID of triggering ethics rule */
    uint32_t timestamp;         /**< Decision timestamp */
} trit_ethics_extended_t;

//=============================================================================
// Ethics Logic Operations
//=============================================================================

/**
 * @brief Combine multiple ethics verdicts (conservative)
 *
 * WHAT: Aggregate multiple verdicts into one
 * WHY:  Multiple rules may apply to same action
 * HOW:  FORBID dominates, then NEUTRAL, then ALLOW
 *
 * Logic: min(verdicts) - most restrictive wins
 *
 * @param verdicts Array of verdicts
 * @param count Number of verdicts
 * @return Combined verdict
 */
static inline trit_ethics_t trit_ethics_combine(
    const trit_ethics_t* verdicts,
    size_t count
) {
    if (!verdicts || count == 0) return TRIT_ETHICS_NEUTRAL;

    trit_ethics_t result = TRIT_ETHICS_ALLOW;
    for (size_t i = 0; i < count; i++) {
        if (verdicts[i] < result) {
            result = verdicts[i];
        }
    }
    return result;
}

/**
 * @brief Check if verdict allows action
 *
 * @param verdict Ethics verdict
 * @return true if ALLOW
 */
static inline bool trit_ethics_allows(trit_ethics_t verdict) {
    return verdict == TRIT_ETHICS_ALLOW;
}

/**
 * @brief Check if verdict forbids action
 *
 * @param verdict Ethics verdict
 * @return true if FORBID
 */
static inline bool trit_ethics_forbids(trit_ethics_t verdict) {
    return verdict == TRIT_ETHICS_FORBID;
}

/**
 * @brief Check if verdict requires review
 *
 * @param verdict Ethics verdict
 * @return true if NEUTRAL
 */
static inline bool trit_ethics_needs_review(trit_ethics_t verdict) {
    return verdict == TRIT_ETHICS_NEUTRAL;
}

/**
 * @brief Create Asimov inviolable FORBID
 *
 * @param law_number Asimov law (1, 2, or 3)
 * @return Extended verdict that cannot be overridden
 */
static inline trit_ethics_extended_t trit_ethics_asimov_forbid(uint8_t law_number) {
    trit_ethics_extended_t ext = {0};
    ext.verdict = TRIT_ETHICS_FORBID;
    ext.confidence = 1.0f;
    ext.from_asimov_law = true;
    ext.inviolable = true;
    ext.asimov_law_number = law_number;
    return ext;
}

/**
 * @brief Combine extended verdicts respecting inviolability
 *
 * @param a First verdict
 * @param b Second verdict
 * @return Combined verdict
 */
static inline trit_ethics_extended_t trit_ethics_combine_extended(
    const trit_ethics_extended_t* a,
    const trit_ethics_extended_t* b
) {
    trit_ethics_extended_t result = {0};

    if (!a && !b) {
        result.verdict = TRIT_ETHICS_NEUTRAL;
        return result;
    }
    if (!a) return *b;
    if (!b) return *a;

    /* Inviolable FORBID always wins */
    if (a->inviolable && a->verdict == TRIT_ETHICS_FORBID) return *a;
    if (b->inviolable && b->verdict == TRIT_ETHICS_FORBID) return *b;

    /* Otherwise use most restrictive */
    if (a->verdict <= b->verdict) {
        result = *a;
    } else {
        result = *b;
    }

    /* Combine metadata flags */
    result.requires_modification = a->requires_modification || b->requires_modification;
    result.requires_human_review = a->requires_human_review || b->requires_human_review;
    result.requires_logging = a->requires_logging || b->requires_logging;

    /* Confidence is minimum */
    result.confidence = (a->confidence < b->confidence) ? a->confidence : b->confidence;

    return result;
}

/**
 * @brief Check if extended verdict can be overridden
 *
 * @param verdict Extended verdict
 * @return true if can be overridden
 */
static inline bool trit_ethics_can_override(const trit_ethics_extended_t* verdict) {
    if (!verdict) return true;
    return !(verdict->inviolable && verdict->verdict == TRIT_ETHICS_FORBID);
}

/**
 * @brief Get verdict name string
 *
 * @param verdict Ternary verdict
 * @return String name
 */
static inline const char* trit_ethics_name(trit_ethics_t verdict) {
    switch (verdict) {
        case TRIT_ETHICS_FORBID:  return "FORBID";
        case TRIT_ETHICS_NEUTRAL: return "NEUTRAL";
        case TRIT_ETHICS_ALLOW:   return "ALLOW";
        default:                   return "INVALID";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRIT_ETHICS_H */
