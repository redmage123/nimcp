/**
 * @file test_speech_cortex_topology.cpp
 * @brief Unit tests for speech cortex fractal topology integration
 */

#include <gtest/gtest.h>
#include "perception/nimcp_speech_cortex.h"
#include "utils/nimcp_test_base.h"

class SpeechCortexTopologyTest : public NimcpTestBase {
protected:
    speech_cortex_t* cortex = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent FIRST
    }

    void TearDown() override {
        if (cortex) {
            speech_cortex_destroy(cortex);
            cortex = nullptr;
        }
        NimcpTestBase::TearDown();  // Call parent LAST
    }
};

TEST_F(SpeechCortexTopologyTest, CreateWithTopologyEnabled) {
    speech_cortex_config_t config = speech_cortex_default_config();
    config.enable_fractal_topology = true;
    config.internal_neurons = 440;

    cortex = speech_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);
}

TEST_F(SpeechCortexTopologyTest, CreateWithoutTopology) {
    speech_cortex_config_t config = speech_cortex_default_config();
    config.enable_fractal_topology = false;

    cortex = speech_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);
}

TEST_F(SpeechCortexTopologyTest, TopologyWithAllFeatures) {
    speech_cortex_config_t config = speech_cortex_default_config();
    config.enable_fractal_topology = true;
    config.enable_wernicke = true;
    config.enable_prosody = true;
    config.enable_memory = true;
    config.internal_neurons = 440;

    cortex = speech_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);
}

TEST_F(SpeechCortexTopologyTest, MultipleCreateDestroy) {
    speech_cortex_config_t config = speech_cortex_default_config();
    config.enable_fractal_topology = true;
    config.internal_neurons = 200;

    for (int i = 0; i < 5; i++) {
        cortex = speech_cortex_create(&config);
        ASSERT_NE(cortex, nullptr);
        speech_cortex_destroy(cortex);
        cortex = nullptr;
    }
}
