/**
 * @file test_portia_deception.cpp
 * @brief Unit tests for Portia Stealth and Deception System
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_deception.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"

class PortiaDeceptionTest : public ::testing::Test {
protected:
    portia_deception_t deception;
    portia_deception_config_t config;

    void SetUp() override {
        // Initialize configuration
        config.enable_stealth = true;
        config.enable_mimicry = true;
        config.enable_jamming = true;
        config.default_emission_level = 1.0f;
        config.profile_count = 10;
        config.enable_bio_async = false;  // Disabled for unit tests

        // Create deception system
        deception = portia_deception_init(&config);
        ASSERT_NE(deception, nullptr) << "Failed to create deception system";
    }

    void TearDown() override {
        if (deception) {
            portia_deception_destroy(deception);
            deception = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(PortiaDeceptionTest, InitializeWithValidConfig) {
    // Deception already initialized in SetUp
    EXPECT_NE(deception, nullptr);

    stealth_state_t state;
    int result = portia_deception_get_state(deception, &state);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(state.mode, STEALTH_MODE_NONE);
    EXPECT_FLOAT_EQ(state.emission_level, 1.0f);
    EXPECT_EQ(state.mimicry_profile, 0);
    EXPECT_FALSE(state.jamming_active);
}

TEST_F(PortiaDeceptionTest, InitializeWithInvalidConfig) {
    portia_deception_config_t bad_config = config;
    bad_config.default_emission_level = 2.0f;  // Invalid (> 1.0)

    portia_deception_t bad = portia_deception_init(&bad_config);
    EXPECT_EQ(bad, nullptr);
}

TEST_F(PortiaDeceptionTest, InitializeWithNullConfig) {
    portia_deception_t bad = portia_deception_init(nullptr);
    EXPECT_EQ(bad, nullptr);
}

TEST_F(PortiaDeceptionTest, DestroyNullDeception) {
    // Should not crash
    portia_deception_destroy(nullptr);
}

//=============================================================================
// Stealth Mode Tests
//=============================================================================

TEST_F(PortiaDeceptionTest, SetPassiveStealth) {
    int result = portia_deception_set_mode(deception, STEALTH_MODE_PASSIVE);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    stealth_state_t state;
    result = portia_deception_get_state(deception, &state);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(state.mode, STEALTH_MODE_PASSIVE);
}

TEST_F(PortiaDeceptionTest, SetActiveStealth) {
    int result = portia_deception_set_mode(deception, STEALTH_MODE_ACTIVE);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    stealth_state_t state;
    result = portia_deception_get_state(deception, &state);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(state.mode, STEALTH_MODE_ACTIVE);
}

TEST_F(PortiaDeceptionTest, SetMimicryMode) {
    int result = portia_deception_set_mode(deception, STEALTH_MODE_MIMICRY);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    stealth_state_t state;
    result = portia_deception_get_state(deception, &state);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(state.mode, STEALTH_MODE_MIMICRY);
}

TEST_F(PortiaDeceptionTest, SetModeWhenDisabled) {
    // Create deception with stealth disabled
    portia_deception_config_t no_stealth = config;
    no_stealth.enable_stealth = false;
    portia_deception_t no_stealth_deception = portia_deception_init(&no_stealth);
    ASSERT_NE(no_stealth_deception, nullptr);

    int result = portia_deception_set_mode(no_stealth_deception, STEALTH_MODE_PASSIVE);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_SUPPORTED);

    portia_deception_destroy(no_stealth_deception);
}

TEST_F(PortiaDeceptionTest, SetInvalidMode) {
    int result = portia_deception_set_mode(deception, (stealth_mode_t)999);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// Emission Control Tests
//=============================================================================

TEST_F(PortiaDeceptionTest, SetEmissionLevel) {
    int result = portia_deception_emit(deception, 0.5f);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    stealth_state_t state;
    result = portia_deception_get_state(deception, &state);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(state.emission_level, 0.5f);
}

TEST_F(PortiaDeceptionTest, SetEmissionLevelZero) {
    int result = portia_deception_emit(deception, 0.0f);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    stealth_state_t state;
    result = portia_deception_get_state(deception, &state);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(state.emission_level, 0.0f);
}

TEST_F(PortiaDeceptionTest, SetEmissionLevelOne) {
    int result = portia_deception_emit(deception, 1.0f);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    stealth_state_t state;
    result = portia_deception_get_state(deception, &state);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(state.emission_level, 1.0f);
}

TEST_F(PortiaDeceptionTest, SetInvalidEmissionLevel) {
    int result = portia_deception_emit(deception, 1.5f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);

    result = portia_deception_emit(deception, -0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// Effectiveness Tests
//=============================================================================

TEST_F(PortiaDeceptionTest, EffectivenessInNoneMode) {
    portia_deception_set_mode(deception, STEALTH_MODE_NONE);
    float eff = portia_deception_get_effectiveness(deception);
    EXPECT_FLOAT_EQ(eff, 0.0f);
}

TEST_F(PortiaDeceptionTest, EffectivenessInPassiveMode) {
    portia_deception_set_mode(deception, STEALTH_MODE_PASSIVE);
    portia_deception_emit(deception, 0.0f);  // Silent

    float eff = portia_deception_get_effectiveness(deception);
    EXPECT_FLOAT_EQ(eff, 1.0f);  // Maximum effectiveness when silent
}

TEST_F(PortiaDeceptionTest, EffectivenessInPassiveModeWithEmissions) {
    portia_deception_set_mode(deception, STEALTH_MODE_PASSIVE);
    portia_deception_emit(deception, 0.5f);  // Half emissions

    float eff = portia_deception_get_effectiveness(deception);
    EXPECT_FLOAT_EQ(eff, 0.5f);  // Inversely proportional
}

TEST_F(PortiaDeceptionTest, EffectivenessInActiveMode) {
    portia_deception_set_mode(deception, STEALTH_MODE_ACTIVE);
    portia_deception_emit(deception, 0.0f);  // Silent

    float eff = portia_deception_get_effectiveness(deception);
    EXPECT_FLOAT_EQ(eff, 0.8f);  // Base active stealth effectiveness
}

TEST_F(PortiaDeceptionTest, EffectivenessInActiveModeWithJamming) {
    portia_deception_set_mode(deception, STEALTH_MODE_ACTIVE);
    portia_deception_emit(deception, 0.0f);
    portia_deception_jam(deception, true);

    float eff = portia_deception_get_effectiveness(deception);
    EXPECT_FLOAT_EQ(eff, 0.95f);  // 0.8 + 0.15 jamming bonus
}

//=============================================================================
// Mimicry Profile Tests
//=============================================================================

TEST_F(PortiaDeceptionTest, RegisterMimicryProfile) {
    mimicry_profile_t profile;
    memset(&profile, 0, sizeof(profile));
    strncpy(profile.name, "test_prey", sizeof(profile.name) - 1);
    profile.pattern_length = 4;
    profile.signal_pattern[0] = 1.0f;
    profile.signal_pattern[1] = 0.5f;
    profile.signal_pattern[2] = 0.8f;
    profile.signal_pattern[3] = 0.3f;
    profile.effectiveness = 0.85f;

    uint32_t id = portia_deception_register_profile(deception, &profile);
    EXPECT_GT(id, 0);
}

TEST_F(PortiaDeceptionTest, RegisterMultipleProfiles) {
    mimicry_profile_t profile1, profile2;
    memset(&profile1, 0, sizeof(profile1));
    memset(&profile2, 0, sizeof(profile2));

    strncpy(profile1.name, "profile1", sizeof(profile1.name) - 1);
    profile1.effectiveness = 0.7f;

    strncpy(profile2.name, "profile2", sizeof(profile2.name) - 1);
    profile2.effectiveness = 0.8f;

    uint32_t id1 = portia_deception_register_profile(deception, &profile1);
    uint32_t id2 = portia_deception_register_profile(deception, &profile2);

    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, 0);
    EXPECT_NE(id1, id2);
}

TEST_F(PortiaDeceptionTest, RegisterProfileWhenDisabled) {
    // Create deception with mimicry disabled
    portia_deception_config_t no_mimicry = config;
    no_mimicry.enable_mimicry = false;
    portia_deception_t no_mimicry_deception = portia_deception_init(&no_mimicry);
    ASSERT_NE(no_mimicry_deception, nullptr);

    mimicry_profile_t profile;
    memset(&profile, 0, sizeof(profile));
    strncpy(profile.name, "test", sizeof(profile.name) - 1);

    uint32_t id = portia_deception_register_profile(no_mimicry_deception, &profile);
    EXPECT_EQ(id, 0);

    portia_deception_destroy(no_mimicry_deception);
}

TEST_F(PortiaDeceptionTest, GetProfiles) {
    // Register profiles
    mimicry_profile_t profile1, profile2;
    memset(&profile1, 0, sizeof(profile1));
    memset(&profile2, 0, sizeof(profile2));

    strncpy(profile1.name, "profile1", sizeof(profile1.name) - 1);
    profile1.effectiveness = 0.7f;
    strncpy(profile2.name, "profile2", sizeof(profile2.name) - 1);
    profile2.effectiveness = 0.8f;

    uint32_t id1 = portia_deception_register_profile(deception, &profile1);
    uint32_t id2 = portia_deception_register_profile(deception, &profile2);
    ASSERT_GT(id1, 0);
    ASSERT_GT(id2, 0);

    // Get profiles
    mimicry_profile_t retrieved[10];
    uint32_t count = portia_deception_get_profiles(deception, retrieved, 10);

    EXPECT_EQ(count, 2);
    EXPECT_STREQ(retrieved[0].name, "profile1");
    EXPECT_STREQ(retrieved[1].name, "profile2");
}

TEST_F(PortiaDeceptionTest, ActivateMimicry) {
    // Register profile
    mimicry_profile_t profile;
    memset(&profile, 0, sizeof(profile));
    strncpy(profile.name, "test_prey", sizeof(profile.name) - 1);
    profile.effectiveness = 0.85f;

    uint32_t id = portia_deception_register_profile(deception, &profile);
    ASSERT_GT(id, 0);

    // Activate mimicry
    int result = portia_deception_mimic(deception, id);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Check state
    stealth_state_t state;
    result = portia_deception_get_state(deception, &state);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(state.mode, STEALTH_MODE_MIMICRY);
    EXPECT_EQ(state.mimicry_profile, id);
    EXPECT_FLOAT_EQ(state.effectiveness, 0.85f);
}

TEST_F(PortiaDeceptionTest, ActivateNonexistentProfile) {
    int result = portia_deception_mimic(deception, 999);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// Jamming Tests
//=============================================================================

TEST_F(PortiaDeceptionTest, EnableJamming) {
    int result = portia_deception_jam(deception, true);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    stealth_state_t state;
    result = portia_deception_get_state(deception, &state);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_TRUE(state.jamming_active);
}

TEST_F(PortiaDeceptionTest, DisableJamming) {
    portia_deception_jam(deception, true);

    int result = portia_deception_jam(deception, false);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    stealth_state_t state;
    result = portia_deception_get_state(deception, &state);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_FALSE(state.jamming_active);
}

TEST_F(PortiaDeceptionTest, JammingWhenDisabled) {
    // Create deception with jamming disabled
    portia_deception_config_t no_jamming = config;
    no_jamming.enable_jamming = false;
    portia_deception_t no_jamming_deception = portia_deception_init(&no_jamming);
    ASSERT_NE(no_jamming_deception, nullptr);

    int result = portia_deception_jam(no_jamming_deception, true);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_SUPPORTED);

    portia_deception_destroy(no_jamming_deception);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(PortiaDeceptionTest, ConcurrentModeChanges) {
    const int num_threads = 4;
    const int iterations = 20;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, iterations]() {
            for (int i = 0; i < iterations; i++) {
                stealth_mode_t mode = (stealth_mode_t)((t + i) % 4);
                portia_deception_set_mode(deception, mode);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should complete without crashes
    stealth_state_t state;
    int result = portia_deception_get_state(deception, &state);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PortiaDeceptionTest, ConcurrentEmissionChanges) {
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < 20; i++) {
                float level = (float)(i % 11) / 10.0f;
                portia_deception_emit(deception, level);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should complete without crashes
    float eff = portia_deception_get_effectiveness(deception);
    EXPECT_GE(eff, 0.0f);
    EXPECT_LE(eff, 1.0f);
}

TEST_F(PortiaDeceptionTest, ConcurrentProfileRegistration) {
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t, &success_count]() {
            for (int i = 0; i < 2; i++) {
                mimicry_profile_t profile;
                memset(&profile, 0, sizeof(profile));
                snprintf(profile.name, sizeof(profile.name), "profile_%d_%d", t, i);
                profile.effectiveness = 0.7f;

                uint32_t id = portia_deception_register_profile(deception, &profile);
                if (id > 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should have registered multiple profiles
    EXPECT_GT(success_count.load(), 0);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(PortiaDeceptionTest, GetStateWithNullPointer) {
    int result = portia_deception_get_state(deception, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(PortiaDeceptionTest, SetModeWithNullDeception) {
    int result = portia_deception_set_mode(nullptr, STEALTH_MODE_PASSIVE);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(PortiaDeceptionTest, EmitWithNullDeception) {
    int result = portia_deception_emit(nullptr, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(PortiaDeceptionTest, GetEffectivenessWithNullDeception) {
    float eff = portia_deception_get_effectiveness(nullptr);
    EXPECT_LT(eff, 0.0f);  // Returns negative on error
}

TEST_F(PortiaDeceptionTest, RegisterProfileWithNullPointer) {
    uint32_t id = portia_deception_register_profile(deception, nullptr);
    EXPECT_EQ(id, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
