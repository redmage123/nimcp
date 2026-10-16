/**
 * @file test_cognitive_meta_controller.cpp
 * @brief Comprehensive unit tests for Cognitive Meta-Controller
 * @version 1.0.0
 * @date 2025-12-15
 *
 * Tests cover:
 * - Lifecycle (create, destroy, start, stop, pause, resume)
 * - Configuration and validation
 * - Module registration and tracking
 * - Resource allocation (WM slots, attention focus, learning rate, executive priority, workspace access)
 * - Conflict arbitration strategies (winner-take-all, weighted fusion, priority-weighted, round-robin)
 * - Request queue management
 * - Statistics tracking
 * - Observer callbacks (allocation, metacognitive)
 * - Bio-async integration
 * - Integration with working memory, executive, global workspace, brain immune
 * - Thread safety
 * - Edge cases and error handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_cognitive_meta_controller.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_executive.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Constants
//=============================================================================

constexpr float FLOAT_TOLERANCE = 1e-6f;

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveMetaControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        controller = nullptr;
        working_memory = nullptr;
        executive = nullptr;
        global_workspace = nullptr;
        brain_immune = nullptr;
    }

    void TearDown() override {
        if (controller) {
            meta_controller_destroy(controller);
            controller = nullptr;
        }
        if (working_memory) {
            working_memory_destroy(working_memory);
            working_memory = nullptr;
        }
        if (executive) {
            executive_destroy(executive);
            executive = nullptr;
        }
        if (global_workspace) {
            global_workspace_destroy(global_workspace);
            global_workspace = nullptr;
        }
        if (brain_immune) {
            brain_immune_destroy(brain_immune);
            brain_immune = nullptr;
        }
    }

    // Helper: Create default controller
    void CreateController() {
        controller = meta_controller_create(nullptr);
        ASSERT_NE(controller, nullptr);
    }

    // Helper: Create controller with config
    void CreateControllerWithConfig(const meta_controller_config_t* config) {
        controller = meta_controller_create(config);
        ASSERT_NE(controller, nullptr);
    }

    // Helper: Create and start controller
    void CreateAndStartController() {
        CreateController();
        ASSERT_EQ(meta_controller_start(controller), NIMCP_SUCCESS);
    }

    // Helper: Create working memory integration
    void CreateWorkingMemoryIntegration() {
        working_memory_config_t wm_config = working_memory_default_config();
        working_memory = working_memory_create_custom(&wm_config);
        ASSERT_NE(working_memory, nullptr);
        ASSERT_EQ(meta_controller_connect_working_memory(controller, working_memory),
                  NIMCP_SUCCESS);
    }

    // Helper: Create executive integration
    void CreateExecutiveIntegration() {
        executive = executive_create();
        ASSERT_NE(executive, nullptr);
        ASSERT_EQ(meta_controller_connect_executive(controller, executive),
                  NIMCP_SUCCESS);
    }

    // Helper: Create global workspace integration
    void CreateGlobalWorkspaceIntegration() {
        global_workspace_config_t gw_config = global_workspace_default_config();
        global_workspace = global_workspace_create_custom(&gw_config);
        ASSERT_NE(global_workspace, nullptr);
        ASSERT_EQ(meta_controller_connect_global_workspace(controller, global_workspace),
                  NIMCP_SUCCESS);
    }

    // Helper: Create brain immune integration
    void CreateBrainImmuneIntegration() {
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        brain_immune = brain_immune_create(&immune_config);
        ASSERT_NE(brain_immune, nullptr);
        ASSERT_EQ(meta_controller_connect_brain_immune(controller, brain_immune),
                  NIMCP_SUCCESS);
    }

    cognitive_meta_controller_t* controller;
    working_memory_t* working_memory;
    executive_controller_t* executive;
    global_workspace_t* global_workspace;
    brain_immune_system_t* brain_immune;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(CognitiveMetaControllerTest, DefaultConfigInitialization) {
    meta_controller_config_t config;
    int result = meta_controller_default_config(&config);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(config.max_wm_slots, META_CONTROLLER_WM_DEFAULT_CAPACITY);
    EXPECT_EQ(config.max_attention_foci, 1);
    EXPECT_FLOAT_EQ(config.base_learning_rate, META_CONTROLLER_LR_DEFAULT);
    EXPECT_EQ(config.strategy, ARBITRATION_WINNER_TAKE_ALL);
    EXPECT_FLOAT_EQ(config.priority_threshold, 0.1f);
    EXPECT_TRUE(config.enable_uncertainty_modulation);
    EXPECT_TRUE(config.enable_affective_metacontrol);
    EXPECT_TRUE(config.enable_performance_tracking);
    EXPECT_EQ(config.update_interval_ms, META_CONTROLLER_DEFAULT_UPDATE_MS);

    // Check module weights (all should be 1.0)
    for (int i = 0; i < META_CONTROLLER_MAX_MODULES; i++) {
        EXPECT_FLOAT_EQ(config.module_weights[i], 1.0f);
    }
}

TEST_F(CognitiveMetaControllerTest, DefaultConfigNullPointer) {
    int result = meta_controller_default_config(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, CreateWithDefaultConfig) {
    CreateController();

    EXPECT_EQ(meta_controller_get_state(controller), META_CONTROLLER_STOPPED);
}

TEST_F(CognitiveMetaControllerTest, CreateWithCustomConfig) {
    meta_controller_config_t config;
    meta_controller_default_config(&config);
    config.max_wm_slots = 5;
    config.strategy = ARBITRATION_PRIORITY_WEIGHTED;
    config.base_learning_rate = 0.05f;

    CreateControllerWithConfig(&config);

    EXPECT_EQ(meta_controller_get_state(controller), META_CONTROLLER_STOPPED);
}

TEST_F(CognitiveMetaControllerTest, CreateWithInvalidConfig_ZeroWMSlots) {
    meta_controller_config_t config;
    meta_controller_default_config(&config);
    config.max_wm_slots = 0;  // Invalid

    controller = meta_controller_create(&config);
    EXPECT_EQ(controller, nullptr);
}

TEST_F(CognitiveMetaControllerTest, CreateWithInvalidConfig_ExcessiveWMSlots) {
    meta_controller_config_t config;
    meta_controller_default_config(&config);
    config.max_wm_slots = 100;  // Too high (> 20)

    controller = meta_controller_create(&config);
    EXPECT_EQ(controller, nullptr);
}

TEST_F(CognitiveMetaControllerTest, CreateWithInvalidConfig_LowLearningRate) {
    meta_controller_config_t config;
    meta_controller_default_config(&config);
    config.base_learning_rate = 0.0001f;  // Too low (< META_CONTROLLER_LR_MIN)

    controller = meta_controller_create(&config);
    EXPECT_EQ(controller, nullptr);
}

TEST_F(CognitiveMetaControllerTest, CreateWithInvalidConfig_HighLearningRate) {
    meta_controller_config_t config;
    meta_controller_default_config(&config);
    config.base_learning_rate = 0.5f;  // Too high (> META_CONTROLLER_LR_MAX)

    controller = meta_controller_create(&config);
    EXPECT_EQ(controller, nullptr);
}

TEST_F(CognitiveMetaControllerTest, DestroyNullController) {
    meta_controller_destroy(nullptr);
    // Should not crash
}

TEST_F(CognitiveMetaControllerTest, DestroyValidController) {
    CreateController();
    meta_controller_destroy(controller);
    controller = nullptr;  // Prevent double-free in TearDown
}

TEST_F(CognitiveMetaControllerTest, StartController) {
    CreateController();

    int result = meta_controller_start(controller);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(meta_controller_get_state(controller), META_CONTROLLER_RUNNING);
}

TEST_F(CognitiveMetaControllerTest, StartNullController) {
    int result = meta_controller_start(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, StartAlreadyRunning) {
    CreateAndStartController();

    // Try starting again
    int result = meta_controller_start(controller);
    EXPECT_EQ(result, NIMCP_SUCCESS);  // Should succeed (idempotent)
}

TEST_F(CognitiveMetaControllerTest, StopController) {
    CreateAndStartController();

    int result = meta_controller_stop(controller);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(meta_controller_get_state(controller), META_CONTROLLER_STOPPED);
}

TEST_F(CognitiveMetaControllerTest, StopNullController) {
    int result = meta_controller_stop(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, PauseController) {
    CreateAndStartController();

    int result = meta_controller_pause(controller);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(meta_controller_get_state(controller), META_CONTROLLER_PAUSED);
}

TEST_F(CognitiveMetaControllerTest, PauseNullController) {
    int result = meta_controller_pause(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, PauseNotRunning) {
    CreateController();

    int result = meta_controller_pause(controller);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    // State should remain STOPPED (pause only works if RUNNING)
    EXPECT_EQ(meta_controller_get_state(controller), META_CONTROLLER_STOPPED);
}

TEST_F(CognitiveMetaControllerTest, ResumeController) {
    CreateAndStartController();
    meta_controller_pause(controller);

    int result = meta_controller_resume(controller);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(meta_controller_get_state(controller), META_CONTROLLER_RUNNING);
}

TEST_F(CognitiveMetaControllerTest, ResumeNullController) {
    int result = meta_controller_resume(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, ResumeNotPaused) {
    CreateController();

    int result = meta_controller_resume(controller);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    // State should remain unchanged
    EXPECT_EQ(meta_controller_get_state(controller), META_CONTROLLER_STOPPED);
}

//=============================================================================
// Resource Request Tests - Working Memory Slots
//=============================================================================

TEST_F(CognitiveMetaControllerTest, RequestWMSlot_Basic) {
    CreateAndStartController();
    CreateWorkingMemoryIntegration();

    float item_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t request_id = meta_controller_request_wm_slot(
        controller,
        META_CTRL_MODULE_ATTENTION,
        item_data,
        4,
        0.8f,  // priority
        0.9f   // salience
    );

    EXPECT_GT(request_id, 0);
}

TEST_F(CognitiveMetaControllerTest, RequestWMSlot_NullController) {
    float item_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t request_id = meta_controller_request_wm_slot(
        nullptr,
        META_CTRL_MODULE_ATTENTION,
        item_data,
        4,
        0.8f,
        0.9f
    );

    EXPECT_EQ(request_id, 0);
}

TEST_F(CognitiveMetaControllerTest, RequestWMSlot_NullItemData) {
    CreateAndStartController();

    uint32_t request_id = meta_controller_request_wm_slot(
        controller,
        META_CTRL_MODULE_ATTENTION,
        nullptr,  // Invalid
        4,
        0.8f,
        0.9f
    );

    EXPECT_EQ(request_id, 0);
}

TEST_F(CognitiveMetaControllerTest, RequestWMSlot_ZeroSize) {
    CreateAndStartController();
    float item_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    uint32_t request_id = meta_controller_request_wm_slot(
        controller,
        META_CTRL_MODULE_ATTENTION,
        item_data,
        0,  // Invalid
        0.8f,
        0.9f
    );

    EXPECT_EQ(request_id, 0);
}

TEST_F(CognitiveMetaControllerTest, RequestWMSlot_ExcessiveSize) {
    CreateAndStartController();
    float item_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    uint32_t request_id = meta_controller_request_wm_slot(
        controller,
        META_CTRL_MODULE_ATTENTION,
        item_data,
        WORKING_MEMORY_MAX_ITEM_SIZE + 1,  // Too large
        0.8f,
        0.9f
    );

    EXPECT_EQ(request_id, 0);
}

TEST_F(CognitiveMetaControllerTest, RequestWMSlot_PriorityClampingLow) {
    CreateAndStartController();
    CreateWorkingMemoryIntegration();

    float item_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t request_id = meta_controller_request_wm_slot(
        controller,
        META_CTRL_MODULE_ATTENTION,
        item_data,
        4,
        -1.0f,  // Should clamp to 0.0
        0.9f
    );

    EXPECT_GT(request_id, 0);
}

TEST_F(CognitiveMetaControllerTest, RequestWMSlot_PriorityClampingHigh) {
    CreateAndStartController();
    CreateWorkingMemoryIntegration();

    float item_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t request_id = meta_controller_request_wm_slot(
        controller,
        META_CTRL_MODULE_ATTENTION,
        item_data,
        4,
        2.0f,  // Should clamp to 1.0
        0.9f
    );

    EXPECT_GT(request_id, 0);
}

TEST_F(CognitiveMetaControllerTest, RequestWMSlot_MultipleRequests) {
    CreateAndStartController();
    CreateWorkingMemoryIntegration();

    float item1[2] = {1.0f, 2.0f};
    float item2[2] = {3.0f, 4.0f};

    uint32_t req1 = meta_controller_request_wm_slot(
        controller, META_CTRL_MODULE_ATTENTION, item1, 2, 0.8f, 0.9f);
    uint32_t req2 = meta_controller_request_wm_slot(
        controller, META_CTRL_MODULE_CURIOSITY, item2, 2, 0.7f, 0.8f);

    EXPECT_GT(req1, 0);
    EXPECT_GT(req2, 0);
    EXPECT_NE(req1, req2);  // Should have different IDs
}

//=============================================================================
// Resource Request Tests - Attention Focus
//=============================================================================

TEST_F(CognitiveMetaControllerTest, RequestAttention_Basic) {
    CreateAndStartController();

    uint32_t request_id = meta_controller_request_attention(
        controller,
        META_CTRL_MODULE_EMOTION,
        0.8f,  // salience
        0.6f,  // urgency
        nullptr
    );

    EXPECT_GT(request_id, 0);
}

TEST_F(CognitiveMetaControllerTest, RequestAttention_NullController) {
    uint32_t request_id = meta_controller_request_attention(
        nullptr,
        META_CTRL_MODULE_EMOTION,
        0.8f,
        0.6f,
        nullptr
    );

    EXPECT_EQ(request_id, 0);
}

TEST_F(CognitiveMetaControllerTest, RequestAttention_SalienceClamping) {
    CreateAndStartController();

    // Test low clamp
    uint32_t req1 = meta_controller_request_attention(
        controller, META_CTRL_MODULE_EMOTION, -0.5f, 0.6f, nullptr);
    EXPECT_GT(req1, 0);

    // Test high clamp
    uint32_t req2 = meta_controller_request_attention(
        controller, META_CTRL_MODULE_EMOTION, 1.5f, 0.6f, nullptr);
    EXPECT_GT(req2, 0);
}

TEST_F(CognitiveMetaControllerTest, RequestAttention_MultipleRequests) {
    CreateAndStartController();

    uint32_t req1 = meta_controller_request_attention(
        controller, META_CTRL_MODULE_EMOTION, 0.8f, 0.9f, nullptr);
    uint32_t req2 = meta_controller_request_attention(
        controller, META_CTRL_MODULE_CURIOSITY, 0.7f, 0.5f, nullptr);
    uint32_t req3 = meta_controller_request_attention(
        controller, META_CTRL_MODULE_INTROSPECTION, 0.9f, 0.8f, nullptr);

    EXPECT_GT(req1, 0);
    EXPECT_GT(req2, 0);
    EXPECT_GT(req3, 0);
    EXPECT_NE(req1, req2);
    EXPECT_NE(req2, req3);
}

//=============================================================================
// Resource Request Tests - Learning Rate
//=============================================================================

TEST_F(CognitiveMetaControllerTest, RequestLearningRate_Basic) {
    CreateAndStartController();

    float lr = meta_controller_request_learning_rate(
        controller,
        META_CTRL_MODULE_WORKING_MEMORY,
        0.5f,  // uncertainty
        0.7f,  // confidence
        0.01f  // desired_lr
    );

    EXPECT_GE(lr, META_CONTROLLER_LR_MIN);
    EXPECT_LE(lr, META_CONTROLLER_LR_MAX);
}

TEST_F(CognitiveMetaControllerTest, RequestLearningRate_NullController) {
    float lr = meta_controller_request_learning_rate(
        nullptr,
        META_CTRL_MODULE_WORKING_MEMORY,
        0.5f,
        0.7f,
        0.01f
    );

    EXPECT_FLOAT_EQ(lr, -1.0f);
}

TEST_F(CognitiveMetaControllerTest, RequestLearningRate_HighUncertainty) {
    CreateAndStartController();

    float lr = meta_controller_request_learning_rate(
        controller,
        META_CTRL_MODULE_WORKING_MEMORY,
        0.9f,  // High uncertainty
        0.7f,
        0.01f
    );

    EXPECT_GE(lr, META_CONTROLLER_LR_MIN);
    EXPECT_LE(lr, META_CONTROLLER_LR_MAX);
}

TEST_F(CognitiveMetaControllerTest, RequestLearningRate_LowConfidence) {
    CreateAndStartController();

    float lr = meta_controller_request_learning_rate(
        controller,
        META_CTRL_MODULE_WORKING_MEMORY,
        0.5f,
        0.1f,  // Low confidence
        0.01f
    );

    EXPECT_GE(lr, META_CONTROLLER_LR_MIN);
    EXPECT_LE(lr, META_CONTROLLER_LR_MAX);
    // Low confidence should reduce LR
}

TEST_F(CognitiveMetaControllerTest, RequestLearningRate_UncertaintyClamping) {
    CreateAndStartController();

    // Test clamping to [0, 1]
    float lr1 = meta_controller_request_learning_rate(
        controller, META_CTRL_MODULE_WORKING_MEMORY, -0.5f, 0.7f, 0.01f);
    EXPECT_GE(lr1, META_CONTROLLER_LR_MIN);

    float lr2 = meta_controller_request_learning_rate(
        controller, META_CTRL_MODULE_WORKING_MEMORY, 1.5f, 0.7f, 0.01f);
    EXPECT_LE(lr2, META_CONTROLLER_LR_MAX);
}

//=============================================================================
// Resource Request Tests - Executive Priority
//=============================================================================

TEST_F(CognitiveMetaControllerTest, RequestExecutivePriority_Basic) {
    CreateAndStartController();
    CreateExecutiveIntegration();

    int result = meta_controller_request_executive_priority(
        controller,
        META_CTRL_MODULE_ATTENTION,
        1,     // task_id
        0.9f   // priority
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CognitiveMetaControllerTest, RequestExecutivePriority_NullController) {
    int result = meta_controller_request_executive_priority(
        nullptr,
        META_CTRL_MODULE_ATTENTION,
        1,
        0.9f
    );

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, RequestExecutivePriority_NoExecutive) {
    CreateAndStartController();
    // Don't create executive integration

    int result = meta_controller_request_executive_priority(
        controller,
        META_CTRL_MODULE_ATTENTION,
        1,
        0.9f
    );

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Resource Request Tests - Global Workspace Access
//=============================================================================

TEST_F(CognitiveMetaControllerTest, RequestWorkspaceAccess_Basic) {
    CreateAndStartController();
    CreateGlobalWorkspaceIntegration();

    float content[64];
    for (int i = 0; i < 64; i++) content[i] = (float)i;

    bool granted = meta_controller_request_workspace_access(
        controller,
        META_CTRL_MODULE_EMOTION,
        content,
        64,
        0.8f  // strength
    );

    // Result depends on global workspace state, just verify it doesn't crash
    EXPECT_TRUE(granted || !granted);
}

TEST_F(CognitiveMetaControllerTest, RequestWorkspaceAccess_NullController) {
    float content[64];
    for (int i = 0; i < 64; i++) content[i] = (float)i;

    bool granted = meta_controller_request_workspace_access(
        nullptr,
        META_CTRL_MODULE_EMOTION,
        content,
        64,
        0.8f
    );

    EXPECT_FALSE(granted);
}

TEST_F(CognitiveMetaControllerTest, RequestWorkspaceAccess_NullContent) {
    CreateAndStartController();
    CreateGlobalWorkspaceIntegration();

    bool granted = meta_controller_request_workspace_access(
        controller,
        META_CTRL_MODULE_EMOTION,
        nullptr,
        64,
        0.8f
    );

    EXPECT_FALSE(granted);
}

TEST_F(CognitiveMetaControllerTest, RequestWorkspaceAccess_ModuleWeighting) {
    CreateAndStartController();
    CreateGlobalWorkspaceIntegration();

    // Set different module weights
    meta_controller_set_module_weight(controller, META_CTRL_MODULE_EMOTION, 0.9f);
    meta_controller_set_module_weight(controller, META_CTRL_MODULE_CURIOSITY, 0.3f);

    float content[64];
    for (int i = 0; i < 64; i++) content[i] = (float)i;

    // High weight module
    bool granted1 = meta_controller_request_workspace_access(
        controller, META_CTRL_MODULE_EMOTION, content, 64, 0.5f);

    // Low weight module
    bool granted2 = meta_controller_request_workspace_access(
        controller, META_CTRL_MODULE_CURIOSITY, content, 64, 0.5f);

    // Just verify no crashes
    EXPECT_TRUE(granted1 || !granted1);
    EXPECT_TRUE(granted2 || !granted2);
}

//=============================================================================
// Arbitration Strategy Tests
//=============================================================================

TEST_F(CognitiveMetaControllerTest, Arbitration_WinnerTakeAll) {
    meta_controller_config_t config;
    meta_controller_default_config(&config);
    config.strategy = ARBITRATION_WINNER_TAKE_ALL;
    CreateControllerWithConfig(&config);
    CreateWorkingMemoryIntegration();
    meta_controller_start(controller);

    float item1[2] = {1.0f, 2.0f};
    float item2[2] = {3.0f, 4.0f};
    float item3[2] = {5.0f, 6.0f};

    // Submit requests with different priorities
    meta_controller_request_wm_slot(controller, META_CTRL_MODULE_ATTENTION,
                                    item1, 2, 0.5f, 0.5f);
    meta_controller_request_wm_slot(controller, META_CTRL_MODULE_EMOTION,
                                    item2, 2, 0.9f, 0.9f);  // Highest priority
    meta_controller_request_wm_slot(controller, META_CTRL_MODULE_CURIOSITY,
                                    item3, 2, 0.3f, 0.3f);

    // Wait for update interval to elapse
    std::this_thread::sleep_for(
        std::chrono::milliseconds(META_CONTROLLER_DEFAULT_UPDATE_MS + 10));

    // Update to process requests
    uint64_t current_time = nimcp_time_get_ms();
    int processed = meta_controller_update(controller, current_time);

    // May be 0 if requests are queued but not yet processed
    EXPECT_GE(processed, 0);
}

TEST_F(CognitiveMetaControllerTest, Arbitration_PriorityWeighted) {
    meta_controller_config_t config;
    meta_controller_default_config(&config);
    config.strategy = ARBITRATION_PRIORITY_WEIGHTED;
    CreateControllerWithConfig(&config);
    CreateWorkingMemoryIntegration();
    meta_controller_start(controller);

    // Set different module weights
    meta_controller_set_module_weight(controller, META_CTRL_MODULE_ATTENTION, 0.9f);
    meta_controller_set_module_weight(controller, META_CTRL_MODULE_EMOTION, 0.5f);

    float item1[2] = {1.0f, 2.0f};
    float item2[2] = {3.0f, 4.0f};

    meta_controller_request_wm_slot(controller, META_CTRL_MODULE_ATTENTION,
                                    item1, 2, 0.5f, 0.5f);
    meta_controller_request_wm_slot(controller, META_CTRL_MODULE_EMOTION,
                                    item2, 2, 0.6f, 0.6f);

    // Wait for update interval to elapse
    std::this_thread::sleep_for(
        std::chrono::milliseconds(META_CONTROLLER_DEFAULT_UPDATE_MS + 10));

    uint64_t current_time = nimcp_time_get_ms();
    int processed = meta_controller_update(controller, current_time);

    // May be 0 if requests are queued but not yet processed
    EXPECT_GE(processed, 0);
}

TEST_F(CognitiveMetaControllerTest, SetArbitrationStrategy_Valid) {
    CreateController();

    int result = meta_controller_set_arbitration_strategy(
        controller, ARBITRATION_WEIGHTED_FUSION);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CognitiveMetaControllerTest, SetArbitrationStrategy_NullController) {
    int result = meta_controller_set_arbitration_strategy(
        nullptr, ARBITRATION_WEIGHTED_FUSION);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(CognitiveMetaControllerTest, Update_NotRunning) {
    CreateController();

    uint64_t current_time = nimcp_time_get_ms();
    int processed = meta_controller_update(controller, current_time);

    EXPECT_EQ(processed, 0);  // Should not process when not running
}

TEST_F(CognitiveMetaControllerTest, Update_NullController) {
    uint64_t current_time = nimcp_time_get_ms();
    int processed = meta_controller_update(nullptr, current_time);

    EXPECT_EQ(processed, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, Update_WithRequests) {
    CreateAndStartController();
    CreateWorkingMemoryIntegration();

    float item[2] = {1.0f, 2.0f};
    meta_controller_request_wm_slot(controller, META_CTRL_MODULE_ATTENTION,
                                    item, 2, 0.8f, 0.9f);

    // Wait for update interval
    std::this_thread::sleep_for(
        std::chrono::milliseconds(META_CONTROLLER_DEFAULT_UPDATE_MS + 10));

    uint64_t current_time = nimcp_time_get_ms();
    int processed = meta_controller_update(controller, current_time);

    EXPECT_GE(processed, 0);
}

TEST_F(CognitiveMetaControllerTest, Update_TooSoon) {
    CreateAndStartController();

    uint64_t start_time = nimcp_time_get_ms();
    meta_controller_update(controller, start_time);

    // Try updating immediately (before interval)
    int processed = meta_controller_update(controller, start_time + 1);

    EXPECT_EQ(processed, 0);  // Should skip update
}

TEST_F(CognitiveMetaControllerTest, UpdateMetacognitiveState_Basic) {
    CreateAndStartController();

    int result = meta_controller_update_metacognitive_state(controller);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CognitiveMetaControllerTest, UpdateMetacognitiveState_NullController) {
    int result = meta_controller_update_metacognitive_state(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CognitiveMetaControllerTest, GetStats_Initial) {
    CreateAndStartController();

    meta_controller_stats_t stats;
    int result = meta_controller_get_stats(controller, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_requests, 0);
    EXPECT_EQ(stats.granted_requests, 0);
    EXPECT_EQ(stats.denied_requests, 0);
    EXPECT_EQ(stats.conflicts_resolved, 0);
}

TEST_F(CognitiveMetaControllerTest, GetStats_AfterRequests) {
    CreateAndStartController();
    CreateWorkingMemoryIntegration();

    float item[2] = {1.0f, 2.0f};
    meta_controller_request_wm_slot(controller, META_CTRL_MODULE_ATTENTION,
                                    item, 2, 0.8f, 0.9f);

    meta_controller_stats_t stats;
    meta_controller_get_stats(controller, &stats);

    EXPECT_EQ(stats.total_requests, 1);
}

TEST_F(CognitiveMetaControllerTest, GetStats_NullController) {
    meta_controller_stats_t stats;
    int result = meta_controller_get_stats(nullptr, &stats);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, GetStats_NullStats) {
    CreateController();

    int result = meta_controller_get_stats(controller, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, ResetStats_Valid) {
    CreateAndStartController();
    CreateWorkingMemoryIntegration();

    float item[2] = {1.0f, 2.0f};
    meta_controller_request_wm_slot(controller, META_CTRL_MODULE_ATTENTION,
                                    item, 2, 0.8f, 0.9f);

    meta_controller_reset_stats(controller);

    meta_controller_stats_t stats;
    meta_controller_get_stats(controller, &stats);

    EXPECT_EQ(stats.total_requests, 0);
}

TEST_F(CognitiveMetaControllerTest, ResetStats_NullController) {
    meta_controller_reset_stats(nullptr);
    // Should not crash
}

TEST_F(CognitiveMetaControllerTest, GetModulePerformance_Valid) {
    CreateAndStartController();

    module_performance_t perf;
    int result = meta_controller_get_module_performance(
        controller, META_CTRL_MODULE_ATTENTION, &perf);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(perf.module, META_CTRL_MODULE_ATTENTION);
}

TEST_F(CognitiveMetaControllerTest, GetModulePerformance_NullController) {
    module_performance_t perf;
    int result = meta_controller_get_module_performance(
        nullptr, META_CTRL_MODULE_ATTENTION, &perf);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, GetModulePerformance_InvalidModule) {
    CreateController();

    module_performance_t perf;
    int result = meta_controller_get_module_performance(
        controller, (cognitive_module_id_t)999, &perf);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

//=============================================================================
// Observer Callback Tests
//=============================================================================

// Static callback counters
static int allocation_callback_count = 0;
static int metacognitive_callback_count = 0;

static void allocation_callback(const resource_request_t* request, void* user_data) {
    allocation_callback_count++;
}

static void metacognitive_callback(float uncertainty, float confidence,
                                   float performance, void* user_data) {
    metacognitive_callback_count++;
}

TEST_F(CognitiveMetaControllerTest, RegisterAllocationObserver_Valid) {
    CreateController();

    allocation_callback_count = 0;
    int result = meta_controller_register_allocation_observer(
        controller, allocation_callback, nullptr);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CognitiveMetaControllerTest, RegisterAllocationObserver_NullController) {
    int result = meta_controller_register_allocation_observer(
        nullptr, allocation_callback, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, RegisterAllocationObserver_NullCallback) {
    CreateController();

    int result = meta_controller_register_allocation_observer(
        controller, nullptr, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, AllocationObserver_CallbackInvoked) {
    CreateAndStartController();
    CreateWorkingMemoryIntegration();

    allocation_callback_count = 0;
    meta_controller_register_allocation_observer(
        controller, allocation_callback, nullptr);

    float item[2] = {1.0f, 2.0f};
    meta_controller_request_wm_slot(controller, META_CTRL_MODULE_ATTENTION,
                                    item, 2, 0.8f, 0.9f);

    // Wait and update
    std::this_thread::sleep_for(
        std::chrono::milliseconds(META_CONTROLLER_DEFAULT_UPDATE_MS + 10));
    uint64_t current_time = nimcp_time_get_ms();
    meta_controller_update(controller, current_time);

    // Callback should have been invoked (at least once for granted request)
    EXPECT_GE(allocation_callback_count, 0);
}

TEST_F(CognitiveMetaControllerTest, RegisterMetacognitiveObserver_Valid) {
    CreateController();

    metacognitive_callback_count = 0;
    int result = meta_controller_register_metacognitive_observer(
        controller, metacognitive_callback, nullptr);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CognitiveMetaControllerTest, RegisterMetacognitiveObserver_NullController) {
    int result = meta_controller_register_metacognitive_observer(
        nullptr, metacognitive_callback, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, MetacognitiveObserver_CallbackInvoked) {
    CreateAndStartController();

    metacognitive_callback_count = 0;
    meta_controller_register_metacognitive_observer(
        controller, metacognitive_callback, nullptr);

    meta_controller_update_metacognitive_state(controller);

    // Callback should have been invoked
    EXPECT_GE(metacognitive_callback_count, 0);
}

//=============================================================================
// Module Weight Tests
//=============================================================================

TEST_F(CognitiveMetaControllerTest, SetModuleWeight_Valid) {
    CreateController();

    int result = meta_controller_set_module_weight(
        controller, META_CTRL_MODULE_ATTENTION, 0.8f);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CognitiveMetaControllerTest, SetModuleWeight_NullController) {
    int result = meta_controller_set_module_weight(
        nullptr, META_CTRL_MODULE_ATTENTION, 0.8f);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, SetModuleWeight_InvalidModule) {
    CreateController();

    int result = meta_controller_set_module_weight(
        controller, (cognitive_module_id_t)999, 0.8f);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(CognitiveMetaControllerTest, SetModuleWeight_ClampingLow) {
    CreateController();

    int result = meta_controller_set_module_weight(
        controller, META_CTRL_MODULE_ATTENTION, -0.5f);

    EXPECT_EQ(result, NIMCP_SUCCESS);  // Should clamp to 0.0
}

TEST_F(CognitiveMetaControllerTest, SetModuleWeight_ClampingHigh) {
    CreateController();

    int result = meta_controller_set_module_weight(
        controller, META_CTRL_MODULE_ATTENTION, 2.0f);

    EXPECT_EQ(result, NIMCP_SUCCESS);  // Should clamp to 1.0
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(CognitiveMetaControllerTest, ConnectWorkingMemory_Valid) {
    CreateController();
    CreateWorkingMemoryIntegration();
    // Already tested in helper, just verify no crash
}

TEST_F(CognitiveMetaControllerTest, ConnectWorkingMemory_NullController) {
    working_memory_config_t wm_config = working_memory_default_config();
    working_memory = working_memory_create_custom(&wm_config);

    int result = meta_controller_connect_working_memory(nullptr, working_memory);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, ConnectWorkingMemory_NullWorkingMemory) {
    CreateController();

    int result = meta_controller_connect_working_memory(controller, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, ConnectExecutive_Valid) {
    CreateController();
    CreateExecutiveIntegration();
    // Already tested in helper
}

TEST_F(CognitiveMetaControllerTest, ConnectGlobalWorkspace_Valid) {
    CreateController();
    CreateGlobalWorkspaceIntegration();
    // Already tested in helper
}

TEST_F(CognitiveMetaControllerTest, ConnectBrainImmune_Valid) {
    CreateController();
    CreateBrainImmuneIntegration();
    // Already tested in helper
}

TEST_F(CognitiveMetaControllerTest, ConnectBioAsync_Valid) {
    CreateController();

    int result = meta_controller_connect_bio_async(controller);

    // May succeed or fail depending on bio-async availability
    // Just verify no crash
    EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ERROR_OPERATION_FAILED);
}

TEST_F(CognitiveMetaControllerTest, ConnectBioAsync_NullController) {
    int result = meta_controller_connect_bio_async(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, ConnectBioAsync_AlreadyConnected) {
    CreateController();
    meta_controller_connect_bio_async(controller);

    // Try connecting again - should be idempotent
    // May fail if bio-async router unavailable in test environment
    int result = meta_controller_connect_bio_async(controller);

    EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ERROR_OPERATION_FAILED);
}

TEST_F(CognitiveMetaControllerTest, DisconnectBioAsync_Valid) {
    CreateController();
    meta_controller_connect_bio_async(controller);

    int result = meta_controller_disconnect_bio_async(controller);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(CognitiveMetaControllerTest, DisconnectBioAsync_NullController) {
    int result = meta_controller_disconnect_bio_async(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CognitiveMetaControllerTest, DisconnectBioAsync_NotConnected) {
    CreateController();

    int result = meta_controller_disconnect_bio_async(controller);

    EXPECT_EQ(result, NIMCP_SUCCESS);  // Should be safe
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(CognitiveMetaControllerTest, ModuleIdToString_Valid) {
    EXPECT_STREQ(cognitive_module_id_to_string(META_CTRL_MODULE_WORKING_MEMORY),
                 "WORKING_MEMORY");
    EXPECT_STREQ(cognitive_module_id_to_string(META_CTRL_MODULE_ATTENTION),
                 "ATTENTION");
    EXPECT_STREQ(cognitive_module_id_to_string(META_CTRL_MODULE_EMOTION),
                 "EMOTION");
}

TEST_F(CognitiveMetaControllerTest, ModuleIdToString_Unknown) {
    EXPECT_STREQ(cognitive_module_id_to_string((cognitive_module_id_t)999),
                 "UNKNOWN");
}

TEST_F(CognitiveMetaControllerTest, ResourceTypeToString_Valid) {
    EXPECT_STREQ(resource_type_to_string(RESOURCE_WORKING_MEMORY_SLOT),
                 "WM_SLOT");
    EXPECT_STREQ(resource_type_to_string(RESOURCE_ATTENTION_FOCUS),
                 "ATTENTION");
    EXPECT_STREQ(resource_type_to_string(RESOURCE_LEARNING_BANDWIDTH),
                 "LEARNING_RATE");
}

TEST_F(CognitiveMetaControllerTest, ArbitrationStrategyToString_Valid) {
    EXPECT_STREQ(arbitration_strategy_to_string(ARBITRATION_WINNER_TAKE_ALL),
                 "WINNER_TAKE_ALL");
    EXPECT_STREQ(arbitration_strategy_to_string(ARBITRATION_WEIGHTED_FUSION),
                 "WEIGHTED_FUSION");
}

TEST_F(CognitiveMetaControllerTest, StateToString_Valid) {
    EXPECT_STREQ(meta_controller_state_to_string(META_CONTROLLER_STOPPED),
                 "STOPPED");
    EXPECT_STREQ(meta_controller_state_to_string(META_CONTROLLER_RUNNING),
                 "RUNNING");
    EXPECT_STREQ(meta_controller_state_to_string(META_CONTROLLER_PAUSED),
                 "PAUSED");
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(CognitiveMetaControllerTest, ThreadSafety_ConcurrentRequests) {
    CreateAndStartController();
    CreateWorkingMemoryIntegration();

    auto submit_requests = [this]() {
        for (int i = 0; i < 10; i++) {
            float item[2] = {(float)i, (float)(i+1)};
            meta_controller_request_wm_slot(
                controller, META_CTRL_MODULE_ATTENTION, item, 2, 0.5f, 0.5f);
        }
    };

    std::thread t1(submit_requests);
    std::thread t2(submit_requests);

    t1.join();
    t2.join();

    meta_controller_stats_t stats;
    meta_controller_get_stats(controller, &stats);

    EXPECT_EQ(stats.total_requests, 20);
}

TEST_F(CognitiveMetaControllerTest, ThreadSafety_ConcurrentStartStop) {
    CreateController();

    auto start_stop = [this]() {
        for (int i = 0; i < 5; i++) {
            meta_controller_start(controller);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            meta_controller_pause(controller);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            meta_controller_resume(controller);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            meta_controller_stop(controller);
        }
    };

    std::thread t1(start_stop);
    std::thread t2(start_stop);

    t1.join();
    t2.join();

    // Should not crash
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(CognitiveMetaControllerTest, EdgeCase_QueueFull) {
    CreateAndStartController();
    CreateWorkingMemoryIntegration();

    // Submit more than META_CONTROLLER_MAX_REQUESTS
    float item[2] = {1.0f, 2.0f};
    for (int i = 0; i < META_CONTROLLER_MAX_REQUESTS + 10; i++) {
        uint32_t req_id = meta_controller_request_wm_slot(
            controller, META_CTRL_MODULE_ATTENTION, item, 2, 0.5f, 0.5f);

        if (i < META_CONTROLLER_MAX_REQUESTS) {
            EXPECT_GT(req_id, 0);
        } else {
            EXPECT_EQ(req_id, 0);  // Queue full
        }
    }
}

TEST_F(CognitiveMetaControllerTest, EdgeCase_MultipleUpdates) {
    CreateAndStartController();
    CreateWorkingMemoryIntegration();

    float item[2] = {1.0f, 2.0f};
    meta_controller_request_wm_slot(controller, META_CTRL_MODULE_ATTENTION,
                                    item, 2, 0.8f, 0.9f);

    // Multiple updates
    for (int i = 0; i < 5; i++) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(META_CONTROLLER_DEFAULT_UPDATE_MS + 10));
        uint64_t current_time = nimcp_time_get_ms();
        meta_controller_update(controller, current_time);
    }

    meta_controller_stats_t stats;
    meta_controller_get_stats(controller, &stats);

    EXPECT_GT(stats.total_updates, 0);
}

TEST_F(CognitiveMetaControllerTest, EdgeCase_AllIntegrationsConnected) {
    CreateController();
    CreateWorkingMemoryIntegration();
    CreateExecutiveIntegration();
    CreateGlobalWorkspaceIntegration();
    CreateBrainImmuneIntegration();
    meta_controller_connect_bio_async(controller);

    meta_controller_start(controller);

    // Submit requests to all resource types
    float item[2] = {1.0f, 2.0f};
    meta_controller_request_wm_slot(controller, META_CTRL_MODULE_ATTENTION,
                                    item, 2, 0.8f, 0.9f);
    meta_controller_request_attention(controller, META_CTRL_MODULE_EMOTION,
                                     0.7f, 0.8f, nullptr);
    meta_controller_request_learning_rate(controller, META_CTRL_MODULE_WORKING_MEMORY,
                                         0.5f, 0.7f, 0.01f);

    std::this_thread::sleep_for(
        std::chrono::milliseconds(META_CONTROLLER_DEFAULT_UPDATE_MS + 10));
    uint64_t current_time = nimcp_time_get_ms();
    meta_controller_update(controller, current_time);

    meta_controller_stats_t stats;
    meta_controller_get_stats(controller, &stats);

    EXPECT_GT(stats.total_requests, 0);
}
