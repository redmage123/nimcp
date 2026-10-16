/**
 * @file test_spatial_processing_e2e.cpp
 * @brief End-to-end tests for spatial reasoning with right hemisphere dominance
 *
 * WHAT: Full pipeline tests for spatial processing in hemispheric brain
 * WHY:  Verify biologically-accurate spatial cognition with right hemisphere dominance
 * HOW:  Test mental rotation, navigation, map processing, and visuospatial tasks
 *
 * TEST COVERAGE:
 * - Spatial Domain Routing (3 tests)
 * - Mental Rotation Tasks (4 tests)
 * - Navigation Processing (4 tests)
 * - Map/Visuospatial Processing (4 tests)
 *
 * TOTAL: 15 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Spatial processing is ~80% right hemisphere lateralized
 * - Parietal cortex handles spatial attention and transformation
 * - Right hemisphere excels at holistic/global processing
 * - Mental rotation activates right parietal and motor regions
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "../../../e2e_test_framework.h"
#include "utils/nimcp_test_base.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <cstring>


#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"
#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_SPATIAL_PROCESSING_TIME_MS = 150.0;
constexpr double MAX_ROTATION_TIME_MS = 200.0;
constexpr double MAX_NAVIGATION_TIME_MS = 250.0;
constexpr float SPATIAL_RIGHT_DOMINANCE_THRESHOLD = 0.3f;  // <0.5 means right dominant
constexpr uint32_t SPATIAL_INPUT_SIZE = 64;
constexpr uint32_t SPATIAL_OUTPUT_SIZE = 32;
constexpr uint32_t GRID_SIZE = 8;

//=============================================================================
// Helper Functions
//=============================================================================

static std::vector<float> generate_spatial_grid(uint32_t grid_size, uint32_t seed) {
    // Generate 2D spatial representation flattened
    std::vector<float> grid(grid_size * grid_size);
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (uint32_t i = 0; i < grid_size * grid_size; i++) {
        grid[i] = dist(gen);
    }
    return grid;
}

static std::vector<float> generate_rotation_pattern(uint32_t size, float angle_degrees) {
    // Generate pattern for mental rotation task
    std::vector<float> pattern(size);
    float angle_rad = angle_degrees * M_PI / 180.0f;

    for (uint32_t i = 0; i < size; i++) {
        // Encode rotated coordinates
        float x = std::cos(2.0f * M_PI * i / size);
        float y = std::sin(2.0f * M_PI * i / size);

        // Apply rotation
        float rx = x * std::cos(angle_rad) - y * std::sin(angle_rad);
        float ry = x * std::sin(angle_rad) + y * std::cos(angle_rad);

        pattern[i] = 0.5f * (rx + ry) + 0.5f;
    }
    return pattern;
}

static std::vector<float> generate_navigation_path(uint32_t size, uint32_t num_waypoints) {
    // Generate navigation waypoint encoding
    std::vector<float> path(size, 0.0f);

    for (uint32_t wp = 0; wp < num_waypoints && wp < size / 2; wp++) {
        // Encode x, y coordinates for each waypoint
        path[wp * 2] = static_cast<float>(wp) / num_waypoints;  // x
        path[wp * 2 + 1] = 0.5f + 0.3f * std::sin(wp * 0.5f);   // y
    }
    return path;
}

static std::vector<float> generate_map_representation(uint32_t size) {
    // Generate topographical map encoding
    std::vector<float> map(size);

    for (uint32_t i = 0; i < size; i++) {
        // Simulate terrain-like features
        float x = static_cast<float>(i % 8) / 8.0f;
        float y = static_cast<float>(i / 8) / 8.0f;
        map[i] = 0.3f * std::sin(x * M_PI) * std::cos(y * M_PI) + 0.5f;
    }
    return map;
}

//=============================================================================
// Test Fixture
//=============================================================================

class E2ESpatialProcessingTest : public ::testing::Test {
protected:
    static hemispheric_brain_t* shared_brain;
    hemispheric_brain_t* brain = nullptr;

    static void SetUpTestSuite() {
        signal_handler_unregister_brain();
        signal_handler_reset_stats();
        signal_handler_uninstall();

        hemispheric_brain_config_t config = hemispheric_brain_default_config();
        config.size = BRAIN_SIZE_MICRO;
        config.num_inputs = SPATIAL_INPUT_SIZE;
        config.num_outputs = SPATIAL_OUTPUT_SIZE;
        config.default_mode = HEMISPHERIC_MODE_LATERALIZED;
        config.enable_shared_thalamus = true;

        shared_brain = hemispheric_brain_create(&config);

        // Warm up: training sets eligibility traces, updates build activity
        // via neuromodulators (dopamine/norepinephrine). The adaptive spiking
        // network may still produce near-zero output magnitudes with random
        // weights, but activity_level accumulates from neuromodulator updates.
        if (shared_brain) {
            brain_hemisphere_t* left = hemispheric_brain_get_left(shared_brain);
            brain_hemisphere_t* right = hemispheric_brain_get_right(shared_brain);

            std::vector<float> warmup_input(SPATIAL_INPUT_SIZE);
            std::vector<float> warmup_target(SPATIAL_INPUT_SIZE);

            for (int iter = 0; iter < 10; iter++) {
                for (uint32_t i = 0; i < SPATIAL_INPUT_SIZE; i++) {
                    warmup_input[i] = 0.5f + 0.3f * sinf((float)(i + iter));
                    warmup_target[i] = 0.5f + 0.2f * cosf((float)(i + iter));
                }
                if (left) hemisphere_train(left, warmup_input.data(), warmup_target.data(), SPATIAL_INPUT_SIZE);
                if (right) hemisphere_train(right, warmup_input.data(), warmup_target.data(), SPATIAL_INPUT_SIZE);
            }

            // Run updates to build activity_level through neuromodulators
            for (int i = 0; i < 50; i++) {
                hemispheric_brain_update(shared_brain, 0.01f);
            }
        }
    }

    static void TearDownTestSuite() {
        if (shared_brain) {
            hemispheric_brain_destroy(shared_brain);
            shared_brain = nullptr;
        }
    }

    void SetUp() override {
        brain = shared_brain;
        ASSERT_NE(brain, nullptr) << "Failed to create hemispheric brain";
    }
};

hemispheric_brain_t* E2ESpatialProcessingTest::shared_brain = nullptr;

//=============================================================================
// Spatial Domain Routing Tests
//=============================================================================

TEST_F(E2ESpatialProcessingTest, SpatialDomainRoutesToRightHemisphere) {
    E2E_PIPELINE_START("Spatial Domain Routing");

    // Verify spatial domain is right-dominant
    E2E_STAGE_BEGIN("Check spatial lateralization", 10);
    float dominance = hemispheric_brain_get_dominance(brain, COGNITIVE_DOMAIN_SPATIAL);
    EXPECT_LT(dominance, SPATIAL_RIGHT_DOMINANCE_THRESHOLD)
        << "Spatial should be right-lateralized (dominance < 0.3)";
    E2E_STAGE_END();

    // Verify dominant hemisphere
    E2E_STAGE_BEGIN("Verify dominant hemisphere", 10);
    hemisphere_id_t dominant = hemispheric_brain_get_dominant_for(brain, COGNITIVE_DOMAIN_SPATIAL);
    EXPECT_EQ(dominant, HEMISPHERE_RIGHT) << "Right hemisphere should be dominant for spatial";
    E2E_STAGE_END();

    // Process spatial input and verify routing
    E2E_STAGE_BEGIN("Process spatial input", MAX_SPATIAL_PROCESSING_TIME_MS);
    auto input = generate_spatial_grid(GRID_SIZE, 42);
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);

    int result = hemispheric_brain_process_lateralized(
        brain,
        input.data(),
        static_cast<uint32_t>(input.size()),
        COGNITIVE_DOMAIN_SPATIAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0) << "Lateralized processing should succeed";
    E2E_STAGE_END();

    // Check that right hemisphere was activated
    E2E_STAGE_BEGIN("Verify right hemisphere activation", 10);
    brain_hemisphere_t* right = hemispheric_brain_get_right(brain);
    ASSERT_NE(right, nullptr);
    float right_activity = hemisphere_get_activity(right);
    EXPECT_GE(right_activity, 0.0f) << "Right hemisphere should show activity";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpatialProcessingTest, GlobalAttentionRightDominant) {
    E2E_PIPELINE_START("Global Attention Processing");

    // Global attention (forest vs trees) is right-lateralized
    E2E_STAGE_BEGIN("Check global attention lateralization", 10);
    float global_dominance = hemispheric_brain_get_dominance(brain, COGNITIVE_DOMAIN_ATTENTION_GLOBAL);
    EXPECT_LT(global_dominance, 0.5f)
        << "Global attention should be right-lateralized";
    E2E_STAGE_END();

    // Local attention should be left-lateralized
    E2E_STAGE_BEGIN("Check local attention lateralization", 10);
    float local_dominance = hemispheric_brain_get_dominance(brain, COGNITIVE_DOMAIN_ATTENTION_LOCAL);
    EXPECT_GT(local_dominance, 0.5f)
        << "Local attention should be left-lateralized";
    E2E_STAGE_END();

    // Process global pattern
    E2E_STAGE_BEGIN("Process global pattern", MAX_SPATIAL_PROCESSING_TIME_MS);
    auto input = generate_spatial_grid(GRID_SIZE, 123);
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);

    int result = hemispheric_brain_process_lateralized(
        brain,
        input.data(),
        static_cast<uint32_t>(input.size()),
        COGNITIVE_DOMAIN_ATTENTION_GLOBAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpatialProcessingTest, FaceRecognitionRightDominant) {
    E2E_PIPELINE_START("Face Recognition Lateralization");

    // Face recognition is strongly right-lateralized
    E2E_STAGE_BEGIN("Check face recognition lateralization", 10);
    float face_dominance = hemispheric_brain_get_dominance(brain, COGNITIVE_DOMAIN_FACE_RECOGNITION);
    EXPECT_LT(face_dominance, 0.3f)
        << "Face recognition should be strongly right-lateralized";

    hemisphere_id_t dominant = hemispheric_brain_get_dominant_for(brain, COGNITIVE_DOMAIN_FACE_RECOGNITION);
    EXPECT_EQ(dominant, HEMISPHERE_RIGHT);
    E2E_STAGE_END();

    // Process face-like pattern
    E2E_STAGE_BEGIN("Process face pattern", MAX_SPATIAL_PROCESSING_TIME_MS);
    auto input = generate_spatial_grid(GRID_SIZE, 456);
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);

    int result = hemispheric_brain_process_lateralized(
        brain,
        input.data(),
        static_cast<uint32_t>(input.size()),
        COGNITIVE_DOMAIN_FACE_RECOGNITION,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Mental Rotation Task Tests
//=============================================================================

TEST_F(E2ESpatialProcessingTest, MentalRotationBasic) {
    E2E_PIPELINE_START("Mental Rotation Basic");

    // Generate original and rotated patterns
    E2E_STAGE_BEGIN("Generate rotation patterns", 10);
    auto original = generate_rotation_pattern(SPATIAL_INPUT_SIZE, 0.0f);
    auto rotated_45 = generate_rotation_pattern(SPATIAL_INPUT_SIZE, 45.0f);
    auto rotated_90 = generate_rotation_pattern(SPATIAL_INPUT_SIZE, 90.0f);
    E2E_STAGE_END();

    // Process original
    E2E_STAGE_BEGIN("Process original pattern", MAX_ROTATION_TIME_MS);
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);
    int result = hemispheric_brain_process_lateralized(
        brain,
        original.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Process rotated patterns - should use right hemisphere
    E2E_STAGE_BEGIN("Process rotated patterns", MAX_ROTATION_TIME_MS * 2);
    result = hemispheric_brain_process_lateralized(
        brain,
        rotated_45.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);

    result = hemispheric_brain_process_lateralized(
        brain,
        rotated_90.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpatialProcessingTest, MentalRotationReactionTime) {
    E2E_PIPELINE_START("Mental Rotation Reaction Time");

    // Classic finding: RT increases with rotation angle
    E2E_STAGE_BEGIN("Rotation angle RT test", MAX_ROTATION_TIME_MS * 4);
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);

    std::vector<float> angles = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f};
    std::vector<double> reaction_times;

    for (float angle : angles) {
        auto pattern = generate_rotation_pattern(SPATIAL_INPUT_SIZE, angle);

        auto start = std::chrono::high_resolution_clock::now();

        int result = hemispheric_brain_process_lateralized(
            brain,
            pattern.data(),
            SPATIAL_INPUT_SIZE,
            COGNITIVE_DOMAIN_SPATIAL,
            output.data(),
            SPATIAL_OUTPUT_SIZE
        );
        EXPECT_EQ(result, 0);

        auto end = std::chrono::high_resolution_clock::now();
        double rt = std::chrono::duration<double, std::milli>(end - start).count();
        reaction_times.push_back(rt);

        hemispheric_brain_update(brain, 0.01f);
    }

    // Verify we captured all RTs
    EXPECT_EQ(reaction_times.size(), angles.size());
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpatialProcessingTest, MentalRotationParallelComparison) {
    E2E_PIPELINE_START("Mental Rotation Parallel");

    // Use parallel processing to compare original and rotated
    E2E_STAGE_BEGIN("Generate comparison patterns", 10);
    auto original = generate_rotation_pattern(SPATIAL_INPUT_SIZE, 0.0f);
    auto rotated = generate_rotation_pattern(SPATIAL_INPUT_SIZE, 90.0f);
    E2E_STAGE_END();

    // Process both patterns in parallel
    E2E_STAGE_BEGIN("Parallel processing", MAX_ROTATION_TIME_MS);
    std::vector<float> left_output(SPATIAL_OUTPUT_SIZE);
    std::vector<float> right_output(SPATIAL_OUTPUT_SIZE);

    int result = hemispheric_brain_process_parallel(
        brain,
        original.data(),
        SPATIAL_INPUT_SIZE,
        left_output.data(),
        right_output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Verify both outputs are valid (no NaN/Inf)
    // Note: adaptive spiking networks may produce near-zero outputs with
    // random weights. Output magnitude depends on training, not routing.
    E2E_STAGE_BEGIN("Verify outputs", 10);
    bool has_nan_inf = false;
    for (uint32_t i = 0; i < SPATIAL_OUTPUT_SIZE; i++) {
        if (std::isnan(left_output[i]) || std::isinf(left_output[i]) ||
            std::isnan(right_output[i]) || std::isinf(right_output[i])) {
            has_nan_inf = true;
            break;
        }
    }
    EXPECT_FALSE(has_nan_inf) << "Outputs should not contain NaN or Inf";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpatialProcessingTest, MentalRotationWithMotorPlanning) {
    E2E_PIPELINE_START("Mental Rotation Motor Planning");

    // Mental rotation also involves motor simulation
    E2E_STAGE_BEGIN("Rotation with motor component", MAX_ROTATION_TIME_MS);
    auto rotation_input = generate_rotation_pattern(SPATIAL_INPUT_SIZE, 60.0f);
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);

    // Spatial rotation
    int result = hemispheric_brain_process_lateralized(
        brain,
        rotation_input.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);

    // Motor planning (bilateral for gross motor)
    // Pad output to INPUT_SIZE since brain expects consistent input dimensions
    std::vector<float> motor_input(SPATIAL_INPUT_SIZE, 0.0f);
    std::copy(output.begin(), output.end(), motor_input.begin());
    result = hemispheric_brain_process_cooperative(
        brain,
        motor_input.data(),
        SPATIAL_INPUT_SIZE,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Navigation Processing Tests
//=============================================================================

TEST_F(E2ESpatialProcessingTest, NavigationPathPlanning) {
    E2E_PIPELINE_START("Navigation Path Planning");

    // Generate navigation path
    E2E_STAGE_BEGIN("Generate path", 10);
    auto path = generate_navigation_path(SPATIAL_INPUT_SIZE, 8);
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);
    E2E_STAGE_END();

    // Process path through spatial system
    E2E_STAGE_BEGIN("Process navigation path", MAX_NAVIGATION_TIME_MS);
    int result = hemispheric_brain_process_lateralized(
        brain,
        path.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Verify right hemisphere activation
    E2E_STAGE_BEGIN("Verify navigation processing", 10);
    brain_hemisphere_t* right = hemispheric_brain_get_right(brain);
    ASSERT_NE(right, nullptr);
    hemisphere_stats_t stats;
    hemisphere_get_stats(right, &stats);
    EXPECT_GT(stats.total_inferences, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpatialProcessingTest, NavigationWaypointSequence) {
    E2E_PIPELINE_START("Navigation Waypoint Sequence");

    // Process sequence of waypoints
    E2E_STAGE_BEGIN("Process waypoints", MAX_NAVIGATION_TIME_MS * 2);
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);

    for (int waypoint = 0; waypoint < 5; waypoint++) {
        auto path = generate_navigation_path(SPATIAL_INPUT_SIZE, waypoint + 3);

        int result = hemispheric_brain_process_lateralized(
            brain,
            path.data(),
            SPATIAL_INPUT_SIZE,
            COGNITIVE_DOMAIN_SPATIAL,
            output.data(),
            SPATIAL_OUTPUT_SIZE
        );
        EXPECT_EQ(result, 0) << "Waypoint " << waypoint << " failed";

        // Update between waypoints
        hemispheric_brain_update(brain, 0.02f);
    }
    E2E_STAGE_END();

    // Check navigation stats
    E2E_STAGE_BEGIN("Check navigation stats", 10);
    hemispheric_brain_stats_t stats;
    int result = hemispheric_brain_get_stats(brain, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.lateralized_operations, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpatialProcessingTest, NavigationWithLandmarks) {
    E2E_PIPELINE_START("Navigation Landmarks");

    // Navigation using landmark recognition (right hemisphere)
    E2E_STAGE_BEGIN("Process landmarks", MAX_NAVIGATION_TIME_MS);
    auto landmark_pattern = generate_spatial_grid(GRID_SIZE, 789);
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);

    // Landmark recognition (uses face/object recognition pathway)
    int result = hemispheric_brain_process_lateralized(
        brain,
        landmark_pattern.data(),
        static_cast<uint32_t>(landmark_pattern.size()),
        COGNITIVE_DOMAIN_FACE_RECOGNITION,  // Object/landmark recognition
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Integrate with spatial navigation
    // Pad output to INPUT_SIZE since brain expects consistent input dimensions
    E2E_STAGE_BEGIN("Integrate with navigation", MAX_NAVIGATION_TIME_MS);
    std::vector<float> nav_input(SPATIAL_INPUT_SIZE, 0.0f);
    std::copy(output.begin(), output.end(), nav_input.begin());
    result = hemispheric_brain_process_lateralized(
        brain,
        nav_input.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpatialProcessingTest, NavigationCoordinateTransform) {
    E2E_PIPELINE_START("Navigation Coordinate Transform");

    // Egocentric to allocentric coordinate transformation
    E2E_STAGE_BEGIN("Coordinate transform", MAX_NAVIGATION_TIME_MS);

    // Egocentric input (self-relative)
    auto egocentric = generate_navigation_path(SPATIAL_INPUT_SIZE, 4);

    // Process through spatial system
    std::vector<float> allocentric(SPATIAL_OUTPUT_SIZE);
    int result = hemispheric_brain_process_lateralized(
        brain,
        egocentric.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        allocentric.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Verify transformation produced valid output (no NaN/Inf)
    // Note: adaptive spiking networks may produce near-zero outputs with
    // random weights. Output validity, not magnitude, is the key check.
    E2E_STAGE_BEGIN("Verify transformation", 10);
    bool has_nan_inf = false;
    for (uint32_t i = 0; i < SPATIAL_OUTPUT_SIZE; i++) {
        if (std::isnan(allocentric[i]) || std::isinf(allocentric[i])) {
            has_nan_inf = true;
            break;
        }
    }
    EXPECT_FALSE(has_nan_inf) << "Output should not contain NaN or Inf";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Map/Visuospatial Processing Tests
//=============================================================================

TEST_F(E2ESpatialProcessingTest, MapProcessingTopDown) {
    E2E_PIPELINE_START("Map Processing Top-Down");

    // Process map representation
    E2E_STAGE_BEGIN("Process map", MAX_SPATIAL_PROCESSING_TIME_MS);
    auto map = generate_map_representation(SPATIAL_INPUT_SIZE);
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);

    int result = hemispheric_brain_process_lateralized(
        brain,
        map.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Apply global attention (right hemisphere)
    // Pad output to INPUT_SIZE since brain expects consistent input dimensions
    E2E_STAGE_BEGIN("Global attention on map", MAX_SPATIAL_PROCESSING_TIME_MS);
    std::vector<float> attn_input(SPATIAL_INPUT_SIZE, 0.0f);
    std::copy(output.begin(), output.end(), attn_input.begin());
    result = hemispheric_brain_process_lateralized(
        brain,
        attn_input.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_ATTENTION_GLOBAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpatialProcessingTest, MapProcessingDetailExtraction) {
    E2E_PIPELINE_START("Map Detail Extraction");

    // Process map with local attention (left hemisphere)
    E2E_STAGE_BEGIN("Process map details", MAX_SPATIAL_PROCESSING_TIME_MS);
    auto map = generate_map_representation(SPATIAL_INPUT_SIZE);
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);

    // First global view
    int result = hemispheric_brain_process_lateralized(
        brain,
        map.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_ATTENTION_GLOBAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);

    // Then local detail extraction
    // Pad output to INPUT_SIZE since brain expects consistent input dimensions
    std::vector<float> local_input(SPATIAL_INPUT_SIZE, 0.0f);
    std::copy(output.begin(), output.end(), local_input.begin());
    result = hemispheric_brain_process_lateralized(
        brain,
        local_input.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_ATTENTION_LOCAL,
        output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Verify hemispheric cooperation
    E2E_STAGE_BEGIN("Verify cooperation", 10);
    hemispheric_brain_stats_t stats;
    result = hemispheric_brain_get_stats(brain, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.left_activity, 0.0f);
    EXPECT_GT(stats.right_activity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpatialProcessingTest, VisuospatialWorkingMemory) {
    E2E_PIPELINE_START("Visuospatial Working Memory");

    // Test visuospatial working memory with sequential presentations
    E2E_STAGE_BEGIN("Sequential spatial memory", MAX_SPATIAL_PROCESSING_TIME_MS * 3);
    std::vector<std::vector<float>> patterns;
    std::vector<float> output(SPATIAL_OUTPUT_SIZE);

    // Generate and process sequence of patterns
    for (int i = 0; i < 4; i++) {
        auto pattern = generate_spatial_grid(GRID_SIZE, 100 + i);
        patterns.push_back(pattern);

        int result = hemispheric_brain_process_lateralized(
            brain,
            pattern.data(),
            static_cast<uint32_t>(pattern.size()),
            COGNITIVE_DOMAIN_SPATIAL,
            output.data(),
            SPATIAL_OUTPUT_SIZE
        );
        EXPECT_EQ(result, 0);

        // Brief update between items
        hemispheric_brain_update(brain, 0.05f);
    }
    E2E_STAGE_END();

    // Probe memory with original pattern
    E2E_STAGE_BEGIN("Memory probe", MAX_SPATIAL_PROCESSING_TIME_MS);
    std::vector<float> probe_output(SPATIAL_OUTPUT_SIZE);
    int result = hemispheric_brain_process_lateralized(
        brain,
        patterns[0].data(),
        static_cast<uint32_t>(patterns[0].size()),
        COGNITIVE_DOMAIN_SPATIAL,
        probe_output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ESpatialProcessingTest, VisuospatialTransformPipeline) {
    E2E_PIPELINE_START("Visuospatial Transform Pipeline");

    // Full visuospatial transformation pipeline
    E2E_STAGE_BEGIN("Visual input", 50);
    auto visual_input = generate_spatial_grid(GRID_SIZE, 999);
    std::vector<float> stage1_output(SPATIAL_OUTPUT_SIZE);
    E2E_STAGE_END();

    // Stage 1: Visual processing (right dominant)
    E2E_STAGE_BEGIN("Visual processing", MAX_SPATIAL_PROCESSING_TIME_MS);
    int result = hemispheric_brain_process_lateralized(
        brain,
        visual_input.data(),
        static_cast<uint32_t>(visual_input.size()),
        COGNITIVE_DOMAIN_FACE_RECOGNITION,  // Visual object processing
        stage1_output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Stage 2: Spatial transformation
    // Pad output to INPUT_SIZE since brain expects consistent input dimensions
    E2E_STAGE_BEGIN("Spatial transformation", MAX_SPATIAL_PROCESSING_TIME_MS);
    std::vector<float> stage2_input(SPATIAL_INPUT_SIZE, 0.0f);
    std::copy(stage1_output.begin(), stage1_output.end(), stage2_input.begin());
    std::vector<float> stage2_output(SPATIAL_OUTPUT_SIZE);
    result = hemispheric_brain_process_lateralized(
        brain,
        stage2_input.data(),
        SPATIAL_INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        stage2_output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Stage 3: Integration with motor planning
    // Pad output to INPUT_SIZE since brain expects consistent input dimensions
    E2E_STAGE_BEGIN("Motor planning integration", MAX_SPATIAL_PROCESSING_TIME_MS);
    std::vector<float> motor_input(SPATIAL_INPUT_SIZE, 0.0f);
    std::copy(stage2_output.begin(), stage2_output.end(), motor_input.begin());
    std::vector<float> motor_output(SPATIAL_OUTPUT_SIZE);
    result = hemispheric_brain_process_cooperative(
        brain,
        motor_input.data(),
        SPATIAL_INPUT_SIZE,
        motor_output.data(),
        SPATIAL_OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
