/**
 * @file e2e_test_health_agent_pipeline.cpp
 * @brief End-to-End tests for Health Agent USE functions pipeline
 * @version 1.0.0
 * @date 2025-01-17
 *
 * WHAT: Test complete health agent workflows from anomaly detection to recovery
 * WHY:  Verify USE functions work together in realistic operational scenarios
 * HOW:  Multi-stage pipelines testing full system integration
 *
 * E2E PIPELINES:
 * 1. Anomaly Detection Pipeline: Detect -> Track -> Predict -> Respond
 * 2. Threat Response Pipeline: Detect -> Classify -> Generate Response -> Memory
 * 3. Resource Management Pipeline: Monitor -> Adjust Tier -> Degrade -> Recover
 * 4. Memory Consolidation Pipeline: Store -> Replay -> Consolidate -> Verify
 * 5. Full Agent Lifecycle: Create -> Connect -> Start -> Operate -> Stop -> Destroy
 */

#include "e2e_test_framework.h"
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "portia/nimcp_portia.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "swarm/nimcp_swarm_immune.h"
#include "swarm/nimcp_swarm_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Helper (not a GTest fixture)
//=============================================================================

class HealthAgentE2EHelper {
public:
    nimcp_health_agent_t* agent = nullptr;
    portia_context_t* portia_ctx = nullptr;
    dragonfly_system_t* dragonfly = nullptr;
    NimcpSwarmImmuneSystem* swarm_immune = nullptr;
    NimcpSwarmMemory* swarm_memory = nullptr;

    HealthAgentE2EHelper() {
        // Reset to clean state
        agent = nullptr;
        portia_ctx = nullptr;
        dragonfly = nullptr;
        swarm_immune = nullptr;
        swarm_memory = nullptr;
    }

    ~HealthAgentE2EHelper() {
        // Cleanup in reverse order
        if (agent) {
            if (nimcp_health_agent_is_running(agent)) {
                nimcp_health_agent_stop(agent);
            }
            nimcp_health_agent_destroy(agent);
        }
        if (dragonfly) {
            dragonfly_system_destroy(dragonfly);
        }
        if (swarm_immune) {
            nimcp_swarm_immune_destroy(swarm_immune);
        }
        if (swarm_memory) {
            nimcp_swarm_memory_destroy(swarm_memory);
        }
        // Clean up Portia
        if (portia_ctx) {
            portia_destroy();
            portia_ctx = nullptr;
        }
    }

    // Helper to create agent with all modules
    void setup_full_agent() {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        config.enable_auto_recovery = false;

        agent = nimcp_health_agent_create(&config);

        // Initialize Portia subsystem first with defaults
        portia_config_t portia_sys_config = portia_get_default_config();
        portia_init(&portia_sys_config);
        portia_ctx = portia_get_context();

        // Connect Portia
        health_agent_portia_config_t portia_cfg;
        memset(&portia_cfg, 0, sizeof(portia_cfg));
        portia_cfg.enable_portia = true;
        portia_cfg.enable_tier_monitoring = true;
        portia_cfg.enable_degradation_coordination = true;
        portia_cfg.enable_auto_tier_switch = true;
        portia_cfg.degradation_trigger_threshold = 0.3f;
        portia_cfg.upgrade_health_threshold = 0.8f;
        portia_cfg.tier_check_interval_ms = 500;
        nimcp_health_agent_connect_portia(agent, portia_ctx, &portia_cfg);

        // Create Dragonfly
        dragonfly_config_t df_cfg = dragonfly_default_config();
        dragonfly = dragonfly_system_create(&df_cfg);
        if (dragonfly) {
            health_agent_dragonfly_config_t df_agent_cfg;
            memset(&df_agent_cfg, 0, sizeof(df_agent_cfg));
            df_agent_cfg.enable_dragonfly = true;
            df_agent_cfg.enable_anomaly_tracking = true;
            df_agent_cfg.enable_pursuit_mode = true;
            df_agent_cfg.enable_interception = true;
            df_agent_cfg.enable_prediction_integration = true;
            df_agent_cfg.lock_on_severity_threshold = 0.5f;
            df_agent_cfg.pursuit_timeout_s = 5.0f;
            df_agent_cfg.update_rate_hz = 10;
            nimcp_health_agent_connect_dragonfly(agent, dragonfly, &df_agent_cfg);
        }

        // Create Swarm Immune
        NimcpSwarmImmuneConfig immune_cfg;
        nimcp_swarm_immune_default_config(&immune_cfg);
        swarm_immune = nimcp_swarm_immune_create(&immune_cfg, nullptr, 1);
        if (swarm_immune) {
            health_agent_swarm_immune_config_t immune_agent_cfg;
            memset(&immune_agent_cfg, 0, sizeof(immune_agent_cfg));
            immune_agent_cfg.enable_swarm_immune = true;
            immune_agent_cfg.enable_threat_detection = true;
            immune_agent_cfg.enable_coordinated_response = true;
            immune_agent_cfg.enable_memory_sharing = true;
            immune_agent_cfg.threat_detection_threshold = 0.5f;
            immune_agent_cfg.consensus_timeout_ms = 1000;
            nimcp_health_agent_connect_swarm_immune(agent, swarm_immune, &immune_agent_cfg);
        }

        // Create Swarm Memory
        swarm_memory = nimcp_swarm_memory_create(1000, 3);  // capacity, replication factor
        if (swarm_memory) {
            nimcp_swarm_memory_init(swarm_memory, nullptr);
            health_agent_swarm_memory_config_t mem_agent_cfg;
            memset(&mem_agent_cfg, 0, sizeof(mem_agent_cfg));
            mem_agent_cfg.enable_swarm_memory = true;
            mem_agent_cfg.enable_distributed_storage = true;
            mem_agent_cfg.enable_memory_replay = true;
            mem_agent_cfg.enable_consolidation = true;
            mem_agent_cfg.replay_priority_threshold = 0.3f;
            mem_agent_cfg.consolidation_interval_ms = 5000;
            nimcp_health_agent_connect_swarm_memory(agent, swarm_memory, &mem_agent_cfg);
        }
    }

    health_agent_message_t create_anomaly(
        health_agent_severity_t severity,
        const char* desc
    ) {
        health_agent_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = HEALTH_MSG_ANOMALY_DETECTED;
        msg.severity = severity;
        msg.source = HEALTH_SOURCE_NEURAL;
        msg.timestamp_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
        msg.anomaly_id = static_cast<uint64_t>(rand()) | (static_cast<uint64_t>(rand()) << 32);
        if (desc) {
            strncpy(msg.description, desc, sizeof(msg.description) - 1);
        }
        return msg;
    }
};

// Type alias for consistency with existing code
using HealthAgentE2ETest = HealthAgentE2EHelper;

//=============================================================================
// E2E Pipeline: Anomaly Detection -> Tracking -> Prediction -> Response
//=============================================================================

E2E_TEST(HealthAgentE2E, AnomalyDetectionPipeline) {
    PipelineTracker pipeline("Anomaly Detection Pipeline");

    // Stage 1: Create and configure agent
    pipeline.begin_stage("Agent Creation", 1000);
    HealthAgentE2ETest fixture;
    fixture.setup_full_agent();
    E2E_ASSERT_NOT_NULL(fixture.agent, "Agent creation failed");
    pipeline.end_stage();

    // Skip if dragonfly not available
    if (!fixture.dragonfly) {
        pipeline.fail_stage("Dragonfly not available - skipping");
        return;
    }

    // Stage 2: Start agent
    pipeline.begin_stage("Agent Startup", 500);
    int start_result = nimcp_health_agent_start(fixture.agent);
    E2E_ASSERT(start_result == 0, "Failed to start agent");
    E2E_ASSERT(nimcp_health_agent_is_running(fixture.agent), "Agent not running");
    pipeline.end_stage();

    // Stage 3: Detect and track anomalies
    pipeline.begin_stage("Anomaly Tracking", 2000);
    std::vector<uint32_t> tracked_targets;
    for (int i = 0; i < 10; i++) {
        health_agent_message_t msg = fixture.create_anomaly(
            static_cast<health_agent_severity_t>(HEALTH_SEVERITY_INFO + (i % 4)),
            "E2E test anomaly"
        );
        uint32_t target_id = 0;
        int result = nimcp_health_agent_use_dragonfly_track_anomaly(
            fixture.agent, &msg, &target_id
        );
        if (result == 0) {
            tracked_targets.push_back(target_id);
        }
    }
    E2E_ASSERT(tracked_targets.size() > 0, "No anomalies tracked");
    pipeline.end_stage();

    // Stage 4: Get predictions
    pipeline.begin_stage("Failure Prediction", 1000);
    for (uint32_t target_id : tracked_targets) {
        float time_to_failure = 0.0f;
        float confidence = 0.0f;
        nimcp_health_agent_use_dragonfly_predict(
            fixture.agent, target_id, &time_to_failure, &confidence
        );
    }
    pipeline.end_stage();

    // Stage 5: Verify mode and respond
    pipeline.begin_stage("Response Handling", 500);
    uint32_t mode = 0;
    E2E_ASSERT(nimcp_health_agent_use_dragonfly_get_mode(fixture.agent, &mode) == 0,
               "Failed to get dragonfly mode");
    nimcp_health_agent_use_dragonfly_abort(fixture.agent);  // Cleanup
    pipeline.end_stage();

    // Stage 6: Stop agent
    pipeline.begin_stage("Agent Shutdown", 1000);
    E2E_ASSERT(nimcp_health_agent_stop(fixture.agent) == 0, "Failed to stop agent");
    E2E_ASSERT(!nimcp_health_agent_is_running(fixture.agent), "Agent still running");
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
}

//=============================================================================
// E2E Pipeline: Threat Detection -> Classification -> Response -> Memory
//=============================================================================

E2E_TEST(HealthAgentE2E, ThreatResponsePipeline) {
    PipelineTracker pipeline("Threat Response Pipeline");

    // Stage 1: Setup
    pipeline.begin_stage("System Setup", 1000);
    HealthAgentE2ETest fixture;
    fixture.setup_full_agent();
    E2E_ASSERT_NOT_NULL(fixture.agent, "Agent creation failed");
    pipeline.end_stage();

    if (!fixture.swarm_immune) {
        pipeline.fail_stage("Swarm Immune not available - skipping");
        return;
    }

    // Stage 2: Threat detection
    pipeline.begin_stage("Threat Detection", 2000);
    uint8_t threat_patterns[][8] = {
        {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8},
        {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE},
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77},
    };

    std::vector<uint32_t> detected_threats;
    for (int i = 0; i < 3; i++) {
        bool threat_detected = false;
        uint32_t threat_id = 0;
        nimcp_health_agent_use_swarm_detect_threat(
            fixture.agent, threat_patterns[i], 8, i + 100,
            &threat_detected, &threat_id
        );
        if (threat_detected && threat_id != 0) {
            detected_threats.push_back(threat_id);
        }
    }
    pipeline.end_stage();

    // Stage 3: Generate responses for detected threats
    pipeline.begin_stage("Response Generation", 1500);
    for (uint32_t threat_id : detected_threats) {
        uint32_t response_id = 0;
        nimcp_health_agent_use_swarm_generate_response(
            fixture.agent, threat_id, &response_id
        );
    }
    pipeline.end_stage();

    // Stage 4: Add memory cells for threat patterns
    pipeline.begin_stage("Memory Cell Formation", 1000);
    for (int i = 0; i < 3; i++) {
        uint32_t cell_id = 0;
        nimcp_health_agent_use_swarm_add_memory_cell(
            fixture.agent, threat_patterns[i], 8,
            static_cast<uint32_t>(RESPONSE_ISOLATION), &cell_id
        );
    }
    pipeline.end_stage();

    // Stage 5: Behavior monitoring
    pipeline.begin_stage("Behavior Monitoring", 1000);
    float total_anomaly_score = 0.0f;
    for (uint32_t comp_id = 1; comp_id <= 5; comp_id++) {
        float score = 0.0f;
        nimcp_health_agent_use_swarm_check_behavior(fixture.agent, comp_id, &score);
        total_anomaly_score += score;
    }
    E2E_ASSERT(total_anomaly_score >= 0.0f, "Invalid anomaly scores");
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
}

//=============================================================================
// E2E Pipeline: Resource Monitor -> Tier Adjust -> Degrade -> Recover
//=============================================================================

E2E_TEST(HealthAgentE2E, ResourceManagementPipeline) {
    PipelineTracker pipeline("Resource Management Pipeline");

    // Stage 1: Setup
    pipeline.begin_stage("System Setup", 1000);
    HealthAgentE2ETest fixture;
    fixture.setup_full_agent();
    E2E_ASSERT_NOT_NULL(fixture.agent, "Agent creation failed");
    pipeline.end_stage();

    // Stage 2: Initial full capacity
    pipeline.begin_stage("Full Capacity Mode", 500);
    E2E_ASSERT(nimcp_health_agent_use_portia_set_tier(fixture.agent, PLATFORM_TIER_FULL) == 0,
               "Failed to set full tier");
    E2E_ASSERT(nimcp_health_agent_use_portia_degrade(fixture.agent, 0) == 0,
               "Failed to clear degradation");

    uint32_t power, thermal, degradation;
    E2E_ASSERT(nimcp_health_agent_use_portia_get_status(
        fixture.agent, &power, &thermal, &degradation
    ) == 0, "Failed to get status");
    pipeline.end_stage();

    // Stage 3: Simulate resource pressure
    pipeline.begin_stage("Resource Pressure Simulation", 1500);
    for (int pressure = 0; pressure <= 10; pressure += 2) {
        E2E_ASSERT(nimcp_health_agent_use_portia_degrade(fixture.agent, pressure) == 0,
                   "Failed to set degradation");

        // Adjust tier based on pressure
        platform_tier_t target_tier;
        if (pressure >= 8) {
            target_tier = PLATFORM_TIER_MINIMAL;
        } else if (pressure >= 6) {
            target_tier = PLATFORM_TIER_CONSTRAINED;
        } else if (pressure >= 3) {
            target_tier = PLATFORM_TIER_MEDIUM;
        } else {
            target_tier = PLATFORM_TIER_FULL;
        }

        E2E_ASSERT(nimcp_health_agent_use_portia_set_tier(fixture.agent, target_tier) == 0,
                   "Failed to adjust tier");

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    pipeline.end_stage();

    // Stage 4: Neuron count recommendations at different tiers
    pipeline.begin_stage("Neuron Recommendations", 1000);
    platform_tier_t tiers[] = {PLATFORM_TIER_MINIMAL, PLATFORM_TIER_CONSTRAINED,
                               PLATFORM_TIER_MEDIUM, PLATFORM_TIER_FULL};
    for (auto t : tiers) {
        nimcp_health_agent_use_portia_set_tier(fixture.agent, t);
        uint32_t recommended = 0;
        E2E_ASSERT(nimcp_health_agent_use_portia_get_recommended_neurons(
            fixture.agent, &recommended
        ) == 0, "Failed to get recommendation");
    }
    pipeline.end_stage();

    // Stage 5: Recovery to full capacity
    pipeline.begin_stage("Capacity Recovery", 500);
    E2E_ASSERT(nimcp_health_agent_use_portia_set_tier(fixture.agent, PLATFORM_TIER_FULL) == 0,
               "Failed to recover to full tier");
    E2E_ASSERT(nimcp_health_agent_use_portia_degrade(fixture.agent, 0) == 0,
               "Failed to clear degradation");
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
}

//=============================================================================
// E2E Pipeline: Memory Store -> Replay -> Consolidate -> Verify
//=============================================================================

E2E_TEST(HealthAgentE2E, MemoryConsolidationPipeline) {
    PipelineTracker pipeline("Memory Consolidation Pipeline");

    // Stage 1: Setup
    pipeline.begin_stage("System Setup", 1000);
    HealthAgentE2ETest fixture;
    fixture.setup_full_agent();
    E2E_ASSERT_NOT_NULL(fixture.agent, "Agent creation failed");
    pipeline.end_stage();

    if (!fixture.swarm_memory) {
        pipeline.fail_stage("Swarm Memory not available - skipping");
        return;
    }

    // Stage 2: Store patterns
    pipeline.begin_stage("Pattern Storage", 2000);
    const int NUM_PATTERNS = 50;
    for (int i = 0; i < NUM_PATTERNS; i++) {
        uint8_t pattern[32];
        for (int j = 0; j < 32; j++) {
            pattern[j] = static_cast<uint8_t>((i * 7 + j * 3) % 256);
        }

        char pattern_id[64];
        int result = nimcp_health_agent_use_swarm_memory_store(
            fixture.agent, pattern, sizeof(pattern),
            i % 4,      // Varying types
            (i % 5) + 1, // Varying importance (1-5)
            pattern_id
        );
        E2E_ASSERT(result == 0, "Pattern storage failed");
    }
    pipeline.end_stage();

    // Stage 3: Initial statistics
    pipeline.begin_stage("Initial Statistics", 500);
    uint64_t total_before = 0, consolidated_before = 0;
    float strength_before = 0.0f;
    E2E_ASSERT(nimcp_health_agent_use_swarm_memory_get_stats(
        fixture.agent, &total_before, &consolidated_before, &strength_before
    ) == 0, "Failed to get initial stats");
    pipeline.end_stage();

    // Stage 4: Replay cycles
    pipeline.begin_stage("Memory Replay", 2000);
    int total_replayed = 0;
    for (int cycle = 0; cycle < 5; cycle++) {
        int replayed = nimcp_health_agent_use_swarm_memory_replay(fixture.agent, 10);
        E2E_ASSERT(replayed >= 0, "Replay failed");
        total_replayed += replayed;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    pipeline.end_stage();

    // Stage 5: Consolidation
    pipeline.begin_stage("Memory Consolidation", 3000);
    int consolidated = nimcp_health_agent_use_swarm_memory_consolidate(fixture.agent);
    E2E_ASSERT(consolidated >= 0, "Consolidation failed");
    pipeline.end_stage();

    // Stage 6: Final statistics verification
    pipeline.begin_stage("Statistics Verification", 500);
    uint64_t total_after = 0, consolidated_after = 0;
    float strength_after = 0.0f;
    E2E_ASSERT(nimcp_health_agent_use_swarm_memory_get_stats(
        fixture.agent, &total_after, &consolidated_after, &strength_after
    ) == 0, "Failed to get final stats");
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
}

//=============================================================================
// E2E Pipeline: Full Agent Lifecycle
//=============================================================================

E2E_TEST(HealthAgentE2E, FullAgentLifecycle) {
    PipelineTracker pipeline("Full Agent Lifecycle");

    HealthAgentE2ETest fixture;

    // Stage 1: Create agent with default config
    pipeline.begin_stage("Agent Creation", 500);
    health_agent_config_t config;
    nimcp_health_agent_default_config(&config);
    config.check_interval_ms = 50;
    fixture.agent = nimcp_health_agent_create(&config);
    E2E_ASSERT_NOT_NULL(fixture.agent, "Agent creation failed");
    pipeline.end_stage();

    // Stage 2: Connect modules
    pipeline.begin_stage("Module Connection", 1000);

    // Portia
    health_agent_portia_config_t portia_cfg;
    memset(&portia_cfg, 0, sizeof(portia_cfg));
    portia_cfg.enable_portia = true;
    portia_cfg.enable_tier_monitoring = true;
    portia_cfg.enable_auto_tier_switch = true;
    E2E_ASSERT(nimcp_health_agent_connect_portia(fixture.agent, nullptr, &portia_cfg) == 0,
               "Portia connection failed");

    // Dragonfly
    dragonfly_config_t df_cfg = dragonfly_default_config();
    fixture.dragonfly = dragonfly_system_create(&df_cfg);
    if (fixture.dragonfly) {
        health_agent_dragonfly_config_t df_agent_cfg;
        memset(&df_agent_cfg, 0, sizeof(df_agent_cfg));
        df_agent_cfg.enable_dragonfly = true;
        df_agent_cfg.enable_anomaly_tracking = true;
        df_agent_cfg.enable_prediction_integration = true;
        E2E_ASSERT(nimcp_health_agent_connect_dragonfly(
            fixture.agent, fixture.dragonfly, &df_agent_cfg
        ) == 0, "Dragonfly connection failed");
    }

    // Swarm Immune
    NimcpSwarmImmuneConfig immune_cfg;
    nimcp_swarm_immune_default_config(&immune_cfg);
    fixture.swarm_immune = nimcp_swarm_immune_create(&immune_cfg, nullptr, 1);
    if (fixture.swarm_immune) {
        health_agent_swarm_immune_config_t immune_agent_cfg;
        memset(&immune_agent_cfg, 0, sizeof(immune_agent_cfg));
        immune_agent_cfg.enable_swarm_immune = true;
        immune_agent_cfg.enable_threat_detection = true;
        immune_agent_cfg.enable_coordinated_response = true;
        E2E_ASSERT(nimcp_health_agent_connect_swarm_immune(
            fixture.agent, fixture.swarm_immune, &immune_agent_cfg
        ) == 0, "Swarm Immune connection failed");
    }

    // Swarm Memory
    fixture.swarm_memory = nimcp_swarm_memory_create(1000, 3);  // capacity, replication factor
    if (fixture.swarm_memory) {
        health_agent_swarm_memory_config_t mem_agent_cfg;
        memset(&mem_agent_cfg, 0, sizeof(mem_agent_cfg));
        mem_agent_cfg.enable_swarm_memory = true;
        mem_agent_cfg.enable_distributed_storage = true;
        mem_agent_cfg.enable_memory_replay = true;
        E2E_ASSERT(nimcp_health_agent_connect_swarm_memory(
            fixture.agent, fixture.swarm_memory, &mem_agent_cfg
        ) == 0, "Swarm Memory connection failed");
    }
    pipeline.end_stage();

    // Stage 3: Start agent
    pipeline.begin_stage("Agent Startup", 500);
    E2E_ASSERT(nimcp_health_agent_start(fixture.agent) == 0, "Start failed");
    E2E_ASSERT(nimcp_health_agent_is_running(fixture.agent), "Agent not running");
    pipeline.end_stage();

    // Stage 4: Active operation
    pipeline.begin_stage("Active Operation", 3000);

    // Use all connected modules
    nimcp_health_agent_use_portia_set_tier(fixture.agent, PLATFORM_TIER_FULL);

    if (fixture.dragonfly) {
        health_agent_message_t msg = fixture.create_anomaly(
            HEALTH_SEVERITY_WARNING, "Lifecycle test anomaly"
        );
        uint32_t target_id;
        nimcp_health_agent_use_dragonfly_track_anomaly(fixture.agent, &msg, &target_id);
    }

    if (fixture.swarm_immune) {
        uint8_t data[] = {0x01, 0x02, 0x03};
        bool detected;
        nimcp_health_agent_use_swarm_detect_threat(
            fixture.agent, data, 3, 1, &detected, nullptr
        );
    }

    if (fixture.swarm_memory) {
        uint8_t pattern[] = {0xAA, 0xBB};
        nimcp_health_agent_use_swarm_memory_store(fixture.agent, pattern, 2, 0, 1, nullptr);
        nimcp_health_agent_use_swarm_memory_replay(fixture.agent, 1);
    }

    // Let agent run
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Get statistics
    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(fixture.agent, &stats);
    // Stats retrieval succeeded (void function)
    pipeline.end_stage();

    // Stage 5: Stop agent
    pipeline.begin_stage("Agent Shutdown", 1000);
    E2E_ASSERT(nimcp_health_agent_stop(fixture.agent) == 0, "Stop failed");
    E2E_ASSERT(!nimcp_health_agent_is_running(fixture.agent), "Agent still running");
    pipeline.end_stage();

    // Stage 6: Cleanup (destruction handled by fixture teardown)
    pipeline.begin_stage("Resource Cleanup", 500);
    // Modules will be cleaned up by fixture TearDown
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
}

//=============================================================================
// E2E Pipeline: Concurrent Multi-Module Operations
//=============================================================================

E2E_TEST(HealthAgentE2E, ConcurrentOperationsPipeline) {
    PipelineTracker pipeline("Concurrent Operations Pipeline");

    // Stage 1: Setup
    pipeline.begin_stage("System Setup", 1000);
    HealthAgentE2ETest fixture;
    fixture.setup_full_agent();
    E2E_ASSERT_NOT_NULL(fixture.agent, "Agent creation failed");
    E2E_ASSERT(nimcp_health_agent_start(fixture.agent) == 0, "Start failed");
    pipeline.end_stage();

    // Stage 2: Concurrent operations
    pipeline.begin_stage("Concurrent Operations", 5000);

    std::atomic<int> errors{0};
    std::atomic<int> operations{0};
    std::atomic<bool> running{true};

    // Portia worker
    auto portia_worker = [&]() {
        while (running) {
            for (int i = 0; i < 4 && running; i++) {
                if (nimcp_health_agent_use_portia_set_tier(
                    fixture.agent, static_cast<platform_tier_t>(i)
                ) != 0) {
                    errors++;
                }
                operations++;
            }
        }
    };

    // Dragonfly worker
    auto dragonfly_worker = [&]() {
        if (!fixture.dragonfly) return;
        while (running) {
            uint32_t mode;
            if (nimcp_health_agent_use_dragonfly_get_mode(fixture.agent, &mode) != 0) {
                errors++;
            }
            operations++;
            std::this_thread::yield();
        }
    };

    // Swarm immune worker
    auto immune_worker = [&]() {
        if (!fixture.swarm_immune) return;
        while (running) {
            float score;
            if (nimcp_health_agent_use_swarm_check_behavior(
                fixture.agent, 1, &score
            ) != 0) {
                errors++;
            }
            operations++;
            std::this_thread::yield();
        }
    };

    // Swarm memory worker
    auto memory_worker = [&]() {
        if (!fixture.swarm_memory) return;
        int count = 0;
        while (running && count < 100) {
            uint8_t data[] = {static_cast<uint8_t>(count)};
            if (nimcp_health_agent_use_swarm_memory_store(
                fixture.agent, data, 1, 0, 1, nullptr
            ) != 0) {
                errors++;
            }
            operations++;
            count++;
        }
    };

    // Launch workers
    std::vector<std::thread> threads;
    threads.emplace_back(portia_worker);
    threads.emplace_back(portia_worker);
    if (fixture.dragonfly) threads.emplace_back(dragonfly_worker);
    if (fixture.swarm_immune) threads.emplace_back(immune_worker);
    if (fixture.swarm_memory) threads.emplace_back(memory_worker);

    // Run for a while
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    running = false;

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }

    E2E_ASSERT(operations.load() > 100, "Too few operations completed");
    E2E_ASSERT(errors.load() < operations.load() / 10, "Too many errors");
    pipeline.end_stage();

    // Stage 3: Verify system state
    pipeline.begin_stage("State Verification", 500);
    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(fixture.agent, &stats);
    // Stats retrieval succeeded (void function)
    pipeline.end_stage();

    // Stage 4: Shutdown
    pipeline.begin_stage("Shutdown", 1000);
    E2E_ASSERT(nimcp_health_agent_stop(fixture.agent) == 0, "Stop failed");
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
}

//=============================================================================
// E2E Pipeline: Stress Test
//=============================================================================

E2E_TEST(HealthAgentE2E, StressTestPipeline) {
    PipelineTracker pipeline("Stress Test Pipeline");

    // Stage 1: Setup
    pipeline.begin_stage("System Setup", 1000);
    HealthAgentE2ETest fixture;
    fixture.setup_full_agent();
    E2E_ASSERT_NOT_NULL(fixture.agent, "Agent creation failed");
    pipeline.end_stage();

    // Stage 2: High volume operations
    pipeline.begin_stage("High Volume Operations", 10000);

    const int ITERATIONS = 500;
    int successful = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        // Rotate through operations
        switch (i % 6) {
            case 0:
                if (nimcp_health_agent_use_portia_set_tier(
                    fixture.agent, static_cast<platform_tier_t>(i % 4)
                ) == 0) successful++;
                break;

            case 1: {
                uint32_t power, thermal, degradation;
                if (nimcp_health_agent_use_portia_get_status(
                    fixture.agent, &power, &thermal, &degradation
                ) == 0) successful++;
                break;
            }

            case 2:
                if (fixture.dragonfly) {
                    uint32_t mode;
                    if (nimcp_health_agent_use_dragonfly_get_mode(
                        fixture.agent, &mode
                    ) == 0) successful++;
                }
                break;

            case 3:
                if (fixture.swarm_immune) {
                    float score;
                    if (nimcp_health_agent_use_swarm_check_behavior(
                        fixture.agent, i % 10 + 1, &score
                    ) == 0) successful++;
                }
                break;

            case 4:
                if (fixture.swarm_memory) {
                    uint8_t data[] = {static_cast<uint8_t>(i % 256)};
                    if (nimcp_health_agent_use_swarm_memory_store(
                        fixture.agent, data, 1, 0, 1, nullptr
                    ) == 0) successful++;
                }
                break;

            case 5: {
                uint32_t recommended;
                if (nimcp_health_agent_use_portia_get_recommended_neurons(
                    fixture.agent, &recommended
                ) == 0) successful++;
                break;
            }
        }
    }

    E2E_ASSERT(successful > ITERATIONS / 2, "Too few successful operations");
    pipeline.end_stage();

    // Stage 3: Memory consolidation after stress
    pipeline.begin_stage("Post-Stress Consolidation", 3000);
    if (fixture.swarm_memory) {
        int consolidated = nimcp_health_agent_use_swarm_memory_consolidate(fixture.agent);
        E2E_ASSERT(consolidated >= 0, "Consolidation failed");
    }
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
}
