#!/usr/bin/env python3
"""
Phase 8 Instance-Level Health Agent Enhancement Script.
Target: salience (17), attention (7), global_workspace (8) = 32 files

For each file, adds:
1. heartbeat_instance function (after existing heartbeat)
2. set_instance_health_agent at END
3. FULL training_begin/end/step at END (with real struct field access)
"""
import re
import sys

ROOT = "/home/bbrelin/nimcp/"

files = [
    # === salience/nimcp_salience_substrate_bridge.c (Cat B: bridge_base_t, substrate) ===
    {
        "path": "src/cognitive/salience/nimcp_salience_substrate_bridge.c",
        "is_bridge": True,
        "struct_name": "salience_substrate_bridge",
        "prefix": "salience_substrate_bridge",
        "global_var": "g_salience_substrate_bridge_health_agent",
        "hb_tag": "sal_sub",
        "typed_ptr": "salience_substrate_bridge_t",
        "bridge_type": "substrate",
        "fields": {
            "update_count": "update_count",
            "capacity_field": "effects.overall_capacity",
            "prev_capacity": "prev_overall_capacity",
            "min_capacity": "config.min_capacity",
            "atp_sensitivity": "config.atp_sensitivity",
            "fatigue_sensitivity": "config.fatigue_sensitivity",
        },
    },
    # === salience/nimcp_salience_thalamic_bridge.c (Cat B: bridge_base_t, thalamic) ===
    {
        "path": "src/cognitive/salience/nimcp_salience_thalamic_bridge.c",
        "is_bridge": True,
        "struct_name": "salience_thalamic_bridge",
        "prefix": "salience_thalamic_bridge",
        "global_var": "g_salience_thalamic_bridge_health_agent",
        "hb_tag": "sal_thl",
        "typed_ptr": "salience_thalamic_bridge_t",
        "bridge_type": "thalamic",
        "fields": {
            "attention_weight": "attention_weight",
            "threshold_field": "config.min_salience_threshold",
            "counter_field": "stats.signals_routed",
        },
    },
    # === salience/nimcp_salience_plasticity_bridge.c (Cat B: bridge_base_t, plasticity) ===
    {
        "path": "src/cognitive/salience/nimcp_salience_plasticity_bridge.c",
        "is_bridge": True,
        "struct_name": "salience_plasticity_bridge",
        "prefix": "salience_plasticity_bridge",
        "global_var": "g_salience_plasticity_bridge_health_agent",
        "hb_tag": "sal_pla",
        "typed_ptr": "salience_plasticity_bridge_t",
        "bridge_type": "plasticity",
        "fields": {
            "threshold_field": "bcm_global_threshold",
            "weight_field": "global_learning_rate",
            "synapse_count": "num_synapses",
            "stats_total": "stats.total_updates",
        },
    },
    # === salience/nimcp_salience_snn_bridge.c (Cat B: bridge_base_t, snn) ===
    {
        "path": "src/cognitive/salience/nimcp_salience_snn_bridge.c",
        "is_bridge": True,
        "struct_name": "salience_snn_bridge",
        "prefix": "salience_snn_bridge",
        "global_var": "g_salience_snn_bridge_health_agent",
        "hb_tag": "sal_snn",
        "typed_ptr": "salience_snn_bridge_t",
        "bridge_type": "snn",
        "fields": {
            "stats_spikes": "stats.total_spikes",
            "stats_evals": "stats.total_evaluations",
            "mean_salience": "stats.mean_salience",
        },
    },
    # === salience/nimcp_salience_fep_bridge.c (Cat D: no internal struct, fep) ===
    {
        "path": "src/cognitive/salience/nimcp_salience_fep_bridge.c",
        "is_bridge": True,
        "struct_name": None,
        "prefix": "salience_fep_bridge",
        "global_var": "g_salience_fep_bridge_health_agent",
        "hb_tag": "sal_fep",
        "typed_ptr": "salience_fep_bridge_t",
        "bridge_type": "fep_header",
        "fields": {
            "counter_field": "stats.total_precision_boosts",
            "avg_field": "stats.avg_salience",
        },
    },
    # === salience/nimcp_salience.c (Cat C: non-bridge) ===
    {
        "path": "src/cognitive/salience/nimcp_salience.c",
        "is_bridge": False,
        "struct_name": "salience_evaluator_struct",
        "prefix": "salience",
        "global_var": "g_salience_health_agent",
        "hb_tag": "salience",
        "typed_ptr": None,
        "bridge_type": None,
        "fields": {
            "stats_evaluations": "stats_evaluations",
            "running_avg": "running_avg_salience",
        },
    },
    # === 11 surprise bridges (Cat A: custom struct, already has hbi + health_agent) ===
    {
        "path": "src/cognitive/salience/nimcp_surprise_plasticity_bridge.c",
        "is_bridge": True,
        "struct_name": "surprise_plasticity_bridge",
        "prefix": "surprise_plasticity",
        "global_var": "g_surprise_plasticity_health_agent",
        "hb_tag": "surp_pla",
        "typed_ptr": "surprise_plasticity_bridge_t",
        "bridge_type": "surprise",
        "has_hbi": True,
        "has_health_field": True,
        "fields": {"stats": "stats", "config": "config"},
    },
    {
        "path": "src/cognitive/salience/nimcp_surprise_substrate_bridge.c",
        "is_bridge": True,
        "struct_name": "surprise_substrate_bridge",
        "prefix": "surprise_substrate",
        "global_var": "g_surprise_substrate_health_agent",
        "hb_tag": "surp_sub",
        "typed_ptr": "surprise_substrate_bridge_t",
        "bridge_type": "surprise",
        "has_hbi": True,
        "has_health_field": True,
        "fields": {"stats": "stats", "config": "config"},
    },
    {
        "path": "src/cognitive/salience/nimcp_surprise_imagination_bridge.c",
        "is_bridge": True,
        "struct_name": "surprise_imagination_bridge",
        "prefix": "surprise_imagination",
        "global_var": "g_surprise_imagination_health_agent",
        "hb_tag": "surp_img",
        "typed_ptr": "surprise_imagination_bridge_t",
        "bridge_type": "surprise",
        "has_hbi": True,
        "has_health_field": True,
        "fields": {"stats": "stats", "config": "config"},
    },
    {
        "path": "src/cognitive/salience/nimcp_surprise_thalamic_bridge.c",
        "is_bridge": True,
        "struct_name": "surprise_thalamic_bridge",
        "prefix": "surprise_thalamic",
        "global_var": "g_surprise_thalamic_health_agent",
        "hb_tag": "surp_thl",
        "typed_ptr": "surprise_thalamic_bridge_t",
        "bridge_type": "surprise",
        "has_hbi": True,
        "has_health_field": True,
        "fields": {"stats": "stats", "config": "config"},
    },
    {
        "path": "src/cognitive/salience/nimcp_surprise_pink_noise_bridge.c",
        "is_bridge": True,
        "struct_name": "surprise_pink_noise_bridge",
        "prefix": "surprise_pink_noise",
        "global_var": "g_surprise_pink_noise_health_agent",
        "hb_tag": "surp_pnk",
        "typed_ptr": "surprise_pink_noise_bridge_t",
        "bridge_type": "surprise",
        "has_hbi": True,
        "has_health_field": True,
        "fields": {"stats": "stats", "config": "config"},
    },
    {
        "path": "src/cognitive/salience/nimcp_surprise_self_model_bridge.c",
        "is_bridge": True,
        "struct_name": "surprise_self_model_bridge",
        "prefix": "surprise_self_model",
        "global_var": "g_surprise_self_model_health_agent",
        "hb_tag": "surp_self",
        "typed_ptr": "surprise_self_model_bridge_t",
        "bridge_type": "surprise",
        "has_hbi": True,
        "has_health_field": True,
        "fields": {"stats": "stats", "config": "config"},
    },
    {
        "path": "src/cognitive/salience/nimcp_surprise_fep_bridge.c",
        "is_bridge": True,
        "struct_name": "surprise_fep_bridge",
        "prefix": "surprise_fep",
        "global_var": "g_surprise_fep_bridge_health_agent",
        "hb_tag": "surp_fep",
        "typed_ptr": "surprise_fep_bridge_t",
        "bridge_type": "surprise",
        "has_hbi": True,
        "has_health_field": True,
        "fields": {"stats": "stats", "config": "config"},
    },
    {
        "path": "src/cognitive/salience/nimcp_surprise_snn_bridge.c",
        "is_bridge": True,
        "struct_name": "surprise_snn_bridge",
        "prefix": "surprise_snn",
        "global_var": "g_surprise_snn_health_agent",
        "hb_tag": "surp_snn",
        "typed_ptr": "surprise_snn_bridge_t",
        "bridge_type": "surprise",
        "has_hbi": True,
        "has_health_field": True,
        "fields": {"stats": "stats", "config": "config"},
    },
    {
        "path": "src/cognitive/salience/nimcp_surprise_gw_bridge.c",
        "is_bridge": True,
        "struct_name": "surprise_gw_bridge",
        "prefix": "surprise_gw",
        "global_var": "g_surprise_gw_bridge_health_agent",
        "hb_tag": "surp_gw",
        "typed_ptr": "surprise_gw_bridge_t",
        "bridge_type": "surprise",
        "has_hbi": True,
        "has_health_field": True,
        "fields": {"stats": "stats", "config": "config"},
    },
    {
        "path": "src/cognitive/salience/nimcp_surprise_attention_bridge.c",
        "is_bridge": True,
        "struct_name": "surprise_att_bridge",
        "prefix": "surprise_att",
        "global_var": "g_surprise_att_bridge_health_agent",
        "hb_tag": "surp_att",
        "typed_ptr": "surprise_att_bridge_t",
        "bridge_type": "surprise",
        "has_hbi": True,
        "has_health_field": True,
        "fields": {"stats": "stats", "config": "config"},
    },
    {
        "path": "src/cognitive/salience/nimcp_surprise_amplifier.c",
        "is_bridge": True,
        "struct_name": "surprise_amplifier",
        "prefix": "surprise_amplifier",
        "global_var": "g_surprise_amplifier_health_agent",
        "hb_tag": "surp_amp",
        "typed_ptr": "surprise_amplifier_t",
        "bridge_type": "surprise_amplifier",
        "has_hbi": True,
        "has_health_field": True,
        "fields": {"stats": "stats", "config": "config", "current_level": "current_level"},
    },
    # === ATTENTION ===
    {
        "path": "src/cognitive/attention/nimcp_attention_plasticity_bridge.c",
        "is_bridge": True,
        "struct_name": "attention_plasticity_bridge",
        "prefix": "attention_plasticity_bridge",
        "global_var": "g_attention_plasticity_bridge_health_agent",
        "hb_tag": "att_pla",
        "typed_ptr": "attention_plasticity_bridge_t",
        "bridge_type": "plasticity",
        "fields": {
            "threshold_field": "state.bcm_threshold",
            "weight_field": "global_learning_rate",
            "synapse_count": "synapse_count",
            "stats_total": "stats.total_updates",
        },
    },
    {
        "path": "src/cognitive/attention/nimcp_attention_snn_bridge.c",
        "is_bridge": True,
        "struct_name": "attention_snn_bridge",
        "prefix": "attention_snn_bridge",
        "global_var": "g_attention_snn_bridge_health_agent",
        "hb_tag": "att_snn",
        "typed_ptr": "attention_snn_bridge_t",
        "bridge_type": "snn",
        "fields": {
            "stats_spikes": "stats.total_spikes",
            "stats_evals": "stats.total_cycles",
        },
    },
    {
        "path": "src/cognitive/attention/nimcp_attention_substrate_bridge.c",
        "is_bridge": True,
        "struct_name": "attention_substrate_bridge",
        "prefix": "attention_substrate_bridge",
        "global_var": "g_attention_substrate_bridge_health_agent",
        "hb_tag": "att_sub",
        "typed_ptr": "attention_substrate_bridge_t",
        "bridge_type": "substrate",
        "fields": {
            "update_count": "update_count",
            "capacity_field": "effects.overall_capacity",
            "prev_capacity": "prev_overall_capacity",
            "min_capacity": "config.min_capacity",
            "atp_sensitivity": "config.atp_sensitivity",
            "fatigue_sensitivity": "config.fatigue_sensitivity",
        },
    },
    {
        "path": "src/cognitive/attention/nimcp_attention_thalamic_bridge.c",
        "is_bridge": True,
        "struct_name": "attention_thalamic_bridge",
        "prefix": "attention_thalamic_bridge",
        "global_var": "g_attention_thalamic_bridge_health_agent",
        "hb_tag": "att_thl",
        "typed_ptr": "attention_thalamic_bridge_t",
        "bridge_type": "thalamic",
        "fields": {
            "attention_weight": "attention_weight",
            "threshold_field": "config.min_priority_threshold",
            "counter_field": "stats.signals_routed",
        },
    },
    {
        "path": "src/cognitive/attention/nimcp_attention_sleep_bridge.c",
        "is_bridge": True,
        "struct_name": "attention_sleep_bridge_struct",
        "prefix": "attention_sleep_bridge",
        "global_var": "g_attention_sleep_bridge_health_agent",
        "hb_tag": "att_slp",
        "typed_ptr": "attention_sleep_bridge_t",
        "bridge_type": "sleep",
        "fields": {
            "threshold_field": "config.min_focus_threshold",
            "capacity_field": "effects.focus_capacity",
        },
    },
    {
        "path": "src/cognitive/attention/nimcp_attention_fep_bridge.c",
        "is_bridge": True,
        "struct_name": None,
        "prefix": "attention_fep_bridge",
        "global_var": "g_attention_fep_bridge_health_agent",
        "hb_tag": "att_fep",
        "typed_ptr": "attention_fep_bridge_t",
        "bridge_type": "fep_header",
        "fields": {
            "counter_field": "stats.total_updates",
        },
    },
    {
        "path": "src/cognitive/attention/nimcp_emotion_attention.c",
        "is_bridge": False,
        "struct_name": "emotion_attention_system",
        "prefix": "emotion_attention",
        "global_var": "g_emotion_attention_health_agent",
        "hb_tag": "emo_att",
        "typed_ptr": None,
        "bridge_type": None,
        "fields": {
            "stats": "stats",
            "arousal": "current_arousal",
            "valence": "current_valence",
            "attention_width": "current_attention_width",
        },
    },
    # === GLOBAL WORKSPACE ===
    {
        "path": "src/cognitive/global_workspace/nimcp_gw_plasticity_bridge.c",
        "is_bridge": True,
        "struct_name": "gw_plasticity_bridge",
        "prefix": "gw_plasticity_bridge",
        "global_var": "g_gw_plasticity_bridge_health_agent",
        "hb_tag": "gw_pla",
        "typed_ptr": "gw_plasticity_bridge_t",
        "bridge_type": "plasticity",
        "fields": {
            "threshold_field": "access_learning.threshold",
            "weight_field": "global_eligibility",
            "synapse_count": "num_synapses",
            "stats_total": "stats.total_updates",
        },
    },
    {
        "path": "src/cognitive/global_workspace/nimcp_gw_snn_bridge.c",
        "is_bridge": True,
        "struct_name": "gw_snn_bridge",
        "prefix": "gw_snn_bridge",
        "global_var": "g_gw_snn_bridge_health_agent",
        "hb_tag": "gw_snn",
        "typed_ptr": "gw_snn_bridge_t",
        "bridge_type": "snn",
        "fields": {
            "stats_spikes": "stats.total_spikes",
            "stats_evals": "stats.total_cycles",
        },
    },
    {
        "path": "src/cognitive/global_workspace/nimcp_gw_substrate_bridge.c",
        "is_bridge": True,
        "struct_name": "gw_substrate_bridge",
        "prefix": "gw_substrate_bridge",
        "global_var": "g_gw_substrate_bridge_health_agent",
        "hb_tag": "gw_sub",
        "typed_ptr": "gw_substrate_bridge_t",
        "bridge_type": "substrate",
        "fields": {
            "update_count": "stats.update_count",
            "capacity_field": "effects.overall_capacity",
            "min_capacity": "config.min_capacity",
            "atp_sensitivity": "config.atp_sensitivity",
            "fatigue_sensitivity": "config.fatigue_sensitivity",
        },
    },
    {
        "path": "src/cognitive/global_workspace/nimcp_gw_thalamic_bridge.c",
        "is_bridge": True,
        "struct_name": "gw_thalamic_bridge",
        "prefix": "gw_thalamic_bridge",
        "global_var": "g_gw_thalamic_bridge_health_agent",
        "hb_tag": "gw_thl",
        "typed_ptr": "gw_thalamic_bridge_t",
        "bridge_type": "thalamic",
        "fields": {
            "attention_weight": "attention_weight",
            "threshold_field": "config.min_salience_threshold",
            "counter_field": "stats.signals_routed",
            "update_count": "update_count",
        },
    },
    {
        "path": "src/cognitive/global_workspace/nimcp_global_workspace_fep_bridge.c",
        "is_bridge": True,
        "struct_name": None,
        "prefix": "global_workspace_fep_bridge",
        "global_var": "g_global_workspace_fep_bridge_health_agent",
        "hb_tag": "gw_fep",
        "typed_ptr": "global_workspace_fep_bridge_t",
        "bridge_type": "fep_header",
        "fields": {
            "counter_field": "stats.total_updates",
        },
    },
    {
        "path": "src/cognitive/global_workspace/nimcp_global_workspace.c",
        "is_bridge": False,
        "struct_name": "global_workspace_struct",
        "prefix": "global_workspace",
        "global_var": "g_global_workspace_health_agent",
        "hb_tag": "gw",
        "typed_ptr": None,
        "bridge_type": None,
        "fields": {
            "stats": "stats",
            "num_competitors": "num_active_competitors",
            "num_subscribers": "num_subscribers",
        },
    },
    {
        "path": "src/cognitive/global_workspace/nimcp_global_workspace_immune.c",
        "is_bridge": False,
        "struct_name": None,
        "prefix": "global_workspace_immune",
        "global_var": "g_global_workspace_immune_health_agent",
        "hb_tag": "gw_imm",
        "typed_ptr": None,
        "bridge_type": None,
        "fields": {},
    },
    {
        "path": "src/cognitive/global_workspace/nimcp_global_workspace_shannon.c",
        "is_bridge": False,
        "struct_name": None,
        "prefix": "global_workspace_shannon",
        "global_var": "g_global_workspace_shannon_health_agent",
        "hb_tag": "gw_sha",
        "typed_ptr": None,
        "bridge_type": None,
        "fields": {},
    },
]


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


def gen_set_instance_bridge(prefix, typed_ptr, global_var):
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


def gen_set_instance_nonbridge(prefix, global_var):
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


def gen_training_substrate(prefix, typed_ptr, hb_tag, fields):
    uc = fields.get("update_count", "update_count")
    cap = fields.get("capacity_field", "effects.overall_capacity")
    mc = fields.get("min_capacity", "config.min_capacity")
    atp = fields.get("atp_sensitivity", "config.atp_sensitivity")
    fat = fields.get("fatigue_sensitivity", "config.fatigue_sensitivity")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    /* Reset update counter for this training session */
    bridge->{uc} = 0;

    /* Reset effects to baseline for training calibration */
    bridge->{cap} = 1.0f;

    NIMCP_LOGGING_INFO("{prefix}: training session begun, counters reset");
    return 0;
}}

int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    float avg_capacity = bridge->{cap};
    NIMCP_LOGGING_INFO("{prefix}: training ended, updates=%llu avg_capacity=%.3f min_cap=%.3f",
                       (unsigned long long)bridge->{uc}, avg_capacity, bridge->{mc});
    return 0;
}}

int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    float base_min = 0.2f;
    bridge->{mc} = base_min + (1.0f - base_min) * p * 0.1f;
    bridge->{atp} = 1.0f + p * 0.05f;
    bridge->{fat} = 1.0f + p * 0.03f;
    bridge->{uc}++;

    return 0;
}}
"""


def gen_training_thalamic(prefix, typed_ptr, hb_tag, fields):
    aw = fields.get("attention_weight", "attention_weight")
    th = fields.get("threshold_field", "config.min_salience_threshold")
    ct = fields.get("counter_field", "stats.signals_routed")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->{aw} = 1.0f;

    NIMCP_LOGGING_INFO("{prefix}: training session begun, stats reset");
    return 0;
}}

int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    NIMCP_LOGGING_INFO("{prefix}: training ended, routed=%u attention=%.3f threshold=%.3f",
                       bridge->{ct}, bridge->{aw}, bridge->{th});
    return 0;
}}

int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    float base_threshold = 0.3f;
    bridge->{th} = base_threshold * (1.0f - 0.15f * p);
    bridge->{aw} = 1.0f - 0.1f * (1.0f - p);

    return 0;
}}
"""


def gen_training_plasticity(prefix, typed_ptr, hb_tag, fields):
    th = fields.get("threshold_field", "bcm_global_threshold")
    wf = fields.get("weight_field", "global_learning_rate")
    sc = fields.get("synapse_count", "num_synapses")
    st = fields.get("stats_total", "stats.total_updates")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->{th} = 1.0f;
    bridge->{wf} = bridge->config.base_learning_rate * 1.5f;

    NIMCP_LOGGING_INFO("{prefix}: training begun, synapses=%u lr=%.4f",
                       bridge->{sc}, bridge->{wf});
    return 0;
}}

int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    float avg_lr = bridge->{wf};
    bridge->{wf} = bridge->config.base_learning_rate;

    NIMCP_LOGGING_INFO("{prefix}: training ended, synapses=%u bcm=%.3f avg_lr=%.4f",
                       bridge->{sc}, bridge->{th}, avg_lr);
    return 0;
}}

int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    bridge->{th} = 1.0f + 0.5f * p;
    float lr_scale = 1.5f * (1.0f - 0.5f * p);
    bridge->{wf} = bridge->config.base_learning_rate * lr_scale;
    bridge->{st}++;

    return 0;
}}
"""


def gen_training_snn(prefix, typed_ptr, hb_tag, fields):
    sp = fields.get("stats_spikes", "stats.total_spikes")
    ev = fields.get("stats_evals", "stats.total_evaluations")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    NIMCP_LOGGING_INFO("{prefix}: training begun, stats reset");
    return 0;
}}

int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    NIMCP_LOGGING_INFO("{prefix}: training ended, spikes=%llu evals=%llu",
                       (unsigned long long)bridge->{sp},
                       (unsigned long long)bridge->{ev});
    return 0;
}}

int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    bridge->{ev}++;
    return 0;
}}
"""


def gen_training_sleep(prefix, typed_ptr, hb_tag, fields):
    th = fields.get("threshold_field", "config.min_focus_threshold")
    cap = fields.get("capacity_field", "effects.focus_capacity")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    bridge->{cap} = 1.0f;

    NIMCP_LOGGING_INFO("{prefix}: training begun, capacity=%.3f", bridge->{cap});
    return 0;
}}

int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    NIMCP_LOGGING_INFO("{prefix}: training ended, capacity=%.3f threshold=%.3f",
                       bridge->{cap}, bridge->{th});
    return 0;
}}

int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    float base_thresh = 0.3f;
    bridge->{th} = base_thresh * (1.0f - 0.1f * p);
    return 0;
}}
"""


def gen_training_fep_header(prefix, typed_ptr, hb_tag, fields, global_var):
    """FEP bridges with header-defined struct; use global fallback for health_agent."""
    ct = fields.get("counter_field", "stats.total_updates")
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(NULL, "{hb_tag}_training_begin", 0.0f);

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    memset(&bridge->state, 0, sizeof(bridge->state));

    NIMCP_LOGGING_INFO("{prefix}: training session begun, state/stats reset");
    return 0;
}}

int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(NULL, "{hb_tag}_training_end", 1.0f);

    NIMCP_LOGGING_INFO("{prefix}: training ended, updates=%u",
                       bridge->{ct});
    return 0;
}}

int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(NULL, "{hb_tag}_training_step", p);

    bridge->{ct}++;
    return 0;
}}
"""


def gen_training_surprise(prefix, typed_ptr, hb_tag, fields):
    """Surprise bridges: custom struct with stats, config, health_agent."""
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_begin", 0.0f);

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    NIMCP_LOGGING_INFO("{prefix}: training session begun, stats reset");
    return 0;
}}

int {prefix}_training_end({typed_ptr}* bridge) {{
    if (!bridge) return -1;
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_end", 1.0f);

    NIMCP_LOGGING_INFO("{prefix}: training ended, total_events=%u",
                       (unsigned)bridge->stats.total_events);
    return 0;
}}

int {prefix}_training_step({typed_ptr}* bridge, float progress) {{
    if (!bridge) return -1;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(bridge->health_agent, "{hb_tag}_training_step", p);

    bridge->stats.total_events++;
    return 0;
}}
"""


def gen_training_surprise_amplifier(prefix, typed_ptr, hb_tag, fields):
    return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin({typed_ptr}* amp) {{
    if (!amp) return -1;
    {prefix}_heartbeat_instance(amp->health_agent, "{hb_tag}_training_begin", 0.0f);

    memset(&amp->stats, 0, sizeof(amp->stats));
    amp->current_level = 0.0f;
    amp->active_event_count = 0;

    NIMCP_LOGGING_INFO("{prefix}: training session begun, stats/level reset");
    return 0;
}}

int {prefix}_training_end({typed_ptr}* amp) {{
    if (!amp) return -1;
    {prefix}_heartbeat_instance(amp->health_agent, "{hb_tag}_training_end", 1.0f);

    NIMCP_LOGGING_INFO("{prefix}: training ended, total_events=%llu level=%.3f",
                       (unsigned long long)amp->stats.total_events,
                       amp->current_level);
    return 0;
}}

int {prefix}_training_step({typed_ptr}* amp, float progress) {{
    if (!amp) return -1;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(amp->health_agent, "{hb_tag}_training_step", p);

    amp->stats.total_events++;
    return 0;
}}
"""


def gen_training_nonbridge(prefix, struct_name, hb_tag, fields, global_var):
    inst_var = global_var.replace('_health_agent', '_instance_health_agent')
    if struct_name == "salience_evaluator_struct":
        return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* eval = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_begin", 0.0f);

    eval->stats_evaluations = 0;
    eval->stats_high_salience = 0;
    eval->running_avg_salience = 0.0f;
    eval->running_avg_novelty = 0.0f;
    eval->running_avg_surprise = 0.0f;

    NIMCP_LOGGING_INFO("{prefix}: training begun, eval counters reset");
    return 0;
}}

int {prefix}_training_end(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* eval = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_end", 1.0f);

    NIMCP_LOGGING_INFO("{prefix}: training ended, evals=%llu avg_salience=%.3f",
                       (unsigned long long)eval->stats_evaluations,
                       eval->running_avg_salience);
    return 0;
}}

int {prefix}_training_step(void* ctx, float progress) {{
    if (!ctx) return -1;
    struct {struct_name}* eval = (struct {struct_name}*)ctx;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_step", p);

    eval->stats_evaluations++;
    return 0;
}}
"""
    elif struct_name == "emotion_attention_system":
        return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* sys = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_begin", 0.0f);

    memset(&sys->stats, 0, sizeof(sys->stats));
    sys->current_arousal = 0.5f;
    sys->current_valence = 0.0f;
    sys->current_attention_width = 1.0f;

    NIMCP_LOGGING_INFO("{prefix}: training begun, stats/emotion state reset");
    return 0;
}}

int {prefix}_training_end(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* sys = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_end", 1.0f);

    NIMCP_LOGGING_INFO("{prefix}: training ended, arousal=%.3f valence=%.3f width=%.3f",
                       sys->current_arousal, sys->current_valence, sys->current_attention_width);
    return 0;
}}

int {prefix}_training_step(void* ctx, float progress) {{
    if (!ctx) return -1;
    struct {struct_name}* sys = (struct {struct_name}*)ctx;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_step", p);

    sys->current_attention_width = 1.0f - 0.3f * p;
    return 0;
}}
"""
    elif struct_name == "global_workspace_struct":
        return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* gw = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_begin", 0.0f);

    memset(&gw->stats, 0, sizeof(gw->stats));

    NIMCP_LOGGING_INFO("{prefix}: training begun, stats reset, competitors=%u subscribers=%u",
                       gw->num_active_competitors, gw->num_subscribers);
    return 0;
}}

int {prefix}_training_end(void* ctx) {{
    if (!ctx) return -1;
    struct {struct_name}* gw = (struct {struct_name}*)ctx;
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_end", 1.0f);

    NIMCP_LOGGING_INFO("{prefix}: training ended, total_broadcasts=%llu",
                       (unsigned long long)gw->stats.total_broadcasts);
    return 0;
}}

int {prefix}_training_step(void* ctx, float progress) {{
    if (!ctx) return -1;
    struct {struct_name}* gw = (struct {struct_name}*)ctx;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance({inst_var}, "{hb_tag}_training_step", p);

    gw->stats.total_broadcasts++;
    return 0;
}}
"""
    else:
        # Generic non-bridge with no internal struct (immune, shannon)
        return f"""
/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int {prefix}_training_begin(void* ctx) {{
    if (!ctx) return -1;
    {prefix}_heartbeat_instance(NULL, "{hb_tag}_training_begin", 0.0f);
    NIMCP_LOGGING_INFO("{prefix}: training begun");
    return 0;
}}

int {prefix}_training_end(void* ctx) {{
    if (!ctx) return -1;
    {prefix}_heartbeat_instance(NULL, "{hb_tag}_training_end", 1.0f);
    NIMCP_LOGGING_INFO("{prefix}: training ended");
    return 0;
}}

int {prefix}_training_step(void* ctx, float progress) {{
    if (!ctx) return -1;
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    {prefix}_heartbeat_instance(NULL, "{hb_tag}_training_step", p);
    return 0;
}}
"""


def add_health_agent_to_struct(content, struct_name):
    """Add health_agent field after bridge_base_t base."""
    pattern = rf'(struct\s+{re.escape(struct_name)}\s*\{{[^}}]*?)(}}\s*;)'
    match = re.search(pattern, content, re.DOTALL)
    if match:
        body = match.group(1)
        if 'health_agent' in body:
            return content  # Already has it
        # Add health_agent before closing brace
        new_body = body + "\n    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */\n"
        content = content[:match.start()] + new_body + match.group(2) + content[match.end():]
    return content


def process_file(spec):
    filepath = ROOT + spec["path"]
    is_bridge = spec["is_bridge"]
    struct_name = spec.get("struct_name")
    prefix = spec["prefix"]
    global_var = spec["global_var"]
    hb_tag = spec["hb_tag"]
    typed_ptr = spec.get("typed_ptr")
    bridge_type = spec.get("bridge_type")
    fields = spec.get("fields", {})
    has_hbi = spec.get("has_hbi", False)
    has_health_field = spec.get("has_health_field", False)

    with open(filepath, 'r') as f:
        content = f.read()

    # 1. Add health_agent field to struct (bridges with internal struct only)
    if is_bridge and struct_name and not has_health_field:
        content = add_health_agent_to_struct(content, struct_name)

    # 2. Add heartbeat_instance after existing heartbeat function
    if not has_hbi:
        hb_pattern = rf'(static inline void {re.escape(prefix)}_heartbeat\(const char\* operation, float progress\)\s*\{{[^}}]*\}})'
        match = re.search(hb_pattern, content, re.DOTALL)
        if match:
            hbi = gen_heartbeat_instance(prefix, global_var)
            pos = match.end()
            content = content[:pos] + hbi + content[pos:]

    # 3. Append set_instance and training at END
    if is_bridge:
        if struct_name or bridge_type == "fep_header":
            si = gen_set_instance_bridge(prefix, typed_ptr, global_var)
        else:
            si = gen_set_instance_bridge(prefix, typed_ptr, global_var)
    else:
        si = gen_set_instance_nonbridge(prefix, global_var)

    # Generate training based on bridge type
    if bridge_type == "substrate":
        tr = gen_training_substrate(prefix, typed_ptr, hb_tag, fields)
    elif bridge_type == "thalamic":
        tr = gen_training_thalamic(prefix, typed_ptr, hb_tag, fields)
    elif bridge_type == "plasticity":
        tr = gen_training_plasticity(prefix, typed_ptr, hb_tag, fields)
    elif bridge_type == "snn":
        tr = gen_training_snn(prefix, typed_ptr, hb_tag, fields)
    elif bridge_type == "sleep":
        tr = gen_training_sleep(prefix, typed_ptr, hb_tag, fields)
    elif bridge_type == "fep_header":
        tr = gen_training_fep_header(prefix, typed_ptr, hb_tag, fields, global_var)
    elif bridge_type == "surprise":
        tr = gen_training_surprise(prefix, typed_ptr, hb_tag, fields)
    elif bridge_type == "surprise_amplifier":
        tr = gen_training_surprise_amplifier(prefix, typed_ptr, hb_tag, fields)
    elif not is_bridge:
        tr = gen_training_nonbridge(prefix, struct_name, hb_tag, fields, global_var)
    else:
        tr = ""

    content = content.rstrip() + "\n" + si + tr

    with open(filepath, 'w') as f:
        f.write(content)
    return filepath


if __name__ == "__main__":
    count = 0
    for spec in files:
        try:
            path = process_file(spec)
            count += 1
            print(f"[{count:2d}/{len(files)}] Processed: {spec['path']}")
        except Exception as e:
            print(f"ERROR processing {spec['path']}: {e}")
            import traceback
            traceback.print_exc()

    print(f"\nDone! Processed {count}/{len(files)} files.")
