/**
 * @file test_combinatorial_harm_regression.cpp
 * @brief Regression tests for NIMCP Combinatorial Harm Detection Module
 *
 * Tests specific behaviors that must remain stable:
 * - Memory lock cannot be bypassed
 * - Default patterns always present
 * - Harm multipliers remain at expected values
 * - API contracts preserved across versions
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

class CombinatorialHarmRegressionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        combinatorial_config_t config = combinatorial_default_config();
        config.history_capacity = 100;
        config.time_window_ms = 60000;
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

    action_record_t create_action(action_category_t category, float harm) {
        action_record_t record;
        memset(&record, 0, sizeof(record));
        record.category = category;
        record.acting_agent = 1;
        record.target_agent = 2;
        record.standalone_harm_score = harm;
        return record;
    }

    combinatorial_harm_detector_t detector = nullptr;
};

//=============================================================================
// Memory Lock Regression Tests
// CRITICAL: These patterns must NEVER be removable
//=============================================================================

TEST_F(CombinatorialHarmRegressionTest, DefaultPatterns_All8MustBeRegistered)
{
    // REGRESSION: Ensure all 8 default patterns are registered
    uint32_t registered = combinatorial_register_default_patterns(detector);
    EXPECT_EQ(registered, 8u) << "REGRESSION: Default patterns count changed from 8";
}

TEST_F(CombinatorialHarmRegressionTest, LockedPatterns_CannotBeUnregisteredByIndex)
{
    combinatorial_register_default_patterns(detector);

    // REGRESSION: Try to unregister each locked pattern
    for (uint32_t id = 1; id <= 8; id++) {
        bool removed = combinatorial_unregister_pattern(detector, id);
        EXPECT_FALSE(removed) << "REGRESSION: Locked pattern " << id << " was removable";
    }
}

TEST_F(CombinatorialHarmRegressionTest, LockedPatterns_RemainAfterMultipleRemovalAttempts)
{
    combinatorial_register_default_patterns(detector);

    // REGRESSION: Attack pattern - repeatedly try to remove
    for (int attempt = 0; attempt < 1000; attempt++) {
        for (uint32_t id = 1; id <= 8; id++) {
            combinatorial_unregister_pattern(detector, id);
        }
    }

    // All 8 patterns must still exist
    combinatorial_stats_t stats;
    combinatorial_get_stats(detector, &stats);
    EXPECT_EQ(stats.patterns_registered, 8u) << "REGRESSION: Patterns lost after removal attempts";
}

TEST_F(CombinatorialHarmRegressionTest, MprotectLock_CannotBeLiftedAfterSet)
{
    combinatorial_register_default_patterns(detector);
    combinatorial_lock_patterns_mprotect(detector);

    // REGRESSION: Second lock attempt must fail
    bool second_lock = combinatorial_lock_patterns_mprotect(detector);
    EXPECT_FALSE(second_lock) << "REGRESSION: Second mprotect lock succeeded";

    // Must remain locked
    EXPECT_TRUE(combinatorial_is_mprotect_locked(detector))
        << "REGRESSION: Lock state changed after second attempt";
}

//=============================================================================
// Harm Multiplier Regression Tests
// CRITICAL: These multipliers are part of the safety contract
//=============================================================================

TEST_F(CombinatorialHarmRegressionTest, PhysicalHarm_Has4xMultiplier)
{
    combinatorial_register_default_patterns(detector);

    // REGRESSION: Physical harm must have 4.0x multiplier (highest)
    action_record_t history = create_action(ACTION_CATEGORY_PHYSICAL_ACTION, 0.25f);
    combinatorial_record_action(detector, &history);

    action_record_t pending = create_action(ACTION_CATEGORY_PHYSICAL_ACTION, 0.25f);
    combinatorial_evaluation_t result;
    combinatorial_evaluate(detector, &pending, &result);

    // 0.25 * 4.0 = 1.0 (clamped)
    EXPECT_GE(result.combined_harm_score, 0.9f)
        << "REGRESSION: Physical harm multiplier reduced from 4.0x";
}

TEST_F(CombinatorialHarmRegressionTest, AccessEscalation_Has25xMultiplier)
{
    combinatorial_register_default_patterns(detector);

    // REGRESSION: Access escalation must have 2.5x multiplier
    action_record_t history = create_action(ACTION_CATEGORY_ACCESS_GRANT, 0.35f);
    combinatorial_record_action(detector, &history);

    action_record_t pending = create_action(ACTION_CATEGORY_ACCESS_GRANT, 0.35f);
    combinatorial_evaluation_t result;
    combinatorial_evaluate(detector, &pending, &result);

    // 0.35 * 2.5 = 0.875
    EXPECT_GE(result.combined_harm_score, 0.7f)
        << "REGRESSION: Access escalation multiplier changed from 2.5x";
}

TEST_F(CombinatorialHarmRegressionTest, InformationDisclosure_Has3xMultiplier)
{
    combinatorial_register_default_patterns(detector);

    // REGRESSION: Info disclosure must have 3.0x multiplier
    action_record_t history = create_action(ACTION_CATEGORY_INFORMATION_DISCLOSURE, 0.3f);
    combinatorial_record_action(detector, &history);

    action_record_t pending = create_action(ACTION_CATEGORY_INFORMATION_DISCLOSURE, 0.3f);
    combinatorial_evaluation_t result;
    combinatorial_evaluate(detector, &pending, &result);

    // 0.3 * 3.0 = 0.9
    EXPECT_GE(result.combined_harm_score, 0.7f)
        << "REGRESSION: Information disclosure multiplier changed from 3.0x";
}

//=============================================================================
// API Contract Regression Tests
// CRITICAL: API behavior must remain stable for consumers
//=============================================================================

TEST_F(CombinatorialHarmRegressionTest, DefaultConfig_ValuesUnchanged)
{
    combinatorial_config_t config = combinatorial_default_config();

    // REGRESSION: Default configuration values
    EXPECT_EQ(config.history_capacity, COMBINATORIAL_DEFAULT_HISTORY_CAPACITY)
        << "REGRESSION: Default history capacity changed";
    EXPECT_EQ(config.time_window_ms, COMBINATORIAL_DEFAULT_TIME_WINDOW_MS)
        << "REGRESSION: Default time window changed";
    EXPECT_NEAR(config.harm_threshold, COMBINATORIAL_DEFAULT_HARM_THRESHOLD, 0.01f)
        << "REGRESSION: Default harm threshold changed";
}

TEST_F(CombinatorialHarmRegressionTest, Evaluation_ReturnsStructuredResult)
{
    combinatorial_register_default_patterns(detector);

    action_record_t pending = create_action(ACTION_CATEGORY_DATA_EXPORT, 0.5f);
    combinatorial_evaluation_t result;
    bool success = combinatorial_evaluate(detector, &pending, &result);

    // REGRESSION: Result structure must be complete
    EXPECT_TRUE(success) << "REGRESSION: Evaluate returns false for valid input";
    // result.harmful is a valid bool
    EXPECT_TRUE(result.harmful == true || result.harmful == false);
    // Score is in [0, 1]
    EXPECT_GE(result.combined_harm_score, 0.0f);
    EXPECT_LE(result.combined_harm_score, 1.0f);
    // Confidence is in [0, 1]
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(CombinatorialHarmRegressionTest, RecordAction_ReturnsSequentialIds)
{
    // REGRESSION: Action IDs must be sequential starting from 1
    action_record_t action = create_action(ACTION_CATEGORY_DATA_EXPORT, 0.1f);

    uint64_t id1 = combinatorial_record_action(detector, &action);
    uint64_t id2 = combinatorial_record_action(detector, &action);
    uint64_t id3 = combinatorial_record_action(detector, &action);

    EXPECT_EQ(id1, 1ull) << "REGRESSION: First action ID not 1";
    EXPECT_EQ(id2, 2ull) << "REGRESSION: Second action ID not 2";
    EXPECT_EQ(id3, 3ull) << "REGRESSION: Third action ID not 3";
}

//=============================================================================
// Mathematical Enhancement Regression Tests
// CRITICAL: Mathematical results must be reproducible
//=============================================================================

TEST_F(CombinatorialHarmRegressionTest, Shannon_EntropyNonNegative)
{
    action_record_t action_a = create_action(ACTION_CATEGORY_INFORMATION_DISCLOSURE, 0.3f);
    action_record_t action_b = create_action(ACTION_CATEGORY_ACCESS_GRANT, 0.4f);

    shannon_harm_metrics_t metrics;
    combinatorial_compute_shannon_metrics(detector, &action_a, &action_b, &metrics);

    // REGRESSION: Entropy must be non-negative
    EXPECT_GE(metrics.action_entropy, 0.0f);
    EXPECT_GE(metrics.joint_entropy, 0.0f);
    EXPECT_GE(metrics.normalized_harm_score, 0.0f);
}

TEST_F(CombinatorialHarmRegressionTest, Hyperbolic_CoordsInPoincareDisk)
{
    action_record_t action = create_action(ACTION_CATEGORY_PHYSICAL_ACTION, 0.5f);

    hyperbolic_harm_embedding_t embedding;
    combinatorial_hyperbolic_embed(detector, &action, &embedding);

    // REGRESSION: Coordinates must be in Poincare disk ||(x,y)|| < 1
    float norm_sq = embedding.poincare_coords[0] * embedding.poincare_coords[0] +
                    embedding.poincare_coords[1] * embedding.poincare_coords[1];
    EXPECT_LT(norm_sq, 1.0f) << "REGRESSION: Hyperbolic coords outside Poincare disk";
}

TEST_F(CombinatorialHarmRegressionTest, Phasor_PhaseInValidRange)
{
    action_record_t action_a = create_action(ACTION_CATEGORY_PHYSICAL_ACTION, 0.3f);
    action_record_t action_b = create_action(ACTION_CATEGORY_PHYSICAL_ACTION, 0.4f);

    complex_harm_phasor_t phasor;
    combinatorial_compute_phasor(detector, &action_a, &action_b, &phasor);

    // REGRESSION: Phase must be in [-pi, pi]
    EXPECT_GE(phasor.phase, -M_PI) << "REGRESSION: Phase below -pi";
    EXPECT_LE(phasor.phase, M_PI) << "REGRESSION: Phase above pi";
}

TEST_F(CombinatorialHarmRegressionTest, Fractal_HurstExponentInValidRange)
{
    // Build history for fractal analysis
    for (int i = 0; i < 20; i++) {
        action_record_t record = create_action(
            ACTION_CATEGORY_FINANCIAL_TRANSACTION,
            0.1f + 0.02f * i
        );
        combinatorial_record_action(detector, &record);
    }

    action_record_t pending = create_action(ACTION_CATEGORY_FINANCIAL_TRANSACTION, 0.5f);
    fractal_harm_analysis_t analysis;
    combinatorial_fractal_analysis(detector, &pending, &analysis);

    // REGRESSION: Hurst exponent must be in [0, 1]
    EXPECT_GE(analysis.hurst_exponent, 0.0f) << "REGRESSION: Hurst < 0";
    EXPECT_LE(analysis.hurst_exponent, 1.0f) << "REGRESSION: Hurst > 1";
}

//=============================================================================
// Boundary Condition Regression Tests
// CRITICAL: Edge cases must behave consistently
//=============================================================================

TEST_F(CombinatorialHarmRegressionTest, EmptyHistory_DoesNotCrash)
{
    combinatorial_register_default_patterns(detector);

    action_record_t pending = create_action(ACTION_CATEGORY_DATA_EXPORT, 0.5f);
    combinatorial_evaluation_t result;

    // REGRESSION: Must not crash with empty history
    bool success = combinatorial_evaluate(detector, &pending, &result);
    EXPECT_TRUE(success);
    EXPECT_FALSE(result.harmful); // No history = no combination
}

TEST_F(CombinatorialHarmRegressionTest, MaxHistory_HandlesOverflow)
{
    combinatorial_register_default_patterns(detector);

    // Fill beyond capacity (100)
    for (int i = 0; i < 200; i++) {
        action_record_t record = create_action(ACTION_CATEGORY_DATA_EXPORT, 0.1f);
        uint64_t id = combinatorial_record_action(detector, &record);
        EXPECT_GT(id, 0ull) << "REGRESSION: Record failed at " << i;
    }

    // Must still evaluate
    action_record_t pending = create_action(ACTION_CATEGORY_DATA_EXPORT, 0.5f);
    combinatorial_evaluation_t result;
    bool success = combinatorial_evaluate(detector, &pending, &result);
    EXPECT_TRUE(success) << "REGRESSION: Evaluation failed after overflow";
}

TEST_F(CombinatorialHarmRegressionTest, ZeroHarmScore_HandledCorrectly)
{
    combinatorial_register_default_patterns(detector);

    action_record_t history = create_action(ACTION_CATEGORY_DATA_EXPORT, 0.0f);
    combinatorial_record_action(detector, &history);

    action_record_t pending = create_action(ACTION_CATEGORY_DATA_EXPORT, 0.0f);
    combinatorial_evaluation_t result;
    combinatorial_evaluate(detector, &pending, &result);

    // REGRESSION: Zero * multiplier = 0
    EXPECT_EQ(result.combined_harm_score, 0.0f);
    EXPECT_FALSE(result.harmful);
}

TEST_F(CombinatorialHarmRegressionTest, MaxHarmScore_ClampedTo1)
{
    combinatorial_register_default_patterns(detector);

    // Physical harm with max scores: 1.0 * 4.0 = 4.0 -> clamped to 1.0
    action_record_t history = create_action(ACTION_CATEGORY_PHYSICAL_ACTION, 1.0f);
    combinatorial_record_action(detector, &history);

    action_record_t pending = create_action(ACTION_CATEGORY_PHYSICAL_ACTION, 1.0f);
    combinatorial_evaluation_t result;
    combinatorial_evaluate(detector, &pending, &result);

    // REGRESSION: Must clamp to 1.0
    EXPECT_EQ(result.combined_harm_score, 1.0f);
    EXPECT_TRUE(result.harmful);
}

} // namespace
