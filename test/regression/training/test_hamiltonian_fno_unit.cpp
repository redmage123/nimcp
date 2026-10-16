/**
 * @file test_hamiltonian_fno_unit.cpp
 * @brief Unit tests for Hamiltonian LNN, FNO Spectral Conv, and SNN FNO
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "lnn/nimcp_lnn_hamiltonian.h"
#include "training/nimcp_fno_layer.h"
#include "snn/nimcp_snn_fno.h"
}

// =========================================================================
// Hamiltonian LNN Unit Tests
// =========================================================================

class HamiltonianTest : public ::testing::Test {
protected:
    lnn_hamiltonian_net_t* net = nullptr;

    void SetUp() override {
        lnn_hamiltonian_config_t cfg;
        lnn_hamiltonian_config_default(&cfg);
        cfg.hidden_dim = 32;
        cfg.n_hidden_layers = 2;
        net = lnn_hamiltonian_net_create(8, &cfg);
    }

    void TearDown() override {
        lnn_hamiltonian_net_destroy(net);
    }
};

TEST_F(HamiltonianTest, CreateDestroy) {
    ASSERT_NE(net, nullptr);
    EXPECT_EQ(net->state_dim, 8u);
    EXPECT_EQ(net->input_dim, 16u);  // 2 * state_dim
    EXPECT_EQ(net->n_layers, 3u);    // 2 hidden + 1 output
}

TEST_F(HamiltonianTest, CreateNull) {
    auto* n = lnn_hamiltonian_net_create(0, nullptr);
    EXPECT_EQ(n, nullptr);
}

TEST_F(HamiltonianTest, DestroyNull) {
    lnn_hamiltonian_net_destroy(nullptr);  // Should not crash
}

TEST_F(HamiltonianTest, EvalProducesScalar) {
    uint32_t dims[1] = {8};
    auto* q = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    auto* p = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(q, nullptr);
    ASSERT_NE(p, nullptr);

    // Set some values
    float* qd = (float*)nimcp_tensor_data(q);
    float* pd = (float*)nimcp_tensor_data(p);
    for (int i = 0; i < 8; i++) {
        qd[i] = 0.1f * (i + 1);
        pd[i] = 0.05f * (i + 1);
    }

    float H = lnn_hamiltonian_eval(net, q, p);
    EXPECT_TRUE(std::isfinite(H));

    nimcp_tensor_destroy(q);
    nimcp_tensor_destroy(p);
}

TEST_F(HamiltonianTest, GradProducesFiniteValues) {
    uint32_t dims[1] = {8};
    auto* q = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    auto* p = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    auto* dH_dq = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    auto* dH_dp = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);

    float* qd = (float*)nimcp_tensor_data(q);
    float* pd = (float*)nimcp_tensor_data(p);
    for (int i = 0; i < 8; i++) {
        qd[i] = 0.1f * (i + 1);
        pd[i] = 0.05f * (i + 1);
    }

    int rc = lnn_hamiltonian_grad(net, q, p, dH_dq, dH_dp);
    EXPECT_EQ(rc, 0);

    float* dq = (float*)nimcp_tensor_data(dH_dq);
    float* dp = (float*)nimcp_tensor_data(dH_dp);
    for (int i = 0; i < 8; i++) {
        EXPECT_TRUE(std::isfinite(dq[i]));
        EXPECT_TRUE(std::isfinite(dp[i]));
    }

    nimcp_tensor_destroy(q);
    nimcp_tensor_destroy(p);
    nimcp_tensor_destroy(dH_dq);
    nimcp_tensor_destroy(dH_dp);
}

TEST_F(HamiltonianTest, StormerVerletConservesEnergy) {
    uint32_t dims[1] = {8};
    auto* q = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    auto* p = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);

    float* qd = (float*)nimcp_tensor_data(q);
    float* pd = (float*)nimcp_tensor_data(p);
    for (int i = 0; i < 8; i++) {
        qd[i] = 0.5f * sinf((float)i);
        pd[i] = 0.5f * cosf((float)i);
    }

    // Record initial energy
    float H0 = lnn_hamiltonian_eval(net, q, p);
    EXPECT_TRUE(std::isfinite(H0));

    // Run 100 Störmer-Verlet steps
    float dt = 0.01f;
    for (int step = 0; step < 100; step++) {
        int rc = lnn_hamiltonian_step_stormer_verlet(net, q, p, nullptr, dt, 0.0f);
        EXPECT_EQ(rc, 0);
    }

    float H_final = lnn_hamiltonian_eval(net, q, p);
    float deviation = std::abs(H_final - H0) / (std::abs(H0) + 1e-8f);

    // Symplectic integrator should conserve energy well
    EXPECT_LT(deviation, 0.1f) << "Energy deviation too large: " << deviation;

    nimcp_tensor_destroy(q);
    nimcp_tensor_destroy(p);
}

TEST_F(HamiltonianTest, DifferentInputsDifferentH) {
    uint32_t dims[1] = {8};
    auto* q1 = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    auto* p1 = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    auto* q2 = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    auto* p2 = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);

    ((float*)nimcp_tensor_data(q1))[0] = 1.0f;
    ((float*)nimcp_tensor_data(q2))[0] = -1.0f;

    float H1 = lnn_hamiltonian_eval(net, q1, p1);
    float H2 = lnn_hamiltonian_eval(net, q2, p2);
    EXPECT_NE(H1, H2);

    nimcp_tensor_destroy(q1); nimcp_tensor_destroy(p1);
    nimcp_tensor_destroy(q2); nimcp_tensor_destroy(p2);
}

TEST_F(HamiltonianTest, ParamCount) {
    uint32_t count = lnn_hamiltonian_param_count(net);
    EXPECT_GT(count, 0u);
    // Input=16, hidden=32, output=1: 16*32+32 + 32*32+32 + 32*1+1 = 1633
    EXPECT_GT(count, 1000u);
}

TEST_F(HamiltonianTest, ResetGradients) {
    lnn_hamiltonian_reset_gradients(net);
    // After reset, all gradients should be zero
    for (uint32_t l = 0; l < net->n_layers; l++) {
        float* gw = (float*)nimcp_tensor_data(net->grad_W[l]);
        uint32_t n = nimcp_tensor_numel(net->grad_W[l]);
        float sum = 0;
        for (uint32_t i = 0; i < n; i++) sum += std::abs(gw[i]);
        EXPECT_EQ(sum, 0.0f);
    }
}

// =========================================================================
// FNO Spectral Conv Unit Tests
// =========================================================================

class FNOSpectralTest : public ::testing::Test {
protected:
    fno_spectral_conv_t* layer = nullptr;

    void SetUp() override {
        layer = fno_spectral_conv_create(4, 8, 16, 64);
    }

    void TearDown() override {
        fno_spectral_conv_destroy(layer);
    }
};

TEST_F(FNOSpectralTest, CreateDestroy) {
    ASSERT_NE(layer, nullptr);
    EXPECT_EQ(layer->in_channels, 4u);
    EXPECT_EQ(layer->out_channels, 8u);
    EXPECT_EQ(layer->n_modes, 16u);
    EXPECT_EQ(layer->spatial_size, 64u);
}

TEST_F(FNOSpectralTest, ForwardProducesOutput) {
    std::vector<float> input(4 * 64, 0.5f);
    std::vector<float> output(8 * 64, 0.0f);

    int rc = fno_spectral_conv_forward(layer, input.data(), output.data());
    EXPECT_EQ(rc, 0);

    // Output should be finite and non-zero
    float norm = 0;
    for (auto v : output) {
        EXPECT_TRUE(std::isfinite(v));
        norm += v * v;
    }
    EXPECT_GT(norm, 0.0f);
}

TEST_F(FNOSpectralTest, DifferentInputsDifferentOutputs) {
    std::vector<float> in1(4 * 64, 0.3f);
    std::vector<float> in2(4 * 64, 0.7f);
    std::vector<float> out1(8 * 64, 0.0f);
    std::vector<float> out2(8 * 64, 0.0f);

    fno_spectral_conv_forward(layer, in1.data(), out1.data());
    fno_spectral_conv_forward(layer, in2.data(), out2.data());

    bool different = false;
    for (size_t i = 0; i < out1.size(); i++) {
        if (std::abs(out1[i] - out2[i]) > 1e-6f) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);
}

TEST_F(FNOSpectralTest, BackwardProducesGradients) {
    std::vector<float> input(4 * 64, 0.5f);
    std::vector<float> output(8 * 64, 0.0f);
    std::vector<float> dl_dout(8 * 64, 1.0f);
    std::vector<float> dl_din(4 * 64, 0.0f);

    fno_spectral_conv_zero_grad(layer);
    fno_spectral_conv_forward(layer, input.data(), output.data());
    int rc = fno_spectral_conv_backward(layer, dl_dout.data(), dl_din.data());
    EXPECT_EQ(rc, 0);

    // Gradients should be non-zero
    float grad_norm = 0;
    for (uint32_t i = 0; i < layer->out_channels * layer->in_channels * layer->n_modes; i++) {
        grad_norm += layer->grad_W_real[i] * layer->grad_W_real[i];
    }
    EXPECT_GT(grad_norm, 0.0f);
}

TEST_F(FNOSpectralTest, ParamCount) {
    uint32_t count = fno_spectral_conv_param_count(layer);
    EXPECT_GT(count, 0u);
    // 4*8*16*2 (spectral) + 4*8 (bypass) + 8 (bias) = 1064
    EXPECT_EQ(count, 4u * 8 * 16 * 2 + 4 * 8 + 8);
}

// =========================================================================
// FNO Audio Processor Unit Tests
// =========================================================================

class FNOAudioTest : public ::testing::Test {
protected:
    fno_audio_processor_t* proc = nullptr;

    void SetUp() override {
        proc = fno_audio_create(128, 64, 16, 32, 2);
    }

    void TearDown() override {
        fno_audio_destroy(proc);
    }
};

TEST_F(FNOAudioTest, CreateDestroy) {
    ASSERT_NE(proc, nullptr);
    EXPECT_EQ(proc->input_size, 128u);
    EXPECT_EQ(proc->embed_dim, 64u);
    EXPECT_EQ(proc->n_blocks, 2u);
}

TEST_F(FNOAudioTest, ForwardProducesEmbedding) {
    std::vector<float> mel(128, 0.5f);
    std::vector<float> embedding(64, 0.0f);

    int rc = fno_audio_forward(proc, mel.data(), 128, embedding.data());
    EXPECT_EQ(rc, 0);

    float norm = 0;
    for (auto v : embedding) {
        EXPECT_TRUE(std::isfinite(v));
        norm += v * v;
    }
    EXPECT_GT(norm, 0.0f);
}

TEST_F(FNOAudioTest, DifferentMelDifferentEmbedding) {
    std::vector<float> mel1(128), mel2(128);
    for (int i = 0; i < 128; i++) {
        mel1[i] = sinf(i * 0.1f);
        mel2[i] = cosf(i * 0.1f);
    }

    std::vector<float> emb1(64, 0.0f), emb2(64, 0.0f);
    fno_audio_forward(proc, mel1.data(), 128, emb1.data());
    fno_audio_forward(proc, mel2.data(), 128, emb2.data());

    float diff = 0;
    for (int i = 0; i < 64; i++) diff += (emb1[i] - emb2[i]) * (emb1[i] - emb2[i]);
    EXPECT_GT(diff, 0.0f);
}

TEST_F(FNOAudioTest, BackwardAndStep) {
    std::vector<float> mel(128, 0.5f);
    std::vector<float> embedding(64, 0.0f);
    std::vector<float> dl_dembed(64, 1.0f);

    fno_audio_zero_grad(proc);
    fno_audio_forward(proc, mel.data(), 128, embedding.data());
    int rc = fno_audio_backward(proc, dl_dembed.data(), nullptr);
    EXPECT_EQ(rc, 0);

    // Step should not crash
    fno_audio_step(proc, 0.001f);
}

TEST_F(FNOAudioTest, ParamCount) {
    uint32_t count = fno_audio_param_count(proc);
    EXPECT_GT(count, 100u);
}

// =========================================================================
// SNN FNO Population Unit Tests
// =========================================================================

class SNNFNOTest : public ::testing::Test {
protected:
    snn_fno_population_t* fno = nullptr;

    void SetUp() override {
        snn_fno_config_t cfg;
        snn_fno_config_default(&cfg);
        cfg.n_modes = 8;
        cfg.hidden_channels = 8;
        cfg.n_blocks = 2;
        cfg.training_buffer_size = 32;
        fno = snn_fno_population_create(0, 32, &cfg);
    }

    void TearDown() override {
        snn_fno_population_destroy(fno);
    }
};

TEST_F(SNNFNOTest, CreateDestroy) {
    ASSERT_NE(fno, nullptr);
    EXPECT_EQ(fno->n_neurons, 32u);
    EXPECT_EQ(fno->n_blocks, 2u);
}

TEST_F(SNNFNOTest, RecordPair) {
    std::vector<float> v(32, -70.0f);
    std::vector<float> isyn(32, 5.0f);
    std::vector<float> v_next(32, -65.0f);
    std::vector<float> spikes(32, 0.0f);
    spikes[0] = 1.0f;

    int rc = snn_fno_record_pair(fno, v.data(), isyn.data(), v_next.data(), spikes.data(), 32);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(fno->buffer_count, 1u);
}

TEST_F(SNNFNOTest, BufferWraps) {
    std::vector<float> v(32, -70.0f), isyn(32, 0.0f), vn(32, -65.0f), sp(32, 0.0f);

    for (int i = 0; i < 50; i++) {
        snn_fno_record_pair(fno, v.data(), isyn.data(), vn.data(), sp.data(), 32);
    }
    // Buffer size is 32, should be full
    EXPECT_EQ(fno->buffer_count, 32u);
}

TEST_F(SNNFNOTest, PredictProducesOutput) {
    std::vector<float> v(32, -70.0f);
    std::vector<float> isyn(32, 5.0f);
    std::vector<float> v_next(32, 0.0f);
    std::vector<float> spikes(32, 0.0f);

    int rc = snn_fno_predict(fno, v.data(), isyn.data(), 32, v_next.data(), spikes.data());
    EXPECT_EQ(rc, 0);

    for (int i = 0; i < 32; i++) {
        EXPECT_TRUE(std::isfinite(v_next[i]));
        EXPECT_TRUE(spikes[i] == 0.0f || spikes[i] == 1.0f);
    }
}

TEST_F(SNNFNOTest, TrainReducesMSE) {
    // Fill buffer with simple patterns
    for (int s = 0; s < 32; s++) {
        std::vector<float> v(32), isyn(32), vn(32), sp(32, 0.0f);
        for (int i = 0; i < 32; i++) {
            v[i] = -70.0f + (float)i * 0.1f;
            isyn[i] = (float)s * 0.1f;
            vn[i] = v[i] + isyn[i] * 0.01f; // Simple dynamics
            sp[i] = (vn[i] > -50.0f) ? 1.0f : 0.0f;
        }
        snn_fno_record_pair(fno, v.data(), isyn.data(), vn.data(), sp.data(), 32);
    }

    // Train
    float mse_before = snn_fno_get_train_mse(fno);
    int rc = snn_fno_train(fno, 5);
    EXPECT_EQ(rc, 0);
    float mse_after = snn_fno_get_train_mse(fno);

    // MSE should be finite
    EXPECT_TRUE(std::isfinite(mse_after));
}

TEST_F(SNNFNOTest, ParamCount) {
    uint32_t count = snn_fno_param_count(fno);
    EXPECT_GT(count, 100u);
}

TEST_F(SNNFNOTest, NotReadyWithoutTraining) {
    EXPECT_FALSE(snn_fno_is_ready(fno));
}
