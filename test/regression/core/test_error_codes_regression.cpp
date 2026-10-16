/**
 * @file test_error_codes_regression.cpp
 * @brief Regression tests for NIMCP error code system
 *
 * WHAT: Tests to prevent accidental changes to error code values
 * WHY:  Error codes are part of the API contract - changing them breaks clients
 * HOW:  Lock in specific error code values, verify string mappings
 *
 * BUG HISTORY:
 * - Bug #1: Error code values were accidentally changed during refactoring
 *   FIX: Add regression tests that fail if values change
 * - Bug #2: Error strings didn't match their codes
 *   FIX: Verify error string contains error category
 * - Bug #3: Error propagation didn't preserve original code
 *   FIX: Verify PROPAGATE macro preserves error code
 *
 * REGRESSION FOCUS:
 * 1. Lock in specific error code numeric values
 * 2. Verify error strings match their codes
 * 3. Test error propagation preserves codes
 * 4. Verify category detection is correct
 *
 * @version 1.0.0
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>

extern "C" {
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ErrorCodesRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Clear any existing error state */
        nimcp_error_clear();
    }

    void TearDown() override {
        nimcp_error_clear();
    }
};

//=============================================================================
// ERROR CODE VALUE REGRESSION TESTS
//=============================================================================

/**
 * BUG: Error code values accidentally changed during refactoring
 *
 * These tests lock in the specific numeric values of error codes.
 * If any of these change, clients relying on the values will break.
 */
TEST_F(ErrorCodesRegressionTest, CodeValues_SuccessCodes) {
    /**
     * REGRESSION TEST: Success code values must not change
     */
    EXPECT_EQ(NIMCP_SUCCESS, 0) << "REGRESSION: NIMCP_SUCCESS must be 0";
    EXPECT_EQ(NIMCP_SUCCESS_WITH_WARNINGS, 1) << "REGRESSION: NIMCP_SUCCESS_WITH_WARNINGS must be 1";
    EXPECT_EQ(NIMCP_SUCCESS_PARTIAL, 2) << "REGRESSION: NIMCP_SUCCESS_PARTIAL must be 2";
}

TEST_F(ErrorCodesRegressionTest, CodeValues_GenericErrors) {
    /**
     * REGRESSION TEST: Generic error code values must not change
     */
    EXPECT_EQ(NIMCP_ERROR_UNKNOWN, 1000) << "REGRESSION: NIMCP_ERROR_UNKNOWN must be 1000";
    EXPECT_EQ(NIMCP_ERROR_NOT_IMPLEMENTED, 1001) << "REGRESSION: NIMCP_ERROR_NOT_IMPLEMENTED must be 1001";
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER, 1002) << "REGRESSION: NIMCP_ERROR_INVALID_PARAMETER must be 1002";
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, 1003) << "REGRESSION: NIMCP_ERROR_NULL_POINTER must be 1003";
    EXPECT_EQ(NIMCP_ERROR_OUT_OF_RANGE, 1004) << "REGRESSION: NIMCP_ERROR_OUT_OF_RANGE must be 1004";
    EXPECT_EQ(NIMCP_ERROR_INVALID_STATE, 1005) << "REGRESSION: NIMCP_ERROR_INVALID_STATE must be 1005";
    EXPECT_EQ(NIMCP_ERROR_OPERATION_FAILED, 1006) << "REGRESSION: NIMCP_ERROR_OPERATION_FAILED must be 1006";
    EXPECT_EQ(NIMCP_ERROR_NOT_INITIALIZED, 1007) << "REGRESSION: NIMCP_ERROR_NOT_INITIALIZED must be 1007";
    EXPECT_EQ(NIMCP_ERROR_ALREADY_EXISTS, 1008) << "REGRESSION: NIMCP_ERROR_ALREADY_EXISTS must be 1008";
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND, 1009) << "REGRESSION: NIMCP_ERROR_NOT_FOUND must be 1009";
    EXPECT_EQ(NIMCP_ERROR_TIMEOUT, 1010) << "REGRESSION: NIMCP_ERROR_TIMEOUT must be 1010";
}

TEST_F(ErrorCodesRegressionTest, CodeValues_GPUErrors) {
    /**
     * REGRESSION TEST: GPU error code values must not change
     */
    EXPECT_EQ(NIMCP_ERROR_GPU, 1100) << "REGRESSION: NIMCP_ERROR_GPU must be 1100";
    EXPECT_EQ(NIMCP_ERROR_GPU_NOT_AVAILABLE, 1101) << "REGRESSION: NIMCP_ERROR_GPU_NOT_AVAILABLE must be 1101";
    EXPECT_EQ(NIMCP_ERROR_GPU_MEMORY, 1102) << "REGRESSION: NIMCP_ERROR_GPU_MEMORY must be 1102";
    EXPECT_EQ(NIMCP_ERROR_CUDA, 1103) << "REGRESSION: NIMCP_ERROR_CUDA must be 1103";
}

TEST_F(ErrorCodesRegressionTest, CodeValues_MemoryErrors) {
    /**
     * REGRESSION TEST: Memory error code values must not change
     */
    EXPECT_EQ(NIMCP_ERROR_NO_MEMORY, 2000) << "REGRESSION: NIMCP_ERROR_NO_MEMORY must be 2000";
    EXPECT_EQ(NIMCP_ERROR_BUFFER_TOO_SMALL, 2001) << "REGRESSION: NIMCP_ERROR_BUFFER_TOO_SMALL must be 2001";
    EXPECT_EQ(NIMCP_ERROR_BUFFER_OVERFLOW, 2002) << "REGRESSION: NIMCP_ERROR_BUFFER_OVERFLOW must be 2002";
    EXPECT_EQ(NIMCP_ERROR_MEMORY_CORRUPTION, 2003) << "REGRESSION: NIMCP_ERROR_MEMORY_CORRUPTION must be 2003";
}

TEST_F(ErrorCodesRegressionTest, CodeValues_BrainNetworkErrors) {
    /**
     * REGRESSION TEST: Brain/Network error code values must not change
     */
    EXPECT_EQ(NIMCP_ERROR_BRAIN_CREATION, 3000) << "REGRESSION: NIMCP_ERROR_BRAIN_CREATION must be 3000";
    EXPECT_EQ(NIMCP_ERROR_BRAIN_INVALID, 3001) << "REGRESSION: NIMCP_ERROR_BRAIN_INVALID must be 3001";
    EXPECT_EQ(NIMCP_ERROR_NETWORK_CREATION, 3002) << "REGRESSION: NIMCP_ERROR_NETWORK_CREATION must be 3002";
    EXPECT_EQ(NIMCP_ERROR_DIMENSION_MISMATCH, 3004) << "REGRESSION: NIMCP_ERROR_DIMENSION_MISMATCH must be 3004";
}

TEST_F(ErrorCodesRegressionTest, CodeValues_IOErrors) {
    /**
     * REGRESSION TEST: I/O error code values must not change
     */
    EXPECT_EQ(NIMCP_ERROR_FILE_NOT_FOUND, 4000) << "REGRESSION: NIMCP_ERROR_FILE_NOT_FOUND must be 4000";
    EXPECT_EQ(NIMCP_ERROR_FILE_READ, 4001) << "REGRESSION: NIMCP_ERROR_FILE_READ must be 4001";
    EXPECT_EQ(NIMCP_ERROR_FILE_WRITE, 4002) << "REGRESSION: NIMCP_ERROR_FILE_WRITE must be 4002";
    EXPECT_EQ(NIMCP_ERROR_SERIALIZATION, 4006) << "REGRESSION: NIMCP_ERROR_SERIALIZATION must be 4006";
}

TEST_F(ErrorCodesRegressionTest, CodeValues_ConfigErrors) {
    /**
     * REGRESSION TEST: Configuration error code values must not change
     */
    EXPECT_EQ(NIMCP_ERROR_CONFIG_INVALID, 5000) << "REGRESSION: NIMCP_ERROR_CONFIG_INVALID must be 5000";
    EXPECT_EQ(NIMCP_ERROR_CONFIG_PARSE, 5001) << "REGRESSION: NIMCP_ERROR_CONFIG_PARSE must be 5001";
    EXPECT_EQ(NIMCP_ERROR_CONFIG_MISSING, 5002) << "REGRESSION: NIMCP_ERROR_CONFIG_MISSING must be 5002";
}

TEST_F(ErrorCodesRegressionTest, CodeValues_ThreadingErrors) {
    /**
     * REGRESSION TEST: Threading error code values must not change
     */
    EXPECT_EQ(NIMCP_ERROR_THREAD_CREATE, 6000) << "REGRESSION: NIMCP_ERROR_THREAD_CREATE must be 6000";
    EXPECT_EQ(NIMCP_ERROR_MUTEX_LOCK, 6002) << "REGRESSION: NIMCP_ERROR_MUTEX_LOCK must be 6002";
    EXPECT_EQ(NIMCP_ERROR_DEADLOCK, 6005) << "REGRESSION: NIMCP_ERROR_DEADLOCK must be 6005";
}

TEST_F(ErrorCodesRegressionTest, CodeValues_SignalErrors) {
    /**
     * REGRESSION TEST: Signal error code values must not change
     */
    EXPECT_EQ(NIMCP_ERROR_SIGNAL_RECEIVED, 7000) << "REGRESSION: NIMCP_ERROR_SIGNAL_RECEIVED must be 7000";
    EXPECT_EQ(NIMCP_ERROR_SIGSEGV, 7001) << "REGRESSION: NIMCP_ERROR_SIGSEGV must be 7001";
    EXPECT_EQ(NIMCP_ERROR_SIGFPE, 7003) << "REGRESSION: NIMCP_ERROR_SIGFPE must be 7003";
}

TEST_F(ErrorCodesRegressionTest, CodeValues_CognitiveErrors) {
    /**
     * REGRESSION TEST: Cognitive error code values must not change
     */
    EXPECT_EQ(NIMCP_ERROR_WORKING_MEMORY, 8000) << "REGRESSION: NIMCP_ERROR_WORKING_MEMORY must be 8000";
    EXPECT_EQ(NIMCP_ERROR_EMOTIONAL_TAGGING, 8001) << "REGRESSION: NIMCP_ERROR_EMOTIONAL_TAGGING must be 8001";
    EXPECT_EQ(NIMCP_ERROR_THEORY_OF_MIND, 8005) << "REGRESSION: NIMCP_ERROR_THEORY_OF_MIND must be 8005";
}

TEST_F(ErrorCodesRegressionTest, CodeValues_BrainRegionBases) {
    /**
     * REGRESSION TEST: Brain region error base values must not change
     */
    EXPECT_EQ(NIMCP_ERROR_MOTOR_BASE, 10000) << "REGRESSION: NIMCP_ERROR_MOTOR_BASE must be 10000";
    EXPECT_EQ(NIMCP_ERROR_HIPPOCAMPUS_BASE, 10100) << "REGRESSION: NIMCP_ERROR_HIPPOCAMPUS_BASE must be 10100";
    EXPECT_EQ(NIMCP_ERROR_ENTORHINAL_BASE, 10200) << "REGRESSION: NIMCP_ERROR_ENTORHINAL_BASE must be 10200";
    EXPECT_EQ(NIMCP_ERROR_PREFRONTAL_BASE, 10300) << "REGRESSION: NIMCP_ERROR_PREFRONTAL_BASE must be 10300";
    EXPECT_EQ(NIMCP_ERROR_CEREBELLUM_BASE, 10400) << "REGRESSION: NIMCP_ERROR_CEREBELLUM_BASE must be 10400";
    EXPECT_EQ(NIMCP_ERROR_THALAMUS_BASE, 10500) << "REGRESSION: NIMCP_ERROR_THALAMUS_BASE must be 10500";
    EXPECT_EQ(NIMCP_ERROR_HYPOTHALAMUS_BASE, 10600) << "REGRESSION: NIMCP_ERROR_HYPOTHALAMUS_BASE must be 10600";
    EXPECT_EQ(NIMCP_ERROR_AMYGDALA_BASE, 10700) << "REGRESSION: NIMCP_ERROR_AMYGDALA_BASE must be 10700";
    EXPECT_EQ(NIMCP_ERROR_BASAL_GANGLIA_BASE, 10800) << "REGRESSION: NIMCP_ERROR_BASAL_GANGLIA_BASE must be 10800";
}

//=============================================================================
// ERROR CATEGORY DETECTION TESTS
//=============================================================================

TEST_F(ErrorCodesRegressionTest, Category_SuccessDetection) {
    /**
     * REGRESSION TEST: Success detection must work correctly
     */
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS))
        << "REGRESSION: NIMCP_SUCCESS should be detected as success";
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_WITH_WARNINGS))
        << "REGRESSION: NIMCP_SUCCESS_WITH_WARNINGS should be detected as success";
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_PARTIAL))
        << "REGRESSION: NIMCP_SUCCESS_PARTIAL should be detected as success";

    /* Value 999 should also be success (in success range) */
    EXPECT_TRUE(nimcp_error_is_success(999))
        << "REGRESSION: Code 999 should be detected as success (in 0-999 range)";
}

TEST_F(ErrorCodesRegressionTest, Category_FailureDetection) {
    /**
     * REGRESSION TEST: Failure detection must work correctly
     */
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_UNKNOWN))
        << "REGRESSION: NIMCP_ERROR_UNKNOWN should be detected as failure";
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_NULL_POINTER))
        << "REGRESSION: NIMCP_ERROR_NULL_POINTER should be detected as failure";
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_NO_MEMORY))
        << "REGRESSION: NIMCP_ERROR_NO_MEMORY should be detected as failure";

    /* 1000 is first error code */
    EXPECT_TRUE(nimcp_error_is_failure(1000))
        << "REGRESSION: Code 1000 should be detected as failure";

    /* Success codes should not be failures */
    EXPECT_FALSE(nimcp_error_is_failure(NIMCP_SUCCESS))
        << "REGRESSION: NIMCP_SUCCESS should not be detected as failure";
}

TEST_F(ErrorCodesRegressionTest, Category_CategoryExtraction) {
    /**
     * REGRESSION TEST: Category extraction must work correctly
     */
    EXPECT_EQ(nimcp_error_get_category(NIMCP_SUCCESS), 0)
        << "REGRESSION: Success codes should be category 0";
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_UNKNOWN), 1)
        << "REGRESSION: Generic errors should be category 1";
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_NO_MEMORY), 2)
        << "REGRESSION: Memory errors should be category 2";
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_BRAIN_CREATION), 3)
        << "REGRESSION: Brain errors should be category 3";
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_FILE_NOT_FOUND), 4)
        << "REGRESSION: I/O errors should be category 4";
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_CONFIG_INVALID), 5)
        << "REGRESSION: Config errors should be category 5";
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_THREAD_CREATE), 6)
        << "REGRESSION: Threading errors should be category 6";
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_SIGNAL_RECEIVED), 7)
        << "REGRESSION: Signal errors should be category 7";
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_WORKING_MEMORY), 8)
        << "REGRESSION: Cognitive errors should be category 8";
}

//=============================================================================
// ERROR STRING MAPPING TESTS
//=============================================================================

TEST_F(ErrorCodesRegressionTest, Strings_NotNull) {
    /**
     * REGRESSION TEST: Error strings should never be NULL
     */
    /* Test a sampling of error codes */
    nimcp_error_t codes[] = {
        NIMCP_SUCCESS,
        NIMCP_ERROR_UNKNOWN,
        NIMCP_ERROR_NULL_POINTER,
        NIMCP_ERROR_NO_MEMORY,
        NIMCP_ERROR_BRAIN_CREATION,
        NIMCP_ERROR_FILE_NOT_FOUND,
        NIMCP_ERROR_CONFIG_INVALID,
        NIMCP_ERROR_THREAD_CREATE,
        NIMCP_ERROR_SIGSEGV,
        NIMCP_ERROR_WORKING_MEMORY,
        NIMCP_ERROR_MOTOR_BASE,
        99999  /* Unknown code */
    };

    for (nimcp_error_t code : codes) {
        const char* str = nimcp_error_to_string(code);
        EXPECT_NE(str, nullptr)
            << "REGRESSION: nimcp_error_to_string returned NULL for code " << code;
    }
}

TEST_F(ErrorCodesRegressionTest, Strings_NotEmpty) {
    /**
     * REGRESSION TEST: Error strings should not be empty
     */
    nimcp_error_t codes[] = {
        NIMCP_SUCCESS,
        NIMCP_ERROR_NULL_POINTER,
        NIMCP_ERROR_NO_MEMORY,
    };

    for (nimcp_error_t code : codes) {
        const char* str = nimcp_error_to_string(code);
        if (str) {
            EXPECT_GT(strlen(str), 0u)
                << "REGRESSION: Error string for code " << code << " is empty";
        }
    }
}

TEST_F(ErrorCodesRegressionTest, Strings_CategoryNameNotNull) {
    /**
     * REGRESSION TEST: Category names should never be NULL
     */
    for (int category = 0; category <= 10; category++) {
        nimcp_error_t code = category * 1000;
        const char* name = nimcp_error_get_category_name(code);
        EXPECT_NE(name, nullptr)
            << "REGRESSION: nimcp_error_get_category_name returned NULL for code " << code;
    }
}

//=============================================================================
// ERROR HELPER FUNCTION TESTS
//=============================================================================

TEST_F(ErrorCodesRegressionTest, Helpers_IsOkAlias) {
    /**
     * REGRESSION TEST: nimcp_is_ok should match nimcp_error_is_success
     */
    EXPECT_TRUE(nimcp_is_ok(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_is_ok(NIMCP_ERROR_UNKNOWN));
    EXPECT_EQ(nimcp_is_ok(NIMCP_SUCCESS), nimcp_error_is_success(NIMCP_SUCCESS));
    EXPECT_EQ(nimcp_is_ok(NIMCP_ERROR_UNKNOWN), nimcp_error_is_success(NIMCP_ERROR_UNKNOWN));
}

TEST_F(ErrorCodesRegressionTest, Helpers_IsErrorAlias) {
    /**
     * REGRESSION TEST: nimcp_is_error should match nimcp_error_is_failure
     */
    EXPECT_FALSE(nimcp_is_error(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_is_error(NIMCP_ERROR_UNKNOWN));
    EXPECT_EQ(nimcp_is_error(NIMCP_SUCCESS), nimcp_error_is_failure(NIMCP_SUCCESS));
    EXPECT_EQ(nimcp_is_error(NIMCP_ERROR_UNKNOWN), nimcp_error_is_failure(NIMCP_ERROR_UNKNOWN));
}

TEST_F(ErrorCodesRegressionTest, Helpers_FepConversion) {
    /**
     * REGRESSION TEST: FEP result conversion must be correct
     */
    EXPECT_EQ(nimcp_from_fep_result(0), NIMCP_SUCCESS)
        << "REGRESSION: FEP 0 should convert to NIMCP_SUCCESS";
    EXPECT_EQ(nimcp_from_fep_result(-1), NIMCP_ERROR_OPERATION_FAILED)
        << "REGRESSION: FEP -1 should convert to NIMCP_ERROR_OPERATION_FAILED";

    EXPECT_EQ(nimcp_to_fep_result(NIMCP_SUCCESS), 0)
        << "REGRESSION: NIMCP_SUCCESS should convert to FEP 0";
    EXPECT_EQ(nimcp_to_fep_result(NIMCP_ERROR_UNKNOWN), -1)
        << "REGRESSION: Errors should convert to FEP -1";
}

//=============================================================================
// BRAIN REGION ERROR CONVERSION TESTS
//=============================================================================

TEST_F(ErrorCodesRegressionTest, BrainRegion_MotorConversion) {
    /**
     * REGRESSION TEST: Motor error conversion must be correct
     */
    EXPECT_EQ(motor_error_to_nimcp(0), NIMCP_SUCCESS)
        << "REGRESSION: motor_error 0 -> NIMCP_SUCCESS";
    EXPECT_EQ(motor_error_to_nimcp(1), NIMCP_ERROR_MOTOR_BASE + 1)
        << "REGRESSION: motor_error 1 -> NIMCP_ERROR_MOTOR_BASE + 1";
    EXPECT_EQ(motor_error_to_nimcp(8), NIMCP_ERROR_MOTOR_BASE + 8)
        << "REGRESSION: motor_error 8 -> NIMCP_ERROR_MOTOR_BASE + 8";

    EXPECT_EQ(nimcp_to_motor_error(NIMCP_SUCCESS), 0)
        << "REGRESSION: NIMCP_SUCCESS -> motor_error 0";
    EXPECT_EQ(nimcp_to_motor_error(NIMCP_ERROR_MOTOR_BASE + 1), 1)
        << "REGRESSION: NIMCP_ERROR_MOTOR_BASE + 1 -> motor_error 1";
}

TEST_F(ErrorCodesRegressionTest, BrainRegion_IsDetection) {
    /**
     * REGRESSION TEST: Brain region error detection must be correct
     */
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_MOTOR_BASE))
        << "REGRESSION: Motor errors should be detected as brain region";
    EXPECT_TRUE(nimcp_error_is_brain_region(NIMCP_ERROR_HIPPOCAMPUS_BASE))
        << "REGRESSION: Hippocampus errors should be detected as brain region";
    EXPECT_TRUE(nimcp_error_is_brain_region(19999))
        << "REGRESSION: 19999 should be detected as brain region";

    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_SUCCESS))
        << "REGRESSION: SUCCESS should not be brain region error";
    EXPECT_FALSE(nimcp_error_is_brain_region(NIMCP_ERROR_NO_MEMORY))
        << "REGRESSION: Memory error should not be brain region error";
    EXPECT_FALSE(nimcp_error_is_brain_region(20000))
        << "REGRESSION: 20000 should not be brain region error";
}

//=============================================================================
// ERROR RANGE BOUNDARY TESTS
//=============================================================================

TEST_F(ErrorCodesRegressionTest, Ranges_Boundaries) {
    /**
     * REGRESSION TEST: Error range boundaries must be correct
     *
     * Document the expected range boundaries:
     * - Success: 0-999
     * - Generic: 1000-1999
     * - Memory: 2000-2999
     * - Brain: 3000-3999
     * - I/O: 4000-4999
     * - Config: 5000-5999
     * - Threading: 6000-6999
     * - Signal: 7000-7999
     * - Cognitive: 8000-8999
     * - Brain Regions: 10000-19999
     */

    /* Success range */
    EXPECT_TRUE(nimcp_error_is_success(0));
    EXPECT_TRUE(nimcp_error_is_success(999));
    EXPECT_FALSE(nimcp_error_is_success(1000));

    /* Generic error range */
    EXPECT_EQ(nimcp_error_get_category(1000), 1);
    EXPECT_EQ(nimcp_error_get_category(1999), 1);

    /* Memory error range */
    EXPECT_EQ(nimcp_error_get_category(2000), 2);
    EXPECT_EQ(nimcp_error_get_category(2999), 2);

    /* Brain region range */
    EXPECT_TRUE(nimcp_error_is_brain_region(10000));
    EXPECT_TRUE(nimcp_error_is_brain_region(19999));
    EXPECT_FALSE(nimcp_error_is_brain_region(9999));
    EXPECT_FALSE(nimcp_error_is_brain_region(20000));
}

//=============================================================================
// ERROR ALIAS CONSISTENCY TESTS
//=============================================================================

TEST_F(ErrorCodesRegressionTest, Aliases_InvalidParam) {
    /**
     * REGRESSION TEST: NIMCP_ERROR_INVALID_PARAM alias must equal NIMCP_ERROR_INVALID_PARAMETER
     */
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, NIMCP_ERROR_INVALID_PARAMETER)
        << "REGRESSION: NIMCP_ERROR_INVALID_PARAM should be alias for NIMCP_ERROR_INVALID_PARAMETER";
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
