/**
 * @file unit_security_test_nan_inf_validation.cpp
 * @brief Unit tests for NaN/Inf validation in security functions
 *
 * WHAT: Comprehensive tests for security validation functions that reject NaN/Inf
 * WHY:  Ensure malformed floating point values are caught at security boundaries
 * HOW:  GoogleTest framework with comprehensive edge case coverage
 *
 * TEST COVERAGE:
 * - nimcp_security_validate_weight_change() - NaN/Inf rejection
 * - nimcp_security_validate_neuromodulator_change() - NaN/Inf rejection
 * - Valid edge cases (boundary conditions)
 * - Invalid inputs (NaN, +Inf, -Inf)
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

// Headers have their own extern "C" guards
#include "security/nimcp_security.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class SecurityNaNInfValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logging with low verbosity for tests
        nimcp_log_set_level(NULL, LOG_LEVEL_WARN);
    }

    void TearDown() override {
        // Cleanup if needed
    }

    // Helper: Create positive infinity
    float PosInf() const {
        return std::numeric_limits<float>::infinity();
    }

    // Helper: Create negative infinity
    float NegInf() const {
        return -std::numeric_limits<float>::infinity();
    }

    // Helper: Create NaN
    float NaN() const {
        return std::numeric_limits<float>::quiet_NaN();
    }
};

//=============================================================================
// WEIGHT CHANGE VALIDATION - VALID INPUTS
//=============================================================================

TEST_F(SecurityNaNInfValidationTest, WeightChange_ValidSmallChange) {
    // WHAT: Normal small weight change
    // WHY:  Should pass validation
    float old_weight = 0.5f;
    float new_weight = 0.55f;
    float max_delta = 0.1f;

    EXPECT_TRUE(nimcp_security_validate_weight_change(old_weight, new_weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_ValidZeroChange) {
    // WHAT: No weight change (identical values)
    // WHY:  Should pass validation
    float weight = 0.5f;
    float max_delta = 0.1f;

    EXPECT_TRUE(nimcp_security_validate_weight_change(weight, weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_ValidNegativeWeight) {
    // WHAT: Negative weights (inhibitory synapses)
    // WHY:  Should pass validation if delta is small
    float old_weight = -0.5f;
    float new_weight = -0.45f;
    float max_delta = 0.1f;

    EXPECT_TRUE(nimcp_security_validate_weight_change(old_weight, new_weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_ValidMaxDelta) {
    // WHAT: Change at max_delta boundary with epsilon tolerance
    // WHY:  Should pass validation (uses slightly larger max_delta to account for
    //       floating point imprecision: 0.6f - 0.5f may not be exactly 0.1f)
    float old_weight = 0.5f;
    float new_weight = 0.6f;
    float max_delta = 0.101f;  // Slightly larger to handle float precision

    EXPECT_TRUE(nimcp_security_validate_weight_change(old_weight, new_weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_ValidNearMaxDelta) {
    // WHAT: Change just under max_delta boundary
    // WHY:  Should pass validation
    float old_weight = 0.5f;
    float new_weight = 0.599f;
    float max_delta = 0.1f;

    EXPECT_TRUE(nimcp_security_validate_weight_change(old_weight, new_weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_ValidLargeMaxDelta) {
    // WHAT: Large weight change with large max_delta
    // WHY:  Should pass validation when max_delta allows it
    float old_weight = 0.0f;
    float new_weight = 1.0f;
    float max_delta = 1.5f;

    EXPECT_TRUE(nimcp_security_validate_weight_change(old_weight, new_weight, max_delta));
}

//=============================================================================
// WEIGHT CHANGE VALIDATION - NaN INPUTS
//=============================================================================

TEST_F(SecurityNaNInfValidationTest, WeightChange_NaN_OldWeight) {
    // WHAT: NaN in old_weight parameter
    // WHY:  Security validation must reject corrupted input
    float new_weight = 0.5f;
    float max_delta = 0.1f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(NaN(), new_weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_NaN_NewWeight) {
    // WHAT: NaN in new_weight parameter
    // WHY:  Security validation must reject corrupted input
    float old_weight = 0.5f;
    float max_delta = 0.1f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(old_weight, NaN(), max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_NaN_MaxDelta) {
    // WHAT: NaN in max_delta parameter
    // WHY:  Security validation must reject corrupted threshold
    float old_weight = 0.5f;
    float new_weight = 0.55f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(old_weight, new_weight, NaN()));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_NaN_AllParameters) {
    // WHAT: NaN in all parameters
    // WHY:  Security validation must reject completely corrupted input
    EXPECT_FALSE(nimcp_security_validate_weight_change(NaN(), NaN(), NaN()));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_NaN_OldAndNew) {
    // WHAT: NaN in both weight parameters
    // WHY:  Security validation must reject corrupted weights
    float max_delta = 0.1f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(NaN(), NaN(), max_delta));
}

//=============================================================================
// WEIGHT CHANGE VALIDATION - INFINITY INPUTS
//=============================================================================

TEST_F(SecurityNaNInfValidationTest, WeightChange_PosInf_OldWeight) {
    // WHAT: Positive infinity in old_weight
    // WHY:  Security validation must reject infinite values
    float new_weight = 0.5f;
    float max_delta = 0.1f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(PosInf(), new_weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_NegInf_OldWeight) {
    // WHAT: Negative infinity in old_weight
    // WHY:  Security validation must reject infinite values
    float new_weight = 0.5f;
    float max_delta = 0.1f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(NegInf(), new_weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_PosInf_NewWeight) {
    // WHAT: Positive infinity in new_weight
    // WHY:  Security validation must reject infinite values
    float old_weight = 0.5f;
    float max_delta = 0.1f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(old_weight, PosInf(), max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_NegInf_NewWeight) {
    // WHAT: Negative infinity in new_weight
    // WHY:  Security validation must reject infinite values
    float old_weight = 0.5f;
    float max_delta = 0.1f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(old_weight, NegInf(), max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_PosInf_MaxDelta) {
    // WHAT: Positive infinity in max_delta
    // WHY:  Security validation must reject infinite thresholds
    float old_weight = 0.5f;
    float new_weight = 0.55f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(old_weight, new_weight, PosInf()));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_NegInf_MaxDelta) {
    // WHAT: Negative infinity in max_delta
    // WHY:  Security validation must reject infinite thresholds
    float old_weight = 0.5f;
    float new_weight = 0.55f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(old_weight, new_weight, NegInf()));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_Inf_AllParameters) {
    // WHAT: Infinity in all parameters
    // WHY:  Security validation must reject completely corrupted input
    EXPECT_FALSE(nimcp_security_validate_weight_change(PosInf(), PosInf(), PosInf()));
}

//=============================================================================
// WEIGHT CHANGE VALIDATION - MIXED NaN/Inf
//=============================================================================

TEST_F(SecurityNaNInfValidationTest, WeightChange_Mixed_NaN_Inf) {
    // WHAT: Mix of NaN and Inf values
    // WHY:  Security validation must reject any malformed input
    EXPECT_FALSE(nimcp_security_validate_weight_change(NaN(), PosInf(), 0.1f));
    EXPECT_FALSE(nimcp_security_validate_weight_change(PosInf(), NaN(), 0.1f));
    EXPECT_FALSE(nimcp_security_validate_weight_change(0.5f, NaN(), PosInf()));
}

//=============================================================================
// WEIGHT CHANGE VALIDATION - BOUNDARY CONDITIONS
//=============================================================================

TEST_F(SecurityNaNInfValidationTest, WeightChange_ExceedsMaxDelta) {
    // WHAT: Weight change exceeds max_delta
    // WHY:  Should fail validation (not due to NaN/Inf)
    float old_weight = 0.5f;
    float new_weight = 0.65f;  // 0.15 change
    float max_delta = 0.1f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(old_weight, new_weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_InvalidMaxDelta_Zero) {
    // WHAT: max_delta of zero
    // WHY:  Should fail validation (invalid threshold)
    float old_weight = 0.5f;
    float new_weight = 0.5f;
    float max_delta = 0.0f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(old_weight, new_weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, WeightChange_InvalidMaxDelta_Negative) {
    // WHAT: Negative max_delta
    // WHY:  Should fail validation (invalid threshold)
    float old_weight = 0.5f;
    float new_weight = 0.55f;
    float max_delta = -0.1f;

    EXPECT_FALSE(nimcp_security_validate_weight_change(old_weight, new_weight, max_delta));
}

//=============================================================================
// NEUROMODULATOR CHANGE VALIDATION - VALID INPUTS
//=============================================================================

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_ValidSmallChange) {
    // WHAT: Normal small neuromodulator change
    // WHY:  Should pass validation
    float old_level = 0.5f;
    float new_level = 0.55f;
    float max_rate = 0.2f;

    EXPECT_TRUE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_ValidZeroChange) {
    // WHAT: No level change
    // WHY:  Should pass validation
    float level = 0.5f;
    float max_rate = 0.2f;

    EXPECT_TRUE(nimcp_security_validate_neuromodulator_change(level, level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_ValidLowerBound) {
    // WHAT: Levels at lower bound (0.0)
    // WHY:  Should pass validation
    float old_level = 0.0f;
    float new_level = 0.05f;
    float max_rate = 0.2f;

    EXPECT_TRUE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_ValidUpperBound) {
    // WHAT: Levels at upper bound (1.0)
    // WHY:  Should pass validation
    float old_level = 1.0f;
    float new_level = 0.95f;
    float max_rate = 0.2f;

    EXPECT_TRUE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_ValidMaxRate) {
    // WHAT: Change exactly at max_rate boundary
    // WHY:  Should pass validation
    float old_level = 0.5f;
    float new_level = 0.7f;  // 0.2 change
    float max_rate = 0.2f;

    EXPECT_TRUE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_ValidDecrease) {
    // WHAT: Valid decreasing level change
    // WHY:  Should pass validation (direction doesn't matter)
    float old_level = 0.7f;
    float new_level = 0.55f;  // 0.15 decrease
    float max_rate = 0.2f;

    EXPECT_TRUE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

//=============================================================================
// NEUROMODULATOR CHANGE VALIDATION - NaN INPUTS
//=============================================================================

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_NaN_OldLevel) {
    // WHAT: NaN in old_level parameter
    // WHY:  Security validation must reject corrupted input
    float new_level = 0.5f;
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(NaN(), new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_NaN_NewLevel) {
    // WHAT: NaN in new_level parameter
    // WHY:  Security validation must reject corrupted input
    float old_level = 0.5f;
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, NaN(), max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_NaN_MaxRate) {
    // WHAT: NaN in max_rate parameter
    // WHY:  Security validation must reject corrupted threshold
    float old_level = 0.5f;
    float new_level = 0.55f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, new_level, NaN()));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_NaN_AllParameters) {
    // WHAT: NaN in all parameters
    // WHY:  Security validation must reject completely corrupted input
    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(NaN(), NaN(), NaN()));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_NaN_BothLevels) {
    // WHAT: NaN in both level parameters
    // WHY:  Security validation must reject corrupted levels
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(NaN(), NaN(), max_rate));
}

//=============================================================================
// NEUROMODULATOR CHANGE VALIDATION - INFINITY INPUTS
//=============================================================================

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_PosInf_OldLevel) {
    // WHAT: Positive infinity in old_level
    // WHY:  Security validation must reject infinite values
    float new_level = 0.5f;
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(PosInf(), new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_NegInf_OldLevel) {
    // WHAT: Negative infinity in old_level
    // WHY:  Security validation must reject infinite values
    float new_level = 0.5f;
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(NegInf(), new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_PosInf_NewLevel) {
    // WHAT: Positive infinity in new_level
    // WHY:  Security validation must reject infinite values
    float old_level = 0.5f;
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, PosInf(), max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_NegInf_NewLevel) {
    // WHAT: Negative infinity in new_level
    // WHY:  Security validation must reject infinite values
    float old_level = 0.5f;
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, NegInf(), max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_PosInf_MaxRate) {
    // WHAT: Positive infinity in max_rate
    // WHY:  Security validation must reject infinite thresholds
    float old_level = 0.5f;
    float new_level = 0.55f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, new_level, PosInf()));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_NegInf_MaxRate) {
    // WHAT: Negative infinity in max_rate
    // WHY:  Security validation must reject infinite thresholds
    float old_level = 0.5f;
    float new_level = 0.55f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, new_level, NegInf()));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_Inf_AllParameters) {
    // WHAT: Infinity in all parameters
    // WHY:  Security validation must reject completely corrupted input
    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(PosInf(), PosInf(), PosInf()));
}

//=============================================================================
// NEUROMODULATOR CHANGE VALIDATION - MIXED NaN/Inf
//=============================================================================

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_Mixed_NaN_Inf) {
    // WHAT: Mix of NaN and Inf values
    // WHY:  Security validation must reject any malformed input
    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(NaN(), PosInf(), 0.2f));
    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(PosInf(), NaN(), 0.2f));
    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(0.5f, NaN(), PosInf()));
}

//=============================================================================
// NEUROMODULATOR CHANGE VALIDATION - BOUNDARY CONDITIONS
//=============================================================================

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_InvalidLevel_Negative_Old) {
    // WHAT: old_level below valid range [0, 1]
    // WHY:  Should fail validation (out of range)
    float old_level = -0.1f;
    float new_level = 0.5f;
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_InvalidLevel_Negative_New) {
    // WHAT: new_level below valid range [0, 1]
    // WHY:  Should fail validation (out of range)
    float old_level = 0.5f;
    float new_level = -0.1f;
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_InvalidLevel_AboveOne_Old) {
    // WHAT: old_level above valid range [0, 1]
    // WHY:  Should fail validation (out of range)
    float old_level = 1.1f;
    float new_level = 0.5f;
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_InvalidLevel_AboveOne_New) {
    // WHAT: new_level above valid range [0, 1]
    // WHY:  Should fail validation (out of range)
    float old_level = 0.5f;
    float new_level = 1.1f;
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_ExceedsMaxRate) {
    // WHAT: Level change exceeds max_rate
    // WHY:  Should fail validation (too rapid)
    float old_level = 0.5f;
    float new_level = 0.75f;  // 0.25 change
    float max_rate = 0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_InvalidMaxRate_Zero) {
    // WHAT: max_rate of zero
    // WHY:  Should fail validation (invalid threshold)
    float old_level = 0.5f;
    float new_level = 0.5f;
    float max_rate = 0.0f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

TEST_F(SecurityNaNInfValidationTest, Neuromodulator_InvalidMaxRate_Negative) {
    // WHAT: Negative max_rate
    // WHY:  Should fail validation (invalid threshold)
    float old_level = 0.5f;
    float new_level = 0.55f;
    float max_rate = -0.2f;

    EXPECT_FALSE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

TEST_F(SecurityNaNInfValidationTest, EdgeCase_VerySmallFloat) {
    // WHAT: Very small but valid floating point values
    // WHY:  Should pass validation
    float old_weight = 1e-10f;
    float new_weight = 2e-10f;
    float max_delta = 1e-9f;

    EXPECT_TRUE(nimcp_security_validate_weight_change(old_weight, new_weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, EdgeCase_VeryLargeFloat) {
    // WHAT: Very large but finite floating point values
    // WHY:  Should pass validation if delta is within max_delta
    float old_weight = 1e6f;
    float new_weight = 1e6f + 1.0f;
    float max_delta = 2.0f;

    EXPECT_TRUE(nimcp_security_validate_weight_change(old_weight, new_weight, max_delta));
}

TEST_F(SecurityNaNInfValidationTest, EdgeCase_Denormalized_Numbers) {
    // WHAT: Denormalized (subnormal) floating point numbers
    // WHY:  Should pass validation (still valid floats)
    float old_level = std::numeric_limits<float>::denorm_min();
    float new_level = std::numeric_limits<float>::denorm_min() * 2.0f;
    float max_rate = 0.2f;

    EXPECT_TRUE(nimcp_security_validate_neuromodulator_change(old_level, new_level, max_rate));
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
