/**
 * @file test_linker_stubs.c
 * @brief Stub implementations for undefined symbols in libnimcp.so
 *
 * These functions are declared as 'extern' in sensorimotor.c but have
 * no implementation compiled into the library yet. Providing stubs
 * here allows test binaries to link successfully.
 */

#include <stdint.h>

int nimcp_brain_learn(void* brain, const float* features,
                      uint32_t n_features, const float* target,
                      uint32_t target_size, const char* label,
                      float confidence, float lr) {
    (void)brain; (void)features; (void)n_features;
    (void)target; (void)target_size; (void)label;
    (void)confidence; (void)lr;
    return 0;
}

int nimcp_brain_update_reward(void* brain, float reward, float confidence) {
    (void)brain; (void)reward; (void)confidence;
    return 0;
}

int nimcp_motor_output_translate(void* motor, const float* brain_out,
                                  uint32_t brain_dim, float* commands_out,
                                  uint32_t max_commands) {
    (void)motor; (void)brain_out; (void)brain_dim;
    (void)commands_out; (void)max_commands;
    return 0;
}

int nimcp_brain_infer(void* brain, const float* features,
                      uint32_t num_features, float* outputs,
                      uint32_t num_outputs) {
    (void)brain; (void)features; (void)num_features;
    (void)outputs; (void)num_outputs;
    return 0;
}

uint32_t nimcp_motor_output_get_num_channels(const void* motor) {
    (void)motor;
    return 0;
}
