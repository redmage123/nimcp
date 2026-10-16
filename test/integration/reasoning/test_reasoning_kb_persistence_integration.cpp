/**
 * @file test_reasoning_kb_persistence_integration.cpp
 * @brief Integration tests for KB persistence with quantum reasoning
 *
 * WHAT: Tests KB persistence interacting with live quantum reasoning engine
 * WHY:  Verify persistence works correctly across engine recreation,
 *       preserves confidences, handles derived facts, and merges KBs
 * HOW:  GTest suite using qreason_t and file I/O
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <string>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_kb_persistence.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/memory/nimcp_memory.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class KBPersistenceIntegration : public ::testing::Test {
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
};

/*=============================================================================
 * INTEGRATION TESTS
 *===========================================================================*/

TEST_F(KBPersistenceIntegration, PersistWithQuantumReasoning) {
    /* Add facts and rules to quantum reasoning KB */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.95f);
    qreason_set_fact(ctx, 1, QREASON_TRUE, 0.85f);
    qreason_set_fact(ctx, 2, QREASON_FALSE, 0.7f);

    uint32_t ants[2] = {0, 1};
    qreason_add_rule(ctx, ants, 2, 3, 0.9f);

    uint32_t ants2[1] = {3};
    qreason_add_rule(ctx, ants2, 1, 4, 0.8f);

    /* Export */
    uint8_t* buffer = nullptr;
    size_t size = 0;
    int rc = reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);

    /* Create new qreason and import */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    kb_import_result_t result;
    memset(&result, 0, sizeof(result));
    rc = reasoning_kb_import_from_buffer(ctx2, buffer, size, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.num_facts_imported, 3u);
    EXPECT_EQ(result.num_rules_imported, 2u);

    /* Verify fact values */
    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx2, 0, &conf), QREASON_TRUE);
    EXPECT_EQ(qreason_get_fact(ctx2, 2, &conf), QREASON_FALSE);

    /* Verify rules */
    EXPECT_EQ(ctx2->kb.n_rules, 2u);
    EXPECT_EQ(ctx2->kb.rules[0].consequent, 3u);
    EXPECT_EQ(ctx2->kb.rules[1].consequent, 4u);

    qreason_destroy(ctx2);
    nimcp_free(buffer);
}

TEST_F(KBPersistenceIntegration, PersistPreservesConfidences) {
    /* Set facts with specific confidences */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.123f);
    qreason_set_fact(ctx, 5, QREASON_FALSE, 0.456f);
    qreason_set_fact(ctx, 10, QREASON_TRUE, 0.789f);

    /* Export and reimport */
    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_NE(buffer, nullptr);

    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    reasoning_kb_import_from_buffer(ctx2, buffer, size, nullptr);

    /* Verify confidences are preserved */
    float conf = 0.0f;
    qreason_get_fact(ctx2, 0, &conf);
    EXPECT_NEAR(conf, 0.123f, 0.001f);

    qreason_get_fact(ctx2, 5, &conf);
    EXPECT_NEAR(conf, 0.456f, 0.001f);

    qreason_get_fact(ctx2, 10, &conf);
    EXPECT_NEAR(conf, 0.789f, 0.001f);

    qreason_destroy(ctx2);
    nimcp_free(buffer);
}

TEST_F(KBPersistenceIntegration, PersistAcrossEngineRecreation) {
    /* Build KB */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);
    uint32_t ants[1] = {0};
    qreason_add_rule(ctx, ants, 1, 1, 0.85f);

    /* Export */
    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_NE(buffer, nullptr);

    /* Destroy original engine */
    qreason_destroy(ctx);
    ctx = nullptr;

    /* Create completely new engine */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    /* Import */
    kb_import_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = reasoning_kb_import_from_buffer(ctx2, buffer, size, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.num_facts_imported, 1u);
    EXPECT_EQ(result.num_rules_imported, 1u);

    /* Verify KB is functional */
    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx2, 0, &conf), QREASON_TRUE);
    EXPECT_EQ(ctx2->kb.n_rules, 1u);

    qreason_destroy(ctx2);
    nimcp_free(buffer);
}

TEST_F(KBPersistenceIntegration, FileRoundtrip) {
    const char* filepath = "/tmp/nimcp_kb_integration_test.bin";

    /* Build KB */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);
    qreason_set_fact(ctx, 1, QREASON_FALSE, 0.7f);
    uint32_t ants[1] = {0};
    qreason_add_rule(ctx, ants, 1, 1, 0.8f);

    /* Save to file */
    kb_export_result_t exp;
    memset(&exp, 0, sizeof(exp));
    int rc = reasoning_kb_save_to_file(ctx, filepath, nullptr, &exp);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(exp.bytes_written, 0u);

    /* Load into fresh context */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    kb_import_result_t imp;
    memset(&imp, 0, sizeof(imp));
    rc = reasoning_kb_load_from_file(ctx2, filepath, &imp);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(imp.num_facts_imported, 2u);
    EXPECT_EQ(imp.num_rules_imported, 1u);

    /* Verify */
    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx2, 0, &conf), QREASON_TRUE);
    EXPECT_NEAR(conf, 0.9f, 0.001f);

    qreason_destroy(ctx2);
    remove(filepath);
}

TEST_F(KBPersistenceIntegration, TextFormatReadable) {
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);
    qreason_set_fact(ctx, 1, QREASON_FALSE, 0.7f);

    uint32_t ants[1] = {0};
    qreason_add_rule(ctx, ants, 1, 1, 0.8f);

    kb_persistence_config_t config = reasoning_kb_default_config();
    config.format = KB_FORMAT_TEXT;

    uint8_t* buffer = nullptr;
    size_t size = 0;
    int rc = reasoning_kb_export_to_buffer(ctx, &config, &buffer, &size);
    ASSERT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);

    /* Check human-readable content */
    std::string text((const char*)buffer, size);
    EXPECT_NE(text.find("# NIMCP Knowledge Base"), std::string::npos);
    EXPECT_NE(text.find("[FACTS]"), std::string::npos);
    EXPECT_NE(text.find("[RULES]"), std::string::npos);
    EXPECT_NE(text.find("F 0 T"), std::string::npos);  /* Fact 0 = TRUE */
    EXPECT_NE(text.find("F 1 F"), std::string::npos);  /* Fact 1 = FALSE */
    EXPECT_NE(text.find("R 0 -> 1"), std::string::npos);  /* Rule 0 -> 1 */

    nimcp_free(buffer);
}

TEST_F(KBPersistenceIntegration, ImportMerge) {
    /* Create two separate KBs */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);
    qreason_set_fact(ctx, 1, QREASON_FALSE, 0.8f);

    uint8_t* buffer1 = nullptr;
    size_t size1 = 0;
    reasoning_kb_export_to_buffer(ctx, nullptr, &buffer1, &size1);
    ASSERT_NE(buffer1, nullptr);

    /* Create target with different facts */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);
    qreason_set_fact(ctx2, 5, QREASON_TRUE, 0.7f);

    /* Import: should merge, not replace */
    kb_import_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = reasoning_kb_import_from_buffer(ctx2, buffer1, size1, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.num_facts_imported, 2u);

    /* Both old and new facts should be present */
    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx2, 0, &conf), QREASON_TRUE);
    EXPECT_EQ(qreason_get_fact(ctx2, 5, &conf), QREASON_TRUE);

    qreason_destroy(ctx2);
    nimcp_free(buffer1);
}
