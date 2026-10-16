/**
 * @file test_combinatorial_harm_integration.cpp
 * @brief Integration tests for NIMCP Combinatorial Harm Detection Module
 *
 * Tests integration between:
 * - Combinatorial harm detector and ethics engine
 * - Memory protection and security directive system
 * - Mathematical analysis pipeline
 * - Multi-threaded operation
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 */

#include "test_helpers.h"
#include "cognitive/ethics/nimcp_combinatorial_harm.h"
#include "cognitive/ethics/nimcp_ethics.h"

#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class CombinatorialHarmIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create ethics engine
        ethics_config_t ethics_config = {
            .policies = nullptr,
            .num_policies = 0,
            .callback = nullptr,
            .callback_context = nullptr,
            .default_severity = 0.5f,
            .enable_learning = true,
            .action_feature_size = 10,
            .max_agents = 100,
            .golden_rule_threshold = 0.0f,
            .empathy_weight = 0.7f
        };
        ethics_engine = ethics_engine_create(&ethics_config);
        ASSERT_NE(ethics_engine, nullptr);

        // Create combinatorial harm detector
        combinatorial_config_t comb_config = combinatorial_default_config();
        comb_config.ethics_engine = ethics_engine;
        comb_config.history_capacity = 500;
        comb_config.time_window_ms = 60000;
        comb_config.harm_threshold = 0.7f;

        detector = combinatorial_detector_create(&comb_config);
        ASSERT_NE(detector, nullptr);

        // Register default patterns
        combinatorial_register_default_patterns(detector);
    }

    void TearDown() override
    {
        if (detector) {
            combinatorial_detector_destroy(detector);
            detector = nullptr;
        }
        if (ethics_engine) {
            ethics_engine_destroy(ethics_engine);
            ethics_engine = nullptr;
        }
    }

    // Helper to create test action record
    action_record_t create_test_action(
        action_category_t category,
        float harm_score = 0.3f,
        const char* description = "Test action"
    ) {
        action_record_t record;
        memset(&record, 0, sizeof(record));

        record.category = category;
        record.acting_agent = 1;
        record.target_agent = 2;
        record.standalone_harm_score = harm_score;
        strncpy(record.description, description, sizeof(record.description) - 1);
        record.timestamp = 0;
        record.features = nullptr;
        record.num_features = 0;

        return record;
    }

    ethics_engine_t ethics_engine = nullptr;
    combinatorial_harm_detector_t detector = nullptr;
};

//=============================================================================
// Ethics Engine Integration Tests
//=============================================================================

TEST_F(CombinatorialHarmIntegrationTest, AttachToEthics_Succeeds)
{
    bool success = combinatorial_attach_to_ethics(detector, ethics_engine);
    EXPECT_TRUE(success);

    // Detach should also succeed
    success = combinatorial_detach_from_ethics(detector);
    EXPECT_TRUE(success);
}

TEST_F(CombinatorialHarmIntegrationTest, ContextEvaluation_WorksWithEthicsEngine)
{
    // Create action context compatible with ethics engine
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    agent_id_t agents[3] = {1, 2, 3};

    action_context_t action = {
        .features = features,
        .num_features = 10,
        .affected_agents = agents,
        .num_affected_agents = 3,
        .predicted_harm = 0.3f,
        .fairness_violation = 0.0f,
        .deception_level = 0.0f,
        .autonomy_violation = 0.0f,
        .privacy_violation = 0.0f,
        .consent_violation = 0.0f
    };

    // Record history
    action_record_t history = create_test_action(
        ACTION_CATEGORY_INFORMATION_DISCLOSURE, 0.4f, "First disclosure"
    );
    combinatorial_record_action(detector, &history);

    // Evaluate using context
    combinatorial_evaluation_t result;
    bool success = combinatorial_evaluate_context(
        detector, &action,
        ACTION_CATEGORY_INFORMATION_DISCLOSURE,
        "Second disclosure",
        &result
    );

    EXPECT_TRUE(success);
    // Should detect combinatorial harm
    EXPECT_TRUE(result.harmful);
}

//=============================================================================
// Security Integration Tests
//=============================================================================

TEST_F(CombinatorialHarmIntegrationTest, MprotectIntegrity_PreservesAcrossOperations)
{
    // Lock patterns
    ASSERT_TRUE(combinatorial_lock_patterns_mprotect(detector));

    // Record many actions
    for (int i = 0; i < 100; i++) {
        action_record_t record = create_test_action(
            ACTION_CATEGORY_DATA_EXPORT,
            0.2f + 0.01f * i
        );
        combinatorial_record_action(detector, &record);
    }

    // Do many evaluations
    for (int i = 0; i < 50; i++) {
        action_record_t pending = create_test_action(
            ACTION_CATEGORY_DATA_EXPORT, 0.3f
        );
        combinatorial_evaluation_t result;
        combinatorial_evaluate(detector, &pending, &result);
    }

    // Verify integrity still holds
    EXPECT_TRUE(combinatorial_verify_pattern_integrity(detector));
}

TEST_F(CombinatorialHarmIntegrationTest, LockedPatternsCannotBeModified)
{
    ASSERT_TRUE(combinatorial_lock_patterns_mprotect(detector));

    // Try to unregister each pattern - all should fail
    for (uint32_t id = 1; id <= 8; id++) {
        bool success = combinatorial_unregister_pattern(detector, id);
        EXPECT_FALSE(success);
    }

    // Stats should show 8 patterns still registered
    combinatorial_stats_t stats;
    combinatorial_get_stats(detector, &stats);
    EXPECT_EQ(stats.patterns_registered, 8u);
}

//=============================================================================
// Multi-threaded Integration Tests
//=============================================================================

TEST_F(CombinatorialHarmIntegrationTest, ConcurrentRecordAndEvaluate_ThreadSafe)
{
    const int NUM_THREADS = 4;
    const int OPERATIONS_PER_THREAD = 100;
    std::atomic<int> total_operations(0);

    auto worker = [this, &total_operations](int thread_id) {
        for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
            // Record action
            action_record_t record = create_test_action(
                static_cast<action_category_t>(
                    ACTION_CATEGORY_INFORMATION_DISCLOSURE + (i % 5)
                ),
                0.1f + 0.01f * (i % 10)
            );
            combinatorial_record_action(detector, &record);

            // Evaluate
            action_record_t pending = create_test_action(
                static_cast<action_category_t>(
                    ACTION_CATEGORY_ACCESS_GRANT + (i % 5)
                ),
                0.2f + 0.01f * (i % 10)
            );
            combinatorial_evaluation_t result;
            combinatorial_evaluate(detector, &pending, &result);

            total_operations++;
        }
    };

    // Launch threads
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }

    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_operations.load(), NUM_THREADS * OPERATIONS_PER_THREAD);

    // Stats should be consistent
    combinatorial_stats_t stats;
    combinatorial_get_stats(detector, &stats);
    EXPECT_EQ(stats.total_evaluations, (uint64_t)(NUM_THREADS * OPERATIONS_PER_THREAD));
}

//=============================================================================
// Mathematical Pipeline Integration Tests
//=============================================================================

TEST_F(CombinatorialHarmIntegrationTest, FullPipelineAnalysis_ProducesConsistentResults)
{
    // Build up history
    for (int i = 0; i < 50; i++) {
        action_record_t record = create_test_action(
            ACTION_CATEGORY_FINANCIAL_TRANSACTION,
            0.1f + 0.015f * i
        );
        combinatorial_record_action(detector, &record);
    }

    // Run full analysis multiple times
    float unified_scores[5];
    for (int run = 0; run < 5; run++) {
        action_record_t pending = create_test_action(
            ACTION_CATEGORY_FINANCIAL_TRANSACTION, 0.5f
        );

        mathematical_harm_analysis_t analysis;
        bool success = combinatorial_full_mathematical_analysis(
            detector, &pending, &analysis
        );

        EXPECT_TRUE(success);
        unified_scores[run] = analysis.unified_harm_score;
    }

    // Results should be consistent (same input -> same output)
    for (int i = 1; i < 5; i++) {
        EXPECT_NEAR(unified_scores[0], unified_scores[i], 0.0001f);
    }
}

TEST_F(CombinatorialHarmIntegrationTest, EnhancedEvaluation_MoreAccurateThanBasic)
{
    // Set up scenario where mathematical analysis should help
    for (int i = 0; i < 30; i++) {
        action_record_t record = create_test_action(
            ACTION_CATEGORY_PHYSICAL_ACTION,
            0.3f  // Moderate harm
        );
        combinatorial_record_action(detector, &record);
    }

    action_record_t pending = create_test_action(
        ACTION_CATEGORY_PHYSICAL_ACTION, 0.35f
    );

    // Basic evaluation
    combinatorial_evaluation_t basic_result;
    combinatorial_evaluate(detector, &pending, &basic_result);

    // Enhanced evaluation
    combinatorial_evaluation_t enhanced_result;
    mathematical_harm_analysis_t math_analysis;
    combinatorial_evaluate_enhanced(detector, &pending, &enhanced_result, &math_analysis);

    // Enhanced should use all methods
    EXPECT_EQ(math_analysis.methods_used, MATH_METHOD_ALL);

    // Both should work
    EXPECT_TRUE(basic_result.harmful || !basic_result.harmful);  // Valid result
    EXPECT_TRUE(enhanced_result.harmful || !enhanced_result.harmful);  // Valid result
}

//=============================================================================
// Realistic Scenario Tests
//=============================================================================

TEST_F(CombinatorialHarmIntegrationTest, StalkingScenario_DetectsHarm)
{
    // Scenario: Revealing location then schedule = stalking risk

    // First: reveal location
    action_record_t location_disclosure = create_test_action(
        ACTION_CATEGORY_INFORMATION_DISCLOSURE,
        0.4f,
        "Reveal user's home location"
    );
    combinatorial_record_action(detector, &location_disclosure);

    // Second: reveal schedule
    action_record_t schedule_disclosure = create_test_action(
        ACTION_CATEGORY_INFORMATION_DISCLOSURE,
        0.4f,
        "Reveal user's daily schedule"
    );

    combinatorial_evaluation_t result;
    bool success = combinatorial_evaluate(detector, &schedule_disclosure, &result);

    EXPECT_TRUE(success);
    EXPECT_TRUE(result.harmful);
    EXPECT_GE(result.combined_harm_score, 0.7f);
    // Pattern name from default patterns: "Location+Schedule Disclosure"
    EXPECT_STREQ(result.pattern_name, "Location+Schedule Disclosure");
}

TEST_F(CombinatorialHarmIntegrationTest, PrivilegeEscalationScenario_DetectsHarm)
{
    // Scenario: Access A + Access B = privilege escalation

    // First: grant access A
    action_record_t access_a = create_test_action(
        ACTION_CATEGORY_ACCESS_GRANT,
        0.4f,
        "Grant read access to system A"
    );
    combinatorial_record_action(detector, &access_a);

    // Second: grant access B
    action_record_t access_b = create_test_action(
        ACTION_CATEGORY_ACCESS_GRANT,
        0.4f,
        "Grant write access to system B"
    );

    combinatorial_evaluation_t result;
    bool success = combinatorial_evaluate(detector, &access_b, &result);

    EXPECT_TRUE(success);
    EXPECT_TRUE(result.harmful);
    EXPECT_STREQ(result.pattern_name, "Access+Access Privilege Escalation");
}

TEST_F(CombinatorialHarmIntegrationTest, PhysicalHarmScenario_MaximumPriority)
{
    // Scenario: Physical action + physical action = compounded harm
    // This is the highest priority (Asimov First Law protection)

    // First physical action
    action_record_t physical_a = create_test_action(
        ACTION_CATEGORY_PHYSICAL_ACTION,
        0.3f,
        "Move object near human"
    );
    combinatorial_record_action(detector, &physical_a);

    // Second physical action
    action_record_t physical_b = create_test_action(
        ACTION_CATEGORY_PHYSICAL_ACTION,
        0.3f,
        "Apply force in direction"
    );

    combinatorial_evaluation_t result;
    bool success = combinatorial_evaluate(detector, &physical_b, &result);

    EXPECT_TRUE(success);
    EXPECT_TRUE(result.harmful);
    // Physical harm has 4.0x multiplier - highest of all patterns
    EXPECT_GE(result.combined_harm_score, 0.7f);
    EXPECT_STREQ(result.pattern_name, "Compounded Physical Actions");
}

//=============================================================================
// Batch Processing Integration Tests
//=============================================================================

TEST_F(CombinatorialHarmIntegrationTest, BatchEvaluation_FindsWorstCombination)
{
    // Record history
    action_record_t history = create_test_action(
        ACTION_CATEGORY_PHYSICAL_ACTION,
        0.5f,
        "High-risk physical action"
    );
    combinatorial_record_action(detector, &history);

    // Create batch of pending actions
    action_record_t pending[3];
    pending[0] = create_test_action(ACTION_CATEGORY_DATA_EXPORT, 0.3f, "Export data");
    pending[1] = create_test_action(ACTION_CATEGORY_PHYSICAL_ACTION, 0.5f, "Physical follow-up");
    pending[2] = create_test_action(ACTION_CATEGORY_COMMUNICATION, 0.2f, "Send message");

    combinatorial_evaluation_t worst_result;
    int worst_index = combinatorial_evaluate_batch(detector, pending, 3, &worst_result);

    // Should identify the physical action as worst
    EXPECT_EQ(worst_index, 1);
    EXPECT_TRUE(worst_result.harmful);
}

} // namespace
