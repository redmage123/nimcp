/**
 * Quick compilation test for speech motor module
 */
#include "core/brain/regions/broca/nimcp_speech_motor.h"
#include "utils/memory/nimcp_memory.h"
#include <stdio.h>

int main(void) {
    printf("Testing speech motor module compilation...\n");

    // Initialize memory tracking
    nimcp_memory_init();

    // Create planner with defaults
    speech_motor_config_t config = speech_motor_default_config();
    printf("Default config: max_commands=%u, planning_window=%.1fms, coarticulation=%s\n",
           config.max_commands,
           config.planning_window_ms,
           config.enable_coarticulation ? "enabled" : "disabled");

    // Validate config
    if (!speech_motor_validate_config(&config)) {
        printf("ERROR: Default config validation failed!\n");
        return 1;
    }
    printf("Config validation: PASSED\n");

    // Create planner
    speech_motor_planner_t* planner = speech_motor_create(&config);
    if (!planner) {
        printf("ERROR: Failed to create planner!\n");
        return 1;
    }
    printf("Planner creation: PASSED\n");

    // Plan a simple phoneme sequence: "hello" = /h e l o/
    const uint8_t phonemes[] = {'h', 'e', 'l', 'o'};
    const uint32_t num_phonemes = sizeof(phonemes) / sizeof(phonemes[0]);

    if (!speech_motor_plan_sequence(planner, phonemes, num_phonemes)) {
        printf("ERROR: Failed to plan phoneme sequence!\n");
        speech_motor_destroy(planner);
        return 1;
    }
    printf("Phoneme sequence planning: PASSED\n");

    // Get planned commands
    motor_command_t commands[64];
    uint32_t count = 64;
    if (!speech_motor_get_commands(planner, commands, &count)) {
        printf("ERROR: Failed to get commands!\n");
        speech_motor_destroy(planner);
        return 1;
    }
    printf("Retrieved %u motor commands\n", count);

    // Display first few commands
    printf("\nFirst 5 commands:\n");
    for (uint32_t i = 0; i < 5 && i < count; i++) {
        printf("  [%u] %s: pos=%.2f, vel=%.2f, time=%.1fms (phoneme='%c')\n",
               i,
               speech_motor_articulator_name(commands[i].type),
               commands[i].position,
               commands[i].velocity,
               commands[i].timestamp,
               (char)commands[i].phoneme);
    }

    // Get statistics
    speech_motor_stats_t stats;
    if (speech_motor_get_stats(planner, &stats)) {
        printf("\nStatistics:\n");
        printf("  Phonemes planned: %lu\n", (unsigned long)stats.phonemes_planned);
        printf("  Commands generated: %lu\n", (unsigned long)stats.commands_generated);
        printf("  Queue size: %u\n", stats.queue_size);
    }

    // Test articulator queries
    float pos;
    if (speech_motor_get_articulator(planner, ARTICULATOR_LIPS, &pos)) {
        printf("\nCurrent LIPS position: %.2f\n", pos);
    }

    // Reset planner
    if (!speech_motor_reset(planner)) {
        printf("ERROR: Failed to reset planner!\n");
        speech_motor_destroy(planner);
        return 1;
    }
    printf("\nPlanner reset: PASSED\n");

    // Clean up
    speech_motor_destroy(planner);
    printf("\nPlanner destroyed successfully\n");

    // Check for memory leaks
    nimcp_memory_check_leaks();
    nimcp_memory_cleanup();

    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}
