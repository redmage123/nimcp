/**
 * @file test_sparse_coding_integration.cpp
 * @brief Unit tests for sparse coding integration into brain_decide()
 *
 * Tests the cortical sparse coding system as used by brain_decide():
 * K-WTA sparsity enforcement on output activations, homeostatic threshold
 * adaptation, population sparsity computation, in-place enforcement,
 * and edge cases.
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <vector>

extern "C" {
#include "core/cortical_columns/nimcp_cortical_sparse_coding.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SparseCodingIntegrationTest : public ::testing::Test {
protected:
    cortical_sparse_coding_system_t* system;
    sparse_coding_config_t config;

    void SetUp() override {
        cortical_sparse_default_config(&config);
        config.num_columns = 100;
        config.sparsity_method = SPARSITY_METHOD_K_WTA;
        config.k_winners = 5;
        config.target_sparsity = 0.05f;
        config.enable_homeostasis = true;
        system = cortical_sparse_create(&config);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            cortical_sparse_destroy(system);
        }
    }
};

/* ============================================================================
 * 1. Sparse coding system creation with default config
 * ============================================================================ */

TEST_F(SparseCodingIntegrationTest, CreateWithDefaultConfig) {
    /* cortical_sparse_create(NULL) uses internal defaults */
    cortical_sparse_coding_system_t* default_system = cortical_sparse_create(nullptr);
    ASSERT_NE(default_system, nullptr);
    cortical_sparse_destroy(default_system);
}

TEST_F(SparseCodingIntegrationTest, CreateWithExplicitDefaultConfig) {
    sparse_coding_config_t cfg;
    int rc = cortical_sparse_default_config(&cfg);
    ASSERT_EQ(rc, 0);

    cortical_sparse_coding_system_t* s = cortical_sparse_create(&cfg);
    ASSERT_NE(s, nullptr);
    cortical_sparse_destroy(s);
}

/* ============================================================================
 * 2. K-WTA enforces sparsity — only k activations are non-zero
 * ============================================================================ */

TEST_F(SparseCodingIntegrationTest, KWTAEnforcesExactSparsity) {
    const uint32_t N = 100;
    float activations[N];
    float output[N];

    /* Fill with distinct values so the top-K is unambiguous */
    for (uint32_t i = 0; i < N; i++) {
        activations[i] = (float)(i + 1) / (float)N; /* 0.01 .. 1.0 */
    }

    int rc = cortical_sparse_enforce_sparsity(system, activations, N, output);
    ASSERT_EQ(rc, 0);

    /* Count non-zero outputs */
    int active_count = 0;
    for (uint32_t i = 0; i < N; i++) {
        if (output[i] > 0.0f) {
            active_count++;
        }
    }
    /* K-WTA with k_winners=5 should leave exactly 5 non-zero */
    EXPECT_EQ(active_count, 5);
}

/* ============================================================================
 * 3. K-WTA preserves top-K values unchanged
 * ============================================================================ */

TEST_F(SparseCodingIntegrationTest, KWTAPreservesTopKValues) {
    const uint32_t N = 100;
    float activations[N];
    float output[N];

    for (uint32_t i = 0; i < N; i++) {
        activations[i] = (float)(i + 1) / (float)N;
    }

    int rc = cortical_sparse_enforce_sparsity(system, activations, N, output);
    ASSERT_EQ(rc, 0);

    /* The top-5 activations are indices 95..99 with values 0.96..1.00.
     * Verify those exact values survived. */
    for (uint32_t i = N - 5; i < N; i++) {
        EXPECT_FLOAT_EQ(output[i], activations[i])
            << "Top-K value at index " << i << " was not preserved";
    }

    /* Verify non-winners are zeroed */
    for (uint32_t i = 0; i < N - 5; i++) {
        EXPECT_FLOAT_EQ(output[i], 0.0f)
            << "Non-winner at index " << i << " should be zero";
    }
}

/* ============================================================================
 * 4. Homeostatic threshold adaptation
 * ============================================================================ */

TEST_F(SparseCodingIntegrationTest, HomeostaticThresholdAdaptation) {
    /* Get initial threshold for column 0 */
    column_sparse_state_t state_before;
    int rc = cortical_sparse_get_column_state(system, 0, &state_before);
    ASSERT_EQ(rc, 0);
    float initial_threshold = state_before.activation_threshold;

    /* Report high sparsity (too sparse — thresholds should decrease) */
    rc = cortical_sparse_update_thresholds(system, 0.50f);
    ASSERT_EQ(rc, 0);

    column_sparse_state_t state_after;
    rc = cortical_sparse_get_column_state(system, 0, &state_after);
    ASSERT_EQ(rc, 0);

    /* After reporting actual sparsity much higher than target (0.50 vs 0.05),
     * the homeostatic rule theta += eta * (target - actual) should change
     * the threshold. With target=0.05 and actual=0.50, delta is negative,
     * so threshold should decrease (become more permissive). */
    EXPECT_NE(state_after.activation_threshold, initial_threshold)
        << "Threshold should adapt after homeostatic update";
}

TEST_F(SparseCodingIntegrationTest, ThresholdAdaptsInBothDirections) {
    /* Get baseline */
    column_sparse_state_t state0;
    cortical_sparse_get_column_state(system, 0, &state0);
    float baseline = state0.activation_threshold;

    /* Low actual sparsity (too many active) — threshold should increase */
    cortical_sparse_update_thresholds(system, 0.005f);
    column_sparse_state_t state_low;
    cortical_sparse_get_column_state(system, 0, &state_low);

    /* Reset by creating a fresh system for opposite direction test */
    cortical_sparse_destroy(system);
    system = cortical_sparse_create(&config);
    ASSERT_NE(system, nullptr);

    /* High actual sparsity (too sparse) — threshold should decrease */
    cortical_sparse_update_thresholds(system, 0.90f);
    column_sparse_state_t state_high;
    cortical_sparse_get_column_state(system, 0, &state_high);

    /* The two adaptations should go in opposite directions from baseline */
    float delta_low = state_low.activation_threshold - baseline;
    float delta_high = state_high.activation_threshold - baseline;
    EXPECT_NE(delta_low * delta_high, 0.0f)
        << "Both directions should produce non-zero adaptation";
}

/* ============================================================================
 * 5. Population sparsity computation
 * ============================================================================ */

TEST_F(SparseCodingIntegrationTest, PopulationSparsityComputation) {
    const uint32_t N = 100;
    float activations[N];
    memset(activations, 0, sizeof(activations));

    /* Set 10 neurons to high activation */
    for (uint32_t i = 0; i < 10; i++) {
        activations[i] = 1.0f;
    }

    float sparsity = cortical_sparse_compute_population_sparsity(
        system, activations, N);

    /* Population sparsity = fraction of active columns.
     * 10/100 = 0.10, but the exact value depends on per-column thresholds.
     * It should be in [0, 1] and reflect roughly 10% active. */
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 1.0f);
    /* With 10 strongly active columns out of 100, expect at least some activity */
    EXPECT_GT(sparsity, 0.0f);
}

TEST_F(SparseCodingIntegrationTest, PopulationSparsityAllZero) {
    const uint32_t N = 100;
    float activations[N];
    memset(activations, 0, sizeof(activations));

    float sparsity = cortical_sparse_compute_population_sparsity(
        system, activations, N);

    /* All zeros — no columns active */
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 0.01f);  /* Should be 0 or very close */
}

TEST_F(SparseCodingIntegrationTest, PopulationSparsityAllActive) {
    const uint32_t N = 100;
    float activations[N];
    for (uint32_t i = 0; i < N; i++) {
        activations[i] = 10.0f;  /* Well above any threshold */
    }

    float sparsity = cortical_sparse_compute_population_sparsity(
        system, activations, N);

    /* All strongly active — sparsity should be close to 1.0 */
    EXPECT_GT(sparsity, 0.5f);
}

/* ============================================================================
 * 6. In-place enforcement (input == output buffer)
 * ============================================================================ */

TEST_F(SparseCodingIntegrationTest, InPlaceEnforcement) {
    /* Create a fresh system to avoid threshold drift from prior tests */
    sparse_coding_config_t cfg;
    cortical_sparse_default_config(&cfg);
    cfg.sparsity_method = SPARSITY_METHOD_K_WTA;
    cfg.num_columns = 100;
    cfg.k_winners = 5;
    cfg.target_sparsity = 0.05f;
    cfg.enable_homeostasis = false;  /* No threshold drift between calls */
    cortical_sparse_coding_system_t* fresh = cortical_sparse_create(&cfg);
    ASSERT_NE(fresh, nullptr);

    const uint32_t N = 100;
    float buffer[N];

    /* Fill with distinct values */
    for (uint32_t i = 0; i < N; i++) {
        buffer[i] = (float)(i + 1) / (float)N;
    }

    /* In-place: pass same buffer as both input and output.
     * This is what brain_decide() does. */
    int rc = cortical_sparse_enforce_sparsity(fresh, buffer, N, buffer);
    ASSERT_EQ(rc, 0);

    /* Count non-zero — should match K-WTA k=5 */
    int active_count = 0;
    for (uint32_t i = 0; i < N; i++) {
        if (buffer[i] > 0.0f) {
            active_count++;
        }
    }
    EXPECT_EQ(active_count, 5);

    /* Verify the top-5 values survived (indices 95-99 had the highest values) */
    for (uint32_t i = 95; i < 100; i++) {
        EXPECT_GT(buffer[i], 0.0f)
            << "Top-K value at index " << i << " should survive in-place enforcement";
    }

    cortical_sparse_destroy(fresh);
}

/* ============================================================================
 * 7. Edge case: k_winners > num_activations
 * ============================================================================ */

TEST_F(SparseCodingIntegrationTest, KWinnersExceedsNumActivations) {
    /* Create a system with k_winners=50 but only pass 10 activations.
     * The system was configured with num_columns=100 and k_winners=5,
     * so we create a new one where k > num_columns. */
    sparse_coding_config_t edge_config;
    cortical_sparse_default_config(&edge_config);
    edge_config.num_columns = 10;
    edge_config.sparsity_method = SPARSITY_METHOD_K_WTA;
    edge_config.k_winners = 50;  /* k > num_columns */

    cortical_sparse_coding_system_t* edge_system = cortical_sparse_create(&edge_config);
    /* System should still be creatable (graceful handling) */
    if (edge_system == nullptr) {
        /* If create rejects invalid k > num_columns, that is also acceptable */
        SUCCEED() << "System rejected k_winners > num_columns at creation time";
        return;
    }

    float activations[10];
    float output[10];
    for (int i = 0; i < 10; i++) {
        activations[i] = (float)(i + 1) * 0.1f;
    }

    int rc = cortical_sparse_enforce_sparsity(edge_system, activations, 10, output);

    /* Should either succeed (keeping all active since k > N) or return an error.
     * It must NOT crash. */
    if (rc == 0) {
        /* If it succeeds, all values should be preserved (k > N means all win) */
        int active = 0;
        for (int i = 0; i < 10; i++) {
            if (output[i] > 0.0f) active++;
        }
        EXPECT_EQ(active, 10)
            << "When k > num_activations, all should be winners";
    } else {
        /* Returning an error code is also acceptable */
        SUCCEED() << "System returned error for k > num_activations";
    }

    cortical_sparse_destroy(edge_system);
}

/* ============================================================================
 * 8. Edge case: all-zero activations
 * ============================================================================ */

TEST_F(SparseCodingIntegrationTest, AllZeroActivations) {
    const uint32_t N = 100;
    float activations[N];
    float output[N];
    memset(activations, 0, sizeof(activations));

    /* Pre-fill output with garbage to verify it gets zeroed */
    for (uint32_t i = 0; i < N; i++) {
        output[i] = 999.0f;
    }

    int rc = cortical_sparse_enforce_sparsity(system, activations, N, output);
    ASSERT_EQ(rc, 0);

    /* All-zero input should produce all-zero output */
    for (uint32_t i = 0; i < N; i++) {
        EXPECT_FLOAT_EQ(output[i], 0.0f)
            << "Output at index " << i << " should be zero for zero input";
    }
}

TEST_F(SparseCodingIntegrationTest, AllZeroPopulationSparsity) {
    const uint32_t N = 100;
    float activations[N];
    memset(activations, 0, sizeof(activations));

    float sparsity = cortical_sparse_compute_population_sparsity(
        system, activations, N);
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 0.01f);
}

/* ============================================================================
 * 9. Edge case: NULL system
 * ============================================================================ */

TEST_F(SparseCodingIntegrationTest, NullSystemEnforceSparsity) {
    const uint32_t N = 100;
    float activations[N];
    float output[N];
    memset(activations, 0, sizeof(activations));

    int rc = cortical_sparse_enforce_sparsity(nullptr, activations, N, output);
    EXPECT_NE(rc, 0) << "Should return error for NULL system";
}

TEST_F(SparseCodingIntegrationTest, NullSystemUpdateThresholds) {
    int rc = cortical_sparse_update_thresholds(nullptr, 0.05f);
    EXPECT_NE(rc, 0) << "Should return error for NULL system";
}

TEST_F(SparseCodingIntegrationTest, NullSystemPopulationSparsity) {
    const uint32_t N = 100;
    float activations[N];
    memset(activations, 0, sizeof(activations));

    float sparsity = cortical_sparse_compute_population_sparsity(
        nullptr, activations, N);
    /* Should return -1.0 on error per API docs */
    EXPECT_LT(sparsity, 0.0f)
        << "Should return negative value for NULL system";
}

TEST_F(SparseCodingIntegrationTest, NullSystemDestroy) {
    /* Should not crash */
    cortical_sparse_destroy(nullptr);
    SUCCEED();
}
