/**
 * @file test_snn_basket.cpp
 * @brief Unit tests for the SNN fast-spiking basket cell pool.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "snn/nimcp_snn_basket.h"

class SnnBasketTest : public ::testing::Test {
protected:
    snn_basket_pool_t* bp = nullptr;

    void TearDown() override {
        if (bp) {
            snn_basket_pool_destroy(bp);
            bp = nullptr;
        }
    }
};

TEST_F(SnnBasketTest, CreateProducesExpectedCellCount) {
    bp = snn_basket_pool_create(7, 1000, 0.2f);
    ASSERT_NE(nullptr, bp);
    EXPECT_EQ(200u, bp->n_cells);
    EXPECT_EQ(7u, bp->parent_pop_id);
    EXPECT_NE(nullptr, bp->membrane_v);
    EXPECT_NE(nullptr, bp->refractory);
    EXPECT_NE(nullptr, bp->spike_output);
}

TEST_F(SnnBasketTest, CreateClampsFractionBelowMinimum) {
    bp = snn_basket_pool_create(0, 1000, 0.001f);
    ASSERT_NE(nullptr, bp);
    /* fraction clamped up to 0.01 → 10 cells */
    EXPECT_EQ(10u, bp->n_cells);
}

TEST_F(SnnBasketTest, CreateClampsFractionAboveMaximum) {
    bp = snn_basket_pool_create(0, 1000, 0.9f);
    ASSERT_NE(nullptr, bp);
    /* fraction clamped down to 0.5 → 500 cells */
    EXPECT_EQ(500u, bp->n_cells);
}

TEST_F(SnnBasketTest, CreateWithZeroParentReturnsNull) {
    snn_basket_pool_t* b = snn_basket_pool_create(0, 0, 0.2f);
    EXPECT_EQ(nullptr, b);
}

TEST_F(SnnBasketTest, CreateGuaranteesAtLeastOneCell) {
    /* Tiny parent + min fraction still yields at least one cell. */
    bp = snn_basket_pool_create(0, 10, 0.01f);
    ASSERT_NE(nullptr, bp);
    EXPECT_GE(bp->n_cells, 1u);
}

TEST_F(SnnBasketTest, DestroyNullDoesNotCrash) {
    snn_basket_pool_destroy(nullptr);
    SUCCEED();
}

TEST_F(SnnBasketTest, ResetZerosSpikesAndDrive) {
    bp = snn_basket_pool_create(0, 100, 0.2f);
    ASSERT_NE(nullptr, bp);

    /* Dirty the state. */
    for (uint32_t i = 0; i < bp->n_cells; ++i) {
        bp->membrane_v[i] = 0.0f;
        bp->refractory[i] = 3.0f;
        bp->spike_output[i] = 1.0f;
    }
    bp->drive_filtered = 0.5f;

    snn_basket_pool_reset(bp);

    for (uint32_t i = 0; i < bp->n_cells; ++i) {
        EXPECT_FLOAT_EQ(bp->v_rest, bp->membrane_v[i]);
        EXPECT_FLOAT_EQ(0.0f, bp->refractory[i]);
        EXPECT_FLOAT_EQ(0.0f, bp->spike_output[i]);
    }
    EXPECT_FLOAT_EQ(0.0f, bp->drive_filtered);
}

TEST_F(SnnBasketTest, StepWithPositiveDriveEventuallyFires) {
    bp = snn_basket_pool_create(0, 500, 0.2f);
    ASSERT_NE(nullptr, bp);

    bool any_spike_seen = false;
    for (int step = 0; step < 50; ++step) {
        snn_basket_pool_step(bp, 0.5f, 1.0f);
        float rate = snn_basket_pool_mean_rate(bp);
        if (rate > 0.0f) {
            any_spike_seen = true;
            break;
        }
    }
    EXPECT_TRUE(any_spike_seen);
}

TEST_F(SnnBasketTest, StepWithZeroDriveProducesNoSpikes) {
    bp = snn_basket_pool_create(0, 500, 0.2f);
    ASSERT_NE(nullptr, bp);
    snn_basket_pool_reset(bp);

    for (int step = 0; step < 100; ++step) {
        snn_basket_pool_step(bp, 0.0f, 1.0f);
        EXPECT_FLOAT_EQ(0.0f, snn_basket_pool_mean_rate(bp));
    }
}

TEST_F(SnnBasketTest, EmitInhibitionAppliesUniformProduct) {
    bp = snn_basket_pool_create(0, 100, 0.2f);
    ASSERT_NE(nullptr, bp);

    /* Force a known spike mean of 0.5 — half firing. */
    for (uint32_t i = 0; i < bp->n_cells; ++i) {
        bp->spike_output[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }
    const float expected_mean = 0.5f;
    const float expected_delta = bp->gain_inhib_to_parent * expected_mean;

    const uint32_t parent_n = 64;
    std::vector<float> parent_I_syn(parent_n, 0.0f);

    snn_basket_pool_emit_inhibition(bp, parent_I_syn.data(), parent_n);

    for (uint32_t n = 0; n < parent_n; ++n) {
        EXPECT_FLOAT_EQ(expected_delta, parent_I_syn[n]);
    }
}

TEST_F(SnnBasketTest, EmitInhibitionColdStartInjectsZero) {
    bp = snn_basket_pool_create(0, 100, 0.2f);
    ASSERT_NE(nullptr, bp);
    /* Fresh pool — no step called — spike_output all zero. */

    const uint32_t parent_n = 32;
    std::vector<float> parent_I_syn(parent_n, 0.0f);

    snn_basket_pool_emit_inhibition(bp, parent_I_syn.data(), parent_n);

    for (uint32_t n = 0; n < parent_n; ++n) {
        EXPECT_FLOAT_EQ(0.0f, parent_I_syn[n]);
    }
}

TEST_F(SnnBasketTest, MeanRateInUnitInterval) {
    bp = snn_basket_pool_create(0, 200, 0.2f);
    ASSERT_NE(nullptr, bp);

    /* After reset: 0. */
    EXPECT_FLOAT_EQ(0.0f, snn_basket_pool_mean_rate(bp));

    /* Drive strongly then poll repeatedly. */
    for (int step = 0; step < 100; ++step) {
        snn_basket_pool_step(bp, 0.8f, 1.0f);
        float r = snn_basket_pool_mean_rate(bp);
        EXPECT_GE(r, 0.0f);
        EXPECT_LE(r, 1.0f);
    }
}

TEST_F(SnnBasketTest, DisabledInhibGainMakesEmitNoop) {
    bp = snn_basket_pool_create(0, 100, 0.2f);
    ASSERT_NE(nullptr, bp);

    /* Force non-trivial spike mean so delta would be non-zero if gain != 0. */
    for (uint32_t i = 0; i < bp->n_cells; ++i) {
        bp->spike_output[i] = 1.0f;
    }
    bp->gain_inhib_to_parent = 0.0f;

    const uint32_t parent_n = 32;
    std::vector<float> parent_I_syn(parent_n, 0.0f);

    snn_basket_pool_emit_inhibition(bp, parent_I_syn.data(), parent_n);

    for (uint32_t n = 0; n < parent_n; ++n) {
        EXPECT_FLOAT_EQ(0.0f, parent_I_syn[n]);
    }
}

TEST_F(SnnBasketTest, EmitInhibitionNullSafeInputs) {
    bp = snn_basket_pool_create(0, 100, 0.2f);
    ASSERT_NE(nullptr, bp);

    /* Null parent buffer must not crash. */
    snn_basket_pool_emit_inhibition(bp, nullptr, 32);

    /* Null basket pool must not crash. */
    std::vector<float> parent_I_syn(32, 0.0f);
    snn_basket_pool_emit_inhibition(nullptr, parent_I_syn.data(), 32);
    SUCCEED();
}
