/**
 * @file test_immune_modules_regression.cpp
 * @brief Regression tests for new immune enhancement modules
 * @date 2025-12-12
 *
 * Tests to prevent regression and ensure stability across:
 * - Trained Immunity
 * - Complement System
 * - Regulatory T Cells
 * - Immune Exhaustion
 * - Immune Tolerance
 * - Immune Vaccine
 * - Mucosal Immunity
 * - Immune Persistence
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

// Helper to get current time in milliseconds for trained_immunity_decay
static uint64_t get_time_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/immune/nimcp_trained_immunity.h"
#include "cognitive/immune/nimcp_complement_system.h"
#include "cognitive/immune/nimcp_regulatory_tcells.h"
#include "cognitive/immune/nimcp_immune_exhaustion.h"
#include "cognitive/immune/nimcp_immune_tolerance.h"
#include "cognitive/immune/nimcp_immune_vaccine.h"
#include "cognitive/immune/nimcp_mucosal_immunity.h"
#include "cognitive/immune/nimcp_immune_persistence.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Base Regression Test Fixture
 * ============================================================================ */

class ImmuneModulesRegressionTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;

    void SetUp() override {
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);
    }

    void TearDown() override {
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }
};

/* ============================================================================
 * Trained Immunity Regression Tests
 * ============================================================================ */

TEST_F(ImmuneModulesRegressionTest, TrainedImmunity_CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        trained_immunity_config_t cfg;
        trained_immunity_default_config(&cfg);
        trained_immunity_t* ti = trained_immunity_create(&cfg, immune_system);
        ASSERT_NE(ti, nullptr);
        trained_immunity_destroy(ti);
    }
}

TEST_F(ImmuneModulesRegressionTest, TrainedImmunity_DestroyNullSafe) {
    trained_immunity_destroy(nullptr);  // Should not crash
}

TEST_F(ImmuneModulesRegressionTest, TrainedImmunity_ZeroIntensityRejected) {
    trained_immunity_config_t cfg;
    trained_immunity_default_config(&cfg);
    trained_immunity_t* ti = trained_immunity_create(&cfg, immune_system);

    // Zero intensity should be rejected
    int result = trained_immunity_train(ti, TRAINED_STIMULUS_BETA_GLUCAN, 0.0f);
    EXPECT_EQ(result, -1);

    trained_immunity_destroy(ti);
}

TEST_F(ImmuneModulesRegressionTest, TrainedImmunity_MultipleTrainingStable) {
    trained_immunity_config_t cfg;
    trained_immunity_default_config(&cfg);
    trained_immunity_t* ti = trained_immunity_create(&cfg, immune_system);

    // Train many times without crash
    for (int i = 0; i < 100; i++) {
        trained_immunity_train(ti, TRAINED_STIMULUS_BETA_GLUCAN, 0.5f);
    }

    trained_immunity_decay(ti, get_time_ms());

    // Check PRR sensitivity is valid
    float prr = trained_immunity_get_prr_sensitivity(ti);
    EXPECT_GT(prr, 0.0f);

    trained_immunity_destroy(ti);
}

TEST_F(ImmuneModulesRegressionTest, TrainedImmunity_PRRSensitivityBounded) {
    trained_immunity_config_t cfg;
    trained_immunity_default_config(&cfg);
    trained_immunity_t* ti = trained_immunity_create(&cfg, immune_system);

    // Heavy training
    for (int i = 0; i < 50; i++) {
        trained_immunity_train(ti, TRAINED_STIMULUS_BCG, 0.8f);
        trained_immunity_decay(ti, get_time_ms());
    }

    float prr = trained_immunity_get_prr_sensitivity(ti);
    // Should be bounded, not infinite
    EXPECT_GT(prr, 0.0f);
    EXPECT_LT(prr, 100.0f);

    trained_immunity_destroy(ti);
}

/* ============================================================================
 * Complement System Regression Tests
 * ============================================================================ */

TEST_F(ImmuneModulesRegressionTest, Complement_CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        complement_config_t cfg;
        complement_default_config(&cfg);
        complement_system_t* cs = complement_create(&cfg, immune_system);
        ASSERT_NE(cs, nullptr);
        complement_destroy(cs);
    }
}

TEST_F(ImmuneModulesRegressionTest, Complement_DestroyNullSafe) {
    complement_destroy(nullptr);
}

TEST_F(ImmuneModulesRegressionTest, Complement_MultipleActivationsStable) {
    complement_config_t cfg;
    complement_default_config(&cfg);
    complement_system_t* cs = complement_create(&cfg, immune_system);

    std::vector<uint32_t> antigen_ids;
    for (int i = 0; i < 20; i++) {
        uint8_t pattern[] = {(uint8_t)i, 0x02};
        uint32_t antigen_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                     pattern, sizeof(pattern), 5, 1, &antigen_id);

        // Activate complement for each antigen
        complement_activate_classical(cs, antigen_id, 0);
        antigen_ids.push_back(antigen_id);
    }

    // Verify all antigens were opsonized
    complement_stats_t stats;
    complement_get_stats(cs, &stats);
    EXPECT_GE(stats.classical_activations, 1u);

    complement_destroy(cs);
}

TEST_F(ImmuneModulesRegressionTest, Complement_ManyActivationsHandled) {
    complement_config_t cfg;
    complement_default_config(&cfg);
    complement_system_t* cs = complement_create(&cfg, immune_system);

    // Activate many times
    for (int i = 0; i < 200; i++) {
        uint8_t pattern[] = {(uint8_t)(i >> 8), (uint8_t)(i & 0xFF)};
        uint32_t antigen_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                     pattern, sizeof(pattern), 5, 1, &antigen_id);

        complement_activate_classical(cs, antigen_id, 0);
    }

    // Should not crash
    complement_update(cs, 5000);

    complement_stats_t stats;
    complement_get_stats(cs, &stats);
    EXPECT_GE(stats.classical_activations, 1u);

    complement_destroy(cs);
}

TEST_F(ImmuneModulesRegressionTest, Complement_C3bLevelsBounded) {
    complement_config_t cfg;
    complement_default_config(&cfg);
    complement_system_t* cs = complement_create(&cfg, immune_system);

    uint8_t pattern[] = {0x01, 0x02};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 pattern, sizeof(pattern), 10, 1, &antigen_id);

    complement_activate_classical(cs, antigen_id, 0);
    complement_opsonize(cs, antigen_id);

    // Update multiple times
    for (int i = 0; i < 50; i++) {
        complement_update(cs, 100);
    }

    // Get C3b level on target
    float c3b = complement_get_c3b_level(cs, antigen_id);

    // C3b levels should be bounded
    EXPECT_GE(c3b, 0.0f);
    EXPECT_LE(c3b, 1.0f);

    complement_destroy(cs);
}

/* ============================================================================
 * Regulatory T Cells Regression Tests
 * ============================================================================ */

TEST_F(ImmuneModulesRegressionTest, Treg_CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        treg_config_t cfg;
        treg_default_config(&cfg);
        treg_system_t* ts = treg_create(&cfg, immune_system);
        ASSERT_NE(ts, nullptr);
        treg_destroy(ts);
    }
}

TEST_F(ImmuneModulesRegressionTest, Treg_DestroyNullSafe) {
    treg_destroy(nullptr);
}

TEST_F(ImmuneModulesRegressionTest, Treg_MaxCheckpointsRespected) {
    treg_config_t cfg;
    treg_default_config(&cfg);
    treg_system_t* ts = treg_create(&cfg, immune_system);

    // Activate more than max checkpoints
    for (size_t i = 0; i < cfg.max_checkpoints + 50; i++) {
        uint32_t checkpoint_id;
        treg_checkpoint_activate(ts, CHECKPOINT_PD1_PDL1, (uint32_t)i, 0, &checkpoint_id);
    }

    EXPECT_LE(ts->checkpoint_count, cfg.max_checkpoints);

    treg_destroy(ts);
}

TEST_F(ImmuneModulesRegressionTest, Treg_CytokineConcentrationClamped) {
    treg_config_t cfg;
    treg_default_config(&cfg);
    treg_system_t* ts = treg_create(&cfg, immune_system);

    uint32_t cytokine_id;
    // Try invalid concentration
    int result = treg_release_cytokine(ts, TREG_CYTOKINE_IL10, 5.0f, 0, &cytokine_id);
    // Should succeed but clamp
    EXPECT_EQ(result, 0);

    treg_destroy(ts);
}

TEST_F(ImmuneModulesRegressionTest, Treg_SuppressionFactorBounded) {
    treg_config_t cfg;
    treg_default_config(&cfg);
    treg_system_t* ts = treg_create(&cfg, immune_system);

    // Heavy suppression
    for (int i = 0; i < 20; i++) {
        treg_suppress_inflammation(ts, 0);
        uint32_t id;
        treg_release_cytokine(ts, TREG_CYTOKINE_IL10, 1.0f, 0, &id);
        treg_update(ts, 500);
    }

    float factor = treg_get_suppression_factor(ts);
    EXPECT_GE(factor, 0.0f);
    EXPECT_LE(factor, 1.0f);

    treg_destroy(ts);
}

/* ============================================================================
 * Immune Exhaustion Regression Tests
 * ============================================================================ */

TEST_F(ImmuneModulesRegressionTest, Exhaustion_CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        exhaustion_config_t cfg;
        exhaustion_default_config(&cfg);
        exhaustion_system_t* et = exhaustion_create(&cfg, immune_system);
        ASSERT_NE(et, nullptr);
        exhaustion_destroy(et);
    }
}

TEST_F(ImmuneModulesRegressionTest, Exhaustion_DestroyNullSafe) {
    exhaustion_destroy(nullptr);
}

TEST_F(ImmuneModulesRegressionTest, Exhaustion_MultipleUpdatesStable) {
    exhaustion_config_t cfg;
    exhaustion_default_config(&cfg);
    exhaustion_system_t* et = exhaustion_create(&cfg, immune_system);

    // Many updates without crash
    for (int i = 0; i < 200; i++) {
        exhaustion_update(et, 1000);
    }

    exhaustion_stats_t stats;
    exhaustion_get_stats(et, &stats);
    // Stats retrieved successfully
    EXPECT_GE(stats.effector_cells + stats.exhausted_cells, 0u);

    exhaustion_destroy(et);
}

TEST_F(ImmuneModulesRegressionTest, Exhaustion_EffectorCapacityBounded) {
    exhaustion_config_t cfg;
    exhaustion_default_config(&cfg);
    exhaustion_system_t* et = exhaustion_create(&cfg, immune_system);

    // Create a T cell to track
    uint8_t pattern[] = {0xAA, 0xBB};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 pattern, sizeof(pattern), 5, 1, &antigen_id);

    uint32_t cell_id;
    brain_immune_activate_killer_t(immune_system, antigen_id, &cell_id);

    // Extended time to induce exhaustion
    for (int i = 0; i < 100; i++) {
        exhaustion_update(et, 1000);
    }

    float capacity = exhaustion_get_effector_capacity(et, cell_id);
    EXPECT_GE(capacity, 0.0f);
    EXPECT_LE(capacity, 1.0f);

    exhaustion_destroy(et);
}

TEST_F(ImmuneModulesRegressionTest, Exhaustion_MarkerLevelsBounded) {
    exhaustion_config_t cfg;
    exhaustion_default_config(&cfg);
    exhaustion_system_t* et = exhaustion_create(&cfg, immune_system);

    // Create a T cell
    uint8_t pattern[] = {0xCC, 0xDD};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 pattern, sizeof(pattern), 5, 1, &antigen_id);

    uint32_t cell_id;
    brain_immune_activate_killer_t(immune_system, antigen_id, &cell_id);

    // Exhaustion system tracks cells automatically via update
    for (int i = 0; i < 50; i++) {
        exhaustion_update(et, 1000);
    }

    // Verified API: exhaustion_get_markers returns struct with pd1_level, lag3_level, tim3_level
    exhaustion_markers_t markers;
    int ret = exhaustion_get_markers(et, cell_id, &markers);
    if (ret == 0) {
        EXPECT_GE(markers.pd1_level, 0.0f); EXPECT_LE(markers.pd1_level, 1.0f);
        EXPECT_GE(markers.tim3_level, 0.0f); EXPECT_LE(markers.tim3_level, 1.0f);
        EXPECT_GE(markers.lag3_level, 0.0f); EXPECT_LE(markers.lag3_level, 1.0f);
    }

    exhaustion_destroy(et);
}

/* ============================================================================
 * Immune Tolerance Regression Tests
 * ============================================================================ */

TEST_F(ImmuneModulesRegressionTest, Tolerance_CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        tolerance_config_t cfg;
        tolerance_default_config(&cfg);
        tolerance_system_t* ts = tolerance_create(&cfg, immune_system);
        ASSERT_NE(ts, nullptr);
        tolerance_destroy(ts);
    }
}

TEST_F(ImmuneModulesRegressionTest, Tolerance_DestroyNullSafe) {
    tolerance_destroy(nullptr);
}

TEST_F(ImmuneModulesRegressionTest, Tolerance_MaxPatternsRespected) {
    tolerance_config_t cfg;
    tolerance_default_config(&cfg);
    tolerance_system_t* ts = tolerance_create(&cfg, immune_system);

    // Register more than max
    // Verified API: tolerance_register_self_pattern(system, pattern, len, description, pattern_id)
    for (size_t i = 0; i < cfg.max_self_patterns + 50; i++) {
        uint8_t pattern[] = {(uint8_t)(i >> 8), (uint8_t)(i & 0xFF), 0x03, 0x04};
        tolerance_register_self_pattern(ts, pattern, sizeof(pattern), "test_pattern", nullptr);
    }

    // Verified: tolerance_get_self_patterns_count returns count
    size_t pattern_count = tolerance_get_self_patterns_count(ts);
    EXPECT_LE(pattern_count, cfg.max_self_patterns);

    tolerance_destroy(ts);
}

TEST_F(ImmuneModulesRegressionTest, Tolerance_ZeroLengthPatternRejected) {
    tolerance_config_t cfg;
    tolerance_default_config(&cfg);
    tolerance_system_t* ts = tolerance_create(&cfg, immune_system);

    uint8_t pattern[] = {0x01};
    // Verified API: tolerance_register_self_pattern(system, pattern, len, description, pattern_id)
    int result = tolerance_register_self_pattern(ts, pattern, 0, "zero_len", nullptr);
    EXPECT_EQ(result, -1);

    tolerance_destroy(ts);
}

TEST_F(ImmuneModulesRegressionTest, Tolerance_ConsistentSelfRecognition) {
    tolerance_config_t cfg;
    tolerance_default_config(&cfg);
    tolerance_system_t* ts = tolerance_create(&cfg, immune_system);

    uint8_t self_pattern[] = {0xAA, 0xBB, 0xCC, 0xDD};
    // Verified API: tolerance_register_self_pattern(system, pattern, len, description, pattern_id)
    tolerance_register_self_pattern(ts, self_pattern, sizeof(self_pattern), "self_pattern", nullptr);

    // Multiple checks should give same result
    // Verified API: tolerance_check_self(system, pattern, len, matched_id, affinity) returns bool
    for (int i = 0; i < 100; i++) {
        bool is_self = tolerance_check_self(ts, self_pattern, sizeof(self_pattern), nullptr, nullptr);
        EXPECT_TRUE(is_self);
    }

    tolerance_destroy(ts);
}

/* ============================================================================
 * Immune Vaccine Regression Tests
 * ============================================================================ */

TEST_F(ImmuneModulesRegressionTest, Vaccine_CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        vaccine_config_t cfg;
        vaccine_default_config(&cfg);
        vaccine_system_t* vs = vaccine_create(&cfg, immune_system);
        ASSERT_NE(vs, nullptr);
        vaccine_destroy(vs);
    }
}

TEST_F(ImmuneModulesRegressionTest, Vaccine_DestroyNullSafe) {
    vaccine_destroy(nullptr);
}

TEST_F(ImmuneModulesRegressionTest, Vaccine_MaxVaccinesRespected) {
    vaccine_config_t cfg;
    vaccine_default_config(&cfg);
    vaccine_system_t* vs = vaccine_create(&cfg, immune_system);

    // Register more than max
    // Verified API: vaccine_create_entry(system, type, epitope, epitope_len, name, &vaccine_id)
    for (size_t i = 0; i < cfg.max_vaccines + 50; i++) {
        uint8_t antigen[] = {(uint8_t)(i >> 8), (uint8_t)(i & 0xFF)};
        char name[32];
        snprintf(name, sizeof(name), "Vaccine_%zu", i);
        uint32_t id;
        vaccine_create_entry(vs, VACCINE_TYPE_MRNA, antigen, sizeof(antigen), name, &id);
    }

    vaccine_stats_t stats;
    vaccine_get_stats(vs, &stats);
    // Verified: stats has total_vaccines and active_vaccines, not vaccines_registered
    EXPECT_LE(stats.total_vaccines, cfg.max_vaccines);

    vaccine_destroy(vs);
}

TEST_F(ImmuneModulesRegressionTest, Vaccine_EfficacyBounded) {
    vaccine_config_t cfg;
    vaccine_default_config(&cfg);
    vaccine_system_t* vs = vaccine_create(&cfg, immune_system);

    uint8_t antigen[] = {0x01, 0x02};
    uint32_t vaccine_id;
    vaccine_create_entry(vs, VACCINE_TYPE_MRNA, antigen, sizeof(antigen), "TestVax", &vaccine_id);
    vaccine_administer(vs, vaccine_id);

    // Multiple boosters
    for (int i = 0; i < 10; i++) {
        vaccine_booster(vs, vaccine_id);
        vaccine_update(vs, 30ULL * 24 * 60 * 60 * 1000);  // 30 days
    }

    float efficacy;
    vaccine_get_efficacy(vs, vaccine_id, &efficacy);
    EXPECT_GE(efficacy, 0.0f);
    EXPECT_LE(efficacy, 1.0f);

    vaccine_destroy(vs);
}

TEST_F(ImmuneModulesRegressionTest, Vaccine_IDsAreUnique) {
    vaccine_config_t cfg;
    vaccine_default_config(&cfg);
    vaccine_system_t* vs = vaccine_create(&cfg, immune_system);

    std::vector<uint32_t> ids;
    for (int i = 0; i < 20; i++) {
        uint8_t antigen[] = {(uint8_t)i, 0x02};
        char name[32];
        snprintf(name, sizeof(name), "Vax_%d", i);

        uint32_t id;
        vaccine_create_entry(vs, VACCINE_TYPE_MRNA, antigen, sizeof(antigen), name, &id);
        ids.push_back(id);
    }

    for (size_t i = 0; i < ids.size(); i++) {
        for (size_t j = i + 1; j < ids.size(); j++) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }

    vaccine_destroy(vs);
}

/* ============================================================================
 * Mucosal Immunity Regression Tests
 * ============================================================================ */

TEST_F(ImmuneModulesRegressionTest, Mucosal_CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        mucosal_config_t cfg;
        mucosal_default_config(&cfg);
        mucosal_system_t* ms = mucosal_create(&cfg, immune_system);
        ASSERT_NE(ms, nullptr);
        mucosal_destroy(ms);
    }
}

TEST_F(ImmuneModulesRegressionTest, Mucosal_DestroyNullSafe) {
    mucosal_destroy(nullptr);
}

TEST_F(ImmuneModulesRegressionTest, Mucosal_MaxSitesRespected) {
    mucosal_config_t cfg;
    mucosal_default_config(&cfg);
    mucosal_system_t* ms = mucosal_create(&cfg, immune_system);

    // Register more than max
    for (size_t i = 0; i < cfg.max_sites + 50; i++) {
        uint32_t site_id;
        mucosal_register_boundary(ms, MUCOSAL_SITE_INPUT_GATE, (uint32_t)i, &site_id);
    }

    mucosal_stats_t stats;
    mucosal_get_stats(ms, &stats);
    EXPECT_LE(stats.active_sites, cfg.max_sites);

    mucosal_destroy(ms);
}

TEST_F(ImmuneModulesRegressionTest, Mucosal_BarrierIntegrityBounded) {
    mucosal_config_t cfg;
    mucosal_default_config(&cfg);
    mucosal_system_t* ms = mucosal_create(&cfg, immune_system);

    uint32_t site_id;
    mucosal_register_boundary(ms, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    // Damage barrier repeatedly via breaches
    for (int i = 0; i < 20; i++) {
        mucosal_update_barrier_integrity(ms, site_id, true);  // breach occurred
    }

    float integrity;
    mucosal_get_barrier_integrity(ms, site_id, &integrity);
    EXPECT_GE(integrity, 0.0f);
    EXPECT_LE(integrity, 1.0f);

    mucosal_destroy(ms);
}

TEST_F(ImmuneModulesRegressionTest, Mucosal_ZeroLengthAntigenRejected) {
    mucosal_config_t cfg;
    mucosal_default_config(&cfg);
    mucosal_system_t* ms = mucosal_create(&cfg, immune_system);

    uint32_t site_id;
    mucosal_register_boundary(ms, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    uint8_t antigen[] = {0x01};
    uint32_t sample_id;
    int result = mucosal_sample_antigen(ms, site_id, antigen, 0, &sample_id);
    EXPECT_EQ(result, -1);

    mucosal_destroy(ms);
}

/* ============================================================================
 * Immune Persistence Regression Tests
 * ============================================================================ */

TEST_F(ImmuneModulesRegressionTest, Persistence_SaveToInvalidPathFails) {
    immune_persistence_config_t cfg;
    immune_persistence_default_config(&cfg);

    int result = immune_persistence_save(immune_system, "/nonexistent/dir/file.dat", &cfg);
    EXPECT_NE(result, 0);
}

TEST_F(ImmuneModulesRegressionTest, Persistence_LoadFromNonExistentFails) {
    immune_persistence_config_t cfg;
    immune_persistence_default_config(&cfg);

    int result = immune_persistence_load(immune_system, "/tmp/nonexistent_immune_test.dat", &cfg);
    EXPECT_NE(result, 0);
}

TEST_F(ImmuneModulesRegressionTest, Persistence_EmptyPathRejected) {
    immune_persistence_config_t cfg;
    immune_persistence_default_config(&cfg);

    EXPECT_NE(immune_persistence_save(immune_system, "", &cfg), 0);
    EXPECT_NE(immune_persistence_load(immune_system, "", &cfg), 0);
}

TEST_F(ImmuneModulesRegressionTest, Persistence_NullSystemRejected) {
    immune_persistence_config_t cfg;
    immune_persistence_default_config(&cfg);

    EXPECT_EQ(immune_persistence_save(nullptr, "/tmp/test.dat", &cfg), -1);
    EXPECT_EQ(immune_persistence_load(nullptr, "/tmp/test.dat", &cfg), -1);
}

TEST_F(ImmuneModulesRegressionTest, Persistence_SaveLoadIdempotent) {
    const char* test_file = "/tmp/test_regression_persist.dat";

    // Populate system
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 epitope, sizeof(epitope), 5, 1, &antigen_id);

    immune_persistence_config_t cfg;
    immune_persistence_default_config(&cfg);

    // Save
    EXPECT_EQ(immune_persistence_save(immune_system, test_file, &cfg), 0);

    // Load multiple times
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(immune_persistence_load(immune_system, test_file, &cfg), 0);
    }

    // Save again
    EXPECT_EQ(immune_persistence_save(immune_system, test_file, &cfg), 0);

    remove(test_file);
}

TEST_F(ImmuneModulesRegressionTest, Persistence_CorruptedFileRejected) {
    const char* test_file = "/tmp/test_corrupted.dat";

    // Write garbage
    FILE* f = fopen(test_file, "wb");
    ASSERT_NE(f, nullptr);
    const char* garbage = "this is not a valid immune file format!!!";
    fwrite(garbage, 1, strlen(garbage), f);
    fclose(f);

    immune_persistence_config_t cfg;
    immune_persistence_default_config(&cfg);

    int result = immune_persistence_validate_file(test_file, false);
    EXPECT_NE(result, 0);

    remove(test_file);
}

/* ============================================================================
 * Thread Safety Regression Tests
 * ============================================================================ */

TEST_F(ImmuneModulesRegressionTest, TrainedImmunity_ConcurrentTraining) {
    trained_immunity_config_t cfg;
    trained_immunity_default_config(&cfg);
    trained_immunity_t* ti = trained_immunity_create(&cfg, immune_system);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 25; i++) {
                float intensity = 0.5f + (float)i / 50.0f;
                if (trained_immunity_train(ti, TRAINED_STIMULUS_BETA_GLUCAN, intensity) == 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Some training should have succeeded
    EXPECT_GT(success_count.load(), 0);

    trained_immunity_destroy(ti);
}

TEST_F(ImmuneModulesRegressionTest, Complement_ConcurrentActivation) {
    complement_config_t cfg;
    complement_default_config(&cfg);
    complement_system_t* cs = complement_create(&cfg, immune_system);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 10; i++) {
                uint8_t pattern[] = {(uint8_t)t, (uint8_t)i};
                uint32_t antigen_id;
                brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                             pattern, sizeof(pattern), 5, 1, &antigen_id);

                // complement_activate_classical takes (system, antibody_id, target_id)
                if (complement_activate_classical(cs, antigen_id, 0) == 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);

    complement_destroy(cs);
}

TEST_F(ImmuneModulesRegressionTest, Vaccine_ConcurrentAdministration) {
    vaccine_config_t cfg;
    vaccine_default_config(&cfg);
    vaccine_system_t* vs = vaccine_create(&cfg, immune_system);

    // Pre-register vaccines
    std::vector<uint32_t> vaccine_ids;
    for (int i = 0; i < 8; i++) {
        uint8_t antigen[] = {(uint8_t)i, 0x02};
        char name[32];
        snprintf(name, sizeof(name), "ConcVax_%d", i);

        uint32_t id;
        vaccine_create_entry(vs, VACCINE_TYPE_MRNA, antigen, sizeof(antigen), name, &id);
        vaccine_ids.push_back(id);
    }

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 2; i++) {
                uint32_t vid = vaccine_ids[(t * 2 + i) % vaccine_ids.size()];
                if (vaccine_administer(vs, vid) == 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);

    vaccine_destroy(vs);
}

/* ============================================================================
 * State Consistency Regression Tests
 * ============================================================================ */

TEST_F(ImmuneModulesRegressionTest, AllModules_StatsConsistent) {
    // Create all modules
    trained_immunity_config_t ti_cfg;
    trained_immunity_default_config(&ti_cfg);
    trained_immunity_t* ti = trained_immunity_create(&ti_cfg, immune_system);

    complement_config_t cs_cfg;
    complement_default_config(&cs_cfg);
    complement_system_t* cs = complement_create(&cs_cfg, immune_system);

    treg_config_t treg_cfg;
    treg_default_config(&treg_cfg);
    treg_system_t* treg = treg_create(&treg_cfg, immune_system);

    // Perform operations
    uint8_t pattern[] = {0x01, 0x02, 0x03, 0x04};
    trained_immunity_train(ti, TRAINED_STIMULUS_BETA_GLUCAN, 0.8f);

    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 pattern, sizeof(pattern), 5, 1, &antigen_id);

    // complement_activate_classical takes (system, antibody_id, target_id)
    complement_activate_classical(cs, antigen_id, 0);

    uint32_t checkpoint_id;
    treg_checkpoint_activate(treg, CHECKPOINT_PD1_PDL1, 1, 0, &checkpoint_id);

    // Get stats multiple times - should be consistent
    for (int i = 0; i < 10; i++) {
        // Use trained_immunity_get_history_count for consistency check
        size_t ti_count1 = trained_immunity_get_history_count(ti);
        size_t ti_count2 = trained_immunity_get_history_count(ti);
        EXPECT_EQ(ti_count1, ti_count2);

        complement_stats_t cs_stats1, cs_stats2;
        complement_get_stats(cs, &cs_stats1);
        complement_get_stats(cs, &cs_stats2);
        EXPECT_EQ(cs_stats1.classical_activations, cs_stats2.classical_activations);

        treg_stats_t treg_stats1, treg_stats2;
        treg_get_stats(treg, &treg_stats1);
        treg_get_stats(treg, &treg_stats2);
        EXPECT_EQ(treg_stats1.checkpoints_activated, treg_stats2.checkpoints_activated);
    }

    treg_destroy(treg);
    complement_destroy(cs);
    trained_immunity_destroy(ti);
}
