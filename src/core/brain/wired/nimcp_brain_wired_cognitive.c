/**
 * @file nimcp_brain_wired_cognitive.c
 * @brief Wired-module group BRAIN_WIRED_COGNITIVE. See include/core/brain/nimcp_brain_wired.h.
 *
 * Modules driven by this group (dead-code audit 2026-11-24):
 *
 *   core/brain/factory/init/nimcp_brain_init_motor.c
 *       The motor cortex (brain->motor + substrate/thalamic/quantum bridges)
 *       was never created by any built init path, so brain_tick_sensorimotor's
 *       motor branch was dead. create() now calls
 *       nimcp_brain_factory_init_motor_subsystem() when brain->motor is NULL
 *       (opt-out: NIMCP_WIRED_COGNITIVE_MOTOR=0); tick() calls
 *       nimcp_brain_factory_update_motor_subsystem() every MOTOR_EVERY ticks
 *       (bio-inbox drain + substrate metabolic modulation); destroy() frees
 *       what this group created (brain_destroy never freed brain->motor).
 *
 *   middleware/training/nimcp_learning_signal_adapter.c
 *   middleware/training/nimcp_weight_update_adapter.c
 *   middleware/training/nimcp_training_event_adapter.c
 *   plasticity/adaptive/nimcp_weights.c   (network_stats_* part)
 *       A small three-factor sequence predictor over the real training input
 *       of brain_learn_vector. Features are pooled to CH channels; a CHxCH
 *       linear predictor guesses the next pooled input from the previous one.
 *       learning_signal_adapter turns (prediction, actual) into prediction
 *       error / reward / novelty / synchrony + eligibility; the combined signal
 *       gates weight_update_adapter's Hebbian rule with post = prediction
 *       error (i.e. a neuromodulated delta rule) and its homeostatic scaling.
 *       Threshold crossings of the learning signal are published as training
 *       events on training_event_adapter, whose handlers capture surprising
 *       inputs for rule induction and run consolidation. network_stats_*
 *       tracks the predictor's inference/learning time, loss and sparsity.
 *       Predictor weights persist in the <checkpoint>.wired_cognitive sidecar.
 *
 *   core/brain/learning/nimcp_reasoning_learning.c
 *       On each CONSOLIDATION_DUE event (every CONSOLIDATE_EVERY ticks) the
 *       captured inputs (binarized against per-channel running means, labelled
 *       by online perceptual prototypes) are passed to brain_learn_logical_rule,
 *       which induces "percept class <- feature conjunction" rules into
 *       brain->symbolic_logic. At most one KB rule per class (MAX_PROTO total)
 *       per brain lifetime; later captures refine a support/contradiction
 *       confidence for each rule.
 *
 *   cognitive/checkpointing/nimcp_checkpointing.c
 *       Not a cognitive-state store: it is a whole-brain periodic checkpoint
 *       manager. Driving its save loop would duplicate the main checkpoint, so
 *       only its read-only inspection is used: save() runs
 *       nimcp_checkpoint_verify()/nimcp_checkpoint_get_info() on the
 *       checkpoint the brain just wrote and reports the result in status_json.
 *
 * Not built by this group (reported to the lead): cognitive/mental_health/
 * interventions.c (superseded by the built nimcp_mental_health.c, duplicate
 * symbols) and cognitive/wellbeing/nimcp_wellbeing_homeostasis.c (superseded
 * by the built nimcp_wellbeing_enhanced.c, duplicate symbols).
 */
#include "core/brain/nimcp_brain_wired.h"
#include "core/brain/nimcp_brain_internal.h"

#include "core/brain/factory/init/nimcp_brain_init_motor.h"
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "core/brain/regions/motor/nimcp_motor_quantum_bridge.h"
#include "core/motor/nimcp_motor_substrate_bridge.h"
#include "core/motor/nimcp_motor_thalamic_bridge.h"
#include "core/brain/learning/nimcp_reasoning_learning.h"
#include "middleware/training/nimcp_learning_signal_adapter.h"
#include "middleware/training/nimcp_weight_update_adapter.h"
#include "middleware/training/nimcp_training_event_adapter.h"
#include "plasticity/adaptive/nimcp_weights.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Checkpoint inspection API (src/cognitive/checkpointing/nimcp_checkpointing.c).
 * Included last: it pulls in the public nimcp.h. */
#include "cognitive/checkpointing/nimcp_checkpointing.h"

/* ------------------------------------------------------------------------- */
/* Tunables                                                                   */
/* ------------------------------------------------------------------------- */
#define CH                 64u     /* pooled channels (also rule feature width) */
#define SIG_EVERY          4u      /* predictor / learning-signal cadence (ticks) */
#define MOTOR_EVERY        60u     /* motor substrate + bio inbox (~1 s at 16.6 ms) */
#define CAPTURE_EVERY      25u     /* baseline example capture cadence */
#define CONSOLIDATE_EVERY  600u    /* CONSOLIDATION_DUE event cadence */
#define WARMUP_SIGNALS     64u     /* learning signals before events may fire */
#define MAX_PROTO          8u
#define MAX_EX             32u
#define MIN_EX_PER_RULE    4u
#define PROTO_NEW_DIST     0.15f   /* mean sq. distance that founds a new prototype */
#define PROTO_LR           0.05f
#define HOMEO_TARGET       0.25f   /* target mean |prediction| */
#define W_CLAMP            1.0f
#define SIDECAR_MAGIC      0x31474F43u  /* "COG1" */
#define SIDECAR_VERSION    1u

typedef struct {
    brain_t brain;
    nimcp_mutex_t* lock;

    /* motor (nimcp_brain_init_motor.c) */
    bool motor_owned;
    bool motor_sub_owned, motor_thal_owned, motor_quant_owned;
    uint64_t motor_updates;

    /* training adapters + predictor */
    learning_signal_adapter_t lsa;
    weight_update_adapter_t   wua;
    weight_update_delta_t*    delta;
    training_event_adapter_t  tea;
    nimcp_network_stats_t*    stats;
    float* W[CH];                       /* predictor rows (float** for the adapter API) */
    float  prev[CH], cur[CH], pred[CH], err[CH];
    float  ch_mean[CH];
    bool   have_prev;
    learning_signal_t last_sig;
    float  mse_ema, abs_pred_ema;
    uint64_t ticks, signals, nonfinite_skips, weight_updates, homeostasis_calls;
    uint64_t evt_counts[TRAINING_EVENT_CUSTOM + 1];

    /* reasoning (nimcp_reasoning_learning.c) */
    float    proto[MAX_PROTO][CH];
    uint32_t proto_n[MAX_PROTO];
    uint32_t n_protos;
    float    ex_feat[MAX_EX][CH];
    uint8_t  ex_label[MAX_EX];
    uint32_t ex_count, ex_pos;
    bool     rule_in_kb[MAX_PROTO];     /* runtime only: the KB is not persisted */
    uint64_t rule_mask[MAX_PROTO];
    float    rule_conf[MAX_PROTO];
    uint32_t rule_support[MAX_PROTO], rule_contra[MAX_PROTO];
    uint32_t rules_added, induction_rounds, induction_failures;

    /* checkpoint inspection (nimcp_checkpointing.c) */
    uint64_t ckpt_verified, ckpt_verify_failed, ckpt_last_size;
} cog_state_t;

static cog_state_t* cog_get(struct brain_struct* brain) {
    return brain ? (cog_state_t*)brain->wired[BRAIN_WIRED_COGNITIVE] : NULL;
}

static int env_off(const char* key) {
    const char* v = getenv(key);
    return v && v[0] == '0';
}

/* ------------------------------------------------------------------------- */
/* Reasoning: prototypes, example capture, rule induction                     */
/* ------------------------------------------------------------------------- */
static uint32_t cog_classify(cog_state_t* s, const float* x) {
    uint32_t best = 0;
    float best_d = INFINITY;
    for (uint32_t p = 0; p < s->n_protos; p++) {
        float d = 0.0f;
        for (uint32_t i = 0; i < CH; i++) {
            float t = x[i] - s->proto[p][i];
            d += t * t;
        }
        d /= (float)CH;
        if (d < best_d) { best_d = d; best = p; }
    }
    if (s->n_protos < MAX_PROTO && (s->n_protos == 0 || best_d > PROTO_NEW_DIST)) {
        best = s->n_protos++;
        memcpy(s->proto[best], x, sizeof(float) * CH);
        s->proto_n[best] = 1;
        return best;
    }
    for (uint32_t i = 0; i < CH; i++) {
        s->proto[best][i] += PROTO_LR * (x[i] - s->proto[best][i]);
    }
    s->proto_n[best]++;
    return best;
}

static void cog_capture_example(cog_state_t* s) {
    uint32_t label = cog_classify(s, s->cur);
    float* f = s->ex_feat[s->ex_pos];
    uint64_t bits = 0;
    for (uint32_t i = 0; i < CH; i++) {
        f[i] = (s->cur[i] > s->ch_mean[i]) ? 1.0f : 0.0f;
        if (f[i] > 0.5f) bits |= (1ull << i);
    }
    s->ex_label[s->ex_pos] = (uint8_t)label;
    s->ex_pos = (s->ex_pos + 1) % MAX_EX;
    if (s->ex_count < MAX_EX) s->ex_count++;

    /* Refine the rule already induced for this class: does the input still
     * satisfy the rule's premise conjunction? */
    if (s->rule_in_kb[label]) {
        bool match = (bits & s->rule_mask[label]) == s->rule_mask[label];
        if (match) s->rule_support[label]++; else s->rule_contra[label]++;
        uint32_t tot = s->rule_support[label] + s->rule_contra[label];
        s->rule_conf[label] = (float)s->rule_support[label] / (float)tot;
    }
}

static void cog_induce_rules(cog_state_t* s) {
    brain_t brain = s->brain;
    if (!brain || !brain->symbolic_logic || s->ex_count == 0) {
        return;
    }
    uint32_t per_label[MAX_PROTO] = {0};
    for (uint32_t e = 0; e < s->ex_count; e++) per_label[s->ex_label[e]]++;

    logical_example_t ex[MAX_EX];
    uint32_t k = 0;
    for (uint32_t e = 0; e < s->ex_count; e++) {
        uint32_t l = s->ex_label[e];
        if (s->rule_in_kb[l] || per_label[l] < MIN_EX_PER_RULE) continue;
        memset(&ex[k], 0, sizeof(ex[k]));
        ex[k].features = s->ex_feat[e];
        ex[k].num_features = CH;
        snprintf(ex[k].label, sizeof(ex[k].label), "percept_c%u", l);
        ex[k].confidence = 1.0f;
        snprintf(ex[k].context, sizeof(ex[k].context), "wired_cognitive");
        k++;
    }
    if (k == 0) {
        return;
    }
    s->induction_rounds++;

    learned_rule_t** rules = NULL;
    int nr = 0;
    if (!brain_learn_logical_rule(brain, ex, k, &rules, &nr, false)) {
        s->induction_failures++;
        return;
    }
    for (int r = 0; r < nr; r++) {
        learned_rule_t* rule = rules ? rules[r] : NULL;
        if (!rule) continue;
        unsigned l = 0;
        if (sscanf(rule->rule_name, "rule_percept_c%u", &l) == 1 && l < MAX_PROTO &&
            rule->support_count > 0 && !s->rule_in_kb[l]) {
            /* Premise conjunction = channels with >= 70% support in this class
             * (same threshold brain_learn_logical_rule uses). */
            uint32_t cnt[CH] = {0}, n = 0;
            for (uint32_t e = 0; e < s->ex_count; e++) {
                if (s->ex_label[e] != l) continue;
                n++;
                for (uint32_t i = 0; i < CH; i++) cnt[i] += (s->ex_feat[e][i] > 0.5f);
            }
            uint64_t mask = 0;
            for (uint32_t i = 0; i < CH; i++)
                if (n > 0 && (float)cnt[i] >= 0.7f * (float)n) mask |= (1ull << i);
            s->rule_in_kb[l] = true;
            s->rule_mask[l] = mask;
            s->rule_conf[l] = isfinite(rule->confidence) ? rule->confidence : 0.0f;
            s->rules_added++;
        }
        learned_rule_destroy(rule);
    }
    nimcp_free(rules);
}

/* ------------------------------------------------------------------------- */
/* Training-event handlers (user_data = cog_state_t*; run inside tick, locked) */
/* ------------------------------------------------------------------------- */
static void on_capture_event(const training_event_data_t* ev, void* ud) {
    cog_state_t* s = (cog_state_t*)ud;
    if (!s || !ev) return;
    s->evt_counts[ev->type]++;
    cog_capture_example(s);
}

static void on_count_event(const training_event_data_t* ev, void* ud) {
    cog_state_t* s = (cog_state_t*)ud;
    if (!s || !ev) return;
    s->evt_counts[ev->type]++;
}

static void on_consolidation(const training_event_data_t* ev, void* ud) {
    cog_state_t* s = (cog_state_t*)ud;
    if (!s || !ev) return;
    s->evt_counts[ev->type]++;

    /* Homeostatic scaling of the predictor toward HOMEO_TARGET mean |pred|.
     * The adapter scales by target/actual clamped to [0.5, 2]; pass a
     * softened rate so one call moves the gain by (target/actual)^0.1. */
    float actual = s->abs_pred_ema;
    if (isfinite(actual) && actual > 1e-6f) {
        float soft = HOMEO_TARGET * powf(actual / HOMEO_TARGET, 0.1f);
        if (isfinite(soft) && soft > 0.0f &&
            weight_update_adapter_apply_homeostasis(s->wua, s->W, HOMEO_TARGET, soft)) {
            s->homeostasis_calls++;
        }
    }
    cog_induce_rules(s);
}

static void cog_publish(cog_state_t* s, training_event_type_t type, float strength) {
    training_event_data_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.timestamp = nimcp_time_get_us();
    ev.strength = isfinite(strength) ? strength : 0.0f;
    (void)training_event_adapter_publish(s->tea, &ev);
}

/* ------------------------------------------------------------------------- */
/* Predictor step (every SIG_EVERY ticks)                                     */
/* ------------------------------------------------------------------------- */
static void cog_signal_step(cog_state_t* s, float dt_ms) {
    uint64_t t0 = nimcp_time_get_us();

    /* prediction = tanh(W . prev) */
    float abs_sum = 0.0f;
    for (uint32_t j = 0; j < CH; j++) {
        float a = 0.0f;
        for (uint32_t i = 0; i < CH; i++) a += s->W[i][j] * s->prev[i];
        s->pred[j] = tanhf(a);
        abs_sum += fabsf(s->pred[j]);
    }
    uint64_t t1 = nimcp_time_get_us();
    network_stats_record_inference(s->stats, (float)(t1 - t0));

    float dt_s = dt_ms * (float)SIG_EVERY / 1000.0f;
    (void)learning_signal_adapter_update_eligibility(s->lsa, s->cur, CH, dt_s);
    learning_signal_t sig;
    if (!learning_signal_adapter_compute(s->lsa, s->pred, s->cur, CH, t1, &sig)) {
        return;
    }
    if (!isfinite(sig.combined_signal) || !isfinite(sig.prediction_error)) {
        s->nonfinite_skips++;
        return;
    }
    s->last_sig = sig;
    s->signals++;

    /* Neuromodulated delta rule: Hebbian(pre = prev, post = error) gated by
     * the combined learning signal (clamped so a novelty burst can't blow
     * the step up). */
    for (uint32_t j = 0; j < CH; j++) s->err[j] = s->cur[j] - s->pred[j];
    learning_signal_t gate = sig;
    gate.combined_signal = fminf(fmaxf(sig.combined_signal, 0.0f), 2.0f);
    if (weight_update_adapter_compute(s->wua, s->prev, s->err, &gate, t1, s->delta)) {
        for (uint32_t i = 0; i < CH; i++) {
            for (uint32_t j = 0; j < CH; j++) {
                float w = s->W[i][j] + s->delta->delta_weights[i][j];
                if (!isfinite(w)) continue;
                s->W[i][j] = fminf(fmaxf(w, -W_CLAMP), W_CLAMP);
            }
        }
        s->weight_updates++;
    }

    float mse = sig.prediction_error;
    float abs_pred = abs_sum / (float)CH;
    if (s->signals == 1) {
        s->mse_ema = mse;
        s->abs_pred_ema = abs_pred;
    } else {
        s->mse_ema += 0.02f * (mse - s->mse_ema);
        s->abs_pred_ema += 0.02f * (abs_pred - s->abs_pred_ema);
    }

    uint32_t active = 0;
    for (uint32_t i = 0; i < CH; i++) active += (fabsf(s->cur[i]) > 0.1f);
    network_stats_update_sparsity(s->stats, active, CH);
    network_stats_record_learning(s->stats, (float)(nimcp_time_get_us() - t0), mse);

    /* Training events from learning-signal threshold crossings. */
    if (s->signals > WARMUP_SIGNALS) {
        if (mse > 2.0f * s->mse_ema && mse > 1e-4f)
            cog_publish(s, TRAINING_EVENT_ERROR_COMPUTED, mse);
        if (sig.synchrony_signal > 0.8f)
            cog_publish(s, TRAINING_EVENT_SYNCHRONY_HIGH, sig.synchrony_signal);
        if (sig.reward_signal > 0.5f)
            cog_publish(s, TRAINING_EVENT_REWARD_RECEIVED, sig.reward_signal);
        if (sig.novelty_signal > 4.0f * s->mse_ema * s->mse_ema && sig.novelty_signal > 1e-6f)
            cog_publish(s, TRAINING_EVENT_PATTERN_DETECTED, sig.novelty_signal);
    }
}

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* ------------------------------------------------------------------------- */
static void cog_free(cog_state_t* s) {
    if (!s) return;
    if (s->tea)   training_event_adapter_destroy(s->tea);
    if (s->delta) weight_update_delta_destroy(s->delta);
    if (s->wua)   weight_update_adapter_destroy(s->wua);
    if (s->lsa)   learning_signal_adapter_destroy(s->lsa);
    if (s->stats) network_stats_destroy(s->stats);
    for (uint32_t i = 0; i < CH; i++) nimcp_free(s->W[i]);
    if (s->lock) nimcp_mutex_free(s->lock);
    nimcp_free(s);
}

int brain_wired_cognitive_create(struct brain_struct* brain) {
    if (!brain) {
        return -1;
    }
    if (brain->wired[BRAIN_WIRED_COGNITIVE]) {
        return 0;
    }
    cog_state_t* s = (cog_state_t*)nimcp_calloc(1, sizeof(cog_state_t));
    if (!s) {
        return -1;
    }
    s->brain = brain;
    s->lock = nimcp_mutex_create(NULL);

    learning_signal_adapter_config_t lc = learning_signal_adapter_default_config();
    lc.num_channels = CH;
    lc.learning_rate = 1.0f;          /* gate magnitude; step size lives in wua */
    s->lsa = learning_signal_adapter_create(&lc);

    weight_update_adapter_config_t wc = weight_update_adapter_default_config();
    wc.num_pre = CH;
    wc.num_post = CH;
    wc.rule = PLASTICITY_HEBBIAN;     /* post = prediction error -> delta rule */
    wc.learning_rate = 0.02f;
    wc.weight_min = -0.01f;           /* with normalization: per-step |dw| clamp */
    wc.weight_max = 0.01f;
    wc.enable_normalization = true;
    wc.enable_homeostasis = true;
    s->wua = weight_update_adapter_create(&wc);
    s->delta = weight_update_delta_create(CH, CH);

    training_event_adapter_config_t tc = training_event_adapter_default_config(NULL);
    s->tea = training_event_adapter_create(&tc);
    s->stats = network_stats_create();

    bool ok = s->lock && s->lsa && s->wua && s->delta && s->tea && s->stats;
    for (uint32_t i = 0; ok && i < CH; i++) {
        s->W[i] = (float*)nimcp_calloc(CH, sizeof(float));
        if (!s->W[i]) ok = false;
    }
    if (ok) {
        ok = training_event_adapter_register_handler(s->tea, TRAINING_EVENT_ERROR_COMPUTED, on_capture_event, s) &&
             training_event_adapter_register_handler(s->tea, TRAINING_EVENT_PATTERN_DETECTED, on_capture_event, s) &&
             training_event_adapter_register_handler(s->tea, TRAINING_EVENT_SYNCHRONY_HIGH, on_count_event, s) &&
             training_event_adapter_register_handler(s->tea, TRAINING_EVENT_REWARD_RECEIVED, on_count_event, s) &&
             training_event_adapter_register_handler(s->tea, TRAINING_EVENT_CONSOLIDATION_DUE, on_consolidation, s);
    }
    if (!ok) {
        cog_free(s);
        return -1;
    }
    /* Identity-ish start: predict "next input = current input" weakly. */
    for (uint32_t i = 0; i < CH; i++) s->W[i][i] = 0.5f;

    /* Motor cortex: only if nothing else created it. */
    if (!brain->motor && !env_off("NIMCP_WIRED_COGNITIVE_MOTOR")) {
        if (nimcp_brain_factory_init_motor_subsystem(brain) && brain->motor) {
            s->motor_owned = true;
            s->motor_sub_owned = brain->motor_substrate_bridge != NULL;
            s->motor_thal_owned = brain->motor_thalamic_bridge != NULL;
            s->motor_quant_owned = brain->motor_quantum_bridge != NULL;
        }
    }

    brain->wired[BRAIN_WIRED_COGNITIVE] = s;
    return 0;
}

void brain_wired_cognitive_tick(struct brain_struct* brain, const float* f, uint32_t n, float dt_ms) {
    cog_state_t* s = cog_get(brain);
    if (!s) {
        return;
    }
    s->ticks++;
    if (!(dt_ms > 0.0f) || !isfinite(dt_ms)) dt_ms = 16.6f;

    /* Motor upkeep runs outside our lock (it takes motor/bridge locks). */
    if (s->motor_owned && (s->ticks % MOTOR_EVERY) == 0) {
        uint64_t dt_us = (uint64_t)(dt_ms * 1000.0f) * MOTOR_EVERY;
        if (nimcp_brain_factory_update_motor_subsystem(brain, dt_us)) s->motor_updates++;
    }

    if (!f || n == 0) {
        return;
    }
    nimcp_mutex_lock(s->lock);

    /* Pool the training input into CH channels (mean of contiguous chunks),
     * squashed into [-1, 1]. Reject the step on any non-finite input. */
    float pooled[CH];
    for (uint32_t c = 0; c < CH; c++) {
        uint32_t a = (uint32_t)(((uint64_t)c * n) / CH);
        uint32_t b = (uint32_t)(((uint64_t)(c + 1) * n) / CH);
        if (b <= a) b = a + 1;
        if (b > n) b = n;
        float sum = 0.0f;
        uint32_t cnt = 0;
        for (uint32_t i = a; i < b; i++) { sum += f[i]; cnt++; }
        pooled[c] = cnt ? tanhf(sum / (float)cnt) : 0.0f;
        if (!isfinite(pooled[c])) {
            s->nonfinite_skips++;
            nimcp_mutex_unlock(s->lock);
            return;
        }
    }
    if (s->have_prev) memcpy(s->prev, s->cur, sizeof(s->prev));
    memcpy(s->cur, pooled, sizeof(s->cur));
    for (uint32_t c = 0; c < CH; c++) s->ch_mean[c] += 0.01f * (s->cur[c] - s->ch_mean[c]);

    if (!s->have_prev) {
        memcpy(s->prev, s->cur, sizeof(s->prev));
        s->have_prev = true;
    } else if ((s->ticks % SIG_EVERY) == 0) {
        cog_signal_step(s, dt_ms);
    }

    if ((s->ticks % CAPTURE_EVERY) == 0) {
        cog_capture_example(s);
    }
    if ((s->ticks % CONSOLIDATE_EVERY) == 0) {
        cog_publish(s, TRAINING_EVENT_CONSOLIDATION_DUE, 1.0f);
    }
    (void)training_event_adapter_process_events(s->tea);

    nimcp_mutex_unlock(s->lock);
}

/* Sidecar <path>.wired_cognitive: header, predictor, prototypes, rule stats. */
typedef struct {
    uint32_t magic, version, ch, max_proto;
    uint64_t ticks, signals, weight_updates;
    float mse_ema, abs_pred_ema;
    uint32_t n_protos, rules_added;
} cog_sidecar_hdr_t;

int brain_wired_cognitive_save(struct brain_struct* brain, const char* path) {
    cog_state_t* s = cog_get(brain);
    if (!s || !path) {
        return 0;
    }
    /* Inspect the main checkpoint the brain just wrote (read-only). */
    if (nimcp_checkpoint_verify(path)) {
        nimcp_checkpoint_info_t info;
        memset(&info, 0, sizeof(info));
        if (nimcp_checkpoint_get_info(path, &info)) s->ckpt_last_size = (uint64_t)info.size_bytes;
        s->ckpt_verified++;
    } else {
        s->ckpt_verify_failed++;
    }

    char p[1024];
    snprintf(p, sizeof(p), "%s.wired_cognitive", path);
    char tmp[1040];
    snprintf(tmp, sizeof(tmp), "%s.tmp", p);
    FILE* fp = fopen(tmp, "wb");
    if (!fp) {
        return -1;
    }
    nimcp_mutex_lock(s->lock);
    cog_sidecar_hdr_t h = {
        .magic = SIDECAR_MAGIC, .version = SIDECAR_VERSION, .ch = CH, .max_proto = MAX_PROTO,
        .ticks = s->ticks, .signals = s->signals, .weight_updates = s->weight_updates,
        .mse_ema = s->mse_ema, .abs_pred_ema = s->abs_pred_ema,
        .n_protos = s->n_protos, .rules_added = s->rules_added,
    };
    bool ok = fwrite(&h, sizeof(h), 1, fp) == 1;
    for (uint32_t i = 0; ok && i < CH; i++) ok = fwrite(s->W[i], sizeof(float), CH, fp) == CH;
    ok = ok && fwrite(s->ch_mean, sizeof(float), CH, fp) == CH;
    ok = ok && fwrite(s->proto, sizeof(s->proto), 1, fp) == 1;
    ok = ok && fwrite(s->proto_n, sizeof(s->proto_n), 1, fp) == 1;
    ok = ok && fwrite(s->rule_mask, sizeof(s->rule_mask), 1, fp) == 1;
    ok = ok && fwrite(s->rule_conf, sizeof(s->rule_conf), 1, fp) == 1;
    ok = ok && fwrite(s->rule_support, sizeof(s->rule_support), 1, fp) == 1;
    ok = ok && fwrite(s->rule_contra, sizeof(s->rule_contra), 1, fp) == 1;
    nimcp_mutex_unlock(s->lock);
    if (fclose(fp) != 0) ok = false;
    if (!ok || rename(tmp, p) != 0) {
        remove(tmp);
        return -1;
    }
    return 0;
}

static bool all_finite(const float* v, size_t n, float lim) {
    for (size_t i = 0; i < n; i++) {
        if (!isfinite(v[i]) || fabsf(v[i]) > lim) return false;
    }
    return true;
}

int brain_wired_cognitive_load(struct brain_struct* brain, const char* path) {
    cog_state_t* s = cog_get(brain);
    if (!s || !path) {
        return 1;
    }
    char p[1024];
    snprintf(p, sizeof(p), "%s.wired_cognitive", path);
    FILE* fp = fopen(p, "rb");
    if (!fp) {
        return 1;   /* no sidecar: fresh state */
    }
    cog_sidecar_hdr_t h;
    float W[CH][CH], ch_mean[CH], proto[MAX_PROTO][CH], rule_conf[MAX_PROTO];
    uint32_t proto_n[MAX_PROTO], rule_support[MAX_PROTO], rule_contra[MAX_PROTO];
    uint64_t rule_mask[MAX_PROTO];
    bool ok = fread(&h, sizeof(h), 1, fp) == 1 &&
              h.magic == SIDECAR_MAGIC && h.version == SIDECAR_VERSION &&
              h.ch == CH && h.max_proto == MAX_PROTO && h.n_protos <= MAX_PROTO;
    ok = ok && fread(W, sizeof(W), 1, fp) == 1;
    ok = ok && fread(ch_mean, sizeof(ch_mean), 1, fp) == 1;
    ok = ok && fread(proto, sizeof(proto), 1, fp) == 1;
    ok = ok && fread(proto_n, sizeof(proto_n), 1, fp) == 1;
    ok = ok && fread(rule_mask, sizeof(rule_mask), 1, fp) == 1;
    ok = ok && fread(rule_conf, sizeof(rule_conf), 1, fp) == 1;
    ok = ok && fread(rule_support, sizeof(rule_support), 1, fp) == 1;
    ok = ok && fread(rule_contra, sizeof(rule_contra), 1, fp) == 1;
    fclose(fp);
    ok = ok && all_finite(&W[0][0], CH * CH, W_CLAMP) && all_finite(ch_mean, CH, 1.0f) &&
         all_finite(&proto[0][0], MAX_PROTO * CH, 1.0f) && all_finite(rule_conf, MAX_PROTO, 1.0f) &&
         isfinite(h.mse_ema) && isfinite(h.abs_pred_ema);
    if (!ok) {
        return -1;
    }
    nimcp_mutex_lock(s->lock);
    for (uint32_t i = 0; i < CH; i++) memcpy(s->W[i], W[i], sizeof(float) * CH);
    memcpy(s->ch_mean, ch_mean, sizeof(ch_mean));
    memcpy(s->proto, proto, sizeof(proto));
    memcpy(s->proto_n, proto_n, sizeof(proto_n));
    memcpy(s->rule_mask, rule_mask, sizeof(rule_mask));
    memcpy(s->rule_conf, rule_conf, sizeof(rule_conf));
    memcpy(s->rule_support, rule_support, sizeof(rule_support));
    memcpy(s->rule_contra, rule_contra, sizeof(rule_contra));
    s->n_protos = h.n_protos;
    s->mse_ema = h.mse_ema;
    s->abs_pred_ema = h.abs_pred_ema;
    /* The symbolic-logic KB is not persisted by the brain, so rules are
     * re-induced into the fresh KB on the next consolidation. */
    memset(s->rule_in_kb, 0, sizeof(s->rule_in_kb));
    s->ex_count = 0;
    s->ex_pos = 0;
    nimcp_mutex_unlock(s->lock);
    return 0;
}

void brain_wired_cognitive_destroy(struct brain_struct* brain) {
    cog_state_t* s = cog_get(brain);
    if (!s) {
        return;
    }
    if (s->motor_owned) {
        if (s->motor_quant_owned && brain->motor_quantum_bridge) {
            motor_quantum_bridge_destroy((motor_quantum_bridge_t*)brain->motor_quantum_bridge);
            brain->motor_quantum_bridge = NULL;
        }
        if (s->motor_thal_owned && brain->motor_thalamic_bridge) {
            motor_thalamic_bridge_destroy((motor_thalamic_bridge_t*)brain->motor_thalamic_bridge);
            brain->motor_thalamic_bridge = NULL;
        }
        if (s->motor_sub_owned && brain->motor_substrate_bridge) {
            motor_substrate_bridge_destroy((motor_substrate_bridge_t*)brain->motor_substrate_bridge);
            brain->motor_substrate_bridge = NULL;
        }
        brain->motor_enabled = false;
        if (brain->motor) {
            motor_destroy((motor_adapter_t*)brain->motor);
            brain->motor = NULL;
        }
    }
    brain->wired[BRAIN_WIRED_COGNITIVE] = NULL;
    cog_free(s);
}

int brain_wired_cognitive_status_json(struct brain_struct* brain, char* buf, uint32_t cap) {
    cog_state_t* s = cog_get(brain);
    if (!buf || cap == 0) {
        return 0;
    }
    if (!s) {
        return snprintf(buf, cap, "{\"created\":false}");
    }
    nimcp_mutex_lock(s->lock);
    float avg_conf = 0.0f;
    uint32_t nrules = 0;
    for (uint32_t l = 0; l < MAX_PROTO; l++) {
        if (s->rule_in_kb[l]) { avg_conf += s->rule_conf[l]; nrules++; }
    }
    if (nrules) avg_conf /= (float)nrules;
    float loss = s->stats ? s->stats->current_loss : 0.0f;
    float sparsity = s->stats ? s->stats->running_sparsity : 0.0f;
    int w = snprintf(buf, cap,
        "{\"created\":true,\"ticks\":%llu,"
        "\"motor\":{\"owned\":%s,\"present\":%s,\"substrate_bridge\":%s,\"thalamic_bridge\":%s,"
        "\"quantum_bridge\":%s,\"updates\":%llu},"
        "\"predictor\":{\"signals\":%llu,\"weight_updates\":%llu,\"homeostasis\":%llu,"
        "\"mse\":%.6f,\"mse_ema\":%.6f,\"abs_pred_ema\":%.4f,\"sparsity\":%.4f,"
        "\"reward\":%.4f,\"novelty\":%.6f,\"synchrony\":%.4f,\"eligibility\":%.4f,"
        "\"nonfinite_skips\":%llu},"
        "\"events\":{\"error\":%llu,\"pattern\":%llu,\"synchrony\":%llu,\"reward\":%llu,"
        "\"consolidation\":%llu},"
        "\"reasoning\":{\"logic_present\":%s,\"prototypes\":%u,\"examples\":%u,"
        "\"rules_added\":%u,\"rules_live\":%u,\"avg_rule_conf\":%.3f,\"rounds\":%u,"
        "\"failures\":%u},"
        "\"checkpoint\":{\"verified\":%llu,\"verify_failed\":%llu,\"last_size\":%llu}}",
        (unsigned long long)s->ticks,
        s->motor_owned ? "true" : "false", brain->motor ? "true" : "false",
        brain->motor_substrate_bridge ? "true" : "false",
        brain->motor_thalamic_bridge ? "true" : "false",
        brain->motor_quantum_bridge ? "true" : "false",
        (unsigned long long)s->motor_updates,
        (unsigned long long)s->signals, (unsigned long long)s->weight_updates,
        (unsigned long long)s->homeostasis_calls,
        (double)loss, (double)s->mse_ema, (double)s->abs_pred_ema, (double)sparsity,
        (double)s->last_sig.reward_signal, (double)s->last_sig.novelty_signal,
        (double)s->last_sig.synchrony_signal, (double)s->last_sig.eligibility,
        (unsigned long long)s->nonfinite_skips,
        (unsigned long long)s->evt_counts[TRAINING_EVENT_ERROR_COMPUTED],
        (unsigned long long)s->evt_counts[TRAINING_EVENT_PATTERN_DETECTED],
        (unsigned long long)s->evt_counts[TRAINING_EVENT_SYNCHRONY_HIGH],
        (unsigned long long)s->evt_counts[TRAINING_EVENT_REWARD_RECEIVED],
        (unsigned long long)s->evt_counts[TRAINING_EVENT_CONSOLIDATION_DUE],
        brain->symbolic_logic ? "true" : "false", s->n_protos, s->ex_count,
        s->rules_added, nrules, (double)avg_conf, s->induction_rounds, s->induction_failures,
        (unsigned long long)s->ckpt_verified, (unsigned long long)s->ckpt_verify_failed,
        (unsigned long long)s->ckpt_last_size);
    nimcp_mutex_unlock(s->lock);
    return w;
}
