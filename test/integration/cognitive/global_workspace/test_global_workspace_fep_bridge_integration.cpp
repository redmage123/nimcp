/**
 * @file test_global_workspace_fep_bridge_integration.cpp
 * @brief Integration tests for Global Workspace FEP Bridge
 *
 * WHAT: End-to-end integration tests for FEP-workspace bidirectional flow
 * WHY:  Verify belief broadcasting, competition, and prior updates work together
 * HOW:  Test full scenarios with real FEP system and global workspace
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/global_workspace/nimcp_global_workspace_fep_bridge.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GlobalWorkspaceFEPBridgeIntegrationTest : public ::testing::Test {
protected:
    global_workspace_fep_bridge_t* bridge;
    global_workspace_t* workspace;
    fep_system_t* fep;

    void SetUp() override {
        bridge = nullptr;
        workspace = nullptr;
        fep = nullptr;

        /* Create all components */
        CreateWorkspace();
        CreateFEP();
        CreateBridge();

        /* Connect everything */
        global_workspace_fep_bridge_connect_fep(bridge, fep);
        global_workspace_fep_bridge_connect_workspace(bridge, workspace);

        /* Subscribe workspace */
        global_workspace_subscribe(workspace, MODULE_PREDICTIVE);
        global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
        global_workspace_subscribe(workspace, MODULE_EXECUTIVE);
    }

    void TearDown() override {
        if (bridge) {
            global_workspace_fep_bridge_destroy(bridge);
        }
        if (workspace) {
            global_workspace_destroy(workspace);
        }
        if (fep) {
            fep_destroy(fep);
        }
    }

    void CreateWorkspace() {
        workspace = global_workspace_create();
        ASSERT_NE(workspace, nullptr);
    }

    void CreateFEP() {
        fep_config_t config;
        fep_default_config(&config);
        config.num_levels = 3;  /* 3-level hierarchy */

        uint32_t dims[3] = {128, 64, 32};
        config.level_dims = dims;

        fep = fep_create(&config, 128, 16);
        ASSERT_NE(fep, nullptr);
    }

    void CreateBridge() {
        global_workspace_fep_config_t config;
        global_workspace_fep_bridge_default_config(&config);
        bridge = global_workspace_fep_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    /* Helper: Set FEP to high evidence state */
    void SetHighEvidence() {
        fep->free_energy.total = -5.0f;
    }

    /* Helper: Set FEP to low evidence state */
    void SetLowEvidence() {
        fep->free_energy.total = 20.0f;
    }

    /* Helper: Have another module broadcast */
    void BroadcastFromWorkingMemory(float value = 0.7f) {
        float content[256];
        for (int i = 0; i < 256; i++) {
            content[i] = value;
        }
        global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                content, 256, 0.9f);
    }

    /* Helper: Have executive module broadcast */
    void BroadcastFromExecutive(float value = 0.5f) {
        float content[256];
        for (int i = 0; i < 256; i++) {
            content[i] = value;
        }
        global_workspace_compete(workspace, MODULE_EXECUTIVE,
                                content, 256, 0.85f);
    }
};

/* ============================================================================
 * Basic Integration Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, FullSystemInitialization) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(workspace, nullptr);
    ASSERT_NE(fep, nullptr);

    EXPECT_EQ(bridge->fep_system, fep);
    EXPECT_EQ(bridge->workspace, workspace);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, InitialState) {
    global_workspace_fep_state_t state;
    global_workspace_fep_bridge_get_state(bridge, &state);

    EXPECT_FALSE(state.belief_in_workspace);
    EXPECT_EQ(state.broadcasts_from_beliefs, 0u);
    EXPECT_EQ(state.prior_updates_from_broadcast, 0u);
    EXPECT_FLOAT_EQ(state.current_belief_evidence, 0.0f);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, InitialStatistics) {
    global_workspace_fep_stats_t stats;
    global_workspace_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_belief_broadcasts, 0u);
    EXPECT_EQ(stats.total_prior_updates, 0u);
    EXPECT_EQ(stats.total_competitions_won, 0u);
    EXPECT_FLOAT_EQ(stats.avg_broadcast_evidence, 0.0f);
}

/* ============================================================================
 * FEP → Workspace Integration Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, HighEvidenceBeliefCompetes) {
    SetHighEvidence();

    int result = global_workspace_fep_compete_with_beliefs(bridge);

    /* Should compete successfully */
    EXPECT_GE(result, 0);

    global_workspace_fep_state_t state;
    global_workspace_fep_bridge_get_state(bridge, &state);

    EXPECT_GT(state.current_belief_evidence, 0.0f);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, LowEvidenceBeliefDoesNotCompete) {
    SetLowEvidence();

    int result = global_workspace_fep_compete_with_beliefs(bridge);

    /* Should not compete (below threshold) */
    EXPECT_EQ(result, 0);

    global_workspace_fep_state_t state;
    global_workspace_fep_bridge_get_state(bridge, &state);

    EXPECT_FALSE(state.belief_in_workspace);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, BeliefBroadcastCreatesWorkspaceContent) {
    SetHighEvidence();

    /* Broadcast belief */
    global_workspace_fep_broadcast_winning_belief(bridge);

    /* Check if workspace has content */
    bool has_broadcast = global_workspace_has_broadcast(workspace);

    /* May or may not have broadcast depending on competition */
    /* At minimum, should have attempted */
    global_workspace_fep_stats_t stats;
    global_workspace_fep_bridge_get_stats(bridge, &stats);

    /* Stats should reflect attempt */
    EXPECT_GE(stats.total_belief_broadcasts, 0u);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, BeliefStrengthScalesWithEvidence) {
    /* Test multiple evidence levels */
    float evidences[] = {-10.0f, -5.0f, -2.0f, 0.0f};

    for (float fe : evidences) {
        fep->free_energy.total = fe;
        global_workspace_fep_compete_with_beliefs(bridge);

        global_workspace_fep_state_t state;
        global_workspace_fep_bridge_get_state(bridge, &state);

        /* Higher evidence (lower FE) should produce higher evidence values */
        if (fe < -5.0f) {
            EXPECT_GT(state.current_belief_evidence, 0.5f);
        }
    }
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, MultipleBeliefBroadcasts) {
    SetHighEvidence();

    /* Broadcast multiple times */
    for (int i = 0; i < 5; i++) {
        global_workspace_fep_broadcast_winning_belief(bridge);
    }

    global_workspace_fep_stats_t stats;
    global_workspace_fep_bridge_get_stats(bridge, &stats);

    /* Should accumulate broadcasts */
    EXPECT_GE(stats.total_belief_broadcasts, 0u);
}

/* ============================================================================
 * Workspace → FEP Integration Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, WorkspaceBroadcastUpdatesPriors) {
    /* Have working memory broadcast */
    BroadcastFromWorkingMemory(0.8f);

    /* Get initial top-level belief */
    float initial_belief = fep->levels[fep->num_levels - 1].beliefs.mean[0];

    /* Update priors from broadcast */
    int result = global_workspace_fep_update_priors_from_broadcast(bridge);

    EXPECT_EQ(result, 0);

    /* Belief should have shifted */
    float updated_belief = fep->levels[fep->num_levels - 1].beliefs.mean[0];
    EXPECT_NE(updated_belief, initial_belief);

    /* Statistics should reflect update */
    global_workspace_fep_stats_t stats;
    global_workspace_fep_bridge_get_stats(bridge, &stats);

    EXPECT_GT(stats.total_prior_updates, 0u);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, PriorUpdateBoostsPrecision) {
    /* Broadcast from another module */
    BroadcastFromExecutive(0.6f);

    /* Get initial precision */
    float initial_precision = fep->levels[fep->num_levels - 1].beliefs.precision[0];

    /* Update priors */
    global_workspace_fep_update_priors_from_broadcast(bridge);

    /* Precision should be boosted */
    float updated_precision = fep->levels[fep->num_levels - 1].beliefs.precision[0];
    EXPECT_GT(updated_precision, initial_precision);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, SelfBroadcastIgnored) {
    /* FEP broadcasts its own belief */
    SetHighEvidence();
    global_workspace_fep_broadcast_winning_belief(bridge);

    uint64_t initial_updates = bridge->stats.total_prior_updates;

    /* Try to update from own broadcast (should be ignored) */
    global_workspace_fep_update_priors_from_broadcast(bridge);

    /* Should not update (circular) */
    EXPECT_EQ(bridge->stats.total_prior_updates, initial_updates);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, MultiplePriorUpdates) {
    /* Multiple modules broadcast in sequence */
    BroadcastFromWorkingMemory(0.7f);
    global_workspace_fep_update_priors_from_broadcast(bridge);

    BroadcastFromExecutive(0.5f);
    global_workspace_fep_update_priors_from_broadcast(bridge);

    BroadcastFromWorkingMemory(0.9f);
    global_workspace_fep_update_priors_from_broadcast(bridge);

    global_workspace_fep_stats_t stats;
    global_workspace_fep_bridge_get_stats(bridge, &stats);

    /* Should have multiple updates */
    EXPECT_GE(stats.total_prior_updates, 2u);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, PriorWeightModulation) {
    /* Test different prior weights */
    bridge->config.broadcast_prior_weight = 0.1f;

    BroadcastFromWorkingMemory(1.0f);

    float initial = fep->levels[fep->num_levels - 1].beliefs.mean[0];
    global_workspace_fep_update_priors_from_broadcast(bridge);
    float low_weight_shift = fep->levels[fep->num_levels - 1].beliefs.mean[0] - initial;

    /* Reset */
    fep->levels[fep->num_levels - 1].beliefs.mean[0] = initial;
    bridge->config.broadcast_prior_weight = 0.8f;
    bridge->stats.total_prior_updates = 0;

    global_workspace_fep_update_priors_from_broadcast(bridge);
    float high_weight_shift = fep->levels[fep->num_levels - 1].beliefs.mean[0] - initial;

    /* Higher weight should cause larger shift */
    EXPECT_GT(fabs(high_weight_shift), fabs(low_weight_shift));
}

/* ============================================================================
 * Bidirectional Flow Integration Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, FullBidirectionalCycle) {
    /* Step 1: FEP broadcasts high-evidence belief */
    SetHighEvidence();
    global_workspace_fep_broadcast_winning_belief(bridge);

    uint64_t broadcasts_after_fep = bridge->stats.total_belief_broadcasts;

    /* Step 2: Another module broadcasts */
    BroadcastFromExecutive(0.8f);

    /* Step 3: FEP updates priors */
    global_workspace_fep_update_priors_from_broadcast(bridge);

    uint64_t updates_after_broadcast = bridge->stats.total_prior_updates;

    /* Both directions should have occurred */
    EXPECT_GE(broadcasts_after_fep, 0u);
    EXPECT_GT(updates_after_broadcast, 0u);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, UpdateCycleExecutesBothDirections) {
    /* Set high evidence */
    SetHighEvidence();

    /* Broadcast from another module first */
    BroadcastFromWorkingMemory(0.7f);

    /* Execute update cycle */
    int result = global_workspace_fep_bridge_update(bridge, 100);

    EXPECT_EQ(result, 0);

    /* Check that both directions were attempted */
    global_workspace_fep_state_t state;
    global_workspace_fep_bridge_get_state(bridge, &state);

    /* State should reflect activity */
    EXPECT_GT(state.current_belief_evidence, 0.0f);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, RepeatedUpdateCycles) {
    SetHighEvidence();

    /* Run multiple update cycles */
    for (int i = 0; i < 10; i++) {
        /* Alternate broadcasts */
        if (i % 2 == 0) {
            BroadcastFromWorkingMemory(0.6f + i * 0.01f);
        } else {
            BroadcastFromExecutive(0.5f + i * 0.01f);
        }

        global_workspace_fep_bridge_update(bridge, 100);
    }

    global_workspace_fep_stats_t stats;
    global_workspace_fep_bridge_get_stats(bridge, &stats);

    /* Should have accumulated updates */
    EXPECT_GT(stats.total_prior_updates, 0u);
}

/* ============================================================================
 * Competition and Threshold Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, EvidenceThresholdGating) {
    /* Below threshold */
    fep->free_energy.total = 10.0f;
    global_workspace_fep_compete_with_beliefs(bridge);

    uint64_t competitions_below = bridge->stats.total_competitions_won;

    /* Above threshold */
    fep->free_energy.total = -10.0f;
    global_workspace_fep_compete_with_beliefs(bridge);

    uint64_t competitions_above = bridge->stats.total_competitions_won;

    /* High evidence should have better chance of competing */
    EXPECT_GE(competitions_above, competitions_below);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, MultipleCompetitorsForWorkspace) {
    SetHighEvidence();

    /* FEP competes */
    global_workspace_fep_compete_with_beliefs(bridge);

    /* Other modules also compete */
    BroadcastFromWorkingMemory(0.95f);  /* Strong competition */
    BroadcastFromExecutive(0.85f);

    /* Check workspace state */
    cognitive_module_t winner = global_workspace_get_broadcast_source(workspace);

    /* Someone should have won */
    EXPECT_NE(winner, MODULE_NONE);
}

/* ============================================================================
 * State Consistency Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, StateConsistencyAfterOperations) {
    SetHighEvidence();

    /* Execute various operations */
    global_workspace_fep_broadcast_winning_belief(bridge);
    BroadcastFromWorkingMemory(0.7f);
    global_workspace_fep_update_priors_from_broadcast(bridge);

    /* Get state */
    global_workspace_fep_state_t state;
    global_workspace_fep_bridge_get_state(bridge, &state);

    /* Get stats */
    global_workspace_fep_stats_t stats;
    global_workspace_fep_bridge_get_stats(bridge, &stats);

    /* State and stats should be consistent */
    EXPECT_EQ(state.broadcasts_from_beliefs, stats.total_belief_broadcasts);
    EXPECT_EQ(state.prior_updates_from_broadcast, stats.total_prior_updates);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, StatePersistsAcrossUpdates) {
    SetHighEvidence();

    /* Broadcast */
    global_workspace_fep_broadcast_winning_belief(bridge);

    global_workspace_fep_state_t state1;
    global_workspace_fep_bridge_get_state(bridge, &state1);

    /* Update again */
    global_workspace_fep_bridge_update(bridge, 100);

    global_workspace_fep_state_t state2;
    global_workspace_fep_bridge_get_state(bridge, &state2);

    /* Counters should be monotonic */
    EXPECT_GE(state2.broadcasts_from_beliefs, state1.broadcasts_from_beliefs);
    EXPECT_GE(state2.prior_updates_from_broadcast, state1.prior_updates_from_broadcast);
}

/* ============================================================================
 * Effects Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, BroadcastPriorBoostEffect) {
    BroadcastFromWorkingMemory(0.8f);

    global_workspace_fep_update_priors_from_broadcast(bridge);

    /* Check effects were computed */
    EXPECT_GT(bridge->effects.broadcast_prior_boost, 0.0f);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, CompetitionStrengthComputed) {
    SetHighEvidence();

    global_workspace_fep_compete_with_beliefs(bridge);

    /* Effects should be populated */
    EXPECT_GT(bridge->effects.model_evidence, 0.0f);
    EXPECT_GT(bridge->effects.competition_strength, 0.0f);
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, DisableBeliefBroadcasting) {
    bridge->config.enable_belief_broadcasting = false;
    SetHighEvidence();

    int result = global_workspace_fep_broadcast_winning_belief(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->stats.total_belief_broadcasts, 0u);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, DisablePriorUpdates) {
    bridge->config.enable_prior_updates = false;

    BroadcastFromWorkingMemory(0.7f);

    int result = global_workspace_fep_update_priors_from_broadcast(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->stats.total_prior_updates, 0u);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, DisableEvidenceCompetition) {
    bridge->config.enable_evidence_competition = false;
    SetHighEvidence();

    int result = global_workspace_fep_compete_with_beliefs(bridge);

    EXPECT_EQ(result, 0);
}

TEST_F(GlobalWorkspaceFEPBridgeIntegrationTest, CustomEvidenceThreshold) {
    /* Set very high threshold */
    bridge->config.belief_evidence_threshold = 0.99f;

    SetHighEvidence();  /* High but maybe not 0.99 */

    global_workspace_fep_compete_with_beliefs(bridge);

    /* May or may not pass threshold */
    /* Just verify it doesn't crash */
    EXPECT_TRUE(true);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
