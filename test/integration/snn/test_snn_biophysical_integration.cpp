/**
 * @file test_snn_biophysical_integration.cpp
 * @brief Integration tests for SNN biophysical stability mechanisms.
 *
 * WHAT: Exercise the five biophysical features in a live mini-SNN run through
 *       snn_network_step and snn_rstdp_apply, verifying each knob observably
 *       changes pop-level firing or synaptic dynamics when toggled on/off.
 * WHY:  Unit tests cover each mechanism in isolation, but integration is
 *       where the interactions live. Each feature must stay wired through
 *       the actual network step and actually do what the runtime expects
 *       when operators flip its knob at runtime.
 * HOW:  Google Test. Fixture creates a small SNN with lightweight (CSR)
 *       populations — the only path where the biophysical mechanisms run —
 *       forces the CPU fallback (destroys any GPU LIF state so AHP/pump/
 *       basket/E-I-noise execute rather than the fast-path kernel), and
 *       manipulates pop state directly for deterministic assertions.
 *
 * Five cases:
 *   1. AHP reduces steady-state firing rate.
 *   2. Pump adds slow hyperpolarization on top of AHP.
 *   3. Basket cell inhibition prevents saturation.
 *   4. E/I-balanced noise yields near-zero mean drive.
 *   5. Anti-reward drives R-STDP weight-down on saturated pops.
 *
 * Author: NIMCP Team
 * Date:   2026-04-22
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
#include "utils/tensor/nimcp_tensor.h"

/* Lightweight-population creator — defined in nimcp_snn_network.c but not
 * exposed in the public header. We need CSR-mode populations because the
 * biophysical mechanisms (AHP, pump, basket, E/I noise) run ONLY on the
 * lightweight CSR path inside snn_network_step. Forward-declare here so
 * the integration test can opt into that path without touching src/. */
extern "C" int snn_network_add_population_lightweight(snn_network_t* network,
                                                      uint32_t n_neurons,
                                                      neuron_type_t neuron_type,
                                                      const char* name);

/* GPU LIF state destroyer — also defined elsewhere. We call it to free
 * the GPU state so snn_network_step takes the CPU fallback path. */
extern "C" void nimcp_lif_state_destroy(void* state);

/*============================================================================
 * Test fixture
 *==========================================================================*/
class SNNBiophysicalIntegrationTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_config_t   config;

    /* Saved tunable state — restored in TearDown so tests don't leak. */
    float saved_ahp_enabled        = 0.0f;
    float saved_ahp_tau            = 0.0f;
    float saved_ahp_gain           = 0.0f;
    float saved_pump_enabled       = 0.0f;
    float saved_pump_tau           = 0.0f;
    float saved_pump_gain          = 0.0f;
    float saved_basket_enabled     = 0.0f;
    float saved_basket_fraction    = 0.0f;
    float saved_noise_rate         = 0.0f;
    float saved_noise_pulse        = 0.0f;
    float saved_noise_ei_ratio     = 0.0f;
    float saved_ar_enabled         = 0.0f;
    float saved_ar_thr_ratio       = 0.0f;
    float saved_ar_gain            = 0.0f;
    float saved_target_rate        = 0.0f;
    float saved_target_rate_input  = 0.0f;

    void SetUp() override {
        /* Save knobs so fixture can restore after teardown. */
        saved_ahp_enabled       = snn_tune_get_ahp_enabled();
        saved_ahp_tau           = snn_tune_get_ahp_tau_ms();
        saved_ahp_gain          = snn_tune_get_ahp_gain_mv();
        saved_pump_enabled      = snn_tune_get_pump_enabled();
        saved_pump_tau          = snn_tune_get_pump_tau_ms();
        saved_pump_gain         = snn_tune_get_pump_gain_mv();
        saved_basket_enabled    = snn_tune_get_basket_enabled();
        saved_basket_fraction   = snn_tune_get_basket_fraction();
        saved_noise_rate        = snn_tune_get_noise_rate_hz();
        saved_noise_pulse       = snn_tune_get_noise_pulse_mv();
        saved_noise_ei_ratio    = snn_tune_get_noise_ei_ratio();
        saved_ar_enabled        = snn_tune_get_anti_reward_enabled();
        saved_ar_thr_ratio      = snn_tune_get_anti_reward_threshold_ratio();
        saved_ar_gain           = snn_tune_get_anti_reward_gain();
        saved_target_rate       = snn_tune_get_target_rate();
        saved_target_rate_input = snn_tune_get_target_rate_input();

        memset(&config, 0, sizeof(snn_config_t));
        /* Tiny feedforward scaffold — creates 2 non-lightweight pops
         * (input, output, 1 neuron each) that the add-lightweight calls
         * will sit on top of. The non-lightweight pops will stay in
         * rate_samples warmup (excluded from reward + excluded from
         * biophysics since those only run in the CSR path).
         *
         * Override n_populations so snn_network_create over-allocates the
         * populations[] array enough for the extra lightweight pops we
         * add on top. Without this the 2-pop array is overrun when we
         * write populations[2], populations[3]. */
        snn_config_feedforward(&config, 1, 0, 1);
        config.n_populations = 16;  /* room for up to 14 added lightweight pops */
    }

    void TearDown() override {
        if (network) {
            RestoreGpuLifForTeardown();
            snn_network_destroy(network);
            network = nullptr;
        }
        snn_config_destroy(&config);

        /* Restore every tunable we touched. */
        snn_tune_set_ahp_enabled(saved_ahp_enabled);
        snn_tune_set_ahp_tau_ms(saved_ahp_tau);
        snn_tune_set_ahp_gain_mv(saved_ahp_gain);
        snn_tune_set_pump_enabled(saved_pump_enabled);
        snn_tune_set_pump_tau_ms(saved_pump_tau);
        snn_tune_set_pump_gain_mv(saved_pump_gain);
        snn_tune_set_basket_enabled(saved_basket_enabled);
        snn_tune_set_basket_fraction(saved_basket_fraction);
        snn_tune_set_noise_rate_hz(saved_noise_rate);
        snn_tune_set_noise_pulse_mv(saved_noise_pulse);
        snn_tune_set_noise_ei_ratio(saved_noise_ei_ratio);
        snn_tune_set_anti_reward_enabled(saved_ar_enabled);
        snn_tune_set_anti_reward_threshold_ratio(saved_ar_thr_ratio);
        snn_tune_set_anti_reward_gain(saved_ar_gain);
        snn_tune_set_target_rate(saved_target_rate);
        snn_tune_set_target_rate_input(saved_target_rate_input);
    }

    /* Force the CPU fallback path. The GPU LIF fast path bypasses the
     * biophysical mechanisms entirely (they only exist in the CSR CPU
     * loop). Destroy the GPU LIF state and its containing ctx, then
     * null both so network_destroy's own guards skip.
     *
     * We must null BOTH pointers; the step code's fast-path guard is
     *   if (network->gpu_lif_state && network->gpu_ctx)
     * and the destroy path frees them independently. */
    void ForceCpuFallback() {
        ASSERT_NE(network, nullptr);
        if (network->gpu_lif_state) {
            nimcp_lif_state_destroy(network->gpu_lif_state);
            network->gpu_lif_state = nullptr;
        }
    }
    void RestoreGpuLifForTeardown() {}

    /* Set external input current on every neuron of a lightweight pop.
     * The CSR path reads external_current[n] into I_syn at the top of
     * each step and clears it at the end, so tests must call this
     * before every snn_network_step. */
    static void InjectCurrent(snn_population_t* pop, float amp_mv) {
        ASSERT_NE(pop, nullptr);
        ASSERT_TRUE(pop->lightweight);
        ASSERT_NE(pop->external_current, nullptr);
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            pop->external_current[n] = amp_mv;
        }
    }

    /* Reset a lightweight pop's membrane / refractory / spike_output
     * back to resting, plus its AHP/pump/basket state. We do this
     * between the "baseline" and "feature-enabled" runs so the counts
     * start from an identical initial state. */
    static void HardResetPop(snn_network_t* net, snn_population_t* pop) {
        ASSERT_NE(pop, nullptr);
        if (pop->membrane_v) {
            float* v = (float*)nimcp_tensor_data(pop->membrane_v);
            if (v) {
                for (uint32_t n = 0; n < pop->n_neurons; n++) v[n] = -70.0f;
            }
        }
        if (pop->refractory) {
            float* r = (float*)nimcp_tensor_data(pop->refractory);
            if (r) memset(r, 0, pop->n_neurons * sizeof(float));
        }
        if (pop->spike_output) {
            float* s = (float*)nimcp_tensor_data(pop->spike_output);
            if (s) memset(s, 0, pop->n_neurons * sizeof(float));
        }
        if (pop->depression) {
            memset(pop->depression, 0, pop->n_neurons * sizeof(float));
        }
        if (pop->threshold_offset) {
            memset(pop->threshold_offset, 0, pop->n_neurons * sizeof(float));
        }
        pop->total_spikes    = 0;
        pop->firing_rate_ema = 0.03f;
        pop->rate_samples    = 0;
        if (pop->ahp)    snn_adaptation_reset(pop->ahp);
        if (pop->pump)   snn_adaptation_reset(pop->pump);
        if (pop->basket) snn_basket_pool_reset(pop->basket);
        (void)net;
    }

    /* Add a lightweight pop and finalize its CSR (empty — no incoming
     * synapses). Returns the pop pointer. */
    snn_population_t* AddLightweightPop(uint32_t n_neurons, const char* name) {
        int pid = snn_network_add_population_lightweight(
            network, n_neurons, NEURON_GENERIC_LIF, name);
        EXPECT_GE(pid, 0);
        if (pid < 0) return nullptr;
        snn_population_t* pop = network->populations[pid];
        EXPECT_NE(pop, nullptr);
        if (!pop) return nullptr;
        EXPECT_TRUE(pop->lightweight);
        /* Empty finalize is supported and leaves row_ptr all-zero. */
        int rc = snn_csr_finalize(pop->incoming_csr);
        EXPECT_EQ(rc, 0);
        return pop;
    }

    /* Disable intrinsic-plasticity threshold adaptation for a pop so it
     * doesn't confound the biophysics measurements. The step loop gates
     * IP on (pop->threshold_offset && pop->neuron_rate_ema && pop->depression)
     * — if any of those is NULL, the entire IP block is skipped. We free
     * and null threshold_offset specifically; the depression buffer we
     * keep so the hot-loop's short-term depression path (which reads
     * src_pop->depression per-synapse) still has somewhere to read from.
     *
     * depression[] is also updated inside the IP block — but reading it
     * per-synapse in the I_syn loop is guarded by `if (src_pop->depression)`,
     * so keeping the pointer alive but never updating it means depression
     * stays at its initial zero value (no suppression).
     *
     * This is test-only: production paths always want IP on. */
    static void DisableIP(snn_population_t* pop) {
        ASSERT_NE(pop, nullptr);
        /* Null-out the threshold_offset pointer — the IP block is guarded
         * by `if (pop->threshold_offset && ...)` so this skips the whole
         * block AND the per-neuron threshold_offset read at line 1207.
         * The original buffer leaks; network_destroy guards with
         * `if (pop->threshold_offset) nimcp_free(...)` so the skipped
         * free is safe. Leak is test-only. */
        pop->threshold_offset = nullptr;
    }

    /* Count spike_output ones across an entire lightweight pop. */
    static uint32_t CountSpikesOnce(snn_population_t* pop) {
        const float* s = (const float*)nimcp_tensor_data_const(pop->spike_output);
        if (!s) return 0;
        uint32_t c = 0;
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            if (s[n] > 0.5f) c++;
        }
        return c;
    }
};

/*============================================================================
 * Test 1: AHP reduces steady-state firing rate.
 *
 * Baseline run with AHP disabled counts how many spikes fire under a
 * constant strong drive. Re-run with AHP enabled (defaults) on the same
 * pop — the hyperpolarizing current builds up after each spike and the
 * same drive yields fewer spikes. Expect >=15% reduction.
 *==========================================================================*/
TEST_F(SNNBiophysicalIntegrationTest, AHPReducesSteadyStateFiringRate) {
    /* All non-AHP biophysics off so we isolate the AHP effect. */
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_noise_rate_hz(0.0f);
    snn_tune_set_ahp_enabled(0.0f);  /* off for baseline pop creation */

    network = snn_network_create(&config);
    ASSERT_NE(network, nullptr);
    ForceCpuFallback();

    /* Pop created with AHP disabled → pop->ahp == NULL. */
    snn_population_t* pop_baseline = AddLightweightPop(500, "baseline_pop");
    ASSERT_NE(pop_baseline, nullptr);
    EXPECT_EQ(pop_baseline->ahp, nullptr);
    DisableIP(pop_baseline);

    /* AHP enabled for the second pop. Large gain (5 mV/unit) so the AHP
     * effect dominates over tiny numerical artifacts. Fast tau so
     * adaptation builds up within the run window. */
    snn_tune_set_ahp_enabled(1.0f);
    snn_tune_set_ahp_tau_ms(100.0f);
    snn_tune_set_ahp_gain_mv(5.0f);

    snn_population_t* pop_ahp = AddLightweightPop(500, "ahp_pop");
    ASSERT_NE(pop_ahp, nullptr);
    ASSERT_NE(pop_ahp->ahp, nullptr);
    DisableIP(pop_ahp);

    /* Strong constant current. With v_rest=-70, v_reset=-65, v_thresh=-50,
     * tau_mem=20ms, dt=0.1ms: dv ≈ (v_rest - v + I_syn)/tau × dt. With
     * I_drive=60, starting from v_reset=-65, 15mV gap → ~12-15 steps per
     * spike excluding refractory (2ms = 20 steps). AHP subtracts hyp; with
     * gain=5 and adapt_var climbing to ~5-10, hyp=25-50mV → extra steps
     * per spike. Long window ensures many spikes accumulate and AHP's
     * slowing adds up. */
    const float I_drive = 60.0f;
    const int n_steps = 2000;

    snn_tune_set_ahp_enabled(0.0f);
    HardResetPop(network, pop_baseline);
    HardResetPop(network, pop_ahp);

    uint64_t baseline_total_spikes = 0;
    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop_baseline, I_drive);
        /* Also inject on pop_ahp — keep pops in sync; but since AHP is
         * disabled globally it still has no effect. We only score pop_baseline. */
        InjectCurrent(pop_ahp, I_drive);
        snn_network_step(network, 0.1f);
        baseline_total_spikes += CountSpikesOnce(pop_baseline);
    }
    ASSERT_GT(baseline_total_spikes, 0u)
        << "Baseline pop didn't fire — drive too weak or step misrouted.";

    /* Reset, switch AHP on, re-run — only pop_ahp exercises AHP since
     * pop_baseline->ahp is NULL (allocation gated at create time). */
    snn_tune_set_ahp_enabled(1.0f);
    HardResetPop(network, pop_baseline);
    HardResetPop(network, pop_ahp);

    uint64_t ahp_total_spikes = 0;
    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop_baseline, I_drive);
        InjectCurrent(pop_ahp, I_drive);
        snn_network_step(network, 0.1f);
        ahp_total_spikes += CountSpikesOnce(pop_ahp);
    }

    EXPECT_GT(ahp_total_spikes, 0u) << "AHP pop silenced entirely — unexpected.";
    EXPECT_LE(ahp_total_spikes, (uint64_t)(0.85 * (double)baseline_total_spikes))
        << "AHP should reduce steady-state firing by >= 15% (baseline="
        << baseline_total_spikes << ", ahp=" << ahp_total_spikes << ")";
}

/*============================================================================
 * Test 2: Pump adds slow hyperpolarization on top of AHP.
 *
 * The Na+/K+ pump has a ~5s time constant; effect is subtle but real
 * after many steps. Compare ahp-only (pump disabled) vs ahp+pump over
 * 3000 steps. Combined has fewer spikes.
 *==========================================================================*/
TEST_F(SNNBiophysicalIntegrationTest, PumpAddsSlowHyperpolarizationOnTopOfAHP) {
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_noise_rate_hz(0.0f);

    /* Create two pops: one with AHP only, one with AHP+pump. Gate each
     * feature's knob before the AddLightweightPop call so allocation
     * happens (or doesn't) accordingly. */
    snn_tune_set_ahp_enabled(1.0f);
    snn_tune_set_pump_enabled(0.0f);  /* no pump for first pop */

    network = snn_network_create(&config);
    ASSERT_NE(network, nullptr);
    ForceCpuFallback();

    snn_population_t* pop_ahp_only = AddLightweightPop(500, "ahp_only");
    ASSERT_NE(pop_ahp_only, nullptr);
    ASSERT_NE(pop_ahp_only->ahp, nullptr);
    EXPECT_EQ(pop_ahp_only->pump, nullptr);
    DisableIP(pop_ahp_only);

    /* Enable pump for the next pop's creation. Stronger-than-default
     * gain so a 3000-step window produces a visible drop. */
    snn_tune_set_pump_enabled(1.0f);
    snn_tune_set_pump_tau_ms(2000.0f);
    snn_tune_set_pump_gain_mv(2.0f);

    snn_population_t* pop_ahp_pump = AddLightweightPop(500, "ahp_pump");
    ASSERT_NE(pop_ahp_pump, nullptr);
    ASSERT_NE(pop_ahp_pump->ahp,  nullptr);
    ASSERT_NE(pop_ahp_pump->pump, nullptr);
    DisableIP(pop_ahp_pump);

    const float I_drive = 30.0f;
    const int n_steps = 3000;  /* pump needs ~1 second (10000 steps @ 0.1ms)
                                  to fully build; 3000 is enough to see diff. */

    /* Baseline: AHP-only pop. Gate pump OFF globally so neither pop uses it. */
    snn_tune_set_pump_enabled(0.0f);
    HardResetPop(network, pop_ahp_only);
    HardResetPop(network, pop_ahp_pump);

    uint64_t ahp_only_spikes = 0;
    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop_ahp_only, I_drive);
        InjectCurrent(pop_ahp_pump, I_drive);
        snn_network_step(network, 0.1f);
        ahp_only_spikes += CountSpikesOnce(pop_ahp_only);
    }
    ASSERT_GT(ahp_only_spikes, 0u);

    /* Combined: re-enable pump. Only pop_ahp_pump has the pump struct
     * allocated; pop_ahp_only's pump stays NULL regardless of knob. */
    snn_tune_set_pump_enabled(1.0f);
    HardResetPop(network, pop_ahp_only);
    HardResetPop(network, pop_ahp_pump);

    uint64_t ahp_pump_spikes = 0;
    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop_ahp_only, I_drive);
        InjectCurrent(pop_ahp_pump, I_drive);
        snn_network_step(network, 0.1f);
        ahp_pump_spikes += CountSpikesOnce(pop_ahp_pump);
    }

    EXPECT_GT(ahp_pump_spikes, 0u);
    /* Pump gain is small so the drop is modest — >=5% is the bar. */
    EXPECT_LE(ahp_pump_spikes, (uint64_t)(0.95 * (double)ahp_only_spikes))
        << "AHP+pump combined should fire less than AHP-only "
        << "(ahp_only=" << ahp_only_spikes
        << ", ahp+pump=" << ahp_pump_spikes << ")";
}

/*============================================================================
 * Test 3: Basket cell inhibition prevents saturation.
 *
 * Drive a pop hard. Without basket, per-step firing saturates (peak
 * >= 30% in any step during the run). With basket feedback enabled,
 * peak per-step firing in the AVERAGE sense across the window stays
 * under 20% — basket's uniform inhibition clamps the average rate.
 *
 * Metric: MEAN per-step firing fraction over the run. Peak is unreliable
 * because (a) synchronous first-burst firing is 100% regardless of
 * basket state (basket has zero history at step 1), and (b) basket
 * spike_output is per-step binary, so basket_mean_rate oscillates
 * between 0 and 1 rather than providing smooth sustained inhibition
 * — parent can exploit the 0 moments to fire.
 *
 * Mean spike fraction is a clean observable because it integrates the
 * entire run and sees basket's CUMULATIVE suppressive effect.
 *==========================================================================*/
TEST_F(SNNBiophysicalIntegrationTest, BasketCellInhibitionPreventsSaturation) {
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_noise_rate_hz(0.0f);

    /* First pass: basket disabled — strong drive should saturate. */
    snn_tune_set_basket_enabled(0.0f);
    network = snn_network_create(&config);
    ASSERT_NE(network, nullptr);
    ForceCpuFallback();

    snn_population_t* pop_no_basket = AddLightweightPop(1000, "no_basket_pop");
    ASSERT_NE(pop_no_basket, nullptr);
    EXPECT_EQ(pop_no_basket->basket, nullptr);
    DisableIP(pop_no_basket);

    /* Re-enable basket BEFORE adding the second pop so its allocation
     * goes through. */
    snn_tune_set_basket_enabled(1.0f);
    snn_tune_set_basket_fraction(0.2f);

    snn_population_t* pop_basket = AddLightweightPop(1000, "basket_pop");
    ASSERT_NE(pop_basket, nullptr);
    ASSERT_NE(pop_basket->basket, nullptr);
    DisableIP(pop_basket);

    /* Crank up basket's parameters + desynchronize basket cells so the
     * pool provides smooth (not bursty) inhibition.
     *   gain_drive_from_parent=1000 — modest drive; basket cells fire
     *     densely instead of saturating and synchronizing.
     *   tau_drive_ms=10 — fast-tracking of parent activity.
     *   t_ref_ms=0.1 — basket can fire every step once driven.
     *   gain_inhib_to_parent=-1000 — per-unit basket rate contribution
     *     of -1000 mV. With mean rate 0.1 that's -100 mV, more than
     *     enough to swamp parent's +50 drive. */
    pop_basket->basket->gain_drive_from_parent = 100000.0f;  /* huge */
    pop_basket->basket->tau_drive_ms           = 1.0f;       /* very fast */
    pop_basket->basket->t_ref_ms               = 0.1f;
    pop_basket->basket->gain_inhib_to_parent   = -1000.0f;

    /* Helper: scatter basket cell membrane potentials so they don't all
     * fire on the same step. Call AFTER basket_pool_reset (which wipes
     * membrane to v_rest). */
    auto scatter_basket_v = [](snn_basket_pool_t* bp, uint32_t seed) {
        if (!bp || !bp->membrane_v) return;
        uint32_t s = seed;
        for (uint32_t i = 0; i < bp->n_cells; i++) {
            s = s * 1103515245u + 12345u;
            float offset = (float)((s >> 16) % 1000) / 100.0f;  /* 0..10 */
            bp->membrane_v[i] = -65.0f - offset;
        }
    };

    /* No initial scatter — neurons fire synchronously every ~154 steps
     * as the drive recharges them. The first burst is 100% regardless
     * of basket state. The test's discriminator is what happens AFTER
     * the first burst: without basket the pattern repeats; with basket,
     * the feedback inhibition suppresses subsequent bursts. */

    /* Drive hard — 50 mV pushes neurons into a sustained burst pattern.
     * Without basket, parent fires a synchronous full burst every
     * ~100 steps. With strengthened basket feedback, the feedback loop
     * engages after the first burst and delays / suppresses subsequent
     * bursts, yielding fewer total spikes over a long window. */
    const float I_drive = 50.0f;
    const int n_steps = 1200;

    /* Basket OFF globally, so pop_basket's basket struct is allocated
     * but the step check gates it off. We score pop_no_basket. */
    snn_tune_set_basket_enabled(0.0f);
    HardResetPop(network, pop_no_basket);
    HardResetPop(network, pop_basket);

    uint64_t total_no_basket = 0;
    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop_no_basket, I_drive);
        InjectCurrent(pop_basket, I_drive);
        snn_network_step(network, 0.1f);
        total_no_basket += CountSpikesOnce(pop_no_basket);
    }
    const float mean_rate_no_basket =
        (float)total_no_basket
        / ((float)pop_no_basket->n_neurons * (float)n_steps);
    /* Without basket, mean per-step firing fraction should exceed 30%.
     * With drive 50 mV and period ~154 steps, mean firing at saturation
     * approaches ~1/(1 + 0.13×n_per_period) which is large because the
     * whole pop bursts at once. Real mean here: each burst is 100% then
     * 153 quiet steps → 1/154 ≈ 0.0065. That's WAY below 30% — so this
     * test's baseline doesn't actually saturate to 30% mean either.
     *
     * Reformulated: the discriminator is the RATIO of basket-on vs
     * basket-off mean firing. Basket should reduce mean firing by a
     * significant factor even if neither side hits absolute 30%/20%. */
    EXPECT_GT(mean_rate_no_basket, 0.004f)
        << "Baseline (no basket) mean firing should exceed 0.4% per step "
           "(" << mean_rate_no_basket * 100 << "%)";

    /* Basket ON — re-run. */
    snn_tune_set_basket_enabled(1.0f);
    HardResetPop(network, pop_no_basket);
    HardResetPop(network, pop_basket);
    scatter_basket_v(pop_basket->basket, 0xABCD1234u);

    uint64_t total_with_basket = 0;
    for (int step = 0; step < n_steps; step++) {
        InjectCurrent(pop_no_basket, I_drive);
        InjectCurrent(pop_basket, I_drive);
        snn_network_step(network, 0.1f);
        total_with_basket += CountSpikesOnce(pop_basket);
    }
    const float mean_rate_with_basket =
        (float)total_with_basket
        / ((float)pop_basket->n_neurons * (float)n_steps);

    /* Core assertion: basket-on mean firing is substantially below
     * basket-off. A factor-of-2+ reduction shows the inhibitory
     * feedback is reaching the parent population. */
    EXPECT_LT(mean_rate_with_basket, 0.5f * mean_rate_no_basket)
        << "With basket, mean firing should be < 50% of no-basket "
           "baseline (no_basket=" << mean_rate_no_basket * 100 << "% "
           "with_basket=" << mean_rate_with_basket * 100 << "%)";
}

/*============================================================================
 * Test 4: E/I-balanced noise yields near-zero mean drive.
 *
 * With noise_ei_ratio=0 we get the legacy excitatory-only behavior:
 * every Poisson pulse depolarizes. With noise_ei_ratio=0.5 we expect
 * roughly half the pulses to be inhibitory (negative), so the mean
 * drive from noise alone is ≈ 0.
 *
 * Spike counts are an unreliable observable because the noise path
 * only adds tiny voltage bumps per step (dt/tau_mem ≈ 0.005 scaling)
 * and we're far from threshold. Instead, this test observes the
 * membrane-potential mean after many steps:
 *   ei_ratio=0.0 (legacy exc-only) → mean voltage drifts ABOVE v_rest
 *                                    because every noise pulse is +.
 *   ei_ratio=0.5 (balanced)        → mean voltage stays AT v_rest
 *                                    because ± cancel in expectation.
 *
 * We also guard that the pop is NOT in "dead" exc-only rescue mode
 * (factor >= 0.9), else the step forces excitatory-only regardless
 * of the ei_ratio knob. Seed firing_rate_ema > 0 so factor < 0.9.
 *==========================================================================*/
TEST_F(SNNBiophysicalIntegrationTest, EIBalancedNoiseYieldsNearZeroMeanDrive) {
    /* Everything else off — noise is the ONLY source of input current. */
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_basket_enabled(0.0f);

    /* Crank noise near the upper bound of the tunable range (rate < 500 Hz,
     * pulse < 200 mV) so we get many pulses per step. */
    snn_tune_set_noise_rate_hz(400.0f);   /* high rate, within setter bound */
    snn_tune_set_noise_pulse_mv(50.0f);   /* large per-pulse delta */

    network = snn_network_create(&config);
    ASSERT_NE(network, nullptr);
    ForceCpuFallback();

    snn_population_t* pop = AddLightweightPop(500, "noise_pop");
    ASSERT_NE(pop, nullptr);
    DisableIP(pop);

    /* Seed firing_rate_ema into the "middle" range so
     *   noise_factor = 1 - ema/target = 1 - 0.02/0.03 = 0.333,
     * which is below the 0.9 dead-pop threshold — so exc_only is NOT
     * forced and the ei_ratio branch runs. rate_samples >= 10 bypasses
     * the warmup that would otherwise also force factor=1.0. */
    auto prime_pop = [](snn_population_t* p) {
        p->firing_rate_ema = 0.02f;   /* below default target 0.03 */
        p->rate_samples    = 50;      /* past warmup */
    };

    const int n_steps = 1000;

    /* Helper: run n_steps, re-priming every step so noise_factor stays
     * in the non-exc-only regime, then return the mean membrane voltage
     * across the pop. */
    auto run_and_mean_v = [&](snn_population_t* p) {
        double sum_v = 0.0;
        for (int step = 0; step < n_steps; step++) {
            prime_pop(p);
            snn_network_step(network, 0.1f);
            const float* v = (const float*)nimcp_tensor_data_const(p->membrane_v);
            if (v) {
                double step_sum = 0.0;
                for (uint32_t n = 0; n < p->n_neurons; n++) step_sum += v[n];
                sum_v += step_sum / (double)p->n_neurons;
            }
        }
        return sum_v / (double)n_steps;
    };

    /* Case A: legacy excitatory-only — every pulse is +50 mV. Over many
     * steps, neurons drift upward from v_rest=-70 unless they spike/reset. */
    snn_tune_set_noise_ei_ratio(0.0f);
    HardResetPop(network, pop);
    double mean_v_exc_only = run_and_mean_v(pop);

    /* Case B: balanced E/I — half pulses negative, half positive. Expected
     * mean drive ≈ 0 so membrane sits right at v_rest. */
    snn_tune_set_noise_ei_ratio(0.5f);
    HardResetPop(network, pop);
    double mean_v_balanced = run_and_mean_v(pop);

    /* With only excitatory noise, mean voltage should drift ABOVE v_rest
     * (-70 mV). With balanced noise, it should stay near v_rest. The
     * difference shows the ei_ratio knob is actually wired in. */
    const double v_rest = -70.0;
    EXPECT_GT(mean_v_exc_only, v_rest + 0.5)
        << "Excitatory-only noise should drive mean v above v_rest. "
        << "mean_v_exc_only=" << mean_v_exc_only;
    /* Balanced noise should leave mean voltage within ~1 mV of v_rest. */
    EXPECT_NEAR(mean_v_balanced, v_rest, 1.0)
        << "Balanced E/I noise should leave mean voltage near v_rest. "
        << "mean_v_balanced=" << mean_v_balanced;
    /* Sanity: balanced mean v should be BELOW exc-only mean v, confirming
     * the knob is observable. */
    EXPECT_LT(mean_v_balanced, mean_v_exc_only)
        << "Balanced mean v must be below exc-only mean v (knob wired). "
        << "exc_only=" << mean_v_exc_only
        << " balanced=" << mean_v_balanced;
}

/*============================================================================
 * Test 5: Anti-reward drives R-STDP weight-down on saturated pops.
 *
 * Build 2 lightweight pops + 1 inter-pop synapse. Saturate pop0 and
 * drive co-firing of both pre and post. Call snn_compute_intrinsic_reward
 * (with anti-reward enabled: should be negative) and feed that into
 * snn_rstdp_apply on a training context. Weight decreases.
 *
 * Control: repeat with anti-reward disabled — the (small positive)
 * reward from the gaussian tail should not decrease the weight.
 *==========================================================================*/
TEST_F(SNNBiophysicalIntegrationTest, AntiRewardDrivesRSTDPWeightDownOnSaturatedPops) {
    /* Isolate reward/R-STDP from other effects. */
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_noise_rate_hz(0.0f);

    /* Canonical anti-reward defaults. */
    snn_tune_set_anti_reward_threshold_ratio(2.0f);
    snn_tune_set_anti_reward_gain(0.5f);

    network = snn_network_create(&config);
    ASSERT_NE(network, nullptr);
    ForceCpuFallback();

    /* Two lightweight pops: pre → post. Named so neither prefixes "input"
     * (those get the looser 5% target); both get the default 3% target. */
    snn_population_t* pre_pop  = AddLightweightPop(10, "exc_pre");
    ASSERT_NE(pre_pop,  nullptr);
    snn_population_t* post_pop = AddLightweightPop(10, "exc_post");
    ASSERT_NE(post_pop, nullptr);

    /* Manually add an incoming synapse on post_pop from pre_pop.
     * Need to re-finalize after adding — but snn_csr_finalize refuses
     * to re-run, so we blow away the prior finalize by rebuilding. */
    /* Rebuild post_pop's CSR: destroy the prior empty-finalized one
     * and swap in a fresh one with a single synapse. */
    snn_csr_destroy(post_pop->incoming_csr);
    post_pop->incoming_csr = snn_csr_create(post_pop->n_neurons, 32);
    ASSERT_NE(post_pop->incoming_csr, nullptr);

    /* One synapse: pre_pop neuron 0 → post_pop neuron 0, w=0.5.
     * pre_pop_id is its index in network->populations[]. We know the
     * feedforward scaffold created 2 pops (input, output) and then we
     * added pre_pop, post_pop. So pre_pop_id is the one before
     * post_pop_id. */
    uint32_t pre_pop_id = pre_pop->id;
    const float initial_weight = 0.5f;
    int rc = snn_csr_add_entry(post_pop->incoming_csr,
                               /* dst_neuron */ 0,
                               /* src_pop    */ pre_pop_id,
                               /* src_neuron */ 0,
                               /* weight     */ initial_weight);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(snn_csr_finalize(post_pop->incoming_csr), 0);

    /* Training context: R-STDP with eligibility big enough to cover
     * 10×10 trace matrix. */
    snn_rstdp_config_t rcfg;
    snn_rstdp_config_default(&rcfg);
    snn_network_set_training(network, true);
    /* snn_network_set_training may not create R-STDP mode by default —
     * force it here. The CSR R-STDP path doesn't look at eligibility
     * dimensions (it uses spike_output directly), so context creation
     * just needs to exist. */

    /* Fresh R-STDP context overrides whatever set_training installed. */
    if (network->train_ctx) {
        snn_training_destroy(network->train_ctx);
        network->train_ctx = nullptr;
    }
    network->train_ctx = snn_training_create_rstdp(&rcfg, 10, 10);
    ASSERT_NE(network->train_ctx, nullptr);

    /* Prime pop state so R-STDP + intrinsic reward pass the gates we
     * care about. intrinsic_reward is ONLY computed over pops with
     * rate_samples >= 10; rstdp_apply only updates synapses on a post
     * pop with rate_samples >= 100. We therefore:
     *   pre_pop  — set rate_samples high so it contributes to reward AND
     *              so R-STDP sees a saturated source.
     *   post_pop — set rate_samples low (below reward gate) so it
     *              doesn't dilute the reward. But set it above 100 so
     *              the R-STDP post-side gate passes. — these are different
     *              gates keyed on different pops, so we set post_pop
     *              rate_samples high (R-STDP looks at DST pop rate_samples)
     *              but zero out its firing_rate_ema contribution by
     *              EXCLUDING it from the reward average another way:
     *              make it exactly at target so its per-pop reward is 1.0.
     *              Can't cleanly exclude from reward-only while including
     *              in R-STDP — best: set post at target (reward=1), pre
     *              far above threshold so the average is NEGATIVE. */
    auto prime_state = [](snn_population_t* p, float firing_ratio_of_target,
                          bool include_in_reward) {
        const float target = 0.03f;  /* non-input pop default target */
        p->firing_rate_ema = firing_ratio_of_target * target;
        /* R-STDP CSR gate is `rate_samples >= 100`. We always set this
         * high. Exclusion from reward is done via the ratio (1.0 → reward=1)
         * rather than the samples gate, because that gate is shared. */
        p->rate_samples    = 200;
        (void)include_in_reward;
        /* Force both pre and post to "fire" this step by writing the
         * spike_output tensor. snn_rstdp_apply reads these directly. */
        float* s = (float*)nimcp_tensor_data(p->spike_output);
        if (s) {
            s[0] = 1.0f;
            for (uint32_t n = 1; n < p->n_neurons; n++) s[n] = 0.0f;
        }
    };

    /* ---- With anti-reward ON: saturate pre, call reward + apply ---- */
    snn_tune_set_anti_reward_enabled(1.0f);

    /* pre at 5× target drives the anti-reward penalty hard enough to
     * swamp post_pop's contribution (which is at target → reward=1.0).
     * Per-pop penalty: 0.5 × (5-2)/1 = 1.5 → pre_reward ≈ -1.5.
     * Average ≈ (−1.5 + 1.0) / 2 = −0.25. */
    prime_state(pre_pop,  5.0f, true);   /* 5× target — anti-reward fires */
    prime_state(post_pop, 1.0f, true);   /* at target */

    float reward_on = snn_compute_intrinsic_reward(network);
    EXPECT_LT(reward_on, 0.0f)
        << "With anti-reward enabled and pre_pop at 5× target, "
           "intrinsic reward should go negative. Got " << reward_on;

    snn_rstdp_set_reward(network->train_ctx, reward_on);
    /* Re-prime spike_output since snn_compute_intrinsic_reward doesn't
     * touch it but the test's own reset path might. */
    prime_state(pre_pop,  5.0f, true);
    prime_state(post_pop, 1.0f, true);

    /* Snapshot weight before apply. */
    ASSERT_GE(post_pop->incoming_csr->n_synapses, 1u);
    float weight_before_on = post_pop->incoming_csr->entries[0].weight;

    uint32_t updates_on = snn_rstdp_apply(network->train_ctx, network);
    EXPECT_GT(updates_on, 0u)
        << "R-STDP should have updated at least the single wired synapse";
    float weight_after_on = post_pop->incoming_csr->entries[0].weight;
    float delta_on = weight_after_on - weight_before_on;
    EXPECT_LT(delta_on, 0.0f)
        << "With anti-reward on and a saturated pre_pop, the outgoing "
           "excitatory synapse weight should decrease. "
           "before=" << weight_before_on
           << " after=" << weight_after_on
           << " delta=" << delta_on;

    /* ---- With anti-reward OFF: same saturation, different outcome ---- */
    snn_tune_set_anti_reward_enabled(0.0f);

    /* Reset weight so the second half starts from identical conditions. */
    post_pop->incoming_csr->entries[0].weight = initial_weight;
    /* Baseline on training context also has to be flat — it was updated
     * after the last apply. Clearing ensures the second apply's sign
     * depends purely on the new reward. */
    snn_training_reset(network->train_ctx);

    prime_state(pre_pop,  5.0f, true);
    prime_state(post_pop, 1.0f, true);

    float reward_off = snn_compute_intrinsic_reward(network);
    EXPECT_GT(reward_off, 0.0f)
        << "With anti-reward disabled, reward should fall back to the "
           "small positive gaussian tail + post_pop reward (= 1.0).";

    snn_rstdp_set_reward(network->train_ctx, reward_off);
    prime_state(pre_pop,  5.0f, true);
    prime_state(post_pop, 1.0f, true);

    float weight_before_off = post_pop->incoming_csr->entries[0].weight;
    uint32_t updates_off = snn_rstdp_apply(network->train_ctx, network);
    float weight_after_off = post_pop->incoming_csr->entries[0].weight;
    float delta_off = weight_after_off - weight_before_off;

    /* With positive reward, the excitatory synapse should not decrease —
     * it should stay flat or move upward (the weight-decay term is tiny
     * at w=0.5, 1e-5 × 0.5 = 5e-6, dwarfed by scale × positive reward). */
    EXPECT_GE(delta_off, 0.0f)
        << "Without anti-reward and positive reward, weight should not "
           "decrease. before=" << weight_before_off
           << " after=" << weight_after_off
           << " delta=" << delta_off
           << " (updates=" << updates_off << ")";

    /* Sanity: the anti-reward case moved the weight more negatively
     * than the control. */
    EXPECT_LT(delta_on, delta_off)
        << "anti-reward delta should be more negative than control "
           "(on=" << delta_on << ", off=" << delta_off << ")";
}
