/**
 * @file test_proton_pumps.cpp
 * @brief Unit tests for Proton Pumps module (nimcp_proton_pumps.h)
 *
 * WHAT: Test suite for proton pump systems - V-ATPase, NHE, NBC
 * WHY:  Verify active pH regulation mechanisms for vesicle acidification
 *       and intracellular pH homeostasis
 * HOW:  GTest-based tests for lifecycle, individual pump operations,
 *       system updates, and energy coupling
 *
 * @author NIMCP Development Team
 * @date 2026-02-19
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "chemistry/ph/nimcp_proton_pumps.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ProtonPumpsTest : public ::testing::Test {
protected:
    nimcp_pump_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
        nimcp_pump_error_t err = nimcp_pump_init(&system, nullptr);
        ASSERT_EQ(err, PUMP_OK);
    }

    void TearDown() override {
        nimcp_pump_shutdown(&system);
    }

    static constexpr float FLOAT_TOLERANCE = 0.01f;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(PumpLifecycleTest, InitWithNullConfig) {
    nimcp_pump_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_pump_error_t err = nimcp_pump_init(&sys, nullptr);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_TRUE(sys.initialized);
    nimcp_pump_shutdown(&sys);
}

TEST(PumpLifecycleTest, InitWithCustomConfig) {
    nimcp_pump_system_t sys;
    memset(&sys, 0, sizeof(sys));

    nimcp_pump_config_t config = {
        .vatpase_density = 10.0f,
        .vatpase_max_rate = 80.0f,
        .nhe_isoform = NHE_ISOFORM_NHE1,
        .nhe_density = 100.0f,
        .nhe_set_point = 7.2f,
        .nbc_density = 50.0f,
        .nbc_stoichiometry = 2.0f,
        .atp_concentration = 5.0f,
        .atp_regeneration_rate = 0.1f,
        .na_gradient = 140.0f,
        .cl_concentration = 110.0f
    };

    nimcp_pump_error_t err = nimcp_pump_init(&sys, &config);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_TRUE(sys.initialized);
    nimcp_pump_shutdown(&sys);
}

TEST(PumpLifecycleTest, InitNull) {
    nimcp_pump_error_t err = nimcp_pump_init(nullptr, nullptr);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

TEST(PumpLifecycleTest, ShutdownSuccess) {
    nimcp_pump_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_pump_init(&sys, nullptr);
    nimcp_pump_error_t err = nimcp_pump_shutdown(&sys);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_FALSE(sys.initialized);
}

TEST(PumpLifecycleTest, ShutdownNull) {
    nimcp_pump_error_t err = nimcp_pump_shutdown(nullptr);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

TEST(PumpLifecycleTest, ResetSuccess) {
    nimcp_pump_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_pump_init(&sys, nullptr);

    /* Modify state to verify reset restores it */
    sys.atp_consumed = 100.0f;
    sys.update_count = 50;

    nimcp_pump_error_t err = nimcp_pump_reset(&sys);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_EQ(sys.update_count, 0u);

    nimcp_pump_shutdown(&sys);
}

TEST(PumpLifecycleTest, ResetNull) {
    nimcp_pump_error_t err = nimcp_pump_reset(nullptr);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

//=============================================================================
// V-ATPase Tests
//=============================================================================

TEST_F(ProtonPumpsTest, VatpaseSetActivity) {
    nimcp_pump_error_t err = nimcp_vatpase_set_activity(
        &system.vatpase, 0.8f);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_NEAR(system.vatpase.activity_level, 0.8f, FLOAT_TOLERANCE);
}

TEST_F(ProtonPumpsTest, VatpaseSetActivityNull) {
    nimcp_pump_error_t err = nimcp_vatpase_set_activity(nullptr, 0.8f);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

TEST_F(ProtonPumpsTest, VatpaseSetActivityClampHigh) {
    nimcp_pump_error_t err = nimcp_vatpase_set_activity(
        &system.vatpase, 1.5f);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_LE(system.vatpase.activity_level, 1.0f);
}

TEST_F(ProtonPumpsTest, VatpaseSetActivityClampLow) {
    nimcp_pump_error_t err = nimcp_vatpase_set_activity(
        &system.vatpase, -0.5f);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_GE(system.vatpase.activity_level, 0.0f);
}

TEST_F(ProtonPumpsTest, VatpaseSetAssembly) {
    nimcp_pump_error_t err = nimcp_vatpase_set_assembly(
        &system.vatpase, 0.7f);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_NEAR(system.vatpase.v0_v1_assembly, 0.7f, FLOAT_TOLERANCE);
}

TEST_F(ProtonPumpsTest, VatpaseSetAssemblyNull) {
    nimcp_pump_error_t err = nimcp_vatpase_set_assembly(nullptr, 0.7f);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

TEST_F(ProtonPumpsTest, VatpaseCalculateFlux) {
    nimcp_vatpase_set_activity(&system.vatpase, 1.0f);
    nimcp_vatpase_set_assembly(&system.vatpase, 1.0f);

    float h_flux, atp_cost;
    nimcp_pump_error_t err = nimcp_vatpase_calculate_flux(
        &system.vatpase, 5.0f, 7.0f, &h_flux, &atp_cost);
    EXPECT_EQ(err, PUMP_OK);
    /* Active pump should produce positive H+ flux */
    EXPECT_GT(h_flux, 0.0f);
    /* Should consume some ATP */
    EXPECT_GT(atp_cost, 0.0f);
}

TEST_F(ProtonPumpsTest, VatpaseCalculateFluxNull) {
    float h_flux, atp_cost;
    nimcp_pump_error_t err = nimcp_vatpase_calculate_flux(
        nullptr, 5.0f, 7.0f, &h_flux, &atp_cost);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);

    err = nimcp_vatpase_calculate_flux(
        &system.vatpase, 5.0f, 7.0f, nullptr, &atp_cost);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);

    err = nimcp_vatpase_calculate_flux(
        &system.vatpase, 5.0f, 7.0f, &h_flux, nullptr);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

TEST_F(ProtonPumpsTest, VatpaseFluxIncreasesWithActivity) {
    nimcp_vatpase_set_assembly(&system.vatpase, 1.0f);

    float flux_low, flux_high, atp;

    nimcp_vatpase_set_activity(&system.vatpase, 0.2f);
    nimcp_vatpase_calculate_flux(&system.vatpase, 5.0f, 7.0f, &flux_low, &atp);

    nimcp_vatpase_set_activity(&system.vatpase, 0.9f);
    nimcp_vatpase_calculate_flux(&system.vatpase, 5.0f, 7.0f, &flux_high, &atp);

    EXPECT_GT(flux_high, flux_low);
}

TEST_F(ProtonPumpsTest, VatpaseFluxDependsOnAssembly) {
    nimcp_vatpase_set_activity(&system.vatpase, 1.0f);

    float flux_disassembled, flux_assembled, atp;

    nimcp_vatpase_set_assembly(&system.vatpase, 0.1f);
    nimcp_vatpase_calculate_flux(&system.vatpase, 5.0f, 7.0f, &flux_disassembled, &atp);

    nimcp_vatpase_set_assembly(&system.vatpase, 1.0f);
    nimcp_vatpase_calculate_flux(&system.vatpase, 5.0f, 7.0f, &flux_assembled, &atp);

    EXPECT_GT(flux_assembled, flux_disassembled);
}

TEST_F(ProtonPumpsTest, VatpaseFluxReducedByAcidicLumen) {
    /* V-ATPase should slow down when vesicle is already very acidic */
    nimcp_vatpase_set_activity(&system.vatpase, 1.0f);
    nimcp_vatpase_set_assembly(&system.vatpase, 1.0f);

    float flux_neutral, flux_acidic, atp;

    nimcp_vatpase_calculate_flux(&system.vatpase, 5.0f, 7.0f, &flux_neutral, &atp);
    nimcp_vatpase_calculate_flux(&system.vatpase, 5.0f, 4.0f, &flux_acidic, &atp);

    /* At lower luminal pH, pump should work harder / be inhibited */
    EXPECT_GE(flux_neutral, flux_acidic);
}

TEST_F(ProtonPumpsTest, VatpaseNoFluxWithZeroATP) {
    nimcp_vatpase_set_activity(&system.vatpase, 1.0f);
    nimcp_vatpase_set_assembly(&system.vatpase, 1.0f);

    float h_flux, atp_cost;
    nimcp_vatpase_calculate_flux(&system.vatpase, 0.0f, 7.0f, &h_flux, &atp_cost);

    /* No ATP should mean no flux */
    EXPECT_NEAR(h_flux, 0.0f, FLOAT_TOLERANCE);
}

//=============================================================================
// NHE Tests
//=============================================================================

TEST_F(ProtonPumpsTest, NHESetActivity) {
    nimcp_pump_error_t err = nimcp_nhe_set_activity(
        &system.nhe, 0.6f);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_NEAR(system.nhe.activity_level, 0.6f, FLOAT_TOLERANCE);
}

TEST_F(ProtonPumpsTest, NHESetActivityNull) {
    nimcp_pump_error_t err = nimcp_nhe_set_activity(nullptr, 0.6f);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

TEST_F(ProtonPumpsTest, NHESetSetpoint) {
    nimcp_pump_error_t err = nimcp_nhe_set_setpoint(
        &system.nhe, 7.15f);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_NEAR(system.nhe.set_point, 7.15f, FLOAT_TOLERANCE);
}

TEST_F(ProtonPumpsTest, NHESetSetpointNull) {
    nimcp_pump_error_t err = nimcp_nhe_set_setpoint(nullptr, 7.15f);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

TEST_F(ProtonPumpsTest, NHECalculateExchange) {
    nimcp_nhe_set_activity(&system.nhe, 1.0f);
    nimcp_nhe_set_setpoint(&system.nhe, 7.2f);

    float exchange_rate;
    /* Intracellular pH below set point should activate NHE (export H+) */
    nimcp_pump_error_t err = nimcp_nhe_calculate_exchange(
        &system.nhe, 6.8f, 140.0f, &exchange_rate);
    EXPECT_EQ(err, PUMP_OK);
    /* NHE exports H+ when intracellular pH is too low */
    EXPECT_GT(exchange_rate, 0.0f);
}

TEST_F(ProtonPumpsTest, NHECalculateExchangeNull) {
    float exchange_rate;
    nimcp_pump_error_t err = nimcp_nhe_calculate_exchange(
        nullptr, 7.0f, 140.0f, &exchange_rate);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);

    err = nimcp_nhe_calculate_exchange(
        &system.nhe, 7.0f, 140.0f, nullptr);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

TEST_F(ProtonPumpsTest, NHEExchangeHigherWhenMoreAcidic) {
    nimcp_nhe_set_activity(&system.nhe, 1.0f);
    nimcp_nhe_set_setpoint(&system.nhe, 7.2f);

    float rate_mild_acidosis, rate_severe_acidosis;

    nimcp_nhe_calculate_exchange(&system.nhe, 7.0f, 140.0f, &rate_mild_acidosis);
    nimcp_nhe_calculate_exchange(&system.nhe, 6.5f, 140.0f, &rate_severe_acidosis);

    /* More severe acidosis should stimulate more H+ export */
    EXPECT_GE(rate_severe_acidosis, rate_mild_acidosis);
}

TEST_F(ProtonPumpsTest, NHEExchangeRequiresNaGradient) {
    nimcp_nhe_set_activity(&system.nhe, 1.0f);
    nimcp_nhe_set_setpoint(&system.nhe, 7.2f);

    float rate_with_na, rate_no_na;

    nimcp_nhe_calculate_exchange(&system.nhe, 6.8f, 140.0f, &rate_with_na);
    nimcp_nhe_calculate_exchange(&system.nhe, 6.8f, 0.0f, &rate_no_na);

    /* No Na+ gradient should reduce exchange */
    EXPECT_GT(rate_with_na, rate_no_na);
}

//=============================================================================
// NBC Tests
//=============================================================================

TEST_F(ProtonPumpsTest, NBCSetActivity) {
    nimcp_pump_error_t err = nimcp_nbc_set_activity(
        &system.nbc, 0.5f);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_NEAR(system.nbc.activity_level, 0.5f, FLOAT_TOLERANCE);
}

TEST_F(ProtonPumpsTest, NBCSetActivityNull) {
    nimcp_pump_error_t err = nimcp_nbc_set_activity(nullptr, 0.5f);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

TEST_F(ProtonPumpsTest, NBCCalculateTransport) {
    nimcp_nbc_set_activity(&system.nbc, 1.0f);

    float transport_rate;
    nimcp_pump_error_t err = nimcp_nbc_calculate_transport(
        &system.nbc, 10.0f, 5.0f, &transport_rate);
    EXPECT_EQ(err, PUMP_OK);
    /* With positive gradients, should have positive transport */
    EXPECT_GT(transport_rate, 0.0f);
}

TEST_F(ProtonPumpsTest, NBCCalculateTransportNull) {
    float transport_rate;
    nimcp_pump_error_t err = nimcp_nbc_calculate_transport(
        nullptr, 10.0f, 5.0f, &transport_rate);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);

    err = nimcp_nbc_calculate_transport(
        &system.nbc, 10.0f, 5.0f, nullptr);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

TEST_F(ProtonPumpsTest, NBCTransportIncreasesWithGradient) {
    nimcp_nbc_set_activity(&system.nbc, 1.0f);

    float rate_low_gradient, rate_high_gradient;

    nimcp_nbc_calculate_transport(&system.nbc, 2.0f, 1.0f, &rate_low_gradient);
    nimcp_nbc_calculate_transport(&system.nbc, 20.0f, 10.0f, &rate_high_gradient);

    EXPECT_GT(rate_high_gradient, rate_low_gradient);
}

//=============================================================================
// System Update Tests
//=============================================================================

TEST_F(ProtonPumpsTest, UpdateSystem) {
    nimcp_pump_error_t err = nimcp_pump_update(
        &system, 1.0f, 7.2f, 7.4f, 5.5f);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_EQ(system.update_count, 1u);
}

TEST_F(ProtonPumpsTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        nimcp_pump_error_t err = nimcp_pump_update(
            &system, 0.1f, 7.2f, 7.4f, 5.5f);
        EXPECT_EQ(err, PUMP_OK);
    }
    EXPECT_EQ(system.update_count, 100u);
}

TEST_F(ProtonPumpsTest, UpdateNull) {
    nimcp_pump_error_t err = nimcp_pump_update(
        nullptr, 1.0f, 7.2f, 7.4f, 5.5f);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

//=============================================================================
// Net H+ Flux Tests
//=============================================================================

TEST_F(ProtonPumpsTest, GetNetHFlux) {
    /* Perform some updates to accumulate flux */
    nimcp_pump_update(&system, 1.0f, 7.2f, 7.4f, 5.5f);

    float h_flux;
    nimcp_pump_error_t err = nimcp_pump_get_net_h_flux(&system, &h_flux);
    EXPECT_EQ(err, PUMP_OK);
    /* Net flux can be positive or negative depending on pump balance */
}

TEST_F(ProtonPumpsTest, GetNetHFluxNull) {
    float h_flux;
    nimcp_pump_error_t err = nimcp_pump_get_net_h_flux(nullptr, &h_flux);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);

    err = nimcp_pump_get_net_h_flux(&system, nullptr);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

//=============================================================================
// ATP Consumption Tests
//=============================================================================

TEST_F(ProtonPumpsTest, GetATPConsumption) {
    nimcp_pump_update(&system, 10.0f, 7.2f, 7.4f, 5.5f);

    float atp_rate;
    nimcp_pump_error_t err = nimcp_pump_get_atp_consumption(&system, &atp_rate);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_GE(atp_rate, 0.0f);
}

TEST_F(ProtonPumpsTest, GetATPConsumptionNull) {
    float atp_rate;
    nimcp_pump_error_t err = nimcp_pump_get_atp_consumption(nullptr, &atp_rate);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);

    err = nimcp_pump_get_atp_consumption(&system, nullptr);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

//=============================================================================
// ATP Supply Tests
//=============================================================================

TEST_F(ProtonPumpsTest, SupplyATP) {
    float initial_atp = system.atp_available;
    nimcp_pump_error_t err = nimcp_pump_supply_atp(&system, 5.0f);
    EXPECT_EQ(err, PUMP_OK);
    EXPECT_GT(system.atp_available, initial_atp);
}

TEST_F(ProtonPumpsTest, SupplyATPNull) {
    nimcp_pump_error_t err = nimcp_pump_supply_atp(nullptr, 5.0f);
    EXPECT_EQ(err, PUMP_ERR_NULL_PTR);
}

TEST_F(ProtonPumpsTest, SupplyATPNegative) {
    nimcp_pump_error_t err = nimcp_pump_supply_atp(&system, -1.0f);
    EXPECT_EQ(err, PUMP_ERR_INVALID_PARAM);
}

TEST_F(ProtonPumpsTest, ATPDepletionReducesPumping) {
    /* With ATP, pumps should work */
    nimcp_pump_supply_atp(&system, 10.0f);
    nimcp_vatpase_set_activity(&system.vatpase, 1.0f);
    nimcp_vatpase_set_assembly(&system.vatpase, 1.0f);

    float flux_with_atp, atp_cost;
    nimcp_vatpase_calculate_flux(&system.vatpase, 10.0f, 7.0f, &flux_with_atp, &atp_cost);

    float flux_no_atp;
    nimcp_vatpase_calculate_flux(&system.vatpase, 0.0f, 7.0f, &flux_no_atp, &atp_cost);

    EXPECT_GT(flux_with_atp, flux_no_atp);
}

//=============================================================================
// State String Tests
//=============================================================================

TEST_F(ProtonPumpsTest, StateStringAllValues) {
    EXPECT_NE(nimcp_pump_state_string(PUMP_STATE_INACTIVE), nullptr);
    EXPECT_NE(nimcp_pump_state_string(PUMP_STATE_BASAL), nullptr);
    EXPECT_NE(nimcp_pump_state_string(PUMP_STATE_ACTIVATED), nullptr);
    EXPECT_NE(nimcp_pump_state_string(PUMP_STATE_INHIBITED), nullptr);
    EXPECT_NE(nimcp_pump_state_string(PUMP_STATE_SATURATED), nullptr);
}

TEST_F(ProtonPumpsTest, StateStringUnknown) {
    const char* str = nimcp_pump_state_string((nimcp_pump_state_t)999);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Error String Tests
//=============================================================================

TEST_F(ProtonPumpsTest, ErrorStringAllValues) {
    EXPECT_NE(nimcp_pump_error_string(PUMP_OK), nullptr);
    EXPECT_NE(nimcp_pump_error_string(PUMP_ERR_NULL_PTR), nullptr);
    EXPECT_NE(nimcp_pump_error_string(PUMP_ERR_INVALID_PARAM), nullptr);
    EXPECT_NE(nimcp_pump_error_string(PUMP_ERR_NOT_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_pump_error_string(PUMP_ERR_NO_ATP), nullptr);
    EXPECT_NE(nimcp_pump_error_string(PUMP_ERR_GRADIENT_DEPLETED), nullptr);
    EXPECT_NE(nimcp_pump_error_string(PUMP_ERR_PUMP_INHIBITED), nullptr);
    EXPECT_NE(nimcp_pump_error_string(PUMP_ERR_CAPACITY_EXCEEDED), nullptr);
}

TEST_F(ProtonPumpsTest, ErrorStringUnknown) {
    const char* str = nimcp_pump_error_string((nimcp_pump_error_t)-999);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Integration-Style Tests
//=============================================================================

TEST_F(ProtonPumpsTest, FullPumpCycleWithATP) {
    /* Supply ATP, activate pumps, update, check consumption */
    nimcp_pump_supply_atp(&system, 20.0f);

    nimcp_vatpase_set_activity(&system.vatpase, 0.8f);
    nimcp_vatpase_set_assembly(&system.vatpase, 0.9f);
    nimcp_nhe_set_activity(&system.nhe, 0.7f);
    nimcp_nhe_set_setpoint(&system.nhe, 7.2f);
    nimcp_nbc_set_activity(&system.nbc, 0.6f);

    /* Run several update cycles */
    for (int i = 0; i < 10; i++) {
        nimcp_pump_error_t err = nimcp_pump_update(
            &system, 1.0f, 7.1f, 7.4f, 6.0f);
        EXPECT_EQ(err, PUMP_OK);
    }

    /* Check that pumps consumed ATP */
    float atp_rate;
    nimcp_pump_get_atp_consumption(&system, &atp_rate);
    EXPECT_GE(atp_rate, 0.0f);

    /* Check that system tracked updates */
    EXPECT_EQ(system.update_count, 10u);
}

TEST_F(ProtonPumpsTest, PumpStatesReflectActivity) {
    nimcp_vatpase_set_activity(&system.vatpase, 0.0f);
    EXPECT_EQ(system.vatpase.state, PUMP_STATE_INACTIVE);

    nimcp_vatpase_set_activity(&system.vatpase, 0.5f);
    /* After setting non-zero activity, state should not be INACTIVE */
    EXPECT_NE(system.vatpase.state, PUMP_STATE_INACTIVE);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
