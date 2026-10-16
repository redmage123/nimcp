#include <gtest/gtest.h>

#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "core/brain/nimcp_brain.h"
#include "utils/spectral/nimcp_fft.h"

#include <cstring>

//=============================================================================
// Test Fixture
//=============================================================================

class BrainOscillationsRealTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    brain_oscillation_analyzer_t* analyzer = nullptr;

    void SetUp() override {
        brain = brain_create("test_brain", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (analyzer) {
            brain_oscillation_destroy(analyzer);
            analyzer = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(BrainOscillationsRealTest, CreateDestroy) {
    analyzer = brain_oscillation_create(brain, 500, 250);
    EXPECT_NE(analyzer, nullptr);
}

TEST_F(BrainOscillationsRealTest, CreateWithNullBrain) {
    analyzer = brain_oscillation_create(nullptr, 500, 250);
    EXPECT_EQ(analyzer, nullptr);
}

TEST_F(BrainOscillationsRealTest, DestroyNullAnalyzer) {
    brain_oscillation_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// Activity Recording Tests
//=============================================================================

TEST_F(BrainOscillationsRealTest, RecordActivity) {
    analyzer = brain_oscillation_create(brain, 500, 250);
    ASSERT_NE(analyzer, nullptr);

    bool result = brain_oscillation_record_activity(analyzer);
    EXPECT_TRUE(result);
}

TEST_F(BrainOscillationsRealTest, RecordCustomValue) {
    analyzer = brain_oscillation_create(brain, 500, 250);
    ASSERT_NE(analyzer, nullptr);

    bool result = brain_oscillation_record_value(analyzer, 0.75f);
    EXPECT_TRUE(result);
}

TEST_F(BrainOscillationsRealTest, RecordMultipleValues) {
    analyzer = brain_oscillation_create(brain, 500, 250);
    ASSERT_NE(analyzer, nullptr);

    for (int i = 0; i < 10; i++) {
        bool result = brain_oscillation_record_value(analyzer, 0.5f + i * 0.05f);
        EXPECT_TRUE(result);
    }
}

TEST_F(BrainOscillationsRealTest, RecordActivityWithNullAnalyzer) {
    bool result = brain_oscillation_record_activity(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Wave Power Tests
//=============================================================================

TEST_F(BrainOscillationsRealTest, GetWavePower) {
    analyzer = brain_oscillation_create(brain, 1000, 100);
    ASSERT_NE(analyzer, nullptr);

    // Fill buffer with data
    for (int i = 0; i < 150; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    brain_wave_power_t wave_power;
    bool result = brain_oscillation_get_wave_power(analyzer, &wave_power);
    EXPECT_TRUE(result);
    EXPECT_GE(wave_power.total_power, 0.0f);
}

TEST_F(BrainOscillationsRealTest, GetWavePowerWithNullOutput) {
    analyzer = brain_oscillation_create(brain, 500, 250);
    ASSERT_NE(analyzer, nullptr);

    bool result = brain_oscillation_get_wave_power(analyzer, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Full Analysis Tests
//=============================================================================

TEST_F(BrainOscillationsRealTest, AnalyzeOscillations) {
    analyzer = brain_oscillation_create(brain, 1000, 100);
    ASSERT_NE(analyzer, nullptr);

    // Fill buffer with oscillating data
    for (int i = 0; i < 150; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    oscillation_analysis_t results;
    bool result = brain_oscillation_analyze(analyzer, &results);
    EXPECT_TRUE(result);
}

TEST_F(BrainOscillationsRealTest, AnalyzeWithNullResults) {
    analyzer = brain_oscillation_create(brain, 500, 250);
    ASSERT_NE(analyzer, nullptr);

    bool result = brain_oscillation_analyze(analyzer, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Cognitive State Tests
//=============================================================================

TEST_F(BrainOscillationsRealTest, GetCognitiveState) {
    analyzer = brain_oscillation_create(brain, 1000, 100);
    ASSERT_NE(analyzer, nullptr);

    // Fill buffer
    for (int i = 0; i < 150; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    cognitive_state_t state;
    float confidence;
    bool result = brain_oscillation_get_state(analyzer, &state, &confidence);
    EXPECT_TRUE(result);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(BrainOscillationsRealTest, GetStateWithNullOutputs) {
    analyzer = brain_oscillation_create(brain, 500, 250);
    ASSERT_NE(analyzer, nullptr);

    bool result = brain_oscillation_get_state(analyzer, nullptr, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Cross-Frequency Coupling Tests
//=============================================================================

TEST_F(BrainOscillationsRealTest, ComputePAC) {
    analyzer = brain_oscillation_create(brain, 1000, 100);
    ASSERT_NE(analyzer, nullptr);

    // Fill buffer with data
    for (int i = 0; i < 150; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    float pac = brain_oscillation_compute_pac(analyzer,
                                              BRAIN_WAVE_THETA,
                                              BRAIN_WAVE_GAMMA);
    EXPECT_GE(pac, -1.0f);
    EXPECT_LE(pac, 1.0f);
}

TEST_F(BrainOscillationsRealTest, ComputePACAlphaBeta) {
    analyzer = brain_oscillation_create(brain, 1000, 100);
    ASSERT_NE(analyzer, nullptr);

    for (int i = 0; i < 150; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    float pac = brain_oscillation_compute_pac(analyzer,
                                              BRAIN_WAVE_ALPHA,
                                              BRAIN_WAVE_BETA);
    EXPECT_GE(pac, -1.0f);
    EXPECT_LE(pac, 1.0f);
}

//=============================================================================
// Network Synchrony Tests
//=============================================================================

TEST_F(BrainOscillationsRealTest, ComputeSynchrony) {
    analyzer = brain_oscillation_create(brain, 1000, 100);
    ASSERT_NE(analyzer, nullptr);

    for (int i = 0; i < 150; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    float synchrony = brain_oscillation_compute_synchrony(analyzer);
    EXPECT_GE(synchrony, -1.0f);
    EXPECT_LE(synchrony, 1.0f);
}

//=============================================================================
// Spectrum Export Tests
//=============================================================================

TEST_F(BrainOscillationsRealTest, GetSpectrum) {
    analyzer = brain_oscillation_create(brain, 1000, 100);
    ASSERT_NE(analyzer, nullptr);

    for (int i = 0; i < 150; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    float* spectrum = nullptr;
    uint32_t num_bins = 0;
    bool result = brain_oscillation_get_spectrum(analyzer, &spectrum, &num_bins);
    EXPECT_TRUE(result);
    if (result) {
        EXPECT_NE(spectrum, nullptr);
        EXPECT_GT(num_bins, 0);
    }
}

TEST_F(BrainOscillationsRealTest, GetActivityBuffer) {
    analyzer = brain_oscillation_create(brain, 500, 250);
    ASSERT_NE(analyzer, nullptr);

    for (int i = 0; i < 50; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    const float* buffer = nullptr;
    uint32_t size = 0;
    bool result = brain_oscillation_get_activity_buffer(analyzer, &buffer, &size);
    EXPECT_TRUE(result);
    if (result) {
        EXPECT_NE(buffer, nullptr);
        EXPECT_GT(size, 0);
    }
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(BrainOscillationsRealTest, StateToString) {
    const char* str = brain_oscillation_state_to_string(COGNITIVE_STATE_FOCUSED);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0);
}

TEST_F(BrainOscillationsRealTest, RecommendedWindowDelta) {
    uint32_t window = brain_oscillation_recommended_window(BRAIN_WAVE_DELTA);
    EXPECT_GT(window, 0);
    EXPECT_GE(window, 1000);  // Delta needs at least 1 second
}

TEST_F(BrainOscillationsRealTest, RecommendedWindowGamma) {
    uint32_t window = brain_oscillation_recommended_window(BRAIN_WAVE_GAMMA);
    EXPECT_GT(window, 0);
    EXPECT_LE(window, 500);  // Gamma is fast
}
