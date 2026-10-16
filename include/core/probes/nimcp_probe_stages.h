/**
 * @file nimcp_probe_stages.h
 * @brief Pipeline instrumentation — zero-overhead probe points in brain_decide/brain_learn_vector
 *
 * PROBE_STAGE() macro: evaluates to a single NULL pointer check when no probes are attached.
 * When probes are active, populates a stack-allocated context and writes to double buffer.
 * No heap allocations on the hot path.
 */
#ifndef NIMCP_PROBE_STAGES_H
#define NIMCP_PROBE_STAGES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct probe_registry;

/* Inline metric struct for stage context — avoids include cycle with nimcp_brain_probes.h */
typedef struct probe_stage_metric {
    char     key[64];
    int      type;   /* 0=float, 1=int, 2=string */
    union {
        float   f;
        int64_t i;
        char    s[64];
    } value;
    uint64_t timestamp_us;
} probe_stage_metric_t;

/* ============================================================================
 * Pipeline Stage IDs
 * ============================================================================ */

typedef enum {
    /* Inference pipeline (brain_decide) — 12 stages */
    PROBE_INF_PRE_FORWARD    = 0x00,  /**< Before forward pass */
    PROBE_INF_ADAPTIVE_FWD   = 0x01,  /**< After adaptive network forward */
    PROBE_INF_CNN_FWD        = 0x02,  /**< After CNN classifier */
    PROBE_INF_SNN_FWD        = 0x03,  /**< After SNN forward */
    PROBE_INF_LNN_GATE       = 0x04,  /**< After LNN gating */
    PROBE_INF_PREDICTION_ERR = 0x05,  /**< Prediction error computed */
    PROBE_INF_REASONING      = 0x06,  /**< After reasoning engine */
    PROBE_INF_EMOTION        = 0x07,  /**< After emotional tagging */
    PROBE_INF_ETHICS         = 0x08,  /**< After ethics evaluation */
    PROBE_INF_WORLD_PRIOR    = 0x09,  /**< After world prior check */
    PROBE_INF_DECISION       = 0x0A,  /**< Final decision output */
    PROBE_INF_COGNITIVE      = 0x0B,  /**< After cognitive dispatch */

    /* Training pipeline (brain_learn_vector) — 10 stages */
    PROBE_TRAIN_INPUT        = 0x10,  /**< Input validation */
    PROBE_TRAIN_ADAPTIVE     = 0x11,  /**< After adaptive_network_learn */
    PROBE_TRAIN_BPTT         = 0x12,  /**< After BPTT replay */
    PROBE_TRAIN_UTM          = 0x13,  /**< After UTM step */
    PROBE_TRAIN_SNN          = 0x14,  /**< After SNN step */
    PROBE_TRAIN_LNN          = 0x15,  /**< After LNN step */
    PROBE_TRAIN_PLASTICITY   = 0x16,  /**< After biological plasticity */
    PROBE_TRAIN_COGNITIVE    = 0x17,  /**< After cognitive subsystems */
    PROBE_TRAIN_ENGRAM       = 0x18,  /**< After engram encoding */
    PROBE_TRAIN_WM_CURRICULUM= 0x19,  /**< After world model step */

    PROBE_STAGE_COUNT        = 0x1A   /**< Total number of stages */
} probe_stage_id_t;

/* ============================================================================
 * Stage Context (stack-allocated by PROBE_STAGE macro)
 * ============================================================================ */

typedef struct probe_stage_context {
    probe_stage_id_t        stage_id;
    uint64_t                timestamp_us;
    probe_stage_metric_t    metrics[16];   /**< Stage-local metrics (max 16 per stage) */
    uint32_t                metric_count;
} probe_stage_context_t;

/* ============================================================================
 * Stage Recording (called by PROBE_STAGE macro)
 * ============================================================================ */

void probe_registry_record_stage(struct probe_registry* reg,
                                  const probe_stage_context_t* ctx);

/* ============================================================================
 * PROBE_STAGE Macro — Zero-Overhead Instrumentation
 *
 * When brain->probe_registry is NULL, compiles to a single pointer comparison.
 * When active, fills a stack-allocated context and records it.
 *
 * Usage:
 *   PROBE_STAGE(brain, PROBE_INF_ADAPTIVE_FWD, {
 *       PROBE_SET_FLOAT(&_ctx, "active_neurons", (float)active_neurons);
 *       PROBE_SET_FLOAT(&_ctx, "output_norm", output_norm);
 *   });
 * ============================================================================ */

#define PROBE_STAGE(brain, _probe_sid, code_block) do {                 \
    if ((brain)->probe_registry) {                                      \
        probe_stage_context_t _ctx;                                     \
        __builtin_memset(&_ctx, 0, sizeof(_ctx));                       \
        _ctx.stage_id = (_probe_sid);                                   \
        extern uint64_t nimcp_time_get_us(void);                        \
        _ctx.timestamp_us = nimcp_time_get_us();                        \
        { code_block }                                                  \
        probe_registry_record_stage(                                    \
            (struct probe_registry*)(brain)->probe_registry, &_ctx);    \
    }                                                                   \
} while (0)

/* Helper macros for filling stage context metrics */
#define PROBE_SET_FLOAT(ctx, key_str, val) do {                              \
    if ((ctx)->metric_count < 16) {                                          \
        probe_stage_metric_t* _m = &(ctx)->metrics[(ctx)->metric_count];     \
        __builtin_strncpy(_m->key, (key_str), 63);                          \
        _m->key[63] = '\0';                                                 \
        _m->type = 0; /* PROBE_METRIC_FLOAT */                              \
        _m->value.f = (val);                                                 \
        _m->timestamp_us = (ctx)->timestamp_us;                              \
        (ctx)->metric_count++;                                               \
    }                                                                        \
} while (0)

#define PROBE_SET_INT(ctx, key_str, val) do {                                \
    if ((ctx)->metric_count < 16) {                                          \
        probe_stage_metric_t* _m = &(ctx)->metrics[(ctx)->metric_count];     \
        __builtin_strncpy(_m->key, (key_str), 63);                          \
        _m->key[63] = '\0';                                                 \
        _m->type = 1; /* PROBE_METRIC_INT */                                \
        _m->value.i = (val);                                                 \
        _m->timestamp_us = (ctx)->timestamp_us;                              \
        (ctx)->metric_count++;                                               \
    }                                                                        \
} while (0)

#define PROBE_SET_STRING(ctx, key_str, val) do {                             \
    if ((ctx)->metric_count < 16) {                                          \
        probe_stage_metric_t* _m = &(ctx)->metrics[(ctx)->metric_count];     \
        __builtin_strncpy(_m->key, (key_str), 63);                          \
        _m->key[63] = '\0';                                                 \
        _m->type = 2; /* PROBE_METRIC_STRING */                             \
        __builtin_strncpy(_m->value.s, (val), 63);                          \
        _m->value.s[63] = '\0';                                             \
        _m->timestamp_us = (ctx)->timestamp_us;                              \
        (ctx)->metric_count++;                                               \
    }                                                                        \
} while (0)

/* ============================================================================
 * Stage Name Lookup
 * ============================================================================ */

const char* probe_stage_name(probe_stage_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PROBE_STAGES_H */
