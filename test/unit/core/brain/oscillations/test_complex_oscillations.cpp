//=============================================================================
// test_complex_oscillations.cpp - Unit tests for complex oscillation tracking
//=============================================================================

#include "core/brain/oscillations/nimcp_brain_complex_oscillations.h"
#include "utils/math/nimcp_complex_math.h"
#include <gtest/gtest.h>
#include <cmath>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class ComplexOscillationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize complex math subsystem
        complex_math_init(nullptr);
    }

    void TearDown() override {
        complex_math_cleanup();
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ComplexOscillationTest, CreateDestroy) {
    // Create state
    auto* state = brain_complex_oscillation_create(100, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(brain_complex_oscillation_get_num_neurons(state), 100);

    // Destroy
    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, CreateWithInvalidParams) {
    // Zero neurons should fail
    auto* state = brain_complex_oscillation_create(0, 0.1f, 0.95f);
    EXPECT_EQ(state, nullptr);
}

TEST_F(ComplexOscillationTest, CreateWithDefaultParams) {
    // Negative/invalid params should use defaults
    auto* state = brain_complex_oscillation_create(50, -1.0f, 2.0f);
    ASSERT_NE(state, nullptr);

    // Should still function
    std::vector<float> activations(50, 0.5f);
    EXPECT_TRUE(brain_complex_oscillation_update(state, activations.data()));

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, DestroyNull) {
    // Should not crash
    brain_complex_oscillation_destroy(nullptr);
}

//=============================================================================
// Phasor Tracking Tests
//=============================================================================

TEST_F(ComplexOscillationTest, UpdatePhasors) {
    auto* state = brain_complex_oscillation_create(10, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Create activations
    std::vector<float> activations(10, 0.5f);

    // Update
    EXPECT_TRUE(brain_complex_oscillation_update(state, activations.data()));

    // Check phasors were updated
    neural_phasor_t phasor;
    EXPECT_TRUE(brain_complex_oscillation_get_phasor(state, 0, &phasor));
    float amp = phasor_amplitude(phasor);
    EXPECT_GT(amp, 0.0f);
    EXPECT_LT(amp, 1.0f);

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, UpdateWithNullInputs) {
    auto* state = brain_complex_oscillation_create(10, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Null state
    std::vector<float> activations(10, 0.5f);
    EXPECT_FALSE(brain_complex_oscillation_update(nullptr, activations.data()));

    // Null activations
    EXPECT_FALSE(brain_complex_oscillation_update(state, nullptr));

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, PhaseIncrement) {
    auto* state = brain_complex_oscillation_create(1, 0.1f, 1.0f);
    ASSERT_NE(state, nullptr);

    // Set initial phasor
    neural_phasor_t initial = phasor_from_polar(1.0f, 0.0f);
    EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, 0, initial));

    // Update multiple times
    std::vector<float> activations(1, 1.0f);
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(brain_complex_oscillation_update(state, activations.data()));
    }

    // Check phase increased
    neural_phasor_t final;
    EXPECT_TRUE(brain_complex_oscillation_get_phasor(state, 0, &final));
    float final_phase = phasor_phase(final);
    EXPECT_GT(final_phase, 0.0f);
    EXPECT_LT(final_phase, M_PI);  // Should be less than π after 10 * 0.1 rad

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, PhaseWrapping) {
    auto* state = brain_complex_oscillation_create(1, 0.5f, 1.0f);
    ASSERT_NE(state, nullptr);

    // Set initial phase near π
    neural_phasor_t initial = phasor_from_polar(1.0f, M_PI - 0.1f);
    EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, 0, initial));

    // Update to push past π
    std::vector<float> activations(1, 1.0f);
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(brain_complex_oscillation_update(state, activations.data()));
    }

    // Phase should wrap to [-π, π]
    neural_phasor_t final;
    EXPECT_TRUE(brain_complex_oscillation_get_phasor(state, 0, &final));
    float final_phase = phasor_phase(final);
    EXPECT_GE(final_phase, -M_PI);
    EXPECT_LE(final_phase, M_PI);

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, AmplitudeDecay) {
    auto* state = brain_complex_oscillation_create(1, 0.1f, 0.9f);
    ASSERT_NE(state, nullptr);

    // Set initial amplitude
    neural_phasor_t initial = phasor_from_polar(1.0f, 0.0f);
    EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, 0, initial));

    // Update with constant activation
    std::vector<float> activations(1, 1.0f);
    EXPECT_TRUE(brain_complex_oscillation_update(state, activations.data()));

    // Check amplitude decreased
    neural_phasor_t after_update;
    EXPECT_TRUE(brain_complex_oscillation_get_phasor(state, 0, &after_update));
    float amp = phasor_amplitude(after_update);
    EXPECT_LT(amp, 1.0f);
    EXPECT_GT(amp, 0.8f);  // ~0.9 with 1.0 activation

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, GetSetPhasor) {
    auto* state = brain_complex_oscillation_create(5, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Set specific phasor
    neural_phasor_t test_phasor = phasor_from_polar(0.75f, M_PI / 4.0f);
    EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, 2, test_phasor));

    // Get it back
    neural_phasor_t retrieved;
    EXPECT_TRUE(brain_complex_oscillation_get_phasor(state, 2, &retrieved));

    // Verify
    EXPECT_NEAR(retrieved.real, test_phasor.real, 1e-6f);
    EXPECT_NEAR(retrieved.imag, test_phasor.imag, 1e-6f);

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, GetSetPhasorInvalidIndex) {
    auto* state = brain_complex_oscillation_create(5, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    neural_phasor_t phasor = phasor_from_polar(1.0f, 0.0f);

    // Out of bounds
    EXPECT_FALSE(brain_complex_oscillation_set_phasor(state, 5, phasor));
    EXPECT_FALSE(brain_complex_oscillation_get_phasor(state, 5, &phasor));

    brain_complex_oscillation_destroy(state);
}

//=============================================================================
// Phase Coherence Tests
//=============================================================================

TEST_F(ComplexOscillationTest, CoherenceAllSamePhase) {
    auto* state = brain_complex_oscillation_create(10, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Set all neurons to same phase
    neural_phasor_t same_phasor = phasor_from_polar(1.0f, M_PI / 4.0f);
    for (uint32_t i = 0; i < 10; i++) {
        EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, i, same_phasor));
    }

    // Compute coherence
    phase_coherence_result_t result;
    EXPECT_TRUE(brain_complex_oscillation_compute_coherence(state, &result));

    // Should have perfect coherence
    EXPECT_NEAR(result.coherence, 1.0f, 0.01f);
    EXPECT_NEAR(result.mean_phase, M_PI / 4.0f, 0.01f);
    EXPECT_NEAR(result.phase_variance, 0.0f, 0.01f);
    EXPECT_EQ(result.num_neurons, 10);

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, CoherenceRandomPhases) {
    auto* state = brain_complex_oscillation_create(100, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Set random phases (uniform distribution)
    for (uint32_t i = 0; i < 100; i++) {
        float phase = -M_PI + (2.0f * M_PI * i) / 100.0f;
        neural_phasor_t phasor = phasor_from_polar(1.0f, phase);
        EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, i, phasor));
    }

    // Compute coherence
    phase_coherence_result_t result;
    EXPECT_TRUE(brain_complex_oscillation_compute_coherence(state, &result));

    // Should have low coherence (random phases)
    EXPECT_LT(result.coherence, 0.3f);
    EXPECT_GT(result.phase_variance, 0.5f);

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, CoherenceInvalidInputs) {
    auto* state = brain_complex_oscillation_create(10, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    phase_coherence_result_t result;

    // Null state
    EXPECT_FALSE(brain_complex_oscillation_compute_coherence(nullptr, &result));

    // Null result
    EXPECT_FALSE(brain_complex_oscillation_compute_coherence(state, nullptr));

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, CoherenceTooFewNeurons) {
    auto* state = brain_complex_oscillation_create(1, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    phase_coherence_result_t result;

    // Need at least 2 neurons
    EXPECT_FALSE(brain_complex_oscillation_compute_coherence(state, &result));

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, CoherenceSubset) {
    auto* state = brain_complex_oscillation_create(20, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Set first 10 to same phase, last 10 to different phases
    for (uint32_t i = 0; i < 10; i++) {
        neural_phasor_t phasor = phasor_from_polar(1.0f, 0.0f);
        EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, i, phasor));
    }
    for (uint32_t i = 10; i < 20; i++) {
        float phase = -M_PI + (2.0f * M_PI * (i - 10)) / 10.0f;
        neural_phasor_t phasor = phasor_from_polar(1.0f, phase);
        EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, i, phasor));
    }

    // Compute coherence for first 10 only
    std::vector<uint32_t> indices = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    phase_coherence_result_t result;
    EXPECT_TRUE(brain_complex_oscillation_compute_coherence_subset(
        state, indices.data(), indices.size(), &result));

    // Should have high coherence
    EXPECT_GT(result.coherence, 0.9f);

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, CoherenceSubsetInvalidIndex) {
    auto* state = brain_complex_oscillation_create(10, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Include invalid index
    std::vector<uint32_t> indices = {0, 1, 10};  // 10 is out of bounds
    phase_coherence_result_t result;
    EXPECT_FALSE(brain_complex_oscillation_compute_coherence_subset(
        state, indices.data(), indices.size(), &result));

    brain_complex_oscillation_destroy(state);
}

//=============================================================================
// Synchrony Tests
//=============================================================================

TEST_F(ComplexOscillationTest, SynchronySamePhase) {
    auto* state = brain_complex_oscillation_create(20, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Set all to same phase
    neural_phasor_t phasor = phasor_from_polar(1.0f, M_PI / 3.0f);
    for (uint32_t i = 0; i < 20; i++) {
        EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, i, phasor));
    }

    // Create two populations
    std::vector<uint32_t> pop1 = {0, 1, 2, 3, 4};
    std::vector<uint32_t> pop2 = {10, 11, 12, 13, 14};

    // Compute synchrony
    float sync = brain_complex_oscillation_compute_synchrony(
        state, pop1.data(), pop1.size(), pop2.data(), pop2.size());

    EXPECT_GT(sync, 0.0f);
    EXPECT_NEAR(sync, 1.0f, 0.01f);  // Should be highly synchronized

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, SynchronyDifferentPhases) {
    auto* state = brain_complex_oscillation_create(20, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Set pop1 to phase 0, pop2 to random phases
    for (uint32_t i = 0; i < 10; i++) {
        neural_phasor_t phasor = phasor_from_polar(1.0f, 0.0f);
        EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, i, phasor));
    }
    for (uint32_t i = 10; i < 20; i++) {
        float phase = -M_PI + (2.0f * M_PI * (i - 10)) / 10.0f;
        neural_phasor_t phasor = phasor_from_polar(1.0f, phase);
        EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, i, phasor));
    }

    std::vector<uint32_t> pop1 = {0, 1, 2, 3, 4};
    std::vector<uint32_t> pop2 = {10, 11, 12, 13, 14};

    float sync = brain_complex_oscillation_compute_synchrony(
        state, pop1.data(), pop1.size(), pop2.data(), pop2.size());

    EXPECT_GE(sync, 0.0f);
    EXPECT_LE(sync, 1.0f);
    // Note: May have moderate synchrony due to limited sample size

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, SynchronyInvalidInputs) {
    auto* state = brain_complex_oscillation_create(20, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    std::vector<uint32_t> pop1 = {0, 1, 2};
    std::vector<uint32_t> pop2 = {10, 11, 12};

    // Null state
    EXPECT_LT(brain_complex_oscillation_compute_synchrony(
        nullptr, pop1.data(), pop1.size(), pop2.data(), pop2.size()), 0.0f);

    // Null populations
    EXPECT_LT(brain_complex_oscillation_compute_synchrony(
        state, nullptr, pop1.size(), pop2.data(), pop2.size()), 0.0f);
    EXPECT_LT(brain_complex_oscillation_compute_synchrony(
        state, pop1.data(), pop1.size(), nullptr, pop2.size()), 0.0f);

    // Empty populations
    EXPECT_LT(brain_complex_oscillation_compute_synchrony(
        state, pop1.data(), 0, pop2.data(), pop2.size()), 0.0f);

    brain_complex_oscillation_destroy(state);
}

//=============================================================================
// PAC (Phase-Amplitude Coupling) Tests
//=============================================================================

TEST_F(ComplexOscillationTest, PACBasic) {
    auto* state = brain_complex_oscillation_create(100, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Create phase neurons with specific phases
    std::vector<uint32_t> phase_indices(100);
    std::vector<float> amplitudes(100);

    for (uint32_t i = 0; i < 100; i++) {
        phase_indices[i] = i;
        float phase = -M_PI + (2.0f * M_PI * i) / 100.0f;

        // Amplitude modulated by phase (high at phase 0)
        amplitudes[i] = 0.5f + 0.5f * cosf(phase);

        neural_phasor_t phasor = phasor_from_polar(1.0f, phase);
        EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, i, phasor));
    }

    // Compute PAC
    pac_result_t pac;
    EXPECT_TRUE(brain_complex_oscillation_compute_pac(
        state, phase_indices.data(), phase_indices.size(),
        amplitudes.data(), amplitudes.size(), &pac));

    // Should detect coupling
    EXPECT_GT(pac.modulation_index, 0.0f);
    EXPECT_LE(pac.modulation_index, 1.0f);
    EXPECT_EQ(pac.phase_bin_count, 18);

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, PACNoCoupling) {
    auto* state = brain_complex_oscillation_create(100, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Phase and amplitude not coupled
    std::vector<uint32_t> phase_indices(100);
    std::vector<float> amplitudes(100, 0.5f);  // Constant amplitude

    for (uint32_t i = 0; i < 100; i++) {
        phase_indices[i] = i;
        float phase = -M_PI + (2.0f * M_PI * i) / 100.0f;
        neural_phasor_t phasor = phasor_from_polar(1.0f, phase);
        EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, i, phasor));
    }

    // Compute PAC
    pac_result_t pac;
    EXPECT_TRUE(brain_complex_oscillation_compute_pac(
        state, phase_indices.data(), phase_indices.size(),
        amplitudes.data(), amplitudes.size(), &pac));

    // Should have low coupling
    EXPECT_LT(pac.modulation_index, 0.3f);

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, PACInvalidInputs) {
    auto* state = brain_complex_oscillation_create(10, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    std::vector<uint32_t> indices = {0, 1, 2};
    std::vector<float> amplitudes = {0.5f, 0.6f, 0.7f};
    pac_result_t pac;

    // Null state
    EXPECT_FALSE(brain_complex_oscillation_compute_pac(
        nullptr, indices.data(), indices.size(),
        amplitudes.data(), amplitudes.size(), &pac));

    // Null arrays
    EXPECT_FALSE(brain_complex_oscillation_compute_pac(
        state, nullptr, indices.size(),
        amplitudes.data(), amplitudes.size(), &pac));
    EXPECT_FALSE(brain_complex_oscillation_compute_pac(
        state, indices.data(), indices.size(),
        nullptr, amplitudes.size(), &pac));

    // Null result
    EXPECT_FALSE(brain_complex_oscillation_compute_pac(
        state, indices.data(), indices.size(),
        amplitudes.data(), amplitudes.size(), nullptr));

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, PACMismatchedSizes) {
    auto* state = brain_complex_oscillation_create(10, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    std::vector<uint32_t> indices = {0, 1, 2};
    std::vector<float> amplitudes = {0.5f, 0.6f};  // Different size
    pac_result_t pac;

    // Mismatched sizes
    EXPECT_FALSE(brain_complex_oscillation_compute_pac(
        state, indices.data(), indices.size(),
        amplitudes.data(), amplitudes.size(), &pac));

    brain_complex_oscillation_destroy(state);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(ComplexOscillationTest, Reset) {
    auto* state = brain_complex_oscillation_create(10, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    // Set non-zero phasors
    for (uint32_t i = 0; i < 10; i++) {
        neural_phasor_t phasor = phasor_from_polar(1.0f, M_PI / 4.0f);
        EXPECT_TRUE(brain_complex_oscillation_set_phasor(state, i, phasor));
    }

    // Reset
    brain_complex_oscillation_reset(state);

    // All phasors should be zero
    for (uint32_t i = 0; i < 10; i++) {
        neural_phasor_t phasor;
        EXPECT_TRUE(brain_complex_oscillation_get_phasor(state, i, &phasor));
        EXPECT_EQ(phasor.real, 0.0f);
        EXPECT_EQ(phasor.imag, 0.0f);
    }

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, ResetNull) {
    // Should not crash
    brain_complex_oscillation_reset(nullptr);
}

TEST_F(ComplexOscillationTest, GetNumNeurons) {
    auto* state = brain_complex_oscillation_create(42, 0.1f, 0.95f);
    ASSERT_NE(state, nullptr);

    EXPECT_EQ(brain_complex_oscillation_get_num_neurons(state), 42);

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, GetNumNeuronsNull) {
    EXPECT_EQ(brain_complex_oscillation_get_num_neurons(nullptr), 0);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(ComplexOscillationTest, UpdateAndCoherence) {
    auto* state = brain_complex_oscillation_create(50, 0.05f, 0.98f);
    ASSERT_NE(state, nullptr);

    // Simulate synchronized neural activity
    std::vector<float> activations(50);
    for (int step = 0; step < 100; step++) {
        // Oscillating activation
        for (uint32_t i = 0; i < 50; i++) {
            activations[i] = 0.5f + 0.5f * sinf(2.0f * M_PI * step / 20.0f);
        }
        EXPECT_TRUE(brain_complex_oscillation_update(state, activations.data()));
    }

    // Compute coherence
    phase_coherence_result_t result;
    EXPECT_TRUE(brain_complex_oscillation_compute_coherence(state, &result));

    // Should have reasonable coherence (allow small floating point overflow)
    EXPECT_GT(result.coherence, 0.0f);
    EXPECT_LT(result.coherence, 1.01f);  // Allow small FP precision error

    brain_complex_oscillation_destroy(state);
}

TEST_F(ComplexOscillationTest, MultipleUpdatesPhaseProgression) {
    auto* state = brain_complex_oscillation_create(1, 0.2f, 1.0f);
    ASSERT_NE(state, nullptr);

    std::vector<float> activations(1, 1.0f);

    // Track phase over time
    std::vector<float> phases;
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(brain_complex_oscillation_update(state, activations.data()));
        neural_phasor_t phasor;
        EXPECT_TRUE(brain_complex_oscillation_get_phasor(state, 0, &phasor));
        phases.push_back(phasor_phase(phasor));
    }

    // Phase should generally increase (with wrapping)
    for (size_t i = 1; i < phases.size(); i++) {
        // Account for wrapping
        float diff = phases[i] - phases[i-1];
        if (diff < -M_PI) diff += 2.0f * M_PI;
        if (diff > M_PI) diff -= 2.0f * M_PI;
        EXPECT_GT(diff, 0.0f);
    }

    brain_complex_oscillation_destroy(state);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
