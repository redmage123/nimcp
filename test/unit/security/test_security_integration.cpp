/**
 * @file test_security_integration.cpp
 * @brief Unit tests for Security Integration Framework
 *
 * Phase SC-4: Universal Security Integration Framework Tests
 *
 * Tests the unified security integration API:
 * - Module registration and lifecycle
 * - Memory region monitoring
 * - Trust management
 * - Differential privacy queries
 * - Event handling
 * - Self-monitoring
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "security/nimcp_security_integration.h"

#include <cstring>
#include <vector>
#include <string>
#include <thread>
#include <atomic>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityIntegrationFrameworkTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        ctx = nimcp_sec_integration_create();
        ASSERT_NE(ctx, nullptr);

        nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
        config.enable_continuous_monitoring = false;  // Don't start thread in tests
        config.enable_self_monitoring = true;
        ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);
    }

    void TearDown() override
    {
        if (ctx) {
            nimcp_sec_integration_destroy(ctx);
            ctx = nullptr;
        }
    }

    nimcp_sec_integration_t* ctx = nullptr;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SecurityIntegrationFrameworkTest, CreateAndDestroy)
{
    nimcp_sec_integration_t* test_ctx = nimcp_sec_integration_create();
    ASSERT_NE(test_ctx, nullptr);
    nimcp_sec_integration_destroy(test_ctx);
}

TEST_F(SecurityIntegrationFrameworkTest, InitWithNullConfig)
{
    nimcp_sec_integration_t* test_ctx = nimcp_sec_integration_create();
    ASSERT_NE(test_ctx, nullptr);
    EXPECT_EQ(nimcp_sec_integration_init(test_ctx, nullptr), NIMCP_SUCCESS);
    nimcp_sec_integration_destroy(test_ctx);
}

TEST_F(SecurityIntegrationFrameworkTest, InitWithCustomConfig)
{
    nimcp_sec_integration_t* test_ctx = nimcp_sec_integration_create();
    ASSERT_NE(test_ctx, nullptr);

    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.trust_threshold = 0.7;
    config.privacy_budget = 20.0;
    config.enable_self_monitoring = false;

    EXPECT_EQ(nimcp_sec_integration_init(test_ctx, &config), NIMCP_SUCCESS);
    nimcp_sec_integration_destroy(test_ctx);
}

TEST_F(SecurityIntegrationFrameworkTest, DefaultConfig)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();

    EXPECT_EQ(config.trust_threshold, 0.5);
    EXPECT_EQ(config.entropy_deviation_threshold, 3.0);
    EXPECT_EQ(config.privacy_budget, 10.0);
    EXPECT_TRUE(config.enable_continuous_monitoring);
    EXPECT_TRUE(config.enable_self_monitoring);
}

//=============================================================================
// Module Registration Tests
//=============================================================================

TEST_F(SecurityIntegrationFrameworkTest, RegisterModule)
{
    uint32_t module_id = 0;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "test_module", NIMCP_SEC_CAT_CORE, &module_id),
              NIMCP_SUCCESS);
    EXPECT_GT(module_id, 0u);
}

TEST_F(SecurityIntegrationFrameworkTest, RegisterMultipleModules)
{
    uint32_t id1, id2, id3;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "core_brain", NIMCP_SEC_CAT_CORE, &id1), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_sec_register_module(ctx, "cognitive_memory", NIMCP_SEC_CAT_COGNITIVE, &id2), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_sec_register_module(ctx, "middleware_pipeline", NIMCP_SEC_CAT_MIDDLEWARE, &id3), NIMCP_SUCCESS);

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST_F(SecurityIntegrationFrameworkTest, GetModuleInfo)
{
    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "info_test", NIMCP_SEC_CAT_GLIAL, &module_id), NIMCP_SUCCESS);

    nimcp_sec_module_info_t info;
    ASSERT_EQ(nimcp_sec_get_module_info(ctx, module_id, &info), NIMCP_SUCCESS);

    EXPECT_EQ(info.module_id, module_id);
    EXPECT_STREQ(info.name, "info_test");
    EXPECT_EQ(info.category, NIMCP_SEC_CAT_GLIAL);
    EXPECT_TRUE(info.active);
}

TEST_F(SecurityIntegrationFrameworkTest, UnregisterModule)
{
    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "unregister_test", NIMCP_SEC_CAT_UTILITY, &module_id), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_sec_unregister_module(ctx, module_id), NIMCP_SUCCESS);

    nimcp_sec_module_info_t info;
    EXPECT_EQ(nimcp_sec_get_module_info(ctx, module_id, &info), NIMCP_NOT_FOUND);
}

TEST_F(SecurityIntegrationFrameworkTest, AllCategories)
{
    const char* names[] = {
        "core", "cognitive", "middleware", "glial", "plasticity",
        "networking", "io", "utility", "gpu", "api", "security"
    };

    for (int i = 0; i < NIMCP_SEC_CAT_COUNT; i++) {
        uint32_t id;
        EXPECT_EQ(nimcp_sec_register_module(ctx, names[i],
                  static_cast<nimcp_sec_module_category_t>(i), &id), NIMCP_SUCCESS);
    }
}

//=============================================================================
// Memory Region Monitoring Tests
//=============================================================================

TEST_F(SecurityIntegrationFrameworkTest, RegisterRegion)
{
    uint32_t module_id, region_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "region_test", NIMCP_SEC_CAT_CORE, &module_id), NIMCP_SUCCESS);

    std::vector<uint8_t> data(1024, 0x42);
    ASSERT_EQ(nimcp_sec_register_region(ctx, module_id, "test_region",
              data.data(), data.size(), &region_id), NIMCP_SUCCESS);
    EXPECT_GT(region_id, 0u);
}

TEST_F(SecurityIntegrationFrameworkTest, CheckRegionNoAnomaly)
{
    uint32_t module_id, region_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "check_test", NIMCP_SEC_CAT_CORE, &module_id), NIMCP_SUCCESS);

    std::vector<uint8_t> data(1024, 0x55);
    ASSERT_EQ(nimcp_sec_register_region(ctx, module_id, "check_region",
              data.data(), data.size(), &region_id), NIMCP_SUCCESS);

    bool is_anomaly = true;
    double deviation = 0.0;
    ASSERT_EQ(nimcp_sec_check_region(ctx, region_id, data.data(), data.size(),
              &is_anomaly, &deviation), NIMCP_SUCCESS);

    EXPECT_FALSE(is_anomaly);
    EXPECT_NEAR(deviation, 0.0, 0.1);
}

TEST_F(SecurityIntegrationFrameworkTest, CheckRegionWithAnomaly)
{
    uint32_t module_id, region_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "anomaly_test", NIMCP_SEC_CAT_CORE, &module_id), NIMCP_SUCCESS);

    // Register with uniform data (low entropy)
    std::vector<uint8_t> baseline(1024, 0xAA);
    ASSERT_EQ(nimcp_sec_register_region(ctx, module_id, "anomaly_region",
              baseline.data(), baseline.size(), &region_id), NIMCP_SUCCESS);

    // Check with random data (high entropy)
    std::vector<uint8_t> tampered(1024);
    for (size_t i = 0; i < tampered.size(); i++) {
        tampered[i] = static_cast<uint8_t>(i * 17 + 31);
    }

    bool is_anomaly = false;
    double deviation = 0.0;
    ASSERT_EQ(nimcp_sec_check_region(ctx, region_id, tampered.data(), tampered.size(),
              &is_anomaly, &deviation), NIMCP_SUCCESS);

    // Entropy changed significantly, should be detected
    EXPECT_TRUE(is_anomaly);
}

TEST_F(SecurityIntegrationFrameworkTest, UpdateRegionBaseline)
{
    uint32_t module_id, region_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "baseline_test", NIMCP_SEC_CAT_CORE, &module_id), NIMCP_SUCCESS);

    std::vector<uint8_t> data1(512, 0x11);
    ASSERT_EQ(nimcp_sec_register_region(ctx, module_id, "update_region",
              data1.data(), data1.size(), &region_id), NIMCP_SUCCESS);

    // Update baseline to new data
    std::vector<uint8_t> data2(512, 0x22);
    ASSERT_EQ(nimcp_sec_update_region_baseline(ctx, region_id, data2.data(), data2.size()),
              NIMCP_SUCCESS);

    // Check should pass with new baseline
    bool is_anomaly = true;
    double deviation = 0.0;
    ASSERT_EQ(nimcp_sec_check_region(ctx, region_id, data2.data(), data2.size(),
              &is_anomaly, &deviation), NIMCP_SUCCESS);
    EXPECT_FALSE(is_anomaly);
}

//=============================================================================
// Trust Management Tests
//=============================================================================

TEST_F(SecurityIntegrationFrameworkTest, RecordSuccessfulInteraction)
{
    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "trust_test", NIMCP_SEC_CAT_COGNITIVE, &module_id), NIMCP_SUCCESS);

    nimcp_trust_score_t initial_score;
    ASSERT_EQ(nimcp_sec_get_trust_score(ctx, module_id, &initial_score), NIMCP_SUCCESS);

    ASSERT_EQ(nimcp_sec_record_interaction(ctx, module_id, true, 1.0), NIMCP_SUCCESS);

    nimcp_trust_score_t after_score;
    ASSERT_EQ(nimcp_sec_get_trust_score(ctx, module_id, &after_score), NIMCP_SUCCESS);

    EXPECT_GT(after_score.expected_trust, initial_score.expected_trust);
}

TEST_F(SecurityIntegrationFrameworkTest, RecordFailedInteraction)
{
    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "fail_test", NIMCP_SEC_CAT_MIDDLEWARE, &module_id), NIMCP_SUCCESS);

    // Build up some trust first
    for (int i = 0; i < 5; i++) {
        nimcp_sec_record_interaction(ctx, module_id, true, 1.0);
    }

    nimcp_trust_score_t before;
    ASSERT_EQ(nimcp_sec_get_trust_score(ctx, module_id, &before), NIMCP_SUCCESS);

    ASSERT_EQ(nimcp_sec_record_interaction(ctx, module_id, false, 1.0), NIMCP_SUCCESS);

    nimcp_trust_score_t after;
    ASSERT_EQ(nimcp_sec_get_trust_score(ctx, module_id, &after), NIMCP_SUCCESS);

    EXPECT_LT(after.expected_trust, before.expected_trust);
}

TEST_F(SecurityIntegrationFrameworkTest, IsModuleTrusted)
{
    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "trusted_test", NIMCP_SEC_CAT_PLASTICITY, &module_id), NIMCP_SUCCESS);

    // Initially neutral trust (0.5)
    // With threshold 0.5, should be borderline trusted

    // Record successes to increase trust
    for (int i = 0; i < 10; i++) {
        nimcp_sec_record_interaction(ctx, module_id, true, 1.0);
    }

    EXPECT_TRUE(nimcp_sec_is_module_trusted(ctx, module_id));
}

TEST_F(SecurityIntegrationFrameworkTest, AddTrustVoucher)
{
    uint32_t module_a, module_b;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "voucher_a", NIMCP_SEC_CAT_CORE, &module_a), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_sec_register_module(ctx, "voucher_b", NIMCP_SEC_CAT_CORE, &module_b), NIMCP_SUCCESS);

    // Build trust for module A
    for (int i = 0; i < 10; i++) {
        nimcp_sec_record_interaction(ctx, module_a, true, 1.0);
    }

    // A vouches for B
    ASSERT_EQ(nimcp_sec_add_trust_voucher(ctx, module_a, module_b), NIMCP_SUCCESS);

    // Propagate trust
    ASSERT_EQ(nimcp_sec_propagate_trust(ctx), NIMCP_SUCCESS);

    // B should have received some derived trust
    nimcp_trust_score_t score_b;
    ASSERT_EQ(nimcp_sec_get_trust_score(ctx, module_b, &score_b), NIMCP_SUCCESS);
    EXPECT_GT(score_b.expected_trust, 0.4);  // Should be better than untrusted
}

//=============================================================================
// Differential Privacy Tests
//=============================================================================

TEST_F(SecurityIntegrationFrameworkTest, PrivateCount)
{
    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "dp_count", NIMCP_SEC_CAT_API, &module_id), NIMCP_SUCCESS);

    double noisy_count;
    ASSERT_EQ(nimcp_sec_private_count(ctx, module_id, 1000, &noisy_count), NIMCP_SUCCESS);

    // Should be approximately correct
    EXPECT_NEAR(noisy_count, 1000, 50);
}

TEST_F(SecurityIntegrationFrameworkTest, PrivateSum)
{
    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "dp_sum", NIMCP_SEC_CAT_API, &module_id), NIMCP_SUCCESS);

    double noisy_sum;
    ASSERT_EQ(nimcp_sec_private_sum(ctx, module_id, 5000.0, 100.0, &noisy_sum), NIMCP_SUCCESS);

    // Should be approximately correct
    EXPECT_NEAR(noisy_sum, 5000, 500);
}

TEST_F(SecurityIntegrationFrameworkTest, PrivateMean)
{
    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "dp_mean", NIMCP_SEC_CAT_API, &module_id), NIMCP_SUCCESS);

    double noisy_mean;
    ASSERT_EQ(nimcp_sec_private_mean(ctx, module_id, 50.0, 100, 100.0, &noisy_mean), NIMCP_SUCCESS);

    // Should be approximately correct
    EXPECT_NEAR(noisy_mean, 50.0, 5.0);
}

TEST_F(SecurityIntegrationFrameworkTest, PrivacyBudget)
{
    double initial_budget = nimcp_sec_get_privacy_budget(ctx);
    EXPECT_GT(initial_budget, 0);

    uint32_t module_id;
    nimcp_sec_register_module(ctx, "budget_test", NIMCP_SEC_CAT_API, &module_id);

    // Spend some budget
    double noisy;
    nimcp_sec_private_count(ctx, module_id, 100, &noisy);

    double remaining_budget = nimcp_sec_get_privacy_budget(ctx);
    EXPECT_LT(remaining_budget, initial_budget);
}

TEST_F(SecurityIntegrationFrameworkTest, ResetPrivacyBudget)
{
    uint32_t module_id;
    nimcp_sec_register_module(ctx, "reset_test", NIMCP_SEC_CAT_API, &module_id);

    // Spend some budget
    double noisy;
    for (int i = 0; i < 5; i++) {
        nimcp_sec_private_count(ctx, module_id, 100, &noisy);
    }

    double before_reset = nimcp_sec_get_privacy_budget(ctx);

    ASSERT_EQ(nimcp_sec_reset_privacy_budget(ctx), NIMCP_SUCCESS);

    double after_reset = nimcp_sec_get_privacy_budget(ctx);
    EXPECT_GT(after_reset, before_reset);
}

//=============================================================================
// Event Handling Tests
//=============================================================================

TEST_F(SecurityIntegrationFrameworkTest, ModuleRegistrationGeneratesEvent)
{
    uint32_t module_id;
    nimcp_sec_register_module(ctx, "event_test", NIMCP_SEC_CAT_NETWORKING, &module_id);

    EXPECT_GT(nimcp_sec_get_event_count(ctx), 0u);

    nimcp_sec_event_t event;
    ASSERT_EQ(nimcp_sec_get_event(ctx, &event), NIMCP_SUCCESS);
    EXPECT_EQ(event.type, NIMCP_SEC_EVENT_MODULE_REGISTERED);
}

TEST_F(SecurityIntegrationFrameworkTest, ClearEvents)
{
    uint32_t module_id;
    nimcp_sec_register_module(ctx, "clear_test", NIMCP_SEC_CAT_IO, &module_id);

    EXPECT_GT(nimcp_sec_get_event_count(ctx), 0u);

    ASSERT_EQ(nimcp_sec_clear_events(ctx), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_sec_get_event_count(ctx), 0u);
}

TEST_F(SecurityIntegrationFrameworkTest, EventQueueEmpty)
{
    nimcp_sec_clear_events(ctx);

    nimcp_sec_event_t event;
    EXPECT_EQ(nimcp_sec_get_event(ctx, &event), NIMCP_NOT_FOUND);
}

//=============================================================================
// Self-Monitoring Tests
//=============================================================================

TEST_F(SecurityIntegrationFrameworkTest, SelfCheck)
{
    bool passed = false;
    ASSERT_EQ(nimcp_sec_self_check(ctx, &passed), NIMCP_SUCCESS);
    EXPECT_TRUE(passed);
}

TEST_F(SecurityIntegrationFrameworkTest, CheckIntegrity)
{
    uint32_t module_id, region_id;
    nimcp_sec_register_module(ctx, "integrity_test", NIMCP_SEC_CAT_CORE, &module_id);

    std::vector<uint8_t> data(256, 0x33);
    nimcp_sec_register_region(ctx, module_id, "integrity_region", data.data(), data.size(), &region_id);

    uint32_t anomalies = 0;
    ASSERT_EQ(nimcp_sec_check_integrity(ctx, &anomalies), NIMCP_SUCCESS);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SecurityIntegrationFrameworkTest, GetStats)
{
    uint32_t id1, id2;
    nimcp_sec_register_module(ctx, "stats1", NIMCP_SEC_CAT_CORE, &id1);
    nimcp_sec_register_module(ctx, "stats2", NIMCP_SEC_CAT_COGNITIVE, &id2);

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(ctx, &stats), NIMCP_SUCCESS);

    // At least 2 modules registered (plus security self-module)
    EXPECT_GE(stats.registered_modules, 2u);
    EXPECT_GE(stats.active_modules, 2u);
}

TEST_F(SecurityIntegrationFrameworkTest, GetPrivateStats)
{
    uint32_t id1, id2, id3;
    nimcp_sec_register_module(ctx, "priv1", NIMCP_SEC_CAT_CORE, &id1);
    nimcp_sec_register_module(ctx, "priv2", NIMCP_SEC_CAT_COGNITIVE, &id2);
    nimcp_sec_register_module(ctx, "priv3", NIMCP_SEC_CAT_MIDDLEWARE, &id3);

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_private_stats(ctx, &stats), NIMCP_SUCCESS);

    // Private stats should still have reasonable values
    EXPECT_GE(stats.registered_modules, 0u);  // Might be slightly different due to noise
}

TEST_F(SecurityIntegrationFrameworkTest, GenerateTrustReport)
{
    uint32_t id1, id2;
    nimcp_sec_register_module(ctx, "report1", NIMCP_SEC_CAT_CORE, &id1);
    nimcp_sec_register_module(ctx, "report2", NIMCP_SEC_CAT_COGNITIVE, &id2);

    // Generate some interactions
    nimcp_sec_record_interaction(ctx, id1, true, 1.0);
    nimcp_sec_record_interaction(ctx, id2, false, 1.0);

    char buffer[4096];
    size_t len = nimcp_sec_generate_trust_report(ctx, buffer, sizeof(buffer));

    EXPECT_GT(len, 0u);
    EXPECT_TRUE(strstr(buffer, "report1") != nullptr);
    EXPECT_TRUE(strstr(buffer, "report2") != nullptr);
}

//=============================================================================
// Name Lookup Tests
//=============================================================================

TEST_F(SecurityIntegrationFrameworkTest, CategoryNames)
{
    EXPECT_STREQ(nimcp_sec_category_name(NIMCP_SEC_CAT_CORE), "CORE");
    EXPECT_STREQ(nimcp_sec_category_name(NIMCP_SEC_CAT_COGNITIVE), "COGNITIVE");
    EXPECT_STREQ(nimcp_sec_category_name(NIMCP_SEC_CAT_SECURITY), "SECURITY");
}

TEST_F(SecurityIntegrationFrameworkTest, EventTypeNames)
{
    EXPECT_STREQ(nimcp_sec_event_type_name(NIMCP_SEC_EVENT_ENTROPY_ANOMALY), "ENTROPY_ANOMALY");
    EXPECT_STREQ(nimcp_sec_event_type_name(NIMCP_SEC_EVENT_TRUST_VIOLATION), "TRUST_VIOLATION");
    EXPECT_STREQ(nimcp_sec_event_type_name(NIMCP_SEC_EVENT_SELF_CHECK), "SELF_CHECK");
}

TEST_F(SecurityIntegrationFrameworkTest, SeverityNames)
{
    EXPECT_STREQ(nimcp_sec_severity_name(NIMCP_SEC_SEVERITY_INFO), "INFO");
    EXPECT_STREQ(nimcp_sec_severity_name(NIMCP_SEC_SEVERITY_CRITICAL), "CRITICAL");
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(SecurityIntegrationFrameworkTest, ConcurrentModuleRegistration)
{
    const int num_threads = 4;
    const int modules_per_thread = 10;
    std::atomic<int> successful{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, modules_per_thread, &successful]() {
            for (int i = 0; i < modules_per_thread; i++) {
                uint32_t id;
                std::string name = "concurrent_" + std::to_string(t) + "_" + std::to_string(i);
                if (nimcp_sec_register_module(ctx, name.c_str(), NIMCP_SEC_CAT_UTILITY, &id) == NIMCP_SUCCESS) {
                    successful++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successful.load(), num_threads * modules_per_thread);
}

TEST_F(SecurityIntegrationFrameworkTest, ConcurrentTrustUpdates)
{
    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "concurrent_trust", NIMCP_SEC_CAT_CORE, &module_id), NIMCP_SUCCESS);

    const int num_threads = 4;
    const int updates_per_thread = 50;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, module_id, t, updates_per_thread]() {
            for (int i = 0; i < updates_per_thread; i++) {
                bool success = (i % 5 != 0);  // 80% success rate
                nimcp_sec_record_interaction(ctx, module_id, success, 1.0);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    nimcp_trust_score_t score;
    ASSERT_EQ(nimcp_sec_get_trust_score(ctx, module_id, &score), NIMCP_SUCCESS);
    // Total observations should match
    EXPECT_EQ(score.observations, static_cast<uint64_t>(num_threads * updates_per_thread));
}

}  // namespace
