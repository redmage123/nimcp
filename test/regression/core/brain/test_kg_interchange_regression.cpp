/**
 * @file test_kg_interchange_regression.cpp
 * @brief Regression tests for KG Interchange module
 *
 * WHAT: Tests for export format stability, round-trip data integrity,
 *       and large KG export robustness
 * WHY:  Prevent regressions in JSON export format and data handling
 * HOW:  Export KGs of various sizes, verify JSON structure and content
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

class KGInterchangeRegressionTest : public ::testing::Test {
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
        snprintf(path, sizeof(path), "/tmp/nimcp_regression_interchange_%d%s",
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

    /** Helper: export to buffer and return as string */
    std::string export_to_string() {
        void* buffer = nullptr;
        size_t size = 0;
        int ret = kg_export_to_buffer(kg, nullptr, &buffer, &size);
        if (ret != 0 || !buffer) return "";
        std::string result((const char*)buffer, size - 1);
        nimcp_free(buffer);
        return result;
    }
};

// ============================================================================
// Export Format Stability Tests
// ============================================================================

TEST_F(KGInterchangeRegressionTest, JSONStructure_HasNodesAndEdgesArrays) {
    /* The JSON export must always have top-level "nodes" and "edges" arrays */
    add_node("stability_node", BRAIN_KG_NODE_COGNITIVE, "Stability test");

    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());

    /* Verify top-level structure: { "nodes": [...], "edges": [...] } */
    EXPECT_NE(json.find("{"), std::string::npos);
    EXPECT_NE(json.find("\"nodes\""), std::string::npos);
    EXPECT_NE(json.find("\"edges\""), std::string::npos);

    /* nodes should appear before edges in the output */
    size_t nodes_pos = json.find("\"nodes\"");
    size_t edges_pos = json.find("\"edges\"");
    EXPECT_LT(nodes_pos, edges_pos);
}

TEST_F(KGInterchangeRegressionTest, NodeJSON_ContainsRequiredFields) {
    /* Each node in JSON must have: id, name, type, state, enabled, description */
    add_node("field_check", BRAIN_KG_NODE_CORTICAL, "Field check node");

    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());

    EXPECT_NE(json.find("\"id\""), std::string::npos);
    EXPECT_NE(json.find("\"name\""), std::string::npos);
    EXPECT_NE(json.find("\"type\""), std::string::npos);
    EXPECT_NE(json.find("\"state\""), std::string::npos);
    EXPECT_NE(json.find("\"enabled\""), std::string::npos);
    EXPECT_NE(json.find("\"description\""), std::string::npos);
    EXPECT_NE(json.find("field_check"), std::string::npos);
}

TEST_F(KGInterchangeRegressionTest, EdgeJSON_ContainsRequiredFields) {
    /* Each edge in JSON must have: id, from, to, type, weight, bidirectional, description */
    brain_kg_node_id_t n1 = add_node("edge_src", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t n2 = add_node("edge_dst", BRAIN_KG_NODE_COGNITIVE);
    add_edge(n1, n2, BRAIN_KG_EDGE_SENDS_TO, "Edge field check", 0.85f);

    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());

    /* Find edges section and verify fields */
    size_t edges_pos = json.find("\"edges\"");
    ASSERT_NE(edges_pos, std::string::npos);

    std::string edges_section = json.substr(edges_pos);
    EXPECT_NE(edges_section.find("\"id\""), std::string::npos);
    EXPECT_NE(edges_section.find("\"from\""), std::string::npos);
    EXPECT_NE(edges_section.find("\"to\""), std::string::npos);
    EXPECT_NE(edges_section.find("\"type\""), std::string::npos);
    EXPECT_NE(edges_section.find("\"weight\""), std::string::npos);
    EXPECT_NE(edges_section.find("\"bidirectional\""), std::string::npos);
    EXPECT_NE(edges_section.find("\"description\""), std::string::npos);
    EXPECT_NE(edges_section.find("Edge field check"), std::string::npos);
}

TEST_F(KGInterchangeRegressionTest, EmptyKG_ProducesEmptyArrays) {
    /* Empty KG should produce { "nodes": [], "edges": [] } or equivalent */
    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());

    /* The nodes array should be present but empty (no node objects) */
    EXPECT_NE(json.find("\"nodes\""), std::string::npos);
    EXPECT_NE(json.find("\"edges\""), std::string::npos);

    /* Should not contain node-specific fields like "enabled" */
    EXPECT_EQ(json.find("\"enabled\""), std::string::npos);
}

// ============================================================================
// Round-Trip Tests
// ============================================================================

TEST_F(KGInterchangeRegressionTest, RoundTrip_ExportThenVerifyParseable) {
    /* Export to buffer, verify the JSON is well-formed enough to be valid */
    brain_kg_node_id_t n1 = add_node("round_trip_a", BRAIN_KG_NODE_CORTICAL,
                                      "First round-trip node");
    brain_kg_node_id_t n2 = add_node("round_trip_b", BRAIN_KG_NODE_SUBCORTICAL,
                                      "Second round-trip node");
    add_edge(n1, n2, BRAIN_KG_EDGE_CONNECTS_TO, "Round trip edge", 0.7f);

    std::string json = export_to_string();
    ASSERT_FALSE(json.empty());

    /* Basic JSON validation: starts with {, ends with }, balanced braces */
    size_t first_brace = json.find_first_not_of(" \t\n\r");
    ASSERT_NE(first_brace, std::string::npos);
    EXPECT_EQ(json[first_brace], '{');

    size_t last_brace = json.find_last_not_of(" \t\n\r");
    ASSERT_NE(last_brace, std::string::npos);
    EXPECT_EQ(json[last_brace], '}');

    /* Count braces - should be balanced */
    int brace_count = 0;
    int bracket_count = 0;
    for (char c : json) {
        if (c == '{') brace_count++;
        else if (c == '}') brace_count--;
        else if (c == '[') bracket_count++;
        else if (c == ']') bracket_count--;
    }
    EXPECT_EQ(brace_count, 0) << "Unbalanced curly braces in JSON output";
    EXPECT_EQ(bracket_count, 0) << "Unbalanced square brackets in JSON output";
}

TEST_F(KGInterchangeRegressionTest, RoundTrip_FileExportMatchesBufferExport) {
    /* Export to file and buffer should produce identical content */
    brain_kg_node_id_t n1 = add_node("file_buf_a", BRAIN_KG_NODE_CORTICAL);
    brain_kg_node_id_t n2 = add_node("file_buf_b", BRAIN_KG_NODE_SUBCORTICAL);
    add_edge(n1, n2, BRAIN_KG_EDGE_CONNECTS_TO, "File vs buffer", 0.6f);

    /* Export to file */
    std::string path = get_temp_path("_roundtrip.json");
    int ret = kg_export_full(kg, path.c_str(), nullptr);
    ASSERT_EQ(ret, 0);
    std::string file_content = read_file(path);
    remove_temp(path);

    /* Export to buffer */
    std::string buf_content = export_to_string();

    /* Both should contain the same data */
    ASSERT_FALSE(file_content.empty());
    ASSERT_FALSE(buf_content.empty());
    EXPECT_NE(file_content.find("file_buf_a"), std::string::npos);
    EXPECT_NE(buf_content.find("file_buf_a"), std::string::npos);
    EXPECT_NE(file_content.find("file_buf_b"), std::string::npos);
    EXPECT_NE(buf_content.find("file_buf_b"), std::string::npos);
    EXPECT_NE(file_content.find("File vs buffer"), std::string::npos);
    EXPECT_NE(buf_content.find("File vs buffer"), std::string::npos);
}

TEST_F(KGInterchangeRegressionTest, RoundTrip_ExportImportNoError) {
    /* Export to file, then import the file - should succeed without errors */
    brain_kg_node_id_t n = add_node("export_import_node", BRAIN_KG_NODE_COGNITIVE);
    (void)n;

    std::string path = get_temp_path("_rt_import.json");
    int ret = kg_export_full(kg, path.c_str(), nullptr);
    ASSERT_EQ(ret, 0);

    /* Import the file back */
    kg_import_result_t result;
    memset(&result, 0, sizeof(result));
    ret = kg_import_from_file(kg, path.c_str(), nullptr, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.errors, 0u);

    remove_temp(path);
}

// ============================================================================
// Large KG Export Tests
// ============================================================================

TEST_F(KGInterchangeRegressionTest, LargeKG_ManyNodes_DoesNotCrash) {
    /* Add many nodes and export - verify no crash or truncation */
    const int NODE_COUNT = 200;
    for (int i = 0; i < NODE_COUNT; i++) {
        char name[64];
        snprintf(name, sizeof(name), "large_kg_node_%d", i);
        brain_kg_node_id_t nid = add_node(name, BRAIN_KG_NODE_COGNITIVE, "Bulk node");
        ASSERT_NE(nid, BRAIN_KG_INVALID_NODE) << "Failed to add node " << i;
    }

    void* buffer = nullptr;
    size_t size = 0;
    int ret = kg_export_to_buffer(kg, nullptr, &buffer, &size);
    EXPECT_EQ(ret, 0);
    ASSERT_NE(buffer, nullptr);
    EXPECT_GT(size, 1000u) << "Buffer too small for " << NODE_COUNT << " nodes";

    std::string json((const char*)buffer, size - 1);

    /* Verify first and last nodes are present (no truncation) */
    EXPECT_NE(json.find("large_kg_node_0"), std::string::npos);
    EXPECT_NE(json.find("large_kg_node_199"), std::string::npos);

    /* Verify JSON is still well-formed */
    int brace_count = 0;
    for (char c : json) {
        if (c == '{') brace_count++;
        else if (c == '}') brace_count--;
    }
    EXPECT_EQ(brace_count, 0) << "Unbalanced braces in large export";

    nimcp_free(buffer);
}

TEST_F(KGInterchangeRegressionTest, LargeKG_ManyEdges_DoesNotCrash) {
    /* Add nodes with many edges and export */
    const int NODE_COUNT = 50;
    brain_kg_node_id_t nodes[50];

    for (int i = 0; i < NODE_COUNT; i++) {
        char name[64];
        snprintf(name, sizeof(name), "edge_test_node_%d", i);
        nodes[i] = add_node(name, BRAIN_KG_NODE_COGNITIVE);
        ASSERT_NE(nodes[i], BRAIN_KG_INVALID_NODE);
    }

    /* Create a chain of edges */
    for (int i = 0; i < NODE_COUNT - 1; i++) {
        char desc[64];
        snprintf(desc, sizeof(desc), "edge_%d_to_%d", i, i + 1);
        brain_kg_edge_id_t eid = add_edge(nodes[i], nodes[i + 1],
                                           BRAIN_KG_EDGE_CONNECTS_TO, desc, 0.5f);
        ASSERT_NE(eid, BRAIN_KG_INVALID_NODE) << "Failed to add edge " << i;
    }

    void* buffer = nullptr;
    size_t size = 0;
    int ret = kg_export_to_buffer(kg, nullptr, &buffer, &size);
    EXPECT_EQ(ret, 0);
    ASSERT_NE(buffer, nullptr);

    std::string json((const char*)buffer, size - 1);

    /* Verify edges are in the output */
    EXPECT_NE(json.find("edge_0_to_1"), std::string::npos);
    EXPECT_NE(json.find("edge_48_to_49"), std::string::npos);

    nimcp_free(buffer);
}

TEST_F(KGInterchangeRegressionTest, LargeKG_FileExport_DoesNotCrash) {
    /* Export large KG to file */
    const int NODE_COUNT = 100;
    for (int i = 0; i < NODE_COUNT; i++) {
        char name[64];
        snprintf(name, sizeof(name), "file_node_%d", i);
        add_node(name, BRAIN_KG_NODE_COGNITIVE);
    }

    std::string path = get_temp_path("_large.json");
    int ret = kg_export_full(kg, path.c_str(), nullptr);
    EXPECT_EQ(ret, 0);

    std::string content = read_file(path);
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("file_node_0"), std::string::npos);
    EXPECT_NE(content.find("file_node_99"), std::string::npos);

    remove_temp(path);
}

// ============================================================================
// Multiple Export Consistency Tests
// ============================================================================

TEST_F(KGInterchangeRegressionTest, MultipleExports_SameContent) {
    /* Exporting the same KG twice should produce consistent results */
    add_node("consistent_a", BRAIN_KG_NODE_CORTICAL, "First node");
    add_node("consistent_b", BRAIN_KG_NODE_SUBCORTICAL, "Second node");

    std::string json1 = export_to_string();
    std::string json2 = export_to_string();

    ASSERT_FALSE(json1.empty());
    ASSERT_FALSE(json2.empty());

    /* Both exports should contain the same nodes */
    EXPECT_NE(json1.find("consistent_a"), std::string::npos);
    EXPECT_NE(json2.find("consistent_a"), std::string::npos);
    EXPECT_NE(json1.find("consistent_b"), std::string::npos);
    EXPECT_NE(json2.find("consistent_b"), std::string::npos);
}

TEST_F(KGInterchangeRegressionTest, ExportAfterNodeRemoval_ReflectsChange) {
    /* After removing a node, export should not contain it */
    brain_kg_node_id_t n1 = add_node("keep_me", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t n2 = add_node("remove_me", BRAIN_KG_NODE_COGNITIVE);
    (void)n1;

    /* Export before removal */
    std::string before = export_to_string();
    EXPECT_NE(before.find("remove_me"), std::string::npos);

    /* Remove node and re-export */
    brain_kg_remove_node(kg, n2);
    std::string after = export_to_string();
    EXPECT_NE(after.find("keep_me"), std::string::npos);
    EXPECT_EQ(after.find("remove_me"), std::string::npos)
        << "Removed node should not appear in export";
}
