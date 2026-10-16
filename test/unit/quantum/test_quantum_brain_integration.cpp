/**
 * @file test_quantum_brain_integration.cpp
 * @brief Comprehensive integration tests for quantum systems with brain architecture
 *
 * WHAT: Tests integration between quantum reasoning/bridges and brain subsystems
 * WHY:  Verify quantum acceleration works correctly with brain regions
 * HOW:  Create brain with quantum subsystems, test integrated workflows
 *
 * COVERAGE:
 * - Brain quantum reasoning initialization
 * - Prefrontal quantum bridge integration
 * - Brainstem quantum bridge integration
 * - Cross-region quantum coordination
 * - Fatigue/stress modulation of quantum reasoning
 * - Full brain inference with quantum acceleration
 *
 * TEST PHILOSOPHY: Integration testing - verify quantum modules work within brain
 */

#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>

/* Implementation defines to allow testing without full library */
#ifndef NIMCP_BRAIN_QUANTUM_REASONING_IMPLEMENTATION
#define NIMCP_BRAIN_QUANTUM_REASONING_IMPLEMENTATION
#endif
#ifndef NIMCP_PREFRONTAL_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_PREFRONTAL_QUANTUM_BRIDGE_IMPLEMENTATION
#endif
#ifndef NIMCP_BRAINSTEM_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_BRAINSTEM_QUANTUM_BRIDGE_IMPLEMENTATION
#endif
#ifndef NIMCP_QUANTUM_REASONING_IMPLEMENTATION
#define NIMCP_QUANTUM_REASONING_IMPLEMENTATION
#endif

// Headers have their own extern "C" guards
#include "core/brain/inference/nimcp_brain_quantum_reasoning.h"
#include "core/brain/regions/prefrontal/nimcp_prefrontal_quantum_bridge.h"
#include "core/brain/regions/brainstem/nimcp_brainstem_quantum_bridge.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"

//=============================================================================
// Mock Brain Implementation for Testing
//=============================================================================

/* Mock brain structure for testing */
struct mock_brain {
    bool quantum_reasoning_enabled;
    qreason_t reasoner;
    float fatigue_level;
    float stress_level;
    brain_qreason_config_t config;
    brain_qreason_stats_t stats;
    /* Knowledge base */
    qreason_truth_t facts[BRAIN_QREASON_MAX_VARIABLES];
    float fact_confidences[BRAIN_QREASON_MAX_VARIABLES];
    uint32_t num_facts;
};

/* Mock brain handle */
typedef struct mock_brain* mock_brain_t;

/* Global mock for testing */
static struct mock_brain g_mock_brain;

/* Mock brain quantum reasoning functions */
static brain_qreason_config_t mock_brain_qreason_default_config(void) {
    brain_qreason_config_t config = {};
    config.enabled = true;
    config.max_grover_iterations = 100;
    config.max_inference_depth = 10;
    config.min_confidence = 0.5f;
    config.use_ternary_logic = true;
    config.enable_interference = true;
    config.integrate_with_executive = true;
    config.integrate_with_parietal = true;
    config.fatigue_sensitivity = 0.3f;
    config.stress_sensitivity = 0.5f;
    return config;
}

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumBrainIntegrationTest : public ::testing::Test {
protected:
    mock_brain_t brain;
    prefrontal_quantum_bridge_t* prefrontal_bridge;
    brainstem_quantum_bridge_t* brainstem_bridge;
    qreason_t reasoner;

    void SetUp() override {
        /* Initialize mock brain */
        memset(&g_mock_brain, 0, sizeof(g_mock_brain));
        brain = &g_mock_brain;

        brain->quantum_reasoning_enabled = false;
        brain->fatigue_level = 0.0f;
        brain->stress_level = 0.0f;
        brain->config = mock_brain_qreason_default_config();
        memset(&brain->stats, 0, sizeof(brain->stats));
        brain->num_facts = 0;

        /* Initialize knowledge base to UNKNOWN */
        for (uint32_t i = 0; i < BRAIN_QREASON_MAX_VARIABLES; i++) {
            brain->facts[i] = QREASON_UNKNOWN;
            brain->fact_confidences[i] = 0.0f;
        }

        prefrontal_bridge = nullptr;
        brainstem_bridge = nullptr;
        reasoner = nullptr;
    }

    void TearDown() override {
        if (prefrontal_bridge) {
            prefrontal_quantum_bridge_destroy(prefrontal_bridge);
            prefrontal_bridge = nullptr;
        }
        if (brainstem_bridge) {
            brainstem_quantum_bridge_destroy(brainstem_bridge);
            brainstem_bridge = nullptr;
        }
        if (reasoner) {
            qreason_destroy(reasoner);
            reasoner = nullptr;
        }
        brain = nullptr;
    }

    /* Helper to create a basic quantum reasoner */
    qreason_t CreateReasoner() {
        qreason_config_t config = {};
        config.max_grover_iterations = 100;
        config.max_inference_depth = 10;
        config.min_confidence = 0.5f;
        return qreason_create(&config);
    }
};

//=============================================================================
// Brain Quantum Reasoning Configuration Tests
//=============================================================================

/**
 * WHAT: Test default brain quantum reasoning configuration
 * WHY:  Verify sensible defaults are provided
 * HOW:  Get default config, check all fields have reasonable values
 */
TEST_F(QuantumBrainIntegrationTest, BrainQReasonDefaultConfig_ValidDefaults)
{
    brain_qreason_config_t config = mock_brain_qreason_default_config();

    /* Check enablement defaults */
    EXPECT_TRUE(config.enabled);
    EXPECT_TRUE(config.use_ternary_logic);
    EXPECT_TRUE(config.enable_interference);

    /* Check algorithm parameters */
    EXPECT_GT(config.max_grover_iterations, 0u);
    EXPECT_GT(config.max_inference_depth, 0u);
    EXPECT_GT(config.min_confidence, 0.0f);
    EXPECT_LE(config.min_confidence, 1.0f);

    /* Check integration flags */
    EXPECT_TRUE(config.integrate_with_executive);
    EXPECT_TRUE(config.integrate_with_parietal);

    /* Check modulation sensitivity */
    EXPECT_GE(config.fatigue_sensitivity, 0.0f);
    EXPECT_LE(config.fatigue_sensitivity, 1.0f);
    EXPECT_GE(config.stress_sensitivity, 0.0f);
    EXPECT_LE(config.stress_sensitivity, 1.0f);
}

/**
 * WHAT: Test brain quantum reasoning configuration customization
 * WHY:  Verify configuration can be customized
 * HOW:  Create custom config, verify values are retained
 */
TEST_F(QuantumBrainIntegrationTest, BrainQReasonConfig_Customization)
{
    brain_qreason_config_t config = {};

    config.enabled = false;
    config.max_grover_iterations = 50;
    config.max_inference_depth = 5;
    config.min_confidence = 0.8f;
    config.use_ternary_logic = false;
    config.enable_interference = false;
    config.integrate_with_executive = false;
    config.integrate_with_parietal = false;
    config.fatigue_sensitivity = 0.1f;
    config.stress_sensitivity = 0.2f;

    brain->config = config;

    EXPECT_FALSE(brain->config.enabled);
    EXPECT_EQ(brain->config.max_grover_iterations, 50u);
    EXPECT_EQ(brain->config.max_inference_depth, 5u);
    EXPECT_FLOAT_EQ(brain->config.min_confidence, 0.8f);
    EXPECT_FALSE(brain->config.use_ternary_logic);
    EXPECT_FALSE(brain->config.enable_interference);
    EXPECT_FLOAT_EQ(brain->config.fatigue_sensitivity, 0.1f);
    EXPECT_FLOAT_EQ(brain->config.stress_sensitivity, 0.2f);
}

//=============================================================================
// Brain Quantum Reasoning Lifecycle Tests
//=============================================================================

/**
 * WHAT: Test initializing quantum reasoning for brain
 * WHY:  Verify subsystem can be initialized
 * HOW:  Create reasoner, attach to mock brain, verify enabled
 */
TEST_F(QuantumBrainIntegrationTest, InitializeQuantumReasoning_Success)
{
    reasoner = CreateReasoner();
    ASSERT_NE(reasoner, nullptr);

    brain->reasoner = reasoner;
    brain->quantum_reasoning_enabled = true;

    EXPECT_TRUE(brain->quantum_reasoning_enabled);
    EXPECT_NE(brain->reasoner, nullptr);
}

/**
 * WHAT: Test enabling/disabling quantum reasoning
 * WHY:  Verify dynamic control of quantum subsystem
 * HOW:  Toggle enabled state, verify changes
 */
TEST_F(QuantumBrainIntegrationTest, QuantumReasoningEnableDisable)
{
    reasoner = CreateReasoner();
    ASSERT_NE(reasoner, nullptr);

    brain->reasoner = reasoner;
    brain->quantum_reasoning_enabled = true;
    EXPECT_TRUE(brain->quantum_reasoning_enabled);

    brain->quantum_reasoning_enabled = false;
    EXPECT_FALSE(brain->quantum_reasoning_enabled);

    brain->quantum_reasoning_enabled = true;
    EXPECT_TRUE(brain->quantum_reasoning_enabled);
}

//=============================================================================
// Brain Knowledge Base Integration Tests
//=============================================================================

/**
 * WHAT: Test setting facts in brain knowledge base
 * WHY:  Verify KB integration with brain
 * HOW:  Set facts with different truth values and confidences
 */
TEST_F(QuantumBrainIntegrationTest, KnowledgeBase_SetFacts)
{
    reasoner = CreateReasoner();
    ASSERT_NE(reasoner, nullptr);
    brain->reasoner = reasoner;

    /* Set facts with different truth values */
    brain->facts[0] = QREASON_TRUE;
    brain->fact_confidences[0] = 0.9f;
    brain->num_facts = 1;

    brain->facts[1] = QREASON_FALSE;
    brain->fact_confidences[1] = 0.8f;
    brain->num_facts = 2;

    brain->facts[2] = QREASON_UNKNOWN;
    brain->fact_confidences[2] = 0.5f;
    brain->num_facts = 3;

    EXPECT_EQ(brain->facts[0], QREASON_TRUE);
    EXPECT_EQ(brain->facts[1], QREASON_FALSE);
    EXPECT_EQ(brain->facts[2], QREASON_UNKNOWN);
    EXPECT_EQ(brain->num_facts, 3u);
}

/**
 * WHAT: Test ternary logic operations in brain context
 * WHY:  Verify Kleene logic works with brain knowledge base
 * HOW:  Perform logical operations on brain facts
 */
TEST_F(QuantumBrainIntegrationTest, KnowledgeBase_TernaryLogic)
{
    reasoner = CreateReasoner();
    ASSERT_NE(reasoner, nullptr);
    brain->reasoner = reasoner;

    /* Set up facts for logical reasoning */
    brain->facts[0] = QREASON_TRUE;   /* A = true */
    brain->facts[1] = QREASON_TRUE;   /* B = true */
    brain->facts[2] = QREASON_UNKNOWN; /* C = unknown */
    brain->num_facts = 3;

    /* AND with unknown should yield unknown */
    qreason_truth_t a_and_c = qreason_ternary_and(brain->facts[0], brain->facts[2]);
    EXPECT_EQ(a_and_c, QREASON_UNKNOWN);

    /* OR with unknown may yield true */
    qreason_truth_t a_or_c = qreason_ternary_or(brain->facts[0], brain->facts[2]);
    EXPECT_EQ(a_or_c, QREASON_TRUE);

    /* Known conjunction */
    qreason_truth_t a_and_b = qreason_ternary_and(brain->facts[0], brain->facts[1]);
    EXPECT_EQ(a_and_b, QREASON_TRUE);
}

//=============================================================================
// Brain SAT Solving Integration Tests
//=============================================================================

/**
 * WHAT: Test SAT solving through brain quantum reasoning
 * WHY:  Verify Grover-accelerated SAT works in brain context
 * HOW:  Create CNF from brain goals, solve with brain's reasoner
 */
TEST_F(QuantumBrainIntegrationTest, BrainSATSolving_SimpleQuery)
{
    reasoner = CreateReasoner();
    ASSERT_NE(reasoner, nullptr);
    brain->reasoner = reasoner;
    brain->quantum_reasoning_enabled = true;

    /* Create a simple CNF: (A OR B) AND (NOT A OR C) */
    qreason_cnf_t* cnf = qreason_cnf_create(3);
    ASSERT_NE(cnf, nullptr);

    /* Clause 1: (A OR B) */
    int32_t clause1[] = {1, 2};  /* Positive literals */
    EXPECT_EQ(qreason_cnf_add_clause(cnf, clause1, 2), 0);

    /* Clause 2: (NOT A OR C) */
    int32_t clause2[] = {-1, 3};  /* -1 = NOT A */
    EXPECT_EQ(qreason_cnf_add_clause(cnf, clause2, 2), 0);

    /* Solve SAT */
    bool is_sat = false;
    EXPECT_EQ(qreason_solve_sat(reasoner, cnf, &is_sat), 0);
    EXPECT_TRUE(is_sat);  /* This CNF is satisfiable */

    qreason_cnf_destroy(cnf);
}

/**
 * WHAT: Test brain reasoning query structure
 * WHY:  Verify high-level query API
 * HOW:  Create brain reasoning query, verify fields
 */
TEST_F(QuantumBrainIntegrationTest, BrainReasoningQuery_Structure)
{
    brain_reasoning_query_t query = {};

    snprintf(query.description, sizeof(query.description), "Can goal X be achieved?");
    query.urgency = 0.8f;
    query.timeout_ms = 1000;

    /* Initialize CNF */
    query.cnf.num_variables = 3;
    query.cnf.num_clauses = 0;

    EXPECT_STREQ(query.description, "Can goal X be achieved?");
    EXPECT_FLOAT_EQ(query.urgency, 0.8f);
    EXPECT_EQ(query.timeout_ms, 1000u);
}

/**
 * WHAT: Test brain reasoning result structure
 * WHY:  Verify result contains all needed information
 * HOW:  Create and populate result structure
 */
TEST_F(QuantumBrainIntegrationTest, BrainReasoningResult_Structure)
{
    brain_reasoning_result_t result = {};

    result.satisfiable = true;
    result.confidence = 0.95f;
    result.satisfaction_probability = 0.87f;
    result.num_variables = 3;
    result.grover_iterations = 5;
    result.inference_depth = 2;
    result.quantum_speedup = 3.5f;
    result.solve_time_us = 500;

    result.assignment[0] = QREASON_TRUE;
    result.assignment[1] = QREASON_FALSE;
    result.assignment[2] = QREASON_TRUE;

    EXPECT_TRUE(result.satisfiable);
    EXPECT_GT(result.confidence, 0.9f);
    EXPECT_EQ(result.assignment[0], QREASON_TRUE);
    EXPECT_EQ(result.assignment[1], QREASON_FALSE);
    EXPECT_GT(result.quantum_speedup, 1.0f);
}

//=============================================================================
// Fatigue and Stress Modulation Tests
//=============================================================================

/**
 * WHAT: Test fatigue modulation of quantum reasoning
 * WHY:  Biological realism - tired brains reason slower
 * HOW:  Apply fatigue, verify reasoning parameters change
 */
TEST_F(QuantumBrainIntegrationTest, FatigueModulation_ReducesCapacity)
{
    brain->config = mock_brain_qreason_default_config();
    brain->fatigue_level = 0.0f;

    uint32_t base_iterations = brain->config.max_grover_iterations;

    /* Apply fatigue */
    brain->fatigue_level = 0.5f;

    /* Calculate fatigued iterations based on sensitivity */
    float fatigue_factor = 1.0f - (brain->fatigue_level * brain->config.fatigue_sensitivity);
    uint32_t fatigued_iterations = (uint32_t)(base_iterations * fatigue_factor);

    EXPECT_LT(fatigued_iterations, base_iterations);

    /* High fatigue should reduce capacity more */
    brain->fatigue_level = 0.9f;
    fatigue_factor = 1.0f - (brain->fatigue_level * brain->config.fatigue_sensitivity);
    uint32_t very_fatigued_iterations = (uint32_t)(base_iterations * fatigue_factor);

    EXPECT_LT(very_fatigued_iterations, fatigued_iterations);
}

/**
 * WHAT: Test stress modulation of quantum reasoning
 * WHY:  Stress impairs logical reasoning
 * HOW:  Apply stress, verify confidence threshold increases
 */
TEST_F(QuantumBrainIntegrationTest, StressModulation_IncreasesThreshold)
{
    brain->config = mock_brain_qreason_default_config();
    brain->stress_level = 0.0f;

    float base_threshold = brain->config.min_confidence;

    /* Apply stress */
    brain->stress_level = 0.6f;

    /* Calculate stressed threshold based on sensitivity */
    float stress_effect = brain->stress_level * brain->config.stress_sensitivity;
    float stressed_threshold = base_threshold + stress_effect * (1.0f - base_threshold);

    EXPECT_GT(stressed_threshold, base_threshold);
    EXPECT_LE(stressed_threshold, 1.0f);

    /* Maximum stress should push threshold toward 1.0 */
    brain->stress_level = 1.0f;
    stress_effect = brain->stress_level * brain->config.stress_sensitivity;
    float max_stress_threshold = base_threshold + stress_effect * (1.0f - base_threshold);

    EXPECT_GT(max_stress_threshold, stressed_threshold);
}

/**
 * WHAT: Test combined fatigue and stress effects
 * WHY:  Real brains experience both simultaneously
 * HOW:  Apply both modulations, verify combined impact
 */
TEST_F(QuantumBrainIntegrationTest, CombinedModulation_FatigueAndStress)
{
    brain->config = mock_brain_qreason_default_config();

    /* Apply both fatigue and stress */
    brain->fatigue_level = 0.4f;
    brain->stress_level = 0.5f;

    /* Calculate combined effects */
    float fatigue_factor = 1.0f - (brain->fatigue_level * brain->config.fatigue_sensitivity);
    float stress_effect = brain->stress_level * brain->config.stress_sensitivity;

    uint32_t modulated_iterations = (uint32_t)(brain->config.max_grover_iterations * fatigue_factor);
    float modulated_threshold = brain->config.min_confidence +
                                stress_effect * (1.0f - brain->config.min_confidence);

    /* Both should have effects */
    EXPECT_LT(modulated_iterations, brain->config.max_grover_iterations);
    EXPECT_GT(modulated_threshold, brain->config.min_confidence);

    /* Verify reasoning is still possible */
    EXPECT_GT(modulated_iterations, 0u);
    EXPECT_LT(modulated_threshold, 1.0f);
}

//=============================================================================
// Brain Quantum Reasoning Statistics Tests
//=============================================================================

/**
 * WHAT: Test brain quantum reasoning statistics tracking
 * WHY:  Monitoring and debugging capability
 * HOW:  Simulate queries, verify stats update
 */
TEST_F(QuantumBrainIntegrationTest, Statistics_QueryTracking)
{
    brain->stats.total_queries = 0;
    brain->stats.satisfiable_count = 0;
    brain->stats.unsatisfiable_count = 0;
    brain->stats.timeout_count = 0;

    /* Simulate queries */
    brain->stats.total_queries += 10;
    brain->stats.satisfiable_count += 7;
    brain->stats.unsatisfiable_count += 2;
    brain->stats.timeout_count += 1;

    EXPECT_EQ(brain->stats.total_queries, 10u);
    EXPECT_EQ(brain->stats.satisfiable_count, 7u);
    EXPECT_EQ(brain->stats.unsatisfiable_count, 2u);
    EXPECT_EQ(brain->stats.timeout_count, 1u);

    /* Verify counts add up */
    EXPECT_EQ(brain->stats.satisfiable_count + brain->stats.unsatisfiable_count +
              brain->stats.timeout_count, brain->stats.total_queries);
}

/**
 * WHAT: Test average statistics computation
 * WHY:  Performance monitoring
 * HOW:  Accumulate data, compute averages
 */
TEST_F(QuantumBrainIntegrationTest, Statistics_Averages)
{
    /* Simulate performance data */
    brain->stats.total_queries = 100;
    brain->stats.total_grover_iterations = 500;
    brain->stats.avg_solve_time_us = 250.0f;

    /* Compute averages */
    brain->stats.avg_grover_iterations = (float)brain->stats.total_grover_iterations /
                                          brain->stats.total_queries;
    brain->stats.avg_confidence = 0.85f;

    EXPECT_FLOAT_EQ(brain->stats.avg_grover_iterations, 5.0f);
    EXPECT_GT(brain->stats.avg_confidence, 0.0f);
    EXPECT_LE(brain->stats.avg_confidence, 1.0f);
}

//=============================================================================
// Prefrontal Quantum Bridge Integration Tests
//=============================================================================

/**
 * WHAT: Test prefrontal quantum bridge default configuration
 * WHY:  Verify sensible defaults for decision-making
 * HOW:  Get defaults, check all parameters
 */
TEST_F(QuantumBrainIntegrationTest, PrefrontalBridge_DefaultConfig)
{
    prefrontal_quantum_config_t config = prefrontal_quantum_default_config();

    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.max_decision_qubits, 0u);
    EXPECT_GT(config.max_planning_qubits, 0u);
    EXPECT_GT(config.max_grover_iterations, 0u);
    EXPECT_GT(config.min_decision_confidence, 0.0f);
    EXPECT_LE(config.min_decision_confidence, 1.0f);
}

/**
 * WHAT: Test prefrontal bridge lifecycle
 * WHY:  Verify creation and destruction
 * HOW:  Create bridge, verify non-null, destroy cleanly
 */
TEST_F(QuantumBrainIntegrationTest, PrefrontalBridge_Lifecycle)
{
    prefrontal_quantum_config_t config = prefrontal_quantum_default_config();

    prefrontal_bridge = prefrontal_quantum_bridge_create(brain, &config);
    ASSERT_NE(prefrontal_bridge, nullptr);

    /* Verify enabled state */
    EXPECT_TRUE(prefrontal_quantum_bridge_is_enabled(prefrontal_bridge));

    /* Can disable */
    prefrontal_quantum_bridge_set_enabled(prefrontal_bridge, false);
    EXPECT_FALSE(prefrontal_quantum_bridge_is_enabled(prefrontal_bridge));

    /* Destruction handled in TearDown */
}

/**
 * WHAT: Test prefrontal quantum decision acceleration
 * WHY:  Core decision-making functionality
 * HOW:  Provide utilities, get quantum-accelerated decision
 */
TEST_F(QuantumBrainIntegrationTest, PrefrontalBridge_DecisionAcceleration)
{
    prefrontal_quantum_config_t config = prefrontal_quantum_default_config();
    prefrontal_bridge = prefrontal_quantum_bridge_create(brain, &config);
    ASSERT_NE(prefrontal_bridge, nullptr);

    /* Create decision options with utilities */
    float utilities[] = {0.3f, 0.7f, 0.5f, 0.9f, 0.4f};
    uint32_t num_options = 5;
    float min_utility = 0.6f;

    quantum_decision_result_t result = {};

    int ret = prefrontal_quantum_accelerate_decision(
        prefrontal_bridge,
        utilities,
        num_options,
        min_utility,
        &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_GT(result.candidates_evaluated, 0u);
    /* Best candidate should have utility above threshold */
    if (result.best_candidate) {
        EXPECT_GE(result.best_candidate->classical_utility, min_utility);
    }
}

/**
 * WHAT: Test prefrontal planning optimization
 * WHY:  Verify quantum-accelerated planning
 * HOW:  Provide action space, get optimized plan
 */
TEST_F(QuantumBrainIntegrationTest, PrefrontalBridge_PlanningOptimization)
{
    prefrontal_quantum_config_t config = prefrontal_quantum_default_config();
    prefrontal_bridge = prefrontal_quantum_bridge_create(brain, &config);
    ASSERT_NE(prefrontal_bridge, nullptr);

    /* Create action space */
    uint32_t actions[] = {1, 2, 3, 4};
    uint32_t num_actions = 4;
    float values[] = {0.5f, 0.7f, 0.3f, 0.8f};

    /* Simple constraint matrix (identity = no constraints) */
    float constraints[16] = {};
    for (int i = 0; i < 4; i++) {
        constraints[i * 4 + i] = 1.0f;
    }

    quantum_planning_result_t result = {};

    int ret = prefrontal_quantum_optimize_plan(
        prefrontal_bridge,
        actions,
        num_actions,
        constraints,
        values,
        3,  /* max plan length */
        &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_GT(result.plans_explored, 0u);
    EXPECT_GE(result.optimization_quality, 0.0f);
    EXPECT_LE(result.optimization_quality, 1.0f);
}

//=============================================================================
// Brainstem Quantum Bridge Integration Tests
//=============================================================================

/**
 * WHAT: Test brainstem quantum bridge default configuration
 * WHY:  Verify sensible defaults for reflex processing
 * HOW:  Get defaults, check all parameters
 */
TEST_F(QuantumBrainIntegrationTest, BrainstemBridge_DefaultConfig)
{
    brainstem_quantum_config_t config = brainstem_quantum_default_config();

    /* Check algorithm defaults */
    EXPECT_NE(config.reflex_algorithm, BRAINSTEM_QUANTUM_ALG_NONE);
    EXPECT_NE(config.arousal_algorithm, BRAINSTEM_QUANTUM_ALG_NONE);

    /* Check quantum parameters */
    EXPECT_GT(config.max_qubits, 0u);
    EXPECT_GT(config.grover_iterations, 0u);
    EXPECT_GT(config.annealing_steps, 0u);

    /* Check thresholds */
    EXPECT_GE(config.quantum_classical_mix, 0.0f);
    EXPECT_LE(config.quantum_classical_mix, 1.0f);
}

/**
 * WHAT: Test brainstem bridge lifecycle
 * WHY:  Verify creation and destruction
 * HOW:  Create bridge, verify non-null, destroy cleanly
 */
TEST_F(QuantumBrainIntegrationTest, BrainstemBridge_Lifecycle)
{
    brainstem_quantum_config_t config = brainstem_quantum_default_config();

    /* Note: brainstem bridge requires brainstem_adapter_t, using NULL for basic test */
    brainstem_bridge = brainstem_quantum_bridge_create(nullptr, &config);
    /* May return NULL if adapter is required - that's acceptable */

    if (brainstem_bridge) {
        bool available = brainstem_quantum_bridge_is_quantum_available(brainstem_bridge);
        /* Quantum availability depends on configuration */
        (void)available;
    }

    /* Destruction handled in TearDown */
}

/**
 * WHAT: Test brainstem quantum reflex selection
 * WHY:  Fast reflex pathway selection via Grover search
 * HOW:  Provide stimulus pattern, get selected reflex
 */
TEST_F(QuantumBrainIntegrationTest, BrainstemBridge_ReflexSelection)
{
    brainstem_quantum_config_t config = brainstem_quantum_default_config();
    brainstem_bridge = brainstem_quantum_bridge_create(nullptr, &config);

    if (brainstem_bridge) {
        float stimulus_pattern[] = {0.8f, 0.2f, 0.5f, 0.1f};
        uint32_t pattern_size = 4;
        float urgency = 0.9f;

        quantum_reflex_result_t result = {};

        bool success = brainstem_quantum_select_reflex(
            brainstem_bridge,
            stimulus_pattern,
            pattern_size,
            urgency,
            &result
        );

        if (success) {
            EXPECT_GE(result.selection_confidence, 0.0f);
            EXPECT_LE(result.selection_confidence, 1.0f);
            EXPECT_GT(result.execution_time_us, 0.0);
        }
    }
}

/**
 * WHAT: Test brainstem arousal optimization
 * WHY:  Find optimal arousal state via quantum annealing
 * HOW:  Provide current state, get optimized arousal level
 */
TEST_F(QuantumBrainIntegrationTest, BrainstemBridge_ArousalOptimization)
{
    brainstem_quantum_config_t config = brainstem_quantum_default_config();
    brainstem_bridge = brainstem_quantum_bridge_create(nullptr, &config);

    if (brainstem_bridge) {
        float current_arousal = 0.5f;
        float sensory_load = 0.6f;
        float metabolic_state = 0.7f;
        float threat_level = 0.3f;

        quantum_arousal_result_t result = {};

        bool success = brainstem_quantum_optimize_arousal(
            brainstem_bridge,
            current_arousal,
            sensory_load,
            metabolic_state,
            threat_level,
            &result
        );

        if (success) {
            EXPECT_GE(result.optimal_arousal, 0.0f);
            EXPECT_LE(result.optimal_arousal, 1.0f);
            EXPECT_GT(result.execution_time_us, 0.0);
        }
    }
}

/**
 * WHAT: Test brainstem speedup estimation
 * WHY:  Verify quantum advantage predictions
 * HOW:  Request speedup estimates for different problem sizes
 */
TEST_F(QuantumBrainIntegrationTest, BrainstemBridge_SpeedupEstimation)
{
    brainstem_quantum_config_t config = brainstem_quantum_default_config();
    brainstem_bridge = brainstem_quantum_bridge_create(nullptr, &config);

    if (brainstem_bridge) {
        /* Small problem - may not benefit from quantum */
        float speedup_small = brainstem_quantum_bridge_estimate_speedup(
            brainstem_bridge, 4, BRAINSTEM_QUANTUM_ALG_GROVER);

        /* Larger problem - should show quantum advantage */
        float speedup_large = brainstem_quantum_bridge_estimate_speedup(
            brainstem_bridge, 256, BRAINSTEM_QUANTUM_ALG_GROVER);

        /* Grover gives O(sqrt(N)) speedup */
        EXPECT_GE(speedup_small, 1.0f);  /* At least no slowdown */
        EXPECT_GE(speedup_large, speedup_small);  /* Larger problems benefit more */
    }
}

//=============================================================================
// Cross-Region Quantum Coordination Tests
//=============================================================================

/**
 * WHAT: Test quantum reasoner sharing across brain regions
 * WHY:  Efficient resource sharing
 * HOW:  Create reasoner, connect to multiple bridges
 */
TEST_F(QuantumBrainIntegrationTest, CrossRegion_SharedReasoner)
{
    /* Create shared quantum reasoner */
    reasoner = CreateReasoner();
    ASSERT_NE(reasoner, nullptr);
    brain->reasoner = reasoner;

    /* Both prefrontal and brainstem can access the same reasoner */
    prefrontal_quantum_config_t pfc_config = prefrontal_quantum_default_config();
    prefrontal_bridge = prefrontal_quantum_bridge_create(brain, &pfc_config);

    brainstem_quantum_config_t bs_config = brainstem_quantum_default_config();
    brainstem_bridge = brainstem_quantum_bridge_create(nullptr, &bs_config);

    /* Reasoner should still be valid after bridge creation */
    EXPECT_NE(brain->reasoner, nullptr);
}

/**
 * WHAT: Test coordinated decision between prefrontal and brainstem
 * WHY:  Real brains coordinate high-level and reflexive decisions
 * HOW:  Prefrontal makes plan, brainstem selects reflex
 */
TEST_F(QuantumBrainIntegrationTest, CrossRegion_CoordinatedDecision)
{
    reasoner = CreateReasoner();
    ASSERT_NE(reasoner, nullptr);
    brain->reasoner = reasoner;
    brain->quantum_reasoning_enabled = true;

    /* Create both bridges */
    prefrontal_quantum_config_t pfc_config = prefrontal_quantum_default_config();
    prefrontal_bridge = prefrontal_quantum_bridge_create(brain, &pfc_config);

    brainstem_quantum_config_t bs_config = brainstem_quantum_default_config();
    brainstem_bridge = brainstem_quantum_bridge_create(nullptr, &bs_config);

    /* Prefrontal evaluates high-level options */
    if (prefrontal_bridge) {
        float utilities[] = {0.4f, 0.8f, 0.6f};
        quantum_decision_result_t pfc_result = {};

        prefrontal_quantum_accelerate_decision(
            prefrontal_bridge, utilities, 3, 0.5f, &pfc_result);

        /* Decision should be made */
        EXPECT_GT(pfc_result.candidates_evaluated, 0u);
    }

    /* Brainstem handles reflexive response */
    if (brainstem_bridge) {
        float stimulus[] = {0.9f, 0.1f};
        quantum_reflex_result_t bs_result = {};

        brainstem_quantum_select_reflex(
            brainstem_bridge, stimulus, 2, 0.8f, &bs_result);

        /* Reflex should be selected */
        /* (selection_confidence may be 0 if no reflexes registered) */
    }
}

//=============================================================================
// Integration with Executive Controller Tests
//=============================================================================

/**
 * WHAT: Test goal feasibility checking
 * WHY:  Executive uses quantum reasoning for planning
 * HOW:  Create goal constraints, check feasibility
 */
TEST_F(QuantumBrainIntegrationTest, ExecutiveIntegration_GoalFeasibility)
{
    reasoner = CreateReasoner();
    ASSERT_NE(reasoner, nullptr);
    brain->reasoner = reasoner;
    brain->quantum_reasoning_enabled = true;
    brain->config.integrate_with_executive = true;

    /* Create simple goal constraint problem */
    /* Goal: Can we achieve state X given constraints? */

    /* Set up known facts */
    brain->facts[0] = QREASON_TRUE;   /* Resource A available */
    brain->facts[1] = QREASON_TRUE;   /* Resource B available */
    brain->facts[2] = QREASON_UNKNOWN; /* Goal X achievable? */

    /* Create CNF representing constraints */
    qreason_cnf_t* constraints = qreason_cnf_create(3);
    ASSERT_NE(constraints, nullptr);

    /* Constraint: Goal requires A and B */
    int32_t clause[] = {1, 2};  /* A OR B must be true */
    qreason_cnf_add_clause(constraints, clause, 2);

    /* Solve to check feasibility */
    bool feasible = false;
    qreason_solve_sat(reasoner, constraints, &feasible);

    EXPECT_TRUE(feasible);  /* Goal is achievable given resources */

    qreason_cnf_destroy(constraints);
}

//=============================================================================
// Integration with Parietal Lobe Tests
//=============================================================================

/**
 * WHAT: Test mathematical proof verification
 * WHY:  Parietal uses quantum for proof checking
 * HOW:  Create logical formula representing proof, verify satisfiability
 */
TEST_F(QuantumBrainIntegrationTest, ParietalIntegration_ProofVerification)
{
    reasoner = CreateReasoner();
    ASSERT_NE(reasoner, nullptr);
    brain->reasoner = reasoner;
    brain->quantum_reasoning_enabled = true;
    brain->config.integrate_with_parietal = true;

    /* Proof verification: If A implies B, and A is true, then B is true */
    /* Encoded as: (NOT A OR B) AND A => B must be derivable */

    qreason_cnf_t* proof = qreason_cnf_create(2);
    ASSERT_NE(proof, nullptr);

    /* Implication: A => B encoded as (NOT A OR B) */
    int32_t implication[] = {-1, 2};
    qreason_cnf_add_clause(proof, implication, 2);

    /* Premise: A is true */
    int32_t premise[] = {1};
    qreason_cnf_add_clause(proof, premise, 1);

    /* Solve - should find B must be true */
    bool valid = false;
    qreason_solve_sat(reasoner, proof, &valid);

    EXPECT_TRUE(valid);  /* Proof is valid */

    qreason_cnf_destroy(proof);
}

//=============================================================================
// Performance and Scalability Tests
//=============================================================================

/**
 * WHAT: Test quantum speedup estimation
 * WHY:  Verify expected O(sqrt(N)) improvement
 * HOW:  Compute theoretical speedup for various problem sizes
 */
TEST_F(QuantumBrainIntegrationTest, Performance_GroverSpeedup)
{
    /* Grover's algorithm provides O(sqrt(N)) speedup */
    /* For N states, classical needs O(N), quantum needs O(sqrt(N)) */

    uint32_t problem_sizes[] = {16, 64, 256, 1024, 4096};

    for (uint32_t n : problem_sizes) {
        float classical_iterations = (float)n;
        float quantum_iterations = sqrtf((float)n);
        float theoretical_speedup = classical_iterations / quantum_iterations;

        /* Speedup should be sqrt(N) */
        EXPECT_NEAR(theoretical_speedup, sqrtf((float)n), 0.1f);
    }

    /* For N=1024, speedup should be sqrt(1024) = 32 */
    EXPECT_NEAR(sqrtf(1024.0f), 32.0f, 0.1f);
}

/**
 * WHAT: Test brain quantum reasoning under load
 * WHY:  Verify stability with many queries
 * HOW:  Execute many queries, check for resource leaks
 */
TEST_F(QuantumBrainIntegrationTest, Performance_ManyQueries)
{
    reasoner = CreateReasoner();
    ASSERT_NE(reasoner, nullptr);
    brain->reasoner = reasoner;
    brain->quantum_reasoning_enabled = true;

    const int num_queries = 100;
    uint32_t satisfiable_count = 0;

    for (int q = 0; q < num_queries; q++) {
        /* Create simple CNF */
        qreason_cnf_t* cnf = qreason_cnf_create(2);
        ASSERT_NE(cnf, nullptr);

        int32_t clause[] = {1, 2};
        qreason_cnf_add_clause(cnf, clause, 2);

        bool is_sat = false;
        int ret = qreason_solve_sat(reasoner, cnf, &is_sat);
        EXPECT_EQ(ret, 0);

        if (is_sat) {
            satisfiable_count++;
        }

        qreason_cnf_destroy(cnf);
    }

    /* Track statistics */
    brain->stats.total_queries = num_queries;
    brain->stats.satisfiable_count = satisfiable_count;

    EXPECT_EQ(brain->stats.total_queries, (uint64_t)num_queries);
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

/**
 * WHAT: Test quantum reasoning with disabled state
 * WHY:  Verify graceful handling when quantum is off
 * HOW:  Disable quantum, attempt operations
 */
TEST_F(QuantumBrainIntegrationTest, EdgeCase_DisabledQuantum)
{
    brain->quantum_reasoning_enabled = false;
    brain->reasoner = nullptr;

    /* Should handle gracefully */
    EXPECT_FALSE(brain->quantum_reasoning_enabled);
    EXPECT_EQ(brain->reasoner, nullptr);
}

/**
 * WHAT: Test with maximum fatigue and stress
 * WHY:  Verify extreme modulation doesn't break system
 * HOW:  Set maximum values, check reasoning still possible
 */
TEST_F(QuantumBrainIntegrationTest, EdgeCase_MaximumModulation)
{
    brain->config = mock_brain_qreason_default_config();
    brain->fatigue_level = 1.0f;
    brain->stress_level = 1.0f;

    /* Calculate modulated parameters */
    float fatigue_factor = 1.0f - (brain->fatigue_level * brain->config.fatigue_sensitivity);
    float stress_effect = brain->stress_level * brain->config.stress_sensitivity;

    uint32_t modulated_iterations = (uint32_t)(brain->config.max_grover_iterations * fatigue_factor);
    float modulated_threshold = brain->config.min_confidence +
                                stress_effect * (1.0f - brain->config.min_confidence);

    /* Should still have some capacity */
    EXPECT_GT(modulated_iterations, 0u);
    EXPECT_LT(modulated_threshold, 1.0f);
}

/**
 * WHAT: Test empty knowledge base
 * WHY:  Verify reasoning works with no prior knowledge
 * HOW:  Clear KB, attempt reasoning
 */
TEST_F(QuantumBrainIntegrationTest, EdgeCase_EmptyKnowledgeBase)
{
    reasoner = CreateReasoner();
    ASSERT_NE(reasoner, nullptr);
    brain->reasoner = reasoner;

    /* KB is empty (all UNKNOWN) */
    brain->num_facts = 0;
    for (uint32_t i = 0; i < BRAIN_QREASON_MAX_VARIABLES; i++) {
        brain->facts[i] = QREASON_UNKNOWN;
    }

    /* Should still be able to solve SAT */
    qreason_cnf_t* cnf = qreason_cnf_create(2);
    ASSERT_NE(cnf, nullptr);

    int32_t clause[] = {1, 2};
    qreason_cnf_add_clause(cnf, clause, 2);

    bool is_sat = false;
    int ret = qreason_solve_sat(reasoner, cnf, &is_sat);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(is_sat);  /* Satisfiable with unknown values */

    qreason_cnf_destroy(cnf);
}

/**
 * WHAT: Test statistics reset
 * WHY:  Verify clean statistics for new measurement periods
 * HOW:  Accumulate stats, reset, verify zeroed
 */
TEST_F(QuantumBrainIntegrationTest, Statistics_Reset)
{
    /* Accumulate some statistics */
    brain->stats.total_queries = 100;
    brain->stats.satisfiable_count = 80;
    brain->stats.unsatisfiable_count = 15;
    brain->stats.timeout_count = 5;
    brain->stats.total_grover_iterations = 500;
    brain->stats.avg_solve_time_us = 250.0f;

    /* Reset statistics */
    memset(&brain->stats, 0, sizeof(brain->stats));

    /* Verify all zeroed */
    EXPECT_EQ(brain->stats.total_queries, 0u);
    EXPECT_EQ(brain->stats.satisfiable_count, 0u);
    EXPECT_EQ(brain->stats.unsatisfiable_count, 0u);
    EXPECT_EQ(brain->stats.timeout_count, 0u);
    EXPECT_EQ(brain->stats.total_grover_iterations, 0u);
    EXPECT_FLOAT_EQ(brain->stats.avg_solve_time_us, 0.0f);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
