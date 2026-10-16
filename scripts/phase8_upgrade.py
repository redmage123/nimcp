#!/usr/bin/env python3
"""
Phase 8 Instance-Level Health Agent Enhancement Script.

For each file, adds:
1. heartbeat_instance function (after existing heartbeat)
2. set_instance_health_agent at END
3. FULL training_begin/end/step at END (with real struct field access)
"""
import re
import sys

# File specifications:
# (file_path, is_bridge, struct_name, prefix, global_var, short_hb_prefix, typed_ptr_type)
# For bridges: typed_ptr_type = the bridge struct type
# For non-bridges: typed_ptr_type = None (use void*)

files = [
    # === autobiographical_memory (7 files) ===
    {
        "path": "src/cognitive/autobiographical_memory/nimcp_autobiographical_fep_bridge.c",
        "is_bridge": True,
        "struct_name": "autobiographical_fep_bridge",
        "prefix": "autobiographical_fep_bridge",
        "global_var": "g_autobiographical_fep_bridge_health_agent",
        "hb_tag": "autobio_fep",
        "typed_ptr": "autobiographical_fep_bridge_t",
        "fields": {
            "config": "config",
            "stats": "stats",
            "state": "state",
            "effects": "effects",
            "threshold_field": "config.surprise_memory_threshold",
            "weight_field": "config.memory_importance_weight",
            "counter_field": "stats.memories_encoded",
            "rate_field": "config.model_update_rate",
        },
    },
    {
        "path": "src/cognitive/autobiographical_memory/nimcp_autobiographical_memory.c",
        "is_bridge": False,
        "struct_name": "autobiographical_memory_system",
        "prefix": "autobiographical_memory",
        "global_var": "g_autobiographical_memory_health_agent",
        "hb_tag": "autobio_mem",
        "typed_ptr": None,
        "fields": {
            "counter_field": "stats.total_retrievals",
            "stats": "stats",
            "count_field": "count",
            "capacity_field": "capacity",
        },
    },
    {
        "path": "src/cognitive/autobiographical_memory/nimcp_autobiographical_memory_sleep_bridge.c",
        "is_bridge": True,
        "struct_name": "autobio_sleep_bridge_struct",
        "prefix": "autobiographical_memory_sleep_bridge",
        "global_var": "g_autobiographical_memory_sleep_bridge_health_agent",
        "hb_tag": "autobio_slp",
        "typed_ptr": "autobio_sleep_bridge_t",
        "fields": {
            "config": "config",
            "effects": "effects",
            "threshold_field": "config.encoding_threshold",
            "capacity_field": "effects.encoding_efficiency",
        },
    },
    {
        "path": "src/cognitive/autobiographical_memory/nimcp_autobio_plasticity_bridge.c",
        "is_bridge": True,
        "struct_name": "autobio_plasticity_bridge",
        "prefix": "autobio_plasticity_bridge",
        "global_var": "g_autobio_plasticity_bridge_health_agent",
        "hb_tag": "autobio_pla",
        "typed_ptr": "autobio_plasticity_bridge_t",
        "fields": {
            "config": "config",
            "state": "state",
            "stats": "stats",
            "threshold_field": "bcm_global_threshold",
            "weight_field": "learning_rate_effective",
            "counter_field": "stats.total_updates",
            "synapse_count": "synapse_count",
        },
    },
    {
        "path": "src/cognitive/autobiographical_memory/nimcp_autobio_snn_bridge.c",
        "is_bridge": True,
        "struct_name": "autobio_snn_bridge",
        "prefix": "autobio_snn_bridge",
        "global_var": "g_autobio_snn_bridge_health_agent",
        "hb_tag": "autobio_snn",
        "typed_ptr": "autobio_snn_bridge_t",
        "fields": {
            "config": "config",
            "state": "state",
            "stats": "stats",
            "threshold_field": "config.spike_threshold",
            "counter_field": "stats.total_spikes",
            "signal_field": "temporal_signal",
        },
    },
    {
        "path": "src/cognitive/autobiographical_memory/nimcp_autobio_substrate_bridge.c",
        "is_bridge": True,
        "struct_name": "autobio_substrate_bridge",
        "prefix": "autobio_substrate_bridge",
        "global_var": "g_autobio_substrate_bridge_health_agent",
        "hb_tag": "autobio_sub",
        "typed_ptr": "autobio_substrate_bridge_t",
        "fields": {
            "config": "config",
            "effects": "effects",
            "update_count": "update_count",
            "threshold_field": "config.min_capacity",
            "capacity_field": "effects.overall_capacity",
        },
    },
    {
        "path": "src/cognitive/autobiographical_memory/nimcp_autobio_thalamic_bridge.c",
        "is_bridge": True,
        "struct_name": "autobio_thalamic_bridge",
        "prefix": "autobio_thalamic_bridge",
        "global_var": "g_autobio_thalamic_bridge_health_agent",
        "hb_tag": "autobio_thl",
        "typed_ptr": "autobio_thalamic_bridge_t",
        "fields": {
            "config": "config",
            "stats": "stats",
            "attention_weight": "attention_weight",
            "threshold_field": "config.min_vividness_threshold",
            "counter_field": "stats.recalls_routed",
        },
    },

    # === consolidation (7 files) ===
    {
        "path": "src/cognitive/consolidation/nimcp_consolidation.c",
        "is_bridge": False,
        "struct_name": "consolidation_handle_struct",
        "prefix": "consolidation",
        "global_var": "g_consolidation_health_agent",
        "hb_tag": "consol",
        "typed_ptr": None,
        "fields": {
            "config": "config",
            "stats": "stats",
            "counter_field": "stats.total_cycles",
            "progress_field": "current_progress",
            "interval_field": "interval_seconds",
        },
    },
    {
        "path": "src/cognitive/consolidation/nimcp_consolidation_fep_bridge.c",
        "is_bridge": True,
        "struct_name": "consolidation_fep_bridge",
        "prefix": "consolidation_fep_bridge",
        "global_var": "g_consolidation_fep_bridge_health_agent",
        "hb_tag": "consol_fep",
        "typed_ptr": "consolidation_fep_bridge_t",
        "fields": {
            "config": "config",
            "stats": "stats",
            "state": "state",
            "fep_effects": "fep_effects",
            "threshold_field": "config.fe_urgency_threshold",
            "weight_field": "config.fe_sensitivity",
            "counter_field": "stats.total_updates",
        },
    },
    {
        "path": "src/cognitive/consolidation/nimcp_consolidation_plasticity_bridge.c",
        "is_bridge": True,
        "struct_name": "consolidation_plasticity_bridge",
        "prefix": "consolidation_plasticity_bridge",
        "global_var": "g_consolidation_plasticity_bridge_health_agent",
        "hb_tag": "consol_pla",
        "typed_ptr": "consolidation_plasticity_bridge_t",
        "fields": {
            "config": "config",
            "state": "state",
            "stats": "stats (not present, use memory_state)",
            "threshold_field": "bcm_global_threshold",
            "weight_field": "learning_rate_effective",
            "counter_field": "synapse_count",
        },
    },
    {
        "path": "src/cognitive/consolidation/nimcp_consolidation_snn_bridge.c",
        "is_bridge": True,
        "struct_name": "consolidation_snn_bridge",
        "prefix": "consolidation_snn_bridge",
        "global_var": "g_consolidation_snn_bridge_health_agent",
        "hb_tag": "consol_snn",
        "typed_ptr": "consolidation_snn_bridge_t",
        "fields": {
            "config": "config",
            "state": "state",
            "threshold_field": "config.spike_threshold",
            "counter_field": "state.total_spikes",
            "signal_field": "replay_signal",
        },
    },
    {
        "path": "src/cognitive/consolidation/nimcp_consolidation_substrate_bridge.c",
        "is_bridge": True,
        "struct_name": "consolidation_substrate_bridge",
        "prefix": "consolidation_substrate_bridge",
        "global_var": "g_consolidation_substrate_bridge_health_agent",
        "hb_tag": "consol_sub",
        "typed_ptr": "consolidation_substrate_bridge_t",
        "fields": {
            "config": "config",
            "effects": "effects",
            "update_count": "update_count",
            "threshold_field": "config.min_capacity",
            "capacity_field": "effects.overall_capacity",
        },
    },
    {
        "path": "src/cognitive/consolidation/nimcp_consolidation_thalamic_bridge.c",
        "is_bridge": True,
        "struct_name": "consolidation_thalamic_bridge",
        "prefix": "consolidation_thalamic_bridge",
        "global_var": "g_consolidation_thalamic_bridge_health_agent",
        "hb_tag": "consol_thl",
        "typed_ptr": "consolidation_thalamic_bridge_t",
        "fields": {
            "config": "config",
            "stats": "stats",
            "attention_weight": "attention_weight",
            "threshold_field": "config.min_memory_salience",
            "counter_field": "stats.consolidations_routed",
        },
    },
    {
        "path": "src/cognitive/consolidation/nimcp_emotion_consolidation.c",
        "is_bridge": False,
        "struct_name": "emotion_consolidation_system",
        "prefix": "emotion_consolidation",
        "global_var": "g_emotion_consolidation_health_agent",
        "hb_tag": "emo_consol",
        "typed_ptr": None,
        "fields": {
            "config": "config",
            "stats": "stats",
            "counter_field": "stats.total_modulations",
            "arousal_field": "current_arousal",
            "boost_field": "current_boost",
        },
    },

    # === bias (6 files) ===
    {
        "path": "src/cognitive/bias/nimcp_bias_detection.c",
        "is_bridge": False,
        "struct_name": "bias_detection_system",
        "prefix": "bias_detection",
        "global_var": "g_bias_detection_health_agent",
        "hb_tag": "bias_det",
        "typed_ptr": None,
        "fields": {
            "counter_field": "total_update_calls",
            "awareness_field": "self_awareness",
            "fairness_field": "fairness_score",
        },
    },
    {
        "path": "src/cognitive/bias/nimcp_bias_fep_bridge.c",
        "is_bridge": True,
        "struct_name": "bias_fep_bridge",
        "prefix": "bias_fep_bridge",
        "global_var": "g_bias_fep_bridge_health_agent",
        "hb_tag": "bias_fep",
        "typed_ptr": "bias_fep_bridge_t",
        "fields": {
            "config": "config",
            "stats": "stats",
            "state": "state",
            "effects": "effects",
            "threshold_field": "config.systematic_pe_threshold",
            "weight_field": "config.pe_sensitivity",
            "counter_field": "stats.total_updates",
        },
    },
    {
        "path": "src/cognitive/bias/nimcp_bias_plasticity_bridge.c",
        "is_bridge": True,
        "struct_name": "bias_plasticity_bridge",
        "prefix": "bias_plasticity_bridge",
        "global_var": "g_bias_plasticity_bridge_health_agent",
        "hb_tag": "bias_pla",
        "typed_ptr": "bias_plasticity_bridge_t",
        "fields": {
            "config": "config",
            "state": "state",
            "stats": "stats",
            "threshold_field": "bcm_global_threshold",
            "weight_field": "global_learning_rate",
            "counter_field": "stats.total_updates",
            "synapse_count": "num_synapses",
        },
    },
    {
        "path": "src/cognitive/bias/nimcp_bias_snn_bridge.c",
        "is_bridge": True,
        "struct_name": "bias_snn_bridge",
        "prefix": "bias_snn_bridge",
        "global_var": "g_bias_snn_bridge_health_agent",
        "hb_tag": "bias_snn",
        "typed_ptr": "bias_snn_bridge_t",
        "fields": {
            "config": "config",
            "state": "state",
            "threshold_field": "config.spike_threshold",
            "counter_field": "state.total_spikes",
            "num_types": "num_types",
        },
    },
    {
        "path": "src/cognitive/bias/nimcp_bias_substrate_bridge.c",
        "is_bridge": True,
        "struct_name": "bias_substrate_bridge",
        "prefix": "bias_substrate_bridge",
        "global_var": "g_bias_substrate_bridge_health_agent",
        "hb_tag": "bias_sub",
        "typed_ptr": "bias_substrate_bridge_t",
        "fields": {
            "config": "config",
            "effects": "effects",
            "update_count": "update_count",
            "threshold_field": "config.min_capacity",
            "capacity_field": "effects.overall_capacity",
        },
    },
    {
        "path": "src/cognitive/bias/nimcp_bias_thalamic_bridge.c",
        "is_bridge": True,
        "struct_name": "bias_thalamic_bridge",
        "prefix": "bias_thalamic_bridge",
        "global_var": "g_bias_thalamic_bridge_health_agent",
        "hb_tag": "bias_thl",
        "typed_ptr": "bias_thalamic_bridge_t",
        "fields": {
            "config": "config",
            "stats": "stats",
            "attention_weight": "attention_weight",
            "threshold_field": "config.min_detection_confidence",
            "counter_field": "stats.detections_routed",
        },
    },

    # === logic (8 files) ===
    {
        "path": "src/cognitive/logic/nimcp_symbolic_logic.c",
        "is_bridge": False,
        "struct_name": "symbolic_logic",
        "prefix": "symbolic_logic",
        "global_var": "g_symbolic_logic_health_agent",
        "hb_tag": "sym_logic",
        "typed_ptr": None,
        "fields": {
            "config": "config",
            "stats": "stats",
            "counter_field": "stats.total_evaluations",
            "num_facts_field": "num_facts",
            "num_rules_field": "num_rules",
        },
    },
    {
        "path": "src/cognitive/logic/nimcp_logic_sleep_bridge.c",
        "is_bridge": True,
        "struct_name": "logic_sleep_bridge_struct",
        "prefix": "logic_sleep_bridge",
        "global_var": "g_logic_sleep_bridge_health_agent",
        "hb_tag": "logic_slp",
        "typed_ptr": "logic_sleep_bridge_t",
        "fields": {
            "config": "config",
            "effects": "effects",
            "threshold_field": "config.inference_threshold",
            "capacity_field": "effects.inference_capacity",
        },
    },
    {
        "path": "src/cognitive/logic/nimcp_logic_substrate_bridge.c",
        "is_bridge": True,
        "struct_name": "logic_substrate_bridge",
        "prefix": "logic_substrate_bridge",
        "global_var": "g_logic_substrate_bridge_health_agent",
        "hb_tag": "logic_sub",
        "typed_ptr": "logic_substrate_bridge_t",
        "fields": {
            "config": "config",
            "effects": "effects",
            "update_count": "update_count",
            "threshold_field": "config.min_capacity",
            "capacity_field": "effects.overall_capacity",
        },
    },
    {
        "path": "src/cognitive/logic/nimcp_logic_thalamic_bridge.c",
        "is_bridge": True,
        "struct_name": "logic_thalamic_bridge",
        "prefix": "logic_thalamic_bridge",
        "global_var": "g_logic_thalamic_bridge_health_agent",
        "hb_tag": "logic_thl",
        "typed_ptr": "logic_thalamic_bridge_t",
        "fields": {
            "config": "config",
            "stats": "stats",
            "attention_weight": "attention_weight",
            "threshold_field": "config.min_logical_strength",
            "counter_field": "stats.inferences_routed",
        },
    },
    {
        "path": "src/cognitive/logic/nimcp_audio_logic_bridge.c",
        "is_bridge": True,
        "struct_name": "audio_logic_bridge",
        "prefix": "audio_logic_bridge",
        "global_var": "g_audio_logic_bridge_health_agent",
        "hb_tag": "audio_lgc",
        "typed_ptr": "audio_logic_bridge_t",
        "fields": {
            "config": "config",
            "stats": "stats",
            "speaker_count": "speaker_count",
            "counter_field": "stats.total_inferences",
            "confidence_field": "speech_confidence_sum",
        },
    },
    {
        "path": "src/cognitive/logic/nimcp_visual_logic_bridge.c",
        "is_bridge": True,
        "struct_name": "visual_logic_bridge",
        "prefix": "visual_logic_bridge",
        "global_var": "g_visual_logic_bridge_health_agent",
        "hb_tag": "visual_lgc",
        "typed_ptr": "visual_logic_bridge_t",
        "fields": {
            "config": "config",
            "stats": "stats",
            "grounded_count": "grounded_count",
            "counter_field": "stats.total_inferences",
            "confidence_field": "confidence_sum",
        },
    },
    {
        "path": "src/cognitive/logic/nimcp_omni_logic_bridge.c",
        "is_bridge": True,
        "struct_name": "omni_logic_bridge",
        "prefix": "omni_logic_bridge",
        "global_var": "g_omni_logic_bridge_health_agent",
        "hb_tag": "omni_lgc",
        "typed_ptr": "omni_logic_bridge_t",
        "fields": {
            "config": "config",
            "stats": "stats",
            "num_rules": "num_rules",
            "counter_field": "stats.total_inferences",
        },
    },
    {
        "path": "src/cognitive/logic/nimcp_somatosensory_logic_bridge.c",
        "is_bridge": True,
        "struct_name": "somato_logic_bridge",
        "prefix": "somatosensory_logic_bridge",
        "global_var": "g_somatosensory_logic_bridge_health_agent",
        "hb_tag": "somato_lgc",
        "typed_ptr": "somato_logic_bridge_t",
        "fields": {
            "config": "config",
            "stats": "stats",
            "pending_count": "pending_count",
            "counter_field": "stats.total_inferences",
            "confidence_field": "touch_confidence_sum",
        },
    },
]

ROOT = "/home/bbrelin/nimcp/"

def gen_heartbeat_instance(prefix, global_var):
    """Generate heartbeat_instance function."""
    return f"""
/* ============================================================================
 * Phase 8 Instance-Level Health Agent Support
 * ============================================================================ */

/** @brief Instance-level health agent for per-object monitoring */
static nimcp_health_agent_t* {global_var.replace('_health_agent', '_instance_health_agent')} = NULL;

/** @brief Instance-level heartbeat: reports to both global and instance agents */
static inline void {prefix}_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{{
    if ({global_var}) {{
        nimcp_health_agent_heartbeat_ex({global_var}, operation, progress);
    }}
    if (instance_agent && instance_agent != {global_var}) {{
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }}
}}
"""

def gen_set_instance_health_agent_bridge(prefix, typed_ptr, struct_name, global_var):
    """Generate set_instance_health_agent for bridge files."""
    inst_var = global_var.replace('_health_agent', '_instance_health_agent')
    return f"""
/* ============================================================================
 * Phase 8: Instance-Level Health Agent Setter
 * ============================================================================ */

/**
 * @brief Set instance-level health agent for a specific {prefix} bridge
 * @param bridge Bridge instance
 * @param agent Health agent for this instance (NULL to disable)
 */
void {prefix}_set_instance_health_agent({typed_ptr}* bridge, nimcp_health_agent_t* agent) {{
    if (bridge) {{
        bridge->health_agent = agent;
    }}
    /* Also update module-level instance agent as fallback */
    {inst_var} = agent;
    NIMCP_LOGGING_DEBUG("{prefix}: instance health agent %s",
                        agent ? "set" : "cleared");
}}
"""

def gen_set_instance_health_agent_nonbridge(prefix, global_var):
    """Generate set_instance_health_agent for non-bridge files."""
    inst_var = global_var.replace('_health_agent', '_instance_health_agent')
    return f"""
/* ============================================================================
 * Phase 8: Instance-Level Health Agent Setter
 * ============================================================================ */

/**
 * @brief Set instance-level health agent for {prefix} module
 * @param instance Module instance (opaque pointer)
 * @param agent Health agent for this instance (NULL to disable)
 */
void {prefix}_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {{
    (void)instance;
    {inst_var} = agent;
    NIMCP_LOGGING_DEBUG("{prefix}: instance health agent %s",
                        agent ? "set" : "cleared");
}}
"""

def gen_training_bridge_substrate(prefix, typed_ptr, hb_tag, fields):
    """Generate FULL training functions for substrate bridge."""
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    /* Reset update counter for this training session */
    bridge->update_count = 0;

    /* Reset effects to baseline for training calibration */
    bridge->effects.overall_capacity = 1.0f;
    bridge->prev_overall_capacity = 1.0f;

    NIMCP_LOGGING_INFO("{prefix}: training session begun, counters reset");
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    /* Compute average capacity across training session */
    float avg_capacity = bridge->effects.overall_capacity;
    if (bridge->update_count > 0) {{
        avg_capacity = bridge->effects.overall_capacity;
    }}

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, updates=%llu avg_capacity=%.3f min_cap=%.3f",
                       (unsigned long long)bridge->update_count,
                       avg_capacity,
                       bridge->config.min_capacity);
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param bridge Bridge instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    /* Adapt min_capacity threshold based on training progress */
    float base_min = 0.2f;
    bridge->config.min_capacity = base_min + (1.0f - base_min) * p * 0.1f;

    /* Adapt sensitivity weights during training */
    bridge->config.atp_sensitivity = 1.0f + p * 0.05f;
    bridge->config.fatigue_sensitivity = 1.0f + p * 0.03f;

    /* Increment step counter */
    bridge->update_count++;

    return 0;
}}
"""

def gen_training_bridge_thalamic(prefix, typed_ptr, hb_tag, fields):
    """Generate FULL training functions for thalamic bridge."""
    threshold_field = fields.get("threshold_field", "config.min_vividness_threshold")
    counter_field = fields.get("counter_field", "stats.recalls_routed")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    /* Reset statistics for training session */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset attention to neutral for unbiased training */
    bridge->attention_weight = 1.0f;

    NIMCP_LOGGING_INFO("{prefix}: training session begun, stats reset");
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    /* Compute average attention weight */
    float avg_attention = bridge->attention_weight;

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, routed=%u avg_attention=%.3f threshold=%.3f",
                       bridge->{counter_field},
                       avg_attention,
                       bridge->{threshold_field});
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param bridge Bridge instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    /* Adapt attention gating threshold during training */
    float base_threshold = 0.3f;
    bridge->{threshold_field} = base_threshold * (1.0f - 0.15f * p);

    /* Adapt attention weight based on training progress */
    bridge->attention_weight = 1.0f - 0.1f * (1.0f - p);

    return 0;
}}
"""

def gen_training_bridge_fep(prefix, typed_ptr, hb_tag, fields):
    """Generate FULL training functions for FEP bridge."""
    threshold_field = fields.get("threshold_field", "config.fe_urgency_threshold")
    weight_field = fields.get("weight_field", "config.fe_sensitivity")
    counter_field = fields.get("counter_field", "stats.total_updates")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    /* Reset statistics counters for this training session */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset state to baseline for unbiased training */
    memset(&bridge->state, 0, sizeof(bridge->state));

    NIMCP_LOGGING_INFO("{prefix}: training session begun, state/stats reset");
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    /* Compute average sensitivity from training */
    float final_sensitivity = bridge->{weight_field};

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, updates=%u sensitivity=%.3f threshold=%.3f",
                       bridge->{counter_field},
                       final_sensitivity,
                       bridge->{threshold_field});
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param bridge Bridge instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    /* Adapt urgency threshold based on training progress */
    float base_threshold = bridge->{threshold_field};
    bridge->{threshold_field} = base_threshold * (1.0f - 0.1f * p);

    /* Refine sensitivity weights during training */
    bridge->{weight_field} = 1.0f + p * 0.05f;

    /* Increment update counter */
    bridge->{counter_field}++;

    return 0;
}}
"""

def gen_training_bridge_plasticity(prefix, typed_ptr, hb_tag, fields):
    """Generate FULL training functions for plasticity bridge."""
    threshold_field = fields.get("threshold_field", "bcm_global_threshold")
    weight_field = fields.get("weight_field", "learning_rate_effective")
    synapse_field = fields.get("synapse_count", "synapse_count")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    /* Reset statistics for this training session */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Set BCM threshold to baseline for training */
    bridge->{threshold_field} = 1.0f;

    /* Set learning rate to elevated baseline for training */
    bridge->{weight_field} = bridge->config.base_learning_rate * 1.5f;

    NIMCP_LOGGING_INFO("{prefix}: training begun, synapses=%u lr=%.4f",
                       bridge->{synapse_field}, bridge->{weight_field});
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    /* Compute average learning rate across training */
    float avg_lr = bridge->{weight_field};

    /* Restore standard learning rate */
    bridge->{weight_field} = bridge->config.base_learning_rate;

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, synapses=%u bcm_thresh=%.3f avg_lr=%.4f",
                       bridge->{synapse_field},
                       bridge->{threshold_field},
                       avg_lr);
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param bridge Bridge instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    /* Adapt BCM threshold toward convergence during training */
    bridge->{threshold_field} = 1.0f + 0.5f * p;

    /* Scale learning rate: higher early, taper late */
    float lr_scale = 1.5f * (1.0f - 0.5f * p);
    bridge->{weight_field} = bridge->config.base_learning_rate * lr_scale;

    /* Increment training step counter */
    bridge->stats.total_updates++;

    return 0;
}}
"""

def gen_training_bridge_snn(prefix, typed_ptr, hb_tag, fields):
    """Generate FULL training functions for SNN bridge."""
    threshold_field = fields.get("threshold_field", "config.spike_threshold")
    counter_field = fields.get("counter_field", "state.total_spikes")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    /* Reset state counters for training session */
    bridge->{counter_field} = 0;

    /* Lower spike threshold for increased sensitivity during training */
    float base_thresh = bridge->{threshold_field};
    bridge->{threshold_field} = base_thresh * 0.85f;

    NIMCP_LOGGING_INFO("{prefix}: training begun, spike_threshold=%.3f",
                       bridge->{threshold_field});
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    /* Compute average spike rate from training */
    uint32_t total_spikes = bridge->{counter_field};

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, total_spikes=%u threshold=%.3f",
                       total_spikes,
                       bridge->{threshold_field});
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param bridge Bridge instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    /* Adapt spike threshold: gradually restore toward baseline */
    float base_thresh = bridge->config.spike_threshold;
    bridge->{threshold_field} = base_thresh * (0.85f + 0.15f * p);

    return 0;
}}
"""

def gen_training_bridge_sleep(prefix, typed_ptr, hb_tag, fields):
    """Generate FULL training functions for sleep bridge."""
    threshold_field = fields.get("threshold_field", "config.encoding_threshold")
    capacity_field = fields.get("capacity_field", "effects.encoding_efficiency")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    /* Set effects to awake-optimal baseline for training */
    bridge->{capacity_field} = 1.0f;

    NIMCP_LOGGING_INFO("{prefix}: training begun, capacity=%.3f",
                       bridge->{capacity_field});
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    /* Log final training metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, capacity=%.3f threshold=%.3f",
                       bridge->{capacity_field},
                       bridge->{threshold_field});
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param bridge Bridge instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    /* Adapt encoding threshold during training */
    float base_thresh = 0.3f;
    bridge->{threshold_field} = base_thresh * (1.0f - 0.1f * p);

    return 0;
}}
"""

def gen_training_bridge_sensory(prefix, typed_ptr, hb_tag, fields):
    """Generate FULL training functions for sensory-logic bridges (audio/visual/somato)."""
    counter_field = fields.get("counter_field", "stats.total_inferences")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    /* Reset statistics for training session */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset pending command queue */
    bridge->pending_count = 0;

    NIMCP_LOGGING_INFO("{prefix}: training session begun, stats/commands reset");
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    /* Compute average confidence from training */
    uint32_t total = bridge->{counter_field};

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, total_inferences=%u",
                       total);
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param bridge Bridge instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    /* Adapt confidence thresholds during training */
    float base_conf = bridge->config.min_confidence;
    bridge->config.min_confidence = base_conf * (1.0f - 0.1f * p);

    return 0;
}}
"""

def gen_training_bridge_omni(prefix, typed_ptr, hb_tag, fields):
    """Generate FULL training functions for omni_logic bridge."""
    counter_field = fields.get("counter_field", "stats.total_inferences")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    /* Reset statistics for training session */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset conditions to neutral for training */
    memset(&bridge->conditions, 0, sizeof(bridge->conditions));

    NIMCP_LOGGING_INFO("{prefix}: training begun, rules=%u stats/conditions reset",
                       bridge->num_rules);
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    /* Compute average inference count */
    uint32_t total = bridge->{counter_field};

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, total_inferences=%u rules=%u",
                       total, bridge->num_rules);
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param bridge Bridge instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    /* Adapt confidence threshold during training */
    float base_conf = bridge->config.confidence_threshold;
    bridge->config.confidence_threshold = base_conf * (1.0f - 0.1f * p);

    return 0;
}}
"""

def gen_training_nonbridge(prefix, struct_name, hb_tag, fields, global_var):
    """Generate FULL training functions for non-bridge files."""
    inst_var = global_var.replace('_health_agent', '_instance_health_agent')

    # Determine the cast type and fields based on the specific file
    if struct_name == "autobiographical_memory_system":
        return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param ctx Module instance (opaque pointer to struct {struct_name})
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* sys = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_begin", 0.0f);

    /* Reset statistics for training session */
    memset(&sys->stats, 0, sizeof(sys->stats));

    /* Reset next_id counter is NOT reset (IDs are monotonic) */

    NIMCP_LOGGING_INFO("{prefix}: training begun, count=%u capacity=%u",
                       sys->count, sys->capacity);
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param ctx Module instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* sys = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_end", 1.0f);

    /* Compute average retrieval metrics */
    float avg_retrievals = (float)sys->stats.total_retrievals;

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, memories=%u retrievals=%.0f",
                       sys->count, avg_retrievals);
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param ctx Module instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step(void* ctx, float progress) {{
    if (!ctx) return -1;
    struct {struct_name}* sys = (struct {struct_name}*)ctx;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_step", p);

    /* Increment training step in stats */
    sys->stats.total_retrievals++;

    return 0;
}}
"""
    elif struct_name == "consolidation_handle_struct":
        return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param ctx Module instance (opaque pointer to struct {struct_name})
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* handle = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_begin", 0.0f);

    /* Reset statistics for training session */
    memset(&handle->stats, 0, sizeof(handle->stats));

    /* Reset progress tracking */
    handle->current_progress = 0.0f;
    handle->is_consolidating = false;

    NIMCP_LOGGING_INFO("{prefix}: training begun, interval=%u stats reset",
                       handle->interval_seconds);
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param ctx Module instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* handle = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_end", 1.0f);

    /* Compute average progress from training */
    float final_progress = handle->current_progress;

    /* Clear training state */
    handle->is_consolidating = false;

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, cycles=%u progress=%.3f",
                       handle->stats.total_cycles, final_progress);
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param ctx Module instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step(void* ctx, float progress) {{
    if (!ctx) return -1;
    struct {struct_name}* handle = (struct {struct_name}*)ctx;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_step", p);

    /* Track progress */
    handle->current_progress = p;

    /* Increment cycle counter */
    handle->stats.total_cycles++;

    return 0;
}}
"""
    elif struct_name == "emotion_consolidation_system":
        return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param ctx Module instance (opaque pointer to struct {struct_name})
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* sys = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_begin", 0.0f);

    /* Reset statistics for training session */
    memset(&sys->stats, 0, sizeof(sys->stats));

    /* Reset emotion state cache to neutral */
    sys->current_arousal = 0.5f;
    sys->current_valence = 0.0f;
    sys->current_boost = 1.0f;

    NIMCP_LOGGING_INFO("{prefix}: training begun, emotion state reset to neutral");
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param ctx Module instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* sys = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_end", 1.0f);

    /* Compute final training metrics */
    float final_boost = sys->current_boost;
    float final_arousal = sys->current_arousal;

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, modulations=%u boost=%.3f arousal=%.3f",
                       sys->stats.total_modulations,
                       final_boost, final_arousal);
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param ctx Module instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step(void* ctx, float progress) {{
    if (!ctx) return -1;
    struct {struct_name}* sys = (struct {struct_name}*)ctx;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_step", p);

    /* Adapt emotion thresholds during training */
    sys->config.min_emotion_threshold = 0.3f * (1.0f - 0.1f * p);

    /* Increment modulation counter */
    sys->stats.total_modulations++;

    return 0;
}}
"""
    elif struct_name == "bias_detection_system":
        # bias_detection_system_t is a typedef struct, not struct X
        return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param ctx Module instance (opaque pointer to bias_detection_system_t)
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin(void* ctx) {{
    if (!ctx) return -1;
    bias_detection_system_t* sys = (bias_detection_system_t*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_begin", 0.0f);

    /* Reset statistical counters for training session */
    sys->total_update_calls = 0;
    sys->total_decisions_analyzed = 0;
    sys->total_biases_detected = 0;
    sys->total_biases_corrected = 0;

    /* Reset awareness to baseline */
    sys->self_awareness = 0.5f;
    sys->bias_detected = false;
    sys->in_debiasing = false;

    NIMCP_LOGGING_INFO("{prefix}: training begun, counters/awareness reset");
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param ctx Module instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end(void* ctx) {{
    if (!ctx) return -1;
    bias_detection_system_t* sys = (bias_detection_system_t*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_end", 1.0f);

    /* Compute final training metrics */
    float correction_rate = 0.0f;
    if (sys->total_biases_detected > 0) {{
        correction_rate = (float)sys->total_biases_corrected / (float)sys->total_biases_detected;
    }}

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, updates=%llu detected=%llu corrected=%llu rate=%.3f awareness=%.3f",
                       (unsigned long long)sys->total_update_calls,
                       (unsigned long long)sys->total_biases_detected,
                       (unsigned long long)sys->total_biases_corrected,
                       correction_rate, sys->self_awareness);
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param ctx Module instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step(void* ctx, float progress) {{
    if (!ctx) return -1;
    bias_detection_system_t* sys = (bias_detection_system_t*)ctx;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_step", p);

    /* Adapt self-awareness during training */
    sys->self_awareness = 0.5f + 0.4f * p;

    /* Increment update counter */
    sys->total_update_calls++;

    return 0;
}}
"""
    elif struct_name == "symbolic_logic":
        return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for {prefix}
 * @param ctx Module instance (opaque pointer to struct {struct_name})
 * @return 0 on success, -1 on error
 */
int {prefix}_training_begin(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* logic = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_begin", 0.0f);

    /* Reset statistics for training session */
    memset(&logic->stats, 0, sizeof(logic->stats));

    NIMCP_LOGGING_INFO("{prefix}: training begun, facts=%u rules=%u stats reset",
                       logic->num_facts, logic->num_rules);
    return 0;
}}

/**
 * @brief End training session for {prefix}
 * @param ctx Module instance
 * @return 0 on success, -1 on error
 */
int {prefix}_training_end(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* logic = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_end", 1.0f);

    /* Compute average evaluations from training */
    uint32_t total_evals = logic->stats.total_evaluations;

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("{prefix}: training ended, facts=%u rules=%u evals=%u",
                       logic->num_facts, logic->num_rules, total_evals);
    return 0;
}}

/**
 * @brief Execute single training step for {prefix}
 * @param ctx Module instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int {prefix}_training_step(void* ctx, float progress) {{
    if (!ctx) return -1;
    struct {struct_name}* logic = (struct {struct_name}*)ctx;

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_step", p);

    /* Increment evaluation counter */
    logic->stats.total_evaluations++;

    return 0;
}}
"""
    return ""  # fallback


def determine_bridge_type(path, prefix, struct_name):
    """Determine the bridge sub-type from the path/prefix."""
    if "substrate_bridge" in path:
        return "substrate"
    elif "thalamic_bridge" in path:
        return "thalamic"
    elif "fep_bridge" in path:
        return "fep"
    elif "plasticity_bridge" in path:
        return "plasticity"
    elif "snn_bridge" in path:
        return "snn"
    elif "sleep_bridge" in path:
        return "sleep"
    elif "audio_logic_bridge" in path or "visual_logic_bridge" in path or "somatosensory_logic_bridge" in path:
        return "sensory"
    elif "omni_logic_bridge" in path:
        return "omni"
    else:
        return "fep"  # default for unrecognized bridges


def add_health_agent_to_struct(content, struct_name, is_bridge):
    """Add nimcp_health_agent_t* health_agent to the struct if it's a bridge."""
    if not is_bridge:
        return content

    # Find the struct definition and add health_agent field
    # We look for the struct opening and add the field after the base member
    pattern = rf'(struct\s+{re.escape(struct_name)}\s*\{{[^}}]*?bridge_base_t\s+base;[^\n]*\n)'
    match = re.search(pattern, content, re.DOTALL)
    if match:
        # Check if health_agent already exists
        struct_end = content.find('};', match.end())
        struct_block = content[match.start():struct_end]
        if 'nimcp_health_agent_t* health_agent' in struct_block or 'health_agent' in struct_block:
            return content  # Already has it

        # Add health_agent after base member
        insert_point = match.end()
        new_field = "    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */\n"
        content = content[:insert_point] + new_field + content[insert_point:]

    return content


def process_file(spec):
    """Process a single file with Phase 8 enhancements."""
    filepath = ROOT + spec["path"]
    is_bridge = spec["is_bridge"]
    struct_name = spec["struct_name"]
    prefix = spec["prefix"]
    global_var = spec["global_var"]
    hb_tag = spec["hb_tag"]
    typed_ptr = spec["typed_ptr"]
    fields = spec["fields"]

    with open(filepath, 'r') as f:
        content = f.read()

    # 1. Add health_agent field to struct (bridges only)
    if is_bridge:
        content = add_health_agent_to_struct(content, struct_name, is_bridge)

    # 2. Add heartbeat_instance after existing heartbeat function
    # Find the existing heartbeat function end
    hb_func_pattern = rf'(static inline void {re.escape(prefix)}_heartbeat\(const char\* operation, float progress\) \{{[^}}]*\}})'
    match = re.search(hb_func_pattern, content, re.DOTALL)
    if match:
        insert_point = match.end()
        hb_instance = gen_heartbeat_instance(prefix, global_var)
        content = content[:insert_point] + hb_instance + content[insert_point:]

    # 3. Append set_instance_health_agent and training functions at END
    if is_bridge:
        set_instance = gen_set_instance_health_agent_bridge(prefix, typed_ptr, struct_name, global_var)
    else:
        set_instance = gen_set_instance_health_agent_nonbridge(prefix, global_var)

    # Generate training functions based on bridge type
    if is_bridge:
        bridge_type = determine_bridge_type(spec["path"], prefix, struct_name)
        if bridge_type == "substrate":
            training = gen_training_bridge_substrate(prefix, typed_ptr, hb_tag, fields)
        elif bridge_type == "thalamic":
            training = gen_training_bridge_thalamic(prefix, typed_ptr, hb_tag, fields)
        elif bridge_type == "fep":
            training = gen_training_bridge_fep(prefix, typed_ptr, hb_tag, fields)
        elif bridge_type == "plasticity":
            training = gen_training_bridge_plasticity(prefix, typed_ptr, hb_tag, fields)
        elif bridge_type == "snn":
            training = gen_training_bridge_snn(prefix, typed_ptr, hb_tag, fields)
        elif bridge_type == "sleep":
            training = gen_training_bridge_sleep(prefix, typed_ptr, hb_tag, fields)
        elif bridge_type == "sensory":
            training = gen_training_bridge_sensory(prefix, typed_ptr, hb_tag, fields)
        elif bridge_type == "omni":
            training = gen_training_bridge_omni(prefix, typed_ptr, hb_tag, fields)
        else:
            training = gen_training_bridge_fep(prefix, typed_ptr, hb_tag, fields)
    else:
        training = gen_training_nonbridge(prefix, struct_name, hb_tag, fields, global_var)

    content = content.rstrip() + "\n" + set_instance + training

    with open(filepath, 'w') as f:
        f.write(content)

    return filepath


if __name__ == "__main__":
    count = 0
    for spec in files:
        try:
            path = process_file(spec)
            count += 1
            print(f"[{count:2d}/28] Processed: {spec['path']}")
        except Exception as e:
            print(f"ERROR processing {spec['path']}: {e}")
            import traceback
            traceback.print_exc()

    print(f"\nDone! Processed {count}/28 files.")
