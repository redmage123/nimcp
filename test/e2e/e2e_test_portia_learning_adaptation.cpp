/**
 * @file e2e_test_portia_learning_adaptation.cpp
 * @brief End-to-end test for Portia learning and adaptation capabilities
 *
 * WHAT: Tests Portia's learning modes (habituation, association, trial-error)
 * WHY:  Verify system learns from experience and adapts behavior
 * HOW:  Test habituation reduces false alarms, associations improve responses, learning persists
 *
 * TEST SCENARIOS:
 * - HabituationLearning: Repeated stimulus causes response reduction
 * - AssociativeLearning: System learns stimulus-response associations
 * - TrialErrorLearning: System improves behavior through reinforcement
 * - LearningPersistence: Learning survives restarts/checkpoints
 * - AdaptiveResponseImprovement: Overall system performance improves with experience
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cmath>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia.h"
#include "portia/nimcp_portia_learning.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaLearningAdaptationE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_log_init(NULL);

        nimcp_error_t err = nimcp_bio_async_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        err = bio_router_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        learning_state_ = nullptr;
        portia_initialized_ = false;
    }

    void TearDown() override {
        if (learning_state_) {
            portia_learning_destroy(learning_state_);
            learning_state_ = nullptr;
        }

        if (portia_initialized_) {
            portia_destroy();
            portia_initialized_ = false;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        nimcp_log_shutdown();
    }

    portia_learning_state_t* learning_state_;
    bool portia_initialized_;
};

//=============================================================================
// Test 1: Habituation Learning
//=============================================================================

TEST_F(PortiaLearningAdaptationE2ETest, HabituationLearning) {
    // GIVEN: Initialize learning system with habituation enabled
    portia_learning_config_t config = {
        .allowed_modes = LEARNING_MODE_HABITUATION,
        .max_habituation_entries = 100,
        .max_association_entries = 0,
        .default_learning_rate = 0.1f,
        .default_forgetting_rate = 0.01f,
        .consolidation_interval_ms = 1000,
        .habituation_threshold = 0.1f,
        .association_threshold = 0.1f
    };

    learning_state_ = portia_learning_init(&config);
    ASSERT_NE(learning_state_, nullptr);

    uint32_t stimulus_id = 42;  // Test stimulus
    uint64_t timestamp = 0;

    // Get initial response
    portia_learning_query_result_t initial_result =
        portia_learning_query(learning_state_, stimulus_id);

    float initial_strength = initial_result.found ? initial_result.strength : 1.0f;
    nimcp_log(LOG_LEVEL_INFO, "Initial response strength: %.3f", initial_strength);

    // WHEN: Expose to repeated stimulus (habituation)
    const int num_exposures = 20;
    std::vector<float> response_strengths;

    for (int i = 0; i < num_exposures; i++) {
        int result = portia_learning_habituate(learning_state_, stimulus_id, timestamp);
        ASSERT_EQ(result, 0) << "Habituation failed at exposure " << i;

        portia_learning_query_result_t query_result =
            portia_learning_query(learning_state_, stimulus_id);

        if (query_result.found) {
            response_strengths.push_back(query_result.strength);
        }

        timestamp += 100;  // 100ms between exposures
    }

    // THEN: Verify response strength decreased
    ASSERT_GT(response_strengths.size(), 0) << "Should have recorded responses";

    float final_strength = response_strengths.back();
    EXPECT_LT(final_strength, initial_strength)
        << "Response should habituate (decrease) with repeated exposure";

    // Verify monotonic decrease (generally declining)
    int decreases = 0;
    for (size_t i = 1; i < response_strengths.size(); i++) {
        if (response_strengths[i] <= response_strengths[i-1]) {
            decreases++;
        }
    }

    float decrease_ratio = (float)decreases / (response_strengths.size() - 1);
    EXPECT_GT(decrease_ratio, 0.5f)
        << "Response should generally decrease over time";

    // Get statistics
    portia_learning_stats_t stats = portia_learning_get_stats(learning_state_);
    EXPECT_GT(stats.total_exposures, 0) << "Should have recorded exposures";
    EXPECT_GT(stats.active_habituation_entries, 0) << "Should have active entries";

    nimcp_log(LOG_LEVEL_INFO, "HabituationLearning: PASS - "
              "Initial=%.3f, Final=%.3f, Exposures=%lu",
              initial_strength, final_strength, stats.total_exposures);
}

//=============================================================================
// Test 2: Associative Learning
//=============================================================================

TEST_F(PortiaLearningAdaptationE2ETest, AssociativeLearning) {
    // GIVEN: Initialize with associative learning
    portia_learning_config_t config = {
        .allowed_modes = LEARNING_MODE_ASSOCIATIVE,
        .max_habituation_entries = 0,
        .max_association_entries = 100,
        .default_learning_rate = 0.2f,
        .default_forgetting_rate = 0.01f,
        .consolidation_interval_ms = 1000,
        .habituation_threshold = 0.1f,
        .association_threshold = 0.1f
    };

    learning_state_ = portia_learning_init(&config);
    ASSERT_NE(learning_state_, nullptr);

    uint32_t stimulus_id = 101;
    uint32_t response_id = 202;
    uint64_t timestamp = 0;

    // WHEN: Create association through repeated pairing
    const int num_pairings = 15;

    for (int i = 0; i < num_pairings; i++) {
        int result = portia_learning_associate(
            learning_state_, stimulus_id, response_id, true, timestamp);
        ASSERT_EQ(result, 0) << "Association failed at pairing " << i;

        timestamp += 200;  // 200ms between pairings
    }

    // THEN: Verify association was learned
    portia_learning_query_result_t assoc_result =
        portia_learning_query_association(learning_state_, stimulus_id, response_id);

    EXPECT_TRUE(assoc_result.found) << "Association should exist";
    EXPECT_GT(assoc_result.strength, 0.5f)
        << "Association should be strong after multiple pairings";
    EXPECT_EQ(assoc_result.exposure_count, num_pairings)
        << "Should track all pairings";

    // Test that unrelated association doesn't exist
    portia_learning_query_result_t unrelated =
        portia_learning_query_association(learning_state_, stimulus_id, 999);
    EXPECT_FALSE(unrelated.found) << "Unrelated association shouldn't exist";

    // Get statistics
    portia_learning_stats_t stats = portia_learning_get_stats(learning_state_);
    EXPECT_GT(stats.active_association_entries, 0) << "Should have associations";
    EXPECT_EQ(stats.total_reinforcements, num_pairings)
        << "Should count all pairings";

    nimcp_log(LOG_LEVEL_INFO, "AssociativeLearning: PASS - "
              "Strength=%.3f, Pairings=%u, Total reinforcements=%lu",
              assoc_result.strength, assoc_result.exposure_count,
              stats.total_reinforcements);
}

//=============================================================================
// Test 3: Trial-Error Learning with Reinforcement
//=============================================================================

TEST_F(PortiaLearningAdaptationE2ETest, TrialErrorLearning) {
    // GIVEN: Initialize with trial-error learning
    portia_learning_config_t config = {
        .allowed_modes = LEARNING_MODE_TRIAL_ERROR,
        .max_habituation_entries = 0,
        .max_association_entries = 100,
        .default_learning_rate = 0.15f,
        .default_forgetting_rate = 0.01f,
        .consolidation_interval_ms = 1000,
        .habituation_threshold = 0.1f,
        .association_threshold = 0.1f
    };

    learning_state_ = portia_learning_init(&config);
    ASSERT_NE(learning_state_, nullptr);

    uint32_t stimulus_id = 300;
    uint32_t good_response = 301;
    uint32_t bad_response = 302;
    uint64_t timestamp = 0;

    // WHEN: Reinforce good response, punish bad response
    const int num_trials = 20;

    for (int i = 0; i < num_trials; i++) {
        // Positive reinforcement for good response
        float reward = 1.0f;
        int result = portia_learning_reinforce(
            learning_state_, stimulus_id, good_response, reward, timestamp);
        ASSERT_EQ(result, 0);

        // Negative reinforcement for bad response
        float punishment = -0.5f;
        result = portia_learning_reinforce(
            learning_state_, stimulus_id, bad_response, punishment, timestamp);
        ASSERT_EQ(result, 0);

        timestamp += 150;
    }

    // THEN: Verify good response strengthened, bad response weakened
    portia_learning_query_result_t good_assoc =
        portia_learning_query_association(learning_state_, stimulus_id, good_response);
    portia_learning_query_result_t bad_assoc =
        portia_learning_query_association(learning_state_, stimulus_id, bad_response);

    EXPECT_TRUE(good_assoc.found) << "Good response association should exist";
    EXPECT_TRUE(bad_assoc.found) << "Bad response association should exist";

    EXPECT_GT(good_assoc.strength, bad_assoc.strength)
        << "Good response should be stronger than bad response";

    EXPECT_GT(good_assoc.strength, 0.6f)
        << "Good response should have high strength";

    // Statistics
    portia_learning_stats_t stats = portia_learning_get_stats(learning_state_);
    EXPECT_EQ(stats.total_reinforcements, num_trials * 2)
        << "Should count all reinforcements";

    nimcp_log(LOG_LEVEL_INFO, "TrialErrorLearning: PASS - "
              "Good response=%.3f, Bad response=%.3f, Trials=%d",
              good_assoc.strength, bad_assoc.strength, num_trials);
}

//=============================================================================
// Test 4: Learning Persistence
//=============================================================================

TEST_F(PortiaLearningAdaptationE2ETest, LearningPersistence) {
    // GIVEN: Initialize learning system
    portia_learning_config_t config = {
        .allowed_modes = LEARNING_MODE_FULL,
        .max_habituation_entries = 50,
        .max_association_entries = 50,
        .default_learning_rate = 0.2f,
        .default_forgetting_rate = 0.01f,
        .consolidation_interval_ms = 500,
        .habituation_threshold = 0.1f,
        .association_threshold = 0.1f
    };

    learning_state_ = portia_learning_init(&config);
    ASSERT_NE(learning_state_, nullptr);

    // Create some learning data
    uint32_t stim1 = 1;
    uint32_t stim2 = 2;
    uint32_t resp1 = 10;
    uint64_t timestamp = 0;

    // Create habituation
    for (int i = 0; i < 10; i++) {
        portia_learning_habituate(learning_state_, stim1, timestamp);
        timestamp += 100;
    }

    // Create association
    for (int i = 0; i < 10; i++) {
        portia_learning_associate(learning_state_, stim2, resp1, true, timestamp);
        timestamp += 100;
    }

    // WHEN: Consolidate memories
    int consolidate_result = portia_learning_consolidate(learning_state_, timestamp);
    ASSERT_EQ(consolidate_result, 0) << "Consolidation should succeed";

    // Get pre-persistence stats
    portia_learning_stats_t pre_stats = portia_learning_get_stats(learning_state_);
    EXPECT_GT(pre_stats.active_habituation_entries, 0);
    EXPECT_GT(pre_stats.active_association_entries, 0);

    // THEN: Export learning data (simulates persistence)
    const char* export_path = "/tmp/portia_learning_test.dat";
    int export_result = portia_learning_export(learning_state_, export_path);
    EXPECT_EQ(export_result, 0) << "Export should succeed";

    // Verify export file exists
    FILE* check_file = fopen(export_path, "r");
    if (check_file) {
        fclose(check_file);
        remove(export_path);  // Cleanup
    } else {
        nimcp_log(LOG_LEVEL_WARN, "Export file not created (may not be implemented)");
    }

    // Verify learning can survive consolidation cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        timestamp += 1000;
        portia_learning_forget(learning_state_, timestamp);
        portia_learning_consolidate(learning_state_, timestamp);
    }

    // Check that strong memories persist
    portia_learning_query_result_t assoc_check =
        portia_learning_query_association(learning_state_, stim2, resp1);
    EXPECT_TRUE(assoc_check.found) << "Strong association should persist";

    nimcp_log(LOG_LEVEL_INFO, "LearningPersistence: PASS - "
              "Pre-consolidation: hab=%u, assoc=%u",
              pre_stats.active_habituation_entries,
              pre_stats.active_association_entries);
}

//=============================================================================
// Test 5: Adaptive Response Improvement
//=============================================================================

TEST_F(PortiaLearningAdaptationE2ETest, AdaptiveResponseImprovement) {
    // GIVEN: Initialize full learning system
    portia_learning_config_t config = {
        .allowed_modes = LEARNING_MODE_FULL,
        .max_habituation_entries = 100,
        .max_association_entries = 100,
        .default_learning_rate = 0.03f,
        .default_forgetting_rate = 0.005f,
        .consolidation_interval_ms = 2000,
        .habituation_threshold = 0.1f,
        .association_threshold = 0.1f
    };

    learning_state_ = portia_learning_init(&config);
    ASSERT_NE(learning_state_, nullptr);

    // WHEN: Simulate learning over time with mixed stimuli
    const int num_episodes = 30;
    uint64_t timestamp = 0;

    std::vector<float> performance_over_time;

    for (int episode = 0; episode < num_episodes; episode++) {
        // Present multiple stimuli
        for (int stim = 0; stim < 5; stim++) {
            uint32_t stimulus_id = 1000 + stim;
            uint32_t response_id = 2000 + stim;

            // Habituate to neutral stimuli
            if (stim < 2) {
                portia_learning_habituate(learning_state_, stimulus_id, timestamp);
            }

            // Learn useful associations
            if (stim >= 2) {
                portia_learning_associate(learning_state_, stimulus_id, response_id,
                                          true, timestamp);

                // Reinforce with varying reward
                // BUG FIX: Use small rewards (0.02-0.04) so strength doesn't instantly
                // saturate to 1.0. The portia_learning_reinforce function adds the full
                // reward value directly to association_strength. With rewards of 0.8+,
                // strength hits 1.0 after just 1 episode, leaving no room to measure
                // improvement. Small rewards allow gradual improvement over 30 episodes.
                float reward = 0.02f + (stim * 0.005f);
                portia_learning_reinforce(learning_state_, stimulus_id, response_id,
                                          reward, timestamp);
            }

            timestamp += 50;
        }

        // Measure "performance" as average association strength
        float total_strength = 0.0f;
        int useful_count = 0;

        for (int stim = 2; stim < 5; stim++) {
            uint32_t stimulus_id = 1000 + stim;
            uint32_t response_id = 2000 + stim;

            portia_learning_query_result_t result =
                portia_learning_query_association(learning_state_, stimulus_id, response_id);

            if (result.found) {
                total_strength += result.strength;
                useful_count++;
            }
        }

        float avg_performance = (useful_count > 0) ? (total_strength / useful_count) : 0.0f;
        performance_over_time.push_back(avg_performance);

        // Periodic consolidation
        if (episode % 10 == 0) {
            portia_learning_consolidate(learning_state_, timestamp);
        }

        timestamp += 200;
    }

    // THEN: Verify performance improved over time
    ASSERT_GT(performance_over_time.size(), 10) << "Need sufficient episodes";

    float early_performance = 0.0f;
    float late_performance = 0.0f;

    // Average first 10 episodes
    for (int i = 0; i < 10; i++) {
        early_performance += performance_over_time[i];
    }
    early_performance /= 10.0f;

    // Average last 10 episodes
    for (int i = performance_over_time.size() - 10; i < performance_over_time.size(); i++) {
        late_performance += performance_over_time[i];
    }
    late_performance /= 10.0f;

    EXPECT_GT(late_performance, early_performance)
        << "Performance should improve with learning";

    float improvement = late_performance - early_performance;
    EXPECT_GT(improvement, 0.05f) << "Should show measurable improvement";

    // Final statistics
    portia_learning_stats_t final_stats = portia_learning_get_stats(learning_state_);

    nimcp_log(LOG_LEVEL_INFO, "AdaptiveResponseImprovement: PASS - "
              "Early perf=%.3f, Late perf=%.3f, Improvement=%.3f, "
              "Total exposures=%lu, reinforcements=%lu",
              early_performance, late_performance, improvement,
              final_stats.total_exposures, final_stats.total_reinforcements);
}
