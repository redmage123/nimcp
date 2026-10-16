/**
 * @file test_policy_parser.cpp
 * @brief Unit tests for NIMCP Policy Parser
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "security/nimcp_policy_parser.h"
#include "security/nimcp_policy_ast.h"

class PolicyParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing to set up
    }

    void TearDown() override {
        // Nothing to tear down
    }

    nimcp_ast_node_t* parse(const char* input) {
        char* error = nullptr;
        nimcp_ast_node_t* ast = nimcp_policy_parse(input, "test.policy", &error);
        if (error) {
            last_error = std::string(error);
            free(error);
        }
        return ast;
    }

    std::string last_error;
};

/* ========================================================================
 * Basic Parsing Tests
 * ======================================================================== */

TEST_F(PolicyParserTest, ParseSimpleLiteral) {
    const char* input = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NIMCP_AST_RULE);
    EXPECT_STREQ(ast->rule.name, "test");

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseStringLiteral) {
    const char* input = R"(
        rule "test" {
            condition: contains("hello world", "world")
            action: DENY
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NIMCP_AST_RULE);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseNumberLiterals) {
    const char* input = R"(
        rule "test" {
            condition: 42 > 10
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NIMCP_AST_RULE);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseFloatLiterals) {
    const char* input = R"(
        rule "test" {
            condition: 3.14 < 10.5
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NIMCP_AST_RULE);

    nimcp_ast_destroy(ast);
}

/* ========================================================================
 * Expression Parsing Tests
 * ======================================================================== */

TEST_F(PolicyParserTest, ParseBinaryExpressions) {
    const char* input = R"(
        rule "test" {
            condition: 10 + 5 == 15
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);
    EXPECT_EQ(ast->rule.condition->type, NIMCP_AST_BINARY_OP);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseLogicalAnd) {
    const char* input = R"(
        rule "test" {
            condition: true AND false
            action: DENY
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);
    EXPECT_EQ(ast->rule.condition->type, NIMCP_AST_BINARY_OP);
    EXPECT_EQ(ast->rule.condition->binary.op, NIMCP_OP_AND);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseLogicalOr) {
    const char* input = R"(
        rule "test" {
            condition: true OR false
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);
    EXPECT_EQ(ast->rule.condition->type, NIMCP_AST_BINARY_OP);
    EXPECT_EQ(ast->rule.condition->binary.op, NIMCP_OP_OR);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseComplexCondition) {
    const char* input = R"(
        rule "complex" {
            condition: (10 > 5 AND 20 < 30) OR NOT false
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseComparison) {
    const char* input = R"(
        rule "test" {
            condition: 10 >= 5
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);
    EXPECT_EQ(ast->rule.condition->type, NIMCP_AST_BINARY_OP);
    EXPECT_EQ(ast->rule.condition->binary.op, NIMCP_OP_GE);

    nimcp_ast_destroy(ast);
}

/* ========================================================================
 * Function Call Parsing Tests
 * ======================================================================== */

TEST_F(PolicyParserTest, ParseFunctionCallNoArgs) {
    const char* input = R"(
        rule "test" {
            condition: test()
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);
    EXPECT_EQ(ast->rule.condition->type, NIMCP_AST_CALL);
    EXPECT_STREQ(ast->rule.condition->call.name, "test");
    EXPECT_EQ(ast->rule.condition->call.num_args, 0);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseFunctionCallWithArgs) {
    const char* input = R"(
        rule "test" {
            condition: contains("hello", "lo")
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);
    EXPECT_EQ(ast->rule.condition->type, NIMCP_AST_CALL);
    EXPECT_STREQ(ast->rule.condition->call.name, "contains");
    EXPECT_EQ(ast->rule.condition->call.num_args, 2);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseNestedFunctionCalls) {
    const char* input = R"(
        rule "test" {
            condition: length(contains("hello", "lo"))
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);
    EXPECT_EQ(ast->rule.condition->type, NIMCP_AST_CALL);

    nimcp_ast_destroy(ast);
}

/* ========================================================================
 * Member Access Parsing Tests
 * ======================================================================== */

TEST_F(PolicyParserTest, ParseMemberAccess) {
    const char* input = R"(
        rule "test" {
            condition: input.length > 10
            action: DENY
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);
    EXPECT_EQ(ast->rule.condition->type, NIMCP_AST_BINARY_OP);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseChainedMemberAccess) {
    const char* input = R"(
        rule "test" {
            condition: request.user.authenticated
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);

    nimcp_ast_destroy(ast);
}

/* ========================================================================
 * Policy Parsing Tests
 * ======================================================================== */

TEST_F(PolicyParserTest, ParsePolicy) {
    const char* input = R"(
        policy "api_access" {
            rule "require_auth" {
                condition: NOT request.authenticated
                action: DENY
            }

            rule "rate_limit" {
                condition: request.rate > 100
                action: THROTTLE
            }
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NIMCP_AST_POLICY);
    EXPECT_STREQ(ast->policy.name, "api_access");
    EXPECT_EQ(ast->policy.num_rules, 2);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseMultipleRules) {
    const char* input = R"(
        policy "test" {
            rule "rule1" {
                condition: true
                action: ALLOW
            }

            rule "rule2" {
                condition: false
                action: DENY
            }

            rule "rule3" {
                condition: 10 > 5
                action: LOG
            }
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NIMCP_AST_POLICY);
    EXPECT_EQ(ast->policy.num_rules, 3);

    nimcp_ast_destroy(ast);
}

/* ========================================================================
 * Error Handling Tests
 * ======================================================================== */

TEST_F(PolicyParserTest, ParseInvalidSyntax) {
    const char* input = "this is not valid syntax";

    nimcp_ast_node_t* ast = parse(input);
    EXPECT_EQ(ast, nullptr);
    EXPECT_FALSE(last_error.empty());
}

TEST_F(PolicyParserTest, ParseUnterminatedString) {
    const char* input = R"(
        rule "test" {
            condition: "unterminated
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    EXPECT_EQ(ast, nullptr);
}

TEST_F(PolicyParserTest, ParseMissingBrace) {
    const char* input = R"(
        rule "test" {
            condition: true
            action: ALLOW
    )";

    nimcp_ast_node_t* ast = parse(input);
    EXPECT_EQ(ast, nullptr);
}

/* ========================================================================
 * Comment Tests
 * ======================================================================== */

TEST_F(PolicyParserTest, ParseWithComments) {
    const char* input = R"(
        # This is a comment
        rule "test" {
            # Another comment
            condition: true  # Inline comment
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NIMCP_AST_RULE);

    nimcp_ast_destroy(ast);
}

/* ========================================================================
 * Unary Operator Tests
 * ======================================================================== */

TEST_F(PolicyParserTest, ParseNotOperator) {
    const char* input = R"(
        rule "test" {
            condition: NOT true
            action: DENY
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);
    EXPECT_EQ(ast->rule.condition->type, NIMCP_AST_UNARY_OP);
    EXPECT_EQ(ast->rule.condition->unary.op, NIMCP_OP_NOT);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseNegation) {
    const char* input = R"(
        rule "test" {
            condition: -10 < 0
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    ASSERT_NE(ast->rule.condition, nullptr);

    nimcp_ast_destroy(ast);
}

/* ========================================================================
 * AST Validation Tests
 * ======================================================================== */

TEST_F(PolicyParserTest, ValidateAST) {
    const char* input = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    EXPECT_TRUE(nimcp_ast_validate(ast));

    nimcp_ast_destroy(ast);
}

/* ========================================================================
 * Complex Real-World Examples
 * ======================================================================== */

TEST_F(PolicyParserTest, ParseSQLInjectionDetection) {
    const char* input = R"(
        rule "block_sql_injection" {
            condition: contains(input, "SELECT") AND contains(input, "FROM")
            action: DENY
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NIMCP_AST_RULE);

    nimcp_ast_destroy(ast);
}

TEST_F(PolicyParserTest, ParseComplexDetection) {
    const char* input = R"(
        rule "complex_detection" {
            condition: (
                length(input) > 1000 AND
                entropy(input) > 0.8 AND
                (contains(input, "script") OR contains(input, "eval"))
            )
            action: DENY
        }
    )";

    nimcp_ast_node_t* ast = parse(input);
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NIMCP_AST_RULE);

    nimcp_ast_destroy(ast);
}
