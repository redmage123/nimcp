#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// Headers have their own extern "C" guards
#include "nimcp.h"
#include "utils/signal/nimcp_signal_filter.h"
#include "utils/memory/nimcp_memory.h"

#define M_PI 3.14159265358979323846

int main() {
    nimcp_init();

    const uint32_t n = 1024;
    const float sample_rate = 1000.0f;
    const float test_freq = 6.0f;  // Theta frequency

    // Generate 6Hz pure tone
    float* signal = (float*)nimcp_malloc(n * sizeof(float));
    for (uint32_t i = 0; i < n; i++) {
        float t = (float)i / sample_rate;
        signal[i] = sinf(2.0f * M_PI * test_freq * t);
    }

    printf("Testing signal filter with 6Hz signal (Theta band 4-8Hz)\n");
    printf("====================================================\n\n");

    // Test each band filter
    const char* band_names[] = {"Delta", "Theta", "Alpha", "Beta", "Gamma"};
    float band_ranges[][2] = {
        {0.0f, 4.0f},      // Delta
        {4.0f, 8.0f},      // Theta
        {8.0f, 13.0f},     // Alpha
        {13.0f, 30.0f},    // Beta
        {30.0f, 100.0f}    // Gamma
    };

    for (int b = 0; b < 5; b++) {
        float* filtered = (float*)nimcp_malloc(n * sizeof(float));

        signal_filter_config_t config;
        signal_filter_t* filter = NULL;

        // Use lowpass for Delta, bandpass for others
        if (band_ranges[b][0] < 0.5f) {
            config = signal_filter_lowpass_config(band_ranges[b][1], sample_rate);
        } else {
            config = signal_filter_bandpass_config(band_ranges[b][0], band_ranges[b][1], sample_rate);
        }

        filter = signal_filter_create(&config);
        if (!filter) {
            printf("%s: FAILED to create filter\n", band_names[b]);
            nimcp_free(filtered);
            continue;
        }

        bool success = signal_filter_apply(filter, signal, filtered, n);
        signal_filter_destroy(filter);

        if (!success) {
            printf("%s: FAILED to apply filter\n", band_names[b]);
            nimcp_free(filtered);
            continue;
        }

        // Compute RMS power (skip first 100 and last 100 samples to avoid transients)
        float power = 0.0f;
        uint32_t count = 0;
        for (uint32_t i = 100; i < n - 100; i++) {
            power += filtered[i] * filtered[i];
            count++;
        }
        power = sqrtf(power / (float)count);

        printf("%s (%2.0f-%3.0fHz): RMS power = %.6f%s\n",
               band_names[b],
               band_ranges[b][0],
               band_ranges[b][1],
               power,
               (b == 1) ? " <-- EXPECTED MAXIMUM" : "");

        nimcp_free(filtered);
    }

    printf("\nExpected: Theta should have highest RMS power\n");

    nimcp_free(signal);
    nimcp_shutdown();
    return 0;
}
