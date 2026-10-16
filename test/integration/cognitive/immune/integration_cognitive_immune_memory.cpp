/**
 * @file integration_cognitive_immune_memory.cpp
 * @brief Integration tests for memory-immune system interaction
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_memory_immune_integration.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/consolidation/nimcp_consolidation.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class MemoryImmuneIntegrationTest : public ::testing::Test {
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

        /* Create integration */
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
};

/* ============================================================================
 * Integration Scenarios
 * ============================================================================ */

TEST_F(MemoryImmuneIntegrationTest, InflammationImpairsWorkingMemory) {
    /* WHAT: Test that inflammation reduces working memory capacity */

    /* Initial state: normal capacity */
    memory_immune_metrics_t metrics;
    memory_immune_get_metrics(integration, &metrics);
    EXPECT_EQ(metrics.current_wm_capacity, WM_CAPACITY_BASELINE);

    /* Simulate inflammation */
    integration->metrics.inflammation_level = INFLAMMATION_REGIONAL;
    integration->metrics.active_inflammation_sites = 3;

    /* Update capacity */
    uint32_t new_capacity = memory_immune_update_wm_capacity(integration);

    /* Verify capacity reduced */
    EXPECT_LT(new_capacity, WM_CAPACITY_BASELINE);
    EXPECT_EQ(new_capacity, WM_CAPACITY_MODERATE_INFLAMMATION);

    /* Verify working memory is impacted */
    memory_immune_get_metrics(integration, &metrics);
    EXPECT_LT(metrics.wm_capacity_ratio, 1.0f);
}

TEST_F(MemoryImmuneIntegrationTest, CytokinesModulateEncoding) {
    /* WHAT: Test that cytokines modulate memory encoding strength */

    /* Low IL-1β: should enhance encoding */
    integration->metrics.il1_concentration = 0.15f;
    integration->metrics.tnf_concentration = 0.05f;
    integration->metrics.il10_concentration = 0.1f;

    float encoding_strength = memory_immune_compute_encoding_strength(integration);
    EXPECT_GT(encoding_strength, 1.0f);  /* Enhanced */

    /* High IL-1β: should impair encoding */
    integration->metrics.il1_concentration = 0.8f;
    encoding_strength = memory_immune_compute_encoding_strength(integration);
    EXPECT_LT(encoding_strength, 1.0f);  /* Impaired */

    /* High TNF-α: should impair encoding */
    integration->metrics.il1_concentration = 0.1f;
    integration->metrics.tnf_concentration = 0.7f;
    encoding_strength = memory_immune_compute_encoding_strength(integration);
    EXPECT_LT(encoding_strength, 1.0f);  /* Impaired */
}

TEST_F(MemoryImmuneIntegrationTest, ImmuneMemoryFormationDuringConsolidation) {
    /* WHAT: Test that immune memories form during consolidation */

    /* Set immune phase to memory formation */
    integration->metrics.immune_phase = IMMUNE_PHASE_MEMORY;

    /* Check consolidation boost */
    float boost = memory_immune_get_consolidation_boost(integration);
    EXPECT_GT(boost, 1.0f);

    /* Create immune-cognitive memory link */
    int result = memory_immune_create_memory_link(
        integration,
        100,  /* B cell ID */
        true,  /* is_b_cell */
        "threat_pattern_A",
        0.9f
    );
    EXPECT_EQ(result, 0);

    /* Verify link created */
    size_t link_count = 0;
    const immune_cognitive_memory_link_t* links =
        memory_immune_get_memory_links(integration, &link_count);
    EXPECT_EQ(link_count, 1u);
    EXPECT_STREQ(links[0].pattern_name, "threat_pattern_A");
}

TEST_F(MemoryImmuneIntegrationTest, InflammationResolutionRestoresCapacity) {
    /* WHAT: Test capacity restoration after inflammation resolves */

    /* Start with inflammation */
    integration->metrics.inflammation_level = INFLAMMATION_SYSTEMIC;
    uint32_t impaired_capacity = memory_immune_update_wm_capacity(integration);
    EXPECT_LT(impaired_capacity, WM_CAPACITY_BASELINE);

    /* Resolve inflammation */
    integration->metrics.inflammation_level = INFLAMMATION_NONE;
    integration->metrics.active_inflammation_sites = 0;
    integration->metrics.immune_phase = IMMUNE_PHASE_RESOLUTION;

    /* Update state */
    memory_immune_update_state(integration, 1000);

    /* Verify capacity restored */
    uint32_t restored_capacity = memory_immune_update_wm_capacity(integration);
    EXPECT_EQ(restored_capacity, WM_CAPACITY_BASELINE);

    /* Verify state is recovering */
    memory_immune_state_t state = memory_immune_get_state(integration);
    EXPECT_EQ(state, MEM_IMMUNE_RECOVERING);
}

TEST_F(MemoryImmuneIntegrationTest, CytokineStormSeverelyImpairs) {
    /* WHAT: Test that cytokine storm severely impairs memory */

    /* Simulate cytokine storm */
    integration->metrics.inflammation_level = INFLAMMATION_STORM;
    integration->metrics.active_inflammation_sites = 15;
    integration->metrics.il1_concentration = 0.9f;
    integration->metrics.tnf_concentration = 0.9f;
    integration->metrics.il6_concentration = 0.9f;

    /* Update state */
    memory_immune_update_state(integration, 1000);

    /* Verify state is STORM */
    EXPECT_EQ(memory_immune_get_state(integration), MEM_IMMUNE_STORM);

    /* Verify severe capacity reduction */
    uint32_t capacity = memory_immune_update_wm_capacity(integration);
    EXPECT_EQ(capacity, WM_CAPACITY_SEVERE_INFLAMMATION);

    /* Verify severe encoding impairment */
    float encoding = memory_immune_compute_encoding_strength(integration);
    EXPECT_LT(encoding, 0.8f);
}

TEST_F(MemoryImmuneIntegrationTest, ImmuneCognitiveMemoryCrossTalk) {
    /* WHAT: Test bidirectional cross-talk between immune and cognitive memory */

    /* Create multiple immune-cognitive links */
    memory_immune_create_memory_link(integration, 1, true, "pathogen_A", 0.9f);
    memory_immune_create_memory_link(integration, 2, false, "threat_context_B", 0.8f);
    memory_immune_create_memory_link(integration, 3, true, "danger_signal_C", 0.95f);

    /* Verify all links created */
    size_t link_count = 0;
    const immune_cognitive_memory_link_t* links =
        memory_immune_get_memory_links(integration, &link_count);
    EXPECT_EQ(link_count, 3u);

    /* Reactivate immune cell 1 */
    int result = memory_immune_reactivate_linked_pattern(integration, 1);
    EXPECT_EQ(result, 0);

    /* Verify reactivation count increased */
    links = memory_immune_get_memory_links(integration, &link_count);
    EXPECT_EQ(links[0].reactivation_count, 1u);

    /* Reactivate again */
    memory_immune_reactivate_linked_pattern(integration, 1);
    links = memory_immune_get_memory_links(integration, &link_count);
    EXPECT_EQ(links[0].reactivation_count, 2u);
}

TEST_F(MemoryImmuneIntegrationTest, WorkingMemoryUnderInflammation) {
    /* WHAT: Test working memory performance degrades under inflammation */

    /* Add items to working memory at baseline */
    for (int i = 0; i < 7; i++) {
        float item[10] = {(float)i, (float)i+1, (float)i+2};
        working_memory_add(working_memory, item, 10, 0.8f);
    }

    uint32_t baseline_size = working_memory_get_size(working_memory);
    EXPECT_EQ(baseline_size, 7u);  /* Full capacity */

    /* Induce inflammation */
    integration->metrics.inflammation_level = INFLAMMATION_REGIONAL;
    uint32_t reduced_capacity = memory_immune_update_wm_capacity(integration);
    EXPECT_EQ(reduced_capacity, WM_CAPACITY_MODERATE_INFLAMMATION);

    /* In real integration, WM capacity would be dynamically updated */
    /* Here we verify the integration correctly computes the reduced capacity */
    memory_immune_metrics_t metrics;
    memory_immune_get_metrics(integration, &metrics);
    EXPECT_EQ(metrics.current_wm_capacity, WM_CAPACITY_MODERATE_INFLAMMATION);
}

TEST_F(MemoryImmuneIntegrationTest, EncodingStrengthAffectsSalience) {
    /* WHAT: Test that encoding strength modulates item salience */

    float base_salience = 0.7f;

    /* Enhanced encoding (low IL-1β) */
    integration->metrics.encoding_strength_multiplier = 1.3f;
    float enhanced_salience = memory_immune_modulate_salience(
        integration, base_salience
    );
    EXPECT_GT(enhanced_salience, base_salience);

    /* Impaired encoding (high TNF-α) */
    integration->metrics.encoding_strength_multiplier = 0.7f;
    float impaired_salience = memory_immune_modulate_salience(
        integration, base_salience
    );
    EXPECT_LT(impaired_salience, base_salience);
}

TEST_F(MemoryImmuneIntegrationTest, StateTransitions) {
    /* WHAT: Test state transitions through immune response cycle */

    /* Start normal */
    EXPECT_EQ(memory_immune_get_state(integration), MEM_IMMUNE_NORMAL);

    /* Mild inflammation → Enhanced (low cytokines) */
    integration->metrics.inflammation_level = INFLAMMATION_NONE;
    integration->metrics.encoding_strength_multiplier = 1.2f;
    memory_immune_update_state(integration, 1000);
    /* State may be ENHANCED or NORMAL depending on thresholds */

    /* Moderate inflammation → Impaired */
    integration->metrics.inflammation_level = INFLAMMATION_REGIONAL;
    integration->metrics.encoding_strength_multiplier = 0.8f;
    memory_immune_update_state(integration, 2000);
    EXPECT_EQ(memory_immune_get_state(integration), MEM_IMMUNE_IMPAIRED);

    /* Resolution → Recovering */
    integration->metrics.inflammation_level = INFLAMMATION_NONE;
    integration->metrics.immune_phase = IMMUNE_PHASE_RESOLUTION;
    memory_immune_update_state(integration, 3000);
    EXPECT_EQ(memory_immune_get_state(integration), MEM_IMMUNE_RECOVERING);
}

TEST_F(MemoryImmuneIntegrationTest, StatisticsTracking) {
    /* WHAT: Test that statistics are properly tracked */

    /* Perform various operations */
    integration->metrics.inflammation_level = INFLAMMATION_REGIONAL;
    memory_immune_update_wm_capacity(integration);  /* Capacity reduction */

    integration->metrics.inflammation_level = INFLAMMATION_NONE;
    memory_immune_update_wm_capacity(integration);  /* Capacity restoration */

    memory_immune_create_memory_link(integration, 1, true, "test", 0.8f);

    /* Get statistics */
    memory_immune_stats_t stats;
    memory_immune_get_stats(integration, &stats);

    /* Verify tracking */
    EXPECT_GT(stats.wm_capacity_reductions, 0u);
    EXPECT_GT(stats.wm_capacity_restorations, 0u);
    EXPECT_EQ(stats.immune_memories_consolidated, 1u);
}

/* ============================================================================
 * End-to-End Scenario Tests
 * ============================================================================ */

TEST_F(MemoryImmuneIntegrationTest, E2E_ImmuneResponseCycle) {
    /* WHAT: End-to-end test of complete immune response affecting memory */

    memory_immune_stats_t stats;

    /* Phase 1: Surveillance (Normal) */
    EXPECT_EQ(memory_immune_get_state(integration), MEM_IMMUNE_NORMAL);
    memory_immune_get_metrics(integration, &integration->metrics);
    EXPECT_EQ(integration->metrics.current_wm_capacity, WM_CAPACITY_BASELINE);

    /* Phase 2: Antigen Detection → Activation */
    /* (Would present antigen to immune system in full integration) */

    /* Phase 3: Inflammation → Impairment */
    integration->metrics.inflammation_level = INFLAMMATION_REGIONAL;
    integration->metrics.active_inflammation_sites = 3;
    integration->metrics.il1_concentration = 0.7f;
    integration->metrics.tnf_concentration = 0.6f;

    memory_immune_update_state(integration, 5000);
    uint32_t impaired_capacity = memory_immune_update_wm_capacity(integration);

    EXPECT_EQ(memory_immune_get_state(integration), MEM_IMMUNE_IMPAIRED);
    EXPECT_LT(impaired_capacity, WM_CAPACITY_BASELINE);

    /* Phase 4: Memory Formation */
    integration->metrics.immune_phase = IMMUNE_PHASE_MEMORY;
    memory_immune_create_memory_link(integration, 42, true, "learned_threat", 0.95f);

    float consolidation_boost = memory_immune_get_consolidation_boost(integration);
    EXPECT_GT(consolidation_boost, 1.0f);

    /* Phase 5: Resolution → Recovery */
    integration->metrics.inflammation_level = INFLAMMATION_NONE;
    integration->metrics.active_inflammation_sites = 0;
    integration->metrics.immune_phase = IMMUNE_PHASE_RESOLUTION;
    integration->metrics.il1_concentration = 0.0f;   /* Pro-inflammatory resolved */
    integration->metrics.tnf_concentration = 0.0f;   /* Pro-inflammatory resolved */
    integration->metrics.il10_concentration = 0.2f;   /* Anti-inflammatory returning to baseline */

    memory_immune_update_state(integration, 10000);
    uint32_t restored_capacity = memory_immune_update_wm_capacity(integration);

    EXPECT_EQ(memory_immune_get_state(integration), MEM_IMMUNE_RECOVERING);
    EXPECT_EQ(restored_capacity, WM_CAPACITY_BASELINE);

    /* Verify statistics */
    memory_immune_get_stats(integration, &stats);
    EXPECT_GT(stats.impairment_episodes, 0u);
    EXPECT_GT(stats.wm_capacity_reductions, 0u);
    EXPECT_GT(stats.wm_capacity_restorations, 0u);
    EXPECT_EQ(stats.immune_memories_consolidated, 1u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
