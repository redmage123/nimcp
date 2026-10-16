//=============================================================================
// test_snn_heterogeneity_unit.cpp — Unit tests for per-neuron LIF heterogeneity
//=============================================================================
/**
 * @file test_snn_heterogeneity_unit.cpp
 * @brief Wave G — Unit tests for the per-neuron τ_mem / v_thresh heterogeneity
 *        setter and the snn_pop_neuron_lif_params() resolution helper.
 *
 * WHAT: Verifies snn_network_set_pop_heterogeneity(), the per-neuron arrays
 *       allocation contract, the σ clamp, and the helper's NULL-fallback /
 *       per-neuron-override behaviour.
 * WHY:  Per-neuron heterogeneity is the structural fix for lock-step firing.
 *       Pure-helper tests pin contract before integration tests verify the
 *       behavioural improvement.
 * HOW:  Google Test. Builds tiny lightweight CSR pops and inspects the
 *       per-neuron arrays directly.
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

namespace {

class SnnHeterogeneityUnitTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }
    static void TearDownTestSuite() {
        nimcp_shutdown();
    }

    void SetUp() override {
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
    }

    int add_pop(uint32_t n) {
        int pop_id = snn_network_add_population_lightweight(
            net_, n, NEURON_GENERIC_LIF, "het_pop");
        if (pop_id < 0) return pop_id;
        snn_network_finalize_connections(net_);
        return pop_id;
    }

    snn_network_t* net_ = nullptr;
};

//-----------------------------------------------------------------------------
// 1. Default state on a fresh pop: sigma = 0, arrays NULL.
//-----------------------------------------------------------------------------
TEST_F(SnnHeterogeneityUnitTest, DefaultSigmaZeroAndArraysNull) {
    int pop_id = add_pop(64);
    ASSERT_GE(pop_id, 0);
    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    ASSERT_NE(pop, nullptr);
    EXPECT_FLOAT_EQ(pop->heterogeneity_sigma, 0.0f);
    EXPECT_EQ(pop->tau_mem_per_neuron,  nullptr);
    EXPECT_EQ(pop->v_thresh_per_neuron, nullptr);
}

//-----------------------------------------------------------------------------
// 2. Setter sigma=0.1 takes effect: σ stored, arrays alloc'd, each neuron's
//    tau_mem within ±3σ of base value (Gaussian draw).
//-----------------------------------------------------------------------------
TEST_F(SnnHeterogeneityUnitTest, SigmaSetAllocatesAndPopulatesArrays) {
    const uint32_t N = 64;
    int pop_id = add_pop(N);
    ASSERT_GE(pop_id, 0);

    int rc = snn_network_set_pop_heterogeneity(net_, (uint32_t)pop_id, 0.1f);
    EXPECT_EQ(rc, 0);

    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    ASSERT_NE(pop, nullptr);
    EXPECT_FLOAT_EQ(pop->heterogeneity_sigma, 0.1f);
    ASSERT_NE(pop->tau_mem_per_neuron,  nullptr);
    ASSERT_NE(pop->v_thresh_per_neuron, nullptr);

    /* Each neuron's τ should sit within ±5σ of the pop-wide base — generous
     * envelope to keep the test deterministic across PRNG seeds while still
     * catching pathological draws / rounding bugs. */
    snn_lif_params_t base = snn_pop_lif_params(pop, &net_->config);
    const float tau_lo = base.tau_mem * (1.0f - 5.0f * 0.1f);
    const float tau_hi = base.tau_mem * (1.0f + 5.0f * 0.1f);
    const float vthr_lo = base.v_thresh * (1.0f - 5.0f * 0.1f);
    const float vthr_hi = base.v_thresh * (1.0f + 5.0f * 0.1f);

    for (uint32_t n = 0; n < N; n++) {
        EXPECT_GT(pop->tau_mem_per_neuron[n], 0.0f)
            << "tau must remain positive (n=" << n << ")";
        EXPECT_GE(pop->tau_mem_per_neuron[n], tau_lo)
            << "n=" << n << " tau=" << pop->tau_mem_per_neuron[n];
        EXPECT_LE(pop->tau_mem_per_neuron[n], tau_hi)
            << "n=" << n << " tau=" << pop->tau_mem_per_neuron[n];
        /* v_thresh is negative (e.g. -50 mV); both bounds are negative, so
         * the test reads "magnitude must sit within ±5σ of |base|". Use
         * std::min/max to compare without flipping signs. */
        const float vlo = std::min(vthr_lo, vthr_hi);
        const float vhi = std::max(vthr_lo, vthr_hi);
        EXPECT_GE(pop->v_thresh_per_neuron[n], vlo);
        EXPECT_LE(pop->v_thresh_per_neuron[n], vhi);
    }

    /* Sanity: at N=64 with sigma=0.1, the realised standard deviation
     * should be in roughly [0.05, 0.15] of base — this is a probabilistic
     * test but with N=64 outliers are extremely rare. Fails the envelope
     * with prob < 1e-6 under a correct implementation. */
    float mean = 0.0f, sumsq = 0.0f;
    for (uint32_t n = 0; n < N; n++) mean += pop->tau_mem_per_neuron[n];
    mean /= N;
    for (uint32_t n = 0; n < N; n++) {
        float d = pop->tau_mem_per_neuron[n] - mean;
        sumsq += d * d;
    }
    float realised_sd = std::sqrt(sumsq / (N - 1));
    float realised_rel = realised_sd / base.tau_mem;
    EXPECT_GT(realised_rel, 0.04f)
        << "realized σ_rel=" << realised_rel
        << " too small — heterogeneity may be silently disabled";
    EXPECT_LT(realised_rel, 0.2f)
        << "realized σ_rel=" << realised_rel
        << " too large — Box-Muller may be amplifying";
}

//-----------------------------------------------------------------------------
// 3. Setter sigma=99 clamps to 0.5.
//-----------------------------------------------------------------------------
TEST_F(SnnHeterogeneityUnitTest, SigmaClampsAtHalf) {
    int pop_id = add_pop(16);
    ASSERT_GE(pop_id, 0);

    int rc = snn_network_set_pop_heterogeneity(net_, (uint32_t)pop_id, 99.0f);
    EXPECT_EQ(rc, 0);

    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    ASSERT_NE(pop, nullptr);
    EXPECT_FLOAT_EQ(pop->heterogeneity_sigma, 0.5f);
}

//-----------------------------------------------------------------------------
// 4. Setter rejects bad inputs (NULL net, OOB pop_id, NaN/Inf σ).
//-----------------------------------------------------------------------------
TEST_F(SnnHeterogeneityUnitTest, SetterRejectsBadInputs) {
    int pop_id = add_pop(4);
    ASSERT_GE(pop_id, 0);

    EXPECT_LT(snn_network_set_pop_heterogeneity(nullptr, 0, 0.1f), 0);
    EXPECT_LT(snn_network_set_pop_heterogeneity(net_, 9999, 0.1f), 0);
    EXPECT_LT(snn_network_set_pop_heterogeneity(net_, (uint32_t)pop_id,
                                                std::nanf("")), 0);
    EXPECT_LT(snn_network_set_pop_heterogeneity(net_, (uint32_t)pop_id,
                                                INFINITY), 0);

    /* Negative σ clamps to 0 (symmetric with gap-coupling setter). */
    EXPECT_EQ(snn_network_set_pop_heterogeneity(net_, (uint32_t)pop_id, -1.0f), 0);
    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    EXPECT_FLOAT_EQ(pop->heterogeneity_sigma, 0.0f);
}

//-----------------------------------------------------------------------------
// 5. Setter sigma=0 after sigma>0 frees the per-neuron arrays (symmetric
//    off-switch). Calls the setter twice to exercise the free path.
//-----------------------------------------------------------------------------
TEST_F(SnnHeterogeneityUnitTest, SigmaZeroFreesArrays) {
    int pop_id = add_pop(16);
    ASSERT_GE(pop_id, 0);

    ASSERT_EQ(snn_network_set_pop_heterogeneity(net_, (uint32_t)pop_id, 0.1f), 0);
    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    ASSERT_NE(pop->tau_mem_per_neuron,  nullptr);
    ASSERT_NE(pop->v_thresh_per_neuron, nullptr);

    ASSERT_EQ(snn_network_set_pop_heterogeneity(net_, (uint32_t)pop_id, 0.0f), 0);
    EXPECT_EQ(pop->tau_mem_per_neuron,  nullptr);
    EXPECT_EQ(pop->v_thresh_per_neuron, nullptr);
    EXPECT_FLOAT_EQ(pop->heterogeneity_sigma, 0.0f);
}

//-----------------------------------------------------------------------------
// 6. snn_pop_neuron_lif_params helper:
//    - When per-neuron arrays are NULL, returns pop-wide values.
//    - When arrays are present, returns per-neuron values.
//-----------------------------------------------------------------------------
TEST_F(SnnHeterogeneityUnitTest, NeuronLifParamsHelperFallback) {
    int pop_id = add_pop(8);
    ASSERT_GE(pop_id, 0);
    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    ASSERT_NE(pop, nullptr);

    /* Arrays NULL: per-neuron lookup must equal pop-wide. */
    snn_lif_params_t pop_wide = snn_pop_lif_params(pop, &net_->config);
    for (uint32_t n = 0; n < pop->n_neurons; n++) {
        snn_lif_params_t per_n = snn_pop_neuron_lif_params(pop, n, &net_->config);
        EXPECT_FLOAT_EQ(per_n.tau_mem,  pop_wide.tau_mem)
            << "n=" << n << ": NULL τ array must fall back to pop-wide";
        EXPECT_FLOAT_EQ(per_n.v_thresh, pop_wide.v_thresh)
            << "n=" << n << ": NULL v_thresh array must fall back to pop-wide";
    }
}

TEST_F(SnnHeterogeneityUnitTest, NeuronLifParamsHelperOverride) {
    int pop_id = add_pop(8);
    ASSERT_GE(pop_id, 0);
    snn_population_t* pop = snn_network_get_population(net_, (uint32_t)pop_id);
    ASSERT_NE(pop, nullptr);

    /* Engage heterogeneity, then directly inspect the helper. */
    ASSERT_EQ(snn_network_set_pop_heterogeneity(net_, (uint32_t)pop_id, 0.1f), 0);
    ASSERT_NE(pop->tau_mem_per_neuron,  nullptr);
    ASSERT_NE(pop->v_thresh_per_neuron, nullptr);

    /* Manually override the per-neuron arrays with sentinels so the test
     * does not depend on the PRNG draw — pin the helper's read path. */
    pop->tau_mem_per_neuron[3]  = 42.0f;
    pop->v_thresh_per_neuron[3] = -99.0f;

    snn_lif_params_t per_n = snn_pop_neuron_lif_params(pop, 3, &net_->config);
    EXPECT_FLOAT_EQ(per_n.tau_mem,  42.0f);
    EXPECT_FLOAT_EQ(per_n.v_thresh, -99.0f);

    /* Other fields (v_reset, v_rest, t_ref) must remain pop-wide — only
     * τ_mem and v_thresh are heterogenized today. */
    snn_lif_params_t pop_wide = snn_pop_lif_params(pop, &net_->config);
    EXPECT_FLOAT_EQ(per_n.v_reset, pop_wide.v_reset);
    EXPECT_FLOAT_EQ(per_n.v_rest,  pop_wide.v_rest);
    EXPECT_FLOAT_EQ(per_n.t_ref,   pop_wide.t_ref);
}

}  // anonymous namespace
