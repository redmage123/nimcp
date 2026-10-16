/**
 * @file test_wiring_diagram_validation_regression.cpp
 * @brief Regression tests for Phase 9: Wiring Diagram Validation & Dependency Resolution
 *
 * Tests ensure validation and dependency resolution behavior remains consistent:
 * - API stability tests
 * - Return value consistency
 * - Edge case behavior preservation
 * - Performance regression tests
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <filesystem>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WiringValidationRegression : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
    std::string temp_dir_;

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = false;
        bio_config.enable_logging = false;
        nimcp_bio_async_init(&bio_config);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = false;
        router_config.enable_logging = false;
        bio_router_init(&router_config);

        // Create temporary directory
        temp_dir_ = "/tmp/nimcp_wiring_valid_regression_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir_);

        // Create wiring diagram
        wd_ = wiring_diagram_create(temp_dir_.c_str());
        ASSERT_NE(wd_, nullptr);
        wiring_diagram_set_auto_persist(wd_, false);
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

    // Helper: Add module with dependencies
    int AddModule(bio_module_id_t id, const char* name,
                  const std::vector<bio_module_id_t>& deps = {}) {
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        strncpy(config.module_name, name, sizeof(config.module_name) - 1);
        config.module_id = id;
        config.subsystem = WIRING_SUBSYSTEM_CORE;
        config.min_tier = PLATFORM_TIER_MINIMAL;
        config.enabled = true;

        if (!deps.empty()) {
            config.depends_on = (bio_module_id_t*)
                nimcp_calloc(deps.size(), sizeof(bio_module_id_t));
            if (config.depends_on) {
                for (size_t i = 0; i < deps.size(); i++) {
                    config.depends_on[i] = deps[i];
                }
                config.depends_on_count = deps.size();
                config.depends_on_capacity = deps.size();
            }
        }

        return wiring_diagram_add_module(wd_, name, &config);
    }

    // Helper: Add disabled module
    int AddDisabledModule(bio_module_id_t id, const char* name,
                          const std::vector<bio_module_id_t>& deps = {}) {
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        strncpy(config.module_name, name, sizeof(config.module_name) - 1);
        config.module_id = id;
        config.subsystem = WIRING_SUBSYSTEM_CORE;
        config.min_tier = PLATFORM_TIER_MINIMAL;
        config.enabled = false;  // Disabled

        if (!deps.empty()) {
            config.depends_on = (bio_module_id_t*)
                nimcp_calloc(deps.size(), sizeof(bio_module_id_t));
            if (config.depends_on) {
                for (size_t i = 0; i < deps.size(); i++) {
                    config.depends_on[i] = deps[i];
                }
                config.depends_on_count = deps.size();
                config.depends_on_capacity = deps.size();
            }
        }

        return wiring_diagram_add_module(wd_, name, &config);
    }
};

//=============================================================================
// API STABILITY REGRESSION TESTS
//=============================================================================

/**
 * @test wiring_diagram_validate null parameters return -1
 * @regression Error return value must remain -1 for compatibility
 */
TEST_F(WiringValidationRegression, API_Validate_NullReturnsMinusOne) {
    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(nullptr, &result), -1);
    EXPECT_EQ(wiring_diagram_validate(wd_, nullptr), -1);
    EXPECT_EQ(wiring_diagram_validate(nullptr, nullptr), -1);
}

/**
 * @test wiring_diagram_check_circular_deps null wd returns -1, other params optional
 * @regression Error return value must remain -1 for null wd
 */
TEST_F(WiringValidationRegression, API_CheckCircularDeps_NullWDReturnsMinusOne) {
    bio_module_id_t cycle[10];
    uint32_t count;
    EXPECT_EQ(wiring_diagram_check_circular_deps(nullptr, cycle, 10, &count), -1);
    // Note: cycle_modules and cycle_count can be null (implementation allows it)
    EXPECT_EQ(wiring_diagram_check_circular_deps(wd_, nullptr, 10, &count), 0);  // Empty diagram, no cycles
    EXPECT_EQ(wiring_diagram_check_circular_deps(wd_, cycle, 10, nullptr), 0);   // Empty diagram, no cycles
}

/**
 * @test wiring_diagram_get_startup_order null parameters return -1
 * @regression Error return value must remain -1 for compatibility
 */
TEST_F(WiringValidationRegression, API_GetStartupOrder_NullReturnsMinusOne) {
    bio_module_id_t order[10];
    EXPECT_EQ(wiring_diagram_get_startup_order(nullptr, order, 10), -1);
    EXPECT_EQ(wiring_diagram_get_startup_order(wd_, nullptr, 10), -1);
}

/**
 * @test wiring_diagram_get_dependency_chain null parameters return -1
 * @regression Error return value must remain -1 for compatibility
 */
TEST_F(WiringValidationRegression, API_GetDependencyChain_NullReturnsMinusOne) {
    bio_module_id_t deps[10];
    EXPECT_EQ(wiring_diagram_get_dependency_chain(nullptr, static_cast<bio_module_id_t>(0x100), deps, 10), -1);
    EXPECT_EQ(wiring_diagram_get_dependency_chain(wd_, static_cast<bio_module_id_t>(0x100), nullptr, 10), -1);
}

/**
 * @test wiring_diagram_get_dependents null parameters return -1
 * @regression Error return value must remain -1 for compatibility
 */
TEST_F(WiringValidationRegression, API_GetDependents_NullReturnsMinusOne) {
    bio_module_id_t deps[10];
    EXPECT_EQ(wiring_diagram_get_dependents(nullptr, static_cast<bio_module_id_t>(0x100), deps, 10), -1);
    EXPECT_EQ(wiring_diagram_get_dependents(wd_, static_cast<bio_module_id_t>(0x100), nullptr, 10), -1);
}

/**
 * @test wiring_diagram_can_disable_module null parameter return -1
 * @regression Error return value must remain -1 for compatibility
 */
TEST_F(WiringValidationRegression, API_CanDisableModule_NullReturnsMinusOne) {
    EXPECT_EQ(wiring_diagram_can_disable_module(nullptr, static_cast<bio_module_id_t>(0x100)), -1);
}

/**
 * @test Empty diagram validates successfully
 * @regression Empty diagram is valid, not an error
 */
TEST_F(WiringValidationRegression, API_EmptyDiagramIsValid) {
    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);
    EXPECT_TRUE(result.valid);
    EXPECT_FALSE(result.has_circular_deps);
    EXPECT_FALSE(result.has_missing_deps);
}

//=============================================================================
// BEHAVIOR CONSISTENCY REGRESSION TESTS
//=============================================================================

/**
 * @test Circular dependency sets has_circular_deps
 * @regression has_circular_deps flag must be set correctly
 */
TEST_F(WiringValidationRegression, Behavior_CircularDepsSetFlag) {
    AddModule(static_cast<bio_module_id_t>(0x100), "module_a", {static_cast<bio_module_id_t>(0x101)});
    AddModule(static_cast<bio_module_id_t>(0x101), "module_b", {static_cast<bio_module_id_t>(0x100)});

    wiring_validation_result_t result;
    wiring_diagram_validate(wd_, &result);

    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.has_circular_deps);
}

/**
 * @test Missing dependency sets has_missing_deps
 * @regression has_missing_deps flag must be set correctly
 */
TEST_F(WiringValidationRegression, Behavior_MissingDepsSetFlag) {
    AddModule(static_cast<bio_module_id_t>(0x100), "module_a", {static_cast<bio_module_id_t>(0x999)});

    wiring_validation_result_t result;
    wiring_diagram_validate(wd_, &result);

    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.has_missing_deps);
}

/**
 * @test Duplicate module IDs detected
 * @regression Duplicate IDs must cause validation failure
 */
TEST_F(WiringValidationRegression, Behavior_DuplicateIDsInvalid) {
    AddModule(static_cast<bio_module_id_t>(0x100), "module_a");
    AddModule(static_cast<bio_module_id_t>(0x100), "module_b");  // Same ID

    wiring_validation_result_t result;
    wiring_diagram_validate(wd_, &result);

    EXPECT_FALSE(result.valid);
    EXPECT_GT(result.error_count, 0u);
}

/**
 * @test Startup order respects dependencies
 * @regression Dependencies must start before dependents
 */
TEST_F(WiringValidationRegression, Behavior_StartupOrderRespectsDeps) {
    AddModule(static_cast<bio_module_id_t>(0x100), "base");
    AddModule(static_cast<bio_module_id_t>(0x101), "middle", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "top", {static_cast<bio_module_id_t>(0x101)});

    bio_module_id_t order[10];
    int count = wiring_diagram_get_startup_order(wd_, order, 10);
    EXPECT_EQ(count, 3);

    // base (0x100) must come before middle (0x101) which must come before top (0x102)
    int base_idx = -1, middle_idx = -1, top_idx = -1;
    for (int i = 0; i < count; i++) {
        if (order[i] == static_cast<bio_module_id_t>(0x100)) base_idx = i;
        if (order[i] == static_cast<bio_module_id_t>(0x101)) middle_idx = i;
        if (order[i] == static_cast<bio_module_id_t>(0x102)) top_idx = i;
    }

    EXPECT_LT(base_idx, middle_idx);
    EXPECT_LT(middle_idx, top_idx);
}

/**
 * @test Transitive dependencies are included
 * @regression get_dependency_chain must include all transitive deps
 */
TEST_F(WiringValidationRegression, Behavior_TransitiveDepsIncluded) {
    AddModule(static_cast<bio_module_id_t>(0x100), "base");
    AddModule(static_cast<bio_module_id_t>(0x101), "middle", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "top", {static_cast<bio_module_id_t>(0x101)});

    bio_module_id_t deps[10];
    int count = wiring_diagram_get_dependency_chain(wd_, static_cast<bio_module_id_t>(0x102), deps, 10);
    EXPECT_EQ(count, 2);  // middle and base
}

/**
 * @test can_disable returns 0 if has enabled dependents
 * @regression Modules with enabled dependents cannot be disabled
 */
TEST_F(WiringValidationRegression, Behavior_CannotDisableWithDependents) {
    AddModule(static_cast<bio_module_id_t>(0x100), "base");
    AddModule(static_cast<bio_module_id_t>(0x101), "child", {static_cast<bio_module_id_t>(0x100)});

    EXPECT_EQ(wiring_diagram_can_disable_module(wd_, static_cast<bio_module_id_t>(0x100)), 0);
    EXPECT_EQ(wiring_diagram_can_disable_module(wd_, static_cast<bio_module_id_t>(0x101)), 1);
}

/**
 * @test Disabled modules don't block disabling dependencies
 * @regression Only enabled dependents block disable
 */
TEST_F(WiringValidationRegression, Behavior_DisabledDependentsDontBlock) {
    AddModule(static_cast<bio_module_id_t>(0x100), "base");
    AddDisabledModule(static_cast<bio_module_id_t>(0x101), "disabled_child", {static_cast<bio_module_id_t>(0x100)});

    // base can be disabled because child is disabled
    EXPECT_EQ(wiring_diagram_can_disable_module(wd_, static_cast<bio_module_id_t>(0x100)), 1);
}

/**
 * @test check_circular_deps returns -1 and cycle modules when cycles exist
 * @regression Returns -1 (not 1) when cycles detected
 */
TEST_F(WiringValidationRegression, Behavior_CircularDepsReportsCycle) {
    AddModule(static_cast<bio_module_id_t>(0x100), "a", {static_cast<bio_module_id_t>(0x101)});
    AddModule(static_cast<bio_module_id_t>(0x101), "b", {static_cast<bio_module_id_t>(0x100)});

    bio_module_id_t cycle[10];
    uint32_t count = 0;
    int result = wiring_diagram_check_circular_deps(wd_, cycle, 10, &count);

    EXPECT_EQ(result, -1);  // Has cycles - returns -1
    EXPECT_GT(count, 0u);
}

//=============================================================================
// EDGE CASE REGRESSION TESTS
//=============================================================================

/**
 * @test Single module with no dependencies validates
 * @regression Single module is valid
 */
TEST_F(WiringValidationRegression, Edge_SingleModuleValid) {
    AddModule(static_cast<bio_module_id_t>(0x100), "single");

    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);
    EXPECT_TRUE(result.valid);
}

/**
 * @test Module depending on itself is circular
 * @regression Self-dependency is circular
 */
TEST_F(WiringValidationRegression, Edge_SelfDependencyIsCircular) {
    AddModule(static_cast<bio_module_id_t>(0x100), "self", {static_cast<bio_module_id_t>(0x100)});

    wiring_validation_result_t result;
    wiring_diagram_validate(wd_, &result);

    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.has_circular_deps);
}

/**
 * @test Disabled module with missing dep still fails validation
 * @regression All modules (enabled or disabled) have deps validated
 */
TEST_F(WiringValidationRegression, Edge_DisabledModuleMissingDepStillFails) {
    AddDisabledModule(static_cast<bio_module_id_t>(0x100), "disabled", {static_cast<bio_module_id_t>(0x999)});

    wiring_validation_result_t result;
    // Implementation validates all modules' deps, not just enabled ones
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), -1);
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.has_missing_deps);
}

/**
 * @test Get startup order with zero buffer size returns -1
 * @regression Zero buffer must fail gracefully
 */
TEST_F(WiringValidationRegression, Edge_ZeroBufferSize) {
    AddModule(static_cast<bio_module_id_t>(0x100), "test");

    bio_module_id_t order[1];
    EXPECT_EQ(wiring_diagram_get_startup_order(wd_, order, 0), -1);
}

/**
 * @test Diamond dependency graph resolves correctly
 * @regression Diamond pattern must not cause issues
 */
TEST_F(WiringValidationRegression, Edge_DiamondDependency) {
    //     D
    //    / \
    //   B   C
    //    \ /
    //     A
    AddModule(static_cast<bio_module_id_t>(0x100), "a");
    AddModule(static_cast<bio_module_id_t>(0x101), "b", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "c", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x103), "d", {static_cast<bio_module_id_t>(0x101), static_cast<bio_module_id_t>(0x102)});

    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);
    EXPECT_TRUE(result.valid);
    EXPECT_FALSE(result.has_circular_deps);

    bio_module_id_t order[10];
    int count = wiring_diagram_get_startup_order(wd_, order, 10);
    EXPECT_EQ(count, 4);

    // A must be first, D must be last
    EXPECT_EQ(order[0], static_cast<bio_module_id_t>(0x100));
    EXPECT_EQ(order[3], static_cast<bio_module_id_t>(0x103));
}

//=============================================================================
// PERFORMANCE REGRESSION TESTS
//=============================================================================

/**
 * @test Validation performance baseline
 * @regression Validation must complete in reasonable time
 */
TEST_F(WiringValidationRegression, Performance_ValidationBaseline) {
    const int MODULE_COUNT = 100;

    // Create linear dependency chain
    for (int i = 0; i < MODULE_COUNT; i++) {
        std::string name = "module_" + std::to_string(i);
        if (i == 0) {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str());
        } else {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str(),
                      {static_cast<bio_module_id_t>(0x100 + i - 1)});
        }
    }

    auto start = std::chrono::high_resolution_clock::now();

    wiring_validation_result_t result;
    wiring_diagram_validate(wd_, &result);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    EXPECT_TRUE(result.valid);

    printf("  [Regression] Validate %d modules: %ld us\n", MODULE_COUNT, elapsed_us);

    // Should complete in < 50ms
    EXPECT_LT(elapsed_us, 50000);
}

/**
 * @test Startup order performance baseline
 * @regression Topological sort must complete quickly
 */
TEST_F(WiringValidationRegression, Performance_StartupOrderBaseline) {
    const int MODULE_COUNT = 100;

    // Create linear dependency chain
    for (int i = 0; i < MODULE_COUNT; i++) {
        std::string name = "module_" + std::to_string(i);
        if (i == 0) {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str());
        } else {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str(),
                      {static_cast<bio_module_id_t>(0x100 + i - 1)});
        }
    }

    bio_module_id_t order[200];

    auto start = std::chrono::high_resolution_clock::now();

    int count = wiring_diagram_get_startup_order(wd_, order, 200);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    EXPECT_EQ(count, MODULE_COUNT);

    printf("  [Regression] Startup order %d modules: %ld us\n", count, elapsed_us);

    // Should complete in < 50ms
    EXPECT_LT(elapsed_us, 50000);
}

/**
 * @test Dependency chain performance baseline
 * @regression Transitive deps must be computed quickly
 */
TEST_F(WiringValidationRegression, Performance_DependencyChainBaseline) {
    const int CHAIN_LENGTH = 50;

    // Create linear dependency chain
    for (int i = 0; i < CHAIN_LENGTH; i++) {
        std::string name = "module_" + std::to_string(i);
        if (i == 0) {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str());
        } else {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str(),
                      {static_cast<bio_module_id_t>(0x100 + i - 1)});
        }
    }

    bio_module_id_t deps[100];
    const int ITERATIONS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        wiring_diagram_get_dependency_chain(wd_, static_cast<bio_module_id_t>(0x100 + CHAIN_LENGTH - 1), deps, 100);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    printf("  [Regression] Dependency chain %d iterations: %ld us (%.2f us/iter)\n",
           ITERATIONS, elapsed_us, (float)elapsed_us / ITERATIONS);

    // Should complete in < 100ms for 1000 iterations
    EXPECT_LT(elapsed_us, 100000);
}

/**
 * @test Circular dep detection performance
 * @regression Cycle detection must be efficient
 */
TEST_F(WiringValidationRegression, Performance_CircularDetectionBaseline) {
    const int MODULE_COUNT = 50;

    // Create a valid graph (no cycles) to test worst case
    for (int i = 0; i < MODULE_COUNT; i++) {
        std::string name = "module_" + std::to_string(i);
        if (i == 0) {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str());
        } else {
            // Each module depends on all previous modules (dense graph)
            std::vector<bio_module_id_t> deps;
            for (int j = 0; j < i && j < 5; j++) {  // Limit to 5 deps for sanity
                deps.push_back(static_cast<bio_module_id_t>(0x100 + i - j - 1));
            }
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str(), deps);
        }
    }

    bio_module_id_t cycle[100];
    uint32_t count;

    auto start = std::chrono::high_resolution_clock::now();

    int has_cycle = wiring_diagram_check_circular_deps(wd_, cycle, 100, &count);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    EXPECT_EQ(has_cycle, 0);  // No cycles

    printf("  [Regression] Circular detection %d modules: %ld us\n", MODULE_COUNT, elapsed_us);

    // Should complete in < 50ms
    EXPECT_LT(elapsed_us, 50000);
}

//=============================================================================
// THREAD SAFETY REGRESSION TESTS
//=============================================================================

/**
 * @test Concurrent validation is safe
 * @regression No crashes or data corruption under concurrent access
 */
TEST_F(WiringValidationRegression, ThreadSafety_ConcurrentValidation) {
    // Create some modules
    for (int i = 0; i < 20; i++) {
        std::string name = "module_" + std::to_string(i);
        if (i == 0) {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str());
        } else {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str(),
                      {static_cast<bio_module_id_t>(0x100 + i - 1)});
        }
    }

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &error_count]() {
            for (int i = 0; i < 100; i++) {
                wiring_validation_result_t result;
                int ret = wiring_diagram_validate(wd_, &result);
                if (ret < 0 || !result.valid) {
                    error_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0);
}

/**
 * @test Concurrent startup order computation is safe
 * @regression Multiple threads can call get_startup_order
 */
TEST_F(WiringValidationRegression, ThreadSafety_ConcurrentStartupOrder) {
    // Create some modules
    for (int i = 0; i < 10; i++) {
        std::string name = "module_" + std::to_string(i);
        if (i == 0) {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str());
        } else {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str(),
                      {static_cast<bio_module_id_t>(0x100 + i - 1)});
        }
    }

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &error_count]() {
            bio_module_id_t order[20];
            for (int i = 0; i < 100; i++) {
                int count = wiring_diagram_get_startup_order(wd_, order, 20);
                if (count != 10) {
                    error_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0);
}

/**
 * @test Concurrent dependency chain lookup is safe
 * @regression Multiple threads can call get_dependency_chain
 */
TEST_F(WiringValidationRegression, ThreadSafety_ConcurrentDependencyChain) {
    // Create chain
    for (int i = 0; i < 10; i++) {
        std::string name = "module_" + std::to_string(i);
        if (i == 0) {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str());
        } else {
            AddModule(static_cast<bio_module_id_t>(0x100 + i), name.c_str(),
                      {static_cast<bio_module_id_t>(0x100 + i - 1)});
        }
    }

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &success_count]() {
            bio_module_id_t deps[20];
            for (int i = 0; i < 100; i++) {
                int count = wiring_diagram_get_dependency_chain(
                    wd_, static_cast<bio_module_id_t>(0x109), deps, 20);
                if (count == 9) {  // module_9 depends on 0-8
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 400);  // 4 threads * 100 iterations
}

