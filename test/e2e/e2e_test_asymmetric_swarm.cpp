/**
 * @file e2e_test_asymmetric_swarm.cpp
 * @brief E2E Test for Asymmetric Swarm with Role-Based Brain Configurations
 *
 * WHAT: Complete end-to-end test of heterogeneous drone swarm with role-specific brains
 * WHY:  Verify memory optimization and role-based training works in realistic scenarios
 * HOW:  Simulate multi-role swarm operations with different brain configurations
 *
 * TEST SCENARIO:
 * 1. Initialize asymmetric swarm (scouts, workers, coordinator, sensors, guardians, relays)
 * 2. Verify memory savings from role-based brain configurations
 * 3. Train drones with role-specific learning rates
 * 4. Perform role-based weight synchronization
 * 5. Dynamic role reassignment during operation
 * 6. Inter-role knowledge transfer
 * 7. Verify swarm coherence and specialization
 *
 * @author NIMCP Development Team
 * @date 2025-12-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cmath>

// Headers already have their own extern "C" guards - don't wrap to avoid CUDA conflicts
#include "swarm/nimcp_swarm_brain_local.h"
#include "core/brain/factory/init/nimcp_brain_init_config.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AsymmetricSwarmE2ETest : public ::testing::Test {
protected:
    swarm_brain_manager_t* manager;
    swarm_brain_config_t config;

    // Swarm composition
    static constexpr uint32_t NUM_SCOUTS = 3;
    static constexpr uint32_t NUM_WORKERS = 10;
    static constexpr uint32_t NUM_COORDINATORS = 1;
    static constexpr uint32_t NUM_SENSORS = 4;
    static constexpr uint32_t NUM_GUARDIANS = 2;
    static constexpr uint32_t NUM_RELAYS = 2;
    static constexpr uint32_t TOTAL_AGENTS = NUM_SCOUTS + NUM_WORKERS + NUM_COORDINATORS +
                                              NUM_SENSORS + NUM_GUARDIANS + NUM_RELAYS;

    void SetUp() override {
        config = swarm_brain_local_default_config();
        config.enable_bio_async = false;  // Disable for deterministic testing
        config.test_mode = true;          // Skip actual brain creation for fast tests
        manager = swarm_brain_manager_create(&config);
        ASSERT_NE(manager, nullptr);
    }

    void TearDown() override {
        if (manager) {
            swarm_brain_manager_destroy(manager);
            manager = nullptr;
        }
    }

    // Helper: Create asymmetric swarm
    void CreateAsymmetricSwarm() {
        uint32_t agent_id = 1;

        // Create scouts (SMALL brains - navigation focused)
        for (uint32_t i = 0; i < NUM_SCOUTS; i++) {
            int result = swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_SCOUT);
            ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to create scout " << i;
        }

        // Create workers (MICRO brains - minimal, task-focused)
        for (uint32_t i = 0; i < NUM_WORKERS; i++) {
            int result = swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_WORKER);
            ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to create worker " << i;
        }

        // Create coordinator (MEDIUM brain - full cognitive capabilities)
        for (uint32_t i = 0; i < NUM_COORDINATORS; i++) {
            int result = swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_COORDINATOR);
            ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to create coordinator " << i;
        }

        // Create sensors (TINY brains - perception focused)
        for (uint32_t i = 0; i < NUM_SENSORS; i++) {
            int result = swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_SENSOR);
            ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to create sensor " << i;
        }

        // Create guardians (SMALL brains - security focused)
        for (uint32_t i = 0; i < NUM_GUARDIANS; i++) {
            int result = swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_GUARDIAN);
            ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to create guardian " << i;
        }

        // Create relays (MICRO brains - minimal, communication focused)
        for (uint32_t i = 0; i < NUM_RELAYS; i++) {
            int result = swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_RELAY);
            ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to create relay " << i;
        }

        ASSERT_EQ(swarm_brain_get_agent_count(manager), TOTAL_AGENTS);
    }

    // Helper: Simulate training cycle for all agents
    void SimulateTrainingCycle() {
        float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
        float target[5] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f};

        // Train each role group
        drone_role_t roles[] = {
            DRONE_ROLE_SCOUT, DRONE_ROLE_WORKER, DRONE_ROLE_COORDINATOR,
            DRONE_ROLE_SENSOR, DRONE_ROLE_GUARDIAN, DRONE_ROLE_RELAY
        };

        for (int r = 0; r < 6; r++) {
            uint32_t* agents = nullptr;
            uint32_t count = 0;
            swarm_brain_get_agents_by_role(manager, roles[r], &agents, &count);

            for (uint32_t i = 0; i < count; i++) {
                swarm_brain_train_with_role(manager, agents[i], roles[r],
                                           input, 10, target, 5, nullptr);
            }

            if (agents) nimcp_free(agents);
        }
    }
};

//=============================================================================
// E2E Scenario Tests
//=============================================================================

/**
 * SCENARIO: Complete Asymmetric Swarm Lifecycle
 *
 * Tests the full lifecycle of an asymmetric drone swarm:
 * 1. Creation with role-based brain configurations
 * 2. Role distribution verification
 * 3. Training with role-specific parameters
 * 4. Role-based synchronization
 * 5. Graceful shutdown
 */
TEST_F(AsymmetricSwarmE2ETest, CompleteSwarmLifecycle) {
    // Phase 1: Create asymmetric swarm
    CreateAsymmetricSwarm();

    // Phase 2: Verify role distribution
    uint32_t* agents = nullptr;
    uint32_t count = 0;

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_SCOUT, &agents, &count);
    EXPECT_EQ(count, NUM_SCOUTS);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_WORKER, &agents, &count);
    EXPECT_EQ(count, NUM_WORKERS);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_COORDINATOR, &agents, &count);
    EXPECT_EQ(count, NUM_COORDINATORS);
    if (agents) nimcp_free(agents);

    // Phase 3: Simulate training cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        SimulateTrainingCycle();
    }

    // Phase 4: Role-based synchronization
    EXPECT_EQ(swarm_brain_sync_role_group(manager, DRONE_ROLE_SCOUT), NIMCP_SUCCESS);
    EXPECT_EQ(swarm_brain_sync_role_group(manager, DRONE_ROLE_WORKER), NIMCP_SUCCESS);
    EXPECT_EQ(swarm_brain_sync_role_group(manager, DRONE_ROLE_SENSOR), NIMCP_SUCCESS);

    // Phase 5: Verify stats
    swarm_brain_stats_t stats;
    EXPECT_EQ(swarm_brain_local_get_stats(manager, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.num_agents, TOTAL_AGENTS);
    EXPECT_GT(stats.sync_count, 0u);

    // Phase 6: Cleanup is handled by TearDown
    SUCCEED();
}

/**
 * SCENARIO: Memory Optimization Verification
 *
 * Verifies that role-based brain configurations provide expected memory savings:
 * - Workers/Relays use MICRO brains (~25 neurons)
 * - Sensors use TINY brains (~100 neurons)
 * - Scouts/Guardians use SMALL brains (~500 neurons)
 * - Coordinators use MEDIUM brains (~1000 neurons)
 */
TEST_F(AsymmetricSwarmE2ETest, MemoryOptimizationVerification) {
    // Verify brain size presets
    uint32_t micro_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MICRO);
    uint32_t tiny_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY);
    uint32_t small_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL);
    uint32_t medium_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM);

    EXPECT_EQ(micro_neurons, 25u);
    EXPECT_EQ(tiny_neurons, 100u);
    EXPECT_EQ(small_neurons, 500u);
    EXPECT_EQ(medium_neurons, 1000u);

    // Verify role templates use expected brain sizes
    drone_brain_template_t scout_templ = swarm_brain_get_role_template(DRONE_ROLE_SCOUT);
    drone_brain_template_t worker_templ = swarm_brain_get_role_template(DRONE_ROLE_WORKER);
    drone_brain_template_t coord_templ = swarm_brain_get_role_template(DRONE_ROLE_COORDINATOR);
    drone_brain_template_t sensor_templ = swarm_brain_get_role_template(DRONE_ROLE_SENSOR);
    drone_brain_template_t guardian_templ = swarm_brain_get_role_template(DRONE_ROLE_GUARDIAN);
    drone_brain_template_t relay_templ = swarm_brain_get_role_template(DRONE_ROLE_RELAY);

    EXPECT_EQ(scout_templ.brain_size, BRAIN_SIZE_SMALL);
    EXPECT_EQ(worker_templ.brain_size, BRAIN_SIZE_MICRO);
    EXPECT_EQ(coord_templ.brain_size, BRAIN_SIZE_MEDIUM);
    EXPECT_EQ(sensor_templ.brain_size, BRAIN_SIZE_TINY);
    EXPECT_EQ(guardian_templ.brain_size, BRAIN_SIZE_SMALL);
    EXPECT_EQ(relay_templ.brain_size, BRAIN_SIZE_MICRO);

    // Calculate theoretical memory savings
    // Uniform swarm: 22 * 1000 neurons = 22000 neurons
    // Asymmetric swarm:
    //   3 scouts * 500 = 1500
    //   10 workers * 25 = 250
    //   1 coordinator * 1000 = 1000
    //   4 sensors * 100 = 400
    //   2 guardians * 500 = 1000
    //   2 relays * 25 = 50
    //   Total: 4200 neurons

    uint32_t uniform_neurons = TOTAL_AGENTS * medium_neurons;  // 22000
    uint32_t asymmetric_neurons =
        NUM_SCOUTS * small_neurons +       // 1500
        NUM_WORKERS * micro_neurons +      // 250
        NUM_COORDINATORS * medium_neurons + // 1000
        NUM_SENSORS * tiny_neurons +       // 400
        NUM_GUARDIANS * small_neurons +    // 1000
        NUM_RELAYS * micro_neurons;        // 50

    // Verify significant memory savings (>80% reduction)
    float savings = 1.0f - (float)asymmetric_neurons / (float)uniform_neurons;
    EXPECT_GT(savings, 0.80f) << "Expected >80% memory reduction, got " << (savings * 100) << "%";
}

/**
 * SCENARIO: Role-Specific Training Behavior
 *
 * Verifies that different roles have appropriate training configurations:
 * - Scouts: High learning rate (0.01) for exploration
 * - Workers: Low learning rate (0.001) for stability
 * - Coordinator: Moderate learning rate (0.005), syncs with all
 * - Sensors: Fast adaptation (0.02)
 * - Guardians: Conservative (0.003)
 * - Relays: Minimal learning (0.0001)
 */
TEST_F(AsymmetricSwarmE2ETest, RoleSpecificTrainingBehavior) {
    role_training_config_t scout_cfg = swarm_brain_get_role_training_config(DRONE_ROLE_SCOUT);
    role_training_config_t worker_cfg = swarm_brain_get_role_training_config(DRONE_ROLE_WORKER);
    role_training_config_t coord_cfg = swarm_brain_get_role_training_config(DRONE_ROLE_COORDINATOR);
    role_training_config_t sensor_cfg = swarm_brain_get_role_training_config(DRONE_ROLE_SENSOR);
    role_training_config_t guardian_cfg = swarm_brain_get_role_training_config(DRONE_ROLE_GUARDIAN);
    role_training_config_t relay_cfg = swarm_brain_get_role_training_config(DRONE_ROLE_RELAY);

    // Verify learning rate hierarchy
    EXPECT_GT(sensor_cfg.learning_rate, scout_cfg.learning_rate);    // Sensor > Scout
    EXPECT_GT(scout_cfg.learning_rate, coord_cfg.learning_rate);     // Scout > Coordinator
    EXPECT_GT(coord_cfg.learning_rate, guardian_cfg.learning_rate);  // Coordinator > Guardian
    EXPECT_GT(guardian_cfg.learning_rate, worker_cfg.learning_rate); // Guardian > Worker
    EXPECT_GT(worker_cfg.learning_rate, relay_cfg.learning_rate);    // Worker > Relay

    // Verify sync behaviors
    EXPECT_TRUE(scout_cfg.sync_within_role);     // Scouts sync within role
    EXPECT_TRUE(worker_cfg.sync_within_role);    // Workers sync within role
    EXPECT_FALSE(coord_cfg.sync_within_role);    // Coordinator syncs with ALL
    EXPECT_TRUE(sensor_cfg.sync_within_role);    // Sensors sync within role
    EXPECT_TRUE(guardian_cfg.sync_within_role);  // Guardians sync within role
    EXPECT_TRUE(relay_cfg.sync_within_role);     // Relays sync within role

    // Verify sync strength hierarchy
    EXPECT_GT(relay_cfg.sync_strength, worker_cfg.sync_strength);     // Relays most synchronized
    EXPECT_GT(worker_cfg.sync_strength, guardian_cfg.sync_strength);  // Workers highly synchronized
    EXPECT_GT(guardian_cfg.sync_strength, sensor_cfg.sync_strength);  // Guardians moderate
    EXPECT_GT(sensor_cfg.sync_strength, scout_cfg.sync_strength);     // Scouts less synchronized
}

/**
 * SCENARIO: Inter-Role Knowledge Transfer
 *
 * Tests knowledge transfer between roles:
 * - Guardians learn from Sensors (threat detection)
 * - Workers learn from Scouts (navigation hints)
 * - Scouts learn from Coordinator (strategic guidance)
 */
TEST_F(AsymmetricSwarmE2ETest, InterRoleKnowledgeTransfer) {
    CreateAsymmetricSwarm();

    // Verify transfer configurations
    role_training_config_t guardian_cfg = swarm_brain_get_role_training_config(DRONE_ROLE_GUARDIAN);
    EXPECT_EQ(guardian_cfg.transfer_from, DRONE_ROLE_SENSOR);
    EXPECT_TRUE(guardian_cfg.enable_transfer_learning);

    role_training_config_t worker_cfg = swarm_brain_get_role_training_config(DRONE_ROLE_WORKER);
    EXPECT_EQ(worker_cfg.transfer_from, DRONE_ROLE_SCOUT);
    EXPECT_TRUE(worker_cfg.enable_transfer_learning);

    role_training_config_t scout_cfg = swarm_brain_get_role_training_config(DRONE_ROLE_SCOUT);
    EXPECT_EQ(scout_cfg.transfer_from, DRONE_ROLE_COORDINATOR);
    EXPECT_TRUE(scout_cfg.enable_transfer_learning);

    // Get agents by role
    uint32_t* guardians = nullptr;
    uint32_t guardian_count = 0;
    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_GUARDIAN, &guardians, &guardian_count);
    ASSERT_GT(guardian_count, 0u);

    // Perform knowledge transfer from sensors to guardians
    for (uint32_t i = 0; i < guardian_count; i++) {
        int result = swarm_brain_transfer_role_knowledge(
            manager, guardians[i], DRONE_ROLE_SENSOR, guardian_cfg.transfer_weight
        );
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    if (guardians) nimcp_free(guardians);
}

/**
 * SCENARIO: Dynamic Role Reassignment During Operation
 *
 * Tests that roles can be dynamically reassigned during swarm operation:
 * 1. Start with default configuration
 * 2. Promote workers to scouts when exploration needed
 * 3. Demote scouts to workers when consolidating
 * 4. Promote a sensor to guardian when threat detected
 */
TEST_F(AsymmetricSwarmE2ETest, DynamicRoleReassignment) {
    CreateAsymmetricSwarm();

    uint32_t* agents = nullptr;
    uint32_t count = 0;

    // Get initial worker list
    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_WORKER, &agents, &count);
    ASSERT_EQ(count, NUM_WORKERS);
    uint32_t worker_to_promote = agents[0];
    if (agents) nimcp_free(agents);

    // Promote first worker to scout
    EXPECT_EQ(swarm_brain_set_agent_role(manager, worker_to_promote, DRONE_ROLE_SCOUT), NIMCP_SUCCESS);

    // Verify role changed
    EXPECT_EQ(swarm_brain_get_agent_role(manager, worker_to_promote), DRONE_ROLE_SCOUT);

    // Verify counts updated
    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_WORKER, &agents, &count);
    EXPECT_EQ(count, NUM_WORKERS - 1);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_SCOUT, &agents, &count);
    EXPECT_EQ(count, NUM_SCOUTS + 1);
    if (agents) nimcp_free(agents);

    // Sync the new scout with its role group
    EXPECT_EQ(swarm_brain_sync_role_group(manager, DRONE_ROLE_SCOUT), NIMCP_SUCCESS);

    // Get a sensor and promote to guardian
    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_SENSOR, &agents, &count);
    ASSERT_GT(count, 0u);
    uint32_t sensor_to_promote = agents[0];
    if (agents) nimcp_free(agents);

    EXPECT_EQ(swarm_brain_set_agent_role(manager, sensor_to_promote, DRONE_ROLE_GUARDIAN), NIMCP_SUCCESS);
    EXPECT_EQ(swarm_brain_get_agent_role(manager, sensor_to_promote), DRONE_ROLE_GUARDIAN);

    // Verify final counts
    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_SENSOR, &agents, &count);
    EXPECT_EQ(count, NUM_SENSORS - 1);
    if (agents) nimcp_free(agents);

    swarm_brain_get_agents_by_role(manager, DRONE_ROLE_GUARDIAN, &agents, &count);
    EXPECT_EQ(count, NUM_GUARDIANS + 1);
    if (agents) nimcp_free(agents);
}

/**
 * SCENARIO: Swarm Coherence Under Heterogeneous Training
 *
 * Tests that the swarm maintains coherence when different roles
 * train with different learning rates and sync strategies.
 */
TEST_F(AsymmetricSwarmE2ETest, SwarmCoherenceUnderHeterogeneousTraining) {
    CreateAsymmetricSwarm();

    // Run multiple training and sync cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // Train all agents with role-specific configs
        SimulateTrainingCycle();

        // Sync role groups
        swarm_brain_sync_role_group(manager, DRONE_ROLE_SCOUT);
        swarm_brain_sync_role_group(manager, DRONE_ROLE_WORKER);
        swarm_brain_sync_role_group(manager, DRONE_ROLE_SENSOR);
        swarm_brain_sync_role_group(manager, DRONE_ROLE_GUARDIAN);
        swarm_brain_sync_role_group(manager, DRONE_ROLE_RELAY);

        // Coordinator syncs with all (sync_within_role = false)
        // This is handled internally by the training function
    }

    // Verify swarm is still coherent
    swarm_brain_stats_t stats;
    EXPECT_EQ(swarm_brain_local_get_stats(manager, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.num_agents, TOTAL_AGENTS);

    // Verify all agents still active
    for (uint32_t i = 1; i <= TOTAL_AGENTS; i++) {
        EXPECT_TRUE(swarm_brain_has_agent(manager, i));
    }
}

/**
 * SCENARIO: Performance Under Scale
 *
 * Tests swarm performance with larger agent counts to verify
 * memory optimization benefits at scale.
 */
TEST_F(AsymmetricSwarmE2ETest, PerformanceUnderScale) {
    // Create large swarm: 100 agents
    // 10 scouts, 60 workers, 5 coordinators, 15 sensors, 5 guardians, 5 relays
    const uint32_t LARGE_SCOUTS = 10;
    const uint32_t LARGE_WORKERS = 60;
    const uint32_t LARGE_COORDINATORS = 5;
    const uint32_t LARGE_SENSORS = 15;
    const uint32_t LARGE_GUARDIANS = 5;
    const uint32_t LARGE_RELAYS = 5;
    const uint32_t LARGE_TOTAL = 100;

    uint64_t start_time = nimcp_time_get_us();
    uint32_t agent_id = 1;

    for (uint32_t i = 0; i < LARGE_SCOUTS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_SCOUT);
    }
    for (uint32_t i = 0; i < LARGE_WORKERS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_WORKER);
    }
    for (uint32_t i = 0; i < LARGE_COORDINATORS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_COORDINATOR);
    }
    for (uint32_t i = 0; i < LARGE_SENSORS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_SENSOR);
    }
    for (uint32_t i = 0; i < LARGE_GUARDIANS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_GUARDIAN);
    }
    for (uint32_t i = 0; i < LARGE_RELAYS; i++) {
        swarm_brain_create_for_agent_with_role(manager, agent_id++, DRONE_ROLE_RELAY);
    }

    uint64_t creation_time = nimcp_time_get_us() - start_time;

    EXPECT_EQ(swarm_brain_get_agent_count(manager), LARGE_TOTAL);

    // Creation should complete reasonably fast (< 30 seconds for 100 agents)
    // Note: With memory optimization, this should be faster than uniform brains
    EXPECT_LT(creation_time, 30000000u);  // 30 seconds

    // Sync all role groups
    start_time = nimcp_time_get_us();
    swarm_brain_sync_role_group(manager, DRONE_ROLE_SCOUT);
    swarm_brain_sync_role_group(manager, DRONE_ROLE_WORKER);
    swarm_brain_sync_role_group(manager, DRONE_ROLE_COORDINATOR);
    swarm_brain_sync_role_group(manager, DRONE_ROLE_SENSOR);
    swarm_brain_sync_role_group(manager, DRONE_ROLE_GUARDIAN);
    swarm_brain_sync_role_group(manager, DRONE_ROLE_RELAY);
    uint64_t sync_time = nimcp_time_get_us() - start_time;

    // Syncing should be fast (< 5 seconds)
    EXPECT_LT(sync_time, 5000000u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
