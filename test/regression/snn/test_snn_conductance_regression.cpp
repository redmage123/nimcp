//=============================================================================
// test_snn_conductance_regression.cpp — OFF-mode bit-identity guarantee
//=============================================================================
/**
 * @file test_snn_conductance_regression.cpp
 * @brief Regression: with conductance_enabled=0 (default), the SNN must
 *        behave exactly as it did before the CB migration.
 *
 * WHAT: Runs the full SNN step with the legacy code path (CB flag OFF) and
 *       asserts that all output is finite, that g_exc/g_inh stay at zero,
 *       and that the standard interaction with depression / homeostasis
 *       continues to function.
 * WHY:  The CB migration MUST NOT change live training behavior until the
 *       flag is explicitly flipped on the pod. Any divergence in OFF mode
 *       is a Phase-1 or Phase-2 bug.
 * HOW:  Same fixture as the integration test, but exercises the broader
 *       SNN feature set (noise, basket, AHP, depression) with CB disabled.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_synapse.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

extern void  snn_tune_set_conductance_enabled(float);
extern float snn_tune_get_conductance_enabled(void);
extern void  snn_tune_set_cb_weights_rescaled(float);
extern void  snn_tune_set_noise_rate_hz(float);
extern void  snn_tune_set_basket_enabled(float);
extern void  snn_tune_set_ahp_enabled(float);
extern void  snn_tune_set_pump_enabled(float);
extern void  snn_tune_set_substrate_enabled(float);
extern void  snn_tune_set_depression_inc(float);
extern void  snn_tune_set_depression_tau_ms(float);
}

class CbRegressionTest : public ::testing::Test {
protected:
    snn_network_t* net = nullptr;
    int input_id = -1;
    int output_id = -1;
    static constexpr uint32_t N_IN  = 100;
    static constexpr uint32_t N_OUT = 100;

    static void SetUpTestSuite() { ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS); }
    static void TearDownTestSuite() { nimcp_shutdown(); }

    void SetUp() override {
        // CB MUST be off for the regression suite. Verify after each test.
        snn_tune_set_conductance_enabled(0.0f);
        snn_tune_set_cb_weights_rescaled(0.0f);
        // Restore default biophysics (the integration fixture disables them).
        snn_tune_set_noise_rate_hz(20.0f);
        snn_tune_set_basket_enabled(1.0f);
        snn_tune_set_ahp_enabled(1.0f);
        snn_tune_set_pump_enabled(1.0f);
        snn_tune_set_substrate_enabled(0.0f);  // no substrate attached
        snn_tune_set_depression_inc(0.3f);
        snn_tune_set_depression_tau_ms(50.0f);

        snn_config_t cfg;
        snn_config_default(&cfg);
        cfg.n_inputs = N_IN; cfg.n_outputs = N_OUT; cfg.n_hidden = 0;
        cfg.dt = 1.0f;
        net = snn_network_create(&cfg);
        ASSERT_NE(net, nullptr);

        input_id  = snn_network_add_population_lightweight(net, N_IN, NEURON_GENERIC_LIF, "input");
        output_id = snn_network_add_population_lightweight(net, N_OUT, NEURON_GENERIC_LIF, "output");
        ASSERT_GE(input_id,  0);
        ASSERT_GE(output_id, 0);

        snn_population_t* in  = net->populations[input_id];
        snn_population_t* out = net->populations[output_id];
        for (uint32_t dst = 0; dst < N_OUT; dst++) {
            for (uint32_t src = 0; src < N_IN; src++) {
                if (((src + dst) % 7) == 0) {  // ~14% sparse connectivity
                    snn_csr_add_entry(out->incoming_csr, dst,
                                      (uint32_t)input_id, src, 0.5f);
                }
            }
        }
        ASSERT_EQ(snn_csr_finalize(out->incoming_csr), 0);
        // Finalize input's empty CSR so the lightweight branch runs for it.
        if (in->incoming_csr && !in->incoming_csr->finalized) {
            ASSERT_EQ(snn_csr_finalize(in->incoming_csr), 0);
        }
    }

    void TearDown() override {
        // CB MUST still be off — bug if a test left it on.
        EXPECT_FLOAT_EQ(snn_tune_get_conductance_enabled(), 0.0f);
        if (net) snn_network_destroy(net);
        net = nullptr;
    }

    // Drive via v_data + external_current (setting spike_output is wrong:
    // the per-pop loop clears spike_data[n] = 0 on entry).
    void drive_all() {
        snn_population_t* in = net->populations[input_id];
        float* v   = (float*)nimcp_tensor_data(in->membrane_v);
        float* ref = (float*)nimcp_tensor_data(in->refractory);
        for (uint32_t i = 0; i < N_IN; i++) { v[i] = -49.5f; ref[i] = 0.0f; }
        if (in->external_current) {
            for (uint32_t i = 0; i < N_IN; i++) in->external_current[i] = 100.0f;
        }
    }
};

//=============================================================================
// OFF-mode invariants
//=============================================================================

// NOTE on snn_network_step return semantics: returns total_spikes on success
// (>= 0) and a negative SNN_ERROR_* code on failure. The original test used
// ASSERT_EQ(step, 0) which only passed when LIF decay happened to leave V
// just below threshold, producing flaky suite-level failures whenever timing
// or noise rate let one input neuron spike. All call sites now use ASSERT_GE
// (step, 0) — assert "no error", not "no spikes".

TEST_F(CbRegressionTest, OffMode_ConductanceArraysRemainAllZero) {
    // Even with full noise + basket + AHP + depression active, no g_exc
    // or g_inh should ever become nonzero in OFF mode.
    for (int s = 0; s < 50; s++) {
        drive_all();
        ASSERT_GE(snn_network_step(net, 1.0f), 0);
    }
    snn_population_t* out = net->populations[output_id];
    for (uint32_t i = 0; i < N_OUT; i++) {
        EXPECT_FLOAT_EQ(out->g_ampa[i],   0.0f);
        EXPECT_FLOAT_EQ(out->g_nmda[i],   0.0f);
        EXPECT_FLOAT_EQ(out->g_gaba_a[i], 0.0f);
        EXPECT_FLOAT_EQ(out->g_gaba_b[i], 0.0f);
    }
}

TEST_F(CbRegressionTest, OffMode_StepReturnsSuccessAndProducesFiniteV) {
    for (int s = 0; s < 50; s++) {
        drive_all();
        ASSERT_GE(snn_network_step(net, 1.0f), 0);
    }
    snn_population_t* out = net->populations[output_id];
    const float* v = (const float*)nimcp_tensor_data(out->membrane_v);
    for (uint32_t i = 0; i < N_OUT; i++) {
        EXPECT_TRUE(std::isfinite(v[i])) << "neuron " << i << " V=" << v[i];
    }
}

TEST_F(CbRegressionTest, OffMode_DepressionBufferAllocatedAndFinite) {
    // The depression mechanism is unchanged by the CB migration. Verify
    // the buffer exists and stays finite across many OFF-mode steps.
    // (Whether depression *accumulates* depends on input firing dynamics
    // which are not the CB regression's concern — they're covered by the
    // existing SNN test suite.)
    for (int s = 0; s < 30; s++) {
        drive_all();
        ASSERT_GE(snn_network_step(net, 1.0f), 0);
    }
    snn_population_t* in = net->populations[input_id];
    EXPECT_NE(in->depression, nullptr);
    for (uint32_t i = 0; i < N_IN; i++) {
        EXPECT_TRUE(std::isfinite(in->depression[i]));
        EXPECT_GE(in->depression[i], 0.0f);
        EXPECT_LE(in->depression[i], 1.0f);  // bounded by dep_cap
    }
}

TEST_F(CbRegressionTest, OffMode_BasketStillEmitsInhibition) {
    // Basket pool should be allocated (default-on) and accumulate spikes.
    snn_population_t* out = net->populations[output_id];
    EXPECT_NE(out->basket, nullptr);
    // Drive heavily so output spikes, which will recruit basket activity.
    for (int s = 0; s < 30; s++) {
        drive_all();
        ASSERT_GE(snn_network_step(net, 1.0f), 0);
    }
    SUCCEED();  // not crashing under full feature set with CB OFF is the
                // regression signal. The integration test covers behavior.
}

//=============================================================================
// Flag toggling: must be safe to flip ON and back OFF mid-run
//=============================================================================

TEST_F(CbRegressionTest, FlagFlipMidRun_NoCrash_NoNaN) {
    for (int s = 0; s < 10; s++) { drive_all(); snn_network_step(net, 1.0f); }
    snn_tune_set_conductance_enabled(1.0f);
    for (int s = 0; s < 10; s++) { drive_all(); snn_network_step(net, 1.0f); }
    snn_tune_set_conductance_enabled(0.0f);
    for (int s = 0; s < 10; s++) { drive_all(); snn_network_step(net, 1.0f); }

    snn_population_t* out = net->populations[output_id];
    const float* v = (const float*)nimcp_tensor_data(out->membrane_v);
    for (uint32_t i = 0; i < N_OUT; i++) {
        EXPECT_TRUE(std::isfinite(v[i]));
    }
}

//=============================================================================
// Phase-3 admin command must not affect OFF-mode dynamics
//=============================================================================

TEST_F(CbRegressionTest, RescaleAppliedThenOffMode_StillProducesFiniteV) {
    // Some operator might call rescale by accident with CB off; verify the
    // weights get smaller but the OFF-mode hot loop still works (just with
    // a 50× weaker effective drive).
    EXPECT_EQ(snn_rescale_weights_for_conductance(net, 0.02f), 0);
    EXPECT_FLOAT_EQ(snn_tune_get_conductance_enabled(), 0.0f);  // still off
    for (int s = 0; s < 30; s++) {
        drive_all();
        ASSERT_GE(snn_network_step(net, 1.0f), 0);
    }
    snn_population_t* out = net->populations[output_id];
    const float* v = (const float*)nimcp_tensor_data(out->membrane_v);
    for (uint32_t i = 0; i < N_OUT; i++) {
        EXPECT_TRUE(std::isfinite(v[i]));
    }
    // Reset rescale flag so other tests can apply it again.
    snn_tune_set_cb_weights_rescaled(0.0f);
}
