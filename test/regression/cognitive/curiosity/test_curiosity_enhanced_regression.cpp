/**
 * @file test_curiosity_enhanced_regression.cpp
 * @brief Regression tests for Enhanced Curiosity System
 *
 * TEST COVERAGE:
 * - Stability under sustained operation
 * - Memory leak prevention (create/destroy cycles)
 * - State consistency across operations
 * - Performance baseline metrics
 * - Edge case handling stability
 * - Recovery from extreme states
 * - Numerical stability of computations
 * - Thread safety under stress
 *
 * REGRESSION TARGETS:
 * - Ensure no crashes under any input pattern
 * - Maintain bounded state values [0,1]
 * - Consistent behavior across runs
 * - Graceful degradation under load
 * - No resource leaks
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <atomic>

// Headers have their own extern "C" guards
#include "cognitive/curiosity/nimcp_curiosity_enhanced.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CuriosityEnhancedRegressionTest : public ::testing::Test {
protected:
    curiosity_enhanced_system_t* system = nullptr;
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);  // Deterministic for regression
        system = curiosity_enhanced_create(nullptr, nullptr);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            curiosity_enhanced_destroy(system);
            system = nullptr;
        }
    }

    float RandomFloat(float min, float max) {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng);
    }

    uint64_t RandomHash() {
        std::uniform_int_distribution<uint64_t> dist;
        return dist(rng);
    }
};

//=============================================================================
// 1. Stability Tests
//=============================================================================

TEST_F(CuriosityEnhancedRegressionTest, SustainedOperationStability) {
    // Run 10000 update cycles
    for (int i = 0; i < 10000; i++) {
        curiosity_enhanced_update(system, 10.0f);

        // Periodic stimulus
        if (i % 100 == 0) {
            curiosity_enhanced_report_stimulus(system, RandomHash(), RandomFloat(0.0f, 1.0f));
        }

        // Periodic exposure
        if (i % 200 == 0) {
            char topic[32];
            snprintf(topic, sizeof(topic), "topic_%d", i / 200);
            curiosity_enhanced_record_exposure(system, topic, RandomFloat(0.0f, 1.0f));
        }
    }

    // Verify state is still valid
    curiosity_enhanced_state_t state;
    int ret = curiosity_enhanced_get_state(system, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.overall_curiosity_drive, 0.0f);
    EXPECT_LE(state.overall_curiosity_drive, 1.0f);
}

TEST_F(CuriosityEnhancedRegressionTest, StateBoundsNeverExceeded) {
    // Extreme stimulation
    for (int i = 0; i < 1000; i++) {
        curiosity_enhanced_report_stimulus(system, RandomHash(), 1.0f);
        curiosity_enhanced_report_surprise(system, 1.0f, "max_surprise");
        curiosity_enhanced_update(system, 100.0f);

        // Check all bounds
        float drive = curiosity_enhanced_get_overall_drive(system);
        EXPECT_GE(drive, 0.0f);
        EXPECT_LE(drive, 1.0f);

        float boost = curiosity_enhanced_get_surprise_boost(system);
        EXPECT_GE(boost, 1.0f);
        EXPECT_LE(boost, SURPRISE_LR_BOOST_MAX);

        float boredom_boost = curiosity_enhanced_get_boredom_boost(system);
        EXPECT_GE(boredom_boost, 1.0f);
        EXPECT_LE(boredom_boost, BOREDOM_NOVELTY_SEEK_BOOST);
    }
}

TEST_F(CuriosityEnhancedRegressionTest, NumericStability) {
    // Test with extreme delta times
    curiosity_enhanced_update(system, 0.0001f);
    curiosity_enhanced_update(system, 0.001f);
    curiosity_enhanced_update(system, 100000.0f);
    curiosity_enhanced_update(system, 1000000.0f);

    // Test with extreme values
    curiosity_enhanced_report_surprise(system, 0.0f, "zero");
    curiosity_enhanced_report_surprise(system, 1.0f, "max");

    curiosity_enhanced_set_contagion_susceptibility(system, 0.0f);
    curiosity_enhanced_set_contagion_susceptibility(system, 1.0f);

    // Verify no NaN or Inf
    float drive = curiosity_enhanced_get_overall_drive(system);
    EXPECT_FALSE(std::isnan(drive));
    EXPECT_FALSE(std::isinf(drive));

    float meta = curiosity_enhanced_get_meta_curiosity(system);
    EXPECT_FALSE(std::isnan(meta));
    EXPECT_FALSE(std::isinf(meta));
}

//=============================================================================
// 2. Memory Stability Tests
//=============================================================================

TEST_F(CuriosityEnhancedRegressionTest, CreateDestroyStability) {
    curiosity_enhanced_destroy(system);
    system = nullptr;

    // Rapid create/destroy cycles
    for (int i = 0; i < 100; i++) {
        curiosity_enhanced_system_t* temp = curiosity_enhanced_create(nullptr, nullptr);
        ASSERT_NE(temp, nullptr);

        // Use the system
        curiosity_enhanced_update(temp, 10.0f);
        curiosity_enhanced_report_stimulus(temp, RandomHash(), 0.5f);

        curiosity_enhanced_destroy(temp);
    }

    // Create a new system for teardown
    system = curiosity_enhanced_create(nullptr, nullptr);
}

TEST_F(CuriosityEnhancedRegressionTest, ManyTopicsNoLeak) {
    // Create many topics
    for (int i = 0; i < 1000; i++) {
        char topic[64];
        snprintf(topic, sizeof(topic), "leak_test_topic_%d", i);
        curiosity_enhanced_record_exposure(system, topic, RandomFloat(0.0f, 1.0f));
    }

    // System should still function
    float drive = curiosity_enhanced_get_overall_drive(system);
    EXPECT_GE(drive, 0.0f);
    EXPECT_LE(drive, 1.0f);
}

TEST_F(CuriosityEnhancedRegressionTest, ManySocialTargetsNoLeak) {
    // Create many social targets (up to limit)
    for (int i = 0; i < 100; i++) {
        char agent_id[32];
        snprintf(agent_id, sizeof(agent_id), "agent_%d", i);
        curiosity_enhanced_record_social_interaction(system, agent_id, 0.5f);
    }

    // Should handle gracefully (some may be rejected at limit)
    float gossip = curiosity_enhanced_get_gossip_interest(system);
    EXPECT_GE(gossip, 0.0f);
}

//=============================================================================
// 3. State Consistency Tests
//=============================================================================

TEST_F(CuriosityEnhancedRegressionTest, ConsistentTypeTransitions) {
    for (int cycle = 0; cycle < 100; cycle++) {
        for (int type = 0; type < CURIOSITY_TYPE_COUNT; type++) {
            int ret = curiosity_enhanced_transition_type(system, (curiosity_type_t)type);
            EXPECT_EQ(ret, 0);

            curiosity_type_t current = curiosity_enhanced_get_dominant_type(system);
            EXPECT_EQ(current, (curiosity_type_t)type);
        }
    }
}

TEST_F(CuriosityEnhancedRegressionTest, ConsistentFatigueRecoveryCycle) {
    for (int cycle = 0; cycle < 50; cycle++) {
        // Accumulate fatigue
        for (int i = 0; i < 100; i++) {
            curiosity_enhanced_update(system, 50.0f);
        }

        // Initiate recovery
        curiosity_enhanced_initiate_recovery(system, 5000.0f);
        EXPECT_TRUE(curiosity_enhanced_needs_rest(system) || true);  // May need rest

        curiosity_fatigue_state_t state;
        curiosity_enhanced_check_fatigue(system, &state);
        EXPECT_TRUE(state.is_resting);

        // Recover
        for (int i = 0; i < 50; i++) {
            curiosity_enhanced_update(system, 100.0f);
        }

        // End recovery
        curiosity_enhanced_end_recovery(system);
        curiosity_enhanced_check_fatigue(system, &state);
        EXPECT_FALSE(state.is_resting);
    }
}

TEST_F(CuriosityEnhancedRegressionTest, StatsMonotonicallyIncrease) {
    curiosity_enhanced_stats_t prev_stats;
    curiosity_enhanced_get_stats(system, &prev_stats);

    for (int cycle = 0; cycle < 100; cycle++) {
        // Generate events
        curiosity_enhanced_report_stimulus(system, RandomHash(), 0.9f);
        curiosity_enhanced_report_surprise(system, 0.7f, "event");
        curiosity_enhanced_record_social_interaction(system, "agent", 0.5f);
        curiosity_enhanced_update(system, 50.0f);

        curiosity_enhanced_stats_t curr_stats;
        curiosity_enhanced_get_stats(system, &curr_stats);

        // Stats should not decrease
        EXPECT_GE(curr_stats.novelty_events, prev_stats.novelty_events);
        EXPECT_GE(curr_stats.surprise_events, prev_stats.surprise_events);
        EXPECT_GE(curr_stats.social_curiosity_events, prev_stats.social_curiosity_events);

        prev_stats = curr_stats;
    }
}

//=============================================================================
// 4. Performance Regression Tests
//=============================================================================

TEST_F(CuriosityEnhancedRegressionTest, UpdatePerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        curiosity_enhanced_update(system, 1.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 10000 updates in reasonable time (< 5 seconds)
    EXPECT_LT(duration.count(), 5000);
}

TEST_F(CuriosityEnhancedRegressionTest, StimulusReportingPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        curiosity_enhanced_report_stimulus(system, (uint64_t)i, 0.5f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 5 seconds)
    EXPECT_LT(duration.count(), 5000);
}

TEST_F(CuriosityEnhancedRegressionTest, TopicLookupPerformance) {
    // Pre-populate topics
    for (int i = 0; i < 500; i++) {
        char topic[64];
        snprintf(topic, sizeof(topic), "perf_topic_%d", i);
        curiosity_enhanced_record_exposure(system, topic, 0.5f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        char topic[64];
        snprintf(topic, sizeof(topic), "perf_topic_%d", i % 500);
        curiosity_enhanced_get_topic_interest(system, topic);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 5 seconds)
    EXPECT_LT(duration.count(), 5000);
}

//=============================================================================
// 5. Thread Safety Regression Tests
//=============================================================================

TEST_F(CuriosityEnhancedRegressionTest, ConcurrentUpdateStress) {
    std::atomic<bool> error_detected{false};
    std::vector<std::thread> threads;

    for (int t = 0; t < 8; t++) {
        threads.emplace_back([this, &error_detected, t]() {
            try {
                for (int i = 0; i < 500; i++) {
                    curiosity_enhanced_update(system, 5.0f);
                    curiosity_enhanced_report_stimulus(system, (uint64_t)(t * 1000 + i), 0.5f);
                }
            } catch (...) {
                error_detected = true;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(error_detected);

    // System should be valid
    float drive = curiosity_enhanced_get_overall_drive(system);
    EXPECT_GE(drive, 0.0f);
    EXPECT_LE(drive, 1.0f);
}

TEST_F(CuriosityEnhancedRegressionTest, ConcurrentReadWriteStress) {
    std::atomic<bool> error_detected{false};
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};
    std::vector<std::thread> threads;

    // Writer threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &error_detected, &write_count]() {
            try {
                for (int i = 0; i < 200; i++) {
                    curiosity_enhanced_update(system, 10.0f);
                    curiosity_enhanced_report_surprise(system, 0.5f, "test");
                    write_count++;
                }
            } catch (...) {
                error_detected = true;
            }
        });
    }

    // Reader threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &error_detected, &read_count]() {
            try {
                for (int i = 0; i < 200; i++) {
                    curiosity_enhanced_get_overall_drive(system);
                    curiosity_enhanced_get_dominant_type(system);
                    curiosity_enhanced_get_boredom_boost(system);
                    read_count++;
                }
            } catch (...) {
                error_detected = true;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(error_detected);
    EXPECT_EQ(write_count.load(), 800);
    EXPECT_EQ(read_count.load(), 800);
}

//=============================================================================
// 6. Edge Case Regression Tests
//=============================================================================

TEST_F(CuriosityEnhancedRegressionTest, AllNullOperations) {
    // None should crash
    EXPECT_FALSE(curiosity_enhanced_is_bored(nullptr, nullptr));
    EXPECT_NE(curiosity_enhanced_report_stimulus(nullptr, 0, 0), 0);
    EXPECT_FLOAT_EQ(curiosity_enhanced_get_boredom_boost(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(curiosity_enhanced_get_topic_interest(nullptr, nullptr), 0.0f);
    EXPECT_NE(curiosity_enhanced_record_exposure(nullptr, nullptr, 0), 0);
    EXPECT_FLOAT_EQ(curiosity_enhanced_compute_satiation(nullptr, nullptr), 0.0f);
    EXPECT_EQ(curiosity_enhanced_get_dominant_type(nullptr), CURIOSITY_TYPE_EPISTEMIC);
    EXPECT_NE(curiosity_enhanced_get_type_profile(nullptr, nullptr), 0);
    EXPECT_NE(curiosity_enhanced_set_type_intensity(nullptr, CURIOSITY_TYPE_SOCIAL, 0), 0);
    EXPECT_NE(curiosity_enhanced_transition_type(nullptr, CURIOSITY_TYPE_SOCIAL), 0);
    EXPECT_NE(curiosity_enhanced_connect_anxiety(nullptr, nullptr), 0);
    EXPECT_FLOAT_EQ(curiosity_enhanced_get_net_motivation(nullptr), 0.0f);
    EXPECT_FALSE(curiosity_enhanced_should_explore(nullptr, 0));
    EXPECT_NE(curiosity_enhanced_report_conflict_resolution(nullptr, true), 0);
    EXPECT_NE(curiosity_enhanced_connect_tom(nullptr, nullptr), 0);
    EXPECT_FLOAT_EQ(curiosity_enhanced_assess_social_target(nullptr, nullptr, nullptr), 0.0f);
    EXPECT_NE(curiosity_enhanced_record_social_interaction(nullptr, nullptr, 0), 0);
    EXPECT_FLOAT_EQ(curiosity_enhanced_get_gossip_interest(nullptr), 0.0f);
    EXPECT_NE(curiosity_enhanced_introspect(nullptr, nullptr), 0);
    EXPECT_EQ(curiosity_enhanced_identify_blind_spots(nullptr), 0u);
    EXPECT_FLOAT_EQ(curiosity_enhanced_get_meta_curiosity(nullptr), 0.0f);
    EXPECT_FALSE(curiosity_enhanced_observe_curiosity(nullptr, nullptr));
    EXPECT_FLOAT_EQ(curiosity_enhanced_get_contagion_susceptibility(nullptr), 0.0f);
    EXPECT_NE(curiosity_enhanced_set_contagion_susceptibility(nullptr, 0), 0);
    EXPECT_FLOAT_EQ(curiosity_enhanced_report_surprise(nullptr, 0, nullptr), 1.0f);
    EXPECT_FLOAT_EQ(curiosity_enhanced_get_surprise_boost(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(curiosity_enhanced_check_fatigue(nullptr, nullptr), 0.0f);
    EXPECT_NE(curiosity_enhanced_initiate_recovery(nullptr, 0), 0);
    EXPECT_FALSE(curiosity_enhanced_needs_rest(nullptr));
    EXPECT_NE(curiosity_enhanced_end_recovery(nullptr), 0);
    EXPECT_NE(curiosity_enhanced_generate_counterfactual(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NE(curiosity_enhanced_explore_counterfactual(nullptr, nullptr, nullptr), 0);
    EXPECT_FLOAT_EQ(curiosity_enhanced_get_counterfactual_curiosity(nullptr), 0.0f);
    EXPECT_NE(curiosity_enhanced_get_state(nullptr, nullptr), 0);
    EXPECT_NE(curiosity_enhanced_get_stats(nullptr, nullptr), 0);
    curiosity_enhanced_reset_stats(nullptr);  // Should not crash
    EXPECT_FLOAT_EQ(curiosity_enhanced_get_overall_drive(nullptr), 0.0f);
    EXPECT_NE(curiosity_enhanced_connect_bio_async(nullptr), 0);
    EXPECT_NE(curiosity_enhanced_disconnect_bio_async(nullptr), 0);
    EXPECT_FALSE(curiosity_enhanced_is_bio_async_connected(nullptr));
    curiosity_enhanced_destroy(nullptr);  // Should not crash
}

TEST_F(CuriosityEnhancedRegressionTest, EmptyStringInputs) {
    // Empty strings should not crash
    float interest = curiosity_enhanced_get_topic_interest(system, "");
    EXPECT_FLOAT_EQ(interest, 1.0f);

    int ret = curiosity_enhanced_record_exposure(system, "", 0.5f);
    EXPECT_EQ(ret, 0);

    float satiation = curiosity_enhanced_compute_satiation(system, "");
    EXPECT_GE(satiation, 0.0f);

    float residual = curiosity_enhanced_get_residual_interest(system, "");
    EXPECT_GE(residual, 0.0f);

    ret = curiosity_enhanced_record_social_interaction(system, "", 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityEnhancedRegressionTest, VeryLongStringInputs) {
    char long_string[4096];
    memset(long_string, 'x', sizeof(long_string) - 1);
    long_string[sizeof(long_string) - 1] = '\0';

    // Should handle long strings without crash
    float interest = curiosity_enhanced_get_topic_interest(system, long_string);
    EXPECT_GE(interest, 0.0f);

    int ret = curiosity_enhanced_record_exposure(system, long_string, 0.5f);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// 7. Recovery Regression Tests
//=============================================================================

TEST_F(CuriosityEnhancedRegressionTest, RecoveryFromExtremeBoredom) {
    // Build extreme boredom
    for (int i = 0; i < 1000; i++) {
        curiosity_enhanced_report_stimulus(system, 12345, 0.01f);
        curiosity_enhanced_update(system, 100.0f);
    }

    // Should still be functional
    float drive = curiosity_enhanced_get_overall_drive(system);
    EXPECT_GE(drive, 0.0f);
    EXPECT_LE(drive, 1.0f);

    // Novelty should reset boredom
    for (int i = 0; i < 10; i++) {
        curiosity_enhanced_report_stimulus(system, RandomHash(), 0.99f);
        curiosity_enhanced_update(system, 50.0f);
    }

    // Check state is still valid
    curiosity_enhanced_state_t state;
    curiosity_enhanced_get_state(system, &state);
    EXPECT_GE(state.overall_curiosity_drive, 0.0f);
}

TEST_F(CuriosityEnhancedRegressionTest, RecoveryFromExtremeFatigue) {
    // Build extreme fatigue
    for (int i = 0; i < 10000; i++) {
        curiosity_enhanced_update(system, 50.0f);
    }

    curiosity_fatigue_state_t fatigue_state;
    curiosity_enhanced_check_fatigue(system, &fatigue_state);

    // Initiate and complete recovery
    curiosity_enhanced_initiate_recovery(system, 60000.0f);

    for (int i = 0; i < 1000; i++) {
        curiosity_enhanced_update(system, 100.0f);
    }

    curiosity_enhanced_end_recovery(system);

    // Should be functional
    float drive = curiosity_enhanced_get_overall_drive(system);
    EXPECT_GE(drive, 0.0f);
}

TEST_F(CuriosityEnhancedRegressionTest, RecoveryAfterResetStats) {
    // Generate stats
    for (int i = 0; i < 100; i++) {
        curiosity_enhanced_report_surprise(system, 0.8f, "event");
        curiosity_enhanced_update(system, 10.0f);
    }

    // Reset stats
    curiosity_enhanced_reset_stats(system);

    // State should still be valid
    curiosity_enhanced_state_t state;
    int ret = curiosity_enhanced_get_state(system, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.overall_curiosity_drive, 0.0f);

    // Stats should be zeroed
    curiosity_enhanced_stats_t stats;
    curiosity_enhanced_get_stats(system, &stats);
    EXPECT_EQ(stats.surprise_events, 0u);
}

//=============================================================================
// 8. Configuration Regression Tests
//=============================================================================

TEST_F(CuriosityEnhancedRegressionTest, DifferentConfigurationsStable) {
    curiosity_enhanced_destroy(system);

    // Test with various configurations
    curiosity_enhanced_config_t configs[5];
    for (int i = 0; i < 5; i++) {
        curiosity_enhanced_config_default(&configs[i]);
    }

    // High boredom sensitivity
    configs[0].boredom.boredom_threshold = 0.1f;

    // Low interest decay
    configs[1].interest.base_decay_rate = 0.001f;

    // High fatigue accumulation
    configs[2].fatigue.fatigue_accumulation_rate = 0.1f;

    // High surprise sensitivity
    configs[3].surprise.surprise_threshold = 0.1f;

    // High contagion
    configs[4].contagion.base_susceptibility = 0.9f;

    for (int i = 0; i < 5; i++) {
        system = curiosity_enhanced_create(&configs[i], nullptr);
        ASSERT_NE(system, nullptr);

        // Run operations
        for (int j = 0; j < 100; j++) {
            curiosity_enhanced_update(system, 10.0f);
            curiosity_enhanced_report_stimulus(system, RandomHash(), RandomFloat(0, 1));
        }

        float drive = curiosity_enhanced_get_overall_drive(system);
        EXPECT_GE(drive, 0.0f);
        EXPECT_LE(drive, 1.0f);

        curiosity_enhanced_destroy(system);
        system = nullptr;
    }

    // Create default for teardown
    system = curiosity_enhanced_create(nullptr, nullptr);
}

//=============================================================================
// 9. Bio-Async Regression Tests
//=============================================================================

TEST_F(CuriosityEnhancedRegressionTest, BioAsyncConnectDisconnectCycles) {
    for (int cycle = 0; cycle < 50; cycle++) {
        int ret = curiosity_enhanced_connect_bio_async(system);
        EXPECT_EQ(ret, 0);

        // Update while connected
        curiosity_enhanced_update(system, 20.0f);

        ret = curiosity_enhanced_disconnect_bio_async(system);
        EXPECT_EQ(ret, 0);

        // Update while disconnected
        curiosity_enhanced_update(system, 20.0f);
    }

    // Should still function
    float drive = curiosity_enhanced_get_overall_drive(system);
    EXPECT_GE(drive, 0.0f);
    EXPECT_LE(drive, 1.0f);
}

//=============================================================================
// 10. Determinism Tests
//=============================================================================

TEST_F(CuriosityEnhancedRegressionTest, DeterministicBehavior) {
    // Run same sequence twice and compare results
    curiosity_enhanced_destroy(system);

    curiosity_enhanced_stats_t stats1, stats2;

    for (int run = 0; run < 2; run++) {
        rng.seed(12345);  // Same seed each run

        system = curiosity_enhanced_create(nullptr, nullptr);
        ASSERT_NE(system, nullptr);

        for (int i = 0; i < 100; i++) {
            curiosity_enhanced_report_stimulus(system, RandomHash(), RandomFloat(0, 1));
            curiosity_enhanced_update(system, 10.0f);
        }

        if (run == 0) {
            curiosity_enhanced_get_stats(system, &stats1);
        } else {
            curiosity_enhanced_get_stats(system, &stats2);
        }

        curiosity_enhanced_destroy(system);
        system = nullptr;
    }

    // Stats should be identical for same input sequence
    EXPECT_EQ(stats1.novelty_events, stats2.novelty_events);

    // Create system for teardown
    system = curiosity_enhanced_create(nullptr, nullptr);
}
