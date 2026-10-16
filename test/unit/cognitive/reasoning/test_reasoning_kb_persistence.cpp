/**
 * @file test_reasoning_kb_persistence.cpp
 * @brief Unit tests for KB persistence (export/import/validate)
 *
 * WHAT: Tests for reasoning_kb_* serialization/deserialization functions
 * WHY:  Verify correctness of binary and text KB persistence
 * HOW:  GTest suite operating on qreason_t knowledge bases
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cmath>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_kb_persistence.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/memory/nimcp_memory.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class KBPersistenceTest : public ::testing::Test {
protected:
    qreason_t ctx = nullptr;

    void SetUp() override {
        ctx = qreason_create(nullptr);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qreason_destroy(ctx);
            ctx = nullptr;
        }
    }

    /** Helper: add N facts to the KB */
    void add_facts(uint32_t count) {
        for (uint32_t i = 0; i < count && i < QREASON_MAX_VARIABLES; i++) {
            int8_t truth = (i % 2 == 0) ? QREASON_TRUE : QREASON_FALSE;
            qreason_set_fact(ctx, i, truth, 0.5f + (float)i * 0.01f);
        }
    }

    /** Helper: add N rules to the KB */
    void add_rules(uint32_t count) {
        for (uint32_t i = 0; i < count && i < QREASON_MAX_RULES; i++) {
            uint32_t ants[2] = { i % QREASON_MAX_VARIABLES,
                                 (i + 1) % QREASON_MAX_VARIABLES };
            uint32_t cons = (i + 2) % QREASON_MAX_VARIABLES;
            qreason_add_rule(ctx, ants, 2, cons, 0.8f + (float)i * 0.001f);
        }
    }
};

/*=============================================================================
 * DEFAULT CONFIG
 *===========================================================================*/

TEST_F(KBPersistenceTest, DefaultConfig) {
    kb_persistence_config_t config = reasoning_kb_default_config();
    EXPECT_EQ(config.format, KB_FORMAT_BINARY);
    EXPECT_TRUE(config.include_derived_facts);
    EXPECT_TRUE(config.include_confidences);
    EXPECT_FALSE(config.compress);
}

/*=============================================================================
 * EXPORT TESTS
 *===========================================================================*/

TEST_F(KBPersistenceTest, ExportEmptyKB) {
    uint8_t* buffer = nullptr;
    size_t size = 0;

    int rc = reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    EXPECT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);
    EXPECT_GE(size, sizeof(kb_header_t));

    /* Verify header */
    const kb_header_t* header = (const kb_header_t*)buffer;
    EXPECT_EQ(header->magic, KB_PERSISTENCE_MAGIC);
    EXPECT_EQ(header->version, KB_PERSISTENCE_VERSION);
    EXPECT_EQ(header->num_facts, 0u);
    EXPECT_EQ(header->num_rules, 0u);

    nimcp_free(buffer);
}

TEST_F(KBPersistenceTest, ExportSingleFact) {
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);

    uint8_t* buffer = nullptr;
    size_t size = 0;

    int rc = reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    EXPECT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);

    const kb_header_t* header = (const kb_header_t*)buffer;
    EXPECT_EQ(header->num_facts, 1u);
    EXPECT_EQ(header->num_rules, 0u);

    /* Verify fact entry */
    const kb_fact_entry_t* fact = (const kb_fact_entry_t*)(buffer + sizeof(kb_header_t));
    EXPECT_EQ(fact->variable_id, 0u);
    EXPECT_EQ(fact->truth_value, 1u);  /* TRUE */
    EXPECT_NEAR(fact->confidence, 0.9f, 0.001f);

    nimcp_free(buffer);
}

TEST_F(KBPersistenceTest, ExportMultipleFacts) {
    add_facts(5);

    uint8_t* buffer = nullptr;
    size_t size = 0;

    int rc = reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    EXPECT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);

    const kb_header_t* header = (const kb_header_t*)buffer;
    EXPECT_EQ(header->num_facts, 5u);

    nimcp_free(buffer);
}

TEST_F(KBPersistenceTest, ExportWithRules) {
    add_facts(3);
    add_rules(2);

    uint8_t* buffer = nullptr;
    size_t size = 0;

    int rc = reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    EXPECT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);

    const kb_header_t* header = (const kb_header_t*)buffer;
    EXPECT_EQ(header->num_facts, 3u);
    EXPECT_EQ(header->num_rules, 2u);

    nimcp_free(buffer);
}

TEST_F(KBPersistenceTest, ExportBinaryFormat) {
    add_facts(2);

    kb_persistence_config_t config = reasoning_kb_default_config();
    config.format = KB_FORMAT_BINARY;

    uint8_t* buffer = nullptr;
    size_t size = 0;

    int rc = reasoning_kb_export_to_buffer(ctx, &config, &buffer, &size);
    EXPECT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);

    /* Binary starts with magic number bytes */
    const kb_header_t* header = (const kb_header_t*)buffer;
    EXPECT_EQ(header->magic, KB_PERSISTENCE_MAGIC);

    nimcp_free(buffer);
}

TEST_F(KBPersistenceTest, ExportTextFormat) {
    add_facts(2);
    add_rules(1);

    kb_persistence_config_t config = reasoning_kb_default_config();
    config.format = KB_FORMAT_TEXT;

    uint8_t* buffer = nullptr;
    size_t size = 0;

    int rc = reasoning_kb_export_to_buffer(ctx, &config, &buffer, &size);
    EXPECT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);
    EXPECT_GT(size, 0u);

    /* Text starts with '#' */
    EXPECT_EQ(buffer[0], '#');

    /* Should contain FACTS and RULES markers */
    std::string text((const char*)buffer, size);
    EXPECT_NE(text.find("[FACTS]"), std::string::npos);
    EXPECT_NE(text.find("[RULES]"), std::string::npos);

    nimcp_free(buffer);
}

/*=============================================================================
 * IMPORT TESTS
 *===========================================================================*/

TEST_F(KBPersistenceTest, ImportFromBinaryBuffer) {
    /* Export first */
    add_facts(3);
    add_rules(1);

    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_NE(buffer, nullptr);

    /* Create fresh context and import */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    kb_import_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = reasoning_kb_import_from_buffer(ctx2, buffer, size, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.num_facts_imported, 3u);
    EXPECT_EQ(result.num_rules_imported, 1u);
    EXPECT_EQ(result.num_conflicts, 0u);
    EXPECT_EQ(result.bytes_read, size);

    qreason_destroy(ctx2);
    nimcp_free(buffer);
}

TEST_F(KBPersistenceTest, ImportFromTextBuffer) {
    /* Export text first */
    add_facts(2);
    add_rules(1);

    kb_persistence_config_t config = reasoning_kb_default_config();
    config.format = KB_FORMAT_TEXT;

    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(ctx, &config, &buffer, &size);
    ASSERT_NE(buffer, nullptr);

    /* Create fresh context and import */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    kb_import_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = reasoning_kb_import_from_buffer(ctx2, buffer, size, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.num_facts_imported, 2u);
    EXPECT_EQ(result.num_rules_imported, 1u);

    qreason_destroy(ctx2);
    nimcp_free(buffer);
}

/*=============================================================================
 * ROUNDTRIP TESTS
 *===========================================================================*/

TEST_F(KBPersistenceTest, RoundtripBinary) {
    /* Add known facts */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);
    qreason_set_fact(ctx, 1, QREASON_FALSE, 0.7f);
    qreason_set_fact(ctx, 5, QREASON_TRUE, 0.6f);

    /* Add a rule */
    uint32_t ants[2] = {0, 1};
    qreason_add_rule(ctx, ants, 2, 5, 0.85f);

    /* Export */
    uint8_t* buffer = nullptr;
    size_t size = 0;
    int rc = reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_EQ(rc, 0);

    /* Import into fresh context */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    kb_import_result_t result;
    memset(&result, 0, sizeof(result));
    rc = reasoning_kb_import_from_buffer(ctx2, buffer, size, &result);
    EXPECT_EQ(rc, 0);

    /* Verify facts match */
    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx2, 0, &conf), QREASON_TRUE);
    EXPECT_NEAR(conf, 0.9f, 0.001f);

    EXPECT_EQ(qreason_get_fact(ctx2, 1, &conf), QREASON_FALSE);
    EXPECT_NEAR(conf, 0.7f, 0.001f);

    EXPECT_EQ(qreason_get_fact(ctx2, 5, &conf), QREASON_TRUE);
    EXPECT_NEAR(conf, 0.6f, 0.001f);

    /* Verify rule */
    EXPECT_EQ(result.num_rules_imported, 1u);
    EXPECT_EQ(ctx2->kb.n_rules, 1u);
    EXPECT_EQ(ctx2->kb.rules[0].consequent, 5u);
    EXPECT_NEAR(ctx2->kb.rules[0].confidence, 0.85f, 0.001f);

    qreason_destroy(ctx2);
    nimcp_free(buffer);
}

TEST_F(KBPersistenceTest, RoundtripText) {
    qreason_set_fact(ctx, 2, QREASON_TRUE, 0.8f);
    qreason_set_fact(ctx, 3, QREASON_FALSE, 0.5f);

    uint32_t ants[1] = {2};
    qreason_add_rule(ctx, ants, 1, 3, 0.75f);

    kb_persistence_config_t config = reasoning_kb_default_config();
    config.format = KB_FORMAT_TEXT;

    uint8_t* buffer = nullptr;
    size_t size = 0;
    int rc = reasoning_kb_export_to_buffer(ctx, &config, &buffer, &size);
    ASSERT_EQ(rc, 0);

    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    kb_import_result_t result;
    memset(&result, 0, sizeof(result));
    rc = reasoning_kb_import_from_buffer(ctx2, buffer, size, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.num_facts_imported, 2u);
    EXPECT_EQ(result.num_rules_imported, 1u);

    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx2, 2, &conf), QREASON_TRUE);
    EXPECT_NEAR(conf, 0.8f, 0.01f);

    qreason_destroy(ctx2);
    nimcp_free(buffer);
}

/*=============================================================================
 * VALIDATION TESTS
 *===========================================================================*/

TEST_F(KBPersistenceTest, ValidateValidBuffer) {
    add_facts(2);

    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_NE(buffer, nullptr);

    EXPECT_EQ(reasoning_kb_validate_buffer(buffer, size), 0);

    nimcp_free(buffer);
}

TEST_F(KBPersistenceTest, ValidateInvalidMagic) {
    add_facts(1);

    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_NE(buffer, nullptr);

    /* Corrupt magic number */
    kb_header_t* header = (kb_header_t*)buffer;
    header->magic = 0xDEADBEEF;

    EXPECT_EQ(reasoning_kb_validate_buffer(buffer, size), -1);

    nimcp_free(buffer);
}

TEST_F(KBPersistenceTest, ValidateInvalidVersion) {
    add_facts(1);

    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_NE(buffer, nullptr);

    kb_header_t* header = (kb_header_t*)buffer;
    header->version = 999;

    EXPECT_EQ(reasoning_kb_validate_buffer(buffer, size), -1);

    nimcp_free(buffer);
}

TEST_F(KBPersistenceTest, ValidateCorruptedChecksum) {
    add_facts(2);

    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_NE(buffer, nullptr);

    kb_header_t* header = (kb_header_t*)buffer;
    header->checksum ^= 0xFFFFFFFF;  /* Flip all bits */

    EXPECT_EQ(reasoning_kb_validate_buffer(buffer, size), -1);

    nimcp_free(buffer);
}

TEST_F(KBPersistenceTest, ValidateNullBuffer) {
    EXPECT_EQ(reasoning_kb_validate_buffer(nullptr, 100), -1);
}

TEST_F(KBPersistenceTest, ValidateTruncatedBuffer) {
    add_facts(3);

    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_NE(buffer, nullptr);

    /* Truncate to just header (missing data) */
    EXPECT_EQ(reasoning_kb_validate_buffer(buffer, sizeof(kb_header_t)), -1);

    /* Truncate to less than header */
    EXPECT_EQ(reasoning_kb_validate_buffer(buffer, 4), -1);

    nimcp_free(buffer);
}

/*=============================================================================
 * CONFLICT RESOLUTION
 *===========================================================================*/

TEST_F(KBPersistenceTest, ImportConflictResolution) {
    /* Create source KB with fact: var 0 = TRUE, conf=0.9 */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);

    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_NE(buffer, nullptr);

    /* Create target with conflicting fact: var 0 = FALSE, conf=0.5 */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);
    qreason_set_fact(ctx2, 0, QREASON_FALSE, 0.5f);

    kb_import_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = reasoning_kb_import_from_buffer(ctx2, buffer, size, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.num_conflicts, 1u);

    /* Higher confidence (0.9 > 0.5) should win: TRUE */
    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx2, 0, &conf), QREASON_TRUE);
    EXPECT_NEAR(conf, 0.9f, 0.001f);

    qreason_destroy(ctx2);
    nimcp_free(buffer);
}

/*=============================================================================
 * CONFIG OPTIONS
 *===========================================================================*/

TEST_F(KBPersistenceTest, ExportExcludeConfidences) {
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);

    kb_persistence_config_t config = reasoning_kb_default_config();
    config.include_confidences = false;

    uint8_t* buffer = nullptr;
    size_t size = 0;
    int rc = reasoning_kb_export_to_buffer(ctx, &config, &buffer, &size);
    EXPECT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);

    /* Fact confidence should be 0.0 when excluded */
    const kb_fact_entry_t* fact = (const kb_fact_entry_t*)(buffer + sizeof(kb_header_t));
    EXPECT_NEAR(fact->confidence, 0.0f, 0.001f);

    nimcp_free(buffer);
}

/*=============================================================================
 * CHECKSUM
 *===========================================================================*/

TEST_F(KBPersistenceTest, GetChecksum) {
    add_facts(3);

    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_NE(buffer, nullptr);

    uint32_t checksum = reasoning_kb_get_checksum(buffer, size);
    EXPECT_NE(checksum, 0u);

    /* Checksum should match header */
    const kb_header_t* header = (const kb_header_t*)buffer;
    EXPECT_EQ(checksum, header->checksum);

    nimcp_free(buffer);
}

/*=============================================================================
 * FILE I/O
 *===========================================================================*/

TEST_F(KBPersistenceTest, SaveToFileAndLoadRoundtrip) {
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.85f);
    qreason_set_fact(ctx, 3, QREASON_FALSE, 0.6f);

    uint32_t ants[1] = {0};
    qreason_add_rule(ctx, ants, 1, 3, 0.9f);

    const char* filepath = "/tmp/nimcp_kb_test_roundtrip.bin";

    kb_export_result_t exp_result;
    memset(&exp_result, 0, sizeof(exp_result));
    int rc = reasoning_kb_save_to_file(ctx, filepath, nullptr, &exp_result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(exp_result.bytes_written, 0u);

    /* Load into fresh context */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    kb_import_result_t imp_result;
    memset(&imp_result, 0, sizeof(imp_result));
    rc = reasoning_kb_load_from_file(ctx2, filepath, &imp_result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(imp_result.num_facts_imported, 2u);
    EXPECT_EQ(imp_result.num_rules_imported, 1u);

    /* Verify fact */
    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx2, 0, &conf), QREASON_TRUE);
    EXPECT_NEAR(conf, 0.85f, 0.001f);

    qreason_destroy(ctx2);
    remove(filepath);
}

TEST_F(KBPersistenceTest, SaveToInvalidPath) {
    int rc = reasoning_kb_save_to_file(ctx, "/nonexistent/dir/file.bin", nullptr, nullptr);
    EXPECT_EQ(rc, -1);
}

TEST_F(KBPersistenceTest, LoadFromMissingFile) {
    int rc = reasoning_kb_load_from_file(ctx, "/tmp/nimcp_kb_nonexistent.bin", nullptr);
    EXPECT_EQ(rc, -1);
}

/*=============================================================================
 * LARGE KB
 *===========================================================================*/

TEST_F(KBPersistenceTest, LargeKB) {
    /* Add many facts and rules within limits */
    uint32_t num_facts = 30;  /* Limited by QREASON_MAX_VARIABLES=32 */
    uint32_t num_rules = 50;

    add_facts(num_facts);
    add_rules(num_rules);

    /* Export */
    uint8_t* buffer = nullptr;
    size_t size = 0;
    int rc = reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    EXPECT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);

    const kb_header_t* header = (const kb_header_t*)buffer;
    EXPECT_EQ(header->num_facts, num_facts);
    EXPECT_EQ(header->num_rules, num_rules);

    /* Import into fresh context */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    kb_import_result_t result;
    memset(&result, 0, sizeof(result));
    rc = reasoning_kb_import_from_buffer(ctx2, buffer, size, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.num_facts_imported, num_facts);
    EXPECT_EQ(result.num_rules_imported, num_rules);

    qreason_destroy(ctx2);
    nimcp_free(buffer);
}

/*=============================================================================
 * NULL ARGUMENT GUARD CLAUSES
 *===========================================================================*/

TEST_F(KBPersistenceTest, ExportNullSource) {
    uint8_t* buffer = nullptr;
    size_t size = 0;
    EXPECT_EQ(reasoning_kb_export_to_buffer(nullptr, nullptr, &buffer, &size), -1);
}

TEST_F(KBPersistenceTest, ExportNullBufferOut) {
    size_t size = 0;
    EXPECT_EQ(reasoning_kb_export_to_buffer(ctx, nullptr, nullptr, &size), -1);
}

TEST_F(KBPersistenceTest, ExportNullSizeOut) {
    uint8_t* buffer = nullptr;
    EXPECT_EQ(reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, nullptr), -1);
}

TEST_F(KBPersistenceTest, ImportNullTarget) {
    uint8_t dummy[32] = {0};
    EXPECT_EQ(reasoning_kb_import_from_buffer(nullptr, dummy, 32, nullptr), -1);
}

TEST_F(KBPersistenceTest, ImportNullBuffer) {
    EXPECT_EQ(reasoning_kb_import_from_buffer(ctx, nullptr, 100, nullptr), -1);
}

TEST_F(KBPersistenceTest, ImportZeroSize) {
    uint8_t dummy[1] = {0};
    EXPECT_EQ(reasoning_kb_import_from_buffer(ctx, dummy, 0, nullptr), -1);
}

TEST_F(KBPersistenceTest, SaveNullSource) {
    EXPECT_EQ(reasoning_kb_save_to_file(nullptr, "/tmp/test.bin", nullptr, nullptr), -1);
}

TEST_F(KBPersistenceTest, SaveNullPath) {
    EXPECT_EQ(reasoning_kb_save_to_file(ctx, nullptr, nullptr, nullptr), -1);
}

TEST_F(KBPersistenceTest, LoadNullTarget) {
    EXPECT_EQ(reasoning_kb_load_from_file(nullptr, "/tmp/test.bin", nullptr), -1);
}

TEST_F(KBPersistenceTest, LoadNullPath) {
    EXPECT_EQ(reasoning_kb_load_from_file(ctx, nullptr, nullptr), -1);
}
