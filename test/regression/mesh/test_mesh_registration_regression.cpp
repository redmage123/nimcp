/**
 * @file test_mesh_registration_regression.cpp
 * @brief Regression Tests for Module Registration System
 *
 * WHAT: Tests for module registration stability, edge cases, and scalability
 * WHY:  Catch regressions in module registry behavior under stress
 * HOW:  Test registration at scale, cycles, memory pressure, and error handling
 *
 * TEST COVERAGE:
 * - Registration with 100+ modules doesn't degrade
 * - Repeated register/unregister cycles
 * - Registration under memory pressure
 * - Registration with invalid pointers (graceful failure)
 * - Duplicate detection and handling
 * - Category-based filtering at scale
 * - Magic validation edge cases
 * - Concurrent registration operations
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <random>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_module_registry.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Constants
// =============================================================================

static constexpr size_t SCALE_MODULE_COUNT = 150;
static constexpr size_t REGISTER_UNREGISTER_CYCLES = 50;
static constexpr size_t CONCURRENT_THREADS = 8;
static constexpr uint32_t TEST_MODULE_MAGIC = 0xDEADBEEF;

// =============================================================================
// Mock Module Structure for Testing
// =============================================================================

typedef struct test_mock_module {
    uint32_t magic;
    char name[64];
    float data[16];
    bool initialized;
} test_mock_module_t;

#define test_mock_module_t_MAGIC TEST_MODULE_MAGIC

// =============================================================================
// Test Fixture
// =============================================================================

class MeshRegistrationRegressionTest : public ::testing::Test {
protected:
    mesh_module_registry_t* registry_ = nullptr;
    mesh_bootstrap_t* bootstrap_ = nullptr;
    std::vector<test_mock_module_t*> modules_;

    void SetUp() override {
        mesh_module_registry_config_t config;
        mesh_module_registry_default_config(&config);
        config.max_modules = 512;
        config.require_magic_validation = true;
        config.require_size_validation = true;
        config.enable_duplicate_detection = true;
        config.verbose_logging = false;

        registry_ = mesh_module_registry_create(&config);
        ASSERT_NE(registry_, nullptr);
    }

    void TearDown() override {
        if (registry_) {
            mesh_module_registry_destroy(registry_);
            registry_ = nullptr;
        }
        if (bootstrap_) {
            mesh_bootstrap_destroy(bootstrap_);
            bootstrap_ = nullptr;
        }
        for (auto* mod : modules_) {
            nimcp_free(mod);
        }
        modules_.clear();
    }

    test_mock_module_t* CreateMockModule(const char* name) {
        auto* mod = static_cast<test_mock_module_t*>(
            nimcp_calloc(1, sizeof(test_mock_module_t)));
        if (mod) {
            mod->magic = TEST_MODULE_MAGIC;
            strncpy(mod->name, name, sizeof(mod->name) - 1);
            mod->initialized = true;
            modules_.push_back(mod);
        }
        return mod;
    }

    nimcp_error_t RegisterModule(const char* name, mesh_adapter_category_t category) {
        test_mock_module_t* mod = CreateMockModule(name);
        if (!mod) return NIMCP_ERROR_MEMORY;

        mesh_module_descriptor_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.module_name = name;
        desc.category = category;
        desc.module_instance = mod;
        desc.module_size = sizeof(test_mock_module_t);
        desc.module_magic = TEST_MODULE_MAGIC;
        desc.endorser_role = ENDORSER_ROLE_OPTIONAL;

        return mesh_module_registry_register(registry_, &desc);
    }
};

// =============================================================================
// Test 1: Scale Registration - 100+ Modules
// =============================================================================

TEST_F(MeshRegistrationRegressionTest, ScaleRegistration100PlusModules) {
    // Bug scenario: Registration performance degraded significantly at scale
    auto start = std::chrono::high_resolution_clock::now();

    mesh_adapter_category_t categories[] = {
        MESH_ADAPTER_CATEGORY_COGNITIVE,
        MESH_ADAPTER_CATEGORY_PERCEPTION,
        MESH_ADAPTER_CATEGORY_SUBCORTICAL,
        MESH_ADAPTER_CATEGORY_MOTOR,
        MESH_ADAPTER_CATEGORY_MEMORY,
        MESH_ADAPTER_CATEGORY_SECURITY
    };
    size_t num_categories = sizeof(categories) / sizeof(categories[0]);

    size_t success_count = 0;
    for (size_t i = 0; i < SCALE_MODULE_COUNT; i++) {
        char name[64];
        snprintf(name, sizeof(name), "scale_module_%zu", i);

        mesh_adapter_category_t cat = categories[i % num_categories];
        nimcp_error_t err = RegisterModule(name, cat);

        if (err == NIMCP_SUCCESS) {
            success_count++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // All registrations should succeed
    EXPECT_EQ(success_count, SCALE_MODULE_COUNT)
        << "All scale registrations should succeed";

    // Performance assertion: should complete in reasonable time
    EXPECT_LT(duration_ms, 5000)
        << "Registration of " << SCALE_MODULE_COUNT << " modules took too long: "
        << duration_ms << "ms";

    // Verify registry stats
    mesh_module_registry_stats_t stats;
    mesh_module_registry_get_stats(registry_, &stats);
    EXPECT_EQ(stats.total_registered, SCALE_MODULE_COUNT);

    // Verify lookup performance at scale
    auto lookup_start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < SCALE_MODULE_COUNT; i++) {
        char name[64];
        snprintf(name, sizeof(name), "scale_module_%zu", i);
        const mesh_registered_module_t* found = mesh_module_registry_get(registry_, name);
        EXPECT_NE(found, nullptr) << "Module " << name << " should be found";
    }
    auto lookup_end = std::chrono::high_resolution_clock::now();
    auto lookup_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        lookup_end - lookup_start).count();

    EXPECT_LT(lookup_duration_ms, 1000)
        << "Lookup of " << SCALE_MODULE_COUNT << " modules took too long: "
        << lookup_duration_ms << "ms";
}

// =============================================================================
// Test 2: Repeated Register/Unregister Cycles
// =============================================================================

TEST_F(MeshRegistrationRegressionTest, RepeatedRegisterUnregisterCycles) {
    // Bug scenario: Memory leak or corruption during repeated cycles
    for (size_t cycle = 0; cycle < REGISTER_UNREGISTER_CYCLES; cycle++) {
        // Register 10 modules
        for (int i = 0; i < 10; i++) {
            char name[64];
            snprintf(name, sizeof(name), "cycle_%zu_module_%d", cycle, i);

            nimcp_error_t err = RegisterModule(name, MESH_ADAPTER_CATEGORY_COGNITIVE);
            ASSERT_EQ(err, NIMCP_SUCCESS)
                << "Registration failed at cycle " << cycle << " module " << i;
        }

        // Verify all registered
        mesh_module_registry_stats_t stats;
        mesh_module_registry_get_stats(registry_, &stats);
        size_t expected = (cycle + 1) * 10;
        EXPECT_EQ(stats.total_registered, expected);

        // Unregister half
        for (int i = 0; i < 5; i++) {
            char name[64];
            snprintf(name, sizeof(name), "cycle_%zu_module_%d", cycle, i);

            nimcp_error_t err = mesh_module_registry_unregister(registry_, name);
            EXPECT_EQ(err, NIMCP_SUCCESS)
                << "Unregistration failed at cycle " << cycle << " module " << i;
        }

        // Verify unregistration
        for (int i = 0; i < 5; i++) {
            char name[64];
            snprintf(name, sizeof(name), "cycle_%zu_module_%d", cycle, i);

            bool contains = mesh_module_registry_contains(registry_, name);
            EXPECT_FALSE(contains) << "Module " << name << " should be unregistered";
        }
    }

    // Final stats check
    mesh_module_registry_stats_t final_stats;
    mesh_module_registry_get_stats(registry_, &final_stats);
    // Each cycle leaves 5 modules registered
    size_t expected_remaining = REGISTER_UNREGISTER_CYCLES * 5;
    EXPECT_EQ(final_stats.total_registered, expected_remaining);
}

// =============================================================================
// Test 3: Registration Under Memory Pressure
// =============================================================================

TEST_F(MeshRegistrationRegressionTest, RegistrationUnderMemoryPressure) {
    // Bug scenario: Registration failed ungracefully under low memory
    // Simulate memory pressure by pre-allocating many modules

    std::vector<void*> pressure_allocations;
    const size_t PRESSURE_ALLOC_SIZE = 1024 * 1024;  // 1MB chunks
    const size_t PRESSURE_ALLOC_COUNT = 10;

    // Create memory pressure
    for (size_t i = 0; i < PRESSURE_ALLOC_COUNT; i++) {
        void* mem = nimcp_malloc(PRESSURE_ALLOC_SIZE);
        if (mem) {
            memset(mem, 0xAA, PRESSURE_ALLOC_SIZE);
            pressure_allocations.push_back(mem);
        }
    }

    // Try registrations under pressure
    size_t success = 0;
    size_t fail = 0;

    for (size_t i = 0; i < 50; i++) {
        char name[64];
        snprintf(name, sizeof(name), "pressure_module_%zu", i);

        nimcp_error_t err = RegisterModule(name, MESH_ADAPTER_CATEGORY_COGNITIVE);
        if (err == NIMCP_SUCCESS) {
            success++;
        } else {
            fail++;
            // Failure should be graceful - no crash
        }
    }

    // Release pressure
    for (void* mem : pressure_allocations) {
        nimcp_free(mem);
    }

    // Under memory pressure, some failures are acceptable but no crashes
    EXPECT_GT(success, 0) << "At least some registrations should succeed";

    // Verify registry is still functional
    mesh_module_registry_stats_t stats;
    nimcp_error_t err = mesh_module_registry_get_stats(registry_, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Registry should still be functional";
}

// =============================================================================
// Test 4: Registration with Invalid Pointers
// =============================================================================

TEST_F(MeshRegistrationRegressionTest, RegistrationWithInvalidPointers) {
    // Bug scenario: Invalid pointers caused crashes instead of errors

    // Test 1: NULL registry
    mesh_module_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.module_name = "test";
    desc.category = MESH_ADAPTER_CATEGORY_COGNITIVE;
    desc.module_instance = CreateMockModule("test");
    desc.module_size = sizeof(test_mock_module_t);
    desc.module_magic = TEST_MODULE_MAGIC;

    nimcp_error_t err = mesh_module_registry_register(nullptr, &desc);
    EXPECT_NE(err, NIMCP_SUCCESS) << "NULL registry should fail";

    // Test 2: NULL descriptor
    err = mesh_module_registry_register(registry_, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "NULL descriptor should fail";

    // Test 3: NULL module name
    desc.module_name = nullptr;
    err = mesh_module_registry_register(registry_, &desc);
    EXPECT_NE(err, NIMCP_SUCCESS) << "NULL module name should fail";
    desc.module_name = "test_restored";

    // Test 4: NULL module instance
    desc.module_instance = nullptr;
    err = mesh_module_registry_register(registry_, &desc);
    EXPECT_NE(err, NIMCP_SUCCESS) << "NULL module instance should fail";

    // Test 5: Zero module size
    desc.module_instance = CreateMockModule("test2");
    desc.module_size = 0;
    err = mesh_module_registry_register(registry_, &desc);
    EXPECT_NE(err, NIMCP_SUCCESS) << "Zero module size should fail";

    // Test 6: Unregister with NULL name
    err = mesh_module_registry_unregister(registry_, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "NULL unregister name should fail";

    // Test 7: Get with NULL name
    const mesh_registered_module_t* found = mesh_module_registry_get(registry_, nullptr);
    EXPECT_EQ(found, nullptr) << "NULL get should return nullptr";
}

// =============================================================================
// Test 5: Duplicate Detection Under Load
// =============================================================================

TEST_F(MeshRegistrationRegressionTest, DuplicateDetectionUnderLoad) {
    // Bug scenario: Duplicate detection missed duplicates under concurrent load
    const size_t MODULE_COUNT = 50;

    // Register initial modules
    for (size_t i = 0; i < MODULE_COUNT; i++) {
        char name[64];
        snprintf(name, sizeof(name), "dup_test_module_%zu", i);

        nimcp_error_t err = RegisterModule(name, MESH_ADAPTER_CATEGORY_COGNITIVE);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    // Try to register duplicates
    std::atomic<size_t> duplicate_rejections{0};

    for (size_t i = 0; i < MODULE_COUNT; i++) {
        char name[64];
        snprintf(name, sizeof(name), "dup_test_module_%zu", i);

        // Create new module instance with same name
        test_mock_module_t* mod = static_cast<test_mock_module_t*>(
            nimcp_calloc(1, sizeof(test_mock_module_t)));
        mod->magic = TEST_MODULE_MAGIC;
        strncpy(mod->name, name, sizeof(mod->name) - 1);
        mod->initialized = true;

        mesh_module_descriptor_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.module_name = name;
        desc.category = MESH_ADAPTER_CATEGORY_COGNITIVE;
        desc.module_instance = mod;
        desc.module_size = sizeof(test_mock_module_t);
        desc.module_magic = TEST_MODULE_MAGIC;

        nimcp_error_t err = mesh_module_registry_register(registry_, &desc);
        if (err != NIMCP_SUCCESS) {
            duplicate_rejections++;
        }

        nimcp_free(mod);  // Free since not added to modules_ list
    }

    // All duplicates should be rejected
    EXPECT_EQ(duplicate_rejections.load(), MODULE_COUNT)
        << "All duplicates should be rejected";

    // Verify stats show duplicate detections
    mesh_module_registry_stats_t stats;
    mesh_module_registry_get_stats(registry_, &stats);
    EXPECT_EQ(stats.duplicate_detections, MODULE_COUNT);
    EXPECT_EQ(stats.total_registered, MODULE_COUNT);
}

// =============================================================================
// Test 6: Category Filtering at Scale
// =============================================================================

TEST_F(MeshRegistrationRegressionTest, CategoryFilteringAtScale) {
    // Bug scenario: Category filtering was O(n) and slow at scale
    const size_t MODULES_PER_CATEGORY = 30;

    mesh_adapter_category_t categories[] = {
        MESH_ADAPTER_CATEGORY_COGNITIVE,
        MESH_ADAPTER_CATEGORY_PERCEPTION,
        MESH_ADAPTER_CATEGORY_SUBCORTICAL,
        MESH_ADAPTER_CATEGORY_MOTOR,
        MESH_ADAPTER_CATEGORY_MEMORY
    };
    size_t num_categories = sizeof(categories) / sizeof(categories[0]);

    // Register modules in each category
    for (size_t cat_idx = 0; cat_idx < num_categories; cat_idx++) {
        for (size_t i = 0; i < MODULES_PER_CATEGORY; i++) {
            char name[64];
            snprintf(name, sizeof(name), "cat%zu_module_%zu", cat_idx, i);

            nimcp_error_t err = RegisterModule(name, categories[cat_idx]);
            EXPECT_EQ(err, NIMCP_SUCCESS);
        }
    }

    // Time category filtering
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t cat_idx = 0; cat_idx < num_categories; cat_idx++) {
        const mesh_registered_module_t* modules[MODULES_PER_CATEGORY];
        size_t count;

        nimcp_error_t err = mesh_module_registry_get_by_category(
            registry_, categories[cat_idx], modules, MODULES_PER_CATEGORY, &count);

        EXPECT_EQ(err, NIMCP_SUCCESS);
        EXPECT_EQ(count, MODULES_PER_CATEGORY)
            << "Category " << cat_idx << " should have " << MODULES_PER_CATEGORY << " modules";
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(duration_ms, 500)
        << "Category filtering took too long: " << duration_ms << "ms";

    // Verify stats per category
    mesh_module_registry_stats_t stats;
    mesh_module_registry_get_stats(registry_, &stats);
    EXPECT_EQ(stats.cognitive_count, MODULES_PER_CATEGORY);
    EXPECT_EQ(stats.perception_count, MODULES_PER_CATEGORY);
    EXPECT_EQ(stats.subcortical_count, MODULES_PER_CATEGORY);
    EXPECT_EQ(stats.motor_count, MODULES_PER_CATEGORY);
    EXPECT_EQ(stats.memory_count, MODULES_PER_CATEGORY);
}

// =============================================================================
// Test 7: Magic Validation Edge Cases
// =============================================================================

TEST_F(MeshRegistrationRegressionTest, MagicValidationEdgeCases) {
    // Bug scenario: Magic validation accepted corrupted magic numbers

    // Test 1: Wrong magic number
    test_mock_module_t* bad_magic = static_cast<test_mock_module_t*>(
        nimcp_calloc(1, sizeof(test_mock_module_t)));
    bad_magic->magic = 0xBADC0DE;  // Wrong magic

    mesh_module_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.module_name = "bad_magic_module";
    desc.category = MESH_ADAPTER_CATEGORY_COGNITIVE;
    desc.module_instance = bad_magic;
    desc.module_size = sizeof(test_mock_module_t);
    desc.module_magic = TEST_MODULE_MAGIC;  // Expected magic

    nimcp_error_t err = mesh_module_registry_register(registry_, &desc);
    EXPECT_NE(err, NIMCP_SUCCESS) << "Wrong magic should be rejected";
    nimcp_free(bad_magic);

    // Test 2: Zero magic number
    test_mock_module_t* zero_magic = static_cast<test_mock_module_t*>(
        nimcp_calloc(1, sizeof(test_mock_module_t)));
    zero_magic->magic = 0;

    desc.module_name = "zero_magic_module";
    desc.module_instance = zero_magic;
    desc.module_magic = 0;

    err = mesh_module_registry_register(registry_, &desc);
    // Zero magic might be rejected or accepted depending on policy
    nimcp_free(zero_magic);

    // Test 3: Validate all after registration
    test_mock_module_t* good_mod = CreateMockModule("good_magic_module");
    desc.module_name = "good_magic_module";
    desc.module_instance = good_mod;
    desc.module_magic = TEST_MODULE_MAGIC;

    err = mesh_module_registry_register(registry_, &desc);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Corrupt the magic after registration
    good_mod->magic = 0xDEAD;

    size_t invalid_count;
    err = mesh_module_registry_validate_all(registry_, &invalid_count);
    EXPECT_NE(err, NIMCP_SUCCESS) << "Validation should detect corruption";
    EXPECT_GT(invalid_count, 0u) << "Should find invalid modules";

    // Verify stats show magic validation failure
    mesh_module_registry_stats_t stats;
    mesh_module_registry_get_stats(registry_, &stats);
    EXPECT_GT(stats.magic_validation_failures, 0u);
}

// =============================================================================
// Test 8: Concurrent Registration Operations
// =============================================================================

TEST_F(MeshRegistrationRegressionTest, ConcurrentRegistrationOperations) {
    // Bug scenario: Concurrent registrations caused race conditions
    std::atomic<size_t> success_count{0};
    std::atomic<size_t> fail_count{0};
    std::vector<std::thread> threads;

    const size_t MODULES_PER_THREAD = 20;

    for (size_t t = 0; t < CONCURRENT_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < MODULES_PER_THREAD; i++) {
                char name[64];
                snprintf(name, sizeof(name), "concurrent_t%zu_m%zu", t, i);

                test_mock_module_t* mod = static_cast<test_mock_module_t*>(
                    nimcp_calloc(1, sizeof(test_mock_module_t)));
                if (!mod) {
                    fail_count++;
                    continue;
                }

                mod->magic = TEST_MODULE_MAGIC;
                strncpy(mod->name, name, sizeof(mod->name) - 1);

                mesh_module_descriptor_t desc;
                memset(&desc, 0, sizeof(desc));
                desc.module_name = name;
                desc.category = static_cast<mesh_adapter_category_t>(t % 6);
                desc.module_instance = mod;
                desc.module_size = sizeof(test_mock_module_t);
                desc.module_magic = TEST_MODULE_MAGIC;

                nimcp_error_t err = mesh_module_registry_register(registry_, &desc);
                if (err == NIMCP_SUCCESS) {
                    success_count++;
                } else {
                    fail_count++;
                    nimcp_free(mod);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // All unique registrations should succeed (no race condition duplicates)
    size_t expected_total = CONCURRENT_THREADS * MODULES_PER_THREAD;
    EXPECT_EQ(success_count.load(), expected_total)
        << "All concurrent registrations should succeed";
    EXPECT_EQ(fail_count.load(), 0u)
        << "No failures expected with unique names";

    // Verify final count
    mesh_module_registry_stats_t stats;
    mesh_module_registry_get_stats(registry_, &stats);
    EXPECT_EQ(stats.total_registered, expected_total);
}

