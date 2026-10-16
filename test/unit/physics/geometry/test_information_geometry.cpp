/**
 * @file test_information_geometry.cpp
 * @brief Unit tests for Information Geometry module
 *
 * WHAT: Test suite for nimcp_information_geometry
 * WHY:  Verify Fisher information, natural gradient, and manifold operations
 * HOW:  Unit tests for create, compute, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "physics/geometry/nimcp_information_geometry.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class InformationGeometryTest : public ::testing::Test {
protected:
    nimcp_info_geometry_t geom = nullptr;

    void SetUp() override {
        nimcp_info_geom_config_t config = nimcp_info_geom_default_config();
        geom = nimcp_info_geom_create(&config);
        ASSERT_NE(geom, nullptr);
    }

    void TearDown() override {
        if (geom) {
            nimcp_info_geom_destroy(geom);
            geom = nullptr;
        }
    }
};

class FisherInfoTest : public ::testing::Test {
protected:
    nimcp_fisher_info_t fisher = nullptr;
    static constexpr uint32_t FISHER_DIM = 16;

    void SetUp() override {
        nimcp_fisher_config_t config = nimcp_fisher_default_config();
        config.param_dim = FISHER_DIM;  /* Match test data dimension */
        fisher = nimcp_fisher_create(&config);
        ASSERT_NE(fisher, nullptr);
    }

    void TearDown() override {
        if (fisher) {
            nimcp_fisher_destroy(fisher);
            fisher = nullptr;
        }
    }
};

class NaturalGradientTest : public ::testing::Test {
protected:
    nimcp_natural_gradient_t ng = nullptr;
    static constexpr uint32_t PARAM_DIM = 32;

    void SetUp() override {
        nimcp_natural_grad_config_t config = nimcp_natural_grad_default_config();
        ng = nimcp_natural_grad_create(&config, PARAM_DIM);
        ASSERT_NE(ng, nullptr);
    }

    void TearDown() override {
        if (ng) {
            nimcp_natural_grad_destroy(ng);
            ng = nullptr;
        }
    }
};

class NeuralManifoldTest : public ::testing::Test {
protected:
    nimcp_neural_manifold_t manifold = nullptr;
    static constexpr uint32_t AMBIENT_DIM = 64;

    void SetUp() override {
        nimcp_manifold_config_t config = nimcp_manifold_default_config();
        manifold = nimcp_manifold_create(&config, AMBIENT_DIM);
        ASSERT_NE(manifold, nullptr);
    }

    void TearDown() override {
        if (manifold) {
            nimcp_manifold_destroy(manifold);
            manifold = nullptr;
        }
    }
};

//=============================================================================
// Information Geometry Creation Tests
//=============================================================================

TEST(InfoGeomCreateTest, CreateWithDefaultConfig) {
    nimcp_info_geometry_t g = nimcp_info_geom_create(nullptr);
    ASSERT_NE(g, nullptr);
    nimcp_info_geom_destroy(g);
}

TEST(InfoGeomCreateTest, CreateWithCustomConfig) {
    nimcp_info_geom_config_t config = nimcp_info_geom_default_config();
    config.latent_dim = 32;
    config.ambient_dim = 256;
    config.regularization = 1e-5f;
    config.learning_rate = 0.001f;
    config.enable_ema = true;
    config.ema_decay = 0.99f;

    nimcp_info_geometry_t g = nimcp_info_geom_create(&config);
    ASSERT_NE(g, nullptr);
    nimcp_info_geom_destroy(g);
}

TEST(InfoGeomCreateTest, DestroyNull) {
    /* Should not crash */
    nimcp_info_geom_destroy(nullptr);
}

TEST(InfoGeomCreateTest, DefaultConfigValues) {
    nimcp_info_geom_config_t config = nimcp_info_geom_default_config();

    EXPECT_EQ(config.latent_dim, INFO_GEOM_DEFAULT_LATENT_DIM);
    EXPECT_GT(config.regularization, 0.0f);
    EXPECT_GT(config.learning_rate, 0.0f);
}

//=============================================================================
// Information Geometry State Tests
//=============================================================================

TEST_F(InformationGeometryTest, GetStateSuccess) {
    nimcp_info_geom_state_t state;
    nimcp_info_geom_error_t err = nimcp_info_geom_get_state(geom, &state);
    EXPECT_EQ(err, INFO_GEOM_OK);
}

TEST_F(InformationGeometryTest, GetStateNullPtr) {
    nimcp_info_geom_error_t err = nimcp_info_geom_get_state(geom, nullptr);
    EXPECT_EQ(err, INFO_GEOM_ERR_NULL_PTR);
}

TEST_F(InformationGeometryTest, GetStatsSuccess) {
    nimcp_info_geom_stats_t stats;
    nimcp_info_geom_error_t err = nimcp_info_geom_get_stats(geom, &stats);
    EXPECT_EQ(err, INFO_GEOM_OK);
    EXPECT_EQ(stats.updates, 0u);
}

TEST_F(InformationGeometryTest, ResetStatsSuccess) {
    nimcp_info_geom_error_t err = nimcp_info_geom_reset_stats(geom);
    EXPECT_EQ(err, INFO_GEOM_OK);
}

TEST_F(InformationGeometryTest, ResetStatsNull) {
    nimcp_info_geom_error_t err = nimcp_info_geom_reset_stats(nullptr);
    EXPECT_EQ(err, INFO_GEOM_ERR_NULL_PTR);
}

//=============================================================================
// Fisher Information Computation Tests
//=============================================================================

TEST_F(InformationGeometryTest, ComputeFisherSuccess) {
    /* Create a simple probability distribution */
    std::vector<float> distribution(16);
    float sum = 0.0f;
    for (size_t i = 0; i < distribution.size(); i++) {
        distribution[i] = static_cast<float>(i + 1);
        sum += distribution[i];
    }
    /* Normalize to probability distribution */
    for (size_t i = 0; i < distribution.size(); i++) {
        distribution[i] /= sum;
    }

    nimcp_info_geom_error_t err = nimcp_info_geom_compute_fisher(
        geom, distribution.data(), static_cast<uint32_t>(distribution.size()));
    EXPECT_EQ(err, INFO_GEOM_OK);
}

TEST_F(InformationGeometryTest, ComputeFisherNullPtr) {
    nimcp_info_geom_error_t err = nimcp_info_geom_compute_fisher(geom, nullptr, 16);
    EXPECT_EQ(err, INFO_GEOM_ERR_NULL_PTR);
}

TEST_F(InformationGeometryTest, ComputeFisherInvalidDim) {
    std::vector<float> distribution(1024);
    /* Zero size should fail */
    nimcp_info_geom_error_t err = nimcp_info_geom_compute_fisher(geom, distribution.data(), 0);
    EXPECT_EQ(err, INFO_GEOM_ERR_INVALID_DIM);
}

//=============================================================================
// Natural Gradient Tests
//=============================================================================

TEST_F(InformationGeometryTest, NaturalGradientSuccess) {
    /* First compute Fisher */
    std::vector<float> distribution(16, 1.0f / 16.0f);
    nimcp_info_geom_compute_fisher(geom, distribution.data(), 16);

    /* Then compute natural gradient */
    std::vector<float> gradient(16, 0.1f);
    std::vector<float> natural_grad(16, 0.0f);

    nimcp_info_geom_error_t err = nimcp_info_geom_natural_gradient(
        geom, gradient.data(), natural_grad.data(), 16);
    EXPECT_EQ(err, INFO_GEOM_OK);

    /* Natural gradient should be different from standard gradient */
    bool different = false;
    for (size_t i = 0; i < 16; i++) {
        if (std::abs(natural_grad[i] - gradient[i]) > 1e-6f) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);
}

TEST_F(InformationGeometryTest, NaturalGradientNullPtrs) {
    std::vector<float> gradient(16, 0.1f);
    std::vector<float> natural_grad(16, 0.0f);

    EXPECT_EQ(nimcp_info_geom_natural_gradient(geom, nullptr, natural_grad.data(), 16),
              INFO_GEOM_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_info_geom_natural_gradient(geom, gradient.data(), nullptr, 16),
              INFO_GEOM_ERR_NULL_PTR);
}

//=============================================================================
// Parameter Update Tests
//=============================================================================

TEST_F(InformationGeometryTest, UpdateSuccess) {
    /* Set up Fisher information */
    std::vector<float> distribution(16, 1.0f / 16.0f);
    nimcp_info_geom_compute_fisher(geom, distribution.data(), 16);

    /* Update parameters */
    std::vector<float> parameters(16, 0.5f);
    std::vector<float> gradient(16, 0.1f);

    nimcp_info_geom_error_t err = nimcp_info_geom_update(
        geom, parameters.data(), gradient.data(), 16, 0.01f);
    EXPECT_EQ(err, INFO_GEOM_OK);

    /* Parameters should have changed */
    bool changed = false;
    for (size_t i = 0; i < 16; i++) {
        if (std::abs(parameters[i] - 0.5f) > 1e-8f) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed);
}

//=============================================================================
// Geodesic Distance Tests
//=============================================================================

TEST_F(InformationGeometryTest, GeodesicDistanceSuccess) {
    std::vector<float> point_a(16, 0.0f);
    std::vector<float> point_b(16, 1.0f);
    float distance = 0.0f;

    nimcp_info_geom_error_t err = nimcp_info_geom_geodesic_distance(
        geom, point_a.data(), point_b.data(), 16, &distance);
    EXPECT_EQ(err, INFO_GEOM_OK);
    EXPECT_GT(distance, 0.0f);
}

TEST_F(InformationGeometryTest, GeodesicDistanceZeroForSamePoint) {
    std::vector<float> point(16, 0.5f);
    float distance = -1.0f;

    nimcp_info_geom_error_t err = nimcp_info_geom_geodesic_distance(
        geom, point.data(), point.data(), 16, &distance);
    EXPECT_EQ(err, INFO_GEOM_OK);
    EXPECT_NEAR(distance, 0.0f, 1e-6f);
}

TEST_F(InformationGeometryTest, GeodesicDistanceNullPtrs) {
    std::vector<float> point(16, 0.0f);
    float distance = 0.0f;

    EXPECT_EQ(nimcp_info_geom_geodesic_distance(geom, nullptr, point.data(), 16, &distance),
              INFO_GEOM_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_info_geom_geodesic_distance(geom, point.data(), nullptr, 16, &distance),
              INFO_GEOM_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_info_geom_geodesic_distance(geom, point.data(), point.data(), 16, nullptr),
              INFO_GEOM_ERR_NULL_PTR);
}

//=============================================================================
// KL Divergence Tests
//=============================================================================

TEST_F(InformationGeometryTest, KLDivergenceSuccess) {
    /* Create two probability distributions */
    std::vector<float> p(16, 1.0f / 16.0f);  /* Uniform */
    std::vector<float> q(16);  /* Non-uniform */
    float sum = 0.0f;
    for (size_t i = 0; i < 16; i++) {
        q[i] = static_cast<float>(i + 1);
        sum += q[i];
    }
    for (size_t i = 0; i < 16; i++) {
        q[i] /= sum;
    }

    float kl_div = 0.0f;
    nimcp_info_geom_error_t err = nimcp_info_geom_kl_divergence(
        geom, p.data(), q.data(), 16, &kl_div);
    EXPECT_EQ(err, INFO_GEOM_OK);
    EXPECT_GE(kl_div, 0.0f);  /* KL divergence is non-negative */
}

TEST_F(InformationGeometryTest, KLDivergenceZeroForIdentical) {
    std::vector<float> p(16, 1.0f / 16.0f);
    float kl_div = -1.0f;

    nimcp_info_geom_error_t err = nimcp_info_geom_kl_divergence(
        geom, p.data(), p.data(), 16, &kl_div);
    EXPECT_EQ(err, INFO_GEOM_OK);
    EXPECT_NEAR(kl_div, 0.0f, 1e-6f);
}

//=============================================================================
// Fisher Info Component Tests
//=============================================================================

TEST(FisherCreateTest, CreateWithDefaultConfig) {
    nimcp_fisher_info_t f = nimcp_fisher_create(nullptr);
    ASSERT_NE(f, nullptr);
    nimcp_fisher_destroy(f);
}

TEST(FisherCreateTest, CreateWithCustomConfig) {
    nimcp_fisher_config_t config = nimcp_fisher_default_config();
    config.param_dim = 64;
    config.sample_size = 100;
    config.use_empirical = true;

    nimcp_fisher_info_t f = nimcp_fisher_create(&config);
    ASSERT_NE(f, nullptr);
    nimcp_fisher_destroy(f);
}

TEST(FisherCreateTest, DestroyNull) {
    nimcp_fisher_destroy(nullptr);
}

TEST_F(FisherInfoTest, ComputeSuccess) {
    /* Create gradient samples */
    const uint32_t num_samples = 10;
    std::vector<float> gradients(num_samples * FISHER_DIM);

    /* Fill with random-ish gradients */
    for (size_t i = 0; i < gradients.size(); i++) {
        gradients[i] = static_cast<float>(i % 7) * 0.1f - 0.3f;
    }

    nimcp_info_geom_error_t err = nimcp_fisher_compute(
        fisher, gradients.data(), num_samples, FISHER_DIM);
    EXPECT_EQ(err, INFO_GEOM_OK);
}

TEST_F(FisherInfoTest, ComputeNullPtr) {
    nimcp_info_geom_error_t err = nimcp_fisher_compute(fisher, nullptr, 10, 16);
    EXPECT_EQ(err, INFO_GEOM_ERR_NULL_PTR);
}

TEST_F(FisherInfoTest, SolveSuccess) {
    /* First compute Fisher */
    std::vector<float> gradients(10 * FISHER_DIM);
    for (size_t i = 0; i < gradients.size(); i++) {
        gradients[i] = static_cast<float>(i % 5) * 0.2f - 0.4f;
    }
    nimcp_fisher_compute(fisher, gradients.data(), 10, FISHER_DIM);

    /* Solve F*x = b */
    std::vector<float> b(FISHER_DIM, 1.0f);
    std::vector<float> x(FISHER_DIM, 0.0f);

    nimcp_info_geom_error_t err = nimcp_fisher_solve(fisher, b.data(), x.data(), FISHER_DIM);
    EXPECT_EQ(err, INFO_GEOM_OK);
}

TEST_F(FisherInfoTest, GetMatrixSuccess) {
    /* First compute Fisher */
    std::vector<float> gradients(10 * FISHER_DIM);
    for (size_t i = 0; i < gradients.size(); i++) {
        gradients[i] = static_cast<float>(i % 3) * 0.15f;
    }
    nimcp_fisher_compute(fisher, gradients.data(), 10, FISHER_DIM);

    /* Get the Fisher matrix - size is dim*dim */
    std::vector<float> matrix(FISHER_DIM * FISHER_DIM);
    nimcp_info_geom_error_t err = nimcp_fisher_get_matrix(
        fisher, matrix.data(), FISHER_DIM * FISHER_DIM, false);
    EXPECT_EQ(err, INFO_GEOM_OK);

    /* Check diagonal elements are positive (regularization ensures this) */
    for (uint32_t i = 0; i < FISHER_DIM; i++) {
        EXPECT_GE(matrix[i * FISHER_DIM + i], 0.0f);
    }
}

//=============================================================================
// Natural Gradient Component Tests
//=============================================================================

TEST(NaturalGradCreateTest, CreateWithDefaultConfig) {
    nimcp_natural_gradient_t n = nimcp_natural_grad_create(nullptr, 32);
    ASSERT_NE(n, nullptr);
    nimcp_natural_grad_destroy(n);
}

TEST(NaturalGradCreateTest, CreateWithCustomConfig) {
    nimcp_natural_grad_config_t config = nimcp_natural_grad_default_config();
    config.learning_rate = 0.01f;
    config.momentum = 0.9f;
    config.enable_warmup = true;
    config.warmup_steps = 100;

    nimcp_natural_gradient_t n = nimcp_natural_grad_create(&config, 64);
    ASSERT_NE(n, nullptr);
    nimcp_natural_grad_destroy(n);
}

TEST(NaturalGradCreateTest, DestroyNull) {
    nimcp_natural_grad_destroy(nullptr);
}

TEST_F(NaturalGradientTest, GetLearningRate) {
    /* With warmup enabled by default, initial LR is 0.0f (warms up over time) */
    float lr = nimcp_natural_grad_get_lr(ng);
    EXPECT_GE(lr, 0.0f);  /* Learning rate is non-negative */
}

TEST_F(NaturalGradientTest, GetLearningRateNull) {
    float lr = nimcp_natural_grad_get_lr(nullptr);
    EXPECT_EQ(lr, 0.0f);
}

TEST_F(NaturalGradientTest, StepSuccess) {
    std::vector<float> parameters(PARAM_DIM, 0.5f);
    std::vector<float> gradient(PARAM_DIM, 0.1f);

    nimcp_info_geom_error_t err = nimcp_natural_grad_step(
        ng, parameters.data(), gradient.data(), PARAM_DIM);
    EXPECT_EQ(err, INFO_GEOM_OK);
}

TEST_F(NaturalGradientTest, StepNullPtrs) {
    std::vector<float> data(PARAM_DIM, 0.1f);

    EXPECT_EQ(nimcp_natural_grad_step(ng, nullptr, data.data(), PARAM_DIM),
              INFO_GEOM_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_natural_grad_step(ng, data.data(), nullptr, PARAM_DIM),
              INFO_GEOM_ERR_NULL_PTR);
}

//=============================================================================
// Neural Manifold Component Tests
//=============================================================================

TEST(ManifoldCreateTest, CreateWithDefaultConfig) {
    nimcp_neural_manifold_t m = nimcp_manifold_create(nullptr, 64);
    ASSERT_NE(m, nullptr);
    nimcp_manifold_destroy(m);
}

TEST(ManifoldCreateTest, CreateWithCustomConfig) {
    nimcp_manifold_config_t config = nimcp_manifold_default_config();
    config.intrinsic_dim = 8;
    config.num_samples = 200;
    config.compute_curvature = true;
    config.enable_embedding = true;

    nimcp_neural_manifold_t m = nimcp_manifold_create(&config, 128);
    ASSERT_NE(m, nullptr);
    nimcp_manifold_destroy(m);
}

TEST(ManifoldCreateTest, DestroyNull) {
    nimcp_manifold_destroy(nullptr);
}

TEST_F(NeuralManifoldTest, AddSamplesSuccess) {
    const uint32_t num_samples = 20;
    std::vector<float> samples(num_samples * AMBIENT_DIM);

    /* Generate samples on a manifold (e.g., circle embedded in high-D space) */
    for (uint32_t i = 0; i < num_samples; i++) {
        float theta = 2.0f * 3.14159f * static_cast<float>(i) / num_samples;
        samples[i * AMBIENT_DIM + 0] = std::cos(theta);
        samples[i * AMBIENT_DIM + 1] = std::sin(theta);
        for (uint32_t j = 2; j < AMBIENT_DIM; j++) {
            samples[i * AMBIENT_DIM + j] = 0.0f;
        }
    }

    nimcp_info_geom_error_t err = nimcp_manifold_add_samples(
        manifold, samples.data(), num_samples, AMBIENT_DIM);
    EXPECT_EQ(err, INFO_GEOM_OK);
}

TEST_F(NeuralManifoldTest, AddSamplesNullPtr) {
    nimcp_info_geom_error_t err = nimcp_manifold_add_samples(manifold, nullptr, 10, AMBIENT_DIM);
    EXPECT_EQ(err, INFO_GEOM_ERR_NULL_PTR);
}

TEST_F(NeuralManifoldTest, EstimateDimSuccess) {
    /* Add some samples first */
    const uint32_t num_samples = 50;
    std::vector<float> samples(num_samples * AMBIENT_DIM);
    for (size_t i = 0; i < samples.size(); i++) {
        samples[i] = static_cast<float>(i % 13) * 0.1f;
    }
    nimcp_manifold_add_samples(manifold, samples.data(), num_samples, AMBIENT_DIM);

    uint32_t intrinsic_dim = 0;
    nimcp_info_geom_error_t err = nimcp_manifold_estimate_dim(manifold, &intrinsic_dim);
    EXPECT_EQ(err, INFO_GEOM_OK);
    EXPECT_GT(intrinsic_dim, 0u);
}

TEST_F(NeuralManifoldTest, CurvatureSuccess) {
    /* Add samples */
    const uint32_t num_samples = 30;
    std::vector<float> samples(num_samples * AMBIENT_DIM);
    for (size_t i = 0; i < samples.size(); i++) {
        samples[i] = static_cast<float>(i % 7) * 0.15f;
    }
    nimcp_manifold_add_samples(manifold, samples.data(), num_samples, AMBIENT_DIM);

    /* Compute curvature at a point */
    std::vector<float> point(AMBIENT_DIM, 0.1f);
    float curvature = 0.0f;

    nimcp_info_geom_error_t err = nimcp_manifold_curvature(
        manifold, point.data(), AMBIENT_DIM, &curvature);
    EXPECT_EQ(err, INFO_GEOM_OK);
}

TEST_F(NeuralManifoldTest, ProjectSuccess) {
    /* Add samples */
    const uint32_t num_samples = 30;
    std::vector<float> samples(num_samples * AMBIENT_DIM);
    for (size_t i = 0; i < samples.size(); i++) {
        samples[i] = static_cast<float>(i % 11) * 0.12f;
    }
    nimcp_manifold_add_samples(manifold, samples.data(), num_samples, AMBIENT_DIM);

    /* Project a point */
    std::vector<float> point(AMBIENT_DIM, 0.5f);
    std::vector<float> projected(AMBIENT_DIM, 0.0f);

    nimcp_info_geom_error_t err = nimcp_manifold_project(
        manifold, point.data(), projected.data(), AMBIENT_DIM);
    EXPECT_EQ(err, INFO_GEOM_OK);
}

TEST_F(NeuralManifoldTest, GeodesicSuccess) {
    /* Add samples */
    const uint32_t num_samples = 30;
    std::vector<float> samples(num_samples * AMBIENT_DIM);
    for (size_t i = 0; i < samples.size(); i++) {
        samples[i] = static_cast<float>(i % 9) * 0.11f;
    }
    nimcp_manifold_add_samples(manifold, samples.data(), num_samples, AMBIENT_DIM);

    /* Compute geodesic path */
    std::vector<float> start(AMBIENT_DIM, 0.0f);
    std::vector<float> end(AMBIENT_DIM, 1.0f);
    const uint32_t path_steps = 10;
    std::vector<float> path(path_steps * AMBIENT_DIM, 0.0f);

    nimcp_info_geom_error_t err = nimcp_manifold_geodesic(
        manifold, start.data(), end.data(), path.data(), path_steps, AMBIENT_DIM);
    EXPECT_EQ(err, INFO_GEOM_OK);
}

//=============================================================================
// Error String Tests
//=============================================================================

TEST(ErrorStringTest, AllErrorsHaveStrings) {
    EXPECT_NE(nimcp_info_geom_error_string(INFO_GEOM_OK), nullptr);
    EXPECT_NE(nimcp_info_geom_error_string(INFO_GEOM_ERR_NULL_PTR), nullptr);
    EXPECT_NE(nimcp_info_geom_error_string(INFO_GEOM_ERR_INVALID_DIM), nullptr);
    EXPECT_NE(nimcp_info_geom_error_string(INFO_GEOM_ERR_SINGULAR_MATRIX), nullptr);
    EXPECT_NE(nimcp_info_geom_error_string(INFO_GEOM_ERR_NOT_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_info_geom_error_string(INFO_GEOM_ERR_ALREADY_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_info_geom_error_string(INFO_GEOM_ERR_NO_MEMORY), nullptr);
    EXPECT_NE(nimcp_info_geom_error_string(INFO_GEOM_ERR_COMPUTATION), nullptr);
}

TEST(ErrorStringTest, UnknownErrorCode) {
    const char* str = nimcp_info_geom_error_string((nimcp_info_geom_error_t)-999);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
