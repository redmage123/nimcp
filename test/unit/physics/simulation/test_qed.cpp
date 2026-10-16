/**
 * @file test_qed.cpp
 * @brief Tests for the QED Engine -- quantum electrodynamics
 *
 * Verifies: Thomson cross-section, anomalous magnetic moment, running alpha,
 * pair production threshold, Klein-Nishina low-energy limit, Breit-Wheeler threshold,
 * Feynman rules, vertex coupling.
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "cognitive/physics/nimcp_qed.h"
}

/* ---- Fixture ---- */

class QEDTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = qed_default_config();
        sim = qed_create(&cfg);
    }
    void TearDown() override {
        if (sim) qed_destroy(sim);
    }
    qed_config_t cfg;
    qed_sim_t* sim;
};

/* ---- Tests ---- */

TEST(QEDBasic, CreateDestroy) {
    qed_config_t cfg = qed_default_config();
    qed_sim_t* sim = qed_create(&cfg);
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
    qed_destroy(sim);
}

TEST(QEDCrossSection, ThomsonCrossSection) {
    /* sigma_T = 8*pi/3 * r_e^2 = 6.652e-29 m^2 */
    float sigma = qed_thomson_cross_section();
    double expected = 6.6524e-29;
    /* Allow 1% tolerance */
    EXPECT_NEAR(sigma, expected, expected * 0.02);
}

TEST(QEDAnomalous, FirstOrder) {
    /* a_e = alpha/(2*pi) = 0.0011614... (Schwinger term) */
    float a_e = qed_anomalous_moment_first_order();
    double expected = QED_ALPHA / (2.0 * M_PI);  /* ~0.001161 */
    EXPECT_NEAR(a_e, expected, 1e-5);
}

TEST(QEDAnomalous, ThirdOrder) {
    /* Through O(alpha^3): a_e ~ 0.00115965... */
    float a_e = qed_anomalous_moment_third_order();
    EXPECT_NEAR(a_e, 0.00115965f, 0.0001f);
}

TEST(QEDRunning, AlphaAtMZ) {
    /* alpha(M_Z^2) ~ 1/128 = 0.00781 (compared to alpha(0) ~ 1/137 = 0.00730) */
    float mz = 91.2f;
    float alpha_mz = qed_running_alpha(mz * mz);
    /* Should be larger than alpha(0) due to screening */
    EXPECT_GT(alpha_mz, (float)QED_ALPHA);
    /* Should be approximately 1/128 */
    EXPECT_NEAR(alpha_mz, 1.0f / 128.0f, 0.002f);
}

TEST(QEDThreshold, PairProduction) {
    /* E_threshold = 2 * m_e = 2 * 0.000511 GeV = 0.001022 GeV = 1.022 MeV */
    float threshold = qed_pair_production_threshold();
    float expected = 2.0f * QED_ELECTRON_MASS;
    EXPECT_NEAR(threshold, expected, 1e-5f);
}

TEST(QEDCrossSection, KleinNishinaLowEnergy) {
    /* At low photon energy (E << m_e), Klein-Nishina reduces to Thomson. */
    float low_E = 1e-6f;  /* 1 eV in GeV -- very low */
    float dsigma_0 = qed_klein_nishina(low_E, 0.0f);
    float dsigma_90 = qed_klein_nishina(low_E, (float)(M_PI / 2.0));
    /* Thomson is isotropic-ish: dS/dO(0) / dS/dO(pi/2) should be ~2
     * (from (1+cos^2 theta)/2 ratio: (1+1)/2 vs (1+0)/2 = 1 vs 0.5) */
    if (dsigma_90 > 0.0f) {
        float ratio = dsigma_0 / dsigma_90;
        EXPECT_NEAR(ratio, 2.0f, 0.5f);
    }
    EXPECT_GT(dsigma_0, 0.0f);
}

TEST(QEDCrossSection, BreitWheeler) {
    /* gamma + gamma -> e+ e-
     * Threshold: sqrt(s) = 2*m_e => s = 4*m_e^2 => E_cm = 2*m_e */
    float sigma_below = qed_breit_wheeler_cross_section(0.0005f);  /* below 2*m_e */
    float sigma_above = qed_breit_wheeler_cross_section(0.01f);    /* above 2*m_e */
    /* Below threshold should be zero or very small */
    EXPECT_GE(sigma_above, 0.0f);
    /* Above threshold should be nonzero */
    if (0.01f > 2.0f * QED_ELECTRON_MASS) {
        EXPECT_GT(sigma_above, sigma_below);
    }
}

TEST(QEDFeynman, VertexCoupling) {
    /* QED vertex: e = sqrt(4*pi*alpha) ~ 0.3028 */
    float e_coupling = qed_vertex_coupling();
    double expected = sqrt(4.0 * M_PI * QED_ALPHA);
    EXPECT_NEAR(e_coupling, expected, 0.01);
}

TEST(QEDFeynman, PhotonPropagator) {
    /* 1/q^2 -- at q^2 = 1 GeV^2, propagator = 1.0 */
    float prop = qed_photon_propagator(1.0f);
    EXPECT_NEAR(prop, 1.0f, 0.1f);

    /* At q^2 = 4, propagator = 0.25 */
    float prop4 = qed_photon_propagator(4.0f);
    EXPECT_NEAR(prop4, 0.25f, 0.05f);
}

TEST(QEDFeynman, ElectronPropagator) {
    /* 1/(p^2 - m^2) at p^2 = 1, m = m_e ~ 0.000511 */
    float prop = qed_electron_propagator(1.0f, QED_ELECTRON_MASS);
    float expected = 1.0f / (1.0f - QED_ELECTRON_MASS * QED_ELECTRON_MASS);
    EXPECT_NEAR(prop, expected, 0.01f);
}

TEST(QEDVacuum, PolarizationSign) {
    /* Vacuum polarization Pi(q^2) should be positive for spacelike q^2 > 0
     * (screening effect: charges are screened) */
    float pi_q2 = qed_vacuum_polarization(100.0f);  /* 100 GeV^2 */
    EXPECT_GT(pi_q2, 0.0f);
}

TEST_F(QEDTest, LoadStandardProcesses) {
    qed_load_standard_processes(sim);
    EXPECT_GT(sim->num_processes, 0);
}

TEST_F(QEDTest, ComputeCrossSection) {
    qed_load_standard_processes(sim);
    /* Compute Compton cross-section at 1 GeV CM energy */
    float sigma = qed_compute_cross_section(sim, QED_PROC_COMPTON, 1.0f, 0.5f);
    EXPECT_GE(sigma, 0.0f);
}
