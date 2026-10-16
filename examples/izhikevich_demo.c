//=============================================================================
// izhikevich_demo.c - Izhikevich Neuron Model Demonstration
//=============================================================================
/**
 * @file izhikevich_demo.c
 * @brief Demonstrates all Izhikevich neuron firing patterns
 *
 * WHAT THIS DEMO SHOWS:
 * 1. Seven different neuron types with distinct firing patterns
 * 2. Response to constant current injection
 * 3. Voltage traces and spike timing
 * 4. Parameter sensitivity
 *
 * FIRING PATTERNS DEMONSTRATED:
 * - Regular Spiking (RS): Adapting excitatory neuron
 * - Intrinsically Bursting (IB): Burst generation
 * - Chattering (CH): Fast rhythmic bursting
 * - Fast Spiking (FS): Non-adapting inhibitory neuron
 * - Low-Threshold Spiking (LTS): Rebound behavior
 * - Thalamo-Cortical (TC): Thalamic relay neuron
 * - Resonator (RZ): Subthreshold oscillations
 *
 * OUTPUT:
 * - Console: Spike times and statistics
 * - CSV files: Voltage traces for plotting
 *
 * USAGE:
 * ./izhikevich_demo
 *
 * @author NIMCP Development Team
 * @date 2025-11-06
 */

#include "core/neuron_models/nimcp_neuron_model.h"
#include "core/neuron_models/nimcp_izhikevich.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//=============================================================================
// Configuration
//=============================================================================

#define SIMULATION_DURATION_MS 500.0f  /**< Total simulation time */
#define TIMESTEP_MS 0.25f              /**< Integration timestep */
#define INPUT_CURRENT 10.0f            /**< Constant input current */
#define CSV_OUTPUT_DIR "."             /**< Output directory for CSV files */

//=============================================================================
// Demo Functions
//=============================================================================

/**
 * @brief Run simulation for single neuron preset
 *
 * WHAT: Simulates one Izhikevich neuron type for given duration
 * WHY: Demonstrates characteristic firing pattern
 * HOW: Steps through time, records voltage, detects spikes
 *
 * @param preset Neuron type to simulate
 * @param duration_ms Simulation duration in milliseconds
 * @param dt Timestep in milliseconds
 * @param input_current Constant input current
 */
static void simulate_preset(izhikevich_preset_t preset, float duration_ms, float dt,
                             float input_current) {
    // Get preset information
    const char* name = izhikevich_get_preset_name(preset);
    const char* desc = izhikevich_get_preset_description(preset);
    izhikevich_params_t params = izhikevich_get_preset_params(preset);

    printf("\n");
    printf("========================================================================\n");
    printf("Neuron Type: %s\n", name);
    printf("Description: %s\n", desc);
    printf("Parameters: a=%.3f, b=%.3f, c=%.1f, d=%.3f\n", params.a, params.b, params.c, params.d);
    printf("========================================================================\n");

    // Create neuron with this preset
    const neuron_model_vtable_t* vtable = izhikevich_get_vtable();
    neuron_model_state_t neuron = neuron_model_create(vtable, &params);

    if (!neuron) {
        fprintf(stderr, "ERROR: Failed to create neuron for preset %s\n", name);
        return;
    }

    // Open CSV file for voltage trace
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/izhikevich_%s.csv", CSV_OUTPUT_DIR, name);

    // Replace spaces with underscores
    for (char* p = filename; *p; p++) {
        if (*p == ' ' || *p == '(' || *p == ')') {
            *p = '_';
        }
    }

    FILE* csv = fopen(filename, "w");
    if (csv) {
        fprintf(csv, "time_ms,voltage_mv\n");
    }

    // Simulation variables
    float time = 0.0f;
    int num_steps = (int)(duration_ms / dt);
    int spike_count = 0;
    float first_spike_time = -1.0f;
    float last_spike_time = -1.0f;

    printf("\nSimulating for %.1f ms with I=%.1f, dt=%.3f ms...\n", duration_ms, input_current,
           dt);

    // Main simulation loop
    for (int step = 0; step < num_steps; step++) {
        // Update neuron
        neuron_model_update(neuron, dt, input_current);

        // Get current voltage
        float voltage = neuron_model_get_voltage(neuron);

        // Check for spike
        if (neuron_model_check_spike(neuron)) {
            spike_count++;

            if (first_spike_time < 0.0f) {
                first_spike_time = time;
            }
            last_spike_time = time;

            // Print first few spikes
            if (spike_count <= 5) {
                printf("  Spike #%d at t=%.2f ms (v=%.2f mV)\n", spike_count, time, voltage);
            } else if (spike_count == 6) {
                printf("  ... (remaining spikes omitted)\n");
            }

            neuron_model_post_spike(neuron);
        }

        // Record voltage to CSV (every 4th point to reduce file size)
        if (csv && step % 4 == 0) {
            fprintf(csv, "%.3f,%.3f\n", time, voltage);
        }

        time += dt;
    }

    if (csv) {
        fclose(csv);
        printf("\nVoltage trace saved to: %s\n", filename);
    }

    // Print summary statistics
    printf("\nSummary:\n");
    printf("  Total spikes: %d\n", spike_count);

    if (spike_count > 0) {
        printf("  First spike:  %.2f ms\n", first_spike_time);
        printf("  Last spike:   %.2f ms\n", last_spike_time);

        if (spike_count > 1) {
            float duration = last_spike_time - first_spike_time;
            float frequency = (spike_count - 1) / (duration / 1000.0f);  // Hz
            printf("  Average frequency: %.2f Hz\n", frequency);
        }
    } else {
        printf("  No spikes detected (subthreshold activity)\n");
    }

    // Cleanup
    neuron_model_destroy(neuron);
}

/**
 * @brief Print usage information
 */
static void print_usage(void) {
    printf("\n");
    printf("=======================================================================\n");
    printf("NIMCP Izhikevich Neuron Model Demonstration\n");
    printf("=======================================================================\n");
    printf("\n");
    printf("This demo simulates 7 different neuron types and shows their\n");
    printf("characteristic firing patterns in response to constant current.\n");
    printf("\n");
    printf("Neuron Types:\n");
    printf("  1. Regular Spiking (RS)        - Adapting cortical pyramidal cell\n");
    printf("  2. Intrinsically Bursting (IB) - Bursting neuron\n");
    printf("  3. Chattering (CH)              - Fast rhythmic bursting\n");
    printf("  4. Fast Spiking (FS)            - Inhibitory interneuron\n");
    printf("  5. Low-Threshold Spiking (LTS)  - Low-threshold interneuron\n");
    printf("  6. Thalamo-Cortical (TC)        - Thalamic relay neuron\n");
    printf("  7. Resonator (RZ)               - Subthreshold oscillator\n");
    printf("\n");
    printf("Output: CSV files with voltage traces (for plotting with gnuplot/Python)\n");
    printf("\n");
    printf("=======================================================================\n");
    printf("\n");
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    print_usage();

    printf("Starting simulations...\n");
    printf("\n");

    // Simulate all presets
    simulate_preset(IZHI_PRESET_REGULAR_SPIKING, SIMULATION_DURATION_MS, TIMESTEP_MS,
                    INPUT_CURRENT);

    simulate_preset(IZHI_PRESET_INTRINSICALLY_BURSTING, SIMULATION_DURATION_MS, TIMESTEP_MS,
                    INPUT_CURRENT);

    simulate_preset(IZHI_PRESET_CHATTERING, SIMULATION_DURATION_MS, TIMESTEP_MS, INPUT_CURRENT);

    simulate_preset(IZHI_PRESET_FAST_SPIKING, SIMULATION_DURATION_MS, TIMESTEP_MS, INPUT_CURRENT);

    simulate_preset(IZHI_PRESET_LOW_THRESHOLD_SPIKING, SIMULATION_DURATION_MS, TIMESTEP_MS,
                    INPUT_CURRENT);

    simulate_preset(IZHI_PRESET_THALAMO_CORTICAL, SIMULATION_DURATION_MS, TIMESTEP_MS,
                    INPUT_CURRENT);

    simulate_preset(IZHI_PRESET_RESONATOR, SIMULATION_DURATION_MS, TIMESTEP_MS, INPUT_CURRENT);

    printf("\n");
    printf("=======================================================================\n");
    printf("All simulations complete!\n");
    printf("=======================================================================\n");
    printf("\n");
    printf("To visualize voltage traces, use Python/matplotlib:\n");
    printf("  import pandas as pd\n");
    printf("  import matplotlib.pyplot as plt\n");
    printf("  df = pd.read_csv('izhikevich_Regular_Spiking__RS_.csv')\n");
    printf("  plt.plot(df['time_ms'], df['voltage_mv'])\n");
    printf("  plt.xlabel('Time (ms)')\n");
    printf("  plt.ylabel('Voltage (mV)')\n");
    printf("  plt.title('Regular Spiking Neuron')\n");
    printf("  plt.show()\n");
    printf("\n");

    return 0;
}
