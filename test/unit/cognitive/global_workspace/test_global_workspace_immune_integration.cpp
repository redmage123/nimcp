/**
 * @file test_global_workspace_immune_integration.cpp
 * @brief Unit tests for Global Workspace - Brain Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * TEST COVERAGE:
 * - Connection/disconnection lifecycle
 * - Inflammation modulation of GW threshold/capacity
 * - Anomaly detection (rapid switching, module hijack, corruption)
 * - Immune response triggering from anomalies
 * - Statistics and query functions
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/global_workspace/nimcp_global_workspace_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

#include <cmath>
#include <cstring>

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GlobalWorkspaceImmuneTest : public ::testing::Test {
protected:
    global_workspace_t* workspace;
    brain_immune_system_t* immune;
    gw_immune_context_t* context;

    void SetUp() override {
        // Create global workspace
        global_workspace_config_t gw_config = global_workspace_default_config();
        gw_config.capacity_dim = 256;
        gw_config.ignition_threshold = 0.6f;
        workspace = global_workspace_create_custom(&gw_config);
        ASSERT_NE(workspace, nullptr);

        // Create brain immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Start immune system
        brain_immune_start(immune);

        context = nullptr;
    }

    void TearDown() override {
        if (context != nullptr) {
            global_workspace_disconnect_immune(context);
            context = nullptr;
        }
        if (immune != nullptr) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (workspace != nullptr) {
            global_workspace_destroy(workspace);
            workspace = nullptr;
        }
    }

    // Helper: Create test broadcast content
    void create_test_content(float* content, uint32_t dim, float value) {
        for (uint32_t i = 0; i < dim; i++) {
            content[i] = value;
        }
    }

    // Helper: Create corrupted content
    void create_corrupted_content(float* content, uint32_t dim) {
        for (uint32_t i = 0; i < dim; i++) {
            if (i % 10 == 0) {
                content[i] = NAN;
            } else if (i % 10 == 1) {
                content[i] = INFINITY;
            } else {
                content[i] = 0.5f;
            }
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceImmuneTest, ConnectionLifecycle) {
    // Connect
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);
    EXPECT_EQ(context->workspace, workspace);
    EXPECT_EQ(context->immune, immune);
    EXPECT_TRUE(context->connected);

    // Disconnect
    global_workspace_disconnect_immune(context);
    context = nullptr;  // Prevent double-free in TearDown
}

TEST_F(GlobalWorkspaceImmuneTest, NullParameterHandling) {
    // NULL workspace
    context = global_workspace_connect_immune(nullptr, immune);
    EXPECT_EQ(context, nullptr);

    // NULL immune
    context = global_workspace_connect_immune(workspace, nullptr);
    EXPECT_EQ(context, nullptr);

    // NULL context disconnect (should not crash)
    global_workspace_disconnect_immune(nullptr);
}

TEST_F(GlobalWorkspaceImmuneTest, DefaultAnomalyConfig) {
    gw_immune_anomaly_config_t config = gw_immune_default_anomaly_config();

    EXPECT_GT(config.rapid_switch_threshold_ms, 0.0f);
    EXPECT_GT(config.strength_spike_threshold, 0.0f);
    EXPECT_GT(config.module_hijack_threshold, 0.0f);
    EXPECT_GT(config.module_hijack_window, 0u);
    EXPECT_GT(config.repetitive_count_threshold, 0u);
    EXPECT_TRUE(config.enable_anomaly_detection);
    EXPECT_TRUE(config.auto_trigger_immune);
}

/* ============================================================================
 * Inflammation Modulation Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceImmuneTest, InflammationModulation_None) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    // Set manual inflammation to NONE
    int result = gw_immune_set_manual_inflammation(context, INFLAMMATION_NONE);
    EXPECT_EQ(result, 0);

    // Check modulation
    gw_inflammation_modulation_t mod;
    result = gw_immune_get_modulation(context, &mod);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mod.level, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(mod.threshold_multiplier, 1.0f);
    EXPECT_FLOAT_EQ(mod.capacity_multiplier, 1.0f);

    // Threshold should be unchanged
    float threshold = global_workspace_get_ignition_threshold(workspace);
    EXPECT_FLOAT_EQ(threshold, 0.6f);
}

TEST_F(GlobalWorkspaceImmuneTest, InflammationModulation_Local) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    float baseline = global_workspace_get_ignition_threshold(workspace);

    // Set manual inflammation to LOCAL
    int result = gw_immune_set_manual_inflammation(context, INFLAMMATION_LOCAL);
    EXPECT_EQ(result, 0);

    // Check modulation
    gw_inflammation_modulation_t mod;
    result = gw_immune_get_modulation(context, &mod);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mod.level, INFLAMMATION_LOCAL);
    EXPECT_FLOAT_EQ(mod.threshold_multiplier, 1.1f);
    EXPECT_FLOAT_EQ(mod.capacity_multiplier, 0.9f);

    // Threshold should be increased
    float threshold = global_workspace_get_ignition_threshold(workspace);
    EXPECT_FLOAT_EQ(threshold, baseline * 1.1f);
}

TEST_F(GlobalWorkspaceImmuneTest, InflammationModulation_Regional) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    float baseline = global_workspace_get_ignition_threshold(workspace);

    // Set manual inflammation to REGIONAL
    int result = gw_immune_set_manual_inflammation(context, INFLAMMATION_REGIONAL);
    EXPECT_EQ(result, 0);

    // Check modulation
    gw_inflammation_modulation_t mod;
    result = gw_immune_get_modulation(context, &mod);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mod.level, INFLAMMATION_REGIONAL);
    EXPECT_FLOAT_EQ(mod.threshold_multiplier, 1.3f);
    EXPECT_FLOAT_EQ(mod.capacity_multiplier, 0.7f);

    // Threshold should be significantly increased
    float threshold = global_workspace_get_ignition_threshold(workspace);
    EXPECT_FLOAT_EQ(threshold, baseline * 1.3f);
}

TEST_F(GlobalWorkspaceImmuneTest, InflammationModulation_Systemic) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    float baseline = global_workspace_get_ignition_threshold(workspace);

    // Set manual inflammation to SYSTEMIC
    int result = gw_immune_set_manual_inflammation(context, INFLAMMATION_SYSTEMIC);
    EXPECT_EQ(result, 0);

    // Check modulation
    gw_inflammation_modulation_t mod;
    result = gw_immune_get_modulation(context, &mod);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mod.level, INFLAMMATION_SYSTEMIC);
    EXPECT_FLOAT_EQ(mod.threshold_multiplier, 1.5f);
    EXPECT_FLOAT_EQ(mod.capacity_multiplier, 0.5f);

    // Threshold should be greatly increased
    float threshold = global_workspace_get_ignition_threshold(workspace);
    EXPECT_FLOAT_EQ(threshold, baseline * 1.5f);
}

TEST_F(GlobalWorkspaceImmuneTest, InflammationModulation_Storm) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    float baseline = global_workspace_get_ignition_threshold(workspace);

    // Set manual inflammation to STORM
    int result = gw_immune_set_manual_inflammation(context, INFLAMMATION_STORM);
    EXPECT_EQ(result, 0);

    // Check modulation
    gw_inflammation_modulation_t mod;
    result = gw_immune_get_modulation(context, &mod);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mod.level, INFLAMMATION_STORM);
    EXPECT_FLOAT_EQ(mod.threshold_multiplier, 1.8f);
    EXPECT_FLOAT_EQ(mod.capacity_multiplier, 0.2f);

    // Threshold should be extremely increased (near-complete disruption)
    float threshold = global_workspace_get_ignition_threshold(workspace);
    EXPECT_FLOAT_EQ(threshold, baseline * 1.8f);
}

TEST_F(GlobalWorkspaceImmuneTest, InflammationModulation_DisconnectRestoresBaseline) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    float baseline = global_workspace_get_ignition_threshold(workspace);

    // Set inflammation
    gw_immune_set_manual_inflammation(context, INFLAMMATION_SYSTEMIC);

    // Threshold should be modified
    float modified = global_workspace_get_ignition_threshold(workspace);
    EXPECT_GT(modified, baseline);

    // Disconnect
    global_workspace_disconnect_immune(context);
    context = nullptr;

    // Threshold should be restored
    float restored = global_workspace_get_ignition_threshold(workspace);
    EXPECT_FLOAT_EQ(restored, baseline);
}

/* ============================================================================
 * Anomaly Detection Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceImmuneTest, AnomalyDetection_NoAnomalies) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    // Create normal broadcast
    float content[256];
    create_test_content(content, 256, 0.5f);

    // Compete
    global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content, 256, 0.8f);

    // Check for anomalies
    uint32_t anomaly_count = gw_immune_check_broadcast(context);
    EXPECT_EQ(anomaly_count, 0u);
}

TEST_F(GlobalWorkspaceImmuneTest, AnomalyDetection_ModuleHijacking) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    // Disable auto-trigger for testing
    context->anomaly_config.auto_trigger_immune = false;

    float content[256];
    create_test_content(content, 256, 0.5f);

    // Same module broadcasts repeatedly (3+ times)
    // Must call gw_immune_check_broadcast after each to track module streak
    for (int i = 0; i < 4; i++) {
        global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content, 256, 0.8f);
        gw_immune_check_broadcast(context);  // Track this broadcast
        usleep(60000);  // Wait past refractory period
    }

    // Check for anomalies (module hijacking should now be detected)
    gw_broadcast_anomaly_t anomalies[5];
    uint32_t count;
    int result = gw_immune_detect_anomalies(context, anomalies, 5, &count);
    EXPECT_EQ(result, 0);

    // Should detect module hijacking
    bool found_hijack = false;
    for (uint32_t i = 0; i < count; i++) {
        if (anomalies[i].type == GW_ANOMALY_MODULE_HIJACK) {
            found_hijack = true;
            EXPECT_EQ(anomalies[i].severity, GW_ANOMALY_SEVERITY_SEVERE);
            EXPECT_EQ(anomalies[i].source_module, MODULE_WORKING_MEMORY);
            break;
        }
    }
    EXPECT_TRUE(found_hijack);
}

TEST_F(GlobalWorkspaceImmuneTest, AnomalyDetection_CorruptedContent) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    // Disable auto-trigger
    context->anomaly_config.auto_trigger_immune = false;

    // Create corrupted content (NaN/Inf)
    float content[256];
    create_corrupted_content(content, 256);

    // Compete
    global_workspace_compete(workspace, MODULE_PERCEPTION, content, 256, 0.8f);

    // Detect anomalies
    gw_broadcast_anomaly_t anomalies[5];
    uint32_t count;
    int result = gw_immune_detect_anomalies(context, anomalies, 5, &count);
    EXPECT_EQ(result, 0);

    // Should detect corruption
    bool found_corruption = false;
    for (uint32_t i = 0; i < count; i++) {
        if (anomalies[i].type == GW_ANOMALY_CORRUPTED_CONTENT) {
            found_corruption = true;
            EXPECT_EQ(anomalies[i].severity, GW_ANOMALY_SEVERITY_CRITICAL);
            break;
        }
    }
    EXPECT_TRUE(found_corruption);
}

TEST_F(GlobalWorkspaceImmuneTest, AnomalyDetection_RapidSwitching) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    // Disable auto-trigger
    context->anomaly_config.auto_trigger_immune = false;

    float content1[256], content2[256];
    create_test_content(content1, 256, 0.5f);
    create_test_content(content2, 256, 0.7f);

    // First broadcast
    global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content1, 256, 0.8f);
    gw_immune_check_broadcast(context);  // Track first broadcast

    // Second broadcast immediately (violates refractory period)
    usleep(10000);  // 10ms < refractory period (50ms)
    global_workspace_compete(workspace, MODULE_EXECUTIVE, content2, 256, 0.9f);
    gw_immune_check_broadcast(context);  // Track second broadcast - should detect rapid switching

    // Detect anomalies
    gw_broadcast_anomaly_t anomalies[5];
    uint32_t count;
    int result = gw_immune_detect_anomalies(context, anomalies, 5, &count);
    EXPECT_EQ(result, 0);

    // Should detect rapid switching
    bool found_rapid = false;
    for (uint32_t i = 0; i < count; i++) {
        if (anomalies[i].type == GW_ANOMALY_RAPID_SWITCHING) {
            found_rapid = true;
            EXPECT_EQ(anomalies[i].severity, GW_ANOMALY_SEVERITY_MODERATE);
            break;
        }
    }
    EXPECT_TRUE(found_rapid);
}

/* ============================================================================
 * Immune Response Triggering Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceImmuneTest, ImmuneTrigger_AnomalyPresentsAntigen) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    // Create anomaly
    gw_broadcast_anomaly_t anomaly;
    anomaly.type = GW_ANOMALY_MODULE_HIJACK;
    anomaly.severity = GW_ANOMALY_SEVERITY_SEVERE;
    anomaly.broadcast_id = 1;
    anomaly.source_module = MODULE_WORKING_MEMORY;
    anomaly.timestamp_ms = 0;
    anomaly.anomaly_score = 0.8f;
    snprintf(anomaly.description, sizeof(anomaly.description), "Test hijacking");

    // Trigger immune response
    int result = gw_immune_trigger_response(context, &anomaly);
    EXPECT_EQ(result, 0);

    // Check that antigen was presented
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);
}

TEST_F(GlobalWorkspaceImmuneTest, ImmuneTrigger_SeverityMapsToAntigenSeverity) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    // Test each severity level
    gw_anomaly_severity_t severities[] = {
        GW_ANOMALY_SEVERITY_MILD,
        GW_ANOMALY_SEVERITY_MODERATE,
        GW_ANOMALY_SEVERITY_SEVERE,
        GW_ANOMALY_SEVERITY_CRITICAL
    };

    for (int i = 0; i < 4; i++) {
        gw_broadcast_anomaly_t anomaly;
        anomaly.type = GW_ANOMALY_CORRUPTED_CONTENT;
        anomaly.severity = severities[i];
        anomaly.broadcast_id = i;
        anomaly.source_module = MODULE_PERCEPTION;
        anomaly.timestamp_ms = 0;
        anomaly.anomaly_score = 0.9f;
        snprintf(anomaly.description, sizeof(anomaly.description), "Test %d", i);

        int result = gw_immune_trigger_response(context, &anomaly);
        EXPECT_EQ(result, 0);
    }

    // Check that antigens were presented
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_EQ(stats.antigens_processed, 4u);
}

TEST_F(GlobalWorkspaceImmuneTest, ImmuneTrigger_AutoTriggerOnAnomaly) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    // Enable auto-trigger
    context->anomaly_config.auto_trigger_immune = true;

    // Create corrupted content
    float content[256];
    create_corrupted_content(content, 256);

    // Broadcast (should auto-trigger immune)
    global_workspace_compete(workspace, MODULE_PERCEPTION, content, 256, 0.8f);
    uint32_t count = gw_immune_check_broadcast(context);

    // Should have triggered immune
    EXPECT_GT(count, 0u);

    // Check statistics
    uint64_t total_broadcasts, total_anomalies, total_triggers;
    gw_immune_get_stats(context, &total_broadcasts, &total_anomalies, &total_triggers);
    EXPECT_GT(total_anomalies, 0u);
    EXPECT_GT(total_triggers, 0u);
}

/* ============================================================================
 * Query and Statistics Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceImmuneTest, AnomalyHistory_Recording) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    context->anomaly_config.auto_trigger_immune = false;

    // Generate anomalies
    float content[256];
    create_test_content(content, 256, 0.5f);

    // Module hijacking (3+ consecutive)
    for (int i = 0; i < 4; i++) {
        global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content, 256, 0.8f);
        gw_immune_check_broadcast(context);
        usleep(60000);
    }

    // Get anomaly history
    gw_broadcast_anomaly_t history[10];
    uint32_t count;
    int result = gw_immune_get_anomaly_history(context, history, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GT(count, 0u);

    // Check most recent anomaly
    EXPECT_EQ(history[0].type, GW_ANOMALY_MODULE_HIJACK);
}

TEST_F(GlobalWorkspaceImmuneTest, Statistics_Tracking) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    context->anomaly_config.auto_trigger_immune = true;

    // Generate some broadcasts
    float content[256];
    create_test_content(content, 256, 0.5f);

    for (int i = 0; i < 5; i++) {
        global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content, 256, 0.8f);
        gw_immune_check_broadcast(context);
        usleep(60000);
    }

    // Get statistics
    uint64_t total_broadcasts, total_anomalies, total_triggers;
    int result = gw_immune_get_stats(context, &total_broadcasts, &total_anomalies, &total_triggers);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(total_broadcasts, 5u);
}

TEST_F(GlobalWorkspaceImmuneTest, PrintState_NoErrors) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    // Should not crash
    gw_immune_print_state(context, false);
    gw_immune_print_state(context, true);

    // NULL context should not crash
    gw_immune_print_state(nullptr, false);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceImmuneTest, UtilityFunctions_StringConversion) {
    // Anomaly type to string
    EXPECT_STREQ(gw_anomaly_type_to_string(GW_ANOMALY_NONE), "NONE");
    EXPECT_STREQ(gw_anomaly_type_to_string(GW_ANOMALY_RAPID_SWITCHING), "RAPID_SWITCHING");
    EXPECT_STREQ(gw_anomaly_type_to_string(GW_ANOMALY_MODULE_HIJACK), "MODULE_HIJACK");
    EXPECT_STREQ(gw_anomaly_type_to_string(GW_ANOMALY_CORRUPTED_CONTENT), "CORRUPTED_CONTENT");

    // Anomaly severity to string
    EXPECT_STREQ(gw_anomaly_severity_to_string(GW_ANOMALY_SEVERITY_NONE), "NONE");
    EXPECT_STREQ(gw_anomaly_severity_to_string(GW_ANOMALY_SEVERITY_MILD), "MILD");
    EXPECT_STREQ(gw_anomaly_severity_to_string(GW_ANOMALY_SEVERITY_MODERATE), "MODERATE");
    EXPECT_STREQ(gw_anomaly_severity_to_string(GW_ANOMALY_SEVERITY_SEVERE), "SEVERE");
    EXPECT_STREQ(gw_anomaly_severity_to_string(GW_ANOMALY_SEVERITY_CRITICAL), "CRITICAL");
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(GlobalWorkspaceImmuneTest, Integration_InflammationDisruptsBroadcast) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    float content[256];
    create_test_content(content, 256, 0.7f);

    // Normal broadcast should succeed
    bool won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content, 256, 0.7f);
    EXPECT_TRUE(won);

    // Set high inflammation
    gw_immune_set_manual_inflammation(context, INFLAMMATION_SYSTEMIC);

    // Same strength broadcast should now fail (threshold increased)
    usleep(60000);
    won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content, 256, 0.7f);
    EXPECT_FALSE(won);  // Below raised threshold
}

TEST_F(GlobalWorkspaceImmuneTest, Integration_AnomalyTriggersImmuneLeadsToInflammation) {
    context = global_workspace_connect_immune(workspace, immune);
    ASSERT_NE(context, nullptr);

    context->anomaly_config.auto_trigger_immune = true;

    // Create severe anomaly (corrupted content)
    float content[256];
    create_corrupted_content(content, 256);

    // Broadcast (triggers immune)
    global_workspace_compete(workspace, MODULE_PERCEPTION, content, 256, 0.8f);
    gw_immune_check_broadcast(context);

    // Immune should have antigen
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GT(stats.antigens_processed, 0u);

    // Cytokines should be released (IL-1 for critical severity)
    EXPECT_GT(stats.cytokines_released, 0u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
