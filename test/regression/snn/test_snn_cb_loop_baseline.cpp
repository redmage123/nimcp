//=============================================================================
// test_snn_cb_loop_baseline.cpp — Regression fixture locking the exact
// behavior of the inline CPU CB hot loop. Foundation for CB-GPU-5 Phase A
// (the helper extraction must reproduce these checksums identically).
//=============================================================================
/**
 * @file test_snn_cb_loop_baseline.cpp
 * @brief Build a deterministic 2-population CB network, run a fixed
 *        number of steps, and assert hashes of (membrane V, conductances
 *        g_ampa/g_nmda/g_gaba_a/g_gaba_b, spike output, total_spikes)
 *        match locked golden values.
 *
 * WHAT: Pin the inline CPU CB hot loop's per-step output. The CB-GPU-5
 *       Phase A refactor extracts the deposit / integration / post-spike
 *       blocks into snn_cb_deposit_pop / snn_cb_integration_pop /
 *       snn_cb_post_spike_pop helpers. Each helper extraction must
 *       reproduce the SAME bytes this fixture asserts, otherwise it
 *       changed behavior — fail loud, fast.
 * WHY:  The CPU CB hot loop is ~600 lines fusing decay + gap-junction +
 *       refractory + deposit + integrate + spike + adaptation. A
 *       refactor that drops a load-bearing line is easy to commit and
 *       impossible to spot in code review. This fixture is the
 *       contract.
 * HOW:  Disable all stochastic features (noise, substrate dropout,
 *       basket, AHP/pump) so the loop is deterministic. Use Poisson
 *       seed = 0 anyway as a belt-and-braces. Build a tiny network
 *       with manually-set weights. Run 30 steps. Hash the final state
 *       via a simple rolling FNV-1a over float bytes.
 *
 *       Golden values are populated empirically — first run with
 *       checksum-print enabled, lock the printed values here.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_synapse.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "utils/tensor/nimcp_tensor.h"
extern void  snn_tune_set_noise_rate_hz(float);
extern void  snn_tune_set_basket_enabled(float);
extern void  snn_tune_set_ahp_enabled(float);
extern void  snn_tune_set_pump_enabled(float);
extern void  snn_tune_set_substrate_enabled(float);
extern void  snn_tune_set_conductance_enabled(float);
extern void  snn_tune_set_cb_weights_rescaled(float);
extern void  snn_tune_set_max_scale_dead(float);
extern void  snn_tune_set_metabolic_cap(float);
extern void  snn_tune_set_homeo_bounds(float, float);
extern void  snn_tune_set_depression_inc(float);
extern void  snn_tune_set_depression_tau_ms(float);
extern void  snn_tune_set_depression_cap(float);
extern void  snn_tune_set_e_ampa_mv(float);
extern void  snn_tune_set_e_gaba_a_mv(float);
extern void  snn_tune_set_tau_ampa_ms(float);
extern void  snn_tune_set_tau_gaba_a_ms(float);
}

namespace {

// Simple FNV-1a 64-bit over arbitrary bytes — deterministic across hosts
// (no dependence on libc hashing or order-of-allocation pointers).
static uint64_t fnv1a_64(const void* data, size_t bytes, uint64_t seed = 0xcbf29ce484222325ULL) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t h = seed;
    for (size_t i = 0; i < bytes; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

class SnnCbLoopBaseline : public ::testing::Test {
protected:
    static constexpr uint32_t N_IN  = 16;
    static constexpr uint32_t N_OUT = 32;
    static constexpr int      STEPS = 30;
    static constexpr float    DT_MS = 1.0f;
    static constexpr float    WEIGHT = 0.5f;  // mild excitatory drive

    static void SetUpTestSuite()    { ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS); }
    static void TearDownTestSuite() { nimcp_shutdown(); }

    void SetUp() override {
        // Disable everything stochastic to make the run deterministic.
        snn_tune_set_noise_rate_hz(0.0f);
        snn_tune_set_basket_enabled(0.0f);
        snn_tune_set_ahp_enabled(0.0f);
        snn_tune_set_pump_enabled(0.0f);
        snn_tune_set_substrate_enabled(0.0f);
        snn_tune_set_max_scale_dead(1.05f);
        snn_tune_set_metabolic_cap(1.0f);
        snn_tune_set_homeo_bounds(0.99f, 1.02f);
        snn_tune_set_depression_inc(0.3f);
        snn_tune_set_depression_tau_ms(50.0f);
        snn_tune_set_depression_cap(0.5f);
        // CB-on baseline.
        snn_tune_set_conductance_enabled(1.0f);
        snn_tune_set_cb_weights_rescaled(0.0f);
        snn_tune_set_e_ampa_mv(0.0f);
        snn_tune_set_e_gaba_a_mv(-75.0f);
        snn_tune_set_tau_ampa_ms(2.0f);
        snn_tune_set_tau_gaba_a_ms(10.0f);

        snn_config_t cfg;
        snn_config_default(&cfg);
        cfg.n_inputs  = N_IN;
        cfg.n_outputs = N_OUT;
        cfg.n_hidden  = 0;
        cfg.dt        = DT_MS;
        net_ = snn_network_create(&cfg);
        ASSERT_NE(net_, nullptr);

        input_id_  = snn_network_add_population_lightweight(
            net_, N_IN, NEURON_GENERIC_LIF, "input");
        output_id_ = snn_network_add_population_lightweight(
            net_, N_OUT, NEURON_GENERIC_LIF, "output");
        ASSERT_GE(input_id_,  0);
        ASSERT_GE(output_id_, 0);

        // Wire dense input → output with deterministic weights.
        snn_population_t* out = net_->populations[output_id_];
        ASSERT_NE(out->incoming_csr, nullptr);
        for (uint32_t dst = 0; dst < N_OUT; dst++) {
            for (uint32_t src = 0; src < N_IN; src++) {
                ASSERT_EQ(snn_csr_add_entry(out->incoming_csr, dst,
                                            (uint32_t)input_id_, src, WEIGHT), 0);
            }
        }
        ASSERT_EQ(snn_csr_finalize(out->incoming_csr), 0);
        snn_population_t* in = net_->populations[input_id_];
        if (in->incoming_csr && !in->incoming_csr->finalized) {
            ASSERT_EQ(snn_csr_finalize(in->incoming_csr), 0);
        }

        // Drive input every step from a known initial state.
        float* v = (float*)nimcp_tensor_data(in->membrane_v);
        for (uint32_t i = 0; i < N_IN; i++) v[i] = -49.5f;
        if (in->external_current) {
            for (uint32_t i = 0; i < N_IN; i++) in->external_current[i] = 5.0f;
        }
    }

    void TearDown() override {
        snn_tune_set_conductance_enabled(0.0f);
        if (net_) snn_network_destroy(net_);
        net_ = nullptr;
    }

    snn_network_t* net_      = nullptr;
    int            input_id_  = -1;
    int            output_id_ = -1;
};

// Walk both populations, hash V + g_ampa + g_nmda + g_gaba_a + g_gaba_b
// + spike_output into a single FNV-1a checksum. Total spike count is
// captured separately via network counters.
static uint64_t hash_pop_state(const snn_population_t* pop, uint64_t seed) {
    uint64_t h = seed;
    if (!pop) return h;
    if (pop->membrane_v) {
        const float* v = (const float*)nimcp_tensor_data(pop->membrane_v);
        h = fnv1a_64(v, pop->n_neurons * sizeof(float), h);
    }
    if (pop->g_ampa)   h = fnv1a_64(pop->g_ampa,   pop->n_neurons * sizeof(float), h);
    if (pop->g_nmda)   h = fnv1a_64(pop->g_nmda,   pop->n_neurons * sizeof(float), h);
    if (pop->g_gaba_a) h = fnv1a_64(pop->g_gaba_a, pop->n_neurons * sizeof(float), h);
    if (pop->g_gaba_b) h = fnv1a_64(pop->g_gaba_b, pop->n_neurons * sizeof(float), h);
    if (pop->spike_output) {
        const float* sp = (const float*)nimcp_tensor_data(pop->spike_output);
        h = fnv1a_64(sp, pop->n_neurons * sizeof(float), h);
    }
    return h;
}

TEST_F(SnnCbLoopBaseline, ThirtyStepCheckpointHash) {
    snn_population_t* in  = net_->populations[input_id_];
    snn_population_t* out = net_->populations[output_id_];
    ASSERT_NE(in,  nullptr);
    ASSERT_NE(out, nullptr);

    for (int s = 0; s < STEPS; s++) {
        // Re-pin input drive each step so the network has predictable
        // input rather than relying on random Poisson drive.
        float* v = (float*)nimcp_tensor_data(in->membrane_v);
        for (uint32_t i = 0; i < N_IN; i++) {
            if (v[i] < -65.0f) v[i] = -65.0f;
        }
        if (in->external_current) {
            for (uint32_t i = 0; i < N_IN; i++) in->external_current[i] = 5.0f;
        }
        // snn_network_step returns total spike count (>=0), not error code.
        ASSERT_GE(snn_network_step(net_, DT_MS), 0)
            << "step " << s << " failed";
    }

    uint64_t h = 0xcbf29ce484222325ULL;
    h = hash_pop_state(in,  h);
    h = hash_pop_state(out, h);

    // First-run discovery: print the value if no golden is locked yet.
    // To lock: replace the EXPECT_NE below with EXPECT_EQ + the printed hash.
    std::printf("[CB-baseline] 30-step state hash = 0x%016llx  in_total_spikes=%llu  out_total_spikes=%llu\n",
                static_cast<unsigned long long>(h),
                static_cast<unsigned long long>(in->total_spikes),
                static_cast<unsigned long long>(out->total_spikes));
    std::fflush(stdout);

    // Golden-lock: any refactor of the CPU CB hot loop must reproduce
    // EXACTLY these values. Phase A of CB-GPU-5 (helper extraction) is
    // a NO-OP refactor; this hash is the contract that proves it.
    // Phase B (GPU cutover) gets its own parallel CB+GPU integration
    // test — the CPU baseline must hold regardless.
    //
    // Captured 2026-04-28 from cc-activation @ HEAD pre-refactor.
    constexpr uint64_t GOLDEN_HASH       = 0xd3d9d3d3e5cd5d25ULL;
    constexpr uint64_t GOLDEN_IN_SPIKES  = 160ULL;
    constexpr uint64_t GOLDEN_OUT_SPIKES = 320ULL;
    EXPECT_EQ(h, GOLDEN_HASH)
        << "CB hot-loop output drifted — run printed above";
    EXPECT_EQ(static_cast<uint64_t>(in->total_spikes),  GOLDEN_IN_SPIKES);
    EXPECT_EQ(static_cast<uint64_t>(out->total_spikes), GOLDEN_OUT_SPIKES);
}

}  // namespace
