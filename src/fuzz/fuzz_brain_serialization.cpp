/**
 * @file fuzz_brain_serialization.cpp
 * @brief Fuzzing target for brain save/load serialization
 *
 * Tests brain serialization and deserialization with corrupted data
 * to discover parsing bugs, buffer overflows, and format errors.
 *
 * CRITICAL: Brain file loading is a major attack surface. Corrupted
 * brain files could crash the system, leak memory, or worse.
 *
 * This fuzzer tests:
 * - Malformed brain file headers
 * - Invalid neuron counts (integer overflow)
 * - Corrupted synapse data
 * - Invalid magic numbers
 * - Truncated files
 * - Oversized allocations
 *
 * Build:
 *   cmake -DENABLE_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++ ..
 *   make fuzz_brain_serialization
 *
 * Run:
 *   ./fuzz_brain_serialization -max_total_time=600
 *   ./fuzz_brain_serialization corpus_brain/ -max_total_time=3600
 *
 * Expected to find:
 * - Buffer overflows in deserialization
 * - Integer overflows in size calculations
 * - NULL pointer dereferences
 * - Memory leaks
 * - Assertion failures
 * - Format string bugs
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include "core/brain/nimcp_brain.h"

// Minimum file size to be interesting (header size)
#define MIN_INPUT_SIZE 64

/**
 * @brief LLVMFuzzerTestOneInput - Main fuzzer entry point
 *
 * WHAT: Tests brain_load() with random/corrupted file data
 * WHY:  Find bugs in deserialization before they reach production
 * HOW:  Write fuzzer data to temp file, try to load as brain
 *
 * @param data Fuzzer-generated random bytes
 * @param size Length of data
 * @return 0 always (non-zero would keep this input in corpus)
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Need at least a minimal header
    if (size < MIN_INPUT_SIZE) {
        return 0;
    }

    // Create unique temp file for this fuzzing iteration
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/tmp/fuzz_brain_%d.nimcp", getpid());

    // Write fuzzer data to file
    FILE* fp = fopen(temp_path, "wb");
    if (!fp) {
        return 0;
    }

    fwrite(data, 1, size, fp);
    fclose(fp);

    // Test 1: Try to load corrupted brain file
    brain_t brain = brain_load(temp_path);

    if (brain != NULL) {
        // If load succeeded, verify brain is valid
        brain_stats_t stats;
        if (brain_get_stats(brain, &stats)) {
            // Check for obviously invalid values
            if (stats.num_neurons > 1000000) {
                fprintf(stderr, "FUZZ ERROR: Suspiciously large neuron count: %u\n",
                        stats.num_neurons);
            }
            if (stats.num_synapses > 10000000) {
                fprintf(stderr, "FUZZ ERROR: Suspiciously large synapse count: %llu\n",
                        (unsigned long long)stats.num_synapses);
            }
        }

        // Try to save it back (tests serialization too)
        char temp_save_path[256];
        snprintf(temp_save_path, sizeof(temp_save_path),
                 "/tmp/fuzz_brain_save_%d.nimcp", getpid());
        brain_save(brain, temp_save_path);
        unlink(temp_save_path);

        // Try to use the brain (tests if state is valid)
        float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                               0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
        brain_decision_t decision = brain_decide(brain, features, 10);

        // Check for NaN/Inf in output
        if (decision.label) {
            if (decision.confidence < 0.0f || decision.confidence > 1.0f) {
                fprintf(stderr, "FUZZ ERROR: Invalid confidence: %f\n",
                        decision.confidence);
            }
            brain_free_decision(&decision);
        }

        // Cleanup
        brain_destroy(brain);
    }

    // Test 2: Try to load with modified magic number
    if (size >= 8) {
        uint8_t* modified_data = (uint8_t*)malloc(size);
        if (modified_data) {
            memcpy(modified_data, data, size);

            // Corrupt magic number (first 8 bytes typically)
            modified_data[0] ^= 0xFF;
            modified_data[1] ^= 0xFF;

            FILE* fp2 = fopen(temp_path, "wb");
            if (fp2) {
                fwrite(modified_data, 1, size, fp2);
                fclose(fp2);

                brain_t brain2 = brain_load(temp_path);
                if (brain2) {
                    // Should have rejected bad magic, but didn't!
                    fprintf(stderr, "FUZZ ERROR: Loaded brain despite bad magic!\n");
                    brain_destroy(brain2);
                }
            }

            free(modified_data);
        }
    }

    // Test 3: Try to load truncated file
    if (size > 100) {
        FILE* fp3 = fopen(temp_path, "wb");
        if (fp3) {
            // Write only half the data
            fwrite(data, 1, size / 2, fp3);
            fclose(fp3);

            brain_t brain3 = brain_load(temp_path);
            if (brain3) {
                brain_destroy(brain3);
            }
        }
    }

    // Test 4: Edge cases with NULL
    brain_load(NULL);  // Should handle gracefully
    brain_load("");    // Should handle gracefully
    brain_load("/nonexistent/path/to/brain.nimcp");  // Should handle gracefully

    // Cleanup temp file
    unlink(temp_path);

    return 0;
}

/**
 * @brief LLVMFuzzerInitialize - Optional fuzzer initialization
 *
 * WHAT: Initialize brain system before fuzzing
 * WHY:  Set up any global state
 * HOW:  Called once at startup
 */
extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    (void)argc;
    (void)argv;

    // Suppress error messages to avoid spam
    // (fuzzer will still detect crashes/assertions)

    return 0;
}
