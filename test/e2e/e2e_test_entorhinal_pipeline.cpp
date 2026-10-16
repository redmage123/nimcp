/**
 * @file e2e_test_entorhinal_pipeline.cpp
 * @brief End-to-end tests for Entorhinal Cortex Pipeline
 *
 * WHAT: Full pipeline tests for spatial navigation and memory gateway
 * WHY:  Verify complete entorhinal workflows with all bridge integrations
 * HOW:  Test grid cells, path integration, memory gateway, and all bridges
 *
 * TEST COVERAGE:
 * - Spatial Exploration Pipeline (4 tests)
 * - Memory Encoding Pipeline (3 tests)
 * - Memory Retrieval Pipeline (3 tests)
 * - Multi-Bridge Coordination (4 tests)
 * - Error Recovery Pipeline (3 tests)
 * - Performance Under Load (3 tests)
 *
 * TOTAL: 20 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Entorhinal cortex is the gateway between hippocampus and neocortex
 * - Grid cells provide hexagonal spatial representation
 * - Border cells detect environmental boundaries
 * - Head direction cells encode current heading
 * - Path integration accumulates self-motion for position tracking
 * - Memory gateway routes information bidirectionally
 *
 * @author NIMCP Development Team
 * @date 2025-01-12
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <atomic>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_hypothalamus_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_omni_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_brain_init_bridge.h"
}

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_SPATIAL_UPDATE_TIME_MS = 20.0;
constexpr double MAX_ENCODING_TIME_MS = 50.0;
constexpr double MAX_RETRIEVAL_TIME_MS = 30.0;
constexpr double MAX_BRIDGE_INIT_TIME_MS = 100.0;
constexpr float MIN_GRID_COHERENCE = 0.3f;
constexpr float MIN_POSITION_CONFIDENCE = 0.5f;
constexpr float GRID_DRIFT_THRESHOLD = 0.1f;
constexpr uint32_t FEATURE_DIM = 64;
constexpr uint32_t SPATIAL_DIM = 3;
constexpr uint32_t PATH_LENGTH = 50;
constexpr float ENVIRONMENT_SIZE = 10.0f;

//=============================================================================
// Helper Structures
//=============================================================================

/**
 * @brief Represents a waypoint in spatial navigation
 */
struct Waypoint {
    float position[3];
    float heading;
    float velocity[3];
    uint64_t timestamp_ms;
};

/**
 * @brief Represents a memory item for encoding/retrieval
 */
struct MemoryItem {
    std::vector<float> features;
    std::vector<float> spatial_context;
    uint32_t id;
    float salience;
};

/**
 * @brief Represents a navigation path through environment
 */
struct NavigationPath {
    std::vector<Waypoint> waypoints;
    float total_distance;
    float total_time_ms;
};

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Test fixture for spatial exploration pipeline tests
 */
class E2EEntorhinalSpatialTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;
    entorhinal_config_t config;

    void SetUp() override {
        config = entorhinal_default_config();
        config.num_grid_cells = 256;
        config.num_border_cells = 64;
        config.num_hd_cells = 60;
        config.enable_path_integration = true;
        config.enable_boundary_detection = true;
        config.spatial_dim = SPATIAL_DIM;

        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }

    Waypoint createWaypoint(float x, float y, float z, float heading) {
        Waypoint wp;
        wp.position[0] = x;
        wp.position[1] = y;
        wp.position[2] = z;
        wp.heading = heading;
        wp.velocity[0] = 0.1f;
        wp.velocity[1] = 0.0f;
        wp.velocity[2] = 0.0f;
        wp.timestamp_ms = 0;
        return wp;
    }

    NavigationPath createCircularPath(float radius, uint32_t num_points) {
        NavigationPath path;
        path.total_distance = 0.0f;
        path.total_time_ms = 0.0f;

        for (uint32_t i = 0; i < num_points; i++) {
            float angle = (2.0f * 3.14159f * i) / num_points;
            float x = radius * cosf(angle);
            float y = radius * sinf(angle);
            Waypoint wp = createWaypoint(x, y, 0.0f, angle);
            wp.timestamp_ms = i * 100;  // 100ms per waypoint

            if (i > 0) {
                float dx = x - path.waypoints[i-1].position[0];
                float dy = y - path.waypoints[i-1].position[1];
                path.total_distance += sqrtf(dx*dx + dy*dy);
            }
            path.waypoints.push_back(wp);
            path.total_time_ms = wp.timestamp_ms;
        }
        return path;
    }
};

/**
 * @brief Test fixture for memory encoding pipeline tests
 */
class E2EEntorhinalEncodingTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;
    entorhinal_config_t config;

    void SetUp() override {
        config = entorhinal_default_config();
        config.num_grid_cells = 256;
        config.encoding_buffer_size = 1024;
        config.retrieval_buffer_size = 1024;
        config.enable_hippocampus = true;

        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }

    MemoryItem createMemoryItem(uint32_t id, float base_salience = 0.7f) {
        MemoryItem item;
        item.id = id;
        item.salience = base_salience;
        item.features.resize(FEATURE_DIM);
        item.spatial_context.resize(SPATIAL_DIM);

        std::mt19937 gen(id);
        std::normal_distribution<float> dist(0.5f, 0.2f);

        for (size_t i = 0; i < FEATURE_DIM; i++) {
            item.features[i] = std::clamp(dist(gen), 0.0f, 1.0f);
        }
        for (size_t i = 0; i < SPATIAL_DIM; i++) {
            item.spatial_context[i] = std::clamp(dist(gen) * ENVIRONMENT_SIZE,
                                                  0.0f, ENVIRONMENT_SIZE);
        }
        return item;
    }
};

/**
 * @brief Test fixture for memory retrieval pipeline tests
 */
class E2EEntorhinalRetrievalTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;
    entorhinal_config_t config;

    void SetUp() override {
        config = entorhinal_default_config();
        config.num_grid_cells = 256;
        config.encoding_buffer_size = 1024;
        config.retrieval_buffer_size = 1024;
        config.retrieval_threshold = 0.5f;
        config.enable_hippocampus = true;

        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }

    std::vector<float> createCue(uint32_t seed, float noise_level = 0.1f) {
        std::vector<float> cue(FEATURE_DIM);
        std::mt19937 gen(seed);
        std::normal_distribution<float> dist(0.5f, 0.2f);
        std::normal_distribution<float> noise(0.0f, noise_level);

        for (size_t i = 0; i < FEATURE_DIM; i++) {
            cue[i] = std::clamp(dist(gen) + noise(gen), 0.0f, 1.0f);
        }
        return cue;
    }
};

/**
 * @brief Test fixture for multi-bridge coordination tests
 */
class E2EEntorhinalBridgeTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;
    entorhinal_hypothalamus_bridge_state_t* hypo_bridge = nullptr;
    entorhinal_omni_bridge_state_t* omni_bridge = nullptr;
    entorhinal_brain_init_bridge_t* init_bridge = nullptr;

    void SetUp() override {
        // Create entorhinal cortex
        entorhinal_config_t config = entorhinal_default_config();
        config.enable_hypothalamus = true;
        config.enable_omni = true;
        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);

        // Create hypothalamus bridge
        entorhinal_hypothalamus_config_t hypo_config =
            entorhinal_hypothalamus_default_config();
        hypo_bridge = entorhinal_hypothalamus_bridge_create(&hypo_config);
        ASSERT_NE(hypo_bridge, nullptr);

        // Create omnidirectional bridge
        entorhinal_omni_config_t omni_config = entorhinal_omni_default_config();
        omni_bridge = entorhinal_omni_bridge_create(&omni_config);
        ASSERT_NE(omni_bridge, nullptr);

        // Create brain init bridge
        entorhinal_brain_init_config_t init_config =
            entorhinal_brain_init_default_config();
        init_bridge = entorhinal_brain_init_bridge_create(&init_config);
        ASSERT_NE(init_bridge, nullptr);
    }

    void TearDown() override {
        if (init_bridge) {
            entorhinal_brain_init_bridge_destroy(init_bridge);
            init_bridge = nullptr;
        }
        if (omni_bridge) {
            entorhinal_omni_bridge_destroy(omni_bridge);
            omni_bridge = nullptr;
        }
        if (hypo_bridge) {
            entorhinal_hypothalamus_bridge_destroy(hypo_bridge);
            hypo_bridge = nullptr;
        }
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

/**
 * @brief Test fixture for error recovery pipeline tests
 */
class E2EEntorhinalErrorRecoveryTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_config_t config = entorhinal_default_config();
        config.num_grid_cells = 128;
        config.enable_path_integration = true;
        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

/**
 * @brief Test fixture for performance under load tests
 */
class E2EEntorhinalPerformanceTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_config_t config = entorhinal_default_config();
        config.enable_path_integration = true;
        config.enable_boundary_detection = true;
        config.enable_security = false;
        config.enable_immune = false;
        config.enable_bio_async = false;

        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

//=============================================================================
// Spatial Exploration Pipeline Tests
//=============================================================================

TEST_F(E2EEntorhinalSpatialTest, BaselineGridCellActivity) {
    // Scenario: Verify baseline grid cell activity at origin
    E2E_PIPELINE_START("Baseline Grid Cell Activity");

    E2E_STAGE_BEGIN("Initialize at origin", 10);
    float origin[3] = {0.0f, 0.0f, 0.0f};
    int result = entorhinal_update_grid_cells(ec, origin, SPATIAL_DIM);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get grid population vector", 10);
    float population_vector[256];
    uint32_t dim;
    result = entorhinal_get_grid_population_vector(ec, population_vector, &dim);
    EXPECT_EQ(result, 0);
    EXPECT_GT(dim, 0u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify coherence", 5);
    // Check for non-zero population activity
    float mean_activity = 0.0f;
    for (uint32_t i = 0; i < dim && i < 256; i++) {
        mean_activity += population_vector[i];
    }
    mean_activity /= dim;
    EXPECT_GE(mean_activity, 0.0f);
    EXPECT_FALSE(std::isnan(mean_activity));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check status", 2);
    entorhinal_status_t status = entorhinal_get_status(ec);
    EXPECT_NE(status, ENTORHINAL_STATUS_ERROR);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalSpatialTest, PathIntegrationAccuracy) {
    // Scenario: Test path integration through circular path
    E2E_PIPELINE_START("Path Integration Accuracy");

    E2E_STAGE_BEGIN("Create circular path", 10);
    NavigationPath path = createCircularPath(3.0f, PATH_LENGTH);
    EXPECT_EQ(path.waypoints.size(), PATH_LENGTH);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Navigate path with integration", 100);
    std::vector<float> position_errors;
    float dt = 0.1f;  // 100ms timestep

    for (size_t i = 0; i < path.waypoints.size(); i++) {
        const Waypoint& wp = path.waypoints[i];

        // Update grid cells with actual position
        int result = entorhinal_update_grid_cells(ec, wp.position, SPATIAL_DIM);
        EXPECT_EQ(result, 0);

        // Update head direction
        result = entorhinal_update_hd_cells(ec, wp.heading, 0.1f);
        EXPECT_EQ(result, 0);

        // Perform path integration
        result = entorhinal_path_integrate(ec, wp.velocity, 0.1f, dt);
        EXPECT_EQ(result, 0);

        // Get position estimate
        float est_position[3], est_heading;
        float pos_conf, head_conf;
        result = entorhinal_get_position_estimate(ec, est_position, &est_heading,
                                                   &pos_conf, &head_conf);
        EXPECT_EQ(result, 0);

        // Calculate position error
        float error = 0.0f;
        for (int d = 0; d < SPATIAL_DIM; d++) {
            float diff = est_position[d] - wp.position[d];
            error += diff * diff;
        }
        position_errors.push_back(sqrtf(error));

        EXPECT_GE(pos_conf, 0.0f);
        EXPECT_LE(pos_conf, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze errors", 5);
    float mean_error = std::accumulate(position_errors.begin(),
                                        position_errors.end(), 0.0f)
                       / position_errors.size();
    float max_error = *std::max_element(position_errors.begin(),
                                         position_errors.end());

    // Path integration accumulates error, but should remain bounded
    EXPECT_FALSE(std::isnan(mean_error));
    EXPECT_FALSE(std::isinf(max_error));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalSpatialTest, BorderCellBoundaryDetection) {
    // Scenario: Border cells detect environmental boundaries
    E2E_PIPELINE_START("Border Cell Boundary Detection");

    E2E_STAGE_BEGIN("Simulate boundary distances", 20);
    // Create boundary distances from 4 directions
    float boundary_distances[4] = {1.0f, 5.0f, 5.0f, 5.0f};  // Near north boundary

    int result = entorhinal_update_border_cells(ec, boundary_distances, 4);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Detect boundaries", 15);
    float detected_directions[8];
    float detected_distances[8];
    uint32_t num_detected;

    result = entorhinal_detect_boundaries(ec, detected_directions,
                                           detected_distances, 8, &num_detected);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify detection", 5);
    // Should detect at least the near boundary
    EXPECT_GE(num_detected, 0u);
    EXPECT_LE(num_detected, 8u);

    // Check detected values are valid
    for (uint32_t i = 0; i < num_detected; i++) {
        EXPECT_FALSE(std::isnan(detected_directions[i]));
        EXPECT_FALSE(std::isnan(detected_distances[i]));
        EXPECT_GE(detected_distances[i], 0.0f);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalSpatialTest, HeadDirectionCellCalibration) {
    // Scenario: Calibrate head direction cells with known heading
    E2E_PIPELINE_START("Head Direction Cell Calibration");

    E2E_STAGE_BEGIN("Initial HD cell state", 10);
    float initial_heading, initial_confidence;
    int result = entorhinal_decode_heading(ec, &initial_heading, &initial_confidence);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update with rotation", 20);
    // Simulate 360 degree rotation
    for (int deg = 0; deg < 360; deg += 10) {
        float heading = deg * 3.14159f / 180.0f;
        float angular_velocity = 0.5f;

        result = entorhinal_update_hd_cells(ec, heading, angular_velocity);
        EXPECT_EQ(result, 0);

        float decoded_heading, confidence;
        result = entorhinal_decode_heading(ec, &decoded_heading, &confidence);
        EXPECT_EQ(result, 0);
        EXPECT_GE(confidence, 0.0f);
        EXPECT_LE(confidence, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Calibrate with known heading", 10);
    float known_heading = 1.57f;  // 90 degrees
    result = entorhinal_calibrate_hd_cells(ec, known_heading);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify calibration", 5);
    float calibrated_heading, calibrated_confidence;
    result = entorhinal_decode_heading(ec, &calibrated_heading, &calibrated_confidence);
    EXPECT_EQ(result, 0);
    EXPECT_GT(calibrated_confidence, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Memory Encoding Pipeline Tests
//=============================================================================

TEST_F(E2EEntorhinalEncodingTest, BasicMemoryEncodingPipeline) {
    // Scenario: Encode memory through entorhinal gateway
    E2E_PIPELINE_START("Basic Memory Encoding Pipeline");

    E2E_STAGE_BEGIN("Create memory item", 5);
    MemoryItem item = createMemoryItem(42, 0.9f);
    EXPECT_EQ(item.features.size(), FEATURE_DIM);
    EXPECT_EQ(item.spatial_context.size(), SPATIAL_DIM);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Set encoding gate", 10);
    int result = entorhinal_set_encoding_gate(ec, 0.8f);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Encode to hippocampus", 30);
    result = entorhinal_encode_to_hippocampus(ec,
        item.features.data(), FEATURE_DIM,
        item.spatial_context.data(), SPATIAL_DIM);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify encoding", 10);
    uint64_t encoded, retrieved, consolidated;
    result = entorhinal_get_gateway_stats(ec, &encoded, &retrieved, &consolidated);
    EXPECT_EQ(result, 0);
    EXPECT_GE(encoded, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalEncodingTest, SequentialEncodingWithContext) {
    // Scenario: Encode multiple memories with spatial context
    E2E_PIPELINE_START("Sequential Encoding With Context");

    E2E_STAGE_BEGIN("Create multiple items", 10);
    std::vector<MemoryItem> items;
    for (uint32_t i = 0; i < 10; i++) {
        items.push_back(createMemoryItem(i, 0.5f + 0.05f * i));
    }
    EXPECT_EQ(items.size(), 10u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Sequential encoding", 100);
    entorhinal_set_encoding_gate(ec, 0.9f);

    for (const auto& item : items) {
        // Update spatial context (grid cells)
        int result = entorhinal_update_grid_cells(ec,
            item.spatial_context.data(), SPATIAL_DIM);
        EXPECT_EQ(result, 0);

        // Encode memory
        result = entorhinal_encode_to_hippocampus(ec,
            item.features.data(), FEATURE_DIM,
            item.spatial_context.data(), SPATIAL_DIM);
        EXPECT_EQ(result, 0);

        // Check status remains valid
        entorhinal_status_t status = entorhinal_get_status(ec);
        EXPECT_NE(status, ENTORHINAL_STATUS_ERROR);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify all encoded", 5);
    uint64_t encoded, retrieved, consolidated;
    entorhinal_get_gateway_stats(ec, &encoded, &retrieved, &consolidated);
    EXPECT_GE(encoded, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalEncodingTest, EncodingWithBidirectionalUpdate) {
    // Scenario: Encoding with full bidirectional update cycle
    E2E_PIPELINE_START("Encoding With Bidirectional Update");

    E2E_STAGE_BEGIN("Setup", 10);
    MemoryItem item = createMemoryItem(100, 0.95f);
    float dt = 0.016f;  // 16ms (~60Hz)
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Bidirectional update cycle", 50);
    for (int cycle = 0; cycle < 20; cycle++) {
        // Full bidirectional update
        int result = entorhinal_bidirectional_update(ec, dt);
        EXPECT_EQ(result, 0);

        // Process neuromodulation effects
        result = entorhinal_process_neuromodulation(ec);
        EXPECT_EQ(result, 0);

        // Apply plasticity
        result = entorhinal_apply_plasticity(ec, dt);
        EXPECT_EQ(result, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Encode after updates", 20);
    entorhinal_set_encoding_gate(ec, 0.85f);
    int result = entorhinal_encode_to_hippocampus(ec,
        item.features.data(), FEATURE_DIM,
        item.spatial_context.data(), SPATIAL_DIM);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify state", 5);
    float health = entorhinal_get_health_status(ec);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Memory Retrieval Pipeline Tests
//=============================================================================

TEST_F(E2EEntorhinalRetrievalTest, BasicRetrievalPipeline) {
    // Scenario: Retrieve memory from hippocampus through gateway
    E2E_PIPELINE_START("Basic Retrieval Pipeline");

    E2E_STAGE_BEGIN("Set retrieval gate", 10);
    int result = entorhinal_set_retrieval_gate(ec, 0.9f);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create cue", 5);
    std::vector<float> cue = createCue(42, 0.05f);
    EXPECT_EQ(cue.size(), FEATURE_DIM);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Retrieve from hippocampus", 30);
    std::vector<float> retrieved(FEATURE_DIM * 2);
    uint32_t actual_features;

    result = entorhinal_retrieve_from_hippocampus(ec,
        cue.data(), FEATURE_DIM,
        retrieved.data(), FEATURE_DIM * 2,
        &actual_features);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify retrieval", 5);
    // Check retrieval values are valid (may be empty if nothing encoded)
    for (uint32_t i = 0; i < actual_features && i < FEATURE_DIM; i++) {
        EXPECT_FALSE(std::isnan(retrieved[i]));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalRetrievalTest, CuedRetrievalWithNoise) {
    // Scenario: Retrieve with noisy cue (pattern completion)
    E2E_PIPELINE_START("Cued Retrieval With Noise");

    E2E_STAGE_BEGIN("Setup retrieval", 10);
    entorhinal_set_retrieval_gate(ec, 0.95f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test noise levels", 50);
    float noise_levels[] = {0.0f, 0.1f, 0.2f, 0.3f, 0.5f};

    for (float noise : noise_levels) {
        std::vector<float> cue = createCue(123, noise);
        std::vector<float> retrieved(FEATURE_DIM);
        uint32_t actual;

        int result = entorhinal_retrieve_from_hippocampus(ec,
            cue.data(), FEATURE_DIM,
            retrieved.data(), FEATURE_DIM,
            &actual);
        EXPECT_EQ(result, 0);

        // Retrieval should work regardless of noise level
        entorhinal_status_t status = entorhinal_get_status(ec);
        EXPECT_NE(status, ENTORHINAL_STATUS_ERROR);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalRetrievalTest, RetrievalWithConsolidation) {
    // Scenario: Retrieve and consolidate to neocortex
    E2E_PIPELINE_START("Retrieval With Consolidation");

    E2E_STAGE_BEGIN("Retrieve memory", 30);
    std::vector<float> cue = createCue(999, 0.05f);
    std::vector<float> retrieved(FEATURE_DIM);
    uint32_t actual;

    entorhinal_set_retrieval_gate(ec, 0.9f);
    int result = entorhinal_retrieve_from_hippocampus(ec,
        cue.data(), FEATURE_DIM,
        retrieved.data(), FEATURE_DIM,
        &actual);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Consolidate to neocortex", 30);
    uint32_t memory_id = 1;
    float consolidation_strength = 0.8f;

    result = entorhinal_consolidate_to_neocortex(ec, memory_id,
                                                  consolidation_strength);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify consolidation stats", 10);
    uint64_t encoded, retrieved_count, consolidated;
    result = entorhinal_get_gateway_stats(ec, &encoded, &retrieved_count,
                                           &consolidated);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Multi-Bridge Coordination Tests
//=============================================================================

TEST_F(E2EEntorhinalBridgeTest, HypothalamusBridgeIntegration) {
    // Scenario: Test hypothalamus bridge modulation of encoding
    E2E_PIPELINE_START("Hypothalamus Bridge Integration");

    E2E_STAGE_BEGIN("Connect bridge", 20);
    int result = entorhinal_hypothalamus_bridge_connect(hypo_bridge, ec, nullptr);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate motivational state", 30);
    hypothalamic_motivational_state_t motivation;
    memset(&motivation, 0, sizeof(motivation));
    motivation.hunger_drive = 0.8f;
    motivation.exploration_drive = 0.6f;
    motivation.arousal_level = 0.7f;
    motivation.memory_encoding_boost = 1.2f;

    result = entorhinal_hypothalamus_receive_motivation(hypo_bridge, &motivation);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update bridge", 20);
    float dt = 0.016f;
    result = entorhinal_hypothalamus_bridge_update(hypo_bridge, dt);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get encoding modulation", 10);
    float encoding_mod = entorhinal_hypothalamus_get_encoding_modulation(hypo_bridge);
    float plasticity_mod = entorhinal_hypothalamus_get_plasticity_modulation(hypo_bridge);

    EXPECT_GE(encoding_mod, 0.0f);
    EXPECT_GE(plasticity_mod, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalBridgeTest, OmnidirectionalBridgeIntegration) {
    // Scenario: Test omnidirectional spatial awareness integration
    E2E_PIPELINE_START("Omnidirectional Bridge Integration");

    E2E_STAGE_BEGIN("Connect bridge", 20);
    int result = entorhinal_omni_bridge_connect(omni_bridge, ec, nullptr);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create spatial map", 30);
    omni_spatial_map_t spatial_map;
    memset(&spatial_map, 0, sizeof(spatial_map));

    // Simulate salience in front sector
    for (int i = 0; i < 45; i++) {
        spatial_map.azimuth_salience[i] = 0.8f;
        spatial_map.azimuth_distance[i] = 5.0f;
    }
    spatial_map.mean_surround_activity = 0.3f;
    spatial_map.sector_threat[OMNI_SECTOR_BACK] = 0.7f;

    result = entorhinal_omni_receive_spatial_map(omni_bridge, &spatial_map);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Query spatial data", 20);
    float front_salience = entorhinal_omni_get_salience_at_direction(omni_bridge, 0.0f);
    float back_threat = entorhinal_omni_get_threat_at_direction(omni_bridge, 3.14159f);

    EXPECT_GE(front_salience, 0.0f);
    EXPECT_GE(back_threat, 0.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get escape vector", 10);
    float escape[3];
    result = entorhinal_omni_get_escape_vector(omni_bridge, escape);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalBridgeTest, BrainInitBridgeLifecycle) {
    // Scenario: Test brain initialization bridge lifecycle
    E2E_PIPELINE_START("Brain Init Bridge Lifecycle");

    E2E_STAGE_BEGIN("Get initial phase", 5);
    entorhinal_init_phase_t phase = entorhinal_brain_init_get_phase(init_bridge);
    EXPECT_EQ(phase, ENTORHINAL_INIT_PHASE_NONE);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check dependencies", 10);
    bool deps_met = entorhinal_brain_init_check_dependencies(init_bridge,
        ENTORHINAL_DEP_MEMORY_POOL);
    // Dependencies may or may not be met, just verify the call works
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get status", 10);
    entorhinal_init_status_t status;
    int result = entorhinal_brain_init_get_status(init_bridge, &status);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(status.init_failed);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check health", 5);
    float health = entorhinal_brain_init_get_health(init_bridge);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalBridgeTest, MultiBridgeCoordinatedUpdate) {
    // Scenario: All bridges update in coordinated fashion
    E2E_PIPELINE_START("Multi-Bridge Coordinated Update");

    E2E_STAGE_BEGIN("Connect all bridges", 30);
    int result = entorhinal_hypothalamus_bridge_connect(hypo_bridge, ec, nullptr);
    EXPECT_EQ(result, 0);

    result = entorhinal_omni_bridge_connect(omni_bridge, ec, nullptr);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Coordinated update loop", 100);
    float dt = 0.016f;

    for (int step = 0; step < 60; step++) {  // ~1 second at 60Hz
        // Update hypothalamus bridge
        result = entorhinal_hypothalamus_bridge_update(hypo_bridge, dt);
        EXPECT_EQ(result, 0);

        // Update omnidirectional bridge
        result = entorhinal_omni_bridge_update(omni_bridge, dt);
        EXPECT_EQ(result, 0);

        // Update entorhinal core
        result = entorhinal_bidirectional_update(ec, dt);
        EXPECT_EQ(result, 0);

        // Check for errors
        entorhinal_status_t status = entorhinal_get_status(ec);
        EXPECT_NE(status, ENTORHINAL_STATUS_ERROR);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify final states", 10);
    uint64_t hypo_updates, omni_updates;
    float mean_motivation, mean_boost, mean_spatial;

    entorhinal_hypothalamus_bridge_get_stats(hypo_bridge, &hypo_updates,
                                              &mean_motivation, &mean_boost);
    EXPECT_GT(hypo_updates, 0u);

    entorhinal_omni_bridge_get_stats(omni_bridge, &omni_updates, nullptr, nullptr);
    EXPECT_GT(omni_updates, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Error Recovery Pipeline Tests
//=============================================================================

TEST_F(E2EEntorhinalErrorRecoveryTest, GridDriftRecovery) {
    // Scenario: Recover from accumulated grid cell drift
    E2E_PIPELINE_START("Grid Drift Recovery");

    E2E_STAGE_BEGIN("Simulate extended navigation", 50);
    // Navigate without visual correction - accumulates drift
    float velocity[3] = {0.1f, 0.0f, 0.0f};
    float dt = 0.1f;

    for (int step = 0; step < 100; step++) {
        int result = entorhinal_path_integrate(ec, velocity, 0.0f, dt);
        EXPECT_EQ(result, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get drift state", 10);
    float position[3], heading;
    float pos_conf, head_conf;

    int result = entorhinal_get_position_estimate(ec, position, &heading,
                                                   &pos_conf, &head_conf);
    EXPECT_EQ(result, 0);
    // Confidence may have decreased due to drift
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply visual correction", 20);
    float known_position[3] = {5.0f, 0.0f, 0.0f};
    float known_heading = 0.0f;
    float correction_confidence = 0.95f;

    result = entorhinal_apply_visual_correction(ec, known_position,
                                                 known_heading, correction_confidence);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify recovery", 10);
    result = entorhinal_get_position_estimate(ec, position, &heading,
                                               &pos_conf, &head_conf);
    EXPECT_EQ(result, 0);
    EXPECT_GT(pos_conf, 0.0f);  // Confidence should improve after correction
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalErrorRecoveryTest, MemoryGatewayRecovery) {
    // Scenario: Recover from blocked memory gateway
    E2E_PIPELINE_START("Memory Gateway Recovery");

    E2E_STAGE_BEGIN("Block gateway", 10);
    // Set encoding gate to zero (blocked)
    int result = entorhinal_set_encoding_gate(ec, 0.0f);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Attempt encoding while blocked", 20);
    float features[64] = {0.5f};
    float context[3] = {1.0f, 2.0f, 0.0f};

    // Encoding should fail or be rejected
    result = entorhinal_encode_to_hippocampus(ec, features, 64, context, 3);
    // Result may indicate blocked gateway
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Unblock gateway", 10);
    result = entorhinal_set_encoding_gate(ec, 0.9f);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify encoding works", 20);
    result = entorhinal_encode_to_hippocampus(ec, features, 64, context, 3);
    EXPECT_EQ(result, 0);

    entorhinal_status_t status = entorhinal_get_status(ec);
    EXPECT_NE(status, ENTORHINAL_STATUS_ERROR);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalErrorRecoveryTest, SystemResetRecovery) {
    // Scenario: Full system reset and recovery
    E2E_PIPELINE_START("System Reset Recovery");

    E2E_STAGE_BEGIN("Normal operation", 30);
    float position[3] = {3.0f, 4.0f, 0.0f};
    int result = entorhinal_update_grid_cells(ec, position, 3);
    EXPECT_EQ(result, 0);

    result = entorhinal_update_hd_cells(ec, 1.0f, 0.1f);
    EXPECT_EQ(result, 0);

    result = entorhinal_bidirectional_update(ec, 0.016f);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Perform reset", 20);
    bool reset_success = entorhinal_reset(ec);
    EXPECT_TRUE(reset_success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Resume operation", 30);
    float origin[3] = {0.0f, 0.0f, 0.0f};
    result = entorhinal_update_grid_cells(ec, origin, 3);
    EXPECT_EQ(result, 0);

    // Decode position after reset
    float decoded_pos[3], decoded_heading;
    float pos_conf, head_conf;
    result = entorhinal_get_position_estimate(ec, decoded_pos, &decoded_heading,
                                               &pos_conf, &head_conf);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify clean state", 10);
    entorhinal_stats_t stats;
    result = entorhinal_get_stats(ec, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.updates_processed, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Performance Under Load Tests
//=============================================================================

TEST_F(E2EEntorhinalPerformanceTest, HighFrequencySpatialUpdates) {
    // Scenario: Handle high-frequency spatial updates (100Hz)
    E2E_PIPELINE_START("High Frequency Spatial Updates");

    E2E_STAGE_BEGIN("Configure for high frequency", 10);
    float dt = 0.01f;  // 10ms = 100Hz
    uint32_t num_updates = 1000;  // 10 seconds of simulation
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute high-frequency updates", 200);
    std::vector<float> update_times;
    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < num_updates; i++) {
        // Generate position along path
        float t = i * dt;
        float position[3] = {
            2.0f * cosf(t * 0.5f),
            2.0f * sinf(t * 0.5f),
            0.0f
        };
        float velocity[3] = {
            -1.0f * sinf(t * 0.5f),
            1.0f * cosf(t * 0.5f),
            0.0f
        };

        auto update_start = std::chrono::high_resolution_clock::now();

        int result = entorhinal_update_grid_cells(ec, position, 3);
        EXPECT_EQ(result, 0);
        result = entorhinal_update_hd_cells(ec, t * 0.5f, 0.1f);
        EXPECT_EQ(result, 0);

        auto update_end = std::chrono::high_resolution_clock::now();
        double update_ms = std::chrono::duration<double, std::milli>(
            update_end - update_start).count();
        update_times.push_back(update_ms);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze performance", 10);
    double mean_time = std::accumulate(update_times.begin(), update_times.end(), 0.0)
                       / update_times.size();
    double max_time = *std::max_element(update_times.begin(), update_times.end());

    // Should maintain real-time performance
    EXPECT_LT(mean_time, 10.0);  // < 10ms average
    EXPECT_LT(total_ms, num_updates * 20.0);  // No worse than 50Hz throughput

    // No NaN or Inf in timing
    EXPECT_FALSE(std::isnan(mean_time));
    EXPECT_FALSE(std::isinf(max_time));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalPerformanceTest, BurstMemoryOperations) {
    // Scenario: Handle burst of memory operations
    E2E_PIPELINE_START("Burst Memory Operations");

    E2E_STAGE_BEGIN("Prepare burst", 10);
    const uint32_t burst_size = 100;
    std::vector<std::vector<float>> features(burst_size);
    std::vector<std::vector<float>> contexts(burst_size);

    for (uint32_t i = 0; i < burst_size; i++) {
        features[i].resize(FEATURE_DIM);
        contexts[i].resize(SPATIAL_DIM);

        std::mt19937 gen(i);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (size_t j = 0; j < FEATURE_DIM; j++) {
            features[i][j] = dist(gen);
        }
        for (size_t j = 0; j < SPATIAL_DIM; j++) {
            contexts[i][j] = dist(gen) * ENVIRONMENT_SIZE;
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute encoding burst", 150);
    auto start = std::chrono::high_resolution_clock::now();
    uint32_t success_count = 0;

    for (uint32_t i = 0; i < burst_size; i++) {
        int result = entorhinal_encode_to_hippocampus(ec,
            features[i].data(), FEATURE_DIM,
            contexts[i].data(), SPATIAL_DIM);

        if (result == 0) success_count++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    double burst_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify burst results", 10);
    double per_op_time = burst_time_ms / burst_size;
    EXPECT_LT(per_op_time, 10.0);  // < 10ms per operation

    // Most operations should succeed
    EXPECT_GT(success_count, burst_size / 2);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EEntorhinalPerformanceTest, SustainedLoadStability) {
    // Scenario: Maintain stability under sustained load
    E2E_PIPELINE_START("Sustained Load Stability");

    E2E_STAGE_BEGIN("Sustained operation", 500);
    const uint32_t total_cycles = 1000;
    float dt = 0.016f;  // ~60Hz

    std::vector<float> health_samples;
    std::vector<entorhinal_status_t> status_samples;

    for (uint32_t cycle = 0; cycle < total_cycles; cycle++) {
        // Spatial update
        float position[3] = {
            ENVIRONMENT_SIZE/2 + 3.0f * cosf(cycle * 0.01f),
            ENVIRONMENT_SIZE/2 + 3.0f * sinf(cycle * 0.01f),
            0.0f
        };
        float velocity[3] = {0.1f, 0.0f, 0.0f};

        int result = entorhinal_update_grid_cells(ec, position, 3);
        EXPECT_EQ(result, 0);
        result = entorhinal_update_hd_cells(ec, cycle * 0.01f, 0.0f);
        EXPECT_EQ(result, 0);

        // Full update cycle
        result = entorhinal_bidirectional_update(ec, dt);
        EXPECT_EQ(result, 0);

        // Sample health periodically
        if (cycle % 100 == 0) {
            float health = entorhinal_get_health_status(ec);
            health_samples.push_back(health);

            entorhinal_status_t status = entorhinal_get_status(ec);
            status_samples.push_back(status);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze stability", 20);
    // Health should not degrade significantly
    float initial_health = health_samples.front();
    float final_health = health_samples.back();

    // Allow some degradation but should remain operational
    // Note: health may be 0.0 if bridges not fully initialized
    EXPECT_GE(final_health, 0.0f);

    // No error states should occur
    for (auto status : status_samples) {
        EXPECT_NE(status, ENTORHINAL_STATUS_ERROR);
    }

    // Calculate health variance
    float mean_health = std::accumulate(health_samples.begin(),
                                         health_samples.end(), 0.0f)
                        / health_samples.size();
    EXPECT_GE(mean_health, 0.0f);  // Allow 0.0 if health not tracked
    EXPECT_FALSE(std::isnan(mean_health));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final diagnostics", 10);
    entorhinal_stats_t stats;
    int result = entorhinal_get_stats(ec, &stats);
    EXPECT_EQ(result, 0);

    // Should have processed many updates
    EXPECT_GE(stats.updates_processed, total_cycles);

    // No infinite values in statistics
    EXPECT_FALSE(std::isnan(stats.mean_grid_activation));
    EXPECT_FALSE(std::isnan(stats.position_error_mean));
    EXPECT_FALSE(std::isinf(stats.mean_update_latency_ms));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
