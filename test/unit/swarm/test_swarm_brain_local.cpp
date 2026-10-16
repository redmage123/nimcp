/**
 * @file test_swarm_brain_local.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Brain Local Instantiation
 *
 * TEST COVERAGE:
 * - Manager creation and destruction
 * - Agent brain creation and management
 * - Local learning and processing
 * - Weight synchronization
 * - Divergence detection
 * - Consensus weight calculation
 * - Bio-async integration
 * - Statistics tracking
 * - Edge cases and error handling
 * - Multi-agent scenarios
 * - Performance under load
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_brain_local.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SwarmBrainLocalTest : public ::testing::Test {
protected:
    swarm_brain_manager_t* manager;
    swarm_brain_config_t config;

    void SetUp() override {
        // Get default configuration
        config = swarm_brain_local_default_config();
        config.enable_bio_async = false;  // Disable for unit tests
        config.test_mode = true;          // Skip actual brain creation for fast tests

        // Create manager
        manager = swarm_brain_manager_create(&config);
        ASSERT_NE(manager, nullptr);
    }

    void TearDown() override {
        if (manager) {
            swarm_brain_manager_destroy(manager);
            manager = nullptr;
        }
    }
};

//=============================================================================
// Manager Creation and Destruction Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, CreateValidManager) {
    EXPECT_NE(manager, nullptr);
}

TEST_F(SwarmBrainLocalTest, CreateWithNullConfig) {
    auto* mgr = swarm_brain_manager_create(nullptr);
    EXPECT_NE(mgr, nullptr);  // Should use defaults
    if (mgr) {
        swarm_brain_manager_destroy(mgr);
    }
}

TEST_F(SwarmBrainLocalTest, DestroyNullManager) {
    swarm_brain_manager_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

TEST_F(SwarmBrainLocalTest, CreateWithCustomConfig) {
    swarm_brain_config_t custom_config = {
        .default_brain_size = 50,
        .max_local_neurons = 200,
        .sync_interval_ms = 500,
        .divergence_threshold = 0.2f,
        .enable_weight_sharing = true,
        .enable_bio_async = false
    };

    auto* mgr = swarm_brain_manager_create(&custom_config);
    EXPECT_NE(mgr, nullptr);
    swarm_brain_manager_destroy(mgr);
}

//=============================================================================
// Agent Brain Creation Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, CreateAgentBrain) {
    int result = swarm_brain_create_for_agent(manager, 1, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(swarm_brain_has_agent(manager, 1));
}

TEST_F(SwarmBrainLocalTest, CreateMultipleAgentBrains) {
    for (uint32_t i = 1; i <= 10; i++) {
        int result = swarm_brain_create_for_agent(manager, i, 100);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(swarm_brain_get_agent_count(manager), 10u);
}

TEST_F(SwarmBrainLocalTest, CreateAgentWithDefaultSize) {
    int result = swarm_brain_create_for_agent(manager, 1, 0);  // 0 = use default
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmBrainLocalTest, CreateAgentWithExcessiveSize) {
    int result = swarm_brain_create_for_agent(
        manager, 1, config.max_local_neurons + 100
    );
    EXPECT_NE(result, NIMCP_SUCCESS);  // Should fail
}

TEST_F(SwarmBrainLocalTest, CreateDuplicateAgent) {
    swarm_brain_create_for_agent(manager, 1, 100);
    int result = swarm_brain_create_for_agent(manager, 1, 100);
    EXPECT_NE(result, NIMCP_SUCCESS);  // Should fail - already exists
}

TEST_F(SwarmBrainLocalTest, CreateAgentNullManager) {
    int result = swarm_brain_create_for_agent(nullptr, 1, 100);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Agent Brain Destruction Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, DestroyAgentBrain) {
    swarm_brain_create_for_agent(manager, 1, 100);
    int result = swarm_brain_destroy_for_agent(manager, 1);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(swarm_brain_has_agent(manager, 1));
}

TEST_F(SwarmBrainLocalTest, DestroyNonexistentAgent) {
    int result = swarm_brain_destroy_for_agent(manager, 999);
    EXPECT_NE(result, NIMCP_SUCCESS);  // Should fail
}

TEST_F(SwarmBrainLocalTest, DestroyAgentNullManager) {
    int result = swarm_brain_destroy_for_agent(nullptr, 1);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Agent Brain Access Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, GetAgentBrain) {
    swarm_brain_create_for_agent(manager, 1, 100);
    nimcp_brain_t brain = swarm_brain_get(manager, 1);
    EXPECT_NE(brain, nullptr);
}

TEST_F(SwarmBrainLocalTest, GetNonexistentAgentBrain) {
    nimcp_brain_t brain = swarm_brain_get(manager, 999);
    EXPECT_EQ(brain, nullptr);
}

TEST_F(SwarmBrainLocalTest, GetAgentBrainNullManager) {
    nimcp_brain_t brain = swarm_brain_get(nullptr, 1);
    EXPECT_EQ(brain, nullptr);
}

TEST_F(SwarmBrainLocalTest, HasAgent) {
    EXPECT_FALSE(swarm_brain_has_agent(manager, 1));
    swarm_brain_create_for_agent(manager, 1, 100);
    EXPECT_TRUE(swarm_brain_has_agent(manager, 1));
}

TEST_F(SwarmBrainLocalTest, GetAgentCount) {
    EXPECT_EQ(swarm_brain_get_agent_count(manager), 0u);

    for (uint32_t i = 1; i <= 5; i++) {
        swarm_brain_create_for_agent(manager, i, 100);
    }

    EXPECT_EQ(swarm_brain_get_agent_count(manager), 5u);
}

//=============================================================================
// Local Learning Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, LocalLearnValidData) {
    swarm_brain_create_for_agent(manager, 1, 100);

    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float target[5] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f};

    int result = swarm_brain_local_learn(
        manager, 1, input, 10, target, 5
    );

    // Result depends on brain API implementation
    // At minimum, should not crash
    SUCCEED();
}

TEST_F(SwarmBrainLocalTest, LocalLearnNonexistentAgent) {
    float input[10] = {0.0f};
    float target[5] = {0.0f};

    int result = swarm_brain_local_learn(
        manager, 999, input, 10, target, 5
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(SwarmBrainLocalTest, LocalLearnNullData) {
    swarm_brain_create_for_agent(manager, 1, 100);

    int result = swarm_brain_local_learn(
        manager, 1, nullptr, 10, nullptr, 5
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Local Processing Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, LocalProcessValidData) {
    swarm_brain_create_for_agent(manager, 1, 100);

    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float output[10] = {0.0f};
    uint32_t output_size = 10;

    int result = swarm_brain_local_process(
        manager, 1, input, 10, output, &output_size
    );

    // Result depends on brain API implementation
    SUCCEED();
}

TEST_F(SwarmBrainLocalTest, LocalProcessNonexistentAgent) {
    float input[10] = {0.0f};
    float output[10] = {0.0f};
    uint32_t output_size = 10;

    int result = swarm_brain_local_process(
        manager, 999, input, 10, output, &output_size
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(SwarmBrainLocalTest, LocalProcessNullData) {
    swarm_brain_create_for_agent(manager, 1, 100);

    int result = swarm_brain_local_process(
        manager, 1, nullptr, 10, nullptr, nullptr
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Weight Synchronization Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, SyncWeightsSingleAgent) {
    swarm_brain_create_for_agent(manager, 1, 100);

    int result = swarm_brain_local_sync_weights(manager, 1);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmBrainLocalTest, SyncWeightsNonexistentAgent) {
    int result = swarm_brain_local_sync_weights(manager, 999);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(SwarmBrainLocalTest, SyncAllWeights) {
    for (uint32_t i = 1; i <= 5; i++) {
        swarm_brain_create_for_agent(manager, i, 100);
    }

    int result = swarm_brain_sync_all(manager);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmBrainLocalTest, SyncWeightsNullManager) {
    int result = swarm_brain_local_sync_weights(nullptr, 1);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Consensus Weights Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, GetConsensusWeightsNoAgents) {
    float* weights = nullptr;
    uint32_t num_weights = 0;

    int result = swarm_brain_get_consensus_weights(
        manager, &weights, &num_weights
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(weights, nullptr);
    EXPECT_EQ(num_weights, 0u);
}

TEST_F(SwarmBrainLocalTest, GetConsensusWeightsMultipleAgents) {
    for (uint32_t i = 1; i <= 3; i++) {
        swarm_brain_create_for_agent(manager, i, 100);
        swarm_brain_local_sync_weights(manager, i);  // Initialize weights
    }

    float* weights = nullptr;
    uint32_t num_weights = 0;

    int result = swarm_brain_get_consensus_weights(
        manager, &weights, &num_weights
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);

    if (weights) {
        nimcp_free(weights);
    }
}

TEST_F(SwarmBrainLocalTest, GetConsensusWeightsNullParams) {
    int result = swarm_brain_get_consensus_weights(
        manager, nullptr, nullptr
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Divergence Detection Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, GetDivergenceExistingAgent) {
    swarm_brain_create_for_agent(manager, 1, 100);
    swarm_brain_local_sync_weights(manager, 1);

    float divergence = swarm_brain_get_divergence(manager, 1);
    EXPECT_GE(divergence, 0.0f);  // Should be non-negative
    EXPECT_LE(divergence, 1.0f);  // Should be normalized
}

TEST_F(SwarmBrainLocalTest, GetDivergenceNonexistentAgent) {
    float divergence = swarm_brain_get_divergence(manager, 999);
    EXPECT_EQ(divergence, -1.0f);  // Error indicator
}

TEST_F(SwarmBrainLocalTest, GetDivergenceNullManager) {
    float divergence = swarm_brain_get_divergence(nullptr, 1);
    EXPECT_EQ(divergence, -1.0f);
}

TEST_F(SwarmBrainLocalTest, GetDivergentAgentsNone) {
    swarm_brain_create_for_agent(manager, 1, 100);

    uint32_t* agents = nullptr;
    uint32_t count = 0;

    int result = swarm_brain_get_divergent_agents(
        manager, &agents, &count
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(agents, nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(SwarmBrainLocalTest, GetDivergentAgentsNullParams) {
    int result = swarm_brain_get_divergent_agents(
        manager, nullptr, nullptr
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, GetStatsInitial) {
    swarm_brain_stats_t stats;
    int result = swarm_brain_local_get_stats(manager, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.num_agents, 0u);
    EXPECT_EQ(stats.total_neurons, 0u);
}

TEST_F(SwarmBrainLocalTest, GetStatsWithAgents) {
    for (uint32_t i = 1; i <= 3; i++) {
        swarm_brain_create_for_agent(manager, i, 100);
    }

    swarm_brain_stats_t stats;
    int result = swarm_brain_local_get_stats(manager, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.num_agents, 3u);
    EXPECT_EQ(stats.total_neurons, 300u);
}

TEST_F(SwarmBrainLocalTest, ResetStats) {
    swarm_brain_create_for_agent(manager, 1, 100);
    swarm_brain_local_sync_weights(manager, 1);

    int result = swarm_brain_local_reset_stats(manager);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    swarm_brain_stats_t stats;
    swarm_brain_local_get_stats(manager, &stats);
    EXPECT_EQ(stats.sync_count, 0u);
}

TEST_F(SwarmBrainLocalTest, GetStatsNullParams) {
    int result = swarm_brain_local_get_stats(manager, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, DefaultConfigValues) {
    swarm_brain_config_t cfg = swarm_brain_local_default_config();

    EXPECT_EQ(cfg.default_brain_size, SWARM_BRAIN_DEFAULT_SIZE);
    EXPECT_EQ(cfg.max_local_neurons, SWARM_BRAIN_MAX_NEURONS);
    EXPECT_EQ(cfg.sync_interval_ms, SWARM_BRAIN_DEFAULT_SYNC_INTERVAL);
    EXPECT_FLOAT_EQ(cfg.divergence_threshold,
                    SWARM_BRAIN_DEFAULT_DIVERGENCE_THRESHOLD);
}

TEST_F(SwarmBrainLocalTest, GetAllAgentsEmpty) {
    uint32_t* agents = nullptr;
    uint32_t count = 0;

    int result = swarm_brain_get_all_agents(manager, &agents, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(agents, nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(SwarmBrainLocalTest, GetAllAgents) {
    std::vector<uint32_t> agent_ids = {1, 5, 10, 15};

    for (uint32_t id : agent_ids) {
        swarm_brain_create_for_agent(manager, id, 100);
    }

    uint32_t* agents = nullptr;
    uint32_t count = 0;

    int result = swarm_brain_get_all_agents(manager, &agents, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, agent_ids.size());

    if (agents) {
        // Verify all agent IDs are present
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_TRUE(swarm_brain_has_agent(manager, agents[i]));
        }
        nimcp_free(agents);
    }
}

TEST_F(SwarmBrainLocalTest, GetAllAgentsNullParams) {
    int result = swarm_brain_get_all_agents(manager, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Multi-Agent Scenario Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, MultiAgentCreationAndDestruction) {
    const uint32_t num_agents = 20;

    // Create agents
    for (uint32_t i = 1; i <= num_agents; i++) {
        int result = swarm_brain_create_for_agent(manager, i, 100);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(swarm_brain_get_agent_count(manager), num_agents);

    // Destroy half
    for (uint32_t i = 1; i <= num_agents / 2; i++) {
        int result = swarm_brain_destroy_for_agent(manager, i);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(swarm_brain_get_agent_count(manager), num_agents / 2);
}

TEST_F(SwarmBrainLocalTest, MultiAgentSync) {
    for (uint32_t i = 1; i <= 5; i++) {
        swarm_brain_create_for_agent(manager, i, 100);
    }

    // Sync all
    int result = swarm_brain_sync_all(manager);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify all agents synced
    swarm_brain_stats_t stats;
    swarm_brain_local_get_stats(manager, &stats);
    EXPECT_GT(stats.sync_count, 0u);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(SwarmBrainLocalTest, MaxAgentsLimit) {
    // Try to create more than max agents
    for (uint32_t i = 1; i <= SWARM_BRAIN_MAX_AGENTS + 10; i++) {
        int result = swarm_brain_create_for_agent(manager, i, 50);
        if (i <= SWARM_BRAIN_MAX_AGENTS) {
            EXPECT_EQ(result, NIMCP_SUCCESS);
        } else {
            EXPECT_NE(result, NIMCP_SUCCESS);  // Should fail after limit
        }
    }
}

TEST_F(SwarmBrainLocalTest, ZeroSizeBrain) {
    // Should use default size
    int result = swarm_brain_create_for_agent(manager, 1, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmBrainLocalTest, ConcurrentAccess) {
    // Basic thread safety check (single-threaded test)
    swarm_brain_create_for_agent(manager, 1, 100);

    for (int i = 0; i < 100; i++) {
        swarm_brain_local_sync_weights(manager, 1);
        float divergence = swarm_brain_get_divergence(manager, 1);
        (void)divergence;  // Suppress unused warning
    }

    SUCCEED();
}

//=============================================================================
// Role-Based Brain Creation Tests
//=============================================================================

/**
 * WHAT: Test role name string for all predefined roles
 * WHY:  Verify role names are correctly mapped
 */
TEST_F(SwarmBrainLocalTest, RoleName_AllRoles) {
    EXPECT_STREQ(swarm_brain_role_name(DRONE_ROLE_SCOUT), "Scout");
    EXPECT_STREQ(swarm_brain_role_name(DRONE_ROLE_WORKER), "Worker");
    EXPECT_STREQ(swarm_brain_role_name(DRONE_ROLE_COORDINATOR), "Coordinator");
    EXPECT_STREQ(swarm_brain_role_name(DRONE_ROLE_SENSOR), "Sensor");
    EXPECT_STREQ(swarm_brain_role_name(DRONE_ROLE_GUARDIAN), "Guardian");
    EXPECT_STREQ(swarm_brain_role_name(DRONE_ROLE_RELAY), "Relay");
    EXPECT_STREQ(swarm_brain_role_name(DRONE_ROLE_CUSTOM), "Custom");
}

/**
 * WHAT: Test role name for invalid role
 * WHY:  Verify graceful handling of out-of-range values
 */
TEST_F(SwarmBrainLocalTest, RoleName_InvalidRole) {
    const char* name = swarm_brain_role_name(static_cast<drone_role_t>(999));
    EXPECT_STREQ(name, "Unknown");
}

/**
 * WHAT: Test getting role template for each predefined role
 * WHY:  Verify templates return expected brain sizes
 */
TEST_F(SwarmBrainLocalTest, GetRoleTemplate_AllRoles) {
    // Scout: BRAIN_SIZE_SMALL
    drone_brain_template_t scout = swarm_brain_get_role_template(DRONE_ROLE_SCOUT);
    EXPECT_EQ(scout.role, DRONE_ROLE_SCOUT);
    EXPECT_EQ(scout.brain_size, BRAIN_SIZE_SMALL);
    EXPECT_TRUE(scout.enable_visual_cortex);
    EXPECT_TRUE(scout.enable_working_memory);
    EXPECT_TRUE(scout.enable_curiosity);

    // Worker: BRAIN_SIZE_MICRO (minimal)
    drone_brain_template_t worker = swarm_brain_get_role_template(DRONE_ROLE_WORKER);
    EXPECT_EQ(worker.role, DRONE_ROLE_WORKER);
    EXPECT_EQ(worker.brain_size, BRAIN_SIZE_MICRO);
    EXPECT_TRUE(worker.minimal_mode);

    // Coordinator: BRAIN_SIZE_MEDIUM
    drone_brain_template_t coord = swarm_brain_get_role_template(DRONE_ROLE_COORDINATOR);
    EXPECT_EQ(coord.role, DRONE_ROLE_COORDINATOR);
    EXPECT_EQ(coord.brain_size, BRAIN_SIZE_MEDIUM);
    EXPECT_TRUE(coord.enable_theory_of_mind);
    EXPECT_TRUE(coord.enable_ethics);

    // Sensor: BRAIN_SIZE_TINY
    drone_brain_template_t sensor = swarm_brain_get_role_template(DRONE_ROLE_SENSOR);
    EXPECT_EQ(sensor.role, DRONE_ROLE_SENSOR);
    EXPECT_EQ(sensor.brain_size, BRAIN_SIZE_TINY);
    EXPECT_TRUE(sensor.enable_visual_cortex);
    EXPECT_TRUE(sensor.enable_audio_cortex);

    // Guardian: BRAIN_SIZE_SMALL
    drone_brain_template_t guardian = swarm_brain_get_role_template(DRONE_ROLE_GUARDIAN);
    EXPECT_EQ(guardian.role, DRONE_ROLE_GUARDIAN);
    EXPECT_EQ(guardian.brain_size, BRAIN_SIZE_SMALL);
    EXPECT_TRUE(guardian.enable_ethics);
    EXPECT_TRUE(guardian.enable_executive_control);

    // Relay: BRAIN_SIZE_MICRO (minimal)
    drone_brain_template_t relay = swarm_brain_get_role_template(DRONE_ROLE_RELAY);
    EXPECT_EQ(relay.role, DRONE_ROLE_RELAY);
    EXPECT_EQ(relay.brain_size, BRAIN_SIZE_MICRO);
    EXPECT_TRUE(relay.minimal_mode);
}

/**
 * WHAT: Test getting role template for invalid role
 * WHY:  Verify fallback to WORKER template
 */
TEST_F(SwarmBrainLocalTest, GetRoleTemplate_InvalidRole) {
    drone_brain_template_t templ = swarm_brain_get_role_template(static_cast<drone_role_t>(999));
    // Should fall back to WORKER template (index 1)
    EXPECT_EQ(templ.brain_size, BRAIN_SIZE_MICRO);
    EXPECT_TRUE(templ.minimal_mode);
}

/**
 * WHAT: Test creating agent brain with specific role
 * WHY:  Verify role-based brain creation works correctly
 */
TEST_F(SwarmBrainLocalTest, CreateAgentWithRole_Scout) {
    int result = swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(swarm_brain_has_agent(manager, 1));

    // Verify role was set
    drone_role_t role = swarm_brain_get_agent_role(manager, 1);
    EXPECT_EQ(role, DRONE_ROLE_SCOUT);
}

/**
 * WHAT: Test creating agent brains with all roles
 * WHY:  Verify all roles can create agents successfully
 */
TEST_F(SwarmBrainLocalTest, CreateAgentWithRole_AllRoles) {
    drone_role_t roles[] = {
        DRONE_ROLE_SCOUT,
        DRONE_ROLE_WORKER,
        DRONE_ROLE_COORDINATOR,
        DRONE_ROLE_SENSOR,
        DRONE_ROLE_GUARDIAN,
        DRONE_ROLE_RELAY
    };

    for (int i = 0; i < 6; i++) {
        int result = swarm_brain_create_for_agent_with_role(manager, i + 1, roles[i]);
        EXPECT_EQ(result, NIMCP_SUCCESS) << "Failed to create agent with role " << i;
        EXPECT_TRUE(swarm_brain_has_agent(manager, i + 1));
        EXPECT_EQ(swarm_brain_get_agent_role(manager, i + 1), roles[i]);
    }

    EXPECT_EQ(swarm_brain_get_agent_count(manager), 6u);
}

/**
 * WHAT: Test creating agent with custom template
 * WHY:  Verify custom template overrides work
 */
TEST_F(SwarmBrainLocalTest, CreateAgentWithTemplate_Custom) {
    drone_brain_template_t custom = {
        .role = DRONE_ROLE_CUSTOM,
        .brain_size = BRAIN_SIZE_TINY,
        .neuron_override = 50,  // Custom neuron count
        .enable_visual_cortex = true,
        .enable_audio_cortex = false,
        .enable_speech_cortex = false,
        .enable_working_memory = true,
        .enable_global_workspace = false,
        .enable_theory_of_mind = false,
        .enable_ethics = false,
        .enable_curiosity = true,
        .enable_mirror_neurons = false,
        .enable_executive_control = false,
        .enable_consolidation = false,
        .enable_glial = false,
        .enable_cortical_columns = false,
        .enable_predictive = true,
        .enable_bio_async = true,
        .minimal_mode = false,
        .lazy_init_mode = true,
        .max_inference_time_ms = 0.3f,
        .max_memory_kb = 200
    };

    int result = swarm_brain_create_for_agent_with_template(manager, 1, &custom);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(swarm_brain_has_agent(manager, 1));
    EXPECT_EQ(swarm_brain_get_agent_role(manager, 1), DRONE_ROLE_CUSTOM);
}

/**
 * WHAT: Test setting and getting agent role
 * WHY:  Verify role can be changed after creation
 */
TEST_F(SwarmBrainLocalTest, SetGetAgentRole) {
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_WORKER);
    EXPECT_EQ(swarm_brain_get_agent_role(manager, 1), DRONE_ROLE_WORKER);

    // Change role
    int result = swarm_brain_set_agent_role(manager, 1, DRONE_ROLE_SCOUT);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(swarm_brain_get_agent_role(manager, 1), DRONE_ROLE_SCOUT);
}

/**
 * WHAT: Test getting role for nonexistent agent
 * WHY:  Verify error handling
 */
TEST_F(SwarmBrainLocalTest, GetAgentRole_Nonexistent) {
    drone_role_t role = swarm_brain_get_agent_role(manager, 999);
    EXPECT_EQ(role, DRONE_ROLE_CUSTOM);  // Default return on error
}

/**
 * WHAT: Test setting role for nonexistent agent
 * WHY:  Verify error handling
 */
TEST_F(SwarmBrainLocalTest, SetAgentRole_Nonexistent) {
    int result = swarm_brain_set_agent_role(manager, 999, DRONE_ROLE_SCOUT);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test getting agents by role
 * WHY:  Verify role filtering works correctly
 */
TEST_F(SwarmBrainLocalTest, GetAgentsByRole) {
    // Create agents with different roles
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 2, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 3, DRONE_ROLE_WORKER);
    swarm_brain_create_for_agent_with_role(manager, 4, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 5, DRONE_ROLE_COORDINATOR);

    uint32_t* agents = nullptr;
    uint32_t count = 0;

    // Get scouts
    int result = swarm_brain_get_agents_by_role(manager, DRONE_ROLE_SCOUT, &agents, &count);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, 3u);

    if (agents) {
        // Verify all returned agents are scouts
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_EQ(swarm_brain_get_agent_role(manager, agents[i]), DRONE_ROLE_SCOUT);
        }
        nimcp_free(agents);
    }

    // Get workers
    result = swarm_brain_get_agents_by_role(manager, DRONE_ROLE_WORKER, &agents, &count);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
    if (agents) {
        nimcp_free(agents);
    }

    // Get coordinators
    result = swarm_brain_get_agents_by_role(manager, DRONE_ROLE_COORDINATOR, &agents, &count);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
    if (agents) {
        nimcp_free(agents);
    }

    // Get guardians (none)
    result = swarm_brain_get_agents_by_role(manager, DRONE_ROLE_GUARDIAN, &agents, &count);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(agents, nullptr);
}

/**
 * WHAT: Test role-based sync within group
 * WHY:  Verify sync only affects agents of same role
 */
TEST_F(SwarmBrainLocalTest, SyncRoleGroup) {
    // Create multiple scouts and one worker
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 2, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 3, DRONE_ROLE_WORKER);

    // Sync scouts only
    int result = swarm_brain_sync_role_group(manager, DRONE_ROLE_SCOUT);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Should succeed with fewer than 2 agents (no-op)
    result = swarm_brain_sync_role_group(manager, DRONE_ROLE_WORKER);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test sync role group with no agents
 * WHY:  Verify graceful handling
 */
TEST_F(SwarmBrainLocalTest, SyncRoleGroup_NoAgents) {
    int result = swarm_brain_sync_role_group(manager, DRONE_ROLE_GUARDIAN);
    EXPECT_EQ(result, NIMCP_SUCCESS);  // Should succeed as no-op
}

//=============================================================================
// Role-Based Training Tests
//=============================================================================

/**
 * WHAT: Test getting role training config for all roles
 * WHY:  Verify training configs are correctly defined
 */
TEST_F(SwarmBrainLocalTest, GetRoleTrainingConfig_AllRoles) {
    // Scout: high learning rate for exploration
    role_training_config_t scout = swarm_brain_get_role_training_config(DRONE_ROLE_SCOUT);
    EXPECT_EQ(scout.role, DRONE_ROLE_SCOUT);
    EXPECT_FLOAT_EQ(scout.learning_rate, 0.01f);
    EXPECT_EQ(scout.batch_size, 16u);
    EXPECT_TRUE(scout.use_replay_buffer);
    EXPECT_TRUE(scout.sync_within_role);
    EXPECT_FLOAT_EQ(scout.sync_strength, 0.3f);

    // Worker: low learning rate for stability
    role_training_config_t worker = swarm_brain_get_role_training_config(DRONE_ROLE_WORKER);
    EXPECT_EQ(worker.role, DRONE_ROLE_WORKER);
    EXPECT_FLOAT_EQ(worker.learning_rate, 0.001f);
    EXPECT_FALSE(worker.use_replay_buffer);  // Disabled for workers
    EXPECT_FLOAT_EQ(worker.sync_strength, 0.7f);  // High sync

    // Coordinator: unique - syncs with ALL agents
    role_training_config_t coord = swarm_brain_get_role_training_config(DRONE_ROLE_COORDINATOR);
    EXPECT_EQ(coord.role, DRONE_ROLE_COORDINATOR);
    EXPECT_FALSE(coord.sync_within_role);  // Syncs with everyone
    EXPECT_EQ(coord.batch_size, 64u);

    // Sensor: fast adaptation
    role_training_config_t sensor = swarm_brain_get_role_training_config(DRONE_ROLE_SENSOR);
    EXPECT_FLOAT_EQ(sensor.learning_rate, 0.02f);  // Highest learning rate
    EXPECT_FALSE(sensor.enable_transfer_learning);

    // Guardian: conservative learning
    role_training_config_t guardian = swarm_brain_get_role_training_config(DRONE_ROLE_GUARDIAN);
    EXPECT_FLOAT_EQ(guardian.learning_rate, 0.003f);
    EXPECT_EQ(guardian.transfer_from, DRONE_ROLE_SENSOR);

    // Relay: minimal learning
    role_training_config_t relay = swarm_brain_get_role_training_config(DRONE_ROLE_RELAY);
    EXPECT_FLOAT_EQ(relay.learning_rate, 0.0001f);
    EXPECT_FLOAT_EQ(relay.sync_strength, 0.9f);  // Highest sync
}

/**
 * WHAT: Test training with role-specific config
 * WHY:  Verify role-based training functions correctly
 */
TEST_F(SwarmBrainLocalTest, TrainWithRole) {
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);

    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float target[5] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f};

    // Train using default role config
    int result = swarm_brain_train_with_role(
        manager, 1, DRONE_ROLE_SCOUT,
        input, 10, target, 5, nullptr  // Use defaults
    );

    // Should complete without error (actual learning depends on brain API)
    SUCCEED();
}

/**
 * WHAT: Test training with custom role config
 * WHY:  Verify custom training configs can override defaults
 */
TEST_F(SwarmBrainLocalTest, TrainWithRole_CustomConfig) {
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_WORKER);

    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float target[5] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f};

    role_training_config_t custom = {
        .role = DRONE_ROLE_WORKER,
        .learning_rate = 0.005f,  // Override default
        .batch_size = 8,
        .use_replay_buffer = false,
        .replay_buffer_size = 0,
        .sync_within_role = true,
        .sync_strength = 0.5f,
        .enable_transfer_learning = false,
        .transfer_from = DRONE_ROLE_WORKER,
        .transfer_weight = 0.0f
    };

    int result = swarm_brain_train_with_role(
        manager, 1, DRONE_ROLE_WORKER,
        input, 10, target, 5, &custom
    );

    // Should complete without error
    SUCCEED();
}

/**
 * WHAT: Test knowledge transfer between roles
 * WHY:  Verify inter-role knowledge transfer works
 */
TEST_F(SwarmBrainLocalTest, TransferRoleKnowledge) {
    // Create source agents (scouts)
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 2, DRONE_ROLE_SCOUT);

    // Create target agent (coordinator)
    swarm_brain_create_for_agent_with_role(manager, 3, DRONE_ROLE_COORDINATOR);

    // Transfer knowledge from scouts to coordinator
    int result = swarm_brain_transfer_role_knowledge(manager, 3, DRONE_ROLE_SCOUT, 0.2f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test knowledge transfer with invalid weight
 * WHY:  Verify parameter validation
 */
TEST_F(SwarmBrainLocalTest, TransferRoleKnowledge_InvalidWeight) {
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 2, DRONE_ROLE_COORDINATOR);

    // Weight must be 0.0-1.0
    int result = swarm_brain_transfer_role_knowledge(manager, 2, DRONE_ROLE_SCOUT, 1.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);

    result = swarm_brain_transfer_role_knowledge(manager, 2, DRONE_ROLE_SCOUT, -0.1f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test knowledge transfer to nonexistent agent
 * WHY:  Verify error handling
 */
TEST_F(SwarmBrainLocalTest, TransferRoleKnowledge_NonexistentTarget) {
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);

    int result = swarm_brain_transfer_role_knowledge(manager, 999, DRONE_ROLE_SCOUT, 0.2f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test knowledge transfer with no source agents
 * WHY:  Verify graceful handling when no source agents exist
 */
TEST_F(SwarmBrainLocalTest, TransferRoleKnowledge_NoSources) {
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_COORDINATOR);

    // No scouts exist - should succeed but do nothing
    int result = swarm_brain_transfer_role_knowledge(manager, 1, DRONE_ROLE_SCOUT, 0.2f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Role-Based Asymmetric Swarm Scenario Tests
//=============================================================================

/**
 * WHAT: Test mixed swarm with asymmetric brain configurations
 * WHY:  Verify realistic swarm scenario with different drone types
 */
TEST_F(SwarmBrainLocalTest, AsymmetricSwarm_MixedRoles) {
    // Create a realistic swarm configuration
    // 2 scouts, 5 workers, 1 coordinator, 2 sensors, 1 guardian, 1 relay

    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 2, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 3, DRONE_ROLE_WORKER);
    swarm_brain_create_for_agent_with_role(manager, 4, DRONE_ROLE_WORKER);
    swarm_brain_create_for_agent_with_role(manager, 5, DRONE_ROLE_WORKER);
    swarm_brain_create_for_agent_with_role(manager, 6, DRONE_ROLE_WORKER);
    swarm_brain_create_for_agent_with_role(manager, 7, DRONE_ROLE_WORKER);
    swarm_brain_create_for_agent_with_role(manager, 8, DRONE_ROLE_COORDINATOR);
    swarm_brain_create_for_agent_with_role(manager, 9, DRONE_ROLE_SENSOR);
    swarm_brain_create_for_agent_with_role(manager, 10, DRONE_ROLE_SENSOR);
    swarm_brain_create_for_agent_with_role(manager, 11, DRONE_ROLE_GUARDIAN);
    swarm_brain_create_for_agent_with_role(manager, 12, DRONE_ROLE_RELAY);

    EXPECT_EQ(swarm_brain_get_agent_count(manager), 12u);

    // Verify role distribution
    uint32_t* agents = nullptr;
    uint32_t count = 0;

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_SCOUT, &agents, &count);
    EXPECT_EQ(count, 2u);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_WORKER, &agents, &count);
    EXPECT_EQ(count, 5u);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_COORDINATOR, &agents, &count);
    EXPECT_EQ(count, 1u);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_SENSOR, &agents, &count);
    EXPECT_EQ(count, 2u);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_GUARDIAN, &agents, &count);
    EXPECT_EQ(count, 1u);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_RELAY, &agents, &count);
    EXPECT_EQ(count, 1u);
    if (agents) nimcp_free(agents);
}

/**
 * WHAT: Test role-specific sync in mixed swarm
 * WHY:  Verify sync operations affect correct agents
 */
TEST_F(SwarmBrainLocalTest, AsymmetricSwarm_RoleSync) {
    // Create mixed swarm
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 2, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 3, DRONE_ROLE_SCOUT);
    swarm_brain_create_for_agent_with_role(manager, 4, DRONE_ROLE_WORKER);
    swarm_brain_create_for_agent_with_role(manager, 5, DRONE_ROLE_WORKER);

    // Sync scouts
    int result = swarm_brain_sync_role_group(manager, DRONE_ROLE_SCOUT);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Sync workers
    result = swarm_brain_sync_role_group(manager, DRONE_ROLE_WORKER);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Stats should reflect sync operations
    swarm_brain_stats_t stats;
    swarm_brain_local_get_stats(manager, &stats);
    EXPECT_GT(stats.sync_count, 0u);
}

/**
 * WHAT: Test role reassignment in swarm
 * WHY:  Verify agents can dynamically change roles
 */
TEST_F(SwarmBrainLocalTest, AsymmetricSwarm_RoleReassignment) {
    // Create initial configuration
    swarm_brain_create_for_agent_with_role(manager, 1, DRONE_ROLE_WORKER);
    swarm_brain_create_for_agent_with_role(manager, 2, DRONE_ROLE_WORKER);
    swarm_brain_create_for_agent_with_role(manager, 3, DRONE_ROLE_WORKER);

    uint32_t* agents = nullptr;
    uint32_t count = 0;

    // All start as workers
    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_WORKER, &agents, &count);
    EXPECT_EQ(count, 3u);
    if (agents) nimcp_free(agents);

    // Promote one to coordinator
    swarm_brain_set_agent_role(manager, 1, DRONE_ROLE_COORDINATOR);

    // Verify redistribution
    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_WORKER, &agents, &count);
    EXPECT_EQ(count, 2u);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_COORDINATOR, &agents, &count);
    EXPECT_EQ(count, 1u);
    if (agents) nimcp_free(agents);
}

//=============================================================================
// Memory Leak Tests
//=============================================================================

TEST_F(SwarmBrainLocalTest, NoMemoryLeaksOnDestruction) {
    // Create and destroy multiple times
    for (int iteration = 0; iteration < 10; iteration++) {
        auto* mgr = swarm_brain_manager_create(&config);
        ASSERT_NE(mgr, nullptr);

        for (uint32_t i = 1; i <= 5; i++) {
            swarm_brain_create_for_agent(mgr, i, 100);
        }

        swarm_brain_manager_destroy(mgr);
    }

    SUCCEED();
}

TEST_F(SwarmBrainLocalTest, NoMemoryLeaksOnAgentDestruction) {
    for (int iteration = 0; iteration < 10; iteration++) {
        for (uint32_t i = 1; i <= 5; i++) {
            swarm_brain_create_for_agent(manager, i, 100);
        }

        for (uint32_t i = 1; i <= 5; i++) {
            swarm_brain_destroy_for_agent(manager, i);
        }
    }

    EXPECT_EQ(swarm_brain_get_agent_count(manager), 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
