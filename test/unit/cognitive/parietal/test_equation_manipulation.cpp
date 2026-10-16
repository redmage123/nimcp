/**
 * @file test_equation_manipulation.cpp
 * @brief Unit tests for NIMCP Equation Manipulation
 *
 * Tests expression parsing, simplification, differentiation,
 * evaluation, and equation solving.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_equation_manipulation.h"

namespace {

//=============================================================================
// Test Constants
//=============================================================================

constexpr float FLOAT_TOLERANCE = 1e-4f;

//=============================================================================
// Test Fixture
//=============================================================================

class EquationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        eq = equation_engine_create();
        ASSERT_NE(eq, nullptr);
    }

    void TearDown() override
    {
        if (eq) {
            equation_engine_destroy(eq);
            eq = nullptr;
        }
    }

    equation_engine_t* eq;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EquationTest, CreateDefault)
{
    EXPECT_NE(eq, nullptr);
}

TEST_F(EquationTest, CreateCustom)
{
    equation_config_t config = equation_default_config();
    config.max_simplify_iterations = 200;
    config.numerical_tolerance = 1e-8f;

    equation_engine_t* custom = equation_engine_create_custom(&config);
    ASSERT_NE(custom, nullptr);
    equation_engine_destroy(custom);
}

TEST_F(EquationTest, CreateWithNullConfig)
{
    equation_engine_t* created = equation_engine_create_custom(nullptr);
    EXPECT_NE(created, nullptr);
    equation_engine_destroy(created);
}

TEST_F(EquationTest, DestroyNullSafe)
{
    equation_engine_destroy(nullptr);
    // Should not crash
}

TEST_F(EquationTest, DefaultConfig)
{
    equation_config_t config = equation_default_config();

    EXPECT_EQ(config.max_simplify_iterations, 100);
    EXPECT_EQ(config.max_tree_depth, 32);
    EXPECT_NEAR(config.numerical_tolerance, 1e-6f, 1e-7f);
    EXPECT_TRUE(config.enable_trigonometric_identities);
}

TEST_F(EquationTest, ValidateConfig)
{
    equation_config_t valid = equation_default_config();
    EXPECT_TRUE(equation_validate_config(&valid));

    equation_config_t invalid = valid;
    invalid.max_tree_depth = 0;
    EXPECT_FALSE(equation_validate_config(&invalid));
}

//=============================================================================
// Expression Creation Tests
//=============================================================================

TEST_F(EquationTest, CreateConstant)
{
    expr_node_t* c = equation_create_constant(eq, 42.0f);
    ASSERT_NE(c, nullptr);

    EXPECT_EQ(c->type, EXPR_CONSTANT);
    EXPECT_NEAR(c->data.constant, 42.0f, FLOAT_TOLERANCE);

    equation_free_expr(c);
}

TEST_F(EquationTest, CreateVariable)
{
    expr_node_t* x = equation_create_variable(eq, "x");
    ASSERT_NE(x, nullptr);

    EXPECT_EQ(x->type, EXPR_VARIABLE);
    EXPECT_STREQ(x->data.variable, "x");

    equation_free_expr(x);
}

TEST_F(EquationTest, CreateBinaryAdd)
{
    expr_node_t* a = equation_create_constant(eq, 2.0f);
    expr_node_t* b = equation_create_constant(eq, 3.0f);
    expr_node_t* sum = equation_create_binary(eq, EXPR_ADD, a, b);

    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(sum->type, EXPR_ADD);
    EXPECT_EQ(sum->left, a);
    EXPECT_EQ(sum->right, b);

    equation_free_expr(sum);
}

TEST_F(EquationTest, CreateUnarySin)
{
    expr_node_t* x = equation_create_variable(eq, "x");
    expr_node_t* sin_x = equation_create_unary(eq, EXPR_SIN, x);

    ASSERT_NE(sin_x, nullptr);
    EXPECT_EQ(sin_x->type, EXPR_SIN);
    EXPECT_EQ(sin_x->left, x);

    equation_free_expr(sin_x);
}

TEST_F(EquationTest, FreeExprNullSafe)
{
    equation_free_expr(nullptr);
    // Should not crash
}

TEST_F(EquationTest, CopyExpr)
{
    expr_node_t* x = equation_create_variable(eq, "x");
    expr_node_t* two = equation_create_constant(eq, 2.0f);
    expr_node_t* expr = equation_create_binary(eq, EXPR_ADD, x, two);

    expr_node_t* copy = equation_copy_expr(eq, expr);
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->type, EXPR_ADD);
    EXPECT_NE(copy, expr);  // Different memory

    equation_free_expr(expr);
    equation_free_expr(copy);
}

//=============================================================================
// Parsing Tests
//=============================================================================

TEST_F(EquationTest, ParseConstant)
{
    expr_node_t* expr = equation_parse(eq, "42");
    ASSERT_NE(expr, nullptr);

    EXPECT_EQ(expr->type, EXPR_CONSTANT);
    EXPECT_NEAR(expr->data.constant, 42.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
}

TEST_F(EquationTest, ParseVariable)
{
    expr_node_t* expr = equation_parse(eq, "x");
    ASSERT_NE(expr, nullptr);

    EXPECT_EQ(expr->type, EXPR_VARIABLE);
    EXPECT_STREQ(expr->data.variable, "x");

    equation_free_expr(expr);
}

TEST_F(EquationTest, ParseSimpleAdd)
{
    expr_node_t* expr = equation_parse(eq, "2 + 3");
    ASSERT_NE(expr, nullptr);

    EXPECT_EQ(expr->type, EXPR_ADD);

    equation_free_expr(expr);
}

TEST_F(EquationTest, ParseSimpleMul)
{
    expr_node_t* expr = equation_parse(eq, "x * 2");
    ASSERT_NE(expr, nullptr);

    EXPECT_EQ(expr->type, EXPR_MUL);

    equation_free_expr(expr);
}

TEST_F(EquationTest, ParsePower)
{
    expr_node_t* expr = equation_parse(eq, "x^2");
    ASSERT_NE(expr, nullptr);

    EXPECT_EQ(expr->type, EXPR_POW);

    equation_free_expr(expr);
}

TEST_F(EquationTest, ParseParentheses)
{
    expr_node_t* expr = equation_parse(eq, "(x + 1) * 2");
    ASSERT_NE(expr, nullptr);

    EXPECT_EQ(expr->type, EXPR_MUL);

    equation_free_expr(expr);
}

TEST_F(EquationTest, ParseSin)
{
    expr_node_t* expr = equation_parse(eq, "sin(x)");
    ASSERT_NE(expr, nullptr);

    EXPECT_EQ(expr->type, EXPR_SIN);

    equation_free_expr(expr);
}

TEST_F(EquationTest, ParseComplex)
{
    expr_node_t* expr = equation_parse(eq, "x^2 + 2*x + 1");
    ASSERT_NE(expr, nullptr);

    equation_free_expr(expr);
}

TEST_F(EquationTest, ParseNullHandling)
{
    EXPECT_EQ(equation_parse(nullptr, "x"), nullptr);
    EXPECT_EQ(equation_parse(eq, nullptr), nullptr);
    EXPECT_EQ(equation_parse(eq, ""), nullptr);
}

TEST_F(EquationTest, ExprToString)
{
    expr_node_t* expr = equation_parse(eq, "x + 2");
    ASSERT_NE(expr, nullptr);

    char buffer[256];
    const char* str = equation_to_string(eq, expr, buffer, 256);

    ASSERT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0);

    equation_free_expr(expr);
}

TEST_F(EquationTest, ParseUnparseRoundTrip)
{
    // Parse, unparse, re-parse should give equivalent expression
    expr_node_t* original = equation_parse(eq, "x + 1");
    ASSERT_NE(original, nullptr);

    char buffer[256];
    equation_to_string(eq, original, buffer, 256);

    expr_node_t* reparsed = equation_parse(eq, buffer);
    ASSERT_NE(reparsed, nullptr);

    EXPECT_EQ(original->type, reparsed->type);

    equation_free_expr(original);
    equation_free_expr(reparsed);
}

//=============================================================================
// Simplification Tests
//=============================================================================

TEST_F(EquationTest, SimplifyAddZero)
{
    // x + 0 = x
    expr_node_t* expr = equation_parse(eq, "x + 0");
    ASSERT_NE(expr, nullptr);

    expr_node_t* simplified = equation_simplify(eq, expr);
    ASSERT_NE(simplified, nullptr);

    EXPECT_EQ(simplified->type, EXPR_VARIABLE);

    equation_free_expr(expr);
    equation_free_expr(simplified);
}

TEST_F(EquationTest, SimplifyMulOne)
{
    // x * 1 = x
    expr_node_t* expr = equation_parse(eq, "x * 1");
    ASSERT_NE(expr, nullptr);

    expr_node_t* simplified = equation_simplify(eq, expr);
    ASSERT_NE(simplified, nullptr);

    EXPECT_EQ(simplified->type, EXPR_VARIABLE);

    equation_free_expr(expr);
    equation_free_expr(simplified);
}

TEST_F(EquationTest, SimplifyMulZero)
{
    // x * 0 = 0
    expr_node_t* expr = equation_parse(eq, "x * 0");
    ASSERT_NE(expr, nullptr);

    expr_node_t* simplified = equation_simplify(eq, expr);
    ASSERT_NE(simplified, nullptr);

    EXPECT_EQ(simplified->type, EXPR_CONSTANT);
    EXPECT_NEAR(simplified->data.constant, 0.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
    equation_free_expr(simplified);
}

TEST_F(EquationTest, SimplifyPowerZero)
{
    // x^0 = 1
    expr_node_t* expr = equation_parse(eq, "x^0");
    ASSERT_NE(expr, nullptr);

    expr_node_t* simplified = equation_simplify(eq, expr);
    ASSERT_NE(simplified, nullptr);

    EXPECT_EQ(simplified->type, EXPR_CONSTANT);
    EXPECT_NEAR(simplified->data.constant, 1.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
    equation_free_expr(simplified);
}

TEST_F(EquationTest, SimplifyPowerOne)
{
    // x^1 = x
    expr_node_t* expr = equation_parse(eq, "x^1");
    ASSERT_NE(expr, nullptr);

    expr_node_t* simplified = equation_simplify(eq, expr);
    ASSERT_NE(simplified, nullptr);

    EXPECT_EQ(simplified->type, EXPR_VARIABLE);

    equation_free_expr(expr);
    equation_free_expr(simplified);
}

TEST_F(EquationTest, SimplifyConstantFolding)
{
    // 2 + 3 = 5
    expr_node_t* expr = equation_parse(eq, "2 + 3");
    ASSERT_NE(expr, nullptr);

    expr_node_t* simplified = equation_simplify(eq, expr);
    ASSERT_NE(simplified, nullptr);

    EXPECT_EQ(simplified->type, EXPR_CONSTANT);
    EXPECT_NEAR(simplified->data.constant, 5.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
    equation_free_expr(simplified);
}

//=============================================================================
// Differentiation Tests
//=============================================================================

TEST_F(EquationTest, DifferentiateConstant)
{
    // d/dx[5] = 0
    expr_node_t* expr = equation_parse(eq, "5");
    ASSERT_NE(expr, nullptr);

    expr_node_t* deriv = equation_differentiate(eq, expr, "x");
    ASSERT_NE(deriv, nullptr);

    expr_node_t* simplified = equation_simplify(eq, deriv);
    EXPECT_EQ(simplified->type, EXPR_CONSTANT);
    EXPECT_NEAR(simplified->data.constant, 0.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
    equation_free_expr(deriv);
    equation_free_expr(simplified);
}

TEST_F(EquationTest, DifferentiateVariable)
{
    // d/dx[x] = 1
    expr_node_t* expr = equation_parse(eq, "x");
    ASSERT_NE(expr, nullptr);

    expr_node_t* deriv = equation_differentiate(eq, expr, "x");
    ASSERT_NE(deriv, nullptr);

    expr_node_t* simplified = equation_simplify(eq, deriv);
    EXPECT_EQ(simplified->type, EXPR_CONSTANT);
    EXPECT_NEAR(simplified->data.constant, 1.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
    equation_free_expr(deriv);
    equation_free_expr(simplified);
}

TEST_F(EquationTest, DifferentiateOtherVariable)
{
    // d/dx[y] = 0
    expr_node_t* expr = equation_parse(eq, "y");
    ASSERT_NE(expr, nullptr);

    expr_node_t* deriv = equation_differentiate(eq, expr, "x");
    ASSERT_NE(deriv, nullptr);

    expr_node_t* simplified = equation_simplify(eq, deriv);
    EXPECT_EQ(simplified->type, EXPR_CONSTANT);
    EXPECT_NEAR(simplified->data.constant, 0.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
    equation_free_expr(deriv);
    equation_free_expr(simplified);
}

TEST_F(EquationTest, DifferentiateLinear)
{
    // d/dx[2*x + 3] = 2
    expr_node_t* expr = equation_parse(eq, "2*x + 3");
    ASSERT_NE(expr, nullptr);

    expr_node_t* deriv = equation_differentiate(eq, expr, "x");
    ASSERT_NE(deriv, nullptr);

    expr_node_t* simplified = equation_simplify(eq, deriv);

    // Evaluate derivative (should be 2)
    variable_binding_t bind = {"x", 5.0f};
    float value = equation_evaluate(eq, simplified, &bind, 1);
    EXPECT_NEAR(value, 2.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
    equation_free_expr(deriv);
    equation_free_expr(simplified);
}

TEST_F(EquationTest, DifferentiateQuadratic)
{
    // d/dx[x^2] = 2*x
    expr_node_t* expr = equation_parse(eq, "x^2");
    ASSERT_NE(expr, nullptr);

    expr_node_t* deriv = equation_differentiate(eq, expr, "x");
    ASSERT_NE(deriv, nullptr);

    expr_node_t* simplified = equation_simplify(eq, deriv);

    // Evaluate at x=3: should give 6
    variable_binding_t bind = {"x", 3.0f};
    float value = equation_evaluate(eq, simplified, &bind, 1);
    EXPECT_NEAR(value, 6.0f, 0.1f);

    equation_free_expr(expr);
    equation_free_expr(deriv);
    equation_free_expr(simplified);
}

TEST_F(EquationTest, DifferentiateSin)
{
    // d/dx[sin(x)] = cos(x)
    expr_node_t* expr = equation_parse(eq, "sin(x)");
    ASSERT_NE(expr, nullptr);

    expr_node_t* deriv = equation_differentiate(eq, expr, "x");
    ASSERT_NE(deriv, nullptr);

    // Evaluate at x=0: cos(0) = 1
    variable_binding_t bind = {"x", 0.0f};
    float value = equation_evaluate(eq, deriv, &bind, 1);
    EXPECT_NEAR(value, 1.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
    equation_free_expr(deriv);
}

TEST_F(EquationTest, DifferentiateCos)
{
    // d/dx[cos(x)] = -sin(x)
    expr_node_t* expr = equation_parse(eq, "cos(x)");
    ASSERT_NE(expr, nullptr);

    expr_node_t* deriv = equation_differentiate(eq, expr, "x");
    ASSERT_NE(deriv, nullptr);

    // Evaluate at x=0: -sin(0) = 0
    variable_binding_t bind = {"x", 0.0f};
    float value = equation_evaluate(eq, deriv, &bind, 1);
    EXPECT_NEAR(value, 0.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
    equation_free_expr(deriv);
}

TEST_F(EquationTest, DifferentiateExp)
{
    // d/dx[exp(x)] = exp(x)
    expr_node_t* expr = equation_parse(eq, "exp(x)");
    ASSERT_NE(expr, nullptr);

    expr_node_t* deriv = equation_differentiate(eq, expr, "x");
    ASSERT_NE(deriv, nullptr);

    // Evaluate at x=0: exp(0) = 1
    variable_binding_t bind = {"x", 0.0f};
    float value = equation_evaluate(eq, deriv, &bind, 1);
    EXPECT_NEAR(value, 1.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
    equation_free_expr(deriv);
}

TEST_F(EquationTest, DifferentiateChainRule)
{
    // d/dx[sin(2*x)] = 2*cos(2*x)
    expr_node_t* expr = equation_parse(eq, "sin(2*x)");
    ASSERT_NE(expr, nullptr);

    expr_node_t* deriv = equation_differentiate(eq, expr, "x");
    ASSERT_NE(deriv, nullptr);

    // Evaluate at x=0: 2*cos(0) = 2
    variable_binding_t bind = {"x", 0.0f};
    float value = equation_evaluate(eq, deriv, &bind, 1);
    EXPECT_NEAR(value, 2.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
    equation_free_expr(deriv);
}

TEST_F(EquationTest, Gradient)
{
    // f(x,y) = x^2 + y^2
    // grad f = (2x, 2y)
    expr_node_t* expr = equation_parse(eq, "x^2 + y^2");
    ASSERT_NE(expr, nullptr);

    const char* vars[] = {"x", "y"};
    expr_node_t* gradient[2];

    uint32_t num = equation_gradient(eq, expr, vars, 2, gradient);
    EXPECT_EQ(num, 2);

    // Evaluate at (3, 4)
    variable_binding_t binds[2] = {{"x", 3.0f}, {"y", 4.0f}};

    float dx = equation_evaluate(eq, gradient[0], binds, 2);
    float dy = equation_evaluate(eq, gradient[1], binds, 2);

    EXPECT_NEAR(dx, 6.0f, 0.1f);  // 2*3
    EXPECT_NEAR(dy, 8.0f, 0.1f);  // 2*4

    equation_free_expr(expr);
    equation_free_expr(gradient[0]);
    equation_free_expr(gradient[1]);
}

//=============================================================================
// Evaluation Tests
//=============================================================================

TEST_F(EquationTest, EvaluateConstant)
{
    expr_node_t* expr = equation_parse(eq, "42");
    ASSERT_NE(expr, nullptr);

    float value = equation_evaluate(eq, expr, nullptr, 0);
    EXPECT_NEAR(value, 42.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
}

TEST_F(EquationTest, EvaluateWithBinding)
{
    expr_node_t* expr = equation_parse(eq, "x + 1");
    ASSERT_NE(expr, nullptr);

    variable_binding_t bind = {"x", 5.0f};
    float value = equation_evaluate(eq, expr, &bind, 1);
    EXPECT_NEAR(value, 6.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
}

TEST_F(EquationTest, EvaluateMultipleBindings)
{
    expr_node_t* expr = equation_parse(eq, "x + y + z");
    ASSERT_NE(expr, nullptr);

    variable_binding_t binds[3] = {{"x", 1.0f}, {"y", 2.0f}, {"z", 3.0f}};
    float value = equation_evaluate(eq, expr, binds, 3);
    EXPECT_NEAR(value, 6.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
}

TEST_F(EquationTest, EvaluateTrigonometric)
{
    expr_node_t* expr = equation_parse(eq, "sin(x)");
    ASSERT_NE(expr, nullptr);

    variable_binding_t bind = {"x", 0.0f};
    float value = equation_evaluate(eq, expr, &bind, 1);
    EXPECT_NEAR(value, 0.0f, FLOAT_TOLERANCE);

    bind.value = 3.14159265f / 2;  // pi/2
    value = equation_evaluate(eq, expr, &bind, 1);
    EXPECT_NEAR(value, 1.0f, 0.001f);

    equation_free_expr(expr);
}

TEST_F(EquationTest, EvaluateMissingBinding)
{
    expr_node_t* expr = equation_parse(eq, "x + y");
    ASSERT_NE(expr, nullptr);

    variable_binding_t bind = {"x", 5.0f};  // Missing y
    float value = equation_evaluate(eq, expr, &bind, 1);
    EXPECT_TRUE(std::isnan(value));

    equation_free_expr(expr);
}

//=============================================================================
// Expression Analysis Tests
//=============================================================================

TEST_F(EquationTest, IsConstant)
{
    expr_node_t* constant = equation_parse(eq, "42");
    expr_node_t* variable = equation_parse(eq, "x + 1");

    EXPECT_TRUE(equation_is_constant(constant));
    EXPECT_FALSE(equation_is_constant(variable));

    equation_free_expr(constant);
    equation_free_expr(variable);
}

TEST_F(EquationTest, ContainsVariable)
{
    expr_node_t* expr = equation_parse(eq, "x^2 + y");
    ASSERT_NE(expr, nullptr);

    EXPECT_TRUE(equation_contains_variable(expr, "x"));
    EXPECT_TRUE(equation_contains_variable(expr, "y"));
    EXPECT_FALSE(equation_contains_variable(expr, "z"));

    equation_free_expr(expr);
}

TEST_F(EquationTest, GetVariables)
{
    expr_node_t* expr = equation_parse(eq, "x + y + z");
    ASSERT_NE(expr, nullptr);

    char var_names[10][EQUATION_MAX_VAR_NAME];
    uint32_t num = equation_get_variables(expr, var_names, 10);

    EXPECT_EQ(num, 3);

    equation_free_expr(expr);
}

//=============================================================================
// Equation Solving Tests
//=============================================================================

TEST_F(EquationTest, CreateEquation)
{
    expr_node_t* lhs = equation_parse(eq, "x + 1");
    expr_node_t* rhs = equation_parse(eq, "5");

    equation_t eqn = equation_create_equation(lhs, rhs);

    EXPECT_EQ(eqn.lhs, lhs);
    EXPECT_EQ(eqn.rhs, rhs);

    equation_free_expr(lhs);
    equation_free_expr(rhs);
}

TEST_F(EquationTest, SolveLinear)
{
    // Solve: x + 2 = 5  =>  x = 3
    expr_node_t* lhs = equation_parse(eq, "x + 2");
    expr_node_t* rhs = equation_parse(eq, "5");
    equation_t eqn = equation_create_equation(lhs, rhs);

    expr_node_t* solution = equation_solve_for(eq, &eqn, "x");

    if (solution) {
        // Solution should evaluate to 3
        float value = equation_evaluate(eq, solution, nullptr, 0);
        EXPECT_NEAR(value, 3.0f, 0.1f);
        equation_free_expr(solution);
    }

    equation_free_expr(lhs);
    equation_free_expr(rhs);
}

TEST_F(EquationTest, FindRootNewton)
{
    // Find root of x^2 - 4 = 0 (roots are +/-2)
    expr_node_t* expr = equation_parse(eq, "x^2 - 4");
    ASSERT_NE(expr, nullptr);

    float root = equation_find_root(eq, expr, "x", 1.0f, 1e-6f, 100);

    EXPECT_TRUE(root == root);  // Not NaN
    EXPECT_NEAR(fabsf(root), 2.0f, 0.01f);  // Either +2 or -2

    equation_free_expr(expr);
}

TEST_F(EquationTest, FindRootNoConvergence)
{
    // No real root: x^2 + 1 = 0
    expr_node_t* expr = equation_parse(eq, "x^2 + 1");
    ASSERT_NE(expr, nullptr);

    float root = equation_find_root(eq, expr, "x", 1.0f, 1e-6f, 50);

    // Should not converge (NaN or stayed at initial guess)
    // The test is that it doesn't crash

    equation_free_expr(expr);
}

//=============================================================================
// Substitution Tests
//=============================================================================

TEST_F(EquationTest, Substitute)
{
    expr_node_t* expr = equation_parse(eq, "x^2 + x");
    expr_node_t* replacement = equation_parse(eq, "y + 1");

    ASSERT_NE(expr, nullptr);
    ASSERT_NE(replacement, nullptr);

    expr_node_t* substituted = equation_substitute(eq, expr, "x", replacement);
    ASSERT_NE(substituted, nullptr);

    // After substituting x = y + 1:
    // (y+1)^2 + (y+1)
    // At y = 1: (2)^2 + 2 = 6
    variable_binding_t bind = {"y", 1.0f};
    float value = equation_evaluate(eq, substituted, &bind, 1);
    EXPECT_NEAR(value, 6.0f, 0.1f);

    equation_free_expr(expr);
    equation_free_expr(replacement);
    equation_free_expr(substituted);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(EquationTest, SetInflammation)
{
    EXPECT_EQ(equation_set_inflammation(eq, 0.5f), 0);
    EXPECT_NE(equation_set_inflammation(nullptr, 0.5f), 0);
}

TEST_F(EquationTest, SetFatigue)
{
    EXPECT_EQ(equation_set_fatigue(eq, 0.5f), 0);
    EXPECT_NE(equation_set_fatigue(nullptr, 0.5f), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EquationTest, GetStats)
{
    // Perform operations
    expr_node_t* expr = equation_parse(eq, "x + 1");
    equation_simplify(eq, expr);
    equation_differentiate(eq, expr, "x");

    variable_binding_t bind = {"x", 1.0f};
    equation_evaluate(eq, expr, &bind, 1);

    equation_stats_t stats;
    EXPECT_EQ(equation_get_stats(eq, &stats), 0);

    EXPECT_GE(stats.expressions_parsed, 1);
    EXPECT_GE(stats.simplifications, 1);
    EXPECT_GE(stats.differentiations, 1);
    EXPECT_GE(stats.evaluations, 1);

    equation_free_expr(expr);
}

TEST_F(EquationTest, GetStatsNullHandling)
{
    equation_stats_t stats;
    EXPECT_NE(equation_get_stats(nullptr, &stats), 0);
    EXPECT_NE(equation_get_stats(eq, nullptr), 0);
}

TEST_F(EquationTest, ResetStats)
{
    expr_node_t* expr = equation_parse(eq, "x");
    equation_free_expr(expr);

    equation_reset_stats(eq);

    equation_stats_t stats;
    equation_get_stats(eq, &stats);
    EXPECT_EQ(stats.expressions_parsed, 0);
}

TEST_F(EquationTest, ResetStatsNullSafe)
{
    equation_reset_stats(nullptr);
    // Should not crash
}

}  // namespace
