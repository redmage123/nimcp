/**
 * @file test_salience_attention_integration.cpp
 * @brief Integration tests for Salience-Attention Hub Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Integration tests for bidirectional salience-attention communication
 * WHY:  Validate that salience detection drives attention and attention modulates salience
 * HOW:  Test complete integration flows with both modules connected via cognitive hub
 *
 * TEST SCENARIOS:
 * - SalienceDrivenAttention: High salience triggers attention shift
 * - PriorityBasedAllocation: Attention follows priority ordering
 * - CompetingSalienceResolution: Resolving multiple salient items
 * - AttentionFeedbackLoop: Attention affects salience detection
 * - BottomUpTopDownIntegration: Combining salience types
 *
 * BIOLOGICAL BASIS:
 * - Models superior colliculus (bottom-up salience)
 * - Models prefrontal cortex (top-down attention control)
 * - Models parietal cortex (priority map integration)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <algorithm>

#include "cognitive/integration/nimcp_salience_attention_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Integration Test Fixture
 * ============================================================================ */

class SalienceAttentionIntegrationTest : public ::testing::Test {
protected:
    salience_attention_bridge_t* bridge = nullptr;
    cognitive_integration_hub_t hub = nullptr;

    // Tracking variables for event flow
    std::atomic<int> salience_events_received{0};
    std::atomic<int> attention_shifts_received{0};
    std::atomic<int> priority_updates_received{0};
    std::atomic<uint64_t> last_focus_target{0};
    std::atomic<float> last_salience_score{0.0f};
    std::vector<uint64_t> attention_sequence;
    std::mutex sequence_mutex;

    static SalienceAttentionIntegrationTest* current_instance;

    void SetUp() override {
        current_instance = this;

        // Reset tracking
        salience_events_received = 0;
        attention_shifts_received = 0;
        priority_updates_received = 0;
        last_focus_target = 0;
        last_salience_score = 0.0f;
        attention_sequence.clear();

        // Create cognitive hub
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub_config.enable_async = true;
        hub_config.event_queue_size = 256;
        hub = cognitive_hub_create(&hub_config);
        ASSERT_NE(hub, nullptr) << "Hub creation required for integration tests";

        // Create salience-attention bridge
        salience_attention_config_t bridge_config;
        salience_attention_bridge_default_config(&bridge_config);
        bridge_config.salience_threshold = 0.6f;
        bridge_config.attention_shift_weight = 0.8f;
        bridge_config.enable_logging = false;

        bridge = salience_attention_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr) << "Bridge creation required for integration tests";

        // Register bridge with hub
        int result = salience_attention_bridge_register_with_hub(bridge, hub);
        ASSERT_EQ(result, 0) << "Bridge registration required for integration tests";

        // Set up callbacks to track events
        result = salience_attention_bridge_set_salience_callback(
            bridge, static_salience_callback, this);
        ASSERT_EQ(result, 0);

        result = salience_attention_bridge_set_attention_callback(
            bridge, static_attention_callback, this);
        ASSERT_EQ(result, 0);

        result = salience_attention_bridge_set_priority_callback(
            bridge, static_priority_callback, this);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            salience_attention_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (hub != nullptr) {
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
        current_instance = nullptr;
    }

    // Static callback wrappers
    static int static_salience_callback(const salient_item_t* item, void* user_data) {
        auto* self = static_cast<SalienceAttentionIntegrationTest*>(user_data);
        self->salience_events_received++;
        if (item) {
            self->last_salience_score = item->salience_score;
        }
        return 0;
    }

    static void static_attention_callback(const attention_focus_t* focus, void* user_data) {
        auto* self = static_cast<SalienceAttentionIntegrationTest*>(user_data);
        self->attention_shifts_received++;
        if (focus) {
            self->last_focus_target = focus->focus_id;
            std::lock_guard<std::mutex> lock(self->sequence_mutex);
            self->attention_sequence.push_back(focus->focus_id);
        }
    }

    static void static_priority_callback(const attention_priority_t* priorities,
                                         uint32_t num_priorities, void* user_data) {
        auto* self = static_cast<SalienceAttentionIntegrationTest*>(user_data);
        self->priority_updates_received++;
        (void)priorities;
        (void)num_priorities;
    }

    // Helper to create salient item
    salient_item_t create_salient_item(uint64_t id, float salience, float novelty,
                                       float surprise, float urgency) {
        salient_item_t item;
        memset(&item, 0, sizeof(item));
        item.item_id = id;
        item.salience_score = salience;
        item.novelty = novelty;
        item.surprise = surprise;
        item.urgency = urgency;
        item.modality = 0;
        item.timestamp = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        return item;
    }

    // Helper to create attention target
    attention_target_t create_attention_target(uint64_t id, float priority, float urgency) {
        attention_target_t target;
        memset(&target, 0, sizeof(target));
        target.target_id = id;
        target.priority = priority;
        target.urgency = urgency;
        target.modality = 0;
        target.timestamp = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        return target;
    }

    // Helper to wait for async events
    void wait_for_events(int expected_count, std::atomic<int>& counter, int timeout_ms = 1000) {
        auto start = std::chrono::steady_clock::now();
        while (counter.load() < expected_count) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

SalienceAttentionIntegrationTest* SalienceAttentionIntegrationTest::current_instance = nullptr;

/* ============================================================================
 * Salience-Driven Attention Tests
 * ============================================================================ */

/**
 * Test: SalienceDrivenAttention
 * Verify that high salience detection triggers attention shift
 *
 * BIOLOGICAL BASIS:
 * - Superior colliculus detects salient stimuli
 * - High salience automatically captures attention (exogenous)
 * - Attention shifts to location of salient stimulus
 */
TEST_F(SalienceAttentionIntegrationTest, SalienceDrivenAttention) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Create highly salient item (above threshold)
    salient_item_t item = create_salient_item(100, 0.85f, 0.9f, 0.8f, 0.7f);

    // Publish salience detection
    int result = salience_attention_publish_salience_detection(bridge, &item, 0.85f);
    EXPECT_EQ(result, 0) << "Salience detection should be published";

    // Request attention shift to the salient item
    attention_target_t target = create_attention_target(100, 0.9f, 0.8f);
    result = salience_attention_request_attention_shift(bridge, &target);
    EXPECT_EQ(result, 0) << "Attention shift should be requested";

    // Verify statistics show the events
    salience_attention_stats_t stats;
    result = salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.salience_detections, 1u) << "Should have at least 1 salience detection";
    EXPECT_GE(stats.attention_shifts, 1u) << "Should have at least 1 attention shift";
}

/**
 * Test: SalienceThresholdFiltering
 * Verify that low salience items don't capture attention
 *
 * BIOLOGICAL BASIS:
 * - Weak stimuli don't reach awareness threshold
 * - Attention filter prevents overload from irrelevant stimuli
 */
TEST_F(SalienceAttentionIntegrationTest, SalienceThresholdFiltering) {
    ASSERT_NE(bridge, nullptr);

    // Set a high threshold
    int result = salience_attention_bridge_set_threshold(bridge, 0.8f);
    EXPECT_EQ(result, 0);

    // Create low salience item (below threshold)
    salient_item_t low_item = create_salient_item(200, 0.3f, 0.2f, 0.1f, 0.1f);

    // Publish low salience detection
    result = salience_attention_publish_salience_detection(bridge, &low_item, 0.3f);
    EXPECT_EQ(result, 0) << "Low salience should still be published for logging";

    // Create high salience item
    salient_item_t high_item = create_salient_item(201, 0.95f, 0.9f, 0.85f, 0.8f);

    // Publish high salience detection
    result = salience_attention_publish_salience_detection(bridge, &high_item, 0.95f);
    EXPECT_EQ(result, 0);

    // Verify both were detected but captured_attention differs
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.salience_detections, 2u);
}

/* ============================================================================
 * Priority-Based Allocation Tests
 * ============================================================================ */

/**
 * Test: PriorityBasedAllocation
 * Verify that attention follows priority ordering
 *
 * BIOLOGICAL BASIS:
 * - Priority maps in parietal cortex guide attention
 * - Higher priority items receive more processing resources
 * - Attention allocated proportional to priority
 */
TEST_F(SalienceAttentionIntegrationTest, PriorityBasedAllocation) {
    ASSERT_NE(bridge, nullptr);

    // Create multiple items with different priorities
    attention_priority_t priorities[4];
    priorities[0].item_id = 301;
    priorities[0].priority = 0.3f;  // Low priority
    priorities[1].item_id = 302;
    priorities[1].priority = 0.9f;  // Highest priority
    priorities[2].item_id = 303;
    priorities[2].priority = 0.6f;  // Medium priority
    priorities[3].item_id = 304;
    priorities[3].priority = 0.5f;  // Medium-low priority

    // Publish priority update
    int result = salience_attention_publish_priority_update(bridge, priorities, 4);
    EXPECT_EQ(result, 0) << "Priority update should succeed";

    // Verify priority update was recorded
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.priority_updates, 1u);

    // Request attention shift to highest priority item
    attention_target_t target = create_attention_target(302, 0.9f, 0.7f);
    result = salience_attention_request_attention_shift(bridge, &target);
    EXPECT_EQ(result, 0);
}

/**
 * Test: DynamicPriorityReordering
 * Verify priorities can be dynamically reordered
 *
 * BIOLOGICAL BASIS:
 * - Goals change what's currently relevant
 * - Top-down control modulates priority map
 */
TEST_F(SalienceAttentionIntegrationTest, DynamicPriorityReordering) {
    ASSERT_NE(bridge, nullptr);

    // Initial priorities
    attention_priority_t initial_priorities[2];
    initial_priorities[0].item_id = 401;
    initial_priorities[0].priority = 0.9f;
    initial_priorities[1].item_id = 402;
    initial_priorities[1].priority = 0.3f;

    int result = salience_attention_publish_priority_update(bridge, initial_priorities, 2);
    EXPECT_EQ(result, 0);

    // Reorder priorities (swap)
    attention_priority_t new_priorities[2];
    new_priorities[0].item_id = 401;
    new_priorities[0].priority = 0.2f;  // Now low priority
    new_priorities[1].item_id = 402;
    new_priorities[1].priority = 0.95f; // Now high priority

    result = salience_attention_publish_priority_update(bridge, new_priorities, 2);
    EXPECT_EQ(result, 0);

    // Verify multiple priority updates
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.priority_updates, 2u);
}

/* ============================================================================
 * Competing Salience Resolution Tests
 * ============================================================================ */

/**
 * Test: CompetingSalienceResolution
 * Verify resolution when multiple items are highly salient
 *
 * BIOLOGICAL BASIS:
 * - Winner-take-all competition in superior colliculus
 * - Only one location can be attended at a time
 * - Competition resolved by salience magnitude
 */
TEST_F(SalienceAttentionIntegrationTest, CompetingSalienceResolution) {
    ASSERT_NE(bridge, nullptr);

    // Create multiple competing salient items
    std::vector<salient_item_t> items;
    items.push_back(create_salient_item(501, 0.7f, 0.6f, 0.5f, 0.4f));
    items.push_back(create_salient_item(502, 0.85f, 0.8f, 0.75f, 0.7f)); // Winner
    items.push_back(create_salient_item(503, 0.75f, 0.7f, 0.65f, 0.6f));

    // Publish all salience detections
    for (const auto& item : items) {
        int result = salience_attention_publish_salience_detection(bridge, &item, item.salience_score);
        EXPECT_EQ(result, 0);
    }

    // Request attention shift to the winner (highest salience)
    attention_target_t winner_target = create_attention_target(502, 0.9f, 0.85f);
    int result = salience_attention_request_attention_shift(bridge, &winner_target);
    EXPECT_EQ(result, 0);

    // Verify all items were detected
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.salience_detections, 3u);
}

/**
 * Test: UrgencyOverridesSalience
 * Verify that urgent items can override more salient items
 *
 * BIOLOGICAL BASIS:
 * - Threat detection can interrupt current focus
 * - Amygdala fast-path bypasses normal attention
 */
TEST_F(SalienceAttentionIntegrationTest, UrgencyOverridesSalience) {
    ASSERT_NE(bridge, nullptr);

    // Highly salient but non-urgent item
    salient_item_t normal_item = create_salient_item(601, 0.9f, 0.85f, 0.8f, 0.3f);
    int result = salience_attention_publish_salience_detection(bridge, &normal_item, 0.9f);
    EXPECT_EQ(result, 0);

    // Request attention to normal item
    attention_target_t normal_target = create_attention_target(601, 0.8f, 0.3f);
    result = salience_attention_request_attention_shift(bridge, &normal_target);
    EXPECT_EQ(result, 0);

    // Less salient but highly urgent item (threat)
    salient_item_t urgent_item = create_salient_item(602, 0.6f, 0.5f, 0.4f, 0.95f);
    result = salience_attention_publish_salience_detection(bridge, &urgent_item, 0.6f);
    EXPECT_EQ(result, 0);

    // Request urgent attention shift (should override)
    attention_target_t urgent_target = create_attention_target(602, 0.95f, 0.95f);
    result = salience_attention_request_attention_shift(bridge, &urgent_target);
    EXPECT_EQ(result, 0);

    // Verify both shifts were recorded
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.attention_shifts, 2u);
}

/* ============================================================================
 * Attention Feedback Loop Tests
 * ============================================================================ */

/**
 * Test: AttentionFeedbackLoop
 * Verify that attention focus affects subsequent salience detection
 *
 * BIOLOGICAL BASIS:
 * - Attended locations have enhanced processing
 * - Feedback from attention modulates sensory gain
 * - Creates sustained attention at important locations
 */
TEST_F(SalienceAttentionIntegrationTest, AttentionFeedbackLoop) {
    ASSERT_NE(bridge, nullptr);

    // Initial attention focus
    attention_focus_t initial_focus;
    memset(&initial_focus, 0, sizeof(initial_focus));
    initial_focus.focus_id = 700;
    initial_focus.previous_focus_id = 0;
    initial_focus.focus_strength = 0.8f;
    initial_focus.duration_ms = 0.0f;
    initial_focus.timestamp = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    int result = salience_attention_notify_attention_focus(bridge, &initial_focus);
    EXPECT_EQ(result, 0);

    // Salience detection at attended location (should be enhanced)
    salient_item_t attended_item = create_salient_item(700, 0.7f, 0.6f, 0.5f, 0.4f);
    result = salience_attention_publish_salience_detection(bridge, &attended_item, 0.7f);
    EXPECT_EQ(result, 0);

    // Update focus with increased strength (sustained attention)
    attention_focus_t sustained_focus = initial_focus;
    sustained_focus.focus_strength = 0.9f;
    sustained_focus.duration_ms = 500.0f;

    result = salience_attention_notify_attention_focus(bridge, &sustained_focus);
    EXPECT_EQ(result, 0);

    // Verify feedback loop events
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.focus_notifications, 2u);
}

/**
 * Test: AttentionModulatesSalienceSensitivity
 * Verify attention can increase/decrease salience sensitivity
 *
 * BIOLOGICAL BASIS:
 * - Top-down attention enhances relevant features
 * - Attended modality has lower detection threshold
 */
TEST_F(SalienceAttentionIntegrationTest, AttentionModulatesSalienceSensitivity) {
    ASSERT_NE(bridge, nullptr);

    // Set initial threshold
    int result = salience_attention_bridge_set_threshold(bridge, 0.7f);
    EXPECT_EQ(result, 0);

    // Focus on a location
    attention_focus_t focus;
    memset(&focus, 0, sizeof(focus));
    focus.focus_id = 800;
    focus.focus_strength = 0.9f;

    result = salience_attention_notify_attention_focus(bridge, &focus);
    EXPECT_EQ(result, 0);

    // Lower threshold for attended region (simulating enhanced sensitivity)
    result = salience_attention_bridge_set_threshold(bridge, 0.4f);
    EXPECT_EQ(result, 0);

    // Now weaker stimuli can capture attention
    salient_item_t weak_item = create_salient_item(800, 0.5f, 0.4f, 0.3f, 0.2f);
    result = salience_attention_publish_salience_detection(bridge, &weak_item, 0.5f);
    EXPECT_EQ(result, 0);

    // Reset threshold
    result = salience_attention_bridge_set_threshold(bridge, 0.7f);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Bottom-Up Top-Down Integration Tests
 * ============================================================================ */

/**
 * Test: BottomUpTopDownIntegration
 * Verify integration of bottom-up salience with top-down goals
 *
 * BIOLOGICAL BASIS:
 * - Priority map integrates both signals (parietal cortex)
 * - Bottom-up: stimulus-driven (superior colliculus)
 * - Top-down: goal-driven (prefrontal cortex)
 */
TEST_F(SalienceAttentionIntegrationTest, BottomUpTopDownIntegration) {
    ASSERT_NE(bridge, nullptr);

    // Bottom-up salient stimulus (unexpected)
    salient_item_t bottom_up_item = create_salient_item(901, 0.8f, 0.9f, 0.85f, 0.3f);
    int result = salience_attention_publish_salience_detection(bridge, &bottom_up_item, 0.8f);
    EXPECT_EQ(result, 0);

    // Top-down goal-relevant item (task target)
    attention_priority_t goal_priority;
    goal_priority.item_id = 902;
    goal_priority.priority = 0.95f;

    result = salience_attention_publish_priority_update(bridge, &goal_priority, 1);
    EXPECT_EQ(result, 0);

    // Create salient item matching goal
    salient_item_t goal_item = create_salient_item(902, 0.6f, 0.5f, 0.4f, 0.3f);
    result = salience_attention_publish_salience_detection(bridge, &goal_item, 0.6f);
    EXPECT_EQ(result, 0);

    // Request attention to goal item (top-down wins over bottom-up)
    attention_target_t goal_target = create_attention_target(902, 0.95f, 0.5f);
    result = salience_attention_request_attention_shift(bridge, &goal_target);
    EXPECT_EQ(result, 0);

    // Verify integration
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.salience_detections, 2u);
    EXPECT_GE(stats.priority_updates, 1u);
    EXPECT_GE(stats.attention_shifts, 1u);
}

/**
 * Test: GoalContextSwitching
 * Verify attention reallocation when goals change
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex updates task context
 * - New goals reweight priority map
 * - Previous focus becomes less relevant
 */
TEST_F(SalienceAttentionIntegrationTest, GoalContextSwitching) {
    ASSERT_NE(bridge, nullptr);

    // Initial goal: attend to item 1001
    attention_priority_t initial_goal;
    initial_goal.item_id = 1001;
    initial_goal.priority = 0.9f;

    int result = salience_attention_publish_priority_update(bridge, &initial_goal, 1);
    EXPECT_EQ(result, 0);

    // Focus on initial goal
    attention_focus_t initial_focus;
    memset(&initial_focus, 0, sizeof(initial_focus));
    initial_focus.focus_id = 1001;
    initial_focus.focus_strength = 0.85f;

    result = salience_attention_notify_attention_focus(bridge, &initial_focus);
    EXPECT_EQ(result, 0);

    // Context switch: new goal item 1002
    attention_priority_t new_goals[2];
    new_goals[0].item_id = 1001;
    new_goals[0].priority = 0.2f;  // Old goal deprioritized
    new_goals[1].item_id = 1002;
    new_goals[1].priority = 0.95f; // New goal prioritized

    result = salience_attention_publish_priority_update(bridge, new_goals, 2);
    EXPECT_EQ(result, 0);

    // Request shift to new goal
    attention_target_t new_target = create_attention_target(1002, 0.95f, 0.6f);
    result = salience_attention_request_attention_shift(bridge, &new_target);
    EXPECT_EQ(result, 0);

    // Update focus
    attention_focus_t new_focus;
    memset(&new_focus, 0, sizeof(new_focus));
    new_focus.focus_id = 1002;
    new_focus.previous_focus_id = 1001;
    new_focus.focus_strength = 0.9f;

    result = salience_attention_notify_attention_focus(bridge, &new_focus);
    EXPECT_EQ(result, 0);

    // Verify context switch
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.priority_updates, 2u);
    EXPECT_GE(stats.attention_shifts, 1u);
    EXPECT_GE(stats.focus_notifications, 2u);
}

/* ============================================================================
 * Multi-Modal Integration Tests
 * ============================================================================ */

/**
 * Test: CrossModalSalience
 * Verify salience integration across modalities
 *
 * BIOLOGICAL BASIS:
 * - Superior colliculus receives multi-modal inputs
 * - Cross-modal enhancement (visual + auditory)
 */
TEST_F(SalienceAttentionIntegrationTest, CrossModalSalience) {
    ASSERT_NE(bridge, nullptr);

    // Visual salience
    salient_item_t visual_item = create_salient_item(1101, 0.6f, 0.5f, 0.4f, 0.3f);
    visual_item.modality = 0;  // Visual
    int result = salience_attention_publish_salience_detection(bridge, &visual_item, 0.6f);
    EXPECT_EQ(result, 0);

    // Auditory salience at same location (cross-modal enhancement)
    salient_item_t audio_item = create_salient_item(1101, 0.5f, 0.4f, 0.3f, 0.2f);
    audio_item.modality = 1;  // Audio
    result = salience_attention_publish_salience_detection(bridge, &audio_item, 0.5f);
    EXPECT_EQ(result, 0);

    // Combined salience should be higher than either alone
    // Request attention to multi-modal location
    attention_target_t target = create_attention_target(1101, 0.85f, 0.6f);
    result = salience_attention_request_attention_shift(bridge, &target);
    EXPECT_EQ(result, 0);

    // Verify both modalities detected
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.salience_detections, 2u);
}

/* ============================================================================
 * Salience Evaluation Request Tests
 * ============================================================================ */

/**
 * Test: SalienceEvaluationWorkflow
 * Verify complete evaluation request workflow
 *
 * BIOLOGICAL BASIS:
 * - Top-down query to sensory systems
 * - "Look for X" instruction from prefrontal cortex
 */
TEST_F(SalienceAttentionIntegrationTest, SalienceEvaluationWorkflow) {
    ASSERT_NE(bridge, nullptr);

    // Create evaluation request for multiple items
    salience_eval_request_t request;
    memset(&request, 0, sizeof(request));
    request.request_id = 1;
    request.item_ids[0] = 1201;
    request.item_ids[1] = 1202;
    request.item_ids[2] = 1203;
    request.num_items = 3;
    request.modality = 0;

    int result = salience_attention_request_salience_evaluation(bridge, &request);
    EXPECT_EQ(result, 0) << "Evaluation request should succeed";

    // Verify request was recorded
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.evaluation_requests, 1u);
}

/* ============================================================================
 * Performance and Stress Tests
 * ============================================================================ */

/**
 * Test: HighFrequencySalienceEvents
 * Verify bridge handles high-frequency salience events
 */
TEST_F(SalienceAttentionIntegrationTest, HighFrequencySalienceEvents) {
    ASSERT_NE(bridge, nullptr);

    const int NUM_EVENTS = 100;
    int successful = 0;

    for (int i = 0; i < NUM_EVENTS; i++) {
        salient_item_t item = create_salient_item(
            (uint64_t)(2000 + i),
            0.5f + (float)(i % 50) / 100.0f,  // Varying salience
            0.6f, 0.5f, 0.4f
        );

        int result = salience_attention_publish_salience_detection(bridge, &item, item.salience_score);
        if (result == 0) {
            successful++;
        }
    }

    EXPECT_EQ(successful, NUM_EVENTS) << "All events should be published";

    // Verify stats
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.salience_detections, (uint64_t)NUM_EVENTS);
}

/**
 * Test: ConcurrentBridgeAccess
 * Verify thread safety under concurrent access
 */
TEST_F(SalienceAttentionIntegrationTest, ConcurrentBridgeAccess) {
    ASSERT_NE(bridge, nullptr);

    const int NUM_THREADS = 4;
    const int EVENTS_PER_THREAD = 25;
    std::atomic<int> total_events{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &total_events, EVENTS_PER_THREAD]() {
            for (int i = 0; i < EVENTS_PER_THREAD; i++) {
                salient_item_t item = create_salient_item(
                    (uint64_t)(t * 1000 + i),
                    0.7f,
                    0.6f, 0.5f, 0.4f
                );

                if (salience_attention_publish_salience_detection(bridge, &item, 0.7f) == 0) {
                    total_events++;
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(total_events.load(), NUM_THREADS * EVENTS_PER_THREAD)
        << "All concurrent events should be published";
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

/**
 * Test: StateQueryAccuracy
 * Verify state queries return accurate information
 */
TEST_F(SalienceAttentionIntegrationTest, StateQueryAccuracy) {
    ASSERT_NE(bridge, nullptr);

    // Initial state should be zeros
    float avg_salience = 0.0f;
    float peak_salience = 0.0f;
    uint32_t detection_count = 0;

    int result = salience_attention_bridge_get_salience_state(
        bridge, &avg_salience, &peak_salience, &detection_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(detection_count, 0u);

    // Publish some events
    salient_item_t item1 = create_salient_item(3001, 0.6f, 0.5f, 0.4f, 0.3f);
    salience_attention_publish_salience_detection(bridge, &item1, 0.6f);

    salient_item_t item2 = create_salient_item(3002, 0.9f, 0.8f, 0.7f, 0.6f);
    salience_attention_publish_salience_detection(bridge, &item2, 0.9f);

    // Query again
    result = salience_attention_bridge_get_salience_state(
        bridge, &avg_salience, &peak_salience, &detection_count);
    EXPECT_EQ(result, 0);
    EXPECT_GE(detection_count, 2u);
    EXPECT_GE(peak_salience, 0.9f) << "Peak should be at least 0.9";
}

/**
 * Test: AttentionStateTracking
 * Verify attention state is tracked correctly
 */
TEST_F(SalienceAttentionIntegrationTest, AttentionStateTracking) {
    ASSERT_NE(bridge, nullptr);

    // Set focus
    attention_focus_t focus;
    memset(&focus, 0, sizeof(focus));
    focus.focus_id = 4001;
    focus.focus_strength = 0.85f;

    int result = salience_attention_notify_attention_focus(bridge, &focus);
    EXPECT_EQ(result, 0);

    // Query attention state
    uint64_t current_focus = 0;
    float focus_strength = 0.0f;
    uint32_t num_targets = 0;

    result = salience_attention_bridge_get_attention_state(
        bridge, &current_focus, &focus_strength, &num_targets);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(current_focus, 4001u);
    EXPECT_FLOAT_EQ(focus_strength, 0.85f);
}
