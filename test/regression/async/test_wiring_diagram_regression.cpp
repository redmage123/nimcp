/**
 * @file test_wiring_diagram_regression.cpp
 * @brief Regression tests for KG-Based Wiring Diagram System
 *
 * Tests specific scenarios that have caused issues in the past:
 * - Empty module names
 * - Null pointer handling
 * - Persistence edge cases
 * - Hardware detection edge cases
 * - Concurrent access patterns
 * - Large module counts
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>
#include <fstream>
#include <filesystem>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WiringDiagramRegressionTest : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
    std::string temp_dir_;

    void SetUp() override {
        // Initialize bio-async for module context creation
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = false;
        bio_config.enable_logging = false;
        nimcp_bio_async_init(&bio_config);

        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = false;
        router_config.enable_logging = false;
        bio_router_init(&router_config);

        // Create temporary directory
        temp_dir_ = "/tmp/nimcp_wiring_regression_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir_);
        std::filesystem::create_directories(temp_dir_ + "/subsystems");
    }

    void TearDown() override {
        if (wd_) {
            wiring_diagram_destroy(wd_);
            wd_ = nullptr;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        if (!temp_dir_.empty()) {
            std::filesystem::remove_all(temp_dir_);
        }
    }
};

//=============================================================================
// Regression: Empty/Invalid Module Names (Issue #1)
//=============================================================================

TEST_F(WiringDiagramRegressionTest, Regression_EmptyModuleNameRejected) {
    // Regression: Empty module names were being accepted, causing lookup failures
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    wiring_module_config_t config;
    wiring_module_config_init(&config);

    // Empty name should be rejected
    int result = wiring_diagram_add_module(wd_, "", &config);
    EXPECT_EQ(result, -1) << "Regression: Empty module name must be rejected";
}

TEST_F(WiringDiagramRegressionTest, Regression_WhitespaceOnlyModuleName) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    wiring_module_config_t config;
    wiring_module_config_init(&config);

    // Module with only whitespace - implementation may accept or reject
    // Just verify it doesn't crash
    wiring_diagram_add_module(wd_, "   ", &config);
    SUCCEED();
}

TEST_F(WiringDiagramRegressionTest, Regression_VeryLongModuleName) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // Create very long name (exceeds buffer)
    std::string long_name(256, 'X');

    wiring_module_config_t config;
    wiring_module_config_init(&config);

    // Should handle gracefully (truncate or reject)
    int result = wiring_diagram_add_module(wd_, long_name.c_str(), &config);
    // Just verify no crash
    (void)result;
    SUCCEED();
}

//=============================================================================
// Regression: Default Profile Values (Issue #2)
//=============================================================================

TEST_F(WiringDiagramRegressionTest, Regression_DefaultProfileUsesConservativeTier) {
    // Regression: Default profile was using MEDIUM tier, should be CONSTRAINED
    wiring_hardware_profile_t profile;
    wiring_get_default_profile(&profile);

    EXPECT_EQ(profile.tier, PLATFORM_TIER_CONSTRAINED)
        << "Regression: Default profile should use conservative CONSTRAINED tier";
    EXPECT_EQ(profile.hw_flags, WIRING_HW_NONE)
        << "Regression: Default profile should have no hardware flags";
}

TEST_F(WiringDiagramRegressionTest, Regression_DefaultConfigUsesMinimalTier) {
    // Regression: Module config init was using BASIC tier, should be MINIMAL
    wiring_module_config_t config;
    wiring_module_config_init(&config);

    EXPECT_EQ(config.min_tier, PLATFORM_TIER_MINIMAL)
        << "Regression: Default config should use MINIMAL tier";
    EXPECT_TRUE(config.enabled)
        << "Regression: Modules should be enabled by default";
}

//=============================================================================
// Regression: Hardware Flags String Formatting (Issue #3)
//=============================================================================

TEST_F(WiringDiagramRegressionTest, Regression_HardwareFlagsLowercase) {
    // Regression: Hardware flags were being output as uppercase, but file naming uses lowercase
    char buffer[256];

    wiring_hardware_flags_to_string(WIRING_HW_CUDA, buffer, sizeof(buffer));
    EXPECT_NE(strstr(buffer, "cuda"), nullptr)
        << "Regression: Hardware flags should use lowercase names";
    EXPECT_EQ(strstr(buffer, "CUDA"), nullptr)
        << "Regression: Hardware flags should NOT use uppercase names";
}

TEST_F(WiringDiagramRegressionTest, Regression_CombinedHardwareFlagsDelimiter) {
    char buffer[256];

    wiring_hardware_flags_to_string(
        (wiring_hardware_flags_t)(WIRING_HW_CUDA | WIRING_HW_ROCM),
        buffer, sizeof(buffer)
    );

    // Should have delimiter between flags
    EXPECT_NE(strstr(buffer, "|"), nullptr)
        << "Regression: Combined flags should have | delimiter";
}

//=============================================================================
// Regression: Persistence Path Issues (Issue #4)
//=============================================================================

TEST_F(WiringDiagramRegressionTest, Regression_PersistenceToCorrectSubsystemFile) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    // Add module to ETHICS subsystem
    wiring_module_config_t config;
    wiring_module_config_init(&config);
    config.subsystem = WIRING_SUBSYSTEM_ETHICS;
    ASSERT_EQ(wiring_diagram_add_module(wd_, "ethics_test_module", &config), 0);

    // Persist
    ASSERT_EQ(wiring_diagram_persist(wd_), 0);

    // Verify file was created in correct location
    std::string expected_file = temp_dir_ + "/subsystems/ethics.jsonl";
    EXPECT_TRUE(std::filesystem::exists(expected_file))
        << "Regression: Module should persist to subsystems/<subsystem>.jsonl";
}

TEST_F(WiringDiagramRegressionTest, Regression_LoadFromCorrectSubsystemFile) {
    // Create ethics.jsonl manually
    std::string ethics_file = temp_dir_ + "/subsystems/ethics.jsonl";
    std::ofstream file(ethics_file);
    file << R"({"type":"entity","name":"ethics_module","entityType":"CognitiveModule","subsystem":"ethics","enabled":true})" << "\n";
    file.close();

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // Load ethics subsystem
    ASSERT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_ETHICS), 0);

    // Verify module is loaded (shallow copy - no cleanup needed)
    wiring_module_config_t config;
    memset(&config, 0, sizeof(config));
    int result = wiring_diagram_get_module_config(wd_, "ethics_module", &config);
    EXPECT_EQ(result, 0)
        << "Regression: Module should load from subsystems/<subsystem>.jsonl";
    // Note: Don't call cleanup - shallow copy, memory owned by diagram
}

//=============================================================================
// Regression: Null Pointer Handling (Issue #5)
//=============================================================================

TEST_F(WiringDiagramRegressionTest, Regression_NullDiagramOperations) {
    // All operations on null diagram should fail gracefully
    EXPECT_EQ(wiring_diagram_load_master(nullptr), -1);
    EXPECT_EQ(wiring_diagram_load_subsystem(nullptr, WIRING_SUBSYSTEM_CORE), -1);
    EXPECT_EQ(wiring_diagram_load_platform(nullptr, PLATFORM_TIER_FULL), -1);
    EXPECT_EQ(wiring_diagram_load_hardware(nullptr, WIRING_HW_CUDA), -1);
    EXPECT_EQ(wiring_diagram_persist(nullptr), -1);

    wiring_module_config_t config;
    EXPECT_EQ(wiring_diagram_get_module_config(nullptr, "test", &config), -1);
    EXPECT_EQ(wiring_diagram_add_module(nullptr, "test", &config), -1);
    EXPECT_EQ(wiring_diagram_remove_module(nullptr, "test"), -1);

    SUCCEED() << "Regression: All null diagram operations handled gracefully";
}

TEST_F(WiringDiagramRegressionTest, Regression_NullProfileOperations) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // Null profile should be handled
    EXPECT_EQ(wiring_diagram_load_for_profile(wd_, nullptr), -1);
    EXPECT_FALSE(wiring_diagram_module_available(wd_, "test", nullptr));

    // Null output in get_default_profile should not crash
    wiring_get_default_profile(nullptr);
    SUCCEED();
}

//=============================================================================
// Regression: Concurrent Access (Issue #6)
//=============================================================================

TEST_F(WiringDiagramRegressionTest, Regression_ConcurrentAddAndQuery) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    std::atomic<int> add_success{0};
    std::atomic<int> query_success{0};
    std::vector<std::thread> threads;

    // Writer threads add modules
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t, &add_success]() {
            for (int i = 0; i < 25; i++) {
                std::string name = "concurrent_module_" + std::to_string(t) + "_" + std::to_string(i);
                wiring_module_config_t config;
                wiring_module_config_init(&config);
                if (wiring_diagram_add_module(wd_, name.c_str(), &config) == 0) {
                    add_success++;
                }
            }
        });
    }

    // Reader threads query modules
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &query_success]() {
            for (int i = 0; i < 50; i++) {
                const char* modules[100];
                uint32_t count = wiring_diagram_get_all_modules(wd_, modules, 100);
                if (count >= 0) {
                    query_success++;
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(add_success.load(), 100)
        << "Regression: All concurrent adds should succeed";
    EXPECT_EQ(query_success.load(), 200)
        << "Regression: All concurrent queries should succeed";
}

//=============================================================================
// Regression: Large Module Count (Issue #7)
//=============================================================================

TEST_F(WiringDiagramRegressionTest, Regression_LargeModuleCount) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    const int MODULE_COUNT = 500;

    // Add many modules
    for (int i = 0; i < MODULE_COUNT; i++) {
        std::string name = "large_test_module_" + std::to_string(i);
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        int result = wiring_diagram_add_module(wd_, name.c_str(), &config);
        ASSERT_EQ(result, 0) << "Failed to add module " << i;
    }

    // Verify all can be retrieved
    const char* modules[600];
    uint32_t count = wiring_diagram_get_all_modules(wd_, modules, 600);
    EXPECT_EQ(count, (uint32_t)MODULE_COUNT)
        << "Regression: Should handle large module counts";
}

//=============================================================================
// Regression: Subsystem Boundary Values (Issue #8)
//=============================================================================

TEST_F(WiringDiagramRegressionTest, Regression_SubsystemBoundaryValues) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // Test all valid subsystems
    for (int i = 0; i < WIRING_SUBSYSTEM_COUNT; i++) {
        const char* name = wiring_subsystem_to_string((wiring_subsystem_t)i);
        EXPECT_NE(name, nullptr) << "Subsystem " << i << " should have a name";
        EXPECT_GT(strlen(name), 0u) << "Subsystem " << i << " name should not be empty";
    }

    // Test invalid subsystem
    const char* invalid_name = wiring_subsystem_to_string((wiring_subsystem_t)999);
    EXPECT_NE(invalid_name, nullptr) << "Invalid subsystem should return 'unknown'";
}

TEST_F(WiringDiagramRegressionTest, Regression_RelationTypeBoundaryValues) {
    // Test all valid relations
    for (int i = 0; i < WIRING_RELATION_COUNT; i++) {
        const char* name = wiring_relation_to_string((wiring_relation_type_t)i);
        EXPECT_NE(name, nullptr) << "Relation " << i << " should have a name";
        EXPECT_GT(strlen(name), 0u) << "Relation " << i << " name should not be empty";
    }

    // Test invalid relation
    const char* invalid_name = wiring_relation_to_string((wiring_relation_type_t)999);
    EXPECT_NE(invalid_name, nullptr) << "Invalid relation should return 'unknown'";
}

//=============================================================================
// Regression: Stats with Disabled Modules (Issue #9)
//=============================================================================

TEST_F(WiringDiagramRegressionTest, Regression_StatsCountDisabledModulesCorrectly) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    // Add 3 enabled modules
    for (int i = 0; i < 3; i++) {
        std::string name = "enabled_module_" + std::to_string(i);
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        config.enabled = true;
        ASSERT_EQ(wiring_diagram_add_module(wd_, name.c_str(), &config), 0);
    }

    // Add 2 disabled modules
    for (int i = 0; i < 2; i++) {
        std::string name = "disabled_module_" + std::to_string(i);
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        config.enabled = false;
        ASSERT_EQ(wiring_diagram_add_module(wd_, name.c_str(), &config), 0);
    }

    uint32_t total = 0, enabled = 0, relations = 0;
    int result = wiring_diagram_get_stats(wd_, &total, &enabled, &relations);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(total, 5u) << "Regression: Total should include all modules";
    EXPECT_EQ(enabled, 3u) << "Regression: Enabled count should only count enabled modules";
}

