//=============================================================================
// test_platform_tier.c - Platform Tier System Test
//=============================================================================
/**
 * @file test_platform_tier.c
 * @brief Quick test program for platform tier detection
 *
 * WHAT: Verify platform tier system works correctly
 * WHY:  Ensure tier detection and config retrieval are functional
 * HOW:  Detect tier, print config, test validation
 */

#include "utils/platform/nimcp_platform_tier.h"
#include "utils/platform/nimcp_system_resources.h"
#include <stdio.h>
#include <string.h>

// Forward declaration from implementation (for testing)
void platform_tier_print_config(platform_tier_t tier);

int main(void)
{
    printf("========================================\n");
    printf("NIMCP Platform Tier System Test\n");
    printf("========================================\n\n");

    // Step 1: Query system resources
    printf("Step 1: Querying system resources...\n");
    system_resources_t resources;
    if (!system_resources_query(&resources)) {
        printf("ERROR: Failed to query system resources\n");
        return 1;
    }

    printf("System Resources:\n");
    printf("  Total RAM:      %llu MB\n", (unsigned long long)resources.total_ram_mb);
    printf("  Available RAM:  %llu MB\n", (unsigned long long)resources.available_ram_mb);
    printf("  CPU cores:      %u\n", resources.num_cpu_cores);
    printf("  CPU threads:    %u\n", resources.num_threads);
    printf("\n");

    // Step 2: Detect platform tier
    printf("Step 2: Detecting platform tier...\n");
    platform_tier_t detected_tier = platform_tier_detect();
    printf("Detected tier: %s\n\n", platform_tier_get_name(detected_tier));

    // Step 3: Get tier configuration
    printf("Step 3: Getting tier configuration...\n");
    platform_tier_config_t config = platform_tier_get_config(detected_tier);
    printf("Tier: %s\n", platform_tier_get_name(config.tier));
    printf("Max neurons: %u\n", config.max_neurons);
    printf("Max synapses/neuron: %u\n", config.max_synapses_per_neuron);
    printf("Memory budget: %u MB\n", config.memory_budget_mb);
    printf("\n");

    // Step 4: Test module availability
    printf("Step 4: Testing cognitive module availability...\n");

    struct {
        cognitive_module_flags_t flag;
        const char* name;
    } modules[] = {
        {COGNITIVE_MODULE_ATTENTION, "Attention"},
        {COGNITIVE_MODULE_WORKING_MEMORY, "Working Memory"},
        {COGNITIVE_MODULE_CURIOSITY, "Curiosity"},
        {COGNITIVE_MODULE_REASONING, "Reasoning"},
        {COGNITIVE_MODULE_EMOTIONS, "Emotions"},
        {COGNITIVE_MODULE_VISUAL_CORTEX, "Visual Cortex"},
        {COGNITIVE_MODULE_AUDIO_CORTEX, "Audio Cortex"},
        {COGNITIVE_MODULE_META_LEARNING, "Meta-Learning"},
    };

    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
        bool available = platform_tier_can_enable_module(detected_tier, modules[i].flag);
        printf("  %-20s: %s\n", modules[i].name, available ? "ENABLED" : "DISABLED");
    }
    printf("\n");

    // Step 5: Get recommended neuron count
    printf("Step 5: Getting recommended neuron count...\n");
    uint32_t recommended = platform_tier_recommend_neuron_count(detected_tier, &resources);
    printf("Recommended neuron count: %u\n", recommended);
    printf("\n");

    // Step 6: Test config validation
    printf("Step 6: Testing config validation...\n");

    // Test 1: Valid config (should pass)
    platform_tier_config_t valid_config = config;
    char error_msg[256];
    if (platform_tier_validate_config(detected_tier, &valid_config, error_msg, sizeof(error_msg))) {
        printf("  Valid config test: PASSED\n");
    } else {
        printf("  Valid config test: FAILED - %s\n", error_msg);
    }

    // Test 2: Invalid config - too many neurons (should fail)
    platform_tier_config_t invalid_config = config;
    invalid_config.max_neurons = config.max_neurons * 10;  // 10x over limit
    if (!platform_tier_validate_config(detected_tier, &invalid_config, error_msg, sizeof(error_msg))) {
        printf("  Invalid config test: PASSED (correctly rejected)\n");
        printf("    Error: %s\n", error_msg);
    } else {
        printf("  Invalid config test: FAILED (should have rejected)\n");
    }
    printf("\n");

    // Step 7: Print all tier configurations
    printf("Step 7: All tier configurations:\n\n");
    for (int tier = PLATFORM_TIER_FULL; tier < PLATFORM_TIER_COUNT; tier++) {
        platform_tier_print_config((platform_tier_t)tier);
    }

    printf("========================================\n");
    printf("Platform Tier System Test Complete\n");
    printf("========================================\n");

    return 0;
}
