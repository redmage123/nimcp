/**
 * @file test_circadian.cpp
 * @brief Unit tests for the circadian rhythm module
 *
 * WHAT: Tests for circadian rhythm modulation
 * WHY:  Ensure accurate phase tracking and modulation
 * HOW:  Use GoogleTest framework with phase and modulation validation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/medulla/nimcp_circadian.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CircadianTest : public ::testing::Test {
protected:
    circadian_rhythm_t* circadian = nullptr;

    void SetUp() override {
        circadian_config_t config;
        circadian_default_config(&config);
        circadian = circadian_create(&config);
        ASSERT_NE(circadian, nullptr);
    }

    void TearDown() override {
        if (circadian) {
            circadian_destroy(circadian);
            circadian = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(CircadianTest, DefaultConfig) {
    circadian_config_t config;
    int result = circadian_default_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify free-running period is around 24 hours
    EXPECT_GT(config.free_running_period_hours, 23.0f);
    EXPECT_LT(config.free_running_period_hours, 26.0f);
}

TEST_F(CircadianTest, DefaultConfigNull) {
    int result = circadian_default_config(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CircadianTest, CreateWithNullConfig) {
    circadian_rhythm_t* c = circadian_create(nullptr);
    EXPECT_NE(c, nullptr);
    if (c) circadian_destroy(c);
}

TEST_F(CircadianTest, DestroyNull) {
    circadian_destroy(nullptr);
}

//=============================================================================
// Phase Tests
//=============================================================================

TEST_F(CircadianTest, GetPhase) {
    circadian_phase_t phase = circadian_get_phase(circadian);
    EXPECT_GE((int)phase, 0);
    EXPECT_LT((int)phase, (int)CIRCADIAN_PHASE_COUNT);
}

TEST_F(CircadianTest, GetCyclePosition) {
    float position = circadian_get_cycle_position(circadian);
    EXPECT_GE(position, 0.0f);
    EXPECT_LE(position, 1.0f);
}

TEST_F(CircadianTest, ResetPhase) {
    int result = circadian_reset_phase(circadian, CIRCADIAN_PHASE_MORNING);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    circadian_phase_t phase = circadian_get_phase(circadian);
    EXPECT_EQ(phase, CIRCADIAN_PHASE_MORNING);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(CircadianTest, GetArousalModulation) {
    float mod = circadian_get_modulation(circadian, CIRCADIAN_MODULATION_AROUSAL);
    EXPECT_GE(mod, 0.0f);
    EXPECT_LE(mod, 2.0f);  // Allow some range above 1.0
}

TEST_F(CircadianTest, GetLearningRateModulation) {
    float mod = circadian_get_modulation(circadian, CIRCADIAN_MODULATION_LEARNING_RATE);
    EXPECT_GE(mod, 0.0f);
    EXPECT_LE(mod, 2.0f);
}

TEST_F(CircadianTest, GetConsolidationModulation) {
    float mod = circadian_get_modulation(circadian, CIRCADIAN_MODULATION_CONSOLIDATION);
    EXPECT_GE(mod, 0.0f);
    EXPECT_LE(mod, 2.0f);
}

TEST_F(CircadianTest, GetMetabolismModulation) {
    float mod = circadian_get_modulation(circadian, CIRCADIAN_MODULATION_METABOLISM);
    EXPECT_GE(mod, 0.0f);
    EXPECT_LE(mod, 2.0f);
}

//=============================================================================
// Sleep Pressure Tests
//=============================================================================

TEST_F(CircadianTest, GetSleepPressure) {
    float pressure = circadian_get_sleep_pressure(circadian);
    EXPECT_GE(pressure, 0.0f);
    EXPECT_LE(pressure, 1.0f);
}

//=============================================================================
// Zeitgeber Tests
//=============================================================================

TEST_F(CircadianTest, ApplyZeitgeberLight) {
    int result = circadian_apply_zeitgeber(circadian, CIRCADIAN_ZEITGEBER_LIGHT, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CircadianTest, ApplyZeitgeberActivity) {
    int result = circadian_apply_zeitgeber(circadian, CIRCADIAN_ZEITGEBER_ACTIVITY, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CircadianTest, ApplyZeitgeberSocial) {
    int result = circadian_apply_zeitgeber(circadian, CIRCADIAN_ZEITGEBER_SOCIAL, 0.3f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CircadianTest, ApplyZeitgeberInvalidType) {
    int result = circadian_apply_zeitgeber(circadian, CIRCADIAN_ZEITGEBER_COUNT, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(CircadianTest, Update) {
    circadian_phase_t initial_phase = circadian_get_phase(circadian);

    // Multiple updates shouldn't crash
    for (int i = 0; i < 100; i++) {
        int result = circadian_update(circadian);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Phase should still be valid
    circadian_phase_t phase = circadian_get_phase(circadian);
    EXPECT_GE((int)phase, 0);
    EXPECT_LT((int)phase, (int)CIRCADIAN_PHASE_COUNT);
}

TEST_F(CircadianTest, UpdateNull) {
    int result = circadian_update(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Time Scale Tests
//=============================================================================

TEST_F(CircadianTest, SetTimeScale) {
    int result = circadian_set_time_scale(circadian, 60.0f);  // 1min = 1hour
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CircadianTest, SetTimeScaleNull) {
    int result = circadian_set_time_scale(nullptr, 60.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Phase Name Tests
//=============================================================================

TEST_F(CircadianTest, PhaseNames) {
    const char* name = circadian_phase_name(CIRCADIAN_PHASE_NIGHT_DEEP);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = circadian_phase_name(CIRCADIAN_PHASE_MORNING);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = circadian_phase_name(CIRCADIAN_PHASE_EVENING);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(CircadianTest, ModulationNames) {
    const char* name = circadian_modulation_name(CIRCADIAN_MODULATION_AROUSAL);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = circadian_modulation_name(CIRCADIAN_MODULATION_LEARNING_RATE);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

//=============================================================================
// Bio-async Tests
//=============================================================================

TEST_F(CircadianTest, BioAsyncConnection) {
    bool connected = circadian_is_bio_async_connected(circadian);
    EXPECT_FALSE(connected);

    int result = circadian_connect_bio_async(circadian);
    // Result depends on router availability

    circadian_disconnect_bio_async(circadian);
    connected = circadian_is_bio_async_connected(circadian);
    EXPECT_FALSE(connected);
}

TEST_F(CircadianTest, BioAsyncNullState) {
    bool connected = circadian_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
