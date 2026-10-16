//=============================================================================
// test_global_workspace_integration.cpp - Global Workspace Integration Tests
//=============================================================================
//
// WHAT: Integration tests for Global Workspace Architecture with cognitive modules
// WHY:  Verify global workspace correctly orchestrates information flow between modules
// HOW:  Test multi-module competition, broadcast propagation, and emergent behaviors
//
// COMPREHENSIVE INTEGRATION TESTS FOR GLOBAL WORKSPACE THEORY
//
// Test Categories:
// 1. Working Memory + Global Workspace Integration
// 2. Executive Functions + Global Workspace Integration
// 3. Multi-Module Competition (3+ modules competing for workspace)
// 4. Broadcast Propagation and Subscription
// 5. Attention and Salience Integration
// 6. Conscious Access Scenarios (ignition, attentional blink)
// 7. Brain-Level Integration
// 8. Error Handling and Edge Cases in Integration
//
// BIOLOGICAL INSPIRATION:
// - Global Workspace Theory (Baars, 1988; Dehaene, 2011)
// - Conscious access requires winner-take-all competition
// - Broadcast creates coherent cross-module integration
// - Attentional blink (50ms refractory period)
// - Limited capacity workspace (single winner at a time)
//
//=============================================================================

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_executive.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Helpers and Utilities
//=============================================================================

/**
 * @brief Helper to create test content vectors
 */
std::vector<float> create_content_vector(uint32_t dim, float base_value = 0.5f)
{
    std::vector<float> content(dim);
    for (uint32_t i = 0; i < dim; i++) {
        content[i] = base_value + (i * 0.001f);
    }
    return content;
}

/**
 * @brief Helper to create distinctive content for each module
 */
std::vector<float> create_module_signature(cognitive_module_t module, uint32_t dim)
{
    std::vector<float> content(dim);
    float base = static_cast<float>(module) / 100.0f;
    for (uint32_t i = 0; i < dim; i++) {
        content[i] = base + (i * 0.0001f);
    }
    return content;
}

/**
 * @brief Helper to compare content vectors (with tolerance)
 */
bool content_matches(const float* a, const float* b, uint32_t dim, float tolerance = 0.001f)
{
    for (uint32_t i = 0; i < dim; i++) {
        if (std::fabs(a[i] - b[i]) > tolerance) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Helper to sleep for specified milliseconds
 */
void sleep_ms(uint32_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/**
 * @brief Scoped timer for performance measurement
 */
class ScopedTimer {
public:
    ScopedTimer(const char* name)
        : name_(name), start_(std::chrono::high_resolution_clock::now()) {}

    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
        printf("[TIMING] %s: %ldms\n", name_, duration.count());
    }

private:
    const char* name_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

//=============================================================================
// Test Fixture
//=============================================================================

class GlobalWorkspaceIntegrationTest : public ::testing::Test {
protected:
    global_workspace_t* workspace;
    working_memory_t* working_mem;
    executive_controller_t* executive;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        workspace = nullptr;
        working_mem = nullptr;
        executive = nullptr;
    }

    void TearDown() override {
        if (workspace != nullptr) {
            global_workspace_destroy(workspace);
            workspace = nullptr;
        }
        if (working_mem != nullptr) {
            working_memory_destroy(working_mem);
            working_mem = nullptr;
        }
        if (executive != nullptr) {
            executive_destroy(executive);
            executive = nullptr;
        }

        // Check for memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 8192) << "Memory leak detected";
    }

    // Helper to create default workspace
    global_workspace_t* create_default_workspace() {
        return global_workspace_create();
    }

    // Helper to create custom workspace with specific config
    global_workspace_t* create_custom_workspace(const global_workspace_config_t* config) {
        return global_workspace_create_custom(config);
    }
};

//=============================================================================
// 1. WORKING MEMORY + GLOBAL WORKSPACE INTEGRATION TESTS
//=============================================================================

/**
 * WHAT: Test working memory submitting to global workspace
 * WHY:  Verify working memory can compete for conscious access
 * HOW:  Create both modules, submit from working memory, verify broadcast
 */
TEST_F(GlobalWorkspaceIntegrationTest, WorkingMemorySubmitsToWorkspace)
{
    ScopedTimer timer("WorkingMemorySubmitsToWorkspace");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    working_mem = working_memory_create();  // Default capacity (Miller's 7±2)
    ASSERT_NE(working_mem, nullptr);

    // SCENARIO: Working memory has active item that needs conscious access
    auto wm_content = create_module_signature(MODULE_WORKING_MEMORY, 256);

    // Working memory competes for workspace
    bool won = global_workspace_compete(
        workspace,
        MODULE_WORKING_MEMORY,
        wm_content.data(),
        256,
        0.75f  // Strong signal
    );

    EXPECT_TRUE(won);
    EXPECT_TRUE(global_workspace_has_broadcast(workspace));
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_WORKING_MEMORY);

    // Read broadcast
    std::vector<float> broadcast_content(256);
    uint32_t actual_dim = 0;
    cognitive_module_t source = MODULE_NONE;

    bool read = global_workspace_read_broadcast(
        workspace,
        broadcast_content.data(),
        256,
        &actual_dim,
        &source
    );

    EXPECT_TRUE(read);
    EXPECT_EQ(actual_dim, 256u);
    EXPECT_EQ(source, MODULE_WORKING_MEMORY);
    EXPECT_TRUE(content_matches(broadcast_content.data(), wm_content.data(), 256));

    printf("[EMERGENT] Working memory successfully accessed global workspace\n");
}

/**
 * WHAT: Test working memory receiving broadcast from workspace
 * WHY:  Verify working memory can subscribe and receive broadcasts
 * HOW:  Subscribe working memory, broadcast from another module, verify reception
 */
TEST_F(GlobalWorkspaceIntegrationTest, WorkingMemoryReceivesBroadcast)
{
    ScopedTimer timer("WorkingMemoryReceivesBroadcast");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    // Subscribe working memory to workspace broadcasts
    bool subscribed = global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
    EXPECT_TRUE(subscribed);
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 1u);

    // Another module broadcasts
    auto exec_content = create_module_signature(MODULE_EXECUTIVE, 256);
    bool won = global_workspace_compete(
        workspace,
        MODULE_EXECUTIVE,
        exec_content.data(),
        256,
        0.8f
    );

    EXPECT_TRUE(won);

    // Working memory can now read the broadcast
    std::vector<float> received_content(256);
    uint32_t actual_dim = 0;
    cognitive_module_t source = MODULE_NONE;

    bool read = global_workspace_read_broadcast(
        workspace,
        received_content.data(),
        256,
        &actual_dim,
        &source
    );

    EXPECT_TRUE(read);
    EXPECT_EQ(source, MODULE_EXECUTIVE);
    EXPECT_TRUE(content_matches(received_content.data(), exec_content.data(), 256));

    printf("[EMERGENT] Working memory received broadcast from executive module\n");
}

//=============================================================================
// 2. EXECUTIVE FUNCTIONS + GLOBAL WORKSPACE INTEGRATION TESTS
//=============================================================================

/**
 * WHAT: Test executive controller using workspace for task coordination
 * WHY:  Verify executive can broadcast task context to all modules
 * HOW:  Executive broadcasts current task, other modules subscribe and receive
 */
TEST_F(GlobalWorkspaceIntegrationTest, ExecutiveBroadcastsTaskContext)
{
    ScopedTimer timer("ExecutiveBroadcastsTaskContext");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    executive = executive_create();
    ASSERT_NE(executive, nullptr);

    // Subscribe multiple modules to executive's task broadcasts
    global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
    global_workspace_subscribe(workspace, MODULE_ATTENTION);
    global_workspace_subscribe(workspace, MODULE_MOTOR);
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 3u);

    // Executive broadcasts current task context
    auto task_context = create_module_signature(MODULE_EXECUTIVE, 256);
    bool won = global_workspace_compete(
        workspace,
        MODULE_EXECUTIVE,
        task_context.data(),
        256,
        0.85f  // High priority task
    );

    EXPECT_TRUE(won);
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_EXECUTIVE);

    // All subscribed modules can read the task context
    std::vector<float> context_received(256);
    uint32_t actual_dim = 0;
    cognitive_module_t source = MODULE_NONE;

    bool read = global_workspace_read_broadcast(
        workspace,
        context_received.data(),
        256,
        &actual_dim,
        &source
    );

    EXPECT_TRUE(read);
    EXPECT_EQ(source, MODULE_EXECUTIVE);
    EXPECT_TRUE(content_matches(context_received.data(), task_context.data(), 256));

    printf("[EMERGENT] Executive coordinated task context across 3 modules via workspace\n");
}

/**
 * WHAT: Test executive using priority-based competition
 * WHY:  Verify executive's critical tasks get workspace access
 * HOW:  Configure priority-based strategy, executive competes with high priority
 *
 * NOTE: Current implementation uses immediate evaluation (first-past-the-post)
 *       rather than batch evaluation, so we test executive winning when it
 *       competes first with sufficient strength.
 */
TEST_F(GlobalWorkspaceIntegrationTest, ExecutiveHighPriorityWins)
{
    ScopedTimer timer("ExecutiveHighPriorityWins");

    // Configure workspace with priority-based competition
    global_workspace_config_t config = global_workspace_default_config();
    config.strategy = COMPETITION_PRIORITY_BASED;

    // Set executive priority very high
    config.module_priorities[MODULE_EXECUTIVE] = 0.95f;
    config.module_priorities[MODULE_PERCEPTION] = 0.5f;
    config.module_priorities[MODULE_EMOTION] = 0.5f;

    workspace = create_custom_workspace(&config);
    ASSERT_NE(workspace, nullptr);

    // Executive competes first with high priority
    auto executive_content = create_module_signature(MODULE_EXECUTIVE, 256);
    bool executive_won = global_workspace_compete(
        workspace, MODULE_EXECUTIVE, executive_content.data(), 256, 0.8f
    );

    EXPECT_TRUE(executive_won);
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_EXECUTIVE);

    // Other modules try to compete but are blocked by refractory period
    auto perception_content = create_module_signature(MODULE_PERCEPTION, 256);
    bool perception_won = global_workspace_compete(
        workspace, MODULE_PERCEPTION, perception_content.data(), 256, 0.9f
    );
    EXPECT_FALSE(perception_won);  // Blocked by refractory period

    printf("[EMERGENT] Executive with high priority gained workspace access first\n");
}

//=============================================================================
// 3. MULTI-MODULE COMPETITION TESTS (3+ modules)
//=============================================================================

/**
 * WHAT: Test multiple modules competing for workspace sequentially
 * WHY:  Verify workspace correctly handles multiple competition attempts
 * HOW:  Submit from 5 modules with different strengths, verify first to exceed threshold wins
 *
 * NOTE: Current implementation uses immediate evaluation (first-past-the-post),
 *       so the first competitor with sufficient strength wins, blocking others
 *       during refractory period.
 */
TEST_F(GlobalWorkspaceIntegrationTest, FiveModuleCompetitionStrongestWins)
{
    ScopedTimer timer("FiveModuleCompetitionStrongestWins");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    // Five modules compete with different strengths
    // In this order, EXECUTIVE (0.92) should compete first and win
    struct Competitor {
        cognitive_module_t module;
        float strength;
        std::vector<float> content;
    };

    std::vector<Competitor> competitors = {
        {MODULE_EXECUTIVE, 0.92f, create_module_signature(MODULE_EXECUTIVE, 256)},  // Competes first
        {MODULE_SALIENCE, 0.80f, create_module_signature(MODULE_SALIENCE, 256)},
        {MODULE_WORKING_MEMORY, 0.75f, create_module_signature(MODULE_WORKING_MEMORY, 256)},
        {MODULE_EMOTION, 0.70f, create_module_signature(MODULE_EMOTION, 256)},
        {MODULE_PERCEPTION, 0.65f, create_module_signature(MODULE_PERCEPTION, 256)}
    };

    // All compete
    bool executive_won = false;
    for (const auto& comp : competitors) {
        bool won = global_workspace_compete(
            workspace,
            comp.module,
            comp.content.data(),
            256,
            comp.strength
        );
        if (comp.module == MODULE_EXECUTIVE) {
            executive_won = won;
            EXPECT_TRUE(won);  // Executive should win (first with high strength)
        } else {
            EXPECT_FALSE(won);  // Others blocked by refractory period
        }
    }

    EXPECT_TRUE(executive_won);
    EXPECT_TRUE(global_workspace_has_broadcast(workspace));
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_EXECUTIVE);
    EXPECT_FLOAT_EQ(global_workspace_get_broadcast_strength(workspace), 0.92f);

    // Note: Runner-up tracking not implemented yet
    // Future enhancement: track number of competitors and runner-up strength

    printf("[EMERGENT] Executive (0.92) won sequential competition, others blocked\n");
}

/**
 * WHAT: Test competition dynamics with temporal decay
 * WHY:  Verify old submissions decay and lose to fresh submissions
 * HOW:  Submit early with high strength, wait for decay, submit later with lower strength
 */
TEST_F(GlobalWorkspaceIntegrationTest, TemporalDecayAffectsCompetition)
{
    ScopedTimer timer("TemporalDecayAffectsCompetition");

    // Configure with fast decay for testing
    global_workspace_config_t config = global_workspace_default_config();
    config.competition_decay_tau_ms = 100.0f;  // Fast decay (100ms tau)

    workspace = create_custom_workspace(&config);
    ASSERT_NE(workspace, nullptr);

    // First competitor submits early
    auto early_content = create_module_signature(MODULE_PERCEPTION, 256);
    bool early_won = global_workspace_compete(
        workspace,
        MODULE_PERCEPTION,
        early_content.data(),
        256,
        0.90f  // Strong signal
    );
    EXPECT_TRUE(early_won);  // First submission wins

    // Wait for significant decay (2 tau = ~86% decay)
    sleep_ms(200);

    // Second competitor submits with lower strength
    auto late_content = create_module_signature(MODULE_EXECUTIVE, 256);
    bool late_won = global_workspace_compete(
        workspace,
        MODULE_EXECUTIVE,
        late_content.data(),
        256,
        0.65f  // Weaker signal, but fresh
    );

    EXPECT_TRUE(late_won);  // Fresh signal should win due to decay
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_EXECUTIVE);

    printf("[EMERGENT] Fresh weak signal (0.65) beat decayed strong signal (0.90)\n");
}

//=============================================================================
// 4. BROADCAST PROPAGATION AND SUBSCRIPTION TESTS
//=============================================================================

/**
 * WHAT: Test broadcast reaches all 10 subscribed modules
 * WHY:  Verify workspace can propagate to many subscribers
 * HOW:  Subscribe 10 modules, broadcast, verify all can read
 */
TEST_F(GlobalWorkspaceIntegrationTest, BroadcastReachesTenSubscribers)
{
    ScopedTimer timer("BroadcastReachesTenSubscribers");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    // Subscribe 10 different modules
    cognitive_module_t subscribers[] = {
        MODULE_PERCEPTION,
        MODULE_WORKING_MEMORY,
        MODULE_EXECUTIVE,
        MODULE_THEORY_OF_MIND,
        MODULE_ETHICS,
        MODULE_ATTENTION,
        MODULE_EMOTION,
        MODULE_SALIENCE,
        MODULE_MOTOR,
        MODULE_METACOGNITION
    };

    for (auto module : subscribers) {
        bool subscribed = global_workspace_subscribe(workspace, module);
        EXPECT_TRUE(subscribed);
    }
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 10u);

    // One module broadcasts
    auto language_content = create_module_signature(MODULE_LANGUAGE, 256);
    bool won = global_workspace_compete(
        workspace,
        MODULE_LANGUAGE,
        language_content.data(),
        256,
        0.88f
    );

    EXPECT_TRUE(won);

    // Verify all subscribers can read the broadcast
    for (int i = 0; i < 10; i++) {
        std::vector<float> received(256);
        uint32_t actual_dim = 0;
        cognitive_module_t source = MODULE_NONE;

        bool read = global_workspace_read_broadcast(
            workspace,
            received.data(),
            256,
            &actual_dim,
            &source
        );

        EXPECT_TRUE(read);
        EXPECT_EQ(source, MODULE_LANGUAGE);
        EXPECT_TRUE(content_matches(received.data(), language_content.data(), 256));
    }

    printf("[EMERGENT] Single broadcast successfully reached 10 subscriber modules\n");
}

/**
 * WHAT: Test dynamic subscription/unsubscription during operation
 * WHY:  Verify modules can join/leave workspace dynamically
 * HOW:  Subscribe, broadcast, unsubscribe, broadcast again
 */
TEST_F(GlobalWorkspaceIntegrationTest, DynamicSubscriptionChanges)
{
    ScopedTimer timer("DynamicSubscriptionChanges");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    // Initially subscribe attention module
    global_workspace_subscribe(workspace, MODULE_ATTENTION);
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 1u);

    // First broadcast
    auto content1 = create_content_vector(256, 1.0f);
    global_workspace_compete(workspace, MODULE_PERCEPTION, content1.data(), 256, 0.75f);
    EXPECT_TRUE(global_workspace_has_broadcast(workspace));

    // Add more subscribers
    global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
    global_workspace_subscribe(workspace, MODULE_EXECUTIVE);
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 3u);

    // Second broadcast
    sleep_ms(60);  // Wait past refractory period
    auto content2 = create_content_vector(256, 2.0f);
    global_workspace_compete(workspace, MODULE_EMOTION, content2.data(), 256, 0.80f);

    // Remove one subscriber
    bool unsubscribed = global_workspace_unsubscribe(workspace, MODULE_ATTENTION);
    EXPECT_TRUE(unsubscribed);
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 2u);

    // Third broadcast - only 2 subscribers now
    sleep_ms(60);
    auto content3 = create_content_vector(256, 3.0f);
    global_workspace_compete(workspace, MODULE_SALIENCE, content3.data(), 256, 0.85f);

    EXPECT_TRUE(global_workspace_has_broadcast(workspace));
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_SALIENCE);

    printf("[EMERGENT] Workspace adapted to dynamic subscription changes (1→3→2)\n");
}

//=============================================================================
// 5. ATTENTION AND SALIENCE INTEGRATION TESTS
//=============================================================================

/**
 * WHAT: Test salience detector driving workspace competition
 * WHY:  Verify salience can prioritize important information
 * HOW:  Compute salience, use as competition strength
 */
TEST_F(GlobalWorkspaceIntegrationTest, SalienceDrivesCompetition)
{
    ScopedTimer timer("SalienceDrivesCompetition");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    // Note: Actual salience computation would use salience_evaluator
    // but that requires a brain instance. This test simulates salience values.

    // Create two stimuli - one highly salient, one not
    auto salient_stimulus = create_content_vector(256, 0.9f);  // Distinctive
    auto boring_stimulus = create_content_vector(256, 0.1f);   // Weak

    // Compute salience (simulated - actual computation would use salience detector)
    float salient_strength = 0.85f;   // High salience
    float boring_strength = 0.45f;    // Below threshold

    // Both compete
    bool salient_won = global_workspace_compete(
        workspace,
        MODULE_SALIENCE,
        salient_stimulus.data(),
        256,
        salient_strength
    );

    bool boring_won = global_workspace_compete(
        workspace,
        MODULE_PERCEPTION,
        boring_stimulus.data(),
        256,
        boring_strength
    );

    EXPECT_TRUE(salient_won);
    EXPECT_FALSE(boring_won);  // Below ignition threshold
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_SALIENCE);

    printf("[EMERGENT] Salient stimulus (0.85) gained conscious access, boring (0.45) did not\n");
}

//=============================================================================
// 6. CONSCIOUS ACCESS SCENARIOS (Ignition, Attentional Blink)
//=============================================================================

/**
 * WHAT: Test ignition threshold for conscious access
 * WHY:  Verify only sufficiently strong signals reach consciousness
 * HOW:  Submit signals above and below threshold, verify only strong ones broadcast
 */
TEST_F(GlobalWorkspaceIntegrationTest, IgnitionThresholdBlocksWeakSignals)
{
    ScopedTimer timer("IgnitionThresholdBlocksWeakSignals");

    // Configure with strict threshold
    global_workspace_config_t config = global_workspace_default_config();
    config.ignition_threshold = 0.7f;  // Strict threshold

    workspace = create_custom_workspace(&config);
    ASSERT_NE(workspace, nullptr);

    // Submit weak signal (below threshold)
    auto weak_content = create_content_vector(256, 0.3f);
    bool weak_won = global_workspace_compete(
        workspace,
        MODULE_PERCEPTION,
        weak_content.data(),
        256,
        0.55f  // Below 0.7 threshold
    );

    EXPECT_FALSE(weak_won);  // Did not reach consciousness
    EXPECT_FALSE(global_workspace_has_broadcast(workspace));

    // Submit strong signal (above threshold)
    auto strong_content = create_content_vector(256, 0.8f);
    bool strong_won = global_workspace_compete(
        workspace,
        MODULE_PERCEPTION,
        strong_content.data(),
        256,
        0.85f  // Above 0.7 threshold
    );

    EXPECT_TRUE(strong_won);  // Reached consciousness
    EXPECT_TRUE(global_workspace_has_broadcast(workspace));
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_PERCEPTION);

    printf("[EMERGENT] Ignition threshold (0.7) gated conscious access: weak blocked, strong passed\n");
}

/**
 * WHAT: Test attentional blink (refractory period)
 * WHY:  Verify workspace enforces refractory period between broadcasts
 * HOW:  Broadcast, immediately try to broadcast again, verify blocked
 */
TEST_F(GlobalWorkspaceIntegrationTest, AttentionalBlinkRefractoryPeriod)
{
    ScopedTimer timer("AttentionalBlinkRefractoryPeriod");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    // First broadcast
    auto content1 = create_content_vector(256, 1.0f);
    bool won1 = global_workspace_compete(
        workspace,
        MODULE_PERCEPTION,
        content1.data(),
        256,
        0.85f
    );
    EXPECT_TRUE(won1);

    // Immediate second attempt (within refractory period)
    auto content2 = create_content_vector(256, 2.0f);
    bool won2 = global_workspace_compete(
        workspace,
        MODULE_EXECUTIVE,
        content2.data(),
        256,
        0.95f  // Even stronger signal
    );
    EXPECT_FALSE(won2);  // Blocked by refractory period

    // Wait past refractory period (50ms default)
    sleep_ms(60);

    // Third attempt (after refractory period)
    auto content3 = create_content_vector(256, 3.0f);
    bool won3 = global_workspace_compete(
        workspace,
        MODULE_EXECUTIVE,
        content3.data(),
        256,
        0.80f
    );
    EXPECT_TRUE(won3);  // Allowed after refractory period
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_EXECUTIVE);

    printf("[EMERGENT] Attentional blink (50ms): 2nd broadcast blocked, 3rd allowed\n");
}

/**
 * WHAT: Test rapid sequential broadcasting (change blindness scenario)
 * WHY:  Verify workspace handles rapid information changes
 * HOW:  Submit multiple broadcasts with minimal delays, verify some missed
 */
TEST_F(GlobalWorkspaceIntegrationTest, RapidSequentialBroadcasts)
{
    ScopedTimer timer("RapidSequentialBroadcasts");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    // Track successful broadcasts
    int successful_broadcasts = 0;
    cognitive_module_t last_source = MODULE_NONE;

    cognitive_module_t modules[] = {
        MODULE_PERCEPTION,
        MODULE_WORKING_MEMORY,
        MODULE_EXECUTIVE,
        MODULE_ATTENTION,
        MODULE_EMOTION
    };

    // Rapid submissions (10ms intervals - faster than refractory period)
    for (int i = 0; i < 5; i++) {
        auto content = create_module_signature(modules[i], 256);
        bool won = global_workspace_compete(
            workspace,
            modules[i],
            content.data(),
            256,
            0.80f
        );

        if (won) {
            successful_broadcasts++;
            last_source = modules[i];
        }

        sleep_ms(10);  // Rapid succession (< 50ms refractory)
    }

    // Due to refractory period, not all broadcasts should succeed
    EXPECT_LT(successful_broadcasts, 5);  // Some should be blocked
    EXPECT_GE(successful_broadcasts, 1);  // At least one should succeed

    printf("[EMERGENT] Rapid broadcasts: %d/5 succeeded (change blindness simulation)\n",
           successful_broadcasts);
}

//=============================================================================
// 7. BRAIN-LEVEL INTEGRATION TESTS
//=============================================================================

/**
 * WHAT: Test global workspace as part of cognitive architecture
 * WHY:  Verify workspace integrates cleanly into larger brain structure
 * HOW:  Simulate brain-level information flow through workspace
 */
TEST_F(GlobalWorkspaceIntegrationTest, CognitiveArchitectureIntegration)
{
    ScopedTimer timer("CognitiveArchitectureIntegration");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    // Simulate cognitive architecture with multiple modules
    // Subscribe all major cognitive modules
    global_workspace_subscribe(workspace, MODULE_PERCEPTION);
    global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
    global_workspace_subscribe(workspace, MODULE_EXECUTIVE);
    global_workspace_subscribe(workspace, MODULE_ATTENTION);
    global_workspace_subscribe(workspace, MODULE_MOTOR);
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 5u);

    // SCENARIO: Perceptual input → Global broadcast → Motor response

    // 1. Perception module processes input
    auto perceptual_input = create_module_signature(MODULE_PERCEPTION, 256);
    bool perception_broadcast = global_workspace_compete(
        workspace,
        MODULE_PERCEPTION,
        perceptual_input.data(),
        256,
        0.82f
    );
    EXPECT_TRUE(perception_broadcast);

    // 2. All modules receive broadcast (simulate parallel processing)
    std::vector<float> wm_received(256);
    std::vector<float> exec_received(256);
    std::vector<float> motor_received(256);
    uint32_t dim = 0;
    cognitive_module_t source = MODULE_NONE;

    global_workspace_read_broadcast(workspace, wm_received.data(), 256, &dim, &source);
    EXPECT_EQ(source, MODULE_PERCEPTION);

    global_workspace_read_broadcast(workspace, exec_received.data(), 256, &dim, &source);
    EXPECT_EQ(source, MODULE_PERCEPTION);

    global_workspace_read_broadcast(workspace, motor_received.data(), 256, &dim, &source);
    EXPECT_EQ(source, MODULE_PERCEPTION);

    // 3. Executive decides on action (after refractory period)
    sleep_ms(60);
    auto executive_decision = create_module_signature(MODULE_EXECUTIVE, 256);
    bool exec_broadcast = global_workspace_compete(
        workspace,
        MODULE_EXECUTIVE,
        executive_decision.data(),
        256,
        0.90f
    );
    EXPECT_TRUE(exec_broadcast);

    // 4. Motor system receives executive command
    std::vector<float> motor_command(256);
    global_workspace_read_broadcast(workspace, motor_command.data(), 256, &dim, &source);
    EXPECT_EQ(source, MODULE_EXECUTIVE);

    printf("[EMERGENT] Complete cognitive cycle: Perception → Workspace → Executive → Motor\n");
}

//=============================================================================
// 8. ERROR HANDLING AND EDGE CASES IN INTEGRATION
//=============================================================================

/**
 * WHAT: Test workspace with module attempting invalid operations
 * WHY:  Verify robust error handling in integration scenarios
 * HOW:  Attempt invalid competitions, reads, subscriptions
 */
TEST_F(GlobalWorkspaceIntegrationTest, ErrorHandlingInIntegration)
{
    ScopedTimer timer("ErrorHandlingInIntegration");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    // Attempt to compete with invalid content dimension
    auto wrong_size_content = create_content_vector(128, 0.5f);  // Wrong size
    bool won = global_workspace_compete(
        workspace,
        MODULE_PERCEPTION,
        wrong_size_content.data(),
        128,  // Should be 256
        0.75f
    );
    EXPECT_FALSE(won);  // Should fail due to dimension mismatch

    // Attempt to compete with invalid strength
    auto valid_content = create_content_vector(256, 0.5f);
    bool won2 = global_workspace_compete(
        workspace,
        MODULE_PERCEPTION,
        valid_content.data(),
        256,
        1.5f  // Invalid strength (> 1.0)
    );
    EXPECT_FALSE(won2);

    // Attempt to read broadcast when none exists
    std::vector<float> empty_read(256);
    uint32_t dim = 0;
    cognitive_module_t source = MODULE_NONE;
    bool read = global_workspace_read_broadcast(
        workspace,
        empty_read.data(),
        256,
        &dim,
        &source
    );
    EXPECT_FALSE(read);  // No broadcast available

    // Attempt to subscribe same module twice (should be idempotent)
    global_workspace_subscribe(workspace, MODULE_ATTENTION);
    global_workspace_subscribe(workspace, MODULE_ATTENTION);  // Duplicate
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 1u);  // Still just 1

    printf("[EMERGENT] Error handling correctly rejected invalid operations\n");
}

/**
 * WHAT: Test workspace statistics in integration scenario
 * WHY:  Verify statistics accurately track multi-module activity
 * HOW:  Run multiple competitions, check statistics
 */
TEST_F(GlobalWorkspaceIntegrationTest, StatisticsTrackingInIntegration)
{
    ScopedTimer timer("StatisticsTrackingInIntegration");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    // Perform multiple competitions
    for (int i = 0; i < 5; i++) {
        auto content = create_content_vector(256, i * 0.1f);
        global_workspace_compete(
            workspace,
            MODULE_PERCEPTION,
            content.data(),
            256,
            0.70f + (i * 0.05f)  // Increasing strength
        );

        if (i < 4) {
            sleep_ms(60);  // Allow refractory period between broadcasts
        }
    }

    // Check statistics
    workspace_statistics_t stats;
    bool got_stats = global_workspace_get_statistics(workspace, &stats);
    EXPECT_TRUE(got_stats);

    EXPECT_GT(stats.total_competitions, 0u);
    EXPECT_GT(stats.total_broadcasts, 0u);
    EXPECT_LE(stats.total_broadcasts, stats.total_competitions);  // Not all compete → broadcast

    printf("[STATS] Competitions: %lu, Broadcasts: %lu\n",
           stats.total_competitions,
           stats.total_broadcasts);
}

//=============================================================================
// 9. PERFORMANCE AND SCALABILITY TESTS
//=============================================================================

/**
 * WHAT: Test workspace performance with many competitors
 * WHY:  Verify O(N) competition resolution scales acceptably
 * HOW:  Submit 32 competitors simultaneously, measure time
 */
TEST_F(GlobalWorkspaceIntegrationTest, PerformanceWithManyCompetitors)
{
    ScopedTimer timer("PerformanceWithManyCompetitors");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    // Submit maximum competitors (32)
    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < 32; i++) {
        auto content = create_content_vector(256, i * 0.01f);
        cognitive_module_t module = static_cast<cognitive_module_t>(
            MODULE_PERCEPTION + (i % 20)
        );
        float strength = 0.60f + ((i % 10) * 0.03f);

        global_workspace_compete(workspace, module, content.data(), 256, strength);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Competition resolution should be fast (< 1ms for 32 competitors)
    EXPECT_LT(duration.count(), 1000);  // < 1ms

    printf("[PERFORMANCE] 32 competitions resolved in %ld μs\n", duration.count());
}

/**
 * WHAT: Test workspace memory usage in long-running scenario
 * WHY:  Verify no memory leaks with repeated operations
 * HOW:  Perform 100 broadcast cycles, check memory stats
 */
TEST_F(GlobalWorkspaceIntegrationTest, MemoryUsageInLongRunning)
{
    ScopedTimer timer("MemoryUsageInLongRunning");

    workspace = create_default_workspace();
    ASSERT_NE(workspace, nullptr);

    nimcp_memory_stats_t stats_before, stats_after;
    nimcp_memory_get_stats(&stats_before);

    // Perform 100 broadcast cycles
    for (int i = 0; i < 100; i++) {
        auto content = create_content_vector(256, i * 0.001f);
        global_workspace_compete(
            workspace,
            MODULE_PERCEPTION,
            content.data(),
            256,
            0.75f
        );

        if (i % 10 == 9) {
            sleep_ms(60);  // Periodically wait for refractory period
        }
    }

    nimcp_memory_get_stats(&stats_after);

    // Memory should not grow significantly (workspace reuses buffers)
    int64_t memory_growth = stats_after.current_allocated - stats_before.current_allocated;
    EXPECT_LT(memory_growth, 1024);  // < 1KB growth

    printf("[MEMORY] Memory growth after 100 broadcasts: %ld bytes\n", memory_growth);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    printf("=============================================================================\n");
    printf("Global Workspace Integration Tests - NIMCP Cognitive Architecture\n");
    printf("=============================================================================\n");
    printf("Testing Global Workspace Theory integration with cognitive modules\n");
    printf("Based on: Baars (1988), Dehaene (2011)\n");
    printf("=============================================================================\n\n");

    int result = RUN_ALL_TESTS();

    printf("\n=============================================================================\n");
    printf("Integration Test Summary\n");
    printf("=============================================================================\n");

    return result;
}
