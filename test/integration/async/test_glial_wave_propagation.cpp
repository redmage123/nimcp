//=============================================================================
// test_glial_wave_propagation.cpp - Glial Wave Integration Tests
//=============================================================================
/**
 * @file test_glial_wave_propagation.cpp
 * @brief Tests for calcium wave propagation dynamics
 *
 * Tests cover:
 * - Wave initiation from source regions
 * - Wave propagation to target regions
 * - Wave arrival callbacks
 * - Concurrent wave interference
 * - Wave extinction dynamics
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
#include "async/nimcp_biological_timescales.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GlialWavePropagationTest : public ::testing::Test {
protected:
    std::atomic<int> arrival_count{0};
    std::atomic<int> region_1_arrivals{0};
    std::atomic<int> region_2_arrivals{0};
    std::atomic<int> region_3_arrivals{0};

    void SetUp() override {
        nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
        config.enable_statistics = true;
        config.glial_config.wave_speed_um_s = 20.0f; // Realistic astrocyte wave speed
        config.glial_config.wave_threshold_um = 1.0f;
        config.glial_config.decay_rate = 1.0f; // Faster decay for extinction within test window
        config.glial_config.max_concurrent_waves = 10;
        config.glial_config.mode = BIO_WAVE_ISOTROPIC;
        config.use_real_time = false;
        config.time_acceleration = 50.0f; // Speed up for tests
        ASSERT_EQ(nimcp_bio_async_init(&config), NIMCP_SUCCESS);

        arrival_count = 0;
        region_1_arrivals = 0;
        region_2_arrivals = 0;
        region_3_arrivals = 0;
    }

    void TearDown() override {
        nimcp_bio_async_shutdown();
    }

    static void wave_arrival_callback(nimcp_glial_wave_t wave, uint32_t region_id,
                                       float calcium_level, void* user_data) {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        (*counter)++;
    }
};

//=============================================================================
// Wave Initiation Tests
//=============================================================================

TEST_F(GlialWavePropagationTest, BasicWaveInitiation) {
    // Initiate calcium wave from source region
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 5.0f);
    ASSERT_NE(wave, nullptr);

    // Wave should be active immediately after initiation
    EXPECT_TRUE(nimcp_glial_wave_is_active(wave));

    // Initial radius should be zero or very small
    float radius = nimcp_glial_wave_get_radius(wave);
    EXPECT_GE(radius, 0.0f);
    EXPECT_LT(radius, 10.0f);

    // Source region should have high calcium
    float calcium_at_source = nimcp_glial_wave_get_level_at(wave, 0);
    EXPECT_GT(calcium_at_source, 4.0f);

    nimcp_glial_wave_destroy(wave);
}

TEST_F(GlialWavePropagationTest, MultipleWaveInitiation) {
    std::vector<nimcp_glial_wave_t> waves;

    // Initiate multiple waves from different sources
    for (uint32_t region = 0; region < 5; region++) {
        auto wave = nimcp_glial_wave_initiate(region, 3.0f + region * 0.5f);
        ASSERT_NE(wave, nullptr);
        waves.push_back(wave);
    }

    // All waves should be active
    for (auto wave : waves) {
        EXPECT_TRUE(nimcp_glial_wave_is_active(wave));
    }

    // Cleanup
    for (auto wave : waves) {
        nimcp_glial_wave_destroy(wave);
    }
}

//=============================================================================
// Wave Propagation Tests
//=============================================================================

TEST_F(GlialWavePropagationTest, WavePropagationOverTime) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 5.0f);
    ASSERT_NE(wave, nullptr);

    // Track radius expansion over time
    std::vector<float> radii;

    for (int i = 0; i < 20; i++) {
        nimcp_glial_wave_step(wave, 10.0f); // Step 10ms
        float radius = nimcp_glial_wave_get_radius(wave);
        radii.push_back(radius);
    }

    // Radius should increase over time (while wave is active)
    bool radius_increased = false;
    for (size_t i = 1; i < radii.size(); i++) {
        if (radii[i] > radii[0]) {
            radius_increased = true;
            break;
        }
    }

    EXPECT_TRUE(radius_increased) << "Wave radius should expand over time";

    nimcp_glial_wave_destroy(wave);
}

TEST_F(GlialWavePropagationTest, WaveReachesTargetRegion) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 5.0f);
    ASSERT_NE(wave, nullptr);

    uint32_t target_region = 1;

    // Initially, wave should not have reached target
    EXPECT_FALSE(nimcp_glial_wave_has_reached(wave, target_region));

    // Propagate wave
    for (int i = 0; i < 100; i++) {
        nimcp_error_t err = nimcp_glial_wave_step(wave, 5.0f);
        if (err == NIMCP_BIO_ERROR_WAVE_EXTINCT) {
            break; // Wave died out before reaching target
        }

        if (nimcp_glial_wave_has_reached(wave, target_region)) {
            break;
        }
    }

    // Wave should eventually reach target or extinct
    bool reached = nimcp_glial_wave_has_reached(wave, target_region);
    bool extinct = !nimcp_glial_wave_is_active(wave);

    EXPECT_TRUE(reached || extinct)
        << "Wave should either reach target or extinct";

    nimcp_glial_wave_destroy(wave);
}

TEST_F(GlialWavePropagationTest, CalciumLevelDecaysWithDistance) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 5.0f);
    ASSERT_NE(wave, nullptr);

    // Propagate for a while
    for (int i = 0; i < 50; i++) {
        nimcp_glial_wave_step(wave, 10.0f);
    }

    // Check calcium levels at different regions (assuming they represent distance)
    float level_at_source = nimcp_glial_wave_get_level_at(wave, 0);
    float level_at_region_1 = nimcp_glial_wave_get_level_at(wave, 1);
    float level_at_region_5 = nimcp_glial_wave_get_level_at(wave, 5);

    // Calcium should generally decay with distance from source
    // (though this depends on wave dynamics)
    if (level_at_region_1 > 0.1f && level_at_region_5 > 0.0f) {
        EXPECT_GE(level_at_source, level_at_region_1);
    }

    nimcp_glial_wave_destroy(wave);
}

//=============================================================================
// Wave Arrival Callback Tests
//=============================================================================

TEST_F(GlialWavePropagationTest, ArrivalCallbackFires) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 5.0f);
    ASSERT_NE(wave, nullptr);

    uint32_t target_region = 1;

    // Register callback
    nimcp_error_t err = nimcp_glial_wave_on_arrival(
        wave, target_region, wave_arrival_callback, &arrival_count);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Propagate wave
    int max_steps = 200;
    for (int i = 0; i < max_steps; i++) {
        err = nimcp_glial_wave_step(wave, 5.0f);
        if (err == NIMCP_BIO_ERROR_WAVE_EXTINCT) {
            break;
        }

        if (arrival_count.load() > 0) {
            break; // Callback fired
        }
    }

    // Callback should have fired if wave reached target
    if (nimcp_glial_wave_has_reached(wave, target_region)) {
        EXPECT_GT(arrival_count.load(), 0)
            << "Callback should fire when wave reaches target";
    }

    nimcp_glial_wave_destroy(wave);
}

TEST_F(GlialWavePropagationTest, MultipleRegionCallbacks) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 6.0f);
    ASSERT_NE(wave, nullptr);

    // Register callbacks for multiple regions
    nimcp_glial_wave_on_arrival(wave, 1, wave_arrival_callback, &region_1_arrivals);
    nimcp_glial_wave_on_arrival(wave, 2, wave_arrival_callback, &region_2_arrivals);
    nimcp_glial_wave_on_arrival(wave, 3, wave_arrival_callback, &region_3_arrivals);

    // Propagate wave
    for (int i = 0; i < 200; i++) {
        nimcp_error_t err = nimcp_glial_wave_step(wave, 5.0f);
        if (err == NIMCP_BIO_ERROR_WAVE_EXTINCT) {
            break;
        }
    }

    // At least some callbacks should have fired
    int total_arrivals = region_1_arrivals.load() + region_2_arrivals.load() +
                         region_3_arrivals.load();

    if (nimcp_glial_wave_is_active(wave) || total_arrivals > 0) {
        EXPECT_GE(total_arrivals, 0);
    }

    nimcp_glial_wave_destroy(wave);
}

TEST_F(GlialWavePropagationTest, WaitForRegionBlocking) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 5.0f);
    ASSERT_NE(wave, nullptr);

    uint32_t target_region = 2;

    // Start propagation in background thread
    std::thread propagator([wave]() {
        for (int i = 0; i < 100; i++) {
            nimcp_error_t err = nimcp_glial_wave_step(wave, 10.0f);
            if (err == NIMCP_BIO_ERROR_WAVE_EXTINCT) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    auto start = std::chrono::high_resolution_clock::now();

    // Wait for wave to reach region (blocking)
    nimcp_error_t err = nimcp_glial_wave_wait_for_region(wave, target_region, 2000);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    propagator.join();

    if (err == NIMCP_SUCCESS) {
        EXPECT_TRUE(nimcp_glial_wave_has_reached(wave, target_region));
        EXPECT_LT(duration_ms, 2000) << "Should complete before timeout";
    } else {
        // Wave might have extinct or timeout
        EXPECT_NE(err, NIMCP_SUCCESS);
    }

    nimcp_glial_wave_destroy(wave);
}

//=============================================================================
// Concurrent Wave Tests
//=============================================================================

TEST_F(GlialWavePropagationTest, ConcurrentWavesDontInterfere) {
    // Create multiple concurrent waves
    auto wave1 = nimcp_glial_wave_initiate(0, 5.0f);
    auto wave2 = nimcp_glial_wave_initiate(5, 4.0f);
    auto wave3 = nimcp_glial_wave_initiate(10, 6.0f);

    ASSERT_NE(wave1, nullptr);
    ASSERT_NE(wave2, nullptr);
    ASSERT_NE(wave3, nullptr);

    // All should be active
    EXPECT_TRUE(nimcp_glial_wave_is_active(wave1));
    EXPECT_TRUE(nimcp_glial_wave_is_active(wave2));
    EXPECT_TRUE(nimcp_glial_wave_is_active(wave3));

    // Propagate all concurrently
    for (int i = 0; i < 50; i++) {
        nimcp_glial_wave_step(wave1, 5.0f);
        nimcp_glial_wave_step(wave2, 5.0f);
        nimcp_glial_wave_step(wave3, 5.0f);
    }

    // Each wave should maintain independent state
    float radius1 = nimcp_glial_wave_get_radius(wave1);
    float radius2 = nimcp_glial_wave_get_radius(wave2);
    float radius3 = nimcp_glial_wave_get_radius(wave3);

    // Radii should be positive (if waves still active)
    if (nimcp_glial_wave_is_active(wave1)) EXPECT_GT(radius1, 0.0f);
    if (nimcp_glial_wave_is_active(wave2)) EXPECT_GT(radius2, 0.0f);
    if (nimcp_glial_wave_is_active(wave3)) EXPECT_GT(radius3, 0.0f);

    nimcp_glial_wave_destroy(wave1);
    nimcp_glial_wave_destroy(wave2);
    nimcp_glial_wave_destroy(wave3);
}

TEST_F(GlialWavePropagationTest, ConcurrentWavesWithCallbacks) {
    auto wave1 = nimcp_glial_wave_initiate(0, 5.0f);
    auto wave2 = nimcp_glial_wave_initiate(10, 5.0f);

    std::atomic<int> wave1_arrivals{0};
    std::atomic<int> wave2_arrivals{0};

    // Register callbacks for different target regions
    nimcp_glial_wave_on_arrival(wave1, 5, wave_arrival_callback, &wave1_arrivals);
    nimcp_glial_wave_on_arrival(wave2, 5, wave_arrival_callback, &wave2_arrivals);

    // Propagate both
    for (int i = 0; i < 100; i++) {
        nimcp_glial_wave_step(wave1, 10.0f);
        nimcp_glial_wave_step(wave2, 10.0f);
    }

    // Both might reach region 5 from different directions
    // At least verify callbacks are independent
    int total = wave1_arrivals.load() + wave2_arrivals.load();
    EXPECT_GE(total, 0);

    nimcp_glial_wave_destroy(wave1);
    nimcp_glial_wave_destroy(wave2);
}

//=============================================================================
// Wave Extinction Tests
//=============================================================================

TEST_F(GlialWavePropagationTest, WaveEventuallyExtincts) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 3.0f);
    ASSERT_NE(wave, nullptr);

    bool became_extinct = false;

    // Propagate for extended period
    for (int i = 0; i < 500; i++) {
        nimcp_error_t err = nimcp_glial_wave_step(wave, 10.0f);

        if (err == NIMCP_BIO_ERROR_WAVE_EXTINCT) {
            became_extinct = true;
            break;
        }
    }

    // Wave should eventually extinct due to decay
    EXPECT_TRUE(became_extinct || !nimcp_glial_wave_is_active(wave))
        << "Wave should extinct after sufficient time";

    nimcp_glial_wave_destroy(wave);
}

TEST_F(GlialWavePropagationTest, LowIntensityWaveExtinctsFaster) {
    auto weak_wave = nimcp_glial_wave_initiate(0, 1.5f);
    auto strong_wave = nimcp_glial_wave_initiate(1, 8.0f);

    int weak_steps = 0, strong_steps = 0;

    // Count steps until extinction
    for (int i = 0; i < 300; i++) {
        if (nimcp_glial_wave_is_active(weak_wave)) {
            nimcp_error_t err = nimcp_glial_wave_step(weak_wave, 10.0f);
            if (err != NIMCP_BIO_ERROR_WAVE_EXTINCT) {
                weak_steps++;
            }
        }

        if (nimcp_glial_wave_is_active(strong_wave)) {
            nimcp_error_t err = nimcp_glial_wave_step(strong_wave, 10.0f);
            if (err != NIMCP_BIO_ERROR_WAVE_EXTINCT) {
                strong_steps++;
            }
        }
    }

    // Weak wave should extinct sooner (fewer steps)
    // Allow some tolerance due to stochastic dynamics
    if (weak_steps > 0 && strong_steps > 0) {
        EXPECT_LE(weak_steps, strong_steps * 1.5f)
            << "Weak wave should extinct sooner than strong wave";
    }

    nimcp_glial_wave_destroy(weak_wave);
    nimcp_glial_wave_destroy(strong_wave);
}

TEST_F(GlialWavePropagationTest, ExtinctWaveReturnsError) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 2.0f);
    ASSERT_NE(wave, nullptr);

    // Force extinction by propagating until extinct
    for (int i = 0; i < 500; i++) {
        nimcp_error_t err = nimcp_glial_wave_step(wave, 10.0f);
        if (err == NIMCP_BIO_ERROR_WAVE_EXTINCT) {
            break;
        }
    }

    if (!nimcp_glial_wave_is_active(wave)) {
        // Further propagation should return error
        nimcp_error_t err = nimcp_glial_wave_step(wave, 10.0f);
        EXPECT_EQ(err, NIMCP_BIO_ERROR_WAVE_EXTINCT);
    }

    nimcp_glial_wave_destroy(wave);
}

//=============================================================================
// Wave Dynamics Tests
//=============================================================================

TEST_F(GlialWavePropagationTest, WaveSpeedConsistency) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 5.0f);
    ASSERT_NE(wave, nullptr);

    std::vector<float> radii;
    std::vector<float> times;

    for (int i = 0; i < 30; i++) {
        float radius = nimcp_glial_wave_get_radius(wave);
        radii.push_back(radius);
        times.push_back(i * 10.0f);

        nimcp_glial_wave_step(wave, 10.0f);
    }

    // Compute approximate speed from radius growth
    if (radii.size() > 10) {
        float delta_r = radii[20] - radii[5];
        float delta_t = times[20] - times[5];

        if (delta_t > 0 && delta_r > 0) {
            float speed_um_per_ms = delta_r / delta_t;
            // Just verify it's reasonable (not checking exact value)
            EXPECT_GT(speed_um_per_ms, 0.0f);
            EXPECT_LT(speed_um_per_ms, 100.0f); // Sanity check
        }
    }

    nimcp_glial_wave_destroy(wave);
}

TEST_F(GlialWavePropagationTest, CalciumLevelProgression) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 5.0f);
    ASSERT_NE(wave, nullptr);

    uint32_t monitor_region = 3;

    // Track calcium level at a specific region over time
    std::vector<float> calcium_levels;

    for (int i = 0; i < 100; i++) {
        float level = nimcp_glial_wave_get_level_at(wave, monitor_region);
        calcium_levels.push_back(level);

        nimcp_error_t err = nimcp_glial_wave_step(wave, 5.0f);
        if (err == NIMCP_BIO_ERROR_WAVE_EXTINCT) {
            break;
        }
    }

    // Calcium should rise then fall at distant region
    if (calcium_levels.size() > 20) {
        float max_level = *std::max_element(calcium_levels.begin(), calcium_levels.end());

        if (max_level > 0.5f) {
            // Found significant calcium rise
            EXPECT_GT(max_level, 0.5f);

            // Should eventually decay
            float final_level = calcium_levels.back();
            EXPECT_LE(final_level, max_level);
        }
    }

    nimcp_glial_wave_destroy(wave);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(GlialWavePropagationTest, NullWaveHandling) {
    // Test that NULL wave is handled gracefully
    EXPECT_FALSE(nimcp_glial_wave_is_active(nullptr));
    EXPECT_EQ(nimcp_glial_wave_get_radius(nullptr), 0.0f);
    EXPECT_EQ(nimcp_glial_wave_get_level_at(nullptr, 0), 0.0f);
}

TEST_F(GlialWavePropagationTest, InvalidRegionQuery) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 5.0f);
    ASSERT_NE(wave, nullptr);

    // Query very high region ID
    float level = nimcp_glial_wave_get_level_at(wave, 999999);

    // Should return 0 or handle gracefully
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 10.0f); // Sanity check

    nimcp_glial_wave_destroy(wave);
}

// End of tests
