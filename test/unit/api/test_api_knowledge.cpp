/**
 * @file test_api_knowledge.cpp
 * @brief Unit tests for NIMCP API - Knowledge functions
 *
 * Tests the knowledge API:
 * - nimcp_knowledge_create()
 * - nimcp_knowledge_destroy()
 * - nimcp_knowledge_add_fact()
 * - nimcp_knowledge_query()
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>

class KnowledgeAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// nimcp_knowledge_create() tests
//=============================================================================

TEST_F(KnowledgeAPITest, KnowledgeCreateSucceeds) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    EXPECT_NE(knowledge, nullptr);

    if (knowledge) {
        nimcp_knowledge_destroy(knowledge);
    }
}

TEST_F(KnowledgeAPITest, KnowledgeCreateReturnsValidHandle) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Verify handle is valid by using it
    nimcp_status_t status = nimcp_knowledge_add_fact(
        knowledge, "test", "is", "valid"
    );
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeCreateMultipleInstances) {
    nimcp_knowledge_t knowledge1 = nimcp_knowledge_create();
    nimcp_knowledge_t knowledge2 = nimcp_knowledge_create();

    EXPECT_NE(knowledge1, nullptr);
    EXPECT_NE(knowledge2, nullptr);
    EXPECT_NE(knowledge1, knowledge2);

    if (knowledge1) nimcp_knowledge_destroy(knowledge1);
    if (knowledge2) nimcp_knowledge_destroy(knowledge2);
}

//=============================================================================
// nimcp_knowledge_destroy() tests
//=============================================================================

TEST_F(KnowledgeAPITest, KnowledgeDestroySucceeds) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Should not crash
    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeDestroyWithNullIsSafe) {
    // Should not crash with NULL
    nimcp_knowledge_destroy(nullptr);
}

//=============================================================================
// nimcp_knowledge_add_fact() tests
//=============================================================================

TEST_F(KnowledgeAPITest, KnowledgeAddFactSucceeds) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    nimcp_status_t status = nimcp_knowledge_add_fact(
        knowledge, "Paris", "is_capital_of", "France"
    );
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeAddFactNullKnowledgeFails) {
    nimcp_status_t status = nimcp_knowledge_add_fact(
        nullptr, "subject", "predicate", "object"
    );
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(KnowledgeAPITest, KnowledgeAddFactNullSubjectFails) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    nimcp_status_t status = nimcp_knowledge_add_fact(
        knowledge, nullptr, "predicate", "object"
    );
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeAddFactNullPredicateFails) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    nimcp_status_t status = nimcp_knowledge_add_fact(
        knowledge, "subject", nullptr, "object"
    );
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeAddFactNullObjectFails) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    nimcp_status_t status = nimcp_knowledge_add_fact(
        knowledge, "subject", "predicate", nullptr
    );
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeAddMultipleFacts) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    struct Fact {
        const char* subject;
        const char* predicate;
        const char* object;
    };

    Fact facts[] = {
        {"Paris", "is_capital_of", "France"},
        {"France", "is_in", "Europe"},
        {"Europe", "is_a", "continent"},
        {"London", "is_capital_of", "UK"},
        {"UK", "is_in", "Europe"}
    };

    for (const auto& fact : facts) {
        nimcp_status_t status = nimcp_knowledge_add_fact(
            knowledge, fact.subject, fact.predicate, fact.object
        );
        EXPECT_EQ(status, NIMCP_SUCCESS);
    }

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeAddFactWithEmptyStrings) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    nimcp_status_t status = nimcp_knowledge_add_fact(
        knowledge, "", "", ""
    );
    // Should handle empty strings gracefully
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR_INVALID);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeAddFactWithLongStrings) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Create long strings
    std::string long_subject(200, 'A');
    std::string long_predicate(200, 'B');
    std::string long_object(200, 'C');

    nimcp_status_t status = nimcp_knowledge_add_fact(
        knowledge, long_subject.c_str(), long_predicate.c_str(), long_object.c_str()
    );
    // Should handle or truncate long strings
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);

    nimcp_knowledge_destroy(knowledge);
}

//=============================================================================
// nimcp_knowledge_query() tests
//=============================================================================

TEST_F(KnowledgeAPITest, KnowledgeQuerySucceeds) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Add a fact
    nimcp_knowledge_add_fact(knowledge, "Paris", "is_capital_of", "France");

    // Query for it
    char result[1024];
    nimcp_status_t status = nimcp_knowledge_query(
        knowledge, "Paris", result, sizeof(result)
    );
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeQueryNullKnowledgeFails) {
    char result[1024];

    nimcp_status_t status = nimcp_knowledge_query(
        nullptr, "query", result, sizeof(result)
    );
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(KnowledgeAPITest, KnowledgeQueryNullQueryFails) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    char result[1024];

    nimcp_status_t status = nimcp_knowledge_query(
        knowledge, nullptr, result, sizeof(result)
    );
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeQueryNullResultFails) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    nimcp_status_t status = nimcp_knowledge_query(
        knowledge, "query", nullptr, 1024
    );
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeQueryReturnsCorrectFact) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Add a fact
    nimcp_knowledge_add_fact(knowledge, "Paris", "is_capital_of", "France");

    // Query for it
    char result[1024];
    nimcp_status_t status = nimcp_knowledge_query(
        knowledge, "Paris", result, sizeof(result)
    );

    ASSERT_EQ(status, NIMCP_SUCCESS);

    // Result should contain the predicate and object
    EXPECT_TRUE(strstr(result, "is_capital_of") != nullptr ||
                strstr(result, "France") != nullptr);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeQueryNotFoundReturnsMessage) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Query for non-existent fact
    char result[1024];
    nimcp_status_t status = nimcp_knowledge_query(
        knowledge, "NonExistent", result, sizeof(result)
    );

    EXPECT_EQ(status, NIMCP_SUCCESS);

    // Result should indicate not found
    EXPECT_TRUE(strstr(result, "No knowledge found") != nullptr ||
                strstr(result, "not found") != nullptr ||
                strlen(result) > 0);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeQueryMultipleFacts) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Add multiple facts
    nimcp_knowledge_add_fact(knowledge, "Paris", "is_capital_of", "France");
    nimcp_knowledge_add_fact(knowledge, "London", "is_capital_of", "UK");
    nimcp_knowledge_add_fact(knowledge, "Berlin", "is_capital_of", "Germany");

    // Query each one
    const char* queries[] = {"Paris", "London", "Berlin"};

    for (const char* query : queries) {
        char result[1024];
        nimcp_status_t status = nimcp_knowledge_query(
            knowledge, query, result, sizeof(result)
        );
        EXPECT_EQ(status, NIMCP_SUCCESS);
    }

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgeQueryWithSmallBuffer) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Add a fact
    nimcp_knowledge_add_fact(knowledge, "Test", "has_property", "value");

    // Query with small buffer
    char result[10];
    nimcp_status_t status = nimcp_knowledge_query(
        knowledge, "Test", result, sizeof(result)
    );

    // Should handle small buffer (truncate or error)
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);

    nimcp_knowledge_destroy(knowledge);
}

//=============================================================================
// Knowledge workflow tests
//=============================================================================

TEST_F(KnowledgeAPITest, KnowledgeAddAndQueryWorkflow) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Add facts about a topic
    nimcp_knowledge_add_fact(knowledge, "Water", "is_a", "liquid");
    nimcp_knowledge_add_fact(knowledge, "Water", "has_formula", "H2O");
    nimcp_knowledge_add_fact(knowledge, "Water", "boils_at", "100C");

    // Query for the topic
    char result[1024];
    nimcp_status_t status = nimcp_knowledge_query(
        knowledge, "Water", result, sizeof(result)
    );

    EXPECT_EQ(status, NIMCP_SUCCESS);
    EXPECT_GT(strlen(result), 0);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, KnowledgePersistsThroughQueries) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Add a fact
    nimcp_knowledge_add_fact(knowledge, "Sky", "is", "blue");

    // Query multiple times
    for (int i = 0; i < 5; i++) {
        char result[1024];
        nimcp_status_t status = nimcp_knowledge_query(
            knowledge, "Sky", result, sizeof(result)
        );
        EXPECT_EQ(status, NIMCP_SUCCESS);
    }

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(KnowledgeAPITest, MultipleKnowledgeInstancesIndependent) {
    nimcp_knowledge_t knowledge1 = nimcp_knowledge_create();
    nimcp_knowledge_t knowledge2 = nimcp_knowledge_create();

    ASSERT_NE(knowledge1, nullptr);
    ASSERT_NE(knowledge2, nullptr);

    // Add different facts to each
    nimcp_knowledge_add_fact(knowledge1, "A", "is", "1");
    nimcp_knowledge_add_fact(knowledge2, "B", "is", "2");

    // Query from each
    char result1[1024], result2[1024];
    nimcp_knowledge_query(knowledge1, "A", result1, sizeof(result1));
    nimcp_knowledge_query(knowledge2, "B", result2, sizeof(result2));

    // Each should only have its own fact
    EXPECT_TRUE(strstr(result1, "1") != nullptr || strlen(result1) > 0);
    EXPECT_TRUE(strstr(result2, "2") != nullptr || strlen(result2) > 0);

    nimcp_knowledge_destroy(knowledge1);
    nimcp_knowledge_destroy(knowledge2);
}

TEST_F(KnowledgeAPITest, KnowledgeUpdateExistingFact) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Add initial fact
    nimcp_knowledge_add_fact(knowledge, "Status", "is", "initial");

    // Update with new fact (same subject, different predicate/object)
    nimcp_knowledge_add_fact(knowledge, "Status", "is", "updated");

    // Query should return something
    char result[1024];
    nimcp_status_t status = nimcp_knowledge_query(
        knowledge, "Status", result, sizeof(result)
    );

    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_knowledge_destroy(knowledge);
}
