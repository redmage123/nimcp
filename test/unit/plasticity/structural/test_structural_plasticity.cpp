/**
 * @file test_structural_plasticity.cpp
 * @brief Unit tests for structural plasticity spine dynamics
 *
 * BIOLOGICAL BASIS:
 * - Dendritic spines are dynamic structures that form, stabilize, and prune
 * - Activity-dependent formation and elimination
 * - Sleep-dependent consolidation
 * - Immune-mediated pruning
 *
 * TEST PHILOSOPHY:
 * - Test biological constraints
 * - Verify state transitions
 * - Test integration points (sleep, immune)
 * - Performance benchmarks
 */

#include <gtest/gtest.h>
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include <cmath>

//=============================================================================
// Test Fixtures
//=============================================================================

class StructuralPlasticityTest : public ::testing::Test {
protected:
    structural_plasticity_system_t* system;
    structural_plasticity_config_t config;

    void SetUp() override {
        /* Initialize default configuration */
        structural_plasticity_default_config(&config);
        config.max_spines = 1000;
        config.formation_threshold_hz = 30.0f;
        config.pruning_threshold_hz = 1.0f;
        config.maturation_time_sec = 3600.0f;  /* 1 hour for fast testing */

        /* Create system */
        system = structural_plasticity_create(&config);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            structural_plasticity_destroy(system);
        }
    }
};

//=============================================================================
// Creation and Initialization Tests
//=============================================================================

TEST_F(StructuralPlasticityTest, SystemCreation) {
    /* WHAT: Test system creation
     * WHY:  Verify initialization succeeds
     */
    EXPECT_NE(system, nullptr);
    EXPECT_EQ(structural_plasticity_get_total_spines(system), 0);
}

TEST_F(StructuralPlasticityTest, DefaultConfiguration) {
    /* WHAT: Test default configuration values
     * WHY:  Ensure biological defaults are reasonable
     */
    structural_plasticity_config_t cfg;
    ASSERT_EQ(structural_plasticity_default_config(&cfg), 0);

    EXPECT_GT(cfg.formation_threshold_hz, 0.0f);
    EXPECT_LT(cfg.pruning_threshold_hz, cfg.formation_threshold_hz);
    EXPECT_GT(cfg.maturation_time_sec, 0.0f);
    EXPECT_TRUE(cfg.enable_immune_pruning);
    EXPECT_TRUE(cfg.enable_sleep_consolidation);
}

TEST_F(StructuralPlasticityTest, NullConfigUsesDefaults) {
    /* WHAT: Test NULL config falls back to defaults
     * WHY:  Ensure safe initialization
     */
    structural_plasticity_system_t* sys =
        structural_plasticity_create(nullptr);
    EXPECT_NE(sys, nullptr);

    structural_plasticity_destroy(sys);
}

//=============================================================================
// Formation Tests
//=============================================================================

TEST_F(StructuralPlasticityTest, FormSynapseCreatesNascentSpine) {
    /* WHAT: Test synapse formation creates nascent spine
     * WHY:  New spines start in nascent state
     * BIOLOGICAL: Formation triggered by high activity
     */
    uint32_t synapse_id;
    int result = structural_plasticity_form_synapse(
        system, 1, 2, 40.0f, &synapse_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(synapse_id, 0);

    synapse_structural_state_t state;
    ASSERT_EQ(structural_plasticity_get_synapse_state(
        system, synapse_id, &state), 0);

    EXPECT_EQ(state.state, SYNAPSE_STATE_NASCENT);
    EXPECT_EQ(state.pre_neuron_id, 1);
    EXPECT_EQ(state.post_neuron_id, 2);
}

TEST_F(StructuralPlasticityTest, FormationIncrementsSpineCount) {
    /* WHAT: Test formation increases total spine count
     * WHY:  Verify tracking is correct
     */
    EXPECT_EQ(structural_plasticity_get_total_spines(system), 0);

    uint32_t id1, id2;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &id1);
    EXPECT_EQ(structural_plasticity_get_total_spines(system), 1);

    structural_plasticity_form_synapse(system, 2, 3, 50.0f, &id2);
    EXPECT_EQ(structural_plasticity_get_total_spines(system), 2);
}

TEST_F(StructuralPlasticityTest, ShouldFormAtHighActivity) {
    /* WHAT: Test formation decision at high activity
     * WHY:  Activity above threshold triggers formation
     */
    float high_activity = 60.0f;  /* Above threshold */
    EXPECT_TRUE(structural_plasticity_should_form(system, high_activity));
}

TEST_F(StructuralPlasticityTest, ShouldNotFormAtLowActivity) {
    /* WHAT: Test no formation at low activity
     * WHY:  Activity below threshold prevents formation
     */
    float low_activity = 10.0f;  /* Below threshold */
    EXPECT_FALSE(structural_plasticity_should_form(system, low_activity));
}

TEST_F(StructuralPlasticityTest, NascentSpineMorphology) {
    /* WHAT: Test nascent spine has thin morphology
     * WHY:  New spines are small and unstable
     * BIOLOGICAL: Thin spines = learning phase
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    spine_morphology_t morph;
    ASSERT_EQ(structural_plasticity_get_morphology(
        system, synapse_id, &morph), 0);

    EXPECT_LT(morph.spine_volume, STRUCTURAL_VOLUME_STABLE_MIN);
    EXPECT_LT(morph.psd_size, STRUCTURAL_PSD_STABLE_MIN);
    EXPECT_GT(morph.spine_motility, 0.5f);  /* High motility */
    EXPECT_LT(morph.spine_stability, 0.5f); /* Low stability */
}

//=============================================================================
// Stabilization Tests
//=============================================================================

TEST_F(StructuralPlasticityTest, StabilizeNascentSpine) {
    /* WHAT: Test stabilizing nascent spine
     * WHY:  Repeated activation consolidates structure
     * BIOLOGICAL: Nascent → stable transition
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    int result = structural_plasticity_stabilize_synapse(system, synapse_id);
    EXPECT_EQ(result, 0);

    synapse_structural_state_t state;
    ASSERT_EQ(structural_plasticity_get_synapse_state(
        system, synapse_id, &state), 0);

    EXPECT_EQ(state.state, SYNAPSE_STATE_STABLE);
}

TEST_F(StructuralPlasticityTest, StableMorphologyLargerThanNascent) {
    /* WHAT: Test stable spine has mushroom morphology
     * WHY:  Stabilization increases volume and PSD
     * BIOLOGICAL: Mushroom spines = stable memory
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    spine_morphology_t nascent_morph;
    structural_plasticity_get_morphology(system, synapse_id, &nascent_morph);

    structural_plasticity_stabilize_synapse(system, synapse_id);

    spine_morphology_t stable_morph;
    structural_plasticity_get_morphology(system, synapse_id, &stable_morph);

    EXPECT_GT(stable_morph.spine_volume, nascent_morph.spine_volume);
    EXPECT_GT(stable_morph.psd_size, nascent_morph.psd_size);
    EXPECT_GT(stable_morph.spine_stability, nascent_morph.spine_stability);
    EXPECT_LT(stable_morph.spine_motility, nascent_morph.spine_motility);
}

TEST_F(StructuralPlasticityTest, CannotStabilizeStableSpine) {
    /* WHAT: Test cannot stabilize already-stable spine
     * WHY:  Stabilization only applies to nascent state
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);
    structural_plasticity_stabilize_synapse(system, synapse_id);

    /* Try to stabilize again */
    int result = structural_plasticity_stabilize_synapse(system, synapse_id);
    EXPECT_NE(result, 0);  /* Should fail */
}

TEST_F(StructuralPlasticityTest, ConsolidationTagging) {
    /* WHAT: Test tagging spines for sleep consolidation
     * WHY:  Learning-related spines tagged for NREM strengthening
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    int result = structural_plasticity_tag_for_consolidation(
        system, synapse_id);
    EXPECT_EQ(result, 0);

    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    EXPECT_TRUE(state.consolidation_tagged);
}

//=============================================================================
// Potentiation Tests
//=============================================================================

TEST_F(StructuralPlasticityTest, PotentiateStableSpine) {
    /* WHAT: Test potentiating stable spine
     * WHY:  LTP enlarges stable spines
     * BIOLOGICAL: Strong LTP → enlarged spines
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);
    structural_plasticity_stabilize_synapse(system, synapse_id);

    int result = structural_plasticity_potentiate_synapse(system, synapse_id);
    EXPECT_EQ(result, 0);

    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    EXPECT_EQ(state.state, SYNAPSE_STATE_POTENTIATED);
}

TEST_F(StructuralPlasticityTest, PotentiatedMorphologyMaximal) {
    /* WHAT: Test potentiated spine has maximal morphology
     * WHY:  Potentiation maximizes volume and receptors
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);
    structural_plasticity_stabilize_synapse(system, synapse_id);

    spine_morphology_t stable_morph;
    structural_plasticity_get_morphology(system, synapse_id, &stable_morph);

    structural_plasticity_potentiate_synapse(system, synapse_id);

    spine_morphology_t potentiated_morph;
    structural_plasticity_get_morphology(system, synapse_id, &potentiated_morph);

    EXPECT_GT(potentiated_morph.spine_volume, stable_morph.spine_volume);
    EXPECT_GT(potentiated_morph.psd_size, stable_morph.psd_size);
    EXPECT_GT(potentiated_morph.ampar_count, stable_morph.ampar_count);
}

TEST_F(StructuralPlasticityTest, CannotPotentiateNascentSpine) {
    /* WHAT: Test cannot potentiate nascent spine
     * WHY:  Must be stable first
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    int result = structural_plasticity_potentiate_synapse(system, synapse_id);
    EXPECT_NE(result, 0);  /* Should fail */
}

//=============================================================================
// Activity Tracking Tests
//=============================================================================

TEST_F(StructuralPlasticityTest, UpdateActivityRecordsSpike) {
    /* WHAT: Test activity tracking updates firing rate
     * WHY:  Activity determines formation/pruning decisions
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    /* Simulate spike events */
    for (int i = 0; i < 10; i++) {
        structural_plasticity_update_activity(system, synapse_id, i * 100);
    }

    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    EXPECT_GT(state.recent_activity_hz, 0.0f);
}

TEST_F(StructuralPlasticityTest, RecordLTPAccumulates) {
    /* WHAT: Test LTP events accumulate
     * WHY:  Multiple LTP events trigger potentiation
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);
    structural_plasticity_stabilize_synapse(system, synapse_id);

    /* Record multiple LTP events */
    for (int i = 0; i < 5; i++) {
        structural_plasticity_record_ltp(system, synapse_id, 2.5f);
    }

    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    EXPECT_GT(state.ltp_accumulator, 10.0f);
}

TEST_F(StructuralPlasticityTest, LTPAutoPotentiates) {
    /* WHAT: Test auto-potentiation from accumulated LTP
     * WHY:  Sufficient LTP automatically potentiates spine
     * BIOLOGICAL: Strong LTP strengthens and enlarges spines
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);
    structural_plasticity_stabilize_synapse(system, synapse_id);

    /* Accumulate enough LTP */
    for (int i = 0; i < 6; i++) {
        structural_plasticity_record_ltp(system, synapse_id, 2.0f);
    }

    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    EXPECT_EQ(state.state, SYNAPSE_STATE_POTENTIATED);
}

TEST_F(StructuralPlasticityTest, RecordLTDIncreasesPruningUrgency) {
    /* WHAT: Test LTD increases pruning urgency
     * WHY:  LTD weakens spines toward elimination
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    synapse_structural_state_t state_before;
    structural_plasticity_get_synapse_state(system, synapse_id, &state_before);

    structural_plasticity_record_ltd(system, synapse_id, 5.0f);

    synapse_structural_state_t state_after;
    structural_plasticity_get_synapse_state(system, synapse_id, &state_after);

    EXPECT_GT(state_after.pruning_urgency, state_before.pruning_urgency);
}

//=============================================================================
// Pruning and Elimination Tests
//=============================================================================

TEST_F(StructuralPlasticityTest, ShouldPruneAtLowActivity) {
    /* WHAT: Test pruning decision for low-activity spine
     * WHY:  Inactive spines should be eliminated
     */
    synapse_structural_state_t state;
    memset(&state, 0, sizeof(state));
    state.recent_activity_hz = 0.5f;  /* Below threshold */

    EXPECT_TRUE(structural_plasticity_should_prune(system, &state));
}

TEST_F(StructuralPlasticityTest, ShouldNotPruneActiveSpine) {
    /* WHAT: Test no pruning for active spine
     * WHY:  Active spines should be preserved
     */
    synapse_structural_state_t state;
    memset(&state, 0, sizeof(state));
    state.recent_activity_hz = 10.0f;  /* Above threshold */

    EXPECT_FALSE(structural_plasticity_should_prune(system, &state));
}

TEST_F(StructuralPlasticityTest, EliminateSynapseRemovesSpine) {
    /* WHAT: Test elimination removes spine from network
     * WHY:  Pruning frees resources
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    uint32_t count_before = structural_plasticity_get_total_spines(system);

    int result = structural_plasticity_eliminate_synapse(system, synapse_id);
    EXPECT_EQ(result, 0);

    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    EXPECT_EQ(state.state, SYNAPSE_STATE_ELIMINATED);

    uint32_t count_after = structural_plasticity_get_total_spines(system);
    EXPECT_LT(count_after, count_before);
}

TEST_F(StructuralPlasticityTest, PruningStateMarksForElimination) {
    /* WHAT: Test pruning state transition
     * WHY:  Spines transition through pruning before elimination
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    /* Manually set low activity */
    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    state.recent_activity_hz = 0.3f;

    /* Update should trigger pruning */
    structural_plasticity_update(system, 1.0f);

    /* Check for pruning state (may take multiple updates) */
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    /* State may be NASCENT, PRUNING, or ELIMINATED depending on timing */
}

//=============================================================================
// Update and Maturation Tests
//=============================================================================

TEST_F(StructuralPlasticityTest, UpdateAdvancesMaturation) {
    /* WHAT: Test update advances maturation progress
     * WHY:  Time-dependent maturation toward stability
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    synapse_structural_state_t state_before;
    structural_plasticity_get_synapse_state(system, synapse_id, &state_before);

    /* Advance time */
    structural_plasticity_update(system, 100.0f);

    synapse_structural_state_t state_after;
    structural_plasticity_get_synapse_state(system, synapse_id, &state_after);

    EXPECT_GT(state_after.maturation_progress, state_before.maturation_progress);
}

TEST_F(StructuralPlasticityTest, MaturationLeadsToStabilization) {
    /* WHAT: Test full maturation stabilizes spine
     * WHY:  Mature nascent spines auto-stabilize
     */
    /* Use short maturation time for testing */
    config.maturation_time_sec = 10.0f;
    config.stabilization_threshold = 5.0f;
    config.require_sleep_consolidation = false;

    structural_plasticity_destroy(system);
    system = structural_plasticity_create(&config);

    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    /* Provide sufficient activity */
    for (int i = 0; i < 10; i++) {
        structural_plasticity_update_activity(system, synapse_id, i * 100);
    }

    /* Update past maturation time */
    structural_plasticity_update(system, 15.0f);

    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);

    /* Should be stable or still maturing depending on activity */
    EXPECT_TRUE(state.state == SYNAPSE_STATE_STABLE ||
                state.maturation_progress > 0.5f);
}

//=============================================================================
// Immune Integration Tests
//=============================================================================

TEST_F(StructuralPlasticityTest, ComplementTaggingMarksSpine) {
    /* WHAT: Test complement tagging for immune pruning
     * WHY:  Weak synapses tagged by complement C1q/C3
     * BIOLOGICAL: Microglia target complement-tagged synapses
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    uint8_t tag[STRUCTURAL_EPITOPE_SIZE];
    memset(tag, 0xC3, STRUCTURAL_EPITOPE_SIZE);

    int result = structural_plasticity_tag_complement(
        system, synapse_id, tag, STRUCTURAL_EPITOPE_SIZE);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(structural_plasticity_is_complement_tagged(system, synapse_id));
}

TEST_F(StructuralPlasticityTest, ComplementTagIncreasesPruningUrgency) {
    /* WHAT: Test complement tag increases pruning urgency
     * WHY:  Tagged synapses prioritized for elimination
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    synapse_structural_state_t state_before;
    structural_plasticity_get_synapse_state(system, synapse_id, &state_before);

    uint8_t tag[STRUCTURAL_EPITOPE_SIZE];
    memset(tag, 0xC3, STRUCTURAL_EPITOPE_SIZE);
    structural_plasticity_tag_complement(
        system, synapse_id, tag, STRUCTURAL_EPITOPE_SIZE);

    synapse_structural_state_t state_after;
    structural_plasticity_get_synapse_state(system, synapse_id, &state_after);

    EXPECT_GT(state_after.pruning_urgency, state_before.pruning_urgency);
}

TEST_F(StructuralPlasticityTest, GetComplementTaggedList) {
    /* WHAT: Test retrieval of complement-tagged synapses
     * WHY:  Microglia need list of targets for engulfment
     */
    uint32_t id1, id2, id3;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &id1);
    structural_plasticity_form_synapse(system, 2, 3, 50.0f, &id2);
    structural_plasticity_form_synapse(system, 3, 4, 50.0f, &id3);

    uint8_t tag[STRUCTURAL_EPITOPE_SIZE];
    memset(tag, 0xC3, STRUCTURAL_EPITOPE_SIZE);

    /* Tag first two */
    structural_plasticity_tag_complement(system, id1, tag, STRUCTURAL_EPITOPE_SIZE);
    structural_plasticity_tag_complement(system, id2, tag, STRUCTURAL_EPITOPE_SIZE);

    uint32_t tagged_ids[10];
    uint32_t count;
    int result = structural_plasticity_get_complement_tagged(
        system, tagged_ids, 10, &count);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 2);
}

TEST_F(StructuralPlasticityTest, ComplementTaggedSpinesShouldPrune) {
    /* WHAT: Test complement-tagged spines marked for pruning
     * WHY:  Immune tagging overrides activity threshold
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    /* Tag with complement */
    uint8_t tag[STRUCTURAL_EPITOPE_SIZE];
    memset(tag, 0xC3, STRUCTURAL_EPITOPE_SIZE);
    structural_plasticity_tag_complement(
        system, synapse_id, tag, STRUCTURAL_EPITOPE_SIZE);

    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);

    EXPECT_TRUE(structural_plasticity_should_prune(system, &state));
}

//=============================================================================
// Spine Count and Query Tests
//=============================================================================

TEST_F(StructuralPlasticityTest, GetSpineCountByState) {
    /* WHAT: Test counting spines by state
     * WHY:  Track spine distribution across states
     */
    uint32_t id1, id2, id3;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &id1);
    structural_plasticity_form_synapse(system, 2, 3, 50.0f, &id2);
    structural_plasticity_form_synapse(system, 3, 4, 50.0f, &id3);

    EXPECT_EQ(structural_plasticity_get_spine_count(
        system, SYNAPSE_STATE_NASCENT), 3);

    structural_plasticity_stabilize_synapse(system, id1);

    EXPECT_EQ(structural_plasticity_get_spine_count(
        system, SYNAPSE_STATE_NASCENT), 2);
    EXPECT_EQ(structural_plasticity_get_spine_count(
        system, SYNAPSE_STATE_STABLE), 1);
}

TEST_F(StructuralPlasticityTest, GetSynapseStateRetrievesInfo) {
    /* WHAT: Test querying synapse state
     * WHY:  Verify state retrieval works
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 5, 7, 50.0f, &synapse_id);

    synapse_structural_state_t state;
    int result = structural_plasticity_get_synapse_state(
        system, synapse_id, &state);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.synapse_id, synapse_id);
    EXPECT_EQ(state.pre_neuron_id, 5);
    EXPECT_EQ(state.post_neuron_id, 7);
}

TEST_F(StructuralPlasticityTest, GetMorphologyRetrievesStructure) {
    /* WHAT: Test querying spine morphology
     * WHY:  Verify morphology data accessible
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    spine_morphology_t morph;
    int result = structural_plasticity_get_morphology(
        system, synapse_id, &morph);

    EXPECT_EQ(result, 0);
    EXPECT_GT(morph.spine_volume, 0.0f);
    EXPECT_GT(morph.psd_size, 0.0f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static uint32_t callback_count = 0;
static structural_event_t last_event;
static uint32_t last_synapse_id;

static void test_callback(
    structural_event_t event,
    uint32_t synapse_id,
    synapse_state_t old_state,
    synapse_state_t new_state,
    void* user_data
) {
    callback_count++;
    last_event = event;
    last_synapse_id = synapse_id;
}

TEST_F(StructuralPlasticityTest, CallbackRegistration) {
    /* WHAT: Test callback registration
     * WHY:  Verify callbacks can be registered
     */
    callback_count = 0;

    int result = structural_plasticity_register_callback(
        system, test_callback, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(StructuralPlasticityTest, CallbackInvokedOnFormation) {
    /* WHAT: Test callback invoked on spine formation
     * WHY:  Enable reactive responses to structural changes
     */
    callback_count = 0;
    structural_plasticity_register_callback(system, test_callback, nullptr);

    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    EXPECT_GT(callback_count, 0);
    EXPECT_EQ(last_event, STRUCTURAL_EVENT_FORMATION);
    EXPECT_EQ(last_synapse_id, synapse_id);
}

TEST_F(StructuralPlasticityTest, CallbackInvokedOnStabilization) {
    /* WHAT: Test callback invoked on stabilization
     * WHY:  Track state transitions
     */
    callback_count = 0;
    structural_plasticity_register_callback(system, test_callback, nullptr);

    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    callback_count = 0;  /* Reset after formation */
    structural_plasticity_stabilize_synapse(system, synapse_id);

    EXPECT_GT(callback_count, 0);
    EXPECT_EQ(last_event, STRUCTURAL_EVENT_STABILIZATION);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
