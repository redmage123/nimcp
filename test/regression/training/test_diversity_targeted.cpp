/**
 * @file test_diversity_targeted.cpp
 * @brief Regression tests for the target-aware, task-bounded diversity loss
 *
 * Bug (2026-11-04): the UTM anti-collapse diversity gradient (weight 0.5,
 * scale-invariant cosine push) outweighed the MSE gradient ~1000x and, being
 * orthogonal to the output, inflated output norms without limit — the LNN
 * readout drifted to 10x the target rms with ~0 cosine to the target.
 *
 * nimcp_anti_collapse_diversity_loss_targeted() fixes this with a hinge
 * (push only when outputs are more alike than their targets) and a bound
 * (||added|| <= weight * ||task gradient||).
 *
 * @date 2026-11-04
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <random>
#include <vector>

extern "C" {
#include "training/nimcp_unified_training.h"
}

namespace {

float l2(const std::vector<float>& v) {
    double s = 0.0;
    for (float x : v) s += (double)x * x;
    return (float)std::sqrt(s);
}

float rms(const std::vector<float>& v) {
    return l2(v) / std::sqrt((float)v.size());
}

std::vector<float> diff(const std::vector<float>& a, const std::vector<float>& b) {
    std::vector<float> d(a.size());
    for (size_t i = 0; i < a.size(); i++) d[i] = a[i] - b[i];
    return d;
}

class DiversityTargetedTest : public ::testing::Test {
protected:
    nimcp_anti_collapse_state_t st;
    nimcp_anti_collapse_config_t cfg;

    void SetUp() override {
        std::memset(&cfg, 0, sizeof(cfg));
        cfg.diversity_loss_weight = 0.5f;
        cfg.diversity_buffer_size = 8;
        cfg.use_gradient_normalization = true;
        cfg.gradient_target_norm = 0.0f;
        cfg.gradient_clip_value = 5.0f;
        cfg.adaptive_gradient_target = true;
        ASSERT_EQ(nimcp_anti_collapse_init(&st, &cfg), 0);
    }
    void TearDown() override { nimcp_anti_collapse_destroy(&st); }
};

/* 1. Guards: bad args return 0 and leave grad untouched */
TEST_F(DiversityTargetedTest, BadArgsReturnZeroAndLeaveGradUntouched) {
    float out[4] = {1, 2, 3, 4}, tgt[4] = {1, 0, 0, 0};
    float grad[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    const float ref[4] = {0.1f, 0.2f, 0.3f, 0.4f};

    EXPECT_FLOAT_EQ(nimcp_anti_collapse_diversity_loss_targeted(NULL, out, tgt, 4, grad, 4), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_anti_collapse_diversity_loss_targeted(&st, NULL, tgt, 4, grad, 4), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_anti_collapse_diversity_loss_targeted(&st, out, tgt, 4, grad, 0), 0.0f);

    st.config.diversity_loss_weight = 0.0f;
    nimcp_anti_collapse_diversity_loss_targeted(&st, out, tgt, 4, grad, 4);
    EXPECT_FLOAT_EQ(nimcp_anti_collapse_diversity_loss_targeted(&st, out, tgt, 4, grad, 4), 0.0f);

    for (int i = 0; i < 4; i++) EXPECT_FLOAT_EQ(grad[i], ref[i]);
}

/* 2. First call has no history: adds nothing but stores output + target */
TEST_F(DiversityTargetedTest, FirstCallAddsNothingButStores) {
    float out[4] = {1, 2, 3, 4}, tgt[4] = {4, 3, 2, 1};
    float grad[4] = {0.1f, 0.1f, 0.1f, 0.1f};

    float loss = nimcp_anti_collapse_diversity_loss_targeted(&st, out, tgt, 4, grad, 4);
    EXPECT_FLOAT_EQ(loss, 0.0f);
    for (int i = 0; i < 4; i++) EXPECT_FLOAT_EQ(grad[i], 0.1f);

    EXPECT_EQ(st.buffer_count, 1u);
    ASSERT_NE(st.diversity_buffer, nullptr);
    ASSERT_NE(st.target_buffer, nullptr);
    EXPECT_EQ(st.output_dim, 4u);
    EXPECT_EQ(st.target_dim, 4u);
    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(st.diversity_buffer[i], out[i]);
        EXPECT_FLOAT_EQ(st.target_buffer[i], tgt[i]);
    }
}

/* 3. Outputs no more alike than targets: no push */
TEST_F(DiversityTargetedTest, NoPushWhenOutputsNoMoreAlikeThanTargets) {
    /* Targets identical (cos 1); outputs cos < 1 */
    float tgt[4] = {1, 1, 0, 0};
    float o1[4] = {1, 0, 0, 0}, o2[4] = {1, 1, 0, 0};
    float g1[4] = {0.1f, 0.1f, 0.1f, 0.1f};
    nimcp_anti_collapse_diversity_loss_targeted(&st, o1, tgt, 4, g1, 4);

    float g2[4] = {0.1f, -0.2f, 0.3f, -0.4f};
    const float ref[4] = {0.1f, -0.2f, 0.3f, -0.4f};
    float loss = nimcp_anti_collapse_diversity_loss_targeted(&st, o2, tgt, 4, g2, 4);
    EXPECT_FLOAT_EQ(loss, 0.0f);
    for (int i = 0; i < 4; i++) EXPECT_FLOAT_EQ(g2[i], ref[i]);
}

/* 4. Outputs more alike than targets (collapse): push and positive loss */
TEST_F(DiversityTargetedTest, PushWhenOutputsMoreAlikeThanTargets) {
    /* Orthogonal targets (cos 0); nearly identical outputs (cos ~1) */
    float t1[4] = {1, 0, 0, 0}, t2[4] = {0, 1, 0, 0};
    float o1[4] = {1, 1, 1, 1}, o2[4] = {1, 1, 1, 1.1f};
    float g1[4] = {0.1f, 0.1f, 0.1f, 0.1f};
    nimcp_anti_collapse_diversity_loss_targeted(&st, o1, t1, 4, g1, 4);

    float g2[4] = {0.1f, -0.2f, 0.3f, -0.4f};
    const float ref[4] = {0.1f, -0.2f, 0.3f, -0.4f};
    float loss = nimcp_anti_collapse_diversity_loss_targeted(&st, o2, t2, 4, g2, 4);
    EXPECT_GT(loss, 0.0f);
    float changed = 0.0f;
    for (int i = 0; i < 4; i++) changed += std::fabs(g2[i] - ref[i]);
    EXPECT_GT(changed, 0.0f);
}

/* 5. Added gradient never exceeds weight * ||task gradient||, even for tiny outputs */
TEST_F(DiversityTargetedTest, AddedGradientBoundedByTaskGradient) {
    const uint32_t D = 64;
    std::mt19937 rng(7);
    std::normal_distribution<float> n01(0.0f, 1.0f);

    for (int trial = 0; trial < 20; trial++) {
        std::vector<float> prev(D), out(D), t1(D, 0.0f), t2(D, 0.0f);
        for (uint32_t j = 0; j < D; j++) {
            prev[j] = 1e-4f * (1.0f + 0.01f * n01(rng));   /* tiny, collapsed */
            out[j]  = 1e-4f * (1.0f + 0.01f * n01(rng));
        }
        t1[trial % D] = 1.0f;                   /* orthogonal targets */
        t2[(trial + 1) % D] = 1.0f;

        nimcp_anti_collapse_destroy(&st);
        ASSERT_EQ(nimcp_anti_collapse_init(&st, &cfg), 0);
        std::vector<float> gp(D, 0.0f);
        nimcp_anti_collapse_diversity_loss_targeted(&st, prev.data(), t1.data(), D, gp.data(), D);

        std::vector<float> task(D);
        for (uint32_t j = 0; j < D; j++) task[j] = 1e-5f * n01(rng);
        std::vector<float> g = task;
        nimcp_anti_collapse_diversity_loss_targeted(&st, out.data(), t2.data(), D, g.data(), D);

        float added = l2(diff(g, task));
        EXPECT_GT(added, 0.0f) << "trial " << trial;
        EXPECT_LE(added, 0.5f * l2(task) * 1.001f) << "trial " << trial;
    }
}

/* 6. Zero task gradient: nothing added */
TEST_F(DiversityTargetedTest, ZeroTaskGradientAddsNothing) {
    float t1[4] = {1, 0, 0, 0}, t2[4] = {0, 1, 0, 0};
    float o1[4] = {1, 1, 1, 1}, o2[4] = {1, 1, 1, 1.1f};
    float g1[4] = {0, 0, 0, 0};
    nimcp_anti_collapse_diversity_loss_targeted(&st, o1, t1, 4, g1, 4);

    float g2[4] = {0, 0, 0, 0};
    float loss = nimcp_anti_collapse_diversity_loss_targeted(&st, o2, t2, 4, g2, 4);
    EXPECT_GT(loss, 0.0f);   /* collapse is still reported */
    for (int i = 0; i < 4; i++) EXPECT_FLOAT_EQ(g2[i], 0.0f);
}

/* 7. No target: still pushes, still bounded */
TEST_F(DiversityTargetedTest, NullTargetStillBounded) {
    float o1[4] = {1, 1, 1, 1}, o2[4] = {1, 1, 1, 1.1f};
    float g1[4] = {0.01f, 0.01f, 0.01f, 0.01f};
    nimcp_anti_collapse_diversity_loss_targeted(&st, o1, NULL, 0, g1, 4);
    EXPECT_EQ(st.target_buffer, nullptr);

    std::vector<float> task = {1e-3f, -2e-3f, 1e-3f, 0.0f};
    std::vector<float> g = task;
    float loss = nimcp_anti_collapse_diversity_loss_targeted(&st, o2, NULL, 0, g.data(), 4);
    EXPECT_GT(loss, 0.0f);
    float added = l2(diff(g, task));
    EXPECT_GT(added, 0.0f);
    EXPECT_LE(added, 0.5f * l2(task) * 1.001f);
}

/* 8. The bug: a linear readout trained with Adam (as UTM does) on MSE +
 *    diversity. Inputs are temporally correlated (like the LNN state) and
 *    targets are not, so consecutive outputs are more alike than the targets.
 *    The legacy term inflates the output ~10x past target scale (matches the
 *    live LNN readout drift 0.03 -> 0.30); the targeted term cuts that by
 *    more than half. Adam noise alone (no diversity) also sits above target
 *    scale here, so "near target" is not asserted. */
namespace {
float train_readout(int mode /* 0 none, 1 legacy, 2 targeted */) {
    const uint32_t IN = 64, OUT = 256, STEPS = 3000, TAIL = 200;
    const float rho = 0.9f, lr = 1e-3f;
    std::mt19937 rng(1);
    std::normal_distribution<float> n01(0.0f, 1.0f);

    std::vector<float> A(OUT * IN), W(OUT * IN), m(OUT * IN, 0.0f), v(OUT * IN, 0.0f);
    for (auto& a : A) a = 0.03f / std::sqrt((float)IN) * n01(rng);
    for (auto& w : W) w = 1e-3f * n01(rng);

    nimcp_anti_collapse_config_t c;
    std::memset(&c, 0, sizeof(c));
    c.diversity_loss_weight = 0.5f;
    c.diversity_buffer_size = 32;
    nimcp_anti_collapse_state_t s;
    nimcp_anti_collapse_init(&s, &c);

    std::vector<float> x(IN), z(IN), y(OUT), t(OUT), g(OUT);
    for (auto& e : x) e = n01(rng);
    double tail_rms = 0.0;
    float b1t = 1.0f, b2t = 1.0f;
    for (uint32_t step = 0; step < STEPS; step++) {
        for (uint32_t i = 0; i < IN; i++) {
            x[i] = rho * x[i] + std::sqrt(1.0f - rho * rho) * n01(rng);
            z[i] = n01(rng);
        }
        for (uint32_t o = 0; o < OUT; o++) {
            float yo = 0.0f, to = 0.0f;
            for (uint32_t i = 0; i < IN; i++) {
                yo += W[o * IN + i] * x[i];
                to += A[o * IN + i] * z[i];
            }
            y[o] = yo;
            t[o] = to;
        }
        for (uint32_t o = 0; o < OUT; o++) g[o] = 2.0f * (y[o] - t[o]) / (float)OUT;
        if (mode == 1) {
            nimcp_anti_collapse_diversity_loss(&s, y.data(), g.data(), OUT);
        } else if (mode == 2) {
            nimcp_anti_collapse_diversity_loss_targeted(&s, y.data(), t.data(), OUT,
                                                        g.data(), OUT);
        }
        /* Weight gradient g x^T, normalized to unit norm (UTM normalizes
         * gradients before the optimizer), then Adam. */
        const float gn = l2(g) * l2(x);
        if (gn <= 0.0f) continue;
        b1t *= 0.9f;
        b2t *= 0.999f;
        for (uint32_t o = 0; o < OUT; o++) {
            for (uint32_t i = 0; i < IN; i++) {
                const size_t k = (size_t)o * IN + i;
                const float G = g[o] * x[i] / gn;
                m[k] = 0.9f * m[k] + 0.1f * G;
                v[k] = 0.999f * v[k] + 0.001f * G * G;
                W[k] -= lr * (m[k] / (1.0f - b1t)) / (std::sqrt(v[k] / (1.0f - b2t)) + 1e-8f);
            }
        }
        if (step >= STEPS - TAIL) tail_rms += rms(y);
    }
    nimcp_anti_collapse_destroy(&s);
    return (float)(tail_rms / TAIL);
}
}  // namespace

TEST(DiversityTargetedTraining, TargetedStopsReadoutInflation) {
    const float target_rms = 0.03f;
    const float none = train_readout(0);
    const float legacy = train_readout(1);
    const float fixed = train_readout(2);
    std::printf("readout rms: none %.4f  legacy %.4f  targeted %.4f  (target ~%.2f)\n",
                none, legacy, fixed, target_rms);
    EXPECT_GT(legacy, 5.0f * target_rms);   /* reproduces the bug */
    EXPECT_LT(fixed, 0.5f * legacy);        /* fix removes most of it */
}

/* 9. Dimension change restarts history; destroy frees both buffers */
TEST_F(DiversityTargetedTest, DimChangeRestartsAndDestroyFrees) {
    float o4[4] = {1, 2, 3, 4}, t4[4] = {1, 0, 0, 0};
    float g4[4] = {0.1f, 0.1f, 0.1f, 0.1f};
    nimcp_anti_collapse_diversity_loss_targeted(&st, o4, t4, 4, g4, 4);
    nimcp_anti_collapse_diversity_loss_targeted(&st, o4, t4, 4, g4, 4);
    EXPECT_EQ(st.buffer_count, 2u);

    /* Output dim change */
    float o6[6] = {1, 2, 3, 4, 5, 6}, g6[6] = {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};
    float loss = nimcp_anti_collapse_diversity_loss_targeted(&st, o6, t4, 4, g6, 6);
    EXPECT_FLOAT_EQ(loss, 0.0f);
    EXPECT_EQ(st.buffer_count, 1u);
    EXPECT_EQ(st.output_dim, 6u);

    /* Target dim change */
    float t3[3] = {0, 1, 0};
    loss = nimcp_anti_collapse_diversity_loss_targeted(&st, o6, t3, 3, g6, 6);
    EXPECT_FLOAT_EQ(loss, 0.0f);
    EXPECT_EQ(st.buffer_count, 1u);
    EXPECT_EQ(st.target_dim, 3u);

    nimcp_anti_collapse_destroy(&st);
    EXPECT_EQ(st.diversity_buffer, nullptr);
    EXPECT_EQ(st.target_buffer, nullptr);
    EXPECT_EQ(st.target_dim, 0u);
    /* TearDown destroys again: must be safe */
}

}  // namespace
