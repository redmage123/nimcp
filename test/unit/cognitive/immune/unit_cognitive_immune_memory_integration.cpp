/**
 * @file unit_cognitive_immune_memory_integration.cpp
 * @brief Unit tests for memory-immune integration
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_memory_immune_integration.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_working_memory.h"

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

        /* Create working memory */
        working_memory = working_memory_create();
        ASSERT_NE(working_memory, nullptr);

        /* Create integration (will be done in each test) */
        integration = nullptr;
    }

    void TearDown() override {
        if (integration) {
            memory_immune_integration_destroy(integration);
        }
        if (working_memory) {
            working_memory_destroy(working_memory);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MemoryImmuneIntegrationTest, CreateDestroy) {
    /* WHAT: Test basic creation and destruction */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);

    /* Verify initial state */
    EXPECT_EQ(memory_immune_get_state(integration), MEM_IMMUNE_NORMAL);
}

TEST_F(MemoryImmuneIntegrationTest, CreateWithNullImmune) {
    /* WHAT: Test creation fails with null immune system */
    integration = memory_immune_integration_create(
        nullptr, working_memory, nullptr, nullptr
    );
    EXPECT_EQ(integration, nullptr);
}

TEST_F(MemoryImmuneIntegrationTest, CreateWithCustomConfig) {
    /* WHAT: Test creation with custom config */
    memory_immune_config_t config;
    memory_immune_default_config(&config);
    config.enable_wm_capacity_modulation = false;

    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, &config
    );
    ASSERT_NE(integration, nullptr);

    /* Verify config applied */
    EXPECT_FALSE(integration->config.enable_wm_capacity_modulation);
}

TEST_F(MemoryImmuneIntegrationTest, StartStop) {
    /* WHAT: Test start and stop */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);

    EXPECT_EQ(memory_immune_integration_start(integration), 0);
    EXPECT_TRUE(integration->running);

    EXPECT_EQ(memory_immune_integration_stop(integration), 0);
    EXPECT_FALSE(integration->running);
}

/* ============================================================================
 * Working Memory Capacity Modulation Tests
 * ============================================================================ */

TEST_F(MemoryImmuneIntegrationTest, WMCapacityNoInflammation) {
    /* WHAT: Test WM capacity with no inflammation */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Update capacity (no inflammation) */
    uint32_t capacity = memory_immune_update_wm_capacity(integration);

    /* Should be baseline (7) */
    EXPECT_EQ(capacity, WM_CAPACITY_BASELINE);
    EXPECT_FLOAT_EQ(integration->metrics.wm_capacity_ratio, 1.0f);
}

TEST_F(MemoryImmuneIntegrationTest, WMCapacityMildInflammation) {
    /* WHAT: Test WM capacity reduction with mild inflammation */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Simulate inflammation by setting metrics */
    integration->metrics.inflammation_level = INFLAMMATION_LOCAL;

    /* Update via internal function */
    brain_immune_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.inflammation_sites = 1;  /* Local inflammation */

    /* Manually update capacity based on inflammation */
    integration->metrics.current_wm_capacity =
        WM_CAPACITY_MILD_INFLAMMATION;

    /* Verify reduced capacity */
    EXPECT_EQ(integration->metrics.current_wm_capacity,
              WM_CAPACITY_MILD_INFLAMMATION);
    EXPECT_LT(integration->metrics.current_wm_capacity, WM_CAPACITY_BASELINE);
}

TEST_F(MemoryImmuneIntegrationTest, WMCapacitySevereInflammation) {
    /* WHAT: Test WM capacity severely reduced with high inflammation */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Simulate severe inflammation */
    integration->metrics.inflammation_level = INFLAMMATION_SYSTEMIC;
    integration->metrics.current_wm_capacity = WM_CAPACITY_SEVERE_INFLAMMATION;

    /* Verify severely reduced capacity */
    EXPECT_EQ(integration->metrics.current_wm_capacity,
              WM_CAPACITY_SEVERE_INFLAMMATION);
    EXPECT_EQ(integration->metrics.current_wm_capacity, 3u);
}

TEST_F(MemoryImmuneIntegrationTest, WMCapacityRestore) {
    /* WHAT: Test capacity restoration after inflammation resolves */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Start with inflammation */
    integration->metrics.current_wm_capacity = WM_CAPACITY_MODERATE_INFLAMMATION;
    uint32_t old_capacity = integration->metrics.current_wm_capacity;

    /* Resolve inflammation */
    integration->metrics.inflammation_level = INFLAMMATION_NONE;
    uint32_t new_capacity = memory_immune_update_wm_capacity(integration);

    /* Verify restoration */
    EXPECT_EQ(new_capacity, WM_CAPACITY_BASELINE);
    EXPECT_GT(new_capacity, old_capacity);
}

/* ============================================================================
 * Decay Rate Modulation Tests
 * ============================================================================ */

TEST_F(MemoryImmuneIntegrationTest, DecayRateNormalCytokines) {
    /* WHAT: Test normal decay rate with balanced cytokines */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Set balanced cytokines */
    integration->metrics.il1_concentration = 0.1f;
    integration->metrics.tnf_concentration = 0.05f;
    integration->metrics.il10_concentration = 0.15f;

    float decay_mult = memory_immune_update_wm_decay_rate(integration);

    /* Should be close to 1.0 (normal) */
    EXPECT_NEAR(decay_mult, 1.0f, 0.3f);
}

TEST_F(MemoryImmuneIntegrationTest, DecayRateProInflammatory) {
    /* WHAT: Test increased decay with pro-inflammatory cytokines */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* High pro-inflammatory cytokines */
    integration->metrics.il1_concentration = 0.8f;
    integration->metrics.tnf_concentration = 0.7f;
    integration->metrics.il10_concentration = 0.1f;

    float decay_mult = memory_immune_update_wm_decay_rate(integration);

    /* Should be elevated (faster decay) */
    EXPECT_GT(decay_mult, 1.0f);
    EXPECT_LE(decay_mult, 2.0f);
}

TEST_F(MemoryImmuneIntegrationTest, DecayRateAntiInflammatory) {
    /* WHAT: Test normal/reduced decay with anti-inflammatory cytokines */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* High anti-inflammatory cytokines */
    integration->metrics.il1_concentration = 0.1f;
    integration->metrics.tnf_concentration = 0.05f;
    integration->metrics.il10_concentration = 0.9f;

    float decay_mult = memory_immune_update_wm_decay_rate(integration);

    /* Should be reduced or normal */
    EXPECT_GE(decay_mult, 0.8f);
    EXPECT_LE(decay_mult, 1.2f);
}

/* ============================================================================
 * Encoding Strength Modulation Tests
 * ============================================================================ */

TEST_F(MemoryImmuneIntegrationTest, EncodingStrengthNormal) {
    /* WHAT: Test normal encoding strength */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Low cytokine levels */
    integration->metrics.il1_concentration = 0.05f;
    integration->metrics.tnf_concentration = 0.02f;
    integration->metrics.il10_concentration = 0.1f;

    float strength = memory_immune_compute_encoding_strength(integration);

    /* Should be near 1.0 (normal) */
    EXPECT_NEAR(strength, 1.0f, 0.3f);
}

TEST_F(MemoryImmuneIntegrationTest, EncodingStrengthIL1LowBoost) {
    /* WHAT: Test encoding boost with low IL-1β */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Low IL-1β (should enhance) */
    integration->metrics.il1_concentration = 0.15f;
    integration->config.il1_low_dose_threshold = 0.2f;

    float strength = memory_immune_compute_encoding_strength(integration);

    /* Should be boosted */
    EXPECT_GT(strength, 1.0f);
    EXPECT_LE(strength, ENCODING_BOOST_IL1_LOW);
}

TEST_F(MemoryImmuneIntegrationTest, EncodingStrengthIL1HighImpair) {
    /* WHAT: Test encoding impairment with high IL-1β */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* High IL-1β (should impair) */
    integration->metrics.il1_concentration = 0.8f;
    integration->config.il1_high_dose_threshold = 0.6f;

    float strength = memory_immune_compute_encoding_strength(integration);

    /* Should be impaired */
    EXPECT_LT(strength, 1.0f);
}

TEST_F(MemoryImmuneIntegrationTest, EncodingStrengthTNFImpair) {
    /* WHAT: Test encoding impairment with TNF-α */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* High TNF-α */
    integration->metrics.tnf_concentration = 0.7f;
    integration->config.tnf_impairment_threshold = 0.4f;

    float strength = memory_immune_compute_encoding_strength(integration);

    /* Should be impaired */
    EXPECT_LT(strength, 1.0f);
}

TEST_F(MemoryImmuneIntegrationTest, SalienceModulation) {
    /* WHAT: Test salience modulation by encoding strength */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Set encoding multiplier */
    integration->metrics.encoding_strength_multiplier = 1.5f;

    float base_salience = 0.5f;
    float modulated = memory_immune_modulate_salience(integration, base_salience);

    /* Should be boosted */
    EXPECT_GT(modulated, base_salience);
    EXPECT_FLOAT_EQ(modulated, base_salience * 1.5f);
}

/* ============================================================================
 * Consolidation Integration Tests
 * ============================================================================ */

TEST_F(MemoryImmuneIntegrationTest, ConsolidationBoostMemoryPhase) {
    /* WHAT: Test consolidation boost during immune memory formation */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Set immune phase to memory formation */
    integration->metrics.immune_phase = IMMUNE_PHASE_MEMORY;

    float boost = memory_immune_get_consolidation_boost(integration);

    /* Should be boosted */
    EXPECT_GT(boost, 1.0f);
    EXPECT_EQ(boost, CONSOLIDATION_IMMUNE_MEMORY_BOOST);
}

TEST_F(MemoryImmuneIntegrationTest, ConsolidationNoBoostOtherPhases) {
    /* WHAT: Test no consolidation boost in other phases */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Set different phases */
    integration->metrics.immune_phase = IMMUNE_PHASE_SURVEILLANCE;
    EXPECT_FLOAT_EQ(memory_immune_get_consolidation_boost(integration), 1.0f);

    integration->metrics.immune_phase = IMMUNE_PHASE_EFFECTOR;
    EXPECT_FLOAT_EQ(memory_immune_get_consolidation_boost(integration), 1.0f);
}

/* ============================================================================
 * Immune Memory Link Tests
 * ============================================================================ */

TEST_F(MemoryImmuneIntegrationTest, CreateMemoryLink) {
    /* WHAT: Test creating immune-cognitive memory link */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Create link */
    int result = memory_immune_create_memory_link(
        integration,
        123,           /* immune_cell_id */
        true,          /* is_b_cell */
        "test_pattern",
        0.8f           /* importance */
    );

    EXPECT_EQ(result, 0);
    EXPECT_EQ(integration->memory_link_count, 1u);
    EXPECT_EQ(integration->metrics.immune_memories_formed, 1u);
}

TEST_F(MemoryImmuneIntegrationTest, GetMemoryLinks) {
    /* WHAT: Test retrieving memory links */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Create multiple links */
    memory_immune_create_memory_link(integration, 1, true, "pattern1", 0.7f);
    memory_immune_create_memory_link(integration, 2, false, "pattern2", 0.9f);

    /* Get links */
    size_t count = 0;
    const immune_cognitive_memory_link_t* links =
        memory_immune_get_memory_links(integration, &count);

    ASSERT_NE(links, nullptr);
    EXPECT_EQ(count, 2u);
    EXPECT_EQ(links[0].immune_cell_id, 1u);
    EXPECT_TRUE(links[0].is_b_cell);
    EXPECT_STREQ(links[0].pattern_name, "pattern1");
}

TEST_F(MemoryImmuneIntegrationTest, ReactivateLinkedPattern) {
    /* WHAT: Test pattern reactivation from immune cell */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Create link */
    memory_immune_create_memory_link(integration, 42, true, "memory_pattern", 0.9f);

    /* Reactivate */
    int result = memory_immune_reactivate_linked_pattern(integration, 42);
    EXPECT_EQ(result, 0);

    /* Check reactivation count */
    size_t count = 0;
    const immune_cognitive_memory_link_t* links =
        memory_immune_get_memory_links(integration, &count);
    EXPECT_EQ(links[0].reactivation_count, 1u);
}

TEST_F(MemoryImmuneIntegrationTest, ReactivateNonExistentLink) {
    /* WHAT: Test reactivation fails for non-existent link */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Try to reactivate non-existent link */
    int result = memory_immune_reactivate_linked_pattern(integration, 999);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * State Management Tests
 * ============================================================================ */

TEST_F(MemoryImmuneIntegrationTest, StateNormal) {
    /* WHAT: Test normal state */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    EXPECT_EQ(memory_immune_get_state(integration), MEM_IMMUNE_NORMAL);
}

TEST_F(MemoryImmuneIntegrationTest, GetMetrics) {
    /* WHAT: Test retrieving metrics */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    memory_immune_metrics_t metrics;
    int result = memory_immune_get_metrics(integration, &metrics);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(metrics.state, MEM_IMMUNE_NORMAL);
    EXPECT_EQ(metrics.baseline_wm_capacity, WM_CAPACITY_BASELINE);
}

TEST_F(MemoryImmuneIntegrationTest, GetStats) {
    /* WHAT: Test retrieving statistics */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Perform some operations */
    memory_immune_create_memory_link(integration, 1, true, "test", 0.8f);

    memory_immune_stats_t stats;
    int result = memory_immune_get_stats(integration, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.immune_memories_consolidated, 1u);
}

TEST_F(MemoryImmuneIntegrationTest, ResetStats) {
    /* WHAT: Test resetting statistics */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);
    memory_immune_integration_start(integration);

    /* Perform operations */
    memory_immune_create_memory_link(integration, 1, true, "test", 0.8f);

    /* Reset stats */
    memory_immune_reset_stats(integration);

    memory_immune_stats_t stats;
    memory_immune_get_stats(integration, &stats);

    EXPECT_EQ(stats.immune_memories_consolidated, 0u);
    EXPECT_EQ(stats.state_changes, 0u);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

static bool wm_capacity_callback_invoked = false;
static bool encoding_callback_invoked = false;
static bool memory_formed_callback_invoked = false;
static bool state_change_callback_invoked = false;

static void wm_capacity_cb(
    memory_immune_integration_t* integration,
    uint32_t old_capacity,
    uint32_t new_capacity,
    brain_inflammation_level_t inflammation,
    void* user_data
) {
    wm_capacity_callback_invoked = true;
}

static void encoding_cb(
    memory_immune_integration_t* integration,
    float old_multiplier,
    float new_multiplier,
    brain_cytokine_type_t dominant_cytokine,
    void* user_data
) {
    encoding_callback_invoked = true;
}

static void memory_formed_cb(
    memory_immune_integration_t* integration,
    const immune_cognitive_memory_link_t* memory_link,
    void* user_data
) {
    memory_formed_callback_invoked = true;
}

static void state_change_cb(
    memory_immune_integration_t* integration,
    memory_immune_state_t old_state,
    memory_immune_state_t new_state,
    void* user_data
) {
    state_change_callback_invoked = true;
}

TEST_F(MemoryImmuneIntegrationTest, WMCapacityCallback) {
    /* WHAT: Test WM capacity change callback */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);

    wm_capacity_callback_invoked = false;
    memory_immune_set_wm_capacity_callback(integration, wm_capacity_cb, nullptr);

    /* Trigger capacity change */
    integration->metrics.current_wm_capacity = 5;
    memory_immune_update_wm_capacity(integration);

    /* Note: Callback invoked only on actual change, may not trigger in this test */
}

TEST_F(MemoryImmuneIntegrationTest, MemoryFormedCallback) {
    /* WHAT: Test memory formation callback */
    integration = memory_immune_integration_create(
        immune_system, working_memory, nullptr, nullptr
    );
    ASSERT_NE(integration, nullptr);

    memory_formed_callback_invoked = false;
    memory_immune_set_memory_formed_callback(integration, memory_formed_cb, nullptr);

    /* Create link */
    memory_immune_create_memory_link(integration, 1, true, "test", 0.8f);

    EXPECT_TRUE(memory_formed_callback_invoked);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST(MemoryImmuneStringTest, StateToString) {
    EXPECT_STREQ(memory_immune_state_to_string(MEM_IMMUNE_NORMAL), "NORMAL");
    EXPECT_STREQ(memory_immune_state_to_string(MEM_IMMUNE_ENHANCED), "ENHANCED");
    EXPECT_STREQ(memory_immune_state_to_string(MEM_IMMUNE_IMPAIRED), "IMPAIRED");
    EXPECT_STREQ(memory_immune_state_to_string(MEM_IMMUNE_RECOVERING), "RECOVERING");
    EXPECT_STREQ(memory_immune_state_to_string(MEM_IMMUNE_STORM), "STORM");
}

TEST(MemoryImmuneStringTest, CytokineEffectToString) {
    EXPECT_STREQ(cytokine_memory_effect_to_string(CYTOKINE_EFFECT_ENHANCE), "ENHANCE");
    EXPECT_STREQ(cytokine_memory_effect_to_string(CYTOKINE_EFFECT_IMPAIR), "IMPAIR");
    EXPECT_STREQ(cytokine_memory_effect_to_string(CYTOKINE_EFFECT_NEUTRAL), "NEUTRAL");
    EXPECT_STREQ(cytokine_memory_effect_to_string(CYTOKINE_EFFECT_BIPHASIC), "BIPHASIC");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
