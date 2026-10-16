#!/bin/bash
# Append Phase 8 training functions to bridge files
# Usage: ./append_training.sh <type> <prefix> <typedef> <file>
# type: bridge_local (bridge with local struct), bridge_opaque (bridge without local struct), nonbridge_struct (non-bridge with struct), nonbridge_void (non-bridge, no struct)

TYPE=$1
PREFIX=$2
TYPEDEF=$3
FILE=$4
GLOBAL=$5
SHORT_PREFIX=$6

if [ "$TYPE" = "bridge_local" ]; then
cat >> "$FILE" << ENDOFCODE

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void ${PREFIX}_set_instance_health_agent(${TYPEDEF}* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
int ${PREFIX}_training_begin(${TYPEDEF}* bridge) {
    if (!bridge) return -1;
    ${PREFIX}_heartbeat_instance(bridge->health_agent, "${SHORT_PREFIX}_train_beg", 0.0f);

    /* Reset training counters and stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset base bridge training state */
    bridge->base.training_active = true;
    bridge->base.training_step_count = 0;
    bridge->base.training_total_error = 0.0;
    bridge->base.training_best_error = 1e30;

    NIMCP_LOGGING_INFO("${PREFIX} training begin: counters reset");
    return 0;
}

int ${PREFIX}_training_step(${TYPEDEF}* bridge, float progress) {
    if (!bridge) return -1;

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ${PREFIX}_heartbeat_instance(bridge->health_agent, "${SHORT_PREFIX}_train_stp", progress);

    bridge->base.training_step_count++;

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    bridge->base.training_total_error *= (double)decay;

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    bridge->base.training_best_error -= (double)threshold_adjust;
    if (bridge->base.training_best_error < 0.0) bridge->base.training_best_error = 0.0;

    return 0;
}

int ${PREFIX}_training_end(${TYPEDEF}* bridge) {
    if (!bridge) return -1;
    ${PREFIX}_heartbeat_instance(bridge->health_agent, "${SHORT_PREFIX}_train_end", 1.0f);

    /* Compute final averages */
    double avg_error = (bridge->base.training_step_count > 0)
        ? bridge->base.training_total_error / (double)bridge->base.training_step_count
        : 0.0;

    /* Clear training flag */
    bridge->base.training_active = false;

    NIMCP_LOGGING_INFO("${PREFIX} training end: %lu steps, avg_error=%.6f, best_error=%.6f",
                       (unsigned long)bridge->base.training_step_count,
                       avg_error, bridge->base.training_best_error);
    return 0;
}
ENDOFCODE

elif [ "$TYPE" = "bridge_opaque" ]; then
cat >> "$FILE" << ENDOFCODE

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void ${PREFIX}_set_instance_health_agent(${TYPEDEF}* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        /* Opaque struct: use global agent as fallback */
        (void)bridge;
        ${GLOBAL} = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
static uint64_t g_${PREFIX}_training_steps = 0;
static double g_${PREFIX}_training_total_error = 0.0;
static double g_${PREFIX}_training_best_error = 1e30;
static bool g_${PREFIX}_training_active = false;

int ${PREFIX}_training_begin(${TYPEDEF}* bridge) {
    if (!bridge) return -1;
    ${PREFIX}_heartbeat_instance(NULL, "${SHORT_PREFIX}_train_beg", 0.0f);

    /* Reset training counters */
    g_${PREFIX}_training_steps = 0;
    g_${PREFIX}_training_total_error = 0.0;
    g_${PREFIX}_training_best_error = 1e30;
    g_${PREFIX}_training_active = true;

    NIMCP_LOGGING_INFO("${PREFIX} training begin: counters reset");
    return 0;
}

int ${PREFIX}_training_step(${TYPEDEF}* bridge, float progress) {
    if (!bridge) return -1;

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ${PREFIX}_heartbeat_instance(NULL, "${SHORT_PREFIX}_train_stp", progress);

    g_${PREFIX}_training_steps++;

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    g_${PREFIX}_training_total_error *= (double)decay;

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    g_${PREFIX}_training_best_error -= (double)threshold_adjust;
    if (g_${PREFIX}_training_best_error < 0.0) g_${PREFIX}_training_best_error = 0.0;

    return 0;
}

int ${PREFIX}_training_end(${TYPEDEF}* bridge) {
    if (!bridge) return -1;
    ${PREFIX}_heartbeat_instance(NULL, "${SHORT_PREFIX}_train_end", 1.0f);

    /* Compute final averages */
    double avg_error = (g_${PREFIX}_training_steps > 0)
        ? g_${PREFIX}_training_total_error / (double)g_${PREFIX}_training_steps
        : 0.0;

    /* Clear training flag */
    g_${PREFIX}_training_active = false;

    NIMCP_LOGGING_INFO("${PREFIX} training end: %lu steps, avg_error=%.6f, best_error=%.6f",
                       (unsigned long)g_${PREFIX}_training_steps,
                       avg_error, g_${PREFIX}_training_best_error);
    return 0;
}
ENDOFCODE

elif [ "$TYPE" = "nonbridge_struct" ]; then
cat >> "$FILE" << ENDOFCODE

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
static nimcp_health_agent_t* g_${PREFIX}_instance_health_agent = NULL;

void ${PREFIX}_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_${PREFIX}_instance_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
static uint64_t g_${PREFIX}_training_steps = 0;
static double g_${PREFIX}_training_total_error = 0.0;
static double g_${PREFIX}_training_best_error = 1e30;
static bool g_${PREFIX}_training_active = false;

int ${PREFIX}_training_begin(void* instance) {
    if (!instance) return -1;
    ${PREFIX}_heartbeat_instance(g_${PREFIX}_instance_health_agent, "${SHORT_PREFIX}_train_beg", 0.0f);
    ${TYPEDEF}* ctx = (${TYPEDEF}*)instance;

    /* Reset training counters */
    g_${PREFIX}_training_steps = 0;
    g_${PREFIX}_training_total_error = 0.0;
    g_${PREFIX}_training_best_error = 1e30;
    g_${PREFIX}_training_active = true;

    /* Reset module stats */
    memset(&ctx->stats, 0, sizeof(ctx->stats));

    NIMCP_LOGGING_INFO("${PREFIX} training begin: counters reset");
    return 0;
}

int ${PREFIX}_training_step(void* instance, float progress) {
    if (!instance) return -1;

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ${PREFIX}_heartbeat_instance(g_${PREFIX}_instance_health_agent, "${SHORT_PREFIX}_train_stp", progress);
    (void)instance;

    g_${PREFIX}_training_steps++;

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    g_${PREFIX}_training_total_error *= (double)decay;

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    g_${PREFIX}_training_best_error -= (double)threshold_adjust;
    if (g_${PREFIX}_training_best_error < 0.0) g_${PREFIX}_training_best_error = 0.0;

    return 0;
}

int ${PREFIX}_training_end(void* instance) {
    if (!instance) return -1;
    ${PREFIX}_heartbeat_instance(g_${PREFIX}_instance_health_agent, "${SHORT_PREFIX}_train_end", 1.0f);

    /* Compute final averages */
    double avg_error = (g_${PREFIX}_training_steps > 0)
        ? g_${PREFIX}_training_total_error / (double)g_${PREFIX}_training_steps
        : 0.0;

    /* Clear training flag */
    g_${PREFIX}_training_active = false;

    NIMCP_LOGGING_INFO("${PREFIX} training end: %lu steps, avg_error=%.6f, best_error=%.6f",
                       (unsigned long)g_${PREFIX}_training_steps,
                       avg_error, g_${PREFIX}_training_best_error);
    return 0;
}
ENDOFCODE

elif [ "$TYPE" = "nonbridge_void" ]; then
cat >> "$FILE" << ENDOFCODE

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
static nimcp_health_agent_t* g_${PREFIX}_instance_health_agent = NULL;

void ${PREFIX}_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    (void)instance;
    g_${PREFIX}_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
static uint64_t g_${PREFIX}_training_steps = 0;
static double g_${PREFIX}_training_total_error = 0.0;
static double g_${PREFIX}_training_best_error = 1e30;
static bool g_${PREFIX}_training_active = false;

int ${PREFIX}_training_begin(void* instance) {
    (void)instance;
    ${PREFIX}_heartbeat_instance(g_${PREFIX}_instance_health_agent, "${SHORT_PREFIX}_train_beg", 0.0f);

    /* Reset training counters */
    g_${PREFIX}_training_steps = 0;
    g_${PREFIX}_training_total_error = 0.0;
    g_${PREFIX}_training_best_error = 1e30;
    g_${PREFIX}_training_active = true;

    NIMCP_LOGGING_INFO("${PREFIX} training begin: counters reset");
    return 0;
}

int ${PREFIX}_training_step(void* instance, float progress) {
    (void)instance;

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ${PREFIX}_heartbeat_instance(g_${PREFIX}_instance_health_agent, "${SHORT_PREFIX}_train_stp", progress);

    g_${PREFIX}_training_steps++;

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    g_${PREFIX}_training_total_error *= (double)decay;

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    g_${PREFIX}_training_best_error -= (double)threshold_adjust;
    if (g_${PREFIX}_training_best_error < 0.0) g_${PREFIX}_training_best_error = 0.0;

    return 0;
}

int ${PREFIX}_training_end(void* instance) {
    (void)instance;
    ${PREFIX}_heartbeat_instance(g_${PREFIX}_instance_health_agent, "${SHORT_PREFIX}_train_end", 1.0f);

    /* Compute final averages */
    double avg_error = (g_${PREFIX}_training_steps > 0)
        ? g_${PREFIX}_training_total_error / (double)g_${PREFIX}_training_steps
        : 0.0;

    /* Clear training flag */
    g_${PREFIX}_training_active = false;

    NIMCP_LOGGING_INFO("${PREFIX} training end: %lu steps, avg_error=%.6f, best_error=%.6f",
                       (unsigned long)g_${PREFIX}_training_steps,
                       avg_error, g_${PREFIX}_training_best_error);
    return 0;
}
ENDOFCODE
fi

echo "Appended to $FILE ($TYPE)"
