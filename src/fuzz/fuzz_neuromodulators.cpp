/**
 * @file fuzz_neuromodulators.cpp
 * @brief Fuzzing target for neuromodulator system
 *
 * Tests neuromodulator dynamics with random/corrupted data to discover
 * bugs in biochemical simulation, level tracking, and receptor effects.
 *
 * CRITICAL: Neuromodulator accuracy is essential for:
 * - Learning rate gating (when to learn)
 * - Attention allocation (what to process)
 * - Memory consolidation (what to remember)
 * - Emotional context (how to value information)
 *
 * Bugs here could cause:
 * - NaN propagation through learning rates
 * - Invalid concentration ranges (negative, > 1.0)
 * - Incorrect decay dynamics (accumulation, divergence)
 * - Reward prediction errors (reinforcement learning failures)
 * - Memory leaks in receptor profiles
 *
 * This fuzzer tests:
 * - System creation with random configs
 * - Setting/getting levels with invalid values (NaN, Inf, negative, > 1.0)
 * - Release functions with extreme magnitudes
 * - Decay dynamics (stability, convergence)
 * - Receptor effect computation
 * - Edge cases (NULL, zero decay, extreme time steps)
 *
 * Build:
 *   cmake -DENABLE_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++ ..
 *   make fuzz_neuromodulators
 *
 * Run:
 *   ./fuzz_neuromodulators -max_total_time=600
 *   ./fuzz_neuromodulators corpus_neuromod/ -max_total_time=3600
 *
 * Expected to find:
 * - NaN/Inf in concentration calculations
 * - Negative or > 1.0 concentration bugs
 * - Decay instability (divergence, oscillation)
 * - Division by zero in effect computation
 * - Memory leaks in system creation/destruction
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

// Minimum input size to be interesting
#define MIN_INPUT_SIZE 64

/**
 * @brief Check if float is valid (not NaN, not Inf)
 */
static bool is_valid_float(float value)
{
    return !std::isnan(value) && !std::isinf(value);
}

/**
 * @brief Check if concentration is in valid biological range
 */
static bool is_valid_concentration(float value)
{
    return is_valid_float(value) && value >= 0.0f && value <= 1.0f;
}

/**
 * @brief Test neuromodulator system with fuzzer data
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Need at least config data
    if (size < MIN_INPUT_SIZE) {
        return 0;
    }

    // Test 1: Create system with random config
    neuromodulator_config_t config;
    memcpy(&config, data, sizeof(config));

    // Sanitize config to reasonable ranges (but allow some invalid values to test handling)
    if (config.dopamine_decay <= 0.0f || config.dopamine_decay > 1000.0f) {
        config.dopamine_decay = 2.0f;
    }
    if (config.serotonin_decay <= 0.0f || config.serotonin_decay > 1000.0f) {
        config.serotonin_decay = 10.0f;
    }
    if (config.acetylcholine_decay <= 0.0f || config.acetylcholine_decay > 1000.0f) {
        config.acetylcholine_decay = 0.5f;
    }
    if (config.norepinephrine_decay <= 0.0f || config.norepinephrine_decay > 1000.0f) {
        config.norepinephrine_decay = 3.0f;
    }

    neuromodulator_system_t system = neuromodulator_system_create(&config);
    if (!system) {
        return 0;  // Failed to create (OOM or invalid config)
    }

    // Test 2: Get and verify initial levels
    neuromodulator_pool_t pool;
    if (neuromodulator_get_levels(system, &pool)) {
        // Check for invalid concentrations
        if (!is_valid_concentration(pool.dopamine)) {
            fprintf(stderr, "FUZZ ERROR: Invalid initial dopamine: %f\n", pool.dopamine);
        }
        if (!is_valid_concentration(pool.serotonin)) {
            fprintf(stderr, "FUZZ ERROR: Invalid initial serotonin: %f\n", pool.serotonin);
        }
        if (!is_valid_concentration(pool.acetylcholine)) {
            fprintf(stderr, "FUZZ ERROR: Invalid initial acetylcholine: %f\n", pool.acetylcholine);
        }
        if (!is_valid_concentration(pool.norepinephrine)) {
            fprintf(stderr, "FUZZ ERROR: Invalid initial norepinephrine: %f\n", pool.norepinephrine);
        }
    }

    // Test 3: Random operations from fuzzer data
    size_t offset = sizeof(config);

    while (offset + 16 <= size) {
        uint8_t op_code = data[offset++];

        if (op_code % 8 == 0) {
            // SET LEVEL operation
            if (offset + 8 > size) break;

            uint32_t type_raw = *(uint32_t*)(data + offset);
            offset += 4;

            neuromodulator_type_t type = (neuromodulator_type_t)(type_raw % NEUROMOD_COUNT);

            float level = *(float*)(data + offset);
            offset += 4;

            // Try to set level (should handle invalid values gracefully)
            if (!is_valid_float(level)) {
                fprintf(stderr, "FUZZ: Setting invalid level (NaN/Inf): %f\n", level);
            }

            neuromodulator_set_level(system, type, level);

            // Verify level was set correctly
            float retrieved = neuromodulator_get_level(system, type);
            if (!is_valid_concentration(retrieved)) {
                fprintf(stderr, "FUZZ ERROR: Invalid concentration after set: %f\n", retrieved);
            }

        } else if (op_code % 8 == 1) {
            // RELEASE DOPAMINE operation
            if (offset + 8 > size) break;

            float reward_magnitude = *(float*)(data + offset);
            offset += 4;

            float predicted_reward = *(float*)(data + offset);
            offset += 4;

            // Release dopamine
            float released = neuromodulator_release_dopamine(system, reward_magnitude,
                                                            predicted_reward);

            if (!is_valid_float(released)) {
                fprintf(stderr, "FUZZ ERROR: Invalid dopamine release: %f\n", released);
            }

            // Check dopamine level after release
            float dopamine_level = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
            if (!is_valid_concentration(dopamine_level)) {
                fprintf(stderr, "FUZZ ERROR: Invalid dopamine level after release: %f\n",
                        dopamine_level);
            }

        } else if (op_code % 8 == 2) {
            // RELEASE SEROTONIN operation
            if (offset + 4 > size) break;

            float punishment_magnitude = *(float*)(data + offset);
            offset += 4;

            float released = neuromodulator_release_serotonin(system, punishment_magnitude);

            if (!is_valid_float(released)) {
                fprintf(stderr, "FUZZ ERROR: Invalid serotonin release: %f\n", released);
            }

            float serotonin_level = neuromodulator_get_level(system, NEUROMOD_SEROTONIN);
            if (!is_valid_concentration(serotonin_level)) {
                fprintf(stderr, "FUZZ ERROR: Invalid serotonin level: %f\n", serotonin_level);
            }

        } else if (op_code % 8 == 3) {
            // RELEASE ACETYLCHOLINE operation
            if (offset + 4 > size) break;

            float salience = *(float*)(data + offset);
            offset += 4;

            float released = neuromodulator_release_acetylcholine(system, salience);

            if (!is_valid_float(released)) {
                fprintf(stderr, "FUZZ ERROR: Invalid acetylcholine release: %f\n", released);
            }

            float ach_level = neuromodulator_get_level(system, NEUROMOD_ACETYLCHOLINE);
            if (!is_valid_concentration(ach_level)) {
                fprintf(stderr, "FUZZ ERROR: Invalid acetylcholine level: %f\n", ach_level);
            }

        } else if (op_code % 8 == 4) {
            // RELEASE NOREPINEPHRINE operation
            if (offset + 8 > size) break;

            float threat_level = *(float*)(data + offset);
            offset += 4;

            float uncertainty = *(float*)(data + offset);
            offset += 4;

            float released = neuromodulator_release_norepinephrine(system, threat_level,
                                                                  uncertainty);

            if (!is_valid_float(released)) {
                fprintf(stderr, "FUZZ ERROR: Invalid norepinephrine release: %f\n", released);
            }

            float ne_level = neuromodulator_get_level(system, NEUROMOD_NOREPINEPHRINE);
            if (!is_valid_concentration(ne_level)) {
                fprintf(stderr, "FUZZ ERROR: Invalid norepinephrine level: %f\n", ne_level);
            }

        } else if (op_code % 8 == 5) {
            // UPDATE (decay) operation
            if (offset + 4 > size) break;

            float dt = *(float*)(data + offset);
            offset += 4;

            // Update dynamics
            if (!is_valid_float(dt)) {
                fprintf(stderr, "FUZZ: Updating with invalid dt (NaN/Inf): %f\n", dt);
            }

            neuromodulator_update(system, dt);

            // Check all levels after update
            neuromodulator_pool_t pool_after;
            if (neuromodulator_get_levels(system, &pool_after)) {
                if (!is_valid_concentration(pool_after.dopamine)) {
                    fprintf(stderr, "FUZZ ERROR: Invalid dopamine after update: %f\n",
                            pool_after.dopamine);
                }
                if (!is_valid_concentration(pool_after.serotonin)) {
                    fprintf(stderr, "FUZZ ERROR: Invalid serotonin after update: %f\n",
                            pool_after.serotonin);
                }
                if (!is_valid_concentration(pool_after.acetylcholine)) {
                    fprintf(stderr, "FUZZ ERROR: Invalid acetylcholine after update: %f\n",
                            pool_after.acetylcholine);
                }
                if (!is_valid_concentration(pool_after.norepinephrine)) {
                    fprintf(stderr, "FUZZ ERROR: Invalid norepinephrine after update: %f\n",
                            pool_after.norepinephrine);
                }
            }

        } else if (op_code % 8 == 6) {
            // COMPUTE EFFECTS operation
            if (offset + sizeof(receptor_profile_t) > size) break;

            receptor_profile_t receptors;
            memcpy(&receptors, data + offset, sizeof(receptors));
            offset += sizeof(receptors);

            modulation_effects_t effects;
            if (neuromodulator_compute_effects(system, &receptors, &effects)) {
                // Verify effects are valid
                if (!is_valid_float(effects.learning_rate_multiplier)) {
                    fprintf(stderr, "FUZZ ERROR: Invalid learning_rate_multiplier: %f\n",
                            effects.learning_rate_multiplier);
                }

                if (!is_valid_float(effects.transmission_gain)) {
                    fprintf(stderr, "FUZZ ERROR: Invalid transmission_gain: %f\n",
                            effects.transmission_gain);
                }

                if (!is_valid_float(effects.excitability_shift)) {
                    fprintf(stderr, "FUZZ ERROR: Invalid excitability_shift: %f\n",
                            effects.excitability_shift);
                }

                if (!is_valid_float(effects.attention_weight)) {
                    fprintf(stderr, "FUZZ ERROR: Invalid attention_weight: %f\n",
                            effects.attention_weight);
                }

                // Test modulation functions
                float base_lr = 0.01f;
                float modulated_lr = neuromodulator_modulate_learning_rate(base_lr, &effects);
                if (!is_valid_float(modulated_lr) || modulated_lr < 0.0f) {
                    fprintf(stderr, "FUZZ ERROR: Invalid modulated learning rate: %f\n",
                            modulated_lr);
                }

                float base_weight = 0.5f;
                float modulated_weight = neuromodulator_modulate_transmission(base_weight, &effects);
                if (!is_valid_float(modulated_weight)) {
                    fprintf(stderr, "FUZZ ERROR: Invalid modulated weight: %f\n",
                            modulated_weight);
                }

                float base_threshold = 0.3f;
                float modulated_threshold = neuromodulator_modulate_threshold(base_threshold,
                                                                              &effects);
                if (!is_valid_float(modulated_threshold)) {
                    fprintf(stderr, "FUZZ ERROR: Invalid modulated threshold: %f\n",
                            modulated_threshold);
                }
            }

        } else if (op_code % 8 == 7) {
            // GET STATS operation
            neuromodulator_stats_t stats;
            if (neuromodulator_get_stats(system, &stats)) {
                // Verify stats are valid
                if (!is_valid_concentration(stats.current_dopamine)) {
                    fprintf(stderr, "FUZZ ERROR: Invalid stats dopamine: %f\n",
                            stats.current_dopamine);
                }

                if (!is_valid_float(stats.dopamine_variance) ||
                    stats.dopamine_variance < 0.0f) {
                    fprintf(stderr, "FUZZ ERROR: Invalid dopamine variance: %f\n",
                            stats.dopamine_variance);
                }
            }
        }
    }

    // Test 4: Stress test with rapid releases and updates
    for (int i = 0; i < 100 && i < (int)size / 4; i++) {
        neuromodulator_release_dopamine(system, 0.5f, 0.3f);
        neuromodulator_update(system, 0.01f);

        // Check for accumulation bug (should decay back to baseline)
        float dopamine = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
        if (dopamine > 1.5f) {  // Allow some overshoot, but not too much
            fprintf(stderr, "FUZZ ERROR: Dopamine accumulation detected: %f\n", dopamine);
            break;
        }
    }

    // Test 5: Reset and verify
    neuromodulator_reset(system);

    neuromodulator_pool_t final_pool;
    if (neuromodulator_get_levels(system, &final_pool)) {
        // Should be back to baseline after reset
        // (Allow some tolerance for floating point)
        if (!is_valid_concentration(final_pool.dopamine)) {
            fprintf(stderr, "FUZZ ERROR: Invalid dopamine after reset: %f\n",
                    final_pool.dopamine);
        }
    }

    neuromodulator_system_destroy(system);

    // Test 6: Edge cases
    neuromodulator_system_destroy(NULL);  // Should handle gracefully

    neuromodulator_system_t null_config_system = neuromodulator_system_create(NULL);
    if (null_config_system) {
        // Should create with default config
        neuromodulator_system_destroy(null_config_system);
    }

    // Test 7: Receptor profile presets (should never crash)
    receptor_profile_t cortical_ex = neuromodulator_profile_cortical_excitatory();
    receptor_profile_t cortical_in = neuromodulator_profile_cortical_inhibitory();
    receptor_profile_t hippocampal = neuromodulator_profile_hippocampal();
    receptor_profile_t striatal = neuromodulator_profile_striatal();
    receptor_profile_t amygdala = neuromodulator_profile_amygdala();

    (void)cortical_ex;
    (void)cortical_in;
    (void)hippocampal;
    (void)striatal;
    (void)amygdala;

    return 0;
}

/**
 * @brief LLVMFuzzerInitialize - Optional fuzzer initialization
 */
extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    (void)argc;
    (void)argv;

    // Suppress error messages to avoid spam
    // (fuzzer will still detect crashes/assertions)

    return 0;
}
