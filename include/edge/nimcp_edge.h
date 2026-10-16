#ifndef NIMCP_EDGE_H
#define NIMCP_EDGE_H

/**
 * @file nimcp_edge.h
 * @brief Edge Brain Architecture — Distillation, Deployment & Distributed Cognition
 *
 * Master API for producing optimized child brains for resource-constrained
 * devices (phones, drones, IoT, robots) from a trained master brain.
 *
 * Subsystems:
 *   - Resize (expand/contract): runtime neuron count changes
 *   - Distillation: teacher-student knowledge transfer
 *   - Quantization: INT8/FP16/INT4 weight compression
 *   - Federated learning: gradient aggregation across devices
 *   - Swarm communication: peer mesh + master link
 *   - Power management: battery-aware compute scaling
 *   - Safety: rollback, OTA, offline degradation, versioning
 *   - Privacy: differential privacy on gradient sharing
 *   - Early exit: adaptive computation depth
 *   - EWC: catastrophic forgetting protection
 *   - Gossip: peer-to-peer weight transfer
 *   - Telemetry: device health monitoring
 */

#include "edge/nimcp_edge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Default Configurations
 * ============================================================================ */

nimcp_resize_config_t nimcp_resize_config_default(void);
nimcp_distill_config_t nimcp_distill_config_default(void);
nimcp_quantize_config_t nimcp_quantize_config_default(void);
nimcp_federated_config_t nimcp_federated_config_default(void);
nimcp_power_config_t nimcp_power_config_default(void);
nimcp_offline_policy_t nimcp_offline_policy_default(void);
nimcp_gossip_config_t nimcp_gossip_config_default(void);
nimcp_edge_dp_config_t nimcp_dp_config_default(void);
nimcp_device_profile_t nimcp_device_profile_default(void);

/* ============================================================================
 * Device Profile & Auto-Optimization
 * ============================================================================ */

/**
 * @brief Analyze device profile and produce an optimized child brain.
 * The master selects neuron count, subsystems, quantization, and distills.
 */
int nimcp_brain_optimize_for_device(
    nimcp_brain_t master,
    const nimcp_device_profile_t* device,
    nimcp_brain_t* child,
    nimcp_optimization_report_t* report);

/**
 * @brief Compute optimal neuron count for a device profile.
 */
uint32_t nimcp_compute_optimal_neurons(const nimcp_device_profile_t* device);

/**
 * @brief Compute subsystem mask for a device profile.
 * Returns bitmask of subsystems to enable.
 */
uint32_t nimcp_compute_subsystem_mask(const nimcp_device_profile_t* device);

/* ============================================================================
 * Brain Resize (Expand / Contract)
 * ============================================================================ */

/**
 * @brief Resize a brain's neural network at runtime.
 * Thread-safe: acquires network mutex.
 */
int nimcp_edge_brain_resize(nimcp_brain_t brain, const nimcp_resize_config_t* config);

/**
 * @brief Query resize feasibility without executing (dry run).
 */
int nimcp_edge_brain_resize_check(
    nimcp_brain_t brain,
    const nimcp_resize_config_t* config,
    nimcp_resize_report_t* report);

/**
 * @brief Score neuron importance (activity, connectivity, weight magnitude, uniqueness).
 * @param scores Output array of size num_neurons.
 */
int nimcp_edge_score_neuron_importance(
    nimcp_brain_t brain,
    float* scores,
    uint32_t num_neurons);

/* ============================================================================
 * Maturation Tracking
 * ============================================================================ */

nimcp_maturation_tracker_t* nimcp_maturation_create(
    uint32_t capacity, uint32_t maturation_steps, float existing_lr_scale);

void nimcp_maturation_destroy(nimcp_maturation_tracker_t* tracker);

int nimcp_maturation_add_neuron(nimcp_maturation_tracker_t* tracker, uint32_t neuron_id);

/**
 * @brief Advance maturation by one step. Updates stages and output scales.
 */
int nimcp_maturation_step(nimcp_maturation_tracker_t* tracker);

float nimcp_maturation_get_progress(const nimcp_maturation_tracker_t* tracker);
float nimcp_maturation_get_output_scale(const nimcp_maturation_tracker_t* tracker, uint32_t neuron_id);
float nimcp_maturation_get_lr_scale(const nimcp_maturation_tracker_t* tracker);

/* ============================================================================
 * Knowledge Distillation
 * ============================================================================ */

/**
 * @brief Distill a trained master brain into a smaller student brain.
 */
int nimcp_brain_distill(
    nimcp_brain_t teacher,
    nimcp_brain_t* student,
    const nimcp_distill_config_t* config,
    nimcp_distill_report_t* report);

/**
 * @brief Batch distill: create N optimized child brains.
 */
int nimcp_brain_distill_batch(
    nimcp_brain_t teacher,
    nimcp_brain_t* students,
    const nimcp_distill_config_t* configs,
    nimcp_distill_report_t* reports,
    uint32_t count);

/* ============================================================================
 * Quantization
 * ============================================================================ */

/**
 * @brief Quantize a brain's weights in-place.
 */
int nimcp_brain_quantize(nimcp_brain_t brain, const nimcp_quantize_config_t* config);

/**
 * @brief Create a quantized tensor from FP32 data.
 */
nimcp_quantized_tensor_t* nimcp_quantize_tensor(
    const float* data, uint32_t num_elements,
    nimcp_quantization_t precision, const float* calibration_min, const float* calibration_max);

void nimcp_quantized_tensor_destroy(nimcp_quantized_tensor_t* qt);

/**
 * @brief Dequantize INT8 tensor back to FP32.
 */
int nimcp_dequantize_tensor(const nimcp_quantized_tensor_t* qt, float* output);

/* ============================================================================
 * Delta Weight Pushes
 * ============================================================================ */

int nimcp_weight_delta_compute(
    const float* old_weights, const float* new_weights,
    uint32_t num_weights, float sparsity_threshold,
    nimcp_weight_delta_t* delta);

int nimcp_weight_delta_apply(float* weights, const nimcp_weight_delta_t* delta);

int nimcp_weight_delta_compress(nimcp_weight_delta_t* delta);
int nimcp_weight_delta_decompress(nimcp_weight_delta_t* delta);

void nimcp_weight_delta_destroy(nimcp_weight_delta_t* delta);

/* ============================================================================
 * Model Versioning
 * ============================================================================ */

nimcp_model_version_t nimcp_version_create(uint32_t major, uint32_t minor, uint32_t patch,
                                            const uint32_t* layer_sizes, uint32_t num_layers);

int nimcp_version_check_compatibility(
    const nimcp_model_version_t* device,
    const nimcp_model_version_t* master,
    nimcp_compatibility_result_t* result);

uint32_t nimcp_version_compute_arch_hash(const uint32_t* layer_sizes, uint32_t num_layers);

/* ============================================================================
 * Rollback Safety
 * ============================================================================ */

int nimcp_rollback_init(nimcp_rollback_state_t* state, const float* current_weights,
                         uint32_t num_weights, float baseline_loss,
                         uint32_t validation_steps, float threshold);

int nimcp_rollback_check_step(nimcp_rollback_state_t* state, float step_loss);

int nimcp_rollback_execute(nimcp_rollback_state_t* state, float* weights);

void nimcp_rollback_cleanup(nimcp_rollback_state_t* state);

/* ============================================================================
 * Early Exit
 * ============================================================================ */

nimcp_early_exit_t* nimcp_early_exit_create(
    const uint32_t* exit_layers, const float* thresholds,
    const uint32_t* layer_sizes, uint32_t num_exits, uint32_t output_size);

void nimcp_early_exit_destroy(nimcp_early_exit_t* ee);

/**
 * @brief Evaluate exit head at given layer. Returns confidence.
 * If confidence > threshold, writes output to result and returns the exit index.
 * Returns -1 if no early exit triggered.
 */
int nimcp_early_exit_evaluate(
    nimcp_early_exit_t* ee, uint32_t exit_idx,
    const float* layer_activation, uint32_t layer_size,
    float* output, float* confidence);

void nimcp_early_exit_get_stats(const nimcp_early_exit_t* ee,
                                 uint64_t* total, uint64_t* early, uint64_t* full);

/* ============================================================================
 * Offline Degradation Policy
 * ============================================================================ */

int nimcp_offline_policy_init(nimcp_offline_policy_t* policy);

int nimcp_offline_policy_step(nimcp_offline_policy_t* policy);

void nimcp_offline_policy_on_sync(nimcp_offline_policy_t* policy);

float nimcp_offline_get_confidence(const nimcp_offline_policy_t* policy);

float nimcp_offline_get_lr_scale(const nimcp_offline_policy_t* policy);

/* ============================================================================
 * Gossip Learning
 * ============================================================================ */

int nimcp_gossip_init(nimcp_gossip_config_t* config);

nimcp_gossip_update_t* nimcp_gossip_create_update(
    uint32_t sender_id, const float* old_weights, const float* new_weights,
    uint32_t num_weights, float loss, float ema_loss);

int nimcp_gossip_apply_update(
    float* local_weights, const nimcp_gossip_update_t* update,
    const nimcp_gossip_config_t* config);

bool nimcp_gossip_should_broadcast(float current_loss, float ema_loss,
                                    const nimcp_gossip_config_t* config);

bool nimcp_gossip_is_seen(const nimcp_gossip_config_t* config, uint32_t hash);
void nimcp_gossip_mark_seen(nimcp_gossip_config_t* config, uint32_t hash);

void nimcp_gossip_update_destroy(nimcp_gossip_update_t* update);

/* ============================================================================
 * OTA Update Safety
 * ============================================================================ */

int nimcp_ota_init(nimcp_ota_state_t* state);

int nimcp_ota_stage_weights(nimcp_ota_state_t* state,
                             const float* weights, uint32_t count,
                             const uint8_t* checksum);

int nimcp_ota_validate(nimcp_ota_state_t* state,
                        const float** test_inputs, const float** expected_outputs,
                        uint32_t num_tests, uint32_t input_size, uint32_t output_size,
                        float tolerance);

bool nimcp_ota_is_safe_to_swap(float threat_level, bool motor_active,
                                bool inference_in_progress, float battery_pct);

int nimcp_ota_swap(nimcp_ota_state_t* state, float* active_weights);

void nimcp_ota_cleanup(nimcp_ota_state_t* state);

/* ============================================================================
 * Elastic Weight Consolidation (EWC)
 * ============================================================================ */

nimcp_ewc_state_t* nimcp_ewc_create(uint32_t num_params, float lambda);

void nimcp_ewc_destroy(nimcp_ewc_state_t* ewc);

/**
 * @brief Compute Fisher Information diagonal from recent gradients.
 * Call after local training, before accepting master weights.
 */
int nimcp_ewc_compute_fisher(nimcp_ewc_state_t* ewc,
                              const float* gradients, uint32_t num_samples);

/**
 * @brief Set anchor weights (current local weights before master push).
 */
int nimcp_ewc_set_anchor(nimcp_ewc_state_t* ewc, const float* weights);

/**
 * @brief Apply EWC-aware weight blending.
 * Protects locally-important weights from master overwrites.
 */
int nimcp_ewc_blend_weights(const nimcp_ewc_state_t* ewc,
                             float* local_weights, const float* master_weights,
                             float base_blend_ratio);

/* ============================================================================
 * Power Management
 * ============================================================================ */

int nimcp_power_init(nimcp_power_config_t* config);

nimcp_power_mode_t nimcp_power_update(nimcp_power_config_t* config,
                                       float battery_pct, float temperature_c);

float nimcp_power_get_inference_hz(const nimcp_power_config_t* config);
float nimcp_power_get_lr_scale(const nimcp_power_config_t* config);
bool nimcp_power_is_gpu_enabled(const nimcp_power_config_t* config);

/* ============================================================================
 * Device Telemetry
 * ============================================================================ */

int nimcp_telemetry_collect(const nimcp_edge_ctx_t* ctx, nimcp_device_telemetry_t* telemetry);

int nimcp_telemetry_serialize(const nimcp_device_telemetry_t* telemetry,
                               uint8_t* buffer, uint32_t buffer_size, uint32_t* bytes_written);

int nimcp_telemetry_deserialize(const uint8_t* buffer, uint32_t size,
                                 nimcp_device_telemetry_t* telemetry);

/**
 * @brief Master-side: analyze telemetry and generate directives.
 * Returns bitmask of recommended actions.
 */
uint32_t nimcp_telemetry_analyze(const nimcp_device_telemetry_t* telemetry);

#define NIMCP_TELEMETRY_ACTION_NONE          0x00
#define NIMCP_TELEMETRY_ACTION_REDISTILL     0x01
#define NIMCP_TELEMETRY_ACTION_REDUCE_COMPUTE 0x02
#define NIMCP_TELEMETRY_ACTION_POWER_SAVE    0x04
#define NIMCP_TELEMETRY_ACTION_ALERT_ANOMALY 0x08
#define NIMCP_TELEMETRY_ACTION_STOP_UPDATES  0x10
#define NIMCP_TELEMETRY_ACTION_INVESTIGATE   0x20

/* ============================================================================
 * Differential Privacy
 * ============================================================================ */

int nimcp_edge_dp_init(nimcp_edge_dp_config_t* config);

/**
 * @brief Apply differential privacy to gradients before sharing.
 * Clips gradient norm, adds calibrated Gaussian noise, tracks budget.
 */
int nimcp_edge_dp_privatize_gradients(nimcp_edge_dp_config_t* config,
                                  float* gradients, uint32_t num_params);

bool nimcp_edge_dp_budget_exhausted(const nimcp_edge_dp_config_t* config);

/* ============================================================================
 * Swarm Communication
 * ============================================================================ */

nimcp_swarm_transport_t* nimcp_swarm_transport_create(
    uint32_t device_id, const char* multicast_group, uint16_t peer_port,
    const char* master_host, uint16_t master_port);

void nimcp_swarm_transport_destroy(nimcp_swarm_transport_t* transport);

int nimcp_swarm_send_peer(nimcp_swarm_transport_t* transport,
                           const nimcp_swarm_message_t* msg);

int nimcp_swarm_send_master(nimcp_swarm_transport_t* transport,
                             const nimcp_swarm_message_t* msg);

int nimcp_swarm_recv(nimcp_swarm_transport_t* transport,
                      nimcp_swarm_message_t* msg, uint32_t timeout_ms);

int nimcp_swarm_send_heartbeat(nimcp_swarm_transport_t* transport,
                                const float* position);

nimcp_swarm_message_t* nimcp_swarm_message_create(
    nimcp_swarm_msg_type_t type, uint32_t sender_id,
    uint32_t recipient_id, const void* payload, uint32_t payload_size);

void nimcp_swarm_message_destroy(nimcp_swarm_message_t* msg);

/* ============================================================================
 * Federated Learning (Master-side aggregation)
 * ============================================================================ */

/**
 * @brief Aggregate gradients from multiple devices.
 * @param gradients Array of per-device gradient reports
 * @param count Number of devices
 * @param aggregated Output: averaged gradient (must be pre-allocated)
 */
int nimcp_federated_aggregate(
    const nimcp_federated_gradient_t* gradients, uint32_t count,
    float* aggregated, uint32_t num_params,
    nimcp_fed_aggregation_t method);

/**
 * @brief Blend master weights into device weights.
 * Uses EWC if available, otherwise uniform blend.
 */
int nimcp_federated_blend(
    float* device_weights, const float* master_weights,
    uint32_t num_params, float blend_ratio,
    const nimcp_ewc_state_t* ewc);

/* ============================================================================
 * Edge Context Lifecycle
 * ============================================================================ */

nimcp_edge_ctx_t* nimcp_edge_ctx_create(const nimcp_device_profile_t* profile);
void nimcp_edge_ctx_destroy(nimcp_edge_ctx_t* ctx);

/**
 * @brief Record an inference result for telemetry.
 */
void nimcp_edge_record_inference(nimcp_edge_ctx_t* ctx, float inference_ms, float loss);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EDGE_H */
