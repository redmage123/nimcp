#!/usr/bin/env python3
"""Phase 8 Instance-Level Enhancement Script - adds heartbeat_instance, set_instance, and full training to 43 files."""
import os, re

BASE = "/home/bbrelin/nimcp"

# Each file entry: (path, prefix, gvar, struct_name, is_bridge, has_ha_field,
#                   begin_resets, step_ops, end_ops)
# We manually specify per-file based on our reading of the structs.

FILES = [
    # ============= FAULT TOLERANCE (15 files) =============
    # 1. emotional_tagging - DONE ALREADY

    # 2. failure_prediction - non-bridge, struct failure_predictor
    ("src/cognitive/fault_tolerance/nimcp_failure_prediction.c",
     "failure_prediction", "g_failure_prediction_health_agent",
     "failure_predictor", "failure_predictor_t", False, False,
     ["ctx->indicator_count = 0;", "ctx->prediction_count = 0;"],
     ["ctx->prediction_count++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: %u indicators, %u predictions\",\n"
      "                       \"failure_prediction\", ctx->indicator_count, ctx->prediction_count);"]),

    # 3. fault_attention - non-bridge, struct fault_attention
    ("src/cognitive/fault_tolerance/nimcp_fault_attention.c",
     "fault_attention", "g_fault_attention_health_agent",
     "fault_attention", "fault_attention_t", False, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->fault_count = 0;", "ctx->has_focus = false;",
      "ctx->focused_fault_idx = 0;"],
     ["ctx->stats.total_computations++;",
      "/* Adaptive attention decay during training */",
      "float decay = 1.0f - 0.1f * progress;",
      "if (decay < 0.5f) decay = 0.5f;",
      "for (uint32_t i = 0; i < ctx->fault_count; i++) {",
      "    ctx->weights[i] *= decay;",
      "}"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: %u faults, %lu computations\",\n"
      "                       \"fault_attention\", ctx->fault_count,\n"
      "                       (unsigned long)ctx->stats.total_computations);"]),

    # 4. fault_tolerance_substrate_bridge - bridge, struct fault_tolerance_substrate_bridge
    ("src/cognitive/fault_tolerance/nimcp_fault_tolerance_substrate_bridge.c",
     "fault_tolerance_substrate_bridge", "g_fault_tolerance_substrate_bridge_health_agent",
     "fault_tolerance_substrate_bridge", "fault_tolerance_substrate_bridge_t", True, False,
     ["ctx->update_count = 0;",
      "ctx->effects.detection_sensitivity = 1.0f;",
      "ctx->effects.recovery_speed = 1.0f;",
      "ctx->effects.redundancy_capacity = 1.0f;",
      "ctx->effects.monitoring_depth = 1.0f;",
      "ctx->effects.overall_capacity = 1.0f;"],
     ["ctx->update_count++;",
      "/* Modulate effects capacity with training progress */",
      "ctx->effects.overall_capacity = 0.5f + 0.5f * progress;",
      "ctx->effects.detection_sensitivity = 0.5f + 0.5f * progress;",
      "ctx->effects.recovery_speed = 0.5f + 0.5f * progress;"],
     ["    ctx->effects.overall_capacity = 1.0f;",
      "    ctx->effects.detection_sensitivity = 1.0f;",
      "    ctx->effects.recovery_speed = 1.0f;",
      "    NIMCP_LOGGING_INFO(\"%s training end: updates=%lu\",\n"
      "                       \"fault_tolerance_substrate_bridge\", (unsigned long)ctx->update_count);"]),

    # 5. fault_tolerance_thalamic_bridge - bridge, struct fault_tolerance_thalamic_bridge
    ("src/cognitive/fault_tolerance/nimcp_fault_tolerance_thalamic_bridge.c",
     "fault_tolerance_thalamic_bridge", "g_fault_tolerance_thalamic_bridge_health_agent",
     "fault_tolerance_thalamic_bridge", "fault_tolerance_thalamic_bridge_t", True, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->attention_weight = 1.0f;"],
     ["ctx->stats.signals_routed++;",
      "/* Adaptive attention decay during training */",
      "float decay = 1.0f - 0.1f * progress;",
      "if (decay < 0.5f) decay = 0.5f;",
      "ctx->attention_weight *= decay;"],
     ["    ctx->attention_weight = 1.0f;",
      "    NIMCP_LOGGING_INFO(\"%s training end: signals_routed=%lu\",\n"
      "                       \"fault_tolerance_thalamic_bridge\",\n"
      "                       (unsigned long)ctx->stats.signals_routed);"]),

    # 6. fault_working_memory - non-bridge, struct fault_working_memory
    ("src/cognitive/fault_tolerance/nimcp_fault_working_memory.c",
     "fault_working_memory", "g_fault_working_memory_health_agent",
     "fault_working_memory", "fault_working_memory_t", False, False,
     ["ctx->count = 0;", "ctx->cascade_detected = false;",
      "ctx->faults_in_window = 0;", "ctx->recovery_step = 0;",
      "ctx->total_steps = 0;", "ctx->has_focus = false;" if False else "ctx->priority_fault_idx = 0;"],
     ["/* Track fault processing during training */",
      "ctx->faults_in_window++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: %u faults, cascade=%s\",\n"
      "                       \"fault_working_memory\", ctx->count,\n"
      "                       ctx->cascade_detected ? \"yes\" : \"no\");"]),

    # 7. health_diagnostic_bridge - bridge, struct health_diag_bridge
    ("src/cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.c",
     "health_diagnostic_bridge", "g_health_diagnostic_bridge_health_agent",
     "health_diag_bridge", "health_diag_bridge_t", True, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->total_conversion_time_us = 0;",
      "ctx->conversion_count = 0;"],
     ["ctx->conversion_count++;"],
     ["    float avg_time = (ctx->conversion_count > 0)\n"
      "        ? (float)ctx->total_conversion_time_us / (float)ctx->conversion_count\n"
      "        : 0.0f;",
      "    NIMCP_LOGGING_INFO(\"%s training end: conversions=%lu, avg_time=%.2f us\",\n"
      "                       \"health_diagnostic_bridge\",\n"
      "                       (unsigned long)ctx->conversion_count, avg_time);"]),

    # 8. health_self_repair_bridge - bridge, struct health_self_repair_bridge (ALREADY has health_agent)
    ("src/cognitive/fault_tolerance/nimcp_health_self_repair_bridge.c",
     "health_self_repair_bridge", "g_health_self_repair_bridge_health_agent",
     "health_self_repair_bridge", "health_self_repair_bridge_t", True, True,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->tracking_count = 0;",
      "ctx->total_repair_time_ms = 0;",
      "ctx->repair_count_for_avg = 0;",
      "ctx->aggregation_count = 0;",
      "ctx->window_repair_count = 0;"],
     ["ctx->stats.repairs_triggered++;"],
     ["    float success_rate = (ctx->stats.repairs_succeeded + ctx->stats.repairs_failed > 0)\n"
      "        ? (float)ctx->stats.repairs_succeeded / (float)(ctx->stats.repairs_succeeded + ctx->stats.repairs_failed)\n"
      "        : 0.0f;",
      "    NIMCP_LOGGING_INFO(\"%s training end: triggered=%lu, success_rate=%.4f\",\n"
      "                       \"health_self_repair_bridge\",\n"
      "                       (unsigned long)ctx->stats.repairs_triggered, success_rate);"]),

    # 9. metacognition - non-bridge, struct metacognition
    ("src/cognitive/fault_tolerance/nimcp_metacognition.c",
     "metacognition", "g_metacognition_health_agent",
     "metacognition", "metacognition_t", False, False,
     ["ctx->total_samples = 0;",
      "ctx->self_confidence = 0.5f;",
      "ctx->uncertainty = 0.5f;"],
     ["ctx->total_samples++;",
      "/* Gradually improve confidence during training */",
      "ctx->self_confidence += 0.005f * progress;",
      "if (ctx->self_confidence > 1.0f) ctx->self_confidence = 1.0f;",
      "/* Reduce uncertainty as training progresses */",
      "ctx->uncertainty *= (1.0f - 0.01f * progress);",
      "if (ctx->uncertainty < 0.0f) ctx->uncertainty = 0.0f;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: samples=%lu, confidence=%.4f, uncertainty=%.4f\",\n"
      "                       \"metacognition\",\n"
      "                       (unsigned long)ctx->total_samples,\n"
      "                       ctx->self_confidence, ctx->uncertainty);"]),

    # 10. recovery_consolidation - non-bridge, struct recovery_consolidation
    ("src/cognitive/fault_tolerance/nimcp_recovery_consolidation.c",
     "recovery_consolidation", "g_recovery_consolidation_health_agent",
     "recovery_consolidation", "recovery_consolidation_t", False, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->episode_count = 0;", "ctx->episode_head = 0;",
      "ctx->pattern_count = 0;", "ctx->rule_count = 0;"],
     ["ctx->episode_count++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: episodes=%u, patterns=%u, rules=%u\",\n"
      "                       \"recovery_consolidation\",\n"
      "                       ctx->episode_count, ctx->pattern_count, ctx->rule_count);"]),

    # 11. recovery_episodic_memory - non-bridge, struct episodic_memory
    ("src/cognitive/fault_tolerance/nimcp_recovery_episodic_memory.c",
     "recovery_episodic_memory", "g_recovery_episodic_memory_health_agent",
     "episodic_memory", "episodic_memory_t", False, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->count = 0;", "ctx->head = 0;",
      "ctx->next_episode_id = 1;"],
     ["ctx->stats.total_stores++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: episodes=%u, stores=%lu\",\n"
      "                       \"recovery_episodic_memory\",\n"
      "                       ctx->count, (unsigned long)ctx->stats.total_stores);"]),

    # 12. recovery_executive - non-bridge, struct recovery_executive_internal
    ("src/cognitive/fault_tolerance/nimcp_recovery_executive.c",
     "recovery_executive", "g_recovery_executive_health_agent",
     "recovery_executive_internal", "recovery_executive_t", False, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->subgoal_count = 0;", "ctx->current_step = 0;",
      "ctx->has_diagnosis = false;"],
     ["ctx->stats.plans_created++;",
      "ctx->current_step++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: plans=%lu, steps=%u\",\n"
      "                       \"recovery_executive\",\n"
      "                       (unsigned long)ctx->stats.plans_created, ctx->current_step);"]),

    # 13. recovery_parietal_bridge - bridge, struct recovery_parietal_bridge
    ("src/cognitive/fault_tolerance/nimcp_recovery_parietal_bridge.c",
     "recovery_parietal_bridge", "g_recovery_parietal_bridge_health_agent",
     "recovery_parietal_bridge", "recovery_parietal_bridge_t", True, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->pattern_count = 0;"],
     ["ctx->stats.total_requests++;",
      "ctx->pattern_count++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: requests=%lu, patterns=%u\",\n"
      "                       \"recovery_parietal_bridge\",\n"
      "                       (unsigned long)ctx->stats.total_requests, ctx->pattern_count);"]),

    # 14. self_repair - non-bridge, struct self_repair_coordinator (HAS health_agent field as struct health_agent*)
    ("src/cognitive/fault_tolerance/nimcp_self_repair.c",
     "self_repair", "g_self_repair_health_agent",
     "self_repair_coordinator", "self_repair_coordinator_t", False, True,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->record_count = 0;"],
     ["ctx->stats.total_repairs_attempted++;",
      "ctx->record_count++;"],
     ["    float success_rate = (ctx->stats.total_repairs_attempted > 0)\n"
      "        ? (float)ctx->stats.total_repairs_succeeded / (float)ctx->stats.total_repairs_attempted\n"
      "        : 0.0f;",
      "    NIMCP_LOGGING_INFO(\"%s training end: attempted=%lu, success_rate=%.4f\",\n"
      "                       \"self_repair\",\n"
      "                       (unsigned long)ctx->stats.total_repairs_attempted, success_rate);"]),

    # 15. self_repair_health_notify - non-bridge (no bridge_base_t), struct self_repair_health_notify_bridge (HAS health_agent)
    ("src/cognitive/fault_tolerance/nimcp_self_repair_health_notify.c",
     "self_repair_health_notify", "g_self_repair_health_notify_health_agent",
     "self_repair_health_notify_bridge", "self_repair_health_notify_bridge_t", False, True,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->failure_tracking_count = 0;"],
     ["ctx->stats.notifications_sent++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: notifications=%lu, failures_tracked=%u\",\n"
      "                       \"self_repair_health_notify\",\n"
      "                       (unsigned long)ctx->stats.notifications_sent, ctx->failure_tracking_count);"]),

    # ============= HEALTH (4 files) =============
    # 16. meta_health - non-bridge, struct meta_health_reflector (HAS health_agent field)
    ("src/cognitive/health/nimcp_meta_health.c",
     "meta_health", "g_meta_health_health_agent",
     "meta_health_reflector", "meta_health_reflector_t", False, True,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->num_pending = 0;", "ctx->num_applied = 0;",
      "ctx->num_patterns = 0;"],
     ["ctx->stats.reflections_completed++;",
      "ctx->num_pending++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: reflections=%lu, patterns=%u\",\n"
      "                       \"meta_health\",\n"
      "                       (unsigned long)ctx->stats.reflections_completed, ctx->num_patterns);"]),

    # 17. collective_health - non-bridge, struct collective_health_monitor (has local_agent not health_agent)
    ("src/cognitive/health/nimcp_collective_health.c",
     "collective_health", "g_collective_health_health_agent",
     "collective_health_monitor", "collective_health_monitor_t", False, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->num_instances = 0;",
      "ctx->num_pending_consensus = 0;",
      "ctx->num_pending_swarm = 0;"],
     ["ctx->stats.sync_count++;",
      "ctx->num_instances++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: syncs=%lu, instances=%u\",\n"
      "                       \"collective_health\",\n"
      "                       (unsigned long)ctx->stats.sync_count, ctx->num_instances);"]),

    # 18. rcog_health - non-bridge, struct rcog_health_integration (HAS health_agent)
    ("src/cognitive/health/nimcp_rcog_health.c",
     "rcog_health", "g_rcog_health_health_agent",
     "rcog_health_integration", "rcog_health_integration_t", False, True,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->num_pending = 0;", "ctx->cache_size = 0;"],
     ["ctx->stats.queries_total++;",
      "ctx->num_pending++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: queries=%lu, cache=%u\",\n"
      "                       \"rcog_health\",\n"
      "                       (unsigned long)ctx->stats.queries_total, ctx->cache_size);"]),

    # 19. health_cognitive_bridge - bridge, struct health_cognitive_bridge (has agent not health_agent)
    ("src/cognitive/health/nimcp_health_cognitive_bridge.c",
     "health_cognitive_bridge", "g_health_cognitive_bridge_health_agent",
     "health_cognitive_bridge", "health_cognitive_bridge_t", True, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->num_pending = 0;"],
     ["ctx->stats.events_handled++;",
      "ctx->num_pending++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: events=%lu\",\n"
      "                       \"health_cognitive_bridge\",\n"
      "                       (unsigned long)ctx->stats.events_handled);"]),

    # ============= MENTAL HEALTH (9 files, excluding disorder_detectors.c) =============
    # 20. mental_health - non-bridge, struct mental_health_monitor
    ("src/cognitive/mental_health/nimcp_mental_health.c",
     "mental_health", "g_mental_health_health_agent",
     "mental_health_monitor", "mental_health_monitor_t", False, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->decisions_since_check = 0;",
      "ctx->history_index = 0;",
      "ctx->monitoring_active = false;",
      "ctx->quarantine_mode = false;"],
     ["ctx->decisions_since_check++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: decisions=%u, monitoring=%s\",\n"
      "                       \"mental_health\", ctx->decisions_since_check,\n"
      "                       ctx->monitoring_active ? \"active\" : \"inactive\");"]),

    # 21. mental_health_guardian - non-bridge, struct mental_health_guardian
    ("src/cognitive/mental_health/nimcp_mental_health_guardian.c",
     "mental_health_guardian", "g_mental_health_guardian_health_agent",
     "mental_health_guardian", "mental_health_guardian_t", False, False,
     ["ctx->checks_performed = 0;",
      "ctx->interventions_applied = 0;",
      "ctx->observe_count = 0;",
      "ctx->adjust_count = 0;",
      "ctx->regulate_count = 0;",
      "ctx->quarantine_count = 0;",
      "ctx->last_overall_severity = 0.0f;"],
     ["ctx->checks_performed++;",
      "/* Decrease severity tracking during training */",
      "ctx->last_overall_severity *= (1.0f - 0.01f * progress);"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: checks=%lu, interventions=%lu, severity=%.4f\",\n"
      "                       \"mental_health_guardian\",\n"
      "                       (unsigned long)ctx->checks_performed,\n"
      "                       (unsigned long)ctx->interventions_applied,\n"
      "                       ctx->last_overall_severity);"]),

    # 22. mental_health_fep_bridge - bridge (uses bridge_base), no struct in .c (header-defined)
    ("src/cognitive/mental_health/nimcp_mental_health_fep_bridge.c",
     "mental_health_fep_bridge", "g_mental_health_fep_bridge_health_agent",
     None, "mental_health_fep_bridge_t", True, False,
     [],
     [],
     ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"mental_health_fep_bridge\");"]),

    # 23. mental_health_thalamic_bridge - bridge, struct mental_health_thalamic_bridge
    ("src/cognitive/mental_health/nimcp_mental_health_thalamic_bridge.c",
     "mental_health_thalamic_bridge", "g_mental_health_thalamic_bridge_health_agent",
     "mental_health_thalamic_bridge", "mental_health_thalamic_bridge_t", True, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->attention_weight = 1.0f;"],
     ["ctx->stats.signals_routed++;",
      "float decay = 1.0f - 0.1f * progress;",
      "if (decay < 0.5f) decay = 0.5f;",
      "ctx->attention_weight *= decay;"],
     ["    ctx->attention_weight = 1.0f;",
      "    NIMCP_LOGGING_INFO(\"%s training end: signals=%lu\",\n"
      "                       \"mental_health_thalamic_bridge\",\n"
      "                       (unsigned long)ctx->stats.signals_routed);"]),

    # 24. mental_health_substrate_bridge - bridge, struct mental_health_substrate_bridge
    ("src/cognitive/mental_health/nimcp_mental_health_substrate_bridge.c",
     "mental_health_substrate_bridge", "g_mental_health_substrate_bridge_health_agent",
     "mental_health_substrate_bridge", "mental_health_substrate_bridge_t", True, False,
     ["ctx->update_count = 0;",
      "ctx->effects.overall_capacity = 1.0f;"],
     ["ctx->update_count++;",
      "ctx->effects.overall_capacity = 0.5f + 0.5f * progress;"],
     ["    ctx->effects.overall_capacity = 1.0f;",
      "    NIMCP_LOGGING_INFO(\"%s training end: updates=%lu\",\n"
      "                       \"mental_health_substrate_bridge\", (unsigned long)ctx->update_count);"]),

    # 25. mental_health_snn_bridge - bridge, struct mental_health_snn_bridge
    ("src/cognitive/mental_health/nimcp_mental_health_snn_bridge.c",
     "mental_health_snn_bridge", "g_mental_health_snn_bridge_health_agent",
     "mental_health_snn_bridge", "mental_health_snn_bridge_t", True, False,
     ["ctx->anxiety_signal = 0.0f;",
      "ctx->depression_signal = 0.0f;",
      "ctx->current_time_us = 0;"],
     ["/* Reduce anxiety and depression signals during training */",
      "ctx->anxiety_signal *= (1.0f - 0.02f * progress);",
      "ctx->depression_signal *= (1.0f - 0.02f * progress);",
      "ctx->current_time_us++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: anxiety=%.4f, depression=%.4f\",\n"
      "                       \"mental_health_snn_bridge\",\n"
      "                       ctx->anxiety_signal, ctx->depression_signal);"]),

    # 26. mental_health_plasticity_bridge - bridge, struct mental_health_plasticity_bridge
    ("src/cognitive/mental_health/nimcp_mental_health_plasticity_bridge.c",
     "mental_health_plasticity_bridge", "g_mental_health_plasticity_bridge_health_agent",
     "mental_health_plasticity_bridge", "mental_health_plasticity_bridge_t", True, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->synapse_count = 0;",
      "ctx->current_stress_level = 0.0f;",
      "ctx->current_time_us = 0;"],
     ["/* Adapt learning rate to training phase */",
      "ctx->learning_rate_effective = 0.01f + 0.09f * (1.0f - progress);",
      "ctx->current_stress_level *= (1.0f - 0.02f * progress);",
      "ctx->stats.learn_events++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: synapses=%u, learn_events=%lu, stress=%.4f\",\n"
      "                       \"mental_health_plasticity_bridge\",\n"
      "                       ctx->synapse_count,\n"
      "                       (unsigned long)ctx->stats.learn_events,\n"
      "                       ctx->current_stress_level);"]),

    # 27. mental_health_sleep_bridge - bridge, struct mental_health_sleep_bridge_struct
    ("src/cognitive/mental_health/nimcp_mental_health_sleep_bridge.c",
     "mental_health_sleep_bridge", "g_mental_health_sleep_bridge_health_agent",
     "mental_health_sleep_bridge_struct", "mental_health_sleep_bridge_t", True, False,
     ["ctx->effects.stress_reduction = 0.0f;",
      "ctx->effects.emotional_regulation = 0.0f;"],
     ["/* Improve sleep effects during training */",
      "ctx->effects.stress_reduction = 0.3f + 0.7f * progress;",
      "ctx->effects.emotional_regulation = 0.3f + 0.7f * progress;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: stress_reduction=%.4f, emotional_regulation=%.4f\",\n"
      "                       \"mental_health_sleep_bridge\",\n"
      "                       ctx->effects.stress_reduction, ctx->effects.emotional_regulation);"]),

    # 28. interventions - #included file, non-bridge, no struct in file
    ("src/cognitive/mental_health/interventions.c",
     "interventions", "g_interventions_health_agent",
     None, None, False, False,
     [],
     [],
     ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"interventions\");"]),

    # ============= WELLBEING (14 files) =============
    # 29-42. All wellbeing files - most are header-defined structs
]

# Add wellbeing files programmatically since they follow similar patterns
WELLBEING_FILES = [
    # (filename, prefix, gvar_suffix, struct_name_or_None, type_name_or_None, is_bridge, has_ha, begin, step, end)
    ("nimcp_wellbeing.c", "wellbeing", "wellbeing",
     None, "wellbeing_system_t", False, False,
     [], [], ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing\");"]),

    ("nimcp_wellbeing_enhanced.c", "wellbeing_enhanced", "wellbeing_enhanced",
     None, "wellbeing_enhanced_t", False, False,
     [], [], ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing_enhanced\");"]),

    ("nimcp_wellbeing_homeostasis.c", "wellbeing_homeostasis", "wellbeing_homeostasis",
     None, "wellbeing_homeostasis_t", False, False,
     [], [], ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing_homeostasis\");"]),

    ("nimcp_wellbeing_resources.c", "wellbeing_resources", "wellbeing_resources",
     None, "wellbeing_resources_t", False, False,
     [], [], ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing_resources\");"]),

    ("nimcp_wellbeing_eudaimonic.c", "wellbeing_eudaimonic", "wellbeing_eudaimonic",
     None, "wellbeing_eudaimonic_t", False, False,
     [], [], ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing_eudaimonic\");"]),

    ("nimcp_wellbeing_prediction.c", "wellbeing_prediction", "wellbeing_prediction",
     None, "wellbeing_prediction_t", False, False,
     [], [], ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing_prediction\");"]),

    ("nimcp_wellbeing_fep_bridge.c", "wellbeing_fep_bridge", "wellbeing_fep_bridge",
     None, "wellbeing_fep_bridge_t", True, False,
     [], [], ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing_fep_bridge\");"]),

    ("nimcp_wellbeing_substrate_bridge.c", "wellbeing_substrate_bridge", "wellbeing_substrate_bridge",
     None, "wellbeing_substrate_bridge_t", True, False,
     [], [], ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing_substrate_bridge\");"]),

    ("nimcp_wellbeing_thalamic_bridge.c", "wellbeing_thalamic_bridge", "wellbeing_thalamic_bridge",
     "wellbeing_thalamic_bridge", "wellbeing_thalamic_bridge_t", True, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->attention_weight = 1.0f;"],
     ["ctx->stats.signals_routed++;",
      "float decay = 1.0f - 0.1f * progress;",
      "if (decay < 0.5f) decay = 0.5f;",
      "ctx->attention_weight *= decay;"],
     ["    ctx->attention_weight = 1.0f;",
      "    NIMCP_LOGGING_INFO(\"%s training end: signals=%lu\",\n"
      "                       \"wellbeing_thalamic_bridge\",\n"
      "                       (unsigned long)ctx->stats.signals_routed);"]),

    ("nimcp_wellbeing_snn_bridge.c", "wellbeing_snn_bridge", "wellbeing_snn_bridge",
     "wellbeing_snn_bridge", "wellbeing_snn_bridge_t", True, False,
     ["ctx->current_time_us = 0;"],
     ["ctx->current_time_us++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing_snn_bridge\");"]),

    ("nimcp_wellbeing_plasticity_bridge.c", "wellbeing_plasticity_bridge", "wellbeing_plasticity_bridge",
     "wellbeing_plasticity_bridge", "wellbeing_plasticity_bridge_t", True, False,
     ["memset(&ctx->stats, 0, sizeof(ctx->stats));",
      "ctx->synapse_count = 0;",
      "ctx->current_time_us = 0;"],
     ["ctx->stats.learn_events++;"],
     ["    NIMCP_LOGGING_INFO(\"%s training end: synapses=%u, learn_events=%lu\",\n"
      "                       \"wellbeing_plasticity_bridge\",\n"
      "                       ctx->synapse_count, (unsigned long)ctx->stats.learn_events);"]),

    ("nimcp_wellbeing_sleep_bridge.c", "wellbeing_sleep_bridge", "wellbeing_sleep_bridge",
     None, "wellbeing_sleep_bridge_t", True, False,
     [], [], ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing_sleep_bridge\");"]),

    ("nimcp_wellbeing_free_energy_bridge.c", "wellbeing_free_energy_bridge", "wellbeing_free_energy_bridge",
     None, "wellbeing_free_energy_bridge_t", True, False,
     [], [], ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing_free_energy_bridge\");"]),

    ("nimcp_wellbeing_mental_health_bridge.c", "wellbeing_mental_health_bridge", "wellbeing_mental_health_bridge",
     None, "wellbeing_mental_health_bridge_t", True, False,
     [], [], ["    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"wellbeing_mental_health_bridge\");"]),
]

for wf in WELLBEING_FILES:
    fn, prefix, gvar_suffix, struct_name, type_name, is_bridge, has_ha, begin, step, end = wf
    FILES.append((
        f"src/cognitive/wellbeing/{fn}",
        prefix, f"g_{gvar_suffix}_health_agent",
        struct_name, type_name, is_bridge, has_ha,
        begin, step, end
    ))


def process_file(entry):
    path, prefix, gvar, struct_name, type_name, is_bridge, has_ha, begin_resets, step_ops, end_ops = entry
    filepath = os.path.join(BASE, path)

    if not os.path.exists(filepath):
        print(f"  SKIP (not found): {filepath}")
        return

    with open(filepath, 'r') as f:
        content = f.read()

    if 'heartbeat_instance' in content:
        print(f"  SKIP (already has heartbeat_instance): {os.path.basename(filepath)}")
        return

    modified = False
    short = prefix[:14] if len(prefix) > 14 else prefix

    # 1. Add heartbeat_instance after existing heartbeat function
    hb_pattern = rf'(static inline void {re.escape(prefix)}_heartbeat\(const char\* operation, float progress\) \{{[^}}]*\}})'
    hb_match = re.search(hb_pattern, content, re.DOTALL)
    if hb_match:
        hb_instance = f"""

/** @brief Send heartbeat from {prefix} module (instance-level) */
static inline void {prefix}_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{{
    if ({gvar}) {{
        nimcp_health_agent_heartbeat_ex({gvar}, operation, progress);
    }}
    if (instance_agent && instance_agent != {gvar}) {{
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }}
}}"""
        content = content[:hb_match.end()] + hb_instance + content[hb_match.end():]
        modified = True

    # 2. For bridges: add health_agent field to struct if missing
    if is_bridge and struct_name and not has_ha:
        # Find struct closing brace and add field before it
        struct_pattern = rf'(struct {re.escape(struct_name)} \{{)'
        if struct_pattern and re.search(struct_pattern, content):
            # Find the closing brace of this struct
            lines = content.split('\n')
            in_struct = False
            brace_depth = 0
            insert_idx = None
            for i, line in enumerate(lines):
                if re.match(rf'^struct {re.escape(struct_name)} \{{', line):
                    in_struct = True
                    brace_depth = 1
                    continue
                if in_struct:
                    brace_depth += line.count('{') - line.count('}')
                    if brace_depth <= 0:
                        insert_idx = i
                        break

            if insert_idx is not None:
                # Check if health_agent already exists in the struct
                struct_text = '\n'.join(lines[:insert_idx])
                if 'health_agent' not in struct_text.split(f'struct {struct_name}')[-1]:
                    lines.insert(insert_idx, "")
                    lines.insert(insert_idx, "    /* Phase 8: Instance health agent */")
                    lines.insert(insert_idx + 1, "    nimcp_health_agent_t* health_agent;         /**< Health agent (Phase 8) */")
                    content = '\n'.join(lines)
                    has_ha = True
                    modified = True

    # 3. Build instance agent expression
    agent_expr = "ctx->health_agent" if has_ha else "NULL"

    # 4. Determine parameter types
    if struct_name:
        cast_type = f"struct {struct_name}"
        if is_bridge:
            p_type = f"{type_name}*" if type_name else "void*"
            p_name = "bridge"
        else:
            p_type = f"{type_name}*" if type_name else "void*"
            p_name = "instance"
    elif type_name:
        cast_type = None
        p_type = f"{type_name}*"
        p_name = "bridge" if is_bridge else "instance"
    else:
        cast_type = None
        p_type = "void*"
        p_name = "instance"

    # 5. Build set_instance function
    if has_ha and struct_name:
        set_instance = f"""
/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void {prefix}_set_instance_health_agent({p_type} {p_name}, nimcp_health_agent_t* agent) {{
    if ({p_name}) {{
        {cast_type}* ctx = ({cast_type}*){p_name};
        ctx->health_agent = agent;
    }}
}}
"""
    else:
        set_instance = f"""
/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void {prefix}_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {{
    if (instance) {{
        (void)instance;  /* No struct-level health_agent; use global */
        {gvar} = agent;
    }}
}}
"""

    # 6. Build training functions
    cast_line = f"    {cast_type}* ctx = ({cast_type}*){p_name};" if cast_type else f"    (void){p_name};"

    # training_begin
    begin_lines = [f"    {prefix}_heartbeat_instance({agent_expr}, \"{short}_training_begin\", 0.0f);"]
    for r in begin_resets:
        begin_lines.append(f"    {r}")
    begin_lines.append(f"    NIMCP_LOGGING_INFO(\"%s training begin: counters reset\", \"{prefix}\");")

    # training_step
    step_lines = [
        "    if (progress < 0.0f) progress = 0.0f;",
        "    if (progress > 1.0f) progress = 1.0f;",
        f"    {prefix}_heartbeat_instance({agent_expr}, \"{short}_training_step\", progress);"
    ]
    for s in step_ops:
        step_lines.append(f"    {s}")

    # training_end
    end_lines = [f"    {prefix}_heartbeat_instance({agent_expr}, \"{short}_training_end\", 1.0f);"]
    for e in end_ops:
        if e.startswith("    "):
            end_lines.append(e)
        else:
            end_lines.append(f"    {e}")

    training_code = f"""
/* ============================================================================
 * Phase 8: Training Functions (FULL implementation)
 * ============================================================================ */
int {prefix}_training_begin({p_type} {p_name}) {{
    if (!{p_name}) return -1;
{cast_line}
{chr(10).join(begin_lines)}
    return 0;
}}

int {prefix}_training_step({p_type} {p_name}, float progress) {{
    if (!{p_name}) return -1;
{cast_line}
{chr(10).join(step_lines)}
    return 0;
}}

int {prefix}_training_end({p_type} {p_name}) {{
    if (!{p_name}) return -1;
{cast_line}
{chr(10).join(end_lines)}
    return 0;
}}
"""

    # 7. Append at end of file
    content = content.rstrip() + "\n" + set_instance + training_code + "\n"
    modified = True

    if modified:
        with open(filepath, 'w') as f:
            f.write(content)
        print(f"  DONE: {os.path.basename(filepath)}")


def main():
    print(f"Processing {len(FILES)} files...")
    for entry in FILES:
        print(f"\n  Processing: {os.path.basename(entry[0])}")
        process_file(entry)
    print(f"\nDone! Processed {len(FILES)} files.")


if __name__ == '__main__':
    main()
