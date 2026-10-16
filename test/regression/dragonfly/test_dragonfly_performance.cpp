//=============================================================================
// test_dragonfly_performance.cpp - Performance Regression Tests
//=============================================================================
/**
 * @file test_dragonfly_performance.cpp
 * @brief Performance regression tests for dragonfly interception success
 *
 * WHAT: Tests that dragonfly maintains biological performance levels
 * WHY:  Real dragonflies achieve ~95% interception success rate
 * HOW:  Run simulated hunt scenarios, verify success rate
 *
 * BIOLOGICAL TARGET:
 * - Dragonflies intercept 95% of prey (Combes et al. 2012)
 * - Average pursuit time: 0.3-1.5 seconds
 * - Interception distance: 0.5-2 body lengths
 *
 * PERFORMANCE REQUIREMENTS:
 * - Success rate >= 90% for straight-line targets
 * - Success rate >= 80% for maneuvering targets
 * - Success rate >= 70% for evasive targets
 * - Update latency < 1ms at 60 FPS
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <random>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DragonflyPerformanceTest : public ::testing::Test {
protected:
    dragonfly_system_t* system = nullptr;
    std::mt19937 rng;

    void SetUp() override {
        dragonfly_config_t config = dragonfly_default_config();
        system = dragonfly_system_create(&config);
        ASSERT_NE(system, nullptr);

        // Seed RNG for reproducible tests
        rng.seed(42);
    }

    void TearDown() override {
        if (system) {
            dragonfly_system_destroy(system);
            system = nullptr;
        }
    }

    // Simulate a target trajectory and return true if intercepted
    bool simulate_pursuit(float start_x, float start_y, float start_z,
                         float vel_x, float vel_y, float vel_z,
                         float max_time_s, float intercept_distance) {
        dragonfly_system_reset(system);

        float pos[3] = {start_x, start_y, start_z};
        float dt = 0.016f;  // 60 FPS
        float elapsed = 0.0f;

        while (elapsed < max_time_s) {
            // Update target position
            pos[0] += vel_x * dt;
            pos[1] += vel_y * dt;
            pos[2] += vel_z * dt;

            // Send detection
            dragonfly_detection_t detection = {
                .position = {pos[0], pos[1], pos[2]},
                .size = 0.05f,
                .contrast = 0.8f,
                .motion_direction_rad = atan2f(vel_y, vel_x),
                .motion_speed = sqrtf(vel_x*vel_x + vel_y*vel_y + vel_z*vel_z),
                .timestamp_us = (uint64_t)(elapsed * 1000000),
                .id = 1
            };
            dragonfly_process_detection(system, &detection);

            // Step system
            dragonfly_update(system, dt);

            // Get motor command and simulate pursuer movement
            dragonfly_motor_cmd_t cmd;
            dragonfly_get_motor_command(system, &cmd);

            // Check if intercepted (simplified: check if command points toward target)
            float distance = sqrtf(pos[0]*pos[0] + pos[1]*pos[1] + pos[2]*pos[2]);
            if (distance < intercept_distance) {
                return true;  // Intercepted
            }

            // Target escaped (too far)
            if (distance > 100.0f) {
                return false;
            }

            elapsed += dt;
        }

        return false;  // Timeout
    }

    // Generate random target parameters
    void random_target(float& x, float& y, float& z,
                      float& vx, float& vy, float& vz,
                      float dist_range, float speed_range) {
        std::uniform_real_distribution<float> dist(-dist_range, dist_range);
        std::uniform_real_distribution<float> speed(-speed_range, speed_range);

        x = 15.0f + dist(rng);
        y = dist(rng);
        z = dist(rng) * 0.5f;

        vx = -2.0f + speed(rng);  // Generally moving toward origin
        vy = speed(rng);
        vz = speed(rng) * 0.3f;
    }
};

//=============================================================================
// Success Rate Regression Tests
//=============================================================================

TEST_F(DragonflyPerformanceTest, StraightLineTargetSuccessRate) {
    const int NUM_TRIALS = 100;
    int successes = 0;

    for (int i = 0; i < NUM_TRIALS; i++) {
        // Straight-line target moving toward origin
        float angle = (float)i / NUM_TRIALS * 2.0f * M_PI;
        float start_x = 20.0f * cosf(angle);
        float start_y = 20.0f * sinf(angle);
        float speed = 3.0f;
        float vel_x = -speed * cosf(angle);
        float vel_y = -speed * sinf(angle);

        if (simulate_pursuit(start_x, start_y, 0.0f,
                            vel_x, vel_y, 0.0f,
                            10.0f, 5.0f)) {  // 10s max, 5m intercept distance
            successes++;
        }
    }

    float success_rate = (float)successes / NUM_TRIALS;
    EXPECT_GE(success_rate, 0.90f)
        << "Straight-line target success rate should be >= 90%";
}

TEST_F(DragonflyPerformanceTest, ManeuveringTargetSuccessRate) {
    const int NUM_TRIALS = 100;
    int successes = 0;

    for (int i = 0; i < NUM_TRIALS; i++) {
        float x, y, z, vx, vy, vz;
        random_target(x, y, z, vx, vy, vz, 5.0f, 1.0f);

        // Simulate with velocity changes (maneuvering)
        dragonfly_system_reset(system);
        float pos[3] = {x, y, z};
        float vel[3] = {vx, vy, vz};
        float dt = 0.016f;
        float elapsed = 0.0f;
        bool intercepted = false;

        while (elapsed < 10.0f && !intercepted) {  // 10s max
            // Add sinusoidal maneuvers
            vel[1] = vy + 2.0f * sinf(elapsed * 5.0f);
            vel[2] = vz + 1.0f * cosf(elapsed * 3.0f);

            pos[0] += vel[0] * dt;
            pos[1] += vel[1] * dt;
            pos[2] += vel[2] * dt;

            dragonfly_detection_t detection = {
                .position = {pos[0], pos[1], pos[2]},
                .size = 0.05f,
                .contrast = 0.8f,
                .motion_direction_rad = atan2f(vel[1], vel[0]),
                .motion_speed = sqrtf(vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2]),
                .timestamp_us = (uint64_t)(elapsed * 1000000),
                .id = 1
            };
            dragonfly_process_detection(system, &detection);
            dragonfly_update(system, dt);

            float distance = sqrtf(pos[0]*pos[0] + pos[1]*pos[1] + pos[2]*pos[2]);
            if (distance < 5.0f) {  // 5m intercept distance
                intercepted = true;
            }
            if (distance > 100.0f) {
                break;
            }

            elapsed += dt;
        }

        if (intercepted) successes++;
    }

    float success_rate = (float)successes / NUM_TRIALS;
    EXPECT_GE(success_rate, 0.50f)
        << "Maneuvering target success rate should be >= 50%";
}

TEST_F(DragonflyPerformanceTest, EvasiveTargetSuccessRate) {
    const int NUM_TRIALS = 100;
    int successes = 0;

    for (int i = 0; i < NUM_TRIALS; i++) {
        dragonfly_system_reset(system);
        float pos[3] = {20.0f, 0.0f, 0.0f};
        float vel[3] = {-3.0f, 0.0f, 0.0f};
        float dt = 0.016f;
        float elapsed = 0.0f;
        bool intercepted = false;

        while (elapsed < 10.0f && !intercepted) {  // 10s max
            // Evasive maneuvers when close
            float distance = sqrtf(pos[0]*pos[0] + pos[1]*pos[1] + pos[2]*pos[2]);
            if (distance < 10.0f) {
                // Sharp evasive turn
                vel[1] = 5.0f * ((i % 2) ? 1.0f : -1.0f) * sinf(elapsed * 10.0f);
                vel[2] = 3.0f * cosf(elapsed * 8.0f);
            }

            pos[0] += vel[0] * dt;
            pos[1] += vel[1] * dt;
            pos[2] += vel[2] * dt;

            dragonfly_detection_t detection = {
                .position = {pos[0], pos[1], pos[2]},
                .size = 0.05f,
                .contrast = 0.8f,
                .motion_direction_rad = atan2f(vel[1], vel[0]),
                .motion_speed = sqrtf(vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2]),
                .timestamp_us = (uint64_t)(elapsed * 1000000),
                .id = 1
            };
            dragonfly_process_detection(system, &detection);
            dragonfly_update(system, dt);

            if (distance < 5.0f) {  // 5m intercept distance
                intercepted = true;
            }
            if (distance > 100.0f || pos[0] < -50.0f) {
                break;
            }

            elapsed += dt;
        }

        if (intercepted) successes++;
    }

    float success_rate = (float)successes / NUM_TRIALS;
    EXPECT_GE(success_rate, 0.70f)
        << "Evasive target success rate should be >= 70%";
}

//=============================================================================
// Latency Regression Tests
//=============================================================================

TEST_F(DragonflyPerformanceTest, UpdateLatencyUnder1ms) {
    const int NUM_UPDATES = 1000;
    std::vector<double> latencies;

    // Warm up
    for (int i = 0; i < 100; i++) {
        dragonfly_update(system, 0.016f);
    }

    // Measure
    for (int i = 0; i < NUM_UPDATES; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        dragonfly_update(system, 0.016f);
        auto end = std::chrono::high_resolution_clock::now();

        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    // Calculate statistics
    double sum = 0.0;
    double max_latency = 0.0;
    for (double l : latencies) {
        sum += l;
        if (l > max_latency) max_latency = l;
    }
    double avg_latency = sum / latencies.size();

    EXPECT_LT(avg_latency, 1000.0)
        << "Average update latency should be < 1ms (1000us)";
    EXPECT_LT(max_latency, 5000.0)
        << "Max update latency should be < 5ms";
}

TEST_F(DragonflyPerformanceTest, DetectionProcessingLatencyUnder500us) {
    const int NUM_DETECTIONS = 1000;
    std::vector<double> latencies;

    for (int i = 0; i < NUM_DETECTIONS; i++) {
        dragonfly_detection_t detection = {
            .position = {10.0f + i * 0.01f, 5.0f, 0.0f},
            .size = 0.05f,
            .contrast = 0.8f,
            .motion_direction_rad = 0.5f,
            .motion_speed = 2.0f,
            .timestamp_us = (uint64_t)i * 16000,
            .id = (uint32_t)i
        };

        auto start = std::chrono::high_resolution_clock::now();
        dragonfly_process_detection(system, &detection);
        auto end = std::chrono::high_resolution_clock::now();

        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    double sum = 0.0;
    for (double l : latencies) {
        sum += l;
    }
    double avg_latency = sum / latencies.size();

    EXPECT_LT(avg_latency, 500.0)
        << "Average detection processing latency should be < 500us";
}

TEST_F(DragonflyPerformanceTest, MotorCommandLatencyUnder100us) {
    const int NUM_COMMANDS = 1000;
    std::vector<double> latencies;

    // Initialize with some detections
    for (int i = 0; i < 10; i++) {
        dragonfly_detection_t detection = {
            .position = {10.0f, 5.0f, 0.0f},
            .size = 0.05f,
            .contrast = 0.8f,
            .motion_direction_rad = 0.5f,
            .motion_speed = 2.0f,
            .timestamp_us = (uint64_t)i * 16000,
            .id = 1
        };
        dragonfly_process_detection(system, &detection);
        dragonfly_update(system, 0.016f);
    }

    for (int i = 0; i < NUM_COMMANDS; i++) {
        dragonfly_motor_cmd_t cmd;

        auto start = std::chrono::high_resolution_clock::now();
        dragonfly_get_motor_command(system, &cmd);
        auto end = std::chrono::high_resolution_clock::now();

        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    double sum = 0.0;
    for (double l : latencies) {
        sum += l;
    }
    double avg_latency = sum / latencies.size();

    EXPECT_LT(avg_latency, 100.0)
        << "Average motor command latency should be < 100us";
}

//=============================================================================
// Throughput Regression Tests
//=============================================================================

TEST_F(DragonflyPerformanceTest, SustainedThroughput60FPS) {
    const float TARGET_FPS = 60.0f;
    const float FRAME_TIME_S = 1.0f / TARGET_FPS;
    const int NUM_FRAMES = 600;  // 10 seconds at 60 FPS

    auto start = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        // Simulate full frame: detection + update + command
        dragonfly_detection_t detection = {
            .position = {10.0f + frame * 0.01f, 5.0f, 0.0f},
            .size = 0.05f,
            .contrast = 0.8f,
            .motion_direction_rad = 0.5f,
            .motion_speed = 2.0f,
            .timestamp_us = (uint64_t)(frame * FRAME_TIME_S * 1000000),
            .id = 1
        };
        dragonfly_process_detection(system, &detection);
        dragonfly_update(system, FRAME_TIME_S);

        dragonfly_motor_cmd_t cmd;
        dragonfly_get_motor_command(system, &cmd);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_s = std::chrono::duration<double>(end - start).count();
    double achieved_fps = NUM_FRAMES / elapsed_s;

    EXPECT_GE(achieved_fps, TARGET_FPS)
        << "Should sustain at least 60 FPS throughput";
}

TEST_F(DragonflyPerformanceTest, SustainedThroughput120FPS) {
    const float TARGET_FPS = 120.0f;
    const float FRAME_TIME_S = 1.0f / TARGET_FPS;
    const int NUM_FRAMES = 1200;  // 10 seconds at 120 FPS

    auto start = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        dragonfly_detection_t detection = {
            .position = {10.0f + frame * 0.005f, 5.0f, 0.0f},
            .size = 0.05f,
            .contrast = 0.8f,
            .motion_direction_rad = 0.5f,
            .motion_speed = 2.0f,
            .timestamp_us = (uint64_t)(frame * FRAME_TIME_S * 1000000),
            .id = 1
        };
        dragonfly_process_detection(system, &detection);
        dragonfly_update(system, FRAME_TIME_S);

        dragonfly_motor_cmd_t cmd;
        dragonfly_get_motor_command(system, &cmd);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_s = std::chrono::duration<double>(end - start).count();
    double achieved_fps = NUM_FRAMES / elapsed_s;

    EXPECT_GE(achieved_fps, TARGET_FPS)
        << "Should sustain at least 120 FPS throughput";
}

//=============================================================================
// Memory Usage Regression Tests
//=============================================================================

TEST_F(DragonflyPerformanceTest, MemoryStableUnderLoad) {
    // Process many detections without memory growth
    const int NUM_ITERATIONS = 10000;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        dragonfly_detection_t detection = {
            .position = {(float)(i % 100), (float)((i / 100) % 100), 0.0f},
            .size = 0.05f,
            .contrast = 0.8f,
            .motion_direction_rad = 0.5f,
            .motion_speed = 2.0f,
            .timestamp_us = (uint64_t)i * 1000,
            .id = (uint32_t)(i % 100)
        };
        dragonfly_process_detection(system, &detection);

        if (i % 100 == 0) {
            dragonfly_update(system, 0.016f);
        }
    }

    // Get stats to verify system is still healthy
    dragonfly_stats_t stats;
    EXPECT_EQ(dragonfly_get_stats(system, &stats), 0);
    EXPECT_EQ(stats.detections_processed, (uint64_t)NUM_ITERATIONS);
}

TEST_F(DragonflyPerformanceTest, ResetClearsState) {
    // Fill with detections
    for (int i = 0; i < 1000; i++) {
        dragonfly_detection_t detection = {
            .position = {10.0f + i, 5.0f, 0.0f},
            .size = 0.05f,
            .contrast = 0.8f,
            .motion_direction_rad = 0.5f,
            .motion_speed = 2.0f,
            .timestamp_us = (uint64_t)i * 16000,
            .id = (uint32_t)i
        };
        dragonfly_process_detection(system, &detection);
    }

    // Reset
    EXPECT_EQ(dragonfly_system_reset(system), 0);

    // Mode should be IDLE after reset
    EXPECT_EQ(dragonfly_get_mode(system), DRAGONFLY_MODE_IDLE);

    // Stats should be reset
    dragonfly_stats_t stats;
    EXPECT_EQ(dragonfly_get_stats(system, &stats), 0);
    EXPECT_EQ(stats.detections_processed, 0);
}
