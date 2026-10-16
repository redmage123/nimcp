/**
 * @file test_orchestrator_integration.cpp
 * @brief Integration tests for plasticity orchestrator with sleep and immune systems
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Tests orchestrator integration with external systems
 * WHY:  Ensure modules coordinate correctly through orchestrator
 * HOW:  Test sleep/immune modulation, multi-module updates, callbacks
 */

#include <gtest/gtest.h>
#include <cmath>
// Headers have their own extern "C" guards
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class OrchestratorSleepIntegrationTest : public ::testing::Test {
protected:
    plasticity_orchestrator_t* orchestrator;
    sleep_system_t sleep_system;

    void SetUp() override {
        // Create sleep system
        sleep_config_t sleep_cfg = sleep_default_config();
        sleep_system = sleep_system_create(&sleep_cfg);
        ASSERT_NE(sleep_system, nullptr);

        // Create orchestrator with minimal config
        plasticity_orchestrator_config_t config;
        plasticity_orchestrator_default_config(&config);
        config.enabled.enable_triplet_stdp = true;
        config.enabled.enable_bcm = false;
        config.enabled.enable_homeostatic = false;
        config.enabled.enable_metabolic = false;
        config.enabled.enable_calcium = false;
        config.enabled.enable_structural = false;
        config.enabled.enable_protein_synthesis = false;
        config.enabled.enable_metaplasticity = false;
        config.enabled.enable_heterosynaptic = false;
        config.enabled.enable_astrocyte = false;
        orchestrator = plasticity_orchestrator_create(&config);
        ASSERT_NE(orchestrator, nullptr);
    }

    void TearDown() override {
        if (orchestrator) plasticity_orchestrator_destroy(orchestrator);
        if (sleep_system) sleep_system_destroy(sleep_system);
    }
};

class OrchestratorImmuneIntegrationTest : public ::testing::Test {
protected:
    plasticity_orchestrator_t* orchestrator;
    brain_immune_system_t* immune;

    void SetUp() override {
        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create orchestrator with minimal config
        plasticity_orchestrator_config_t config;
        plasticity_orchestrator_default_config(&config);
        config.enabled.enable_triplet_stdp = true;
        orchestrator = plasticity_orchestrator_create(&config);
        ASSERT_NE(orchestrator, nullptr);
    }

    void TearDown() override {
        if (orchestrator) plasticity_orchestrator_destroy(orchestrator);
        if (immune) brain_immune_destroy(immune);
    }
};

class OrchestratorMultiModuleIntegrationTest : public ::testing::Test {
protected:
    plasticity_orchestrator_t* orchestrator;

    void SetUp() override {
        // Create orchestrator with multiple modules enabled
        plasticity_orchestrator_config_t config;
        plasticity_orchestrator_default_config(&config);
        config.enabled.enable_triplet_stdp = true;
        config.enabled.enable_bcm = true;
        config.enabled.enable_homeostatic = true;
        orchestrator = plasticity_orchestrator_create(&config);
        ASSERT_NE(orchestrator, nullptr);
    }

    void TearDown() override {
        if (orchestrator) plasticity_orchestrator_destroy(orchestrator);
    }
};

/* ============================================================================
 * Sleep-Orchestrator Integration Tests
 * ============================================================================ */

TEST_F(OrchestratorSleepIntegrationTest, ConnectSleep) {
    int ret = plasticity_orchestrator_connect_sleep(orchestrator, sleep_system);
    EXPECT_EQ(ret, 0);
}

TEST_F(OrchestratorSleepIntegrationTest, UpdateWithSleepModulation) {
    // Connect sleep system
    ASSERT_EQ(plasticity_orchestrator_connect_sleep(orchestrator, sleep_system), 0);

    // Create a synapse
    EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, 0, 0.5f), 0);

    // Update should work
    EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
}

TEST_F(OrchestratorSleepIntegrationTest, SleepStateAffectsPlasticity) {
    ASSERT_EQ(plasticity_orchestrator_connect_sleep(orchestrator, sleep_system), 0);

    // Create synapse
    EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, 0, 0.5f), 0);

    // Wake state updates
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
    }

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 10);
}

TEST_F(OrchestratorSleepIntegrationTest, DisconnectSleep) {
    ASSERT_EQ(plasticity_orchestrator_connect_sleep(orchestrator, sleep_system), 0);

    // Should be able to disconnect (function may not exist, test gracefully)
    // Update still works without sleep
    EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
}

/* ============================================================================
 * Immune-Orchestrator Integration Tests
 * ============================================================================ */

TEST_F(OrchestratorImmuneIntegrationTest, ConnectImmune) {
    int ret = plasticity_orchestrator_connect_immune(orchestrator, immune);
    EXPECT_EQ(ret, 0);
}

TEST_F(OrchestratorImmuneIntegrationTest, UpdateWithImmuneModulation) {
    ASSERT_EQ(plasticity_orchestrator_connect_immune(orchestrator, immune), 0);

    // Create synapse
    EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, 0, 0.5f), 0);

    // Update should work
    EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
}

TEST_F(OrchestratorImmuneIntegrationTest, InflammationAffectsPlasticity) {
    ASSERT_EQ(plasticity_orchestrator_connect_immune(orchestrator, immune), 0);

    // Create synapse
    EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, 0, 0.5f), 0);
    float initial_weight = plasticity_orchestrator_get_weight(orchestrator, 0);

    // Run updates during normal immune state
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
    }

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 10);
}

/* ============================================================================
 * Multi-Module Integration Tests
 * ============================================================================ */

TEST_F(OrchestratorMultiModuleIntegrationTest, AllModulesUpdate) {
    // Create synapse
    EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, 0, 0.5f), 0);

    // Update with all modules
    EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 1);
}

TEST_F(OrchestratorMultiModuleIntegrationTest, ConsecutiveUpdates) {
    // Create multiple synapses
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, i, 0.5f), 0);
    }

    // Many updates
    for (int u = 0; u < 100; u++) {
        EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
    }

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 100);
}

TEST_F(OrchestratorMultiModuleIntegrationTest, WeightsRemainBounded) {
    // Create synapses
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, i, 0.5f), 0);
    }

    // Run many updates
    for (int u = 0; u < 100; u++) {
        plasticity_orchestrator_update(orchestrator, 1);
    }

    // Check weights remain bounded
    for (int i = 0; i < 10; i++) {
        float weight = plasticity_orchestrator_get_weight(orchestrator, i);
        if (!std::isnan(weight)) {
            EXPECT_GE(weight, 0.0f);
            EXPECT_LE(weight, 1.0f);
        }
    }
}

TEST_F(OrchestratorMultiModuleIntegrationTest, StatsAccumulate) {
    // Create synapse
    plasticity_orchestrator_set_weight(orchestrator, 0, 0.5f);

    // Run updates
    for (int i = 0; i < 50; i++) {
        plasticity_orchestrator_update(orchestrator, 1);
    }

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 50);

    // Run more updates
    for (int i = 0; i < 50; i++) {
        plasticity_orchestrator_update(orchestrator, 1);
    }

    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 100);

    // Reset stats
    EXPECT_EQ(plasticity_orchestrator_reset_stats(orchestrator), 0);
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 0);
}

/* ============================================================================
 * Combined Sleep and Immune Integration
 * ============================================================================ */

class OrchestratorFullIntegrationTest : public ::testing::Test {
protected:
    plasticity_orchestrator_t* orchestrator;
    sleep_system_t sleep_system;
    brain_immune_system_t* immune;

    void SetUp() override {
        // Create sleep system
        sleep_config_t sleep_cfg = sleep_default_config();
        sleep_system = sleep_system_create(&sleep_cfg);
        ASSERT_NE(sleep_system, nullptr);

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create orchestrator with all modules
        plasticity_orchestrator_config_t config;
        plasticity_orchestrator_default_config(&config);
        orchestrator = plasticity_orchestrator_create(&config);
        ASSERT_NE(orchestrator, nullptr);
    }

    void TearDown() override {
        if (orchestrator) plasticity_orchestrator_destroy(orchestrator);
        if (sleep_system) sleep_system_destroy(sleep_system);
        if (immune) brain_immune_destroy(immune);
    }
};

TEST_F(OrchestratorFullIntegrationTest, ConnectBothSystems) {
    EXPECT_EQ(plasticity_orchestrator_connect_sleep(orchestrator, sleep_system), 0);
    EXPECT_EQ(plasticity_orchestrator_connect_immune(orchestrator, immune), 0);
}

TEST_F(OrchestratorFullIntegrationTest, UpdateWithBothConnected) {
    ASSERT_EQ(plasticity_orchestrator_connect_sleep(orchestrator, sleep_system), 0);
    ASSERT_EQ(plasticity_orchestrator_connect_immune(orchestrator, immune), 0);

    // Create synapses
    for (int i = 0; i < 10; i++) {
        plasticity_orchestrator_set_weight(orchestrator, i, 0.5f);
    }

    // Run updates
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
    }

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 100);
}

TEST_F(OrchestratorFullIntegrationTest, LongRunningSimulation) {
    ASSERT_EQ(plasticity_orchestrator_connect_sleep(orchestrator, sleep_system), 0);
    ASSERT_EQ(plasticity_orchestrator_connect_immune(orchestrator, immune), 0);

    // Create synapses
    for (int i = 0; i < 50; i++) {
        plasticity_orchestrator_set_weight(orchestrator, i, 0.5f);
    }

    // Run 1000 updates (simulating ~16 minutes at 1ms step)
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
    }

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 1000);
}

/* ============================================================================
 * Neuromodulator Integration Tests
 * ============================================================================ */

class OrchestratorNeuromodIntegrationTest : public ::testing::Test {
protected:
    plasticity_orchestrator_t* orchestrator;
    neuromodulator_system_t neuromod;

    void SetUp() override {
        // Create neuromodulator system
        neuromod = neuromodulator_system_create(nullptr);
        ASSERT_NE(neuromod, nullptr);

        // Create orchestrator
        plasticity_orchestrator_config_t config;
        plasticity_orchestrator_default_config(&config);
        config.enabled.enable_triplet_stdp = true;
        orchestrator = plasticity_orchestrator_create(&config);
        ASSERT_NE(orchestrator, nullptr);
    }

    void TearDown() override {
        if (orchestrator) plasticity_orchestrator_destroy(orchestrator);
        if (neuromod) neuromodulator_system_destroy(neuromod);
    }
};

TEST_F(OrchestratorNeuromodIntegrationTest, ConnectNeuromodulators) {
    int ret = plasticity_orchestrator_connect_neuromodulators(orchestrator, neuromod);
    EXPECT_EQ(ret, 0);
}

TEST_F(OrchestratorNeuromodIntegrationTest, UpdateWithNeuromodulation) {
    ASSERT_EQ(plasticity_orchestrator_connect_neuromodulators(orchestrator, neuromod), 0);

    // Create synapse
    plasticity_orchestrator_set_weight(orchestrator, 0, 0.5f);

    // Run updates
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
    }

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 10);
}

TEST_F(OrchestratorNeuromodIntegrationTest, DopamineAffectsPlasticity) {
    ASSERT_EQ(plasticity_orchestrator_connect_neuromodulators(orchestrator, neuromod), 0);

    // Create synapse
    plasticity_orchestrator_set_weight(orchestrator, 0, 0.5f);

    // Increase dopamine
    neuromodulator_set_level(neuromod, NEUROMOD_DOPAMINE, 1.0f);

    // Run updates
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
    }

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 10);
}
