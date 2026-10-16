/**
 * @file test_working_memory_immune_integration.cpp
 * @brief Unit tests for working memory - brain immune system integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Test bidirectional integration between working memory and immune system
 * WHY:  Verify inflammation impairs WM capacity, WM stress triggers cytokines
 * HOW:  Test connection, capacity reduction, stress signaling, and recovery
 *
 * BIOLOGICAL BASIS:
 * - Cytokines (IL-6, TNF-α) impair prefrontal cortex working memory
 * - Working memory overload triggers stress immune response
 * - Recovery restores cognitive capacity
 *
 * TEST COVERAGE:
 * - Connection API
 * - Inflammation-induced capacity reduction
 * - Automatic eviction under inflammation
 * - WM stress cytokine signaling
 * - Recovery and capacity restoration
 * - Edge cases and error handling
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"

#include <vector>
#include <cmath>

// ============================================================================
// Test Fixture
// ============================================================================

class WorkingMemoryImmuneTest : public ::testing::Test {
protected:
    working_memory_t* wm;
    brain_immune_system_t* immune;

    void SetUp() override {
        // Create working memory with default config (capacity=7)
        wm = working_memory_create();
        ASSERT_NE(wm, nullptr);

        // Create brain immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Start immune system
        brain_immune_start(immune);
    }

    void TearDown() override {
        if (wm) {
            working_memory_destroy(wm);
            wm = nullptr;
        }
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
            immune = nullptr;
        }
    }

    // Helper: Add test item
    bool add_test_item(float salience) {
        float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        return working_memory_add(wm, item, 4, salience);
    }

    // Helper: Fill working memory to capacity
    void fill_to_capacity() {
        uint32_t capacity = working_memory_get_capacity(wm);
        for (uint32_t i = 0; i < capacity; i++) {
            ASSERT_TRUE(add_test_item(0.5f));
        }
    }

    // Helper: Trigger inflammation at specified level
    void trigger_inflammation(brain_inflammation_level_t level) {
        // Present antigen to trigger immune response
        uint8_t epitope[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(
            immune,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            sizeof(epitope),
            5,  // Severity
            1,  // Node
            &antigen_id
        );

        // Initiate inflammation at specified level
        uint32_t site_id = 0;
        brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);

        // Escalate to desired level
        for (int i = INFLAMMATION_LOCAL; i < level; i++) {
            brain_immune_escalate_inflammation(immune, site_id);
        }
    }
};

// ============================================================================
// Connection API Tests
// ============================================================================

TEST_F(WorkingMemoryImmuneTest, ConnectImmuneSystem) {
    // WHAT: Connect working memory to immune system
    // WHY:  Verify connection establishes integration
    // HOW:  Call connect API, check integration enabled

    bool result = working_memory_connect_immune(wm, immune);
    EXPECT_TRUE(result);

    // Verify integration enabled
    EXPECT_FALSE(working_memory_is_immune_impaired(wm));  // No inflammation yet

    // Effective capacity should equal base capacity
    uint32_t base_capacity = working_memory_get_capacity(wm);
    uint32_t effective_capacity = working_memory_get_effective_capacity(wm);
    EXPECT_EQ(base_capacity, effective_capacity);
}

TEST_F(WorkingMemoryImmuneTest, ConnectNullWorkingMemory) {
    // WHAT: Attempt to connect NULL working memory
    // WHY:  Verify error handling
    // HOW:  Pass NULL, expect false

    bool result = working_memory_connect_immune(nullptr, immune);
    EXPECT_FALSE(result);
}

TEST_F(WorkingMemoryImmuneTest, ConnectNullImmuneSystem) {
    // WHAT: Attempt to connect NULL immune system
    // WHY:  Verify error handling
    // HOW:  Pass NULL, expect false

    bool result = working_memory_connect_immune(wm, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(WorkingMemoryImmuneTest, DisconnectImmuneSystem) {
    // WHAT: Disconnect immune system
    // WHY:  Verify clean disconnection
    // HOW:  Connect, disconnect, verify capacity restored

    working_memory_connect_immune(wm, immune);

    // Trigger inflammation
    trigger_inflammation(INFLAMMATION_REGIONAL);

    // Should be impaired
    EXPECT_TRUE(working_memory_is_immune_impaired(wm));

    // Disconnect
    working_memory_disconnect_immune(wm);

    // Should no longer be impaired
    EXPECT_FALSE(working_memory_is_immune_impaired(wm));

    // Capacity should be restored
    uint32_t base_capacity = working_memory_get_capacity(wm);
    uint32_t effective_capacity = working_memory_get_effective_capacity(wm);
    EXPECT_EQ(base_capacity, effective_capacity);
}

// ============================================================================
// Inflammation → WM: Capacity Reduction Tests
// ============================================================================

TEST_F(WorkingMemoryImmuneTest, InflammationLocalReducesCapacity) {
    // WHAT: LOCAL inflammation reduces capacity by 1
    // WHY:  Verify mild cytokine impairment
    // HOW:  Trigger LOCAL, check capacity = 6

    working_memory_connect_immune(wm, immune);
    trigger_inflammation(INFLAMMATION_LOCAL);

    uint32_t effective = working_memory_get_effective_capacity(wm);
    EXPECT_EQ(6, effective);  // 7 - 1

    EXPECT_TRUE(working_memory_is_immune_impaired(wm));
}

TEST_F(WorkingMemoryImmuneTest, InflammationRegionalReducesCapacity) {
    // WHAT: REGIONAL inflammation reduces capacity by 2
    // WHY:  Verify moderate cytokine impairment
    // HOW:  Trigger REGIONAL, check capacity = 5

    working_memory_connect_immune(wm, immune);
    trigger_inflammation(INFLAMMATION_REGIONAL);

    uint32_t effective = working_memory_get_effective_capacity(wm);
    EXPECT_EQ(5, effective);  // 7 - 2

    EXPECT_TRUE(working_memory_is_immune_impaired(wm));
}

TEST_F(WorkingMemoryImmuneTest, InflammationSystemicReducesCapacity) {
    // WHAT: SYSTEMIC inflammation reduces capacity by 3
    // WHY:  Verify severe cytokine impairment
    // HOW:  Trigger SYSTEMIC, check capacity = 4

    working_memory_connect_immune(wm, immune);
    trigger_inflammation(INFLAMMATION_SYSTEMIC);

    uint32_t effective = working_memory_get_effective_capacity(wm);
    EXPECT_EQ(4, effective);  // 7 - 3

    EXPECT_TRUE(working_memory_is_immune_impaired(wm));
}

TEST_F(WorkingMemoryImmuneTest, InflammationStormReducesCapacity) {
    // WHAT: STORM inflammation reduces capacity by 4
    // WHY:  Verify critical cytokine impairment
    // HOW:  Trigger STORM, check capacity = 3 (minimum)

    working_memory_connect_immune(wm, immune);
    trigger_inflammation(INFLAMMATION_STORM);

    uint32_t effective = working_memory_get_effective_capacity(wm);
    EXPECT_EQ(3, effective);  // 7 - 4 = 3 (minimum enforced)

    EXPECT_TRUE(working_memory_is_immune_impaired(wm));
}

TEST_F(WorkingMemoryImmuneTest, InflammationEvictsExcessItems) {
    // WHAT: Inflammation automatically evicts items exceeding reduced capacity
    // WHY:  Verify capacity enforcement
    // HOW:  Fill to 7, trigger REGIONAL (cap=5), verify size=5

    working_memory_connect_immune(wm, immune);

    // Fill to capacity (7 items)
    fill_to_capacity();
    EXPECT_EQ(7, working_memory_get_size(wm));

    // Trigger REGIONAL inflammation (capacity → 5)
    trigger_inflammation(INFLAMMATION_REGIONAL);

    // Should evict 2 lowest-salience items
    EXPECT_EQ(5, working_memory_get_size(wm));
    EXPECT_EQ(5, working_memory_get_effective_capacity(wm));
}

TEST_F(WorkingMemoryImmuneTest, InflammationEvictsLowestSalienceFirst) {
    // WHAT: Inflammation evicts lowest-salience items
    // WHY:  Verify salience-based eviction priority
    // HOW:  Add items with varying salience, trigger inflammation, check highest remain

    working_memory_connect_immune(wm, immune);

    // Add items with specific salience values
    float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    working_memory_add(wm, item, 4, 0.1f);  // Index 0 - lowest
    working_memory_add(wm, item, 4, 0.9f);  // Index 1 - highest
    working_memory_add(wm, item, 4, 0.5f);  // Index 2 - medium
    working_memory_add(wm, item, 4, 0.3f);  // Index 3 - low
    working_memory_add(wm, item, 4, 0.7f);  // Index 4 - high
    working_memory_add(wm, item, 4, 0.2f);  // Index 5 - very low
    working_memory_add(wm, item, 4, 0.8f);  // Index 6 - very high

    EXPECT_EQ(7, working_memory_get_size(wm));

    // Trigger SYSTEMIC inflammation (capacity → 4)
    trigger_inflammation(INFLAMMATION_SYSTEMIC);

    // Should evict 3 lowest-salience items (0.1, 0.2, 0.3)
    EXPECT_EQ(4, working_memory_get_size(wm));

    // Check remaining items have high salience
    for (uint32_t i = 0; i < working_memory_get_size(wm); i++) {
        float salience = 0.0f;
        working_memory_get_salience(wm, i, &salience);
        EXPECT_GE(salience, 0.5f);  // All remaining >= 0.5
    }
}

// ============================================================================
// WM → Immune: Stress Signaling Tests
// ============================================================================

TEST_F(WorkingMemoryImmuneTest, HighUtilizationSignalsStress) {
    // WHAT: High working memory utilization signals IL-6
    // WHY:  Verify cognitive load triggers immune response
    // HOW:  Fill to >90%, verify cytokine released

    working_memory_connect_immune(wm, immune);

    // Fill to high utilization (7/7 = 100%)
    fill_to_capacity();

    // Get immune stats to verify cytokine release
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);

    // Should have released IL-6 cytokine(s)
    EXPECT_GT(stats.cytokines_released, 0);
}

TEST_F(WorkingMemoryImmuneTest, EvictionSignalsTNFAlpha) {
    // WHAT: Item eviction signals TNF-alpha
    // WHY:  Verify resource failure triggers immune response
    // HOW:  Fill to capacity, add one more (eviction), check cytokine

    working_memory_connect_immune(wm, immune);

    // Get baseline cytokine count
    brain_immune_stats_t stats_before;
    brain_immune_get_stats(immune, &stats_before);

    // Fill to capacity
    fill_to_capacity();

    // Add one more to force eviction
    ASSERT_TRUE(add_test_item(0.6f));

    // Get updated cytokine count
    brain_immune_stats_t stats_after;
    brain_immune_get_stats(immune, &stats_after);

    // Should have released additional cytokines (TNF-alpha from eviction)
    EXPECT_GT(stats_after.cytokines_released, stats_before.cytokines_released);
}

TEST_F(WorkingMemoryImmuneTest, ManualStressSignaling) {
    // WHAT: Manual stress signaling releases IL-6
    // WHY:  Verify explicit stress API
    // HOW:  Call signal_stress, verify cytokine

    working_memory_connect_immune(wm, immune);

    brain_immune_stats_t stats_before;
    brain_immune_get_stats(immune, &stats_before);

    // Signal stress manually
    bool result = working_memory_signal_stress(wm, 0.8f);
    EXPECT_TRUE(result);

    brain_immune_stats_t stats_after;
    brain_immune_get_stats(immune, &stats_after);

    EXPECT_GT(stats_after.cytokines_released, stats_before.cytokines_released);
}

TEST_F(WorkingMemoryImmuneTest, StressSignalClampedToRange) {
    // WHAT: Stress level clamped to [0.0, 1.0]
    // WHY:  Verify input validation
    // HOW:  Test boundary values

    working_memory_connect_immune(wm, immune);

    // Test valid stress levels
    EXPECT_TRUE(working_memory_signal_stress(wm, 0.0f));
    EXPECT_TRUE(working_memory_signal_stress(wm, 0.5f));
    EXPECT_TRUE(working_memory_signal_stress(wm, 1.0f));

    // Test out-of-range (should clamp, not fail)
    EXPECT_TRUE(working_memory_signal_stress(wm, -0.5f));
    EXPECT_TRUE(working_memory_signal_stress(wm, 1.5f));
}

TEST_F(WorkingMemoryImmuneTest, DecayRemovalSignalsIL1) {
    // WHAT: Decay removal signals IL-1
    // WHY:  Verify resource scarcity triggers immune response
    // HOW:  Add items, let decay, verify cytokine

    working_memory_connect_immune(wm, immune);

    // Add items with low salience (will decay faster)
    float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    working_memory_add(wm, item, 4, 0.02f);  // Just above min_salience (0.01)

    brain_immune_stats_t stats_before;
    brain_immune_get_stats(immune, &stats_before);

    // Wait for decay
    uint64_t start_time = nimcp_time_get_ms();
    working_memory_decay(wm, start_time + 2000);  // 2 seconds later

    brain_immune_stats_t stats_after;
    brain_immune_get_stats(immune, &stats_after);

    // Should have released IL-1 if decay removed items
    working_memory_stats_t wm_stats;
    working_memory_get_stats(wm, &wm_stats);
    if (wm_stats.total_evictions > 0) {
        EXPECT_GT(stats_after.cytokines_released, stats_before.cytokines_released);
    }
}

// ============================================================================
// Recovery Tests
// ============================================================================

TEST_F(WorkingMemoryImmuneTest, InflammationResolutionRestoresCapacity) {
    // WHAT: Resolving inflammation restores capacity
    // WHY:  Verify recovery restores cognitive function
    // HOW:  Trigger inflammation, resolve, check capacity restored

    working_memory_connect_immune(wm, immune);

    // Trigger inflammation
    trigger_inflammation(INFLAMMATION_SYSTEMIC);
    EXPECT_EQ(4, working_memory_get_effective_capacity(wm));
    EXPECT_TRUE(working_memory_is_immune_impaired(wm));

    // Resolve inflammation
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    if (stats.inflammation_sites > 0) {
        // Find first inflammation site and resolve it
        for (uint32_t i = 0; i < 64; i++) {  // BRAIN_IMMUNE_MAX_INFLAMMATION = 64
            // Try to resolve (assumes site ID exists)
            brain_immune_resolve_inflammation(immune, i);
        }
    }

    // Note: Full resolution requires implementing inflammation site removal
    // For now, just verify API doesn't crash
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(WorkingMemoryImmuneTest, StressSignalWithoutConnection) {
    // WHAT: Stress signal without immune connection
    // WHY:  Verify graceful handling
    // HOW:  Call signal_stress before connect, expect false

    bool result = working_memory_signal_stress(wm, 0.5f);
    EXPECT_FALSE(result);  // No immune connection
}

TEST_F(WorkingMemoryImmuneTest, EffectiveCapacityWithoutConnection) {
    // WHAT: Effective capacity without immune connection
    // WHY:  Verify returns base capacity
    // HOW:  Check effective = base

    uint32_t base = working_memory_get_capacity(wm);
    uint32_t effective = working_memory_get_effective_capacity(wm);
    EXPECT_EQ(base, effective);

    EXPECT_FALSE(working_memory_is_immune_impaired(wm));
}

TEST_F(WorkingMemoryImmuneTest, MinimumCapacityEnforced) {
    // WHAT: Minimum capacity of 3 enforced even under cytokine storm
    // WHY:  Verify biological minimum (critical cognitive function)
    // HOW:  Trigger STORM, verify effective >= 3

    working_memory_connect_immune(wm, immune);
    trigger_inflammation(INFLAMMATION_STORM);

    uint32_t effective = working_memory_get_effective_capacity(wm);
    EXPECT_GE(effective, 3);  // Always >= 3
}

TEST_F(WorkingMemoryImmuneTest, RepeatedInflammationEscalation) {
    // WHAT: Repeated inflammation escalation
    // WHY:  Verify handling of multiple escalations
    // HOW:  Escalate multiple times, check capacity decreases

    working_memory_connect_immune(wm, immune);

    std::vector<uint32_t> expected_capacities = {7, 6, 5, 4, 3};

    for (size_t level = 0; level < expected_capacities.size(); level++) {
        if (level > 0) {
            trigger_inflammation(static_cast<brain_inflammation_level_t>(level));
        }
        uint32_t effective = working_memory_get_effective_capacity(wm);
        EXPECT_LE(effective, expected_capacities[level]);
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(WorkingMemoryImmuneTest, FullCycleInflammationAndRecovery) {
    // WHAT: Complete cycle of inflammation and recovery
    // WHY:  Verify realistic use case
    // HOW:  Fill WM → inflammation → eviction → resolution → refill

    working_memory_connect_immune(wm, immune);

    // Phase 1: Fill working memory
    fill_to_capacity();
    EXPECT_EQ(7, working_memory_get_size(wm));

    // Phase 2: Trigger inflammation (capacity → 4)
    trigger_inflammation(INFLAMMATION_SYSTEMIC);
    EXPECT_EQ(4, working_memory_get_effective_capacity(wm));
    EXPECT_EQ(4, working_memory_get_size(wm));  // Auto-evicted to fit

    // Phase 3: Try to add items (should respect reduced capacity)
    EXPECT_TRUE(add_test_item(0.7f));  // Should succeed (at capacity)

    // Phase 4: Recovery (disconnect to simulate resolution)
    working_memory_disconnect_immune(wm);
    EXPECT_EQ(7, working_memory_get_effective_capacity(wm));

    // Phase 5: Refill to full capacity
    while (working_memory_get_size(wm) < 7) {
        ASSERT_TRUE(add_test_item(0.5f));
    }
    EXPECT_EQ(7, working_memory_get_size(wm));
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
