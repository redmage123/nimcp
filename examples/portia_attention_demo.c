/**
 * @file portia_attention_demo.c
 * @brief Demonstration of Portia attention-based resource allocation
 *
 * This demo simulates a Portia spider's cognitive resource allocation
 * during different behaviors:
 * 1. Routine patrolling (low attention)
 * 2. Prey detection (attention shift)
 * 3. Complex hunting planning (high resource demand)
 * 4. Task completion (resource release)
 *
 * The demo shows how resources dynamically reallocate based on salience,
 * with smooth transitions and biological decay.
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "portia/nimcp_portia_attention.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Helper Functions
//=============================================================================

static void print_separator(void) {
    printf("\n");
    printf("================================================================\n");
}

static void print_allocations(portia_attention_state_t state, const char* phase) {
    printf("\n--- %s ---\n", phase);
    printf("%-15s %10s %10s\n", "Resource", "Salience", "Allocation");
    printf("%-15s %10s %10s\n", "---------------", "----------", "----------");

    for (int i = 0; i < ATTENTION_TARGET_COUNT; i++) {
        attention_target_t target = (attention_target_t)i;
        float salience = portia_attention_get_salience(state, target);
        float allocation = portia_attention_get_allocation(state, target);

        printf("%-15s %10.3f %10.3f\n",
               portia_attention_target_name(target),
               salience,
               allocation);
    }
}

static void wait_ms(int ms) {
    usleep(ms * 1000);
}

//=============================================================================
// Simulation Phases
//=============================================================================

static void simulate_patrolling(portia_attention_state_t state) {
    print_separator();
    printf("PHASE 1: Routine Patrolling\n");
    printf("The spider is patrolling its territory with low cognitive load.\n");
    printf("Resources are evenly distributed across basic functions.\n");

    // Low, balanced salience
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.3f);
    portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.2f);
    portia_attention_update_salience(state, ATTENTION_TARGET_PROCESSING, 0.2f);
    portia_attention_update_salience(state, ATTENTION_TARGET_SENSORS, 0.4f);
    portia_attention_update_salience(state, ATTENTION_TARGET_COMMUNICATION, 0.1f);

    portia_attention_reallocate(state, true);
    print_allocations(state, "Patrolling");

    wait_ms(1000);
}

static void simulate_prey_detection(portia_attention_state_t state) {
    print_separator();
    printf("PHASE 2: Prey Detection!\n");
    printf("A potential prey has been detected. Sensory resources spike.\n");
    printf("Neural processing begins ramping up for target analysis.\n");

    // Sensors spike, processing increases
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.6f);
    portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.3f);
    portia_attention_update_salience(state, ATTENTION_TARGET_PROCESSING, 0.5f);
    portia_attention_update_salience(state, ATTENTION_TARGET_SENSORS, 0.9f);  // High!
    portia_attention_update_salience(state, ATTENTION_TARGET_COMMUNICATION, 0.1f);

    portia_attention_reallocate(state, true);
    print_allocations(state, "Prey Detected");

    wait_ms(1000);
}

static void simulate_planning(portia_attention_state_t state) {
    print_separator();
    printf("PHASE 3: Complex Planning\n");
    printf("Portia is planning a complex hunting strategy - a detour approach.\n");
    printf("Neural and processing resources are maximally allocated.\n");
    printf("This is where Portia shows remarkable cognitive flexibility!\n");

    // Maximum cognitive resources
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 1.0f);      // Max!
    portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.8f);
    portia_attention_update_salience(state, ATTENTION_TARGET_PROCESSING, 0.9f);   // High!
    portia_attention_update_salience(state, ATTENTION_TARGET_SENSORS, 0.7f);
    portia_attention_update_salience(state, ATTENTION_TARGET_COMMUNICATION, 0.2f);

    portia_attention_reallocate(state, true);
    print_allocations(state, "Planning");

    printf("\n** Notice how neurons and processing get the largest allocations! **\n");

    wait_ms(1500);
}

static void simulate_execution(portia_attention_state_t state) {
    print_separator();
    printf("PHASE 4: Executing Plan\n");
    printf("Portia is executing the hunting plan with high sensory-motor coordination.\n");
    printf("Processing remains high, but shifts toward motor control.\n");

    // Execution phase
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.8f);
    portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.5f);
    portia_attention_update_salience(state, ATTENTION_TARGET_PROCESSING, 0.9f);  // Still high
    portia_attention_update_salience(state, ATTENTION_TARGET_SENSORS, 0.8f);
    portia_attention_update_salience(state, ATTENTION_TARGET_COMMUNICATION, 0.3f);

    portia_attention_reallocate(state, true);
    print_allocations(state, "Execution");

    wait_ms(1000);
}

static void simulate_completion(portia_attention_state_t state) {
    print_separator();
    printf("PHASE 5: Task Completion & Decay\n");
    printf("The hunt is complete! Resources are being released.\n");
    printf("Salience decays over time as the spider relaxes.\n");

    // Simulate time passing and decay
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t start_ms = ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;

    for (int i = 0; i < 5; i++) {
        wait_ms(300);

        uint64_t current_ms = start_ms + (i + 1) * 300;
        portia_attention_decay(state, current_ms);
        portia_attention_reallocate(state, false);

        printf("\nAfter %.1f seconds of decay:\n", (i + 1) * 0.3f);
        print_allocations(state, "Decaying");
    }

    printf("\n** Notice how all salience values gradually decrease! **\n");
}

static void demonstrate_resource_requests(portia_attention_state_t state) {
    print_separator();
    printf("DEMONSTRATION: Dynamic Resource Requests\n");
    printf("Showing how resources can be requested and released on demand.\n");

    // Reset to baseline
    for (int i = 0; i < ATTENTION_TARGET_COUNT; i++) {
        portia_attention_update_salience(state, (attention_target_t)i, 0.5f);
    }
    portia_attention_reallocate(state, true);

    print_allocations(state, "Baseline");

    // Request more neural resources
    printf("\n>> Requesting more neural resources (50%%)...\n");
    portia_attention_request(state, ATTENTION_TARGET_NEURONS, 0.5f);
    print_allocations(state, "After Request");

    wait_ms(500);

    // Release some memory resources
    printf("\n>> Releasing memory resources...\n");
    float current = portia_attention_get_allocation(state, ATTENTION_TARGET_MEMORY);
    portia_attention_release(state, ATTENTION_TARGET_MEMORY, current * 0.3f);
    print_allocations(state, "After Release");
}

//=============================================================================
// Main Demo
//=============================================================================

int main(int argc, char** argv) {
    printf("================================================================\n");
    printf("   PORTIA SPIDER ATTENTION-BASED RESOURCE ALLOCATION DEMO\n");
    printf("================================================================\n");
    printf("\n");
    printf("This demo simulates how Portia spiders dynamically allocate\n");
    printf("cognitive resources based on task demands - a remarkable\n");
    printf("example of attention-based resource management in nature!\n");

    // Initialize logging
    nimcp_log_config_t log_config = nimcp_log_default_config();
    log_config.level = LOG_LEVEL_INFO;
    log_config.destinations = NIMCP_LOG_DEST_CONSOLE;
    nimcp_log_init(&log_config);

    // Initialize memory tracking
    nimcp_memory_init();

    // Create attention system with custom configuration
    portia_attention_config_t config = {
        .reallocation_threshold = 0.05f,    // 5% change triggers reallocation
        .decay_rate_per_second = 0.2f,      // 20% decay per second
        .update_interval_ms = 100,           // Update every 100ms
        .enable_preemption = true,
        .preemption_threshold = 0.3f,        // 30% salience difference
        .hysteresis_factor = 0.15f,          // 15% hysteresis
        .smoothing_alpha = 0.4f              // 40% new, 60% old
    };

    printf("\nInitializing Portia attention system...\n");
    printf("  Budget: 100%% (normalized to 1.0)\n");
    printf("  Resources: %d (neurons, memory, processing, sensors, communication)\n",
           ATTENTION_TARGET_COUNT);
    printf("  Decay rate: %.1f%% per second\n", config.decay_rate_per_second * 100);
    printf("  Smoothing: %.1f%% new value, %.1f%% old value\n",
           config.smoothing_alpha * 100, (1.0f - config.smoothing_alpha) * 100);

    portia_attention_state_t state = portia_attention_init(
        &config,
        ATTENTION_TARGET_COUNT,
        1.0f  // 100% resource budget
    );

    if (!state) {
        fprintf(stderr, "ERROR: Failed to initialize attention system!\n");
        return 1;
    }

    // Run simulation phases
    simulate_patrolling(state);
    simulate_prey_detection(state);
    simulate_planning(state);
    simulate_execution(state);
    simulate_completion(state);
    demonstrate_resource_requests(state);

    // Show final statistics
    print_separator();
    printf("FINAL STATISTICS\n");
    portia_attention_stats_t stats;
    portia_attention_get_stats(state, &stats);

    printf("\nOperation counts:\n");
    printf("  Salience updates:  %lu\n", stats.salience_updates);
    printf("  Reallocations:     %lu\n", stats.reallocations);
    printf("  Resource requests: %lu\n", stats.requests);
    printf("  Resource releases: %lu\n", stats.releases);
    printf("\nResource metrics:\n");
    printf("  Average salience:  %.3f\n", stats.avg_salience);
    printf("  Total allocated:   %.3f (%.1f%% of budget)\n",
           stats.total_allocated, stats.total_allocated * 100);

    print_separator();
    printf("\nFinal state:\n");
    portia_attention_print_state(state);

    // Cleanup
    printf("\nCleaning up...\n");
    portia_attention_destroy(state);

    // Check for memory leaks
    nimcp_memory_check_leaks();
    nimcp_memory_cleanup();
    nimcp_log_shutdown();

    print_separator();
    printf("Demo complete!\n");
    printf("\nKEY TAKEAWAYS:\n");
    printf("  1. Resources dynamically reallocate based on task salience\n");
    printf("  2. Smooth transitions prevent jitter\n");
    printf("  3. Hysteresis prevents oscillation\n");
    printf("  4. Biological decay models attention fading\n");
    printf("  5. Fair allocation respects priorities and constraints\n");
    printf("================================================================\n");

    return 0;
}
