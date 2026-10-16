/**
 * @file test_portia_learning.cpp
 * @brief Unit tests for Portia spider learning subsystem
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_learning.h"
#include "security/nimcp_blood_brain_barrier.h"

class PortiaLearningTest : public ::testing::Test {
protected:
    portia_learning_state_t* learning_state;
    portia_learning_config_t config;

    void SetUp() override {
        // Setup default configuration
        config.allowed_modes = LEARNING_MODE_FULL;
        config.max_habituation_entries = 16;
        config.max_association_entries = 16;
        config.default_learning_rate = 0.1f;
        config.default_forgetting_rate = 0.05f;
        config.consolidation_interval_ms = 1000;
        config.habituation_threshold = 0.1f;
        config.association_threshold = 0.1f;

        learning_state = portia_learning_init(&config);
    }

    void TearDown() override {
        if (learning_state) {
            portia_learning_destroy(learning_state);
            learning_state = nullptr;
        }
    }
};

// Initialization and Cleanup Tests
TEST_F(PortiaLearningTest, InitializationSuccess) {
    ASSERT_NE(learning_state, nullptr);
    EXPECT_TRUE(learning_state->is_initialized);
    EXPECT_EQ(learning_state->active_mode, LEARNING_MODE_FULL);
    EXPECT_EQ(learning_state->habituation_capacity, 16u);
    EXPECT_EQ(learning_state->association_capacity, 16u);
}

TEST_F(PortiaLearningTest, InitializationWithNullConfig) {
    portia_learning_state_t* state = portia_learning_init(nullptr);
    EXPECT_NE(state, nullptr); // Should use defaults
    portia_learning_destroy(state);
}

TEST_F(PortiaLearningTest, DestroyNullState) {
    // Should not crash
    portia_learning_destroy(nullptr);
}

TEST_F(PortiaLearningTest, InitialStatistics) {
    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);

    EXPECT_EQ(stats.active_habituation_entries, 0u);
    EXPECT_EQ(stats.active_association_entries, 0u);
    EXPECT_EQ(stats.total_exposures, 0u);
    EXPECT_EQ(stats.total_reinforcements, 0u);
}

// Learning Mode Tests
TEST_F(PortiaLearningTest, SetLearningMode) {
    int result = portia_learning_set_mode(learning_state, LEARNING_MODE_HABITUATION);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(learning_state->active_mode, LEARNING_MODE_HABITUATION);
}

TEST_F(PortiaLearningTest, SetMultipleModesWithBitmask) {
    portia_learning_mode_t mode = static_cast<portia_learning_mode_t>(
        LEARNING_MODE_HABITUATION | LEARNING_MODE_ASSOCIATIVE
    );
    int result = portia_learning_set_mode(learning_state, mode);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(learning_state->active_mode & LEARNING_MODE_HABITUATION);
    EXPECT_TRUE(learning_state->active_mode & LEARNING_MODE_ASSOCIATIVE);
}

TEST_F(PortiaLearningTest, SetModeToDisabled) {
    int result = portia_learning_set_mode(learning_state, LEARNING_MODE_DISABLED);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(learning_state->active_mode, LEARNING_MODE_DISABLED);
}

TEST_F(PortiaLearningTest, SetModeNullState) {
    int result = portia_learning_set_mode(nullptr, LEARNING_MODE_HABITUATION);
    EXPECT_NE(result, 0);
}

// Habituation Tests
TEST_F(PortiaLearningTest, BasicHabituation) {
    uint32_t stimulus_id = 42;
    uint64_t timestamp = 1000;

    int result = portia_learning_habituate(learning_state, stimulus_id, timestamp);
    EXPECT_EQ(result, 0);

    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_GT(stats.active_habituation_entries, 0u);
}

TEST_F(PortiaLearningTest, RepeatedHabituationReducesResponse) {
    uint32_t stimulus_id = 100;
    uint64_t timestamp = 1000;

    // First exposure
    portia_learning_habituate(learning_state, stimulus_id, timestamp);
    portia_learning_query_result_t result1 = portia_learning_query(learning_state, stimulus_id);

    // Second exposure - should reduce response
    timestamp += 100;
    portia_learning_habituate(learning_state, stimulus_id, timestamp);
    portia_learning_query_result_t result2 = portia_learning_query(learning_state, stimulus_id);

    EXPECT_TRUE(result1.found);
    EXPECT_TRUE(result2.found);
    EXPECT_LT(result2.strength, result1.strength);
}

TEST_F(PortiaLearningTest, HabituationMultipleStimuli) {
    uint64_t timestamp = 1000;

    for (uint32_t i = 0; i < 10; i++) {
        int result = portia_learning_habituate(learning_state, i, timestamp);
        EXPECT_EQ(result, 0);
        timestamp += 100;
    }

    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_EQ(stats.active_habituation_entries, 10u);
}

TEST_F(PortiaLearningTest, HabituationCapacityLimit) {
    uint64_t timestamp = 1000;

    // Fill beyond capacity
    for (uint32_t i = 0; i < config.max_habituation_entries + 5; i++) {
        portia_learning_habituate(learning_state, i, timestamp);
        timestamp += 100;
    }

    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_LE(stats.active_habituation_entries, config.max_habituation_entries);
    EXPECT_GT(stats.habituation_evictions, 0u);
}

TEST_F(PortiaLearningTest, HabituationNullState) {
    int result = portia_learning_habituate(nullptr, 42, 1000);
    EXPECT_NE(result, 0);
}

// Sensitization Tests
TEST_F(PortiaLearningTest, BasicSensitization) {
    uint32_t stimulus_id = 50;
    float boost = 0.3f;
    uint64_t timestamp = 1000;

    // First habituate
    portia_learning_habituate(learning_state, stimulus_id, timestamp);
    portia_learning_query_result_t result1 = portia_learning_query(learning_state, stimulus_id);

    // Then sensitize
    int result = portia_learning_sensitize(learning_state, stimulus_id, boost, timestamp + 100);
    EXPECT_EQ(result, 0);

    portia_learning_query_result_t result2 = portia_learning_query(learning_state, stimulus_id);
    EXPECT_GT(result2.strength, result1.strength);
}

TEST_F(PortiaLearningTest, SensitizationNewStimulus) {
    uint32_t stimulus_id = 60;
    float boost = 0.5f;
    uint64_t timestamp = 1000;

    int result = portia_learning_sensitize(learning_state, stimulus_id, boost, timestamp);
    EXPECT_EQ(result, 0);

    portia_learning_query_result_t query = portia_learning_query(learning_state, stimulus_id);
    EXPECT_TRUE(query.found);
    EXPECT_GT(query.strength, 0.0f);
}

TEST_F(PortiaLearningTest, SensitizationNullState) {
    int result = portia_learning_sensitize(nullptr, 42, 0.5f, 1000);
    EXPECT_NE(result, 0);
}

// Association Tests
TEST_F(PortiaLearningTest, BasicAssociation) {
    uint32_t stimulus_id = 200;
    uint32_t response_id = 300;
    uint64_t timestamp = 1000;

    int result = portia_learning_associate(learning_state, stimulus_id, response_id, true, timestamp);
    EXPECT_EQ(result, 0);

    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_GT(stats.active_association_entries, 0u);
}

TEST_F(PortiaLearningTest, PositiveAndNegativeAssociations) {
    uint32_t stimulus_id = 210;
    uint32_t response_id1 = 310;
    uint32_t response_id2 = 320;
    uint64_t timestamp = 1000;

    // Positive association
    portia_learning_associate(learning_state, stimulus_id, response_id1, true, timestamp);
    portia_learning_query_result_t result1 =
        portia_learning_query_association(learning_state, stimulus_id, response_id1);

    // Negative association
    portia_learning_associate(learning_state, stimulus_id, response_id2, false, timestamp + 100);
    portia_learning_query_result_t result2 =
        portia_learning_query_association(learning_state, stimulus_id, response_id2);

    EXPECT_TRUE(result1.found);
    EXPECT_TRUE(result2.found);
}

TEST_F(PortiaLearningTest, MultipleAssociations) {
    uint64_t timestamp = 1000;

    for (uint32_t i = 0; i < 10; i++) {
        int result = portia_learning_associate(learning_state, i, i + 100, true, timestamp);
        EXPECT_EQ(result, 0);
        timestamp += 100;
    }

    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_EQ(stats.active_association_entries, 10u);
}

TEST_F(PortiaLearningTest, AssociationCapacityLimit) {
    uint64_t timestamp = 1000;

    // Fill beyond capacity (LRU eviction should occur)
    for (uint32_t i = 0; i < config.max_association_entries + 5; i++) {
        portia_learning_associate(learning_state, i, i + 100, true, timestamp);
        timestamp += 100;
    }

    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_LE(stats.active_association_entries, config.max_association_entries);
    EXPECT_GT(stats.association_evictions, 0u);
}

TEST_F(PortiaLearningTest, AssociationNullState) {
    int result = portia_learning_associate(nullptr, 1, 2, true, 1000);
    EXPECT_NE(result, 0);
}

// Reinforcement Tests
TEST_F(PortiaLearningTest, BasicReinforcement) {
    uint32_t stimulus_id = 400;
    uint32_t response_id = 500;
    uint64_t timestamp = 1000;

    // Create association
    portia_learning_associate(learning_state, stimulus_id, response_id, true, timestamp);
    portia_learning_query_result_t result1 =
        portia_learning_query_association(learning_state, stimulus_id, response_id);

    // Reinforce it
    float reward = 0.5f;
    int result = portia_learning_reinforce(learning_state, stimulus_id, response_id, reward, timestamp + 100);
    EXPECT_EQ(result, 0);

    portia_learning_query_result_t result2 =
        portia_learning_query_association(learning_state, stimulus_id, response_id);

    EXPECT_TRUE(result1.found);
    EXPECT_TRUE(result2.found);
    EXPECT_GE(result2.strength, result1.strength);
}

TEST_F(PortiaLearningTest, RepeatedReinforcement) {
    uint32_t stimulus_id = 410;
    uint32_t response_id = 510;
    uint64_t timestamp = 1000;

    portia_learning_associate(learning_state, stimulus_id, response_id, true, timestamp);

    // Multiple reinforcements
    for (int i = 0; i < 5; i++) {
        portia_learning_reinforce(learning_state, stimulus_id, response_id, 0.3f, timestamp);
        timestamp += 100;
    }

    portia_learning_query_result_t result =
        portia_learning_query_association(learning_state, stimulus_id, response_id);

    EXPECT_TRUE(result.found);
    EXPECT_GT(result.strength, 0.5f); // Should be well-learned
}

TEST_F(PortiaLearningTest, NegativeReinforcement) {
    uint32_t stimulus_id = 420;
    uint32_t response_id = 520;
    uint64_t timestamp = 1000;

    portia_learning_associate(learning_state, stimulus_id, response_id, true, timestamp);
    portia_learning_query_result_t result1 =
        portia_learning_query_association(learning_state, stimulus_id, response_id);

    // Negative reward weakens association
    float negative_reward = -0.3f;
    portia_learning_reinforce(learning_state, stimulus_id, response_id, negative_reward, timestamp + 100);

    portia_learning_query_result_t result2 =
        portia_learning_query_association(learning_state, stimulus_id, response_id);

    EXPECT_TRUE(result1.found);
    EXPECT_TRUE(result2.found);
    EXPECT_LT(result2.strength, result1.strength);
}

TEST_F(PortiaLearningTest, ReinforcementNullState) {
    int result = portia_learning_reinforce(nullptr, 1, 2, 0.5f, 1000);
    EXPECT_NE(result, 0);
}

// Query Tests
TEST_F(PortiaLearningTest, QueryNonexistentStimulus) {
    portia_learning_query_result_t result = portia_learning_query(learning_state, 9999);
    EXPECT_FALSE(result.found);
}

TEST_F(PortiaLearningTest, QueryExistingStimulus) {
    uint32_t stimulus_id = 600;
    portia_learning_habituate(learning_state, stimulus_id, 1000);

    portia_learning_query_result_t result = portia_learning_query(learning_state, stimulus_id);
    EXPECT_TRUE(result.found);
    EXPECT_GT(result.strength, 0.0f);
    EXPECT_GT(result.exposure_count, 0u);
}

TEST_F(PortiaLearningTest, QueryNullState) {
    portia_learning_query_result_t result = portia_learning_query(nullptr, 42);
    EXPECT_FALSE(result.found);
}

TEST_F(PortiaLearningTest, QueryAssociationNonexistent) {
    portia_learning_query_result_t result =
        portia_learning_query_association(learning_state, 9999, 8888);
    EXPECT_FALSE(result.found);
}

TEST_F(PortiaLearningTest, QueryAssociationExisting) {
    uint32_t stimulus_id = 700;
    uint32_t response_id = 800;

    portia_learning_associate(learning_state, stimulus_id, response_id, true, 1000);

    portia_learning_query_result_t result =
        portia_learning_query_association(learning_state, stimulus_id, response_id);

    EXPECT_TRUE(result.found);
    EXPECT_GT(result.strength, 0.0f);
}

// Forgetting Tests
TEST_F(PortiaLearningTest, ForgettingReducesStrength) {
    // Test forgetting on associations (associations weaken over time)
    uint32_t stimulus_id = 900;
    uint32_t response_id = 901;
    uint64_t timestamp = 1000;

    portia_learning_associate(learning_state, stimulus_id, response_id, true, timestamp);
    portia_learning_query_result_t result1 = portia_learning_query_association(learning_state, stimulus_id, response_id);

    // Apply forgetting - associations should weaken
    portia_learning_forget(learning_state, timestamp + 5000);
    portia_learning_query_result_t result2 = portia_learning_query_association(learning_state, stimulus_id, response_id);

    EXPECT_TRUE(result1.found);
    EXPECT_TRUE(result2.found);
    EXPECT_LT(result2.strength, result1.strength);
}

TEST_F(PortiaLearningTest, ForgettingMultipleEntries) {
    uint64_t timestamp = 1000;

    for (uint32_t i = 0; i < 5; i++) {
        portia_learning_habituate(learning_state, i, timestamp);
    }

    int result = portia_learning_forget(learning_state, timestamp + 10000);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaLearningTest, ForgettingNullState) {
    int result = portia_learning_forget(nullptr, 1000);
    EXPECT_NE(result, 0);
}

// Consolidation Tests
TEST_F(PortiaLearningTest, ConsolidationRemovesWeakEntries) {
    uint64_t timestamp = 1000;

    // Create multiple weak habituation entries
    for (uint32_t i = 0; i < 10; i++) {
        portia_learning_habituate(learning_state, i, timestamp);
        // Let them decay
        portia_learning_forget(learning_state, timestamp + 1000);
    }

    portia_learning_stats_t stats1 = portia_learning_get_stats(learning_state);

    // Consolidate
    portia_learning_consolidate(learning_state, timestamp + 2000);

    portia_learning_stats_t stats2 = portia_learning_get_stats(learning_state);

    // Should have fewer entries after consolidation
    EXPECT_LE(stats2.active_habituation_entries, stats1.active_habituation_entries);
}

TEST_F(PortiaLearningTest, ConsolidationStrengthenImportant) {
    uint32_t stimulus_id = 1000;
    uint64_t timestamp = 1000;

    // Create strong association through reinforcement
    portia_learning_associate(learning_state, stimulus_id, stimulus_id + 100, true, timestamp);
    for (int i = 0; i < 5; i++) {
        portia_learning_reinforce(learning_state, stimulus_id, stimulus_id + 100, 0.5f, timestamp);
        timestamp += 100;
    }

    portia_learning_query_result_t result1 =
        portia_learning_query_association(learning_state, stimulus_id, stimulus_id + 100);

    // Consolidate
    portia_learning_consolidate(learning_state, timestamp + config.consolidation_interval_ms);

    portia_learning_query_result_t result2 =
        portia_learning_query_association(learning_state, stimulus_id, stimulus_id + 100);

    EXPECT_TRUE(result1.found);
    EXPECT_TRUE(result2.found);
    // Strong associations should remain or strengthen
    EXPECT_GE(result2.strength, result1.strength * 0.9f);
}

TEST_F(PortiaLearningTest, ConsolidationNullState) {
    int result = portia_learning_consolidate(nullptr, 1000);
    EXPECT_NE(result, 0);
}

// Reset Tests
TEST_F(PortiaLearningTest, ResetClearsAllLearning) {
    uint64_t timestamp = 1000;

    // Add various learning entries
    for (uint32_t i = 0; i < 5; i++) {
        portia_learning_habituate(learning_state, i, timestamp);
        portia_learning_associate(learning_state, i, i + 100, true, timestamp);
    }

    portia_learning_stats_t stats1 = portia_learning_get_stats(learning_state);
    EXPECT_GT(stats1.active_habituation_entries, 0u);
    EXPECT_GT(stats1.active_association_entries, 0u);

    // Reset
    int result = portia_learning_reset(learning_state);
    EXPECT_EQ(result, 0);

    portia_learning_stats_t stats2 = portia_learning_get_stats(learning_state);
    EXPECT_EQ(stats2.active_habituation_entries, 0u);
    EXPECT_EQ(stats2.active_association_entries, 0u);
}

TEST_F(PortiaLearningTest, ResetNullState) {
    int result = portia_learning_reset(nullptr);
    EXPECT_NE(result, 0);
}

// Statistics Tests
TEST_F(PortiaLearningTest, StatisticsAccurateCount) {
    uint64_t timestamp = 1000;

    uint32_t expected_hab = 7;
    uint32_t expected_assoc = 5;

    for (uint32_t i = 0; i < expected_hab; i++) {
        portia_learning_habituate(learning_state, i, timestamp);
    }

    for (uint32_t i = 0; i < expected_assoc; i++) {
        portia_learning_associate(learning_state, i + 100, i + 200, true, timestamp);
    }

    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_EQ(stats.active_habituation_entries, expected_hab);
    EXPECT_EQ(stats.active_association_entries, expected_assoc);
}

TEST_F(PortiaLearningTest, StatisticsExposureCount) {
    uint32_t stimulus_id = 1100;
    uint64_t timestamp = 1000;

    portia_learning_stats_t stats1 = portia_learning_get_stats(learning_state);

    for (int i = 0; i < 10; i++) {
        portia_learning_habituate(learning_state, stimulus_id, timestamp);
        timestamp += 100;
    }

    portia_learning_stats_t stats2 = portia_learning_get_stats(learning_state);
    EXPECT_GT(stats2.total_exposures, stats1.total_exposures);
}

TEST_F(PortiaLearningTest, StatisticsReinforcementCount) {
    uint32_t stimulus_id = 1200;
    uint32_t response_id = 1300;
    uint64_t timestamp = 1000;

    portia_learning_associate(learning_state, stimulus_id, response_id, true, timestamp);

    portia_learning_stats_t stats1 = portia_learning_get_stats(learning_state);

    for (int i = 0; i < 5; i++) {
        portia_learning_reinforce(learning_state, stimulus_id, response_id, 0.5f, timestamp);
        timestamp += 100;
    }

    portia_learning_stats_t stats2 = portia_learning_get_stats(learning_state);
    EXPECT_GT(stats2.total_reinforcements, stats1.total_reinforcements);
}

TEST_F(PortiaLearningTest, StatisticsNullState) {
    portia_learning_stats_t stats = portia_learning_get_stats(nullptr);
    // Should return zeroed stats
    EXPECT_EQ(stats.active_habituation_entries, 0u);
    EXPECT_EQ(stats.active_association_entries, 0u);
}

// Export Tests
TEST_F(PortiaLearningTest, ExportWithData) {
    uint64_t timestamp = 1000;

    for (uint32_t i = 0; i < 5; i++) {
        portia_learning_habituate(learning_state, i, timestamp);
        portia_learning_associate(learning_state, i, i + 100, true, timestamp);
    }

    int result = portia_learning_export(learning_state, "/tmp/learning_export_test.txt");
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaLearningTest, ExportNullState) {
    int result = portia_learning_export(nullptr, "/tmp/test.txt");
    EXPECT_NE(result, 0);
}

TEST_F(PortiaLearningTest, ExportNullPath) {
    int result = portia_learning_export(learning_state, nullptr);
    EXPECT_NE(result, 0);
}

// Bio-Async Tests
TEST_F(PortiaLearningTest, ProcessInbox) {
    int result = portia_learning_process_inbox(learning_state);
    // Should succeed or return appropriate error
    EXPECT_GE(result, -1); // -1 for no inbox, 0+ for success
}

TEST_F(PortiaLearningTest, ProcessInboxNullState) {
    int result = portia_learning_process_inbox(nullptr);
    EXPECT_NE(result, 0);
}

// Thread Safety Tests
TEST_F(PortiaLearningTest, ConcurrentHabituation) {
    auto habituate_worker = [this](uint32_t start_id, uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            portia_learning_habituate(learning_state, start_id + i, 1000 + i * 100);
        }
    };

    std::thread t1(habituate_worker, 0, 10);
    std::thread t2(habituate_worker, 100, 10);

    t1.join();
    t2.join();

    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_GE(stats.active_habituation_entries, 10u);
}

TEST_F(PortiaLearningTest, ConcurrentAssociation) {
    auto associate_worker = [this](uint32_t start_id, uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            portia_learning_associate(learning_state, start_id + i,
                                     start_id + i + 1000, true, 1000);
        }
    };

    std::thread t1(associate_worker, 0, 10);
    std::thread t2(associate_worker, 500, 10);

    t1.join();
    t2.join();

    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_GE(stats.active_association_entries, 10u);
}

// Edge Cases
TEST_F(PortiaLearningTest, ZeroCapacityConfig) {
    portia_learning_config_t zero_config = config;
    zero_config.max_habituation_entries = 0;
    zero_config.max_association_entries = 0;

    portia_learning_state_t* state = portia_learning_init(&zero_config);
    EXPECT_NE(state, nullptr); // Should handle gracefully
    portia_learning_destroy(state);
}

TEST_F(PortiaLearningTest, VeryLargeLearningRate) {
    portia_learning_config_t large_config = config;
    large_config.default_learning_rate = 10.0f;

    portia_learning_state_t* state = portia_learning_init(&large_config);
    EXPECT_NE(state, nullptr);

    // Learning should be clamped appropriately
    portia_learning_associate(state, 1, 2, true, 1000);
    portia_learning_query_result_t result = portia_learning_query_association(state, 1, 2);
    EXPECT_TRUE(result.found);
    EXPECT_LE(result.strength, 1.0f);

    portia_learning_destroy(state);
}

TEST_F(PortiaLearningTest, NegativeLearningRate) {
    portia_learning_config_t neg_config = config;
    neg_config.default_learning_rate = -0.5f;

    portia_learning_state_t* state = portia_learning_init(&neg_config);
    EXPECT_NE(state, nullptr); // Should handle gracefully
    portia_learning_destroy(state);
}

TEST_F(PortiaLearningTest, VeryHighForgettingRate) {
    portia_learning_config_t forget_config = config;
    forget_config.default_forgetting_rate = 0.99f;

    portia_learning_state_t* state = portia_learning_init(&forget_config);
    EXPECT_NE(state, nullptr);

    // Test with associations which do decay with high forgetting rate
    portia_learning_associate(state, 1, 2, true, 1000);
    portia_learning_query_result_t result1 = portia_learning_query_association(state, 1, 2);

    // Apply forgetting - with 0.99 forgetting rate, associations should nearly vanish
    portia_learning_forget(state, 2000);

    portia_learning_query_result_t result2 = portia_learning_query_association(state, 1, 2);
    EXPECT_TRUE(result1.found);
    // With very high forgetting rate, association should be significantly weakened or removed
    // (it may be deactivated if below threshold, so check for both cases)
    if (result2.found) {
        EXPECT_LT(result2.strength, result1.strength);
    }

    portia_learning_destroy(state);
}

TEST_F(PortiaLearningTest, StimulusIdBoundaries) {
    uint32_t max_id = UINT32_MAX;
    uint32_t min_id = 0;

    EXPECT_EQ(portia_learning_habituate(learning_state, max_id, 1000), 0);
    EXPECT_EQ(portia_learning_habituate(learning_state, min_id, 1000), 0);

    portia_learning_query_result_t result1 = portia_learning_query(learning_state, max_id);
    portia_learning_query_result_t result2 = portia_learning_query(learning_state, min_id);

    EXPECT_TRUE(result1.found);
    EXPECT_TRUE(result2.found);
}

TEST_F(PortiaLearningTest, TimestampOrdering) {
    uint32_t stimulus_id = 2000;

    // Add with increasing timestamps
    portia_learning_habituate(learning_state, stimulus_id, 1000);
    portia_learning_habituate(learning_state, stimulus_id, 2000);
    portia_learning_habituate(learning_state, stimulus_id, 3000);

    portia_learning_query_result_t result = portia_learning_query(learning_state, stimulus_id);
    EXPECT_TRUE(result.found);
    EXPECT_EQ(result.last_update_ms, 3000u);
}

TEST_F(PortiaLearningTest, TimestampBackwardsInTime) {
    uint32_t stimulus_id = 2100;

    // Add with timestamp going backwards (should handle gracefully)
    portia_learning_habituate(learning_state, stimulus_id, 3000);
    portia_learning_habituate(learning_state, stimulus_id, 2000);
    portia_learning_habituate(learning_state, stimulus_id, 1000);

    portia_learning_query_result_t result = portia_learning_query(learning_state, stimulus_id);
    EXPECT_TRUE(result.found);
}

// Integration Tests
TEST_F(PortiaLearningTest, CompleteWorkflow) {
    uint32_t stimulus_id = 3000;
    uint32_t response_id = 4000;
    uint64_t timestamp = 1000;

    // 1. Habituate to stimulus
    for (int i = 0; i < 3; i++) {
        portia_learning_habituate(learning_state, stimulus_id, timestamp);
        timestamp += 100;
    }

    // 2. Create association
    portia_learning_associate(learning_state, stimulus_id, response_id, true, timestamp);
    timestamp += 100;

    // 3. Reinforce association
    for (int i = 0; i < 3; i++) {
        portia_learning_reinforce(learning_state, stimulus_id, response_id, 0.5f, timestamp);
        timestamp += 100;
    }

    // 4. Query both
    portia_learning_query_result_t hab_result = portia_learning_query(learning_state, stimulus_id);
    portia_learning_query_result_t assoc_result =
        portia_learning_query_association(learning_state, stimulus_id, response_id);

    EXPECT_TRUE(hab_result.found);
    EXPECT_TRUE(assoc_result.found);
    EXPECT_GT(assoc_result.strength, 0.5f);

    // 5. Apply forgetting
    portia_learning_forget(learning_state, timestamp + 5000);

    // 6. Consolidate
    portia_learning_consolidate(learning_state, timestamp + 6000);

    // 7. Verify strong association survives
    assoc_result = portia_learning_query_association(learning_state, stimulus_id, response_id);
    EXPECT_TRUE(assoc_result.found);
}

TEST_F(PortiaLearningTest, MultiModalLearning) {
    uint64_t timestamp = 1000;

    // Enable all learning modes
    portia_learning_set_mode(learning_state, LEARNING_MODE_FULL);

    // Habituate to multiple stimuli
    for (uint32_t i = 0; i < 5; i++) {
        portia_learning_habituate(learning_state, i, timestamp);
    }

    // Create associations
    for (uint32_t i = 0; i < 5; i++) {
        portia_learning_associate(learning_state, i, i + 100, true, timestamp);
    }

    // Sensitize important stimulus
    portia_learning_sensitize(learning_state, 2, 0.5f, timestamp);

    // Verify statistics
    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_EQ(stats.active_habituation_entries, 5u);
    EXPECT_EQ(stats.active_association_entries, 5u);
}
