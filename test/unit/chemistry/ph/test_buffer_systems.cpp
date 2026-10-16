/**
 * @file test_buffer_systems.cpp
 * @brief Unit tests for Buffer Systems module (nimcp_buffer_systems.h)
 *
 * WHAT: Test suite for pH buffering and homeostasis
 * WHY:  Verify bicarbonate, phosphate, protein buffer systems and
 *       Henderson-Hasselbalch calculations
 * HOW:  GTest-based tests for lifecycle, individual buffers, manager,
 *       and utility functions
 *
 * @author NIMCP Development Team
 * @date 2026-02-19
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "chemistry/ph/nimcp_buffer_systems.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BufferSystemsTest : public ::testing::Test {
protected:
    nimcp_buffer_manager_t manager;

    void SetUp() override {
        memset(&manager, 0, sizeof(manager));
        nimcp_buffer_error_t err = nimcp_buffer_init(&manager, nullptr);
        ASSERT_EQ(err, BUFFER_OK);
    }

    void TearDown() override {
        nimcp_buffer_shutdown(&manager);
    }

    static constexpr float FLOAT_TOLERANCE = 0.01f;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(BufferLifecycleTest, InitWithNullConfig) {
    nimcp_buffer_manager_t mgr;
    memset(&mgr, 0, sizeof(mgr));
    nimcp_buffer_error_t err = nimcp_buffer_init(&mgr, nullptr);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_TRUE(mgr.initialized);
    nimcp_buffer_shutdown(&mgr);
}

TEST(BufferLifecycleTest, InitWithCustomConfig) {
    nimcp_buffer_manager_t mgr;
    memset(&mgr, 0, sizeof(mgr));

    nimcp_buffer_config_t config = {
        .initial_hco3 = 26.0f,
        .initial_pco2 = 38.0f,
        .ca_activity = 0.9f,
        .initial_phosphate = 1.2f,
        .total_protein = 75.0f,
        .albumin_fraction = 0.6f,
        .regeneration_rate = 0.05f,
        .target_ph = 7.4f
    };

    nimcp_buffer_error_t err = nimcp_buffer_init(&mgr, &config);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_TRUE(mgr.initialized);
    nimcp_buffer_shutdown(&mgr);
}

TEST(BufferLifecycleTest, InitNull) {
    nimcp_buffer_error_t err = nimcp_buffer_init(nullptr, nullptr);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST(BufferLifecycleTest, ShutdownSuccess) {
    nimcp_buffer_manager_t mgr;
    memset(&mgr, 0, sizeof(mgr));
    nimcp_buffer_init(&mgr, nullptr);
    nimcp_buffer_error_t err = nimcp_buffer_shutdown(&mgr);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_FALSE(mgr.initialized);
}

TEST(BufferLifecycleTest, ShutdownNull) {
    nimcp_buffer_error_t err = nimcp_buffer_shutdown(nullptr);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST(BufferLifecycleTest, ResetSuccess) {
    nimcp_buffer_manager_t mgr;
    memset(&mgr, 0, sizeof(mgr));
    nimcp_buffer_init(&mgr, nullptr);

    /* Modify state to verify reset restores it */
    mgr.proton_load = 5.0f;
    mgr.update_count = 100;

    nimcp_buffer_error_t err = nimcp_buffer_reset(&mgr);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_EQ(mgr.update_count, 0u);

    nimcp_buffer_shutdown(&mgr);
}

TEST(BufferLifecycleTest, ResetNull) {
    nimcp_buffer_error_t err = nimcp_buffer_reset(nullptr);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

//=============================================================================
// Bicarbonate Buffer Tests
//=============================================================================

TEST_F(BufferSystemsTest, BicarbonateSetConcentration) {
    nimcp_buffer_error_t err = nimcp_bicarbonate_set_concentration(
        &manager.bicarbonate, 26.0f);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_NEAR(manager.bicarbonate.hco3_concentration, 26.0f, FLOAT_TOLERANCE);
}

TEST_F(BufferSystemsTest, BicarbonateSetConcentrationNull) {
    nimcp_buffer_error_t err = nimcp_bicarbonate_set_concentration(nullptr, 24.0f);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, BicarbonateSetConcentrationNegative) {
    nimcp_buffer_error_t err = nimcp_bicarbonate_set_concentration(
        &manager.bicarbonate, -1.0f);
    EXPECT_EQ(err, BUFFER_ERR_INVALID_PARAM);
}

TEST_F(BufferSystemsTest, BicarbonateSetPCO2) {
    nimcp_buffer_error_t err = nimcp_bicarbonate_set_pco2(
        &manager.bicarbonate, 45.0f);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_NEAR(manager.bicarbonate.pco2, 45.0f, FLOAT_TOLERANCE);
}

TEST_F(BufferSystemsTest, BicarbonateSetPCO2Null) {
    nimcp_buffer_error_t err = nimcp_bicarbonate_set_pco2(nullptr, 40.0f);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, BicarbonateSetPCO2Negative) {
    nimcp_buffer_error_t err = nimcp_bicarbonate_set_pco2(
        &manager.bicarbonate, -5.0f);
    EXPECT_EQ(err, BUFFER_ERR_INVALID_PARAM);
}

TEST_F(BufferSystemsTest, BicarbonateCalculatePH) {
    /* Set known values: pH = 6.1 + log10(24 / (0.03 * 40)) ~ 7.4 */
    nimcp_bicarbonate_set_concentration(&manager.bicarbonate, BUFFER_HCO3_NORMAL);
    nimcp_bicarbonate_set_pco2(&manager.bicarbonate, BUFFER_PCO2_NORMAL);

    float ph;
    nimcp_buffer_error_t err = nimcp_bicarbonate_calculate_ph(
        &manager.bicarbonate, &ph);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_NEAR(ph, 7.4f, 0.1f);
}

TEST_F(BufferSystemsTest, BicarbonateCalculatePHNull) {
    float ph;
    nimcp_buffer_error_t err = nimcp_bicarbonate_calculate_ph(nullptr, &ph);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);

    err = nimcp_bicarbonate_calculate_ph(&manager.bicarbonate, nullptr);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, BicarbonateApplyAcid) {
    nimcp_bicarbonate_set_concentration(&manager.bicarbonate, BUFFER_HCO3_NORMAL);
    nimcp_bicarbonate_set_pco2(&manager.bicarbonate, BUFFER_PCO2_NORMAL);

    float delta_ph;
    nimcp_buffer_error_t err = nimcp_bicarbonate_apply_acid(
        &manager.bicarbonate, 1.0f, &delta_ph);
    EXPECT_EQ(err, BUFFER_OK);
    /* Acid load should decrease pH (negative delta) */
    EXPECT_LT(delta_ph, 0.0f);
}

TEST_F(BufferSystemsTest, BicarbonateApplyAcidNull) {
    float delta_ph;
    nimcp_buffer_error_t err = nimcp_bicarbonate_apply_acid(nullptr, 1.0f, &delta_ph);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);

    err = nimcp_bicarbonate_apply_acid(&manager.bicarbonate, 1.0f, nullptr);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, BicarbonateApplyAcidReducesHCO3) {
    nimcp_bicarbonate_set_concentration(&manager.bicarbonate, BUFFER_HCO3_NORMAL);
    float initial_hco3 = manager.bicarbonate.hco3_concentration;

    float delta_ph;
    nimcp_bicarbonate_apply_acid(&manager.bicarbonate, 2.0f, &delta_ph);

    /* HCO3- should be consumed by acid load */
    EXPECT_LT(manager.bicarbonate.hco3_concentration, initial_hco3);
}

//=============================================================================
// Phosphate Buffer Tests
//=============================================================================

TEST_F(BufferSystemsTest, PhosphateSetConcentration) {
    nimcp_buffer_error_t err = nimcp_phosphate_set_concentration(
        &manager.phosphate, 2.0f);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_NEAR(manager.phosphate.total_phosphate, 2.0f, FLOAT_TOLERANCE);
}

TEST_F(BufferSystemsTest, PhosphateSetConcentrationNull) {
    nimcp_buffer_error_t err = nimcp_phosphate_set_concentration(nullptr, 1.0f);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, PhosphateSetConcentrationNegative) {
    nimcp_buffer_error_t err = nimcp_phosphate_set_concentration(
        &manager.phosphate, -1.0f);
    EXPECT_EQ(err, BUFFER_ERR_INVALID_PARAM);
}

TEST_F(BufferSystemsTest, PhosphateApplyAcid) {
    nimcp_phosphate_set_concentration(&manager.phosphate, 2.0f);

    float delta_ph;
    nimcp_buffer_error_t err = nimcp_phosphate_apply_acid(
        &manager.phosphate, 7.2f, 0.5f, &delta_ph);
    EXPECT_EQ(err, BUFFER_OK);
    /* Acid should lower pH */
    EXPECT_LT(delta_ph, 0.0f);
}

TEST_F(BufferSystemsTest, PhosphateApplyAcidNull) {
    float delta_ph;
    nimcp_buffer_error_t err = nimcp_phosphate_apply_acid(
        nullptr, 7.2f, 0.5f, &delta_ph);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);

    err = nimcp_phosphate_apply_acid(
        &manager.phosphate, 7.2f, 0.5f, nullptr);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, PhosphateBufferingNearPKA) {
    /* Phosphate buffer is most effective near its pKa of 6.8 */
    nimcp_phosphate_set_concentration(&manager.phosphate, 2.0f);

    float delta_near_pka, delta_far_from_pka;

    nimcp_phosphate_apply_acid(&manager.phosphate, 6.8f, 0.1f, &delta_near_pka);

    /* Reset and test at pH far from pKa */
    nimcp_phosphate_set_concentration(&manager.phosphate, 2.0f);
    nimcp_phosphate_apply_acid(&manager.phosphate, 8.5f, 0.1f, &delta_far_from_pka);

    /* The pH change should be smaller near the pKa (better buffering) */
    EXPECT_LT(fabsf(delta_near_pka), fabsf(delta_far_from_pka));
}

//=============================================================================
// Protein Buffer Tests
//=============================================================================

TEST_F(BufferSystemsTest, ProteinSetConcentration) {
    nimcp_buffer_error_t err = nimcp_protein_set_concentration(
        &manager.protein, 70.0f);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_NEAR(manager.protein.total_protein, 70.0f, FLOAT_TOLERANCE);
}

TEST_F(BufferSystemsTest, ProteinSetConcentrationNull) {
    nimcp_buffer_error_t err = nimcp_protein_set_concentration(nullptr, 70.0f);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, ProteinSetConcentrationNegative) {
    nimcp_buffer_error_t err = nimcp_protein_set_concentration(
        &manager.protein, -1.0f);
    EXPECT_EQ(err, BUFFER_ERR_INVALID_PARAM);
}

TEST_F(BufferSystemsTest, ProteinApplyAcid) {
    nimcp_protein_set_concentration(&manager.protein, 70.0f);

    float delta_ph;
    nimcp_buffer_error_t err = nimcp_protein_apply_acid(
        &manager.protein, 7.2f, 0.5f, &delta_ph);
    EXPECT_EQ(err, BUFFER_OK);
    /* Acid should lower pH */
    EXPECT_LT(delta_ph, 0.0f);
}

TEST_F(BufferSystemsTest, ProteinApplyAcidNull) {
    float delta_ph;
    nimcp_buffer_error_t err = nimcp_protein_apply_acid(
        nullptr, 7.2f, 0.5f, &delta_ph);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);

    err = nimcp_protein_apply_acid(
        &manager.protein, 7.2f, 0.5f, nullptr);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, ProteinHigherConcentrationBetterBuffering) {
    /* More protein should provide more buffering */
    nimcp_protein_set_concentration(&manager.protein, 30.0f);
    float delta_low;
    nimcp_protein_apply_acid(&manager.protein, 7.2f, 0.5f, &delta_low);

    nimcp_protein_set_concentration(&manager.protein, 100.0f);
    float delta_high;
    nimcp_protein_apply_acid(&manager.protein, 7.2f, 0.5f, &delta_high);

    /* Higher protein concentration should have smaller pH change */
    EXPECT_LT(fabsf(delta_high), fabsf(delta_low));
}

//=============================================================================
// Buffer Manager Tests
//=============================================================================

TEST_F(BufferSystemsTest, AddComponentSuccess) {
    nimcp_buffer_error_t err = nimcp_buffer_add_component(
        &manager, BUFFER_TYPE_CUSTOM, "TestBuffer", 7.0f, 5.0f);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_EQ(manager.num_components, 1u);
}

TEST_F(BufferSystemsTest, AddComponentNull) {
    nimcp_buffer_error_t err = nimcp_buffer_add_component(
        nullptr, BUFFER_TYPE_CUSTOM, "TestBuffer", 7.0f, 5.0f);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, AddMultipleComponents) {
    nimcp_buffer_add_component(
        &manager, BUFFER_TYPE_CUSTOM, "Buffer1", 6.5f, 5.0f);
    nimcp_buffer_add_component(
        &manager, BUFFER_TYPE_CUSTOM, "Buffer2", 7.0f, 3.0f);
    nimcp_buffer_add_component(
        &manager, BUFFER_TYPE_AMMONIA, "NH3Buffer", 9.25f, 1.0f);

    EXPECT_EQ(manager.num_components, 3u);
}

TEST_F(BufferSystemsTest, AddComponentCapacityLimit) {
    /* Fill up to max */
    for (uint32_t i = 0; i < BUFFER_MAX_SYSTEMS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Buffer%u", i);
        nimcp_buffer_error_t err = nimcp_buffer_add_component(
            &manager, BUFFER_TYPE_CUSTOM, name, 7.0f, 1.0f);
        EXPECT_EQ(err, BUFFER_OK);
    }

    /* One more should fail */
    nimcp_buffer_error_t err = nimcp_buffer_add_component(
        &manager, BUFFER_TYPE_CUSTOM, "Overflow", 7.0f, 1.0f);
    EXPECT_EQ(err, BUFFER_ERR_CAPACITY_EXCEEDED);
}

TEST_F(BufferSystemsTest, ApplyAcidLoad) {
    float new_ph;
    nimcp_buffer_error_t err = nimcp_buffer_apply_acid_load(
        &manager, 0.5f, 7.4f, &new_ph);
    EXPECT_EQ(err, BUFFER_OK);
    /* Acid load should lower pH */
    EXPECT_LT(new_ph, 7.4f);
}

TEST_F(BufferSystemsTest, ApplyAcidLoadNull) {
    float new_ph;
    nimcp_buffer_error_t err = nimcp_buffer_apply_acid_load(
        nullptr, 0.5f, 7.4f, &new_ph);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);

    err = nimcp_buffer_apply_acid_load(&manager, 0.5f, 7.4f, nullptr);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, ApplyBaseLoad) {
    float new_ph;
    nimcp_buffer_error_t err = nimcp_buffer_apply_base_load(
        &manager, 0.5f, 7.4f, &new_ph);
    EXPECT_EQ(err, BUFFER_OK);
    /* Base load should raise pH */
    EXPECT_GT(new_ph, 7.4f);
}

TEST_F(BufferSystemsTest, ApplyBaseLoadNull) {
    float new_ph;
    nimcp_buffer_error_t err = nimcp_buffer_apply_base_load(
        nullptr, 0.5f, 7.4f, &new_ph);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);

    err = nimcp_buffer_apply_base_load(&manager, 0.5f, 7.4f, nullptr);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, AcidAndBaseOpposite) {
    /* Acid and base loads should push pH in opposite directions */
    float acid_ph, base_ph;
    float start_ph = 7.4f;

    nimcp_buffer_apply_acid_load(&manager, 0.5f, start_ph, &acid_ph);
    nimcp_buffer_apply_base_load(&manager, 0.5f, start_ph, &base_ph);

    EXPECT_LT(acid_ph, start_ph);
    EXPECT_GT(base_ph, start_ph);
}

TEST_F(BufferSystemsTest, GetTotalCapacity) {
    float capacity;
    nimcp_buffer_error_t err = nimcp_buffer_get_total_capacity(&manager, &capacity);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_GE(capacity, 0.0f);
}

TEST_F(BufferSystemsTest, GetTotalCapacityNull) {
    float capacity;
    nimcp_buffer_error_t err = nimcp_buffer_get_total_capacity(nullptr, &capacity);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);

    err = nimcp_buffer_get_total_capacity(&manager, nullptr);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

TEST_F(BufferSystemsTest, UpdateSystem) {
    nimcp_buffer_error_t err = nimcp_buffer_update(&manager, 1.0f);
    EXPECT_EQ(err, BUFFER_OK);
    EXPECT_EQ(manager.update_count, 1u);
}

TEST_F(BufferSystemsTest, UpdateMultiple) {
    for (int i = 0; i < 50; i++) {
        nimcp_buffer_error_t err = nimcp_buffer_update(&manager, 0.5f);
        EXPECT_EQ(err, BUFFER_OK);
    }
    EXPECT_EQ(manager.update_count, 50u);
}

TEST_F(BufferSystemsTest, UpdateNull) {
    nimcp_buffer_error_t err = nimcp_buffer_update(nullptr, 1.0f);
    EXPECT_EQ(err, BUFFER_ERR_NULL_PTR);
}

//=============================================================================
// Henderson-Hasselbalch Tests
//=============================================================================

TEST_F(BufferSystemsTest, HendersonHasselbalchEquimolar) {
    /* When [A-] == [HA], pH = pKa */
    float ph = nimcp_buffer_henderson_hasselbalch(7.0f, 10.0f, 10.0f);
    EXPECT_NEAR(ph, 7.0f, FLOAT_TOLERANCE);
}

TEST_F(BufferSystemsTest, HendersonHasselbalchBaseExcess) {
    /* When [A-] > [HA], pH > pKa */
    float ph = nimcp_buffer_henderson_hasselbalch(7.0f, 100.0f, 10.0f);
    EXPECT_NEAR(ph, 8.0f, FLOAT_TOLERANCE);
}

TEST_F(BufferSystemsTest, HendersonHasselbalchAcidExcess) {
    /* When [HA] > [A-], pH < pKa */
    float ph = nimcp_buffer_henderson_hasselbalch(7.0f, 10.0f, 100.0f);
    EXPECT_NEAR(ph, 6.0f, FLOAT_TOLERANCE);
}

TEST_F(BufferSystemsTest, HendersonHasselbalchBicarbonate) {
    /* Classic bicarbonate: pH = 6.1 + log10(24 / (0.03 * 40)) ~ 7.4 */
    float dissolved_co2 = BUFFER_CO2_SOLUBILITY * BUFFER_PCO2_NORMAL;
    float ph = nimcp_buffer_henderson_hasselbalch(
        BUFFER_BICARBONATE_PKA, BUFFER_HCO3_NORMAL, dissolved_co2);
    EXPECT_NEAR(ph, 7.4f, 0.1f);
}

//=============================================================================
// Buffer Ratio Tests
//=============================================================================

TEST_F(BufferSystemsTest, RatioFromPHAtPKA) {
    /* At pKa, ratio [A-]/[HA] = 1 */
    float ratio = nimcp_buffer_ratio_from_ph(7.0f, 7.0f);
    EXPECT_NEAR(ratio, 1.0f, FLOAT_TOLERANCE);
}

TEST_F(BufferSystemsTest, RatioFromPHAbovePKA) {
    /* 1 pH unit above pKa, ratio = 10 */
    float ratio = nimcp_buffer_ratio_from_ph(8.0f, 7.0f);
    EXPECT_NEAR(ratio, 10.0f, 0.1f);
}

TEST_F(BufferSystemsTest, RatioFromPHBelowPKA) {
    /* 1 pH unit below pKa, ratio = 0.1 */
    float ratio = nimcp_buffer_ratio_from_ph(6.0f, 7.0f);
    EXPECT_NEAR(ratio, 0.1f, 0.01f);
}

//=============================================================================
// Buffer Capacity Tests
//=============================================================================

TEST_F(BufferSystemsTest, CapacityAtPKA) {
    /* Buffer capacity is maximum at pH = pKa */
    float cap_at_pka = nimcp_buffer_capacity_at_ph(10.0f, 7.0f, 7.0f);
    float cap_away = nimcp_buffer_capacity_at_ph(10.0f, 7.0f, 9.0f);
    EXPECT_GT(cap_at_pka, cap_away);
}

TEST_F(BufferSystemsTest, CapacityIncreasesWithConcentration) {
    float cap_low = nimcp_buffer_capacity_at_ph(5.0f, 7.0f, 7.0f);
    float cap_high = nimcp_buffer_capacity_at_ph(20.0f, 7.0f, 7.0f);
    EXPECT_GT(cap_high, cap_low);
}

TEST_F(BufferSystemsTest, CapacitySymmetricAroundPKA) {
    /* Capacity should be approximately symmetric around pKa */
    float cap_above = nimcp_buffer_capacity_at_ph(10.0f, 7.0f, 7.5f);
    float cap_below = nimcp_buffer_capacity_at_ph(10.0f, 7.0f, 6.5f);
    EXPECT_NEAR(cap_above, cap_below, 0.5f);
}

//=============================================================================
// pH / H+ Conversion Tests
//=============================================================================

TEST_F(BufferSystemsTest, PHToHNeutral) {
    /* pH 7 = 10^-7 M H+ */
    float h = nimcp_buffer_ph_to_h(7.0f);
    EXPECT_NEAR(h, 1e-7f, 1e-8f);
}

TEST_F(BufferSystemsTest, PHToHAcidic) {
    /* pH 1 = 0.1 M H+ */
    float h = nimcp_buffer_ph_to_h(1.0f);
    EXPECT_NEAR(h, 0.1f, 0.01f);
}

TEST_F(BufferSystemsTest, HToPHNeutral) {
    /* 10^-7 M H+ = pH 7 */
    float ph = nimcp_buffer_h_to_ph(1e-7f);
    EXPECT_NEAR(ph, 7.0f, 0.1f);
}

TEST_F(BufferSystemsTest, HToPHAcidic) {
    /* 0.01 M H+ = pH 2 */
    float ph = nimcp_buffer_h_to_ph(0.01f);
    EXPECT_NEAR(ph, 2.0f, 0.1f);
}

TEST_F(BufferSystemsTest, PHRoundTrip) {
    /* Convert pH -> H+ -> pH should be identity */
    float original_ph = 7.35f;
    float h = nimcp_buffer_ph_to_h(original_ph);
    float recovered_ph = nimcp_buffer_h_to_ph(h);
    EXPECT_NEAR(recovered_ph, original_ph, 0.1f);
}

//=============================================================================
// State String Tests
//=============================================================================

TEST_F(BufferSystemsTest, StateStringAllValues) {
    EXPECT_NE(nimcp_buffer_state_string(BUFFER_STATE_NORMAL), nullptr);
    EXPECT_NE(nimcp_buffer_state_string(BUFFER_STATE_DEPLETED), nullptr);
    EXPECT_NE(nimcp_buffer_state_string(BUFFER_STATE_SATURATED), nullptr);
    EXPECT_NE(nimcp_buffer_state_string(BUFFER_STATE_REGENERATING), nullptr);
}

TEST_F(BufferSystemsTest, StateStringUnknown) {
    const char* str = nimcp_buffer_state_string((nimcp_buffer_state_t)999);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Error String Tests
//=============================================================================

TEST_F(BufferSystemsTest, ErrorStringAllValues) {
    EXPECT_NE(nimcp_buffer_error_string(BUFFER_OK), nullptr);
    EXPECT_NE(nimcp_buffer_error_string(BUFFER_ERR_NULL_PTR), nullptr);
    EXPECT_NE(nimcp_buffer_error_string(BUFFER_ERR_INVALID_PARAM), nullptr);
    EXPECT_NE(nimcp_buffer_error_string(BUFFER_ERR_NOT_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_buffer_error_string(BUFFER_ERR_BUFFER_EXHAUSTED), nullptr);
    EXPECT_NE(nimcp_buffer_error_string(BUFFER_ERR_CAPACITY_EXCEEDED), nullptr);
    EXPECT_NE(nimcp_buffer_error_string(BUFFER_ERR_INVALID_PH), nullptr);
}

TEST_F(BufferSystemsTest, ErrorStringUnknown) {
    const char* str = nimcp_buffer_error_string((nimcp_buffer_error_t)-999);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Integration-Style Tests
//=============================================================================

TEST_F(BufferSystemsTest, BufferingAttenuatesAcidShift) {
    /* With buffers active, a given acid load should cause less pH change
       than it would in pure water */
    float buffered_ph;
    nimcp_buffer_apply_acid_load(&manager, 0.1f, 7.4f, &buffered_ph);

    float buffered_shift = 7.4f - buffered_ph;
    EXPECT_GT(buffered_shift, 0.0f);
    EXPECT_LT(buffered_shift, 1.0f);
}

TEST_F(BufferSystemsTest, RepeatedAcidLoadsDecreasePH) {
    float ph = 7.4f;
    float prev_ph = ph;

    for (int i = 0; i < 5; i++) {
        nimcp_buffer_apply_acid_load(&manager, 0.5f, prev_ph, &ph);
        EXPECT_LT(ph, prev_ph);
        prev_ph = ph;
    }
}

TEST_F(BufferSystemsTest, RepeatedBaseLoadsIncreasePH) {
    float ph = 7.4f;
    float prev_ph = ph;

    for (int i = 0; i < 5; i++) {
        nimcp_buffer_apply_base_load(&manager, 0.5f, prev_ph, &ph);
        EXPECT_GT(ph, prev_ph);
        prev_ph = ph;
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
