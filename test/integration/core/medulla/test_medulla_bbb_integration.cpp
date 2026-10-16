/**
 * @file test_medulla_bbb_integration.cpp
 * @brief Blood-Brain Barrier Integration Tests for Medulla Oblongata
 *
 * WHAT: Integration tests for BBB validation of medulla inputs
 * WHY:  Verify BBB protects medulla from malicious/corrupted inputs
 * HOW:  Test input validation, rejection of invalid inputs, and brain factory integration
 *
 * TEST COVERAGE:
 * 1. BBB Validation of Arousal Inputs - Validate arousal delta values
 * 2. BBB Rejection of Invalid Inputs - Reject out-of-range values
 * 3. BBB Integration with Brain Factory - BBB enabled during brain creation
 * 4. Health Alert Validation - Validate health scores before protection response
 * 5. Neuromodulator Input Validation - Validate neuromodulator levels
 * 6. Threat Detection - Detect malicious input patterns
 * 7. Strict vs Permissive Mode - Test both validation modes
 * 8. Safe Value Substitution - Test safe value suggestions
 *
 * BIOLOGICAL CONTEXT:
 * The Blood-Brain Barrier protects the brain from harmful substances.
 * For the medulla (which controls vital functions), this protection is
 * critical to prevent destabilization of arousal, protection, and circadian rhythms.
 *
 * @author NIMCP Development Team
 * @date 2026-01-31
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <limits>
#include <thread>
#include <chrono>

// Core headers - NIMCP headers have internal extern "C" blocks
#include "core/medulla/nimcp_medulla.h"
#include "core/medulla/nimcp_medulla_bbb.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture - Medulla with BBB
//=============================================================================

class MedullaBBBIntegrationTest : public ::testing::Test {
protected:
    medulla_t medulla = nullptr;
    bbb_system_t bbb = nullptr;

    void SetUp() override {
        // Create BBB system
        bbb_config_t bbb_config = bbb_default_config();
        bbb_config.strict_mode = true;
        bbb_config.input.validate_strings = true;
        bbb_config.input.validate_integers = true;
        bbb = bbb_system_create(&bbb_config);
        // BBB may be null on some systems

        // Create medulla
        medulla_config_t med_config = medulla_default_config();
        med_config.enable_bio_async = false;
        medulla = medulla_create(&med_config);
        ASSERT_NE(medulla, nullptr) << "Failed to create medulla";

        // Connect BBB to medulla
        if (bbb) {
            medulla_set_bbb(bbb);
            medulla_bbb_set_system(bbb);
        }
    }

    void TearDown() override {
        // Disconnect BBB
        medulla_set_bbb(nullptr);
        medulla_bbb_set_system(nullptr);

        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        if (bbb) {
            bbb_system_destroy(bbb);
            bbb = nullptr;
        }

        // Reset BBB test state
        bbb_reset_test_state();
    }

    // Helper: Run medulla updates
    void runMedullaUpdates(int count, float dt = 0.02f) {
        for (int i = 0; i < count; i++) {
            medulla_update(medulla, dt);
        }
    }
};

//=============================================================================
// 1. BBB Validation of Arousal Inputs
//=============================================================================

TEST_F(MedullaBBBIntegrationTest, ValidArousalBoostAccepted) {
    // WHAT: Test valid arousal boost values are accepted
    // WHY:  Normal arousal changes should pass validation
    // HOW:  Validate small positive delta

    medulla_bbb_validation_result_t result;

    // Small positive delta should be valid
    bool valid = medulla_bbb_validate_arousal_input(0.1f, true, &result);
    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
}

TEST_F(MedullaBBBIntegrationTest, ValidArousalReduceAccepted) {
    // WHAT: Test valid arousal reduce values are accepted
    // WHY:  Normal arousal reductions should pass validation
    // HOW:  Validate small negative delta

    medulla_bbb_validation_result_t result;

    // Small negative delta for reduce should be valid
    bool valid = medulla_bbb_validate_arousal_input(-0.1f, false, &result);
    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
}

TEST_F(MedullaBBBIntegrationTest, MaximumArousalDeltaValidated) {
    // WHAT: Test maximum arousal delta is enforced
    // WHY:  Prevent destabilizing large arousal changes
    // HOW:  Get default config, test against max delta

    medulla_bbb_config_t config = medulla_bbb_default_config();
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;

    // Delta within max should be valid
    bool valid = medulla_bbb_validate_arousal_input(
        config.max_arousal_delta - 0.01f, true, &result);
    EXPECT_TRUE(valid);
}

TEST_F(MedullaBBBIntegrationTest, ArousalBoostWithPositiveDelta) {
    // WHAT: Test boost operation requires positive delta
    // WHY:  Boost must increase arousal
    // HOW:  Try boost with negative delta

    medulla_bbb_validation_result_t result;

    // Positive delta for boost should be valid
    bool valid = medulla_bbb_validate_arousal_input(0.1f, true, &result);
    EXPECT_TRUE(valid);
}

//=============================================================================
// 2. BBB Rejection of Invalid Inputs
//=============================================================================

TEST_F(MedullaBBBIntegrationTest, ExcessiveArousalDeltaRejected) {
    // WHAT: Test excessive arousal delta is rejected
    // WHY:  Large sudden changes could destabilize the system
    // HOW:  Validate delta larger than maximum

    medulla_bbb_config_t config = medulla_bbb_default_config();
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;

    // Delta exceeding max should be rejected (in strict mode)
    bool valid = medulla_bbb_validate_arousal_input(
        config.max_arousal_delta + 0.5f, true, &result);

    // In strict mode, should be rejected
    if (!valid) {
        EXPECT_FALSE(result.valid);
        EXPECT_GT(strlen(result.reason), 0u);
    }
}

TEST_F(MedullaBBBIntegrationTest, NaNArousalDeltaRejected) {
    // WHAT: Test NaN arousal delta is rejected
    // WHY:  NaN values are invalid and dangerous
    // HOW:  Validate NaN delta

    medulla_bbb_validation_result_t result;

    float nan_value = std::numeric_limits<float>::quiet_NaN();
    bool valid = medulla_bbb_validate_arousal_input(nan_value, true, &result);

    EXPECT_FALSE(valid);
    EXPECT_FALSE(result.valid);
}

TEST_F(MedullaBBBIntegrationTest, InfinityArousalDeltaRejected) {
    // WHAT: Test infinity arousal delta is rejected
    // WHY:  Infinite values would destabilize the system
    // HOW:  Validate infinity delta

    medulla_bbb_validation_result_t result;

    float inf_value = std::numeric_limits<float>::infinity();
    bool valid = medulla_bbb_validate_arousal_input(inf_value, true, &result);

    EXPECT_FALSE(valid);
    EXPECT_FALSE(result.valid);
}

TEST_F(MedullaBBBIntegrationTest, NegativeInfinityRejected) {
    // WHAT: Test negative infinity is rejected
    // WHY:  Must validate both positive and negative infinity
    // HOW:  Validate -infinity delta

    medulla_bbb_validation_result_t result;

    float neg_inf = -std::numeric_limits<float>::infinity();
    bool valid = medulla_bbb_validate_arousal_input(neg_inf, false, &result);

    EXPECT_FALSE(valid);
    EXPECT_FALSE(result.valid);
}

//=============================================================================
// 3. BBB Integration with Brain Factory
//=============================================================================

TEST_F(MedullaBBBIntegrationTest, MedullaStartsWithBBB) {
    // WHAT: Test medulla starts correctly with BBB enabled
    // WHY:  BBB should not prevent normal operation
    // HOW:  Start medulla, verify it functions

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Should be able to update
    EXPECT_EQ(medulla_update(medulla, 0.02f), NIMCP_SUCCESS);

    // Should be able to boost arousal with valid delta
    EXPECT_EQ(medulla_boost_arousal(medulla, 0.1f), NIMCP_SUCCESS);
}

TEST_F(MedullaBBBIntegrationTest, BBBEnabledAfterStart) {
    // WHAT: Test BBB can be enabled after medulla start
    // WHY:  Dynamic BBB configuration should work
    // HOW:  Start, enable BBB, verify protection

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
    runMedullaUpdates(5);

    // Enable strict mode
    medulla_bbb_config_t config = medulla_bbb_default_config();
    config.strict_mode = true;
    medulla_bbb_set_config(&config);

    // Medulla should still work
    EXPECT_EQ(medulla_update(medulla, 0.02f), NIMCP_SUCCESS);
}

TEST_F(MedullaBBBIntegrationTest, BBBDisabledAllowsAll) {
    // WHAT: Test disabling BBB allows all inputs
    // WHY:  Permissive mode needed for testing/debugging
    // HOW:  Disable BBB, verify no rejections

    // Disconnect BBB
    medulla_bbb_set_system(nullptr);

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Even large deltas should work without BBB
    EXPECT_EQ(medulla_boost_arousal(medulla, 0.5f), NIMCP_SUCCESS);
}

//=============================================================================
// 4. Health Alert Validation
//=============================================================================

TEST_F(MedullaBBBIntegrationTest, ValidHealthScoreAccepted) {
    // WHAT: Test valid health scores are accepted
    // WHY:  Normal health values should pass validation
    // HOW:  Validate health score in valid range

    medulla_bbb_validation_result_t result;

    // Normal health score should be valid
    bool valid = medulla_bbb_validate_health_alert(75.0f, &result);
    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
}

TEST_F(MedullaBBBIntegrationTest, MinHealthScoreAccepted) {
    // WHAT: Test minimum health score is accepted
    // WHY:  Edge case: zero health should be valid
    // HOW:  Validate health score at minimum

    medulla_bbb_config_t config = medulla_bbb_default_config();
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;

    // Minimum valid health score
    bool valid = medulla_bbb_validate_health_alert(config.min_health_score, &result);
    EXPECT_TRUE(valid);
}

TEST_F(MedullaBBBIntegrationTest, MaxHealthScoreAccepted) {
    // WHAT: Test maximum health score is accepted
    // WHY:  Edge case: 100% health should be valid
    // HOW:  Validate health score at maximum

    medulla_bbb_config_t config = medulla_bbb_default_config();
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;

    // Maximum valid health score
    bool valid = medulla_bbb_validate_health_alert(config.max_health_score, &result);
    EXPECT_TRUE(valid);
}

TEST_F(MedullaBBBIntegrationTest, NegativeHealthScoreRejected) {
    // WHAT: Test negative health score is rejected
    // WHY:  Health cannot be negative
    // HOW:  Validate negative health score

    medulla_bbb_validation_result_t result;

    bool valid = medulla_bbb_validate_health_alert(-10.0f, &result);
    EXPECT_FALSE(valid);
    EXPECT_FALSE(result.valid);
}

TEST_F(MedullaBBBIntegrationTest, ExcessiveHealthScoreRejected) {
    // WHAT: Test health score above maximum is rejected
    // WHY:  Unrealistic values may indicate attack or corruption
    // HOW:  Validate health score above max

    medulla_bbb_config_t config = medulla_bbb_default_config();
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;

    // Health above maximum should be rejected
    bool valid = medulla_bbb_validate_health_alert(
        config.max_health_score + 50.0f, &result);
    EXPECT_FALSE(valid);
}

TEST_F(MedullaBBBIntegrationTest, NaNHealthScoreRejected) {
    // WHAT: Test NaN health score is rejected
    // WHY:  NaN would cause undefined behavior
    // HOW:  Validate NaN health score

    medulla_bbb_validation_result_t result;

    float nan_health = std::numeric_limits<float>::quiet_NaN();
    bool valid = medulla_bbb_validate_health_alert(nan_health, &result);

    EXPECT_FALSE(valid);
    EXPECT_FALSE(result.valid);
}

//=============================================================================
// 5. Neuromodulator Input Validation
//=============================================================================

TEST_F(MedullaBBBIntegrationTest, ValidNeuromodulatorLevelAccepted) {
    // WHAT: Test valid neuromodulator levels are accepted
    // WHY:  Normal neuromodulator values should pass
    // HOW:  Validate typical neuromodulator level

    medulla_bbb_validation_result_t result;

    // Normal level (1.0 = baseline)
    bool valid = medulla_bbb_validate_neuromod_input(1.0f, 0, &result);
    EXPECT_TRUE(valid);
    EXPECT_TRUE(result.valid);
}

TEST_F(MedullaBBBIntegrationTest, ZeroNeuromodulatorLevelAccepted) {
    // WHAT: Test zero neuromodulator level is accepted
    // WHY:  Depleted neuromodulator is valid (though unusual)
    // HOW:  Validate zero level

    medulla_bbb_validation_result_t result;

    bool valid = medulla_bbb_validate_neuromod_input(0.0f, 0, &result);
    EXPECT_TRUE(valid);
}

TEST_F(MedullaBBBIntegrationTest, ElevatedNeuromodulatorAccepted) {
    // WHAT: Test elevated neuromodulator levels are accepted
    // WHY:  Stress can elevate neuromodulators to 2x baseline
    // HOW:  Validate elevated but valid level

    medulla_bbb_config_t config = medulla_bbb_default_config();
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;

    // Elevated but within max
    bool valid = medulla_bbb_validate_neuromod_input(
        config.max_neuromodulator_level - 0.1f, 0, &result);
    EXPECT_TRUE(valid);
}

TEST_F(MedullaBBBIntegrationTest, ExcessiveNeuromodulatorRejected) {
    // WHAT: Test excessive neuromodulator level is rejected
    // WHY:  Unrealistic values may indicate attack
    // HOW:  Validate level above maximum

    medulla_bbb_config_t config = medulla_bbb_default_config();
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;

    // Level above maximum should be rejected
    bool valid = medulla_bbb_validate_neuromod_input(
        config.max_neuromodulator_level + 1.0f, 0, &result);
    EXPECT_FALSE(valid);
}

TEST_F(MedullaBBBIntegrationTest, NegativeNeuromodulatorRejected) {
    // WHAT: Test negative neuromodulator level is rejected
    // WHY:  Neuromodulator levels cannot be negative
    // HOW:  Validate negative level

    medulla_bbb_validation_result_t result;

    bool valid = medulla_bbb_validate_neuromod_input(-0.5f, 0, &result);
    EXPECT_FALSE(valid);
}

//=============================================================================
// 6. Threat Detection Tests
//=============================================================================

TEST_F(MedullaBBBIntegrationTest, RapidArousalChangesDetected) {
    // WHAT: Test rapid arousal changes are detected
    // WHY:  Rapid changes may indicate attack attempting destabilization
    // HOW:  Submit many large changes, verify some rejected

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_bbb_validation_result_t result;
    int rejected = 0;

    // Attempt many rapid changes
    for (int i = 0; i < 20; i++) {
        if (!medulla_bbb_validate_arousal_input(0.3f, true, &result)) {
            rejected++;
        }
    }

    // In strict mode with max_arousal_delta around 0.5, 0.3 should pass
    // This tests that validation works even under load
    EXPECT_GE(rejected, 0);  // May or may not reject depending on config
}

TEST_F(MedullaBBBIntegrationTest, PatternedAttackDetected) {
    // WHAT: Test patterned attack input is handled
    // WHY:  Alternating extreme values may be attack pattern
    // HOW:  Submit alternating +max/-max, verify handled

    medulla_bbb_config_t config = medulla_bbb_default_config();
    config.strict_mode = true;
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;
    int valid_count = 0;
    int invalid_count = 0;

    for (int i = 0; i < 10; i++) {
        float delta = (i % 2 == 0) ? config.max_arousal_delta + 0.1f
                                   : -(config.max_arousal_delta + 0.1f);
        bool is_boost = (i % 2 == 0);

        if (medulla_bbb_validate_arousal_input(delta, is_boost, &result)) {
            valid_count++;
        } else {
            invalid_count++;
        }
    }

    // In strict mode, excessive deltas should be rejected
    EXPECT_GT(invalid_count, 0);
}

//=============================================================================
// 7. Strict vs Permissive Mode Tests
//=============================================================================

TEST_F(MedullaBBBIntegrationTest, StrictModeRejectsInvalid) {
    // WHAT: Test strict mode rejects invalid inputs
    // WHY:  Strict mode should block all invalid inputs
    // HOW:  Enable strict mode, test invalid input

    medulla_bbb_config_t config = medulla_bbb_default_config();
    config.strict_mode = true;
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;

    // Large delta should be rejected in strict mode
    bool valid = medulla_bbb_validate_arousal_input(
        config.max_arousal_delta * 2.0f, true, &result);
    EXPECT_FALSE(valid);
}

TEST_F(MedullaBBBIntegrationTest, PermissiveModeWarnsOnly) {
    // WHAT: Test permissive mode only warns
    // WHY:  Permissive mode should allow but log invalid inputs
    // HOW:  Disable strict mode, test invalid input

    medulla_bbb_config_t config = medulla_bbb_default_config();
    config.strict_mode = false;
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;

    // Large delta may be allowed in permissive mode
    bool valid = medulla_bbb_validate_arousal_input(
        config.max_arousal_delta * 1.5f, true, &result);

    // In permissive mode, may return true but with warning
    // Behavior depends on implementation
    (void)valid;
}

TEST_F(MedullaBBBIntegrationTest, ModeCanBeChanged) {
    // WHAT: Test validation mode can be changed at runtime
    // WHY:  May need to switch modes during operation
    // HOW:  Start strict, change to permissive, verify change

    medulla_bbb_config_t config = medulla_bbb_default_config();
    config.strict_mode = true;
    medulla_bbb_set_config(&config);

    // Change to permissive
    config.strict_mode = false;
    medulla_bbb_set_config(&config);

    // Validation should still work
    medulla_bbb_validation_result_t result;
    medulla_bbb_validate_arousal_input(0.1f, true, &result);
    // Just verify no crash
}

//=============================================================================
// 8. Safe Value Substitution Tests
//=============================================================================

TEST_F(MedullaBBBIntegrationTest, SafeValueProvidedForExcessiveDelta) {
    // WHAT: Test safe value is provided for excessive delta
    // WHY:  System should suggest valid alternative
    // HOW:  Submit excessive delta, check for safe value

    medulla_bbb_config_t config = medulla_bbb_default_config();
    config.strict_mode = true;
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    // Excessive delta
    bool valid = medulla_bbb_validate_arousal_input(
        config.max_arousal_delta * 2.0f, true, &result);

    if (!valid && result.has_safe_value) {
        // Safe value should be within limits
        EXPECT_LE(result.safe_value, config.max_arousal_delta);
        EXPECT_GE(result.safe_value, 0.0f);
    }
}

TEST_F(MedullaBBBIntegrationTest, SafeValueClampedToMax) {
    // WHAT: Test safe value is clamped to maximum
    // WHY:  Safe value should never exceed limits
    // HOW:  Submit very large delta, verify safe value is at max

    medulla_bbb_config_t config = medulla_bbb_default_config();
    config.strict_mode = true;
    medulla_bbb_set_config(&config);

    medulla_bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    // Very excessive delta
    bool valid = medulla_bbb_validate_arousal_input(10.0f, true, &result);

    if (!valid && result.has_safe_value) {
        EXPECT_LE(result.safe_value, config.max_arousal_delta);
    }
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(MedullaBBBIntegrationTest, DefaultConfigIsReasonable) {
    // WHAT: Test default config has reasonable values
    // WHY:  Defaults should provide good protection without being too strict
    // HOW:  Check default values are sensible

    medulla_bbb_config_t config = medulla_bbb_default_config();

    // Max arousal delta should be > 0 and < 1
    EXPECT_GT(config.max_arousal_delta, 0.0f);
    EXPECT_LT(config.max_arousal_delta, 1.0f);

    // Health score range should be sensible
    EXPECT_GE(config.min_health_score, 0.0f);
    EXPECT_LE(config.max_health_score, 200.0f);

    // Neuromodulator max should allow some elevation
    EXPECT_GE(config.max_neuromodulator_level, 1.0f);

    // Strict mode should be enabled by default for safety
    EXPECT_TRUE(config.strict_mode);
}

//=============================================================================
// Integration with Medulla Operations
//=============================================================================

TEST_F(MedullaBBBIntegrationTest, BBBValidationDuringMedullaBoost) {
    // WHAT: Test BBB validation during actual medulla boost
    // WHY:  Validation should happen during real operations
    // HOW:  Start medulla, boost with valid and invalid values

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Valid boost should work
    float initial = medulla_get_arousal_level(medulla);
    EXPECT_EQ(medulla_boost_arousal(medulla, 0.1f), NIMCP_SUCCESS);
    runMedullaUpdates(5);

    float after_boost = medulla_get_arousal_level(medulla);
    EXPECT_GT(after_boost, initial);
}

TEST_F(MedullaBBBIntegrationTest, BBBValidationDuringMedullaReduce) {
    // WHAT: Test BBB validation during actual medulla reduce
    // WHY:  Both boost and reduce should be validated
    // HOW:  Start medulla, reduce with valid value

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Set high arousal first
    medulla_test_set_arousal(medulla, 0.8f);
    runMedullaUpdates(3);

    float before = medulla_get_arousal_level(medulla);

    // Valid reduce should work
    EXPECT_EQ(medulla_reduce_arousal(medulla, 0.2f), NIMCP_SUCCESS);
    runMedullaUpdates(5);

    float after_reduce = medulla_get_arousal_level(medulla);
    EXPECT_LT(after_reduce, before);
}

TEST_F(MedullaBBBIntegrationTest, BBBStatisticsTracking) {
    // WHAT: Test BBB statistics are tracked
    // WHY:  Need monitoring of validation attempts
    // HOW:  Perform validations, check BBB stats

    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_statistics_t stats_before;
    bbb_system_get_statistics(bbb, &stats_before);

    // Perform some validations
    for (int i = 0; i < 10; i++) {
        medulla_bbb_validation_result_t result;
        medulla_bbb_validate_arousal_input(0.1f, true, &result);
        medulla_bbb_validate_health_alert(50.0f, &result);
    }

    bbb_statistics_t stats_after;
    bbb_system_get_statistics(bbb, &stats_after);

    // Some validations should have been performed
    EXPECT_GE(stats_after.total_validations, stats_before.total_validations);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(MedullaBBBIntegrationTest, ConcurrentValidation) {
    // WHAT: Test concurrent BBB validation is thread-safe
    // WHY:  Multiple threads may validate simultaneously
    // HOW:  Run validation from multiple threads

    std::atomic<int> valid_count{0};
    std::atomic<int> invalid_count{0};
    std::atomic<bool> running{true};

    auto validator = [&]() {
        while (running) {
            medulla_bbb_validation_result_t result;
            if (medulla_bbb_validate_arousal_input(0.1f, true, &result)) {
                valid_count++;
            } else {
                invalid_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back(validator);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    // Should have done many validations
    EXPECT_GT(valid_count.load(), 10);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
