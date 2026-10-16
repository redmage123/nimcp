/**
 * @file test_kg_interchange.cpp
 * @brief Unit tests for KG Interchange module (import/export/interoperability)
 *
 * WHAT: Tests for kg_export_full(), kg_export_to_buffer(), default config
 *       functions, utility functions, and NULL-safety guards
 * WHY:  Zero tests existed for this module; need coverage for all public APIs
 * HOW:  GoogleTest with brain_kg_t fixture, verify JSON output structure
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

class KGInterchangeTest : public ::testing::Test {
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

    /** Helper to add a node to the KG */
    brain_kg_node_id_t add_node(const char* name, brain_kg_node_type_t type,
                                const char* desc = "Test node") {
        return brain_kg_add_node(kg, name, type, desc);
    }

    /** Helper to add an edge between two nodes */
    brain_kg_edge_id_t add_edge(brain_kg_node_id_t from, brain_kg_node_id_t to,
                                brain_kg_edge_type_t type = BRAIN_KG_EDGE_CONNECTS_TO,
                                const char* desc = "Test edge",
                                float weight = 0.5f) {
        return brain_kg_add_edge(kg, from, to, type, desc, weight);
    }

    /** Helper to get temp file path for export tests */
    std::string get_temp_path(const char* suffix = ".json") {
        char path[256];
        snprintf(path, sizeof(path), "/tmp/nimcp_test_interchange_%d%s",
                 (int)getpid(), suffix);
        return std::string(path);
    }

    /** Helper to clean up a temp file */
    void remove_temp(const std::string& path) {
        remove(path.c_str());
    }

    /** Helper to read file contents into a string */
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
};

// ============================================================================
// Default Configuration Tests
// ============================================================================

TEST_F(KGInterchangeTest, ExportOptionsDefault_Success) {
    kg_export_options_t opts;
    int ret = kg_export_options_default(&opts);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(opts.format, KG_FORMAT_JSON);
    EXPECT_TRUE(opts.include_metadata);
    EXPECT_FALSE(opts.include_weights);
    EXPECT_FALSE(opts.include_history);
    EXPECT_FALSE(opts.compress_output);
    EXPECT_FALSE(opts.encrypt_output);
    EXPECT_EQ(opts.node_filter, nullptr);
    EXPECT_EQ(opts.edge_filter, nullptr);
    EXPECT_EQ(opts.max_depth, 0u);
}

TEST_F(KGInterchangeTest, ExportOptionsDefault_NullReturnsError) {
    int ret = kg_export_options_default(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ImportOptionsDefault_Success) {
    kg_import_options_t opts;
    int ret = kg_import_options_default(&opts);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(opts.format, KG_FORMAT_JSON);
    EXPECT_TRUE(opts.merge_existing);
    EXPECT_FALSE(opts.overwrite_conflicts);
    EXPECT_TRUE(opts.validate_schema);
    EXPECT_FALSE(opts.dry_run);
    EXPECT_EQ(opts.id_mapping_file, nullptr);
    EXPECT_EQ(opts.default_classification, nullptr);
}

TEST_F(KGInterchangeTest, ImportOptionsDefault_NullReturnsError) {
    int ret = kg_import_options_default(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ImportResultFree_NullSafe) {
    kg_import_result_free(nullptr);  // Should not crash
}

TEST_F(KGInterchangeTest, ImportResultFree_WithErrorLog) {
    kg_import_result_t result;
    memset(&result, 0, sizeof(result));
    result.error_log_path = (char*)nimcp_malloc(32);
    ASSERT_NE(result.error_log_path, nullptr);
    strcpy(result.error_log_path, "test_log.txt");
    kg_import_result_free(&result);
    EXPECT_EQ(result.error_log_path, nullptr);
}

TEST_F(KGInterchangeTest, ImportResultFree_NullErrorLog) {
    kg_import_result_t result;
    memset(&result, 0, sizeof(result));
    result.error_log_path = nullptr;
    kg_import_result_free(&result);  // Should not crash
}

// ============================================================================
// kg_export_to_buffer Tests
// ============================================================================

TEST_F(KGInterchangeTest, ExportToBuffer_EmptyKG_ReturnsValidJSON) {
    void* buffer = nullptr;
    size_t size = 0;

    int ret = kg_export_to_buffer(kg, nullptr, &buffer, &size);
    EXPECT_EQ(ret, 0);
    ASSERT_NE(buffer, nullptr);
    EXPECT_GT(size, 0u);

    std::string json((const char*)buffer, size - 1);  // -1 to skip null terminator
    EXPECT_NE(json.find("\"nodes\""), std::string::npos);
    EXPECT_NE(json.find("\"edges\""), std::string::npos);

    nimcp_free(buffer);
}

TEST_F(KGInterchangeTest, ExportToBuffer_WithNodes_ContainsNodeData) {
    brain_kg_node_id_t n1 = add_node("prefrontal_cortex", BRAIN_KG_NODE_CORTICAL,
                                      "Executive control");
    brain_kg_node_id_t n2 = add_node("hippocampus", BRAIN_KG_NODE_SUBCORTICAL,
                                      "Memory formation");
    ASSERT_NE(n1, BRAIN_KG_INVALID_NODE);
    ASSERT_NE(n2, BRAIN_KG_INVALID_NODE);

    void* buffer = nullptr;
    size_t size = 0;

    int ret = kg_export_to_buffer(kg, nullptr, &buffer, &size);
    EXPECT_EQ(ret, 0);
    ASSERT_NE(buffer, nullptr);

    std::string json((const char*)buffer, size - 1);
    EXPECT_NE(json.find("prefrontal_cortex"), std::string::npos);
    EXPECT_NE(json.find("hippocampus"), std::string::npos);
    EXPECT_NE(json.find("Executive control"), std::string::npos);
    EXPECT_NE(json.find("Memory formation"), std::string::npos);

    nimcp_free(buffer);
}

TEST_F(KGInterchangeTest, ExportToBuffer_WithEdges_ContainsEdgeData) {
    brain_kg_node_id_t n1 = add_node("node_a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t n2 = add_node("node_b", BRAIN_KG_NODE_COGNITIVE);
    ASSERT_NE(n1, BRAIN_KG_INVALID_NODE);
    ASSERT_NE(n2, BRAIN_KG_INVALID_NODE);

    brain_kg_edge_id_t e = add_edge(n1, n2, BRAIN_KG_EDGE_SENDS_TO,
                                    "Signal pathway", 0.75f);
    ASSERT_NE(e, BRAIN_KG_INVALID_NODE);

    void* buffer = nullptr;
    size_t size = 0;

    int ret = kg_export_to_buffer(kg, nullptr, &buffer, &size);
    EXPECT_EQ(ret, 0);
    ASSERT_NE(buffer, nullptr);

    std::string json((const char*)buffer, size - 1);
    EXPECT_NE(json.find("\"edges\""), std::string::npos);
    EXPECT_NE(json.find("Signal pathway"), std::string::npos);
    EXPECT_NE(json.find("\"from\""), std::string::npos);
    EXPECT_NE(json.find("\"to\""), std::string::npos);

    nimcp_free(buffer);
}

TEST_F(KGInterchangeTest, ExportToBuffer_NullKG_ReturnsError) {
    void* buffer = nullptr;
    size_t size = 0;
    int ret = kg_export_to_buffer(nullptr, nullptr, &buffer, &size);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ExportToBuffer_NullBuffer_ReturnsError) {
    size_t size = 0;
    int ret = kg_export_to_buffer(kg, nullptr, nullptr, &size);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ExportToBuffer_NullSize_ReturnsError) {
    void* buffer = nullptr;
    int ret = kg_export_to_buffer(kg, nullptr, &buffer, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ExportToBuffer_BufferSizeReported) {
    add_node("test_node", BRAIN_KG_NODE_COGNITIVE, "A test node");

    void* buffer = nullptr;
    size_t size = 0;

    int ret = kg_export_to_buffer(kg, nullptr, &buffer, &size);
    EXPECT_EQ(ret, 0);
    ASSERT_NE(buffer, nullptr);

    /* Size should include null terminator */
    size_t json_len = strlen((const char*)buffer);
    EXPECT_EQ(size, json_len + 1);

    nimcp_free(buffer);
}

TEST_F(KGInterchangeTest, ExportToBuffer_WithExplicitOptions) {
    add_node("opt_node", BRAIN_KG_NODE_CORE);

    kg_export_options_t opts;
    kg_export_options_default(&opts);
    opts.format = KG_FORMAT_JSON;
    opts.include_metadata = true;

    void* buffer = nullptr;
    size_t size = 0;

    int ret = kg_export_to_buffer(kg, &opts, &buffer, &size);
    EXPECT_EQ(ret, 0);
    ASSERT_NE(buffer, nullptr);

    std::string json((const char*)buffer, size - 1);
    EXPECT_NE(json.find("opt_node"), std::string::npos);

    nimcp_free(buffer);
}

// ============================================================================
// kg_export_full Tests
// ============================================================================

TEST_F(KGInterchangeTest, ExportFull_EmptyKG_WritesValidJSON) {
    std::string path = get_temp_path();

    int ret = kg_export_full(kg, path.c_str(), nullptr);
    EXPECT_EQ(ret, 0);

    std::string content = read_file(path);
    EXPECT_NE(content.find("\"nodes\""), std::string::npos);
    EXPECT_NE(content.find("\"edges\""), std::string::npos);

    remove_temp(path);
}

TEST_F(KGInterchangeTest, ExportFull_WithNodesAndEdges_WritesContent) {
    brain_kg_node_id_t n1 = add_node("export_node_a", BRAIN_KG_NODE_CORTICAL);
    brain_kg_node_id_t n2 = add_node("export_node_b", BRAIN_KG_NODE_SUBCORTICAL);
    add_edge(n1, n2, BRAIN_KG_EDGE_CONNECTS_TO, "Test connection", 0.9f);

    std::string path = get_temp_path();
    int ret = kg_export_full(kg, path.c_str(), nullptr);
    EXPECT_EQ(ret, 0);

    std::string content = read_file(path);
    EXPECT_NE(content.find("export_node_a"), std::string::npos);
    EXPECT_NE(content.find("export_node_b"), std::string::npos);
    EXPECT_NE(content.find("Test connection"), std::string::npos);

    remove_temp(path);
}

TEST_F(KGInterchangeTest, ExportFull_NullKG_ReturnsError) {
    std::string path = get_temp_path();
    int ret = kg_export_full(nullptr, path.c_str(), nullptr);
    EXPECT_EQ(ret, -1);
    remove_temp(path);
}

TEST_F(KGInterchangeTest, ExportFull_NullPath_ReturnsError) {
    int ret = kg_export_full(kg, nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ExportFull_InvalidPath_ReturnsError) {
    int ret = kg_export_full(kg, "/nonexistent/dir/file.json", nullptr);
    EXPECT_EQ(ret, -1);
}

// ============================================================================
// kg_export_subgraph Tests
// ============================================================================

TEST_F(KGInterchangeTest, ExportSubgraph_NullKG_ReturnsError) {
    int ret = kg_export_subgraph(nullptr, 0, 2, "/tmp/test.json", nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ExportSubgraph_NullPath_ReturnsError) {
    int ret = kg_export_subgraph(kg, 0, 2, nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ExportSubgraph_ValidParams_Succeeds) {
    brain_kg_node_id_t n = add_node("sub_root", BRAIN_KG_NODE_CORE);
    std::string path = get_temp_path("_subgraph.json");

    int ret = kg_export_subgraph(kg, n, 3, path.c_str(), nullptr);
    EXPECT_EQ(ret, 0);

    std::string content = read_file(path);
    EXPECT_FALSE(content.empty());

    remove_temp(path);
}

// ============================================================================
// Import Operations Tests
// ============================================================================

TEST_F(KGInterchangeTest, ImportFromFile_NullKG_ReturnsError) {
    int ret = kg_import_from_file(nullptr, "/tmp/test.json", nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ImportFromFile_NullPath_ReturnsError) {
    int ret = kg_import_from_file(kg, nullptr, nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ImportFromFile_NonexistentFile_ReturnsError) {
    kg_import_result_t result;
    memset(&result, 0, sizeof(result));
    int ret = kg_import_from_file(kg, "/nonexistent/file.json", nullptr, &result);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(result.errors, 1u);
}

TEST_F(KGInterchangeTest, ImportFromFile_ValidFile_Succeeds) {
    /* First export to create a valid file */
    std::string path = get_temp_path("_import.json");
    kg_export_full(kg, path.c_str(), nullptr);

    kg_import_result_t result;
    memset(&result, 0, sizeof(result));
    int ret = kg_import_from_file(kg, path.c_str(), nullptr, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.errors, 0u);
    EXPECT_GE(result.duration_ms, 0u);

    remove_temp(path);
}

TEST_F(KGInterchangeTest, ImportFromBuffer_NullKG_ReturnsError) {
    const char* data = "{}";
    int ret = kg_import_from_buffer(nullptr, data, strlen(data), nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ImportFromBuffer_NullBuffer_ReturnsError) {
    int ret = kg_import_from_buffer(kg, nullptr, 10, nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ImportFromBuffer_ZeroSize_ReturnsError) {
    const char* data = "{}";
    int ret = kg_import_from_buffer(kg, data, 0, nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ImportFromBuffer_ValidData_Succeeds) {
    const char* data = "{\"nodes\":[],\"edges\":[]}";
    kg_import_result_t result;
    memset(&result, 0, sizeof(result));
    int ret = kg_import_from_buffer(kg, data, strlen(data), nullptr, &result);
    EXPECT_EQ(ret, 0);
}

TEST_F(KGInterchangeTest, ImportValidate_NullPath_ReturnsError) {
    int ret = kg_import_validate(nullptr, nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, ImportValidate_ValidPath_Succeeds) {
    /* Create a valid export file first */
    std::string path = get_temp_path("_validate.json");
    kg_export_full(kg, path.c_str(), nullptr);

    kg_import_result_t preview;
    memset(&preview, 0, sizeof(preview));
    int ret = kg_import_validate(path.c_str(), nullptr, &preview);
    EXPECT_EQ(ret, 0);

    remove_temp(path);
}

// ============================================================================
// Incremental Sync Tests
// ============================================================================

TEST_F(KGInterchangeTest, SyncExportChanges_NullKG_ReturnsError) {
    int ret = kg_sync_export_changes(nullptr, 0, "/tmp/sync.json", nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, SyncExportChanges_NullPath_ReturnsError) {
    int ret = kg_sync_export_changes(kg, 0, nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, SyncExportChanges_ValidParams_Succeeds) {
    add_node("sync_node", BRAIN_KG_NODE_COGNITIVE);
    std::string path = get_temp_path("_sync.json");

    int ret = kg_sync_export_changes(kg, 0, path.c_str(), nullptr);
    EXPECT_EQ(ret, 0);

    std::string content = read_file(path);
    EXPECT_NE(content.find("sync_node"), std::string::npos);

    remove_temp(path);
}

TEST_F(KGInterchangeTest, SyncImportChanges_NullKG_ReturnsError) {
    int ret = kg_sync_import_changes(nullptr, "/tmp/sync.json", nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, SyncImportChanges_NullPath_ReturnsError) {
    int ret = kg_sync_import_changes(kg, nullptr, nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

// ============================================================================
// Ontology API Tests
// ============================================================================

TEST_F(KGInterchangeTest, OntologyImport_NullKG_ReturnsError) {
    int ret = kg_ontology_import(nullptr, KG_ONTOLOGY_WORDNET, "/tmp/wordnet");
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, OntologyImport_NullPath_ReturnsError) {
    int ret = kg_ontology_import(kg, KG_ONTOLOGY_WORDNET, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, OntologyImport_ValidParams_Succeeds) {
    int ret = kg_ontology_import(kg, KG_ONTOLOGY_CONCEPTNET, "/tmp/conceptnet");
    EXPECT_EQ(ret, 0);
}

TEST_F(KGInterchangeTest, OntologyLink_NullKG_ReturnsError) {
    int ret = kg_ontology_link(nullptr, 0, KG_ONTOLOGY_WIKIDATA, "Q12345");
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, OntologyLink_NullExternalID_ReturnsError) {
    int ret = kg_ontology_link(kg, 0, KG_ONTOLOGY_WIKIDATA, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KGInterchangeTest, OntologyLink_ValidParams_Succeeds) {
    brain_kg_node_id_t n = add_node("linked_node", BRAIN_KG_NODE_COGNITIVE);
    int ret = kg_ontology_link(kg, n, KG_ONTOLOGY_WIKIDATA, "Q12345");
    EXPECT_EQ(ret, 0);
}

TEST_F(KGInterchangeTest, OntologyLookup_NullParams_ReturnsError) {
    brain_kg_node_id_t matches[10];
    uint32_t count = 0;

    EXPECT_EQ(kg_ontology_lookup(nullptr, KG_ONTOLOGY_WORDNET, "test", matches, 10, &count), -1);
    EXPECT_EQ(kg_ontology_lookup(kg, KG_ONTOLOGY_WORDNET, nullptr, matches, 10, &count), -1);
    EXPECT_EQ(kg_ontology_lookup(kg, KG_ONTOLOGY_WORDNET, "test", nullptr, 10, &count), -1);
    EXPECT_EQ(kg_ontology_lookup(kg, KG_ONTOLOGY_WORDNET, "test", matches, 0, &count), -1);
    EXPECT_EQ(kg_ontology_lookup(kg, KG_ONTOLOGY_WORDNET, "test", matches, 10, nullptr), -1);
}

TEST_F(KGInterchangeTest, OntologyLookup_ValidParams_ReturnsZeroMatches) {
    brain_kg_node_id_t matches[10];
    uint32_t count = 99;  /* Should be reset to 0 */

    int ret = kg_ontology_lookup(kg, KG_ONTOLOGY_WORDNET, "dog", matches, 10, &count);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 0u);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(KGInterchangeTest, FormatToString_AllFormats) {
    EXPECT_STREQ(kg_export_format_to_string(KG_FORMAT_JSON), "JSON");
    EXPECT_STREQ(kg_export_format_to_string(KG_FORMAT_JSON_LD), "JSON_LD");
    EXPECT_STREQ(kg_export_format_to_string(KG_FORMAT_RDF_XML), "RDF_XML");
    EXPECT_STREQ(kg_export_format_to_string(KG_FORMAT_RDF_TURTLE), "RDF_TURTLE");
    EXPECT_STREQ(kg_export_format_to_string(KG_FORMAT_RDF_NTRIPLES), "RDF_NTRIPLES");
    EXPECT_STREQ(kg_export_format_to_string(KG_FORMAT_GRAPHML), "GRAPHML");
    EXPECT_STREQ(kg_export_format_to_string(KG_FORMAT_GEXF), "GEXF");
    EXPECT_STREQ(kg_export_format_to_string(KG_FORMAT_CSV), "CSV");
    EXPECT_STREQ(kg_export_format_to_string(KG_FORMAT_PARQUET), "PARQUET");
    EXPECT_STREQ(kg_export_format_to_string(KG_FORMAT_NIMCP_BINARY), "NIMCP_BINARY");
}

TEST_F(KGInterchangeTest, FormatToString_Invalid_ReturnsUnknown) {
    EXPECT_STREQ(kg_export_format_to_string((kg_export_format_t)999), "UNKNOWN");
}

TEST_F(KGInterchangeTest, OntologyTypeToString_AllTypes) {
    EXPECT_STREQ(kg_ontology_type_to_string(KG_ONTOLOGY_WORDNET), "WordNet");
    EXPECT_STREQ(kg_ontology_type_to_string(KG_ONTOLOGY_CONCEPTNET), "ConceptNet");
    EXPECT_STREQ(kg_ontology_type_to_string(KG_ONTOLOGY_WIKIDATA), "Wikidata");
    EXPECT_STREQ(kg_ontology_type_to_string(KG_ONTOLOGY_DBPEDIA), "DBpedia");
    EXPECT_STREQ(kg_ontology_type_to_string(KG_ONTOLOGY_SCHEMA_ORG), "Schema.org");
    EXPECT_STREQ(kg_ontology_type_to_string(KG_ONTOLOGY_CUSTOM), "Custom");
}

TEST_F(KGInterchangeTest, OntologyTypeToString_Invalid_ReturnsUnknown) {
    EXPECT_STREQ(kg_ontology_type_to_string((kg_ontology_type_t)999), "UNKNOWN");
}

TEST_F(KGInterchangeTest, InferFormatFromPath_AllExtensions) {
    EXPECT_EQ(kg_infer_format_from_path("data.json"), KG_FORMAT_JSON);
    EXPECT_EQ(kg_infer_format_from_path("data.jsonld"), KG_FORMAT_JSON_LD);
    EXPECT_EQ(kg_infer_format_from_path("data.rdf"), KG_FORMAT_RDF_XML);
    EXPECT_EQ(kg_infer_format_from_path("data.xml"), KG_FORMAT_RDF_XML);
    EXPECT_EQ(kg_infer_format_from_path("data.ttl"), KG_FORMAT_RDF_TURTLE);
    EXPECT_EQ(kg_infer_format_from_path("data.nt"), KG_FORMAT_RDF_NTRIPLES);
    EXPECT_EQ(kg_infer_format_from_path("data.graphml"), KG_FORMAT_GRAPHML);
    EXPECT_EQ(kg_infer_format_from_path("data.gexf"), KG_FORMAT_GEXF);
    EXPECT_EQ(kg_infer_format_from_path("data.csv"), KG_FORMAT_CSV);
    EXPECT_EQ(kg_infer_format_from_path("data.parquet"), KG_FORMAT_PARQUET);
    EXPECT_EQ(kg_infer_format_from_path("data.nimcp"), KG_FORMAT_NIMCP_BINARY);
    EXPECT_EQ(kg_infer_format_from_path("data.bin"), KG_FORMAT_NIMCP_BINARY);
}

TEST_F(KGInterchangeTest, InferFormatFromPath_UnknownExtension_DefaultsToJSON) {
    EXPECT_EQ(kg_infer_format_from_path("data.xyz"), KG_FORMAT_JSON);
}

TEST_F(KGInterchangeTest, InferFormatFromPath_NullPath_DefaultsToJSON) {
    EXPECT_EQ(kg_infer_format_from_path(nullptr), KG_FORMAT_JSON);
}

TEST_F(KGInterchangeTest, InferFormatFromPath_NoExtension_DefaultsToJSON) {
    EXPECT_EQ(kg_infer_format_from_path("datafile"), KG_FORMAT_JSON);
}

TEST_F(KGInterchangeTest, FormatExtension_AllFormats) {
    EXPECT_STREQ(kg_format_extension(KG_FORMAT_JSON), ".json");
    EXPECT_STREQ(kg_format_extension(KG_FORMAT_JSON_LD), ".jsonld");
    EXPECT_STREQ(kg_format_extension(KG_FORMAT_RDF_XML), ".rdf");
    EXPECT_STREQ(kg_format_extension(KG_FORMAT_RDF_TURTLE), ".ttl");
    EXPECT_STREQ(kg_format_extension(KG_FORMAT_RDF_NTRIPLES), ".nt");
    EXPECT_STREQ(kg_format_extension(KG_FORMAT_GRAPHML), ".graphml");
    EXPECT_STREQ(kg_format_extension(KG_FORMAT_GEXF), ".gexf");
    EXPECT_STREQ(kg_format_extension(KG_FORMAT_CSV), ".csv");
    EXPECT_STREQ(kg_format_extension(KG_FORMAT_PARQUET), ".parquet");
    EXPECT_STREQ(kg_format_extension(KG_FORMAT_NIMCP_BINARY), ".nimcp");
}

TEST_F(KGInterchangeTest, FormatExtension_Invalid_DefaultsToJson) {
    EXPECT_STREQ(kg_format_extension((kg_export_format_t)999), ".json");
}
