/**
 * @file nimcp_brain_wired_compute.c
 * @brief Wired-module group BRAIN_WIRED_COMPUTE. See include/core/brain/nimcp_brain_wired.h.
 *
 * WHAT: Drives the compute/analysis modules recovered by the 2026-11-24
 *       dead-code audit from the live brain:
 *
 *   SIMD kernels (src/core/neuralnet/nimcp_neuralnet_simd.c)
 *     CPU capabilities are detected at create; nimcp_simd_dot_product and
 *     nimcp_simd_matvec_mul do this group's own vector math (feature energy,
 *     fidelity cosines, W_rec * x). The ANN forward path is NOT touched:
 *     neural_network_forward_simd() is a pass-through to
 *     neural_network_forward(), so there is nothing to switch over.
 *
 *   Semantic compression of the training feature stream
 *     Every SAMPLE_EVERY ticks the feature vector handed to brain_learn_vector
 *     is average-pooled to POOL_DIM and fed to two compressors:
 *       - NLP sparse-dictionary compressor (src/networking/nlp/...):
 *         matching-pursuit encode; the dictionary is learned in batches from
 *         a ring of recent samples (LEARN_EVERY ticks) and persisted in the
 *         sidecar <checkpoint>.wired_compute.
 *       - bio-async prototype (VQ) compressor (src/async/...): nearest
 *         prototype within a quality budget, else a new prototype; codebook
 *         size growth is a novelty signal. Pruned when full. Not persisted
 *         (no enumeration API; rebuilds within minutes).
 *     Both are analysis-only: they read the features, they write nothing
 *     back into shared brain state.
 *
 *   LNN recurrent-topology analysis (every TOPO_EVERY ticks)
 *     A snapshot of one LNN layer's W_rec and state x is taken under the
 *     LNN network mutex (leaf lock: nothing else is called while held), then
 *     analysed off-lock:
 *       - ternary LNN (src/lnn/nimcp_lnn_ternary.c): W_rec quantized to
 *         {-1,0,+1}; reports sparsity/sign balance and fidelity
 *         cos(W_rec x, ternary(W_rec) x). Shadow copy only — the LNN keeps
 *         its float weights (ternary is an option, not a replacement;
 *         lnn_layer_quantize_to_ternary() is the explicit opt-in API).
 *       - ternary wiring (src/lnn/nimcp_lnn_wiring_ternary.c): the layer's
 *         binary wiring signed by the learned W_rec -> learned E/I ratio and
 *         hub out-degree.
 *       - ternary community detection (src/core/topology/..._ternary.c):
 *         signed Louvain on the symmetrized quantized W_rec -> signed /
 *         unsigned modularity, communities, network E/I.
 *       - Louvain + modularity (src/utils/algorithms/): weighted Louvain on
 *         |W_rec + W_rec^T| -> communities, modularity Q, partition check.
 *     The snapshot also counts non-finite W_rec entries (NaN tripwire).
 *
 * Thread safety: tick/save/load run on the training thread; status_json may
 * run on RPC workers, so every reported field lives under st->lock. The
 * compressors are only touched from tick/save/load (training thread).
 */
#include "core/brain/nimcp_brain_wired.h"
#include "core/brain/nimcp_brain_internal.h"

#include "core/neuralnet/nimcp_neuralnet_simd.h"
#include "networking/nlp/nimcp_semantic_compression.h"
#include "async/nimcp_semantic_compression.h"
#include "lnn/nimcp_lnn_types.h"
#include "lnn/nimcp_lnn_ternary.h"
#include "lnn/nimcp_lnn_wiring_ternary.h"
#include "core/topology/nimcp_community_detection_ternary.h"
#include "utils/algorithms/nimcp_louvain.h"
#include "utils/algorithms/nimcp_modularity.h"
#include "utils/containers/nimcp_graph.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*=============================================================================
 * Tuning
 *===========================================================================*/

#define POOL_DIM          64u     /* pooled feature dimension fed to compressors */
#define SAMPLE_EVERY      8u      /* ticks between compression samples */
#define RING_SAMPLES      64u     /* samples kept for dictionary learning */
#define LEARN_EVERY       512u    /* ticks between dictionary-learning batches */
#define TOPO_FIRST        1000u   /* first topology analysis at this tick */
#define TOPO_EVERY        20000u  /* ticks between topology analyses */
#define TOPO_MAX_NEURONS  2048u   /* skip LNN layers larger than this */
#define TERNARY_THRESHOLD LNN_TERNARY_DEFAULT_THRESHOLD   /* relative to max|w| */
#define LOUVAIN_MAX_DEG   32u     /* target mean degree for the Louvain graph */

/*=============================================================================
 * State
 *===========================================================================*/

typedef struct {
    nimcp_mutex_t* lock;              /* guards the reported fields below */
    uint64_t ticks;

    /* SIMD */
    nimcp_cpu_caps_t caps;
    float feature_energy_ema;
    uint64_t nonfinite_feature_ticks;

    /* Compression */
    semantic_compressor_t* nlp;            /* sparse dictionary (learned, persisted) */
    nimcp_semantic_compressor_t* vq;       /* prototype codebook */
    float pooled[POOL_DIM];
    float ring[RING_SAMPLES][POOL_DIM];
    uint32_t ring_count;
    uint32_t ring_head;
    uint64_t nlp_compressions;
    uint64_t nlp_learn_batches;
    uint64_t nlp_dictionary_resets;
    float nlp_quality_ema;
    float nlp_ratio;
    float nlp_sparsity;
    uint64_t vq_compressions;
    uint32_t vq_prototypes;
    uint32_t vq_pruned;
    float vq_quality_ema;
    float vq_ratio;
    int nlp_loaded;                        /* 1 = dictionary restored from sidecar */

    /* Topology (last analysis) */
    uint64_t topo_runs;
    uint64_t topo_skipped;
    float topo_ms;
    uint32_t lnn_layer;
    uint32_t lnn_n;
    uint32_t wrec_nonfinite;
    float tern_sparsity;
    uint32_t tern_pos, tern_neg;
    float tern_fidelity;                   /* cos(W x, T(W) x) */
    int wiring_valid;
    uint32_t wiring_exc, wiring_inh;
    float wiring_ei;
    uint32_t wiring_hub, wiring_hub_out;
    uint32_t tcomm_n;
    float tcomm_q_signed, tcomm_q_unsigned, tcomm_ei;
    uint32_t louvain_n;
    double louvain_q;
    uint32_t louvain_iters;
    uint32_t louvain_edges;
    int louvain_valid;
} compute_state_t;

static compute_state_t* get_state(struct brain_struct* brain) {
    return brain ? (compute_state_t*)brain->wired[BRAIN_WIRED_COMPUTE] : NULL;
}

static float finite_or(float v, float fallback) {
    return isfinite(v) ? v : fallback;
}

static void sidecar_path(const char* checkpoint, char* out, size_t cap) {
    snprintf(out, cap, "%s.wired_compute", checkpoint);
}

/*=============================================================================
 * Lifecycle
 *===========================================================================*/

int brain_wired_compute_create(struct brain_struct* brain) {
    if (!brain) {
        return -1;
    }
    if (brain->wired[BRAIN_WIRED_COMPUTE]) {
        return 0;
    }

    compute_state_t* st = nimcp_calloc(1, sizeof(*st));
    if (!st) {
        return -1;
    }
    st->lock = nimcp_mutex_create(NULL);
    if (!st->lock) {
        nimcp_free(st);
        return -1;
    }

    nimcp_cpu_detect(&st->caps);

    /* NLP sparse-dictionary compressor: dictionary_size <= dim keeps the
     * Gram-Schmidt initial basis non-degenerate. Online learning is off; the
     * group learns in rate-limited batches instead. */
    semantic_compression_config_t ncfg;
    semantic_get_default_config(&ncfg);
    ncfg.dictionary_size = 48;
    ncfg.primitive_vector_size = POOL_DIM;
    ncfg.max_active_primitives = 8;
    ncfg.learning_rate = 0.01f;
    ncfg.learning_iterations = 1;
    ncfg.enable_online_learning = false;
    ncfg.enable_bio_async = false;
    st->nlp = semantic_compressor_create(&ncfg);

    /* Prototype (VQ) compressor */
    nimcp_compression_config_t vcfg = nimcp_semantic_compressor_default_config();
    vcfg.max_primitives = 256;
    vcfg.vector_dimension = POOL_DIM;
    vcfg.quality_level = 0.8f;           /* reuse a prototype within 20% rel. error */
    vcfg.enable_delta = false;
    vcfg.enable_residuals = false;
    vcfg.primitive_learning_rate = 0.05f;
    vcfg.min_primitive_usage = 2;
    vcfg.bio_async_enabled = false;
    st->vq = nimcp_semantic_compressor_create(&vcfg);

    if (!st->nlp && !st->vq) {
        nimcp_mutex_free(st->lock);
        nimcp_free(st);
        return -1;
    }

    brain->wired[BRAIN_WIRED_COMPUTE] = st;
    fprintf(stderr, "[WIRED] COMPUTE: simd_level=%d nlp=%s vq=%s lnn=%s\n",
            (int)st->caps.best_level, st->nlp ? "yes" : "no", st->vq ? "yes" : "no",
            brain->lnn_network ? "yes" : "no");
    return 0;
}

void brain_wired_compute_destroy(struct brain_struct* brain) {
    compute_state_t* st = get_state(brain);
    if (!st) {
        if (brain) brain->wired[BRAIN_WIRED_COMPUTE] = NULL;
        return;
    }
    brain->wired[BRAIN_WIRED_COMPUTE] = NULL;
    if (st->nlp) semantic_compressor_destroy(st->nlp);
    if (st->vq) nimcp_semantic_compressor_destroy(st->vq);
    if (st->lock) nimcp_mutex_free(st->lock);
    nimcp_free(st);
}

/*=============================================================================
 * Compression (every SAMPLE_EVERY ticks)
 *===========================================================================*/

/* Average-pool f[0..n) into POOL_DIM bins. Returns false on non-finite input. */
static bool pool_features(const float* f, uint32_t n, float* out) {
    for (uint32_t b = 0; b < POOL_DIM; b++) {
        uint32_t lo = (uint32_t)(((uint64_t)b * n) / POOL_DIM);
        uint32_t hi = (uint32_t)(((uint64_t)(b + 1) * n) / POOL_DIM);
        if (hi <= lo) hi = lo + 1;
        if (hi > n) hi = n;
        if (lo >= n) lo = n - 1;
        float sum = 0.0f;
        for (uint32_t i = lo; i < hi; i++) {
            if (!isfinite(f[i])) return false;
            sum += f[i];
        }
        out[b] = sum / (float)(hi - lo);
    }
    return true;
}

/* A dictionary that went non-finite is useless and would poison every
 * encode: detect and re-initialise. */
static bool nlp_dictionary_finite(semantic_compressor_t* nlp, uint32_t count) {
    for (uint32_t p = 0; p < count; p++) {
        semantic_primitive_t prim;
        if (semantic_get_primitive(nlp, p, &prim) != NIMCP_SUCCESS) break;
        if (!prim.vector) continue;
        for (uint32_t k = 0; k < prim.vector_size; k++) {
            if (!isfinite(prim.vector[k])) return false;
        }
    }
    return true;
}

static void run_compression(compute_state_t* st) {
    float nlp_quality = NAN, nlp_ratio = NAN, nlp_sparsity = NAN;
    float vq_quality = NAN, vq_ratio = NAN;
    uint32_t vq_protos = 0, pruned = 0;
    bool nlp_ok = false, vq_ok = false;

    if (st->nlp) {
        uint8_t buf[sizeof(uint32_t) * 4 + sizeof(float) + 8 * (sizeof(uint32_t) + sizeof(float))];
        uint32_t sz = (uint32_t)sizeof(buf);
        semantic_compression_metadata_t meta;
        memset(&meta, 0, sizeof(meta));
        if (semantic_compress_with_quality(st->nlp, st->pooled, POOL_DIM, 0.0f,
                                           buf, &sz, &meta) == NIMCP_SUCCESS) {
            nlp_ok = true;
            nlp_quality = meta.quality_score;
            nlp_ratio = semantic_get_compression_ratio(st->nlp);
            nlp_sparsity = semantic_get_sparsity(st->nlp);
        }
    }

    if (st->vq) {
        nimcp_compressed_signal_t* c = nimcp_semantic_compressor_compress(st->vq, st->pooled, POOL_DIM);
        if (c) {
            vq_ok = true;
            vq_quality = c->quality_metric;
            nimcp_compressed_signal_destroy(c);
        }
        nimcp_compression_stats_t vs;
        if (nimcp_semantic_compressor_get_stats(st->vq, &vs) == NIMCP_SUCCESS) {
            vq_protos = vs.active_primitives;
            vq_ratio = vs.avg_compression_ratio;
            /* Keep the codebook adaptive: when full, drop rarely used prototypes */
            if (vs.active_primitives >= 256) {
                pruned = nimcp_semantic_compressor_prune_primitives(st->vq, 2);
                vq_protos = vs.active_primitives - pruned;
            }
        }
    }

    nimcp_mutex_lock(st->lock);
    if (nlp_ok) {
        st->nlp_compressions++;
        if (isfinite(nlp_quality)) {
            st->nlp_quality_ema = (st->nlp_compressions == 1)
                ? nlp_quality : 0.99f * st->nlp_quality_ema + 0.01f * nlp_quality;
        }
        st->nlp_ratio = finite_or(nlp_ratio, st->nlp_ratio);
        st->nlp_sparsity = finite_or(nlp_sparsity, st->nlp_sparsity);
    }
    if (vq_ok) {
        st->vq_compressions++;
        if (isfinite(vq_quality)) {
            st->vq_quality_ema = (st->vq_compressions == 1)
                ? vq_quality : 0.99f * st->vq_quality_ema + 0.01f * vq_quality;
        }
        st->vq_ratio = finite_or(vq_ratio, st->vq_ratio);
        st->vq_prototypes = vq_protos;
        st->vq_pruned += pruned;
    }
    nimcp_mutex_unlock(st->lock);
}

static void run_dictionary_learning(compute_state_t* st) {
    if (!st->nlp || st->ring_count < 8) {
        return;
    }
    const float* samples[RING_SAMPLES];
    for (uint32_t i = 0; i < st->ring_count; i++) {
        samples[i] = st->ring[i];
    }
    bool reset = false;
    if (semantic_learn_primitives(st->nlp, samples, st->ring_count, POOL_DIM) == NIMCP_SUCCESS) {
        if (!nlp_dictionary_finite(st->nlp, 48)) {
            semantic_compressor_reset(st->nlp, true);
            reset = true;
        }
    }
    nimcp_mutex_lock(st->lock);
    st->nlp_learn_batches++;
    if (reset) st->nlp_dictionary_resets++;
    nimcp_mutex_unlock(st->lock);
}

/*=============================================================================
 * LNN topology analysis (every TOPO_EVERY ticks)
 *===========================================================================*/

typedef struct {
    uint32_t layer, n;
    float* w;      /* n*n row-major: w[i*n+j] = weight j -> i (matches wiring(i,j)) */
    float* x;      /* n */
    uint32_t* row_ptr;            /* owned copy of the wiring CSR */
    uint32_t* col_idx;
    lnn_wiring_type_t wiring_type;
} lnn_snapshot_t;

static void snapshot_free(lnn_snapshot_t* s) {
    nimcp_free(s->w);
    nimcp_free(s->x);
    nimcp_free(s->row_ptr);
    nimcp_free(s->col_idx);
    memset(s, 0, sizeof(*s));
}

/* Copy one LNN layer's W_rec, state and wiring CSR under the network mutex.
 * Only memcpy happens while the lock is held. */
static bool take_lnn_snapshot(struct brain_struct* brain, lnn_snapshot_t* s) {
    memset(s, 0, sizeof(*s));
    lnn_network_t* net = (lnn_network_t*)brain->lnn_network;
    if (!net || !net->layers || net->n_layers == 0) {
        return false;
    }

    /* Choose the largest layer that fits the analysis budget (by size only;
     * sizes are fixed after creation, read here without the lock). */
    uint32_t best = UINT32_MAX, best_n = 0;
    for (uint32_t l = 0; l < net->n_layers; l++) {
        lnn_layer_t* L = net->layers[l];
        if (!L || !L->W_rec) continue;
        if (L->n_neurons >= 8 && L->n_neurons <= TOPO_MAX_NEURONS && L->n_neurons > best_n) {
            best = l;
            best_n = L->n_neurons;
        }
    }
    if (best == UINT32_MAX) {
        return false;
    }

    const uint32_t n = best_n;
    s->w = nimcp_malloc((size_t)n * n * sizeof(float));
    s->x = nimcp_calloc(n, sizeof(float));
    if (!s->w || !s->x) {
        snapshot_free(s);
        return false;
    }
    uint32_t edges_cap = 0;

    nimcp_platform_mutex_t* mtx = (nimcp_platform_mutex_t*)net->mutex;
    if (mtx) nimcp_platform_mutex_lock(mtx);

    bool ok = false;
    lnn_layer_t* L = (best < net->n_layers) ? net->layers[best] : NULL;
    if (L && L->W_rec && L->n_neurons == n &&
        nimcp_tensor_numel(L->W_rec) == (size_t)n * n) {
        const float* wd = (const float*)nimcp_tensor_data_const(L->W_rec);
        if (wd) {
            memcpy(s->w, wd, (size_t)n * n * sizeof(float));
            ok = true;
        }
        if (L->x && nimcp_tensor_numel(L->x) == n) {
            const float* xd = (const float*)nimcp_tensor_data_const(L->x);
            if (xd) memcpy(s->x, xd, n * sizeof(float));
        }
        const lnn_wiring_t* wr = L->wiring;
        if (ok && wr && wr->n_neurons == n && wr->row_ptr && wr->col_idx) {
            edges_cap = wr->row_ptr[n];
            s->row_ptr = nimcp_malloc((n + 1) * sizeof(uint32_t));
            s->col_idx = (edges_cap > 0) ? nimcp_malloc(edges_cap * sizeof(uint32_t)) : NULL;
            if (s->row_ptr && (s->col_idx || edges_cap == 0)) {
                memcpy(s->row_ptr, wr->row_ptr, (n + 1) * sizeof(uint32_t));
                if (edges_cap) memcpy(s->col_idx, wr->col_idx, edges_cap * sizeof(uint32_t));
                s->wiring_type = wr->type;
            } else {
                nimcp_free(s->row_ptr);
                nimcp_free(s->col_idx);
                s->row_ptr = NULL;
                s->col_idx = NULL;
            }
        }
    }

    if (mtx) nimcp_platform_mutex_unlock(mtx);

    if (!ok) {
        snapshot_free(s);
        return false;
    }
    s->layer = best;
    s->n = n;
    return true;
}

static float cosine(const float* a, const float* b, uint32_t n) {
    float ab = nimcp_simd_dot_product(a, b, n);
    float aa = nimcp_simd_dot_product(a, a, n);
    float bb = nimcp_simd_dot_product(b, b, n);
    float d = sqrtf(aa) * sqrtf(bb);
    return (d > 1e-12f && isfinite(d)) ? ab / d : 0.0f;
}

typedef struct {
    float tern_sparsity, tern_fidelity;
    uint32_t tern_pos, tern_neg;
    int wiring_valid;
    uint32_t wiring_exc, wiring_inh, wiring_hub, wiring_hub_out;
    float wiring_ei;
    uint32_t tcomm_n;
    float tcomm_q_signed, tcomm_q_unsigned, tcomm_ei;
    uint32_t louvain_n, louvain_iters, louvain_edges;
    double louvain_q;
    int louvain_valid;
} topo_result_t;

/* (1) Ternary LNN shadow: quantize W_rec, compare W x vs T(W) x */
static void analyse_ternary_lnn(const lnn_snapshot_t* s, topo_result_t* r) {
    const uint32_t n = s->n;
    uint32_t dims2[2] = {n, n};
    uint32_t dims1[1] = {n};
    nimcp_tensor_t* wt = nimcp_tensor_create(dims2, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* xt = nimcp_tensor_create(dims1, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* yt = nimcp_tensor_create(dims1, 1, NIMCP_DTYPE_F32);
    float* yf = nimcp_malloc(n * sizeof(float));
    if (!wt || !xt || !yt || !yf) goto out;

    memcpy(nimcp_tensor_data(wt), s->w, (size_t)n * n * sizeof(float));

    /* Probe with the live state; fall back to a fixed +/- pattern when the
     * state is (near) zero, e.g. right after a reset. */
    float* xd = (float*)nimcp_tensor_data(xt);
    float xn = nimcp_simd_dot_product(s->x, s->x, n);
    for (uint32_t i = 0; i < n; i++) {
        xd[i] = (xn > 1e-12f) ? s->x[i] : ((i & 1u) ? -1.0f : 1.0f);
    }

    lnn_ternary_matrix_t* tm = lnn_ternary_matrix_from_float(wt, TERNARY_THRESHOLD,
                                                             TERNARY_PACK_NONE, true);
    if (!tm) goto out;

    uint32_t pos = 0, zero = 0, neg = 0;
    lnn_ternary_matrix_stats(tm, &pos, &zero, &neg);
    r->tern_pos = pos;
    r->tern_neg = neg;
    r->tern_sparsity = lnn_ternary_matrix_sparsity(tm);

    nimcp_simd_matvec_mul(s->w, xd, yf, n, n);
    if (lnn_ternary_matmul(tm, xt, yt) == LNN_SUCCESS) {
        r->tern_fidelity = cosine(yf, (const float*)nimcp_tensor_data(yt), n);
    }
    lnn_ternary_matrix_destroy(tm);

out:
    nimcp_free(yf);
    nimcp_tensor_destroy(wt);
    nimcp_tensor_destroy(xt);
    nimcp_tensor_destroy(yt);
}

/* (2) Ternary wiring: the binary wiring signed by learned W_rec */
static void analyse_ternary_wiring(const lnn_snapshot_t* s, float wmax, topo_result_t* r) {
    if (!s->row_ptr) return;
    const uint32_t n = s->n;

    lnn_wiring_t view;
    memset(&view, 0, sizeof(view));
    view.type = s->wiring_type;
    view.n_neurons = n;
    view.row_ptr = s->row_ptr;
    view.col_idx = s->col_idx;
    view.n_edges = s->row_ptr[n];

    /* Seeded so the base E/I draw is deterministic; every existing edge is
     * then overwritten with the sign of its learned weight. */
    lnn_ternary_wiring_t* tw = lnn_ternary_wiring_from_binary(&view, 0.8f, 0x5eedULL);
    if (!tw) return;

    const float thr = TERNARY_THRESHOLD * wmax;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t k = s->row_ptr[i]; k < s->row_ptr[i + 1]; k++) {
            uint32_t j = s->col_idx[k];
            if (j >= n) continue;
            float w = s->w[(size_t)i * n + j];
            trit_t t = (fabsf(w) < thr) ? LNN_CONN_WEAK
                     : (w > 0.0f ? LNN_CONN_EXCITATORY : LNN_CONN_INHIBITORY);
            lnn_ternary_wiring_set(tw, i, j, t);
        }
    }

    uint32_t exc = 0, inh = 0, absent = 0;
    float sparsity = 0.0f, ei = 0.0f;
    lnn_ternary_wiring_stats(tw, &exc, &inh, &absent, &sparsity, &ei);
    r->wiring_exc = exc;
    r->wiring_inh = inh;
    r->wiring_ei = ei;

    uint32_t hub = 0, hub_out = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t d = lnn_ternary_wiring_out_degree(tw, i);
        if (d > hub_out) {
            hub_out = d;
            hub = i;
        }
    }
    r->wiring_hub = hub;
    r->wiring_hub_out = hub_out;
    r->wiring_valid = 1;
    lnn_ternary_wiring_destroy(tw);
}

/* (3) Ternary (signed) communities + (4) weighted Louvain on |W + W^T| */
static void analyse_communities(const lnn_snapshot_t* s, topo_result_t* r) {
    const uint32_t n = s->n;
    float* sym = nimcp_malloc((size_t)n * n * sizeof(float));
    if (!sym) return;

    float smax = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sym[(size_t)i * n + i] = 0.0f;
        for (uint32_t j = i + 1; j < n; j++) {
            float v = s->w[(size_t)i * n + j] + s->w[(size_t)j * n + i];
            sym[(size_t)i * n + j] = v;
            sym[(size_t)j * n + i] = v;
            if (fabsf(v) > smax) smax = fabsf(v);
        }
    }
    if (smax <= 0.0f || !isfinite(smax)) {
        nimcp_free(sym);
        return;
    }

    /* Edge threshold: start at the ternary threshold, raise until the mean
     * degree is bounded (keeps both community passes cheap). */
    float thr = TERNARY_THRESHOLD * smax;
    uint64_t edges = 0;
    for (int attempt = 0; attempt < 16; attempt++) {
        edges = 0;
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = i + 1; j < n; j++) {
                if (fabsf(sym[(size_t)i * n + j]) >= thr) edges++;
            }
        }
        if (edges <= (uint64_t)n * LOUVAIN_MAX_DEG / 2) break;
        thr *= 1.25f;
    }

    /* (3) signed ternary communities */
    trit_matrix_t* adj = community_ternary_quantize_adjacency(sym, n, thr, TERNARY_PACK_NONE);
    if (adj) {
        community_ternary_config_t cfg;
        community_ternary_config_default(&cfg);
        cfg.max_iterations = 20;
        community_ternary_result_t* cr = community_ternary_detect(adj, &cfg);
        if (cr) {
            r->tcomm_n = cr->num_communities;
            r->tcomm_q_signed = cr->modularity_signed;
            r->tcomm_q_unsigned = cr->modularity;
            r->tcomm_ei = cr->network_ei_ratio;
            community_ternary_result_free(cr);
        }
        trit_matrix_destroy(adj);
    }

    /* (4) weighted Louvain */
    if (n <= NIMCP_MAX_VERTICES) {
        NimcpGraph* g = nimcp_graph_create();
        if (g) {
            bool ok = true;
            for (uint32_t i = 0; i < n && ok; i++) {
                ok = (nimcp_graph_add_vertex(g, (uint64_t)i + 1u, 0.0f, 0.0f, 0.0f, 0) == i);
            }
            uint32_t added = 0;
            for (uint32_t i = 0; i < n && ok; i++) {
                for (uint32_t j = i + 1; j < n; j++) {
                    float v = fabsf(sym[(size_t)i * n + j]);
                    if (v < thr) continue;
                    nimcp_graph_add_edge(g, i, j, v);
                    nimcp_graph_add_edge(g, j, i, v);
                    added++;
                }
            }
            if (ok && added > 0) {
                NimcpCommunityPartition* p = nimcp_louvain_detect(g, 1.0, 0);
                if (p) {
                    r->louvain_n = nimcp_count_communities(p->assignments, n);
                    r->louvain_q = nimcp_calculate_modularity(g, p->assignments, n);
                    r->louvain_iters = p->iterations;
                    r->louvain_edges = added;
                    r->louvain_valid = nimcp_validate_partition(p->assignments, n,
                                                                p->num_communities) ? 1 : 0;
                    nimcp_community_partition_destroy(p);
                }
            }
            nimcp_graph_destroy(g);
        }
    }

    nimcp_free(sym);
}

static void run_topology(struct brain_struct* brain, compute_state_t* st) {
    uint64_t t0 = nimcp_time_get_us();
    lnn_snapshot_t s;
    if (!take_lnn_snapshot(brain, &s)) {
        nimcp_mutex_lock(st->lock);
        st->topo_skipped++;
        nimcp_mutex_unlock(st->lock);
        return;
    }

    /* NaN tripwire: count and refuse to analyse non-finite weights */
    uint32_t nonfinite = 0;
    float wmax = 0.0f;
    const size_t nn = (size_t)s.n * s.n;
    for (size_t k = 0; k < nn; k++) {
        float v = s.w[k];
        if (!isfinite(v)) { nonfinite++; continue; }
        if (fabsf(v) > wmax) wmax = fabsf(v);
    }
    for (uint32_t i = 0; i < s.n; i++) {
        if (!isfinite(s.x[i])) s.x[i] = 0.0f;
    }

    topo_result_t r;
    memset(&r, 0, sizeof(r));
    if (nonfinite == 0 && wmax > 0.0f) {
        analyse_ternary_lnn(&s, &r);
        analyse_ternary_wiring(&s, wmax, &r);
        analyse_communities(&s, &r);
    }

    float ms = (float)(nimcp_time_get_us() - t0) / 1000.0f;

    nimcp_mutex_lock(st->lock);
    st->topo_runs++;
    st->topo_ms = ms;
    st->lnn_layer = s.layer;
    st->lnn_n = s.n;
    st->wrec_nonfinite = nonfinite;
    if (nonfinite == 0) {
        st->tern_sparsity = r.tern_sparsity;
        st->tern_pos = r.tern_pos;
        st->tern_neg = r.tern_neg;
        st->tern_fidelity = r.tern_fidelity;
        st->wiring_valid = r.wiring_valid;
        st->wiring_exc = r.wiring_exc;
        st->wiring_inh = r.wiring_inh;
        st->wiring_ei = r.wiring_ei;
        st->wiring_hub = r.wiring_hub;
        st->wiring_hub_out = r.wiring_hub_out;
        st->tcomm_n = r.tcomm_n;
        st->tcomm_q_signed = r.tcomm_q_signed;
        st->tcomm_q_unsigned = r.tcomm_q_unsigned;
        st->tcomm_ei = r.tcomm_ei;
        st->louvain_n = r.louvain_n;
        st->louvain_q = r.louvain_q;
        st->louvain_iters = r.louvain_iters;
        st->louvain_edges = r.louvain_edges;
        st->louvain_valid = r.louvain_valid;
    }
    nimcp_mutex_unlock(st->lock);

    if (nonfinite) {
        fprintf(stderr, "[WIRED] COMPUTE: LNN layer %u W_rec has %u non-finite entries\n",
                s.layer, nonfinite);
    }
    snapshot_free(&s);
}

/*=============================================================================
 * Tick
 *===========================================================================*/

void brain_wired_compute_tick(struct brain_struct* brain, const float* f, uint32_t n, float dt_ms) {
    (void)dt_ms;
    compute_state_t* st = get_state(brain);
    if (!st) {
        return;
    }
    st->ticks++;

    if (f && n > 0 && (st->ticks % SAMPLE_EVERY) == 0) {
        if (pool_features(f, n, st->pooled)) {
            float e = nimcp_simd_dot_product(st->pooled, st->pooled, POOL_DIM) / (float)POOL_DIM;
            memcpy(st->ring[st->ring_head], st->pooled, sizeof(st->pooled));
            st->ring_head = (st->ring_head + 1) % RING_SAMPLES;
            if (st->ring_count < RING_SAMPLES) st->ring_count++;

            run_compression(st);

            if (isfinite(e)) {
                nimcp_mutex_lock(st->lock);
                st->feature_energy_ema = 0.99f * st->feature_energy_ema + 0.01f * e;
                nimcp_mutex_unlock(st->lock);
            }
        } else {
            nimcp_mutex_lock(st->lock);
            st->nonfinite_feature_ticks++;
            nimcp_mutex_unlock(st->lock);
        }
    }

    if ((st->ticks % LEARN_EVERY) == 0) {
        run_dictionary_learning(st);
    }

    if (st->ticks == TOPO_FIRST ||
        (st->ticks > TOPO_FIRST && ((st->ticks - TOPO_FIRST) % TOPO_EVERY) == 0)) {
        run_topology(brain, st);
    }
}

/*=============================================================================
 * Persistence: sidecar <checkpoint>.wired_compute = NLP dictionary
 *===========================================================================*/

int brain_wired_compute_save(struct brain_struct* brain, const char* path) {
    compute_state_t* st = get_state(brain);
    if (!st || !path || !st->nlp) {
        return 0;
    }
    char side[1024], tmp[1040];
    sidecar_path(path, side, sizeof(side));
    snprintf(tmp, sizeof(tmp), "%s.tmp", side);
    if (semantic_save_dictionary(st->nlp, tmp) != NIMCP_SUCCESS) {
        unlink(tmp);
        return -1;
    }
    if (rename(tmp, side) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

int brain_wired_compute_load(struct brain_struct* brain, const char* path) {
    compute_state_t* st = get_state(brain);
    if (!st || !path || !st->nlp) {
        return 1;
    }
    char side[1024];
    sidecar_path(path, side, sizeof(side));
    if (access(side, R_OK) != 0) {
        return 1;   /* no sidecar: fresh dictionary */
    }
    if (semantic_load_dictionary(st->nlp, side) != NIMCP_SUCCESS) {
        return -1;  /* invalid file: the live dictionary is left untouched */
    }
    nimcp_mutex_lock(st->lock);
    st->nlp_loaded = 1;
    nimcp_mutex_unlock(st->lock);
    return 0;
}

/*=============================================================================
 * Status
 *===========================================================================*/

int brain_wired_compute_status_json(struct brain_struct* brain, char* buf, uint32_t cap) {
    compute_state_t* st = get_state(brain);
    if (!st) {
        return snprintf(buf, cap, "null");
    }
    nimcp_mutex_lock(st->lock);
    int w = snprintf(buf, cap,
        "{\"ticks\":%llu,\"simd_level\":%d,\"avx2\":%s,\"fma\":%s,"
        "\"feature_energy\":%.6g,\"nonfinite_feature_ticks\":%llu,"
        "\"nlp\":{\"on\":%s,\"compressions\":%llu,\"quality\":%.4f,\"ratio\":%.3f,"
        "\"sparsity\":%.3f,\"learn_batches\":%llu,\"resets\":%llu,\"loaded\":%d},"
        "\"vq\":{\"on\":%s,\"compressions\":%llu,\"quality\":%.4f,\"ratio\":%.3f,"
        "\"prototypes\":%u,\"pruned\":%u},"
        "\"topology\":{\"runs\":%llu,\"skipped\":%llu,\"ms\":%.2f,\"lnn_layer\":%u,\"n\":%u,"
        "\"wrec_nonfinite\":%u,"
        "\"ternary\":{\"sparsity\":%.4f,\"pos\":%u,\"neg\":%u,\"fidelity\":%.4f},"
        "\"wiring\":{\"valid\":%d,\"exc\":%u,\"inh\":%u,\"ei\":%.4f,\"hub\":%u,\"hub_out\":%u},"
        "\"ternary_communities\":{\"n\":%u,\"q_signed\":%.4f,\"q_unsigned\":%.4f,\"ei\":%.4f},"
        "\"louvain\":{\"n\":%u,\"q\":%.4f,\"iters\":%u,\"edges\":%u,\"valid\":%d}}}",
        (unsigned long long)st->ticks, (int)st->caps.best_level,
        st->caps.has_avx2 ? "true" : "false", st->caps.has_fma ? "true" : "false",
        (double)finite_or(st->feature_energy_ema, 0.0f),
        (unsigned long long)st->nonfinite_feature_ticks,
        st->nlp ? "true" : "false", (unsigned long long)st->nlp_compressions,
        (double)finite_or(st->nlp_quality_ema, 0.0f), (double)finite_or(st->nlp_ratio, 0.0f),
        (double)finite_or(st->nlp_sparsity, 0.0f), (unsigned long long)st->nlp_learn_batches,
        (unsigned long long)st->nlp_dictionary_resets, st->nlp_loaded,
        st->vq ? "true" : "false", (unsigned long long)st->vq_compressions,
        (double)finite_or(st->vq_quality_ema, 0.0f), (double)finite_or(st->vq_ratio, 0.0f),
        st->vq_prototypes, st->vq_pruned,
        (unsigned long long)st->topo_runs, (unsigned long long)st->topo_skipped,
        (double)finite_or(st->topo_ms, 0.0f), st->lnn_layer, st->lnn_n, st->wrec_nonfinite,
        (double)finite_or(st->tern_sparsity, 0.0f), st->tern_pos, st->tern_neg,
        (double)finite_or(st->tern_fidelity, 0.0f),
        st->wiring_valid, st->wiring_exc, st->wiring_inh,
        (double)finite_or(st->wiring_ei, -1.0f), st->wiring_hub, st->wiring_hub_out,
        st->tcomm_n, (double)finite_or(st->tcomm_q_signed, 0.0f),
        (double)finite_or(st->tcomm_q_unsigned, 0.0f), (double)finite_or(st->tcomm_ei, -1.0f),
        st->louvain_n, isfinite(st->louvain_q) ? st->louvain_q : 0.0,
        st->louvain_iters, st->louvain_edges, st->louvain_valid);
    nimcp_mutex_unlock(st->lock);
    return w;
}
