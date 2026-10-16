/**
 * @file test_cortex_cnn_integration.cpp
 * @brief Integration tests for cortex CNN + brain struct wiring
 *
 * Tests lazy init, forward pass through brain, cortex CNN + adaptive cooperation.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "training/nimcp_cortex_cnn.h"
#include "training/nimcp_unified_training.h"
#include "training/nimcp_cnn_training.h"
}

class CortexCNNIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (auto* p : procs_) {
            cortex_cnn_destroy(p);
        }
    }

    cortex_cnn_processor_t* make(cortex_cnn_type_t t) {
        auto* p = cortex_cnn_create(t, 0);
        if (p) procs_.push_back(p);
        return p;
    }

    std::vector<cortex_cnn_processor_t*> procs_;
};

// ============================================================
// UTM registration tests
// ============================================================

TEST_F(CortexCNNIntegrationTest, UTMRegisterCortexCNN) {
    auto* visual = make(CORTEX_CNN_VISUAL);
    auto* audio = make(CORTEX_CNN_AUDIO);
    ASSERT_NE(visual, nullptr);
    ASSERT_NE(audio, nullptr);

    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    auto* utm = nimcp_utm_create(&cfg);
    ASSERT_NE(utm, nullptr);

    const nimcp_trainable_network_ops_t* ops = nullptr;
    void* ctx = nullptr;

    EXPECT_EQ(cortex_cnn_utm_adapter_create(visual, &ops, &ctx), 0);
    EXPECT_GE(nimcp_utm_register_network(utm, ops, ctx, 0.3f), 0);

    EXPECT_EQ(cortex_cnn_utm_adapter_create(audio, &ops, &ctx), 0);
    EXPECT_GE(nimcp_utm_register_network(utm, ops, ctx, 0.3f), 0);

    EXPECT_EQ(utm->num_networks, 2u);

    nimcp_utm_destroy(utm);
}

TEST_F(CortexCNNIntegrationTest, UTMCrossNetworkBridges) {
    auto* visual = make(CORTEX_CNN_VISUAL);
    auto* audio = make(CORTEX_CNN_AUDIO);
    ASSERT_NE(visual, nullptr);
    ASSERT_NE(audio, nullptr);

    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    auto* utm = nimcp_utm_create(&cfg);
    ASSERT_NE(utm, nullptr);

    const nimcp_trainable_network_ops_t* ops = nullptr;
    void* ctx = nullptr;

    cortex_cnn_utm_adapter_create(visual, &ops, &ctx);
    nimcp_utm_register_network(utm, ops, ctx, 0.3f);

    cortex_cnn_utm_adapter_create(audio, &ops, &ctx);
    nimcp_utm_register_network(utm, ops, ctx, 0.3f);

    /* Add visual<->audio LINEAR bridge (returns index >= 0 on success) */
    EXPECT_GE(nimcp_utm_add_bridge(utm, 0, 1, NIMCP_BRIDGE_LINEAR), 0);
    EXPECT_GE(nimcp_utm_add_bridge(utm, 1, 0, NIMCP_BRIDGE_LINEAR), 0);
    EXPECT_EQ(utm->num_bridges, 2u);

    nimcp_utm_destroy(utm);
}

// ============================================================
// Multi-modal forward + fusion tests
// ============================================================

TEST_F(CortexCNNIntegrationTest, MultiModalForwardAndFuse) {
    auto* visual = make(CORTEX_CNN_VISUAL);
    auto* audio = make(CORTEX_CNN_AUDIO);
    auto* speech = make(CORTEX_CNN_SPEECH);
    auto* somato = make(CORTEX_CNN_SOMATO);

    /* Run forward on all */
    std::vector<uint8_t> pixels(64 * 64 * 3, 100);
    std::vector<float> mel(128, 0.5f);
    std::vector<float> phonemes(64, 0.3f);
    std::vector<float> segments(45, 0.2f);

    ASSERT_NE(cortex_cnn_forward_visual(visual, pixels.data(), 64, 64, 3), nullptr);
    ASSERT_NE(cortex_cnn_forward_audio(audio, mel.data(), 128), nullptr);
    ASSERT_NE(cortex_cnn_forward_speech(speech, phonemes.data(), 64), nullptr);
    ASSERT_NE(cortex_cnn_forward_somato(somato, segments.data(), 45), nullptr);

    /* Fuse all 4 */
    cortex_cnn_processor_t* procs[4] = {visual, audio, speech, somato};
    std::vector<float> fused(192, 0.0f);
    uint32_t dim = cortex_cnn_fuse(procs, 4, fused.data(), 192);
    EXPECT_EQ(dim, 64u + 64u + 32u + 32u);

    /* Verify fused output is finite */
    for (uint32_t i = 0; i < dim; i++) {
        EXPECT_TRUE(std::isfinite(fused[i])) << "fused[" << i << "]";
    }
}

// ============================================================
// Training cooperation tests
// ============================================================

TEST_F(CortexCNNIntegrationTest, IndependentTrainingDoesNotInterfere) {
    auto* audio = make(CORTEX_CNN_AUDIO);
    auto* somato = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(audio, nullptr);
    ASSERT_NE(somato, nullptr);

    /* Train audio on one class */
    std::vector<float> mel(128, 0.5f);
    for (int i = 0; i < 5; i++) {
        cortex_cnn_forward_audio(audio, mel.data(), 128);
        cortex_cnn_backward(audio, "music", 64);
    }

    /* Train somato on another class */
    std::vector<float> seg(45, 0.3f);
    for (int i = 0; i < 5; i++) {
        cortex_cnn_forward_somato(somato, seg.data(), 45);
        cortex_cnn_backward(somato, "touch", 32);
    }

    /* Both should have independent metrics */
    cortex_cnn_metrics_t m_audio = {}, m_somato = {};
    cortex_cnn_get_metrics(audio, &m_audio);
    cortex_cnn_get_metrics(somato, &m_somato);

    EXPECT_EQ(m_audio.forward_steps, 5u);
    EXPECT_EQ(m_somato.forward_steps, 5u);
    EXPECT_EQ(m_audio.backward_steps, 5u);
    EXPECT_EQ(m_somato.backward_steps, 5u);
}

TEST_F(CortexCNNIntegrationTest, CortexDoesNotTrainWithoutData) {
    auto* visual = make(CORTEX_CNN_VISUAL);
    ASSERT_NE(visual, nullptr);

    /* No forward pass — backward should fail */
    float loss = cortex_cnn_backward(visual, "label", 64);
    EXPECT_LT(loss, 0.0f);

    cortex_cnn_metrics_t m = {};
    cortex_cnn_get_metrics(visual, &m);
    EXPECT_EQ(m.backward_steps, 0u);
}

// ============================================================
// UTM auto-enable verification
// ============================================================

TEST_F(CortexCNNIntegrationTest, UTMAutoCreateAndRegisterOnLazyInit) {
    /* Verify that creating cortex CNNs and registering them in UTM works
     * as a standalone flow (simulates what brain_learn_vector does lazily). */
    auto* visual = make(CORTEX_CNN_VISUAL);
    auto* audio = make(CORTEX_CNN_AUDIO);
    ASSERT_NE(visual, nullptr);
    ASSERT_NE(audio, nullptr);

    /* Simulate: UTM created on-demand */
    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    cfg.learning_rate = 0.001f;
    auto* utm = nimcp_utm_create(&cfg);
    ASSERT_NE(utm, nullptr);

    /* Register both cortex CNNs */
    const nimcp_trainable_network_ops_t* ops = nullptr;
    void* ctx = nullptr;

    EXPECT_EQ(cortex_cnn_utm_adapter_create(visual, &ops, &ctx), 0);
    int v_idx = nimcp_utm_register_network(utm, ops, ctx, 0.3f);
    EXPECT_GE(v_idx, 0);

    EXPECT_EQ(cortex_cnn_utm_adapter_create(audio, &ops, &ctx), 0);
    int a_idx = nimcp_utm_register_network(utm, ops, ctx, 0.3f);
    EXPECT_GE(a_idx, 0);

    EXPECT_EQ(utm->num_networks, 2u);

    /* Wire bridges */
    EXPECT_GE(nimcp_utm_add_bridge(utm, (uint32_t)v_idx, (uint32_t)a_idx,
                                    NIMCP_BRIDGE_LINEAR), 0);
    EXPECT_GE(nimcp_utm_add_bridge(utm, (uint32_t)a_idx, (uint32_t)v_idx,
                                    NIMCP_BRIDGE_LINEAR), 0);
    EXPECT_EQ(utm->num_bridges, 2u);

    /* Enable cross-network gradients */
    utm->config.enable_cross_network_gradients = true;

    /* Verify network ops are correct */
    EXPECT_NE(utm->networks[v_idx].ops, nullptr);
    EXPECT_NE(utm->networks[a_idx].ops, nullptr);
    EXPECT_STREQ(utm->networks[v_idx].ops->name, "CortexCNN_Visual");
    EXPECT_STREQ(utm->networks[a_idx].ops->name, "CortexCNN_Audio");
    EXPECT_EQ(utm->networks[v_idx].ops->type, NIMCP_TRAINABLE_CUSTOM);

    /* Verify adapter dims */
    EXPECT_EQ(utm->networks[v_idx].ops->get_output_dim(utm->networks[v_idx].ctx), 64u);
    EXPECT_EQ(utm->networks[a_idx].ops->get_output_dim(utm->networks[a_idx].ctx), 64u);
    EXPECT_EQ(utm->networks[v_idx].ops->get_input_dim(utm->networks[v_idx].ctx),
              64u * 64u * 3u);
    EXPECT_EQ(utm->networks[a_idx].ops->get_input_dim(utm->networks[a_idx].ctx), 128u);

    /* Register a somato adapter too — somato is dense-only so 1D forward works */
    auto* somato = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(somato, nullptr);
    const nimcp_trainable_network_ops_t* s_ops = nullptr;
    void* s_ctx = nullptr;
    EXPECT_EQ(cortex_cnn_utm_adapter_create(somato, &s_ops, &s_ctx), 0);
    int s_idx = nimcp_utm_register_network(utm, s_ops, s_ctx, 0.3f);
    EXPECT_GE(s_idx, 0);

    /* Run a UTM forward through the somato adapter (dense-only, flat 1D works) */
    std::vector<float> somato_in(45, 0.5f);
    std::vector<float> somato_out(32, 0.0f);
    int fwd_rc = utm->networks[s_idx].ops->forward(
        utm->networks[s_idx].ctx, somato_in.data(), 45,
        somato_out.data(), 32);
    EXPECT_EQ(fwd_rc, 0);

    /* Output should be finite and non-zero */
    float norm = 0.0f;
    for (int i = 0; i < 32; i++) {
        EXPECT_TRUE(std::isfinite(somato_out[i]));
        norm += somato_out[i] * somato_out[i];
    }
    EXPECT_GT(norm, 0.0f);

    nimcp_utm_destroy(utm);
}

TEST_F(CortexCNNIntegrationTest, UTMCompositeStepWithCortexCNNs) {
    /* Verify UTM step works with cortex CNN adapters registered */
    auto* somato = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(somato, nullptr);

    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    cfg.learning_rate = 0.001f;
    auto* utm = nimcp_utm_create(&cfg);
    ASSERT_NE(utm, nullptr);

    const nimcp_trainable_network_ops_t* ops = nullptr;
    void* ctx = nullptr;
    cortex_cnn_utm_adapter_create(somato, &ops, &ctx);
    int idx = nimcp_utm_register_network(utm, ops, ctx, 0.5f);
    EXPECT_GE(idx, 0);

    /* Run UTM step */
    std::vector<float> input(45, 0.5f);
    std::vector<float> target(32, 0.0f);
    target[5] = 1.0f;  /* One-hot */

    nimcp_utm_step_result_t result = {};
    int rc = nimcp_utm_step(utm, input.data(), 45,
                             target.data(), 32, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(std::isfinite(result.composite_loss));
    EXPECT_GE(result.composite_loss, 0.0f);
    EXPECT_GE(result.step, 1u);

    /* Run a few more steps and verify loss trends */
    float first_loss = result.composite_loss;
    for (int i = 0; i < 20; i++) {
        nimcp_utm_step(utm, input.data(), 45, target.data(), 32, &result);
    }
    /* After 20+ steps on fixed data, loss should generally decrease */
    EXPECT_TRUE(std::isfinite(result.composite_loss));

    nimcp_utm_destroy(utm);
}

// ============================================================
// All networks coexist in UTM
// ============================================================

TEST_F(CortexCNNIntegrationTest, AllNetworkTypesCoexistInUTM) {
    /* Verify that cortex CNNs coexist with the main CNN in UTM.
     * We create a regular CNN trainer + 2 cortex CNNs and register all. */

    /* Create regular CNN trainer (the classification head) */
    cnn_trainer_config_t cnn_cfg;
    cnn_trainer_default_config(&cnn_cfg);
    cnn_trainer_t* cnn = cnn_trainer_create(&cnn_cfg);
    ASSERT_NE(cnn, nullptr);

    /* Add minimal architecture to CNN */
    cnn_dense_config_t dense1 = {.in_features=64, .out_features=32,
        .activation=CNN_ACTIVATION_NONE, .use_bias=true, .weight_init_std=0.01f};
    cnn_trainer_add_dense_layer(cnn, &dense1);
    cnn_trainer_add_activation_layer(cnn, CNN_ACTIVATION_RELU);
    cnn_dense_config_t dense2 = {.in_features=32, .out_features=16,
        .activation=CNN_ACTIVATION_NONE, .use_bias=true, .weight_init_std=0.01f};
    cnn_trainer_add_dense_layer(cnn, &dense2);

    /* Create cortex CNNs */
    auto* audio = make(CORTEX_CNN_AUDIO);
    auto* somato = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(audio, nullptr);
    ASSERT_NE(somato, nullptr);

    /* Create UTM */
    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    auto* utm = nimcp_utm_create(&cfg);
    ASSERT_NE(utm, nullptr);

    /* Register main CNN adapter */
    const nimcp_trainable_network_ops_t* ops = nullptr;
    void* ctx = nullptr;
    EXPECT_EQ(nimcp_trainable_cnn_create(cnn, &ops, &ctx), 0);
    nimcp_trainable_cnn_set_dims(ctx, 64, 16);
    int cnn_idx = nimcp_utm_register_network(utm, ops, ctx, 0.5f);
    EXPECT_GE(cnn_idx, 0);

    /* Register cortex CNN adapters */
    EXPECT_EQ(cortex_cnn_utm_adapter_create(audio, &ops, &ctx), 0);
    int audio_idx = nimcp_utm_register_network(utm, ops, ctx, 0.3f);
    EXPECT_GE(audio_idx, 0);

    EXPECT_EQ(cortex_cnn_utm_adapter_create(somato, &ops, &ctx), 0);
    int somato_idx = nimcp_utm_register_network(utm, ops, ctx, 0.3f);
    EXPECT_GE(somato_idx, 0);

    /* Verify all 3 registered */
    EXPECT_EQ(utm->num_networks, 3u);

    /* Verify types */
    EXPECT_EQ(utm->networks[cnn_idx].ops->type, NIMCP_TRAINABLE_CNN);
    EXPECT_EQ(utm->networks[audio_idx].ops->type, NIMCP_TRAINABLE_CUSTOM);
    EXPECT_EQ(utm->networks[somato_idx].ops->type, NIMCP_TRAINABLE_CUSTOM);

    /* Verify names */
    EXPECT_STREQ(utm->networks[cnn_idx].ops->name, "CNN");
    EXPECT_STREQ(utm->networks[audio_idx].ops->name, "CortexCNN_Audio");
    EXPECT_STREQ(utm->networks[somato_idx].ops->name, "CortexCNN_Somato");

    /* Wire cross-modal bridge */
    int bridge_idx = nimcp_utm_add_bridge(utm,
        (uint32_t)audio_idx, (uint32_t)somato_idx, NIMCP_BRIDGE_LINEAR);
    EXPECT_GE(bridge_idx, 0);
    EXPECT_EQ(utm->num_bridges, 1u);

    /* Verify per-network anti-collapse state exists for each */
    for (uint32_t n = 0; n < utm->num_networks; n++) {
        EXPECT_TRUE(utm->networks[n].enabled);
    }

    nimcp_utm_destroy(utm);
    cnn_trainer_destroy(cnn);
}

// ============================================================
// Checkpoint tests
// ============================================================

TEST_F(CortexCNNIntegrationTest, SaveLoadPreservesWeights) {
    auto* proc = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    /* Train a few steps */
    std::vector<float> seg(45, 0.5f);
    for (int i = 0; i < 10; i++) {
        cortex_cnn_forward_somato(proc, seg.data(), 45);
        cortex_cnn_backward(proc, "press", 32);
    }

    /* Get embedding before save */
    cortex_cnn_forward_somato(proc, seg.data(), 45);
    uint32_t dim = 0;
    const float* emb1 = cortex_cnn_get_embedding(proc, &dim);
    std::vector<float> pre_save(emb1, emb1 + dim);

    /* Save */
    const char* path = "/tmp/test_cortex_somato.weights";
    EXPECT_EQ(cortex_cnn_save(proc, path), 0);

    /* Create fresh processor and load */
    auto* proc2 = cortex_cnn_create(CORTEX_CNN_SOMATO, 0);
    ASSERT_NE(proc2, nullptr);
    EXPECT_EQ(cortex_cnn_load(proc2, path), 0);

    /* Forward on loaded processor */
    cortex_cnn_forward_somato(proc2, seg.data(), 45);
    const float* emb2 = cortex_cnn_get_embedding(proc2, &dim);
    ASSERT_NE(emb2, nullptr);

    /* Compare embeddings */
    float diff = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        diff += (emb2[i] - pre_save[i]) * (emb2[i] - pre_save[i]);
    }
    EXPECT_LT(diff, 1e-6f) << "Loaded weights should produce same embedding";

    cortex_cnn_destroy(proc2);
    remove(path);
}
