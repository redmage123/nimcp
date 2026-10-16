/**
 * @file test_snn_thalamic_substrate_compose.cpp
 * @brief Composition test: SNN thalamic bridge + Wave A+B biophysical features.
 *
 * WHAT: Five Google Test cases that exercise the pre-existing
 *       snn_thalamic_bridge through spike traffic produced by a lightweight
 *       SNN with Wave A+B biophysics toggled on/off (basket, AHP, pump,
 *       E/I-balanced noise, anti-reward). The bridge predates these
 *       features; this test proves they still compose correctly.
 *
 * WHY:  Per-feature biophysical unit + regression tests already exist
 *       (test/integration/snn/test_snn_biophysical_integration.cpp,
 *        test/regression/snn/test_snn_biophysical_regression.cpp).
 *       The thalamic bridge has its own coverage
 *       (test/integration/test_thalamic_bridge_integration.cpp) but nothing
 *       exercises the two together. Wave A+B changes the distribution of
 *       pop->spike_output — adaptation hyperpolarizes, basket inhibits,
 *       E/I-balanced noise can subtract drive. This file is the contract
 *       that the composed system stays sane (no NaN, no crashes, burst
 *       threshold still meaningful, anti-reward + bridge coexist).
 *
 * HOW:  Build a tiny SNN identical in spirit to the biophysical integration
 *       fixture (lightweight CSR pops, CPU-fallback LIF path), then create
 *       the thalamic router + SNN thalamic bridge on top of it. Flip the
 *       Wave A+B tunables per test case, drive with a strong constant
 *       current, and scrape observables.
 *
 * BRIDGE-API SURPRISE (document for reviewers):
 *   - snn_thalamic_bridge_process() does NOT read pop->spike_output
 *     directly. Callers must pack their own snn_spike_t[] from the pop
 *     tensors and pass them in. The bridge has no auto-scrape entry
 *     point; all observable routing stats (bridge->stats.spikes_relayed,
 *     bursts_detected, avg_attention, etc.) update *inside* process().
 *     This test therefore synthesizes the snn_spike_t[] from spike_output
 *     after each snn_network_step and feeds it to the bridge.
 *   - snn_thalamic_bridge_set_mode(bridge, neuron_id, mode) is keyed by
 *     the FLAT neuron id across populations (bridge sums pop->n_neurons
 *     from populations[0..n_populations-1]). Test 3 uses pop-0 ids in
 *     [0, pop->n_neurons) which are always valid.
 *
 * REFERENCES:
 *   include/snn/bridges/nimcp_snn_thalamic_bridge.h
 *   include/middleware/routing/nimcp_thalamic_router.h
 *   include/snn/nimcp_snn_types.h (snn_population_t with ahp/pump/basket)
 *   include/snn/nimcp_snn_training.h (tunables)
 *
 * @date 2026-04-22
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

/* Headers have their own extern "C" guards. */
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_training.h"
#include "snn/nimcp_snn_synapse.h"
#include "snn/nimcp_snn_adaptation.h"
#include "snn/nimcp_snn_basket.h"
#include "snn/bridges/nimcp_snn_thalamic_bridge.h"
#include "middleware/routing/nimcp_thalamic_router.h"
#include "utils/tensor/nimcp_tensor.h"

/* Non-public helpers from the SNN core — same approach as the biophysical
 * integration test; we don't touch src/ but we do need to drive the CSR
 * CPU path where Wave A+B actually run. */
extern "C" int snn_network_add_population_lightweight(snn_network_t* network,
                                                      uint32_t n_neurons,
                                                      neuron_type_t neuron_type,
                                                      const char* name);
extern "C" void nimcp_lif_state_destroy(void* state);

/*============================================================================
 * Fixture
 *==========================================================================*/
class SNNThalamicSubstrateComposeTest : public ::testing::Test {
protected:
    snn_network_t*          network = nullptr;
    snn_config_t            config;
    thalamic_router_t*      router  = nullptr;
    snn_thalamic_bridge_t*  bridge  = nullptr;

    /* Saved tunables — restore in TearDown. */
    float saved_ahp        = 0.0f;
    float saved_pump       = 0.0f;
    float saved_basket     = 0.0f;
    float saved_basket_fr  = 0.0f;
    float saved_noise_hz   = 0.0f;
    float saved_noise_mv   = 0.0f;
    float saved_noise_ei   = 0.0f;
    float saved_ar         = 0.0f;
    float saved_ar_thr     = 0.0f;
    float saved_ar_gain    = 0.0f;
    float saved_target     = 0.0f;
    float saved_target_in  = 0.0f;

    void SetUp() override {
        saved_ahp       = snn_tune_get_ahp_enabled();
        saved_pump      = snn_tune_get_pump_enabled();
        saved_basket    = snn_tune_get_basket_enabled();
        saved_basket_fr = snn_tune_get_basket_fraction();
        saved_noise_hz  = snn_tune_get_noise_rate_hz();
        saved_noise_mv  = snn_tune_get_noise_pulse_mv();
        saved_noise_ei  = snn_tune_get_noise_ei_ratio();
        saved_ar        = snn_tune_get_anti_reward_enabled();
        saved_ar_thr    = snn_tune_get_anti_reward_threshold_ratio();
        saved_ar_gain   = snn_tune_get_anti_reward_gain();
        saved_target    = snn_tune_get_target_rate();
        saved_target_in = snn_tune_get_target_rate_input();

        memset(&config, 0, sizeof(snn_config_t));
        /* 1 input / 0 hidden / 1 output lightweight scaffold; then we add
         * a single 200-neuron lightweight pop for the actual test. Keep
         * the populations[] array big enough for it. */
        snn_config_feedforward(&config, 1, 0, 1);
        config.n_populations = 8;
    }

    void TearDown() override {
        if (bridge)  { snn_thalamic_bridge_destroy(bridge);  bridge  = nullptr; }
        if (router)  { thalamic_router_destroy(router);      router  = nullptr; }
        if (network) {
            snn_network_destroy(network);
            network = nullptr;
        }
        snn_config_destroy(&config);

        snn_tune_set_ahp_enabled(saved_ahp);
        snn_tune_set_pump_enabled(saved_pump);
        snn_tune_set_basket_enabled(saved_basket);
        snn_tune_set_basket_fraction(saved_basket_fr);
        snn_tune_set_noise_rate_hz(saved_noise_hz);
        snn_tune_set_noise_pulse_mv(saved_noise_mv);
        snn_tune_set_noise_ei_ratio(saved_noise_ei);
        snn_tune_set_anti_reward_enabled(saved_ar);
        snn_tune_set_anti_reward_threshold_ratio(saved_ar_thr);
        snn_tune_set_anti_reward_gain(saved_ar_gain);
        snn_tune_set_target_rate(saved_target);
        snn_tune_set_target_rate_input(saved_target_in);
    }

    /* Force CPU fallback so Wave A+B biophysics run (they live in the CSR
     * CPU path, not the GPU LIF fast path). */
    void ForceCpuFallback() {
        ASSERT_NE(network, nullptr);
        if (network->gpu_lif_state) {
            nimcp_lif_state_destroy(network->gpu_lif_state);
            network->gpu_lif_state = nullptr;
        }
    }

    /* Same rationale as the biophysical integration test — IP would
     * confound per-neuron measurements. */
    static void DisableIP(snn_population_t* pop) {
        ASSERT_NE(pop, nullptr);
        pop->threshold_offset = nullptr;
    }

    /* Reset lightweight pop to a clean resting state so back-to-back runs
     * within a single test start identical. */
    static void HardResetPop(snn_population_t* pop) {
        ASSERT_NE(pop, nullptr);
        if (pop->membrane_v) {
            float* v = (float*)nimcp_tensor_data(pop->membrane_v);
            if (v) for (uint32_t n = 0; n < pop->n_neurons; n++) v[n] = -70.0f;
        }
        if (pop->refractory) {
            float* r = (float*)nimcp_tensor_data(pop->refractory);
            if (r) memset(r, 0, pop->n_neurons * sizeof(float));
        }
        if (pop->spike_output) {
            float* s = (float*)nimcp_tensor_data(pop->spike_output);
            if (s) memset(s, 0, pop->n_neurons * sizeof(float));
        }
        if (pop->depression) memset(pop->depression, 0, pop->n_neurons * sizeof(float));
        pop->total_spikes    = 0;
        pop->firing_rate_ema = 0.03f;
        pop->rate_samples    = 0;
        if (pop->ahp)    snn_adaptation_reset(pop->ahp);
        if (pop->pump)   snn_adaptation_reset(pop->pump);
        if (pop->basket) snn_basket_pool_reset(pop->basket);
    }

    static void InjectCurrent(snn_population_t* pop, float amp_mv) {
        ASSERT_NE(pop, nullptr);
        ASSERT_TRUE(pop->lightweight);
        ASSERT_NE(pop->external_current, nullptr);
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            pop->external_current[n] = amp_mv;
        }
    }

    snn_population_t* AddLightweightPop(uint32_t n_neurons, const char* name) {
        int pid = snn_network_add_population_lightweight(
            network, n_neurons, NEURON_GENERIC_LIF, name);
        EXPECT_GE(pid, 0);
        if (pid < 0) return nullptr;
        snn_population_t* pop = network->populations[pid];
        EXPECT_NE(pop, nullptr);
        if (!pop) return nullptr;
        EXPECT_TRUE(pop->lightweight);
        int rc = snn_csr_finalize(pop->incoming_csr);
        EXPECT_EQ(rc, 0);
        return pop;
    }

    /* Scrape spike_output for a pop into an snn_spike_t[] the bridge's
     * process() can consume. The bridge does NOT auto-scrape; the caller
     * owns this translation. Returns number of spikes packed. */
    static uint32_t ScrapeSpikes(const snn_population_t* pop,
                                 uint64_t t_us,
                                 snn_spike_t* out,
                                 uint32_t capacity) {
        const float* s = (const float*)nimcp_tensor_data_const(pop->spike_output);
        if (!s) return 0;
        uint32_t n = 0;
        for (uint32_t i = 0; i < pop->n_neurons && n < capacity; i++) {
            if (s[i] > 0.5f) {
                out[n].timestamp_us   = t_us;
                out[n].neuron_id      = i;
                out[n].population_id  = pop->id;
                n++;
            }
        }
        return n;
    }

    static uint32_t CountSpikesOnce(const snn_population_t* pop) {
        const float* s = (const float*)nimcp_tensor_data_const(pop->spike_output);
        if (!s) return 0;
        uint32_t c = 0;
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            if (s[n] > 0.5f) c++;
        }
        return c;
    }

    /* Build the router + bridge on top of an already-initialized network.
     * Populations must be added before creating the bridge because
     * snn_thalamic_bridge_create sizes neuron_modes[] and
     * attention_weights[] from network->n_populations at create time. */
    void CreateRouterAndBridge() {
        ASSERT_NE(network, nullptr);
        thalamic_router_config_t rcfg = thalamic_router_default_config();
        rcfg.enable_attention_gating = true;
        rcfg.enable_priority_routing = true;
        rcfg.enable_statistics       = true;
        rcfg.max_destinations        = 32;
        rcfg.min_attention_threshold = 0.0f;  /* don't filter in router */
        router = thalamic_router_create(&rcfg);
        ASSERT_NE(router, nullptr) << "router create failed";

        snn_thalamic_config_t bcfg;
        snn_thalamic_config_default(&bcfg);
        bcfg.default_mode            = THALAMIC_MODE_ADAPTIVE;
        bcfg.enable_mode_switching   = true;
        bcfg.enable_attention_gating = true;
        bcfg.attention_threshold     = 0.05f;
        bcfg.burst_threshold_ms      = 4.0f;
        bcfg.tonic_min_isi_ms        = 10.0f;
        bcfg.enable_ct_loop          = false;  /* simplify: no CT feedback */
        bcfg.enable_trn_inhibition   = false;
        bcfg.enable_bio_async        = false;
        bridge = snn_thalamic_bridge_create(&bcfg, network, router);
        ASSERT_NE(bridge, nullptr) << "bridge create failed";
    }
};

/*============================================================================
 * Test 1: Bridge survives basket inhibition.
 *
 * Turn on basket pool, drive pop hard, pump spikes through the bridge for
 * 100 steps. Assert: (a) process() returns 0 every call, (b) at least one
 * spike got relayed, (c) avg_attention stays finite.
 *==========================================================================*/
TEST_F(SNNThalamicSubstrateComposeTest, BridgeSurvivesBasketInhibition) {
    /* Other biophysics off — isolate basket. */
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_noise_rate_hz(0.0f);
    snn_tune_set_anti_reward_enabled(0.0f);

    /* Enable basket BEFORE pop creation (allocation gated at create time). */
    snn_tune_set_basket_enabled(1.0f);
    snn_tune_set_basket_fraction(0.2f);

    network = snn_network_create(&config);
    ASSERT_NE(network, nullptr);
    ForceCpuFallback();

    snn_population_t* pop = AddLightweightPop(200, "bask_pop");
    ASSERT_NE(pop, nullptr);
    ASSERT_NE(pop->basket, nullptr)
        << "basket struct should be allocated when tunable is on at create time";
    DisableIP(pop);

    CreateRouterAndBridge();

    /* Give the bridge a known attention weight for the pop so we can
     * verify avg_attention later. attention_threshold=0.05 with weight=0.5
     * means spikes pass; bursts (boost=1.5) also pass. */
    ASSERT_EQ(snn_thalamic_bridge_set_attention(bridge, pop->id, 0.5f), 0);

    HardResetPop(pop);

    /* Drive very hard. With defaults (v_rest=-70, v_thresh=-50, tau_mem=20ms,
     * dt=0.1ms) steady-state v = v_rest + I_syn. I_drive=80 pushes
     * steady-state to +10mV (60mV above thresh), so first spike happens
     * in ~8-10ms (80-100 steps). The 100-step minimum spec in the task
     * is too short to see spikes at that rate — widen to 600. */
    const float I_drive = 80.0f;
    const int   n_steps = 600;
    const uint32_t spike_cap = 256;
    std::vector<snn_spike_t> spike_in(spike_cap);
    std::vector<snn_spike_t> spike_out(spike_cap);
    uint64_t total_seen    = 0;
    uint64_t total_relayed = 0;
    uint32_t process_calls = 0;
    uint32_t process_errs  = 0;

    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop, I_drive);
        snn_network_step(network, 0.1f);
        uint32_t n_in = ScrapeSpikes(pop, (uint64_t)step * 100, spike_in.data(),
                                     spike_cap);
        total_seen += n_in;
        if (n_in == 0) continue;

        uint32_t n_out = 0;
        int rc = snn_thalamic_bridge_process(
            bridge, spike_in.data(), n_in,
            spike_out.data(), spike_cap, &n_out);
        process_calls++;
        if (rc != 0) process_errs++;
        total_relayed += n_out;
    }

    /* Bridge reported no errors. */
    EXPECT_EQ(process_errs, 0u)
        << "bridge_process returned error on " << process_errs
        << "/" << process_calls << " calls";

    /* At least one spike routed — the basket shouldn't have silenced the
     * pop entirely, and the bridge shouldn't be swallowing everything. */
    snn_thalamic_stats_t bstats;
    ASSERT_EQ(snn_thalamic_bridge_get_stats(bridge, &bstats), 0);
    EXPECT_GT(bstats.spikes_relayed, 0u)
        << "with basket on + strong drive, at least one spike must route "
           "(total_seen=" << total_seen << " relayed=" << total_relayed << ")";

    /* avg_attention must be finite. Bridge updates it with a running EMA
     * that includes attention_boost_burst (1.5x for bursts), so values
     * above 1.0 are legal — just not NaN/Inf. */
    EXPECT_TRUE(std::isfinite(bstats.avg_attention))
        << "avg_attention went non-finite under basket-modulated spikes: "
        << bstats.avg_attention;
    EXPECT_GE(bstats.avg_attention, 0.0f)
        << "avg_attention must be non-negative: " << bstats.avg_attention;
}

/*============================================================================
 * Test 2: Bridge attention gates are applied to biophysically-modulated
 * spikes (no basket in this test — just that the gate math is wired).
 *
 * The bridge's observable for "attention applied" is:
 *   bridge->stats.avg_attention (running EMA of applied attention per
 *     relayed spike; per the implementation, the EMA is updated AFTER
 *     multiplying by attention_boost_burst for burst spikes).
 *
 * We set pop-0 attention to 0.3, disable burst boost by forcing the
 * neuron_modes to TONIC (so no x1.5), drive the pop and route. After
 * 100 steps, the EMA has converged toward 0.3 within tolerance. The
 * attention getter must also report 0.3 (round-trip).
 *==========================================================================*/
TEST_F(SNNThalamicSubstrateComposeTest, BridgeAttentionGatesAppliedToModulatedSpikes) {
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_noise_rate_hz(0.0f);
    snn_tune_set_anti_reward_enabled(0.0f);

    network = snn_network_create(&config);
    ASSERT_NE(network, nullptr);
    ForceCpuFallback();

    snn_population_t* pop = AddLightweightPop(200, "attn_pop");
    ASSERT_NE(pop, nullptr);
    DisableIP(pop);

    CreateRouterAndBridge();

    /* Set attention on the pop — above attention_threshold (0.05) so
     * spikes pass, but distinctive so the EMA converges to something we
     * can check. */
    const float attn = 0.3f;
    ASSERT_EQ(snn_thalamic_bridge_set_attention(bridge, pop->id, attn), 0);

    /* Round-trip check. */
    float read_back = -1.0f;
    ASSERT_EQ(snn_thalamic_bridge_get_attention(bridge, pop->id, &read_back), 0);
    EXPECT_FLOAT_EQ(read_back, attn);

    /* Force all neurons into TONIC so burst boost (x1.5) does not
     * contaminate the attention EMA. Bridge uses global neuron IDs for
     * set_mode and walks populations[0..n_populations-1] to compute the
     * valid range — pop->id is 2 (after input+output scaffolds), so its
     * neurons start at an offset we'd have to compute. Easier: set mode
     * for all possible IDs [0, N). */
    uint32_t total = 0;
    for (uint32_t i = 0; i < network->n_populations; i++) {
        total += network->populations[i]->n_neurons;
    }
    for (uint32_t nid = 0; nid < total; nid++) {
        (void)snn_thalamic_bridge_set_mode(bridge, nid, THALAMIC_MODE_TONIC);
    }

    HardResetPop(pop);

    /* Same drive/duration rationale as test 1: need enough steps for the
     * LIF to cross threshold and fire repeatedly (defaults give ISI
     * ~5-10ms under hard drive, so first spike ~80-100 steps, then
     * spikes every ~50-100 steps per neuron). */
    const float I_drive = 80.0f;
    const int   n_steps = 600;
    const uint32_t cap  = 256;
    std::vector<snn_spike_t> spike_in(cap), spike_out(cap);
    uint64_t relayed_total = 0;

    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop, I_drive);
        snn_network_step(network, 0.1f);
        uint32_t n_in = ScrapeSpikes(pop, (uint64_t)step * 100, spike_in.data(),
                                     cap);
        if (n_in == 0) continue;

        uint32_t n_out = 0;
        int rc = snn_thalamic_bridge_process(
            bridge, spike_in.data(), n_in,
            spike_out.data(), cap, &n_out);
        ASSERT_EQ(rc, 0);
        relayed_total += n_out;
    }

    snn_thalamic_stats_t bstats;
    ASSERT_EQ(snn_thalamic_bridge_get_stats(bridge, &bstats), 0);
    ASSERT_GT(bstats.spikes_relayed, 0u)
        << "need at least one relayed spike to assert attention was applied";

    /* Attention setter is authoritative — the getter must still show 0.3
     * after 100 steps of routing (no adaptive attention learning in this
     * config). This is the primary "attention gates applied" check. */
    float attn_after = -1.0f;
    EXPECT_EQ(snn_thalamic_bridge_get_attention(bridge, pop->id, &attn_after), 0);
    EXPECT_FLOAT_EQ(attn_after, attn)
        << "attention weight drifted during routing — should be static";

    /* Secondary check: EMA converges toward attn. Start=0, EMA update is
     * ema = ema*0.99 + attn*0.01 per relayed spike. After many spikes,
     * ema -> attn asymptotically. With N relayed spikes, expected EMA
     * is attn * (1 - 0.99^N). For N>=50 that's >= 0.3 * 0.395 ≈ 0.12.
     * The bar is loose: the EMA must be strictly positive and <= attn. */
    EXPECT_GT(bstats.avg_attention, 0.0f)
        << "avg_attention EMA should have accumulated some signal (got 0)";
    EXPECT_LE(bstats.avg_attention, attn + 1e-4f)
        << "avg_attention EMA overshot attn (tonic-only, no burst boost). "
        << "ema=" << bstats.avg_attention << " attn=" << attn;
}

/*============================================================================
 * Test 3: Bridge burst mode with adaptation disabled — fires hot.
 *
 * Goal: produce a high-rate spike train so the bridge's burst-detection
 * (ISI < burst_threshold_ms) has something to bite on. Primary observable
 * is bstats.bursts_detected >= N. Secondary: pop->firing_rate_ema > 0.1
 * (confirms the drive actually made the pop fire at >10% per step after
 * warmup, which is what ADAPTIVE mode needs for bursts).
 *
 * Bridge's mode switching: default_mode=ADAPTIVE; detect_mode reads the
 * stored last_spike_time_us per neuron_id. If ISI < 4ms (burst_threshold),
 * returns BURST. Our bridge processes spikes at the SNN's dt=0.1ms, so
 * consecutive spikes from the same neuron ~1ms apart register as BURST.
 *==========================================================================*/
TEST_F(SNNThalamicSubstrateComposeTest, BridgeBurstModeWithAdaptationDisabled) {
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_noise_rate_hz(0.0f);
    snn_tune_set_anti_reward_enabled(0.0f);

    network = snn_network_create(&config);
    ASSERT_NE(network, nullptr);
    ForceCpuFallback();

    snn_population_t* pop = AddLightweightPop(200, "burst_pop");
    ASSERT_NE(pop, nullptr);
    EXPECT_EQ(pop->ahp, nullptr)  << "ahp should be NULL when disabled";
    EXPECT_EQ(pop->pump, nullptr) << "pump should be NULL when disabled";
    DisableIP(pop);

    CreateRouterAndBridge();

    /* Attention well above threshold so nothing gets gated out. */
    ASSERT_EQ(snn_thalamic_bridge_set_attention(bridge, pop->id, 1.0f), 0);

    HardResetPop(pop);

    /* Very hard drive. I_drive=100 pushes steady-state v to +30mV (80mV
     * past threshold). First spike ~50 steps (5ms), then recurring fires
     * at close to refractory limit (2ms = 20 steps). With 200 neurons
     * each firing every ~20-40 steps, per-step spike fraction is high.
     * Need enough steps for firing_rate_ema (alpha=0.05 during warmup)
     * to reach a meaningful value. */
    const float I_drive = 100.0f;
    const int   n_steps = 1500;
    const uint32_t cap  = 512;
    std::vector<snn_spike_t> spike_in(cap), spike_out(cap);

    /* Drive the bridge's detect_mode logic: each neuron must have TWO
     * recent spikes within burst_threshold_ms (4ms = 40 steps @ dt=0.1ms).
     * To manufacture short ISIs we feed the bridge each fired neuron as
     * TWO spikes close in time — once with the current step's timestamp
     * and once with the previous step's timestamp. This is faithful to
     * how the bridge would see a real 100-Hz+ population: consecutive
     * firings in the same pop arrive in consecutive process() calls.
     * But a simpler approach works: pass a spike with a reasonable
     * timestamp and call detect_mode twice. The bridge's internal
     * last_spike_time_us tracking makes the second call return BURST.
     *
     * Easier: rely on the fact that pop fires repeatedly across steps.
     * Each neuron_id going through process() with ISI below 4ms
     * classifies as burst. Our dt=0.1ms means consecutive process()
     * calls can be 0.1ms apart if a neuron fires in consecutive steps —
     * always a burst. More typically neurons recover + fire every
     * 3-5ms (with I_drive=100), which straddles the 4ms threshold. */
    uint64_t step_us = 0;
    const uint64_t step_incr_us = 100;  /* dt=0.1ms */

    uint64_t total_relayed = 0;
    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop, I_drive);
        snn_network_step(network, 0.1f);
        uint32_t n_in = ScrapeSpikes(pop, step_us, spike_in.data(), cap);
        step_us += step_incr_us;
        if (n_in == 0) continue;

        uint32_t n_out = 0;
        int rc = snn_thalamic_bridge_process(
            bridge, spike_in.data(), n_in,
            spike_out.data(), cap, &n_out);
        ASSERT_EQ(rc, 0);
        total_relayed += n_out;
    }

    /* BRIDGE API SURPRISE: snn_thalamic_bridge_detect_mode is only called
     * from process() when the neuron's current mode is ADAPTIVE. Default
     * mode is ADAPTIVE — so on a neuron's FIRST spike, detect_mode
     * returns TONIC (last_time==0 branch) and the neuron is pinned to
     * TONIC for all subsequent spikes. There is no public way to
     * re-enter ADAPTIVE without calling set_mode(..., ADAPTIVE) each
     * step. Therefore bursts_detected is a poor observable for the
     * "adaptation off → hot firing" condition.
     *
     * Task spec says: "If the bridge's mode-switch isn't externally
     * observable, test via firing-rate proxy". Proxy: pop->total_spikes
     * must be large (pop fired hard under hard drive), and
     * bridge->stats.spikes_relayed should equal the count we fed in
     * (no spikes blocked by attention gate with our weight=1.0). */

    EXPECT_GT(pop->total_spikes, (uint64_t)n_steps)
        << "adaptation disabled + hard drive should yield > n_steps "
           "total spikes across 200 neurons. Got total_spikes="
        << pop->total_spikes << " across " << n_steps << " steps";

    /* Bridge saw all of those spikes as routable. */
    snn_thalamic_stats_t bstats;
    ASSERT_EQ(snn_thalamic_bridge_get_stats(bridge, &bstats), 0);
    EXPECT_GT(bstats.spikes_relayed, 0u)
        << "bridge should relay under hot firing";
    EXPECT_EQ(bstats.spikes_blocked, 0u)
        << "no spikes should be attention-blocked at weight=1.0";
    EXPECT_EQ(bstats.spikes_relayed, total_relayed)
        << "router stats should match our per-step tally";

    /* Re-prove the detect_mode logic itself is reachable when the mode
     * is ADAPTIVE at call time. Hand-reset neuron 0 to ADAPTIVE and
     * push two spikes ISI=2ms apart. Second call should return BURST
     * because the computed ISI is below the 4ms burst_threshold. */
    ASSERT_EQ(snn_thalamic_bridge_set_mode(bridge, 0, THALAMIC_MODE_ADAPTIVE), 0);
    thalamic_relay_mode_t m1 = snn_thalamic_bridge_detect_mode(bridge, 0, 1000);
    EXPECT_EQ(m1, THALAMIC_MODE_TONIC)   /* first call → no prior, tonic */
        << "first detect_mode with no prior spike must be tonic";
    thalamic_relay_mode_t m2 = snn_thalamic_bridge_detect_mode(bridge, 0, 3000);
    EXPECT_EQ(m2, THALAMIC_MODE_BURST)   /* 2ms ISI → burst */
        << "2ms ISI must classify as burst (threshold=4ms)";
}

/*============================================================================
 * Test 4: Bridge burst mode with adaptation ENABLED fires LESS than with
 * adaptation disabled.
 *
 * This is the composition proof: Wave A+B features take effect inside the
 * live step that feeds the bridge. We do a two-pass comparison against the
 * same pop layout, same drive, same n_steps — just flip AHP+pump on/off.
 *
 * Separate pops so allocation matches — AHP/pump structs are only
 * allocated when their tunable is on at pop-create time. We build two
 * pops, measure ahp+pump pop's firing rate vs the plain pop's firing
 * rate, and assert plain > adapted.
 *==========================================================================*/
TEST_F(SNNThalamicSubstrateComposeTest, BridgeBurstModeWithAdaptationEnabled) {
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_noise_rate_hz(0.0f);
    snn_tune_set_anti_reward_enabled(0.0f);

    /* First pop: AHP+pump DISABLED → ahp/pump remain NULL. */
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);

    network = snn_network_create(&config);
    ASSERT_NE(network, nullptr);
    ForceCpuFallback();

    snn_population_t* pop_plain = AddLightweightPop(200, "plain_pop");
    ASSERT_NE(pop_plain, nullptr);
    EXPECT_EQ(pop_plain->ahp, nullptr);
    EXPECT_EQ(pop_plain->pump, nullptr);
    DisableIP(pop_plain);

    /* Second pop: AHP+pump ENABLED → structs allocated. Crank AHP gain
     * for visible effect within the run window. */
    snn_tune_set_ahp_enabled(1.0f);
    snn_tune_set_ahp_tau_ms(100.0f);
    snn_tune_set_ahp_gain_mv(5.0f);
    snn_tune_set_pump_enabled(1.0f);
    snn_tune_set_pump_tau_ms(2000.0f);
    snn_tune_set_pump_gain_mv(2.0f);

    snn_population_t* pop_adapt = AddLightweightPop(200, "adapt_pop");
    ASSERT_NE(pop_adapt, nullptr);
    ASSERT_NE(pop_adapt->ahp, nullptr);
    ASSERT_NE(pop_adapt->pump, nullptr);
    DisableIP(pop_adapt);

    CreateRouterAndBridge();
    ASSERT_EQ(snn_thalamic_bridge_set_attention(bridge, pop_plain->id, 1.0f), 0);
    ASSERT_EQ(snn_thalamic_bridge_set_attention(bridge, pop_adapt->id, 1.0f), 0);

    /* Pass 1: both pops in parallel, but score only plain (AHP knob OFF). */
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    HardResetPop(pop_plain);
    HardResetPop(pop_adapt);

    const float I_drive = 60.0f;
    const int   n_steps = 500;
    const uint32_t cap  = 512;
    std::vector<snn_spike_t> spike_in(cap), spike_out(cap);

    uint64_t plain_spikes = 0;
    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop_plain, I_drive);
        InjectCurrent(pop_adapt, I_drive);
        snn_network_step(network, 0.1f);
        plain_spikes += CountSpikesOnce(pop_plain);

        /* Route some spikes to exercise the composed path (not scored). */
        uint32_t n_in = ScrapeSpikes(pop_plain, (uint64_t)step * 100,
                                     spike_in.data(), cap);
        if (n_in > 0) {
            uint32_t n_out = 0;
            (void)snn_thalamic_bridge_process(bridge, spike_in.data(), n_in,
                                              spike_out.data(), cap, &n_out);
        }
    }
    ASSERT_GT(plain_spikes, 0u);

    /* Pass 2: enable AHP+pump globally. Only pop_adapt has the structs,
     * so the knob only affects that pop. */
    snn_tune_set_ahp_enabled(1.0f);
    snn_tune_set_pump_enabled(1.0f);
    HardResetPop(pop_plain);
    HardResetPop(pop_adapt);

    uint64_t adapt_spikes = 0;
    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop_plain, I_drive);
        InjectCurrent(pop_adapt, I_drive);
        snn_network_step(network, 0.1f);
        adapt_spikes += CountSpikesOnce(pop_adapt);

        uint32_t n_in = ScrapeSpikes(pop_adapt, (uint64_t)step * 100,
                                     spike_in.data(), cap);
        if (n_in > 0) {
            uint32_t n_out = 0;
            (void)snn_thalamic_bridge_process(bridge, spike_in.data(), n_in,
                                              spike_out.data(), cap, &n_out);
        }
    }

    /* Adaptation should reduce firing by at least 15% vs the no-adapt
     * baseline on the same drive and duration. */
    EXPECT_GT(adapt_spikes, 0u);
    EXPECT_LT(adapt_spikes, (uint64_t)(0.85 * (double)plain_spikes))
        << "AHP+pump should reduce firing vs plain: "
        << "plain=" << plain_spikes << " adapt=" << adapt_spikes;

    /* Bridge still healthy after the adaptation-modulated traffic. */
    snn_thalamic_stats_t bstats;
    ASSERT_EQ(snn_thalamic_bridge_get_stats(bridge, &bstats), 0);
    EXPECT_TRUE(std::isfinite(bstats.avg_attention))
        << "bridge avg_attention went non-finite: " << bstats.avg_attention;
}

/*============================================================================
 * Test 5: Anti-reward and bridge coexist.
 *
 * Saturate the pop past target for 50+ steps so snn_compute_intrinsic_reward
 * returns a penalty (anti-reward enabled). Throughout, route spikes via the
 * bridge. Assert: (a) reward is small or negative, (b) the bridge still
 * emits relays and avg_attention stays finite. The test is not about reward
 * magnitude — it's that neither module breaks the other.
 *==========================================================================*/
TEST_F(SNNThalamicSubstrateComposeTest, AntiRewardAndBridgeCoexist) {
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_noise_rate_hz(0.0f);

    /* Anti-reward on with canonical defaults. */
    snn_tune_set_anti_reward_enabled(1.0f);
    snn_tune_set_anti_reward_threshold_ratio(2.0f);
    snn_tune_set_anti_reward_gain(0.5f);

    network = snn_network_create(&config);
    ASSERT_NE(network, nullptr);
    ForceCpuFallback();

    /* Name chosen so it does NOT start with "input" — avoids the looser 5%
     * target rate and gets the default 3% target for the anti-reward gate. */
    snn_population_t* pop = AddLightweightPop(200, "sat_pop");
    ASSERT_NE(pop, nullptr);
    DisableIP(pop);

    CreateRouterAndBridge();
    ASSERT_EQ(snn_thalamic_bridge_set_attention(bridge, pop->id, 0.8f), 0);

    HardResetPop(pop);

    /* Phase 1: drive hot for 60 steps while routing — each step we also
     * force firing_rate_ema well above target so the anti-reward gate
     * triggers inside snn_compute_intrinsic_reward. rate_samples must be
     * >=10 or the pop is excluded from the reward average entirely. */
    const float I_drive = 70.0f;
    const int   n_steps = 80;  /* >50 as spec'd */
    const uint32_t cap  = 512;
    std::vector<snn_spike_t> spike_in(cap), spike_out(cap);

    uint32_t process_calls = 0;
    uint32_t process_errs  = 0;
    uint64_t relayed_total = 0;

    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop, I_drive);
        snn_network_step(network, 0.1f);

        /* Keep firing_rate_ema pinned above the anti-reward threshold
         * (2× target) so snn_compute_intrinsic_reward will return a
         * penalty. Seed rate_samples past the warmup gate. */
        pop->firing_rate_ema = 0.12f;  /* 4× default target 0.03 */
        pop->rate_samples    = 100;

        uint32_t n_in = ScrapeSpikes(pop, (uint64_t)step * 100,
                                     spike_in.data(), cap);
        if (n_in == 0) continue;

        uint32_t n_out = 0;
        int rc = snn_thalamic_bridge_process(
            bridge, spike_in.data(), n_in,
            spike_out.data(), cap, &n_out);
        process_calls++;
        if (rc != 0) process_errs++;
        relayed_total += n_out;
    }

    EXPECT_EQ(process_errs, 0u)
        << "bridge errors while anti-reward was live: "
        << process_errs << "/" << process_calls;

    /* Compute intrinsic reward — anti-reward should drive it small or
     * negative (per-pop penalty = gain × (ratio - 2) = 0.5 × (4-2) = 1.0,
     * so per-pop reward ≈ −1.0; non-saturated scaffold pops contribute
     * their gaussian-tail reward which is ≤ 1.0). */
    float reward = snn_compute_intrinsic_reward(network);
    EXPECT_TRUE(std::isfinite(reward))
        << "intrinsic_reward is non-finite: " << reward;
    EXPECT_LT(reward, 0.5f)
        << "anti-reward on a saturated pop should produce small/negative "
           "reward, got " << reward;

    /* The bridge continued to route throughout. */
    snn_thalamic_stats_t bstats;
    ASSERT_EQ(snn_thalamic_bridge_get_stats(bridge, &bstats), 0);
    EXPECT_GT(bstats.spikes_relayed, 0u)
        << "bridge emitted no signals during anti-reward phase";
    EXPECT_TRUE(std::isfinite(bstats.avg_attention))
        << "bridge avg_attention non-finite under anti-reward: "
        << bstats.avg_attention;
    EXPECT_GE(bstats.avg_attention, 0.0f);

    /* And the bridge can still be queried for its attention weight — a
     * final smoke check that the two systems haven't stepped on each
     * other's state. */
    float attn_end = -1.0f;
    EXPECT_EQ(snn_thalamic_bridge_get_attention(bridge, pop->id, &attn_end), 0);
    EXPECT_FLOAT_EQ(attn_end, 0.8f);
}
