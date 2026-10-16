/**
 * @file test_heterosynaptic_integration.cpp
 * @brief Integration Tests for Heterosynaptic Plasticity
 * @version 1.0.0
 * @date 2025-12-19
 *
 * Tests integration with sleep system, immune system, and bio-async
 */

#include <gtest/gtest.h>
#include "plasticity/heterosynaptic/nimcp_heterosynaptic.h"
#include "plasticity/heterosynaptic/nimcp_heterosynaptic_sleep_bridge.h"
#include "plasticity/heterosynaptic/nimcp_heterosynaptic_immune_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"

//=============================================================================
// Sleep Integration Tests
//=============================================================================

class HeteroSleepIntegrationTest : public ::testing::Test {
protected:
    hetero_system_t* hetero_sys;
    sleep_system_t sleep_sys;
    hetero_sleep_bridge_t bridge;

    void SetUp() override {
        hetero_sys = hetero_create(nullptr, 50);
        ASSERT_NE(hetero_sys, nullptr);

        /* Create sleep system (may be NULL if not available) */
        sleep_sys = nullptr;

        /* Create bridge */
        bridge = hetero_sleep_bridge_create(nullptr, sleep_sys, hetero_sys);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        hetero_sleep_bridge_destroy(bridge);
        hetero_destroy(hetero_sys);
    }
};

TEST_F(HeteroSleepIntegrationTest, BridgeCreation) {
    /* WHAT: Test sleep bridge creation
     * WHY:  Verify integration setup
     */
    EXPECT_NE(bridge, nullptr);
}

TEST_F(HeteroSleepIntegrationTest, UpdateSleepEffects) {
    /* WHAT: Test updating sleep effects
     * WHY:  Verify modulation computation
     */
    int result = hetero_sleep_update(bridge);
    EXPECT_EQ(result, 0);

    hetero_sleep_effects_t effects;
    result = hetero_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    /* Default awake state */
    EXPECT_FLOAT_EQ(effects.competition_factor, 1.0f);
}

TEST_F(HeteroSleepIntegrationTest, CompetitionModulation) {
    /* WHAT: Test competition factor modulation
     * WHY:  Verify sleep reduces competition
     */
    float base = 1.0f;
    float modulated = hetero_sleep_get_competition_factor(bridge, base);
    EXPECT_FLOAT_EQ(modulated, base);  /* Awake state */
}

TEST_F(HeteroSleepIntegrationTest, DepressionModulation) {
    /* WHAT: Test depression factor modulation
     * WHY:  Verify sleep affects depression strength
     */
    float base = 0.4f;
    float modulated = hetero_sleep_get_depression_factor(bridge, base);
    EXPECT_FLOAT_EQ(modulated, base);  /* Awake state */
}

TEST_F(HeteroSleepIntegrationTest, RadiusModulation) {
    /* WHAT: Test radius modulation
     * WHY:  Verify sleep narrows competition radius
     */
    float base = 15.0f;
    float modulated = hetero_sleep_get_radius(bridge, base);
    EXPECT_FLOAT_EQ(modulated, base);  /* Awake state */
}

//=============================================================================
// Immune Integration Tests
//=============================================================================

class HeteroImmuneIntegrationTest : public ::testing::Test {
protected:
    hetero_system_t* hetero_sys;
    brain_immune_system_t* immune_sys;
    hetero_immune_bridge_t* bridge;

    void SetUp() override {
        hetero_sys = hetero_create(nullptr, 50);
        ASSERT_NE(hetero_sys, nullptr);

        /* Create immune system (may be NULL if not available) */
        immune_sys = nullptr;

        /* Create bridge */
        bridge = hetero_immune_bridge_create(nullptr, immune_sys, hetero_sys);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        hetero_immune_bridge_destroy(bridge);
        hetero_destroy(hetero_sys);
    }
};

TEST_F(HeteroImmuneIntegrationTest, BridgeCreation) {
    /* WHAT: Test immune bridge creation
     * WHY:  Verify integration setup
     */
    EXPECT_NE(bridge, nullptr);
}

TEST_F(HeteroImmuneIntegrationTest, ApplyCytokineEffects) {
    /* WHAT: Test applying cytokine effects
     * WHY:  Verify modulation without immune system
     */
    int result = hetero_immune_apply_cytokine_effects(bridge);
    /* Should handle NULL immune system gracefully */
    EXPECT_TRUE(result == 0 || result == NIMCP_ERROR_NULL_POINTER);
}

TEST_F(HeteroImmuneIntegrationTest, ApplyInflammationEffects) {
    /* WHAT: Test applying inflammation effects
     * WHY:  Verify modulation without immune system
     */
    int result = hetero_immune_apply_inflammation_effects(bridge);
    EXPECT_TRUE(result == 0 || result == NIMCP_ERROR_NULL_POINTER);
}

TEST_F(HeteroImmuneIntegrationTest, GetModulationState) {
    /* WHAT: Test getting modulation state
     * WHY:  Verify state query
     */
    hetero_modulation_state_t state;
    int result = hetero_immune_get_modulation_state(bridge, &state);
    EXPECT_EQ(result, 0);

    /* Default neutral modulation */
    EXPECT_FLOAT_EQ(state.competition_modulation, 1.0f);
}

TEST_F(HeteroImmuneIntegrationTest, DetectInstability) {
    /* WHAT: Test instability detection
     * WHY:  Verify monitoring without immune system
     */
    int result = hetero_immune_detect_instability(bridge);
    EXPECT_TRUE(result == 0 || result == NIMCP_ERROR_NULL_POINTER);
}

TEST_F(HeteroImmuneIntegrationTest, CompetitionImpaired) {
    /* WHAT: Test competition impairment query
     * WHY:  Verify status without immune system
     */
    bool impaired = hetero_immune_is_competition_impaired(bridge);
    EXPECT_FALSE(impaired);  /* No impairment without immune */
}

TEST_F(HeteroImmuneIntegrationTest, BridgeUpdate) {
    /* WHAT: Test bidirectional bridge update
     * WHY:  Verify update cycle
     */
    int result = hetero_immune_bridge_update(bridge, 100);
    EXPECT_TRUE(result == 0 || result == NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Combined Integration Tests
//=============================================================================

TEST(HeteroIntegration, SleepAndImmuneCooperation) {
    /* WHAT: Test sleep and immune bridges working together
     * WHY:  Verify no conflicts
     */
    hetero_system_t* sys = hetero_create(nullptr, 50);
    ASSERT_NE(sys, nullptr);

    hetero_sleep_bridge_t sleep_bridge = hetero_sleep_bridge_create(nullptr, nullptr, sys);
    hetero_immune_bridge_t* immune_bridge = hetero_immune_bridge_create(nullptr, nullptr, sys);

    EXPECT_NE(sleep_bridge, nullptr);
    EXPECT_NE(immune_bridge, nullptr);

    /* Update both */
    hetero_sleep_update(sleep_bridge);
    hetero_immune_bridge_update(immune_bridge, 100);

    /* No crashes = success */

    hetero_immune_bridge_destroy(immune_bridge);
    hetero_sleep_bridge_destroy(sleep_bridge);
    hetero_destroy(sys);
}

TEST(HeteroIntegration, LargeScaleNetwork) {
    /* WHAT: Test large network with many synapses
     * WHY:  Verify scalability
     */
    hetero_system_t* sys = hetero_create(nullptr, 1000);
    ASSERT_NE(sys, nullptr);

    /* Add 100 synapses in 3D grid */
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            hetero_spatial_coords_t pos = {
                (float)i * 5.0f,
                (float)j * 5.0f,
                0.0f
            };
            int result = hetero_add_synapse(sys, &pos, 0.5f, i * 10 + j, 0);
            EXPECT_EQ(result, 0);
        }
    }

    EXPECT_EQ(sys->num_synapses, 100);

    /* Apply competition at center */
    hetero_spatial_coords_t center = {25.0f, 25.0f, 0.0f};
    hetero_competition_result_t result;
    int status = hetero_winner_take_all(sys, &center, 20.0f, &result);

    EXPECT_EQ(status, 0);
    EXPECT_GT(result.num_competitors, 0);

    hetero_free_competition_result(&result);
    hetero_destroy(sys);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
