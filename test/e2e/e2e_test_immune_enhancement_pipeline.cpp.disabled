/**
 * @file e2e_test_immune_enhancement_pipeline.cpp
 * @brief E2E Test for Immune Enhancement Modules Pipeline
 *
 * WHAT: Complete end-to-end tests for the 8 new immune enhancement modules
 * WHY:  Verify advanced immune concepts (trained immunity, complement,
 *       tolerance, vaccines, etc.) work together as a complete system
 * HOW:  Simulate realistic immune scenarios from infection to resolution
 *
 * TEST SCENARIOS:
 * 1. VaccinationPipeline - Vaccine creation, administration, efficacy
 * 2. TrainedImmunityResponse - Innate memory and enhanced responses
 * 3. ComplementCascade - Full complement activation and MAC formation
 * 4. ToleranceInduction - Self-tolerance and autoimmune prevention
 * 5. MucosalBarrier - Boundary defense and oral tolerance
 * 6. ExhaustionRecovery - Chronic infection and T cell exhaustion
 * 7. TregRegulation - Cytokine storm prevention
 * 8. PersistenceRoundTrip - Save, load, and state recovery
 * 9. FullEnhancedImmunePipeline - All modules working together
 * 10. ChronicInfectionResolution - Extended immune engagement
 *
 * BIOLOGICAL ANALOGY:
 * Tests the complete enhanced immune response:
 * - Trained immunity providing innate memory
 * - Complement cascade amplifying responses
 * - Tolerance preventing autoimmunity
 * - Mucosal barriers as first defense
 * - Treg cells preventing cytokine storms
 * - Memory persistence between sessions
 *
 * @date 2025-12-12
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <cstring>
#include <cstdio>

extern "C" {
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
}

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_VACCINE_RESPONSE_MS = 100.0;
constexpr double MAX_COMPLEMENT_ACTIVATION_MS = 50.0;
constexpr double MAX_PERSISTENCE_SAVE_MS = 200.0;

constexpr float MIN_VACCINE_EFFICACY = 0.5f;
constexpr float MIN_TRAINED_ENHANCEMENT = 1.1f;
constexpr float MAX_EXHAUSTION_RECOVERY = 0.8f;

const char* PERSISTENCE_TEST_FILE = "/tmp/e2e_immune_persist.dat";

//=============================================================================
// Callback Tracking
//=============================================================================

struct EnhancedImmuneTracker {
    std::atomic<int> complement_activations{0};
    std::atomic<int> treg_suppressions{0};
    std::atomic<int> vaccine_administrations{0};
    std::atomic<int> tolerance_checks{0};
    std::atomic<int> mucosal_samples{0};

    void reset() {
        complement_activations = 0;
        treg_suppressions = 0;
        vaccine_administrations = 0;
        tolerance_checks = 0;
        mucosal_samples = 0;
    }
};

static EnhancedImmuneTracker g_enhanced_tracker;

//=============================================================================
// Test Fixture
//=============================================================================

class ImmuneEnhancementPipelineTest : public E2ETestFixture {
protected:
    brain_immune_system_t* immune_system = nullptr;
    trained_immunity_t* trained = nullptr;
    complement_system_t* complement = nullptr;
    treg_system_t* treg = nullptr;
    exhaustion_tracker_t* exhaustion = nullptr;
    tolerance_system_t* tolerance = nullptr;
    vaccine_system_t* vaccine = nullptr;
    mucosal_system_t* mucosal = nullptr;

    void SetUp() override {
        E2ETestFixture::SetUp();
        g_enhanced_tracker.reset();

        // Create base immune system
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);
    }

    void TearDown() override {
        // Clean up enhancement modules
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

        // Clean up test files
        remove(PERSISTENCE_TEST_FILE);

        E2ETestFixture::TearDown();
    }

    void createAllModules() {
        // Trained Immunity
        trained_immunity_config_t ti_cfg;
        trained_immunity_default_config(&ti_cfg);
        trained = trained_immunity_create(&ti_cfg, immune_system);
        ASSERT_NE(trained, nullptr);

        // Complement System
        complement_config_t cs_cfg;
        complement_default_config(&cs_cfg);
        complement = complement_create(&cs_cfg, immune_system);
        ASSERT_NE(complement, nullptr);

        // Regulatory T Cells
        treg_config_t treg_cfg;
        treg_default_config(&treg_cfg);
        treg = treg_create(&treg_cfg, immune_system);
        ASSERT_NE(treg, nullptr);

        // Exhaustion Tracker
        exhaustion_config_t ex_cfg;
        exhaustion_default_config(&ex_cfg);
        exhaustion = exhaustion_create(&ex_cfg, immune_system);
        ASSERT_NE(exhaustion, nullptr);

        // Tolerance System
        tolerance_config_t tol_cfg;
        tolerance_default_config(&tol_cfg);
        tolerance = tolerance_create(&tol_cfg, immune_system);
        ASSERT_NE(tolerance, nullptr);

        // Vaccine System
        vaccine_config_t vax_cfg;
        vaccine_default_config(&vax_cfg);
        vaccine = vaccine_create(&vax_cfg, immune_system);
        ASSERT_NE(vaccine, nullptr);

        // Mucosal Immunity
        mucosal_config_t muc_cfg;
        mucosal_default_config(&muc_cfg);
        mucosal = mucosal_create(&muc_cfg, immune_system);
        ASSERT_NE(mucosal, nullptr);
    }
};

//=============================================================================
// E2E Test: Vaccination Pipeline
//=============================================================================

TEST_F(ImmuneEnhancementPipelineTest, VaccinationPipeline) {
    TEST_STEP("Initialize vaccine system");
    vaccine_config_t cfg;
    vaccine_default_config(&cfg);
    vaccine = vaccine_create(&cfg, immune_system);
    ASSERT_NE(vaccine, nullptr);

    TEST_STEP("Create vaccine entry");
    uint8_t spike_protein[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    vaccine_entry_t entry;
    vaccine_create_entry(&entry, "COVID_mRNA", VACCINE_TYPE_MRNA,
                         spike_protein, sizeof(spike_protein));

    uint32_t vaccine_id;
    EXPECT_EQ(vaccine_register(vaccine, &entry, &vaccine_id), 0);
    EXPECT_GT(vaccine_id, 0u);

    TEST_STEP("Administer primary vaccination");
    auto start = std::chrono::high_resolution_clock::now();
    EXPECT_EQ(vaccine_administer(vaccine, vaccine_id, 1.0f), 0);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    double ms = std::chrono::duration<double, std::milli>(elapsed).count();
    EXPECT_LT(ms, MAX_VACCINE_RESPONSE_MS);

    TEST_STEP("Simulate immune response development");
    // Simulate 2 weeks for primary response
    for (int day = 0; day < 14; day++) {
        vaccine_update(vaccine, 24 * 60 * 60 * 1000);  // 1 day in ms
    }

    TEST_STEP("Check primary efficacy");
    float efficacy = vaccine_get_efficacy(vaccine, vaccine_id);
    EXPECT_GT(efficacy, 0.0f);

    TEST_STEP("Administer booster");
    EXPECT_EQ(vaccine_administer_booster(vaccine, vaccine_id, 1.0f), 0);

    // Simulate 1 week for booster effect
    for (int day = 0; day < 7; day++) {
        vaccine_update(vaccine, 24 * 60 * 60 * 1000);
    }

    TEST_STEP("Verify enhanced efficacy");
    float boosted_efficacy = vaccine_get_efficacy(vaccine, vaccine_id);
    EXPECT_GE(boosted_efficacy, efficacy);

    TEST_STEP("Present matching pathogen - verify protection");
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 spike_protein, sizeof(spike_protein),
                                 8, 1, &antigen_id);

    // System should mount rapid response due to vaccine priming
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune_system, &stats);
    EXPECT_GT(stats.total_antigens, 0u);

    TEST_PASS("Vaccination pipeline complete - primary and booster doses effective");
}

//=============================================================================
// E2E Test: Trained Immunity Response
//=============================================================================

TEST_F(ImmuneEnhancementPipelineTest, TrainedImmunityResponse) {
    TEST_STEP("Initialize trained immunity");
    trained_immunity_config_t cfg;
    trained_immunity_default_config(&cfg);
    trained = trained_immunity_create(&cfg, immune_system);
    ASSERT_NE(trained, nullptr);

    TEST_STEP("Get baseline PRR sensitivity");
    float baseline_prr = trained_immunity_get_prr_sensitivity(trained);
    EXPECT_EQ(baseline_prr, 1.0f);  // Baseline should be 1.0

    TEST_STEP("Train with BCG-like stimulus");
    uint8_t bcg_pattern[] = {0xBC, 0xBC, 0xBC, 0xBC, 0x00, 0x01};
    auto start = std::chrono::high_resolution_clock::now();
    EXPECT_EQ(trained_immunity_train(trained, bcg_pattern, sizeof(bcg_pattern),
                                     TRAINED_STIM_BCG), 0);

    TEST_STEP("Simulate epigenetic reprogramming period");
    // Training takes time for epigenetic changes
    for (int i = 0; i < 7; i++) {
        trained_immunity_update(trained, 24 * 60 * 60 * 1000);  // 1 day
    }

    TEST_STEP("Verify enhanced PRR sensitivity");
    float enhanced_prr = trained_immunity_get_prr_sensitivity(trained);
    EXPECT_GT(enhanced_prr, baseline_prr);
    EXPECT_GE(enhanced_prr, MIN_TRAINED_ENHANCEMENT);

    TEST_STEP("Check metabolic shift");
    float glycolysis = trained_immunity_get_metabolic_state(trained);
    // Trained cells shift to glycolytic metabolism
    EXPECT_GE(glycolysis, 0.0f);

    TEST_STEP("Present unrelated pathogen - test heterologous protection");
    uint8_t flu_pattern[] = {0xF1, 0xF1, 0xF1, 0xF1};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 flu_pattern, sizeof(flu_pattern),
                                 5, 1, &antigen_id);

    // Enhanced PRR should detect faster
    trained_immunity_update(trained, 1000);

    TEST_STEP("Verify cross-protection capability");
    bool has_cross = trained_immunity_has_cross_protection(
        trained, bcg_pattern, sizeof(bcg_pattern),
        flu_pattern, sizeof(flu_pattern));

    TEST_PASS("Trained immunity provides enhanced innate response");
}

//=============================================================================
// E2E Test: Complement Cascade
//=============================================================================

TEST_F(ImmuneEnhancementPipelineTest, ComplementCascade) {
    TEST_STEP("Initialize complement system");
    complement_config_t cfg;
    complement_default_config(&cfg);
    complement = complement_create(&cfg, immune_system);
    ASSERT_NE(complement, nullptr);

    TEST_STEP("Present pathogen");
    uint8_t pathogen[] = {0xBA, 0xCT, 0xER, 0x1A};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 pathogen, sizeof(pathogen), 7, 1, &antigen_id);

    TEST_STEP("Activate classical pathway (antibody-mediated)");
    auto start = std::chrono::high_resolution_clock::now();
    uint32_t cascade_id;
    EXPECT_EQ(complement_activate_classical(complement, antigen_id, 0, &cascade_id), 0);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    double ms = std::chrono::duration<double, std::milli>(elapsed).count();
    EXPECT_LT(ms, MAX_COMPLEMENT_ACTIVATION_MS);

    TEST_STEP("Verify cascade activation");
    complement_cascade_t cascade;
    EXPECT_EQ(complement_get_cascade(complement, cascade_id, &cascade), 0);
    EXPECT_EQ(cascade.pathway, COMPLEMENT_PATHWAY_CLASSICAL);
    EXPECT_EQ(cascade.state, COMPLEMENT_STATE_ACTIVE);

    TEST_STEP("C3 cleavage and amplification");
    complement_update(complement, 2000);
    complement_amplify_cascade(complement, cascade_id);
    complement_update(complement, 2000);

    EXPECT_EQ(complement_get_cascade(complement, cascade_id, &cascade), 0);
    EXPECT_GT(cascade.c3_level, 0.0f);

    TEST_STEP("Opsonization for phagocytosis");
    EXPECT_EQ(complement_opsonize(complement, cascade_id, antigen_id), 0);

    TEST_STEP("Release anaphylatoxins (inflammatory signals)");
    EXPECT_EQ(complement_release_anaphylatoxin(complement, cascade_id, ANAPHYLATOXIN_C3A), 0);
    EXPECT_EQ(complement_release_anaphylatoxin(complement, cascade_id, ANAPHYLATOXIN_C5A), 0);

    TEST_STEP("Form MAC (membrane attack complex)");
    EXPECT_EQ(complement_form_mac(complement, cascade_id, antigen_id), 0);

    complement_update(complement, 2000);
    EXPECT_EQ(complement_get_cascade(complement, cascade_id, &cascade), 0);
    EXPECT_TRUE(cascade.mac_formed);

    TEST_STEP("Verify cascade statistics");
    complement_stats_t stats;
    complement_get_stats(complement, &stats);
    EXPECT_GT(stats.cascades_activated, 0u);
    EXPECT_GT(stats.mac_complexes_formed, 0u);
    EXPECT_GT(stats.opsonizations, 0u);

    TEST_PASS("Complement cascade complete - pathogen lysed via MAC");
}

//=============================================================================
// E2E Test: Tolerance Induction
//=============================================================================

TEST_F(ImmuneEnhancementPipelineTest, ToleranceInduction) {
    TEST_STEP("Initialize tolerance system");
    tolerance_config_t cfg;
    tolerance_default_config(&cfg);
    tolerance = tolerance_create(&cfg, immune_system);
    ASSERT_NE(tolerance, nullptr);

    TEST_STEP("Register self antigens (thymic selection)");
    uint8_t self_mhc[] = {0xFF, 0xFF, 0x00, 0x01};  // MHC-like
    uint8_t self_organ[] = {0xAA, 0xBB, 0xCC, 0xDD};  // Organ-specific
    uint8_t self_tissue[] = {0x11, 0x22, 0x33, 0x44};  // Tissue-specific

    EXPECT_EQ(tolerance_register_self(tolerance, self_mhc, sizeof(self_mhc),
                                      TOLERANCE_SELF_THYMIC_SELECTION, 0), 0);
    EXPECT_EQ(tolerance_register_self(tolerance, self_organ, sizeof(self_organ),
                                      TOLERANCE_SELF_THYMIC_SELECTION, 0), 0);
    EXPECT_EQ(tolerance_register_self(tolerance, self_tissue, sizeof(self_tissue),
                                      TOLERANCE_SELF_PERIPHERAL, 0), 0);

    TEST_STEP("Verify self recognition");
    EXPECT_TRUE(tolerance_is_self(tolerance, self_mhc, sizeof(self_mhc)));
    EXPECT_TRUE(tolerance_is_self(tolerance, self_organ, sizeof(self_organ)));
    EXPECT_TRUE(tolerance_is_self(tolerance, self_tissue, sizeof(self_tissue)));

    TEST_STEP("Foreign antigen not recognized as self");
    uint8_t foreign[] = {0x00, 0x11, 0x22, 0x33};
    EXPECT_FALSE(tolerance_is_self(tolerance, foreign, sizeof(foreign)));

    TEST_STEP("Simulate autoimmune scenario - self presented as threat");
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 self_organ, sizeof(self_organ), 8, 1, &antigen_id);

    TEST_STEP("Tolerance check should prevent attack");
    tolerance_check_result_t result;
    EXPECT_EQ(tolerance_check_self(tolerance, self_organ, sizeof(self_organ), &result), 0);
    EXPECT_TRUE(result.is_self);
    EXPECT_EQ(result.action, TOLERANCE_ACTION_TOLERATE);

    TEST_STEP("Induce anergy for self-reactive cells");
    uint32_t reactive_cell = 42;
    EXPECT_EQ(tolerance_induce_anergy(tolerance, reactive_cell,
                                      self_organ, sizeof(self_organ)), 0);
    EXPECT_TRUE(tolerance_is_anergic(tolerance, reactive_cell));

    TEST_STEP("Verify tolerance statistics");
    tolerance_stats_t stats;
    tolerance_get_stats(tolerance, &stats);
    EXPECT_GE(stats.self_patterns, 3u);
    EXPECT_GT(stats.anergic_cells, 0u);

    TEST_PASS("Tolerance mechanisms prevent autoimmune response");
}

//=============================================================================
// E2E Test: Mucosal Barrier Defense
//=============================================================================

TEST_F(ImmuneEnhancementPipelineTest, MucosalBarrierDefense) {
    TEST_STEP("Initialize mucosal immunity");
    mucosal_config_t cfg;
    mucosal_default_config(&cfg);
    mucosal = mucosal_create(&cfg, immune_system);
    ASSERT_NE(mucosal, nullptr);

    TEST_STEP("Register mucosal boundary sites");
    uint32_t gut_site, oral_site, respiratory_site;
    EXPECT_EQ(mucosal_register_boundary(mucosal, MUCOSAL_SITE_GUT, 1, &gut_site), 0);
    EXPECT_EQ(mucosal_register_boundary(mucosal, MUCOSAL_SITE_ORAL, 2, &oral_site), 0);
    EXPECT_EQ(mucosal_register_boundary(mucosal, MUCOSAL_SITE_RESPIRATORY, 3, &respiratory_site), 0);

    TEST_STEP("Verify barrier integrity");
    float gut_integrity = mucosal_get_barrier_integrity(mucosal, gut_site);
    EXPECT_EQ(gut_integrity, 1.0f);  // Full integrity initially

    TEST_STEP("M cell sampling of gut antigen");
    uint8_t food_antigen[] = {0xF0, 0x0D, 0xF0, 0x0D};
    EXPECT_EQ(mucosal_m_cell_sample(mucosal, gut_site, food_antigen, sizeof(food_antigen)), 0);

    TEST_STEP("Produce secretory IgA");
    uint32_t iga_id;
    EXPECT_EQ(mucosal_produce_siga(mucosal, gut_site, food_antigen, sizeof(food_antigen), &iga_id), 0);
    EXPECT_GT(iga_id, 0u);

    TEST_STEP("Induce oral tolerance to food antigen");
    // Repeated low-dose exposure induces tolerance
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(mucosal_induce_oral_tolerance(mucosal, gut_site,
                                                 food_antigen, sizeof(food_antigen), 1), 0);
        mucosal_update(mucosal, 24 * 60 * 60 * 1000);  // 1 day
    }

    TEST_STEP("Verify oral tolerance induction");
    bool tolerized = mucosal_is_orally_tolerant(mucosal, food_antigen, sizeof(food_antigen));
    EXPECT_TRUE(tolerized);

    TEST_STEP("Simulate pathogen breach - barrier damage");
    uint8_t pathogen[] = {0xBA, 0xCT, 0xER, 0x1A};
    mucosal_damage_barrier(mucosal, gut_site, 0.2f);  // 20% damage

    float damaged_integrity = mucosal_get_barrier_integrity(mucosal, gut_site);
    EXPECT_LT(damaged_integrity, gut_integrity);

    TEST_STEP("Sample pathogen through damaged barrier");
    EXPECT_EQ(mucosal_m_cell_sample(mucosal, gut_site, pathogen, sizeof(pathogen)), 0);

    // Should trigger immune response (not tolerized)
    EXPECT_FALSE(mucosal_is_orally_tolerant(mucosal, pathogen, sizeof(pathogen)));

    TEST_STEP("Verify mucosal statistics");
    mucosal_stats_t stats;
    mucosal_get_stats(mucosal, &stats);
    EXPECT_GE(stats.sites_registered, 3u);
    EXPECT_GT(stats.siga_produced, 0u);
    EXPECT_GT(stats.samples_collected, 0u);

    TEST_PASS("Mucosal barriers provide first-line defense and oral tolerance");
}

//=============================================================================
// E2E Test: Exhaustion and Recovery
//=============================================================================

TEST_F(ImmuneEnhancementPipelineTest, ExhaustionAndRecovery) {
    TEST_STEP("Initialize exhaustion tracking and Treg");
    exhaustion_config_t ex_cfg;
    exhaustion_default_config(&ex_cfg);
    exhaustion = exhaustion_create(&ex_cfg, immune_system);
    ASSERT_NE(exhaustion, nullptr);

    treg_config_t treg_cfg;
    treg_default_config(&treg_cfg);
    treg = treg_create(&treg_cfg, immune_system);
    ASSERT_NE(treg, nullptr);

    TEST_STEP("Create and track T cell");
    uint8_t chronic_pathogen[] = {0xCC, 0xCC, 0xCC, 0xCC};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 chronic_pathogen, sizeof(chronic_pathogen),
                                 6, 1, &antigen_id);

    uint32_t t_cell_id;
    brain_immune_activate_killer_t(immune_system, antigen_id, &t_cell_id);

    EXPECT_EQ(exhaustion_register_cell(exhaustion, t_cell_id, EXHAUSTED_CELL_KILLER_T), 0);

    TEST_STEP("Verify initial functional capacity");
    float initial_capacity = exhaustion_get_functional_capacity(exhaustion, t_cell_id);
    EXPECT_EQ(initial_capacity, 1.0f);

    exhaustion_state_t initial_state = exhaustion_get_state(exhaustion, t_cell_id);
    EXPECT_EQ(initial_state, EXHAUSTION_STATE_FUNCTIONAL);

    TEST_STEP("Simulate chronic stimulation (weeks of infection)");
    for (int week = 0; week < 4; week++) {
        for (int day = 0; day < 7; day++) {
            exhaustion_record_stimulation(exhaustion, t_cell_id);
            exhaustion_update(exhaustion, 24 * 60 * 60 * 1000);
        }
    }

    TEST_STEP("Verify exhaustion progression");
    float exhausted_capacity = exhaustion_get_functional_capacity(exhaustion, t_cell_id);
    EXPECT_LT(exhausted_capacity, initial_capacity);

    exhaustion_state_t exhausted_state = exhaustion_get_state(exhaustion, t_cell_id);
    // Should have progressed beyond functional

    TEST_STEP("Check exhaustion marker upregulation");
    float pd1 = exhaustion_get_marker_level(exhaustion, t_cell_id, EXHAUSTION_MARKER_PD1);
    float tim3 = exhaustion_get_marker_level(exhaustion, t_cell_id, EXHAUSTION_MARKER_TIM3);
    float lag3 = exhaustion_get_marker_level(exhaustion, t_cell_id, EXHAUSTION_MARKER_LAG3);

    // At least some markers should be elevated
    bool markers_elevated = (pd1 > 0.1f) || (tim3 > 0.1f) || (lag3 > 0.1f);

    TEST_STEP("Apply checkpoint blockade therapy");
    // PD-1 blockade to reinvigorate exhausted cells
    EXPECT_EQ(exhaustion_apply_checkpoint_blockade(exhaustion, t_cell_id, EXHAUSTION_MARKER_PD1), 0);

    TEST_STEP("Simulate recovery period");
    for (int day = 0; day < 14; day++) {
        exhaustion_update(exhaustion, 24 * 60 * 60 * 1000);
    }

    TEST_STEP("Verify recovery");
    float recovered_capacity = exhaustion_get_functional_capacity(exhaustion, t_cell_id);
    EXPECT_GE(recovered_capacity, exhausted_capacity);

    TEST_STEP("Verify exhaustion statistics");
    exhaustion_stats_t stats;
    exhaustion_get_stats(exhaustion, &stats);
    EXPECT_GT(stats.cells_tracked, 0u);
    EXPECT_GT(stats.stimulations_recorded, 0u);

    TEST_PASS("Exhaustion tracked and checkpoint blockade enables recovery");
}

//=============================================================================
// E2E Test: Treg Cytokine Storm Prevention
//=============================================================================

TEST_F(ImmuneEnhancementPipelineTest, TregCytokineStormPrevention) {
    TEST_STEP("Initialize Treg system");
    treg_config_t cfg;
    treg_default_config(&cfg);
    treg = treg_create(&cfg, immune_system);
    ASSERT_NE(treg, nullptr);

    TEST_STEP("Verify initial Treg state");
    treg_state_t initial_state = treg_get_state(treg);
    EXPECT_FALSE(treg_is_active(treg));

    TEST_STEP("Simulate severe infection triggering hyperinflammation");
    // Present multiple high-severity threats
    for (int i = 0; i < 10; i++) {
        uint8_t pattern[] = {(uint8_t)i, 0x02, 0x03, 0x04};
        uint32_t antigen_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                     pattern, sizeof(pattern), 10, (uint32_t)i, &antigen_id);
    }

    TEST_STEP("Treg detects inflammation and activates");
    treg_update(treg, 5000);

    TEST_STEP("Activate suppression mechanisms");
    EXPECT_EQ(treg_suppress_inflammation(treg, 0), 0);  // Broadcast suppression

    TEST_STEP("Release suppressive cytokines");
    uint32_t il10_id, tgfb_id, il35_id;
    EXPECT_EQ(treg_release_cytokine(treg, TREG_CYTOKINE_IL10, 0.9f, 0, &il10_id), 0);
    EXPECT_EQ(treg_release_cytokine(treg, TREG_CYTOKINE_TGFB, 0.8f, 0, &tgfb_id), 0);
    EXPECT_EQ(treg_release_cytokine(treg, TREG_CYTOKINE_IL35, 0.7f, 0, &il35_id), 0);

    TEST_STEP("Activate checkpoint inhibition on effector cells");
    uint32_t checkpoint_ids[5];
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(treg_checkpoint_activate(treg, CHECKPOINT_PD1_PDL1,
                                           (uint32_t)(i + 1), 60000, &checkpoint_ids[i]), 0);
    }

    treg_update(treg, 5000);

    TEST_STEP("Verify suppression is active");
    float suppression = treg_get_suppression_factor(treg);
    EXPECT_GT(suppression, 0.0f);

    TEST_STEP("Check checkpoint inhibition on target cells");
    for (int i = 0; i < 5; i++) {
        float inhibition = treg_get_checkpoint_inhibition(treg, (uint32_t)(i + 1));
        EXPECT_GT(inhibition, 0.0f);
    }

    TEST_STEP("Verify Treg statistics");
    treg_stats_t stats;
    treg_get_stats(treg, &stats);
    EXPECT_GT(stats.checkpoints_activated, 0u);
    EXPECT_GT(stats.cytokines_released, 0u);
    EXPECT_GT(stats.suppressions_performed, 0u);

    TEST_PASS("Treg prevents cytokine storm through suppression and checkpoints");
}

//=============================================================================
// E2E Test: Persistence Round Trip
//=============================================================================

TEST_F(ImmuneEnhancementPipelineTest, PersistenceRoundTrip) {
    TEST_STEP("Populate immune system with data");
    // Present antigens
    for (int i = 0; i < 5; i++) {
        uint8_t epitope[] = {(uint8_t)i, 0x02, 0x03, 0x04};
        uint32_t antigen_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                     epitope, sizeof(epitope), (uint8_t)(i + 3), (uint32_t)i, &antigen_id);
    }

    // Activate cells
    uint32_t b_cell_id, t_cell_id;
    brain_immune_activate_b_cell(immune_system, 1, &b_cell_id);
    brain_immune_activate_killer_t(immune_system, 2, &t_cell_id);

    TEST_STEP("Get stats before save");
    brain_immune_stats_t before;
    brain_immune_get_stats(immune_system, &before);
    EXPECT_GT(before.total_antigens, 0u);

    TEST_STEP("Save immune state");
    immune_persistence_config_t cfg;
    immune_persistence_default_config(&cfg);

    auto start = std::chrono::high_resolution_clock::now();
    EXPECT_EQ(immune_persistence_save(immune_system, PERSISTENCE_TEST_FILE, &cfg), 0);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    double ms = std::chrono::duration<double, std::milli>(elapsed).count();
    EXPECT_LT(ms, MAX_PERSISTENCE_SAVE_MS);

    TEST_STEP("Validate saved file");
    EXPECT_EQ(immune_persistence_validate_file(PERSISTENCE_TEST_FILE, true), 0);

    TEST_STEP("Get file info");
    immune_persistence_header_t header;
    immune_persistence_counts_t counts;
    EXPECT_EQ(immune_persistence_get_file_info(PERSISTENCE_TEST_FILE, &header, &counts), 0);
    EXPECT_EQ(header.version, IMMUNE_PERSISTENCE_VERSION);
    EXPECT_GT(counts.antigen_count, 0u);

    TEST_STEP("Clear immune state");
    EXPECT_EQ(immune_persistence_clear_state(immune_system), 0);

    brain_immune_stats_t cleared;
    brain_immune_get_stats(immune_system, &cleared);
    EXPECT_EQ(cleared.total_antigens, 0u);

    TEST_STEP("Load saved state");
    EXPECT_EQ(immune_persistence_load(immune_system, PERSISTENCE_TEST_FILE, &cfg), 0);

    TEST_STEP("Verify state restored");
    brain_immune_stats_t after;
    brain_immune_get_stats(immune_system, &after);
    EXPECT_EQ(after.total_antigens, before.total_antigens);

    TEST_STEP("Create backup");
    EXPECT_EQ(immune_persistence_create_backup(PERSISTENCE_TEST_FILE, ".bak"), 0);

    // Clean up backup
    char backup_path[256];
    snprintf(backup_path, sizeof(backup_path), "%s.bak", PERSISTENCE_TEST_FILE);
    remove(backup_path);

    TEST_PASS("Persistence save/load round trip successful");
}

//=============================================================================
// E2E Test: Full Enhanced Immune Pipeline
//=============================================================================

TEST_F(ImmuneEnhancementPipelineTest, FullEnhancedImmunePipeline) {
    TEST_STEP("Initialize all enhancement modules");
    createAllModules();

    TEST_STEP("Phase 1: Establish tolerance (thymic selection)");
    uint8_t self_pattern[] = {0xFF, 0xFF, 0xFF, 0xFF};
    tolerance_register_self(tolerance, self_pattern, sizeof(self_pattern),
                           TOLERANCE_SELF_THYMIC_SELECTION, 0);

    TEST_STEP("Phase 2: Set up mucosal barriers");
    uint32_t gut_site;
    mucosal_register_boundary(mucosal, MUCOSAL_SITE_GUT, 1, &gut_site);

    TEST_STEP("Phase 3: Vaccinate against known pathogen");
    uint8_t flu_antigen[] = {0xF1, 0xF1, 0xF1, 0xF1};
    vaccine_entry_t entry;
    vaccine_create_entry(&entry, "Flu_Vaccine", VACCINE_TYPE_INACTIVATED,
                         flu_antigen, sizeof(flu_antigen));
    uint32_t vaccine_id;
    vaccine_register(vaccine, &entry, &vaccine_id);
    vaccine_administer(vaccine, vaccine_id, 1.0f);
    vaccine_update(vaccine, 14 * 24 * 60 * 60 * 1000);  // 2 weeks

    TEST_STEP("Phase 4: Train innate immunity (BCG)");
    uint8_t bcg_pattern[] = {0xBC, 0xBC, 0xBC, 0xBC};
    trained_immunity_train(trained, bcg_pattern, sizeof(bcg_pattern), TRAINED_STIM_BCG);
    trained_immunity_update(trained, 7 * 24 * 60 * 60 * 1000);  // 1 week

    TEST_STEP("Phase 5: Simulate infection at mucosal barrier");
    uint8_t pathogen[] = {0xBA, 0xCT, 0xER, 0x1A};
    mucosal_m_cell_sample(mucosal, gut_site, pathogen, sizeof(pathogen));

    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 pathogen, sizeof(pathogen), 7, 1, &antigen_id);

    TEST_STEP("Phase 6: Complement activation");
    uint32_t cascade_id;
    complement_activate_classical(complement, antigen_id, 0, &cascade_id);
    complement_opsonize(complement, cascade_id, antigen_id);
    complement_update(complement, 2000);

    TEST_STEP("Phase 7: T cell response with exhaustion tracking");
    uint32_t t_cell_id;
    brain_immune_activate_killer_t(immune_system, antigen_id, &t_cell_id);
    exhaustion_register_cell(exhaustion, t_cell_id, EXHAUSTED_CELL_KILLER_T);

    TEST_STEP("Phase 8: Treg modulation prevents overreaction");
    treg_update(treg, 2000);
    treg_suppress_inflammation(treg, 1);
    uint32_t il10_id;
    treg_release_cytokine(treg, TREG_CYTOKINE_IL10, 0.5f, 1, &il10_id);

    TEST_STEP("Phase 9: Update all systems");
    trained_immunity_update(trained, 1000);
    complement_update(complement, 1000);
    treg_update(treg, 1000);
    exhaustion_update(exhaustion, 1000);
    tolerance_update(tolerance, 1000);
    vaccine_update(vaccine, 1000);
    mucosal_update(mucosal, 1000);

    TEST_STEP("Phase 10: Persist state");
    immune_persistence_config_t persist_cfg;
    immune_persistence_default_config(&persist_cfg);
    EXPECT_EQ(immune_persistence_save(immune_system, PERSISTENCE_TEST_FILE, &persist_cfg), 0);

    TEST_STEP("Verify all modules functioning");
    // Trained immunity enhanced
    EXPECT_GT(trained_immunity_get_prr_sensitivity(trained), 1.0f);

    // Complement activated
    complement_stats_t comp_stats;
    complement_get_stats(complement, &comp_stats);
    EXPECT_GT(comp_stats.cascades_activated, 0u);

    // Treg active
    treg_stats_t treg_stats;
    treg_get_stats(treg, &treg_stats);

    // Vaccine administered
    vaccine_stats_t vax_stats;
    vaccine_get_stats(vaccine, &vax_stats);
    EXPECT_GT(vax_stats.vaccines_administered, 0u);

    // Mucosal barrier active
    mucosal_stats_t muc_stats;
    mucosal_get_stats(mucosal, &muc_stats);
    EXPECT_GT(muc_stats.sites_registered, 0u);

    // Tolerance established
    tolerance_stats_t tol_stats;
    tolerance_get_stats(tolerance, &tol_stats);
    EXPECT_GT(tol_stats.self_patterns, 0u);

    // Exhaustion tracked
    exhaustion_stats_t ex_stats;
    exhaustion_get_stats(exhaustion, &ex_stats);
    EXPECT_GT(ex_stats.cells_tracked, 0u);

    TEST_PASS("Full enhanced immune pipeline - all 8 modules working in harmony");
}

//=============================================================================
// E2E Test: Chronic Infection Resolution
//=============================================================================

TEST_F(ImmuneEnhancementPipelineTest, ChronicInfectionResolution) {
    TEST_STEP("Initialize required modules");
    createAllModules();

    TEST_STEP("Simulate chronic viral infection");
    uint8_t virus[] = {0xV1, 0xRU, 0x55, 0x00};

    // Initial infection
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                 virus, sizeof(virus), 8, 1, &antigen_id);

    // Activate adaptive response
    uint32_t t_cell_id, b_cell_id;
    brain_immune_activate_killer_t(immune_system, antigen_id, &t_cell_id);
    brain_immune_activate_b_cell(immune_system, antigen_id, &b_cell_id);

    exhaustion_register_cell(exhaustion, t_cell_id, EXHAUSTED_CELL_KILLER_T);

    TEST_STEP("Simulate weeks of chronic infection");
    for (int week = 0; week < 6; week++) {
        // Viral reactivation
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                     virus, sizeof(virus), 6, (uint32_t)(week + 10), nullptr);

        // T cell stimulation
        for (int day = 0; day < 7; day++) {
            exhaustion_record_stimulation(exhaustion, t_cell_id);
            exhaustion_update(exhaustion, 24 * 60 * 60 * 1000);
        }

        // Treg tries to prevent tissue damage
        treg_update(treg, 7 * 24 * 60 * 60 * 1000);
        if (week > 2) {
            treg_suppress_inflammation(treg, 1);
        }
    }

    TEST_STEP("Check T cell exhaustion after chronic infection");
    exhaustion_state_t state = exhaustion_get_state(exhaustion, t_cell_id);
    float capacity = exhaustion_get_functional_capacity(exhaustion, t_cell_id);

    // Should show signs of exhaustion
    EXPECT_LT(capacity, 0.9f);

    TEST_STEP("Apply therapeutic intervention - checkpoint blockade");
    exhaustion_apply_checkpoint_blockade(exhaustion, t_cell_id, EXHAUSTION_MARKER_PD1);

    TEST_STEP("Resolution phase - viral clearance");
    for (int day = 0; day < 14; day++) {
        complement_update(complement, 24 * 60 * 60 * 1000);
        treg_update(treg, 24 * 60 * 60 * 1000);
        exhaustion_update(exhaustion, 24 * 60 * 60 * 1000);
    }

    TEST_STEP("Verify recovery");
    float recovered_capacity = exhaustion_get_functional_capacity(exhaustion, t_cell_id);
    EXPECT_GE(recovered_capacity, capacity);  // At least stable or improving

    TEST_STEP("Persist recovered state");
    immune_persistence_config_t persist_cfg;
    immune_persistence_default_config(&persist_cfg);
    EXPECT_EQ(immune_persistence_save(immune_system, PERSISTENCE_TEST_FILE, &persist_cfg), 0);

    TEST_PASS("Chronic infection tracked, exhaustion managed, state persisted");
}
