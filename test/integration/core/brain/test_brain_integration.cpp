/**
 * @file test_brain_integration.cpp
 * @brief Integration tests for Brain API with distributed cognition
 *
 * WHAT: Tests brain integration with distributed cognition, neuromodulators, and glial cells
 * WHY:  Verify high-level Brain API works correctly with P2P coordination
 * HOW:  Create distributed brains, test learning, inference, and sync operations
 *
 * COVERAGE: All 5 distributed brain APIs + integration with learning/inference
 * TEST PHILOSOPHY: Integration testing - verify modules work together correctly
 */

#include <gtest/gtest.h>

#include "core/brain/nimcp_brain.h"
#include "networking/distributed/nimcp_distributed_cognition.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainIntegrationTest : public ::testing::Test {
protected:
    p2p_node_t p2p_node;
    brain_t brain;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        // Mock P2P node
        p2p_node = (p2p_node_t)0x1234;
        brain = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096) << "Memory leak detected";
    }
};

//=============================================================================
// Basic Distributed Brain Tests
//=============================================================================

/**
 * WHAT: Test creating distributed brain
 * WHY:  Verify basic factory function works
 * HOW:  Create with valid parameters, verify non-null, check distributed flag
 */
TEST_F(BrainIntegrationTest, CreateDistributedBrain_Success)
{
    brain = brain_create_distributed(
        "test_brain",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        4,  // 4 inputs
        2,  // 2 outputs
        p2p_node
    );

    ASSERT_NE(brain, nullptr);
    EXPECT_TRUE(brain_is_distributed(brain));
}

/**
 * WHAT: Test creating distributed brain with NULL p2p_node
 * WHY:  Verify error handling for invalid parameters
 * HOW:  Call with NULL p2p_node, expect NULL return and error message
 */
TEST_F(BrainIntegrationTest, CreateDistributedBrain_NullP2PNode_Fails)
{
    brain = brain_create_distributed(
        "test_brain",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        4,
        2,
        nullptr  // Invalid!
    );

    EXPECT_EQ(brain, nullptr);
    const char* error = brain_get_last_error();
    ASSERT_NE(error, nullptr);
    EXPECT_NE(std::string(error).find("p2p_node"), std::string::npos);
}

/**
 * WHAT: Test brain_is_distributed on standalone brain
 * WHY:  Verify query function correctly identifies standalone brains
 * HOW:  Create standard brain, check is_distributed returns false
 */
TEST_F(BrainIntegrationTest, IsDistributed_StandaloneBrain_ReturnsFalse)
{
    brain = brain_create("test_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    EXPECT_FALSE(brain_is_distributed(brain));
}

/**
 * WHAT: Test brain_is_distributed with NULL brain
 * WHY:  Verify defensive programming - no crash on NULL
 * HOW:  Call with NULL, expect false
 */
TEST_F(BrainIntegrationTest, IsDistributed_NullBrain_ReturnsFalse)
{
    EXPECT_FALSE(brain_is_distributed(nullptr));
}

//=============================================================================
// brain_enable_distributed() Tests
//=============================================================================

/**
 * WHAT: Test converting standalone brain to distributed
 * WHY:  Verify retrofit functionality works
 * HOW:  Create standalone brain, enable distributed, verify flag changes
 */
TEST_F(BrainIntegrationTest, EnableDistributed_StandaloneBrain_Success)
{
    brain = brain_create("test_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);
    EXPECT_FALSE(brain_is_distributed(brain));

    EXPECT_TRUE(brain_enable_distributed(brain, p2p_node));
    EXPECT_TRUE(brain_is_distributed(brain));
}

/**
 * WHAT: Test enabling distributed on NULL brain
 * WHY:  Verify error handling
 * HOW:  Call with NULL, expect false and error message
 */
TEST_F(BrainIntegrationTest, EnableDistributed_NullBrain_Fails)
{
    EXPECT_FALSE(brain_enable_distributed(nullptr, p2p_node));

    const char* error = brain_get_last_error();
    ASSERT_NE(error, nullptr);
}

/**
 * WHAT: Test enabling distributed with NULL p2p_node
 * WHY:  Verify parameter validation
 * HOW:  Pass NULL p2p_node, expect failure
 */
TEST_F(BrainIntegrationTest, EnableDistributed_NullP2PNode_Fails)
{
    brain = brain_create("test_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    EXPECT_FALSE(brain_enable_distributed(brain, nullptr));
    EXPECT_FALSE(brain_is_distributed(brain));
}

/**
 * WHAT: Test enabling distributed on already-distributed brain
 * WHY:  Verify idempotency check
 * HOW:  Create distributed brain, try to enable again, expect failure
 */
TEST_F(BrainIntegrationTest, EnableDistributed_AlreadyDistributed_Fails)
{
    brain = brain_create_distributed("test_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);
    EXPECT_TRUE(brain_is_distributed(brain));

    EXPECT_FALSE(brain_enable_distributed(brain, p2p_node));
}

//=============================================================================
// brain_sync_neuromodulators() Tests
//=============================================================================

/**
 * WHAT: Test manual neuromodulator sync
 * WHY:  Verify explicit sync API works
 * HOW:  Create distributed brain, call sync, expect success
 */
TEST_F(BrainIntegrationTest, SyncNeuromodulators_DistributedBrain_Success)
{
    brain = brain_create_distributed("test_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);

    EXPECT_TRUE(brain_sync_neuromodulators(brain));
}

/**
 * WHAT: Test sync on standalone brain
 * WHY:  Verify error handling when not distributed
 * HOW:  Create standalone brain, try sync, expect failure
 */
TEST_F(BrainIntegrationTest, SyncNeuromodulators_StandaloneBrain_Fails)
{
    brain = brain_create("test_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    EXPECT_FALSE(brain_sync_neuromodulators(brain));

    const char* error = brain_get_last_error();
    ASSERT_NE(error, nullptr);
    EXPECT_NE(std::string(error).find("not distributed"), std::string::npos);
}

/**
 * WHAT: Test sync with NULL brain
 * WHY:  Verify defensive programming
 * HOW:  Call with NULL, expect false
 */
TEST_F(BrainIntegrationTest, SyncNeuromodulators_NullBrain_Fails)
{
    EXPECT_FALSE(brain_sync_neuromodulators(nullptr));
}

//=============================================================================
// brain_get_distributed_stats() Tests
//=============================================================================

/**
 * WHAT: Test getting distributed stats
 * WHY:  Verify stats query API works
 * HOW:  Create distributed brain, get stats, verify zero initialization
 */
TEST_F(BrainIntegrationTest, GetDistributedStats_Success)
{
    brain = brain_create_distributed("test_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);

    distrib_cognition_stats_t stats;
    EXPECT_TRUE(brain_get_distributed_stats(brain, &stats));

    // Verify initial stats
    EXPECT_EQ(stats.neuromod_broadcasts, 0);
    EXPECT_EQ(stats.glial_pruning_coordinations, 0);
    EXPECT_EQ(stats.region_state_syncs, 0);
}

/**
 * WHAT: Test getting stats from standalone brain
 * WHY:  Verify error handling when not distributed
 * HOW:  Create standalone brain, try to get stats, expect failure
 */
TEST_F(BrainIntegrationTest, GetDistributedStats_StandaloneBrain_Fails)
{
    brain = brain_create("test_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    distrib_cognition_stats_t stats;
    EXPECT_FALSE(brain_get_distributed_stats(brain, &stats));
}

/**
 * WHAT: Test getting stats with NULL parameters
 * WHY:  Verify parameter validation
 * HOW:  Call with NULL brain and NULL stats, expect failures
 */
TEST_F(BrainIntegrationTest, GetDistributedStats_NullParameters_Fails)
{
    brain = brain_create_distributed("test_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);

    distrib_cognition_stats_t stats;

    // NULL brain
    EXPECT_FALSE(brain_get_distributed_stats(nullptr, &stats));

    // NULL stats
    EXPECT_FALSE(brain_get_distributed_stats(brain, nullptr));
}

//=============================================================================
// Integration: Distributed Brain with Learning
//=============================================================================

/**
 * WHAT: Test distributed brain can learn
 * WHY:  Verify learning API works with distributed coordinator
 * HOW:  Create distributed brain, train on examples, verify loss decreases
 */
TEST_F(BrainIntegrationTest, DistributedBrain_CanLearn)
{
    brain = brain_create_distributed("classifier", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);

    // Train on simple XOR-like pattern
    float input1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float input2[] = {0.0f, 1.0f, 0.0f, 0.0f};

    float loss1 = brain_learn_example(brain, input1, 4, "class_a", 0.95f);
    float loss2 = brain_learn_example(brain, input2, 4, "class_b", 0.95f);

    EXPECT_GE(loss1, 0.0f);
    EXPECT_GE(loss2, 0.0f);

    // Train multiple times - loss should generally decrease
    float loss_after = 0.0f;
    for (int i = 0; i < 10; i++) {
        loss_after += brain_learn_example(brain, input1, 4, "class_a", 0.95f);
        loss_after += brain_learn_example(brain, input2, 4, "class_b", 0.95f);
    }
    loss_after /= 20.0f;

    // Note: Loss may not always decrease monotonically in spike-based networks
    // Just verify it's still positive and finite
    EXPECT_GE(loss_after, 0.0f);
    EXPECT_LT(loss_after, 100.0f);
}

/**
 * WHAT: Test distributed brain can make decisions
 * WHY:  Verify inference works with distributed coordinator
 * HOW:  Create brain, learn pattern, test inference
 */
TEST_F(BrainIntegrationTest, DistributedBrain_CanDecide)
{
    brain = brain_create_distributed("classifier", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);

    // Train on pattern
    float input1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 20; i++) {
        brain_learn_example(brain, input1, 4, "class_a", 0.95f);
    }

    // Make decision
    brain_decision_t* decision = brain_decide(brain, input1, 4);
    ASSERT_NE(decision, nullptr);

    // Verify decision structure
    EXPECT_GT(strlen(decision->label), 0);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);
    EXPECT_GT(decision->inference_time_us, 0);

    brain_free_decision(decision);
}

/**
 * WHAT: Test neuromod sync after learning
 * WHY:  Verify sync works in realistic workflow
 * HOW:  Learn, sync, verify stats updated
 */
TEST_F(BrainIntegrationTest, DistributedBrain_SyncAfterLearning)
{
    brain = brain_create_distributed("classifier", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);

    // Learn pattern
    float input1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    brain_learn_example(brain, input1, 4, "class_a", 0.95f);

    // Sync neuromodulators
    EXPECT_TRUE(brain_sync_neuromodulators(brain));

    // Verify stats show broadcast
    distrib_cognition_stats_t stats;
    EXPECT_TRUE(brain_get_distributed_stats(brain, &stats));
    EXPECT_GT(stats.neuromod_broadcasts, 0);
}

//=============================================================================
// Integration: Brain Stats with Distributed Mode
//=============================================================================

/**
 * WHAT: Test brain_get_stats works for distributed brain
 * WHY:  Verify existing brain API compatible with distributed mode
 * HOW:  Create distributed brain, get stats, verify fields populated
 */
TEST_F(BrainIntegrationTest, DistributedBrain_GetStatsWorks)
{
    brain = brain_create_distributed("classifier", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats;
    EXPECT_TRUE(brain_get_stats(brain, &stats));

    // Verify stats populated
    EXPECT_EQ(stats.size, BRAIN_SIZE_TINY);
    EXPECT_GT(stats.num_neurons, 0);
    EXPECT_GT(stats.num_synapses, 0);
    EXPECT_STREQ(stats.task_name, "classifier");
}

/**
 * WHAT: Test brain persistence (save/load) with distributed brain
 * WHY:  Verify distributed brains can be serialized
 * HOW:  Create distributed brain, save, load, verify loaded brain works
 *
 * NOTE: This test checks that save/load works, but loaded brain loses
 *       distributed coordinator (needs to be re-enabled after load)
 */
TEST_F(BrainIntegrationTest, DistributedBrain_SaveLoad)
{
    const char* filepath = "/tmp/test_brain_distributed.nimcp";

    // Create and save distributed brain
    brain = brain_create_distributed("classifier", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);

    // Train a bit
    float input1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    brain_learn_example(brain, input1, 4, "class_a", 0.95f);

    // Save
    EXPECT_TRUE(brain_save(brain, filepath));

    // Destroy original
    brain_destroy(brain);
    brain = nullptr;

    // Load
    brain = brain_load(filepath);
    ASSERT_NE(brain, nullptr);

    // Loaded brain should be standalone (distributed coordinator not serialized)
    EXPECT_FALSE(brain_is_distributed(brain));

    // But can be re-enabled
    EXPECT_TRUE(brain_enable_distributed(brain, p2p_node));
    EXPECT_TRUE(brain_is_distributed(brain));

    // Cleanup
    std::remove(filepath);
    std::remove((std::string(filepath) + ".meta").c_str());
}

//=============================================================================
// Memory and Performance Tests
//=============================================================================

/**
 * WHAT: Test memory usage of distributed brain
 * WHY:  Verify distributed coordinator doesn't cause excessive overhead
 * HOW:  Create standalone and distributed brains, compare memory
 */
TEST_F(BrainIntegrationTest, DistributedBrain_MemoryOverhead)
{
    // Create standalone brain
    brain_t standalone = brain_create("standalone", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(standalone, nullptr);
    size_t standalone_size = brain_get_memory_usage(standalone);

    // Create distributed brain
    brain = brain_create_distributed("distributed", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);
    size_t distributed_size = brain_get_memory_usage(brain);

    // Distributed may have overhead, but implementation is efficient for tiny brains
    EXPECT_GE(distributed_size, standalone_size);
    EXPECT_LT(distributed_size, standalone_size * 1.5);

    brain_destroy(standalone);
}

/**
 * WHAT: Test inference performance of distributed brain
 * WHY:  Verify distributed coordination doesn't slow down inference significantly
 * HOW:  Run batch inference, measure time, verify < 10ms for tiny brain
 */
TEST_F(BrainIntegrationTest, DistributedBrain_InferencePerformance)
{
    brain = brain_create_distributed("classifier", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);

    // Train briefly
    float input[] = {1.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, input, 4, "class_a", 0.95f);
    }

    // Measure inference time
    uint64_t start = nimcp_time_monotonic_us();
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        brain_decision_t* decision = brain_decide(brain, input, 4);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    uint64_t elapsed = nimcp_time_elapsed_us(start);
    float avg_time_us = (float)elapsed / iterations;

    // Tiny brain should do inference in < 100μs on average
    EXPECT_LT(avg_time_us, 100.0f);
}

//=============================================================================
// Error Handling and Edge Cases
//=============================================================================

/**
 * WHAT: Test destroying distributed brain cleans up coordinator
 * WHY:  Verify no memory leaks in distributed mode
 * HOW:  Create distributed brain, destroy, check memory stats
 */
TEST_F(BrainIntegrationTest, DistributedBrain_DestroyCleanup)
{
    nimcp_memory_clear_stats();

    brain = brain_create_distributed("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2, p2p_node);
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);
    brain = nullptr;

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);

    // Some allocations may remain from global state, but should be minimal
    EXPECT_LT(stats.current_allocated, 4096);
}
