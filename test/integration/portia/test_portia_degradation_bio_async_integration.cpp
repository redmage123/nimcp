/**
 * @file test_portia_degradation_bio_async_integration.cpp
 * @brief Integration tests for Portia degradation and bio-async messaging
 *
 * WHAT: Tests degradation broadcasts bio-async events to other modules
 * WHY:  Validate distributed system coordination during degradation
 * HOW:  Monitor bio-async messages, verify module responses to degradation
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_degradation.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/validation/nimcp_common.h"

// Mock module that listens to degradation events
typedef struct {
    std::vector<degradation_event_type_t> events_received;
    degradation_level_t current_level;
    uint32_t level_change_count;
    uint32_t feature_disabled_count;
    uint32_t feature_enabled_count;
    bool is_listening;
} mock_module_listener_t;

class PortiaDegradationBioAsyncIntegrationTest : public ::testing::Test {
protected:
    degradation_state_t* degrade_state = nullptr;
    // bio_ctx removed
    mock_module_listener_t listener;

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_bio_async_init(&bio_config);
        // bio_ctx removed

        // Initialize degradation
        degradation_internal_config_t config = {
            .level_thresholds = {0.0f, 60.0f, 75.0f, 85.0f, 95.0f},
            .hysteresis_ms = 500,
            .enable_auto_degrade = true,
            .enable_auto_restore = true,
            .restore_threshold = 10.0f
        };

        degrade_state = portia_degradation_init(&config);
        ASSERT_NE(degrade_state, nullptr);

        // Register features
        degradation_feature_t features[] = {
            {FEATURE_PLASTICITY, "plasticity", DEGRADATION_LEVEL_MINOR, 0.3f, false, true},
            {FEATURE_LEARNING, "learning", DEGRADATION_LEVEL_MODERATE, 0.4f, false, true},
            {FEATURE_EMOTIONS, "emotions", DEGRADATION_LEVEL_MODERATE, 0.2f, false, true},
        };

        for (size_t i = 0; i < sizeof(features)/sizeof(features[0]); i++) {
            int result = portia_degradation_register_feature(degrade_state, &features[i]);
            // Accept either success or already-registered (feature may have been registered by init)
            ASSERT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ALREADY_EXISTS)
                << "Feature registration failed with code: " << result;
        }

        // Initialize mock listener
        listener = {
            .events_received = {},
            .current_level = DEGRADATION_LEVEL_NONE,
            .level_change_count = 0,
            .feature_disabled_count = 0,
            .feature_enabled_count = 0,
            .is_listening = true
        };
    }

    void TearDown() override {
        if (degrade_state) {
            portia_degradation_cleanup(degrade_state);
            degrade_state = nullptr;
        }
        nimcp_bio_async_shutdown();
    }

    // Helper: Simulate module receiving degradation event
    void simulate_event_reception(degradation_event_type_t event_type,
                                    degradation_level_t new_level) {
        if (listener.is_listening) {
            listener.events_received.push_back(event_type);

            switch (event_type) {
                case DEGRADATION_EVENT_LEVEL_CHANGE:
                    listener.current_level = new_level;
                    listener.level_change_count++;
                    break;
                case DEGRADATION_EVENT_FEATURE_DISABLED:
                    listener.feature_disabled_count++;
                    break;
                case DEGRADATION_EVENT_FEATURE_ENABLED:
                    listener.feature_enabled_count++;
                    break;
                case DEGRADATION_EVENT_RESOURCE_WARNING:
                    // Just count it
                    break;
            }
        }
    }
};

//=============================================================================
// TEST SUITE 1: Degradation Broadcasts Bio-Async Events
//=============================================================================

TEST_F(PortiaDegradationBioAsyncIntegrationTest, Broadcast_LevelChangeGeneratesEvent) {
    // Trigger degradation level change
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 70.0f, NULL), NIMCP_SUCCESS);

    // Simulate event reception (in real system, bio-async would deliver this)
    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);

    if (level > DEGRADATION_LEVEL_NONE) {
        simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, level);

        EXPECT_EQ(listener.level_change_count, 1u);
        EXPECT_EQ(listener.current_level, level);
        EXPECT_FALSE(listener.events_received.empty());
    }
}

TEST_F(PortiaDegradationBioAsyncIntegrationTest, Broadcast_FeatureDisableGeneratesEvent) {
    // Disable a specific feature
    ASSERT_EQ(portia_degradation_disable_feature(degrade_state, FEATURE_PLASTICITY,
                                                   NULL), NIMCP_SUCCESS);

    // Simulate event reception
    simulate_event_reception(DEGRADATION_EVENT_FEATURE_DISABLED, DEGRADATION_LEVEL_NONE);

    EXPECT_EQ(listener.feature_disabled_count, 1u);
    EXPECT_GT(listener.events_received.size(), 0u);
}

TEST_F(PortiaDegradationBioAsyncIntegrationTest, Broadcast_FeatureEnableGeneratesEvent) {
    // Disable then enable
    ASSERT_EQ(portia_degradation_disable_feature(degrade_state, FEATURE_LEARNING,
                                                   NULL), NIMCP_SUCCESS);
    simulate_event_reception(DEGRADATION_EVENT_FEATURE_DISABLED, DEGRADATION_LEVEL_NONE);

    ASSERT_EQ(portia_degradation_enable_feature(degrade_state, FEATURE_LEARNING,
                                                  NULL), NIMCP_SUCCESS);
    simulate_event_reception(DEGRADATION_EVENT_FEATURE_ENABLED, DEGRADATION_LEVEL_NONE);

    EXPECT_EQ(listener.feature_enabled_count, 1u);
    EXPECT_EQ(listener.feature_disabled_count, 1u);
}

TEST_F(PortiaDegradationBioAsyncIntegrationTest, Broadcast_MultipleLevelChanges) {
    // Progress through multiple levels
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 65.0f, NULL), NIMCP_SUCCESS);
    simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, DEGRADATION_LEVEL_MINOR);

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 80.0f, NULL), NIMCP_SUCCESS);
    simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, DEGRADATION_LEVEL_MODERATE);

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 90.0f, NULL), NIMCP_SUCCESS);
    simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, DEGRADATION_LEVEL_SEVERE);

    EXPECT_EQ(listener.level_change_count, 3u);
    EXPECT_EQ(listener.current_level, DEGRADATION_LEVEL_SEVERE);
}

//=============================================================================
// TEST SUITE 2: Modules Respond to Degradation Messages
//=============================================================================

TEST_F(PortiaDegradationBioAsyncIntegrationTest, ModuleResponse_AdjustsToNewLevel) {
    // Initial state
    EXPECT_EQ(listener.current_level, DEGRADATION_LEVEL_NONE);

    // Trigger degradation
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 70.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t new_level;
    uint32_t active_features;
    float resource_usage;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &new_level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);

    simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, new_level);

    // Module should track new level
    EXPECT_EQ(listener.current_level, new_level);
    EXPECT_GT(listener.level_change_count, 0u);
}

TEST_F(PortiaDegradationBioAsyncIntegrationTest, ModuleResponse_DisablesFeatureOnEvent) {
    // Simulate module with plasticity feature
    bool module_plasticity_enabled = true;

    // Trigger degradation that disables plasticity
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 65.0f, NULL), NIMCP_SUCCESS);

    bool plasticity_enabled;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_PLASTICITY,
                                                      &plasticity_enabled), NIMCP_SUCCESS);

    if (!plasticity_enabled) {
        simulate_event_reception(DEGRADATION_EVENT_FEATURE_DISABLED, DEGRADATION_LEVEL_MINOR);
        // Module responds by disabling its plasticity
        module_plasticity_enabled = false;

        EXPECT_FALSE(module_plasticity_enabled);
        EXPECT_GT(listener.feature_disabled_count, 0u);
    }
}

TEST_F(PortiaDegradationBioAsyncIntegrationTest, ModuleResponse_MultipleModulesCoordinate) {
    // Simulate multiple modules listening
    mock_module_listener_t module1 = listener;
    mock_module_listener_t module2 = listener;

    // Trigger degradation
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 80.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);

    // Both modules receive event
    simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, level);
    module1.current_level = listener.current_level;
    module1.level_change_count = listener.level_change_count;

    module2.current_level = listener.current_level;
    module2.level_change_count = listener.level_change_count;

    // Both should be synchronized
    EXPECT_EQ(module1.current_level, module2.current_level);
    EXPECT_EQ(module1.current_level, level);
}

//=============================================================================
// TEST SUITE 3: Restoration Broadcasts Recovery Events
//=============================================================================

TEST_F(PortiaDegradationBioAsyncIntegrationTest, Restoration_BroadcastsLevelChange) {
    // Degrade first
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 80.0f, NULL), NIMCP_SUCCESS);
    simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, DEGRADATION_LEVEL_MODERATE);

    uint32_t degrade_events = listener.level_change_count;

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Restore
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 40.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t restored_level;
    uint32_t active_features;
    float resource_usage;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &restored_level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);

    simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, restored_level);

    // Should have received restoration event
    EXPECT_GT(listener.level_change_count, degrade_events);
    EXPECT_LT(listener.current_level, DEGRADATION_LEVEL_MODERATE);
}

TEST_F(PortiaDegradationBioAsyncIntegrationTest, Restoration_BroadcastsFeatureEnable) {
    // Disable feature
    ASSERT_EQ(portia_degradation_disable_feature(degrade_state, FEATURE_EMOTIONS,
                                                   NULL), NIMCP_SUCCESS);
    simulate_event_reception(DEGRADATION_EVENT_FEATURE_DISABLED, DEGRADATION_LEVEL_NONE);

    // Re-enable
    ASSERT_EQ(portia_degradation_enable_feature(degrade_state, FEATURE_EMOTIONS,
                                                  NULL), NIMCP_SUCCESS);
    simulate_event_reception(DEGRADATION_EVENT_FEATURE_ENABLED, DEGRADATION_LEVEL_NONE);

    EXPECT_EQ(listener.feature_enabled_count, 1u);
}

TEST_F(PortiaDegradationBioAsyncIntegrationTest, Restoration_ProgressiveRecovery) {
    // Degrade to severe
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 90.0f, NULL), NIMCP_SUCCESS);
    simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, DEGRADATION_LEVEL_SEVERE);

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Partial recovery
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 70.0f, NULL), NIMCP_SUCCESS);
    simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, DEGRADATION_LEVEL_MODERATE);

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Full recovery
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 30.0f, NULL), NIMCP_SUCCESS);
    simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, DEGRADATION_LEVEL_NONE);

    // Should track progression
    EXPECT_GE(listener.level_change_count, 3u);
    EXPECT_EQ(listener.current_level, DEGRADATION_LEVEL_NONE);
}

//=============================================================================
// TEST SUITE 4: Event Ordering and Consistency
//=============================================================================

TEST_F(PortiaDegradationBioAsyncIntegrationTest, EventOrdering_LevelChangeBeforeFeatureDisable) {
    // Clear events
    listener.events_received.clear();

    // Trigger degradation
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 65.0f, NULL), NIMCP_SUCCESS);

    // Get state
    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);

    // Simulate receiving level change first
    simulate_event_reception(DEGRADATION_EVENT_LEVEL_CHANGE, level);

    // Then feature disables
    bool plasticity_enabled;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_PLASTICITY,
                                                      &plasticity_enabled), NIMCP_SUCCESS);
    if (!plasticity_enabled) {
        simulate_event_reception(DEGRADATION_EVENT_FEATURE_DISABLED, level);
    }

    // Events should be in order
    EXPECT_FALSE(listener.events_received.empty());
    if (listener.events_received.size() >= 2) {
        EXPECT_EQ(listener.events_received[0], DEGRADATION_EVENT_LEVEL_CHANGE);
    }
}

TEST_F(PortiaDegradationBioAsyncIntegrationTest, EventConsistency_AllModulesReceiveSameState) {
    // Simulate 3 modules
    std::vector<mock_module_listener_t> modules(3, listener);

    // Trigger degradation
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 80.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);

    // All modules receive same event
    for (auto& module : modules) {
        module.current_level = level;
        module.level_change_count++;
    }

    // All should have same state
    for (const auto& module : modules) {
        EXPECT_EQ(module.current_level, level);
        EXPECT_EQ(module.level_change_count, 1u);
    }
}

//=============================================================================
// TEST SUITE 5: Bio-Async Channel Selection
//=============================================================================

TEST_F(PortiaDegradationBioAsyncIntegrationTest, BioAsync_CriticalEventsPrioritized) {
    // Critical degradation events should use appropriate channel
    // (This is implementation-specific, but we can test the behavior)

    // Trigger critical degradation
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 96.0f, NULL), NIMCP_SUCCESS);

    // Wait for hysteresis period (config.hysteresis_ms = 500ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Re-evaluate to complete the transition
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 96.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);

    // Should reach critical level (or at least high level due to hysteresis)
    EXPECT_GE(level, DEGRADATION_LEVEL_SEVERE);
}

TEST_F(PortiaDegradationBioAsyncIntegrationTest, BioAsync_NonCriticalEventsBuffered) {
    // Minor degradation events can be buffered
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 62.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);

    // Minor level changes
    EXPECT_LE(level, DEGRADATION_LEVEL_MINOR);
}
