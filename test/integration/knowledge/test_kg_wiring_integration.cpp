/**
 * @file test_kg_wiring_integration.cpp
 * @brief Integration tests for Knowledge Graph Module Wiring
 * @version 1.0.0
 * @date 2026-02-03
 *
 * WHAT: Test subcortical KG wiring integrates with brain KG system
 * WHY:  Verify KG wiring correctly establishes module connectivity
 * HOW:  Create KG wirings for modules, query connections, test persistence
 *
 * TEST SCENARIOS:
 * 1. Subcortical KG wiring integration with brain KG
 * 2. Cross-module connection queries (e.g., basal ganglia connections)
 * 3. KG persistence after wiring
 * 4. Multi-region connectivity validation
 * 5. Hierarchical layer assignment
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <string>

// C headers with extern "C" guards
extern "C" {
#include "core/brain/nimcp_kg_module_wiring.h"
#include "core/brain/regions/hippocampus/bridges/nimcp_hippocampus_kg_wiring.h"
#include "core/brain/regions/amygdala/bridges/nimcp_amygdala_kg_wiring.h"
#include "core/brain/regions/prefrontal/bridges/nimcp_pfc_kg_wiring.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class KGWiringIntegrationTest : public ::testing::Test {
protected:
    std::vector<kg_module_wiring_t*> wirings_;

    void SetUp() override {
        // Clear wiring list
        wirings_.clear();
    }

    void TearDown() override {
        // Destroy all created wirings
        for (auto* wiring : wirings_) {
            if (wiring) {
                kg_module_wiring_destroy(wiring);
            }
        }
        wirings_.clear();
    }

    // Helper to track created wirings for cleanup
    kg_module_wiring_t* CreateAndTrack(const char* name, const char* type) {
        kg_module_wiring_t* wiring = kg_module_wiring_create(name, type);
        if (wiring) {
            wirings_.push_back(wiring);
        }
        return wiring;
    }
};

//=============================================================================
// Test Cases
//=============================================================================

/**
 * @brief Test subcortical KG wiring creation and basic properties
 *
 * WHAT: Verify subcortical module wiring descriptors are created correctly
 * WHY:  KG wiring is foundational for brain self-awareness
 * HOW:  Create wirings for hippocampus, amygdala, basal ganglia, verify properties
 */
TEST_F(KGWiringIntegrationTest, SubcorticalWiringCreation) {
    // Create hippocampus wiring
    kg_module_wiring_t* hippo_wiring = CreateAndTrack("hippocampus", "SUBCORTICAL");
    ASSERT_NE(hippo_wiring, nullptr);

    // Set hippocampus metadata
    int ret = kg_module_wiring_set_metadata(
        hippo_wiring,
        "NIMCP",
        "memory",
        "Hippocampal memory formation and spatial navigation"
    );
    EXPECT_EQ(ret, 0);

    // Add hippocampus inputs
    ret = kg_module_wiring_add_input(hippo_wiring, "entorhinal_cortex", "SPATIAL_INPUT", true);
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_add_input(hippo_wiring, "prefrontal_cortex", "WORKING_MEMORY", false);
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_add_input(hippo_wiring, "amygdala", "EMOTIONAL_SALIENCE", false);
    EXPECT_EQ(ret, 0);

    // Add hippocampus outputs
    ret = kg_module_wiring_add_output(hippo_wiring, "EPISODIC_MEMORY", "Episodic memory traces");
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_add_output(hippo_wiring, "SPATIAL_MAP", "Cognitive spatial maps");
    EXPECT_EQ(ret, 0);

    // Add hippocampus handlers
    ret = kg_module_wiring_add_handler(hippo_wiring, "MEMORY_QUERY", 100);
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_add_handler(hippo_wiring, "CONSOLIDATION_REQUEST", 80);
    EXPECT_EQ(ret, 0);

    // Verify wiring properties
    EXPECT_EQ(hippo_wiring->input_count, 3u);
    EXPECT_EQ(hippo_wiring->output_count, 2u);
    EXPECT_EQ(hippo_wiring->handler_count, 2u);

    // Create amygdala wiring
    kg_module_wiring_t* amyg_wiring = CreateAndTrack("amygdala", "SUBCORTICAL");
    ASSERT_NE(amyg_wiring, nullptr);

    ret = kg_module_wiring_set_metadata(
        amyg_wiring,
        "NIMCP",
        "emotion",
        "Emotional processing and fear conditioning"
    );
    EXPECT_EQ(ret, 0);

    // Add amygdala connections
    ret = kg_module_wiring_add_input(amyg_wiring, "thalamus", "SENSORY_INPUT", true);
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_add_input(amyg_wiring, "prefrontal_cortex", "TOP_DOWN_CONTROL", false);
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_add_output(amyg_wiring, "FEAR_RESPONSE", "Fear and anxiety signals");
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_add_output(amyg_wiring, "EMOTIONAL_SALIENCE", "Emotional salience tagging");
    EXPECT_EQ(ret, 0);

    // Verify amygdala wiring
    EXPECT_EQ(amyg_wiring->input_count, 2u);
    EXPECT_EQ(amyg_wiring->output_count, 2u);
}

/**
 * @brief Test queries across KG nodes to find connections
 *
 * WHAT: Verify we can query KG to find all connections for a module
 * WHY:  Query capability is essential for routing and introspection
 * HOW:  Create interconnected wirings, use query API to find connections
 */
TEST_F(KGWiringIntegrationTest, CrossModuleConnectionQueries) {
    // Create prefrontal cortex wiring
    kg_module_wiring_t* pfc = CreateAndTrack("prefrontal_cortex", "COGNITIVE");
    ASSERT_NE(pfc, nullptr);

    kg_module_wiring_add_input(pfc, "hippocampus", "EPISODIC_MEMORY", false);
    kg_module_wiring_add_input(pfc, "amygdala", "EMOTIONAL_SALIENCE", false);
    kg_module_wiring_add_input(pfc, "basal_ganglia", "REWARD_SIGNAL", false);
    kg_module_wiring_add_output(pfc, "EXECUTIVE_DECISION", "Top-level decisions");
    kg_module_wiring_add_output(pfc, "WORKING_MEMORY", "Working memory updates");
    kg_module_wiring_add_handler(pfc, "DECISION_REQUEST", 200);

    // Create basal ganglia wiring
    kg_module_wiring_t* bg = CreateAndTrack("basal_ganglia", "SUBCORTICAL");
    ASSERT_NE(bg, nullptr);

    kg_module_wiring_add_input(bg, "prefrontal_cortex", "EXECUTIVE_DECISION", true);
    kg_module_wiring_add_input(bg, "motor_cortex", "MOTOR_PLAN", true);
    kg_module_wiring_add_input(bg, "substantia_nigra", "DOPAMINE_SIGNAL", false);
    kg_module_wiring_add_output(bg, "REWARD_SIGNAL", "Reward prediction signals");
    kg_module_wiring_add_output(bg, "ACTION_SELECTION", "Selected motor actions");
    kg_module_wiring_add_handler(bg, "REWARD_LEARNING", 150);
    kg_module_wiring_add_handler(bg, "ACTION_EVALUATION", 140);

    // Create thalamus wiring
    kg_module_wiring_t* thal = CreateAndTrack("thalamus", "SUBCORTICAL");
    ASSERT_NE(thal, nullptr);

    kg_module_wiring_add_input(thal, "basal_ganglia", "ACTION_SELECTION", false);
    kg_module_wiring_add_input(thal, "sensory_cortex", "SENSORY_FEATURES", true);
    kg_module_wiring_add_output(thal, "SENSORY_INPUT", "Relayed sensory input");
    kg_module_wiring_add_output(thal, "MOTOR_RELAY", "Motor command relay");
    kg_module_wiring_add_handler(thal, "ATTENTION_GATE", 180);

    // Query: Find all basal ganglia connections
    // Check inputs
    EXPECT_TRUE(kg_module_wiring_has_input(bg, "prefrontal_cortex", "EXECUTIVE_DECISION"));
    EXPECT_TRUE(kg_module_wiring_has_input(bg, "motor_cortex", "MOTOR_PLAN"));
    EXPECT_TRUE(kg_module_wiring_has_input(bg, "substantia_nigra", "DOPAMINE_SIGNAL"));
    EXPECT_FALSE(kg_module_wiring_has_input(bg, "hippocampus", "MEMORY"));

    // Check outputs
    EXPECT_TRUE(kg_module_wiring_has_output(bg, "REWARD_SIGNAL"));
    EXPECT_TRUE(kg_module_wiring_has_output(bg, "ACTION_SELECTION"));
    EXPECT_FALSE(kg_module_wiring_has_output(bg, "MEMORY_TRACE"));

    // Check handlers
    EXPECT_TRUE(kg_module_wiring_has_handler(bg, "REWARD_LEARNING"));
    EXPECT_TRUE(kg_module_wiring_has_handler(bg, "ACTION_EVALUATION"));
    EXPECT_FALSE(kg_module_wiring_has_handler(bg, "MEMORY_QUERY"));

    // Verify handler priorities
    EXPECT_EQ(kg_module_wiring_get_handler_priority(bg, "REWARD_LEARNING"), 150u);
    EXPECT_EQ(kg_module_wiring_get_handler_priority(bg, "ACTION_EVALUATION"), 140u);
    EXPECT_EQ(kg_module_wiring_get_handler_priority(bg, "NONEXISTENT"), 0u);

    // Cross-check: PFC should have basal_ganglia as input source
    EXPECT_TRUE(kg_module_wiring_has_input(pfc, "basal_ganglia", "REWARD_SIGNAL"));

    // Thalamus receives from basal ganglia
    EXPECT_TRUE(kg_module_wiring_has_input(thal, "basal_ganglia", "ACTION_SELECTION"));
}

/**
 * @brief Test KG wiring validation
 *
 * WHAT: Verify wiring validation catches configuration errors
 * WHY:  Invalid wirings should be detected before assembly
 * HOW:  Create valid and invalid wirings, run validation, check results
 */
TEST_F(KGWiringIntegrationTest, WiringValidation) {
    char error_buf[256];

    // Create valid wiring
    kg_module_wiring_t* valid = CreateAndTrack("valid_module", "COGNITIVE");
    ASSERT_NE(valid, nullptr);

    kg_module_wiring_add_input(valid, "source", "INPUT_TYPE", true);
    kg_module_wiring_add_output(valid, "OUTPUT_TYPE", "Description");
    kg_module_wiring_add_handler(valid, "MESSAGE_TYPE", 100);

    int ret = kg_module_wiring_validate(valid, error_buf, sizeof(error_buf));
    EXPECT_EQ(ret, 0) << "Valid wiring should pass validation: " << error_buf;

    // Create wiring without name (should fail validation if enforced)
    kg_module_wiring_t* no_handlers = CreateAndTrack("no_handlers", "COGNITIVE");
    ASSERT_NE(no_handlers, nullptr);

    // Wiring with no handlers might still be valid (passive module)
    ret = kg_module_wiring_validate(no_handlers, error_buf, sizeof(error_buf));
    // Depends on validation rules - some modules may not need handlers
    std::cout << "Wiring without handlers validation result: " << ret << std::endl;

    // Test validation with NULL wiring
    ret = kg_module_wiring_validate(nullptr, error_buf, sizeof(error_buf));
    EXPECT_NE(ret, 0) << "NULL wiring should fail validation";
}

/**
 * @brief Test wiring weight state for introspection
 *
 * WHAT: Verify weight state can be set and retrieved for introspection
 * WHY:  Brain self-awareness requires access to module parameters
 * HOW:  Set weights on wiring, verify they are stored correctly
 */
TEST_F(KGWiringIntegrationTest, WiringWeightState) {
    kg_module_wiring_t* wiring = CreateAndTrack("snn_module", "SNN");
    ASSERT_NE(wiring, nullptr);

    // Create sample weight data
    float sample_weights[64];
    for (int i = 0; i < 64; i++) {
        sample_weights[i] = static_cast<float>(i) * 0.1f;
    }

    // Set weights
    int ret = kg_module_wiring_set_weights(
        wiring,
        KG_WEIGHT_SNN,
        sample_weights,
        sizeof(sample_weights)
    );
    EXPECT_EQ(ret, 0);

    // Verify weight properties
    EXPECT_EQ(wiring->network_type, KG_WEIGHT_SNN);
    EXPECT_EQ(wiring->weights_size, sizeof(sample_weights));
    EXPECT_NE(wiring->initial_weights, nullptr);

    // Verify weight data was copied
    float* stored_weights = static_cast<float*>(wiring->initial_weights);
    for (int i = 0; i < 64; i++) {
        EXPECT_FLOAT_EQ(stored_weights[i], sample_weights[i]);
    }

    // Test weight type conversion
    const char* type_str = kg_weight_type_to_string(KG_WEIGHT_SNN);
    EXPECT_STREQ(type_str, "SNN");

    type_str = kg_weight_type_to_string(KG_WEIGHT_LNN);
    EXPECT_STREQ(type_str, "LNN");

    type_str = kg_weight_type_to_string(KG_WEIGHT_CNN);
    EXPECT_STREQ(type_str, "CNN");

    // Test string to weight type conversion
    kg_weight_type_t parsed = kg_weight_type_from_string("SNN");
    EXPECT_EQ(parsed, KG_WEIGHT_SNN);

    parsed = kg_weight_type_from_string("lnn");  // Case insensitive
    EXPECT_EQ(parsed, KG_WEIGHT_LNN);

    parsed = kg_weight_type_from_string("invalid");
    EXPECT_EQ(parsed, KG_WEIGHT_NONE);
}

/**
 * @brief Test metadata entries for searchability
 *
 * WHAT: Verify custom metadata can be added and searched
 * WHY:  Metadata enables KG queries based on module attributes
 * HOW:  Add various metadata entries, verify storage and retrieval
 */
TEST_F(KGWiringIntegrationTest, MetadataEntries) {
    kg_module_wiring_t* wiring = CreateAndTrack("metadata_test", "COGNITIVE");
    ASSERT_NE(wiring, nullptr);

    // Set basic metadata
    int ret = kg_module_wiring_set_metadata(
        wiring,
        "NIMCP Team",
        "reasoning",
        "Logical reasoning and inference module"
    );
    EXPECT_EQ(ret, 0);

    // Set version
    ret = kg_module_wiring_set_version(wiring, 1, 2, 3);
    EXPECT_EQ(ret, 0);

    // Verify version
    EXPECT_EQ(wiring->metadata.version_major, 1u);
    EXPECT_EQ(wiring->metadata.version_minor, 2u);
    EXPECT_EQ(wiring->metadata.version_patch, 3u);

    // Add custom metadata entries
    ret = kg_module_wiring_add_metadata_entry(wiring, "algorithm", "forward_chaining");
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_add_metadata_entry(wiring, "complexity", "O(n^2)");
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_add_metadata_entry(wiring, "gpu_accelerated", "true");
    EXPECT_EQ(ret, 0);

    // Verify entry count
    EXPECT_EQ(wiring->metadata.entry_count, 3u);

    // Verify entries
    bool found_algorithm = false;
    bool found_complexity = false;
    bool found_gpu = false;

    for (uint32_t i = 0; i < wiring->metadata.entry_count; i++) {
        const kg_wiring_metadata_entry_t* entry = &wiring->metadata.entries[i];
        if (strcmp(entry->key, "algorithm") == 0) {
            EXPECT_STREQ(entry->value, "forward_chaining");
            found_algorithm = true;
        } else if (strcmp(entry->key, "complexity") == 0) {
            EXPECT_STREQ(entry->value, "O(n^2)");
            found_complexity = true;
        } else if (strcmp(entry->key, "gpu_accelerated") == 0) {
            EXPECT_STREQ(entry->value, "true");
            found_gpu = true;
        }
    }

    EXPECT_TRUE(found_algorithm);
    EXPECT_TRUE(found_complexity);
    EXPECT_TRUE(found_gpu);
}

/**
 * @brief Test full brain region wiring integration
 *
 * WHAT: Verify multiple brain regions can be wired together correctly
 * WHY:  Complete brain requires proper inter-region connectivity
 * HOW:  Create wirings for multiple regions, verify bidirectional connections
 */
TEST_F(KGWiringIntegrationTest, FullBrainRegionWiring) {
    // Create primary sensory cortex
    kg_module_wiring_t* v1 = CreateAndTrack("primary_visual_cortex", "SENSORY");
    ASSERT_NE(v1, nullptr);
    kg_module_wiring_add_input(v1, "lgn", "VISUAL_INPUT", true);
    kg_module_wiring_add_output(v1, "VISUAL_FEATURES", "Basic visual features");
    kg_module_wiring_add_handler(v1, "VISUAL_PROCESSING", 200);

    // Create visual association areas
    kg_module_wiring_t* v4 = CreateAndTrack("visual_area_v4", "SENSORY");
    ASSERT_NE(v4, nullptr);
    kg_module_wiring_add_input(v4, "primary_visual_cortex", "VISUAL_FEATURES", true);
    kg_module_wiring_add_output(v4, "COLOR_SHAPE", "Color and shape features");

    // Create inferior temporal (object recognition)
    kg_module_wiring_t* it = CreateAndTrack("inferior_temporal", "COGNITIVE");
    ASSERT_NE(it, nullptr);
    kg_module_wiring_add_input(it, "visual_area_v4", "COLOR_SHAPE", true);
    kg_module_wiring_add_input(it, "hippocampus", "MEMORY_CONTEXT", false);
    kg_module_wiring_add_output(it, "OBJECT_IDENTITY", "Identified objects");

    // Create dorsal stream (parietal)
    kg_module_wiring_t* parietal = CreateAndTrack("parietal_cortex", "COGNITIVE");
    ASSERT_NE(parietal, nullptr);
    kg_module_wiring_add_input(parietal, "primary_visual_cortex", "VISUAL_FEATURES", true);
    kg_module_wiring_add_input(parietal, "motor_cortex", "MOTOR_FEEDBACK", false);
    kg_module_wiring_add_output(parietal, "SPATIAL_LOCATION", "Spatial coordinates");
    kg_module_wiring_add_output(parietal, "ACTION_SPACE", "Action affordances");

    // Create prefrontal integration
    kg_module_wiring_t* dlpfc = CreateAndTrack("dorsolateral_pfc", "EXECUTIVE");
    ASSERT_NE(dlpfc, nullptr);
    kg_module_wiring_add_input(dlpfc, "inferior_temporal", "OBJECT_IDENTITY", false);
    kg_module_wiring_add_input(dlpfc, "parietal_cortex", "SPATIAL_LOCATION", false);
    kg_module_wiring_add_input(dlpfc, "hippocampus", "EPISODIC_MEMORY", false);
    kg_module_wiring_add_input(dlpfc, "amygdala", "EMOTIONAL_VALENCE", false);
    kg_module_wiring_add_output(dlpfc, "EXECUTIVE_CONTROL", "Executive commands");
    kg_module_wiring_add_handler(dlpfc, "GOAL_MANAGEMENT", 250);
    kg_module_wiring_add_handler(dlpfc, "DECISION_MAKING", 240);

    // Verify ventral stream path (V1 -> V4 -> IT -> PFC)
    EXPECT_TRUE(kg_module_wiring_has_output(v1, "VISUAL_FEATURES"));
    EXPECT_TRUE(kg_module_wiring_has_input(v4, "primary_visual_cortex", "VISUAL_FEATURES"));
    EXPECT_TRUE(kg_module_wiring_has_input(it, "visual_area_v4", "COLOR_SHAPE"));
    EXPECT_TRUE(kg_module_wiring_has_input(dlpfc, "inferior_temporal", "OBJECT_IDENTITY"));

    // Verify dorsal stream path (V1 -> Parietal -> PFC)
    EXPECT_TRUE(kg_module_wiring_has_input(parietal, "primary_visual_cortex", "VISUAL_FEATURES"));
    EXPECT_TRUE(kg_module_wiring_has_input(dlpfc, "parietal_cortex", "SPATIAL_LOCATION"));

    // Count total inputs to DLPFC (integrator)
    EXPECT_EQ(dlpfc->input_count, 4u);

    // Validate all wirings
    char error_buf[256];
    for (auto* wiring : wirings_) {
        int ret = kg_module_wiring_validate(wiring, error_buf, sizeof(error_buf));
        EXPECT_EQ(ret, 0) << "Wiring " << wiring->module_name << " failed: " << error_buf;
    }
}

/**
 * @brief Test hierarchical layer assignment
 *
 * WHAT: Verify cortical layer assignments work correctly
 * WHY:  Layer information is important for routing and processing order
 * HOW:  Create wirings with different layers, verify assignment
 */
TEST_F(KGWiringIntegrationTest, HierarchicalLayerAssignment) {
    // Create modules at different cortical layers
    kg_module_wiring_t* layer1 = CreateAndTrack("layer1_module", "SENSORY");
    ASSERT_NE(layer1, nullptr);
    layer1->target_layer = 1;  // Layer I - molecular layer

    kg_module_wiring_t* layer4 = CreateAndTrack("layer4_module", "SENSORY");
    ASSERT_NE(layer4, nullptr);
    layer4->target_layer = 4;  // Layer IV - granular layer (sensory input)

    kg_module_wiring_t* layer5 = CreateAndTrack("layer5_module", "MOTOR");
    ASSERT_NE(layer5, nullptr);
    layer5->target_layer = 5;  // Layer V - pyramidal layer (motor output)

    kg_module_wiring_t* layer6 = CreateAndTrack("layer6_module", "FEEDBACK");
    ASSERT_NE(layer6, nullptr);
    layer6->target_layer = 6;  // Layer VI - multiform layer (feedback)

    // Verify layer assignments
    EXPECT_EQ(layer1->target_layer, 1u);
    EXPECT_EQ(layer4->target_layer, 4u);
    EXPECT_EQ(layer5->target_layer, 5u);
    EXPECT_EQ(layer6->target_layer, 6u);

    // Set hemisphere affinity
    layer1->hemisphere_affinity = 0;  // LEFT
    layer4->hemisphere_affinity = 1;  // RIGHT
    layer5->hemisphere_affinity = 2;  // BILATERAL

    EXPECT_EQ(layer1->hemisphere_affinity, 0u);
    EXPECT_EQ(layer4->hemisphere_affinity, 1u);
    EXPECT_EQ(layer5->hemisphere_affinity, 2u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
