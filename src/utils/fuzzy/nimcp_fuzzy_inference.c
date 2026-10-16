//=============================================================================
// nimcp_fuzzy_inference.c - Fuzzy Inference System Implementations
//=============================================================================
/**
 * @file nimcp_fuzzy_inference.c
 * @brief Mamdani, Sugeno, Tsukamoto FIS with defuzzification and ANFIS
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "utils/fuzzy/nimcp_fuzzy_inference.h"
#include "constants/nimcp_buffer_constants.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fuzzy_inference)

//=============================================================================
// Exception Handling
//=============================================================================
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

#ifdef _MSC_VER
static __declspec(thread) char tls_fuzzy_inference_error[NIMCP_ERROR_BUFFER_SIZE] = {0};
#else
static __thread char tls_fuzzy_inference_error[NIMCP_ERROR_BUFFER_SIZE] = {0};
#endif

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(tls_fuzzy_inference_error, sizeof(tls_fuzzy_inference_error), fmt, args);
    va_end(args);
}

static inline float minf(float a, float b) { return (a < b) ? a : b; }
static inline float maxf(float a, float b) { return (a > b) ? a : b; }

//=============================================================================
// Internal Engine Structure
//=============================================================================

struct fuzzy_inference_engine {
    fuzzy_inference_config_t config;
    fuzzy_inference_stats_t stats;

    fuzzy_variable_t inputs[FUZZY_MAX_INPUTS];
    uint32_t num_inputs;

    fuzzy_variable_t outputs[FUZZY_MAX_OUTPUTS];
    uint32_t num_outputs;

    fuzzy_rule_t rules[FUZZY_MAX_RULES];
    uint32_t num_rules;

    float inflammation_level;
    float fatigue_level;
};

//=============================================================================
// Lifecycle
//=============================================================================

fuzzy_inference_config_t fuzzy_inference_default_config(void) {
    fuzzy_inference_config_t config;
    memset(&config, 0, sizeof(config));
    config.fis_type = FUZZY_FIS_MAMDANI;
    config.defuzz_method = FUZZY_DEFUZZ_CENTROID;
    config.and_method = FUZZY_TNORM_MIN;
    config.or_method = FUZZY_TCONORM_MAX;
    config.implication = FUZZY_IMPL_MAMDANI;
    config.aggregation = FUZZY_AGG_MAX;
    config.defuzz_resolution = FUZZY_RESOLUTION;
    config.enable_anfis = false;
    config.anfis_learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config.anfis_max_epochs = 100;
    config.anfis_convergence_tol = 1e-5f;
    config.inflammation_sensitivity = 0.3f;
    config.fatigue_sensitivity = 0.2f;
    return config;
}

fuzzy_inference_engine_t* fuzzy_inference_create(void) {
    return fuzzy_inference_create_custom(NULL);
}

fuzzy_inference_engine_t* fuzzy_inference_create_custom(const fuzzy_inference_config_t* config) {
    fuzzy_inference_engine_t* engine = (fuzzy_inference_engine_t*)nimcp_calloc(1,
        sizeof(fuzzy_inference_engine_t));
    if (!engine) {
        set_error("Failed to allocate fuzzy inference engine");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "fuzzy_inference_create_custom: Failed to allocate fuzzy inference engine");
        return NULL;
    }

    engine->config = config ? *config : fuzzy_inference_default_config();
    memset(&engine->stats, 0, sizeof(engine->stats));
    engine->num_inputs = 0;
    engine->num_outputs = 0;
    engine->num_rules = 0;
    engine->inflammation_level = 0.0f;
    engine->fatigue_level = 0.0f;

    fuzzy_inference_heartbeat("fuzzy_inference_create", 1.0f);
    return engine;
}

void fuzzy_inference_destroy(fuzzy_inference_engine_t* engine) {
    if (!engine) return;
    fuzzy_inference_heartbeat("fuzzy_inference_destroy", 0.0f);
    nimcp_free(engine);
}

//=============================================================================
// Variable Registration
//=============================================================================

int fuzzy_inference_add_input(fuzzy_inference_engine_t* engine,
                               const fuzzy_variable_t* var) {
    if (!engine || !var) {
        set_error("fuzzy_inference_add_input: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_inference_add_input: NULL argument");
        return FUZZY_INF_ERR_NULL;
    }
    if (engine->num_inputs >= FUZZY_MAX_INPUTS) {
        set_error("fuzzy_inference_add_input: max inputs reached (%u)", FUZZY_MAX_INPUTS);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_OUT_OF_RANGE, "fuzzy_inference_add_input: max inputs reached (%u)", FUZZY_MAX_INPUTS);
        return FUZZY_INF_ERR_MAX_INPUTS;
    }
    engine->inputs[engine->num_inputs++] = *var;
    return FUZZY_INF_ERR_OK;
}

int fuzzy_inference_add_output(fuzzy_inference_engine_t* engine,
                                const fuzzy_variable_t* var) {
    if (!engine || !var) {
        set_error("fuzzy_inference_add_output: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_inference_add_output: NULL argument");
        return FUZZY_INF_ERR_NULL;
    }
    if (engine->num_outputs >= FUZZY_MAX_OUTPUTS) {
        set_error("fuzzy_inference_add_output: max outputs reached (%u)", FUZZY_MAX_OUTPUTS);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_OUT_OF_RANGE, "fuzzy_inference_add_output: max outputs reached (%u)", FUZZY_MAX_OUTPUTS);
        return FUZZY_INF_ERR_MAX_OUTPUTS;
    }
    engine->outputs[engine->num_outputs++] = *var;
    return FUZZY_INF_ERR_OK;
}

//=============================================================================
// Rule Management
//=============================================================================

int fuzzy_inference_add_rule(fuzzy_inference_engine_t* engine,
                              const fuzzy_rule_t* rule) {
    if (!engine || !rule) {
        set_error("fuzzy_inference_add_rule: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_inference_add_rule: NULL argument");
        return FUZZY_INF_ERR_NULL;
    }
    if (engine->num_rules >= FUZZY_MAX_RULES) {
        set_error("fuzzy_inference_add_rule: max rules reached (%u)", FUZZY_MAX_RULES);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_OUT_OF_RANGE, "fuzzy_inference_add_rule: max rules reached (%u)", FUZZY_MAX_RULES);
        return FUZZY_INF_ERR_MAX_RULES;
    }
    engine->rules[engine->num_rules++] = *rule;
    return FUZZY_INF_ERR_OK;
}

int fuzzy_inference_clear_rules(fuzzy_inference_engine_t* engine) {
    if (!engine) {
        set_error("fuzzy_inference_clear_rules: NULL engine");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_inference_clear_rules: NULL engine");
        return FUZZY_INF_ERR_NULL;
    }
    engine->num_rules = 0;
    return FUZZY_INF_ERR_OK;
}

int fuzzy_inference_get_rule_count(const fuzzy_inference_engine_t* engine) {
    if (!engine) {
        set_error("fuzzy_inference_get_rule_count: NULL engine");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_inference_get_rule_count: NULL engine");
        return 0;  /* Special case: returns 0 for NULL */
    }
    return (int)engine->num_rules;
}

fuzzy_rule_t fuzzy_rule_mamdani(uint32_t in_var1, uint32_t in_term1,
                                 uint32_t in_var2, uint32_t in_term2,
                                 uint32_t out_var, uint32_t out_term,
                                 float weight) {
    fuzzy_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.antecedents[0].var_index = in_var1;
    rule.antecedents[0].term_index = in_term1;
    rule.antecedents[1].var_index = in_var2;
    rule.antecedents[1].term_index = in_term2;
    rule.num_antecedents = 2;
    rule.connector = FUZZY_TNORM_MIN;
    rule.use_or = false;
    rule.weight = weight;
    rule.mamdani.var_index = out_var;
    rule.mamdani.term_index = out_term;
    return rule;
}

fuzzy_rule_t fuzzy_rule_sugeno(uint32_t in_var1, uint32_t in_term1,
                                uint32_t in_var2, uint32_t in_term2,
                                const float* coefficients, uint32_t num_coeffs,
                                float weight) {
    fuzzy_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.antecedents[0].var_index = in_var1;
    rule.antecedents[0].term_index = in_term1;
    rule.antecedents[1].var_index = in_var2;
    rule.antecedents[1].term_index = in_term2;
    rule.num_antecedents = 2;
    rule.connector = FUZZY_TNORM_MIN;
    rule.use_or = false;
    rule.weight = weight;
    if (coefficients && num_coeffs > 0) {
        uint32_t c = (num_coeffs > FUZZY_SUGENO_MAX_COEFFS) ? FUZZY_SUGENO_MAX_COEFFS : num_coeffs;
        memcpy(rule.sugeno.coefficients, coefficients, c * sizeof(float));
        rule.sugeno.num_coeffs = c;
    }
    return rule;
}

//=============================================================================
// Rule Firing
//=============================================================================

static float compute_rule_strength(const fuzzy_inference_engine_t* engine,
                                    const fuzzy_rule_t* rule,
                                    const float* inputs) {
    if (rule->num_antecedents == 0) return 0.0f;

    float strength = 0.0f;

    for (uint32_t i = 0; i < rule->num_antecedents; i++) {
        const fuzzy_antecedent_t* ant = &rule->antecedents[i];

        if (ant->var_index >= engine->num_inputs) continue;
        const fuzzy_variable_t* var = &engine->inputs[ant->var_index];
        if (ant->term_index >= var->num_terms) continue;
        const fuzzy_set_t* term = &var->terms[ant->term_index];

        float mu = fuzzy_set_evaluate(term, inputs[ant->var_index]);
        mu = fuzzy_apply_hedge(mu, ant->hedge);
        if (ant->negated) mu = 1.0f - mu;

        if (i == 0) {
            strength = mu;
        } else {
            if (rule->use_or) {
                strength = fuzzy_tconorm(strength, mu,
                    (fuzzy_tconorm_type_t)rule->connector);
            } else {
                strength = fuzzy_tnorm(strength, mu, rule->connector);
            }
        }
    }

    return strength * rule->weight;
}

//=============================================================================
// Defuzzification
//=============================================================================

float fuzzy_defuzzify(const fuzzy_discrete_set_t* set, fuzzy_defuzz_type_t method) {
    if (!set || !set->values || set->resolution == 0) return 0.0f;

    /* Guard against division by zero when resolution == 1 */
    float dx = (set->resolution > 1)
        ? (set->x_max - set->x_min) / (float)(set->resolution - 1)
        : 0.0f;

    switch (method) {
        case FUZZY_DEFUZZ_CENTROID: {
            float num = 0.0f, den = 0.0f;
            for (uint32_t i = 0; i < set->resolution; i++) {
                float x = set->x_min + (float)i * dx;
                num += x * set->values[i];
                den += set->values[i];
            }
            return (den > FUZZY_PRECISION) ? num / den : (set->x_min + set->x_max) / 2.0f;
        }
        case FUZZY_DEFUZZ_BISECTOR: {
            float total = 0.0f;
            for (uint32_t i = 0; i < set->resolution; i++) total += set->values[i];
            float half = total / 2.0f;
            float cumulative = 0.0f;
            for (uint32_t i = 0; i < set->resolution; i++) {
                cumulative += set->values[i];
                if (cumulative >= half) return set->x_min + (float)i * dx;
            }
            return set->x_max;
        }
        case FUZZY_DEFUZZ_MOM: {
            float max_val = 0.0f;
            for (uint32_t i = 0; i < set->resolution; i++) {
                if (set->values[i] > max_val) max_val = set->values[i];
            }
            float sum = 0.0f;
            uint32_t count = 0;
            for (uint32_t i = 0; i < set->resolution; i++) {
                if (fabsf(set->values[i] - max_val) < FUZZY_PRECISION) {
                    sum += set->x_min + (float)i * dx;
                    count++;
                }
            }
            return (count > 0) ? sum / (float)count : (set->x_min + set->x_max) / 2.0f;
        }
        case FUZZY_DEFUZZ_SOM: {
            float max_val = 0.0f;
            for (uint32_t i = 0; i < set->resolution; i++) {
                if (set->values[i] > max_val) max_val = set->values[i];
            }
            for (uint32_t i = 0; i < set->resolution; i++) {
                if (fabsf(set->values[i] - max_val) < FUZZY_PRECISION) {
                    return set->x_min + (float)i * dx;
                }
            }
            return set->x_min;
        }
        case FUZZY_DEFUZZ_LOM: {
            float max_val = 0.0f;
            for (uint32_t i = 0; i < set->resolution; i++) {
                if (set->values[i] > max_val) max_val = set->values[i];
            }
            for (int i = (int)set->resolution - 1; i >= 0; i--) {
                if (fabsf(set->values[i] - max_val) < FUZZY_PRECISION) {
                    return set->x_min + (float)i * dx;
                }
            }
            return set->x_max;
        }
        case FUZZY_DEFUZZ_WEIGHTED_AVG: {
            /* Treat each local maximum as a term center */
            float num = 0.0f, den = 0.0f;
            for (uint32_t i = 0; i < set->resolution; i++) {
                float x = set->x_min + (float)i * dx;
                num += x * set->values[i];
                den += set->values[i];
            }
            return (den > FUZZY_PRECISION) ? num / den : (set->x_min + set->x_max) / 2.0f;
        }
        case FUZZY_DEFUZZ_WEIGHTED_SUM: {
            float sum = 0.0f;
            for (uint32_t i = 0; i < set->resolution; i++) {
                float x = set->x_min + (float)i * dx;
                sum += x * set->values[i];
            }
            return sum;
        }
        default:
            return (set->x_min + set->x_max) / 2.0f;
    }
}

//=============================================================================
// Mamdani Inference
//=============================================================================

static int evaluate_mamdani(fuzzy_inference_engine_t* engine,
                             const float* inputs, uint32_t num_inputs,
                             fuzzy_inference_result_t* result) {
    uint32_t resolution = engine->config.defuzz_resolution;

    for (uint32_t o = 0; o < engine->num_outputs; o++) {
        const fuzzy_variable_t* out_var = &engine->outputs[o];

        /* Create aggregated output discrete set */
        fuzzy_discrete_set_t agg_set;
        memset(&agg_set, 0, sizeof(agg_set));
        int rc = fuzzy_discrete_set_create(&agg_set, resolution,
                                            out_var->universe_min, out_var->universe_max);
        if (rc != FUZZY_ERR_OK) return FUZZY_INF_ERR_ALLOC;

        /* Guard against division by zero when resolution == 1 */
        float dx = (resolution > 1)
            ? (out_var->universe_max - out_var->universe_min) / (float)(resolution - 1)
            : 0.0f;

        /* Evaluate each rule targeting this output */
        for (uint32_t r = 0; r < engine->num_rules; r++) {
            const fuzzy_rule_t* rule = &engine->rules[r];
            if (rule->mamdani.var_index != o) continue;

            float strength = compute_rule_strength(engine, rule, inputs);
            /* P3: Bounds check before writing to rule_firing_strengths */
            if (r >= FUZZY_MAX_RULES) break;
            result->rule_firing_strengths[r] = strength;
            if (strength <= FUZZY_PRECISION) continue;

            result->num_rules_fired++;
            result->total_firing_strength += strength;

            /* Get the consequent term's MF */
            if (rule->mamdani.term_index >= out_var->num_terms) continue;
            const fuzzy_set_t* term = &out_var->terms[rule->mamdani.term_index];

            /* Implicate and aggregate */
            for (uint32_t i = 0; i < resolution; i++) {
                float x = out_var->universe_min + (float)i * dx;
                float term_mu = fuzzy_set_evaluate(term, x);
                float impl = fuzzy_implication(strength, term_mu,
                                                engine->config.implication);
                agg_set.values[i] = fuzzy_aggregate(agg_set.values[i], impl,
                                                     engine->config.aggregation);
            }
        }

        /* Defuzzify */
        result->crisp_outputs[o] = fuzzy_defuzzify(&agg_set, engine->config.defuzz_method);
        fuzzy_discrete_set_free(&agg_set);
    }

    result->num_outputs = engine->num_outputs;
    return FUZZY_INF_ERR_OK;
}

//=============================================================================
// Sugeno Inference
//=============================================================================

static int evaluate_sugeno(fuzzy_inference_engine_t* engine,
                            const float* inputs, uint32_t num_inputs,
                            fuzzy_inference_result_t* result) {
    /* Sugeno: z = Σ(w_i * f_i) / Σ(w_i) where f_i = c0 + c1*x1 + c2*x2 + ... */

    for (uint32_t o = 0; o < engine->num_outputs; o++) {
        float weighted_sum = 0.0f;
        float weight_sum = 0.0f;

        for (uint32_t r = 0; r < engine->num_rules; r++) {
            const fuzzy_rule_t* rule = &engine->rules[r];

            float strength = compute_rule_strength(engine, rule, inputs);
            /* P3: Bounds check before writing to rule_firing_strengths */
            if (r >= FUZZY_MAX_RULES) break;
            result->rule_firing_strengths[r] = strength;
            if (strength <= FUZZY_PRECISION) continue;

            result->num_rules_fired++;
            result->total_firing_strength += strength;

            /* Compute Sugeno output: f = c0 + c1*x1 + c2*x2 + ... */
            float f = 0.0f;
            if (rule->sugeno.num_coeffs > 0) {
                f = rule->sugeno.coefficients[0]; /* constant term */
                for (uint32_t j = 1; j < rule->sugeno.num_coeffs && j <= num_inputs; j++) {
                    f += rule->sugeno.coefficients[j] * inputs[j - 1];
                }
            }

            weighted_sum += strength * f;
            weight_sum += strength;
        }

        result->crisp_outputs[o] = (weight_sum > FUZZY_PRECISION) ?
                                    weighted_sum / weight_sum : 0.0f;
    }

    result->num_outputs = engine->num_outputs;
    return FUZZY_INF_ERR_OK;
}

//=============================================================================
// Tsukamoto Inference
//=============================================================================

static float inverse_monotonic_mf(const fuzzy_mf_t* mf, float mu,
                                   float x_min, float x_max) {
    /* Binary search for x such that MF(x) ≈ mu */
    float lo = x_min, hi = x_max;
    float mid;

    /* Determine if MF is increasing or decreasing */
    float val_lo = fuzzy_mf_evaluate(mf, lo);
    float val_hi = fuzzy_mf_evaluate(mf, hi);
    bool increasing = (val_hi > val_lo);

    for (int iter = 0; iter < 50; iter++) {
        mid = (lo + hi) / 2.0f;
        float val = fuzzy_mf_evaluate(mf, mid);

        if (fabsf(val - mu) < FUZZY_PRECISION) return mid;

        if (increasing) {
            if (val < mu) lo = mid;
            else hi = mid;
        } else {
            if (val > mu) lo = mid;
            else hi = mid;
        }
    }
    return (lo + hi) / 2.0f;
}

static int evaluate_tsukamoto(fuzzy_inference_engine_t* engine,
                               const float* inputs, uint32_t num_inputs,
                               fuzzy_inference_result_t* result) {
    for (uint32_t o = 0; o < engine->num_outputs; o++) {
        const fuzzy_variable_t* out_var = &engine->outputs[o];
        float weighted_sum = 0.0f;
        float weight_sum = 0.0f;

        for (uint32_t r = 0; r < engine->num_rules; r++) {
            const fuzzy_rule_t* rule = &engine->rules[r];
            if (rule->mamdani.var_index != o) continue;

            float strength = compute_rule_strength(engine, rule, inputs);
            /* P3: Bounds check before writing to rule_firing_strengths */
            if (r >= FUZZY_MAX_RULES) break;
            result->rule_firing_strengths[r] = strength;
            if (strength <= FUZZY_PRECISION) continue;

            result->num_rules_fired++;
            result->total_firing_strength += strength;

            /* Inverse: find x where MF(x) = strength */
            if (rule->mamdani.term_index >= out_var->num_terms) continue;
            const fuzzy_set_t* term = &out_var->terms[rule->mamdani.term_index];

            float z = inverse_monotonic_mf(&term->mf, strength,
                                            out_var->universe_min, out_var->universe_max);
            weighted_sum += strength * z;
            weight_sum += strength;
        }

        result->crisp_outputs[o] = (weight_sum > FUZZY_PRECISION) ?
                                    weighted_sum / weight_sum : 0.0f;
    }

    result->num_outputs = engine->num_outputs;
    return FUZZY_INF_ERR_OK;
}

//=============================================================================
// Main Inference Evaluation
//=============================================================================

int fuzzy_inference_evaluate(fuzzy_inference_engine_t* engine,
                              const float* inputs, uint32_t num_inputs,
                              fuzzy_inference_result_t* out_result) {
    if (!engine || !inputs || !out_result) {
        NIMCP_THROW_IMMUNE_RECOVER(FUZZY_INF_ERR_NULL, "fuzzy_inference_evaluate: NULL argument");
        return FUZZY_INF_ERR_NULL;
    }
    if (num_inputs != engine->num_inputs) {
        set_error("fuzzy_inference_evaluate: expected %u inputs, got %u",
                  engine->num_inputs, num_inputs);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "fuzzy_inference_evaluate: expected %u inputs, got %u",
                  engine->num_inputs, num_inputs);
        return FUZZY_INF_ERR_INPUT_MISMATCH;
    }
    if (engine->num_rules == 0) {
        set_error("fuzzy_inference_evaluate: no rules defined");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_STATE, "fuzzy_inference_evaluate: no rules defined");
        return FUZZY_INF_ERR_NO_RULES;
    }

    fuzzy_inference_heartbeat("fuzzy_inference_evaluate", 0.0f);

    memset(out_result, 0, sizeof(fuzzy_inference_result_t));

    int rc;
    switch (engine->config.fis_type) {
        case FUZZY_FIS_MAMDANI:
            rc = evaluate_mamdani(engine, inputs, num_inputs, out_result);
            break;
        case FUZZY_FIS_SUGENO:
            rc = evaluate_sugeno(engine, inputs, num_inputs, out_result);
            break;
        case FUZZY_FIS_TSUKAMOTO:
            rc = evaluate_tsukamoto(engine, inputs, num_inputs, out_result);
            break;
        default:
            rc = FUZZY_INF_ERR_INVALID_FIS;
            break;
    }

    engine->stats.inferences_run++;
    engine->stats.rules_evaluated += engine->num_rules;
    engine->stats.defuzzifications += engine->num_outputs;

    fuzzy_inference_heartbeat("fuzzy_inference_evaluate", 1.0f);
    return rc;
}

int fuzzy_inference_evaluate_batch(fuzzy_inference_engine_t* engine,
                                    const float* inputs, uint32_t num_samples,
                                    uint32_t num_inputs,
                                    fuzzy_inference_result_t* out_results) {
    if (!engine || !inputs || !out_results) {
        set_error("fuzzy_inference_evaluate_batch: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_inference_evaluate_batch: NULL argument");
        return FUZZY_INF_ERR_NULL;
    }

    fuzzy_inference_heartbeat("fuzzy_inference_batch", 0.0f);

    for (uint32_t s = 0; s < num_samples; s++) {
        int rc = fuzzy_inference_evaluate(engine,
                                           &inputs[s * num_inputs],
                                           num_inputs,
                                           &out_results[s]);
        if (rc != FUZZY_INF_ERR_OK) return rc;

        if (s % 100 == 0) {
            fuzzy_inference_heartbeat("fuzzy_inference_batch",
                                      (float)s / (float)num_samples);
        }
    }

    fuzzy_inference_heartbeat("fuzzy_inference_batch", 1.0f);
    return FUZZY_INF_ERR_OK;
}

//=============================================================================
// ANFIS Training
//=============================================================================

int fuzzy_anfis_train(fuzzy_inference_engine_t* engine,
                       const float* input_data, const float* target_data,
                       uint32_t num_samples, uint32_t num_epochs,
                       float* out_final_error) {
    if (!engine || !input_data || !target_data) {
        set_error("fuzzy_anfis_train: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_anfis_train: NULL argument");
        return FUZZY_INF_ERR_NULL;
    }
    /* P1-U1: Division by zero guard - num_samples==0 causes div-by-zero at epoch_error /= num_samples */
    if (num_samples == 0) {
        set_error("fuzzy_anfis_train: num_samples is 0");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "fuzzy_anfis_train: num_samples is 0");
        return FUZZY_INF_ERR_NULL;
    }
    if (engine->config.fis_type != FUZZY_FIS_SUGENO) {
        set_error("fuzzy_anfis_train: ANFIS requires Sugeno-type FIS");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_STATE, "fuzzy_anfis_train: ANFIS requires Sugeno-type FIS");
        return FUZZY_INF_ERR_INVALID_FIS;
    }
    if (engine->num_rules == 0) {
        set_error("fuzzy_anfis_train: no rules defined");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_STATE, "fuzzy_anfis_train: no rules defined");
        return FUZZY_INF_ERR_NO_RULES;
    }

    fuzzy_inference_heartbeat("fuzzy_anfis_train", 0.0f);

    float lr = engine->config.anfis_learning_rate;
    float last_error = FLT_MAX;

    for (uint32_t epoch = 0; epoch < num_epochs; epoch++) {
        float epoch_error = 0.0f;

        for (uint32_t s = 0; s < num_samples; s++) {
            const float* sample = &input_data[s * engine->num_inputs];
            float target = target_data[s];

            /* Forward pass */
            fuzzy_inference_result_t result;
            int rc = fuzzy_inference_evaluate(engine, sample, engine->num_inputs, &result);
            if (rc != FUZZY_INF_ERR_OK) continue;

            float output = (result.num_outputs > 0) ? result.crisp_outputs[0] : 0.0f;
            float error = target - output;
            epoch_error += error * error;

            /* Gradient descent on Sugeno consequent parameters */
            float total_strength = result.total_firing_strength;
            if (total_strength < FUZZY_PRECISION) continue;

            for (uint32_t r = 0; r < engine->num_rules; r++) {
                float w = result.rule_firing_strengths[r] / total_strength;
                if (w < FUZZY_PRECISION) continue;

                fuzzy_rule_t* rule = &engine->rules[r];

                /* Update constant term */
                if (rule->sugeno.num_coeffs > 0) {
                    rule->sugeno.coefficients[0] += lr * error * w;
                }
                /* Update input-dependent coefficients */
                for (uint32_t j = 1; j < rule->sugeno.num_coeffs &&
                     j <= engine->num_inputs; j++) {
                    rule->sugeno.coefficients[j] += lr * error * w * sample[j - 1];
                }
            }
        }

        epoch_error /= (float)num_samples;
        engine->stats.anfis_training_steps++;
        engine->stats.anfis_last_error = epoch_error;

        /* Check convergence */
        if (fabsf(last_error - epoch_error) < engine->config.anfis_convergence_tol) {
            break;
        }
        last_error = epoch_error;

        if (epoch % 10 == 0) {
            fuzzy_inference_heartbeat("fuzzy_anfis_train",
                                      (float)epoch / (float)num_epochs);
        }
    }

    if (out_final_error) *out_final_error = last_error;

    fuzzy_inference_heartbeat("fuzzy_anfis_train", 1.0f);
    return FUZZY_INF_ERR_OK;
}

//=============================================================================
// Modulation & Statistics
//=============================================================================

int fuzzy_inference_set_inflammation(fuzzy_inference_engine_t* engine, float level) {
    if (!engine) {
        set_error("fuzzy_inference_set_inflammation: NULL engine");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_inference_set_inflammation: NULL engine");
        return FUZZY_INF_ERR_NULL;
    }
    engine->inflammation_level = nimcp_clampf(level, 0.0f, 1.0f);
    return FUZZY_INF_ERR_OK;
}

int fuzzy_inference_set_fatigue(fuzzy_inference_engine_t* engine, float level) {
    if (!engine) {
        set_error("fuzzy_inference_set_fatigue: NULL engine");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_inference_set_fatigue: NULL engine");
        return FUZZY_INF_ERR_NULL;
    }
    engine->fatigue_level = nimcp_clampf(level, 0.0f, 1.0f);
    return FUZZY_INF_ERR_OK;
}

int fuzzy_inference_get_stats(const fuzzy_inference_engine_t* engine,
                               fuzzy_inference_stats_t* stats) {
    if (!engine || !stats) {
        set_error("fuzzy_inference_get_stats: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_inference_get_stats: NULL argument");
        return FUZZY_INF_ERR_NULL;
    }
    *stats = engine->stats;
    return FUZZY_INF_ERR_OK;
}

void fuzzy_inference_reset_stats(fuzzy_inference_engine_t* engine) {
    if (!engine) return;
    memset(&engine->stats, 0, sizeof(engine->stats));
}

const char* fuzzy_inference_get_last_error(void) {
    return tls_fuzzy_inference_error;
}
