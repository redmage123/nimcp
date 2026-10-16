/**
 * @file test_combinatorial_harm.cpp
 * @brief Unit tests for NIMCP Combinatorial Harm Detection Module
 *
 * Tests the combinatorial harm detection functionality including:
 * - Detector lifecycle (create, configure, destroy)
 * - Pattern registration (default and custom patterns)
 * - Action history management (record, query, clear)
 * - Harm evaluation (single action, batch, enhanced)
 * - Memory locking (mprotect integration)
 * - Mathematical enhancements (Shannon, fractal, hyperbolic, etc.)
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 */

#include "test_helpers.h"
#include "cognitive/ethics/nimcp_combinatorial_harm.h"
#include "cognitive/ethics/nimcp_ethics.h"

#include <cstring>
#include <cmath>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class CombinatorialHarmTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create default configuration
        config = combinatorial_default_config();
        config.history_capacity = 100;
        config.time_window_ms = 60000;  // 1 minute
        config.harm_threshold = 0.7f;

        detector = combinatorial_detector_create(&config);
        ASSERT_NE(detector, nullptr);
    }

    void TearDown() override
    {
        if (detector) {
            combinatorial_detector_destroy(detector);
            detector = nullptr;
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
        record.timestamp = 0;  // Will be set by detector
        record.features = nullptr;
        record.num_features = 0;

        return record;
    }

    // Helper to create custom pattern
    combination_pattern_t create_test_pattern(
        action_category_t cat_a,
        action_category_t cat_b,
        float multiplier = 2.0f,
        bool locked = false
    ) {
        combination_pattern_t pattern;
        memset(&pattern, 0, sizeof(pattern));

        strncpy(pattern.name, "Test Pattern", sizeof(pattern.name) - 1);
        strncpy(pattern.description, "Test pattern for unit testing",
                sizeof(pattern.description) - 1);
        pattern.category_a = cat_a;
        pattern.category_b = cat_b;
        pattern.harm_multiplier = multiplier;
        pattern.time_sensitivity = 0.5f;
        pattern.bidirectional = true;
        pattern.enabled = true;
        pattern.locked = locked;

        return pattern;
    }

    combinatorial_config_t config;
    combinatorial_harm_detector_t detector = nullptr;
};

//=============================================================================
// Detector Lifecycle Tests
//=============================================================================

TEST_F(CombinatorialHarmTest, Create_WithValidConfig_Succeeds)
{
    EXPECT_NE(detector, nullptr);
}

TEST_F(CombinatorialHarmTest, Create_WithNullConfig_ReturnsNull)
{
    combinatorial_harm_detector_t null_detector = combinatorial_detector_create(nullptr);
    EXPECT_EQ(null_detector, nullptr);
}

TEST_F(CombinatorialHarmTest, Create_WithZeroCapacity_ReturnsNull)
{
    combinatorial_config_t bad_config = combinatorial_default_config();
    bad_config.history_capacity = 0;

    combinatorial_harm_detector_t bad_detector = combinatorial_detector_create(&bad_config);
    EXPECT_EQ(bad_detector, nullptr);
}

TEST_F(CombinatorialHarmTest, DefaultConfig_HasSaneValues)
{
    combinatorial_config_t default_config = combinatorial_default_config();

    EXPECT_GT(default_config.history_capacity, 0u);
    EXPECT_GT(default_config.time_window_ms, 0ull);
    EXPECT_GT(default_config.harm_threshold, 0.0f);
    EXPECT_LE(default_config.harm_threshold, 1.0f);
}

//=============================================================================
// Pattern Registration Tests
//=============================================================================

TEST_F(CombinatorialHarmTest, RegisterDefaultPatterns_Returns8)
{
    uint32_t registered = combinatorial_register_default_patterns(detector);
    EXPECT_EQ(registered, 8u);
}

TEST_F(CombinatorialHarmTest, RegisterCustomPattern_Succeeds)
{
    combination_pattern_t pattern = create_test_pattern(
        ACTION_CATEGORY_COMMUNICATION,
        ACTION_CATEGORY_COMMUNICATION,
        2.5f
    );

    uint32_t id = combinatorial_register_pattern(detector, &pattern);
    EXPECT_GT(id, 0u);
}

TEST_F(CombinatorialHarmTest, RegisterPattern_WithNullDetector_ReturnsZero)
{
    combination_pattern_t pattern = create_test_pattern(
        ACTION_CATEGORY_COMMUNICATION,
        ACTION_CATEGORY_COMMUNICATION
    );

    uint32_t id = combinatorial_register_pattern(nullptr, &pattern);
    EXPECT_EQ(id, 0u);
}

TEST_F(CombinatorialHarmTest, UnregisterLockedPattern_Fails)
{
    // Register default patterns (all locked)
    combinatorial_register_default_patterns(detector);

    // Try to unregister pattern 1 (should fail - it's locked)
    bool success = combinatorial_unregister_pattern(detector, 1);
    EXPECT_FALSE(success);
}

TEST_F(CombinatorialHarmTest, UnregisterUnlockedPattern_Succeeds)
{
    // Register an unlocked pattern
    combination_pattern_t pattern = create_test_pattern(
        ACTION_CATEGORY_COMMUNICATION,
        ACTION_CATEGORY_COMMUNICATION,
        2.0f,
        false  // Not locked
    );

    uint32_t id = combinatorial_register_pattern(detector, &pattern);
    EXPECT_GT(id, 0u);

    // Unregister should succeed
    bool success = combinatorial_unregister_pattern(detector, id);
    EXPECT_TRUE(success);
}

//=============================================================================
// Action History Tests
//=============================================================================

TEST_F(CombinatorialHarmTest, RecordAction_ReturnsValidId)
{
    action_record_t record = create_test_action(ACTION_CATEGORY_INFORMATION_DISCLOSURE);

    uint64_t id = combinatorial_record_action(detector, &record);
    EXPECT_GT(id, 0ull);
}

TEST_F(CombinatorialHarmTest, RecordMultipleActions_IdsIncrement)
{
    action_record_t record1 = create_test_action(ACTION_CATEGORY_INFORMATION_DISCLOSURE);
    action_record_t record2 = create_test_action(ACTION_CATEGORY_ACCESS_GRANT);

    uint64_t id1 = combinatorial_record_action(detector, &record1);
    uint64_t id2 = combinatorial_record_action(detector, &record2);

    EXPECT_EQ(id2, id1 + 1);
}

TEST_F(CombinatorialHarmTest, GetHistory_ReturnsRecordedActions)
{
    // Record some actions
    action_record_t record1 = create_test_action(
        ACTION_CATEGORY_INFORMATION_DISCLOSURE,
        0.3f,
        "Action 1"
    );
    action_record_t record2 = create_test_action(
        ACTION_CATEGORY_ACCESS_GRANT,
        0.4f,
        "Action 2"
    );

    combinatorial_record_action(detector, &record1);
    combinatorial_record_action(detector, &record2);

    // Query history
    action_record_t history[10];
    uint32_t count = combinatorial_get_history(detector, 10, history);

    EXPECT_EQ(count, 2u);
}

TEST_F(CombinatorialHarmTest, ClearHistory_RemovesAllActions)
{
    // Record an action
    action_record_t record = create_test_action(ACTION_CATEGORY_DATA_EXPORT);
    combinatorial_record_action(detector, &record);

    // Clear history
    combinatorial_clear_history(detector);

    // Query history - should be empty
    action_record_t history[10];
    uint32_t count = combinatorial_get_history(detector, 10, history);

    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Harm Evaluation Tests
//=============================================================================

TEST_F(CombinatorialHarmTest, Evaluate_WithNoHistory_ReturnsNotHarmful)
{
    // Register patterns
    combinatorial_register_default_patterns(detector);

    // Evaluate pending action with no history
    action_record_t pending = create_test_action(
        ACTION_CATEGORY_INFORMATION_DISCLOSURE,
        0.3f,
        "Reveal schedule"
    );

    combinatorial_evaluation_t result;
    bool success = combinatorial_evaluate(detector, &pending, &result);

    EXPECT_TRUE(success);
    EXPECT_FALSE(result.harmful);
}

TEST_F(CombinatorialHarmTest, Evaluate_WithMatchingHistory_DetectsHarm)
{
    // Register patterns
    combinatorial_register_default_patterns(detector);

    // Record a historical action (information disclosure)
    action_record_t history_action = create_test_action(
        ACTION_CATEGORY_INFORMATION_DISCLOSURE,
        0.4f,  // High enough base harm
        "Reveal location"
    );
    combinatorial_record_action(detector, &history_action);

    // Evaluate pending action (also information disclosure)
    action_record_t pending = create_test_action(
        ACTION_CATEGORY_INFORMATION_DISCLOSURE,
        0.4f,
        "Reveal schedule"
    );

    combinatorial_evaluation_t result;
    bool success = combinatorial_evaluate(detector, &pending, &result);

    EXPECT_TRUE(success);
    // Should detect harm because info+info pattern has 3.0x multiplier
    // 0.4 * 3.0 = 1.2, clamped to 1.0, which exceeds 0.7 threshold
    EXPECT_TRUE(result.harmful);
    EXPECT_GT(result.combined_harm_score, 0.7f);
}

TEST_F(CombinatorialHarmTest, Evaluate_WithNullDetector_ReturnsFalse)
{
    action_record_t pending = create_test_action(ACTION_CATEGORY_INFORMATION_DISCLOSURE);
    combinatorial_evaluation_t result;

    bool success = combinatorial_evaluate(nullptr, &pending, &result);
    EXPECT_FALSE(success);
}

TEST_F(CombinatorialHarmTest, Evaluate_WithNullPendingAction_ReturnsFalse)
{
    combinatorial_evaluation_t result;

    bool success = combinatorial_evaluate(detector, nullptr, &result);
    EXPECT_FALSE(success);
}

//=============================================================================
// Memory Protection Tests (mprotect integration)
//=============================================================================

TEST_F(CombinatorialHarmTest, IsMprotectLocked_InitiallyFalse)
{
    EXPECT_FALSE(combinatorial_is_mprotect_locked(detector));
}

TEST_F(CombinatorialHarmTest, LockPatternsMprotect_WithLockedPatterns_Succeeds)
{
    // Register default patterns (all locked)
    combinatorial_register_default_patterns(detector);

    // Lock with mprotect
    bool success = combinatorial_lock_patterns_mprotect(detector);
    EXPECT_TRUE(success);

    // Verify locked
    EXPECT_TRUE(combinatorial_is_mprotect_locked(detector));
}

TEST_F(CombinatorialHarmTest, LockPatternsMprotect_Twice_ReturnsFalse)
{
    combinatorial_register_default_patterns(detector);

    // First lock succeeds
    bool first = combinatorial_lock_patterns_mprotect(detector);
    EXPECT_TRUE(first);

    // Second lock fails (already locked)
    bool second = combinatorial_lock_patterns_mprotect(detector);
    EXPECT_FALSE(second);
}

TEST_F(CombinatorialHarmTest, VerifyPatternIntegrity_AfterLock_ReturnsTrue)
{
    combinatorial_register_default_patterns(detector);
    combinatorial_lock_patterns_mprotect(detector);

    // Verify integrity
    bool intact = combinatorial_verify_pattern_integrity(detector);
    EXPECT_TRUE(intact);
}

TEST_F(CombinatorialHarmTest, GetDirectiveSystem_AfterLock_ReturnsNonNull)
{
    combinatorial_register_default_patterns(detector);
    combinatorial_lock_patterns_mprotect(detector);

    const nimcp_directive_system_t* system = combinatorial_get_directive_system(detector);
    EXPECT_NE(system, nullptr);
}

TEST_F(CombinatorialHarmTest, GetDirectiveSystem_BeforeLock_ReturnsNull)
{
    combinatorial_register_default_patterns(detector);

    // Don't lock
    const nimcp_directive_system_t* system = combinatorial_get_directive_system(detector);
    EXPECT_EQ(system, nullptr);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CombinatorialHarmTest, GetStats_InitiallyZero)
{
    combinatorial_stats_t stats;
    bool success = combinatorial_get_stats(detector, &stats);

    EXPECT_TRUE(success);
    EXPECT_EQ(stats.total_evaluations, 0ull);
    EXPECT_EQ(stats.combinations_detected, 0ull);
    EXPECT_EQ(stats.actions_blocked, 0ull);
}

TEST_F(CombinatorialHarmTest, GetStats_TracksEvaluations)
{
    combinatorial_register_default_patterns(detector);

    // Record action
    action_record_t record = create_test_action(ACTION_CATEGORY_DATA_EXPORT);
    combinatorial_record_action(detector, &record);

    // Evaluate
    action_record_t pending = create_test_action(ACTION_CATEGORY_DATA_EXPORT);
    combinatorial_evaluation_t result;
    combinatorial_evaluate(detector, &pending, &result);

    // Check stats
    combinatorial_stats_t stats;
    combinatorial_get_stats(detector, &stats);

    EXPECT_EQ(stats.total_evaluations, 1ull);
}

TEST_F(CombinatorialHarmTest, ResetStats_ClearsCounters)
{
    combinatorial_register_default_patterns(detector);

    // Do some evaluations
    action_record_t record = create_test_action(ACTION_CATEGORY_DATA_EXPORT);
    combinatorial_record_action(detector, &record);

    action_record_t pending = create_test_action(ACTION_CATEGORY_DATA_EXPORT);
    combinatorial_evaluation_t result;
    combinatorial_evaluate(detector, &pending, &result);

    // Reset stats
    combinatorial_reset_stats(detector);

    // Check stats are cleared
    combinatorial_stats_t stats;
    combinatorial_get_stats(detector, &stats);

    EXPECT_EQ(stats.total_evaluations, 0ull);
}

//=============================================================================
// Mathematical Enhancement Tests
//=============================================================================

TEST_F(CombinatorialHarmTest, ComputeShannonMetrics_Succeeds)
{
    action_record_t action_a = create_test_action(ACTION_CATEGORY_INFORMATION_DISCLOSURE, 0.3f);
    action_record_t action_b = create_test_action(ACTION_CATEGORY_ACCESS_GRANT, 0.4f);

    shannon_harm_metrics_t metrics;
    bool success = combinatorial_compute_shannon_metrics(detector, &action_a, &action_b, &metrics);

    EXPECT_TRUE(success);
    EXPECT_GE(metrics.action_entropy, 0.0f);
    EXPECT_GE(metrics.joint_entropy, 0.0f);
    EXPECT_GE(metrics.normalized_harm_score, 0.0f);
    EXPECT_LE(metrics.normalized_harm_score, 1.0f);
}

TEST_F(CombinatorialHarmTest, FractalAnalysis_Succeeds)
{
    // Record some history for fractal analysis
    for (int i = 0; i < 10; i++) {
        action_record_t record = create_test_action(
            ACTION_CATEGORY_DATA_EXPORT,
            0.1f * i
        );
        combinatorial_record_action(detector, &record);
    }

    action_record_t pending = create_test_action(ACTION_CATEGORY_DATA_EXPORT, 0.5f);

    fractal_harm_analysis_t analysis;
    bool success = combinatorial_fractal_analysis(detector, &pending, &analysis);

    EXPECT_TRUE(success);
    EXPECT_GE(analysis.hurst_exponent, 0.0f);
    EXPECT_LE(analysis.hurst_exponent, 1.0f);
}

TEST_F(CombinatorialHarmTest, HyperbolicEmbed_ProducesValidCoords)
{
    action_record_t action = create_test_action(ACTION_CATEGORY_PHYSICAL_ACTION, 0.5f);

    hyperbolic_harm_embedding_t embedding;
    bool success = combinatorial_hyperbolic_embed(detector, &action, &embedding);

    EXPECT_TRUE(success);

    // Check Poincare disk constraint: ||(x,y)|| < 1
    float norm_sq = embedding.poincare_coords[0] * embedding.poincare_coords[0] +
                    embedding.poincare_coords[1] * embedding.poincare_coords[1];
    EXPECT_LT(norm_sq, 1.0f);
}

TEST_F(CombinatorialHarmTest, ComputePhasor_ProducesValidPhase)
{
    action_record_t action_a = create_test_action(ACTION_CATEGORY_PHYSICAL_ACTION, 0.3f);
    action_record_t action_b = create_test_action(ACTION_CATEGORY_PHYSICAL_ACTION, 0.4f);

    complex_harm_phasor_t phasor;
    bool success = combinatorial_compute_phasor(detector, &action_a, &action_b, &phasor);

    EXPECT_TRUE(success);
    // Phase should be in [-pi, pi]
    EXPECT_GE(phasor.phase, -M_PI);
    EXPECT_LE(phasor.phase, M_PI);
}

TEST_F(CombinatorialHarmTest, QuantumSearch_Converges)
{
    // Record some history
    for (int i = 0; i < 5; i++) {
        action_record_t record = create_test_action(
            ACTION_CATEGORY_RESOURCE_ALLOCATION,
            0.2f
        );
        combinatorial_record_action(detector, &record);
    }

    action_record_t pending = create_test_action(ACTION_CATEGORY_RESOURCE_ALLOCATION, 0.5f);

    quantum_harm_search_t search;
    bool success = combinatorial_quantum_search(detector, &pending, 100, &search);

    EXPECT_TRUE(success);
    // Quantum walk should have run
    EXPECT_GT(search.walk_steps, 0u);
}

TEST_F(CombinatorialHarmTest, PinkNoiseAnalysis_DetectsSpectralSlope)
{
    // Record enough history for spectral analysis
    for (int i = 0; i < 20; i++) {
        action_record_t record = create_test_action(
            ACTION_CATEGORY_COMMUNICATION,
            0.3f + 0.02f * (i % 5)  // Some variation
        );
        combinatorial_record_action(detector, &record);
    }

    action_record_t pending = create_test_action(ACTION_CATEGORY_COMMUNICATION, 0.4f);

    pink_noise_harm_analysis_t analysis;
    bool success = combinatorial_pink_noise_analysis(detector, &pending, &analysis);

    EXPECT_TRUE(success);
    // Spectral slope should be computed (negative for 1/f noise)
    EXPECT_NE(analysis.spectral_slope, 0.0f);
}

TEST_F(CombinatorialHarmTest, FullMathematicalAnalysis_CombinesAllMethods)
{
    combinatorial_register_default_patterns(detector);

    // Record history
    for (int i = 0; i < 10; i++) {
        action_record_t record = create_test_action(
            ACTION_CATEGORY_FINANCIAL_TRANSACTION,
            0.2f + 0.05f * i
        );
        combinatorial_record_action(detector, &record);
    }

    action_record_t pending = create_test_action(ACTION_CATEGORY_FINANCIAL_TRANSACTION, 0.5f);

    mathematical_harm_analysis_t analysis;
    bool success = combinatorial_full_mathematical_analysis(detector, &pending, &analysis);

    EXPECT_TRUE(success);
    // All methods should be used
    EXPECT_EQ(analysis.methods_used, MATH_METHOD_ALL);
    // Unified score should be non-negative (can exceed 1.0 in Bayesian fusion)
    EXPECT_GE(analysis.unified_harm_score, 0.0f);
    // Confidence should be valid
    EXPECT_GE(analysis.confidence, 0.0f);
    EXPECT_LE(analysis.confidence, 1.0f);
}

TEST_F(CombinatorialHarmTest, EvaluateEnhanced_UsesAllMethods)
{
    combinatorial_register_default_patterns(detector);

    // Record some history
    action_record_t history_action = create_test_action(
        ACTION_CATEGORY_PHYSICAL_ACTION,
        0.4f
    );
    combinatorial_record_action(detector, &history_action);

    action_record_t pending = create_test_action(ACTION_CATEGORY_PHYSICAL_ACTION, 0.4f);

    combinatorial_evaluation_t result;
    mathematical_harm_analysis_t math_analysis;

    bool success = combinatorial_evaluate_enhanced(detector, &pending, &result, &math_analysis);

    EXPECT_TRUE(success);
    // Physical+Physical pattern has 4.0x multiplier - should detect harm
    EXPECT_TRUE(result.harmful);
    EXPECT_GT(result.combined_harm_score, 0.7f);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(CombinatorialHarmTest, RingBufferWraparound_HandlesCorrectly)
{
    // Fill history beyond capacity (100)
    for (uint32_t i = 0; i < 150; i++) {
        action_record_t record = create_test_action(
            ACTION_CATEGORY_DATA_EXPORT,
            0.1f
        );
        combinatorial_record_action(detector, &record);
    }

    // Should still work
    combinatorial_stats_t stats;
    combinatorial_get_stats(detector, &stats);

    // History should be at capacity (100)
    EXPECT_EQ(stats.actions_in_history, 100u);
}

TEST_F(CombinatorialHarmTest, HyperbolicDistance_IsSymmetric)
{
    hyperbolic_harm_embedding_t embed_a = {
        .poincare_coords = {0.3f, 0.2f},
        .hyperbolic_distance = 0.36f,
        .angular_position = 0.5f,
        .hierarchy_depth = 1.0f
    };

    hyperbolic_harm_embedding_t embed_b = {
        .poincare_coords = {0.5f, 0.1f},
        .hyperbolic_distance = 0.51f,
        .angular_position = 0.8f,
        .hierarchy_depth = 2.0f
    };

    float dist_ab = combinatorial_hyperbolic_distance(&embed_a, &embed_b);
    float dist_ba = combinatorial_hyperbolic_distance(&embed_b, &embed_a);

    // Distance should be symmetric
    EXPECT_NEAR(dist_ab, dist_ba, 0.0001f);
}

} // namespace
