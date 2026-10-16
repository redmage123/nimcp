/**
 * @file test_parietal_integration.cpp
 * @brief Integration tests for Parietal Lobe module
 *
 * Tests cross-submodule interactions:
 * - Number sense + Spatial reasoning (geometric estimation)
 * - Mathematical intuition + Pattern extrapolation
 * - Full orchestrator integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/parietal/nimcp_number_sense.h"
#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include "cognitive/parietal/nimcp_mathematical_intuition.h"
#include "cognitive/parietal/nimcp_equation_manipulation.h"

//=============================================================================
// Test Fixture: Number Sense + Spatial Integration
//=============================================================================

class NumberSpatialIntegrationTest : public ::testing::Test {
protected:
    number_sense_t* ns;
    spatial_reasoning_t* sr;

    void SetUp() override {
        ns = number_sense_create();
        ASSERT_NE(ns, nullptr);
        sr = spatial_reasoning_create();
        ASSERT_NE(sr, nullptr);
    }

    void TearDown() override {
        spatial_reasoning_destroy(sr);
        number_sense_destroy(ns);
    }
};

TEST_F(NumberSpatialIntegrationTest, GeometricMagnitudeEstimation) {
    /* Test: Estimate magnitudes using number sense */

    /* Use number sense to estimate from magnitude */
    number_estimate_t est_width = number_sense_estimate_from_magnitude(ns, 5.2f);
    number_estimate_t est_height = number_sense_estimate_from_magnitude(ns, 3.8f);

    /* Number sense gives approximate values with uncertainty */
    EXPECT_GT(est_width.confidence, 0.5f);
    EXPECT_GT(est_height.confidence, 0.5f);

    /* Compute diagonal using number sense magnitudes */
    float width = est_width.magnitude;
    float height = est_height.magnitude;
    float diagonal = sqrtf(width * width + height * height);

    /* Verify diagonal is reasonable (approx sqrt(5.2^2 + 3.8^2) = ~6.4) */
    EXPECT_GT(diagonal, 4.0f);
    EXPECT_LT(diagonal, 10.0f);
}

TEST_F(NumberSpatialIntegrationTest, CompareSpatialDistances) {
    /* Test: Compare distances using number sense */

    /* Two distances to compare */
    float dist1 = 10.0f;
    float dist2 = 15.0f;

    /* Use number sense to compare */
    number_comparison_t cmp = number_sense_compare(ns, dist1, dist2);

    /* Should detect dist1 < dist2 with good confidence */
    EXPECT_LT(cmp.direction, 0);  /* -1 means a < b */
    EXPECT_GT(cmp.confidence, 0.7f);  /* High confidence for 1:1.5 ratio */
}

TEST_F(NumberSpatialIntegrationTest, MentalRotationWithComparison) {
    /* Test: Rotate object and use number sense to compare magnitudes */

    /* Create two spatial objects */
    spatial_object_t obj_a = {};
    obj_a.id = 1;
    obj_a.position.x = 5.0f;
    obj_a.position.y = 0.0f;
    obj_a.position.z = 0.0f;

    spatial_object_t obj_b = {};
    obj_b.id = 2;
    obj_b.position.x = 0.0f;
    obj_b.position.y = 5.0f;
    obj_b.position.z = 0.0f;

    /* Compare the objects */
    rotation_result_t rot = spatial_rotate_and_compare(sr, &obj_a, &obj_b);

    /* Objects have same bounding radius, so should be geometrically similar */
    /* The rotation angle should be ~90 degrees */
    EXPECT_GE(rot.rotation_angle, 0.0f);
    EXPECT_LE(rot.rotation_angle, 360.0f);

    /* Compare position magnitudes using number sense */
    float mag_a = sqrtf(obj_a.position.x * obj_a.position.x +
                        obj_a.position.y * obj_a.position.y);
    float mag_b = sqrtf(obj_b.position.x * obj_b.position.x +
                        obj_b.position.y * obj_b.position.y);

    number_comparison_t cmp = number_sense_compare(ns, mag_a, mag_b);
    /* Should be perceived as equal (within Weber fraction) */
    EXPECT_EQ(cmp.direction, 0);
}

TEST_F(NumberSpatialIntegrationTest, SubitizingSmallCounts) {
    /* Test: Subitizing with spatial objects */

    /* Create input representing 3 items (within subitizing range) */
    float items[] = {1.0f, 1.0f, 1.0f};
    number_estimate_t est = number_sense_subitize(ns, items, 3);

    /* Should recognize 3 instantly */
    EXPECT_TRUE(est.is_subitized);
    EXPECT_NEAR(est.magnitude, 3.0f, 0.5f);
    EXPECT_GT(est.confidence, 0.9f);

    /* Large count should not subitize */
    float many_items[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    number_estimate_t est_many = number_sense_subitize(ns, many_items, 7);
    EXPECT_FALSE(est_many.is_subitized);
}

//=============================================================================
// Test Fixture: Mathematical Intuition Integration
//=============================================================================

class MathIntuitionIntegrationTest : public ::testing::Test {
protected:
    math_intuition_t* mi;

    void SetUp() override {
        mi = math_intuition_create();
        ASSERT_NE(mi, nullptr);
    }

    void TearDown() override {
        math_intuition_destroy(mi);
    }
};

TEST_F(MathIntuitionIntegrationTest, ArithmeticPatternAndExtrapolation) {
    /* Test: Detect arithmetic pattern and extrapolate */

    /* Sequence: 2, 4, 6, 8 - arithmetic progression */
    float seq[] = {2.0f, 4.0f, 6.0f, 8.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 4);

    EXPECT_EQ(pattern.type, PATTERN_ARITHMETIC);
    EXPECT_NEAR(pattern.params.arithmetic.difference, 2.0f, 0.1f);
    EXPECT_GT(pattern.confidence, 0.8f);

    /* Extrapolate pattern - position 5 means 5th index (0-based: 6th term) */
    float next = math_extrapolate(mi, &pattern, 5);
    /* The API uses formula: first + n*diff, so position 5 = 2 + 5*2 = 12 */
    EXPECT_NEAR(next, 12.0f, 0.5f);
}

TEST_F(MathIntuitionIntegrationTest, GeometricPatternRecognition) {
    /* Test: Detect geometric pattern and verify */

    /* Sequence: 2, 4, 8, 16 - geometric progression (ratio 2) */
    float seq[] = {2.0f, 4.0f, 8.0f, 16.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 4);

    EXPECT_EQ(pattern.type, PATTERN_GEOMETRIC);
    EXPECT_NEAR(pattern.params.geometric.ratio, 2.0f, 0.1f);
    EXPECT_GT(pattern.confidence, 0.8f);

    /* Extrapolate - position 5 uses formula: first * ratio^n = 2 * 2^5 = 64 */
    float next = math_extrapolate(mi, &pattern, 5);
    EXPECT_NEAR(next, 64.0f, 2.0f);
}

TEST_F(MathIntuitionIntegrationTest, SymmetryDetection) {
    /* Test: Detect symmetry in points */

    /* Points with reflection symmetry around y-axis */
    vec3_t points[] = {
        {-2.0f, 1.0f, 0.0f},
        {2.0f, 1.0f, 0.0f},
        {-1.0f, 2.0f, 0.0f},
        {1.0f, 2.0f, 0.0f},
        {0.0f, 3.0f, 0.0f}
    };

    symmetry_result_t sym = math_detect_symmetry(mi, points, 5);

    /* Should detect reflection symmetry */
    EXPECT_TRUE(sym.has_reflection);
    EXPECT_GT(sym.confidence, 0.7f);
}

TEST_F(MathIntuitionIntegrationTest, AnalogyTest) {
    /* Test: Solve analogy using math intuition */

    /* Analogy: 2:4 :: 3:? (doubling relationship) */
    analogy_result_t result = math_solve_analogy(mi, 2.0f, 4.0f, 3.0f);

    EXPECT_NEAR(result.answer, 6.0f, 0.5f);
    EXPECT_GT(result.confidence, 0.7f);
}

TEST_F(MathIntuitionIntegrationTest, PolynomialPatternDetection) {
    /* Test: Detect polynomial pattern (squares) */

    float seq[] = {1.0f, 4.0f, 9.0f, 16.0f, 25.0f};  /* n^2 */
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 5);

    /* Squares are detected as PATTERN_SQUARE which is a specialized pattern type */
    EXPECT_EQ(pattern.type, PATTERN_SQUARE);
    EXPECT_GT(pattern.confidence, 0.8f);
}

//=============================================================================
// Test Fixture: Full Orchestrator Integration
//=============================================================================

class ParietalOrchestratorIntegrationTest : public ::testing::Test {
protected:
    parietal_lobe_t* parietal;

    void SetUp() override {
        parietal = parietal_create();
        ASSERT_NE(parietal, nullptr);
    }

    void TearDown() override {
        parietal_destroy(parietal);
    }
};

TEST_F(ParietalOrchestratorIntegrationTest, ConvenienceWrapperPattern) {
    /* Test: Use convenience wrapper for pattern detection */

    float seq[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    detected_pattern_t pattern = parietal_detect_pattern(parietal, seq, 5);

    EXPECT_EQ(pattern.type, PATTERN_ARITHMETIC);
    EXPECT_NEAR(pattern.params.arithmetic.difference, 1.0f, 0.1f);
    EXPECT_GT(pattern.confidence, 0.8f);
}

TEST_F(ParietalOrchestratorIntegrationTest, ConvenienceWrapperEstimate) {
    /* Test: Use convenience wrapper for quantity estimation */

    float values[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};  /* 5 items */
    number_estimate_t est = parietal_estimate_quantity(parietal, values, 5);

    /* The estimation includes Weber-law noise, so just verify reasonable result */
    EXPECT_GT(est.magnitude, 0.0f);
    EXPECT_LT(est.magnitude, 20.0f);  /* Should be in reasonable range */
    EXPECT_GT(est.confidence, 0.5f);
}

TEST_F(ParietalOrchestratorIntegrationTest, ProcessPatternRequest) {
    /* Test: Process pattern detection through orchestrator */

    parietal_request_t req = {};
    req.type = PARIETAL_PATTERN_DETECT;
    req.request_id = 1;

    float pattern_data[] = {3.0f, 6.0f, 9.0f, 12.0f};
    req.input.pattern_input.sequence = pattern_data;
    req.input.pattern_input.length = 4;

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_PATTERN_DETECT);
    EXPECT_EQ(result.output.pattern.type, PATTERN_ARITHMETIC);
    EXPECT_NEAR(result.output.pattern.params.arithmetic.difference, 3.0f, 0.1f);
}

TEST_F(ParietalOrchestratorIntegrationTest, ProcessComparisonRequest) {
    /* Test: Process quantity comparison through orchestrator */

    parietal_request_t req = {};
    req.type = PARIETAL_COMPARE_QUANTITIES;
    req.request_id = 2;
    req.input.comparison_input.magnitude_a = 100.0f;
    req.input.comparison_input.magnitude_b = 50.0f;

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_COMPARE_QUANTITIES);
    EXPECT_GT(result.output.comparison.direction, 0);  /* 100 > 50 */
    EXPECT_GT(result.output.comparison.confidence, 0.8f);
}

TEST_F(ParietalOrchestratorIntegrationTest, ProcessMentalRotation) {
    /* Test: Process mental rotation through orchestrator */

    spatial_object_t obj_a = {};
    obj_a.id = 1;
    obj_a.bounding_radius = 1.0f;

    spatial_object_t obj_b = {};
    obj_b.id = 2;
    obj_b.bounding_radius = 1.0f;  /* Same size */

    parietal_request_t req = {};
    req.type = PARIETAL_MENTAL_ROTATION;
    req.request_id = 3;
    req.input.rotation_input.object_a = &obj_a;
    req.input.rotation_input.object_b = &obj_b;

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_MENTAL_ROTATION);
    /* Processing time should be positive */
    EXPECT_GE(result.output.rotation.processing_time_ms, 0);
}

TEST_F(ParietalOrchestratorIntegrationTest, ChainedProcessing) {
    /* Test: Multiple sequential operations */

    /* Step 1: Detect pattern */
    float seq[] = {2.0f, 4.0f, 8.0f, 16.0f};
    detected_pattern_t pattern = parietal_detect_pattern(parietal, seq, 4);
    EXPECT_EQ(pattern.type, PATTERN_GEOMETRIC);

    /* Step 2: Use pattern to predict next value (position 5 = 2 * 2^5 = 64) */
    float next = math_extrapolate(parietal_get_math_intuition(parietal), &pattern, 5);
    EXPECT_NEAR(next, 64.0f, 4.0f);

    /* Step 3: Compare prediction with expected using number sense */
    number_comparison_t cmp = number_sense_compare(
        parietal_get_number_sense(parietal),
        next,
        64.0f
    );
    EXPECT_EQ(cmp.direction, 0);  /* Should be equal */
}

TEST_F(ParietalOrchestratorIntegrationTest, SubmoduleAccess) {
    /* Test: Access submodules through orchestrator */

    /* Get submodules */
    number_sense_t* ns = parietal_get_number_sense(parietal);
    spatial_reasoning_t* sr = parietal_get_spatial(parietal);
    math_intuition_t* mi = parietal_get_math_intuition(parietal);
    equation_engine_t* eq = parietal_get_equation_engine(parietal);

    EXPECT_NE(ns, nullptr);
    EXPECT_NE(sr, nullptr);
    EXPECT_NE(mi, nullptr);
    EXPECT_NE(eq, nullptr);

    /* Use number sense directly - magnitude includes some noise/uncertainty */
    number_estimate_t est = number_sense_estimate_from_magnitude(ns, 7.0f);
    /* Just verify we get a positive magnitude with reasonable confidence */
    EXPECT_GT(est.magnitude, 0.0f);
    EXPECT_GT(est.confidence, 0.5f);

    /* Use math intuition directly */
    float pattern_seq[] = {1.0f, 3.0f, 5.0f, 7.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, pattern_seq, 4);
    EXPECT_EQ(pattern.type, PATTERN_ARITHMETIC);
}

TEST_F(ParietalOrchestratorIntegrationTest, ModulationEffect) {
    /* Test: Inflammation affects processing */

    /* Get baseline performance */
    float baseline_seq[] = {1.0f, 2.0f, 3.0f, 4.0f};
    detected_pattern_t baseline = parietal_detect_pattern(parietal, baseline_seq, 4);
    float baseline_conf = baseline.confidence;

    /* Apply inflammation */
    int ret = parietal_set_inflammation(parietal, 0.5f);
    EXPECT_EQ(ret, 0);

    /* Check performance under inflammation */
    detected_pattern_t inflamed = parietal_detect_pattern(parietal, baseline_seq, 4);

    /* Pattern should still be detected, but confidence may be lower */
    EXPECT_EQ(inflamed.type, PATTERN_ARITHMETIC);
    /* Inflammation may reduce confidence */
    EXPECT_LE(inflamed.confidence, baseline_conf + 0.1f);

    /* Reset */
    parietal_set_inflammation(parietal, 0.0f);
}

TEST_F(ParietalOrchestratorIntegrationTest, StatisticsTracking) {
    /* Test: Statistics are tracked across operations */

    /* Perform several operations */
    for (int i = 0; i < 5; i++) {
        float values[] = {(float)(i+1), (float)(i+1), (float)(i+1)};
        parietal_estimate_quantity(parietal, values, 3);
    }

    for (int i = 0; i < 3; i++) {
        float seq[] = {1.0f, 2.0f, 3.0f, 4.0f};
        parietal_detect_pattern(parietal, seq, 4);
    }

    /* Get statistics */
    parietal_stats_t stats;
    int ret = parietal_get_stats(parietal, &stats);
    EXPECT_EQ(ret, 0);

    /* Verify counts */
    EXPECT_GE(stats.total_requests, 8);  /* At least 5 estimates + 3 patterns */
    EXPECT_GT(stats.avg_processing_time_us, 0.0f);
}

TEST_F(ParietalOrchestratorIntegrationTest, MultipleRequestTypes) {
    /* Test: Process different request types */

    /* Pattern detection */
    parietal_request_t req1 = {};
    req1.type = PARIETAL_PATTERN_DETECT;
    req1.request_id = 1;
    float seq1[] = {1.0f, 4.0f, 9.0f, 16.0f};  /* Squares */
    req1.input.pattern_input.sequence = seq1;
    req1.input.pattern_input.length = 4;

    parietal_result_t res1 = parietal_process(parietal, &req1);
    EXPECT_TRUE(res1.success);
    /* Squares are detected as PATTERN_SQUARE */
    EXPECT_EQ(res1.output.pattern.type, PATTERN_SQUARE);

    /* Quantity estimation */
    parietal_request_t req2 = {};
    req2.type = PARIETAL_ESTIMATE_QUANTITY;
    req2.request_id = 2;
    float vals[] = {1.0f, 1.0f, 1.0f, 1.0f};
    req2.input.quantity_input.values = vals;
    req2.input.quantity_input.num_values = 4;

    parietal_result_t res2 = parietal_process(parietal, &req2);
    EXPECT_TRUE(res2.success);
    EXPECT_NEAR(res2.output.estimate.magnitude, 4.0f, 1.0f);

    /* Analogy */
    parietal_request_t req3 = {};
    req3.type = PARIETAL_SOLVE_ANALOGY;
    req3.request_id = 3;
    req3.input.analogy_input.a = 1.0f;
    req3.input.analogy_input.b = 2.0f;
    req3.input.analogy_input.c = 5.0f;

    parietal_result_t res3 = parietal_process(parietal, &req3);
    EXPECT_TRUE(res3.success);
    EXPECT_NEAR(res3.output.analogy.answer, 10.0f, 0.5f);  /* Doubling */
}

//=============================================================================
// Test Fixture: Cross-Domain Integration
//=============================================================================

class CrossDomainIntegrationTest : public ::testing::Test {
protected:
    parietal_lobe_t* parietal;
    number_sense_t* ns;
    spatial_reasoning_t* sr;
    math_intuition_t* mi;

    void SetUp() override {
        parietal = parietal_create();
        ASSERT_NE(parietal, nullptr);

        /* Get submodules */
        ns = parietal_get_number_sense(parietal);
        sr = parietal_get_spatial(parietal);
        mi = parietal_get_math_intuition(parietal);

        ASSERT_NE(ns, nullptr);
        ASSERT_NE(sr, nullptr);
        ASSERT_NE(mi, nullptr);
    }

    void TearDown() override {
        parietal_destroy(parietal);
    }
};

TEST_F(CrossDomainIntegrationTest, SpatialPatternRecognition) {
    /* Test: Detect pattern in spatial distances */

    /* Distances forming arithmetic sequence */
    float distances[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    /* Detect pattern */
    detected_pattern_t pattern = math_detect_pattern(mi, distances, 5);
    EXPECT_EQ(pattern.type, PATTERN_ARITHMETIC);

    /* Compare consecutive differences using number sense */
    for (int i = 1; i < 5; i++) {
        float diff = distances[i] - distances[i-1];
        number_comparison_t cmp = number_sense_compare(ns, diff, 1.0f);
        EXPECT_EQ(cmp.direction, 0);  /* All differences should equal 1 */
    }
}

TEST_F(CrossDomainIntegrationTest, QuantityEstimationAccuracy) {
    /* Test: Estimation accuracy follows Weber's law */

    /* Larger magnitudes have proportionally larger uncertainty */
    number_estimate_t small = number_sense_estimate_from_magnitude(ns, 10.0f);
    number_estimate_t large = number_sense_estimate_from_magnitude(ns, 100.0f);

    /* Weber fraction: uncertainty / magnitude should be similar */
    float weber_small = small.uncertainty / small.magnitude;
    float weber_large = large.uncertainty / large.magnitude;

    /* Weber fractions should be approximately equal */
    EXPECT_NEAR(weber_small, weber_large, 0.1f);
}

TEST_F(CrossDomainIntegrationTest, CoordinateTransform) {
    /* Test: Coordinate transformation */

    /* Observer at origin looking along +X axis */
    observer_pose_t observer = {};
    observer.position.x = 0.0f;
    observer.position.y = 0.0f;
    observer.position.z = 0.0f;
    observer.orientation.w = 1.0f;  /* Identity quaternion */
    observer.orientation.x = 0.0f;
    observer.orientation.y = 0.0f;
    observer.orientation.z = 0.0f;
    observer.heading = 0.0f;

    /* Local position */
    vec3_t local = {5.0f, 0.0f, 0.0f};

    /* Transform to world coordinates */
    vec3_t world = spatial_ego_to_allocentric(sr, local, &observer);

    /* Should be same as local (observer at origin, no rotation) */
    EXPECT_NEAR(world.x, local.x, 0.1f);
    EXPECT_NEAR(world.y, local.y, 0.1f);
    EXPECT_NEAR(world.z, local.z, 0.1f);
}

TEST_F(CrossDomainIntegrationTest, PatternExtrapolationChain) {
    /* Test: Chain pattern detection and extrapolation */

    /* Start with arithmetic sequence */
    float seq[] = {5.0f, 10.0f, 15.0f, 20.0f};

    /* Detect and extrapolate */
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 4);
    EXPECT_EQ(pattern.type, PATTERN_ARITHMETIC);

    /* Position 5 = first + 5*diff = 5 + 5*5 = 30 */
    float next = math_extrapolate(mi, &pattern, 5);
    EXPECT_NEAR(next, 30.0f, 1.0f);

    /* Compare extrapolated value with expected */
    number_comparison_t cmp = number_sense_compare(ns, next, 30.0f);
    EXPECT_EQ(cmp.direction, 0);  /* Equal */
}

//=============================================================================
// Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
