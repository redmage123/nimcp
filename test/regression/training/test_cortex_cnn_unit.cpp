/**
 * @file test_cortex_cnn_unit.cpp
 * @brief Unit tests for per-cortex CNN processors
 *
 * Tests factory creation, forward pass shapes, backward gradient validity,
 * and attention fusion for all 4 modality types.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

extern "C" {
#include "training/nimcp_cortex_cnn.h"
#include "training/nimcp_unified_training.h"
}

class CortexCNNUnitTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (auto* p : procs_) {
            cortex_cnn_destroy(p);
        }
        procs_.clear();
    }

    cortex_cnn_processor_t* create(cortex_cnn_type_t type, uint32_t dim = 0) {
        auto* p = cortex_cnn_create(type, dim);
        if (p) procs_.push_back(p);
        return p;
    }

    std::vector<cortex_cnn_processor_t*> procs_;
};

// ============================================================
// Factory creation tests
// ============================================================

TEST_F(CortexCNNUnitTest, CreateVisual) {
    auto* proc = create(CORTEX_CNN_VISUAL);
    ASSERT_NE(proc, nullptr);
    EXPECT_EQ(cortex_cnn_get_type(proc), CORTEX_CNN_VISUAL);
}

TEST_F(CortexCNNUnitTest, CreateAudio) {
    auto* proc = create(CORTEX_CNN_AUDIO);
    ASSERT_NE(proc, nullptr);
    EXPECT_EQ(cortex_cnn_get_type(proc), CORTEX_CNN_AUDIO);
}

TEST_F(CortexCNNUnitTest, CreateSpeech) {
    auto* proc = create(CORTEX_CNN_SPEECH);
    ASSERT_NE(proc, nullptr);
    EXPECT_EQ(cortex_cnn_get_type(proc), CORTEX_CNN_SPEECH);
}

TEST_F(CortexCNNUnitTest, CreateSomato) {
    auto* proc = create(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);
    EXPECT_EQ(cortex_cnn_get_type(proc), CORTEX_CNN_SOMATO);
}

TEST_F(CortexCNNUnitTest, CreateWithCustomEmbedDim) {
    auto* proc = create(CORTEX_CNN_SOMATO, 16);
    ASSERT_NE(proc, nullptr);

    uint32_t dim = 0;
    const float* emb = cortex_cnn_get_embedding(proc, &dim);
    /* No forward yet, so embedding is NULL */
    EXPECT_EQ(emb, nullptr);
}

TEST_F(CortexCNNUnitTest, TypeNames) {
    EXPECT_STREQ(cortex_cnn_type_name(CORTEX_CNN_VISUAL), "Visual");
    EXPECT_STREQ(cortex_cnn_type_name(CORTEX_CNN_AUDIO), "Audio");
    EXPECT_STREQ(cortex_cnn_type_name(CORTEX_CNN_SPEECH), "Speech");
    EXPECT_STREQ(cortex_cnn_type_name(CORTEX_CNN_SOMATO), "Somato");
}

TEST_F(CortexCNNUnitTest, DestroyNull) {
    cortex_cnn_destroy(nullptr);  /* Should not crash */
}

// ============================================================
// Forward pass tests
// ============================================================

TEST_F(CortexCNNUnitTest, ForwardVisual_ProducesEmbedding) {
    auto* proc = create(CORTEX_CNN_VISUAL);
    ASSERT_NE(proc, nullptr);

    std::vector<uint8_t> pixels(64 * 64 * 3, 128);
    const float* emb = cortex_cnn_forward_visual(proc, pixels.data(), 64, 64, 3);
    ASSERT_NE(emb, nullptr);

    uint32_t dim = 0;
    const float* emb2 = cortex_cnn_get_embedding(proc, &dim);
    EXPECT_EQ(dim, 64u);
    EXPECT_EQ(emb, emb2);
}

TEST_F(CortexCNNUnitTest, ForwardAudio_ProducesEmbedding) {
    auto* proc = create(CORTEX_CNN_AUDIO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> mel(128, 0.5f);
    const float* emb = cortex_cnn_forward_audio(proc, mel.data(), 128);
    ASSERT_NE(emb, nullptr);

    uint32_t dim = 0;
    cortex_cnn_get_embedding(proc, &dim);
    EXPECT_EQ(dim, 64u);
}

TEST_F(CortexCNNUnitTest, ForwardSpeech_ProducesEmbedding) {
    auto* proc = create(CORTEX_CNN_SPEECH);
    ASSERT_NE(proc, nullptr);

    std::vector<float> phonemes(64, 0.3f);
    const float* emb = cortex_cnn_forward_speech(proc, phonemes.data(), 64);
    ASSERT_NE(emb, nullptr);

    uint32_t dim = 0;
    cortex_cnn_get_embedding(proc, &dim);
    EXPECT_EQ(dim, 32u);
}

TEST_F(CortexCNNUnitTest, ForwardSomato_ProducesEmbedding) {
    auto* proc = create(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> segments(45, 0.1f);
    const float* emb = cortex_cnn_forward_somato(proc, segments.data(), 45);
    ASSERT_NE(emb, nullptr);

    uint32_t dim = 0;
    cortex_cnn_get_embedding(proc, &dim);
    EXPECT_EQ(dim, 32u);
}

TEST_F(CortexCNNUnitTest, ForwardOutputFiniteNonZero) {
    auto* proc = create(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> segments(45);
    for (int i = 0; i < 45; i++) segments[i] = (float)(i + 1) / 45.0f;
    const float* emb = cortex_cnn_forward_somato(proc, segments.data(), 45);
    ASSERT_NE(emb, nullptr);

    uint32_t dim = 0;
    cortex_cnn_get_embedding(proc, &dim);
    float norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        EXPECT_TRUE(std::isfinite(emb[i])) << "emb[" << i << "] = " << emb[i];
        norm += emb[i] * emb[i];
    }
    /* Embedding should not be all-zero for non-zero input */
    EXPECT_GT(norm, 0.0f);
}

TEST_F(CortexCNNUnitTest, ForwardDifferentInputsDifferentOutputs) {
    auto* proc = create(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> seg1(45, 0.1f);
    std::vector<float> seg2(45, 0.9f);

    const float* emb1 = cortex_cnn_forward_somato(proc, seg1.data(), 45);
    ASSERT_NE(emb1, nullptr);
    uint32_t dim = 0;
    cortex_cnn_get_embedding(proc, &dim);
    std::vector<float> copy1(emb1, emb1 + dim);

    const float* emb2 = cortex_cnn_forward_somato(proc, seg2.data(), 45);
    ASSERT_NE(emb2, nullptr);

    /* Check outputs differ */
    float diff = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        diff += (emb2[i] - copy1[i]) * (emb2[i] - copy1[i]);
    }
    EXPECT_GT(diff, 1e-10f) << "Different inputs should produce different embeddings";
}

TEST_F(CortexCNNUnitTest, ForwardWrongModality_ReturnsNull) {
    auto* proc = create(CORTEX_CNN_VISUAL);
    ASSERT_NE(proc, nullptr);

    /* Try to use audio forward on visual processor */
    std::vector<float> mel(128, 0.5f);
    const float* emb = cortex_cnn_forward_audio(proc, mel.data(), 128);
    EXPECT_EQ(emb, nullptr);
}

// ============================================================
// Backward pass tests
// ============================================================

TEST_F(CortexCNNUnitTest, BackwardReturnsLoss) {
    auto* proc = create(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> segments(45, 0.5f);
    cortex_cnn_forward_somato(proc, segments.data(), 45);

    float loss = cortex_cnn_backward(proc, "test_label", 32);
    EXPECT_GE(loss, 0.0f);
    EXPECT_TRUE(std::isfinite(loss));
}

TEST_F(CortexCNNUnitTest, BackwardWithoutForward_ReturnsError) {
    auto* proc = create(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    float loss = cortex_cnn_backward(proc, "test_label", 32);
    EXPECT_LT(loss, 0.0f);
}

TEST_F(CortexCNNUnitTest, BackwardLossDecreases) {
    auto* proc = create(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> segments(45, 0.5f);
    float first_loss = 0.0f;
    float last_loss = 0.0f;

    for (int step = 0; step < 20; step++) {
        cortex_cnn_forward_somato(proc, segments.data(), 45);
        float loss = cortex_cnn_backward(proc, "cat", 32);
        if (step == 0) first_loss = loss;
        if (step == 19) last_loss = loss;
    }

    EXPECT_LT(last_loss, first_loss) << "Loss should decrease over training steps";
}

// ============================================================
// Metrics tests
// ============================================================

TEST_F(CortexCNNUnitTest, MetricsAfterTraining) {
    auto* proc = create(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> segments(45, 0.5f);
    cortex_cnn_forward_somato(proc, segments.data(), 45);
    cortex_cnn_backward(proc, "test", 32);

    cortex_cnn_metrics_t m = {};
    EXPECT_EQ(cortex_cnn_get_metrics(proc, &m), 0);
    EXPECT_EQ(m.type, CORTEX_CNN_SOMATO);
    EXPECT_EQ(m.forward_steps, 1u);
    EXPECT_EQ(m.backward_steps, 1u);
    EXPECT_GE(m.last_loss, 0.0f);
    EXPECT_GE(m.embedding_norm, 0.0f);
    EXPECT_EQ(m.embedding_dim, 32u);
    EXPECT_GT(m.num_params, 0u);
}

// ============================================================
// Fusion tests
// ============================================================

TEST_F(CortexCNNUnitTest, FuseSingleActive) {
    auto* proc = create(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> segments(45, 0.5f);
    cortex_cnn_forward_somato(proc, segments.data(), 45);

    cortex_cnn_processor_t* procs[4] = {nullptr, nullptr, nullptr, proc};
    std::vector<float> fused(192, 0.0f);
    uint32_t dim = cortex_cnn_fuse(procs, 4, fused.data(), 192);
    EXPECT_EQ(dim, 32u);

    /* Check fused output is non-zero */
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) sum += std::abs(fused[i]);
    EXPECT_GT(sum, 0.0f);
}

TEST_F(CortexCNNUnitTest, FuseMultipleActive) {
    auto* p_audio = create(CORTEX_CNN_AUDIO);
    auto* p_somato = create(CORTEX_CNN_SOMATO);
    ASSERT_NE(p_audio, nullptr);
    ASSERT_NE(p_somato, nullptr);

    std::vector<float> mel(128, 0.3f);
    std::vector<float> seg(45, 0.7f);
    cortex_cnn_forward_audio(p_audio, mel.data(), 128);
    cortex_cnn_forward_somato(p_somato, seg.data(), 45);

    cortex_cnn_processor_t* procs[4] = {nullptr, p_audio, nullptr, p_somato};
    std::vector<float> fused(192, 0.0f);
    uint32_t dim = cortex_cnn_fuse(procs, 4, fused.data(), 192);
    EXPECT_EQ(dim, 64u + 32u);  /* audio(64) + somato(32) */
}

TEST_F(CortexCNNUnitTest, FuseNoActive) {
    cortex_cnn_processor_t* procs[4] = {nullptr, nullptr, nullptr, nullptr};
    std::vector<float> fused(192, 0.0f);
    uint32_t dim = cortex_cnn_fuse(procs, 4, fused.data(), 192);
    EXPECT_EQ(dim, 0u);
}

// ============================================================
// UTM adapter tests
// ============================================================

TEST_F(CortexCNNUnitTest, UTMAdapterCreate) {
    auto* proc = create(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    const nimcp_trainable_network_ops_t* ops = nullptr;
    void* ctx = nullptr;
    int rc = cortex_cnn_utm_adapter_create(proc, &ops, &ctx);
    EXPECT_EQ(rc, 0);
    EXPECT_NE(ops, nullptr);
    EXPECT_NE(ctx, nullptr);
    EXPECT_STREQ(ops->name, "CortexCNN_Somato");

    /* Cleanup adapter */
    if (ops && ops->destroy) ops->destroy(ctx);
}
