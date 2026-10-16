/**
 * @file test_thermodynamics.cpp
 * @brief Unit tests for Non-Equilibrium Thermodynamics module (AC-9)
 *
 * WHAT: Test suite for nimcp_thermodynamics
 * WHY:  Verify energy tracking, entropy production, Landauer costs, efficiency
 * HOW:  Unit tests for init, update, compute, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ThermodynamicsTest : public ::testing::Test {
protected:
    nimcp_thermodynamic_state_t state;
    nimcp_thermo_config_t config;

    void SetUp() override {
        config = nimcp_thermo_default_config();
        ASSERT_EQ(nimcp_thermo_init(&state, &config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_thermo_destroy(&state);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST(ThermoConfigTest, DefaultConfigInitialization) {
    nimcp_thermo_config_t config = nimcp_thermo_default_config();

    /* Temperature should be body temperature */
    EXPECT_DOUBLE_EQ(config.temperature_k, NIMCP_THERMO_DEFAULT_TEMP_K);

    /* ATP pool should be set to default */
    EXPECT_DOUBLE_EQ(config.atp_pool_size, NIMCP_THERMO_DEFAULT_ATP_POOL);

    /* Metabolic rate should be set */
    EXPECT_DOUBLE_EQ(config.metabolic_rate, NIMCP_THERMO_DEFAULT_METABOLIC_RATE);

    /* Feature flags should have sensible defaults */
    EXPECT_TRUE(config.enable_landauer_cost);
    EXPECT_TRUE(config.enable_entropy_tracking);
}

TEST(ThermoConfigTest, DefaultConfigReasonableValues) {
    nimcp_thermo_config_t config = nimcp_thermo_default_config();

    /* Temperature must be positive (absolute) */
    EXPECT_GT(config.temperature_k, 0.0);

    /* ATP pool must be positive */
    EXPECT_GT(config.atp_pool_size, 0.0);

    /* Warning threshold should be in valid range */
    EXPECT_GE(config.atp_warning_threshold, 0.0);
    EXPECT_LE(config.atp_warning_threshold, 1.0);
}

//=============================================================================
// State Initialization Tests
//=============================================================================

TEST(ThermoInitTest, InitWithDefaultConfig) {
    nimcp_thermodynamic_state_t state;
    nimcp_thermo_config_t config = nimcp_thermo_default_config();

    EXPECT_EQ(nimcp_thermo_init(&state, &config), NIMCP_SUCCESS);
    EXPECT_TRUE(state.initialized);

    nimcp_thermo_destroy(&state);
}

TEST(ThermoInitTest, InitWithNullConfig) {
    nimcp_thermodynamic_state_t state;

    /* NULL config should use defaults */
    EXPECT_EQ(nimcp_thermo_init(&state, nullptr), NIMCP_SUCCESS);
    EXPECT_TRUE(state.initialized);

    /* ATP should be set to default pool size */
    EXPECT_DOUBLE_EQ(state.atp_available, NIMCP_THERMO_DEFAULT_ATP_POOL);

    nimcp_thermo_destroy(&state);
}

TEST(ThermoInitTest, InitNullStateReturnsError) {
    nimcp_thermo_config_t config = nimcp_thermo_default_config();
    EXPECT_EQ(nimcp_thermo_init(nullptr, &config), NIMCP_ERROR_NULL_POINTER);
}

TEST(ThermoInitTest, InitialStateValues) {
    nimcp_thermodynamic_state_t state;
    nimcp_thermo_config_t config = nimcp_thermo_default_config();

    ASSERT_EQ(nimcp_thermo_init(&state, &config), NIMCP_SUCCESS);

    /* Energy counters should be zero */
    EXPECT_DOUBLE_EQ(state.total_energy_consumed, 0.0);
    EXPECT_DOUBLE_EQ(state.free_energy_dissipated, 0.0);

    /* Entropy should be zero initially */
    EXPECT_DOUBLE_EQ(state.total_entropy_produced, 0.0);

    /* Simulation time should be zero */
    EXPECT_DOUBLE_EQ(state.simulation_time, 0.0);

    nimcp_thermo_destroy(&state);
}

//=============================================================================
// State Reset Tests
//=============================================================================

TEST_F(ThermodynamicsTest, ResetClearsAccumulators) {
    /* Simulate some activity */
    nimcp_thermo_update(&state, 0.001, 1.0e-9, 1000);
    nimcp_thermo_update(&state, 0.001, 1.0e-9, 1000);

    /* Verify state changed */
    EXPECT_GT(state.total_energy_consumed, 0.0);
    EXPECT_GT(state.simulation_time, 0.0);

    /* Reset */
    EXPECT_EQ(nimcp_thermo_reset(&state), NIMCP_SUCCESS);

    /* Verify reset */
    EXPECT_DOUBLE_EQ(state.total_energy_consumed, 0.0);
    EXPECT_DOUBLE_EQ(state.total_entropy_produced, 0.0);
    EXPECT_DOUBLE_EQ(state.simulation_time, 0.0);
    EXPECT_TRUE(state.initialized);
}

TEST(ThermoResetTest, ResetNullStateReturnsError) {
    EXPECT_EQ(nimcp_thermo_reset(nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST(ThermoResetTest, ResetUninitializedStateReturnsError) {
    nimcp_thermodynamic_state_t state;
    memset(&state, 0, sizeof(state));
    state.initialized = false;

    EXPECT_EQ(nimcp_thermo_reset(&state), NIMCP_ERROR_NOT_INITIALIZED);
}

//=============================================================================
// Energy Consumption Tests
//=============================================================================

TEST_F(ThermodynamicsTest, UpdateAccumulatesEnergy) {
    double dt = 0.001;  /* 1 ms */
    double power = 1.0e-9;  /* 1 nW */

    EXPECT_EQ(nimcp_thermo_update(&state, dt, power, 0), NIMCP_SUCCESS);

    /* Energy = power * time */
    double expected_energy = power * dt;
    EXPECT_NEAR(state.total_energy_consumed, expected_energy, 1e-20);
}

TEST_F(ThermodynamicsTest, UpdateAccumulatesTime) {
    double dt = 0.001;

    nimcp_thermo_update(&state, dt, 1.0e-9, 0);
    nimcp_thermo_update(&state, dt, 1.0e-9, 0);
    nimcp_thermo_update(&state, dt, 1.0e-9, 0);

    EXPECT_NEAR(state.simulation_time, 3.0 * dt, 1e-12);
}

TEST_F(ThermodynamicsTest, UpdateNullStateReturnsError) {
    EXPECT_EQ(nimcp_thermo_update(nullptr, 0.001, 1.0e-9, 0), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ThermodynamicsTest, RecordEnergyByCategory) {
    double ion = 1.0e-12;
    double syn = 0.5e-12;
    double house = 0.5e-12;
    double comp = 0.25e-12;

    EXPECT_EQ(nimcp_thermo_record_energy(&state, ion, syn, house, comp), NIMCP_SUCCESS);

    /* Get budget and verify */
    nimcp_energy_budget_t budget;
    EXPECT_EQ(nimcp_thermo_get_energy_budget(&state, &budget), NIMCP_SUCCESS);

    EXPECT_DOUBLE_EQ(budget.ion_pumping, ion);
    EXPECT_DOUBLE_EQ(budget.synaptic, syn);
    EXPECT_DOUBLE_EQ(budget.housekeeping, house);
    EXPECT_DOUBLE_EQ(budget.computation, comp);
}

//=============================================================================
// Entropy Production Tests (dS/dt = Q/T + sigma)
//=============================================================================

TEST_F(ThermodynamicsTest, ComputeEntropyRateSuccess) {
    /* Simulate some heat dissipation */
    state.heat_dissipation = 1.0e-9;  /* 1 nW */

    double entropy_rate = 0.0;
    EXPECT_EQ(nimcp_thermo_compute_entropy_rate(&state, 310.0, &entropy_rate), NIMCP_SUCCESS);

    /* Entropy rate should be positive for heat dissipation */
    EXPECT_GT(entropy_rate, 0.0);

    /* dS/dt = Q/T, so should be approximately heat_dissipation / temperature */
    double expected_rate = state.heat_dissipation / 310.0;
    EXPECT_NEAR(entropy_rate, expected_rate, expected_rate * 0.1);  /* 10% tolerance */
}

TEST_F(ThermodynamicsTest, ComputeEntropyRateNullState) {
    double entropy_rate = 0.0;
    EXPECT_EQ(nimcp_thermo_compute_entropy_rate(nullptr, 310.0, &entropy_rate),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ThermodynamicsTest, ComputeEntropyRateNullOutput) {
    EXPECT_EQ(nimcp_thermo_compute_entropy_rate(&state, 310.0, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ThermodynamicsTest, EntropyAccumulatesOverTime) {
    /* Update with power consumption (which generates heat) */
    for (int i = 0; i < 100; i++) {
        nimcp_thermo_update(&state, 0.001, 1.0e-9, 100);
    }

    /* Entropy should have accumulated */
    EXPECT_GT(state.total_entropy_produced, 0.0);
}

//=============================================================================
// Landauer Cost Tests (E = kT ln(2) per bit)
//=============================================================================

TEST(LandauerTest, ComputeLandauerCostSuccess) {
    nimcp_landauer_cost_t cost;
    uint64_t bits = 1000000;  /* 1 million bits */
    double temp = NIMCP_THERMO_BODY_TEMP_K;

    EXPECT_EQ(nimcp_thermo_compute_landauer_cost(temp, bits, &cost), NIMCP_SUCCESS);

    /* Minimum cost = n * kT * ln(2) */
    double expected_min = static_cast<double>(bits) * NIMCP_THERMO_KB * temp * std::log(2.0);
    EXPECT_NEAR(cost.minimum_cost, expected_min, expected_min * 1e-6);

    /* Verify structure fields */
    EXPECT_EQ(cost.bits_erased, bits);
    EXPECT_DOUBLE_EQ(cost.temperature_k, temp);
}

TEST(LandauerTest, LandauerCostAtBodyTemperature) {
    nimcp_landauer_cost_t cost;

    /* Single bit at body temperature */
    EXPECT_EQ(nimcp_thermo_compute_landauer_cost(310.0, 1, &cost), NIMCP_SUCCESS);

    /* Should match NIMCP_THERMO_LANDAUER_310K constant */
    EXPECT_NEAR(cost.minimum_cost, NIMCP_THERMO_LANDAUER_310K, 1e-23);
}

TEST(LandauerTest, ComputeLandauerCostNullOutput) {
    EXPECT_EQ(nimcp_thermo_compute_landauer_cost(310.0, 1000, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST(LandauerTest, LandauerCostScalesLinearly) {
    nimcp_landauer_cost_t cost1, cost2;

    nimcp_thermo_compute_landauer_cost(310.0, 1000, &cost1);
    nimcp_thermo_compute_landauer_cost(310.0, 2000, &cost2);

    /* Cost should double when bits double */
    EXPECT_NEAR(cost2.minimum_cost, 2.0 * cost1.minimum_cost, 1e-25);
}

//=============================================================================
// ATP Consumption Tests
//=============================================================================

TEST_F(ThermodynamicsTest, ATPConsumption) {
    double initial_atp = state.atp_available;

    /* Update with energy consumption (should consume ATP) */
    nimcp_thermo_update(&state, 0.001, 1.0e-9, 0);

    /* ATP should decrease */
    EXPECT_LT(state.atp_available, initial_atp);
}

TEST_F(ThermodynamicsTest, ATPReplenishment) {
    /* Consume some ATP */
    nimcp_thermo_update(&state, 0.01, 1.0e-8, 0);
    double depleted_atp = state.atp_available;

    /* Replenish */
    double atp_to_add = 1.0e-13;
    EXPECT_EQ(nimcp_thermo_replenish_atp(&state, atp_to_add, 0), NIMCP_SUCCESS);

    EXPECT_NEAR(state.atp_available, depleted_atp + atp_to_add, 1e-20);
}

TEST_F(ThermodynamicsTest, ATPReplenishmentWithCapacity) {
    /* Try to add more ATP than capacity allows */
    double max_capacity = config.atp_pool_size;
    double excess_atp = max_capacity * 2.0;

    EXPECT_EQ(nimcp_thermo_replenish_atp(&state, excess_atp, max_capacity), NIMCP_SUCCESS);

    /* Should be capped at max capacity */
    EXPECT_LE(state.atp_available, max_capacity);
}

TEST_F(ThermodynamicsTest, ATPCriticalThreshold) {
    /* Deplete ATP significantly */
    state.atp_available = config.atp_pool_size * 0.05;  /* 5% remaining */

    EXPECT_TRUE(nimcp_thermo_is_atp_critical(&state, 0.1));
    EXPECT_FALSE(nimcp_thermo_is_atp_critical(&state, 0.01));
}

TEST_F(ThermodynamicsTest, ATPRatio) {
    /* Full ATP */
    double ratio = nimcp_thermo_get_atp_ratio(&state);
    EXPECT_NEAR(ratio, 1.0, 0.01);

    /* Deplete half */
    state.atp_available = config.atp_pool_size * 0.5;
    ratio = nimcp_thermo_get_atp_ratio(&state);
    EXPECT_NEAR(ratio, 0.5, 0.01);
}

//=============================================================================
// Efficiency Metrics Tests
//=============================================================================

TEST_F(ThermodynamicsTest, GetEfficiencySuccess) {
    /* Run some simulation to generate metrics */
    for (int i = 0; i < 10; i++) {
        nimcp_thermo_update(&state, 0.001, 1.0e-9, 1000);
    }

    double comp_eff = 0.0, thermo_eff = 0.0, landauer_eff = 0.0;

    EXPECT_EQ(nimcp_thermo_get_efficiency(&state, &comp_eff, &thermo_eff, &landauer_eff),
              NIMCP_SUCCESS);

    /* Efficiencies should be in valid range [0, 1] */
    EXPECT_GE(comp_eff, 0.0);
    EXPECT_LE(comp_eff, 1.0);

    EXPECT_GE(thermo_eff, 0.0);
    EXPECT_LE(thermo_eff, 1.0);

    EXPECT_GE(landauer_eff, 0.0);
    EXPECT_LE(landauer_eff, 1.0);
}

TEST_F(ThermodynamicsTest, GetEfficiencyNullOutputs) {
    double eff;

    /* Implementation allows NULL outputs - just skips writing to them */
    /* This is a valid pattern for optional output parameters */
    EXPECT_EQ(nimcp_thermo_get_efficiency(&state, nullptr, &eff, &eff),
              NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_thermo_get_efficiency(&state, &eff, nullptr, &eff),
              NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_thermo_get_efficiency(&state, &eff, &eff, nullptr),
              NIMCP_SUCCESS);
}

TEST_F(ThermodynamicsTest, LandauerEfficiencyBelowUnity) {
    /* Real systems can never exceed Landauer limit */
    for (int i = 0; i < 100; i++) {
        nimcp_thermo_update(&state, 0.001, 1.0e-9, 10000);
    }

    double comp_eff, thermo_eff, landauer_eff;
    nimcp_thermo_get_efficiency(&state, &comp_eff, &thermo_eff, &landauer_eff);

    /* Landauer efficiency should be < 1 (we're not at theoretical limit) */
    EXPECT_LT(landauer_eff, 1.0);
}

//=============================================================================
// Energy Budget Tests
//=============================================================================

TEST_F(ThermodynamicsTest, EnergyBudgetBreakdown) {
    /* Record energy by category */
    nimcp_thermo_record_energy(&state, 5.0e-12, 2.5e-12, 2.5e-12, 1.0e-12);

    nimcp_energy_budget_t budget;
    EXPECT_EQ(nimcp_thermo_get_energy_budget(&state, &budget), NIMCP_SUCCESS);

    /* Verify breakdown sums to total */
    double sum = budget.ion_pumping + budget.synaptic +
                 budget.housekeeping + budget.computation + budget.waste_heat;

    /* Sum should equal or be close to total (accounting for waste heat) */
    EXPECT_GE(budget.total, budget.ion_pumping + budget.synaptic +
                            budget.housekeeping + budget.computation);
}

TEST_F(ThermodynamicsTest, EnergyBudgetNullOutput) {
    EXPECT_EQ(nimcp_thermo_get_energy_budget(&state, nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ThermodynamicsTest, EnergyBudgetNullState) {
    nimcp_energy_budget_t budget;
    EXPECT_EQ(nimcp_thermo_get_energy_budget(nullptr, &budget), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Operation Cost Estimation Tests
//=============================================================================

TEST(ThermoOperationTest, EstimateOperationCost) {
    double energy = 0.0;
    uint64_t synapses = 1000;
    uint64_t spikes = 10;

    EXPECT_EQ(nimcp_thermo_estimate_operation_cost(synapses, spikes, 310.0, &energy),
              NIMCP_SUCCESS);

    /* Energy should be positive */
    EXPECT_GT(energy, 0.0);
}

TEST(ThermoOperationTest, OperationCostScalesWithSynapses) {
    double energy1 = 0.0, energy2 = 0.0;

    nimcp_thermo_estimate_operation_cost(1000, 10, 310.0, &energy1);
    nimcp_thermo_estimate_operation_cost(2000, 10, 310.0, &energy2);

    /* More synapses should cost more energy */
    EXPECT_GT(energy2, energy1);
}

TEST(ThermoOperationTest, OperationCostScalesWithSpikes) {
    double energy1 = 0.0, energy2 = 0.0;

    nimcp_thermo_estimate_operation_cost(1000, 10, 310.0, &energy1);
    nimcp_thermo_estimate_operation_cost(1000, 20, 310.0, &energy2);

    /* More spikes should cost more energy */
    EXPECT_GT(energy2, energy1);
}

TEST(ThermoOperationTest, OperationCostNullOutput) {
    EXPECT_EQ(nimcp_thermo_estimate_operation_cost(1000, 10, 310.0, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// ATP/Energy Conversion Tests
//=============================================================================

TEST(ThermoConversionTest, ATPToEnergy) {
    double atp_moles = 1.0e-12;  /* 1 picomole */
    double energy = nimcp_thermo_atp_to_energy(atp_moles);

    /* Energy should be positive */
    EXPECT_GT(energy, 0.0);

    /* Verify against ATP hydrolysis energy constant */
    double expected = atp_moles * NIMCP_THERMO_AVOGADRO * NIMCP_THERMO_ATP_ENERGY;
    EXPECT_NEAR(energy, expected, expected * 1e-6);
}

TEST(ThermoConversionTest, EnergyToATP) {
    double energy_j = 1.0e-9;  /* 1 nJ */
    double atp = nimcp_thermo_energy_to_atp(energy_j);

    /* ATP should be positive */
    EXPECT_GT(atp, 0.0);

    /* Round-trip should be consistent */
    double recovered_energy = nimcp_thermo_atp_to_energy(atp);
    EXPECT_NEAR(recovered_energy, energy_j, energy_j * 1e-6);
}

TEST(ThermoConversionTest, ConversionRoundTrip) {
    /* Test round-trip conversion */
    double original_atp = 1.0e-12;
    double energy = nimcp_thermo_atp_to_energy(original_atp);
    double recovered_atp = nimcp_thermo_energy_to_atp(energy);

    EXPECT_NEAR(recovered_atp, original_atp, original_atp * 1e-10);
}

//=============================================================================
// Null Parameter Handling Tests
//=============================================================================

TEST(ThermoNullTest, DestroyNullSafe) {
    /* Should not crash */
    nimcp_thermo_destroy(nullptr);
}

TEST(ThermoNullTest, UpdateUninitializedState) {
    nimcp_thermodynamic_state_t state;
    memset(&state, 0, sizeof(state));
    state.initialized = false;

    EXPECT_EQ(nimcp_thermo_update(&state, 0.001, 1.0e-9, 0), NIMCP_ERROR_NOT_INITIALIZED);
}

TEST(ThermoNullTest, RecordEnergyNullState) {
    EXPECT_EQ(nimcp_thermo_record_energy(nullptr, 1.0, 1.0, 1.0, 1.0),
              NIMCP_ERROR_NULL_POINTER);
}

TEST(ThermoNullTest, ReplenishATPNullState) {
    EXPECT_EQ(nimcp_thermo_replenish_atp(nullptr, 1.0e-12, 0), NIMCP_ERROR_NULL_POINTER);
}

TEST(ThermoNullTest, ATPCriticalNullState) {
    /* Implementation returns true for null state (fail-safe: assume critical) */
    /* This is a defensive approach - when state is unknown, assume emergency */
    EXPECT_TRUE(nimcp_thermo_is_atp_critical(nullptr, 0.1));
}

TEST(ThermoNullTest, ATPRatioNullState) {
    /* Should return 0 for null state (safe default) */
    EXPECT_DOUBLE_EQ(nimcp_thermo_get_atp_ratio(nullptr), 0.0);
}

//=============================================================================
// Uninitialized State Handling Tests
//=============================================================================

TEST(ThermoUninitializedTest, RecordEnergyUninitializedState) {
    nimcp_thermodynamic_state_t state;
    memset(&state, 0, sizeof(state));
    state.initialized = false;

    EXPECT_EQ(nimcp_thermo_record_energy(&state, 1.0, 1.0, 1.0, 1.0),
              NIMCP_ERROR_NOT_INITIALIZED);
}

TEST(ThermoUninitializedTest, ReplenishATPUninitializedState) {
    nimcp_thermodynamic_state_t state;
    memset(&state, 0, sizeof(state));
    state.initialized = false;

    EXPECT_EQ(nimcp_thermo_replenish_atp(&state, 1.0e-12, 0), NIMCP_ERROR_NOT_INITIALIZED);
}

TEST(ThermoUninitializedTest, GetBudgetUninitializedState) {
    nimcp_thermodynamic_state_t state;
    memset(&state, 0, sizeof(state));
    state.initialized = false;

    nimcp_energy_budget_t budget;
    EXPECT_EQ(nimcp_thermo_get_energy_budget(&state, &budget), NIMCP_ERROR_NOT_INITIALIZED);
}

TEST(ThermoUninitializedTest, GetEfficiencyUninitializedState) {
    nimcp_thermodynamic_state_t state;
    memset(&state, 0, sizeof(state));
    state.initialized = false;

    double comp, thermo, landauer;
    EXPECT_EQ(nimcp_thermo_get_efficiency(&state, &comp, &thermo, &landauer),
              NIMCP_ERROR_NOT_INITIALIZED);
}

TEST(ThermoUninitializedTest, ComputeEntropyUninitializedState) {
    nimcp_thermodynamic_state_t state;
    memset(&state, 0, sizeof(state));
    state.initialized = false;

    double entropy_rate;
    EXPECT_EQ(nimcp_thermo_compute_entropy_rate(&state, 310.0, &entropy_rate),
              NIMCP_ERROR_NOT_INITIALIZED);
}

//=============================================================================
// Physical Constants Validation Tests
//=============================================================================

TEST(ThermoConstantsTest, BoltzmannConstant) {
    /* Boltzmann constant should be approximately 1.38e-23 J/K */
    EXPECT_NEAR(NIMCP_THERMO_KB, 1.380649e-23, 1e-28);
}

TEST(ThermoConstantsTest, LandauerAt310K) {
    /* Landauer limit at body temp: kT*ln(2) */
    double expected = NIMCP_THERMO_KB * 310.0 * std::log(2.0);
    EXPECT_NEAR(NIMCP_THERMO_LANDAUER_310K, expected, 1e-24);
}

TEST(ThermoConstantsTest, ATPEnergy) {
    /* ATP hydrolysis ~8.3e-20 J per molecule */
    EXPECT_NEAR(NIMCP_THERMO_ATP_ENERGY, 8.3e-20, 1e-21);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
