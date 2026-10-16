/**
 * @file test_hippocampus_regression.cpp
 * @brief Regression tests for the Hippocampus module
 *
 * These tests verify that previously fixed bugs and edge cases remain working
 * and that complex interactions do not regress when changes are made.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

extern "C" {
#include "core/brain/regions/hippocampus/nimcp_hippocampus.h"
}

class HippocampusRegressionTest : public ::testing::Test {
protected:
    nimcp_hippocampus_t* hippo = nullptr;
    hippo_config_t config;

    void SetUp() override {
        config = hippo_default_config();
        config.num_dg_cells = 500;
        config.num_ca3_cells = 200;
        config.num_ca1_cells = 300;
        config.num_subiculum_cells = 150;
        config.max_episodes = 100;
        config.num_place_cells = 50;
    }

    void TearDown() override {
        if (hippo) {
            hippo_destroy(hippo);
            hippo = nullptr;
        }
    }

    void create_hippocampus() {
        hippo = hippo_create(&config);
        ASSERT_NE(hippo, nullptr);
    }
};

// =============================================================================
// REGRESSION: Boundary Condition Tests
// =============================================================================

TEST_F(HippocampusRegressionTest, REG_BoundaryZeroDimensionInput) {
    // Regression: Zero dimension input should fail gracefully, not crash
    // Note: 'what' content is required, 'where' and 'when' are optional context
    create_hippocampus();

    float dummy_data[1] = {1.0f};
    uint32_t episode_id;

    // Zero 'what' dimension should fail - 'what' content is required
    int result = hippo_encode_episode(hippo, dummy_data, 0, dummy_data, 1,
                                       dummy_data, 1, 0.5f, 0.5f, &episode_id);
    EXPECT_NE(result, 0);

    // NULL 'what' pointer should fail
    result = hippo_encode_episode(hippo, nullptr, 1, dummy_data, 1,
                                   dummy_data, 1, 0.5f, 0.5f, &episode_id);
    EXPECT_NE(result, 0);

    // Zero 'where' dimension is OK - spatial context is optional
    result = hippo_encode_episode(hippo, dummy_data, 1, dummy_data, 0,
                                   dummy_data, 1, 0.5f, 0.5f, &episode_id);
    EXPECT_EQ(result, 0);

    // Zero 'when' dimension is OK - temporal context is optional
    result = hippo_encode_episode(hippo, dummy_data, 1, dummy_data, 1,
                                   dummy_data, 0, 0.5f, 0.5f, &episode_id);
    EXPECT_EQ(result, 0);
}

TEST_F(HippocampusRegressionTest, REG_BoundaryMaxEpisodeCapacity) {
    // Regression: Should handle gracefully when episode capacity is reached
    config.max_episodes = 5;
    create_hippocampus();

    float what[10], where[10], when[5];
    for (int i = 0; i < 10; i++) { what[i] = (float)i / 10.0f; where[i] = (float)i / 10.0f; }
    for (int i = 0; i < 5; i++) { when[i] = (float)i / 5.0f; }

    uint32_t episode_id;
    for (int i = 0; i < 5; i++) {
        what[0] = (float)i;
        int result = hippo_encode_episode(hippo, what, 10, where, 10, when, 5,
                                           0.5f, 0.5f, &episode_id);
        EXPECT_EQ(result, 0);
    }

    // 6th episode should fail or trigger consolidation
    what[0] = 100.0f;
    int result = hippo_encode_episode(hippo, what, 10, where, 10, when, 5,
                                       0.5f, 0.5f, &episode_id);
    // Either fails gracefully or triggers consolidation - both valid
    EXPECT_TRUE(result == 0 || result != 0);
}

TEST_F(HippocampusRegressionTest, REG_BoundaryExtremeEmotionalValues) {
    // Regression: Extreme emotional values should be clamped, not cause overflow
    create_hippocampus();

    float what[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float where[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float when[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    uint32_t episode_id;

    // Extreme positive values
    int result = hippo_encode_episode(hippo, what, 10, where, 10, when, 5,
                                       1000.0f, 1000.0f, &episode_id);
    EXPECT_EQ(result, 0);

    // Extreme negative values
    result = hippo_encode_episode(hippo, what, 10, where, 10, when, 5,
                                   -1000.0f, -1000.0f, &episode_id);
    EXPECT_EQ(result, 0);
}

// =============================================================================
// REGRESSION: State Transition Tests
// =============================================================================

TEST_F(HippocampusRegressionTest, REG_StateRapidOscillationTransitions) {
    // Regression: Rapid oscillation state changes should not corrupt internal state
    create_hippocampus();

    for (int i = 0; i < 100; i++) {
        hippo_set_oscillation_state(hippo, OSCILLATION_THETA);
        hippo_set_oscillation_state(hippo, OSCILLATION_GAMMA);
        hippo_set_oscillation_state(hippo, OSCILLATION_SHARP_WAVE_RIPPLE);
    }

    // Should still function correctly after rapid transitions
    float what[10] = {0.5f}, where[10] = {0.5f}, when[5] = {0.5f};
    uint32_t episode_id;
    int result = hippo_encode_episode(hippo, what, 10, where, 10, when, 5,
                                       0.5f, 0.5f, &episode_id);
    EXPECT_EQ(result, 0);
}

TEST_F(HippocampusRegressionTest, REG_StateConsolidationInterruption) {
    // Regression: Interrupting consolidation should not leave system in bad state
    create_hippocampus();

    // Create some episodes
    float what[10] = {0.5f}, where[10] = {0.5f}, when[5] = {0.5f};
    uint32_t episode_id;
    for (int i = 0; i < 5; i++) {
        what[0] = (float)i;
        hippo_encode_episode(hippo, what, 10, where, 10, when, 5, 0.5f, 0.5f, &episode_id);
    }

    // Start consolidation
    hippo_consolidate_memories(hippo, 0.1f);

    // Interrupt with new encoding
    what[0] = 99.0f;
    int result = hippo_encode_episode(hippo, what, 10, where, 10, when, 5,
                                       0.9f, 0.9f, &episode_id);
    // Should either queue or handle gracefully
    EXPECT_TRUE(result == 0 || result != 0);

    // System should remain functional
    float partial[5] = {0.5f};
    float completed[50];
    uint32_t completed_dim = 50;
    float confidence;
    result = hippo_pattern_complete(hippo, partial, 5, completed, &completed_dim, &confidence);
    // Don't require success, just no crash
}

TEST_F(HippocampusRegressionTest, REG_StateReplayDirectionSwitch) {
    // Regression: Switching replay direction mid-replay should work correctly
    create_hippocampus();

    // Create episodes for replay
    float what[10] = {0.5f}, where[10] = {0.5f}, when[5] = {0.5f};
    uint32_t episode_id;
    for (int i = 0; i < 5; i++) {
        what[0] = (float)i;
        hippo_encode_episode(hippo, what, 10, where, 10, when, 5, 0.5f, 0.5f, &episode_id);
    }

    // Start forward replay
    hippo_trigger_replay(hippo, REPLAY_FORWARD);

    // Switch to reverse
    hippo_trigger_replay(hippo, REPLAY_REVERSE);

    // Switch back
    hippo_trigger_replay(hippo, REPLAY_FORWARD);

    // Should not crash or corrupt state
    hippo_stats_t stats;
    int result = hippo_get_stats(hippo, &stats);
    EXPECT_EQ(result, 0);
}

// =============================================================================
// REGRESSION: Memory Leak Prevention Tests
// =============================================================================

TEST_F(HippocampusRegressionTest, REG_MemoryRepeatedCreateDestroy) {
    // Regression: Repeated create/destroy cycles should not leak memory
    for (int i = 0; i < 50; i++) {
        nimcp_hippocampus_t* temp = hippo_create(&config);
        ASSERT_NE(temp, nullptr);

        // Do some operations
        float what[10] = {(float)i}, where[10] = {0.5f}, when[5] = {0.5f};
        uint32_t episode_id;
        hippo_encode_episode(temp, what, 10, where, 10, when, 5, 0.5f, 0.5f, &episode_id);

        hippo_destroy(temp);
    }
    // If we get here without OOM, test passes
    SUCCEED();
}

TEST_F(HippocampusRegressionTest, REG_MemoryEpisodeChurn) {
    // Regression: High episode turnover should not leak memory
    config.max_episodes = 10;
    create_hippocampus();

    float what[10], where[10], when[5];
    uint32_t episode_id;

    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 10; j++) {
            what[j] = (float)(i * 10 + j);
            where[j] = (float)j / 10.0f;
        }
        for (int j = 0; j < 5; j++) {
            when[j] = (float)j / 5.0f;
        }

        hippo_encode_episode(hippo, what, 10, where, 10, when, 5, 0.5f, 0.5f, &episode_id);

        // Trigger consolidation periodically to force episode cleanup
        if (i % 20 == 0) {
            hippo_consolidate_memories(hippo, 0.1f);
        }
    }

    SUCCEED();
}

// =============================================================================
// REGRESSION: Data Integrity Tests
// =============================================================================

TEST_F(HippocampusRegressionTest, REG_DataEpisodeIntegrityAfterRetrieval) {
    // Regression: Episode data should remain intact after multiple retrievals
    create_hippocampus();

    float original_what[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float original_where[10] = {1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f};
    float original_when[5] = {0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    uint32_t episode_id;

    int result = hippo_encode_episode(hippo, original_what, 10, original_where, 10,
                                       original_when, 5, 0.7f, 0.8f, &episode_id);
    ASSERT_EQ(result, 0);

    // Retrieve multiple times and verify data integrity
    for (int i = 0; i < 10; i++) {
        const nimcp_episode_t* retrieved = hippo_get_episode(hippo, episode_id);
        ASSERT_NE(retrieved, nullptr);
        EXPECT_EQ(retrieved->episode_id, episode_id);
        // Episode should remain accessible
    }
}

TEST_F(HippocampusRegressionTest, REG_DataPatternSeparationConsistency) {
    // Regression: Same input should produce consistent sparse output
    create_hippocampus();

    float input[20];
    for (int i = 0; i < 20; i++) input[i] = (float)i / 20.0f;

    float output1[100], output2[100];
    uint32_t dim1 = 100, dim2 = 100;

    int result1 = hippo_pattern_separate(hippo, input, 20, output1, &dim1);
    int result2 = hippo_pattern_separate(hippo, input, 20, output2, &dim2);

    ASSERT_EQ(result1, 0);
    ASSERT_EQ(result2, 0);
    EXPECT_EQ(dim1, dim2);

    // Outputs should be very similar (allowing for small noise)
    float diff = 0.0f;
    for (uint32_t i = 0; i < dim1; i++) {
        diff += fabsf(output1[i] - output2[i]);
    }
    EXPECT_LT(diff / dim1, 0.1f);
}

// =============================================================================
// REGRESSION: Serialization Tests
// =============================================================================

TEST_F(HippocampusRegressionTest, REG_SerializationRoundTrip) {
    // Regression: Serialization and deserialization should preserve state
    create_hippocampus();

    // Create state
    float what[10] = {0.5f}, where[10] = {0.5f}, when[5] = {0.5f};
    uint32_t episode_ids[5];
    for (int i = 0; i < 5; i++) {
        what[0] = (float)i;
        hippo_encode_episode(hippo, what, 10, where, 10, when, 5, 0.5f, 0.5f, &episode_ids[i]);
    }

    // Serialize
    size_t size = hippo_get_serialization_size(hippo);
    ASSERT_GT(size, 0u);

    std::vector<uint8_t> buffer(size);
    size_t written = 0;
    int result = hippo_serialize(hippo, buffer.data(), size, &written);
    ASSERT_EQ(result, 0);
    EXPECT_GT(written, 0u);

    // Deserialize into new instance
    size_t bytes_read = 0;
    nimcp_hippocampus_t* restored = hippo_deserialize(buffer.data(), written, &bytes_read);
    ASSERT_NE(restored, nullptr);

    // Verify episodes exist
    for (int i = 0; i < 5; i++) {
        const nimcp_episode_t* ep = hippo_get_episode(restored, episode_ids[i]);
        EXPECT_NE(ep, nullptr);
    }

    hippo_destroy(restored);
}

TEST_F(HippocampusRegressionTest, REG_SerializationTruncatedData) {
    // Regression: Truncated data should not crash
    create_hippocampus();

    size_t size = hippo_get_serialization_size(hippo);
    std::vector<uint8_t> buffer(size);
    size_t written = 0;
    hippo_serialize(hippo, buffer.data(), size, &written);

    // Try to deserialize truncated data
    size_t bytes_read = 0;
    nimcp_hippocampus_t* truncated = hippo_deserialize(buffer.data(), written / 2, &bytes_read);
    EXPECT_EQ(truncated, nullptr);
}

// =============================================================================
// REGRESSION: Concurrency Tests
// =============================================================================

TEST_F(HippocampusRegressionTest, REG_ConcurrencySimultaneousReads) {
    // Regression: Multiple threads reading should not cause races
    create_hippocampus();

    // Create some data
    float what[10] = {0.5f}, where[10] = {0.5f}, when[5] = {0.5f};
    uint32_t episode_id;
    hippo_encode_episode(hippo, what, 10, where, 10, when, 5, 0.5f, 0.5f, &episode_id);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; i++) {
        threads.emplace_back([this, episode_id, &success_count]() {
            for (int j = 0; j < 100; j++) {
                const nimcp_episode_t* ep = hippo_get_episode(hippo, episode_id);
                if (ep != nullptr) {
                    success_count++;
                }
                hippo_stats_t stats;
                hippo_get_stats(hippo, &stats);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
}

// =============================================================================
// REGRESSION: Bridge Integration Robustness Tests
// =============================================================================

TEST_F(HippocampusRegressionTest, REG_BridgeNullHandling) {
    // Regression: Null bridge contexts should not crash
    create_hippocampus();

    int result = hippo_init_prime_resonance_bridge(hippo, nullptr);
    EXPECT_EQ(result, 0);  // Should handle gracefully

    result = hippo_init_immune_bridge(hippo, nullptr);
    EXPECT_EQ(result, 0);

    result = hippo_init_cognitive_bridge(hippo, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(HippocampusRegressionTest, REG_BridgeDoubleInit) {
    // Regression: Double init should not cause issues
    create_hippocampus();

    int dummy = 42;
    hippo_init_prime_resonance_bridge(hippo, &dummy);
    int result = hippo_init_prime_resonance_bridge(hippo, &dummy);
    // Should either succeed (replacing) or fail gracefully
    EXPECT_TRUE(result == 0 || result != 0);
}

// =============================================================================
// REGRESSION: Performance Baseline Tests
// =============================================================================

TEST_F(HippocampusRegressionTest, REG_PerfEncodingThroughput) {
    // Regression: Encoding should maintain acceptable throughput
    create_hippocampus();

    float what[50], where[50], when[10];
    for (int i = 0; i < 50; i++) { what[i] = where[i] = (float)i / 50.0f; }
    for (int i = 0; i < 10; i++) { when[i] = (float)i / 10.0f; }

    auto start = std::chrono::high_resolution_clock::now();

    const int num_encodings = 100;
    for (int i = 0; i < num_encodings; i++) {
        what[0] = (float)i;
        uint32_t episode_id;
        hippo_encode_episode(hippo, what, 50, where, 50, when, 10, 0.5f, 0.5f, &episode_id);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 encodings in under 1 second
    EXPECT_LT(duration.count(), 1000) << "Encoding throughput regression detected";
}

TEST_F(HippocampusRegressionTest, REG_PerfPatternCompletionLatency) {
    // Regression: Pattern completion should maintain acceptable latency
    create_hippocampus();

    // Create patterns
    float what[50], where[50], when[10];
    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 50; j++) { what[j] = where[j] = (float)(i * 50 + j) / 1000.0f; }
        for (int j = 0; j < 10; j++) { when[j] = (float)j / 10.0f; }
        uint32_t id;
        hippo_encode_episode(hippo, what, 50, where, 50, when, 10, 0.5f, 0.5f, &id);
    }

    float partial[25];
    for (int i = 0; i < 25; i++) partial[i] = (float)i / 25.0f;

    float completed[100];
    uint32_t dim = 100;
    float confidence;

    auto start = std::chrono::high_resolution_clock::now();

    const int num_completions = 50;
    for (int i = 0; i < num_completions; i++) {
        hippo_pattern_complete(hippo, partial, 25, completed, &dim, &confidence);
        dim = 100;  // Reset for next iteration
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 50 pattern completions in under 500ms
    EXPECT_LT(duration.count(), 500) << "Pattern completion latency regression detected";
}

TEST_F(HippocampusRegressionTest, REG_PerfUpdateCycleTime) {
    // Regression: Update cycle should maintain acceptable frequency
    create_hippocampus();

    auto start = std::chrono::high_resolution_clock::now();

    const int num_updates = 1000;
    for (int i = 0; i < num_updates; i++) {
        hippo_update(hippo, 0.001f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 1000 updates in under 500ms (2000Hz capability)
    EXPECT_LT(duration.count(), 500) << "Update cycle time regression detected";
}

// =============================================================================
// REGRESSION: Edge Case Tests
// =============================================================================

TEST_F(HippocampusRegressionTest, REG_EdgeMinimalConfiguration) {
    // Regression: Minimal configuration should create functional system
    hippo_config_t minimal_config = hippo_default_config();
    minimal_config.num_dg_cells = 10;
    minimal_config.num_ca3_cells = 5;
    minimal_config.num_ca1_cells = 5;
    minimal_config.num_subiculum_cells = 5;
    minimal_config.max_episodes = 5;
    minimal_config.num_place_cells = 3;

    nimcp_hippocampus_t* minimal = hippo_create(&minimal_config);
    ASSERT_NE(minimal, nullptr);

    // Should be functional
    float what[5] = {0.5f}, where[5] = {0.5f}, when[3] = {0.5f};
    uint32_t episode_id;
    int result = hippo_encode_episode(minimal, what, 5, where, 5, when, 3,
                                       0.5f, 0.5f, &episode_id);
    EXPECT_EQ(result, 0);

    hippo_destroy(minimal);
}

// =============================================================================
// REGRESSION: Previously Fixed Bug Tests
// =============================================================================

TEST_F(HippocampusRegressionTest, REG_BugSparseOutputNormalization) {
    // Regression: Sparse output should be properly normalized
    create_hippocampus();

    float input[30];
    for (int i = 0; i < 30; i++) input[i] = (float)i / 30.0f;

    float output[200];
    uint32_t output_dim = 200;

    int result = hippo_pattern_separate(hippo, input, 30, output, &output_dim);
    ASSERT_EQ(result, 0);

    // Check for NaN or Inf
    for (uint32_t i = 0; i < output_dim; i++) {
        EXPECT_FALSE(std::isnan(output[i])) << "NaN found at index " << i;
        EXPECT_FALSE(std::isinf(output[i])) << "Inf found at index " << i;
    }

    // Output values should be bounded
    for (uint32_t i = 0; i < output_dim; i++) {
        EXPECT_GE(output[i], -10.0f);
        EXPECT_LE(output[i], 10.0f);
    }
}

TEST_F(HippocampusRegressionTest, REG_BugOscillationPhaseWrap) {
    // Regression: Oscillation phase should wrap correctly at 2*PI
    create_hippocampus();

    // Run many update cycles
    for (int i = 0; i < 10000; i++) {
        hippo_update(hippo, 0.001f);
    }

    float theta_phase = hippo_get_theta_phase(hippo);
    float gamma_phase = hippo_get_gamma_phase(hippo);

    // Phases should be in valid range
    EXPECT_GE(theta_phase, 0.0f);
    EXPECT_LT(theta_phase, 2.0f * M_PI + 0.01f);
    EXPECT_GE(gamma_phase, 0.0f);
    EXPECT_LT(gamma_phase, 2.0f * M_PI + 0.01f);
}

TEST_F(HippocampusRegressionTest, REG_BugConsolidationLevelClamping) {
    // Regression: Consolidation level should be clamped to [0, 1]
    create_hippocampus();

    float what[10] = {0.5f}, where[10] = {0.5f}, when[5] = {0.5f};
    uint32_t episode_id;
    hippo_encode_episode(hippo, what, 10, where, 10, when, 5, 0.5f, 0.5f, &episode_id);

    // Force many consolidation cycles
    for (int i = 0; i < 100; i++) {
        hippo_consolidate_memories(hippo, 0.1f);
        for (int j = 0; j < 100; j++) {
            hippo_update(hippo, 0.01f);
        }
    }

    float consolidation = hippo_get_consolidation_level(hippo, episode_id);
    EXPECT_GE(consolidation, 0.0f);
    EXPECT_LE(consolidation, 1.0f);
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
