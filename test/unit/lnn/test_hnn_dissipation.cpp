/**
 * @file test_hnn_dissipation.cpp
 * @brief Unit tests for the HNN Rayleigh dissipation adapter.
 *
 * WHAT: Exercises the biologically-motivated, substrate-coupled damping term
 *       added to the Hamiltonian Neural Network. Conservation is preserved
 *       when dissipation is disabled (default) OR when no substrate pointer
 *       is supplied. When both enabled AND substrate-equipped, the momentum
 *       equation becomes dp/dt = -∂H/∂q - γ·p with γ = γ_max·(1 - capacity).
 * WHY:  Real neural dynamics dissipate energy under metabolic stress. Without
 *       tests that pin down the "no-op when disabled / no-op when substrate
 *       missing" branches, future refactors could silently break backward
 *       compatibility for every existing HNN caller in the codebase.
 * HOW:  Google Test. Build a tiny 2-dim H-network, probe derivatives via
 *       the substrate-aware forward, and integrate for multi-step decay.
 *
 * @date 2026-04-22
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "lnn/nimcp_lnn_hamiltonian.h"
#include "utils/tensor/nimcp_tensor.h"
#include "core/substrate/nimcp_substrate_effects.h"
}

/*============================================================================
 * Fixture: saves + restores the dissipation tunables across every test.
 *==========================================================================*/
class HNNDissipationTest : public ::testing::Test {
protected:
    float saved_enabled   = 0.0f;
    float saved_gamma_max = 0.0f;

    void SetUp() override {
        saved_enabled   = hnn_tune_get_dissipation_enabled();
        saved_gamma_max = hnn_tune_get_dissipation_gamma_max();

        /* Deterministic seed so Xavier weight draws are stable across runs. */
        srand(42);
    }

    void TearDown() override {
        hnn_tune_set_dissipation_enabled(saved_enabled);
        hnn_tune_set_dissipation_gamma_max(saved_gamma_max);
    }

    /* Tiny 2-dim HNN with modest hidden layer so forward/backward are cheap. */
    lnn_hamiltonian_net_t* MakeNet(uint32_t dim = 2) {
        lnn_hamiltonian_config_t cfg;
        lnn_hamiltonian_config_default(&cfg);
        cfg.hidden_dim = 8;
        cfg.n_hidden_layers = 2;
        cfg.separable = false;
        return lnn_hamiltonian_net_create(dim, &cfg);
    }

    nimcp_tensor_t* MakeVec(uint32_t n, float fill) {
        uint32_t dims[1] = {n};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!t) return nullptr;
        float* d = (float*)nimcp_tensor_data(t);
        for (uint32_t i = 0; i < n; i++) d[i] = fill;
        return t;
    }

    nimcp_tensor_t* MakeVec2(float a, float b) {
        uint32_t dims[1] = {2};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!t) return nullptr;
        float* d = (float*)nimcp_tensor_data(t);
        d[0] = a; d[1] = b;
        return t;
    }

    /* Fill a healthy axon_substrate_effects_t — all biological modulators at
     * their no-stress defaults (overall_capacity = 1.0). */
    static void FillHealthyAxon(axon_substrate_effects_t* a) {
        memset(a, 0, sizeof(*a));
        a->temperature_q10_factor  = 1.0f;
        a->atp_velocity_factor     = 1.0f;
        a->myelin_efficiency       = 1.0f;
        a->overall_velocity_mod    = 1.0f;
        a->ion_gradient_strength   = 1.0f;
        a->ap_amplitude_mod        = 1.0f;
        a->spike_reliability       = 1.0f;
        a->pump_activity           = 1.0f;
        a->refractory_period_mod   = 1.0f;
        a->transport_efficiency    = 1.0f;
        a->kinesin_activity        = 1.0f;
        a->membrane_capacitance_mod = 1.0f;
        a->membrane_leak_mod       = 1.0f;
        a->overall_capacity        = 1.0f;
    }

    static float L2(const float* v, uint32_t n) {
        float s = 0.0f;
        for (uint32_t i = 0; i < n; i++) s += v[i] * v[i];
        return std::sqrt(s);
    }
};

/*============================================================================
 * Test 1: Default (disabled) preserves exact conservation.
 *
 * When g_hnn_dissipation_enabled == 0.0 (default), dp must equal -∂H/∂q
 * exactly — no γ·p term, regardless of substrate pointer.
 *==========================================================================*/
TEST_F(HNNDissipationTest, DefaultDisabled_ExactConservation) {
    /* Explicitly ensure disabled — even though it's the default, fixture
     * ordering could race if a previous test left it on. */
    hnn_tune_set_dissipation_enabled(0.0f);
    EXPECT_FLOAT_EQ(hnn_tune_get_dissipation_enabled(), 0.0f);

    lnn_hamiltonian_net_t* net = MakeNet(2);
    ASSERT_NE(net, nullptr);

    nimcp_tensor_t* q  = MakeVec2(0.5f, -0.3f);
    nimcp_tensor_t* p  = MakeVec2(1.2f,  0.8f);
    nimcp_tensor_t* dq = MakeVec(2, 0.0f);
    nimcp_tensor_t* dp = MakeVec(2, 0.0f);

    /* Baseline: legacy forward (no substrate) */
    ASSERT_EQ(lnn_hamiltonian_forward(net, q, p, dq, dp), 0);
    float baseline_dp[2];
    memcpy(baseline_dp, nimcp_tensor_data_const(dp), 2 * sizeof(float));

    /* Even with a stressed substrate pointer, disabled flag must mean no
     * damping applied — dp identical to baseline. */
    axon_substrate_effects_t axon;
    FillHealthyAxon(&axon);
    axon.overall_capacity = 0.1f;  /* very stressed — would dissipate if on */

    nimcp_tensor_t* dq2 = MakeVec(2, 0.0f);
    nimcp_tensor_t* dp2 = MakeVec(2, 0.0f);
    ASSERT_EQ(lnn_hamiltonian_forward_with_substrate(net, q, p, dq2, dp2, &axon), 0);

    const float* dp2_data = (const float*)nimcp_tensor_data_const(dp2);
    EXPECT_FLOAT_EQ(dp2_data[0], baseline_dp[0]);
    EXPECT_FLOAT_EQ(dp2_data[1], baseline_dp[1]);

    nimcp_tensor_destroy(q);
    nimcp_tensor_destroy(p);
    nimcp_tensor_destroy(dq);
    nimcp_tensor_destroy(dp);
    nimcp_tensor_destroy(dq2);
    nimcp_tensor_destroy(dp2);
    lnn_hamiltonian_net_destroy(net);
}

/*============================================================================
 * Test 2: Enabled flag alone (no substrate) still leaves dp untouched.
 *
 * The legacy API path (lnn_hamiltonian_forward) implicitly passes axon_eff=NULL.
 * With dissipation enabled but no substrate, the damping precondition is
 * still not met — dp must equal the pure conservation case.
 *==========================================================================*/
TEST_F(HNNDissipationTest, EnabledButNoSubstrate_NoDissipation) {
    hnn_tune_set_dissipation_enabled(1.0f);
    hnn_tune_set_dissipation_gamma_max(0.5f);  /* arbitrary large γ_max */

    lnn_hamiltonian_net_t* net = MakeNet(2);
    ASSERT_NE(net, nullptr);

    nimcp_tensor_t* q  = MakeVec2(0.5f, -0.3f);
    nimcp_tensor_t* p  = MakeVec2(1.2f,  0.8f);
    nimcp_tensor_t* dq_on  = MakeVec(2, 0.0f);
    nimcp_tensor_t* dp_on  = MakeVec(2, 0.0f);
    nimcp_tensor_t* dq_off = MakeVec(2, 0.0f);
    nimcp_tensor_t* dp_off = MakeVec(2, 0.0f);

    /* Compute once with disabled to get the conservation reference. */
    hnn_tune_set_dissipation_enabled(0.0f);
    ASSERT_EQ(lnn_hamiltonian_forward(net, q, p, dq_off, dp_off), 0);

    /* Now enable and call the legacy API — no substrate is ever plumbed
     * to the gradient computation so dp must be identical. */
    hnn_tune_set_dissipation_enabled(1.0f);
    ASSERT_EQ(lnn_hamiltonian_forward(net, q, p, dq_on, dp_on), 0);

    const float* dp_on_data  = (const float*)nimcp_tensor_data_const(dp_on);
    const float* dp_off_data = (const float*)nimcp_tensor_data_const(dp_off);
    EXPECT_FLOAT_EQ(dp_on_data[0], dp_off_data[0]);
    EXPECT_FLOAT_EQ(dp_on_data[1], dp_off_data[1]);

    nimcp_tensor_destroy(q);
    nimcp_tensor_destroy(p);
    nimcp_tensor_destroy(dq_on);
    nimcp_tensor_destroy(dp_on);
    nimcp_tensor_destroy(dq_off);
    nimcp_tensor_destroy(dp_off);
    lnn_hamiltonian_net_destroy(net);
}

/*============================================================================
 * Test 3: Healthy substrate (overall_capacity = 1) keeps γ = 0 → no damping.
 *==========================================================================*/
TEST_F(HNNDissipationTest, EnabledWithHealthySubstrate_NoDissipation) {
    hnn_tune_set_dissipation_enabled(1.0f);
    hnn_tune_set_dissipation_gamma_max(0.1f);

    lnn_hamiltonian_net_t* net = MakeNet(2);
    ASSERT_NE(net, nullptr);

    nimcp_tensor_t* q  = MakeVec2(0.5f, -0.3f);
    nimcp_tensor_t* p  = MakeVec2(1.2f,  0.8f);
    nimcp_tensor_t* dq_healthy  = MakeVec(2, 0.0f);
    nimcp_tensor_t* dp_healthy  = MakeVec(2, 0.0f);
    nimcp_tensor_t* dq_nosub    = MakeVec(2, 0.0f);
    nimcp_tensor_t* dp_nosub    = MakeVec(2, 0.0f);

    /* Healthy axon substrate — overall_capacity = 1 → γ = γ_max·0 = 0. */
    axon_substrate_effects_t axon;
    FillHealthyAxon(&axon);
    ASSERT_EQ(axon.overall_capacity, 1.0f);

    ASSERT_EQ(lnn_hamiltonian_forward_with_substrate(
                 net, q, p, dq_healthy, dp_healthy, &axon), 0);
    /* Reference: same call with NULL substrate — pure conservation. */
    ASSERT_EQ(lnn_hamiltonian_forward_with_substrate(
                 net, q, p, dq_nosub, dp_nosub, nullptr), 0);

    const float* dph = (const float*)nimcp_tensor_data_const(dp_healthy);
    const float* dpn = (const float*)nimcp_tensor_data_const(dp_nosub);
    EXPECT_FLOAT_EQ(dph[0], dpn[0]);
    EXPECT_FLOAT_EQ(dph[1], dpn[1]);

    nimcp_tensor_destroy(q);
    nimcp_tensor_destroy(p);
    nimcp_tensor_destroy(dq_healthy);
    nimcp_tensor_destroy(dp_healthy);
    nimcp_tensor_destroy(dq_nosub);
    nimcp_tensor_destroy(dp_nosub);
    lnn_hamiltonian_net_destroy(net);
}

/*============================================================================
 * Test 4: Stressed substrate → dp shifted by exactly -γ·p.
 *
 * γ = γ_max · (1 - overall_capacity) = 0.1 · (1 - 0.3) = 0.07.
 * Expected: dp_with - dp_without = -0.07 · p (component-wise).
 *==========================================================================*/
TEST_F(HNNDissipationTest, EnabledWithStressedSubstrate_Dissipates) {
    hnn_tune_set_dissipation_enabled(1.0f);
    hnn_tune_set_dissipation_gamma_max(0.1f);

    lnn_hamiltonian_net_t* net = MakeNet(2);
    ASSERT_NE(net, nullptr);

    const float p0 = 1.2f;
    const float p1 = 0.8f;

    nimcp_tensor_t* q  = MakeVec2(0.5f, -0.3f);
    nimcp_tensor_t* p  = MakeVec2(p0, p1);
    nimcp_tensor_t* dq_nosub = MakeVec(2, 0.0f);
    nimcp_tensor_t* dp_nosub = MakeVec(2, 0.0f);
    nimcp_tensor_t* dq_sub   = MakeVec(2, 0.0f);
    nimcp_tensor_t* dp_sub   = MakeVec(2, 0.0f);

    /* Stressed substrate — capacity 0.3 → γ = 0.1 · 0.7 = 0.07. */
    axon_substrate_effects_t axon;
    FillHealthyAxon(&axon);
    axon.overall_capacity = 0.3f;

    /* Reference: no substrate → pure conservation, exact Hamilton's. */
    ASSERT_EQ(lnn_hamiltonian_forward_with_substrate(
                 net, q, p, dq_nosub, dp_nosub, nullptr), 0);
    /* Stressed: dp should have γ·p subtracted. */
    ASSERT_EQ(lnn_hamiltonian_forward_with_substrate(
                 net, q, p, dq_sub, dp_sub, &axon), 0);

    const float expected_gamma = 0.07f;
    const float* dps = (const float*)nimcp_tensor_data_const(dp_sub);
    const float* dpn = (const float*)nimcp_tensor_data_const(dp_nosub);

    EXPECT_NEAR(dps[0], dpn[0] - expected_gamma * p0, 1e-5f);
    EXPECT_NEAR(dps[1], dpn[1] - expected_gamma * p1, 1e-5f);

    /* dq must be unchanged — dissipation acts only on momentum. */
    const float* dqs = (const float*)nimcp_tensor_data_const(dq_sub);
    const float* dqn = (const float*)nimcp_tensor_data_const(dq_nosub);
    EXPECT_FLOAT_EQ(dqs[0], dqn[0]);
    EXPECT_FLOAT_EQ(dqs[1], dqn[1]);

    nimcp_tensor_destroy(q);
    nimcp_tensor_destroy(p);
    nimcp_tensor_destroy(dq_nosub);
    nimcp_tensor_destroy(dp_nosub);
    nimcp_tensor_destroy(dq_sub);
    nimcp_tensor_destroy(dp_sub);
    lnn_hamiltonian_net_destroy(net);
}

/*============================================================================
 * Test 5: Multi-step integration — momentum decays under sustained damping.
 *
 * With overall_capacity = 0.5 (γ = γ_max/2 = 0.05 for γ_max = 0.1) and a
 * reasonable dt, after ~100 steps the momentum magnitude should have
 * decayed well below its initial value. We use a fresh H-net with small
 * weight scale so that ∂H/∂q is small compared to the damping term, making
 * the decay dominated by Rayleigh friction.
 *==========================================================================*/
TEST_F(HNNDissipationTest, MomentumDecay_Simulated) {
    hnn_tune_set_dissipation_enabled(1.0f);
    hnn_tune_set_dissipation_gamma_max(0.5f);  /* strong damping so decay is clean */

    lnn_hamiltonian_net_t* net = MakeNet(2);
    ASSERT_NE(net, nullptr);

    /* Zero the H-network output layer's weights + biases so ∂H/∂q ≈ 0,
     * isolating the damping dynamics from any spurious Hamiltonian force.
     * The grad buffers stay zero which is fine — we never call apply_grad. */
    {
        uint32_t L = net->n_layers - 1;
        float* W = (float*)nimcp_tensor_data(net->W[L]);
        float* b = (float*)nimcp_tensor_data(net->b[L]);
        uint32_t wn = nimcp_tensor_numel(net->W[L]);
        uint32_t bn = nimcp_tensor_numel(net->b[L]);
        for (uint32_t i = 0; i < wn; i++) W[i] = 0.0f;
        for (uint32_t i = 0; i < bn; i++) b[i] = 0.0f;
    }

    nimcp_tensor_t* q = MakeVec2(0.0f, 0.0f);
    nimcp_tensor_t* p = MakeVec2(1.0f, 1.0f);  /* ||p_0|| = sqrt(2) ≈ 1.414 */

    /* Stressed — capacity = 0.5 → γ = 0.5 · 0.5 = 0.25. */
    axon_substrate_effects_t axon;
    FillHealthyAxon(&axon);
    axon.overall_capacity = 0.5f;

    const float* p_data = (const float*)nimcp_tensor_data_const(p);
    const float initial_norm = L2(p_data, 2);
    ASSERT_GT(initial_norm, 1.0f);

    /* Integrate 100 steps, dt = 0.1 — cumulative γ·dt·N = 0.25·0.1·100 = 2.5
     * so exp(-2.5) ≈ 0.082 (though symplectic+Rayleigh is slightly different,
     * the order-of-magnitude envelope holds easily). */
    const float dt = 0.1f;
    for (int step = 0; step < 100; step++) {
        int rc = lnn_hamiltonian_step_stormer_verlet_with_substrate(
            net, q, p, /* no input */ nullptr, dt, 0.0f, &axon);
        ASSERT_EQ(rc, 0) << "integrator failed at step " << step;
    }

    const float final_norm = L2(p_data, 2);
    EXPECT_LT(final_norm, initial_norm * 0.5f)
        << "expected strong decay; initial=" << initial_norm
        << " final=" << final_norm;

    nimcp_tensor_destroy(q);
    nimcp_tensor_destroy(p);
    lnn_hamiltonian_net_destroy(net);
}

/*============================================================================
 * Bonus: getter/setter round-trip + clamp coverage for coverage hygiene.
 *==========================================================================*/
TEST_F(HNNDissipationTest, TunableEnabledRoundTripsNonzeroToOne) {
    hnn_tune_set_dissipation_enabled(0.0f);
    EXPECT_FLOAT_EQ(hnn_tune_get_dissipation_enabled(), 0.0f);
    hnn_tune_set_dissipation_enabled(0.3f);
    EXPECT_FLOAT_EQ(hnn_tune_get_dissipation_enabled(), 1.0f);
    hnn_tune_set_dissipation_enabled(-4.0f);
    EXPECT_FLOAT_EQ(hnn_tune_get_dissipation_enabled(), 1.0f);
    hnn_tune_set_dissipation_enabled(0.0f);
    EXPECT_FLOAT_EQ(hnn_tune_get_dissipation_enabled(), 0.0f);
}

TEST_F(HNNDissipationTest, TunableGammaMaxClampsOutOfRange) {
    hnn_tune_set_dissipation_gamma_max(0.3f);
    EXPECT_FLOAT_EQ(hnn_tune_get_dissipation_gamma_max(), 0.3f);

    /* Clamp low */
    hnn_tune_set_dissipation_gamma_max(-1.0f);
    EXPECT_FLOAT_EQ(hnn_tune_get_dissipation_gamma_max(), 0.0f);

    /* Clamp high */
    hnn_tune_set_dissipation_gamma_max(1000.0f);
    EXPECT_FLOAT_EQ(hnn_tune_get_dissipation_gamma_max(), 10.0f);

    /* Mid-range */
    hnn_tune_set_dissipation_gamma_max(2.5f);
    EXPECT_FLOAT_EQ(hnn_tune_get_dissipation_gamma_max(), 2.5f);
}
