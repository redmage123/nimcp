/**
 * @file test_lnn_readout.cpp
 * @brief Unit tests for the LNN learned linear readout (2026-11-03).
 *
 * The readout maps the last layer's state x [n_outputs] to the training
 * target: y = W·x + b. It replaced scoring the raw ODE state (|x| up to
 * ±10) against the first n_outputs dims of a 4096-dim target, which pinned
 * the Vast LNN MSE at ~24.
 *
 * TEST COVERAGE:
 * - forward equals W·x + b
 * - backward matches finite differences for W, b and x
 * - re-init with the same dim keeps learned weights; new dim re-creates
 * - lnn_network_zero_gradients leaves readout grads alone (the adjoint
 *   backward calls it via lnn_gradient_reset); readout_zero_grad clears them
 * - save/load round-trip preserves the readout exactly (format v3)
 * - a v2 file (no readout block) loads with readout_dim == 0
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_types.h"
#include "utils/tensor/nimcp_tensor.h"

namespace {

constexpr uint32_t N_IN  = 8;
constexpr uint32_t N_OUT = 4;     // LNN state dim feeding the readout
constexpr uint32_t R_DIM = 6;     // readout output dim

float* Data(nimcp_tensor_t* t) { return static_cast<float*>(nimcp_tensor_data(t)); }

class LnnReadoutTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(lnn_init(1), LNN_SUCCESS);
        net = lnn_network_create_ncp(N_IN, 8, 8, N_OUT);
        ASSERT_NE(net, nullptr);
        ASSERT_EQ(net->n_outputs, N_OUT);
        EXPECT_EQ(net->readout_dim, 0u);
        EXPECT_EQ(net->readout_W, nullptr);
    }

    void TearDown() override {
        lnn_network_destroy(net);
        net = nullptr;
        for (const auto& p : tmp_paths) std::remove(p.c_str());
    }

    std::string TmpPath(const char* tag) {
        std::string p = "/tmp/test_lnn_readout_" + std::to_string(getpid()) + "_" + tag + ".lnn";
        tmp_paths.push_back(p);
        return p;
    }

    /** Fill W, b with deterministic distinct values. */
    void SetKnownWeights() {
        float* W = Data(net->readout_W);
        float* b = Data(net->readout_b);
        for (uint32_t o = 0; o < R_DIM; o++) {
            for (uint32_t k = 0; k < N_OUT; k++) {
                W[o * N_OUT + k] = 0.1f * static_cast<float>(o + 1)
                                 - 0.05f * static_cast<float>(k);
            }
            b[o] = 0.01f * static_cast<float>(o) - 0.02f;
        }
    }

    /** L = 0.5 * Σ (y - t)²  with y = W·x + b. */
    double Loss(const std::vector<float>& x, const std::vector<float>& t) {
        std::vector<float> y(R_DIM);
        EXPECT_EQ(lnn_network_readout_forward(net, x.data(), y.data()), LNN_SUCCESS);
        double l = 0.0;
        for (uint32_t o = 0; o < R_DIM; o++) {
            double d = static_cast<double>(y[o]) - t[o];
            l += 0.5 * d * d;
        }
        return l;
    }

    lnn_network_t* net = nullptr;
    std::vector<std::string> tmp_paths;
};

TEST_F(LnnReadoutTest, InitAllocatesShapesAndScale) {
    ASSERT_EQ(lnn_network_readout_init(net, R_DIM, 0.5f, 42), LNN_SUCCESS);
    EXPECT_EQ(net->readout_dim, R_DIM);
    ASSERT_NE(net->readout_W, nullptr);
    ASSERT_NE(net->readout_b, nullptr);
    ASSERT_NE(net->readout_grad_W, nullptr);
    ASSERT_NE(net->readout_grad_b, nullptr);
    EXPECT_EQ(nimcp_tensor_numel(net->readout_W), static_cast<size_t>(R_DIM) * N_OUT);
    EXPECT_EQ(nimcp_tensor_numel(net->readout_b), static_cast<size_t>(R_DIM));

    const float* W = Data(net->readout_W);
    const float* b = Data(net->readout_b);
    bool any_nonzero = false;
    for (size_t i = 0; i < static_cast<size_t>(R_DIM) * N_OUT; i++) {
        EXPECT_TRUE(std::isfinite(W[i]));
        if (W[i] != 0.0f) any_nonzero = true;
    }
    EXPECT_TRUE(any_nonzero);
    for (uint32_t o = 0; o < R_DIM; o++) EXPECT_EQ(b[o], 0.0f);
}

TEST_F(LnnReadoutTest, InitRejectsBadArgs) {
    EXPECT_NE(lnn_network_readout_init(nullptr, R_DIM, 0.5f, 1), LNN_SUCCESS);
    EXPECT_NE(lnn_network_readout_init(net, 0, 0.5f, 1), LNN_SUCCESS);
    EXPECT_EQ(net->readout_dim, 0u);
}

TEST_F(LnnReadoutTest, ForwardWithoutReadoutFails) {
    std::vector<float> x(N_OUT, 1.0f), y(R_DIM, 0.0f);
    EXPECT_NE(lnn_network_readout_forward(net, x.data(), y.data()), LNN_SUCCESS);
}

TEST_F(LnnReadoutTest, ForwardEqualsWxPlusB) {
    ASSERT_EQ(lnn_network_readout_init(net, R_DIM, 0.5f, 7), LNN_SUCCESS);
    SetKnownWeights();
    const std::vector<float> x = { 1.5f, -2.0f, 0.25f, 3.0f };
    std::vector<float> y(R_DIM, 0.0f);
    ASSERT_EQ(lnn_network_readout_forward(net, x.data(), y.data()), LNN_SUCCESS);

    const float* W = Data(net->readout_W);
    const float* b = Data(net->readout_b);
    for (uint32_t o = 0; o < R_DIM; o++) {
        double ref = b[o];
        for (uint32_t k = 0; k < N_OUT; k++) ref += static_cast<double>(W[o * N_OUT + k]) * x[k];
        EXPECT_NEAR(y[o], ref, 1e-5) << "output " << o;
    }
}

TEST_F(LnnReadoutTest, BackwardMatchesFiniteDifferences) {
    ASSERT_EQ(lnn_network_readout_init(net, R_DIM, 0.5f, 7), LNN_SUCCESS);
    SetKnownWeights();
    std::vector<float> x = { 0.7f, -1.2f, 0.4f, 2.1f };
    const std::vector<float> t = { 0.1f, -0.3f, 0.2f, 0.0f, 0.5f, -0.1f };

    // Analytic: dL/dy = y - t.
    std::vector<float> y(R_DIM);
    ASSERT_EQ(lnn_network_readout_forward(net, x.data(), y.data()), LNN_SUCCESS);
    std::vector<float> dl_dy(R_DIM);
    for (uint32_t o = 0; o < R_DIM; o++) dl_dy[o] = y[o] - t[o];

    lnn_network_readout_zero_grad(net);
    std::vector<float> dl_dx(N_OUT, 123.0f);
    ASSERT_EQ(lnn_network_readout_backward(net, x.data(), dl_dy.data(), dl_dx.data()),
              LNN_SUCCESS);
    const std::vector<float> gW(Data(net->readout_grad_W),
                                Data(net->readout_grad_W) + R_DIM * N_OUT);
    const std::vector<float> gb(Data(net->readout_grad_b),
                                Data(net->readout_grad_b) + R_DIM);

    const float eps = 1e-3f;
    const double tol = 2e-3;
    float* W = Data(net->readout_W);
    for (size_t i = 0; i < static_cast<size_t>(R_DIM) * N_OUT; i++) {
        const float w0 = W[i];
        W[i] = w0 + eps; double lp = Loss(x, t);
        W[i] = w0 - eps; double lm = Loss(x, t);
        W[i] = w0;
        EXPECT_NEAR(gW[i], (lp - lm) / (2.0 * eps), tol) << "dW[" << i << "]";
    }
    float* b = Data(net->readout_b);
    for (uint32_t o = 0; o < R_DIM; o++) {
        const float b0 = b[o];
        b[o] = b0 + eps; double lp = Loss(x, t);
        b[o] = b0 - eps; double lm = Loss(x, t);
        b[o] = b0;
        EXPECT_NEAR(gb[o], (lp - lm) / (2.0 * eps), tol) << "db[" << o << "]";
    }
    for (uint32_t k = 0; k < N_OUT; k++) {
        const float x0 = x[k];
        x[k] = x0 + eps; double lp = Loss(x, t);
        x[k] = x0 - eps; double lm = Loss(x, t);
        x[k] = x0;
        EXPECT_NEAR(dl_dx[k], (lp - lm) / (2.0 * eps), tol) << "dx[" << k << "]";
    }
}

TEST_F(LnnReadoutTest, BackwardAccumulatesAcrossCalls) {
    ASSERT_EQ(lnn_network_readout_init(net, R_DIM, 0.5f, 7), LNN_SUCCESS);
    const std::vector<float> x = { 1.0f, 2.0f, -1.0f, 0.5f };
    const std::vector<float> g(R_DIM, 1.0f);
    lnn_network_readout_zero_grad(net);
    ASSERT_EQ(lnn_network_readout_backward(net, x.data(), g.data(), nullptr), LNN_SUCCESS);
    ASSERT_EQ(lnn_network_readout_backward(net, x.data(), g.data(), nullptr), LNN_SUCCESS);
    const float* gW = Data(net->readout_grad_W);
    const float* gb = Data(net->readout_grad_b);
    for (uint32_t o = 0; o < R_DIM; o++) {
        EXPECT_FLOAT_EQ(gb[o], 2.0f);
        for (uint32_t k = 0; k < N_OUT; k++) EXPECT_FLOAT_EQ(gW[o * N_OUT + k], 2.0f * x[k]);
    }
}

TEST_F(LnnReadoutTest, ReinitSameDimKeepsWeightsNewDimRecreates) {
    ASSERT_EQ(lnn_network_readout_init(net, R_DIM, 0.5f, 7), LNN_SUCCESS);
    SetKnownWeights();
    const float w_marker = Data(net->readout_W)[3];
    ASSERT_EQ(lnn_network_readout_init(net, R_DIM, 0.5f, 999), LNN_SUCCESS);
    EXPECT_FLOAT_EQ(Data(net->readout_W)[3], w_marker);

    ASSERT_EQ(lnn_network_readout_init(net, R_DIM + 2, 0.5f, 7), LNN_SUCCESS);
    EXPECT_EQ(net->readout_dim, R_DIM + 2);
    EXPECT_EQ(nimcp_tensor_numel(net->readout_W), static_cast<size_t>(R_DIM + 2) * N_OUT);

    lnn_network_readout_free(net);
    EXPECT_EQ(net->readout_dim, 0u);
    EXPECT_EQ(net->readout_W, nullptr);
    EXPECT_EQ(net->readout_grad_b, nullptr);
}

TEST_F(LnnReadoutTest, NetworkZeroGradientsLeavesReadoutGrads) {
    ASSERT_EQ(lnn_network_readout_init(net, R_DIM, 0.5f, 7), LNN_SUCCESS);
    const std::vector<float> x(N_OUT, 1.0f), g(R_DIM, 1.0f);
    lnn_network_readout_zero_grad(net);
    ASSERT_EQ(lnn_network_readout_backward(net, x.data(), g.data(), nullptr), LNN_SUCCESS);

    // The adjoint backward calls this internally (lnn_gradient_reset).
    ASSERT_EQ(lnn_network_zero_gradients(net), LNN_SUCCESS);
    EXPECT_FLOAT_EQ(Data(net->readout_grad_b)[0], 1.0f);
    EXPECT_FLOAT_EQ(Data(net->readout_grad_W)[0], 1.0f);

    lnn_network_readout_zero_grad(net);
    for (uint32_t o = 0; o < R_DIM; o++) EXPECT_EQ(Data(net->readout_grad_b)[o], 0.0f);
    for (size_t i = 0; i < static_cast<size_t>(R_DIM) * N_OUT; i++) {
        EXPECT_EQ(Data(net->readout_grad_W)[i], 0.0f);
    }
}

TEST_F(LnnReadoutTest, SaveLoadRoundTripPreservesReadout) {
    ASSERT_EQ(lnn_network_readout_init(net, R_DIM, 0.5f, 7), LNN_SUCCESS);
    SetKnownWeights();
    const std::string path = TmpPath("roundtrip");
    ASSERT_EQ(lnn_network_save(net, path.c_str()), 0);

    lnn_network_t* loaded = lnn_network_load(path.c_str());
    ASSERT_NE(loaded, nullptr);
    ASSERT_EQ(loaded->readout_dim, R_DIM);
    ASSERT_NE(loaded->readout_W, nullptr);
    ASSERT_NE(loaded->readout_grad_W, nullptr);
    EXPECT_EQ(0, std::memcmp(Data(loaded->readout_W), Data(net->readout_W),
                             sizeof(float) * R_DIM * N_OUT));
    EXPECT_EQ(0, std::memcmp(Data(loaded->readout_b), Data(net->readout_b),
                             sizeof(float) * R_DIM));
    lnn_network_destroy(loaded);
}

TEST_F(LnnReadoutTest, SaveWithoutReadoutLoadsWithoutReadout) {
    const std::string path = TmpPath("noreadout");
    ASSERT_EQ(lnn_network_save(net, path.c_str()), 0);
    lnn_network_t* loaded = lnn_network_load(path.c_str());
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->readout_dim, 0u);
    EXPECT_EQ(loaded->readout_W, nullptr);
    lnn_network_destroy(loaded);
}

// A v2 file is a v3 file without the trailing 8-byte readout header
// (readout_dim = 0, n_outputs) and with version = 2.
TEST_F(LnnReadoutTest, LegacyV2FileLoadsWithoutReadout) {
    const std::string path = TmpPath("v2");
    ASSERT_EQ(lnn_network_save(net, path.c_str()), 0);

    FILE* f = std::fopen(path.c_str(), "rb");
    ASSERT_NE(f, nullptr);
    std::vector<unsigned char> bytes;
    unsigned char buf[4096];
    size_t r;
    while ((r = std::fread(buf, 1, sizeof(buf), f)) > 0) bytes.insert(bytes.end(), buf, buf + r);
    std::fclose(f);
    ASSERT_GT(bytes.size(), 16u);

    uint32_t version = 0, trailer_rd = 99;
    std::memcpy(&version, bytes.data() + 4, sizeof(uint32_t));
    ASSERT_EQ(version, 3u);
    std::memcpy(&trailer_rd, bytes.data() + bytes.size() - 8, sizeof(uint32_t));
    ASSERT_EQ(trailer_rd, 0u) << "expected empty readout block at end of file";

    bytes.resize(bytes.size() - 8);
    const uint32_t v2 = 2;
    std::memcpy(bytes.data() + 4, &v2, sizeof(uint32_t));
    f = std::fopen(path.c_str(), "wb");
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(std::fwrite(bytes.data(), 1, bytes.size(), f), bytes.size());
    std::fclose(f);

    lnn_network_t* loaded = lnn_network_load(path.c_str());
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->readout_dim, 0u);
    EXPECT_EQ(loaded->readout_W, nullptr);
    EXPECT_EQ(loaded->n_outputs, N_OUT);
    lnn_network_destroy(loaded);
}

}  // namespace
