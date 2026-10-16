/**
 * @file e2e_test_memory_immune_pipeline.cpp
 * @brief End-to-end tests for memory-immune integration pipeline
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_memory_immune_integration.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/consolidation/nimcp_consolidation.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class MemoryImmunePipelineTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    working_memory_t* working_memory;
    memory_immune_integration_t* integration;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Start immune system */
        brain_immune_start(immune_system);

        /* Create working memory */
        working_memory = working_memory_create();
        ASSERT_NE(working_memory, nullptr);

        /* Create integration with default config */
        integration = memory_immune_integration_create(
            immune_system, working_memory, nullptr, nullptr
        );
        ASSERT_NE(integration, nullptr);

        memory_immune_integration_start(integration);
    }

    void TearDown() override {
        if (integration) {
            memory_immune_integration_stop(integration);
            memory_immune_integration_destroy(integration);
        }
        if (working_memory) {
            working_memory_destroy(working_memory);
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
    }

    /* Helper: Simulate time passage */
    void simulate_time(uint64_t ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};

/* ============================================================================
 * End-to-End Pipeline Tests
 * ============================================================================ */

TEST_F(MemoryImmunePipelineTest, E2E_CompleteImmuneResponseCycle) {
    /* WHAT: Complete immune response cycle affecting memory throughout */

    /* === PHASE 1: Baseline (Surveillance) === */
    memory_immune_metrics_t metrics;
    memory_immune_get_metrics(integration, &metrics);

    EXPECT_EQ(metrics.state, MEM_IMMUNE_NORMAL);
    EXPECT_EQ(metrics.current_wm_capacity, WM_CAPACITY_BASELINE);
    EXPECT_FLOAT_EQ(metrics.encoding_strength_multiplier, 1.0f);

    /* Add items to working memory at baseline */
    for (int i = 0; i < 7; i++) {
        float item[16];
        for (int j = 0; j < 16; j++) {
            item[j] = static_cast<float>(i * 16 + j);
        }
        bool added = working_memory_add(working_memory, item, 16, 0.7f);
        EXPECT_TRUE(added);
    }
    EXPECT_EQ(working_memory_get_size(working_memory), 7u);

    /* === PHASE 2: Threat Detection → Early Response === */
    /* Low IL-1β concentration (enhancement) */
    integration->metrics.il1_concentration = 0.15f;
    integration->metrics.tnf_concentration = 0.05f;
    integration->metrics.il10_concentration = 0.1f;

    float encoding_strength = memory_immune_compute_encoding_strength(integration);
    EXPECT_GT(encoding_strength, 1.0f);  /* Enhanced encoding */

    /* === PHASE 3: Inflammation → Impairment === */
    /* Simulate systemic inflammation */
    integration->metrics.inflammation_level = INFLAMMATION_REGIONAL;
    integration->metrics.active_inflammation_sites = 4;
    integration->metrics.il1_concentration = 0.7f;
    integration->metrics.tnf_concentration = 0.6f;
    integration->metrics.il6_concentration = 0.5f;
    integration->metrics.il10_concentration = 0.1f;

    /* Update state */
    memory_immune_update_state(integration, 5000);

    /* Verify state is impaired */
    EXPECT_EQ(memory_immune_get_state(integration), MEM_IMMUNE_IMPAIRED);

    /* Verify capacity reduced */
    uint32_t impaired_capacity = memory_immune_update_wm_capacity(integration);
    EXPECT_LT(impaired_capacity, WM_CAPACITY_BASELINE);
    EXPECT_EQ(impaired_capacity, WM_CAPACITY_MODERATE_INFLAMMATION);

    /* Verify encoding impaired */
    encoding_strength = memory_immune_compute_encoding_strength(integration);
    EXPECT_LT(encoding_strength, 1.0f);

    /* Verify decay accelerated */
    float decay_mult = memory_immune_update_wm_decay_rate(integration);
    EXPECT_GT(decay_mult, 1.0f);

    /* === PHASE 4: Memory Formation === */
    /* Transition to memory phase */
    integration->metrics.immune_phase = IMMUNE_PHASE_MEMORY;

    /* Create immune-cognitive memory links */
    memory_immune_create_memory_link(
        integration, 101, true, "threat_pathogen_X", 0.95f
    );
    memory_immune_create_memory_link(
        integration, 102, false, "attack_context_Y", 0.9f
    );
    memory_immune_create_memory_link(
        integration, 103, true, "danger_signal_Z", 0.85f
    );

    /* Verify memory links */
    size_t link_count = 0;
    const immune_cognitive_memory_link_t* links =
        memory_immune_get_memory_links(integration, &link_count);
    EXPECT_EQ(link_count, 3u);

    /* Verify consolidation boost */
    float consolidation_boost = memory_immune_get_consolidation_boost(integration);
    EXPECT_GT(consolidation_boost, 1.0f);
    EXPECT_EQ(consolidation_boost, CONSOLIDATION_IMMUNE_MEMORY_BOOST);

    /* === PHASE 5: Resolution → Recovery === */
    /* Anti-inflammatory response */
    integration->metrics.inflammation_level = INFLAMMATION_LOCAL;
    integration->metrics.active_inflammation_sites = 1;
    integration->metrics.il1_concentration = 0.2f;
    integration->metrics.tnf_concentration = 0.1f;
    integration->metrics.il6_concentration = 0.15f;
    integration->metrics.il10_concentration = 0.8f;  /* High anti-inflammatory */
    integration->metrics.immune_phase = IMMUNE_PHASE_RESOLUTION;

    /* Update state */
    memory_immune_update_state(integration, 10000);

    /* Verify state is recovering */
    EXPECT_EQ(memory_immune_get_state(integration), MEM_IMMUNE_RECOVERING);

    /* === PHASE 6: Complete Resolution === */
    integration->metrics.inflammation_level = INFLAMMATION_NONE;
    integration->metrics.active_inflammation_sites = 0;
    integration->metrics.il1_concentration = 0.0f;
    integration->metrics.tnf_concentration = 0.0f;
    integration->metrics.il6_concentration = 0.0f;
    integration->metrics.il10_concentration = 0.2f;

    /* Update state */
    memory_immune_update_state(integration, 15000);

    /* Verify capacity restored */
    uint32_t restored_capacity = memory_immune_update_wm_capacity(integration);
    EXPECT_EQ(restored_capacity, WM_CAPACITY_BASELINE);

    /* Verify encoding normalized */
    encoding_strength = memory_immune_compute_encoding_strength(integration);
    EXPECT_NEAR(encoding_strength, 1.0f, 0.2f);

    /* === PHASE 7: Memory Persistence === */
    /* Reactivate immune memory cells */
    memory_immune_reactivate_linked_pattern(integration, 101);
    memory_immune_reactivate_linked_pattern(integration, 102);

    /* Verify reactivations */
    links = memory_immune_get_memory_links(integration, &link_count);
    EXPECT_EQ(links[0].reactivation_count, 1u);
    EXPECT_EQ(links[1].reactivation_count, 1u);
    EXPECT_EQ(links[2].reactivation_count, 0u);

    /* === PHASE 8: Statistics Verification === */
    memory_immune_stats_t stats;
    memory_immune_get_stats(integration, &stats);

    EXPECT_GT(stats.state_changes, 0u);
    EXPECT_GT(stats.impairment_episodes, 0u);
    EXPECT_GT(stats.wm_capacity_reductions, 0u);
    EXPECT_GT(stats.wm_capacity_restorations, 0u);
    EXPECT_EQ(stats.immune_memories_consolidated, 3u);
    EXPECT_LT(stats.avg_wm_capacity_ratio, 1.0f);  /* Was impaired at some point */
}

TEST_F(MemoryImmunePipelineTest, E2E_CytokineModulationSequence) {
    /* WHAT: Test sequential cytokine modulation of memory */

    /* Scenario: IL-1β dose-response curve */

    /* Phase 1: Low dose IL-1β (enhancement) */
    integration->metrics.il1_concentration = 0.1f;
    integration->metrics.tnf_concentration = 0.05f;
    integration->config.il1_low_dose_threshold = 0.2f;

    float strength = memory_immune_compute_encoding_strength(integration);
    EXPECT_GT(strength, 1.0f);  /* Enhanced */

    /* Phase 2: Medium dose IL-1β (neutral) */
    integration->metrics.il1_concentration = 0.4f;
    strength = memory_immune_compute_encoding_strength(integration);
    EXPECT_NEAR(strength, 1.0f, 0.2f);  /* Neutral */

    /* Phase 3: High dose IL-1β (impairment) */
    integration->metrics.il1_concentration = 0.8f;
    integration->config.il1_high_dose_threshold = 0.6f;
    strength = memory_immune_compute_encoding_strength(integration);
    EXPECT_LT(strength, 1.0f);  /* Impaired */

    /* Phase 4: Add TNF-α (further impairment) */
    integration->metrics.tnf_concentration = 0.7f;
    integration->config.tnf_impairment_threshold = 0.4f;
    strength = memory_immune_compute_encoding_strength(integration);
    EXPECT_LT(strength, 0.8f);  /* Severely impaired */

    /* Phase 5: IL-10 rescue */
    integration->metrics.il10_concentration = 0.9f;
    strength = memory_immune_compute_encoding_strength(integration);
    EXPECT_GT(strength, 0.5f);  /* Partial rescue (IL-10 raises above clamp floor) */
}

TEST_F(MemoryImmunePipelineTest, E2E_WorkingMemoryCapacityProgression) {
    /* WHAT: Test working memory capacity changes through inflammation levels */

    /* Baseline */
    EXPECT_EQ(memory_immune_update_wm_capacity(integration), WM_CAPACITY_BASELINE);

    /* Mild inflammation (local) */
    integration->metrics.inflammation_level = INFLAMMATION_LOCAL;
    EXPECT_EQ(memory_immune_update_wm_capacity(integration),
              WM_CAPACITY_MILD_INFLAMMATION);

    /* Moderate inflammation (regional) */
    integration->metrics.inflammation_level = INFLAMMATION_REGIONAL;
    EXPECT_EQ(memory_immune_update_wm_capacity(integration),
              WM_CAPACITY_MODERATE_INFLAMMATION);

    /* Severe inflammation (systemic) */
    integration->metrics.inflammation_level = INFLAMMATION_SYSTEMIC;
    EXPECT_EQ(memory_immune_update_wm_capacity(integration),
              WM_CAPACITY_SEVERE_INFLAMMATION);

    /* Critical inflammation (storm) */
    integration->metrics.inflammation_level = INFLAMMATION_STORM;
    EXPECT_EQ(memory_immune_update_wm_capacity(integration),
              WM_CAPACITY_SEVERE_INFLAMMATION);

    /* Recovery progression */
    integration->metrics.inflammation_level = INFLAMMATION_REGIONAL;
    EXPECT_EQ(memory_immune_update_wm_capacity(integration),
              WM_CAPACITY_MODERATE_INFLAMMATION);

    integration->metrics.inflammation_level = INFLAMMATION_LOCAL;
    EXPECT_EQ(memory_immune_update_wm_capacity(integration),
              WM_CAPACITY_MILD_INFLAMMATION);

    integration->metrics.inflammation_level = INFLAMMATION_NONE;
    EXPECT_EQ(memory_immune_update_wm_capacity(integration),
              WM_CAPACITY_BASELINE);
}

TEST_F(MemoryImmunePipelineTest, E2E_ImmuneMemoryReactivation) {
    /* WHAT: Test immune memory cell reactivation triggers cognitive recall */

    /* Create diverse memory links */
    struct {
        uint32_t cell_id;
        bool is_b_cell;
        const char* pattern;
        float importance;
    } memories[] = {
        {1, true, "bacterial_pathogen_A", 0.95f},
        {2, true, "viral_pathogen_B", 0.9f},
        {3, false, "inflammation_context_C", 0.85f},
        {4, false, "tissue_damage_pattern_D", 0.8f},
        {5, true, "toxin_signature_E", 0.92f}
    };

    for (auto& mem : memories) {
        memory_immune_create_memory_link(
            integration, mem.cell_id, mem.is_b_cell, mem.pattern, mem.importance
        );
    }

    /* Verify all created */
    size_t count = 0;
    const immune_cognitive_memory_link_t* links =
        memory_immune_get_memory_links(integration, &count);
    EXPECT_EQ(count, 5u);

    /* Reactivate selective memories */
    EXPECT_EQ(memory_immune_reactivate_linked_pattern(integration, 1), 0);
    EXPECT_EQ(memory_immune_reactivate_linked_pattern(integration, 3), 0);
    EXPECT_EQ(memory_immune_reactivate_linked_pattern(integration, 5), 0);

    /* Verify reactivation counts */
    links = memory_immune_get_memory_links(integration, &count);
    EXPECT_EQ(links[0].reactivation_count, 1u);  /* Cell 1 */
    EXPECT_EQ(links[1].reactivation_count, 0u);  /* Cell 2 */
    EXPECT_EQ(links[2].reactivation_count, 1u);  /* Cell 3 */
    EXPECT_EQ(links[3].reactivation_count, 0u);  /* Cell 4 */
    EXPECT_EQ(links[4].reactivation_count, 1u);  /* Cell 5 */

    /* Multiple reactivations strengthen memory */
    for (int i = 0; i < 5; i++) {
        memory_immune_reactivate_linked_pattern(integration, 1);
    }

    links = memory_immune_get_memory_links(integration, &count);
    EXPECT_EQ(links[0].reactivation_count, 6u);
    EXPECT_GT(links[0].memory_strength, 1.0f);  /* Strengthened */
}

TEST_F(MemoryImmunePipelineTest, E2E_ConsolidationWithImmuneMemory) {
    /* WHAT: Test consolidation boost during immune memory formation */

    /* Normal phase - no boost */
    integration->metrics.immune_phase = IMMUNE_PHASE_SURVEILLANCE;
    EXPECT_FLOAT_EQ(memory_immune_get_consolidation_boost(integration), 1.0f);

    integration->metrics.immune_phase = IMMUNE_PHASE_EFFECTOR;
    EXPECT_FLOAT_EQ(memory_immune_get_consolidation_boost(integration), 1.0f);

    /* Memory phase - boost active */
    integration->metrics.immune_phase = IMMUNE_PHASE_MEMORY;
    float boost = memory_immune_get_consolidation_boost(integration);
    EXPECT_GT(boost, 1.0f);
    EXPECT_EQ(boost, CONSOLIDATION_IMMUNE_MEMORY_BOOST);

    /* Create memories during this phase */
    memory_immune_create_memory_link(integration, 10, true, "memory_A", 0.9f);
    memory_immune_create_memory_link(integration, 11, false, "memory_B", 0.85f);

    /* Verify statistics */
    memory_immune_stats_t stats;
    memory_immune_get_stats(integration, &stats);
    EXPECT_EQ(stats.immune_memories_consolidated, 2u);
}

TEST_F(MemoryImmunePipelineTest, E2E_StatisticsAccumulation) {
    /* WHAT: Verify comprehensive statistics tracking */

    /* Trigger various state changes */
    integration->metrics.inflammation_level = INFLAMMATION_REGIONAL;
    memory_immune_update_state(integration, 1000);

    integration->metrics.inflammation_level = INFLAMMATION_NONE;
    memory_immune_update_state(integration, 2000);

    /* Create memories */
    for (int i = 0; i < 5; i++) {
        memory_immune_create_memory_link(
            integration, 100 + i, true, "pattern", 0.8f
        );
    }

    /* Trigger encoding changes */
    integration->metrics.il1_concentration = 0.1f;
    memory_immune_compute_encoding_strength(integration);

    integration->metrics.il1_concentration = 0.8f;
    memory_immune_compute_encoding_strength(integration);

    /* Get final statistics */
    memory_immune_stats_t stats;
    memory_immune_get_stats(integration, &stats);

    /* Verify tracking */
    EXPECT_GT(stats.total_updates, 0u);
    EXPECT_EQ(stats.immune_memories_consolidated, 5u);
    EXPECT_GT(stats.state_changes, 0u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
