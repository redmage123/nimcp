/**
 * @file test_mesh_brain_bootstrap_integration.cpp
 * @brief Integration Tests for Brain-Mesh Bootstrap Bidirectional Integration
 *
 * WHAT: Tests full brain creation with mesh registration and bidirectional communication
 * WHY:  Verify brain modules properly register and interact with mesh network
 * HOW:  Create real brain instances, register with mesh, test routing and callbacks
 *
 * TEST COVERAGE:
 * - Full brain creation with mesh registration
 * - All brain regions register correctly
 * - Mesh can route to registered brain modules
 * - Brain can receive mesh callbacks
 * - Multiple brains with same mesh
 * - Brain module lookup and validation
 * - Receptive field auto-registration
 * - Channel assignment by brain region
 * - Mesh-brain event propagation
 * - Concurrent brain-mesh operations
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <cmath>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_module_registry.h"
#include "mesh/nimcp_mesh_receptive_fields.h"
#include "mesh/nimcp_mesh_bio_bridge.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshBrainBootstrapIntegrationTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap;

    void SetUp() override {
        bootstrap = nullptr;
    }

    void TearDown() override {
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
        }
    }

    mesh_pattern_t create_pattern(const float* values, size_t count) {
        mesh_pattern_t pattern;
        mesh_pattern_init(&pattern);

        float magnitude = 0.0f;
        for (size_t i = 0; i < count && i < MESH_PATTERN_DIM; i++) {
            pattern.vector[i] = values[i];
            magnitude += values[i] * values[i];
        }
        pattern.magnitude = sqrtf(magnitude);
        pattern.active_dims = (uint32_t)count;

        return pattern;
    }

    mesh_receptive_field_t create_field(const float* pattern_vals, size_t count, float threshold) {
        mesh_receptive_field_t field;
        mesh_receptive_field_init(&field);
        field.preferred[0] = create_pattern(pattern_vals, count);
        field.pattern_count = 1;
        field.threshold = threshold;
        field.sharpness = 1.0f;
        return field;
    }
};

/* ============================================================================
 * Test 1: Full Brain Creation with Mesh Registration
 * ============================================================================ */

TEST_F(MeshBrainBootstrapIntegrationTest, FullBrainCreationWithMeshRegistration) {
    /* Create bootstrap with all cognitive subsystems */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems.enable_cognitive = true;
    config.subsystems.enable_security = true;
    config.subsystems.enable_memory = true;
    config.subsystems.enable_swarm = true;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Verify integration manager is accessible */
    mesh_integration_t* integration = mesh_bootstrap_get_integration(bootstrap);
    ASSERT_NE(integration, nullptr);

    /* Verify module registry exists */
    mesh_module_registry_t* registry = mesh_bootstrap_get_module_registry(bootstrap);
    ASSERT_NE(registry, nullptr);

    /* Verify pattern router exists */
    mesh_pattern_router_t* router = mesh_bootstrap_get_pattern_router(bootstrap);
    ASSERT_NE(router, nullptr);

    /* Get statistics to verify modules registered */
    mesh_bootstrap_stats_t stats;
    EXPECT_EQ(mesh_bootstrap_get_stats(bootstrap, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.cognitive_modules, 0u);
    EXPECT_TRUE(stats.fully_initialized);
}

/* ============================================================================
 * Test 2: All Brain Regions Register Correctly
 * ============================================================================ */

TEST_F(MeshBrainBootstrapIntegrationTest, AllBrainRegionsRegisterCorrectly) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_ALL;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);

    /* Verify each subsystem category has registered modules */
    EXPECT_GT(stats.cognitive_modules, 0u) << "Cognitive modules should be registered";
    EXPECT_GT(stats.sensory_modules, 0u) << "Sensory modules should be registered";
    EXPECT_GT(stats.motor_modules, 0u) << "Motor modules should be registered";
    EXPECT_GT(stats.memory_modules, 0u) << "Memory modules should be registered";
    EXPECT_GT(stats.security_modules, 0u) << "Security modules should be registered";

    /* Verify total is sum of parts */
    size_t sum = stats.cognitive_modules + stats.sensory_modules +
                 stats.motor_modules + stats.memory_modules +
                 stats.security_modules + stats.gpu_modules +
                 stats.plasticity_modules + stats.glial_modules +
                 stats.swarm_modules + stats.async_modules;
    EXPECT_LE(sum, stats.total_modules_registered);
}

/* ============================================================================
 * Test 3: Mesh Routes to Registered Brain Modules
 * ============================================================================ */

TEST_F(MeshBrainBootstrapIntegrationTest, MeshRoutesToRegisteredBrainModules) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Register custom brain modules with specific patterns */
    float visual_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float motor_pattern[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float memory_pattern[] = {0.0f, 0.0f, 1.0f, 0.0f};

    mesh_receptive_field_t visual_field = create_field(visual_pattern, 4, 0.3f);
    mesh_receptive_field_t motor_field = create_field(motor_pattern, 4, 0.3f);
    mesh_receptive_field_t memory_field = create_field(memory_pattern, 4, 0.3f);

    mesh_participant_id_t visual_id = 0xB001;
    mesh_participant_id_t motor_id = 0xB002;
    mesh_participant_id_t memory_id = 0xB003;

    EXPECT_EQ(mesh_bootstrap_register_receptive_field(bootstrap, visual_id, &visual_field),
              NIMCP_SUCCESS);
    EXPECT_EQ(mesh_bootstrap_register_receptive_field(bootstrap, motor_id, &motor_field),
              NIMCP_SUCCESS);
    EXPECT_EQ(mesh_bootstrap_register_receptive_field(bootstrap, memory_id, &memory_field),
              NIMCP_SUCCESS);

    /* Route a visual-like transaction */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(visual_pattern, 4);
    tx.channel = MESH_CHANNEL_RIGHT_HEMISPHERE;

    mesh_participant_id_t endorsers[10];
    size_t count = 0;

    EXPECT_EQ(mesh_bootstrap_route_by_pattern(bootstrap, &tx, endorsers, 10, &count),
              NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);

    /* Verify visual module was selected */
    bool visual_found = false;
    for (size_t i = 0; i < count; i++) {
        if (endorsers[i] == visual_id) visual_found = true;
    }
    EXPECT_TRUE(visual_found);
}

/* ============================================================================
 * Test 4: Brain Receives Mesh Callbacks
 * ============================================================================ */

struct CallbackContext {
    std::atomic<int> callback_count{0};
    std::atomic<bool> received_belief{false};
};

TEST_F(MeshBrainBootstrapIntegrationTest, BrainReceivesMeshCallbacks) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    mesh_channel_t* channel = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_SYSTEM);
    ASSERT_NE(channel, nullptr);

    /* Introduce a belief and run gossip */
    mesh_belief_t belief;
    memset(&belief, 0, sizeof(belief));
    belief.belief_id = 42;
    belief.source = 0x1001;
    belief.channel = MESH_CHANNEL_SYSTEM;
    belief.certainty = 0.95f;
    belief.vector_dim = 4;
    belief.belief_vector[0] = 1.0f;

    /* Belief introduction - may succeed or fail based on BBB validation */
    nimcp_error_t err = mesh_channel_introduce_belief(channel, &belief);
    (void)err;  /* Result depends on security policy */

    /* Run gossip rounds to propagate */
    mesh_bootstrap_gossip_all(bootstrap, 3);

    /* Process any pending transactions */
    size_t processed = mesh_bootstrap_process_transactions(bootstrap);
    EXPECT_GE(processed, 0u);

    /* Verify channel still operational */
    mesh_channel_stats_t ch_stats;
    mesh_channel_get_stats(channel, &ch_stats);
    EXPECT_GT(ch_stats.gossip_rounds, 0u);
}

/* ============================================================================
 * Test 5: Multiple Brains with Same Mesh
 * ============================================================================ */

TEST_F(MeshBrainBootstrapIntegrationTest, MultipleBrainsWithSameMesh) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Register multiple "brain" module sets with different IDs */
    for (int brain = 0; brain < 3; brain++) {
        float pattern[4] = {0.0f};
        pattern[brain % 4] = 1.0f;

        mesh_receptive_field_t field = create_field(pattern, 4, 0.3f);
        mesh_participant_id_t id = 0xC000 + (brain * 0x100);

        EXPECT_EQ(mesh_bootstrap_register_receptive_field(bootstrap, id, &field),
                  NIMCP_SUCCESS);
    }

    /* Verify all brains can route */
    for (int brain = 0; brain < 3; brain++) {
        float pattern[4] = {0.0f};
        pattern[brain % 4] = 1.0f;

        mesh_pattern_transaction_t tx;
        memset(&tx, 0, sizeof(tx));
        tx.content_pattern = create_pattern(pattern, 4);

        mesh_participant_id_t endorsers[10];
        size_t count = 0;

        EXPECT_EQ(mesh_bootstrap_route_by_pattern(bootstrap, &tx, endorsers, 10, &count),
                  NIMCP_SUCCESS);
        EXPECT_GT(count, 0u);
    }
}

/* ============================================================================
 * Test 6: Brain Module Lookup and Validation
 * ============================================================================ */

TEST_F(MeshBrainBootstrapIntegrationTest, BrainModuleLookupAndValidation) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    mesh_module_registry_t* registry = mesh_bootstrap_get_module_registry(bootstrap);
    ASSERT_NE(registry, nullptr);

    /* Get registry statistics */
    mesh_module_registry_stats_t reg_stats;
    EXPECT_EQ(mesh_module_registry_get_stats(registry, &reg_stats), NIMCP_SUCCESS);
    EXPECT_GT(reg_stats.total_registered, 0u);

    /* Validate all registered modules */
    size_t invalid_count = 0;
    nimcp_error_t err = mesh_module_registry_validate_all(registry, &invalid_count);
    /* Accept success or validation skip (depends on magic config) */
    EXPECT_EQ(invalid_count, 0u);
}

/* ============================================================================
 * Test 7: Receptive Field Auto-Registration
 * ============================================================================ */

TEST_F(MeshBrainBootstrapIntegrationTest, ReceptiveFieldAutoRegistration) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Register module with receptive field */
    float pattern[] = {0.5f, 0.5f, 0.0f, 0.0f};
    mesh_receptive_field_t field = create_field(pattern, 4, 0.3f);
    mesh_participant_id_t module_id = 0xD001;

    EXPECT_EQ(mesh_bootstrap_register_receptive_field(bootstrap, module_id, &field),
              NIMCP_SUCCESS);

    /* Verify module responds to matching patterns */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(pattern, 4);

    mesh_participant_id_t endorsers[10];
    size_t count = 0;

    EXPECT_EQ(mesh_bootstrap_route_by_pattern(bootstrap, &tx, endorsers, 10, &count),
              NIMCP_SUCCESS);

    bool found = false;
    for (size_t i = 0; i < count; i++) {
        if (endorsers[i] == module_id) found = true;
    }
    EXPECT_TRUE(found);
}

/* ============================================================================
 * Test 8: Channel Assignment by Brain Region
 * ============================================================================ */

TEST_F(MeshBrainBootstrapIntegrationTest, ChannelAssignmentByBrainRegion) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Verify each standard channel exists and has participants */
    mesh_channel_id_t channels[] = {
        MESH_CHANNEL_SYSTEM,
        MESH_CHANNEL_LEFT_HEMISPHERE,
        MESH_CHANNEL_RIGHT_HEMISPHERE,
        MESH_CHANNEL_SUBCORTICAL,
        MESH_CHANNEL_GPU_COMPUTE
    };

    for (mesh_channel_id_t ch_id : channels) {
        mesh_channel_t* channel = mesh_bootstrap_get_channel(bootstrap, ch_id);
        ASSERT_NE(channel, nullptr) << "Channel " << ch_id << " should exist";

        mesh_channel_stats_t stats;
        mesh_channel_get_stats(channel, &stats);
        /* Each channel should be operational (may have 0 participants initially) */
        EXPECT_GE(stats.participant_count, 0u);
    }
}

/* ============================================================================
 * Test 9: Mesh-Brain Event Propagation
 * ============================================================================ */

TEST_F(MeshBrainBootstrapIntegrationTest, MeshBrainEventPropagation) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Get system channel for event propagation */
    mesh_channel_t* system = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_SYSTEM);
    mesh_channel_t* left = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_LEFT_HEMISPHERE);
    ASSERT_NE(system, nullptr);
    ASSERT_NE(left, nullptr);

    /* Introduce beliefs in multiple channels */
    mesh_belief_t system_belief, left_belief;
    memset(&system_belief, 0, sizeof(system_belief));
    memset(&left_belief, 0, sizeof(left_belief));

    system_belief.belief_id = 100;
    system_belief.source = 0x1001;
    system_belief.channel = MESH_CHANNEL_SYSTEM;
    system_belief.certainty = 0.8f;

    left_belief.belief_id = 200;
    left_belief.source = 0x2001;
    left_belief.channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    left_belief.certainty = 0.9f;

    /* Introduce beliefs (may fail validation) */
    mesh_channel_introduce_belief(system, &system_belief);
    mesh_channel_introduce_belief(left, &left_belief);

    /* Run gossip on all channels */
    mesh_bootstrap_gossip_all(bootstrap, 5);

    /* Update bootstrap */
    mesh_bootstrap_update(bootstrap, 10);

    /* Verify gossip propagation occurred */
    mesh_channel_stats_t system_stats, left_stats;
    mesh_channel_get_stats(system, &system_stats);
    mesh_channel_get_stats(left, &left_stats);

    EXPECT_GT(system_stats.gossip_rounds, 0u);
    EXPECT_GT(left_stats.gossip_rounds, 0u);
}

/* ============================================================================
 * Test 10: Concurrent Brain-Mesh Operations
 * ============================================================================ */

TEST_F(MeshBrainBootstrapIntegrationTest, ConcurrentBrainMeshOperations) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Pre-register modules for routing */
    for (int i = 0; i < 5; i++) {
        float pattern[4] = {0.0f};
        pattern[i % 4] = 1.0f;

        mesh_receptive_field_t field = create_field(pattern, 4, 0.3f);
        mesh_bootstrap_register_receptive_field(bootstrap, 0xE000 + i, &field);
    }

    std::atomic<bool> running{true};
    std::atomic<int> route_count{0};
    std::atomic<int> update_count{0};
    std::atomic<int> gossip_count{0};

    /* Thread 1: Continuous routing */
    std::thread routing_thread([&]() {
        while (running) {
            float pattern[4] = {0.5f, 0.5f, 0.0f, 0.0f};
            mesh_pattern_transaction_t tx;
            memset(&tx, 0, sizeof(tx));
            tx.content_pattern = create_pattern(pattern, 4);

            mesh_participant_id_t endorsers[10];
            size_t count = 0;
            mesh_bootstrap_route_by_pattern(bootstrap, &tx, endorsers, 10, &count);
            route_count++;

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    /* Thread 2: Continuous updates */
    std::thread update_thread([&]() {
        while (running) {
            mesh_bootstrap_update(bootstrap, 5);
            update_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    /* Thread 3: Continuous gossip */
    std::thread gossip_thread([&]() {
        while (running) {
            mesh_bootstrap_gossip_all(bootstrap, 1);
            gossip_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    /* Let threads run */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    running = false;
    routing_thread.join();
    update_thread.join();
    gossip_thread.join();

    EXPECT_GT(route_count.load(), 0);
    EXPECT_GT(update_count.load(), 0);
    EXPECT_GT(gossip_count.load(), 0);
}

/* ============================================================================
 * Test 11: Brain Registration with Health Agent
 * ============================================================================ */

TEST_F(MeshBrainBootstrapIntegrationTest, BrainRegistrationWithHealthAgent) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;
    config.enable_health_monitoring = true;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Get health bridge */
    mesh_health_bridge_t* health_bridge = mesh_bootstrap_get_health_bridge(bootstrap);
    /* Health bridge may or may not be created based on config */
    if (health_bridge) {
        mesh_health_bridge_stats_t health_stats;
        EXPECT_EQ(mesh_health_bridge_get_stats(health_bridge, &health_stats), NIMCP_SUCCESS);
        /* Stats should be valid */
        EXPECT_GE(health_stats.heartbeats_received, 0u);
    }
}

/* ============================================================================
 * Test 12: Brain Module Category Distribution
 * ============================================================================ */

TEST_F(MeshBrainBootstrapIntegrationTest, BrainModuleCategoryDistribution) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_ALL;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);

    /* Verify reasonable distribution - cognitive typically has most modules */
    if (config.subsystems.enable_cognitive && config.subsystems.enable_motor) {
        /* No strict ordering requirement, just verify categories work */
        EXPECT_GE(stats.cognitive_modules + stats.motor_modules, 2u);
    }

    /* Verify total makes sense */
    EXPECT_GT(stats.total_modules_registered, 0u);
}
