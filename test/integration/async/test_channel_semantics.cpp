//=============================================================================
// test_channel_semantics.cpp - Neuromodulator Channel Semantics Tests
//=============================================================================
/**
 * @file test_channel_semantics.cpp
 * @brief Tests for biologically-realistic neuromodulator channel timing
 *
 * Tests cover:
 * - DOPAMINE channel timing (fast completion, reward)
 * - SEROTONIN channel timing (slow, long decay, mood)
 * - NOREPINEPHRINE channel (alerting, priority)
 * - ACETYLCHOLINE channel (fast attention switching)
 * - Confidence decay over time
 *
 * @version 1.0.0
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <cmath>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_biological_timescales.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ChannelSemanticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
        config.enable_statistics = true;
        config.enable_logging = false;
        config.use_real_time = false; // Use accelerated time for tests
        config.time_acceleration = 10.0f; // 10x speedup
        ASSERT_EQ(nimcp_bio_async_init(&config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_bio_async_shutdown();
    }

    // Helper to measure decay over time
    struct DecayMeasurement {
        float time_ms;
        float confidence;
    };

    std::vector<DecayMeasurement> MeasureDecay(nimcp_bio_future_t future,
                                                int num_samples,
                                                float interval_ms) {
        std::vector<DecayMeasurement> measurements;

        for (int i = 0; i < num_samples; i++) {
            float confidence = nimcp_bio_future_get_confidence(future);
            float age = nimcp_bio_future_get_age_ms(future);
            measurements.push_back({age, confidence});

            // Advance time
            nimcp_bio_async_step(interval_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(interval_ms / 10)));
        }

        return measurements;
    }

    // Calculate decay rate from measurements
    float CalculateDecayRate(const std::vector<DecayMeasurement>& measurements) {
        if (measurements.size() < 2) return 0.0f;

        // Fit exponential decay: C(t) = C0 * exp(-t/tau)
        // ln(C) = ln(C0) - t/tau
        float sum_t = 0.0f, sum_ln_c = 0.0f;
        int count = 0;

        for (const auto& m : measurements) {
            if (m.confidence > 0.01f) { // Avoid log(0)
                sum_t += m.time_ms;
                sum_ln_c += std::log(m.confidence);
                count++;
            }
        }

        if (count < 2) return 0.0f;

        // Slope = -1/tau
        float mean_t = sum_t / count;
        float mean_ln_c = sum_ln_c / count;

        float numerator = 0.0f, denominator = 0.0f;
        for (const auto& m : measurements) {
            if (m.confidence > 0.01f) {
                float dt = m.time_ms - mean_t;
                float dlnc = std::log(m.confidence) - mean_ln_c;
                numerator += dt * dlnc;
                denominator += dt * dt;
            }
        }

        if (denominator < 1e-6f) return 0.0f;

        float slope = numerator / denominator;
        return -1.0f / slope; // tau (decay constant)
    }
};

//=============================================================================
// DOPAMINE Channel Tests (Reward/Goal Completion)
//=============================================================================

TEST_F(ChannelSemanticsTest, DopamineChannelFastCompletion) {
    // Create dopamine promise
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    // Complete with reward signal
    float reward = 1.0f;
    auto start = std::chrono::high_resolution_clock::now();

    ASSERT_EQ(nimcp_bio_promise_complete(promise, &reward), NIMCP_SUCCESS);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Dopamine should complete very quickly (computational)
    EXPECT_LT(duration_us, 1000) << "DA completion should be under 1ms";

    // Verify high initial confidence
    float confidence = nimcp_bio_future_get_confidence(future);
    EXPECT_GT(confidence, 0.95f) << "DA should have very high initial confidence";

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(ChannelSemanticsTest, DopamineMediumDecay) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    float reward = 1.0f;
    nimcp_bio_promise_complete(promise, &reward);

    // Measure decay over time
    auto measurements = MeasureDecay(future, 20, 50.0f); // 20 samples, 50ms apart

    // Calculate decay constant
    float tau = CalculateDecayRate(measurements);

    // Dopamine should have medium decay (computational scaled)
    // Expected: ~200ms (computational), allow 100-400ms range
    EXPECT_GT(tau, 100.0f) << "DA decay too fast";
    EXPECT_LT(tau, 400.0f) << "DA decay too slow";

    // Verify confidence dropped significantly after decay period
    EXPECT_LT(measurements.back().confidence, 0.5f)
        << "DA should decay significantly over " << measurements.back().time_ms << "ms";

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// SEROTONIN Channel Tests (Mood/State Coordination)
//=============================================================================

TEST_F(ChannelSemanticsTest, SerotoninSlowDecay) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_SEROTONIN, sizeof(float));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    float mood_state = 0.8f;
    nimcp_bio_promise_complete(promise, &mood_state);

    // Measure decay over longer period
    auto measurements = MeasureDecay(future, 30, 100.0f); // 30 samples, 100ms apart

    float tau = CalculateDecayRate(measurements);

    // Serotonin should have long decay (computational scaled)
    // Expected: ~1000ms+ (computational), allow 800-2000ms range
    EXPECT_GT(tau, 800.0f) << "5-HT decay too fast";

    // After same period as dopamine, 5-HT should still have measurable confidence
    // With tau=1000ms: e^(-1000/1000) = 0.368
    auto serotonin_at_1s = measurements[std::min(10, (int)measurements.size()-1)];
    EXPECT_GT(serotonin_at_1s.confidence, 0.3f)
        << "5-HT should retain measurable confidence at " << serotonin_at_1s.time_ms << "ms";

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(ChannelSemanticsTest, SerotoninSustainedSignal) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_SEROTONIN, sizeof(float));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    float mood = 0.75f;
    nimcp_bio_promise_complete(promise, &mood);

    // Check confidence at multiple time points
    float conf_immediate = nimcp_bio_future_get_confidence(future);

    nimcp_bio_async_step(500.0f);
    float conf_500ms = nimcp_bio_future_get_confidence(future);

    nimcp_bio_async_step(500.0f);
    float conf_1000ms = nimcp_bio_future_get_confidence(future);

    // Serotonin should decay slowly
    EXPECT_GT(conf_immediate, 0.9f);
    EXPECT_GT(conf_500ms, 0.7f) << "5-HT should still be strong at 500ms";
    EXPECT_GT(conf_1000ms, 0.5f) << "5-HT should still be moderate at 1000ms";

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// NOREPINEPHRINE Channel Tests (Alerting/Priority)
//=============================================================================

TEST_F(ChannelSemanticsTest, NorepinephrineFastAlert) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_NOREPINEPHRINE, sizeof(float));
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    float alert_level = 0.9f;
    auto start = std::chrono::high_resolution_clock::now();

    ASSERT_EQ(nimcp_bio_promise_complete(promise, &alert_level), NIMCP_SUCCESS);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // NE should complete quickly (phasic alert)
    EXPECT_LT(duration_us, 1000) << "NE alert should be fast";

    // Verify high initial confidence
    float confidence = nimcp_bio_future_get_confidence(future);
    EXPECT_GT(confidence, 0.95f);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(ChannelSemanticsTest, NorepinephrineMediumDecay) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_NOREPINEPHRINE, sizeof(float));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    float alert = 0.95f;
    nimcp_bio_promise_complete(promise, &alert);

    auto measurements = MeasureDecay(future, 25, 40.0f);
    float tau = CalculateDecayRate(measurements);

    // NE should have medium decay (computational)
    // Expected: ~300ms, allow 200-500ms range
    EXPECT_GT(tau, 200.0f) << "NE decay too fast";
    EXPECT_LT(tau, 500.0f) << "NE decay too slow";

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// ACETYLCHOLINE Channel Tests (Fast Attention)
//=============================================================================

TEST_F(ChannelSemanticsTest, AcetylcholineVeryFastSwitching) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_ACETYLCHOLINE, sizeof(float));
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    float attention = 1.0f;
    auto start = std::chrono::high_resolution_clock::now();

    ASSERT_EQ(nimcp_bio_promise_complete(promise, &attention), NIMCP_SUCCESS);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // ACh should be fastest
    EXPECT_LT(duration_us, 500) << "ACh should complete in <500us";

    float confidence = nimcp_bio_future_get_confidence(future);
    EXPECT_GT(confidence, 0.98f) << "ACh should have highest initial confidence";

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(ChannelSemanticsTest, AcetylcholineShortDecay) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_ACETYLCHOLINE, sizeof(float));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    float attn = 1.0f;
    nimcp_bio_promise_complete(promise, &attn);

    auto measurements = MeasureDecay(future, 15, 20.0f);
    float tau = CalculateDecayRate(measurements);

    // ACh should have shortest decay (computational)
    // Expected: ~50ms, allow 30-100ms range
    EXPECT_GT(tau, 30.0f) << "ACh decay too fast";
    EXPECT_LT(tau, 150.0f) << "ACh decay too slow";

    // Should decay quickly
    EXPECT_LT(measurements.back().confidence, 0.3f)
        << "ACh should decay rapidly";

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Comparative Channel Tests
//=============================================================================

TEST_F(ChannelSemanticsTest, CompareChannelDecayRates) {
    // Create promises for all channels
    auto create_and_complete = [](nimcp_bio_channel_type_t channel) -> nimcp_bio_future_t {
        auto promise = nimcp_bio_promise_create(channel, sizeof(float));
        auto future = nimcp_bio_promise_get_future(promise);
        float value = 1.0f;
        nimcp_bio_promise_complete(promise, &value);
        nimcp_bio_promise_destroy(promise); // Future still valid
        return future;
    };

    auto da_future = create_and_complete(BIO_CHANNEL_DOPAMINE);
    auto ht_future = create_and_complete(BIO_CHANNEL_SEROTONIN);
    auto ne_future = create_and_complete(BIO_CHANNEL_NOREPINEPHRINE);
    auto ach_future = create_and_complete(BIO_CHANNEL_ACETYLCHOLINE);

    // Sample all at same time point (500ms)
    nimcp_bio_async_step(500.0f);

    float da_conf = nimcp_bio_future_get_confidence(da_future);
    float ht_conf = nimcp_bio_future_get_confidence(ht_future);
    float ne_conf = nimcp_bio_future_get_confidence(ne_future);
    float ach_conf = nimcp_bio_future_get_confidence(ach_future);

    // Expected ordering at 500ms: ACh < DA ≈ NE < 5-HT
    EXPECT_LT(ach_conf, da_conf) << "ACh should decay faster than DA";
    EXPECT_LT(ach_conf, ne_conf) << "ACh should decay faster than NE";
    EXPECT_LT(da_conf, ht_conf) << "DA should decay faster than 5-HT";
    EXPECT_LT(ne_conf, ht_conf) << "NE should decay faster than 5-HT";

    // 5-HT should still be relatively high (with tau=1000ms: e^(-500/1000) = 0.606)
    EXPECT_GT(ht_conf, 0.5f) << "5-HT should still have decent confidence at 500ms";

    // ACh should be very low
    EXPECT_LT(ach_conf, 0.2f) << "ACh should be mostly decayed at 500ms";

    nimcp_bio_future_destroy(da_future);
    nimcp_bio_future_destroy(ht_future);
    nimcp_bio_future_destroy(ne_future);
    nimcp_bio_future_destroy(ach_future);
}

TEST_F(ChannelSemanticsTest, ChannelDecayAfterWait) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    float reward = 1.0f;
    nimcp_bio_promise_complete(promise, &reward);

    // Wait for result
    float result;
    ASSERT_EQ(nimcp_bio_future_wait(future, &result, 100), NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(result, 1.0f);

    // Even after successful wait, confidence should start high
    float conf_immediate = nimcp_bio_future_get_confidence(future);
    EXPECT_GT(conf_immediate, 0.9f);

    // But decay over time
    nimcp_bio_async_step(1000.0f);
    float conf_later = nimcp_bio_future_get_confidence(future);
    EXPECT_LT(conf_later, conf_immediate);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Confidence Decay Edge Cases
//=============================================================================

TEST_F(ChannelSemanticsTest, ConfidenceNeverNegative) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_ACETYLCHOLINE, sizeof(float));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    float value = 1.0f;
    nimcp_bio_promise_complete(promise, &value);

    // Advance time significantly
    for (int i = 0; i < 100; i++) {
        nimcp_bio_async_step(100.0f);
        float conf = nimcp_bio_future_get_confidence(future);
        EXPECT_GE(conf, 0.0f) << "Confidence should never be negative at t=" << (i*100);
        EXPECT_LE(conf, 1.0f) << "Confidence should never exceed 1.0 at t=" << (i*100);
    }

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(ChannelSemanticsTest, PendingFutureZeroConfidence) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    // Before completion, confidence should be 0 or very low
    float conf = nimcp_bio_future_get_confidence(future);
    EXPECT_EQ(conf, 0.0f) << "Pending future should have zero confidence";

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(ChannelSemanticsTest, MultipleChannelsIndependentDecay) {
    // Create multiple futures on different channels
    auto da_promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
    auto ht_promise = nimcp_bio_promise_create(BIO_CHANNEL_SEROTONIN, sizeof(float));

    auto da_future = nimcp_bio_promise_get_future(da_promise);
    auto ht_future = nimcp_bio_promise_get_future(ht_promise);

    float value = 1.0f;
    nimcp_bio_promise_complete(da_promise, &value);
    nimcp_bio_promise_complete(ht_promise, &value);

    // Track confidence independently
    for (int i = 0; i < 10; i++) {
        float da_conf = nimcp_bio_future_get_confidence(da_future);
        float ht_conf = nimcp_bio_future_get_confidence(ht_future);

        // DA should always decay faster
        if (i > 2) { // After some time
            EXPECT_LT(da_conf, ht_conf)
                << "DA should decay faster than 5-HT at step " << i;
        }

        nimcp_bio_async_step(100.0f);
    }

    nimcp_bio_future_destroy(da_future);
    nimcp_bio_future_destroy(ht_future);
    nimcp_bio_promise_destroy(da_promise);
    nimcp_bio_promise_destroy(ht_promise);
}

//=============================================================================
// Statistics Verification
//=============================================================================

TEST_F(ChannelSemanticsTest, VerifyChannelStatistics) {
    nimcp_bio_async_reset_stats();

    // Create and complete futures on different channels
    for (int i = 0; i < 5; i++) {
        auto da_promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
        float value = 1.0f;
        nimcp_bio_promise_complete(da_promise, &value);
        nimcp_bio_promise_destroy(da_promise);
    }

    for (int i = 0; i < 3; i++) {
        auto ach_promise = nimcp_bio_promise_create(BIO_CHANNEL_ACETYLCHOLINE, sizeof(float));
        float value = 1.0f;
        nimcp_bio_promise_complete(ach_promise, &value);
        nimcp_bio_promise_destroy(ach_promise);
    }

    nimcp_bio_async_stats_t stats;
    ASSERT_EQ(nimcp_bio_async_get_stats(&stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.channel_stats[BIO_CHANNEL_DOPAMINE].releases, 5u);
    EXPECT_EQ(stats.channel_stats[BIO_CHANNEL_ACETYLCHOLINE].releases, 3u);
    EXPECT_GE(stats.total_futures_created, 8u);
    EXPECT_GE(stats.total_futures_completed, 8u);
}

// End of tests
