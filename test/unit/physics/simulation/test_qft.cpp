/**
 * @file test_qft.cpp
 * @brief Tests for the QFT Engine -- quantum field theory, Standard Model, RMT
 *
 * Verifies: alpha_s(M_Z), asymptotic freedom, Higgs mechanism masses,
 * Goldstone theorem, RMT eigenvalue generation, lattice Metropolis.
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "cognitive/physics/nimcp_qft.h"
}

/* ---- Fixture ---- */

class QFTTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = qft_default_config();
        sim = qft_create(&cfg);
    }
    void TearDown() override {
        if (sim) qft_destroy(sim);
    }
    qft_config_t cfg;
    qft_sim_t* sim;
};

/* ---- Tests ---- */

TEST(QFTBasic, CreateDestroy) {
    qft_config_t cfg = qft_default_config();
    qft_sim_t* sim = qft_create(&cfg);
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
    qft_destroy(sim);
}

TEST(QFTCoupling, AlphaSAtMZ) {
    /* alpha_s(M_Z = 91.2 GeV) ~ 0.118 */
    float alpha_s = qft_alpha_s(91.2f);
    EXPECT_NEAR(alpha_s, 0.118f, 0.015f);
}

TEST(QFTCoupling, AsymptoticFreedom) {
    /* alpha_s should DECREASE at higher energies (asymptotic freedom) */
    float alpha_low  = qft_alpha_s(1.0f);    /* 1 GeV */
    float alpha_high = qft_alpha_s(91.2f);   /* M_Z */

    EXPECT_GT(alpha_low, 0.0f);
    EXPECT_GT(alpha_high, 0.0f);
    EXPECT_GT(alpha_low, alpha_high);  /* stronger at lower energy */
}

TEST(QFTCoupling, AlphaEMRunning) {
    /* alpha_EM increases at higher energies (anti-screening) */
    float a_low  = qft_alpha_em(1.0f);     /* ~1/137 at low energy */
    float a_high = qft_alpha_em(91.2f);    /* ~1/128 at M_Z */

    EXPECT_GT(a_low, 0.0f);
    EXPECT_GT(a_high, 0.0f);
    EXPECT_GT(a_high, a_low);  /* alpha grows with energy */
}

TEST_F(QFTTest, HiggsMechanismMasses) {
    /* v = 246 GeV, gives M_W ~ 80 GeV, M_Z ~ 91 GeV */
    qft_higgs_mechanism(sim, 246.0f, 0.13f);

    EXPECT_NEAR(sim->higgs.W_mass, 80.4f, 3.0f);
    EXPECT_NEAR(sim->higgs.Z_mass, 91.2f, 3.0f);
    EXPECT_TRUE(sim->higgs.broken);
    EXPECT_GT(sim->higgs.higgs_mass, 0.0f);
}

TEST_F(QFTTest, HiggsWeinbergAngle) {
    qft_higgs_mechanism(sim, 246.0f, 0.13f);

    /* sin^2(theta_W) ~ 0.231 */
    EXPECT_NEAR(sim->higgs.weinberg_angle, 0.231f, 0.03f);
}

TEST(QFTGoldstone, Count) {
    /* SU(2) x U(1) -> U(1)_EM
     * dim(SU(2)xU(1)) = 3+1 = 4, dim(U(1)) = 1
     * Goldstone bosons = 4 - 1 = 3 (eaten by W+, W-, Z) */
    uint32_t count = qft_goldstone_count(4, 1);
    EXPECT_EQ(count, 3u);
}

TEST(QFTGoldstone, NoBreaking) {
    /* If group dims are equal, no breaking => 0 Goldstones */
    uint32_t count = qft_goldstone_count(3, 3);
    EXPECT_EQ(count, 0u);
}

TEST(QFTBeta, Beta0QCD) {
    /* QCD: b0 = -(11*N_c - 2*N_f) / (48*pi^2)
     * For N_c=3, N_f=6: 11*3 - 2*6 = 33 - 12 = 21
     * The beta_0 should be negative (asymptotic freedom) */
    float b0 = qft_beta_0_sun(3, 6);
    EXPECT_LT(b0, 0.0f);  /* negative => asymptotic freedom */
}

TEST(QFTRMT, GenerateEigenvalues) {
    qft_config_t cfg = qft_default_config();
    cfg.rmt_matrix_dim = 16;
    cfg.rmt_samples = 1;
    qft_sim_t* sim = qft_create(&cfg);
    ASSERT_NE(sim, nullptr);

    qft_rmt_generate(sim, QFT_RMT_GUE);
    qft_rmt_compute_eigenvalues(sim);

    /* Should have eigenvalues */
    ASSERT_NE(sim->rmt.eigenvalues, nullptr);
    EXPECT_EQ(sim->rmt.matrix_dim, 16u);

    /* Eigenvalues should be real numbers (not NaN) */
    for (uint32_t i = 0; i < sim->rmt.matrix_dim; i++) {
        EXPECT_FALSE(std::isnan(sim->rmt.eigenvalues[i]));
    }

    /* Eigenvalues should be sorted */
    for (uint32_t i = 1; i < sim->rmt.matrix_dim; i++) {
        EXPECT_GE(sim->rmt.eigenvalues[i], sim->rmt.eigenvalues[i - 1]);
    }

    qft_destroy(sim);
}

TEST(QFTRMT, WignerSurmise) {
    /* GUE Wigner surmise at s=1: p(1) = (32/pi^2)*1^2*exp(-4/pi)
     * = 32/pi^2 * exp(-4/pi) ~ 3.243 * 0.2790 ~ 0.905 */
    double p_gue = qft_rmt_wigner_surmise(QFT_RMT_GUE, 1.0);
    EXPECT_GT(p_gue, 0.0);
    double expected = (32.0 / (M_PI * M_PI)) * exp(-4.0 / M_PI);
    EXPECT_NEAR(p_gue, expected, 0.1);
}

TEST(QFTLattice, Metropolis) {
    qft_config_t cfg = qft_default_config();
    cfg.lattice_L = 4;
    cfg.lattice_dim = 2;
    qft_sim_t* sim = qft_create(&cfg);
    ASSERT_NE(sim, nullptr);

    qft_lattice_init(sim, true);  /* hot start */

    int rc = qft_lattice_metropolis_sweep(sim);
    EXPECT_EQ(rc, 0);

    qft_lattice_measure(sim);
    /* Action should be finite */
    EXPECT_FALSE(std::isnan(sim->lattice.action));
    EXPECT_FALSE(std::isinf(sim->lattice.action));

    qft_destroy(sim);
}

TEST_F(QFTTest, LoadStandardModel) {
    qft_load_standard_model(sim);
    /* Should have fields (quarks, leptons, gauge bosons, Higgs) */
    EXPECT_GT(sim->num_fields, 0u);
    /* Should have couplings */
    EXPECT_GT(sim->num_couplings, 0u);
}

TEST(QFTCoupling, RunCoupling) {
    /* Run a coupling g from mu1 to mu2 with positive beta_0 (QED-like).
     * g should grow at higher scale. */
    float g_low = 0.3f;
    float b0 = 1.0f / (12.0f * (float)M_PI * (float)M_PI);  /* positive */
    float g_high = qft_run_coupling(g_low, 1.0f, 100.0f, b0);
    EXPECT_GT(g_high, g_low);  /* coupling grows for positive beta */
}
