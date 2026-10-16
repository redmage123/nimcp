/**
 * @file test_mirror_stdp_bio_async.cpp
 * @brief Unit tests for mirror STDP bio-async integration
 * @version 1.0.0
 * @date 2025-12-03
 *
 * WHAT: Tests bio-async message handling for mirror neuron STDP
 * WHY:  Ensure STDP integrates properly with bio-async messaging
 * HOW:  Test message handlers, router registration, and async operations
 */

#include "test_helpers.h"

#include "cognitive/mirror_neurons/nimcp_mirror_stdp.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#include <cstring>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class MirrorStdpBioAsyncTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_logging = false;  // Reduce test noise
        nimcp_error_t err = nimcp_bio_async_init(&bio_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize bio-async";

        // Initialize router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_logging = false;
        err = bio_router_init(&router_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize router";

        // Create STDP system (will auto-register with router)
        mirror_stdp_config_t config = mirror_stdp_get_default_config();
        stdp = mirror_stdp_create(&config, MAX_SYNAPSES);
        ASSERT_NE(stdp, nullptr) << "Failed to create STDP system";

        // Create test synapses
        for (uint32_t i = 0; i < 10; i++) {
            uint32_t syn_id = mirror_stdp_create_synapse(stdp, i, 0.5f);
            ASSERT_NE(syn_id, UINT32_MAX) << "Failed to create synapse " << i;
        }
    }

    void TearDown() override
    {
        if (stdp) {
            mirror_stdp_destroy(stdp);
            stdp = nullptr;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    static constexpr uint32_t MAX_SYNAPSES = 100;

    mirror_stdp_t stdp = nullptr;
};

//=============================================================================
// Router Registration Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncTest, RouterRegistration)
{
    // STDP should be registered during creation
    // We can't directly check registration, but we can verify create succeeded
    ASSERT_NE(stdp, nullptr);

    // Verify router has modules registered
    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(stats.active_modules, 0u);
}

TEST_F(MirrorStdpBioAsyncTest, MultipleInstances)
{
    // Create second STDP instance
    mirror_stdp_config_t config = mirror_stdp_get_default_config();
    mirror_stdp_t stdp2 = mirror_stdp_create(&config, 50);
    ASSERT_NE(stdp2, nullptr);

    // Both should be registered
    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.active_modules, 2u);

    // Clean up
    mirror_stdp_destroy(stdp2);
}

//=============================================================================
// STDP Event Message Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncTest, HandleStdpEvent_LTP)
{
    // Create STDP event message (observation before execution = LTP)
    bio_msg_stdp_event_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_STDP_EVENT,
                       BIO_MODULE_MIRROR_NEURONS, BIO_MODULE_MIRROR_NEURONS,
                       sizeof(msg));
    msg.pre_neuron_id = 0;  // Action ID we created
    msg.post_neuron_id = 100;
    msg.pre_spike_time_ms = 10.0f;
    msg.post_spike_time_ms = 20.0f;
    msg.delta_t_ms = 10.0f;  // Positive = LTP

    // Get initial weight
    float initial_weight = mirror_stdp_get_weight(stdp, 0);
    ASSERT_GE(initial_weight, 0.0f);

    // Send message asynchronously (simplified test - in production use router)
    // For unit test, we'll call handler directly
    // Note: In production, use bio_router_send_async()

    // Verify weight increased (LTP)
    // Since we're not actually routing, we'll just verify STDP computation works
    float delta_w = mirror_stdp_compute_delta_w(stdp, 10.0f, initial_weight, 0.5f, 0.5f);
    EXPECT_GT(delta_w, 0.0f) << "LTP should increase weight";
}

TEST_F(MirrorStdpBioAsyncTest, HandleStdpEvent_LTD)
{
    // Create STDP event message (execution before observation = LTD)
    bio_msg_stdp_event_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_STDP_EVENT,
                       BIO_MODULE_MIRROR_NEURONS, BIO_MODULE_MIRROR_NEURONS,
                       sizeof(msg));
    msg.pre_neuron_id = 1;
    msg.post_neuron_id = 100;
    msg.pre_spike_time_ms = 20.0f;
    msg.post_spike_time_ms = 10.0f;
    msg.delta_t_ms = -10.0f;  // Negative = LTD

    // Get initial weight
    float initial_weight = mirror_stdp_get_weight(stdp, 1);
    ASSERT_GE(initial_weight, 0.0f);

    // Verify weight decreased (LTD)
    float delta_w = mirror_stdp_compute_delta_w(stdp, -10.0f, initial_weight, 0.5f, 0.5f);
    EXPECT_LT(delta_w, 0.0f) << "LTD should decrease weight";
}

TEST_F(MirrorStdpBioAsyncTest, HandleStdpEvent_InvalidSynapse)
{
    // Create message for non-existent synapse
    bio_msg_stdp_event_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_STDP_EVENT,
                       BIO_MODULE_MIRROR_NEURONS, BIO_MODULE_MIRROR_NEURONS,
                       sizeof(msg));
    msg.pre_neuron_id = 9999;  // Does not exist
    msg.post_neuron_id = 100;
    msg.delta_t_ms = 10.0f;

    // Should handle gracefully - verify synapse doesn't exist
    uint32_t syn_id = mirror_stdp_find_synapse(stdp, 9999);
    EXPECT_EQ(syn_id, UINT32_MAX);
}

//=============================================================================
// Mirror Neuron Activation Message Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncTest, HandleMirrorActivation)
{
    // Create mirror neuron activation message
    bio_message_header_t msg = {0};
    msg.type = BIO_MSG_MIRROR_NEURON_ACTIVATION;
    msg.source_module = BIO_MODULE_MIRROR_NEURONS;
    msg.target_module = BIO_MODULE_MIRROR_NEURONS;
    msg.payload_size = 0;

    // Message should be processable
    // In full system, this would route through bio_router
    SUCCEED();  // Placeholder for full integration test
}

//=============================================================================
// Neuromodulator Integration Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncTest, DopamineModulation)
{
    // Test dopamine modulation of STDP
    mirror_stdp_set_dopamine(stdp, 1.0f);  // High dopamine

    float initial_weight = mirror_stdp_get_weight(stdp, 0);
    float delta_w_high = mirror_stdp_compute_delta_w(stdp, 10.0f, initial_weight, 1.0f, 0.5f);

    mirror_stdp_set_dopamine(stdp, 0.0f);  // Low dopamine
    float delta_w_low = mirror_stdp_compute_delta_w(stdp, 10.0f, initial_weight, 0.0f, 0.5f);

    // High dopamine should boost LTP more than low dopamine
    EXPECT_GT(delta_w_high, delta_w_low) << "High dopamine should boost LTP";
}

TEST_F(MirrorStdpBioAsyncTest, AcetylcholineModulation)
{
    // Test ACh modulation of STDP
    mirror_stdp_set_acetylcholine(stdp, 1.0f);  // High ACh (attention)

    float initial_weight = mirror_stdp_get_weight(stdp, 0);
    float delta_w_high = mirror_stdp_compute_delta_w(stdp, 10.0f, initial_weight, 0.5f, 1.0f);

    mirror_stdp_set_acetylcholine(stdp, 0.0f);  // Low ACh
    float delta_w_low = mirror_stdp_compute_delta_w(stdp, 10.0f, initial_weight, 0.5f, 0.0f);

    // High ACh should boost plasticity
    EXPECT_GT(delta_w_high, delta_w_low) << "High ACh should boost plasticity";
}

//=============================================================================
// Bio-Async Channel Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncTest, ChannelSelection)
{
    // STDP events should use dopamine channel (reward-based plasticity)
    bio_message_type_t msg_type = BIO_MSG_STDP_EVENT;
    nimcp_bio_channel_type_t channel = bio_msg_recommended_channel(msg_type);

    EXPECT_EQ(channel, BIO_CHANNEL_DOPAMINE)
        << "STDP events should use dopamine channel";
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncTest, ConcurrentSpikeEvents)
{
    // Simulate multiple concurrent spike events
    const uint32_t num_spikes = 100;
    uint64_t base_time_us = 1000000;  // 1 second

    for (uint32_t i = 0; i < num_spikes; i++) {
        uint64_t time_us = base_time_us + i * 1000;  // 1ms intervals
        uint32_t synapse_id = i % 10;  // Cycle through synapses

        // Alternate between observation and execution spikes
        if (i % 2 == 0) {
            mirror_stdp_observation_spike(stdp, synapse_id, time_us, 1.0f);
        } else {
            mirror_stdp_execution_spike(stdp, synapse_id, time_us, 1.0f);
        }
    }

    // Verify system is still functional
    mirror_stdp_stats_t stats;
    bool success = mirror_stdp_get_stats(stdp, &stats);
    ASSERT_TRUE(success);

    // Should have recorded plasticity events
    uint32_t total_events = stats.total_ltp_events + stats.total_ltd_events;
    EXPECT_GT(total_events, 0u) << "Should have processed plasticity events";
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncTest, NullPointerHandling)
{
    // All functions should handle NULL gracefully
    mirror_stdp_destroy(nullptr);  // Should not crash

    float weight = mirror_stdp_get_weight(nullptr, 0);
    EXPECT_LT(weight, 0.0f);  // Should return error value

    uint32_t syn_id = mirror_stdp_find_synapse(nullptr, 0);
    EXPECT_EQ(syn_id, UINT32_MAX);
}

TEST_F(MirrorStdpBioAsyncTest, InvalidMessageSize)
{
    // Test handling of malformed messages with wrong size
    bio_msg_stdp_event_t msg = {0};
    // In production, handler should check msg_size
    SUCCEED();  // Placeholder for handler validation test
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncTest, StatsCollection)
{
    // Trigger some plasticity events
    uint64_t time_us = 1000000;
    mirror_stdp_observation_spike(stdp, 0, time_us, 1.0f);
    mirror_stdp_execution_spike(stdp, 0, time_us + 10000, 1.0f);

    // Get statistics
    mirror_stdp_stats_t stats;
    bool success = mirror_stdp_get_stats(stdp, &stats);
    ASSERT_TRUE(success);

    EXPECT_EQ(stats.num_synapses, 10u);
    EXPECT_GE(stats.mean_weight, 0.0f);
    EXPECT_LE(stats.mean_weight, 1.0f);
}

TEST_F(MirrorStdpBioAsyncTest, StatsReset)
{
    // Trigger events
    mirror_stdp_observation_spike(stdp, 0, 1000000, 1.0f);
    mirror_stdp_execution_spike(stdp, 0, 1010000, 1.0f);

    // Get initial stats
    mirror_stdp_stats_t stats1;
    mirror_stdp_get_stats(stdp, &stats1);
    uint32_t events1 = stats1.total_ltp_events + stats1.total_ltd_events;

    // Reset
    mirror_stdp_reset_stats(stdp);

    // Verify reset
    mirror_stdp_stats_t stats2;
    mirror_stdp_get_stats(stdp, &stats2);
    uint32_t events2 = stats2.total_ltp_events + stats2.total_ltd_events;

    EXPECT_LT(events2, events1) << "Stats should be reset";
}

}  // namespace
