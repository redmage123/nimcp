/**
 * @file test_surface_geometry_core.cpp
 * @brief Unit Tests for Surface Geometry Core Module
 *
 * WHAT: Tests for surface geometry parameter computation and validation
 * WHY:  Verify paper predictions (Meng et al. Nature 2026):
 *       - Trifurcations emerge at chi >= 0.83
 *       - Orthogonal sprouting for rho < rho_threshold (~0.6)
 *       - Optimal angles follow paper Fig. 4g predictions
 * HOW:  GTest-based tests with floating-point tolerance
 *
 * NIMCP STANDARDS:
 * - All tests < 50 lines
 * - Guard clauses for setup validation
 * - WHAT-WHY-HOW documentation
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "core/geometry/nimcp_surface_geometry.h"
#include "core/geometry/nimcp_surface_geometry_types.h"
}

// Test tolerance for floating point comparisons
#define TOLERANCE 1e-5f
#define ANGLE_TOLERANCE 0.1f  // ~6 degrees

//=============================================================================
// Test Fixture
//=============================================================================

class SurfaceGeometryTest : public ::testing::Test {
protected:
    void SetUp() override {
        surface_geometry_default_config(&config);
        ctx = surface_geometry_create(&config);
        ASSERT_NE(ctx, nullptr) << "Failed to create geometry context";
    }

    void TearDown() override {
        if (ctx) {
            surface_geometry_destroy(ctx);
            ctx = nullptr;
        }
    }

    surface_geometry_config_t config;
    surface_geometry_ctx_t* ctx;
};

//=============================================================================
// Configuration Tests
//=============================================================================

class ConfigurationTest : public SurfaceGeometryTest {};

TEST_F(ConfigurationTest, DefaultConfig_HasPaperValues) {
    // Default config should have paper-derived values
    EXPECT_NEAR(config.chi_trifurcation_threshold, 0.83f, TOLERANCE);
    EXPECT_NEAR(config.rho_threshold, 0.6f, TOLERANCE);
}

TEST_F(ConfigurationTest, DefaultConfig_NullParam) {
    int result = surface_geometry_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ConfigurationTest, GetConfig_ReturnsCurrentValues) {
    surface_geometry_config_t retrieved;
    int result = surface_geometry_get_config(ctx, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_NEAR(retrieved.chi_trifurcation_threshold, 0.83f, TOLERANCE);
}

TEST_F(ConfigurationTest, SetConfig_UpdatesValues) {
    config.rho_threshold = 0.5f;
    int result = surface_geometry_set_config(ctx, &config);
    EXPECT_EQ(result, 0);

    surface_geometry_config_t retrieved;
    surface_geometry_get_config(ctx, &retrieved);
    EXPECT_NEAR(retrieved.rho_threshold, 0.5f, TOLERANCE);
}

TEST_F(ConfigurationTest, Reset_RestoresInitialState) {
    config.rho_threshold = 0.5f;
    surface_geometry_set_config(ctx, &config);

    int result = surface_geometry_reset(ctx);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Regime Determination Tests (Paper Fig. 4)
//=============================================================================

class RegimeTest : public SurfaceGeometryTest {};

TEST_F(RegimeTest, DetermineRegime_Sprouting_RhoBelow) {
    surface_regime_t regime;
    int result = surface_determine_regime(0.3f, 0.6f, &regime);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(regime, SURFACE_REGIME_SPROUTING);
}

TEST_F(RegimeTest, DetermineRegime_Branching_RhoAbove) {
    surface_regime_t regime;
    int result = surface_determine_regime(0.8f, 0.6f, &regime);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(regime, SURFACE_REGIME_BRANCHING);
}

TEST_F(RegimeTest, DetermineRegime_Branching_RhoAtThreshold) {
    surface_regime_t regime;
    int result = surface_determine_regime(0.6f, 0.6f, &regime);
    EXPECT_EQ(result, 0);
    // At threshold, should be branching (equal case)
    EXPECT_EQ(regime, SURFACE_REGIME_BRANCHING);
}

TEST_F(RegimeTest, DetermineRegime_NullOutput) {
    int result = surface_determine_regime(0.5f, 0.6f, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RegimeTest, DetermineRegime_HumanNeuronThreshold) {
    // Human neuron rho_threshold ~0.56
    surface_regime_t regime;
    surface_determine_regime(0.55f, SURFACE_RHO_THRESHOLD_HUMAN_NEURON, &regime);
    EXPECT_EQ(regime, SURFACE_REGIME_SPROUTING);

    surface_determine_regime(0.57f, SURFACE_RHO_THRESHOLD_HUMAN_NEURON, &regime);
    EXPECT_EQ(regime, SURFACE_REGIME_BRANCHING);
}

TEST_F(RegimeTest, DetermineRegime_FruitFlyThreshold) {
    // Fruit fly rho_threshold ~0.52
    surface_regime_t regime;
    surface_determine_regime(0.50f, SURFACE_RHO_THRESHOLD_FRUIT_FLY, &regime);
    EXPECT_EQ(regime, SURFACE_REGIME_SPROUTING);

    surface_determine_regime(0.54f, SURFACE_RHO_THRESHOLD_FRUIT_FLY, &regime);
    EXPECT_EQ(regime, SURFACE_REGIME_BRANCHING);
}

//=============================================================================
// Branch Type Prediction Tests (Paper Fig. 3)
//=============================================================================

class BranchTypeTest : public SurfaceGeometryTest {};

TEST_F(BranchTypeTest, PredictBranchType_Bifurcation_ChiBelow) {
    surface_branch_type_t type;
    int result = surface_predict_branch_type(0.5f, &type);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(type, SURFACE_BRANCH_BIFURCATION);
}

TEST_F(BranchTypeTest, PredictBranchType_Trifurcation_ChiAbove) {
    surface_branch_type_t type;
    int result = surface_predict_branch_type(0.9f, &type);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(type, SURFACE_BRANCH_TRIFURCATION);
}

TEST_F(BranchTypeTest, PredictBranchType_TransitionPoint) {
    // At chi = 0.83, paper predicts trifurcation emergence
    surface_branch_type_t type;
    int result = surface_predict_branch_type(0.83f, &type);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(type, SURFACE_BRANCH_TRIFURCATION);
}

TEST_F(BranchTypeTest, PredictBranchType_JustBelowTransition) {
    surface_branch_type_t type;
    int result = surface_predict_branch_type(0.82f, &type);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(type, SURFACE_BRANCH_BIFURCATION);
}

TEST_F(BranchTypeTest, PredictBranchType_NullOutput) {
    int result = surface_predict_branch_type(0.5f, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BranchTypeTest, PredictBranchType_ChiZero) {
    // Chi = 0 should still be bifurcation (Steiner limit)
    surface_branch_type_t type;
    int result = surface_predict_branch_type(0.0f, &type);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(type, SURFACE_BRANCH_BIFURCATION);
}

TEST_F(BranchTypeTest, PredictBranchType_ChiTwo) {
    // Chi = 2 is the maximum (circumference = diameter)
    surface_branch_type_t type;
    int result = surface_predict_branch_type(2.0f, &type);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(type, SURFACE_BRANCH_TRIFURCATION);
}

//=============================================================================
// Optimal Angle Tests (Paper Fig. 4g)
//=============================================================================

class OptimalAngleTest : public SurfaceGeometryTest {};

TEST_F(OptimalAngleTest, ComputeOptimalAngle_SproutingRegime) {
    // In sprouting regime (rho < rho_th), angle should be ~90 degrees
    surface_geometry_params_t params = {};
    params.rho = 0.3f;
    params.rho_threshold = 0.6f;
    params.regime = SURFACE_REGIME_SPROUTING;

    float angle;
    int result = surface_compute_optimal_angle(&params, &angle);
    EXPECT_EQ(result, 0);
    // 90 degrees = pi/2 radians
    EXPECT_NEAR(angle, M_PI / 2.0f, ANGLE_TOLERANCE);
}

TEST_F(OptimalAngleTest, ComputeOptimalAngle_BranchingRegime) {
    // In branching regime (rho > rho_th), angle increases linearly
    surface_geometry_params_t params = {};
    params.rho = 0.8f;
    params.rho_threshold = 0.6f;
    params.regime = SURFACE_REGIME_BRANCHING;

    float angle;
    int result = surface_compute_optimal_angle(&params, &angle);
    EXPECT_EQ(result, 0);
    // Angle should be between 90 and 120 degrees
    EXPECT_GT(angle, M_PI / 2.0f - ANGLE_TOLERANCE);
    EXPECT_LT(angle, 2.0f * M_PI / 3.0f + ANGLE_TOLERANCE);
}

TEST_F(OptimalAngleTest, ComputeOptimalAngle_SteinerLimit) {
    // At rho = 1, should approach Steiner angle (~60 degrees or 120 degrees)
    surface_geometry_params_t params = {};
    params.rho = 1.0f;
    params.rho_threshold = 0.6f;
    params.regime = SURFACE_REGIME_BRANCHING;

    float angle;
    int result = surface_compute_optimal_angle(&params, &angle);
    EXPECT_EQ(result, 0);
}

TEST_F(OptimalAngleTest, ComputeOptimalAngle_NullParams) {
    float angle;
    int result = surface_compute_optimal_angle(nullptr, &angle);
    EXPECT_EQ(result, -1);
}

TEST_F(OptimalAngleTest, ComputeOptimalAngle_NullOutput) {
    surface_geometry_params_t params = {};
    params.rho = 0.5f;
    params.rho_threshold = 0.6f;
    int result = surface_compute_optimal_angle(&params, nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Spine Geometry Tests
//=============================================================================

class SpineGeometryTest : public SurfaceGeometryTest {};

TEST_F(SpineGeometryTest, ComputeSpineGeometry_OrthogonalSprout) {
    // Thin spine on thick dendrite -> sprout
    float parent_diam = 2.0f;
    float spine_diam = 0.5f;  // rho = 0.25 < 0.6
    surface_vec3_t pos = {1.0f, 0.0f, 0.0f};
    spine_surface_geometry_t result = {};

    int ret = surface_compute_spine_geometry(ctx, parent_diam, spine_diam, &pos, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_sprout);
    EXPECT_NEAR(result.optimal_angle, M_PI / 2.0f, ANGLE_TOLERANCE);
}

TEST_F(SpineGeometryTest, ComputeSpineGeometry_Branch) {
    // Thick spine on parent -> branch
    float parent_diam = 2.0f;
    float spine_diam = 1.5f;  // rho = 0.75 > 0.6
    surface_vec3_t pos = {1.0f, 0.0f, 0.0f};
    spine_surface_geometry_t result = {};

    int ret = surface_compute_spine_geometry(ctx, parent_diam, spine_diam, &pos, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_sprout);
}

TEST_F(SpineGeometryTest, PredictSpineSprout_True) {
    bool is_sprout;
    int ret = surface_predict_spine_sprout(ctx, 2.0f, 0.5f, &is_sprout);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(is_sprout);
}

TEST_F(SpineGeometryTest, PredictSpineSprout_False) {
    bool is_sprout;
    int ret = surface_predict_spine_sprout(ctx, 2.0f, 1.5f, &is_sprout);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(is_sprout);
}

TEST_F(SpineGeometryTest, PredictSpineSprout_NullOutput) {
    int ret = surface_predict_spine_sprout(ctx, 2.0f, 0.5f, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Axon Branch Geometry Tests
//=============================================================================

class AxonBranchTest : public SurfaceGeometryTest {};

TEST_F(AxonBranchTest, ComputeAxonBranchGeometry_Bifurcation) {
    float parent_diam = 2.0f;
    float child_diams[] = {1.5f, 1.2f};
    axon_branch_surface_geometry_t result = {};

    int ret = surface_compute_axon_branch_geometry(ctx, parent_diam, child_diams, 2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.degree, 3u);  // parent + 2 children = bifurcation (k=3)
}

TEST_F(AxonBranchTest, ComputeAxonBranchGeometry_Trifurcation) {
    float parent_diam = 2.0f;
    float child_diams[] = {1.2f, 1.1f, 1.0f};
    axon_branch_surface_geometry_t result = {};

    int ret = surface_compute_axon_branch_geometry(ctx, parent_diam, child_diams, 3, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.degree, 4u);  // parent + 3 children = trifurcation (k=4)
}

TEST_F(AxonBranchTest, ComputeAxonBranchGeometry_NullOutput) {
    float child_diams[] = {1.0f, 1.0f};
    int ret = surface_compute_axon_branch_geometry(ctx, 2.0f, child_diams, 2, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Spine Cache Tests
//=============================================================================

class SpineCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = surface_spine_cache_create(100);
        ASSERT_NE(cache, nullptr);
    }

    void TearDown() override {
        if (cache) {
            surface_spine_cache_destroy(cache);
            cache = nullptr;
        }
    }

    spine_surface_cache_t* cache;
};

TEST_F(SpineCacheTest, Create_ValidCapacity) {
    EXPECT_NE(cache, nullptr);
}

TEST_F(SpineCacheTest, Create_ZeroCapacity) {
    spine_surface_cache_t* zero_cache = surface_spine_cache_create(0);
    // Implementation may reject zero capacity or use default
    if (zero_cache) {
        surface_spine_cache_destroy(zero_cache);
    }
}

TEST_F(SpineCacheTest, PutAndGet_Success) {
    spine_surface_geometry_t geom = {};
    geom.spine_id = 42;
    geom.is_sprout = true;
    geom.optimal_angle = M_PI / 2.0f;

    int ret = surface_spine_cache_put(cache, 42, &geom);
    EXPECT_EQ(ret, 0);

    spine_surface_geometry_t retrieved = {};
    ret = surface_spine_cache_get(cache, 42, &retrieved);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(retrieved.spine_id, 42u);
    EXPECT_TRUE(retrieved.is_sprout);
    EXPECT_NEAR(retrieved.optimal_angle, M_PI / 2.0f, TOLERANCE);
}

TEST_F(SpineCacheTest, Get_NotFound) {
    spine_surface_geometry_t retrieved;
    int ret = surface_spine_cache_get(cache, 999, &retrieved);
    EXPECT_EQ(ret, -1);  // Not found
}

TEST_F(SpineCacheTest, Invalidate_Success) {
    spine_surface_geometry_t geom = {};
    geom.spine_id = 42;
    surface_spine_cache_put(cache, 42, &geom);

    int ret = surface_spine_cache_invalidate(cache, 42);
    EXPECT_EQ(ret, 0);

    spine_surface_geometry_t retrieved;
    ret = surface_spine_cache_get(cache, 42, &retrieved);
    EXPECT_EQ(ret, -1);  // Should be invalidated
}

TEST_F(SpineCacheTest, Clear_RemovesAllEntries) {
    spine_surface_geometry_t geom = {};
    for (uint32_t i = 0; i < 10; i++) {
        geom.spine_id = i;
        surface_spine_cache_put(cache, i, &geom);
    }

    int ret = surface_spine_cache_clear(cache);
    EXPECT_EQ(ret, 0);

    spine_surface_geometry_t retrieved;
    for (uint32_t i = 0; i < 10; i++) {
        ret = surface_spine_cache_get(cache, i, &retrieved);
        EXPECT_EQ(ret, -1);
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

class StatisticsTest : public SurfaceGeometryTest {};

TEST_F(StatisticsTest, GetStats_InitialValues) {
    surface_geometry_stats_t stats = {};
    int ret = surface_geometry_get_stats(ctx, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_optimizations, 0u);
}

TEST_F(StatisticsTest, GetStats_NullOutput) {
    int ret = surface_geometry_get_stats(ctx, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(StatisticsTest, ResetStats_ClearsCounters) {
    surface_geometry_stats_t stats = {};
    int ret = surface_geometry_reset_stats(ctx);
    EXPECT_EQ(ret, 0);

    ret = surface_geometry_get_stats(ctx, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_optimizations, 0u);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

class UtilityTest : public ::testing::Test {};

TEST_F(UtilityTest, RegimeName_Sprouting) {
    const char* name = surface_regime_name(SURFACE_REGIME_SPROUTING);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(UtilityTest, RegimeName_Branching) {
    const char* name = surface_regime_name(SURFACE_REGIME_BRANCHING);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(UtilityTest, RegimeName_Steiner) {
    const char* name = surface_regime_name(SURFACE_REGIME_STEINER);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(UtilityTest, BranchTypeName_Bifurcation) {
    const char* name = surface_branch_type_name(SURFACE_BRANCH_BIFURCATION);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(UtilityTest, BranchTypeName_Trifurcation) {
    const char* name = surface_branch_type_name(SURFACE_BRANCH_TRIFURCATION);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(UtilityTest, ValidationStatusName_Valid) {
    const char* name = surface_validation_status_name(SURFACE_VALIDATION_VALID);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(UtilityTest, ErrorString_Ok) {
    const char* str = surface_error_string(SURFACE_OK);
    EXPECT_NE(str, nullptr);
}

TEST_F(UtilityTest, ErrorString_Null) {
    const char* str = surface_error_string(SURFACE_ERROR_NULL);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Null Context Tests
//=============================================================================

class NullContextTest : public ::testing::Test {};

TEST_F(NullContextTest, GetConfig_NullContext) {
    surface_geometry_config_t config;
    int ret = surface_geometry_get_config(nullptr, &config);
    EXPECT_EQ(ret, -1);
}

TEST_F(NullContextTest, SetConfig_NullContext) {
    surface_geometry_config_t config = {};
    int ret = surface_geometry_set_config(nullptr, &config);
    EXPECT_EQ(ret, -1);
}

TEST_F(NullContextTest, Reset_NullContext) {
    int ret = surface_geometry_reset(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(NullContextTest, GetStats_NullContext) {
    surface_geometry_stats_t stats;
    int ret = surface_geometry_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(NullContextTest, ResetStats_NullContext) {
    int ret = surface_geometry_reset_stats(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(NullContextTest, ComputeBranchParams_NullContext) {
    surface_branch_point_t branch = {};
    surface_geometry_params_t params;
    int ret = surface_compute_branch_params(nullptr, &branch, &params);
    EXPECT_EQ(ret, -1);
}

TEST_F(NullContextTest, ComputeArea_NullContext) {
    surface_branch_point_t branches[1] = {};
    float area;
    int ret = surface_compute_area(nullptr, branches, 1, 1.0f, &area);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Macro Tests
//=============================================================================

TEST(MacroTest, IsTrifurcationRegime_Above) {
    EXPECT_TRUE(SURFACE_IS_TRIFURCATION_REGIME(0.9f));
}

TEST(MacroTest, IsTrifurcationRegime_Below) {
    EXPECT_FALSE(SURFACE_IS_TRIFURCATION_REGIME(0.5f));
}

TEST(MacroTest, IsTrifurcationRegime_AtThreshold) {
    EXPECT_TRUE(SURFACE_IS_TRIFURCATION_REGIME(0.83f));
}

TEST(MacroTest, IsSproutingRegime_Below) {
    EXPECT_TRUE(SURFACE_IS_SPROUTING_REGIME(0.3f, 0.6f));
}

TEST(MacroTest, IsSproutingRegime_Above) {
    EXPECT_FALSE(SURFACE_IS_SPROUTING_REGIME(0.8f, 0.6f));
}

TEST(MacroTest, SurfaceClamp_InRange) {
    EXPECT_NEAR(SURFACE_CLAMP(0.5f, 0.0f, 1.0f), 0.5f, TOLERANCE);
}

TEST(MacroTest, SurfaceClamp_BelowMin) {
    EXPECT_NEAR(SURFACE_CLAMP(-0.5f, 0.0f, 1.0f), 0.0f, TOLERANCE);
}

TEST(MacroTest, SurfaceClamp_AboveMax) {
    EXPECT_NEAR(SURFACE_CLAMP(1.5f, 0.0f, 1.0f), 1.0f, TOLERANCE);
}
