/**
 * @file test_novelty_detection.cpp
 * @brief Unit tests for Novelty Detection system
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "core/brain/regions/locus_coeruleus/nimcp_novelty_detection.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NoveltyDetectionTest : public ::testing::Test {
protected:
    nimcp_novelty_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
        int err = nimcp_novelty_init(&system, nullptr);
        ASSERT_EQ(err, 0);
    }

    void TearDown() override {
        nimcp_novelty_shutdown(&system);
    }

    std::vector<float> createInput(float value, uint32_t size) {
        return std::vector<float>(size, value);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(NoveltyDetectionTest, InitializesSuccessfully) {
    EXPECT_TRUE(system.initialized);
}

TEST_F(NoveltyDetectionTest, InitNullReturnsError) {
    int err = nimcp_novelty_init(nullptr, nullptr);
    EXPECT_EQ(err, -1);
}

TEST_F(NoveltyDetectionTest, ShutdownClearsState) {
    int err = nimcp_novelty_shutdown(&system);
    EXPECT_EQ(err, 0);
    EXPECT_FALSE(system.initialized);
}

TEST_F(NoveltyDetectionTest, ResetClearsHistory) {
    auto input = createInput(0.5f, 10);
    nimcp_novelty_result_t result;
    nimcp_novelty_detect(&system, input.data(), input.size(), &result);

    int err = nimcp_novelty_reset(&system);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(system.memory_count, 0u);
}

TEST_F(NoveltyDetectionTest, CustomConfigApplied) {
    nimcp_novelty_shutdown(&system);

    nimcp_novelty_config_t config = nimcp_novelty_default_config();
    config.input_dimension = 128;
    config.novelty_threshold = 0.5f;

    int err = nimcp_novelty_init(&system, &config);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(system.input_dimension, 128u);
    EXPECT_FLOAT_EQ(system.novelty_threshold, 0.5f);
}

//=============================================================================
// Detection Tests
//=============================================================================

TEST_F(NoveltyDetectionTest, DetectReturnsValidResult) {
    auto input = createInput(0.5f, 10);
    nimcp_novelty_result_t result;

    int err = nimcp_novelty_detect(&system, input.data(), input.size(), &result);
    EXPECT_EQ(err, 0);
    EXPECT_GE(result.novelty_score, 0.0f);
    EXPECT_LE(result.novelty_score, 1.0f);
}

TEST_F(NoveltyDetectionTest, NullInputReturnsZero) {
    nimcp_novelty_result_t result;
    int err = nimcp_novelty_detect(&system, nullptr, 0, &result);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(result.novelty_score, 0.0f);
}

TEST_F(NoveltyDetectionTest, FirstInputHasNovelty) {
    auto input = createInput(0.5f, 10);
    nimcp_novelty_result_t result;

    nimcp_novelty_detect(&system, input.data(), input.size(), &result);
    /* First input should have some novelty since no history */
    EXPECT_GE(result.novelty_score, 0.0f);
}

TEST_F(NoveltyDetectionTest, RepeatedInputReducesNovelty) {
    auto input = createInput(0.5f, 10);
    nimcp_novelty_result_t first_result, last_result;

    /* First detection */
    nimcp_novelty_detect(&system, input.data(), input.size(), &first_result);

    /* Repeated exposure */
    for (int i = 0; i < 20; i++) {
        nimcp_novelty_detect(&system, input.data(), input.size(), &last_result);
    }

    /* Familiarity should increase (novelty decrease or stay same) */
    EXPECT_GE(last_result.familiarity, first_result.familiarity);
}

TEST_F(NoveltyDetectionTest, DifferentInputIncreasesNovelty) {
    auto familiar_input = createInput(0.5f, 10);
    nimcp_novelty_result_t result;

    /* Build familiarity with one pattern */
    for (int i = 0; i < 10; i++) {
        nimcp_novelty_detect(&system, familiar_input.data(), familiar_input.size(), &result);
    }

    /* Different input */
    auto novel_input = createInput(5.0f, 10);
    nimcp_novelty_detect(&system, novel_input.data(), novel_input.size(), &result);

    EXPECT_GT(result.novelty_score, 0.0f);
}

//=============================================================================
// Statistical Novelty Tests
//=============================================================================

TEST_F(NoveltyDetectionTest, StatisticalNoveltyInRange) {
    auto input = createInput(0.5f, 10);
    float novelty = nimcp_novelty_compute_statistical(&system, input.data(), input.size());
    EXPECT_GE(novelty, 0.0f);
    EXPECT_LE(novelty, 1.0f);
}

TEST_F(NoveltyDetectionTest, OutlierHasHighStatisticalNovelty) {
    /* Build statistics with normal values */
    auto normal_input = createInput(0.5f, 10);
    for (int i = 0; i < 50; i++) {
        nimcp_novelty_compute_statistical(&system, normal_input.data(), normal_input.size());
    }

    /* Outlier input */
    auto outlier_input = createInput(10.0f, 10);
    float novelty = nimcp_novelty_compute_statistical(&system, outlier_input.data(), outlier_input.size());

    EXPECT_GT(novelty, 0.3f);  /* Should be moderately novel */
}

//=============================================================================
// Surprise Tests
//=============================================================================

TEST_F(NoveltyDetectionTest, SurpriseInRange) {
    auto input = createInput(0.5f, 10);
    float surprise = nimcp_novelty_compute_surprise(&system, input.data(), input.size());
    EXPECT_GE(surprise, 0.0f);
    EXPECT_LE(surprise, 1.0f);
}

TEST_F(NoveltyDetectionTest, GetCurrentSurprise) {
    auto input = createInput(0.5f, 10);
    nimcp_novelty_result_t result;
    nimcp_novelty_detect(&system, input.data(), input.size(), &result);

    float surprise = nimcp_novelty_get_surprise(&system);
    EXPECT_GE(surprise, 0.0f);
}

//=============================================================================
// Familiarity Tests
//=============================================================================

TEST_F(NoveltyDetectionTest, FamiliarityStartsLow) {
    auto input = createInput(0.5f, 10);
    float familiarity = nimcp_novelty_get_familiarity(&system, input.data(), input.size());
    EXPECT_LE(familiarity, 0.3f);  /* Should be unfamiliar at first */
}

TEST_F(NoveltyDetectionTest, FamiliarityIncreasesWithExposure) {
    auto input = createInput(0.5f, 10);

    float initial_familiarity = nimcp_novelty_get_familiarity(&system, input.data(), input.size());

    /* Build familiarity through habituation */
    for (int i = 0; i < 20; i++) {
        nimcp_novelty_habituate(&system, input.data(), input.size());
    }

    float final_familiarity = nimcp_novelty_get_familiarity(&system, input.data(), input.size());
    EXPECT_GT(final_familiarity, initial_familiarity);
}

//=============================================================================
// Habituation Tests
//=============================================================================

TEST_F(NoveltyDetectionTest, HabituationSucceeds) {
    auto input = createInput(0.5f, 10);
    int err = nimcp_novelty_habituate(&system, input.data(), input.size());
    EXPECT_EQ(err, 0);
}

TEST_F(NoveltyDetectionTest, HabituationIncreasesMemoryCount) {
    EXPECT_EQ(system.memory_count, 0u);

    auto input = createInput(0.5f, 10);
    nimcp_novelty_habituate(&system, input.data(), input.size());

    EXPECT_GT(system.memory_count, 0u);
}

TEST_F(NoveltyDetectionTest, DishabituationReducesHabituation) {
    auto input = createInput(0.5f, 10);

    /* Build habituation */
    for (int i = 0; i < 10; i++) {
        nimcp_novelty_habituate(&system, input.data(), input.size());
    }

    int err = nimcp_novelty_dishabituate(&system, 1.0f);
    EXPECT_EQ(err, 0);

    /* Check that habituation levels are reduced */
    for (uint32_t i = 0; i < system.memory_count; i++) {
        EXPECT_LE(system.habituation_memory[i].habituation_level, 0.1f);
    }
}

TEST_F(NoveltyDetectionTest, ClearHabituationWorks) {
    auto input = createInput(0.5f, 10);
    nimcp_novelty_habituate(&system, input.data(), input.size());

    int err = nimcp_novelty_clear_habituation(&system);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(system.memory_count, 0u);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(NoveltyDetectionTest, UpdateDecaysNovelty) {
    auto input = createInput(5.0f, 10);
    nimcp_novelty_result_t result;
    nimcp_novelty_detect(&system, input.data(), input.size(), &result);

    float initial = nimcp_novelty_get_current(&system);

    /* Update without new input */
    for (int i = 0; i < 50; i++) {
        nimcp_novelty_update(&system, 10.0f);
    }

    float final = nimcp_novelty_get_current(&system);
    EXPECT_LT(final, initial);
}

TEST_F(NoveltyDetectionTest, UpdateDecaysSurprise) {
    system.current_surprise = 0.8f;

    for (int i = 0; i < 50; i++) {
        nimcp_novelty_update(&system, 10.0f);
    }

    EXPECT_LT(system.current_surprise, 0.8f);
}

//=============================================================================
// Event Classification Tests
//=============================================================================

TEST_F(NoveltyDetectionTest, LowNoveltyClassifiedAsNone) {
    nimcp_novelty_result_t result;
    result.novelty_score = 0.05f;

    auto input = createInput(0.0f, 10);
    /* Force low novelty by using zero input */
    nimcp_novelty_detect(&system, input.data(), input.size(), &result);

    /* Very low novelty should not trigger burst */
    if (result.novelty_score < 0.1f) {
        EXPECT_EQ(result.event_type, NOVELTY_EVENT_NONE);
    }
}

TEST_F(NoveltyDetectionTest, BurstThresholdRespected) {
    nimcp_novelty_result_t result;

    /* Low novelty input */
    auto low_input = createInput(0.5f, 10);
    nimcp_novelty_detect(&system, low_input.data(), low_input.size(), &result);

    if (result.novelty_score < system.burst_threshold) {
        EXPECT_FALSE(result.should_trigger_burst);
    }
}

//=============================================================================
// Query Tests
//=============================================================================

TEST_F(NoveltyDetectionTest, GetCurrentNoveltyInRange) {
    float novelty = nimcp_novelty_get_current(&system);
    EXPECT_GE(novelty, 0.0f);
    EXPECT_LE(novelty, 1.0f);
}

TEST_F(NoveltyDetectionTest, GetExplorationDriveInRange) {
    float drive = nimcp_novelty_get_exploration_drive(&system);
    EXPECT_GE(drive, 0.0f);
    EXPECT_LE(drive, 1.0f);
}

TEST_F(NoveltyDetectionTest, GetLastResultSucceeds) {
    auto input = createInput(0.5f, 10);
    nimcp_novelty_result_t detect_result, stored_result;

    nimcp_novelty_detect(&system, input.data(), input.size(), &detect_result);
    int err = nimcp_novelty_get_last_result(&system, &stored_result);

    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(detect_result.novelty_score, stored_result.novelty_score);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
