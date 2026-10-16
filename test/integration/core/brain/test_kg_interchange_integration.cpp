/**
 * @file test_kg_interchange_integration.cpp
 * @brief Integration tests for KG Interchange module
 *
 * WHAT: Tests for end-to-end workflows involving KG creation, population,
 *       export, and verification of exported content
 * WHY:  Verify that the interchange module works correctly with the brain KG
 *       system in realistic scenarios
 * HOW:  Create KGs with various structures, export, and verify content integrity
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" {
#include "core/brain/nimcp_kg_interchange.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class KGInterchangeIntegrationTest : public ::testing::Test {
protected:
    brain_kg_t* kg;

    void SetUp() override {
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;
        kg_config.enable_integrity_checks = false;
        kg = brain_kg_create(&kg_config);
        ASSERT_NE(kg, nullptr);
    }

    void TearDown() override {
        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }

    brain_kg_node_id_t add_node(const char* name, brain_kg_node_type_t type,
                                const char* desc = "Test node") {
        return brain_kg_add_node(kg, name, type, desc);
    }

    brain_kg_edge_id_t add_edge(brain_kg_node_id_t from, brain_kg_node_id_t to,
                                brain_kg_edge_type_t type = BRAIN_KG_EDGE_CONNECTS_TO,
                                const char* desc = "Test edge",
                                float weight = 0.5f) {
        return brain_kg_add_edge(kg, from, to, type, desc, weight);
    }

    std::string get_temp_path(const char* suffix = ".json") {
        char path[256];
        snprintf(path, sizeof(path), "/tmp/nimcp_integration_interchange_%d%s",
                 (int)getpid(), suffix);
        return std::string(path);
    }

    void remove_temp(const std::string& path) {
        remove(path.c_str());
    }

    std::string read_file(const std::string& path) {
        FILE* fp = fopen(path.c_str(), "r");
        if (!fp) return "";
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (sz <= 0) { fclose(fp); return ""; }
        std::string content((size_t)sz, '\0');
        size_t rd = fread(&content[0], 1, (size_t)sz, fp);
        content.resize(rd);
        fclose(fp);
        return content;
    }

    std::string export_to_string() {
        void* buffer = nullptr;
        size_t size = 0;
        int ret = kg_export_to_buffer(kg, nullptr, &buffer, &size);
        if (ret != 0 || !buffer) return "";
        std::string result((const char*)buffer, size - 1);
        nimcp_free(buffer);
        return result;
    }

    /** Build a realistic brain-like KG */
    void populate_brain_modules() {
        brain_kg_node_id_t pfc = add_node("prefrontal_cortex",
            BRAIN_KG_NODE_CORTICAL, "Executive control and planning");
        brain_kg_node_id_t hipp = add_node("hippocampus",
            BRAIN_KG_NODE_SUBCORTICAL, "Memory formation and spatial navigation");
        brain_kg_node_id_t bg = add_node("basal_ganglia",
            BRAIN_KG_NODE_SUBCORTICAL, "Motor control and learning");
        brain_kg_node_id_t cereb = add_node("cerebellum",
            BRAIN_KG_NODE_BRAINSTEM, "Motor coordination and timing");
        brain_kg_node_id_t immune = add_node("immune_system",
            BRAIN_KG_NODE_SECURITY, "Threat detection and response");
        brain_kg_node_id_t visual = add_node("visual_cortex",
            BRAIN_KG_NODE_PERCEPTION, "Visual processing");

        add_edge(pfc, hipp, BRAIN_KG_EDGE_CONNECTS_TO,
                 "Memory retrieval pathway", 0.8f);
        add_edge(pfc, bg, BRAIN_KG_EDGE_MODULATES,
                 "Action selection modulation", 0.6f);
        add_edge(hipp, pfc, BRAIN_KG_EDGE_SENDS_TO,
                 "Memory recall signal", 0.7f);
        add_edge(bg, cereb, BRAIN_KG_EDGE_COORDINATES_WITH,
                 "Motor coordination", 0.9f);
        add_edge(immune, pfc, BRAIN_KG_EDGE_INHIBITS,
                 "Sickness behavior", 0.3f);
        add_edge(visual, pfc, BRAIN_KG_EDGE_SENDS_TO,
                 "Visual input to executive", 0.75f);
    }
};

// ============================================================================
// Integration: Create, Populate, Export, Verify
// ============================================================================

TEST_F(KGInterchangeIntegrationTest, CreateAndExport_AllModulesPresent) {
    populate_brain_modules();

    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());

    /* Verify all 6 modules are in the export */
    EXPECT_NE(json.find("prefrontal_cortex"), std::string::npos);
    EXPECT_NE(json.find("hippocampus"), std::string::npos);
    EXPECT_NE(json.find("basal_ganglia"), std::string::npos);
    EXPECT_NE(json.find("cerebellum"), std::string::npos);
    EXPECT_NE(json.find("immune_system"), std::string::npos);
    EXPECT_NE(json.find("visual_cortex"), std::string::npos);
}

TEST_F(KGInterchangeIntegrationTest, CreateAndExport_AllEdgesPresent) {
    populate_brain_modules();

    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());

    /* Verify edge descriptions are in the export */
    EXPECT_NE(json.find("Memory retrieval pathway"), std::string::npos);
    EXPECT_NE(json.find("Action selection modulation"), std::string::npos);
    EXPECT_NE(json.find("Memory recall signal"), std::string::npos);
    EXPECT_NE(json.find("Motor coordination"), std::string::npos);
    EXPECT_NE(json.find("Sickness behavior"), std::string::npos);
    EXPECT_NE(json.find("Visual input to executive"), std::string::npos);
}

TEST_F(KGInterchangeIntegrationTest, CreateAndExport_NodeDescriptionsPreserved) {
    populate_brain_modules();

    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());

    EXPECT_NE(json.find("Executive control and planning"), std::string::npos);
    EXPECT_NE(json.find("Memory formation and spatial navigation"), std::string::npos);
    EXPECT_NE(json.find("Motor control and learning"), std::string::npos);
    EXPECT_NE(json.find("Motor coordination and timing"), std::string::npos);
    EXPECT_NE(json.find("Threat detection and response"), std::string::npos);
    EXPECT_NE(json.find("Visual processing"), std::string::npos);
}

// ============================================================================
// Integration: Export After Modifications
// ============================================================================

TEST_F(KGInterchangeIntegrationTest, ExportAfterAddingNodes_CapturesUpdates) {
    /* Start with one node */
    add_node("original_node", BRAIN_KG_NODE_COGNITIVE, "Original");

    std::string json1 = export_to_string();
    ASSERT_FALSE(json1.empty());
    EXPECT_NE(json1.find("original_node"), std::string::npos);
    EXPECT_EQ(json1.find("new_node"), std::string::npos);

    /* Add another node */
    add_node("new_node", BRAIN_KG_NODE_PERCEPTION, "Newly added");

    std::string json2 = export_to_string();
    ASSERT_FALSE(json2.empty());
    EXPECT_NE(json2.find("original_node"), std::string::npos);
    EXPECT_NE(json2.find("new_node"), std::string::npos);
    EXPECT_NE(json2.find("Newly added"), std::string::npos);
}

TEST_F(KGInterchangeIntegrationTest, ExportAfterAddingEdges_CapturesUpdates) {
    brain_kg_node_id_t n1 = add_node("src_node", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t n2 = add_node("dst_node", BRAIN_KG_NODE_COGNITIVE);

    /* Export before edge */
    std::string before = export_to_string();
    ASSERT_FALSE(before.empty());
    EXPECT_EQ(before.find("New connection"), std::string::npos);

    /* Add edge and export again */
    add_edge(n1, n2, BRAIN_KG_EDGE_EXCITES, "New connection", 0.95f);

    std::string after = export_to_string();
    ASSERT_FALSE(after.empty());
    EXPECT_NE(after.find("New connection"), std::string::npos);
}

TEST_F(KGInterchangeIntegrationTest, ExportAfterNodeUpdate_ReflectsNewState) {
    brain_kg_node_id_t n = add_node("updatable_node", BRAIN_KG_NODE_COGNITIVE,
                                     "Initial description");

    /* Update node state */
    brain_kg_update_node(kg, n, "Updated description", BRAIN_KG_STATE_ACTIVE);

    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());
    EXPECT_NE(json.find("Updated description"), std::string::npos);
}

TEST_F(KGInterchangeIntegrationTest, ExportAfterNodeRemoval_ExcludesRemoved) {
    brain_kg_node_id_t keep = add_node("keep_node", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t remove_me = add_node("gone_node", BRAIN_KG_NODE_COGNITIVE);
    add_edge(keep, remove_me, BRAIN_KG_EDGE_CONNECTS_TO, "Doomed edge", 0.5f);

    /* Remove node (also removes its edges) */
    brain_kg_remove_node(kg, remove_me);

    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());
    EXPECT_NE(json.find("keep_node"), std::string::npos);
    EXPECT_EQ(json.find("gone_node"), std::string::npos);
    EXPECT_EQ(json.find("Doomed edge"), std::string::npos);
}

// ============================================================================
// Integration: Export to File then Read Back
// ============================================================================

TEST_F(KGInterchangeIntegrationTest, FileExportReadBack_ContentMatches) {
    populate_brain_modules();

    std::string path = get_temp_path("_readback.json");
    int ret = kg_export_full(kg, path.c_str(), nullptr);
    ASSERT_EQ(ret, 0);

    std::string content = read_file(path);
    ASSERT_FALSE(content.empty());

    /* Verify all modules present in file */
    EXPECT_NE(content.find("prefrontal_cortex"), std::string::npos);
    EXPECT_NE(content.find("hippocampus"), std::string::npos);
    EXPECT_NE(content.find("basal_ganglia"), std::string::npos);
    EXPECT_NE(content.find("cerebellum"), std::string::npos);
    EXPECT_NE(content.find("immune_system"), std::string::npos);
    EXPECT_NE(content.find("visual_cortex"), std::string::npos);

    /* Verify edges present in file */
    EXPECT_NE(content.find("Memory retrieval pathway"), std::string::npos);
    EXPECT_NE(content.find("Motor coordination"), std::string::npos);

    remove_temp(path);
}

TEST_F(KGInterchangeIntegrationTest, FileExportThenImport_Succeeds) {
    populate_brain_modules();

    std::string path = get_temp_path("_export_import.json");

    /* Export */
    int ret = kg_export_full(kg, path.c_str(), nullptr);
    ASSERT_EQ(ret, 0);

    /* Create a second KG and import into it */
    brain_kg_config_t kg_config2;
    brain_kg_default_config(&kg_config2);
    kg_config2.enable_security = false;
    kg_config2.enable_access_control = false;
    kg_config2.enable_integrity_checks = false;
    brain_kg_t* kg2 = brain_kg_create(&kg_config2);
    ASSERT_NE(kg2, nullptr);

    kg_import_result_t result;
    memset(&result, 0, sizeof(result));
    ret = kg_import_from_file(kg2, path.c_str(), nullptr, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.errors, 0u);

    brain_kg_destroy(kg2);
    remove_temp(path);
}

TEST_F(KGInterchangeIntegrationTest, BufferExportThenImport_Succeeds) {
    populate_brain_modules();

    /* Export to buffer */
    void* buffer = nullptr;
    size_t size = 0;
    int ret = kg_export_to_buffer(kg, nullptr, &buffer, &size);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(buffer, nullptr);

    /* Create a second KG and import from buffer */
    brain_kg_config_t kg_config2;
    brain_kg_default_config(&kg_config2);
    kg_config2.enable_security = false;
    kg_config2.enable_access_control = false;
    kg_config2.enable_integrity_checks = false;
    brain_kg_t* kg2 = brain_kg_create(&kg_config2);
    ASSERT_NE(kg2, nullptr);

    kg_import_result_t result;
    memset(&result, 0, sizeof(result));
    ret = kg_import_from_buffer(kg2, buffer, size, nullptr, &result);
    EXPECT_EQ(ret, 0);

    brain_kg_destroy(kg2);
    nimcp_free(buffer);
}

// ============================================================================
// Integration: Incremental Sync Workflow
// ============================================================================

TEST_F(KGInterchangeIntegrationTest, SyncExportImport_Workflow) {
    /* Create initial KG state */
    add_node("sync_initial", BRAIN_KG_NODE_COGNITIVE, "Initial state");

    /* Export sync changes */
    std::string path = get_temp_path("_sync.json");
    int ret = kg_sync_export_changes(kg, 0, path.c_str(), nullptr);
    ASSERT_EQ(ret, 0);

    /* Import sync changes into another KG */
    brain_kg_config_t kg_config2;
    brain_kg_default_config(&kg_config2);
    kg_config2.enable_security = false;
    kg_config2.enable_access_control = false;
    kg_config2.enable_integrity_checks = false;
    brain_kg_t* kg2 = brain_kg_create(&kg_config2);
    ASSERT_NE(kg2, nullptr);

    kg_import_result_t result;
    memset(&result, 0, sizeof(result));
    ret = kg_sync_import_changes(kg2, path.c_str(), nullptr, &result);
    EXPECT_EQ(ret, 0);

    brain_kg_destroy(kg2);
    remove_temp(path);
}

// ============================================================================
// Integration: Mixed Node Types and Edge Types
// ============================================================================

TEST_F(KGInterchangeIntegrationTest, MixedTypes_AllNodeTypesExported) {
    add_node("core_mod", BRAIN_KG_NODE_CORE, "Core module");
    add_node("cortical_mod", BRAIN_KG_NODE_CORTICAL, "Cortical module");
    add_node("subcortical_mod", BRAIN_KG_NODE_SUBCORTICAL, "Subcortical module");
    add_node("brainstem_mod", BRAIN_KG_NODE_BRAINSTEM, "Brainstem module");
    add_node("cognitive_mod", BRAIN_KG_NODE_COGNITIVE, "Cognitive module");
    add_node("perception_mod", BRAIN_KG_NODE_PERCEPTION, "Perception module");
    add_node("plasticity_mod", BRAIN_KG_NODE_PLASTICITY, "Plasticity module");
    add_node("training_mod", BRAIN_KG_NODE_TRAINING, "Training module");
    add_node("security_mod", BRAIN_KG_NODE_SECURITY, "Security module");
    add_node("integration_mod", BRAIN_KG_NODE_INTEGRATION, "Integration module");
    add_node("coordinator_mod", BRAIN_KG_NODE_COORDINATOR, "Coordinator module");
    add_node("utility_mod", BRAIN_KG_NODE_UTILITY, "Utility module");

    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());

    /* Verify all node type names appear */
    EXPECT_NE(json.find("core_mod"), std::string::npos);
    EXPECT_NE(json.find("cortical_mod"), std::string::npos);
    EXPECT_NE(json.find("subcortical_mod"), std::string::npos);
    EXPECT_NE(json.find("brainstem_mod"), std::string::npos);
    EXPECT_NE(json.find("cognitive_mod"), std::string::npos);
    EXPECT_NE(json.find("perception_mod"), std::string::npos);
    EXPECT_NE(json.find("plasticity_mod"), std::string::npos);
    EXPECT_NE(json.find("training_mod"), std::string::npos);
    EXPECT_NE(json.find("security_mod"), std::string::npos);
    EXPECT_NE(json.find("integration_mod"), std::string::npos);
    EXPECT_NE(json.find("coordinator_mod"), std::string::npos);
    EXPECT_NE(json.find("utility_mod"), std::string::npos);
}

TEST_F(KGInterchangeIntegrationTest, MixedTypes_AllEdgeTypesExported) {
    brain_kg_node_id_t n1 = add_node("src", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t n2 = add_node("dst1", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t n3 = add_node("dst2", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t n4 = add_node("dst3", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t n5 = add_node("dst4", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t n6 = add_node("dst5", BRAIN_KG_NODE_COGNITIVE);

    add_edge(n1, n2, BRAIN_KG_EDGE_CONNECTS_TO, "conn_edge", 0.5f);
    add_edge(n1, n3, BRAIN_KG_EDGE_SENDS_TO, "send_edge", 0.6f);
    add_edge(n1, n4, BRAIN_KG_EDGE_MODULATES, "mod_edge", 0.7f);
    add_edge(n1, n5, BRAIN_KG_EDGE_EXCITES, "excite_edge", 0.8f);
    add_edge(n1, n6, BRAIN_KG_EDGE_INHIBITS, "inhibit_edge", 0.9f);

    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());

    EXPECT_NE(json.find("conn_edge"), std::string::npos);
    EXPECT_NE(json.find("send_edge"), std::string::npos);
    EXPECT_NE(json.find("mod_edge"), std::string::npos);
    EXPECT_NE(json.find("excite_edge"), std::string::npos);
    EXPECT_NE(json.find("inhibit_edge"), std::string::npos);
}

// ============================================================================
// Integration: Ontology Workflow
// ============================================================================

TEST_F(KGInterchangeIntegrationTest, OntologyWorkflow_ImportLinkLookup) {
    /* Full ontology workflow: import, link, lookup */
    brain_kg_node_id_t n = add_node("concept_node", BRAIN_KG_NODE_COGNITIVE,
                                     "A concept to link");

    /* Import ontology (stub - succeeds immediately) */
    int ret = kg_ontology_import(kg, KG_ONTOLOGY_WORDNET, "/tmp/wordnet.dat");
    EXPECT_EQ(ret, 0);

    /* Link node to ontology entry */
    ret = kg_ontology_link(kg, n, KG_ONTOLOGY_WORDNET, "synset_01234567_n");
    EXPECT_EQ(ret, 0);

    /* Lookup term in ontology */
    brain_kg_node_id_t matches[10];
    uint32_t count = 0;
    ret = kg_ontology_lookup(kg, KG_ONTOLOGY_WORDNET, "concept", matches, 10, &count);
    EXPECT_EQ(ret, 0);
    /* Stub returns 0 matches, which is expected */
    EXPECT_EQ(count, 0u);
}

// ============================================================================
// Integration: Export with Explicit Options
// ============================================================================

TEST_F(KGInterchangeIntegrationTest, ExportWithOptions_JSONFormat) {
    populate_brain_modules();

    kg_export_options_t opts;
    kg_export_options_default(&opts);
    opts.format = KG_FORMAT_JSON;
    opts.include_metadata = true;
    opts.include_weights = true;

    void* buffer = nullptr;
    size_t size = 0;
    int ret = kg_export_to_buffer(kg, &opts, &buffer, &size);
    EXPECT_EQ(ret, 0);
    ASSERT_NE(buffer, nullptr);

    std::string json((const char*)buffer, size - 1);
    EXPECT_NE(json.find("prefrontal_cortex"), std::string::npos);
    EXPECT_NE(json.find("\"weight\""), std::string::npos);

    nimcp_free(buffer);
}

TEST_F(KGInterchangeIntegrationTest, ExportToFileWithOptions_Succeeds) {
    populate_brain_modules();

    kg_export_options_t opts;
    kg_export_options_default(&opts);
    opts.format = KG_FORMAT_JSON;

    std::string path = get_temp_path("_opts.json");
    int ret = kg_export_full(kg, path.c_str(), &opts);
    EXPECT_EQ(ret, 0);

    std::string content = read_file(path);
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("prefrontal_cortex"), std::string::npos);

    remove_temp(path);
}
