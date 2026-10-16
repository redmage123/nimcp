/**
 * @file test_cortex_cnn_regression.cpp
 * @brief Regression tests for cortex CNN stability, anti-collapse, checkpoint
 *
 * Guards against gradient NaN/Inf, mode collapse, checkpoint corruption,
 * and edge-case inputs.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

extern "C" {
#include "training/nimcp_cortex_cnn.h"
}

class CortexCNNRegressionTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (auto* p : procs_) cortex_cnn_destroy(p);
    }

    cortex_cnn_processor_t* make(cortex_cnn_type_t t) {
        auto* p = cortex_cnn_create(t, 0);
        if (p) procs_.push_back(p);
        return p;
    }

    std::vector<cortex_cnn_processor_t*> procs_;
};

// ============================================================
// Gradient stability
// ============================================================

TEST_F(CortexCNNRegressionTest, GradientStability100Steps) {
    auto* proc = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> seg(45, 0.5f);
    for (int step = 0; step < 100; step++) {
        /* Vary input slightly each step */
        seg[step % 45] = 0.1f + 0.8f * ((float)step / 100.0f);

        const float* emb = cortex_cnn_forward_somato(proc, seg.data(), 45);
        ASSERT_NE(emb, nullptr) << "Forward failed at step " << step;

        uint32_t dim = 0;
        cortex_cnn_get_embedding(proc, &dim);
        for (uint32_t i = 0; i < dim; i++) {
            ASSERT_TRUE(std::isfinite(emb[i]))
                << "NaN/Inf in embedding[" << i << "] at step " << step;
        }

        float loss = cortex_cnn_backward(proc, "test", 32);
        ASSERT_TRUE(std::isfinite(loss)) << "NaN/Inf loss at step " << step;
    }
}

TEST_F(CortexCNNRegressionTest, AudioGradientStability100Steps) {
    auto* proc = make(CORTEX_CNN_AUDIO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> mel(128, 0.0f);
    for (int step = 0; step < 100; step++) {
        for (int i = 0; i < 128; i++) {
            mel[i] = sinf((float)(i + step) * 0.1f) * 0.5f + 0.5f;
        }

        const float* emb = cortex_cnn_forward_audio(proc, mel.data(), 128);
        ASSERT_NE(emb, nullptr) << "Forward failed at step " << step;

        float loss = cortex_cnn_backward(proc, "tone", 64);
        ASSERT_TRUE(std::isfinite(loss)) << "NaN/Inf loss at step " << step;
    }
}

// ============================================================
// Anti-collapse (diversity maintained)
// ============================================================

TEST_F(CortexCNNRegressionTest, OutputDiversityMaintained) {
    auto* proc = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    /* Train for 50 steps */
    std::vector<float> seg(45, 0.5f);
    for (int step = 0; step < 50; step++) {
        cortex_cnn_forward_somato(proc, seg.data(), 45);
        cortex_cnn_backward(proc, "press", 32);
    }

    /* Get embeddings for different inputs and check cosine similarity < 0.99 */
    std::vector<float> seg_a(45, 0.1f);
    std::vector<float> seg_b(45, 0.9f);

    cortex_cnn_forward_somato(proc, seg_a.data(), 45);
    uint32_t dim = 0;
    const float* emb_a = cortex_cnn_get_embedding(proc, &dim);
    std::vector<float> copy_a(emb_a, emb_a + dim);

    cortex_cnn_forward_somato(proc, seg_b.data(), 45);
    const float* emb_b = cortex_cnn_get_embedding(proc, &dim);

    /* Compute cosine similarity */
    float dot = 0, norm_a = 0, norm_b = 0;
    for (uint32_t i = 0; i < dim; i++) {
        dot += copy_a[i] * emb_b[i];
        norm_a += copy_a[i] * copy_a[i];
        norm_b += emb_b[i] * emb_b[i];
    }
    float cos_sim = (norm_a > 0 && norm_b > 0) ?
        dot / (sqrtf(norm_a) * sqrtf(norm_b)) : 0.0f;

    EXPECT_LT(cos_sim, 0.99f)
        << "Outputs should be diverse (cosine sim=" << cos_sim << ")";
}

// ============================================================
// Edge-case inputs
// ============================================================

TEST_F(CortexCNNRegressionTest, ZeroInputDoesNotProduceNaN) {
    auto* proc = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> zeros(45, 0.0f);
    const float* emb = cortex_cnn_forward_somato(proc, zeros.data(), 45);
    ASSERT_NE(emb, nullptr);

    uint32_t dim = 0;
    cortex_cnn_get_embedding(proc, &dim);
    for (uint32_t i = 0; i < dim; i++) {
        EXPECT_TRUE(std::isfinite(emb[i]))
            << "NaN from zero input at emb[" << i << "]";
    }
}

TEST_F(CortexCNNRegressionTest, ZeroPixelsDoNotCrash) {
    auto* proc = make(CORTEX_CNN_VISUAL);
    ASSERT_NE(proc, nullptr);

    std::vector<uint8_t> zeros(64 * 64 * 3, 0);
    const float* emb = cortex_cnn_forward_visual(proc, zeros.data(), 64, 64, 3);
    ASSERT_NE(emb, nullptr);

    uint32_t dim = 0;
    cortex_cnn_get_embedding(proc, &dim);
    for (uint32_t i = 0; i < dim; i++) {
        EXPECT_TRUE(std::isfinite(emb[i]));
    }
}

TEST_F(CortexCNNRegressionTest, MaxPixelsDoNotOverflow) {
    auto* proc = make(CORTEX_CNN_VISUAL);
    ASSERT_NE(proc, nullptr);

    std::vector<uint8_t> maxpix(64 * 64 * 3, 255);
    const float* emb = cortex_cnn_forward_visual(proc, maxpix.data(), 64, 64, 3);
    ASSERT_NE(emb, nullptr);

    uint32_t dim = 0;
    cortex_cnn_get_embedding(proc, &dim);
    for (uint32_t i = 0; i < dim; i++) {
        EXPECT_TRUE(std::isfinite(emb[i]));
        EXPECT_LT(std::abs(emb[i]), 1e6f) << "Embedding value too large";
    }
}

// ============================================================
// Determinism
// ============================================================

TEST_F(CortexCNNRegressionTest, SameInputSameOutput) {
    auto* proc = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> seg(45, 0.42f);

    cortex_cnn_forward_somato(proc, seg.data(), 45);
    uint32_t dim = 0;
    const float* emb1 = cortex_cnn_get_embedding(proc, &dim);
    std::vector<float> copy1(emb1, emb1 + dim);

    cortex_cnn_forward_somato(proc, seg.data(), 45);
    const float* emb2 = cortex_cnn_get_embedding(proc, &dim);

    for (uint32_t i = 0; i < dim; i++) {
        EXPECT_FLOAT_EQ(copy1[i], emb2[i])
            << "Non-deterministic output at index " << i;
    }
}

// ============================================================
// Checkpoint round-trip
// ============================================================

TEST_F(CortexCNNRegressionTest, CheckpointRoundTrip) {
    auto* proc = make(CORTEX_CNN_AUDIO);
    ASSERT_NE(proc, nullptr);

    /* Train */
    std::vector<float> mel(128, 0.5f);
    for (int i = 0; i < 10; i++) {
        cortex_cnn_forward_audio(proc, mel.data(), 128);
        cortex_cnn_backward(proc, "beep", 64);
    }

    /* Forward and capture */
    cortex_cnn_forward_audio(proc, mel.data(), 128);
    uint32_t dim = 0;
    const float* emb = cortex_cnn_get_embedding(proc, &dim);
    std::vector<float> pre(emb, emb + dim);

    /* Save + load */
    const char* path = "/tmp/test_cortex_audio_ckpt.weights";
    ASSERT_EQ(cortex_cnn_save(proc, path), 0);

    auto* proc2 = cortex_cnn_create(CORTEX_CNN_AUDIO, 0);
    ASSERT_NE(proc2, nullptr);
    ASSERT_EQ(cortex_cnn_load(proc2, path), 0);

    cortex_cnn_forward_audio(proc2, mel.data(), 128);
    const float* emb2 = cortex_cnn_get_embedding(proc2, &dim);
    ASSERT_NE(emb2, nullptr);

    float max_diff = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        max_diff = std::max(max_diff, std::abs(emb2[i] - pre[i]));
    }
    EXPECT_LT(max_diff, 1e-5f) << "Checkpoint should preserve weights exactly";

    cortex_cnn_destroy(proc2);
    remove(path);
}
