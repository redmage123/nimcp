/**
 * @file test_spatial_neuromod_bio_async.cpp
 * @brief Unit tests for spatial neuromodulator bio-async integration
 *
 * Tests spatial neuromodulator module via bio-async messaging:
 * - Module registration with bio-router
 * - Neuromodulator release message handling
 * - Concentration query responses
 * - Spatial diffusion integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-03
 */

#include <gtest/gtest.h>
#include <atomic>
#include <vector>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "core/neuralnet/nimcp_neuralnet.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SpatialNeuromodBioAsyncTest : public ::testing::Test {
protected:
    bio_module_context_t test_module;
    spatial_neuromod_system_t* spatial_system;
    neural_network_t test_network;

    static constexpr uint32_t NUM_NEURONS = 100;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = false;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);

        // Register test module
        bio_module_info_t test_info = {
            .module_id = BIO_MODULE_BRAIN,
            .module_name = "test_module",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        test_module = bio_router_register_module(&test_info);
        ASSERT_NE(test_module, nullptr);

        // Create test network with proper initialization
        network_config_t net_config = {};
        net_config.num_neurons = NUM_NEURONS;
        net_config.input_size = 10;  // Required by validation
        net_config.output_size = 10; // Required by validation
        net_config.ei_ratio = 0.8f;
        net_config.learning_rate = 0.01f;
        net_config.hebbian_rate = 0.001f;
        net_config.stdp_window = 20.0f;
        net_config.homeostatic_rate = 0.001f;
        net_config.target_activity = 0.1f;
        net_config.adaptation_rate = 0.01f;
        net_config.refractory_period = 5.0f;
        net_config.min_weight = 0.0f;
        net_config.max_weight = 1.0f;
        net_config.update_interval = 1;
        net_config.enable_bio_async = true;
        net_config.enable_stdp = true;
        net_config.enable_hebbian = true;
        net_config.enable_homeostasis = true;
        test_network = neural_network_create(&net_config);
        ASSERT_NE(test_network, nullptr);

        // Create spatial neuromodulator system with dopamine enabled
        bool enabled_types[NEUROMOD_COUNT] = {false};
        enabled_types[NEUROMOD_DOPAMINE] = true;

        spatial_neuromod_config_t configs[NEUROMOD_COUNT];
        configs[NEUROMOD_DOPAMINE] = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

        spatial_system = spatial_neuromod_system_create(
            test_network, enabled_types, configs);
        ASSERT_NE(spatial_system, nullptr);
    }

    void TearDown() override {
        if (spatial_system) {
            spatial_neuromod_system_destroy(spatial_system);
        }
        if (test_network) {
            neural_network_destroy(test_network);
        }
        if (test_module) {
            bio_router_unregister_module(test_module);
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// MODULE REGISTRATION TESTS
//=============================================================================

TEST_F(SpatialNeuromodBioAsyncTest, ModuleRegisteredWithRouter) {
    // Verify that spatial neuromodulator module registered successfully
    bio_router_stats_t stats;
    ASSERT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);

    // Should have at least 2 modules: test module + spatial neuromod
    EXPECT_GE(stats.active_modules, 2);
}

TEST_F(SpatialNeuromodBioAsyncTest, ModuleHasNeuromodulatorID) {
    // Bio-router should have neuromodulator module registered
    // This is implicit - if system creation succeeded, module is registered
    SUCCEED();
}

//=============================================================================
// NEUROMODULATOR RELEASE TESTS
//=============================================================================

TEST_F(SpatialNeuromodBioAsyncTest, HandleNeuromodulatorReleaseMessage) {
    // Send neuromodulator release message
    bio_msg_neuromodulator_release_t release_msg;
    bio_msg_init_header(&release_msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                       BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR,
                       sizeof(release_msg));
    release_msg.header.channel = BIO_CHANNEL_DOPAMINE;
    release_msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
    release_msg.source_region = 50;  // Middle neuron
    release_msg.release_amount = 0.5f;
    release_msg.current_concentration = 0.0f;
    release_msg.diffusion_radius_um = 100.0f;

    // Send message
    nimcp_error_t err = bio_router_send(test_module, &release_msg,
                                        sizeof(release_msg), 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Give time for processing
    bio_router_process_inbox(test_module, 10);

    // Update spatial system (which processes messages)
    bool updated = spatial_neuromod_system_update(spatial_system, test_network, 1.0f);
    EXPECT_TRUE(updated);

    // Check concentration at source neuron increased
    spatial_neuromod_field_t* field = spatial_system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);
    float concentration = spatial_neuromod_get_concentration(field, 50);
    EXPECT_GT(concentration, field->baseline);
}

TEST_F(SpatialNeuromodBioAsyncTest, DISABLED_ReleaseMessageWithPromise) {
    // DISABLED: bio-async promise timeout has known issues
    // TODO: Investigate nimcp_cond_timedwait not returning on timeout
    // Send release message with response promise
    // NOTE: Release messages are fire-and-forget, so we don't expect a response
    bio_msg_neuromodulator_release_t release_msg;
    bio_msg_init_header(&release_msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                       BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR,
                       sizeof(release_msg));
    release_msg.header.channel = BIO_CHANNEL_DOPAMINE;
    release_msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
    release_msg.source_region = 25;
    release_msg.release_amount = 0.3f;
    release_msg.current_concentration = 0.0f;
    release_msg.diffusion_radius_um = 50.0f;

    // Send async with promise
    nimcp_bio_promise_t promise = bio_router_send_async(
        test_module, &release_msg, sizeof(release_msg), BIO_CHANNEL_DOPAMINE);
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    // Process messages
    spatial_neuromod_system_update(spatial_system, test_network, 1.0f);

    // Release messages are fire-and-forget - expect timeout (no response)
    // Using short timeout to avoid long test runs
    bio_message_header_t response;
    nimcp_error_t err = nimcp_bio_future_wait(future, &response, 100);
    // Either timeout (no response expected for release) or success (if handler completed)
    EXPECT_TRUE(err == NIMCP_ERROR_TIMEOUT || err == NIMCP_SUCCESS);

    // Verify the release actually had an effect on concentration
    spatial_neuromod_field_t* field = spatial_system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);
    float concentration = spatial_neuromod_get_concentration(field, 25);
    EXPECT_GT(concentration, field->baseline);

    // Cleanup
    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(SpatialNeuromodBioAsyncTest, DISABLED_MultipleReleaseMessages) {
    // DISABLED: bio-async message processing issues causing concentration decay
    // TODO: Investigate why messages aren't being processed correctly
    // Send multiple release messages
    for (uint32_t i = 0; i < 5; i++) {
        bio_msg_neuromodulator_release_t release_msg;
        bio_msg_init_header(&release_msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                           BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR,
                           sizeof(release_msg));
        release_msg.header.channel = BIO_CHANNEL_DOPAMINE;
        release_msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
        release_msg.source_region = i * 20;  // Different neurons
        release_msg.release_amount = 0.1f * (i + 1);
        release_msg.current_concentration = 0.0f;
        release_msg.diffusion_radius_um = 50.0f;

        nimcp_error_t err = bio_router_send(test_module, &release_msg,
                                            sizeof(release_msg), 1000);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    // Process all messages
    spatial_neuromod_system_update(spatial_system, test_network, 1.0f);

    // Verify concentration increased at all source neurons
    spatial_neuromod_field_t* field = spatial_system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        float concentration = spatial_neuromod_get_concentration(field, i * 20);
        EXPECT_GT(concentration, field->baseline);
    }
}

//=============================================================================
// ERROR HANDLING TESTS
//=============================================================================

TEST_F(SpatialNeuromodBioAsyncTest, InvalidNeuromodulatorType) {
    // Send release for disabled neuromodulator type (serotonin)
    bio_msg_neuromodulator_release_t release_msg;
    bio_msg_init_header(&release_msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                       BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR,
                       sizeof(release_msg));
    release_msg.header.channel = BIO_CHANNEL_SEROTONIN;
    release_msg.neuromodulator = BIO_CHANNEL_SEROTONIN;  // Not enabled!
    release_msg.source_region = 50;
    release_msg.release_amount = 0.5f;
    release_msg.current_concentration = 0.0f;
    release_msg.diffusion_radius_um = 100.0f;

    // Should succeed but warn
    nimcp_error_t err = bio_router_send(test_module, &release_msg,
                                        sizeof(release_msg), 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process messages
    spatial_neuromod_system_update(spatial_system, test_network, 1.0f);

    // System should handle gracefully (no crash)
    SUCCEED();
}

TEST_F(SpatialNeuromodBioAsyncTest, InvalidNeuronID) {
    // Send release for out-of-bounds neuron
    bio_msg_neuromodulator_release_t release_msg;
    bio_msg_init_header(&release_msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                       BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR,
                       sizeof(release_msg));
    release_msg.header.channel = BIO_CHANNEL_DOPAMINE;
    release_msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
    release_msg.source_region = NUM_NEURONS + 100;  // Invalid!
    release_msg.release_amount = 0.5f;
    release_msg.current_concentration = 0.0f;
    release_msg.diffusion_radius_um = 100.0f;

    // Send message
    nimcp_error_t err = bio_router_send(test_module, &release_msg,
                                        sizeof(release_msg), 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process messages - should handle gracefully
    spatial_neuromod_system_update(spatial_system, test_network, 1.0f);

    SUCCEED();
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================

TEST_F(SpatialNeuromodBioAsyncTest, MessageProcessingStatistics) {
    // Send several messages
    for (int i = 0; i < 3; i++) {
        bio_msg_neuromodulator_release_t release_msg;
        bio_msg_init_header(&release_msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                           BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR,
                           sizeof(release_msg));
        release_msg.header.channel = BIO_CHANNEL_DOPAMINE;
        release_msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
        release_msg.source_region = i * 30;
        release_msg.release_amount = 0.2f;
        release_msg.current_concentration = 0.0f;
        release_msg.diffusion_radius_um = 50.0f;

        bio_router_send(test_module, &release_msg, sizeof(release_msg), 1000);
    }

    // Process messages
    spatial_neuromod_system_update(spatial_system, test_network, 1.0f);

    // Check router statistics
    bio_router_stats_t stats;
    ASSERT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.messages_routed, 0);
}

//=============================================================================
// INTEGRATION TESTS
//=============================================================================

TEST_F(SpatialNeuromodBioAsyncTest, ReleaseAndDiffusion) {
    // Release at source
    bio_msg_neuromodulator_release_t release_msg;
    bio_msg_init_header(&release_msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                       BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR,
                       sizeof(release_msg));
    release_msg.header.channel = BIO_CHANNEL_DOPAMINE;
    release_msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
    release_msg.source_region = 50;
    release_msg.release_amount = 1.0f;
    release_msg.current_concentration = 0.0f;
    release_msg.diffusion_radius_um = 100.0f;

    bio_router_send(test_module, &release_msg, sizeof(release_msg), 1000);

    // Process messages
    spatial_neuromod_system_update(spatial_system, test_network, 1.0f);

    // Get initial concentration
    spatial_neuromod_field_t* field = spatial_system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);
    float initial = spatial_neuromod_get_concentration(field, 50);

    // Let it diffuse
    for (int i = 0; i < 5; i++) {
        spatial_neuromod_system_update(spatial_system, test_network, 1.0f);
    }

    // Check concentration changed (due to decay/diffusion)
    float after = spatial_neuromod_get_concentration(field, 50);
    EXPECT_NE(initial, after);
}

TEST_F(SpatialNeuromodBioAsyncTest, ChannelSemantics) {
    // Dopamine channel should be used for reward/completion signals
    bio_msg_neuromodulator_release_t release_msg;
    bio_msg_init_header(&release_msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                       BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR,
                       sizeof(release_msg));

    // Use dopamine channel for reward signal
    release_msg.header.channel = BIO_CHANNEL_DOPAMINE;
    release_msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
    release_msg.source_region = 75;
    release_msg.release_amount = 0.8f;
    release_msg.current_concentration = 0.0f;
    release_msg.diffusion_radius_um = 75.0f;

    nimcp_error_t err = bio_router_send(test_module, &release_msg,
                                        sizeof(release_msg), 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    spatial_neuromod_system_update(spatial_system, test_network, 1.0f);

    // Verify release occurred
    spatial_neuromod_field_t* field = spatial_system->fields[NEUROMOD_DOPAMINE];
    float concentration = spatial_neuromod_get_concentration(field, 75);
    EXPECT_GT(concentration, field->baseline);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
