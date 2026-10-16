/**
 * @file test_astrocyte_types_real.cpp
 * @brief Real tests for region-specific astrocyte types with specialized modulation
 *
 * COVERAGE TARGET: astrocyte_types module (currently 0%)
 * APPROACH: Test all real functions with actual instances
 * FOCUS: Type-specific modulation, context initialization, type dispatch
 */

#include <gtest/gtest.h>
#include <cmath>

#include "glial/astrocyte_types/nimcp_astrocyte_types.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AstrocyteTypesRealTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
    }

    void TearDown() override {
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0) << "Memory leak detected";
    }
};

//=============================================================================
// Type Name Tests
//=============================================================================

TEST_F(AstrocyteTypesRealTest, GetTypeName_Generic) {
    const char* name = astrocyte_type_get_name(ASTROCYTE_TYPE_GENERIC);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "GENERIC");
}

TEST_F(AstrocyteTypesRealTest, GetTypeName_V1Sensory) {
    const char* name = astrocyte_type_get_name(ASTROCYTE_TYPE_V1_SENSORY);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "V1_SENSORY");
}

TEST_F(AstrocyteTypesRealTest, GetTypeName_A1Sensory) {
    const char* name = astrocyte_type_get_name(ASTROCYTE_TYPE_A1_SENSORY);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "A1_SENSORY");
}

TEST_F(AstrocyteTypesRealTest, GetTypeName_MultimodalIntegration) {
    const char* name = astrocyte_type_get_name(ASTROCYTE_TYPE_MULTIMODAL_INTEGRATION);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "MULTIMODAL_INTEGRATION");
}

TEST_F(AstrocyteTypesRealTest, GetTypeName_Metacognitive) {
    const char* name = astrocyte_type_get_name(ASTROCYTE_TYPE_METACOGNITIVE);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "METACOGNITIVE");
}

TEST_F(AstrocyteTypesRealTest, GetTypeName_ExecutiveControl) {
    const char* name = astrocyte_type_get_name(ASTROCYTE_TYPE_EXECUTIVE_CONTROL);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "EXECUTIVE_CONTROL");
}

TEST_F(AstrocyteTypesRealTest, GetTypeName_AllTypes) {
    // Test all valid types return non-null names
    for (int i = 0; i < ASTROCYTE_TYPE_COUNT; i++) {
        const char* name = astrocyte_type_get_name((astrocyte_type_t)i);
        ASSERT_NE(name, nullptr) << "Type " << i << " returned null name";
        EXPECT_GT(strlen(name), 0) << "Type " << i << " has empty name";
    }
}

//=============================================================================
// Context Initialization Tests
//=============================================================================

TEST_F(AstrocyteTypesRealTest, InitContext_Generic) {
    astrocyte_type_context_t context;
    nimcp_result_t result = astrocyte_type_init_context(&context, ASTROCYTE_TYPE_GENERIC);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(context.type, ASTROCYTE_TYPE_GENERIC);
}

TEST_F(AstrocyteTypesRealTest, InitContext_V1Sensory) {
    astrocyte_type_context_t context;
    nimcp_result_t result = astrocyte_type_init_context(&context, ASTROCYTE_TYPE_V1_SENSORY);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(context.type, ASTROCYTE_TYPE_V1_SENSORY);

    // Check V1-specific parameters are initialized
    EXPECT_GE(context.v1.preferred_orientation, 0.0f);
    EXPECT_LE(context.v1.preferred_orientation, 180.0f);
    EXPECT_GT(context.v1.orientation_selectivity, 0.0f);
    EXPECT_GE(context.v1.contrast_adaptation_state, 0.0f);
    EXPECT_LE(context.v1.contrast_adaptation_state, 1.0f);
}

TEST_F(AstrocyteTypesRealTest, InitContext_A1Sensory) {
    astrocyte_type_context_t context;
    nimcp_result_t result = astrocyte_type_init_context(&context, ASTROCYTE_TYPE_A1_SENSORY);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(context.type, ASTROCYTE_TYPE_A1_SENSORY);

    // Check A1-specific parameters are initialized
    EXPECT_GT(context.a1.preferred_frequency_hz, 0.0f);
    EXPECT_GT(context.a1.frequency_bandwidth_octaves, 0.0f);
    EXPECT_GT(context.a1.adaptation_timescale_ms, 0.0f);
}

TEST_F(AstrocyteTypesRealTest, InitContext_MultimodalIntegration) {
    astrocyte_type_context_t context;
    nimcp_result_t result = astrocyte_type_init_context(&context, ASTROCYTE_TYPE_MULTIMODAL_INTEGRATION);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(context.type, ASTROCYTE_TYPE_MULTIMODAL_INTEGRATION);

    // Check multimodal-specific parameters are initialized
    EXPECT_EQ(context.multimodal.num_modalities_active, 0);
    EXPECT_GT(context.multimodal.temporal_binding_window_ms, 0.0f);
    EXPECT_GE(context.multimodal.cross_modal_enhancement, 1.0f);
}

TEST_F(AstrocyteTypesRealTest, InitContext_Metacognitive) {
    astrocyte_type_context_t context;
    nimcp_result_t result = astrocyte_type_init_context(&context, ASTROCYTE_TYPE_METACOGNITIVE);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(context.type, ASTROCYTE_TYPE_METACOGNITIVE);

    // Check metacognitive-specific parameters are initialized
    EXPECT_GE(context.metacognitive.uncertainty_level, 0.0f);
    EXPECT_LE(context.metacognitive.uncertainty_level, 1.0f);
    EXPECT_GE(context.metacognitive.confidence_threshold, 0.0f);
    EXPECT_LE(context.metacognitive.confidence_threshold, 1.0f);
}

TEST_F(AstrocyteTypesRealTest, InitContext_ExecutiveControl) {
    astrocyte_type_context_t context;
    nimcp_result_t result = astrocyte_type_init_context(&context, ASTROCYTE_TYPE_EXECUTIVE_CONTROL);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(context.type, ASTROCYTE_TYPE_EXECUTIVE_CONTROL);

    // Check executive control-specific parameters are initialized
    EXPECT_GE(context.executive.goal_relevance, 0.0f);
    EXPECT_LE(context.executive.goal_relevance, 1.0f);
    EXPECT_GE(context.executive.working_memory_load, 0.0f);
    EXPECT_LE(context.executive.working_memory_load, 1.0f);
}

TEST_F(AstrocyteTypesRealTest, InitContext_AllTypes) {
    // Test all valid types can be initialized
    for (int i = 0; i < ASTROCYTE_TYPE_COUNT; i++) {
        astrocyte_type_context_t context;
        nimcp_result_t result = astrocyte_type_init_context(&context, (astrocyte_type_t)i);

        EXPECT_EQ(result, NIMCP_SUCCESS) << "Failed to init type " << i;
        EXPECT_EQ(context.type, (astrocyte_type_t)i) << "Type mismatch for " << i;
    }
}

//=============================================================================
// Type-Specific Modulation Tests (with real astrocyte and synapse)
//=============================================================================

TEST_F(AstrocyteTypesRealTest, V1Modulate_ReturnsValidFactor) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_V1_SENSORY, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_type_context_t context;
    astrocyte_type_init_context(&context, ASTROCYTE_TYPE_V1_SENSORY);

    // Call modulation with null synapse (should still return valid factor)
    float factor = astrocyte_type_v1_modulate(astro, nullptr, &context);

    // V1 modulation should return factor in range 0.5-2.0
    EXPECT_GE(factor, 0.3f);
    EXPECT_LE(factor, 2.5f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTypesRealTest, A1Modulate_ReturnsValidFactor) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_A1_SENSORY, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_type_context_t context;
    astrocyte_type_init_context(&context, ASTROCYTE_TYPE_A1_SENSORY);

    float factor = astrocyte_type_a1_modulate(astro, nullptr, &context);

    // A1 modulation should return factor in range 0.5-2.0
    EXPECT_GE(factor, 0.3f);
    EXPECT_LE(factor, 2.5f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTypesRealTest, MultimodalModulate_ReturnsValidFactor) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_MULTIMODAL_INTEGRATION, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_type_context_t context;
    astrocyte_type_init_context(&context, ASTROCYTE_TYPE_MULTIMODAL_INTEGRATION);

    float factor = astrocyte_type_multimodal_modulate(astro, nullptr, &context);

    // Multimodal modulation should return factor in range 1.0-2.5
    EXPECT_GE(factor, 0.8f);
    EXPECT_LE(factor, 3.0f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTypesRealTest, MetacognitiveModulate_ReturnsValidFactor) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_METACOGNITIVE, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_type_context_t context;
    astrocyte_type_init_context(&context, ASTROCYTE_TYPE_METACOGNITIVE);

    float factor = astrocyte_type_metacognitive_modulate(astro, nullptr, &context);

    // Metacognitive modulation should return factor in range 0.8-1.5
    EXPECT_GE(factor, 0.5f);
    EXPECT_LE(factor, 2.0f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTypesRealTest, ExecutiveModulate_ReturnsValidFactor) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_EXECUTIVE_CONTROL, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_type_context_t context;
    astrocyte_type_init_context(&context, ASTROCYTE_TYPE_EXECUTIVE_CONTROL);

    float factor = astrocyte_type_executive_modulate(astro, nullptr, &context);

    // Executive modulation should return factor in range 0.3-2.0
    EXPECT_GE(factor, 0.2f);
    EXPECT_LE(factor, 2.5f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTypesRealTest, GenericModulate_ReturnsValidFactor) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_type_context_t context;
    astrocyte_type_init_context(&context, ASTROCYTE_TYPE_GENERIC);

    float factor = astrocyte_type_generic_modulate(astro, nullptr, &context);

    // Generic modulation should return factor in range 0.8-1.2
    EXPECT_GE(factor, 0.6f);
    EXPECT_LE(factor, 1.5f);

    astrocyte_destroy(astro);
}

//=============================================================================
// Type Dispatch Tests
//=============================================================================

TEST_F(AstrocyteTypesRealTest, DispatchModulation_Generic) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_type_context_t context;
    astrocyte_type_init_context(&context, ASTROCYTE_TYPE_GENERIC);

    float factor = astrocyte_type_dispatch_modulation(astro, nullptr, &context);

    EXPECT_GE(factor, 0.5f);
    EXPECT_LE(factor, 1.5f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTypesRealTest, DispatchModulation_V1Sensory) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_V1_SENSORY, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_type_context_t context;
    astrocyte_type_init_context(&context, ASTROCYTE_TYPE_V1_SENSORY);

    float factor = astrocyte_type_dispatch_modulation(astro, nullptr, &context);

    EXPECT_GE(factor, 0.3f);
    EXPECT_LE(factor, 2.5f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTypesRealTest, DispatchModulation_AllTypes) {
    // Test dispatch works for all types
    for (int i = 0; i < ASTROCYTE_TYPE_COUNT; i++) {
        astrocyte_type_t type = (astrocyte_type_t)i;
        astrocyte_t* astro = astrocyte_create(i, type, 0.0f, 0.0f, 0.0f, 50.0f);
        ASSERT_NE(astro, nullptr) << "Failed to create astrocyte type " << i;

        astrocyte_type_context_t context;
        astrocyte_type_init_context(&context, type);

        float factor = astrocyte_type_dispatch_modulation(astro, nullptr, &context);

        // All modulation factors should be positive and reasonable
        EXPECT_GT(factor, 0.0f) << "Type " << i << " returned non-positive factor";
        EXPECT_LT(factor, 5.0f) << "Type " << i << " returned unreasonably large factor";

        astrocyte_destroy(astro);
    }
}

//=============================================================================
// Context Modification Tests
//=============================================================================

TEST_F(AstrocyteTypesRealTest, V1Context_ModifyOrientation) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_V1_SENSORY, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_type_context_t context;
    astrocyte_type_init_context(&context, ASTROCYTE_TYPE_V1_SENSORY);

    // Modify context
    context.v1.preferred_orientation = 45.0f;
    context.v1.local_contrast = 0.8f;

    float factor = astrocyte_type_v1_modulate(astro, nullptr, &context);
    EXPECT_GT(factor, 0.0f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTypesRealTest, MultimodalContext_VaryModalities) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_MULTIMODAL_INTEGRATION, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_type_context_t context;
    astrocyte_type_init_context(&context, ASTROCYTE_TYPE_MULTIMODAL_INTEGRATION);

    // Test with different numbers of active modalities
    for (uint32_t num_modalities = 0; num_modalities <= 5; num_modalities++) {
        context.multimodal.num_modalities_active = num_modalities;
        float factor = astrocyte_type_multimodal_modulate(astro, nullptr, &context);
        EXPECT_GT(factor, 0.0f) << "Failed with " << num_modalities << " modalities";
    }

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTypesRealTest, ExecutiveContext_VaryGoalRelevance) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_EXECUTIVE_CONTROL, 0.0f, 0.0f, 0.0f, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_type_context_t context;
    astrocyte_type_init_context(&context, ASTROCYTE_TYPE_EXECUTIVE_CONTROL);

    // Test with different goal relevance levels
    for (int i = 0; i <= 10; i++) {
        context.executive.goal_relevance = i / 10.0f;
        float factor = astrocyte_type_executive_modulate(astro, nullptr, &context);
        EXPECT_GT(factor, 0.0f) << "Failed with goal_relevance " << context.executive.goal_relevance;
    }

    astrocyte_destroy(astro);
}
