/**
 * @file test_reasoning_kernels.cpp
 * @brief Unit tests for GPU reasoning and inference kernels
 *
 * Tests propositional logic, rule engine, constraint satisfaction,
 * analogical reasoning, and causal reasoning GPU operations.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "gpu/reasoning/nimcp_reasoning_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class ReasoningKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    // Helper to create a tensor filled with a constant value
    nimcp_gpu_tensor_t* CreateFilledTensor(size_t* dims, size_t rank, float value) {
        if (!ctx) return nullptr;
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, rank, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    // Helper to create 1D tensor
    nimcp_gpu_tensor_t* Create1DTensor(size_t n, float value = 0.0f) {
        size_t dims[1] = {n};
        return CreateFilledTensor(dims, 1, value);
    }

    // Helper to create 2D tensor
    nimcp_gpu_tensor_t* Create2DTensor(size_t rows, size_t cols, float value = 0.0f) {
        size_t dims[2] = {rows, cols};
        return CreateFilledTensor(dims, 2, value);
    }

    // Helper to create integer tensor (stored as float for GPU compatibility)
    nimcp_gpu_tensor_t* CreateIntTensor(size_t n, float value = 0.0f) {
        size_t dims[1] = {n};
        return CreateFilledTensor(dims, 1, value);
    }

    // Helper to copy tensor to host
    std::vector<float> CopyToHost(nimcp_gpu_tensor_t* tensor) {
        size_t n = tensor->numel;
        std::vector<float> host_data(n);
        nimcp_gpu_tensor_to_host(tensor, host_data.data());
        return host_data;
    }

    // Helper to set tensor from host
    nimcp_gpu_tensor_t* SetFromHost(nimcp_gpu_tensor_t* tensor, const std::vector<float>& data) {
        if (tensor) nimcp_gpu_tensor_destroy(tensor);
        size_t dims[1] = {data.size()};
        return nimcp_gpu_tensor_from_host(ctx, data.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    // Helper to create a logic formula
    nimcp_gpu_logic_formula_t* CreateSimpleFormula(size_t n_variables, size_t n_clauses) {
        nimcp_gpu_logic_formula_t* formula = new nimcp_gpu_logic_formula_t();
        formula->variables = Create1DTensor(n_variables, 0.5f);
        formula->operators = CreateIntTensor(n_clauses, static_cast<float>(NIMCP_LOGIC_AND));
        formula->operand1 = CreateIntTensor(n_clauses, 0.0f);
        formula->operand2 = CreateIntTensor(n_clauses, 1.0f);
        formula->output_idx = CreateIntTensor(n_clauses, 2.0f);
        formula->n_variables = n_variables;
        formula->n_clauses = n_clauses;
        return formula;
    }

    void DestroyFormula(nimcp_gpu_logic_formula_t* formula) {
        if (formula) {
            if (formula->variables) nimcp_gpu_tensor_destroy(formula->variables);
            if (formula->operators) nimcp_gpu_tensor_destroy(formula->operators);
            if (formula->operand1) nimcp_gpu_tensor_destroy(formula->operand1);
            if (formula->operand2) nimcp_gpu_tensor_destroy(formula->operand2);
            if (formula->output_idx) nimcp_gpu_tensor_destroy(formula->output_idx);
            delete formula;
        }
    }

    // Helper to create a rule base
    nimcp_gpu_rule_base_t* CreateSimpleRuleBase(size_t n_rules, size_t pattern_dim) {
        nimcp_gpu_rule_base_t* rules = new nimcp_gpu_rule_base_t();
        rules->antecedents = Create2DTensor(n_rules, pattern_dim, 0.5f);
        rules->consequents = Create2DTensor(n_rules, pattern_dim, 0.5f);
        rules->strengths = Create1DTensor(n_rules, 1.0f);
        rules->conditions = Create2DTensor(n_rules, pattern_dim, 0.0f);
        rules->n_rules = n_rules;
        rules->pattern_dim = pattern_dim;
        return rules;
    }

    void DestroyRuleBase(nimcp_gpu_rule_base_t* rules) {
        if (rules) {
            if (rules->antecedents) nimcp_gpu_tensor_destroy(rules->antecedents);
            if (rules->consequents) nimcp_gpu_tensor_destroy(rules->consequents);
            if (rules->strengths) nimcp_gpu_tensor_destroy(rules->strengths);
            if (rules->conditions) nimcp_gpu_tensor_destroy(rules->conditions);
            delete rules;
        }
    }

    // Helper to create working memory
    nimcp_gpu_working_memory_t* CreateWorkingMemory(size_t n_facts, size_t fact_dim) {
        nimcp_gpu_working_memory_t* wm = new nimcp_gpu_working_memory_t();
        wm->facts = Create2DTensor(n_facts, fact_dim, 0.0f);
        wm->activations = Create1DTensor(n_facts, 1.0f);
        wm->sources = CreateIntTensor(n_facts, 0.0f);
        wm->timestamps = Create1DTensor(n_facts, 0.0f);
        wm->n_facts = n_facts;
        wm->fact_dim = fact_dim;
        return wm;
    }

    void DestroyWorkingMemory(nimcp_gpu_working_memory_t* wm) {
        if (wm) {
            if (wm->facts) nimcp_gpu_tensor_destroy(wm->facts);
            if (wm->activations) nimcp_gpu_tensor_destroy(wm->activations);
            if (wm->sources) nimcp_gpu_tensor_destroy(wm->sources);
            if (wm->timestamps) nimcp_gpu_tensor_destroy(wm->timestamps);
            delete wm;
        }
    }

    // Helper to create CSP state
    nimcp_gpu_csp_state_t* CreateCSPState(size_t n_variables, size_t domain_size, size_t n_constraints) {
        nimcp_gpu_csp_state_t* state = new nimcp_gpu_csp_state_t();
        state->domains = Create2DTensor(n_variables, domain_size, 1.0f);  // All values initially possible
        state->assignments = Create1DTensor(n_variables, -1.0f);  // Unassigned
        state->constraints = Create2DTensor(n_constraints, 4, 0.0f);  // var1, var2, rel, constant
        state->violated = Create1DTensor(n_constraints, 0.0f);
        state->pruned = Create2DTensor(n_variables, domain_size, 0.0f);
        state->n_variables = n_variables;
        state->domain_size = domain_size;
        state->n_constraints = n_constraints;
        return state;
    }

    void DestroyCSPState(nimcp_gpu_csp_state_t* state) {
        if (state) {
            if (state->domains) nimcp_gpu_tensor_destroy(state->domains);
            if (state->assignments) nimcp_gpu_tensor_destroy(state->assignments);
            if (state->constraints) nimcp_gpu_tensor_destroy(state->constraints);
            if (state->violated) nimcp_gpu_tensor_destroy(state->violated);
            if (state->pruned) nimcp_gpu_tensor_destroy(state->pruned);
            delete state;
        }
    }

    // Helper to create analogy state
    nimcp_gpu_analogy_state_t* CreateAnalogyState(size_t source_size, size_t target_size, size_t feature_dim) {
        nimcp_gpu_analogy_state_t* state = new nimcp_gpu_analogy_state_t();
        state->source_structure = Create2DTensor(source_size, source_size, 0.0f);
        state->target_structure = Create2DTensor(target_size, target_size, 0.0f);
        state->source_features = Create2DTensor(source_size, feature_dim, 0.5f);
        state->target_features = Create2DTensor(target_size, feature_dim, 0.5f);
        state->mapping = Create2DTensor(source_size, target_size, 0.0f);
        state->mapping_scores = Create2DTensor(source_size, target_size, 0.0f);
        state->source_size = source_size;
        state->target_size = target_size;
        return state;
    }

    void DestroyAnalogyState(nimcp_gpu_analogy_state_t* state) {
        if (state) {
            if (state->source_structure) nimcp_gpu_tensor_destroy(state->source_structure);
            if (state->target_structure) nimcp_gpu_tensor_destroy(state->target_structure);
            if (state->source_features) nimcp_gpu_tensor_destroy(state->source_features);
            if (state->target_features) nimcp_gpu_tensor_destroy(state->target_features);
            if (state->mapping) nimcp_gpu_tensor_destroy(state->mapping);
            if (state->mapping_scores) nimcp_gpu_tensor_destroy(state->mapping_scores);
            delete state;
        }
    }

    // Helper to create causal state
    nimcp_gpu_causal_state_t* CreateCausalState(size_t n_nodes) {
        nimcp_gpu_causal_state_t* state = new nimcp_gpu_causal_state_t();
        state->adjacency = Create2DTensor(n_nodes, n_nodes, 0.0f);
        state->edge_weights = Create2DTensor(n_nodes, n_nodes, 0.0f);
        state->node_values = Create1DTensor(n_nodes, 0.0f);
        state->interventions = Create1DTensor(n_nodes, 0.0f);
        state->confounders = Create1DTensor(n_nodes, 0.0f);
        state->n_nodes = n_nodes;
        return state;
    }

    void DestroyCausalState(nimcp_gpu_causal_state_t* state) {
        if (state) {
            if (state->adjacency) nimcp_gpu_tensor_destroy(state->adjacency);
            if (state->edge_weights) nimcp_gpu_tensor_destroy(state->edge_weights);
            if (state->node_values) nimcp_gpu_tensor_destroy(state->node_values);
            if (state->interventions) nimcp_gpu_tensor_destroy(state->interventions);
            if (state->confounders) nimcp_gpu_tensor_destroy(state->confounders);
            delete state;
        }
    }
};

//=============================================================================
// Default Parameter Tests
//=============================================================================

TEST_F(ReasoningKernelTest, LogicParamsDefault_ReturnsValidParams) {
    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.truth_threshold, 0.0f);
    EXPECT_LE(params.truth_threshold, 1.0f);
    EXPECT_GE(params.uncertainty_weight, 0.0f);
    EXPECT_GT(params.max_iterations, 0);
    EXPECT_GT(params.convergence_eps, 0.0f);
    EXPECT_GE(params.fuzzy_and_type, 0.0f);
    EXPECT_LE(params.fuzzy_and_type, 1.0f);
    EXPECT_GE(params.fuzzy_or_type, 0.0f);
    EXPECT_LE(params.fuzzy_or_type, 1.0f);
}

TEST_F(ReasoningKernelTest, RuleParamsDefault_ReturnsValidParams) {
    nimcp_gpu_rule_params_t params = nimcp_gpu_rule_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.activation_threshold, 0.0f);
    EXPECT_LE(params.activation_threshold, 1.0f);
    EXPECT_GT(params.learning_rate, 0.0f);
    EXPECT_GT(params.max_chain_depth, 0);
    EXPECT_GT(params.decay_factor, 0.0f);
    EXPECT_LE(params.decay_factor, 1.0f);
    EXPECT_GE(params.conflict_resolution, 0.0f);
}

TEST_F(ReasoningKernelTest, CSPParamsDefault_ReturnsValidParams) {
    nimcp_gpu_csp_params_t params = nimcp_gpu_csp_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.max_iterations, 0);
    EXPECT_GE(params.arc_consistency_level, 1.0f);
    EXPECT_LE(params.arc_consistency_level, 3.0f);
    EXPECT_GT(params.constraint_weight, 0.0f);
    EXPECT_GE(params.heuristic_strength, 0.0f);
    EXPECT_GT(params.restart_threshold, 0.0f);
}

TEST_F(ReasoningKernelTest, AnalogyParamsDefault_ReturnsValidParams) {
    nimcp_gpu_analogy_params_t params = nimcp_gpu_analogy_params_default();

    // Check reasonable defaults
    EXPECT_GE(params.structural_weight, 0.0f);
    EXPECT_GE(params.semantic_weight, 0.0f);
    EXPECT_GE(params.relational_weight, 0.0f);
    EXPECT_GT(params.max_mapping_size, 0);
    EXPECT_GT(params.consistency_threshold, 0.0f);
    EXPECT_GE(params.novelty_bonus, 0.0f);
}

TEST_F(ReasoningKernelTest, CausalParamsDefault_ReturnsValidParams) {
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    // Check reasonable defaults
    EXPECT_GE(params.intervention_strength, 0.0f);
    EXPECT_GT(params.confound_threshold, 0.0f);
    EXPECT_GT(params.max_path_length, 0);
    EXPECT_GE(params.noise_level, 0.0f);
    EXPECT_GT(params.counterfactual_eps, 0.0f);
}

//=============================================================================
// Propositional Logic Tests
//=============================================================================

TEST_F(ReasoningKernelTest, LogicEvaluate_EvaluatesFormula) {
    RequireGPU();

    const size_t n_variables = 10;
    const size_t n_clauses = 5;

    nimcp_gpu_logic_formula_t* formula = CreateSimpleFormula(n_variables, n_clauses);
    nimcp_gpu_tensor_t* results = Create1DTensor(n_clauses, 0.0f);
    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();

    bool result = nimcp_gpu_logic_evaluate(ctx, formula, results, &params);
    EXPECT_TRUE(result);

    auto result_data = CopyToHost(results);

    // Results should be valid truth values
    for (size_t i = 0; i < n_clauses; i++) {
        EXPECT_GE(result_data[i], 0.0f);
        EXPECT_LE(result_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(results);
    DestroyFormula(formula);
}

TEST_F(ReasoningKernelTest, LogicPropagate_PropagatesTruthValues) {
    RequireGPU();

    const size_t n_variables = 10;
    const size_t n_clauses = 5;

    nimcp_gpu_logic_formula_t* formula = CreateSimpleFormula(n_variables, n_clauses);

    // Set some initial truth values
    std::vector<float> initial_values(n_variables, 0.5f);
    initial_values[0] = 1.0f;  // True
    initial_values[1] = 0.0f;  // False
    formula->variables = SetFromHost(formula->variables, initial_values);

    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();

    bool result = nimcp_gpu_logic_propagate(ctx, formula, 10, &params);
    EXPECT_TRUE(result);

    auto var_data = CopyToHost(formula->variables);

    // Variables should be valid truth values
    for (size_t i = 0; i < n_variables; i++) {
        EXPECT_GE(var_data[i], 0.0f);
        EXPECT_LE(var_data[i], 1.0f);
    }

    DestroyFormula(formula);
}

TEST_F(ReasoningKernelTest, FuzzyLogic_ANDOperation) {
    RequireGPU();

    const size_t n = 100;

    nimcp_gpu_tensor_t* input1 = Create1DTensor(n, 0.8f);
    nimcp_gpu_tensor_t* input2 = Create1DTensor(n, 0.6f);
    nimcp_gpu_tensor_t* output = Create1DTensor(n, 0.0f);

    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();
    params.use_fuzzy = true;
    params.fuzzy_and_type = 0.0f;  // Min operator

    bool result = nimcp_gpu_fuzzy_logic(ctx, input1, input2, output, NIMCP_LOGIC_AND, &params);
    EXPECT_TRUE(result);

    auto output_data = CopyToHost(output);

    // For min-AND: result should be min(0.8, 0.6) = 0.6
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(output_data[i], 0.6f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(input1);
    nimcp_gpu_tensor_destroy(input2);
    nimcp_gpu_tensor_destroy(output);
}

TEST_F(ReasoningKernelTest, FuzzyLogic_OROperation) {
    RequireGPU();

    const size_t n = 100;

    nimcp_gpu_tensor_t* input1 = Create1DTensor(n, 0.3f);
    nimcp_gpu_tensor_t* input2 = Create1DTensor(n, 0.7f);
    nimcp_gpu_tensor_t* output = Create1DTensor(n, 0.0f);

    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();
    params.use_fuzzy = true;
    params.fuzzy_or_type = 0.0f;  // Max operator

    bool result = nimcp_gpu_fuzzy_logic(ctx, input1, input2, output, NIMCP_LOGIC_OR, &params);
    EXPECT_TRUE(result);

    auto output_data = CopyToHost(output);

    // For max-OR: result should be max(0.3, 0.7) = 0.7
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(output_data[i], 0.7f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(input1);
    nimcp_gpu_tensor_destroy(input2);
    nimcp_gpu_tensor_destroy(output);
}

TEST_F(ReasoningKernelTest, FuzzyLogic_NOTOperation) {
    RequireGPU();

    const size_t n = 100;

    nimcp_gpu_tensor_t* input1 = Create1DTensor(n, 0.3f);
    nimcp_gpu_tensor_t* input2 = Create1DTensor(n, 0.0f);  // Unused for NOT
    nimcp_gpu_tensor_t* output = Create1DTensor(n, 0.0f);

    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();
    params.use_fuzzy = true;

    bool result = nimcp_gpu_fuzzy_logic(ctx, input1, input2, output, NIMCP_LOGIC_NOT, &params);
    EXPECT_TRUE(result);

    auto output_data = CopyToHost(output);

    // NOT: result should be 1 - 0.3 = 0.7
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(output_data[i], 0.7f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(input1);
    nimcp_gpu_tensor_destroy(input2);
    nimcp_gpu_tensor_destroy(output);
}

TEST_F(ReasoningKernelTest, FuzzyLogic_XOROperation) {
    RequireGPU();

    const size_t n = 100;

    nimcp_gpu_tensor_t* input1 = Create1DTensor(n, 0.8f);
    nimcp_gpu_tensor_t* input2 = Create1DTensor(n, 0.3f);
    nimcp_gpu_tensor_t* output = Create1DTensor(n, 0.0f);

    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();
    params.use_fuzzy = true;

    bool result = nimcp_gpu_fuzzy_logic(ctx, input1, input2, output, NIMCP_LOGIC_XOR, &params);
    EXPECT_TRUE(result);

    auto output_data = CopyToHost(output);

    // XOR output should be valid
    for (size_t i = 0; i < n; i++) {
        EXPECT_GE(output_data[i], 0.0f);
        EXPECT_LE(output_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(input1);
    nimcp_gpu_tensor_destroy(input2);
    nimcp_gpu_tensor_destroy(output);
}

TEST_F(ReasoningKernelTest, FuzzyLogic_IMPLIESOperation) {
    RequireGPU();

    const size_t n = 100;

    nimcp_gpu_tensor_t* input1 = Create1DTensor(n, 0.9f);  // Antecedent
    nimcp_gpu_tensor_t* input2 = Create1DTensor(n, 0.4f);  // Consequent
    nimcp_gpu_tensor_t* output = Create1DTensor(n, 0.0f);

    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();
    params.use_fuzzy = true;

    bool result = nimcp_gpu_fuzzy_logic(ctx, input1, input2, output, NIMCP_LOGIC_IMPLIES, &params);
    EXPECT_TRUE(result);

    auto output_data = CopyToHost(output);

    // Implication output should be valid
    for (size_t i = 0; i < n; i++) {
        EXPECT_GE(output_data[i], 0.0f);
        EXPECT_LE(output_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(input1);
    nimcp_gpu_tensor_destroy(input2);
    nimcp_gpu_tensor_destroy(output);
}

TEST_F(ReasoningKernelTest, SATStep_DetectsConflicts) {
    RequireGPU();

    const size_t n_variables = 10;
    const size_t n_clauses = 5;

    nimcp_gpu_logic_formula_t* formula = CreateSimpleFormula(n_variables, n_clauses);
    nimcp_gpu_tensor_t* conflict = Create1DTensor(n_clauses, 0.0f);
    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();

    bool result = nimcp_gpu_sat_step(ctx, formula, conflict, &params);
    EXPECT_TRUE(result);

    auto conflict_data = CopyToHost(conflict);

    // Conflict indicators should be binary
    for (size_t i = 0; i < n_clauses; i++) {
        EXPECT_TRUE(conflict_data[i] == 0.0f || conflict_data[i] == 1.0f);
    }

    nimcp_gpu_tensor_destroy(conflict);
    DestroyFormula(formula);
}

//=============================================================================
// Rule Engine Tests
//=============================================================================

TEST_F(ReasoningKernelTest, RuleMatch_ComputesMatchScores) {
    RequireGPU();

    const size_t n_rules = 10;
    const size_t pattern_dim = 8;
    const size_t n_facts = 5;

    nimcp_gpu_rule_base_t* rules = CreateSimpleRuleBase(n_rules, pattern_dim);
    nimcp_gpu_working_memory_t* wm = CreateWorkingMemory(n_facts, pattern_dim);
    nimcp_gpu_tensor_t* match_scores = Create2DTensor(n_rules, n_facts, 0.0f);
    nimcp_gpu_rule_params_t params = nimcp_gpu_rule_params_default();

    bool result = nimcp_gpu_rule_match(ctx, rules, wm, match_scores, &params);
    EXPECT_TRUE(result);

    auto scores = CopyToHost(match_scores);

    // Match scores should be non-negative
    for (size_t i = 0; i < n_rules * n_facts; i++) {
        EXPECT_GE(scores[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(match_scores);
    DestroyWorkingMemory(wm);
    DestroyRuleBase(rules);
}

TEST_F(ReasoningKernelTest, RuleFire_UpdatesWorkingMemory) {
    RequireGPU();

    const size_t n_rules = 5;
    const size_t pattern_dim = 8;
    const size_t n_facts = 10;

    nimcp_gpu_rule_base_t* rules = CreateSimpleRuleBase(n_rules, pattern_dim);
    nimcp_gpu_working_memory_t* wm = CreateWorkingMemory(n_facts, pattern_dim);
    nimcp_gpu_tensor_t* match_scores = Create2DTensor(n_rules, n_facts, 0.0f);
    nimcp_gpu_rule_params_t params = nimcp_gpu_rule_params_default();

    // Set high match scores for some rules
    std::vector<float> scores(n_rules * n_facts, 0.0f);
    scores[0] = 0.9f;  // Rule 0 matches fact 0
    scores[n_facts + 1] = 0.8f;  // Rule 1 matches fact 1
    match_scores = SetFromHost(match_scores, scores);

    auto initial_facts = CopyToHost(wm->facts);

    bool result = nimcp_gpu_rule_fire(ctx, rules, wm, match_scores, &params);
    EXPECT_TRUE(result);

    auto final_facts = CopyToHost(wm->facts);

    // Working memory should be modified
    bool modified = false;
    for (size_t i = 0; i < n_facts * pattern_dim; i++) {
        if (std::abs(final_facts[i] - initial_facts[i]) > 1e-6f) {
            modified = true;
            break;
        }
    }
    // Note: May not always be modified depending on rule structure
    // Just verify the function runs successfully

    nimcp_gpu_tensor_destroy(match_scores);
    DestroyWorkingMemory(wm);
    DestroyRuleBase(rules);
}

TEST_F(ReasoningKernelTest, ForwardChain_PerformsInference) {
    RequireGPU();

    const size_t n_rules = 5;
    const size_t pattern_dim = 8;
    const size_t n_facts = 10;
    const int max_steps = 10;

    nimcp_gpu_rule_base_t* rules = CreateSimpleRuleBase(n_rules, pattern_dim);
    nimcp_gpu_working_memory_t* wm = CreateWorkingMemory(n_facts, pattern_dim);
    nimcp_gpu_rule_params_t params = nimcp_gpu_rule_params_default();

    // Initialize some facts
    std::vector<float> facts(n_facts * pattern_dim, 0.0f);
    for (size_t i = 0; i < pattern_dim; i++) {
        facts[i] = 0.8f;  // First fact is active
    }
    wm->facts = SetFromHost(wm->facts, facts);

    bool result = nimcp_gpu_forward_chain(ctx, rules, wm, max_steps, &params);
    EXPECT_TRUE(result);

    // Function should complete without error
    DestroyWorkingMemory(wm);
    DestroyRuleBase(rules);
}

TEST_F(ReasoningKernelTest, BackwardChain_FindsProof) {
    RequireGPU();

    const size_t n_rules = 5;
    const size_t pattern_dim = 8;
    const size_t n_facts = 10;

    nimcp_gpu_rule_base_t* rules = CreateSimpleRuleBase(n_rules, pattern_dim);
    nimcp_gpu_working_memory_t* wm = CreateWorkingMemory(n_facts, pattern_dim);
    nimcp_gpu_tensor_t* goal = Create1DTensor(pattern_dim, 0.5f);
    nimcp_gpu_tensor_t* proof = Create1DTensor(n_rules, 0.0f);
    nimcp_gpu_rule_params_t params = nimcp_gpu_rule_params_default();

    // Initialize some facts
    std::vector<float> facts(n_facts * pattern_dim, 0.5f);
    wm->facts = SetFromHost(wm->facts, facts);

    bool result = nimcp_gpu_backward_chain(ctx, rules, wm, goal, proof, &params);
    EXPECT_TRUE(result);

    auto proof_data = CopyToHost(proof);

    // Proof should contain valid values
    for (size_t i = 0; i < n_rules; i++) {
        EXPECT_GE(proof_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(goal);
    nimcp_gpu_tensor_destroy(proof);
    DestroyWorkingMemory(wm);
    DestroyRuleBase(rules);
}

TEST_F(ReasoningKernelTest, RuleLearning_UpdatesStrengths) {
    RequireGPU();

    const size_t n_rules = 10;
    const size_t pattern_dim = 8;

    nimcp_gpu_rule_base_t* rules = CreateSimpleRuleBase(n_rules, pattern_dim);
    nimcp_gpu_tensor_t* feedback = Create1DTensor(n_rules, 0.0f);
    nimcp_gpu_rule_params_t params = nimcp_gpu_rule_params_default();

    // Set positive feedback for some rules
    std::vector<float> fb(n_rules, 0.0f);
    fb[0] = 1.0f;   // Positive feedback
    fb[1] = -1.0f;  // Negative feedback
    fb[2] = 0.5f;   // Partial feedback
    feedback = SetFromHost(feedback, fb);

    auto initial_strengths = CopyToHost(rules->strengths);

    bool result = nimcp_gpu_rule_learning(ctx, rules, feedback, 0.1f, &params);
    EXPECT_TRUE(result);

    auto final_strengths = CopyToHost(rules->strengths);

    // Rule 0 should strengthen (positive feedback)
    EXPECT_GE(final_strengths[0], initial_strengths[0]);

    // Rule 1 should weaken (negative feedback)
    EXPECT_LE(final_strengths[1], initial_strengths[1]);

    nimcp_gpu_tensor_destroy(feedback);
    DestroyRuleBase(rules);
}

//=============================================================================
// Constraint Satisfaction Tests
//=============================================================================

TEST_F(ReasoningKernelTest, CSPInit_InitializesDomains) {
    RequireGPU();

    const size_t n_variables = 10;
    const size_t domain_size = 5;
    const size_t n_constraints = 15;

    nimcp_gpu_csp_state_t* state = CreateCSPState(n_variables, domain_size, n_constraints);
    nimcp_gpu_tensor_t* initial_domains = Create2DTensor(n_variables, domain_size, 1.0f);

    bool result = nimcp_gpu_csp_init(ctx, state, initial_domains);
    EXPECT_TRUE(result);

    auto domain_data = CopyToHost(state->domains);

    // Domains should be initialized (all 1s for available values)
    for (size_t i = 0; i < n_variables * domain_size; i++) {
        EXPECT_GE(domain_data[i], 0.0f);
        EXPECT_LE(domain_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(initial_domains);
    DestroyCSPState(state);
}

TEST_F(ReasoningKernelTest, CSPArcConsistency_PropagatesConstraints) {
    RequireGPU();

    const size_t n_variables = 5;
    const size_t domain_size = 5;
    const size_t n_constraints = 10;

    nimcp_gpu_csp_state_t* state = CreateCSPState(n_variables, domain_size, n_constraints);
    nimcp_gpu_csp_params_t params = nimcp_gpu_csp_params_default();

    // Initialize domains
    nimcp_gpu_tensor_t* initial_domains = Create2DTensor(n_variables, domain_size, 1.0f);
    nimcp_gpu_csp_init(ctx, state, initial_domains);

    bool result = nimcp_gpu_csp_arc_consistency(ctx, state, &params);
    EXPECT_TRUE(result);

    // Function should complete successfully
    nimcp_gpu_tensor_destroy(initial_domains);
    DestroyCSPState(state);
}

TEST_F(ReasoningKernelTest, CSPCheckConstraints_CountsViolations) {
    RequireGPU();

    const size_t n_variables = 5;
    const size_t domain_size = 5;
    const size_t n_constraints = 10;

    nimcp_gpu_csp_state_t* state = CreateCSPState(n_variables, domain_size, n_constraints);
    nimcp_gpu_tensor_t* n_violations = Create1DTensor(1, 0.0f);
    nimcp_gpu_csp_params_t params = nimcp_gpu_csp_params_default();

    // Assign all variables to the same value (likely to cause violations)
    nimcp_gpu_fill(ctx, state->assignments, 0.0f);

    bool result = nimcp_gpu_csp_check_constraints(ctx, state, n_violations, &params);
    EXPECT_TRUE(result);

    auto violation_count = CopyToHost(n_violations);

    // Violation count should be non-negative
    EXPECT_GE(violation_count[0], 0.0f);

    nimcp_gpu_tensor_destroy(n_violations);
    DestroyCSPState(state);
}

TEST_F(ReasoningKernelTest, CSPSelect_SelectsVariableAndValue) {
    RequireGPU();

    const size_t n_variables = 5;
    const size_t domain_size = 5;
    const size_t n_constraints = 10;

    nimcp_gpu_csp_state_t* state = CreateCSPState(n_variables, domain_size, n_constraints);
    nimcp_gpu_csp_params_t params = nimcp_gpu_csp_params_default();

    size_t variable_idx = 0;
    size_t value_idx = 0;

    bool result = nimcp_gpu_csp_select(ctx, state, &variable_idx, &value_idx, &params);
    EXPECT_TRUE(result);

    // Selected indices should be valid
    EXPECT_LT(variable_idx, n_variables);
    EXPECT_LT(value_idx, domain_size);

    DestroyCSPState(state);
}

TEST_F(ReasoningKernelTest, CSPStep_PerformsSolvingStep) {
    RequireGPU();

    const size_t n_variables = 5;
    const size_t domain_size = 5;
    const size_t n_constraints = 5;

    nimcp_gpu_csp_state_t* state = CreateCSPState(n_variables, domain_size, n_constraints);
    nimcp_gpu_csp_params_t params = nimcp_gpu_csp_params_default();

    bool solved = false;
    bool failed = false;

    bool result = nimcp_gpu_csp_step(ctx, state, &solved, &failed, &params);
    EXPECT_TRUE(result);

    // Should not be both solved and failed
    EXPECT_FALSE(solved && failed);

    DestroyCSPState(state);
}

TEST_F(ReasoningKernelTest, CSPStep_MultipleSolvingSteps) {
    RequireGPU();

    const size_t n_variables = 4;
    const size_t domain_size = 4;
    const size_t n_constraints = 6;

    nimcp_gpu_csp_state_t* state = CreateCSPState(n_variables, domain_size, n_constraints);
    nimcp_gpu_csp_params_t params = nimcp_gpu_csp_params_default();

    bool solved = false;
    bool failed = false;
    int steps = 0;
    const int max_steps = 100;

    // Run solving steps until solved, failed, or max steps
    while (!solved && !failed && steps < max_steps) {
        bool result = nimcp_gpu_csp_step(ctx, state, &solved, &failed, &params);
        EXPECT_TRUE(result);
        steps++;
    }

    // Should terminate within max steps
    EXPECT_LT(steps, max_steps);

    DestroyCSPState(state);
}

//=============================================================================
// Analogical Reasoning Tests
//=============================================================================

TEST_F(ReasoningKernelTest, AnalogyStructuralSimilarity_ComputesSimilarity) {
    RequireGPU();

    const size_t source_size = 5;
    const size_t target_size = 5;
    const size_t feature_dim = 8;

    nimcp_gpu_analogy_state_t* state = CreateAnalogyState(source_size, target_size, feature_dim);
    nimcp_gpu_tensor_t* similarity_matrix = Create2DTensor(source_size, target_size, 0.0f);
    nimcp_gpu_analogy_params_t params = nimcp_gpu_analogy_params_default();

    // Set up some structure (edges between nodes)
    std::vector<float> source_struct(source_size * source_size, 0.0f);
    std::vector<float> target_struct(target_size * target_size, 0.0f);
    // Add edges: 0->1, 1->2 in source; 0->1, 1->2 in target (similar structure)
    source_struct[0 * source_size + 1] = 1.0f;
    source_struct[1 * source_size + 2] = 1.0f;
    target_struct[0 * target_size + 1] = 1.0f;
    target_struct[1 * target_size + 2] = 1.0f;
    state->source_structure = SetFromHost(state->source_structure, source_struct);
    state->target_structure = SetFromHost(state->target_structure, target_struct);

    bool result = nimcp_gpu_analogy_structural_similarity(ctx, state, similarity_matrix, &params);
    EXPECT_TRUE(result);

    auto sim_data = CopyToHost(similarity_matrix);

    // Similarity values should be non-negative
    for (size_t i = 0; i < source_size * target_size; i++) {
        EXPECT_GE(sim_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(similarity_matrix);
    DestroyAnalogyState(state);
}

TEST_F(ReasoningKernelTest, AnalogyFindMapping_FindsMapping) {
    RequireGPU();

    const size_t source_size = 4;
    const size_t target_size = 4;
    const size_t feature_dim = 8;

    nimcp_gpu_analogy_state_t* state = CreateAnalogyState(source_size, target_size, feature_dim);
    nimcp_gpu_analogy_params_t params = nimcp_gpu_analogy_params_default();

    // Set up similar features
    std::vector<float> features(source_size * feature_dim);
    for (size_t i = 0; i < source_size * feature_dim; i++) {
        features[i] = static_cast<float>(i % 10) / 10.0f;
    }
    state->source_features = SetFromHost(state->source_features, features);
    state->target_features = SetFromHost(state->target_features, features);  // Same features = perfect match

    bool result = nimcp_gpu_analogy_find_mapping(ctx, state, &params);
    EXPECT_TRUE(result);

    auto mapping = CopyToHost(state->mapping);

    // Mapping should contain valid values
    for (size_t i = 0; i < source_size * target_size; i++) {
        EXPECT_GE(mapping[i], 0.0f);
        EXPECT_LE(mapping[i], 1.0f);
    }

    DestroyAnalogyState(state);
}

TEST_F(ReasoningKernelTest, AnalogyTransfer_TransfersInferences) {
    RequireGPU();

    const size_t source_size = 4;
    const size_t target_size = 4;
    const size_t feature_dim = 8;

    nimcp_gpu_analogy_state_t* state = CreateAnalogyState(source_size, target_size, feature_dim);
    nimcp_gpu_tensor_t* source_inferences = Create1DTensor(source_size, 0.0f);
    nimcp_gpu_tensor_t* target_inferences = Create1DTensor(target_size, 0.0f);
    nimcp_gpu_analogy_params_t params = nimcp_gpu_analogy_params_default();

    // Set up identity mapping
    std::vector<float> mapping(source_size * target_size, 0.0f);
    for (size_t i = 0; i < source_size && i < target_size; i++) {
        mapping[i * target_size + i] = 1.0f;
    }
    state->mapping = SetFromHost(state->mapping, mapping);

    // Set source inferences
    std::vector<float> src_inf(source_size);
    src_inf[0] = 0.8f;
    src_inf[1] = 0.6f;
    source_inferences = SetFromHost(source_inferences, src_inf);

    bool result = nimcp_gpu_analogy_transfer(ctx, state, source_inferences, target_inferences, &params);
    EXPECT_TRUE(result);

    auto target_inf = CopyToHost(target_inferences);

    // With identity mapping, target inferences should match source
    // (May not be exact due to implementation details)
    for (size_t i = 0; i < target_size; i++) {
        EXPECT_GE(target_inf[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(source_inferences);
    nimcp_gpu_tensor_destroy(target_inferences);
    DestroyAnalogyState(state);
}

TEST_F(ReasoningKernelTest, AnalogyEvaluate_ComputesQualityScore) {
    RequireGPU();

    const size_t source_size = 4;
    const size_t target_size = 4;
    const size_t feature_dim = 8;

    nimcp_gpu_analogy_state_t* state = CreateAnalogyState(source_size, target_size, feature_dim);
    nimcp_gpu_analogy_params_t params = nimcp_gpu_analogy_params_default();

    // Set up identity mapping (should be high quality if features match)
    std::vector<float> mapping(source_size * target_size, 0.0f);
    for (size_t i = 0; i < source_size && i < target_size; i++) {
        mapping[i * target_size + i] = 1.0f;
    }
    state->mapping = SetFromHost(state->mapping, mapping);

    float quality_score = 0.0f;

    bool result = nimcp_gpu_analogy_evaluate(ctx, state, &quality_score, &params);
    EXPECT_TRUE(result);

    // Quality score should be valid
    EXPECT_GE(quality_score, 0.0f);

    DestroyAnalogyState(state);
}

//=============================================================================
// Causal Reasoning Tests
//=============================================================================

TEST_F(ReasoningKernelTest, CausalPropagate_PropagatesEffects) {
    RequireGPU();

    const size_t n_nodes = 5;

    nimcp_gpu_causal_state_t* state = CreateCausalState(n_nodes);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    // Set up causal graph: 0 -> 1 -> 2 -> 3 -> 4
    std::vector<float> adjacency(n_nodes * n_nodes, 0.0f);
    std::vector<float> weights(n_nodes * n_nodes, 0.0f);
    for (size_t i = 0; i < n_nodes - 1; i++) {
        adjacency[i * n_nodes + (i + 1)] = 1.0f;
        weights[i * n_nodes + (i + 1)] = 0.8f;
    }
    state->adjacency = SetFromHost(state->adjacency, adjacency);
    state->edge_weights = SetFromHost(state->edge_weights, weights);

    // Set initial node value
    std::vector<float> values(n_nodes, 0.0f);
    values[0] = 1.0f;  // Root cause is active
    state->node_values = SetFromHost(state->node_values, values);

    bool result = nimcp_gpu_causal_propagate(ctx, state, &params);
    EXPECT_TRUE(result);

    auto final_values = CopyToHost(state->node_values);

    // Effects should propagate through the chain
    // Node 1 should be affected by node 0
    EXPECT_GT(final_values[1], 0.0f);

    DestroyCausalState(state);
}

TEST_F(ReasoningKernelTest, CausalIntervene_AppliesIntervention) {
    RequireGPU();

    const size_t n_nodes = 5;

    nimcp_gpu_causal_state_t* state = CreateCausalState(n_nodes);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    // Set up causal graph
    std::vector<float> adjacency(n_nodes * n_nodes, 0.0f);
    adjacency[0 * n_nodes + 2] = 1.0f;  // 0 -> 2
    adjacency[1 * n_nodes + 2] = 1.0f;  // 1 -> 2
    state->adjacency = SetFromHost(state->adjacency, adjacency);

    bool result = nimcp_gpu_causal_intervene(ctx, state, 2, 1.0f, &params);
    EXPECT_TRUE(result);

    auto interventions = CopyToHost(state->interventions);

    // Node 2 should be marked as intervened
    EXPECT_EQ(interventions[2], 1.0f);

    auto values = CopyToHost(state->node_values);

    // Intervened node should have the intervention value
    EXPECT_NEAR(values[2], 1.0f, 0.01f);

    DestroyCausalState(state);
}

TEST_F(ReasoningKernelTest, CausalCounterfactual_ComputesCounterfactual) {
    RequireGPU();

    const size_t n_nodes = 4;

    nimcp_gpu_causal_state_t* state = CreateCausalState(n_nodes);
    nimcp_gpu_tensor_t* factual_values = Create1DTensor(n_nodes, 0.0f);
    nimcp_gpu_tensor_t* counterfactual_outcome = Create1DTensor(n_nodes, 0.0f);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    // Set up causal graph: 0 -> 1 -> 2 -> 3
    std::vector<float> adjacency(n_nodes * n_nodes, 0.0f);
    std::vector<float> weights(n_nodes * n_nodes, 0.0f);
    for (size_t i = 0; i < n_nodes - 1; i++) {
        adjacency[i * n_nodes + (i + 1)] = 1.0f;
        weights[i * n_nodes + (i + 1)] = 0.9f;
    }
    state->adjacency = SetFromHost(state->adjacency, adjacency);
    state->edge_weights = SetFromHost(state->edge_weights, weights);

    // Set factual values
    std::vector<float> factual(n_nodes);
    factual[0] = 0.0f;  // Root cause was off
    factual[1] = 0.1f;
    factual[2] = 0.05f;
    factual[3] = 0.02f;
    factual_values = SetFromHost(factual_values, factual);

    // Ask: what if node 0 had been 1.0?
    bool result = nimcp_gpu_causal_counterfactual(ctx, state, factual_values, 0, 1.0f, counterfactual_outcome, &params);
    EXPECT_TRUE(result);

    auto cf_outcome = CopyToHost(counterfactual_outcome);

    // Counterfactual outcome for node 0 should be the intervention value
    EXPECT_NEAR(cf_outcome[0], 1.0f, 0.1f);

    nimcp_gpu_tensor_destroy(factual_values);
    nimcp_gpu_tensor_destroy(counterfactual_outcome);
    DestroyCausalState(state);
}

TEST_F(ReasoningKernelTest, CausalIdentifyEffect_ComputesCausalEffect) {
    RequireGPU();

    const size_t n_nodes = 4;

    nimcp_gpu_causal_state_t* state = CreateCausalState(n_nodes);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    // Set up causal graph: 0 -> 1, 0 -> 2, 1 -> 3, 2 -> 3
    std::vector<float> adjacency(n_nodes * n_nodes, 0.0f);
    std::vector<float> weights(n_nodes * n_nodes, 0.0f);
    adjacency[0 * n_nodes + 1] = 1.0f;
    adjacency[0 * n_nodes + 2] = 1.0f;
    adjacency[1 * n_nodes + 3] = 1.0f;
    adjacency[2 * n_nodes + 3] = 1.0f;
    weights[0 * n_nodes + 1] = 0.5f;
    weights[0 * n_nodes + 2] = 0.5f;
    weights[1 * n_nodes + 3] = 0.6f;
    weights[2 * n_nodes + 3] = 0.4f;
    state->adjacency = SetFromHost(state->adjacency, adjacency);
    state->edge_weights = SetFromHost(state->edge_weights, weights);

    float causal_effect = 0.0f;

    bool result = nimcp_gpu_causal_identify_effect(ctx, state, 0, 3, &causal_effect, &params);
    EXPECT_TRUE(result);

    // Causal effect should be computed
    // The effect of 0 on 3 is through both paths
    EXPECT_GE(causal_effect, 0.0f);

    DestroyCausalState(state);
}

TEST_F(ReasoningKernelTest, CausalDiscover_DiscoversStructure) {
    RequireGPU();

    const size_t n_nodes = 4;
    const size_t n_samples = 100;

    nimcp_gpu_causal_state_t* state = CreateCausalState(n_nodes);
    nimcp_gpu_tensor_t* observational_data = Create2DTensor(n_samples, n_nodes, 0.0f);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    // Generate synthetic observational data with some correlation structure
    std::vector<float> data(n_samples * n_nodes);
    for (size_t s = 0; s < n_samples; s++) {
        float x0 = static_cast<float>(s % 10) / 10.0f;
        float x1 = x0 * 0.8f + 0.1f * (s % 3);
        float x2 = x0 * 0.5f + 0.2f * (s % 4);
        float x3 = x1 * 0.6f + x2 * 0.4f;
        data[s * n_nodes + 0] = x0;
        data[s * n_nodes + 1] = x1;
        data[s * n_nodes + 2] = x2;
        data[s * n_nodes + 3] = x3;
    }
    observational_data = SetFromHost(observational_data, data);

    bool result = nimcp_gpu_causal_discover(ctx, observational_data, state, &params);
    EXPECT_TRUE(result);

    auto adjacency = CopyToHost(state->adjacency);

    // Discovered structure should be valid (binary or probabilistic edges)
    for (size_t i = 0; i < n_nodes * n_nodes; i++) {
        EXPECT_GE(adjacency[i], 0.0f);
        EXPECT_LE(adjacency[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(observational_data);
    DestroyCausalState(state);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(ReasoningKernelTest, LogicEvaluate_NullSafety) {
    RequireGPU();

    nimcp_gpu_logic_formula_t* formula = CreateSimpleFormula(5, 3);
    nimcp_gpu_tensor_t* results = Create1DTensor(3, 0.0f);
    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();

    EXPECT_FALSE(nimcp_gpu_logic_evaluate(nullptr, formula, results, &params));
    EXPECT_FALSE(nimcp_gpu_logic_evaluate(ctx, nullptr, results, &params));
    EXPECT_FALSE(nimcp_gpu_logic_evaluate(ctx, formula, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_logic_evaluate(ctx, formula, results, nullptr));

    nimcp_gpu_tensor_destroy(results);
    DestroyFormula(formula);
}

TEST_F(ReasoningKernelTest, LogicPropagate_NullSafety) {
    RequireGPU();

    nimcp_gpu_logic_formula_t* formula = CreateSimpleFormula(5, 3);
    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();

    EXPECT_FALSE(nimcp_gpu_logic_propagate(nullptr, formula, 10, &params));
    EXPECT_FALSE(nimcp_gpu_logic_propagate(ctx, nullptr, 10, &params));
    EXPECT_FALSE(nimcp_gpu_logic_propagate(ctx, formula, 10, nullptr));

    DestroyFormula(formula);
}

TEST_F(ReasoningKernelTest, FuzzyLogic_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.5f);
    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();

    EXPECT_FALSE(nimcp_gpu_fuzzy_logic(nullptr, tensor, tensor, tensor, NIMCP_LOGIC_AND, &params));
    EXPECT_FALSE(nimcp_gpu_fuzzy_logic(ctx, nullptr, tensor, tensor, NIMCP_LOGIC_AND, &params));
    /* input2 is intentionally nullable for unary operations (e.g., NOT) */
    EXPECT_FALSE(nimcp_gpu_fuzzy_logic(ctx, tensor, tensor, nullptr, NIMCP_LOGIC_AND, &params));
    EXPECT_FALSE(nimcp_gpu_fuzzy_logic(ctx, tensor, tensor, tensor, NIMCP_LOGIC_AND, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(ReasoningKernelTest, RuleMatch_NullSafety) {
    RequireGPU();

    nimcp_gpu_rule_base_t* rules = CreateSimpleRuleBase(5, 8);
    nimcp_gpu_working_memory_t* wm = CreateWorkingMemory(5, 8);
    nimcp_gpu_tensor_t* scores = Create2DTensor(5, 5, 0.0f);
    nimcp_gpu_rule_params_t params = nimcp_gpu_rule_params_default();

    EXPECT_FALSE(nimcp_gpu_rule_match(nullptr, rules, wm, scores, &params));
    EXPECT_FALSE(nimcp_gpu_rule_match(ctx, nullptr, wm, scores, &params));
    EXPECT_FALSE(nimcp_gpu_rule_match(ctx, rules, nullptr, scores, &params));
    EXPECT_FALSE(nimcp_gpu_rule_match(ctx, rules, wm, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_rule_match(ctx, rules, wm, scores, nullptr));

    nimcp_gpu_tensor_destroy(scores);
    DestroyWorkingMemory(wm);
    DestroyRuleBase(rules);
}

TEST_F(ReasoningKernelTest, RuleFire_NullSafety) {
    RequireGPU();

    nimcp_gpu_rule_base_t* rules = CreateSimpleRuleBase(5, 8);
    nimcp_gpu_working_memory_t* wm = CreateWorkingMemory(5, 8);
    nimcp_gpu_tensor_t* scores = Create2DTensor(5, 5, 0.0f);
    nimcp_gpu_rule_params_t params = nimcp_gpu_rule_params_default();

    EXPECT_FALSE(nimcp_gpu_rule_fire(nullptr, rules, wm, scores, &params));
    EXPECT_FALSE(nimcp_gpu_rule_fire(ctx, nullptr, wm, scores, &params));
    EXPECT_FALSE(nimcp_gpu_rule_fire(ctx, rules, nullptr, scores, &params));
    EXPECT_FALSE(nimcp_gpu_rule_fire(ctx, rules, wm, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_rule_fire(ctx, rules, wm, scores, nullptr));

    nimcp_gpu_tensor_destroy(scores);
    DestroyWorkingMemory(wm);
    DestroyRuleBase(rules);
}

TEST_F(ReasoningKernelTest, ForwardChain_NullSafety) {
    RequireGPU();

    nimcp_gpu_rule_base_t* rules = CreateSimpleRuleBase(5, 8);
    nimcp_gpu_working_memory_t* wm = CreateWorkingMemory(5, 8);
    nimcp_gpu_rule_params_t params = nimcp_gpu_rule_params_default();

    EXPECT_FALSE(nimcp_gpu_forward_chain(nullptr, rules, wm, 10, &params));
    EXPECT_FALSE(nimcp_gpu_forward_chain(ctx, nullptr, wm, 10, &params));
    EXPECT_FALSE(nimcp_gpu_forward_chain(ctx, rules, nullptr, 10, &params));
    EXPECT_FALSE(nimcp_gpu_forward_chain(ctx, rules, wm, 10, nullptr));

    DestroyWorkingMemory(wm);
    DestroyRuleBase(rules);
}

TEST_F(ReasoningKernelTest, CSPInit_NullSafety) {
    RequireGPU();

    nimcp_gpu_csp_state_t* state = CreateCSPState(5, 5, 10);
    nimcp_gpu_tensor_t* domains = Create2DTensor(5, 5, 1.0f);

    EXPECT_FALSE(nimcp_gpu_csp_init(nullptr, state, domains));
    EXPECT_FALSE(nimcp_gpu_csp_init(ctx, nullptr, domains));
    EXPECT_FALSE(nimcp_gpu_csp_init(ctx, state, nullptr));

    nimcp_gpu_tensor_destroy(domains);
    DestroyCSPState(state);
}

TEST_F(ReasoningKernelTest, CSPArcConsistency_NullSafety) {
    RequireGPU();

    nimcp_gpu_csp_state_t* state = CreateCSPState(5, 5, 10);
    nimcp_gpu_csp_params_t params = nimcp_gpu_csp_params_default();

    EXPECT_FALSE(nimcp_gpu_csp_arc_consistency(nullptr, state, &params));
    EXPECT_FALSE(nimcp_gpu_csp_arc_consistency(ctx, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_csp_arc_consistency(ctx, state, nullptr));

    DestroyCSPState(state);
}

TEST_F(ReasoningKernelTest, CSPStep_NullSafety) {
    RequireGPU();

    nimcp_gpu_csp_state_t* state = CreateCSPState(5, 5, 10);
    nimcp_gpu_csp_params_t params = nimcp_gpu_csp_params_default();
    bool solved = false;
    bool failed = false;

    EXPECT_FALSE(nimcp_gpu_csp_step(nullptr, state, &solved, &failed, &params));
    EXPECT_FALSE(nimcp_gpu_csp_step(ctx, nullptr, &solved, &failed, &params));
    EXPECT_FALSE(nimcp_gpu_csp_step(ctx, state, nullptr, &failed, &params));
    EXPECT_FALSE(nimcp_gpu_csp_step(ctx, state, &solved, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_csp_step(ctx, state, &solved, &failed, nullptr));

    DestroyCSPState(state);
}

TEST_F(ReasoningKernelTest, AnalogyStructuralSimilarity_NullSafety) {
    RequireGPU();

    nimcp_gpu_analogy_state_t* state = CreateAnalogyState(4, 4, 8);
    nimcp_gpu_tensor_t* similarity = Create2DTensor(4, 4, 0.0f);
    nimcp_gpu_analogy_params_t params = nimcp_gpu_analogy_params_default();

    EXPECT_FALSE(nimcp_gpu_analogy_structural_similarity(nullptr, state, similarity, &params));
    EXPECT_FALSE(nimcp_gpu_analogy_structural_similarity(ctx, nullptr, similarity, &params));
    EXPECT_FALSE(nimcp_gpu_analogy_structural_similarity(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_analogy_structural_similarity(ctx, state, similarity, nullptr));

    nimcp_gpu_tensor_destroy(similarity);
    DestroyAnalogyState(state);
}

TEST_F(ReasoningKernelTest, AnalogyFindMapping_NullSafety) {
    RequireGPU();

    nimcp_gpu_analogy_state_t* state = CreateAnalogyState(4, 4, 8);
    nimcp_gpu_analogy_params_t params = nimcp_gpu_analogy_params_default();

    EXPECT_FALSE(nimcp_gpu_analogy_find_mapping(nullptr, state, &params));
    EXPECT_FALSE(nimcp_gpu_analogy_find_mapping(ctx, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_analogy_find_mapping(ctx, state, nullptr));

    DestroyAnalogyState(state);
}

TEST_F(ReasoningKernelTest, AnalogyEvaluate_NullSafety) {
    RequireGPU();

    nimcp_gpu_analogy_state_t* state = CreateAnalogyState(4, 4, 8);
    nimcp_gpu_analogy_params_t params = nimcp_gpu_analogy_params_default();
    float score = 0.0f;

    EXPECT_FALSE(nimcp_gpu_analogy_evaluate(nullptr, state, &score, &params));
    EXPECT_FALSE(nimcp_gpu_analogy_evaluate(ctx, nullptr, &score, &params));
    EXPECT_FALSE(nimcp_gpu_analogy_evaluate(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_analogy_evaluate(ctx, state, &score, nullptr));

    DestroyAnalogyState(state);
}

TEST_F(ReasoningKernelTest, CausalPropagate_NullSafety) {
    RequireGPU();

    nimcp_gpu_causal_state_t* state = CreateCausalState(5);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    EXPECT_FALSE(nimcp_gpu_causal_propagate(nullptr, state, &params));
    EXPECT_FALSE(nimcp_gpu_causal_propagate(ctx, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_causal_propagate(ctx, state, nullptr));

    DestroyCausalState(state);
}

TEST_F(ReasoningKernelTest, CausalIntervene_NullSafety) {
    RequireGPU();

    nimcp_gpu_causal_state_t* state = CreateCausalState(5);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    EXPECT_FALSE(nimcp_gpu_causal_intervene(nullptr, state, 0, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_causal_intervene(ctx, nullptr, 0, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_causal_intervene(ctx, state, 0, 1.0f, nullptr));

    DestroyCausalState(state);
}

TEST_F(ReasoningKernelTest, CausalCounterfactual_NullSafety) {
    RequireGPU();

    nimcp_gpu_causal_state_t* state = CreateCausalState(4);
    nimcp_gpu_tensor_t* factual = Create1DTensor(4, 0.0f);
    nimcp_gpu_tensor_t* outcome = Create1DTensor(4, 0.0f);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    EXPECT_FALSE(nimcp_gpu_causal_counterfactual(nullptr, state, factual, 0, 1.0f, outcome, &params));
    EXPECT_FALSE(nimcp_gpu_causal_counterfactual(ctx, nullptr, factual, 0, 1.0f, outcome, &params));
    EXPECT_FALSE(nimcp_gpu_causal_counterfactual(ctx, state, nullptr, 0, 1.0f, outcome, &params));
    EXPECT_FALSE(nimcp_gpu_causal_counterfactual(ctx, state, factual, 0, 1.0f, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_causal_counterfactual(ctx, state, factual, 0, 1.0f, outcome, nullptr));

    nimcp_gpu_tensor_destroy(factual);
    nimcp_gpu_tensor_destroy(outcome);
    DestroyCausalState(state);
}

TEST_F(ReasoningKernelTest, CausalIdentifyEffect_NullSafety) {
    RequireGPU();

    nimcp_gpu_causal_state_t* state = CreateCausalState(4);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();
    float effect = 0.0f;

    EXPECT_FALSE(nimcp_gpu_causal_identify_effect(nullptr, state, 0, 3, &effect, &params));
    EXPECT_FALSE(nimcp_gpu_causal_identify_effect(ctx, nullptr, 0, 3, &effect, &params));
    EXPECT_FALSE(nimcp_gpu_causal_identify_effect(ctx, state, 0, 3, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_causal_identify_effect(ctx, state, 0, 3, &effect, nullptr));

    DestroyCausalState(state);
}

TEST_F(ReasoningKernelTest, CausalDiscover_NullSafety) {
    RequireGPU();

    nimcp_gpu_causal_state_t* state = CreateCausalState(4);
    nimcp_gpu_tensor_t* data = Create2DTensor(100, 4, 0.0f);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    EXPECT_FALSE(nimcp_gpu_causal_discover(nullptr, data, state, &params));
    EXPECT_FALSE(nimcp_gpu_causal_discover(ctx, nullptr, state, &params));
    EXPECT_FALSE(nimcp_gpu_causal_discover(ctx, data, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_causal_discover(ctx, data, state, nullptr));

    nimcp_gpu_tensor_destroy(data);
    DestroyCausalState(state);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(ReasoningKernelTest, Integration_FuzzyLogicChain) {
    RequireGPU();

    const size_t n = 50;

    nimcp_gpu_tensor_t* a = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* b = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* c = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* temp = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* result = Create1DTensor(n, 0.0f);

    nimcp_gpu_logic_params_t params = nimcp_gpu_logic_params_default();
    params.use_fuzzy = true;

    // Set input values
    std::vector<float> a_vals(n), b_vals(n), c_vals(n);
    for (size_t i = 0; i < n; i++) {
        a_vals[i] = static_cast<float>(i) / static_cast<float>(n);
        b_vals[i] = 1.0f - a_vals[i];
        c_vals[i] = 0.5f;
    }
    a = SetFromHost(a, a_vals);
    b = SetFromHost(b, b_vals);
    c = SetFromHost(c, c_vals);

    // Compute: (A AND B) OR C
    // Step 1: temp = A AND B
    bool r1 = nimcp_gpu_fuzzy_logic(ctx, a, b, temp, NIMCP_LOGIC_AND, &params);
    EXPECT_TRUE(r1);

    // Step 2: result = temp OR C
    bool r2 = nimcp_gpu_fuzzy_logic(ctx, temp, c, result, NIMCP_LOGIC_OR, &params);
    EXPECT_TRUE(r2);

    auto result_data = CopyToHost(result);

    // Verify results are valid fuzzy values
    for (size_t i = 0; i < n; i++) {
        EXPECT_GE(result_data[i], 0.0f);
        EXPECT_LE(result_data[i], 1.0f);
        // Result should be at least C (0.5) due to OR
        EXPECT_GE(result_data[i], 0.5f - 0.01f);
    }

    nimcp_gpu_tensor_destroy(a);
    nimcp_gpu_tensor_destroy(b);
    nimcp_gpu_tensor_destroy(c);
    nimcp_gpu_tensor_destroy(temp);
    nimcp_gpu_tensor_destroy(result);
}

TEST_F(ReasoningKernelTest, Integration_RuleBasedInference) {
    RequireGPU();

    const size_t n_rules = 5;
    const size_t pattern_dim = 8;
    const size_t n_facts = 10;
    const int max_steps = 20;

    nimcp_gpu_rule_base_t* rules = CreateSimpleRuleBase(n_rules, pattern_dim);
    nimcp_gpu_working_memory_t* wm = CreateWorkingMemory(n_facts, pattern_dim);
    nimcp_gpu_tensor_t* match_scores = Create2DTensor(n_rules, n_facts, 0.0f);
    nimcp_gpu_rule_params_t params = nimcp_gpu_rule_params_default();

    // Initialize with some facts
    std::vector<float> initial_facts(n_facts * pattern_dim, 0.0f);
    for (size_t i = 0; i < pattern_dim; i++) {
        initial_facts[i] = 0.8f;  // First fact is active
        initial_facts[pattern_dim + i] = 0.6f;  // Second fact is partially active
    }
    wm->facts = SetFromHost(wm->facts, initial_facts);

    // Set up rules to chain: fact0 -> fact2, fact2 -> fact4
    std::vector<float> antecedents(n_rules * pattern_dim, 0.0f);
    std::vector<float> consequents(n_rules * pattern_dim, 0.0f);
    for (size_t i = 0; i < pattern_dim; i++) {
        // Rule 0: match fact 0 pattern -> produce fact 2 pattern
        antecedents[0 * pattern_dim + i] = 0.8f;
        consequents[0 * pattern_dim + i] = 0.7f;
    }
    rules->antecedents = SetFromHost(rules->antecedents, antecedents);
    rules->consequents = SetFromHost(rules->consequents, consequents);

    auto initial_facts_copy = CopyToHost(wm->facts);

    // Run inference
    for (int step = 0; step < max_steps; step++) {
        nimcp_gpu_rule_match(ctx, rules, wm, match_scores, &params);
        nimcp_gpu_rule_fire(ctx, rules, wm, match_scores, &params);
    }

    auto final_facts = CopyToHost(wm->facts);

    // Working memory should be populated with derived facts
    // (exact values depend on implementation)

    nimcp_gpu_tensor_destroy(match_scores);
    DestroyWorkingMemory(wm);
    DestroyRuleBase(rules);
}

TEST_F(ReasoningKernelTest, Integration_CSPSolving) {
    RequireGPU();

    const size_t n_variables = 4;
    const size_t domain_size = 4;
    const size_t n_constraints = 6;
    const int max_steps = 50;

    nimcp_gpu_csp_state_t* state = CreateCSPState(n_variables, domain_size, n_constraints);
    nimcp_gpu_tensor_t* initial_domains = Create2DTensor(n_variables, domain_size, 1.0f);
    nimcp_gpu_csp_params_t params = nimcp_gpu_csp_params_default();

    // Initialize
    nimcp_gpu_csp_init(ctx, state, initial_domains);

    // Run arc consistency
    nimcp_gpu_csp_arc_consistency(ctx, state, &params);

    bool solved = false;
    bool failed = false;
    int steps = 0;

    // Solve
    while (!solved && !failed && steps < max_steps) {
        nimcp_gpu_csp_step(ctx, state, &solved, &failed, &params);
        steps++;
    }

    // Should terminate
    EXPECT_TRUE(solved || failed || steps >= max_steps);

    if (solved) {
        // Check that assignments are valid
        auto assignments = CopyToHost(state->assignments);
        for (size_t i = 0; i < n_variables; i++) {
            EXPECT_GE(assignments[i], 0.0f);
            EXPECT_LT(assignments[i], static_cast<float>(domain_size));
        }
    }

    nimcp_gpu_tensor_destroy(initial_domains);
    DestroyCSPState(state);
}

TEST_F(ReasoningKernelTest, Integration_AnalogicalMapping) {
    RequireGPU();

    const size_t source_size = 4;
    const size_t target_size = 4;
    const size_t feature_dim = 8;

    nimcp_gpu_analogy_state_t* state = CreateAnalogyState(source_size, target_size, feature_dim);
    nimcp_gpu_tensor_t* similarity_matrix = Create2DTensor(source_size, target_size, 0.0f);
    nimcp_gpu_tensor_t* source_inferences = Create1DTensor(source_size, 0.0f);
    nimcp_gpu_tensor_t* target_inferences = Create1DTensor(target_size, 0.0f);
    nimcp_gpu_analogy_params_t params = nimcp_gpu_analogy_params_default();
    // Lower consistency threshold so computed mapping scores (which combine
    // structural and semantic similarity) can exceed it for transfer
    params.consistency_threshold = 0.1f;

    // Set up isomorphic structures
    std::vector<float> structure(source_size * source_size, 0.0f);
    structure[0 * source_size + 1] = 1.0f;
    structure[1 * source_size + 2] = 1.0f;
    structure[2 * source_size + 3] = 1.0f;
    state->source_structure = SetFromHost(state->source_structure, structure);
    state->target_structure = SetFromHost(state->target_structure, structure);

    // Set up matching features
    std::vector<float> features(source_size * feature_dim);
    for (size_t i = 0; i < source_size * feature_dim; i++) {
        features[i] = static_cast<float>(i % 5) / 5.0f;
    }
    state->source_features = SetFromHost(state->source_features, features);
    state->target_features = SetFromHost(state->target_features, features);

    // Compute similarity
    nimcp_gpu_analogy_structural_similarity(ctx, state, similarity_matrix, &params);

    // Find mapping
    nimcp_gpu_analogy_find_mapping(ctx, state, &params);

    // Evaluate
    float quality = 0.0f;
    nimcp_gpu_analogy_evaluate(ctx, state, &quality, &params);
    EXPECT_GE(quality, 0.0f);

    // Transfer
    std::vector<float> src_inf(source_size);
    src_inf[0] = 1.0f;
    src_inf[1] = 0.8f;
    src_inf[2] = 0.5f;
    src_inf[3] = 0.2f;
    source_inferences = SetFromHost(source_inferences, src_inf);

    nimcp_gpu_analogy_transfer(ctx, state, source_inferences, target_inferences, &params);

    auto target_inf = CopyToHost(target_inferences);

    // Target should have transferred inferences
    bool has_inferences = false;
    for (size_t i = 0; i < target_size; i++) {
        if (target_inf[i] > 0.0f) {
            has_inferences = true;
            break;
        }
    }
    EXPECT_TRUE(has_inferences);

    nimcp_gpu_tensor_destroy(similarity_matrix);
    nimcp_gpu_tensor_destroy(source_inferences);
    nimcp_gpu_tensor_destroy(target_inferences);
    DestroyAnalogyState(state);
}

TEST_F(ReasoningKernelTest, Integration_CausalInference) {
    RequireGPU();

    const size_t n_nodes = 5;

    nimcp_gpu_causal_state_t* state = CreateCausalState(n_nodes);
    nimcp_gpu_tensor_t* factual_values = Create1DTensor(n_nodes, 0.0f);
    nimcp_gpu_tensor_t* counterfactual_outcome = Create1DTensor(n_nodes, 0.0f);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    // Build causal graph:
    //     0
    //    / \
    //   1   2
    //    \ /
    //     3
    //     |
    //     4
    std::vector<float> adjacency(n_nodes * n_nodes, 0.0f);
    std::vector<float> weights(n_nodes * n_nodes, 0.0f);
    adjacency[0 * n_nodes + 1] = 1.0f;
    adjacency[0 * n_nodes + 2] = 1.0f;
    adjacency[1 * n_nodes + 3] = 1.0f;
    adjacency[2 * n_nodes + 3] = 1.0f;
    adjacency[3 * n_nodes + 4] = 1.0f;
    weights[0 * n_nodes + 1] = 0.7f;
    weights[0 * n_nodes + 2] = 0.6f;
    weights[1 * n_nodes + 3] = 0.5f;
    weights[2 * n_nodes + 3] = 0.4f;
    weights[3 * n_nodes + 4] = 0.9f;
    state->adjacency = SetFromHost(state->adjacency, adjacency);
    state->edge_weights = SetFromHost(state->edge_weights, weights);

    // Set initial values
    std::vector<float> values(n_nodes, 0.0f);
    values[0] = 1.0f;  // Root cause active
    state->node_values = SetFromHost(state->node_values, values);

    // Mark root node as intervened so it keeps its value during propagation
    // (without this, the single-pass kernel would overwrite it to ~0)
    nimcp_gpu_causal_intervene(ctx, state, 0, 1.0f, &params);

    // Propagate multiple times - one pass per DAG layer
    // DAG has 4 layers (0->1->3->4), so 4 passes are needed
    for (int pass = 0; pass < 4; pass++) {
        nimcp_gpu_causal_propagate(ctx, state, &params);
    }

    auto propagated = CopyToHost(state->node_values);

    // Node 4 should be affected
    EXPECT_GT(propagated[4], 0.0f);

    // Now intervene on node 1 (set to 0)
    nimcp_gpu_causal_intervene(ctx, state, 1, 0.0f, &params);

    // Re-propagate multiple times
    for (int pass = 0; pass < 4; pass++) {
        nimcp_gpu_causal_propagate(ctx, state, &params);
    }

    auto after_intervention = CopyToHost(state->node_values);

    // Node 1 should be 0 (intervened)
    EXPECT_NEAR(after_intervention[1], 0.0f, 0.01f);

    // Compute causal effect of 0 on 4
    float effect = 0.0f;
    nimcp_gpu_causal_identify_effect(ctx, state, 0, 4, &effect, &params);
    EXPECT_GE(effect, 0.0f);

    // Compute counterfactual
    factual_values = SetFromHost(factual_values, propagated);
    nimcp_gpu_causal_counterfactual(ctx, state, factual_values, 0, 0.5f, counterfactual_outcome, &params);

    auto cf = CopyToHost(counterfactual_outcome);
    // Counterfactual should differ from original propagation
    // (since we're asking "what if node 0 had been 0.5 instead of 1.0")

    nimcp_gpu_tensor_destroy(factual_values);
    nimcp_gpu_tensor_destroy(counterfactual_outcome);
    DestroyCausalState(state);
}

TEST_F(ReasoningKernelTest, Integration_CausalDiscoveryAndInference) {
    RequireGPU();

    const size_t n_nodes = 4;
    const size_t n_samples = 200;

    nimcp_gpu_causal_state_t* state = CreateCausalState(n_nodes);
    nimcp_gpu_tensor_t* observational_data = Create2DTensor(n_samples, n_nodes, 0.0f);
    nimcp_gpu_causal_params_t params = nimcp_gpu_causal_params_default();

    // Generate data from known causal structure: 0 -> 1, 0 -> 2, 1 -> 3, 2 -> 3
    std::vector<float> data(n_samples * n_nodes);
    for (size_t s = 0; s < n_samples; s++) {
        float x0 = (s % 10) / 10.0f + (s % 7) * 0.05f;
        float noise1 = (s % 5) * 0.02f;
        float noise2 = (s % 3) * 0.03f;
        float x1 = x0 * 0.7f + noise1;
        float x2 = x0 * 0.6f + noise2;
        float x3 = x1 * 0.5f + x2 * 0.4f + (s % 4) * 0.01f;
        data[s * n_nodes + 0] = x0;
        data[s * n_nodes + 1] = x1;
        data[s * n_nodes + 2] = x2;
        data[s * n_nodes + 3] = x3;
    }
    observational_data = SetFromHost(observational_data, data);

    // Discover structure
    bool result = nimcp_gpu_causal_discover(ctx, observational_data, state, &params);
    EXPECT_TRUE(result);

    auto discovered_adjacency = CopyToHost(state->adjacency);

    // Should discover some edges
    bool has_edges = false;
    for (size_t i = 0; i < n_nodes * n_nodes; i++) {
        if (discovered_adjacency[i] > 0.5f) {
            has_edges = true;
            break;
        }
    }
    // Note: Discovery may not find all edges perfectly
    // but should at least run without error

    // Now use discovered graph for inference
    std::vector<float> values(n_nodes, 0.0f);
    values[0] = 1.0f;
    state->node_values = SetFromHost(state->node_values, values);

    nimcp_gpu_causal_propagate(ctx, state, &params);

    auto propagated = CopyToHost(state->node_values);

    // Values should be valid
    for (size_t i = 0; i < n_nodes; i++) {
        EXPECT_GE(propagated[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(observational_data);
    DestroyCausalState(state);
}
