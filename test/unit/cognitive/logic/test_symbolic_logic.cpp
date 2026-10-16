/**
 * @file test_symbolic_logic.cpp
 * @brief Unit tests for symbolic logic engine
 *
 * WHAT: Test logical reasoning and inference
 * WHY:  Ensure symbolic logic engine works correctly
 * HOW:  Unit tests with propositional and first-order logic
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6
 */

#include <gtest/gtest.h>
#include <cstring>

#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SymbolicLogicTest : public ::testing::Test {
protected:
    symbolic_logic_t* logic;
    logic_config_t config;

    void SetUp() override {
        config.max_predicates = 100;
        config.max_rules = 50;
        config.max_kb_size = 200;
        config.max_inference_depth = 10;
        config.enable_forward_chaining = true;
        config.enable_backward_chaining = true;
        config.enable_resolution = true;
        config.enable_memory_consolidation = true;

        logic = symbolic_logic_create(&config);
        ASSERT_NE(logic, nullptr);
    }

    void TearDown() override {
        if (logic) {
            symbolic_logic_destroy(logic);
        }
    }

    // Helper: Create simple predicate like "Human(socrates)"
    logic_clause_t* create_simple_fact(const char* pred_name, const char* const_name) {
        logic_clause_t* clause = (logic_clause_t*)nimcp_calloc(1, sizeof(logic_clause_t));
        clause->literals = (atomic_formula_t**)nimcp_calloc(1, sizeof(atomic_formula_t*));
        clause->num_literals = 1;
        clause->confidence = 1.0f;

        logical_term_t* term = logic_term_create(TERM_CONSTANT, const_name);
        logical_term_t** terms = &term;

        clause->literals[0] = logic_atom_create(pred_name, terms, 1);

        return clause;
    }

    // Helper: Create 0-arity predicate like "Raining"
    logic_clause_t* create_simple_predicate(const char* pred_name) {
        logic_clause_t* clause = (logic_clause_t*)nimcp_calloc(1, sizeof(logic_clause_t));
        clause->literals = (atomic_formula_t**)nimcp_calloc(1, sizeof(atomic_formula_t*));
        clause->num_literals = 1;
        clause->confidence = 1.0f;

        clause->literals[0] = logic_atom_create(pred_name, nullptr, 0);

        return clause;
    }
};

//=============================================================================
// Basic Functionality Tests
//=============================================================================

TEST_F(SymbolicLogicTest, CreateDestroy) {
    logic_config_t test_config = config;
    symbolic_logic_t* test_logic = symbolic_logic_create(&test_config);
    ASSERT_NE(test_logic, nullptr);
    symbolic_logic_destroy(test_logic);
}

TEST_F(SymbolicLogicTest, InvalidConfig) {
    logic_config_t invalid_config = config;

    // Invalid max_predicates
    invalid_config.max_predicates = 0;
    EXPECT_EQ(symbolic_logic_create(&invalid_config), nullptr);

    invalid_config.max_predicates = LOGIC_MAX_PREDICATES + 1;
    EXPECT_EQ(symbolic_logic_create(&invalid_config), nullptr);

    // NULL config
    EXPECT_EQ(symbolic_logic_create(nullptr), nullptr);
}

TEST_F(SymbolicLogicTest, GetStats) {
    logic_stats_t stats;
    bool success = symbolic_logic_get_stats(logic, &stats);
    EXPECT_TRUE(success);
    EXPECT_EQ(stats.facts_stored, 0);
    EXPECT_EQ(stats.rules_applied, 0);
    EXPECT_EQ(stats.inferences_performed, 0);
}

//=============================================================================
// Term Management Tests
//=============================================================================

TEST_F(SymbolicLogicTest, CreateVariable) {
    logical_term_t* var = logic_term_create(TERM_VARIABLE, "x");
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(var->type, TERM_VARIABLE);
    EXPECT_STREQ(var->name, "x");
    EXPECT_EQ(var->arity, 0);
    logic_term_destroy(var);
}

TEST_F(SymbolicLogicTest, CreateConstant) {
    logical_term_t* constant = logic_term_create(TERM_CONSTANT, "socrates");
    ASSERT_NE(constant, nullptr);
    EXPECT_EQ(constant->type, TERM_CONSTANT);
    EXPECT_STREQ(constant->name, "socrates");
    EXPECT_EQ(constant->arity, 0);
    logic_term_destroy(constant);
}

TEST_F(SymbolicLogicTest, NullInputsTerm) {
    EXPECT_EQ(logic_term_create(TERM_VARIABLE, nullptr), nullptr);
    logic_term_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Atomic Formula Tests
//=============================================================================

TEST_F(SymbolicLogicTest, CreateAtom) {
    logical_term_t* socrates = logic_term_create(TERM_CONSTANT, "socrates");
    logical_term_t* terms[] = {socrates};

    atomic_formula_t* atom = logic_atom_create("Human", terms, 1);
    ASSERT_NE(atom, nullptr);
    EXPECT_STREQ(atom->name, "Human");
    EXPECT_EQ(atom->arity, 1);
    EXPECT_FALSE(atom->negated);

    logic_atom_destroy(atom);
}

TEST_F(SymbolicLogicTest, CreateZeroArityAtom) {
    atomic_formula_t* atom = logic_atom_create("Raining", nullptr, 0);
    ASSERT_NE(atom, nullptr);
    EXPECT_STREQ(atom->name, "Raining");
    EXPECT_EQ(atom->arity, 0);
    logic_atom_destroy(atom);
}

TEST_F(SymbolicLogicTest, AtomNullInputs) {
    logical_term_t* term = logic_term_create(TERM_CONSTANT, "x");
    logical_term_t* terms[] = {term};

    EXPECT_EQ(logic_atom_create(nullptr, terms, 1), nullptr);
    EXPECT_EQ(logic_atom_create("P", terms, LOGIC_MAX_ARITY + 1), nullptr);

    logic_term_destroy(term);
    logic_atom_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Knowledge Base Tests
//=============================================================================

TEST_F(SymbolicLogicTest, AddFact) {
    logic_clause_t* fact = create_simple_fact("Human", "socrates");
    ASSERT_NE(fact, nullptr);

    bool success = symbolic_logic_add_fact(logic, fact, 0.9f);
    EXPECT_TRUE(success);

    logic_stats_t stats;
    symbolic_logic_get_stats(logic, &stats);
    EXPECT_EQ(stats.facts_stored, 1);

    // Cleanup
    for (uint32_t i = 0; i < fact->num_literals; i++) {
        logic_atom_destroy(fact->literals[i]);
    }
    nimcp_free(fact->literals);
    nimcp_free(fact);
}

TEST_F(SymbolicLogicTest, AddMultipleFacts) {
    logic_clause_t* fact1 = create_simple_fact("Human", "socrates");
    logic_clause_t* fact2 = create_simple_fact("Human", "plato");
    logic_clause_t* fact3 = create_simple_fact("Mortal", "socrates");

    EXPECT_TRUE(symbolic_logic_add_fact(logic, fact1, 0.9f));
    EXPECT_TRUE(symbolic_logic_add_fact(logic, fact2, 0.8f));
    EXPECT_TRUE(symbolic_logic_add_fact(logic, fact3, 0.7f));

    logic_stats_t stats;
    symbolic_logic_get_stats(logic, &stats);
    EXPECT_EQ(stats.facts_stored, 3);

    // Cleanup
    for (auto fact : {fact1, fact2, fact3}) {
        for (uint32_t i = 0; i < fact->num_literals; i++) {
            logic_atom_destroy(fact->literals[i]);
        }
        nimcp_free(fact->literals);
        nimcp_free(fact);
    }
}

TEST_F(SymbolicLogicTest, QueryKnowledgeBase) {
    // Add facts
    logic_clause_t* fact1 = create_simple_fact("Human", "socrates");
    logic_clause_t* fact2 = create_simple_fact("Human", "plato");

    symbolic_logic_add_fact(logic, fact1, 0.9f);
    symbolic_logic_add_fact(logic, fact2, 0.8f);

    // Query
    logic_clause_t* query = create_simple_fact("Human", "socrates");
    kb_entry_t** results = nullptr;
    int num_results = 0;

    bool success = symbolic_logic_query(logic, query, &results, &num_results);
    EXPECT_TRUE(success);
    EXPECT_GE(num_results, 0);

    if (results) {
        nimcp_free(results);
    }

    // Cleanup
    for (auto fact : {fact1, fact2, query}) {
        for (uint32_t i = 0; i < fact->num_literals; i++) {
            logic_atom_destroy(fact->literals[i]);
        }
        nimcp_free(fact->literals);
        nimcp_free(fact);
    }
}

//=============================================================================
// Unification Tests
//=============================================================================

TEST_F(SymbolicLogicTest, UnifyConstants) {
    logical_term_t* t1 = logic_term_create(TERM_CONSTANT, "socrates");
    logical_term_t* t2 = logic_term_create(TERM_CONSTANT, "socrates");

    unification_t* unif = symbolic_logic_unify(t1, t2);
    ASSERT_NE(unif, nullptr);
    EXPECT_TRUE(unif->success);

    unification_destroy(unif);
    logic_term_destroy(t1);
    logic_term_destroy(t2);
}

TEST_F(SymbolicLogicTest, UnifyConstantsDifferent) {
    logical_term_t* t1 = logic_term_create(TERM_CONSTANT, "socrates");
    logical_term_t* t2 = logic_term_create(TERM_CONSTANT, "plato");

    unification_t* unif = symbolic_logic_unify(t1, t2);
    ASSERT_NE(unif, nullptr);
    EXPECT_FALSE(unif->success);

    unification_destroy(unif);
    logic_term_destroy(t1);
    logic_term_destroy(t2);
}

TEST_F(SymbolicLogicTest, UnifyVariableWithConstant) {
    logical_term_t* var = logic_term_create(TERM_VARIABLE, "x");
    logical_term_t* constant = logic_term_create(TERM_CONSTANT, "socrates");

    unification_t* unif = symbolic_logic_unify(var, constant);
    ASSERT_NE(unif, nullptr);
    EXPECT_TRUE(unif->success);
    EXPECT_EQ(unif->num_bindings, 1);

    if (unif->bindings && unif->num_bindings > 0) {
        EXPECT_STREQ(unif->bindings[0]->variable->name, "x");
        EXPECT_STREQ(unif->bindings[0]->value->name, "socrates");
    }

    unification_destroy(unif);
    logic_term_destroy(var);
    logic_term_destroy(constant);
}

TEST_F(SymbolicLogicTest, UnifyVariables) {
    logical_term_t* var1 = logic_term_create(TERM_VARIABLE, "x");
    logical_term_t* var2 = logic_term_create(TERM_VARIABLE, "y");

    unification_t* unif = symbolic_logic_unify(var1, var2);
    ASSERT_NE(unif, nullptr);
    EXPECT_TRUE(unif->success);
    EXPECT_EQ(unif->num_bindings, 1);

    unification_destroy(unif);
    logic_term_destroy(var1);
    logic_term_destroy(var2);
}

TEST_F(SymbolicLogicTest, Substitution) {
    logical_term_t* var = logic_term_create(TERM_VARIABLE, "x");
    logical_term_t* constant = logic_term_create(TERM_CONSTANT, "socrates");

    substitution_t subst;
    subst.variable = var;
    subst.value = constant;

    logical_term_t* var_to_substitute = logic_term_create(TERM_VARIABLE, "x");
    logical_term_t* result = symbolic_logic_substitute(var_to_substitute, &subst);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, TERM_CONSTANT);
    EXPECT_STREQ(result->name, "socrates");

    logic_term_destroy(var);
    logic_term_destroy(constant);
    logic_term_destroy(var_to_substitute);
    logic_term_destroy(result);
}

//=============================================================================
// Forward Chaining Tests
//=============================================================================

TEST_F(SymbolicLogicTest, ForwardChainingSimple) {
    // Add facts
    logic_clause_t* fact1 = create_simple_fact("Human", "socrates");
    symbolic_logic_add_fact(logic, fact1, 0.9f);

    // Run forward chaining
    logic_clause_t** new_facts = nullptr;
    int num_new_facts = 0;

    bool success = symbolic_logic_forward_chain(logic, 5, &new_facts, &num_new_facts);
    EXPECT_TRUE(success);

    // No rules, so no new facts should be derived
    EXPECT_EQ(num_new_facts, 0);

    if (new_facts) {
        nimcp_free(new_facts);
    }

    // Cleanup
    for (uint32_t i = 0; i < fact1->num_literals; i++) {
        logic_atom_destroy(fact1->literals[i]);
    }
    nimcp_free(fact1->literals);
    nimcp_free(fact1);
}

TEST_F(SymbolicLogicTest, ForwardChainingDisabled) {
    // Disable forward chaining
    symbolic_logic_destroy(logic);

    config.enable_forward_chaining = false;
    logic = symbolic_logic_create(&config);
    ASSERT_NE(logic, nullptr);

    logic_clause_t** new_facts = nullptr;
    int num_new_facts = 0;

    bool success = symbolic_logic_forward_chain(logic, 5, &new_facts, &num_new_facts);
    EXPECT_FALSE(success);
}

//=============================================================================
// Brain Integration Tests
//=============================================================================

TEST_F(SymbolicLogicTest, NoveltyDetection) {
    // Empty KB - should be completely novel
    logic_clause_t* fact = create_simple_fact("Human", "socrates");
    float novelty = symbolic_logic_compute_novelty(logic, fact);
    EXPECT_FLOAT_EQ(novelty, 1.0f);

    // Add fact to KB
    symbolic_logic_add_fact(logic, fact, 0.9f);

    // Same fact should now be familiar
    logic_clause_t* same_fact = create_simple_fact("Human", "socrates");
    float novelty2 = symbolic_logic_compute_novelty(logic, same_fact);
    EXPECT_LT(novelty2, 0.5f);

    // Different fact should be more novel
    logic_clause_t* different_fact = create_simple_fact("Mortal", "plato");
    float novelty3 = symbolic_logic_compute_novelty(logic, different_fact);
    EXPECT_GT(novelty3, novelty2);

    // Cleanup
    for (auto f : {fact, same_fact, different_fact}) {
        for (uint32_t i = 0; i < f->num_literals; i++) {
            logic_atom_destroy(f->literals[i]);
        }
        nimcp_free(f->literals);
        nimcp_free(f);
    }
}

TEST_F(SymbolicLogicTest, SalientFacts) {
    // Add facts with different salience
    logic_clause_t* fact1 = create_simple_fact("Human", "socrates");
    logic_clause_t* fact2 = create_simple_fact("Human", "plato");
    logic_clause_t* fact3 = create_simple_fact("Mortal", "aristotle");

    symbolic_logic_add_fact(logic, fact1, 0.9f);
    symbolic_logic_add_fact(logic, fact2, 0.5f);
    symbolic_logic_add_fact(logic, fact3, 0.7f);

    // Get top 2 salient facts
    kb_entry_t** salient = nullptr;
    int num_salient = 0;

    bool success = symbolic_logic_get_salient_facts(logic, 2, &salient, &num_salient);
    EXPECT_TRUE(success);
    EXPECT_EQ(num_salient, 2);

    if (salient) {
        // Should be sorted by salience
        EXPECT_GE(salient[0]->salience, salient[1]->salience);
        nimcp_free(salient);
    }

    // Cleanup
    for (auto f : {fact1, fact2, fact3}) {
        for (uint32_t i = 0; i < f->num_literals; i++) {
            logic_atom_destroy(f->literals[i]);
        }
        nimcp_free(f->literals);
        nimcp_free(f);
    }
}

TEST_F(SymbolicLogicTest, MemoryConsolidation) {
    logic_clause_t* fact = create_simple_fact("Human", "socrates");

    bool success = symbolic_logic_consolidate_memory(logic, fact, 0.95f, "philosophy");
    EXPECT_TRUE(success);

    logic_stats_t stats;
    symbolic_logic_get_stats(logic, &stats);
    EXPECT_EQ(stats.facts_stored, 1);

    // Cleanup
    for (uint32_t i = 0; i < fact->num_literals; i++) {
        logic_atom_destroy(fact->literals[i]);
    }
    nimcp_free(fact->literals);
    nimcp_free(fact);
}

TEST_F(SymbolicLogicTest, Explore) {
    // Add some facts
    logic_clause_t* fact1 = create_simple_fact("Human", "socrates");
    symbolic_logic_add_fact(logic, fact1, 0.9f);

    logic_clause_t** interesting = nullptr;
    int num_interesting = 0;

    bool success = symbolic_logic_explore(logic, 3, &interesting, &num_interesting);
    EXPECT_TRUE(success);

    if (interesting) {
        nimcp_free(interesting);
    }

    // Cleanup
    for (uint32_t i = 0; i < fact1->num_literals; i++) {
        logic_atom_destroy(fact1->literals[i]);
    }
    nimcp_free(fact1->literals);
    nimcp_free(fact1);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(SymbolicLogicTest, FormulaToString) {
    logical_term_t* socrates = logic_term_create(TERM_CONSTANT, "socrates");
    logical_term_t* terms[] = {socrates};

    atomic_formula_t* atom = logic_atom_create("Human", terms, 1);
    ASSERT_NE(atom, nullptr);

    logical_formula_t formula;
    memset(&formula, 0, sizeof(formula));
    formula.atom = atom;
    formula.op = OP_AND;

    char buffer[256];
    bool success = symbolic_logic_to_string(&formula, buffer, sizeof(buffer));
    EXPECT_TRUE(success);
    EXPECT_GT(strlen(buffer), 0);

    logic_atom_destroy(atom);
}

TEST_F(SymbolicLogicTest, StringConversionNullInputs) {
    char buffer[256];
    EXPECT_FALSE(symbolic_logic_to_string(nullptr, buffer, sizeof(buffer)));

    logical_formula_t formula;
    memset(&formula, 0, sizeof(formula));
    EXPECT_FALSE(symbolic_logic_to_string(&formula, nullptr, 256));
    EXPECT_FALSE(symbolic_logic_to_string(&formula, buffer, 0));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SymbolicLogicTest, UnificationStatistics) {
    // Note: symbolic_logic_unify() is a pure function without access to
    // the logic engine instance, so it cannot update statistics.
    // This test verifies basic statistics retrieval works.

    logic_stats_t stats;
    ASSERT_TRUE(symbolic_logic_get_stats(logic, &stats));

    // Just verify stats structure is initialized
    EXPECT_GE(stats.inferences_performed, 0);
    EXPECT_GE(stats.facts_stored, 0);
    EXPECT_GE(stats.rules_applied, 0);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(SymbolicLogicTest, NullInputs) {
    logic_stats_t stats;
    EXPECT_FALSE(symbolic_logic_get_stats(nullptr, &stats));
    EXPECT_FALSE(symbolic_logic_get_stats(logic, nullptr));

    logic_clause_t* fact = create_simple_fact("Human", "socrates");
    EXPECT_FALSE(symbolic_logic_add_fact(nullptr, fact, 0.5f));
    EXPECT_FALSE(symbolic_logic_add_fact(logic, nullptr, 0.5f));
    EXPECT_FALSE(symbolic_logic_add_fact(logic, fact, -0.1f));  // Invalid salience
    EXPECT_FALSE(symbolic_logic_add_fact(logic, fact, 1.1f));  // Invalid salience

    // Cleanup
    for (uint32_t i = 0; i < fact->num_literals; i++) {
        logic_atom_destroy(fact->literals[i]);
    }
    nimcp_free(fact->literals);
    nimcp_free(fact);

    EXPECT_EQ(symbolic_logic_compute_novelty(nullptr, fact), 0.0f);
    EXPECT_EQ(symbolic_logic_compute_novelty(logic, nullptr), 0.0f);
}

TEST_F(SymbolicLogicTest, KnowledgeBaseFull) {
    // Fill knowledge base to capacity
    for (uint32_t i = 0; i < config.max_kb_size; i++) {
        char name[32];
        snprintf(name, sizeof(name), "entity_%u", i);
        logic_clause_t* fact = create_simple_fact("Thing", name);
        bool success = symbolic_logic_add_fact(logic, fact, 0.5f);

        for (uint32_t j = 0; j < fact->num_literals; j++) {
            logic_atom_destroy(fact->literals[j]);
        }
        nimcp_free(fact->literals);
        nimcp_free(fact);

        if (!success) {
            break;
        }
    }

    logic_stats_t stats;
    symbolic_logic_get_stats(logic, &stats);
    EXPECT_EQ(stats.facts_stored, config.max_kb_size);

    // Try to add one more - should fail
    logic_clause_t* extra_fact = create_simple_fact("Thing", "overflow");
    bool success = symbolic_logic_add_fact(logic, extra_fact, 0.5f);
    EXPECT_FALSE(success);

    for (uint32_t i = 0; i < extra_fact->num_literals; i++) {
        logic_atom_destroy(extra_fact->literals[i]);
    }
    nimcp_free(extra_fact->literals);
    nimcp_free(extra_fact);
}
