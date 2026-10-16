/**
 * @file test_mesh_bootstrap.cpp
 * @brief Unit tests for NIMCP Mesh Network Bootstrap System
 *
 * Tests the complete system bootstrap that integrates all NIMCP
 * modules into the unified mesh network.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_types.h"
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST(MeshBootstrapConfigTest, DefaultConfig) {
    mesh_bootstrap_config_t config;
    nimcp_error_t err = mesh_bootstrap_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(config.subsystems.enable_cognitive);
    EXPECT_TRUE(config.subsystems.enable_sensory);
    EXPECT_TRUE(config.subsystems.enable_motor);
    EXPECT_TRUE(config.subsystems.enable_memory);
    EXPECT_TRUE(config.subsystems.enable_security);
    EXPECT_TRUE(config.subsystems.enable_gpu);
    EXPECT_TRUE(config.subsystems.enable_plasticity);
    EXPECT_TRUE(config.subsystems.enable_glial);
    EXPECT_TRUE(config.subsystems.enable_swarm);
    EXPECT_TRUE(config.subsystems.enable_async);
    EXPECT_TRUE(config.subsystems.enable_lnn);
    EXPECT_TRUE(config.subsystems.enable_snn);
    EXPECT_TRUE(config.auto_discover_modules);
    EXPECT_TRUE(config.auto_register_all);
    EXPECT_TRUE(config.enable_health_monitoring);
}

TEST(MeshBootstrapConfigTest, DefaultConfigNullPtr) {
    nimcp_error_t err = mesh_bootstrap_default_config(NULL);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST(MeshBootstrapConfigTest, SubsystemsAllMacro) {
    mesh_subsystem_flags_t flags = MESH_SUBSYSTEMS_ALL;

    EXPECT_TRUE(flags.enable_cognitive);
    EXPECT_TRUE(flags.enable_sensory);
    EXPECT_TRUE(flags.enable_motor);
    EXPECT_TRUE(flags.enable_memory);
    EXPECT_TRUE(flags.enable_security);
    EXPECT_TRUE(flags.enable_gpu);
    EXPECT_TRUE(flags.enable_plasticity);
    EXPECT_TRUE(flags.enable_glial);
    EXPECT_TRUE(flags.enable_swarm);
    EXPECT_TRUE(flags.enable_async);
    EXPECT_TRUE(flags.enable_lnn);
    EXPECT_TRUE(flags.enable_snn);
    EXPECT_TRUE(flags.enable_nlp);
    EXPECT_FALSE(flags.enable_superhuman);  /* Disabled by default */
    EXPECT_FALSE(flags.enable_quantum);     /* Disabled by default */
}

TEST(MeshBootstrapConfigTest, SubsystemsCoreMacro) {
    mesh_subsystem_flags_t flags = MESH_SUBSYSTEMS_CORE;

    EXPECT_TRUE(flags.enable_cognitive);
    EXPECT_TRUE(flags.enable_sensory);
    EXPECT_TRUE(flags.enable_motor);
    EXPECT_TRUE(flags.enable_memory);
    EXPECT_TRUE(flags.enable_security);
    EXPECT_FALSE(flags.enable_gpu);        /* Disabled in core */
    EXPECT_FALSE(flags.enable_plasticity); /* Disabled in core */
    EXPECT_FALSE(flags.enable_glial);      /* Disabled in core */
    EXPECT_TRUE(flags.enable_swarm);
    EXPECT_TRUE(flags.enable_async);
    EXPECT_FALSE(flags.enable_lnn);
    EXPECT_FALSE(flags.enable_snn);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

class MeshBootstrapLifecycleTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;

    void TearDown() override {
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
        }
    }
};

TEST_F(MeshBootstrapLifecycleTest, CreateWithDefaults) {
    bootstrap = mesh_bootstrap_create(NULL);
    ASSERT_NE(bootstrap, nullptr);

    mesh_bootstrap_stats_t stats;
    nimcp_error_t err = mesh_bootstrap_get_stats(bootstrap, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(stats.fully_initialized);
    EXPECT_GT(stats.total_modules_registered, 0u);
}

TEST_F(MeshBootstrapLifecycleTest, CreateWithCoreOnly) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);

    /* Core subsystems should be registered */
    EXPECT_GT(stats.cognitive_modules, 0u);
    EXPECT_GT(stats.sensory_modules, 0u);
    EXPECT_GT(stats.motor_modules, 0u);
    EXPECT_GT(stats.memory_modules, 0u);
    EXPECT_GT(stats.security_modules, 0u);

    /* Non-core should be zero */
    EXPECT_EQ(stats.gpu_modules, 0u);
    EXPECT_EQ(stats.plasticity_modules, 0u);
    EXPECT_EQ(stats.glial_modules, 0u);
    EXPECT_EQ(stats.lnn_modules, 0u);
    EXPECT_EQ(stats.snn_modules, 0u);
}

TEST_F(MeshBootstrapLifecycleTest, DestroyNull) {
    /* Should not crash */
    mesh_bootstrap_destroy(NULL);
}

TEST_F(MeshBootstrapLifecycleTest, DestroyInvalid) {
    /* mesh_bootstrap_t is opaque, so we test with a corrupted pointer */
    /* The destroy function should handle bad pointers gracefully */

    /* This test just verifies that passing NULL doesn't crash */
    /* (covered by DestroyNull test) */
    /* We cannot test with invalid memory without UB */
    SUCCEED();
}

/* ============================================================================
 * Component Access Tests
 * ============================================================================ */

class MeshBootstrapAccessTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;

    void SetUp() override {
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.verbose_logging = false;  /* Quiet tests */
        bootstrap = mesh_bootstrap_create(&config);
        ASSERT_NE(bootstrap, nullptr);
    }

    void TearDown() override {
        mesh_bootstrap_destroy(bootstrap);
    }
};

TEST_F(MeshBootstrapAccessTest, GetIntegration) {
    mesh_integration_t* integration = mesh_bootstrap_get_integration(bootstrap);
    EXPECT_NE(integration, nullptr);
}

TEST_F(MeshBootstrapAccessTest, GetRegistry) {
    mesh_participant_registry_t* registry = mesh_bootstrap_get_registry(bootstrap);
    EXPECT_NE(registry, nullptr);
}

TEST_F(MeshBootstrapAccessTest, GetChannels) {
    mesh_channel_t* system = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_SYSTEM);
    EXPECT_NE(system, nullptr);

    mesh_channel_t* left = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_NE(left, nullptr);

    mesh_channel_t* right = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_RIGHT_HEMISPHERE);
    EXPECT_NE(right, nullptr);

    mesh_channel_t* subcortical = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_SUBCORTICAL);
    EXPECT_NE(subcortical, nullptr);

    mesh_channel_t* gpu = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_GPU_COMPUTE);
    EXPECT_NE(gpu, nullptr);
}

TEST_F(MeshBootstrapAccessTest, GetInvalidChannel) {
    mesh_channel_t* invalid = mesh_bootstrap_get_channel(bootstrap, (mesh_channel_id_t)999);
    EXPECT_EQ(invalid, nullptr);
}

TEST_F(MeshBootstrapAccessTest, GetIntegrationNull) {
    mesh_integration_t* integration = mesh_bootstrap_get_integration(NULL);
    EXPECT_EQ(integration, nullptr);
}

/* ============================================================================
 * Module Registration Tests
 * ============================================================================ */

TEST_F(MeshBootstrapAccessTest, CognitiveModulesRegistered) {
    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);

    /* Should have registered multiple cognitive modules */
    EXPECT_GT(stats.cognitive_modules, 10u);
}

TEST_F(MeshBootstrapAccessTest, SensoryModulesRegistered) {
    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);

    EXPECT_GT(stats.sensory_modules, 10u);
}

TEST_F(MeshBootstrapAccessTest, MotorModulesRegistered) {
    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);

    EXPECT_GT(stats.motor_modules, 10u);
}

TEST_F(MeshBootstrapAccessTest, MemoryModulesRegistered) {
    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);

    EXPECT_GT(stats.memory_modules, 10u);
}

TEST_F(MeshBootstrapAccessTest, SecurityModulesRegistered) {
    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);

    EXPECT_GT(stats.security_modules, 5u);
}

TEST_F(MeshBootstrapAccessTest, TotalModulesRegistered) {
    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);

    /* Should have registered a large number of modules */
    EXPECT_GT(stats.total_modules_registered, 100u);

    /* Total should equal sum of categories */
    size_t expected_total =
        stats.cognitive_modules +
        stats.sensory_modules +
        stats.motor_modules +
        stats.memory_modules +
        stats.security_modules +
        stats.gpu_modules +
        stats.plasticity_modules +
        stats.glial_modules +
        stats.swarm_modules +
        stats.async_modules +
        stats.lnn_modules +
        stats.snn_modules;

    EXPECT_EQ(stats.total_modules_registered, expected_total);
}

/* ============================================================================
 * Update and Processing Tests
 * ============================================================================ */

TEST_F(MeshBootstrapAccessTest, Update) {
    nimcp_error_t err = mesh_bootstrap_update(bootstrap, 100);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapAccessTest, ProcessTransactions) {
    size_t processed = mesh_bootstrap_process_transactions(bootstrap);
    /* Initial processing may be zero (no pending transactions) */
    EXPECT_GE(processed, 0u);
}

TEST_F(MeshBootstrapAccessTest, GossipAll) {
    nimcp_error_t err = mesh_bootstrap_gossip_all(bootstrap, 3);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapAccessTest, HasConverged) {
    /* After gossip rounds, check convergence */
    mesh_bootstrap_gossip_all(bootstrap, 5);
    bool converged = mesh_bootstrap_has_converged(bootstrap);
    /* May or may not have converged, but should not crash */
    (void)converged;
}

TEST_F(MeshBootstrapAccessTest, GetFreeEnergy) {
    float fe = mesh_bootstrap_get_free_energy(bootstrap);
    EXPECT_GE(fe, 0.0f);
    EXPECT_LE(fe, 2.0f);  /* Should be reasonable */
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshBootstrapAccessTest, GetStats) {
    mesh_bootstrap_stats_t stats;
    nimcp_error_t err = mesh_bootstrap_get_stats(bootstrap, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(stats.fully_initialized);
    EXPECT_EQ(stats.total_channels_active, (size_t)MESH_NUM_STANDARD_CHANNELS);
    EXPECT_GT(stats.initialization_time_ns, 0u);
}

TEST_F(MeshBootstrapAccessTest, GetStatsNull) {
    nimcp_error_t err = mesh_bootstrap_get_stats(bootstrap, NULL);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshBootstrapAccessTest, PrintStatus) {
    /* Should not crash */
    mesh_bootstrap_print_status(bootstrap);
}

TEST_F(MeshBootstrapAccessTest, PrintStatusNull) {
    /* Should not crash */
    mesh_bootstrap_print_status(NULL);
}

/* ============================================================================
 * Manual Registration Tests
 * ============================================================================ */

class MeshBootstrapRegistrationTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;

    void SetUp() override {
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        /* Disable auto-registration to test manual registration */
        config.subsystems = (mesh_subsystem_flags_t){0};
        config.verbose_logging = false;
        bootstrap = mesh_bootstrap_create(&config);
        ASSERT_NE(bootstrap, nullptr);
    }

    void TearDown() override {
        mesh_bootstrap_destroy(bootstrap);
    }
};

TEST_F(MeshBootstrapRegistrationTest, RegisterCognitiveModules) {
    /* Note: Core components (PFC left/right) are always registered first */
    size_t registered = mesh_bootstrap_register_cognitive_modules(bootstrap);
    EXPECT_GT(registered, 0u);

    mesh_bootstrap_stats_t after;
    mesh_bootstrap_get_stats(bootstrap, &after);
    /* Should have cognitive modules registered now */
    EXPECT_GT(after.cognitive_modules, 0u);
}

TEST_F(MeshBootstrapRegistrationTest, RegisterSensoryModules) {
    size_t registered = mesh_bootstrap_register_sensory_modules(bootstrap);
    EXPECT_GT(registered, 0u);
}

TEST_F(MeshBootstrapRegistrationTest, RegisterMotorModules) {
    size_t registered = mesh_bootstrap_register_motor_modules(bootstrap);
    EXPECT_GT(registered, 0u);
}

TEST_F(MeshBootstrapRegistrationTest, RegisterMemoryModules) {
    size_t registered = mesh_bootstrap_register_memory_modules(bootstrap);
    EXPECT_GT(registered, 0u);
}

TEST_F(MeshBootstrapRegistrationTest, RegisterSecurityModules) {
    size_t registered = mesh_bootstrap_register_security_modules(bootstrap);
    EXPECT_GT(registered, 0u);
}

TEST_F(MeshBootstrapRegistrationTest, RegisterGpuModules) {
    size_t registered = mesh_bootstrap_register_gpu_modules(bootstrap);
    EXPECT_GT(registered, 0u);
}

TEST_F(MeshBootstrapRegistrationTest, RegisterPlasticityModules) {
    size_t registered = mesh_bootstrap_register_plasticity_modules(bootstrap);
    EXPECT_GT(registered, 0u);
}

TEST_F(MeshBootstrapRegistrationTest, RegisterGlialModules) {
    size_t registered = mesh_bootstrap_register_glial_modules(bootstrap);
    EXPECT_GT(registered, 0u);
}

TEST_F(MeshBootstrapRegistrationTest, RegisterSwarmModules) {
    size_t registered = mesh_bootstrap_register_swarm_modules(bootstrap);
    EXPECT_GT(registered, 0u);
}

TEST_F(MeshBootstrapRegistrationTest, RegisterAsyncModules) {
    size_t registered = mesh_bootstrap_register_async_modules(bootstrap);
    EXPECT_GT(registered, 0u);
}

TEST_F(MeshBootstrapRegistrationTest, RegisterLnnModules) {
    size_t registered = mesh_bootstrap_register_lnn_modules(bootstrap);
    EXPECT_GT(registered, 0u);
}

TEST_F(MeshBootstrapRegistrationTest, RegisterSnnModules) {
    size_t registered = mesh_bootstrap_register_snn_modules(bootstrap);
    EXPECT_GT(registered, 0u);
}

/* ============================================================================
 * Full System Integration Test
 * ============================================================================ */

TEST(MeshBootstrapIntegrationTest, FullSystemWithGossip) {
    /* Create full system */
    mesh_bootstrap_t* bootstrap = mesh_bootstrap_create(NULL);
    ASSERT_NE(bootstrap, nullptr);

    /* Get stats */
    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);
    EXPECT_GT(stats.total_modules_registered, 100u);

    /* Run several update cycles */
    for (int i = 0; i < 10; i++) {
        mesh_bootstrap_update(bootstrap, 10);
        mesh_bootstrap_gossip_all(bootstrap, 1);
    }

    /* Process any pending transactions */
    mesh_bootstrap_process_transactions(bootstrap);

    /* Check system state */
    float fe = mesh_bootstrap_get_free_energy(bootstrap);
    EXPECT_GE(fe, 0.0f);

    /* Print status */
    mesh_bootstrap_print_status(bootstrap);

    mesh_bootstrap_destroy(bootstrap);
}

TEST(MeshBootstrapIntegrationTest, QuietModeNoOutput) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.verbose_logging = false;

    mesh_bootstrap_t* bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);
    EXPECT_TRUE(stats.fully_initialized);

    mesh_bootstrap_destroy(bootstrap);
}

/* ============================================================================
 * Subsystem Combination Tests
 * ============================================================================ */

TEST(MeshBootstrapSubsystemTest, OnlyCognitive) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = (mesh_subsystem_flags_t){
        .enable_cognitive = true,
        .enable_sensory = false,
        .enable_motor = false,
        .enable_memory = false,
        .enable_security = false,
        .enable_gpu = false,
        .enable_plasticity = false,
        .enable_glial = false,
        .enable_swarm = false,
        .enable_async = false,
        .enable_lnn = false,
        .enable_snn = false,
        .enable_nlp = false,
        .enable_superhuman = false,
        .enable_quantum = false
    };

    mesh_bootstrap_t* bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);

    /* Only cognitive category registration runs, so only cognitive modules counted */
    EXPECT_GT(stats.cognitive_modules, 0u);
    EXPECT_EQ(stats.sensory_modules, 0u);
    /* motor_modules counter only updated by motor category registration */
    /* Core components are registered but tracked separately */
    EXPECT_EQ(stats.motor_modules, 0u);
    EXPECT_EQ(stats.memory_modules, 0u);
    EXPECT_EQ(stats.security_modules, 0u);

    mesh_bootstrap_destroy(bootstrap);
}

TEST(MeshBootstrapSubsystemTest, OnlyGpu) {
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = (mesh_subsystem_flags_t){
        .enable_cognitive = false,
        .enable_sensory = false,
        .enable_motor = false,
        .enable_memory = false,
        .enable_security = false,
        .enable_gpu = true,
        .enable_plasticity = false,
        .enable_glial = false,
        .enable_swarm = false,
        .enable_async = false,
        .enable_lnn = false,
        .enable_snn = false,
        .enable_nlp = false,
        .enable_superhuman = false,
        .enable_quantum = false
    };

    mesh_bootstrap_t* bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);

    EXPECT_GT(stats.gpu_modules, 0u);

    mesh_bootstrap_destroy(bootstrap);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST(MeshBootstrapErrorTest, UpdateNull) {
    nimcp_error_t err = mesh_bootstrap_update(NULL, 100);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST(MeshBootstrapErrorTest, ProcessNull) {
    size_t processed = mesh_bootstrap_process_transactions(NULL);
    EXPECT_EQ(processed, 0u);
}

TEST(MeshBootstrapErrorTest, GossipNull) {
    nimcp_error_t err = mesh_bootstrap_gossip_all(NULL, 3);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST(MeshBootstrapErrorTest, ConvergedNull) {
    bool converged = mesh_bootstrap_has_converged(NULL);
    EXPECT_FALSE(converged);
}

TEST(MeshBootstrapErrorTest, FreeEnergyNull) {
    float fe = mesh_bootstrap_get_free_energy(NULL);
    EXPECT_EQ(fe, 1.0f);  /* Default value */
}

/* ============================================================================
 * Pattern-Based Routing Tests
 * ============================================================================ */

class MeshBootstrapPatternRoutingTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap;

    void SetUp() override {
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        /* Minimal subsystems for faster test */
        config.subsystems.enable_cognitive = true;
        config.subsystems.enable_sensory = false;
        config.subsystems.enable_motor = false;
        config.subsystems.enable_memory = false;
        config.subsystems.enable_security = false;
        config.subsystems.enable_gpu = false;
        config.subsystems.enable_plasticity = false;
        config.subsystems.enable_glial = false;
        config.subsystems.enable_swarm = false;
        config.subsystems.enable_async = false;
        config.subsystems.enable_lnn = false;
        config.subsystems.enable_snn = false;

        bootstrap = mesh_bootstrap_create(&config);
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
};

TEST_F(MeshBootstrapPatternRoutingTest, GetPatternRouter) {
    ASSERT_NE(bootstrap, nullptr);

    mesh_pattern_router_t* router = mesh_bootstrap_get_pattern_router(bootstrap);
    EXPECT_NE(router, nullptr);
}

TEST_F(MeshBootstrapPatternRoutingTest, GetPatternRouterNull) {
    mesh_pattern_router_t* router = mesh_bootstrap_get_pattern_router(nullptr);
    EXPECT_EQ(router, nullptr);
}

TEST_F(MeshBootstrapPatternRoutingTest, RegisterReceptiveField) {
    ASSERT_NE(bootstrap, nullptr);

    /* Create a simple receptive field */
    mesh_receptive_field_t field;
    mesh_receptive_field_init(&field);

    float pattern_values[] = {1.0f, 0.5f, 0.0f, 0.0f};
    field.preferred[0] = create_pattern(pattern_values, 4);
    field.pattern_count = 1;
    field.threshold = 0.5f;

    /* Register for a made-up module ID */
    mesh_participant_id_t module_id = 0x9999;
    nimcp_error_t err = mesh_bootstrap_register_receptive_field(
        bootstrap, module_id, &field);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapPatternRoutingTest, RegisterReceptiveFieldNullBootstrap) {
    mesh_receptive_field_t field;
    mesh_receptive_field_init(&field);

    nimcp_error_t err = mesh_bootstrap_register_receptive_field(
        nullptr, 0x9999, &field);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapPatternRoutingTest, RegisterReceptiveFieldNullField) {
    ASSERT_NE(bootstrap, nullptr);

    nimcp_error_t err = mesh_bootstrap_register_receptive_field(
        bootstrap, 0x9999, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapPatternRoutingTest, RouteByPattern) {
    ASSERT_NE(bootstrap, nullptr);

    /* Register a module with a pattern */
    mesh_receptive_field_t field;
    mesh_receptive_field_init(&field);

    float pattern_values[] = {1.0f, 0.0f, 0.0f, 0.0f};
    field.preferred[0] = create_pattern(pattern_values, 4);
    field.pattern_count = 1;
    field.threshold = 0.3f;

    mesh_participant_id_t module_id = 0x9999;
    mesh_bootstrap_register_receptive_field(bootstrap, module_id, &field);

    /* Create a matching transaction */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(pattern_values, 4);

    mesh_participant_id_t endorsers[10];
    size_t count = 0;

    nimcp_error_t err = mesh_bootstrap_route_by_pattern(
        bootstrap, &tx, endorsers, 10, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);
}

TEST_F(MeshBootstrapPatternRoutingTest, ApplyNeuromodulation) {
    ASSERT_NE(bootstrap, nullptr);

    nimcp_error_t err = mesh_bootstrap_apply_neuromodulation(
        bootstrap, MESH_NEUROMOD_DOPAMINE, 0.8f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = mesh_bootstrap_apply_neuromodulation(
        bootstrap, MESH_NEUROMOD_NOREPINEPHRINE, 0.7f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = mesh_bootstrap_apply_neuromodulation(
        bootstrap, MESH_NEUROMOD_ACETYLCHOLINE, 0.6f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = mesh_bootstrap_apply_neuromodulation(
        bootstrap, MESH_NEUROMOD_SEROTONIN, 0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapPatternRoutingTest, ApplyNeuromodulationNull) {
    nimcp_error_t err = mesh_bootstrap_apply_neuromodulation(
        nullptr, MESH_NEUROMOD_DOPAMINE, 0.8f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapPatternRoutingTest, LearnRoutingOutcome) {
    ASSERT_NE(bootstrap, nullptr);

    /* Register a module */
    mesh_receptive_field_t field;
    mesh_receptive_field_init(&field);

    float pattern_values[] = {1.0f, 0.0f, 0.0f, 0.0f};
    field.preferred[0] = create_pattern(pattern_values, 4);
    field.pattern_count = 1;
    field.threshold = 0.3f;

    mesh_participant_id_t module_id = 0x9999;
    mesh_bootstrap_register_receptive_field(bootstrap, module_id, &field);

    /* Create a transaction */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(pattern_values, 4);

    /* Learn from successful outcome */
    mesh_participant_id_t endorsers[] = {module_id};
    nimcp_error_t err = mesh_bootstrap_learn_routing_outcome(
        bootstrap, &tx, endorsers, 1, true, 1.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Learn from failed outcome */
    err = mesh_bootstrap_learn_routing_outcome(
        bootstrap, &tx, endorsers, 1, false, -0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapPatternRoutingTest, BrainLikeRoutingScenario) {
    ASSERT_NE(bootstrap, nullptr);

    /* Simulate visual and motor modules */
    mesh_receptive_field_t visual_field;
    mesh_receptive_field_init(&visual_field);
    float visual_pattern[] = {1.0f, 0.8f, 0.1f, 0.0f};
    visual_field.preferred[0] = create_pattern(visual_pattern, 4);
    visual_field.pattern_count = 1;
    visual_field.threshold = 0.4f;

    mesh_receptive_field_t motor_field;
    mesh_receptive_field_init(&motor_field);
    float motor_pattern[] = {0.0f, 0.0f, 1.0f, 0.9f};
    motor_field.preferred[0] = create_pattern(motor_pattern, 4);
    motor_field.pattern_count = 1;
    motor_field.threshold = 0.4f;

    mesh_participant_id_t visual_id = 0x1001;
    mesh_participant_id_t motor_id = 0x1002;

    mesh_bootstrap_register_receptive_field(bootstrap, visual_id, &visual_field);
    mesh_bootstrap_register_receptive_field(bootstrap, motor_id, &motor_field);

    /* Visual transaction should select visual module */
    mesh_pattern_transaction_t visual_tx;
    memset(&visual_tx, 0, sizeof(visual_tx));
    visual_tx.content_pattern = create_pattern(visual_pattern, 4);

    mesh_participant_id_t endorsers[10];
    size_t count = 0;

    mesh_bootstrap_route_by_pattern(bootstrap, &visual_tx, endorsers, 10, &count);
    EXPECT_GE(count, 1u);

    /* Check that visual module was selected */
    bool visual_selected = false;
    for (size_t i = 0; i < count; i++) {
        if (endorsers[i] == visual_id) visual_selected = true;
    }
    EXPECT_TRUE(visual_selected);

    /* Motor transaction should select motor module */
    mesh_pattern_transaction_t motor_tx;
    memset(&motor_tx, 0, sizeof(motor_tx));
    motor_tx.content_pattern = create_pattern(motor_pattern, 4);

    count = 0;
    mesh_bootstrap_route_by_pattern(bootstrap, &motor_tx, endorsers, 10, &count);
    EXPECT_GE(count, 1u);

    /* Check that motor module was selected */
    bool motor_selected = false;
    for (size_t i = 0; i < count; i++) {
        if (endorsers[i] == motor_id) motor_selected = true;
    }
    EXPECT_TRUE(motor_selected);
}
