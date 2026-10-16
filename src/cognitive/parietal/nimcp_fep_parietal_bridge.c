/**
 * @file nimcp_fep_parietal_bridge.c
 * @brief FEP-Parietal bridge stub implementation
 *
 * Stub implementation providing minimal functionality for linking.
 * Full implementation pending.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/parietal/nimcp_fep_parietal_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(fep_parietal_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from fep_parietal_bridge module (instance-level) */
static inline void fep_parietal_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_fep_parietal_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fep_parietal_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_fep_parietal_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "FEP_PARIETAL_BRIDGE"


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

struct fep_parietal_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    fep_parietal_config_t config;
    bool enabled;
    float inflammation_level;
    float fatigue_level;
    fep_parietal_stats_t stats;
    fep_system_t* fep_system;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;

    /* Phase 5: Domain-specific prediction error tracking */
    float precision_weights[7];       /* Per-domain precision (inverse variance) */
    float prediction_errors[7];       /* Running PE per domain */
    float learning_rate_mods[7];      /* PE-modulated learning rates */
    uint64_t domain_update_counts[7]; /* Update counter per domain */

    /* Phase 5+: Hierarchical generative model state */
    float* level_means[FEP_PARIETAL_MAX_LEVELS];
    float* level_precisions[FEP_PARIETAL_MAX_LEVELS];
    uint32_t level_dims[FEP_PARIETAL_MAX_LEVELS];
    uint32_t num_levels;

    /* Linear transition model: predictions from level n+1 → level n */
    float* transition_weights[FEP_PARIETAL_MAX_LEVELS - 1]; /* dim[n] * dim[n+1] */

    /* Attention precision modulation */
    float* attention_precision;
    uint32_t attention_dim;

    /* PE history ring buffer for adaptive precision */
    float pe_history[64];
    uint32_t pe_history_idx;
    uint32_t pe_history_count;

    /* Training state */
    uint64_t training_samples;
    float training_loss_ema;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(fep_parietal_bridge)

/* Thread-local error message */
static _Thread_local char g_error_message[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    strncpy(g_error_message, msg, sizeof(g_error_message) - 1);
    g_error_message[sizeof(g_error_message) - 1] = '\0';
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

fep_parietal_config_t fep_parietal_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_default", 0.0f);


    fep_parietal_config_t config = {0};
    config.enabled = true;
    config.num_levels = 4;
    for (uint32_t i = 0; i < FEP_PARIETAL_MAX_LEVELS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && FEP_PARIETAL_MAX_LEVELS > 256) {
            fep_parietal_bridge_heartbeat("fep_parietal_loop",
                             (float)(i + 1) / (float)FEP_PARIETAL_MAX_LEVELS);
        }

        config.level_dims[i] = 32;
    }
    config.belief_learning_rate = NIMCP_LEARNING_RATE_COARSE;
    config.precision_learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config.policy_learning_rate = 0.05f;
    config.enable_active_inference = true;
    config.planning_horizon = 5;
    config.exploration_weight = 0.3f;
    config.action_temperature = NIMCP_TEMPERATURE_DEFAULT;
    config.initial_precision = 1.0f;
    config.min_precision = 0.01f;
    config.max_precision = 100.0f;
    config.adaptive_precision = true;
    config.enable_numerical_model = true;
    config.enable_spatial_model = true;
    config.enable_algebraic_model = true;
    config.enable_physical_model = true;
    config.inflammation_sensitivity = 0.5f;
    config.fatigue_sensitivity = 0.5f;
    return config;
}

fep_parietal_bridge_t* fep_parietal_bridge_create(const fep_parietal_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_create", 0.0f);


    fep_parietal_bridge_t* bridge = nimcp_calloc(1, sizeof(fep_parietal_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate fep_parietal_bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Initialize bridge base infrastructure (mutex, security, coordinator) */
    if (bridge_base_init(&bridge->base, 0, "fep_parietal") != 0) {
        set_error("bridge_base_init failed for fep_parietal");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                              "bridge_base_init failed for fep_parietal");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->config = config ? *config : fep_parietal_default_config();
    bridge->enabled = bridge->config.enabled;

    /* Phase 5: Initialize domain precision */
    for (int d = 0; d < 7; d++) {
        bridge->precision_weights[d] = bridge->config.initial_precision;
        bridge->learning_rate_mods[d] = 1.0f;
    }

    /* Phase 5+: Initialize hierarchical generative model */
    bridge->num_levels = bridge->config.num_levels;
    if (bridge->num_levels > FEP_PARIETAL_MAX_LEVELS)
        bridge->num_levels = FEP_PARIETAL_MAX_LEVELS;
    for (uint32_t l = 0; l < bridge->num_levels; l++) {
        uint32_t dim = bridge->config.level_dims[l];
        if (dim == 0) dim = 32;
        if (dim > FEP_PARIETAL_MAX_BELIEF_DIM) dim = FEP_PARIETAL_MAX_BELIEF_DIM;
        bridge->level_dims[l] = dim;
        bridge->level_means[l] = nimcp_calloc(dim, sizeof(float));
        bridge->level_precisions[l] = nimcp_calloc(dim, sizeof(float));
        if (bridge->level_precisions[l]) {
            for (uint32_t i = 0; i < dim; i++)
                bridge->level_precisions[l][i] = bridge->config.initial_precision;
        }
    }
    /* Initialize transition weights with small random-ish values (identity-like) */
    for (uint32_t l = 0; l + 1 < bridge->num_levels; l++) {
        uint32_t dim_lo = bridge->level_dims[l];
        uint32_t dim_hi = bridge->level_dims[l + 1];
        bridge->transition_weights[l] = nimcp_calloc(dim_lo * dim_hi, sizeof(float));
        if (bridge->transition_weights[l]) {
            uint32_t min_d = dim_lo < dim_hi ? dim_lo : dim_hi;
            for (uint32_t i = 0; i < min_d; i++)
                bridge->transition_weights[l][i * dim_hi + i] = 1.0f; /* identity diagonal */
        }
    }

    NIMCP_LOGGING_INFO("Created %s bridge", "fep_parietal");
    return bridge;
}

void fep_parietal_bridge_destroy(fep_parietal_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_destroy", 0.0f);


    if (bridge) {
        /* Free hierarchical model state */
        for (uint32_t l = 0; l < FEP_PARIETAL_MAX_LEVELS; l++) {
            nimcp_free(bridge->level_means[l]);
            nimcp_free(bridge->level_precisions[l]);
        }
        for (uint32_t l = 0; l + 1 < FEP_PARIETAL_MAX_LEVELS; l++) {
            nimcp_free(bridge->transition_weights[l]);
        }
        nimcp_free(bridge->attention_precision);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int fep_parietal_set_enabled(fep_parietal_bridge_t* bridge, bool enabled) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_set_ena", 0.0f);


    bridge->enabled = enabled;
    return 0;
}

bool fep_parietal_is_available(const fep_parietal_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_is_avai", 0.0f);


    return bridge && bridge->enabled;
}

/* ============================================================================
 * PREDICTIVE PROCESSING API
 * ============================================================================ */

int fep_parietal_update_beliefs(
    fep_parietal_bridge_t* bridge,
    const float* observations,
    uint32_t num_observations,
    fep_math_domain_t domain,
    fep_math_belief_t* beliefs
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_update_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, observations, sizeof(*observations));

    if (!bridge || !observations || !beliefs) return -1;

    /* Phase 5: Precision-weighted belief update */
    int d = (int)domain;
    if (d >= 0 && d < 7) {
        bridge->domain_update_counts[d]++;
    }

    if (!beliefs->mean) {
        beliefs->mean = nimcp_calloc(num_observations, sizeof(float));
        beliefs->precision = nimcp_calloc(num_observations, sizeof(float));
        beliefs->dim = num_observations;
        if (!beliefs->mean || !beliefs->precision) return -1;
        for (uint32_t i = 0; i < num_observations; i++) {
            beliefs->precision[i] = bridge->config.initial_precision;
        }
    }

    float lr = bridge->config.belief_learning_rate;
    if (d >= 0 && d < 7) lr *= bridge->learning_rate_mods[d];

    /* Inflammation/fatigue modulation */
    float mod = 1.0f - bridge->inflammation_level * bridge->config.inflammation_sensitivity * 0.3f
                     - bridge->fatigue_level * bridge->config.fatigue_sensitivity * 0.2f;
    lr *= fmaxf(0.3f, mod);

    float pe_sum = 0.0f;
    uint32_t dim = beliefs->dim < num_observations ? beliefs->dim : num_observations;
    for (uint32_t i = 0; i < dim; i++) {
        float pe = observations[i] - beliefs->mean[i];
        beliefs->mean[i] += lr * beliefs->precision[i] * pe;
        pe_sum += pe * pe;

        if (bridge->config.adaptive_precision) {
            float pe_sq = pe * pe;
            beliefs->precision[i] = beliefs->precision[i] * 0.99f +
                                    (1.0f / (pe_sq + 0.01f)) * 0.01f;
            beliefs->precision[i] = fmaxf(bridge->config.min_precision,
                                   fminf(bridge->config.max_precision, beliefs->precision[i]));
        }
    }

    beliefs->domain = domain;
    beliefs->confidence = 1.0f / (1.0f + sqrtf(pe_sum / (float)(dim > 0 ? dim : 1)));
    beliefs->surprise = sqrtf(pe_sum / (float)(dim > 0 ? dim : 1));

    if (d >= 0 && d < 7) {
        bridge->prediction_errors[d] = bridge->prediction_errors[d] * 0.9f +
                                       beliefs->surprise * 0.1f;
        bridge->precision_weights[d] = 1.0f / (bridge->prediction_errors[d] + 0.01f);
        bridge->learning_rate_mods[d] = fminf(3.0f, 1.0f + bridge->prediction_errors[d] * 0.5f);
    }

    /* Also update internal level-0 beliefs from observations */
    if (bridge->num_levels > 0 && bridge->level_means[0]) {
        uint32_t l0_dim = bridge->level_dims[0];
        uint32_t upd = l0_dim < num_observations ? l0_dim : num_observations;
        for (uint32_t i = 0; i < upd; i++) {
            float pe = observations[i] - bridge->level_means[0][i];
            bridge->level_means[0][i] += lr * pe;
        }
    }

    bridge->stats.belief_updates++;
    return 0;
}

int fep_parietal_predict(
    fep_parietal_bridge_t* bridge,
    const fep_math_belief_t* beliefs,
    fep_math_prediction_t* prediction
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_predict", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, beliefs, sizeof(*beliefs));

    if (!bridge || !beliefs || !prediction) return -1;
    memset(prediction, 0, sizeof(*prediction));

    uint32_t dim = beliefs->dim;
    if (dim == 0) return 0;

    prediction->predicted = nimcp_calloc(dim, sizeof(float));
    if (!prediction->predicted) return -1;
    prediction->dim = dim;

    /* Generate predictions: use transition weights from level 1→0 as the
       generative model, or fall back to identity (predict = beliefs.mean) */
    if (bridge->num_levels >= 2 && bridge->transition_weights[0] &&
        bridge->level_dims[0] > 0) {
        uint32_t dim_lo = bridge->level_dims[0];
        uint32_t dim_hi = bridge->level_dims[1];
        uint32_t out_dim = dim < dim_lo ? dim : dim_lo;
        uint32_t in_dim = beliefs->dim < dim_hi ? beliefs->dim : dim_hi;

        for (uint32_t i = 0; i < out_dim; i++) {
            float sum = 0.0f;
            for (uint32_t j = 0; j < in_dim; j++) {
                sum += bridge->transition_weights[0][i * dim_hi + j] * beliefs->mean[j];
            }
            prediction->predicted[i] = sum;
        }
    } else {
        /* Identity: predicted = beliefs.mean */
        uint32_t copy_dim = dim < beliefs->dim ? dim : beliefs->dim;
        for (uint32_t i = 0; i < copy_dim; i++)
            prediction->predicted[i] = beliefs->mean[i];
    }

    /* Compute error magnitude from belief precision (expected PE) */
    float expected_pe = 0.0f;
    if (beliefs->precision) {
        for (uint32_t i = 0; i < dim; i++) {
            float p = beliefs->precision[i];
            if (p > 1e-6f) expected_pe += 1.0f / p;
        }
        expected_pe = sqrtf(expected_pe / (float)dim);
    }
    prediction->error_magnitude = expected_pe;
    prediction->free_energy = 0.5f * expected_pe * expected_pe * (float)dim;

    bridge->stats.predictions_made++;
    return 0;
}

int fep_parietal_prediction_error(
    fep_parietal_bridge_t* bridge,
    const float* predicted,
    const float* actual,
    uint32_t dim,
    fep_math_prediction_t* error
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_predict", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, predicted, sizeof(*predicted));

    if (!bridge || !predicted || !actual || !error) return -1;

    memset(error, 0, sizeof(*error));
    error->dim = dim;

    /* Phase 5: Real precision-weighted prediction error */
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float e = actual[i] - predicted[i];
        sum_sq += e * e;
    }
    error->error_magnitude = sqrtf(sum_sq / (float)(dim > 0 ? dim : 1));
    error->free_energy = 0.5f * sum_sq;

    bridge->stats.predictions_made++;
    bridge->stats.avg_prediction_error =
        bridge->stats.avg_prediction_error * 0.95f + error->error_magnitude * 0.05f;

    return 0;
}

float fep_parietal_compute_free_energy(
    fep_parietal_bridge_t* bridge,
    const fep_math_belief_t* beliefs,
    const float* observations,
    uint32_t num_observations
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_compute", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, observations, sizeof(*observations));

    if (!bridge || !beliefs || !observations) return 0.0f;

    /* Phase 5: F = Inaccuracy + Complexity */
    float inaccuracy = 0.0f;
    if (beliefs->mean && beliefs->dim > 0) {
        uint32_t dim = beliefs->dim < num_observations ? beliefs->dim : num_observations;
        for (uint32_t i = 0; i < dim; i++) {
            float e = observations[i] - beliefs->mean[i];
            float prec = (beliefs->precision && i < beliefs->dim) ? beliefs->precision[i] : 1.0f;
            inaccuracy += prec * e * e;
        }
        inaccuracy *= 0.5f;
    }

    float complexity = 0.0f;
    if (beliefs->precision && beliefs->dim > 0) {
        for (uint32_t i = 0; i < beliefs->dim; i++) {
            float p = beliefs->precision[i];
            if (p > 1e-6f) {
                complexity += logf(p) - p + 1.0f;
            }
        }
        complexity = fabsf(complexity) * 0.5f;
    }

    float fe = inaccuracy + complexity;
    bridge->stats.avg_free_energy = bridge->stats.avg_free_energy * 0.95f + fe * 0.05f;

    return fe;
}

/* ============================================================================
 * ACTIVE INFERENCE API
 * ============================================================================ */

int fep_parietal_evaluate_policies(
    fep_parietal_bridge_t* bridge,
    const fep_problem_state_t* problem,
    fep_math_policy_t* policies,
    uint32_t* num_policies
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_evaluat", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, num_policies, sizeof(*num_policies));

    if (!bridge || !problem || !policies || !num_policies) return -1;

    /* Evaluate each strategy type as a policy */
    const fep_problem_strategy_t strategies[] = {
        FEP_STRATEGY_ALGEBRAIC, FEP_STRATEGY_GEOMETRIC, FEP_STRATEGY_NUMERICAL,
        FEP_STRATEGY_ANALOGICAL, FEP_STRATEGY_EXHAUSTIVE, FEP_STRATEGY_HEURISTIC,
        FEP_STRATEGY_INTUITIVE
    };
    const char* strategy_names[] = {
        "algebraic", "geometric", "numerical", "analogical",
        "exhaustive", "heuristic", "intuitive"
    };
    /* Complexity costs per strategy (higher = more expensive) */
    const float complexity_costs[] = { 0.4f, 0.5f, 0.3f, 0.6f, 0.8f, 0.2f, 0.1f };

    uint32_t n = 7;
    if (n > FEP_PARIETAL_MAX_POLICIES) n = FEP_PARIETAL_MAX_POLICIES;
    *num_policies = n;

    float dist = problem->distance_to_goal;
    int domain = (int)problem->domain;
    float total_exp = 0.0f;

    for (uint32_t p = 0; p < n; p++) {
        policies[p].strategy = strategies[p];
        policies[p].complexity_cost = complexity_costs[p];
        strncpy(policies[p].description, strategy_names[p], sizeof(policies[p].description) - 1);

        /* Pragmatic value: domain affinity + distance reduction potential */
        float pragmatic = 0.5f;
        if ((domain == FEP_MATH_DOMAIN_ALGEBRAIC && p == 0) ||
            (domain == FEP_MATH_DOMAIN_SPATIAL && p == 1) ||
            (domain == FEP_MATH_DOMAIN_NUMERICAL && p == 2) ||
            (domain == FEP_MATH_DOMAIN_PHYSICAL && (p == 1 || p == 2))) {
            pragmatic = 0.9f; /* Domain-matched strategy */
        }
        if (dist > 0.0f) pragmatic *= fminf(1.0f, dist);
        policies[p].pragmatic_value = pragmatic;

        /* Epistemic value: uncertainty reduction potential */
        float epistemic = 0.3f;
        if (domain >= 0 && domain < 7) {
            float pe = bridge->prediction_errors[domain];
            epistemic = fminf(1.0f, pe * 0.5f + 0.2f); /* Higher PE → more to learn */
        }
        policies[p].epistemic_value = epistemic;

        /* Expected free energy: G(π) = pragmatic + exploration_weight * epistemic - complexity */
        float efe = pragmatic + bridge->config.exploration_weight * epistemic
                   - complexity_costs[p];
        policies[p].expected_free_energy = efe;

        /* Softmax probability (compute exp, sum later) */
        float scaled = efe / fmaxf(0.01f, bridge->config.action_temperature);
        scaled = fminf(20.0f, fmaxf(-20.0f, scaled)); /* clamp for numerical stability */
        policies[p].probability = expf(scaled);
        total_exp += policies[p].probability;
    }

    /* Normalize probabilities */
    if (total_exp > 1e-10f) {
        for (uint32_t p = 0; p < n; p++)
            policies[p].probability /= total_exp;
    }

    bridge->stats.policies_evaluated += n;
    return 0;
}

int fep_parietal_active_inference(
    fep_parietal_bridge_t* bridge,
    const fep_problem_state_t* problem,
    fep_active_inference_result_t* result
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_active_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, problem, sizeof(*problem));

    if (!bridge || !problem || !result) return -1;
    memset(result, 0, sizeof(*result));

    if (!bridge->config.enable_active_inference) {
        result->selected_strategy = FEP_STRATEGY_INTUITIVE;
        return 0;
    }

    /* Evaluate all policies */
    fep_math_policy_t policies[FEP_PARIETAL_MAX_POLICIES];
    uint32_t n = 0;
    int rc = fep_parietal_evaluate_policies(bridge, problem, policies, &n);
    if (rc != 0 || n == 0) {
        result->selected_strategy = FEP_STRATEGY_INTUITIVE;
        return 0;
    }

    /* Select best policy by highest probability (softmax of EFE) */
    uint32_t best = 0;
    float best_prob = policies[0].probability;
    for (uint32_t p = 1; p < n; p++) {
        if (policies[p].probability > best_prob) {
            best_prob = policies[p].probability;
            best = p;
        }
    }

    result->selected_strategy = policies[best].strategy;
    result->expected_improvement = policies[best].pragmatic_value;
    result->exploration_bonus = policies[best].epistemic_value;

    /* Generate action vector: direction toward goal modulated by strategy */
    if (problem->state_vector && problem->goal_state &&
        problem->state_dim > 0 && problem->goal_dim > 0) {
        uint32_t act_dim = problem->state_dim < problem->goal_dim ?
                           problem->state_dim : problem->goal_dim;
        result->action = nimcp_calloc(act_dim, sizeof(float));
        if (result->action) {
            result->action_dim = act_dim;
            float step_scale = 0.1f; /* Base step size */
            /* Adjust step by strategy */
            if (result->selected_strategy == FEP_STRATEGY_EXHAUSTIVE) step_scale = 0.05f;
            if (result->selected_strategy == FEP_STRATEGY_INTUITIVE) step_scale = 0.3f;
            if (result->selected_strategy == FEP_STRATEGY_HEURISTIC) step_scale = 0.2f;

            for (uint32_t i = 0; i < act_dim; i++) {
                result->action[i] = step_scale * (problem->goal_state[i] - problem->state_vector[i]);
            }
        }
    }

    /* Copy evaluated policies */
    result->evaluated_policies = nimcp_calloc(n, sizeof(fep_math_policy_t));
    if (result->evaluated_policies) {
        memcpy(result->evaluated_policies, policies, n * sizeof(fep_math_policy_t));
        result->num_policies = n;
    }

    bridge->stats.active_inferences++;
    if ((int)result->selected_strategy < 7)
        bridge->stats.strategy_selections[(int)result->selected_strategy]++;

    return 0;
}

int fep_parietal_update_from_action(
    fep_parietal_bridge_t* bridge,
    const float* action,
    uint32_t action_dim,
    const float* outcome,
    uint32_t outcome_dim
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_update_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, action, sizeof(*action));

    if (!bridge || !action || !outcome) return -1;

    /* Update level-0 beliefs from the observed outcome */
    if (bridge->num_levels > 0 && bridge->level_means[0]) {
        uint32_t dim = bridge->level_dims[0];
        uint32_t upd_dim = dim < outcome_dim ? dim : outcome_dim;
        float lr = bridge->config.belief_learning_rate;

        for (uint32_t i = 0; i < upd_dim; i++) {
            float pe = outcome[i] - bridge->level_means[0][i];
            float prec = bridge->level_precisions[0] ?
                         bridge->level_precisions[0][i] : 1.0f;
            bridge->level_means[0][i] += lr * prec * pe;
        }
    }

    /* Update transition weights using action→outcome prediction error */
    if (bridge->num_levels >= 2 && bridge->transition_weights[0]) {
        uint32_t dim_lo = bridge->level_dims[0];
        uint32_t dim_hi = bridge->level_dims[1];
        uint32_t out_d = dim_lo < outcome_dim ? dim_lo : outcome_dim;
        uint32_t in_d = dim_hi < action_dim ? dim_hi : action_dim;
        float model_lr = bridge->config.precision_learning_rate * 0.1f;

        for (uint32_t i = 0; i < out_d; i++) {
            float pred = 0.0f;
            for (uint32_t j = 0; j < in_d; j++)
                pred += bridge->transition_weights[0][i * dim_hi + j] * action[j];
            float pe = outcome[i] - pred;
            /* SGD update on transition weights */
            for (uint32_t j = 0; j < in_d; j++)
                bridge->transition_weights[0][i * dim_hi + j] += model_lr * pe * action[j];
        }
    }

    /* Record PE in history for adaptive precision */
    float pe_mag = 0.0f;
    uint32_t pe_dim = action_dim < outcome_dim ? action_dim : outcome_dim;
    for (uint32_t i = 0; i < pe_dim; i++) {
        float e = outcome[i] - action[i];
        pe_mag += e * e;
    }
    pe_mag = sqrtf(pe_mag / (float)(pe_dim > 0 ? pe_dim : 1));
    bridge->pe_history[bridge->pe_history_idx % 64] = pe_mag;
    bridge->pe_history_idx++;
    if (bridge->pe_history_count < 64) bridge->pe_history_count++;

    return 0;
}

/* ============================================================================
 * PRECISION MODULATION API
 * ============================================================================ */

int fep_parietal_set_attention_precision(
    fep_parietal_bridge_t* bridge,
    const float* attention_weights,
    uint32_t dim
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_set_att", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, attention_weights, sizeof(*attention_weights));

    if (!bridge || !attention_weights || dim == 0) return -1;

    /* Store attention weights for precision modulation */
    nimcp_free(bridge->attention_precision);
    bridge->attention_precision = nimcp_calloc(dim, sizeof(float));
    if (!bridge->attention_precision) return -1;
    memcpy(bridge->attention_precision, attention_weights, dim * sizeof(float));
    bridge->attention_dim = dim;

    /* Modulate level-0 precision by attention */
    if (bridge->num_levels > 0 && bridge->level_precisions[0]) {
        uint32_t mod_dim = bridge->level_dims[0] < dim ? bridge->level_dims[0] : dim;
        for (uint32_t i = 0; i < mod_dim; i++) {
            /* Attention amplifies precision: attended features get higher precision */
            float att = fmaxf(0.0f, fminf(1.0f, attention_weights[i]));
            bridge->level_precisions[0][i] *= (0.5f + att); /* range: 0.5x to 1.5x */
            bridge->level_precisions[0][i] = fmaxf(bridge->config.min_precision,
                fminf(bridge->config.max_precision, bridge->level_precisions[0][i]));
        }
    }

    return 0;
}

int fep_parietal_adapt_precision(fep_parietal_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_adapt_p", 0.0f);

    if (!bridge) return -1;

    /* Compute mean and variance of recent prediction errors */
    if (bridge->pe_history_count == 0) return 0;

    float pe_mean = 0.0f;
    for (uint32_t i = 0; i < bridge->pe_history_count; i++)
        pe_mean += bridge->pe_history[i];
    pe_mean /= (float)bridge->pe_history_count;

    float pe_var = 0.0f;
    for (uint32_t i = 0; i < bridge->pe_history_count; i++) {
        float d = bridge->pe_history[i] - pe_mean;
        pe_var += d * d;
    }
    pe_var /= (float)bridge->pe_history_count;

    /* Adapt per-domain precision based on PE statistics */
    for (int d = 0; d < 7; d++) {
        if (bridge->domain_update_counts[d] == 0) continue;
        /* High PE variance → low precision (uncertain), low variance → high precision */
        float domain_pe = bridge->prediction_errors[d];
        float new_prec = 1.0f / (domain_pe * domain_pe + pe_var + 0.01f);
        /* EMA update */
        bridge->precision_weights[d] = bridge->precision_weights[d] * 0.9f + new_prec * 0.1f;
        bridge->precision_weights[d] = fmaxf(bridge->config.min_precision,
            fminf(bridge->config.max_precision, bridge->precision_weights[d]));
    }

    /* Adapt hierarchical level precisions */
    float lr = bridge->config.precision_learning_rate;
    for (uint32_t l = 0; l < bridge->num_levels; l++) {
        if (!bridge->level_precisions[l]) continue;
        for (uint32_t i = 0; i < bridge->level_dims[l]; i++) {
            /* Shrink precision toward inverse PE mean */
            float target = 1.0f / (pe_mean + 0.01f);
            bridge->level_precisions[l][i] += lr * (target - bridge->level_precisions[l][i]);
            bridge->level_precisions[l][i] = fmaxf(bridge->config.min_precision,
                fminf(bridge->config.max_precision, bridge->level_precisions[l][i]));
        }
    }

    /* Update avg_precision stat */
    float avg = 0.0f;
    int cnt = 0;
    for (int d = 0; d < 7; d++) {
        if (bridge->domain_update_counts[d] > 0) {
            avg += bridge->precision_weights[d];
            cnt++;
        }
    }
    if (cnt > 0) bridge->stats.avg_precision = avg / (float)cnt;

    return 0;
}

int fep_parietal_get_precision(
    const fep_parietal_bridge_t* bridge,
    uint32_t level,
    float** precision,
    uint32_t* dim
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_get_pre", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, dim, sizeof(*dim));

    if (!bridge || !precision || !dim) return -1;

    if (level >= bridge->num_levels || !bridge->level_precisions[level]) {
        *precision = NULL;
        *dim = 0;
        return -1;
    }

    /* Return a copy of the precision array for the requested level */
    uint32_t d = bridge->level_dims[level];
    float* copy = nimcp_calloc(d, sizeof(float));
    if (!copy) { *precision = NULL; *dim = 0; return -1; }
    memcpy(copy, bridge->level_precisions[level], d * sizeof(float));
    *precision = copy;
    *dim = d;
    return 0;
}

/* ============================================================================
 * HIERARCHICAL MODEL API
 * ============================================================================ */

int fep_parietal_get_generative_model(
    const fep_parietal_bridge_t* bridge,
    fep_math_generative_model_t* model
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_get_gen", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, model, sizeof(*model));

    if (!bridge || !model) return -1;
    memset(model, 0, sizeof(*model));

    /* Export hierarchical beliefs into model structure */
    /* Level 0: sensory */
    if (bridge->num_levels > 0 && bridge->level_means[0]) {
        model->sensory_beliefs.dim = bridge->level_dims[0];
        model->sensory_beliefs.mean = nimcp_calloc(bridge->level_dims[0], sizeof(float));
        model->sensory_beliefs.precision = nimcp_calloc(bridge->level_dims[0], sizeof(float));
        if (model->sensory_beliefs.mean)
            memcpy(model->sensory_beliefs.mean, bridge->level_means[0],
                   bridge->level_dims[0] * sizeof(float));
        if (model->sensory_beliefs.precision)
            memcpy(model->sensory_beliefs.precision, bridge->level_precisions[0],
                   bridge->level_dims[0] * sizeof(float));
    }
    /* Level 1: features */
    if (bridge->num_levels > 1 && bridge->level_means[1]) {
        model->feature_beliefs.dim = bridge->level_dims[1];
        model->feature_beliefs.mean = nimcp_calloc(bridge->level_dims[1], sizeof(float));
        model->feature_beliefs.precision = nimcp_calloc(bridge->level_dims[1], sizeof(float));
        if (model->feature_beliefs.mean)
            memcpy(model->feature_beliefs.mean, bridge->level_means[1],
                   bridge->level_dims[1] * sizeof(float));
        if (model->feature_beliefs.precision)
            memcpy(model->feature_beliefs.precision, bridge->level_precisions[1],
                   bridge->level_dims[1] * sizeof(float));
    }
    /* Level 2: structural */
    if (bridge->num_levels > 2 && bridge->level_means[2]) {
        model->structural_beliefs.dim = bridge->level_dims[2];
        model->structural_beliefs.mean = nimcp_calloc(bridge->level_dims[2], sizeof(float));
        model->structural_beliefs.precision = nimcp_calloc(bridge->level_dims[2], sizeof(float));
        if (model->structural_beliefs.mean)
            memcpy(model->structural_beliefs.mean, bridge->level_means[2],
                   bridge->level_dims[2] * sizeof(float));
        if (model->structural_beliefs.precision)
            memcpy(model->structural_beliefs.precision, bridge->level_precisions[2],
                   bridge->level_dims[2] * sizeof(float));
    }
    /* Level 3: abstract */
    if (bridge->num_levels > 3 && bridge->level_means[3]) {
        model->abstract_beliefs.dim = bridge->level_dims[3];
        model->abstract_beliefs.mean = nimcp_calloc(bridge->level_dims[3], sizeof(float));
        model->abstract_beliefs.precision = nimcp_calloc(bridge->level_dims[3], sizeof(float));
        if (model->abstract_beliefs.mean)
            memcpy(model->abstract_beliefs.mean, bridge->level_means[3],
                   bridge->level_dims[3] * sizeof(float));
        if (model->abstract_beliefs.precision)
            memcpy(model->abstract_beliefs.precision, bridge->level_precisions[3],
                   bridge->level_dims[3] * sizeof(float));
    }

    model->total_free_energy = bridge->stats.avg_free_energy;
    model->inaccuracy = bridge->stats.avg_prediction_error;
    model->complexity = model->total_free_energy - model->inaccuracy;

    return 0;
}

float fep_parietal_train_model(
    fep_parietal_bridge_t* bridge,
    const float** observations,
    const float** targets,
    uint32_t num_samples
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_train_m", 0.0f);

    if (!bridge || !observations || !targets || num_samples == 0) return 0.0f;
    if (bridge->num_levels < 2 || !bridge->transition_weights[0]) return 0.0f;

    uint32_t dim_lo = bridge->level_dims[0];
    uint32_t dim_hi = bridge->level_dims[1];
    float lr = bridge->config.belief_learning_rate * 0.1f;
    float total_loss = 0.0f;

    for (uint32_t s = 0; s < num_samples; s++) {
        if (!observations[s] || !targets[s]) continue;

        /* Forward pass: predicted = W * observation */
        float sample_loss = 0.0f;
        for (uint32_t i = 0; i < dim_lo; i++) {
            float pred = 0.0f;
            for (uint32_t j = 0; j < dim_hi; j++)
                pred += bridge->transition_weights[0][i * dim_hi + j] * observations[s][j];

            float pe = targets[s][i] - pred;
            sample_loss += pe * pe;

            /* SGD weight update */
            for (uint32_t j = 0; j < dim_hi; j++)
                bridge->transition_weights[0][i * dim_hi + j] += lr * pe * observations[s][j];
        }
        total_loss += sample_loss / (float)(dim_lo > 0 ? dim_lo : 1);
    }

    float avg_loss = total_loss / (float)num_samples;
    bridge->training_samples += num_samples;
    bridge->training_loss_ema = bridge->training_loss_ema * 0.9f + avg_loss * 0.1f;

    return avg_loss;
}

/* ============================================================================
 * DOMAIN-SPECIFIC API
 * ============================================================================ */

int fep_parietal_numerical_inference(
    fep_parietal_bridge_t* bridge,
    const float* quantities,
    uint32_t num_quantities,
    fep_math_belief_t* estimated
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_numeric", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, quantities, sizeof(*quantities));

    if (!bridge || !quantities || !estimated) return -1;

    /* Route through belief update with NUMERICAL domain */
    int rc = fep_parietal_update_beliefs(bridge, quantities, num_quantities,
                                          FEP_MATH_DOMAIN_NUMERICAL, estimated);
    if (rc != 0) return rc;

    /* Number sense: Weber-Fechner scaling — precision decreases with magnitude */
    if (estimated->precision && estimated->mean) {
        for (uint32_t i = 0; i < estimated->dim; i++) {
            float mag = fabsf(estimated->mean[i]);
            if (mag > 1.0f) {
                /* Weber fraction: precision ∝ 1/magnitude (logarithmic number line) */
                float weber_mod = 1.0f / logf(mag + 1.0f);
                estimated->precision[i] *= weber_mod;
                estimated->precision[i] = fmaxf(bridge->config.min_precision,
                    estimated->precision[i]);
            }
        }
    }

    estimated->domain = FEP_MATH_DOMAIN_NUMERICAL;
    return 0;
}

int fep_parietal_spatial_inference(
    fep_parietal_bridge_t* bridge,
    const float* positions,
    uint32_t num_positions,
    fep_math_belief_t* transformed
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_spatial", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, positions, sizeof(*positions));

    if (!bridge || !positions || !transformed) return -1;

    /* Route through belief update with SPATIAL domain */
    int rc = fep_parietal_update_beliefs(bridge, positions, num_positions,
                                          FEP_MATH_DOMAIN_SPATIAL, transformed);
    if (rc != 0) return rc;

    /* Spatial: pairwise distance relationships inform precision.
       Nearby points have correlated uncertainty (spatial smoothing). */
    if (transformed->mean && transformed->precision && num_positions >= 2) {
        for (uint32_t i = 0; i < transformed->dim && i + 1 < transformed->dim; i += 2) {
            /* Treat consecutive pairs as (x,y) coordinates */
            float dx = transformed->mean[i];
            float dy = transformed->mean[i + 1];
            float dist = sqrtf(dx * dx + dy * dy);
            /* Spatial precision inversely related to distance from origin */
            float spatial_prec = 1.0f / (0.1f + dist * 0.01f);
            transformed->precision[i] = fmaxf(bridge->config.min_precision,
                fminf(bridge->config.max_precision,
                       transformed->precision[i] * 0.7f + spatial_prec * 0.3f));
            transformed->precision[i + 1] = transformed->precision[i];
        }
    }

    transformed->domain = FEP_MATH_DOMAIN_SPATIAL;
    return 0;
}

int fep_parietal_physics_inference(
    fep_parietal_bridge_t* bridge,
    const float* state,
    uint32_t state_dim,
    float dt,
    fep_math_belief_t* predicted
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_physics", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    if (!bridge || !state || !predicted) return -1;

    /* First update beliefs with current state */
    int rc = fep_parietal_update_beliefs(bridge, state, state_dim,
                                          FEP_MATH_DOMAIN_PHYSICAL, predicted);
    if (rc != 0) return rc;

    /* Physics: simple dynamics model.
       Treat state as [pos0, vel0, pos1, vel1, ...] pairs.
       Predict: pos += vel * dt (Euler integration) */
    if (predicted->mean && state_dim >= 2 && dt > 0.0f) {
        for (uint32_t i = 0; i + 1 < predicted->dim; i += 2) {
            float pos = predicted->mean[i];
            float vel = predicted->mean[i + 1];
            predicted->mean[i] = pos + vel * dt;
            /* Velocity unchanged (no forces in basic model) */
            /* Precision decreases with prediction horizon (uncertainty grows) */
            if (predicted->precision) {
                float horizon_decay = 1.0f / (1.0f + dt * 0.5f);
                predicted->precision[i] *= horizon_decay;
                predicted->precision[i + 1] *= horizon_decay;
                predicted->precision[i] = fmaxf(bridge->config.min_precision, predicted->precision[i]);
                predicted->precision[i + 1] = fmaxf(bridge->config.min_precision, predicted->precision[i + 1]);
            }
        }
        /* Recompute confidence from updated precision */
        float prec_sum = 0.0f;
        for (uint32_t i = 0; i < predicted->dim; i++)
            prec_sum += predicted->precision ? predicted->precision[i] : 1.0f;
        predicted->confidence = fminf(1.0f, prec_sum / ((float)predicted->dim + 1.0f));
    }

    predicted->domain = FEP_MATH_DOMAIN_PHYSICAL;
    return 0;
}

int fep_parietal_engineering_inference(
    fep_parietal_bridge_t* bridge,
    const float* input,
    uint32_t input_dim,
    fep_math_domain_t domain,
    fep_math_belief_t* result
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_enginee", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, input, sizeof(*input));

    if (!bridge || !input || !result) return -1;

    /* Route through belief update with the specified engineering domain */
    int rc = fep_parietal_update_beliefs(bridge, input, input_dim, domain, result);
    if (rc != 0) return rc;

    /* Engineering domains get a domain-specific precision boost when enabled */
    if (result->precision) {
        float domain_boost = 1.0f;
        int d = (int)domain;
        if (d >= 0 && d < 7 && bridge->domain_update_counts[d] > 10) {
            /* Experience-based precision: more experience → higher precision */
            domain_boost = fminf(2.0f, 1.0f + logf((float)bridge->domain_update_counts[d]) * 0.1f);
        }
        for (uint32_t i = 0; i < result->dim; i++) {
            result->precision[i] *= domain_boost;
            result->precision[i] = fmaxf(bridge->config.min_precision,
                fminf(bridge->config.max_precision, result->precision[i]));
        }
    }

    result->domain = domain;
    return 0;
}

/* ============================================================================
 * SURPRISE & CURIOSITY API
 * ============================================================================ */

float fep_parietal_compute_surprise(
    fep_parietal_bridge_t* bridge,
    const float* observation,
    uint32_t dim
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_compute", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, observation, sizeof(*observation));

    if (!bridge || !observation || dim == 0) return 0.0f;

    /* Phase 5: Surprise = magnitude of observation */
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum_sq += observation[i] * observation[i];
    }
    float surprise = sqrtf(sum_sq / (float)dim);
    bridge->stats.total_surprise += surprise;

    return surprise;
}

float fep_parietal_epistemic_value(
    fep_parietal_bridge_t* bridge,
    const float* query,
    uint32_t query_dim
) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_epistem", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, query, sizeof(*query));

    if (!bridge || !query || query_dim == 0) return 0.0f;

    /* Epistemic value = expected information gain from observing query.
       Approximate as: how much current beliefs disagree with query
       weighted by precision (uncertain regions = higher epistemic value). */
    float info_gain = 0.0f;

    if (bridge->num_levels > 0 && bridge->level_means[0] && bridge->level_precisions[0]) {
        uint32_t dim = bridge->level_dims[0] < query_dim ? bridge->level_dims[0] : query_dim;
        for (uint32_t i = 0; i < dim; i++) {
            float pe = query[i] - bridge->level_means[0][i];
            float prec = bridge->level_precisions[0][i];
            /* Low precision + high PE = high epistemic value (lots to learn) */
            if (prec > 1e-6f) {
                info_gain += (pe * pe) / prec;
            } else {
                info_gain += pe * pe * 100.0f; /* Very uncertain → very informative */
            }
        }
        info_gain /= (float)(dim > 0 ? dim : 1);
        /* Transform to bounded [0, 1] range */
        info_gain = 1.0f - expf(-info_gain * 0.5f);
    } else {
        /* No model yet: everything is informative */
        float mag = 0.0f;
        for (uint32_t i = 0; i < query_dim; i++)
            mag += query[i] * query[i];
        info_gain = fminf(1.0f, sqrtf(mag / (float)query_dim));
    }

    return info_gain;
}

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int fep_parietal_set_inflammation(fep_parietal_bridge_t* bridge, float level) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_set_inf", 0.0f);


    bridge->inflammation_level = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
    return 0;
}

int fep_parietal_set_fatigue(fep_parietal_bridge_t* bridge, float level) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_set_fat", 0.0f);


    bridge->fatigue_level = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
    return 0;
}

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int fep_parietal_get_stats(
    const fep_parietal_bridge_t* bridge,
    fep_parietal_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_parietal_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_get_sta", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, stats, sizeof(*stats));

    return 0;
}

void fep_parietal_reset_stats(fep_parietal_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_reset_s", 0.0f);


    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
    }
}

const char* fep_parietal_get_last_error(void) {
    return g_error_message[0] ? g_error_message : NULL;
}

/* ============================================================================
 * INTEGRATION API
 * ============================================================================ */

int fep_parietal_attach_fep_system(
    fep_parietal_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_attach_", 0.0f);


    bridge->fep_system = fep;
    return 0;
}

void fep_parietal_free_belief(fep_math_belief_t* belief) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_free_be", 0.0f);


    if (belief) {
        nimcp_free(belief->mean);
        nimcp_free(belief->precision);
        memset(belief, 0, sizeof(*belief));
    }
}

void fep_parietal_free_prediction(fep_math_prediction_t* prediction) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_free_pr", 0.0f);


    if (prediction) {
        nimcp_free(prediction->predicted);
        nimcp_free(prediction->actual);
        nimcp_free(prediction->error);
        nimcp_free(prediction->weighted_error);
        memset(prediction, 0, sizeof(*prediction));
    }
}

void fep_parietal_free_inference_result(fep_active_inference_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_fep_parietal_free_in", 0.0f);


    if (result) {
        nimcp_free(result->action);
        nimcp_free(result->evaluated_policies);
        memset(result, 0, sizeof(*result));
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int fep_parietal_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    fep_parietal_bridge_heartbeat("fep_parietal_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "FEP_Parietal_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                fep_parietal_bridge_heartbeat("fep_parietal_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "FEP_Parietal_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "FEP_Parietal_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

//=============================================================================
// Instance Health Agent Setter (B23 Upgrade)
//=============================================================================

void fep_parietal_bridge_set_instance_health_agent(
    fep_parietal_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B23 Upgrade)
//=============================================================================

int fep_parietal_bridge_training_begin(fep_parietal_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_parietal_bridge_training_begin: NULL argument");
        return -1;
    }
    fep_parietal_bridge_heartbeat_instance(bridge->health_agent, "fep_parietal_bridge_training_begin", 0.0f);
    return 0;
}

int fep_parietal_bridge_training_end(fep_parietal_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_parietal_bridge_training_end: NULL argument");
        return -1;
    }
    fep_parietal_bridge_heartbeat_instance(bridge->health_agent, "fep_parietal_bridge_training_end", 1.0f);
    return 0;
}

int fep_parietal_bridge_training_step(fep_parietal_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_parietal_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "fep_parietal_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "fep_parietal_bridge_training_step");
    fep_parietal_bridge_heartbeat_instance(bridge->health_agent, "fep_parietal_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
