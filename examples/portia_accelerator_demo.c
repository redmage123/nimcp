/**
 * @file portia_accelerator_demo.c
 * @brief Demonstration of Portia hardware accelerator detection
 *
 * WHAT: Interactive demo showing accelerator detection capabilities
 * WHY:  Showcase hardware detection and selection features
 * HOW:  Detect, display, and select optimal accelerators
 *
 * USAGE:
 *   ./portia_accelerator_demo
 *
 * @author NIMCP Portia Team
 * @date 2025-12-08
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "portia/nimcp_portia_accelerator.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Display Functions
//=============================================================================

static void print_banner(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     NIMCP Portia Hardware Accelerator Detection Demo        ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

static void print_section(const char* title) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");
}

static void print_accelerator_summary(portia_accelerator_system_t system) {
    uint32_t count = portia_accelerator_get_count(system);
    uint32_t mask = portia_accelerator_get_type_mask(system);

    printf("Total Accelerators Detected: %u\n\n", count);

    printf("Available Types:\n");
    if (mask & ACCELERATOR_TYPE_GPU) {
        printf("  ✓ GPU  - Graphics Processing Unit\n");
    }
    if (mask & ACCELERATOR_TYPE_NPU) {
        printf("  ✓ NPU  - Neural Processing Unit\n");
    }
    if (mask & ACCELERATOR_TYPE_DSP) {
        printf("  ✓ DSP  - Digital Signal Processor\n");
    }
    if (mask & ACCELERATOR_TYPE_FPGA) {
        printf("  ✓ FPGA - Field Programmable Gate Array\n");
    }
    if (mask & ACCELERATOR_TYPE_TPU) {
        printf("  ✓ TPU  - Tensor Processing Unit\n");
    }

    if (mask == 0) {
        printf("  (None detected - will use CPU)\n");
    }

    printf("\n");
}

static void print_accelerator_details(const accelerator_info_t* info, uint32_t index) {
    printf("─────────────────────────────────────────────────────────────\n");
    printf("Accelerator #%u\n", index);
    printf("─────────────────────────────────────────────────────────────\n");
    printf("  Type:          %s\n", portia_accelerator_type_name(info->type));
    printf("  Name:          %s\n", info->name);
    printf("  Vendor:        %s\n", info->vendor);
    printf("  Status:        %s\n", info->available ? "Available" : "Unavailable");
    printf("  Initialized:   %s\n", info->initialized ? "Yes" : "No");
    printf("\n");
    printf("  Compute Units: %u\n", info->compute_units);
    printf("  Memory:        %.2f GB\n",
           info->memory_bytes / (1024.0 * 1024.0 * 1024.0));
    printf("  Peak TFlops:   %.2f\n", info->peak_tflops);
    printf("  Power:         %.2f W\n", info->power_watts);
    printf("\n");

    // Power estimates
    printf("  Power Estimates:\n");
    printf("    Idle (0%%):   %.1f W\n",
           portia_accelerator_estimate_power(info, 0.0f));
    printf("    Med (50%%):   %.1f W\n",
           portia_accelerator_estimate_power(info, 50.0f));
    printf("    Full (100%%): %.1f W\n",
           portia_accelerator_estimate_power(info, 100.0f));

    printf("\n");
}

static void demonstrate_selection(portia_accelerator_system_t system) {
    accelerator_info_t best;

    if (!portia_accelerator_get_best(system, &best)) {
        printf("No accelerators available for selection.\n");
        return;
    }

    printf("Optimal Accelerator Selected:\n");
    printf("  %s (%s)\n", best.name, best.vendor);
    printf("  Type: %s\n", portia_accelerator_type_name(best.type));
    printf("  Performance: %.2f TFlops\n", best.peak_tflops);
    printf("  Memory: %.2f GB\n", best.memory_bytes / (1024.0 * 1024.0 * 1024.0));
    printf("  Power: %.2f W\n", best.power_watts);
    printf("\n");

    // Calculate efficiency
    float efficiency = best.peak_tflops / best.power_watts;
    printf("  Efficiency: %.3f TFlops/W\n", efficiency);

    // Set as preferred
    if (portia_accelerator_set_preferred(system, best.type)) {
        printf("  ✓ Set as preferred accelerator\n");
    }
}

static void demonstrate_scenarios(portia_accelerator_system_t system) {
    printf("Scenario Analysis:\n\n");

    // Scenario 1: High Performance
    printf("1. High Performance Workload:\n");
    portia_accelerator_config_t high_perf = {
        .detect_gpu = true,
        .detect_npu = true,
        .detect_dsp = true,
        .detect_fpga = true,
        .detect_tpu = true,
        .auto_select = true,
        .power_budget_watts = 500.0f,
        .min_memory_gb = 4.0f,
        .min_tflops = 5.0f,
        .prefer_low_power = false
    };

    uint32_t count = portia_accelerator_get_count(system);
    float best_score = 0.0f;
    accelerator_info_t best_for_perf = {0};

    for (uint32_t i = 0; i < count; i++) {
        accelerator_info_t info;
        if (portia_accelerator_get_info(system, i, &info)) {
            float score = portia_accelerator_calculate_score(&info, &high_perf);
            if (score > best_score) {
                best_score = score;
                best_for_perf = info;
            }
        }
    }

    if (best_score > 0) {
        printf("   Recommended: %s (score: %.1f)\n", best_for_perf.name, best_score);
        printf("   Reason: Highest raw performance\n\n");
    } else {
        printf("   No suitable accelerator found\n\n");
    }

    // Scenario 2: Power Efficient
    printf("2. Power-Efficient Workload:\n");
    portia_accelerator_config_t low_power = {
        .detect_gpu = true,
        .detect_npu = true,
        .detect_dsp = true,
        .detect_fpga = true,
        .detect_tpu = true,
        .auto_select = true,
        .power_budget_watts = 50.0f,
        .min_memory_gb = 0.5f,
        .min_tflops = 0.5f,
        .prefer_low_power = true
    };

    best_score = 0.0f;
    accelerator_info_t best_for_power = {0};

    for (uint32_t i = 0; i < count; i++) {
        accelerator_info_t info;
        if (portia_accelerator_get_info(system, i, &info)) {
            float score = portia_accelerator_calculate_score(&info, &low_power);
            if (score > best_score) {
                best_score = score;
                best_for_power = info;
            }
        }
    }

    if (best_score > 0) {
        printf("   Recommended: %s (score: %.1f)\n", best_for_power.name, best_score);
        printf("   Reason: Best performance per watt\n\n");
    } else {
        printf("   No suitable accelerator found\n\n");
    }

    // Scenario 3: Edge Deployment
    printf("3. Edge Deployment:\n");
    portia_accelerator_config_t edge = {
        .detect_gpu = false,
        .detect_npu = true,
        .detect_dsp = true,
        .detect_fpga = false,
        .detect_tpu = true,
        .auto_select = true,
        .power_budget_watts = 10.0f,
        .min_memory_gb = 0.1f,
        .min_tflops = 0.1f,
        .prefer_low_power = true,
        .require_int8 = true
    };

    best_score = 0.0f;
    accelerator_info_t best_for_edge = {0};

    for (uint32_t i = 0; i < count; i++) {
        accelerator_info_t info;
        if (portia_accelerator_get_info(system, i, &info)) {
            float score = portia_accelerator_calculate_score(&info, &edge);
            if (score > best_score) {
                best_score = score;
                best_for_edge = info;
            }
        }
    }

    if (best_score > 0) {
        printf("   Recommended: %s (score: %.1f)\n", best_for_edge.name, best_score);
        printf("   Reason: Ultra-low power for edge devices\n\n");
    } else {
        printf("   No suitable accelerator found\n\n");
    }
}

//=============================================================================
// Main Demo
//=============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Initialize logging
    nimcp_log_init("portia_accelerator_demo.log");
    nimcp_log_set_level(NIMCP_LOG_LEVEL_INFO);

    // Initialize BBB
    if (!bbb_helpers_is_initialized()) {
        if (!bbb_helpers_init()) {
            fprintf(stderr, "Failed to initialize BBB helpers\n");
            return 1;
        }
    }

    print_banner();

    // Initialize accelerator system
    print_section("Initialization");
    printf("Initializing Portia accelerator detection system...\n");

    portia_accelerator_config_t config = portia_accelerator_default_config();
    portia_accelerator_system_t system = portia_accelerator_init(&config);

    if (!system) {
        fprintf(stderr, "Failed to initialize accelerator system\n");
        return 1;
    }

    printf("✓ System initialized successfully\n");

    // Detect accelerators
    print_section("Hardware Detection");
    printf("Scanning for hardware accelerators...\n\n");

    uint32_t total = portia_accelerator_detect_all(system);

    printf("Detection complete!\n\n");

    print_accelerator_summary(system);

    // Display detailed info
    if (total > 0) {
        print_section("Detected Accelerators");

        for (uint32_t i = 0; i < total; i++) {
            accelerator_info_t info;
            if (portia_accelerator_get_info(system, i, &info)) {
                print_accelerator_details(&info, i);
            }
        }

        // Demonstrate selection
        print_section("Automatic Selection");
        demonstrate_selection(system);

        // Demonstrate scenarios
        print_section("Workload Scenarios");
        demonstrate_scenarios(system);
    } else {
        printf("\nNo hardware accelerators detected on this system.\n");
        printf("The system will fall back to CPU-based computation.\n");
    }

    // Cleanup
    print_section("Cleanup");
    printf("Shutting down accelerator system...\n");
    portia_accelerator_shutdown(system);
    printf("✓ Shutdown complete\n");

    nimcp_log_shutdown();
    bbb_helpers_shutdown();

    printf("\n");
    printf("Demo completed successfully!\n");
    printf("\n");

    return 0;
}
