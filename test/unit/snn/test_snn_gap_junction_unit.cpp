//=============================================================================
// test_snn_gap_junction_unit.cpp — Unit tests for PV gap-junction coupling
//=============================================================================
/**
 * @file test_snn_gap_junction_unit.cpp
 * @brief Wave D — Unit tests for the per-pop gap-junction coupling setter
 *        and the mean-pull-toward-mean math applied in the SNN hot loop.
 *
 * WHAT: Verifies snn_network_set_pop_gap_coupling() and the mathematical
 *       behavior of the gap-junction adjustment.
 * WHY:  Gap junctions among PV basket cells are the primary substrate for
 *       cortical gamma rhythm. The math must be correct in isolation
 *       before integration tests can verify electrical propagation.
 * HOW:  Google Test. Builds a tiny lightweight CSR pop, sets membrane V
 *       directly, drives a single network step with conductance_mode ON,
 *       and verifies post-step V matches the mean-pull-toward-mean formula.
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "utils/tensor/nimcp_tensor.h"
}

/* SNN tunables — extern declarations (no header). */
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

class SnnGapJunctionUnitTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }
    static void TearDownTestSuite() {
        nimcp_shutdown();
    }

    void SetUp() override {
        /* Quiet baseline: disable noise, AHP, pump, basket, substrate so the
         * only voltage change in the hot loop comes from the gap-junction
         * adjustment + pure-leak LIF integration. */
        snn_tune_set_noise_rate_hz(0.0f);
        snn_tune_set_basket_enabled(0.0f);
        snn_tune_set_ahp_enabled(0.0f);
        snn_tune_set_pump_enabled(0.0f);
        snn_tune_set_substrate_enabled(0.0f);
        snn_tune_set_conductance_enabled(0.0f);  /* tests opt in */
        snn_tune_set_cb_weights_rescaled(1.0f);  /* skip rescale guard */

        snn_config_t cfg;
        snn_config_default(&cfg);
        cfg.n_inputs  = 1;
        cfg.n_outputs = 1;
        cfg.n_hidden  = 0;
        cfg.dt        = 1.0f;
        net_ = snn_network_create(&cfg);
        ASSERT_NE(net_, nullptr);
    }

    void TearDown() override {
        if (net_) {
            snn_network_destroy(net_);
            net_ = nullptr;
        }
        snn_tune_set_conductance_enabled(0.0f);
    }

    /* Add a 4-neuron lightweight pop and finalize CSR. Returns pop_id. */
    int add_pop4() {
        int pop_id = snn_network_add_population_lightweight(
            net_, 4, NEURON_GENERIC_LIF, "gap_pop");
        if (pop_id < 0) return pop_id;
        snn_network_finalize_connections(net_);
        return pop_id;
    }

    snn_network_t* net_ = nullptr;
};

//-----------------------------------------------------------------------------
// 1. Default gap_coupling is 0 (legacy behavior preserved by default)
//-----------------------------------------------------------------------------
TEST_F(SnnGapJunctionUnitTest, DefaultGapCouplingIsZero) {
    int pop_id = add_pop4();
    ASSERT_GE(pop_id, 0);
    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    ASSERT_NE(pop, nullptr);
    EXPECT_FLOAT_EQ(pop->gap_coupling, 0.0f);
}

//-----------------------------------------------------------------------------
// 2. Setter takes effect
//-----------------------------------------------------------------------------
TEST_F(SnnGapJunctionUnitTest, SetterAssignsValue) {
    int pop_id = add_pop4();
    ASSERT_GE(pop_id, 0);
    int rc = snn_network_set_pop_gap_coupling(net_, (uint32_t)pop_id, 0.05f);
    EXPECT_EQ(rc, 0);
    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    ASSERT_NE(pop, nullptr);
    EXPECT_FLOAT_EQ(pop->gap_coupling, 0.05f);

    /* Setter is idempotent — overwrite. */
    rc = snn_network_set_pop_gap_coupling(net_, (uint32_t)pop_id, 0.10f);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(pop->gap_coupling, 0.10f);
}

//-----------------------------------------------------------------------------
// 3. Setter rejects bad inputs (NULL, OOB pop_id, NaN/Inf)
//-----------------------------------------------------------------------------
TEST_F(SnnGapJunctionUnitTest, SetterRejectsBadInputs) {
    int pop_id = add_pop4();
    ASSERT_GE(pop_id, 0);

    EXPECT_LT(snn_network_set_pop_gap_coupling(nullptr, 0, 0.05f), 0);
    EXPECT_LT(snn_network_set_pop_gap_coupling(net_, 9999, 0.05f), 0);
    EXPECT_LT(snn_network_set_pop_gap_coupling(net_, (uint32_t)pop_id,
                                               std::nanf("")), 0);
    EXPECT_LT(snn_network_set_pop_gap_coupling(net_, (uint32_t)pop_id,
                                               INFINITY), 0);

    /* Negative is clamped to 0, not an error. */
    EXPECT_EQ(snn_network_set_pop_gap_coupling(net_, (uint32_t)pop_id, -1.0f), 0);
    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    EXPECT_FLOAT_EQ(pop->gap_coupling, 0.0f);
}

//-----------------------------------------------------------------------------
// 4. Mean-pull-toward-mean math: coupling=1.0, V=[-50,-60,-60,-60]
//    → V_mean = -57.5
//    → n0 should be pulled to -57.5 (delta = +2.5)... wait
//
// Actually: with coupling=1.0 the *adjustment* is (V_mean - V_n) × 1.0,
// so post-adjust voltages are: V_n + 1.0 × (V_mean - V_n) = V_mean.
// All 4 neurons converge to V_mean in one step — easiest to verify.
//
// To verify the more nuanced "n0 pulls down by 7.5, others pull up by 2.5"
// behaviour described in the task brief, we use coupling = 1.0 but check
// that the DELTAs match (V_mean - V_n) for each neuron.
//-----------------------------------------------------------------------------
TEST_F(SnnGapJunctionUnitTest, MeanPullTowardMean_Coupling1_FullConverge) {
    /* CB ON so the gap-junction term is active. */
    snn_tune_set_conductance_enabled(1.0f);

    int pop_id = add_pop4();
    ASSERT_GE(pop_id, 0);
    ASSERT_EQ(snn_network_set_pop_gap_coupling(net_, (uint32_t)pop_id, 1.0f), 0);

    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    ASSERT_NE(pop, nullptr);
    ASSERT_EQ(pop->n_neurons, 4u);

    float* v = (float*)nimcp_tensor_data(pop->membrane_v);
    float* ref = (float*)nimcp_tensor_data(pop->refractory);
    ASSERT_NE(v, nullptr);
    ASSERT_NE(ref, nullptr);

    /* Set initial state: n0=-50, others=-60. V_mean = -57.5.
     * With coupling=1 the gap term sets V_n exactly to V_mean for every
     * neuron, but the LIF integration also runs in the same step. To
     * isolate the gap math, set V_rest = current V (so leak contributes
     * 0) and force refractory so the LIF integration / spike generation
     * is bypassed AFTER the gap-junction adjustment is applied (gap is
     * applied BEFORE the refractory continue). */
    v[0] = -50.0f;
    v[1] = -60.0f;
    v[2] = -60.0f;
    v[3] = -60.0f;
    /* Force refractory so the LIF integration runs but the gap adjust
     * still happens (it runs BEFORE the refractory check by design). */
    for (uint32_t i = 0; i < 4; i++) ref[i] = 100.0f;

    /* Step the network. This must invoke the lightweight CSR path
     * (since we used add_population_lightweight + finalize). */
    int rc = snn_network_step(net_, 1.0f);
    ASSERT_GE(rc, 0);

    /* Pre-step V_mean = (-50 + -60×3) / 4 = -57.5
     * With coupling=1.0: V_n_post = V_n + 1.0 × (V_mean - V_n) = V_mean
     * for all n. */
    const float V_mean_expected = -57.5f;
    EXPECT_NEAR(v[0], V_mean_expected, 1e-4f)
        << "n0 should be pulled UP from -50 to V_mean=-57.5 (delta +7.5 toward mean)";
    EXPECT_NEAR(v[1], V_mean_expected, 1e-4f)
        << "n1 should be pulled DOWN from -60 to V_mean=-57.5 (delta -2.5 toward mean wait, +2.5)";
    EXPECT_NEAR(v[2], V_mean_expected, 1e-4f);
    EXPECT_NEAR(v[3], V_mean_expected, 1e-4f);
}

//-----------------------------------------------------------------------------
// 5. Mean-pull-toward-mean DELTAS: coupling=1.0, n0=-50, others=-60.
//    V_mean = -57.5. n0 delta should pull TOWARD -57.5 by 7.5 (wait: from -50
//    toward -57.5 is a DOWNWARD pull, delta = -7.5). Others toward -57.5 is
//    UPWARD pull, delta = +2.5 each. The TASK BRIEF says:
//      "n0's dv should pull toward -57.5 by 7.5 mV; neurons 1-3's dv should
//       push UP to -57.5 by +2.5 mV."
//    Delta direction matches: n0 pulled by -7.5, others +2.5. (Magnitude 7.5
//    for n0; the brief says "by 7.5 mV" without sign — both consistent.)
//-----------------------------------------------------------------------------
TEST_F(SnnGapJunctionUnitTest, DeltaMagnitudesMatchBrief) {
    snn_tune_set_conductance_enabled(1.0f);

    int pop_id = add_pop4();
    ASSERT_GE(pop_id, 0);
    ASSERT_EQ(snn_network_set_pop_gap_coupling(net_, (uint32_t)pop_id, 1.0f), 0);

    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    float* v = (float*)nimcp_tensor_data(pop->membrane_v);
    float* ref = (float*)nimcp_tensor_data(pop->refractory);

    const float v_initial[4] = { -50.0f, -60.0f, -60.0f, -60.0f };
    for (uint32_t i = 0; i < 4; i++) {
        v[i] = v_initial[i];
        ref[i] = 100.0f;  /* in refractory, so LIF integration won't run */
    }

    int rc = snn_network_step(net_, 1.0f);
    ASSERT_GE(rc, 0);

    /* Compute deltas — gap-junction adjustment is the only voltage change
     * because refractory blocks the LIF path. */
    const float V_mean = -57.5f;
    EXPECT_NEAR(v[0] - v_initial[0], V_mean - v_initial[0], 1e-4f)
        << "n0 delta should equal V_mean - V_n = -57.5 - (-50) = -7.5";
    for (uint32_t i = 1; i < 4; i++) {
        EXPECT_NEAR(v[i] - v_initial[i], V_mean - v_initial[i], 1e-4f)
            << "n" << i << " delta should equal V_mean - V_n = -57.5 - (-60) = +2.5";
    }
}

//-----------------------------------------------------------------------------
// 6. Coupling = 0 → no effect (legacy behaviour preserved).
//    Verifies that with gap_coupling=0 the per-neuron V is unchanged by
//    the gap path. Refractory blocks LIF integration so V should be EXACT.
//-----------------------------------------------------------------------------
TEST_F(SnnGapJunctionUnitTest, ZeroCouplingPreservesLegacyBehavior) {
    snn_tune_set_conductance_enabled(1.0f);  /* CB on */

    int pop_id = add_pop4();
    ASSERT_GE(pop_id, 0);
    /* Default 0 — don't call setter. */
    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    ASSERT_FLOAT_EQ(pop->gap_coupling, 0.0f);

    float* v = (float*)nimcp_tensor_data(pop->membrane_v);
    float* ref = (float*)nimcp_tensor_data(pop->refractory);

    const float v_initial[4] = { -50.0f, -60.0f, -60.0f, -60.0f };
    for (uint32_t i = 0; i < 4; i++) {
        v[i] = v_initial[i];
        ref[i] = 100.0f;
    }

    int rc = snn_network_step(net_, 1.0f);
    ASSERT_GE(rc, 0);

    /* gap_coupling=0 ⇒ no membrane voltage change from gap path.
     * Refractory blocks LIF integration. */
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(v[i], v_initial[i])
            << "n" << i << ": gap_coupling=0 must leave V untouched";
    }
}

//-----------------------------------------------------------------------------
// 7. CB OFF gates the gap-junction path off — even with coupling > 0 the
//    hot-loop gap-junction adjustment must not run. Compare behaviorally:
//    with CB off, gap_coupling=1.0 must produce IDENTICAL V trajectory to
//    gap_coupling=0.0. This is more robust than checking absolute V values
//    because it does not depend on which code path (GPU vs CPU) handles
//    the LIF integration when CB is off.
//-----------------------------------------------------------------------------
TEST_F(SnnGapJunctionUnitTest, ConductanceOffSuppressesGapPath) {
    snn_tune_set_conductance_enabled(0.0f);  /* CB off */

    /* Two parallel networks. Both CB off; only gap_coupling differs. */
    snn_config_t cfg;
    snn_config_default(&cfg);
    cfg.n_inputs  = 1;
    cfg.n_outputs = 1;
    cfg.n_hidden  = 0;
    cfg.dt        = 1.0f;

    snn_network_t* net_zero    = snn_network_create(&cfg);
    snn_network_t* net_coupled = snn_network_create(&cfg);
    ASSERT_NE(net_zero, nullptr);
    ASSERT_NE(net_coupled, nullptr);

    int pop_zero    = snn_network_add_population_lightweight(
        net_zero,    4, NEURON_GENERIC_LIF, "p0");
    int pop_coupled = snn_network_add_population_lightweight(
        net_coupled, 4, NEURON_GENERIC_LIF, "p1");
    ASSERT_GE(pop_zero, 0);
    ASSERT_GE(pop_coupled, 0);
    snn_network_finalize_connections(net_zero);
    snn_network_finalize_connections(net_coupled);

    /* Only the second has coupling; both should behave identically with
     * CB off because the hot-loop gates on cb_mode. */
    ASSERT_EQ(snn_network_set_pop_gap_coupling(
        net_coupled, (uint32_t)pop_coupled, 1.0f), 0);

    snn_population_t* p0 = snn_network_get_population(net_zero,    (uint32_t)pop_zero);
    snn_population_t* p1 = snn_network_get_population(net_coupled, (uint32_t)pop_coupled);
    float* v0 = (float*)nimcp_tensor_data(p0->membrane_v);
    float* v1 = (float*)nimcp_tensor_data(p1->membrane_v);
    float* r0 = (float*)nimcp_tensor_data(p0->refractory);
    float* r1 = (float*)nimcp_tensor_data(p1->refractory);

    const float v_initial[4] = { -55.0f, -60.0f, -65.0f, -68.0f };
    for (uint32_t i = 0; i < 4; i++) {
        v0[i] = v1[i] = v_initial[i];
        r0[i] = r1[i] = 0.0f;  /* not refractory — let LIF run; the test is
                                  about behaviorally identical trajectories */
    }

    /* Run a few steps. */
    for (int s = 0; s < 3; s++) {
        ASSERT_GE(snn_network_step(net_zero,    1.0f), 0);
        ASSERT_GE(snn_network_step(net_coupled, 1.0f), 0);
    }

    /* If gating works, both V trajectories must be bit-identical. */
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(v0[i], v1[i])
            << "n" << i << ": CB-off must gate gap-junction path off "
                              "(zero=" << v0[i] << " coupled=" << v1[i] << ")";
    }

    snn_network_destroy(net_zero);
    snn_network_destroy(net_coupled);
}

}  // anonymous namespace
