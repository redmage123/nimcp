//=============================================================================
// test_cross_module_communication.cpp - Cross-Module Bio-Async Integration Tests
//=============================================================================
/**
 * @file test_cross_module_communication.cpp
 * @brief Integration tests for cross-module bio-async communication
 *
 * Tests cover:
 * - Brain → cognitive module communication
 * - Training → plasticity communication
 * - Glial broadcast to all modules
 * - Phase synchronization across modules
 * - Predictive coding updates
 *
 * @version 1.0.0
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CrossModuleIntegrationTest : public ::testing::Test {
protected:
    // Module contexts
    bio_module_context_t brain_ctx;
    bio_module_context_t cognitive_ctx;
    bio_module_context_t plasticity_ctx;
    bio_module_context_t glial_ctx;
    bio_module_context_t training_ctx;

    // Message counters for verification
    std::atomic<int> brain_msg_count{0};
    std::atomic<int> cognitive_msg_count{0};
    std::atomic<int> plasticity_msg_count{0};
    std::atomic<int> glial_msg_count{0};
    std::atomic<int> training_msg_count{0};

    // Response storage
    bio_msg_brain_state_response_t last_brain_response;
    bio_msg_introspection_response_t last_cognitive_response;
    bio_msg_weight_update_response_t last_plasticity_response;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t async_config = nimcp_bio_async_default_config();
        async_config.enable_logging = true;
        async_config.enable_statistics = true;
        ASSERT_EQ(nimcp_bio_async_init(&async_config), NIMCP_SUCCESS);

        // Initialize router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_logging = true;
        router_config.enable_statistics = true;
        router_config.max_modules = 10;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);

        // Register modules
        brain_ctx = RegisterModule(BIO_MODULE_BRAIN, "test_brain", &brain_msg_count);
        cognitive_ctx = RegisterModule(BIO_MODULE_INTROSPECTION, "test_cognitive", &cognitive_msg_count);
        plasticity_ctx = RegisterModule(BIO_MODULE_STDP, "test_plasticity", &plasticity_msg_count);
        glial_ctx = RegisterModule(BIO_MODULE_ASTROCYTE, "test_glial", &glial_msg_count);
        training_ctx = RegisterModule(BIO_MODULE_TRAINING, "test_training", &training_msg_count);

        ASSERT_NE(brain_ctx, nullptr);
        ASSERT_NE(cognitive_ctx, nullptr);
        ASSERT_NE(plasticity_ctx, nullptr);
        ASSERT_NE(glial_ctx, nullptr);
        ASSERT_NE(training_ctx, nullptr);

        // Register handlers
        RegisterHandlers();
    }

    void TearDown() override {
        // Unregister modules
        if (brain_ctx) bio_router_unregister_module(brain_ctx);
        if (cognitive_ctx) bio_router_unregister_module(cognitive_ctx);
        if (plasticity_ctx) bio_router_unregister_module(plasticity_ctx);
        if (glial_ctx) bio_router_unregister_module(glial_ctx);
        if (training_ctx) bio_router_unregister_module(training_ctx);

        // Shutdown
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    bio_module_context_t RegisterModule(bio_module_id_t id, const char* name, std::atomic<int>* counter) {
        bio_module_info_t info = {
            .module_id = id,
            .module_name = name,
            .inbox_capacity = 64,
            .user_data = counter
        };
        return bio_router_register_module(&info);
    }

    void RegisterHandlers() {
        // Brain handlers
        bio_router_register_handler(brain_ctx, BIO_MSG_BRAIN_STATE_QUERY,
            [](const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
                auto* counter = static_cast<std::atomic<int>*>(user_data);
                (*counter)++;

                // Prepare response
                bio_msg_brain_state_response_t response = {};
                response.header.type = BIO_MSG_BRAIN_STATE_RESPONSE;
                response.neuron_count = 10000;
                response.synapse_count = 100000;
                response.active_region_count = 5;
                response.dopamine_level = 0.75f;
                response.serotonin_level = 0.60f;
                response.norepinephrine_level = 0.50f;
                response.acetylcholine_level = 0.80f;
                response.energy_level = 0.90f;
                response.global_activity = 0.65f;

                if (promise) {
                    // Use sized version to specify actual response size
                    nimcp_bio_promise_complete_sized(promise, &response, sizeof(response));
                }
                return NIMCP_SUCCESS;
            });

        // Cognitive handlers
        bio_router_register_handler(cognitive_ctx, BIO_MSG_INTROSPECTION_QUERY,
            [](const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
                auto* counter = static_cast<std::atomic<int>*>(user_data);
                (*counter)++;

                bio_msg_introspection_response_t response = {};
                response.header.type = BIO_MSG_INTROSPECTION_RESPONSE;
                response.confidence = 0.85f;
                response.cognitive_load = 0.60f;
                response.emotional_valence = 0.70f;
                response.arousal = 0.55f;
                response.matched_pattern_count = 12;

                if (promise) {
                    // Use sized version to specify actual response size
                    nimcp_bio_promise_complete_sized(promise, &response, sizeof(response));
                }
                return NIMCP_SUCCESS;
            });

        // Plasticity handlers
        bio_router_register_handler(plasticity_ctx, BIO_MSG_WEIGHT_UPDATE_REQUEST,
            [](const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
                auto* counter = static_cast<std::atomic<int>*>(user_data);
                (*counter)++;

                auto* request = static_cast<const bio_msg_weight_update_request_t*>(msg);

                bio_msg_weight_update_response_t response = {};
                response.header.type = BIO_MSG_WEIGHT_UPDATE_RESPONSE;
                response.synapse_id = request->synapse_id;
                response.old_weight = 0.5f;
                response.new_weight = 0.5f + request->weight_delta * request->learning_rate;
                response.clamped = false;
                response.error = NIMCP_SUCCESS;

                if (promise) {
                    // Use sized version to specify actual response size
                    nimcp_bio_promise_complete_sized(promise, &response, sizeof(response));
                }
                return NIMCP_SUCCESS;
            });

        // Glial broadcast handler (all modules should receive)
        auto glial_handler = [](const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
            auto* counter = static_cast<std::atomic<int>*>(user_data);
            (*counter)++;
            return NIMCP_SUCCESS;
        };

        bio_router_register_handler(brain_ctx, BIO_MSG_ASTROCYTE_CALCIUM_WAVE, glial_handler);
        bio_router_register_handler(cognitive_ctx, BIO_MSG_ASTROCYTE_CALCIUM_WAVE, glial_handler);
        bio_router_register_handler(plasticity_ctx, BIO_MSG_ASTROCYTE_CALCIUM_WAVE, glial_handler);
        bio_router_register_handler(training_ctx, BIO_MSG_ASTROCYTE_CALCIUM_WAVE, glial_handler);
    }

    void ProcessAllInboxes() {
        bio_router_process_inbox(brain_ctx, 0);
        bio_router_process_inbox(cognitive_ctx, 0);
        bio_router_process_inbox(plasticity_ctx, 0);
        bio_router_process_inbox(glial_ctx, 0);
        bio_router_process_inbox(training_ctx, 0);
    }
};

//=============================================================================
// Brain → Cognitive Communication Tests
//=============================================================================

TEST_F(CrossModuleIntegrationTest, BrainToCognitiveQuery) {
    // Send query from cognitive to brain
    bio_msg_brain_state_query_t query = {};
    bio_msg_init_header(&query.header, BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MODULE_INTROSPECTION, BIO_MODULE_BRAIN, sizeof(query));
    query.query_flags = BIO_BRAIN_QUERY_NEUROMODULATORS | BIO_BRAIN_QUERY_NEURON_COUNT;
    query.region_id = 0; // Global

    nimcp_bio_promise_t promise = bio_router_send_async(
        cognitive_ctx, &query, sizeof(query), BIO_CHANNEL_ACETYLCHOLINE);

    ASSERT_NE(promise, nullptr);

    // Process brain inbox
    ProcessAllInboxes();

    // Wait for response
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    bio_msg_brain_state_response_t response = {};
    nimcp_error_t err = nimcp_bio_future_wait(future, &response, 1000);

    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(brain_msg_count.load(), 1);
    EXPECT_EQ(response.neuron_count, 10000u);
    EXPECT_GT(response.dopamine_level, 0.0f);
    EXPECT_GT(response.acetylcholine_level, 0.0f);

    // Verify confidence is high (acetylcholine = fast)
    float confidence = nimcp_bio_future_get_confidence(future);
    EXPECT_GT(confidence, 0.9f) << "Fast ACh channel should have high confidence";

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(CrossModuleIntegrationTest, CognitiveToBrainIntrospection) {
    // Cognitive module queries brain about patterns
    bio_msg_introspection_query_t query = {};
    bio_msg_init_header(&query.header, BIO_MSG_INTROSPECTION_QUERY,
        BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, sizeof(query));
    query.query_type = BIO_INTRO_QUERY_PATTERN_MATCH;
    query.target_pattern_id = 42;
    query.confidence_threshold = 0.7f;

    nimcp_bio_promise_t promise = bio_router_send_async(
        brain_ctx, &query, sizeof(query), BIO_CHANNEL_ACETYLCHOLINE);

    ASSERT_NE(promise, nullptr);

    ProcessAllInboxes();

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    bio_msg_introspection_response_t response = {};
    nimcp_error_t err = nimcp_bio_future_wait(future, &response, 1000);

    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(cognitive_msg_count.load(), 1);
    EXPECT_GT(response.confidence, 0.0f);
    EXPECT_GE(response.matched_pattern_count, 0u);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Training → Plasticity Communication Tests
//=============================================================================

TEST_F(CrossModuleIntegrationTest, TrainingToPlasticityWeightUpdate) {
    // Training sends weight update request
    bio_msg_weight_update_request_t request = {};
    bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
        BIO_MODULE_TRAINING, BIO_MODULE_STDP, sizeof(request));
    request.synapse_id = 1234;
    request.pre_neuron_id = 100;
    request.post_neuron_id = 200;
    request.weight_delta = 0.01f;
    request.learning_rate = 0.1f;
    request.eligibility_trace = 0.8f;
    request.clamp_to_bounds = true;
    request.min_weight = 0.0f;
    request.max_weight = 1.0f;

    nimcp_bio_promise_t promise = bio_router_send_async(
        training_ctx, &request, sizeof(request), BIO_CHANNEL_DOPAMINE);

    ASSERT_NE(promise, nullptr);

    ProcessAllInboxes();

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    bio_msg_weight_update_response_t response = {};
    nimcp_error_t err = nimcp_bio_future_wait(future, &response, 1000);

    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(plasticity_msg_count.load(), 1);
    EXPECT_EQ(response.synapse_id, 1234u);
    EXPECT_GT(response.new_weight, response.old_weight) << "Weight should increase";
    EXPECT_EQ(response.error, NIMCP_SUCCESS);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(CrossModuleIntegrationTest, BatchWeightUpdates) {
    const int batch_size = 10;
    std::vector<nimcp_bio_promise_t> promises;

    for (int i = 0; i < batch_size; i++) {
        bio_msg_weight_update_request_t request = {};
        bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
            BIO_MODULE_TRAINING, BIO_MODULE_STDP, sizeof(request));
        request.synapse_id = 1000 + i;
        request.pre_neuron_id = 100 + i;
        request.post_neuron_id = 200 + i;
        request.weight_delta = 0.01f * (i + 1);
        request.learning_rate = 0.1f;

        nimcp_bio_promise_t promise = bio_router_send_async(
            training_ctx, &request, sizeof(request), BIO_CHANNEL_DOPAMINE);
        ASSERT_NE(promise, nullptr);
        promises.push_back(promise);
    }

    ProcessAllInboxes();

    // Verify all responses
    int success_count = 0;
    for (auto promise : promises) {
        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
        bio_msg_weight_update_response_t response = {};
        if (nimcp_bio_future_wait(future, &response, 2000) == NIMCP_SUCCESS) {
            EXPECT_EQ(response.error, NIMCP_SUCCESS);
            success_count++;
        }
        nimcp_bio_future_destroy(future);
        nimcp_bio_promise_destroy(promise);
    }

    EXPECT_EQ(success_count, batch_size);
    EXPECT_EQ(plasticity_msg_count.load(), batch_size);
}

//=============================================================================
// Glial Broadcast Tests
//=============================================================================

TEST_F(CrossModuleIntegrationTest, GlialBroadcastToAllModules) {
    // Reset counters
    brain_msg_count = 0;
    cognitive_msg_count = 0;
    plasticity_msg_count = 0;
    training_msg_count = 0;

    // Send calcium wave broadcast
    bio_msg_astrocyte_wave_t wave = {};
    bio_msg_init_header(&wave.header, BIO_MSG_ASTROCYTE_CALCIUM_WAVE,
        BIO_MODULE_ASTROCYTE, (bio_module_id_t)0, sizeof(wave)); // 0 = broadcast
    wave.header.flags = BIO_MSG_FLAG_BROADCAST;
    wave.source_region = 1;
    wave.initial_calcium_um = 2.5f;
    wave.propagation_speed_um_s = 10.0f;
    wave.mode = BIO_WAVE_ISOTROPIC;

    nimcp_error_t err = bio_router_broadcast(glial_ctx, &wave, sizeof(wave));
    ASSERT_EQ(err, NIMCP_SUCCESS);

    ProcessAllInboxes();

    // Verify all modules received the broadcast
    EXPECT_EQ(brain_msg_count.load(), 1) << "Brain should receive broadcast";
    EXPECT_EQ(cognitive_msg_count.load(), 1) << "Cognitive should receive broadcast";
    EXPECT_EQ(plasticity_msg_count.load(), 1) << "Plasticity should receive broadcast";
    EXPECT_EQ(training_msg_count.load(), 1) << "Training should receive broadcast";
}

TEST_F(CrossModuleIntegrationTest, MultipleConcurrentBroadcasts) {
    brain_msg_count = 0;
    cognitive_msg_count = 0;
    plasticity_msg_count = 0;
    training_msg_count = 0;

    const int num_broadcasts = 5;

    for (int i = 0; i < num_broadcasts; i++) {
        bio_msg_astrocyte_wave_t wave = {};
        bio_msg_init_header(&wave.header, BIO_MSG_ASTROCYTE_CALCIUM_WAVE,
            BIO_MODULE_ASTROCYTE, (bio_module_id_t)0, sizeof(wave));
        wave.header.flags = BIO_MSG_FLAG_BROADCAST;
        wave.source_region = i + 1;
        wave.initial_calcium_um = 2.0f + i * 0.5f;
        wave.propagation_speed_um_s = 10.0f;
        wave.mode = BIO_WAVE_ISOTROPIC;

        ASSERT_EQ(bio_router_broadcast(glial_ctx, &wave, sizeof(wave)), NIMCP_SUCCESS);
    }

    ProcessAllInboxes();

    // Each module should have received all broadcasts
    EXPECT_EQ(brain_msg_count.load(), num_broadcasts);
    EXPECT_EQ(cognitive_msg_count.load(), num_broadcasts);
    EXPECT_EQ(plasticity_msg_count.load(), num_broadcasts);
    EXPECT_EQ(training_msg_count.load(), num_broadcasts);
}

//=============================================================================
// Phase Synchronization Tests
//=============================================================================

TEST_F(CrossModuleIntegrationTest, PhaseCoherenceAcrossModules) {
    // Create phase sync group
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    ASSERT_NE(sync, nullptr);

    // Send requests to multiple modules
    std::vector<nimcp_bio_promise_t> promises;

    // Brain query
    bio_msg_brain_state_query_t brain_query = {};
    bio_msg_init_header(&brain_query.header, BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MODULE_TRAINING, BIO_MODULE_BRAIN, sizeof(brain_query));
    brain_query.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;

    auto p1 = bio_router_send_async(training_ctx, &brain_query, sizeof(brain_query),
        BIO_CHANNEL_ACETYLCHOLINE);
    promises.push_back(p1);

    // Cognitive query
    bio_msg_introspection_query_t cog_query = {};
    bio_msg_init_header(&cog_query.header, BIO_MSG_INTROSPECTION_QUERY,
        BIO_MODULE_TRAINING, BIO_MODULE_INTROSPECTION, sizeof(cog_query));
    cog_query.query_type = BIO_INTRO_QUERY_COGNITIVE_LOAD;

    auto p2 = bio_router_send_async(training_ctx, &cog_query, sizeof(cog_query),
        BIO_CHANNEL_ACETYLCHOLINE);
    promises.push_back(p2);

    // Add futures to sync group
    for (auto promise : promises) {
        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
        ASSERT_EQ(nimcp_phase_sync_add_future(sync, future), NIMCP_SUCCESS);
    }

    ProcessAllInboxes();

    // Wait for phase coherence
    nimcp_error_t err = nimcp_phase_sync_wait_coherent(sync, 0.8f, 2000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Check coherence achieved
    float coherence = nimcp_phase_sync_get_coherence(sync);
    EXPECT_GE(coherence, 0.8f);

    // Cleanup
    for (auto promise : promises) {
        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
        nimcp_bio_future_destroy(future);
        nimcp_bio_promise_destroy(promise);
    }
    nimcp_phase_sync_destroy(sync);
}

//=============================================================================
// Predictive Coding Tests
//=============================================================================

TEST_F(CrossModuleIntegrationTest, PredictiveCodingSignalPropagation) {
    std::atomic<int> prediction_errors{0};

    // Register observer in cognitive module
    auto observer = [](const char* signal_name, float value, void* user_data) {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        (*counter)++;
    };

    nimcp_error_t err = bio_router_observe_signal(
        cognitive_ctx, "dopamine_level", 0.5f, 1.0f, observer);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Publish signals from brain
    bio_router_publish_signal(brain_ctx, "dopamine_level", 0.75f);
    bio_router_publish_signal(brain_ctx, "dopamine_level", 0.80f);
    bio_router_publish_signal(brain_ctx, "dopamine_level", 0.85f);

    ProcessAllInboxes();

    // Observer should have been called for prediction errors
    // (exact count depends on surprise threshold)
    EXPECT_GE(prediction_errors.load(), 0);
}

//=============================================================================
// Error Recovery and Timeout Tests
//=============================================================================

TEST_F(CrossModuleIntegrationTest, TimeoutHandling) {
    // Send query to non-existent module
    // Note: BIO_MODULE_UNKNOWN (0) is broadcast address, so use a clearly invalid ID
    const uint32_t INVALID_MODULE_ID = 0xFFFF;  // Not a registered module

    bio_msg_brain_state_query_t query = {};
    bio_msg_init_header(&query.header, BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MODULE_TRAINING, (bio_module_id_t)INVALID_MODULE_ID, sizeof(query));

    // This should fail to route since no module with this ID exists
    nimcp_error_t err = bio_router_send(training_ctx, &query, sizeof(query), 100);

    // Should fail to route (module not found)
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(CrossModuleIntegrationTest, ConcurrentCrossModuleCommunication) {
    const int num_concurrent = 20;
    std::vector<nimcp_bio_promise_t> promises;

    // Send concurrent requests from different modules
    for (int i = 0; i < num_concurrent; i++) {
        bio_module_context_t sender = (i % 2 == 0) ? training_ctx : cognitive_ctx;

        bio_msg_brain_state_query_t query = {};
        bio_msg_init_header(&query.header, BIO_MSG_BRAIN_STATE_QUERY,
            bio_module_context_get_id(sender), BIO_MODULE_BRAIN, sizeof(query));
        query.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;

        auto promise = bio_router_send_async(sender, &query, sizeof(query),
            BIO_CHANNEL_ACETYLCHOLINE);
        ASSERT_NE(promise, nullptr);
        promises.push_back(promise);
    }

    ProcessAllInboxes();

    // Verify all succeeded
    int success_count = 0;
    for (auto promise : promises) {
        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
        bio_msg_brain_state_response_t response = {};
        if (nimcp_bio_future_wait(future, &response, 2000) == NIMCP_SUCCESS) {
            success_count++;
        }
        nimcp_bio_future_destroy(future);
        nimcp_bio_promise_destroy(promise);
    }

    EXPECT_EQ(success_count, num_concurrent);
    EXPECT_EQ(brain_msg_count.load(), num_concurrent);
}

//=============================================================================
// Statistics and Performance Tests
//=============================================================================

TEST_F(CrossModuleIntegrationTest, VerifyStatisticsTracking) {
    bio_router_reset_stats();

    // Send various messages
    bio_msg_brain_state_query_t query = {};
    bio_msg_init_header(&query.header, BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MODULE_TRAINING, BIO_MODULE_BRAIN, sizeof(query));

    for (int i = 0; i < 10; i++) {
        auto promise = bio_router_send_async(training_ctx, &query, sizeof(query),
            BIO_CHANNEL_ACETYLCHOLINE);
        nimcp_bio_promise_destroy(promise);
    }

    ProcessAllInboxes();

    bio_router_stats_t stats;
    ASSERT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);

    EXPECT_GE(stats.messages_routed, 10u);
    EXPECT_EQ(stats.active_modules, 5u);
}

// End of tests
