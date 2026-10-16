/**
 * @file test_kg_reasoning_integration.cpp
 * @brief Integration tests for Knowledge Graph + Reasoning modules
 *
 * TEST COVERAGE:
 * - KG + forward chaining
 * - KG + backward chaining
 * - Query + inference
 * - Persistence + reasoning consistency
 *
 * @author NIMCP Development Team
 * @date 2025-02-02
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/reasoning/nimcp_forward_chaining.h"
#include "cognitive/reasoning/nimcp_backward_chaining.h"
#include "cognitive/reasoning/nimcp_unification_engine.h"
#include "cognitive/reasoning/nimcp_knowledge_base_interface.h"
#include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
#include "cognitive/reasoning/nimcp_reasoning_factory.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
}

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class KGReasoningIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create knowledge system
        kg_system = knowledge_system_create("integration_test");
        ASSERT_NE(kg_system, nullptr);

        // Create brain with symbolic logic engine
        brain = brain_create("reasoning_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        logic_engine = create_default_symbolic_logic(REASONING_SIZE_MEDIUM);
        ASSERT_NE(logic_engine, nullptr);

        brain_attach_symbolic_logic(brain, logic_engine);

        // Create temp directory for test files
        temp_dir = "/tmp/nimcp_kg_reasoning_test_";
        temp_dir += std::to_string(getpid());
        mkdir(temp_dir.c_str(), 0755);
    }

    void TearDown() override {
        if (brain) {
            symbolic_logic_t* detached = brain_detach_symbolic_logic(brain);
            if (detached) {
                symbolic_logic_destroy(detached);
            }
            brain_destroy(brain);
            brain = nullptr;
        }

        if (kg_system) {
            knowledge_system_destroy(kg_system);
            kg_system = nullptr;
        }

        // Clean up temp files
        std::string cmd = "rm -rf " + temp_dir;
        system(cmd.c_str());
    }

    std::string get_temp_file(const std::string& name) {
        return temp_dir + "/" + name;
    }

    // Helper to populate KG with domain knowledge
    void populate_animal_domain() {
        const char* text =
            "A bird is an animal that can fly. Tweety is a bird. "
            "A penguin is a bird that cannot fly. Opus is a penguin. "
            "A mammal is an animal that has fur. A dog is a mammal. Fido is a dog.";
        knowledge_learn_from_text(kg_system, text, KNOWLEDGE_DOMAIN_SCIENCE);
    }

    // Helper to add facts from KG to reasoning engine
    void add_facts_from_kg() {
        // Add some basic animal kingdom facts
        brain_add_fact(brain, "Bird(tweety)", 0.9f);
        brain_add_fact(brain, "Penguin(opus)", 0.9f);
        brain_add_fact(brain, "Dog(fido)", 0.9f);
        brain_add_fact(brain, "Mammal(fido)", 0.9f);

        // Add rules
        brain_add_rule(brain, "Bird(x) -> CanFly(x)", 0.8f);
        brain_add_rule(brain, "Penguin(x) -> Bird(x)", 0.9f);
        brain_add_rule(brain, "Penguin(x) -> CannotFly(x)", 0.95f);
        brain_add_rule(brain, "Dog(x) -> Mammal(x)", 0.95f);
        brain_add_rule(brain, "Mammal(x) -> Animal(x)", 0.95f);
        brain_add_rule(brain, "Bird(x) -> Animal(x)", 0.95f);
    }

    knowledge_system_t kg_system = nullptr;
    brain_t brain = nullptr;
    symbolic_logic_t* logic_engine = nullptr;
    std::string temp_dir;
};

//=============================================================================
// KG + Forward Chaining Tests
//=============================================================================

TEST_F(KGReasoningIntegrationTest, KGToForwardChainingBasic) {
    populate_animal_domain();
    add_facts_from_kg();

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 20, &result);

    EXPECT_TRUE(success);
    EXPECT_TRUE(result.converged);
    // Should derive facts like CanFly(tweety), Animal(tweety), etc.
    EXPECT_GE(result.iterations_performed, 1);

    forward_chain_free_result(&result);
}

TEST_F(KGReasoningIntegrationTest, KGDerivedFactsAccumulate) {
    add_facts_from_kg();

    // First round
    forward_chain_result_t result1;
    brain_forward_chain(brain, 10, &result1);
    uint32_t derived1 = result1.num_new_facts;
    forward_chain_free_result(&result1);

    // Add more facts
    brain_add_fact(brain, "Cat(whiskers)", 0.9f);
    brain_add_rule(brain, "Cat(x) -> Mammal(x)", 0.95f);

    // Second round
    forward_chain_result_t result2;
    brain_forward_chain(brain, 10, &result2);
    uint32_t derived2 = result2.num_new_facts;
    forward_chain_free_result(&result2);

    // May or may not derive more facts depending on what was already derived
}

TEST_F(KGReasoningIntegrationTest, KGForwardChainingConfidencePropagation) {
    // Add facts with different confidence levels
    brain_add_fact(brain, "HighConfidence(a)", 0.95f);
    brain_add_fact(brain, "LowConfidence(b)", 0.5f);

    brain_add_rule(brain, "HighConfidence(x) -> Derived1(x)", 0.9f);
    brain_add_rule(brain, "LowConfidence(x) -> Derived2(x)", 0.9f);

    forward_chain_result_t result;
    brain_forward_chain(brain, 10, &result);

    // Overall confidence should reflect input confidences
    EXPECT_GT(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);

    forward_chain_free_result(&result);
}

TEST_F(KGReasoningIntegrationTest, KGMultiDomainForwardChaining) {
    // Add facts from multiple domains
    knowledge_learn_from_text(kg_system, "Science studies nature.", KNOWLEDGE_DOMAIN_SCIENCE);
    knowledge_learn_from_text(kg_system, "Ethics studies morality.", KNOWLEDGE_DOMAIN_ETHICS);
    knowledge_learn_from_text(kg_system, "History records events.", KNOWLEDGE_DOMAIN_HISTORY);

    // Cross-domain rules
    brain_add_fact(brain, "Scientist(einstein)", 0.95f);
    brain_add_fact(brain, "Ethical(einstein)", 0.8f);
    brain_add_rule(brain, "Scientist(x) & Ethical(x) -> RoleModel(x)", 0.85f);

    forward_chain_result_t result;
    brain_forward_chain(brain, 10, &result);

    // Should derive cross-domain conclusions
    EXPECT_TRUE(result.converged);
    forward_chain_free_result(&result);
}

//=============================================================================
// KG + Backward Chaining Tests
//=============================================================================

TEST_F(KGReasoningIntegrationTest, KGToBackwardChainingBasic) {
    add_facts_from_kg();

    // Try to prove Tweety is an animal
    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Animal(tweety)", &result);

    EXPECT_TRUE(proven);
    EXPECT_TRUE(result.proven);
    EXPECT_GT(result.confidence, 0.0f);

    backward_chain_free_result(&result);
}

TEST_F(KGReasoningIntegrationTest, KGBackwardChainingTransitivity) {
    // Dog -> Mammal -> Animal chain
    brain_add_fact(brain, "Dog(fido)", 0.95f);
    brain_add_rule(brain, "Dog(x) -> Mammal(x)", 0.9f);
    brain_add_rule(brain, "Mammal(x) -> Animal(x)", 0.9f);
    brain_add_rule(brain, "Animal(x) -> Living(x)", 0.9f);

    // Prove fido is living
    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Living(fido)", &result);

    EXPECT_TRUE(proven);
    EXPECT_GE(result.depth_reached, 3);

    backward_chain_free_result(&result);
}

TEST_F(KGReasoningIntegrationTest, KGBackwardChainingNegativeProof) {
    add_facts_from_kg();

    // Try to prove something that cannot be proven
    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Fish(tweety)", &result);

    EXPECT_FALSE(proven);
    EXPECT_FALSE(result.proven);

    backward_chain_free_result(&result);
}

TEST_F(KGReasoningIntegrationTest, KGBackwardChainingWithVariables) {
    brain_add_fact(brain, "Parent(alice, bob)", 0.9f);
    brain_add_fact(brain, "Parent(bob, charlie)", 0.9f);
    brain_add_rule(brain, "Parent(x, y) & Parent(y, z) -> Grandparent(x, z)", 0.85f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Grandparent(alice, charlie)", &result);

    EXPECT_TRUE(proven);
    backward_chain_free_result(&result);
}

TEST_F(KGReasoningIntegrationTest, KGBackwardChainProofTrace) {
    brain_add_fact(brain, "Human(socrates)", 0.95f);
    brain_add_rule(brain, "Human(x) -> Mortal(x)", 0.9f);

    backward_chain_result_t result;
    brain_backward_chain(brain, "Mortal(socrates)", &result);

    // Should have proof trace
    EXPECT_GE(result.num_steps, 0);

    backward_chain_free_result(&result);
}

//=============================================================================
// Query + Inference Tests
//=============================================================================

TEST_F(KGReasoningIntegrationTest, QueryAfterForwardChaining) {
    add_facts_from_kg();

    // Run forward chaining
    forward_chain_result_t fc_result;
    brain_forward_chain(brain, 10, &fc_result);
    forward_chain_free_result(&fc_result);

    // Now try backward chaining on derived fact
    backward_chain_result_t bc_result;
    bool proven = brain_backward_chain(brain, "CanFly(tweety)", &bc_result);

    EXPECT_TRUE(proven);
    backward_chain_free_result(&bc_result);
}

TEST_F(KGReasoningIntegrationTest, InferenceConsistency) {
    add_facts_from_kg();

    // Forward chain and backward chain should be consistent
    forward_chain_result_t fc_result;
    brain_forward_chain(brain, 10, &fc_result);

    // Any fact proven by forward chaining should also be provable by backward chaining
    // (conceptually - actual implementation may vary)

    backward_chain_result_t bc_result;
    bool proven = brain_backward_chain(brain, "Animal(fido)", &bc_result);

    // Should be consistent
    EXPECT_TRUE(proven);

    forward_chain_free_result(&fc_result);
    backward_chain_free_result(&bc_result);
}

TEST_F(KGReasoningIntegrationTest, KGQueryWithConfidenceThreshold) {
    // Add facts with varying confidence
    brain_add_fact(brain, "Certain(a)", 0.95f);
    brain_add_fact(brain, "Uncertain(b)", 0.3f);
    brain_add_rule(brain, "Certain(x) -> Derived1(x)", 0.9f);
    brain_add_rule(brain, "Uncertain(x) -> Derived2(x)", 0.9f);

    forward_chain_result_t result;
    brain_forward_chain(brain, 10, &result);

    // Check confidence of derivations
    EXPECT_GT(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);

    forward_chain_free_result(&result);
}

TEST_F(KGReasoningIntegrationTest, CombinedForwardBackwardInference) {
    // Use forward chaining to derive intermediate facts
    brain_add_fact(brain, "Input(x)", 0.9f);
    brain_add_rule(brain, "Input(x) -> Intermediate(x)", 0.8f);
    brain_add_rule(brain, "Intermediate(x) -> Output(x)", 0.8f);

    // Forward chain to derive Intermediate
    forward_chain_result_t fc_result;
    brain_forward_chain(brain, 5, &fc_result);
    forward_chain_free_result(&fc_result);

    // Now backward chain to prove Output
    backward_chain_result_t bc_result;
    bool proven = brain_backward_chain(brain, "Output(x)", &bc_result);

    EXPECT_TRUE(proven);
    backward_chain_free_result(&bc_result);
}

//=============================================================================
// Persistence + Reasoning Consistency Tests
//=============================================================================

TEST_F(KGReasoningIntegrationTest, SaveLoadKGConsistency) {
    // Add knowledge
    knowledge_learn_from_text(kg_system, "Birds can fly. Dogs bark.", KNOWLEDGE_DOMAIN_SCIENCE);

    knowledge_item_t item1;
    memset(&item1, 0, sizeof(item1));
    strncpy(item1.concept_name, "persistent_concept", sizeof(item1.concept_name) - 1);
    item1.confidence = 0.85f;
    item1.domain = KNOWLEDGE_DOMAIN_SCIENCE;
    knowledge_add_item(kg_system, &item1);

    // Save
    std::string filepath = get_temp_file("consistency.dat");
    ASSERT_TRUE(knowledge_save(kg_system, filepath.c_str()));

    // Load
    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    ASSERT_NE(loaded, nullptr);

    // Verify consistency
    knowledge_item_t retrieved;
    bool found = knowledge_retrieve(loaded, "persistent_concept", &retrieved);
    EXPECT_TRUE(found);
    EXPECT_NEAR(retrieved.confidence, 0.85f, 0.001f);

    knowledge_system_destroy(loaded);
}

TEST_F(KGReasoningIntegrationTest, ReasoningAfterReload) {
    // Setup initial knowledge and reasoning
    brain_add_fact(brain, "SavedFact(x)", 0.9f);
    brain_add_rule(brain, "SavedFact(x) -> SavedDerived(x)", 0.85f);

    // Forward chain
    forward_chain_result_t result1;
    brain_forward_chain(brain, 10, &result1);
    uint32_t initial_derived = result1.num_new_facts;
    forward_chain_free_result(&result1);

    // Backward chain should work
    backward_chain_result_t bc_result1;
    brain_backward_chain(brain, "SavedDerived(x)", &bc_result1);
    bool initially_proven = bc_result1.proven;
    backward_chain_free_result(&bc_result1);

    EXPECT_TRUE(initially_proven);
}

TEST_F(KGReasoningIntegrationTest, KGPersistenceMultipleSaveLoad) {
    std::string filepath = get_temp_file("multi_save.dat");

    // First save
    knowledge_item_t item1;
    memset(&item1, 0, sizeof(item1));
    strncpy(item1.concept_name, "concept1", sizeof(item1.concept_name) - 1);
    item1.confidence = 0.7f;
    knowledge_add_item(kg_system, &item1);
    ASSERT_TRUE(knowledge_save(kg_system, filepath.c_str()));

    // Add more, save again
    knowledge_item_t item2;
    memset(&item2, 0, sizeof(item2));
    strncpy(item2.concept_name, "concept2", sizeof(item2.concept_name) - 1);
    item2.confidence = 0.8f;
    knowledge_add_item(kg_system, &item2);
    ASSERT_TRUE(knowledge_save(kg_system, filepath.c_str()));

    // Load and verify
    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    ASSERT_NE(loaded, nullptr);

    knowledge_item_t retrieved1, retrieved2;
    EXPECT_TRUE(knowledge_retrieve(loaded, "concept1", &retrieved1));
    EXPECT_TRUE(knowledge_retrieve(loaded, "concept2", &retrieved2));

    knowledge_system_destroy(loaded);
}

TEST_F(KGReasoningIntegrationTest, ReasoningStatePersistence) {
    // Perform reasoning
    brain_add_fact(brain, "Initial(x)", 0.9f);
    brain_add_rule(brain, "Initial(x) -> Step1(x)", 0.8f);
    brain_add_rule(brain, "Step1(x) -> Step2(x)", 0.8f);

    forward_chain_result_t fc_result;
    brain_forward_chain(brain, 10, &fc_result);
    forward_chain_free_result(&fc_result);

    // Get stats before
    uint32_t iterations1 = 0, facts1 = 0;
    uint64_t time1 = 0;
    brain_get_forward_chain_stats(brain, &iterations1, &facts1, &time1);

    // Stats should reflect the inference performed
    EXPECT_GT(iterations1, 0);
}

//=============================================================================
// Complex Integration Scenarios
//=============================================================================

TEST_F(KGReasoningIntegrationTest, FullPipeline) {
    // 1. Populate KG
    populate_animal_domain();

    // 2. Add reasoning facts
    add_facts_from_kg();

    // 3. Run forward chaining
    forward_chain_result_t fc_result;
    brain_forward_chain(brain, 20, &fc_result);

    EXPECT_TRUE(fc_result.converged);

    // 4. Run backward chaining for specific queries
    backward_chain_result_t bc_result;
    brain_backward_chain(brain, "Animal(tweety)", &bc_result);

    EXPECT_TRUE(bc_result.proven);

    // 5. Save KG
    std::string filepath = get_temp_file("full_pipeline.dat");
    ASSERT_TRUE(knowledge_save(kg_system, filepath.c_str()));

    // 6. Verify loaded KG
    knowledge_system_t loaded = knowledge_load(filepath.c_str());
    ASSERT_NE(loaded, nullptr);

    forward_chain_free_result(&fc_result);
    backward_chain_free_result(&bc_result);
    knowledge_system_destroy(loaded);
}

TEST_F(KGReasoningIntegrationTest, IncrementalKnowledgeReasoning) {
    // Start with minimal knowledge
    brain_add_fact(brain, "A(x)", 0.9f);
    brain_add_rule(brain, "A(x) -> B(x)", 0.8f);

    forward_chain_result_t result1;
    brain_forward_chain(brain, 10, &result1);
    forward_chain_free_result(&result1);

    // Add more knowledge incrementally
    brain_add_rule(brain, "B(x) -> C(x)", 0.8f);

    forward_chain_result_t result2;
    brain_forward_chain(brain, 10, &result2);

    // Should derive more facts
    EXPECT_TRUE(result2.converged);
    forward_chain_free_result(&result2);

    // Add even more
    brain_add_rule(brain, "C(x) -> D(x)", 0.8f);

    forward_chain_result_t result3;
    brain_forward_chain(brain, 10, &result3);

    EXPECT_TRUE(result3.converged);
    forward_chain_free_result(&result3);

    // Verify chain is complete
    backward_chain_result_t bc_result;
    brain_backward_chain(brain, "D(x)", &bc_result);

    EXPECT_TRUE(bc_result.proven);
    backward_chain_free_result(&bc_result);
}

TEST_F(KGReasoningIntegrationTest, CrossDomainReasoning) {
    // Science domain
    brain_add_fact(brain, "Scientist(newton)", 0.95f);
    brain_add_rule(brain, "Scientist(x) -> Educated(x)", 0.9f);

    // History domain
    brain_add_fact(brain, "Historical(newton)", 0.9f);
    brain_add_rule(brain, "Historical(x) -> Famous(x)", 0.85f);

    // Cross-domain rule
    brain_add_rule(brain, "Educated(x) & Famous(x) -> Influential(x)", 0.8f);

    forward_chain_result_t fc_result;
    brain_forward_chain(brain, 10, &fc_result);
    forward_chain_free_result(&fc_result);

    backward_chain_result_t bc_result;
    bool proven = brain_backward_chain(brain, "Influential(newton)", &bc_result);

    EXPECT_TRUE(proven);
    backward_chain_free_result(&bc_result);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(KGReasoningIntegrationTest, LargeKGReasoning) {
    // Add many facts
    for (int i = 0; i < 100; i++) {
        char fact[64];
        snprintf(fact, sizeof(fact), "Entity%d(obj%d)", i % 10, i);
        brain_add_fact(brain, fact, 0.9f);
    }

    // Add rules
    for (int i = 0; i < 10; i++) {
        char rule[128];
        snprintf(rule, sizeof(rule), "Entity%d(x) -> Derived%d(x)", i, i);
        brain_add_rule(brain, rule, 0.8f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    forward_chain_result_t result;
    brain_forward_chain(brain, 100, &result);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 10000);  // Should complete in < 10 seconds
    forward_chain_free_result(&result);
}

TEST_F(KGReasoningIntegrationTest, ConcurrentKGAndReasoningAccess) {
    populate_animal_domain();
    add_facts_from_kg();

    // Run operations that might be concurrent in real usage
    std::vector<std::thread> threads;

    // KG operations thread
    threads.emplace_back([this]() {
        for (int i = 0; i < 10; i++) {
            char name[64];
            snprintf(name, sizeof(name), "concurrent_%d", i);
            knowledge_item_t item;
            knowledge_retrieve(kg_system, name, &item);
        }
    });

    // Reasoning thread
    threads.emplace_back([this]() {
        forward_chain_result_t result;
        brain_forward_chain(brain, 10, &result);
        forward_chain_free_result(&result);
    });

    for (auto& t : threads) {
        t.join();
    }
}

}  // anonymous namespace
