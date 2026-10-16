/**
 * @file portia_sensor_fusion_demo.c
 * @brief Demonstration of Portia lightweight sensor fusion
 *
 * This demo shows how Portia spider-inspired sensor fusion can integrate
 * multiple sensory modalities with minimal computational resources.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "portia/nimcp_portia_sensor_fusion.h"
#include "async/nimcp_bio_ctx.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/logging/nimcp_logging.h"

#define DEMO_DURATION_SEC 5
#define FUSION_RATE_HZ 20

/**
 * Simulate visual sensor (high frequency, precise)
 */
static sensor_reading_t simulate_visual_sensor(float time_sec) {
    sensor_reading_t reading;
    reading.type = SENSOR_TYPE_VISUAL;

    // Simulate tracking a moving target
    reading.value = 10.0f + 5.0f * sinf(time_sec * 2.0f * M_PI / 3.0f);

    // Add small noise
    reading.value += ((rand() % 100) / 1000.0f - 0.05f);

    reading.confidence = 0.9f;
    reading.timestamp_ms = nimcp_platform_get_time_ms();
    reading.valid = true;

    return reading;
}

/**
 * Simulate vibration sensor (medium frequency, complementary)
 */
static sensor_reading_t simulate_vibration_sensor(float time_sec) {
    sensor_reading_t reading;
    reading.type = SENSOR_TYPE_VIBRATION;

    // Vibration detects similar motion with delay
    reading.value = 10.0f + 4.5f * sinf((time_sec - 0.1f) * 2.0f * M_PI / 3.0f);

    // More noise than visual
    reading.value += ((rand() % 100) / 500.0f - 0.1f);

    reading.confidence = 0.75f;
    reading.timestamp_ms = nimcp_platform_get_time_ms();
    reading.valid = true;

    return reading;
}

/**
 * Simulate IMU sensor (high frequency, velocity-based)
 */
static sensor_reading_t simulate_imu_sensor(float time_sec) {
    sensor_reading_t reading;
    reading.type = SENSOR_TYPE_IMU;

    // IMU measures rate of change
    reading.value = 10.0f + 5.0f * cosf(time_sec * 2.0f * M_PI / 3.0f);

    // Medium noise
    reading.value += ((rand() % 100) / 800.0f - 0.0625f);

    reading.confidence = 0.85f;
    reading.timestamp_ms = nimcp_platform_get_time_ms();
    reading.valid = true;

    return reading;
}

/**
 * Simulate chemical sensor (low frequency, slow response)
 */
static sensor_reading_t simulate_chemical_sensor(float time_sec) {
    sensor_reading_t reading;
    reading.type = SENSOR_TYPE_CHEMICAL;

    // Chemical sensors have slow response
    reading.value = 10.0f + 3.0f * sinf((time_sec - 0.5f) * 2.0f * M_PI / 3.0f);

    // Higher noise
    reading.value += ((rand() % 100) / 300.0f - 0.166f);

    reading.confidence = 0.6f;
    reading.timestamp_ms = nimcp_platform_get_time_ms();
    reading.valid = true;

    return reading;
}

/**
 * Print fusion state
 */
static void print_state(const fused_state_t* state) {
    printf("  Position: (%.3f, %.3f, %.3f)\n", state->x, state->y, state->z);
    printf("  Velocity: (%.3f, %.3f, %.3f)\n", state->vx, state->vy, state->vz);
    printf("  Heading:  %.3f rad (%.1f deg)\n",
           state->heading, state->heading * 180.0f / M_PI);
    printf("  Confidence: %.3f\n", state->confidence);
    printf("  Contributing sensors: 0x%08X (%d active)\n",
           state->contributing_sensors,
           __builtin_popcount(state->contributing_sensors));
}

/**
 * Print statistics
 */
static void print_stats(const portia_fusion_stats_t* stats) {
    printf("\nFusion Statistics:\n");
    printf("  Total updates:       %lu\n", (unsigned long)stats->total_updates);
    printf("  Successful fusions:  %lu\n", (unsigned long)stats->successful_fusions);
    printf("  Outliers rejected:   %lu\n", (unsigned long)stats->outliers_rejected);
    printf("  Sensor dropouts:     %lu\n", (unsigned long)stats->sensor_dropouts);
    printf("  Average confidence:  %.3f\n", stats->average_confidence);
    printf("  Active sensors:      %u\n", stats->active_sensor_count);
}

/**
 * Demo 1: Simple weighted average fusion
 */
static void demo_weighted_average(void) {
    printf("\n=== Demo 1: Weighted Average Fusion ===\n");

    // Create bio-async context
    nimcp_bio_ctx_t* bio_ctx = nimcp_bio_ctx_create(32, 1024);
    if (!bio_ctx) {
        printf("ERROR: Failed to create bio-async context\n");
        return;
    }

    // Configure fusion
    portia_fusion_config_t config = portia_fusion_default_config();
    config.enable_kalman = false;  // Use weighted average
    config.fusion_rate_hz = FUSION_RATE_HZ;
    config.outlier_threshold = 3.0f;
    config.min_sensors = 2;

    // Initialize fusion
    portia_fusion_ctx_t* fusion = portia_fusion_init(&config, bio_ctx);
    if (!fusion) {
        printf("ERROR: Failed to initialize fusion\n");
        nimcp_bio_ctx_destroy(bio_ctx);
        return;
    }

    printf("Initialized weighted average fusion at %u Hz\n", config.fusion_rate_hz);
    printf("Running for %d seconds...\n", DEMO_DURATION_SEC);

    uint64_t start_time = nimcp_platform_get_time_ms();
    int update_count = 0;

    // Main fusion loop
    while ((nimcp_platform_get_time_ms() - start_time) < (DEMO_DURATION_SEC * 1000)) {
        float time_sec = (nimcp_platform_get_time_ms() - start_time) / 1000.0f;

        // Update visual sensor at 50 Hz
        if (update_count % 2 == 0) {
            sensor_reading_t visual = simulate_visual_sensor(time_sec);
            portia_fusion_update_sensor(fusion, &visual);
        }

        // Update IMU at 100 Hz
        sensor_reading_t imu = simulate_imu_sensor(time_sec);
        portia_fusion_update_sensor(fusion, &imu);

        // Update vibration at 20 Hz
        if (update_count % 5 == 0) {
            sensor_reading_t vibration = simulate_vibration_sensor(time_sec);
            portia_fusion_update_sensor(fusion, &vibration);
        }

        // Update chemical at 5 Hz
        if (update_count % 20 == 0) {
            sensor_reading_t chemical = simulate_chemical_sensor(time_sec);
            portia_fusion_update_sensor(fusion, &chemical);
        }

        // Process fusion
        if (portia_fusion_process(fusion)) {
            // Print state periodically
            if (update_count % 20 == 0) {
                printf("\n[t=%.2fs]\n", time_sec);
                fused_state_t state;
                if (portia_fusion_get_state(fusion, &state)) {
                    print_state(&state);
                }
            }
        }

        update_count++;
        nimcp_platform_sleep_ms(10);  // 100 Hz loop
    }

    // Print final statistics
    portia_fusion_stats_t stats;
    if (portia_fusion_get_stats(fusion, &stats)) {
        print_stats(&stats);
    }

    // Cleanup
    portia_fusion_destroy(fusion);
    nimcp_bio_ctx_destroy(bio_ctx);
}

/**
 * Demo 2: Kalman filter fusion
 */
static void demo_kalman_filter(void) {
    printf("\n=== Demo 2: Extended Kalman Filter Fusion ===\n");

    nimcp_bio_ctx_t* bio_ctx = nimcp_bio_ctx_create(32, 1024);
    if (!bio_ctx) {
        printf("ERROR: Failed to create bio-async context\n");
        return;
    }

    portia_fusion_config_t config = portia_fusion_default_config();
    config.enable_kalman = true;  // Use Kalman filter
    config.process_noise = 0.05f;
    config.fusion_rate_hz = FUSION_RATE_HZ;

    portia_fusion_ctx_t* fusion = portia_fusion_init(&config, bio_ctx);
    if (!fusion) {
        printf("ERROR: Failed to initialize fusion\n");
        nimcp_bio_ctx_destroy(bio_ctx);
        return;
    }

    printf("Initialized Kalman filter fusion at %u Hz\n", config.fusion_rate_hz);
    printf("Running for %d seconds...\n", DEMO_DURATION_SEC);

    uint64_t start_time = nimcp_platform_get_time_ms();
    int update_count = 0;

    while ((nimcp_platform_get_time_ms() - start_time) < (DEMO_DURATION_SEC * 1000)) {
        float time_sec = (nimcp_platform_get_time_ms() - start_time) / 1000.0f;

        // Add all sensors
        sensor_reading_t visual = simulate_visual_sensor(time_sec);
        sensor_reading_t imu = simulate_imu_sensor(time_sec);
        sensor_reading_t vibration = simulate_vibration_sensor(time_sec);

        portia_fusion_update_sensor(fusion, &visual);
        portia_fusion_update_sensor(fusion, &imu);
        portia_fusion_update_sensor(fusion, &vibration);

        // Process fusion
        if (portia_fusion_process(fusion)) {
            if (update_count % 20 == 0) {
                printf("\n[t=%.2fs]\n", time_sec);
                fused_state_t state;
                if (portia_fusion_get_state(fusion, &state)) {
                    print_state(&state);
                }
            }
        }

        update_count++;
        nimcp_platform_sleep_ms(50);  // 20 Hz
    }

    portia_fusion_stats_t stats;
    if (portia_fusion_get_stats(fusion, &stats)) {
        print_stats(&stats);
    }

    portia_fusion_destroy(fusion);
    nimcp_bio_ctx_destroy(bio_ctx);
}

/**
 * Demo 3: Sensor failure and recovery
 */
static void demo_sensor_failure(void) {
    printf("\n=== Demo 3: Sensor Failure and Recovery ===\n");

    nimcp_bio_ctx_t* bio_ctx = nimcp_bio_ctx_create(32, 1024);
    if (!bio_ctx) {
        printf("ERROR: Failed to create bio-async context\n");
        return;
    }

    portia_fusion_config_t config = portia_fusion_default_config();
    config.enable_fallback = true;
    config.min_sensors = 2;

    portia_fusion_ctx_t* fusion = portia_fusion_init(&config, bio_ctx);
    if (!fusion) {
        printf("ERROR: Failed to initialize fusion\n");
        nimcp_bio_ctx_destroy(bio_ctx);
        return;
    }

    printf("Demonstrating sensor failure and recovery...\n");

    uint64_t start_time = nimcp_platform_get_time_ms();

    // Phase 1: Normal operation (0-2s)
    printf("\nPhase 1: Normal operation with all sensors\n");
    while ((nimcp_platform_get_time_ms() - start_time) < 2000) {
        float time_sec = (nimcp_platform_get_time_ms() - start_time) / 1000.0f;

        sensor_reading_t visual = simulate_visual_sensor(time_sec);
        sensor_reading_t imu = simulate_imu_sensor(time_sec);
        sensor_reading_t vibration = simulate_vibration_sensor(time_sec);

        portia_fusion_update_sensor(fusion, &visual);
        portia_fusion_update_sensor(fusion, &imu);
        portia_fusion_update_sensor(fusion, &vibration);
        portia_fusion_process(fusion);

        nimcp_platform_sleep_ms(50);
    }

    fused_state_t state;
    portia_fusion_get_state(fusion, &state);
    printf("Confidence with all sensors: %.3f\n", state.confidence);

    // Phase 2: Visual sensor failure (2-4s)
    printf("\nPhase 2: Visual sensor disabled (simulating failure)\n");
    portia_fusion_enable_sensor(fusion, SENSOR_TYPE_VISUAL, false);

    while ((nimcp_platform_get_time_ms() - start_time) < 4000) {
        float time_sec = (nimcp_platform_get_time_ms() - start_time) / 1000.0f;

        sensor_reading_t imu = simulate_imu_sensor(time_sec);
        sensor_reading_t vibration = simulate_vibration_sensor(time_sec);

        portia_fusion_update_sensor(fusion, &imu);
        portia_fusion_update_sensor(fusion, &vibration);
        portia_fusion_process(fusion);

        nimcp_platform_sleep_ms(50);
    }

    portia_fusion_get_state(fusion, &state);
    printf("Confidence without visual: %.3f\n", state.confidence);

    // Phase 3: Recovery (4-5s)
    printf("\nPhase 3: Visual sensor recovered\n");
    portia_fusion_enable_sensor(fusion, SENSOR_TYPE_VISUAL, true);

    while ((nimcp_platform_get_time_ms() - start_time) < 5000) {
        float time_sec = (nimcp_platform_get_time_ms() - start_time) / 1000.0f;

        sensor_reading_t visual = simulate_visual_sensor(time_sec);
        sensor_reading_t imu = simulate_imu_sensor(time_sec);
        sensor_reading_t vibration = simulate_vibration_sensor(time_sec);

        portia_fusion_update_sensor(fusion, &visual);
        portia_fusion_update_sensor(fusion, &imu);
        portia_fusion_update_sensor(fusion, &vibration);
        portia_fusion_process(fusion);

        nimcp_platform_sleep_ms(50);
    }

    portia_fusion_get_state(fusion, &state);
    printf("Confidence after recovery: %.3f\n", state.confidence);

    portia_fusion_stats_t stats;
    if (portia_fusion_get_stats(fusion, &stats)) {
        print_stats(&stats);
    }

    portia_fusion_destroy(fusion);
    nimcp_bio_ctx_destroy(bio_ctx);
}

/**
 * Main demonstration
 */
int main(int argc, char** argv) {
    printf("========================================\n");
    printf("Portia Spider Sensor Fusion Demo\n");
    printf("========================================\n");
    printf("\nThis demo showcases lightweight multi-modal sensor fusion\n");
    printf("inspired by Portia spiders' ability to integrate visual,\n");
    printf("vibrational, and chemical cues with minimal neural resources.\n");

    // Seed random number generator
    srand(time(NULL));

    // Run demonstrations
    demo_weighted_average();
    printf("\n");
    nimcp_platform_sleep_ms(1000);

    demo_kalman_filter();
    printf("\n");
    nimcp_platform_sleep_ms(1000);

    demo_sensor_failure();

    printf("\n========================================\n");
    printf("Demo Complete!\n");
    printf("========================================\n");

    return 0;
}
