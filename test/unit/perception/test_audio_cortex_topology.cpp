/**
 * @file test_audio_cortex_topology.cpp
 * @brief Unit tests for audio cortex fractal topology integration
 */

#include <gtest/gtest.h>
#include "perception/nimcp_audio_cortex.h"
#include "utils/nimcp_test_base.h"

class AudioCortexTopologyTest : public NimcpTestBase {
protected:
    audio_cortex_t* cortex = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent FIRST
    }

    void TearDown() override {
        if (cortex) {
            audio_cortex_destroy(cortex);
            cortex = nullptr;
        }
        NimcpTestBase::TearDown();  // Call parent LAST
    }
};

TEST_F(AudioCortexTopologyTest, CreateWithTopologyEnabled) {
    audio_cortex_config_t config = {
        .sample_rate = 16000,
        .frame_size = 512,
        .num_freq_bins = 256,
        .num_mel_filters = 40,
        .num_mfcc = 13,
        .num_channels = 1,
        .feature_dim = 128,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = true,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 400
    };

    cortex = audio_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);
}

TEST_F(AudioCortexTopologyTest, CreateWithoutTopology) {
    audio_cortex_config_t config = {
        .sample_rate = 16000,
        .frame_size = 512,
        .num_freq_bins = 256,
        .num_mel_filters = 40,
        .num_mfcc = 13,
        .num_channels = 1,
        .feature_dim = 128,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 400
    };

    cortex = audio_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);
}

TEST_F(AudioCortexTopologyTest, TopologyWithAllFeatures) {
    audio_cortex_config_t config = {
        .sample_rate = 16000,
        .frame_size = 512,
        .num_freq_bins = 256,
        .num_mel_filters = 40,
        .num_mfcc = 13,
        .num_channels = 1,
        .feature_dim = 128,
        .enable_attention = true,
        .enable_memory = true,
        .enable_fractal_topology = true,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 400
    };

    cortex = audio_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);
}

TEST_F(AudioCortexTopologyTest, MultipleCreateDestroy) {
    audio_cortex_config_t config = {
        .sample_rate = 16000,
        .frame_size = 512,
        .num_freq_bins = 256,
        .num_mel_filters = 40,
        .num_mfcc = 13,
        .num_channels = 1,
        .feature_dim = 128,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = true,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 200
    };

    for (int i = 0; i < 5; i++) {
        cortex = audio_cortex_create(&config);
        ASSERT_NE(cortex, nullptr);
        audio_cortex_destroy(cortex);
        cortex = nullptr;
    }
}
