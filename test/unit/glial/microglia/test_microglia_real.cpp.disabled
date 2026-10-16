/**
 * @file test_microglia_real.cpp
 * @brief Real tests for microglia glial cells
 *
 * COVERAGE TARGET: microglia module (currently 0%)
 * APPROACH: Test all real functions with actual instances
 * FOCUS: Synapse monitoring, activity tracking, weak synapse identification, pruning
 */

#include <gtest/gtest.h>

#include "glial/microglia/nimcp_microglia.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MicrogliaRealTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
    }

    void TearDown() override {
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0) << "Memory leak detected";
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(MicrogliaRealTest, CreateDestroy) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);

    ASSERT_NE(mg, nullptr);
    EXPECT_EQ(mg->id, 0);
    EXPECT_FLOAT_EQ(mg->position[0], 0.0f);
    EXPECT_FLOAT_EQ(mg->position[1], 0.0f);
    EXPECT_FLOAT_EQ(mg->position[2], 0.0f);
    EXPECT_FLOAT_EQ(mg->surveillance_radius, 100.0f);
    EXPECT_EQ(mg->num_monitored_synapses, 0);
    EXPECT_EQ(mg->total_synapses_pruned, 0);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, CreateWithDifferentParameters) {
    microglia_t* mg = microglia_create(42, 10.0f, 20.0f, 30.0f, 150.0f);

    ASSERT_NE(mg, nullptr);
    EXPECT_EQ(mg->id, 42);
    EXPECT_FLOAT_EQ(mg->position[0], 10.0f);
    EXPECT_FLOAT_EQ(mg->position[1], 20.0f);
    EXPECT_FLOAT_EQ(mg->position[2], 30.0f);
    EXPECT_FLOAT_EQ(mg->surveillance_radius, 150.0f);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, DestroyNull) {
    // Should handle null gracefully
    microglia_destroy(nullptr);
    SUCCEED();
}

TEST_F(MicrogliaRealTest, MultipleCreateDestroy) {
    const int count = 50;
    microglia_t* microglia[count];

    for (int i = 0; i < count; i++) {
        microglia[i] = microglia_create(i, i * 10.0f, i * 20.0f, i * 30.0f, 100.0f);
        ASSERT_NE(microglia[i], nullptr);
    }

    for (int i = 0; i < count; i++) {
        microglia_destroy(microglia[i]);
    }
}

//=============================================================================
// Synapse Monitoring Tests
//=============================================================================

TEST_F(MicrogliaRealTest, MonitorSynapse_Single) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    nimcp_result_t result = microglia_monitor_synapse(mg, 10002);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(mg->num_monitored_synapses, 1);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, MonitorSynapse_Multiple) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    // Monitor multiple synapses
    for (uint32_t i = 1; i <= 10; i++) {
        nimcp_result_t result = microglia_monitor_synapse(mg, 10000 + i);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(mg->num_monitored_synapses, 10);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, MonitorSynapse_Duplicate) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    microglia_monitor_synapse(mg, 10002);
    uint32_t count_before = mg->num_monitored_synapses;

    // Monitor same synapse again (should be idempotent)
    microglia_monitor_synapse(mg, 10002);
    uint32_t count_after = mg->num_monitored_synapses;

    // Count should not increase (idempotent behavior)
    EXPECT_EQ(count_before, count_after);

    microglia_destroy(mg);
}

//=============================================================================
// Activity Tracking Tests
//=============================================================================

TEST_F(MicrogliaRealTest, TrackSynapseActivity_SingleUpdate) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    microglia_monitor_synapse(mg, 10002);

    // Track activity
    microglia_track_synapse_activity(mg, 10002, 1.0f, 1000);

    // Activity score should be updated
    float score = microglia_get_synapse_activity_score(mg, 10002);
    EXPECT_GT(score, 0.0f);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, TrackSynapseActivity_MultipleUpdates) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    microglia_monitor_synapse(mg, 10002);

    // Track multiple activity events
    for (int i = 0; i < 10; i++) {
        microglia_track_synapse_activity(mg, 10002, 1.0f, 1000 + i * 1000);
    }

    float score = microglia_get_synapse_activity_score(mg, 10002);
    EXPECT_GT(score, 0.0f);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, TrackSynapseActivity_UnknownSynapse) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    // Track activity for synapse not monitored (should not crash)
    microglia_track_synapse_activity(mg, 99999, 1.0f, 1000);

    SUCCEED();

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, GetSynapseActivityScore_NotMonitored) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    // Get activity score for synapse not monitored
    float score = microglia_get_synapse_activity_score(mg, 99999);

    // Should return 0.0
    EXPECT_FLOAT_EQ(score, 0.0f);

    microglia_destroy(mg);
}

//=============================================================================
// Activity Score Update Tests
//=============================================================================

TEST_F(MicrogliaRealTest, UpdateActivityScores) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    microglia_monitor_synapse(mg, 10002);
    microglia_track_synapse_activity(mg, 10002, 1.0f, 1000);

    // Update activity scores (decay over time)
    microglia_update_activity_scores(mg, 2000);
    microglia_update_activity_scores(mg, 3000);

    SUCCEED();

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, UpdateActivityScores_MultipleTimesteps) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    microglia_monitor_synapse(mg, 10002);
    microglia_track_synapse_activity(mg, 10002, 1.0f, 1000);

    // Update over many timesteps
    for (int i = 0; i < 100; i++) {
        microglia_update_activity_scores(mg, 1000 + i * 100);
    }

    SUCCEED();

    microglia_destroy(mg);
}

//=============================================================================
// Weak Synapse Identification Tests
//=============================================================================

TEST_F(MicrogliaRealTest, IdentifyWeakSynapses_None) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    microglia_monitor_synapse(mg, 10002);
    microglia_track_synapse_activity(mg, 10002, 1.0f, 1000);

    uint32_t weak_ids[10];
    uint32_t count = microglia_identify_weak_synapses(mg, weak_ids, 10);

    // Active synapse should not be identified as weak
    EXPECT_EQ(count, 0);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, IdentifyWeakSynapses_Some) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    // Monitor some synapses but don't track activity (they remain weak)
    microglia_monitor_synapse(mg, 10002);
    microglia_monitor_synapse(mg, 10003);
    microglia_monitor_synapse(mg, 10004);

    uint32_t weak_ids[10];
    uint32_t count = microglia_identify_weak_synapses(mg, weak_ids, 10);

    // All synapses with no activity should be weak
    EXPECT_GT(count, 0);
    EXPECT_LE(count, 3);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, IdentifyWeakSynapses_LimitedBuffer) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    // Monitor many weak synapses
    for (uint32_t i = 1; i <= 20; i++) {
        microglia_monitor_synapse(mg, 10000 + i);
    }

    // Buffer can only hold 5
    uint32_t weak_ids[5];
    uint32_t count = microglia_identify_weak_synapses(mg, weak_ids, 5);

    // Should return at most 5
    EXPECT_LE(count, 5);

    microglia_destroy(mg);
}

//=============================================================================
// Pruning Tests
//=============================================================================

TEST_F(MicrogliaRealTest, PruneWeakSynapses_None) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    microglia_monitor_synapse(mg, 10002);
    microglia_track_synapse_activity(mg, 10002, 1.0f, 1000);

    uint32_t pruned = microglia_prune_weak_synapses(mg);

    // Active synapse should not be pruned
    EXPECT_EQ(pruned, 0);
    EXPECT_EQ(mg->total_synapses_pruned, 0);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, PruneWeakSynapses_Some) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    // Monitor weak synapses
    microglia_monitor_synapse(mg, 10002);
    microglia_monitor_synapse(mg, 10003);

    uint32_t before = mg->num_monitored_synapses;
    uint32_t pruned = microglia_prune_weak_synapses(mg);
    uint32_t after = mg->num_monitored_synapses;

    // Some synapses should be pruned
    EXPECT_GT(pruned, 0);
    EXPECT_LT(after, before);
    EXPECT_EQ(mg->total_synapses_pruned, pruned);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, GetTotalPruned) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    EXPECT_EQ(microglia_get_total_pruned(mg), 0);

    // Monitor and prune
    for (uint32_t i = 1; i <= 10; i++) {
        microglia_monitor_synapse(mg, 10000 + i);
    }

    microglia_prune_weak_synapses(mg);
    uint32_t total = microglia_get_total_pruned(mg);

    EXPECT_GT(total, 0);

    microglia_destroy(mg);
}

//=============================================================================
// Network Management Tests
//=============================================================================

TEST_F(MicrogliaRealTest, NetworkCreate) {
    microglia_network_t* network = microglia_network_create(10);

    ASSERT_NE(network, nullptr);
    EXPECT_EQ(network->num_microglia, 0);
    EXPECT_EQ(network->capacity, 10);

    microglia_network_destroy(network);
}

TEST_F(MicrogliaRealTest, NetworkAdd) {
    microglia_network_t* network = microglia_network_create(10);
    ASSERT_NE(network, nullptr);

    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    nimcp_result_t result = microglia_network_add(network, mg);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(network->num_microglia, 1);

    microglia_network_destroy(network);
}

TEST_F(MicrogliaRealTest, NetworkAddMultiple) {
    microglia_network_t* network = microglia_network_create(10);
    ASSERT_NE(network, nullptr);

    for (int i = 0; i < 5; i++) {
        microglia_t* mg = microglia_create(i, 0.0f, 0.0f, 0.0f, 100.0f);
        microglia_network_add(network, mg);
    }

    EXPECT_EQ(network->num_microglia, 5);

    microglia_network_destroy(network);
}

TEST_F(MicrogliaRealTest, NetworkStep) {
    microglia_network_t* network = microglia_network_create(10);
    ASSERT_NE(network, nullptr);

    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    microglia_network_add(network, mg);

    // Step network
    microglia_network_step(network, 1000);
    microglia_network_step(network, 2000);
    microglia_network_step(network, 3000);

    SUCCEED();

    microglia_network_destroy(network);
}

TEST_F(MicrogliaRealTest, NetworkFindBySynapse_NotFound) {
    microglia_network_t* network = microglia_network_create(10);
    ASSERT_NE(network, nullptr);

    microglia_t* found = microglia_network_find_by_synapse(network, 99999);
    EXPECT_EQ(found, nullptr);

    microglia_network_destroy(network);
}

TEST_F(MicrogliaRealTest, NetworkFindBySynapse_Found) {
    microglia_network_t* network = microglia_network_create(10);
    ASSERT_NE(network, nullptr);

    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    microglia_monitor_synapse(mg, 10002);
    microglia_network_add(network, mg);

    microglia_t* found = microglia_network_find_by_synapse(network, 10002);
    EXPECT_EQ(found, mg);

    microglia_network_destroy(network);
}

//=============================================================================
// Threshold and Configuration Tests
//=============================================================================

TEST_F(MicrogliaRealTest, PruningThreshold_Default) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    // Default pruning threshold should be set
    EXPECT_GT(mg->pruning_threshold, 0.0f);
    EXPECT_LT(mg->pruning_threshold, 1.0f);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, PruningRate_Default) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    // Default pruning rate should be set
    EXPECT_GT(mg->pruning_rate, 0.0f);

    microglia_destroy(mg);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(MicrogliaRealTest, MonitorManySymapses) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    // Monitor many synapses (up to capacity)
    for (uint32_t i = 1; i <= 100; i++) {
        microglia_monitor_synapse(mg, 10000 + i);
    }

    EXPECT_GE(mg->num_monitored_synapses, 10);

    microglia_destroy(mg);
}

TEST_F(MicrogliaRealTest, ActivityTrackingOverTime) {
    microglia_t* mg = microglia_create(0, 0.0f, 0.0f, 0.0f, 100.0f);
    ASSERT_NE(mg, nullptr);

    microglia_monitor_synapse(mg, 10002);

    // Track activity with varying levels
    for (int i = 0; i < 100; i++) {
        float activity = (i % 2 == 0) ? 1.0f : 0.0f;
        microglia_track_synapse_activity(mg, 10002, activity, 1000 + i * 100);
    }

    // Update scores
    microglia_update_activity_scores(mg, 11000);

    float score = microglia_get_synapse_activity_score(mg, 10002);
    EXPECT_GE(score, 0.0f);

    microglia_destroy(mg);
}
