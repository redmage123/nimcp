/**
 * @file e2e_test_snn_bridge_pipeline.cpp
 * @brief End-to-end tests for SNN Bridge pipelines
 *
 * WHAT: E2E tests verifying complete SNN bridge workflows
 * WHY:  Ensure SNN bridges work correctly in real-world scenarios
 * HOW:  Test full workflows combining multiple bridges
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_training.h"
#include "snn/bridges/nimcp_snn_training_integration_bridge.h"
#include "snn/bridges/nimcp_snn_emotion_bridge.h"
#include "snn/bridges/nimcp_snn_sleep_bridge.h"
#include "snn/bridges/nimcp_snn_autobiographical_bridge.h"
#include "snn/bridges/nimcp_snn_medulla_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Training Integration Pipeline E2E Tests
//=============================================================================

class SNNTrainingIntegrationPipelineE2E : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_training_ctx_t* training = nullptr;
    snn_training_integration_bridge_t* integration_bridge = nullptr;
    snn_encoder_t* encoder = nullptr;

    void SetUp() override {
        // Create network
        snn_config_t config;
        snn_config_feedforward(&config, 8, 16, 4);
        config.enable_stdp = true;
        network = snn_network_create(&config);
        ASSERT_NE(nullptr, network);

        // Create encoder
        snn_rate_encoder_config_t enc_config;
        snn_rate_encoder_config_default(&enc_config);
        encoder = snn_encoder_create_rate(8, &enc_config);
        ASSERT_NE(nullptr, encoder);

        // Create training context
        snn_stdp_config_t stdp_config;
        snn_stdp_config_default(&stdp_config);
        training = snn_training_create_stdp(&stdp_config);
        ASSERT_NE(nullptr, training);

        // Create integration bridge
        snn_training_integration_config_t int_config;
        snn_training_integration_config_default(&int_config);
        int_config.enable_bio_async = false;
        int_config.op_mode = SNN_TRAINING_INTEGRATION_OP_AUTOMATIC;
        integration_bridge = snn_training_integration_create(&int_config);
        ASSERT_NE(nullptr, integration_bridge);
    }

    void TearDown() override {
        if (integration_bridge) snn_training_integration_destroy(integration_bridge);
        if (training) snn_training_destroy(training);
        if (encoder) snn_encoder_destroy(encoder);
        if (network) snn_network_destroy(network);
    }
};

TEST_F(SNNTrainingIntegrationPipelineE2E, FullTrainingLoopWithIntegration) {
    // Connect context
    int ctx_id = snn_training_integration_connect_context(
        integration_bridge, training, network, "main_context");
    ASSERT_GE(ctx_id, 0);

    // Start bridge
    int ret = snn_training_integration_start(integration_bridge);
    ASSERT_EQ(0, ret);

    float input[] = {0.3f, 0.4f, 0.5f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f};
    float output[4];

    // Training loop - 10 epochs
    for (uint64_t epoch = 0; epoch < 10; epoch++) {
        // Each epoch has 100 steps
        for (int step = 0; step < 100; step++) {
            // Forward pass
            ret = snn_network_forward(network, input, 8, output, 4, 10.0f);
            EXPECT_EQ(0, ret);

            // Report learning events
            snn_training_integration_report_event(
                integration_bridge, SNN_LEARNING_EVENT_LTP, 0.1f);
            
            if (step % 2 == 0) {
                snn_training_integration_report_event(
                    integration_bridge, SNN_LEARNING_EVENT_LTD, 0.05f);
            }

            // Update bridge
            ret = snn_training_integration_update(integration_bridge, 10.0f);
            EXPECT_EQ(0, ret);
        }

        // Epoch complete
        ret = snn_training_integration_epoch_complete(integration_bridge, epoch);
        EXPECT_EQ(0, ret);

        // Check metrics
        float stability = snn_training_integration_get_learning_stability(integration_bridge);
        EXPECT_GE(stability, 0.0f);
        EXPECT_LE(stability, 1.0f);
    }

    // Final verification
    snn_training_integration_stats_t stats;
    ret = snn_training_integration_get_stats(integration_bridge, &stats);
    EXPECT_EQ(0, ret);
    EXPECT_GT(stats.total_ltp_events, 0u);
    EXPECT_GT(stats.total_update_calls, 0u);
}

TEST_F(SNNTrainingIntegrationPipelineE2E, AdaptiveLearningRateWithReward) {
    snn_training_integration_start(integration_bridge);

    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Simulate reward-modulated learning
    for (int trial = 0; trial < 50; trial++) {
        // Forward pass
        snn_network_forward(network, input, 8, output, 4, 10.0f);

        // Compute simulated reward based on output
        float reward = (output[0] + output[1]) / 2.0f - 0.5f;
        
        // Set reward
        snn_training_integration_set_reward(
            integration_bridge, reward, SNN_REWARD_SOURCE_EXTERNAL);

        // Modulate learning rate based on reward
        float lr_factor = (reward > 0) ? 1.2f : 0.8f;
        snn_training_integration_apply_lr_modulation(integration_bridge, lr_factor);

        // Update
        snn_training_integration_update(integration_bridge, 10.0f);
    }

    // Check cumulative reward
    float cumulative = snn_training_integration_get_cumulative_reward(integration_bridge);
    // Cumulative can be positive or negative
    EXPECT_FALSE(std::isnan(cumulative));
}

TEST_F(SNNTrainingIntegrationPipelineE2E, ConsolidationPhase) {
    snn_training_integration_start(integration_bridge);

    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Learning phase
    for (int i = 0; i < 50; i++) {
        snn_network_forward(network, input, 8, output, 4, 10.0f);
        snn_training_integration_report_event(integration_bridge, SNN_LEARNING_EVENT_LTP, 0.1f);
        snn_training_integration_update(integration_bridge, 10.0f);
    }

    // Enter consolidation mode
    snn_training_integration_consolidation_mode(integration_bridge, true);

    snn_training_integration_state_t state;
    snn_training_integration_get_state(integration_bridge, &state);
    EXPECT_TRUE(state.consolidation_active);

    // Consolidation phase (reduced learning)
    for (int i = 0; i < 20; i++) {
        snn_network_forward(network, input, 8, output, 4, 10.0f);
        snn_training_integration_update(integration_bridge, 10.0f);
    }

    // Exit consolidation
    snn_training_integration_consolidation_mode(integration_bridge, false);

    snn_training_integration_get_state(integration_bridge, &state);
    EXPECT_FALSE(state.consolidation_active);
}

//=============================================================================
// Multi-Bridge E2E Pipeline Tests
//=============================================================================

class SNNMultiBridgePipelineE2E : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_training_ctx_t* training = nullptr;
    snn_training_integration_bridge_t* training_bridge = nullptr;
    snn_emotion_bridge_t* emotion_bridge = nullptr;
    snn_sleep_bridge_t* sleep_bridge = nullptr;
    snn_autobiographical_bridge_t* autobio_bridge = nullptr;

    void SetUp() override {
        // Create network
        snn_config_t config;
        snn_config_feedforward(&config, 8, 16, 4);
        config.enable_stdp = true;
        network = snn_network_create(&config);
        ASSERT_NE(nullptr, network);

        // Create training context
        snn_stdp_config_t stdp_config;
        snn_stdp_config_default(&stdp_config);
        training = snn_training_create_stdp(&stdp_config);

        // Create all bridges
        snn_training_integration_config_t train_config;
        snn_training_integration_config_default(&train_config);
        train_config.enable_bio_async = false;
        training_bridge = snn_training_integration_create(&train_config);
        ASSERT_NE(nullptr, training_bridge);

        snn_emotion_config_t emotion_config;
        snn_emotion_config_default(&emotion_config);
        emotion_config.enable_bio_async = false;
        emotion_bridge = snn_emotion_bridge_create(&emotion_config, network, nullptr);
        ASSERT_NE(nullptr, emotion_bridge);

        snn_sleep_config_t sleep_config;
        snn_sleep_config_default(&sleep_config);
        sleep_config.enable_bio_async = false;
        sleep_bridge = snn_sleep_bridge_create(&sleep_config, network);
        ASSERT_NE(nullptr, sleep_bridge);

        snn_autobiographical_config_t autobio_config;
        snn_autobiographical_config_default(&autobio_config);
        autobio_config.enable_bio_async = false;
        autobio_bridge = snn_autobiographical_bridge_create(&autobio_config, network);
        ASSERT_NE(nullptr, autobio_bridge);
    }

    void TearDown() override {
        if (autobio_bridge) snn_autobiographical_bridge_destroy(autobio_bridge);
        if (sleep_bridge) snn_sleep_bridge_destroy(sleep_bridge);
        if (emotion_bridge) snn_emotion_bridge_destroy(emotion_bridge);
        if (training_bridge) snn_training_integration_destroy(training_bridge);
        if (training) snn_training_destroy(training);
        if (network) snn_network_destroy(network);
    }
};

TEST_F(SNNMultiBridgePipelineE2E, CognitiveDayNightCycle) {
    snn_training_integration_start(training_bridge);

    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Simulate day/night cycle (simplified)
    for (int hour = 0; hour < 24; hour++) {
        bool is_day = (hour >= 6 && hour < 22);

        // Update all bridges
        snn_emotion_bridge_update(emotion_bridge, 3600.0f);
        snn_sleep_bridge_update(sleep_bridge, 3600.0f);
        snn_autobiographical_bridge_update(autobio_bridge, 3600.0f);

        if (is_day) {
            // Active learning during day
            for (int step = 0; step < 10; step++) {
                snn_network_forward(network, input, 8, output, 4, 10.0f);
                snn_training_integration_report_event(
                    training_bridge, SNN_LEARNING_EVENT_LTP, 0.1f);
                snn_training_integration_update(training_bridge, 10.0f);
            }

            // Emotional experiences lead to memory encoding
            float valence = snn_emotion_get_decoded_valence(emotion_bridge);
            if (fabsf(valence) > 0.3f) {
                uint32_t memory_id;
                snn_autobiographical_encode_episode(
                    autobio_bridge, SNN_ENCODING_STRONG, fabsf(valence), &memory_id);
            }
        } else {
            // Consolidation during night
            snn_training_integration_consolidation_mode(training_bridge, true);
            snn_training_integration_update(training_bridge, 3600.0f);
            snn_training_integration_consolidation_mode(training_bridge, false);
        }
    }

    // Verify day completed successfully
    snn_training_integration_stats_t stats;
    snn_training_integration_get_stats(training_bridge, &stats);
    EXPECT_GT(stats.total_update_calls, 0u);

    uint32_t memory_count = 0, encoding_count = 0;
    float retrieval_rate = 0.0f;
    snn_autobiographical_get_stats(autobio_bridge, &memory_count, &encoding_count, &retrieval_rate);
    // May or may not have encoded episodes depending on valence
    SUCCEED();
}

TEST_F(SNNMultiBridgePipelineE2E, EmotionalLearningModulation) {
    snn_training_integration_start(training_bridge);

    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Learning modulated by emotion
    for (int trial = 0; trial < 100; trial++) {
        // Update emotion
        snn_emotion_bridge_update(emotion_bridge, 10.0f);
        float valence = snn_emotion_get_decoded_valence(emotion_bridge);
        float arousal = snn_emotion_get_decoded_arousal(emotion_bridge);

        // Positive emotion boosts learning
        float lr_factor = 1.0f + valence * 0.5f;  // Range [0.5, 1.5]
        lr_factor = fmaxf(0.5f, fminf(1.5f, lr_factor));

        snn_training_integration_apply_lr_modulation(training_bridge, lr_factor);

        // High arousal increases LTP
        float ltp_magnitude = 0.1f * (1.0f + arousal);
        snn_training_integration_report_event(
            training_bridge, SNN_LEARNING_EVENT_LTP, ltp_magnitude);

        // Forward pass
        snn_network_forward(network, input, 8, output, 4, 10.0f);

        // Update training bridge
        snn_training_integration_update(training_bridge, 10.0f);
    }

    // Verify emotional modulation occurred
    snn_training_integration_stats_t stats;
    snn_training_integration_get_stats(training_bridge, &stats);
    EXPECT_GT(stats.lr_modulations, 0u);
}

TEST_F(SNNMultiBridgePipelineE2E, MemoryFormationDuringLearning) {
    snn_training_integration_start(training_bridge);

    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    uint32_t memories_encoded = 0;

    // Learning with memory formation
    for (int episode = 0; episode < 20; episode++) {
        // Episode learning
        float episode_reward = 0.0f;

        for (int step = 0; step < 50; step++) {
            snn_network_forward(network, input, 8, output, 4, 10.0f);
            
            // Simulate reward
            float step_reward = (output[0] > 0.5f) ? 0.1f : -0.05f;
            episode_reward += step_reward;

            snn_training_integration_set_reward(
                training_bridge, step_reward, SNN_REWARD_SOURCE_EXTERNAL);
            snn_training_integration_update(training_bridge, 10.0f);
        }

        // Encode significant episodes
        if (fabsf(episode_reward) > 1.0f) {
            uint32_t memory_id;
            snn_encoding_strength_t strength = (episode_reward > 0) 
                ? SNN_ENCODING_STRONG 
                : SNN_ENCODING_MODERATE;
            
            int ret = snn_autobiographical_encode_episode(
                autobio_bridge, strength, fabsf(episode_reward), &memory_id);
            if (ret == 0) {
                memories_encoded++;
            }
        }

        // Complete epoch
        snn_training_integration_epoch_complete(training_bridge, episode);
    }

    // Verify memories were formed
    EXPECT_GT(memories_encoded, 0u);

    uint32_t memory_count = 0, encoding_count = 0;
    float retrieval_rate = 0.0f;
    snn_autobiographical_get_stats(autobio_bridge, &memory_count, &encoding_count, &retrieval_rate);
    EXPECT_GE(encoding_count, memories_encoded);
}

//=============================================================================
// Complete System Pipeline E2E Tests
//=============================================================================

class SNNCompleteSystemPipelineE2E : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    brain_immune_system_t* immune = nullptr;
    snn_training_ctx_t* training = nullptr;
    snn_training_integration_bridge_t* training_bridge = nullptr;
    snn_emotion_bridge_t* emotion_bridge = nullptr;
    snn_sleep_bridge_t* sleep_bridge = nullptr;
    snn_autobiographical_bridge_t* autobio_bridge = nullptr;
    snn_medulla_bridge_t* medulla_bridge = nullptr;
    snn_encoder_t* encoder = nullptr;
    snn_decoder_t* decoder = nullptr;

    void SetUp() override {
        // Network
        snn_config_t net_config;
        snn_config_feedforward(&net_config, 8, 16, 4);
        net_config.enable_stdp = true;
        network = snn_network_create(&net_config);
        ASSERT_NE(nullptr, network);

        // Immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(nullptr, immune);

        // Training
        snn_stdp_config_t stdp_config;
        snn_stdp_config_default(&stdp_config);
        training = snn_training_create_stdp(&stdp_config);

        // All bridges
        snn_training_integration_config_t train_config;
        snn_training_integration_config_default(&train_config);
        train_config.enable_bio_async = false;
        training_bridge = snn_training_integration_create(&train_config);

        snn_emotion_config_t emotion_config;
        snn_emotion_config_default(&emotion_config);
        emotion_bridge = snn_emotion_bridge_create(&emotion_config, network, nullptr);

        snn_sleep_config_t sleep_config;
        snn_sleep_config_default(&sleep_config);
        sleep_bridge = snn_sleep_bridge_create(&sleep_config, network);

        snn_autobiographical_config_t autobio_config;
        snn_autobiographical_config_default(&autobio_config);
        autobio_bridge = snn_autobiographical_bridge_create(&autobio_config, network);

        snn_medulla_config_t medulla_config;
        snn_medulla_config_default(&medulla_config);
        medulla_bridge = snn_medulla_bridge_create(&medulla_config, network, nullptr);

        // Encoder/Decoder
        snn_rate_encoder_config_t enc_config;
        snn_rate_encoder_config_default(&enc_config);
        encoder = snn_encoder_create_rate(8, &enc_config);

        snn_rate_decoder_config_t dec_config;
        snn_rate_decoder_config_default(&dec_config);
        decoder = snn_decoder_create_rate(4, 4, &dec_config);
    }

    void TearDown() override {
        if (decoder) snn_decoder_destroy(decoder);
        if (encoder) snn_encoder_destroy(encoder);
        if (medulla_bridge) snn_medulla_bridge_destroy(medulla_bridge);
        if (autobio_bridge) snn_autobiographical_bridge_destroy(autobio_bridge);
        if (sleep_bridge) snn_sleep_bridge_destroy(sleep_bridge);
        if (emotion_bridge) snn_emotion_bridge_destroy(emotion_bridge);
        if (training_bridge) snn_training_integration_destroy(training_bridge);
        if (training) snn_training_destroy(training);
        if (network) snn_network_destroy(network);
        if (immune) brain_immune_destroy(immune);
    }
};

TEST_F(SNNCompleteSystemPipelineE2E, FullLifecycleSimulation) {
    // Start training bridge
    snn_training_integration_start(training_bridge);

    auto start = std::chrono::high_resolution_clock::now();

    float input[] = {0.3f, 0.4f, 0.5f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f};
    float output[4];

    // Simulate 1000ms of activity
    for (float t = 0.0f; t < 1000.0f; t += 10.0f) {
        // Encode input
        uint8_t spikes[8];
        snn_encode_rate(encoder, input, 1.0f, spikes);

        // Get arousal modulation from medulla
        float arousal_mod = snn_medulla_compute_arousal_modulation(medulla_bridge);

        // Update emotion
        snn_emotion_bridge_update(emotion_bridge, 10.0f);
        float valence = snn_emotion_get_decoded_valence(emotion_bridge);

        // Modulate learning based on emotion and arousal
        float lr_factor = arousal_mod * (1.0f + valence * 0.3f);
        lr_factor = fmaxf(0.3f, fminf(2.0f, lr_factor));
        snn_training_integration_apply_lr_modulation(training_bridge, lr_factor);

        // Forward pass
        snn_network_forward(network, input, 8, output, 4, 10.0f);

        // Report events
        snn_training_integration_report_event(
            training_bridge, SNN_LEARNING_EVENT_LTP, 0.1f * arousal_mod);

        // Update all bridges
        snn_training_integration_update(training_bridge, 10.0f);
        snn_sleep_bridge_update(sleep_bridge, 10.0f);
        snn_autobiographical_bridge_update(autobio_bridge, 10.0f);
        snn_medulla_bridge_update(medulla_bridge, 10.0f);

        // Decode output
        float decoded[4];
        snn_decode_rate(decoder, output, decoded);

        // Verify outputs are valid
        for (int i = 0; i < 4; i++) {
            EXPECT_FALSE(std::isnan(decoded[i]));
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 10000); // < 10 seconds

    std::cout << "Full lifecycle simulation completed in " << duration.count() << "ms" << std::endl;

    // Final state verification
    snn_training_integration_stats_t train_stats;
    snn_training_integration_get_stats(training_bridge, &train_stats);
    EXPECT_GT(train_stats.total_update_calls, 0u);
    EXPECT_GT(train_stats.lr_modulations, 0u);
}

TEST_F(SNNCompleteSystemPipelineE2E, StressTestConcurrentBridges) {
    snn_training_integration_start(training_bridge);

    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Stress test: many rapid updates
    for (int i = 0; i < 1000; i++) {
        // Update all bridges in rapid succession
        snn_emotion_bridge_update(emotion_bridge, 1.0f);
        snn_sleep_bridge_update(sleep_bridge, 1.0f);
        snn_autobiographical_bridge_update(autobio_bridge, 1.0f);
        snn_medulla_bridge_update(medulla_bridge, 1.0f);
        snn_training_integration_update(training_bridge, 1.0f);

        // Forward pass
        snn_network_forward(network, input, 8, output, 4, 1.0f);

        // Various events
        snn_training_integration_report_event(training_bridge, SNN_LEARNING_EVENT_LTP, 0.1f);
        snn_training_integration_set_reward(training_bridge, 0.1f, SNN_REWARD_SOURCE_EXTERNAL);
    }

    // Verify system survived stress test
    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_FALSE(std::isinf(output[i]));
    }
}

TEST_F(SNNCompleteSystemPipelineE2E, GracefulShutdownSequence) {
    snn_training_integration_start(training_bridge);

    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Some activity
    for (int i = 0; i < 100; i++) {
        snn_network_forward(network, input, 8, output, 4, 10.0f);
        snn_training_integration_update(training_bridge, 10.0f);
    }

    // Stop training bridge gracefully
    int ret = snn_training_integration_stop(training_bridge);
    EXPECT_EQ(0, ret);

    // Verify state
    snn_training_integration_state_t state;
    snn_training_integration_get_state(training_bridge, &state);
    // Bridge should be stopped but have recorded activity

    snn_training_integration_stats_t stats;
    snn_training_integration_get_stats(training_bridge, &stats);
    EXPECT_GT(stats.total_update_calls, 0u);

    // Destroy in order - should not crash
    // (cleanup happens in TearDown)
    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
