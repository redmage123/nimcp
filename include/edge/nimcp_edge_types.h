#ifndef NIMCP_EDGE_TYPES_H
#define NIMCP_EDGE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct nimcp_brain_handle* nimcp_brain_t;
typedef struct neural_network_struct* neural_network_t;

/* ============================================================================
 * Device Profile — describes target platform constraints
 * ============================================================================ */

typedef enum {
    NIMCP_DEVICE_SENSOR,       /* Observe and report (IoT, camera) */
    NIMCP_DEVICE_ACTUATOR,     /* Act on environment (robot, drone) */
    NIMCP_DEVICE_COORDINATOR,  /* Lead a group (swarm leader, hub) */
    NIMCP_DEVICE_GENERAL       /* General purpose (phone, tablet) */
} nimcp_device_role_t;

typedef struct {
    /* Hardware constraints */
    uint32_t ram_mb;
    uint32_t vram_mb;              /* 0 = CPU only */
    uint32_t cpu_cores;
    float cpu_gflops;
    uint32_t storage_mb;
    float power_budget_watts;      /* 0 = unlimited */
    float target_inference_ms;
    float target_hz;               /* Decision frequency */

    /* Capabilities */
    bool has_gpu;
    bool has_camera;
    bool has_microphone;
    bool has_imu;
    bool has_motor_control;
    bool has_network;
    bool has_persistent_storage;

    /* Mission profile */
    nimcp_device_role_t role;
    char device_name[64];
} nimcp_device_profile_t;

/* ============================================================================
 * Quantization
 * ============================================================================ */

typedef enum {
    NIMCP_QUANT_NONE = 0,       /* FP32 */
    NIMCP_QUANT_FP16,           /* Half precision */
    NIMCP_QUANT_INT8_SYMMETRIC, /* Per-tensor symmetric */
    NIMCP_QUANT_INT8_AFFINE,    /* Per-channel asymmetric (best accuracy) */
    NIMCP_QUANT_INT4,           /* 4-bit (8x compression) */
    NIMCP_QUANT_TERNARY         /* {-1, 0, +1} with scale */
} nimcp_quantization_t;

typedef struct {
    nimcp_quantization_t weight_precision;
    nimcp_quantization_t activation_precision;
    bool calibrate;
    uint32_t calibration_samples;
    float accuracy_threshold;     /* Abort if accuracy drops below */
} nimcp_quantize_config_t;

typedef struct {
    float scale;
    int32_t zero_point;
    float min_val;
    float max_val;
} nimcp_quant_params_t;

typedef struct {
    int8_t* data;
    nimcp_quant_params_t* per_channel_params; /* NULL for per-tensor */
    uint32_t num_channels;
    uint32_t num_elements;
    nimcp_quantization_t precision;
} nimcp_quantized_tensor_t;

/* ============================================================================
 * Resize (Expand / Contract)
 * ============================================================================ */

typedef enum {
    NIMCP_RESIZE_EXPAND,
    NIMCP_RESIZE_CONTRACT,
    NIMCP_RESIZE_REBALANCE
} nimcp_resize_mode_t;

typedef enum {
    NIMCP_WIRE_RANDOM,
    NIMCP_WIRE_ACTIVITY,
    NIMCP_WIRE_NEIGHBOR
} nimcp_wiring_strategy_t;

typedef enum {
    NIMCP_COMPACT_LAZY,
    NIMCP_COMPACT_EAGER
} nimcp_compact_mode_t;

typedef struct {
    nimcp_resize_mode_t mode;
    uint32_t target_neuron_count;

    /* Expansion */
    float initial_weight_scale;
    uint32_t maturation_steps;
    nimcp_wiring_strategy_t wiring;
    uint32_t fan_in_target;
    float existing_lr_scale;

    /* Contraction */
    nimcp_compact_mode_t compaction;
    bool enable_knowledge_transfer;
    uint32_t fadeout_steps;
    float min_importance_threshold;

    /* Common */
    bool preserve_io_layers;
    bool rebuild_gpu_cache;
    float diamond_ratio_tolerance;
} nimcp_resize_config_t;

typedef struct {
    uint32_t neurons_before;
    uint32_t neurons_after;
    uint32_t layers_affected;
    float estimated_ram_delta_mb;
    float estimated_vram_delta_mb;
    bool feasible;
    char reason[256];
} nimcp_resize_report_t;

/* ============================================================================
 * Maturation (for newly added neurons)
 * ============================================================================ */

typedef enum {
    NIMCP_MATURATION_PROGENITOR = 0,   /* Silent, accumulating stats */
    NIMCP_MATURATION_IMMATURE,         /* Partially active, 0.5x LR */
    NIMCP_MATURATION_INTEGRATING,      /* Ramping up to full */
    NIMCP_MATURATION_MATURE            /* Fully integrated */
} nimcp_maturation_stage_t;

typedef struct {
    uint32_t neuron_id;
    nimcp_maturation_stage_t stage;
    float maturity;                    /* 0.0 - 1.0 */
    uint32_t steps_in_stage;
    uint32_t connections_formed;
    float output_scale;                /* 0.0 (silent) → 1.0 (full) */
} nimcp_neuron_maturation_t;

typedef struct {
    nimcp_neuron_maturation_t* neurons;
    uint32_t count;
    uint32_t capacity;
    uint32_t maturation_steps;         /* Total steps to full maturity */
    float existing_lr_scale;           /* LR scale for old neurons during maturation */
} nimcp_maturation_tracker_t;

/* ============================================================================
 * Knowledge Distillation
 * ============================================================================ */

typedef struct {
    uint32_t target_neurons;
    float importance_threshold;
    uint32_t distillation_steps;
    float temperature;
    bool preserve_specialists;

    bool include_snn;
    bool include_lnn;
    bool include_cnn;

    uint32_t num_layers;         /* 0 = auto (diamond) */
    uint32_t* layer_sizes;       /* NULL = auto */

    nimcp_quantize_config_t quantization;
} nimcp_distill_config_t;

typedef struct {
    float accuracy_retention;    /* % of teacher accuracy retained */
    uint32_t neurons_selected;
    uint32_t neurons_pruned;
    float teacher_loss;
    float student_loss;
    uint32_t steps_trained;
    float compression_ratio;     /* teacher_params / student_params */
} nimcp_distill_report_t;

/* ============================================================================
 * Delta Weight Push
 * ============================================================================ */

typedef struct {
    uint32_t version_from;
    uint32_t version_to;
    uint32_t num_changes;
    uint32_t* layer_indices;
    uint32_t* neuron_indices;
    float* weight_deltas;
    uint32_t compressed_size;
    uint8_t* compressed_data;    /* LZ4-compressed payload */
} nimcp_weight_delta_t;

/* ============================================================================
 * Model Versioning
 * ============================================================================ */

typedef struct {
    uint32_t major;              /* Architecture change — incompatible */
    uint32_t minor;              /* Weight update — delta-compatible */
    uint32_t patch;              /* Config change — compatible */
    uint32_t arch_hash;          /* Hash of layer_sizes */
} nimcp_model_version_t;

typedef struct {
    nimcp_model_version_t device_version;
    nimcp_model_version_t master_version;
    bool architecturally_compatible;
    bool delta_compatible;
    char migration_path[64];     /* "re-distill", "delta", "none" */
} nimcp_compatibility_result_t;

/* ============================================================================
 * Rollback
 * ============================================================================ */

typedef struct {
    float* previous_weights;
    uint32_t num_weights;
    uint32_t previous_version;
    float baseline_loss;
    uint32_t validation_steps;
    float rollback_threshold;    /* Rollback if loss > baseline × this */
    uint32_t steps_evaluated;
    float running_loss;
    bool rollback_triggered;
    bool active;                 /* Whether a rollback buffer is held */
} nimcp_rollback_state_t;

/* ============================================================================
 * Early Exit
 * ============================================================================ */

typedef struct {
    uint32_t num_exits;
    uint32_t exit_layers[8];             /* Max 8 exit points */
    float confidence_thresholds[8];
    float* exit_weights[8];              /* Per-exit projection: layer_size → output_size */
    float* exit_biases[8];
    uint32_t output_size;
    bool enabled;

    /* Stats */
    uint64_t total_inferences;
    uint64_t early_exits[8];             /* Count per exit point */
    uint64_t full_depth_count;
} nimcp_early_exit_t;

/* ============================================================================
 * Offline Degradation Policy
 * ============================================================================ */

typedef enum {
    NIMCP_OFFLINE_NORMAL = 0,
    NIMCP_OFFLINE_CAUTIOUS,
    NIMCP_OFFLINE_CONSERVATIVE,
    NIMCP_OFFLINE_FROZEN
} nimcp_offline_mode_t;

typedef struct {
    uint64_t last_sync_timestamp;
    uint32_t steps_since_sync;
    float confidence_decay_rate;
    float min_confidence_multiplier;
    float current_confidence;

    uint32_t cautious_after_steps;
    uint32_t conservative_after_steps;
    uint32_t frozen_after_steps;
    nimcp_offline_mode_t current_mode;
} nimcp_offline_policy_t;

/* ============================================================================
 * Gossip Learning (Peer-to-Peer)
 * ============================================================================ */

typedef struct {
    uint32_t sender_id;
    uint32_t experience_hash;
    float urgency;
    uint32_t num_weights;
    uint32_t* weight_indices;
    float* weight_deltas;
    float sender_confidence;
    uint32_t ttl;                /* Time-to-live (hops remaining) */
} nimcp_gossip_update_t;

typedef struct {
    float gossip_blend_ratio;    /* How much to trust peer updates (default 0.1) */
    float urgency_threshold;     /* Min urgency to apply immediately */
    uint32_t max_ttl;            /* Max propagation hops (default 3) */
    float broadcast_loss_ratio;  /* Broadcast if loss > ema × this */
    uint32_t rate_limit_ms;      /* Min ms between broadcasts */
    uint32_t* seen_hashes;       /* Ring buffer of recent experience hashes */
    uint32_t seen_hash_count;
    uint32_t seen_hash_capacity;
} nimcp_gossip_config_t;

/* ============================================================================
 * OTA Update Safety
 * ============================================================================ */

typedef enum {
    NIMCP_OTA_IDLE = 0,
    NIMCP_OTA_DOWNLOADING,
    NIMCP_OTA_VALIDATING,
    NIMCP_OTA_READY_TO_SWAP,
    NIMCP_OTA_SWAPPED,
    NIMCP_OTA_VERIFYING,
    NIMCP_OTA_COMPLETE,
    NIMCP_OTA_FAILED,
    NIMCP_OTA_ROLLED_BACK
} nimcp_ota_stage_t;

typedef struct {
    nimcp_ota_stage_t stage;
    float* staged_weights;
    uint32_t staged_count;
    uint8_t checksum[32];        /* SHA-256 */
    uint32_t test_inputs_passed;
    uint32_t test_inputs_total;
    bool safe_to_swap;
} nimcp_ota_state_t;

/* ============================================================================
 * Elastic Weight Consolidation (EWC)
 * ============================================================================ */

typedef struct {
    float* fisher_diagonal;      /* Per-weight importance */
    float* anchor_weights;       /* Weights at last consolidation */
    float ewc_lambda;            /* Regularization strength */
    uint32_t num_params;
    bool initialized;
} nimcp_ewc_state_t;

/* ============================================================================
 * Power Management
 * ============================================================================ */

typedef enum {
    NIMCP_POWER_FULL = 0,
    NIMCP_POWER_BALANCED,
    NIMCP_POWER_SAVING,
    NIMCP_POWER_CRITICAL
} nimcp_power_mode_t;

typedef struct {
    nimcp_power_mode_t mode;
    float inference_hz;
    float learning_rate_scale;
    uint32_t subsystem_mask;
    bool early_exit_forced;
    float early_exit_threshold;
    bool gpu_enabled;
    bool auto_manage;            /* Auto-adjust based on battery/temp */

    /* Thresholds for auto mode */
    float balanced_battery_pct;  /* Enter BALANCED below this (default 80) */
    float saving_battery_pct;    /* Enter SAVING below this (default 50) */
    float critical_battery_pct;  /* Enter CRITICAL below this (default 20) */
    float thermal_throttle_c;    /* Throttle above this temp */
} nimcp_power_config_t;

/* ============================================================================
 * Device Telemetry
 * ============================================================================ */

typedef struct {
    uint32_t device_id;
    uint64_t timestamp;

    /* Performance */
    float avg_inference_ms;
    float p99_inference_ms;
    float avg_loss;
    float loss_trend;

    /* Confidence */
    float avg_confidence;
    float low_confidence_pct;
    float anomaly_rate;

    /* Resources */
    float ram_usage_pct;
    float cpu_usage_pct;
    float battery_pct;
    float temperature_c;

    /* Learning */
    uint32_t steps_since_sync;
    float local_accuracy;
    uint32_t rollbacks_triggered;
    nimcp_offline_mode_t offline_mode;
    nimcp_power_mode_t power_mode;

    /* Version */
    nimcp_model_version_t model_version;
} nimcp_device_telemetry_t;

/* ============================================================================
 * Differential Privacy
 * ============================================================================ */

typedef struct {
    float noise_scale;           /* Gaussian noise sigma */
    float gradient_clip_norm;    /* Max gradient L2 norm */
    float privacy_budget_epsilon;
    float privacy_spent;
    bool enabled;
} nimcp_edge_dp_config_t;

/* ============================================================================
 * Federated Learning
 * ============================================================================ */

typedef struct {
    uint32_t device_id;
    uint32_t num_params;
    float* gradients;
    uint32_t local_steps;        /* Steps since last sync */
    nimcp_model_version_t version;
} nimcp_federated_gradient_t;

typedef enum {
    NIMCP_FED_AVG,               /* Simple averaging */
    NIMCP_FED_PROX,              /* Proximal term (prevents drift) */
    NIMCP_FED_MEDIAN             /* Byzantine-tolerant median */
} nimcp_fed_aggregation_t;

typedef struct {
    nimcp_fed_aggregation_t aggregation;
    float blend_ratio;           /* Local weight in blend (default 0.7) */
    uint32_t sync_interval_steps;/* Steps between syncs */
    uint32_t min_devices_for_agg;/* Min devices before aggregating */
    bool enable_ewc;
    nimcp_edge_dp_config_t dp;
    nimcp_gossip_config_t gossip;
} nimcp_federated_config_t;

/* ============================================================================
 * Swarm Communication
 * ============================================================================ */

typedef enum {
    NIMCP_SWARM_MSG_PERCEPT,
    NIMCP_SWARM_MSG_THREAT,
    NIMCP_SWARM_MSG_MAP_UPDATE,
    NIMCP_SWARM_MSG_TASK_CLAIM,
    NIMCP_SWARM_MSG_TASK_COMPLETE,
    NIMCP_SWARM_MSG_TASK_FAIL,
    NIMCP_SWARM_MSG_HEARTBEAT,
    NIMCP_SWARM_MSG_GRADIENT,
    NIMCP_SWARM_MSG_EXPERIENCE,
    NIMCP_SWARM_MSG_MODEL_SYNC,
    NIMCP_SWARM_MSG_REPORT,
    NIMCP_SWARM_MSG_DIRECTIVE,
    NIMCP_SWARM_MSG_WEIGHT_PUSH,
    NIMCP_SWARM_MSG_GOSSIP_UPDATE,
    NIMCP_SWARM_MSG_SENSORY_STREAM   /* Device → master: raw sensory experience */
} nimcp_swarm_msg_type_t;

typedef struct {
    nimcp_swarm_msg_type_t type;
    uint32_t sender_id;
    uint32_t recipient_id;       /* 0 = broadcast */
    uint64_t timestamp;
    uint32_t payload_size;
    uint8_t* payload;
    uint8_t checksum[32];
} nimcp_swarm_message_t;

typedef struct {
    uint32_t device_id;
    float position[3];
    float confidence;
    uint32_t object_class;
    float feature_vector[64];
    uint64_t timestamp;
} nimcp_swarm_percept_t;

typedef struct {
    int socket_fd;               /* UDP multicast for peers */
    int master_fd;               /* TCP to master */
    uint32_t device_id;
    char multicast_group[64];
    uint16_t peer_port;
    char master_host[256];
    uint16_t master_port;
    bool connected_to_master;
    bool peer_mesh_active;
    uint64_t last_heartbeat_sent;
    uint32_t heartbeat_interval_ms;
} nimcp_swarm_transport_t;

/* ============================================================================
 * Master Optimization Report
 * ============================================================================ */

typedef struct {
    uint32_t neuron_count;
    uint32_t subsystems_enabled;
    uint32_t subsystems_disabled;
    float estimated_ram_mb;
    float estimated_inference_ms;
    float accuracy_retention;
    char warnings[16][128];
    uint32_t num_warnings;
    nimcp_quantization_t quantization_used;
    nimcp_model_version_t version;
} nimcp_optimization_report_t;

/* ============================================================================
 * Edge Brain Context (per-device runtime state)
 * ============================================================================ */

typedef struct {
    nimcp_device_profile_t profile;
    nimcp_model_version_t version;
    nimcp_rollback_state_t rollback;
    nimcp_early_exit_t* early_exit;
    nimcp_offline_policy_t offline;
    nimcp_power_config_t power;
    nimcp_ewc_state_t* ewc;
    nimcp_maturation_tracker_t* maturation;
    nimcp_gossip_config_t gossip;
    nimcp_ota_state_t ota;
    nimcp_swarm_transport_t* transport;
    nimcp_edge_dp_config_t dp;

    /* Telemetry accumulation */
    float inference_times[100];
    uint32_t inference_time_idx;
    float recent_losses[100];
    uint32_t recent_loss_idx;
    uint64_t total_steps;
} nimcp_edge_ctx_t;

#endif /* NIMCP_EDGE_TYPES_H */
