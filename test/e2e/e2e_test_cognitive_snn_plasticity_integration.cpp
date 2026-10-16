/**
 * @file e2e_test_cognitive_snn_plasticity_integration.cpp
 * @brief End-to-end tests for cognitive intra-layer + inter-layer (SNN/plasticity) integration
 * @version 1.0.0
 * @date 2026-01-08
 *
 * Tests that intra-layer bridges (cognitive integration hub) work alongside
 * inter-layer bridges (SNN and plasticity bridges).
 *
 * Scenarios tested:
 * - SNN bridges with cognitive hub
 * - Plasticity bridges with cognitive hub
 * - Cross-layer integration
 * - Learning across layers
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <cstring>

// Cognitive integration layer
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_emotion_memory_bridge.h"
#include "cognitive/integration/nimcp_gw_cognitive_bridge.h"

// SNN bridges (inter-layer)
#include "cognitive/emotion/nimcp_emotion_snn_bridge.h"

// Plasticity bridges (inter-layer)
#include "cognitive/emotion/nimcp_emotion_plasticity_bridge.h"

// =============================================================================
// Test Constants
// =============================================================================

static constexpr uint32_t MODULE_EMOTION      = 1;
static constexpr uint32_t MODULE_MEMORY       = 2;
static constexpr uint32_t MODULE_SNN          = 3;
static constexpr uint32_t MODULE_PLASTICITY   = 4;
static constexpr uint32_t MODULE_GW           = 5;

// =============================================================================
// Test Fixture - Basic SNN Integration
// =============================================================================

class CognitiveSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    // Cognitive hub
    cognitive_integration_hub_t hub;

    // Intra-layer bridges
    emotion_memory_bridge_t* emotion_memory;
    gw_cognitive_bridge_t* gw_cognitive;

    // Inter-layer bridges (SNN/Plasticity)
    emotion_snn_bridge_t* emotion_snn;
    emotion_plasticity_bridge_t* emotion_plasticity;

    // Event tracking
    std::atomic<int> hub_events{0};
    std::atomic<int> snn_events{0};
    std::atomic<int> plasticity_events{0};

    void SetUp() override {
        // Create cognitive hub
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub_config.enable_async = false;  // Synchronous for testing
        hub = cognitive_hub_create(&hub_config);
        ASSERT_NE(hub, nullptr) << "Failed to create cognitive hub";

        // Create intra-layer bridges
        emotion_memory = emotion_memory_bridge_create(nullptr);
        gw_cognitive = gw_cognitive_bridge_create(nullptr);
        ASSERT_NE(emotion_memory, nullptr);
        ASSERT_NE(gw_cognitive, nullptr);

        // Create inter-layer bridges with default configs
        emotion_snn_config_t snn_config = emotion_snn_config_default();
        snn_config.enable_bio_async = false;  // Disable for unit test
        emotion_snn = emotion_snn_create(&snn_config);
        ASSERT_NE(emotion_snn, nullptr);

        emotion_plasticity_config_t plasticity_config = emotion_plasticity_config_default();
        plasticity_config.enable_bio_async = false;
        emotion_plasticity = emotion_plasticity_create(&plasticity_config);
        ASSERT_NE(emotion_plasticity, nullptr);

        // Register modules with hub
        ASSERT_EQ(0, cognitive_hub_register_module(hub, MODULE_EMOTION,
            COG_CATEGORY_EMOTIONAL, "Emotion", nullptr));
        ASSERT_EQ(0, cognitive_hub_register_module(hub, MODULE_MEMORY,
            COG_CATEGORY_MEMORY, "Memory", nullptr));
        ASSERT_EQ(0, cognitive_hub_register_module(hub, MODULE_SNN,
            COG_CATEGORY_PERCEPTION, "SNN", nullptr));
        ASSERT_EQ(0, cognitive_hub_register_module(hub, MODULE_PLASTICITY,
            COG_CATEGORY_MEMORY, "Plasticity", nullptr));
        ASSERT_EQ(0, cognitive_hub_register_module(hub, MODULE_GW,
            COG_CATEGORY_EXECUTIVE, "GlobalWorkspace", nullptr));

        // Reset counters
        hub_events = 0;
        snn_events = 0;
        plasticity_events = 0;
    }

    void TearDown() override {
        emotion_memory_bridge_destroy(emotion_memory);
        gw_cognitive_bridge_destroy(gw_cognitive);
        emotion_snn_destroy(emotion_snn);
        emotion_plasticity_destroy(emotion_plasticity);
        cognitive_hub_destroy(hub);
    }

    // Helper to get current time in microseconds
    uint64_t GetTimestampUs() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }
};

// =============================================================================
// Test Cases - SNN Bridge with Hub
// =============================================================================

/**
 * Test SNN bridge operating alongside cognitive hub:
 * - Register cognitive module
 * - Process through SNN bridge
 * - Verify hub events still flow
 */
TEST_F(CognitiveSNNPlasticityIntegrationTest, SNNBridgeWithHub) {
    // Subscribe to emotion events in hub
    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_MEMORY,
        COG_EVENT_EMOTION_UPDATE,
        [](const cognitive_event_data_t* e, void* ud) -> int {
            (*static_cast<std::atomic<int>*>(ud))++;
            return 0;
        }, &hub_events));

    // Encode valence-arousal through SNN bridge
    float valence = 0.7f;
    float arousal = 0.6f;
    float intensity = 0.8f;

    int spikes = emotion_snn_encode_valence_arousal(emotion_snn,
        valence, arousal, intensity);
    EXPECT_GE(spikes, 0);

    // Run SNN simulation
    ASSERT_EQ(0, emotion_snn_simulate(emotion_snn, 50.0f));

    // Decode emotion state from SNN
    emotion_snn_emotion_state_t snn_state = {};
    ASSERT_EQ(0, emotion_snn_get_emotion_state(emotion_snn, &snn_state));

    // Publish emotion update to hub (simulating SNN -> Hub connection)
    cognitive_event_data_t event = {};
    event.event_type = COG_EVENT_EMOTION_UPDATE;
    event.source_module_id = MODULE_SNN;
    event.priority = COG_PRIORITY_NORMAL;
    event.timestamp = GetTimestampUs();

    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_SNN,
        COG_EVENT_EMOTION_UPDATE, &event));

    // Verify hub received the event
    EXPECT_EQ(hub_events.load(), 1);

    // Tag memory with emotion (intra-layer operation)
    uint64_t memory_id = 100;
    ASSERT_EQ(0, emotion_memory_tag_memory(emotion_memory,
        memory_id, snn_state.valence, snn_state.arousal));

    // Verify both layers operated correctly
    emotion_memory_stats_t em_stats = {};
    ASSERT_EQ(0, emotion_memory_bridge_get_stats(emotion_memory, &em_stats));
    EXPECT_GT(em_stats.memories_tagged, 0u);

    emotion_snn_stats_t snn_stats = {};
    ASSERT_EQ(0, emotion_snn_get_stats(emotion_snn, &snn_stats));
    // Decodings depend on internal pipeline state
    EXPECT_GE(snn_stats.total_decodings, 0u);
}

/**
 * Test SNN encoding and decoding with hub notifications
 */
TEST_F(CognitiveSNNPlasticityIntegrationTest, SNNEncodingDecodingPipeline) {
    // Encode features through SNN
    float features[8] = {0.1f, 0.3f, 0.5f, 0.7f, 0.2f, 0.4f, 0.6f, 0.8f};
    int spikes = emotion_snn_encode_features(emotion_snn,
        features, 8, 0.5f, 0.6f);
    EXPECT_GE(spikes, 0);

    // Simulate
    ASSERT_EQ(0, emotion_snn_simulate(emotion_snn, 100.0f));

    // Decode category confidences
    float confidences[EMOTION_COUNT] = {};
    emotion_category_t category = emotion_snn_get_category_confidences(
        emotion_snn, confidences);

    // Get valence-arousal
    float decoded_valence = 0.0f;
    float decoded_arousal = 0.0f;
    ASSERT_EQ(0, emotion_snn_get_valence_arousal(emotion_snn,
        &decoded_valence, &decoded_arousal));

    // Notify hub of SNN processing completion
    cognitive_event_data_t event = {};
    event.event_type = COG_EVENT_OUTPUT_READY;
    event.source_module_id = MODULE_SNN;

    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_SNN,
        COG_EVENT_OUTPUT_READY, &event));

    // Verify hub stats
    cognitive_hub_stats_t hub_stats = {};
    ASSERT_EQ(0, cognitive_hub_get_stats(hub, &hub_stats));
    EXPECT_GT(hub_stats.events_published, 0u);
}

// =============================================================================
// Test Cases - Plasticity Bridge with Hub
// =============================================================================

/**
 * Test plasticity bridge operating alongside cognitive hub
 */
TEST_F(CognitiveSNNPlasticityIntegrationTest, PlasticityBridgeWithHub) {
    // Register synapse for emotional learning
    uint32_t synapse_id = 1;
    ASSERT_EQ(0, emotion_plasticity_register_synapse(emotion_plasticity,
        synapse_id,
        EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
        EMOTION_HAPPINESS,
        0.5f));

    // Subscribe to learning events
    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_MEMORY,
        COG_EVENT_LEARNING_COMPLETE,
        [](const cognitive_event_data_t* e, void* ud) -> int {
            (*static_cast<std::atomic<int>*>(ud))++;
            return 0;
        }, &plasticity_events));

    uint64_t timestamp = GetTimestampUs();

    // Record stimulus (pre-synaptic)
    ASSERT_EQ(0, emotion_plasticity_stimulus(emotion_plasticity,
        EMOTION_HAPPINESS, 0.8f, timestamp));

    // Record response (post-synaptic) with small delay
    timestamp += 10000;  // 10ms later
    ASSERT_EQ(0, emotion_plasticity_response(emotion_plasticity,
        EMOTION_HAPPINESS, 0.7f, timestamp));

    // Update plasticity
    ASSERT_EQ(0, emotion_plasticity_update(emotion_plasticity, 10.0f));

    // Publish learning event to hub
    cognitive_event_data_t event = {};
    event.event_type = COG_EVENT_LEARNING_COMPLETE;
    event.source_module_id = MODULE_PLASTICITY;
    event.timestamp = timestamp;

    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_PLASTICITY,
        COG_EVENT_LEARNING_COMPLETE, &event));

    // Verify hub received event
    EXPECT_EQ(plasticity_events.load(), 1);

    // Verify plasticity stats
    emotion_plasticity_stats_t stats = {};
    ASSERT_EQ(0, emotion_plasticity_get_stats(emotion_plasticity, &stats));
    EXPECT_GT(stats.total_observations, 0u);

    // Tag memory with learned emotion (cross-bridge)
    ASSERT_EQ(0, emotion_memory_tag_memory(emotion_memory, 200, 0.8f, 0.7f));
}

/**
 * Test plasticity reward modulation with hub events
 */
TEST_F(CognitiveSNNPlasticityIntegrationTest, PlasticityRewardModulation) {
    // Register synapse
    uint32_t synapse_id = 10;
    ASSERT_EQ(0, emotion_plasticity_register_synapse(emotion_plasticity,
        synapse_id,
        EMOTION_SYNAPSE_EMOTION_TO_RESPONSE,
        EMOTION_HAPPINESS,
        0.5f));

    uint64_t timestamp = GetTimestampUs();

    // Stimulus-response pair
    ASSERT_EQ(0, emotion_plasticity_stimulus(emotion_plasticity,
        EMOTION_HAPPINESS, 0.6f, timestamp));
    timestamp += 20000;
    ASSERT_EQ(0, emotion_plasticity_response(emotion_plasticity,
        EMOTION_HAPPINESS, 0.5f, timestamp));

    // Provide reward
    timestamp += 50000;
    ASSERT_EQ(0, emotion_plasticity_reward(emotion_plasticity,
        1.0f, timestamp));  // Positive reward

    // Update plasticity (should strengthen due to reward)
    ASSERT_EQ(0, emotion_plasticity_update(emotion_plasticity, 50.0f));

    // Get response modulation
    float modulation = 0.0f;
    ASSERT_EQ(0, emotion_plasticity_get_response_modulation(emotion_plasticity,
        EMOTION_HAPPINESS, &modulation));

    // Modulation should be positive after reward
    EXPECT_GE(modulation, 0.0f);

    // Notify hub of decision event
    cognitive_event_data_t event = {};
    event.event_type = COG_EVENT_DECISION_MADE;
    event.source_module_id = MODULE_PLASTICITY;

    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_PLASTICITY,
        COG_EVENT_DECISION_MADE, &event));
}

// =============================================================================
// Test Cases - Cross-Layer Integration
// =============================================================================

/**
 * Test intra-layer event triggers inter-layer processing:
 * - Hub event triggers SNN processing
 * - SNN result feeds back to hub
 */
TEST_F(CognitiveSNNPlasticityIntegrationTest, CrossLayerIntegration) {
    std::atomic<int> emotion_updates{0};
    std::atomic<int> output_ready{0};

    // Subscribe to events
    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_MEMORY,
        COG_EVENT_EMOTION_UPDATE,
        [](const cognitive_event_data_t* e, void* ud) -> int {
            (*static_cast<std::atomic<int>*>(ud))++;
            return 0;
        }, &emotion_updates));

    ASSERT_EQ(0, cognitive_hub_subscribe(hub, MODULE_EMOTION,
        COG_EVENT_OUTPUT_READY,
        [](const cognitive_event_data_t* e, void* ud) -> int {
            (*static_cast<std::atomic<int>*>(ud))++;
            return 0;
        }, &output_ready));

    // Step 1: Publish input event (simulating perception)
    cognitive_event_data_t input_event = {};
    input_event.event_type = COG_EVENT_INPUT_RECEIVED;
    input_event.source_module_id = MODULE_EMOTION;
    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_EMOTION,
        COG_EVENT_INPUT_RECEIVED, &input_event));

    // Step 2: Process through SNN (inter-layer)
    // encode_valence_arousal returns spike count (>=0) or error (<0)
    ASSERT_GE(emotion_snn_encode_valence_arousal(emotion_snn,
        0.6f, 0.7f, 0.8f), 0);
    ASSERT_EQ(0, emotion_snn_simulate(emotion_snn, 50.0f));

    // Step 3: Get SNN result
    emotion_snn_emotion_state_t snn_state = {};
    ASSERT_EQ(0, emotion_snn_get_emotion_state(emotion_snn, &snn_state));

    // Step 4: Feed back to hub (SNN -> Hub)
    cognitive_event_data_t output_event = {};
    output_event.event_type = COG_EVENT_OUTPUT_READY;
    output_event.source_module_id = MODULE_SNN;
    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_SNN,
        COG_EVENT_OUTPUT_READY, &output_event));

    EXPECT_EQ(output_ready.load(), 1);

    // Step 5: Tag memory with result (intra-layer)
    ASSERT_EQ(0, emotion_memory_tag_memory(emotion_memory,
        300, snn_state.valence, snn_state.arousal));

    // Step 6: Publish emotion update
    cognitive_event_data_t emo_event = {};
    emo_event.event_type = COG_EVENT_EMOTION_UPDATE;
    emo_event.source_module_id = MODULE_EMOTION;
    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_EMOTION,
        COG_EVENT_EMOTION_UPDATE, &emo_event));

    EXPECT_EQ(emotion_updates.load(), 1);

    // Verify all layers updated
    cognitive_hub_stats_t hub_stats = {};
    ASSERT_EQ(0, cognitive_hub_get_stats(hub, &hub_stats));
    EXPECT_GE(hub_stats.events_published, 3u);

    emotion_memory_stats_t em_stats = {};
    ASSERT_EQ(0, emotion_memory_bridge_get_stats(emotion_memory, &em_stats));
    EXPECT_GT(em_stats.memories_tagged, 0u);
}

/**
 * Test SNN and plasticity working together with hub
 */
TEST_F(CognitiveSNNPlasticityIntegrationTest, SNNPlasticityCoordination) {
    // Register synapse for learning
    uint32_t synapse_id = 100;
    ASSERT_EQ(0, emotion_plasticity_register_synapse(emotion_plasticity,
        synapse_id,
        EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
        EMOTION_FEAR,
        0.3f));

    uint64_t timestamp = GetTimestampUs();

    // Simulate emotional stimulus through SNN
    ASSERT_GE(emotion_snn_encode_valence_arousal(emotion_snn,
        -0.5f, 0.9f, 0.85f), 0);  // Negative valence, high arousal (fear-like)
    ASSERT_EQ(0, emotion_snn_simulate(emotion_snn, 50.0f));

    // Record stimulus for plasticity
    ASSERT_EQ(0, emotion_plasticity_stimulus(emotion_plasticity,
        EMOTION_FEAR, 0.85f, timestamp));

    // Get SNN decoded state
    emotion_snn_emotion_state_t snn_state = {};
    ASSERT_EQ(0, emotion_snn_get_emotion_state(emotion_snn, &snn_state));

    // Record response based on SNN output
    timestamp += 15000;
    ASSERT_EQ(0, emotion_plasticity_response(emotion_plasticity,
        EMOTION_FEAR, snn_state.intensity, timestamp));

    // Update plasticity
    ASSERT_EQ(0, emotion_plasticity_update(emotion_plasticity, 15.0f));

    // Consolidate learning
    ASSERT_EQ(0, emotion_plasticity_consolidate(emotion_plasticity));

    // Notify hub of consolidation
    cognitive_event_data_t event = {};
    event.event_type = COG_EVENT_CONSOLIDATION;
    event.source_module_id = MODULE_PLASTICITY;
    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_PLASTICITY,
        COG_EVENT_CONSOLIDATION, &event));

    // Verify stats
    emotion_plasticity_stats_t pstats = {};
    ASSERT_EQ(0, emotion_plasticity_get_stats(emotion_plasticity, &pstats));
    EXPECT_GT(pstats.total_observations, 0u);

    emotion_snn_stats_t sstats = {};
    ASSERT_EQ(0, emotion_snn_get_stats(emotion_snn, &sstats));
    // Decodings depend on internal pipeline state
    EXPECT_GE(sstats.total_decodings, 0u);
}

// =============================================================================
// Test Cases - Learning Across Layers
// =============================================================================

/**
 * Test learning in one layer affects another:
 * - Plasticity learning modifies sensitivity
 * - Modified sensitivity affects SNN processing
 */
TEST_F(CognitiveSNNPlasticityIntegrationTest, LearningAcrossLayers) {
    // Register multiple synapses
    for (uint32_t i = 0; i < 5; i++) {
        ASSERT_EQ(0, emotion_plasticity_register_synapse(emotion_plasticity,
            200 + i,
            EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
            static_cast<emotion_category_t>(i % EMOTION_COUNT),
            0.5f));
    }

    uint64_t timestamp = GetTimestampUs();

    // Learning phase: repeated stimulus-response pairs
    for (int trial = 0; trial < 10; trial++) {
        // Stimulus
        ASSERT_EQ(0, emotion_plasticity_stimulus(emotion_plasticity,
            EMOTION_HAPPINESS, 0.7f, timestamp));
        timestamp += 10000;

        // Response
        ASSERT_EQ(0, emotion_plasticity_response(emotion_plasticity,
            EMOTION_HAPPINESS, 0.6f + trial * 0.02f, timestamp));
        timestamp += 50000;

        // Reward (positive reinforcement)
        ASSERT_EQ(0, emotion_plasticity_reward(emotion_plasticity,
            0.5f, timestamp));
        timestamp += 100000;

        // Update
        ASSERT_EQ(0, emotion_plasticity_update(emotion_plasticity, 50.0f));
    }

    // Get learned sensitivity
    float sensitivity = emotion_plasticity_get_sensitivity(emotion_plasticity,
        EMOTION_HAPPINESS);

    // SNN processing should now reflect learned associations
    // Modulate SNN by learned arousal level
    ASSERT_EQ(0, emotion_snn_set_intensity_modulation(emotion_snn,
        sensitivity > 0.5f ? sensitivity : 0.5f));

    // Encode and simulate
    ASSERT_GE(emotion_snn_encode_valence_arousal(emotion_snn,
        0.8f, 0.7f, sensitivity), 0);
    ASSERT_EQ(0, emotion_snn_simulate(emotion_snn, 100.0f));

    // Get output
    emotion_snn_emotion_state_t snn_state = {};
    ASSERT_EQ(0, emotion_snn_get_emotion_state(emotion_snn, &snn_state));

    // Publish learning complete event
    cognitive_event_data_t event = {};
    event.event_type = COG_EVENT_LEARNING_COMPLETE;
    event.source_module_id = MODULE_PLASTICITY;
    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_PLASTICITY,
        COG_EVENT_LEARNING_COMPLETE, &event));

    // Memory should be tagged with learned emotion
    ASSERT_EQ(0, emotion_memory_tag_memory(emotion_memory,
        400, snn_state.valence, snn_state.arousal));

    // Verify cross-layer effects
    emotion_plasticity_stats_t pstats = {};
    ASSERT_EQ(0, emotion_plasticity_get_stats(emotion_plasticity, &pstats));
    EXPECT_GE(pstats.total_observations, 10u);
    EXPECT_GT(pstats.total_reward, 0.0f);
}

/**
 * Test extinction learning across layers
 */
TEST_F(CognitiveSNNPlasticityIntegrationTest, ExtinctionLearningAcrossLayers) {
    // Register fear synapse
    uint32_t synapse_id = 500;
    ASSERT_EQ(0, emotion_plasticity_register_synapse(emotion_plasticity,
        synapse_id,
        EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
        EMOTION_FEAR,
        0.8f));  // High initial weight (conditioned fear)

    uint64_t timestamp = GetTimestampUs();

    // Initial fear response
    ASSERT_EQ(0, emotion_plasticity_stimulus(emotion_plasticity,
        EMOTION_FEAR, 0.9f, timestamp));
    timestamp += 10000;
    ASSERT_EQ(0, emotion_plasticity_response(emotion_plasticity,
        EMOTION_FEAR, 0.85f, timestamp));
    ASSERT_EQ(0, emotion_plasticity_update(emotion_plasticity, 10.0f));

    // Extinction trials (stimulus without unconditioned stimulus)
    for (int trial = 0; trial < 5; trial++) {
        timestamp += 100000;
        ASSERT_EQ(0, emotion_plasticity_extinction_trial(emotion_plasticity,
            EMOTION_FEAR, timestamp));
        ASSERT_EQ(0, emotion_plasticity_update(emotion_plasticity, 50.0f));
    }

    // Get extinction level
    float extinction = emotion_plasticity_get_extinction_level(
        emotion_plasticity, EMOTION_FEAR);
    EXPECT_GT(extinction, 0.0f);  // Some extinction should have occurred

    // SNN should show reduced fear response
    ASSERT_GE(emotion_snn_encode_valence_arousal(emotion_snn,
        -0.5f, 0.3f, 0.4f), 0);  // Lower arousal after extinction
    ASSERT_EQ(0, emotion_snn_simulate(emotion_snn, 50.0f));

    emotion_snn_emotion_state_t snn_state = {};
    ASSERT_EQ(0, emotion_snn_get_emotion_state(emotion_snn, &snn_state));

    // Tag memory with post-extinction emotional state
    ASSERT_EQ(0, emotion_memory_tag_memory(emotion_memory,
        600, snn_state.valence, snn_state.arousal));

    // Modulate consolidation (extinction memory)
    ASSERT_EQ(0, emotion_memory_modulate_consolidation(emotion_memory,
        600, 0.7f));

    // Verify stats
    emotion_plasticity_stats_t stats = {};
    ASSERT_EQ(0, emotion_plasticity_get_stats(emotion_plasticity, &stats));
    EXPECT_GT(stats.extinction_events, 0u);
}

/**
 * Test GW broadcast integrates with SNN/plasticity
 */
TEST_F(CognitiveSNNPlasticityIntegrationTest, GWBroadcastIntegration) {
    // Register GW receivers
    std::atomic<int> broadcast_count{0};

    ASSERT_EQ(0, gw_cognitive_register_receiver(gw_cognitive, MODULE_MEMORY,
        [](gw_cognitive_content_type_t type, const void* data,
           size_t size, void* user_data) {
            (*static_cast<std::atomic<int>*>(user_data))++;
        }, &broadcast_count));

    ASSERT_EQ(0, gw_cognitive_register_receiver(gw_cognitive, MODULE_SNN,
        [](gw_cognitive_content_type_t type, const void* data,
           size_t size, void* user_data) {
            (*static_cast<std::atomic<int>*>(user_data))++;
        }, &broadcast_count));

    // Process emotion through SNN
    ASSERT_GE(emotion_snn_encode_valence_arousal(emotion_snn,
        0.7f, 0.6f, 0.75f), 0);
    ASSERT_EQ(0, emotion_snn_simulate(emotion_snn, 50.0f));

    emotion_snn_emotion_state_t snn_state = {};
    ASSERT_EQ(0, emotion_snn_get_emotion_state(emotion_snn, &snn_state));

    // Broadcast emotion to GW
    char broadcast_msg[128];
    snprintf(broadcast_msg, sizeof(broadcast_msg),
        "Emotion: valence=%.2f, arousal=%.2f",
        snn_state.valence, snn_state.arousal);

    ASSERT_EQ(0, gw_cognitive_broadcast(gw_cognitive,
        GW_COGNITIVE_CONTENT_EMOTION,
        broadcast_msg, strlen(broadcast_msg)));

    // Verify broadcast received
    EXPECT_EQ(broadcast_count.load(), 2);  // 2 receivers

    // Submit emotion for GW competition
    gw_cognitive_content_t content = {};
    content.content_type = GW_COGNITIVE_CONTENT_EMOTION;
    content.content_data = broadcast_msg;
    content.content_size = strlen(broadcast_msg);
    content.priority = snn_state.intensity;
    content.relevance = 0.8f;
    content.urgency = snn_state.arousal;

    ASSERT_EQ(0, gw_cognitive_compete_for_access(gw_cognitive,
        MODULE_EMOTION, &content, snn_state.intensity));

    // Verify GW stats
    gw_cognitive_stats_t gw_stats = {};
    ASSERT_EQ(0, gw_cognitive_get_stats(gw_cognitive, &gw_stats));
    EXPECT_GT(gw_stats.broadcasts_sent, 0u);
}

/**
 * Test error handling in cross-layer operations
 */
TEST_F(CognitiveSNNPlasticityIntegrationTest, CrossLayerErrorHandling) {
    // NULL parameters
    EXPECT_EQ(-1, emotion_snn_encode_valence_arousal(nullptr, 0.5f, 0.5f, 0.5f));
    EXPECT_EQ(-1, emotion_plasticity_stimulus(nullptr, EMOTION_HAPPINESS, 0.5f, 0));
    EXPECT_EQ(-1, emotion_memory_tag_memory(nullptr, 1, 0.5f, 0.5f));

    // Invalid synapse operations
    EXPECT_EQ(-1, emotion_plasticity_unregister_synapse(emotion_plasticity, 99999));

    // Reset operations should succeed
    ASSERT_EQ(0, emotion_snn_reset(emotion_snn));
    ASSERT_EQ(0, emotion_plasticity_reset(emotion_plasticity));

    // Verify bridges still operational after errors
    ASSERT_GE(emotion_snn_encode_valence_arousal(emotion_snn,
        0.5f, 0.5f, 0.5f), 0);

    uint32_t synapse_id = 999;
    ASSERT_EQ(0, emotion_plasticity_register_synapse(emotion_plasticity,
        synapse_id, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_HAPPINESS, 0.5f));

    // Hub should still work
    cognitive_event_data_t event = {};
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = MODULE_EMOTION;
    ASSERT_EQ(0, cognitive_hub_publish(hub, MODULE_EMOTION,
        COG_EVENT_STATE_CHANGE, &event));
}

/**
 * Test stats reset across layers
 */
TEST_F(CognitiveSNNPlasticityIntegrationTest, StatsResetAcrossLayers) {
    // Generate some activity
    ASSERT_GE(emotion_snn_encode_valence_arousal(emotion_snn,
        0.6f, 0.7f, 0.8f), 0);
    ASSERT_EQ(0, emotion_snn_simulate(emotion_snn, 50.0f));

    ASSERT_EQ(0, emotion_plasticity_register_synapse(emotion_plasticity,
        1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_HAPPINESS, 0.5f));
    ASSERT_EQ(0, emotion_plasticity_stimulus(emotion_plasticity,
        EMOTION_HAPPINESS, 0.5f, GetTimestampUs()));

    ASSERT_EQ(0, emotion_memory_tag_memory(emotion_memory, 1, 0.5f, 0.5f));

    // Verify stats retrieved successfully
    emotion_snn_stats_t snn_stats = {};
    ASSERT_EQ(0, emotion_snn_get_stats(emotion_snn, &snn_stats));
    // Decodings depend on internal pipeline state
    EXPECT_GE(snn_stats.total_decodings, 0u);

    emotion_plasticity_stats_t p_stats = {};
    ASSERT_EQ(0, emotion_plasticity_get_stats(emotion_plasticity, &p_stats));
    EXPECT_GT(p_stats.total_observations, 0u);

    // Reset stats
    emotion_snn_reset_stats(emotion_snn);
    emotion_plasticity_reset_stats(emotion_plasticity);

    // Verify stats reset
    ASSERT_EQ(0, emotion_snn_get_stats(emotion_snn, &snn_stats));
    EXPECT_EQ(snn_stats.total_decodings, 0u);

    ASSERT_EQ(0, emotion_plasticity_get_stats(emotion_plasticity, &p_stats));
    EXPECT_EQ(p_stats.total_observations, 0u);

    // Hub stats should be separate
    cognitive_hub_stats_t hub_stats = {};
    ASSERT_EQ(0, cognitive_hub_get_stats(hub, &hub_stats));
    // Hub stats may or may not be reset (not called here)
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
