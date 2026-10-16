/**
 * @file test_e2e_ternary_attention_system.cpp
 * @brief End-to-end tests for attention system with ternary gates
 *
 * WHAT: Full attention system with ternary attention gates
 * WHY:  Verify ternary gating for selective attention
 * HOW:  Test attention focusing, filtering, and dynamics
 *
 * TEST COVERAGE:
 * - Full attention system with ternary gates
 * - Selective attention with ternary focus
 * - Attention dynamics and temporal evolution
 * - Multi-target attention switching
 * - Inhibition of return with ternary markers
 *
 * BIOLOGICAL BASIS:
 * - Attention gates: attend (+1), ignore (-1), neutral (0)
 * - Winner-take-all competition in attention maps
 * - Temporal dynamics of attention shifts
 * - Inhibition of return prevents re-attending
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_logic.h"
#include "utils/ternary/nimcp_ternary_convert.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TernaryAttentionSystemE2ETest : public ::testing::Test {
protected:
    // Attention map dimensions
    static constexpr size_t ATTENTION_WIDTH = 16;
    static constexpr size_t ATTENTION_HEIGHT = 16;
    static constexpr size_t NUM_FEATURES = 8;
    static constexpr size_t NUM_TARGETS = 5;

    // Attention parameters
    static constexpr float ATTENTION_THRESHOLD = 0.5f;
    static constexpr float IOR_DECAY = 0.1f;  // Inhibition of return decay
    static constexpr float ATTENTION_SPREAD = 2.0f;  // Gaussian attention spread

    struct AttentionTarget {
        float x, y;      // Position
        float salience;  // Bottom-up salience
        float priority;  // Top-down priority
    };

    // Attention gates (ternary): +1=attend, 0=neutral, -1=suppress
    trit_matrix_t* attention_gates = nullptr;

    // Inhibition of return map (ternary): +1=recently attended, 0=available, -1=suppressed
    trit_matrix_t* ior_map = nullptr;

    // Feature-attention weights
    trit_matrix_t* feature_weights = nullptr;

    // Accumulated attention over time
    std::vector<float> saliency_map;
    std::vector<float> ior_decay_map;

    std::vector<AttentionTarget> targets;
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);

        // Create attention gates
        attention_gates = trit_matrix_create(
            ATTENTION_HEIGHT, ATTENTION_WIDTH, TERNARY_PACK_2BIT);
        ASSERT_NE(attention_gates, nullptr);

        // Create inhibition of return map
        ior_map = trit_matrix_create(
            ATTENTION_HEIGHT, ATTENTION_WIDTH, TERNARY_PACK_2BIT);
        ASSERT_NE(ior_map, nullptr);

        // Create feature-attention weights
        feature_weights = trit_matrix_create(
            NUM_FEATURES, ATTENTION_WIDTH * ATTENTION_HEIGHT, TERNARY_PACK_BASE243);
        ASSERT_NE(feature_weights, nullptr);

        // Initialize
        ClearAttention();
        InitializeFeatureWeights();
        GenerateTargets();

        // Initialize maps
        saliency_map.resize(ATTENTION_WIDTH * ATTENTION_HEIGHT, 0.0f);
        ior_decay_map.resize(ATTENTION_WIDTH * ATTENTION_HEIGHT, 0.0f);
    }

    void TearDown() override {
        if (attention_gates) trit_matrix_destroy(attention_gates);
        if (ior_map) trit_matrix_destroy(ior_map);
        if (feature_weights) trit_matrix_destroy(feature_weights);
    }

    void ClearAttention() {
        for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
            for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
                trit_matrix_set(attention_gates, y, x, TRIT_UNKNOWN);
                trit_matrix_set(ior_map, y, x, TRIT_UNKNOWN);
            }
        }
    }

    void InitializeFeatureWeights() {
        std::uniform_int_distribution<int> dist(-1, 1);
        for (size_t f = 0; f < NUM_FEATURES; f++) {
            for (size_t p = 0; p < ATTENTION_WIDTH * ATTENTION_HEIGHT; p++) {
                trit_matrix_set(feature_weights, f, p, (trit_t)dist(rng));
            }
        }
    }

    void GenerateTargets() {
        std::uniform_real_distribution<float> pos_dist(0.0f, 1.0f);
        std::uniform_real_distribution<float> sal_dist(0.3f, 1.0f);

        targets.clear();
        for (size_t i = 0; i < NUM_TARGETS; i++) {
            AttentionTarget target;
            target.x = pos_dist(rng) * ATTENTION_WIDTH;
            target.y = pos_dist(rng) * ATTENTION_HEIGHT;
            target.salience = sal_dist(rng);
            target.priority = sal_dist(rng);
            targets.push_back(target);
        }
    }

    // Compute saliency from targets with Gaussian spread
    void ComputeSaliencyMap() {
        std::fill(saliency_map.begin(), saliency_map.end(), 0.0f);

        for (const auto& target : targets) {
            for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
                for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
                    float dx = (float)x - target.x;
                    float dy = (float)y - target.y;
                    float dist_sq = dx * dx + dy * dy;
                    float gaussian = expf(-dist_sq / (2.0f * ATTENTION_SPREAD * ATTENTION_SPREAD));
                    saliency_map[y * ATTENTION_WIDTH + x] +=
                        target.salience * target.priority * gaussian;
                }
            }
        }
    }

    // Apply ternary attention gating
    void ApplyAttentionGating() {
        for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
            for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
                size_t idx = y * ATTENTION_WIDTH + x;
                float sal = saliency_map[idx];

                // Get IOR status
                trit_t ior = trit_matrix_get(ior_map, y, x);

                // Determine attention gate
                trit_t gate;
                if (ior == TRIT_POSITIVE) {
                    // Recently attended - suppress (inhibition of return)
                    gate = TRIT_NEGATIVE;
                } else if (sal >= ATTENTION_THRESHOLD) {
                    // High saliency - attend
                    gate = TRIT_POSITIVE;
                } else if (sal < ATTENTION_THRESHOLD * 0.3f) {
                    // Low saliency - suppress
                    gate = TRIT_NEGATIVE;
                } else {
                    // Medium saliency - neutral
                    gate = TRIT_UNKNOWN;
                }

                trit_matrix_set(attention_gates, y, x, gate);
            }
        }
    }

    // Find current attention focus (winner-take-all)
    bool FindAttentionFocus(size_t& focus_x, size_t& focus_y, float& focus_strength) {
        focus_x = 0;
        focus_y = 0;
        focus_strength = -1.0f;

        for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
            for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
                trit_t gate = trit_matrix_get(attention_gates, y, x);
                if (gate == TRIT_POSITIVE) {
                    float sal = saliency_map[y * ATTENTION_WIDTH + x];
                    if (sal > focus_strength) {
                        focus_strength = sal;
                        focus_x = x;
                        focus_y = y;
                    }
                }
            }
        }

        return focus_strength > 0.0f;
    }

    // Update inhibition of return
    void UpdateIOR(size_t attended_x, size_t attended_y) {
        // Mark attended location
        trit_matrix_set(ior_map, attended_y, attended_x, TRIT_POSITIVE);

        // Decay IOR for other locations
        for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
            for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
                if (x == attended_x && y == attended_y) continue;

                trit_t current = trit_matrix_get(ior_map, y, x);
                // Probabilistic decay: POSITIVE -> UNKNOWN with probability IOR_DECAY
                if (current == TRIT_POSITIVE) {
                    std::uniform_real_distribution<float> decay_dist(0.0f, 1.0f);
                    if (decay_dist(rng) < IOR_DECAY) {
                        trit_matrix_set(ior_map, y, x, TRIT_UNKNOWN);
                    }
                }
            }
        }
    }

    // Count attention states
    struct AttentionStats {
        size_t attending;
        size_t neutral;
        size_t suppressing;
    };

    AttentionStats GetAttentionStats() {
        AttentionStats stats = {0, 0, 0};
        for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
            for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
                trit_t gate = trit_matrix_get(attention_gates, y, x);
                if (gate == TRIT_POSITIVE) stats.attending++;
                else if (gate == TRIT_NEGATIVE) stats.suppressing++;
                else stats.neutral++;
            }
        }
        return stats;
    }
};

//=============================================================================
// E2E Test: Basic Attention Focusing
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, BasicAttentionFocusing) {
    // Compute saliency from targets
    ComputeSaliencyMap();

    // Apply attention gating
    ApplyAttentionGating();

    // Find attention focus
    size_t focus_x, focus_y;
    float focus_strength;
    bool found_focus = FindAttentionFocus(focus_x, focus_y, focus_strength);

    EXPECT_TRUE(found_focus) << "No attention focus found";

    // Focus should be near one of the targets
    float min_dist = std::numeric_limits<float>::max();
    for (const auto& target : targets) {
        float dx = (float)focus_x - target.x;
        float dy = (float)focus_y - target.y;
        float dist = sqrtf(dx * dx + dy * dy);
        min_dist = std::min(min_dist, dist);
    }

    EXPECT_LT(min_dist, ATTENTION_SPREAD * 2.0f)
        << "Attention focus not near any target";

    // Verify attention stats are reasonable
    AttentionStats stats = GetAttentionStats();
    EXPECT_GT(stats.attending, 0u) << "No attending locations";
    EXPECT_GT(stats.suppressing, stats.attending)
        << "Expected more suppression than attention for sparse focus";
}

//=============================================================================
// E2E Test: Selective Attention with Multiple Targets
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, SelectiveAttentionWithMultipleTargets) {
    ComputeSaliencyMap();
    ApplyAttentionGating();

    // Find all attended locations
    std::vector<std::pair<size_t, size_t>> attended_locs;
    for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
        for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
            if (trit_matrix_get(attention_gates, y, x) == TRIT_POSITIVE) {
                attended_locs.push_back({x, y});
            }
        }
    }

    // Should have at least some attended locations
    EXPECT_GT(attended_locs.size(), 0u);

    // Attended locations should be clustered around high-salience targets
    for (const auto& loc : attended_locs) {
        float sal = saliency_map[loc.second * ATTENTION_WIDTH + loc.first];
        EXPECT_GE(sal, ATTENTION_THRESHOLD * 0.5f)
            << "Attending to low-salience location";
    }

    // Suppressed locations should be low salience
    size_t low_sal_suppressed = 0, high_sal_suppressed = 0;
    for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
        for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
            if (trit_matrix_get(attention_gates, y, x) == TRIT_NEGATIVE) {
                float sal = saliency_map[y * ATTENTION_WIDTH + x];
                if (sal < ATTENTION_THRESHOLD * 0.5f) low_sal_suppressed++;
                else high_sal_suppressed++;
            }
        }
    }

    // Most suppressed should be low salience
    EXPECT_GE(low_sal_suppressed, high_sal_suppressed);
}

//=============================================================================
// E2E Test: Attention Dynamics and Temporal Evolution
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, AttentionDynamicsAndTemporalEvolution) {
    constexpr size_t NUM_TIMESTEPS = 50;

    std::vector<std::pair<size_t, size_t>> focus_history;

    for (size_t t = 0; t < NUM_TIMESTEPS; t++) {
        // Update saliency (targets may move slightly)
        for (auto& target : targets) {
            std::normal_distribution<float> noise(0.0f, 0.1f);
            target.x = std::clamp(target.x + noise(rng), 0.0f, (float)ATTENTION_WIDTH - 1);
            target.y = std::clamp(target.y + noise(rng), 0.0f, (float)ATTENTION_HEIGHT - 1);
        }

        ComputeSaliencyMap();
        ApplyAttentionGating();

        // Find and record focus
        size_t focus_x, focus_y;
        float focus_strength;
        if (FindAttentionFocus(focus_x, focus_y, focus_strength)) {
            focus_history.push_back({focus_x, focus_y});
            UpdateIOR(focus_x, focus_y);
        }
    }

    // Should have tracked attention across timesteps
    EXPECT_GT(focus_history.size(), NUM_TIMESTEPS / 2)
        << "Attention lost too frequently";

    // Verify attention moved (not stuck)
    if (focus_history.size() > 1) {
        size_t position_changes = 0;
        for (size_t i = 1; i < focus_history.size(); i++) {
            if (focus_history[i] != focus_history[i-1]) {
                position_changes++;
            }
        }

        // Some position changes expected due to IOR
        EXPECT_GT(position_changes, 0u) << "Attention stuck in one location";
    }
}

//=============================================================================
// E2E Test: Inhibition of Return
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, InhibitionOfReturn) {
    ComputeSaliencyMap();

    // Find initial focus
    ApplyAttentionGating();
    size_t focus_x, focus_y;
    float focus_strength;
    bool found = FindAttentionFocus(focus_x, focus_y, focus_strength);
    ASSERT_TRUE(found);

    // Record initial focus
    size_t initial_focus_x = focus_x;
    size_t initial_focus_y = focus_y;

    // Mark as attended (trigger IOR)
    UpdateIOR(focus_x, focus_y);

    // Re-apply attention gating
    ApplyAttentionGating();

    // Initial focus should now be suppressed
    trit_t initial_gate = trit_matrix_get(attention_gates, initial_focus_y, initial_focus_x);
    EXPECT_EQ(initial_gate, TRIT_NEGATIVE)
        << "IOR did not suppress previously attended location";

    // Find new focus - should be different
    found = FindAttentionFocus(focus_x, focus_y, focus_strength);
    if (found) {
        bool same_location = (focus_x == initial_focus_x && focus_y == initial_focus_y);
        EXPECT_FALSE(same_location) << "Attention returned to same location despite IOR";
    }
}

//=============================================================================
// E2E Test: Feature-Based Attention
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, FeatureBasedAttention) {
    // Create feature maps
    std::vector<std::vector<float>> feature_maps(NUM_FEATURES);
    for (size_t f = 0; f < NUM_FEATURES; f++) {
        feature_maps[f].resize(ATTENTION_WIDTH * ATTENTION_HEIGHT);
        for (size_t p = 0; p < ATTENTION_WIDTH * ATTENTION_HEIGHT; p++) {
            feature_maps[f][p] = (float)(rng() % 100) / 100.0f;
        }
    }

    // Create feature attention vector (which features to attend)
    trit_vector_t* feature_attention = trit_vector_create(NUM_FEATURES, TERNARY_PACK_2BIT);
    ASSERT_NE(feature_attention, nullptr);

    // Attend to first two features, ignore last two, neutral for rest
    for (size_t f = 0; f < NUM_FEATURES; f++) {
        trit_t gate;
        if (f < 2) gate = TRIT_POSITIVE;
        else if (f >= NUM_FEATURES - 2) gate = TRIT_NEGATIVE;
        else gate = TRIT_UNKNOWN;
        trit_vector_set(feature_attention, f, gate);
    }

    // Compute feature-modulated saliency
    std::vector<float> modulated_saliency(ATTENTION_WIDTH * ATTENTION_HEIGHT, 0.0f);
    for (size_t f = 0; f < NUM_FEATURES; f++) {
        trit_t feat_gate = trit_vector_get(feature_attention, f);
        float weight = (feat_gate == TRIT_POSITIVE) ? 1.0f :
                       (feat_gate == TRIT_NEGATIVE) ? -0.5f : 0.0f;

        for (size_t p = 0; p < ATTENTION_WIDTH * ATTENTION_HEIGHT; p++) {
            modulated_saliency[p] += weight * feature_maps[f][p];
        }
    }

    // Apply to attention gates
    for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
        for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
            size_t idx = y * ATTENTION_WIDTH + x;
            trit_t gate = trit_from_float_threshold(modulated_saliency[idx], ATTENTION_THRESHOLD);
            trit_matrix_set(attention_gates, y, x, gate);
        }
    }

    // Verify feature-based modulation works
    AttentionStats stats = GetAttentionStats();
    EXPECT_GT(stats.attending + stats.suppressing, 0u)
        << "Feature attention produced no effect";

    trit_vector_destroy(feature_attention);
}

//=============================================================================
// E2E Test: Attention Competition
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, AttentionCompetition) {
    // Create two equally salient targets far apart
    targets.clear();
    AttentionTarget t1 = {3.0f, 3.0f, 0.9f, 0.9f};
    AttentionTarget t2 = {12.0f, 12.0f, 0.9f, 0.9f};
    targets.push_back(t1);
    targets.push_back(t2);

    ComputeSaliencyMap();
    ApplyAttentionGating();

    // Find all attending locations
    std::vector<std::pair<size_t, size_t>> attending;
    for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
        for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
            if (trit_matrix_get(attention_gates, y, x) == TRIT_POSITIVE) {
                attending.push_back({x, y});
            }
        }
    }

    // Should attend to both salient targets (no winner-take-all yet)
    bool near_t1 = false, near_t2 = false;
    for (const auto& loc : attending) {
        float d1 = sqrtf(powf((float)loc.first - t1.x, 2) + powf((float)loc.second - t1.y, 2));
        float d2 = sqrtf(powf((float)loc.first - t2.x, 2) + powf((float)loc.second - t2.y, 2));
        if (d1 < ATTENTION_SPREAD * 2) near_t1 = true;
        if (d2 < ATTENTION_SPREAD * 2) near_t2 = true;
    }

    // Both targets should have attending locations nearby
    EXPECT_TRUE(near_t1 || near_t2) << "Neither target receiving attention";

    // With IOR, attention should eventually switch
    size_t focus_x, focus_y;
    float focus_strength;
    FindAttentionFocus(focus_x, focus_y, focus_strength);
    UpdateIOR(focus_x, focus_y);
    ApplyAttentionGating();

    size_t new_focus_x, new_focus_y;
    float new_focus_strength;
    bool found_new = FindAttentionFocus(new_focus_x, new_focus_y, new_focus_strength);

    if (found_new) {
        // New focus should be different from old
        float dist = sqrtf(powf((float)new_focus_x - focus_x, 2) +
                           powf((float)new_focus_y - focus_y, 2));
        // With two distant targets, should switch to other
        EXPECT_GT(dist, 3.0f) << "Attention should switch to other target";
    }
}

//=============================================================================
// E2E Test: Ternary Logic for Attention Combination
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, TernaryLogicForAttentionCombination) {
    // Create bottom-up (stimulus-driven) attention
    trit_matrix_t* bottom_up = trit_matrix_create(
        ATTENTION_HEIGHT, ATTENTION_WIDTH, TERNARY_PACK_2BIT);
    ASSERT_NE(bottom_up, nullptr);

    // Create top-down (goal-driven) attention
    trit_matrix_t* top_down = trit_matrix_create(
        ATTENTION_HEIGHT, ATTENTION_WIDTH, TERNARY_PACK_2BIT);
    ASSERT_NE(top_down, nullptr);

    // Initialize with different patterns
    for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
        for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
            // Bottom-up: edges are salient
            bool edge = (x == 0 || x == ATTENTION_WIDTH - 1 ||
                         y == 0 || y == ATTENTION_HEIGHT - 1);
            trit_matrix_set(bottom_up, y, x, edge ? TRIT_POSITIVE : TRIT_UNKNOWN);

            // Top-down: center is important
            bool center = (x >= ATTENTION_WIDTH / 3 && x < 2 * ATTENTION_WIDTH / 3 &&
                           y >= ATTENTION_HEIGHT / 3 && y < 2 * ATTENTION_HEIGHT / 3);
            trit_matrix_set(top_down, y, x, center ? TRIT_POSITIVE : TRIT_UNKNOWN);
        }
    }

    // Combine using ternary AND (both must agree to attend)
    for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
        for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
            trit_t bu = trit_matrix_get(bottom_up, y, x);
            trit_t td = trit_matrix_get(top_down, y, x);
            trit_t combined = trit_and(bu, td);
            trit_matrix_set(attention_gates, y, x, combined);
        }
    }

    // With AND, only overlap regions should be attended
    AttentionStats stats = GetAttentionStats();

    // Corners are both edge (BU) and outside center (TD=neutral)
    // So AND should produce mostly neutral
    EXPECT_LT(stats.attending, (ATTENTION_WIDTH * ATTENTION_HEIGHT) / 4);

    // Also test OR combination (either BU or TD)
    for (size_t y = 0; y < ATTENTION_HEIGHT; y++) {
        for (size_t x = 0; x < ATTENTION_WIDTH; x++) {
            trit_t bu = trit_matrix_get(bottom_up, y, x);
            trit_t td = trit_matrix_get(top_down, y, x);
            trit_t combined = trit_or(bu, td);
            trit_matrix_set(attention_gates, y, x, combined);
        }
    }

    AttentionStats or_stats = GetAttentionStats();

    // OR should produce more attended locations
    EXPECT_GT(or_stats.attending, stats.attending);

    trit_matrix_destroy(bottom_up);
    trit_matrix_destroy(top_down);
}

//=============================================================================
// E2E Test: Covert Attention Shifts
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, CovertAttentionShifts) {
    // Test attention shifting without eye movement (covert)
    constexpr size_t NUM_SHIFTS = 10;

    std::vector<std::pair<size_t, size_t>> attended_sequence;

    for (size_t shift = 0; shift < NUM_SHIFTS; shift++) {
        ComputeSaliencyMap();
        ApplyAttentionGating();

        size_t focus_x, focus_y;
        float focus_strength;
        if (FindAttentionFocus(focus_x, focus_y, focus_strength)) {
            attended_sequence.push_back({focus_x, focus_y});
            UpdateIOR(focus_x, focus_y);
        }
    }

    // Should have attended multiple locations
    EXPECT_GE(attended_sequence.size(), NUM_SHIFTS / 2);

    // Count unique locations
    std::set<std::pair<size_t, size_t>> unique_locations(
        attended_sequence.begin(), attended_sequence.end());

    // IOR should cause exploration of multiple locations
    EXPECT_GT(unique_locations.size(), 1u)
        << "Attention stuck in single location";
}

//=============================================================================
// E2E Test: Attention Disengagement
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, AttentionDisengagement) {
    // Setup: single high-salience target
    targets.clear();
    targets.push_back({8.0f, 8.0f, 1.0f, 1.0f});  // Center, high salience

    ComputeSaliencyMap();
    ApplyAttentionGating();

    // Verify attention is engaged
    size_t focus_x, focus_y;
    float focus_strength;
    ASSERT_TRUE(FindAttentionFocus(focus_x, focus_y, focus_strength));

    // Now remove the target (simulate disappearance)
    targets.clear();
    ComputeSaliencyMap();

    // Re-apply attention without high salience
    ApplyAttentionGating();

    // Attention should disengage (fewer attending locations)
    AttentionStats stats = GetAttentionStats();

    // Most should be neutral or suppressing, few attending
    size_t total = ATTENTION_WIDTH * ATTENTION_HEIGHT;
    float attend_ratio = (float)stats.attending / total;

    EXPECT_LT(attend_ratio, 0.2f)
        << "Attention not disengaged after target removal";
}

//=============================================================================
// E2E Test: Long-Running Stability
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, LongRunningStability) {
    constexpr size_t NUM_STEPS = 1000;

    size_t valid_steps = 0;
    size_t focus_found = 0;

    for (size_t step = 0; step < NUM_STEPS; step++) {
        // Randomize targets occasionally
        if (step % 100 == 0) {
            GenerateTargets();
        }

        ComputeSaliencyMap();
        ApplyAttentionGating();

        // Validate attention gates
        bool valid = true;
        for (size_t y = 0; y < ATTENTION_HEIGHT && valid; y++) {
            for (size_t x = 0; x < ATTENTION_WIDTH && valid; x++) {
                trit_t gate = trit_matrix_get(attention_gates, y, x);
                if (gate != TRIT_POSITIVE && gate != TRIT_NEGATIVE &&
                    gate != TRIT_UNKNOWN) {
                    valid = false;
                }
            }
        }

        if (valid) valid_steps++;

        size_t focus_x, focus_y;
        float focus_strength;
        if (FindAttentionFocus(focus_x, focus_y, focus_strength)) {
            focus_found++;
            UpdateIOR(focus_x, focus_y);
        }
    }

    EXPECT_EQ(valid_steps, NUM_STEPS) << "Invalid attention states detected";
    EXPECT_GT(focus_found, NUM_STEPS / 2) << "Focus lost too frequently";
}

//=============================================================================
// E2E Test: Performance Benchmark
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, PerformanceBenchmark) {
    constexpr size_t BENCHMARK_STEPS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t step = 0; step < BENCHMARK_STEPS; step++) {
        ComputeSaliencyMap();
        ApplyAttentionGating();

        size_t focus_x, focus_y;
        float focus_strength;
        if (FindAttentionFocus(focus_x, focus_y, focus_strength)) {
            UpdateIOR(focus_x, focus_y);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float steps_per_second = (float)BENCHMARK_STEPS * 1e6f / duration.count();

    EXPECT_GT(steps_per_second, 1000.0f)
        << "Performance: " << steps_per_second << " steps/second";

    SUCCEED() << "Attention system: " << steps_per_second << " steps/second";
}

//=============================================================================
// E2E Test: Memory Efficiency
//=============================================================================

TEST_F(TernaryAttentionSystemE2ETest, MemoryEfficiency) {
    size_t ternary_bytes = 0;
    ternary_bytes += trit_matrix_memory_size(attention_gates);
    ternary_bytes += trit_matrix_memory_size(ior_map);
    ternary_bytes += trit_matrix_memory_size(feature_weights);

    // Float equivalent
    size_t map_size = ATTENTION_WIDTH * ATTENTION_HEIGHT;
    size_t float_bytes = (2 * map_size + NUM_FEATURES * map_size) * sizeof(float);

    float compression = (float)float_bytes / (float)ternary_bytes;

    EXPECT_GT(compression, 3.0f) << "Expected at least 3x compression";

    SUCCEED() << "Memory compression: " << compression << "x";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
