/**
 * @file portia_demo.c
 * @brief Demonstration of Portia Spider Target Classification and Deception
 *
 * WHAT: Example usage of classification and stealth systems
 * WHY:  Show how Portia identifies prey and uses deception
 * HOW:  Create classifier, track targets, activate stealth modes
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "portia/nimcp_portia_classification.h"
#include "portia/nimcp_portia_deception.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "portia_demo"

/**
 * WHAT: Simulate target movement in a scene
 * WHY:  Demonstrate tracking and classification
 * HOW:  Update positions over time
 */
static void simulate_scene(portia_classifier_t classifier)
{
    printf("\n=== Simulating Hunter Scene ===\n\n");

    // Add a small, fast-moving target (prey)
    printf("1. Adding small, fast-moving target (potential prey)...\n");
    uint32_t prey_id = portia_classification_add_target(
        classifier, 0.0f, 0.0f, 0.0f, 0.3f);
    if (prey_id == 0) {
        fprintf(stderr, "Failed to add prey target\n");
        return;
    }
    printf("   - Registered target ID: %u\n", prey_id);

    // Add a large, fast-moving target (threat)
    printf("\n2. Adding large, fast-moving target (potential threat)...\n");
    uint32_t threat_id = portia_classification_add_target(
        classifier, 10.0f, 10.0f, 0.0f, 2.5f);
    if (threat_id == 0) {
        fprintf(stderr, "Failed to add threat target\n");
        return;
    }
    printf("   - Registered target ID: %u\n", threat_id);

    // Add a small, stationary target (neutral)
    printf("\n3. Adding small, stationary target (neutral)...\n");
    uint32_t neutral_id = portia_classification_add_target(
        classifier, -5.0f, -5.0f, 0.0f, 0.2f);
    if (neutral_id == 0) {
        fprintf(stderr, "Failed to add neutral target\n");
        return;
    }
    printf("   - Registered target ID: %u\n", neutral_id);

    // Add a large, stationary target (obstacle)
    printf("\n4. Adding large, stationary target (obstacle)...\n");
    uint32_t obstacle_id = portia_classification_add_target(
        classifier, 5.0f, -10.0f, 0.0f, 3.0f);
    if (obstacle_id == 0) {
        fprintf(stderr, "Failed to add obstacle target\n");
        return;
    }
    printf("   - Registered target ID: %u\n", obstacle_id);

    printf("\n=== Tracking Targets Over Time ===\n\n");

    // Simulate movement for 10 time steps
    for (int step = 1; step <= 10; step++) {
        printf("Time step %d:\n", step);

        // Update prey (moving quickly)
        float prey_x = (float)step * 1.2f;
        float prey_y = (float)step * 0.8f;
        portia_classification_update(classifier, prey_id, prey_x, prey_y, 0.0f);

        // Update threat (moving quickly in different direction)
        float threat_x = 10.0f + (float)step * 1.5f;
        float threat_y = 10.0f - (float)step * 1.0f;
        portia_classification_update(classifier, threat_id, threat_x, threat_y, 0.0f);

        // Update neutral (barely moving)
        portia_classification_update(classifier, neutral_id,
            -5.0f + 0.01f, -5.0f + 0.01f, 0.0f);

        // Update obstacle (not moving)
        portia_classification_update(classifier, obstacle_id,
            5.0f, -10.0f, 0.0f);

        // Small delay to simulate time
        usleep(50000);  // 50ms
    }

    printf("\n=== Classification Results ===\n\n");

    // Classify all targets
    const char* class_names[] = {
        "UNKNOWN", "FRIENDLY", "NEUTRAL", "THREAT", "PREY", "OBSTACLE"
    };

    uint32_t target_ids[] = {prey_id, threat_id, neutral_id, obstacle_id};
    const char* target_names[] = {"Prey", "Threat", "Neutral", "Obstacle"};

    for (int i = 0; i < 4; i++) {
        target_class_t classification;
        float confidence;

        int result = portia_classification_classify(
            classifier, target_ids[i], &classification, &confidence);

        if (result == NIMCP_SUCCESS) {
            printf("%s target (ID %u):\n", target_names[i], target_ids[i]);
            printf("   - Classification: %s\n", class_names[classification]);
            printf("   - Confidence: %.2f%%\n", confidence * 100.0f);

            // Get full target info
            target_info_t info;
            if (portia_classification_get_target(classifier, target_ids[i], &info) == NIMCP_SUCCESS) {
                printf("   - Position: (%.2f, %.2f, %.2f)\n", info.x, info.y, info.z);
                printf("   - Velocity: (%.2f, %.2f, %.2f)\n", info.vx, info.vy, info.vz);
                float speed = sqrtf(info.vx * info.vx + info.vy * info.vy + info.vz * info.vz);
                printf("   - Speed: %.2f units/s\n", speed);
                printf("   - Observations: %u\n", info.observation_count);
            }
            printf("\n");
        }
    }

    // Get threat list
    printf("=== Threat Assessment ===\n\n");
    uint32_t threats[10];
    uint32_t threat_count = portia_classification_get_threats(
        classifier, threats, 10);

    printf("Detected %u threat(s):\n", threat_count);
    for (uint32_t i = 0; i < threat_count; i++) {
        printf("   - Target ID: %u\n", threats[i]);
    }
}

/**
 * WHAT: Demonstrate stealth and deception capabilities
 * WHY:  Show how Portia uses mimicry to hunt
 * HOW:  Activate different stealth modes and mimicry profiles
 */
static void demonstrate_stealth(portia_deception_t deception)
{
    printf("\n\n=== Stealth and Deception Demonstration ===\n\n");

    // Start in normal mode
    printf("1. Normal operation (no stealth):\n");
    stealth_state_t state;
    portia_deception_get_state(deception, &state);
    printf("   - Mode: NONE\n");
    printf("   - Emission level: %.2f\n", state.emission_level);
    printf("   - Effectiveness: %.2f%%\n\n", state.effectiveness * 100.0f);

    // Activate passive stealth
    printf("2. Activating passive stealth (minimize emissions):\n");
    portia_deception_set_mode(deception, STEALTH_MODE_PASSIVE);
    portia_deception_emit(deception, 0.2f);  // Low emissions
    portia_deception_get_state(deception, &state);
    printf("   - Mode: PASSIVE\n");
    printf("   - Emission level: %.2f\n", state.emission_level);
    printf("   - Effectiveness: %.2f%%\n\n", state.effectiveness * 100.0f);

    // Activate active stealth with jamming
    printf("3. Activating active stealth with jamming:\n");
    portia_deception_set_mode(deception, STEALTH_MODE_ACTIVE);
    portia_deception_emit(deception, 0.1f);  // Very low emissions
    portia_deception_jam(deception, true);   // Enable jamming
    portia_deception_get_state(deception, &state);
    printf("   - Mode: ACTIVE\n");
    printf("   - Emission level: %.2f\n", state.emission_level);
    printf("   - Jamming: %s\n", state.jamming_active ? "ACTIVE" : "INACTIVE");
    printf("   - Effectiveness: %.2f%%\n\n", state.effectiveness * 100.0f);

    // Register mimicry profiles
    printf("4. Registering mimicry profiles:\n\n");

    // Profile 1: Prey spider
    mimicry_profile_t prey_profile = {0};
    snprintf(prey_profile.name, sizeof(prey_profile.name), "prey_spider");
    prey_profile.pattern_length = 4;
    prey_profile.signal_pattern[0] = 0.3f;
    prey_profile.signal_pattern[1] = 0.5f;
    prey_profile.signal_pattern[2] = 0.2f;
    prey_profile.signal_pattern[3] = 0.4f;
    prey_profile.effectiveness = 0.85f;

    uint32_t prey_profile_id = portia_deception_register_profile(
        deception, &prey_profile);
    printf("   - Registered '%s' (ID: %u, effectiveness: %.2f%%)\n",
           prey_profile.name, prey_profile_id, prey_profile.effectiveness * 100.0f);

    // Profile 2: Harmless insect
    mimicry_profile_t insect_profile = {0};
    snprintf(insect_profile.name, sizeof(insect_profile.name), "harmless_insect");
    insect_profile.pattern_length = 3;
    insect_profile.signal_pattern[0] = 0.1f;
    insect_profile.signal_pattern[1] = 0.2f;
    insect_profile.signal_pattern[2] = 0.15f;
    insect_profile.effectiveness = 0.75f;

    uint32_t insect_profile_id = portia_deception_register_profile(
        deception, &insect_profile);
    printf("   - Registered '%s' (ID: %u, effectiveness: %.2f%%)\n",
           insect_profile.name, insect_profile_id, insect_profile.effectiveness * 100.0f);

    // Profile 3: Mate courtship signal
    mimicry_profile_t courtship_profile = {0};
    snprintf(courtship_profile.name, sizeof(courtship_profile.name), "courtship_signal");
    courtship_profile.pattern_length = 5;
    courtship_profile.signal_pattern[0] = 0.6f;
    courtship_profile.signal_pattern[1] = 0.8f;
    courtship_profile.signal_pattern[2] = 0.7f;
    courtship_profile.signal_pattern[3] = 0.9f;
    courtship_profile.signal_pattern[4] = 0.6f;
    courtship_profile.effectiveness = 0.92f;

    uint32_t courtship_profile_id = portia_deception_register_profile(
        deception, &courtship_profile);
    printf("   - Registered '%s' (ID: %u, effectiveness: %.2f%%)\n\n",
           courtship_profile.name, courtship_profile_id, courtship_profile.effectiveness * 100.0f);

    // List all profiles
    printf("5. Available mimicry profiles:\n");
    mimicry_profile_t profiles[10];
    uint32_t profile_count = portia_deception_get_profiles(deception, profiles, 10);
    for (uint32_t i = 0; i < profile_count; i++) {
        printf("   - ID %u: %s (effectiveness: %.2f%%)\n",
               profiles[i].profile_id, profiles[i].name,
               profiles[i].effectiveness * 100.0f);
    }
    printf("\n");

    // Activate mimicry
    printf("6. Activating courtship signal mimicry (lure prey):\n");
    portia_deception_mimic(deception, courtship_profile_id);
    portia_deception_get_state(deception, &state);
    printf("   - Mode: MIMICRY\n");
    printf("   - Active profile: %u (%s)\n",
           state.mimicry_profile, courtship_profile.name);
    printf("   - Effectiveness: %.2f%%\n", state.effectiveness * 100.0f);
    printf("   - Status: Imitating mate signals to attract prey\n");
}

/**
 * WHAT: Main demonstration program
 * WHY:  Show complete Portia system capabilities
 * HOW:  Run classification and deception demos
 */
int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║     PORTIA SPIDER - TARGET CLASSIFICATION & DECEPTION         ║\n");
    printf("║                                                               ║\n");
    printf("║  Master hunter spider with advanced prey detection           ║\n");
    printf("║  and deceptive signaling capabilities                        ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    // Initialize logging
    nimcp_logging_init(NULL);
    nimcp_logging_set_level(LOG_LEVEL_INFO);

    // Create classification system
    printf("\n=== Initializing Classification System ===\n\n");
    portia_classification_config_t class_config = {
        .classification_threshold = 0.5f,
        .max_targets = 50,
        .retention_time_ms = 10000,
        .enable_prediction = true,
        .enable_bio_async = false
    };

    portia_classifier_t classifier = portia_classification_init(&class_config);
    if (!classifier) {
        fprintf(stderr, "Failed to initialize classification system\n");
        return 1;
    }
    printf("Classification system ready\n");
    printf("   - Max targets: %u\n", class_config.max_targets);
    printf("   - Classification threshold: %.2f\n", class_config.classification_threshold);
    printf("   - Retention time: %u ms\n", class_config.retention_time_ms);

    // Create deception system
    printf("\n=== Initializing Deception System ===\n\n");
    portia_deception_config_t decept_config = {
        .enable_stealth = true,
        .enable_mimicry = true,
        .enable_jamming = true,
        .default_emission_level = 1.0f,
        .profile_count = 10,
        .enable_bio_async = false
    };

    portia_deception_t deception = portia_deception_init(&decept_config);
    if (!deception) {
        fprintf(stderr, "Failed to initialize deception system\n");
        portia_classification_destroy(classifier);
        return 1;
    }
    printf("Deception system ready\n");
    printf("   - Stealth: %s\n", decept_config.enable_stealth ? "ENABLED" : "DISABLED");
    printf("   - Mimicry: %s\n", decept_config.enable_mimicry ? "ENABLED" : "DISABLED");
    printf("   - Jamming: %s\n", decept_config.enable_jamming ? "ENABLED" : "DISABLED");

    // Run demonstrations
    simulate_scene(classifier);
    demonstrate_stealth(deception);

    // Cleanup
    printf("\n\n=== Shutting Down ===\n\n");
    printf("Destroying deception system...\n");
    portia_deception_destroy(deception);

    printf("Destroying classification system...\n");
    portia_classification_destroy(classifier);

    printf("Cleanup complete\n\n");

    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                    DEMONSTRATION COMPLETE                     ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    nimcp_logging_shutdown();
    return 0;
}
