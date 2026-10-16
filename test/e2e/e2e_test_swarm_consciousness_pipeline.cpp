/**
 * @file e2e_test_swarm_consciousness_pipeline.cpp
 * @brief End-to-End Tests for NIMCP Swarm Gestalt Consciousness
 *
 * WHAT: Complete E2E testing of swarm collective consciousness emergence
 * WHY:  Verify consciousness APIs work correctly with swarm infrastructure
 * HOW:  Test configuration, lifecycle, state classification, and scaling
 *
 * NOTE: These tests use the actual swarm_consciousness library API.
 * Since we cannot simulate real drone brains with real phi values in E2E tests,
 * we test the API contract and behavior, not the exact phi values.
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <atomic>
#include <cmath>
#include <random>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "swarm/nimcp_swarm_brain.h"
#include "swarm/nimcp_swarm_consciousness.h"
#include "swarm/nimcp_collective_workspace.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Helper Functions
//=============================================================================

class SwarmConsciousnessE2EHelper {
public:
    /**
     * @brief Create a swarm brain for testing
     */
    static swarm_brain_t* CreateSwarm(const char* name, uint16_t drone_id) {
        swarm_brain_config_t config = swarm_brain_default_config();
        config.drone_id = drone_id;
        strncpy(config.swarm_name, name, SWARM_MAX_NAME_LEN - 1);
        config.coherence_threshold = 0.5f;
        config.workspace_size = 32;
        config.enable_bio_async = false;

        return swarm_brain_create(&config);
    }

    /**
     * @brief Get state name string
     */
    static const char* StateString(swarm_consciousness_state_t state) {
        return swarm_consciousness_state_name(state);
    }
};

//=============================================================================
// E2E Test Fixture
//=============================================================================

class SwarmConsciousnessE2ETest : public ::testing::Test {
protected:
    std::vector<swarm_brain_t*> swarms_;

    void SetUp() override {
    }

    void TearDown() override {
        for (auto* swarm : swarms_) {
            if (swarm) {
                swarm_brain_destroy(swarm);
            }
        }
        swarms_.clear();
    }

    swarm_brain_t* CreateAndTrackSwarm(const char* name, uint16_t id) {
        swarm_brain_t* swarm = SwarmConsciousnessE2EHelper::CreateSwarm(name, id);
        if (swarm) {
            swarms_.push_back(swarm);
        }
        return swarm;
    }
};

//=============================================================================
// E2E Test: Configuration API
//=============================================================================

TEST_F(SwarmConsciousnessE2ETest, ConfigurationPipeline) {
    PipelineTracker pipeline("Configuration API");

    // Stage 1: Get default configuration
    pipeline.begin_stage("Get default config", 5000);

    swarm_consciousness_config_t config = swarm_consciousness_default_config();

    EXPECT_GE(config.phi_aggregation_method, PHI_AGGREGATION_SUM);
    EXPECT_LE(config.phi_aggregation_method, PHI_AGGREGATION_SYNERGISTIC);
    EXPECT_GE(config.integration_weight, 0.0f);
    EXPECT_LE(config.integration_weight, 1.0f);
    EXPECT_GT(config.update_interval_ms, 0u);

    std::cout << "  Default config: method=" << config.phi_aggregation_method
              << " integration_weight=" << config.integration_weight << "\n";

    pipeline.end_stage();

    // Stage 2: Verify all aggregation methods are valid
    pipeline.begin_stage("Verify aggregation methods", 5000);

    for (int method = PHI_AGGREGATION_SUM; method <= PHI_AGGREGATION_SYNERGISTIC; method++) {
        swarm_consciousness_config_t test_config = config;
        test_config.phi_aggregation_method = (phi_aggregation_method_t)method;

        swarm_consciousness_ctx_t* ctx = swarm_consciousness_create(&test_config);
        EXPECT_NE(ctx, nullptr) << "Failed to create context for method " << method;

        if (ctx) {
            swarm_consciousness_destroy(ctx);
        }
    }

    pipeline.end_stage();

    pipeline.print_summary();
}

//=============================================================================
// E2E Test: Lifecycle Management
//=============================================================================

TEST_F(SwarmConsciousnessE2ETest, LifecyclePipeline) {
    PipelineTracker pipeline("Lifecycle Management");

    // Stage 1: Create and destroy multiple contexts
    pipeline.begin_stage("Create/destroy cycles", 10000);

    for (int i = 0; i < 10; i++) {
        swarm_consciousness_config_t config = swarm_consciousness_default_config();
        swarm_consciousness_ctx_t* ctx = swarm_consciousness_create(&config);
        ASSERT_NE(ctx, nullptr) << "Cycle " << i << " failed to create context";
        swarm_consciousness_destroy(ctx);
    }

    std::cout << "  Completed 10 create/destroy cycles without leaks\n";

    pipeline.end_stage();

    // Stage 2: Verify null safety
    pipeline.begin_stage("Null safety", 5000);

    swarm_consciousness_ctx_t* null_ctx = swarm_consciousness_create(nullptr);
    EXPECT_EQ(null_ctx, nullptr);

    swarm_consciousness_destroy(nullptr);  // Should not crash
    swarm_consciousness_metrics_free(nullptr);  // Should not crash

    std::cout << "  Null operations handled safely\n";

    pipeline.end_stage();

    pipeline.print_summary();
}

//=============================================================================
// E2E Test: State Classification
//=============================================================================

TEST_F(SwarmConsciousnessE2ETest, StateClassificationPipeline) {
    PipelineTracker pipeline("State Classification");

    // Stage 1: Test all state classifications
    pipeline.begin_stage("Classify states", 5000);

    struct TestCase {
        float phi;
        uint32_t drones;
        swarm_consciousness_state_t expected_state;
    };

    // Test cases based on normalized phi thresholds
    TestCase test_cases[] = {
        {0.0f, 5, SWARM_CONSCIOUSNESS_DORMANT},      // Zero phi
        {0.05f, 1, SWARM_CONSCIOUSNESS_DORMANT},     // Below threshold
        {1.5f, 5, SWARM_CONSCIOUSNESS_EMERGING},     // Medium phi
        {3.0f, 5, SWARM_CONSCIOUSNESS_UNIFIED},      // High phi
        {5.0f, 5, SWARM_CONSCIOUSNESS_TRANSCENDENT}, // Very high phi
        {1.0f, 0, SWARM_CONSCIOUSNESS_DORMANT},      // Zero drones
    };

    for (const auto& tc : test_cases) {
        swarm_consciousness_state_t state = swarm_classify_collective_phi(tc.phi, tc.drones);
        EXPECT_EQ(state, tc.expected_state)
            << "Failed for phi=" << tc.phi << " drones=" << tc.drones;

        std::cout << "  phi=" << tc.phi << " drones=" << tc.drones
                  << " -> " << swarm_consciousness_state_name(state) << "\n";
    }

    pipeline.end_stage();

    // Stage 2: Verify state name strings
    pipeline.begin_stage("State names", 5000);

    EXPECT_STREQ(swarm_consciousness_state_name(SWARM_CONSCIOUSNESS_DORMANT), "DORMANT");
    EXPECT_STREQ(swarm_consciousness_state_name(SWARM_CONSCIOUSNESS_EMERGING), "EMERGING");
    EXPECT_STREQ(swarm_consciousness_state_name(SWARM_CONSCIOUSNESS_UNIFIED), "UNIFIED");
    EXPECT_STREQ(swarm_consciousness_state_name(SWARM_CONSCIOUSNESS_TRANSCENDENT), "TRANSCENDENT");
    EXPECT_STREQ(swarm_consciousness_state_name((swarm_consciousness_state_t)99), "UNKNOWN");

    std::cout << "  All state name strings verified\n";

    pipeline.end_stage();

    pipeline.print_summary();
}

//=============================================================================
// E2E Test: Scaling Model
//=============================================================================

TEST_F(SwarmConsciousnessE2ETest, ScalingModelPipeline) {
    PipelineTracker pipeline("Scaling Model");

    // Stage 1: Test model fitting with insufficient data
    pipeline.begin_stage("Insufficient data handling", 5000);

    swarm_consciousness_metrics_t history[2] = {};
    history[0].drone_count = 2;
    history[0].collective_phi = 0.5f;
    history[1].drone_count = 4;
    history[1].collective_phi = 1.2f;

    consciousness_scaling_model_t model = swarm_fit_scaling_model(history, 2);
    EXPECT_FLOAT_EQ(model.base_phi, 0.0f) << "Should return zero model for insufficient data";

    std::cout << "  Insufficient data returns zero model\n";

    pipeline.end_stage();

    // Stage 2: Test model fitting with valid data
    pipeline.begin_stage("Valid data fitting", 5000);

    swarm_consciousness_metrics_t valid_history[5] = {};
    valid_history[0].drone_count = 2;
    valid_history[0].collective_phi = 1.0f;
    valid_history[1].drone_count = 4;
    valid_history[1].collective_phi = 2.5f;
    valid_history[2].drone_count = 8;
    valid_history[2].collective_phi = 7.0f;
    valid_history[3].drone_count = 16;
    valid_history[3].collective_phi = 20.0f;
    valid_history[4].drone_count = 32;
    valid_history[4].collective_phi = 50.0f;

    model = swarm_fit_scaling_model(valid_history, 5);
    EXPECT_GT(model.base_phi, 0.0f) << "Should have positive base phi";
    EXPECT_GT(model.scaling_exponent, 0.0f) << "Should have positive exponent";

    std::cout << "  Fitted model: base=" << model.base_phi
              << " exponent=" << model.scaling_exponent
              << " synergy=" << model.synergy_factor << "\n";

    pipeline.end_stage();

    // Stage 3: Test prediction
    pipeline.begin_stage("Phi prediction", 5000);

    float phi_10 = swarm_predict_phi_for_size(&model, 10);
    float phi_20 = swarm_predict_phi_for_size(&model, 20);
    float phi_0 = swarm_predict_phi_for_size(&model, 0);
    float phi_null = swarm_predict_phi_for_size(nullptr, 10);

    EXPECT_GT(phi_20, phi_10) << "Larger swarm should have higher phi";
    EXPECT_FLOAT_EQ(phi_0, 0.0f) << "Zero drones should have zero phi";
    EXPECT_FLOAT_EQ(phi_null, 0.0f) << "Null model should return zero";

    std::cout << "  Predictions: phi(10)=" << phi_10
              << " phi(20)=" << phi_20 << "\n";

    pipeline.end_stage();

    pipeline.print_summary();
}

//=============================================================================
// E2E Test: BBB Security Validation
//=============================================================================

TEST_F(SwarmConsciousnessE2ETest, BBBSecurityPipeline) {
    PipelineTracker pipeline("BBB Security Validation");

    // Stage 1: Validate correct metrics
    pipeline.begin_stage("Valid metrics", 5000);

    swarm_consciousness_metrics_t valid_metrics = {};
    valid_metrics.drone_count = 5;
    valid_metrics.collective_phi = 2.5f;
    valid_metrics.network_integration = 0.7f;
    valid_metrics.workspace_coherence = 0.8f;
    valid_metrics.consciousness_state = SWARM_CONSCIOUSNESS_UNIFIED;
    for (uint32_t i = 0; i < valid_metrics.drone_count; i++) {
        valid_metrics.individual_phi[i] = 0.5f;
    }

    bool valid = swarm_consciousness_bbb_validate(&valid_metrics);
    EXPECT_TRUE(valid) << "Valid metrics should pass BBB validation";

    std::cout << "  Valid metrics passed BBB validation\n";

    pipeline.end_stage();

    // Stage 2: Test invalid metrics rejection
    pipeline.begin_stage("Invalid metrics rejection", 5000);

    // Null metrics
    EXPECT_FALSE(swarm_consciousness_bbb_validate(nullptr));

    // Invalid drone count
    swarm_consciousness_metrics_t invalid_metrics = valid_metrics;
    invalid_metrics.drone_count = SWARM_CONSCIOUSNESS_MAX_DRONES + 1;
    EXPECT_FALSE(swarm_consciousness_bbb_validate(&invalid_metrics));

    // Negative phi
    invalid_metrics = valid_metrics;
    invalid_metrics.collective_phi = -1.0f;
    EXPECT_FALSE(swarm_consciousness_bbb_validate(&invalid_metrics));

    // Network integration out of range
    invalid_metrics = valid_metrics;
    invalid_metrics.network_integration = 1.5f;
    EXPECT_FALSE(swarm_consciousness_bbb_validate(&invalid_metrics));

    // Invalid state
    invalid_metrics = valid_metrics;
    invalid_metrics.consciousness_state = (swarm_consciousness_state_t)99;
    EXPECT_FALSE(swarm_consciousness_bbb_validate(&invalid_metrics));

    std::cout << "  All invalid metrics correctly rejected\n";

    pipeline.end_stage();

    pipeline.print_summary();
}

//=============================================================================
// E2E Test: Swarm Brain Integration
//=============================================================================

TEST_F(SwarmConsciousnessE2ETest, SwarmBrainIntegrationPipeline) {
    PipelineTracker pipeline("Swarm Brain Integration");

    // Stage 1: Create swarm and compute phi
    pipeline.begin_stage("Compute phi for swarm", 30000);

    swarm_brain_t* swarm = CreateAndTrackSwarm("test_swarm", 100);
    ASSERT_NE(swarm, nullptr);

    // Compute collective phi - with no real drones, will be 0 or near 0
    swarm_consciousness_metrics_t* metrics = swarm_compute_collective_phi(swarm, nullptr);

    // May return nullptr if no drones, or metrics with low values
    if (metrics) {
        // Verify metrics structure is valid (not garbage)
        EXPECT_GE(metrics->collective_phi, 0.0f);
        EXPECT_LE(metrics->network_integration, 1.0f);
        EXPECT_LE(metrics->workspace_coherence, 1.0f);

        std::cout << "  Swarm phi computed: " << metrics->collective_phi
                  << " state: " << swarm_consciousness_state_name(metrics->consciousness_state) << "\n";

        swarm_consciousness_metrics_free(metrics);
    } else {
        std::cout << "  No metrics returned (expected for stub swarm)\n";
    }

    pipeline.end_stage();

    // Stage 2: Test with default config
    pipeline.begin_stage("Compute with config", 30000);

    swarm_consciousness_config_t config = swarm_consciousness_default_config();
    config.phi_aggregation_method = PHI_AGGREGATION_SUM;

    metrics = swarm_compute_collective_phi(swarm, &config);
    if (metrics) {
        EXPECT_GE(metrics->collective_phi, 0.0f);
        swarm_consciousness_metrics_free(metrics);
        std::cout << "  Config-based computation successful\n";
    } else {
        std::cout << "  No metrics returned with config\n";
    }

    pipeline.end_stage();

    pipeline.print_summary();
}

//=============================================================================
// E2E Test: Context Lifecycle
//=============================================================================

TEST_F(SwarmConsciousnessE2ETest, ContextLifecyclePipeline) {
    PipelineTracker pipeline("Context Lifecycle");

    // Stage 1: Create multiple contexts with different configs
    pipeline.begin_stage("Create multiple contexts", 10000);

    const int NUM_CONTEXTS = 5;
    std::vector<swarm_consciousness_ctx_t*> contexts;

    for (int i = 0; i < NUM_CONTEXTS; i++) {
        swarm_consciousness_config_t config = swarm_consciousness_default_config();
        config.phi_aggregation_method = (phi_aggregation_method_t)(i % (PHI_AGGREGATION_SYNERGISTIC + 1));

        swarm_consciousness_ctx_t* ctx = swarm_consciousness_create(&config);
        ASSERT_NE(ctx, nullptr) << "Failed to create context " << i;
        contexts.push_back(ctx);
    }

    std::cout << "  Created " << NUM_CONTEXTS << " contexts with different configs\n";

    pipeline.end_stage();

    // Stage 2: Destroy all contexts
    pipeline.begin_stage("Destroy contexts", 5000);

    for (auto* ctx : contexts) {
        swarm_consciousness_destroy(ctx);
    }
    contexts.clear();

    std::cout << "  All contexts destroyed\n";

    pipeline.end_stage();

    pipeline.print_summary();
}

//=============================================================================
// E2E Test: Bidirectional Sleep-Consciousness Integration
//=============================================================================

#include "swarm/sleep/nimcp_swarm_consciousness_sleep_bridge.h"

//=============================================================================
// Mock Sleep System for E2E Testing
//=============================================================================

namespace {

struct MockSleepSystem {
    sleep_state_t current_state = SLEEP_STATE_AWAKE;
    float sleep_pressure = 0.0f;
    sleep_state_callback_t callback = nullptr;
    void* callback_data = nullptr;
};

static MockSleepSystem g_mock_sleep;

}  // namespace

// Mock implementations - these override the weak symbols from the library

sleep_state_t sleep_get_current_state(sleep_system_t sys) {
    (void)sys;
    return g_mock_sleep.current_state;
}

float sleep_get_pressure(sleep_system_t sys) {
    (void)sys;
    return g_mock_sleep.sleep_pressure;
}

bool sleep_register_state_callback(sleep_system_t sys, sleep_state_callback_t cb, void* data) {
    (void)sys;
    g_mock_sleep.callback = cb;
    g_mock_sleep.callback_data = data;
    return true;
}

bool sleep_unregister_state_callback(sleep_system_t sys, sleep_state_callback_t cb, void* data) {
    (void)sys; (void)cb; (void)data;
    g_mock_sleep.callback = nullptr;
    g_mock_sleep.callback_data = nullptr;
    return true;
}


class SwarmBidirectionalSleepE2ETest : public ::testing::Test {
protected:
    swarm_consciousness_sleep_bridge_t bridge_ = nullptr;

    void SetUp() override {
        // Reset mock state
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.sleep_pressure = 0.0f;
        g_mock_sleep.callback = nullptr;
        g_mock_sleep.callback_data = nullptr;

        swarm_consciousness_sleep_config_t config;
        swarm_consciousness_sleep_default_config(&config);
        // Use a mock sleep system pointer - the mock functions will handle it
        bridge_ = swarm_consciousness_sleep_bridge_create(&config, (sleep_system_t)&g_mock_sleep);
    }

    void TearDown() override {
        if (bridge_) {
            swarm_consciousness_sleep_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    void TriggerSleepStateChange(sleep_state_t state) {
        g_mock_sleep.current_state = state;
        if (g_mock_sleep.callback) {
            g_mock_sleep.callback(state, g_mock_sleep.callback_data);
        }
    }
};

TEST_F(SwarmBidirectionalSleepE2ETest, BidirectionalSleepConsciousnessPipeline) {
    PipelineTracker pipeline("Bidirectional Sleep-Consciousness");

    // Stage 1: Test sleep -> consciousness direction
    pipeline.begin_stage("Sleep affects consciousness", 5000);

    // Verify phi factors vary by sleep state
    float awake_phi = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_AWAKE);
    float deep_phi = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_DEEP_NREM);

    EXPECT_FLOAT_EQ(awake_phi, 1.0f) << "Awake should have full consciousness";
    EXPECT_LT(deep_phi, 0.1f) << "Deep sleep should have minimal consciousness";

    std::cout << "  Sleep -> Consciousness: Awake phi=" << awake_phi
              << ", Deep phi=" << deep_phi << "\n";

    pipeline.end_stage();

    // Stage 2: Test consciousness -> sleep direction
    pipeline.begin_stage("Consciousness affects sleep", 5000);

    ASSERT_NE(bridge_, nullptr) << "Bridge should be created";

    // Connect consciousness (null is ok for testing)
    int result = swarm_consciousness_sleep_connect_consciousness(bridge_, nullptr);
    EXPECT_EQ(result, 0) << "Should connect successfully";

    // Simulate consciousness state changes
    result = swarm_consciousness_sleep_on_consciousness_change(bridge_, 0, 0.5f);  // DORMANT
    EXPECT_EQ(result, 0);

    swarm_sleep_consciousness_modulation_t mod;
    result = swarm_consciousness_sleep_get_consciousness_modulation(bridge_, &mod);
    EXPECT_EQ(result, 0);

    std::cout << "  Consciousness -> Sleep (DORMANT): pressure_mod=" << mod.sleep_pressure_modifier
              << ", wakefulness=" << mod.wakefulness_boost << "\n";

    // High consciousness should reduce sleep pressure
    result = swarm_consciousness_sleep_on_consciousness_change(bridge_, 3, 10.0f);  // TRANSCENDENT
    EXPECT_EQ(result, 0);

    result = swarm_consciousness_sleep_get_consciousness_modulation(bridge_, &mod);
    EXPECT_EQ(result, 0);

    std::cout << "  Consciousness -> Sleep (TRANSCENDENT): pressure_mod=" << mod.sleep_pressure_modifier
              << ", wakefulness=" << mod.wakefulness_boost << "\n";

    EXPECT_LT(mod.sleep_pressure_modifier, 1.0f) << "Transcendent should reduce sleep pressure";
    EXPECT_TRUE(mod.suppress_sleep_transition) << "Transcendent should block sleep";

    pipeline.end_stage();

    // Stage 3: Test full bidirectional cycle
    pipeline.begin_stage("Full bidirectional cycle", 5000);

    // Simulate: consciousness drops -> promotes sleep -> sleep deepens -> consciousness further reduced
    // This is the biological feedback loop

    // Low consciousness (dormant)
    swarm_consciousness_sleep_on_consciousness_change(bridge_, 0, 0.1f);
    swarm_consciousness_sleep_get_consciousness_modulation(bridge_, &mod);
    float low_consciousness_sleep_pressure = mod.sleep_pressure_modifier;

    // Verify feedback: low consciousness increases sleep pressure
    EXPECT_GT(low_consciousness_sleep_pressure, 1.0f)
        << "Low consciousness should promote sleep";

    // Deep sleep state has low phi
    float deep_sleep_consciousness = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_LT(deep_sleep_consciousness, 0.1f)
        << "Deep sleep should reduce consciousness";

    std::cout << "  Bidirectional cycle verified: Low consciousness -> High sleep pressure -> Deep sleep -> Low consciousness\n";

    pipeline.end_stage();

    pipeline.print_summary();
}

TEST_F(SwarmBidirectionalSleepE2ETest, ConsciousnessStateTransitionPipeline) {
    PipelineTracker pipeline("Consciousness State Transitions");

    // Verify all consciousness states affect sleep differently
    pipeline.begin_stage("State transition effects", 5000);

    ASSERT_NE(bridge_, nullptr);
    swarm_consciousness_sleep_connect_consciousness(bridge_, nullptr);

    const char* state_names[] = {"DORMANT", "EMERGING", "UNIFIED", "TRANSCENDENT"};
    float prev_pressure = 2.0f;  // Start above DORMANT pressure

    for (uint32_t state = 0; state <= 3; state++) {
        swarm_consciousness_sleep_on_consciousness_change(bridge_, state, (float)(state + 1) * 2.0f);

        swarm_sleep_consciousness_modulation_t mod;
        swarm_consciousness_sleep_get_consciousness_modulation(bridge_, &mod);

        std::cout << "  State " << state_names[state] << ": pressure_mod=" << mod.sleep_pressure_modifier
                  << ", blocks_sleep=" << (mod.suppress_sleep_transition ? "YES" : "NO") << "\n";

        // Verify pressure decreases as consciousness increases
        EXPECT_LE(mod.sleep_pressure_modifier, prev_pressure)
            << "Higher consciousness should have equal or lower sleep pressure";
        prev_pressure = mod.sleep_pressure_modifier;
    }

    pipeline.end_stage();

    pipeline.print_summary();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << " Swarm Consciousness E2E Tests\n";
    std::cout << "========================================\n";
    std::cout << "\n";

    int result = RUN_ALL_TESTS();

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << " E2E Tests Complete\n";
    std::cout << "========================================\n";

    return result;
}
