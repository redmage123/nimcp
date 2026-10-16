/**
 * @file test_quantum_superposition.cpp
 * @brief Comprehensive unit tests for quantum superposition operations
 *
 * Tests cover:
 * - State initialization and normalization
 * - Uniform superposition creation
 * - Custom superposition states
 * - Amplitude manipulation
 * - Probability computation
 * - State evolution
 * - Interference patterns
 * - Measurement operations
 *
 * @version Phase C2: Quantum Reasoning Integration
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumSuperpositionTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Nothing to set up */
    }

    void TearDown() override {
        /* Nothing to tear down */
    }

    /**
     * @brief Compute total probability from amplitudes
     */
    float compute_total_probability(const qreason_qstate_t* qstate) {
        float total = 0.0f;
        for (uint32_t i = 0; i < qstate->state_dim; i++) {
            total += qstate->amplitudes[i] * qstate->amplitudes[i];
        }
        return total;
    }

    /**
     * @brief Check if state is normalized
     */
    bool is_normalized(const qreason_qstate_t* qstate, float tolerance = 1e-4f) {
        float total = compute_total_probability(qstate);
        return fabsf(total - 1.0f) < tolerance;
    }
};

//=============================================================================
// State Initialization Tests
//=============================================================================

TEST_F(QuantumSuperpositionTest, InitUniformSmall) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 2);

    EXPECT_EQ(qstate.n_qubits, 2u);
    EXPECT_EQ(qstate.state_dim, 4u);

    float expected = 1.0f / sqrtf(4.0f);
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_NEAR(qstate.amplitudes[i], expected, 1e-6f);
    }
}

TEST_F(QuantumSuperpositionTest, InitUniformMedium) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 6);

    EXPECT_EQ(qstate.n_qubits, 6u);
    EXPECT_EQ(qstate.state_dim, 64u);

    float expected = 1.0f / sqrtf(64.0f);
    for (uint32_t i = 0; i < 64; i++) {
        EXPECT_NEAR(qstate.amplitudes[i], expected, 1e-6f);
    }
}

TEST_F(QuantumSuperpositionTest, InitUniformLarge) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 10);

    EXPECT_EQ(qstate.n_qubits, 10u);
    EXPECT_EQ(qstate.state_dim, 1024u);

    float expected = 1.0f / sqrtf(1024.0f);

    /* Check a sample of amplitudes */
    EXPECT_NEAR(qstate.amplitudes[0], expected, 1e-6f);
    EXPECT_NEAR(qstate.amplitudes[511], expected, 1e-6f);
    EXPECT_NEAR(qstate.amplitudes[1023], expected, 1e-6f);
}

TEST_F(QuantumSuperpositionTest, InitCapped) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 20);  /* Should cap at 16 */

    EXPECT_EQ(qstate.n_qubits, 16u);
    EXPECT_EQ(qstate.state_dim, 65536u);
}

TEST_F(QuantumSuperpositionTest, InitNormalized) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 5);

    EXPECT_TRUE(is_normalized(&qstate));
}

//=============================================================================
// Probability Tests
//=============================================================================

TEST_F(QuantumSuperpositionTest, ProbabilityUniform) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    /* Each state should have probability 1/8 */
    for (uint32_t i = 0; i < 8; i++) {
        float prob = qstate.amplitudes[i] * qstate.amplitudes[i];
        EXPECT_NEAR(prob, 1.0f / 8.0f, 1e-6f);
    }
}

TEST_F(QuantumSuperpositionTest, ProbabilitySum) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 4);

    float total = compute_total_probability(&qstate);
    EXPECT_NEAR(total, 1.0f, 1e-5f);
}

TEST_F(QuantumSuperpositionTest, ProbabilityAfterModification) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 2);

    /* Modify amplitudes */
    qstate.amplitudes[0] = 0.8f;
    qstate.amplitudes[1] = 0.4f;
    qstate.amplitudes[2] = 0.3f;
    qstate.amplitudes[3] = 0.2f;

    /* Check individual probabilities */
    EXPECT_NEAR(qstate.amplitudes[0] * qstate.amplitudes[0], 0.64f, 1e-6f);
    EXPECT_NEAR(qstate.amplitudes[1] * qstate.amplitudes[1], 0.16f, 1e-6f);
    EXPECT_NEAR(qstate.amplitudes[2] * qstate.amplitudes[2], 0.09f, 1e-6f);
    EXPECT_NEAR(qstate.amplitudes[3] * qstate.amplitudes[3], 0.04f, 1e-6f);
}

//=============================================================================
// Diffusion Operator Tests
//=============================================================================

TEST_F(QuantumSuperpositionTest, DiffusionOnUniform) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    std::vector<float> before(8);
    for (uint32_t i = 0; i < 8; i++) {
        before[i] = qstate.amplitudes[i];
    }

    qreason_diffusion(&qstate);

    /* Uniform state is unchanged by diffusion (up to numerical precision) */
    for (uint32_t i = 0; i < 8; i++) {
        EXPECT_NEAR(qstate.amplitudes[i], before[i], 1e-5f);
    }
}

TEST_F(QuantumSuperpositionTest, DiffusionAmplification) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    /* Mark state 0 by flipping phase */
    qstate.amplitudes[0] *= -1.0f;

    float amp_before = qstate.amplitudes[0];
    qreason_diffusion(&qstate);
    float amp_after = qstate.amplitudes[0];

    /* After diffusion, the flipped amplitude should increase in magnitude */
    EXPECT_GT(fabsf(amp_after), fabsf(amp_before));
}

TEST_F(QuantumSuperpositionTest, DiffusionMean) {
    qreason_qstate_t qstate;
    qstate.n_qubits = 2;
    qstate.state_dim = 4;

    qstate.amplitudes[0] = 0.7f;
    qstate.amplitudes[1] = 0.3f;
    qstate.amplitudes[2] = 0.5f;
    qstate.amplitudes[3] = 0.5f;

    /* Mean = (0.7 + 0.3 + 0.5 + 0.5) / 4 = 0.5 */
    float mean = 0.5f;

    qreason_diffusion(&qstate);

    /* After diffusion: 2*mean - old */
    EXPECT_NEAR(qstate.amplitudes[0], 2.0f * mean - 0.7f, 1e-5f);
    EXPECT_NEAR(qstate.amplitudes[1], 2.0f * mean - 0.3f, 1e-5f);
    EXPECT_NEAR(qstate.amplitudes[2], 2.0f * mean - 0.5f, 1e-5f);
    EXPECT_NEAR(qstate.amplitudes[3], 2.0f * mean - 0.5f, 1e-5f);
}

//=============================================================================
// Oracle Tests
//=============================================================================

TEST_F(QuantumSuperpositionTest, OraclePhaseFlip) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 2);

    /* Create CNF: x0 AND x1 (only state 3 satisfies) */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 2;
    cnf.n_clauses = 2;

    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0] = {0, false};

    cnf.clauses[1].n_literals = 1;
    cnf.clauses[1].literals[0] = {1, false};

    float amp_before = qstate.amplitudes[3];
    qreason_oracle_cnf(&qstate, &cnf);
    float amp_after = qstate.amplitudes[3];

    EXPECT_NEAR(amp_after, -amp_before, 1e-6f);
}

TEST_F(QuantumSuperpositionTest, OraclePreservesNorm) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 1;
    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0] = {0, false};
    cnf.clauses[0].literals[1] = {1, false};

    float prob_before = compute_total_probability(&qstate);
    qreason_oracle_cnf(&qstate, &cnf);
    float prob_after = compute_total_probability(&qstate);

    EXPECT_NEAR(prob_before, prob_after, 1e-5f);
}

//=============================================================================
// Measurement Tests
//=============================================================================

TEST_F(QuantumSuperpositionTest, MeasureMaxAmplitude) {
    qreason_qstate_t qstate;
    qstate.n_qubits = 2;
    qstate.state_dim = 4;

    qstate.amplitudes[0] = 0.1f;
    qstate.amplitudes[1] = 0.2f;
    qstate.amplitudes[2] = 0.9f;  /* Highest */
    qstate.amplitudes[3] = 0.3f;

    uint32_t result = qreason_measure(&qstate);
    EXPECT_EQ(result, 2u);
}

TEST_F(QuantumSuperpositionTest, MeasureNegativeAmplitude) {
    qreason_qstate_t qstate;
    qstate.n_qubits = 2;
    qstate.state_dim = 4;

    qstate.amplitudes[0] = 0.1f;
    qstate.amplitudes[1] = -0.9f;  /* Highest probability (0.81) */
    qstate.amplitudes[2] = 0.3f;
    qstate.amplitudes[3] = 0.2f;

    uint32_t result = qreason_measure(&qstate);
    EXPECT_EQ(result, 1u);
}

TEST_F(QuantumSuperpositionTest, MeasureUniform) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 2);

    uint32_t result = qreason_measure(&qstate);
    /* With uniform amplitudes, first state with max is returned */
    EXPECT_EQ(result, 0u);
}

//=============================================================================
// Interference Tests
//=============================================================================

TEST_F(QuantumSuperpositionTest, ConstructiveInterference) {
    qreason_qstate_t qstate;
    qstate.n_qubits = 2;
    qstate.state_dim = 4;

    /* Set up state where diffusion causes constructive interference at state 0 */
    qstate.amplitudes[0] = 0.5f;
    qstate.amplitudes[1] = 0.5f;
    qstate.amplitudes[2] = 0.5f;
    qstate.amplitudes[3] = 0.5f;

    /* Flip phase of state 0 */
    qstate.amplitudes[0] = -0.5f;

    /* Apply diffusion - should amplify state 0 */
    qreason_diffusion(&qstate);

    /* State 0 should have larger amplitude (in magnitude) */
    EXPECT_GT(fabsf(qstate.amplitudes[0]), fabsf(qstate.amplitudes[1]));
}

TEST_F(QuantumSuperpositionTest, DestructiveInterference) {
    qreason_qstate_t qstate;
    qstate.n_qubits = 2;
    qstate.state_dim = 4;

    /* Mean = 0.5, all start at 0.5 */
    qstate.amplitudes[0] = 0.5f;
    qstate.amplitudes[1] = 0.5f;
    qstate.amplitudes[2] = 0.5f;
    qstate.amplitudes[3] = 0.5f;

    /* After diffusion (no phase flip), uniform stays uniform */
    qreason_diffusion(&qstate);

    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_NEAR(qstate.amplitudes[i], 0.5f, 1e-5f);
    }
}

//=============================================================================
// Grover Iteration Tests
//=============================================================================

TEST_F(QuantumSuperpositionTest, SingleGroverIteration) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    /* Target state 5 */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 3;

    /* x0 = 1, x1 = 0, x2 = 1 (binary 101 = 5) */
    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0] = {0, false};

    cnf.clauses[1].n_literals = 1;
    cnf.clauses[1].literals[0] = {1, true};

    cnf.clauses[2].n_literals = 1;
    cnf.clauses[2].literals[0] = {2, false};

    float prob_before = qstate.amplitudes[5] * qstate.amplitudes[5];

    qreason_oracle_cnf(&qstate, &cnf);
    qreason_diffusion(&qstate);

    float prob_after = qstate.amplitudes[5] * qstate.amplitudes[5];

    /* Probability should increase */
    EXPECT_GT(prob_after, prob_before);
}

TEST_F(QuantumSuperpositionTest, MultipleGroverIterations) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 4);

    /* Target state 10 */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 4;
    cnf.n_clauses = 4;

    /* x0=0, x1=1, x2=0, x3=1 (binary 1010 = 10) */
    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0] = {0, true};

    cnf.clauses[1].n_literals = 1;
    cnf.clauses[1].literals[0] = {1, false};

    cnf.clauses[2].n_literals = 1;
    cnf.clauses[2].literals[0] = {2, true};

    cnf.clauses[3].n_literals = 1;
    cnf.clauses[3].literals[0] = {3, false};

    std::vector<float> probs;
    probs.push_back(qstate.amplitudes[10] * qstate.amplitudes[10]);

    for (int iter = 0; iter < 5; iter++) {
        qreason_oracle_cnf(&qstate, &cnf);
        qreason_diffusion(&qstate);
        probs.push_back(qstate.amplitudes[10] * qstate.amplitudes[10]);
    }

    /* Probability should peak then oscillate */
    float max_prob = *std::max_element(probs.begin(), probs.end());
    EXPECT_GT(max_prob, 0.5f);
}

//=============================================================================
// State Size Tests
//=============================================================================

TEST_F(QuantumSuperpositionTest, SingleQubit) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 1);

    EXPECT_EQ(qstate.n_qubits, 1u);
    EXPECT_EQ(qstate.state_dim, 2u);

    float expected = 1.0f / sqrtf(2.0f);
    EXPECT_NEAR(qstate.amplitudes[0], expected, 1e-6f);
    EXPECT_NEAR(qstate.amplitudes[1], expected, 1e-6f);
}

TEST_F(QuantumSuperpositionTest, StateDimensions) {
    for (uint32_t n = 1; n <= 10; n++) {
        qreason_qstate_t qstate;
        qreason_qstate_init(&qstate, n);

        EXPECT_EQ(qstate.n_qubits, n);
        EXPECT_EQ(qstate.state_dim, 1u << n);
    }
}

//=============================================================================
// Amplitude Distribution Tests
//=============================================================================

TEST_F(QuantumSuperpositionTest, AmplitudeRange) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 5);

    /* All amplitudes should be positive and equal initially */
    for (uint32_t i = 0; i < qstate.state_dim; i++) {
        EXPECT_GE(qstate.amplitudes[i], 0.0f);
        EXPECT_LE(qstate.amplitudes[i], 1.0f);
    }
}

TEST_F(QuantumSuperpositionTest, AmplitudesAfterOracle) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 2;
    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0] = {0, false};
    cnf.clauses[0].literals[1] = {1, false};
    cnf.clauses[1].n_literals = 2;
    cnf.clauses[1].literals[0] = {1, false};
    cnf.clauses[1].literals[1] = {2, false};

    qreason_oracle_cnf(&qstate, &cnf);

    /* Some amplitudes should be negative (phase flipped) */
    bool has_negative = false;
    for (uint32_t i = 0; i < qstate.state_dim; i++) {
        if (qstate.amplitudes[i] < 0.0f) {
            has_negative = true;
            break;
        }
    }
    EXPECT_TRUE(has_negative);
}

//=============================================================================
// Satisfaction Probability Tests
//=============================================================================

TEST_F(QuantumSuperpositionTest, SatProbEmpty) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 0;

    float prob = qreason_satisfaction_probability(&qstate, &cnf);
    EXPECT_NEAR(prob, 1.0f, 1e-5f);
}

TEST_F(QuantumSuperpositionTest, SatProbSingleSolution) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    /* Unique solution at state 7 (all true) */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 3;

    for (uint32_t i = 0; i < 3; i++) {
        cnf.clauses[i].n_literals = 1;
        cnf.clauses[i].literals[0] = {i, false};
    }

    float prob = qreason_satisfaction_probability(&qstate, &cnf);
    EXPECT_NEAR(prob, 1.0f / 8.0f, 1e-5f);
}

TEST_F(QuantumSuperpositionTest, SatProbAfterAmplification) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 3;

    for (uint32_t i = 0; i < 3; i++) {
        cnf.clauses[i].n_literals = 1;
        cnf.clauses[i].literals[0] = {i, false};
    }

    float prob_before = qreason_satisfaction_probability(&qstate, &cnf);

    /* Apply Grover iteration */
    qreason_oracle_cnf(&qstate, &cnf);
    qreason_diffusion(&qstate);

    float prob_after = qreason_satisfaction_probability(&qstate, &cnf);

    EXPECT_GT(prob_after, prob_before);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
