/**
 * @file test_lnn_thalamic_compose.cpp
 * @brief Integration tests — LNN + thalamic adapter composition
 *
 * TEST COVERAGE:
 * - A router attached + BURST mode: forward step executes without crash
 *   and produces finite output.
 * - TONIC vs BURST: output trajectories over 50 steps differ measurably
 *   (burst-mode tau clamp changes the dynamics).
 * - Attach → detach → re-attach → forward: no crash, finite output.
 *
 * @date 2026-04-22
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_training.h"
#include "lnn/nimcp_lnn_types.h"
#include "core/thalamic/nimcp_thalamic_channel.h"
#include "middleware/routing/nimcp_thalamic_router.h"
#include "utils/tensor/nimcp_tensor.h"
}

/* ------------------------------------------------------------------------- */
/* Fixture                                                                   */
/* ------------------------------------------------------------------------- */

class LnnThalamicComposeTest : public ::testing::Test {
protected:
    lnn_network_t*     net    = nullptr;
    thalamic_router_t* router = nullptr;

    void SetUp() override {
        net = lnn_network_create_ncp(4, 6, 6, 2);
        ASSERT_NE(nullptr, net);

        thalamic_router_config_t cfg;
        std::memset(&cfg, 0, sizeof(cfg));
        cfg.max_queue_size            = 128;
        cfg.max_destinations          = 4;
        cfg.enable_attention_gating   = true;
        cfg.enable_priority_routing   = true;
        cfg.enable_statistics         = true;
        cfg.min_attention_threshold   = 0.0f;
        cfg.enable_learning           = true;
        cfg.num_neurons               = 32;
        router = thalamic_router_create(&cfg);
        ASSERT_NE(nullptr, router);

        /* Known-on defaults for every test. */
        lnn_tune_set_thalamic_enabled(1.0f);
        lnn_tune_set_thalamic_input_gain_on(1.0f);
        lnn_tune_set_thalamic_burst_tau_clamp_on(1.0f);
    }

    void TearDown() override {
        if (net) {
            lnn_network_destroy(net);
            net = nullptr;
        }
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
    }

    nimcp_tensor_t* MakeVec(uint32_t n, float v) {
        uint32_t dims[1] = { n };
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (t) {
            float* d = (float*)nimcp_tensor_data(t);
            for (uint32_t i = 0; i < n; i++) d[i] = v;
        }
        return t;
    }

    /* Drive n_steps forward passes with the same input, accumulating
     * the L2 norm of the output trajectory. A stable measure of
     * "dynamical footprint" that is sensitive to tau clamping. */
    float RunSteps(int n_steps, float input_value) {
        nimcp_tensor_t* in  = MakeVec(4, input_value);
        nimcp_tensor_t* out = MakeVec(2, 0.0f);
        float sum_sq = 0.0f;
        lnn_reset_state(net);
        for (int s = 0; s < n_steps; s++) {
            int rc = lnn_network_forward_step(net, in, out, 1.0f);
            EXPECT_EQ(LNN_SUCCESS, rc);
            const float* d = (const float*)nimcp_tensor_data_const(out);
            for (int i = 0; i < 2; i++) {
                sum_sq += d[i] * d[i];
            }
        }
        nimcp_tensor_destroy(in);
        nimcp_tensor_destroy(out);
        return std::sqrt(sum_sq);
    }
};

/* ------------------------------------------------------------------------- */
/* Tests                                                                     */
/* ------------------------------------------------------------------------- */

TEST_F(LnnThalamicComposeTest, BurstModeForwardExecutesAndIsFinite) {
    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, router));
    thalamic_channel_set_mode(net->thalamic_channel, THALAMIC_CHAN_BURST);

    nimcp_tensor_t* in  = MakeVec(4, 0.3f);
    nimcp_tensor_t* out = MakeVec(2, 0.0f);

    for (int s = 0; s < 10; s++) {
        ASSERT_EQ(LNN_SUCCESS,
                  lnn_network_forward_step(net, in, out, 1.0f));
        const float* d = (const float*)nimcp_tensor_data_const(out);
        for (int i = 0; i < 2; i++) {
            EXPECT_TRUE(std::isfinite(d[i]));
        }
    }

    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
}

TEST_F(LnnThalamicComposeTest, BurstVsTonicTrajectoriesDiffer) {
    /* TONIC trajectory — no tau clamp. */
    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, router));
    thalamic_channel_set_mode(net->thalamic_channel, THALAMIC_CHAN_TONIC);
    float tonic_norm = RunSteps(50, 0.3f);

    /* BURST trajectory — tau clamp to 1 ms for every layer every step. */
    thalamic_channel_set_mode(net->thalamic_channel, THALAMIC_CHAN_BURST);
    float burst_norm = RunSteps(50, 0.3f);

    /* Both should be finite. */
    EXPECT_TRUE(std::isfinite(tonic_norm));
    EXPECT_TRUE(std::isfinite(burst_norm));

    /* The trajectories should differ by at least a small margin. If
     * they were bit-identical the clamp wouldn't be observable. A 1e-6
     * threshold is well inside float noise but well below any real
     * tau-driven dynamical divergence. */
    EXPECT_GT(std::fabs(burst_norm - tonic_norm), 1e-6f)
        << "burst=" << burst_norm << " tonic=" << tonic_norm;
}

TEST_F(LnnThalamicComposeTest, AttachDetachReattachNoCrash) {
    /* Attach. */
    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, router));
    ASSERT_NE(nullptr, net->thalamic_channel);

    /* Detach. */
    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, nullptr));
    EXPECT_EQ(nullptr, net->thalamic_channel);

    /* Re-attach. */
    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, router));
    ASSERT_NE(nullptr, net->thalamic_channel);

    /* Forward step after the cycle — must still produce finite output. */
    nimcp_tensor_t* in  = MakeVec(4, 0.1f);
    nimcp_tensor_t* out = MakeVec(2, 0.0f);
    ASSERT_EQ(LNN_SUCCESS, lnn_network_forward_step(net, in, out, 1.0f));
    const float* d = (const float*)nimcp_tensor_data_const(out);
    for (int i = 0; i < 2; i++) {
        EXPECT_TRUE(std::isfinite(d[i]));
    }
    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    lnn_init(0);
    int rc = RUN_ALL_TESTS();
    lnn_shutdown();
    return rc;
}
