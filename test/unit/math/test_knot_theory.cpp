/**
 * @file test_knot_theory.cpp
 * @brief Tests for the knot theory engine (Google Test)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/math/nimcp_knot_theory.h"
}

/* ---------- fixture ---------- */

class KnotTheoryFixture : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = knot_theory_default_config();
        eng = knot_theory_create(&cfg);
        ASSERT_NE(eng, nullptr);
    }
    void TearDown() override {
        knot_theory_destroy(eng);
    }
    knot_config_t cfg;
    knot_theory_engine_t *eng;
};

/* ---------- lifecycle ---------- */

TEST(KnotTheoryTest, CreateDestroy) {
    knot_config_t cfg = knot_theory_default_config();
    knot_theory_engine_t *eng = knot_theory_create(&cfg);
    ASSERT_NE(eng, nullptr);
    knot_theory_destroy(eng);
}

/* ---------- load known knots ---------- */

TEST_F(KnotTheoryFixture, LoadUnknot) {
    uint32_t id = knot_load_known(eng, KNOT_UNKNOT);
    EXPECT_TRUE(knot_is_unknot(eng, id));
}

TEST_F(KnotTheoryFixture, LoadTrefoil) {
    uint32_t id = knot_load_known(eng, KNOT_TREFOIL);
    EXPECT_FALSE(knot_is_unknot(eng, id));
}

/* ---------- writhe ---------- */

TEST_F(KnotTheoryFixture, UnknotWritheZero) {
    uint32_t id = knot_load_known(eng, KNOT_UNKNOT);
    int32_t w = knot_writhe(&eng->diagrams[id]);
    EXPECT_EQ(w, 0);
}

TEST_F(KnotTheoryFixture, TrefoilWrithe) {
    uint32_t id = knot_load_known(eng, KNOT_TREFOIL);
    int32_t w = knot_writhe(&eng->diagrams[id]);
    /* Trefoil writhe is +3 or -3 depending on orientation */
    EXPECT_TRUE(w == 3 || w == -3);
}

/* ---------- Alexander polynomial ---------- */

TEST(KnotTheoryTest, TrefoilAlexander) {
    knot_config_t cfg = knot_theory_default_config();
    cfg.compute_alexander = true;
    knot_theory_engine_t *eng = knot_theory_create(&cfg);
    ASSERT_NE(eng, nullptr);

    uint32_t id = knot_load_known(eng, KNOT_TREFOIL);
    knot_polynomial_t alex = knot_alexander_polynomial(eng, id);

    /* Trefoil Alexander: Delta(t) = t - 1 + t^{-1} (or equivalent)
       Evaluate at t=1: Delta(1) = 1 (always for knots) */
    double val_at_1 = knot_poly_eval(&alex, 1.0);
    EXPECT_NEAR(val_at_1, 1.0, 1e-6);

    /* Evaluate at t=-1: |Delta(-1)| = determinant = 3 for trefoil */
    double val_at_neg1 = knot_poly_eval(&alex, -1.0);
    EXPECT_NEAR(fabs(val_at_neg1), 3.0, 1e-6);

    knot_theory_destroy(eng);
}

/* ---------- from Gauss code ---------- */

TEST_F(KnotTheoryFixture, GaussCodeCreation) {
    /* Trefoil Gauss code: 1 -2 3 -1 2 -3 */
    int32_t gauss[] = {1, -2, 3, -1, 2, -3};
    uint32_t id = knot_from_gauss_code(eng, gauss, 6, "trefoil_gauss");

    EXPECT_EQ(eng->diagrams[id].num_crossings, 3u);
}

/* ---------- braid operations ---------- */

TEST(KnotTheoryTest, BraidComposeLength) {
    /* Braid a: sigma1, sigma2 (length 2)
       Braid b: sigma1^-1 (length 1)
       Compose: length = 2 + 1 = 3 */
    knot_braid_t a = {{0}, 0, 3};
    a.generators[0] = 1;
    a.generators[1] = 2;
    a.length = 2;

    knot_braid_t b = {{0}, 0, 3};
    b.generators[0] = -1;
    b.length = 1;

    knot_braid_t ab = knot_braid_compose(&a, &b);
    EXPECT_EQ(ab.length, 3u);
    EXPECT_EQ(knot_braid_length(&ab), 3u);
    EXPECT_EQ(ab.generators[0], 1);
    EXPECT_EQ(ab.generators[1], 2);
    EXPECT_EQ(ab.generators[2], -1);
}

/* ---------- braid inverse ---------- */

TEST(KnotTheoryTest, BraidInverse) {
    knot_braid_t b = {{0}, 0, 3};
    b.generators[0] = 1;
    b.generators[1] = -2;
    b.generators[2] = 1;
    b.length = 3;

    knot_braid_t inv = knot_braid_inverse(&b);
    EXPECT_EQ(inv.length, 3u);
    /* Inverse reverses order and negates each generator */
    EXPECT_EQ(inv.generators[0], -1);
    EXPECT_EQ(inv.generators[1], 2);
    EXPECT_EQ(inv.generators[2], -1);
}

/* ---------- polynomial operations ---------- */

TEST(KnotTheoryTest, PolynomialEval) {
    knot_polynomial_t p;
    memset(&p, 0, sizeof(p));
    /* 1 + 2t + 3t^2, min_exp=0 */
    p.min_exp = 0;
    p.num_terms = 3;
    p.coeffs[0] = 1.0;
    p.coeffs[1] = 2.0;
    p.coeffs[2] = 3.0;

    /* At t=2: 1 + 4 + 12 = 17 */
    EXPECT_NEAR(knot_poly_eval(&p, 2.0), 17.0, 1e-9);
    EXPECT_NEAR(knot_poly_eval(&p, 0.0), 1.0, 1e-9);
}

/* ---------- invariants ---------- */

TEST(KnotTheoryTest, FigureEightInvariants) {
    knot_config_t cfg = knot_theory_default_config();
    cfg.compute_alexander = true;
    knot_theory_engine_t *eng = knot_theory_create(&cfg);
    ASSERT_NE(eng, nullptr);

    uint32_t id = knot_load_known(eng, KNOT_FIGURE_8);
    knot_invariants_t inv = knot_compute_invariants(eng, id);

    /* Figure-8 is alternating and amphicheiral */
    EXPECT_TRUE(inv.is_alternating);
    /* Crossing number = 4 */
    EXPECT_EQ(inv.crossing_number, 4u);
    /* Writhe of standard figure-8 diagram is 0 */
    EXPECT_EQ(inv.writhe, 0);

    knot_theory_destroy(eng);
}
