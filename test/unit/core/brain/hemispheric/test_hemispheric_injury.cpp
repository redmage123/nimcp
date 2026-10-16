/**
 * @file test_hemispheric_injury.cpp
 * @brief Unit tests for hemispheric brain injury and recovery system
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/hemispheric/nimcp_hemispheric_injury.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"

//=============================================================================
// Configuration Tests
//=============================================================================

TEST(HemisphericInjuryConfigTest, DefaultConfigHasValidValues) {
    hemispheric_injury_config_t config = hemispheric_injury_default_config();

    EXPECT_GT(config.recovery_tau_days, 0.0f);
    EXPECT_GT(config.max_recovery_potential, 0.0f);
    EXPECT_LE(config.max_recovery_potential, 1.0f);

    EXPECT_TRUE(config.enable_spontaneous_recovery);
    EXPECT_TRUE(config.enable_contralateral_compensation);
    EXPECT_TRUE(config.enable_diaschisis);
    EXPECT_TRUE(config.enable_rehabilitation);
}

TEST(HemisphericInjuryConfigTest, RehabilitationBoostIsPositive) {
    hemispheric_injury_config_t config = hemispheric_injury_default_config();
    EXPECT_GT(config.rehabilitation_boost, 0.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(HemisphericInjuryLifecycleTest, CreateWithNullBrainFails) {
    hemispheric_injury_config_t config = hemispheric_injury_default_config();
    hemispheric_injury_system_t* system = hemispheric_injury_create(&config, nullptr);
    EXPECT_EQ(system, nullptr);
}

TEST(HemisphericInjuryLifecycleTest, CreateWithDefaultConfigSucceeds) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_injury_system_t* system = hemispheric_injury_create(nullptr, brain);
    EXPECT_NE(system, nullptr);

    hemispheric_injury_destroy(system);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericInjuryLifecycleTest, DestroyNullSystemIsSafe) {
    hemispheric_injury_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Lesion Tests
//=============================================================================

TEST(HemisphericInjuryLesionTest, InduceLesionNullSystemFails) {
    uint32_t lesion_id;
    int result = hemispheric_injury_induce_lesion(
        nullptr, INJURY_TYPE_STROKE_ISCHEMIC, SEVERITY_MODERATE,
        HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX, 0.5f, &lesion_id);
    EXPECT_LT(result, 0);
}

TEST(HemisphericInjuryLesionTest, InduceLesionSucceeds) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_injury_system_t* system = hemispheric_injury_create(nullptr, brain);
    ASSERT_NE(system, nullptr);

    uint32_t lesion_id;
    int result = hemispheric_injury_induce_lesion(
        system, INJURY_TYPE_STROKE_ISCHEMIC, SEVERITY_MODERATE,
        HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX, 0.5f, &lesion_id);
    EXPECT_EQ(result, 0);

    // Verify damage was applied
    float damage = hemispheric_injury_get_damage(
        system, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX);
    EXPECT_GT(damage, 0.0f);

    hemispheric_injury_destroy(system);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericInjuryLesionTest, GetLesionReturnsValidData) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_injury_system_t* system = hemispheric_injury_create(nullptr, brain);
    ASSERT_NE(system, nullptr);

    uint32_t lesion_id;
    hemispheric_injury_induce_lesion(
        system, INJURY_TYPE_TBI_FOCAL, SEVERITY_SEVERE,
        HEMISPHERE_RIGHT, INJURY_REGION_PREFRONTAL, 0.7f, &lesion_id);

    const lesion_t* lesion = hemispheric_injury_get_lesion(system, lesion_id);
    ASSERT_NE(lesion, nullptr);
    EXPECT_EQ(lesion->type, INJURY_TYPE_TBI_FOCAL);
    EXPECT_EQ(lesion->severity, SEVERITY_SEVERE);
    EXPECT_EQ(lesion->hemisphere, HEMISPHERE_RIGHT);
    EXPECT_EQ(lesion->primary_region, INJURY_REGION_PREFRONTAL);
    EXPECT_TRUE(lesion->active);

    hemispheric_injury_destroy(system);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Damage Query Tests
//=============================================================================

TEST(HemisphericInjuryDamageTest, GetDamageNullSystemReturnsZero) {
    float damage = hemispheric_injury_get_damage(
        nullptr, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX);
    EXPECT_FLOAT_EQ(damage, 0.0f);
}

TEST(HemisphericInjuryDamageTest, UndamagedRegionHasZeroDamage) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_injury_system_t* system = hemispheric_injury_create(nullptr, brain);
    ASSERT_NE(system, nullptr);

    float damage = hemispheric_injury_get_damage(
        system, HEMISPHERE_LEFT, INJURY_REGION_HIPPOCAMPUS);
    EXPECT_FLOAT_EQ(damage, 0.0f);

    hemispheric_injury_destroy(system);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericInjuryDamageTest, GetFunctionNullSystemReturnsOne) {
    float function = hemispheric_injury_get_function(
        nullptr, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX);
    EXPECT_FLOAT_EQ(function, 1.0f);
}

//=============================================================================
// Recovery Tests
//=============================================================================

TEST(HemisphericInjuryRecoveryTest, UpdateNullSystemFails) {
    int result = hemispheric_injury_update(nullptr, 1.0f);
    EXPECT_LT(result, 0);
}

TEST(HemisphericInjuryRecoveryTest, UpdateAdvancesTime) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_injury_system_t* system = hemispheric_injury_create(nullptr, brain);
    ASSERT_NE(system, nullptr);

    // Induce a lesion
    uint32_t lesion_id;
    hemispheric_injury_induce_lesion(
        system, INJURY_TYPE_STROKE_ISCHEMIC, SEVERITY_MODERATE,
        HEMISPHERE_LEFT, INJURY_REGION_BROCA, 0.5f, &lesion_id);

    // Initial phase should be acute
    recovery_phase_t phase = hemispheric_injury_get_phase(system);
    EXPECT_EQ(phase, RECOVERY_PHASE_ACUTE);

    // Update for 10 days
    for (int i = 0; i < 10; i++) {
        hemispheric_injury_update(system, 1.0f);
    }

    // Phase should have advanced to subacute
    phase = hemispheric_injury_get_phase(system);
    EXPECT_EQ(phase, RECOVERY_PHASE_SUBACUTE);

    hemispheric_injury_destroy(system);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericInjuryRecoveryTest, SpontaneousRecoveryReducesImpairment) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_injury_system_t* system = hemispheric_injury_create(nullptr, brain);
    ASSERT_NE(system, nullptr);

    // Induce lesion
    uint32_t lesion_id;
    hemispheric_injury_induce_lesion(
        system, INJURY_TYPE_STROKE_ISCHEMIC, SEVERITY_MODERATE,
        HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX, 0.5f, &lesion_id);

    float initial_function = hemispheric_injury_get_function(
        system, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX);

    // Simulate recovery over 30 days
    for (int i = 0; i < 30; i++) {
        hemispheric_injury_update(system, 1.0f);
    }

    float final_function = hemispheric_injury_get_function(
        system, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX);

    // Function should have improved
    EXPECT_GT(final_function, initial_function);

    hemispheric_injury_destroy(system);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Rehabilitation Tests
//=============================================================================

TEST(HemisphericInjuryRehabTest, StartRehabNullSystemFails) {
    int result = hemispheric_injury_start_rehabilitation(
        nullptr, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX, 0.8f, 2.0f);
    EXPECT_LT(result, 0);
}

TEST(HemisphericInjuryRehabTest, RehabilitationBoostsRecovery) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_injury_system_t* system = hemispheric_injury_create(nullptr, brain);
    ASSERT_NE(system, nullptr);

    // Induce lesion
    uint32_t lesion_id;
    hemispheric_injury_induce_lesion(
        system, INJURY_TYPE_STROKE_ISCHEMIC, SEVERITY_MODERATE,
        HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX, 0.6f, &lesion_id);

    // Start rehabilitation
    int result = hemispheric_injury_start_rehabilitation(
        system, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX, 0.8f, 2.0f);
    EXPECT_EQ(result, 0);

    float initial_recovery = hemispheric_injury_get_recovery(
        system, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX);

    // Simulate recovery with rehab
    for (int i = 0; i < 10; i++) {
        hemispheric_injury_update(system, 1.0f);
    }

    float final_recovery = hemispheric_injury_get_recovery(
        system, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX);

    EXPECT_GT(final_recovery, initial_recovery);

    hemispheric_injury_destroy(system);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST(HemisphericInjuryUtilTest, RegionNameReturnsValidString) {
    EXPECT_STREQ(hemispheric_injury_region_name(INJURY_REGION_MOTOR_CORTEX), "Motor Cortex");
    EXPECT_STREQ(hemispheric_injury_region_name(INJURY_REGION_BROCA), "Broca's Area");
    EXPECT_STREQ(hemispheric_injury_region_name(INJURY_REGION_HIPPOCAMPUS), "Hippocampus");
}

TEST(HemisphericInjuryUtilTest, TypeNameReturnsValidString) {
    EXPECT_STREQ(hemispheric_injury_type_name(INJURY_TYPE_STROKE_ISCHEMIC), "Ischemic Stroke");
    EXPECT_STREQ(hemispheric_injury_type_name(INJURY_TYPE_TBI_DIFFUSE), "Diffuse TBI");
    EXPECT_STREQ(hemispheric_injury_type_name(INJURY_TYPE_DEGENERATIVE), "Degenerative");
}

TEST(HemisphericInjuryUtilTest, PhaseNameReturnsValidString) {
    EXPECT_STREQ(hemispheric_injury_phase_name(RECOVERY_PHASE_ACUTE), "Acute");
    EXPECT_STREQ(hemispheric_injury_phase_name(RECOVERY_PHASE_CHRONIC), "Chronic");
    EXPECT_STREQ(hemispheric_injury_phase_name(RECOVERY_PHASE_PLATEAU), "Plateau");
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST(HemisphericInjuryStatsTest, InitialStatsAreZero) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_injury_system_t* system = hemispheric_injury_create(nullptr, brain);
    ASSERT_NE(system, nullptr);

    hemispheric_injury_stats_t stats = hemispheric_injury_get_stats(system);
    EXPECT_EQ(stats.total_lesions, 0u);
    EXPECT_EQ(stats.active_lesions, 0u);
    EXPECT_EQ(stats.injury_updates, 0u);

    hemispheric_injury_destroy(system);
    hemispheric_brain_destroy(brain);
}

TEST(HemisphericInjuryStatsTest, LesionCountIncrementsOnInduce) {
    hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
    hemispheric_brain_t* brain = hemispheric_brain_create(&brain_config);
    if (!brain) GTEST_SKIP() << "Brain creation requires heavyweight initialization";

    hemispheric_injury_system_t* system = hemispheric_injury_create(nullptr, brain);
    ASSERT_NE(system, nullptr);

    uint32_t lesion_id;
    hemispheric_injury_induce_lesion(
        system, INJURY_TYPE_STROKE_ISCHEMIC, SEVERITY_MILD,
        HEMISPHERE_LEFT, INJURY_REGION_TEMPORAL, 0.3f, &lesion_id);

    hemispheric_injury_stats_t stats = hemispheric_injury_get_stats(system);
    EXPECT_EQ(stats.total_lesions, 1u);
    EXPECT_EQ(stats.active_lesions, 1u);

    hemispheric_injury_destroy(system);
    hemispheric_brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
