/**
 * @file test_physiology.cpp
 * @brief Tests for the physiology simulation engine (gtest)
 *
 * Validates Hill equation O2 dissociation, cardiac output, Henderson-Hasselbalch
 * blood pH, GFR, Poiseuille flow, acid-base classification.
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "cognitive/physics/nimcp_physiology.h"
}

class PhysiologyTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = physiology_default_config();
        sim = physiology_create(&cfg);
    }
    void TearDown() override {
        if (sim) physiology_destroy(sim);
    }
    physiology_config_t cfg{};
    physiology_sim_t* sim = nullptr;
};

TEST_F(PhysiologyTest, CreateDestroy) {
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
}

/* ---- Hill equation: SO2 = pO2^n / (P50^n + pO2^n) ----------------------- */

TEST(PhysiologyHillTest, AtP50) {
    float so2 = physiology_hill_equation(PHYSIO_P50_MMHG, PHYSIO_P50_MMHG,
                                          PHYSIO_HILL_N);
    EXPECT_NEAR(so2, 0.5f, 0.01f);
}

TEST(PhysiologyHillTest, HighPO2) {
    float so2 = physiology_hill_equation(100.0f, PHYSIO_P50_MMHG, PHYSIO_HILL_N);
    EXPECT_GT(so2, 0.95f);
    EXPECT_LE(so2, 1.0f);
}

TEST(PhysiologyHillTest, LowPO2) {
    float so2 = physiology_hill_equation(10.0f, PHYSIO_P50_MMHG, PHYSIO_HILL_N);
    EXPECT_LT(so2, 0.2f);
    EXPECT_GT(so2, 0.0f);
}

TEST(PhysiologyHillTest, SigmoidalShape) {
    float so2_low = physiology_hill_equation(10.0f, PHYSIO_P50_MMHG, PHYSIO_HILL_N);
    float so2_p50 = physiology_hill_equation(PHYSIO_P50_MMHG, PHYSIO_P50_MMHG,
                                              PHYSIO_HILL_N);
    float so2_high = physiology_hill_equation(100.0f, PHYSIO_P50_MMHG, PHYSIO_HILL_N);
    EXPECT_LT(so2_low, so2_p50);
    EXPECT_LT(so2_p50, so2_high);
}

/* ---- cardiac output: CO = HR * SV / 1000 -------------------------------- */

TEST(PhysiologyCardiacTest, OutputResting) {
    float co = physiology_cardiac_output(PHYSIO_RESTING_HR, PHYSIO_RESTING_SV_ML);
    EXPECT_NEAR(co, 5.04f, 0.5f);
}

TEST(PhysiologyCardiacTest, OutputExercise) {
    float co = physiology_cardiac_output(150.0f, 100.0f);
    EXPECT_NEAR(co, 15.0f, 1.0f);
}

/* ---- Henderson-Hasselbalch: pH = 6.1 + log([HCO3-]/(0.03*pCO2)) -------- */

TEST(PhysiologyHHTest, Normal) {
    float ph = physiology_henderson_hasselbalch(24.0f, 40.0f);
    EXPECT_NEAR(ph, 7.4f, 0.05f);
}

TEST(PhysiologyHHTest, Acidosis) {
    float ph = physiology_henderson_hasselbalch(24.0f, 60.0f);
    EXPECT_LT(ph, 7.35f);
}

TEST(PhysiologyHHTest, Alkalosis) {
    float ph = physiology_henderson_hasselbalch(30.0f, 40.0f);
    EXPECT_GT(ph, 7.45f);
}

/* ---- GFR: Kf * (Pgc - Pbc - pi_gc) -------------------------------------- */

TEST(PhysiologyGFRTest, Normal) {
    float gfr = physiology_gfr(PHYSIO_KF_NORMAL, PHYSIO_PGC_MMHG,
                                PHYSIO_PBC_MMHG, PHYSIO_PI_GC_MMHG);
    EXPECT_NEAR(gfr, 125.0f, 5.0f);
}

/* ---- MAP = CO * TPR ------------------------------------------------------ */

TEST(PhysiologyMAPTest, Normal) {
    float map = physiology_map(PHYSIO_RESTING_CO_L_MIN, PHYSIO_TPR_NORMAL);
    EXPECT_NEAR(map, 93.0f, 5.0f);
}

/* ---- acid-base classification -------------------------------------------- */

TEST(PhysiologyAcidBaseTest, ClassificationNormal) {
    physio_acidbase_status_t status = physiology_classify_acidbase(7.40f, 40.0f, 24.0f);
    EXPECT_EQ(status, PHYSIO_ACIDBASE_NORMAL);
}

TEST(PhysiologyAcidBaseTest, ClassificationRespAcidosis) {
    physio_acidbase_status_t status = physiology_classify_acidbase(7.25f, 60.0f, 24.0f);
    EXPECT_EQ(status, PHYSIO_ACIDBASE_RESP_ACIDOSIS);
}

/* ---- load healthy adult -------------------------------------------------- */

TEST_F(PhysiologyTest, LoadHealthyAdult) {
    physiology_load_healthy_adult(sim);
    EXPECT_NEAR(sim->cardiovascular.heart_rate, PHYSIO_RESTING_HR, 10.0f);
    EXPECT_NEAR(sim->acidbase.blood_ph, 7.4f, 0.1f);
}

/* ---- step basic ---------------------------------------------------------- */

TEST_F(PhysiologyTest, StepBasic) {
    physiology_load_healthy_adult(sim);
    int rc = physiology_step(sim, 1.0f);
    EXPECT_EQ(rc, 0);
}
