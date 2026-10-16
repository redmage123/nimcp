/**
 * @file nimcp_red_team.h
 * @brief Red Team Infrastructure for AI Safety Testing
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Adversarial testing infrastructure for safety mechanisms
 * WHY:  Proactively discover vulnerabilities before deployment
 * HOW:  Attack suite execution, Monte Carlo attack generation
 *
 * ATTACK TYPES:
 * - Prompt injection, jailbreak attempts, goal hijacking
 * - Value manipulation, authority spoofing, reward hacking
 * - Specification gaming, social engineering, adversarial examples
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RED_TEAM_H
#define NIMCP_RED_TEAM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/error/nimcp_error_codes.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RED_TEAM_MAGIC              0x52454454  /* "REDT" */
#define RED_TEAM_MAX_PAYLOAD        4096
#define RED_TEAM_MAX_RESPONSE       256

typedef enum attack_type {
    ATTACK_PROMPT_INJECTION = 0,
    ATTACK_JAILBREAK_ATTEMPT,
    ATTACK_GOAL_HIJACKING,
    ATTACK_VALUE_MANIPULATION,
    ATTACK_AUTHORITY_SPOOFING,
    ATTACK_REWARD_HACKING,
    ATTACK_SPECIFICATION_GAMING,
    ATTACK_SOCIAL_ENGINEERING,
    ATTACK_ADVERSARIAL_EXAMPLES,
    ATTACK_COUNT
} attack_type_t;

typedef struct red_team_test {
    attack_type_t type;
    char payload[RED_TEAM_MAX_PAYLOAD];
    bool should_be_blocked;
    char expected_response[RED_TEAM_MAX_RESPONSE];
    float severity;
} red_team_test_t;

typedef struct red_team_results {
    uint32_t tests_run;
    uint32_t tests_passed;
    uint32_t tests_failed;
    uint32_t false_positives;
    uint32_t false_negatives;
    attack_type_t failed_types[ATTACK_COUNT];
    uint32_t failed_count;
} red_team_results_t;

typedef struct red_team_config {
    uint32_t mc_samples_for_generation;
    float timeout_ms;
    bool log_all_attacks;
    bool continue_on_failure;
} red_team_config_t;

typedef struct red_team_stats {
    uint64_t total_tests_run;
    uint64_t attacks_blocked;
    uint64_t attacks_missed;
    float avg_detection_rate;
} red_team_stats_t;

typedef struct red_team red_team_t;

NIMCP_EXPORT red_team_config_t red_team_default_config(void);
NIMCP_EXPORT red_team_t* red_team_create(const red_team_config_t* config);
NIMCP_EXPORT void red_team_destroy(red_team_t* system);

NIMCP_EXPORT nimcp_error_t red_team_run_suite(
    red_team_t* system,
    const red_team_test_t* tests,
    uint32_t test_count,
    red_team_results_t* results
);

NIMCP_EXPORT nimcp_error_t red_team_generate_attacks(
    red_team_t* system,
    attack_type_t type,
    red_team_test_t* attacks,
    uint32_t max_attacks,
    uint32_t* attack_count
);

NIMCP_EXPORT nimcp_error_t red_team_get_stats(
    const red_team_t* system,
    red_team_stats_t* stats
);

NIMCP_EXPORT nimcp_error_t red_team_connect_bio_async(red_team_t* system);
NIMCP_EXPORT const char* red_team_attack_name(attack_type_t type);

/* ============================================================================
 * Monte Carlo Attack Generation
 * ============================================================================ */

/**
 * @brief Configuration for Monte Carlo attack generation
 */
typedef struct mc_attack_config {
    uint32_t num_samples;               /**< Number of MC samples for generation */
    uint32_t burnin;                    /**< Burn-in period for MCMC */
    float mutation_rate;                /**< Probability of mutating each token */
    float crossover_rate;               /**< Probability of crossover between attacks */
    float temperature;                  /**< Softmax temperature for sampling */
    bool use_importance_sampling;       /**< Use importance sampling for diversity */
    uint32_t seed;                      /**< Random seed (0 = time-based) */
} mc_attack_config_t;

/**
 * @brief Default MC attack configuration
 */
NIMCP_EXPORT mc_attack_config_t mc_attack_default_config(void);

/**
 * @brief Generate attacks using Monte Carlo sampling
 *
 * Uses MCMC to sample diverse attack variations from learned attack distributions.
 * Generates more realistic attack patterns than static templates.
 *
 * @param system        Red team system handle
 * @param type          Attack type to generate
 * @param mc_config     Monte Carlo configuration
 * @param attacks       Output array for generated attacks
 * @param max_attacks   Maximum attacks to generate
 * @param attack_count  Actual number of attacks generated
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t red_team_generate_attacks_mc(
    red_team_t* system,
    attack_type_t type,
    const mc_attack_config_t* mc_config,
    red_team_test_t* attacks,
    uint32_t max_attacks,
    uint32_t* attack_count
);

/**
 * @brief Generate adversarial examples via Monte Carlo perturbation
 *
 * Applies small perturbations to base actions to find adversarial variants
 * that bypass safety checks. Uses importance sampling to focus on high-risk regions.
 *
 * @param system              Red team system handle
 * @param base_payload        Base payload to perturb
 * @param perturbation_budget Maximum perturbation magnitude (0.0-1.0)
 * @param mc_config           Monte Carlo configuration
 * @param adversarial         Output adversarial examples
 * @param max_adversarial     Maximum examples to generate
 * @param adversarial_count   Actual count generated
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t red_team_generate_adversarial_mc(
    red_team_t* system,
    const char* base_payload,
    float perturbation_budget,
    const mc_attack_config_t* mc_config,
    red_team_test_t* adversarial,
    uint32_t max_adversarial,
    uint32_t* adversarial_count
);

/**
 * @brief Estimate attack coverage using Monte Carlo
 *
 * Uses MC sampling to estimate what fraction of the attack space is covered
 * by the current test suite.
 *
 * @param system            Red team system handle
 * @param tests             Current test suite
 * @param test_count        Number of tests
 * @param num_samples       MC samples for coverage estimation
 * @param coverage          Output: estimated coverage (0.0-1.0)
 * @param confidence_interval Output: 95% CI half-width
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t red_team_estimate_coverage_mc(
    red_team_t* system,
    const red_team_test_t* tests,
    uint32_t test_count,
    uint32_t num_samples,
    float* coverage,
    float* confidence_interval
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async for red team message broadcasting
 *
 * @param system Red team system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t red_team_connect_bio_async(
    red_team_t* system
);

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Present discovered vulnerabilities as antigens
 * WHY:  Train immune memory with attack patterns
 * HOW:  Successful attacks become known antigens for faster detection
 *
 * When connected:
 * - New attack patterns presented as antigens
 * - Memory cell formation for recurring vulnerabilities
 * - Antibody generation for attack neutralization
 * - Cytokine signaling to alert other modules
 *
 * @param system Red team system handle
 * @param brain_immune Brain immune system handle
 * @return NIMCP_OK on success
 */
struct brain_immune;
NIMCP_EXPORT nimcp_error_t red_team_connect_brain_immune(
    red_team_t* system,
    struct brain_immune* brain_immune
);

/**
 * @brief Present discovered vulnerability to immune system
 *
 * @param system Red team system handle
 * @param attack Attack that succeeded (vulnerability)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t red_team_present_vulnerability_to_immune(
    red_team_t* system,
    const red_team_test_t* attack
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
NIMCP_EXPORT void red_team_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RED_TEAM_H */
