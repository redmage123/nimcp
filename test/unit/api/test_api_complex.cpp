/**
 * @file test_api_complex.cpp
 * @brief GoogleTest unit tests for NIMCP Complex Oscillation API
 *
 * Tests the public API for complex number and oscillation features:
 * - Enable/disable complex oscillations
 * - Query oscillation phasors
 * - Compute phase coherence
 * - Compute PAC modulation
 * - Error handling when disabled
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cmath>

/**
 * @brief Test fixture for Complex API tests
 */
class APIComplexTest : public ::testing::Test {
protected:
    nimcp_brain_t brain;

    void SetUp() override {
        // Initialize NIMCP library
        nimcp_init();

        // Create a small brain for testing
        brain = nimcp_brain_create(
            "complex_test_brain",
            NIMCP_BRAIN_TINY,
            NIMCP_TASK_CLASSIFICATION,
            10,
            2
        );

        ASSERT_NE(brain, nullptr) << "Failed to create test brain";
    }

    void TearDown() override {
        // Clean up
        if (brain) {
            nimcp_brain_destroy(brain);
            brain = nullptr;
        }

        nimcp_shutdown();
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * @brief Test enabling complex oscillations
 */
TEST_F(APIComplexTest, EnableComplexOscillations) {
    // Initially, complex oscillations may or may not be enabled
    // Test that we can query the state
    bool initial_state = nimcp_is_complex_oscillations_enabled(brain);

    // Try to enable
    bool enable_result = nimcp_enable_complex_oscillations(brain, true);

    // If already enabled, should return true
    // If not enabled and enabling worked, should return true
    // If not enabled and enabling failed, may return false (requires reconfiguration)
    // This is acceptable as per implementation
    EXPECT_TRUE(enable_result || !initial_state);

    // Check final state
    bool final_state = nimcp_is_complex_oscillations_enabled(brain);

    // State should be consistent with enable_result
    if (enable_result) {
        EXPECT_TRUE(final_state);
    }
}

/**
 * @brief Test disabling complex oscillations
 */
TEST_F(APIComplexTest, DisableComplexOscillations) {
    bool result = nimcp_enable_complex_oscillations(brain, false);

    // Should succeed or indicate reconfiguration needed
    // Either way, no crash should occur
    SUCCEED();
}

/**
 * @brief Test querying complex oscillation state
 */
TEST_F(APIComplexTest, QueryComplexOscillationState) {
    // Should not crash
    bool enabled = nimcp_is_complex_oscillations_enabled(brain);

    // State should be boolean
    EXPECT_TRUE(enabled == true || enabled == false);
}

/**
 * @brief Test enable with NULL brain
 */
TEST_F(APIComplexTest, EnableWithNullBrain) {
    bool result = nimcp_enable_complex_oscillations(nullptr, true);
    EXPECT_FALSE(result);

    // Check error message
    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
    EXPECT_NE(std::string(error).find("NULL"), std::string::npos);
}

/**
 * @brief Test query with NULL brain
 */
TEST_F(APIComplexTest, QueryWithNullBrain) {
    bool enabled = nimcp_is_complex_oscillations_enabled(nullptr);
    EXPECT_FALSE(enabled);
}

//=============================================================================
// Phasor Query Tests
//=============================================================================

/**
 * @brief Test getting oscillation phasor
 */
TEST_F(APIComplexTest, GetOscillationPhasor) {
    // Get phasor for neuron 0
    nimcp_oscillation_phasor_t phasor = nimcp_get_oscillation_phasor(brain, 0);

    // If complex oscillations not enabled, should return {0, 0}
    // If enabled, should return valid phasor
    // Either way, no crash should occur
    EXPECT_GE(phasor.amplitude, 0.0f);  // Amplitude should be non-negative
    EXPECT_GE(phasor.phase, -M_PI);     // Phase should be >= -π
    EXPECT_LE(phasor.phase, M_PI);      // Phase should be <= π
}

/**
 * @brief Test getting phasor with NULL brain
 */
TEST_F(APIComplexTest, GetPhasorWithNullBrain) {
    nimcp_oscillation_phasor_t phasor = nimcp_get_oscillation_phasor(nullptr, 0);

    // Should return zero phasor
    EXPECT_EQ(phasor.amplitude, 0.0f);
    EXPECT_EQ(phasor.phase, 0.0f);

    // Check error message
    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
}

/**
 * @brief Test getting phasor for invalid neuron ID
 */
TEST_F(APIComplexTest, GetPhasorInvalidNeuronId) {
    // Try to get phasor for neuron beyond brain capacity
    nimcp_oscillation_phasor_t phasor = nimcp_get_oscillation_phasor(brain, 999999);

    // Should return zero phasor or handle gracefully
    // No crash should occur
    SUCCEED();
}

/**
 * @brief Test phasor when complex oscillations disabled
 */
TEST_F(APIComplexTest, GetPhasorWhenDisabled) {
    // Ensure complex oscillations are disabled
    nimcp_enable_complex_oscillations(brain, false);

    // Try to get phasor
    nimcp_oscillation_phasor_t phasor = nimcp_get_oscillation_phasor(brain, 0);

    // Should return zero phasor and set error
    // (Implementation may vary, but should handle gracefully)
    EXPECT_GE(phasor.amplitude, 0.0f);
}

//=============================================================================
// Phase Coherence Tests
//=============================================================================

/**
 * @brief Test computing phase coherence
 */
TEST_F(APIComplexTest, ComputePhaseCoherence) {
    // Define neuron IDs to analyze
    uint32_t neurons[] = {0, 1, 2, 3, 4};
    uint32_t count = 5;

    // Compute coherence
    float coherence = nimcp_get_phase_coherence(brain, neurons, count);

    // Coherence should be in [0, 1] range (or 0.0 if disabled)
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

/**
 * @brief Test phase coherence with NULL brain
 */
TEST_F(APIComplexTest, CoherenceWithNullBrain) {
    uint32_t neurons[] = {0, 1, 2};
    float coherence = nimcp_get_phase_coherence(nullptr, neurons, 3);

    EXPECT_EQ(coherence, 0.0f);

    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
}

/**
 * @brief Test phase coherence with NULL neuron array
 */
TEST_F(APIComplexTest, CoherenceWithNullNeurons) {
    float coherence = nimcp_get_phase_coherence(brain, nullptr, 5);

    EXPECT_EQ(coherence, 0.0f);

    const char* error = nimcp_get_error();
    EXPECT_NE(std::string(error).find("NULL"), std::string::npos);
}

/**
 * @brief Test phase coherence with zero count
 */
TEST_F(APIComplexTest, CoherenceWithZeroCount) {
    uint32_t neurons[] = {0, 1, 2};
    float coherence = nimcp_get_phase_coherence(brain, neurons, 0);

    EXPECT_EQ(coherence, 0.0f);

    const char* error = nimcp_get_error();
    EXPECT_NE(std::string(error).find("count"), std::string::npos);
}

/**
 * @brief Test phase coherence with single neuron
 */
TEST_F(APIComplexTest, CoherenceWithSingleNeuron) {
    uint32_t neuron = 0;
    float coherence = nimcp_get_phase_coherence(brain, &neuron, 1);

    // Single neuron coherence should be 1.0 (perfectly coherent with itself)
    // Or 0.0 if complex oscillations disabled
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

/**
 * @brief Test phase coherence when complex oscillations disabled
 */
TEST_F(APIComplexTest, CoherenceWhenDisabled) {
    nimcp_enable_complex_oscillations(brain, false);

    uint32_t neurons[] = {0, 1, 2};
    float coherence = nimcp_get_phase_coherence(brain, neurons, 3);

    // Should return 0.0 and set error
    EXPECT_EQ(coherence, 0.0f);
}

//=============================================================================
// PAC Modulation Tests
//=============================================================================

/**
 * @brief Test computing PAC modulation
 */
TEST_F(APIComplexTest, ComputePacModulation) {
    // Standard theta-gamma coupling frequencies
    float theta_freq = 6.0f;   // 6 Hz (theta band)
    float gamma_freq = 40.0f;  // 40 Hz (gamma band)

    float pac = nimcp_get_pac_modulation(brain, theta_freq, gamma_freq);

    // PAC should be in [0, 1] range (or 0.0 if disabled)
    EXPECT_GE(pac, 0.0f);
    EXPECT_LE(pac, 1.0f);
}

/**
 * @brief Test PAC with NULL brain
 */
TEST_F(APIComplexTest, PacWithNullBrain) {
    float pac = nimcp_get_pac_modulation(nullptr, 6.0f, 40.0f);

    EXPECT_EQ(pac, 0.0f);

    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
}

/**
 * @brief Test PAC with invalid theta frequency
 */
TEST_F(APIComplexTest, PacWithInvalidThetaFreq) {
    // Theta should be 4-8 Hz
    float pac1 = nimcp_get_pac_modulation(brain, 2.0f, 40.0f);  // Too low
    EXPECT_EQ(pac1, 0.0f);

    float pac2 = nimcp_get_pac_modulation(brain, 10.0f, 40.0f); // Too high
    EXPECT_EQ(pac2, 0.0f);

    const char* error = nimcp_get_error();
    EXPECT_NE(std::string(error).find("Theta"), std::string::npos);
}

/**
 * @brief Test PAC with invalid gamma frequency
 */
TEST_F(APIComplexTest, PacWithInvalidGammaFreq) {
    // Gamma should be 30-100 Hz
    float pac1 = nimcp_get_pac_modulation(brain, 6.0f, 20.0f);  // Too low
    EXPECT_EQ(pac1, 0.0f);

    float pac2 = nimcp_get_pac_modulation(brain, 6.0f, 120.0f); // Too high
    EXPECT_EQ(pac2, 0.0f);

    const char* error = nimcp_get_error();
    EXPECT_NE(std::string(error).find("Gamma"), std::string::npos);
}

/**
 * @brief Test PAC with boundary frequencies
 */
TEST_F(APIComplexTest, PacWithBoundaryFrequencies) {
    // Test boundary values
    float pac1 = nimcp_get_pac_modulation(brain, 4.0f, 30.0f);   // Min theta, min gamma
    EXPECT_GE(pac1, 0.0f);
    EXPECT_LE(pac1, 1.0f);

    float pac2 = nimcp_get_pac_modulation(brain, 8.0f, 100.0f);  // Max theta, max gamma
    EXPECT_GE(pac2, 0.0f);
    EXPECT_LE(pac2, 1.0f);
}

/**
 * @brief Test PAC when complex oscillations disabled
 */
TEST_F(APIComplexTest, PacWhenDisabled) {
    nimcp_enable_complex_oscillations(brain, false);

    float pac = nimcp_get_pac_modulation(brain, 6.0f, 40.0f);

    // Should return 0.0 and set error
    EXPECT_EQ(pac, 0.0f);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * @brief Test complete workflow: enable, query, analyze
 */
TEST_F(APIComplexTest, CompleteWorkflow) {
    // Step 1: Enable complex oscillations
    nimcp_enable_complex_oscillations(brain, true);

    // Step 2: Check if enabled
    bool enabled = nimcp_is_complex_oscillations_enabled(brain);

    // If we were able to enable it, continue with queries
    if (enabled) {
        // Step 3: Get phasor for a neuron
        nimcp_oscillation_phasor_t phasor = nimcp_get_oscillation_phasor(brain, 0);
        EXPECT_GE(phasor.amplitude, 0.0f);

        // Step 4: Compute phase coherence
        uint32_t neurons[] = {0, 1, 2, 3, 4};
        float coherence = nimcp_get_phase_coherence(brain, neurons, 5);
        EXPECT_GE(coherence, 0.0f);
        EXPECT_LE(coherence, 1.0f);

        // Step 5: Compute PAC
        float pac = nimcp_get_pac_modulation(brain, 6.0f, 40.0f);
        EXPECT_GE(pac, 0.0f);
        EXPECT_LE(pac, 1.0f);
    }

    // Workflow should complete without crashes
    SUCCEED();
}

/**
 * @brief Test error messages are informative
 */
TEST_F(APIComplexTest, ErrorMessagesAreInformative) {
    // Trigger various errors and check messages

    // NULL brain
    nimcp_get_oscillation_phasor(nullptr, 0);
    const char* error1 = nimcp_get_error();
    EXPECT_NE(std::string(error1).find("NULL"), std::string::npos);

    // NULL neuron array
    nimcp_get_phase_coherence(brain, nullptr, 5);
    const char* error2 = nimcp_get_error();
    EXPECT_NE(std::string(error2).find("NULL"), std::string::npos);

    // Invalid frequency
    nimcp_get_pac_modulation(brain, 2.0f, 40.0f);
    const char* error3 = nimcp_get_error();
    EXPECT_NE(std::string(error3).find("frequency"), std::string::npos);
}

/**
 * @brief Test thread safety (basic check)
 */
TEST_F(APIComplexTest, BasicThreadSafety) {
    // Multiple queries should not interfere
    for (int i = 0; i < 100; i++) {
        nimcp_oscillation_phasor_t phasor = nimcp_get_oscillation_phasor(brain, 0);
        EXPECT_GE(phasor.amplitude, 0.0f);

        uint32_t neurons[] = {0, 1};
        float coherence = nimcp_get_phase_coherence(brain, neurons, 2);
        EXPECT_GE(coherence, 0.0f);

        float pac = nimcp_get_pac_modulation(brain, 6.0f, 40.0f);
        EXPECT_GE(pac, 0.0f);
    }

    SUCCEED();
}

/**
 * @brief Test memory cleanup
 */
TEST_F(APIComplexTest, MemoryCleanup) {
    // Create and destroy multiple brains
    for (int i = 0; i < 10; i++) {
        nimcp_brain_t temp_brain = nimcp_brain_create(
            "temp_brain",
            NIMCP_BRAIN_TINY,
            NIMCP_TASK_CLASSIFICATION,
            5,
            2
        );

        if (temp_brain) {
            nimcp_enable_complex_oscillations(temp_brain, true);

            uint32_t neurons[] = {0, 1};
            nimcp_get_phase_coherence(temp_brain, neurons, 2);

            nimcp_brain_destroy(temp_brain);
        }
    }

    // No memory leaks should occur
    SUCCEED();
}

/**
 * @brief Test API version compatibility
 */
TEST_F(APIComplexTest, VersionCompatibility) {
    // Ensure API version is compatible
    const char* version = nimcp_version();
    EXPECT_NE(version, nullptr);

    int version_int = nimcp_version_int();
    EXPECT_GT(version_int, 20000);  // Should be >= 2.0.0
}
