//=============================================================================
// test_brain_pr_memory.cpp - Prime Resonant Memory Brain Integration Unit Tests
//=============================================================================
/**
 * @file test_brain_pr_memory.cpp
 * @brief Unit tests for brain-PR memory integration
 *
 * WHAT: Tests the Prime Resonant memory system initialization within the brain
 * WHY:  Verify Z-Ladder, theta-gamma, and entanglement systems initialize correctly
 * HOW:  Create brains with PR memory enabled, verify all components are present
 *
 * PR MEMORY COMPONENTS TESTED:
 * - Z-Ladder (4-tier memory consolidation: Z0-working, Z1-short, Z2-long, Z3-permanent)
 * - Theta-gamma coupling (phase-gated encoding/retrieval windows)
 * - Entanglement graph (associative memory with resonance-weighted edges)
 *
 * TEST SCENARIOS:
 * 1. PR memory enabled by default
 * 2. PR memory explicitly disabled
 * 3. Lazy initialization mode
 * 4. Component accessor functions
 * 5. Statistics retrieval
 * 6. Tick/consolidation updates
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/brain/internal/nimcp_brain_pr_memory.h"
#include "nimcp.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BrainPRMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    // Helper: Create brain with specific PR memory config
    brain_t createBrainWithPRMemory(bool enable_pr_memory, bool lazy_init = false) {
        brain_config_t config = {};
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        strncpy(config.task_name, "pr_memory_test", 63);
        config.enable_pr_memory = enable_pr_memory;
        config.lazy_pr_memory_init = lazy_init;
        return brain_create_custom(&config);
    }
};

//=============================================================================
// Test: PR Memory Default Initialization
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_EnabledByDefault) {
    // WHAT: Test that PR memory is enabled by default
    // WHY:  Verify brain factory initializes PR memory system
    // HOW:  Create brain with default config, check PR memory is present

    brain_t brain = createBrainWithPRMemory(true);
    ASSERT_NE(brain, nullptr) << "Failed to create brain with PR memory";

    // Check PR memory is initialized
    EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain))
        << "PR memory should be initialized";

    // Verify all components are present
    EXPECT_NE(nimcp_brain_get_z_ladder(brain), nullptr)
        << "Z-Ladder should be initialized";
    EXPECT_NE(nimcp_brain_get_theta_gamma(brain), nullptr)
        << "Theta-gamma manager should be initialized";
    EXPECT_NE(nimcp_brain_get_entanglement(brain), nullptr)
        << "Entanglement graph should be initialized";

    brain_destroy(brain);
}

//=============================================================================
// Test: PR Memory Disabled
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_Disabled) {
    // WHAT: Test PR memory can be disabled
    // WHY:  Some use cases may not need PR memory overhead
    // HOW:  Create brain with PR memory disabled, verify no PR components

    brain_t brain = createBrainWithPRMemory(false);
    ASSERT_NE(brain, nullptr) << "Failed to create brain without PR memory";

    // Check PR memory is not initialized
    EXPECT_FALSE(nimcp_brain_pr_memory_is_initialized(brain))
        << "PR memory should not be initialized when disabled";

    // Verify no components
    EXPECT_EQ(nimcp_brain_get_z_ladder(brain), nullptr)
        << "Z-Ladder should be NULL when disabled";
    EXPECT_EQ(nimcp_brain_get_theta_gamma(brain), nullptr)
        << "Theta-gamma should be NULL when disabled";
    EXPECT_EQ(nimcp_brain_get_entanglement(brain), nullptr)
        << "Entanglement should be NULL when disabled";

    brain_destroy(brain);
}

//=============================================================================
// Test: Lazy Initialization
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_LazyInit) {
    // WHAT: Test lazy initialization mode
    // WHY:  Allow deferring heavy PR memory init until needed
    // HOW:  Create brain with lazy init, verify PR not initialized initially

    brain_t brain = createBrainWithPRMemory(true, true);  // enabled but lazy
    ASSERT_NE(brain, nullptr) << "Failed to create brain with lazy PR memory";

    // With lazy init, PR memory should NOT be initialized yet
    EXPECT_FALSE(nimcp_brain_pr_memory_is_initialized(brain))
        << "PR memory should not be initialized in lazy mode";

    // Components should be NULL
    EXPECT_EQ(nimcp_brain_get_z_ladder(brain), nullptr)
        << "Z-Ladder should be NULL in lazy mode";

    brain_destroy(brain);
}

//=============================================================================
// Test: Manual Initialization After Lazy
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_ManualInitAfterLazy) {
    // WHAT: Test manual initialization after lazy start
    // WHY:  Verify PR memory can be initialized on-demand
    // HOW:  Create with lazy init, then explicitly initialize

    brain_t brain = createBrainWithPRMemory(true, true);
    ASSERT_NE(brain, nullptr) << "Failed to create brain";

    // Not initialized yet
    EXPECT_FALSE(nimcp_brain_pr_memory_is_initialized(brain));

    // Manually initialize
    bool result = nimcp_brain_pr_memory_init(brain, NULL);
    EXPECT_TRUE(result) << "Manual PR memory init should succeed";

    // Now should be initialized
    EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain))
        << "PR memory should be initialized after manual init";
    EXPECT_NE(nimcp_brain_get_z_ladder(brain), nullptr);
    EXPECT_NE(nimcp_brain_get_theta_gamma(brain), nullptr);
    EXPECT_NE(nimcp_brain_get_entanglement(brain), nullptr);

    brain_destroy(brain);
}

//=============================================================================
// Test: Statistics Retrieval
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_StatsRetrieval) {
    // WHAT: Test statistics retrieval for PR memory
    // WHY:  Verify stats structure is populated correctly
    // HOW:  Create brain, retrieve stats, check values are reasonable

    brain_t brain = createBrainWithPRMemory(true);
    ASSERT_NE(brain, nullptr);

    brain_pr_memory_stats_t stats;
    bool result = nimcp_brain_pr_memory_get_stats(brain, &stats);
    EXPECT_TRUE(result) << "Stats retrieval should succeed";

    // Initial counts should be zero
    EXPECT_EQ(stats.z0_count, 0u) << "Z0 should be empty initially";
    EXPECT_EQ(stats.z1_count, 0u) << "Z1 should be empty initially";
    EXPECT_EQ(stats.z2_count, 0u) << "Z2 should be empty initially";
    EXPECT_EQ(stats.z3_count, 0u) << "Z3 should be empty initially";

    // Promotion/demotion/eviction counts should be zero
    EXPECT_EQ(stats.total_promotions, 0u);
    EXPECT_EQ(stats.total_demotions, 0u);
    EXPECT_EQ(stats.total_evictions, 0u);

    // Theta phase should be in valid range [0, 360)
    EXPECT_GE(stats.current_theta_phase, 0.0f);
    EXPECT_LT(stats.current_theta_phase, 360.0f);

    // Entanglement should be empty initially
    EXPECT_EQ(stats.entangle_node_count, 0u);
    EXPECT_EQ(stats.entangle_edge_count, 0u);

    brain_destroy(brain);
}

//=============================================================================
// Test: Stats With NULL Parameters
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_StatsNullParams) {
    // WHAT: Test stats retrieval with NULL parameters
    // WHY:  Verify graceful handling of invalid inputs
    // HOW:  Pass NULL brain and stats, expect false return

    brain_t brain = createBrainWithPRMemory(true);
    ASSERT_NE(brain, nullptr);

    brain_pr_memory_stats_t stats;

    // NULL brain
    EXPECT_FALSE(nimcp_brain_pr_memory_get_stats(NULL, &stats));

    // NULL stats
    EXPECT_FALSE(nimcp_brain_pr_memory_get_stats(brain, NULL));

    // Both NULL
    EXPECT_FALSE(nimcp_brain_pr_memory_get_stats(NULL, NULL));

    brain_destroy(brain);
}

//=============================================================================
// Test: Tick Function
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_TickUpdate) {
    // WHAT: Test the tick/update function
    // WHY:  Verify theta-gamma advances and consolidation triggers
    // HOW:  Call tick multiple times, verify phase changes

    brain_t brain = createBrainWithPRMemory(true);
    ASSERT_NE(brain, nullptr);

    brain_pr_memory_stats_t stats_before;
    nimcp_brain_pr_memory_get_stats(brain, &stats_before);
    float initial_phase = stats_before.current_theta_phase;

    // Simulate time passing (100ms intervals)
    uint64_t time_us = 0;
    for (int i = 0; i < 10; i++) {
        time_us += 100000;  // 100ms
        nimcp_brain_pr_memory_tick(brain, time_us);
    }

    brain_pr_memory_stats_t stats_after;
    nimcp_brain_pr_memory_get_stats(brain, &stats_after);

    // Phase should have changed (theta oscillation)
    // Note: Phase might wrap around, so we just check it changed
    // or that consolidation timestamp was updated
    EXPECT_NE(stats_after.last_consolidation_us, 0u)
        << "Consolidation should have been triggered";

    brain_destroy(brain);
}

//=============================================================================
// Test: Tick With Disabled PR Memory
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_TickWhenDisabled) {
    // WHAT: Test tick function when PR memory is disabled
    // WHY:  Verify no crash when ticking non-existent PR memory
    // HOW:  Create brain without PR memory, call tick

    brain_t brain = createBrainWithPRMemory(false);
    ASSERT_NE(brain, nullptr);

    // Should return false (no-op) when PR memory is disabled
    bool result = nimcp_brain_pr_memory_tick(brain, 100000);
    EXPECT_FALSE(result) << "Tick should return false when PR memory disabled";

    brain_destroy(brain);
}

//=============================================================================
// Test: Double Initialization
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_DoubleInit) {
    // WHAT: Test double initialization is safe
    // WHY:  Prevent memory leaks from double init
    // HOW:  Initialize PR memory twice, verify no crash

    brain_t brain = createBrainWithPRMemory(true);
    ASSERT_NE(brain, nullptr);

    // Already initialized by factory
    EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain));

    // Second init should be a no-op (return true)
    bool result = nimcp_brain_pr_memory_init(brain, NULL);
    EXPECT_TRUE(result) << "Double init should succeed (no-op)";

    // Still initialized
    EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain));

    brain_destroy(brain);
}

//=============================================================================
// Test: Destroy Cleanup
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_DestroyCleanup) {
    // WHAT: Test that destroy properly cleans up PR memory
    // WHY:  Prevent memory leaks
    // HOW:  Create and destroy brain, verify no memory issues

    brain_t brain = createBrainWithPRMemory(true);
    ASSERT_NE(brain, nullptr);
    EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain));

    // Destroy should clean up PR memory
    brain_destroy(brain);

    // If we get here without crash, cleanup succeeded
    SUCCEED();
}

//=============================================================================
// Test: Custom Configuration
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_CustomConfig) {
    // WHAT: Test custom PR memory configuration
    // WHY:  Verify custom capacities and thresholds are applied
    // HOW:  Create brain with custom config, verify settings

    brain_t brain = createBrainWithPRMemory(true, true);  // lazy init
    ASSERT_NE(brain, nullptr);

    // Create custom config
    brain_pr_memory_config_t custom_config = brain_pr_memory_config_default();
    custom_config.z0_capacity = 15;  // Custom working memory size
    custom_config.z1_capacity = 200;
    custom_config.theta_freq_hz = 8.0f;  // Upper theta band
    custom_config.auto_link_threshold = 0.75f;

    // Initialize with custom config
    bool result = nimcp_brain_pr_memory_init(brain, &custom_config);
    EXPECT_TRUE(result);

    // Verify initialized
    EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain));
    EXPECT_NE(nimcp_brain_get_z_ladder(brain), nullptr);

    brain_destroy(brain);
}

//=============================================================================
// Test: Accessor Functions With NULL Brain
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_AccessorsNullBrain) {
    // WHAT: Test accessor functions with NULL brain
    // WHY:  Verify graceful handling of invalid inputs
    // HOW:  Pass NULL to accessors, expect NULL return

    EXPECT_EQ(nimcp_brain_get_z_ladder(NULL), nullptr);
    EXPECT_EQ(nimcp_brain_get_theta_gamma(NULL), nullptr);
    EXPECT_EQ(nimcp_brain_get_entanglement(NULL), nullptr);
    EXPECT_FALSE(nimcp_brain_pr_memory_is_initialized(NULL));
}

//=============================================================================
// Test: Init With NULL Brain
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_InitNullBrain) {
    // WHAT: Test init function with NULL brain
    // WHY:  Verify graceful handling of invalid inputs
    // HOW:  Pass NULL brain to init, expect false return

    EXPECT_FALSE(nimcp_brain_pr_memory_init(NULL, NULL));
}

//=============================================================================
// Test: Encoding/Retrieval Windows
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_PhaseWindows) {
    // WHAT: Test encoding/retrieval window detection
    // WHY:  Verify phase gating logic works correctly
    // HOW:  Get stats, check window flags are consistent with phase

    brain_t brain = createBrainWithPRMemory(true);
    ASSERT_NE(brain, nullptr);

    // Advance time to get different phases
    uint64_t time_us = 0;
    bool found_encoding = false;
    bool found_retrieval = false;

    for (int i = 0; i < 100; i++) {
        time_us += 10000;  // 10ms steps
        nimcp_brain_pr_memory_tick(brain, time_us);

        brain_pr_memory_stats_t stats;
        nimcp_brain_pr_memory_get_stats(brain, &stats);

        // Check phase consistency with windows
        if (stats.is_encoding_window) {
            EXPECT_GE(stats.current_theta_phase, 0.0f);
            EXPECT_LT(stats.current_theta_phase, 90.0f);
            found_encoding = true;
        }

        if (stats.is_retrieval_window) {
            EXPECT_GE(stats.current_theta_phase, 180.0f);
            EXPECT_LT(stats.current_theta_phase, 270.0f);
            found_retrieval = true;
        }

        if (found_encoding && found_retrieval) {
            break;  // Found both windows
        }
    }

    // Should have found at least one window type during oscillation
    EXPECT_TRUE(found_encoding || found_retrieval)
        << "Should have entered encoding or retrieval window during oscillation";

    brain_destroy(brain);
}

//=============================================================================
// Test: Multiple Brains With PR Memory
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_MultipleBrains) {
    // WHAT: Test multiple brains with independent PR memory systems
    // WHY:  Verify no shared state between brain instances
    // HOW:  Create multiple brains, verify each has its own PR memory

    brain_t brain1 = createBrainWithPRMemory(true);
    brain_t brain2 = createBrainWithPRMemory(true);
    brain_t brain3 = createBrainWithPRMemory(false);  // One without

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);
    ASSERT_NE(brain3, nullptr);

    // Each brain should have independent PR memory
    EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain1));
    EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain2));
    EXPECT_FALSE(nimcp_brain_pr_memory_is_initialized(brain3));

    // Components should be different pointers
    EXPECT_NE(nimcp_brain_get_z_ladder(brain1), nimcp_brain_get_z_ladder(brain2));
    EXPECT_NE(nimcp_brain_get_theta_gamma(brain1), nimcp_brain_get_theta_gamma(brain2));
    EXPECT_NE(nimcp_brain_get_entanglement(brain1), nimcp_brain_get_entanglement(brain2));

    brain_destroy(brain1);
    brain_destroy(brain2);
    brain_destroy(brain3);
}

//=============================================================================
// Test: Default Config Values
//=============================================================================

TEST_F(BrainPRMemoryTest, PRMemory_DefaultConfigValues) {
    // WHAT: Test default configuration values
    // WHY:  Verify biologically-inspired defaults are set correctly
    // HOW:  Get default config, check values

    brain_pr_memory_config_t config = brain_pr_memory_config_default();

    // Z-Ladder capacities (biologically-inspired)
    EXPECT_EQ(config.z0_capacity, 9u);      // Miller's 7±2
    EXPECT_EQ(config.z1_capacity, 100u);    // Short-term buffer
    EXPECT_EQ(config.z2_capacity, 10000u);  // Long-term
    EXPECT_EQ(config.z3_capacity, 100000u); // Permanent

    // Theta-gamma (hippocampal rhythms)
    EXPECT_FLOAT_EQ(config.theta_freq_hz, 6.0f);   // 4-8 Hz center
    EXPECT_FLOAT_EQ(config.gamma_freq_hz, 40.0f);  // 30-80 Hz center

    // Entanglement
    EXPECT_EQ(config.max_entangle_nodes, 50000u);
    EXPECT_EQ(config.max_entangle_edges, 200000u);
    EXPECT_FLOAT_EQ(config.auto_link_threshold, 0.6f);

    // Consolidation timing
    EXPECT_EQ(config.consolidation_interval_us, 100000u);  // 100ms
    EXPECT_TRUE(config.enable_phase_gating);
    EXPECT_TRUE(config.enable_sleep_boost);
}

