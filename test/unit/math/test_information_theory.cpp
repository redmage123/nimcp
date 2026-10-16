/**
 * @file test_information_theory.cpp
 * @brief Tests for the information theory engine (Google Test)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/math/nimcp_information_theory.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double TOL = 1e-6;

/* ---------- lifecycle ---------- */

TEST(InformationTheoryTest, CreateDestroy) {
    info_theory_t *it = it_create();
    ASSERT_NE(it, nullptr);
    it_destroy(it);
}

/* ---------- entropy of fair coin = 1 bit ---------- */

TEST(InformationTheoryTest, EntropyFairCoin) {
    it_distribution_t d = {0};
    d.size = 2;
    d.prob[0] = 0.5;
    d.prob[1] = 0.5;

    double h = it_shannon_entropy(&d);
    EXPECT_NEAR(h, 1.0, TOL);
}

TEST(InformationTheoryTest, EntropyCertainOutcome) {
    it_distribution_t d = {0};
    d.size = 2;
    d.prob[0] = 1.0;
    d.prob[1] = 0.0;

    double h = it_shannon_entropy(&d);
    EXPECT_NEAR(h, 0.0, TOL);
}

TEST(InformationTheoryTest, EntropyUniform4) {
    /* H(uniform over 4 symbols) = log2(4) = 2 bits */
    it_distribution_t d = {0};
    d.size = 4;
    d.prob[0] = 0.25;
    d.prob[1] = 0.25;
    d.prob[2] = 0.25;
    d.prob[3] = 0.25;

    double h = it_shannon_entropy(&d);
    EXPECT_NEAR(h, 2.0, TOL);
}

/* ---------- KL divergence ---------- */

TEST(InformationTheoryTest, KlDivergenceSame) {
    /* KL(P||P) = 0 */
    it_distribution_t p = {0};
    p.size = 3;
    p.prob[0] = 0.2;
    p.prob[1] = 0.3;
    p.prob[2] = 0.5;

    double kl = it_kl_divergence(&p, &p);
    EXPECT_NEAR(kl, 0.0, TOL);
}

TEST(InformationTheoryTest, KlDivergencePositive) {
    /* KL(P||Q) >= 0 (Gibbs inequality) */
    it_distribution_t p = {0}, q = {0};
    p.size = 2; q.size = 2;
    p.prob[0] = 0.7; p.prob[1] = 0.3;
    q.prob[0] = 0.4; q.prob[1] = 0.6;

    double kl = it_kl_divergence(&p, &q);
    EXPECT_GT(kl, 0.0);
}

/* ---------- mutual information ---------- */

TEST(InformationTheoryTest, MutualInfoSelf) {
    /* I(X;X) = H(X) */
    it_distribution_t px = {0};
    px.size = 3;
    px.prob[0] = 0.2;
    px.prob[1] = 0.3;
    px.prob[2] = 0.5;

    double hx = it_shannon_entropy(&px);

    /* Build joint distribution where Y=X deterministically */
    it_joint_distribution_t joint;
    memset(&joint, 0, sizeof(joint));
    joint.size_x = 3;
    joint.size_y = 3;
    joint.prob[0][0] = 0.2;
    joint.prob[1][1] = 0.3;
    joint.prob[2][2] = 0.5;

    double mi = it_mutual_information(&joint);
    EXPECT_NEAR(mi, hx, TOL);
}

/* ---------- BSC capacity ---------- */

TEST(InformationTheoryTest, BscCapacity) {
    /* C_BSC = 1 - H(p) */
    /* p=0: perfect channel, capacity=1 */
    EXPECT_NEAR(it_capacity_bsc(0.0), 1.0, TOL);

    /* p=0.5: useless channel, capacity=0 */
    EXPECT_NEAR(it_capacity_bsc(0.5), 0.0, TOL);

    /* p=0.1: capacity = 1 - H(0.1) */
    double h01 = -(0.1 * log2(0.1) + 0.9 * log2(0.9));
    EXPECT_NEAR(it_capacity_bsc(0.1), 1.0 - h01, TOL);
}

/* ---------- BEC capacity ---------- */

TEST(InformationTheoryTest, BecCapacity) {
    /* C_BEC = 1 - epsilon */
    EXPECT_NEAR(it_capacity_bec(0.0), 1.0, TOL);
    EXPECT_NEAR(it_capacity_bec(0.3), 0.7, TOL);
    EXPECT_NEAR(it_capacity_bec(1.0), 0.0, TOL);
}

/* ---------- cross entropy ---------- */

TEST(InformationTheoryTest, CrossEntropy) {
    /* H(P,P) = H(P) */
    it_distribution_t p = {0};
    p.size = 2;
    p.prob[0] = 0.7;
    p.prob[1] = 0.3;

    double hp = it_shannon_entropy(&p);
    double ce = it_cross_entropy(&p, &p);
    EXPECT_NEAR(ce, hp, TOL);
}

/* ---------- differential entropy ---------- */

TEST(InformationTheoryTest, DifferentialEntropyGaussian) {
    /* h(X) = 0.5 * log2(2*pi*e*var) */
    double var = 1.0;
    double expected = 0.5 * log2(2.0 * M_PI * exp(1.0) * var);
    double h = it_differential_entropy_gaussian(var);
    EXPECT_NEAR(h, expected, TOL);
}

/* ---------- validate distribution ---------- */

TEST(InformationTheoryTest, ValidateDistribution) {
    it_distribution_t good = {0};
    good.size = 3;
    good.prob[0] = 0.2;
    good.prob[1] = 0.3;
    good.prob[2] = 0.5;
    EXPECT_TRUE(it_validate_distribution(&good));

    it_distribution_t bad = {0};
    bad.size = 2;
    bad.prob[0] = 0.3;
    bad.prob[1] = 0.3;  /* sums to 0.6, not 1.0 */
    EXPECT_FALSE(it_validate_distribution(&bad));
}

/* ---------- AWGN capacity ---------- */

TEST(InformationTheoryTest, AwgnCapacity) {
    /* C_AWGN = 0.5 * log2(1 + SNR) */
    EXPECT_NEAR(it_capacity_awgn(0.0), 0.0, TOL);
    EXPECT_NEAR(it_capacity_awgn(1.0), 0.5 * log2(2.0), TOL);
    EXPECT_NEAR(it_capacity_awgn(3.0), 0.5 * log2(4.0), TOL);  /* = 1.0 */
}
