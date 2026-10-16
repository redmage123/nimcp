/**
 * @file test_wiring_diagram.cpp
 * @brief Comprehensive unit tests for KG-Based Runtime Module Wiring Diagram System
 *
 * Test Categories:
 * 1. Lifecycle Tests - Create, destroy, with custom/default paths
 * 2. Loading Tests - Master, subsystem, platform, hardware, profile loading
 * 3. Query Tests - Module config, availability, subsystem/all modules
 * 4. Dynamic Update Tests - Add/remove modules, relations, handlers, enabled state
 * 5. Persistence Tests - Auto-persist, manual persist, reload
 * 6. Utility Tests - String conversions, config init/cleanup
 * 7. Hardware Detection Tests - Profile detection, default profile
 * 8. Edge Cases - Null pointers, capacity limits, error handling
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <filesystem>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WiringDiagramTest : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
    std::string temp_dir_;

    void SetUp() override {
        // Initialize bio-async system (needed for module contexts)
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = false;
        bio_config.enable_logging = false;
        nimcp_bio_async_init(&bio_config);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = false;
        router_config.enable_logging = false;
        bio_router_init(&router_config);

        // Create temporary directory for test wiring files
        temp_dir_ = "/tmp/nimcp_wiring_test_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir_);
        std::filesystem::create_directories(temp_dir_ + "/subsystems");
        std::filesystem::create_directories(temp_dir_ + "/platforms");
        std::filesystem::create_directories(temp_dir_ + "/hardware");
        std::filesystem::create_directories(temp_dir_ + "/custom");
    }

    void TearDown() override {
        if (wd_) {
            wiring_diagram_destroy(wd_);
            wd_ = nullptr;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        // Clean up temp directory
        if (!temp_dir_.empty()) {
            std::filesystem::remove_all(temp_dir_);
        }
    }

    // Helper: Create a minimal JSONL wiring file
    void CreateTestMasterFile() {
        std::ofstream file(temp_dir_ + "/master.jsonl");
        file << R"({"type":"entity","name":"TestModule","entityType":"CognitiveModule","subsystem":"core","min_tier":0})" << "\n";
        file << R"({"type":"entity","name":"TestModule2","entityType":"CognitiveModule","subsystem":"cognition","min_tier":2})" << "\n";
        file << R"({"type":"relation","from":"TestModule2","to":"TestModule","relationType":"DEPENDS_ON"})" << "\n";
        file.close();
    }

    // Helper: Create subsystem wiring file
    void CreateTestSubsystemFile(const char* subsystem) {
        std::string filename = temp_dir_ + "/subsystems/" + subsystem + ".jsonl";
        std::ofstream file(filename);
        file << R"({"type":"entity","name":")" << subsystem << R"(_module","entityType":"CognitiveModule","subsystem":")" << subsystem << R"(","min_tier":1})" << "\n";
        file.close();
    }

    // Helper: Create platform wiring file
    void CreateTestPlatformFile(const char* tier_name) {
        std::string filename = temp_dir_ + "/platforms/" + tier_name + ".jsonl";
        std::ofstream file(filename);
        file << R"({"type":"entity","name":"PlatformSpecificModule","entityType":"CognitiveModule","enabled":true})" << "\n";
        file.close();
    }

    // Helper: Create hardware wiring file
    void CreateTestHardwareFile(const char* hw_name) {
        std::string filename = temp_dir_ + "/hardware/" + hw_name + ".jsonl";
        std::ofstream file(filename);
        file << R"({"type":"entity","name":"HardwareSpecificModule","entityType":"CognitiveModule","required_hw":")" << hw_name << R"("})" << "\n";
        file.close();
    }

    // Helper: Initialize a module config
    void InitTestModuleConfig(wiring_module_config_t* config, const char* name) {
        wiring_module_config_init(config);
        strncpy(config->module_name, name, sizeof(config->module_name) - 1);
        config->module_id = BIO_MODULE_BRAIN;
        config->subsystem = WIRING_SUBSYSTEM_CORE;
        config->min_tier = PLATFORM_TIER_MINIMAL;
        config->required_hw = WIRING_HW_NONE;
        config->enabled = true;
    }
};

//=============================================================================
// 1. LIFECYCLE TESTS
//=============================================================================

TEST_F(WiringDiagramTest, test_wiring_diagram_create_destroy) {
    // Create wiring diagram with temp path
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr) << "wiring_diagram_create should return non-null";

    // Verify auto-persist is enabled by default
    EXPECT_TRUE(wiring_diagram_get_auto_persist(wd_));

    // Destroy (handled by TearDown, but test explicit call)
    wiring_diagram_destroy(wd_);
    wd_ = nullptr;
}

TEST_F(WiringDiagramTest, test_wiring_diagram_create_with_custom_path) {
    std::string custom_path = temp_dir_ + "/custom_wiring";
    std::filesystem::create_directories(custom_path);

    wd_ = wiring_diagram_create(custom_path.c_str());
    ASSERT_NE(wd_, nullptr) << "wiring_diagram_create with custom path should succeed";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_create_null_path_uses_default) {
    // Creating with NULL should use default path (may fail if default path doesn't exist)
    // This tests that the function handles NULL gracefully
    wd_ = wiring_diagram_create(nullptr);
    // The function should either succeed with default path or return NULL gracefully
    // We just verify it doesn't crash
    if (wd_ != nullptr) {
        EXPECT_TRUE(wiring_diagram_get_auto_persist(wd_));
    }
}

TEST_F(WiringDiagramTest, test_wiring_diagram_destroy_null_safe) {
    // Should not crash when destroying NULL
    wiring_diagram_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// 2. LOADING TESTS
//=============================================================================

TEST_F(WiringDiagramTest, test_wiring_diagram_load_master) {
    CreateTestMasterFile();

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_load_master(wd_);
    EXPECT_EQ(result, 0) << "load_master should succeed with valid file";

    // Verify modules were loaded
    const char* modules[10];
    uint32_t count = wiring_diagram_get_all_modules(wd_, modules, 10);
    EXPECT_GE(count, 1u) << "Should have loaded at least one module";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_master_missing_file) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // No master.jsonl file exists - should return error or handle gracefully
    int result = wiring_diagram_load_master(wd_);
    // Implementation may return 0 (no file is okay) or -1 (file required)
    // Just verify it doesn't crash
    (void)result;
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_subsystem) {
    CreateTestSubsystemFile("ethics");

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_ETHICS);
    EXPECT_EQ(result, 0) << "load_subsystem should succeed with valid file";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_subsystem_missing) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // Missing file should return 0 (not an error per API docs)
    int result = wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_PERCEPTION);
    EXPECT_EQ(result, 0) << "Missing subsystem file should return 0";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_platform) {
    CreateTestPlatformFile("full");

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_load_platform(wd_, PLATFORM_TIER_FULL);
    EXPECT_EQ(result, 0) << "load_platform should succeed with valid file";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_platform_missing) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // Missing file should return 0
    int result = wiring_diagram_load_platform(wd_, PLATFORM_TIER_MEDIUM);
    EXPECT_EQ(result, 0) << "Missing platform file should return 0";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_hardware) {
    CreateTestHardwareFile("cuda");

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_load_hardware(wd_, WIRING_HW_CUDA);
    EXPECT_EQ(result, 0) << "load_hardware should succeed";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_hardware_none) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_load_hardware(wd_, WIRING_HW_NONE);
    EXPECT_EQ(result, 0) << "load_hardware with NONE should succeed (no-op)";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_for_profile) {
    CreateTestMasterFile();
    CreateTestPlatformFile("medium");

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    wiring_hardware_profile_t profile;
    wiring_get_default_profile(&profile);
    profile.tier = PLATFORM_TIER_MEDIUM;
    profile.hw_flags = WIRING_HW_NONE;

    int result = wiring_diagram_load_for_profile(wd_, &profile);
    EXPECT_EQ(result, 0) << "load_for_profile should succeed";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_for_profile_null) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_load_for_profile(wd_, nullptr);
    EXPECT_EQ(result, -1) << "load_for_profile with NULL profile should fail";
}

//=============================================================================
// 3. QUERY TESTS
//=============================================================================

TEST_F(WiringDiagramTest, test_wiring_diagram_get_module_config) {
    CreateTestMasterFile();

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    ASSERT_EQ(wiring_diagram_load_master(wd_), 0);

    wiring_module_config_t config;
    wiring_module_config_init(&config);

    int result = wiring_diagram_get_module_config(wd_, "TestModule", &config);
    EXPECT_EQ(result, 0) << "get_module_config should find existing module";
    EXPECT_STREQ(config.module_name, "TestModule");

    wiring_module_config_cleanup(&config);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_get_module_config_not_found) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    wiring_module_config_t config;
    wiring_module_config_init(&config);

    int result = wiring_diagram_get_module_config(wd_, "NonExistentModule", &config);
    EXPECT_EQ(result, -1) << "get_module_config should return -1 for missing module";

    wiring_module_config_cleanup(&config);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_get_module_config_null_params) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    wiring_module_config_t config;

    EXPECT_EQ(wiring_diagram_get_module_config(nullptr, "test", &config), -1);
    EXPECT_EQ(wiring_diagram_get_module_config(wd_, nullptr, &config), -1);
    EXPECT_EQ(wiring_diagram_get_module_config(wd_, "test", nullptr), -1);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_module_available) {
    CreateTestMasterFile();

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    ASSERT_EQ(wiring_diagram_load_master(wd_), 0);

    wiring_hardware_profile_t profile;
    wiring_get_default_profile(&profile);
    profile.tier = PLATFORM_TIER_FULL;

    // TestModule has min_tier 0, should be available on FULL
    bool available = wiring_diagram_module_available(wd_, "TestModule", &profile);
    EXPECT_TRUE(available);

    // TestModule2 has min_tier 2, check availability
    available = wiring_diagram_module_available(wd_, "TestModule2", &profile);
    EXPECT_TRUE(available);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_module_available_tier_constraint) {
    CreateTestMasterFile();

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    ASSERT_EQ(wiring_diagram_load_master(wd_), 0);

    wiring_hardware_profile_t profile;
    wiring_get_default_profile(&profile);
    profile.tier = PLATFORM_TIER_MINIMAL;

    // TestModule2 has min_tier 2 (CONSTRAINED), should NOT be available on MINIMAL (1)
    bool available = wiring_diagram_module_available(wd_, "TestModule2", &profile);
    EXPECT_FALSE(available);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_get_subsystem_modules) {
    CreateTestMasterFile();

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    ASSERT_EQ(wiring_diagram_load_master(wd_), 0);

    const char* modules[10];
    uint32_t count = wiring_diagram_get_subsystem_modules(
        wd_, WIRING_SUBSYSTEM_CORE, modules, 10
    );
    EXPECT_GE(count, 1u) << "Should find at least one core module";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_get_subsystem_modules_empty) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    const char* modules[10];
    uint32_t count = wiring_diagram_get_subsystem_modules(
        wd_, WIRING_SUBSYSTEM_SOCIAL, modules, 10
    );
    EXPECT_EQ(count, 0u) << "Empty subsystem should return 0";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_get_all_modules) {
    CreateTestMasterFile();

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    ASSERT_EQ(wiring_diagram_load_master(wd_), 0);

    const char* modules[10];
    uint32_t count = wiring_diagram_get_all_modules(wd_, modules, 10);
    EXPECT_GE(count, 2u) << "Should find at least 2 modules from master";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_get_all_modules_empty) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    const char* modules[10];
    uint32_t count = wiring_diagram_get_all_modules(wd_, modules, 10);
    EXPECT_EQ(count, 0u) << "Empty diagram should return 0";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_get_all_modules_null) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    EXPECT_EQ(wiring_diagram_get_all_modules(nullptr, nullptr, 10), 0u);
    EXPECT_EQ(wiring_diagram_get_all_modules(wd_, nullptr, 10), 0u);
}

//=============================================================================
// 4. DYNAMIC UPDATE TESTS
//=============================================================================

TEST_F(WiringDiagramTest, test_wiring_diagram_add_module) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // Disable auto-persist to avoid file writes during test
    wiring_diagram_set_auto_persist(wd_, false);

    wiring_module_config_t config;
    InitTestModuleConfig(&config, "NewModule");

    int result = wiring_diagram_add_module(wd_, "NewModule", &config);
    EXPECT_EQ(result, 0) << "add_module should succeed";

    // Verify module was added
    wiring_module_config_t retrieved;
    wiring_module_config_init(&retrieved);
    result = wiring_diagram_get_module_config(wd_, "NewModule", &retrieved);
    EXPECT_EQ(result, 0) << "Should find newly added module";

    wiring_module_config_cleanup(&retrieved);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_add_module_update_existing) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    wiring_module_config_t config;
    InitTestModuleConfig(&config, "UpdateModule");
    config.min_tier = PLATFORM_TIER_MINIMAL;

    ASSERT_EQ(wiring_diagram_add_module(wd_, "UpdateModule", &config), 0);

    // Update the module
    config.min_tier = PLATFORM_TIER_FULL;
    EXPECT_EQ(wiring_diagram_add_module(wd_, "UpdateModule", &config), 0);

    // Verify update
    wiring_module_config_t retrieved;
    wiring_module_config_init(&retrieved);
    ASSERT_EQ(wiring_diagram_get_module_config(wd_, "UpdateModule", &retrieved), 0);
    EXPECT_EQ(retrieved.min_tier, PLATFORM_TIER_FULL);

    wiring_module_config_cleanup(&retrieved);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_add_module_null_params) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    wiring_module_config_t config;
    InitTestModuleConfig(&config, "Test");

    EXPECT_EQ(wiring_diagram_add_module(nullptr, "Test", &config), -1);
    EXPECT_EQ(wiring_diagram_add_module(wd_, nullptr, &config), -1);
    EXPECT_EQ(wiring_diagram_add_module(wd_, "Test", nullptr), -1);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_add_relation) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    // Add two modules first
    wiring_module_config_t config1, config2;
    InitTestModuleConfig(&config1, "ModuleA");
    InitTestModuleConfig(&config2, "ModuleB");

    ASSERT_EQ(wiring_diagram_add_module(wd_, "ModuleA", &config1), 0);
    ASSERT_EQ(wiring_diagram_add_module(wd_, "ModuleB", &config2), 0);

    // Add relation: B depends on A
    int result = wiring_diagram_add_relation(
        wd_, "ModuleB", "ModuleA", WIRING_RELATION_DEPENDS_ON
    );
    EXPECT_EQ(result, 0) << "add_relation should succeed";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_add_relation_sends_to) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    wiring_module_config_t config1, config2;
    InitTestModuleConfig(&config1, "Sender");
    InitTestModuleConfig(&config2, "Receiver");

    ASSERT_EQ(wiring_diagram_add_module(wd_, "Sender", &config1), 0);
    ASSERT_EQ(wiring_diagram_add_module(wd_, "Receiver", &config2), 0);

    int result = wiring_diagram_add_relation(
        wd_, "Sender", "Receiver", WIRING_RELATION_SENDS_TO
    );
    EXPECT_EQ(result, 0) << "add_relation SENDS_TO should succeed";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_add_relation_null_params) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    EXPECT_EQ(wiring_diagram_add_relation(nullptr, "A", "B", WIRING_RELATION_DEPENDS_ON), -1);
    EXPECT_EQ(wiring_diagram_add_relation(wd_, nullptr, "B", WIRING_RELATION_DEPENDS_ON), -1);
    EXPECT_EQ(wiring_diagram_add_relation(wd_, "A", nullptr, WIRING_RELATION_DEPENDS_ON), -1);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_add_handler) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    wiring_module_config_t config;
    InitTestModuleConfig(&config, "HandlerModule");
    ASSERT_EQ(wiring_diagram_add_module(wd_, "HandlerModule", &config), 0);

    int result = wiring_diagram_add_handler(
        wd_, "HandlerModule", BIO_MSG_BRAIN_STATE_QUERY
    );
    EXPECT_EQ(result, 0) << "add_handler should succeed";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_add_handler_multiple) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    wiring_module_config_t config;
    InitTestModuleConfig(&config, "MultiHandler");
    ASSERT_EQ(wiring_diagram_add_module(wd_, "MultiHandler", &config), 0);

    EXPECT_EQ(wiring_diagram_add_handler(wd_, "MultiHandler", BIO_MSG_BRAIN_STATE_QUERY), 0);
    EXPECT_EQ(wiring_diagram_add_handler(wd_, "MultiHandler", BIO_MSG_BRAIN_STATE_RESPONSE), 0);
    EXPECT_EQ(wiring_diagram_add_handler(wd_, "MultiHandler", BIO_MSG_NEURON_ACTIVATION_REQUEST), 0);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_add_handler_null_params) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    EXPECT_EQ(wiring_diagram_add_handler(nullptr, "Test", BIO_MSG_BRAIN_STATE_QUERY), -1);
    EXPECT_EQ(wiring_diagram_add_handler(wd_, nullptr, BIO_MSG_BRAIN_STATE_QUERY), -1);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_remove_module) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    wiring_module_config_t config;
    InitTestModuleConfig(&config, "ToRemove");
    ASSERT_EQ(wiring_diagram_add_module(wd_, "ToRemove", &config), 0);

    // Verify it exists
    wiring_module_config_t retrieved;
    wiring_module_config_init(&retrieved);
    ASSERT_EQ(wiring_diagram_get_module_config(wd_, "ToRemove", &retrieved), 0);
    wiring_module_config_cleanup(&retrieved);

    // Remove it
    int result = wiring_diagram_remove_module(wd_, "ToRemove");
    EXPECT_EQ(result, 0) << "remove_module should succeed";

    // Verify it's gone
    wiring_module_config_init(&retrieved);
    result = wiring_diagram_get_module_config(wd_, "ToRemove", &retrieved);
    EXPECT_EQ(result, -1) << "Module should not be found after removal";
    wiring_module_config_cleanup(&retrieved);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_remove_module_not_found) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_remove_module(wd_, "NonExistent");
    EXPECT_EQ(result, -1) << "remove_module should return -1 for non-existent module";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_set_enabled) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    wiring_module_config_t config;
    InitTestModuleConfig(&config, "EnableTest");
    config.enabled = true;
    ASSERT_EQ(wiring_diagram_add_module(wd_, "EnableTest", &config), 0);

    // Disable the module
    int result = wiring_diagram_set_enabled(wd_, "EnableTest", false);
    EXPECT_EQ(result, 0) << "set_enabled should succeed";

    // Verify it's disabled
    wiring_module_config_t retrieved;
    wiring_module_config_init(&retrieved);
    ASSERT_EQ(wiring_diagram_get_module_config(wd_, "EnableTest", &retrieved), 0);
    EXPECT_FALSE(retrieved.enabled);
    wiring_module_config_cleanup(&retrieved);

    // Re-enable
    result = wiring_diagram_set_enabled(wd_, "EnableTest", true);
    EXPECT_EQ(result, 0);

    wiring_module_config_init(&retrieved);
    ASSERT_EQ(wiring_diagram_get_module_config(wd_, "EnableTest", &retrieved), 0);
    EXPECT_TRUE(retrieved.enabled);
    wiring_module_config_cleanup(&retrieved);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_set_enabled_not_found) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_set_enabled(wd_, "NonExistent", true);
    EXPECT_EQ(result, -1) << "set_enabled should return -1 for non-existent module";
}

//=============================================================================
// 5. PERSISTENCE TESTS
//=============================================================================

TEST_F(WiringDiagramTest, test_wiring_diagram_auto_persist_default_enabled) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    EXPECT_TRUE(wiring_diagram_get_auto_persist(wd_));
}

TEST_F(WiringDiagramTest, test_wiring_diagram_set_auto_persist) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    wiring_diagram_set_auto_persist(wd_, false);
    EXPECT_FALSE(wiring_diagram_get_auto_persist(wd_));

    wiring_diagram_set_auto_persist(wd_, true);
    EXPECT_TRUE(wiring_diagram_get_auto_persist(wd_));
}

TEST_F(WiringDiagramTest, test_wiring_diagram_persist_and_reload) {
    // Create and populate diagram
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    wiring_module_config_t config;
    InitTestModuleConfig(&config, "PersistentModule");
    // Module is added with subsystem WIRING_SUBSYSTEM_CORE
    ASSERT_EQ(wiring_diagram_add_module(wd_, "PersistentModule", &config), 0);

    // Explicitly persist (saves to subsystems/core.jsonl based on module subsystem)
    int result = wiring_diagram_persist(wd_);
    EXPECT_EQ(result, 0) << "persist should succeed";

    // Destroy and recreate
    wiring_diagram_destroy(wd_);

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // Load the core subsystem where the module was persisted
    result = wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_CORE);
    EXPECT_EQ(result, 0);

    // Verify module was persisted and can be retrieved
    wiring_module_config_t retrieved;
    wiring_module_config_init(&retrieved);
    result = wiring_diagram_get_module_config(wd_, "PersistentModule", &retrieved);
    EXPECT_EQ(result, 0) << "Module should be found after persistence and reload";
    if (result == 0) {
        EXPECT_STREQ(retrieved.module_name, "PersistentModule");
    }
    wiring_module_config_cleanup(&retrieved);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_persist_null) {
    EXPECT_EQ(wiring_diagram_persist(nullptr), -1);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_persist_subsystem) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_persist_subsystem(wd_, WIRING_SUBSYSTEM_ETHICS);
    // Should succeed even with empty subsystem
    EXPECT_EQ(result, 0);
}

//=============================================================================
// 6. UTILITY TESTS
//=============================================================================

TEST_F(WiringDiagramTest, test_wiring_subsystem_to_string) {
    EXPECT_STREQ(wiring_subsystem_to_string(WIRING_SUBSYSTEM_CORE), "core");
    EXPECT_STREQ(wiring_subsystem_to_string(WIRING_SUBSYSTEM_ETHICS), "ethics");
    EXPECT_STREQ(wiring_subsystem_to_string(WIRING_SUBSYSTEM_PERCEPTION), "perception");
    EXPECT_STREQ(wiring_subsystem_to_string(WIRING_SUBSYSTEM_COGNITION), "cognition");
    EXPECT_STREQ(wiring_subsystem_to_string(WIRING_SUBSYSTEM_MEMORY), "memory");
    EXPECT_STREQ(wiring_subsystem_to_string(WIRING_SUBSYSTEM_EMOTION), "emotion");
    EXPECT_STREQ(wiring_subsystem_to_string(WIRING_SUBSYSTEM_IMMUNE), "immune");
    EXPECT_STREQ(wiring_subsystem_to_string(WIRING_SUBSYSTEM_PLASTICITY), "plasticity");
    EXPECT_STREQ(wiring_subsystem_to_string(WIRING_SUBSYSTEM_RECURSIVE), "recursive");
    EXPECT_STREQ(wiring_subsystem_to_string(WIRING_SUBSYSTEM_SOCIAL), "social");

    // Invalid subsystem
    const char* unknown = wiring_subsystem_to_string((wiring_subsystem_t)999);
    EXPECT_NE(unknown, nullptr);
}

TEST_F(WiringDiagramTest, test_wiring_relation_to_string) {
    EXPECT_STREQ(wiring_relation_to_string(WIRING_RELATION_DEPENDS_ON), "DEPENDS_ON");
    EXPECT_STREQ(wiring_relation_to_string(WIRING_RELATION_SENDS_TO), "SENDS_TO");
    EXPECT_STREQ(wiring_relation_to_string(WIRING_RELATION_RECEIVES_FROM), "RECEIVES_FROM");
    EXPECT_STREQ(wiring_relation_to_string(WIRING_RELATION_HANDLES_MESSAGE), "HANDLES_MESSAGE");
    EXPECT_STREQ(wiring_relation_to_string(WIRING_RELATION_BELONGS_TO), "BELONGS_TO");
    EXPECT_STREQ(wiring_relation_to_string(WIRING_RELATION_REQUIRES_HW), "REQUIRES_HW");
    EXPECT_STREQ(wiring_relation_to_string(WIRING_RELATION_AVAILABLE_ON_TIER), "AVAILABLE_ON_TIER");

    // Invalid relation
    const char* unknown = wiring_relation_to_string((wiring_relation_type_t)999);
    EXPECT_NE(unknown, nullptr);
}

TEST_F(WiringDiagramTest, test_wiring_hardware_flags_to_string) {
    char buffer[256];

    // Test single flags (implementation uses lowercase names)
    wiring_hardware_flags_to_string(WIRING_HW_CUDA, buffer, sizeof(buffer));
    EXPECT_NE(strstr(buffer, "cuda"), nullptr);

    wiring_hardware_flags_to_string(WIRING_HW_ROCM, buffer, sizeof(buffer));
    EXPECT_NE(strstr(buffer, "rocm"), nullptr);

    wiring_hardware_flags_to_string(WIRING_HW_LOIHI, buffer, sizeof(buffer));
    EXPECT_NE(strstr(buffer, "loihi"), nullptr);

    // Test NONE
    wiring_hardware_flags_to_string(WIRING_HW_NONE, buffer, sizeof(buffer));
    EXPECT_NE(buffer[0], '\0');

    // Test combined flags
    wiring_hardware_flags_to_string(
        (wiring_hardware_flags_t)(WIRING_HW_CUDA | WIRING_HW_ROCM),
        buffer, sizeof(buffer)
    );
    EXPECT_NE(strstr(buffer, "cuda"), nullptr);
    EXPECT_NE(strstr(buffer, "rocm"), nullptr);
}

TEST_F(WiringDiagramTest, test_wiring_hardware_flags_to_string_small_buffer) {
    char small_buffer[8];
    const char* result = wiring_hardware_flags_to_string(
        WIRING_HW_CUDA, small_buffer, sizeof(small_buffer)
    );
    EXPECT_EQ(result, small_buffer);
    // Buffer should be null-terminated even if truncated
    EXPECT_LT(strlen(small_buffer), sizeof(small_buffer));
}

TEST_F(WiringDiagramTest, test_wiring_module_config_init_cleanup) {
    wiring_module_config_t config;

    // Test init - implementation uses sensible defaults:
    // - min_tier = PLATFORM_TIER_MINIMAL (modules are minimal by default)
    // - enabled = true (modules are enabled by default)
    wiring_module_config_init(&config);
    EXPECT_EQ(config.module_name[0], '\0');
    EXPECT_EQ(config.module_id, 0);
    EXPECT_EQ(config.subsystem, WIRING_SUBSYSTEM_CORE);
    EXPECT_EQ(config.depends_on, nullptr);
    EXPECT_EQ(config.depends_on_count, 0u);
    EXPECT_EQ(config.sends_to, nullptr);
    EXPECT_EQ(config.sends_to_count, 0u);
    EXPECT_EQ(config.receives_from, nullptr);
    EXPECT_EQ(config.receives_from_count, 0u);
    EXPECT_EQ(config.handles_messages, nullptr);
    EXPECT_EQ(config.handles_message_count, 0u);
    EXPECT_EQ(config.min_tier, PLATFORM_TIER_MINIMAL);
    EXPECT_EQ(config.required_hw, WIRING_HW_NONE);
    EXPECT_TRUE(config.enabled);
    EXPECT_FALSE(config.discovered);

    // Test cleanup (should be safe on zeroed struct)
    wiring_module_config_cleanup(&config);
    SUCCEED();
}

TEST_F(WiringDiagramTest, test_wiring_diagram_get_stats) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    // Add some modules
    wiring_module_config_t config;
    InitTestModuleConfig(&config, "Stats1");
    ASSERT_EQ(wiring_diagram_add_module(wd_, "Stats1", &config), 0);

    InitTestModuleConfig(&config, "Stats2");
    config.enabled = false;  // Disabled module
    ASSERT_EQ(wiring_diagram_add_module(wd_, "Stats2", &config), 0);

    InitTestModuleConfig(&config, "Stats3");
    ASSERT_EQ(wiring_diagram_add_module(wd_, "Stats3", &config), 0);

    // Add a relation
    ASSERT_EQ(wiring_diagram_add_relation(wd_, "Stats3", "Stats1", WIRING_RELATION_DEPENDS_ON), 0);

    uint32_t total_modules = 0, enabled_modules = 0, total_relations = 0;
    int result = wiring_diagram_get_stats(wd_, &total_modules, &enabled_modules, &total_relations);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(total_modules, 3u);
    EXPECT_EQ(enabled_modules, 2u);  // Stats2 is disabled
    EXPECT_GE(total_relations, 1u);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_get_stats_null_params) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    uint32_t val = 0;

    // NULL wiring diagram
    EXPECT_EQ(wiring_diagram_get_stats(nullptr, &val, &val, &val), -1);

    // NULL output params should still work (partial retrieval)
    int result = wiring_diagram_get_stats(wd_, nullptr, nullptr, nullptr);
    // Implementation may return 0 or -1 depending on design
    (void)result;
}

//=============================================================================
// 7. HARDWARE DETECTION TESTS
//=============================================================================

TEST_F(WiringDiagramTest, test_wiring_detect_hardware_profile) {
    wiring_hardware_profile_t profile;
    memset(&profile, 0, sizeof(profile));

    int result = wiring_detect_hardware_profile(&profile);
    EXPECT_EQ(result, 0) << "detect_hardware_profile should succeed";

    // Verify sensible values
    EXPECT_GE(profile.cpu_cores, 1u);
    EXPECT_GE(profile.memory_mb, 64u);  // At least 64MB
    EXPECT_GE((int)profile.tier, (int)PLATFORM_TIER_BASIC);
    EXPECT_LE((int)profile.tier, (int)PLATFORM_TIER_FULL);
}

TEST_F(WiringDiagramTest, test_wiring_detect_hardware_profile_null) {
    int result = wiring_detect_hardware_profile(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(WiringDiagramTest, test_wiring_get_default_profile) {
    wiring_hardware_profile_t profile;
    memset(&profile, 0xFF, sizeof(profile));  // Fill with junk

    wiring_get_default_profile(&profile);

    // Verify conservative defaults
    EXPECT_EQ(profile.tier, PLATFORM_TIER_CONSTRAINED);
    EXPECT_EQ(profile.hw_flags, WIRING_HW_NONE);
    EXPECT_GT(profile.memory_mb, 0u);
    EXPECT_GT(profile.cpu_cores, 0u);
    EXPECT_EQ(profile.gpu_compute_units, 0u);
    EXPECT_EQ(profile.neuromorphic_cores, 0u);
}

TEST_F(WiringDiagramTest, test_wiring_get_default_profile_null_safe) {
    // Should not crash with NULL
    wiring_get_default_profile(nullptr);
    SUCCEED();
}

//=============================================================================
// 8. EDGE CASES AND ERROR HANDLING
//=============================================================================

TEST_F(WiringDiagramTest, test_wiring_diagram_operations_on_null) {
    // All operations with NULL wiring diagram should fail gracefully
    EXPECT_EQ(wiring_diagram_load_master(nullptr), -1);
    EXPECT_EQ(wiring_diagram_load_subsystem(nullptr, WIRING_SUBSYSTEM_CORE), -1);
    EXPECT_EQ(wiring_diagram_load_platform(nullptr, PLATFORM_TIER_FULL), -1);
    EXPECT_EQ(wiring_diagram_load_hardware(nullptr, WIRING_HW_CUDA), -1);

    const char* modules[10];
    EXPECT_EQ(wiring_diagram_get_subsystem_modules(nullptr, WIRING_SUBSYSTEM_CORE, modules, 10), 0u);

    wiring_hardware_profile_t profile;
    EXPECT_FALSE(wiring_diagram_module_available(nullptr, "test", &profile));
}

TEST_F(WiringDiagramTest, test_wiring_diagram_module_name_boundary) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    // Test with maximum length name (63 chars + null)
    char long_name[65];
    memset(long_name, 'A', 63);
    long_name[63] = '\0';

    wiring_module_config_t config;
    InitTestModuleConfig(&config, long_name);

    int result = wiring_diagram_add_module(wd_, long_name, &config);
    EXPECT_EQ(result, 0) << "Should accept maximum length name";

    // Verify retrieval
    wiring_module_config_t retrieved;
    wiring_module_config_init(&retrieved);
    result = wiring_diagram_get_module_config(wd_, long_name, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(retrieved.module_name, long_name);
    wiring_module_config_cleanup(&retrieved);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_empty_module_name) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    wiring_module_config_t config;
    wiring_module_config_init(&config);

    // Empty name should fail
    int result = wiring_diagram_add_module(wd_, "", &config);
    EXPECT_EQ(result, -1) << "Empty name should fail";
}

TEST_F(WiringDiagramTest, test_wiring_diagram_concurrent_access) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Create threads that add modules concurrently
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, i, &success_count]() {
            for (int j = 0; j < 10; j++) {
                std::string name = "ConcurrentModule_" + std::to_string(i) + "_" + std::to_string(j);
                wiring_module_config_t config;
                wiring_module_config_init(&config);
                strncpy(config.module_name, name.c_str(), sizeof(config.module_name) - 1);
                config.enabled = true;

                if (wiring_diagram_add_module(wd_, name.c_str(), &config) == 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All 40 additions should succeed (thread-safe implementation)
    EXPECT_EQ(success_count.load(), 40);

    // Verify modules exist
    const char* modules[50];
    uint32_t count = wiring_diagram_get_all_modules(wd_, modules, 50);
    EXPECT_EQ(count, 40u);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_get_module_config_by_id) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);
    wiring_diagram_set_auto_persist(wd_, false);

    wiring_module_config_t config;
    InitTestModuleConfig(&config, "IdTestModule");
    config.module_id = BIO_MODULE_ATTENTION;
    ASSERT_EQ(wiring_diagram_add_module(wd_, "IdTestModule", &config), 0);

    wiring_module_config_t retrieved;
    wiring_module_config_init(&retrieved);

    int result = wiring_diagram_get_module_config_by_id(wd_, BIO_MODULE_ATTENTION, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(retrieved.module_name, "IdTestModule");

    wiring_module_config_cleanup(&retrieved);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_get_module_config_by_id_not_found) {
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    wiring_module_config_t config;
    wiring_module_config_init(&config);

    int result = wiring_diagram_get_module_config_by_id(wd_, BIO_MODULE_INTROSPECTION, &config);
    EXPECT_EQ(result, -1);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_all_subsystems) {
    // Create subsystem files
    CreateTestSubsystemFile("ethics");
    CreateTestSubsystemFile("perception");

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_load_all_subsystems(wd_);
    EXPECT_EQ(result, 0);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_custom) {
    // Create custom wiring file
    std::string filename = temp_dir_ + "/custom/custom_override.jsonl";
    std::ofstream file(filename);
    file << R"({"type":"entity","name":"CustomModule","entityType":"CognitiveModule"})" << "\n";
    file.close();

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_load_custom(wd_, "custom_override.jsonl");
    EXPECT_EQ(result, 0);
}

TEST_F(WiringDiagramTest, test_wiring_diagram_load_all_custom) {
    // Create multiple custom files
    std::ofstream file1(temp_dir_ + "/custom/override1.jsonl");
    file1 << R"({"type":"entity","name":"Custom1","entityType":"CognitiveModule"})" << "\n";
    file1.close();

    std::ofstream file2(temp_dir_ + "/custom/override2.jsonl");
    file2 << R"({"type":"entity","name":"Custom2","entityType":"CognitiveModule"})" << "\n";
    file2.close();

    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    int result = wiring_diagram_load_all_custom(wd_);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// 9. HARDWARE FLAG COMBINATIONS
//=============================================================================

TEST_F(WiringDiagramTest, test_wiring_hardware_flag_any_gpu) {
    // Test WIRING_HW_ANY_GPU includes all GPU types
    EXPECT_TRUE(WIRING_HW_ANY_GPU & WIRING_HW_CUDA);
    EXPECT_TRUE(WIRING_HW_ANY_GPU & WIRING_HW_ROCM);
    EXPECT_TRUE(WIRING_HW_ANY_GPU & WIRING_HW_ONEAPI);
    EXPECT_TRUE(WIRING_HW_ANY_GPU & WIRING_HW_METAL);
    EXPECT_TRUE(WIRING_HW_ANY_GPU & WIRING_HW_OPENCL);

    // Should not include neuromorphic
    EXPECT_FALSE(WIRING_HW_ANY_GPU & WIRING_HW_LOIHI);
}

TEST_F(WiringDiagramTest, test_wiring_hardware_flag_any_neuromorphic) {
    // Test WIRING_HW_ANY_NEUROMORPHIC includes all neuromorphic types
    EXPECT_TRUE(WIRING_HW_ANY_NEUROMORPHIC & WIRING_HW_LOIHI);
    EXPECT_TRUE(WIRING_HW_ANY_NEUROMORPHIC & WIRING_HW_SPINNAKER);
    EXPECT_TRUE(WIRING_HW_ANY_NEUROMORPHIC & WIRING_HW_AKIDA);
    EXPECT_TRUE(WIRING_HW_ANY_NEUROMORPHIC & WIRING_HW_BRAINSCALES);

    // Should not include GPU
    EXPECT_FALSE(WIRING_HW_ANY_NEUROMORPHIC & WIRING_HW_CUDA);
}

//=============================================================================
// 10. SUBSYSTEM ENUMERATION BOUNDS
//=============================================================================

TEST_F(WiringDiagramTest, test_wiring_subsystem_count) {
    // Verify WIRING_SUBSYSTEM_COUNT is correct
    EXPECT_EQ(WIRING_SUBSYSTEM_COUNT, 10);

    // Verify we can iterate all subsystems
    for (int i = 0; i < WIRING_SUBSYSTEM_COUNT; i++) {
        const char* name = wiring_subsystem_to_string((wiring_subsystem_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(WiringDiagramTest, test_wiring_relation_count) {
    // Verify WIRING_RELATION_COUNT is correct
    EXPECT_EQ(WIRING_RELATION_COUNT, 7);

    // Verify we can iterate all relations
    for (int i = 0; i < WIRING_RELATION_COUNT; i++) {
        const char* name = wiring_relation_to_string((wiring_relation_type_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}
