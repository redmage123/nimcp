/**
 * @file nimcp_probe_builtins.c
 * @brief Built-in sampler functions that wrap existing metrics structs
 *
 * Each sampler reads from one of the brain's inline metric structs
 * (network_metrics, cognitive_stats, training_dashboard, stats) and
 * populates probe_metric_t key-value pairs.
 */

#include "core/probes/nimcp_brain_probes.h"
#include "core/brain/nimcp_brain_internal.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Generic Sampler — reports module existence and basic stats
 * ============================================================================ */

void probe_generic_sampler(
    void* module_ptr, uint16_t module_id, brain_t brain,
    probe_metric_t* metrics, uint32_t* count, void* user_data)
{
    (void)brain; (void)user_data;
    if (!metrics || !count) return;
    uint32_t max = *count;
    uint32_t n = 0;
    extern uint64_t nimcp_time_get_us(void);
    uint64_t ts = nimcp_time_get_us();

    const char* name = probe_module_name(module_id);
    if (n < max) {
        strncpy(metrics[n].key, "initialized", PROBE_KEY_LEN - 1);
        metrics[n].type = PROBE_METRIC_INT;
        metrics[n].value.i = module_ptr ? 1 : 0;
        metrics[n].timestamp_us = ts;
        n++;
    }
    if (n < max && name) {
        strncpy(metrics[n].key, "module_name", PROBE_KEY_LEN - 1);
        metrics[n].type = PROBE_METRIC_STRING;
        strncpy(metrics[n].value.s, name, PROBE_STRING_LEN - 1);
        metrics[n].timestamp_us = ts;
        n++;
    }

    *count = n;
}

/* ============================================================================
 * Helper: set metric values (avoids repetitive boilerplate)
 * ============================================================================ */

static inline void set_float(probe_metric_t* m, const char* key, float val, uint64_t ts) {
    strncpy(m->key, key, PROBE_KEY_LEN - 1);
    m->key[PROBE_KEY_LEN - 1] = '\0';
    m->type = PROBE_METRIC_FLOAT;
    m->value.f = val;
    m->timestamp_us = ts;
}

static inline void set_int(probe_metric_t* m, const char* key, int64_t val, uint64_t ts) {
    strncpy(m->key, key, PROBE_KEY_LEN - 1);
    m->key[PROBE_KEY_LEN - 1] = '\0';
    m->type = PROBE_METRIC_INT;
    m->value.i = val;
    m->timestamp_us = ts;
}

static inline void set_string(probe_metric_t* m, const char* key, const char* val, uint64_t ts) {
    strncpy(m->key, key, PROBE_KEY_LEN - 1);
    m->key[PROBE_KEY_LEN - 1] = '\0';
    m->type = PROBE_METRIC_STRING;
    strncpy(m->value.s, val ? val : "", PROBE_STRING_LEN - 1);
    m->value.s[PROBE_STRING_LEN - 1] = '\0';
    m->timestamp_us = ts;
}

/* ============================================================================
 * Network Metrics Sampler
 * ============================================================================ */

static void sample_network_metrics(
    void* module_ptr, uint16_t module_id, brain_t brain,
    probe_metric_t* metrics, uint32_t* count, void* user_data)
{
    (void)module_ptr; (void)module_id; (void)user_data;
    if (!brain || !metrics || !count) return;
    uint32_t max = *count;
    uint32_t n = 0;
    extern uint64_t nimcp_time_get_us(void);
    uint64_t ts = nimcp_time_get_us();

    #define EMIT_F(key, val) do { if (n < max) set_float(&metrics[n++], key, val, ts); } while(0)
    #define EMIT_I(key, val) do { if (n < max) set_int(&metrics[n++], key, val, ts); } while(0)

    EMIT_F("ann.ema_loss",   brain->network_metrics.ema_ann_loss);
    EMIT_F("ann.last_loss",  brain->network_metrics.last_ann_loss);
    EMIT_I("ann.steps",      (int64_t)brain->network_metrics.ann_steps);
    EMIT_F("cnn.ema_loss",   brain->network_metrics.ema_cnn_loss);
    EMIT_I("cnn.steps",      (int64_t)brain->network_metrics.cnn_steps);
    EMIT_F("snn.ema_loss",   brain->network_metrics.ema_snn_loss);
    EMIT_F("snn.last_loss",  brain->network_metrics.last_snn_loss);
    EMIT_I("snn.steps",      (int64_t)brain->network_metrics.snn_steps);
    EMIT_F("lnn.ema_loss",   brain->network_metrics.ema_lnn_loss);
    EMIT_I("lnn.steps",      (int64_t)brain->network_metrics.lnn_steps);
    EMIT_F("hnn.energy",     brain->network_metrics.hnn_energy);
    EMIT_F("hnn.deviation",  brain->network_metrics.hnn_energy_deviation);
    EMIT_I("hnn.active",     brain->network_metrics.hnn_active ? 1 : 0);
    EMIT_F("fno.audio.ema_loss",  brain->network_metrics.fno_audio_ema_loss);
    EMIT_F("fno.audio.last_loss", brain->network_metrics.fno_audio_loss);
    EMIT_I("fno.audio.steps",     (int64_t)brain->network_metrics.fno_audio_steps);
    EMIT_I("fno.audio.params",    (int64_t)brain->network_metrics.fno_audio_params);
    EMIT_F("fno.visual.ema_loss",  brain->network_metrics.fno_visual_ema_loss);
    EMIT_F("fno.visual.last_loss", brain->network_metrics.fno_visual_loss);
    EMIT_I("fno.visual.steps",     (int64_t)brain->network_metrics.fno_visual_steps);
    EMIT_I("fno.visual.params",    (int64_t)brain->network_metrics.fno_visual_params);
    EMIT_F("fno.speech.ema_loss",  brain->network_metrics.fno_speech_ema_loss);
    EMIT_F("fno.speech.last_loss", brain->network_metrics.fno_speech_loss);
    EMIT_I("fno.speech.steps",     (int64_t)brain->network_metrics.fno_speech_steps);
    EMIT_I("fno.speech.params",    (int64_t)brain->network_metrics.fno_speech_params);
    EMIT_F("fno.somato.ema_loss",  brain->network_metrics.fno_somato_ema_loss);
    EMIT_F("fno.somato.last_loss", brain->network_metrics.fno_somato_loss);
    EMIT_I("fno.somato.steps",     (int64_t)brain->network_metrics.fno_somato_steps);
    EMIT_I("fno.somato.params",    (int64_t)brain->network_metrics.fno_somato_params);

    #undef EMIT_F
    #undef EMIT_I

    *count = n;
}

probe_handle_t probe_attach_network_metrics(probe_registry_t* reg, uint32_t interval_ms) {
    uint16_t modules[] = {0x0100};  /* BIO_MODULE_BRAIN */
    return probe_create_active(reg, "network_metrics", modules, 1,
                                sample_network_metrics, NULL, interval_ms);
}

/* ============================================================================
 * Cognitive Stats Sampler
 * ============================================================================ */

static void sample_cognitive_stats(
    void* module_ptr, uint16_t module_id, brain_t brain,
    probe_metric_t* metrics, uint32_t* count, void* user_data)
{
    (void)module_ptr; (void)module_id; (void)user_data;
    if (!brain || !metrics || !count) return;
    uint32_t max = *count;
    uint32_t n = 0;
    extern uint64_t nimcp_time_get_us(void);
    uint64_t ts = nimcp_time_get_us();

    #define EMIT_F(key, val) do { if (n < max) set_float(&metrics[n++], key, val, ts); } while(0)
    #define EMIT_I(key, val) do { if (n < max) set_int(&metrics[n++], key, val, ts); } while(0)

    EMIT_I("grounded_lang.steps", (int64_t)brain->cognitive_stats.grounded_lang_steps);
    EMIT_I("knowledge.steps",     (int64_t)brain->cognitive_stats.knowledge_steps);
    EMIT_I("vae.steps",           (int64_t)brain->cognitive_stats.vae_steps);
    EMIT_F("vae.last_loss",       brain->cognitive_stats.vae_last_loss);
    EMIT_I("fep_parietal.steps",  (int64_t)brain->cognitive_stats.fep_parietal_steps);
    EMIT_I("physics_nn.steps",    (int64_t)brain->cognitive_stats.physics_nn_steps);
    EMIT_I("pred_hierarchy.steps",(int64_t)brain->cognitive_stats.pred_hierarchy_steps);
    EMIT_F("pred_hierarchy.loss", brain->cognitive_stats.pred_hierarchy_last_loss);
    EMIT_I("jepa.steps",          (int64_t)brain->cognitive_stats.jepa_steps);
    EMIT_F("jepa.last_loss",      brain->cognitive_stats.jepa_last_loss);
    EMIT_I("creative.steps",      (int64_t)brain->cognitive_stats.creative_steps);
    EMIT_I("self_heal.steps",     (int64_t)brain->cognitive_stats.self_heal_steps);
    EMIT_I("intuition.steps",     (int64_t)brain->cognitive_stats.intuition_steps);
    EMIT_I("fep_orch.steps",      (int64_t)brain->cognitive_stats.fep_orchestrator_steps);

    #undef EMIT_F
    #undef EMIT_I

    *count = n;
}

probe_handle_t probe_attach_cognitive_stats(probe_registry_t* reg, uint32_t interval_ms) {
    uint16_t modules[] = {0x0100};
    return probe_create_active(reg, "cognitive_stats", modules, 1,
                                sample_cognitive_stats, NULL, interval_ms);
}

/* ============================================================================
 * Training Dashboard Sampler
 * ============================================================================ */

static void sample_training_dashboard(
    void* module_ptr, uint16_t module_id, brain_t brain,
    probe_metric_t* metrics, uint32_t* count, void* user_data)
{
    (void)module_ptr; (void)module_id; (void)user_data;
    if (!brain || !metrics || !count) return;
    uint32_t max = *count;
    uint32_t n = 0;
    extern uint64_t nimcp_time_get_us(void);
    uint64_t ts = nimcp_time_get_us();

    #define EMIT_F(key, val) do { if (n < max) set_float(&metrics[n++], key, val, ts); } while(0)
    #define EMIT_I(key, val) do { if (n < max) set_int(&metrics[n++], key, val, ts); } while(0)
    #define EMIT_S(key, val) do { if (n < max) set_string(&metrics[n++], key, val, ts); } while(0)

    EMIT_I("stage",            (int64_t)brain->training_dashboard.current_stage);
    EMIT_I("step",             (int64_t)brain->training_dashboard.current_step);
    EMIT_S("domain",           brain->training_dashboard.current_domain);
    EMIT_F("fact_ratio",       brain->training_dashboard.fact_ratio);
    EMIT_I("warm_start_done",  brain->training_dashboard.warm_start_complete ? 1 : 0);
    EMIT_I("warm_start_step",  (int64_t)brain->training_dashboard.warm_start_step);
    EMIT_F("lr_physics",       brain->training_dashboard.lr_physics);
    EMIT_F("lr_chemistry",     brain->training_dashboard.lr_chemistry);
    EMIT_F("lr_biology",       brain->training_dashboard.lr_biology);
    EMIT_I("wm_steps",         (int64_t)brain->training_dashboard.wm_steps);
    EMIT_I("wm_phys",          (int64_t)brain->training_dashboard.wm_physics_transitions);
    EMIT_I("wm_chem",          (int64_t)brain->training_dashboard.wm_chemistry_transitions);
    EMIT_I("wm_bio",           (int64_t)brain->training_dashboard.wm_biology_transitions);
    EMIT_I("collapse_events",  (int64_t)brain->training_dashboard.collapse_events);
    EMIT_I("surprises",        (int64_t)brain->training_dashboard.surprises_stored);
    EMIT_I("replays",          (int64_t)brain->training_dashboard.replays_done);
    EMIT_I("vocab_size",       (int64_t)brain->training_dashboard.vocab_size);
    EMIT_F("lang_confidence",  brain->training_dashboard.language_confidence);
    EMIT_I("active_engines",   (int64_t)brain->training_dashboard.active_engines);

    #undef EMIT_F
    #undef EMIT_I
    #undef EMIT_S

    *count = n;
}

probe_handle_t probe_attach_training_dashboard(probe_registry_t* reg, uint32_t interval_ms) {
    uint16_t modules[] = {0x0100};
    return probe_create_active(reg, "training_dashboard", modules, 1,
                                sample_training_dashboard, NULL, interval_ms);
}

/* ============================================================================
 * Inference Stats Sampler
 * ============================================================================ */

static void sample_inference(
    void* module_ptr, uint16_t module_id, brain_t brain,
    probe_metric_t* metrics, uint32_t* count, void* user_data)
{
    (void)module_ptr; (void)module_id; (void)user_data;
    if (!brain || !metrics || !count) return;
    uint32_t max = *count;
    uint32_t n = 0;
    extern uint64_t nimcp_time_get_us(void);
    uint64_t ts = nimcp_time_get_us();

    #define EMIT_F(key, val) do { if (n < max) set_float(&metrics[n++], key, val, ts); } while(0)
    #define EMIT_I(key, val) do { if (n < max) set_int(&metrics[n++], key, val, ts); } while(0)

    EMIT_I("total_inferences",   (int64_t)brain->stats.total_inferences);
    EMIT_I("total_learn_steps",  (int64_t)brain->stats.total_learning_steps);
    EMIT_F("avg_inference_us",   brain->stats.avg_inference_time_us);
    EMIT_F("current_lr",         brain->stats.current_learning_rate);
    EMIT_F("avg_sparsity",       brain->stats.avg_sparsity);
    EMIT_F("accuracy",           brain->stats.accuracy);
    EMIT_F("running_accuracy",   brain->stats.running_accuracy);
    EMIT_I("num_neurons",        (int64_t)brain->stats.num_neurons);
    EMIT_I("num_synapses",       (int64_t)brain->stats.num_synapses);
    EMIT_I("memory_bytes",       (int64_t)brain->stats.memory_bytes);
    EMIT_F("novelty_score",      brain->last_novelty_score);
    EMIT_F("curiosity_drive",    brain->last_curiosity_drive);
    EMIT_I("recurrent_iters",    (int64_t)brain->recurrent_iteration_count);

    #undef EMIT_F
    #undef EMIT_I

    *count = n;
}

probe_handle_t probe_attach_inference(probe_registry_t* reg, uint32_t interval_ms) {
    uint16_t modules[] = {0x0100};
    return probe_create_active(reg, "inference", modules, 1,
                                sample_inference, NULL, interval_ms);
}

/* ============================================================================
 * Glial System Sampler — astrocytes, oligodendrocytes, microglia
 * ============================================================================ */

#include "glial/integration/nimcp_glial_integration.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "glial/microglia/nimcp_microglia.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"

static void sample_glial(
    void* module_ptr, uint16_t module_id, brain_t brain,
    probe_metric_t* metrics, uint32_t* count, void* user_data)
{
    (void)module_id; (void)user_data;
    if (!brain || !metrics || !count) return;

    glial_integration_t* gi = (glial_integration_t*)module_ptr;
    if (!gi) { *count = 0; return; }

    uint32_t max = *count;
    uint32_t n = 0;
    extern uint64_t nimcp_time_get_us(void);
    uint64_t ts = nimcp_time_get_us();

    #define EMIT_F(key, val) do { if (n < max) set_float(&metrics[n++], key, val, ts); } while(0)
    #define EMIT_I(key, val) do { if (n < max) set_int(&metrics[n++], key, val, ts); } while(0)

    /* Integration-level counters */
    EMIT_I("total_modulations", (int64_t)gi->total_astrocyte_modulations);
    EMIT_I("total_myelinations", (int64_t)gi->total_oligodendrocyte_myelinations);
    EMIT_I("total_prunings", (int64_t)gi->total_microglia_prunings);
    EMIT_I("total_neuromod_updates", (int64_t)gi->total_neuromod_updates);
    EMIT_I("astro_enabled", gi->enable_astrocyte_modulation ? 1 : 0);
    EMIT_I("oligo_enabled", gi->enable_oligodendrocyte_myelination ? 1 : 0);
    EMIT_I("microglia_enabled", gi->enable_microglia_pruning ? 1 : 0);
    EMIT_I("neuromod_enabled", gi->enable_spatial_neuromod ? 1 : 0);

    /* Astrocyte network stats */
    if (gi->astrocyte_network) {
        astrocyte_network_t* an = gi->astrocyte_network;
        /* Use public accessor if available, otherwise read struct directly */
        extern bool brain_get_astrocyte_stats(brain_t, astrocyte_stats_t*);
        astrocyte_stats_t ast_stats = {0};
        if (brain_get_astrocyte_stats(brain, &ast_stats)) {
            EMIT_I("astro.count", (int64_t)ast_stats.num_astrocytes);
            EMIT_F("astro.avg_calcium_um", ast_stats.avg_calcium_um);
            EMIT_I("astro.tripartite_synapses", (int64_t)ast_stats.num_tripartite_synapses);
            EMIT_I("astro.modulations", (int64_t)ast_stats.total_modulations);
            EMIT_F("astro.avg_modulation", ast_stats.avg_modulation_strength);
        }
    }

    /* Oligodendrocyte network — report presence */
    EMIT_I("oligo.active", gi->oligodendrocyte_network ? 1 : 0);

    /* Microglia network — report presence + use integration counters */
    EMIT_I("microglia.active", gi->microglia_network ? 1 : 0);

    #undef EMIT_F
    #undef EMIT_I

    *count = n;
}

probe_handle_t probe_attach_glial(probe_registry_t* reg, uint32_t interval_ms) {
    uint16_t modules[] = {0x0350};  /* BIO_MODULE_GLIAL */
    return probe_create_active(reg, "glial", modules, 1,
                                sample_glial, NULL, interval_ms);
}

/* ============================================================================
 * Neuron Statistics Sampler — activation distribution across network
 * ============================================================================ */

#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_internal.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_sparse_synapse.h"

static void sample_neurons(
    void* module_ptr, uint16_t module_id, brain_t brain,
    probe_metric_t* metrics, uint32_t* count, void* user_data)
{
    (void)module_id; (void)user_data;
    if (!brain || !metrics || !count) return;

    neural_network_t nn = adaptive_network_get_base_network(brain->network);
    if (!nn || nn->num_neurons == 0) { *count = 0; return; }

    uint32_t max = *count;
    uint32_t n = 0;
    extern uint64_t nimcp_time_get_us(void);
    uint64_t ts = nimcp_time_get_us();

    #define EMIT_F(key, val) do { if (n < max) set_float(&metrics[n++], key, val, ts); } while(0)
    #define EMIT_I(key, val) do { if (n < max) set_int(&metrics[n++], key, val, ts); } while(0)

    /* Sample neuron state distribution (read-only, no locks needed) */
    uint32_t total = nn->num_neurons;
    uint32_t sample_n = total < 10000 ? total : 10000;  /* Cap sampling to 10K */
    uint32_t stride = total / sample_n;
    if (stride < 1) stride = 1;

    float sum = 0.0f, sum_sq = 0.0f;
    float min_state = 1e30f, max_state = -1e30f;
    uint32_t zero_count = 0, nan_count = 0, negative_count = 0;
    uint32_t sampled = 0;

    for (uint32_t i = 0; i < total && sampled < sample_n; i += stride) {
        float s = nn->neurons[i].state;
        if (!isfinite(s)) { nan_count++; continue; }
        sum += s;
        sum_sq += s * s;
        if (s < min_state) min_state = s;
        if (s > max_state) max_state = s;
        if (fabsf(s) < 1e-7f) zero_count++;
        if (s < 0.0f) negative_count++;
        sampled++;
    }

    float mean = sampled > 0 ? sum / (float)sampled : 0.0f;
    float variance = sampled > 1 ? (sum_sq / (float)sampled - mean * mean) : 0.0f;
    if (variance < 0.0f) variance = 0.0f;
    float std_dev = sqrtf(variance);
    float sparsity = sampled > 0 ? (float)zero_count / (float)sampled : 0.0f;

    EMIT_I("total", (int64_t)total);
    EMIT_I("sampled", (int64_t)sampled);
    EMIT_F("state.mean", mean);
    EMIT_F("state.std", std_dev);
    EMIT_F("state.min", min_state);
    EMIT_F("state.max", max_state);
    EMIT_F("state.sparsity", sparsity);
    EMIT_I("state.nan_count", (int64_t)nan_count);
    EMIT_I("state.negative", (int64_t)negative_count);

    #undef EMIT_F
    #undef EMIT_I
    *count = n;
}

probe_handle_t probe_attach_neurons(probe_registry_t* reg, uint32_t interval_ms) {
    uint16_t modules[] = {0x0500};  /* ADAPTIVE network */
    return probe_create_active(reg, "neurons", modules, 1,
                                sample_neurons, NULL, interval_ms);
}

/* ============================================================================
 * Synapse Statistics Sampler — weight distribution, metadata pool health
 * ============================================================================ */

static void sample_synapses(
    void* module_ptr, uint16_t module_id, brain_t brain,
    probe_metric_t* metrics, uint32_t* count, void* user_data)
{
    (void)module_id; (void)user_data;
    if (!brain || !metrics || !count) return;

    neural_network_t nn = adaptive_network_get_base_network(brain->network);
    if (!nn || nn->num_neurons == 0) { *count = 0; return; }

    uint32_t max = *count;
    uint32_t n = 0;
    extern uint64_t nimcp_time_get_us(void);
    uint64_t ts = nimcp_time_get_us();

    #define EMIT_F(key, val) do { if (n < max) set_float(&metrics[n++], key, val, ts); } while(0)
    #define EMIT_I(key, val) do { if (n < max) set_int(&metrics[n++], key, val, ts); } while(0)

    /* Sample synapse weights from a subset of neurons */
    uint32_t total_neurons = nn->num_neurons;
    uint32_t sample_neurons = total_neurons < 1000 ? total_neurons : 1000;
    uint32_t stride = total_neurons / sample_neurons;
    if (stride < 1) stride = 1;

    float w_sum = 0.0f, w_sum_sq = 0.0f;
    float w_min = 1e30f, w_max = -1e30f;
    uint32_t total_synapses = 0, zero_weights = 0;
    uint32_t excitatory = 0, inhibitory = 0;

    for (uint32_t i = 0; i < total_neurons && (i / stride) < sample_neurons; i += stride) {
        neuron_t* neuron = &nn->neurons[i];
        uint32_t syn_count = sparse_synapse_count(&neuron->incoming);

        for (uint32_t s = 0; s < syn_count && s < 32; s++) {  /* Cap per-neuron to 32 */
            synapse_handle_t* h = sparse_synapse_get(&neuron->incoming, s);
            if (!h) continue;

            float w = h->weight;
            if (!isfinite(w)) continue;

            w_sum += w;
            w_sum_sq += w * w;
            if (w < w_min) w_min = w;
            if (w > w_max) w_max = w;
            if (fabsf(w) < 1e-7f) zero_weights++;
            if (w > 0.0f) excitatory++;
            else if (w < 0.0f) inhibitory++;
            total_synapses++;
        }
    }

    float w_mean = total_synapses > 0 ? w_sum / (float)total_synapses : 0.0f;
    float w_var = total_synapses > 1 ?
        (w_sum_sq / (float)total_synapses - w_mean * w_mean) : 0.0f;
    if (w_var < 0.0f) w_var = 0.0f;

    EMIT_I("sampled_synapses", (int64_t)total_synapses);
    EMIT_F("weight.mean", w_mean);
    EMIT_F("weight.std", sqrtf(w_var));
    EMIT_F("weight.min", total_synapses > 0 ? w_min : 0.0f);
    EMIT_F("weight.max", total_synapses > 0 ? w_max : 0.0f);
    EMIT_I("weight.zero", (int64_t)zero_weights);
    EMIT_I("excitatory", (int64_t)excitatory);
    EMIT_I("inhibitory", (int64_t)inhibitory);
    EMIT_F("ei_ratio", excitatory > 0 ?
        (float)excitatory / (float)(excitatory + inhibitory) : 0.0f);

    /* Metadata pool health — use total_slots from pool struct */
    EMIT_I("metadata.pool_active", nn->synapse_metadata_pool ? 1 : 0);

    #undef EMIT_F
    #undef EMIT_I
    *count = n;
}

probe_handle_t probe_attach_synapses(probe_registry_t* reg, uint32_t interval_ms) {
    uint16_t modules[] = {0x0500};  /* ADAPTIVE network */
    return probe_create_active(reg, "synapses", modules, 1,
                                sample_synapses, NULL, interval_ms);
}

/* ============================================================================
 * Brain Regions Sampler — all cortexes, subcortical, language areas
 *
 * Reports which regions are enabled and their update timestamps.
 * Covers: cortex CNNs (visual/audio/speech/somato), broca, wernicke,
 * hippocampus, amygdala, hypothalamus, basal ganglia, cerebellum,
 * medulla, parietal, thalamic bridges, cortical columns.
 * ============================================================================ */

static void sample_brain_regions(
    void* module_ptr, uint16_t module_id, brain_t brain,
    probe_metric_t* metrics, uint32_t* count, void* user_data)
{
    (void)module_ptr; (void)module_id; (void)user_data;
    if (!brain || !metrics || !count) return;

    uint32_t max = *count;
    uint32_t n = 0;
    extern uint64_t nimcp_time_get_us(void);
    uint64_t ts = nimcp_time_get_us();

    #define EMIT_I(key, val) do { if (n < max) set_int(&metrics[n++], key, val, ts); } while(0)
    #define EMIT_F(key, val) do { if (n < max) set_float(&metrics[n++], key, val, ts); } while(0)

    /* Cortex CNN processors (4 modalities) */
    EMIT_I("visual_cortex", brain->cortex_cnns[0] ? 1 : 0);
    EMIT_I("audio_cortex", brain->cortex_cnns[1] ? 1 : 0);
    EMIT_I("speech_cortex", brain->cortex_cnns[2] ? 1 : 0);
    EMIT_I("somato_cortex", brain->cortex_cnns[3] ? 1 : 0);

    /* Language regions */
    EMIT_I("broca", brain->broca_enabled ? 1 : 0);
    EMIT_I("wernicke", brain->wernicke_enabled ? 1 : 0);

    /* Subcortical */
    EMIT_I("hippocampus", brain->engram_system ? 1 : 0);
    EMIT_I("amygdala", brain->emotional_learning ? 1 : 0);
    EMIT_I("hypothalamus", brain->hypothalamus_enabled ? 1 : 0);
    EMIT_I("basal_ganglia", brain->basal_ganglia_enabled ? 1 : 0);
    EMIT_I("cerebellum", brain->cerebellum_enabled ? 1 : 0);
    EMIT_I("medulla", brain->medulla_enabled ? 1 : 0);

    /* Higher cortical */
    EMIT_I("parietal", brain->parietal_enabled ? 1 : 0);
    EMIT_I("prefrontal", brain->executive ? 1 : 0);

    /* Cortical columns (Thousand Brains) */
    EMIT_I("cortical_columns", brain->tb_integration_hub ? 1 : 0);

    /* Thalamic bridges (count active) */
    int thalamic_count = 0;
    if (brain->broca_thalamic_bridge) thalamic_count++;
    if (brain->language_thalamic_bridge) thalamic_count++;
    if (brain->cerebellum_thalamic_bridge) thalamic_count++;
    if (brain->hippocampus_thalamic_bridge) thalamic_count++;
    if (brain->hypothalamus_thalamic_bridge) thalamic_count++;
    if (brain->motor_thalamic_bridge) thalamic_count++;
    if (brain->occipital_thalamic_bridge) thalamic_count++;
    if (brain->temporal_thalamic_bridge) thalamic_count++;
    EMIT_I("thalamic_bridges", (int64_t)thalamic_count);

    /* Glial (already has dedicated probe, but summary here) */
    EMIT_I("glial", brain->glial ? 1 : 0);

    /* Count total active regions directly */
    int total_active =
        (brain->cortex_cnns[0] ? 1 : 0) + (brain->cortex_cnns[1] ? 1 : 0) +
        (brain->cortex_cnns[2] ? 1 : 0) + (brain->cortex_cnns[3] ? 1 : 0) +
        (brain->broca_enabled ? 1 : 0) + (brain->wernicke_enabled ? 1 : 0) +
        (brain->engram_system ? 1 : 0) + (brain->emotional_learning ? 1 : 0) +
        (brain->hypothalamus_enabled ? 1 : 0) + (brain->basal_ganglia_enabled ? 1 : 0) +
        (brain->cerebellum_enabled ? 1 : 0) + (brain->medulla_enabled ? 1 : 0) +
        (brain->parietal_enabled ? 1 : 0) + (brain->executive ? 1 : 0) +
        (brain->tb_integration_hub ? 1 : 0) + (brain->glial ? 1 : 0);
    EMIT_I("total_active_regions", (int64_t)total_active);

    #undef EMIT_I
    #undef EMIT_F
    *count = n;
}

probe_handle_t probe_attach_brain_regions(probe_registry_t* reg, uint32_t interval_ms) {
    uint16_t modules[] = {0x0100};  /* BIO_MODULE_BRAIN */
    return probe_create_active(reg, "brain_regions", modules, 1,
                                sample_brain_regions, NULL, interval_ms);
}

/* ============================================================================
 * Cognitive Dispatch Sampler — parallel actor pattern metrics
 * ============================================================================ */

static void sample_dispatch(
    void* module_ptr, uint16_t module_id, brain_t brain,
    probe_metric_t* metrics, uint32_t* count, void* user_data)
{
    (void)module_ptr; (void)module_id; (void)user_data;
    if (!brain || !metrics || !count) return;
    uint32_t max = *count;
    uint32_t n = 0;
    extern uint64_t nimcp_time_get_us(void);
    uint64_t ts = nimcp_time_get_us();

    #define EMIT_F(key, val) do { if (n < max) set_float(&metrics[n++], key, val, ts); } while(0)
    #define EMIT_I(key, val) do { if (n < max) set_int(&metrics[n++], key, val, ts); } while(0)

    EMIT_I("dispatch.last_us",       (int64_t)brain->dispatch_metrics.last_dispatch_us);
    EMIT_I("dispatch.modules_ran",   (int64_t)brain->dispatch_metrics.last_modules_executed);
    EMIT_I("dispatch.modules_submitted", (int64_t)brain->dispatch_metrics.last_modules_submitted);
    EMIT_I("dispatch.total_parallel",(int64_t)brain->dispatch_metrics.total_dispatches);
    EMIT_I("dispatch.total_sequential",(int64_t)brain->dispatch_metrics.total_sequential);
    EMIT_I("dispatch.cumulative_us", (int64_t)brain->dispatch_metrics.cumulative_dispatch_us);
    EMIT_I("dispatch.pool_threads",  (int64_t)brain->dispatch_metrics.pool_thread_count);
    EMIT_F("dispatch.slowest_us",    brain->dispatch_metrics.slowest_module_us);
    EMIT_F("dispatch.avg_module_us", brain->dispatch_metrics.avg_module_us);

    /* Compute average dispatch time if we have data */
    if (brain->dispatch_metrics.total_dispatches > 0) {
        float avg = (float)brain->dispatch_metrics.cumulative_dispatch_us /
                    (float)brain->dispatch_metrics.total_dispatches;
        EMIT_F("dispatch.avg_dispatch_us", avg);
    }

    #undef EMIT_F
    #undef EMIT_I

    *count = n;
}

probe_handle_t probe_attach_dispatch(probe_registry_t* reg, uint32_t interval_ms) {
    uint16_t modules[] = {0x0100};  /* BIO_MODULE_BRAIN */
    return probe_create_active(reg, "dispatch", modules, 1,
                                sample_dispatch, NULL, interval_ms);
}
