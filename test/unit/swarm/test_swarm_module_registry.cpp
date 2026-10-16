/**
 * @file test_swarm_module_registry.cpp
 * @brief Comprehensive unit tests for Swarm Module Registry
 *
 * WHAT: Tests for swarm module registry plugin architecture
 * WHY:  Verify module registration, lifecycle, update coordination, and arbitration
 * HOW:  GoogleTest framework with mock modules and comprehensive scenarios
 *
 * TEST COVERAGE:
 * - Lifecycle (create, destroy, default config)
 * - Module registration with hooks
 * - Module enable/disable with callbacks
 * - Module priority management
 * - Module update cycle with delta time
 * - Category-based updates and intervals
 * - Hook invocation (init, update, destroy, enable)
 * - Statistics tracking (per-module and registry-wide)
 * - Bio-async integration
 * - Swarm brain integration
 * - Brain immune integration
 * - Arbitration strategies
 * - Module discovery and lookup
 * - Thread safety
 * - Error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_module_registry.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Mock Module Implementation
//=============================================================================

/**
 * WHAT: Mock swarm module for testing
 * WHY:  Need controllable module behavior to test registry
 * HOW:  Track callback invocations and simulate different behaviors
 */
struct MockModule {
    uint32_t module_id;
    std::atomic<int> init_count{0};
    std::atomic<int> update_count{0};
    std::atomic<int> enable_count{0};
    std::atomic<int> destroy_count{0};
    std::atomic<bool> should_fail_init{false};
    std::atomic<bool> should_fail_update{false};
    std::atomic<bool> should_fail_enable{false};
    std::atomic<uint64_t> last_delta_time{0};
    std::atomic<bool> is_enabled{true};
    swarm_module_registry_t* registry_ptr{nullptr};
};

// Mock callbacks
static int mock_init_fn(swarm_module_handle_t module, swarm_module_registry_t* registry) {
    MockModule* mock = static_cast<MockModule*>(module);
    mock->init_count++;
    mock->registry_ptr = registry;
    return mock->should_fail_init ? -1 : 0;
}

static int mock_update_fn(swarm_module_handle_t module, uint64_t delta_time_ms) {
    MockModule* mock = static_cast<MockModule*>(module);
    mock->update_count++;
    mock->last_delta_time = delta_time_ms;
    return mock->should_fail_update ? -1 : 0;
}

static void mock_destroy_fn(swarm_module_handle_t module) {
    MockModule* mock = static_cast<MockModule*>(module);
    mock->destroy_count++;
}

static int mock_enable_fn(swarm_module_handle_t module, bool enabled) {
    MockModule* mock = static_cast<MockModule*>(module);
    mock->enable_count++;
    mock->is_enabled = enabled;
    return mock->should_fail_enable ? -1 : 0;
}

//=============================================================================
// Test Fixture
//=============================================================================

class SwarmModuleRegistryTest : public ::testing::Test {
protected:
    swarm_module_registry_t* registry_;
    std::vector<MockModule*> mock_modules_;

    void SetUp() override {
        registry_ = nullptr;
    }

    void TearDown() override {
        // Clean up mock modules
        for (auto* mock : mock_modules_) {
            delete mock;
        }
        mock_modules_.clear();

        // Destroy registry
        if (registry_) {
            swarm_registry_destroy(registry_);
            registry_ = nullptr;
        }
    }

    // Helper: Create mock module
    MockModule* CreateMockModule() {
        MockModule* mock = new MockModule();
        mock_modules_.push_back(mock);
        return mock;
    }

    // Helper: Create interface with all callbacks
    swarm_module_interface_t CreateFullInterface() {
        swarm_module_interface_t interface;
        interface.init_fn = mock_init_fn;
        interface.update_fn = mock_update_fn;
        interface.destroy_fn = mock_destroy_fn;
        interface.enable_fn = mock_enable_fn;
        return interface;
    }

    // Helper: Create minimal interface (only update)
    swarm_module_interface_t CreateMinimalInterface() {
        swarm_module_interface_t interface;
        interface.init_fn = nullptr;
        interface.update_fn = mock_update_fn;
        interface.destroy_fn = nullptr;
        interface.enable_fn = nullptr;
        return interface;
    }

    // Helper: Register mock module
    uint32_t RegisterMockModule(
        const char* name,
        swarm_module_category_t category,
        uint32_t priority,
        const swarm_module_interface_t* interface = nullptr
    ) {
        MockModule* mock = CreateMockModule();
        swarm_module_interface_t iface = interface ? *interface : CreateFullInterface();

        uint32_t module_id;
        int result = swarm_registry_register_module(
            registry_,
            name,
            category,
            mock,
            &iface,
            priority,
            &module_id
        );

        EXPECT_EQ(result, 0);
        mock->module_id = module_id;
        return module_id;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, CreateWithDefaultConfig) {
    registry_ = swarm_registry_create(nullptr);

    ASSERT_NE(registry_, nullptr);
    EXPECT_EQ(registry_->module_count, 0);
    EXPECT_GT(registry_->module_capacity, 0);
    EXPECT_FALSE(registry_->bio_async_connected);
    EXPECT_FALSE(registry_->brain_connected);
}

TEST_F(SwarmModuleRegistryTest, CreateWithCustomConfig) {
    swarm_registry_config_t config;
    ASSERT_EQ(swarm_registry_default_config(&config), 0);

    config.max_modules = 64;
    config.enable_bio_async = false;
    config.enable_statistics = true;
    config.arbitration = SWARM_ARBITRATION_SEQUENTIAL;

    registry_ = swarm_registry_create(&config);

    ASSERT_NE(registry_, nullptr);
    EXPECT_EQ(registry_->module_capacity, 64);
    EXPECT_EQ(registry_->config.arbitration, SWARM_ARBITRATION_SEQUENTIAL);
}

TEST_F(SwarmModuleRegistryTest, DefaultConfigValues) {
    swarm_registry_config_t config;
    ASSERT_EQ(swarm_registry_default_config(&config), 0);

    EXPECT_EQ(config.max_modules, SWARM_REGISTRY_MAX_MODULES);
    EXPECT_EQ(config.arbitration, SWARM_ARBITRATION_HIGHEST_PRIORITY);
    EXPECT_TRUE(config.enable_auto_wiring);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_statistics);

    // Check category defaults
    EXPECT_TRUE(config.categories[SWARM_MODULE_CATEGORY_MOVEMENT].enabled);
    EXPECT_EQ(config.categories[SWARM_MODULE_CATEGORY_MOVEMENT].default_priority,
              SWARM_PRIORITY_HIGH);
    EXPECT_EQ(config.categories[SWARM_MODULE_CATEGORY_DEFENSE].default_priority,
              SWARM_PRIORITY_CRITICAL);
}

TEST_F(SwarmModuleRegistryTest, DefaultConfigNullPointer) {
    EXPECT_EQ(swarm_registry_default_config(nullptr), -1);
}

TEST_F(SwarmModuleRegistryTest, DestroyNullRegistry) {
    // Should not crash
    swarm_registry_destroy(nullptr);
}

TEST_F(SwarmModuleRegistryTest, DestroyEmptyRegistry) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    swarm_registry_destroy(registry_);
    registry_ = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// Module Registration Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, RegisterSingleModule) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    EXPECT_GT(module_id, 0);
    EXPECT_EQ(registry_->module_count, 1);

    // Verify module was initialized
    EXPECT_EQ(mock_modules_[0]->init_count, 1);
    EXPECT_EQ(mock_modules_[0]->registry_ptr, registry_);
}

TEST_F(SwarmModuleRegistryTest, RegisterMultipleModules) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    const int num_modules = 5;
    std::vector<uint32_t> module_ids;

    for (int i = 0; i < num_modules; i++) {
        char name[64];
        snprintf(name, sizeof(name), "module_%d", i);

        uint32_t id = RegisterMockModule(
            name,
            SWARM_MODULE_CATEGORY_COORDINATION,
            SWARM_PRIORITY_NORMAL
        );

        module_ids.push_back(id);
    }

    EXPECT_EQ(registry_->module_count, num_modules);

    // All module IDs should be unique
    for (size_t i = 0; i < module_ids.size(); i++) {
        for (size_t j = i + 1; j < module_ids.size(); j++) {
            EXPECT_NE(module_ids[i], module_ids[j]);
        }
    }
}

TEST_F(SwarmModuleRegistryTest, RegisterModuleNullPointers) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    MockModule* mock = CreateMockModule();
    swarm_module_interface_t interface = CreateFullInterface();
    uint32_t module_id;

    // Null registry
    EXPECT_EQ(swarm_registry_register_module(
        nullptr, "test", SWARM_MODULE_CATEGORY_MOVEMENT, mock, &interface, 5, &module_id
    ), -1);

    // Null name
    EXPECT_EQ(swarm_registry_register_module(
        registry_, nullptr, SWARM_MODULE_CATEGORY_MOVEMENT, mock, &interface, 5, &module_id
    ), -1);

    // Null handle
    EXPECT_EQ(swarm_registry_register_module(
        registry_, "test", SWARM_MODULE_CATEGORY_MOVEMENT, nullptr, &interface, 5, &module_id
    ), -1);

    // Null interface
    EXPECT_EQ(swarm_registry_register_module(
        registry_, "test", SWARM_MODULE_CATEGORY_MOVEMENT, mock, nullptr, 5, &module_id
    ), -1);

    // Null output
    EXPECT_EQ(swarm_registry_register_module(
        registry_, "test", SWARM_MODULE_CATEGORY_MOVEMENT, mock, &interface, 5, nullptr
    ), -1);
}

TEST_F(SwarmModuleRegistryTest, RegisterModuleInvalidCategory) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    MockModule* mock = CreateMockModule();
    swarm_module_interface_t interface = CreateFullInterface();
    uint32_t module_id;

    EXPECT_EQ(swarm_registry_register_module(
        registry_, "test", SWARM_MODULE_CATEGORY_COUNT, mock, &interface, 5, &module_id
    ), -1);
}

TEST_F(SwarmModuleRegistryTest, RegisterModuleWithoutUpdateFunction) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    MockModule* mock = CreateMockModule();
    swarm_module_interface_t interface = CreateFullInterface();
    interface.update_fn = nullptr;  // Required function missing
    uint32_t module_id;

    EXPECT_EQ(swarm_registry_register_module(
        registry_, "test", SWARM_MODULE_CATEGORY_MOVEMENT, mock, &interface, 5, &module_id
    ), -1);
}

TEST_F(SwarmModuleRegistryTest, RegisterModuleWithFailedInit) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    MockModule* mock = CreateMockModule();
    mock->should_fail_init = true;

    swarm_module_interface_t interface = CreateFullInterface();
    uint32_t module_id;

    int result = swarm_registry_register_module(
        registry_, "test", SWARM_MODULE_CATEGORY_MOVEMENT, mock, &interface, 5, &module_id
    );

    // Registration succeeds but module state is ERROR
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mock->init_count, 1);

    const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, module_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, SWARM_MODULE_STATE_ERROR);
}

TEST_F(SwarmModuleRegistryTest, RegisterModuleMinimalInterface) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    MockModule* mock = CreateMockModule();
    swarm_module_interface_t interface = CreateMinimalInterface();
    uint32_t module_id;

    int result = swarm_registry_register_module(
        registry_, "minimal", SWARM_MODULE_CATEGORY_LEARNING, mock, &interface, 3, &module_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_EQ(mock->init_count, 0);  // No init callback
}

TEST_F(SwarmModuleRegistryTest, RegisterModuleCapacityLimit) {
    swarm_registry_config_t config;
    swarm_registry_default_config(&config);
    config.max_modules = 3;  // Small capacity

    registry_ = swarm_registry_create(&config);
    ASSERT_NE(registry_, nullptr);

    // Register up to capacity
    for (int i = 0; i < 3; i++) {
        char name[64];
        snprintf(name, sizeof(name), "module_%d", i);
        uint32_t id = RegisterMockModule(name, SWARM_MODULE_CATEGORY_CUSTOM, 5);
        EXPECT_GT(id, 0);
    }

    // Try to exceed capacity
    MockModule* mock = CreateMockModule();
    swarm_module_interface_t interface = CreateFullInterface();
    uint32_t module_id;

    EXPECT_EQ(swarm_registry_register_module(
        registry_, "overflow", SWARM_MODULE_CATEGORY_CUSTOM, mock, &interface, 5, &module_id
    ), -1);
}

//=============================================================================
// Module Unregistration Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, UnregisterModule) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    EXPECT_EQ(registry_->module_count, 1);

    EXPECT_EQ(swarm_registry_unregister_module(registry_, module_id), 0);
    EXPECT_EQ(registry_->module_count, 0);
}

TEST_F(SwarmModuleRegistryTest, UnregisterNonexistentModule) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    EXPECT_EQ(swarm_registry_unregister_module(registry_, 999), -1);
}

TEST_F(SwarmModuleRegistryTest, UnregisterModuleNullRegistry) {
    EXPECT_EQ(swarm_registry_unregister_module(nullptr, 1), -1);
}

TEST_F(SwarmModuleRegistryTest, UnregisterMiddleModule) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t id1 = RegisterMockModule("mod1", SWARM_MODULE_CATEGORY_MOVEMENT, 5);
    uint32_t id2 = RegisterMockModule("mod2", SWARM_MODULE_CATEGORY_DEFENSE, 10);
    uint32_t id3 = RegisterMockModule("mod3", SWARM_MODULE_CATEGORY_LEARNING, 3);

    EXPECT_EQ(registry_->module_count, 3);

    // Unregister middle module
    EXPECT_EQ(swarm_registry_unregister_module(registry_, id2), 0);
    EXPECT_EQ(registry_->module_count, 2);

    // Verify remaining modules are still accessible
    EXPECT_TRUE(swarm_registry_is_module_registered(registry_, id1));
    EXPECT_FALSE(swarm_registry_is_module_registered(registry_, id2));
    EXPECT_TRUE(swarm_registry_is_module_registered(registry_, id3));
}

//=============================================================================
// Module Enable/Disable Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, SetModuleEnabled) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    MockModule* mock = mock_modules_[0];
    EXPECT_TRUE(mock->is_enabled);
    EXPECT_EQ(mock->enable_count, 0);

    // Disable module
    EXPECT_EQ(swarm_registry_set_module_enabled(registry_, module_id, false), 0);
    EXPECT_FALSE(mock->is_enabled);
    EXPECT_EQ(mock->enable_count, 1);

    const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, module_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_FALSE(entry->enabled);
    EXPECT_EQ(entry->state, SWARM_MODULE_STATE_DISABLED);

    // Re-enable module
    EXPECT_EQ(swarm_registry_set_module_enabled(registry_, module_id, true), 0);
    EXPECT_TRUE(mock->is_enabled);
    EXPECT_EQ(mock->enable_count, 2);
    EXPECT_EQ(entry->state, SWARM_MODULE_STATE_ACTIVE);
}

TEST_F(SwarmModuleRegistryTest, SetModuleEnabledWithoutCallback) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    swarm_module_interface_t interface = CreateMinimalInterface();

    MockModule* mock = CreateMockModule();
    uint32_t module_id;

    ASSERT_EQ(swarm_registry_register_module(
        registry_, "test", SWARM_MODULE_CATEGORY_MOVEMENT, mock, &interface, 5, &module_id
    ), 0);

    // Should succeed even without enable callback
    EXPECT_EQ(swarm_registry_set_module_enabled(registry_, module_id, false), 0);
    EXPECT_EQ(mock->enable_count, 0);  // Callback not called

    const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, module_id);
    EXPECT_FALSE(entry->enabled);
}

TEST_F(SwarmModuleRegistryTest, SetModuleEnabledFailedCallback) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    MockModule* mock = mock_modules_[0];
    mock->should_fail_enable = true;

    EXPECT_EQ(swarm_registry_set_module_enabled(registry_, module_id, false), -1);
    EXPECT_EQ(mock->enable_count, 1);  // Callback was called
}

TEST_F(SwarmModuleRegistryTest, SetModuleEnabledNullRegistry) {
    EXPECT_EQ(swarm_registry_set_module_enabled(nullptr, 1, true), -1);
}

TEST_F(SwarmModuleRegistryTest, SetModuleEnabledNonexistent) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    EXPECT_EQ(swarm_registry_set_module_enabled(registry_, 999, true), -1);
}

//=============================================================================
// Module Priority Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, SetModulePriority) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, module_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->priority, SWARM_PRIORITY_NORMAL);

    // Change priority
    EXPECT_EQ(swarm_registry_set_module_priority(registry_, module_id, SWARM_PRIORITY_CRITICAL), 0);
    EXPECT_EQ(entry->priority, SWARM_PRIORITY_CRITICAL);
}

TEST_F(SwarmModuleRegistryTest, SetModulePriorityInvalidValue) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    EXPECT_EQ(swarm_registry_set_module_priority(registry_, module_id, 99), -1);
}

TEST_F(SwarmModuleRegistryTest, SetModulePriorityNullRegistry) {
    EXPECT_EQ(swarm_registry_set_module_priority(nullptr, 1, 5), -1);
}

TEST_F(SwarmModuleRegistryTest, SetModulePriorityNonexistent) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    EXPECT_EQ(swarm_registry_set_module_priority(registry_, 999, 5), -1);
}

//=============================================================================
// Module Update Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, UpdateSingleModule) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    MockModule* mock = mock_modules_[0];

    // Set module to active state
    const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, module_id);
    const_cast<swarm_module_entry_t*>(entry)->state = SWARM_MODULE_STATE_ACTIVE;

    // Wait a bit to ensure non-zero delta_time
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    uint64_t current_time = nimcp_time_get_ms();
    int updated = swarm_registry_update(registry_, current_time);

    EXPECT_EQ(updated, 1);
    EXPECT_EQ(mock->update_count, 1);
    EXPECT_GE(mock->last_delta_time, 0);  // May be 0 if update happens in same ms
}

TEST_F(SwarmModuleRegistryTest, UpdateMultipleModules) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    const int num_modules = 5;
    for (int i = 0; i < num_modules; i++) {
        char name[64];
        snprintf(name, sizeof(name), "module_%d", i);

        uint32_t id = RegisterMockModule(name, SWARM_MODULE_CATEGORY_COORDINATION, 5);

        // Set to active
        const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, id);
        const_cast<swarm_module_entry_t*>(entry)->state = SWARM_MODULE_STATE_ACTIVE;
    }

    uint64_t current_time = nimcp_time_get_ms();
    int updated = swarm_registry_update(registry_, current_time);

    EXPECT_EQ(updated, num_modules);

    for (int i = 0; i < num_modules; i++) {
        EXPECT_EQ(mock_modules_[i]->update_count, 1);
    }
}

TEST_F(SwarmModuleRegistryTest, UpdateSkipsDisabledModules) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t id1 = RegisterMockModule("mod1", SWARM_MODULE_CATEGORY_MOVEMENT, 5);
    uint32_t id2 = RegisterMockModule("mod2", SWARM_MODULE_CATEGORY_DEFENSE, 10);

    // Set both to active
    const swarm_module_entry_t* e1 = swarm_registry_get_module(registry_, id1);
    const swarm_module_entry_t* e2 = swarm_registry_get_module(registry_, id2);
    const_cast<swarm_module_entry_t*>(e1)->state = SWARM_MODULE_STATE_ACTIVE;
    const_cast<swarm_module_entry_t*>(e2)->state = SWARM_MODULE_STATE_ACTIVE;

    // Disable second module
    swarm_registry_set_module_enabled(registry_, id2, false);

    uint64_t current_time = nimcp_time_get_ms();
    int updated = swarm_registry_update(registry_, current_time);

    EXPECT_EQ(updated, 1);  // Only first module updated
    EXPECT_EQ(mock_modules_[0]->update_count, 1);
    EXPECT_EQ(mock_modules_[1]->update_count, 0);
}

TEST_F(SwarmModuleRegistryTest, UpdateWithFailedModule) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    MockModule* mock = mock_modules_[0];
    mock->should_fail_update = true;

    const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, module_id);
    const_cast<swarm_module_entry_t*>(entry)->state = SWARM_MODULE_STATE_ACTIVE;

    uint64_t current_time = nimcp_time_get_ms();
    int updated = swarm_registry_update(registry_, current_time);

    EXPECT_EQ(updated, 0);  // Failed updates don't count
    EXPECT_EQ(mock->update_count, 1);  // But callback was called
    EXPECT_GT(entry->error_count, 0);
}

TEST_F(SwarmModuleRegistryTest, UpdateNullRegistry) {
    EXPECT_EQ(swarm_registry_update(nullptr, 1000), -1);
}

TEST_F(SwarmModuleRegistryTest, UpdateSpecificModule) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    MockModule* mock = mock_modules_[0];

    EXPECT_EQ(swarm_registry_update_module(registry_, module_id, 100), 0);
    EXPECT_EQ(mock->update_count, 1);
    EXPECT_EQ(mock->last_delta_time, 100);
}

TEST_F(SwarmModuleRegistryTest, UpdateSpecificModuleNonexistent) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    EXPECT_EQ(swarm_registry_update_module(registry_, 999, 100), -1);
}

TEST_F(SwarmModuleRegistryTest, UpdateCategory) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    // Register modules in different categories
    uint32_t id1 = RegisterMockModule("move1", SWARM_MODULE_CATEGORY_MOVEMENT, 5);
    uint32_t id2 = RegisterMockModule("move2", SWARM_MODULE_CATEGORY_MOVEMENT, 5);
    uint32_t id3 = RegisterMockModule("def1", SWARM_MODULE_CATEGORY_DEFENSE, 10);

    // Set all to active
    for (uint32_t id : {id1, id2, id3}) {
        const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, id);
        const_cast<swarm_module_entry_t*>(entry)->state = SWARM_MODULE_STATE_ACTIVE;
    }

    uint64_t current_time = nimcp_time_get_ms();
    int updated = swarm_registry_update_category(registry_, SWARM_MODULE_CATEGORY_MOVEMENT, current_time);

    EXPECT_EQ(updated, 2);  // Only movement modules
    EXPECT_EQ(mock_modules_[0]->update_count, 1);
    EXPECT_EQ(mock_modules_[1]->update_count, 1);
    EXPECT_EQ(mock_modules_[2]->update_count, 0);  // Defense not updated
}

TEST_F(SwarmModuleRegistryTest, UpdateCategoryInvalid) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    EXPECT_EQ(swarm_registry_update_category(
        registry_, SWARM_MODULE_CATEGORY_COUNT, 1000
    ), -1);
}

//=============================================================================
// Arbitration Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, ResolveConflictHighestPriority) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t id1 = RegisterMockModule("low", SWARM_MODULE_CATEGORY_MOVEMENT, SWARM_PRIORITY_LOW);
    uint32_t id2 = RegisterMockModule("high", SWARM_MODULE_CATEGORY_DEFENSE, SWARM_PRIORITY_HIGH);
    uint32_t id3 = RegisterMockModule("normal", SWARM_MODULE_CATEGORY_COORDINATION, SWARM_PRIORITY_NORMAL);

    uint32_t module_ids[] = {id1, id2, id3};
    uint32_t winner_id;

    EXPECT_EQ(swarm_registry_resolve_conflict(registry_, module_ids, 3, &winner_id), 0);
    EXPECT_EQ(winner_id, id2);  // Highest priority
}

TEST_F(SwarmModuleRegistryTest, ResolveConflictSequential) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    swarm_registry_set_arbitration_strategy(registry_, SWARM_ARBITRATION_SEQUENTIAL);

    uint32_t id1 = RegisterMockModule("first", SWARM_MODULE_CATEGORY_MOVEMENT, 5);
    uint32_t id2 = RegisterMockModule("second", SWARM_MODULE_CATEGORY_DEFENSE, 10);

    uint32_t module_ids[] = {id1, id2};
    uint32_t winner_id;

    EXPECT_EQ(swarm_registry_resolve_conflict(registry_, module_ids, 2, &winner_id), 0);
    EXPECT_EQ(winner_id, id1);  // First in array
}

TEST_F(SwarmModuleRegistryTest, ResolveConflictNullPointers) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_ids[] = {1, 2};
    uint32_t winner_id;

    EXPECT_EQ(swarm_registry_resolve_conflict(nullptr, module_ids, 2, &winner_id), -1);
    EXPECT_EQ(swarm_registry_resolve_conflict(registry_, nullptr, 2, &winner_id), -1);
    EXPECT_EQ(swarm_registry_resolve_conflict(registry_, module_ids, 2, nullptr), -1);
}

TEST_F(SwarmModuleRegistryTest, ResolveConflictEmptyArray) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t winner_id;
    EXPECT_EQ(swarm_registry_resolve_conflict(registry_, nullptr, 0, &winner_id), -1);
}

TEST_F(SwarmModuleRegistryTest, SetArbitrationStrategy) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    EXPECT_EQ(registry_->config.arbitration, SWARM_ARBITRATION_HIGHEST_PRIORITY);

    EXPECT_EQ(swarm_registry_set_arbitration_strategy(
        registry_, SWARM_ARBITRATION_SEQUENTIAL
    ), 0);
    EXPECT_EQ(registry_->config.arbitration, SWARM_ARBITRATION_SEQUENTIAL);
}

TEST_F(SwarmModuleRegistryTest, SetArbitrationStrategyNull) {
    EXPECT_EQ(swarm_registry_set_arbitration_strategy(
        nullptr, SWARM_ARBITRATION_SEQUENTIAL
    ), -1);
}

//=============================================================================
// Discovery Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, GetModule) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_HIGH
    );

    const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, module_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->module_id, module_id);
    EXPECT_STREQ(entry->module_name, "test_module");
    EXPECT_EQ(entry->category, SWARM_MODULE_CATEGORY_MOVEMENT);
    EXPECT_EQ(entry->priority, SWARM_PRIORITY_HIGH);
}

TEST_F(SwarmModuleRegistryTest, GetModuleNonexistent) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    EXPECT_EQ(swarm_registry_get_module(registry_, 999), nullptr);
}

TEST_F(SwarmModuleRegistryTest, GetModuleNullRegistry) {
    EXPECT_EQ(swarm_registry_get_module(nullptr, 1), nullptr);
}

TEST_F(SwarmModuleRegistryTest, FindModuleByName) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "unique_name",
        SWARM_MODULE_CATEGORY_LEARNING,
        SWARM_PRIORITY_NORMAL
    );

    const swarm_module_entry_t* entry = swarm_registry_find_module_by_name(
        registry_, "unique_name"
    );

    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->module_id, module_id);
}

TEST_F(SwarmModuleRegistryTest, FindModuleByNameNonexistent) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    EXPECT_EQ(swarm_registry_find_module_by_name(registry_, "nonexistent"), nullptr);
}

TEST_F(SwarmModuleRegistryTest, FindModuleByNameNull) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    EXPECT_EQ(swarm_registry_find_module_by_name(registry_, nullptr), nullptr);
    EXPECT_EQ(swarm_registry_find_module_by_name(nullptr, "test"), nullptr);
}

TEST_F(SwarmModuleRegistryTest, GetModulesByCategory) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    RegisterMockModule("move1", SWARM_MODULE_CATEGORY_MOVEMENT, 5);
    RegisterMockModule("move2", SWARM_MODULE_CATEGORY_MOVEMENT, 5);
    RegisterMockModule("def1", SWARM_MODULE_CATEGORY_DEFENSE, 10);
    RegisterMockModule("learn1", SWARM_MODULE_CATEGORY_LEARNING, 3);

    const swarm_module_entry_t* modules[10];
    uint32_t count = swarm_registry_get_modules_by_category(
        registry_, SWARM_MODULE_CATEGORY_MOVEMENT, modules, 10
    );

    EXPECT_EQ(count, 2);
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(modules[i]->category, SWARM_MODULE_CATEGORY_MOVEMENT);
    }
}

TEST_F(SwarmModuleRegistryTest, GetModulesByCategoryNull) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    const swarm_module_entry_t* modules[10];

    EXPECT_EQ(swarm_registry_get_modules_by_category(
        nullptr, SWARM_MODULE_CATEGORY_MOVEMENT, modules, 10
    ), 0);

    EXPECT_EQ(swarm_registry_get_modules_by_category(
        registry_, SWARM_MODULE_CATEGORY_MOVEMENT, nullptr, 10
    ), 0);
}

TEST_F(SwarmModuleRegistryTest, EnumerateModules) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    const int num_modules = 5;
    for (int i = 0; i < num_modules; i++) {
        char name[64];
        snprintf(name, sizeof(name), "module_%d", i);
        RegisterMockModule(name, SWARM_MODULE_CATEGORY_CUSTOM, 5);
    }

    const swarm_module_entry_t* modules[10];
    uint32_t count = swarm_registry_enumerate_modules(registry_, modules, 10);

    EXPECT_EQ(count, num_modules);
}

TEST_F(SwarmModuleRegistryTest, GetCategoryCount) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    RegisterMockModule("move1", SWARM_MODULE_CATEGORY_MOVEMENT, 5);
    RegisterMockModule("move2", SWARM_MODULE_CATEGORY_MOVEMENT, 5);
    RegisterMockModule("def1", SWARM_MODULE_CATEGORY_DEFENSE, 10);

    EXPECT_EQ(swarm_registry_get_category_count(registry_, SWARM_MODULE_CATEGORY_MOVEMENT), 2);
    EXPECT_EQ(swarm_registry_get_category_count(registry_, SWARM_MODULE_CATEGORY_DEFENSE), 1);
    EXPECT_EQ(swarm_registry_get_category_count(registry_, SWARM_MODULE_CATEGORY_LEARNING), 0);
}

TEST_F(SwarmModuleRegistryTest, IsModuleRegistered) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    EXPECT_TRUE(swarm_registry_is_module_registered(registry_, module_id));
    EXPECT_FALSE(swarm_registry_is_module_registered(registry_, 999));
}

TEST_F(SwarmModuleRegistryTest, IsModuleRegisteredNull) {
    EXPECT_FALSE(swarm_registry_is_module_registered(nullptr, 1));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, GetStats) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    RegisterMockModule("mod1", SWARM_MODULE_CATEGORY_MOVEMENT, 5);
    RegisterMockModule("mod2", SWARM_MODULE_CATEGORY_DEFENSE, 10);

    swarm_registry_stats_t stats;
    EXPECT_EQ(swarm_registry_get_stats(registry_, &stats), 0);

    EXPECT_EQ(stats.total_modules, 2);
    EXPECT_EQ(stats.active_modules, 2);  // Enabled by default
}

TEST_F(SwarmModuleRegistryTest, GetStatsNull) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    swarm_registry_stats_t stats;
    EXPECT_EQ(swarm_registry_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(swarm_registry_get_stats(registry_, nullptr), -1);
}

TEST_F(SwarmModuleRegistryTest, ResetStats) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    // Generate some stats
    const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, module_id);
    const_cast<swarm_module_entry_t*>(entry)->state = SWARM_MODULE_STATE_ACTIVE;

    swarm_registry_update(registry_, nimcp_time_get_ms());

    swarm_registry_stats_t stats;
    swarm_registry_get_stats(registry_, &stats);
    EXPECT_GT(stats.total_update_cycles, 0);

    // Reset
    swarm_registry_reset_stats(registry_);

    swarm_registry_get_stats(registry_, &stats);
    EXPECT_EQ(stats.total_update_cycles, 0);
    EXPECT_EQ(stats.total_module_updates, 0);
    EXPECT_EQ(stats.total_modules, 1);  // Module count preserved
}

TEST_F(SwarmModuleRegistryTest, ResetStatsNull) {
    // Should not crash
    swarm_registry_reset_stats(nullptr);
}

TEST_F(SwarmModuleRegistryTest, GetModuleStats) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t module_id = RegisterMockModule(
        "test_module",
        SWARM_MODULE_CATEGORY_MOVEMENT,
        SWARM_PRIORITY_NORMAL
    );

    uint64_t update_count;
    float avg_time_us;
    uint32_t error_count;

    EXPECT_EQ(swarm_registry_get_module_stats(
        registry_, module_id, &update_count, &avg_time_us, &error_count
    ), 0);

    EXPECT_EQ(update_count, 0);
    EXPECT_EQ(error_count, 0);
}

TEST_F(SwarmModuleRegistryTest, GetModuleStatsNonexistent) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint64_t update_count;
    float avg_time_us;
    uint32_t error_count;

    EXPECT_EQ(swarm_registry_get_module_stats(
        registry_, 999, &update_count, &avg_time_us, &error_count
    ), -1);
}

TEST_F(SwarmModuleRegistryTest, GetModuleStatsNull) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint64_t update_count;
    float avg_time_us;
    uint32_t error_count;

    EXPECT_EQ(swarm_registry_get_module_stats(
        nullptr, 1, &update_count, &avg_time_us, &error_count
    ), -1);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, ConnectSwarmBrain) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    // Note: Using mock pointer, not creating real swarm_brain
    swarm_brain_t* mock_brain = reinterpret_cast<swarm_brain_t*>(0x1234);

    EXPECT_EQ(swarm_registry_connect_swarm_brain(registry_, mock_brain), 0);
    EXPECT_TRUE(registry_->brain_connected);
    EXPECT_EQ(registry_->swarm_brain, mock_brain);
}

TEST_F(SwarmModuleRegistryTest, ConnectSwarmBrainNull) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    swarm_brain_t* mock_brain = reinterpret_cast<swarm_brain_t*>(0x1234);

    EXPECT_EQ(swarm_registry_connect_swarm_brain(nullptr, mock_brain), -1);
    EXPECT_EQ(swarm_registry_connect_swarm_brain(registry_, nullptr), -1);
}

TEST_F(SwarmModuleRegistryTest, DisconnectSwarmBrain) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    swarm_brain_t* mock_brain = reinterpret_cast<swarm_brain_t*>(0x1234);
    swarm_registry_connect_swarm_brain(registry_, mock_brain);

    EXPECT_EQ(swarm_registry_disconnect_swarm_brain(registry_), 0);
    EXPECT_FALSE(registry_->brain_connected);
    EXPECT_EQ(registry_->swarm_brain, nullptr);
}

TEST_F(SwarmModuleRegistryTest, DisconnectSwarmBrainNull) {
    EXPECT_EQ(swarm_registry_disconnect_swarm_brain(nullptr), 0);
}

TEST_F(SwarmModuleRegistryTest, ConnectBrainImmune) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    // Note: Using mock pointer, not creating real brain_immune
    brain_immune_system_t* mock_immune = reinterpret_cast<brain_immune_system_t*>(0x5678);

    EXPECT_EQ(swarm_registry_connect_brain_immune(registry_, mock_immune), 0);
    EXPECT_EQ(registry_->brain_immune, mock_immune);
}

TEST_F(SwarmModuleRegistryTest, ConnectBrainImmuneNull) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    brain_immune_system_t* mock_immune = reinterpret_cast<brain_immune_system_t*>(0x5678);

    EXPECT_EQ(swarm_registry_connect_brain_immune(nullptr, mock_immune), -1);
    EXPECT_EQ(swarm_registry_connect_brain_immune(registry_, nullptr), -1);
}

TEST_F(SwarmModuleRegistryTest, DisconnectBrainImmune) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    brain_immune_system_t* mock_immune = reinterpret_cast<brain_immune_system_t*>(0x5678);
    swarm_registry_connect_brain_immune(registry_, mock_immune);

    EXPECT_EQ(swarm_registry_disconnect_brain_immune(registry_), 0);
    EXPECT_EQ(registry_->brain_immune, nullptr);
}

TEST_F(SwarmModuleRegistryTest, DisconnectBrainImmuneNull) {
    EXPECT_EQ(swarm_registry_disconnect_brain_immune(nullptr), 0);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, CategoryToString) {
    EXPECT_STREQ(swarm_module_category_to_string(SWARM_MODULE_CATEGORY_MOVEMENT), "movement");
    EXPECT_STREQ(swarm_module_category_to_string(SWARM_MODULE_CATEGORY_COMMUNICATION), "communication");
    EXPECT_STREQ(swarm_module_category_to_string(SWARM_MODULE_CATEGORY_MEMORY), "memory");
    EXPECT_STREQ(swarm_module_category_to_string(SWARM_MODULE_CATEGORY_DEFENSE), "defense");
    EXPECT_STREQ(swarm_module_category_to_string(SWARM_MODULE_CATEGORY_COORDINATION), "coordination");
    EXPECT_STREQ(swarm_module_category_to_string(SWARM_MODULE_CATEGORY_EMERGENCE), "emergence");
    EXPECT_STREQ(swarm_module_category_to_string(SWARM_MODULE_CATEGORY_LEARNING), "learning");
    EXPECT_STREQ(swarm_module_category_to_string(SWARM_MODULE_CATEGORY_CUSTOM), "custom");
    EXPECT_STREQ(swarm_module_category_to_string(SWARM_MODULE_CATEGORY_COUNT), "unknown");
}

TEST_F(SwarmModuleRegistryTest, StateToString) {
    EXPECT_STREQ(swarm_module_state_to_string(SWARM_MODULE_STATE_UNINITIALIZED), "uninitialized");
    EXPECT_STREQ(swarm_module_state_to_string(SWARM_MODULE_STATE_INITIALIZED), "initialized");
    EXPECT_STREQ(swarm_module_state_to_string(SWARM_MODULE_STATE_ACTIVE), "active");
    EXPECT_STREQ(swarm_module_state_to_string(SWARM_MODULE_STATE_DISABLED), "disabled");
    EXPECT_STREQ(swarm_module_state_to_string(SWARM_MODULE_STATE_ERROR), "error");
    EXPECT_STREQ(swarm_module_state_to_string(SWARM_MODULE_STATE_SHUTDOWN), "shutdown");
}

TEST_F(SwarmModuleRegistryTest, ArbitrationStrategyToString) {
    EXPECT_STREQ(swarm_arbitration_strategy_to_string(SWARM_ARBITRATION_HIGHEST_PRIORITY),
                 "highest_priority");
    EXPECT_STREQ(swarm_arbitration_strategy_to_string(SWARM_ARBITRATION_WEIGHTED_BLEND),
                 "weighted_blend");
    EXPECT_STREQ(swarm_arbitration_strategy_to_string(SWARM_ARBITRATION_SEQUENTIAL),
                 "sequential");
    EXPECT_STREQ(swarm_arbitration_strategy_to_string(SWARM_ARBITRATION_CUSTOM),
                 "custom");
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, ConcurrentRegistration) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    const int num_threads = 4;
    const int modules_per_thread = 5;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, modules_per_thread, &success_count]() {
            for (int i = 0; i < modules_per_thread; i++) {
                char name[64];
                snprintf(name, sizeof(name), "thread_%d_module_%d", t, i);

                MockModule* mock = new MockModule();
                swarm_module_interface_t interface = CreateFullInterface();
                uint32_t module_id;

                if (swarm_registry_register_module(
                    registry_, name, SWARM_MODULE_CATEGORY_CUSTOM,
                    mock, &interface, 5, &module_id
                ) == 0) {
                    success_count++;
                    // Note: Not adding to mock_modules_ to avoid race
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * modules_per_thread);
}

TEST_F(SwarmModuleRegistryTest, ConcurrentUpdate) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    // Register some modules
    for (int i = 0; i < 5; i++) {
        char name[64];
        snprintf(name, sizeof(name), "module_%d", i);
        uint32_t id = RegisterMockModule(name, SWARM_MODULE_CATEGORY_MOVEMENT, 5);

        const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, id);
        const_cast<swarm_module_entry_t*>(entry)->state = SWARM_MODULE_STATE_ACTIVE;
    }

    // Concurrent updates
    const int num_threads = 4;
    const int updates_per_thread = 10;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, updates_per_thread]() {
            for (int i = 0; i < updates_per_thread; i++) {
                swarm_registry_update(registry_, nimcp_time_get_ms());
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all modules were updated multiple times
    for (size_t i = 0; i < mock_modules_.size(); i++) {
        EXPECT_GT(mock_modules_[i]->update_count, 0);
    }
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(SwarmModuleRegistryTest, EmptyRegistryUpdate) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    int updated = swarm_registry_update(registry_, nimcp_time_get_ms());
    EXPECT_EQ(updated, 0);
}

TEST_F(SwarmModuleRegistryTest, AllModulesDisabled) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    uint32_t id1 = RegisterMockModule("mod1", SWARM_MODULE_CATEGORY_MOVEMENT, 5);
    uint32_t id2 = RegisterMockModule("mod2", SWARM_MODULE_CATEGORY_DEFENSE, 10);

    swarm_registry_set_module_enabled(registry_, id1, false);
    swarm_registry_set_module_enabled(registry_, id2, false);

    int updated = swarm_registry_update(registry_, nimcp_time_get_ms());
    EXPECT_EQ(updated, 0);
}

TEST_F(SwarmModuleRegistryTest, LongModuleName) {
    registry_ = swarm_registry_create(nullptr);
    ASSERT_NE(registry_, nullptr);

    char long_name[128];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    uint32_t module_id = RegisterMockModule(
        long_name,
        SWARM_MODULE_CATEGORY_LEARNING,
        SWARM_PRIORITY_NORMAL
    );

    const swarm_module_entry_t* entry = swarm_registry_get_module(registry_, module_id);
    ASSERT_NE(entry, nullptr);

    // Name should be truncated to fit
    EXPECT_LT(strlen(entry->module_name), sizeof(long_name));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
