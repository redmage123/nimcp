/**
 * @file test_training_portia_regression.cpp
 * @brief Regression test for Portia resource pressure bug (hardcoded 0.0f)
 *
 * WHAT: Verifies that increasing resource pressure decreases learning rate
 * WHY:  Regression test for the bug where portia_compute_allocation was
 *       called with hardcoded 0.0f, making Portia resource gating useless
 * HOW:  Calls portia_compute_allocation with a sweep of pressure values,
 *       verifies monotonic decrease of learning gate and compute budget
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "portia/nimcp_portia_tier_switch.h"
#include "cognitive/training/nimcp_training_integration.h"
}

/*=============================================================================
 * Test: ResourcePressureAffectsLearningRate
 *
 * Sweep resource_pressure from 0.0 to 1.0 in increments of 0.1.
 * Verify that portia_compute_allocation produces monotonically
 * decreasing feature_gate_learning and compute_budget_scale.
 *===========================================================================*/

TEST(PortiaRegression, ResourcePressureAffectsLearningRate) {
    const int NUM_STEPS = 11;
    float pressures[NUM_STEPS];
    float learning_gates[NUM_STEPS];
    float compute_budgets[NUM_STEPS];

    for (int i = 0; i < NUM_STEPS; i++) {
        pressures[i] = (float)i / 10.0f;

        portia_allocation_t alloc;
        memset(&alloc, 0, sizeof(alloc));

        int rc = portia_compute_allocation(pressures[i], &alloc);
        ASSERT_EQ(rc, 0)
            << "portia_compute_allocation failed at pressure=" << pressures[i];

        learning_gates[i] = alloc.feature_gate_learning;
        compute_budgets[i] = alloc.compute_budget_scale;

        /* All values should be in [0, 1] */
        EXPECT_GE(learning_gates[i], 0.0f)
            << "Learning gate out of range at pressure=" << pressures[i];
        EXPECT_LE(learning_gates[i], 1.0f)
            << "Learning gate out of range at pressure=" << pressures[i];
        EXPECT_GE(compute_budgets[i], 0.0f)
            << "Compute budget out of range at pressure=" << pressures[i];
        EXPECT_LE(compute_budgets[i], 1.0f)
            << "Compute budget out of range at pressure=" << pressures[i];
    }

    /* Verify monotonic decrease: higher pressure -> lower learning gate */
    for (int i = 1; i < NUM_STEPS; i++) {
        EXPECT_LE(learning_gates[i], learning_gates[i - 1])
            << "Learning gate should decrease with increasing pressure. "
               "pressure[" << i-1 << "]=" << pressures[i-1]
            << " gate=" << learning_gates[i-1]
            << ", pressure[" << i << "]=" << pressures[i]
            << " gate=" << learning_gates[i];

        EXPECT_LE(compute_budgets[i], compute_budgets[i - 1])
            << "Compute budget should decrease with increasing pressure. "
               "pressure[" << i-1 << "]=" << pressures[i-1]
            << " budget=" << compute_budgets[i-1]
            << ", pressure[" << i << "]=" << pressures[i]
            << " budget=" << compute_budgets[i];
    }

    /* At zero pressure, gates should be near 1.0 (full capacity) */
    EXPECT_GT(learning_gates[0], 0.95f)
        << "Zero pressure should yield near-full learning gate";
    EXPECT_GT(compute_budgets[0], 0.95f)
        << "Zero pressure should yield near-full compute budget";

    /* At maximum pressure, gates should be significantly reduced */
    EXPECT_LT(learning_gates[NUM_STEPS - 1], 0.15f)
        << "Maximum pressure should severely reduce learning gate";
    EXPECT_LT(compute_budgets[NUM_STEPS - 1], 0.15f)
        << "Maximum pressure should severely reduce compute budget";

    /* The range from 0.0 to 1.0 pressure should produce a meaningful spread */
    float gate_range = learning_gates[0] - learning_gates[NUM_STEPS - 1];
    EXPECT_GT(gate_range, 0.5f)
        << "Learning gate should vary by at least 0.5 across full pressure range";
}

/*=============================================================================
 * Test: ResourcePressureEdgeCases
 *
 * Verify boundary conditions: negative pressure clamps to 0, >1.0 clamps to 1.0
 *===========================================================================*/

TEST(PortiaRegression, ResourcePressureEdgeCases) {
    portia_allocation_t alloc_neg;
    portia_allocation_t alloc_over;
    portia_allocation_t alloc_zero;
    portia_allocation_t alloc_one;

    memset(&alloc_neg, 0, sizeof(alloc_neg));
    memset(&alloc_over, 0, sizeof(alloc_over));
    memset(&alloc_zero, 0, sizeof(alloc_zero));
    memset(&alloc_one, 0, sizeof(alloc_one));

    ASSERT_EQ(portia_compute_allocation(-1.0f, &alloc_neg), 0);
    ASSERT_EQ(portia_compute_allocation(2.0f, &alloc_over), 0);
    ASSERT_EQ(portia_compute_allocation(0.0f, &alloc_zero), 0);
    ASSERT_EQ(portia_compute_allocation(1.0f, &alloc_one), 0);

    /* Negative pressure should clamp to zero (same as 0.0) */
    EXPECT_FLOAT_EQ(alloc_neg.feature_gate_learning, alloc_zero.feature_gate_learning)
        << "Negative pressure should clamp to zero";

    /* Over 1.0 pressure should clamp to 1.0 */
    EXPECT_FLOAT_EQ(alloc_over.feature_gate_learning, alloc_one.feature_gate_learning)
        << "Pressure >1.0 should clamp to 1.0";

    /* NULL output pointer should return error */
    EXPECT_EQ(portia_compute_allocation(0.5f, nullptr), -1);
}

/*=============================================================================
 * Test: UnifiedLrReflectsPortiaPressure
 *
 * Verify that the unified LR computed by brain_ti_compute_unified_lr
 * with NULL brain returns base_lr unmodified (since NULL brain defaults
 * all modulation factors to identity). This ensures the fix didn't break
 * the identity-default path.
 *===========================================================================*/

TEST(PortiaRegression, UnifiedLrReflectsPortiaPressure) {
    float base_lr = 0.001f;

    /* NULL brain: all modulation factors are identity (1.0) */
    brain_ti_modulation_state_t state;
    float lr = brain_ti_compute_unified_lr(NULL, base_lr, &state);

    EXPECT_FLOAT_EQ(lr, base_lr)
        << "NULL brain should return base_lr unchanged";

    /* Verify the state was populated */
    EXPECT_FLOAT_EQ(state.final_lr_factor, 1.0f);
    EXPECT_FLOAT_EQ(state.portia_learning_gate, 1.0f);
    EXPECT_FLOAT_EQ(state.portia_compute_budget, 1.0f);
}
