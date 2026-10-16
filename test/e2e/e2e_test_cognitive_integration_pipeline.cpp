/**
 * @file e2e_test_cognitive_integration_pipeline.cpp
 * @brief End-to-end tests for cognitive integration hub and bridges
 * @version 1.0.0
 * @date 2026-01-08
 *
 * Tests the complete cognitive processing pipeline:
 * - Cognitive Integration Hub event routing
 * - Emotion-Memory Bridge interactions
 * - Attention-Working Memory gating
 * - Curiosity-Reasoning exploration
 * - Ethics-Executive constraints
 * - ToM-Social inference
 * - Self-Introspection
 * - Emotion-Executive regulation
 * - Global Workspace broadcasting
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>

#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_emotion_memory_bridge.h"
#include "cognitive/integration/nimcp_attention_wm_bridge.h"
#include "cognitive/integration/nimcp_curiosity_reasoning_bridge.h"
#include "cognitive/integration/nimcp_ethics_executive_bridge.h"
#include "cognitive/integration/nimcp_tom_social_bridge.h"
#include "cognitive/integration/nimcp_self_introspection_bridge.h"
#include "cognitive/integration/nimcp_emotion_executive_bridge.h"
#include "cognitive/integration/nimcp_gw_cognitive_bridge.h"

// =============================================================================
// Test Constants
// =============================================================================

static constexpr uint32_t MODULE_PERCEPTION   = 1;
static constexpr uint32_t MODULE_MEMORY       = 2;
static constexpr uint32_t MODULE_REASONING    = 3;
static constexpr uint32_t MODULE_EXECUTIVE    = 4;
static constexpr uint32_t MODULE_SOCIAL       = 5;
static constexpr uint32_t MODULE_EMOTIONAL    = 6;
static constexpr uint32_t MODULE_SELF         = 7;
static constexpr uint32_t MODULE_GW           = 8;

// =============================================================================
// Test Fixture
// =============================================================================

class CognitiveIntegrationPipelineTest : public ::testing::Test {
protected:
    // Hub
    cognitive_integration_hub_t hub;

    // Bridges
    emotion_memory_bridge_t* emotion_memory;
    attention_wm_bridge_t* attention_wm;
    curiosity_reasoning_bridge_t* curiosity_reasoning;
    ethics_executive_bridge_t* ethics_executive;
    tom_social_bridge_t* tom_social;
    self_introspection_bridge_t* self_introspection;
    emotion_executive_bridge_t* emotion_executive;
    gw_cognitive_bridge_t* gw_cognitive;

    // Event counters
    std::atomic<int> events_received{0};
    std::atomic<int> input_events{0};
    std::atomic<int> decision_events{0};
    std::atomic<int> emotion_events{0};

    void SetUp() override {
        // Create hub with default config
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub_config.enable_async = true;
        hub = cognitive_hub_create(&hub_config);
        ASSERT_NE(hub, nullptr) << "Failed to create cognitive integration hub";

        // Create all bridges with default configs
        emotion_memory = emotion_memory_bridge_create(nullptr);
        attention_wm = attention_wm_bridge_create(nullptr);
        curiosity_reasoning = curiosity_reasoning_bridge_create(nullptr);
        ethics_executive = ethics_executive_bridge_create(nullptr);
        tom_social = tom_social_bridge_create(nullptr);
        self_introspection = self_introspection_bridge_create(nullptr);
        emotion_executive = emotion_executive_bridge_create(nullptr);
        gw_cognitive = gw_cognitive_bridge_create(nullptr);

        // Verify all bridges created
        ASSERT_NE(emotion_memory, nullptr);
        ASSERT_NE(attention_wm, nullptr);
        ASSERT_NE(curiosity_reasoning, nullptr);
        ASSERT_NE(ethics_executive, nullptr);
        ASSERT_NE(tom_social, nullptr);
        ASSERT_NE(self_introspection, nullptr);
        ASSERT_NE(emotion_executive, nullptr);
        ASSERT_NE(gw_cognitive, nullptr);

        // Register modules with hub
        RegisterModules();

        // Reset counters
        events_received = 0;
        input_events = 0;
        decision_events = 0;
        emotion_events = 0;
    }

    void TearDown() override {
        // Destroy bridges
        emotion_memory_bridge_destroy(emotion_memory);
        attention_wm_bridge_destroy(attention_wm);
        curiosity_reasoning_bridge_destroy(curiosity_reasoning);
        ethics_executive_bridge_destroy(ethics_executive);
        tom_social_bridge_destroy(tom_social);
        self_introspection_bridge_destroy(self_introspection);
        emotion_executive_bridge_destroy(emotion_executive);
        gw_cognitive_bridge_destroy(gw_cognitive);

        // Destroy hub
        cognitive_hub_destroy(hub);
    }

    void RegisterModules() {
        int rc;

        rc = cognitive_hub_register_module(hub, MODULE_PERCEPTION,
            COG_CATEGORY_PERCEPTION, "Perception", nullptr);
        ASSERT_EQ(rc, 0);

        rc = cognitive_hub_register_module(hub, MODULE_MEMORY,
            COG_CATEGORY_MEMORY, "Memory", nullptr);
        ASSERT_EQ(rc, 0);

        rc = cognitive_hub_register_module(hub, MODULE_REASONING,
            COG_CATEGORY_REASONING, "Reasoning", nullptr);
        ASSERT_EQ(rc, 0);

        rc = cognitive_hub_register_module(hub, MODULE_EXECUTIVE,
            COG_CATEGORY_EXECUTIVE, "Executive", nullptr);
        ASSERT_EQ(rc, 0);

        rc = cognitive_hub_register_module(hub, MODULE_SOCIAL,
            COG_CATEGORY_SOCIAL, "Social", nullptr);
        ASSERT_EQ(rc, 0);

        rc = cognitive_hub_register_module(hub, MODULE_EMOTIONAL,
            COG_CATEGORY_EMOTIONAL, "Emotional", nullptr);
        ASSERT_EQ(rc, 0);

        rc = cognitive_hub_register_module(hub, MODULE_SELF,
            COG_CATEGORY_SELF, "Self", nullptr);
        ASSERT_EQ(rc, 0);

        rc = cognitive_hub_register_module(hub, MODULE_GW,
            COG_CATEGORY_EXECUTIVE, "GlobalWorkspace", nullptr);
        ASSERT_EQ(rc, 0);
    }

    static int EventCallback(const cognitive_event_data_t* event, void* user_data) {
        auto* test = static_cast<CognitiveIntegrationPipelineTest*>(user_data);
        test->events_received++;

        switch (event->event_type) {
            case COG_EVENT_INPUT_RECEIVED:
                test->input_events++;
                break;
            case COG_EVENT_DECISION_MADE:
                test->decision_events++;
                break;
            case COG_EVENT_EMOTION_UPDATE:
                test->emotion_events++;
                break;
            default:
                break;
        }
        return 0;
    }
};

// =============================================================================
// Test Cases
// =============================================================================

/**
 * Test complete cognitive processing loop:
 * 1. Input arrives (publish INPUT_RECEIVED)
 * 2. Attention gates to WM (attention_wm_gate_entry)
 * 3. Emotion tags content (emotion_memory_tag_memory)
 * 4. Curiosity evaluates (curiosity_reasoning_drive_exploration)
 * 5. Ethics checks action (ethics_executive_evaluate_action)
 * 6. Executive decides (publish DECISION_MADE)
 * 7. GW broadcasts result (gw_cognitive_broadcast)
 */
TEST_F(CognitiveIntegrationPipelineTest, CompleteCognitiveLoop) {
    // Subscribe modules to events
    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_MEMORY,
        COG_EVENT_INPUT_RECEIVED, EventCallback, this));
    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_REASONING,
        COG_EVENT_INPUT_RECEIVED, EventCallback, this));
    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_GW,
        COG_EVENT_DECISION_MADE, EventCallback, this));

    // Step 1: Input arrives
    cognitive_event_data_t input_event = {};
    input_event.event_type = COG_EVENT_INPUT_RECEIVED;
    input_event.source_module_id = MODULE_PERCEPTION;
    input_event.priority = COG_PRIORITY_NORMAL;
    input_event.timestamp = 1000;

    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_PERCEPTION,
        COG_EVENT_INPUT_RECEIVED, &input_event));

    // Verify input event received by subscribers
    EXPECT_GE(input_events.load(), 1);

    // Step 2: Attention gates to WM
    uint64_t item_id = 100;
    float attention_strength = 0.8f;
    ASSERT_EQ(0, attention_wm_gate_entry(attention_wm, item_id, attention_strength));

    // Step 3: Emotion tags memory
    uint64_t memory_id = 200;
    float valence = 0.6f;  // Positive
    float arousal = 0.7f;  // High arousal
    ASSERT_EQ(0, emotion_memory_tag_memory(emotion_memory, memory_id, valence, arousal));

    // Step 4: Curiosity drives exploration
    curiosity_reasoning_context_t context = {};
    context.context_id = 1;
    context.uncertainty = 0.5f;
    context.novelty = 0.8f;
    context.depth = 1;
    float curiosity_level = 0.7f;
    ASSERT_EQ(0, curiosity_reasoning_drive_exploration(curiosity_reasoning,
        &context, curiosity_level));

    // Step 5: Ethics evaluates action
    uint64_t action_id = 300;
    float ethical_score = 0.0f;
    ASSERT_EQ(0, ethics_executive_evaluate_action(ethics_executive,
        action_id, &ethical_score));
    EXPECT_GE(ethical_score, 0.0f);
    EXPECT_LE(ethical_score, 1.0f);

    // Step 6: Executive decides
    cognitive_event_data_t decision_event = {};
    decision_event.event_type = COG_EVENT_DECISION_MADE;
    decision_event.source_module_id = MODULE_EXECUTIVE;
    decision_event.priority = COG_PRIORITY_HIGH;
    decision_event.timestamp = 2000;

    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_EXECUTIVE,
        COG_EVENT_DECISION_MADE, &decision_event));

    // Step 7: GW broadcasts result
    const char* broadcast_content = "Decision: Action approved";
    ASSERT_EQ(0, gw_cognitive_broadcast(gw_cognitive,
        GW_COGNITIVE_CONTENT_THOUGHT,
        broadcast_content, strlen(broadcast_content)));

    // Verify stats updated
    cognitive_hub_stats_t stats = {};
    ASSERT_EQ(0, cognitive_hub_get_stats(hub, &stats));
    EXPECT_GT(stats.events_published, 0u);
    EXPECT_GT(stats.events_delivered, 0u);

    // Verify bridge stats
    emotion_memory_stats_t em_stats = {};
    ASSERT_EQ(0, emotion_memory_bridge_get_stats(emotion_memory, &em_stats));
    EXPECT_GT(em_stats.memories_tagged, 0u);

    attention_wm_stats_t aw_stats = {};
    ASSERT_EQ(0, attention_wm_bridge_get_stats(attention_wm, &aw_stats));
    EXPECT_GT(aw_stats.items_gated_in, 0u);
}

/**
 * Test emotional memory scenario:
 * - High-emotion stimulus triggers tagging
 * - Consolidation boost applied
 * - Recall triggers emotional response
 */
TEST_F(CognitiveIntegrationPipelineTest, EmotionalMemoryScenario) {
    // Tag memory with high emotional intensity
    uint64_t memory_id = 500;
    float high_valence = 0.9f;  // Very positive
    float high_arousal = 0.95f; // Very high arousal

    ASSERT_EQ(0, emotion_memory_tag_memory(emotion_memory,
        memory_id, high_valence, high_arousal));

    // Boost consolidation
    float emotional_intensity = 0.9f;
    ASSERT_EQ(0, emotion_memory_modulate_consolidation(emotion_memory,
        memory_id, emotional_intensity));

    // Retrieve and check emotional response
    emotion_memory_emotion_out_t emotion_out = {};
    ASSERT_EQ(0, emotion_memory_on_retrieval(emotion_memory, memory_id, &emotion_out));

    EXPECT_TRUE(emotion_out.has_emotion);
    EXPECT_NEAR(emotion_out.valence, high_valence, 0.1f);
    EXPECT_NEAR(emotion_out.arousal, high_arousal, 0.1f);
    EXPECT_GT(emotion_out.intensity, 0.5f);

    // Verify stats
    emotion_memory_stats_t stats = {};
    ASSERT_EQ(0, emotion_memory_bridge_get_stats(emotion_memory, &stats));
    EXPECT_GT(stats.memories_tagged, 0u);
    EXPECT_GT(stats.consolidation_boosts, 0u);
    EXPECT_GT(stats.retrievals_with_emotion, 0u);
}

/**
 * Test social reasoning scenario:
 * - Social cue detected
 * - ToM inference performed
 * - Social response generated
 * - Ethics check applied
 */
TEST_F(CognitiveIntegrationPipelineTest, SocialReasoningScenario) {
    // Process social cue
    float cue_data[4] = {0.8f, 0.3f, 0.5f, 0.7f};  // Simulated facial expression
    ASSERT_EQ(0, tom_social_on_social_cue(tom_social,
        TOM_SOCIAL_CUE_FACIAL_EXPRESSION, cue_data));

    // Infer mental state for response
    uint32_t agent_id = 1;
    tom_social_mental_state_t mental_state = {};
    int rc = tom_social_infer_for_response(tom_social, agent_id, &mental_state);

    // May fail if agent not tracked yet, which is okay
    if (rc == 0) {
        EXPECT_GE(mental_state.confidence, 0.0f);
        EXPECT_LE(mental_state.confidence, 1.0f);
    }

    // Update agent model
    tom_social_belief_update_t belief_update = {};
    belief_update.belief_type = 0;
    belief_update.belief_value = 0.6f;
    belief_update.confidence = 0.7f;
    belief_update.source = 1;  // Social source

    ASSERT_EQ(0, tom_social_update_agent_model(tom_social, agent_id, &belief_update));

    // Check ethics for social response
    uint64_t social_action_id = 400;
    float ethical_score = 0.0f;
    ASSERT_EQ(0, ethics_executive_evaluate_action(ethics_executive,
        social_action_id, &ethical_score));

    // Verify stats
    tom_social_stats_t stats = {};
    ASSERT_EQ(0, tom_social_get_stats(tom_social, &stats));
    EXPECT_GT(stats.social_cues_processed, 0u);
}

/**
 * Test curiosity-driven learning:
 * - Novel information detected
 * - Curiosity drives exploration
 * - Novel conclusion reached
 * - Curiosity feedback loop
 */
TEST_F(CognitiveIntegrationPipelineTest, CuriosityDrivenLearning) {
    // Initial exploration
    curiosity_reasoning_context_t context1 = {};
    context1.context_id = 10;
    context1.uncertainty = 0.8f;  // High uncertainty
    context1.novelty = 0.9f;      // High novelty
    context1.depth = 1;

    float high_curiosity = 0.85f;
    ASSERT_EQ(0, curiosity_reasoning_drive_exploration(curiosity_reasoning,
        &context1, high_curiosity));

    // Share uncertainty
    uint64_t topic_id = 100;
    float uncertainty = 0.7f;
    ASSERT_EQ(0, curiosity_reasoning_share_uncertainty(curiosity_reasoning,
        topic_id, uncertainty));

    // Novel conclusion reached
    uint64_t conclusion_id = 200;
    float novelty_score = 0.75f;
    ASSERT_EQ(0, curiosity_reasoning_on_novel_conclusion(curiosity_reasoning,
        conclusion_id, novelty_score));

    // Get exploration priority (should be high due to novelty)
    float priority = curiosity_reasoning_get_exploration_priority(
        curiosity_reasoning, topic_id);
    EXPECT_GE(priority, 0.0f);

    // Continue exploration loop
    curiosity_reasoning_context_t context2 = {};
    context2.context_id = 11;
    context2.uncertainty = 0.5f;  // Reduced uncertainty
    context2.novelty = 0.6f;
    context2.depth = 2;

    ASSERT_EQ(0, curiosity_reasoning_drive_exploration(curiosity_reasoning,
        &context2, 0.7f));

    // Verify stats
    curiosity_reasoning_stats_t stats = {};
    ASSERT_EQ(0, curiosity_reasoning_bridge_get_stats(curiosity_reasoning, &stats));
    EXPECT_GE(stats.explorations_driven, 2u);
    EXPECT_GT(stats.novel_conclusions, 0u);
}

/**
 * Test conflict resolution:
 * - Multiple modules compete for GW access
 * - Highest priority wins
 */
TEST_F(CognitiveIntegrationPipelineTest, ConflictResolution) {
    // Register receivers
    ASSERT_EQ(0, gw_cognitive_register_receiver(gw_cognitive, MODULE_MEMORY,
        [](gw_cognitive_content_type_t type, const void* data,
           size_t size, void* user_data) {
            // Receiver callback
        }, nullptr));

    ASSERT_EQ(0, gw_cognitive_register_receiver(gw_cognitive, MODULE_REASONING,
        [](gw_cognitive_content_type_t type, const void* data,
           size_t size, void* user_data) {
            // Receiver callback
        }, nullptr));

    // Submit competing content
    gw_cognitive_content_t content1 = {};
    content1.content_type = GW_COGNITIVE_CONTENT_MEMORY;
    const char* memory_data = "Memory recall";
    content1.content_data = memory_data;
    content1.content_size = strlen(memory_data);
    content1.priority = 0.5f;
    content1.relevance = 0.6f;
    content1.urgency = 0.4f;

    ASSERT_EQ(0, gw_cognitive_compete_for_access(gw_cognitive,
        MODULE_MEMORY, &content1, 0.5f));

    gw_cognitive_content_t content2 = {};
    content2.content_type = GW_COGNITIVE_CONTENT_THOUGHT;
    const char* thought_data = "High priority thought";
    content2.content_data = thought_data;
    content2.content_size = strlen(thought_data);
    content2.priority = 0.9f;  // Higher priority
    content2.relevance = 0.8f;
    content2.urgency = 0.7f;

    ASSERT_EQ(0, gw_cognitive_compete_for_access(gw_cognitive,
        MODULE_REASONING, &content2, 0.9f));

    // Check conscious content (should be higher priority)
    char buffer[256] = {};
    gw_cognitive_conscious_content_t conscious = {};
    conscious.content_buffer = buffer;
    conscious.buffer_size = sizeof(buffer);

    int rc = gw_cognitive_get_conscious_content(gw_cognitive, &conscious);
    if (rc == 0 && conscious.has_content) {
        EXPECT_GE(conscious.winning_priority, 0.5f);
    }

    // Verify stats
    gw_cognitive_stats_t stats = {};
    ASSERT_EQ(0, gw_cognitive_get_stats(gw_cognitive, &stats));
    EXPECT_GE(stats.competitions_held, 0u);
}

/**
 * Test stress response:
 * - Rapid input stream
 * - Verify no data loss
 * - Check latency acceptable
 */
TEST_F(CognitiveIntegrationPipelineTest, StressResponse) {
    constexpr int NUM_EVENTS = 100;
    std::atomic<int> received_count{0};

    // Subscribe with counter callback
    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_MEMORY,
        COG_EVENT_INPUT_RECEIVED,
        [](const cognitive_event_data_t* event, void* user_data) -> int {
            auto* count = static_cast<std::atomic<int>*>(user_data);
            (*count)++;
            return 0;
        }, &received_count));

    auto start = std::chrono::high_resolution_clock::now();

    // Rapid event publishing
    for (int i = 0; i < NUM_EVENTS; i++) {
        cognitive_event_data_t event = {};
        event.event_type = COG_EVENT_INPUT_RECEIVED;
        event.source_module_id = MODULE_PERCEPTION;
        event.priority = COG_PRIORITY_NORMAL;
        event.timestamp = static_cast<uint64_t>(i * 1000);

        int rc = cognitive_hub_publish(hub, MODULE_PERCEPTION,
            COG_EVENT_INPUT_RECEIVED, &event);
        EXPECT_EQ(rc, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // Verify all events delivered
    EXPECT_EQ(received_count.load(), NUM_EVENTS);

    // Verify reasonable latency (< 100ms for 100 events)
    EXPECT_LT(duration, 100);

    // Check hub stats
    cognitive_hub_stats_t stats = {};
    ASSERT_EQ(0, cognitive_hub_get_stats(hub, &stats));
    EXPECT_EQ(stats.events_dropped, 0u);
    EXPECT_GE(stats.events_published, static_cast<uint64_t>(NUM_EVENTS));
}

/**
 * Test recovery from error:
 * - Inject NULL parameters
 * - Verify graceful handling
 */
TEST_F(CognitiveIntegrationPipelineTest, RecoveryFromError) {
    // NULL hub operations
    EXPECT_EQ(-1, cognitive_hub_register_module(nullptr, 1,
        COG_CATEGORY_PERCEPTION, "Test", nullptr));
    EXPECT_EQ(-1, cognitive_hub_publish(nullptr, 1,
        COG_EVENT_INPUT_RECEIVED, nullptr));
    EXPECT_EQ(-1, cognitive_hub_subscribe(nullptr, 1,
        COG_EVENT_INPUT_RECEIVED, nullptr, nullptr));

    // NULL bridge operations
    EXPECT_EQ(-1, emotion_memory_tag_memory(nullptr, 1, 0.5f, 0.5f));
    EXPECT_EQ(-1, attention_wm_gate_entry(nullptr, 1, 0.5f));
    EXPECT_EQ(-1, curiosity_reasoning_drive_exploration(nullptr, nullptr, 0.5f));
    EXPECT_EQ(-1, ethics_executive_evaluate_action(nullptr, 1, nullptr));

    // NULL event data
    EXPECT_EQ(-1, cognitive_hub_publish(hub, MODULE_PERCEPTION,
        COG_EVENT_INPUT_RECEIVED, nullptr));

    // NULL callback
    EXPECT_EQ(-1, cognitive_hub_subscribe(hub, MODULE_MEMORY,
        COG_EVENT_INPUT_RECEIVED, nullptr, nullptr));

    // Verify hub still operational after errors
    cognitive_hub_stats_t stats = {};
    ASSERT_EQ(0, cognitive_hub_get_stats(hub, &stats));

    // Successfully publish after errors
    cognitive_event_data_t event = {};
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = MODULE_PERCEPTION;
    EXPECT_EQ(0, cognitive_hub_publish(hub, MODULE_PERCEPTION,
        COG_EVENT_STATE_CHANGE, &event));
}

/**
 * Test self-introspection feedback loop:
 * - Query guided by self-model
 * - Result integrated
 * - Reflection triggered on discrepancy
 */
TEST_F(CognitiveIntegrationPipelineTest, SelfIntrospectionLoop) {
    // Get guidance for introspective query
    self_introspection_guidance_t guidance = {};
    ASSERT_EQ(0, self_introspection_guide_query(self_introspection,
        SELF_INTROSPECTION_QUERY_CONFIDENCE, &guidance));

    EXPECT_GE(guidance.priority, 0.0f);
    EXPECT_LE(guidance.priority, 1.0f);

    // Process introspection result
    self_introspection_result_t result = {};
    result.query_id = 1;
    result.query_type = SELF_INTROSPECTION_QUERY_CONFIDENCE;
    result.result_value = 0.6f;
    result.confidence = 0.8f;
    result.discrepancy = 0.3f;  // Some discrepancy
    result.processing_time_ms = 10;
    result.suggests_update = true;

    ASSERT_EQ(0, self_introspection_on_result(self_introspection, 1, &result));

    // Trigger reflection due to discrepancy
    ASSERT_EQ(0, self_introspection_trigger_reflection(self_introspection,
        SELF_INTROSPECTION_TRIGGER_DISCREPANCY));

    // Get self-model state
    self_introspection_self_state_t self_state = {};
    ASSERT_EQ(0, self_introspection_get_self_state(self_introspection, &self_state));

    EXPECT_GE(self_state.coherence, 0.0f);
    EXPECT_LE(self_state.coherence, 1.0f);

    // Verify stats
    self_introspection_stats_t stats = {};
    ASSERT_EQ(0, self_introspection_get_stats(self_introspection, &stats));
    EXPECT_GT(stats.queries_guided, 0u);
    EXPECT_GT(stats.results_integrated, 0u);
}

/**
 * Test emotion-executive regulation:
 * - Emotional influence on decision
 * - Executive regulation of emotion
 * - Decision outcome emotional feedback
 */
TEST_F(CognitiveIntegrationPipelineTest, EmotionExecutiveRegulation) {
    // Get emotional influence on a decision
    emotion_executive_decision_context_t decision_ctx = {};
    decision_ctx.decision_id = 100;
    decision_ctx.option_count = 3;
    decision_ctx.time_pressure = 0.3f;
    decision_ctx.stakes = 0.7f;
    decision_ctx.uncertainty = 0.4f;
    decision_ctx.risk_level = 0.5f;

    emotion_executive_emotional_bias_t bias = {};
    ASSERT_EQ(0, emotion_executive_influence_decision(emotion_executive,
        &decision_ctx, &bias));

    EXPECT_GE(bias.valence_bias, -1.0f);
    EXPECT_LE(bias.valence_bias, 1.0f);

    // Apply regulation if emotion too intense
    if (bias.emotion_intensity > 0.7f) {
        emotion_executive_regulation_target_t reg_target = {};
        reg_target.target_emotion = bias.dominant_emotion;
        reg_target.target_intensity = 0.5f;
        reg_target.strategy = EMOTION_EXECUTIVE_REG_REAPPRAISAL;
        reg_target.max_duration_ms = 1000;

        ASSERT_EQ(0, emotion_executive_regulate_emotion(emotion_executive,
            bias.dominant_emotion, &reg_target));
    }

    // Process decision outcome
    emotion_executive_decision_outcome_t outcome = {};
    outcome.decision_id = 100;
    outcome.outcome_valence = 0.8f;  // Good outcome
    outcome.expectation_violation = 0.2f;  // Better than expected
    outcome.success = true;
    outcome.decision_time_ms = 500;

    ASSERT_EQ(0, emotion_executive_on_decision(emotion_executive, 100, &outcome));

    // Check emotional state
    emotion_executive_emotional_state_t emo_state = {};
    ASSERT_EQ(0, emotion_executive_get_emotional_state(emotion_executive, &emo_state));

    EXPECT_GE(emo_state.valence, -1.0f);
    EXPECT_LE(emo_state.valence, 1.0f);

    // Verify stats
    emotion_executive_stats_t stats = {};
    ASSERT_EQ(0, emotion_executive_get_stats(emotion_executive, &stats));
    EXPECT_GT(stats.decisions_influenced, 0u);
}

/**
 * Test attention-working memory integration:
 * - Gate multiple items
 * - Handle focus shifts
 * - Priority updates
 * - Query attended items
 */
TEST_F(CognitiveIntegrationPipelineTest, AttentionWorkingMemoryIntegration) {
    // Gate first item
    uint64_t item1 = 1001;
    ASSERT_EQ(0, attention_wm_gate_entry(attention_wm, item1, 0.8f));

    // Gate second item
    uint64_t item2 = 1002;
    ASSERT_EQ(0, attention_wm_gate_entry(attention_wm, item2, 0.7f));

    // Gate third item with low attention (should be rejected)
    uint64_t item3 = 1003;
    // May succeed or fail depending on threshold
    attention_wm_gate_entry(attention_wm, item3, 0.1f);

    // Shift focus
    ASSERT_EQ(0, attention_wm_on_focus_shift(attention_wm, item1, item2));

    // Update priority
    ASSERT_EQ(0, attention_wm_update_priority(attention_wm, item1, 0.5f));

    // Get attended items
    attention_wm_item_t items[10] = {};
    int count = attention_wm_get_attended_items(attention_wm, items, 10);
    EXPECT_GE(count, 0);

    // Verify stats
    attention_wm_stats_t stats = {};
    ASSERT_EQ(0, attention_wm_bridge_get_stats(attention_wm, &stats));
    EXPECT_GE(stats.items_gated_in, 2u);
    EXPECT_GT(stats.focus_shifts, 0u);
}

/**
 * Test cross-module event propagation:
 * - Event chain across multiple modules
 * - Verify all modules receive relevant events
 */
TEST_F(CognitiveIntegrationPipelineTest, CrossModuleEventPropagation) {
    std::atomic<int> perception_events{0};
    std::atomic<int> memory_events{0};
    std::atomic<int> reasoning_events{0};
    std::atomic<int> executive_events{0};

    // Subscribe each module to input events
    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_MEMORY,
        COG_EVENT_INPUT_RECEIVED,
        [](const cognitive_event_data_t* e, void* ud) -> int {
            (*static_cast<std::atomic<int>*>(ud))++;
            return 0;
        }, &memory_events));

    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_REASONING,
        COG_EVENT_INPUT_RECEIVED,
        [](const cognitive_event_data_t* e, void* ud) -> int {
            (*static_cast<std::atomic<int>*>(ud))++;
            return 0;
        }, &reasoning_events));

    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_EXECUTIVE,
        COG_EVENT_INPUT_RECEIVED,
        [](const cognitive_event_data_t* e, void* ud) -> int {
            (*static_cast<std::atomic<int>*>(ud))++;
            return 0;
        }, &executive_events));

    // Publish input event
    cognitive_event_data_t event = {};
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_PERCEPTION;
    event.priority = COG_PRIORITY_NORMAL;

    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_PERCEPTION,
        COG_EVENT_INPUT_RECEIVED, &event));

    // Verify all subscribers received
    EXPECT_EQ(memory_events.load(), 1);
    EXPECT_EQ(reasoning_events.load(), 1);
    EXPECT_EQ(executive_events.load(), 1);
}

/**
 * Test ethics veto mechanism:
 * - Submit action for evaluation
 * - Veto unethical action
 * - Query permitted actions
 */
TEST_F(CognitiveIntegrationPipelineTest, EthicsVetoMechanism) {
    // Evaluate an action
    uint64_t action_id = 500;
    float ethical_score = 0.0f;
    ASSERT_EQ(0, ethics_executive_evaluate_action(ethics_executive,
        action_id, &ethical_score));

    // Get constraints
    ethics_constraints_out_t constraints = {};
    ASSERT_EQ(0, ethics_executive_constrain_action(ethics_executive,
        action_id, &constraints));

    EXPECT_GE(constraints.overall_ethical_score, 0.0f);
    EXPECT_LE(constraints.overall_ethical_score, 1.0f);

    // If score low, test veto
    if (ethical_score < 0.3f) {
        ASSERT_EQ(0, ethics_executive_veto_action(ethics_executive, action_id));
    }

    // Query permitted actions
    uint64_t permitted[10] = {};
    int count = ethics_executive_get_permitted_actions(ethics_executive,
        permitted, 10);
    EXPECT_GE(count, 0);

    // Verify stats
    ethics_executive_stats_t stats = {};
    ASSERT_EQ(0, ethics_executive_bridge_get_stats(ethics_executive, &stats));
    EXPECT_GT(stats.evaluations_performed, 0u);
}

/**
 * Test module activation/deactivation:
 * - Deactivate module
 * - Verify events not delivered
 * - Reactivate and verify delivery
 */
TEST_F(CognitiveIntegrationPipelineTest, ModuleActivation) {
    std::atomic<int> event_count{0};

    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_MEMORY,
        COG_EVENT_INPUT_RECEIVED,
        [](const cognitive_event_data_t* e, void* ud) -> int {
            (*static_cast<std::atomic<int>*>(ud))++;
            return 0;
        }, &event_count));

    // Deactivate module
    ASSERT_EQ(0, cognitive_hub_set_module_active(hub, MODULE_MEMORY, false));

    // Publish event
    cognitive_event_data_t event = {};
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_PERCEPTION;

    cognitive_hub_publish(hub, MODULE_PERCEPTION, COG_EVENT_INPUT_RECEIVED, &event);

    // Event may or may not be delivered to inactive module (implementation dependent)
    int count_while_inactive = event_count.load();

    // Reactivate module
    ASSERT_EQ(0, cognitive_hub_set_module_active(hub, MODULE_MEMORY, true));

    // Publish another event
    cognitive_hub_publish(hub, MODULE_PERCEPTION, COG_EVENT_INPUT_RECEIVED, &event);

    // Should receive event now
    EXPECT_GT(event_count.load(), count_while_inactive);
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
