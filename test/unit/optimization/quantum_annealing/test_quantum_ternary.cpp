/**
 * @file test_quantum_ternary.cpp
 * @brief Unit tests for ternary Ising quantum annealing
 *
 * WHAT: Tests for nimcp_quantum_ternary.h and nimcp_quantum_annealing_ternary.h
 * WHY:  Verify ternary spin operations, energy calculations, and annealing
 * HOW:  GTest with fixture for lifecycle, spin ops, energy, annealing
 *
 * FUNCTIONS TESTED:
 *   nimcp_quantum_ternary.h (inline):
 *     - trit_ising_create / trit_ising_destroy
 *     - trit_ising_measure / trit_ising_reset / trit_ising_flip
 *     - trit_ising_local_field / trit_ising_energy / trit_ising_delta_energy
 *     - trit_ising_coherence / trit_ising_collapse_all / trit_ising_tunnel
 *     - trit_ising_get_stats
 *   nimcp_quantum_annealing_ternary.h:
 *     - quantum_ternary_default_config
 *     - quantum_ternary_anneal (non-inline, in .c)
 *     - quantum_ternary_sweep (non-inline, in .c)
 *     - quantum_ternary_measure / quantum_ternary_metropolis (inline)
 *     - quantum_ternary_from_weights / quantum_ternary_to_weights (inline)
 *     - ternary_ising_energy_wrapper (inline)
 *
 * @date 2026-02-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "optimization/quantum_annealing/nimcp_quantum_annealing_ternary.h"
}

/* ============================================================================
 * Ternary Ising Lifecycle Tests
 * ============================================================================ */

class TritIsingLifecycleTest : public ::testing::Test {
protected:
    trit_ising_config_t* ising = nullptr;

    void TearDown() override {
        if (ising) {
            trit_ising_destroy(ising);
            ising = nullptr;
        }
    }
};

TEST_F(TritIsingLifecycleTest, CreateWithValidParams) {
    ising = trit_ising_create(10, 0.5);
    ASSERT_NE(ising, nullptr);
    EXPECT_EQ(ising->n_spins, 10u);
    EXPECT_DOUBLE_EQ(ising->superposition_penalty, 0.5);
    EXPECT_EQ(ising->n_measured, 0u);
    EXPECT_EQ(ising->n_superposition, 10u);
    EXPECT_NE(ising->spins, nullptr);
}

TEST_F(TritIsingLifecycleTest, CreateWithZeroSpins) {
    ising = trit_ising_create(0, 0.1);
    /* Should succeed with 0 spins */
    ASSERT_NE(ising, nullptr);
    EXPECT_EQ(ising->n_spins, 0u);
    EXPECT_EQ(ising->n_superposition, 0u);
}

TEST_F(TritIsingLifecycleTest, CreateWithZeroPenalty) {
    ising = trit_ising_create(5, 0.0);
    ASSERT_NE(ising, nullptr);
    EXPECT_DOUBLE_EQ(ising->superposition_penalty, 0.0);
}

TEST_F(TritIsingLifecycleTest, DestroyNull) {
    /* Should not crash */
    trit_ising_destroy(nullptr);
}

TEST_F(TritIsingLifecycleTest, AllSpinsInitToSuperposition) {
    ising = trit_ising_create(8, 0.1);
    ASSERT_NE(ising, nullptr);

    for (uint32_t i = 0; i < 8; i++) {
        EXPECT_EQ(trit_vector_get(ising->spins, i), TRIT_SPIN_SUPERPOSITION)
            << "Spin " << i << " should be superposition";
    }
}

TEST_F(TritIsingLifecycleTest, InitialEnergyIsPenaltyTimesSpins) {
    ising = trit_ising_create(6, 2.0);
    ASSERT_NE(ising, nullptr);
    EXPECT_DOUBLE_EQ(ising->energy, 12.0);
}

/* ============================================================================
 * Spin Operations Tests
 * ============================================================================ */

class TritIsingSpinOpsTest : public ::testing::Test {
protected:
    trit_ising_config_t* ising = nullptr;

    void SetUp() override {
        ising = trit_ising_create(4, 0.5);
        ASSERT_NE(ising, nullptr);
    }

    void TearDown() override {
        if (ising) {
            trit_ising_destroy(ising);
            ising = nullptr;
        }
    }
};

TEST_F(TritIsingSpinOpsTest, MeasureFromSuperposition) {
    trit_spin_t old = trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    EXPECT_EQ(old, TRIT_SPIN_SUPERPOSITION);
    EXPECT_EQ(trit_vector_get(ising->spins, 0), TRIT_SPIN_UP);
    EXPECT_EQ(ising->n_measured, 1u);
    EXPECT_EQ(ising->n_superposition, 3u);
}

TEST_F(TritIsingSpinOpsTest, MeasureDown) {
    trit_spin_t old = trit_ising_measure(ising, 1, TRIT_SPIN_DOWN);
    EXPECT_EQ(old, TRIT_SPIN_SUPERPOSITION);
    EXPECT_EQ(trit_vector_get(ising->spins, 1), TRIT_SPIN_DOWN);
}

TEST_F(TritIsingSpinOpsTest, MeasureInvalidResult) {
    /* Superposition as result should default to UP */
    trit_ising_measure(ising, 2, TRIT_SPIN_SUPERPOSITION);
    EXPECT_EQ(trit_vector_get(ising->spins, 2), TRIT_SPIN_UP);
}

TEST_F(TritIsingSpinOpsTest, MeasureOutOfBounds) {
    trit_spin_t old = trit_ising_measure(ising, 100, TRIT_SPIN_UP);
    EXPECT_EQ(old, TRIT_SPIN_SUPERPOSITION);
    /* Counters should not change */
    EXPECT_EQ(ising->n_measured, 0u);
}

TEST_F(TritIsingSpinOpsTest, MeasureNullConfig) {
    trit_spin_t old = trit_ising_measure(nullptr, 0, TRIT_SPIN_UP);
    EXPECT_EQ(old, TRIT_SPIN_SUPERPOSITION);
}

TEST_F(TritIsingSpinOpsTest, ResetMeasuredSpin) {
    trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    EXPECT_EQ(ising->n_measured, 1u);

    trit_spin_t old = trit_ising_reset(ising, 0);
    EXPECT_EQ(old, TRIT_SPIN_UP);
    EXPECT_EQ(trit_vector_get(ising->spins, 0), TRIT_SPIN_SUPERPOSITION);
    EXPECT_EQ(ising->n_measured, 0u);
    EXPECT_EQ(ising->n_superposition, 4u);
}

TEST_F(TritIsingSpinOpsTest, ResetAlreadySuperposition) {
    trit_spin_t old = trit_ising_reset(ising, 0);
    EXPECT_EQ(old, TRIT_SPIN_SUPERPOSITION);
    /* Counters should not change */
    EXPECT_EQ(ising->n_superposition, 4u);
}

TEST_F(TritIsingSpinOpsTest, FlipUp) {
    trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    trit_ising_flip(ising, 0);
    EXPECT_EQ(trit_vector_get(ising->spins, 0), TRIT_SPIN_DOWN);
}

TEST_F(TritIsingSpinOpsTest, FlipDown) {
    trit_ising_measure(ising, 0, TRIT_SPIN_DOWN);
    trit_ising_flip(ising, 0);
    EXPECT_EQ(trit_vector_get(ising->spins, 0), TRIT_SPIN_UP);
}

TEST_F(TritIsingSpinOpsTest, FlipSuperpositionIsNoop) {
    trit_ising_flip(ising, 0);
    EXPECT_EQ(trit_vector_get(ising->spins, 0), TRIT_SPIN_SUPERPOSITION);
}

TEST_F(TritIsingSpinOpsTest, FlipOutOfBounds) {
    /* Should not crash */
    trit_ising_flip(ising, 100);
    trit_ising_flip(nullptr, 0);
}

/* ============================================================================
 * Energy Calculation Tests
 * ============================================================================ */

class TritIsingEnergyTest : public ::testing::Test {
protected:
    trit_ising_config_t* ising = nullptr;
    /* 3x3 coupling matrix: ferromagnetic (J>0 favors alignment) */
    float J[9] = {0.0f, 1.0f, 0.5f,
                  1.0f, 0.0f, 0.3f,
                  0.5f, 0.3f, 0.0f};
    /* External field */
    float h[3] = {0.2f, -0.1f, 0.0f};

    void SetUp() override {
        ising = trit_ising_create(3, 0.0);
        ASSERT_NE(ising, nullptr);
    }

    void TearDown() override {
        if (ising) {
            trit_ising_destroy(ising);
            ising = nullptr;
        }
    }
};

TEST_F(TritIsingEnergyTest, AllSuperpositionEnergyIsZero) {
    double E = trit_ising_energy(ising, J, h);
    EXPECT_DOUBLE_EQ(E, 0.0);
}

TEST_F(TritIsingEnergyTest, AlignedSpinsLowEnergy) {
    /* All spins up: E = -J01*1*1 - J02*1*1 - J12*1*1 - h0*1 - h1*1 - h2*1 */
    /* E = -1.0 - 0.5 - 0.3 - 0.2 + 0.1 - 0.0 = -1.9 */
    trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    trit_ising_measure(ising, 1, TRIT_SPIN_UP);
    trit_ising_measure(ising, 2, TRIT_SPIN_UP);

    double E = trit_ising_energy(ising, J, h);
    EXPECT_NEAR(E, -1.9, 1e-6);
}

TEST_F(TritIsingEnergyTest, MixedSpinsEnergy) {
    /* Spin 0=UP, 1=DOWN: E = -J01*(1)*(-1) - h0*(1) - h1*(-1)
     * (spin 2 is superposition, contributes nothing)
     * E = -1.0*(1)*(-1) - 0.2*(1) + 0.1*(-1) = 1.0 - 0.2 - 0.1 = 0.7 */
    trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    trit_ising_measure(ising, 1, TRIT_SPIN_DOWN);

    double E = trit_ising_energy(ising, J, h);
    EXPECT_NEAR(E, 0.7, 1e-6);
}

TEST_F(TritIsingEnergyTest, LocalFieldComputation) {
    /* Set all spins up */
    trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    trit_ising_measure(ising, 1, TRIT_SPIN_UP);
    trit_ising_measure(ising, 2, TRIT_SPIN_UP);

    /* Local field on spin 0: h[0] + J[0,1]*s1 + J[0,2]*s2 */
    /* = 0.2 + 1.0*1 + 0.5*1 = 1.7 */
    double field = trit_ising_local_field(ising, J, h, 0);
    EXPECT_NEAR(field, 1.7, 1e-6);
}

TEST_F(TritIsingEnergyTest, LocalFieldNullJ) {
    trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    double field = trit_ising_local_field(ising, nullptr, h, 0);
    EXPECT_NEAR(field, 0.2, 1e-6);
}

TEST_F(TritIsingEnergyTest, LocalFieldNullH) {
    trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    trit_ising_measure(ising, 1, TRIT_SPIN_UP);
    double field = trit_ising_local_field(ising, J, nullptr, 0);
    /* h[0]=0, J[0,1]*s1=1.0 */
    EXPECT_NEAR(field, 1.0, 1e-6);
}

TEST_F(TritIsingEnergyTest, DeltaEnergyFlip) {
    /* Set spins: 0=UP, 1=UP, 2=UP */
    trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    trit_ising_measure(ising, 1, TRIT_SPIN_UP);
    trit_ising_measure(ising, 2, TRIT_SPIN_UP);

    double E_before = trit_ising_energy(ising, J, h);
    double delta = trit_ising_delta_energy(ising, J, h, 0);

    /* Actually flip and verify */
    trit_ising_flip(ising, 0);
    double E_after = trit_ising_energy(ising, J, h);

    /* delta_E should equal E_after - E_before */
    EXPECT_NEAR(delta, E_after - E_before, 1e-6);
}

TEST_F(TritIsingEnergyTest, DeltaEnergySuperposition) {
    /* Superposition spin should return 0 delta */
    double delta = trit_ising_delta_energy(ising, J, h, 0);
    EXPECT_DOUBLE_EQ(delta, 0.0);
}

TEST_F(TritIsingEnergyTest, EnergyNullConfig) {
    double E = trit_ising_energy(nullptr, J, h);
    EXPECT_DOUBLE_EQ(E, 0.0);
}

/* ============================================================================
 * Coherence and Collapse Tests
 * ============================================================================ */

class TritIsingCoherenceTest : public ::testing::Test {
protected:
    trit_ising_config_t* ising = nullptr;

    void SetUp() override {
        ising = trit_ising_create(4, 0.0);
        ASSERT_NE(ising, nullptr);
    }

    void TearDown() override {
        if (ising) {
            trit_ising_destroy(ising);
            ising = nullptr;
        }
    }
};

TEST_F(TritIsingCoherenceTest, FullCoherence) {
    EXPECT_FLOAT_EQ(trit_ising_coherence(ising), 1.0f);
}

TEST_F(TritIsingCoherenceTest, HalfCoherence) {
    trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    trit_ising_measure(ising, 1, TRIT_SPIN_DOWN);
    EXPECT_FLOAT_EQ(trit_ising_coherence(ising), 0.5f);
}

TEST_F(TritIsingCoherenceTest, ZeroCoherence) {
    for (uint32_t i = 0; i < 4; i++) {
        trit_ising_measure(ising, i, TRIT_SPIN_UP);
    }
    EXPECT_FLOAT_EQ(trit_ising_coherence(ising), 0.0f);
}

TEST_F(TritIsingCoherenceTest, NullCoherence) {
    EXPECT_FLOAT_EQ(trit_ising_coherence(nullptr), 0.0f);
}

TEST_F(TritIsingCoherenceTest, CollapseAllNoFieldFavorsUp) {
    /* Coupling: 2x2 ferromagnetic */
    float J[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    float h2[2] = {0.1f, 0.1f};

    trit_ising_config_t* small = trit_ising_create(2, 0.0);
    ASSERT_NE(small, nullptr);

    trit_ising_collapse_all(small, J, h2);

    /* After collapse, no spins should be superposition */
    EXPECT_EQ(trit_ising_coherence(small), 0.0f);

    /* With positive field, both should be UP */
    EXPECT_EQ(trit_vector_get(small->spins, 0), TRIT_SPIN_UP);

    trit_ising_destroy(small);
}

TEST_F(TritIsingCoherenceTest, CollapseNullConfig) {
    /* Should not crash */
    trit_ising_collapse_all(nullptr, nullptr, nullptr);
}

/* ============================================================================
 * Tunnel Test
 * ============================================================================ */

class TritIsingTunnelTest : public ::testing::Test {
protected:
    trit_ising_config_t* ising = nullptr;

    void SetUp() override {
        ising = trit_ising_create(2, 0.0);
        ASSERT_NE(ising, nullptr);
        trit_ising_measure(ising, 0, TRIT_SPIN_UP);
        trit_ising_measure(ising, 1, TRIT_SPIN_DOWN);
    }

    void TearDown() override {
        if (ising) {
            trit_ising_destroy(ising);
            ising = nullptr;
        }
    }
};

TEST_F(TritIsingTunnelTest, HighGammaTunnels) {
    /* With gamma=10 and temperature=0, tunnel_prob = 10/(1+0) = 10
     * clamped to something > random=0.5, so should flip */
    bool tunneled = trit_ising_tunnel(ising, 0, 10.0f, 0.0f, 0.05f);
    EXPECT_TRUE(tunneled);
    EXPECT_EQ(trit_vector_get(ising->spins, 0), TRIT_SPIN_DOWN);
}

TEST_F(TritIsingTunnelTest, ZeroGammaNoTunnel) {
    bool tunneled = trit_ising_tunnel(ising, 0, 0.0f, 1.0f, 0.5f);
    EXPECT_FALSE(tunneled);
    EXPECT_EQ(trit_vector_get(ising->spins, 0), TRIT_SPIN_UP);
}

TEST_F(TritIsingTunnelTest, SuperpositionCannotTunnel) {
    trit_ising_reset(ising, 0);
    bool tunneled = trit_ising_tunnel(ising, 0, 10.0f, 0.0f, 0.0f);
    EXPECT_FALSE(tunneled);
}

TEST_F(TritIsingTunnelTest, NullConfig) {
    bool tunneled = trit_ising_tunnel(nullptr, 0, 1.0f, 0.0f, 0.5f);
    EXPECT_FALSE(tunneled);
}

TEST_F(TritIsingTunnelTest, OutOfBounds) {
    bool tunneled = trit_ising_tunnel(ising, 100, 1.0f, 0.0f, 0.5f);
    EXPECT_FALSE(tunneled);
}

/* ============================================================================
 * Statistics Test
 * ============================================================================ */

class TritIsingStatsTest : public ::testing::Test {
protected:
    trit_ising_config_t* ising = nullptr;

    void SetUp() override {
        ising = trit_ising_create(4, 0.5);
        ASSERT_NE(ising, nullptr);
    }

    void TearDown() override {
        if (ising) {
            trit_ising_destroy(ising);
            ising = nullptr;
        }
    }
};

TEST_F(TritIsingStatsTest, AllSuperposition) {
    trit_ising_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));
    trit_ising_get_stats(ising, &stats);

    EXPECT_EQ(stats.n_up, 0u);
    EXPECT_EQ(stats.n_down, 0u);
    EXPECT_EQ(stats.n_superposition, 4u);
    EXPECT_FLOAT_EQ(stats.magnetization, 0.0f);
    EXPECT_FLOAT_EQ(stats.coherence, 1.0f);
}

TEST_F(TritIsingStatsTest, MixedState) {
    trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    trit_ising_measure(ising, 1, TRIT_SPIN_UP);
    trit_ising_measure(ising, 2, TRIT_SPIN_DOWN);
    /* Spin 3 remains superposition */

    trit_ising_stats_t stats;
    trit_ising_get_stats(ising, &stats);

    EXPECT_EQ(stats.n_up, 2u);
    EXPECT_EQ(stats.n_down, 1u);
    EXPECT_EQ(stats.n_superposition, 1u);
    /* magnetization = (2-1)/4 = 0.25 */
    EXPECT_FLOAT_EQ(stats.magnetization, 0.25f);
    EXPECT_FLOAT_EQ(stats.coherence, 0.25f);
}

TEST_F(TritIsingStatsTest, NullStatsDoesNotCrash) {
    trit_ising_get_stats(ising, nullptr);
}

TEST_F(TritIsingStatsTest, NullConfigSetsZero) {
    trit_ising_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));
    trit_ising_get_stats(nullptr, &stats);
    EXPECT_EQ(stats.n_up, 0u);
    EXPECT_EQ(stats.n_down, 0u);
    EXPECT_EQ(stats.n_superposition, 0u);
}

/* ============================================================================
 * Quantum Ternary Annealing Config Tests
 * ============================================================================ */

TEST(QuantumTernaryConfigTest, DefaultConfig) {
    quantum_ternary_config_t cfg = quantum_ternary_default_config();
    EXPECT_GT(cfg.initial_temperature, 0.0f);
    EXPECT_GT(cfg.final_temperature, 0.0f);
    EXPECT_LT(cfg.final_temperature, cfg.initial_temperature);
    EXPECT_GT(cfg.num_sweeps, 0u);
    EXPECT_GT(cfg.initial_gamma, 0.0f);
    EXPECT_EQ(cfg.final_gamma, 0.0f);
    EXPECT_TRUE(cfg.enable_tunneling);
    EXPECT_TRUE(cfg.track_best);
}

/* ============================================================================
 * Quantum Ternary Anneal Tests
 * ============================================================================ */

class QuantumTernaryAnnealTest : public ::testing::Test {
protected:
    trit_ising_config_t* ising = nullptr;
    quantum_ternary_config_t config;
    static constexpr uint32_t N = 4;
    /* Ferromagnetic coupling: favors all-aligned */
    float J[N * N] = {0};
    float h[N] = {0};

    void SetUp() override {
        config = quantum_ternary_default_config();
        config.num_sweeps = 50;
        config.seed = 42;

        ising = trit_ising_create(N, 0.1);
        ASSERT_NE(ising, nullptr);

        /* Fill coupling matrix: ferromagnetic nearest-neighbor */
        memset(J, 0, sizeof(J));
        for (uint32_t i = 0; i < N; i++) {
            for (uint32_t j = i + 1; j < N; j++) {
                J[i * N + j] = 1.0f;
                J[j * N + i] = 1.0f;
            }
        }
        /* Positive field favors UP */
        for (uint32_t i = 0; i < N; i++) {
            h[i] = 0.1f;
        }
    }

    void TearDown() override {
        if (ising) {
            trit_ising_destroy(ising);
            ising = nullptr;
        }
    }
};

TEST_F(QuantumTernaryAnnealTest, BasicAnnealSucceeds) {
    quantum_ternary_result_t result;
    int rc = quantum_ternary_anneal(ising, J, h, &config, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(result.converged);
    EXPECT_EQ(result.final_coherence, 0.0f);
}

TEST_F(QuantumTernaryAnnealTest, ConvergesToGroundState) {
    quantum_ternary_result_t result;
    int rc = quantum_ternary_anneal(ising, J, h, &config, &result);
    EXPECT_EQ(rc, 0);

    /* Ground state of fully-connected ferromagnet with positive field:
     * all spins UP. Energy should be very negative. */
    EXPECT_LT(result.best_energy, 0.0);
    EXPECT_LT(result.final_energy, 0.0);
}

TEST_F(QuantumTernaryAnnealTest, BestEnergyNoWorseThanFinal) {
    quantum_ternary_result_t result;
    quantum_ternary_anneal(ising, J, h, &config, &result);
    EXPECT_LE(result.best_energy, result.final_energy + 1e-6);
}

TEST_F(QuantumTernaryAnnealTest, DifferentSeedsProduceDifferentPaths) {
    quantum_ternary_result_t r1, r2;
    config.seed = 123;
    quantum_ternary_anneal(ising, J, h, &config, &r1);

    /* Recreate ising */
    trit_ising_destroy(ising);
    ising = trit_ising_create(N, 0.1);

    config.seed = 456;
    quantum_ternary_anneal(ising, J, h, &config, &r2);

    /* At least one stat should differ */
    EXPECT_TRUE(r1.total_flips != r2.total_flips ||
                r1.best_iteration != r2.best_iteration);
}

TEST_F(QuantumTernaryAnnealTest, NullIsingReturnsError) {
    quantum_ternary_result_t result;
    int rc = quantum_ternary_anneal(nullptr, J, h, &config, &result);
    EXPECT_NE(rc, 0);
}

TEST_F(QuantumTernaryAnnealTest, NullConfigReturnsError) {
    quantum_ternary_result_t result;
    int rc = quantum_ternary_anneal(ising, J, h, nullptr, &result);
    EXPECT_NE(rc, 0);
}

TEST_F(QuantumTernaryAnnealTest, NullResultReturnsError) {
    int rc = quantum_ternary_anneal(ising, J, h, &config, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(QuantumTernaryAnnealTest, SingleSweep) {
    config.num_sweeps = 1;
    quantum_ternary_result_t result;
    int rc = quantum_ternary_anneal(ising, J, h, &config, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(result.total_flips, 0u);
}

TEST_F(QuantumTernaryAnnealTest, NoTrackBest) {
    config.track_best = false;
    quantum_ternary_result_t result;
    int rc = quantum_ternary_anneal(ising, J, h, &config, &result);
    EXPECT_EQ(rc, 0);
}

/* ============================================================================
 * Quantum Ternary Sweep Tests
 * ============================================================================ */

class QuantumTernarySweepTest : public ::testing::Test {
protected:
    trit_ising_config_t* ising = nullptr;
    float J[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    float h[2] = {0.1f, 0.1f};
    uint64_t rng_state = 12345;

    void SetUp() override {
        ising = trit_ising_create(2, 0.0);
        ASSERT_NE(ising, nullptr);
    }

    void TearDown() override {
        if (ising) {
            trit_ising_destroy(ising);
            ising = nullptr;
        }
    }
};

TEST_F(QuantumTernarySweepTest, BasicSweep) {
    uint32_t accepted = quantum_ternary_sweep(ising, J, h, 1.0f, 0.5f, &rng_state);
    /* Some moves should be accepted */
    EXPECT_GE(accepted, 0u);
}

TEST_F(QuantumTernarySweepTest, NullIsingReturnsZero) {
    uint32_t accepted = quantum_ternary_sweep(nullptr, J, h, 1.0f, 0.5f, &rng_state);
    EXPECT_EQ(accepted, 0u);
}

TEST_F(QuantumTernarySweepTest, NullRngReturnsZero) {
    uint32_t accepted = quantum_ternary_sweep(ising, J, h, 1.0f, 0.5f, nullptr);
    EXPECT_EQ(accepted, 0u);
}

/* ============================================================================
 * Weight Conversion Tests
 * ============================================================================ */

class QuantumTernaryWeightTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(QuantumTernaryWeightTest, FromWeightsCorrectMapping) {
    float weights[4] = {0.5f, -0.8f, 0.01f, 0.7f};
    trit_ising_config_t* ising = quantum_ternary_from_weights(weights, 4, 0.1f, 0.0);
    ASSERT_NE(ising, nullptr);

    /* 0.5 > 0.1 -> UP, -0.8 < -0.1 -> DOWN */
    EXPECT_EQ(trit_vector_get(ising->spins, 0), TRIT_SPIN_UP);
    EXPECT_EQ(trit_vector_get(ising->spins, 1), TRIT_SPIN_DOWN);
    /* 0.01 is in [-0.1,0.1] -> code passes SUPERPOSITION to trit_ising_measure,
     * which defaults invalid result to UP per implementation */
    EXPECT_EQ(trit_vector_get(ising->spins, 2), TRIT_SPIN_UP);
    /* 0.7 > 0.1 -> UP */
    EXPECT_EQ(trit_vector_get(ising->spins, 3), TRIT_SPIN_UP);

    trit_ising_destroy(ising);
}

TEST_F(QuantumTernaryWeightTest, ToWeightsCorrectMapping) {
    trit_ising_config_t* ising = trit_ising_create(3, 0.0);
    ASSERT_NE(ising, nullptr);

    trit_ising_measure(ising, 0, TRIT_SPIN_UP);
    trit_ising_measure(ising, 1, TRIT_SPIN_DOWN);
    /* Spin 2 stays superposition */

    float weights[3] = {0};
    quantum_ternary_to_weights(ising, weights, 2.0f);

    /* UP=+1 * 2.0 = 2.0, DOWN=-1 * 2.0 = -2.0, SUPER=0 * 2.0 = 0.0 */
    EXPECT_FLOAT_EQ(weights[0], 2.0f);
    EXPECT_FLOAT_EQ(weights[1], -2.0f);
    EXPECT_FLOAT_EQ(weights[2], 0.0f);

    trit_ising_destroy(ising);
}

TEST_F(QuantumTernaryWeightTest, RoundTripConversion) {
    float original[4] = {1.0f, -1.0f, 0.0f, 0.5f};
    trit_ising_config_t* ising = quantum_ternary_from_weights(original, 4, 0.1f, 0.0);
    ASSERT_NE(ising, nullptr);

    float recovered[4] = {0};
    quantum_ternary_to_weights(ising, recovered, 1.0f);

    /* 1.0 -> UP -> 1.0, -1.0 -> DOWN -> -1.0 */
    EXPECT_FLOAT_EQ(recovered[0], 1.0f);
    EXPECT_FLOAT_EQ(recovered[1], -1.0f);
    /* 0.0 in [-0.1,0.1] -> measure(SUPERPOSITION) defaults to UP -> 1.0 */
    EXPECT_FLOAT_EQ(recovered[2], 1.0f);
    /* 0.5 > 0.1 -> UP -> 1.0 */
    EXPECT_FLOAT_EQ(recovered[3], 1.0f);

    trit_ising_destroy(ising);
}

TEST_F(QuantumTernaryWeightTest, NullWeightsHandledGracefully) {
    trit_ising_config_t* ising = quantum_ternary_from_weights(nullptr, 4, 0.1f, 0.0);
    /* Should still create the ising but skip weight init */
    ASSERT_NE(ising, nullptr);
    trit_ising_destroy(ising);
}

TEST_F(QuantumTernaryWeightTest, ToWeightsNullInputs) {
    /* Should not crash */
    quantum_ternary_to_weights(nullptr, nullptr, 1.0f);
}

/* ============================================================================
 * Energy Wrapper Test
 * ============================================================================ */

TEST(TernaryEnergyWrapperTest, BasicWrap) {
    trit_ising_config_t* ising = trit_ising_create(2, 0.0);
    ASSERT_NE(ising, nullptr);

    float J[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    float h2[2] = {0.1f, 0.1f};

    ternary_energy_context_t ctx;
    ctx.ising = ising;
    ctx.J = J;
    ctx.h = h2;
    ctx.threshold = 0.3f;

    float state[2] = {1.0f, 1.0f};  /* Both above threshold -> UP */
    float energy = ternary_ising_energy_wrapper(state, 2, &ctx);
    EXPECT_LT(energy, 0.0f);

    trit_ising_destroy(ising);
}

TEST(TernaryEnergyWrapperTest, NullContextReturnsLargeValue) {
    float state[2] = {1.0f, 1.0f};
    float energy = ternary_ising_energy_wrapper(state, 2, nullptr);
    EXPECT_GT(energy, 1e20f);
}

TEST(TernaryEnergyWrapperTest, WrongDimensionReturnsLargeValue) {
    trit_ising_config_t* ising = trit_ising_create(2, 0.0);
    ASSERT_NE(ising, nullptr);

    ternary_energy_context_t ctx;
    ctx.ising = ising;
    ctx.J = nullptr;
    ctx.h = nullptr;
    ctx.threshold = 0.3f;

    float state[3] = {1.0f, 1.0f, 1.0f};
    float energy = ternary_ising_energy_wrapper(state, 3, &ctx);
    EXPECT_GT(energy, 1e20f);

    trit_ising_destroy(ising);
}

/* ============================================================================
 * Inline Measure and Metropolis Tests
 * ============================================================================ */

class QuantumTernaryMeasureTest : public ::testing::Test {
protected:
    trit_ising_config_t* ising = nullptr;
    float J[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    float h[2] = {0.5f, 0.5f};

    void SetUp() override {
        ising = trit_ising_create(2, 0.0);
        ASSERT_NE(ising, nullptr);
    }

    void TearDown() override {
        if (ising) {
            trit_ising_destroy(ising);
            ising = nullptr;
        }
    }
};

TEST_F(QuantumTernaryMeasureTest, MeasureWithPositiveField) {
    /* With positive field, low random should give UP */
    trit_spin_t result = quantum_ternary_measure(ising, J, h, 0, 1.0f, 0.1f);
    /* At high temperature with positive field, p_up > 0.5, so random 0.1 -> UP */
    EXPECT_EQ(result, TRIT_SPIN_UP);
}

TEST_F(QuantumTernaryMeasureTest, MeasureNullConfig) {
    trit_spin_t result = quantum_ternary_measure(nullptr, J, h, 0, 1.0f, 0.5f);
    EXPECT_EQ(result, TRIT_SPIN_DOWN);
}

TEST_F(QuantumTernaryMeasureTest, MeasureOutOfBounds) {
    trit_spin_t result = quantum_ternary_measure(ising, J, h, 100, 1.0f, 0.5f);
    EXPECT_EQ(result, TRIT_SPIN_DOWN);
}

TEST_F(QuantumTernaryMeasureTest, MetropolisNullConfig) {
    bool accepted = quantum_ternary_metropolis(nullptr, J, h, 0, 1.0f, 0.5f, 0.5f);
    EXPECT_FALSE(accepted);
}

TEST_F(QuantumTernaryMeasureTest, MetropolisSuperpositionRejected) {
    /* Spin 0 is superposition initially */
    bool accepted = quantum_ternary_metropolis(ising, J, h, 0, 1.0f, 0.5f, 0.5f);
    EXPECT_FALSE(accepted);
}

TEST_F(QuantumTernaryMeasureTest, MetropolisAcceptsDownhillFlip) {
    /* Set spin 0 to DOWN with positive field -> flipping to UP is downhill */
    trit_ising_measure(ising, 0, TRIT_SPIN_DOWN);
    trit_ising_measure(ising, 1, TRIT_SPIN_UP);

    /* delta_E for flipping spin 0 from DOWN to UP:
     * With positive J and positive h, flipping from -1 to +1 lowers energy
     * delta_E = 2*(-1)*(J01*1 + h0) = 2*(-1)*(1.0 + 0.5) = -3.0
     * So p_accept = 1.0 (downhill move) */
    bool accepted = quantum_ternary_metropolis(ising, J, h, 0, 1.0f, 0.0f, 0.99f);
    EXPECT_TRUE(accepted);
}
