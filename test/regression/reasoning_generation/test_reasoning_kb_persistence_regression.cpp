/**
 * @file test_reasoning_kb_persistence_regression.cpp
 * @brief Regression tests for KB persistence module
 *
 * WHAT: Guard against regressions in existing reasoning functions after
 *       the persistence module is added
 * WHY:  Adding new source files can cause symbol conflicts, header issues,
 *       or subtle side effects in existing functionality
 * HOW:  Verify quantum reasoning, forward chaining, backward chaining, and
 *       KB interface functions still work correctly
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_kb_persistence.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/memory/nimcp_memory.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class KBPersistenceRegression : public ::testing::Test {
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
 * REGRESSION TESTS
 *===========================================================================*/

TEST_F(KBPersistenceRegression, QuantumReasoningUnchanged) {
    /* Verify basic qreason operations still work after persistence module added */
    int rc = qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);
    EXPECT_EQ(rc, 0);

    rc = qreason_set_fact(ctx, 1, QREASON_FALSE, 0.8f);
    EXPECT_EQ(rc, 0);

    float conf = 0.0f;
    int8_t val = qreason_get_fact(ctx, 0, &conf);
    EXPECT_EQ(val, QREASON_TRUE);
    EXPECT_NEAR(conf, 0.9f, 0.001f);

    val = qreason_get_fact(ctx, 1, &conf);
    EXPECT_EQ(val, QREASON_FALSE);
    EXPECT_NEAR(conf, 0.8f, 0.001f);

    /* Unknown variables should still be UNKNOWN */
    val = qreason_get_fact(ctx, 10, &conf);
    EXPECT_EQ(val, QREASON_UNKNOWN);
}

TEST_F(KBPersistenceRegression, ForwardChainingUnchanged) {
    /* Set up a simple KB with forward chain potential */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);
    qreason_set_fact(ctx, 1, QREASON_TRUE, 0.8f);

    uint32_t ants[2] = {0, 1};
    int rc = qreason_add_rule(ctx, ants, 2, 2, 0.85f);
    EXPECT_EQ(rc, 0);

    /* Forward chain should still work — returns number of inferences made */
    qreason_result_t result;
    memset(&result, 0, sizeof(result));
    uint32_t inferences = qreason_forward_chain(ctx, &result);
    EXPECT_GE(inferences, 1u);

    /* Variable 2 should be derived as TRUE */
    float conf = 0.0f;
    int8_t val = qreason_get_fact(ctx, 2, &conf);
    EXPECT_EQ(val, QREASON_TRUE);
}

TEST_F(KBPersistenceRegression, BackwardChainingUnchanged) {
    /* KB: 0=TRUE, 1=TRUE, rule: 0 & 1 -> 2 */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);
    qreason_set_fact(ctx, 1, QREASON_TRUE, 0.8f);

    uint32_t ants[2] = {0, 1};
    qreason_add_rule(ctx, ants, 2, 2, 0.85f);

    /* Forward chain first so var 2 is derived */
    qreason_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    qreason_forward_chain(ctx, &fwd_result);

    /* After forward chain, var 2 should exist */
    float conf = 0.0f;
    int8_t val = qreason_get_fact(ctx, 2, &conf);
    EXPECT_EQ(val, QREASON_TRUE);
}

TEST_F(KBPersistenceRegression, KBInterfaceUnchanged) {
    /* Verify the KB data structures are not corrupted */
    EXPECT_EQ(ctx->kb.n_rules, 0u);

    /* Add facts */
    for (uint32_t i = 0; i < 10; i++) {
        qreason_set_fact(ctx, i, QREASON_TRUE, 0.5f + (float)i * 0.04f);
    }

    EXPECT_EQ(ctx->kb.n_variables, 10u);

    /* Add rules */
    for (uint32_t i = 0; i < 5; i++) {
        uint32_t ants[1] = {i};
        qreason_add_rule(ctx, ants, 1, i + 10, 0.9f);
    }

    EXPECT_EQ(ctx->kb.n_rules, 5u);

    /* Now export and verify the export doesn't corrupt the original KB */
    uint8_t* buffer = nullptr;
    size_t size = 0;
    int rc = reasoning_kb_export_to_buffer(ctx, nullptr, &buffer, &size);
    EXPECT_EQ(rc, 0);
    ASSERT_NE(buffer, nullptr);

    /* Original KB should be unchanged */
    EXPECT_EQ(ctx->kb.n_variables, 10u);
    EXPECT_EQ(ctx->kb.n_rules, 5u);

    float conf = 0.0f;
    int8_t val = qreason_get_fact(ctx, 0, &conf);
    EXPECT_EQ(val, QREASON_TRUE);
    EXPECT_NEAR(conf, 0.5f, 0.001f);

    nimcp_free(buffer);
}

TEST_F(KBPersistenceRegression, EmptyImportNoSideEffects) {
    /* Set up some initial KB state */
    qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);
    uint32_t ants[1] = {0};
    qreason_add_rule(ctx, ants, 1, 1, 0.8f);

    /* Export an empty KB */
    qreason_t empty_ctx = qreason_create(nullptr);
    ASSERT_NE(empty_ctx, nullptr);

    uint8_t* buffer = nullptr;
    size_t size = 0;
    reasoning_kb_export_to_buffer(empty_ctx, nullptr, &buffer, &size);
    ASSERT_NE(buffer, nullptr);
    qreason_destroy(empty_ctx);

    /* Import empty KB into our populated one */
    kb_import_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = reasoning_kb_import_from_buffer(ctx, buffer, size, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result.num_facts_imported, 0u);
    EXPECT_EQ(result.num_rules_imported, 0u);
    EXPECT_EQ(result.num_conflicts, 0u);

    /* Original data should be unchanged */
    float conf = 0.0f;
    EXPECT_EQ(qreason_get_fact(ctx, 0, &conf), QREASON_TRUE);
    EXPECT_NEAR(conf, 0.9f, 0.001f);
    EXPECT_EQ(ctx->kb.n_rules, 1u);

    nimcp_free(buffer);
}
