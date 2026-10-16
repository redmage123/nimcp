/**
 * @file e2e_test_bio_async_plasticity_pipeline.cpp
 * @brief E2E Tests for Bio-Async Plasticity Pipeline
 *
 * WHAT: Complete plasticity pipelines using bio-async messaging
 * WHY:  Verify STDP, homeostatic, and neuromodulator coordination via bio-async
 * HOW:  Test plasticity modules communicating through bio-router with proper channels
 *
 * TEST PIPELINES:
 * - STDPPipeline: Spike-timing dependent plasticity with temporal coordination
 * - HomeostaticPipeline: Homeostatic plasticity balancing network activity
 * - NeuromodulatorPipeline: Dopamine/serotonin modulated learning
 * - MultiPlasticityPipeline: Combined plasticity mechanisms coordinated via bio-async
 *
 * @author NIMCP Development Team
 * @date 2025-12-03
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <atomic>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BioAsyncPlasticityE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async system
        nimcp_error_t err = nimcp_bio_async_init(NULL);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-async initialization failed";

        // Initialize router
        err = bio_router_init(NULL);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Router initialization failed";

        // Register STDP module
        bio_module_info_t stdp_info = {
            .module_id = BIO_MODULE_STDP,
            .module_name = "stdp_test",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        stdp_ctx_ = bio_router_register_module(&stdp_info);
        ASSERT_NE(stdp_ctx_, nullptr) << "Failed to register STDP module";

        // Register homeostatic module
        bio_module_info_t homeo_info = {
            .module_id = BIO_MODULE_HOMEOSTATIC,
            .module_name = "homeostatic_test",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        homeo_ctx_ = bio_router_register_module(&homeo_info);
        ASSERT_NE(homeo_ctx_, nullptr) << "Failed to register homeostatic module";

        // Register neuromodulator module
        bio_module_info_t neuromod_info = {
            .module_id = BIO_MODULE_NEUROMODULATOR,
            .module_name = "neuromod_test",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        neuromod_ctx_ = bio_router_register_module(&neuromod_info);
        ASSERT_NE(neuromod_ctx_, nullptr) << "Failed to register neuromodulator module";

        // Reset counters
        stdp_events_received_.store(0);
        weight_updates_received_.store(0);
        neuromod_releases_received_.store(0);
    }

    void TearDown() override {
        if (stdp_ctx_) bio_router_unregister_module(stdp_ctx_);
        if (homeo_ctx_) bio_router_unregister_module(homeo_ctx_);
        if (neuromod_ctx_) bio_router_unregister_module(neuromod_ctx_);

        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    bio_module_context_t stdp_ctx_{nullptr};
    bio_module_context_t homeo_ctx_{nullptr};
    bio_module_context_t neuromod_ctx_{nullptr};

public:
    static std::atomic<int> stdp_events_received_;
    static std::atomic<int> weight_updates_received_;
    static std::atomic<int> neuromod_releases_received_;
    static std::atomic<float> last_weight_delta_;
};

// Static member initialization
std::atomic<int> BioAsyncPlasticityE2ETest::stdp_events_received_{0};
std::atomic<int> BioAsyncPlasticityE2ETest::weight_updates_received_{0};
std::atomic<int> BioAsyncPlasticityE2ETest::neuromod_releases_received_{0};
std::atomic<float> BioAsyncPlasticityE2ETest::last_weight_delta_{0.0f};

//=============================================================================
// Static Handler Functions (No Lambda Captures)
//=============================================================================

static nimcp_error_t stdp_handler_static(const void* msg, size_t msg_size,
                                         nimcp_bio_promise_t response_promise,
                                         void* user_data) {
    const bio_msg_stdp_event_t* stdp_msg =
        static_cast<const bio_msg_stdp_event_t*>(msg);

    BioAsyncPlasticityE2ETest::stdp_events_received_.fetch_add(1);

    // Calculate weight change based on timing
    float delta_t = stdp_msg->delta_t_ms;
    float weight_change = 0.0f;

    if (delta_t > 0) {
        // Post after pre: potentiation
        weight_change = 0.01f * expf(-delta_t / 20.0f);
    } else {
        // Pre after post: depression
        weight_change = -0.01f * expf(delta_t / 20.0f);
    }

    BioAsyncPlasticityE2ETest::last_weight_delta_.store(weight_change);

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &weight_change);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t homeostatic_handler_static(const void* msg, size_t msg_size,
                                                 nimcp_bio_promise_t response_promise,
                                                 void* user_data) {
    // Use generic message header - homeostatic adjustment type not defined
    const bio_message_header_t* header =
        reinterpret_cast<const bio_message_header_t*>(msg);
    (void)header;  // Unused in this handler

    std::atomic<int>* counter = static_cast<std::atomic<int>*>(user_data);
    if (counter) {
        counter->fetch_add(1);
    }

    if (response_promise) {
        float ack = 1.0f;
        nimcp_bio_promise_complete(response_promise, &ack);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t neuromod_handler_static(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t response_promise,
                                              void* user_data) {
    const bio_msg_neuromodulator_release_t* neuromod_msg =
        static_cast<const bio_msg_neuromodulator_release_t*>(msg);

    BioAsyncPlasticityE2ETest::neuromod_releases_received_.fetch_add(1);

    if (response_promise) {
        float concentration = neuromod_msg->current_concentration;
        nimcp_bio_promise_complete(response_promise, &concentration);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t learning_rate_handler_static(const void* msg, size_t msg_size,
                                                   nimcp_bio_promise_t response_promise,
                                                   void* user_data) {
    const bio_msg_learning_rate_update_t* lr_msg =
        static_cast<const bio_msg_learning_rate_update_t*>(msg);
    (void)lr_msg;  // Unused in this handler

    std::atomic<int>* counter = static_cast<std::atomic<int>*>(user_data);
    if (counter) {
        counter->fetch_add(1);
    }

    if (response_promise) {
        float ack = 1.0f;
        nimcp_bio_promise_complete(response_promise, &ack);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t unified_handler_static(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t response_promise,
                                            void* user_data) {
    std::atomic<int>* counter = static_cast<std::atomic<int>*>(user_data);
    if (counter) {
        counter->fetch_add(1);
    }

    if (response_promise) {
        float ack = 1.0f;
        nimcp_bio_promise_complete(response_promise, &ack);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Pipeline 1: STDP Event Processing
//=============================================================================

TEST_F(BioAsyncPlasticityE2ETest, STDPPipeline) {
    E2E_PIPELINE_START("STDP Event Processing via Bio-Async");

    // Stage 1: Register STDP event handler
    E2E_STAGE_BEGIN("Register STDP handlers", 100);

    bio_router_register_handler(stdp_ctx_, BIO_MSG_STDP_EVENT, stdp_handler_static);

    E2E_STAGE_END();

    // Stage 2: Send STDP events with various timings
    E2E_STAGE_BEGIN("Send STDP events", 300);

    const int NUM_STDP_EVENTS = 20;
    std::vector<nimcp_bio_promise_t> stdp_promises;

    for (int i = 0; i < NUM_STDP_EVENTS; i++) {
        bio_msg_stdp_event_t stdp_msg;
        bio_msg_init_header(&stdp_msg.header, BIO_MSG_STDP_EVENT,
                            BIO_MODULE_BRAIN, BIO_MODULE_STDP, sizeof(stdp_msg));
        stdp_msg.pre_neuron_id = i;
        stdp_msg.post_neuron_id = i + 100;
        stdp_msg.pre_spike_time_ms = 100.0f;
        stdp_msg.post_spike_time_ms = 100.0f + (i - 10) * 2.0f; // Range: -20ms to +20ms
        stdp_msg.delta_t_ms = stdp_msg.post_spike_time_ms - stdp_msg.pre_spike_time_ms;

        nimcp_bio_promise_t promise = bio_router_send_async(
            stdp_ctx_, &stdp_msg, sizeof(stdp_msg),
            BIO_CHANNEL_DOPAMINE); // Reward channel for plasticity
        E2E_ASSERT_NOT_NULL(promise, "Failed to send STDP event");

        stdp_promises.push_back(promise);
    }

    E2E_STAGE_END();

    // Stage 3: Process STDP events
    E2E_STAGE_BEGIN("Process STDP events", 500);

    auto start = std::chrono::steady_clock::now();
    while (stdp_events_received_.load() < NUM_STDP_EVENTS &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1000) {
        bio_router_process_inbox(stdp_ctx_, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    E2E_ASSERT(stdp_events_received_.load() >= NUM_STDP_EVENTS / 2,
               "Expected at least half of STDP events to be processed");

    E2E_STAGE_END();

    // Stage 4: Verify weight changes calculated
    E2E_STAGE_BEGIN("Verify STDP calculations", 100);

    float last_delta = last_weight_delta_.load();
    E2E_ASSERT(std::abs(last_delta) > 0.0f && std::abs(last_delta) < 0.02f,
               "Weight delta out of expected range");

    E2E_STAGE_END();

    // Cleanup
    for (auto promise : stdp_promises) {
        nimcp_bio_promise_destroy(promise);
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Homeostatic Plasticity
//=============================================================================

TEST_F(BioAsyncPlasticityE2ETest, HomeostaticPipeline) {
    E2E_PIPELINE_START("Homeostatic Plasticity via Bio-Async");

    static std::atomic<int> homeostatic_adjustments{0};
    std::atomic<float> total_adjustment{0.0f};

    // Stage 1: Re-register homeostatic handler with user_data
    E2E_STAGE_BEGIN("Register homeostatic handlers", 100);

    // Unregister and re-register with user_data
    bio_router_unregister_module(homeo_ctx_);
    bio_module_info_t homeo_info = {
        .module_id = BIO_MODULE_HOMEOSTATIC,
        .module_name = "homeostatic_test",
        .inbox_capacity = 100,
        .user_data = &homeostatic_adjustments
    };
    homeo_ctx_ = bio_router_register_module(&homeo_info);
    ASSERT_NE(homeo_ctx_, nullptr);

    bio_router_register_handler(homeo_ctx_, BIO_MSG_HOMEOSTATIC_ADJUSTMENT,
                                 homeostatic_handler_static);

    E2E_STAGE_END();

    // Stage 2: Simulate network activity requiring homeostatic adjustment
    E2E_STAGE_BEGIN("Send homeostatic adjustment requests", 300);

    const int NUM_NEURONS = 50;
    std::vector<nimcp_bio_promise_t> homeo_promises;

    for (int i = 0; i < NUM_NEURONS; i++) {
        // Define homeostatic adjustment message
        struct bio_msg_homeostatic_adjustment_t {
            bio_message_header_t header;
            uint32_t neuron_id;
            float current_rate;
            float target_rate;
            float adjustment_factor;
        };

        bio_msg_homeostatic_adjustment_t homeo_msg;
        bio_msg_init_header(&homeo_msg.header, BIO_MSG_HOMEOSTATIC_ADJUSTMENT,
                            BIO_MODULE_BRAIN, BIO_MODULE_HOMEOSTATIC, sizeof(homeo_msg));
        homeo_msg.neuron_id = i;
        homeo_msg.current_rate = 10.0f + (i % 10) * 2.0f; // 10-28 Hz
        homeo_msg.target_rate = 15.0f; // Target 15 Hz
        homeo_msg.adjustment_factor = (homeo_msg.target_rate - homeo_msg.current_rate) / homeo_msg.current_rate;

        total_adjustment.fetch_add(homeo_msg.adjustment_factor);

        nimcp_bio_promise_t promise = bio_router_send_async(
            homeo_ctx_, &homeo_msg, sizeof(homeo_msg),
            BIO_CHANNEL_SEROTONIN); // Slow channel for homeostatic changes
        E2E_ASSERT_NOT_NULL(promise, "Failed to send homeostatic adjustment");

        homeo_promises.push_back(promise);
    }

    E2E_STAGE_END();

    // Stage 3: Process homeostatic adjustments
    E2E_STAGE_BEGIN("Process homeostatic adjustments", 500);

    auto start = std::chrono::steady_clock::now();
    while (homeostatic_adjustments.load() < NUM_NEURONS &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1000) {
        bio_router_process_inbox(homeo_ctx_, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    E2E_ASSERT(homeostatic_adjustments.load() >= NUM_NEURONS / 2,
               "Expected at least half of homeostatic adjustments to be processed");

    E2E_STAGE_END();

    // Stage 4: Verify bio-async confidence decay for serotonin channel
    E2E_STAGE_BEGIN("Verify serotonin channel decay", 200);

    // Serotonin has slow decay, confidence should still be high
    if (!homeo_promises.empty()) {
        nimcp_bio_future_t future = nimcp_bio_promise_get_future(homeo_promises[0]);
        if (future) {
            float confidence = nimcp_bio_future_get_confidence(future);
            E2E_ASSERT(confidence > 0.5f,
                       "Serotonin channel confidence decayed too quickly");
        }
    }

    E2E_STAGE_END();

    // Cleanup
    for (auto promise : homeo_promises) {
        nimcp_bio_promise_destroy(promise);
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Neuromodulator-Gated Learning
//=============================================================================

TEST_F(BioAsyncPlasticityE2ETest, NeuromodulatorPipeline) {
    E2E_PIPELINE_START("Neuromodulator-Gated Learning via Bio-Async");

    static std::atomic<int> learning_rate_updates{0};

    // Stage 1: Re-register neuromodulator handlers with user_data
    E2E_STAGE_BEGIN("Register neuromodulator handlers", 100);

    // Unregister and re-register with user_data
    bio_router_unregister_module(neuromod_ctx_);
    bio_module_info_t neuromod_info = {
        .module_id = BIO_MODULE_NEUROMODULATOR,
        .module_name = "neuromod_test",
        .inbox_capacity = 100,
        .user_data = &learning_rate_updates
    };
    neuromod_ctx_ = bio_router_register_module(&neuromod_info);
    ASSERT_NE(neuromod_ctx_, nullptr);

    bio_router_register_handler(neuromod_ctx_, BIO_MSG_NEUROMODULATOR_RELEASE,
                                 neuromod_handler_static);

    bio_router_register_handler(neuromod_ctx_, BIO_MSG_LEARNING_RATE_UPDATE,
                                 learning_rate_handler_static);

    E2E_STAGE_END();

    // Stage 2: Send neuromodulator release events
    E2E_STAGE_BEGIN("Send neuromodulator releases", 300);

    const int NUM_RELEASES = 5;
    std::vector<nimcp_bio_promise_t> neuromod_promises;

    nimcp_bio_channel_type_t channels[] = {
        BIO_CHANNEL_DOPAMINE,
        BIO_CHANNEL_SEROTONIN,
        BIO_CHANNEL_NOREPINEPHRINE,
        BIO_CHANNEL_ACETYLCHOLINE,
        BIO_CHANNEL_DOPAMINE
    };

    for (int i = 0; i < NUM_RELEASES; i++) {
        bio_msg_neuromodulator_release_t neuromod_msg;
        bio_msg_init_header(&neuromod_msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                            BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR, sizeof(neuromod_msg));
        neuromod_msg.neuromodulator = channels[i];
        neuromod_msg.source_region = i;
        neuromod_msg.release_amount = 0.5f + i * 0.1f;
        neuromod_msg.current_concentration = 1.0f + i * 0.2f;
        neuromod_msg.diffusion_radius_um = 100.0f;

        nimcp_bio_promise_t promise = bio_router_send_async(
            neuromod_ctx_, &neuromod_msg, sizeof(neuromod_msg),
            channels[i]); // Use matching channel for release
        E2E_ASSERT_NOT_NULL(promise, "Failed to send neuromodulator release");

        neuromod_promises.push_back(promise);
    }

    E2E_STAGE_END();

    // Stage 3: Process neuromodulator releases
    E2E_STAGE_BEGIN("Process neuromodulator releases", 500);

    auto start = std::chrono::steady_clock::now();
    while (neuromod_releases_received_.load() < NUM_RELEASES &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1000) {
        bio_router_process_inbox(neuromod_ctx_, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    E2E_ASSERT(neuromod_releases_received_.load() >= NUM_RELEASES / 2,
               "Expected at least half of neuromodulator releases to be processed");

    E2E_STAGE_END();

    // Stage 4: Send learning rate updates modulated by neuromodulators
    E2E_STAGE_BEGIN("Send modulated learning rate updates", 300);

    const int NUM_LR_UPDATES = 10;
    std::vector<nimcp_bio_promise_t> lr_promises;

    for (int i = 0; i < NUM_LR_UPDATES; i++) {
        bio_msg_learning_rate_update_t lr_msg;
        bio_msg_init_header(&lr_msg.header, BIO_MSG_LEARNING_RATE_UPDATE,
                            BIO_MODULE_NEUROMODULATOR, BIO_MODULE_NEUROMODULATOR, sizeof(lr_msg));
        lr_msg.synapse_id = i;
        lr_msg.base_learning_rate = 0.001f;
        lr_msg.dopamine_level = 0.8f + i * 0.02f;
        lr_msg.serotonin_level = 0.5f;
        lr_msg.modulated_learning_rate = lr_msg.base_learning_rate * lr_msg.dopamine_level;

        nimcp_bio_promise_t promise = bio_router_send_async(
            neuromod_ctx_, &lr_msg, sizeof(lr_msg),
            BIO_CHANNEL_DOPAMINE); // Dopamine modulates learning rate
        E2E_ASSERT_NOT_NULL(promise, "Failed to send learning rate update");

        lr_promises.push_back(promise);
    }

    E2E_STAGE_END();

    // Stage 5: Process learning rate updates
    E2E_STAGE_BEGIN("Process learning rate updates", 500);

    start = std::chrono::steady_clock::now();
    while (learning_rate_updates.load() < NUM_LR_UPDATES &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1000) {
        bio_router_process_inbox(neuromod_ctx_, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    E2E_ASSERT(learning_rate_updates.load() >= NUM_LR_UPDATES / 2,
               "Expected at least half of learning rate updates to be processed");

    E2E_STAGE_END();

    // Cleanup
    for (auto promise : neuromod_promises) {
        nimcp_bio_promise_destroy(promise);
    }
    for (auto promise : lr_promises) {
        nimcp_bio_promise_destroy(promise);
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Multi-Plasticity Coordination
//=============================================================================

TEST_F(BioAsyncPlasticityE2ETest, MultiPlasticityPipeline) {
    E2E_PIPELINE_START("Multi-Plasticity Coordination via Bio-Async");

    static std::atomic<int> total_plasticity_events{0};

    // Stage 1: Re-register handlers with user_data
    E2E_STAGE_BEGIN("Register multi-plasticity handlers", 100);

    // Unregister and re-register with user_data
    bio_router_unregister_module(stdp_ctx_);
    bio_module_info_t stdp_info = {
        .module_id = BIO_MODULE_STDP,
        .module_name = "stdp_test",
        .inbox_capacity = 100,
        .user_data = &total_plasticity_events
    };
    stdp_ctx_ = bio_router_register_module(&stdp_info);
    ASSERT_NE(stdp_ctx_, nullptr);

    // Register category handler for all plasticity messages (0x0200-0x02FF)
    bio_router_register_category_handler(stdp_ctx_, 0x0200, unified_handler_static);

    E2E_STAGE_END();

    // Stage 2: Send mixed plasticity events
    E2E_STAGE_BEGIN("Send mixed plasticity events", 500);

    std::vector<nimcp_bio_promise_t> mixed_promises;

    // Send STDP events
    for (int i = 0; i < 5; i++) {
        bio_msg_stdp_event_t stdp_msg;
        bio_msg_init_header(&stdp_msg.header, BIO_MSG_STDP_EVENT,
                            BIO_MODULE_BRAIN, BIO_MODULE_STDP, sizeof(stdp_msg));
        stdp_msg.pre_neuron_id = i;
        stdp_msg.post_neuron_id = i + 100;
        stdp_msg.pre_spike_time_ms = 100.0f;
        stdp_msg.post_spike_time_ms = 105.0f;
        stdp_msg.delta_t_ms = 5.0f;

        nimcp_bio_promise_t promise = bio_router_send_async(
            stdp_ctx_, &stdp_msg, sizeof(stdp_msg), BIO_CHANNEL_DOPAMINE);
        if (promise) mixed_promises.push_back(promise);
    }

    // Send weight update requests
    for (int i = 0; i < 5; i++) {
        bio_msg_weight_update_request_t weight_msg;
        bio_msg_init_header(&weight_msg.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                            BIO_MODULE_BRAIN, BIO_MODULE_STDP, sizeof(weight_msg));
        weight_msg.synapse_id = i;
        weight_msg.pre_neuron_id = i;
        weight_msg.post_neuron_id = i + 100;
        weight_msg.weight_delta = 0.01f;
        weight_msg.learning_rate = 0.001f;
        weight_msg.eligibility_trace = 1.0f;
        weight_msg.clamp_to_bounds = true;
        weight_msg.min_weight = 0.0f;
        weight_msg.max_weight = 1.0f;

        nimcp_bio_promise_t promise = bio_router_send_async(
            stdp_ctx_, &weight_msg, sizeof(weight_msg), BIO_CHANNEL_DOPAMINE);
        if (promise) mixed_promises.push_back(promise);
    }

    // Send neuromodulator releases
    for (int i = 0; i < 3; i++) {
        bio_msg_neuromodulator_release_t neuromod_msg;
        bio_msg_init_header(&neuromod_msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
                            BIO_MODULE_BRAIN, BIO_MODULE_STDP, sizeof(neuromod_msg));
        neuromod_msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
        neuromod_msg.source_region = i;
        neuromod_msg.release_amount = 0.5f;
        neuromod_msg.current_concentration = 1.0f;
        neuromod_msg.diffusion_radius_um = 100.0f;

        nimcp_bio_promise_t promise = bio_router_send_async(
            stdp_ctx_, &neuromod_msg, sizeof(neuromod_msg), BIO_CHANNEL_DOPAMINE);
        if (promise) mixed_promises.push_back(promise);
    }

    E2E_STAGE_END();

    // Stage 3: Process all plasticity events
    E2E_STAGE_BEGIN("Process all plasticity events", 1000);

    auto start = std::chrono::steady_clock::now();
    while (total_plasticity_events.load() < static_cast<int>(mixed_promises.size()) &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 2000) {
        bio_router_process_inbox(stdp_ctx_, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    E2E_ASSERT(total_plasticity_events.load() >= static_cast<int>(mixed_promises.size()) / 2,
               "Expected at least half of plasticity events to be processed");

    E2E_STAGE_END();

    // Stage 4: Verify bio-async statistics
    E2E_STAGE_BEGIN("Verify bio-async statistics", 100);

    nimcp_bio_async_stats_t stats;
    nimcp_error_t err = nimcp_bio_async_get_stats(&stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get bio-async stats");

    // Check that dopamine channel was heavily used
    E2E_ASSERT(stats.channel_stats[BIO_CHANNEL_DOPAMINE].releases > 0,
               "Expected dopamine releases for plasticity events");

    E2E_STAGE_END();

    // Cleanup
    for (auto promise : mixed_promises) {
        nimcp_bio_promise_destroy(promise);
    }

    E2E_PIPELINE_END();
}
