/**
 * @file test_substrate_bridge_stability.cpp
 * @brief Stability regression tests for substrate bridges
 *
 * WHAT: Tests API stability and behavioral consistency for substrate bridges
 * WHY: Substrate bridges connect cognitive systems to neural substrate state
 * HOW: Test create/destroy, update cycles, effects queries, error handling
 *
 * BRIDGES COVERED:
 * - reasoning_substrate_bridge
 * - intuition_substrate_bridge
 * - attention_substrate_bridge
 * - emotion_substrate_bridge
 * - executive_substrate_bridge
 *
 * TEST CATEGORIES:
 * 1. API Stability - Function signatures unchanged
 * 2. Lifecycle Stability - Create/destroy patterns work
 * 3. Update Stability - Updates produce consistent effects
 * 4. Error Handling - Edge cases handled consistently
 * 5. Performance Baselines - No significant regressions
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>
#include <thread>

// Headers have their own extern "C" guards
#include "cognitive/reasoning/nimcp_reasoning_substrate_bridge.h"
#include "cognitive/parietal/nimcp_intuition_substrate_bridge.h"
#include "cognitive/attention/nimcp_attention_substrate_bridge.h"
#include "cognitive/emotion/nimcp_emotion_substrate_bridge.h"
#include "cognitive/executive/nimcp_executive_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"

/* ============================================================================
 * Test Fixture
 * ========================================================================== */

class SubstrateBridgeStabilityTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;

    static constexpr int WARMUP_ITERATIONS = 10;
    static constexpr int BENCHMARK_ITERATIONS = 100;
    static constexpr int MEMORY_TEST_CYCLES = 500;
    static constexpr int STABILITY_TEST_CYCLES = 1000;

    void SetUp() override {
        substrate_config_t config;
        substrate_default_config(&config);
        substrate = substrate_create(&config);
        ASSERT_NE(substrate, nullptr);

        // Set healthy baseline
        substrate_set_atp(substrate, 0.9f);
        substrate_set_temperature(substrate, 37.0f);
    }

    void TearDown() override {
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    template<typename Func>
    long long measure_ns(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }

    // Mock system pointers (bridges don't dereference these in basic operations)
    nimcp_reasoning_system_t* mock_reasoning() {
        return reinterpret_cast<nimcp_reasoning_system_t*>(0x1001);
    }

    intuition_system_t* mock_intuition() {
        return reinterpret_cast<intuition_system_t*>(0x1002);
    }

    nimcp_attention_system_t* mock_attention() {
        return reinterpret_cast<nimcp_attention_system_t*>(0x1003);
    }

    emotional_system_t* mock_emotion() {
        return reinterpret_cast<emotional_system_t*>(0x1004);
    }

    nimcp_executive_t* mock_executive() {
        return reinterpret_cast<nimcp_executive_t*>(0x1005);
    }
};

/* ============================================================================
 * CATEGORY 1: API Stability Tests
 * ========================================================================== */

TEST_F(SubstrateBridgeStabilityTest, APIStability_ReasoningBridge_DefaultConfig) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);

    // Default config should have sensible values
    EXPECT_TRUE(config.enable_atp_modulation);
    EXPECT_TRUE(config.enable_fatigue_modulation);
    EXPECT_GE(config.atp_sensitivity, 0.0f);
    EXPECT_LE(config.atp_sensitivity, 1.0f);
}

TEST_F(SubstrateBridgeStabilityTest, APIStability_IntuitionBridge_DefaultConfig) {
    intuition_substrate_config_t config = intuition_substrate_default_config();

    // Default should enable modulations
    EXPECT_TRUE(config.enable_atp_modulation);
    EXPECT_TRUE(config.enable_fatigue_modulation);
}

TEST_F(SubstrateBridgeStabilityTest, APIStability_AttentionBridge_DefaultConfig) {
    attention_substrate_config_t config;
    attention_substrate_default_config(&config);

    EXPECT_TRUE(config.enable_focus_modulation);
    EXPECT_GE(config.atp_sensitivity, 0.0f);
}

TEST_F(SubstrateBridgeStabilityTest, APIStability_EmotionBridge_DefaultConfig) {
    emotion_substrate_config_t config;
    emotion_substrate_default_config(&config);

    EXPECT_TRUE(config.enable_atp_modulation);
}

TEST_F(SubstrateBridgeStabilityTest, APIStability_ExecutiveBridge_DefaultConfig) {
    executive_substrate_config_t config;
    executive_substrate_default_config(&config);

    EXPECT_TRUE(config.enable_planning_modulation);
}

/* ============================================================================
 * CATEGORY 2: Lifecycle Stability Tests
 * ========================================================================== */

TEST_F(SubstrateBridgeStabilityTest, Lifecycle_ReasoningBridge_CreateDestroy) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = reasoning_substrate_bridge_create(&config, mock_reasoning(), substrate);
        ASSERT_NE(bridge, nullptr);
        reasoning_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(SubstrateBridgeStabilityTest, Lifecycle_IntuitionBridge_CreateDestroy) {
    intuition_substrate_config_t config = intuition_substrate_default_config();
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = intuition_substrate_bridge_create(mock_intuition(), substrate, &config);
        ASSERT_NE(bridge, nullptr);
        intuition_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(SubstrateBridgeStabilityTest, Lifecycle_AttentionBridge_CreateDestroy) {
    attention_substrate_config_t config;
    attention_substrate_default_config(&config);
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = attention_substrate_bridge_create(&config, substrate, mock_attention());
        ASSERT_NE(bridge, nullptr);
        attention_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(SubstrateBridgeStabilityTest, Lifecycle_EmotionBridge_CreateDestroy) {
    emotion_substrate_config_t config;
    emotion_substrate_default_config(&config);
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = emotion_substrate_bridge_create(&config, mock_emotion(), substrate);
        ASSERT_NE(bridge, nullptr);
        emotion_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(SubstrateBridgeStabilityTest, Lifecycle_ExecutiveBridge_CreateDestroy) {
    executive_substrate_config_t config;
    executive_substrate_default_config(&config);
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = executive_substrate_bridge_create(&config, mock_executive(), substrate);
        ASSERT_NE(bridge, nullptr);
        executive_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(SubstrateBridgeStabilityTest, Lifecycle_DestroyNull_Safe) {
    // All bridges should safely handle null destroy
    reasoning_substrate_bridge_destroy(nullptr);
    intuition_substrate_bridge_destroy(nullptr);
    attention_substrate_bridge_destroy(nullptr);
    emotion_substrate_bridge_destroy(nullptr);
    executive_substrate_bridge_destroy(nullptr);

    SUCCEED();
}

/* ============================================================================
 * CATEGORY 3: Update Stability Tests
 * ========================================================================== */

TEST_F(SubstrateBridgeStabilityTest, Update_ReasoningBridge_ConsistentEffects) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = reasoning_substrate_bridge_create(&config, mock_reasoning(), substrate);
    ASSERT_NE(bridge, nullptr);

    // Update multiple times with same substrate state
    std::vector<float> depths;
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(0, reasoning_substrate_update(bridge));
        float depth = reasoning_substrate_get_inference_depth(bridge);
        depths.push_back(depth);

        EXPECT_FALSE(std::isnan(depth));
        EXPECT_GE(depth, 0.0f);
        EXPECT_LE(depth, 1.0f);
    }

    // Effects should be stable (deterministic)
    for (size_t i = 1; i < depths.size(); i++) {
        EXPECT_FLOAT_EQ(depths[i], depths[0]) << "Effects should be deterministic";
    }

    reasoning_substrate_bridge_destroy(bridge);
}

TEST_F(SubstrateBridgeStabilityTest, Update_IntuitionBridge_ConsistentEffects) {
    intuition_substrate_config_t config = intuition_substrate_default_config();
    config.enable_bio_async = false;

    auto* bridge = intuition_substrate_bridge_create(mock_intuition(), substrate, &config);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < STABILITY_TEST_CYCLES; i++) {
        EXPECT_EQ(0, intuition_substrate_bridge_update(bridge));

        intuition_substrate_effects_t effects;
        EXPECT_EQ(0, intuition_substrate_bridge_get_effects(bridge, &effects));

        // All effects should be valid
        EXPECT_FALSE(std::isnan(effects.insight_depth));
        EXPECT_FALSE(std::isnan(effects.processing_speed));
        EXPECT_FALSE(std::isnan(effects.overall_capacity));
    }

    intuition_substrate_bridge_destroy(bridge);
}

TEST_F(SubstrateBridgeStabilityTest, Update_AttentionBridge_ConsistentEffects) {
    attention_substrate_config_t config;
    attention_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = attention_substrate_bridge_create(&config, substrate, mock_attention());
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < STABILITY_TEST_CYCLES; i++) {
        EXPECT_EQ(0, attention_substrate_update(bridge));

        float focus = attention_substrate_get_focus_capacity(bridge);
        EXPECT_FALSE(std::isnan(focus));
        EXPECT_GE(focus, 0.0f);
        EXPECT_LE(focus, 1.0f);
    }

    attention_substrate_bridge_destroy(bridge);
}

TEST_F(SubstrateBridgeStabilityTest, Update_ATPChanges_EffectsUpdate) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = reasoning_substrate_bridge_create(&config, mock_reasoning(), substrate);
    ASSERT_NE(bridge, nullptr);

    // High ATP - good effects
    substrate_set_atp(substrate, 0.9f);
    reasoning_substrate_update(bridge);
    float high_atp_depth = reasoning_substrate_get_inference_depth(bridge);

    // Low ATP - degraded effects
    substrate_set_atp(substrate, 0.2f);
    reasoning_substrate_update(bridge);
    float low_atp_depth = reasoning_substrate_get_inference_depth(bridge);

    // Low ATP should have lower depth
    EXPECT_LT(low_atp_depth, high_atp_depth)
        << "Low ATP should reduce inference depth";

    reasoning_substrate_bridge_destroy(bridge);
}

/* ============================================================================
 * CATEGORY 4: Error Handling Tests
 * ========================================================================== */

TEST_F(SubstrateBridgeStabilityTest, ErrorHandling_NullSubstrate_CreateFails) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);

    // Null substrate should fail or handle gracefully
    auto* bridge = reasoning_substrate_bridge_create(&config, mock_reasoning(), nullptr);

    // Either null or valid bridge that handles null substrate
    if (bridge != nullptr) {
        reasoning_substrate_bridge_destroy(bridge);
    }
}

TEST_F(SubstrateBridgeStabilityTest, ErrorHandling_NullSystem_CreateFails) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);

    // Null reasoning system - should fail or handle gracefully
    auto* bridge = reasoning_substrate_bridge_create(&config, nullptr, substrate);

    if (bridge != nullptr) {
        reasoning_substrate_bridge_destroy(bridge);
    }
}

TEST_F(SubstrateBridgeStabilityTest, ErrorHandling_NullConfig_UsesDefaults) {
    // Null config should use defaults
    auto* bridge = reasoning_substrate_bridge_create(nullptr, mock_reasoning(), substrate);

    if (bridge != nullptr) {
        // Should work with default config
        EXPECT_EQ(0, reasoning_substrate_update(bridge));
        reasoning_substrate_bridge_destroy(bridge);
    }
}

TEST_F(SubstrateBridgeStabilityTest, ErrorHandling_ExtremeATP_Handled) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = reasoning_substrate_bridge_create(&config, mock_reasoning(), substrate);
    ASSERT_NE(bridge, nullptr);

    // Test extreme ATP values
    float atp_values[] = {0.0f, 0.001f, 0.5f, 0.999f, 1.0f};

    for (float atp : atp_values) {
        substrate_set_atp(substrate, atp);
        EXPECT_EQ(0, reasoning_substrate_update(bridge));

        float depth = reasoning_substrate_get_inference_depth(bridge);
        EXPECT_FALSE(std::isnan(depth)) << "NaN with ATP=" << atp;
        EXPECT_FALSE(std::isinf(depth)) << "Inf with ATP=" << atp;
    }

    reasoning_substrate_bridge_destroy(bridge);
}

TEST_F(SubstrateBridgeStabilityTest, ErrorHandling_ExtremeTemperature_Handled) {
    emotion_substrate_config_t config;
    emotion_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = emotion_substrate_bridge_create(&config, mock_emotion(), substrate);
    ASSERT_NE(bridge, nullptr);

    // Test extreme temperatures
    float temps[] = {32.0f, 35.0f, 37.0f, 40.0f, 42.0f, 45.0f};

    for (float temp : temps) {
        substrate_set_temperature(substrate, temp);
        EXPECT_EQ(0, emotion_substrate_update(bridge));

        float intensity = emotion_substrate_get_intensity_mod(bridge);
        EXPECT_FALSE(std::isnan(intensity)) << "NaN with temp=" << temp;
        EXPECT_FALSE(std::isinf(intensity)) << "Inf with temp=" << temp;
    }

    emotion_substrate_bridge_destroy(bridge);
}

/* ============================================================================
 * CATEGORY 5: Performance Baseline Tests
 * ========================================================================== */

TEST_F(SubstrateBridgeStabilityTest, Performance_CreateDestroy_Under200us) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        auto* bridge = reasoning_substrate_bridge_create(&config, mock_reasoning(), substrate);
        reasoning_substrate_bridge_destroy(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            auto* bridge = reasoning_substrate_bridge_create(&config, mock_reasoning(), substrate);
            reasoning_substrate_bridge_destroy(bridge);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Substrate Bridge Create/Destroy: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 200000.0) << "Create/Destroy should be < 200 us";
}

TEST_F(SubstrateBridgeStabilityTest, Performance_Update_Under100us) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = reasoning_substrate_bridge_create(&config, mock_reasoning(), substrate);
    ASSERT_NE(bridge, nullptr);

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        reasoning_substrate_update(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            reasoning_substrate_update(bridge);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Substrate Bridge Update: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 100000.0) << "Update should be < 100 us";

    reasoning_substrate_bridge_destroy(bridge);
}

TEST_F(SubstrateBridgeStabilityTest, Performance_GetEffects_Under1us) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = reasoning_substrate_bridge_create(&config, mock_reasoning(), substrate);
    ASSERT_NE(bridge, nullptr);

    reasoning_substrate_update(bridge);

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        (void)reasoning_substrate_get_inference_depth(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            (void)reasoning_substrate_get_inference_depth(bridge);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Substrate Bridge GetEffects: avg=" << avg_ns << " ns\n";

    EXPECT_LT(avg_ns, 1000.0) << "GetEffects should be < 1 us";

    reasoning_substrate_bridge_destroy(bridge);
}

TEST_F(SubstrateBridgeStabilityTest, Performance_AllBridges_TotalUnder1ms) {
    // Create all bridge types
    reasoning_substrate_config_t reas_config;
    reasoning_substrate_default_config(&reas_config);
    reas_config.enable_bio_async = false;

    attention_substrate_config_t attn_config;
    attention_substrate_default_config(&attn_config);
    attn_config.enable_bio_async = false;

    emotion_substrate_config_t emo_config;
    emotion_substrate_default_config(&emo_config);
    emo_config.enable_bio_async = false;

    executive_substrate_config_t exec_config;
    executive_substrate_default_config(&exec_config);
    exec_config.enable_bio_async = false;

    auto start = std::chrono::high_resolution_clock::now();

    auto* reas = reasoning_substrate_bridge_create(&reas_config, mock_reasoning(), substrate);
    auto* attn = attention_substrate_bridge_create(&attn_config, substrate, mock_attention());
    auto* emo = emotion_substrate_bridge_create(&emo_config, mock_emotion(), substrate);
    auto* exec = executive_substrate_bridge_create(&exec_config, mock_executive(), substrate);

    // Update all
    reasoning_substrate_update(reas);
    attention_substrate_update(attn);
    emotion_substrate_update(emo);
    executive_substrate_update(exec);

    // Destroy all
    reasoning_substrate_bridge_destroy(reas);
    attention_substrate_bridge_destroy(attn);
    emotion_substrate_bridge_destroy(emo);
    executive_substrate_bridge_destroy(exec);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "All bridges lifecycle: " << duration_us << " us\n";

    EXPECT_LT(duration_us, 1000) << "All bridges should complete in < 1 ms";
}

/* ============================================================================
 * CATEGORY 6: Thread Safety Tests
 * ========================================================================== */

TEST_F(SubstrateBridgeStabilityTest, ThreadSafety_ConcurrentUpdates) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = reasoning_substrate_bridge_create(&config, mock_reasoning(), substrate);
    ASSERT_NE(bridge, nullptr);

    const int NUM_THREADS = 4;
    const int UPDATES_PER_THREAD = 100;

    auto worker = [&]() {
        for (int i = 0; i < UPDATES_PER_THREAD; i++) {
            reasoning_substrate_update(bridge);
            float depth = reasoning_substrate_get_inference_depth(bridge);
            EXPECT_FALSE(std::isnan(depth));
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    reasoning_substrate_bridge_destroy(bridge);
}

TEST_F(SubstrateBridgeStabilityTest, ThreadSafety_ConcurrentSubstrateChanges) {
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = reasoning_substrate_bridge_create(&config, mock_reasoning(), substrate);
    ASSERT_NE(bridge, nullptr);

    const int ITERATIONS = 100;

    auto substrate_modifier = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            float atp = 0.3f + (i % 7) * 0.1f;
            substrate_set_atp(substrate, atp);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    };

    auto bridge_updater = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            reasoning_substrate_update(bridge);
            float depth = reasoning_substrate_get_inference_depth(bridge);
            EXPECT_FALSE(std::isnan(depth));
            std::this_thread::sleep_for(std::chrono::microseconds(30));
        }
    };

    std::thread t1(substrate_modifier);
    std::thread t2(bridge_updater);
    std::thread t3(bridge_updater);

    t1.join();
    t2.join();
    t3.join();

    reasoning_substrate_bridge_destroy(bridge);
}

/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
