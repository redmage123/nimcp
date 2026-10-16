/**
 * @file nimcp_edge_config.c
 * @brief Default configurations for edge brain subsystems.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include <math.h>
#include <string.h>

/* ============================================================================
 * Default Config Constructors
 * ============================================================================ */

nimcp_resize_config_t nimcp_resize_config_default(void) {
    nimcp_resize_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.mode = NIMCP_RESIZE_EXPAND;
    cfg.target_neuron_count = 0;

    /* Expansion defaults */
    cfg.initial_weight_scale = 0.01f;
    cfg.maturation_steps = 500;
    cfg.wiring = NIMCP_WIRE_ACTIVITY;
    cfg.fan_in_target = 128;
    cfg.existing_lr_scale = 0.5f;

    /* Contraction defaults */
    cfg.compaction = NIMCP_COMPACT_LAZY;
    cfg.enable_knowledge_transfer = true;
    cfg.fadeout_steps = 200;
    cfg.min_importance_threshold = 0.01f;

    /* Common */
    cfg.preserve_io_layers = true;
    cfg.rebuild_gpu_cache = true;
    cfg.diamond_ratio_tolerance = 0.1f;

    return cfg;
}

nimcp_distill_config_t nimcp_distill_config_default(void) {
    nimcp_distill_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.target_neurons = 50000;
    cfg.importance_threshold = 0.05f;
    cfg.distillation_steps = 5000;
    cfg.temperature = 3.0f;
    cfg.preserve_specialists = true;

    cfg.include_snn = true;
    cfg.include_lnn = true;
    cfg.include_cnn = true;

    cfg.num_layers = 0;    /* auto (diamond) */
    cfg.layer_sizes = NULL;

    cfg.quantization = nimcp_quantize_config_default();

    return cfg;
}

nimcp_quantize_config_t nimcp_quantize_config_default(void) {
    nimcp_quantize_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.weight_precision = NIMCP_QUANT_INT8_SYMMETRIC;
    cfg.activation_precision = NIMCP_QUANT_NONE;
    cfg.calibrate = true;
    cfg.calibration_samples = 256;
    cfg.accuracy_threshold = 0.95f;

    return cfg;
}

nimcp_federated_config_t nimcp_federated_config_default(void) {
    nimcp_federated_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.aggregation = NIMCP_FED_AVG;
    cfg.blend_ratio = 0.7f;
    cfg.sync_interval_steps = 100;
    cfg.min_devices_for_agg = 2;
    cfg.enable_ewc = true;
    cfg.dp = nimcp_dp_config_default();
    cfg.gossip = nimcp_gossip_config_default();

    return cfg;
}

nimcp_power_config_t nimcp_power_config_default(void) {
    nimcp_power_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.mode = NIMCP_POWER_FULL;
    cfg.inference_hz = 30.0f;
    cfg.learning_rate_scale = 1.0f;
    cfg.subsystem_mask = 0xFFFFFFFF;
    cfg.early_exit_forced = false;
    cfg.early_exit_threshold = 0.9f;
    cfg.gpu_enabled = true;
    cfg.auto_manage = true;

    cfg.balanced_battery_pct = 80.0f;
    cfg.saving_battery_pct = 50.0f;
    cfg.critical_battery_pct = 20.0f;
    cfg.thermal_throttle_c = 80.0f;

    return cfg;
}

nimcp_offline_policy_t nimcp_offline_policy_default(void) {
    nimcp_offline_policy_t policy;
    memset(&policy, 0, sizeof(policy));

    policy.last_sync_timestamp = 0;
    policy.steps_since_sync = 0;
    policy.confidence_decay_rate = 0.9999f;
    policy.min_confidence_multiplier = 0.5f;
    policy.current_confidence = 1.0f;

    policy.cautious_after_steps = 1000;
    policy.conservative_after_steps = 5000;
    policy.frozen_after_steps = 20000;
    policy.current_mode = NIMCP_OFFLINE_NORMAL;

    return policy;
}

nimcp_gossip_config_t nimcp_gossip_config_default(void) {
    nimcp_gossip_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.gossip_blend_ratio = 0.1f;
    cfg.urgency_threshold = 0.5f;
    cfg.max_ttl = 3;
    cfg.broadcast_loss_ratio = 1.5f;
    cfg.rate_limit_ms = 1000;
    cfg.seen_hashes = NULL;
    cfg.seen_hash_count = 0;
    cfg.seen_hash_capacity = 0;

    return cfg;
}

nimcp_edge_dp_config_t nimcp_dp_config_default(void) {
    nimcp_edge_dp_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.noise_scale = 0.01f;
    cfg.gradient_clip_norm = 1.0f;
    cfg.privacy_budget_epsilon = 10.0f;
    cfg.privacy_spent = 0.0f;
    cfg.enabled = false;

    return cfg;
}

nimcp_device_profile_t nimcp_device_profile_default(void) {
    nimcp_device_profile_t prof;
    memset(&prof, 0, sizeof(prof));

    prof.ram_mb = 512;
    prof.vram_mb = 0;
    prof.cpu_cores = 4;
    prof.cpu_gflops = 10.0f;
    prof.storage_mb = 1024;
    prof.power_budget_watts = 5.0f;
    prof.target_inference_ms = 20.0f;
    prof.target_hz = 30.0f;

    prof.has_gpu = false;
    prof.has_camera = false;
    prof.has_microphone = false;
    prof.has_imu = false;
    prof.has_motor_control = false;
    prof.has_network = true;
    prof.has_persistent_storage = true;

    prof.role = NIMCP_DEVICE_GENERAL;
    strncpy(prof.device_name, "generic_edge", sizeof(prof.device_name) - 1);

    return prof;
}

/* ============================================================================
 * Device Profile Analysis
 * ============================================================================ */

uint32_t nimcp_compute_optimal_neurons(const nimcp_device_profile_t* device) {
    if (!device) {
        return 0;
    }

    /* Edge devices run the ANN teacher (default 150K neurons).
     * Cap scales with available resources, hard ceiling at 500K
     * (edge devices shouldn't need more — the SNN is the primary learner). */
    #define EDGE_MAX_ANN_NEURONS 500000
    uint32_t max_neurons = EDGE_MAX_ANN_NEURONS;
    uint64_t by_ram_64 = (uint64_t)device->ram_mb * 150;
    uint32_t by_ram = (by_ram_64 > max_neurons) ? max_neurons : (uint32_t)by_ram_64;
    uint32_t by_latency = (uint32_t)fminf(device->target_inference_ms * 50000.0f, (float)max_neurons);
    uint32_t cap = max_neurons;

    uint32_t neurons = by_ram;
    if (by_latency < neurons) {
        neurons = by_latency;
    }
    if (cap < neurons) {
        neurons = cap;
    }

    /* Floor at 1000 neurons minimum for any useful brain */
    if (neurons < 1000) {
        neurons = 1000;
    }

    return neurons;
}

uint32_t nimcp_compute_subsystem_mask(const nimcp_device_profile_t* device) {
    if (!device) {
        return 0;
    }

    /*
     * Bitmask layout (example):
     *   bit 0: core neural net
     *   bit 1: SNN (spiking)
     *   bit 2: LNN (liquid/temporal)
     *   bit 3: CNN (visual/audio)
     *   bit 4: plasticity
     *   bit 5: neuromodulation
     *   bit 6: introspection
     *   bit 7: ethics
     *   bit 8: imagination
     *   bit 9: theory of mind
     *   bit 10: emotions
     *   bit 11: reasoning
     */
    uint32_t mask = 0x01; /* Core neural net always on */

    /* Enable SNN if enough RAM (>256MB) */
    if (device->ram_mb >= 256) {
        mask |= 0x02;
    }

    /* Enable LNN if enough compute */
    if (device->ram_mb >= 512 && device->cpu_gflops >= 5.0f) {
        mask |= 0x04;
    }

    /* Enable CNN if camera or microphone present and enough resources */
    if ((device->has_camera || device->has_microphone) && device->ram_mb >= 512) {
        mask |= 0x08;
    }

    /* Enable plasticity for devices with enough RAM */
    if (device->ram_mb >= 128) {
        mask |= 0x10;
    }

    /* Enable neuromodulation for devices with enough compute */
    if (device->cpu_gflops >= 3.0f) {
        mask |= 0x20;
    }

    /* Higher cognitive functions need significant resources */
    if (device->ram_mb >= 1024 && device->cpu_gflops >= 10.0f) {
        mask |= 0x40;  /* introspection */
        mask |= 0x80;  /* ethics */
    }

    if (device->ram_mb >= 2048 && device->cpu_gflops >= 20.0f) {
        mask |= 0x100; /* imagination */
        mask |= 0x200; /* theory of mind */
        mask |= 0x400; /* emotions */
        mask |= 0x800; /* reasoning */
    }

    return mask;
}
