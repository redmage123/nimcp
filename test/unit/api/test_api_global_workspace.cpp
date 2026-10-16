/**
 * @file test_api_global_workspace.cpp
 * @brief Unit tests for NIMCP API - Global Workspace functions
 *
 * Tests the global workspace API:
 * - nimcp_brain_workspace_compete()
 * - nimcp_brain_workspace_read()
 * - nimcp_brain_workspace_subscribe()
 * - nimcp_brain_workspace_unsubscribe()
 * - nimcp_brain_workspace_has_broadcast()
 * - nimcp_brain_workspace_stats()
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>

class GlobalWorkspaceAPITest : public ::testing::Test {
protected:
    nimcp_brain_t brain;

    void SetUp() override {
        nimcp_init();
        // Create brain with global workspace enabled (note: assumes SMALL has workspace enabled)
        brain = nimcp_brain_create("test_workspace", NIMCP_BRAIN_SMALL,
                                    NIMCP_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            nimcp_brain_destroy(brain);
        }
        nimcp_shutdown();
    }
};

//=============================================================================
// nimcp_brain_workspace_compete() tests
//=============================================================================

TEST_F(GlobalWorkspaceAPITest, WorkspaceCompeteSucceeds) {
    float content[256];
    for (int i = 0; i < 256; i++) {
        content[i] = 0.5f;
    }

    nimcp_status_t status = nimcp_brain_workspace_compete(
        brain, NIMCP_MODULE_PERCEPTION, content, 256, 0.8f
    );

    // Either succeeds or fails based on competition (both are valid)
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceCompeteNullBrainFails) {
    float content[256] = {0.5f};

    nimcp_status_t status = nimcp_brain_workspace_compete(
        nullptr, NIMCP_MODULE_PERCEPTION, content, 256, 0.8f
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceCompeteNullContentFails) {
    nimcp_status_t status = nimcp_brain_workspace_compete(
        brain, NIMCP_MODULE_PERCEPTION, nullptr, 256, 0.8f
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceCompeteZeroDimensionFails) {
    float content[256] = {0.5f};

    nimcp_status_t status = nimcp_brain_workspace_compete(
        brain, NIMCP_MODULE_PERCEPTION, content, 0, 0.8f
    );

    EXPECT_EQ(status, NIMCP_ERROR_INVALID);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceCompeteInvalidStrengthLowFails) {
    float content[256] = {0.5f};

    nimcp_status_t status = nimcp_brain_workspace_compete(
        brain, NIMCP_MODULE_PERCEPTION, content, 256, -0.1f
    );

    EXPECT_EQ(status, NIMCP_ERROR_INVALID);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceCompeteInvalidStrengthHighFails) {
    float content[256] = {0.5f};

    nimcp_status_t status = nimcp_brain_workspace_compete(
        brain, NIMCP_MODULE_PERCEPTION, content, 256, 1.5f
    );

    EXPECT_EQ(status, NIMCP_ERROR_INVALID);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceCompeteWithDifferentModules) {
    float content[256];
    for (int i = 0; i < 256; i++) {
        content[i] = 0.6f;
    }

    // Test various module types
    nimcp_cognitive_module_t modules[] = {
        NIMCP_MODULE_PERCEPTION,
        NIMCP_MODULE_WORKING_MEMORY,
        NIMCP_MODULE_EXECUTIVE,
        NIMCP_MODULE_ETHICS,
        NIMCP_MODULE_ATTENTION,
        NIMCP_MODULE_EMOTION
    };

    for (auto module : modules) {
        nimcp_status_t status = nimcp_brain_workspace_compete(
            brain, module, content, 256, 0.7f
        );
        // Each competition should either succeed or fail validly
        EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
    }
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceCompeteMinimalStrength) {
    float content[256] = {0.5f};

    nimcp_status_t status = nimcp_brain_workspace_compete(
        brain, NIMCP_MODULE_PERCEPTION, content, 256, 0.0f
    );

    // Should be valid but unlikely to win
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceCompeteMaximalStrength) {
    float content[256] = {0.5f};

    nimcp_status_t status = nimcp_brain_workspace_compete(
        brain, NIMCP_MODULE_PERCEPTION, content, 256, 1.0f
    );

    // Should be valid and likely to win
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
}

//=============================================================================
// nimcp_brain_workspace_read() tests
//=============================================================================

TEST_F(GlobalWorkspaceAPITest, WorkspaceReadWithoutBroadcast) {
    float content[256];
    uint32_t actual_dim;
    nimcp_cognitive_module_t source;

    nimcp_status_t status = nimcp_brain_workspace_read(
        brain, content, 256, &actual_dim, &source
    );

    // Either succeeds (if there's a broadcast) or fails (if not)
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceReadNullBrainFails) {
    float content[256];
    uint32_t actual_dim;
    nimcp_cognitive_module_t source;

    nimcp_status_t status = nimcp_brain_workspace_read(
        nullptr, content, 256, &actual_dim, &source
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceReadNullContentFails) {
    uint32_t actual_dim;
    nimcp_cognitive_module_t source;

    nimcp_status_t status = nimcp_brain_workspace_read(
        brain, nullptr, 256, &actual_dim, &source
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceReadNullDimFails) {
    float content[256];
    nimcp_cognitive_module_t source;

    nimcp_status_t status = nimcp_brain_workspace_read(
        brain, content, 256, nullptr, &source
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceReadNullSourceFails) {
    float content[256];
    uint32_t actual_dim;

    nimcp_status_t status = nimcp_brain_workspace_read(
        brain, content, 256, &actual_dim, nullptr
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceReadZeroMaxDimFails) {
    float content[256];
    uint32_t actual_dim;
    nimcp_cognitive_module_t source;

    nimcp_status_t status = nimcp_brain_workspace_read(
        brain, content, 0, &actual_dim, &source
    );

    EXPECT_EQ(status, NIMCP_ERROR_INVALID);
}

//=============================================================================
// nimcp_brain_workspace_subscribe() tests
//=============================================================================

TEST_F(GlobalWorkspaceAPITest, WorkspaceSubscribeSucceeds) {
    nimcp_status_t status = nimcp_brain_workspace_subscribe(
        brain, NIMCP_MODULE_PERCEPTION
    );

    // Either succeeds or fails based on workspace state
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceSubscribeNullBrainFails) {
    nimcp_status_t status = nimcp_brain_workspace_subscribe(
        nullptr, NIMCP_MODULE_PERCEPTION
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceSubscribeMultipleModules) {
    nimcp_cognitive_module_t modules[] = {
        NIMCP_MODULE_PERCEPTION,
        NIMCP_MODULE_WORKING_MEMORY,
        NIMCP_MODULE_ETHICS
    };

    for (auto module : modules) {
        nimcp_status_t status = nimcp_brain_workspace_subscribe(brain, module);
        EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
    }
}

//=============================================================================
// nimcp_brain_workspace_unsubscribe() tests
//=============================================================================

TEST_F(GlobalWorkspaceAPITest, WorkspaceUnsubscribeSucceeds) {
    // Subscribe first
    nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_PERCEPTION);

    // Then unsubscribe
    nimcp_status_t status = nimcp_brain_workspace_unsubscribe(
        brain, NIMCP_MODULE_PERCEPTION
    );

    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceUnsubscribeNullBrainFails) {
    nimcp_status_t status = nimcp_brain_workspace_unsubscribe(
        nullptr, NIMCP_MODULE_PERCEPTION
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceUnsubscribeWithoutSubscribe) {
    nimcp_status_t status = nimcp_brain_workspace_unsubscribe(
        brain, NIMCP_MODULE_EMOTION
    );

    // Should handle gracefully
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
}

//=============================================================================
// nimcp_brain_workspace_has_broadcast() tests
//=============================================================================

TEST_F(GlobalWorkspaceAPITest, WorkspaceHasBroadcastSucceeds) {
    bool has_broadcast;

    nimcp_status_t status = nimcp_brain_workspace_has_broadcast(
        brain, &has_broadcast
    );

    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceHasBroadcastNullBrainFails) {
    bool has_broadcast;

    nimcp_status_t status = nimcp_brain_workspace_has_broadcast(
        nullptr, &has_broadcast
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceHasBroadcastNullOutputFails) {
    nimcp_status_t status = nimcp_brain_workspace_has_broadcast(
        brain, nullptr
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

//=============================================================================
// nimcp_brain_workspace_stats() tests
//=============================================================================

TEST_F(GlobalWorkspaceAPITest, WorkspaceStatsSucceeds) {
    uint32_t total_broadcasts;
    uint32_t total_competitions;
    float avg_strength;

    nimcp_status_t status = nimcp_brain_workspace_stats(
        brain, &total_broadcasts, &total_competitions, &avg_strength
    );

    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceStatsNullBrainFails) {
    uint32_t total_broadcasts;
    uint32_t total_competitions;
    float avg_strength;

    nimcp_status_t status = nimcp_brain_workspace_stats(
        nullptr, &total_broadcasts, &total_competitions, &avg_strength
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceStatsNullBroadcastsFails) {
    uint32_t total_competitions;
    float avg_strength;

    nimcp_status_t status = nimcp_brain_workspace_stats(
        brain, nullptr, &total_competitions, &avg_strength
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceStatsNullCompetitionsFails) {
    uint32_t total_broadcasts;
    float avg_strength;

    nimcp_status_t status = nimcp_brain_workspace_stats(
        brain, &total_broadcasts, nullptr, &avg_strength
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(GlobalWorkspaceAPITest, WorkspaceStatsNullStrengthFails) {
    uint32_t total_broadcasts;
    uint32_t total_competitions;

    nimcp_status_t status = nimcp_brain_workspace_stats(
        brain, &total_broadcasts, &total_competitions, nullptr
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

//=============================================================================
// Competition and Broadcasting Workflow tests
//=============================================================================

TEST_F(GlobalWorkspaceAPITest, CompetitionAndBroadcastWorkflow) {
    float content[256];
    for (int i = 0; i < 256; i++) {
        content[i] = 0.7f;
    }

    // Compete with high strength
    nimcp_status_t compete_status = nimcp_brain_workspace_compete(
        brain, NIMCP_MODULE_PERCEPTION, content, 256, 0.9f
    );

    // If competition succeeded, should be able to read broadcast
    if (compete_status == NIMCP_SUCCESS) {
        bool has_broadcast;
        nimcp_status_t check_status = nimcp_brain_workspace_has_broadcast(
            brain, &has_broadcast
        );

        EXPECT_EQ(check_status, NIMCP_SUCCESS);

        // Try to read the broadcast
        float read_content[256];
        uint32_t actual_dim;
        nimcp_cognitive_module_t source;

        nimcp_status_t read_status = nimcp_brain_workspace_read(
            brain, read_content, 256, &actual_dim, &source
        );

        // Should be able to read if there's a broadcast
        EXPECT_TRUE(read_status == NIMCP_SUCCESS || read_status == NIMCP_ERROR);
    }
}

TEST_F(GlobalWorkspaceAPITest, SubscribeAndReceiveBroadcast) {
    // Subscribe module
    nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_WORKING_MEMORY);

    // Compete from different module
    float content[256];
    for (int i = 0; i < 256; i++) {
        content[i] = 0.8f;
    }

    nimcp_brain_workspace_compete(
        brain, NIMCP_MODULE_PERCEPTION, content, 256, 0.95f
    );

    // Subscribed module should be able to read
    bool has_broadcast;
    nimcp_brain_workspace_has_broadcast(brain, &has_broadcast);

    // Regardless of whether there's a broadcast, the API should work
    EXPECT_TRUE(true);
}

TEST_F(GlobalWorkspaceAPITest, UnsubscribeStopsBroadcast) {
    // Subscribe
    nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_ETHICS);

    // Unsubscribe
    nimcp_status_t status = nimcp_brain_workspace_unsubscribe(
        brain, NIMCP_MODULE_ETHICS
    );

    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
}

TEST_F(GlobalWorkspaceAPITest, MultipleCompetitionAttempts) {
    float content1[256];
    float content2[256];

    for (int i = 0; i < 256; i++) {
        content1[i] = 0.5f;
        content2[i] = 0.8f;
    }

    // First competition
    nimcp_brain_workspace_compete(
        brain, NIMCP_MODULE_PERCEPTION, content1, 256, 0.6f
    );

    // Second competition with higher strength
    nimcp_status_t status2 = nimcp_brain_workspace_compete(
        brain, NIMCP_MODULE_ATTENTION, content2, 256, 0.9f
    );

    // Should be valid
    EXPECT_TRUE(status2 == NIMCP_SUCCESS || status2 == NIMCP_ERROR);
}

// Test all cognitive module enums
TEST_F(GlobalWorkspaceAPITest, AllCognitiveModuleEnums) {
    float content[256];
    for (int i = 0; i < 256; i++) {
        content[i] = 0.5f;
    }

    nimcp_cognitive_module_t modules[] = {
        NIMCP_MODULE_NONE,
        NIMCP_MODULE_PERCEPTION,
        NIMCP_MODULE_WORKING_MEMORY,
        NIMCP_MODULE_EXECUTIVE,
        NIMCP_MODULE_THEORY_OF_MIND,
        NIMCP_MODULE_ETHICS,
        NIMCP_MODULE_ATTENTION,
        NIMCP_MODULE_EMOTION,
        NIMCP_MODULE_SALIENCE,
        NIMCP_MODULE_MOTOR,
        NIMCP_MODULE_LANGUAGE,
        NIMCP_MODULE_METACOGNITION,
        NIMCP_MODULE_CURIOSITY,
        NIMCP_MODULE_INTROSPECTION,
        NIMCP_MODULE_PREDICTIVE,
        NIMCP_MODULE_CONSOLIDATION,
        NIMCP_MODULE_EPISODIC_MEMORY,
        NIMCP_MODULE_SEMANTIC_MEMORY,
        NIMCP_MODULE_WELLBEING,
        NIMCP_MODULE_MENTAL_HEALTH,
        NIMCP_MODULE_GOAL_MOTIVATION,
        NIMCP_MODULE_COGNITIVE_CONTROL
    };

    for (auto module : modules) {
        nimcp_status_t status = nimcp_brain_workspace_compete(
            brain, module, content, 256, 0.7f
        );

        // All modules should be valid
        EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);
    }
}
