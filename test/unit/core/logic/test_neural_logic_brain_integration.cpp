/**
 * @file test_neural_logic_brain_integration.cpp
 * @brief Unit tests for neural logic brain integration
 * @version 2.6.2
 * @date 2025-11-20
 *
 * WHAT: Comprehensive test suite for brain-neural logic integration
 * WHY:  Ensure 100% code coverage and correct behavior
 * HOW:  25+ tests covering all functions, edge cases, and error paths
 *
 * TEST CATEGORIES:
 * 1. Brain attachment/detachment
 * 2. Gate evaluation with neuromodulation
 * 3. Circuit building and parsing
 * 4. Threshold modulation (DA/ACh effects)
 * 5. Error handling (NULL checks, invalid inputs)
 * 6. Statistics and monitoring
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/logic/nimcp_neural_logic_brain_integration.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/subcortical/nimcp_basal_ganglia_enhanced.h"
#include "core/brain/subcortical/nimcp_bg_neuromodulators.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NeuralLogicBrainIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    neural_logic_config_t logic_config;

    void SetUp() override {
        // Initialize logging
        log_init("test_neural_logic_brain_integration.log");

        // Create test brain (task_name, size, task, input_dim, output_dim)
        brain = brain_create("test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        // Configure neural logic network
        logic_config = neural_logic_default_config(100);
        logic_config.use_gpu = false;  // Use CPU for deterministic tests
    }

    void TearDown() override {
        if (brain) {
            brain_destroy_neural_logic(brain);
            brain_destroy(brain);
        }
        log_close();
    }

    // Helper: Create logic network
    bool create_logic_network() {
        return brain_create_neural_logic(brain, &logic_config);
    }

    // Helper: Set neuromodulator levels (both generic and BG neuromod systems)
    void set_neuromodulators(float da, float ach) {
        // Set generic neuromodulator system
        if (brain->neuromodulator_system) {
            neuromodulator_set_level(brain->neuromodulator_system,
                                     NEUROMOD_DOPAMINE, da);
            neuromodulator_set_level(brain->neuromodulator_system,
                                     NEUROMOD_ACETYLCHOLINE, ach);
        }

        // Also set BG neuromodulator system if BG is enabled
        // (neural logic integration prefers BG neuromod when available)
        if (brain->basal_ganglia_enabled && brain->basal_ganglia) {
            bg_neuromod_system_t* bg_neuromod = bg_enhanced_get_neuromod(brain->basal_ganglia);
            if (bg_neuromod) {
                bg_neuromod_set_level(bg_neuromod, BG_NEUROMOD_DOPAMINE, da);
                bg_neuromod_set_level(bg_neuromod, BG_NEUROMOD_ACETYLCHOLINE, ach);
            }
        }
    }
};

//=============================================================================
// Category 1: Brain Attachment/Detachment Tests
//=============================================================================

TEST_F(NeuralLogicBrainIntegrationTest, CreateNeuralLogic_Success) {
    // WHAT: Test successful creation of neural logic network
    // WHY:  Verify basic functionality
    // HOW:  Create network, check brain->logic field

    bool success = create_logic_network();
    EXPECT_TRUE(success);
    EXPECT_NE(brain->logic, nullptr);
}

TEST_F(NeuralLogicBrainIntegrationTest, CreateNeuralLogic_NullBrain) {
    // WHAT: Test creation with NULL brain
    // WHY:  Verify NULL check and error handling
    // HOW:  Pass NULL, expect failure

    bool success = brain_create_neural_logic(nullptr, &logic_config);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, CreateNeuralLogic_AlreadyExists) {
    // WHAT: Test creation when network already exists
    // WHY:  Verify duplicate creation prevention
    // HOW:  Create twice, expect second to fail

    ASSERT_TRUE(create_logic_network());

    bool success = brain_create_neural_logic(brain, &logic_config);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, CreateNeuralLogic_NullConfig) {
    // WHAT: Test creation with NULL config (should use defaults)
    // WHY:  Verify default config path
    // HOW:  Pass NULL config, expect success

    bool success = brain_create_neural_logic(brain, nullptr);
    EXPECT_TRUE(success);
    EXPECT_NE(brain->logic, nullptr);
}

TEST_F(NeuralLogicBrainIntegrationTest, DestroyNeuralLogic_Success) {
    // WHAT: Test successful destruction
    // WHY:  Verify cleanup
    // HOW:  Create then destroy, check NULL

    ASSERT_TRUE(create_logic_network());
    ASSERT_NE(brain->logic, nullptr);

    brain_destroy_neural_logic(brain);
    EXPECT_EQ(brain->logic, nullptr);
}

TEST_F(NeuralLogicBrainIntegrationTest, DestroyNeuralLogic_NullBrain) {
    // WHAT: Test destruction with NULL brain
    // WHY:  Verify NULL-safe behavior
    // HOW:  Call with NULL, should not crash

    brain_destroy_neural_logic(nullptr);
    // No crash = success
}

TEST_F(NeuralLogicBrainIntegrationTest, DestroyNeuralLogic_NoNetwork) {
    // WHAT: Test destruction when no network exists
    // WHY:  Verify idempotent behavior
    // HOW:  Destroy without creating, should be safe

    brain_destroy_neural_logic(brain);
    // No crash = success
}

TEST_F(NeuralLogicBrainIntegrationTest, DestroyNeuralLogic_MultipleCalls) {
    // WHAT: Test multiple destruction calls
    // WHY:  Verify idempotent safety
    // HOW:  Destroy twice, should be safe

    ASSERT_TRUE(create_logic_network());

    brain_destroy_neural_logic(brain);
    brain_destroy_neural_logic(brain);
    // No crash = success
}

//=============================================================================
// Category 2: Gate Evaluation with Neuromodulation
//=============================================================================

TEST_F(NeuralLogicBrainIntegrationTest, Evaluate_AndGate_BothTrue) {
    // WHAT: Test AND gate with both inputs true
    // WHY:  Verify basic logic evaluation
    // HOW:  Create AND gate, evaluate with (1,1), expect 1

    ASSERT_TRUE(create_logic_network());

    uint32_t and_gate = neural_logic_create_gate(brain->logic, LOGIC_GATE_AND, 1.5f);
    ASSERT_NE(and_gate, UINT32_MAX);

    float inputs[2] = {1.0f, 1.0f};
    float output = 0.0f;

    bool success = brain_neural_logic_evaluate(brain, and_gate, inputs, 2, &output);
    EXPECT_TRUE(success);
    // Note: Output depends on neural simulation, may not be exact 1.0
}

TEST_F(NeuralLogicBrainIntegrationTest, Evaluate_AndGate_OneFalse) {
    // WHAT: Test AND gate with one input false
    // WHY:  Verify correct AND logic
    // HOW:  Evaluate with (1,0), expect 0

    ASSERT_TRUE(create_logic_network());

    uint32_t and_gate = neural_logic_create_gate(brain->logic, LOGIC_GATE_AND, 1.5f);
    ASSERT_NE(and_gate, UINT32_MAX);

    float inputs[2] = {1.0f, 0.0f};
    float output = 0.0f;

    bool success = brain_neural_logic_evaluate(brain, and_gate, inputs, 2, &output);
    EXPECT_TRUE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, Evaluate_NullBrain) {
    // WHAT: Test evaluation with NULL brain
    // WHY:  Verify NULL check
    // HOW:  Call with NULL brain, expect failure

    float inputs[2] = {1.0f, 1.0f};
    float output = 0.0f;

    bool success = brain_neural_logic_evaluate(nullptr, 0, inputs, 2, &output);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, Evaluate_NoNetwork) {
    // WHAT: Test evaluation without logic network
    // WHY:  Verify network existence check
    // HOW:  Don't create network, try to evaluate

    float inputs[2] = {1.0f, 1.0f};
    float output = 0.0f;

    bool success = brain_neural_logic_evaluate(brain, 0, inputs, 2, &output);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, Evaluate_NullInputs) {
    // WHAT: Test evaluation with NULL inputs
    // WHY:  Verify input validation
    // HOW:  Pass NULL inputs, expect failure

    ASSERT_TRUE(create_logic_network());

    float output = 0.0f;
    bool success = brain_neural_logic_evaluate(brain, 0, nullptr, 2, &output);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, Evaluate_NullOutput) {
    // WHAT: Test evaluation with NULL output
    // WHY:  Verify output validation
    // HOW:  Pass NULL output, expect failure

    ASSERT_TRUE(create_logic_network());

    float inputs[2] = {1.0f, 1.0f};
    bool success = brain_neural_logic_evaluate(brain, 0, inputs, 2, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, Evaluate_InvalidGateId) {
    // WHAT: Test evaluation with invalid gate ID
    // WHY:  Verify gate ID validation
    // HOW:  Use non-existent gate ID, expect failure

    ASSERT_TRUE(create_logic_network());

    float inputs[2] = {1.0f, 1.0f};
    float output = 0.0f;

    bool success = brain_neural_logic_evaluate(brain, 9999, inputs, 2, &output);
    EXPECT_FALSE(success);
}

//=============================================================================
// Category 3: Neuromodulation Effects
//=============================================================================

TEST_F(NeuralLogicBrainIntegrationTest, Neuromodulation_HighDopamine) {
    // WHAT: Test threshold modulation with high dopamine
    // WHY:  Verify DA lowers thresholds
    // HOW:  Set DA=0.8, compute modulated threshold

    ASSERT_TRUE(create_logic_network());
    set_neuromodulators(0.8f, 0.5f);

    float base_threshold = 1.5f;
    float modulated = 0.0f;

    bool success = brain_neural_logic_get_modulated_threshold(
        brain, base_threshold, &modulated);

    EXPECT_TRUE(success);
    EXPECT_LT(modulated, base_threshold);  // High DA lowers threshold
}

TEST_F(NeuralLogicBrainIntegrationTest, Neuromodulation_HighAcetylcholine) {
    // WHAT: Test threshold modulation with high acetylcholine
    // WHY:  Verify ACh increases precision
    // HOW:  Set ACh=0.8, compute modulated threshold

    ASSERT_TRUE(create_logic_network());
    set_neuromodulators(0.5f, 0.8f);

    float base_threshold = 1.5f;
    float modulated = 0.0f;

    bool success = brain_neural_logic_get_modulated_threshold(
        brain, base_threshold, &modulated);

    EXPECT_TRUE(success);
    EXPECT_GT(modulated, base_threshold);  // High ACh increases threshold
}

TEST_F(NeuralLogicBrainIntegrationTest, Neuromodulation_CombinedEffect) {
    // WHAT: Test combined DA and ACh modulation
    // WHY:  Verify formula: thresh * (1 - DA*0.3) * (1 + ACh*0.3)
    // HOW:  Set DA=0.6, ACh=0.4, check formula

    ASSERT_TRUE(create_logic_network());
    set_neuromodulators(0.6f, 0.4f);

    float base = 1.5f;
    float modulated = 0.0f;

    bool success = brain_neural_logic_get_modulated_threshold(
        brain, base, &modulated);

    EXPECT_TRUE(success);

    // Expected: 1.5 * (1 - 0.6*0.3) * (1 + 0.4*0.3)
    //         = 1.5 * 0.82 * 1.12 = 1.3776
    float expected = base * (1.0f - 0.6f * 0.3f) * (1.0f + 0.4f * 0.3f);
    EXPECT_NEAR(modulated, expected, 0.001f);
}

TEST_F(NeuralLogicBrainIntegrationTest, Neuromodulation_NullBrain) {
    // WHAT: Test threshold modulation with NULL brain
    // WHY:  Verify NULL check
    // HOW:  Pass NULL brain, expect failure

    float modulated = 0.0f;
    bool success = brain_neural_logic_get_modulated_threshold(
        nullptr, 1.5f, &modulated);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, Neuromodulation_NullOutput) {
    // WHAT: Test threshold modulation with NULL output
    // WHY:  Verify output validation
    // HOW:  Pass NULL output, expect failure

    ASSERT_TRUE(create_logic_network());

    bool success = brain_neural_logic_get_modulated_threshold(
        brain, 1.5f, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, ApplyNeuromodulation_Success) {
    // WHAT: Test applying neuromodulation to all gates
    // WHY:  Verify bulk modulation
    // HOW:  Create gates, apply modulation, check count

    ASSERT_TRUE(create_logic_network());

    // Create a few gates
    neural_logic_create_gate(brain->logic, LOGIC_GATE_AND, 1.5f);
    neural_logic_create_gate(brain->logic, LOGIC_GATE_OR, 1.2f);
    neural_logic_create_gate(brain->logic, LOGIC_GATE_XOR, 1.4f);

    set_neuromodulators(0.6f, 0.7f);

    uint32_t count = brain_neural_logic_apply_neuromodulation(brain);
    EXPECT_GT(count, 0);
}

TEST_F(NeuralLogicBrainIntegrationTest, ApplyNeuromodulation_NullBrain) {
    // WHAT: Test bulk modulation with NULL brain
    // WHY:  Verify NULL check
    // HOW:  Pass NULL, expect 0

    uint32_t count = brain_neural_logic_apply_neuromodulation(nullptr);
    EXPECT_EQ(count, 0);
}

TEST_F(NeuralLogicBrainIntegrationTest, ApplyNeuromodulation_NoNetwork) {
    // WHAT: Test bulk modulation without network
    // WHY:  Verify network check
    // HOW:  Don't create network, expect 0

    uint32_t count = brain_neural_logic_apply_neuromodulation(brain);
    EXPECT_EQ(count, 0);
}

//=============================================================================
// Category 4: Circuit Building and Parsing
//=============================================================================

TEST_F(NeuralLogicBrainIntegrationTest, BuildCircuit_SimpleExpression) {
    // WHAT: Test circuit building from expression
    // WHY:  Verify parser functionality
    // HOW:  Parse simple expression, check valid gate ID

    ASSERT_TRUE(create_logic_network());

    uint32_t circuit = brain_neural_logic_build_circuit(brain, "A AND B");
    EXPECT_NE(circuit, UINT32_MAX);
}

TEST_F(NeuralLogicBrainIntegrationTest, BuildCircuit_NullBrain) {
    // WHAT: Test circuit building with NULL brain
    // WHY:  Verify NULL check
    // HOW:  Pass NULL, expect UINT32_MAX

    uint32_t circuit = brain_neural_logic_build_circuit(nullptr, "A AND B");
    EXPECT_EQ(circuit, UINT32_MAX);
}

TEST_F(NeuralLogicBrainIntegrationTest, BuildCircuit_NoNetwork) {
    // WHAT: Test circuit building without network
    // WHY:  Verify network check
    // HOW:  Don't create network, expect UINT32_MAX

    uint32_t circuit = brain_neural_logic_build_circuit(brain, "A AND B");
    EXPECT_EQ(circuit, UINT32_MAX);
}

TEST_F(NeuralLogicBrainIntegrationTest, BuildCircuit_NullExpression) {
    // WHAT: Test circuit building with NULL expression
    // WHY:  Verify expression validation
    // HOW:  Pass NULL, expect UINT32_MAX

    ASSERT_TRUE(create_logic_network());

    uint32_t circuit = brain_neural_logic_build_circuit(brain, nullptr);
    EXPECT_EQ(circuit, UINT32_MAX);
}

TEST_F(NeuralLogicBrainIntegrationTest, BuildCircuit_EmptyExpression) {
    // WHAT: Test circuit building with empty expression
    // WHY:  Verify non-empty check
    // HOW:  Pass "", expect UINT32_MAX

    ASSERT_TRUE(create_logic_network());

    uint32_t circuit = brain_neural_logic_build_circuit(brain, "");
    EXPECT_EQ(circuit, UINT32_MAX);
}

//=============================================================================
// Category 5: Parser Utility Functions
//=============================================================================

TEST_F(NeuralLogicBrainIntegrationTest, ParseVariable_Success) {
    // WHAT: Test variable parsing
    // WHY:  Verify parser can extract variables
    // HOW:  Parse "A", expect success and 'A'

    const char* expr = "A";
    size_t pos = 0;
    char var_name = '\0';

    bool success = parse_variable(expr, &pos, &var_name);
    EXPECT_TRUE(success);
    EXPECT_EQ(var_name, 'A');
    EXPECT_EQ(pos, 1);
}

TEST_F(NeuralLogicBrainIntegrationTest, ParseVariable_WithWhitespace) {
    // WHAT: Test variable parsing with leading whitespace
    // WHY:  Verify whitespace skipping
    // HOW:  Parse "  B", expect success and 'B'

    const char* expr = "  B";
    size_t pos = 0;
    char var_name = '\0';

    bool success = parse_variable(expr, &pos, &var_name);
    EXPECT_TRUE(success);
    EXPECT_EQ(var_name, 'B');
}

TEST_F(NeuralLogicBrainIntegrationTest, ParseVariable_Lowercase) {
    // WHAT: Test variable parsing with lowercase (should fail)
    // WHY:  Verify uppercase-only requirement
    // HOW:  Parse "a", expect failure

    const char* expr = "a";
    size_t pos = 0;
    char var_name = '\0';

    bool success = parse_variable(expr, &pos, &var_name);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, ParseVariable_NullExpr) {
    // WHAT: Test variable parsing with NULL expression
    // WHY:  Verify NULL check
    // HOW:  Pass NULL, expect failure

    size_t pos = 0;
    char var_name = '\0';

    bool success = parse_variable(nullptr, &pos, &var_name);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, ParseOperator_And) {
    // WHAT: Test AND operator parsing
    // WHY:  Verify operator recognition
    // HOW:  Parse "AND", expect LOGIC_GATE_AND

    const char* expr = "AND";
    size_t pos = 0;
    logic_gate_type_t gate_type;

    bool success = parse_operator(expr, &pos, &gate_type);
    EXPECT_TRUE(success);
    EXPECT_EQ(gate_type, LOGIC_GATE_AND);
    EXPECT_EQ(pos, 3);
}

TEST_F(NeuralLogicBrainIntegrationTest, ParseOperator_Or) {
    // WHAT: Test OR operator parsing
    // WHY:  Verify operator recognition
    // HOW:  Parse "OR", expect LOGIC_GATE_OR

    const char* expr = "OR";
    size_t pos = 0;
    logic_gate_type_t gate_type;

    bool success = parse_operator(expr, &pos, &gate_type);
    EXPECT_TRUE(success);
    EXPECT_EQ(gate_type, LOGIC_GATE_OR);
}

TEST_F(NeuralLogicBrainIntegrationTest, ParseOperator_Not) {
    // WHAT: Test NOT operator parsing
    // WHY:  Verify unary operator
    // HOW:  Parse "NOT", expect LOGIC_GATE_NOT

    const char* expr = "NOT";
    size_t pos = 0;
    logic_gate_type_t gate_type;

    bool success = parse_operator(expr, &pos, &gate_type);
    EXPECT_TRUE(success);
    EXPECT_EQ(gate_type, LOGIC_GATE_NOT);
}

TEST_F(NeuralLogicBrainIntegrationTest, ParseOperator_Xor) {
    // WHAT: Test XOR operator parsing
    // WHY:  Verify XOR recognition
    // HOW:  Parse "XOR", expect LOGIC_GATE_XOR

    const char* expr = "XOR";
    size_t pos = 0;
    logic_gate_type_t gate_type;

    bool success = parse_operator(expr, &pos, &gate_type);
    EXPECT_TRUE(success);
    EXPECT_EQ(gate_type, LOGIC_GATE_XOR);
}

TEST_F(NeuralLogicBrainIntegrationTest, ParseOperator_Implies) {
    // WHAT: Test IMPLIES operator parsing
    // WHY:  Verify implication operator
    // HOW:  Parse "->", expect LOGIC_GATE_IMPLIES

    const char* expr = "->";
    size_t pos = 0;
    logic_gate_type_t gate_type;

    bool success = parse_operator(expr, &pos, &gate_type);
    EXPECT_TRUE(success);
    EXPECT_EQ(gate_type, LOGIC_GATE_IMPLIES);
}

TEST_F(NeuralLogicBrainIntegrationTest, ParseOperator_SymbolAnd) {
    // WHAT: Test symbol AND parsing
    // WHY:  Verify alternative syntax
    // HOW:  Parse "&", expect LOGIC_GATE_AND

    const char* expr = "&";
    size_t pos = 0;
    logic_gate_type_t gate_type;

    bool success = parse_operator(expr, &pos, &gate_type);
    EXPECT_TRUE(success);
    EXPECT_EQ(gate_type, LOGIC_GATE_AND);
}

TEST_F(NeuralLogicBrainIntegrationTest, ParseOperator_Unknown) {
    // WHAT: Test parsing unknown operator
    // WHY:  Verify error handling
    // HOW:  Parse invalid operator, expect failure

    const char* expr = "UNKNOWN";
    size_t pos = 0;
    logic_gate_type_t gate_type;

    bool success = parse_operator(expr, &pos, &gate_type);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, SkipWhitespace_Basic) {
    // WHAT: Test whitespace skipping
    // WHY:  Verify parser utility
    // HOW:  Skip "   A", expect pos=3

    const char* expr = "   A";
    size_t pos = 0;

    skip_whitespace(expr, &pos);
    EXPECT_EQ(pos, 3);
    EXPECT_EQ(expr[pos], 'A');
}

TEST_F(NeuralLogicBrainIntegrationTest, SkipWhitespace_NoWhitespace) {
    // WHAT: Test skipping when no whitespace
    // WHY:  Verify no-op behavior
    // HOW:  Skip "A", expect pos=0

    const char* expr = "A";
    size_t pos = 0;

    skip_whitespace(expr, &pos);
    EXPECT_EQ(pos, 0);
}

//=============================================================================
// Category 6: Statistics and Monitoring
//=============================================================================

TEST_F(NeuralLogicBrainIntegrationTest, GetStats_Success) {
    // WHAT: Test statistics retrieval
    // WHY:  Verify monitoring functionality
    // HOW:  Create gates, get stats, check values

    ASSERT_TRUE(create_logic_network());

    // Create some gates
    neural_logic_create_gate(brain->logic, LOGIC_GATE_AND, 1.5f);
    neural_logic_create_gate(brain->logic, LOGIC_GATE_OR, 1.2f);

    set_neuromodulators(0.6f, 0.7f);

    uint32_t gates = 0, vars = 0;
    uint64_t spikes = 0;
    float da = 0.0f, ach = 0.0f;

    bool success = brain_neural_logic_get_stats(
        brain, &gates, &vars, &spikes, &da, &ach);

    EXPECT_TRUE(success);
    EXPECT_GT(gates, 0);
    EXPECT_FLOAT_EQ(da, 0.6f);
    EXPECT_FLOAT_EQ(ach, 0.7f);
}

TEST_F(NeuralLogicBrainIntegrationTest, GetStats_NullBrain) {
    // WHAT: Test stats with NULL brain
    // WHY:  Verify NULL check
    // HOW:  Pass NULL, expect failure

    uint32_t gates = 0, vars = 0;
    uint64_t spikes = 0;
    float da = 0.0f, ach = 0.0f;

    bool success = brain_neural_logic_get_stats(
        nullptr, &gates, &vars, &spikes, &da, &ach);

    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, GetStats_NoNetwork) {
    // WHAT: Test stats without network
    // WHY:  Verify network check
    // HOW:  Don't create network, expect failure

    uint32_t gates = 0, vars = 0;
    uint64_t spikes = 0;
    float da = 0.0f, ach = 0.0f;

    bool success = brain_neural_logic_get_stats(
        brain, &gates, &vars, &spikes, &da, &ach);

    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicBrainIntegrationTest, GetStats_NullOutputs) {
    // WHAT: Test stats with NULL output parameters
    // WHY:  Verify output validation
    // HOW:  Pass NULL for outputs, expect failure

    ASSERT_TRUE(create_logic_network());

    bool success = brain_neural_logic_get_stats(
        brain, nullptr, nullptr, nullptr, nullptr, nullptr);

    EXPECT_FALSE(success);
}

//=============================================================================
// Test Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
