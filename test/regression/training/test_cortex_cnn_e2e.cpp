/**
 * @file test_cortex_cnn_e2e.cpp
 * @brief End-to-end tests for cortex CNN processors
 *
 * Full cycle: sensory data -> cortex CNN -> forward -> learn -> verify progression.
 * Multi-modal training, differentiation verification, cooperation testing.
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

class CortexCNNE2ETest : public ::testing::Test {
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
// Full training cycle: forward -> learn -> loss decreases
// ============================================================

TEST_F(CortexCNNE2ETest, SomatoFullCycle) {
    auto* proc = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> seg(45, 0.5f);
    float losses[50];

    for (int step = 0; step < 50; step++) {
        cortex_cnn_forward_somato(proc, seg.data(), 45);
        losses[step] = cortex_cnn_backward(proc, "press", 32);
        ASSERT_TRUE(std::isfinite(losses[step])) << "Step " << step;
    }

    /* Loss should trend downward: compare first 5 avg vs last 5 avg */
    float early_avg = 0, late_avg = 0;
    for (int i = 0; i < 5; i++) early_avg += losses[i];
    for (int i = 45; i < 50; i++) late_avg += losses[i];
    early_avg /= 5.0f;
    late_avg /= 5.0f;

    EXPECT_LT(late_avg, early_avg) << "Loss should decrease over 50 steps";
}

TEST_F(CortexCNNE2ETest, AudioFullCycle) {
    auto* proc = make(CORTEX_CNN_AUDIO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> mel(128, 0.5f);
    float first_loss = -1, last_loss = -1;

    for (int step = 0; step < 30; step++) {
        cortex_cnn_forward_audio(proc, mel.data(), 128);
        float loss = cortex_cnn_backward(proc, "tone", 64);
        ASSERT_TRUE(std::isfinite(loss));
        if (step == 0) first_loss = loss;
        if (step == 29) last_loss = loss;
    }

    EXPECT_LT(last_loss, first_loss) << "Audio loss should decrease";
}

// ============================================================
// Multi-modal training
// ============================================================

TEST_F(CortexCNNE2ETest, MultiModalTraining) {
    auto* audio = make(CORTEX_CNN_AUDIO);
    auto* somato = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(audio, nullptr);
    ASSERT_NE(somato, nullptr);

    std::vector<float> mel(128, 0.5f);
    std::vector<float> seg(45, 0.3f);

    for (int step = 0; step < 20; step++) {
        /* Both train on same label */
        cortex_cnn_forward_audio(audio, mel.data(), 128);
        cortex_cnn_backward(audio, "stimulus", 64);

        cortex_cnn_forward_somato(somato, seg.data(), 45);
        cortex_cnn_backward(somato, "stimulus", 32);
    }

    /* Fuse and verify */
    cortex_cnn_forward_audio(audio, mel.data(), 128);
    cortex_cnn_forward_somato(somato, seg.data(), 45);

    cortex_cnn_processor_t* procs[4] = {nullptr, audio, nullptr, somato};
    std::vector<float> fused(192, 0.0f);
    uint32_t dim = cortex_cnn_fuse(procs, 4, fused.data(), 192);
    EXPECT_GT(dim, 0u);

    /* Fused embedding should be non-trivial */
    float norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) norm += fused[i] * fused[i];
    EXPECT_GT(norm, 0.0f);
}

// ============================================================
// Differentiation verification
// ============================================================

TEST_F(CortexCNNE2ETest, DifferentImagesProduceDifferentEmbeddings) {
    auto* proc = make(CORTEX_CNN_VISUAL);
    ASSERT_NE(proc, nullptr);

    /* Image A: bright */
    std::vector<uint8_t> bright(64 * 64 * 3, 200);
    cortex_cnn_forward_visual(proc, bright.data(), 64, 64, 3);
    uint32_t dim = 0;
    const float* emb_a = cortex_cnn_get_embedding(proc, &dim);
    std::vector<float> a(emb_a, emb_a + dim);

    /* Image B: dark */
    std::vector<uint8_t> dark(64 * 64 * 3, 30);
    cortex_cnn_forward_visual(proc, dark.data(), 64, 64, 3);
    const float* emb_b = cortex_cnn_get_embedding(proc, &dim);

    float diff = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        diff += (a[i] - emb_b[i]) * (a[i] - emb_b[i]);
    }
    EXPECT_GT(diff, 1e-6f) << "Different images should produce different embeddings";
}

TEST_F(CortexCNNE2ETest, DifferentAudioProduceDifferentEmbeddings) {
    auto* proc = make(CORTEX_CNN_AUDIO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> low(128, 0.1f);
    std::vector<float> high(128, 0.9f);

    cortex_cnn_forward_audio(proc, low.data(), 128);
    uint32_t dim = 0;
    const float* e1 = cortex_cnn_get_embedding(proc, &dim);
    std::vector<float> c1(e1, e1 + dim);

    cortex_cnn_forward_audio(proc, high.data(), 128);
    const float* e2 = cortex_cnn_get_embedding(proc, &dim);

    float diff = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        diff += (c1[i] - e2[i]) * (c1[i] - e2[i]);
    }
    EXPECT_GT(diff, 1e-6f);
}

// ============================================================
// Training progression: weight norms change
// ============================================================

TEST_F(CortexCNNE2ETest, WeightNormsChangeAfterTraining) {
    auto* proc = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    /* Get initial embedding norm */
    std::vector<float> seg(45, 0.5f);
    cortex_cnn_forward_somato(proc, seg.data(), 45);
    cortex_cnn_metrics_t m0 = {};
    cortex_cnn_get_metrics(proc, &m0);
    float norm0 = m0.embedding_norm;

    /* Train 50 steps */
    for (int i = 0; i < 50; i++) {
        cortex_cnn_forward_somato(proc, seg.data(), 45);
        cortex_cnn_backward(proc, "train_label", 32);
    }

    /* Get post-training embedding norm */
    cortex_cnn_forward_somato(proc, seg.data(), 45);
    cortex_cnn_metrics_t m1 = {};
    cortex_cnn_get_metrics(proc, &m1);

    /* Norms should differ after training */
    EXPECT_NE(m1.embedding_norm, norm0)
        << "Embedding norm should change after training";
    EXPECT_EQ(m1.backward_steps, 50u);
}

// ============================================================
// Mode collapse detection
// ============================================================

TEST_F(CortexCNNE2ETest, NoCosineCollapseAfter50Steps) {
    auto* proc = make(CORTEX_CNN_SOMATO);
    ASSERT_NE(proc, nullptr);

    std::vector<float> seg(45, 0.5f);
    for (int i = 0; i < 50; i++) {
        cortex_cnn_forward_somato(proc, seg.data(), 45);
        cortex_cnn_backward(proc, "act", 32);
    }

    /* Check different inputs produce different outputs */
    std::vector<float> seg_a(45, 0.1f);
    std::vector<float> seg_b(45, 0.9f);

    cortex_cnn_forward_somato(proc, seg_a.data(), 45);
    uint32_t dim = 0;
    const float* ea = cortex_cnn_get_embedding(proc, &dim);
    std::vector<float> ca(ea, ea + dim);

    cortex_cnn_forward_somato(proc, seg_b.data(), 45);
    const float* eb = cortex_cnn_get_embedding(proc, &dim);

    float dot = 0, na = 0, nb = 0;
    for (uint32_t i = 0; i < dim; i++) {
        dot += ca[i] * eb[i];
        na += ca[i] * ca[i];
        nb += eb[i] * eb[i];
    }
    float cos_sim = (na > 0 && nb > 0) ? dot / (sqrtf(na) * sqrtf(nb)) : 0.0f;
    EXPECT_LT(cos_sim, 0.99f)
        << "Cosine similarity " << cos_sim << " indicates mode collapse";
}

// ============================================================
// All 4 modalities full cycle
// ============================================================

TEST_F(CortexCNNE2ETest, AllFourModalitiesTrain) {
    auto* v = make(CORTEX_CNN_VISUAL);
    auto* a = make(CORTEX_CNN_AUDIO);
    auto* s = make(CORTEX_CNN_SPEECH);
    auto* m = make(CORTEX_CNN_SOMATO);

    std::vector<uint8_t> pix(64*64*3, 128);
    std::vector<float> mel(128, 0.4f);
    std::vector<float> pho(64, 0.3f);
    std::vector<float> seg(45, 0.5f);

    for (int step = 0; step < 10; step++) {
        cortex_cnn_forward_visual(v, pix.data(), 64, 64, 3);
        cortex_cnn_backward(v, "obj", 64);

        cortex_cnn_forward_audio(a, mel.data(), 128);
        cortex_cnn_backward(a, "snd", 64);

        cortex_cnn_forward_speech(s, pho.data(), 64);
        cortex_cnn_backward(s, "wrd", 32);

        cortex_cnn_forward_somato(m, seg.data(), 45);
        cortex_cnn_backward(m, "tch", 32);
    }

    /* All should have trained */
    cortex_cnn_metrics_t mv={}, ma={}, ms={}, mm={};
    cortex_cnn_get_metrics(v, &mv);
    cortex_cnn_get_metrics(a, &ma);
    cortex_cnn_get_metrics(s, &ms);
    cortex_cnn_get_metrics(m, &mm);

    EXPECT_EQ(mv.backward_steps, 10u);
    EXPECT_EQ(ma.backward_steps, 10u);
    EXPECT_EQ(ms.backward_steps, 10u);
    EXPECT_EQ(mm.backward_steps, 10u);
    EXPECT_GE(mv.last_loss, 0.0f);
    EXPECT_GE(ma.last_loss, 0.0f);
    EXPECT_GE(ms.last_loss, 0.0f);
    EXPECT_GE(mm.last_loss, 0.0f);
}
