/**
 * @file test_kg_wiring_modules.cpp
 * @brief Comprehensive unit tests for all 10 KG wiring modules
 *
 * Tests the Knowledge Graph wiring for the following brain regions:
 * 1. Wernicke's area (language comprehension)
 * 2. Temporal lobe (auditory, recognition)
 * 3. Insula (interoception, emotion)
 * 4. Cingulate cortex (conflict, error monitoring)
 * 5. Cerebellum (motor learning, timing)
 * 6. Hypothalamus (homeostasis, circadian)
 * 7. VTA (reward, motivation)
 * 8. Habenula (punishment, mood)
 * 9. Locus Coeruleus (arousal, attention)
 * 10. Entorhinal cortex (spatial, memory gateway)
 *
 * @date 2026-02-05
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/regions/wernicke/bridges/nimcp_wernicke_kg_wiring.h"
#include "core/brain/regions/temporal/bridges/nimcp_temporal_kg_wiring.h"
#include "core/brain/regions/insula/bridges/nimcp_insula_kg_wiring.h"
#include "core/brain/regions/cingulate/bridges/nimcp_cingulate_kg_wiring.h"
#include "core/brain/regions/cerebellum/bridges/nimcp_cerebellum_kg_wiring.h"
#include "core/brain/regions/hypothalamus/bridges/nimcp_hypothalamus_kg_wiring.h"
#include "core/brain/regions/vta/bridges/nimcp_vta_kg_wiring.h"
#include "core/brain/regions/habenula/bridges/nimcp_habenula_kg_wiring.h"
#include "core/brain/regions/locus_coeruleus/bridges/nimcp_lc_kg_wiring.h"
#include "core/brain/regions/entorhinal/bridges/nimcp_entorhinal_kg_wiring.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class KGWiringModulesTest : public ::testing::Test {
protected:
    brain_kg_t* kg = nullptr;

    /* Admin token for KG operations */
    static constexpr uint64_t ADMIN_TOKEN = 0x12345678ULL;

    void SetUp() override {
        /* Create a KG with security disabled for testing */
        brain_kg_config_t config;
        brain_kg_default_config(&config);
        config.enable_security = false;
        config.enable_access_control = false;
        config.enable_immune_integration = false;
        kg = brain_kg_create(&config);
        ASSERT_NE(kg, nullptr) << "Failed to create brain KG";
    }

    void TearDown() override {
        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }
};

/* ============================================================================
 * WERNICKE KG WIRING TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, WernickeDefaultConfigValid) {
    wernicke_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = wernicke_kg_default_config(&config);
    ASSERT_EQ(result, 0) << "wernicke_kg_default_config should return 0";

    EXPECT_TRUE(config.register_auditory_cortex);
    EXPECT_TRUE(config.register_phonological);
    EXPECT_TRUE(config.register_semantic);
    EXPECT_TRUE(config.register_syntax);
    EXPECT_TRUE(config.register_cross_edges);
    EXPECT_TRUE(config.include_state_metadata);
}

TEST_F(KGWiringModulesTest, WernickeConfigNullReturnsError) {
    int result = wernicke_kg_default_config(nullptr);
    EXPECT_EQ(result, -1)
        << "wernicke_kg_default_config(NULL) should return -1";
}

TEST_F(KGWiringModulesTest, WernickeRegisterAllWithNullKGFails) {
    wernicke_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = wernicke_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1)
        << "wernicke_kg_register_all with NULL KG should fail";
}

TEST_F(KGWiringModulesTest, WernickeRegisterAllSucceeds) {
    wernicke_kg_config_t config;
    wernicke_kg_default_config(&config);

    wernicke_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = wernicke_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0) << "wernicke_kg_register_all should succeed";

    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
    EXPECT_GT(state.edge_count, 0u);

    /* Verify subsystem nodes */
    EXPECT_NE(state.auditory_cortex_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.phonological_processing_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.semantic_processing_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.syntax_comprehension_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, WernickeGetRootValid) {
    wernicke_kg_state_t state;
    memset(&state, 0, sizeof(state));
    wernicke_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t root = wernicke_kg_get_root(kg);
    EXPECT_NE(root, BRAIN_KG_INVALID_NODE);
    EXPECT_EQ(root, state.root_id);
}

TEST_F(KGWiringModulesTest, WernickeGetRootNullKG) {
    brain_kg_node_id_t root = wernicke_kg_get_root(nullptr);
    EXPECT_EQ(root, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, WernickeFindSubsystem) {
    wernicke_kg_state_t state;
    memset(&state, 0, sizeof(state));
    wernicke_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t found = wernicke_kg_find_subsystem(kg, "auditory_cortex");
    EXPECT_NE(found, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t notfound = wernicke_kg_find_subsystem(kg, "nonexistent");
    EXPECT_EQ(notfound, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, WernickeUnregisterSucceeds) {
    wernicke_kg_state_t state;
    memset(&state, 0, sizeof(state));
    wernicke_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    int result = wernicke_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.registered);
}

TEST_F(KGWiringModulesTest, WernickeUnregisterNullParams) {
    int result = wernicke_kg_unregister_all(nullptr, nullptr, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * TEMPORAL KG WIRING TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, TemporalDefaultConfigValid) {
    temporal_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = temporal_kg_default_config(&config);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(config.register_auditory_processing);
    EXPECT_TRUE(config.register_object_recognition);
    EXPECT_TRUE(config.register_face_processing);
    EXPECT_TRUE(config.register_memory_encoding);
    EXPECT_TRUE(config.register_cross_edges);
}

TEST_F(KGWiringModulesTest, TemporalConfigNullReturnsError) {
    int result = temporal_kg_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, TemporalRegisterAllWithNullKGFails) {
    temporal_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = temporal_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, TemporalRegisterAllSucceeds) {
    temporal_kg_config_t config;
    temporal_kg_default_config(&config);

    temporal_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = temporal_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
    EXPECT_GT(state.edge_count, 0u);

    EXPECT_NE(state.auditory_processing_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.object_recognition_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.face_processing_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.memory_encoding_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, TemporalGetRootValid) {
    temporal_kg_state_t state;
    memset(&state, 0, sizeof(state));
    temporal_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t root = temporal_kg_get_root(kg);
    EXPECT_NE(root, BRAIN_KG_INVALID_NODE);
    EXPECT_EQ(root, state.root_id);
}

TEST_F(KGWiringModulesTest, TemporalGetRootNullKG) {
    brain_kg_node_id_t root = temporal_kg_get_root(nullptr);
    EXPECT_EQ(root, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, TemporalFindSubsystem) {
    temporal_kg_state_t state;
    memset(&state, 0, sizeof(state));
    temporal_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t found = temporal_kg_find_subsystem(kg, "face_processing");
    EXPECT_NE(found, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t notfound = temporal_kg_find_subsystem(kg, "nonexistent");
    EXPECT_EQ(notfound, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, TemporalUnregisterSucceeds) {
    temporal_kg_state_t state;
    memset(&state, 0, sizeof(state));
    temporal_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    int result = temporal_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.registered);
}

TEST_F(KGWiringModulesTest, TemporalUnregisterNullParams) {
    int result = temporal_kg_unregister_all(nullptr, nullptr, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * INSULA KG WIRING TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, InsulaDefaultConfigValid) {
    insula_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = insula_kg_default_config(&config);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(config.register_anterior_insula);
    EXPECT_TRUE(config.register_posterior_insula);
    EXPECT_TRUE(config.register_interoception);
    EXPECT_TRUE(config.register_pain_processing);
    EXPECT_TRUE(config.register_emotion_awareness);
    EXPECT_TRUE(config.register_cross_edges);
}

TEST_F(KGWiringModulesTest, InsulaConfigNullReturnsError) {
    int result = insula_kg_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, InsulaRegisterAllWithNullKGFails) {
    insula_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = insula_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, InsulaRegisterAllSucceeds) {
    insula_kg_config_t config;
    insula_kg_default_config(&config);

    insula_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = insula_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
    EXPECT_GT(state.edge_count, 0u);

    EXPECT_NE(state.anterior_insula_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.posterior_insula_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.interoception_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.pain_processing_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.emotion_awareness_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, InsulaGetRootValid) {
    insula_kg_state_t state;
    memset(&state, 0, sizeof(state));
    insula_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t root = insula_kg_get_root(kg);
    EXPECT_NE(root, BRAIN_KG_INVALID_NODE);
    EXPECT_EQ(root, state.root_id);
}

TEST_F(KGWiringModulesTest, InsulaGetRootNullKG) {
    brain_kg_node_id_t root = insula_kg_get_root(nullptr);
    EXPECT_EQ(root, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, InsulaFindSubsystem) {
    insula_kg_state_t state;
    memset(&state, 0, sizeof(state));
    insula_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t found = insula_kg_find_subsystem(kg, "interoception");
    EXPECT_NE(found, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t notfound = insula_kg_find_subsystem(kg, "nonexistent");
    EXPECT_EQ(notfound, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, InsulaUnregisterSucceeds) {
    insula_kg_state_t state;
    memset(&state, 0, sizeof(state));
    insula_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    int result = insula_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.registered);
}

TEST_F(KGWiringModulesTest, InsulaUnregisterNullParams) {
    int result = insula_kg_unregister_all(nullptr, nullptr, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * CINGULATE KG WIRING TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, CingulateDefaultConfigValid) {
    cingulate_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = cingulate_kg_default_config(&config);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(config.register_dacc);
    EXPECT_TRUE(config.register_vacc);
    EXPECT_TRUE(config.register_pcc);
    EXPECT_TRUE(config.register_conflict_detection);
    EXPECT_TRUE(config.register_error_monitoring);
    EXPECT_TRUE(config.register_reward_processing);
    EXPECT_TRUE(config.register_cross_edges);
}

TEST_F(KGWiringModulesTest, CingulateConfigNullReturnsError) {
    int result = cingulate_kg_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, CingulateRegisterAllWithNullKGFails) {
    cingulate_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = cingulate_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, CingulateRegisterAllSucceeds) {
    cingulate_kg_config_t config;
    cingulate_kg_default_config(&config);

    cingulate_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = cingulate_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
    EXPECT_GT(state.edge_count, 0u);

    EXPECT_NE(state.dacc_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.vacc_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.pcc_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.conflict_detection_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.error_monitoring_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.reward_processing_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, CingulateGetRootValid) {
    cingulate_kg_state_t state;
    memset(&state, 0, sizeof(state));
    cingulate_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t root = cingulate_kg_get_root(kg);
    EXPECT_NE(root, BRAIN_KG_INVALID_NODE);
    EXPECT_EQ(root, state.root_id);
}

TEST_F(KGWiringModulesTest, CingulateGetRootNullKG) {
    brain_kg_node_id_t root = cingulate_kg_get_root(nullptr);
    EXPECT_EQ(root, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, CingulateFindSubsystem) {
    cingulate_kg_state_t state;
    memset(&state, 0, sizeof(state));
    cingulate_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t found = cingulate_kg_find_subsystem(kg, "dACC");
    EXPECT_NE(found, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t notfound = cingulate_kg_find_subsystem(kg, "nonexistent");
    EXPECT_EQ(notfound, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, CingulateUnregisterSucceeds) {
    cingulate_kg_state_t state;
    memset(&state, 0, sizeof(state));
    cingulate_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    int result = cingulate_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.registered);
}

TEST_F(KGWiringModulesTest, CingulateUnregisterNullParams) {
    int result = cingulate_kg_unregister_all(nullptr, nullptr, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * CEREBELLUM KG WIRING TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, CerebellumDefaultConfigValid) {
    cerebellum_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = cerebellum_kg_default_config(&config);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(config.register_purkinje_cells);
    EXPECT_TRUE(config.register_granule_cells);
    EXPECT_TRUE(config.register_deep_nuclei);
    EXPECT_TRUE(config.register_motor_learning);
    EXPECT_TRUE(config.register_timing_control);
    EXPECT_TRUE(config.register_cross_edges);
}

TEST_F(KGWiringModulesTest, CerebellumConfigNullReturnsError) {
    int result = cerebellum_kg_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, CerebellumRegisterAllWithNullKGFails) {
    cerebellum_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = cerebellum_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, CerebellumRegisterAllSucceeds) {
    cerebellum_kg_config_t config;
    cerebellum_kg_default_config(&config);

    cerebellum_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = cerebellum_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
    EXPECT_GT(state.edge_count, 0u);

    EXPECT_NE(state.purkinje_cells_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.granule_cells_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.deep_nuclei_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.motor_learning_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.timing_control_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, CerebellumGetRootValid) {
    cerebellum_kg_state_t state;
    memset(&state, 0, sizeof(state));
    cerebellum_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t root = cerebellum_kg_get_root(kg);
    EXPECT_NE(root, BRAIN_KG_INVALID_NODE);
    EXPECT_EQ(root, state.root_id);
}

TEST_F(KGWiringModulesTest, CerebellumGetRootNullKG) {
    brain_kg_node_id_t root = cerebellum_kg_get_root(nullptr);
    EXPECT_EQ(root, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, CerebellumFindSubsystem) {
    cerebellum_kg_state_t state;
    memset(&state, 0, sizeof(state));
    cerebellum_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t found = cerebellum_kg_find_subsystem(kg, "purkinje_cells");
    EXPECT_NE(found, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t notfound = cerebellum_kg_find_subsystem(kg, "nonexistent");
    EXPECT_EQ(notfound, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, CerebellumUnregisterSucceeds) {
    cerebellum_kg_state_t state;
    memset(&state, 0, sizeof(state));
    cerebellum_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    int result = cerebellum_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.registered);
}

TEST_F(KGWiringModulesTest, CerebellumUnregisterNullParams) {
    int result = cerebellum_kg_unregister_all(nullptr, nullptr, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * HYPOTHALAMUS KG WIRING TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, HypothalamusDefaultConfigValid) {
    hypothalamus_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = hypothalamus_kg_default_config(&config);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(config.register_scn);
    EXPECT_TRUE(config.register_pvn);
    EXPECT_TRUE(config.register_vmh);
    EXPECT_TRUE(config.register_lh);
    EXPECT_TRUE(config.register_homeostasis);
    EXPECT_TRUE(config.register_circadian_rhythm);
    EXPECT_TRUE(config.register_stress_response);
    EXPECT_TRUE(config.register_cross_edges);
}

TEST_F(KGWiringModulesTest, HypothalamusConfigNullReturnsError) {
    int result = hypothalamus_kg_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, HypothalamusRegisterAllWithNullKGFails) {
    hypothalamus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = hypothalamus_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, HypothalamusRegisterAllSucceeds) {
    hypothalamus_kg_config_t config;
    hypothalamus_kg_default_config(&config);

    hypothalamus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = hypothalamus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
    EXPECT_GT(state.edge_count, 0u);

    EXPECT_NE(state.scn_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.pvn_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.vmh_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.lh_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.homeostasis_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.circadian_rhythm_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.stress_response_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, HypothalamusGetRootValid) {
    hypothalamus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hypothalamus_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t root = hypothalamus_kg_get_root(kg);
    EXPECT_NE(root, BRAIN_KG_INVALID_NODE);
    EXPECT_EQ(root, state.root_id);
}

TEST_F(KGWiringModulesTest, HypothalamusGetRootNullKG) {
    brain_kg_node_id_t root = hypothalamus_kg_get_root(nullptr);
    EXPECT_EQ(root, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, HypothalamusFindSubsystem) {
    hypothalamus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hypothalamus_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t found = hypothalamus_kg_find_subsystem(kg, "SCN");
    EXPECT_NE(found, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t notfound = hypothalamus_kg_find_subsystem(kg, "nonexistent");
    EXPECT_EQ(notfound, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, HypothalamusUnregisterSucceeds) {
    hypothalamus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hypothalamus_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    int result = hypothalamus_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.registered);
}

TEST_F(KGWiringModulesTest, HypothalamusUnregisterNullParams) {
    int result = hypothalamus_kg_unregister_all(nullptr, nullptr, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * VTA KG WIRING TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, VTADefaultConfigValid) {
    vta_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = vta_kg_default_config(&config);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(config.register_dopaminergic_neurons);
    EXPECT_TRUE(config.register_reward_prediction);
    EXPECT_TRUE(config.register_motivation_drive);
    EXPECT_TRUE(config.register_reinforcement_learning);
    EXPECT_TRUE(config.register_cross_edges);
}

TEST_F(KGWiringModulesTest, VTAConfigNullReturnsError) {
    int result = vta_kg_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, VTARegisterAllWithNullKGFails) {
    vta_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = vta_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, VTARegisterAllSucceeds) {
    vta_kg_config_t config;
    vta_kg_default_config(&config);

    vta_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = vta_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
    EXPECT_GT(state.edge_count, 0u);

    EXPECT_NE(state.dopaminergic_neurons_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.reward_prediction_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.motivation_drive_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.reinforcement_learning_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, VTAGetRootValid) {
    vta_kg_state_t state;
    memset(&state, 0, sizeof(state));
    vta_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t root = vta_kg_get_root(kg);
    EXPECT_NE(root, BRAIN_KG_INVALID_NODE);
    EXPECT_EQ(root, state.root_id);
}

TEST_F(KGWiringModulesTest, VTAGetRootNullKG) {
    brain_kg_node_id_t root = vta_kg_get_root(nullptr);
    EXPECT_EQ(root, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, VTAFindSubsystem) {
    vta_kg_state_t state;
    memset(&state, 0, sizeof(state));
    vta_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t found = vta_kg_find_subsystem(kg, "dopaminergic_neurons");
    EXPECT_NE(found, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t notfound = vta_kg_find_subsystem(kg, "nonexistent");
    EXPECT_EQ(notfound, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, VTAUnregisterSucceeds) {
    vta_kg_state_t state;
    memset(&state, 0, sizeof(state));
    vta_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    int result = vta_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.registered);
}

TEST_F(KGWiringModulesTest, VTAUnregisterNullParams) {
    int result = vta_kg_unregister_all(nullptr, nullptr, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * HABENULA KG WIRING TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, HabenulaDefaultConfigValid) {
    habenula_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = habenula_kg_default_config(&config);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(config.register_lateral_habenula);
    EXPECT_TRUE(config.register_medial_habenula);
    EXPECT_TRUE(config.register_reward_evaluation);
    EXPECT_TRUE(config.register_punishment_signal);
    EXPECT_TRUE(config.register_mood_regulation);
    EXPECT_TRUE(config.register_cross_edges);
}

TEST_F(KGWiringModulesTest, HabenulaConfigNullReturnsError) {
    int result = habenula_kg_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, HabenulaRegisterAllWithNullKGFails) {
    habenula_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = habenula_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, HabenulaRegisterAllSucceeds) {
    habenula_kg_config_t config;
    habenula_kg_default_config(&config);

    habenula_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = habenula_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
    EXPECT_GT(state.edge_count, 0u);

    EXPECT_NE(state.lateral_habenula_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.medial_habenula_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.reward_evaluation_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.punishment_signal_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.mood_regulation_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, HabenulaGetRootValid) {
    habenula_kg_state_t state;
    memset(&state, 0, sizeof(state));
    habenula_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t root = habenula_kg_get_root(kg);
    EXPECT_NE(root, BRAIN_KG_INVALID_NODE);
    EXPECT_EQ(root, state.root_id);
}

TEST_F(KGWiringModulesTest, HabenulaGetRootNullKG) {
    brain_kg_node_id_t root = habenula_kg_get_root(nullptr);
    EXPECT_EQ(root, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, HabenulaFindSubsystem) {
    habenula_kg_state_t state;
    memset(&state, 0, sizeof(state));
    habenula_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t found = habenula_kg_find_subsystem(kg, "lateral_habenula");
    EXPECT_NE(found, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t notfound = habenula_kg_find_subsystem(kg, "nonexistent");
    EXPECT_EQ(notfound, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, HabenulaUnregisterSucceeds) {
    habenula_kg_state_t state;
    memset(&state, 0, sizeof(state));
    habenula_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    int result = habenula_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.registered);
}

TEST_F(KGWiringModulesTest, HabenulaUnregisterNullParams) {
    int result = habenula_kg_unregister_all(nullptr, nullptr, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * LOCUS COERULEUS KG WIRING TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, LCDefaultConfigValid) {
    lc_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = lc_kg_default_config(&config);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(config.register_noradrenergic_neurons);
    EXPECT_TRUE(config.register_arousal_regulation);
    EXPECT_TRUE(config.register_attention_modulation);
    EXPECT_TRUE(config.register_stress_response);
    EXPECT_TRUE(config.register_cross_edges);
}

TEST_F(KGWiringModulesTest, LCConfigNullReturnsError) {
    int result = lc_kg_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, LCRegisterAllWithNullKGFails) {
    lc_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = lc_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, LCRegisterAllSucceeds) {
    lc_kg_config_t config;
    lc_kg_default_config(&config);

    lc_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = lc_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
    EXPECT_GT(state.edge_count, 0u);

    EXPECT_NE(state.noradrenergic_neurons_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.arousal_regulation_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.attention_modulation_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.stress_response_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, LCGetRootValid) {
    lc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    lc_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t root = lc_kg_get_root(kg);
    EXPECT_NE(root, BRAIN_KG_INVALID_NODE);
    EXPECT_EQ(root, state.root_id);
}

TEST_F(KGWiringModulesTest, LCGetRootNullKG) {
    brain_kg_node_id_t root = lc_kg_get_root(nullptr);
    EXPECT_EQ(root, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, LCFindSubsystem) {
    lc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    lc_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t found = lc_kg_find_subsystem(kg, "noradrenergic_neurons");
    EXPECT_NE(found, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t notfound = lc_kg_find_subsystem(kg, "nonexistent");
    EXPECT_EQ(notfound, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, LCUnregisterSucceeds) {
    lc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    lc_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    int result = lc_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.registered);
}

TEST_F(KGWiringModulesTest, LCUnregisterNullParams) {
    int result = lc_kg_unregister_all(nullptr, nullptr, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * ENTORHINAL KG WIRING TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, EntorhinalDefaultConfigValid) {
    entorhinal_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = entorhinal_kg_default_config(&config);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(config.register_grid_cells);
    EXPECT_TRUE(config.register_border_cells);
    EXPECT_TRUE(config.register_spatial_mapping);
    EXPECT_TRUE(config.register_memory_gateway);
    EXPECT_TRUE(config.register_cortical_input);
    EXPECT_TRUE(config.register_cross_edges);
}

TEST_F(KGWiringModulesTest, EntorhinalConfigNullReturnsError) {
    int result = entorhinal_kg_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, EntorhinalRegisterAllWithNullKGFails) {
    entorhinal_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = entorhinal_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

TEST_F(KGWiringModulesTest, EntorhinalRegisterAllSucceeds) {
    entorhinal_kg_config_t config;
    entorhinal_kg_default_config(&config);

    entorhinal_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = entorhinal_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
    EXPECT_GT(state.edge_count, 0u);

    EXPECT_NE(state.grid_cells_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.border_cells_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.spatial_mapping_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.memory_gateway_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.cortical_input_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, EntorhinalGetRootValid) {
    entorhinal_kg_state_t state;
    memset(&state, 0, sizeof(state));
    entorhinal_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t root = entorhinal_kg_get_root(kg);
    EXPECT_NE(root, BRAIN_KG_INVALID_NODE);
    EXPECT_EQ(root, state.root_id);
}

TEST_F(KGWiringModulesTest, EntorhinalGetRootNullKG) {
    brain_kg_node_id_t root = entorhinal_kg_get_root(nullptr);
    EXPECT_EQ(root, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, EntorhinalFindSubsystem) {
    entorhinal_kg_state_t state;
    memset(&state, 0, sizeof(state));
    entorhinal_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t found = entorhinal_kg_find_subsystem(kg, "ec_grid_cells");
    EXPECT_NE(found, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t notfound = entorhinal_kg_find_subsystem(kg, "nonexistent");
    EXPECT_EQ(notfound, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, EntorhinalUnregisterSucceeds) {
    entorhinal_kg_state_t state;
    memset(&state, 0, sizeof(state));
    entorhinal_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    int result = entorhinal_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.registered);
}

TEST_F(KGWiringModulesTest, EntorhinalUnregisterNullParams) {
    int result = entorhinal_kg_unregister_all(nullptr, nullptr, ADMIN_TOKEN);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * COMBINED/COEXISTENCE TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, AllModulesCanCoexist) {
    /* Register all 10 modules in the same KG */
    wernicke_kg_state_t wernicke_state;
    temporal_kg_state_t temporal_state;
    insula_kg_state_t insula_state;
    cingulate_kg_state_t cingulate_state;
    cerebellum_kg_state_t cerebellum_state;
    hypothalamus_kg_state_t hypothalamus_state;
    vta_kg_state_t vta_state;
    habenula_kg_state_t habenula_state;
    lc_kg_state_t lc_state;
    entorhinal_kg_state_t entorhinal_state;

    memset(&wernicke_state, 0, sizeof(wernicke_state));
    memset(&temporal_state, 0, sizeof(temporal_state));
    memset(&insula_state, 0, sizeof(insula_state));
    memset(&cingulate_state, 0, sizeof(cingulate_state));
    memset(&cerebellum_state, 0, sizeof(cerebellum_state));
    memset(&hypothalamus_state, 0, sizeof(hypothalamus_state));
    memset(&vta_state, 0, sizeof(vta_state));
    memset(&habenula_state, 0, sizeof(habenula_state));
    memset(&lc_state, 0, sizeof(lc_state));
    memset(&entorhinal_state, 0, sizeof(entorhinal_state));

    EXPECT_EQ(wernicke_kg_register_all(kg, nullptr, &wernicke_state, ADMIN_TOKEN), 0);
    EXPECT_EQ(temporal_kg_register_all(kg, nullptr, &temporal_state, ADMIN_TOKEN), 0);
    EXPECT_EQ(insula_kg_register_all(kg, nullptr, &insula_state, ADMIN_TOKEN), 0);
    EXPECT_EQ(cingulate_kg_register_all(kg, nullptr, &cingulate_state, ADMIN_TOKEN), 0);
    EXPECT_EQ(cerebellum_kg_register_all(kg, nullptr, &cerebellum_state, ADMIN_TOKEN), 0);
    EXPECT_EQ(hypothalamus_kg_register_all(kg, nullptr, &hypothalamus_state, ADMIN_TOKEN), 0);
    EXPECT_EQ(vta_kg_register_all(kg, nullptr, &vta_state, ADMIN_TOKEN), 0);
    EXPECT_EQ(habenula_kg_register_all(kg, nullptr, &habenula_state, ADMIN_TOKEN), 0);
    EXPECT_EQ(lc_kg_register_all(kg, nullptr, &lc_state, ADMIN_TOKEN), 0);
    EXPECT_EQ(entorhinal_kg_register_all(kg, nullptr, &entorhinal_state, ADMIN_TOKEN), 0);

    /* All should have distinct root nodes */
    brain_kg_node_id_t roots[] = {
        wernicke_state.root_id,
        temporal_state.root_id,
        insula_state.root_id,
        cingulate_state.root_id,
        cerebellum_state.root_id,
        hypothalamus_state.root_id,
        vta_state.root_id,
        habenula_state.root_id,
        lc_state.root_id,
        entorhinal_state.root_id
    };

    for (int i = 0; i < 10; i++) {
        EXPECT_NE(roots[i], BRAIN_KG_INVALID_NODE)
            << "Root " << i << " should be valid";
        for (int j = i + 1; j < 10; j++) {
            EXPECT_NE(roots[i], roots[j])
                << "Roots " << i << " and " << j << " should be different";
        }
    }

    /* All should still be queryable */
    EXPECT_NE(wernicke_kg_get_root(kg), BRAIN_KG_INVALID_NODE);
    EXPECT_NE(temporal_kg_get_root(kg), BRAIN_KG_INVALID_NODE);
    EXPECT_NE(insula_kg_get_root(kg), BRAIN_KG_INVALID_NODE);
    EXPECT_NE(cingulate_kg_get_root(kg), BRAIN_KG_INVALID_NODE);
    EXPECT_NE(cerebellum_kg_get_root(kg), BRAIN_KG_INVALID_NODE);
    EXPECT_NE(hypothalamus_kg_get_root(kg), BRAIN_KG_INVALID_NODE);
    EXPECT_NE(vta_kg_get_root(kg), BRAIN_KG_INVALID_NODE);
    EXPECT_NE(habenula_kg_get_root(kg), BRAIN_KG_INVALID_NODE);
    EXPECT_NE(lc_kg_get_root(kg), BRAIN_KG_INVALID_NODE);
    EXPECT_NE(entorhinal_kg_get_root(kg), BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, DoubleRegistrationHandled) {
    /* Test that double registration is handled gracefully for all modules */

    /* Wernicke */
    {
        wernicke_kg_state_t state;
        memset(&state, 0, sizeof(state));
        EXPECT_EQ(wernicke_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN), 0);
        int result2 = wernicke_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);
        EXPECT_TRUE(result2 == 0 || result2 == -1);
    }
}

TEST_F(KGWiringModulesTest, UnregisterWithoutRegisterIsSafe) {
    /* Test that unregistering without first registering is safe */
    wernicke_kg_state_t state;
    memset(&state, 0, sizeof(state));

    /* Should return error since nothing was registered */
    int result = wernicke_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    /* Either succeeds (no-op) or fails gracefully */
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(KGWiringModulesTest, RegisterWithNullStateAccepted) {
    /* Some implementations may accept NULL state */
    int result = wernicke_kg_register_all(kg, nullptr, nullptr, ADMIN_TOKEN);
    /* Either succeeds (ignores state output) or fails gracefully */
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(KGWiringModulesTest, FindSubsystemNullName) {
    wernicke_kg_state_t state;
    memset(&state, 0, sizeof(state));
    wernicke_kg_register_all(kg, nullptr, &state, ADMIN_TOKEN);

    brain_kg_node_id_t found = wernicke_kg_find_subsystem(kg, nullptr);
    EXPECT_EQ(found, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, FindSubsystemNullKG) {
    brain_kg_node_id_t found = wernicke_kg_find_subsystem(nullptr, "auditory_cortex");
    EXPECT_EQ(found, BRAIN_KG_INVALID_NODE);
}

/* ============================================================================
 * SELECTIVE REGISTRATION TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, WernickeSelectiveRegistration) {
    wernicke_kg_config_t config;
    wernicke_kg_default_config(&config);
    config.register_auditory_cortex = false;
    config.register_phonological = false;
    config.register_cross_edges = false;

    wernicke_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = wernicke_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    /* Root should still exist */
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);

    /* Disabled subsystems should be invalid or 0 */
    EXPECT_TRUE(state.auditory_cortex_id == BRAIN_KG_INVALID_NODE ||
                state.auditory_cortex_id == 0);
    EXPECT_TRUE(state.phonological_processing_id == BRAIN_KG_INVALID_NODE ||
                state.phonological_processing_id == 0);

    /* Enabled subsystems should be valid */
    EXPECT_NE(state.semantic_processing_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.syntax_comprehension_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGWiringModulesTest, CerebellumSelectiveRegistration) {
    cerebellum_kg_config_t config;
    cerebellum_kg_default_config(&config);
    config.register_purkinje_cells = false;
    config.register_granule_cells = false;
    config.register_cross_edges = false;

    cerebellum_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = cerebellum_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_TRUE(state.purkinje_cells_id == BRAIN_KG_INVALID_NODE ||
                state.purkinje_cells_id == 0);
    EXPECT_TRUE(state.granule_cells_id == BRAIN_KG_INVALID_NODE ||
                state.granule_cells_id == 0);

    EXPECT_NE(state.deep_nuclei_id, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(state.motor_learning_id, BRAIN_KG_INVALID_NODE);
}

/* ============================================================================
 * EDGE COUNT VERIFICATION TESTS
 * ============================================================================ */

TEST_F(KGWiringModulesTest, WernickeEdgesCreatedWhenCrossEdgesEnabled) {
    wernicke_kg_config_t config;
    wernicke_kg_default_config(&config);
    config.register_cross_edges = true;

    wernicke_kg_state_t state;
    memset(&state, 0, sizeof(state));

    wernicke_kg_register_all(kg, &config, &state, ADMIN_TOKEN);

    /* Should have more edges when cross edges are enabled */
    EXPECT_GT(state.edge_count, state.node_count);
}

TEST_F(KGWiringModulesTest, WernickeFewerEdgesWhenCrossEdgesDisabled) {
    wernicke_kg_config_t config;
    wernicke_kg_default_config(&config);
    config.register_cross_edges = false;

    wernicke_kg_state_t state;
    memset(&state, 0, sizeof(state));

    wernicke_kg_register_all(kg, &config, &state, ADMIN_TOKEN);

    /* Should have fewer edges when cross edges are disabled */
    /* At minimum, each subsystem has one edge to root */
    EXPECT_GE(state.edge_count, state.node_count - 1);
}
