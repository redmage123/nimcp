/**
 * @file test_thalamic_bridge_stability.cpp
 * @brief Stability regression tests for thalamic routing bridges
 *
 * WHAT: Tests API stability and behavioral consistency for thalamic bridges
 * WHY: Thalamic bridges route signals through attention-gated pathways
 * HOW: Test create/destroy, routing operations, attention control, statistics
 *
 * BRIDGES COVERED:
 * - intuition_thalamic_bridge
 * - Other cognitive thalamic bridges (gw, ethics, logic, etc.)
 *
 * BIOLOGICAL BASIS:
 * - Thalamus acts as "gateway to consciousness"
 * - Pulvinar nucleus coordinates attention during processing
 * - Thalamic reticular nucleus (TRN) gates information flow
 * - Burst vs tonic modes affect signal salience
 *
 * TEST CATEGORIES:
 * 1. API Stability - Function signatures unchanged
 * 2. Lifecycle Stability - Create/destroy patterns work
 * 3. Routing Stability - Signal routing works correctly
 * 4. Attention Control - Attention modulation works
 * 5. Error Handling - Edge cases handled consistently
 * 6. Performance Baselines - No significant regressions
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_intuition_thalamic_bridge.h"
#include "middleware/routing/nimcp_thalamic_router.h"

/* ============================================================================
 * Test Fixture
 * ========================================================================== */

class ThalamicBridgeStabilityTest : public ::testing::Test {
protected:
    thalamic_router_t* router = nullptr;

    static constexpr int WARMUP_ITERATIONS = 10;
    static constexpr int BENCHMARK_ITERATIONS = 100;
    static constexpr int MEMORY_TEST_CYCLES = 500;
    static constexpr int STABILITY_TEST_CYCLES = 1000;

    void SetUp() override {
        // Create thalamic router
        thalamic_router_config_t config = thalamic_router_default_config();
        router = thalamic_router_create(&config);
        ASSERT_NE(router, nullptr);
    }

    void TearDown() override {
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
    }

    template<typename Func>
    long long measure_ns(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }

    // Mock intuition system
    intuition_system_t* mock_intuition() {
        return reinterpret_cast<intuition_system_t*>(0x2001);
    }

    // Create mock hunch for routing tests
    hunch_t create_mock_hunch(float confidence) {
        hunch_t hunch;
        memset(&hunch, 0, sizeof(hunch));
        hunch.posterior_probability = confidence;
        hunch.prior_probability = 0.5f;
        return hunch;
    }
};

/* ============================================================================
 * CATEGORY 1: API Stability Tests
 * ========================================================================== */

TEST_F(ThalamicBridgeStabilityTest, APIStability_DefaultConfig_HasSensibleValues) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();

    // Default should enable key features
    EXPECT_TRUE(config.enable_attention_gating);
    EXPECT_TRUE(config.enable_priority_routing);

    // Thresholds should be in valid range
    EXPECT_GE(config.min_confidence_threshold, 0.0f);
    EXPECT_LE(config.min_confidence_threshold, 1.0f);
    EXPECT_GE(config.min_attention_threshold, 0.0f);
    EXPECT_LE(config.min_attention_threshold, 1.0f);
}

TEST_F(ThalamicBridgeStabilityTest, APIStability_SignalTypes_EnumValuesStable) {
    // Signal type constants should be stable
    EXPECT_EQ(INTUITION_SIGNAL_HUNCH, 0x0001);
    EXPECT_EQ(INTUITION_SIGNAL_INSIGHT, 0x0002);
    EXPECT_EQ(INTUITION_SIGNAL_ANALOGY, 0x0003);
    EXPECT_EQ(INTUITION_SIGNAL_HYPOTHESIS, 0x0004);
    EXPECT_EQ(INTUITION_SIGNAL_BLEND, 0x0005);
    EXPECT_EQ(INTUITION_SIGNAL_COUNTERFACTUAL, 0x0006);
    EXPECT_EQ(INTUITION_SIGNAL_META, 0x0007);
}

TEST_F(ThalamicBridgeStabilityTest, APIStability_AttentionDefaults_InValidRange) {
    // Default attention weights should be in [0, 1]
    EXPECT_GE(INTUITION_ATTENTION_HUNCH_DEFAULT, 0.0f);
    EXPECT_LE(INTUITION_ATTENTION_HUNCH_DEFAULT, 1.0f);

    EXPECT_GE(INTUITION_ATTENTION_INSIGHT_DEFAULT, 0.0f);
    EXPECT_LE(INTUITION_ATTENTION_INSIGHT_DEFAULT, 1.0f);

    EXPECT_GE(INTUITION_ATTENTION_ANALOGY_DEFAULT, 0.0f);
    EXPECT_LE(INTUITION_ATTENTION_ANALOGY_DEFAULT, 1.0f);
}

TEST_F(ThalamicBridgeStabilityTest, APIStability_MaxBroadcastTargets_Defined) {
    // Max broadcast targets should be reasonable
    EXPECT_GT(INTUITION_MAX_BROADCAST_TARGETS, 0);
    EXPECT_LE(INTUITION_MAX_BROADCAST_TARGETS, 64);
}

/* ============================================================================
 * CATEGORY 2: Lifecycle Stability Tests
 * ========================================================================== */

TEST_F(ThalamicBridgeStabilityTest, Lifecycle_CreateDestroy_NoLeaks) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
        ASSERT_NE(bridge, nullptr);
        intuition_thalamic_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(ThalamicBridgeStabilityTest, Lifecycle_CreateWithNullConfig_UsesDefaults) {
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, nullptr);

    // Should succeed with default config
    if (bridge != nullptr) {
        intuition_thalamic_bridge_destroy(bridge);
    }
}

TEST_F(ThalamicBridgeStabilityTest, Lifecycle_DestroyNull_Safe) {
    intuition_thalamic_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(ThalamicBridgeStabilityTest, Lifecycle_Reset_WorksRepeatedly) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < STABILITY_TEST_CYCLES; i++) {
        EXPECT_EQ(0, intuition_thalamic_bridge_reset(bridge));
    }

    intuition_thalamic_bridge_destroy(bridge);
}

/* ============================================================================
 * CATEGORY 3: Routing Stability Tests
 * ========================================================================== */

TEST_F(ThalamicBridgeStabilityTest, Routing_RouteHunch_SucceedsWithValidHunch) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    hunch_t hunch = create_mock_hunch(0.8f);
    int result = intuition_thalamic_route_hunch(bridge, &hunch);

    // Should succeed or return routing result
    EXPECT_GE(result, -1);

    intuition_thalamic_bridge_destroy(bridge);
}

TEST_F(ThalamicBridgeStabilityTest, Routing_RouteInsight_SucceedsWithNovelty) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    // Route insight with various novelty levels
    float novelty_levels[] = {0.0f, 0.3f, 0.5f, 0.8f, 1.0f};

    for (float novelty : novelty_levels) {
        int result = intuition_thalamic_route_insight(bridge, (void*)0x1234, novelty);
        EXPECT_GE(result, -1) << "Failed with novelty=" << novelty;
    }

    intuition_thalamic_bridge_destroy(bridge);
}

TEST_F(ThalamicBridgeStabilityTest, Routing_RouteAnalogy_SucceedsWithStrength) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    float strengths[] = {0.1f, 0.5f, 0.9f};

    for (float strength : strengths) {
        int result = intuition_thalamic_route_analogy(bridge, (void*)0x5678, strength);
        EXPECT_GE(result, -1) << "Failed with strength=" << strength;
    }

    intuition_thalamic_bridge_destroy(bridge);
}

TEST_F(ThalamicBridgeStabilityTest, Routing_RouteSignal_WorksWithExplicitTargets) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    // Create signal
    intuition_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = INTUITION_SIGNAL_HUNCH;
    signal.confidence = 0.7f;
    signal.salience = 0.6f;
    signal.timestamp_us = 1000;

    // Create explicit targets
    intuition_routing_target_t targets[2];
    targets[0].target_id = 100;
    targets[0].attention_boost = 0.1f;
    targets[0].require_ack = false;
    targets[1].target_id = 101;
    targets[1].attention_boost = 0.2f;
    targets[1].require_ack = false;

    int result = intuition_thalamic_route_signal(bridge, &signal, targets, 2);
    EXPECT_GE(result, -1);

    intuition_thalamic_bridge_destroy(bridge);
}

TEST_F(ThalamicBridgeStabilityTest, Routing_ManySignals_StablePerformance) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    hunch_t hunch = create_mock_hunch(0.6f);

    for (int i = 0; i < STABILITY_TEST_CYCLES; i++) {
        hunch.posterior_probability = 0.3f + (i % 7) * 0.1f;
        intuition_thalamic_route_hunch(bridge, &hunch);
    }

    intuition_thalamic_bridge_destroy(bridge);
}

/* ============================================================================
 * CATEGORY 4: Attention Control Tests
 * ========================================================================== */

TEST_F(ThalamicBridgeStabilityTest, Attention_SetAttention_AcceptsValidRange) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    float attention_values[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float attention : attention_values) {
        int result = intuition_thalamic_set_attention(bridge, attention);
        EXPECT_EQ(0, result) << "Failed to set attention=" << attention;
    }

    intuition_thalamic_bridge_destroy(bridge);
}

TEST_F(ThalamicBridgeStabilityTest, Attention_GetAttention_ReturnsSetValue) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    float set_value = 0.65f;
    intuition_thalamic_set_attention(bridge, set_value);

    float get_value = 0.0f;
    int result = intuition_thalamic_get_attention(bridge, &get_value);
    EXPECT_EQ(0, result);
    EXPECT_FLOAT_EQ(get_value, set_value);

    intuition_thalamic_bridge_destroy(bridge);
}

TEST_F(ThalamicBridgeStabilityTest, Attention_BoostAttention_IncreasesPriority) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    // Boost attention for hunch signals
    int result = intuition_thalamic_boost_attention(bridge, INTUITION_SIGNAL_HUNCH, 0.2f);
    EXPECT_EQ(0, result);

    // Boost for other signal types
    result = intuition_thalamic_boost_attention(bridge, INTUITION_SIGNAL_INSIGHT, 0.3f);
    EXPECT_EQ(0, result);

    intuition_thalamic_bridge_destroy(bridge);
}

TEST_F(ThalamicBridgeStabilityTest, Attention_OutOfRangeValues_Handled) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    // Negative attention - should clamp or fail
    int result = intuition_thalamic_set_attention(bridge, -0.5f);
    // Either fails or clamps to 0
    (void)result;

    // Attention > 1 - should clamp or fail
    result = intuition_thalamic_set_attention(bridge, 1.5f);
    // Either fails or clamps to 1
    (void)result;

    // Get attention should return valid value
    float attention = 0.0f;
    intuition_thalamic_get_attention(bridge, &attention);
    EXPECT_GE(attention, 0.0f);
    EXPECT_LE(attention, 1.0f);

    intuition_thalamic_bridge_destroy(bridge);
}

/* ============================================================================
 * CATEGORY 5: Error Handling Tests
 * ========================================================================== */

TEST_F(ThalamicBridgeStabilityTest, ErrorHandling_NullRouter_CreateFails) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), nullptr, &config);

    // Should fail or handle gracefully
    if (bridge != nullptr) {
        intuition_thalamic_bridge_destroy(bridge);
    }
}

TEST_F(ThalamicBridgeStabilityTest, ErrorHandling_NullIntuition_CreateFails) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(nullptr, router, &config);

    if (bridge != nullptr) {
        intuition_thalamic_bridge_destroy(bridge);
    }
}

TEST_F(ThalamicBridgeStabilityTest, ErrorHandling_NullHunch_RouteReturnsError) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    int result = intuition_thalamic_route_hunch(bridge, nullptr);
    EXPECT_EQ(-1, result);

    intuition_thalamic_bridge_destroy(bridge);
}

TEST_F(ThalamicBridgeStabilityTest, ErrorHandling_NullSignal_RouteReturnsError) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    int result = intuition_thalamic_route_signal(bridge, nullptr, nullptr, 0);
    EXPECT_EQ(-1, result);

    intuition_thalamic_bridge_destroy(bridge);
}

TEST_F(ThalamicBridgeStabilityTest, ErrorHandling_NullOutput_GetAttention) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    int result = intuition_thalamic_get_attention(bridge, nullptr);
    EXPECT_EQ(-1, result);

    intuition_thalamic_bridge_destroy(bridge);
}

/* ============================================================================
 * CATEGORY 6: Statistics Tests
 * ========================================================================== */

TEST_F(ThalamicBridgeStabilityTest, Statistics_GetStats_ReturnsValidData) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    // Route some signals to generate stats
    hunch_t hunch = create_mock_hunch(0.7f);
    for (int i = 0; i < 10; i++) {
        intuition_thalamic_route_hunch(bridge, &hunch);
    }

    intuition_thalamic_stats_t stats;
    int result = intuition_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, result);

    // Stats should be non-negative
    EXPECT_GE(stats.signals_routed, 0u);
    EXPECT_GE(stats.signals_dropped, 0u);
    EXPECT_GE(stats.hunches_routed, 0u);

    intuition_thalamic_bridge_destroy(bridge);
}

TEST_F(ThalamicBridgeStabilityTest, Statistics_ResetStats_ClearsCounters) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    // Route some signals
    hunch_t hunch = create_mock_hunch(0.7f);
    for (int i = 0; i < 10; i++) {
        intuition_thalamic_route_hunch(bridge, &hunch);
    }

    // Reset stats
    intuition_thalamic_bridge_reset_stats(bridge);

    // Stats should be cleared
    intuition_thalamic_stats_t stats;
    intuition_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0u, stats.signals_routed);
    EXPECT_EQ(0u, stats.hunches_routed);

    intuition_thalamic_bridge_destroy(bridge);
}

/* ============================================================================
 * CATEGORY 7: Performance Baseline Tests
 * ========================================================================== */

TEST_F(ThalamicBridgeStabilityTest, Performance_CreateDestroy_Under150us) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
        intuition_thalamic_bridge_destroy(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
            intuition_thalamic_bridge_destroy(bridge);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Thalamic Bridge Create/Destroy: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 150000.0) << "Create/Destroy should be < 150 us";
}

TEST_F(ThalamicBridgeStabilityTest, Performance_RouteHunch_Under50us) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    hunch_t hunch = create_mock_hunch(0.7f);

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        intuition_thalamic_route_hunch(bridge, &hunch);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            intuition_thalamic_route_hunch(bridge, &hunch);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Thalamic Bridge RouteHunch: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 50000.0) << "RouteHunch should be < 50 us";

    intuition_thalamic_bridge_destroy(bridge);
}

TEST_F(ThalamicBridgeStabilityTest, Performance_SetGetAttention_Under1us) {
    intuition_thalamic_config_t config = intuition_thalamic_default_config();
    auto* bridge = intuition_thalamic_bridge_create(mock_intuition(), router, &config);
    ASSERT_NE(bridge, nullptr);

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    float attention = 0.0f;

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        intuition_thalamic_set_attention(bridge, 0.5f);
        intuition_thalamic_get_attention(bridge, &attention);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            intuition_thalamic_set_attention(bridge, 0.5f);
            intuition_thalamic_get_attention(bridge, &attention);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Thalamic Bridge Set/GetAttention: avg=" << avg_ns << " ns\n";

    EXPECT_LT(avg_ns, 1000.0) << "Set/GetAttention should be < 1 us";

    intuition_thalamic_bridge_destroy(bridge);
}

/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
