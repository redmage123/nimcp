/**
 * @file test_world_simulator.cpp
 * @brief Tests for the World Simulator -- unified multi-physics coupling
 *
 * Verifies: create/destroy, engine registration, standard couplings,
 * stepping, conservation tracking, temperature field set/get round-trip.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/physics/nimcp_world_simulator.h"
}

/* ---- Fixture ---- */

class WorldSimulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = wsim_default_config();
        sim = wsim_create(&cfg);
    }
    void TearDown() override {
        if (sim) wsim_destroy(sim);
    }
    wsim_config_t cfg;
    world_simulator_t* sim;
};

/* ---- Tests ---- */

TEST(WorldSimulatorBasic, CreateDestroy) {
    wsim_config_t cfg = wsim_default_config();
    world_simulator_t* sim = wsim_create(&cfg);
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
    wsim_destroy(sim);
}

TEST(WorldSimulatorBasic, CreateNullConfig) {
    world_simulator_t* sim = wsim_create(NULL);
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
    wsim_destroy(sim);
}

TEST(WorldSimulatorConfig, DefaultConfigValues) {
    wsim_config_t cfg = wsim_default_config();
    EXPECT_GT(cfg.master_dt, 0.0f);
    EXPECT_GT(cfg.grid_dim, 0);
    EXPECT_GT(cfg.cell_size, 0.0f);
    EXPECT_GT(cfg.ambient_temperature, 0.0f);
    EXPECT_GT(cfg.ambient_pressure, 0.0f);
    EXPECT_GT(cfg.gravity, 0.0f);
}

TEST_F(WorldSimulatorTest, RegisterEngine) {
    /* Register a dummy engine (we don't need a real one -- opaque pointer) */
    int dummy_engine = 42;
    wsim_register_engine(sim, WSIM_ENGINE_NEWTONIAN, &dummy_engine, 0.01f);
    EXPECT_TRUE(sim->engine_active[WSIM_ENGINE_NEWTONIAN]);
    EXPECT_EQ(sim->engines[WSIM_ENGINE_NEWTONIAN], &dummy_engine);
}

TEST_F(WorldSimulatorTest, RegisterMultipleEngines) {
    int dummy1 = 1, dummy2 = 2, dummy3 = 3;
    wsim_register_engine(sim, WSIM_ENGINE_NEWTONIAN, &dummy1, 0.01f);
    wsim_register_engine(sim, WSIM_ENGINE_HEAT, &dummy2, 0.1f);
    wsim_register_engine(sim, WSIM_ENGINE_FLUID, &dummy3, 0.05f);

    EXPECT_TRUE(sim->engine_active[WSIM_ENGINE_NEWTONIAN]);
    EXPECT_TRUE(sim->engine_active[WSIM_ENGINE_HEAT]);
    EXPECT_TRUE(sim->engine_active[WSIM_ENGINE_FLUID]);
    EXPECT_FALSE(sim->engine_active[WSIM_ENGINE_QED]);
}

TEST_F(WorldSimulatorTest, RegisterCoupling) {
    wsim_coupling_t coupling;
    memset(&coupling, 0, sizeof(coupling));
    coupling.type = WSIM_COUPLE_HEAT_TO_CHEM;
    coupling.source = WSIM_ENGINE_HEAT;
    coupling.target = WSIM_ENGINE_BULK_CHEM;
    coupling.strength = 1.0f;
    coupling.enabled = true;

    uint32_t id = wsim_register_coupling(sim, &coupling);
    ASSERT_NE(id, UINT32_MAX);
    EXPECT_GE(sim->num_couplings, 1u);
}

TEST_F(WorldSimulatorTest, EnableStandardCouplings) {
    wsim_enable_standard_couplings(sim);
    /* Should have registered multiple couplings */
    EXPECT_GT(sim->num_couplings, 0u);
}

TEST_F(WorldSimulatorTest, StepEmpty) {
    /* Step with no engines registered -- should not crash */
    int rc = wsim_step(sim, 0.01f);
    EXPECT_EQ(rc, 0);
}

TEST(WorldSimulatorGrid, TemperatureSetGet) {
    wsim_config_t cfg = wsim_default_config();
    cfg.grid_dim = 8;
    cfg.ambient_temperature = 293.15f;
    world_simulator_t* sim = wsim_create(&cfg);
    ASSERT_NE(sim, nullptr);

    /* Set temperature at (2,3,4) to 500K */
    wsim_set_temperature(sim, 2, 3, 4, 500.0f);
    float T = wsim_get_temperature(sim, 2, 3, 4);
    EXPECT_NEAR(T, 500.0f, 0.1f);

    /* Another point should be ambient */
    float T_ambient = wsim_get_temperature(sim, 0, 0, 0);
    EXPECT_NEAR(T_ambient, cfg.ambient_temperature, 1.0f);

    wsim_destroy(sim);
}

TEST(WorldSimulatorGrid, PressureSet) {
    wsim_config_t cfg = wsim_default_config();
    cfg.grid_dim = 8;
    world_simulator_t* sim = wsim_create(&cfg);
    ASSERT_NE(sim, nullptr);

    /* Set pressure -- should not crash */
    wsim_set_pressure(sim, 1, 1, 1, 200000.0f);

    wsim_destroy(sim);
}

TEST(WorldSimulatorConservation, Tracking) {
    wsim_config_t cfg = wsim_default_config();
    cfg.enforce_conservation = true;
    cfg.grid_dim = 8;
    world_simulator_t* sim = wsim_create(&cfg);
    ASSERT_NE(sim, nullptr);

    float e_drift = 0.0f, m_drift = 0.0f, c_drift = 0.0f;
    wsim_conservation_report(sim, &e_drift, &m_drift, &c_drift);
    /* Initially no drift */
    EXPECT_NEAR(e_drift, 0.0f, 0.001f);
    EXPECT_NEAR(m_drift, 0.0f, 0.001f);
    EXPECT_NEAR(c_drift, 0.0f, 0.001f);

    wsim_destroy(sim);
}

TEST_F(WorldSimulatorTest, NoViolationsEmpty) {
    bool violations = wsim_check_violations(sim);
    EXPECT_FALSE(violations);
}

TEST_F(WorldSimulatorTest, StatsInitial) {
    wsim_stats_t stats = wsim_get_stats(sim);
    EXPECT_EQ(stats.master_steps, 0);
    EXPECT_EQ(stats.active_engines, 0);
}

TEST_F(WorldSimulatorTest, StatsAfterStep) {
    wsim_step(sim, 0.01f);
    wsim_step(sim, 0.01f);

    wsim_stats_t stats = wsim_get_stats(sim);
    EXPECT_EQ(stats.master_steps, 2);
}
