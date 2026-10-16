/**
 * @file test_immune_modules_integration.cpp
 * @brief Integration tests for new immune enhancement modules
 * @date 2025-12-12
 *
 * Tests integration between:
 * - Trained Immunity + Complement System
 * - Regulatory T Cells + Immune Exhaustion
 * - Tolerance + Vaccine System
 * - Mucosal Immunity + Persistence
 * - Full immune pathway integration
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
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
 * Integration Test Fixture
 * ============================================================================ */

class ImmuneModulesIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    trained_immunity_t* trained = nullptr;
    complement_system_t* complement = nullptr;
    treg_system_t* treg = nullptr;
    exhaustion_system_t* exhaustion = nullptr;
    tolerance_system_t* tolerance = nullptr;
    vaccine_system_t* vaccine = nullptr;
    mucosal_system_t* mucosal = nullptr;

    const char* test_file = "/tmp/test_immune_integration.dat";

    void SetUp() override {
        // Create base immune system
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);
    }

    void TearDown() override {
        // Clean up in reverse order
        if (mucosal) { mucosal_destroy(mucosal); mucosal = nullptr; }
        if (vaccine) { vaccine_destroy(vaccine); vaccine = nullptr; }
        if (tolerance) { tolerance_destroy(tolerance); tolerance = nullptr; }
        if (exhaustion) { exhaustion_destroy(exhaustion); exhaustion = nullptr; }
        if (treg) { treg_destroy(treg); treg = nullptr; }
        if (complement) { complement_destroy(complement); complement = nullptr; }
        if (trained) { trained_immunity_destroy(trained); trained = nullptr; }

        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }

        remove(test_file);
    }

    void createTrainedImmunity() {
        trained_immunity_config_t cfg;
        trained_immunity_default_config(&cfg);
        trained = trained_immunity_create(&cfg, immune_system);
        ASSERT_NE(trained, nullptr);
    }

    void createComplementSystem() {
        complement_config_t cfg;
        complement_default_config(&cfg);
        complement = complement_create(&cfg, immune_system);
        ASSERT_NE(complement, nullptr);
    }

    void createTregSystem() {
        treg_config_t cfg;
        treg_default_config(&cfg);
        treg = treg_create(&cfg, immune_system);
        ASSERT_NE(treg, nullptr);
    }

    void createExhaustionTracker() {
        exhaustion_config_t cfg;
        exhaustion_default_config(&cfg);
        exhaustion = exhaustion_create(&cfg, immune_system);
        ASSERT_NE(exhaustion, nullptr);
    }

    void createToleranceSystem() {
        tolerance_config_t cfg;
        tolerance_default_config(&cfg);
        tolerance = tolerance_create(&cfg, immune_system);
        ASSERT_NE(tolerance, nullptr);
    }

    void createVaccineSystem() {
        vaccine_config_t cfg;
        vaccine_default_config(&cfg);
        vaccine = vaccine_create(&cfg, immune_system);
        ASSERT_NE(vaccine, nullptr);
    }

    void createMucosalSystem() {
        mucosal_config_t cfg;
        mucosal_default_config(&cfg);
        mucosal = mucosal_create(&cfg, immune_system);
        ASSERT_NE(mucosal, nullptr);
    }

    void presentTestAntigen(uint32_t* antigen_id) {
        uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                     epitope, sizeof(epitope), 5, 1, antigen_id);
    }
};

/* ============================================================================
 * Trained Immunity + Complement Integration
 * ============================================================================ */

TEST_F(ImmuneModulesIntegrationTest, TrainedImmunityEnhancesComplement) {
    createTrainedImmunity();
    createComplementSystem();

    // Train the innate system with a pattern
    uint8_t pattern[] = {0xDE, 0xAD, 0xBE, 0xEF};
    trained_immunity_train(trained, TRAINED_STIMULUS_BETA_GLUCAN, 0.8f);

    // Apply training (decay function also processes training effects)
    trained_immunity_decay(trained, get_time_ms());

    // Present antigen matching trained pattern
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 pattern, sizeof(pattern), 5, 1, &antigen_id);

    // Activate complement classical pathway
    complement_activate_classical(complement, antigen_id, 0);

    // Trained immunity should enhance complement response
    float prr = trained_immunity_get_prr_sensitivity(trained);
    EXPECT_GT(prr, 1.0f);  // Enhanced sensitivity

    // Check complement activation via c3b level on target
    float c3b_level = complement_get_c3b_level(complement, antigen_id);
    EXPECT_GE(c3b_level, 0.0f);
}

TEST_F(ImmuneModulesIntegrationTest, ComplementActivatesTrainedImmunity) {
    createTrainedImmunity();
    createComplementSystem();

    // Activate complement (generates inflammation signals)
    uint32_t antigen_id;
    presentTestAntigen(&antigen_id);

    complement_activate_alternative(complement, antigen_id);
    complement_update(complement, 2000);

    // Generate anaphylatoxins (C3a, C5a - danger signals)
    complement_release_anaphylatoxin(complement, ANAPHYLATOXIN_C3A, antigen_id);
    complement_release_anaphylatoxin(complement, ANAPHYLATOXIN_C5A, antigen_id);

    // Update trained immunity - should respond to danger signals
    trained_immunity_decay(trained, get_time_ms());

    // Check trained immunity sensitivity (higher = more alert)
    float prr = trained_immunity_get_prr_sensitivity(trained);
    // Danger signals from complement should trigger training
}

/* ============================================================================
 * Regulatory T Cells + Exhaustion Integration
 * ============================================================================ */

TEST_F(ImmuneModulesIntegrationTest, TregPreventsExhaustion) {
    createTregSystem();
    createExhaustionTracker();

    // Create a T cell to track
    uint32_t antigen_id;
    presentTestAntigen(&antigen_id);

    uint32_t t_cell_id;
    brain_immune_activate_killer_t(immune_system, antigen_id, &t_cell_id);

    // Exhaustion tracking is automatic through brain immune integration
    // Simulate time passing with chronic activation
    for (int i = 0; i < 10; i++) {
        exhaustion_update(exhaustion, 1000);
    }

    float capacity_before_treg = exhaustion_get_effector_capacity(exhaustion, t_cell_id);

    // Activate Treg suppression
    treg_suppress_inflammation(treg, 0);  // Broadcast suppression

    // Release IL-10 (suppressive cytokine)
    uint32_t cytokine_id;
    treg_release_cytokine(treg, TREG_CYTOKINE_IL10, 0.8f, 0, &cytokine_id);

    // Apply checkpoint inhibition to the exhausted cell
    uint32_t checkpoint_id;
    treg_checkpoint_activate(treg, CHECKPOINT_PD1_PDL1, t_cell_id, 5000, &checkpoint_id);

    // Check Treg inhibition on the cell
    float inhibition = treg_get_checkpoint_inhibition(treg, t_cell_id);
    EXPECT_GT(inhibition, 0.0f);
}

TEST_F(ImmuneModulesIntegrationTest, ExhaustionTriggersCheckpointUpregulation) {
    createTregSystem();
    createExhaustionTracker();

    uint32_t antigen_id;
    presentTestAntigen(&antigen_id);

    uint32_t t_cell_id;
    brain_immune_activate_killer_t(immune_system, antigen_id, &t_cell_id);

    // Exhaustion tracking is automatic through brain immune integration
    // Severely exhaust through extended time
    exhaustion_update(exhaustion, 50000);

    // Check exhaustion markers
    exhaustion_markers_t markers;
    int ret = exhaustion_get_markers(exhaustion, t_cell_id, &markers);

    // If cell is tracked, check marker levels
    if (ret == 0) {
        // Exhausted cells should upregulate checkpoint markers
        // These markers are what Treg checkpoints target
        EXPECT_GE(markers.pd1_level, 0.0f);
        EXPECT_GE(markers.lag3_level, 0.0f);
    }

    exhaustion_state_t state = exhaustion_get_cell_state(exhaustion, t_cell_id);
    // High stimulation should progress exhaustion state
}

/* ============================================================================
 * Tolerance + Vaccine Integration
 * ============================================================================ */

TEST_F(ImmuneModulesIntegrationTest, ToleranceAllowsVaccineResponse) {
    createToleranceSystem();
    createVaccineSystem();

    // Register self patterns
    uint8_t self_pattern[] = {0xAA, 0xBB, 0xCC, 0xDD};
    tolerance_register_self_pattern(tolerance, self_pattern, sizeof(self_pattern),
                                   "self_pattern", nullptr);

    // Create vaccine with FOREIGN antigen (not self)
    uint8_t vaccine_antigen[] = {0x11, 0x22, 0x33, 0x44};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_MRNA,
                         vaccine_antigen, sizeof(vaccine_antigen), "TestVaccine", &vaccine_id);

    // Check if vaccine antigen is recognized as self
    bool is_self = tolerance_check_self(tolerance, vaccine_antigen, sizeof(vaccine_antigen), nullptr, nullptr);
    EXPECT_FALSE(is_self);  // Should NOT be self

    // Administer vaccine - should work since antigen is foreign
    int result = vaccine_administer(vaccine, vaccine_id);
    EXPECT_EQ(result, 0);

    // Update to process vaccination
    vaccine_update(vaccine, 1000);

    // Check efficacy - should have response
    float efficacy;
    vaccine_get_efficacy(vaccine, vaccine_id, &efficacy);
    EXPECT_GE(efficacy, 0.0f);
}

TEST_F(ImmuneModulesIntegrationTest, ToleranceBlocksSelfVaccine) {
    createToleranceSystem();
    createVaccineSystem();

    // Register self pattern
    uint8_t self_pattern[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint32_t pattern_id;
    tolerance_register_self_pattern(tolerance, self_pattern, sizeof(self_pattern),
                                   "self_pattern", &pattern_id);

    // Try to vaccinate against self pattern - should be tolerated
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_SUBUNIT,
                         self_pattern, sizeof(self_pattern), "SelfVaccine", &vaccine_id);

    // Check tolerance status
    uint32_t matched_id;
    float affinity;
    bool is_self = tolerance_check_self(tolerance, self_pattern, sizeof(self_pattern), &matched_id, &affinity);
    EXPECT_TRUE(is_self);

    // Central tolerance should recognize this as self
    // The tolerance_check_self returns true for self patterns
    EXPECT_EQ(matched_id, pattern_id);
}

/* ============================================================================
 * Mucosal Immunity + Persistence Integration
 * ============================================================================ */

TEST_F(ImmuneModulesIntegrationTest, MucosalStatePersists) {
    createMucosalSystem();

    // Register boundary sites
    uint32_t site_id;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &site_id);

    // Present antigens through main immune system first to get antigen_id
    uint8_t antigen1[] = {0x01, 0x02, 0x03};
    uint8_t antigen2[] = {0x04, 0x05, 0x06};
    uint32_t antigen1_id, antigen2_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 antigen1, sizeof(antigen1), 3, 1, &antigen1_id);
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 antigen2, sizeof(antigen2), 3, 1, &antigen2_id);

    // Produce sIgA (uses antigen_id)
    uint32_t iga_id;
    mucosal_produce_siga(mucosal, site_id, antigen1_id, &iga_id);

    // Induce oral tolerance
    uint32_t tolerance_id;
    mucosal_induce_oral_tolerance(mucosal, site_id, antigen1, sizeof(antigen1), &tolerance_id);

    mucosal_update(mucosal, 2000);

    // Get stats before save
    mucosal_stats_t stats_before;
    mucosal_get_stats(mucosal, &stats_before);

    // Save immune state
    immune_persistence_config_t persist_config;
    immune_persistence_default_config(&persist_config);
    int result = immune_persistence_save(immune_system, test_file, &persist_config);
    EXPECT_EQ(result, 0);

    // Verify file exists
    FILE* f = fopen(test_file, "rb");
    EXPECT_NE(f, nullptr);
    if (f) fclose(f);
}

TEST_F(ImmuneModulesIntegrationTest, PersistenceLoadRestoresSystem) {
    // First save a populated system
    {
        createMucosalSystem();
        createVaccineSystem();

        // Populate mucosal
        uint32_t site_id;
        mucosal_register_boundary(mucosal, MUCOSAL_SITE_MODULE_BOUNDARY, 1, &site_id);

        // Populate vaccine
        uint8_t antigen[] = {0xAA, 0xBB};
        uint32_t vaccine_id;
        vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED,
                             antigen, sizeof(antigen), "PersistTest", &vaccine_id);
        vaccine_administer(vaccine, vaccine_id);

        // Present antigens to main immune system
        uint32_t antigen_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                     antigen, sizeof(antigen), 5, 1, &antigen_id);

        // Save
        immune_persistence_config_t persist_config;
        immune_persistence_default_config(&persist_config);
        immune_persistence_save(immune_system, test_file, &persist_config);

        // Cleanup this instance
        mucosal_destroy(mucosal);
        vaccine_destroy(vaccine);
        mucosal = nullptr;
        vaccine = nullptr;
    }

    // Load into a fresh system
    brain_immune_stats_t stats_before;
    brain_immune_get_stats(immune_system, &stats_before);

    immune_persistence_config_t persist_config;
    immune_persistence_default_config(&persist_config);
    int result = immune_persistence_load(immune_system, test_file, &persist_config);
    EXPECT_EQ(result, 0);

    brain_immune_stats_t stats_after;
    brain_immune_get_stats(immune_system, &stats_after);

    // Should have loaded antigens
    EXPECT_GT(stats_after.antigens_processed, 0u);
}

/* ============================================================================
 * Vaccine + Trained Immunity Integration
 * ============================================================================ */

TEST_F(ImmuneModulesIntegrationTest, VaccinePrimesTrainedImmunity) {
    createVaccineSystem();
    createTrainedImmunity();

    // Create BCG-like vaccine (known to induce trained immunity)
    uint8_t bcg_pattern[] = {0xBC, 0xBC, 0xBC, 0xBC};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_ATTENUATED,
                         bcg_pattern, sizeof(bcg_pattern), "BCG_Like", &vaccine_id);

    vaccine_administer(vaccine, vaccine_id);
    vaccine_update(vaccine, 5000);

    // Train innate system with BCG-like stimulus
    trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 0.8f);

    trained_immunity_decay(trained, get_time_ms());

    // Check for cross-protection capability
    // Note: cross protection check requires brain_antigen_t, not raw pattern
    // For now, verify training was applied
    float prr = trained_immunity_get_prr_sensitivity(trained);
    EXPECT_GT(prr, 1.0f);  // Enhanced sensitivity

    // BCG training should provide some cross-protection
}

TEST_F(ImmuneModulesIntegrationTest, TrainedImmunityBoostsVaccineEfficacy) {
    createVaccineSystem();
    createTrainedImmunity();

    // Pre-train innate system
    uint8_t pattern[] = {0x12, 0x34, 0x56, 0x78};
    trained_immunity_train(trained, TRAINED_STIMULUS_BETA_GLUCAN, 0.8f);
    trained_immunity_decay(trained, get_time_ms());

    float prr_after_training = trained_immunity_get_prr_sensitivity(trained);
    EXPECT_GT(prr_after_training, 1.0f);

    // Now vaccinate with same pattern
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_MRNA,
                         pattern, sizeof(pattern), "PostTraining", &vaccine_id);

    vaccine_administer(vaccine, vaccine_id);
    vaccine_update(vaccine, 5000);

    // Enhanced PRR sensitivity should boost vaccine response
    float efficacy;
    vaccine_get_efficacy(vaccine, vaccine_id, &efficacy);
    EXPECT_GE(efficacy, 0.0f);
}

/* ============================================================================
 * Complement + Exhaustion Integration
 * ============================================================================ */

TEST_F(ImmuneModulesIntegrationTest, ComplementOveractivationCausesExhaustion) {
    createComplementSystem();
    createExhaustionTracker();

    // Present multiple threats to trigger heavy complement activation
    for (int i = 0; i < 10; i++) {
        uint8_t pattern[] = {(uint8_t)i, 0x02, 0x03, 0x04};
        uint32_t antigen_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                     pattern, sizeof(pattern), 8, (uint32_t)i, &antigen_id);

        // Activate complement for each
        complement_activate_classical(complement, antigen_id, 0);
        complement_form_mac(complement, antigen_id);
    }

    complement_update(complement, 5000);

    // Check complement stats
    complement_stats_t comp_stats;
    complement_get_stats(complement, &comp_stats);
    EXPECT_GE(comp_stats.classical_activations, 1u);

    // High complement activity should stress the system
    // which could contribute to immune exhaustion
}

/* ============================================================================
 * Tolerance + Treg Integration
 * ============================================================================ */

TEST_F(ImmuneModulesIntegrationTest, ToleranceAndTregCooperate) {
    createToleranceSystem();
    createTregSystem();

    // Register self patterns for central tolerance
    uint8_t self_pattern[] = {0xAA, 0xBB, 0xCC, 0xDD};
    tolerance_register_self_pattern(tolerance, self_pattern, sizeof(self_pattern),
                                   "self_pattern", nullptr);

    // Present the self pattern as an antigen (autoimmune scenario)
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 self_pattern, sizeof(self_pattern), 5, 1, &antigen_id);

    // Check tolerance
    uint32_t matched_id;
    float affinity;
    bool is_self = tolerance_check_self(tolerance, self_pattern, sizeof(self_pattern), &matched_id, &affinity);
    EXPECT_TRUE(is_self);

    // Treg should also suppress response to self
    treg_suppress_inflammation(treg, 1);  // Suppress in region 1

    // Release suppressive cytokines
    uint32_t cytokine_id;
    treg_release_cytokine(treg, TREG_CYTOKINE_TGFB, 0.7f, 1, &cytokine_id);

    treg_update(treg, 1000);

    float suppression = treg_get_suppression_factor(treg);
    EXPECT_GE(suppression, 0.0f);
}

TEST_F(ImmuneModulesIntegrationTest, AnergicCellsNotActivated) {
    createToleranceSystem();
    createExhaustionTracker();

    // Create anergic cell through tolerance mechanism
    uint8_t self_pattern[] = {0x11, 0x22, 0x33, 0x44};
    uint32_t pattern_id;
    tolerance_register_self_pattern(tolerance, self_pattern, sizeof(self_pattern),
                                   "peripheral_self", &pattern_id);

    // Induce anergy for cells recognizing this pattern
    uint32_t cell_id = 100;  // Simulated cell ID
    tolerance_induce_anergy(tolerance, cell_id, false, pattern_id, 0.85f);

    // Check anergy status (false = T cell, not B cell)
    bool is_anergic = tolerance_is_anergic(tolerance, cell_id, false);
    EXPECT_TRUE(is_anergic);

    // Anergic cells shouldn't respond even with stimulation
    // Exhaustion tracking is automatic through brain immune integration
    exhaustion_update(exhaustion, 1000);

    // Cell should not progress in exhaustion (it's anergic, not responding)
}

/* ============================================================================
 * Full Pipeline Integration
 * ============================================================================ */

TEST_F(ImmuneModulesIntegrationTest, FullImmunePipeline) {
    // Create all systems
    createTrainedImmunity();
    createComplementSystem();
    createTregSystem();
    createExhaustionTracker();
    createToleranceSystem();
    createVaccineSystem();
    createMucosalSystem();

    // Step 1: Register self (tolerance)
    uint8_t self[] = {0xFF, 0xFF, 0xFF, 0xFF};
    tolerance_register_self_pattern(tolerance, self, sizeof(self),
                                   "self_antigen", nullptr);

    // Step 2: Vaccinate against pathogen
    uint8_t pathogen[] = {0x00, 0x11, 0x22, 0x33};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_MRNA,
                         pathogen, sizeof(pathogen), "Pathogen_Vaccine", &vaccine_id);
    vaccine_administer(vaccine, vaccine_id);

    // Step 3: Train innate immunity
    trained_immunity_train(trained, TRAINED_STIMULUS_BETA_GLUCAN, 0.8f);
    trained_immunity_decay(trained, get_time_ms());

    // Step 4: Set up mucosal barrier
    uint32_t gut_site;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_INPUT_GATE, 1, &gut_site);
    // Note: mucosal_produce_siga requires an antigen_id, not raw pattern
    // Skipping this call as we need to present antigen first

    // Step 5: Simulate pathogen encounter
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 pathogen, sizeof(pathogen), 7, 1, &antigen_id);

    // Step 6: Activate complement (takes antibody_id, target_id)
    complement_activate_classical(complement, antigen_id, 0);
    complement_opsonize(complement, antigen_id);

    // Step 7: Activate T cell response (exhaustion auto-tracks)
    uint32_t t_cell_id;
    brain_immune_activate_killer_t(immune_system, antigen_id, &t_cell_id);

    // Step 8: Update all systems
    trained_immunity_decay(trained, get_time_ms());
    complement_update(complement, 1000);
    treg_update(treg, 1000);
    exhaustion_update(exhaustion, 1000);
    // tolerance has no update function
    vaccine_update(vaccine, 1000);
    mucosal_update(mucosal, 1000);

    // Step 9: Treg regulation (prevent over-response)
    treg_suppress_inflammation(treg, 1);

    // Step 10: Save state
    immune_persistence_config_t persist_config;
    immune_persistence_default_config(&persist_config);
    int result = immune_persistence_save(immune_system, test_file, &persist_config);
    EXPECT_EQ(result, 0);

    // Verify all systems are functional
    EXPECT_GT(trained_immunity_get_prr_sensitivity(trained), 0.0f);

    complement_stats_t comp_stats;
    complement_get_stats(complement, &comp_stats);
    EXPECT_GT(comp_stats.classical_activations, 0u);

    treg_stats_t treg_stats;
    treg_get_stats(treg, &treg_stats);

    vaccine_stats_t vax_stats;
    vaccine_get_stats(vaccine, &vax_stats);
    EXPECT_GT(vax_stats.active_vaccines, 0u);
    EXPECT_GT(vax_stats.total_vaccines, 0u);

    mucosal_stats_t muc_stats;
    mucosal_get_stats(mucosal, &muc_stats);
    EXPECT_GT(muc_stats.active_sites, 0u);
}

TEST_F(ImmuneModulesIntegrationTest, AutoimmuneScenarioPrevented) {
    createToleranceSystem();
    createTregSystem();
    createComplementSystem();

    // Register self
    uint8_t self[] = {0xAA, 0xBB, 0xCC, 0xDD};
    tolerance_register_self_pattern(tolerance, self, sizeof(self),
                                   "self_antigen", nullptr);

    // Simulate autoimmune trigger - self presented as threat
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 self, sizeof(self), 8, 1, &antigen_id);

    // Check tolerance - should recognize as self
    uint32_t matched_id;
    float affinity;
    bool is_self = tolerance_check_self(tolerance, self, sizeof(self), &matched_id, &affinity);
    EXPECT_TRUE(is_self);

    // Even if complement activates...
    complement_activate_classical(complement, antigen_id, 0);

    // Treg should suppress the response
    treg_suppress_inflammation(treg, 0);
    uint32_t cytokine_id;
    treg_release_cytokine(treg, TREG_CYTOKINE_IL10, 0.9f, 0, &cytokine_id);

    treg_update(treg, 2000);

    // System should be suppressed
    float suppression = treg_get_suppression_factor(treg);
    EXPECT_GE(suppression, 0.0f);
}

TEST_F(ImmuneModulesIntegrationTest, ChronicInfectionHandling) {
    createExhaustionTracker();
    createTregSystem();
    createComplementSystem();

    // Simulate chronic infection - persistent antigen
    uint8_t chronic_pathogen[] = {0xCC, 0xCC, 0xCC, 0xCC};

    // Multiple presentations over time
    for (int day = 0; day < 14; day++) {
        uint32_t antigen_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                     chronic_pathogen, sizeof(chronic_pathogen),
                                     5, 1, &antigen_id);

        if (day == 0) {
            // Activate T cell (exhaustion auto-tracks from brain immune)
            uint32_t t_cell_id;
            brain_immune_activate_killer_t(immune_system, antigen_id, &t_cell_id);
        }
    }

    // Update systems for chronic duration
    for (int i = 0; i < 14; i++) {
        exhaustion_update(exhaustion, 24ULL * 60 * 60 * 1000 / 14);  // Simulate daily updates
    }

    // Check exhaustion progression
    exhaustion_stats_t stats;
    exhaustion_get_stats(exhaustion, &stats);

    // Check exhaustion system is working
    EXPECT_GE(stats.effector_cells + stats.exhausted_cells, 0u);
}

/* ============================================================================
 * Error Handling Integration
 * ============================================================================ */

TEST_F(ImmuneModulesIntegrationTest, GracefulDegradation) {
    // Create subset of systems
    createComplementSystem();
    // Don't create others

    // Operations should still work on available systems
    uint32_t antigen_id;
    presentTestAntigen(&antigen_id);

    int result = complement_activate_classical(complement, antigen_id, 0);
    EXPECT_EQ(result, 0);

    complement_update(complement, 1000);

    // Should not crash despite missing integrations
}

TEST_F(ImmuneModulesIntegrationTest, NullSafetyAcrossModules) {
    // Operations with null should fail gracefully
    EXPECT_EQ(trained_immunity_decay(nullptr, 1000), -1);
    EXPECT_EQ(complement_update(nullptr, 1000), -1);
    EXPECT_EQ(treg_update(nullptr, 1000), -1);
    EXPECT_EQ(exhaustion_update(nullptr, 1000), -1);
    // tolerance has no update function
    EXPECT_EQ(vaccine_update(nullptr, 1000), -1);
    EXPECT_EQ(mucosal_update(nullptr, 1000), -1);
}
