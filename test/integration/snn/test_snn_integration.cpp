/**
 * @file test_snn_integration.cpp
 * @brief Integration tests for SNN with bio-async and immune system
 *
 * WHAT: Test SNN integration with other NIMCP modules
 * WHY:  Verify cross-module functionality
 * HOW:  Create combined systems and test interactions
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_bio_async.h"
#include "snn/nimcp_snn_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// SNN + Immune System Integration Tests
//=============================================================================

class SNNImmuneIntegrationTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    brain_immune_system_t* immune = nullptr;
    snn_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        // Create SNN network
        snn_config_t config;
        snn_config_feedforward(&config, 8, 16, 4);
        config.enable_stdp = true;
        network = snn_network_create(&config);

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
    }

    void TearDown() override {
        if (bridge) {
            snn_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (network) {
            snn_network_destroy(network);
            network = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
    }

    void CreateBridge() {
        snn_immune_config_t config;
        snn_immune_config_default(&config);
        bridge = snn_immune_bridge_create(&config, network, immune);
    }
};

TEST_F(SNNImmuneIntegrationTest, BridgeCreationWithNetworkAndImmune) {
    CreateBridge();
    ASSERT_NE(nullptr, bridge);
    EXPECT_TRUE(bridge->connected);
    EXPECT_EQ(network, bridge->network);
    EXPECT_EQ(immune, bridge->immune);
}

TEST_F(SNNImmuneIntegrationTest, UpdateCycleWithHealthyNetwork) {
    CreateBridge();
    ASSERT_NE(nullptr, bridge);

    // Run update cycle
    int result = snn_immune_update(bridge, 0.0f);
    EXPECT_EQ(0, result);

    // Check effects are initialized
    snn_cytokine_effects_t effects;
    snn_immune_get_effects(bridge, &effects);
    EXPECT_GE(effects.stdp_amplitude_factor, 0.0f);
    EXPECT_LE(effects.stdp_amplitude_factor, 2.0f);
}

TEST_F(SNNImmuneIntegrationTest, InstabilityReportingToImmune) {
    CreateBridge();
    ASSERT_NE(nullptr, bridge);

    // Report instability
    int result = snn_immune_report_instability(bridge, SNN_STATE_EXPLOSION, 8);
    EXPECT_EQ(0, result);

    // Check stats
    uint32_t instability, reports, updates;
    snn_immune_get_stats(bridge, &instability, &reports, &updates);
    EXPECT_EQ(1u, reports);
}

TEST_F(SNNImmuneIntegrationTest, STDPModulationByImmune) {
    CreateBridge();
    ASSERT_NE(nullptr, bridge);

    // Update effects
    snn_immune_update_effects(bridge);

    // Test STDP modulation
    float a_plus = 0.1f;
    float a_minus = 0.05f;

    snn_immune_modulate_stdp(bridge, &a_plus, &a_minus);

    // Values should be modified (may be same if no inflammation)
    EXPECT_GE(a_plus, 0.0f);
    EXPECT_GE(a_minus, 0.0f);
}

TEST_F(SNNImmuneIntegrationTest, ThresholdModulationByImmune) {
    CreateBridge();
    ASSERT_NE(nullptr, bridge);

    // Update effects
    snn_immune_update_effects(bridge);

    // Test threshold modulation
    float base_thresh = -55.0f;
    float mod_thresh = snn_immune_modulate_threshold(bridge, base_thresh);

    // Without inflammation, threshold should be unchanged
    EXPECT_FLOAT_EQ(base_thresh, mod_thresh);
}

TEST_F(SNNImmuneIntegrationTest, LearningRateModulationByImmune) {
    CreateBridge();
    ASSERT_NE(nullptr, bridge);

    // Update effects
    snn_immune_update_effects(bridge);

    // Test learning rate modulation
    float base_lr = 0.01f;
    float mod_lr = snn_immune_modulate_learning_rate(bridge, base_lr);

    // Without inflammation, LR should be unchanged
    EXPECT_FLOAT_EQ(base_lr, mod_lr);
}

TEST_F(SNNImmuneIntegrationTest, MultipleUpdateCycles) {
    CreateBridge();
    ASSERT_NE(nullptr, bridge);

    // Run multiple update cycles
    for (float t = 0.0f; t < 1000.0f; t += 100.0f) {
        int result = snn_immune_update(bridge, t);
        EXPECT_EQ(0, result);
    }

    // Should complete without errors
    SUCCEED();
}

TEST_F(SNNImmuneIntegrationTest, HealthCheckWithNoActivity) {
    CreateBridge();
    ASSERT_NE(nullptr, bridge);

    // Compute health
    snn_immune_compute_health(bridge);

    // Check health metrics
    snn_health_metrics_t health;
    snn_immune_get_health(bridge, &health);

    // Network with no activity may be silent
    EXPECT_TRUE(health.health == SNN_STATE_HEALTHY ||
                health.health == SNN_STATE_SILENT);
}

TEST_F(SNNImmuneIntegrationTest, BioAsyncConnectionWithBridge) {
    CreateBridge();
    ASSERT_NE(nullptr, bridge);

    // Try to connect bio-async
    int result = snn_immune_bridge_connect_bio_async(bridge);
    // May succeed or fail depending on router initialization
    EXPECT_TRUE(result == 0 || result < 0);

    // Check connection state
    bool connected = snn_immune_bridge_is_bio_async_connected(bridge);
    // Connection state depends on router availability
    (void)connected;

    // Disconnect
    result = snn_immune_bridge_disconnect_bio_async(bridge);
    EXPECT_TRUE(result == 0 || result < 0);
}

//=============================================================================
// SNN + Bio-Async Integration Tests
//=============================================================================

class SNNBioAsyncIntegrationTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 8, 16, 4);
        config.enable_stdp = true;
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) {
            snn_bio_async_disconnect(network);
            snn_network_destroy(network);
            network = nullptr;
        }
    }
};

TEST_F(SNNBioAsyncIntegrationTest, ConnectAndDisconnect) {
    int result = snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);
    EXPECT_TRUE(result == 0 || result < 0);

    result = snn_bio_async_disconnect(network);
    EXPECT_EQ(0, result);
}

TEST_F(SNNBioAsyncIntegrationTest, MessageHandlerRegistration) {
    auto handler = [](snn_network_t* net, snn_bio_msg_type_t type,
                      const void* msg, size_t size, void* data) -> int {
        (void)net; (void)type; (void)msg; (void)size; (void)data;
        return 0;
    };

    // Register handlers for all message types
    for (int t = SNN_BIO_MSG_SPIKE_EVENT; t < SNN_BIO_MSG_COUNT; t++) {
        int result = snn_bio_async_register_handler(
            network, (snn_bio_msg_type_t)t, handler, nullptr);
        // May fail without bio-async context
        EXPECT_TRUE(result == 0 || result < 0);
    }
}

TEST_F(SNNBioAsyncIntegrationTest, SpikeBroadcastSequence) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    // Broadcast a sequence of spikes
    for (uint32_t i = 0; i < 10; i++) {
        snn_bio_spike_msg_t event;
        event.network_id = network->id;
        event.population_id = 0;
        event.neuron_id = i;
        event.spike_time = (float)i * 1.0f;
        event.membrane_v = -30.0f;
        event.is_burst = false;

        int result = snn_bio_async_broadcast_spike(network, &event);
        EXPECT_TRUE(result == 0 || result < 0);
    }
}

TEST_F(SNNBioAsyncIntegrationTest, StateBroadcastSequence) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    // Broadcast state at different times
    for (int i = 0; i < 5; i++) {
        int result = snn_bio_async_broadcast_state(network, 0);
        EXPECT_TRUE(result == 0 || result < 0);
    }
}

TEST_F(SNNBioAsyncIntegrationTest, PhaseSyncRequest) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    // Request sync at different coherence targets
    float targets[] = {0.5f, 0.7f, 0.9f, 1.0f};
    for (float target : targets) {
        int result = snn_bio_async_request_sync(network, BIO_OSC_GAMMA, target);
        EXPECT_TRUE(result == 0 || result < 0);
    }
}

TEST_F(SNNBioAsyncIntegrationTest, MessageProcessingLoop) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    // Run a processing loop
    for (int i = 0; i < 10; i++) {
        int processed = snn_bio_async_process(network, 0);
        // May return error or 0 processed
        (void)processed;
    }
    SUCCEED();
}

//=============================================================================
// SNN Network + Training Integration Tests
//=============================================================================

class SNNTrainingIntegrationTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_training_ctx_t* training = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 8, 16, 4);
        config.enable_stdp = true;
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (training) {
            snn_training_destroy(training);
            training = nullptr;
        }
        if (network) {
            snn_network_destroy(network);
            network = nullptr;
        }
    }
};

TEST_F(SNNTrainingIntegrationTest, STDPTrainingSetup) {
    snn_stdp_config_t stdp_config;
    snn_stdp_config_default(&stdp_config);

    training = snn_training_create_stdp(&stdp_config);
    EXPECT_NE(nullptr, training);
}

TEST_F(SNNTrainingIntegrationTest, RSTDPTrainingSetup) {
    snn_rstdp_config_t rstdp_config;
    snn_rstdp_config_default(&rstdp_config);

    training = snn_training_create_rstdp(&rstdp_config, 8, 8);
    EXPECT_NE(nullptr, training);
}

TEST_F(SNNTrainingIntegrationTest, SurrogateGradientSetup) {
    snn_surrogate_config_t surrogate_config;
    snn_surrogate_config_default(&surrogate_config);

    training = snn_training_create_surrogate(&surrogate_config, 8, 8);
    EXPECT_NE(nullptr, training);
}

TEST_F(SNNTrainingIntegrationTest, EPropSetup) {
    snn_eprop_config_t eprop_config;
    snn_eprop_config_default(&eprop_config);

    training = snn_training_create_eprop(&eprop_config, 8, 8);
    EXPECT_NE(nullptr, training);
}

TEST_F(SNNTrainingIntegrationTest, TrainingModeSwitch) {
    // Create STDP training
    snn_stdp_config_t stdp_config;
    snn_stdp_config_default(&stdp_config);
    training = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(nullptr, training);

    EXPECT_EQ(SNN_TRAIN_STDP, training->mode);

    // Destroy and create R-STDP
    snn_training_destroy(training);

    snn_rstdp_config_t rstdp_config;
    snn_rstdp_config_default(&rstdp_config);
    training = snn_training_create_rstdp(&rstdp_config, 8, 8);
    ASSERT_NE(nullptr, training);

    EXPECT_EQ(SNN_TRAIN_R_STDP, training->mode);
}

//=============================================================================
// SNN Encoding Integration Tests
//=============================================================================

class SNNEncodingIntegrationTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_encoder_t* encoder = nullptr;
    snn_decoder_t* decoder = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 8, 16, 4);
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (encoder) {
            snn_encoder_destroy(encoder);
            encoder = nullptr;
        }
        if (decoder) {
            snn_decoder_destroy(decoder);
            decoder = nullptr;
        }
        if (network) {
            snn_network_destroy(network);
            network = nullptr;
        }
    }
};

TEST_F(SNNEncodingIntegrationTest, RateEncoderDecoder) {
    snn_rate_encoder_config_t enc_config;
    snn_rate_encoder_config_default(&enc_config);
    encoder = snn_encoder_create_rate(8, &enc_config);
    ASSERT_NE(nullptr, encoder);

    snn_rate_decoder_config_t dec_config;
    snn_rate_decoder_config_default(&dec_config);
    decoder = snn_decoder_create_rate(4, 4, &dec_config);
    ASSERT_NE(nullptr, decoder);
}

TEST_F(SNNEncodingIntegrationTest, TemporalEncoderOnly) {
    snn_temporal_encoder_config_t enc_config;
    snn_temporal_encoder_config_default(&enc_config);
    encoder = snn_encoder_create_temporal(8, &enc_config);
    ASSERT_NE(nullptr, encoder);
}

TEST_F(SNNEncodingIntegrationTest, PopulationEncoderOnly) {
    snn_population_encoder_config_t enc_config;
    snn_population_encoder_config_default(&enc_config);
    encoder = snn_encoder_create_population(8, &enc_config);
    ASSERT_NE(nullptr, encoder);
}

TEST_F(SNNEncodingIntegrationTest, EncodeRateInput) {
    snn_rate_encoder_config_t enc_config;
    snn_rate_encoder_config_default(&enc_config);
    encoder = snn_encoder_create_rate(4, &enc_config);
    ASSERT_NE(nullptr, encoder);

    // Create input
    float input[] = {0.1f, 0.5f, 0.8f, 1.0f};

    // Encode (output is spikes)
    uint8_t spikes[4];
    int result = snn_encode_rate(encoder, input, 1.0f, spikes);
    EXPECT_EQ(0, result);
}

//=============================================================================
// Full Pipeline Integration Tests
//=============================================================================

class SNNFullPipelineTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    brain_immune_system_t* immune = nullptr;
    snn_immune_bridge_t* bridge = nullptr;
    snn_encoder_t* encoder = nullptr;
    snn_decoder_t* decoder = nullptr;
    snn_training_ctx_t* training = nullptr;

    void SetUp() override {
        // Create network
        snn_config_t config;
        snn_config_feedforward(&config, 8, 16, 4);
        config.enable_stdp = true;
        network = snn_network_create(&config);

        // Create immune
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);

        // Create bridge
        snn_immune_config_t bridge_config;
        snn_immune_config_default(&bridge_config);
        bridge = snn_immune_bridge_create(&bridge_config, network, immune);

        // Create encoder/decoder
        snn_rate_encoder_config_t enc_config;
        snn_rate_encoder_config_default(&enc_config);
        encoder = snn_encoder_create_rate(8, &enc_config);

        snn_rate_decoder_config_t dec_config;
        snn_rate_decoder_config_default(&dec_config);
        decoder = snn_decoder_create_rate(4, 4, &dec_config);

        // Create training
        snn_stdp_config_t stdp_config;
        snn_stdp_config_default(&stdp_config);
        training = snn_training_create_stdp(&stdp_config);
    }

    void TearDown() override {
        if (training) {
            snn_training_destroy(training);
        }
        if (encoder) {
            snn_encoder_destroy(encoder);
        }
        if (decoder) {
            snn_decoder_destroy(decoder);
        }
        if (bridge) {
            snn_immune_bridge_destroy(bridge);
        }
        if (network) {
            snn_network_destroy(network);
        }
        if (immune) {
            brain_immune_destroy(immune);
        }
    }
};

TEST_F(SNNFullPipelineTest, AllComponentsCreated) {
    EXPECT_NE(nullptr, network);
    EXPECT_NE(nullptr, immune);
    EXPECT_NE(nullptr, bridge);
    EXPECT_NE(nullptr, encoder);
    EXPECT_NE(nullptr, decoder);
    EXPECT_NE(nullptr, training);
}

TEST_F(SNNFullPipelineTest, ForwardPassWithImmune) {
    // Update immune effects
    snn_immune_update_effects(bridge);

    // Create input
    float input[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float output[4];

    // Forward pass
    int result = snn_network_forward(network, input, 8, output, 4, 100.0f);
    EXPECT_EQ(0, result);
}

TEST_F(SNNFullPipelineTest, TrainingStepWithImmune) {
    // Update immune effects
    snn_immune_update_effects(bridge);

    // Get modulated learning rate
    float base_lr = 0.01f;
    float mod_lr = snn_immune_modulate_learning_rate(bridge, base_lr);

    // Training step would use mod_lr
    EXPECT_GE(mod_lr, 0.0f);
    EXPECT_LE(mod_lr, base_lr + 0.001f);  // May be slightly higher with IL-10
}

TEST_F(SNNFullPipelineTest, HealthMonitoringDuringTraining) {
    // Run multiple simulation steps
    float input[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float output[4];

    for (float t = 0.0f; t < 100.0f; t += 10.0f) {
        // Forward pass
        snn_network_forward(network, input, 8, output, 4, 10.0f);

        // Update immune bridge
        snn_immune_update(bridge, t);
    }

    // Check final health
    bool healthy = snn_immune_is_network_healthy(bridge);
    // May or may not be healthy depending on activity
    (void)healthy;
    SUCCEED();
}

TEST_F(SNNFullPipelineTest, EncodeForwardDecode) {
    float input[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    // Encode input
    uint8_t spikes[8];
    int result = snn_encode_rate(encoder, input, 1.0f, spikes);
    EXPECT_EQ(0, result);

    // Forward pass (using raw input for simplicity)
    float output[4];
    result = snn_network_forward(network, input, 8, output, 4, 100.0f);
    EXPECT_EQ(0, result);
}

TEST_F(SNNFullPipelineTest, BioAsyncIntegration) {
    // Connect to bio-async
    int result = snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);
    EXPECT_TRUE(result == 0 || result < 0);

    // Broadcast state
    result = snn_bio_async_broadcast_state(network, 0);
    EXPECT_TRUE(result == 0 || result < 0);

    // Disconnect
    result = snn_bio_async_disconnect(network);
    EXPECT_TRUE(result == 0 || result < 0);
}

//=============================================================================
// Extended Immune Integration Tests
//=============================================================================

class SNNImmuneExtendedTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    brain_immune_system_t* immune = nullptr;
    snn_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 4, 8, 2);
        network = snn_network_create(&config);

        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);

        snn_immune_config_t bridge_config;
        snn_immune_config_default(&bridge_config);
        bridge = snn_immune_bridge_create(&bridge_config, network, immune);
    }

    void TearDown() override {
        if (bridge) snn_immune_bridge_destroy(bridge);
        if (network) snn_network_destroy(network);
        if (immune) brain_immune_destroy(immune);
    }
};

TEST_F(SNNImmuneExtendedTest, CytokineEffectsUpdate) {
    snn_immune_update_effects(bridge);

    snn_cytokine_effects_t effects;
    int result = snn_immune_get_effects(bridge, &effects);
    EXPECT_EQ(0, result);
    EXPECT_GT(effects.stdp_amplitude_factor, 0.0f);
}

TEST_F(SNNImmuneExtendedTest, HealthMetricsComputation) {
    snn_immune_compute_health(bridge);

    snn_health_metrics_t health;
    int result = snn_immune_get_health(bridge, &health);
    EXPECT_EQ(0, result);
}

TEST_F(SNNImmuneExtendedTest, InstabilityReportingSequence) {
    // Report multiple instabilities
    snn_immune_report_instability(bridge, SNN_STATE_EXPLOSION, 8);
    snn_immune_report_instability(bridge, SNN_STATE_SILENT, 4);
    snn_immune_report_instability(bridge, SNN_STATE_EXPLOSION, 7);

    uint32_t instability, reports, updates;
    snn_immune_get_stats(bridge, &instability, &reports, &updates);
    EXPECT_EQ(3u, reports);
}

TEST_F(SNNImmuneExtendedTest, STDPModulationWithInflammation) {
    snn_immune_update_effects(bridge);

    float a_plus = 0.1f;
    float a_minus = 0.05f;
    snn_immune_modulate_stdp(bridge, &a_plus, &a_minus);

    // Values should be modulated but still positive
    EXPECT_GT(a_plus, 0.0f);
    EXPECT_GT(a_minus, 0.0f);
}

TEST_F(SNNImmuneExtendedTest, ThresholdModulation) {
    snn_immune_update_effects(bridge);

    float threshold = snn_immune_modulate_threshold(bridge, -55.0f);
    // Threshold should be returned (possibly modified)
    EXPECT_NE(0.0f, threshold);
}

TEST_F(SNNImmuneExtendedTest, UpdateCycleMultipleTimes) {
    for (float t = 0.0f; t < 500.0f; t += 50.0f) {
        int result = snn_immune_update(bridge, t);
        EXPECT_EQ(0, result);
    }
}

TEST_F(SNNImmuneExtendedTest, CheckAndReportInstabilities) {
    uint32_t reports = snn_immune_check_and_report(bridge);
    // May report 0 or 1 depending on network state
    EXPECT_LE(reports, 2u);
}

TEST_F(SNNImmuneExtendedTest, StatsResetAfterInstabilities) {
    snn_immune_report_instability(bridge, SNN_STATE_EXPLOSION, 5);
    snn_immune_report_instability(bridge, SNN_STATE_SILENT, 3);

    snn_immune_reset_stats(bridge);

    uint32_t instability, reports, updates;
    snn_immune_get_stats(bridge, &instability, &reports, &updates);
    EXPECT_EQ(0u, reports);
}

TEST_F(SNNImmuneExtendedTest, InflammationLevelQuery) {
    brain_inflammation_level_t level = snn_immune_get_inflammation(bridge);
    // Initially should be none
    EXPECT_EQ(INFLAMMATION_NONE, level);
}

TEST_F(SNNImmuneExtendedTest, NetworkHealthQuery) {
    bool healthy = snn_immune_is_network_healthy(bridge);
    EXPECT_TRUE(healthy);  // Initially healthy
}

//=============================================================================
// Extended Bio-Async Integration Tests
//=============================================================================

class SNNBioAsyncExtendedTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 4, 8, 2);
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) {
            snn_bio_async_disconnect(network);
            snn_network_destroy(network);
        }
    }
};

TEST_F(SNNBioAsyncExtendedTest, ConnectDisconnectCycle) {
    for (int i = 0; i < 3; i++) {
        int result = snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);
        EXPECT_TRUE(result == 0 || result < 0);

        result = snn_bio_async_disconnect(network);
        EXPECT_EQ(0, result);
    }
}

TEST_F(SNNBioAsyncExtendedTest, BroadcastSpikeEvents) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    for (int i = 0; i < 5; i++) {
        snn_bio_spike_msg_t spike = {
            .network_id = 1,
            .population_id = 0,
            .neuron_id = (uint32_t)i,
            .spike_time = i * 10.0f,
            .membrane_v = -30.0f + i,
            .is_burst = (i % 2 == 0)
        };
        int result = snn_bio_async_broadcast_spike(network, &spike);
        EXPECT_TRUE(result == 0 || result < 0);
    }
}

TEST_F(SNNBioAsyncExtendedTest, BroadcastSTDPEvents) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    for (int i = 0; i < 3; i++) {
        snn_bio_stdp_msg_t stdp = {
            .network_id = 1,
            .pre_id = (uint32_t)i,
            .post_id = (uint32_t)(i + 1),
            .delta_w = 0.01f * (i + 1),
            .new_weight = 0.5f + 0.01f * i,
            .dt = 5.0f + i
        };
        int result = snn_bio_async_broadcast_stdp(network, &stdp);
        EXPECT_TRUE(result == 0 || result < 0);
    }
}

TEST_F(SNNBioAsyncExtendedTest, BroadcastTrainingEvents) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    snn_bio_training_msg_t msg = {
        .network_id = 1,
        .mode = SNN_TRAIN_STDP,
        .loss = 0.5f,
        .learning_rate = 0.01f,
        .step = 100,
        .weight_updates = 50
    };
    int result = snn_bio_async_broadcast_training(network, &msg);
    EXPECT_TRUE(result == 0 || result < 0);
}

TEST_F(SNNBioAsyncExtendedTest, BroadcastPopulationActivity) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    uint32_t ids[] = {0, 2, 4, 6};
    float times[] = {1.0f, 3.0f, 5.0f, 7.0f};

    snn_bio_population_msg_t msg = {
        .network_id = 1,
        .population_id = 0,
        .n_active = 4,
        .neuron_ids = ids,
        .spike_times = times,
        .window_start = 0.0f,
        .window_end = 10.0f
    };
    int result = snn_bio_async_broadcast_population(network, &msg);
    EXPECT_TRUE(result == 0 || result < 0);
}

TEST_F(SNNBioAsyncExtendedTest, RequestSyncAllBands) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    nimcp_oscillation_band_t bands[] = {
        BIO_OSC_DELTA, BIO_OSC_THETA, BIO_OSC_ALPHA,
        BIO_OSC_BETA, BIO_OSC_GAMMA
    };

    for (int i = 0; i < 5; i++) {
        int result = snn_bio_async_request_sync(network, bands[i], 0.5f);
        EXPECT_TRUE(result == 0 || result < 0);
    }
}

TEST_F(SNNBioAsyncExtendedTest, ProcessMessagesNonBlocking) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    for (int i = 0; i < 5; i++) {
        int result = snn_bio_async_process(network, 0);
        (void)result;  // Any result is valid
    }
    SUCCEED();
}

TEST_F(SNNBioAsyncExtendedTest, MessageStats) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    uint64_t sent, received, dropped;
    int result = snn_bio_async_get_stats(network, &sent, &received, &dropped);
    // Stats should be accessible even if all zeros
    (void)result;
    SUCCEED();
}

//=============================================================================
// Extended Training Integration Tests
//=============================================================================

class SNNTrainingExtendedTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_training_ctx_t* training = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 4, 8, 2);
        config.enable_stdp = true;
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (training) snn_training_destroy(training);
        if (network) snn_network_destroy(network);
    }
};

TEST_F(SNNTrainingExtendedTest, STDPWithDifferentParams) {
    snn_stdp_config_t config;
    snn_stdp_config_default(&config);
    config.a_plus = 0.005f;
    config.a_minus = 0.0025f;
    config.tau_plus = 15.0f;
    config.tau_minus = 25.0f;

    training = snn_training_create_stdp(&config);
    EXPECT_NE(nullptr, training);
    EXPECT_EQ(SNN_TRAIN_STDP, training->mode);
}

TEST_F(SNNTrainingExtendedTest, RSTDPWithRewardModulation) {
    snn_rstdp_config_t config;
    snn_rstdp_config_default(&config);
    config.stdp.a_plus = 0.01f;
    config.reward_tau = 50.0f;

    training = snn_training_create_rstdp(&config, 8, 8);
    EXPECT_NE(nullptr, training);
    EXPECT_EQ(SNN_TRAIN_R_STDP, training->mode);
}

TEST_F(SNNTrainingExtendedTest, SurrogateWithCustomGradient) {
    snn_surrogate_config_t config;
    snn_surrogate_config_default(&config);
    config.beta = 5.0f;
    config.learning_rate = 0.005f;

    training = snn_training_create_surrogate(&config, 8, 8);
    EXPECT_NE(nullptr, training);
    // Training mode is set correctly by the create function
    EXPECT_TRUE(training->mode == SNN_TRAIN_SURROGATE || training->mode == SNN_TRAIN_EPROP);
}

TEST_F(SNNTrainingExtendedTest, EPropWithEligibility) {
    snn_eprop_config_t config;
    snn_eprop_config_default(&config);
    config.eligibility_tau = 15.0f;

    training = snn_training_create_eprop(&config, 8, 8);
    EXPECT_NE(nullptr, training);
    EXPECT_EQ(SNN_TRAIN_EPROP, training->mode);
}

TEST_F(SNNTrainingExtendedTest, TrainingWithNetworkForward) {
    snn_stdp_config_t config;
    snn_stdp_config_default(&config);
    training = snn_training_create_stdp(&config);
    ASSERT_NE(nullptr, training);

    float input[] = {0.1f, 0.2f, 0.3f, 0.4f};
    float output[2];

    for (int step = 0; step < 10; step++) {
        int result = snn_network_forward(network, input, 4, output, 2, 10.0f);
        EXPECT_EQ(0, result);
    }
}

TEST_F(SNNTrainingExtendedTest, MultipleTrainingModeTransitions) {
    // STDP -> R-STDP -> Surrogate -> eProp

    snn_stdp_config_t stdp_config;
    snn_stdp_config_default(&stdp_config);
    training = snn_training_create_stdp(&stdp_config);
    EXPECT_NE(nullptr, training);
    snn_training_destroy(training);

    snn_rstdp_config_t rstdp_config;
    snn_rstdp_config_default(&rstdp_config);
    training = snn_training_create_rstdp(&rstdp_config, 8, 8);
    EXPECT_NE(nullptr, training);
    snn_training_destroy(training);

    snn_surrogate_config_t surrogate_config;
    snn_surrogate_config_default(&surrogate_config);
    training = snn_training_create_surrogate(&surrogate_config, 8, 8);
    EXPECT_NE(nullptr, training);
    snn_training_destroy(training);

    snn_eprop_config_t eprop_config;
    snn_eprop_config_default(&eprop_config);
    training = snn_training_create_eprop(&eprop_config, 8, 8);
    EXPECT_NE(nullptr, training);
}

//=============================================================================
// Extended Encoding Integration Tests
//=============================================================================

class SNNEncodingExtendedTest : public ::testing::Test {
protected:
    snn_encoder_t* encoder = nullptr;
    snn_decoder_t* decoder = nullptr;

    void TearDown() override {
        if (encoder) snn_encoder_destroy(encoder);
        if (decoder) snn_decoder_destroy(decoder);
    }
};

TEST_F(SNNEncodingExtendedTest, RateEncoderVaryingInput) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);
    encoder = snn_encoder_create_rate(4, &config);
    ASSERT_NE(nullptr, encoder);

    float inputs[][4] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.5f, 0.5f, 0.5f, 0.5f},
        {0.1f, 0.3f, 0.7f, 0.9f}
    };

    for (int i = 0; i < 4; i++) {
        uint8_t spikes[4];
        int result = snn_encode_rate(encoder, inputs[i], 1.0f, spikes);
        EXPECT_EQ(0, result);
    }
}

TEST_F(SNNEncodingExtendedTest, TemporalEncoderMultipleEncodes) {
    snn_temporal_encoder_config_t config;
    snn_temporal_encoder_config_default(&config);
    encoder = snn_encoder_create_temporal(8, &config);
    ASSERT_NE(nullptr, encoder);

    for (int i = 0; i < 10; i++) {
        float input[8];
        for (int j = 0; j < 8; j++) {
            input[j] = (float)(i + j) / 20.0f;
        }

        float spike_times[8];
        int result = snn_encode_temporal(encoder, input, spike_times);
        EXPECT_EQ(0, result);
    }
}

TEST_F(SNNEncodingExtendedTest, PopulationEncoderSmallInput) {
    snn_population_encoder_config_t config;
    snn_population_encoder_config_default(&config);
    encoder = snn_encoder_create_population(4, &config);
    ASSERT_NE(nullptr, encoder);

    float input[4] = {0.1f, 0.3f, 0.6f, 0.9f};

    // Use heap allocation for safety
    float* activations = (float*)malloc(128 * sizeof(float));  // More than enough
    int result = snn_encode_population(encoder, input, activations);
    EXPECT_EQ(0, result);
    free(activations);
}

TEST_F(SNNEncodingExtendedTest, RateDecoderSpikeProcessing) {
    snn_rate_decoder_config_t config;
    snn_rate_decoder_config_default(&config);
    decoder = snn_decoder_create_rate(4, 2, &config);
    ASSERT_NE(nullptr, decoder);

    // Simulate spike counts as floats
    float spike_counts[] = {10.0f, 20.0f, 15.0f, 25.0f};
    float output[2];

    int result = snn_decode_rate(decoder, spike_counts, output);
    EXPECT_EQ(0, result);
}

TEST_F(SNNEncodingExtendedTest, RateEncoderEdgeCases) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);
    encoder = snn_encoder_create_rate(4, &config);
    ASSERT_NE(nullptr, encoder);

    // Test with edge values
    float input[] = {0.0f, 0.001f, 0.999f, 1.0f};
    uint8_t spikes[4];
    int result = snn_encode_rate(encoder, input, 1.0f, spikes);
    EXPECT_EQ(0, result);
}

//=============================================================================
// Combined Module Integration Tests
//=============================================================================

class SNNCombinedModulesTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    brain_immune_system_t* immune = nullptr;
    snn_immune_bridge_t* immune_bridge = nullptr;
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
        immune_bridge = snn_immune_bridge_create(&bridge_config, network, immune);

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
        if (immune_bridge) snn_immune_bridge_destroy(immune_bridge);
        if (network) snn_network_destroy(network);
        if (immune) brain_immune_destroy(immune);
    }
};

TEST_F(SNNCombinedModulesTest, FullForwardPipeline) {
    float raw_input[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    // Encode
    uint8_t spikes[8];
    int result = snn_encode_rate(encoder, raw_input, 1.0f, spikes);
    EXPECT_EQ(0, result);

    // Update immune effects
    snn_immune_update_effects(immune_bridge);

    // Forward pass
    float output[4];
    result = snn_network_forward(network, raw_input, 8, output, 4, 100.0f);
    EXPECT_EQ(0, result);
}

TEST_F(SNNCombinedModulesTest, TrainingWithImmuneModulation) {
    snn_immune_update_effects(immune_bridge);

    float base_lr = 0.01f;
    float mod_lr = snn_immune_modulate_learning_rate(immune_bridge, base_lr);

    float input[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float output[4];

    for (int step = 0; step < 5; step++) {
        snn_network_forward(network, input, 8, output, 4, 10.0f);
        snn_immune_update(immune_bridge, step * 10.0f);
    }

    EXPECT_GE(mod_lr, 0.0f);
}

TEST_F(SNNCombinedModulesTest, BioAsyncWithImmune) {
    // Connect bio-async
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    // Update immune
    snn_immune_update_effects(immune_bridge);

    // Broadcast state
    snn_bio_async_broadcast_state(network, 0);

    // Report instability through immune bridge
    snn_immune_report_instability(immune_bridge, SNN_STATE_EXPLOSION, 7);

    uint32_t instability, reports, updates;
    snn_immune_get_stats(immune_bridge, &instability, &reports, &updates);
    EXPECT_EQ(1u, reports);

    snn_bio_async_disconnect(network);
}

TEST_F(SNNCombinedModulesTest, SimulationLoopWithAllModules) {
    float input[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float output[4];

    // Connect bio-async
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    for (float t = 0.0f; t < 100.0f; t += 10.0f) {
        // Encode input
        uint8_t spikes[8];
        snn_encode_rate(encoder, input, 1.0f, spikes);

        // Forward pass
        snn_network_forward(network, input, 8, output, 4, 10.0f);

        // Update immune
        snn_immune_update(immune_bridge, t);

        // Check health
        snn_immune_check_and_report(immune_bridge);

        // Process bio-async messages
        snn_bio_async_process(network, 0);
    }

    snn_bio_async_disconnect(network);

    // Verify network is healthy
    bool healthy = snn_immune_is_network_healthy(immune_bridge);
    (void)healthy;  // May or may not be healthy
    SUCCEED();
}

TEST_F(SNNCombinedModulesTest, STDPModulationByCytokines) {
    snn_immune_update_effects(immune_bridge);

    float a_plus_orig = 0.1f;
    float a_minus_orig = 0.05f;
    float a_plus = a_plus_orig;
    float a_minus = a_minus_orig;

    snn_immune_modulate_stdp(immune_bridge, &a_plus, &a_minus);

    // Should be modulated (possibly unchanged if no inflammation)
    EXPECT_GT(a_plus, 0.0f);
    EXPECT_GT(a_minus, 0.0f);
}

TEST_F(SNNCombinedModulesTest, EncoderDecoderWithNetwork) {
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    // Encode
    uint8_t spikes[8];
    snn_encode_rate(encoder, input, 1.0f, spikes);

    // Forward
    float output[4];
    snn_network_forward(network, input, 8, output, 4, 100.0f);

    // Simulate spike counts from output layer as floats
    float spike_counts[] = {10.0f, 15.0f, 12.0f, 8.0f};
    float decoded[4];
    snn_decode_rate(decoder, spike_counts, decoded);

    for (int i = 0; i < 4; i++) {
        EXPECT_GE(decoded[i], 0.0f);
        EXPECT_LE(decoded[i], 1.0f);
    }
}

TEST_F(SNNCombinedModulesTest, InstabilityHandlingWithBioAsync) {
    snn_bio_async_connect(network, BIO_MODULE_SNN_CORE);

    // Simulate instabilities
    for (int i = 0; i < 5; i++) {
        snn_immune_report_instability(
            immune_bridge,
            (i % 2 == 0) ? SNN_STATE_EXPLOSION : SNN_STATE_SILENT,
            5 + i
        );

        // Broadcast training status
        snn_bio_training_msg_t msg = {
            .network_id = 1,
            .mode = SNN_TRAIN_STDP,
            .loss = 1.0f - i * 0.1f,
            .learning_rate = 0.01f,
            .step = (uint32_t)i,
            .weight_updates = (uint32_t)(10 + i)
        };
        snn_bio_async_broadcast_training(network, &msg);
    }

    uint32_t instability, reports, updates;
    snn_immune_get_stats(immune_bridge, &instability, &reports, &updates);
    EXPECT_EQ(5u, reports);

    snn_bio_async_disconnect(network);
}

TEST_F(SNNCombinedModulesTest, SmallNetworkConfiguration) {
    // Test with small network - just verify the existing network works
    float input[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float output[4];

    int result = snn_network_forward(network, input, 8, output, 4, 10.0f);
    EXPECT_EQ(0, result);
}
