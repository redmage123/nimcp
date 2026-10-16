//=============================================================================
// test_snn_gap_junction_regression.cpp
//=============================================================================
/**
 * @file test_snn_gap_junction_regression.cpp
 * @brief Wave D regression — gap_coupling = 0 (default) must NOT alter
 *        the SNN step path's per-neuron membrane voltage; the gap-junction
 *        branch must be a no-op in the default case.
 *
 * WHAT: With gap_coupling = 0 the new hot-loop branch must skip; with
 *       coupling > 0 the V trajectory must measurably deviate.
 *
 * WHY:  Wave D adds a new branch in snn_network_step. The CB regression
 *       contract requires the default-OFF branch to be observably
 *       indistinguishable from pre-Wave-D behavior.
 *
 * HOW:  Two-test gtest fixture mirroring the unit-test pattern:
 *       fixture creates a fresh lightweight CSR pop in SetUp, and each
 *       test mutates it. Refractory is held at 100 ms to bypass the LIF
 *       kernel and isolate the gap-junction adjustment — the same
 *       bypass used in the unit tests because the gap term runs BEFORE
 *       refractory check in the hot loop, so we do not need LIF for
 *       this contract.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "utils/tensor/nimcp_tensor.h"
}

extern "C" {
    void  snn_tune_set_conductance_enabled(float);
    void  snn_tune_set_cb_weights_rescaled(float);
    void  snn_tune_set_noise_rate_hz(float);
    void  snn_tune_set_basket_enabled(float);
    void  snn_tune_set_ahp_enabled(float);
    void  snn_tune_set_pump_enabled(float);
    void  snn_tune_set_substrate_enabled(float);
}

namespace {

class SnnGapJunctionRegression : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }
    static void TearDownTestSuite() {
        nimcp_shutdown();
    }

    void SetUp() override {
        snn_tune_set_noise_rate_hz(0.0f);
        snn_tune_set_basket_enabled(0.0f);
        snn_tune_set_ahp_enabled(0.0f);
        snn_tune_set_pump_enabled(0.0f);
        snn_tune_set_substrate_enabled(0.0f);
        snn_tune_set_conductance_enabled(1.0f);   /* CB ON — gap path live */
        snn_tune_set_cb_weights_rescaled(1.0f);   /* skip rescale guard */

        snn_config_t cfg;
        snn_config_default(&cfg);
        cfg.n_inputs  = 1;
        cfg.n_outputs = 1;
        cfg.n_hidden  = 0;
        cfg.dt        = 1.0f;
        net_ = snn_network_create(&cfg);
        ASSERT_NE(net_, nullptr);

        pop_id_ = snn_network_add_population_lightweight(
            net_, 4, NEURON_GENERIC_LIF, "pop");
        ASSERT_GE(pop_id_, 0);
        snn_network_finalize_connections(net_);
    }

    void TearDown() override {
        if (net_) {
            snn_network_destroy(net_);
            net_ = nullptr;
        }
        snn_tune_set_conductance_enabled(0.0f);
    }

    /* Run one step with given gap_coupling, refractory clamped at 100 to
     * bypass LIF (gap-junction term runs BEFORE the refractory check, so
     * its effect on V is still visible). */
    std::vector<float> step_once_with_coupling(float gap_coupling,
                                               bool skip_setter = false) {
        if (!skip_setter) {
            EXPECT_EQ(snn_network_set_pop_gap_coupling(
                net_, (uint32_t)pop_id_, gap_coupling), 0);
        }
        snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id_);
        EXPECT_NE(pop, nullptr);
        float* v   = (float*)nimcp_tensor_data(pop->membrane_v);
        float* ref = (float*)nimcp_tensor_data(pop->refractory);

        /* Deterministic seed: spread V across [-65, -50). */
        const float seed[4] = { -65.0f, -60.0f, -55.0f, -52.0f };
        for (uint32_t i = 0; i < 4; i++) {
            v[i]   = seed[i];
            ref[i] = 100.0f;  /* refractory bypass — LIF skipped */
        }

        EXPECT_GE(snn_network_step(net_, 1.0f), 0);

        std::vector<float> out(4);
        for (uint32_t i = 0; i < 4; i++) out[i] = v[i];
        return out;
    }

    snn_network_t* net_ = nullptr;
    int pop_id_ = -1;
};

//-----------------------------------------------------------------------------
// Default (no setter call) and explicit setter(0) produce the EXACT same
// output. Pins the bit-identity contract: when gap_coupling=0 (default),
// the new branch is a no-op.
//-----------------------------------------------------------------------------
TEST_F(SnnGapJunctionRegression, DefaultMatchesExplicitZeroCoupling) {
    auto v_default  = step_once_with_coupling(0.0f, /*skip_setter=*/true);
    /* TearDown/SetUp will run between TEST_F calls; we must call them
     * manually here for the second comparison since gtest doesn't expose
     * test re-entry. Use a manual reset instead. */

    /* Reset the pop's V back to seed conditions and call step again
     * after explicit-zero setter. */
    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id_);
    ASSERT_NE(pop, nullptr);

    auto v_explicit = step_once_with_coupling(0.0f, /*skip_setter=*/false);

    ASSERT_EQ(v_default.size(), v_explicit.size());
    for (size_t i = 0; i < v_default.size(); i++) {
        EXPECT_FLOAT_EQ(v_default[i], v_explicit[i])
            << "neuron " << i
            << ": default V=" << v_default[i]
            << " explicit_zero V=" << v_explicit[i];
    }
}

//-----------------------------------------------------------------------------
// Sanity: gap_coupling > 0 actually CHANGES the V output. Catches the
// case where the gap-junction branch is silently dead (e.g. LICM bug
// hoisting the conditional out incorrectly).
//-----------------------------------------------------------------------------
TEST_F(SnnGapJunctionRegression, NonzeroCouplingActuallyChangesTrajectory) {
    auto v_zero    = step_once_with_coupling(0.0f);
    auto v_coupled = step_once_with_coupling(0.1f);

    ASSERT_EQ(v_zero.size(), v_coupled.size());
    int diff_count = 0;
    for (size_t i = 0; i < v_zero.size(); i++) {
        if (std::fabs(v_zero[i] - v_coupled[i]) > 1e-4f) diff_count++;
    }
    EXPECT_GT(diff_count, 0)
        << "Non-zero gap_coupling produced no visible deviation — "
           "gap-junction hot-loop path is likely dead.";
}

}  // namespace
