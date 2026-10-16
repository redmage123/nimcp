/**
 * @file test_cell_biology.cpp
 * @brief Tests for cell biology: cell cycle, membrane transport, signaling (gtest)
 *
 * Validates cell cycle phase advancement, Fick's law diffusion,
 * facilitated transport saturation, osmotic pressure, and cell creation.
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "cognitive/physics/nimcp_cell_biology.h"
}

class CellBiologyTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = cell_biology_default_config();
        sim = cell_biology_create(&cfg);
    }
    void TearDown() override {
        if (sim) cell_biology_destroy(sim);
    }
    cell_biology_config_t cfg{};
    cell_biology_sim_t* sim = nullptr;
};

TEST_F(CellBiologyTest, CreateDestroy) {
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
}

TEST_F(CellBiologyTest, AddCell) {
    cell_sim_cell_t cell = {};
    cell.phase = CELL_PHASE_G1;
    cell.phase_progress = 0.0f;
    cell.atp_level = 1.0f;
    cell.dna_integrity = 1.0f;
    cell.chromosome_count = 46;
    cell.is_diploid = true;
    cell.alive = true;

    int rc = cell_biology_add_cell(sim, &cell);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(sim->num_cells, 1u);
}

TEST(CellBiologyCycleTest, PhasesAdvance) {
    cell_biology_config_t cfg = cell_biology_default_config();
    cfg.dt = 1.0f;
    cfg.growth_factor_conc = 1.0f;
    cfg.nutrient_level = 1.0f;
    cfg.oxygen_level = 1.0f;
    cell_biology_sim_t* sim = cell_biology_create(&cfg);
    ASSERT_NE(sim, nullptr);

    cell_sim_cell_t cell = {};
    cell.phase = CELL_PHASE_G1;
    cell.phase_progress = 0.0f;
    cell.atp_level = 1.0f;
    cell.dna_integrity = 1.0f;
    cell.chromosome_count = 46;
    cell.is_diploid = true;
    cell.alive = true;
    cell.regulators.cyclin_d = 0.8f;
    cell.regulators.cyclin_e = 0.8f;
    cell.regulators.cyclin_a = 0.8f;
    cell.regulators.cyclin_b = 0.9f;
    cell.regulators.rb = 0.5f;
    cell_biology_add_cell(sim, &cell);

    cell_phase_t initial_phase = sim->cells[0].phase;

    for (int i = 0; i < 30; i++) {
        cell_biology_advance_cycle(sim, 0, cfg.dt);
    }

    bool phase_changed = (sim->cells[0].phase != initial_phase) ||
                         (sim->cells[0].phase_progress > 0.5f);
    EXPECT_TRUE(phase_changed);

    cell_biology_destroy(sim);
}

/* ---- Fick's law: J = -D * dC/dx ----------------------------------------- */

TEST(CellBiologyFickTest, PositiveGradient) {
    float J = cell_biology_fick_diffusion(1.0e-9f, 10.0f, 1.0e-6f);
    EXPECT_NEAR(fabsf(J), 0.01f, 0.001f);
}

TEST(CellBiologyFickTest, ProportionalToGradient) {
    float J1 = cell_biology_fick_diffusion(1.0e-9f, 5.0f, 1.0e-6f);
    float J2 = cell_biology_fick_diffusion(1.0e-9f, 10.0f, 1.0e-6f);
    EXPECT_NEAR(fabsf(J2), 2.0f * fabsf(J1), fabsf(J1) * 0.01f);
}

/* ---- facilitated transport saturates at Vmax ----------------------------- */

TEST(CellBiologyTransportTest, AtKm) {
    float v = cell_biology_facilitated_transport(10.0f, 5.0f, 5.0f);
    EXPECT_NEAR(v, 5.0f, 0.1f);
}

TEST(CellBiologyTransportTest, Saturation) {
    float Vmax = 10.0f, Km = 5.0f;
    float v = cell_biology_facilitated_transport(Vmax, Km, 100000.0f);
    EXPECT_NEAR(v, Vmax, 0.1f);
}

TEST(CellBiologyTransportTest, LowSubstrate) {
    float v = cell_biology_facilitated_transport(10.0f, 5.0f, 0.01f);
    float expected = 10.0f * 0.01f / 5.01f;
    EXPECT_NEAR(v, expected, 0.01f);
}

/* ---- osmotic pressure: pi = i*M*R*T ------------------------------------ */

TEST(CellBiologyOsmoticTest, Pressure) {
    float pi = cell_biology_osmotic_pressure(1.0f, 1.0f, 310.15f);
    EXPECT_GT(pi, 0.0f);
    float pi2 = cell_biology_osmotic_pressure(1.0f, 2.0f, 310.15f);
    EXPECT_GT(pi2, pi);
}

/* ---- load mammalian cell ------------------------------------------------- */

TEST_F(CellBiologyTest, LoadMammalianCell) {
    cell_biology_load_mammalian_cell(sim);
    EXPECT_GE(sim->num_cells, 1u);
    EXPECT_TRUE(sim->cells[0].alive);
}

/* ---- step simulation ----------------------------------------------------- */

TEST_F(CellBiologyTest, StepBasic) {
    cell_biology_load_mammalian_cell(sim);
    int rc = cell_biology_step(sim, 0.1f);
    EXPECT_EQ(rc, 0);
}
