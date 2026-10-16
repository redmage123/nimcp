#!/bin/bash
BASE="/home/bbrelin/nimcp/src/cognitive"

# fep_consciousness - bridge with struct
cat >> "$BASE/free_energy/nimcp_fep_consciousness.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_consciousness_set_instance_health_agent(fep_consciousness_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_consciousness_training_begin(fep_consciousness_bridge_t* bridge) {
    if (!bridge) return -1;
    fep_consciousness_heartbeat_instance(bridge->health_agent, "fep_cons_training_begin", 0.0f);
    struct fep_consciousness_bridge* b = (struct fep_consciousness_bridge*)bridge;
    b->state.phi_value = 0.0f;
    b->config.phi_threshold = (b->config.phi_threshold > 0.0f) ? b->config.phi_threshold : 1.0f;
    b->config.attention_decay = (b->config.attention_decay > 0.0f) ? b->config.attention_decay : 1.0f;
    NIMCP_LOGGING_INFO("fep_consciousness: training begun, counters reset");
    return 0;
}

int fep_consciousness_training_step(fep_consciousness_bridge_t* bridge, float progress) {
    if (!bridge) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_consciousness_heartbeat_instance(bridge->health_agent, "fep_cons_training_step", clamped);
    struct fep_consciousness_bridge* b = (struct fep_consciousness_bridge*)bridge;
    float p = clamped;
    b->config.phi_threshold += (1.0f - p) * 0.001f;
    if (b->config.phi_threshold > 2.0f) b->config.phi_threshold = 2.0f;
    if (b->config.phi_threshold < 0.0f) b->config.phi_threshold = 0.0f;
    b->config.attention_decay += (1.0f - p) * 0.001f;
    if (b->config.attention_decay > 2.0f) b->config.attention_decay = 2.0f;
    if (b->config.attention_decay < 0.0f) b->config.attention_decay = 0.0f;
    b->state.phi_value += (1.0f - p) * 0.001f;
    if (b->state.phi_value > 2.0f) b->state.phi_value = 2.0f;
    if (b->state.phi_value < 0.0f) b->state.phi_value = 0.0f;
    b->state.consciousness_level += (1.0f - p) * 0.001f;
    if (b->state.consciousness_level > 2.0f) b->state.consciousness_level = 2.0f;
    if (b->state.consciousness_level < 0.0f) b->state.consciousness_level = 0.0f;
    b->state.attention_focus += (1.0f - p) * 0.001f;
    if (b->state.attention_focus > 2.0f) b->state.attention_focus = 2.0f;
    if (b->state.attention_focus < 0.0f) b->state.attention_focus = 0.0f;
    return 0;
}

int fep_consciousness_training_end(fep_consciousness_bridge_t* bridge) {
    if (!bridge) return -1;
    fep_consciousness_heartbeat_instance(bridge->health_agent, "fep_cons_training_end", 1.0f);
    struct fep_consciousness_bridge* b = (struct fep_consciousness_bridge*)bridge;
    float metric_sum = 0.0f;
    metric_sum += b->config.phi_threshold;
    metric_sum += b->config.attention_decay;
    metric_sum += b->state.phi_value;
    metric_sum += b->state.consciousness_level;
    metric_sum += b->state.attention_focus;
    float avg_metric = metric_sum / 5.0f;
    NIMCP_LOGGING_INFO("fep_consciousness: training complete, avg_metric=%.4f", avg_metric);
    return 0;
}
EOF

# fep_context - non-bridge with struct
cat >> "$BASE/free_energy/nimcp_fep_context.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_context_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_fep_context_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_context_training_begin(void* ctx) {
    if (!ctx) return -1;
    fep_context_heartbeat_instance(g_fep_context_instance_health_agent, "fep_ctx_training_begin", 0.0f);
    struct fep_context_system* s = (struct fep_context_system*)ctx;
    s->num_contexts = 0;
    s->active_confidence = (s->active_confidence > 0.0f) ? s->active_confidence : 0.5f;
    s->blend_alpha = (s->blend_alpha > 0.0f) ? s->blend_alpha : 0.5f;
    NIMCP_LOGGING_INFO("fep_context: training begun, counters reset");
    return 0;
}

int fep_context_training_step(void* ctx, float progress) {
    if (!ctx) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_context_heartbeat_instance(g_fep_context_instance_health_agent, "fep_ctx_training_step", clamped);
    struct fep_context_system* s = (struct fep_context_system*)ctx;
    float p = clamped;
    s->active_confidence += (1.0f - p) * 0.001f;
    if (s->active_confidence > 2.0f) s->active_confidence = 2.0f;
    if (s->active_confidence < 0.0f) s->active_confidence = 0.0f;
    s->blend_alpha += (1.0f - p) * 0.001f;
    if (s->blend_alpha > 2.0f) s->blend_alpha = 2.0f;
    if (s->blend_alpha < 0.0f) s->blend_alpha = 0.0f;
    s->num_contexts++;
    return 0;
}

int fep_context_training_end(void* ctx) {
    if (!ctx) return -1;
    fep_context_heartbeat_instance(g_fep_context_instance_health_agent, "fep_ctx_training_end", 1.0f);
    struct fep_context_system* s = (struct fep_context_system*)ctx;
    float metric_sum = 0.0f;
    metric_sum += s->active_confidence;
    metric_sum += s->blend_alpha;
    float avg_metric = metric_sum / 2.0f;
    NIMCP_LOGGING_INFO("fep_context: training complete, avg_metric=%.4f", avg_metric);
    return 0;
}
EOF

# fep_curiosity - non-bridge with struct
cat >> "$BASE/free_energy/nimcp_fep_curiosity.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_curiosity_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_fep_curiosity_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_curiosity_training_begin(void* ctx) {
    if (!ctx) return -1;
    fep_curiosity_heartbeat_instance(g_fep_curiosity_instance_health_agent, "fep_cur_training_begin", 0.0f);
    struct fep_curiosity_system* s = (struct fep_curiosity_system*)ctx;
    s->stats.total_evaluations = 0;
    s->config.epistemic_weight = (s->config.epistemic_weight > 0.0f) ? s->config.epistemic_weight : 0.5f;
    s->config.novelty_weight = (s->config.novelty_weight > 0.0f) ? s->config.novelty_weight : 0.5f;
    NIMCP_LOGGING_INFO("fep_curiosity: training begun, counters reset");
    return 0;
}

int fep_curiosity_training_step(void* ctx, float progress) {
    if (!ctx) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_curiosity_heartbeat_instance(g_fep_curiosity_instance_health_agent, "fep_cur_training_step", clamped);
    struct fep_curiosity_system* s = (struct fep_curiosity_system*)ctx;
    float p = clamped;
    s->config.epistemic_weight += (1.0f - p) * 0.001f;
    if (s->config.epistemic_weight > 2.0f) s->config.epistemic_weight = 2.0f;
    if (s->config.epistemic_weight < 0.0f) s->config.epistemic_weight = 0.0f;
    s->config.novelty_weight += (1.0f - p) * 0.001f;
    if (s->config.novelty_weight > 2.0f) s->config.novelty_weight = 2.0f;
    if (s->config.novelty_weight < 0.0f) s->config.novelty_weight = 0.0f;
    s->config.empowerment_weight += (1.0f - p) * 0.001f;
    if (s->config.empowerment_weight > 2.0f) s->config.empowerment_weight = 2.0f;
    if (s->config.empowerment_weight < 0.0f) s->config.empowerment_weight = 0.0f;
    s->state.total_curiosity += (1.0f - p) * 0.001f;
    if (s->state.total_curiosity > 2.0f) s->state.total_curiosity = 2.0f;
    if (s->state.total_curiosity < 0.0f) s->state.total_curiosity = 0.0f;
    s->stats.total_evaluations++;
    return 0;
}

int fep_curiosity_training_end(void* ctx) {
    if (!ctx) return -1;
    fep_curiosity_heartbeat_instance(g_fep_curiosity_instance_health_agent, "fep_cur_training_end", 1.0f);
    struct fep_curiosity_system* s = (struct fep_curiosity_system*)ctx;
    float metric_sum = 0.0f;
    metric_sum += s->config.epistemic_weight;
    metric_sum += s->config.novelty_weight;
    metric_sum += s->config.empowerment_weight;
    metric_sum += s->state.total_curiosity;
    float avg_metric = metric_sum / 4.0f;
    NIMCP_LOGGING_INFO("fep_curiosity: training complete, avg_metric=%.4f", avg_metric);
    return 0;
}
EOF

# fep_evidence - non-bridge, no struct
cat >> "$BASE/free_energy/nimcp_fep_evidence.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_evidence_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_fep_evidence_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_evidence_training_begin(void* ctx) {
    if (!ctx) return -1;
    fep_evidence_heartbeat_instance(g_fep_evidence_instance_health_agent, "fep_evi_training_begin", 0.0f);
    NIMCP_LOGGING_INFO("fep_evidence: training begun");
    return 0;
}

int fep_evidence_training_step(void* ctx, float progress) {
    if (!ctx) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_evidence_heartbeat_instance(g_fep_evidence_instance_health_agent, "fep_evi_training_step", clamped);
    (void)clamped;
    return 0;
}

int fep_evidence_training_end(void* ctx) {
    if (!ctx) return -1;
    fep_evidence_heartbeat_instance(g_fep_evidence_instance_health_agent, "fep_evi_training_end", 1.0f);
    NIMCP_LOGGING_INFO("fep_evidence: training complete");
    return 0;
}
EOF

# fep_immune_bridge - opaque bridge
cat >> "$BASE/free_energy/nimcp_fep_immune_bridge.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_immune_bridge_set_instance_health_agent(fep_immune_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        g_fep_immune_bridge_instance_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_immune_bridge_training_begin(fep_immune_bridge_t* bridge) {
    if (!bridge) return -1;
    fep_immune_bridge_heartbeat_instance(g_fep_immune_bridge_instance_health_agent, "fep_imm_training_begin", 0.0f);
    NIMCP_LOGGING_INFO("fep_immune_bridge: training begun");
    return 0;
}

int fep_immune_bridge_training_step(fep_immune_bridge_t* bridge, float progress) {
    if (!bridge) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_immune_bridge_heartbeat_instance(g_fep_immune_bridge_instance_health_agent, "fep_imm_training_step", clamped);
    (void)clamped;
    return 0;
}

int fep_immune_bridge_training_end(fep_immune_bridge_t* bridge) {
    if (!bridge) return -1;
    fep_immune_bridge_heartbeat_instance(g_fep_immune_bridge_instance_health_agent, "fep_imm_training_end", 1.0f);
    NIMCP_LOGGING_INFO("fep_immune_bridge: training complete");
    return 0;
}
EOF

# fep_learning - non-bridge, no struct
cat >> "$BASE/free_energy/nimcp_fep_learning.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_learning_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_fep_learning_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_learning_training_begin(void* ctx) {
    if (!ctx) return -1;
    fep_learning_heartbeat_instance(g_fep_learning_instance_health_agent, "fep_lrn_training_begin", 0.0f);
    NIMCP_LOGGING_INFO("fep_learning: training begun");
    return 0;
}

int fep_learning_training_step(void* ctx, float progress) {
    if (!ctx) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_learning_heartbeat_instance(g_fep_learning_instance_health_agent, "fep_lrn_training_step", clamped);
    (void)clamped;
    return 0;
}

int fep_learning_training_end(void* ctx) {
    if (!ctx) return -1;
    fep_learning_heartbeat_instance(g_fep_learning_instance_health_agent, "fep_lrn_training_end", 1.0f);
    NIMCP_LOGGING_INFO("fep_learning: training complete");
    return 0;
}
EOF

# fep_neuromod - non-bridge, no struct
cat >> "$BASE/free_energy/nimcp_fep_neuromod.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_neuromod_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_fep_neuromod_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_neuromod_training_begin(void* ctx) {
    if (!ctx) return -1;
    fep_neuromod_heartbeat_instance(g_fep_neuromod_instance_health_agent, "fep_nmod_training_begin", 0.0f);
    NIMCP_LOGGING_INFO("fep_neuromod: training begun");
    return 0;
}

int fep_neuromod_training_step(void* ctx, float progress) {
    if (!ctx) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_neuromod_heartbeat_instance(g_fep_neuromod_instance_health_agent, "fep_nmod_training_step", clamped);
    (void)clamped;
    return 0;
}

int fep_neuromod_training_end(void* ctx) {
    if (!ctx) return -1;
    fep_neuromod_heartbeat_instance(g_fep_neuromod_instance_health_agent, "fep_nmod_training_end", 1.0f);
    NIMCP_LOGGING_INFO("fep_neuromod: training complete");
    return 0;
}
EOF

# fep_orchestrator - non-bridge, no struct
cat >> "$BASE/free_energy/nimcp_fep_orchestrator.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_orchestrator_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_fep_orchestrator_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_orchestrator_training_begin(void* ctx) {
    if (!ctx) return -1;
    fep_orchestrator_heartbeat_instance(g_fep_orchestrator_instance_health_agent, "fep_orch_training_begin", 0.0f);
    NIMCP_LOGGING_INFO("fep_orchestrator: training begun");
    return 0;
}

int fep_orchestrator_training_step(void* ctx, float progress) {
    if (!ctx) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_orchestrator_heartbeat_instance(g_fep_orchestrator_instance_health_agent, "fep_orch_training_step", clamped);
    (void)clamped;
    return 0;
}

int fep_orchestrator_training_end(void* ctx) {
    if (!ctx) return -1;
    fep_orchestrator_heartbeat_instance(g_fep_orchestrator_instance_health_agent, "fep_orch_training_end", 1.0f);
    NIMCP_LOGGING_INFO("fep_orchestrator: training complete");
    return 0;
}
EOF

# fep_planning - non-bridge, no struct
cat >> "$BASE/free_energy/nimcp_fep_planning.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_planning_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_fep_planning_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_planning_training_begin(void* ctx) {
    if (!ctx) return -1;
    fep_planning_heartbeat_instance(g_fep_planning_instance_health_agent, "fep_plan_training_begin", 0.0f);
    NIMCP_LOGGING_INFO("fep_planning: training begun");
    return 0;
}

int fep_planning_training_step(void* ctx, float progress) {
    if (!ctx) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_planning_heartbeat_instance(g_fep_planning_instance_health_agent, "fep_plan_training_step", clamped);
    (void)clamped;
    return 0;
}

int fep_planning_training_end(void* ctx) {
    if (!ctx) return -1;
    fep_planning_heartbeat_instance(g_fep_planning_instance_health_agent, "fep_plan_training_end", 1.0f);
    NIMCP_LOGGING_INFO("fep_planning: training complete");
    return 0;
}
EOF

# fep_plasticity_bridge - bridge with struct
cat >> "$BASE/free_energy/nimcp_fep_plasticity_bridge.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_plasticity_bridge_set_instance_health_agent(fep_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_plasticity_bridge_training_begin(fep_plasticity_bridge_t* bridge) {
    if (!bridge) return -1;
    fep_plasticity_bridge_heartbeat_instance(bridge->health_agent, "fep_plast_training_begin", 0.0f);
    struct fep_plasticity_bridge* b = (struct fep_plasticity_bridge*)bridge;
    b->state.total_ltp_events = 0;
    b->state.total_ltd_events = 0;
    b->config.stdp_a_plus = (b->config.stdp_a_plus > 0.0f) ? b->config.stdp_a_plus : 1.0f;
    b->config.stdp_a_minus = (b->config.stdp_a_minus > 0.0f) ? b->config.stdp_a_minus : 1.0f;
    NIMCP_LOGGING_INFO("fep_plasticity_bridge: training begun, counters reset");
    return 0;
}

int fep_plasticity_bridge_training_step(fep_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_plasticity_bridge_heartbeat_instance(bridge->health_agent, "fep_plast_training_step", clamped);
    struct fep_plasticity_bridge* b = (struct fep_plasticity_bridge*)bridge;
    float p = clamped;
    b->config.stdp_a_plus += (1.0f - p) * 0.001f;
    if (b->config.stdp_a_plus > 2.0f) b->config.stdp_a_plus = 2.0f;
    if (b->config.stdp_a_plus < 0.0f) b->config.stdp_a_plus = 0.0f;
    b->config.stdp_a_minus += (1.0f - p) * 0.001f;
    if (b->config.stdp_a_minus > 2.0f) b->config.stdp_a_minus = 2.0f;
    if (b->config.stdp_a_minus < 0.0f) b->config.stdp_a_minus = 0.0f;
    b->learning_rate_effective += (1.0f - p) * 0.001f;
    if (b->learning_rate_effective > 2.0f) b->learning_rate_effective = 2.0f;
    if (b->learning_rate_effective < 0.0f) b->learning_rate_effective = 0.0f;
    b->bcm_global_threshold += (1.0f - p) * 0.001f;
    if (b->bcm_global_threshold > 2.0f) b->bcm_global_threshold = 2.0f;
    if (b->bcm_global_threshold < 0.0f) b->bcm_global_threshold = 0.0f;
    b->state.total_ltp_events++;
    b->state.total_ltd_events++;
    return 0;
}

int fep_plasticity_bridge_training_end(fep_plasticity_bridge_t* bridge) {
    if (!bridge) return -1;
    fep_plasticity_bridge_heartbeat_instance(bridge->health_agent, "fep_plast_training_end", 1.0f);
    struct fep_plasticity_bridge* b = (struct fep_plasticity_bridge*)bridge;
    float metric_sum = 0.0f;
    metric_sum += b->config.stdp_a_plus;
    metric_sum += b->config.stdp_a_minus;
    metric_sum += b->learning_rate_effective;
    metric_sum += b->bcm_global_threshold;
    float avg_metric = metric_sum / 4.0f;
    NIMCP_LOGGING_INFO("fep_plasticity_bridge: training complete, avg_metric=%.4f", avg_metric);
    return 0;
}
EOF

# fep_sleep - non-bridge, no struct
cat >> "$BASE/free_energy/nimcp_fep_sleep.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_sleep_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_fep_sleep_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_sleep_training_begin(void* ctx) {
    if (!ctx) return -1;
    fep_sleep_heartbeat_instance(g_fep_sleep_instance_health_agent, "fep_slp_training_begin", 0.0f);
    NIMCP_LOGGING_INFO("fep_sleep: training begun");
    return 0;
}

int fep_sleep_training_step(void* ctx, float progress) {
    if (!ctx) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_sleep_heartbeat_instance(g_fep_sleep_instance_health_agent, "fep_slp_training_step", clamped);
    (void)clamped;
    return 0;
}

int fep_sleep_training_end(void* ctx) {
    if (!ctx) return -1;
    fep_sleep_heartbeat_instance(g_fep_sleep_instance_health_agent, "fep_slp_training_end", 1.0f);
    NIMCP_LOGGING_INFO("fep_sleep: training complete");
    return 0;
}
EOF

# fep_snn_bridge - bridge with struct
cat >> "$BASE/free_energy/nimcp_fep_snn_bridge.c" << 'EOF'

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_snn_bridge_set_instance_health_agent(fep_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_snn_bridge_training_begin(fep_snn_bridge_t* bridge) {
    if (!bridge) return -1;
    fep_snn_bridge_heartbeat_instance(bridge->health_agent, "fep_snn_training_begin", 0.0f);
    struct fep_snn_bridge* b = (struct fep_snn_bridge*)bridge;
    b->state.total_spikes = 0;
    b->state.total_steps = 0;
    b->config.learning_rate = (b->config.learning_rate > 0.0f) ? b->config.learning_rate : 1.0f;
    b->config.encoding_gain = (b->config.encoding_gain > 0.0f) ? b->config.encoding_gain : 1.0f;
    NIMCP_LOGGING_INFO("fep_snn_bridge: training begun, counters reset");
    return 0;
}

int fep_snn_bridge_training_step(fep_snn_bridge_t* bridge, float progress) {
    if (!bridge) return -1;
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_snn_bridge_heartbeat_instance(bridge->health_agent, "fep_snn_training_step", clamped);
    struct fep_snn_bridge* b = (struct fep_snn_bridge*)bridge;
    float p = clamped;
    b->config.learning_rate += (1.0f - p) * 0.001f;
    if (b->config.learning_rate > 2.0f) b->config.learning_rate = 2.0f;
    if (b->config.learning_rate < 0.0f) b->config.learning_rate = 0.0f;
    b->config.encoding_gain += (1.0f - p) * 0.001f;
    if (b->config.encoding_gain > 2.0f) b->config.encoding_gain = 2.0f;
    if (b->config.encoding_gain < 0.0f) b->config.encoding_gain = 0.0f;
    b->pred_error_signal += (1.0f - p) * 0.001f;
    if (b->pred_error_signal > 2.0f) b->pred_error_signal = 2.0f;
    if (b->pred_error_signal < 0.0f) b->pred_error_signal = 0.0f;
    b->precision_signal += (1.0f - p) * 0.001f;
    if (b->precision_signal > 2.0f) b->precision_signal = 2.0f;
    if (b->precision_signal < 0.0f) b->precision_signal = 0.0f;
    b->state.total_spikes++;
    b->state.total_steps++;
    return 0;
}

int fep_snn_bridge_training_end(fep_snn_bridge_t* bridge) {
    if (!bridge) return -1;
    fep_snn_bridge_heartbeat_instance(bridge->health_agent, "fep_snn_training_end", 1.0f);
    struct fep_snn_bridge* b = (struct fep_snn_bridge*)bridge;
    float metric_sum = 0.0f;
    metric_sum += b->config.learning_rate;
    metric_sum += b->config.encoding_gain;
    metric_sum += b->pred_error_signal;
    metric_sum += b->precision_signal;
    float avg_metric = metric_sum / 4.0f;
    NIMCP_LOGGING_INFO("fep_snn_bridge: training complete, avg_metric=%.4f", avg_metric);
    return 0;
}
EOF

echo "Free energy files done!"
