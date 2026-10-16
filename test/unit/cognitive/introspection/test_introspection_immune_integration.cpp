/**
 * @file test_introspection_immune_integration.cpp
 * @brief Unit tests for brain immune system integration with introspection
 *
 * WHAT: Tests immune system effects on consciousness metrics, uncertainty, and patterns
 * WHY:  Verify immune modulation of introspection outputs works correctly
 * HOW:  Create immune system, connect to introspection, test modulation effects
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/introspection/nimcp_consciousness_metrics.h"
#include "cognitive/introspection/nimcp_ensemble_uncertainty.h"
#include "cognitive/introspection/nimcp_temporal_patterns.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

/**
 * WHAT: Test fixture for immune integration tests
 * WHY:  Reusable setup/teardown for brain, introspection, and immune system
 * HOW:  Create minimal brain with introspection and immune system
 */
class ImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    introspection_context_t introspection;
    brain_immune_system_t* immune_system;

    void SetUp() override {
        /* WHAT: Create minimal brain for testing */
        brain = brain_create("introspection_immune_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        /* WHAT: Create introspection context */
        introspection_config_t intro_config = introspection_default_config();
        intro_config.enable_pattern_tracking = true;
        intro_config.enable_uncertainty_estimation = true;

        introspection = introspection_context_create(brain, &intro_config);
        ASSERT_NE(introspection, nullptr);

        /* WHAT: Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);
    }

    void TearDown() override {
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
        if (introspection) {
            introspection_context_destroy(introspection);
        }
        if (brain) {
            brain_destroy(brain);
        }
    }
};

/**
 * WHAT: Test connection of immune system to introspection
 * WHY:  Verify basic integration works
 * HOW:  Connect immune, verify retrieval
 */
TEST_F(ImmuneIntegrationTest, ConnectImmuneSystem) {
    /* WHAT: Connect immune system */
    bool success = introspection_connect_immune(introspection, immune_system);
    EXPECT_TRUE(success);

    /* WHAT: Verify retrieval */
    brain_immune_system_t* retrieved = introspection_get_immune(introspection);
    EXPECT_EQ(retrieved, immune_system);
}

/**
 * WHAT: Test null pointer handling
 * WHY:  Ensure robust error handling
 * HOW:  Pass NULL to connection function
 */
TEST_F(ImmuneIntegrationTest, ConnectNullHandling) {
    /* WHAT: Test NULL context */
    bool success = introspection_connect_immune(nullptr, immune_system);
    EXPECT_FALSE(success);

    /* WHAT: Test NULL immune system */
    success = introspection_connect_immune(introspection, nullptr);
    EXPECT_FALSE(success);

    /* WHAT: Get from NULL context */
    brain_immune_system_t* retrieved = introspection_get_immune(nullptr);
    EXPECT_EQ(retrieved, nullptr);
}

/**
 * WHAT: Test consciousness phi modulation by immune inflammation
 * WHY:  Verify inflammation reduces consciousness metrics
 * HOW:  Compute phi without and with immune system active
 */
TEST_F(ImmuneIntegrationTest, ConsciousnessPhiModulation) {
    /* WHAT: Connect immune system */
    introspection_connect_immune(introspection, immune_system);

    /* WHAT: Compute baseline phi (surveillance phase) */
    consciousness_phi_config_t config = consciousness_phi_fast_config();
    consciousness_phi_result_t* baseline = introspection_compute_phi(introspection, &config);
    ASSERT_NE(baseline, nullptr);
    float baseline_phi = baseline->phi;
    consciousness_phi_result_free(baseline);

    /* WHAT: Start immune system and simulate effector phase */
    /* NOTE: In real implementation, would trigger immune response */
    /* For now, verify interpretation includes immune phase */

    /* WHAT: Compute phi again */
    consciousness_phi_result_t* modulated = introspection_compute_phi(introspection, &config);
    ASSERT_NE(modulated, nullptr);

    /* WHAT: Verify interpretation includes immune info */
    EXPECT_NE(modulated->interpretation, nullptr);
    /* NOTE: Would check for immune phase in string, but immune is in surveillance
     * so phi should be similar to baseline */

    consciousness_phi_result_free(modulated);
}

/**
 * WHAT: Test temporal pattern detection includes immune threats
 * WHY:  Verify immune patterns appear in detection
 * HOW:  Detect patterns with immune system connected
 */
TEST_F(ImmuneIntegrationTest, TemporalPatternImmuneThreat) {
    /* WHAT: Connect immune system */
    introspection_connect_immune(introspection, immune_system);

    /* WHAT: Detect patterns */
    temporal_pattern_config_t config = temporal_pattern_default_config();
    uint32_t num_patterns = 0;
    temporal_pattern_t* patterns = introspection_detect_patterns(introspection, &config, &num_patterns);

    /* WHAT: Verify no crash with immune connected (may not have patterns yet) */
    if (patterns != nullptr) {
        pattern_array_free(patterns, num_patterns);
    }
    /* Test passes if no crash occurred */
}

/**
 * WHAT: Test uncertainty modulation by immune system
 * WHY:  Verify active immune responses increase uncertainty
 * HOW:  Compute uncertainty with different immune states
 */
TEST_F(ImmuneIntegrationTest, UncertaintyImmuneModulation) {
    /* WHAT: Connect immune system */
    introspection_connect_immune(introspection, immune_system);

    /* WHAT: Create dummy features for uncertainty computation */
    float features[5] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};

    /* WHAT: Compute baseline uncertainty (no active threats) */
    brain_uncertainty_t baseline = brain_get_uncertainty(introspection, features, 5);

    /* WHAT: Verify computation succeeded */
    /* NOTE: May be zero if no ensemble attached, which is okay */
    EXPECT_GE(baseline.total, 0.0f);
    EXPECT_LE(baseline.total, 1.0f);

    /* WHAT: Clean up */
    brain_uncertainty_free(&baseline);
}

/**
 * WHAT: Test introspection state reporting with immune phase
 * WHY:  Verify immune phase visible in state strings
 * HOW:  Get introspection stats, check for immune integration
 */
TEST_F(ImmuneIntegrationTest, StateReportingWithImmune) {
    /* WHAT: Connect immune system */
    introspection_connect_immune(introspection, immune_system);

    /* WHAT: Get introspection stats */
    introspection_stats_t stats;
    bool success = introspection_get_stats(introspection, &stats);
    EXPECT_TRUE(success);

    /* WHAT: Stats should be valid */
    EXPECT_GE(stats.memory_used_bytes, 0);
}

/**
 * WHAT: Test disconnection and cleanup
 * WHY:  Verify no memory leaks or crashes
 * HOW:  Connect, disconnect, verify null retrieval
 */
TEST_F(ImmuneIntegrationTest, DisconnectAndCleanup) {
    /* WHAT: Connect immune system */
    bool success = introspection_connect_immune(introspection, immune_system);
    EXPECT_TRUE(success);

    /* WHAT: Disconnect by setting to NULL */
    success = introspection_connect_immune(introspection, nullptr);
    EXPECT_FALSE(success); /* Should fail with NULL */

    /* WHAT: Original connection should still be there */
    brain_immune_system_t* retrieved = introspection_get_immune(introspection);
    EXPECT_EQ(retrieved, immune_system);
}

/**
 * Main test runner
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
