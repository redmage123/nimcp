/**
 * @file test_claustrum_brain_init_integration.cpp
 * @brief Integration tests for Claustrum brain initialization system
 *
 * WHAT: Tests Claustrum integration with brain factory initialization
 * WHY:  Ensure proper lifecycle management and brain system integration
 * HOW:  Test initialization, configuration, reset, and destruction
 *
 * INTEGRATION POINTS:
 * - Brain factory registration
 * - Configuration propagation
 * - Lifecycle callbacks
 * - Bio-async bridge initialization
 * - KG wiring setup
 * - Security registration
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/claustrum/nimcp_claustrum.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "nimcp.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ClaustrumBrainInitTest : public ::testing::Test {
protected:
    nimcp_claustrum_t claustrum;
    nimcp_claustrum_config_t config;
    bool router_initialized;

    void SetUp() override {
        router_initialized = false;
        memset(&claustrum, 0, sizeof(claustrum));

        /* Initialize bio-async router for integration testing */
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        /* Get default configuration */
        config = nimcp_claustrum_default_config();
    }

    void TearDown() override {
        if (claustrum.initialized) {
            nimcp_claustrum_shutdown(&claustrum);
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(ClaustrumBrainInitTest, InitWithDefaultConfig) {
    nimcp_claustrum_error_t err = nimcp_claustrum_init(&claustrum, NULL);
    EXPECT_EQ(CLAUSTRUM_OK, err);
    EXPECT_TRUE(claustrum.initialized);
}

TEST_F(ClaustrumBrainInitTest, InitWithCustomConfig) {
    config.binding_threshold = 0.7f;
    config.salience_threshold = 0.6f;
    config.temporal_window_ms = 75.0f;
    config.enable_workspace_gating = true;
    config.enable_rapid_switching = true;

    nimcp_claustrum_error_t err = nimcp_claustrum_init(&claustrum, &config);
    EXPECT_EQ(CLAUSTRUM_OK, err);
    EXPECT_TRUE(claustrum.initialized);

    /* Verify config was applied */
    EXPECT_FLOAT_EQ(claustrum.config.binding_threshold, 0.7f);
    EXPECT_FLOAT_EQ(claustrum.config.salience_threshold, 0.6f);
    EXPECT_FLOAT_EQ(claustrum.config.temporal_window_ms, 75.0f);
    EXPECT_TRUE(claustrum.config.enable_workspace_gating);
    EXPECT_TRUE(claustrum.config.enable_rapid_switching);
}

TEST_F(ClaustrumBrainInitTest, InitRejectsNull) {
    nimcp_claustrum_error_t err = nimcp_claustrum_init(NULL, &config);
    EXPECT_EQ(CLAUSTRUM_ERR_NULL_PTR, err);
}

TEST_F(ClaustrumBrainInitTest, InitRejectsDoubleInit) {
    nimcp_claustrum_error_t err = nimcp_claustrum_init(&claustrum, &config);
    EXPECT_EQ(CLAUSTRUM_OK, err);

    err = nimcp_claustrum_init(&claustrum, &config);
    EXPECT_EQ(CLAUSTRUM_ERR_ALREADY_INITIALIZED, err);
}

TEST_F(ClaustrumBrainInitTest, ShutdownCleanup) {
    nimcp_claustrum_error_t err = nimcp_claustrum_init(&claustrum, &config);
    EXPECT_EQ(CLAUSTRUM_OK, err);

    err = nimcp_claustrum_shutdown(&claustrum);
    EXPECT_EQ(CLAUSTRUM_OK, err);
    EXPECT_FALSE(claustrum.initialized);
}

TEST_F(ClaustrumBrainInitTest, ShutdownRejectsNull) {
    nimcp_claustrum_error_t err = nimcp_claustrum_shutdown(NULL);
    EXPECT_EQ(CLAUSTRUM_ERR_NULL_PTR, err);
}

TEST_F(ClaustrumBrainInitTest, ShutdownRejectsUninit) {
    nimcp_claustrum_error_t err = nimcp_claustrum_shutdown(&claustrum);
    EXPECT_EQ(CLAUSTRUM_ERR_NOT_INITIALIZED, err);
}

TEST_F(ClaustrumBrainInitTest, ResetRestoresBaseline) {
    nimcp_claustrum_error_t err = nimcp_claustrum_init(&claustrum, &config);
    EXPECT_EQ(CLAUSTRUM_OK, err);

    /* Do some operations to change state */
    float features[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, features, 8, 0.8f);
    nimcp_claustrum_update(&claustrum, 10.0f);

    /* Reset */
    err = nimcp_claustrum_reset(&claustrum);
    EXPECT_EQ(CLAUSTRUM_OK, err);
    EXPECT_TRUE(claustrum.initialized);
    EXPECT_EQ(CLAUSTRUM_STATE_IDLE, nimcp_claustrum_get_state(&claustrum));
}

TEST_F(ClaustrumBrainInitTest, MultipleInitShutdownCycles) {
    for (int i = 0; i < 5; i++) {
        nimcp_claustrum_error_t err = nimcp_claustrum_init(&claustrum, &config);
        EXPECT_EQ(CLAUSTRUM_OK, err) << "Cycle " << i << " init failed";
        EXPECT_TRUE(claustrum.initialized);

        err = nimcp_claustrum_shutdown(&claustrum);
        EXPECT_EQ(CLAUSTRUM_OK, err) << "Cycle " << i << " shutdown failed";
        EXPECT_FALSE(claustrum.initialized);
    }
}

/*=============================================================================
 * INITIAL STATE TESTS
 *===========================================================================*/

TEST_F(ClaustrumBrainInitTest, InitialStateIsIdle) {
    nimcp_claustrum_init(&claustrum, &config);

    EXPECT_EQ(CLAUSTRUM_STATE_IDLE, nimcp_claustrum_get_state(&claustrum));
    EXPECT_EQ(CLAUSTRUM_STATUS_NORMAL, nimcp_claustrum_get_status(&claustrum));
    EXPECT_EQ(CLAUSTRUM_BRAIN_STATE_DEFAULT, nimcp_claustrum_get_brain_state(&claustrum));
}

TEST_F(ClaustrumBrainInitTest, InitialModalitiesInactive) {
    nimcp_claustrum_init(&claustrum, &config);

    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        nimcp_claustrum_modality_input_t input;
        nimcp_claustrum_error_t err = nimcp_claustrum_get_modality(&claustrum, (nimcp_claustrum_modality_t)i, &input);
        EXPECT_EQ(CLAUSTRUM_OK, err);
        EXPECT_FALSE(input.active);
        EXPECT_FALSE(input.bound);
    }
}

TEST_F(ClaustrumBrainInitTest, InitialPercepstEmpty) {
    nimcp_claustrum_init(&claustrum, &config);
    EXPECT_EQ(0u, claustrum.num_active_percepts);
}

TEST_F(ClaustrumBrainInitTest, InitialWorkspaceEmpty) {
    nimcp_claustrum_init(&claustrum, &config);

    bool occupied = true;
    uint32_t percept_id = 999;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_workspace_state(&claustrum, &occupied, &percept_id);
    EXPECT_EQ(CLAUSTRUM_OK, err);
    EXPECT_FALSE(occupied);
}

TEST_F(ClaustrumBrainInitTest, InitialMetricsZero) {
    nimcp_claustrum_init(&claustrum, &config);

    nimcp_claustrum_metrics_t metrics;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_metrics(&claustrum, &metrics);
    EXPECT_EQ(CLAUSTRUM_OK, err);

    EXPECT_EQ(0u, metrics.total_bindings);
    EXPECT_EQ(0u, metrics.successful_bindings);
    EXPECT_EQ(0u, metrics.failed_bindings);
    EXPECT_EQ(0u, metrics.salience_detections);
    EXPECT_EQ(0u, metrics.workspace_accesses);
    EXPECT_EQ(0u, metrics.state_switches);
    EXPECT_EQ(0u, metrics.update_count);
}

/*=============================================================================
 * CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(ClaustrumBrainInitTest, OscillationParameterConfig) {
    config.gamma_base_freq = 50.0f;
    config.alpha_base_freq = 12.0f;
    config.oscillation_coupling = 0.8f;

    nimcp_claustrum_init(&claustrum, &config);

    EXPECT_FLOAT_EQ(claustrum.config.gamma_base_freq, 50.0f);
    EXPECT_FLOAT_EQ(claustrum.config.alpha_base_freq, 12.0f);
    EXPECT_FLOAT_EQ(claustrum.config.oscillation_coupling, 0.8f);
}

TEST_F(ClaustrumBrainInitTest, WorkspaceGatingConfig) {
    config.enable_workspace_gating = true;
    config.workspace_threshold = 0.7f;
    config.broadcast_duration_ms = 200.0f;

    nimcp_claustrum_init(&claustrum, &config);

    EXPECT_TRUE(claustrum.config.enable_workspace_gating);
    EXPECT_FLOAT_EQ(claustrum.config.workspace_threshold, 0.7f);
    EXPECT_FLOAT_EQ(claustrum.config.broadcast_duration_ms, 200.0f);
}

TEST_F(ClaustrumBrainInitTest, TaskSwitchingConfig) {
    config.switch_threshold = 0.6f;
    config.switch_duration_ms = 150.0f;
    config.enable_rapid_switching = true;

    nimcp_claustrum_init(&claustrum, &config);

    EXPECT_FLOAT_EQ(claustrum.config.switch_threshold, 0.6f);
    EXPECT_FLOAT_EQ(claustrum.config.switch_duration_ms, 150.0f);
    EXPECT_TRUE(claustrum.config.enable_rapid_switching);
}

TEST_F(ClaustrumBrainInitTest, IntegrationFeatureFlags) {
    config.enable_immune_reporting = true;
    config.enable_logging = false;
    config.enable_kg_integration = true;
    config.enable_snn_output = true;

    nimcp_claustrum_init(&claustrum, &config);

    EXPECT_TRUE(claustrum.config.enable_immune_reporting);
    EXPECT_FALSE(claustrum.config.enable_logging);
    EXPECT_TRUE(claustrum.config.enable_kg_integration);
    EXPECT_TRUE(claustrum.config.enable_snn_output);
}

/*=============================================================================
 * CORTICAL LINK INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(ClaustrumBrainInitTest, CorticalLinksInitialized) {
    nimcp_claustrum_init(&claustrum, &config);

    for (int i = 0; i < CLAUSTRUM_REGION_COUNT; i++) {
        nimcp_claustrum_cortical_link_t link;
        nimcp_claustrum_error_t err = nimcp_claustrum_get_cortical_link(&claustrum, (nimcp_claustrum_region_t)i, &link);
        EXPECT_EQ(CLAUSTRUM_OK, err);
        EXPECT_EQ((nimcp_claustrum_region_t)i, link.region);
    }
}

TEST_F(ClaustrumBrainInitTest, CorticalLinkStrengthConfiguration) {
    nimcp_claustrum_init(&claustrum, &config);

    /* Configure bidirectional strength for prefrontal */
    nimcp_claustrum_error_t err = nimcp_claustrum_set_cortical_link_strength(
        &claustrum, CLAUSTRUM_REGION_PREFRONTAL, 0.9f, 0.8f);
    EXPECT_EQ(CLAUSTRUM_OK, err);

    nimcp_claustrum_cortical_link_t link;
    err = nimcp_claustrum_get_cortical_link(&claustrum, CLAUSTRUM_REGION_PREFRONTAL, &link);
    EXPECT_EQ(CLAUSTRUM_OK, err);
    EXPECT_FLOAT_EQ(0.9f, link.forward_strength);
    EXPECT_FLOAT_EQ(0.8f, link.backward_strength);
}

/*=============================================================================
 * OSCILLATOR INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(ClaustrumBrainInitTest, OscillatorInitialized) {
    nimcp_claustrum_init(&claustrum, &config);

    float coherence = -1.0f;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_sync_level(&claustrum, &coherence);
    EXPECT_EQ(CLAUSTRUM_OK, err);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(ClaustrumBrainInitTest, GammaParameterConfiguration) {
    nimcp_claustrum_init(&claustrum, &config);

    nimcp_claustrum_error_t err = nimcp_claustrum_set_gamma(&claustrum, 45.0f, 0.8f);
    EXPECT_EQ(CLAUSTRUM_OK, err);

    EXPECT_FLOAT_EQ(45.0f, claustrum.oscillator.gamma_frequency);
    EXPECT_FLOAT_EQ(0.8f, claustrum.oscillator.gamma_amplitude);
}

TEST_F(ClaustrumBrainInitTest, AlphaParameterConfiguration) {
    nimcp_claustrum_init(&claustrum, &config);

    nimcp_claustrum_error_t err = nimcp_claustrum_set_alpha(&claustrum, 11.0f, 0.7f);
    EXPECT_EQ(CLAUSTRUM_OK, err);

    EXPECT_FLOAT_EQ(11.0f, claustrum.oscillator.alpha_frequency);
    EXPECT_FLOAT_EQ(0.7f, claustrum.oscillator.alpha_amplitude);
}

/*=============================================================================
 * CALLBACK CONFIGURATION TESTS
 *===========================================================================*/

static bool g_binding_callback_invoked = false;
static bool g_state_callback_invoked = false;
static bool g_consciousness_callback_invoked = false;
static bool g_workspace_callback_invoked = false;

static void test_binding_callback(nimcp_claustrum_t*, const nimcp_claustrum_bound_percept_t*, void*) {
    g_binding_callback_invoked = true;
}

static void test_state_callback(nimcp_claustrum_t*, nimcp_claustrum_brain_state_t, nimcp_claustrum_brain_state_t, void*) {
    g_state_callback_invoked = true;
}

static void test_consciousness_callback(nimcp_claustrum_t*, nimcp_claustrum_consciousness_level_t, float, void*) {
    g_consciousness_callback_invoked = true;
}

static void test_workspace_callback(nimcp_claustrum_t*, const void*, size_t, float, void*) {
    g_workspace_callback_invoked = true;
}

TEST_F(ClaustrumBrainInitTest, CallbackConfiguration) {
    g_binding_callback_invoked = false;
    g_state_callback_invoked = false;
    g_consciousness_callback_invoked = false;
    g_workspace_callback_invoked = false;

    config.on_binding = test_binding_callback;
    config.on_state_change = test_state_callback;
    config.on_consciousness_change = test_consciousness_callback;
    config.on_workspace_broadcast = test_workspace_callback;
    config.callback_data = (void*)0x12345678;

    nimcp_claustrum_init(&claustrum, &config);

    EXPECT_EQ(test_binding_callback, claustrum.config.on_binding);
    EXPECT_EQ(test_state_callback, claustrum.config.on_state_change);
    EXPECT_EQ(test_consciousness_callback, claustrum.config.on_consciousness_change);
    EXPECT_EQ(test_workspace_callback, claustrum.config.on_workspace_broadcast);
    EXPECT_EQ((void*)0x12345678, claustrum.config.callback_data);
}

/*=============================================================================
 * UTILITY FUNCTION TESTS
 *===========================================================================*/

TEST_F(ClaustrumBrainInitTest, ErrorStringFunction) {
    EXPECT_NE(nullptr, nimcp_claustrum_error_string(CLAUSTRUM_OK));
    EXPECT_NE(nullptr, nimcp_claustrum_error_string(CLAUSTRUM_ERR_NULL_PTR));
    EXPECT_NE(nullptr, nimcp_claustrum_error_string(CLAUSTRUM_ERR_BINDING_FAILED));
}

TEST_F(ClaustrumBrainInitTest, ModalityStringFunction) {
    EXPECT_NE(nullptr, nimcp_claustrum_modality_string(CLAUSTRUM_MODALITY_VISUAL));
    EXPECT_NE(nullptr, nimcp_claustrum_modality_string(CLAUSTRUM_MODALITY_AUDITORY));
    EXPECT_NE(nullptr, nimcp_claustrum_modality_string(CLAUSTRUM_MODALITY_SOMATOSENSORY));
}

TEST_F(ClaustrumBrainInitTest, StateStringFunction) {
    EXPECT_NE(nullptr, nimcp_claustrum_state_string(CLAUSTRUM_STATE_IDLE));
    EXPECT_NE(nullptr, nimcp_claustrum_state_string(CLAUSTRUM_STATE_BINDING));
    EXPECT_NE(nullptr, nimcp_claustrum_state_string(CLAUSTRUM_STATE_BROADCASTING));
}

TEST_F(ClaustrumBrainInitTest, BrainStateStringFunction) {
    EXPECT_NE(nullptr, nimcp_claustrum_brain_state_string(CLAUSTRUM_BRAIN_STATE_DEFAULT));
    EXPECT_NE(nullptr, nimcp_claustrum_brain_state_string(CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE));
    EXPECT_NE(nullptr, nimcp_claustrum_brain_state_string(CLAUSTRUM_BRAIN_STATE_SALIENCE));
}

TEST_F(ClaustrumBrainInitTest, RegionStringFunction) {
    EXPECT_NE(nullptr, nimcp_claustrum_region_string(CLAUSTRUM_REGION_PREFRONTAL));
    EXPECT_NE(nullptr, nimcp_claustrum_region_string(CLAUSTRUM_REGION_CINGULATE));
    EXPECT_NE(nullptr, nimcp_claustrum_region_string(CLAUSTRUM_REGION_INSULA));
}

TEST_F(ClaustrumBrainInitTest, ConsciousnessStringFunction) {
    EXPECT_NE(nullptr, nimcp_claustrum_consciousness_string(CLAUSTRUM_CONSCIOUSNESS_UNCONSCIOUS));
    EXPECT_NE(nullptr, nimcp_claustrum_consciousness_string(CLAUSTRUM_CONSCIOUSNESS_CONSCIOUS));
    EXPECT_NE(nullptr, nimcp_claustrum_consciousness_string(CLAUSTRUM_CONSCIOUSNESS_FOCAL));
}

TEST_F(ClaustrumBrainInitTest, BioMsgTypeStringFunction) {
    EXPECT_NE(nullptr, nimcp_claustrum_bio_msg_type_string(CLAUSTRUM_BIO_MSG_BINDING));
    EXPECT_NE(nullptr, nimcp_claustrum_bio_msg_type_string(CLAUSTRUM_BIO_MSG_SYNC));
    EXPECT_NE(nullptr, nimcp_claustrum_bio_msg_type_string(CLAUSTRUM_BIO_MSG_WORKSPACE_GATE));
}

/*=============================================================================
 * DEFAULT CONFIG TESTS
 *===========================================================================*/

TEST_F(ClaustrumBrainInitTest, DefaultConfigReasonableValues) {
    nimcp_claustrum_config_t def = nimcp_claustrum_default_config();

    /* Binding parameters */
    EXPECT_GT(def.binding_threshold, 0.0f);
    EXPECT_LE(def.binding_threshold, 1.0f);
    EXPECT_GT(def.temporal_window_ms, 0.0f);

    /* Salience parameters */
    EXPECT_GT(def.salience_threshold, 0.0f);
    EXPECT_LE(def.salience_threshold, 1.0f);

    /* Oscillation parameters - gamma is 30-100 Hz biologically */
    EXPECT_GE(def.gamma_base_freq, 30.0f);
    EXPECT_LE(def.gamma_base_freq, 100.0f);

    /* Alpha is 8-12 Hz biologically */
    EXPECT_GE(def.alpha_base_freq, 8.0f);
    EXPECT_LE(def.alpha_base_freq, 12.0f);

    /* Workspace threshold */
    EXPECT_GT(def.workspace_threshold, 0.0f);
    EXPECT_LE(def.workspace_threshold, 1.0f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
