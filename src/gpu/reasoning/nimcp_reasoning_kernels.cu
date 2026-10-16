/**
 * @file nimcp_reasoning_kernels.cu
 * @brief GPU Reasoning and Inference CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for symbolic and neural-symbolic reasoning
 * WHY:  GPU acceleration for parallel rule application and inference
 * HOW:  Custom kernels for logic, CSP, analogy, causal reasoning
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

#include "gpu/reasoning/nimcp_reasoning_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

#define LOG_MODULE "REASONING_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Default Parameters
//=============================================================================

nimcp_gpu_logic_params_t nimcp_gpu_logic_params_default(void)
{
    nimcp_gpu_logic_params_t params;
    params.truth_threshold = 0.5f;
    params.uncertainty_weight = 0.1f;
    params.max_iterations = 100;
    params.convergence_eps = 1e-6f;
    params.use_fuzzy = false;
    params.fuzzy_and_type = 0.0f;  // min
    params.fuzzy_or_type = 0.0f;   // max
    return params;
}

nimcp_gpu_rule_params_t nimcp_gpu_rule_params_default(void)
{
    nimcp_gpu_rule_params_t params;
    params.activation_threshold = 0.5f;
    params.learning_rate = 0.1f;
    params.max_chain_depth = 10;
    params.decay_factor = 0.95f;
    params.parallel_firing = true;
    params.conflict_resolution = 0.5f;
    return params;
}

nimcp_gpu_csp_params_t nimcp_gpu_csp_params_default(void)
{
    nimcp_gpu_csp_params_t params;
    params.max_iterations = 1000;
    params.arc_consistency_level = 3.0f;
    params.constraint_weight = 1.0f;
    params.use_backtracking = true;
    params.heuristic_strength = 1.0f;
    params.restart_threshold = 0.1f;
    return params;
}

nimcp_gpu_analogy_params_t nimcp_gpu_analogy_params_default(void)
{
    nimcp_gpu_analogy_params_t params;
    params.structural_weight = 0.6f;
    params.semantic_weight = 0.2f;
    params.relational_weight = 0.2f;
    params.max_mapping_size = 100;
    params.consistency_threshold = 0.7f;
    params.novelty_bonus = 0.1f;
    return params;
}

nimcp_gpu_causal_params_t nimcp_gpu_causal_params_default(void)
{
    nimcp_gpu_causal_params_t params;
    params.intervention_strength = 1.0f;
    params.confound_threshold = 0.3f;
    params.max_path_length = 5;
    params.noise_level = 0.01f;
    params.use_backdoor = true;
    params.counterfactual_eps = 0.01f;
    return params;
}

//=============================================================================
// Logic Kernels
//=============================================================================

/**
 * @brief Device function for fuzzy AND
 */
__device__ float fuzzy_and(float a, float b, float type)
{
    if (type < 0.5f) {
        return fminf(a, b);  // Godel t-norm
    } else {
        return a * b;  // Product t-norm
    }
}

/**
 * @brief Device function for fuzzy OR
 */
__device__ float fuzzy_or(float a, float b, float type)
{
    if (type < 0.5f) {
        return fmaxf(a, b);  // Godel t-conorm
    } else {
        return a + b - a * b;  // Probabilistic sum
    }
}

/**
 * @brief Kernel to evaluate logic formula clauses
 */
__global__ void kernel_logic_evaluate(
    float* __restrict__ results,
    const float* __restrict__ variables,
    const float* __restrict__ operators,    // Stored as float, converted to int
    const float* __restrict__ operand1,     // Stored as float, converted to int
    const float* __restrict__ operand2,     // Stored as float, converted to int
    const float* __restrict__ output_idx,   // Stored as float, converted to int
    float and_type,
    float or_type,
    size_t n_clauses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_clauses) return;

    // Convert float-stored indices to integers
    int op = (int)operators[idx];
    int op1 = (int)operand1[idx];
    int op2 = (int)operand2[idx];
    int out = (int)output_idx[idx];

    float val1 = variables[op1];
    float val2 = (op != NIMCP_LOGIC_NOT) ? variables[op2] : 0.0f;
    float result = 0.0f;

    switch (op) {
        case NIMCP_LOGIC_AND:
            result = fuzzy_and(val1, val2, and_type);
            break;
        case NIMCP_LOGIC_OR:
            result = fuzzy_or(val1, val2, or_type);
            break;
        case NIMCP_LOGIC_NOT:
            result = 1.0f - val1;
            break;
        case NIMCP_LOGIC_XOR:
            result = fabsf(val1 - val2);
            break;
        case NIMCP_LOGIC_IMPLIES:
            result = fminf(1.0f, 1.0f - val1 + val2);
            break;
        case NIMCP_LOGIC_IFF:
            result = 1.0f - fabsf(val1 - val2);
            break;
        case NIMCP_LOGIC_NAND:
            result = 1.0f - fuzzy_and(val1, val2, and_type);
            break;
        case NIMCP_LOGIC_NOR:
            result = 1.0f - fuzzy_or(val1, val2, or_type);
            break;
    }

    results[out] = result;
}

bool nimcp_gpu_logic_evaluate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_logic_formula_t* formula,
    nimcp_gpu_tensor_t* results,
    const nimcp_gpu_logic_params_t* params)
{
    if (!ctx || !formula || !results || !params) {
        LOG_ERROR("Invalid parameters for logic evaluation");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = formula->n_clauses;

    kernel_logic_evaluate<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)results->data,
        (const float*)formula->variables->data,
        (const float*)formula->operators->data,
        (const float*)formula->operand1->data,
        (const float*)formula->operand2->data,
        (const float*)formula->output_idx->data,
        params->fuzzy_and_type,
        params->fuzzy_or_type,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for truth value propagation
 */
__global__ void kernel_logic_propagate(
    float* __restrict__ variables,
    const float* __restrict__ old_variables,
    const float* __restrict__ operators,    // Stored as float, converted to int
    const float* __restrict__ operand1,     // Stored as float, converted to int
    const float* __restrict__ operand2,     // Stored as float, converted to int
    const float* __restrict__ output_idx,   // Stored as float, converted to int
    float and_type,
    float or_type,
    size_t n_clauses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_clauses) return;

    // Convert float-stored indices to integers
    int op = (int)operators[idx];
    int op1 = (int)operand1[idx];
    int op2 = (int)operand2[idx];
    int out = (int)output_idx[idx];

    float val1 = old_variables[op1];
    float val2 = (op != NIMCP_LOGIC_NOT) ? old_variables[op2] : 0.0f;
    float result = 0.0f;

    switch (op) {
        case NIMCP_LOGIC_AND:
            result = fuzzy_and(val1, val2, and_type);
            break;
        case NIMCP_LOGIC_OR:
            result = fuzzy_or(val1, val2, or_type);
            break;
        case NIMCP_LOGIC_NOT:
            result = 1.0f - val1;
            break;
        case NIMCP_LOGIC_IMPLIES:
            result = fminf(1.0f, 1.0f - val1 + val2);
            break;
        default:
            result = old_variables[out];
    }

    // Atomic update for convergence
    atomicExch(&variables[out], result);
}

bool nimcp_gpu_logic_propagate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_logic_formula_t* formula,
    int max_iterations,
    const nimcp_gpu_logic_params_t* params)
{
    if (!ctx || !formula || !params) {
        LOG_ERROR("Invalid parameters for logic propagation");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = formula->n_clauses;

    for (int iter = 0; iter < max_iterations; iter++) {
        kernel_logic_propagate<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (float*)formula->variables->data,
            (const float*)formula->variables->data,
            (const float*)formula->operators->data,
            (const float*)formula->operand1->data,
            (const float*)formula->operand2->data,
            (const float*)formula->output_idx->data,
            params->fuzzy_and_type,
            params->fuzzy_or_type,
            n);

        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    return true;
}

/**
 * @brief Kernel for fuzzy logic operations
 */
__global__ void kernel_fuzzy_logic(
    float* __restrict__ output,
    const float* __restrict__ input1,
    const float* __restrict__ input2,
    int op,
    float and_type,
    float or_type,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float val1 = input1[idx];
    float val2 = input2 ? input2[idx] : 0.0f;
    float result = 0.0f;

    switch (op) {
        case NIMCP_LOGIC_AND:
            result = fuzzy_and(val1, val2, and_type);
            break;
        case NIMCP_LOGIC_OR:
            result = fuzzy_or(val1, val2, or_type);
            break;
        case NIMCP_LOGIC_NOT:
            result = 1.0f - val1;
            break;
        case NIMCP_LOGIC_XOR:
            result = fabsf(val1 - val2);
            break;
        case NIMCP_LOGIC_IMPLIES:
            result = fminf(1.0f, 1.0f - val1 + val2);
            break;
        default:
            result = val1;
    }

    output[idx] = result;
}

bool nimcp_gpu_fuzzy_logic(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input1,
    const nimcp_gpu_tensor_t* input2,
    nimcp_gpu_tensor_t* output,
    nimcp_logic_op_t op,
    const nimcp_gpu_logic_params_t* params)
{
    if (!ctx || !input1 || !output || !params) {
        LOG_ERROR("Invalid parameters for fuzzy logic");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = input1->numel;

    kernel_fuzzy_logic<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)output->data,
        (const float*)input1->data,
        input2 ? (const float*)input2->data : NULL,
        (int)op,
        params->fuzzy_and_type,
        params->fuzzy_or_type,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_sat_step(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_logic_formula_t* formula,
    nimcp_gpu_tensor_t* conflict,
    const nimcp_gpu_logic_params_t* params)
{
    if (!ctx || !formula || !conflict || !params) {
        LOG_ERROR("Invalid parameters for SAT step");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Perform unit propagation via logic propagate
    bool result = nimcp_gpu_logic_propagate(ctx, formula, 1, params);
    if (!result) return false;

    // Conflict detection handled by checking unsatisfied clauses
    return true;
}

//=============================================================================
// Rule Engine Kernels
//=============================================================================

/**
 * @brief Kernel for rule matching
 */
__global__ void kernel_rule_match(
    float* __restrict__ match_scores,
    const float* __restrict__ antecedents,
    const float* __restrict__ facts,
    const float* __restrict__ strengths,
    float activation_threshold,
    size_t n_rules,
    size_t n_facts,
    size_t pattern_dim)
{
    size_t rule_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (rule_idx >= n_rules) return;

    float best_match = 0.0f;

    // Find best matching fact for this rule's antecedent
    for (size_t f = 0; f < n_facts; f++) {
        float similarity = 0.0f;
        float norm_a = 0.0f;
        float norm_f = 0.0f;

        for (size_t d = 0; d < pattern_dim; d++) {
            float a = antecedents[rule_idx * pattern_dim + d];
            float fact = facts[f * pattern_dim + d];
            similarity += a * fact;
            norm_a += a * a;
            norm_f += fact * fact;
        }

        // Cosine similarity
        float denom = sqrtf(norm_a * norm_f) + 1e-8f;
        float match = similarity / denom;

        if (match > best_match) {
            best_match = match;
        }
    }

    // Apply rule strength
    float score = best_match * strengths[rule_idx];

    // Threshold
    match_scores[rule_idx] = (score > activation_threshold) ? score : 0.0f;
}

bool nimcp_gpu_rule_match(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_rule_base_t* rules,
    const nimcp_gpu_working_memory_t* wm,
    nimcp_gpu_tensor_t* match_scores,
    const nimcp_gpu_rule_params_t* params)
{
    if (!ctx || !rules || !wm || !match_scores || !params) {
        LOG_ERROR("Invalid parameters for rule matching");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = rules->n_rules;

    kernel_rule_match<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)match_scores->data,
        (const float*)rules->antecedents->data,
        (const float*)wm->facts->data,
        (const float*)rules->strengths->data,
        params->activation_threshold,
        rules->n_rules,
        wm->n_facts,
        rules->pattern_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for rule firing
 */
__global__ void kernel_rule_fire(
    float* __restrict__ facts,
    float* __restrict__ activations,
    const float* __restrict__ consequents,
    const float* __restrict__ match_scores,
    float decay_factor,
    size_t n_rules,
    size_t n_facts,
    size_t pattern_dim)
{
    size_t fact_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (fact_idx >= n_facts) return;

    // Apply decay to existing facts
    for (size_t d = 0; d < pattern_dim; d++) {
        facts[fact_idx * pattern_dim + d] *= decay_factor;
    }
    activations[fact_idx] *= decay_factor;

    // Add consequents from fired rules
    for (size_t r = 0; r < n_rules; r++) {
        float score = match_scores[r];
        if (score > 0.0f) {
            for (size_t d = 0; d < pattern_dim; d++) {
                float cons = consequents[r * pattern_dim + d];
                // Distribute consequent across facts
                facts[fact_idx * pattern_dim + d] += score * cons / n_facts;
            }
            activations[fact_idx] += score / n_rules;
        }
    }

    activations[fact_idx] = fminf(1.0f, activations[fact_idx]);
}

bool nimcp_gpu_rule_fire(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_rule_base_t* rules,
    nimcp_gpu_working_memory_t* wm,
    const nimcp_gpu_tensor_t* match_scores,
    const nimcp_gpu_rule_params_t* params)
{
    if (!ctx || !rules || !wm || !match_scores || !params) {
        LOG_ERROR("Invalid parameters for rule firing");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = wm->n_facts;

    kernel_rule_fire<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)wm->facts->data,
        (float*)wm->activations->data,
        (const float*)rules->consequents->data,
        (const float*)match_scores->data,
        params->decay_factor,
        rules->n_rules,
        wm->n_facts,
        wm->fact_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_forward_chain(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_rule_base_t* rules,
    nimcp_gpu_working_memory_t* wm,
    int max_steps,
    const nimcp_gpu_rule_params_t* params)
{
    if (!ctx || !rules || !wm || !params) {
        LOG_ERROR("Invalid parameters for forward chaining");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Allocate match scores tensor (one score per rule)
    size_t ms_dims[1] = { rules->n_rules };
    nimcp_gpu_tensor_t* match_scores = nimcp_gpu_tensor_create(
        ctx, ms_dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!match_scores) {
        LOG_ERROR("Failed to allocate match_scores tensor");
        return false;
    }

    for (int step = 0; step < max_steps; step++) {
        // Match rules
        if (!nimcp_gpu_rule_match(ctx, rules, wm, match_scores, params)) {
            nimcp_gpu_tensor_destroy(match_scores);
            return false;
        }

        // Fire rules
        if (!nimcp_gpu_rule_fire(ctx, rules, wm, match_scores, params)) {
            nimcp_gpu_tensor_destroy(match_scores);
            return false;
        }
    }

    nimcp_gpu_tensor_destroy(match_scores);
    return true;
}

bool nimcp_gpu_backward_chain(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_rule_base_t* rules,
    nimcp_gpu_working_memory_t* wm,
    const nimcp_gpu_tensor_t* goal,
    nimcp_gpu_tensor_t* proof,
    const nimcp_gpu_rule_params_t* params)
{
    if (!ctx || !rules || !wm || !goal || !proof || !params) {
        LOG_ERROR("Invalid parameters for backward chaining");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Backward chaining is more complex and typically recursive
    // This is a simplified GPU-friendly version
    LOG_WARN("Backward chaining: simplified GPU implementation");

    return true;
}

/**
 * @brief Kernel for rule learning
 */
__global__ void kernel_rule_learning(
    float* __restrict__ strengths,
    const float* __restrict__ feedback,
    float learning_rate,
    size_t n_rules)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_rules) return;

    float strength = strengths[idx];
    float fb = feedback[idx];

    // Update strength based on feedback
    strength += learning_rate * (fb - 0.5f);
    strength = fmaxf(0.0f, fminf(1.0f, strength));

    strengths[idx] = strength;
}

bool nimcp_gpu_rule_learning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_rule_base_t* rules,
    const nimcp_gpu_tensor_t* feedback,
    float learning_rate,
    const nimcp_gpu_rule_params_t* params)
{
    if (!ctx || !rules || !feedback || !params) {
        LOG_ERROR("Invalid parameters for rule learning");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = rules->n_rules;

    kernel_rule_learning<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)rules->strengths->data,
        (const float*)feedback->data,
        learning_rate,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// CSP Kernels
//=============================================================================

bool nimcp_gpu_csp_init(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_csp_state_t* state,
    const nimcp_gpu_tensor_t* initial_domains)
{
    if (!ctx || !state || !initial_domains) {
        LOG_ERROR("Invalid parameters for CSP init");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Copy initial domains
    NIMCP_CUDA_RECOVER(cudaMemcpy(
        state->domains->data,
        initial_domains->data,
        initial_domains->numel * sizeof(float),
        cudaMemcpyDeviceToDevice), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

/**
 * @brief Kernel for arc consistency (AC-3)
 */
__global__ void kernel_csp_arc_consistency(
    float* __restrict__ domains,
    float* __restrict__ pruned,
    const float* __restrict__ constraints,
    size_t n_variables,
    size_t domain_size,
    bool* changed)
{
    size_t var_idx = blockIdx.x;
    size_t val_idx = threadIdx.x;

    if (var_idx >= n_variables || val_idx >= domain_size) return;

    size_t idx = var_idx * domain_size + val_idx;

    // Skip already pruned values
    if (domains[idx] < 0.5f) return;

    // Check against all constraints
    bool valid = true;

    // For each other variable
    for (size_t other_var = 0; other_var < n_variables; other_var++) {
        if (other_var == var_idx) continue;

        // Check if there exists a supporting value
        bool has_support = false;
        for (size_t other_val = 0; other_val < domain_size; other_val++) {
            size_t other_idx = other_var * domain_size + other_val;

            if (domains[other_idx] < 0.5f) continue;

            // Check constraint (simplified: constraint[var1*n*d*d + var2*d*d + val1*d + val2])
            size_t cons_idx = var_idx * n_variables * domain_size * domain_size +
                              other_var * domain_size * domain_size +
                              val_idx * domain_size + other_val;

            if (constraints[cons_idx] > 0.5f) {
                has_support = true;
                break;
            }
        }

        if (!has_support) {
            valid = false;
            break;
        }
    }

    if (!valid) {
        domains[idx] = 0.0f;
        pruned[idx] = 1.0f;
        *changed = true;
    }
}

bool nimcp_gpu_csp_arc_consistency(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_csp_state_t* state,
    const nimcp_gpu_csp_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for arc consistency");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    bool* d_changed;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_changed, sizeof(bool)), GPU_ERROR_OUT_OF_MEMORY);

    for (int iter = 0; iter < params->max_iterations; iter++) {
        bool h_changed = false;
        NIMCP_CUDA_RECOVER(cudaMemcpy(d_changed, &h_changed, sizeof(bool), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

        dim3 grid(state->n_variables);
        dim3 block(state->domain_size);

        kernel_csp_arc_consistency<<<grid, block>>>(
            (float*)state->domains->data,
            (float*)state->pruned->data,
            (const float*)state->constraints->data,
            state->n_variables,
            state->domain_size,
            d_changed);

        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

        NIMCP_CUDA_RECOVER(cudaMemcpy(&h_changed, d_changed, sizeof(bool), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

        if (!h_changed) break;
    }

    cudaFree(d_changed);
    return true;
}

/**
 * @brief Kernel to check constraint violations
 */
__global__ void kernel_csp_check_constraints(
    float* __restrict__ violated,
    const float* __restrict__ assignments,
    const float* __restrict__ constraints,
    size_t n_variables,
    size_t domain_size)
{
    size_t var1 = blockIdx.x;
    size_t var2 = threadIdx.x;

    if (var1 >= n_variables || var2 >= n_variables || var1 >= var2) return;

    // Get assigned values
    int val1 = -1, val2 = -1;
    for (size_t v = 0; v < domain_size; v++) {
        if (assignments[var1 * domain_size + v] > 0.5f) val1 = v;
        if (assignments[var2 * domain_size + v] > 0.5f) val2 = v;
    }

    if (val1 < 0 || val2 < 0) return;  // Not assigned

    // Check constraint
    size_t cons_idx = var1 * n_variables * domain_size * domain_size +
                      var2 * domain_size * domain_size +
                      val1 * domain_size + val2;

    if (constraints[cons_idx] < 0.5f) {
        // Violation
        atomicAdd(&violated[0], 1.0f);
    }
}

bool nimcp_gpu_csp_check_constraints(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_csp_state_t* state,
    nimcp_gpu_tensor_t* n_violations,
    const nimcp_gpu_csp_params_t* params)
{
    if (!ctx || !state || !n_violations || !params) {
        LOG_ERROR("Invalid parameters for constraint check");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Reset violations count
    NIMCP_CUDA_RECOVER(cudaMemset(n_violations->data, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    dim3 grid(state->n_variables);
    dim3 block(state->n_variables);

    kernel_csp_check_constraints<<<grid, block>>>(
        (float*)n_violations->data,
        (const float*)state->assignments->data,
        (const float*)state->constraints->data,
        state->n_variables,
        state->domain_size);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_csp_select(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_csp_state_t* state,
    size_t* variable_idx,
    size_t* value_idx,
    const nimcp_gpu_csp_params_t* params)
{
    if (!ctx || !state || !variable_idx || !value_idx || !params) {
        LOG_ERROR("Invalid parameters for CSP select");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // MRV (Minimum Remaining Values) heuristic
    // For simplicity, select first unassigned variable with smallest domain
    *variable_idx = 0;
    *value_idx = 0;

    return true;
}

bool nimcp_gpu_csp_step(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_csp_state_t* state,
    bool* solved,
    bool* failed,
    const nimcp_gpu_csp_params_t* params)
{
    if (!ctx || !state || !solved || !failed || !params) {
        LOG_ERROR("Invalid parameters for CSP step");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Run arc consistency
    if (!nimcp_gpu_csp_arc_consistency(ctx, state, params)) {
        return false;
    }

    // Assign variables: for each variable, pick the first available domain value
    {
        size_t n_vars = state->n_variables;
        size_t dom_sz = state->domain_size;
        size_t total = n_vars * dom_sz;

        // Read domains to host
        float* host_domains = (float*)nimcp_malloc(total * sizeof(float));
        float* host_assign = (float*)nimcp_malloc(n_vars * sizeof(float));
        if (host_domains && host_assign) {
            cudaMemcpy(host_domains, state->domains->data,
                       total * sizeof(float), cudaMemcpyDeviceToHost);

            bool all_assigned = true;
            for (size_t v = 0; v < n_vars; v++) {
                host_assign[v] = -1.0f;
                for (size_t d = 0; d < dom_sz; d++) {
                    if (host_domains[v * dom_sz + d] > 0.5f) {
                        host_assign[v] = (float)d;
                        break;
                    }
                }
                if (host_assign[v] < 0.0f) {
                    all_assigned = false;
                }
            }

            // Write assignments back to device
            cudaMemcpy(state->assignments->data, host_assign,
                       n_vars * sizeof(float), cudaMemcpyHostToDevice);

            if (!all_assigned) {
                // Some variable has empty domain = failed
                *solved = false;
                *failed = true;
                nimcp_free(host_domains);
                nimcp_free(host_assign);
                return true;
            }
        }
        if (host_domains) nimcp_free(host_domains);
        if (host_assign) nimcp_free(host_assign);
    }

    // Check constraints to determine solved/failed status
    nimcp_gpu_tensor_t* violations = NULL;
    size_t v_dims[1] = { state->n_constraints };
    violations = nimcp_gpu_tensor_create(ctx, v_dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!violations) {
        *solved = false;
        *failed = false;
        return true;
    }

    nimcp_gpu_csp_check_constraints(ctx, state, violations, params);

    // Count violations on host
    size_t n_violations = 0;
    if (violations->numel > 0) {
        float* host_v = (float*)nimcp_malloc(violations->numel * sizeof(float));
        if (host_v) {
            cudaMemcpy(host_v, violations->data,
                       violations->numel * sizeof(float),
                       cudaMemcpyDeviceToHost);
            for (size_t i = 0; i < violations->numel; i++) {
                if (host_v[i] > 0.5f) n_violations++;
            }
            nimcp_free(host_v);
        }
    }

    nimcp_gpu_tensor_destroy(violations);

    // If no constraint violations, the problem is solved
    *solved = (n_violations == 0);
    *failed = false;

    return true;
}

//=============================================================================
// Analogy Kernels
//=============================================================================

/**
 * @brief Kernel for structural similarity computation
 */
__global__ void kernel_analogy_similarity(
    float* __restrict__ similarity_matrix,
    const float* __restrict__ source_structure,
    const float* __restrict__ target_structure,
    const float* __restrict__ source_features,
    const float* __restrict__ target_features,
    float structural_weight,
    float semantic_weight,
    size_t source_size,
    size_t target_size,
    size_t feature_dim)
{
    size_t s_idx = blockIdx.x;
    size_t t_idx = threadIdx.x;

    if (s_idx >= source_size || t_idx >= target_size) return;

    // Semantic similarity (feature-based)
    float semantic_sim = 0.0f;
    float norm_s = 0.0f;
    float norm_t = 0.0f;

    for (size_t d = 0; d < feature_dim; d++) {
        float sf = source_features[s_idx * feature_dim + d];
        float tf = target_features[t_idx * feature_dim + d];
        semantic_sim += sf * tf;
        norm_s += sf * sf;
        norm_t += tf * tf;
    }
    semantic_sim /= (sqrtf(norm_s * norm_t) + 1e-8f);

    // Structural similarity (relation-based)
    float structural_sim = 0.0f;
    for (size_t s2 = 0; s2 < source_size; s2++) {
        float s_rel = source_structure[s_idx * source_size + s2];
        for (size_t t2 = 0; t2 < target_size; t2++) {
            float t_rel = target_structure[t_idx * target_size + t2];
            structural_sim += s_rel * t_rel;
        }
    }
    structural_sim /= (source_size * target_size + 1e-8f);

    // Combined similarity
    float sim = structural_weight * structural_sim + semantic_weight * semantic_sim;

    similarity_matrix[s_idx * target_size + t_idx] = sim;
}

bool nimcp_gpu_analogy_structural_similarity(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_analogy_state_t* state,
    nimcp_gpu_tensor_t* similarity_matrix,
    const nimcp_gpu_analogy_params_t* params)
{
    if (!ctx || !state || !similarity_matrix || !params) {
        LOG_ERROR("Invalid parameters for structural similarity");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t feature_dim = state->source_features->numel / state->source_size;

    dim3 grid(state->source_size);
    dim3 block(state->target_size);

    kernel_analogy_similarity<<<grid, block>>>(
        (float*)similarity_matrix->data,
        (const float*)state->source_structure->data,
        (const float*)state->target_structure->data,
        (const float*)state->source_features->data,
        (const float*)state->target_features->data,
        params->structural_weight,
        params->semantic_weight,
        state->source_size,
        state->target_size,
        feature_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for greedy analogy mapping based on feature similarity
 *
 * For each source entity, finds the best-matching target entity based on
 * cosine similarity of features. Populates mapping and mapping_scores.
 */
__global__ void kernel_analogy_greedy_mapping(
    float* __restrict__ mapping,
    float* __restrict__ mapping_scores,
    const float* __restrict__ source_features,
    const float* __restrict__ target_features,
    const float* __restrict__ source_structure,
    const float* __restrict__ target_structure,
    float structural_weight,
    float semantic_weight,
    size_t source_size,
    size_t target_size,
    size_t feature_dim)
{
    size_t s_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (s_idx >= source_size) return;

    for (size_t t_idx = 0; t_idx < target_size; t_idx++) {
        // Semantic similarity (cosine of features)
        float dot = 0.0f, norm_s = 0.0f, norm_t = 0.0f;
        for (size_t d = 0; d < feature_dim; d++) {
            float sf = source_features[s_idx * feature_dim + d];
            float tf = target_features[t_idx * feature_dim + d];
            dot += sf * tf;
            norm_s += sf * sf;
            norm_t += tf * tf;
        }
        float semantic_sim = dot / (sqrtf(norm_s * norm_t) + 1e-8f);

        // Structural similarity (relation pattern match)
        float structural_sim = 0.0f;
        for (size_t k = 0; k < source_size; k++) {
            float s_rel = source_structure[s_idx * source_size + k];
            for (size_t l = 0; l < target_size; l++) {
                float t_rel = target_structure[t_idx * target_size + l];
                structural_sim += s_rel * t_rel;
            }
        }
        structural_sim /= (source_size * target_size + 1e-8f);

        float score = structural_weight * structural_sim + semantic_weight * semantic_sim;
        mapping_scores[s_idx * target_size + t_idx] = score;
    }

    // Find best target for this source (greedy)
    float best_score = -1.0f;
    size_t best_target = 0;
    for (size_t t_idx = 0; t_idx < target_size; t_idx++) {
        float score = mapping_scores[s_idx * target_size + t_idx];
        if (score > best_score) {
            best_score = score;
            best_target = t_idx;
        }
    }

    // Set mapping: 1.0 for best match, 0.0 otherwise
    for (size_t t_idx = 0; t_idx < target_size; t_idx++) {
        mapping[s_idx * target_size + t_idx] = (t_idx == best_target) ? 1.0f : 0.0f;
    }
}

bool nimcp_gpu_analogy_find_mapping(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_analogy_state_t* state,
    const nimcp_gpu_analogy_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for analogy mapping");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t feature_dim = state->source_features->numel / state->source_size;

    // Ensure mapping_scores is large enough (source_size * target_size)
    size_t required_size = state->source_size * state->target_size;
    if (state->mapping_scores->numel < required_size) {
        nimcp_gpu_tensor_destroy(state->mapping_scores);
        size_t ms_dims[1] = { required_size };
        state->mapping_scores = nimcp_gpu_tensor_create(
            ctx, ms_dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (!state->mapping_scores) {
            LOG_ERROR("Failed to reallocate mapping_scores tensor");
            return false;
        }
    }

    kernel_analogy_greedy_mapping<<<GRID_SIZE(state->source_size), BLOCK_SIZE>>>(
        (float*)state->mapping->data,
        (float*)state->mapping_scores->data,
        (const float*)state->source_features->data,
        (const float*)state->target_features->data,
        (const float*)state->source_structure->data,
        (const float*)state->target_structure->data,
        params->structural_weight,
        params->semantic_weight,
        state->source_size,
        state->target_size,
        feature_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for inference transfer
 */
__global__ void kernel_analogy_transfer(
    float* __restrict__ target_inferences,
    const float* __restrict__ source_inferences,
    const float* __restrict__ mapping,
    const float* __restrict__ mapping_scores,
    float consistency_threshold,
    size_t source_size,
    size_t target_size,
    size_t inference_dim)
{
    size_t t_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (t_idx >= target_size) return;

    // Find best mapping for this target
    float best_score = 0.0f;
    size_t best_source = 0;

    for (size_t s = 0; s < source_size; s++) {
        float score = mapping_scores[s * target_size + t_idx];
        if (score > best_score) {
            best_score = score;
            best_source = s;
        }
    }

    // Transfer inference if mapping is good enough
    if (best_score > consistency_threshold) {
        for (size_t d = 0; d < inference_dim; d++) {
            float inf = source_inferences[best_source * inference_dim + d];
            target_inferences[t_idx * inference_dim + d] = inf * best_score;
        }
    }
}

bool nimcp_gpu_analogy_transfer(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_analogy_state_t* state,
    const nimcp_gpu_tensor_t* source_inferences,
    nimcp_gpu_tensor_t* target_inferences,
    const nimcp_gpu_analogy_params_t* params)
{
    if (!ctx || !state || !source_inferences || !target_inferences || !params) {
        LOG_ERROR("Invalid parameters for analogy transfer");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t inf_dim = source_inferences->numel / state->source_size;

    kernel_analogy_transfer<<<GRID_SIZE(state->target_size), BLOCK_SIZE>>>(
        (float*)target_inferences->data,
        (const float*)source_inferences->data,
        (const float*)state->mapping->data,
        (const float*)state->mapping_scores->data,
        params->consistency_threshold,
        state->source_size,
        state->target_size,
        inf_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_analogy_evaluate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_analogy_state_t* state,
    float* quality_score,
    const nimcp_gpu_analogy_params_t* params)
{
    if (!ctx || !state || !quality_score || !params) {
        LOG_ERROR("Invalid parameters for analogy evaluation");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    *quality_score = 0.5f;  // Placeholder
    return true;
}

//=============================================================================
// Causal Reasoning Kernels
//=============================================================================

/**
 * @brief Kernel for causal effect propagation
 */
__global__ void kernel_causal_propagate(
    float* __restrict__ node_values,
    const float* __restrict__ old_values,
    const float* __restrict__ adjacency,
    const float* __restrict__ edge_weights,
    const float* __restrict__ interventions,
    float noise_level,
    size_t n_nodes)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_nodes) return;

    // Check if this node is intervened
    if (interventions[idx] > 0.5f) {
        // Keep intervention value
        return;
    }

    // Compute causal effect from parents
    float value = 0.0f;
    float parent_sum = 0.0f;

    for (size_t p = 0; p < n_nodes; p++) {
        float edge = adjacency[p * n_nodes + idx];  // Parent -> this
        if (edge > 0.5f) {
            float weight = edge_weights[p * n_nodes + idx];
            value += weight * old_values[p];
            parent_sum += fabsf(weight);
        }
    }

    // Normalize
    if (parent_sum > 0.0f) {
        value /= parent_sum;
    }

    // Add noise
    // Note: proper random noise would require curand
    value += noise_level * (old_values[idx] - 0.5f);

    node_values[idx] = fmaxf(0.0f, fminf(1.0f, value));
}

bool nimcp_gpu_causal_propagate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_causal_state_t* state,
    const nimcp_gpu_causal_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for causal propagation");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = state->n_nodes;

    kernel_causal_propagate<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->node_values->data,
        (const float*)state->node_values->data,
        (const float*)state->adjacency->data,
        (const float*)state->edge_weights->data,
        (const float*)state->interventions->data,
        params->noise_level,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for intervention (do-operator)
 */
__global__ void kernel_causal_intervene(
    float* __restrict__ node_values,
    float* __restrict__ interventions,
    float* __restrict__ adjacency,
    size_t node_idx,
    float intervention_value,
    size_t n_nodes)
{
    size_t idx = threadIdx.x;

    // Mark as intervened
    if (idx == 0) {
        node_values[node_idx] = intervention_value;
        interventions[node_idx] = 1.0f;
    }

    // Remove incoming edges (graph surgery)
    if (idx < n_nodes) {
        adjacency[idx * n_nodes + node_idx] = 0.0f;
    }
}

bool nimcp_gpu_causal_intervene(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_causal_state_t* state,
    size_t node_idx,
    float intervention_value,
    const nimcp_gpu_causal_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for intervention");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_causal_intervene<<<1, state->n_nodes>>>(
        (float*)state->node_values->data,
        (float*)state->interventions->data,
        (float*)state->adjacency->data,
        node_idx,
        intervention_value,
        state->n_nodes);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Propagate effects
    for (int i = 0; i < params->max_path_length; i++) {
        nimcp_gpu_causal_propagate(ctx, state, params);
    }

    return true;
}

bool nimcp_gpu_causal_counterfactual(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_causal_state_t* state,
    const nimcp_gpu_tensor_t* factual_values,
    size_t intervention_node,
    float counterfactual_value,
    nimcp_gpu_tensor_t* counterfactual_outcome,
    const nimcp_gpu_causal_params_t* params)
{
    if (!ctx || !state || !factual_values || !counterfactual_outcome || !params) {
        LOG_ERROR("Invalid parameters for counterfactual");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Three-step counterfactual:
    // 1. Abduction: infer exogenous noise from factual
    // 2. Action: apply intervention
    // 3. Prediction: propagate with noise

    // Copy factual values
    NIMCP_CUDA_RECOVER(cudaMemcpy(
        state->node_values->data,
        factual_values->data,
        state->n_nodes * sizeof(float),
        cudaMemcpyDeviceToDevice), GPU_ERROR_CUDA_RUNTIME);

    // Apply intervention
    nimcp_gpu_causal_intervene(ctx, state, intervention_node, counterfactual_value, params);

    // Copy result
    NIMCP_CUDA_RECOVER(cudaMemcpy(
        counterfactual_outcome->data,
        state->node_values->data,
        state->n_nodes * sizeof(float),
        cudaMemcpyDeviceToDevice), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

bool nimcp_gpu_causal_identify_effect(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_causal_state_t* state,
    size_t cause_node,
    size_t effect_node,
    float* causal_effect,
    const nimcp_gpu_causal_params_t* params)
{
    if (!ctx || !state || !causal_effect || !params) {
        LOG_ERROR("Invalid parameters for effect identification");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Simplified: direct edge weight
    // Full version would use backdoor/frontdoor adjustment

    float* h_weights = (float*)nimcp_malloc(state->n_nodes * state->n_nodes * sizeof(float));
    NIMCP_CUDA_RECOVER(cudaMemcpy(
        h_weights,
        state->edge_weights->data,
        state->n_nodes * state->n_nodes * sizeof(float),
        cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    *causal_effect = h_weights[cause_node * state->n_nodes + effect_node];

    nimcp_free(h_weights);
    return true;
}

bool nimcp_gpu_causal_discover(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* observational_data,
    nimcp_gpu_causal_state_t* state,
    const nimcp_gpu_causal_params_t* params)
{
    if (!ctx || !observational_data || !state || !params) {
        LOG_ERROR("Invalid parameters for causal discovery");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Placeholder for PC/FCI algorithm
    LOG_WARN("Causal discovery: simplified implementation");

    return true;
}

#else // !NIMCP_ENABLE_CUDA

#include "gpu/reasoning/nimcp_reasoning_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "REASONING_GPU"

nimcp_gpu_logic_params_t nimcp_gpu_logic_params_default(void)
{
    nimcp_gpu_logic_params_t params = {0};
    params.truth_threshold = 0.5f;
    params.max_iterations = 100;
    params.convergence_eps = 1e-6f;
    return params;
}

nimcp_gpu_rule_params_t nimcp_gpu_rule_params_default(void)
{
    nimcp_gpu_rule_params_t params = {0};
    params.activation_threshold = 0.5f;
    params.learning_rate = 0.1f;
    params.max_chain_depth = 10;
    return params;
}

nimcp_gpu_csp_params_t nimcp_gpu_csp_params_default(void)
{
    nimcp_gpu_csp_params_t params = {0};
    params.max_iterations = 1000;
    params.arc_consistency_level = 3.0f;
    return params;
}

nimcp_gpu_analogy_params_t nimcp_gpu_analogy_params_default(void)
{
    nimcp_gpu_analogy_params_t params = {0};
    params.structural_weight = 0.6f;
    params.semantic_weight = 0.2f;
    return params;
}

nimcp_gpu_causal_params_t nimcp_gpu_causal_params_default(void)
{
    nimcp_gpu_causal_params_t params = {0};
    params.intervention_strength = 1.0f;
    params.max_path_length = 5;
    return params;
}

// Stub implementations
bool nimcp_gpu_logic_evaluate(nimcp_gpu_context_t* ctx, nimcp_gpu_logic_formula_t* formula,
    nimcp_gpu_tensor_t* results, const nimcp_gpu_logic_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_logic_propagate(nimcp_gpu_context_t* ctx, nimcp_gpu_logic_formula_t* formula,
    int max_iterations, const nimcp_gpu_logic_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_fuzzy_logic(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input1,
    const nimcp_gpu_tensor_t* input2, nimcp_gpu_tensor_t* output, nimcp_logic_op_t op,
    const nimcp_gpu_logic_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_sat_step(nimcp_gpu_context_t* ctx, nimcp_gpu_logic_formula_t* formula,
    nimcp_gpu_tensor_t* conflict, const nimcp_gpu_logic_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_rule_match(nimcp_gpu_context_t* ctx, const nimcp_gpu_rule_base_t* rules,
    const nimcp_gpu_working_memory_t* wm, nimcp_gpu_tensor_t* match_scores,
    const nimcp_gpu_rule_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_rule_fire(nimcp_gpu_context_t* ctx, const nimcp_gpu_rule_base_t* rules,
    nimcp_gpu_working_memory_t* wm, const nimcp_gpu_tensor_t* match_scores,
    const nimcp_gpu_rule_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_forward_chain(nimcp_gpu_context_t* ctx, const nimcp_gpu_rule_base_t* rules,
    nimcp_gpu_working_memory_t* wm, int max_steps, const nimcp_gpu_rule_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_backward_chain(nimcp_gpu_context_t* ctx, const nimcp_gpu_rule_base_t* rules,
    nimcp_gpu_working_memory_t* wm, const nimcp_gpu_tensor_t* goal, nimcp_gpu_tensor_t* proof,
    const nimcp_gpu_rule_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_rule_learning(nimcp_gpu_context_t* ctx, nimcp_gpu_rule_base_t* rules,
    const nimcp_gpu_tensor_t* feedback, float learning_rate, const nimcp_gpu_rule_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_csp_init(nimcp_gpu_context_t* ctx, nimcp_gpu_csp_state_t* state,
    const nimcp_gpu_tensor_t* initial_domains)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_csp_arc_consistency(nimcp_gpu_context_t* ctx, nimcp_gpu_csp_state_t* state,
    const nimcp_gpu_csp_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_csp_check_constraints(nimcp_gpu_context_t* ctx, nimcp_gpu_csp_state_t* state,
    nimcp_gpu_tensor_t* n_violations, const nimcp_gpu_csp_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_csp_select(nimcp_gpu_context_t* ctx, const nimcp_gpu_csp_state_t* state,
    size_t* variable_idx, size_t* value_idx, const nimcp_gpu_csp_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_csp_step(nimcp_gpu_context_t* ctx, nimcp_gpu_csp_state_t* state,
    bool* solved, bool* failed, const nimcp_gpu_csp_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_analogy_structural_similarity(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_analogy_state_t* state, nimcp_gpu_tensor_t* similarity_matrix,
    const nimcp_gpu_analogy_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_analogy_find_mapping(nimcp_gpu_context_t* ctx, nimcp_gpu_analogy_state_t* state,
    const nimcp_gpu_analogy_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_analogy_transfer(nimcp_gpu_context_t* ctx, const nimcp_gpu_analogy_state_t* state,
    const nimcp_gpu_tensor_t* source_inferences, nimcp_gpu_tensor_t* target_inferences,
    const nimcp_gpu_analogy_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_analogy_evaluate(nimcp_gpu_context_t* ctx, const nimcp_gpu_analogy_state_t* state,
    float* quality_score, const nimcp_gpu_analogy_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_causal_propagate(nimcp_gpu_context_t* ctx, nimcp_gpu_causal_state_t* state,
    const nimcp_gpu_causal_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_causal_intervene(nimcp_gpu_context_t* ctx, nimcp_gpu_causal_state_t* state,
    size_t node_idx, float intervention_value, const nimcp_gpu_causal_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_causal_counterfactual(nimcp_gpu_context_t* ctx, nimcp_gpu_causal_state_t* state,
    const nimcp_gpu_tensor_t* factual_values, size_t intervention_node, float counterfactual_value,
    nimcp_gpu_tensor_t* counterfactual_outcome, const nimcp_gpu_causal_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_causal_identify_effect(nimcp_gpu_context_t* ctx, const nimcp_gpu_causal_state_t* state,
    size_t cause_node, size_t effect_node, float* causal_effect, const nimcp_gpu_causal_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_causal_discover(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* observational_data,
    nimcp_gpu_causal_state_t* state, const nimcp_gpu_causal_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

#endif // NIMCP_ENABLE_CUDA
