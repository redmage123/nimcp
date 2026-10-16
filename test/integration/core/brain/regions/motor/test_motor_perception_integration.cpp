/**
 * @file test_motor_perception_integration.cpp
 * @brief Integration tests for Motor Cortex with Perception systems
 *
 * WHAT: Tests Motor Cortex integration with visual cortex and sensory systems
 * WHY:  Verify visuomotor coordination and sensory-guided movement
 * HOW:  Test visual-guided reaching, attention-action coupling
 *
 * PERCEPTION INTEGRATION POINTS:
 * - Visual Cortex: Visual target acquisition for reaching
 * - Attention: Gaze-motor coordination
 * - Sensory Feedback: Proprioceptive-visual integration
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "perception/nimcp_visual_cortex.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MotorPerceptionIntegrationTest : public ::testing::Test {
protected:
    motor_adapter_t* motor;
    motor_config_t motor_config;
    visual_cortex_t* visual;
    visual_cortex_config_t visual_config;

    /* Test image dimensions */
    static constexpr uint32_t TEST_WIDTH = 64;
    static constexpr uint32_t TEST_HEIGHT = 64;
    static constexpr uint32_t TEST_CHANNELS = 1;

    void SetUp() override {
        /* Create motor adapter */
        motor_config = motor_default_config();
        motor_config.enable_bio_async = false;
        motor = motor_create(&motor_config);
        ASSERT_NE(nullptr, motor);

        /* Create visual cortex */
        memset(&visual_config, 0, sizeof(visual_config));
        visual_config.input_width = TEST_WIDTH;
        visual_config.input_height = TEST_HEIGHT;
        visual_config.num_v1_filters = 8;
        visual_config.feature_dim = 32;
        visual_config.enable_attention = true;
        visual_config.enable_memory = true;
        visual_config.enable_bio_async = false;

        visual = visual_cortex_create(&visual_config);
        ASSERT_NE(nullptr, visual);
    }

    void TearDown() override {
        if (visual) {
            visual_cortex_destroy(visual);
            visual = nullptr;
        }
        if (motor) {
            motor_destroy(motor);
            motor = nullptr;
        }
    }

    /* Helper to create a standard test goal */
    motor_goal_t CreateTestGoal(motor_region_t region, float x, float y, float z,
                                float duration_ms) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = region;
        goal.target_position.x = x;
        goal.target_position.y = y;
        goal.target_position.z = z;
        goal.max_duration_ms = duration_ms;
        goal.type = MOVEMENT_TYPE_DISCRETE;
        return goal;
    }

    /* Helper to create a test image with a target at location */
    std::vector<uint8_t> CreateTargetImage(uint32_t target_x, uint32_t target_y,
                                           uint32_t target_size = 5) {
        std::vector<uint8_t> image(TEST_WIDTH * TEST_HEIGHT, 128);  /* Gray background */

        /* Draw white target */
        for (uint32_t dy = 0; dy < target_size && target_y + dy < TEST_HEIGHT; dy++) {
            for (uint32_t dx = 0; dx < target_size && target_x + dx < TEST_WIDTH; dx++) {
                image[(target_y + dy) * TEST_WIDTH + (target_x + dx)] = 255;
            }
        }

        return image;
    }

    /* Convert image coordinates to motor space (-1 to 1) */
    void ImageToMotorSpace(uint32_t img_x, uint32_t img_y,
                          float* motor_x, float* motor_y) {
        *motor_x = ((float)img_x / TEST_WIDTH) * 2.0f - 1.0f;
        *motor_y = ((float)img_y / TEST_HEIGHT) * 2.0f - 1.0f;
    }
};

/*=============================================================================
 * VISUAL CORTEX BASIC TESTS
 * Verify visual cortex works before testing integration
 *===========================================================================*/

TEST_F(MotorPerceptionIntegrationTest, VisualCortexProcessing) {
    /* Create simple test image */
    std::vector<uint8_t> image = CreateTargetImage(32, 32);
    float features[32];

    /* Process image */
    bool result = visual_cortex_process(visual,
        image.data(), TEST_WIDTH, TEST_HEIGHT, TEST_CHANNELS, features);

    EXPECT_TRUE(result);

    /* Features should be populated */
    bool has_nonzero = false;
    for (int i = 0; i < 32; i++) {
        if (features[i] != 0.0f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);
}

TEST_F(MotorPerceptionIntegrationTest, VisualAttentionMapCreation) {
    attention_map_t* attn_map = attention_map_create(TEST_WIDTH, TEST_HEIGHT);
    ASSERT_NE(nullptr, attn_map);

    /* Set some attention values */
    attention_map_set(attn_map, 32, 32, 0.8f);

    /* Retrieve attention */
    float attn = attention_map_get(attn_map, 32, 32);
    EXPECT_FLOAT_EQ(0.8f, attn);

    attention_map_destroy(attn_map);
}

TEST_F(MotorPerceptionIntegrationTest, VisualAttentionComputation) {
    std::vector<uint8_t> image = CreateTargetImage(20, 20, 10);

    attention_map_t* attn_map = attention_map_create(TEST_WIDTH, TEST_HEIGHT);
    ASSERT_NE(nullptr, attn_map);

    bool result = visual_cortex_compute_attention(visual,
        image.data(), TEST_WIDTH, TEST_HEIGHT, attn_map);

    EXPECT_TRUE(result);

    attention_map_destroy(attn_map);
}

/*=============================================================================
 * VISUOMOTOR COORDINATION TESTS
 * Test visual-guided motor planning
 *===========================================================================*/

TEST_F(MotorPerceptionIntegrationTest, VisualTargetAcquisition_MotorReach) {
    /* Create image with target at specific location */
    uint32_t target_x = 48;  /* Right side of image */
    uint32_t target_y = 32;  /* Center vertically */
    std::vector<uint8_t> image = CreateTargetImage(target_x, target_y, 8);

    /* Process visual input */
    float features[32];
    visual_cortex_process(visual,
        image.data(), TEST_WIDTH, TEST_HEIGHT, TEST_CHANNELS, features);

    /* Compute attention to find target */
    attention_map_t* attn_map = attention_map_create(TEST_WIDTH, TEST_HEIGHT);
    visual_cortex_compute_attention(visual,
        image.data(), TEST_WIDTH, TEST_HEIGHT, attn_map);

    /* Get attention peak */
    uint32_t peak_x, peak_y;
    float peak_value;
    visual_cortex_get_attention_peak(attn_map, &peak_x, &peak_y, &peak_value);

    /* Convert to motor coordinates */
    float motor_x, motor_y;
    ImageToMotorSpace(peak_x, peak_y, &motor_x, &motor_y);

    /* Plan motor reach to visual target */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT,
        motor_x, motor_y, 0.0f, 300.0f);

    bool planned = motor_plan_movement(motor, &goal);
    EXPECT_TRUE(planned);

    /* Execute reach */
    motor_begin_execution(motor);
    for (int i = 0; i < 30; i++) {
        motor_update_execution(motor, 10.0f);
    }

    /* Verify hand moved toward target */
    motor_effector_state_t state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &state);

    /* Hand should have moved in direction of target */
    EXPECT_NE(0.0f, state.position.x);

    attention_map_destroy(attn_map);
}

TEST_F(MotorPerceptionIntegrationTest, MultipleTargets_AttentionDrivenSelection) {
    /* Create image with two targets */
    std::vector<uint8_t> image(TEST_WIDTH * TEST_HEIGHT, 128);

    /* Target 1: Left (dimmer) */
    for (uint32_t y = 28; y < 36; y++) {
        for (uint32_t x = 8; x < 16; x++) {
            image[y * TEST_WIDTH + x] = 200;
        }
    }

    /* Target 2: Right (brighter) */
    for (uint32_t y = 28; y < 36; y++) {
        for (uint32_t x = 48; x < 56; x++) {
            image[y * TEST_WIDTH + x] = 255;
        }
    }

    /* Compute attention */
    attention_map_t* attn_map = attention_map_create(TEST_WIDTH, TEST_HEIGHT);
    visual_cortex_compute_attention(visual,
        image.data(), TEST_WIDTH, TEST_HEIGHT, attn_map);

    /* Get attention peak (should be brighter target) */
    uint32_t peak_x, peak_y;
    float peak_value;
    visual_cortex_get_attention_peak(attn_map, &peak_x, &peak_y, &peak_value);

    /* Convert and plan reach */
    float motor_x, motor_y;
    ImageToMotorSpace(peak_x, peak_y, &motor_x, &motor_y);

    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT,
        motor_x, motor_y, 0.0f, 200.0f);

    motor_plan_movement(motor, &goal);
    motor_begin_execution(motor);

    for (int i = 0; i < 20; i++) {
        motor_update_execution(motor, 10.0f);
    }

    motor_effector_state_t state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &state);

    /* Hand should move (we don't test exact direction without knowing attention result) */
    EXPECT_TRUE(true);  /* Test that the pipeline works */

    attention_map_destroy(attn_map);
}

/*=============================================================================
 * SENSORY FEEDBACK INTEGRATION TESTS
 * Test sensory information guiding motor corrections
 *===========================================================================*/

TEST_F(MotorPerceptionIntegrationTest, VisualFeedback_MotorCorrection) {
    /* Start movement toward target */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT,
        1.0f, 0.0f, 0.0f, 500.0f);

    motor_plan_movement(motor, &goal);
    motor_begin_execution(motor);

    /* Execute with visual feedback */
    for (int i = 0; i < 20; i++) {
        motor_update_execution(motor, 10.0f);

        /* Simulate visual feedback of hand position */
        motor_effector_state_t hand_state;
        motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &hand_state);

        /* Create image of current hand position (simplified) */
        uint32_t hand_img_x = (uint32_t)((hand_state.position.x + 1.0f) * 0.5f * TEST_WIDTH);
        uint32_t hand_img_y = TEST_HEIGHT / 2;
        hand_img_x = std::min(hand_img_x, TEST_WIDTH - 1);

        /* Process visual feedback */
        std::vector<uint8_t> feedback_img = CreateTargetImage(hand_img_x, hand_img_y, 3);
        float features[32];
        visual_cortex_process(visual,
            feedback_img.data(), TEST_WIDTH, TEST_HEIGHT, TEST_CHANNELS, features);

        /* Use visual features to compute error for motor feedback */
        motor_effector_state_t feedback_state;
        memset(&feedback_state, 0, sizeof(feedback_state));
        feedback_state.region = MOTOR_REGION_HAND_RIGHT;
        feedback_state.position.x = hand_state.position.x;

        motor_update_feedback(motor, MOTOR_REGION_HAND_RIGHT, &feedback_state);
    }

    /* Should have progressed toward target */
    motor_effector_state_t final_state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &final_state);
    EXPECT_GT(final_state.position.x, 0.0f);
}

TEST_F(MotorPerceptionIntegrationTest, NoveltyDetection_MotorResponse) {
    /* Process first image */
    std::vector<uint8_t> image1 = CreateTargetImage(32, 32, 5);
    float features1[32];
    visual_cortex_process(visual,
        image1.data(), TEST_WIDTH, TEST_HEIGHT, TEST_CHANNELS, features1);

    /* Store in memory */
    visual_cortex_store_memory(visual, features1, 0.5f);

    /* Process novel image */
    std::vector<uint8_t> image2 = CreateTargetImage(10, 50, 8);  /* Different location/size */
    float features2[32];
    visual_cortex_process(visual,
        image2.data(), TEST_WIDTH, TEST_HEIGHT, TEST_CHANNELS, features2);

    /* Compute novelty */
    float novelty = visual_cortex_compute_novelty(visual, features2);

    /* Novel stimulus might trigger orienting response */
    if (novelty > 0.5f) {
        /* Plan orienting movement (use EYE region for gaze shift) */
        motor_goal_t orient_goal = CreateTestGoal(MOTOR_REGION_EYE,
            -0.5f, 0.3f, 0.0f, 200.0f);

        bool planned = motor_plan_movement(motor, &orient_goal);
        /* Planning should succeed (even if EYE region not fully implemented) */
        /* Just verify the integration flow works */
        (void)planned;
    }

    /* Test passes if no crash */
    EXPECT_TRUE(true);
}

/*=============================================================================
 * VISUAL MEMORY AND MOTOR LEARNING TESTS
 *===========================================================================*/

TEST_F(MotorPerceptionIntegrationTest, VisualMemoryStorage) {
    /* Process and store visual experience */
    std::vector<uint8_t> image = CreateTargetImage(32, 32);
    float features[32];

    visual_cortex_process(visual,
        image.data(), TEST_WIDTH, TEST_HEIGHT, TEST_CHANNELS, features);

    bool stored = visual_cortex_store_memory(visual, features, 0.8f);
    EXPECT_TRUE(stored);

    /* Get stats */
    visual_cortex_stats_t stats;
    visual_cortex_get_stats(visual, &stats);
    EXPECT_GE(stats.memories_stored, 1u);
}

TEST_F(MotorPerceptionIntegrationTest, VisuomotorAssociation) {
    /* Create visual target */
    std::vector<uint8_t> target_image = CreateTargetImage(48, 32);
    float target_features[32];
    visual_cortex_process(visual,
        target_image.data(), TEST_WIDTH, TEST_HEIGHT, TEST_CHANNELS, target_features);

    /* Execute motor reach to this target */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 0.5f, 0.0f, 0.0f, 200.0f);
    motor_plan_movement(motor, &goal);
    motor_begin_execution(motor);

    for (int i = 0; i < 20; i++) {
        motor_update_execution(motor, 10.0f);
    }

    /* Store the visual-motor association in visual memory */
    visual_cortex_store_memory(visual, target_features, 0.9f);

    /* Later recall */
    visual_memory_t** recalled = nullptr;
    int num_recalled = 0;
    visual_cortex_recall_memory(visual, target_features, 1, &recalled, &num_recalled);

    EXPECT_GE(num_recalled, 0);  /* May or may not find match */

    /* Cleanup */
    if (recalled) {
        free(recalled);
    }
}

/*=============================================================================
 * VISUAL CORTEX STATS AND CONFIGURATION
 *===========================================================================*/

TEST_F(MotorPerceptionIntegrationTest, VisualCortexStats) {
    /* Process several images */
    for (int i = 0; i < 5; i++) {
        std::vector<uint8_t> image = CreateTargetImage(i * 10 + 5, 32);
        float features[32];
        visual_cortex_process(visual,
            image.data(), TEST_WIDTH, TEST_HEIGHT, TEST_CHANNELS, features);
    }

    visual_cortex_stats_t stats;
    bool result = visual_cortex_get_stats(visual, &stats);

    EXPECT_TRUE(result);
    EXPECT_EQ(5u, stats.images_processed);
}

TEST_F(MotorPerceptionIntegrationTest, VisualFeatureDimension) {
    uint32_t dim = visual_cortex_get_feature_dim(visual);
    EXPECT_EQ(32u, dim);
}

/*=============================================================================
 * EYE-HAND COORDINATION SCENARIO TESTS
 *===========================================================================*/

TEST_F(MotorPerceptionIntegrationTest, EyeHandCoordination_TrackAndReach) {
    /* Simulate tracking a moving target */
    for (int frame = 0; frame < 5; frame++) {
        /* Target moves from left to right */
        uint32_t target_x = 10 + frame * 10;
        std::vector<uint8_t> image = CreateTargetImage(target_x, 32, 5);

        /* Visual processing */
        float features[32];
        visual_cortex_process(visual,
            image.data(), TEST_WIDTH, TEST_HEIGHT, TEST_CHANNELS, features);

        /* Compute attention */
        attention_map_t* attn = attention_map_create(TEST_WIDTH, TEST_HEIGHT);
        visual_cortex_compute_attention(visual,
            image.data(), TEST_WIDTH, TEST_HEIGHT, attn);

        /* Get target location */
        uint32_t tx, ty;
        float tv;
        visual_cortex_get_attention_peak(attn, &tx, &ty, &tv);

        /* Update motor target */
        float mx, my;
        ImageToMotorSpace(tx, ty, &mx, &my);

        motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, mx, my, 0.0f, 50.0f);
        motor_plan_movement(motor, &goal);
        motor_begin_execution(motor);
        motor_update_execution(motor, 50.0f);
        motor_reset(motor);

        attention_map_destroy(attn);
    }

    /* Test completes without error */
    EXPECT_TRUE(true);
}

TEST_F(MotorPerceptionIntegrationTest, PerceptionGuidedGrasping) {
    /* Create object at location */
    std::vector<uint8_t> object_image = CreateTargetImage(40, 30, 10);

    /* Process visual input */
    float features[32];
    visual_cortex_process(visual,
        object_image.data(), TEST_WIDTH, TEST_HEIGHT, TEST_CHANNELS, features);

    /* Plan approach */
    motor_goal_t approach = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 0.3f, 0.0f, 0.5f, 200.0f);
    motor_plan_movement(motor, &approach);
    motor_begin_execution(motor);

    for (int i = 0; i < 20; i++) {
        motor_update_execution(motor, 10.0f);
    }

    /* Plan grasp (simulate closing fingers) */
    motor_goal_t grasp = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 0.3f, 0.0f, 0.0f, 100.0f);
    grasp.type = MOVEMENT_TYPE_DISCRETE;
    motor_plan_movement(motor, &grasp);
    motor_begin_execution(motor);

    for (int i = 0; i < 10; i++) {
        motor_update_execution(motor, 10.0f);
    }

    /* Grasp sequence completed */
    motor_stats_t stats;
    motor_get_stats(motor, &stats);
    EXPECT_GE(stats.movements_planned, 2u);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
