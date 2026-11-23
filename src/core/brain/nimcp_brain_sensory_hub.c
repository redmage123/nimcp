/**
 * @file nimcp_brain_sensory_hub.c
 * @brief Sensory hub: fans hearing and vision out to the rest of the brain.
 *        See include/core/brain/nimcp_brain_sensory_hub.h.
 */
#include "core/brain/nimcp_brain_sensory_hub.h"

#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_hearing.h"
#include "core/brain/nimcp_brain_kg.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Targets are reached through their public functions, declared here with
 * opaque pointers: several of their headers declare tags that clash with
 * typedefs pulled in through brain_internal (see nimcp_brain_init_cochlea.c). */
extern int nimcp_brain_thal_relay_auditory(brain_t, const float*, uint32_t, float*, uint32_t);
extern int nimcp_brain_thal_relay_visual(brain_t, const float*, uint32_t, float*, uint32_t);
extern int wernicke_get_snn_pop_id(const void* adapter);
extern void* wernicke_get_snn_network(const void* adapter);
extern uint32_t hippocampus_encode_memory(void* adapter, const float* features, uint32_t n,
                                          const void* location, float valence);
extern bool brain_add_logical_fact(brain_t brain, const char* fact_str, float salience);

/* Cerebellum fibre structs (nimcp_cerebellum_adapter.h), mirrored. */
typedef struct { uint32_t fiber_id; float activity; float timestamp_ms; uint8_t modality; } hub_mossy_t;
typedef struct { uint32_t fiber_id; float error_signal; float timestamp_ms;
                 uint32_t target_purkinje_id; uint8_t error_type; } hub_climbing_t;
extern bool cerebellum_process_mossy_batch(void* adapter, const hub_mossy_t* inputs, uint32_t count);
extern bool cerebellum_process_climbing_signal(void* adapter, const hub_climbing_t* signal);

/* SNN population: only the two fields used (nimcp_snn_types.h). */
#include "snn/nimcp_snn_types.h"
extern snn_population_t* snn_network_get_population(void* network, uint32_t pop_id);

#define MODULE_PERCEPTION_ID 1        /* cognitive_module_t MODULE_PERCEPTION (nimcp_global_workspace.h: NONE=0, PERCEPTION=1) */
#define HUB_INTENSITY_SCALE 320.0f    /* spike count -> drive, as in the predictor */
#define HUB_MODULE_ID ((bio_module_id_t)0x1F10)
#define HUB_ONSET_JUMP 0.08f          /* loudness rise per tick that counts as an onset */
#define HUB_NOVELTY_GAP_FRAMES 50u    /* >= 2 s of audio between hippocampal encodes */
#define HUB_FACT_GAP_FRAMES 15000u    /* >= 10 min of audio between logic facts */
#define HUB_REWARD_GAP_FRAMES 25u     /* intrinsic reward at most once per second of audio */
#define HUB_IMMUNE_SURPRISE 4.0f      /* prediction 4x worse than "no change": anomalous input */
#define HUB_DIVERGED_SKILL (-5.0f)    /* self-repair: hearing has diverged */

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sensory_hub)

typedef struct {
    bio_message_header_t hdr;
    float features[SENSORY_HUB_AUDIO_CHANNELS];   /* MGN relay of the auditory nerve, [0,1] */
    float surprise;                                /* MSE / MSE(copy) */
    float loudness;
    uint32_t frames;
} hub_audio_msg_t;

typedef struct {
    bio_message_header_t hdr;
    float features[256];                           /* first 256 LGN values */
    float luminance, contrast, change;
    uint32_t n;
} hub_visual_msg_t;

typedef struct {
    nimcp_mutex_t* mu;
    bio_module_context_t router;
    brain_sensory_hub_stats_t st;

    /* audio accumulated since the last tick (guarded by mu) */
    double a_counts[SENSORY_HUB_AUDIO_CHANNELS];
    double a_err2[SENSORY_HUB_AUDIO_CHANNELS];
    double a_err[SENSORY_HUB_AUDIO_CHANNELS];
    double a_mse, a_copy;
    uint32_t a_frames;
    float hidden[1024];
    uint32_t n_hidden;

    /* vision: latest frame (guarded by mu) */
    brain_sensory_visual_frame_t v_frame;
    bool v_pending;

    /* tick-side state */
    float prev_loudness;
    float surprise_ema;
    uint64_t frames_at_last_encode, frames_at_last_fact, frames_at_last_reward;
    float surprise_at_last_reward;
    uint32_t diverged_ticks;
    bool kg_done, facts_done, health_done;   /* late binding: these may appear after attach */
    /* Single-entry guard for brain_sensory_hub_tick: it is called from both
     * brain_learn_vector and brain_decide, and the daemon runs decide_full
     * lock-free on the RO socket, so two ticks could otherwise run the
     * tick-side state below (and the occipital pipeline) concurrently. */
    int tick_busy;                   /* __atomic flag, 1 while a tick runs */
    bool router_fanout;              /* NIMCP_SENSORY_HUB_ROUTER_FANOUT=1: inbox messages */
    uint64_t nonfinite_seen;         /* hearing's nonfinite_resets at the last tick */
    uint32_t nonfinite_ticks;        /* consecutive ticks with new non-finite resets */
    float lgn_out[SENSORY_HUB_VISUAL_MAX];   /* thalamic relay of the LGN (was a function static) */
    float rf_buf[SENSORY_HUB_VISUAL_MAX];    /* receptive-field pooled hypercolumn input */
    double rf_prefix[SENSORY_HUB_VISUAL_MAX + 1];
    float* wide;                     /* num_inputs-wide buffer for WM / workspace */
    float* occ_img;                  /* occipital input image (CHW) built from the LGN maps */
    uint32_t occ_img_n;
    uint32_t occ_frame_id;
    uint32_t wide_n;
} brain_sensory_hub_t;

/* ------------------------------------------------------------ helpers */

static brain_kg_node_id_t kg_node(brain_t b, brain_sensory_hub_t* h, const char* name,
                                  brain_kg_node_type_t type, const char* desc) {
    brain_kg_node_id_t id = brain_kg_find_node(b->internal_kg, name);
    if (id == BRAIN_KG_INVALID_NODE) {
        id = brain_kg_add_node(b->internal_kg, name, type, desc);
        if (id != BRAIN_KG_INVALID_NODE) h->st.kg_nodes++;
    }
    return id;
}

static void kg_edge(brain_t b, brain_sensory_hub_t* h, brain_kg_node_id_t from, brain_kg_node_id_t to,
                    brain_kg_edge_type_t type, const char* desc, float w) {
    if (from == BRAIN_KG_INVALID_NODE || to == BRAIN_KG_INVALID_NODE) return;
    (void)brain_kg_add_edge(b->internal_kg, from, to, type, desc, w);
    h->st.kg_edges++;
}

/* Register the sensory pathway and its targets in the knowledge graph. */
static void register_kg(brain_t b, brain_sensory_hub_t* h) {
    if (!b->internal_kg) return;
    /* Structural writes need ADMIN (access control); drop back to READ after,
     * as nimcp_brain_part_core.c does. */
    brain_kg_set_access_level(b->internal_kg, BRAIN_KG_ACCESS_ADMIN, b->internal_kg_admin_token);
    brain_kg_node_id_t cochlea = kg_node(b, h, "cochlea", BRAIN_KG_NODE_PERCEPTION,
        "Basilar membrane, hair cells; 64 channels, 16 kHz");
    brain_kg_node_id_t nerve = kg_node(b, h, "auditory_nerve", BRAIN_KG_NODE_PERCEPTION,
        "Auditory-nerve fibres: spike counts per 40 ms frame");
    brain_kg_node_id_t hear = kg_node(b, h, "wernicke_predictor", BRAIN_KG_NODE_PERCEPTION,
        "Hearing: 512 ALIF neurons learning with e-prop to predict the next 40 ms of sound");
    brain_kg_node_id_t retina = kg_node(b, h, "retina", BRAIN_KG_NODE_PERCEPTION,
        "Rods, cones, horizontal, bipolar, amacrine and ganglion cells");
    brain_kg_node_id_t optic = kg_node(b, h, "optic_nerve", BRAIN_KG_NODE_PERCEPTION,
        "Myelinated ganglion-cell axons to the LGN");
    brain_kg_node_id_t hub = kg_node(b, h, "sensory_hub", BRAIN_KG_NODE_INTEGRATION,
        "Fans hearing and vision out to the brain each tick");
    kg_edge(b, h, cochlea, nerve, BRAIN_KG_EDGE_SENDS_TO, "hair cells drive auditory-nerve fibres", 1.0f);
    kg_edge(b, h, nerve, hear, BRAIN_KG_EDGE_SENDS_TO, "auditory nerve drives the Wernicke predictor", 1.0f);
    kg_edge(b, h, hear, hub, BRAIN_KG_EDGE_SENDS_TO, "frames, prediction error, hidden activity", 1.0f);
    kg_edge(b, h, retina, optic, BRAIN_KG_EDGE_SENDS_TO, "ganglion-cell spikes", 1.0f);
    kg_edge(b, h, optic, hub, BRAIN_KG_EDGE_SENDS_TO, "spikes at the LGN end", 1.0f);

    static const struct { const char* name; brain_kg_node_type_t type; const char* why; } targets[] = {
        {"thalamus", BRAIN_KG_NODE_SUBCORTICAL, "MGN/LGN relay of hearing and vision"},
        {"bio_async_router", BRAIN_KG_NODE_COORDINATOR, "sensory broadcasts and onset messages"},
        {"cortical_columns", BRAIN_KG_NODE_CORTICAL, "tonotopic / retinotopic hypercolumn input"},
        {"wernicke", BRAIN_KG_NODE_CORTICAL, "hidden auditory activity drives the Wernicke SNN population"},
        {"broca", BRAIN_KG_NODE_CORTICAL, "speech-onset messages (arcuate fasciculus)"},
        {"occipital", BRAIN_KG_NODE_CORTICAL, "LGN input to V1-V5"},
        {"hippocampus", BRAIN_KG_NODE_SUBCORTICAL, "novel sounds and scenes are encoded"},
        {"working_memory", BRAIN_KG_NODE_COGNITIVE, "salient onsets"},
        {"global_workspace", BRAIN_KG_NODE_COGNITIVE, "salient onsets bid for access"},
        {"medulla", BRAIN_KG_NODE_BRAINSTEM, "arousal on loud onsets"},
        {"basal_ganglia", BRAIN_KG_NODE_SUBCORTICAL, "intrinsic reward = prediction learning progress"},
        {"cerebellum", BRAIN_KG_NODE_SUBCORTICAL, "mossy-fibre input, climbing-fibre prediction error"},
        {"immune_system", BRAIN_KG_NODE_SECURITY, "anomalous sensory input (when enabled)"},
        {"symbolic_logic", BRAIN_KG_NODE_COGNITIVE, "perceptual facts"},
        {"health_agent", BRAIN_KG_NODE_COORDINATOR, "heartbeat and self-repair"},
    };
    for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++) {
        brain_kg_node_id_t t = kg_node(b, h, targets[i].name, targets[i].type, targets[i].why);
        kg_edge(b, h, hub, t, BRAIN_KG_EDGE_SENDS_TO, targets[i].why, 0.5f);
    }
    brain_kg_set_access_level(b->internal_kg, BRAIN_KG_ACCESS_READ, b->internal_kg_admin_token);
}

/* KG structure, base facts and the health agent, as soon as each exists
 * (the KG and logic engine may be created after the hub). */
static void late_bind(brain_t b, brain_sensory_hub_t* h) {
    if (!h->kg_done && b->internal_kg) {
        register_kg(b, h);
        h->kg_done = true;
    }
    if (!h->facts_done && b->symbolic_logic) {
        if (brain_add_logical_fact(b, "has_sense(athena, hearing)", 1.0f)) h->st.logic_facts++;
        if (brain_add_logical_fact(b, "has_sense(athena, vision)", 1.0f)) h->st.logic_facts++;
        h->facts_done = true;
    }
    if (!h->health_done && b->health_agent) {
        sensory_hub_set_health_agent((nimcp_health_agent_t*)b->health_agent);
        h->health_done = true;
    }
}

/* ------------------------------------------------------------ lifecycle */

int brain_sensory_hub_attach(struct brain_struct* b) {
    if (!b) return -1;
    if (b->sensory_hub) return 0;
    brain_sensory_hub_t* h = (brain_sensory_hub_t*)calloc(1, sizeof(*h));
    if (!h) return -1;
    h->mu = nimcp_mutex_create(NULL);
    h->wide_n = b->config.num_inputs ? b->config.num_inputs : SENSORY_HUB_AUDIO_CHANNELS;
    h->wide = (float*)calloc(h->wide_n, sizeof(float));
    if (!h->mu || !h->wide) {
        if (h->mu) nimcp_mutex_free(h->mu);
        free(h->wide);
        free(h);
        return -1;
    }
    /* The router has no receiver for the hub's per-tick message types yet
     * (Wernicke is not a router module, Broca has no SPEECH_ONSET handler,
     * nothing handles AUDIO_FEATURE_DETECTED / VISUAL_FEATURES_READY), so
     * broadcasting them only fills every module's bounded inbox. They are
     * opt-in until receivers exist; the scalar signals are always published. */
    const char* fan = getenv("NIMCP_SENSORY_HUB_ROUTER_FANOUT");
    h->router_fanout = fan && fan[0] == '1';
    if (bio_router_is_initialized()) {
        bio_module_info_t info = {HUB_MODULE_ID, "sensory_hub", 0, b};
        h->router = bio_router_register_module(&info);
        if (!h->router)
            fprintf(stderr, "[SENSORY_HUB] bio-async router registration failed (router full?)\n");
    }
    b->sensory_hub = h;
    late_bind(b, h);
    fprintf(stderr, "[SENSORY_HUB] attached: router=%s kg=%llu nodes/%llu edges, targets: thalamus=%d "
            "columns=%u wernicke=%d broca=%d occipital=%d hippocampus=%d wm=%d gw=%d medulla=%d bg=%d "
            "cerebellum=%d immune=%d logic=%d health=%d\n",
            h->router ? "yes" : "no", (unsigned long long)h->st.kg_nodes,
            (unsigned long long)h->st.kg_edges, b->thalamus != NULL, b->num_hypercolumns,
            b->wernicke != NULL, b->broca != NULL, b->occipital != NULL, b->hippocampus != NULL,
            b->working_memory != NULL, b->global_workspace != NULL, b->medulla != NULL,
            b->basal_ganglia != NULL, b->cerebellum != NULL,
            b->immune_system != NULL && b->immune_enabled, b->symbolic_logic != NULL,
            b->health_agent != NULL);
    return 0;
}

void brain_sensory_hub_destroy(struct brain_struct* b) {
    if (!b || !b->sensory_hub) return;
    brain_sensory_hub_t* h = (brain_sensory_hub_t*)b->sensory_hub;
    b->sensory_hub = NULL;
    if (h->router) bio_router_unregister_module(h->router);
    nimcp_mutex_free(h->mu);
    free(h->wide);
    free(h->occ_img);
    free(h);
}

/* --------------------------------------------------------------- posting */

void brain_sensory_hub_post_audio(struct brain_struct* b, const uint16_t* counts, uint32_t channels,
                                  const float* err, float mse, float copy_mse,
                                  const float* hidden, uint32_t n_hidden) {
    if (!b || !b->sensory_hub || !counts) return;
    brain_sensory_hub_t* h = (brain_sensory_hub_t*)b->sensory_hub;
    if (channels > SENSORY_HUB_AUDIO_CHANNELS) channels = SENSORY_HUB_AUDIO_CHANNELS;
    nimcp_mutex_lock(h->mu);
    for (uint32_t c = 0; c < channels; c++) {
        h->a_counts[c] += counts[c];
        if (err && isfinite(err[c])) {   /* nothing non-finite enters the hub */
            h->a_err[c] += err[c];
            h->a_err2[c] += (double)err[c] * err[c];
        }
    }
    if (isfinite(mse) && isfinite(copy_mse)) {
        h->a_mse += mse;
        h->a_copy += copy_mse;
    }
    h->a_frames++;
    if (hidden && n_hidden) {
        h->n_hidden = n_hidden < 1024u ? n_hidden : 1024u;
        for (uint32_t j = 0; j < h->n_hidden; j++)
            h->hidden[j] = isfinite(hidden[j]) ? hidden[j] : 0.0f;
    }
    h->st.audio_frames++;
    nimcp_mutex_unlock(h->mu);
}

void brain_sensory_hub_post_visual(struct brain_struct* b, const brain_sensory_visual_frame_t* f) {
    if (!b || !b->sensory_hub || !f) return;
    brain_sensory_hub_t* h = (brain_sensory_hub_t*)b->sensory_hub;
    nimcp_mutex_lock(h->mu);
    h->v_frame = *f;
    if (h->v_frame.n > SENSORY_HUB_VISUAL_MAX) h->v_frame.n = SENSORY_HUB_VISUAL_MAX;
    h->v_pending = true;
    h->st.visual_frames++;
    nimcp_mutex_unlock(h->mu);
}

/* ------------------------------------------------------------ fan-out */

/* Shared by both senses: novel / salient content goes to the hippocampus,
 * working memory and the global workspace. */
static void salient_to_cognition(brain_t b, brain_sensory_hub_t* h, const float* v, uint32_t n,
                                 float salience, bool encode) {
    if (encode && b->hippocampus) {
        if (hippocampus_encode_memory(b->hippocampus, v, n, NULL, 0.0f) != 0) h->st.hippocampus_encodes++;
    }
    memset(h->wide, 0, h->wide_n * sizeof(float));
    memcpy(h->wide, v, (n < h->wide_n ? n : h->wide_n) * sizeof(float));
    float s = salience < 0.0f ? 0.0f : (salience > 1.0f ? 1.0f : salience);
    if (b->working_memory && working_memory_add(b->working_memory, h->wide, h->wide_n, s))
        h->st.working_memory_adds++;
    if (b->global_workspace &&
        global_workspace_compete(b->global_workspace, (cognitive_module_t)MODULE_PERCEPTION_ID, h->wide, h->wide_n, s))
        h->st.workspace_bids++;
}

static void broadcast(brain_sensory_hub_t* h, void* msg, size_t size, bio_message_type_t type) {
    if (!h->router || !h->router_fanout) return;   /* no receivers yet: see attach */
    bio_msg_init_header((bio_message_header_t*)msg, type, HUB_MODULE_ID, 0,
                        size - sizeof(bio_message_header_t));
    if (bio_router_broadcast(h->router, msg, size) == NIMCP_SUCCESS) h->st.router_broadcasts++;
}

static void send_to(brain_sensory_hub_t* h, void* msg, size_t size, bio_message_type_t type,
                    bio_module_id_t target) {
    if (!h->router || !h->router_fanout) return;   /* no receivers yet: see attach */
    bio_msg_init_header((bio_message_header_t*)msg, type, HUB_MODULE_ID, target,
                        size - sizeof(bio_message_header_t));
    if (bio_router_send(h->router, msg, size, 0) == NIMCP_SUCCESS) h->st.router_sends++;
}

/* Scalar perceptual signals: delivered only to observers registered for the
 * name (bio_router_observe_signal), so they cost no inbox space when nobody
 * listens, and any module can subscribe without a new message handler. */
static void publish(brain_sensory_hub_t* h, const char* name, float value) {
    if (h->router && isfinite(value)) (void)bio_router_publish_signal(h->router, name, value);
}

/* Receptive fields for hypercolumn input. minicolumn_compute samples an
 * input of size > 3 at one point chosen by each minicolumn's tuning
 * preference, so a raw channel / LGN vector gave each minicolumn a single
 * channel. Box-pooling over one minicolumn spacing (prefix sums, O(n))
 * makes each sampled point the mean of a contiguous tonotopic / retinotopic
 * band, so every minicolumn sees its whole patch of the input. */
static const float* rf_pool(brain_sensory_hub_t* h, hypercolumn_t* hc, const float* in, uint32_t n) {
    cc_hypercolumn_stats_t hs;
    memset(&hs, 0, sizeof(hs));
    hypercolumn_get_stats(hc, &hs);
    const uint32_t M = hs.num_minicolumns;
    if (n == 0 || n > SENSORY_HUB_VISUAL_MAX || M < 2 || n <= M) return in;
    const uint32_t r = (uint32_t)((n - 1) / (2.0f * (float)(M - 1)));   /* half a spacing */
    if (r == 0) return in;
    h->rf_prefix[0] = 0.0;
    for (uint32_t j = 0; j < n; j++) h->rf_prefix[j + 1] = h->rf_prefix[j] + in[j];
    for (uint32_t k = 0; k < n; k++) {
        uint32_t lo = k > r ? k - r : 0, hi = k + r + 1 < n ? k + r + 1 : n;
        h->rf_buf[k] = (float)((h->rf_prefix[hi] - h->rf_prefix[lo]) / (double)(hi - lo));
    }
    return h->rf_buf;
}

static void tick_audio(brain_t b, brain_sensory_hub_t* h, const float* counts_mean, const float* err_mean,
                       const float* err_rms, float surprise, uint32_t frames,
                       const float* hidden, uint32_t n_hidden) {
    const uint32_t C = SENSORY_HUB_AUDIO_CHANNELS;
    float nerve[SENSORY_HUB_AUDIO_CHANNELS], mgn[SENSORY_HUB_AUDIO_CHANNELS];
    float loud = 0.0f;
    for (uint32_t c = 0; c < C; c++) {
        nerve[c] = counts_mean[c] / HUB_INTENSITY_SCALE;
        loud += nerve[c];
    }
    loud /= C;
    const float jump = loud - h->prev_loudness;
    const bool onset = jump > HUB_ONSET_JUMP;
    h->prev_loudness = loud;
    h->st.last_loudness = loud;
    h->st.last_surprise = surprise;
    h->surprise_ema = h->surprise_ema == 0.0f ? surprise : 0.98f * h->surprise_ema + 0.02f * surprise;
    if (onset) h->st.onsets++;

    /* Thalamus: the MGN relays the auditory nerve to cortex (attention/TRN gated). */
    memcpy(mgn, nerve, sizeof(mgn));
    if (b->thalamus && nimcp_brain_thal_relay_auditory(b, nerve, C, mgn, C) > 0) h->st.thalamus_relays++;   /* returns channels relayed */

    /* Bio-async: broadcast the frame; onsets go to Wernicke and Broca. */
    hub_audio_msg_t m;
    memset(&m, 0, sizeof(m));
    memcpy(m.features, mgn, sizeof(m.features));
    m.surprise = surprise;
    m.loudness = loud;
    m.frames = frames;
    broadcast(h, &m, sizeof(m), BIO_MSG_AUDIO_FEATURE_DETECTED);
    publish(h, "sensory.audio.surprise", surprise);
    publish(h, "sensory.audio.loudness", loud);
    if (onset) {
        publish(h, "sensory.audio.onset", jump);
        send_to(h, &m, sizeof(m), BIO_MSG_SPEECH_ONSET_DETECTED, BIO_MODULE_WERNICKE);
        send_to(h, &m, sizeof(m), BIO_MSG_SPEECH_ONSET_DETECTED, BIO_MODULE_BROCA);
    }

    /* Cortical columns: tonotopic input to the first hypercolumn, pooled
     * into per-minicolumn frequency bands (rf_pool). */
    if (b->hypercolumns && b->num_hypercolumns > 0 && b->hypercolumns[0]) {
        hypercolumn_compute(b->hypercolumns[0], rf_pool(h, b->hypercolumns[0], mgn, C), C);
        h->st.column_updates++;
    }

    /* Wernicke: the predictor's hidden activity drives the Wernicke SNN
     * population (external current, consumed at its next step). */
    if (b->wernicke && hidden && n_hidden) {
        int pop_id = wernicke_get_snn_pop_id(b->wernicke);
        void* net = wernicke_get_snn_network(b->wernicke);
        snn_population_t* pop = (pop_id >= 0 && net) ? snn_network_get_population(net, (uint32_t)pop_id) : NULL;
        if (pop && pop->external_current && pop->n_neurons > 0) {
            for (uint32_t j = 0; j < n_hidden; j++) pop->external_current[j % pop->n_neurons] += hidden[j];
            h->st.wernicke_injections++;
        }
    }

    /* Medulla: loud onsets raise arousal (startle). */
    if (onset && b->medulla && medulla_boost_arousal(b->medulla, 0.5f * jump) == 0) h->st.medulla_boosts++;

    /* Cerebellum: mossy fibres carry the sound, climbing fibres the
     * prediction error (the cerebellar teaching signal). */
    if (b->cerebellum) {
        hub_mossy_t mf[SENSORY_HUB_AUDIO_CHANNELS];
        for (uint32_t c = 0; c < C; c++) {
            mf[c].fiber_id = c;
            mf[c].activity = mgn[c] < 0 ? 0 : (mgn[c] > 1 ? 1 : mgn[c]);
            mf[c].timestamp_ms = 0.0f;
            mf[c].modality = 1;   /* sensory */
        }
        bool ok = cerebellum_process_mossy_batch(b->cerebellum, mf, C);
        for (uint32_t g = 0; g < 8; g++) {          /* 8 olivary groups of 8 channels */
            float e = 0.0f;
            for (uint32_t c = g * 8; c < g * 8 + 8; c++) e += err_mean[c];
            e = tanhf(e * 4.0f);
            hub_climbing_t cf = {g, e, 0.0f, g, 0};
            ok = cerebellum_process_climbing_signal(b->cerebellum, &cf) && ok;
        }
        if (ok) h->st.cerebellum_updates++;
    }

    /* Basal ganglia: intrinsic reward = learning progress (surprise falling). */
    if (b->basal_ganglia && h->st.audio_frames - h->frames_at_last_reward >= HUB_REWARD_GAP_FRAMES) {
        float progress = h->surprise_at_last_reward > 0 ? h->surprise_at_last_reward - h->surprise_ema : 0.0f;
        h->surprise_at_last_reward = h->surprise_ema;
        h->frames_at_last_reward = h->st.audio_frames;
        if (bg_enhanced_process_reward(b->basal_ganglia, tanhf(progress * 10.0f), 0.0f) == 0) h->st.bg_rewards++;
    }

    /* Hippocampus / working memory / workspace: novel sounds (surprise well
     * above its running level) and onsets. */
    bool novel = surprise > 1.5f * h->surprise_ema &&
                 h->st.audio_frames - h->frames_at_last_encode >= HUB_NOVELTY_GAP_FRAMES;
    if (novel || onset) {
        float v[2 * SENSORY_HUB_AUDIO_CHANNELS];
        memcpy(v, mgn, sizeof(mgn));
        memcpy(v + C, err_rms, C * sizeof(float));
        salient_to_cognition(b, h, v, 2 * C, novel ? surprise / (1.0f + surprise) : jump * 5.0f, novel);
        if (novel) h->frames_at_last_encode = h->st.audio_frames;
    }

    /* Immune: input the brain cannot predict at all is anomalous. Only when
     * the immune system is enabled (it is off in production). */
    if (b->immune_system && b->immune_enabled && surprise > HUB_IMMUNE_SURPRISE) {
        uint8_t epitope[SENSORY_HUB_AUDIO_CHANNELS];
        for (uint32_t c = 0; c < C; c++) epitope[c] = (uint8_t)(fminf(mgn[c], 1.0f) * 255.0f);
        uint32_t id = 0;
        uint32_t sev = surprise > 20.0f ? 8u : (uint32_t)(surprise / 2.5f);
        if (brain_immune_present_antigen(b->immune_system, ANTIGEN_SOURCE_ANOMALY, epitope,
                                         sizeof(epitope), sev < 1 ? 1 : sev, 0, &id) == 0)
            h->st.immune_antigens++;
    }

    /* Symbolic logic: a rate-limited perceptual fact on speech onsets. */
    if (onset && b->symbolic_logic && h->st.audio_frames - h->frames_at_last_fact >= HUB_FACT_GAP_FRAMES) {
        h->frames_at_last_fact = h->st.audio_frames;
        if (brain_add_logical_fact(b, "hears(athena, speech)", fminf(1.0f, loud * 2.0f))) h->st.logic_facts++;
    }
}

/* V1 receives the LGN, not pixels: the occipital lobe gets a retinotopic
 * image whose channels are the ON map, the OFF map and their mean,
 * upsampled (nearest) to the occipital's configured size. */
static void drive_occipital(brain_t b, brain_sensory_hub_t* h, const brain_sensory_visual_frame_t* f) {
    if (!b->occipital || !f->map_w || !f->map_h) return;
    occipital_config_t cfg;
    if (!occipital_get_config(b->occipital, &cfg) || !cfg.image_width || !cfg.image_height) return;
    const uint32_t W = cfg.image_width, H = cfg.image_height, C = cfg.color_channels ? cfg.color_channels : 1;
    const uint32_t need = W * H * C;
    if (h->occ_img_n != need) {
        float* img = (float*)realloc(h->occ_img, need * sizeof(float));
        if (!img) return;
        h->occ_img = img;
        h->occ_img_n = need;
    }
    for (uint32_t y = 0; y < H; y++) {
        uint32_t my = y * f->map_h / H;
        for (uint32_t x = 0; x < W; x++) {
            uint32_t m = my * f->map_w + x * f->map_w / W;
            float on = f->on_map[m], off = f->off_map[m];
            for (uint32_t c = 0; c < C; c++)
                h->occ_img[(size_t)c * W * H + (size_t)y * W + x] =
                    c % 3 == 0 ? on : (c % 3 == 1 ? off : 0.5f * (on + off));
        }
    }
    visual_input_t in = {h->occ_img, W, H, C, 0, ++h->occ_frame_id};
    visual_processing_result_t res;
    memset(&res, 0, sizeof(res));
    if (occipital_set_input(b->occipital, &in) && occipital_process(b->occipital, &res))
        h->st.occipital_frames++;
}

static void tick_visual(brain_t b, brain_sensory_hub_t* h, const brain_sensory_visual_frame_t* f) {
    const uint32_t n = f->n;
    float* lgn_out = h->lgn_out;   /* hub-owned; the tick is single-entry (tick_busy) */
    memcpy(lgn_out, f->lgn, n * sizeof(float));
    if (b->thalamus && nimcp_brain_thal_relay_visual(b, f->lgn, n, lgn_out, n) > 0) h->st.thalamus_relays++;

    hub_visual_msg_t m;
    memset(&m, 0, sizeof(m));
    m.n = n < 256u ? n : 256u;
    memcpy(m.features, lgn_out, m.n * sizeof(float));
    m.luminance = f->luminance;
    m.contrast = f->on_off_contrast;
    m.change = f->change;
    broadcast(h, &m, sizeof(m), BIO_MSG_VISUAL_FEATURES_READY);
    publish(h, "sensory.visual.change", f->change);
    publish(h, "sensory.visual.luminance", f->luminance);
    publish(h, "sensory.visual.contrast", f->on_off_contrast);
    drive_occipital(b, h, f);

    if (b->hypercolumns && b->num_hypercolumns > 1 && b->hypercolumns[1]) {
        hypercolumn_compute(b->hypercolumns[1], rf_pool(h, b->hypercolumns[1], lgn_out, n), n);
        h->st.column_updates++;
    }
    if (b->medulla && f->change > 0.3f && medulla_boost_arousal(b->medulla, 0.2f * f->change) == 0)
        h->st.medulla_boosts++;
    if (f->change > 0.15f) salient_to_cognition(b, h, lgn_out, n, f->change, f->change > 0.3f);
    if (b->immune_system && b->immune_enabled && f->luminance > 0.999f) {
        uint8_t ep[64];
        for (uint32_t i = 0; i < 64; i++) ep[i] = (uint8_t)(fminf(lgn_out[i % n], 1.0f) * 255.0f);
        uint32_t id = 0;
        if (brain_immune_present_antigen(b->immune_system, ANTIGEN_SOURCE_ANOMALY, ep, sizeof(ep), 3, 0, &id) == 0)
            h->st.immune_antigens++;   /* saturated input: a dazzling / corrupted frame */
    }
}

/* Self-repair: a hearing predictor that has diverged is restarted. */
static void self_repair(brain_t b, brain_sensory_hub_t* h) {
    auditory_predictor_stats_t s;
    if (!brain_hearing_get_stats(b, &s)) return;
    /* Non-finite errors: the predictor drops its listening state and carries
     * on, but if that keeps happening tick after tick its weights have gone
     * non-finite and its skill EMA (frozen at a finite value) cannot show it.
     * Restart it after 3 such ticks, without the 3000-frame warm-up gate. */
    if (s.nonfinite_resets < h->nonfinite_seen) h->nonfinite_seen = 0;   /* predictor was replaced */
    h->nonfinite_ticks = s.nonfinite_resets > h->nonfinite_seen ? h->nonfinite_ticks + 1 : 0;
    h->nonfinite_seen = s.nonfinite_resets;
    if (h->nonfinite_ticks >= 3) {
        h->nonfinite_ticks = 0;
        if (brain_hearing_reset(b) == 0) {
            h->nonfinite_seen = 0;
            h->st.self_repairs++;
            fprintf(stderr, "[SENSORY_HUB] self-repair: hearing produced non-finite predictions "
                    "(%llu resets) — predictor restarted\n", (unsigned long long)s.nonfinite_resets);
        }
        return;
    }
    if (s.scored < 3000) return;
    bool bad = !isfinite(s.skill_recent) || s.skill_recent < HUB_DIVERGED_SKILL;
    h->diverged_ticks = bad ? h->diverged_ticks + 1 : 0;
    if (h->diverged_ticks >= 100) {
        h->diverged_ticks = 0;
        if (brain_hearing_reset(b) == 0) {
            h->st.self_repairs++;
            fprintf(stderr, "[SENSORY_HUB] self-repair: hearing diverged (skill %.2f) — predictor restarted\n",
                    s.skill_recent);
        }
    }
}

void brain_sensory_hub_tick(struct brain_struct* b) {
    if (!b || !b->sensory_hub) return;
    brain_sensory_hub_t* h = (brain_sensory_hub_t*)b->sensory_hub;
    const uint32_t C = SENSORY_HUB_AUDIO_CHANNELS;
    float counts[SENSORY_HUB_AUDIO_CHANNELS], err_mean[SENSORY_HUB_AUDIO_CHANNELS],
          err_rms[SENSORY_HUB_AUDIO_CHANNELS];
    float hidden[1024];
    uint32_t n_hidden = 0, frames = 0;
    float surprise = 0.0f;
    brain_sensory_visual_frame_t vf;
    bool have_v = false;

    /* Single entry: a concurrent caller (RO-socket decide_full while
     * learn_vector ticks) returns at once without draining, so the frames
     * stay queued for the tick in progress or the next one. Never blocks decide. */
    if (__atomic_exchange_n(&h->tick_busy, 1, __ATOMIC_ACQUIRE)) return;

    nimcp_mutex_lock(h->mu);
    if (h->a_frames > 0) {
        frames = h->a_frames;
        for (uint32_t c = 0; c < C; c++) {
            counts[c] = (float)(h->a_counts[c] / frames);
            err_mean[c] = (float)(h->a_err[c] / frames);
            err_rms[c] = (float)sqrt(h->a_err2[c] / frames);
        }
        surprise = h->a_copy > 0 ? (float)(h->a_mse / h->a_copy) : 0.0f;
        n_hidden = h->n_hidden;
        memcpy(hidden, h->hidden, n_hidden * sizeof(float));
        memset(h->a_counts, 0, sizeof(h->a_counts));
        memset(h->a_err, 0, sizeof(h->a_err));
        memset(h->a_err2, 0, sizeof(h->a_err2));
        h->a_mse = h->a_copy = 0.0;
        h->a_frames = 0;
    }
    if (h->v_pending) {
        vf = h->v_frame;
        h->v_pending = false;
        have_v = true;
    }
    nimcp_mutex_unlock(h->mu);

    if (!frames && !have_v) {
        __atomic_store_n(&h->tick_busy, 0, __ATOMIC_RELEASE);
        return;
    }
    late_bind(b, h);
    h->st.ticks++;
    sensory_hub_heartbeat("tick", 0.5f);
    if (frames) {
        tick_audio(b, h, counts, err_mean, err_rms, surprise, frames, hidden, n_hidden);
        self_repair(b, h);
    }
    if (have_v) tick_visual(b, h, &vf);
    __atomic_store_n(&h->tick_busy, 0, __ATOMIC_RELEASE);
}

bool brain_sensory_hub_get_stats(struct brain_struct* b, brain_sensory_hub_stats_t* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!b || !b->sensory_hub) return false;
    brain_sensory_hub_t* h = (brain_sensory_hub_t*)b->sensory_hub;
    nimcp_mutex_lock(h->mu);
    *out = h->st;
    nimcp_mutex_unlock(h->mu);
    return true;
}
