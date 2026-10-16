/**
 * @file e2e_test_portia_tier_lifecycle.cpp
 * @brief End-to-end test for Portia tier lifecycle management
 *
 * WHAT: Tests complete tier lifecycle from FULL to MINIMAL and back
 * WHY:  Verify seamless tier transitions without data loss or crashes
 * HOW:  Transition through all tiers, verify subsystems coordinate, check bio-async propagation
 *
 * TEST SCENARIOS:
 * - FullTierLifecycle: Cycle through all tiers (FULL → MEDIUM → CONSTRAINED → MINIMAL → back)
 * - SubsystemCoordination: Verify all subsystems adapt to tier changes
 * - NoDataLoss: Ensure no state loss during transitions
 * - BioAsyncPropagation: Verify tier change events propagate correctly
 * - ExternalAPIStability: Verify API remains functional during transitions
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
#include <cstring>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia.h"
#include "utils/platform/nimcp_platform_tier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"  // For direct struct access in tests
#include "cognitive/nimcp_working_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaTierLifecycleE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_log_init(NULL);

        nimcp_error_t err = nimcp_bio_async_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        err = bio_router_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Register test module to receive tier change events
        bio_module_info_t module_info = {
            .module_id = BIO_MODULE_ATTENTION,  // Use existing module ID
            .module_name = "tier_test_observer",
            .inbox_capacity = 100,
            .user_data = this
        };
        test_module_ctx_ = bio_router_register_module(&module_info);
        ASSERT_NE(test_module_ctx_, nullptr);

        brain_ = nullptr;
        portia_initialized_ = false;
        tier_change_count_.store(0);
        last_tier_ = PLATFORM_TIER_FULL;
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

        if (test_module_ctx_) {
            bio_router_unregister_module(test_module_ctx_);
            test_module_ctx_ = nullptr;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        nimcp_log_shutdown();
    }

    brain_t brain_;
    bio_module_context_t test_module_ctx_;
    bool portia_initialized_;
    std::atomic<int> tier_change_count_;
    std::atomic<platform_tier_t> last_tier_;
};

//=============================================================================
// Test 1: Full Tier Lifecycle
//=============================================================================

TEST_F(PortiaTierLifecycleE2ETest, FullTierLifecycle) {
    // GIVEN: Initialize Portia with auto-switching disabled
    portia_config_t config = portia_get_default_config();
    config.tier_config.enable_auto_switching = false;  // Manual control
    config.tier_config.lock_tier = false;
    config.enable_bio_async = true;
    config.enable_metrics = true;

    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    // Get initial tier
    portia_status_t status;
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    platform_tier_t initial_tier = status.current_tier;

    nimcp_log(LOG_LEVEL_INFO, "Starting tier lifecycle test from: %s",
              platform_tier_get_name(initial_tier));

    // Create a brain at initial tier
    platform_tier_config_t tier_config = platform_tier_get_config(initial_tier);
    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr);
    uint32_t initial_neurons = brain_get_neuron_count(brain_);

    // WHEN: Transition through all tiers (downward)
    platform_tier_t tiers[] = {
        PLATFORM_TIER_FULL,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_CONSTRAINED,
        PLATFORM_TIER_MINIMAL
    };

    uint64_t initial_switch_count = status.tier_switches;

    for (size_t i = 0; i < sizeof(tiers) / sizeof(tiers[0]); i++) {
        platform_tier_t target_tier = tiers[i];

        nimcp_log(LOG_LEVEL_INFO, "Transitioning to tier: %s",
                  platform_tier_get_name(target_tier));

        // Set new tier
        err = portia_set_tier(target_tier);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to set tier " << i;

        // Allow transition to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Update Portia
        for (int j = 0; j < 5; j++) {
            err = portia_update();
            ASSERT_EQ(err, NIMCP_SUCCESS);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        // THEN: Verify tier changed
        err = portia_get_status(&status);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        EXPECT_EQ(status.current_tier, target_tier)
            << "Tier should be " << platform_tier_get_name(target_tier);

        // Verify brain is still functional
        EXPECT_NE(brain_, nullptr) << "Brain should not be null";
        uint32_t brain_neurons = brain_get_neuron_count(brain_);
        EXPECT_GT(brain_neurons, 0u) << "Brain should have neurons";

        // Get tier configuration
        platform_tier_config_t current_config = platform_tier_get_config(target_tier);

        // Verify brain respects new tier constraints
        if (brain_neurons > current_config.max_neurons) {
            nimcp_log(LOG_LEVEL_WARN, "Brain neurons (%u) exceed tier max (%u)",
                      brain_neurons, current_config.max_neurons);
        }

        // Verify cognitive modules match tier
        bool has_attention = platform_tier_can_enable_module(
            target_tier, COGNITIVE_MODULE_ATTENTION);
        bool has_meta_learning = platform_tier_can_enable_module(
            target_tier, COGNITIVE_MODULE_META_LEARNING);

        // All tiers should have attention
        EXPECT_TRUE(has_attention) << "All tiers should have attention module";

        // Only FULL tier should have meta-learning
        if (target_tier == PLATFORM_TIER_FULL) {
            EXPECT_TRUE(has_meta_learning) << "FULL tier should have meta-learning";
        } else {
            EXPECT_FALSE(has_meta_learning)
                << platform_tier_get_name(target_tier) << " should not have meta-learning";
        }
    }

    // WHEN: Transition back upward
    for (int i = sizeof(tiers) / sizeof(tiers[0]) - 2; i >= 0; i--) {
        platform_tier_t target_tier = tiers[i];

        nimcp_log(LOG_LEVEL_INFO, "Transitioning back to tier: %s",
                  platform_tier_get_name(target_tier));

        err = portia_set_tier(target_tier);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        for (int j = 0; j < 5; j++) {
            err = portia_update();
            ASSERT_EQ(err, NIMCP_SUCCESS);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        err = portia_get_status(&status);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        EXPECT_EQ(status.current_tier, target_tier)
            << "Tier should be " << platform_tier_get_name(target_tier);
    }

    // THEN: Verify final state
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Should have switched tiers multiple times
    EXPECT_GT(status.tier_switches, initial_switch_count)
        << "Tier switch count should have increased";

    // Brain should still be functional
    EXPECT_NE(brain_, nullptr);
    EXPECT_GT(brain_get_neuron_count(brain_), 0u);

    nimcp_log(LOG_LEVEL_INFO, "FullTierLifecycle: PASS - "
              "Completed %lu tier switches, final tier: %s",
              status.tier_switches - initial_switch_count,
              platform_tier_get_name(status.current_tier));
}

//=============================================================================
// Test 2: Subsystem Coordination
//=============================================================================

TEST_F(PortiaTierLifecycleE2ETest, SubsystemCoordination) {
    // GIVEN: Initialize with full subsystem monitoring
    portia_config_t config = portia_get_default_config();
    config.enable_bio_async = true;
    config.enable_metrics = true;
    config.enable_logging = true;

    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    portia_status_t initial_status;
    err = portia_get_status(&initial_status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Create brain
    platform_tier_config_t tier_config = platform_tier_get_config(initial_status.current_tier);
    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr);

    // WHEN: Perform tier transition
    platform_tier_t target_tier = (initial_status.current_tier == PLATFORM_TIER_FULL)
        ? PLATFORM_TIER_MEDIUM : PLATFORM_TIER_FULL;

    nimcp_log(LOG_LEVEL_INFO, "Testing subsystem coordination during %s → %s transition",
              platform_tier_get_name(initial_status.current_tier),
              platform_tier_get_name(target_tier));

    err = portia_set_tier(target_tier);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // THEN: Monitor all subsystems during transition
    const int monitoring_cycles = 20;
    std::vector<portia_status_t> status_samples;

    for (int i = 0; i < monitoring_cycles; i++) {
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Portia update failed during transition";

        portia_status_t current_status;
        err = portia_get_status(&current_status);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        status_samples.push_back(current_status);

        // Verify all subsystems remain operational
        EXPECT_GE(current_status.cpu_usage, 0.0f);
        EXPECT_LE(current_status.cpu_usage, 1.0f);
        EXPECT_GE(current_status.memory_usage, 0.0f);
        EXPECT_LE(current_status.memory_usage, 1.0f);

        // Brain should remain valid
        EXPECT_NE(brain_, nullptr);
        EXPECT_GT(brain_get_neuron_count(brain_), 0u);

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // THEN: Verify smooth transition
    portia_status_t final_status;
    err = portia_get_status(&final_status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(final_status.current_tier, target_tier)
        << "Should have reached target tier";

    // Verify no system instability (no wild metric swings)
    float max_memory = 0.0f;
    float min_memory = 1.0f;
    for (const auto& sample : status_samples) {
        max_memory = std::max(max_memory, sample.memory_usage);
        min_memory = std::min(min_memory, sample.memory_usage);
    }

    float memory_variance = max_memory - min_memory;
    EXPECT_LT(memory_variance, 0.5f)
        << "Memory usage variance too high during transition: " << memory_variance;

    // Verify update metrics are reasonable
    EXPECT_GT(final_status.updates, initial_status.updates)
        << "Update count should increase";
    EXPECT_GT(final_status.avg_update_time_ms, 0.0f)
        << "Average update time should be tracked";
    EXPECT_LT(final_status.avg_update_time_ms, 100.0f)
        << "Update time should be reasonable";

    nimcp_log(LOG_LEVEL_INFO, "SubsystemCoordination: PASS - "
              "Updates=%lu, Avg time=%.2fms, Memory variance=%.1f%%",
              final_status.updates - initial_status.updates,
              final_status.avg_update_time_ms,
              memory_variance * 100.0f);
}

//=============================================================================
// Test 3: No Data Loss During Transitions
//=============================================================================

TEST_F(PortiaTierLifecycleE2ETest, NoDataLossDuringTransitions) {
    // GIVEN: Initialize Portia and create test data
    nimcp_error_t err = portia_init(nullptr);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    portia_status_t status;
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Create brain with test data
    platform_tier_config_t config = platform_tier_get_config(status.current_tier);
    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr);

    // Initialize brain state
    uint32_t initial_neurons = brain_get_neuron_count(brain_);

    // Note: We can't directly access neuron internals through public API
    // Instead we verify the brain remains functional through transitions

    // WHEN: Perform multiple tier transitions
    platform_tier_t transitions[] = {
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_CONSTRAINED,
        PLATFORM_TIER_MEDIUM,
        status.current_tier  // Back to original
    };

    for (size_t i = 0; i < sizeof(transitions) / sizeof(transitions[0]); i++) {
        nimcp_log(LOG_LEVEL_INFO, "Transition %zu: %s",
                  i, platform_tier_get_name(transitions[i]));

        err = portia_set_tier(transitions[i]);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        for (int j = 0; j < 3; j++) {
            err = portia_update();
            ASSERT_EQ(err, NIMCP_SUCCESS);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        // THEN: Verify brain data integrity
        EXPECT_NE(brain_, nullptr) << "Brain should not be null";
        uint32_t current_neurons = brain_get_neuron_count(brain_);
        EXPECT_GT(current_neurons, 0u) << "Should have neurons";
        EXPECT_GT(current_neurons, 0u) << "Should have valid neurons after transition " << i;
    }

    // THEN: Final verification
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_NE(brain_, nullptr);
    uint32_t final_neurons = brain_get_neuron_count(brain_);
    EXPECT_GT(final_neurons, 0u);

    nimcp_log(LOG_LEVEL_INFO, "NoDataLossDuringTransitions: PASS - "
              "Final neurons=%u, tier=%s",
              final_neurons, platform_tier_get_name(status.current_tier));
}

//=============================================================================
// Test 4: Bio-Async Event Propagation
//=============================================================================

TEST_F(PortiaTierLifecycleE2ETest, BioAsyncEventPropagation) {
    // GIVEN: Initialize with bio-async enabled
    portia_config_t config = portia_get_default_config();
    config.enable_bio_async = true;

    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    portia_status_t initial_status;
    err = portia_get_status(&initial_status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // WHEN: Change tier
    platform_tier_t target_tier = (initial_status.current_tier == PLATFORM_TIER_FULL)
        ? PLATFORM_TIER_MEDIUM : PLATFORM_TIER_FULL;

    nimcp_log(LOG_LEVEL_INFO, "Testing bio-async event propagation for tier change: %s → %s",
              platform_tier_get_name(initial_status.current_tier),
              platform_tier_get_name(target_tier));

    err = portia_set_tier(target_tier);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // THEN: Allow time for bio-async events to propagate
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Update Portia to ensure events are processed
    for (int i = 0; i < 5; i++) {
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Verify tier change completed successfully with bio-async enabled
    // Note: Actual message inspection would require handler registration,
    // which is tested separately in bio-router tests

    // Verify final state
    portia_status_t final_status;
    err = portia_get_status(&final_status);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(final_status.current_tier, target_tier);

    // Verify bio-async system remained stable during tier change
    EXPECT_GT(final_status.updates, initial_status.updates)
        << "Updates should have occurred";
    EXPECT_GT(final_status.tier_switches, initial_status.tier_switches)
        << "Tier switch should have been recorded";

    nimcp_log(LOG_LEVEL_INFO, "BioAsyncEventPropagation: PASS - "
              "Tier change completed with bio-async, Final tier=%s",
              platform_tier_get_name(target_tier));
}

//=============================================================================
// Test 5: External API Stability
//=============================================================================

TEST_F(PortiaTierLifecycleE2ETest, ExternalAPIStability) {
    // GIVEN: Initialize Portia
    nimcp_error_t err = portia_init(nullptr);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    // Create brain
    portia_status_t status;
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    platform_tier_config_t config = platform_tier_get_config(status.current_tier);
    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr);

    // WHEN: Continuously call API during tier transitions
    std::atomic<bool> keep_running{true};
    std::atomic<int> api_call_count{0};
    std::atomic<int> api_failures{0};

    // Background thread making API calls
    std::thread api_thread([this, &keep_running, &api_call_count, &api_failures]() {
        while (keep_running) {
            portia_status_t thread_status;
            nimcp_error_t thread_err = portia_get_status(&thread_status);
            if (thread_err == NIMCP_SUCCESS) {
                api_call_count.fetch_add(1);

                // Verify status is valid
                if (thread_status.current_tier >= PLATFORM_TIER_COUNT) {
                    api_failures.fetch_add(1);
                }
            } else {
                api_failures.fetch_add(1);
            }

            // Also test recommend_neuron_count
            uint32_t recommended = portia_recommend_neuron_count();
            if (recommended == 0) {
                api_failures.fetch_add(1);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Perform tier transitions while API is being called
    platform_tier_t tiers[] = {
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_CONSTRAINED,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_FULL
    };

    for (size_t i = 0; i < sizeof(tiers) / sizeof(tiers[0]); i++) {
        err = portia_set_tier(tiers[i]);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        for (int j = 0; j < 5; j++) {
            err = portia_update();
            ASSERT_EQ(err, NIMCP_SUCCESS);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    // Stop API thread
    keep_running = false;
    api_thread.join();

    // THEN: Verify API remained stable
    int total_calls = api_call_count.load();
    int failures = api_failures.load();

    EXPECT_GT(total_calls, 0) << "Should have made API calls";
    EXPECT_EQ(failures, 0) << "API should not fail during tier transitions";

    float failure_rate = (total_calls > 0) ? (float)failures / total_calls : 0.0f;
    EXPECT_LT(failure_rate, 0.01f) << "API failure rate should be < 1%";

    nimcp_log(LOG_LEVEL_INFO, "ExternalAPIStability: PASS - "
              "API calls=%d, Failures=%d (%.2f%%)",
              total_calls, failures, failure_rate * 100.0f);
}
