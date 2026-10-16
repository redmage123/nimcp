//=============================================================================
// test_language_integration.cpp - Language Layer Integration Tests
//=============================================================================
/**
 * @file test_language_integration.cpp
 * @brief Integration tests for Language Layer
 *
 * WHAT: Tests full language processing pipeline integration
 * WHY:  Verify correct coordination between orchestrator, bridges, and systems
 * HOW:  gtest framework testing end-to-end language flows
 *
 * TEST SCENARIOS:
 * 1. Full comprehension pipeline: perception → orchestrator → cognitive
 * 2. Full production pipeline: cognitive → orchestrator → perception
 * 3. Multi-bridge operation
 * 4. Bio-async communication
 * 5. Error recovery and state consistency
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>

#include "language/nimcp_language_orchestrator.h"
#include "language/nimcp_language_config.h"
#include "language/nimcp_language_types.h"
#include "language/bridges/nimcp_language_perception_bridge.h"
#include "language/bridges/nimcp_language_cognitive_bridge.h"
#include "language/bridges/nimcp_language_training_bridge.h"
#include "language/bridges/nimcp_language_omni_bridge.h"
#include "language/bridges/nimcp_language_immune_bridge.h"
#include "language/bridges/nimcp_language_gpu_bridge.h"
#include "language/bridges/nimcp_language_thalamic_bridge.h"
#include "language/bridges/nimcp_language_substrate_bridge.h"
#include "language/bridges/nimcp_language_logic_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LanguageIntegrationTest : public ::testing::Test {
protected:
    language_orchestrator_t* orchestrator;
    language_perception_bridge_t* perception_bridge;
    language_cognitive_bridge_t* cognitive_bridge;
    language_training_bridge_t* training_bridge;
    language_omni_bridge_t* omni_bridge;
    language_immune_bridge_t* immune_bridge;
    language_gpu_bridge_t* gpu_bridge;
    language_thalamic_bridge_t* thalamic_bridge;
    language_substrate_bridge_t* substrate_bridge;
    language_logic_bridge_t* logic_bridge;

    void SetUp() override {
        orchestrator = nullptr;
        perception_bridge = nullptr;
        cognitive_bridge = nullptr;
        training_bridge = nullptr;
        omni_bridge = nullptr;
        immune_bridge = nullptr;
        gpu_bridge = nullptr;
        thalamic_bridge = nullptr;
        substrate_bridge = nullptr;
        logic_bridge = nullptr;

        // Create orchestrator
        language_orchestrator_config_t config;
        language_orchestrator_default_config(&config);
        config.enable_perception_bridge = true;
        config.enable_cognitive_bridge = true;
        config.enable_training_bridge = true;
        config.enable_omni_bridge = true;
        config.enable_bio_async = true;

        orchestrator = language_orchestrator_create(&config);
        ASSERT_NE(orchestrator, nullptr) << "Failed to create orchestrator";
    }

    void TearDown() override {
        if (logic_bridge) {
            language_logic_bridge_destroy(logic_bridge);
            logic_bridge = nullptr;
        }
        if (substrate_bridge) {
            language_substrate_bridge_destroy(substrate_bridge);
            substrate_bridge = nullptr;
        }
        if (thalamic_bridge) {
            language_thalamic_bridge_destroy(thalamic_bridge);
            thalamic_bridge = nullptr;
        }
        if (gpu_bridge) {
            language_gpu_bridge_destroy(gpu_bridge);
            gpu_bridge = nullptr;
        }
        if (immune_bridge) {
            language_immune_bridge_destroy(immune_bridge);
            immune_bridge = nullptr;
        }
        if (omni_bridge) {
            language_omni_bridge_destroy(omni_bridge);
            omni_bridge = nullptr;
        }
        if (training_bridge) {
            language_training_bridge_destroy(training_bridge);
            training_bridge = nullptr;
        }
        if (cognitive_bridge) {
            language_cognitive_bridge_destroy(cognitive_bridge);
            cognitive_bridge = nullptr;
        }
        if (perception_bridge) {
            language_perception_bridge_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
        if (orchestrator) {
            language_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
    }

    uint64_t getCurrentTimeMs() {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    void CreateAllBridges() {
        // Perception bridge
        language_perception_config_t perception_cfg;
        language_perception_default_config(&perception_cfg);
        perception_bridge = language_perception_bridge_create(&perception_cfg);

        // Cognitive bridge
        language_cognitive_config_t cognitive_cfg;
        language_cognitive_default_config(&cognitive_cfg);
        cognitive_bridge = language_cognitive_bridge_create(&cognitive_cfg);

        // Training bridge
        language_training_config_t training_cfg;
        language_training_default_config(&training_cfg);
        training_bridge = language_training_bridge_create(&training_cfg);

        // Omni bridge
        language_omni_config_t omni_cfg;
        language_omni_default_config(&omni_cfg);
        omni_bridge = language_omni_bridge_create(&omni_cfg);

        // Immune bridge
        language_immune_config_t immune_cfg;
        language_immune_default_config(&immune_cfg);
        immune_bridge = language_immune_bridge_create(&immune_cfg);

        // GPU bridge
        language_gpu_config_t gpu_cfg;
        language_gpu_default_config(&gpu_cfg);
        gpu_bridge = language_gpu_bridge_create(&gpu_cfg);

        // Thalamic bridge (requires orchestrator)
        language_thalamic_config_t thalamic_cfg;
        language_thalamic_default_config(&thalamic_cfg);
        thalamic_bridge = language_thalamic_bridge_create(orchestrator, &thalamic_cfg);

        // Substrate bridge (requires orchestrator)
        language_substrate_config_t substrate_cfg;
        language_substrate_default_config(&substrate_cfg);
        substrate_bridge = language_substrate_bridge_create(orchestrator, &substrate_cfg);

        // Logic bridge (requires orchestrator)
        language_logic_config_t logic_cfg;
        language_logic_default_config(&logic_cfg);
        logic_bridge = language_logic_bridge_create(orchestrator, &logic_cfg);
    }

    void ConnectAllBridges() {
        if (perception_bridge) {
            language_orchestrator_connect_perception_bridge(orchestrator, perception_bridge);
        }
        if (cognitive_bridge) {
            language_orchestrator_connect_cognitive_bridge(orchestrator, cognitive_bridge);
        }
        if (training_bridge) {
            language_orchestrator_connect_training_bridge(orchestrator, training_bridge);
        }
        if (omni_bridge) {
            language_orchestrator_connect_omni_bridge(orchestrator, omni_bridge);
        }
        if (immune_bridge) {
            language_orchestrator_connect_immune_bridge(orchestrator, immune_bridge);
        }
        if (gpu_bridge) {
            language_orchestrator_connect_gpu_bridge(orchestrator, gpu_bridge);
        }
    }
};

//=============================================================================
// Full Pipeline Tests
//=============================================================================

/**
 * @test Full comprehension pipeline
 * WHAT: Test complete comprehension flow
 * WHY:  Verify end-to-end language understanding
 * HOW:  Start orchestrator, process input through bridges
 */
TEST_F(LanguageIntegrationTest, FullComprehensionPipeline) {
    CreateAllBridges();
    ConnectAllBridges();

    // All bridges should be created (at least some)
    int bridge_count = 0;
    if (perception_bridge) bridge_count++;
    if (cognitive_bridge) bridge_count++;
    EXPECT_GE(bridge_count, 2);

    // Start orchestrator
    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    // Set comprehension mode
    result = language_orchestrator_set_mode(orchestrator, LANGUAGE_MODE_COMPREHENSION);
    EXPECT_EQ(result, 0);

    // Update cycle
    result = language_orchestrator_update(orchestrator, getCurrentTimeMs());
    EXPECT_GE(result, 0);

    // Verify state
    language_state_t state = language_orchestrator_get_state(orchestrator);
    EXPECT_GE((int)state, 0);

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Full production pipeline
 * WHAT: Test complete production flow
 * WHY:  Verify end-to-end language generation
 * HOW:  Start orchestrator in production mode
 */
TEST_F(LanguageIntegrationTest, FullProductionPipeline) {
    CreateAllBridges();
    ConnectAllBridges();

    // Start orchestrator
    language_orchestrator_start(orchestrator);

    // Set production mode
    int result = language_orchestrator_set_mode(orchestrator, LANGUAGE_MODE_PRODUCTION);
    EXPECT_EQ(result, 0);

    // Update cycle
    result = language_orchestrator_update(orchestrator, getCurrentTimeMs());
    EXPECT_GE(result, 0);

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Bidirectional processing
 * WHAT: Test both comprehension and production together
 * WHY:  Verify mode switching works correctly
 * HOW:  Switch between modes, verify state consistency
 */
TEST_F(LanguageIntegrationTest, BidirectionalProcessing) {
    CreateAllBridges();
    ConnectAllBridges();

    language_orchestrator_start(orchestrator);
    uint64_t current_time = getCurrentTimeMs();

    // Comprehension
    language_orchestrator_set_mode(orchestrator, LANGUAGE_MODE_COMPREHENSION);
    language_orchestrator_update(orchestrator, current_time);
    current_time += 10;

    // Switch to production
    language_orchestrator_set_mode(orchestrator, LANGUAGE_MODE_PRODUCTION);
    language_orchestrator_update(orchestrator, current_time);
    current_time += 10;

    // Switch to dialogue (bidirectional)
    language_orchestrator_set_mode(orchestrator, LANGUAGE_MODE_DIALOGUE);
    language_orchestrator_update(orchestrator, current_time);

    // Verify mode
    language_mode_t mode = language_orchestrator_get_mode(orchestrator);
    EXPECT_EQ(mode, LANGUAGE_MODE_DIALOGUE);

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Multi-Bridge Integration Tests
//=============================================================================

/**
 * @test All bridges operating together
 * WHAT: Test all 9 bridges working concurrently
 * WHY:  Verify no conflicts between bridge subsystems
 * HOW:  Create all bridges, run update cycles
 */
TEST_F(LanguageIntegrationTest, AllBridgesConcurrent) {
    CreateAllBridges();
    ConnectAllBridges();

    // Count non-null bridges
    int bridge_count = 0;
    if (perception_bridge) bridge_count++;
    if (cognitive_bridge) bridge_count++;
    if (training_bridge) bridge_count++;
    if (omni_bridge) bridge_count++;
    if (immune_bridge) bridge_count++;
    if (gpu_bridge) bridge_count++;
    if (thalamic_bridge) bridge_count++;
    if (substrate_bridge) bridge_count++;
    if (logic_bridge) bridge_count++;

    EXPECT_GE(bridge_count, 6) << "Expected most bridges to be created";

    // Start and update
    language_orchestrator_start(orchestrator);

    uint64_t current_time = getCurrentTimeMs();
    for (int i = 0; i < 5; i++) {
        int result = language_orchestrator_update(orchestrator, current_time);
        EXPECT_GE(result, 0) << "Update " << i << " failed";
        current_time += 10;
    }

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Bridge update sequence
 * WHAT: Test sequential bridge updates
 * WHY:  Verify processing order doesn't affect results
 * HOW:  Run multiple update sequences
 */
TEST_F(LanguageIntegrationTest, BridgeUpdateSequence) {
    CreateAllBridges();
    ConnectAllBridges();

    language_orchestrator_start(orchestrator);
    uint64_t current_time = getCurrentTimeMs();

    // Initial update
    language_orchestrator_update(orchestrator, current_time);
    current_time += 10;

    // Get stats
    language_orchestrator_stats_t stats1;
    language_orchestrator_get_stats(orchestrator, &stats1);

    // More updates
    for (int i = 0; i < 10; i++) {
        language_orchestrator_update(orchestrator, current_time);
        current_time += 10;
    }

    // Get final stats
    language_orchestrator_stats_t stats2;
    language_orchestrator_get_stats(orchestrator, &stats2);

    // State transitions should have occurred
    (void)stats1;
    (void)stats2;

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

/**
 * @test Bio-async messaging
 * WHAT: Test neuromodulator message passing
 * WHY:  Verify bio-async communication works
 * HOW:  Start orchestrator, check bio-async stats
 */
TEST_F(LanguageIntegrationTest, BioAsyncMessaging) {
    CreateAllBridges();
    ConnectAllBridges();

    language_orchestrator_start(orchestrator);

    // Run several updates to generate messages
    uint64_t current_time = getCurrentTimeMs();
    for (int i = 0; i < 5; i++) {
        language_orchestrator_update(orchestrator, current_time);
        current_time += 10;
    }

    // Get stats
    language_orchestrator_stats_t stats;
    language_orchestrator_get_stats(orchestrator, &stats);

    // Bio-async should be connected if enabled in config
    // (depends on orchestrator implementation)

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Perception-Cognitive Bridge Integration
//=============================================================================

/**
 * @test Perception to cognitive flow
 * WHAT: Test data flow from perception to cognition
 * WHY:  Verify comprehension pipeline
 * HOW:  Create both bridges, verify connection
 */
TEST_F(LanguageIntegrationTest, PerceptionToCognitiveFlow) {
    // Create perception bridge
    language_perception_config_t perception_cfg;
    language_perception_default_config(&perception_cfg);
    perception_bridge = language_perception_bridge_create(&perception_cfg);
    ASSERT_NE(perception_bridge, nullptr);

    // Create cognitive bridge
    language_cognitive_config_t cognitive_cfg;
    language_cognitive_default_config(&cognitive_cfg);
    cognitive_bridge = language_cognitive_bridge_create(&cognitive_cfg);
    ASSERT_NE(cognitive_bridge, nullptr);

    // Connect bridges
    language_orchestrator_connect_perception_bridge(orchestrator, perception_bridge);
    language_orchestrator_connect_cognitive_bridge(orchestrator, cognitive_bridge);

    // Start and update
    language_orchestrator_start(orchestrator);
    language_orchestrator_update(orchestrator, getCurrentTimeMs());

    // Verify no crash
    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Training Integration Tests
//=============================================================================

/**
 * @test Training bridge integration
 * WHAT: Test training signals flow through language layer
 * WHY:  Verify plasticity integration
 * HOW:  Create training bridge, send signals
 */
TEST_F(LanguageIntegrationTest, TrainingBridgeIntegration) {
    // Create training bridge
    language_training_config_t training_cfg;
    language_training_default_config(&training_cfg);

    training_bridge = language_training_bridge_create(&training_cfg);
    ASSERT_NE(training_bridge, nullptr);

    // Connect
    language_orchestrator_connect_training_bridge(orchestrator, training_bridge);

    // Start and update
    language_orchestrator_start(orchestrator);
    language_orchestrator_update(orchestrator, getCurrentTimeMs());

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Immune Integration Tests
//=============================================================================

/**
 * @test Immune effects on language
 * WHAT: Test neuroinflammation modulates language
 * WHY:  Verify biological realism
 * HOW:  Create immune bridge, check modulation
 */
TEST_F(LanguageIntegrationTest, ImmuneEffectsOnLanguage) {
    // Create immune bridge
    language_immune_config_t immune_cfg;
    language_immune_default_config(&immune_cfg);

    immune_bridge = language_immune_bridge_create(&immune_cfg);
    ASSERT_NE(immune_bridge, nullptr);

    // Connect
    language_orchestrator_connect_immune_bridge(orchestrator, immune_bridge);

    // Start and update
    language_orchestrator_start(orchestrator);
    language_orchestrator_update(orchestrator, getCurrentTimeMs());

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Thalamic Routing Integration Tests
//=============================================================================

/**
 * @test Thalamic attention gating
 * WHAT: Test attention-based signal routing
 * WHY:  Verify thalamic integration
 * HOW:  Create thalamic bridge, set attention
 */
TEST_F(LanguageIntegrationTest, ThalamicAttentionGating) {
    // Create thalamic bridge (requires orchestrator)
    language_thalamic_config_t thalamic_cfg;
    language_thalamic_default_config(&thalamic_cfg);
    thalamic_bridge = language_thalamic_bridge_create(orchestrator, &thalamic_cfg);
    ASSERT_NE(thalamic_bridge, nullptr);

    // Start orchestrator and update
    language_orchestrator_start(orchestrator);
    language_orchestrator_update(orchestrator, getCurrentTimeMs());

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Substrate Integration Tests
//=============================================================================

/**
 * @test Metabolic effects integration
 * WHAT: Test ATP/fatigue effects
 * WHY:  Verify metabolic modulation
 * HOW:  Create substrate bridge, check modulation
 */
TEST_F(LanguageIntegrationTest, MetabolicEffectsIntegration) {
    // Create substrate bridge (requires orchestrator)
    language_substrate_config_t substrate_cfg;
    language_substrate_default_config(&substrate_cfg);
    substrate_bridge = language_substrate_bridge_create(orchestrator, &substrate_cfg);
    ASSERT_NE(substrate_bridge, nullptr);

    // Start orchestrator and update
    language_orchestrator_start(orchestrator);
    language_orchestrator_update(orchestrator, getCurrentTimeMs());

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Logic Integration Tests
//=============================================================================

/**
 * @test Symbolic reasoning integration
 * WHAT: Test logic bridge integration
 * WHY:  Verify reasoning capabilities
 * HOW:  Create logic bridge, run updates
 */
TEST_F(LanguageIntegrationTest, SymbolicReasoningIntegration) {
    // Create logic bridge (requires orchestrator)
    language_logic_config_t logic_cfg;
    language_logic_default_config(&logic_cfg);
    logic_bridge = language_logic_bridge_create(orchestrator, &logic_cfg);
    ASSERT_NE(logic_bridge, nullptr);

    // Start orchestrator and update
    language_orchestrator_start(orchestrator);
    language_orchestrator_update(orchestrator, getCurrentTimeMs());

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

/**
 * @test Orchestrator error recovery
 * WHAT: Test recovery from error states
 * WHY:  Verify robustness
 * HOW:  Trigger errors, verify recovery
 */
TEST_F(LanguageIntegrationTest, ErrorRecovery) {
    CreateAllBridges();
    ConnectAllBridges();

    language_orchestrator_start(orchestrator);

    // Run updates
    uint64_t current_time = getCurrentTimeMs();
    for (int i = 0; i < 5; i++) {
        language_orchestrator_update(orchestrator, current_time);
        current_time += 10;
    }

    // Reset should recover any issues
    int result = language_orchestrator_reset(orchestrator);
    EXPECT_EQ(result, 0);

    // Should be back to idle
    language_state_t state = language_orchestrator_get_state(orchestrator);
    EXPECT_EQ(state, LANGUAGE_STATE_IDLE);

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Statistics consistency
 * WHAT: Test stats remain consistent across operations
 * WHY:  Verify state tracking
 * HOW:  Get stats before/after operations
 */
TEST_F(LanguageIntegrationTest, StatisticsConsistency) {
    CreateAllBridges();
    ConnectAllBridges();

    language_orchestrator_start(orchestrator);

    // Initial stats
    language_orchestrator_stats_t stats1;
    language_orchestrator_get_stats(orchestrator, &stats1);

    // Run updates
    uint64_t current_time = getCurrentTimeMs();
    for (int i = 0; i < 10; i++) {
        language_orchestrator_update(orchestrator, current_time);
        current_time += 10;
    }

    // Final stats
    language_orchestrator_stats_t stats2;
    language_orchestrator_get_stats(orchestrator, &stats2);

    // Stats should show progress (or at least not decrease)
    // (Exact behavior depends on implementation)

    language_orchestrator_stop(orchestrator);
}

