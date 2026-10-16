/**
 * @file portia_degradation_demo.c
 * @brief Demonstration of Portia graceful degradation system
 *
 * This example shows how to use the degradation profiles to handle
 * resource constraints by progressively reducing features.
 */

#include "portia/nimcp_portia_degradation.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include <stdio.h>
#include <unistd.h>

/**
 * Simulate resource usage increasing over time
 */
static float simulate_resource_spike(uint32_t time_step) {
    // Simulate gradual resource increase
    if (time_step < 10) {
        return 50.0f + (time_step * 2.0f);  // 50-68%
    } else if (time_step < 20) {
        return 70.0f + ((time_step - 10) * 1.5f);  // 70-85%
    } else if (time_step < 25) {
        return 85.0f + ((time_step - 20) * 2.0f);  // 85-95%
    } else if (time_step < 30) {
        return 95.0f + ((time_step - 25) * 1.0f);  // 95-100%
    } else if (time_step < 40) {
        // Recovery phase
        return 100.0f - ((time_step - 30) * 5.0f);  // 100-50%
    } else {
        return 40.0f;  // Stable
    }
}

/**
 * Print current degradation state
 */
static void print_degradation_state(degradation_state_t* state) {
    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;

    if (portia_degradation_get_state(state, &level, &active_features,
                                      &resource_usage) != NIMCP_OK) {
        printf("Failed to get degradation state\n");
        return;
    }

    const char* level_names[] = {
        "NONE (Normal)",
        "MINOR (Reduced)",
        "MODERATE (Limited)",
        "SEVERE (Critical)",
        "CRITICAL (Survival)"
    };

    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ Degradation State                                               │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    printf("│ Level:            %-45s │\n", level_names[level]);
    printf("│ Active Features:  %-45u │\n", active_features);
    printf("│ Resource Usage:   %-44.1f%% │\n", resource_usage);
    printf("└─────────────────────────────────────────────────────────────────┘\n");
}

/**
 * Print feature status
 */
static void print_feature_status(degradation_state_t* state) {
    printf("\nFeature Status:\n");
    printf("─────────────────────────────────────────────────────────────────\n");

    struct {
        uint32_t id;
        const char* name;
    } features[] = {
        {FEATURE_LOGGING_VERBOSE, "Verbose Logging"},
        {FEATURE_METRICS, "Metrics Collection"},
        {FEATURE_LEARNING, "Learning"},
        {FEATURE_PLASTICITY, "Plasticity"},
        {FEATURE_MEMORY_LONG, "Long-term Memory"},
        {FEATURE_EMOTIONS, "Emotions"},
        {FEATURE_PLANNING, "Planning"},
        {FEATURE_SENSORS_FULL, "Full Sensors"},
        {FEATURE_COMMUNICATION, "Communication"},
        {FEATURE_MEMORY_WORKING, "Working Memory (Core)"}
    };

    for (size_t i = 0; i < sizeof(features) / sizeof(features[0]); i++) {
        bool enabled;
        if (portia_degradation_is_feature_enabled(state, features[i].id,
                                                   &enabled) == NIMCP_OK) {
            printf("  [%c] %s\n", enabled ? '✓' : '✗', features[i].name);
        }
    }
    printf("─────────────────────────────────────────────────────────────────\n\n");
}

/**
 * Main demonstration
 */
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("╔═════════════════════════════════════════════════════════════════╗\n");
    printf("║        Portia Graceful Degradation System Demo                 ║\n");
    printf("╚═════════════════════════════════════════════════════════════════╝\n\n");

    // Initialize BBB
    printf("Initializing security system...\n");
    bbb_init();

    // Configure degradation system
    printf("Configuring degradation system...\n");
    portia_degradation_config_t config = {
        .level_thresholds = {
            [DEGRADATION_LEVEL_NONE] = 0.0f,
            [DEGRADATION_LEVEL_MINOR] = 70.0f,
            [DEGRADATION_LEVEL_MODERATE] = 80.0f,
            [DEGRADATION_LEVEL_SEVERE] = 90.0f,
            [DEGRADATION_LEVEL_CRITICAL] = 95.0f
        },
        .hysteresis_ms = 2000,  // 2 second hysteresis
        .enable_auto_degrade = true,
        .enable_auto_restore = true,
        .restore_threshold = 10.0f
    };

    // Initialize degradation system
    printf("Initializing degradation system...\n");
    degradation_state_t* state = portia_degradation_init(&config);
    if (!state) {
        printf("Failed to initialize degradation system\n");
        bbb_cleanup();
        return 1;
    }

    printf("\nInitialization complete!\n\n");
    printf("═════════════════════════════════════════════════════════════════\n\n");

    // Initial state
    printf("Initial State:\n");
    print_degradation_state(state);
    print_feature_status(state);

    // Simulate resource pressure over time
    printf("\n═════════════════════════════════════════════════════════════════\n");
    printf("Simulating Resource Pressure\n");
    printf("═════════════════════════════════════════════════════════════════\n\n");

    degradation_level_t last_level = DEGRADATION_LEVEL_NONE;

    for (uint32_t step = 0; step < 45; step++) {
        float usage = simulate_resource_spike(step);

        // Evaluate degradation
        nimcp_result_t result = portia_degradation_evaluate(state, usage, NULL);
        if (result != NIMCP_OK) {
            printf("Evaluation failed at step %u\n", step);
            continue;
        }

        // Get current level
        degradation_level_t current_level;
        if (portia_degradation_get_state(state, &current_level, NULL, NULL) != NIMCP_OK) {
            continue;
        }

        // Print when level changes
        if (current_level != last_level) {
            printf("\n[Step %2u] Level Change Detected!\n", step);
            printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
            print_degradation_state(state);
            print_feature_status(state);
            last_level = current_level;
        } else {
            // Just show progress
            printf("Step %2u: Usage %.1f%% - Level %d\n", step, usage, current_level);
        }

        // Small delay to simulate real-time operation
        usleep(500000);  // 0.5 seconds
    }

    // Final state
    printf("\n═════════════════════════════════════════════════════════════════\n");
    printf("Final State\n");
    printf("═════════════════════════════════════════════════════════════════\n\n");
    print_degradation_state(state);
    print_feature_status(state);

    // Test manual feature control
    printf("\n═════════════════════════════════════════════════════════════════\n");
    printf("Testing Manual Feature Control\n");
    printf("═════════════════════════════════════════════════════════════════\n\n");

    printf("Manually disabling learning feature...\n");
    portia_degradation_disable_feature(state, FEATURE_LEARNING, NULL);
    print_feature_status(state);

    printf("Manually enabling learning feature...\n");
    portia_degradation_enable_feature(state, FEATURE_LEARNING, NULL);
    print_feature_status(state);

    // Test forced degradation levels
    printf("\n═════════════════════════════════════════════════════════════════\n");
    printf("Testing Forced Degradation Levels\n");
    printf("═════════════════════════════════════════════════════════════════\n\n");

    printf("Forcing CRITICAL level...\n");
    portia_degradation_set_level(state, DEGRADATION_LEVEL_CRITICAL, NULL);
    print_degradation_state(state);
    print_feature_status(state);

    printf("Forcing NONE level (full restoration)...\n");
    portia_degradation_set_level(state, DEGRADATION_LEVEL_NONE, NULL);
    print_degradation_state(state);
    print_feature_status(state);

    // Get degradation chain
    printf("\n═════════════════════════════════════════════════════════════════\n");
    printf("Degradation Chain (Shutdown Order)\n");
    printf("═════════════════════════════════════════════════════════════════\n\n");

    degradation_feature_t chain[32];
    uint32_t chain_count;
    if (portia_degradation_get_chain(state, chain, 32, &chain_count) == NIMCP_OK) {
        printf("Features will be disabled in this order:\n\n");
        for (uint32_t i = 0; i < chain_count; i++) {
            printf("  %2u. %-25s (Level %d, Cost %.2f%%, %s)\n",
                   i + 1,
                   chain[i].name,
                   chain[i].disable_at,
                   chain[i].resource_cost * 100.0f,
                   chain[i].is_core ? "CORE" : "Optional");
        }
    }

    printf("\n═════════════════════════════════════════════════════════════════\n");
    printf("Demonstration Complete\n");
    printf("═════════════════════════════════════════════════════════════════\n\n");

    // Cleanup
    printf("Cleaning up...\n");
    portia_degradation_cleanup(state);
    bbb_cleanup();

    printf("Done!\n");
    return 0;
}
