/**
 * @file test_nimcp_collective_phi.cpp
 * @brief Unit tests for collective phi (IIT metrics)
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/collective_cognition/nimcp_collective_phi.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class CollectivePhiTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = collective_phi_default_config();
        cps_ = collective_phi_create(&config_);
        ASSERT_NE(cps_, nullptr);
    }

    void TearDown() override {
        if (cps_) {
            collective_phi_destroy(cps_);
            cps_ = nullptr;
        }
    }

    collective_phi_config_t config_;
    collective_phi_system_t* cps_ = nullptr;
};

/*=============================================================================
 * Lifecycle Tests
 *===========================================================================*/

TEST_F(CollectivePhiTest, CreateWithNullConfig) {
    collective_phi_system_t* cps = collective_phi_create(nullptr);
    ASSERT_NE(cps, nullptr);
    collective_phi_destroy(cps);
}

TEST_F(CollectivePhiTest, DestroyNull) {
    collective_phi_destroy(nullptr);  // Should not crash
}

TEST_F(CollectivePhiTest, Reset) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 2, 0.6f), 0);

    EXPECT_EQ(collective_phi_reset(cps_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_phi_get(cps_, &phi), 0);
    EXPECT_EQ(phi.phi_total, 0.0f);
}

/*=============================================================================
 * Instance Management Tests
 *===========================================================================*/

TEST_F(CollectivePhiTest, RegisterInstance) {
    EXPECT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
}

TEST_F(CollectivePhiTest, RegisterMultipleInstances) {
    EXPECT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    EXPECT_EQ(collective_phi_register_instance(cps_, 2, 0.6f), 0);
    EXPECT_EQ(collective_phi_register_instance(cps_, 3, 0.7f), 0);
}

TEST_F(CollectivePhiTest, RegisterDuplicateInstance) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    EXPECT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), -1);
}

TEST_F(CollectivePhiTest, UnregisterInstance) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    EXPECT_EQ(collective_phi_unregister_instance(cps_, 1), 0);
}

TEST_F(CollectivePhiTest, UnregisterNonexistentInstance) {
    EXPECT_EQ(collective_phi_unregister_instance(cps_, 999), -1);
}

TEST_F(CollectivePhiTest, UpdateLocalPhi) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    EXPECT_EQ(collective_phi_update_local(cps_, 1, 0.8f), 0);

    instance_phi_contribution_t contrib;
    ASSERT_EQ(collective_phi_get_contribution(cps_, 1, &contrib), 0);
    EXPECT_FLOAT_EQ(contrib.local_phi, 0.8f);
}

TEST_F(CollectivePhiTest, UpdateLocalPhiNonexistentInstance) {
    EXPECT_EQ(collective_phi_update_local(cps_, 999, 0.5f), -1);
}

/*=============================================================================
 * Information Flow Tests
 *===========================================================================*/

TEST_F(CollectivePhiTest, UpdateFlow) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 2, 0.6f), 0);

    information_flow_t flow;
    memset(&flow, 0, sizeof(flow));
    flow.from_instance = 1;
    flow.to_instance = 2;
    flow.flow_rate = 10.0f;
    flow.mutual_information = 0.5f;
    flow.transfer_entropy = 0.3f;

    EXPECT_EQ(collective_phi_update_flow(cps_, 1, 2, &flow), 0);
}

TEST_F(CollectivePhiTest, GetFlow) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 2, 0.6f), 0);

    information_flow_t flow_in;
    memset(&flow_in, 0, sizeof(flow_in));
    flow_in.from_instance = 1;
    flow_in.to_instance = 2;
    flow_in.flow_rate = 10.0f;
    flow_in.mutual_information = 0.5f;
    ASSERT_EQ(collective_phi_update_flow(cps_, 1, 2, &flow_in), 0);

    information_flow_t flow_out;
    ASSERT_EQ(collective_phi_get_flow(cps_, 1, 2, &flow_out), 0);
    EXPECT_FLOAT_EQ(flow_out.flow_rate, 10.0f);
    EXPECT_FLOAT_EQ(flow_out.mutual_information, 0.5f);
}

TEST_F(CollectivePhiTest, GetFlowInvalidInstances) {
    information_flow_t flow;
    EXPECT_EQ(collective_phi_get_flow(cps_, 1, 2, &flow), -1);
}

/*=============================================================================
 * Computation Tests
 *===========================================================================*/

TEST_F(CollectivePhiTest, UpdateEmpty) {
    EXPECT_EQ(collective_phi_update(cps_), 0);
}

TEST_F(CollectivePhiTest, UpdateSingleInstance) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    EXPECT_EQ(collective_phi_update(cps_), 0);
}

TEST_F(CollectivePhiTest, UpdateMultipleInstances) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 2, 0.6f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 3, 0.7f), 0);

    EXPECT_EQ(collective_phi_update(cps_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_phi_get(cps_, &phi), 0);
    EXPECT_GE(phi.phi_total, 0.0f);
}

TEST_F(CollectivePhiTest, GetPhi) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 2, 0.6f), 0);
    ASSERT_EQ(collective_phi_update(cps_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_phi_get(cps_, &phi), 0);

    // Phi should be non-negative
    EXPECT_GE(phi.phi_local, 0.0f);
    EXPECT_GE(phi.phi_network, 0.0f);
    EXPECT_GE(phi.phi_total, 0.0f);

    // IIT decomposition should be valid
    EXPECT_GE(phi.information, 0.0f);
    EXPECT_GE(phi.integration, 0.0f);
    EXPECT_GE(phi.exclusion, 0.0f);
}

TEST_F(CollectivePhiTest, GetLevel) {
    // No instances = no consciousness
    EXPECT_EQ(collective_phi_get_level(cps_), COLLECTIVE_CONSCIOUSNESS_NONE);
}

TEST_F(CollectivePhiTest, GetContribution) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_update(cps_), 0);

    instance_phi_contribution_t contrib;
    ASSERT_EQ(collective_phi_get_contribution(cps_, 1, &contrib), 0);
    EXPECT_EQ(contrib.instance_id, 1u);
    EXPECT_FLOAT_EQ(contrib.local_phi, 0.5f);
}

TEST_F(CollectivePhiTest, GetContributionNonexistentInstance) {
    instance_phi_contribution_t contrib;
    EXPECT_EQ(collective_phi_get_contribution(cps_, 999, &contrib), -1);
}

/*=============================================================================
 * Qualia Tests
 *===========================================================================*/

TEST_F(CollectivePhiTest, GetQualia) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_update(cps_), 0);

    qualia_report_t report;
    ASSERT_EQ(collective_phi_get_qualia(cps_, &report), 0);

    // Qualia dimensions should be in valid ranges
    EXPECT_GE(report.valence, -1.0f);
    EXPECT_LE(report.valence, 1.0f);
    EXPECT_GE(report.arousal, 0.0f);
    EXPECT_LE(report.arousal, 1.0f);
    EXPECT_GE(report.complexity, 0.0f);
    EXPECT_LE(report.complexity, 1.0f);
}

TEST_F(CollectivePhiTest, UpdateQualia) {
    qualia_report_t report;
    memset(&report, 0, sizeof(report));
    report.valence = 0.5f;
    report.arousal = 0.7f;
    report.complexity = 0.8f;
    report.coherence = 0.6f;
    report.temporal_depth = 0.4f;
    report.spatial_extent = 0.5f;
    report.agency = 0.7f;
    report.metacognition = 0.3f;

    EXPECT_EQ(collective_phi_update_qualia(cps_, &report), 0);

    qualia_report_t retrieved;
    ASSERT_EQ(collective_phi_get_qualia(cps_, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.valence, 0.5f);
    EXPECT_FLOAT_EQ(retrieved.arousal, 0.7f);
}

/*=============================================================================
 * Network Analysis Tests
 *===========================================================================*/

TEST_F(CollectivePhiTest, GetIntegrationMatrix) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 2, 0.6f), 0);
    ASSERT_EQ(collective_phi_update(cps_), 0);

    float matrix[16];
    uint32_t size = 4;
    ASSERT_EQ(collective_phi_get_integration_matrix(cps_, matrix, &size), 0);
    EXPECT_GE(size, 2u);
}

TEST_F(CollectivePhiTest, ComputeMIP) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 2, 0.6f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 3, 0.7f), 0);
    ASSERT_EQ(collective_phi_update(cps_), 0);

    uint32_t partition[16];
    uint32_t num_groups = 0;
    float mip_phi = collective_phi_compute_mip(cps_, partition, &num_groups);

    EXPECT_GE(mip_phi, 0.0f);
    EXPECT_GE(num_groups, 1u);
}

/*=============================================================================
 * Event Tests
 *===========================================================================*/

TEST_F(CollectivePhiTest, GetEventsEmpty) {
    emergence_event_t events[10];
    uint32_t count = collective_phi_get_events(cps_, events, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(CollectivePhiTest, ClearEvents) {
    // Register instances and update to potentially generate events
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 2, 0.8f), 0);
    ASSERT_EQ(collective_phi_update(cps_), 0);

    collective_phi_clear_events(cps_);

    emergence_event_t events[10];
    uint32_t count = collective_phi_get_events(cps_, events, 10);
    EXPECT_EQ(count, 0u);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(CollectivePhiTest, GetStats) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_update(cps_), 0);

    collective_phi_stats_t stats;
    ASSERT_EQ(collective_phi_get_stats(cps_, &stats), 0);
    EXPECT_GT(stats.computations, 0u);
}

TEST_F(CollectivePhiTest, ResetStats) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_update(cps_), 0);

    collective_phi_reset_stats(cps_);

    collective_phi_stats_t stats;
    ASSERT_EQ(collective_phi_get_stats(cps_, &stats), 0);
    EXPECT_EQ(stats.computations, 0u);
}

/*=============================================================================
 * Consciousness Level Tests
 *===========================================================================*/

TEST_F(CollectivePhiTest, ConsciousnessLevelProgression) {
    // With multiple instances and good integration, phi should increase
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.3f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 2, 0.3f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 3, 0.3f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 4, 0.3f), 0);

    // Add information flow between all pairs
    information_flow_t flow;
    memset(&flow, 0, sizeof(flow));
    flow.flow_rate = 10.0f;
    flow.mutual_information = 0.5f;
    flow.transfer_entropy = 0.3f;

    for (uint32_t i = 1; i <= 4; i++) {
        for (uint32_t j = 1; j <= 4; j++) {
            if (i != j) {
                flow.from_instance = i;
                flow.to_instance = j;
                collective_phi_update_flow(cps_, i, j, &flow);
            }
        }
    }

    ASSERT_EQ(collective_phi_update(cps_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_phi_get(cps_, &phi), 0);

    // Should have some integration
    EXPECT_GT(phi.phi_total, 0.0f);
}

/*=============================================================================
 * Debug Tests
 *===========================================================================*/

TEST_F(CollectivePhiTest, DumpDoesNotCrash) {
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 2, 0.6f), 0);
    ASSERT_EQ(collective_phi_update(cps_), 0);

    collective_phi_dump(cps_);  // Should not crash
    collective_phi_dump(nullptr);  // Should not crash
}

/*=============================================================================
 * Phi Evolution Tests
 *===========================================================================*/

TEST_F(CollectivePhiTest, PhiEvolution) {
    // Register instances
    ASSERT_EQ(collective_phi_register_instance(cps_, 1, 0.5f), 0);
    ASSERT_EQ(collective_phi_register_instance(cps_, 2, 0.5f), 0);

    // Run multiple updates and track phi
    float prev_phi = 0.0f;
    for (int i = 0; i < 20; i++) {
        // Gradually increase local phi
        collective_phi_update_local(cps_, 1, 0.5f + i * 0.01f);
        collective_phi_update_local(cps_, 2, 0.5f + i * 0.01f);

        ASSERT_EQ(collective_phi_update(cps_), 0);

        collective_phi_t phi;
        ASSERT_EQ(collective_phi_get(cps_, &phi), 0);

        // Phi should remain non-negative
        EXPECT_GE(phi.phi_total, 0.0f);
        prev_phi = phi.phi_total;
    }
}

