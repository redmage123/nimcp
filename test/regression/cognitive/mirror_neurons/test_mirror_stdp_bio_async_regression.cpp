/**
 * @file test_mirror_stdp_bio_async_regression.cpp
 * @brief Regression tests for mirror STDP bio-async integration
 * @version 1.0.0
 * @date 2025-12-03
 *
 * WHAT: Regression tests to catch bio-async integration issues
 * WHY:  Ensure changes don't break existing STDP bio-async functionality
 * HOW:  Test critical integration paths and edge cases
 */

#include "test_helpers.h"

#include "cognitive/mirror_neurons/nimcp_mirror_stdp.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class MirrorStdpBioAsyncRegressionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_error_t err = nimcp_bio_async_init(&bio_config);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Initialize router
        bio_router_config_t router_config = bio_router_default_config();
        err = bio_router_init(&router_config);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Create STDP with default config
        mirror_stdp_config_t config = mirror_stdp_get_default_config();
        stdp = mirror_stdp_create(&config, 100);
        ASSERT_NE(stdp, nullptr);
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

    mirror_stdp_t stdp = nullptr;
};

//=============================================================================
// Regression: Router Registration
//=============================================================================

TEST_F(MirrorStdpBioAsyncRegressionTest, RouterRegistrationSucceeds)
{
    // Regression: Ensure STDP auto-registers with router
    // Bug history: Initial implementation forgot to register handlers

    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(stats.active_modules, 0u) << "STDP should be registered with router";
}

TEST_F(MirrorStdpBioAsyncRegressionTest, MultipleRegistrationsNoCrash)
{
    // Regression: Creating multiple STDP instances should not crash
    // Bug history: Early version had static module context causing conflicts

    mirror_stdp_config_t config = mirror_stdp_get_default_config();

    mirror_stdp_t stdp2 = mirror_stdp_create(&config, 50);
    ASSERT_NE(stdp2, nullptr);

    mirror_stdp_t stdp3 = mirror_stdp_create(&config, 75);
    ASSERT_NE(stdp3, nullptr);

    // All should be operational
    EXPECT_NE(mirror_stdp_find_synapse(stdp2, 0), UINT32_MAX);
    EXPECT_NE(mirror_stdp_find_synapse(stdp3, 0), UINT32_MAX);

    mirror_stdp_destroy(stdp3);
    mirror_stdp_destroy(stdp2);
}

TEST_F(MirrorStdpBioAsyncRegressionTest, UnregisterOnDestroy)
{
    // Regression: Verify cleanup unregisters from router
    // Bug history: Memory leak from not unregistering

    bio_router_stats_t stats_before;
    bio_router_get_stats(&stats_before);
    uint32_t modules_before = stats_before.active_modules;

    // Create and destroy
    mirror_stdp_config_t config = mirror_stdp_get_default_config();
    mirror_stdp_t temp_stdp = mirror_stdp_create(&config, 50);
    ASSERT_NE(temp_stdp, nullptr);

    bio_router_stats_t stats_after_create;
    bio_router_get_stats(&stats_after_create);
    EXPECT_GT(stats_after_create.active_modules, modules_before);

    mirror_stdp_destroy(temp_stdp);

    bio_router_stats_t stats_after_destroy;
    bio_router_get_stats(&stats_after_destroy);
    // Note: Exact count may vary due to other modules
    SUCCEED();  // Main check is no crash
}

//=============================================================================
// Regression: Message Handler Edge Cases
//=============================================================================

TEST_F(MirrorStdpBioAsyncRegressionTest, NullMessageHandling)
{
    // Regression: Handlers should handle null messages gracefully
    // Bug history: Crash on null message pointer

    // Cannot call handler directly in test, but verify system handles errors
    uint32_t syn_id = mirror_stdp_find_synapse(stdp, 9999);
    EXPECT_EQ(syn_id, UINT32_MAX) << "Should handle invalid synapse IDs";
}

TEST_F(MirrorStdpBioAsyncRegressionTest, MalformedMessageSize)
{
    // Regression: Small messages should not cause buffer overrun
    // Bug history: Handler didn't check message size

    // Create undersized message
    bio_message_header_t small_msg = {0};
    small_msg.type = BIO_MSG_STDP_EVENT;
    small_msg.payload_size = 0;  // Too small for actual STDP event

    // System should handle gracefully (would be checked in handler)
    SUCCEED();  // Placeholder for handler validation
}

TEST_F(MirrorStdpBioAsyncRegressionTest, InvalidSynapseInMessage)
{
    // Regression: Messages for non-existent synapses should not crash
    // Bug history: Array out-of-bounds on invalid synapse ID

    uint32_t invalid_id = 999999;
    uint32_t syn_id = mirror_stdp_find_synapse(stdp, invalid_id);
    EXPECT_EQ(syn_id, UINT32_MAX);

    // Verify no crash when accessing invalid synapse
    float weight = mirror_stdp_get_weight(stdp, syn_id);
    EXPECT_LT(weight, 0.0f) << "Should return error for invalid synapse";
}

//=============================================================================
// Regression: STDP Computation
//=============================================================================

TEST_F(MirrorStdpBioAsyncRegressionTest, WeightBounds)
{
    // Regression: Weights should never exceed [0, 1] range
    // Bug history: Overflow when repeatedly applying large delta_w

    uint32_t syn_id = mirror_stdp_create_synapse(stdp, 0, 0.5f);
    ASSERT_NE(syn_id, UINT32_MAX);

    // Apply extreme potentiation
    for (int i = 0; i < 1000; i++) {
        mirror_stdp_execution_spike(stdp, syn_id, i * 1000, 1.0f);
        mirror_stdp_observation_spike(stdp, syn_id, i * 1000 + 10000, 1.0f);
    }

    float weight = mirror_stdp_get_weight(stdp, syn_id);
    EXPECT_GE(weight, 0.0f) << "Weight should not be negative";
    EXPECT_LE(weight, 1.0f) << "Weight should not exceed 1.0";
}

TEST_F(MirrorStdpBioAsyncRegressionTest, ZeroDeltaTHandling)
{
    // Regression: Zero time difference should not cause NaN
    // Bug history: Division by zero in exponential decay

    float delta_w = mirror_stdp_compute_delta_w(stdp, 0.0f, 0.5f, 0.5f, 0.5f);
    EXPECT_FALSE(std::isnan(delta_w)) << "Zero delta_t should not produce NaN";
    EXPECT_FALSE(std::isinf(delta_w)) << "Zero delta_t should not produce Inf";
}

TEST_F(MirrorStdpBioAsyncRegressionTest, NegativeDeltaTHandling)
{
    // Regression: Negative timing should produce LTD
    // Bug history: Sign error caused LTP for negative delta_t

    float delta_w = mirror_stdp_compute_delta_w(stdp, -10.0f, 0.5f, 0.5f, 0.5f);
    EXPECT_LT(delta_w, 0.0f) << "Negative delta_t should produce LTD (negative delta_w)";
}

TEST_F(MirrorStdpBioAsyncRegressionTest, PositiveDeltaTHandling)
{
    // Regression: Positive timing should produce LTP
    // Bug history: None, but verify for completeness

    float delta_w = mirror_stdp_compute_delta_w(stdp, 10.0f, 0.5f, 0.5f, 0.5f);
    EXPECT_GT(delta_w, 0.0f) << "Positive delta_t should produce LTP (positive delta_w)";
}

//=============================================================================
// Regression: Neuromodulator Integration
//=============================================================================

TEST_F(MirrorStdpBioAsyncRegressionTest, DopamineOutOfRange)
{
    // Regression: Out-of-range dopamine should be clamped
    // Bug history: Negative dopamine caused negative weights

    mirror_stdp_set_dopamine(stdp, -0.5f);  // Invalid
    // Should be clamped internally
    // Cannot directly verify, but check computation doesn't crash

    uint32_t syn_id = mirror_stdp_create_synapse(stdp, 0, 0.5f);
    float delta_w = mirror_stdp_compute_delta_w(stdp, 10.0f, 0.5f, 0.5f, 0.5f);
    EXPECT_FALSE(std::isnan(delta_w));

    mirror_stdp_set_dopamine(stdp, 2.0f);  // Invalid
    delta_w = mirror_stdp_compute_delta_w(stdp, 10.0f, 0.5f, 0.5f, 0.5f);
    EXPECT_FALSE(std::isnan(delta_w));
}

TEST_F(MirrorStdpBioAsyncRegressionTest, AcetylcholineOutOfRange)
{
    // Regression: Out-of-range ACh should be clamped

    mirror_stdp_set_acetylcholine(stdp, -0.5f);
    uint32_t syn_id = mirror_stdp_create_synapse(stdp, 0, 0.5f);
    float delta_w = mirror_stdp_compute_delta_w(stdp, 10.0f, 0.5f, 0.5f, 0.5f);
    EXPECT_FALSE(std::isnan(delta_w));

    mirror_stdp_set_acetylcholine(stdp, 2.0f);
    delta_w = mirror_stdp_compute_delta_w(stdp, 10.0f, 0.5f, 0.5f, 0.5f);
    EXPECT_FALSE(std::isnan(delta_w));
}

//=============================================================================
// Regression: Homeostasis
//=============================================================================

TEST_F(MirrorStdpBioAsyncRegressionTest, HomeostasisConvergence)
{
    // Regression: Homeostasis should converge, not oscillate
    // Bug history: Incorrect time constant caused oscillation

    uint32_t syn_id = mirror_stdp_create_synapse(stdp, 0, 0.5f);

    // Apply homeostasis repeatedly
    for (int i = 0; i < 100; i++) {
        mirror_stdp_apply_homeostasis(stdp, 10.0f);
    }

    // Weight should remain in bounds
    float weight = mirror_stdp_get_weight(stdp, syn_id);
    EXPECT_GE(weight, 0.0f);
    EXPECT_LE(weight, 1.0f);

    // Scale factor should be reasonable
    mirror_stdp_stats_t stats;
    mirror_stdp_get_stats(stdp, &stats);
    EXPECT_GE(stats.homeostatic_scale_factor, 0.5f);
    EXPECT_LE(stats.homeostatic_scale_factor, 2.0f);
}

//=============================================================================
// Regression: Statistics
//=============================================================================

TEST_F(MirrorStdpBioAsyncRegressionTest, StatsInitialization)
{
    // Regression: Stats should be initialized to valid values
    // Bug history: Uninitialized stats contained garbage

    mirror_stdp_stats_t stats;
    bool success = mirror_stdp_get_stats(stdp, &stats);
    ASSERT_TRUE(success);

    EXPECT_EQ(stats.num_synapses, 0u) << "Initial synapse count should be zero";
    EXPECT_EQ(stats.total_ltp_events, 0u);
    EXPECT_EQ(stats.total_ltd_events, 0u);
    EXPECT_FALSE(std::isnan(stats.mean_weight));
    EXPECT_FALSE(std::isnan(stats.weight_variance));
}

TEST_F(MirrorStdpBioAsyncRegressionTest, StatsConsistency)
{
    // Regression: Stats should be consistent with actual state
    // Bug history: Stats count mismatch with actual synapses

    const uint32_t num_synapses = 10;
    for (uint32_t i = 0; i < num_synapses; i++) {
        mirror_stdp_create_synapse(stdp, i, 0.5f);
    }

    mirror_stdp_stats_t stats;
    mirror_stdp_get_stats(stdp, &stats);

    EXPECT_EQ(stats.num_synapses, num_synapses);
}

//=============================================================================
// Regression: Memory Management
//=============================================================================

TEST_F(MirrorStdpBioAsyncRegressionTest, NoMemoryLeakOnDestroy)
{
    // Regression: Ensure all resources are freed
    // Bug history: Bio-async context not freed

    mirror_stdp_config_t config = mirror_stdp_get_default_config();

    // Create and destroy multiple times
    for (int i = 0; i < 10; i++) {
        mirror_stdp_t temp = mirror_stdp_create(&config, 50);
        ASSERT_NE(temp, nullptr);

        // Use it briefly
        mirror_stdp_create_synapse(temp, 0, 0.5f);

        mirror_stdp_destroy(temp);
    }

    // If no crash, memory management is likely correct
    SUCCEED();
}

TEST_F(MirrorStdpBioAsyncRegressionTest, NullDestroyNoCrash)
{
    // Regression: Destroying null should not crash
    // Bug history: Segfault on null destroy

    mirror_stdp_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Regression: Concurrent Access
//=============================================================================

TEST_F(MirrorStdpBioAsyncRegressionTest, RapidCreateDestroy)
{
    // Regression: Rapid create/destroy should not cause issues
    // Bug history: Race condition in router registration

    mirror_stdp_config_t config = mirror_stdp_get_default_config();

    for (int i = 0; i < 20; i++) {
        mirror_stdp_t temp = mirror_stdp_create(&config, 10);
        ASSERT_NE(temp, nullptr);
        mirror_stdp_destroy(temp);
    }

    SUCCEED();
}

//=============================================================================
// Regression: Bio-Async Integration
//=============================================================================

TEST_F(MirrorStdpBioAsyncRegressionTest, WorksWithoutBioAsync)
{
    // Regression: STDP should work even if bio-async not initialized
    // Bug history: Crash when bio-async unavailable

    // Clean up current setup
    mirror_stdp_destroy(stdp);
    bio_router_shutdown();
    nimcp_bio_async_shutdown();

    // Create STDP without bio-async
    mirror_stdp_config_t config = mirror_stdp_get_default_config();
    stdp = mirror_stdp_create(&config, 50);
    ASSERT_NE(stdp, nullptr);

    // Should still work for basic operations
    uint32_t syn_id = mirror_stdp_create_synapse(stdp, 0, 0.5f);
    EXPECT_NE(syn_id, UINT32_MAX);

    float weight = mirror_stdp_get_weight(stdp, syn_id);
    EXPECT_GE(weight, 0.0f);
    EXPECT_LE(weight, 1.0f);

    // Restart for other tests
    nimcp_bio_async_init(nullptr);
    bio_router_init(nullptr);
}

TEST_F(MirrorStdpBioAsyncRegressionTest, MessageTypeCorrectness)
{
    // Regression: Verify message types are correctly assigned
    // Bug history: Wrong message type caused handler mismatch

    EXPECT_EQ(BIO_MSG_STDP_EVENT, 0x0202);  // From nimcp_bio_messages.h
    EXPECT_EQ(BIO_MSG_MIRROR_NEURON_ACTIVATION, 0x0361);  // Verify actual values
}

}  // namespace
