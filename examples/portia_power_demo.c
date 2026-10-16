/**
 * @file portia_power_demo.c
 * @brief Demonstration of Portia Power-Aware Tier System
 *
 * WHAT: Show power monitoring and automatic tier adaptation
 * WHY:  Demonstrate battery-aware resource management
 * HOW:  Monitor power, simulate battery drain, show profile changes
 *
 * USAGE:
 *   ./portia_power_demo
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "portia/nimcp_portia_power.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_tier.h"
#include "utils/platform/nimcp_platform_time.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEMO_MODULE "POWER_DEMO"

/**
 * @brief Print power status in formatted way
 */
static void print_power_status(const power_status_t* status) {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    POWER STATUS                                ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Source:       %-45s ║\n", portia_power_get_source_name(status->source));
    printf("║ Battery:      %5.1f%%                                         ║\n", status->battery_level_pct);
    printf("║ Discharge:    %7.1f mW                                     ║\n", status->discharge_rate_mw);
    printf("║ Runtime:      %7.0f seconds                                ║\n", status->estimated_runtime_s);
    printf("║ Temperature:  %5.1f°C                                        ║\n", status->temperature_c);
    printf("║ Charging:     %-45s ║\n", status->charging ? "Yes" : "No");
    printf("║ Health:       %-45s ║\n", status->health_good ? "Good" : "Degraded");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
}

/**
 * @brief Print tier configuration
 */
static void print_tier_config(const power_tier_config_t* config) {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║             POWER TIER CONFIGURATION                          ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Profile:          %-41s ║\n", portia_power_get_profile_name(config->profile));
    printf("║ Max Neurons:      %-41u ║\n", config->max_neurons);
    printf("║ Max Synapses:     %-41u ║\n", config->max_synapses);
    printf("║ Processing Rate:  %5.1f Hz                                   ║\n", config->processing_rate_hz);
    printf("║ Sampling Rate:    %5.1f%%                                     ║\n", config->sampling_rate * 100.0f);
    printf("║ Batch Size:       %-41u ║\n", config->batch_size);
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Learning:         %-41s ║\n", config->enable_learning ? "Enabled" : "Disabled");
    printf("║ Persistence:      %-41s ║\n", config->enable_persistence ? "Enabled" : "Disabled");
    printf("║ Bio-Async:        %-41s ║\n", config->enable_bio_async ? "Enabled" : "Disabled");
    printf("║ GPU:              %-41s ║\n", config->enable_gpu ? "Enabled" : "Disabled");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Wake Interval:    %5.3f s                                    ║\n", config->wake_interval_s);
    printf("║ Duty Cycle:       %5.1f%%                                     ║\n", config->active_duty_cycle * 100.0f);
    printf("║ Memory Budget:    %-37u MB ║\n", config->memory_budget_mb);
    printf("║ Compute Budget:   %-37u GOPS ║\n", config->compute_budget_gops);
    printf("╚════════════════════════════════════════════════════════════════╝\n");
}

/**
 * @brief Print cognitive modules
 */
static void print_cognitive_modules(uint32_t modules) {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                ENABLED COGNITIVE MODULES                      ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");

    const char* module_names[] = {
        "Attention", "Working Memory", "Salience", "Emotions", "Emotional Tagging",
        "Semantic Memory", "Episodic Memory", "Consolidation", "Executive",
        "Reasoning", "Curiosity", "Meta-Learning", "Introspection", "Self-Awareness",
        "Theory of Mind", "Mirror Neurons", "Empathy", "Global Workspace",
        "Predictive Coding", "Ethics", "Visual Cortex", "Audio Cortex"
    };

    uint32_t module_flags[] = {
        COGNITIVE_MODULE_ATTENTION, COGNITIVE_MODULE_WORKING_MEMORY,
        COGNITIVE_MODULE_SALIENCE, COGNITIVE_MODULE_EMOTIONS,
        COGNITIVE_MODULE_EMOTIONAL_TAG, COGNITIVE_MODULE_SEMANTIC_MEMORY,
        COGNITIVE_MODULE_EPISODIC_MEMORY, COGNITIVE_MODULE_CONSOLIDATION,
        COGNITIVE_MODULE_EXECUTIVE, COGNITIVE_MODULE_REASONING,
        COGNITIVE_MODULE_CURIOSITY, COGNITIVE_MODULE_META_LEARNING,
        COGNITIVE_MODULE_INTROSPECTION, COGNITIVE_MODULE_SELF_AWARENESS,
        COGNITIVE_MODULE_THEORY_OF_MIND, COGNITIVE_MODULE_MIRROR_NEURONS,
        COGNITIVE_MODULE_EMPATHY, COGNITIVE_MODULE_GLOBAL_WORKSPACE,
        COGNITIVE_MODULE_PREDICTIVE, COGNITIVE_MODULE_ETHICS,
        COGNITIVE_MODULE_VISUAL_CORTEX, COGNITIVE_MODULE_AUDIO_CORTEX
    };

    int count = 0;
    for (size_t i = 0; i < sizeof(module_flags) / sizeof(module_flags[0]); i++) {
        if (modules & module_flags[i]) {
            printf("║ ✓ %-60s ║\n", module_names[i]);
            count++;
        }
    }

    if (count == 0) {
        printf("║ (None - reactive mode only)                                  ║\n");
    }

    printf("╚════════════════════════════════════════════════════════════════╝\n");
}

/**
 * @brief Print statistics
 */
static void print_stats(const portia_power_stats_t* stats) {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║              POWER MONITORING STATISTICS                      ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Samples Taken:       %-38llu ║\n", (unsigned long long)stats->samples_taken);
    printf("║ Profile Changes:     %-38llu ║\n", (unsigned long long)stats->profile_changes);
    printf("║ Events Sent:         %-38llu ║\n", (unsigned long long)stats->events_sent);
    printf("║ Avg Battery Level:   %5.1f%%                                  ║\n", stats->avg_battery_level);
    printf("║ Avg Discharge Rate:  %7.1f mW                              ║\n", stats->avg_discharge_rate_mw);
    printf("║ Max Temperature:     %5.1f°C                                 ║\n", stats->max_temperature_c);
    printf("║ Thermal Throttles:   %-38u ║\n", stats->thermal_throttles);
    printf("║ Uptime:              %-34llu seconds ║\n", (unsigned long long)stats->uptime_s);
    printf("╚════════════════════════════════════════════════════════════════╝\n");
}

/**
 * @brief Main demo
 */
int main(void) {
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║         PORTIA POWER-AWARE TIER SYSTEM DEMONSTRATION          ║\n");
    printf("║                                                                ║\n");
    printf("║  Inspired by Portia fimbriata - the jumping spider that       ║\n");
    printf("║  optimizes energy expenditure based on metabolic state        ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    // Initialize logging
    nimcp_log_config_t log_config = nimcp_log_default_config();
    log_config.level = LOG_LEVEL_INFO;
    log_config.destinations = NIMCP_LOG_DEST_CONSOLE;
    nimcp_log_init(&log_config);

    // Initialize power monitoring
    LOG_INFO("[%s] Initializing power monitoring...", DEMO_MODULE);
    portia_power_config_t power_config = portia_power_default_config();
    power_config.poll_interval_ms = 2000;  // Poll every 2 seconds
    power_config.auto_adjust_profile = true;

    portia_power_manager_t pm = portia_power_init(&power_config);
    if (!pm) {
        LOG_ERROR("[%s] Failed to initialize power monitoring", DEMO_MODULE);
        return 1;
    }

    // Detect platform tier
    platform_tier_t base_tier = platform_tier_detect();
    LOG_INFO("[%s] Base platform tier: %s", DEMO_MODULE, platform_tier_get_name(base_tier));

    // Wait a moment for initial readings
    sleep(3);

    // Get initial status
    power_status_t status;
    if (portia_power_get_status(pm, &status)) {
        print_power_status(&status);
    }

    // Get current profile
    power_profile_t profile = portia_power_get_profile(pm);
    LOG_INFO("[%s] Current power profile: %s", DEMO_MODULE,
             portia_power_get_profile_name(profile));

    // Get tier configuration for current profile
    power_tier_config_t tier_config = portia_power_get_tier_config(pm, base_tier, profile);
    print_tier_config(&tier_config);
    print_cognitive_modules(tier_config.cognitive_modules);

    // Estimate runtime
    float runtime_s = portia_power_estimate_runtime(pm, 0.9f);
    if (runtime_s > 0.0f) {
        int hours = (int)(runtime_s / 3600.0f);
        int minutes = (int)((runtime_s - hours * 3600) / 60.0f);
        printf("\n⚡ Estimated Runtime: %d hours %d minutes\n", hours, minutes);
    } else {
        printf("\n⚡ Estimated Runtime: Unlimited (AC power)\n");
    }

    // Demonstrate all power profiles
    printf("\n\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║           DEMONSTRATION OF ALL POWER PROFILES                 ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    power_profile_t profiles[] = {
        POWER_PROFILE_PERFORMANCE,
        POWER_PROFILE_BALANCED,
        POWER_PROFILE_SAVER,
        POWER_PROFILE_CRITICAL,
        POWER_PROFILE_EMERGENCY
    };

    for (int i = 0; i < 5; i++) {
        printf("\n\n┌────────────────────────────────────────────────────────────────┐\n");
        printf("│ Testing Profile: %-45s │\n", portia_power_get_profile_name(profiles[i]));
        printf("└────────────────────────────────────────────────────────────────┘\n");

        // Set profile
        portia_power_set_profile(pm, profiles[i]);

        // Get configuration
        power_tier_config_t config = portia_power_get_tier_config(pm, base_tier, profiles[i]);
        print_tier_config(&config);
        print_cognitive_modules(config.cognitive_modules);

        sleep(1);
    }

    // Monitor for a bit
    printf("\n\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║              MONITORING POWER STATUS (10 seconds)             ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    for (int i = 0; i < 5; i++) {
        sleep(2);

        if (portia_power_get_status(pm, &status)) {
            printf("\n[Sample %d] Battery: %.1f%%, Discharge: %.1f mW, Temp: %.1f°C\n",
                   i + 1,
                   status.battery_level_pct,
                   status.discharge_rate_mw,
                   status.temperature_c);
        }
    }

    // Show statistics
    portia_power_stats_t stats;
    if (portia_power_get_stats(pm, &stats)) {
        print_stats(&stats);
    }

    // Cleanup
    LOG_INFO("[%s] Shutting down power monitoring...", DEMO_MODULE);
    portia_power_shutdown(pm);

    printf("\n\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    DEMO COMPLETE                               ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    nimcp_log_shutdown();
    return 0;
}
