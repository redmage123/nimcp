/**
 * @file test_mesh_receptive_fields.cpp
 * @brief Unit tests for mesh receptive fields (Phase 14)
 *
 * Tests pre-defined receptive field patterns and library initialization.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_receptive_fields.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class MeshReceptiveFieldsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_error_t err = mesh_receptive_fields_init();
        ASSERT_EQ(err, NIMCP_SUCCESS);
    }

    void TearDown() override {
        mesh_receptive_fields_cleanup();
    }
};

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

TEST(MeshReceptiveFieldsInitTest, InitCleanupCycle) {
    nimcp_error_t err = mesh_receptive_fields_init();
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Reinit should be OK */
    err = mesh_receptive_fields_init();
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_receptive_fields_cleanup();
}

/* ============================================================================
 * Memory System Fields Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, HippocampusFieldExists) {
    const mesh_receptive_field_t* field = mesh_receptive_field_get_by_name("hippocampus");
    ASSERT_NE(field, nullptr);
    EXPECT_GT(field->threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, HippocampusRespondsToSpatialPatterns) {
    /* Hippocampus should respond to spatial/memory patterns */
    EXPECT_GT(MESH_RF_HIPPOCAMPUS.preferred[0].vector[MESH_DIM_COGNITIVE_PLANNING], 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, WorkingMemoryFieldExists) {
    EXPECT_GT(MESH_RF_WORKING_MEMORY.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, EpisodicMemoryFieldExists) {
    EXPECT_GT(MESH_RF_EPISODIC_MEMORY.threshold, 0.0f);
}

/* ============================================================================
 * Limbic System Fields Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, AmygdalaFieldExists) {
    const mesh_receptive_field_t* field = mesh_receptive_field_get_by_name("amygdala");
    ASSERT_NE(field, nullptr);
    EXPECT_GT(field->threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, AmygdalaHasThreatSensitivity) {
    /* Amygdala should be sensitive to threat/security patterns */
    EXPECT_GT(MESH_RF_AMYGDALA.preferred[0].vector[MESH_DIM_SECURITY_THREAT], 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, HypothalamusFieldExists) {
    EXPECT_GT(MESH_RF_HYPOTHALAMUS.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, NucleusAccumbensFieldExists) {
    EXPECT_GT(MESH_RF_NUCLEUS_ACCUMBENS.threshold, 0.0f);
}

/* ============================================================================
 * Cortical Fields Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, PFCLeftFieldExists) {
    EXPECT_GT(MESH_RF_PFC_LEFT.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, PFCRightFieldExists) {
    EXPECT_GT(MESH_RF_PFC_RIGHT.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, PFCRespondsToReasoningPatterns) {
    /* PFC should respond to reasoning/executive function patterns */
    EXPECT_GT(MESH_RF_PFC_LEFT.preferred[0].vector[MESH_DIM_COGNITIVE_REASONING], 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, DorsolateralPFCFieldExists) {
    const mesh_receptive_field_t* field = mesh_receptive_field_get_by_name("dorsolateral_pfc");
    ASSERT_NE(field, nullptr);
}

TEST_F(MeshReceptiveFieldsTest, AnteriorCingulateFieldExists) {
    EXPECT_GT(MESH_RF_ANTERIOR_CINGULATE.threshold, 0.0f);
}

/* ============================================================================
 * Motor System Fields Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, MotorCortexFieldExists) {
    EXPECT_GT(MESH_RF_MOTOR_CORTEX.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, CerebellumFieldExists) {
    EXPECT_GT(MESH_RF_CEREBELLUM.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, BasalGangliaFieldExists) {
    EXPECT_GT(MESH_RF_BASAL_GANGLIA.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, MotorFieldsRespondToMotorPatterns) {
    /* Motor cortex should respond to motor command patterns */
    EXPECT_GT(MESH_RF_MOTOR_CORTEX.preferred[0].vector[MESH_DIM_MOTOR_COMMAND], 0.0f);
}

/* ============================================================================
 * Sensory Fields Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, VisualCortexFieldExists) {
    EXPECT_GT(MESH_RF_VISUAL_CORTEX.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, AuditoryCortexFieldExists) {
    EXPECT_GT(MESH_RF_AUDITORY_CORTEX.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, ThalamusFieldExists) {
    EXPECT_GT(MESH_RF_THALAMUS.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, VisualFieldRespondsToVisualPatterns) {
    /* Visual cortex should respond to visual patterns */
    EXPECT_GT(MESH_RF_VISUAL_CORTEX.preferred[0].vector[MESH_DIM_PERCEPTION_VISUAL], 0.0f);
}

/* ============================================================================
 * Cognitive Fields Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, FEPOrchestratorFieldExists) {
    EXPECT_GT(MESH_RF_FEP_ORCHESTRATOR.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, AttentionManagerFieldExists) {
    EXPECT_GT(MESH_RF_ATTENTION_MANAGER.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, ReasoningEngineFieldExists) {
    const mesh_receptive_field_t* field = mesh_receptive_field_get_by_name("reasoning_engine");
    ASSERT_NE(field, nullptr);
}

TEST_F(MeshReceptiveFieldsTest, PlanningModuleFieldExists) {
    EXPECT_GT(MESH_RF_PLANNING_MODULE.threshold, 0.0f);
}

/* ============================================================================
 * Security Fields Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, BBBFieldExists) {
    EXPECT_GT(MESH_RF_BBB.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, ImmuneSystemFieldExists) {
    EXPECT_GT(MESH_RF_IMMUNE_SYSTEM.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, ThreatDetectorFieldExists) {
    EXPECT_GT(MESH_RF_THREAT_DETECTOR.threshold, 0.0f);
}

/* ============================================================================
 * Plasticity Fields Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, STDPFieldExists) {
    EXPECT_GT(MESH_RF_STDP.threshold, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, LTPFieldExists) {
    EXPECT_GT(MESH_RF_LTP.threshold, 0.0f);
}

/* ============================================================================
 * Lookup Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, GetByNameValid) {
    const mesh_receptive_field_t* field = mesh_receptive_field_get_by_name("hippocampus");
    EXPECT_NE(field, nullptr);
}

TEST_F(MeshReceptiveFieldsTest, GetByNameInvalid) {
    const mesh_receptive_field_t* field = mesh_receptive_field_get_by_name("nonexistent_field");
    EXPECT_EQ(field, nullptr);
}

TEST_F(MeshReceptiveFieldsTest, GetByNameNull) {
    const mesh_receptive_field_t* field = mesh_receptive_field_get_by_name(nullptr);
    EXPECT_EQ(field, nullptr);
}

/* ============================================================================
 * Category Lookup Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, GetMemoryFieldsByCategory) {
    const mesh_receptive_field_t* fields[10];
    size_t count = 0;

    nimcp_error_t err = mesh_receptive_fields_get_by_category(
        MESH_ADAPTER_CATEGORY_MEMORY, fields, 10, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);
}

TEST_F(MeshReceptiveFieldsTest, GetCognitiveFieldsByCategory) {
    const mesh_receptive_field_t* fields[10];
    size_t count = 0;

    nimcp_error_t err = mesh_receptive_fields_get_by_category(
        MESH_ADAPTER_CATEGORY_COGNITIVE, fields, 10, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);
}

TEST_F(MeshReceptiveFieldsTest, GetSecurityFieldsByCategory) {
    const mesh_receptive_field_t* fields[10];
    size_t count = 0;

    nimcp_error_t err = mesh_receptive_fields_get_by_category(
        MESH_ADAPTER_CATEGORY_SECURITY, fields, 10, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);
}

/* ============================================================================
 * Field Property Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, ThresholdsInValidRange) {
    /* Thresholds should be between 0 and 1 */
    EXPECT_GT(MESH_RF_HIPPOCAMPUS.threshold, 0.0f);
    EXPECT_LE(MESH_RF_HIPPOCAMPUS.threshold, 1.0f);

    EXPECT_GT(MESH_RF_AMYGDALA.threshold, 0.0f);
    EXPECT_LE(MESH_RF_AMYGDALA.threshold, 1.0f);
}

TEST_F(MeshReceptiveFieldsTest, SharpnessInValidRange) {
    /* Sharpness should be between 0 and 1 */
    EXPECT_GT(MESH_RF_HIPPOCAMPUS.sharpness, 0.0f);
    EXPECT_LE(MESH_RF_HIPPOCAMPUS.sharpness, 1.0f);
}

TEST_F(MeshReceptiveFieldsTest, NeuromodGainIsPositive) {
    EXPECT_GT(MESH_RF_HIPPOCAMPUS.neuromod_gain, 0.0f);
    EXPECT_GT(MESH_RF_AMYGDALA.neuromod_gain, 0.0f);
}

TEST_F(MeshReceptiveFieldsTest, PatternCountIsOne) {
    /* All pre-defined fields have single preferred pattern */
    EXPECT_EQ(MESH_RF_HIPPOCAMPUS.pattern_count, 1u);
    EXPECT_EQ(MESH_RF_AMYGDALA.pattern_count, 1u);
}

/* ============================================================================
 * Create Range Tests
 * ============================================================================ */

TEST_F(MeshReceptiveFieldsTest, CreateRangeField) {
    mesh_receptive_field_t field;

    nimcp_error_t err = mesh_receptive_field_create_range(0, 8, 0.5f, &field);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(field.pattern_count, 1u);
    EXPECT_EQ(field.preferred[0].vector[0], 0.5f);
}

TEST_F(MeshReceptiveFieldsTest, CreateRangeFieldNullOutput) {
    nimcp_error_t err = mesh_receptive_field_create_range(0, 8, 0.5f, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshReceptiveFieldsTest, CreateRangeFieldInvalidRange) {
    mesh_receptive_field_t field;

    /* Start >= MESH_PATTERN_DIM is invalid */
    nimcp_error_t err = mesh_receptive_field_create_range(MESH_PATTERN_DIM, MESH_PATTERN_DIM + 8, 0.5f, &field);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);
}
