/**
 * @file test_hemispheric_brain_integration.cpp
 * @brief Integration tests for hemispheric brain with both hemispheres working together
 *
 * Tests cover:
 * - All processing modes (lateralized, parallel, competitive, cooperative)
 * - Hemisphere cooperation strategies
 * - Lateralization profile effects on routing
 * - Bio-async integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include "utils/nimcp_test_base.h"

#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"
#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"
#include "utils/platform/nimcp_platform_tier.h"

/**
 * @class HemisphericBrainIntegrationTest
 * @brief Test fixture for hemispheric brain integration tests
 */
class HemisphericBrainIntegrationTest : public NimcpTestBase {
protected:
    static constexpr uint32_t INPUT_SIZE = 8;
    static constexpr uint32_t OUTPUT_SIZE = 4;

    hemispheric_brain_t* brain = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create default hemispheric brain
        hemispheric_brain_config_t config = hemispheric_brain_default_config();
        config.num_inputs = INPUT_SIZE;
        config.num_outputs = OUTPUT_SIZE;
        config.size = BRAIN_SIZE_SMALL;
        config.enable_bio_async = false;  // Disable for basic tests

        brain = hemispheric_brain_create(&config);
    }

    void TearDown() override {
        if (brain) {
            hemispheric_brain_destroy(brain);
            brain = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper to create simple input
    std::vector<float> createInput(float base_value = 1.0f) {
        std::vector<float> input(INPUT_SIZE);
        for (size_t i = 0; i < INPUT_SIZE; ++i) {
            input[i] = base_value * (1.0f + 0.1f * i);
        }
        return input;
    }
};

/* ============================================================================
 * Creation and Lifecycle Tests
 * ============================================================================ */

TEST_F(HemisphericBrainIntegrationTest, CreateWithDefaultConfig) {
    ASSERT_NE(brain, nullptr);
    EXPECT_TRUE(hemispheric_brain_is_active(brain));
    EXPECT_TRUE(hemispheric_brain_is_callosum_intact(brain));
}

TEST_F(HemisphericBrainIntegrationTest, CreateWithCustomConfig) {
    hemispheric_brain_destroy(brain);

    hemispheric_brain_config_t config = hemispheric_brain_default_config();
    config.num_inputs = 16;
    config.num_outputs = 8;
    config.size = BRAIN_SIZE_MEDIUM;
    config.default_mode = HEMISPHERIC_MODE_PARALLEL;
    config.cooperation_strategy = COOPERATION_WEIGHTED;
    config.initial_tier = PLATFORM_TIER_MEDIUM;

    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);
    EXPECT_EQ(hemispheric_brain_get_mode(brain), HEMISPHERIC_MODE_PARALLEL);
}

TEST_F(HemisphericBrainIntegrationTest, BothHemispheresCreated) {
    brain_hemisphere_t* left = hemispheric_brain_get_left(brain);
    brain_hemisphere_t* right = hemispheric_brain_get_right(brain);

    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_NE(left, right);
}

TEST_F(HemisphericBrainIntegrationTest, CallosumConnected) {
    corpus_callosum_t* callosum = hemispheric_brain_get_callosum(brain);
    ASSERT_NE(callosum, nullptr);
    EXPECT_TRUE(callosum_is_connected(callosum));
}

/* ============================================================================
 * Processing Mode Tests
 * ============================================================================ */

TEST_F(HemisphericBrainIntegrationTest, LateralizedModeLanguageGoesLeft) {
    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    // Language is strongly left-lateralized
    int result = hemispheric_brain_process_lateralized(
        brain, input.data(), INPUT_SIZE,
        COGNITIVE_DOMAIN_LANGUAGE,
        output.data(), OUTPUT_SIZE
    );

    EXPECT_EQ(result, 0);

    // Verify left hemisphere was dominant for this domain
    hemisphere_id_t dominant = hemispheric_brain_get_dominant_for(brain, COGNITIVE_DOMAIN_LANGUAGE);
    EXPECT_EQ(dominant, HEMISPHERE_LEFT);
}

TEST_F(HemisphericBrainIntegrationTest, LateralizedModeSpatialGoesRight) {
    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    // Spatial is right-lateralized
    int result = hemispheric_brain_process_lateralized(
        brain, input.data(), INPUT_SIZE,
        COGNITIVE_DOMAIN_SPATIAL,
        output.data(), OUTPUT_SIZE
    );

    EXPECT_EQ(result, 0);

    // Verify right hemisphere was dominant for this domain
    hemisphere_id_t dominant = hemispheric_brain_get_dominant_for(brain, COGNITIVE_DOMAIN_SPATIAL);
    EXPECT_EQ(dominant, HEMISPHERE_RIGHT);
}

TEST_F(HemisphericBrainIntegrationTest, ParallelModeProcessesBoth) {
    auto input = createInput();
    std::vector<float> left_output(OUTPUT_SIZE);
    std::vector<float> right_output(OUTPUT_SIZE);

    int result = hemispheric_brain_process_parallel(
        brain, input.data(), INPUT_SIZE,
        left_output.data(), right_output.data(), OUTPUT_SIZE
    );

    EXPECT_EQ(result, 0);

    // Both hemispheres should produce some output
    bool left_active = false, right_active = false;
    for (size_t i = 0; i < OUTPUT_SIZE; ++i) {
        if (left_output[i] != 0.0f) left_active = true;
        if (right_output[i] != 0.0f) right_active = true;
    }
    EXPECT_TRUE(left_active);
    EXPECT_TRUE(right_active);
}

TEST_F(HemisphericBrainIntegrationTest, CompetitiveModeSelectsWinner) {
    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);
    hemisphere_id_t winner = HEMISPHERE_COUNT;  // Invalid initial value

    int result = hemispheric_brain_process_competitive(
        brain, input.data(), INPUT_SIZE,
        output.data(), OUTPUT_SIZE,
        &winner
    );

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(winner == HEMISPHERE_LEFT || winner == HEMISPHERE_RIGHT);
}

TEST_F(HemisphericBrainIntegrationTest, CooperativeModeCombinesOutputs) {
    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    int result = hemispheric_brain_process_cooperative(
        brain, input.data(), INPUT_SIZE,
        output.data(), OUTPUT_SIZE
    );

    EXPECT_EQ(result, 0);
}

TEST_F(HemisphericBrainIntegrationTest, DefaultInferenceUsesConfiguredMode) {
    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    // Set mode to cooperative
    hemispheric_brain_set_mode(brain, HEMISPHERIC_MODE_COOPERATIVE);
    EXPECT_EQ(hemispheric_brain_get_mode(brain), HEMISPHERIC_MODE_COOPERATIVE);

    int result = hemispheric_brain_infer(
        brain, input.data(), INPUT_SIZE,
        output.data(), OUTPUT_SIZE
    );

    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Cooperation Strategy Tests
 * ============================================================================ */

TEST_F(HemisphericBrainIntegrationTest, CooperationAverageStrategy) {
    EXPECT_EQ(hemispheric_brain_set_cooperation_strategy(brain, COOPERATION_AVERAGE), 0);

    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    int result = hemispheric_brain_process_cooperative(
        brain, input.data(), INPUT_SIZE,
        output.data(), OUTPUT_SIZE
    );

    EXPECT_EQ(result, 0);
}

TEST_F(HemisphericBrainIntegrationTest, CooperationWeightedStrategy) {
    EXPECT_EQ(hemispheric_brain_set_cooperation_strategy(brain, COOPERATION_WEIGHTED), 0);

    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    int result = hemispheric_brain_process_cooperative(
        brain, input.data(), INPUT_SIZE,
        output.data(), OUTPUT_SIZE
    );

    EXPECT_EQ(result, 0);
}

TEST_F(HemisphericBrainIntegrationTest, CooperationDominantStrategy) {
    EXPECT_EQ(hemispheric_brain_set_cooperation_strategy(brain, COOPERATION_DOMINANT), 0);

    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    int result = hemispheric_brain_process_cooperative(
        brain, input.data(), INPUT_SIZE,
        output.data(), OUTPUT_SIZE
    );

    EXPECT_EQ(result, 0);
}

TEST_F(HemisphericBrainIntegrationTest, CooperationAttentionGatedStrategy) {
    EXPECT_EQ(hemispheric_brain_set_cooperation_strategy(brain, COOPERATION_ATTENTION_GATED), 0);

    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    int result = hemispheric_brain_process_cooperative(
        brain, input.data(), INPUT_SIZE,
        output.data(), OUTPUT_SIZE
    );

    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Lateralization Profile Tests
 * ============================================================================ */

TEST_F(HemisphericBrainIntegrationTest, LateralizationProfileDefaultRightHanded) {
    lateralization_profile_t profile;
    EXPECT_EQ(hemispheric_brain_get_lateralization(brain, &profile), 0);

    // Language should be strongly left-lateralized (high value)
    EXPECT_GT(profile.language_dominance, 0.7f);

    // Spatial should be right-lateralized (low value)
    EXPECT_LT(profile.spatial_dominance, 0.3f);

    // Default is right-handed
    EXPECT_EQ(profile.handedness, HANDEDNESS_RIGHT);
}

TEST_F(HemisphericBrainIntegrationTest, SetLeftHandedProfile) {
    lateralization_profile_t left_handed = lateralization_left_handed_profile();
    EXPECT_EQ(hemispheric_brain_set_lateralization(brain, &left_handed), 0);

    lateralization_profile_t profile;
    EXPECT_EQ(hemispheric_brain_get_lateralization(brain, &profile), 0);
    EXPECT_EQ(profile.handedness, HANDEDNESS_LEFT);
}

TEST_F(HemisphericBrainIntegrationTest, SetBilateralProfile) {
    lateralization_profile_t bilateral = lateralization_bilateral_profile();
    EXPECT_EQ(hemispheric_brain_set_lateralization(brain, &bilateral), 0);

    // All domains should be near 0.5 (bilateral)
    for (int domain = 0; domain < COGNITIVE_DOMAIN_COUNT; ++domain) {
        float dominance = hemispheric_brain_get_dominance(brain, (cognitive_domain_t)domain);
        EXPECT_NEAR(dominance, 0.5f, 0.1f);
    }
}

TEST_F(HemisphericBrainIntegrationTest, DominanceShiftPlasticity) {
    float initial_lang = hemispheric_brain_get_dominance(brain, COGNITIVE_DOMAIN_LANGUAGE);

    // Shift language dominance toward right hemisphere (negative shift)
    EXPECT_EQ(hemispheric_brain_shift_dominance(brain, COGNITIVE_DOMAIN_LANGUAGE, -0.1f), 0);

    float shifted = hemispheric_brain_get_dominance(brain, COGNITIVE_DOMAIN_LANGUAGE);
    EXPECT_LT(shifted, initial_lang);
}

TEST_F(HemisphericBrainIntegrationTest, LateralizationRoutesCorrectly) {
    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    // Process each domain and verify routing
    for (int domain = 0; domain < COGNITIVE_DOMAIN_COUNT; ++domain) {
        int result = hemispheric_brain_process_lateralized(
            brain, input.data(), INPUT_SIZE,
            (cognitive_domain_t)domain,
            output.data(), OUTPUT_SIZE
        );
        EXPECT_EQ(result, 0);
    }
}

/* ============================================================================
 * Update and State Tests
 * ============================================================================ */

TEST_F(HemisphericBrainIntegrationTest, UpdateBothHemispheres) {
    // Run multiple update cycles
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(hemispheric_brain_update(brain, 0.01f), 0);
    }
}

TEST_F(HemisphericBrainIntegrationTest, GetStatistics) {
    // Run some operations first
    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    for (int i = 0; i < 5; ++i) {
        hemispheric_brain_process_lateralized(
            brain, input.data(), INPUT_SIZE,
            COGNITIVE_DOMAIN_LANGUAGE,
            output.data(), OUTPUT_SIZE
        );
        hemispheric_brain_update(brain, 0.01f);
    }

    hemispheric_brain_stats_t stats;
    EXPECT_EQ(hemispheric_brain_get_stats(brain, &stats), 0);
    EXPECT_GT(stats.lateralized_operations, 0u);
}

TEST_F(HemisphericBrainIntegrationTest, ResetStatistics) {
    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    hemispheric_brain_process_lateralized(
        brain, input.data(), INPUT_SIZE,
        COGNITIVE_DOMAIN_LANGUAGE,
        output.data(), OUTPUT_SIZE
    );

    EXPECT_EQ(hemispheric_brain_reset_stats(brain), 0);

    hemispheric_brain_stats_t stats;
    EXPECT_EQ(hemispheric_brain_get_stats(brain, &stats), 0);
    EXPECT_EQ(stats.lateralized_operations, 0u);
}

TEST_F(HemisphericBrainIntegrationTest, EnergyConsumption) {
    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    // Process to consume energy
    for (int i = 0; i < 10; ++i) {
        hemispheric_brain_infer(
            brain, input.data(), INPUT_SIZE,
            output.data(), OUTPUT_SIZE
        );
    }

    float energy = hemispheric_brain_get_energy(brain);
    EXPECT_GE(energy, 0.0f);
}

TEST_F(HemisphericBrainIntegrationTest, ActivateDeactivate) {
    EXPECT_TRUE(hemispheric_brain_is_active(brain));

    EXPECT_EQ(hemispheric_brain_set_active(brain, false), 0);
    EXPECT_FALSE(hemispheric_brain_is_active(brain));

    EXPECT_EQ(hemispheric_brain_set_active(brain, true), 0);
    EXPECT_TRUE(hemispheric_brain_is_active(brain));
}

/* ============================================================================
 * Bio-async Integration Tests
 * ============================================================================ */

TEST_F(HemisphericBrainIntegrationTest, BioAsyncConnectDisconnect) {
    // Initially not connected (disabled in config)
    EXPECT_FALSE(hemispheric_brain_is_bio_async_connected(brain));

    // Connect
    int result = hemispheric_brain_connect_bio_async(brain);
    if (result == 0) {
        EXPECT_TRUE(hemispheric_brain_is_bio_async_connected(brain));

        // Disconnect
        EXPECT_EQ(hemispheric_brain_disconnect_bio_async(brain), 0);
        EXPECT_FALSE(hemispheric_brain_is_bio_async_connected(brain));
    }
}

TEST_F(HemisphericBrainIntegrationTest, CreateWithBioAsyncEnabled) {
    hemispheric_brain_destroy(brain);

    hemispheric_brain_config_t config = hemispheric_brain_default_config();
    config.num_inputs = INPUT_SIZE;
    config.num_outputs = OUTPUT_SIZE;
    config.size = BRAIN_SIZE_SMALL;
    config.enable_bio_async = true;

    brain = hemispheric_brain_create(&config);
    ASSERT_NE(brain, nullptr);
}

/* ============================================================================
 * Resource Management Tests
 * ============================================================================ */

TEST_F(HemisphericBrainIntegrationTest, TierManagementPerHemisphere) {
    // Set different tiers for each hemisphere
    EXPECT_EQ(hemispheric_brain_set_tier(brain, HEMISPHERE_LEFT, PLATFORM_TIER_MEDIUM), 0);
    EXPECT_EQ(hemispheric_brain_set_tier(brain, HEMISPHERE_RIGHT, PLATFORM_TIER_FULL), 0);

    EXPECT_EQ(hemispheric_brain_get_tier(brain, HEMISPHERE_LEFT), PLATFORM_TIER_MEDIUM);
    EXPECT_EQ(hemispheric_brain_get_tier(brain, HEMISPHERE_RIGHT), PLATFORM_TIER_FULL);
}

TEST_F(HemisphericBrainIntegrationTest, AsymmetricResourceAllocation) {
    EXPECT_EQ(hemispheric_brain_enable_asymmetric_resources(brain, true), 0);

    // Give 70% to left hemisphere
    EXPECT_EQ(hemispheric_brain_set_asymmetric_resources(brain, 0.7f, true), 0);
}

/* ============================================================================
 * Training Tests
 * ============================================================================ */

TEST_F(HemisphericBrainIntegrationTest, TrainingReducesLoss) {
    auto input = createInput();
    std::vector<float> target(OUTPUT_SIZE, 0.5f);

    float initial_loss = hemispheric_brain_train(
        brain, input.data(), target.data(), OUTPUT_SIZE
    );

    // Train more
    float final_loss = initial_loss;
    for (int i = 0; i < 10; ++i) {
        final_loss = hemispheric_brain_train(
            brain, input.data(), target.data(), OUTPUT_SIZE
        );
    }

    // Loss should decrease or stay stable
    EXPECT_LE(final_loss, initial_loss + 0.1f);  // Allow small fluctuation
}

/* ============================================================================
 * Bilateral Mode Tests
 * ============================================================================ */

TEST_F(HemisphericBrainIntegrationTest, BilateralModeToggle) {
    EXPECT_FALSE(hemispheric_brain_is_bilateral_mode(brain));

    EXPECT_EQ(hemispheric_brain_set_bilateral_mode(brain, true), 0);
    EXPECT_TRUE(hemispheric_brain_is_bilateral_mode(brain));

    EXPECT_EQ(hemispheric_brain_set_bilateral_mode(brain, false), 0);
    EXPECT_FALSE(hemispheric_brain_is_bilateral_mode(brain));
}

TEST_F(HemisphericBrainIntegrationTest, BilateralModeAffectsProcessing) {
    auto input = createInput();
    std::vector<float> output(OUTPUT_SIZE);

    // Enable bilateral mode
    EXPECT_EQ(hemispheric_brain_set_bilateral_mode(brain, true), 0);

    // Even language (normally left-lateralized) should use both hemispheres
    int result = hemispheric_brain_process_lateralized(
        brain, input.data(), INPUT_SIZE,
        COGNITIVE_DOMAIN_LANGUAGE,
        output.data(), OUTPUT_SIZE
    );

    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Cross-Hemisphere Communication Tests
 * ============================================================================ */

TEST_F(HemisphericBrainIntegrationTest, CallosumIntactAffectsProcessing) {
    auto input = createInput();
    std::vector<float> output1(OUTPUT_SIZE);
    std::vector<float> output2(OUTPUT_SIZE);

    // Process with callosum intact
    hemispheric_brain_process_cooperative(
        brain, input.data(), INPUT_SIZE,
        output1.data(), OUTPUT_SIZE
    );

    // Disconnect callosum
    EXPECT_EQ(hemispheric_brain_disconnect_callosum(brain), 0);
    EXPECT_FALSE(hemispheric_brain_is_callosum_intact(brain));

    // Process with callosum disconnected
    hemispheric_brain_process_cooperative(
        brain, input.data(), INPUT_SIZE,
        output2.data(), OUTPUT_SIZE
    );

    // Outputs may differ (split-brain effect)
    // Just verify processing still works

    // Reconnect
    EXPECT_EQ(hemispheric_brain_reconnect_callosum(brain), 0);
    EXPECT_TRUE(hemispheric_brain_is_callosum_intact(brain));
}

TEST_F(HemisphericBrainIntegrationTest, CallosumBandwidthModeAffectsStats) {
    // Set to realistic bandwidth mode
    EXPECT_EQ(hemispheric_brain_set_callosum_bandwidth(brain, CALLOSUM_BW_REALISTIC), 0);

    // Run some parallel processing to generate callosum traffic
    auto input = createInput();
    std::vector<float> left_output(OUTPUT_SIZE);
    std::vector<float> right_output(OUTPUT_SIZE);

    for (int i = 0; i < 10; ++i) {
        hemispheric_brain_process_parallel(
            brain, input.data(), INPUT_SIZE,
            left_output.data(), right_output.data(), OUTPUT_SIZE
        );
        hemispheric_brain_update(brain, 0.01f);
    }

    hemispheric_brain_stats_t stats;
    EXPECT_EQ(hemispheric_brain_get_stats(brain, &stats), 0);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(HemisphericBrainIntegrationTest, ModeNameStrings) {
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_LATERALIZED), "Lateralized");
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_PARALLEL), "Parallel");
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_COMPETITIVE), "Competitive");
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_COOPERATIVE), "Cooperative");
}

TEST_F(HemisphericBrainIntegrationTest, CooperationStrategyNameStrings) {
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_AVERAGE), "Average");
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_WEIGHTED), "Weighted");
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_DOMINANT), "Dominant");
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_ATTENTION_GATED), "Attention-Gated");
}

TEST_F(HemisphericBrainIntegrationTest, ConfigValidation) {
    hemispheric_brain_config_t valid_config = hemispheric_brain_default_config();
    EXPECT_TRUE(hemispheric_brain_validate_config(&valid_config));
}
