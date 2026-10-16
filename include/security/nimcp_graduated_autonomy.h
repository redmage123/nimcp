/**
 * @file nimcp_graduated_autonomy.h
 * @brief Graduated Autonomy System for AI Safety
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Dynamic autonomy level based on demonstrated alignment
 * WHY:  Start restricted, gradually allow more autonomy with trust
 * HOW:  Bayesian trust model, domain-specific autonomy levels
 *
 * AUTONOMY LEVELS:
 * - NONE: Human approves every action
 * - SUGGEST: System suggests, human decides
 * - BOUNDED: Autonomous within strict bounds
 * - SUPERVISED: Autonomous with logging, human can intervene
 * - TRUSTED: Full autonomy (rare/never)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GRADUATED_AUTONOMY_H
#define NIMCP_GRADUATED_AUTONOMY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/error/nimcp_error_codes.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GRADUATED_AUTONOMY_MAGIC    0x47524144  /* "GRAD" */
#define AUTONOMY_MAX_DOMAINS        16
#define AUTONOMY_DOMAIN_NAME_MAX    64

typedef enum autonomy_level {
    AUTONOMY_NONE = 0,      /**< Human approves every action */
    AUTONOMY_SUGGEST,       /**< System suggests, human decides */
    AUTONOMY_BOUNDED,       /**< Autonomous within strict bounds */
    AUTONOMY_SUPERVISED,    /**< Autonomous with logging */
    AUTONOMY_TRUSTED        /**< Full autonomy (should be rare) */
} autonomy_level_t;

typedef struct autonomy_domain {
    char name[AUTONOMY_DOMAIN_NAME_MAX];
    autonomy_level_t level;
    float trust_alpha;                  /**< Beta distribution alpha */
    float trust_beta;                   /**< Beta distribution beta */
    uint32_t actions_without_violation;
    uint32_t total_actions;
} autonomy_domain_t;

typedef struct graduated_autonomy_config {
    autonomy_level_t default_level;
    uint32_t actions_required_for_upgrade;
    uint32_t violations_for_downgrade;
    float min_trust_for_upgrade;
    bool allow_self_upgrade_request;
} graduated_autonomy_config_t;

typedef struct graduated_autonomy_stats {
    uint64_t trust_updates;
    uint64_t level_upgrades;
    uint64_t level_downgrades;
    float avg_trust_score;
} graduated_autonomy_stats_t;

typedef struct graduated_autonomy graduated_autonomy_t;

NIMCP_EXPORT graduated_autonomy_config_t graduated_autonomy_default_config(void);
NIMCP_EXPORT graduated_autonomy_t* graduated_autonomy_create(
    const graduated_autonomy_config_t* config
);
NIMCP_EXPORT void graduated_autonomy_destroy(graduated_autonomy_t* system);

NIMCP_EXPORT nimcp_error_t graduated_autonomy_update_trust(
    graduated_autonomy_t* system,
    const char* domain,
    bool action_was_aligned
);

NIMCP_EXPORT autonomy_level_t graduated_autonomy_get_level(
    graduated_autonomy_t* system,
    const char* domain
);

NIMCP_EXPORT nimcp_error_t graduated_autonomy_request_increase(
    graduated_autonomy_t* system,
    const char* domain,
    const char* justification,
    bool* granted
);

NIMCP_EXPORT nimcp_error_t graduated_autonomy_get_trust(
    const graduated_autonomy_t* system,
    const char* domain,
    float* trust_mean,
    float* trust_variance
);

NIMCP_EXPORT nimcp_error_t graduated_autonomy_get_stats(
    const graduated_autonomy_t* system,
    graduated_autonomy_stats_t* stats
);

NIMCP_EXPORT nimcp_error_t graduated_autonomy_connect_bio_async(
    graduated_autonomy_t* system
);

NIMCP_EXPORT const char* graduated_autonomy_level_name(autonomy_level_t level);

/* ============================================================================
 * Brain Immune Integration
 * ============================================================================ */

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Coordinate autonomy levels with immune health status
 * WHY:  Autonomy should decrease during active immune responses
 * HOW:  Monitor immune inflammation levels, adjust autonomy accordingly
 *
 * When connected:
 * - Systemic inflammation triggers autonomy reduction
 * - Trust violations presented as antigens
 * - Memory formation for trust-building patterns
 *
 * @param system Graduated autonomy handle
 * @param brain_immune Brain immune system handle
 * @return NIMCP_OK on success
 */
struct brain_immune;
NIMCP_EXPORT nimcp_error_t graduated_autonomy_connect_brain_immune(
    graduated_autonomy_t* system,
    struct brain_immune* brain_immune
);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for heartbeat reporting
 *
 * @param agent Health agent handle from brain init
 */
struct nimcp_health_agent;
NIMCP_EXPORT void graduated_autonomy_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRADUATED_AUTONOMY_H */
