/**
 * @file test_plasticity_bio_async.cpp
 * @brief Unit tests for plasticity module bio-async integration
 *
 * Tests plasticity mechanisms via bio-async messaging:
 * - STDP weight update handling via dopamine
 * - Neuromodulator release signaling
 * - Homeostatic adjustment broadcasts
 * - Dendritic spike event handling
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <atomic>
#include <vector>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PlasticityBioAsyncTest : public ::testing::Test {
protected:
    bio_module_context_t stdp_module;
    bio_module_context_t neuromod_module;
    bio_module_context_t homeostatic_module;
    bio_module_context_t brain_module;

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

        // Register modules
        bio_module_info_t stdp_info = {
            .module_id = BIO_MODULE_STDP,
            .module_name = "stdp",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        stdp_module = bio_router_register_module(&stdp_info);
        ASSERT_NE(stdp_module, nullptr);

        bio_module_info_t neuromod_info = {
            .module_id = BIO_MODULE_NEUROMODULATOR,
            .module_name = "neuromodulator",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        neuromod_module = bio_router_register_module(&neuromod_info);
        ASSERT_NE(neuromod_module, nullptr);

        bio_module_info_t homeo_info = {
            .module_id = BIO_MODULE_HOMEOSTATIC,
            .module_name = "homeostatic",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        homeostatic_module = bio_router_register_module(&homeo_info);
        ASSERT_NE(homeostatic_module, nullptr);

        bio_module_info_t brain_info = {
            .module_id = BIO_MODULE_BRAIN,
            .module_name = "brain",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        brain_module = bio_router_register_module(&brain_info);
        ASSERT_NE(brain_module, nullptr);
    }

    void TearDown() override {
        bio_router_unregister_module(stdp_module);
        bio_router_unregister_module(neuromod_module);
        bio_router_unregister_module(homeostatic_module);
        bio_router_unregister_module(brain_module);
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// STDP WEIGHT UPDATE TESTS (Dopamine Channel)
//=============================================================================

TEST_F(PlasticityBioAsyncTest, WeightUpdateRequestViaDopamine) {
    std::atomic<bool> update_received{false};
    std::atomic<float> weight_delta{0.0f};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<float>*>*>(user_data);
        data->first->store(true);

        auto* request = static_cast<const bio_msg_weight_update_request_t*>(msg);
        EXPECT_EQ(request->header.type, BIO_MSG_WEIGHT_UPDATE_REQUEST);
        EXPECT_EQ(request->header.channel, BIO_CHANNEL_DOPAMINE);

        data->second->store(request->weight_delta);

        // Send response
        if (promise) {
            bio_msg_weight_update_response_t response;
            bio_msg_init_header(&response.header, BIO_MSG_WEIGHT_UPDATE_RESPONSE,
                               BIO_MODULE_STDP, request->header.source_module,
                               sizeof(response));
            response.synapse_id = request->synapse_id;
            response.old_weight = 0.5f;
            response.new_weight = 0.5f + request->weight_delta;
            response.clamped = false;
            response.error = NIMCP_SUCCESS;

            nimcp_bio_promise_complete(promise, &response);
        }

        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<float>*> handler_data{
        &update_received, &weight_delta
    };

    bio_router_unregister_module(stdp_module);
    bio_module_info_t stdp_info = {
        .module_id = BIO_MODULE_STDP,
        .module_name = "stdp",
        .inbox_capacity = 100,
        .user_data = &handler_data
    };
    stdp_module = bio_router_register_module(&stdp_info);

    bio_router_register_handler(stdp_module, BIO_MSG_WEIGHT_UPDATE_REQUEST, handler);

    // Send weight update request
    bio_msg_weight_update_request_t request;
    bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                       BIO_MODULE_BRAIN, BIO_MODULE_STDP,
                       sizeof(request));
    request.header.channel = BIO_CHANNEL_DOPAMINE;  // Reward signal
    request.synapse_id = 123;
    request.pre_neuron_id = 10;
    request.post_neuron_id = 20;
    request.weight_delta = 0.05f;
    request.learning_rate = 0.01f;
    request.eligibility_trace = 1.0f;
    request.clamp_to_bounds = true;
    request.min_weight = 0.0f;
    request.max_weight = 1.0f;

    bio_router_send(brain_module, &request, sizeof(request), 1000);
    bio_router_process_inbox(stdp_module, 10);

    EXPECT_TRUE(update_received.load());
    EXPECT_FLOAT_EQ(weight_delta.load(), 0.05f);
}

TEST_F(PlasticityBioAsyncTest, STDPEventHandling) {
    std::atomic<bool> event_received{false};
    std::atomic<float> delta_t{0.0f};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<float>*>*>(user_data);
        data->first->store(true);

        auto* event = static_cast<const bio_msg_stdp_event_t*>(msg);
        EXPECT_EQ(event->header.type, BIO_MSG_STDP_EVENT);

        data->second->store(event->delta_t_ms);

        // Compute STDP weight change
        float A_plus = 0.01f;
        float A_minus = 0.012f;
        float tau_plus = 20.0f;
        float tau_minus = 20.0f;

        float dw = 0.0f;
        if (event->delta_t_ms > 0) {
            // Post before pre: LTD
            dw = -A_minus * expf(-event->delta_t_ms / tau_minus);
        } else {
            // Pre before post: LTP
            dw = A_plus * expf(event->delta_t_ms / tau_plus);
        }

        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<float>*> handler_data{
        &event_received, &delta_t
    };

    bio_router_unregister_module(stdp_module);
    bio_module_info_t stdp_info = {
        .module_id = BIO_MODULE_STDP,
        .module_name = "stdp",
        .inbox_capacity = 100,
        .user_data = &handler_data
    };
    stdp_module = bio_router_register_module(&stdp_info);

    bio_router_register_handler(stdp_module, BIO_MSG_STDP_EVENT, handler);

    // Send STDP event (pre before post = LTP)
    bio_msg_stdp_event_t event;
    bio_msg_init_header(&event.header, BIO_MSG_STDP_EVENT,
                       BIO_MODULE_BRAIN, BIO_MODULE_STDP,
                       sizeof(event));
    event.pre_neuron_id = 10;
    event.post_neuron_id = 20;
    event.pre_spike_time_ms = 100.0f;
    event.post_spike_time_ms = 110.0f;
    event.delta_t_ms = 10.0f;  // Pre before post

    bio_router_send(brain_module, &event, sizeof(event), 1000);
    bio_router_process_inbox(stdp_module, 10);

    EXPECT_TRUE(event_received.load());
    EXPECT_FLOAT_EQ(delta_t.load(), 10.0f);
}

TEST_F(PlasticityBioAsyncTest, STDPBatchProcessing) {
    std::atomic<int> events_processed{0};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* count = static_cast<std::atomic<int>*>(user_data);

        auto* batch = static_cast<const bio_msg_stdp_batch_t*>(msg);
        EXPECT_EQ(batch->header.type, BIO_MSG_STDP_BATCH_EVENT);

        // In real implementation, would process batch array following header
        (*count) += batch->event_count;

        return NIMCP_SUCCESS;
    };

    bio_router_unregister_module(stdp_module);
    bio_module_info_t stdp_info = {
        .module_id = BIO_MODULE_STDP,
        .module_name = "stdp",
        .inbox_capacity = 100,
        .user_data = &events_processed
    };
    stdp_module = bio_router_register_module(&stdp_info);

    bio_router_register_handler(stdp_module, BIO_MSG_STDP_BATCH_EVENT, handler);

    // Send batch of STDP events
    bio_msg_stdp_batch_t batch;
    bio_msg_init_header(&batch.header, BIO_MSG_STDP_BATCH_EVENT,
                       BIO_MODULE_BRAIN, BIO_MODULE_STDP,
                       sizeof(batch));
    batch.event_count = 10;
    batch.max_events = 100;

    bio_router_send(brain_module, &batch, sizeof(batch), 1000);
    bio_router_process_inbox(stdp_module, 10);

    EXPECT_EQ(events_processed.load(), 10);
}

//=============================================================================
// NEUROMODULATOR RELEASE TESTS
//=============================================================================

TEST_F(PlasticityBioAsyncTest, NeuromodulatorReleaseEvent) {
    std::atomic<bool> release_detected{false};
    std::atomic<nimcp_bio_channel_type_t> released_type{BIO_CHANNEL_DOPAMINE};
    std::atomic<float> concentration{0.0f};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::tuple<std::atomic<bool>*,
                                           std::atomic<nimcp_bio_channel_type_t>*,
                                           std::atomic<float>*>*>(user_data);

        std::get<0>(*data)->store(true);

        auto* release = static_cast<const bio_msg_neuromodulator_release_t*>(msg);
        EXPECT_EQ(release->header.type, BIO_MSG_NEUROMODULATOR_RELEASE);

        std::get<1>(*data)->store(release->neuromodulator);
        std::get<2>(*data)->store(release->current_concentration);

        return NIMCP_SUCCESS;
    };

    std::tuple<std::atomic<bool>*,
              std::atomic<nimcp_bio_channel_type_t>*,
              std::atomic<float>*> handler_data{
        &release_detected, &released_type, &concentration
    };

    bio_router_unregister_module(neuromod_module);
    bio_module_info_t neuromod_info = {
        .module_id = BIO_MODULE_NEUROMODULATOR,
        .module_name = "neuromodulator",
        .inbox_capacity = 100,
        .user_data = &handler_data
    };
    neuromod_module = bio_router_register_module(&neuromod_info);

    bio_router_register_handler(neuromod_module, BIO_MSG_NEUROMODULATOR_RELEASE, handler);

    // Send dopamine release event
    bio_msg_neuromodulator_release_t release;
    bio_msg_init_header(&release.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                       BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR,
                       sizeof(release));
    release.neuromodulator = BIO_CHANNEL_DOPAMINE;
    release.source_region = 0;
    release.release_amount = 1.0f;  // μM
    release.current_concentration = BIO_DA_PEAK_PHASIC_UM;
    release.diffusion_radius_um = 100.0f;

    bio_router_send(brain_module, &release, sizeof(release), 1000);
    bio_router_process_inbox(neuromod_module, 10);

    EXPECT_TRUE(release_detected.load());
    EXPECT_EQ(released_type.load(), BIO_CHANNEL_DOPAMINE);
    EXPECT_GT(concentration.load(), 0.0f);
}

TEST_F(PlasticityBioAsyncTest, MultipleNeuromodulatorTypes) {
    std::atomic<int> dopamine_count{0};
    std::atomic<int> serotonin_count{0};
    std::atomic<int> norepinephrine_count{0};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::tuple<std::atomic<int>*,
                                           std::atomic<int>*,
                                           std::atomic<int>*>*>(user_data);

        auto* release = static_cast<const bio_msg_neuromodulator_release_t*>(msg);

        switch (release->neuromodulator) {
            case BIO_CHANNEL_DOPAMINE:
                (*std::get<0>(*data))++;
                break;
            case BIO_CHANNEL_SEROTONIN:
                (*std::get<1>(*data))++;
                break;
            case BIO_CHANNEL_NOREPINEPHRINE:
                (*std::get<2>(*data))++;
                break;
            default:
                break;
        }

        return NIMCP_SUCCESS;
    };

    std::tuple<std::atomic<int>*, std::atomic<int>*, std::atomic<int>*> handler_data{
        &dopamine_count, &serotonin_count, &norepinephrine_count
    };

    bio_router_unregister_module(neuromod_module);
    bio_module_info_t neuromod_info = {
        .module_id = BIO_MODULE_NEUROMODULATOR,
        .module_name = "neuromodulator",
        .inbox_capacity = 100,
        .user_data = &handler_data
    };
    neuromod_module = bio_router_register_module(&neuromod_info);

    bio_router_register_handler(neuromod_module, BIO_MSG_NEUROMODULATOR_RELEASE, handler);

    // Send different neuromodulator releases
    nimcp_bio_channel_type_t types[] = {
        BIO_CHANNEL_DOPAMINE,
        BIO_CHANNEL_DOPAMINE,
        BIO_CHANNEL_SEROTONIN,
        BIO_CHANNEL_NOREPINEPHRINE
    };

    for (auto type : types) {
        bio_msg_neuromodulator_release_t release;
        bio_msg_init_header(&release.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                           BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR,
                           sizeof(release));
        release.neuromodulator = type;
        release.current_concentration = 1.0f;

        bio_router_send(brain_module, &release, sizeof(release), 1000);
    }

    bio_router_process_inbox(neuromod_module, 10);

    EXPECT_EQ(dopamine_count.load(), 2);
    EXPECT_EQ(serotonin_count.load(), 1);
    EXPECT_EQ(norepinephrine_count.load(), 1);
}

//=============================================================================
// HOMEOSTATIC ADJUSTMENT TESTS
//=============================================================================

TEST_F(PlasticityBioAsyncTest, HomeostaticAdjustment) {
    std::atomic<bool> adjustment_received{false};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* received = static_cast<std::atomic<bool>*>(user_data);
        *received = true;

        // In real implementation, would adjust synaptic scaling
        // or intrinsic excitability

        return NIMCP_SUCCESS;
    };

    bio_router_unregister_module(homeostatic_module);
    bio_module_info_t homeo_info = {
        .module_id = BIO_MODULE_HOMEOSTATIC,
        .module_name = "homeostatic",
        .inbox_capacity = 100,
        .user_data = &adjustment_received
    };
    homeostatic_module = bio_router_register_module(&homeo_info);

    bio_router_register_handler(homeostatic_module, BIO_MSG_HOMEOSTATIC_ADJUSTMENT, handler);

    // Send homeostatic adjustment
    bio_message_header_t adjustment;
    bio_msg_init_header(&adjustment, BIO_MSG_HOMEOSTATIC_ADJUSTMENT,
                       BIO_MODULE_BRAIN, BIO_MODULE_HOMEOSTATIC,
                       sizeof(adjustment));

    bio_router_send(brain_module, &adjustment, sizeof(adjustment), 1000);
    bio_router_process_inbox(homeostatic_module, 10);

    EXPECT_TRUE(adjustment_received.load());
}

//=============================================================================
// DENDRITIC SPIKE TESTS
//=============================================================================

TEST_F(PlasticityBioAsyncTest, DendriticSpikeEvent) {
    std::atomic<bool> spike_detected{false};

    // Register dendritic module
    bio_module_info_t dend_info = {
        .module_id = BIO_MODULE_DENDRITIC,
        .module_name = "dendritic",
        .inbox_capacity = 100,
        .user_data = &spike_detected
    };
    bio_module_context_t dend_module = bio_router_register_module(&dend_info);
    ASSERT_NE(dend_module, nullptr);

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* detected = static_cast<std::atomic<bool>*>(user_data);
        *detected = true;

        // In real implementation, would trigger dendritic computation
        // and local plasticity

        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(dend_module, BIO_MSG_DENDRITIC_SPIKE, handler);

    // Send dendritic spike
    bio_message_header_t spike;
    bio_msg_init_header(&spike, BIO_MSG_DENDRITIC_SPIKE,
                       BIO_MODULE_BRAIN, BIO_MODULE_DENDRITIC,
                       sizeof(spike));

    bio_router_send(brain_module, &spike, sizeof(spike), 1000);
    bio_router_process_inbox(dend_module, 10);

    EXPECT_TRUE(spike_detected.load());

    bio_router_unregister_module(dend_module);
}

//=============================================================================
// INTEGRATION TESTS
//=============================================================================

TEST_F(PlasticityBioAsyncTest, STDPWithNeuromodulation) {
    // Test: STDP event triggers weight update, modulated by dopamine

    std::atomic<bool> stdp_received{false};
    std::atomic<bool> neuromod_received{false};
    std::atomic<float> final_weight_delta{0.0f};

    // STDP handler
    auto stdp_handler = [](const void* msg, size_t size,
                          nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::tuple<std::atomic<bool>*,
                                           std::atomic<bool>*,
                                           std::atomic<float>*>*>(user_data);
        std::get<0>(*data)->store(true);

        auto* event = static_cast<const bio_msg_stdp_event_t*>(msg);

        // Compute base weight change
        float base_dw = 0.01f * expf(-fabsf(event->delta_t_ms) / 20.0f);
        std::get<2>(*data)->store(base_dw);

        return NIMCP_SUCCESS;
    };

    // Neuromodulator handler
    auto neuromod_handler = [](const void* msg, size_t size,
                               nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::tuple<std::atomic<bool>*,
                                           std::atomic<bool>*,
                                           std::atomic<float>*>*>(user_data);
        std::get<1>(*data)->store(true);

        auto* release = static_cast<const bio_msg_neuromodulator_release_t*>(msg);

        // Modulate weight change by dopamine
        float modulation = release->current_concentration / BIO_DA_PEAK_PHASIC_UM;
        float current_dw = std::get<2>(*data)->load();
        std::get<2>(*data)->store(current_dw * modulation);

        return NIMCP_SUCCESS;
    };

    std::tuple<std::atomic<bool>*, std::atomic<bool>*, std::atomic<float>*> handler_data{
        &stdp_received, &neuromod_received, &final_weight_delta
    };

    // Register handlers
    bio_router_unregister_module(stdp_module);
    bio_router_unregister_module(neuromod_module);

    bio_module_info_t stdp_info = {
        .module_id = BIO_MODULE_STDP,
        .module_name = "stdp",
        .inbox_capacity = 100,
        .user_data = &handler_data
    };
    stdp_module = bio_router_register_module(&stdp_info);
    bio_router_register_handler(stdp_module, BIO_MSG_STDP_EVENT, stdp_handler);

    bio_module_info_t neuromod_info = {
        .module_id = BIO_MODULE_NEUROMODULATOR,
        .module_name = "neuromodulator",
        .inbox_capacity = 100,
        .user_data = &handler_data
    };
    neuromod_module = bio_router_register_module(&neuromod_info);
    bio_router_register_handler(neuromod_module, BIO_MSG_NEUROMODULATOR_RELEASE, neuromod_handler);

    // Send STDP event
    bio_msg_stdp_event_t event;
    bio_msg_init_header(&event.header, BIO_MSG_STDP_EVENT,
                       BIO_MODULE_BRAIN, BIO_MODULE_STDP,
                       sizeof(event));
    event.delta_t_ms = 10.0f;
    bio_router_send(brain_module, &event, sizeof(event), 1000);

    // Send dopamine release
    bio_msg_neuromodulator_release_t release;
    bio_msg_init_header(&release.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                       BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR,
                       sizeof(release));
    release.neuromodulator = BIO_CHANNEL_DOPAMINE;
    release.current_concentration = BIO_DA_PEAK_PHASIC_UM;
    bio_router_send(brain_module, &release, sizeof(release), 1000);

    // Process both
    bio_router_process_inbox(stdp_module, 10);
    bio_router_process_inbox(neuromod_module, 10);

    EXPECT_TRUE(stdp_received.load());
    EXPECT_TRUE(neuromod_received.load());
    EXPECT_GT(final_weight_delta.load(), 0.0f);
}
