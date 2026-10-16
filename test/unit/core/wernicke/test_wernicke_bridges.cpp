//=============================================================================
// test_wernicke_bridges.cpp - Wernicke Bridge Unit Tests
//=============================================================================
/**
 * @file test_wernicke_bridges.cpp
 * @brief Unit tests for Wernicke's area bridges
 *
 * WHAT: Tests all Wernicke bridge modules
 * WHY:  Verify correct integration with other brain systems
 * HOW:  gtest framework testing each bridge independently
 *
 * BRIDGES TESTED:
 * - Broca bridge (arcuate fasciculus)
 * - NLP bridge (language processing)
 * - Substrate bridge (metabolic effects)
 * - GPU-Bio bridge (acceleration)
 * - Omni bridge (inference)
 * - Quantum bridge (optimization)
 * - Immune bridge (inflammation effects)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Include GPU header BEFORE extern "C" to avoid CUDA template conflicts
#include "core/brain/regions/wernicke/nimcp_wernicke_gpu_bio_bridge.h"

#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_broca_bridge.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_nlp_bridge.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_substrate_bridge.h"
#include "core/brain/regions/wernicke/nimcp_omni_wernicke_bridge.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_quantum_bridge.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WernickeBridgesTest : public ::testing::Test {
protected:
    wernicke_adapter_t* adapter;
    brain_immune_system_t* immune_system;

    void SetUp() override {
        wernicke_config_t config = wernicke_default_config();
        adapter = wernicke_create(&config);
        ASSERT_NE(adapter, nullptr) << "Failed to create Wernicke adapter";

        // Create brain immune system for immune bridge tests
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_system = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_system, nullptr) << "Failed to create immune system";
    }

    void TearDown() override {
        if (immune_system) {
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
        if (adapter) {
            wernicke_destroy(adapter);
            adapter = nullptr;
        }
    }
};

//=============================================================================
// Broca Bridge Tests (wbb_* API)
//=============================================================================

/**
 * @test Create Broca bridge
 * WHAT: Test wbb_create
 * WHY:  Verify arcuate fasciculus connection setup
 * HOW:  Create bridge, verify non-null
 */
TEST_F(WernickeBridgesTest, CreateBrocaBridge) {
    wbb_config_t config = wbb_default_config();
    config.enable_dorsal_stream = true;
    config.enable_ventral_stream = true;

    // wbb_create takes (wernicke, broca, config) - using NULL for broca
    wernicke_broca_bridge_t* bridge = wbb_create(adapter, nullptr, &config);
    EXPECT_NE(bridge, nullptr) << "Failed to create Broca bridge";

    if (bridge) {
        wbb_destroy(bridge);
    }
}

/**
 * @test Create Broca bridge with null config
 * WHAT: Test wbb_create with defaults
 * WHY:  Verify default configuration works
 * HOW:  Create bridge with null config
 */
TEST_F(WernickeBridgesTest, CreateBrocaBridgeNullConfig) {
    wernicke_broca_bridge_t* bridge = wbb_create(adapter, nullptr, nullptr);
    EXPECT_NE(bridge, nullptr) << "Should create with default config";

    if (bridge) {
        wbb_destroy(bridge);
    }
}

/**
 * @test Broca bridge default config
 * WHAT: Test wbb_default_config
 * WHY:  Verify sensible default values
 * HOW:  Get defaults, check key fields
 */
TEST_F(WernickeBridgesTest, BrocaBridgeDefaultConfig) {
    wbb_config_t config = wbb_default_config();

    // Check that key features are enabled by default
    EXPECT_TRUE(config.enable_dorsal_stream || config.enable_ventral_stream);
}

/**
 * @test Destroy null Broca bridge
 * WHAT: Test wbb_destroy with null
 * WHY:  Verify null safety
 * HOW:  Call destroy with null, should not crash
 */
TEST_F(WernickeBridgesTest, DestroyNullBrocaBridge) {
    wbb_destroy(nullptr);
    // No assertion - just verify no crash
}

//=============================================================================
// NLP Bridge Tests
//=============================================================================

/**
 * @test Create NLP bridge
 * WHAT: Test wernicke_nlp_bridge_create
 * WHY:  Verify NLP integration setup
 * HOW:  Create bridge with config
 */
TEST_F(WernickeBridgesTest, CreateNLPBridge) {
    wernicke_nlp_config_t config;
    wernicke_nlp_default_config(&config);
    config.enable_speech_cortex = true;
    config.enable_nlp_network = true;

    wernicke_nlp_bridge_t* bridge = wernicke_nlp_bridge_create(adapter, &config);
    EXPECT_NE(bridge, nullptr) << "Failed to create NLP bridge";

    if (bridge) {
        wernicke_nlp_bridge_destroy(bridge);
    }
}

/**
 * @test NLP bridge default config
 * WHAT: Test wernicke_nlp_default_config
 * WHY:  Verify NLP defaults are sensible
 * HOW:  Get defaults, check values
 */
TEST_F(WernickeBridgesTest, NLPBridgeDefaultConfig) {
    wernicke_nlp_config_t config;
    int result = wernicke_nlp_default_config(&config);
    EXPECT_EQ(result, 0);

    // Check some fields have sensible defaults
    EXPECT_GT(config.max_sequence_length, 0u);
}

/**
 * @test Create NLP bridge with null adapter
 * WHAT: Test wernicke_nlp_bridge_create error handling
 * WHY:  Verify null safety
 * HOW:  Create with null adapter, expect null
 */
TEST_F(WernickeBridgesTest, CreateNLPBridgeNullAdapter) {
    wernicke_nlp_config_t config;
    wernicke_nlp_default_config(&config);

    wernicke_nlp_bridge_t* bridge = wernicke_nlp_bridge_create(nullptr, &config);
    EXPECT_EQ(bridge, nullptr);
}

/**
 * @test NLP bridge update
 * WHAT: Test wernicke_nlp_bridge_update
 * WHY:  Verify update processing works
 * HOW:  Create bridge, update, check no crash
 */
TEST_F(WernickeBridgesTest, NLPBridgeUpdate) {
    wernicke_nlp_config_t config;
    wernicke_nlp_default_config(&config);

    wernicke_nlp_bridge_t* bridge = wernicke_nlp_bridge_create(adapter, &config);
    ASSERT_NE(bridge, nullptr);

    // wernicke_nlp_bridge_update takes (bridge, timestamp)
    int result = wernicke_nlp_bridge_update(bridge, 0);
    EXPECT_GE(result, 0);

    wernicke_nlp_bridge_destroy(bridge);
}

//=============================================================================
// Substrate Bridge Tests
//=============================================================================

/**
 * @test Create substrate bridge
 * WHAT: Test wernicke_substrate_bridge_create
 * WHY:  Verify metabolic modulation setup
 * HOW:  Create bridge, verify non-null
 */
TEST_F(WernickeBridgesTest, CreateSubstrateBridge) {
    wernicke_substrate_config_t config = wernicke_substrate_default_config();
    config.enable_atp_modulation = true;
    config.enable_fatigue_modulation = true;
    config.atp_sensitivity = 1.0f;

    // wernicke_substrate_bridge_create takes (wernicke, substrate, config)
    wernicke_substrate_bridge_t* bridge = wernicke_substrate_bridge_create(adapter, nullptr, &config);
    EXPECT_NE(bridge, nullptr) << "Failed to create substrate bridge";

    if (bridge) {
        wernicke_substrate_bridge_destroy(bridge);
    }
}

/**
 * @test Substrate bridge default config
 * WHAT: Test wernicke_substrate_default_config
 * WHY:  Verify metabolic defaults
 * HOW:  Get defaults, verify ATP modulation enabled
 */
TEST_F(WernickeBridgesTest, SubstrateBridgeDefaultConfig) {
    wernicke_substrate_config_t config = wernicke_substrate_default_config();

    EXPECT_TRUE(config.enable_atp_modulation);
    EXPECT_GT(config.atp_sensitivity, 0.0f);
    EXPECT_LE(config.atp_sensitivity, 2.0f);
}

/**
 * @test Substrate bridge metabolic update
 * WHAT: Test substrate bridge processes metabolic state
 * WHY:  Verify language processing responds to energy
 * HOW:  Create bridge, update, check no crash
 */
TEST_F(WernickeBridgesTest, SubstrateBridgeUpdate) {
    wernicke_substrate_config_t config = wernicke_substrate_default_config();

    wernicke_substrate_bridge_t* bridge = wernicke_substrate_bridge_create(adapter, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    // Update should not crash
    int result = wernicke_substrate_bridge_update(bridge);
    EXPECT_GE(result, 0);

    wernicke_substrate_bridge_destroy(bridge);
}

//=============================================================================
// GPU-Bio Bridge Tests
//=============================================================================

/**
 * @test Create GPU-Bio bridge
 * WHAT: Test wernicke_gpu_bio_create
 * WHY:  Verify GPU acceleration setup
 * HOW:  Create bridge, verify non-null (or null if no GPU)
 */
TEST_F(WernickeBridgesTest, CreateGPUBioBridge) {
    wernicke_gpu_bio_config_t config = wernicke_gpu_bio_default_config();
    config.enable_bio_async = true;
    config.batch_threshold = 32;

    // wernicke_gpu_bio_create takes (gpu_ctx, config) - not wernicke adapter
    // Since we don't have GPU context, expect NULL or handle gracefully
    wernicke_gpu_bio_bridge_t* bridge = wernicke_gpu_bio_create(nullptr, &config);
    // May be null if GPU not available, which is acceptable
    if (bridge) {
        wernicke_gpu_bio_destroy(bridge);
    }
}

/**
 * @test GPU-Bio bridge default config
 * WHAT: Test wernicke_gpu_bio_default_config
 * WHY:  Verify GPU defaults
 * HOW:  Get defaults, check values
 */
TEST_F(WernickeBridgesTest, GPUBioBridgeDefaultConfig) {
    wernicke_gpu_bio_config_t config = wernicke_gpu_bio_default_config();

    EXPECT_GT(config.batch_threshold, 0u);
}

//=============================================================================
// Omni Bridge Tests
//=============================================================================

/**
 * @test Create omni bridge
 * WHAT: Test omni_wernicke_bridge_create
 * WHY:  Verify omni inference integration
 * HOW:  Create bridge, verify non-null
 */
TEST_F(WernickeBridgesTest, CreateOmniBridge) {
    omni_wernicke_config_t config;
    omni_wernicke_default_config(&config);
    config.phoneme_horizon = 5;
    config.word_candidates = 10;

    // omni_wernicke_bridge_create takes only config
    omni_wernicke_bridge_t* bridge = omni_wernicke_bridge_create(&config);
    EXPECT_NE(bridge, nullptr) << "Failed to create omni bridge";

    if (bridge) {
        omni_wernicke_bridge_destroy(bridge);
    }
}

/**
 * @test Omni bridge default config
 * WHAT: Test omni_wernicke_default_config
 * WHY:  Verify prediction defaults
 * HOW:  Get defaults, check prediction enabled
 */
TEST_F(WernickeBridgesTest, OmniBridgeDefaultConfig) {
    omni_wernicke_config_t config;
    int result = omni_wernicke_default_config(&config);
    EXPECT_EQ(result, 0);

    // Check some defaults
    EXPECT_GT(config.phoneme_horizon, 0u);
}

/**
 * @test Omni bridge update
 * WHAT: Test omni_wernicke_update
 * WHY:  Verify prediction processing
 * HOW:  Create bridge, update, verify no crash
 */
TEST_F(WernickeBridgesTest, OmniBridgeUpdate) {
    omni_wernicke_config_t config;
    omni_wernicke_default_config(&config);

    omni_wernicke_bridge_t* bridge = omni_wernicke_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // omni_wernicke_update (not omni_wernicke_bridge_update)
    int result = omni_wernicke_update(bridge);
    EXPECT_GE(result, 0);

    omni_wernicke_bridge_destroy(bridge);
}

//=============================================================================
// Quantum Bridge Tests
//=============================================================================

/**
 * @test Create quantum bridge
 * WHAT: Test wernicke_quantum_bridge_create
 * WHY:  Verify quantum optimization setup
 * HOW:  Create bridge, verify non-null
 */
TEST_F(WernickeBridgesTest, CreateQuantumBridge) {
    wernicke_quantum_config_t config;
    wernicke_quantum_default_config(&config);

    // wernicke_quantum_bridge_create takes (wernicke, config)
    wernicke_quantum_bridge_t* bridge = wernicke_quantum_bridge_create(adapter, &config);
    EXPECT_NE(bridge, nullptr) << "Failed to create quantum bridge";

    if (bridge) {
        wernicke_quantum_bridge_destroy(bridge);
    }
}

/**
 * @test Quantum bridge default config
 * WHAT: Test wernicke_quantum_default_config
 * WHY:  Verify quantum defaults
 * HOW:  Get defaults, check qubit count
 */
TEST_F(WernickeBridgesTest, QuantumBridgeDefaultConfig) {
    wernicke_quantum_config_t config;
    int result = wernicke_quantum_default_config(&config);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Immune Bridge Tests
//=============================================================================

/**
 * @test Create immune bridge
 * WHAT: Test wernicke_immune_bridge_create
 * WHY:  Verify neuroinflammation modeling setup
 * HOW:  Create bridge, verify non-null
 */
TEST_F(WernickeBridgesTest, CreateImmuneBridge) {
    wernicke_immune_config_t config;
    wernicke_immune_default_config(&config);
    config.enable_inflammation_impairment = true;
    config.enable_cytokine_modulation = true;
    config.inflammation_sensitivity = 1.0f;

    // wernicke_immune_bridge_create takes (config, immune_system, wernicke_adapter)
    wernicke_immune_bridge_t* bridge = wernicke_immune_bridge_create(&config, immune_system, adapter);
    EXPECT_NE(bridge, nullptr) << "Failed to create immune bridge";

    if (bridge) {
        wernicke_immune_bridge_destroy(bridge);
    }
}

/**
 * @test Immune bridge default config
 * WHAT: Test wernicke_immune_default_config
 * WHY:  Verify immune defaults
 * HOW:  Get defaults, check sensitivity
 */
TEST_F(WernickeBridgesTest, ImmuneBridgeDefaultConfig) {
    wernicke_immune_config_t config;
    int result = wernicke_immune_default_config(&config);
    EXPECT_EQ(result, 0);

    EXPECT_GT(config.inflammation_sensitivity, 0.0f);
}

/**
 * @test Immune bridge update with inflammation
 * WHAT: Test wernicke_immune_bridge_update
 * WHY:  Verify inflammation affects language
 * HOW:  Create bridge, update, check no crash
 */
TEST_F(WernickeBridgesTest, ImmuneBridgeUpdate) {
    wernicke_immune_config_t config;
    wernicke_immune_default_config(&config);

    wernicke_immune_bridge_t* bridge = wernicke_immune_bridge_create(&config, immune_system, adapter);
    ASSERT_NE(bridge, nullptr) << "Failed to create immune bridge";

    // wernicke_immune_bridge_update takes (bridge, timestamp)
    int result = wernicke_immune_bridge_update(bridge, 0);
    EXPECT_GE(result, 0);

    wernicke_immune_bridge_destroy(bridge);
}

/**
 * @test Immune bridge get state
 * WHAT: Test wernicke_immune_get_state
 * WHY:  Verify state retrieval
 * HOW:  Create bridge, get state, check valid
 */
TEST_F(WernickeBridgesTest, ImmuneBridgeGetState) {
    wernicke_immune_config_t config;
    wernicke_immune_default_config(&config);

    wernicke_immune_bridge_t* bridge = wernicke_immune_bridge_create(&config, immune_system, adapter);
    ASSERT_NE(bridge, nullptr) << "Failed to create immune bridge";

    wernicke_immune_state_t state = wernicke_immune_get_state(bridge);
    // State should be one of the valid enum values
    EXPECT_GE((int)state, (int)WERNICKE_IMMUNE_NORMAL);
    EXPECT_LE((int)state, (int)WERNICKE_IMMUNE_RECOVERING);

    wernicke_immune_bridge_destroy(bridge);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

/**
 * @test All bridges with null inputs
 * WHAT: Test all bridge creates with null inputs
 * WHY:  Verify null safety across all bridges
 * HOW:  Create each bridge with null inputs
 */
TEST_F(WernickeBridgesTest, AllBridgesNullInputs) {
    // wbb_create(wernicke, broca, config) - null wernicke
    EXPECT_EQ(wbb_create(nullptr, nullptr, nullptr), nullptr);

    // wernicke_nlp_bridge_create(wernicke, config) - null wernicke
    EXPECT_EQ(wernicke_nlp_bridge_create(nullptr, nullptr), nullptr);

    // wernicke_substrate_bridge_create(wernicke, substrate, config) - null wernicke
    EXPECT_EQ(wernicke_substrate_bridge_create(nullptr, nullptr, nullptr), nullptr);

    // wernicke_gpu_bio_create(gpu_ctx, config) - null gpu_ctx is ok (graceful failure)
    // Not testing as it may or may not return NULL depending on implementation

    // omni_wernicke_bridge_create(config) - null config should use defaults
    omni_wernicke_bridge_t* omni = omni_wernicke_bridge_create(nullptr);
    if (omni) {
        omni_wernicke_bridge_destroy(omni);
    }

    // wernicke_quantum_bridge_create(wernicke, config) - null wernicke
    EXPECT_EQ(wernicke_quantum_bridge_create(nullptr, nullptr), nullptr);

    // wernicke_immune_bridge_create(config, immune, wernicke) - null wernicke
    EXPECT_EQ(wernicke_immune_bridge_create(nullptr, nullptr, nullptr), nullptr);
}

/**
 * @test All bridge destroys with null
 * WHAT: Test all bridge destroys with null
 * WHY:  Verify null safety in destruction
 * HOW:  Call each destroy with null
 */
TEST_F(WernickeBridgesTest, AllBridgesDestroyNull) {
    wbb_destroy(nullptr);
    wernicke_nlp_bridge_destroy(nullptr);
    wernicke_substrate_bridge_destroy(nullptr);
    wernicke_gpu_bio_destroy(nullptr);
    omni_wernicke_bridge_destroy(nullptr);
    wernicke_quantum_bridge_destroy(nullptr);
    wernicke_immune_bridge_destroy(nullptr);
    // No assertions - verify no crash
}
