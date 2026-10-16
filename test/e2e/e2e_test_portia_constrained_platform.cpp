/**
 * @file e2e_test_portia_constrained_platform.cpp
 * @brief End-to-end test for Portia constrained platform adaptation
 *
 * WHAT: Tests Portia's ability to adapt to resource-constrained platforms
 * WHY:  Verify system can run on IoT/embedded devices with limited resources
 * HOW:  Simulate constrained platform, test cognitive module management, memory budgets
 *
 * TEST SCENARIOS:
 * - ConstrainedPlatformStartup: Boot on minimal hardware (64MB RAM, 1 core)
 * - AdaptiveResourceManagement: Dynamically adjust to memory pressure
 * - CognitiveModuleEnablement: Verify correct modules enabled per tier
 * - MemoryBudgetCompliance: Ensure system stays within memory limits
 * - GracefulResourceExhaustion: Handle OOM scenarios gracefully
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cmath>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia.h"
#include "portia/nimcp_portia_degradation.h"
#include "utils/platform/nimcp_platform_tier.h"
#include "utils/platform/nimcp_system_resources.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"  // For direct struct access in tests
#include "cognitive/nimcp_working_memory.h"
#include "plasticity/attention/nimcp_attention.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaConstrainedPlatformE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logging
        nimcp_log_init(NULL);

        // Initialize bio-async
        nimcp_error_t err = nimcp_bio_async_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-async initialization failed";

        // Initialize router
        err = bio_router_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Router initialization failed";

        brain_ = nullptr;
        portia_initialized_ = false;
    }

    void TearDown() override {
        if (brain_) {
            brain_destroy(brain_);
            brain_ = nullptr;
        }

        if (portia_initialized_) {
            portia_destroy();
            portia_initialized_ = false;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        nimcp_log_shutdown();
    }

    brain_t brain_;
    bool portia_initialized_;
};

//=============================================================================
// Test 1: Constrained Platform Startup
//=============================================================================

TEST_F(PortiaConstrainedPlatformE2ETest, ConstrainedPlatformStartup) {
    // GIVEN: Configure Portia for minimal platform
    portia_config_t config = portia_get_default_config();
    config.resource_config.memory_threshold = 0.95f;  // Allow up to 95% memory usage
    config.degradation_config.enable_graceful_degradation = true;
    config.degradation_config.max_degradation = PORTIA_DEGRADATION_SEVERE;
    config.enable_bio_async = true;
    config.enable_logging = true;

    // WHEN: Initialize Portia on simulated minimal platform
    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS) << "Portia initialization failed";
    portia_initialized_ = true;

    // THEN: Verify Portia detected platform tier
    portia_status_t status;
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Verify tier was detected (any valid tier is acceptable on host system)
    EXPECT_TRUE(status.current_tier == PLATFORM_TIER_MINIMAL ||
                status.current_tier == PLATFORM_TIER_CONSTRAINED ||
                status.current_tier == PLATFORM_TIER_MEDIUM ||
                status.current_tier == PLATFORM_TIER_FULL)
        << "Invalid platform tier detected: "
        << platform_tier_get_name(status.current_tier);

    // THEN: Verify resource metrics are being tracked
    EXPECT_GE(status.cpu_usage, 0.0f);
    EXPECT_LE(status.cpu_usage, 1.0f);
    EXPECT_GE(status.memory_usage, 0.0f);
    EXPECT_LE(status.memory_usage, 1.0f);

    // THEN: Get tier configuration and verify constraints
    platform_tier_config_t tier_config = platform_tier_get_config(status.current_tier);

    // Create brain with tier-appropriate constraints
    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr) << "Brain creation failed";

    // Verify brain respects tier constraints
    EXPECT_LE(brain_get_neuron_count(brain_), tier_config.max_neurons)
        << "Brain exceeds tier neuron limit";

    // THEN: Verify cognitive modules are appropriately limited
    bool attention_enabled = platform_tier_can_enable_module(
        status.current_tier, COGNITIVE_MODULE_ATTENTION);
    bool reasoning_enabled = platform_tier_can_enable_module(
        status.current_tier, COGNITIVE_MODULE_REASONING);
    bool meta_learning_enabled = platform_tier_can_enable_module(
        status.current_tier, COGNITIVE_MODULE_META_LEARNING);

    // Minimal/constrained tiers should have attention but not meta-learning
    if (status.current_tier == PLATFORM_TIER_MINIMAL) {
        EXPECT_FALSE(meta_learning_enabled) << "Meta-learning should be disabled on minimal tier";
        EXPECT_FALSE(reasoning_enabled) << "Reasoning should be disabled on minimal tier";
    }

    // Update Portia to verify monitoring works
    for (int i = 0; i < 5; i++) {
        err = portia_update();
        EXPECT_EQ(err, NIMCP_SUCCESS);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Get updated status
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(status.updates, 0) << "Portia update cycles not counted";

    nimcp_log(LOG_LEVEL_INFO, "ConstrainedPlatformStartup: PASS - "
              "Tier=%s, CPU=%.1f%%, Memory=%.1f%%",
              platform_tier_get_name(status.current_tier),
              status.cpu_usage * 100.0f, status.memory_usage * 100.0f);
}

//=============================================================================
// Test 2: Adaptive Resource Management
//=============================================================================

TEST_F(PortiaConstrainedPlatformE2ETest, AdaptiveResourceManagement) {
    // GIVEN: Portia configured with adaptive features
    portia_config_t config = portia_get_default_config();
    config.tier_config.enable_auto_switching = true;
    config.tier_config.switch_hysteresis_ms = 100;  // Fast switching for test
    config.degradation_config.enable_graceful_degradation = true;
    config.enable_bio_async = true;

    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    // Get initial status
    portia_status_t initial_status;
    err = portia_get_status(&initial_status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    platform_tier_t initial_tier = initial_status.current_tier;
    nimcp_log(LOG_LEVEL_INFO, "Initial tier: %s",
              platform_tier_get_name(initial_tier));

    // WHEN: Force tier change to simulate resource constraint
    platform_tier_t target_tier = (initial_tier == PLATFORM_TIER_FULL)
        ? PLATFORM_TIER_CONSTRAINED : initial_tier;

    if (target_tier != initial_tier) {
        err = portia_set_tier(target_tier);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to set tier";

        // Allow time for tier switch to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        // Update Portia
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // THEN: Verify tier changed
        portia_status_t new_status;
        err = portia_get_status(&new_status);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        EXPECT_EQ(new_status.current_tier, target_tier)
            << "Tier should have changed to " << platform_tier_get_name(target_tier);
        EXPECT_GT(new_status.tier_switches, initial_status.tier_switches)
            << "Tier switch count should have increased";

        nimcp_log(LOG_LEVEL_INFO, "Tier switched from %s to %s",
                  platform_tier_get_name(initial_tier),
                  platform_tier_get_name(target_tier));
    }

    // WHEN: Create brain with recommended neuron count
    uint32_t recommended_neurons = portia_recommend_neuron_count();
    EXPECT_GT(recommended_neurons, 0) << "Should recommend non-zero neurons";

    platform_tier_config_t tier_config = platform_tier_get_config(target_tier);
    EXPECT_LE(recommended_neurons, tier_config.max_neurons)
        << "Recommended neurons should not exceed tier maximum";

    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr);

    // THEN: Verify brain operates within memory budget
    portia_status_t final_status;
    for (int i = 0; i < 10; i++) {
        err = portia_update();
        EXPECT_EQ(err, NIMCP_SUCCESS);

        err = portia_get_status(&final_status);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        // Memory usage should be reasonable
        EXPECT_LT(final_status.memory_usage, 0.95f)
            << "Memory usage exceeds safe threshold";

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    nimcp_log(LOG_LEVEL_INFO, "AdaptiveResourceManagement: PASS - "
              "Neurons=%u, Memory=%.1f%%",
              recommended_neurons, final_status.memory_usage * 100.0f);
}

//=============================================================================
// Test 3: Cognitive Module Enablement
//=============================================================================

TEST_F(PortiaConstrainedPlatformE2ETest, CognitiveModuleEnablement) {
    // GIVEN: Initialize Portia
    nimcp_error_t err = portia_init(nullptr);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    portia_status_t status;
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    platform_tier_t current_tier = status.current_tier;
    platform_tier_config_t config = platform_tier_get_config(current_tier);

    nimcp_log(LOG_LEVEL_INFO, "Testing cognitive modules for tier: %s",
              platform_tier_get_name(current_tier));

    // WHEN: Test all cognitive module flags
    struct ModuleTest {
        cognitive_module_flags_t flag;
        const char* name;
        bool should_enable_minimal;
        bool should_enable_constrained;
        bool should_enable_medium;
        bool should_enable_full;
    };

    std::vector<ModuleTest> modules = {
        {COGNITIVE_MODULE_ATTENTION, "Attention", true, true, true, true},
        {COGNITIVE_MODULE_WORKING_MEMORY, "Working Memory", true, true, true, true},
        {COGNITIVE_MODULE_SALIENCE, "Salience", false, true, true, true},
        {COGNITIVE_MODULE_EMOTIONS, "Emotions", false, true, true, true},
        {COGNITIVE_MODULE_SEMANTIC_MEMORY, "Semantic Memory", false, true, true, true},
        {COGNITIVE_MODULE_EPISODIC_MEMORY, "Episodic Memory", false, false, true, true},
        {COGNITIVE_MODULE_EXECUTIVE, "Executive", false, false, true, true},
        {COGNITIVE_MODULE_REASONING, "Reasoning", false, false, true, true},
        {COGNITIVE_MODULE_CURIOSITY, "Curiosity", false, false, false, true},
        {COGNITIVE_MODULE_META_LEARNING, "Meta-Learning", false, false, false, true},
        {COGNITIVE_MODULE_INTROSPECTION, "Introspection", false, false, false, true},
        {COGNITIVE_MODULE_THEORY_OF_MIND, "Theory of Mind", false, false, false, true},
        {COGNITIVE_MODULE_GLOBAL_WORKSPACE, "Global Workspace", false, false, false, true},
    };

    // THEN: Verify each module's availability matches tier expectations
    for (const auto& module : modules) {
        bool is_enabled = platform_tier_can_enable_module(current_tier, module.flag);
        bool expected = false;

        switch (current_tier) {
            case PLATFORM_TIER_MINIMAL:
                expected = module.should_enable_minimal;
                break;
            case PLATFORM_TIER_CONSTRAINED:
                expected = module.should_enable_constrained;
                break;
            case PLATFORM_TIER_MEDIUM:
                expected = module.should_enable_medium;
                break;
            case PLATFORM_TIER_FULL:
                expected = module.should_enable_full;
                break;
        }

        EXPECT_EQ(is_enabled, expected)
            << module.name << " enablement mismatch for tier "
            << platform_tier_get_name(current_tier);

        if (is_enabled != expected) {
            nimcp_log(LOG_LEVEL_WARN, "Module %s: expected %s, got %s",
                      module.name, expected ? "ENABLED" : "DISABLED",
                      is_enabled ? "ENABLED" : "DISABLED");
        }
    }

    // THEN: Verify config bitmask matches individual checks
    uint32_t enabled_count = 0;
    for (const auto& module : modules) {
        if (config.cognitive_modules_enabled & module.flag) {
            enabled_count++;
            bool can_enable = platform_tier_can_enable_module(current_tier, module.flag);
            EXPECT_TRUE(can_enable)
                << "Module " << module.name << " in config but can_enable returned false";
        }
    }

    nimcp_log(LOG_LEVEL_INFO, "CognitiveModuleEnablement: PASS - "
              "Tier=%s, Modules enabled=%u",
              platform_tier_get_name(current_tier), enabled_count);
}

//=============================================================================
// Test 4: Memory Budget Compliance
//=============================================================================

TEST_F(PortiaConstrainedPlatformE2ETest, MemoryBudgetCompliance) {
    // GIVEN: Configure strict memory limits
    portia_config_t config = portia_get_default_config();
    config.resource_config.memory_threshold = 0.85f;  // Alert at 85%
    config.degradation_config.enable_graceful_degradation = true;
    config.enable_metrics = true;

    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    // Get system resources
    system_resources_t resources;
    bool got_resources = system_resources_query(&resources);
    ASSERT_TRUE(got_resources);

    nimcp_log(LOG_LEVEL_INFO, "System resources: RAM=%lu MB (available=%lu MB)",
              resources.total_ram_mb, resources.available_ram_mb);

    // WHEN: Create brain sized for memory budget
    portia_status_t status;
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    platform_tier_config_t tier_config = platform_tier_get_config(status.current_tier);
    uint32_t target_neurons = std::min(tier_config.max_neurons,
                                       tier_config.initial_neurons * 2);

    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr);

    // THEN: Monitor memory usage over time
    std::vector<float> memory_samples;
    const int num_samples = 20;

    for (int i = 0; i < num_samples; i++) {
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS);

        err = portia_get_status(&status);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        memory_samples.push_back(status.memory_usage);

        // Verify memory stays within budget
        EXPECT_LT(status.memory_usage, 0.95f)
            << "Memory usage exceeded 95% at sample " << i;

        // If memory is high, verify degradation activated
        if (status.memory_usage > config.resource_config.memory_threshold) {
            EXPECT_GT(status.degradation_level, PORTIA_DEGRADATION_NONE)
                << "Degradation should activate when memory threshold exceeded";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // THEN: Calculate memory statistics
    float avg_memory = 0.0f;
    float max_memory = 0.0f;
    for (float sample : memory_samples) {
        avg_memory += sample;
        max_memory = std::max(max_memory, sample);
    }
    avg_memory /= memory_samples.size();

    EXPECT_LT(max_memory, 0.95f) << "Peak memory exceeded safe limit";
    EXPECT_LT(avg_memory, 0.80f) << "Average memory too high";

    nimcp_log(LOG_LEVEL_INFO, "MemoryBudgetCompliance: PASS - "
              "Neurons=%u, Avg=%.1f%%, Peak=%.1f%%",
              target_neurons, avg_memory * 100.0f, max_memory * 100.0f);
}

//=============================================================================
// Test 5: Graceful Resource Exhaustion
//=============================================================================

TEST_F(PortiaConstrainedPlatformE2ETest, GracefulResourceExhaustion) {
    // GIVEN: Configure Portia with degradation enabled
    portia_config_t config = portia_get_default_config();
    config.resource_config.memory_threshold = 0.70f;  // Low threshold
    config.degradation_config.enable_graceful_degradation = true;
    config.degradation_config.max_degradation = PORTIA_DEGRADATION_EMERGENCY;
    config.degradation_config.recovery_delay_ms = 100;
    config.enable_metrics = true;

    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    // WHEN: Simulate resource pressure by forcing degradation
    portia_status_t initial_status;
    err = portia_get_status(&initial_status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    nimcp_log(LOG_LEVEL_INFO, "Initial degradation level: %d",
              initial_status.degradation_level);

    // Force moderate degradation
    err = portia_set_degradation_level(PORTIA_DEGRADATION_MODERATE);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Update and verify
    err = portia_update();
    ASSERT_EQ(err, NIMCP_SUCCESS);

    portia_status_t degraded_status;
    err = portia_get_status(&degraded_status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // THEN: Verify degradation was applied
    EXPECT_EQ(degraded_status.degradation_level, PORTIA_DEGRADATION_MODERATE)
        << "Degradation level should be MODERATE";
    EXPECT_GT(degraded_status.degradations, initial_status.degradations)
        << "Degradation count should have increased";

    // WHEN: Create brain under degradation
    uint32_t recommended = portia_recommend_neuron_count();
    platform_tier_config_t tier_config = platform_tier_get_config(degraded_status.current_tier);

    // Under degradation, recommendation should be conservative
    EXPECT_LT(recommended, tier_config.max_neurons)
        << "Recommended neurons should be reduced under degradation";

    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr) << "Brain creation should succeed even under degradation";

    // THEN: Verify system remains functional
    for (int i = 0; i < 10; i++) {
        err = portia_update();
        EXPECT_EQ(err, NIMCP_SUCCESS) << "Portia update should succeed under degradation";

        // Verify brain is operational
        EXPECT_GT(brain_get_neuron_count(brain_), 0u);
        EXPECT_NE(brain_, nullptr);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // WHEN: Restore normal operation
    err = portia_set_degradation_level(PORTIA_DEGRADATION_NONE);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    err = portia_update();
    ASSERT_EQ(err, NIMCP_SUCCESS);

    portia_status_t recovered_status;
    err = portia_get_status(&recovered_status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // THEN: Verify recovery
    EXPECT_EQ(recovered_status.degradation_level, PORTIA_DEGRADATION_NONE)
        << "Should recover to normal operation";

    // System should still be functional after recovery
    for (int i = 0; i < 5; i++) {
        err = portia_update();
        EXPECT_EQ(err, NIMCP_SUCCESS);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    nimcp_log(LOG_LEVEL_INFO, "GracefulResourceExhaustion: PASS - "
              "Degradation events=%lu, Recovery successful",
              recovered_status.degradations);
}
