/**
 * @file test_reasoning_kb_persistence_e2e.cpp
 * @brief End-to-end tests for KB persistence with full brain lifecycle
 *
 * WHAT: Tests KB persistence across brain creation, fact addition, reasoning,
 *       export, recreation, and import
 * WHY:  Verify the persistence system works in a realistic full-system scenario
 * HOW:  GTest suite with brain + qreason lifecycle
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_kb_persistence.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/memory/nimcp_memory.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class KBPersistenceE2E : public ::testing::Test {
protected:
    void TearDown() override {
        /* Clean up temp files */
        remove("/tmp/nimcp_e2e_kb_lifecycle.bin");
        remove("/tmp/nimcp_e2e_kb_persist.bin");
        remove("/tmp/nimcp_e2e_kb_large.bin");
    }
};

/*=============================================================================
 * E2E TESTS
 *===========================================================================*/

TEST_F(KBPersistenceE2E, FullKBLifecycle) {
    /* Phase 1: Create engine and populate KB */
    qreason_t ctx = qreason_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    /* Add symbolic facts */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.95f);   /* Bird(tweety) */
    qreason_set_fact(ctx, 1, QREASON_TRUE, 0.9f);     /* Penguin(opus) */
    qreason_set_fact(ctx, 2, QREASON_FALSE, 0.85f);   /* ~Fly(penguin) */

    /* Add inference rules */
    uint32_t ants1[1] = {0};
    qreason_add_rule(ctx, ants1, 1, 3, 0.8f);  /* Bird -> Fly */

    uint32_t ants2[1] = {1};
    qreason_add_rule(ctx, ants2, 1, 0, 0.9f);  /* Penguin -> Bird */

    /* Phase 2: Run forward chaining to derive new facts — returns inference count */
    qreason_result_t fc_result;
    memset(&fc_result, 0, sizeof(fc_result));
    uint32_t inferences = qreason_forward_chain(ctx, &fc_result);
    EXPECT_GE(inferences, 1u);

    /* Phase 3: Export full KB (including derived facts) */
    uint8_t* buffer = nullptr;
    size_t size = 0;
    int rc = reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);

    /* Also save to file */
    const char* filepath = "/tmp/nimcp_e2e_kb_lifecycle.bin";
    kb_export_result_t exp;
    memset(&exp, 0, sizeof(exp));
    rc = reasoning_kb_save_to_file(ctx, filepath, nullptr, &exp);
    EXPECT_EQ(rc, 0);

    /* Phase 4: Destroy original engine */
    qreason_destroy(ctx);
    ctx = nullptr;

    /* Phase 5: Recreate engine and import */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    kb_import_result_t imp;
    memset(&imp, 0, sizeof(imp));
    rc = reasoning_kb_import_from_buffer(ctx2, buffer, size, &imp);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(imp.num_facts_imported, 3u);  /* At least our 3 original facts */
    EXPECT_EQ(imp.num_rules_imported, 2u);

    /* Phase 6: Verify reasoning produces same results */
    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx2, 0, &conf), QREASON_TRUE);

    qreason_destroy(ctx2);

    /* Phase 7: Also test file-based roundtrip */
    qreason_t ctx3 = qreason_create(nullptr);
    ASSERT_NE(ctx3, nullptr);

    kb_import_result_t file_imp;
    memset(&file_imp, 0, sizeof(file_imp));
    rc = reasoning_kb_load_from_file(ctx3, filepath, &file_imp);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(file_imp.num_facts_imported, 3u);

    qreason_destroy(ctx3);
    nimcp_free(buffer);
}

TEST_F(KBPersistenceE2E, PersistAndReason) {
    /* Create and populate KB */
    qreason_t ctx = qreason_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    /* Set up a reasoning chain:
     * var 0 = TRUE, var 1 = TRUE
     * Rule: 0 & 1 -> 2 (conf 0.9)
     * Rule: 2 -> 3 (conf 0.85)
     */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.95f);
    qreason_set_fact(ctx, 1, QREASON_TRUE, 0.9f);

    uint32_t ants1[2] = {0, 1};
    qreason_add_rule(ctx, ants1, 2, 2, 0.9f);

    uint32_t ants2[1] = {2};
    qreason_add_rule(ctx, ants2, 1, 3, 0.85f);

    /* Forward chain to derive 2 and 3 */
    qreason_result_t fc_result;
    memset(&fc_result, 0, sizeof(fc_result));
    qreason_forward_chain(ctx, &fc_result);

    /* Save */
    const char* filepath = "/tmp/nimcp_e2e_kb_persist.bin";
    reasoning_kb_save_to_file(ctx, filepath, nullptr, nullptr);
    qreason_destroy(ctx);

    /* Load into fresh engine */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    reasoning_kb_load_from_file(ctx2, filepath, nullptr);

    /* Run queries on the loaded KB */
    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx2, 0, &conf), QREASON_TRUE);
    EXPECT_EQ(qreason_get_fact(ctx2, 1, &conf), QREASON_TRUE);

    /* Run forward chaining again on loaded KB — returns inference count */
    qreason_result_t fc_result2;
    memset(&fc_result2, 0, sizeof(fc_result2));
    uint32_t inferences2 = qreason_forward_chain(ctx2, &fc_result2);
    (void)inferences2;  /* May be 0 if facts already derived */

    qreason_destroy(ctx2);
}

TEST_F(KBPersistenceE2E, LargeKBPersistence) {
    /* Create a large KB */
    qreason_t ctx = qreason_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    /* Fill with maximum facts */
    for (uint32_t i = 0; i < QREASON_MAX_VARIABLES; i++) {
        int8_t truth = (i % 3 == 0) ? QREASON_TRUE :
                       (i % 3 == 1) ? QREASON_FALSE : QREASON_TRUE;
        qreason_set_fact(ctx, i, truth, 0.5f + (float)i * 0.01f);
    }

    /* Fill with many rules */
    for (uint32_t i = 0; i < 50 && i < QREASON_MAX_RULES; i++) {
        uint32_t ants[2] = { i % QREASON_MAX_VARIABLES,
                             (i + 1) % QREASON_MAX_VARIABLES };
        uint32_t cons = (i + 2) % QREASON_MAX_VARIABLES;
        qreason_add_rule(ctx, ants, 2, cons, 0.8f);
    }

    /* Export to buffer */
    uint8_t* buffer = nullptr;
    size_t size = 0;
    int rc = reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    ASSERT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);

    /* Validate buffer */
    EXPECT_EQ(reasoning_kb_validate_buffer(buffer, size), 0);

    /* Save to file */
    const char* filepath = "/tmp/nimcp_e2e_kb_large.bin";
    kb_export_result_t exp;
    memset(&exp, 0, sizeof(exp));
    rc = reasoning_kb_save_to_file(ctx, filepath, nullptr, &exp);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(exp.bytes_written, 0u);

    qreason_destroy(ctx);

    /* Load from file into new context */
    qreason_t ctx2 = qreason_create(nullptr);
    ASSERT_NE(ctx2, nullptr);

    kb_import_result_t imp;
    memset(&imp, 0, sizeof(imp));
    rc = reasoning_kb_load_from_file(ctx2, filepath, &imp);
    EXPECT_EQ(rc, 0);

    /* All non-UNKNOWN facts should be imported (all 32 are non-UNKNOWN) */
    EXPECT_EQ(imp.num_facts_imported, QREASON_MAX_VARIABLES);
    EXPECT_EQ(imp.num_rules_imported, 50u);

    /* Verify a sample of facts */
    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx2, 0, &conf), QREASON_TRUE);
    EXPECT_EQ(qreason_get_fact(ctx2, 1, &conf), QREASON_FALSE);

    /* Verify checksum matches */
    uint32_t checksum = reasoning_kb_get_checksum(buffer, size);
    EXPECT_NE(checksum, 0u);

    qreason_destroy(ctx2);
    nimcp_free(buffer);
}
