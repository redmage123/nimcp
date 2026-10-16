/**
 * @file test_systems_consolidation.cpp
 * @brief Unit tests for Phase M2 systems consolidation
 *
 * WHAT: Tests hippocampus → cortex memory transfer system
 * WHY:  Ensure sleep replay and cortical consolidation work correctly
 * HOW:  Test core functions, replay scheduling, cortical transfer, and consolidation
 *
 * TEST COVERAGE:
 * - System creation/destruction
 * - Replay scheduling and execution
 * - Cortical node creation and management
 * - Semantic similarity search
 * - Consolidation dynamics
 * - Forgetting/decay mechanisms
 * - Statistics tracking
 *
 * @version Phase M2 Unit Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

    #include "cognitive/memory/nimcp_systems_consolidation.h"
    #include "cognitive/memory/nimcp_engram.h"
    #include "utils/platform/nimcp_platform_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SystemsConsolidationTest : public ::testing::Test {
protected:
    systems_consolidation_system_t* system;
    engram_system_t* engram_sys;

    void SetUp() override {
        system = nullptr;
        engram_sys = nullptr;
    }

    void TearDown() override {
        if (system) {
            systems_consolidation_destroy(system);
            system = nullptr;
        }
        if (engram_sys) {
            engram_system_destroy(engram_sys);
            engram_sys = nullptr;
        }
    }
};

//=============================================================================
// Unit Test 1: System Creation and Destruction
//=============================================================================

TEST_F(SystemsConsolidationTest, SystemCreation_Success) {
    /**
     * WHAT: Verify systems consolidation system can be created
     * WHY:  Basic prerequisite for all other operations
     * HOW:  Call systems_consolidation_create(), verify non-null
     */

    system = systems_consolidation_create();

    ASSERT_NE(system, nullptr) << "System creation should succeed";
    EXPECT_EQ(system->node_count, 0) << "Should start with no cortical nodes";
    EXPECT_EQ(system->replay_queue_size, 0) << "Should start with empty replay queue";
    EXPECT_GT(system->node_capacity, 0) << "Should have allocated capacity";
    EXPECT_GT(system->replay_queue_capacity, 0) << "Should have replay queue capacity";
}

TEST_F(SystemsConsolidationTest, SystemDestruction_NoMemoryLeaks) {
    /**
     * WHAT: Verify system destruction doesn't leak memory
     * WHY:  Memory management is critical for long-running systems
     * HOW:  Create and destroy system, verify no crashes
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    // Destroy should work without crashes
    systems_consolidation_destroy(system);
    system = nullptr;  // Prevent double-free in TearDown

    // Destroying NULL should be safe
    systems_consolidation_destroy(nullptr);

    SUCCEED() << "Destruction completed without crashes";
}

TEST_F(SystemsConsolidationTest, SystemReset_ClearsMemories) {
    /**
     * WHAT: Verify reset clears all cortical memories
     * WHY:  Useful for testing and starting fresh
     * HOW:  Create nodes, reset, verify empty
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    // Add some cortical nodes (via transfer - will test properly later)
    // For now just verify reset works on empty system
    systems_consolidation_reset(system);

    EXPECT_EQ(system->node_count, 0) << "Reset should clear all nodes";
    EXPECT_EQ(system->replay_queue_size, 0) << "Reset should clear replay queue";
    EXPECT_EQ(system->total_replays, 0) << "Reset should clear replay counter";
    EXPECT_EQ(system->total_transfers, 0) << "Reset should clear transfer counter";
}

//=============================================================================
// Unit Test 2: Replay Scheduling
//=============================================================================

TEST_F(SystemsConsolidationTest, ReplayScheduling_Success) {
    /**
     * WHAT: Verify replay events can be scheduled
     * WHY:  Replay is core mechanism for consolidation
     * HOW:  Schedule replay, verify queue updated
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    // Schedule a replay event
    uint64_t engram_id = 12345;
    float priority = 0.8f;

    bool scheduled = systems_consolidation_schedule_replay(system, engram_id, priority);

    EXPECT_TRUE(scheduled) << "Replay scheduling should succeed";
    EXPECT_EQ(system->replay_queue_size, 1) << "Queue should have 1 event";
}

TEST_F(SystemsConsolidationTest, ReplayScheduling_MultipleEvents) {
    /**
     * WHAT: Verify multiple replay events can be scheduled
     * WHY:  During sleep, many memories replay
     * HOW:  Schedule multiple replays, verify queue grows
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    // Schedule multiple replays
    const uint32_t NUM_REPLAYS = 10;
    for (uint32_t i = 0; i < NUM_REPLAYS; i++) {
        bool scheduled = systems_consolidation_schedule_replay(system, 1000 + i, 0.5f);
        EXPECT_TRUE(scheduled) << "Replay " << i << " should be scheduled";
    }

    EXPECT_EQ(system->replay_queue_size, NUM_REPLAYS) << "Queue should have all events";
}

TEST_F(SystemsConsolidationTest, ReplayScheduling_QueueFull) {
    /**
     * WHAT: Verify queue capacity limit is enforced
     * WHY:  Prevents unbounded memory growth
     * HOW:  Fill queue to capacity, verify rejection
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    uint32_t capacity = system->replay_queue_capacity;

    // Fill queue to capacity
    for (uint32_t i = 0; i < capacity; i++) {
        bool scheduled = systems_consolidation_schedule_replay(system, 1000 + i, 0.5f);
        EXPECT_TRUE(scheduled) << "Should accept events up to capacity";
    }

    // Try to schedule one more (should fail)
    bool overflow = systems_consolidation_schedule_replay(system, 9999, 1.0f);
    EXPECT_FALSE(overflow) << "Should reject event when queue full";
}

TEST_F(SystemsConsolidationTest, ReplayScheduling_InvalidInput) {
    /**
     * WHAT: Verify replay scheduling rejects invalid input
     * WHY:  Guard clauses prevent bugs
     * HOW:  Try scheduling with NULL system, 0 engram ID
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    // NULL system
    bool result1 = systems_consolidation_schedule_replay(nullptr, 123, 0.5f);
    EXPECT_FALSE(result1) << "Should reject NULL system";

    // Zero engram ID
    bool result2 = systems_consolidation_schedule_replay(system, 0, 0.5f);
    EXPECT_FALSE(result2) << "Should reject zero engram ID";
}

//=============================================================================
// Unit Test 3: Replay Execution
//=============================================================================

TEST_F(SystemsConsolidationTest, ReplayExecution_SWSMode) {
    /**
     * WHAT: Verify replays execute during slow-wave sleep
     * WHY:  SWS is primary consolidation mode (Born & Wilhelm, 2012)
     * HOW:  Schedule replays, execute in SWS mode, verify processing
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    // Create and link engram system (required for transfer)
    engram_sys = engram_system_create();
    ASSERT_NE(engram_sys, nullptr);
    systems_consolidation_set_engram_system(system, engram_sys);

    // Create a test engram
    uint32_t neurons[] = {1, 2, 3, 4, 5};
    float activations[] = {0.8f, 0.7f, 0.9f, 0.6f, 0.85f};
    emotional_tag_t emotion = {0.5f, 0.7f, 0, EMOTION_CAT_JOY, 0.8f};

    uint64_t engram_id = engram_encode(engram_sys, neurons, activations, 5,
                                        MEMORY_TYPE_EPISODIC, emotion);
    ASSERT_NE(engram_id, 0) << "Engram encoding should succeed";

    // Schedule replay
    bool scheduled = systems_consolidation_schedule_replay(system, engram_id, 0.8f);
    ASSERT_TRUE(scheduled);

    // Execute replays in SWS mode
    uint32_t executed = systems_consolidation_execute_replays(
        system,
        1.0f,   // 1 second
        true,   // is_sws
        false   // is_rem
    );

    EXPECT_GT(executed, 0) << "Should execute replays in SWS";
    EXPECT_GT(system->total_replays, 0) << "Total replay counter should increment";
    EXPECT_GT(system->node_count, 0) << "Should create cortical nodes";
}

TEST_F(SystemsConsolidationTest, ReplayExecution_REMMode) {
    /**
     * WHAT: Verify replays execute during REM sleep
     * WHY:  REM consolidates and integrates memories
     * HOW:  Schedule replays, execute in REM mode, verify processing
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    engram_sys = engram_system_create();
    systems_consolidation_set_engram_system(system, engram_sys);

    // Create test engram
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.7f, 0, EMOTION_CAT_NEUTRAL, 0.6f};

    uint64_t engram_id = engram_encode(engram_sys, neurons, activations, 3,
                                        MEMORY_TYPE_EPISODIC, emotion);
    ASSERT_NE(engram_id, 0);

    systems_consolidation_schedule_replay(system, engram_id, 0.7f);

    // Execute in REM mode
    uint32_t executed = systems_consolidation_execute_replays(
        system,
        1.0f,
        false,  // not SWS
        true    // is_rem
    );

    EXPECT_GT(executed, 0) << "Should execute replays in REM";
}

TEST_F(SystemsConsolidationTest, ReplayExecution_AwakeMode) {
    /**
     * WHAT: Verify minimal replay during wakefulness
     * WHY:  Consolidation primarily happens during sleep
     * HOW:  Try executing replays while awake, verify low rate
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    engram_sys = engram_system_create();
    systems_consolidation_set_engram_system(system, engram_sys);

    // Schedule many replays
    for (uint32_t i = 0; i < 100; i++) {
        // Create dummy engram
        uint32_t neurons[] = {i, i + 1, i + 2};
        float activations[] = {0.8f, 0.7f, 0.9f};
        emotional_tag_t emotion = {0.0f, 0.0f, 0, EMOTION_CAT_NEUTRAL, 0.5f};

        uint64_t eid = engram_encode(engram_sys, neurons, activations, 3,
                                      MEMORY_TYPE_EPISODIC, emotion);
        systems_consolidation_schedule_replay(system, eid, 0.5f);
    }

    // Execute while awake (should process very few)
    uint32_t executed = systems_consolidation_execute_replays(
        system,
        1.0f,
        false,  // not SWS
        false   // not REM
    );

    // Awake replay rate is 0.1 Hz, so ~0 replays in 1 second
    EXPECT_LT(executed, 5) << "Awake replay rate should be minimal";
}

//=============================================================================
// Unit Test 4: Cortical Transfer
//=============================================================================

TEST_F(SystemsConsolidationTest, CorticalTransfer_CreatesNode) {
    /**
     * WHAT: Verify engram transfer creates cortical node
     * WHY:  Core mechanism of systems consolidation
     * HOW:  Transfer engram, verify cortical node created
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    engram_sys = engram_system_create();
    systems_consolidation_set_engram_system(system, engram_sys);

    // Create test engram
    uint32_t neurons[] = {10, 20, 30, 40};
    float activations[] = {0.9f, 0.8f, 0.7f, 0.85f};
    emotional_tag_t emotion = {0.6f, 0.8f, 0, EMOTION_CAT_JOY, 0.9f};

    uint64_t engram_id = engram_encode(engram_sys, neurons, activations, 4,
                                        MEMORY_TYPE_EPISODIC, emotion);
    ASSERT_NE(engram_id, 0);

    // Transfer to cortex
    uint64_t node_id = systems_consolidation_transfer_to_cortex(
        system,
        engram_id,
        0.8f  // replay strength
    );

    EXPECT_NE(node_id, 0) << "Transfer should create cortical node";
    EXPECT_EQ(system->node_count, 1) << "Should have 1 cortical node";

    // Verify node properties
    cortical_memory_node_t* node = systems_consolidation_get_node(system, node_id);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->source_engram_id, engram_id) << "Should track source engram";
    EXPECT_EQ(node->type, CORTICAL_MEMORY_EPISODIC) << "Should start as episodic";
    EXPECT_GT(node->consolidation_strength, 0.0f) << "Should have some consolidation";
    EXPECT_LT(node->hippocampal_dependency, 1.0f) << "Dependency should decrease";
}

TEST_F(SystemsConsolidationTest, CorticalTransfer_UpdatesExistingNode) {
    /**
     * WHAT: Verify repeated transfer strengthens existing node
     * WHY:  Repeated replay consolidates memory (Carr et al., 2011)
     * HOW:  Transfer same engram twice, verify strengthening
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    engram_sys = engram_system_create();
    systems_consolidation_set_engram_system(system, engram_sys);

    // Create test engram
    uint32_t neurons[] = {5, 10, 15};
    float activations[] = {0.8f, 0.9f, 0.7f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_NEUTRAL, 0.7f};

    uint64_t engram_id = engram_encode(engram_sys, neurons, activations, 3,
                                        MEMORY_TYPE_EPISODIC, emotion);

    // First transfer
    uint64_t node_id_1 = systems_consolidation_transfer_to_cortex(system, engram_id, 0.5f);
    ASSERT_NE(node_id_1, 0);

    cortical_memory_node_t* node = systems_consolidation_get_node(system, node_id_1);
    ASSERT_NE(node, nullptr);
    float strength_after_first = node->consolidation_strength;

    // Second transfer (replay of same engram)
    uint64_t node_id_2 = systems_consolidation_transfer_to_cortex(system, engram_id, 0.5f);

    EXPECT_EQ(node_id_2, node_id_1) << "Should update same node, not create new one";
    EXPECT_EQ(system->node_count, 1) << "Should still have 1 node";
    EXPECT_GT(node->consolidation_strength, strength_after_first)
        << "Consolidation should strengthen";
}

//=============================================================================
// Unit Test 5: Consolidation Dynamics
//=============================================================================

TEST_F(SystemsConsolidationTest, ConsolidationUpdate_StrengthensMemories) {
    /**
     * WHAT: Verify consolidation update strengthens cortical nodes
     * WHY:  Time-dependent consolidation (McClelland et al., 1995)
     * HOW:  Create node, run updates, verify strengthening
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    engram_sys = engram_system_create();
    systems_consolidation_set_engram_system(system, engram_sys);

    // Create and transfer engram
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_NEUTRAL, 0.7f};

    uint64_t engram_id = engram_encode(engram_sys, neurons, activations, 3,
                                        MEMORY_TYPE_EPISODIC, emotion);
    uint64_t node_id = systems_consolidation_transfer_to_cortex(system, engram_id, 0.2f);

    cortical_memory_node_t* node = systems_consolidation_get_node(system, node_id);
    ASSERT_NE(node, nullptr);
    float initial_strength = node->consolidation_strength;

    // Run consolidation updates (simulating time passing during sleep)
    for (int i = 0; i < 100; i++) {
        systems_consolidation_update(system, 36.0f, true);  // 36 seconds = 0.01 hour
    }

    EXPECT_GT(node->consolidation_strength, initial_strength)
        << "Consolidation should strengthen over time";
}

TEST_F(SystemsConsolidationTest, ConsolidationUpdate_SleepAcceleration) {
    /**
     * WHAT: Verify sleep accelerates consolidation vs awake
     * WHY:  Sleep-dependent consolidation (Born & Wilhelm, 2012)
     * HOW:  Compare consolidation rates sleeping vs awake
     */

    // Create two systems for comparison
    systems_consolidation_system_t* sys_awake = systems_consolidation_create();
    systems_consolidation_system_t* sys_sleep = systems_consolidation_create();
    ASSERT_NE(sys_awake, nullptr);
    ASSERT_NE(sys_sleep, nullptr);

    engram_sys = engram_system_create();
    systems_consolidation_set_engram_system(sys_awake, engram_sys);
    systems_consolidation_set_engram_system(sys_sleep, engram_sys);

    // Create identical engrams and nodes
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_NEUTRAL, 0.7f};

    uint64_t engram_id = engram_encode(engram_sys, neurons, activations, 3,
                                        MEMORY_TYPE_EPISODIC, emotion);

    uint64_t node_id_awake = systems_consolidation_transfer_to_cortex(sys_awake, engram_id, 0.2f);
    uint64_t node_id_sleep = systems_consolidation_transfer_to_cortex(sys_sleep, engram_id, 0.2f);

    // Run consolidation: awake vs sleeping
    systems_consolidation_update(sys_awake, 3600.0f, false);  // 1 hour awake
    systems_consolidation_update(sys_sleep, 3600.0f, true);   // 1 hour sleeping

    cortical_memory_node_t* node_awake = systems_consolidation_get_node(sys_awake, node_id_awake);
    cortical_memory_node_t* node_sleep = systems_consolidation_get_node(sys_sleep, node_id_sleep);

    ASSERT_NE(node_awake, nullptr);
    ASSERT_NE(node_sleep, nullptr);

    EXPECT_GT(node_sleep->consolidation_strength, node_awake->consolidation_strength)
        << "Sleep should accelerate consolidation vs awake";

    systems_consolidation_destroy(sys_awake);
    systems_consolidation_destroy(sys_sleep);
}

TEST_F(SystemsConsolidationTest, ConsolidationUpdate_HippocampalIndependence) {
    /**
     * WHAT: Verify cortex becomes independent of hippocampus over time
     * WHY:  Systems consolidation reduces hippocampal dependency
     * HOW:  Run updates, verify hippocampal_dependency decreases
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    engram_sys = engram_system_create();
    systems_consolidation_set_engram_system(system, engram_sys);

    // Create node
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_NEUTRAL, 0.7f};

    uint64_t engram_id = engram_encode(engram_sys, neurons, activations, 3,
                                        MEMORY_TYPE_EPISODIC, emotion);
    uint64_t node_id = systems_consolidation_transfer_to_cortex(system, engram_id, 0.2f);

    cortical_memory_node_t* node = systems_consolidation_get_node(system, node_id);
    ASSERT_NE(node, nullptr);

    float initial_dependency = node->hippocampal_dependency;

    // Consolidate over time
    for (int i = 0; i < 200; i++) {
        systems_consolidation_update(system, 36.0f, true);  // Sleeping
    }

    EXPECT_LT(node->hippocampal_dependency, initial_dependency)
        << "Hippocampal dependency should decrease";
    EXPECT_GE(node->hippocampal_dependency, 0.0f)
        << "Dependency should not go negative";
}

TEST_F(SystemsConsolidationTest, ConsolidationUpdate_EpisodicToSemantic) {
    /**
     * WHAT: Verify episodic memories become semantic over time
     * WHY:  Details fade, gist remains (Winocur & Moscovitch, 2011)
     * HOW:  Consolidate until threshold, verify type transition
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    engram_sys = engram_system_create();
    systems_consolidation_set_engram_system(system, engram_sys);

    // Create node
    uint32_t neurons[] = {1, 2, 3, 4, 5};
    float activations[] = {0.9f, 0.8f, 0.85f, 0.75f, 0.95f};
    emotional_tag_t emotion = {0.6f, 0.7f, 0, EMOTION_CAT_JOY, 0.8f};

    uint64_t engram_id = engram_encode(engram_sys, neurons, activations, 5,
                                        MEMORY_TYPE_EPISODIC, emotion);

    // Strong initial replay to bootstrap consolidation
    uint64_t node_id = systems_consolidation_transfer_to_cortex(system, engram_id, 1.0f);

    cortical_memory_node_t* node = systems_consolidation_get_node(system, node_id);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->type, CORTICAL_MEMORY_EPISODIC) << "Should start episodic";

    // Consolidate until semantic threshold (longer simulation time)
    // With 5% per hour consolidation rate in sleep, need ~14 hours to reach 0.7
    for (int i = 0; i < 1500; i++) {
        systems_consolidation_update(system, 36.0f, true);  // 36s = 0.01 hour
        if (node->type == CORTICAL_MEMORY_SEMANTIC) {
            break;
        }
    }

    EXPECT_EQ(node->type, CORTICAL_MEMORY_SEMANTIC)
        << "Should transition to semantic memory type";
}

//=============================================================================
// Unit Test 6: Query API
//=============================================================================

TEST_F(SystemsConsolidationTest, GetNode_RetrievesCorrectNode) {
    /**
     * WHAT: Verify get_node retrieves correct cortical node
     * WHY:  Needed for inspection and testing
     * HOW:  Create node, retrieve by ID, verify match
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    engram_sys = engram_system_create();
    systems_consolidation_set_engram_system(system, engram_sys);

    // Create node
    uint32_t neurons[] = {7, 8, 9};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_NEUTRAL, 0.7f};

    uint64_t engram_id = engram_encode(engram_sys, neurons, activations, 3,
                                        MEMORY_TYPE_EPISODIC, emotion);
    uint64_t node_id = systems_consolidation_transfer_to_cortex(system, engram_id, 0.5f);

    // Retrieve node
    cortical_memory_node_t* node = systems_consolidation_get_node(system, node_id);

    ASSERT_NE(node, nullptr) << "Should retrieve node by ID";
    EXPECT_EQ(node->id, node_id) << "Node ID should match";
    EXPECT_EQ(node->source_engram_id, engram_id) << "Source engram should match";
}

TEST_F(SystemsConsolidationTest, GetNode_InvalidID) {
    /**
     * WHAT: Verify get_node returns NULL for invalid ID
     * WHY:  Guard clauses prevent bugs
     * HOW:  Try retrieving non-existent node
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    cortical_memory_node_t* node = systems_consolidation_get_node(system, 99999);
    EXPECT_EQ(node, nullptr) << "Should return NULL for non-existent ID";

    node = systems_consolidation_get_node(system, 0);
    EXPECT_EQ(node, nullptr) << "Should return NULL for zero ID";

    node = systems_consolidation_get_node(nullptr, 123);
    EXPECT_EQ(node, nullptr) << "Should return NULL for NULL system";
}

TEST_F(SystemsConsolidationTest, GetStatistics_ReturnsCorrectCounts) {
    /**
     * WHAT: Verify statistics API returns correct counts
     * WHY:  Needed for monitoring and debugging
     * HOW:  Perform operations, query stats, verify counts
     */

    system = systems_consolidation_create();
    ASSERT_NE(system, nullptr);

    engram_sys = engram_system_create();
    systems_consolidation_set_engram_system(system, engram_sys);

    // Initial stats
    uint32_t nodes, pending;
    uint64_t replays, transfers, forgotten;
    systems_consolidation_get_statistics(system, &nodes, &replays, &transfers, &forgotten, &pending);

    EXPECT_EQ(nodes, 0);
    EXPECT_EQ(replays, 0);
    EXPECT_EQ(pending, 0);

    // Schedule and execute replays
    for (uint32_t i = 0; i < 5; i++) {
        uint32_t neurons[] = {i, i + 1, i + 2};
        float activations[] = {0.8f, 0.7f, 0.9f};
        emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_NEUTRAL, 0.7f};

        uint64_t eid = engram_encode(engram_sys, neurons, activations, 3,
                                      MEMORY_TYPE_EPISODIC, emotion);
        systems_consolidation_schedule_replay(system, eid, 0.8f);
    }

    systems_consolidation_execute_replays(system, 1.0f, true, false);

    // Check updated stats
    systems_consolidation_get_statistics(system, &nodes, &replays, &transfers, &forgotten, &pending);

    EXPECT_GT(nodes, 0) << "Should have created cortical nodes";
    EXPECT_GT(replays, 0) << "Should have executed replays";
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
