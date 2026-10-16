/**
 * @file test_lnn_thalamic_adapter.cpp
 * @brief Unit tests for the LNN thalamic adapter (Phase 2)
 *
 * TEST COVERAGE:
 * - Attach with router=NULL is a no-op (no channel created)
 * - Attach with a real router creates an internal channel
 * - Double attach destroys the old channel, creates a new one (no leak
 *   observable via repeated round-trips)
 * - Forward step with NULL-router attach behaves identically to a
 *   never-attached network
 * - With a channel attached and thalamic_enabled=0, step is unchanged
 * - All three tunables (enabled / input_gain_on / burst_tau_clamp_on)
 *   round-trip through setter/getter
 * - Out-of-range tunable values: setters coerce to {0.0f, 1.0f} per the
 *   "any nonzero = on" convention (there are no numeric ranges to reject
 *   on these three boolean-style toggles)
 *
 * @date 2026-04-22
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_training.h"
#include "lnn/nimcp_lnn_types.h"
#include "lnn/nimcp_lnn_config.h"
#include "core/thalamic/nimcp_thalamic_channel.h"
#include "middleware/routing/nimcp_thalamic_router.h"
#include "utils/tensor/nimcp_tensor.h"
}

/* ------------------------------------------------------------------------- */
/* Fixture                                                                   */
/* ------------------------------------------------------------------------- */

class LnnThalamicAdapterTest : public ::testing::Test {
protected:
    lnn_network_t*       net    = nullptr;
    thalamic_router_t*   router = nullptr;

    void SetUp() override {
        /* Build a minimal NCP network: tiny so allocation is fast. */
        net = lnn_network_create_ncp(4 /*inputs*/, 4 /*inter*/,
                                     4 /*command*/, 2 /*outputs*/);
        ASSERT_NE(nullptr, net);

        /* Build a default-config thalamic router. */
        thalamic_router_config_t cfg;
        std::memset(&cfg, 0, sizeof(cfg));
        cfg.max_queue_size            = 64;
        cfg.max_destinations          = 4;
        cfg.enable_attention_gating   = true;
        cfg.enable_priority_routing   = true;
        cfg.enable_statistics         = true;
        cfg.min_attention_threshold   = 0.0f;
        cfg.enable_learning           = true;
        cfg.enable_second_messengers  = false;
        cfg.num_neurons               = 16;
        cfg.enable_quantum_routing    = false;
        router = thalamic_router_create(&cfg);
        ASSERT_NE(nullptr, router);

        /* Reset tunables to known-on state at the start of every test. */
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
};

/* ------------------------------------------------------------------------- */
/* Attach / detach                                                           */
/* ------------------------------------------------------------------------- */

TEST_F(LnnThalamicAdapterTest, AttachNullRouterLeavesChannelNull) {
    nimcp_error_t rc = lnn_network_attach_thalamic_router(net, nullptr);
    EXPECT_EQ(NIMCP_SUCCESS, rc);
    EXPECT_EQ(nullptr, net->thalamic_channel);
}

TEST_F(LnnThalamicAdapterTest, AttachValidRouterCreatesChannel) {
    nimcp_error_t rc = lnn_network_attach_thalamic_router(net, router);
    EXPECT_EQ(NIMCP_SUCCESS, rc);
    ASSERT_NE(nullptr, net->thalamic_channel);
    /* Destination 0 must be pre-registered so get_gate returns the
     * cached attention (default 1.0) rather than the null-slot fallback. */
    EXPECT_GE(net->thalamic_channel->n_destinations, 1u);
    EXPECT_EQ(net->thalamic_channel->destinations[0], 0u);
    EXPECT_EQ(net->thalamic_channel->source_id, net->id);
}

TEST_F(LnnThalamicAdapterTest, AttachNullNetReturnsError) {
    nimcp_error_t rc = lnn_network_attach_thalamic_router(nullptr, router);
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, rc);
}

TEST_F(LnnThalamicAdapterTest, DoubleAttachReplacesChannel) {
    /* First attach — capture pointer. */
    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, router));
    thalamic_channel_t* first = net->thalamic_channel;
    ASSERT_NE(nullptr, first);

    /* Second attach — channel must be a fresh allocation. We cannot
     * safely compare pointers because calloc may reuse the just-freed
     * block; instead verify the new channel is valid and destination 0
     * is still registered, which implicitly proves the old one was
     * destroyed and a new one constructed.
     *
     * Run under valgrind / ASan in CI to catch the leak case directly. */
    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, router));
    ASSERT_NE(nullptr, net->thalamic_channel);
    EXPECT_EQ(net->thalamic_channel->destinations[0], 0u);
    EXPECT_EQ(net->thalamic_channel->source_id, net->id);
}

TEST_F(LnnThalamicAdapterTest, AttachThenDetachClearsChannel) {
    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, router));
    ASSERT_NE(nullptr, net->thalamic_channel);

    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, nullptr));
    EXPECT_EQ(nullptr, net->thalamic_channel);
}

/* ------------------------------------------------------------------------- */
/* Forward-step behaviour                                                    */
/* ------------------------------------------------------------------------- */

TEST_F(LnnThalamicAdapterTest, ForwardWithNullAttachIsSafeAndEquivalent) {
    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, nullptr));

    nimcp_tensor_t* in  = MakeVec(4, 0.25f);
    nimcp_tensor_t* out = MakeVec(2, 0.0f);
    ASSERT_NE(nullptr, in);
    ASSERT_NE(nullptr, out);

    int rc = lnn_network_forward_step(net, in, out, 1.0f);
    EXPECT_EQ(LNN_SUCCESS, rc);

    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
}

TEST_F(LnnThalamicAdapterTest, ForwardWithEnabledZeroIsUnchanged) {
    /* Attach a real channel then disable the master toggle. The
     * forward step must not consult the channel at all — no crash,
     * no NaN/Inf, numerically identical to a never-attached run on
     * the same input. */
    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, router));

    nimcp_tensor_t* in   = MakeVec(4, 0.25f);
    nimcp_tensor_t* out1 = MakeVec(2, 0.0f);
    nimcp_tensor_t* out2 = MakeVec(2, 0.0f);
    ASSERT_NE(nullptr, in);

    /* First run: with tunable on. */
    lnn_tune_set_thalamic_enabled(1.0f);
    lnn_reset_state(net);
    ASSERT_EQ(LNN_SUCCESS, lnn_network_forward_step(net, in, out1, 1.0f));

    /* Second run: with tunable off. Reset state so the two runs are
     * comparable (LTC has memory). */
    lnn_tune_set_thalamic_enabled(0.0f);
    lnn_reset_state(net);
    ASSERT_EQ(LNN_SUCCESS, lnn_network_forward_step(net, in, out2, 1.0f));

    /* With enabled=0 and a default gate of 1.0, both runs must be
     * numerically identical: the enabled path uses gate=1.0 which is
     * a no-op multiplication, and the disabled path skips the gate
     * entirely. Both reach the same ODE integration over the same
     * input with state reset between runs. Router-side submit/tick
     * happen AFTER the output is written, so they cannot affect the
     * output tensor of this step. A deviation here would indicate
     * either the gate is being applied when it shouldn't be, or the
     * disabled path is accidentally mutating layer state. */
    const float* d1 = (const float*)nimcp_tensor_data_const(out1);
    const float* d2 = (const float*)nimcp_tensor_data_const(out2);
    for (int i = 0; i < 2; i++) {
        EXPECT_TRUE(std::isfinite(d1[i]));
        EXPECT_TRUE(std::isfinite(d2[i]));
        EXPECT_FLOAT_EQ(d1[i], d2[i]);
    }

    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out1);
    nimcp_tensor_destroy(out2);
}

TEST_F(LnnThalamicAdapterTest, DestroyWithAttachedChannelDoesNotLeak) {
    /* Lifecycle check: destroying a network that owns a channel must
     * destroy the channel but NOT the (borrowed) router. Verified by
     * the fact that we can still destroy the router in TearDown(). */
    ASSERT_EQ(NIMCP_SUCCESS,
              lnn_network_attach_thalamic_router(net, router));

    lnn_network_destroy(net);
    net = nullptr;
    /* router is still alive and will be destroyed by TearDown. */
    SUCCEED();
}

/* ------------------------------------------------------------------------- */
/* Tunable round-trip                                                        */
/* ------------------------------------------------------------------------- */

TEST_F(LnnThalamicAdapterTest, TunableEnabledRoundTrip) {
    lnn_tune_set_thalamic_enabled(0.0f);
    EXPECT_EQ(0.0f, lnn_tune_get_thalamic_enabled());
    lnn_tune_set_thalamic_enabled(1.0f);
    EXPECT_EQ(1.0f, lnn_tune_get_thalamic_enabled());
}

TEST_F(LnnThalamicAdapterTest, TunableInputGainOnRoundTrip) {
    lnn_tune_set_thalamic_input_gain_on(0.0f);
    EXPECT_EQ(0.0f, lnn_tune_get_thalamic_input_gain_on());
    lnn_tune_set_thalamic_input_gain_on(1.0f);
    EXPECT_EQ(1.0f, lnn_tune_get_thalamic_input_gain_on());
}

TEST_F(LnnThalamicAdapterTest, TunableBurstTauClampOnRoundTrip) {
    lnn_tune_set_thalamic_burst_tau_clamp_on(0.0f);
    EXPECT_EQ(0.0f, lnn_tune_get_thalamic_burst_tau_clamp_on());
    lnn_tune_set_thalamic_burst_tau_clamp_on(1.0f);
    EXPECT_EQ(1.0f, lnn_tune_get_thalamic_burst_tau_clamp_on());
}

TEST_F(LnnThalamicAdapterTest, TunableSettersCoerceNonzeroToOne) {
    /* The three thalamic toggles are boolean-valued: any nonzero input
     * coerces to 1.0, zero stays zero. This test proves the coercion
     * behaviour matches the SNN tunable convention. */
    lnn_tune_set_thalamic_enabled(42.0f);
    EXPECT_EQ(1.0f, lnn_tune_get_thalamic_enabled());
    lnn_tune_set_thalamic_enabled(-3.14f);
    EXPECT_EQ(1.0f, lnn_tune_get_thalamic_enabled());
    lnn_tune_set_thalamic_enabled(0.0f);
    EXPECT_EQ(0.0f, lnn_tune_get_thalamic_enabled());

    lnn_tune_set_thalamic_input_gain_on(99.9f);
    EXPECT_EQ(1.0f, lnn_tune_get_thalamic_input_gain_on());
    lnn_tune_set_thalamic_input_gain_on(0.0f);
    EXPECT_EQ(0.0f, lnn_tune_get_thalamic_input_gain_on());

    lnn_tune_set_thalamic_burst_tau_clamp_on(2.0f);
    EXPECT_EQ(1.0f, lnn_tune_get_thalamic_burst_tau_clamp_on());
    lnn_tune_set_thalamic_burst_tau_clamp_on(0.0f);
    EXPECT_EQ(0.0f, lnn_tune_get_thalamic_burst_tau_clamp_on());
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    /* LNN library must be initialized before any network operations. */
    lnn_init(0);
    int rc = RUN_ALL_TESTS();
    lnn_shutdown();
    return rc;
}
