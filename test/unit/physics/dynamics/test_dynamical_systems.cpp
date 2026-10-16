/**
 * @file test_dynamical_systems.cpp
 * @brief Unit tests for Dynamical Systems API
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Tests the dynamical systems module including:
 * - System lifecycle (create, destroy, config)
 * - RK4 integration
 * - Lyapunov exponent computation
 * - Bifurcation analysis
 * - Attractor reconstruction
 * - Energy landscape analysis
 * - Slow-fast decomposition
 * - Bridge API
 * - Error handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "physics/dynamics/nimcp_dynamical_systems.h"
}

//=============================================================================
// Test Dynamical System Functions
//=============================================================================

// Lorenz system: classic chaotic system
static int lorenz_system(const float* state, uint32_t state_dim,
                         const float* params, uint32_t param_dim,
                         float* derivative, void* context)
{
    (void)param_dim;
    (void)context;

    if (!state || !derivative || state_dim < 3) return -1;

    float sigma = params ? params[0] : 10.0f;
    float rho = params ? params[1] : 28.0f;
    float beta = params ? params[2] : 8.0f / 3.0f;

    float x = state[0];
    float y = state[1];
    float z = state[2];

    derivative[0] = sigma * (y - x);
    derivative[1] = x * (rho - z) - y;
    derivative[2] = x * y - beta * z;

    return 0;
}

// Simple harmonic oscillator
static int harmonic_oscillator(const float* state, uint32_t state_dim,
                               const float* params, uint32_t param_dim,
                               float* derivative, void* context)
{
    (void)param_dim;
    (void)context;

    if (!state || !derivative || state_dim < 2) return -1;

    float omega = params ? params[0] : 1.0f;

    derivative[0] = state[1];
    derivative[1] = -omega * omega * state[0];

    return 0;
}

// Logistic map (discrete, but can be used for testing)
static int logistic_map(const float* state, uint32_t state_dim,
                        const float* params, uint32_t param_dim,
                        float* derivative, void* context)
{
    (void)param_dim;
    (void)context;

    if (!state || !derivative || state_dim < 1) return -1;

    float r = params ? params[0] : 3.9f;
    float x = state[0];

    derivative[0] = r * x * (1.0f - x) - x;  // Continuous approximation

    return 0;
}

//=============================================================================
// Test Fixture
//=============================================================================

class DynamicalSystemsTest : public ::testing::Test {
protected:
    dynsys_system_t sys_ = nullptr;
    dynsys_config_t config_;

    void SetUp() override {
        config_ = dynsys_default_config();
        config_.state_dim = 3;
        config_.param_dim = 3;
        config_.dt = 0.01f;

        sys_ = dynsys_create(&config_, lorenz_system, nullptr, nullptr);
        ASSERT_NE(sys_, nullptr);

        // Set Lorenz parameters
        float params[] = {10.0f, 28.0f, 8.0f / 3.0f};
        dynsys_set_params(sys_, params, 3);
    }

    void TearDown() override {
        if (sys_) {
            dynsys_destroy(sys_);
            sys_ = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, DefaultConfig) {
    dynsys_config_t cfg = dynsys_default_config();

    EXPECT_EQ(cfg.state_dim, 3u);
    EXPECT_EQ(cfg.param_dim, 1u);
    EXPECT_FLOAT_EQ(cfg.dt, DYNSYS_DEFAULT_DT);
    EXPECT_TRUE(cfg.enable_logging);
    EXPECT_TRUE(cfg.enable_metrics);
    EXPECT_TRUE(cfg.enable_kg_wiring);
    EXPECT_TRUE(cfg.enable_exception_handling);
}

TEST_F(DynamicalSystemsTest, LyapunovDefaultConfig) {
    dynsys_lyapunov_config_t cfg = dynsys_lyapunov_default_config();

    EXPECT_EQ(cfg.num_exponents, 3u);
    EXPECT_GT(cfg.orthonormalization_interval, 0.0f);
    EXPECT_GT(cfg.transient_steps, 0u);
    EXPECT_GT(cfg.analysis_steps, 0u);
    EXPECT_GT(cfg.perturbation_size, 0.0f);
}

TEST_F(DynamicalSystemsTest, BifurcationDefaultConfig) {
    dynsys_bifurcation_config_t cfg = dynsys_bifurcation_default_config();

    EXPECT_EQ(cfg.param_index, 0u);
    EXPECT_LT(cfg.param_start, cfg.param_end);
    EXPECT_GT(cfg.num_points, 0u);
    EXPECT_GT(cfg.transient_steps, 0u);
    EXPECT_GT(cfg.sample_steps, 0u);
}

TEST_F(DynamicalSystemsTest, AttractorDefaultConfig) {
    dynsys_attractor_config_t cfg = dynsys_attractor_default_config();

    EXPECT_GT(cfg.embedding_dim, 0u);
    EXPECT_GT(cfg.time_delay, 0u);
    EXPECT_GT(cfg.num_samples, 0u);
}

TEST_F(DynamicalSystemsTest, EnergyDefaultConfig) {
    dynsys_energy_config_t cfg = dynsys_energy_default_config();

    EXPECT_GT(cfg.grid_resolution, 0u);
    EXPECT_LT(cfg.state_min, cfg.state_max);
}

TEST_F(DynamicalSystemsTest, SlowfastDefaultConfig) {
    dynsys_slowfast_config_t cfg = dynsys_slowfast_default_config();

    EXPECT_GT(cfg.epsilon, 0.0f);
    EXPECT_TRUE(cfg.compute_manifold);
}

TEST_F(DynamicalSystemsTest, BridgeDefaultConfig) {
    dynsys_bridge_config_t cfg = dynsys_bridge_default_config();

    EXPECT_TRUE(cfg.enable_kg_wiring);
    EXPECT_TRUE(cfg.enable_exception_handling);
    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_TRUE(cfg.enable_immune_presentation);
    EXPECT_TRUE(cfg.enable_logging);
}

//=============================================================================
// System Lifecycle Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, CreateWithNullConfig) {
    dynsys_system_t s = dynsys_create(nullptr, lorenz_system, nullptr, nullptr);
    EXPECT_EQ(s, nullptr);
}

TEST_F(DynamicalSystemsTest, CreateWithNullFunc) {
    dynsys_config_t cfg = dynsys_default_config();
    dynsys_system_t s = dynsys_create(&cfg, nullptr, nullptr, nullptr);
    EXPECT_EQ(s, nullptr);
}

TEST_F(DynamicalSystemsTest, DestroyNull) {
    dynsys_destroy(nullptr);  // Should not crash
}

TEST_F(DynamicalSystemsTest, InitAndShutdown) {
    EXPECT_EQ(dynsys_init(sys_, nullptr), DYNSYS_OK);
    EXPECT_EQ(dynsys_shutdown(sys_), DYNSYS_OK);
}

TEST_F(DynamicalSystemsTest, InitNullSystem) {
    EXPECT_EQ(dynsys_init(nullptr, nullptr), DYNSYS_ERR_NULL_PTR);
}

TEST_F(DynamicalSystemsTest, ShutdownNullSystem) {
    EXPECT_EQ(dynsys_shutdown(nullptr), DYNSYS_ERR_NULL_PTR);
}

//=============================================================================
// Parameter Setting Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, SetParams) {
    float params[] = {15.0f, 35.0f, 3.0f};
    EXPECT_EQ(dynsys_set_params(sys_, params, 3), DYNSYS_OK);
}

TEST_F(DynamicalSystemsTest, SetParamsNullSystem) {
    float params[] = {15.0f, 35.0f, 3.0f};
    EXPECT_EQ(dynsys_set_params(nullptr, params, 3), DYNSYS_ERR_NULL_PTR);
}

TEST_F(DynamicalSystemsTest, SetParamsNullParams) {
    EXPECT_EQ(dynsys_set_params(sys_, nullptr, 3), DYNSYS_ERR_NULL_PTR);
}

TEST_F(DynamicalSystemsTest, SetParamsDimensionMismatch) {
    float params[] = {15.0f, 35.0f};
    EXPECT_EQ(dynsys_set_params(sys_, params, 2), DYNSYS_ERR_INVALID_DIM);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, StepRK4) {
    float state[] = {1.0f, 0.0f, 0.0f};

    EXPECT_EQ(dynsys_step_rk4(sys_, state, 0.01f), DYNSYS_OK);

    // State should have changed
    EXPECT_NE(state[0], 1.0f);
    // All values should be finite
    EXPECT_TRUE(std::isfinite(state[0]));
    EXPECT_TRUE(std::isfinite(state[1]));
    EXPECT_TRUE(std::isfinite(state[2]));
}

TEST_F(DynamicalSystemsTest, StepRK4NullSystem) {
    float state[] = {1.0f, 0.0f, 0.0f};
    EXPECT_EQ(dynsys_step_rk4(nullptr, state, 0.01f), DYNSYS_ERR_NULL_PTR);
}

TEST_F(DynamicalSystemsTest, StepRK4NullState) {
    EXPECT_EQ(dynsys_step_rk4(sys_, nullptr, 0.01f), DYNSYS_ERR_NULL_PTR);
}

TEST_F(DynamicalSystemsTest, Integrate) {
    float state[] = {1.0f, 0.0f, 0.0f};

    EXPECT_EQ(dynsys_integrate(sys_, state, 100, nullptr), DYNSYS_OK);

    // After 100 steps, state should be on the Lorenz attractor
    EXPECT_TRUE(std::isfinite(state[0]));
    EXPECT_TRUE(std::isfinite(state[1]));
    EXPECT_TRUE(std::isfinite(state[2]));
}

TEST_F(DynamicalSystemsTest, IntegrateWithTrajectory) {
    float state[] = {1.0f, 0.0f, 0.0f};
    std::vector<float> trajectory(100 * 3);

    EXPECT_EQ(dynsys_integrate(sys_, state, 100, trajectory.data()), DYNSYS_OK);

    // Trajectory should have valid values
    EXPECT_TRUE(std::isfinite(trajectory[0]));
    EXPECT_TRUE(std::isfinite(trajectory[299]));  // Last state component
}

TEST_F(DynamicalSystemsTest, IntegrateNullSystem) {
    float state[] = {1.0f, 0.0f, 0.0f};
    EXPECT_EQ(dynsys_integrate(nullptr, state, 100, nullptr), DYNSYS_ERR_NULL_PTR);
}

TEST_F(DynamicalSystemsTest, IntegrateNullState) {
    EXPECT_EQ(dynsys_integrate(sys_, nullptr, 100, nullptr), DYNSYS_ERR_NULL_PTR);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, GetStats) {
    // Run some integrations first
    float state[] = {1.0f, 0.0f, 0.0f};
    dynsys_integrate(sys_, state, 100, nullptr);

    dynsys_stats_t stats;
    EXPECT_EQ(dynsys_get_stats(sys_, &stats), DYNSYS_OK);
    EXPECT_EQ(stats.integrations, 100u);
}

TEST_F(DynamicalSystemsTest, GetStatsNullSystem) {
    dynsys_stats_t stats;
    EXPECT_EQ(dynsys_get_stats(nullptr, &stats), DYNSYS_ERR_NULL_PTR);
}

TEST_F(DynamicalSystemsTest, GetStatsNullStats) {
    EXPECT_EQ(dynsys_get_stats(sys_, nullptr), DYNSYS_ERR_NULL_PTR);
}

//=============================================================================
// Lyapunov Exponent Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, LyapunovCreate) {
    dynsys_lyapunov_config_t config = dynsys_lyapunov_default_config();
    config.analysis_steps = 1000;  // Reduced for speed

    dynsys_lyapunov_t lyap = dynsys_lyapunov_create(&config, sys_);
    ASSERT_NE(lyap, nullptr);

    dynsys_lyapunov_destroy(lyap);
}

TEST_F(DynamicalSystemsTest, LyapunovCreateNullConfig) {
    dynsys_lyapunov_t lyap = dynsys_lyapunov_create(nullptr, sys_);
    EXPECT_EQ(lyap, nullptr);
}

TEST_F(DynamicalSystemsTest, LyapunovCreateNullSystem) {
    dynsys_lyapunov_config_t config = dynsys_lyapunov_default_config();
    dynsys_lyapunov_t lyap = dynsys_lyapunov_create(&config, nullptr);
    EXPECT_EQ(lyap, nullptr);
}

TEST_F(DynamicalSystemsTest, LyapunovDestroyNull) {
    dynsys_lyapunov_destroy(nullptr);  // Should not crash
}

TEST_F(DynamicalSystemsTest, LyapunovCompute) {
    dynsys_lyapunov_config_t config = dynsys_lyapunov_default_config();
    config.transient_steps = 100;
    config.analysis_steps = 500;  // Reduced for speed

    dynsys_lyapunov_t lyap = dynsys_lyapunov_create(&config, sys_);
    ASSERT_NE(lyap, nullptr);

    float initial_state[] = {1.0f, 0.0f, 0.0f};
    dynsys_lyapunov_result_t result;

    EXPECT_EQ(dynsys_lyapunov_compute(lyap, initial_state, &result), DYNSYS_OK);

    // Lorenz system should be chaotic (positive max Lyapunov)
    EXPECT_EQ(result.num_exponents, config.num_exponents);
    EXPECT_TRUE(result.is_chaotic);  // Lorenz is chaotic
    EXPECT_GT(result.max_lyapunov, 0.0f);

    dynsys_lyapunov_destroy(lyap);
}

TEST_F(DynamicalSystemsTest, LyapunovComputeNullParams) {
    dynsys_lyapunov_config_t config = dynsys_lyapunov_default_config();
    dynsys_lyapunov_t lyap = dynsys_lyapunov_create(&config, sys_);
    ASSERT_NE(lyap, nullptr);

    float initial_state[] = {1.0f, 0.0f, 0.0f};
    dynsys_lyapunov_result_t result;

    EXPECT_EQ(dynsys_lyapunov_compute(nullptr, initial_state, &result), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_lyapunov_compute(lyap, nullptr, &result), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_lyapunov_compute(lyap, initial_state, nullptr), DYNSYS_ERR_NULL_PTR);

    dynsys_lyapunov_destroy(lyap);
}

TEST_F(DynamicalSystemsTest, LyapunovMax) {
    dynsys_lyapunov_config_t config = dynsys_lyapunov_default_config();
    config.num_exponents = 1;
    config.transient_steps = 100;
    config.analysis_steps = 500;

    dynsys_lyapunov_t lyap = dynsys_lyapunov_create(&config, sys_);
    ASSERT_NE(lyap, nullptr);

    float initial_state[] = {1.0f, 0.0f, 0.0f};
    float max_exp = 0.0f;

    EXPECT_EQ(dynsys_lyapunov_max(lyap, initial_state, &max_exp), DYNSYS_OK);
    EXPECT_GT(max_exp, 0.0f);  // Lorenz is chaotic

    dynsys_lyapunov_destroy(lyap);
}

TEST_F(DynamicalSystemsTest, LyapunovMaxNullParams) {
    dynsys_lyapunov_config_t config = dynsys_lyapunov_default_config();
    dynsys_lyapunov_t lyap = dynsys_lyapunov_create(&config, sys_);
    ASSERT_NE(lyap, nullptr);

    float initial_state[] = {1.0f, 0.0f, 0.0f};
    float max_exp;

    EXPECT_EQ(dynsys_lyapunov_max(nullptr, initial_state, &max_exp), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_lyapunov_max(lyap, nullptr, &max_exp), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_lyapunov_max(lyap, initial_state, nullptr), DYNSYS_ERR_NULL_PTR);

    dynsys_lyapunov_destroy(lyap);
}

//=============================================================================
// Bifurcation Analysis Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, BifurcationCreate) {
    dynsys_bifurcation_config_t config = dynsys_bifurcation_default_config();

    dynsys_bifurcation_t bif = dynsys_bifurcation_create(&config, sys_);
    ASSERT_NE(bif, nullptr);

    dynsys_bifurcation_destroy(bif);
}

TEST_F(DynamicalSystemsTest, BifurcationCreateNullParams) {
    dynsys_bifurcation_config_t config = dynsys_bifurcation_default_config();

    EXPECT_EQ(dynsys_bifurcation_create(nullptr, sys_), nullptr);
    EXPECT_EQ(dynsys_bifurcation_create(&config, nullptr), nullptr);
}

TEST_F(DynamicalSystemsTest, BifurcationDestroyNull) {
    dynsys_bifurcation_destroy(nullptr);  // Should not crash
}

TEST_F(DynamicalSystemsTest, BifurcationScan) {
    dynsys_bifurcation_config_t config = dynsys_bifurcation_default_config();
    config.num_points = 10;  // Reduced for speed
    config.transient_steps = 50;
    config.sample_steps = 10;
    config.param_start = 20.0f;
    config.param_end = 30.0f;
    config.param_index = 1;  // rho parameter

    dynsys_bifurcation_t bif = dynsys_bifurcation_create(&config, sys_);
    ASSERT_NE(bif, nullptr);

    float initial_state[] = {1.0f, 0.0f, 0.0f};
    dynsys_bifurcation_result_t result;

    EXPECT_EQ(dynsys_bifurcation_scan(bif, initial_state, &result), DYNSYS_OK);

    EXPECT_EQ(result.num_param_points, config.num_points);
    EXPECT_EQ(result.samples_per_point, config.sample_steps);
    EXPECT_NE(result.parameter_values, nullptr);
    EXPECT_NE(result.state_samples, nullptr);

    dynsys_bifurcation_result_free(&result);
    dynsys_bifurcation_destroy(bif);
}

TEST_F(DynamicalSystemsTest, BifurcationScanNullParams) {
    dynsys_bifurcation_config_t config = dynsys_bifurcation_default_config();
    dynsys_bifurcation_t bif = dynsys_bifurcation_create(&config, sys_);
    ASSERT_NE(bif, nullptr);

    float initial_state[] = {1.0f, 0.0f, 0.0f};
    dynsys_bifurcation_result_t result;

    EXPECT_EQ(dynsys_bifurcation_scan(nullptr, initial_state, &result), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_bifurcation_scan(bif, nullptr, &result), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_bifurcation_scan(bif, initial_state, nullptr), DYNSYS_ERR_NULL_PTR);

    dynsys_bifurcation_destroy(bif);
}

TEST_F(DynamicalSystemsTest, BifurcationTypeName) {
    EXPECT_STREQ(dynsys_bifurcation_type_name(BIFURCATION_NONE), "none");
    EXPECT_STREQ(dynsys_bifurcation_type_name(BIFURCATION_SADDLE_NODE), "saddle-node");
    EXPECT_STREQ(dynsys_bifurcation_type_name(BIFURCATION_HOPF), "Hopf");
    EXPECT_STREQ(dynsys_bifurcation_type_name(BIFURCATION_PERIOD_DOUBLING), "period-doubling");
    EXPECT_STREQ(dynsys_bifurcation_type_name((bifurcation_type_t)9999), "unknown");
}

//=============================================================================
// Attractor Reconstruction Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, AttractorCreate) {
    dynsys_attractor_config_t config = dynsys_attractor_default_config();

    dynsys_attractor_t attr = dynsys_attractor_create(&config);
    ASSERT_NE(attr, nullptr);

    dynsys_attractor_destroy(attr);
}

TEST_F(DynamicalSystemsTest, AttractorCreateNullConfig) {
    EXPECT_EQ(dynsys_attractor_create(nullptr), nullptr);
}

TEST_F(DynamicalSystemsTest, AttractorDestroyNull) {
    dynsys_attractor_destroy(nullptr);  // Should not crash
}

TEST_F(DynamicalSystemsTest, AttractorReconstruct) {
    // Generate a time series from the Lorenz system
    float state[] = {1.0f, 0.0f, 0.0f};
    dynsys_integrate(sys_, state, 500, nullptr);  // Discard transient

    std::vector<float> time_series(1000);
    for (size_t i = 0; i < 1000; i++) {
        dynsys_step_rk4(sys_, state, 0.01f);
        time_series[i] = state[0];  // x component
    }

    dynsys_attractor_config_t config = dynsys_attractor_default_config();
    config.embedding_dim = 3;
    config.time_delay = 10;

    dynsys_attractor_t attr = dynsys_attractor_create(&config);
    ASSERT_NE(attr, nullptr);

    dynsys_attractor_result_t result;
    EXPECT_EQ(dynsys_attractor_reconstruct(attr, time_series.data(), 1000, &result), DYNSYS_OK);

    EXPECT_EQ(result.embedding_dim, config.embedding_dim);
    EXPECT_GT(result.num_points, 0u);
    EXPECT_NE(result.embedded_points, nullptr);
    EXPECT_EQ(result.type, ATTRACTOR_STRANGE);  // Lorenz is a strange attractor

    dynsys_attractor_result_free(&result);
    dynsys_attractor_destroy(attr);
}

TEST_F(DynamicalSystemsTest, AttractorReconstructNullParams) {
    dynsys_attractor_config_t config = dynsys_attractor_default_config();
    dynsys_attractor_t attr = dynsys_attractor_create(&config);
    ASSERT_NE(attr, nullptr);

    float time_series[100];
    dynsys_attractor_result_t result;

    EXPECT_EQ(dynsys_attractor_reconstruct(nullptr, time_series, 100, &result), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_attractor_reconstruct(attr, nullptr, 100, &result), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_attractor_reconstruct(attr, time_series, 100, nullptr), DYNSYS_ERR_NULL_PTR);

    dynsys_attractor_destroy(attr);
}

TEST_F(DynamicalSystemsTest, AttractorEstimateParams) {
    std::vector<float> time_series(1000);
    for (size_t i = 0; i < 1000; i++) {
        time_series[i] = sinf(0.1f * i);  // Simple oscillation
    }

    dynsys_attractor_config_t config = dynsys_attractor_default_config();
    dynsys_attractor_t attr = dynsys_attractor_create(&config);
    ASSERT_NE(attr, nullptr);

    uint32_t optimal_dim = 0;
    uint32_t optimal_delay = 0;

    EXPECT_EQ(dynsys_attractor_estimate_params(attr, time_series.data(), 1000,
              &optimal_dim, &optimal_delay), DYNSYS_OK);

    EXPECT_GT(optimal_dim, 0u);
    EXPECT_GT(optimal_delay, 0u);

    dynsys_attractor_destroy(attr);
}

TEST_F(DynamicalSystemsTest, AttractorTypeName) {
    EXPECT_STREQ(dynsys_attractor_type_name(ATTRACTOR_UNKNOWN), "unknown");
    EXPECT_STREQ(dynsys_attractor_type_name(ATTRACTOR_FIXED_POINT), "fixed point");
    EXPECT_STREQ(dynsys_attractor_type_name(ATTRACTOR_LIMIT_CYCLE), "limit cycle");
    EXPECT_STREQ(dynsys_attractor_type_name(ATTRACTOR_STRANGE), "strange attractor");
    EXPECT_STREQ(dynsys_attractor_type_name((attractor_type_t)9999), "unknown");
}

//=============================================================================
// Energy Landscape Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, EnergyCreate) {
    dynsys_energy_config_t config = dynsys_energy_default_config();

    dynsys_energy_t energy = dynsys_energy_create(&config, sys_);
    ASSERT_NE(energy, nullptr);

    dynsys_energy_destroy(energy);
}

TEST_F(DynamicalSystemsTest, EnergyCreateNullParams) {
    dynsys_energy_config_t config = dynsys_energy_default_config();

    EXPECT_EQ(dynsys_energy_create(nullptr, sys_), nullptr);
    EXPECT_EQ(dynsys_energy_create(&config, nullptr), nullptr);
}

TEST_F(DynamicalSystemsTest, EnergyDestroyNull) {
    dynsys_energy_destroy(nullptr);  // Should not crash
}

TEST_F(DynamicalSystemsTest, EnergyCompute) {
    dynsys_energy_config_t config = dynsys_energy_default_config();
    config.grid_resolution = 10;  // Reduced for speed

    dynsys_energy_t energy = dynsys_energy_create(&config, sys_);
    ASSERT_NE(energy, nullptr);

    dynsys_energy_result_t result;
    EXPECT_EQ(dynsys_energy_compute(energy, &result), DYNSYS_OK);

    EXPECT_EQ(result.grid_size, 100u);  // 10x10
    EXPECT_NE(result.energy_values, nullptr);
    EXPECT_GE(result.num_minima, 1u);

    dynsys_energy_result_free(&result);
    dynsys_energy_destroy(energy);
}

TEST_F(DynamicalSystemsTest, EnergyComputeNullParams) {
    dynsys_energy_config_t config = dynsys_energy_default_config();
    dynsys_energy_t energy = dynsys_energy_create(&config, sys_);
    ASSERT_NE(energy, nullptr);

    dynsys_energy_result_t result;

    EXPECT_EQ(dynsys_energy_compute(nullptr, &result), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_energy_compute(energy, nullptr), DYNSYS_ERR_NULL_PTR);

    dynsys_energy_destroy(energy);
}

TEST_F(DynamicalSystemsTest, EnergyFindMinimum) {
    dynsys_energy_config_t config = dynsys_energy_default_config();
    dynsys_energy_t energy = dynsys_energy_create(&config, sys_);
    ASSERT_NE(energy, nullptr);

    float initial_state[] = {1.0f, 1.0f, 1.0f};
    float minimum_state[3];
    float minimum_energy = 0.0f;

    EXPECT_EQ(dynsys_energy_find_minimum(energy, initial_state, minimum_state, &minimum_energy),
              DYNSYS_OK);

    EXPECT_TRUE(std::isfinite(minimum_energy));

    dynsys_energy_destroy(energy);
}

TEST_F(DynamicalSystemsTest, EnergyBarrier) {
    dynsys_energy_config_t config = dynsys_energy_default_config();
    dynsys_energy_t energy = dynsys_energy_create(&config, sys_);
    ASSERT_NE(energy, nullptr);

    float state_a[] = {-1.0f, -1.0f, -1.0f};
    float state_b[] = {1.0f, 1.0f, 1.0f};
    float barrier_height = 0.0f;
    float saddle_state[3];

    EXPECT_EQ(dynsys_energy_barrier(energy, state_a, state_b, &barrier_height, saddle_state),
              DYNSYS_OK);

    EXPECT_GE(barrier_height, 0.0f);

    dynsys_energy_destroy(energy);
}

//=============================================================================
// Slow-Fast Decomposition Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, SlowfastCreate) {
    dynsys_slowfast_config_t config = dynsys_slowfast_default_config();

    dynsys_slowfast_t sf = dynsys_slowfast_create(&config, sys_);
    ASSERT_NE(sf, nullptr);

    dynsys_slowfast_destroy(sf);
}

TEST_F(DynamicalSystemsTest, SlowfastCreateNullParams) {
    dynsys_slowfast_config_t config = dynsys_slowfast_default_config();

    EXPECT_EQ(dynsys_slowfast_create(nullptr, sys_), nullptr);
    EXPECT_EQ(dynsys_slowfast_create(&config, nullptr), nullptr);
}

TEST_F(DynamicalSystemsTest, SlowfastDestroyNull) {
    dynsys_slowfast_destroy(nullptr);  // Should not crash
}

TEST_F(DynamicalSystemsTest, SlowfastCompute) {
    dynsys_slowfast_config_t config = dynsys_slowfast_default_config();
    config.epsilon = 0.1f;

    dynsys_slowfast_t sf = dynsys_slowfast_create(&config, sys_);
    ASSERT_NE(sf, nullptr);

    dynsys_slowfast_result_t result;
    EXPECT_EQ(dynsys_slowfast_compute(sf, &result), DYNSYS_OK);

    EXPECT_GT(result.timescale_ratio, 1.0f);
    EXPECT_TRUE(result.manifold_exists);

    dynsys_slowfast_result_free(&result);
    dynsys_slowfast_destroy(sf);
}

TEST_F(DynamicalSystemsTest, SlowfastProject) {
    dynsys_slowfast_config_t config = dynsys_slowfast_default_config();
    dynsys_slowfast_t sf = dynsys_slowfast_create(&config, sys_);
    ASSERT_NE(sf, nullptr);

    float state[] = {1.0f, 2.0f, 3.0f};
    float projected_state[3];

    EXPECT_EQ(dynsys_slowfast_project(sf, state, projected_state), DYNSYS_OK);

    dynsys_slowfast_destroy(sf);
}

//=============================================================================
// Bridge Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, BridgeCreate) {
    dynsys_bridge_config_t config = dynsys_bridge_default_config();

    dynsys_bridge_t bridge = dynsys_bridge_create(&config, sys_);
    ASSERT_NE(bridge, nullptr);

    dynsys_bridge_destroy(bridge);
}

TEST_F(DynamicalSystemsTest, BridgeCreateNullParams) {
    dynsys_bridge_config_t config = dynsys_bridge_default_config();

    EXPECT_EQ(dynsys_bridge_create(nullptr, sys_), nullptr);
    EXPECT_EQ(dynsys_bridge_create(&config, nullptr), nullptr);
}

TEST_F(DynamicalSystemsTest, BridgeDestroyNull) {
    dynsys_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(DynamicalSystemsTest, BridgeInitAndShutdown) {
    dynsys_bridge_config_t config = dynsys_bridge_default_config();
    dynsys_bridge_t bridge = dynsys_bridge_create(&config, sys_);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dynsys_bridge_init(bridge, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(dynsys_bridge_shutdown(bridge), 0);

    dynsys_bridge_destroy(bridge);
}

TEST_F(DynamicalSystemsTest, BridgeInitNullBridge) {
    EXPECT_EQ(dynsys_bridge_init(nullptr, nullptr, nullptr, nullptr), -1);
}

TEST_F(DynamicalSystemsTest, BridgeShutdownNullBridge) {
    EXPECT_EQ(dynsys_bridge_shutdown(nullptr), -1);
}

TEST_F(DynamicalSystemsTest, BridgeRegisterExceptionHandler) {
    dynsys_bridge_config_t config = dynsys_bridge_default_config();
    dynsys_bridge_t bridge = dynsys_bridge_create(&config, sys_);
    ASSERT_NE(bridge, nullptr);

    void* dummy_handler = (void*)0x12345678;
    EXPECT_EQ(dynsys_bridge_register_exception_handler(bridge, (dynsys_exception_handler_t)dummy_handler), 0);

    dynsys_bridge_destroy(bridge);
}

TEST_F(DynamicalSystemsTest, BridgeRegisterExceptionHandlerNullBridge) {
    EXPECT_EQ(dynsys_bridge_register_exception_handler(nullptr, nullptr), -1);
}

TEST_F(DynamicalSystemsTest, BridgeRaiseException) {
    dynsys_bridge_config_t config = dynsys_bridge_default_config();
    dynsys_bridge_t bridge = dynsys_bridge_create(&config, sys_);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dynsys_bridge_raise_exception(bridge, DYNSYS_EXC_INTEGRATION_DIVERGED,
              "Test exception", nullptr), 0);

    dynsys_bridge_destroy(bridge);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(DynamicalSystemsTest, ErrorString) {
    EXPECT_STREQ(dynsys_error_string(DYNSYS_OK), "Success");
    EXPECT_STREQ(dynsys_error_string(DYNSYS_ERR_NULL_PTR), "Null pointer");
    EXPECT_STREQ(dynsys_error_string(DYNSYS_ERR_INVALID_DIM), "Invalid dimension");
    EXPECT_STREQ(dynsys_error_string(DYNSYS_ERR_DIVERGENCE), "Numerical divergence");
    EXPECT_STREQ(dynsys_error_string(DYNSYS_ERR_NO_MEMORY), "Memory allocation failed");
    EXPECT_STREQ(dynsys_error_string((dynsys_error_t)9999), "Unknown error");
}

TEST_F(DynamicalSystemsTest, NumericalJacobian) {
    float state[] = {1.0f, 2.0f, 3.0f};
    std::vector<float> jacobian(9);  // 3x3

    EXPECT_EQ(dynsys_numerical_jacobian(sys_, state, jacobian.data(), 1e-5f), DYNSYS_OK);

    // Jacobian should have finite values
    for (size_t i = 0; i < 9; i++) {
        EXPECT_TRUE(std::isfinite(jacobian[i]));
    }
}

TEST_F(DynamicalSystemsTest, NumericalJacobianNullParams) {
    float state[] = {1.0f, 2.0f, 3.0f};
    float jacobian[9];

    EXPECT_EQ(dynsys_numerical_jacobian(nullptr, state, jacobian, 1e-5f), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_numerical_jacobian(sys_, nullptr, jacobian, 1e-5f), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_numerical_jacobian(sys_, state, nullptr, 1e-5f), DYNSYS_ERR_NULL_PTR);
}

TEST_F(DynamicalSystemsTest, Eigenvalues) {
    // 2x2 matrix with known eigenvalues
    float matrix[] = {2.0f, 1.0f, 0.0f, 3.0f};  // Eigenvalues: 2 and 3
    float eigenvalues_real[2];
    float eigenvalues_imag[2];

    EXPECT_EQ(dynsys_eigenvalues(matrix, 2, eigenvalues_real, eigenvalues_imag), DYNSYS_OK);

    // At least eigenvalues should be computed
    EXPECT_TRUE(std::isfinite(eigenvalues_real[0]));
    EXPECT_TRUE(std::isfinite(eigenvalues_real[1]));
}

TEST_F(DynamicalSystemsTest, EigenvaluesNullParams) {
    float matrix[4] = {0};
    float eigenvalues_real[2];
    float eigenvalues_imag[2];

    EXPECT_EQ(dynsys_eigenvalues(nullptr, 2, eigenvalues_real, eigenvalues_imag), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_eigenvalues(matrix, 2, nullptr, eigenvalues_imag), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_eigenvalues(matrix, 2, eigenvalues_real, nullptr), DYNSYS_ERR_NULL_PTR);
}

//=============================================================================
// Harmonic Oscillator Test (Simple Non-Chaotic System)
//=============================================================================

TEST(HarmonicOscillatorTest, PeriodicBehavior) {
    dynsys_config_t config = dynsys_default_config();
    config.state_dim = 2;
    config.param_dim = 1;
    config.dt = 0.01f;

    dynsys_system_t sys = dynsys_create(&config, harmonic_oscillator, nullptr, nullptr);
    ASSERT_NE(sys, nullptr);

    float omega = 1.0f;
    dynsys_set_params(sys, &omega, 1);

    // Initial conditions: x=1, v=0
    float state[] = {1.0f, 0.0f};
    float initial_x = state[0];

    // Integrate for one period (T = 2*pi/omega)
    uint32_t steps_per_period = (uint32_t)(2.0f * M_PI / (omega * config.dt));
    dynsys_integrate(sys, state, steps_per_period, nullptr);

    // After one period, should return close to initial state
    EXPECT_NEAR(state[0], initial_x, 0.1f);
    EXPECT_NEAR(state[1], 0.0f, 0.1f);

    dynsys_destroy(sys);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
