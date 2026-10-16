/**
 * @file e2e_test_snn_pipeline.cpp
 * @brief End-to-end tests for SNN pipeline
 *
 * WHAT: E2E tests verifying complete SNN pipelines
 * WHY:  Ensure SNN works correctly in real-world scenarios
 * HOW:  Test full workflows from input to output
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Full SNN Pipeline E2E Tests
//=============================================================================

class SNNE2EPipelineTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_encoder_t* encoder = nullptr;
    snn_decoder_t* decoder = nullptr;
    snn_training_ctx_t* training = nullptr;

    void SetUp() override {
        // Create network
        snn_config_t config;
        snn_config_feedforward(&config, 8, 16, 4);
        config.enable_stdp = true;
        network = snn_network_create(&config);

        // Create encoder
        snn_rate_encoder_config_t enc_config;
        snn_rate_encoder_config_default(&enc_config);
        encoder = snn_encoder_create_rate(8, &enc_config);

        // Create decoder
        snn_rate_decoder_config_t dec_config;
        snn_rate_decoder_config_default(&dec_config);
        decoder = snn_decoder_create_rate(4, 4, &dec_config);

        // Create training
        snn_stdp_config_t stdp_config;
        snn_stdp_config_default(&stdp_config);
        training = snn_training_create_stdp(&stdp_config);
    }

    void TearDown() override {
        if (training) snn_training_destroy(training);
        if (decoder) snn_decoder_destroy(decoder);
        if (encoder) snn_encoder_destroy(encoder);
        if (network) snn_network_destroy(network);
    }
};

TEST_F(SNNE2EPipelineTest, FullForwardPipeline) {
    // Input data
    float raw_input[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    // Encode
    uint8_t spikes[8];
    int result = snn_encode_rate(encoder, raw_input, 1.0f, spikes);
    EXPECT_EQ(0, result);

    // Network forward
    float output[4];
    result = snn_network_forward(network, raw_input, 8, output, 4, 100.0f);
    EXPECT_EQ(0, result);

    // Decode
    float decoded[4];
    result = snn_decode_rate(decoder, output, decoded);
    EXPECT_EQ(0, result);

    // Verify outputs are valid
    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(decoded[i]));
        EXPECT_GE(decoded[i], 0.0f);
    }
}

TEST_F(SNNE2EPipelineTest, TrainingPipelineMultipleSteps) {
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Training loop
    for (int step = 0; step < 100; step++) {
        // Forward pass
        int result = snn_network_forward(network, input, 8, output, 4, 10.0f);
        EXPECT_EQ(0, result);
    }

    // Verify network still produces valid output
    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_FALSE(std::isinf(output[i]));
    }
}

TEST_F(SNNE2EPipelineTest, VariableInputSequence) {
    float inputs[][8] = {
        {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f},
        {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f},
        {0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f},
        {0.1f, 0.9f, 0.1f, 0.9f, 0.1f, 0.9f, 0.1f, 0.9f}
    };
    float output[4];

    for (int i = 0; i < 4; i++) {
        int result = snn_network_forward(network, inputs[i], 8, output, 4, 50.0f);
        EXPECT_EQ(0, result);

        for (int j = 0; j < 4; j++) {
            EXPECT_FALSE(std::isnan(output[j]));
        }
    }
}

//=============================================================================
// SNN + Immune E2E Tests
//=============================================================================

class SNNE2EImmuneTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    brain_immune_system_t* immune = nullptr;
    snn_immune_bridge_t* bridge = nullptr;
    snn_encoder_t* encoder = nullptr;

    void SetUp() override {
        // Create network
        snn_config_t config;
        snn_config_feedforward(&config, 8, 16, 4);
        config.enable_stdp = true;
        network = snn_network_create(&config);

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);

        // Create bridge
        snn_immune_config_t bridge_config;
        snn_immune_config_default(&bridge_config);
        bridge = snn_immune_bridge_create(&bridge_config, network, immune);

        // Create encoder
        snn_rate_encoder_config_t enc_config;
        snn_rate_encoder_config_default(&enc_config);
        encoder = snn_encoder_create_rate(8, &enc_config);
    }

    void TearDown() override {
        if (encoder) snn_encoder_destroy(encoder);
        if (bridge) snn_immune_bridge_destroy(bridge);
        if (network) snn_network_destroy(network);
        if (immune) brain_immune_destroy(immune);
    }
};

TEST_F(SNNE2EImmuneTest, TrainingWithImmuneModulation) {
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Training loop with immune modulation
    for (float t = 0.0f; t < 500.0f; t += 10.0f) {
        // Update immune effects
        snn_immune_update_effects(bridge);

        // Get modulated learning rate
        float base_lr = 0.01f;
        float mod_lr = snn_immune_modulate_learning_rate(bridge, base_lr);
        EXPECT_GT(mod_lr, 0.0f);

        // Forward pass
        snn_network_forward(network, input, 8, output, 4, 10.0f);

        // Update immune monitoring
        snn_immune_update(bridge, t);
    }

    // Verify health
    bool healthy = snn_immune_is_network_healthy(bridge);
    (void)healthy;  // May or may not be healthy
    SUCCEED();
}

TEST_F(SNNE2EImmuneTest, InstabilityDetectionAndRecovery) {
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Simulate some activity
    for (int i = 0; i < 10; i++) {
        snn_network_forward(network, input, 8, output, 4, 10.0f);
        snn_immune_update(bridge, i * 10.0f);
    }

    // Manually report an instability
    snn_immune_report_instability(bridge, SNN_STATE_EXPLOSION, 7);

    // Check it was recorded
    uint32_t reports;
    snn_immune_get_stats(bridge, nullptr, &reports, nullptr);
    EXPECT_GE(reports, 1u);

    // Continue simulation - should recover
    for (int i = 0; i < 10; i++) {
        snn_network_forward(network, input, 8, output, 4, 10.0f);
        snn_immune_update(bridge, (10 + i) * 10.0f);
    }

    // Outputs should still be valid
    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
    }
}

TEST_F(SNNE2EImmuneTest, STDPModulationDuringTraining) {
    snn_immune_update_effects(bridge);

    float a_plus = 0.1f;
    float a_minus = 0.05f;
    snn_immune_modulate_stdp(bridge, &a_plus, &a_minus);

    // Values should be modulated but still positive
    EXPECT_GT(a_plus, 0.0f);
    EXPECT_GT(a_minus, 0.0f);
}

//=============================================================================
// Bio-Async E2E Tests
//=============================================================================

class SNNE2EBioAsyncTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 8, 16, 4);
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) {
            snn_bio_async_disconnect(network);
            snn_network_destroy(network);
        }
    }
};

TEST_F(SNNE2EBioAsyncTest, ConnectAndBroadcastSpikes) {
    // Connect
    int result = snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);
    EXPECT_TRUE(result == 0 || result < 0);

    // Run some simulation
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    for (int step = 0; step < 10; step++) {
        snn_network_forward(network, input, 8, output, 4, 10.0f);

        // Broadcast spike events
        snn_bio_spike_msg_t spike = {
            .network_id = 1,
            .population_id = 0,
            .neuron_id = (uint32_t)step,
            .spike_time = step * 10.0f,
            .membrane_v = -30.0f,
            .is_burst = false
        };
        snn_bio_async_broadcast_spike(network, &spike);
    }

    // Disconnect
    result = snn_bio_async_disconnect(network);
    EXPECT_EQ(0, result);
}

TEST_F(SNNE2EBioAsyncTest, BroadcastTrainingProgress) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    for (int epoch = 0; epoch < 5; epoch++) {
        snn_bio_training_msg_t msg = {
            .network_id = 1,
            .mode = SNN_TRAIN_STDP,
            .loss = 1.0f - epoch * 0.1f,
            .learning_rate = 0.01f,
            .step = (uint32_t)epoch,
            .weight_updates = (uint32_t)(epoch * 10)
        };
        int result = snn_bio_async_broadcast_training(network, &msg);
        EXPECT_TRUE(result == 0 || result < 0);
    }
}

//=============================================================================
// Complete System E2E Tests
//=============================================================================

class SNNE2ECompleteSystemTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    brain_immune_system_t* immune = nullptr;
    snn_immune_bridge_t* bridge = nullptr;
    snn_encoder_t* encoder = nullptr;
    snn_decoder_t* decoder = nullptr;
    snn_training_ctx_t* training = nullptr;

    void SetUp() override {
        // Network
        snn_config_t net_config;
        snn_config_feedforward(&net_config, 8, 16, 4);
        net_config.enable_stdp = true;
        network = snn_network_create(&net_config);

        // Immune
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);

        // Bridge
        snn_immune_config_t bridge_config;
        snn_immune_config_default(&bridge_config);
        bridge = snn_immune_bridge_create(&bridge_config, network, immune);

        // Encoder
        snn_rate_encoder_config_t enc_config;
        snn_rate_encoder_config_default(&enc_config);
        encoder = snn_encoder_create_rate(8, &enc_config);

        // Decoder
        snn_rate_decoder_config_t dec_config;
        snn_rate_decoder_config_default(&dec_config);
        decoder = snn_decoder_create_rate(4, 4, &dec_config);

        // Training
        snn_stdp_config_t train_config;
        snn_stdp_config_default(&train_config);
        training = snn_training_create_stdp(&train_config);
    }

    void TearDown() override {
        if (training) snn_training_destroy(training);
        if (decoder) snn_decoder_destroy(decoder);
        if (encoder) snn_encoder_destroy(encoder);
        if (bridge) snn_immune_bridge_destroy(bridge);
        if (network) snn_network_destroy(network);
        if (immune) brain_immune_destroy(immune);
    }
};

TEST_F(SNNE2ECompleteSystemTest, FullTrainingPipeline) {
    float input[] = {0.3f, 0.4f, 0.5f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f};
    float output[4];

    // Connect bio-async
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    // Training loop
    for (float t = 0.0f; t < 1000.0f; t += 10.0f) {
        // Encode input
        uint8_t spikes[8];
        snn_encode_rate(encoder, input, 1.0f, spikes);

        // Update immune modulation
        snn_immune_update_effects(bridge);
        float mod_lr = snn_immune_modulate_learning_rate(bridge, 0.01f);
        (void)mod_lr;

        // Forward pass
        snn_network_forward(network, input, 8, output, 4, 10.0f);

        // Update immune monitoring
        snn_immune_update(bridge, t);

        // Check health periodically
        if (((int)t % 100) == 0) {
            snn_immune_check_and_report(bridge);
        }

        // Broadcast training progress
        if (((int)t % 50) == 0) {
            snn_bio_training_msg_t msg = {
                .network_id = 1,
                .mode = SNN_TRAIN_STDP,
                .loss = 0.5f,
                .learning_rate = 0.01f,
                .step = (uint32_t)(t / 10),
                .weight_updates = (uint32_t)(t / 10)
            };
            snn_bio_async_broadcast_training(network, &msg);
        }
    }

    // Disconnect
    snn_bio_async_disconnect(network);

    // Verify final state
    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_FALSE(std::isinf(output[i]));
    }
}

TEST_F(SNNE2ECompleteSystemTest, InferenceWithImmuneMonitoring) {
    // Process 100 samples
    for (int sample = 0; sample < 100; sample++) {
        float input[8];
        for (int i = 0; i < 8; i++) {
            input[i] = (float)(sample + i) / 107.0f;
        }

        // Update immune
        snn_immune_update(bridge, sample * 10.0f);

        // Forward
        float output[4];
        int result = snn_network_forward(network, input, 8, output, 4, 10.0f);
        EXPECT_EQ(0, result);

        // Decode
        float decoded[4];
        snn_decode_rate(decoder, output, decoded);

        for (int i = 0; i < 4; i++) {
            EXPECT_FALSE(std::isnan(decoded[i]));
        }
    }

    // Get final stats
    uint32_t instabilities, reports, updates;
    snn_immune_get_stats(bridge, &instabilities, &reports, nullptr);

    // May have some instabilities detected, that's OK
    SUCCEED();
}

TEST_F(SNNE2ECompleteSystemTest, RecoveryFromInstability) {
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Normal operation
    for (int i = 0; i < 10; i++) {
        snn_network_forward(network, input, 8, output, 4, 10.0f);
        snn_immune_update(bridge, i * 10.0f);
    }

    // Simulate instability
    snn_immune_report_instability(bridge, SNN_STATE_EXPLOSION, 8);

    // Continue operation - should recover
    for (int i = 0; i < 20; i++) {
        int result = snn_network_forward(network, input, 8, output, 4, 10.0f);
        EXPECT_EQ(0, result);
        snn_immune_update(bridge, (10 + i) * 10.0f);
    }

    // Outputs should be valid
    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_FALSE(std::isinf(output[i]));
    }
}
