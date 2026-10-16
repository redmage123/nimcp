/**
 * @file test_security_integration_framework.cpp
 * @brief Integration tests for Security Integration Framework
 *
 * Phase SC-4: Universal Security Integration Framework
 *
 * Tests cross-component integration scenarios:
 * 1. Multi-module security workflows
 * 2. Trust propagation across module categories
 * 3. Concurrent module operations
 * 4. Full system security pipeline
 * 5. Event-driven security responses
 * 6. Integration with existing security subsystems
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "security/nimcp_security_integration.h"
#include "security/nimcp_security_math.h"

#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <random>
#include <map>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityIntegrationFrameworkIntegrationTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        ctx = nimcp_sec_integration_create();
        ASSERT_NE(ctx, nullptr);

        nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
        config.enable_continuous_monitoring = false;
        config.enable_self_monitoring = true;
        config.trust_threshold = 0.5;
        config.privacy_budget = 50.0;  // Higher budget for integration tests
        ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);
    }

    void TearDown() override
    {
        if (ctx) {
            nimcp_sec_integration_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Helper to simulate a module's lifecycle
    struct SimulatedModule {
        uint32_t id;
        std::string name;
        nimcp_sec_module_category_t category;
        std::vector<uint32_t> region_ids;
        std::vector<std::vector<uint8_t>> region_data;
    };

    SimulatedModule create_simulated_module(
        const std::string& name,
        nimcp_sec_module_category_t category,
        int num_regions = 1)
    {
        SimulatedModule mod;
        mod.name = name;
        mod.category = category;

        nimcp_sec_register_module(ctx, name.c_str(), category, &mod.id);

        for (int i = 0; i < num_regions; i++) {
            std::vector<uint8_t> data(512 + i * 128, static_cast<uint8_t>(0x40 + i));
            uint32_t region_id;
            std::string region_name = name + "_region_" + std::to_string(i);
            nimcp_sec_register_region(ctx, mod.id, region_name.c_str(),
                                     data.data(), data.size(), &region_id);
            mod.region_ids.push_back(region_id);
            mod.region_data.push_back(std::move(data));
        }

        return mod;
    }

    nimcp_sec_integration_t* ctx = nullptr;
};

//=============================================================================
// Multi-Module Security Workflow Tests
//=============================================================================

/**
 * Test: Full system initialization with all module categories
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, FullSystemInitialization)
{
    // Simulate initializing a complete NIMCP system
    std::map<nimcp_sec_module_category_t, std::vector<SimulatedModule>> system_modules;

    // Core modules
    system_modules[NIMCP_SEC_CAT_CORE].push_back(create_simulated_module("brain", NIMCP_SEC_CAT_CORE, 3));
    system_modules[NIMCP_SEC_CAT_CORE].push_back(create_simulated_module("neurons", NIMCP_SEC_CAT_CORE, 2));
    system_modules[NIMCP_SEC_CAT_CORE].push_back(create_simulated_module("events", NIMCP_SEC_CAT_CORE, 1));

    // Cognitive modules
    system_modules[NIMCP_SEC_CAT_COGNITIVE].push_back(create_simulated_module("memory", NIMCP_SEC_CAT_COGNITIVE, 2));
    system_modules[NIMCP_SEC_CAT_COGNITIVE].push_back(create_simulated_module("executive", NIMCP_SEC_CAT_COGNITIVE, 1));
    system_modules[NIMCP_SEC_CAT_COGNITIVE].push_back(create_simulated_module("emotions", NIMCP_SEC_CAT_COGNITIVE, 1));

    // Middleware modules
    system_modules[NIMCP_SEC_CAT_MIDDLEWARE].push_back(create_simulated_module("pipeline", NIMCP_SEC_CAT_MIDDLEWARE, 2));
    system_modules[NIMCP_SEC_CAT_MIDDLEWARE].push_back(create_simulated_module("encoding", NIMCP_SEC_CAT_MIDDLEWARE, 1));

    // Glial modules
    system_modules[NIMCP_SEC_CAT_GLIAL].push_back(create_simulated_module("astrocytes", NIMCP_SEC_CAT_GLIAL, 1));
    system_modules[NIMCP_SEC_CAT_GLIAL].push_back(create_simulated_module("microglia", NIMCP_SEC_CAT_GLIAL, 1));

    // Utility modules
    system_modules[NIMCP_SEC_CAT_UTILITY].push_back(create_simulated_module("memory_pool", NIMCP_SEC_CAT_UTILITY, 1));
    system_modules[NIMCP_SEC_CAT_UTILITY].push_back(create_simulated_module("thread_pool", NIMCP_SEC_CAT_UTILITY, 1));

    // Verify all modules registered
    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(ctx, &stats), NIMCP_SUCCESS);

    int expected_modules = 1;  // Self-monitoring module
    int expected_regions = 0;
    for (auto& [cat, mods] : system_modules) {
        expected_modules += mods.size();
        for (auto& mod : mods) {
            expected_regions += mod.region_ids.size();
        }
    }

    EXPECT_EQ(stats.registered_modules, static_cast<uint32_t>(expected_modules));
    EXPECT_EQ(stats.monitored_regions, static_cast<uint32_t>(expected_regions));
}

/**
 * Test: Cross-category trust propagation
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, CrossCategoryTrustPropagation)
{
    // Create modules from different categories
    auto core_brain = create_simulated_module("brain", NIMCP_SEC_CAT_CORE);
    auto cog_memory = create_simulated_module("memory", NIMCP_SEC_CAT_COGNITIVE);
    auto mid_pipeline = create_simulated_module("pipeline", NIMCP_SEC_CAT_MIDDLEWARE);
    auto glial_astro = create_simulated_module("astrocytes", NIMCP_SEC_CAT_GLIAL);

    // Build trust for brain module (highly trusted core)
    for (int i = 0; i < 20; i++) {
        nimcp_sec_record_interaction(ctx, core_brain.id, true, 1.0);
    }

    // Brain vouches for memory and pipeline
    ASSERT_EQ(nimcp_sec_add_trust_voucher(ctx, core_brain.id, cog_memory.id), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_sec_add_trust_voucher(ctx, core_brain.id, mid_pipeline.id), NIMCP_SUCCESS);

    // Memory vouches for astrocytes
    ASSERT_EQ(nimcp_sec_add_trust_voucher(ctx, cog_memory.id, glial_astro.id), NIMCP_SUCCESS);

    // Propagate trust
    ASSERT_EQ(nimcp_sec_propagate_trust(ctx), NIMCP_SUCCESS);

    // Verify trust hierarchy
    nimcp_trust_score_t brain_score, memory_score, pipeline_score, astro_score;
    nimcp_sec_get_trust_score(ctx, core_brain.id, &brain_score);
    nimcp_sec_get_trust_score(ctx, cog_memory.id, &memory_score);
    nimcp_sec_get_trust_score(ctx, mid_pipeline.id, &pipeline_score);
    nimcp_sec_get_trust_score(ctx, glial_astro.id, &astro_score);

    // Brain should have highest trust
    EXPECT_GT(brain_score.expected_trust, memory_score.expected_trust);
    EXPECT_GT(brain_score.expected_trust, pipeline_score.expected_trust);

    // Modules vouched for by brain should have elevated trust
    EXPECT_TRUE(nimcp_sec_is_module_trusted(ctx, cog_memory.id));
    EXPECT_TRUE(nimcp_sec_is_module_trusted(ctx, mid_pipeline.id));
}

/**
 * Test: Security event cascade across modules
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, SecurityEventCascade)
{
    // Create interconnected modules
    auto mod_a = create_simulated_module("module_a", NIMCP_SEC_CAT_CORE, 2);
    auto mod_b = create_simulated_module("module_b", NIMCP_SEC_CAT_COGNITIVE, 1);
    auto mod_c = create_simulated_module("module_c", NIMCP_SEC_CAT_MIDDLEWARE, 1);

    // Clear initial events
    nimcp_sec_clear_events(ctx);

    // Build some trust
    for (int i = 0; i < 10; i++) {
        nimcp_sec_record_interaction(ctx, mod_a.id, true, 1.0);
        nimcp_sec_record_interaction(ctx, mod_b.id, true, 1.0);
        nimcp_sec_record_interaction(ctx, mod_c.id, true, 1.0);
    }

    // Simulate tampering in module_a's region
    std::vector<uint8_t> tampered_data(mod_a.region_data[0].size());
    std::mt19937 rng(42);
    for (auto& b : tampered_data) {
        b = rng() % 256;
    }

    bool is_anomaly;
    double deviation;
    nimcp_sec_check_region(ctx, mod_a.region_ids[0], tampered_data.data(),
                          tampered_data.size(), &is_anomaly, &deviation);

    // Should generate security event
    EXPECT_GT(nimcp_sec_get_event_count(ctx), 0u);

    // Module A's trust should decrease
    nimcp_trust_score_t score_a;
    nimcp_sec_get_trust_score(ctx, mod_a.id, &score_a);

    // Record more failures
    nimcp_sec_record_interaction(ctx, mod_a.id, false, 2.0);
    nimcp_sec_record_interaction(ctx, mod_a.id, false, 2.0);

    nimcp_trust_score_t score_a_after;
    nimcp_sec_get_trust_score(ctx, mod_a.id, &score_a_after);
    EXPECT_LT(score_a_after.expected_trust, score_a.expected_trust);
}

/**
 * Test: Privacy-preserving inter-module queries
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, PrivacyPreservingInterModuleQueries)
{
    // Create API module that needs to query other modules
    auto api_module = create_simulated_module("api_gateway", NIMCP_SEC_CAT_API);
    auto data_module = create_simulated_module("data_store", NIMCP_SEC_CAT_UTILITY);

    // Simulate sensitive data in data_store
    int user_count = 1500;
    double avg_score = 72.5;
    double total_value = 125000.0;

    // API module queries with differential privacy
    double noisy_count, noisy_mean, noisy_sum;

    ASSERT_EQ(nimcp_sec_private_count(ctx, api_module.id, user_count, &noisy_count), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_sec_private_mean(ctx, api_module.id, avg_score, user_count, 100.0, &noisy_mean), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_sec_private_sum(ctx, api_module.id, total_value, 1000.0, &noisy_sum), NIMCP_SUCCESS);

    // Results should be approximately correct
    EXPECT_NEAR(noisy_count, user_count, 100);
    EXPECT_NEAR(noisy_mean, avg_score, 10);
    EXPECT_NEAR(noisy_sum, total_value, 10000);

    // Budget should be tracked
    double remaining = nimcp_sec_get_privacy_budget(ctx);
    EXPECT_LT(remaining, 50.0);  // Started with 50
}

//=============================================================================
// Concurrent Operations Tests
//=============================================================================

/**
 * Test: Concurrent module lifecycle operations
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, ConcurrentModuleLifecycle)
{
    const int num_threads = 8;
    const int ops_per_thread = 20;
    std::atomic<int> successful_registrations{0};
    std::atomic<int> successful_interactions{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, ops_per_thread, &successful_registrations, &successful_interactions]() {
            std::mt19937 rng(t * 1000);

            for (int i = 0; i < ops_per_thread; i++) {
                // Register a module
                uint32_t module_id;
                std::string name = "thread_" + std::to_string(t) + "_mod_" + std::to_string(i);
                nimcp_sec_module_category_t cat = static_cast<nimcp_sec_module_category_t>(rng() % NIMCP_SEC_CAT_COUNT);

                if (nimcp_sec_register_module(ctx, name.c_str(), cat, &module_id) == NIMCP_SUCCESS) {
                    successful_registrations++;

                    // Perform some interactions
                    for (int j = 0; j < 5; j++) {
                        bool success = (rng() % 10) > 2;
                        if (nimcp_sec_record_interaction(ctx, module_id, success, 1.0) == NIMCP_SUCCESS) {
                            successful_interactions++;
                        }
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successful_registrations.load(), num_threads * ops_per_thread);
    EXPECT_EQ(successful_interactions.load(), num_threads * ops_per_thread * 5);

    // Verify stats are consistent
    nimcp_sec_integration_stats_t stats;
    nimcp_sec_get_stats(ctx, &stats);
    EXPECT_GE(stats.registered_modules, static_cast<uint32_t>(num_threads * ops_per_thread));
}

/**
 * Test: Concurrent region monitoring
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, ConcurrentRegionMonitoring)
{
    // Create modules with regions
    std::vector<SimulatedModule> modules;
    for (int i = 0; i < 10; i++) {
        modules.push_back(create_simulated_module("monitor_" + std::to_string(i), NIMCP_SEC_CAT_CORE, 2));
    }

    const int num_threads = 4;
    const int checks_per_thread = 50;
    std::atomic<int> successful_checks{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, &modules, checks_per_thread, &successful_checks]() {
            std::mt19937 rng(t * 100);

            for (int i = 0; i < checks_per_thread; i++) {
                // Pick a random module and region
                auto& mod = modules[rng() % modules.size()];
                int region_idx = rng() % mod.region_ids.size();

                bool is_anomaly;
                double deviation;
                if (nimcp_sec_check_region(ctx, mod.region_ids[region_idx],
                                          mod.region_data[region_idx].data(),
                                          mod.region_data[region_idx].size(),
                                          &is_anomaly, &deviation) == NIMCP_SUCCESS) {
                    successful_checks++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successful_checks.load(), num_threads * checks_per_thread);
}

/**
 * Test: Concurrent trust propagation
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, ConcurrentTrustOperations)
{
    // Create a network of modules
    std::vector<uint32_t> module_ids;
    for (int i = 0; i < 20; i++) {
        uint32_t id;
        nimcp_sec_register_module(ctx, ("trust_net_" + std::to_string(i)).c_str(),
                                 NIMCP_SEC_CAT_CORE, &id);
        module_ids.push_back(id);
    }

    // Add voucher relationships
    for (size_t i = 1; i < module_ids.size(); i++) {
        nimcp_sec_add_trust_voucher(ctx, module_ids[i-1], module_ids[i]);
    }

    const int num_threads = 4;
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, &module_ids, &completed]() {
            std::mt19937 rng(t);

            // Some threads record interactions
            if (t % 2 == 0) {
                for (int i = 0; i < 100; i++) {
                    uint32_t id = module_ids[rng() % module_ids.size()];
                    nimcp_sec_record_interaction(ctx, id, (rng() % 10) > 2, 1.0);
                }
            }
            // Other threads propagate trust
            else {
                for (int i = 0; i < 10; i++) {
                    nimcp_sec_propagate_trust(ctx);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }

            completed++;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), num_threads);

    // Verify trust network is consistent
    for (auto id : module_ids) {
        nimcp_trust_score_t score;
        EXPECT_EQ(nimcp_sec_get_trust_score(ctx, id, &score), NIMCP_SUCCESS);
        EXPECT_GE(score.expected_trust, 0.0);
        EXPECT_LE(score.expected_trust, 1.0);
    }
}

//=============================================================================
// Full Security Pipeline Tests
//=============================================================================

/**
 * Test: Complete security monitoring cycle
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, CompleteSecurityMonitoringCycle)
{
    // Initialize a small system
    auto brain = create_simulated_module("brain", NIMCP_SEC_CAT_CORE, 3);
    auto memory = create_simulated_module("memory", NIMCP_SEC_CAT_COGNITIVE, 2);
    auto pipeline = create_simulated_module("pipeline", NIMCP_SEC_CAT_MIDDLEWARE, 2);

    // Set up trust relationships
    nimcp_sec_add_trust_voucher(ctx, brain.id, memory.id);
    nimcp_sec_add_trust_voucher(ctx, brain.id, pipeline.id);

    // Simulate operation cycles
    const int num_cycles = 50;
    int anomalies_detected = 0;
    std::mt19937 rng(12345);

    for (int cycle = 0; cycle < num_cycles; cycle++) {
        // Simulate normal operations (90% success)
        for (auto* mod : {&brain, &memory, &pipeline}) {
            bool success = (rng() % 100) < 90;
            nimcp_sec_record_interaction(ctx, mod->id, success, 1.0);

            // Check regions
            for (size_t r = 0; r < mod->region_ids.size(); r++) {
                bool is_anomaly;
                double deviation;
                nimcp_sec_check_region(ctx, mod->region_ids[r],
                                      mod->region_data[r].data(),
                                      mod->region_data[r].size(),
                                      &is_anomaly, &deviation);
                if (is_anomaly) anomalies_detected++;
            }
        }

        // Propagate trust periodically
        if (cycle % 10 == 0) {
            nimcp_sec_propagate_trust(ctx);
        }
    }

    // Self-check
    bool self_check_passed;
    ASSERT_EQ(nimcp_sec_self_check(ctx, &self_check_passed), NIMCP_SUCCESS);
    EXPECT_TRUE(self_check_passed);

    // All modules should still be trusted
    EXPECT_TRUE(nimcp_sec_is_module_trusted(ctx, brain.id));
    EXPECT_TRUE(nimcp_sec_is_module_trusted(ctx, memory.id));
    EXPECT_TRUE(nimcp_sec_is_module_trusted(ctx, pipeline.id));

    // Stats should reflect operations
    nimcp_sec_integration_stats_t stats;
    nimcp_sec_get_stats(ctx, &stats);
    EXPECT_GT(stats.total_integrity_checks, 0u);
}

/**
 * Test: Attack detection and trust degradation
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, AttackDetectionAndTrustDegradation)
{
    // Create modules
    auto trusted_module = create_simulated_module("trusted", NIMCP_SEC_CAT_CORE, 1);
    auto malicious_module = create_simulated_module("malicious", NIMCP_SEC_CAT_UTILITY, 1);

    // Build initial trust for both
    for (int i = 0; i < 15; i++) {
        nimcp_sec_record_interaction(ctx, trusted_module.id, true, 1.0);
        nimcp_sec_record_interaction(ctx, malicious_module.id, true, 1.0);
    }

    nimcp_trust_score_t initial_trusted, initial_malicious;
    nimcp_sec_get_trust_score(ctx, trusted_module.id, &initial_trusted);
    nimcp_sec_get_trust_score(ctx, malicious_module.id, &initial_malicious);

    // Clear events
    nimcp_sec_clear_events(ctx);

    // Simulate attack: malicious module tampers with its data
    std::vector<uint8_t> tampered(malicious_module.region_data[0].size());
    std::mt19937 rng(99999);
    for (auto& b : tampered) b = rng() % 256;

    // Multiple tampering attempts
    for (int i = 0; i < 5; i++) {
        bool is_anomaly;
        double deviation;
        nimcp_sec_check_region(ctx, malicious_module.region_ids[0],
                              tampered.data(), tampered.size(),
                              &is_anomaly, &deviation);

        // Record failure for malicious module
        nimcp_sec_record_interaction(ctx, malicious_module.id, false, 2.0);
    }

    // Check final trust scores
    nimcp_trust_score_t final_trusted, final_malicious;
    nimcp_sec_get_trust_score(ctx, trusted_module.id, &final_trusted);
    nimcp_sec_get_trust_score(ctx, malicious_module.id, &final_malicious);

    // Trusted module should maintain trust
    EXPECT_NEAR(final_trusted.expected_trust, initial_trusted.expected_trust, 0.1);

    // Malicious module should have degraded trust
    EXPECT_LT(final_malicious.expected_trust, initial_malicious.expected_trust);

    // Should have generated security events
    EXPECT_GT(nimcp_sec_get_event_count(ctx), 0u);
}

/**
 * Test: Privacy budget exhaustion handling
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, PrivacyBudgetExhaustion)
{
    auto api_mod = create_simulated_module("api", NIMCP_SEC_CAT_API);

    double initial_budget = nimcp_sec_get_privacy_budget(ctx);
    int successful_queries = 0;

    // Exhaust budget
    for (int i = 0; i < 200; i++) {
        double noisy;
        if (nimcp_sec_private_count(ctx, api_mod.id, 100, &noisy) == NIMCP_SUCCESS) {
            successful_queries++;
        }
    }

    // Some queries should have succeeded
    EXPECT_GT(successful_queries, 0);
    EXPECT_LT(successful_queries, 200);

    // Budget should be exhausted
    EXPECT_LE(nimcp_sec_get_privacy_budget(ctx), 0.0);

    // Reset and verify
    ASSERT_EQ(nimcp_sec_reset_privacy_budget(ctx), NIMCP_SUCCESS);
    EXPECT_GT(nimcp_sec_get_privacy_budget(ctx), 0.0);

    // Queries should work again
    double noisy;
    EXPECT_EQ(nimcp_sec_private_count(ctx, api_mod.id, 100, &noisy), NIMCP_SUCCESS);
}

//=============================================================================
// Integration with Mathematical Security Tests
//=============================================================================

/**
 * Test: Combined entropy, trust, and privacy workflow
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, CombinedMathematicalSecurityWorkflow)
{
    // Create modules
    auto data_processor = create_simulated_module("data_processor", NIMCP_SEC_CAT_MIDDLEWARE, 2);
    auto analytics = create_simulated_module("analytics", NIMCP_SEC_CAT_API, 1);

    // Phase 1: Entropy monitoring
    // Verify regions have expected entropy characteristics
    for (size_t i = 0; i < data_processor.region_ids.size(); i++) {
        bool is_anomaly;
        double deviation;
        ASSERT_EQ(nimcp_sec_check_region(ctx, data_processor.region_ids[i],
                                        data_processor.region_data[i].data(),
                                        data_processor.region_data[i].size(),
                                        &is_anomaly, &deviation), NIMCP_SUCCESS);
        // Initial check should pass (data matches baseline)
        EXPECT_FALSE(is_anomaly);
    }

    // Phase 2: Trust building
    for (int i = 0; i < 20; i++) {
        nimcp_sec_record_interaction(ctx, data_processor.id, true, 1.0);
        nimcp_sec_record_interaction(ctx, analytics.id, true, 1.0);
    }

    EXPECT_TRUE(nimcp_sec_is_module_trusted(ctx, data_processor.id));
    EXPECT_TRUE(nimcp_sec_is_module_trusted(ctx, analytics.id));

    // Phase 3: Privacy-preserving queries
    // Analytics queries data from data_processor
    double true_record_count = 10000;
    double true_average = 45.7;

    double noisy_count, noisy_mean;
    ASSERT_EQ(nimcp_sec_private_count(ctx, analytics.id, true_record_count, &noisy_count), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_sec_private_mean(ctx, analytics.id, true_average, 10000, 100.0, &noisy_mean), NIMCP_SUCCESS);

    EXPECT_NEAR(noisy_count, true_record_count, 500);
    EXPECT_NEAR(noisy_mean, true_average, 5.0);

    // Phase 4: Generate privacy-preserving report
    nimcp_sec_integration_stats_t private_stats;
    ASSERT_EQ(nimcp_sec_get_private_stats(ctx, &private_stats), NIMCP_SUCCESS);

    // Stats should be reasonable
    EXPECT_GE(private_stats.registered_modules, 0u);
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * Test: Large-scale system with many modules
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, LargeScaleSystem)
{
    const int modules_per_category = 20;
    std::vector<uint32_t> all_module_ids;

    // Register many modules
    for (int cat = 0; cat < NIMCP_SEC_CAT_COUNT; cat++) {
        for (int i = 0; i < modules_per_category; i++) {
            uint32_t id;
            std::string name = "large_" + std::to_string(cat) + "_" + std::to_string(i);
            nimcp_sec_register_module(ctx, name.c_str(),
                                     static_cast<nimcp_sec_module_category_t>(cat), &id);
            all_module_ids.push_back(id);
        }
    }

    // Create trust voucher network
    std::mt19937 rng(42);
    for (size_t i = 0; i < all_module_ids.size(); i++) {
        // Each module vouches for 2-3 others
        int vouches = 2 + (rng() % 2);
        for (int v = 0; v < vouches; v++) {
            uint32_t target = all_module_ids[rng() % all_module_ids.size()];
            if (target != all_module_ids[i]) {
                nimcp_sec_add_trust_voucher(ctx, all_module_ids[i], target);
            }
        }
    }

    // Record interactions
    for (auto id : all_module_ids) {
        int interactions = 5 + (rng() % 10);
        for (int i = 0; i < interactions; i++) {
            bool success = (rng() % 100) < 85;
            nimcp_sec_record_interaction(ctx, id, success, 1.0);
        }
    }

    // Propagate trust
    auto start = std::chrono::high_resolution_clock::now();
    ASSERT_EQ(nimcp_sec_propagate_trust(ctx), NIMCP_SUCCESS);
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(elapsed.count(), 5000);  // Should complete in < 5 seconds

    // Verify stats
    nimcp_sec_integration_stats_t stats;
    nimcp_sec_get_stats(ctx, &stats);
    EXPECT_GE(stats.registered_modules, static_cast<uint32_t>(NIMCP_SEC_CAT_COUNT * modules_per_category));
}

/**
 * Test: High-frequency event generation
 */
TEST_F(SecurityIntegrationFrameworkIntegrationTest, HighFrequencyEvents)
{
    const int num_modules = 50;
    std::vector<SimulatedModule> modules;

    for (int i = 0; i < num_modules; i++) {
        modules.push_back(create_simulated_module("hf_" + std::to_string(i), NIMCP_SEC_CAT_CORE, 1));
    }

    nimcp_sec_clear_events(ctx);

    // Generate many events rapidly
    const int iterations = 100;
    for (int iter = 0; iter < iterations; iter++) {
        for (auto& mod : modules) {
            // Some interactions generate trust events
            nimcp_sec_record_interaction(ctx, mod.id, (iter % 3 != 0), 1.0);
        }
    }

    // Event queue should handle the load
    uint32_t event_count = nimcp_sec_get_event_count(ctx);
    // May have hit queue limit, but should have many events
    EXPECT_GT(event_count, 0u);

    // Should be able to drain events
    nimcp_sec_event_t event;
    int drained = 0;
    while (nimcp_sec_get_event(ctx, &event) == NIMCP_SUCCESS) {
        drained++;
        if (drained > 10000) break;  // Safety limit
    }

    EXPECT_EQ(nimcp_sec_get_event_count(ctx), 0u);
}

}  // namespace
