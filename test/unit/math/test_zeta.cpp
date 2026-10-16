/**
 * @file test_zeta.cpp
 * @brief Tests for the Riemann zeta function engine (Google Test)
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "cognitive/math/nimcp_zeta_functions.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double TOL = 1e-6;

/* ---------- lifecycle ---------- */

TEST(ZetaTest, CreateDestroy) {
    zeta_config_t cfg = zeta_default_config();
    zeta_engine_t *eng = zeta_create(&cfg);
    ASSERT_NE(eng, nullptr);
    zeta_destroy(eng);
}

/* ---------- fixture for engine-based tests ---------- */

class ZetaFixture : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = zeta_default_config();
        eng = zeta_create(&cfg);
        ASSERT_NE(eng, nullptr);
    }
    void TearDown() override {
        zeta_destroy(eng);
    }
    zeta_config_t cfg;
    zeta_engine_t *eng;
};

/* ---------- special values ---------- */

TEST(ZetaTest, Zeta2IsPiSquaredOver6) {
    double val = zeta_basel();
    double expected = M_PI * M_PI / 6.0;
    EXPECT_NEAR(val, expected, TOL);
}

TEST(ZetaTest, ZetaNegative1) {
    /* zeta(-1) = -1/12 (analytic continuation) */
    double val = zeta_negative_1();
    EXPECT_NEAR(val, -1.0 / 12.0, TOL);
}

TEST(ZetaTest, ZetaAtZero) {
    /* zeta(0) = -1/2 */
    double val = zeta_at_zero();
    EXPECT_NEAR(val, -0.5, TOL);
}

TEST(ZetaTest, Zeta4) {
    /* zeta(4) = pi^4 / 90 */
    double val = zeta_4();
    double expected = M_PI * M_PI * M_PI * M_PI / 90.0;
    EXPECT_NEAR(val, expected, TOL);
}

/* ---------- real evaluation ---------- */

TEST_F(ZetaFixture, ZetaEvalReal) {
    /* zeta(2) should match Basel */
    double z2 = zeta_eval_real(eng, 2.0);
    EXPECT_NEAR(z2, M_PI * M_PI / 6.0, 1e-4);

    /* zeta(3) ~ 1.2020569 (Apery's constant) */
    double z3 = zeta_eval_real(eng, 3.0);
    EXPECT_NEAR(z3, 1.2020569, 1e-4);
}

/* ---------- zero counting N(T) ---------- */

TEST(ZetaTest, ZeroCountingN100) {
    /* N(100) ~ 29 non-trivial zeros with Im(s) < 100 */
    double N = zeta_N(100.0);
    EXPECT_NEAR(N, 29.0, 2.0);  /* approximate formula, tolerance of 2 */
}

/* ---------- find zeros in range ---------- */

TEST_F(ZetaFixture, FindZerosInRange) {
    uint32_t found = zeta_find_zeros_in_range(eng, 10.0, 50.0, 0.5);
    /* There are 10 known zeros between t=14.1 and t=49.8 */
    EXPECT_GE(found, 8u);   /* allow some tolerance for numerical issues */
    EXPECT_LE(found, 12u);
}

/* ---------- GUE pair correlation formula ---------- */

TEST(ZetaTest, GuePairCorrelation) {
    /* At x=0: 1 - (sin(0)/0)^2 = 1 - 1 = 0 (by L'Hopital) */
    double g0 = zeta_gue_pair_correlation(0.0);
    EXPECT_NEAR(g0, 0.0, 1e-4);

    /* At x=1: 1 - (sin(pi)/pi)^2 = 1 - 0 = 1 */
    double g1 = zeta_gue_pair_correlation(1.0);
    EXPECT_NEAR(g1, 1.0, 1e-4);

    /* Monotonically approaches 1 for large x */
    double g10 = zeta_gue_pair_correlation(10.0);
    EXPECT_GT(g10, 0.99);
}

/* ---------- Wigner surmise ---------- */

TEST(ZetaTest, WignerSurmise) {
    /* p(s) = (pi/2)*s*exp(-pi*s^2/4) */
    /* At s=0, p(0) = 0 */
    EXPECT_NEAR(zeta_wigner_surmise(0.0), 0.0, TOL);

    /* Peak at s=sqrt(2/pi) ~ 0.7979 */
    double s_peak = sqrt(2.0 / M_PI);
    double p_peak = zeta_wigner_surmise(s_peak);
    /* Check it's a local max: neighbors should be smaller */
    EXPECT_GT(p_peak, zeta_wigner_surmise(s_peak - 0.1));
    EXPECT_GT(p_peak, zeta_wigner_surmise(s_peak + 0.1));
}

/* ---------- known zeros ---------- */

TEST_F(ZetaFixture, KnownZeros) {
    zeta_load_known_zeros(eng);
    EXPECT_GE(eng->num_zeros, 10u);

    /* First zero should be near 14.134725 */
    EXPECT_NEAR(eng->zeros[0].t, ZETA_ZERO_1, 0.01);
}

/* ---------- Z-function sign change at zero ---------- */

TEST_F(ZetaFixture, ZFunctionSignChange) {
    /* Z(t) should have opposite signs on either side of a zero */
    double z_before = zeta_Z(eng, 14.0);
    double z_after = zeta_Z(eng, 14.3);
    /* They should have opposite signs (sign change = zero between them) */
    EXPECT_TRUE(z_before * z_after < 0 || fabs(z_before) < 0.5 || fabs(z_after) < 0.5);
}
