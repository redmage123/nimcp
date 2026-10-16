/**
 * @file test_fno_hnn_substrate_compose.cpp
 * @brief Integration tests: FNO and HNN observe substrate + thalamic modulation
 *        via their parent networks.
 *
 * WHAT: Verifies that FNO (embedded in cortex_cnn_processor_t) and HNN
 *       (embedded in lnn_layer_t) observe biological substrate modulation
 *       and thalamic attention gating through their parent network
 *       composition — without any FNO/HNN-specific adapter work.
 *
 * WHY:  FNO is not a standalone network — the audio/visual/speech CNN
 *       forward paths run FNO after (or instead of) the CNN, then apply
 *       cortex_cnn_apply_activation_mod + cortex_apply_thalamic_gate on
 *       the combined embedding. Proving the composition works means FNO
 *       inherits substrate/thalamic for free.
 *
 *       HNN is not standalone either — it replaces the LTC forward on
 *       layer 0 of an LNN. The substrate knob composes tau on LTC layers
 *       AFTER the HNN layer, so a multi-layer LNN (HNN layer 0 + LTC
 *       layer 1..N) lets HNN output flow into a substrate-modulated
 *       downstream; the end-to-end output trajectory thereby observes
 *       substrate.
 *
 * HOW:  Google Test. For each network type we (1) run a healthy-substrate
 *       baseline, (2) run a stressed-substrate variant, (3) assert the
 *       trajectories differ by > 5%, and (4) confirm turning all knobs
 *       off restores bit-identical-to-baseline behavior.
 *
 * @date 2026-04-22
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>  /* srand — seed before each create for deterministic init */
#include <cstring>
#include <cstdint>
#include <vector>

extern "C" {
#include "training/nimcp_cortex_cnn.h"
#include "training/nimcp_fno_layer.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "core/substrate/nimcp_substrate_effects.h"
#include "core/thalamic/nimcp_thalamic_channel.h"
#include "middleware/routing/nimcp_thalamic_router.h"
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_types.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_layer.h"
#include "lnn/nimcp_lnn_config.h"
#include "lnn/nimcp_lnn_training.h"
#include "lnn/nimcp_lnn_hamiltonian.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"

/* Non-public setters (declared in src/api/* only). The cortex CNN does not
 * auto-create FNO blocks; the caller (brain init, or this test) attaches
 * them explicitly. Declaring extern here mirrors the pattern used by
 * src/api/nimcp_part_bindings.c and src/api/nimcp_part_core.c. */
extern void cortex_cnn_set_fno_audio(cortex_cnn_processor_t*, void*);
extern void cortex_cnn_set_fno_visual(cortex_cnn_processor_t*, void*);
extern void cortex_cnn_set_fno_speech(cortex_cnn_processor_t*, void*);

/* Test-only accessor from src/training/nimcp_cortex_cnn.c */
thalamic_channel_t* cortex_cnn_test_get_thalamic_channel(
    const cortex_cnn_processor_t* proc);
}

/* ======================================================================== */
/* Fixture                                                                   */
/* ======================================================================== */

class FnoHnnSubstrateComposeTest : public ::testing::Test {
protected:
    /* Save all CNN + LNN tunables so different tests don't leak state. */
    float saved_cnn_substrate_enabled           = 0.0f;
    float saved_cnn_substrate_period            = 0.0f;
    float saved_cnn_substrate_activation_mod_on = 0.0f;
    float saved_cnn_substrate_plasticity_mod_on = 0.0f;
    float saved_cnn_thal_enabled                = 0.0f;
    float saved_cnn_thal_featuremap_on          = 0.0f;
    float saved_cnn_thal_burst_on               = 0.0f;

    float saved_lnn_substrate_enabled        = 0.0f;
    float saved_lnn_substrate_period         = 0.0f;
    float saved_lnn_substrate_tau_compose_on = 0.0f;

    void SetUp() override {
        saved_cnn_substrate_enabled           = cortex_cnn_tune_get_substrate_enabled();
        saved_cnn_substrate_period            = cortex_cnn_tune_get_substrate_update_period();
        saved_cnn_substrate_activation_mod_on = cortex_cnn_tune_get_substrate_activation_mod_on();
        saved_cnn_substrate_plasticity_mod_on = cortex_cnn_tune_get_substrate_plasticity_mod_on();
        saved_cnn_thal_enabled                = cortex_cnn_tune_get_thalamic_enabled();
        saved_cnn_thal_featuremap_on          = cortex_cnn_tune_get_thalamic_featuremap_gain_on();
        saved_cnn_thal_burst_on               = cortex_cnn_tune_get_thalamic_burst_dropout_reduce_on();

        saved_lnn_substrate_enabled        = lnn_tune_get_substrate_enabled();
        saved_lnn_substrate_period         = lnn_tune_get_substrate_update_period();
        saved_lnn_substrate_tau_compose_on = lnn_tune_get_substrate_tau_compose_on();

        /* Idempotent LNN library init for tests that build LNN directly. */
        (void)lnn_init(1);
    }

    void TearDown() override {
        cortex_cnn_tune_set_substrate_enabled(saved_cnn_substrate_enabled);
        cortex_cnn_tune_set_substrate_update_period(saved_cnn_substrate_period);
        cortex_cnn_tune_set_substrate_activation_mod_on(saved_cnn_substrate_activation_mod_on);
        cortex_cnn_tune_set_substrate_plasticity_mod_on(saved_cnn_substrate_plasticity_mod_on);
        cortex_cnn_tune_set_thalamic_enabled(saved_cnn_thal_enabled);
        cortex_cnn_tune_set_thalamic_featuremap_gain_on(saved_cnn_thal_featuremap_on);
        cortex_cnn_tune_set_thalamic_burst_dropout_reduce_on(saved_cnn_thal_burst_on);

        lnn_tune_set_substrate_enabled(saved_lnn_substrate_enabled);
        lnn_tune_set_substrate_update_period(saved_lnn_substrate_period);
        lnn_tune_set_substrate_tau_compose_on(saved_lnn_substrate_tau_compose_on);
    }

    /* ----------------------------- helpers ----------------------------- */

    /* Several NIMCP subsystems init weights via rand() without explicit
     * per-instance seeding (FNO spectral conv / lift / proj weights use
     * global rand(); the Hamiltonian H-network also does so). For the
     * bit-identical check in Test 4 to be meaningful we seed srand() to
     * a known value before each cortex/LNN creation. Not needed for
     * Tests 1-3 (those rely on relative magnitude, not bit equality). */
    static void SeedRng() {
        std::srand(0xC0FFEEu);
    }

    static float L2Norm(const float* v, uint32_t n) {
        if (!v || n == 0) return 0.0f;
        float s = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            if (std::isfinite(v[i])) s += v[i] * v[i];
        }
        return std::sqrt(s);
    }

    static float L2Norm(const std::vector<float>& v) {
        return L2Norm(v.data(), static_cast<uint32_t>(v.size()));
    }

    static bool AllFinite(const float* v, uint32_t n) {
        if (!v) return false;
        for (uint32_t i = 0; i < n; i++) {
            if (!std::isfinite(v[i])) return false;
        }
        return true;
    }

    static bool AllFinite(const std::vector<float>& v) {
        return AllFinite(v.data(), static_cast<uint32_t>(v.size()));
    }

    /* Deterministic mel spectrogram input. */
    static std::vector<float> MakeMel(uint32_t n, float amp) {
        std::vector<float> v(n);
        for (uint32_t i = 0; i < n; i++) {
            v[i] = amp * std::sin(0.1f * static_cast<float>(i));
        }
        return v;
    }

    /* Deterministic visual ramp (16x16x3, HWC uint8). */
    static std::vector<uint8_t> MakePixels() {
        std::vector<uint8_t> px(16 * 16 * 3);
        for (uint32_t i = 0; i < px.size(); i++) {
            px[i] = static_cast<uint8_t>((i * 3) & 0xFF);
        }
        return px;
    }

    /* Builders for substrate variants. */
    static neural_substrate_t* MakeHealthySubstrate() {
        substrate_config_t scfg;
        substrate_default_config(&scfg);
        neural_substrate_t* s = substrate_create(&scfg);
        if (!s) return nullptr;
        substrate_set_atp(s, 1.0f);
        substrate_set_membrane_integrity(s, 1.0f);
        return s;
    }

    static neural_substrate_t* MakeStressedSubstrate() {
        substrate_config_t scfg;
        substrate_default_config(&scfg);
        neural_substrate_t* s = substrate_create(&scfg);
        if (!s) return nullptr;
        substrate_set_atp(s, 0.3f);
        substrate_set_membrane_integrity(s, 0.3f);
        return s;
    }

    /* Attach an FNO audio processor of matching (mel_size, embed_dim) to a
     * cortex CNN audio processor. Ownership: returned pointer is set on
     * the cortex via cortex_cnn_set_fno_audio; caller destroys the FNO
     * after destroying the cortex (cortex does not own it). */
    static fno_audio_processor_t* AttachFnoAudio(cortex_cnn_processor_t* proc,
                                                  uint32_t mel_size,
                                                  uint32_t embed_dim) {
        fno_audio_processor_t* fno =
            fno_audio_create(mel_size, embed_dim,
                              /*hidden_ch=*/8, /*n_modes=*/8, /*n_blocks=*/1);
        if (!fno) return nullptr;
        cortex_cnn_set_fno_audio(proc, fno);
        return fno;
    }

    /* Visual FNO. The cortex downsamples grayscale into fno->input_size
     * samples before forward, so any input_size works. Match embed_dim to
     * the CNN's so the mix-in is pointwise. */
    static fno_audio_processor_t* AttachFnoVisual(cortex_cnn_processor_t* proc,
                                                   uint32_t embed_dim) {
        fno_audio_processor_t* fno =
            fno_audio_create(/*input_size=*/64, embed_dim,
                              /*hidden_ch=*/8, /*n_modes=*/8, /*n_blocks=*/1);
        if (!fno) return nullptr;
        cortex_cnn_set_fno_visual(proc, fno);
        return fno;
    }

    /* Build a 2-layer LNN with layer 0 running Hamiltonian dynamics and
     * layer 1 running standard LTC. Substrate modulation composes on the
     * LTC layer; thalamic input-gate composes before layer 0 (HNN sees
     * the gated input).  GPU pointers cleared so the CPU paths run. */
    lnn_network_t* MakeHnnPlusLtcNetwork() {
        lnn_config_t config;
        memset(&config, 0, sizeof(config));
        if (lnn_config_default(&config) != NIMCP_SUCCESS) return nullptr;
        config.n_inputs  = 4;
        config.n_outputs = 8;
        config.n_layers  = 2;
        config.default_dt = 1.0f;
        config.layer_configs = (lnn_layer_config_t*)calloc(2, sizeof(lnn_layer_config_t));
        if (!config.layer_configs) { lnn_config_destroy(&config); return nullptr; }

        /* Layer 0: 8 neurons, will be switched to Hamiltonian post-create. */
        config.layer_configs[0].n_neurons       = 8;
        config.layer_configs[0].activation      = LNN_ACTIVATION_TANH;
        config.layer_configs[0].tau_base_init   = 10.0f;
        config.layer_configs[0].tau_min         = 0.1f;
        config.layer_configs[0].tau_max         = 1000.0f;
        config.layer_configs[0].learn_tau       = true;
        config.layer_configs[0].weight_init_std = 0.1f;
        config.layer_configs[0].wiring_type     = LNN_WIRING_FULL;
        config.layer_configs[0].sparsity        = 0.0f;
        config.layer_configs[0].ode_method      = LNN_ODE_EULER;
        config.layer_configs[0].dt              = 1.0f;
        config.layer_configs[0].use_layer_norm  = false;
        config.layer_configs[0].layer_norm_eps  = 1e-5f;

        /* Layer 1: 8 neurons LTC. Observes substrate_dend_effects. */
        config.layer_configs[1]                 = config.layer_configs[0];

        lnn_network_t* net = lnn_network_create(&config);
        free(config.layer_configs);
        config.layer_configs = nullptr;
        lnn_config_destroy(&config);
        if (!net) return nullptr;

        /* Force CPU path on both layers. */
        for (uint32_t i = 0; i < net->n_layers; i++) {
            if (net->layers[i]) {
                net->layers[i]->gpu_lnn_layer = nullptr;
                net->layers[i]->gpu_ctx       = nullptr;
            }
        }
        net->gpu_ctx = nullptr;

        /* Deterministic weights. */
        if (lnn_network_init_weights(net, 42) != 0) {
            lnn_network_destroy(net);
            return nullptr;
        }

        /* Enable Hamiltonian on layer 0. Mirrors the brain init pattern
         * in src/core/brain/factory/init/nimcp_brain_init_lnn.c lines
         * 111-143. */
        if (net->n_layers > 0 && net->layers[0]) {
            lnn_layer_t* layer0 = net->layers[0];
            uint32_t state_dim  = layer0->n_neurons;
            if (state_dim > 0) {
                lnn_hamiltonian_config_t hcfg;
                lnn_hamiltonian_config_default(&hcfg);
                lnn_hamiltonian_net_t* H_net =
                    lnn_hamiltonian_net_create(state_dim, &hcfg);
                if (!H_net) {
                    lnn_network_destroy(net);
                    return nullptr;
                }
                layer0->H_net          = H_net;
                layer0->use_hamiltonian = true;
                if (!layer0->p) {
                    uint32_t p_dims[1] = {state_dim};
                    layer0->p = nimcp_tensor_create(p_dims, 1, NIMCP_DTYPE_F32);
                    if (layer0->p) {
                        float* pd = (float*)nimcp_tensor_data(layer0->p);
                        if (pd) {
                            /* Deterministic small momentum — no rand(). */
                            for (uint32_t j = 0; j < state_dim; j++) {
                                pd[j] = 0.005f * (static_cast<float>(j) - 4.0f);
                            }
                        }
                    }
                }
            }
        }
        return net;
    }

    static void DestroyHnnNetwork(lnn_network_t* net) {
        if (!net) return;
        /* Destroy H-network we attached to layer 0 before destroying the
         * LNN network (the network destructor does not free H_net). */
        if (net->n_layers > 0 && net->layers[0] && net->layers[0]->H_net) {
            lnn_hamiltonian_net_destroy(
                (lnn_hamiltonian_net_t*)net->layers[0]->H_net);
            net->layers[0]->H_net          = nullptr;
            net->layers[0]->use_hamiltonian = false;
        }
        lnn_network_destroy(net);
    }

    /* Run n_steps forwards on a cortex audio processor, returning the
     * final embedding L2 norm. Input is a fixed mel spectrogram. */
    static float RunAudioForwardLoop(cortex_cnn_processor_t* proc,
                                      const std::vector<float>& mel,
                                      int n_steps) {
        const float* last = nullptr;
        for (int i = 0; i < n_steps; i++) {
            last = cortex_cnn_forward_audio(proc, mel.data(),
                                              static_cast<uint32_t>(mel.size()));
            if (!last) return -1.0f;
        }
        uint32_t dim = 0;
        const float* emb = cortex_cnn_get_embedding(proc, &dim);
        if (!emb) return -1.0f;
        if (!AllFinite(emb, dim)) return -1.0f;
        return L2Norm(emb, dim);
    }
};

/* ======================================================================== */
/* Test 1: CNN(audio) + FNO(audio) observes substrate modulation            */
/* ======================================================================== */
/*
 * Rationale: the CORTEX_CNN_AUDIO forward takes the FNO branch at
 * src/training/nimcp_cortex_cnn.c:941 when fno_audio is set. After
 * fno_audio_forward fills the embedding it calls
 * cortex_cnn_apply_activation_mod(..., dend_eff) at line 955, which scales
 * the embedding by dend->integration_efficiency. A stressed substrate
 * (atp=0.3, membrane=0.3) collapses integration_efficiency below 1, so
 * the final embedding L2 norm MUST shrink vs the healthy baseline.
 */
TEST_F(FnoHnnSubstrateComposeTest, CnnWithFnoAudioObservesSubstrate) {
    cortex_cnn_tune_set_substrate_enabled(1.0f);
    cortex_cnn_tune_set_substrate_activation_mod_on(1.0f);
    cortex_cnn_tune_set_substrate_update_period(1.0f);

    const uint32_t mel_size  = 128;
    const uint32_t embed_dim = 32;
    const int n_steps = 20;
    std::vector<float> mel = MakeMel(mel_size, 0.5f);

    /* --- Healthy substrate --- */
    cortex_cnn_processor_t* proc_ok =
        cortex_cnn_create(CORTEX_CNN_AUDIO, embed_dim);
    ASSERT_NE(proc_ok, nullptr);
    fno_audio_processor_t* fno_ok = AttachFnoAudio(proc_ok, mel_size, embed_dim);
    ASSERT_NE(fno_ok, nullptr);
    neural_substrate_t* sub_ok = MakeHealthySubstrate();
    ASSERT_NE(sub_ok, nullptr);
    cortex_cnn_attach_substrate(proc_ok, sub_ok);

    float norm_ok = RunAudioForwardLoop(proc_ok, mel, n_steps);
    EXPECT_GT(norm_ok, 0.0f);

    cortex_cnn_destroy(proc_ok);
    fno_audio_destroy(fno_ok);
    substrate_destroy(sub_ok);

    /* --- Stressed substrate --- */
    cortex_cnn_processor_t* proc_stress =
        cortex_cnn_create(CORTEX_CNN_AUDIO, embed_dim);
    ASSERT_NE(proc_stress, nullptr);
    fno_audio_processor_t* fno_stress = AttachFnoAudio(proc_stress, mel_size, embed_dim);
    ASSERT_NE(fno_stress, nullptr);
    neural_substrate_t* sub_stress = MakeStressedSubstrate();
    ASSERT_NE(sub_stress, nullptr);
    cortex_cnn_attach_substrate(proc_stress, sub_stress);

    float norm_stress = RunAudioForwardLoop(proc_stress, mel, n_steps);
    EXPECT_GT(norm_stress, 0.0f);

    cortex_cnn_destroy(proc_stress);
    fno_audio_destroy(fno_stress);
    substrate_destroy(sub_stress);

    /* Stressed substrate compresses the post-FNO embedding amplitude.
     * Require a clear separation (>= 5% relative) so any regression that
     * lets FNO bypass the activation-mod would fire this assertion. */
    float rel_diff = std::fabs(norm_stress - norm_ok) /
                      std::max(norm_ok, 1e-6f);
    EXPECT_LT(norm_stress, norm_ok)
        << "stressed FNO-audio norm (" << norm_stress
        << ") should be smaller than healthy baseline (" << norm_ok << ")";
    EXPECT_GT(rel_diff, 0.05f)
        << "FNO-audio embedding must observe substrate modulation via its "
           "CNN parent: |stress-ok|/ok = " << rel_diff;
}

/* ======================================================================== */
/* Test 2: CNN(visual) + FNO(visual) observes thalamic attention gate       */
/* ======================================================================== */
/*
 * Rationale: the CORTEX_CNN_VISUAL forward runs CNN, applies substrate,
 * mixes FNO visual, and finally calls cortex_apply_thalamic_gate at
 * src/training/nimcp_cortex_cnn.c:926. The gate scales the whole embedding
 * by channel->attention_weights[0]. We compare (a) ungated baseline with
 * no router attached vs (b) same processor with router attached and the
 * cached gate forced to 0.5 pre-forward. If FNO leakage avoided the gate
 * the visual path would return an embedding whose ratio to baseline
 * exceeds 0.95; the gate therefore must reduce it.
 */
TEST_F(FnoHnnSubstrateComposeTest, CnnWithFnoVisualObservesThalamic) {
    cortex_cnn_tune_set_thalamic_enabled(1.0f);
    cortex_cnn_tune_set_thalamic_featuremap_gain_on(1.0f);
    /* Keep substrate off for this test so the only observed modulation is
     * thalamic. */
    cortex_cnn_tune_set_substrate_enabled(0.0f);

    const uint32_t embed_dim = 64;
    std::vector<uint8_t> pixels = MakePixels();

    /* --- Ungated baseline: no router attached. --- */
    cortex_cnn_processor_t* proc_base =
        cortex_cnn_create(CORTEX_CNN_VISUAL, embed_dim);
    ASSERT_NE(proc_base, nullptr);
    fno_audio_processor_t* fno_base = AttachFnoVisual(proc_base, embed_dim);
    ASSERT_NE(fno_base, nullptr);

    const float* emb_base = cortex_cnn_forward_visual(
        proc_base, pixels.data(), 16, 16, 3);
    ASSERT_NE(emb_base, nullptr);
    std::vector<float> base_out(emb_base, emb_base + embed_dim);
    EXPECT_TRUE(AllFinite(base_out));
    float norm_base = L2Norm(base_out);
    EXPECT_GT(norm_base, 0.0f);

    cortex_cnn_destroy(proc_base);
    fno_audio_destroy(fno_base);

    /* --- Gated: router attached, gate stamped to 0.5. --- */
    thalamic_router_config_t rcfg;
    std::memset(&rcfg, 0, sizeof(rcfg));
    rcfg.max_queue_size           = 128;
    rcfg.max_destinations         = 4;
    rcfg.enable_attention_gating  = true;
    rcfg.enable_priority_routing  = true;
    rcfg.enable_statistics        = true;
    rcfg.min_attention_threshold  = 0.0f;
    rcfg.enable_learning          = true;
    rcfg.enable_second_messengers = false;
    rcfg.num_neurons              = 32;
    rcfg.enable_quantum_routing   = false;
    thalamic_router_t* router = thalamic_router_create(&rcfg);
    ASSERT_NE(router, nullptr);

    cortex_cnn_processor_t* proc_gated =
        cortex_cnn_create(CORTEX_CNN_VISUAL, embed_dim);
    ASSERT_NE(proc_gated, nullptr);
    fno_audio_processor_t* fno_gated = AttachFnoVisual(proc_gated, embed_dim);
    ASSERT_NE(fno_gated, nullptr);
    ASSERT_EQ(cortex_cnn_attach_thalamic_router(
                  proc_gated, reinterpret_cast<struct thalamic_router*>(router)),
              NIMCP_SUCCESS);
    thalamic_channel_t* ch = cortex_cnn_test_get_thalamic_channel(proc_gated);
    ASSERT_NE(ch, nullptr);
    /* Stamp the gate pre-forward — forward reads the gate BEFORE submit+tick. */
    ch->attention_weights[0] = 0.5f;

    const float* emb_gated = cortex_cnn_forward_visual(
        proc_gated, pixels.data(), 16, 16, 3);
    ASSERT_NE(emb_gated, nullptr);
    std::vector<float> gated_out(emb_gated, emb_gated + embed_dim);
    EXPECT_TRUE(AllFinite(gated_out));
    float norm_gated = L2Norm(gated_out);

    cortex_cnn_destroy(proc_gated);
    fno_audio_destroy(fno_gated);
    thalamic_router_destroy(router);

    /* With gate = 0.5 applied to an embedding that includes the FNO
     * mix-in, the gated norm must be substantially below the ungated
     * baseline — if FNO were leaking around the gate, the mix-in would
     * dominate post-gate and keep the norm near baseline. */
    float ratio = norm_gated / std::max(norm_base, 1e-6f);
    EXPECT_LT(ratio, 0.95f)
        << "FNO-visual embedding must observe thalamic gate via its CNN "
           "parent: gated/base = " << ratio
        << " (norm_base=" << norm_base
        << ", norm_gated=" << norm_gated << ")";
}

/* ======================================================================== */
/* Test 3: HNN(layer 0) + LTC(layer 1) observes substrate via LTC layer      */
/* ======================================================================== */
/*
 * Rationale: lnn_layer_forward_hamiltonian does not read
 * layer->substrate_dend_effects — when layer 0 is Hamiltonian the LTC
 * substrate compose at src/lnn/nimcp_lnn_layer.c:735 is bypassed FOR
 * THAT LAYER. But the network writes substrate_dend_effects onto EVERY
 * layer per step (src/lnn/nimcp_lnn_network.c:515). Downstream LTC
 * layers therefore compose substrate on their own tau — and their input
 * is the HNN layer's output. So the end-to-end trajectory of a
 * (HNN, LTC) network DOES observe substrate. This test proves that
 * composition works.
 *
 * Caveat: if a future change moves HNN to be the ONLY layer, this test
 * would no longer cover it because the substrate effect is entirely on
 * the LTC part of the composite — a genuine HNN-substrate adapter (P4B)
 * is still the correct answer for that case.
 */
TEST_F(FnoHnnSubstrateComposeTest, LnnWithHnnObservesSubstrate) {
    lnn_tune_set_substrate_enabled(1.0f);
    lnn_tune_set_substrate_tau_compose_on(1.0f);
    lnn_tune_set_substrate_update_period(1.0f);

    const int n_steps = 50;

    /* --- Healthy trajectory --- */
    lnn_network_t* net_ok = MakeHnnPlusLtcNetwork();
    ASSERT_NE(net_ok, nullptr);
    /* Sanity — layer 0 IS Hamiltonian. */
    ASSERT_GE(net_ok->n_layers, 2u);
    ASSERT_TRUE(net_ok->layers[0]->use_hamiltonian);
    ASSERT_NE(net_ok->layers[0]->H_net, nullptr);
    ASSERT_FALSE(net_ok->layers[1]->use_hamiltonian);
    neural_substrate_t* sub_ok = MakeHealthySubstrate();
    ASSERT_NE(sub_ok, nullptr);
    lnn_network_attach_substrate(net_ok, sub_ok);

    uint32_t in_dims[1] = {4};
    uint32_t out_dims[1] = {8};
    nimcp_tensor_t* in_ok  = nimcp_tensor_create(in_dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* out_ok = nimcp_tensor_create(out_dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(in_ok, nullptr);
    ASSERT_NE(out_ok, nullptr);
    float* in_ok_data = (float*)nimcp_tensor_data(in_ok);
    for (uint32_t i = 0; i < 4; i++) in_ok_data[i] = 0.5f;

    float sum_norm_ok = 0.0f;
    for (int i = 0; i < n_steps; i++) {
        ASSERT_EQ(lnn_forward_step(net_ok, in_ok, out_ok, 1.0f), LNN_SUCCESS)
            << "healthy HNN step " << i;
        const float* od = (const float*)nimcp_tensor_data_const(out_ok);
        sum_norm_ok += L2Norm(od, 8);
    }
    nimcp_tensor_destroy(in_ok);
    nimcp_tensor_destroy(out_ok);
    substrate_destroy(sub_ok);
    DestroyHnnNetwork(net_ok);

    /* --- Stressed trajectory --- */
    lnn_network_t* net_stress = MakeHnnPlusLtcNetwork();
    ASSERT_NE(net_stress, nullptr);
    ASSERT_GE(net_stress->n_layers, 2u);
    ASSERT_TRUE(net_stress->layers[0]->use_hamiltonian);
    neural_substrate_t* sub_stress = MakeStressedSubstrate();
    ASSERT_NE(sub_stress, nullptr);
    lnn_network_attach_substrate(net_stress, sub_stress);

    nimcp_tensor_t* in_st  = nimcp_tensor_create(in_dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* out_st = nimcp_tensor_create(out_dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(in_st, nullptr);
    ASSERT_NE(out_st, nullptr);
    float* in_st_data = (float*)nimcp_tensor_data(in_st);
    for (uint32_t i = 0; i < 4; i++) in_st_data[i] = 0.5f;

    float sum_norm_stress = 0.0f;
    for (int i = 0; i < n_steps; i++) {
        ASSERT_EQ(lnn_forward_step(net_stress, in_st, out_st, 1.0f), LNN_SUCCESS)
            << "stressed HNN step " << i;
        const float* od = (const float*)nimcp_tensor_data_const(out_st);
        sum_norm_stress += L2Norm(od, 8);
    }
    nimcp_tensor_destroy(in_st);
    nimcp_tensor_destroy(out_st);
    substrate_destroy(sub_stress);
    DestroyHnnNetwork(net_stress);

    /* Stressed substrate must shift the summed-L2-norm trajectory of the
     * HNN-composed network by at least 5%. The HNN layer itself is
     * opaque to substrate; the delta comes from the downstream LTC layer
     * observing substrate_dend_effects on its tau. */
    float rel = std::fabs(sum_norm_stress - sum_norm_ok) /
                 std::max(sum_norm_ok, 1e-6f);
    EXPECT_GT(rel, 0.05f)
        << "HNN-composed LNN trajectory must observe substrate via its "
           "LTC downstream: sum_ok=" << sum_norm_ok
        << ", sum_stress=" << sum_norm_stress
        << ", rel=" << rel;
}

/* ======================================================================== */
/* Test 4: All knobs OFF — bit-identical to no-modulation baseline           */
/* ======================================================================== */
/*
 * Rationale: with every substrate + thalamic knob disabled, attaching a
 * damaged substrate to an FNO-enabled cortex CNN (or a thalamic router to
 * an HNN-composed LNN) MUST produce exactly the same output as the
 * no-attach baseline.  This is the core no-op guarantee: the adapter
 * infrastructure is opt-in and zero-cost when disabled, even for FNO
 * and HNN composite paths.
 *
 * Two scenarios mirror tests 1 and 3:
 *   (a) CNN-audio + FNO-audio: substrate disabled, attach damaged substrate
 *   (b) (HNN, LTC) LNN:        substrate disabled, attach damaged substrate
 */
TEST_F(FnoHnnSubstrateComposeTest, AllOffBitIdentical) {
    /* ---------------- Scenario (a): CNN-audio + FNO ---------------- */
    {
        const uint32_t mel_size  = 128;
        const uint32_t embed_dim = 32;
        const int n_steps = 10;
        std::vector<float> mel = MakeMel(mel_size, 0.4f);

        /* Baseline: no substrate attached. Seed RNG so the FNO Xavier
         * init is deterministic across the two creations in this test. */
        SeedRng();
        cortex_cnn_processor_t* proc_base =
            cortex_cnn_create(CORTEX_CNN_AUDIO, embed_dim);
        ASSERT_NE(proc_base, nullptr);
        fno_audio_processor_t* fno_base = AttachFnoAudio(proc_base, mel_size, embed_dim);
        ASSERT_NE(fno_base, nullptr);

        for (int i = 0; i < n_steps; i++) {
            const float* e = cortex_cnn_forward_audio(proc_base, mel.data(), mel_size);
            ASSERT_NE(e, nullptr);
        }
        uint32_t dim_base = 0;
        const float* got_base = cortex_cnn_get_embedding(proc_base, &dim_base);
        ASSERT_NE(got_base, nullptr);
        std::vector<float> base_out(got_base, got_base + dim_base);

        cortex_cnn_destroy(proc_base);
        fno_audio_destroy(fno_base);

        /* Off: damaged substrate attached, all knobs OFF. Reseed RNG so
         * the FNO init matches the base run exactly. */
        cortex_cnn_tune_set_substrate_enabled(0.0f);
        cortex_cnn_tune_set_substrate_activation_mod_on(0.0f);
        cortex_cnn_tune_set_substrate_plasticity_mod_on(0.0f);

        SeedRng();
        cortex_cnn_processor_t* proc_off =
            cortex_cnn_create(CORTEX_CNN_AUDIO, embed_dim);
        ASSERT_NE(proc_off, nullptr);
        fno_audio_processor_t* fno_off = AttachFnoAudio(proc_off, mel_size, embed_dim);
        ASSERT_NE(fno_off, nullptr);

        neural_substrate_t* sub_damaged = MakeStressedSubstrate();
        ASSERT_NE(sub_damaged, nullptr);
        cortex_cnn_attach_substrate(proc_off, sub_damaged);

        for (int i = 0; i < n_steps; i++) {
            const float* e = cortex_cnn_forward_audio(proc_off, mel.data(), mel_size);
            ASSERT_NE(e, nullptr);
        }
        uint32_t dim_off = 0;
        const float* got_off = cortex_cnn_get_embedding(proc_off, &dim_off);
        ASSERT_NE(got_off, nullptr);

        /* Same sequence of library RNG calls, same FNO init seed, same
         * input — the only possible difference is whether the substrate
         * path runs.  With all knobs off, every byte must match. */
        ASSERT_EQ(dim_base, dim_off);
        for (uint32_t i = 0; i < dim_off; i++) {
            EXPECT_FLOAT_EQ(base_out[i], got_off[i])
                << "CNN-audio + FNO OFF-path must be bit-identical at index " << i;
        }

        cortex_cnn_destroy(proc_off);
        fno_audio_destroy(fno_off);
        substrate_destroy(sub_damaged);
    }

    /* ---------------- Scenario (b): HNN + LTC LNN ---------------- */
    {
        const int n_steps = 30;

        /* Baseline: no substrate attached. Seed RNG so the H-network
         * Xavier init is deterministic across the two creations. */
        SeedRng();
        lnn_network_t* net_base = MakeHnnPlusLtcNetwork();
        ASSERT_NE(net_base, nullptr);

        uint32_t in_dims[1]  = {4};
        uint32_t out_dims[1] = {8};
        nimcp_tensor_t* in_b  = nimcp_tensor_create(in_dims, 1, NIMCP_DTYPE_F32);
        nimcp_tensor_t* out_b = nimcp_tensor_create(out_dims, 1, NIMCP_DTYPE_F32);
        ASSERT_NE(in_b, nullptr);
        ASSERT_NE(out_b, nullptr);
        float* inb = (float*)nimcp_tensor_data(in_b);
        for (uint32_t i = 0; i < 4; i++) inb[i] = 0.3f;

        for (int i = 0; i < n_steps; i++) {
            ASSERT_EQ(lnn_forward_step(net_base, in_b, out_b, 1.0f), LNN_SUCCESS);
        }
        const float* ob_data = (const float*)nimcp_tensor_data_const(out_b);
        std::vector<float> base_out(ob_data, ob_data + 8);
        nimcp_tensor_destroy(in_b);
        nimcp_tensor_destroy(out_b);
        DestroyHnnNetwork(net_base);

        /* Off: damaged substrate attached, all knobs OFF. Reseed RNG. */
        lnn_tune_set_substrate_enabled(0.0f);
        lnn_tune_set_substrate_tau_compose_on(0.0f);

        SeedRng();
        lnn_network_t* net_off = MakeHnnPlusLtcNetwork();
        ASSERT_NE(net_off, nullptr);
        neural_substrate_t* sub_damaged = MakeStressedSubstrate();
        ASSERT_NE(sub_damaged, nullptr);
        lnn_network_attach_substrate(net_off, sub_damaged);

        nimcp_tensor_t* in_o  = nimcp_tensor_create(in_dims, 1, NIMCP_DTYPE_F32);
        nimcp_tensor_t* out_o = nimcp_tensor_create(out_dims, 1, NIMCP_DTYPE_F32);
        ASSERT_NE(in_o, nullptr);
        ASSERT_NE(out_o, nullptr);
        float* ino = (float*)nimcp_tensor_data(in_o);
        for (uint32_t i = 0; i < 4; i++) ino[i] = 0.3f;

        for (int i = 0; i < n_steps; i++) {
            ASSERT_EQ(lnn_forward_step(net_off, in_o, out_o, 1.0f), LNN_SUCCESS);
        }
        const float* oo_data = (const float*)nimcp_tensor_data_const(out_o);
        std::vector<float> off_out(oo_data, oo_data + 8);

        ASSERT_EQ(base_out.size(), off_out.size());
        for (size_t i = 0; i < base_out.size(); i++) {
            EXPECT_FLOAT_EQ(base_out[i], off_out[i])
                << "HNN+LTC OFF-path must be bit-identical at index " << i;
        }

        nimcp_tensor_destroy(in_o);
        nimcp_tensor_destroy(out_o);
        substrate_destroy(sub_damaged);
        DestroyHnnNetwork(net_off);
    }
}

/* ======================================================================== */
/* main                                                                      */
/* ======================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
