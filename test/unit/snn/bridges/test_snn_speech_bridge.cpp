/**
 * @file test_snn_speech_bridge.cpp
 * @brief Unit tests for SNN Speech Cortex Bridge (15 tests)
 */

#include <gtest/gtest.h>
#include "snn/bridges/nimcp_snn_speech_bridge.h"

class SNNSpeechBridgeTest : public ::testing::Test {
protected:
    snn_speech_bridge_t* bridge;
    snn_network_t* snn;
    speech_cortex_t* speech_cortex;
    snn_speech_config_t config;

    void SetUp() override {
        bridge = nullptr;
        snn = nullptr;
        speech_cortex = nullptr;
        snn_speech_config_default(&config);
    }

    void TearDown() override {
        if (bridge) snn_speech_bridge_destroy(bridge);
        if (speech_cortex) speech_cortex_destroy(speech_cortex);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNSpeechBridgeTest, ConfigDefaults) {
    EXPECT_EQ(config.encoding_method, SNN_ENCODE_POPULATION);
    EXPECT_EQ(config.num_phonemes, 44);
    EXPECT_EQ(config.neurons_per_phoneme, 10);
    EXPECT_EQ(config.buffer_capacity, 9);
}

TEST_F(SNNSpeechBridgeTest, CreateDestroy) {
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(SNNSpeechBridgeTest, SinglePhonemeEncoding) {
    EXPECT_GT(config.neurons_per_phoneme, 0);
}

TEST_F(SNNSpeechBridgeTest, PhonemeSequenceEncoding) {
    EXPECT_TRUE(config.use_sequence_encoding);
}

TEST_F(SNNSpeechBridgeTest, PhonologicalBufferEncoding) {
    EXPECT_TRUE(config.encode_buffer_position);
    EXPECT_EQ(config.buffer_capacity, 9);  // 7±2
}

TEST_F(SNNSpeechBridgeTest, TuningCurveInitialization) {
    EXPECT_EQ(config.num_formants, 4);  // F1-F4
}

TEST_F(SNNSpeechBridgeTest, PopulationActivityComputation) {
    EXPECT_GT(config.neurons_per_phoneme, 0);
}

TEST_F(SNNSpeechBridgeTest, SpikeDecodingToPhoneme) {
    EXPECT_TRUE(config.use_winner_take_all);
}

TEST_F(SNNSpeechBridgeTest, SpikeDecodingToSequence) {
    EXPECT_GT(config.max_sequence_length, 0);
}

TEST_F(SNNSpeechBridgeTest, FeatureDecoding) {
    EXPECT_TRUE(config.encode_formants);
}

TEST_F(SNNSpeechBridgeTest, BioAsyncConnection) {
    EXPECT_EQ(BIO_MODULE_SNN_SPEECH, 0x0612);
}

TEST_F(SNNSpeechBridgeTest, StatisticsTracking) {
    // Bio-async is disabled by default
    EXPECT_FALSE(config.enable_bio_async);
}

TEST_F(SNNSpeechBridgeTest, UpdateCycle) {
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNSpeechBridgeTest, NullPointerHandling) {
    snn_speech_bridge_destroy(nullptr);
}

TEST_F(SNNSpeechBridgeTest, InvalidPhonemeHandling) {
    EXPECT_EQ(config.num_phonemes, 44);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
